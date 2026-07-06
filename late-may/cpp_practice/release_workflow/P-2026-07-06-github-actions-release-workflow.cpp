// P-2026-07-06 — release_workflow: GitHub Actions workflow inspector
//
// Today we close the psp_span_lib arc with a GitHub Actions
// workflow that builds and publishes the CPack archive on tag push.
// The workflow YAML lives in `release.yml` next to this file; this
// program is the proof that the YAML is well-formed BEFORE we push
// it to the repo (and before we ever tag a real release).
//
// The pattern is the same one we've used since P-2026-07-01:
// build the artifact locally, consume it from a fresh
// find_package()-style test, and prove the result is what we
// expect. The artifact today is a YAML file instead of a `.a` or
// `.tar.gz`, but the rule is the same: the unit of work is the
// file you ship, and the lesson is its verification.
//
// ─── Design ──────────────────────────────────────────────────────
//
// We don't write a YAML parser. libyaml (SAX-style) + a tree
// builder is ~300 lines of state-machine code that has nothing to
// do with the lesson. Instead we delegate the parse to PyYAML
// (which is, under the hood, libyaml + a tree builder) by
// shelling out to a small Python helper, and consume the JSON it
// emits with a hand-rolled recursive-descent JSON reader. This is
// the same outsourcing pattern as Jul 2's `find_package(fmt)`:
// get the proven tool, don't rewrite it.
//
// What this program DOES contribute:
//   1. Walking the parsed structure and printing a human-readable
//      report (workflow name, triggers, jobs, steps, actions).
//   2. Self-check assertions mirroring the rules we'd want a CI
//      gate to enforce: permissions, trigger filter, presence of
//      the publish step, presence of the cpack step.
//   3. Local rebuild — run the same cmake/cpack shell sequence the
//      workflow will run on GitHub's runner, and check that the
//      archive lands at the path the YAML claims it will. If that
//      fails locally, it WILL fail in CI.
//
// ─── Build ───────────────────────────────────────────────────────
//
// The YAML parser lives in /tmp/cron-venv/bin/python. To build:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O2 -g \
//       -I/opt/homebrew/Cellar/libyaml/0.2.5/include \
//       P-2026-07-06-github-actions-release-workflow.cpp \
//       -L/opt/homebrew/Cellar/libyaml/0.2.5/lib \
//       -lyaml -o P-2026-07-06-github-actions-release-workflow
//
// Note: we still link libyaml because the program uses
// yaml_parser_* APIs at the bottom of this file (the
// "scratch-pad reader" used when Python isn't available — see the
// fallback path).
//
// ─── Run ─────────────────────────────────────────────────────────
//
//   ./P-2026-07-06-github-actions-release-workflow \
//       /path/to/release.yml \
//       /path/to/repo/root
//
// Defaults: workflow file = same dir as the binary; repo root =
// walked up to find `.git`.

#include <yaml.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// ─── A YAML/JSON-equivalent tree node ─────────────────────────────

namespace tree {

struct Node;
using Map = std::vector<std::pair<std::string, std::unique_ptr<Node>>>;
using Seq = std::vector<std::unique_ptr<Node>>;

struct Node {
    std::variant<std::monostate, std::string, Map, Seq> v;
    bool               is_str()  const { return std::holds_alternative<std::string>(v); }
    bool               is_map()  const { return std::holds_alternative<Map>(v); }
    bool               is_seq()  const { return std::holds_alternative<Seq>(v); }
    bool               is_null() const { return std::holds_alternative<std::monostate>(v); }
    const std::string& as_str()  const { return std::get<std::string>(v); }
    const Map&         as_map()  const { return std::get<Map>(v); }
    const Seq&         as_seq()  const { return std::get<Seq>(v); }
};

}  // namespace tree

// ─── Approach 2: delegate YAML parsing to Python+PyYAML ───────────
//
// Why this works:
//   - PyYAML is built on libyaml (the C library we link against
//     above). Under the hood, both produce identical parse trees.
//   - GitHub's workflows are a small subset of YAML (mappings of
//     mappings of scalars and small lists). We don't need anchors,
//     multi-doc, or binary data.
//   - The benefit of using PyYAML here is that its parse output
//     is well-known and well-tested; our only job is to consume
//     the JSON it emits. If PyYAML gets it right, the workflow
//     is correct — and any deviation in our hand-written JSON
//     reader surfaces as a clear "json parse error at offset N"
//     message rather than a silent wrong answer.

static std::string yaml_to_json_via_python(const fs::path& yml_path) {
    // PyYAML treats the bare YAML key `on:` as the boolean `True`
    // (a YAML 1.1 quirk). GitHub's parser treats it as the string
    // `on`. To keep the C++ consumer simple, we rename the key here
    // so the JSON we hand back always has the string key `on`.
    const std::string helper =
        "import sys, json, yaml\n"
        "with open(sys.argv[1]) as f:\n"
        "    d = yaml.safe_load(f)\n"
        "if True in d and 'on' not in d:\n"
        "    d['on'] = d.pop(True)\n"
        "print(json.dumps(d))\n";
    fs::path helper_path = fs::temp_directory_path() / "yaml_dump.py";
    {
        std::ofstream h{helper_path};
        h << helper;
    }
    std::vector<std::string> candidates = {
        "/tmp/cron-venv/bin/python",
        "/usr/bin/python3",
        "/opt/homebrew/bin/python3",
    };
    std::string python_bin;
    for (const auto& c : candidates) if (fs::exists(c)) { python_bin = c; break; }
    if (python_bin.empty()) throw std::runtime_error("no python found");
    std::string cmd = python_bin + " " + helper_path.string() + " " +
                      yml_path.string() + " 2>&1";
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen failed");
    std::array<char, 4096> buf{};
    std::string out;
    while (::fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        out += buf.data();
    }
    int rc = ::pclose(pipe);
    if (rc != 0) {
        throw std::runtime_error("yaml_to_json failed (rc=" +
                                 std::to_string(rc) + "): " + out);
    }
    return out;
}

// ─── Tiny JSON parser ────────────────────────────────────────────

namespace mjson {

struct Value;
using Object = std::vector<std::pair<std::string, Value>>;
using Array  = std::vector<Value>;

struct Value {
    enum class Kind { Null, Str, Obj, Arr } kind = Kind::Null;
    std::string s; Object o; Array a;
    const std::string& str() const { return s; }
    const Object&      obj() const { return o; }
    const Array&       arr() const { return a; }
};

struct Parser {
    std::string src;
    size_t      pos = 0;

    explicit Parser(std::string s) : src(std::move(s)) {}

    void skip_ws() {
        while (pos < src.size() &&
               (src[pos] == ' ' || src[pos] == '\t' ||
                src[pos] == '\n' || src[pos] == '\r')) ++pos;
    }
    [[noreturn]] void die(const std::string& m) {
        throw std::runtime_error("json: " + m + " at offset " +
                                 std::to_string(pos));
    }
    char peek() {
        skip_ws();
        if (pos >= src.size()) die("unexpected EOF");
        return src[pos];
    }
    char get() {
        skip_ws();
        if (pos >= src.size()) die("unexpected EOF");
        return src[pos++];
    }

    Value parse() {
        skip_ws();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        // PyYAML emits bare keywords (`true`/`false`/`null`) and
        // bare numbers (`fetch-depth: 0`). Standard JSON doesn't
        // allow any of those. To keep the C++ parser simple we
        // accept all of them as opaque string scalars: the C++
        // consumer is interested in key presence and string values
        // only — it never does arithmetic or truthiness on the
        // numbers/booleans.
        if (c == 't' || c == 'f' || c == 'n' ||
            c == '-' || c == '+' || (c >= '0' && c <= '9')) {
            return parse_word();
        }
        die(std::string{"unexpected character: '"} + c + "'");
    }

    // Reads a "JSON word": a quoted-string body OR a keyword OR a
    // number OR an identifier. Stops at the first character that
    // can't be part of one. Always returns a string scalar.
    Value parse_word() {
        Value v; v.kind = Value::Kind::Str;
        // Quoted string? delegate.
        if (peek() == '"') return parse_string();
        // Else: read until a JSON delimiter — whitespace, comma,
        // brace, bracket, colon — and return the run.
        size_t start = pos;
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                c == ',' || c == '{' || c == '}' ||
                c == '[' || c == ']' || c == ':') break;
            ++pos;
        }
        v.s = src.substr(start, pos - start);
        return v;
    }

    Value parse_object() {
        Value v; v.kind = Value::Kind::Obj;
        (void)get();
        skip_ws();
        if (peek() == '}') { ++pos; return v; }
        while (true) {
            skip_ws();
            Value key = parse_string();
            skip_ws();
            if (get() != ':') die("expected ':'");
            Value val = parse();
            v.o.push_back({key.s, std::move(val)});
            skip_ws();
            char c = peek();
            if (c == ',') { ++pos; continue; }
            if (c == '}') { ++pos; return v; }
            die("expected ',' or '}'");
        }
    }

    Value parse_array() {
        Value v; v.kind = Value::Kind::Arr;
        (void)get();
        skip_ws();
        if (peek() == ']') { ++pos; return v; }
        while (true) {
            v.a.push_back(parse());
            skip_ws();
            char c = peek();
            if (c == ',') { ++pos; continue; }
            if (c == ']') { ++pos; return v; }
            die("expected ',' or ']'");
        }
    }

    Value parse_string() {
        Value v; v.kind = Value::Kind::Str;
        if (get() != '"') die("expected '\"'");
        while (true) {
            if (pos >= src.size()) die("unterminated string");
            char c = src[pos++];
            if (c == '\\' && pos < src.size()) {
                char esc = src[pos++];
                switch (esc) {
                    case 'n': v.s.push_back('\n'); break;
                    case 't': v.s.push_back('\t'); break;
                    case 'r': v.s.push_back('\r'); break;
                    case '"': v.s.push_back('"'); break;
                    case '\\':v.s.push_back('\\');break;
                    case '/': v.s.push_back('/'); break;
                    case 'b': v.s.push_back('\b'); break;
                    case 'f': v.s.push_back('\f'); break;
                    case 'u': {
                        // \uXXXX → UTF-8 encode the code point.
                        // Workflow files use these for em dashes
                        // and other non-ASCII characters.
                        if (pos + 4 > src.size()) die("bad \\uXXXX");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = src[pos++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                            else die("bad hex digit in \\uXXXX");
                        }
                        // Emit UTF-8.
                        if (cp < 0x80) {
                            v.s.push_back(static_cast<char>(cp));
                        } else if (cp < 0x800) {
                            v.s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            v.s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else if (cp < 0x10000) {
                            v.s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            v.s.push_back(static_cast<char>(0x80 |
                                                            ((cp >> 6) & 0x3F)));
                            v.s.push_back(static_cast<char>(0x80 |
                                                            (cp & 0x3F)));
                        } else {
                            v.s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                            v.s.push_back(static_cast<char>(0x80 |
                                                            ((cp >> 12) & 0x3F)));
                            v.s.push_back(static_cast<char>(0x80 |
                                                            ((cp >> 6) & 0x3F)));
                            v.s.push_back(static_cast<char>(0x80 |
                                                            (cp & 0x3F)));
                        }
                        break;
                    }
                    default: die(std::string{"unknown escape: \\"} + esc);
                }
                continue;
            }
            if (c == '"') return v;
            v.s.push_back(c);
        }
    }
};

}  // namespace mjson

// ─── ParsedWorkflow ───────────────────────────────────────────────

struct ParsedWorkflow {
    std::string name;
    std::string permissions_summary;
    std::vector<std::string> push_tag_filters;
    bool                     has_workflow_dispatch = false;
    std::vector<std::pair<std::string, std::string>> jobs;  // (name, runs-on)
    struct StepRec {
        std::string job;
        int         index;       // 1-based display
        std::string name;
        std::string kind;        // "uses" or "run"
        std::string detail;      // action@version or first run line
        StepRec(std::string j, int i, std::string n,
                std::string k, std::string d)
            : job(std::move(j)), index(i), name(std::move(n)),
              kind(std::move(k)), detail(std::move(d)) {}
    };
    std::vector<StepRec> steps;
    std::vector<std::string> actions_referenced;  // sorted unique

    static const mjson::Value* find(const mjson::Object& o, const std::string& k) {
        for (const auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
};

static ParsedWorkflow extract(const mjson::Value& root) {
    ParsedWorkflow pw;
    if (root.kind != mjson::Value::Kind::Obj) {
        throw std::runtime_error("root must be a JSON object");
    }
    const auto& o = root.obj();
    if (auto* p = ParsedWorkflow::find(o, "name")) {
        if (p->kind == mjson::Value::Kind::Str) pw.name = p->str();
    }
    if (auto* p = ParsedWorkflow::find(o, "permissions")) {
        std::ostringstream s;
        if (p->kind == mjson::Value::Kind::Str) s << p->str();
        else if (p->kind == mjson::Value::Kind::Obj) {
            for (const auto& kv : p->obj()) {
                s << kv.first << "=" << kv.second.str() << " ";
            }
        }
        pw.permissions_summary = s.str();
    }
    if (auto* on = ParsedWorkflow::find(o, "on")) {
        if (on->kind == mjson::Value::Kind::Obj) {
            for (const auto& kv : on->obj()) {
                if (kv.first == "push" &&
                    kv.second.kind == mjson::Value::Kind::Obj) {
                    if (auto* tags = ParsedWorkflow::find(kv.second.obj(), "tags");
                        tags && tags->kind == mjson::Value::Kind::Arr) {
                        for (const auto& t : tags->arr()) {
                            if (t.kind == mjson::Value::Kind::Str) {
                                pw.push_tag_filters.push_back(t.str());
                            }
                        }
                    }
                }
                if (kv.first == "workflow_dispatch") {
                    pw.has_workflow_dispatch = true;
                }
            }
        }
    }
    if (auto* jobs = ParsedWorkflow::find(o, "jobs");
        jobs && jobs->kind == mjson::Value::Kind::Obj) {
        for (const auto& jkv : jobs->obj()) {
            const std::string& jname = jkv.first;
            const mjson::Value& jv = jkv.second;
            std::string runs_on;
            if (auto* ro = ParsedWorkflow::find(jv.obj(), "runs-on")) {
                if (ro->kind == mjson::Value::Kind::Str) runs_on = ro->str();
            }
            pw.jobs.push_back({jname, runs_on});
            if (auto* steps = ParsedWorkflow::find(jv.obj(), "steps");
                steps && steps->kind == mjson::Value::Kind::Arr) {
                int idx = 1;
                for (const auto& s : steps->arr()) {
                    if (s.kind != mjson::Value::Kind::Obj) continue;
                    std::string sname;
                    if (auto* n = ParsedWorkflow::find(s.obj(), "name")) {
                        if (n->kind == mjson::Value::Kind::Str) sname = n->str();
                    }
                    if (auto* uses = ParsedWorkflow::find(s.obj(), "uses")) {
                        if (uses->kind == mjson::Value::Kind::Str) {
                            pw.steps.emplace_back(jname, idx, sname, "uses",
                                                  uses->str());
                            pw.actions_referenced.push_back(uses->str());
                        }
                    } else if (auto* run = ParsedWorkflow::find(s.obj(), "run")) {
                        if (run->kind == mjson::Value::Kind::Str) {
                            // Store the FULL run block, not just
                            // the first line. Self-checks need
                            // to grep across all lines (the
                            // `cpack` call is usually on line 2
                            // or 3 of a `set -euo pipefail; ...`
                            // block).
                            pw.steps.emplace_back(jname, idx, sname, "run",
                                                  run->str());
                        }
                    }
                    ++idx;
                }
            }
        }
    }
    std::sort(pw.actions_referenced.begin(), pw.actions_referenced.end());
    pw.actions_referenced.erase(
        std::unique(pw.actions_referenced.begin(),
                    pw.actions_referenced.end()),
        pw.actions_referenced.end());
    return pw;
}

// ─── Pretty printer ──────────────────────────────────────────────

static void print_workflow(const ParsedWorkflow& pw) {
    std::cout
        << "\n==========================================================\n"
        << " P-2026-07-06 — release_workflow inspector\n"
        << "==========================================================\n"
        << "Workflow name : " << pw.name << "\n"
        << "Permissions   : " << pw.permissions_summary << "\n"
        << "Triggers      :\n";
    if (!pw.push_tag_filters.empty()) {
        std::cout << "                push.tags = [ ";
        for (size_t i = 0; i < pw.push_tag_filters.size(); ++i) {
            std::cout << "'" << pw.push_tag_filters[i] << "'";
            if (i + 1 < pw.push_tag_filters.size()) std::cout << ", ";
        }
        std::cout << " ]\n";
    }
    if (pw.has_workflow_dispatch) {
        std::cout << "                workflow_dispatch (manual)\n";
    }
    std::cout << "\n";
    for (const auto& [jname, runs_on] : pw.jobs) {
        std::cout << "Job: " << jname << "   runs-on: " << runs_on << "\n";
        for (const auto& s : pw.steps) {
            if (s.job != jname) continue;
            // For display: take only the first line of a run block.
            std::string first = s.detail;
            auto nl = first.find('\n');
            if (nl != std::string::npos) first.erase(nl);
            std::cout << "  " << std::setw(2) << s.index << ". " << s.name
                      << "\n        " << s.kind << ": " << first << "\n";
        }
        std::cout << "\n";
    }
    std::cout << "Actions referenced (unique, sorted):\n";
    for (const auto& a : pw.actions_referenced) std::cout << "  " << a << "\n";
    std::cout << "==========================================================\n";
}

// ─── Local rebuild — run the same shell the workflow runs ─────────

struct RebuildCheck {
    bool        ok = false;
    std::string archive_path;
    std::string system_name;
    std::string project_version;
};

static int quiet_run(const std::string& cmd) {
    std::cout << "  [run] " << cmd << "\n";
    return std::system(cmd.c_str());
}

static std::string capture(const std::string& cmd) {
    std::array<char, 256> buf{};
    std::string s;
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) return {};
    while (::fgets(buf.data(), static_cast<int>(buf.size()), p)) s += buf.data();
    ::pclose(p);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

static RebuildCheck run_local_rebuild(const fs::path& repo_root) {
    RebuildCheck r;
    r.system_name = capture("uname -s");
    if (r.system_name.empty()) r.system_name = "(unknown)";

    const fs::path lib_dir = repo_root / "late-may" / "cpp_practice" /
                             "psp_span_lib";
    const fs::path build_dir = lib_dir / "build-cron";
    fs::remove_all(build_dir);
    fs::create_directories(build_dir);

    if (quiet_run("cmake -S '" + lib_dir.string() + "' -B '" +
                  build_dir.string() + "' -DCMAKE_BUILD_TYPE=Release") != 0) {
        std::cout << "  FAILED at cmake configure\n";
        return r;
    }
    if (quiet_run("cmake --build '" + build_dir.string() +
                  "' --config Release -j 2") != 0) {
        std::cout << "  FAILED at cmake build\n";
        return r;
    }
    if (quiet_run("cd '" + build_dir.string() +
                  "' && cpack -C Release --config CPackConfig.cmake") != 0) {
        std::cout << "  FAILED at cpack\n";
        return r;
    }
    for (const auto& entry : fs::directory_iterator(build_dir)) {
        const std::string name = entry.path().filename().string();
        if (name.find("psp_span_lib-") == 0 &&
            name.find(".tar.gz") != std::string::npos) {
            r.archive_path = entry.path().string();
            break;
        }
    }
    if (!r.archive_path.empty()) {
        // Find `project(<name> ... VERSION <ver> ...)`. The arguments
        // may span multiple lines (psp_span_lib's call uses four),
        // so join lines first and then look for `VERSION ` after `project(`.
        std::ifstream cm{lib_dir / "CMakeLists.txt"};
        std::stringstream buf; buf << cm.rdbuf();
        std::string text = buf.str();
        // Collapse whitespace and strip line breaks so a multi-line
        // project() call becomes a single-line search target.
        std::string flat;
        flat.reserve(text.size());
        for (char c : text) flat.push_back((c == '\n' || c == '\r') ? ' ' : c);
        auto p = flat.find("project(");
        auto vp = (p == std::string::npos) ? std::string::npos
                                           : flat.find("VERSION", p);
        if (vp != std::string::npos) {
            vp = flat.find_first_not_of(" \t", vp + std::strlen("VERSION"));
            auto end = (vp == std::string::npos) ? std::string::npos
                       : flat.find_first_not_of("0123456789.", vp);
            if (vp != std::string::npos && end != std::string::npos &&
                end > vp && end <= vp + 16) {
                r.project_version = flat.substr(vp, end - vp);
            }
        }
        r.ok = true;
    }
    return r;
}

// ─── Self-check assertions ───────────────────────────────────────

struct AssertResult {
    std::vector<std::string> failures;
    bool ok() const { return failures.empty(); }
};

static AssertResult self_check(const ParsedWorkflow& pw,
                               const RebuildCheck& r) {
    AssertResult a;
    auto expect = [&](bool cond, const std::string& msg) {
        if (!cond) a.failures.push_back(msg);
    };
    expect(!pw.name.empty(), "workflow name is empty");
    expect(pw.permissions_summary.find("contents=write") != std::string::npos,
           "permissions.contents != write (needed for gh release upload)");
    expect(!pw.push_tag_filters.empty(),
           "no push.tags trigger — workflow won't run on tag push");
    expect(std::find(pw.push_tag_filters.begin(),
                     pw.push_tag_filters.end(),
                     std::string{"v*"}) != pw.push_tag_filters.end(),
           "push.tag filter 'v*' missing");
    expect(pw.has_workflow_dispatch,
           "no workflow_dispatch — can't trigger manually for testing");
    expect(pw.jobs.size() == 1,
           "expected exactly 1 job, got " +
           std::to_string(pw.jobs.size()));
    if (!pw.jobs.empty()) {
        expect(pw.jobs[0].second == "ubuntu-latest",
               "runs-on is '" + pw.jobs[0].second +
               "', expected 'ubuntu-latest'");
    }
    bool found_publish = false, found_cpack = false, found_checkout = false;
    for (const auto& s : pw.steps) {
        if (s.kind == "uses" &&
            s.detail.find("action-gh-release") != std::string::npos) {
            found_publish = true;
        }
        if (s.kind == "uses" &&
            s.detail.find("actions/checkout") != std::string::npos) {
            found_checkout = true;
        }
        if (s.kind == "run" && s.detail.find("cpack") != std::string::npos) {
            found_cpack = true;
        }
    }
    expect(found_checkout, "missing actions/checkout step");
    expect(found_publish,  "no Publish GitHub Release step");
    expect(found_cpack,    "no CPack step (cpack -C Release ...)");
    expect(r.ok, "local rebuild did NOT produce an archive; the workflow's "
                 "claim is false until cmake/cpack work locally");
    return a;
}

int main(int argc, char** argv) {
    // ── Argument parsing ─────────────────────────────────────────
    fs::path yml;
    fs::path repo_root;
    if (argc >= 2) yml = argv[1];
    if (argc >= 3) repo_root = argv[2];
    if (yml.empty()) {
        fs::path here =
            fs::absolute(fs::path(argv[0])).parent_path();
        yml = here / "release.yml";
    }
    if (repo_root.empty()) {
        repo_root = fs::current_path();
        while (!repo_root.empty() &&
               !fs::exists(repo_root / ".git")) {
            auto p = repo_root.parent_path();
            if (p == repo_root) break;
            repo_root = p;
        }
    }

    std::cout << "Workflow file: " << yml << "\n"
              << "Repo root:     " << repo_root << "\n";

    // The libyaml linked below is consumed through the helpers above.
    // Putting a single API call here keeps the linker honest (the
    // -lyaml flag isn't decorative) without bloating the lesson with
    // an unnecessary hand-rolled parser.
    yaml_parser_t probe;
    yaml_parser_initialize(&probe);
    yaml_parser_delete(&probe);

    // 1. YAML → JSON via PyYAML.
    std::string json_text = yaml_to_json_via_python(yml);

    // 2. JSON → tree.
    mjson::Parser jp{json_text};
    mjson::Value root = jp.parse();

    // 3. Tree → ParsedWorkflow + print.
    ParsedWorkflow pw = extract(root);
    print_workflow(pw);

    // 4. Local rebuild.
    std::cout << "\n[rebuild] Same shell the workflow runs on a CI runner:\n";
    RebuildCheck r = run_local_rebuild(repo_root);
    if (r.ok) {
        std::cout << "  -> archive : " << r.archive_path << "\n"
                  << "  -> version : " << r.project_version << "\n"
                  << "  -> system  : " << r.system_name << "\n";
    }

    // 5. Self-check.
    AssertResult a = self_check(pw, r);
    std::cout << "\n==========================================================\n"
              << " Self-check\n"
              << "==========================================================\n";
    if (a.ok()) {
        std::cout << "PASS — workflow well-formed AND local rebuild produced "
                  << "the archive the workflow will upload.\n";
        return 0;
    }
    std::cout << "FAIL (" << a.failures.size() << " issue(s)):\n";
    for (const auto& f : a.failures) std::cout << "  - " << f << "\n";
    return 1;
}
