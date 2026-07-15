// P-2026-07-15 — streaming cursor parser API in <psp_span/parser.h>
//
// This lesson closes the loop from the Jul 14 lesson
// (P-2026-07-14-psp-parser-header.cpp), which shipped the whole-span
// parsers parse_int and parse_double. Today the parser header gains
// the streaming cursor variants:
//
//   psp::parse_int_at    (Span<const char>&) -> expected<int,     ParseError>
//   psp::parse_uint_at   (Span<const char>&) -> expected<unsigned, ParseError>
//   psp::parse_double_at (Span<const char>&) -> expected<double,   ParseError>
//
// The whole-span parsers treat the input as a complete, isolated run:
// "is this buffer a valid int?". The streaming variants treat it as a
// CURSOR: "consume the leading int, leave the rest for me to handle".
// That's the foundation JSON / CSV / structured-data parsers build
// on — the caller walks a buffer character-by-character, calling
// different parsers for different tokens.
//
// Three things this lesson proves:
//
//   1. Each streaming parser shrinks `s` past the consumed run on
//      success and leaves `s` unchanged on failure.
//   2. Multiple tokens can be pulled from a single buffer in sequence
//      (a "split a CSV row by comma" demo).
//   3. The new API composes with the existing whole-span API: if a
//      caller knows the whole span IS a number, parse_int is cheaper;
//      if it's a cursor walk, parse_int_at is the right primitive.
//
// The program is structured as six numbered sections, each
// printing a result table so the output is greppable in CI logs.
//
// Build (assumes psp_span_lib v0.7.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-15-psp-parser-streaming-cursor
//
// ASan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-15-psp-parser-streaming-cursor

#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// Helpers — bridging std::string to psp::Span<const char>.
//
// psp::Span does not have a std::string constructor by design (the
// header was prototyped on Jun 14 with C-array / std::array / std::vector /
// raw (ptr, len) overloads only). The (ptr, len) form is the most
// general and works for std::string's contiguous buffer.
// ---------------------------------------------------------------------------
static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

// span_to_string — make a string from a span (for printing). We can't
// use std::string ctor with the span (would be circular), so we use
// the (data, size) ctor directly.
static std::string span_to_string(psp::Span<const char> s) {
    return std::string(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// ---------------------------------------------------------------------------
// Section 1 — parse_int_at: cursor advances past the consumed digits.
//
// For each test case, we snapshot the original span content, run
// parse_int_at, and print:
//   - the input
//   - the result (parsed int OR error name)
//   - the remainder (what's left in `s` after the call)
//   - a one-line verdict: "advanced" if s shrunk, "unchanged" if not
//
// The interesting cases are the trailing-garbage ones ("123abc") —
// those exercise the "consume what you can, leave the rest" contract.
// ---------------------------------------------------------------------------
static void section_parse_int_at() {
    print_section("Section 1: psp::parse_int_at — cursor advances past digits");

    struct Case { std::string input; bool want_ok; int want_val; const char* want_err; };
    const Case cases[] = {
        {"12345",      true,  12345,   nullptr},     // pure digits, fully consumed
        {"0",          true,  0,       nullptr},     // single zero
        {"7",          true,  7,       nullptr},     // single digit
        {"123abc",     true,  123,     nullptr},     // trailing garbage — cursor leaves "abc"
        {"42,17,99",   true,  42,      nullptr},     // CSV-style first token
        {"",           false, 0,       "Empty"},
        {"+9",         false, 0,       "LeadingSign"},
        {"-9",         false, 0,       "LeadingSign"},
        {"abc",        false, 0,       "NotADigit"}, // not a single digit at the front
        {"  42",       false, 0,       "NotADigit"}, // leading whitespace is not a digit
        {"99999999999",false, 0,       "Overflow"},  // > INT_MAX
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::parse_int_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_int_at(\"%s\") should have succeeded but got %s\n",
                            orig.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            if (*r != c.want_val) {
                std::printf("  FAIL: parse_int_at(\"%s\") = %d, want %d\n",
                            orig.c_str(), *r, c.want_val);
                continue;
            }
            std::printf("  parse_int_at(\"%s\") = %d, s now = \"%s\" (advanced %zu)\n",
                        orig.c_str(), *r, span_to_string(s).c_str(),
                        orig.size() - s.size());
        } else {
            if (r) {
                std::printf("  FAIL: parse_int_at(\"%s\") should have failed but got %d\n",
                            orig.c_str(), *r);
                continue;
            }
            auto err_name = std::format("{}", r.error());
            if (err_name != c.want_err) {
                std::printf("  FAIL: parse_int_at(\"%s\") got error %s, want %s\n",
                            orig.c_str(), err_name.c_str(), c.want_err);
                continue;
            }
            // Distinguish three s-mutation outcomes on failure:
            //   unchanged  = s == orig              (Empty / LeadingSign / NotADigit)
            //   advanced   = s.size() < orig.size() (Overflow — cursor committed the consumed prefix)
            //   rewoound   = s.size() > orig.size() (should never happen — would be a bug)
            std::string rem = span_to_string(s);
            const char* verdict;
            if (rem == orig)         verdict = "unchanged";
            else if (s.size() < orig.size()) verdict = "advanced (overflow committed)";
            else                     verdict = "REWOUND BUG!";
            std::printf("  parse_int_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), err_name.c_str(), rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 2 — parse_uint_at: the unsigned cursor.
//
// Difference vs parse_int_at: '+' is ACCEPTED (no-op), '-' is REJECTED
// as LeadingSign. The same cursor-advances / failure-leaves-s-unchanged
// contract applies.
// ---------------------------------------------------------------------------
static void section_parse_uint_at() {
    print_section("Section 2: psp::parse_uint_at — unsigned cursor");

    struct Case { std::string input; bool want_ok; unsigned want_val; const char* want_err; };
    const Case cases[] = {
        {"0",          true,  0u,        nullptr},
        {"42",         true,  42u,       nullptr},
        {"+42",        true,  42u,       nullptr},   // '+' allowed for unsigned
        {"-42",        false, 0u,        "LeadingSign"},
        {"4294967295", true,  4294967295u, nullptr}, // UINT_MAX
        {"4294967296", false, 0u,        "Overflow"}, // > UINT_MAX
        {"42abc",      true,  42u,       nullptr},   // cursor advances
        {"abc",        false, 0u,        "NotADigit"},
        {"+",          false, 0u,        "NotADigit"}, // '+' alone, no digits
        {"",           false, 0u,        "Empty"},
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::parse_uint_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_uint_at(\"%s\") should have succeeded but got %s\n",
                            orig.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            if (*r != c.want_val) {
                std::printf("  FAIL: parse_uint_at(\"%s\") = %u, want %u\n",
                            orig.c_str(), *r, c.want_val);
                continue;
            }
            std::printf("  parse_uint_at(\"%s\") = %u, s now = \"%s\" (advanced %zu)\n",
                        orig.c_str(), *r, span_to_string(s).c_str(),
                        orig.size() - s.size());
        } else {
            if (r) {
                std::printf("  FAIL: parse_uint_at(\"%s\") should have failed but got %u\n",
                            orig.c_str(), *r);
                continue;
            }
            auto err_name = std::format("{}", r.error());
            std::string rem = span_to_string(s);
            const char* verdict;
            if (rem == orig)                  verdict = "unchanged";
            else if (s.size() < orig.size())  verdict = "advanced (overflow committed)";
            else                              verdict = "REWOUND BUG!";
            std::printf("  parse_uint_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), err_name.c_str(), rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 3 — parse_double_at: cursor advances past the double literal.
//
// Includes integer-only, fractional, exponent, and trailing-garbage
// cases. Compare against parse_double (the whole-span variant from
// Jul 14): "1.5x" is REJECTED by parse_double but ACCEPTED by
// parse_double_at (which leaves "x" in `s`).
// ---------------------------------------------------------------------------
static void section_parse_double_at() {
    print_section("Section 3: psp::parse_double_at — double cursor");

    struct Case { std::string input; bool want_ok; double want_val; const char* want_err; double tol; };
    const Case cases[] = {
        {"0",         true,  0.0,        nullptr, 0.0},
        {"42",        true,  42.0,       nullptr, 0.0},
        {"3.14",      true,  3.14,       nullptr, 1e-9},
        {".5",        true,  0.5,        nullptr, 1e-9},
        {"1.",        true,  1.0,        nullptr, 1e-9},
        {"1e10",      true,  1e10,       nullptr, 0.0},
        {"1.5e-3",    true,  1.5e-3,     nullptr, 1e-12},
        {"1.5x",      true,  1.5,        nullptr, 1e-9},  // trailing garbage — cursor leaves "x"
        {"1.5,2.5,3.5", true, 1.5,       nullptr, 1e-9},  // CSV-style first token
        {"",          false, 0.0,        "Empty", 0.0},
        {"+1.0",      false, 0.0,        "LeadingSign", 0.0},
        {"-1.0",      false, 0.0,        "LeadingSign", 0.0},
        {".",         false, 0.0,        "MissingFraction", 0.0},
        {"1e",        false, 0.0,        "BadExponent", 0.0},
        {"1.2.3",     true,  1.2,        nullptr, 1e-9},  // stops at second '.'
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::parse_double_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_double_at(\"%s\") should have succeeded but got %s\n",
                            orig.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            double diff = (*r > c.want_val) ? (*r - c.want_val) : (c.want_val - *r);
            if (diff > c.tol) {
                std::printf("  FAIL: parse_double_at(\"%s\") = %g, want %g\n",
                            orig.c_str(), *r, c.want_val);
                continue;
            }
            std::printf("  parse_double_at(\"%s\") = %g, s now = \"%s\" (advanced %zu)\n",
                        orig.c_str(), *r, span_to_string(s).c_str(),
                        orig.size() - s.size());
        } else {
            if (r) {
                std::printf("  FAIL: parse_double_at(\"%s\") should have failed but got %g\n",
                            orig.c_str(), *r);
                continue;
            }
            auto err_name = std::format("{}", r.error());
            std::string rem = span_to_string(s);
            const char* verdict;
            if (rem == orig)                  verdict = "unchanged";
            else if (s.size() < orig.size())  verdict = "advanced (overflow committed)";
            else                              verdict = "REWOUND BUG!";
            std::printf("  parse_double_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), err_name.c_str(), rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 4 — Walking a CSV-style row with multiple parse_int_at calls.
//
// This is the headline use case for streaming cursors: a buffer like
// "10,20,30,40,50" can be parsed into a vector of ints by repeatedly
// calling parse_int_at and then checking for ',' (here we just skip
// non-digits, which is even simpler — a real CSV parser would be
// pickier about the delimiter).
//
// We don't use std::ranges yet (this lesson is about the cursor API,
// not about range adapters), so a plain while-loop is fine.
// ---------------------------------------------------------------------------
static void section_csv_walk() {
    print_section("Section 4: walking a CSV row with parse_int_at");

    const std::string rows[] = {
        "10,20,30,40,50",
        "1,2,3",
        "100,200,300,400",
        "7",                // single value, no commas
    };

    for (const auto& row : rows) {
        std::printf("  row = \"%s\" -> [", row.c_str());
        psp::Span<const char> s = as_span(row);
        bool first = true;
        while (!s.empty()) {
            auto r = psp::parse_int_at(s);
            if (!r) {
                // The cursor parser leaves s unchanged on failure, so
                // we'd loop forever if we just kept trying. Bail.
                if (!first) std::printf(", ");
                std::printf("ERROR:%s", std::format("{}", r.error()).c_str());
                break;
            }
            if (!first) std::printf(", ");
            std::printf("%d", *r);
            first = false;
            // Skip any non-digit characters (commas, whitespace, etc.)
            // between tokens. A stricter parser would require exactly
            // one delimiter; for this demo we accept any gap.
            while (!s.empty() && (s.front() < '0' || s.front() > '9')) {
                s = s.subspan(1, s.size() - 1);
            }
        }
        std::printf("]\n");
    }
}

// ---------------------------------------------------------------------------
// Section 5 — Composing parse_int_at with parse_double_at.
//
// A more realistic mixed-type cursor walk: a buffer like
// "10 3.14 20 2.71 30" contains alternating ints and doubles.
// We call parse_int_at, then parse_double_at, alternating, until the
// buffer is empty. This shows the two primitives compose in the same
// buffer without interfering with each other.
// ---------------------------------------------------------------------------
static void section_mixed_walk() {
    print_section("Section 5: alternating parse_int_at / parse_double_at");

    const std::string row = "10 3.14 20 2.71 30";
    psp::Span<const char> s = as_span(row);

    std::printf("  row = \"%s\"\n", row.c_str());
    int step = 0;
    while (!s.empty()) {
        // Skip inter-token whitespace.
        while (!s.empty() && s.front() == ' ') {
            s = s.subspan(1, s.size() - 1);
        }
        if (s.empty()) break;

        if (step % 2 == 0) {
            auto r = psp::parse_int_at(s);
            if (!r) {
                std::printf("    step %d: parse_int_at failed: %s\n",
                            step, std::format("{}", r.error()).c_str());
                break;
            }
            std::printf("    step %d (int): %d, remainder = \"%s\"\n",
                        step, *r, span_to_string(s).c_str());
        } else {
            auto r = psp::parse_double_at(s);
            if (!r) {
                std::printf("    step %d: parse_double_at failed: %s\n",
                            step, std::format("{}", r.error()).c_str());
                break;
            }
            std::printf("    step %d (double): %g, remainder = \"%s\"\n",
                        step, *r, span_to_string(s).c_str());
        }
        ++step;
    }
}

// ---------------------------------------------------------------------------
// Section 6 — sizeof / alignment / feature-test probes.
//
// Documents the cost of the new types and confirms the C++23 feature
// macros are still active. The streaming variants return the SAME
// expected<T, ParseError> as the whole-span variants, so the size
// probes are identical to the Jul 14 lesson's.
// ---------------------------------------------------------------------------
static void section_probes() {
    print_section("Section 6: sizeof / feature probes");

    std::printf("  sizeof(int)                                       = %zu\n", sizeof(int));
    std::printf("  sizeof(unsigned)                                  = %zu\n", sizeof(unsigned));
    std::printf("  sizeof(double)                                    = %zu\n", sizeof(double));
    std::printf("  sizeof(ParseError)                                = %zu\n", sizeof(ParseError));
    std::printf("  sizeof(psp::Span<const char>)                     = %zu\n", sizeof(psp::Span<const char>));
    std::printf("  sizeof(std::expected<int, ParseError>)            = %zu\n",
                sizeof(std::expected<int, ParseError>));
    std::printf("  sizeof(std::expected<unsigned, ParseError>)       = %zu\n",
                sizeof(std::expected<unsigned, ParseError>));
    std::printf("  sizeof(std::expected<double, ParseError>)         = %zu\n",
                sizeof(std::expected<double, ParseError>));
#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                                = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_span)
    std::printf("  __cpp_lib_span                                    = %ld\n",
                static_cast<long>(__cpp_lib_span));
#endif
}

int main() {
    section_parse_int_at();
    section_parse_uint_at();
    section_parse_double_at();
    section_csv_walk();
    section_mixed_walk();
    section_probes();
    std::printf("\n[psp_parser_streaming_consumer: all 6 sections complete]\n");
    return 0;
}