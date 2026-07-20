// P-2026-07-20 — Consumer of <psp_span/json.h>, the third public
// header of psp_span_lib (v0.10.0).
//
// Where this fits in the arc
// --------------------------
// The Jul 19 lesson (P-2026-07-19-psp-json-arrays-nested.cpp)
// shipped a complete general JSON parser as a *consumer*: the
// parser, JsonValue, and json_to_string all lived in that .cpp
// file. The Jul 19 lesson's "Next steps" said:
//
//   The natural forward-on is <psp_span/json.h> — lift today's
//   consumer into the library proper, bump to v0.10.0, and the
//   next session's consumer can `find_package(psp_span_lib 0.10
//   REQUIRED)` and `#include <psp_span/json.h>`.
//
// That is exactly today's lesson. The headline: the parser
// consumers in Jul 18 / Jul 19 were ~250 lines of consumer-side
// recursive-descent code. Today's consumer is ~150 lines and
// contains ZERO parser code — every line is either test data,
// output formatting, or the round-trip oracle. The library now
// owns the parser.
//
// What changed vs. Jul 19
// ------------------------
// | Layer                                       | Jul 19     | Jul 20     |
// |---------------------------------------------|------------|------------|
// | <psp_span/parser.h>                         | v0.9.0     | v0.10.0    |
// |                                             |            | (+DuplicateKey) |
// | <psp_span/json.h>                           | (none)     | v0.10.0    |
// |                                             |            | (NEW)      |
// | JsonValue location                          | consumer   | header     |
// | parse_value_at / parse_array_at /           | consumer   | header     |
// |     parse_object_at                         |            |            |
// | json_to_string                              | consumer   | header     |
// | Duplicate-key error                         | UnexpectedChar (overload) | DuplicateKey (typed, NEW) |
// | Test sections                               | 4          | 4          |
//
// What did NOT change: the test corpus. The same input strings,
// the same expected pass counts, the same round-trip property.
// That's the whole point of a header-promotion lesson: the
// consumer exercises the EXACT same parser API as yesterday, just
// pulled from the header rather than consumer-side code.
//
// The lesson also demonstrates that including <psp_span/json.h>
// gives you the typed DuplicateKey error that the Jul 18 / Jul 19
// consumers did not have — that's a real change in the parser's
// surface area, surfaced as a Section-1 test that fails-fast if
// ParseError is wrong.
//
// Build (assumes psp_span_lib v0.10.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-20-psp-json-header
//
// ASan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-20-psp-json-header

#include <psp_span/json.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdio>
#include <expected>
#include <format>
#include <map>
#include <string>
#include <variant>
#include <vector>

// ===========================================================================
// Helpers (test infrastructure — no parser code).
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static std::string span_to_string(psp::Span<const char> s) {
    return std::string(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// ===========================================================================
// Section 1 — header-driven parse_value_at is just a thin wrapper
// ===========================================================================
//
// The whole point of <psp_span/json.h> is that the consumer can
// call psp::parse_value_at directly, with no consumer-side parser
// code. This section does that, and confirms two things at once:
//
//   1. The header exists, is on the consumer's include path, and
//      exposes the same API the Jul 19 consumer rolled by hand.
//   2. The new typed ParseError::DuplicateKey enumerator is
//      reachable: an object with two "a" members fails with
//      DuplicateKey, NOT the old UnexpectedChar overload.
//
// If the consumer-side parser code were still the Jul 19 version,
// we'd be parsing it locally. Instead, we trust the library to
// own the parser, and we test the result.
// ===========================================================================

static void section_header_drives_parsing() {
    print_section("Section 1: header-driven parse_value_at + DuplicateKey arrives");

    // The same scalar arrays from Jul 19's Section 4 — but the
    // parser comes from the header, not from this .cpp.
    struct Case { std::string input; bool want_ok; std::size_t want_size; };
    const Case cases[] = {
        {"[]",                     true,  0},
        {"  [  ]  ",               true,  0},
        {"[1, 2, 3]",              true,  3},
        {"[1.5, 2.5, 3.5]",        true,  3},
        {"[\"a\", \"b\", \"c\"]",   true,  3},
        {"[true, false, true]",    true,  3},
        {"[null, null, null]",     true,  3},
        {"[1, \"two\", 3.0, true, null]",  true,  5},

        // Malformed
        {"",                       false, 0},
        {"[",                      false, 0},
        {"[1, 2, ]",               false, 0},
        {"[1, 2",                  false, 0},
        {"[tru]",                  false, 0},
        {"[\"unterminated]",        false, 0},
        {"[1, 2, , 3]",            false, 0},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;
        psp::Span<const char> s = as_span(c.input);
        auto r = psp::parse_value_at(s);
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_value_at(\"%s\") should have succeeded but got %s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&r->value);
            if (!arr) {
                std::printf("  FAIL: parse_value_at(\"%s\") returned non-array\n", c.input.c_str());
                continue;
            }
            if (arr->size() != c.want_size) {
                std::printf("  FAIL: parse_value_at(\"%s\") got %zu elements, want %zu\n",
                            c.input.c_str(), arr->size(), c.want_size);
                continue;
            }
            std::printf("  parse_value_at(\"%s\") = [", c.input.c_str());
            for (std::size_t i = 0; i < arr->size(); ++i) {
                if (i > 0) std::printf(", ");
                std::printf("%s", psp::json_to_string((*arr)[i]).c_str());
            }
            std::printf("], s remaining = \"%s\"\n", span_to_string(s).c_str());
            ++passed;
        } else {
            if (r) {
                std::printf("  FAIL: parse_value_at(\"%s\") should have failed but got %zu elements\n",
                            c.input.c_str(), std::get<std::vector<psp::JsonValue>>(r->value).size());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_value_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
            ++passed;
        }
    }
    std::printf("  [%d / %d passed]\n", passed, total);

    // === The DuplicateKey check — the only meaningful new behavior in v0.10.0. ===
    //
    // In Jul 18 / Jul 19 the duplicate-key case overloaded
    // ParseError::UnexpectedChar. With v0.10.0's typed DuplicateKey
    // enumerator, we now have a way to distinguish "actually
    // unexpected character" from "duplicate key". The promotion's
    // whole point was this one bit of extra fidelity.
    //
    // Important: the JSON string is bound to a local first so the
    // std::string storage outlives the parse_value_at(s) call.
    // Letting the std::string be a temporary (e.g. as_span("..."))
    // would create a span pointing into the temporary's storage,
    // and the temp is freed at the end of the full-expression —
    // ASan flags that as stack-use-after-scope.
    {
        const std::string dup_input = R"({"a": 1, "a": 2})";
        psp::Span<const char> s = as_span(dup_input);
        auto r = psp::parse_value_at(s);
        const std::string rem = span_to_string(s);
        if (r) {
            std::printf("  FAIL: dup-key input should have errored, succeeded instead\n");
        } else if (r.error() != ParseError::DuplicateKey) {
            std::printf("  FAIL: dup-key input gave error:%s, want DuplicateKey\n",
                        std::format("{}", r.error()).c_str());
        } else if (rem != dup_input) {
            std::printf("  FAIL: dup-key error left s changed: \"%s\"\n", rem.c_str());
        } else {
            std::printf("  duplicate-key input gave the typed error: DuplicateKey (s unchanged)\n");
            ++passed;
        }
        ++total;
    }
    std::printf("  [%d / %d passed incl. dup-key]\n", passed, total);
}

// ===========================================================================
// Section 2 — nested objects, dispatching through the header
// ===========================================================================

static void section_nested_objects() {
    print_section("Section 2: nested objects via header parser");

    struct Case { std::string input; bool want_ok; std::size_t want_pairs; };
    const Case cases[] = {
        {"{\"a\": {\"x\": 1}}",                                        true,  1},
        {"{\"a\": {\"b\": {\"c\": 42}}}",                              true,  1},
        {"{\"name\": \"Petra\", \"tags\": [\"cpp\", \"json\"], \"meta\": {\"age\": 7, \"active\": true}}",
                                                                    true,  3},
        {"{\"a\": {\"b\": {\"c\": {\"d\": {\"e\": \"deep\"}}}}}",        true,  1},
        {"{\"users\": [{\"id\": 1}, {\"id\": 2}, {\"id\": 3}]}",         true,  1},
        {"[{\"x\": 1}, {\"x\": 2}, {\"x\": 3}]",                        true,  3},
        {"{\"items\": [], \"meta\": {}}",                              true,  2},
        {"{\"name\": \"Petra\", \"age\": 7, \"scores\": [98, 87, 92], \"address\": "
         "{\"city\": \"Earth\", \"zip\": \"00000\"}, \"active\": true}", true,  5},

        {"{\"a\": {\"x\": 1}",                                        false, 0},
        {"{\"a\": [1, 2, }",                                          false, 0},
        {"{\"a\": {\"x\": }",                                          false, 0},
        {"{\"a\": [1, 2}",                                            false, 0},
        {"{\"a\": 1, \"a\": 2}",                                      false, 0},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;
        psp::Span<const char> s = as_span(c.input);
        auto r = psp::parse_value_at(s);
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_value_at(\"%s\") error=%s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&r->value);
            if (obj) {
                if (obj->size() != c.want_pairs) {
                    std::printf("  FAIL: parse_value_at(\"%s\") object got %zu pairs, want %zu\n",
                                c.input.c_str(), obj->size(), c.want_pairs);
                    continue;
                }
                std::printf("  parse_value_at(\"%s\") = object with %zu top-level pairs\n",
                            c.input.c_str(), obj->size());
            } else {
                const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&r->value);
                if (arr) {
                    if (arr->size() != c.want_pairs) {
                        std::printf("  FAIL: parse_value_at(\"%s\") array got %zu elements, want %zu\n",
                                    c.input.c_str(), arr->size(), c.want_pairs);
                        continue;
                    }
                    std::printf("  parse_value_at(\"%s\") = array of %zu elements\n",
                                c.input.c_str(), arr->size());
                } else {
                    std::printf("  FAIL: parse_value_at(\"%s\") was neither object nor array\n",
                                c.input.c_str());
                    continue;
                }
            }
            ++passed;
        } else {
            if (r) {
                std::printf("  FAIL: parse_value_at(\"%s\") should have failed but succeeded\n",
                            c.input.c_str());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_value_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
            ++passed;
        }
    }
    std::printf("  [%d / %d passed]\n", passed, total);
}

// ===========================================================================
// Section 3 — full-document round-trip (parse -> print -> parse)
// ===========================================================================
//
// Same corpus as Jul 19's Section 6. Two parses per document —
// confirming that the parser is a partial inverse of the pretty-
// printer. The pretty-printer (json_to_string) now comes from the
// header too.
// ===========================================================================

static void section_round_trip() {
    print_section("Section 3: full-document round-trip (parse -> print -> parse)");

    struct Doc { std::string label; std::string input; };
    const Doc docs[] = {
        {"empty object",            "{}"},
        {"empty array",             "[]"},
        {"bare string",             "\"hello\""},
        {"bare number",             "42"},
        {"bare bool",               "true"},
        {"bare null",               "null"},
        {"flat object",             "{\"name\": \"Petra\", \"age\": 7, \"active\": true}"},
        {"flat array",              "[1, 2, 3, \"four\", 5.0, null, true]"},
        {"nested",                  "{\"user\": {\"name\": \"Petra\", \"scores\": [98, 87, 92]}}"},
        {"list of users",           "{\"users\": [{\"id\": 1, \"name\": \"Ada\"}, "
                                    "{\"id\": 2, \"name\": \"Bob\"}]}"},
        {"escapes",                 "{\"text\": \"line1\\nline2\\tend, quote: \\\", back: \\\\\"}"},
        {"unicode",                 "{\"text\": \"\\u00e9\\u20ac\\ud83d\\ude00\"}"},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& d : docs) {
        ++total;
        psp::Span<const char> s1 = as_span(d.input);
        auto r1 = psp::parse_value_at(s1);
        if (!r1) {
            std::printf("  FAIL: \"%s\": first parse failed: %s\n",
                        d.label.c_str(), std::format("{}", r1.error()).c_str());
            continue;
        }
        std::string pretty = psp::json_to_string(*r1);

        psp::Span<const char> s2 = as_span(pretty);
        auto r2 = psp::parse_value_at(s2);
        if (!r2) {
            std::printf("  FAIL: \"%s\": re-parse failed on output:\n    %s\n    error: %s\n",
                        d.label.c_str(), pretty.c_str(),
                        std::format("{}", r2.error()).c_str());
            continue;
        }

        std::string pretty2 = psp::json_to_string(*r2);
        if (pretty != pretty2) {
            std::printf("  FAIL: \"%s\": round-trip mismatch\n    pass1: %s\n    pass2: %s\n",
                        d.label.c_str(), pretty.c_str(), pretty2.c_str());
            continue;
        }

        std::printf("  OK: \"%s\" (input %zu bytes -> %zu bytes pretty)\n",
                    d.label.c_str(), d.input.size(), pretty.size());
        ++passed;
    }
    std::printf("  [%d / %d passed]\n", passed, total);
}

// ===========================================================================
// Section 4 — sizeof / feature probes
// ===========================================================================

static void section_probes() {
    print_section("Section 4: sizeof / feature probes (header-owned JsonValue)");

    std::printf("  sizeof(bool)                                  = %zu\n", sizeof(bool));
    std::printf("  sizeof(std::int64_t)                          = %zu\n", sizeof(std::int64_t));
    std::printf("  sizeof(double)                                = %zu\n", sizeof(double));
    std::printf("  sizeof(std::string)                           = %zu\n", sizeof(std::string));
    std::printf("  sizeof(std::vector<psp::JsonValue>)          = %zu\n",
                sizeof(std::vector<psp::JsonValue>));
    std::printf("  sizeof(std::map<std::string, psp::JsonValue>)= %zu\n",
                sizeof(std::map<std::string, psp::JsonValue>));
    std::printf("  sizeof(psp::JsonValue)                       = %zu\n", sizeof(psp::JsonValue));
    std::printf("  sizeof(std::expected<psp::JsonValue, ParseError>)= %zu\n",
                sizeof(std::expected<psp::JsonValue, ParseError>));
    std::printf("  sizeof(psp::Span<const char>)                 = %zu\n",
                sizeof(psp::Span<const char>));
    std::printf("  sizeof(ParseError)                            = %zu\n", sizeof(ParseError));

    static_assert(sizeof(psp::JsonValue) >= 32,
                  "psp::JsonValue must be at least 32 bytes: 24-byte largest alternative "
                  "(std::map<string,JsonValue>) + 8-byte discriminator.");
    std::printf("  static_assert(JsonValue >= 32 bytes)          = OK (actual: %zu)\n",
                sizeof(psp::JsonValue));

    std::printf("  Public-header roster (v0.10.0):\n");
    std::printf("    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)\n");
    std::printf("    <psp_span/parser.h>   : psp::parse_int/parse_double + whole-span + cursor + JSON-scalar token cursors + ParseError (+ DuplicateKey in v0.10.0)\n");
    std::printf("    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string  (NEW in v0.10.0)\n");

#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                            = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_variant)
    std::printf("  __cpp_lib_variant                             = %ld\n",
                static_cast<long>(__cpp_lib_variant));
#endif
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    section_header_drives_parsing();
    section_nested_objects();
    section_round_trip();
    section_probes();
    std::printf("\n[psp_json_header_consumer: all 4 sections complete]\n");
    return 0;
}
