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
//
// v0.14.0 update (P-2026-07-31): parse_int_at now accepts an
// optional leading '+' or '-' (was LeadingSign in v0.13.0) and its
// return type widened from `int` to `std::int64_t`; parse_double_at
// likewise accepts an optional leading sign. parse_uint_at is
// unchanged in API. Stale `+9`/`-9`/`+1.0`/`-1.0` failure cases
// now SUCCEED; the past-INT_MAX `99999999999` "Overflow" case now
// succeeds and returns the int64-shaped value. The `%d` printf
// formats for parse_int_at results are now `%lld` for
// std::int64_t. Behaviour is unchanged for the inputs v0.13.0
// accepted.

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
//
// v0.14.0 update: parse_int_at now accepts an optional leading '+' or
// '-' (was LeadingSign in v0.13.0); its return type widened from
// `int` to `std::int64_t`. The v0.13.0 stale `+9`/`-9` failure cases
// now SUCCEED; the past-INT_MAX `99999999999` Overflow case now
// SUCCEEDS and returns the int64-shaped value. The want_val field is
// therefore std::int64_t, and the printf format is `%lld`.
// ---------------------------------------------------------------------------
static void section_parse_int_at() {
    print_section("Section 1: psp::parse_int_at — cursor advances past digits");

    struct Case { std::string input; bool want_ok; std::int64_t want_val; const char* want_err; };
    const Case cases[] = {
        {"12345",      true,  12345,   nullptr},     // pure digits, fully consumed
        {"0",          true,  0,       nullptr},     // single zero
        {"7",          true,  7,       nullptr},     // single digit
        {"123abc",     true,  123,     nullptr},     // trailing garbage — cursor leaves "abc"
        {"42,17,99",   true,  42,      nullptr},     // CSV-style first token
        // v0.14.0 additions: '+' and '-' are accepted by parse_int_at now.
        {"+9",         true,  9,       nullptr},
        {"-9",         true,  -9,      nullptr},
        {"+100 abc",   true,  100,     nullptr},
        // v0.14.0 widening: past-INT_MAX no longer overflows — it fits
        // in std::int64_t, so it now succeeds.
        {"99999999999",true,  99999999999LL, nullptr},
        {"",           false, 0,       "Empty"},
        {"abc",        false, 0,       "NotADigit"}, // not a single digit at the front
        {"  42",       false, 0,       "NotADigit"}, // leading whitespace is not a digit
    };

    int g_pass = 0, g_fail = 0;
    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::parse_int_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_int_at(\"%s\") should have succeeded but got %s\n",
                            orig.c_str(), std::format("{}", r.error()).c_str());
                ++g_fail;
                continue;
            }
            if (*r != c.want_val) {
                std::printf("  FAIL: parse_int_at(\"%s\") = %lld, want %lld\n",
                            orig.c_str(),
                            static_cast<long long>(*r),
                            static_cast<long long>(c.want_val));
                ++g_fail;
                continue;
            }
            std::printf("  PASS: parse_int_at(\"%s\") = %lld, s now = \"%s\" (advanced %zu)\n",
                        orig.c_str(),
                        static_cast<long long>(*r),
                        span_to_string(s).c_str(),
                        orig.size() - s.size());
            ++g_pass;
        } else {
            if (r) {
                std::printf("  FAIL: parse_int_at(\"%s\") should have failed but got %lld\n",
                            orig.c_str(), static_cast<long long>(*r));
                ++g_fail;
                continue;
            }
            std::string err_name = std::format("{}", r.error());
            if (err_name != c.want_err) {
                std::printf("  FAIL: parse_int_at(\"%s\") got error %s, want %s\n",
                            orig.c_str(), err_name.c_str(), c.want_err);
                ++g_fail;
                continue;
            }
            // Distinguish three s-mutation outcomes on failure:
            //   unchanged  = s == orig              (Empty / NotADigit)
            //   advanced   = s.size() < orig.size() (Overflow — cursor committed the consumed prefix)
            //   rewoound   = s.size() > orig.size() (should never happen — would be a bug)
            std::string rem = span_to_string(s);
            const char* verdict;
            if (rem == orig)         verdict = "unchanged";
            else if (s.size() < orig.size()) verdict = "advanced (overflow committed)";
            else                     verdict = "REWOUND BUG!";
            std::printf("  PASS: parse_int_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), err_name.c_str(), rem.c_str(), verdict);
            ++g_pass;
        }
    }
    std::printf("  [Section 1: %d pass, %d fail]\n", g_pass, g_fail);
}

// ---------------------------------------------------------------------------
// Section 2 — parse_uint_at: the unsigned cursor.
//
// Difference vs parse_int_at: '+' is ACCEPTED (no-op), '-' is REJECTED
// as LeadingSign. The same cursor-advances / failure-leaves-s-unchanged
// contract applies.
//
// v0.14.0 update: parse_uint_at is UNCHANGED in API. The '+' it
// accepted in v0.13.0 is still accepted (no-op); '-' is still
// rejected as LeadingSign. Section 2's test cases were carried over
// unchanged from v0.13.0.
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

    int g_pass = 0, g_fail = 0;
    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::parse_uint_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_uint_at(\"%s\") should have succeeded but got %s\n",
                            orig.c_str(), std::format("{}", r.error()).c_str());
                ++g_fail;
                continue;
            }
            if (*r != c.want_val) {
                std::printf("  FAIL: parse_uint_at(\"%s\") = %u, want %u\n",
                            orig.c_str(), *r, c.want_val);
                ++g_fail;
                continue;
            }
            std::printf("  PASS: parse_uint_at(\"%s\") = %u, s now = \"%s\" (advanced %zu)\n",
                        orig.c_str(), *r, span_to_string(s).c_str(),
                        orig.size() - s.size());
            ++g_pass;
        } else {
            if (r) {
                std::printf("  FAIL: parse_uint_at(\"%s\") should have failed but got %u\n",
                            orig.c_str(), *r);
                ++g_fail;
                continue;
            }
            std::string err_name = std::format("{}", r.error());
            if (err_name != c.want_err) {
                std::printf("  FAIL: parse_uint_at(\"%s\") got error %s, want %s\n",
                            orig.c_str(), err_name.c_str(), c.want_err);
                ++g_fail;
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict;
            if (rem == orig)                  verdict = "unchanged";
            else if (s.size() < orig.size())  verdict = "advanced (overflow committed)";
            else                              verdict = "REWOUND BUG!";
            std::printf("  PASS: parse_uint_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), err_name.c_str(), rem.c_str(), verdict);
            ++g_pass;
        }
    }
    std::printf("  [Section 2: %d pass, %d fail]\n", g_pass, g_fail);
}

// ---------------------------------------------------------------------------
// Section 3 — parse_double_at: cursor advances past the double literal.
//
// Includes integer-only, fractional, exponent, and trailing-garbage
// cases. Compare against parse_double (the whole-span variant from
// Jul 14): "1.5x" is REJECTED by parse_double but ACCEPTED by
// parse_double_at (which leaves "x" in `s`).
//
// v0.14.0 update: parse_double_at now accepts an optional leading
// '+' or '-' (was LeadingSign in v0.13.0). The v0.13.0 stale
// `+1.0`/`-1.0` failure cases now SUCCEED.
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
        // v0.14.0 additions: '+' and '-' are accepted by parse_double_at now.
        {"+1.0",      true,  1.0,        nullptr, 0.0},
        {"-1.0",      true,  -1.0,       nullptr, 0.0},
        {"+3.14",     true,  3.14,       nullptr, 1e-9}, // v0.14.0: signed exponent is unchanged; show +3.14 alone parses cleanly
        {"-0.5rest",  true,  -0.5,       nullptr, 1e-9},
        {"",          false, 0.0,        "Empty", 0.0},
        {".",         false, 0.0,        "MissingFraction", 0.0},
        {"1e",        false, 0.0,        "BadExponent", 0.0},
        {"1.2.3",     true,  1.2,        nullptr, 1e-9},  // stops at second '.'
    };

    int g_pass = 0, g_fail = 0;
    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::parse_double_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_double_at(\"%s\") should have succeeded but got %s\n",
                            orig.c_str(), std::format("{}", r.error()).c_str());
                ++g_fail;
                continue;
            }
            double diff = (*r > c.want_val) ? (*r - c.want_val) : (c.want_val - *r);
            if (diff > c.tol) {
                std::printf("  FAIL: parse_double_at(\"%s\") = %g, want %g\n",
                            orig.c_str(), *r, c.want_val);
                ++g_fail;
                continue;
            }
            std::printf("  PASS: parse_double_at(\"%s\") = %g, s now = \"%s\" (advanced %zu)\n",
                        orig.c_str(), *r, span_to_string(s).c_str(),
                        orig.size() - s.size());
            ++g_pass;
        } else {
            if (r) {
                std::printf("  FAIL: parse_double_at(\"%s\") should have failed but got %g\n",
                            orig.c_str(), *r);
                ++g_fail;
                continue;
            }
            std::string err_name = std::format("{}", r.error());
            if (err_name != c.want_err) {
                std::printf("  FAIL: parse_double_at(\"%s\") got error %s, want %s\n",
                            orig.c_str(), err_name.c_str(), c.want_err);
                ++g_fail;
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict;
            if (rem == orig)                  verdict = "unchanged";
            else if (s.size() < orig.size())  verdict = "advanced (overflow committed)";
            else                              verdict = "REWOUND BUG!";
            std::printf("  PASS: parse_double_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), err_name.c_str(), rem.c_str(), verdict);
            ++g_pass;
        }
    }
    std::printf("  [Section 3: %d pass, %d fail]\n", g_pass, g_fail);
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
//
// v0.14.0 update: parse_int_at returns std::int64_t, so the %d
// printf format for the int result is now %lld.
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
            // v0.14.0: parse_int_at returns std::int64_t, so %lld.
            std::printf("%lld", static_cast<long long>(*r));
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
//
// v0.14.0 update: parse_int_at returns std::int64_t, so the %d
// printf format is now %lld in the int step.
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
            // v0.14.0: parse_int_at returns std::int64_t, so %lld.
            std::printf("    step %d (int): %lld, remainder = \"%s\"\n",
                        step, static_cast<long long>(*r),
                        span_to_string(s).c_str());
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
//
// v0.14.0 update: parse_int_at returns std::int64_t (was int), so
// `sizeof(std::expected<std::int64_t, ParseError>)` is reported
// alongside the unchanged `int` / `unsigned` / `double` rows.
// ---------------------------------------------------------------------------
static void section_probes() {
    print_section("Section 6: sizeof / feature probes");

    std::printf("  sizeof(int)                                       = %zu\n", sizeof(int));
    std::printf("  sizeof(unsigned)                                  = %zu\n", sizeof(unsigned));
    std::printf("  sizeof(double)                                    = %zu\n", sizeof(double));
    std::printf("  sizeof(std::int64_t)                              = %zu\n", sizeof(std::int64_t));
    std::printf("  sizeof(ParseError)                                = %zu\n", sizeof(ParseError));
    std::printf("  sizeof(psp::Span<const char>)                     = %zu\n", sizeof(psp::Span<const char>));
    std::printf("  sizeof(std::expected<int, ParseError>)            = %zu\n",
                sizeof(std::expected<int, ParseError>));
    std::printf("  sizeof(std::expected<unsigned, ParseError>)       = %zu\n",
                sizeof(std::expected<unsigned, ParseError>));
    std::printf("  sizeof(std::expected<double, ParseError>)         = %zu\n",
                sizeof(std::expected<double, ParseError>));
    // v0.14.0: parse_int_at's return type widened from int to int64_t.
    std::printf("  sizeof(std::expected<std::int64_t, ParseError>)   = %zu\n",
                sizeof(std::expected<std::int64_t, ParseError>));
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
    std::printf("\n[psp_parser_streaming_consumer: all 6 sections complete, v0.14.0]\n");
    return 0;
}
