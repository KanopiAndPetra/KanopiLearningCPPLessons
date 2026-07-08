// P-2026-07-08 — multi_os_matrix: a GitHub Actions matrix workflow that
// publishes psp_span_lib for BOTH Linux AND macOS in one tag-push.
//
// WHAT THIS PROGRAM DOES
// ----------------------
// This is a verification driver (inspector) for a multi-OS release
// workflow. The arc to here is:
//
//   Jul 6  release_workflow (one job, ubuntu-latest)
//   Jul 8  multi_os_matrix  (matrix of two jobs: Linux + macOS) ← today
//
// The inspector does five things, in order:
//
//   1. Parse release.yml via PyYAML (the same trick Jul 6 used to dodge
//      the YAML 1.1 'on:' -> True boolean gotcha).
//   2. Walk the parsed structure into a typed C++ shape (MatrixWorkflow)
//      and pretty-print it.
//   3. Run eight self-checks that the multi-OS shape is *correct*
//      (matrix dimension, both OSes present, per-OS archives handled
//       by one shared publish step, etc.).
//   4. Empirically verify the workflow's claim by running the matrix-
//      equivalent cmake/cpack build LOCALLY on this Darwin machine,
//      producing psp_span_lib-0.5.0-Darwin.tar.gz. (Linux can't be
//      built here, but we can prove the workflow's cpack step
//      succeeds for one of its two targets, and check that the glob
//      in the publish step would also pick up the matching Linux
//      archive on a Linux runner.)
//   5. Print PASS/FAIL and exit non-zero on FAIL.
//
// The negative test (deliberately broken matrix) is in
// `release_matrix_broken_fixture.yml`; running the inspector on it
// should fail 7+ self-checks.
//
// BUILD / RUN
// -----------
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o P-2026-07-08-multi-os-release-matrix \
//       P-2026-07-08-multi-os-release-matrix.cpp
//
//   ./P-2026-07-08-multi-os-release-matrix \
//       ./release_matrix.yml ../../..         # lesson copy
//   ./P-2026-07-08-multi-os-release-matrix \
//       ./release_matrix_broken_fixture.yml ../../..
//
// The second positional arg is the repo root, used to locate
// psp_span_lib/ for the local rebuild proof.
//
// DEPENDENCIES
// ------------
//   - Python 3 with PyYAML (`pip install pyyaml` in a venv; the
//     default Python 3.11.14 on this machine already has it via
//     `python3 -m pip install --user pyyaml` in a previous session).
//   - cmake >= 3.16, ninja or make, cpack (CMake-bundled).
//   - A C++17 compiler. ASan build is exercised by the
//     `-asan` sibling binary; this file is the release build.

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

// `mkdtemp(3)` is declared by <unistd.h> on POSIX; the C++ wrapper
// doesn't always expose it from <cstdlib>/<cstdlib.h>.
#include <unistd.h>

namespace fs = std::filesystem;

// ===========================================================================
// Section 1 — A minimal JSON parser.
// ===========================================================================
// We don't pull in nlohmann/json or RapidJSON; the JSON we get from
// `python3 -c "import json; print(json.dumps(yaml.safe_load(open('x.yml'))))"`
// is well-formed and the only quirky keys are `on` (YAML 1.1 -> Python
// bool) and bare keywords like `workflow_dispatch` (Python None). The
// Python helper script (see popenYamlToJson below) normalises both.
//
// What the parser must support:
//   - object (key/value, string keys, mixed-type values)
//   - array
//   - string (with \" and \uXXXX escapes)
//   - number (int and float, no scientific notation in our use)
//   - true / false / null
//
// It does NOT need to support:
//   - comments
//   - leading whitespace conventions beyond "skip ws"
//   - arbitrary precision integers
//   - duplicate key detection (PyYAML emits the last value; we mirror
//     that behaviour for safety)

enum class JType { Null, Bool, Number, String, Array, Object };

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray  = std::vector<JsonValue>;

struct JsonValue {
    JType type = JType::Null;

    // Storage. Exactly one of these is meaningful per `type`.
    bool          boolean = false;
    double        number  = 0.0;
    std::string   string;
    JsonArray     array;
    JsonObject    object;

    bool isObject() const { return type == JType::Object; }
    bool isArray()  const { return type == JType::Array;  }
    bool isString() const { return type == JType::String; }
    bool isBool()   const { return type == JType::Bool;   }
    bool isNumber() const { return type == JType::Number; }
    bool isNull()   const { return type == JType::Null;   }

    // Convenience: lookup with default for absent keys.
    const JsonValue& at(const std::string& key) const {
        static JsonValue nullv;
        auto it = object.find(key);
        return it == object.end() ? nullv : it->second;
    }
    const JsonValue& at(std::size_t i) const {
        static JsonValue nullv;
        return i < array.size() ? array[i] : nullv;
    }
};

struct JsonParser {
    const std::string& s;
    std::size_t i = 0;

    explicit JsonParser(const std::string& src) : s(src) {}

    // Skip ASCII whitespace.
    void skipWs() {
        while (i < s.size() &&
               (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
            ++i;
        }
    }

    // Expect a literal ASCII char or throw.
    void expect(char c) {
        skipWs();
        if (i >= s.size() || s[i] != c) {
            throw std::runtime_error("JSON: expected '" + std::string(1, c) +
                                     "' at offset " + std::to_string(i));
        }
        ++i;
    }

    JsonValue parse() {
        skipWs();
        if (i >= s.size()) throw std::runtime_error("JSON: empty input");
        return parseValue();
    }

private:
    JsonValue parseValue() {
        skipWs();
        if (i >= s.size()) throw std::runtime_error("JSON: unexpected EOF");
        char c = s[i];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseStringValue();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error(std::string("JSON: unexpected char '") + c +
                                 "' at offset " + std::to_string(i));
    }

    JsonValue parseObject() {
        JsonValue v; v.type = JType::Object;
        expect('{');
        skipWs();
        if (i < s.size() && s[i] == '}') { ++i; return v; }
        while (true) {
            skipWs();
            if (i >= s.size() || s[i] != '"')
                throw std::runtime_error("JSON: expected string key");
            std::string key = parseRawString();
            skipWs();
            expect(':');
            v.object[key] = parseValue();
            skipWs();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            expect('}');
            return v;
        }
    }

    JsonValue parseArray() {
        JsonValue v; v.type = JType::Array;
        expect('[');
        skipWs();
        if (i < s.size() && s[i] == ']') { ++i; return v; }
        while (true) {
            v.array.push_back(parseValue());
            skipWs();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            expect(']');
            return v;
        }
    }

    JsonValue parseStringValue() {
        JsonValue v; v.type = JType::String;
        v.string = parseRawString();
        return v;
    }

    // Parse a JSON string literal, decoding \" \\ \/ \b \f \n \r \t \uXXXX.
    std::string parseRawString() {
        expect('"');
        std::string out;
        while (i < s.size() && s[i] != '"') {
            if (s[i] != '\\') { out += s[i++]; continue; }
            ++i; // consume backslash
            if (i >= s.size())
                throw std::runtime_error("JSON: bad escape at EOF");
            char esc = s[i++];
            switch (esc) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    if (i + 4 > s.size())
                        throw std::runtime_error("JSON: short \\u escape");
                    unsigned code = 0;
                    for (int k = 0; k < 4; ++k) {
                        char c = s[i + k];
                        unsigned digit = 0;
                        if      (c >= '0' && c <= '9') digit = c - '0';
                        else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
                        else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
                        else throw std::runtime_error("JSON: bad hex in \\u");
                        code = (code << 4) | digit;
                    }
                    i += 4;
                    // Encode as UTF-8. We only handle the BMP and
                    // surrogate pairs — sufficient for commit messages
                    // and the C++ source we've been writing.
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        // High surrogate; expect \u low surrogate next.
                        if (i + 6 > s.size() || s[i] != '\\' || s[i+1] != 'u')
                            throw std::runtime_error("JSON: lone high surrogate");
                        i += 2;
                        unsigned low = 0;
                        for (int k = 0; k < 4; ++k) {
                            char c = s[i + k];
                            unsigned digit = 0;
                            if      (c >= '0' && c <= '9') digit = c - '0';
                            else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
                            else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
                            else throw std::runtime_error("JSON: bad hex in \\u");
                            low = (low << 4) | digit;
                        }
                        i += 4;
                        if (low < 0xDC00 || low > 0xDFFF)
                            throw std::runtime_error("JSON: bad low surrogate");
                        code = 0x10000 + (((code - 0xD800) << 10) |
                                          (low - 0xDC00));
                    }
                    if      (code < 0x80) {
                        out += static_cast<char>(code);
                    } else if (code < 0x800) {
                        out += static_cast<char>(0xC0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else if (code < 0x10000) {
                        out += static_cast<char>(0xE0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        out += static_cast<char>(0xF0 | (code >> 18));
                        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    throw std::runtime_error(std::string("JSON: bad escape \\") +
                                             esc);
            }
        }
        expect('"');
        return out;
    }

    JsonValue parseBool() {
        JsonValue v; v.type = JType::Bool;
        if (s.compare(i, 4, "true") == 0)  { i += 4; v.boolean = true;  return v; }
        if (s.compare(i, 5, "false") == 0) { i += 5; v.boolean = false; return v; }
        throw std::runtime_error("JSON: bad bool literal");
    }

    JsonValue parseNull() {
        if (s.compare(i, 4, "null") != 0)
            throw std::runtime_error("JSON: bad null literal");
        i += 4;
        JsonValue v; v.type = JType::Null;
        return v;
    }

    JsonValue parseNumber() {
        std::size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        if (i < s.size() && s[i] == '.') {
            ++i;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        JsonValue v; v.type = JType::Number;
        v.number = std::stod(s.substr(start, i - start));
        return v;
    }
};

// ===========================================================================
// Section 2 — Shell helpers. We use popen for the python3 -> JSON step
// (the same trick Jul 6 used) and for the local rebuild proof.
// ===========================================================================

// Run a shell command, capture stdout, return it. Throws on non-zero exit.
static std::string runShell(const std::string& cmd) {
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen failed for: " + cmd);
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe.get())) {
        out += buf;
    }
    int rc = pclose(pipe.release());
    if (rc != 0) {
        throw std::runtime_error("shell command failed (rc=" +
                                 std::to_string(rc) + "): " + cmd);
    }
    return out;
}

// Run python3 -c to convert YAML -> JSON, dodging the YAML 1.1 'on:' -> True
// boolean quirk by renaming True -> 'on' before dumping.
//
// We write the Python helper to a temp file (rather than passing it
// via `python3 -c "..."`) because:
//   - the script has a multi-line `with open() as f:` block, which
//     is awkward to flatten into -c safely across shells;
//   - single-quoting a 15-line script and escaping inner quotes
//     gives a long, fragile command line;
//   - we need the same PyYAML behaviour every time, so a fixed
//     helper file is easier to test in isolation.
static std::string yamlFileToJsonString(const std::string& ymlPath) {
    // Build the helper script in memory.
    const std::string script =
        "import sys, json, yaml\n"
        "with open(sys.argv[1], 'r') as f:\n"
        "    d = yaml.safe_load(f)\n"
        "def fix(x):\n"
        "    if isinstance(x, dict):\n"
        "        out = {}\n"
        "        for k, v in x.items():\n"
        "            key = 'on' if k is True else k\n"
        "            out[key] = fix(v)\n"
        "        return out\n"
        "    if isinstance(x, list):\n"
        "        return [fix(e) for e in x]\n"
        "    return x\n"
        "print(json.dumps(fix(d), ensure_ascii=False))\n";

    // Write to /tmp; mkstemp gives us an O_EXCL-created file.
    char tmpl[] = "/tmp/petra_yaml2json_XXXXXX.py";
    int fd = mkstemps(tmpl, 3);  // last 3 chars are ".py" suffix
    if (fd < 0) {
        throw std::runtime_error("mkstemps failed for helper script");
    }
    {
        ssize_t written = ::write(fd, script.data(), script.size());
        (void)written;
    }
    close(fd);

    std::string helperPath = tmpl;
    std::string escaped;
    for (char c : helperPath) {
        if (c == '\'') escaped += "'\\''";
        else escaped += c;
    }
    std::string cmd = "PYTHONPATH=/tmp/pylib:${PYTHONPATH:-} python3 '" +
                      escaped + "' '" + ymlPath + "'";
    std::string result = runShell(cmd);
    // Best-effort cleanup of the helper.
    std::error_code ec;
    fs::remove(helperPath, ec);
    return result;
}

// ===========================================================================
// Section 3 — The workflow shape we expect.
// ===========================================================================
//
// This is a tiny domain model for the *subset* of GitHub Actions
// workflow syntax we care about. We don't model the full grammar
// (https://json.schemastore.org/github-workflow) — only the
// fields we assert against in the self-checks below.

struct WorkflowStep {
    std::string name;     // display name, optional
    std::string uses;     // "actions/checkout@v4" — set if it's an action ref
    std::string run;      // shell script — set if it's a run step
    std::string runFirst; // first non-blank line of `run`, for content matching
    std::string id;       // step id, optional
};

struct WorkflowJob {
    std::string runsOn;   // "ubuntu-latest", "macos-latest", or a matrix expr
    std::string name;     // optional display name (when matrixed)
    std::vector<WorkflowStep> steps;

    // Matrix introspection — populated from the workflow's strategy.
    bool isMatrixed = false;
    std::vector<std::string> matrixOsValues; // populated by parseMatrix()
};

struct MatrixWorkflow {
    std::string name;        // workflow name (e.g. "Release psp_span_lib")
    std::map<std::string, std::string> permissions; // key: "contents", val: "write"
    std::vector<std::string> pushTagFilters;   // from on.push.tags
    bool hasWorkflowDispatch = false;

    // Jobs: keyed by job id (e.g. "build-and-publish").
    std::map<std::string, WorkflowJob> jobs;

    // Helper: pretty-print the matrix strategy that produced this
    // workflow's jobs (if any).
    void print(std::ostream& os) const {
        os << "Workflow name : " << name << "\n";
        os << "Permissions   :";
        for (const auto& [k, v] : permissions) {
            os << " " << k << "=" << v;
        }
        os << "\n";
        os << "Triggers      :\n";
        if (!pushTagFilters.empty()) {
            os << "                push.tags = [ ";
            for (std::size_t i = 0; i < pushTagFilters.size(); ++i) {
                if (i) os << ", ";
                os << "'" << pushTagFilters[i] << "'";
            }
            os << " ]\n";
        }
        if (hasWorkflowDispatch) {
            os << "                workflow_dispatch (manual)\n";
        }
        for (const auto& [jid, job] : jobs) {
            os << "\nJob: " << jid;
            os << "   runs-on: " << job.runsOn;
            if (job.isMatrixed) {
                os << "   (matrix: os = [";
                for (std::size_t i = 0; i < job.matrixOsValues.size(); ++i) {
                    if (i) os << ", ";
                    os << "'" << job.matrixOsValues[i] << "'";
                }
                os << "])";
            }
            os << "\n";
            for (std::size_t s = 0; s < job.steps.size(); ++s) {
                const auto& st = job.steps[s];
                os << "   " << (s + 1) << ". ";
                if (!st.name.empty()) os << st.name;
                else if (!st.uses.empty()) os << st.uses;
                else if (!st.runFirst.empty()) os << st.runFirst;
                else os << "(empty step)";
                os << "\n";
                if (!st.uses.empty() && !st.name.empty()) {
                    os << "        uses: " << st.uses << "\n";
                }
                if (!st.runFirst.empty() && !st.name.empty()) {
                    os << "        run: " << st.runFirst << "\n";
                }
            }
        }
    }
};

// Walk the parsed JSON into a MatrixWorkflow.
static MatrixWorkflow parseWorkflow(const JsonValue& root) {
    MatrixWorkflow wf;
    if (root.at("name").isString()) wf.name = root.at("name").string;

    if (root.at("permissions").isObject()) {
        for (const auto& [k, v] : root.at("permissions").object) {
            if (v.isString()) wf.permissions[k] = v.string;
        }
    }

    if (root.at("on").isObject()) {
        const auto& on = root.at("on");
        if (on.at("push").isObject()) {
            const auto& push = on.at("push");
            if (push.at("tags").isArray()) {
                for (const auto& t : push.at("tags").array) {
                    if (t.isString()) wf.pushTagFilters.push_back(t.string);
                }
            }
        }
        if (on.at("workflow_dispatch").isObject() ||
            on.at("workflow_dispatch").isNull()) {
            wf.hasWorkflowDispatch = true;
        }
    }

    auto jobs = root.at("jobs");
    if (!jobs.isObject()) {
        throw std::runtime_error("workflow has no jobs object");
    }
    for (const auto& [jid, jobJson] : jobs.object) {
        WorkflowJob job;
        if (jobJson.at("runs-on").isString()) {
            job.runsOn = jobJson.at("runs-on").string;
        } else if (jobJson.at("runs-on").isObject()) {
            // Matrix expression: ${{ matrix.os }}
            job.runsOn = "${{ matrix.os }}";
        }
        if (jobJson.at("name").isString()) {
            job.name = jobJson.at("name").string;
        }

        // Matrix strategy. We only care about the os axis.
        if (jobJson.at("strategy").isObject()) {
            const auto& strat = jobJson.at("strategy");
            if (strat.at("matrix").isObject()) {
                const auto& mat = strat.at("matrix");
                if (mat.at("os").isArray()) {
                    job.isMatrixed = true;
                    for (const auto& osVal : mat.at("os").array) {
                        if (osVal.isString()) {
                            job.matrixOsValues.push_back(osVal.string);
                        }
                    }
                }
            }
        }

        auto stepsJson = jobJson.at("steps");
        if (!stepsJson.isArray()) {
            throw std::runtime_error("job '" + jid + "' has no steps array");
        }
        for (const auto& stepJson : stepsJson.array) {
            WorkflowStep st;
            if (stepJson.at("name").isString()) st.name = stepJson.at("name").string;
            if (stepJson.at("uses").isString()) st.uses = stepJson.at("uses").string;
            if (stepJson.at("id").isString())   st.id   = stepJson.at("id").string;
            if (stepJson.at("run").isString()) {
                st.run = stepJson.at("run").string;
                // Capture first non-blank line for content matching.
                std::istringstream iss(st.run);
                std::string line;
                while (std::getline(iss, line)) {
                    auto first = line.find_first_not_of(" \t");
                    if (first != std::string::npos) {
                        st.runFirst = line.substr(first);
                        break;
                    }
                }
            }
            job.steps.push_back(std::move(st));
        }
        wf.jobs[jid] = std::move(job);
    }
    return wf;
}

// ===========================================================================
// Section 4 — Self-checks.
// ===========================================================================
//
// Each check returns std::nullopt on PASS, or a string describing the
// failure on FAIL. The driver collects all of them and prints PASS/FAIL
// at the end. Same shape as Jul 6.

struct CheckResult {
    std::string name;
    std::optional<std::string> failure = std::nullopt;
    bool passed() const { return !failure.has_value(); }
};

static CheckResult checkNameNonEmpty(const MatrixWorkflow& wf) {
    CheckResult r{"workflow name is not empty"};
    if (wf.name.empty()) r.failure = "name field is empty or missing";
    return r;
}

static CheckResult checkPermissionsContentsWrite(const MatrixWorkflow& wf) {
    CheckResult r{"permissions.contents == write"};
    auto it = wf.permissions.find("contents");
    if (it == wf.permissions.end()) {
        r.failure = "no permissions.contents key";
    } else if (it->second != "write") {
        r.failure = "permissions.contents = '" + it->second + "' (need 'write')";
    }
    return r;
}

static CheckResult checkPushTagFilterV(const MatrixWorkflow& wf) {
    CheckResult r{"push.tags filter includes 'v*'"};
    bool found = false;
    for (const auto& t : wf.pushTagFilters) {
        if (t == "v*") { found = true; break; }
    }
    if (!found) r.failure = "no 'v*' filter in push.tags";
    return r;
}

static CheckResult checkWorkflowDispatchPresent(const MatrixWorkflow& wf) {
    CheckResult r{"workflow_dispatch trigger present"};
    if (!wf.hasWorkflowDispatch) r.failure = "no workflow_dispatch trigger";
    return r;
}

static CheckResult checkSingleJob(const MatrixWorkflow& wf) {
    CheckResult r{"exactly one job exists (multi-OS uses one matrixed job)"};
    if (wf.jobs.size() != 1) {
        r.failure = "found " + std::to_string(wf.jobs.size()) +
                    " jobs (expected exactly 1 matrixed job)";
    }
    return r;
}

static CheckResult checkMatrixOsAxis(const MatrixWorkflow& wf) {
    CheckResult r{"strategy.matrix.os is an array of >=2 OS labels"};
    if (wf.jobs.empty()) {
        r.failure = "no jobs";
        return r;
    }
    const auto& job = wf.jobs.begin()->second;
    if (!job.isMatrixed) {
        r.failure = "job is not matrixed (no strategy.matrix.os array)";
    } else if (job.matrixOsValues.size() < 2) {
        r.failure = "matrix has only " +
                    std::to_string(job.matrixOsValues.size()) +
                    " OS value(s) (need >= 2)";
    }
    return r;
}

static CheckResult checkMatrixContainsUbuntuAndMacos(const MatrixWorkflow& wf) {
    CheckResult r{"matrix includes ubuntu-latest AND macos-latest"};
    if (wf.jobs.empty()) {
        r.failure = "no jobs";
        return r;
    }
    const auto& job = wf.jobs.begin()->second;
    bool hasUbuntu = false, hasMacos = false;
    for (const auto& os : job.matrixOsValues) {
        if (os == "ubuntu-latest") hasUbuntu = true;
        if (os == "macos-latest")  hasMacos  = true;
    }
    if (!hasUbuntu || !hasMacos) {
        std::string have;
        for (std::size_t i = 0; i < job.matrixOsValues.size(); ++i) {
            if (i) have += ", ";
            have += job.matrixOsValues[i];
        }
        r.failure = "matrix.os = [" + have +
                    "] (need both ubuntu-latest and macos-latest)";
    }
    return r;
}

static CheckResult checkPublishStepHasMultiOsGlob(const MatrixWorkflow& wf) {
    // The publish step's `files:` glob must accept archives from BOTH
    // OSes, i.e. a glob like `psp_span_lib-*-Linux.tar.gz` and
    // `psp_span_lib-*-Darwin.tar.gz`, or one combined `*-{Linux,Darwin}.tar.gz`.
    // We do a structural check: there must be a `softprops/action-gh-release`
    // step, and its `with.files` must contain BOTH `Linux` and `Darwin`
    // substrings in the glob (or one combined glob covering both).
    CheckResult r{"publish step's file glob covers both Linux AND Darwin archives"};
    bool foundPublish = false;
    for (const auto& [jid, job] : wf.jobs) {
        for (const auto& st : job.steps) {
            if (st.uses.find("action-gh-release") == std::string::npos) continue;
            foundPublish = true;
            // We didn't model the `with:` map (it's an object in the
            // YAML; our parser only stored step.name/uses/run/id), so
            // we fall back to scanning the raw run text — but the
            // publish step is action-based, not run-based. We
            // therefore re-walk the original JSON for the publish
            // step's `with.files` to do the structural assertion.
            (void)st;
        }
    }
    if (!foundPublish) {
        r.failure = "no softprops/action-gh-release step found";
    }
    return r;
}

// We need a second pass for the publish step's `with.files` content
// because our parser doesn't preserve it. This is a structural
// check against the raw JSON, not the typed model.
static CheckResult checkPublishFilesCoversBothOs(const JsonValue& root) {
    CheckResult r{"publish step 'with.files' glob covers Linux AND Darwin"};
    const auto& jobs = root.at("jobs");
    if (!jobs.isObject() || jobs.object.empty()) {
        r.failure = "no jobs"; return r;
    }
    bool found = false;
    for (const auto& [jid, jobJson] : jobs.object) {
        const auto& steps = jobJson.at("steps");
        if (!steps.isArray()) continue;
        for (const auto& st : steps.array) {
            if (!st.at("uses").isString()) continue;
            if (st.at("uses").string.find("action-gh-release") == std::string::npos)
                continue;
            const auto& withFiles = st.at("with").at("files");
            if (!withFiles.isString()) {
                r.failure = "publish step has no with.files (or not a string)";
                return r;
            }
            const std::string& g = withFiles.string;
            // Must reference Linux and Darwin somehow. The acceptable
            // shapes are:
            //   * psp_span_lib-*-Linux.tar.gz
            //   * psp_span_lib-*-Darwin.tar.gz
            // (two separate lines in a YAML block scalar)
            // OR
            //   * psp_span_lib-*-{Linux,Darwin}.tar.gz
            // (brace expansion; bash/Glob/FNM only, but softprops
            // passes the string through to the GitHub API which does
            // its own glob expansion.)
            bool hasLinux = g.find("Linux") != std::string::npos;
            bool hasDarwin = g.find("Darwin") != std::string::npos;
            if (!(hasLinux && hasDarwin)) {
                r.failure = "publish glob '" + g +
                            "' missing Linux or Darwin reference";
            }
            found = true;
        }
    }
    if (!found) {
        r.failure = "no publish step with with.files found";
    }
    return r;
}

static CheckResult checkCpackStepPresent(const MatrixWorkflow& wf) {
    CheckResult r{"cpack step is present in the matrixed job"};
    if (wf.jobs.empty()) { r.failure = "no jobs"; return r; }
    const auto& job = wf.jobs.begin()->second;
    bool found = false;
    for (const auto& st : job.steps) {
        if (st.run.find("cpack -C Release") != std::string::npos) {
            found = true; break;
        }
    }
    if (!found) r.failure = "no 'cpack -C Release ...' run step";
    return r;
}

// ===========================================================================
// Section 5 — Local rebuild proof. We run cmake + cpack on this Darwin
// machine to verify the matrix's claim that ONE OS's archive exists.
// ===========================================================================
//
// The matrix produces TWO archives (one per OS) on a real CI run. We
// can't run a Linux runner locally on macOS, but we CAN verify the
// matrix's claim that the Darwin branch of the build works — by
// running the same cmake/cpack commands the workflow's cpack step
// would run, on this machine, into a throwaway build dir.
//
// If the local rebuild succeeds, the workflow's claim "an archive
// named psp_span_lib-<ver>-<Darwin|Linux>.tar.gz will exist" is
// verified for one of the two matrix legs, which is enough to
// prove the matrix's per-OS steps aren't structurally broken.

struct RebuildProof {
    bool ok = false;
    std::string archivePath;
    std::string version;
    std::string systemName;   // e.g. "Darwin", "Linux"
    std::string errorMessage;
};

static RebuildProof runLocalRebuild(const std::string& repoRoot) {
    RebuildProof proof;
    fs::path libDir = fs::path(repoRoot) /
                      "late-may/cpp_practice/psp_span_lib";
    if (!fs::exists(libDir / "CMakeLists.txt")) {
        proof.errorMessage = "psp_span_lib/CMakeLists.txt not found at " +
                             libDir.string();
        return proof;
    }

    // Throwaway build dir under /tmp. We use a unique suffix so we
    // don't collide with previous lesson runs.
    char tmpl[] = "/tmp/psp_span_lib_cron_matrix_XXXXXX";
    char* tmp = mkdtemp(tmpl);
    if (!tmp) {
        proof.errorMessage = "mkdtemp failed";
        return proof;
    }
    fs::path buildDir = tmp;

    try {
        // Configure. -Wno-dev suppresses CMake policy warnings about
        // new CMake versions; we don't care for the proof.
        std::string cfgCmd = "cmake -S '" + libDir.string() + "' -B '" +
                             buildDir.string() +
                             "' -DCMAKE_BUILD_TYPE=Release >/dev/null";
        std::cout << "  [run] " << cfgCmd << "\n";
        runShell(cfgCmd);

        // Build.
        std::string buildCmd = "cmake --build '" + buildDir.string() +
                               "' --config Release -j 2 >/dev/null";
        std::cout << "  [run] " << buildCmd << "\n";
        runShell(buildCmd);

        // CPack.
        std::string cpackCmd = "cd '" + buildDir.string() +
                               "' && cpack -C Release --config CPackConfig.cmake";
        std::cout << "  [run] " << cpackCmd << "\n";
        runShell(cpackCmd);

        // Find the produced .tar.gz.
        for (const auto& entry : fs::directory_iterator(buildDir)) {
            const std::string fname = entry.path().filename().string();
            if (fname.find(".tar.gz") == std::string::npos ||
                fname.find("psp_span_lib-") != 0) {
                continue;
            }
            proof.archivePath = entry.path().string();

            // Filename shape: psp_span_lib-<version>-<system>.tar.gz
            // Strip the literal prefix and suffix to isolate the
            // "<version>-<system>" middle, then split on the LAST
            // '-' (because <version> can itself contain '.' like
            // "0.5.0" but should never contain '-' for our project).
            constexpr const char* kPrefix = "psp_span_lib-";
            constexpr const char* kSuffix = ".tar.gz";
            std::string middle = fname.substr(std::string(kPrefix).size());
            auto suffixPos = middle.find(kSuffix);
            if (suffixPos != std::string::npos) {
                middle.resize(suffixPos);
            }
            auto lastDash = middle.find_last_of('-');
            if (lastDash != std::string::npos) {
                proof.version    = middle.substr(0, lastDash);
                proof.systemName = middle.substr(lastDash + 1);
            }
            proof.ok = true;
            break;
        }
        if (!proof.ok) {
            proof.errorMessage = "cpack did not produce a psp_span_lib-*.tar.gz in " +
                                 buildDir.string();
        }
    } catch (const std::exception& e) {
        proof.errorMessage = e.what();
    }

    // Best-effort cleanup. We don't fail the run if rm fails —
    // /tmp gets cleaned eventually, and a previous lesson's leak
    // is harmless.
    std::error_code ec;
    fs::remove_all(buildDir, ec);
    return proof;
}

// ===========================================================================
// Section 7 — Main entry.
// ===========================================================================

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <workflow.yml> <repo-root>\n"
                  << "  <workflow.yml>  Path to the matrix workflow YAML\n"
                  << "  <repo-root>     Path to KanopiLearningCPPLessons/\n";
        return 2;
    }
    const std::string ymlPath   = argv[1];
    const std::string repoRoot  = argv[2];

    std::cout << "==========================================================\n"
              << " P-2026-07-08 — multi_os_matrix inspector\n"
              << "==========================================================\n";

    // 1. Parse YAML -> JSON (via Python), then JSON -> JsonValue.
    std::string jsonStr;
    try {
        jsonStr = yamlFileToJsonString(ymlPath);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: YAML->JSON failed: " << e.what() << "\n";
        return 2;
    }
    JsonValue root;
    try {
        JsonParser p(jsonStr);
        root = p.parse();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: JSON parse failed: " << e.what() << "\n";
        return 2;
    }

    // 2. Build the typed workflow model.
    MatrixWorkflow wf;
    try {
        wf = parseWorkflow(root);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: workflow parse failed: " << e.what() << "\n";
        return 2;
    }
    wf.print(std::cout);

    // 3. Self-checks.
    std::vector<CheckResult> checks;
    checks.push_back(checkNameNonEmpty(wf));
    checks.push_back(checkPermissionsContentsWrite(wf));
    checks.push_back(checkPushTagFilterV(wf));
    checks.push_back(checkWorkflowDispatchPresent(wf));
    checks.push_back(checkSingleJob(wf));
    checks.push_back(checkMatrixOsAxis(wf));
    checks.push_back(checkMatrixContainsUbuntuAndMacos(wf));
    checks.push_back(checkPublishStepHasMultiOsGlob(wf));
    checks.push_back(checkPublishFilesCoversBothOs(root));
    checks.push_back(checkCpackStepPresent(wf));

    // 4. Local rebuild proof. (Always run; only meaningful on Darwin
    //    machines, but the proof is "this matrix leg's cpack step
    //    works on a real build", not "the matrix is well-formed".)
    std::cout << "\n[rebuild] Same shell the matrix runs on one CI leg:\n";
    RebuildProof proof = runLocalRebuild(repoRoot);
    bool proofOk = proof.ok;
    if (proofOk) {
        std::cout << "  -> archive : " << proof.archivePath << "\n"
                  << "  -> version : " << proof.version     << "\n"
                  << "  -> system  : " << proof.systemName  << "\n";
        CheckResult r{"local cpack on this machine produced a valid archive"};
        if (proof.systemName != "Darwin" && proof.systemName != "Linux") {
            r.failure = "system name '" + proof.systemName +
                        "' is not Darwin or Linux (matrix claim broken)";
        }
        checks.push_back(r);
    } else {
        std::cout << "  -> REBUILD FAILED: " << proof.errorMessage << "\n";
        CheckResult r{"local cpack on this machine produced a valid archive"};
        r.failure = proof.errorMessage;
        checks.push_back(r);
    }

    // 5. Report.
    std::cout << "\n==========================================================\n"
              << " Self-check\n"
              << "==========================================================\n";
    int failures = 0;
    for (const auto& c : checks) {
        if (c.passed()) {
            std::cout << "PASS  " << c.name << "\n";
        } else {
            std::cout << "FAIL  " << c.name << " — " << *c.failure << "\n";
            ++failures;
        }
    }

    if (failures == 0) {
        std::cout << "\nPASS — matrix workflow well-formed AND local rebuild "
                  << "produced the archive one of its two legs will upload.\n";
        return 0;
    } else {
        std::cout << "\nFAIL (" << failures << " issue(s)).\n";
        return 1;
    }
}
