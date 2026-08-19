// P-2026-08-19-dependabot-config-inspector.cpp
//
// DEPENDABOT CONFIG INSPECTOR FOR GitHub Actions — closes the
// natural follow-on the Aug 18 sha_pin_inspector lesson's
// "where we go next" section explicitly named:
//
//   > "Dependabot for Actions: a `.github/dependabot.yml` that
//      automatically opens a PR when a SHA-pinned action gets a
//      new upstream commit. The C++ inspector from today would
//      catch the PR's diff and verify that Dependabot rewrote
//      every `@<sha>` correctly."
//
// What this program does
// ----------------------
//
// Reads a Dependabot config YAML (`.github/dependabot.yml`),
// walks every `updates:` block, classifies each block by how
// aggressively it pulls upstream changes, and reports the
// result. Six rules are checked:
//
//   R1  At least one `package-ecosystem: github-actions` block
//       exists  (the lesson's whole point — we want Dependabot
//       to track GitHub Actions at all)
//   R2  Every github-actions block declares a `schedule.interval`
//       that is one of {daily, weekly}  (monthly is too slow for
//       security-relevant action updates; never-schedule is a
//       no-op)
//   R3  Every github-actions block declares a `groups:` map with
//       at least one named group  (otherwise Dependabot opens
//       one PR per action, which floods the queue)
//   R4  Every github-actions block declares a `commit-message`
//       block whose prefix starts with "chore(deps)" or
//       "ci(deps)"  (so the auto-PRs don't pollute the commit
//       log with non-conventional messages)
//   R5  Every github-actions block declares a non-zero
//       `open-pull-requests-limit`  (the default is 5, but the
//       human should set it explicitly so the choice is visible
//       in code review)
//   R6  Every github-actions block declares a `labels:` array
//       with at least one label  (so Dependabot PRs are
//       filterable by label in the GitHub UI)
//
// Self-check rules output:
//   PASS — every rule satisfied
//   FAIL — one or more rules violated, with a per-rule report
//
// What this program does NOT do
// -----------------------------
//
// We do NOT verify that Dependabot is actually enabled on the
// repo (that's a separate API call). We do NOT verify that the
// generated PRs actually re-pin to a SHA — that's what
// yesterday's sha_pin_inspector does on the resulting diff. We
// do NOT verify that the schedule interval is a valid cron
// expression (Dependabot accepts named intervals only).
//
// Build (assumes PyYAML is at /tmp/pylib, same setup Aug 18
// used; this lesson does NOT link against libyaml because we
// outsource parsing to PyYAML via a one-shot subprocess, exactly
// the same pattern Aug 18 used for YAML → JSON):
//
//   clang++ -std=c++17 -O2 -g -Wall -Wextra -Wpedantic \
//       -I/opt/homebrew/include \
//       P-2026-08-19-dependabot-config-inspector.cpp \
//       -o P-2026-08-19-dependabot-config-inspector
//
// Run:
//
//   ./P-2026-08-19-dependabot-config-inspector dependabot_good.yml
//   ./P-2026-08-19-dependabot-config-inspector dependabot_no_ga.yml
//   ./P-2026-08-19-dependabot-config-inspector dependabot_minimal.yml
//
// Exit code: 0 if every rule is satisfied, 1 otherwise.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

// ===========================================================================
// 1. RuleResult: the per-rule verdict
// ===========================================================================
//
// Each self-check rule produces one RuleResult. The final
// self-check aggregates all RuleResults and decides PASS / FAIL.
//
// C++17 scoped enum + free-function name() makes the verdict
// printable in the report and unit-testable.

enum class Verdict { Pass, Fail };

static const char* verdict_name(Verdict v) {
    switch (v) {
        case Verdict::Pass: return "PASS";
        case Verdict::Fail: return "FAIL";
    }
    return "?";
}

struct RuleResult {
    std::string rule_id;     // e.g. "R1"
    std::string rule_name;   // e.g. "github-actions ecosystem present"
    Verdict      verdict = Verdict::Pass;
    std::string  detail;     // human-readable explanation on failure
};

// ===========================================================================
// 2. YAML → JSON via PyYAML (same trick as Aug 18)
// ===========================================================================

static std::string yaml_to_json_via_python(const fs::path& yml_path) {
    const std::string helper =
        "import sys, json, yaml\n"
        "with open(sys.argv[1]) as f:\n"
        "    d = yaml.safe_load(f)\n"
        "if d is None:\n"
        "    d = {}\n"
        "print(json.dumps(d))\n";
    fs::path helper_path = fs::temp_directory_path() / "yaml_dump_dep.py";
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
    std::string cmd =
        std::string{"PYTHONPATH=/tmp/pylib "} +
        python_bin + " " + helper_path.string() + " " +
        yml_path.string() + " 2>&1";
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen failed");
    std::array<char, 4096> buf{};
    std::string out;
    while (::fgets(buf.data(), static_cast<int>(buf.size()), pipe)) out += buf.data();
    int rc = ::pclose(pipe);
    if (rc != 0) {
        throw std::runtime_error("yaml_to_json failed (rc=" +
                                 std::to_string(rc) + "): " + out);
    }
    return out;
}

// ===========================================================================
// 3. Tiny JSON parser (recursive descent — byte-identical to Aug 18)
// ===========================================================================

namespace mjson {
struct Value;
using Object = std::vector<std::pair<std::string, Value>>;
using Array  = std::vector<Value>;

struct Value {
    enum class Kind { Null, Str, Obj, Arr } kind = Kind::Null;
    std::string s;
    Object      o;
    Array       a;
    bool        is_str()  const { return kind == Kind::Str; }
    bool        is_obj()  const { return kind == Kind::Obj; }
    bool        is_arr()  const { return kind == Kind::Arr; }
    const std::string& str() const { return s; }
    const Object&      obj() const { return o; }
    const Array&       arr() const { return a; }
};

struct Parser {
    std::string src;
    std::size_t pos = 0;

    explicit Parser(std::string s) : src(std::move(s)) {}

    void skip_ws() {
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos;
            else break;
        }
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
        if (c == 't' || c == 'f' || c == 'n' ||
            c == '-' || c == '+' || (c >= '0' && c <= '9')) {
            return parse_word();
        }
        die(std::string{"unexpected character: '"} + c + "'");
    }

    Value parse_word() {
        if (peek() == '"') return parse_string();
        std::size_t start = pos;
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                c == ',' || c == '{' || c == '}' ||
                c == '[' || c == ']' || c == ':') break;
            ++pos;
        }
        Value v; v.kind = Value::Kind::Str;
        v.s = src.substr(start, pos - start);
        return v;
    }

    Value parse_object() {
        Value v; v.kind = Value::Kind::Obj;
        (void)get();
        skip_ws();
        if (peek() == '}') { ++pos; return v; }
        while (true) {
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
                    case '"': v.s.push_back('"');  break;
                    case '\\':v.s.push_back('\\'); break;
                    case '/': v.s.push_back('/');  break;
                    case 'b': v.s.push_back('\b'); break;
                    case 'f': v.s.push_back('\f'); break;
                    case 'u': {
                        if (pos + 4 > src.size()) die("bad \\uXXXX");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            unsigned char h = static_cast<unsigned char>(src[pos++]);
                            unsigned nybble = 0;
                            if      (h >= '0' && h <= '9') nybble = static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') nybble = static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') nybble = static_cast<unsigned>(h - 'A' + 10);
                            else die("bad hex digit in \\uXXXX");
                            cp = (cp << 4) | nybble;
                        }
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

// ===========================================================================
// 4. Walk the parsed tree to extract every updates[] block
// ===========================================================================

struct UpdatesBlock {
    std::string directory;        // "/" for repo-root
    std::string package_ecosystem; // "github-actions", "npm", "pip", ...
    std::string schedule_interval; // "" if absent
    std::string commit_prefix;    // "" if absent
    int         open_pr_limit = -1; // -1 if absent
    bool        has_groups      = false;
    bool        has_labels      = false;
};

static const mjson::Value* find_obj(const mjson::Object& o, const std::string& k) {
    for (const auto& kv : o) if (kv.first == k) return &kv.second;
    return nullptr;
}

static std::string get_str_or(const mjson::Object& o,
                              const std::string& key,
                              const std::string& dflt) {
    if (auto* v = find_obj(o, key); v && v->is_str()) return v->str();
    return dflt;
}

static int get_int_or(const mjson::Object& o,
                      const std::string& key,
                      int dflt) {
    if (auto* v = find_obj(o, key); v && v->is_str()) {
        try { return std::stoi(v->str()); }
        catch (...) { return dflt; }
    }
    return dflt;
}

static UpdatesBlock extract_block(const mjson::Value& v) {
    UpdatesBlock b;
    if (!v.is_obj()) return b;
    const auto& o = v.obj();
    b.directory = get_str_or(o, "directory", "/");
    b.package_ecosystem = get_str_or(o, "package-ecosystem", "");
    if (auto* s = find_obj(o, "schedule"); s && s->is_obj()) {
        b.schedule_interval = get_str_or(s->obj(), "interval", "");
    }
    if (auto* c = find_obj(o, "commit-message"); c && c->is_obj()) {
        b.commit_prefix = get_str_or(c->obj(), "prefix", "");
    }
    b.open_pr_limit = get_int_or(o, "open-pull-requests-limit", -1);
    b.has_groups    = find_obj(o, "groups") != nullptr;
    b.has_labels    = find_obj(o, "labels")  != nullptr;
    return b;
}

static std::vector<UpdatesBlock> extract_updates(const mjson::Value& root) {
    std::vector<UpdatesBlock> out;
    if (!root.is_obj()) return out;
    const auto* upd_v = find_obj(root.obj(), "updates");
    if (!upd_v || !upd_v->is_arr()) return out;
    for (const auto& u : upd_v->arr()) out.push_back(extract_block(u));
    return out;
}

// ===========================================================================
// 5. Self-check: six rules over the extracted blocks
// ===========================================================================

static std::vector<RuleResult> self_check(const std::vector<UpdatesBlock>& blocks) {
    std::vector<RuleResult> rs;

    // R1: at least one github-actions block
    {
        RuleResult r;
        r.rule_id = "R1";
        r.rule_name = "github-actions ecosystem present";
        int n_ga = 0;
        for (const auto& b : blocks) if (b.package_ecosystem == "github-actions") ++n_ga;
        if (n_ga >= 1) {
            r.verdict = Verdict::Pass;
            r.detail  = "found " + std::to_string(n_ga) +
                        " github-actions block(s)";
        } else {
            r.verdict = Verdict::Fail;
            r.detail  = "no `package-ecosystem: github-actions` block — "
                        "Dependabot is not tracking GitHub Actions";
        }
        rs.push_back(std::move(r));
    }

    // R2: every github-actions block has schedule.interval in {daily, weekly}
    {
        RuleResult r;
        r.rule_id = "R2";
        r.rule_name = "schedule.interval in {daily, weekly}";
        std::vector<std::string> bad;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& b = blocks[i];
            if (b.package_ecosystem != "github-actions") continue;
            if (b.schedule_interval != "daily" &&
                b.schedule_interval != "weekly") {
                bad.push_back("block[" + std::to_string(i) +
                              "] schedule.interval=\"" +
                              b.schedule_interval + "\"");
            }
        }
        if (bad.empty()) {
            r.verdict = Verdict::Pass;
            r.detail  = "every github-actions block has a daily/weekly interval";
        } else {
            r.verdict = Verdict::Fail;
            r.detail  = std::to_string(bad.size()) +
                        " github-actions block(s) have non-daily/weekly interval: ";
            for (std::size_t i = 0; i < bad.size(); ++i) {
                if (i) r.detail += ", ";
                r.detail += bad[i];
            }
        }
        rs.push_back(std::move(r));
    }

    // R3: every github-actions block declares groups
    {
        RuleResult r;
        r.rule_id = "R3";
        r.rule_name = "groups declared (avoid PR flood)";
        std::vector<std::string> bad;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& b = blocks[i];
            if (b.package_ecosystem != "github-actions") continue;
            if (!b.has_groups) {
                bad.push_back("block[" + std::to_string(i) + "]");
            }
        }
        if (bad.empty()) {
            r.verdict = Verdict::Pass;
            r.detail  = "every github-actions block declares a `groups:` map";
        } else {
            r.verdict = Verdict::Fail;
            r.detail  = std::to_string(bad.size()) +
                        " github-actions block(s) have no `groups:` map: ";
            for (std::size_t i = 0; i < bad.size(); ++i) {
                if (i) r.detail += ", ";
                r.detail += bad[i];
            }
        }
        rs.push_back(std::move(r));
    }

    // R4: commit-message.prefix starts with chore(deps) or ci(deps)
    {
        RuleResult r;
        r.rule_id = "R4";
        r.rule_name = "commit-message.prefix is conventional";
        std::vector<std::string> bad;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& b = blocks[i];
            if (b.package_ecosystem != "github-actions") continue;
            auto starts_with = [](const std::string& s, const std::string& p) {
                return s.size() >= p.size() &&
                       s.compare(0, p.size(), p) == 0;
            };
            if (!starts_with(b.commit_prefix, "chore(deps)") &&
                !starts_with(b.commit_prefix, "ci(deps)")) {
                bad.push_back("block[" + std::to_string(i) +
                              "] commit-message.prefix=\"" +
                              b.commit_prefix + "\"");
            }
        }
        if (bad.empty()) {
            r.verdict = Verdict::Pass;
            r.detail  = "every github-actions block has chore(deps)/ci(deps) prefix";
        } else {
            r.verdict = Verdict::Fail;
            r.detail  = std::to_string(bad.size()) +
                        " github-actions block(s) have non-conventional prefix: ";
            for (std::size_t i = 0; i < bad.size(); ++i) {
                if (i) r.detail += ", ";
                r.detail += bad[i];
            }
        }
        rs.push_back(std::move(r));
    }

    // R5: open-pull-requests-limit > 0
    {
        RuleResult r;
        r.rule_id = "R5";
        r.rule_name = "open-pull-requests-limit > 0";
        std::vector<std::string> bad;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& b = blocks[i];
            if (b.package_ecosystem != "github-actions") continue;
            if (b.open_pr_limit <= 0) {
                bad.push_back("block[" + std::to_string(i) +
                              "] open-pull-requests-limit=" +
                              std::to_string(b.open_pr_limit));
            }
        }
        if (bad.empty()) {
            r.verdict = Verdict::Pass;
            r.detail  = "every github-actions block has a positive PR limit";
        } else {
            r.verdict = Verdict::Fail;
            r.detail  = std::to_string(bad.size()) +
                        " github-actions block(s) have a missing or zero PR limit: ";
            for (std::size_t i = 0; i < bad.size(); ++i) {
                if (i) r.detail += ", ";
                r.detail += bad[i];
            }
        }
        rs.push_back(std::move(r));
    }

    // R6: labels array present with at least one entry
    {
        RuleResult r;
        r.rule_id = "R6";
        r.rule_name = "labels array present";
        std::vector<std::string> bad;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& b = blocks[i];
            if (b.package_ecosystem != "github-actions") continue;
            if (!b.has_labels) {
                bad.push_back("block[" + std::to_string(i) + "]");
            }
        }
        if (bad.empty()) {
            r.verdict = Verdict::Pass;
            r.detail  = "every github-actions block declares a `labels:` array";
        } else {
            r.verdict = Verdict::Fail;
            r.detail  = std::to_string(bad.size()) +
                        " github-actions block(s) have no `labels:` array: ";
            for (std::size_t i = 0; i < bad.size(); ++i) {
                if (i) r.detail += ", ";
                r.detail += bad[i];
            }
        }
        rs.push_back(std::move(r));
    }

    return rs;
}

// ===========================================================================
// 6. Pretty-print the report
// ===========================================================================

static void print_blocks(const std::vector<UpdatesBlock>& blocks) {
    std::cout << "\n"
              << "============================================================\n"
              << " P-2026-08-19 — dependabot_config_inspector\n"
              << "============================================================\n";
    if (blocks.empty()) {
        std::cout << "(no `updates:` blocks found in this dependabot.yml)\n";
        return;
    }
    std::cout << "  " << std::setw(2) << "#"
              << "  " << std::setw(15) << "ECOSYSTEM"
              << "  " << std::setw(10) << "INTERVAL"
              << "  " << std::setw(15) << "PREFIX"
              << "  " << std::setw(5) << "PRLIM"
              << "  " << std::setw(7) << "GROUPS"
              << "  " << std::setw(6) << "LABELS"
              << "  DIRECTORY\n";
    std::cout << "  " << std::string(2, '-')
              << "  " << std::string(15, '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(15, '-')
              << "  " << std::string(5, '-')
              << "  " << std::string(7, '-')
              << "  " << std::string(6, '-')
              << "  ---------\n";
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        std::cout << "  " << std::setw(2) << i
                  << "  " << std::setw(15) << b.package_ecosystem
                  << "  " << std::setw(10) << b.schedule_interval
                  << "  " << std::setw(15) << b.commit_prefix
                  << "  " << std::setw(5)
                  << (b.open_pr_limit >= 0
                          ? std::to_string(b.open_pr_limit) : "-")
                  << "  " << std::setw(7) << (b.has_groups ? "yes" : "no")
                  << "  " << std::setw(6) << (b.has_labels ? "yes" : "no")
                  << "  " << b.directory << "\n";
    }
}

static void print_rules(const std::vector<RuleResult>& rs) {
    std::cout << "\n"
              << "============================================================\n"
              << " Self-check\n"
              << "============================================================\n";
    int n_pass = 0, n_fail = 0;
    for (const auto& r : rs) {
        std::cout << "  " << r.rule_id << "  " << std::setw(4)
                  << verdict_name(r.verdict) << "  " << r.rule_name
                  << " — " << r.detail << "\n";
        if (r.verdict == Verdict::Pass) ++n_pass;
        else ++n_fail;
    }
    std::cout << "\n  " << n_pass << " / " << rs.size() << " rules PASS";
    if (n_fail) std::cout << " (" << n_fail << " FAIL)";
    std::cout << "\n";
}

// ===========================================================================
// 7. Inline unit tests for the rule engine
// ===========================================================================

struct UnitTestResult {
    std::vector<std::pair<std::string, bool>> cases;
    bool ok() const {
        for (const auto& [_, passed] : cases) if (!passed) return false;
        return true;
    }
    int total() const { return static_cast<int>(cases.size()); }
    int passed() const {
        int n = 0;
        for (const auto& [_, p] : cases) if (p) ++n;
        return n;
    }
};

// Build a tiny UpdatesBlock with the given fields. Used by the
// inline unit tests below.
static UpdatesBlock make_block(std::string eco, std::string interval,
                               std::string prefix, int pr_limit,
                               bool groups, bool labels) {
    UpdatesBlock b;
    b.package_ecosystem = std::move(eco);
    b.schedule_interval = std::move(interval);
    b.commit_prefix     = std::move(prefix);
    b.open_pr_limit     = pr_limit;
    b.has_groups        = groups;
    b.has_labels        = labels;
    return b;
}

static UnitTestResult unit_test_rules() {
    UnitTestResult u;
    auto check_rules = [&](const std::vector<UpdatesBlock>& blocks,
                           const std::string& label,
                           std::vector<std::pair<std::string, Verdict>> want) {
        std::vector<RuleResult> got = self_check(blocks);
        bool ok = (got.size() == want.size());
        for (std::size_t i = 0; ok && i < want.size(); ++i) {
            if (got[i].rule_id != want[i].first ||
                got[i].verdict != want[i].second) ok = false;
        }
        u.cases.emplace_back(label, ok);
        if (!ok) {
            std::cout << "  FAIL: " << label << "\n";
            std::cout << "    want: ";
            for (std::size_t i = 0; i < want.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << want[i].first << "="
                            << verdict_name(want[i].second);
            }
            std::cout << "\n    got:  ";
            for (std::size_t i = 0; i < got.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << got[i].rule_id << "="
                            << verdict_name(got[i].verdict);
            }
            std::cout << "\n";
        }
    };

    // U1: empty config — every rule fails (no github-actions block
    // at all is the headline failure).
    check_rules({}, "U1 empty config fails R1..R6",
                {{"R1", Verdict::Fail},
                 {"R2", Verdict::Pass},
                 {"R3", Verdict::Pass},
                 {"R4", Verdict::Pass},
                 {"R5", Verdict::Pass},
                 {"R6", Verdict::Pass}});

    // U2: a single fully-correct github-actions block — every
    // rule passes.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "weekly", "chore(deps)", 5, true, true));
        check_rules(blocks, "U2 single fully-correct block passes all 6 rules",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Pass}});
    }

    // U3: github-actions block with monthly interval — R2 fails.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "monthly", "chore(deps)", 5, true, true));
        check_rules(blocks, "U3 monthly interval fails R2 only",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Fail},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Pass}});
    }

    // U4: github-actions block without groups — R3 fails.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "daily", "ci(deps)", 5, false, true));
        check_rules(blocks, "U4 missing groups fails R3 only",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Fail},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Pass}});
    }

    // U5: github-actions block with a non-conventional prefix —
    // R4 fails. (Conventional means chore(deps) or ci(deps).)
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "weekly", "update-deps", 5, true, true));
        check_rules(blocks, "U5 non-conventional prefix fails R4 only",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Fail},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Pass}});
    }

    // U6: github-actions block with PR limit 0 — R5 fails.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "weekly", "chore(deps)", 0, true, true));
        check_rules(blocks, "U6 PR limit 0 fails R5 only",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Fail},
                     {"R6", Verdict::Pass}});
    }

    // U7: github-actions block without labels — R6 fails.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "weekly", "chore(deps)", 5, true, false));
        check_rules(blocks, "U7 missing labels fails R6 only",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Fail}});
    }

    // U8: a config that has a non-github-actions block (npm) but
    // NO github-actions block — R1 fails; the other rules stay
    // pass because the R2..R6 loops only consider github-actions
    // blocks (which there are zero of, so the bad-list is empty).
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "npm", "weekly", "chore(deps)", 5, true, true));
        check_rules(blocks, "U8 only-npm config fails R1 only",
                    {{"R1", Verdict::Fail},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Pass}});
    }

    // U9: two github-actions blocks — one fully-correct, one
    // with multiple problems. R2..R6 each fail because of the
    // broken block.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "weekly", "chore(deps)", 5, true, true));
        blocks.push_back(make_block(
            "github-actions", "monthly", "update-deps", 0, false, false));
        check_rules(blocks, "U9 mixed config fails R2,R3,R4,R5,R6",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Fail},
                     {"R3", Verdict::Fail},
                     {"R4", Verdict::Fail},
                     {"R5", Verdict::Fail},
                     {"R6", Verdict::Fail}});
    }

    // U10: two github-actions blocks, both fully-correct, plus a
    // non-github-actions block. All six rules pass.
    {
        std::vector<UpdatesBlock> blocks;
        blocks.push_back(make_block(
            "github-actions", "weekly", "chore(deps)", 5, true, true));
        blocks.push_back(make_block(
            "github-actions", "daily", "ci(deps)", 3, true, true));
        blocks.push_back(make_block(
            "npm", "weekly", "chore(deps)", 5, true, true));
        check_rules(blocks, "U10 multi-block config passes all 6 rules",
                    {{"R1", Verdict::Pass},
                     {"R2", Verdict::Pass},
                     {"R3", Verdict::Pass},
                     {"R4", Verdict::Pass},
                     {"R5", Verdict::Pass},
                     {"R6", Verdict::Pass}});
    }

    return u;
}

// ===========================================================================
// 8. main()
// ===========================================================================

int main(int argc, char** argv) {
    // ─── Argument parsing ─────────────────────────────────────────
    fs::path yml;
    if (argc >= 2) {
        yml = argv[1];
    } else {
        fs::path here = fs::absolute(fs::path(argv[0])).parent_path();
        yml = here / "dependabot_good.yml";
    }
    if (!fs::exists(yml)) {
        std::cerr << "File not found: " << yml << "\n";
        return 1;
    }
    std::cout << "Dependabot file: " << yml << "\n";

    // ─── Inline unit tests for the rule engine ────────────────────
    std::cout << "\n============================================================\n"
              << " Unit tests (rule engine)\n"
              << "============================================================\n";
    UnitTestResult ut = unit_test_rules();
    std::cout << "  " << ut.passed() << " / " << ut.total() << " PASS\n";
    bool unit_ok = ut.ok();

    // ─── YAML → JSON via PyYAML ───────────────────────────────────
    std::string json_text;
    try {
        json_text = yaml_to_json_via_python(yml);
    } catch (const std::exception& e) {
        std::cerr << "YAML→JSON failed: " << e.what() << "\n";
        return 1;
    }

    // ─── JSON → tree ──────────────────────────────────────────────
    mjson::Parser jp{json_text};
    mjson::Value root;
    try {
        root = jp.parse();
    } catch (const std::exception& e) {
        std::cerr << "JSON parse failed: " << e.what() << "\n";
        return 1;
    }

    // ─── Tree → updates blocks ────────────────────────────────────
    std::vector<UpdatesBlock> blocks = extract_updates(root);

    // ─── Print the report ─────────────────────────────────────────
    print_blocks(blocks);

    // ─── Self-check ───────────────────────────────────────────────
    std::vector<RuleResult> rs = self_check(blocks);
    print_rules(rs);

    // ─── Decide exit code ─────────────────────────────────────────
    bool rules_ok = true;
    for (const auto& r : rs) if (r.verdict != Verdict::Pass) rules_ok = false;
    if (unit_ok && rules_ok) {
        std::cout << "\nPASS — every rule is satisfied and every unit "
                  << "test passes.\n";
        return 0;
    }
    if (!unit_ok) std::cout << "\nFAIL — unit tests did not all pass "
                            << "(see above).\n";
    if (!rules_ok) std::cout << "\nFAIL — one or more rules are violated "
                             << "(see above).\n";
    return 1;
}
