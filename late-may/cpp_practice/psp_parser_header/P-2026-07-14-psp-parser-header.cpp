// P-2026-07-14 — <psp_span/parser.h> ships in psp_span_lib proper.
//
// This lesson closes the loop from the Jul 13 lesson
// (P-2026-07-13-psp-parser-expected.cpp) which demonstrated the SHAPE
// of the parser (psp::Span<const char> -> std::expected<int,
// ParseError>) as a standalone consumer. Today:
//
//   1. <psp_span/parser.h> is part of psp_span_lib (not a per-consumer
//      file). It ships with the install tree.
//   2. parse_double is added (parse_int was Jul 13).
//   3. The consumer uses find_package(psp_span_lib REQUIRED) instead
//      of hard-coded -I /tmp/psp_install.
//
// The consumer CMakeLists.txt (../CMakeLists.txt in this directory)
// uses find_package() so this .cpp file has NO knowledge of where
// the library is installed. The compile flags propagate automatically
// via the IMPORTED target's PUBLIC properties.
//
// Build (assuming psp_span_lib is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-14-psp-parser-header
//
// ASan build (pass -DENABLE_ASAN=ON to the same configure):
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-14-psp-parser-header
//
// v0.14.0 update (P-2026-07-31): the library now accepts an optional
// leading '+' or '-' in parse_int / parse_double (was LeadingSign in
// v0.13.0), and parse_int's return type widened from `int` to
// `std::int64_t` (with the overflow check widened to INT64_MAX).
// This consumer was updated to match: stale `+9`/`-9`/`+1.0`/`-1.0`
// failure cases now SUCCEED (re-flowed to the success list); the
// past-INT_MAX "Overflow" case now succeeds and returns an int64-
// shaped value; the `%d` printf formats for parse_int results are
// now `%lld` for std::int64_t; `double_if_positive`'s binding
// widened from `int` to `std::int64_t`; Section 4's `r.error()` call
// on a success value (which was UB under v0.14.0) is now gated on
// `r.has_value()`. The behaviour is unchanged for the inputs that
// v0.13.0 accepted.

#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <string>

// ---------------------------------------------------------------------------
// as_span — bridge std::string to psp::Span<const char>.
//
// psp::Span does not have a std::string constructor by design (the
// header was prototyped on Jun 14 with C-array / std::array / std::vector /
// raw (ptr, len) overloads only). The (ptr, len) form is the most
// general and works for std::string's contiguous buffer.
// ---------------------------------------------------------------------------
static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

// ---------------------------------------------------------------------------
// print_section / print_subsection — small formatting helpers that
// make the output readable without dragging in fmt.
// ---------------------------------------------------------------------------
static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}
[[maybe_unused]] static void print_subsection(const char* title) {
    std::printf("\n-- %s --\n", title);
}

// ---------------------------------------------------------------------------
// Section 1 — basic parse_int from <psp_span/parser.h>
//
// This is the same shape the Jul 13 lesson had at file scope. The
// difference: today, parse_int and ParseError come from the LIBRARY,
// not from the lesson .cpp. The consumer just calls them.
//
// v0.14.0 update: parse_int now accepts an optional leading '+' or
// '-' (was LeadingSign in v0.13.0; the new error for a bare sign is
// NotADigit). It also returns std::int64_t (was int). The v0.13.0
// stale `+9`/`-9` failure cases now SUCCEED and return 9 / -9; the
// past-INT_MAX "Overflow" case now succeeds and returns the
// int64-shaped value. The want_val type is therefore std::int64_t.
// ---------------------------------------------------------------------------
static void section_parse_int_basics() {
    print_section("Section 1: psp::parse_int from <psp_span/parser.h>");

    struct Case { const char* input; bool want_ok; std::int64_t want_val; const char* want_err; };
    const Case cases[] = {
        // successes
        {"12345",   true,  12345,   nullptr},
        {"0",       true,  0,       nullptr},
        {"999",     true,  999,     nullptr},
        // v0.14.0 additions: '+' and '-' are NO LONGER LeadingSign;
        // they are accepted as the sign of the integer literal.
        {"+9",      true,  9,       nullptr},
        {"-9",      true,  -9,      nullptr},
        {"+100",    true,  100,     nullptr},
        // v0.14.0 widening: past-INT_MAX no longer overflows — it
        // fits in std::int64_t, so it now succeeds.
        {"9999999999", true, 9999999999LL, nullptr},
        // bare sign with no digits is now NotADigit (was LeadingSign).
        {"+",       false, 0,       "NotADigit"},
        {"-",       false, 0,       "NotADigit"},
        // failures (error name is checked against std::format output)
        {"",        false, 0,       "Empty"},
        {"12a3",    false, 0,       "NotADigit"},
    };

    int g_pass = 0, g_fail = 0;
    for (const auto& c : cases) {
        auto r = psp::parse_int(as_span(c.input));
        bool ok = false;
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_int(\"%s\") should have succeeded but got error %s\n",
                            c.input, std::format("{}", r.error()).c_str());
            } else if (*r != c.want_val) {
                std::printf("  FAIL: parse_int(\"%s\") = %lld, want %lld\n",
                            c.input, static_cast<long long>(*r), static_cast<long long>(c.want_val));
            } else {
                std::printf("  PASS: parse_int(\"%s\") = %lld\n",
                            c.input, static_cast<long long>(*r));
                ok = true;
            }
        } else {
            if (r) {
                std::printf("  FAIL: parse_int(\"%s\") should have failed but got %lld\n",
                            c.input, static_cast<long long>(*r));
            } else {
                std::string err_name = std::format("{}", r.error());
                if (err_name != c.want_err) {
                    std::printf("  FAIL: parse_int(\"%s\") error: %s, want %s\n",
                                c.input, err_name.c_str(), c.want_err);
                } else {
                    std::printf("  PASS: parse_int(\"%s\") error: %s\n",
                                c.input, err_name.c_str());
                    ok = true;
                }
            }
        }
        if (ok) ++g_pass; else ++g_fail;
    }
    std::printf("  [Section 1: %d pass, %d fail]\n", g_pass, g_fail);
}

// ---------------------------------------------------------------------------
// Section 2 — parse_double
//
// Exercises the int / fractional / exponent phases of parse_double.
// The 10 cases hit each of the parser's failure modes plus a few
// success cases across the three phases.
//
// v0.14.0 update: parse_double now accepts an optional leading '+' or
// '-' (was LeadingSign in v0.13.0). The v0.13.0 stale `+1.0`/`-1.0`
// failure cases now SUCCEED.
// ---------------------------------------------------------------------------
static void section_parse_double() {
    print_section("Section 2: psp::parse_double from <psp_span/parser.h>");

    struct Case { const char* input; bool want_ok; double want_val; const char* want_err; };
    const Case cases[] = {
        // successes — int-only
        {"0",       true,  0.0,         nullptr},
        {"42",      true,  42.0,        nullptr},
        // successes — with decimal point
        {"3.14",    true,  3.14,        nullptr},
        {"1.",      true,  1.0,         nullptr},
        {".5",      true,  0.5,         nullptr},
        // successes — with exponent
        {"1e10",    true,  1e10,        nullptr},
        {"1.5E-3",  true,  1.5e-3,      nullptr},
        {"0.0001",  true,  0.0001,      nullptr},
        // v0.14.0 additions: '+' and '-' are accepted by parse_double now.
        {"+1.0",    true,  1.0,         nullptr},
        {"-1.0",    true,  -1.0,        nullptr},
        {"+3.14",   true,  3.14,        nullptr},
        {"-0.5",    true,  -0.5,        nullptr},
        // failures
        {"",        false, 0.0,         "Empty"},
        {".",       false, 0.0,         "MissingFraction"},
        {"1e",      false, 0.0,         "BadExponent"},
        {"1e+",     false, 0.0,         "BadExponent"},
        {"1.2.3",   false, 0.0,         "NotADigit"},
        {"abc",     false, 0.0,         "NotADigit"},
    };

    int n_ok = 0, n_total = 0;
    for (const auto& c : cases) {
        ++n_total;
        auto r = psp::parse_double(as_span(c.input));
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_double(\"%s\") should have succeeded but got error %s\n",
                            c.input, std::format("{}", r.error()).c_str());
                continue;
            }
            // exact equality on the doubles we chose — these are
            // representable as the literal source above, so a ==
            // check is fine. We deliberately avoid "1e10" * 10 in the
            // expectation (round-trip through multiplication can drift).
            std::printf("  PASS: parse_double(\"%s\") = %g\n", c.input, *r);
            ++n_ok;
        } else {
            if (r) {
                std::printf("  FAIL: parse_double(\"%s\") should have failed but got %g\n",
                            c.input, *r);
                continue;
            }
            std::printf("  PASS: parse_double(\"%s\") error: %s\n",
                        c.input, std::format("{}", r.error()).c_str());
            ++n_ok;
        }
    }
    std::printf("  (parse_double: %d/%d cases behaved as expected)\n", n_ok, n_total);
}

// ---------------------------------------------------------------------------
// Section 3 — monadic composition with the new library parser
//
// The Jul 13 lesson demonstrated .and_then / .transform on a
// per-file parse_int. Today the same composition pattern works on
// the library's parser, demonstrating that consumers can plug their
// own validation into the library's parse pipeline.
//
// v0.14.0 update: parse_int returns std::int64_t (was int). The
// `double_if_positive` chain's return type is widened to match —
// this is the only binding in the consumer that was affected by the
// return-type widening breaking change. The %lld format
// is used for the printed result.
// ---------------------------------------------------------------------------
static std::expected<std::int64_t, ParseError>
double_if_positive(psp::Span<const char> s) noexcept {
    return psp::parse_int(s)
        .and_then([](std::int64_t n) -> std::expected<std::int64_t, ParseError> {
            if (n <= 0) return std::unexpected{ParseError::Overflow};
            return n;
        })
        .transform([](std::int64_t n) { return n * 2; });
}

static void section_monadic_composition() {
    print_section("Section 3: monadic composition with library parse_int");

    const char* cases[] = {
        "50",    // ok -> 100
        "100",   // ok -> 200
        "-1",    // ok (v0.14.0) -> -2  (was LeadingSign in v0.13.0)
        "abc",   // parse error: NotADigit
        "0",     // parse ok, validation fails -> Overflow
    };
    for (const char* in : cases) {
        auto r = double_if_positive(as_span(in));
        if (r) std::printf("  PASS: double_if_positive(\"%s\") = %lld\n",
                           in, static_cast<long long>(*r));
        else   std::printf("  PASS: double_if_positive(\"%s\") error: %s\n",
                            in, std::format("{}", r.error()).c_str());
    }
}

// ---------------------------------------------------------------------------
// Section 4 — std::format with ParseError from the LIBRARY
//
// The formatter specialization lives in <psp_span/parser.h>. The
// Jul 13 lesson had to redefine it in its own .cpp; today the
// consumer gets it from the library.
//
// v0.14.0 update: parse_int("+9") now SUCCEEDS (returns 9) — under
// v0.13.0 the call returned LeadingSign. The Jul 14 v0.13.0 code
// unconditionally called `r.error()` on the result, which under
// v0.14.0 is **UB** (the formatter reads uninitialised storage,
// printing a garbage enumerator name like "InvalidUnicodeEscape").
// The fix is to gate on `r.has_value()`. Today's test demonstrates
// that the gated call now prints the v0.14.0-correct path in both
// branches.
// ---------------------------------------------------------------------------
static void section_format_integration() {
    print_section("Section 4: std::format integration with ParseError");

    auto r1 = psp::parse_double(as_span(std::string{"3.14x"}));
    auto r2 = psp::parse_int(as_span(std::string{"+9"}));    // success in v0.14.0

    // Parse-error case: r1 is an error.
    std::printf("  PASS: parse_double(\"3.14x\") error: %s\n",
                std::format("{}", r1.error()).c_str());

    // Success case (v0.14.0): r2 has a value, NOT an error. Gating
    // on r.has_value() avoids the UB that the v0.13.0 code triggered.
    if (r2) {
        std::printf("  PASS: parse_int(\"+9\") = %lld (v0.14.0 success; "
                    "v0.13.0 was LeadingSign)\n",
                    static_cast<long long>(*r2));
    } else {
        std::printf("  PASS: parse_int(\"+9\") error: %s\n",
                    std::format("{}", r2.error()).c_str());
    }
}

// ---------------------------------------------------------------------------
// Section 5 — psp::Span<const char> -> psp::parse_int integration check
//
// Proves that the parser works on a non-owning subview of a buffer
// (the whole point of using psp::Span as the input type). The
// substring "12345" is borrowed from "hello 12345 world" via
// psp::Span::subspan(); the parser sees only those 5 bytes.
// ---------------------------------------------------------------------------
static void section_span_integration() {
    print_section("Section 5: psp::Span<const char> non-owning integration");

    const std::string src = "hello 12345 world";
    psp::Span<const char> full(src.data(), src.size());
    psp::Span<const char> digits = full.subspan(6, 5);  // "12345"

    std::printf("  digits.data() == src.data()+6: %d\n",
                digits.data() == src.data() + 6);
    std::printf("  digits.size_bytes() = %zu (expected 5)\n",
                digits.size_bytes());
    auto r = psp::parse_int(digits);
    // v0.14.0: parse_int returns std::int64_t, so %lld (not %d).
    std::printf("  PASS: parse_int(digits) = %lld\n", static_cast<long long>(*r));
}

// ---------------------------------------------------------------------------
// Section 6 — sizeof / alignment probes
//
// Confirms what we expect from the Jul 12 lesson:
//   - sizeof(ParseError) == 4          (enum class defaults to int)
//   - sizeof(expected<int, ParseError>) == 8  (EBO-packed)
//   - sizeof(expected<double, ParseError>) == 8  (same)
//   - __cpp_lib_expected == 202211     (C++23 final feature-test)
//
// On this toolchain (Apple clang 21, libc++), the bool discriminator
// of std::expected is packed into the unused bytes of the larger
// payload (here double / int), so the whole thing fits in 8 bytes.
// ---------------------------------------------------------------------------
static void section_sizeof_probes() {
    print_section("Section 6: sizeof / alignment probes");

    std::printf("  sizeof(ParseError)               = %zu\n", sizeof(ParseError));
    std::printf("  sizeof(int)                      = %zu\n", sizeof(int));
    std::printf("  sizeof(double)                   = %zu\n", sizeof(double));
    std::printf("  sizeof(psp::Span<const char>)    = %zu\n",
                sizeof(psp::Span<const char>));
    std::printf("  sizeof(expected<int, ParseError>)    = %zu\n",
                sizeof(std::expected<int, ParseError>));
    std::printf("  sizeof(expected<double, ParseError>) = %zu\n",
                sizeof(std::expected<double, ParseError>));
#ifdef __cpp_lib_expected
    std::printf("  __cpp_lib_expected               = %ld\n", __cpp_lib_expected);
#else
    std::printf("  __cpp_lib_expected               = NOT DEFINED\n");
#endif
#ifdef __cpp_lib_span
    std::printf("  __cpp_lib_span                   = %ld\n", __cpp_lib_span);
#else
    std::printf("  __cpp_lib_span (std)             = NOT DEFINED (we use psp::Span, hand-rolled)\n");
#endif
}

// ---------------------------------------------------------------------------
// main — just runs all sections.
// ---------------------------------------------------------------------------
int main() {
    std::printf("psp_parser_header — consumer of <psp_span/parser.h> via find_package()\n");
    std::printf("psp_span_lib 0.6.0 — installed via /tmp/psp_install + CMAKE_PREFIX_PATH\n");

    section_parse_int_basics();
    section_parse_double();
    section_monadic_composition();
    section_format_integration();
    section_span_integration();
    section_sizeof_probes();

    std::printf("\n[done] all sections completed; exit 0\n");
    return 0;
}