// P-2026-08-18-sha-pin-inspector.cpp
//
// SHA-PIN INSPECTOR FOR GitHub Actions WORKFLOWS — closes the
// forward-on item the Aug 15 lesson's "where we go next" section
// explicitly named:
//
//   > "Pin actions to commit SHAs — @v4 is a floating tag"
//
// (Also: this is item #1 on the cross-cutting forward-on list the
// Jul 8 multi-lesson's "Next steps" section enumerated — the
// pinning was always next.)
//
// What this program does
// ----------------------
//
// Reads a workflow YAML file, walks every job's `steps:` list,
// classifies each `uses:` line by how it is pinned, and reports
// the result. Five pin shapes:
//
//   Pinned        — uses: foo/bar@<40-hex-sha>     ✓
//   MajorVersion  — uses: foo/bar@v<digit>...     ⚠ floating tag
//   Branch        — uses: foo/bar@<branch-name>    ⚠ floating ref
//   Unpinned      — uses: foo/bar                 ✗ no @ at all
//   Malformed     — uses: foo/bar@<short-hex>     ✗ wrong shape
//
// Self-check rules:
//   * At least one Pinned `uses:` is REQUIRED (the lesson's whole
//     point — we want to assert that someone DID the work)
//   * Zero MajorVersion / Branch / Unpinned / Malformed is the
//     "all pinned" ideal — this is the assertion the GOOD fixture
//     passes and the BAD / MIXED fixtures fail.
//
// We do NOT verify that the SHAs exist on GitHub. That would
// require network access (an offline SHA-validity check is a
// future lesson — see "What is NOT in this lesson" below).
//
// Build (assumes libyaml is at /opt/homebrew and PyYAML is at
// /tmp/pylib, exactly the same setup the Jul 6 / Jul 8 inspectors
// used):
//
//   clang++ -std=c++17 -O2 -g -Wall -Wextra -Wpedantic \
//       -I/opt/homebrew/include \
//       P-2026-08-18-sha-pin-inspector.cpp \
//       -L/opt/homebrew/lib -lyaml \
//       -o P-2026-08-18-sha-pin-inspector
//
// Run:
//
//   ./P-2026-08-18-sha-pin-inspector release_sha_pinned.yml
//   ./P-2026-08-18-sha-pin-inspector release_floating.yml
//   ./P-2026-08-18-sha-pin-inspector release_mixed_pin.yml
//
// Exit code: 0 if the file is fully pinned, 1 otherwise.

#include <yaml.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
// 1. The PinStatus enum
// ===========================================================================
//
// C++17 scoped enum. Each value has a printable label. Five cases
// cover every shape a `uses:` line can take:
//
//   - Pinned       — 40-hex SHA after the @
//   - MajorVersion — @v<n> or @v<n>.<m> floating tag
//   - Branch       — @<anything-that-isn't-a-sha-or-v-tag> (branch ref)
//   - Unpinned     — no @ at all (just "owner/repo")
//   - Malformed    — @<short-hex> (e.g. @abcdef1 — a SHA must be 40 chars)

enum class PinStatus {
    Pinned,
    MajorVersion,
    Branch,
    Unpinned,
    Malformed,
};

static const char* pin_status_name(PinStatus s) {
    switch (s) {
        case PinStatus::Pinned:       return "Pinned";
        case PinStatus::MajorVersion: return "MajorVersion";
        case PinStatus::Branch:       return "Branch";
        case PinStatus::Unpinned:     return "Unpinned";
        case PinStatus::Malformed:    return "Malformed";
    }
    return "?";
}

// is_lowercase_hex_char: single-character predicate. Used by
// is_40_hex_sha() below. Kept as a free function so it's
// trivially unit-testable.
static bool is_lowercase_hex_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

// is_40_hex_sha: 40 chars, each one a lowercase hex digit. SHA-1
// is 160 bits = 40 hex chars; git's commit SHAs are SHA-1. (The
// new SHA-256 commit objects are 64 hex chars; GitHub's
// recommended action pins are still SHA-1 today — this lesson
// targets the current recommended shape.)
//
// We lowercase the input first, so `B4FFDE65...` (which GitHub's
// UI sometimes shows) is also accepted.
static bool is_40_hex_sha(const std::string& s) {
    if (s.size() != 40) return false;
    for (char c : s) {
        if (!is_lowercase_hex_char(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))))) {
            return false;
        }
    }
    return true;
}

// classify_pin: split "owner/repo@<ref>" into owner/repo + ref, then
// dispatch on the ref's shape.
//
//   owner/repo          → Unpinned
//   owner/repo@v1       → MajorVersion (floating)
//   owner/repo@v2.x     → MajorVersion (floating — the @v2.x form
//                                   is technically a major-version
//                                   pin in SemVer but still moves
//                                   within the v2 line; treat as
//                                   floating for this lesson)
//   owner/repo@main     → Branch (floating)
//   owner/repo@b4ff...  → Pinned (40-hex SHA)
//   owner/repo@abcdef1  → Malformed (short SHA, not 40 chars)
//
// The split is on the LAST '@' — owner/repo contains no '@', so a
// single split is unambiguous. (GitHub action refs don't allow
// nested @ in owner/repo names today, but using rfind is robust
// against any future weirdness.)
static PinStatus classify_pin(const std::string& uses_value) {
    auto at = uses_value.rfind('@');
    if (at == std::string::npos) {
        return PinStatus::Unpinned;
    }
    const std::string ref = uses_value.substr(at + 1);
    if (ref.empty()) {
        return PinStatus::Malformed;
    }
    // @v<n> or @v<n>.<m>... — major-version tag.
    if (ref[0] == 'v' && ref.size() >= 2 &&
        std::isdigit(static_cast<unsigned char>(ref[1]))) {
        return PinStatus::MajorVersion;
    }
    // @<40-hex> — SHA pin.
    if (is_40_hex_sha(ref)) {
        return PinStatus::Pinned;
    }
    // @<short-hex> (anything hex-shaped but wrong length) — malformed.
    bool all_hex = true;
    for (char c : ref) {
        if (!is_lowercase_hex_char(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))))) {
            all_hex = false;
            break;
        }
    }
    if (all_hex) {
        return PinStatus::Malformed;
    }
    // Otherwise: branch name (e.g. `main`, `feature/foo`).
    return PinStatus::Branch;
}

// ===========================================================================
// 2. YAML → JSON via PyYAML (same trick as Jul 6 / Jul 8)
// ===========================================================================

static std::string yaml_to_json_via_python(const fs::path& yml_path) {
    const std::string helper =
        "import sys, json, yaml\n"
        "with open(sys.argv[1]) as f:\n"
        "    d = yaml.safe_load(f)\n"
        "if True in d and 'on' not in d:\n"
        "    d['on'] = d.pop(True)\n"
        "print(json.dumps(d))\n";
    fs::path helper_path = fs::temp_directory_path() / "yaml_dump_pin.py";
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
    // Inject PYTHONPATH=/tmp/pylib into the helper's environment so
    // the PyYAML install we made via `uv pip install --target /tmp/pylib
    // pyyaml` is findable regardless of whether the cron shell set
    // PYTHONPATH itself. Same trick the Jul 6 / Jul 8 lessons use.
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
// 3. Tiny JSON parser (recursive descent, same shape as Jul 6 / Jul 8)
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
// 4. Walk the parsed tree to extract every `uses:` step
// ===========================================================================

struct UsesRecord {
    std::string job;
    int         step_index;     // 1-based
    std::string step_name;
    std::string uses_value;    // raw "owner/repo@<ref>"
    PinStatus   status;
};

static const mjson::Value* find_obj(const mjson::Object& o, const std::string& k) {
    for (const auto& kv : o) if (kv.first == k) return &kv.second;
    return nullptr;
}

static std::vector<UsesRecord> extract_uses(const mjson::Value& root) {
    std::vector<UsesRecord> out;
    if (!root.is_obj()) return out;
    const auto* jobs_v = find_obj(root.obj(), "jobs");
    if (!jobs_v || !jobs_v->is_obj()) return out;
    for (const auto& jkv : jobs_v->obj()) {
        const std::string& jname = jkv.first;
        const mjson::Value& jv = jkv.second;
        if (!jv.is_obj()) continue;
        const auto* steps_v = find_obj(jv.obj(), "steps");
        if (!steps_v || !steps_v->is_arr()) continue;
        int idx = 1;
        for (const auto& s : steps_v->arr()) {
            if (!s.is_obj()) { ++idx; continue; }
            const auto* uses_v = find_obj(s.obj(), "uses");
            if (uses_v && uses_v->is_str()) {
                UsesRecord r;
                r.job        = jname;
                r.step_index = idx;
                r.step_name  = "(unnamed)";
                if (auto* n = find_obj(s.obj(), "name");
                    n && n->is_str()) r.step_name = n->str();
                r.uses_value = uses_v->str();
                r.status     = classify_pin(r.uses_value);
                out.push_back(std::move(r));
            }
            ++idx;
        }
    }
    return out;
}

// ===========================================================================
// 5. Pretty-print the report
// ===========================================================================

static const char* pin_status_marker(PinStatus s) {
    switch (s) {
        case PinStatus::Pinned:       return " OK ";
        case PinStatus::MajorVersion: return "WARN";
        case PinStatus::Branch:       return "WARN";
        case PinStatus::Unpinned:     return "FAIL";
        case PinStatus::Malformed:    return "FAIL";
    }
    return "????";
}

static void print_report(const std::vector<UsesRecord>& uses) {
    std::cout << "\n"
              << "============================================================\n"
              << " P-2026-08-18 — sha_pin_inspector\n"
              << "============================================================\n";
    if (uses.empty()) {
        std::cout << "(no `uses:` steps found in this workflow)\n";
        return;
    }
    std::cout << "  " << std::setw(4)  << "STEP"
              << "  " << std::setw(8)  << "STATUS"
              << "  " << std::setw(14) << "CLASS"
              << "  USES\n";
    std::cout << "  " << std::string(4, '-')
              << "  " << std::string(8, '-')
              << "  " << std::string(14, '-')
              << "  ----\n";
    for (const auto& r : uses) {
        std::cout << "  " << std::setw(4)  << (std::to_string(r.step_index) + ".")
                  << "  " << std::setw(8)  << pin_status_marker(r.status)
                  << "  " << std::setw(14) << pin_status_name(r.status)
                  << "  " << r.uses_value << "\n";
    }
}

// ===========================================================================
// 6. Self-check assertions
// ===========================================================================

struct AssertResult {
    std::vector<std::string> failures;
    bool ok() const { return failures.empty(); }
};

static AssertResult self_check(const std::vector<UsesRecord>& uses) {
    AssertResult a;
    auto expect = [&](bool cond, const std::string& msg) {
        if (!cond) a.failures.push_back(msg);
    };

    int n_pinned = 0, n_major = 0, n_branch = 0, n_unpinned = 0, n_malformed = 0;
    for (const auto& r : uses) {
        switch (r.status) {
            case PinStatus::Pinned:       ++n_pinned;    break;
            case PinStatus::MajorVersion: ++n_major;     break;
            case PinStatus::Branch:       ++n_branch;    break;
            case PinStatus::Unpinned:     ++n_unpinned;  break;
            case PinStatus::Malformed:    ++n_malformed; break;
        }
    }

    expect(!uses.empty(),
           "no `uses:` steps found — workflow has no actions at all");

    // The headline rule: at least one SHA-pinned action is required.
    // Without this, the lesson has nothing to verify. This is the
    // "the lesson's whole point" assertion.
    expect(n_pinned >= 1,
           "no SHA-pinned `uses:` lines — at least one is required "
           "to demonstrate the pin shape");

    // The all-pinned rule: a fully-pinned workflow has zero
    // floating, zero unpinned, zero malformed. This is the rule
    // release_sha_pinned.yml passes and release_floating.yml /
    // release_mixed_pin.yml fail.
    expect(n_major == 0,
           std::to_string(n_major) + " floating major-version tag(s) "
           "(e.g. @v4) — pin to a 40-hex SHA");
    expect(n_branch == 0,
           std::to_string(n_branch) + " floating branch ref(s) "
           "(e.g. @main) — pin to a 40-hex SHA");
    expect(n_unpinned == 0,
           std::to_string(n_unpinned) + " `uses:` line(s) with no "
           "@-reference at all");
    expect(n_malformed == 0,
           std::to_string(n_malformed) + " malformed `uses:` line(s) "
           "(wrong-shape SHA after @)");

    return a;
}

// ===========================================================================
// 7. Tiny in-program unit-test for is_40_hex_sha / classify_pin
// ===========================================================================
//
// These two functions are the entire classifier. They get a 5-case
// unit test inline (no external test framework). This is the same
// pattern the Jul 6 / Jul 8 lessons used for their inspector's
// internal helpers.

struct UnitTestResult {
    std::vector<std::pair<std::string, bool>> cases;
    bool ok() const {
        for (const auto& [_, passed] : cases) if (!passed) return false;
        return true;
    }
    int total()   const { return static_cast<int>(cases.size()); }
    int passed()  const {
        int n = 0;
        for (const auto& [_, p] : cases) if (p) ++n;
        return n;
    }
};

static UnitTestResult unit_test_classify_pin() {
    UnitTestResult u;
    auto check = [&](const std::string& uses, PinStatus want,
                     const std::string& label) {
        PinStatus got = classify_pin(uses);
        bool ok = (got == want);
        u.cases.emplace_back(label, ok);
        if (!ok) {
            std::cout << "  FAIL: " << label << " — classify_pin(\""
                      << uses << "\") = " << pin_status_name(got)
                      << ", want " << pin_status_name(want) << "\n";
        }
    };
    check("actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11",
          PinStatus::Pinned,
          "U1 actions/checkout@<40-hex-sha>");
    check("actions/checkout@v4",
          PinStatus::MajorVersion,
          "U2 actions/checkout@v4");
    check("actions/checkout@v2.1.0",
          PinStatus::MajorVersion,
          "U3 actions/checkout@v2.1.0 (major-version tag, still floating)");
    check("actions/checkout@main",
          PinStatus::Branch,
          "U4 actions/checkout@main");
    check("actions/checkout@abcdef1",
          PinStatus::Malformed,
          "U5 actions/checkout@abcdef1 (short hex, not 40 chars)");
    check("actions/checkout",
          PinStatus::Unpinned,
          "U6 actions/checkout (no @ at all)");
    // Edge: uppercase SHA from GitHub's UI — should be accepted
    // (is_40_hex_sha lowercases first).
    check("actions/checkout@B4FFDE65F46336AB88EB53BE808477A3936BAE11",
          PinStatus::Pinned,
          "U7 uppercase hex SHA from GitHub UI is accepted");
    // Edge: 39-char hex (one short) — Malformed.
    check("actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae1",
          PinStatus::Malformed,
          "U8 39-char hex is Malformed, not Pinned");
    // Edge: 41-char hex (one long) — Malformed.
    check("actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae110",
          PinStatus::Malformed,
          "U9 41-char hex is Malformed, not Pinned");
    // Edge: empty ref (trailing @) — Malformed.
    check("actions/checkout@",
          PinStatus::Malformed,
          "U10 empty ref after @ is Malformed");
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
        // Default to the GOOD fixture in this lesson's dir.
        fs::path here = fs::absolute(fs::path(argv[0])).parent_path();
        yml = here / "release_sha_pinned.yml";
    }
    if (!fs::exists(yml)) {
        std::cerr << "File not found: " << yml << "\n";
        return 1;
    }
    std::cout << "Workflow file: " << yml << "\n";

    // ─── In-program unit tests for the classifier ─────────────────
    std::cout << "\n============================================================\n"
              << " Unit tests (classifier)\n"
              << "============================================================\n";
    UnitTestResult ut = unit_test_classify_pin();
    std::cout << "  " << ut.passed() << " / " << ut.total() << " PASS\n";
    bool unit_ok = ut.ok();

    // ─── libyaml probe ────────────────────────────────────────────
    // The libyaml linked below is consumed through the helpers
    // above; this single API call keeps the linker honest (the
    // -lyaml flag isn't decorative) without bloating the lesson
    // with an unnecessary hand-rolled YAML reader.
    yaml_parser_t probe;
    yaml_parser_initialize(&probe);
    yaml_parser_delete(&probe);

    // ─── 1. YAML → JSON via PyYAML ────────────────────────────────
    std::string json_text;
    try {
        json_text = yaml_to_json_via_python(yml);
    } catch (const std::exception& e) {
        std::cerr << "YAML→JSON failed: " << e.what() << "\n";
        return 1;
    }

    // ─── 2. JSON → tree ───────────────────────────────────────────
    mjson::Parser jp{json_text};
    mjson::Value root;
    try {
        root = jp.parse();
    } catch (const std::exception& e) {
        std::cerr << "JSON parse failed: " << e.what() << "\n";
        return 1;
    }

    // ─── 3. Tree → uses records ───────────────────────────────────
    std::vector<UsesRecord> uses = extract_uses(root);

    // ─── 4. Print the report ──────────────────────────────────────
    print_report(uses);

    // ─── 5. Self-check ────────────────────────────────────────────
    AssertResult a = self_check(uses);
    std::cout << "\n============================================================\n"
              << " Self-check\n"
              << "============================================================\n";
    if (a.ok() && unit_ok) {
        std::cout << "PASS — every `uses:` is SHA-pinned, and the "
                  << "classifier's unit tests all pass.\n";
        return 0;
    }
    if (!unit_ok) {
        std::cout << "FAIL — classifier unit tests did not pass "
                  << "(see above).\n";
    }
    if (!a.ok()) {
        std::cout << "FAIL (" << a.failures.size() << " issue(s)):\n";
        for (const auto& f : a.failures) std::cout << "  - " << f << "\n";
    }
    return 1;
}
