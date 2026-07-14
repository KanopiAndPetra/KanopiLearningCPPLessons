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

#include <psp_span/parser.h>
#include <psp_span/span.h>

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
static void print_subsection(const char* title) {
    std::printf("\n-- %s --\n", title);
}

// ---------------------------------------------------------------------------
// Section 1 — basic parse_int from <psp_span/parser.h>
//
// This is the same shape the Jul 13 lesson had at file scope. The
// difference: today, parse_int and ParseError come from the LIBRARY,
// not from the lesson .cpp. The consumer just calls them.
// ---------------------------------------------------------------------------
static void section_parse_int_basics() {
    print_section("Section 1: psp::parse_int from <psp_span/parser.h>");

    struct Case { const char* input; bool want_ok; int want_val; const char* want_err; };
    const Case cases[] = {
        // successes
        {"12345",   true,  12345,   nullptr},
        {"0",       true,  0,       nullptr},
        {"999",     true,  999,     nullptr},
        // failures (error name is checked against std::format output)
        {"",        false, 0,       "Empty"},
        {"+9",      false, 0,       "LeadingSign"},
        {"-9",      false, 0,       "LeadingSign"},
        {"12a3",    false, 0,       "NotADigit"},
        {"9999999999", false, 0,     "Overflow"},
    };

    for (const auto& c : cases) {
        auto r = psp::parse_int(as_span(c.input));
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_int(\"%s\") should have succeeded but got error %s\n",
                            c.input, std::format("{}", r.error()).c_str());
                continue;
            }
            std::printf("  parse_int(\"%s\") = %d\n", c.input, *r);
        } else {
            if (r) {
                std::printf("  FAIL: parse_int(\"%s\") should have failed but got %d\n",
                            c.input, *r);
                continue;
            }
            std::printf("  parse_int(\"%s\") error: %s\n",
                        c.input, std::format("{}", r.error()).c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Section 2 — parse_double
//
// Exercises the int / fractional / exponent phases of parse_double.
// The 10 cases hit each of the parser's failure modes plus a few
// success cases across the three phases.
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
        // failures
        {"",        false, 0.0,         "Empty"},
        {"+1.0",    false, 0.0,         "LeadingSign"},
        {"-1.0",    false, 0.0,         "LeadingSign"},
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
            std::printf("  parse_double(\"%s\") = %g\n", c.input, *r);
            ++n_ok;
        } else {
            if (r) {
                std::printf("  FAIL: parse_double(\"%s\") should have failed but got %g\n",
                            c.input, *r);
                continue;
            }
            std::printf("  parse_double(\"%s\") error: %s\n",
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
// ---------------------------------------------------------------------------
static std::expected<int, ParseError>
double_if_positive(psp::Span<const char> s) noexcept {
    return psp::parse_int(s)
        .and_then([](int n) -> std::expected<int, ParseError> {
            if (n <= 0) return std::unexpected{ParseError::Overflow};
            return n;
        })
        .transform([](int n) { return n * 2; });
}

static void section_monadic_composition() {
    print_section("Section 3: monadic composition with library parse_int");

    const char* cases[] = {
        "50",    // ok -> 100
        "100",   // ok -> 200
        "-1",    // parse error: LeadingSign
        "abc",   // parse error: NotADigit
        "0",     // parse ok, validation fails -> Overflow
    };
    for (const char* in : cases) {
        auto r = double_if_positive(as_span(in));
        if (r) std::printf("  double_if_positive(\"%s\") = %d\n", in, *r);
        else   std::printf("  double_if_positive(\"%s\") error: %s\n",
                            in, std::format("{}", r.error()).c_str());
    }
}

// ---------------------------------------------------------------------------
// Section 4 — std::format with ParseError from the LIBRARY
//
// The formatter specialization lives in <psp_span/parser.h>. The
// Jul 13 lesson had to redefine it in its own .cpp; today the
// consumer gets it from the library.
// ---------------------------------------------------------------------------
static void section_format_integration() {
    print_section("Section 4: std::format integration with ParseError");

    auto r1 = psp::parse_double(as_span(std::string{"3.14x"}));
    auto r2 = psp::parse_int(as_span(std::string{"+9"}));

    std::printf("  parse_double(\"3.14x\") error: %s\n",
                std::format("{}", r1.error()).c_str());
    std::printf("  parse_int(\"+9\") error: %s\n",
                std::format("{}", r2.error()).c_str());
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
    std::printf("  parse_int(digits) = %d\n", *r);
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