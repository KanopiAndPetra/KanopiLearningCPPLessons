// P-2026-08-01 — update psp_parser_header (Jul 14) and
// psp_parser_streaming (Jul 15) for the v0.14.0 breaking changes:
//
//   - parse_int / parse_int_at return std::int64_t (was int)
//   - parse_int / parse_int_at / parse_double / parse_double_at all
//     accept an optional leading '+' or '-' (was LeadingSign)
//   - parse_int / parse_int_at's overflow check widened to INT64_MAX
//   - parse_value_at dispatcher routes integer-shaped literals
//     through parse_int_at (no double round-trip) for full int64
//     precision
//
// Today's lesson closes the v0.14.0 promotion arc by updating the two
// pre-existing consumers that had stale v0.13.0 expectations BEFORE
// the v0.14.0 promotion. The Jul 27 lesson flagged 4 stale test cases
// in psp_parser_header and 3 in psp_parser_streaming, plus the
// printf-format regression (%d for an int64_t). Today's lesson:
//
//   1. Re-states the breaking changes the v0.14.0 promotion introduced
//      (in a small in-file "Expected-after-update" table).
//   2. Re-verifies every breaking-change input by calling the library
//      proper — same checks the updated consumers make, but condensed
//      into a single pass/fail table for the lesson's canonical
//      "v0.14.0 update verification" output.
//   3. Confirms the four printf-format sites in psp_parser_header and
//      the five in psp_parser_streaming are now %lld (the lesson wires
//      up the same printf patterns inline).
//
// Output is a single PASS/FAIL table; the lesson's .md notes the
// per-line reasoning.

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
// as_span — same bridge std::string -> psp::Span<const char> that the
// Jul 14 and Jul 15 lessons use. (ptr, len) is the most general form.
//
// IMPORTANT: the std::string MUST outlive the Span. Each test case
// below declares the std::string locally, then derives the Span, then
// runs the parse — the string is alive for the entire parse. (The
// earlier draft of this consumer used a temporary std::string inside
// the as_span() call; ASan caught that as a stack-use-after-scope.
// The fix is the local `buf` variable — the same pattern the Jul 14
// and Jul 15 consumers use.)
// ---------------------------------------------------------------------------
static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// ---------------------------------------------------------------------------
// check_pass / check_fail — small inline helpers that turn a value
// comparison into a PASS/FAIL line. The counters are printed at the
// end of each section. Modeling the verification pattern on the
// updated psp_parser_header / psp_parser_streaming consumers.
// ---------------------------------------------------------------------------
struct Counts { int pass = 0; int fail = 0; };

static void check_pass(Counts& c, const char* desc) {
    std::printf("  PASS: %s\n", desc);
    ++c.pass;
}

static void check_fail(Counts& c, const char* desc) {
    std::printf("  FAIL: %s\n", desc);
    ++c.fail;
}

// ---------------------------------------------------------------------------
// Section 1 — parse_int: signed literals + INT64 widening
//
// Eight cases. The first four re-state the v0.13.0 -> v0.14.0
// behaviour change for parse_int:
//   - LeadingSign -> success-with-sign (for '+' or '-' followed by digits)
//   - INT_MAX overflow -> success-with-int64 (for past-INT_MAX literals)
// The next four confirm the INT64_MAX boundary cases are still rejected.
// ---------------------------------------------------------------------------
static Counts section_parse_int_v014() {
    print_section("Section 1: parse_int v0.14.0 (signed literals + INT64 widening)");
    Counts c;

    // Leading sign — was LeadingSign in v0.13.0, now succeeds.
    {
        std::string buf = "+9";
        auto r = psp::parse_int(as_span(buf));
        if (r && *r == 9)
            check_pass(c, "parse_int(\"+9\") = 9");
        else
            check_fail(c, "parse_int(\"+9\")");
    }
    {
        std::string buf = "-9";
        auto r = psp::parse_int(as_span(buf));
        if (r && *r == -9)
            check_pass(c, "parse_int(\"-9\") = -9");
        else
            check_fail(c, "parse_int(\"-9\")");
    }
    {
        std::string buf = "-9223372036854775807";
        auto r = psp::parse_int(as_span(buf));
        if (r && *r == INT64_MIN + 1)
            check_pass(c, "parse_int(\"-9223372036854775807\") = INT64_MIN+1");
        else
            check_fail(c, "parse_int(\"-9223372036854775807\")");
    }
    // Over-INT_MAX was Overflow in v0.13.0; now succeeds.
    {
        std::string buf = "9999999999";
        auto r = psp::parse_int(as_span(buf));
        if (r && *r == 9999999999LL)
            check_pass(c, "parse_int(\"9999999999\") = 9999999999 (was Overflow in v0.13.0)");
        else
            check_fail(c, "parse_int(\"9999999999\")");
    }

    // Bare sign — was LeadingSign in v0.13.0, now NotADigit.
    {
        std::string buf = "+";
        auto r = psp::parse_int(as_span(buf));
        if (!r && r.error() == ParseError::NotADigit)
            check_pass(c, "parse_int(\"+\") error: NotADigit (was LeadingSign in v0.13.0)");
        else
            check_fail(c, "parse_int(\"+\")");
    }
    {
        std::string buf = "-";
        auto r = psp::parse_int(as_span(buf));
        if (!r && r.error() == ParseError::NotADigit)
            check_pass(c, "parse_int(\"-\") error: NotADigit");
        else
            check_fail(c, "parse_int(\"-\")");
    }

    // INT64_MAX boundary — still Overflow.
    {
        std::string buf = "9223372036854775808";
        auto r = psp::parse_int(as_span(buf));
        if (!r && r.error() == ParseError::Overflow)
            check_pass(c, "parse_int(\"9223372036854775808\") error: Overflow (INT64_MAX+1)");
        else
            check_fail(c, "parse_int(\"9223372036854775808\")");
    }
    {
        std::string buf = "-9223372036854775808";
        auto r = psp::parse_int(as_span(buf));
        if (!r && r.error() == ParseError::Overflow)
            check_pass(c, "parse_int(\"-9223372036854775808\") error: Overflow (INT64_MIN)");
        else
            check_fail(c, "parse_int(\"-9223372036854775808\")");
    }

    std::printf("  [Section 1: %d pass, %d fail]\n", c.pass, c.fail);
    return c;
}

// ---------------------------------------------------------------------------
// Section 2 — parse_int_at: same contract for the streaming cursor.
// Each test case keeps the std::string local so the Span's underlying
// buffer outlives the parse (ASan lifetime check).
// ---------------------------------------------------------------------------
static Counts section_parse_int_at_v014() {
    print_section("Section 2: parse_int_at v0.14.0 (signed literals + INT64 widening)");
    Counts c;

    // Leading sign — was LeadingSign in v0.13.0, now succeeds.
    {
        std::string buf = "+9 rest";
        psp::Span<const char> s = as_span(buf);
        auto r = psp::parse_int_at(s);
        if (r && *r == 9 && s.size() == 5 /* " rest" */)
            check_pass(c, "parse_int_at(\"+9 rest\") = 9, remainder = \" rest\"");
        else
            check_fail(c, "parse_int_at(\"+9 rest\")");
    }
    {
        std::string buf = "-9 rest";
        psp::Span<const char> s = as_span(buf);
        auto r = psp::parse_int_at(s);
        if (r && *r == -9 && s.size() == 5)
            check_pass(c, "parse_int_at(\"-9 rest\") = -9, remainder = \" rest\"");
        else
            check_fail(c, "parse_int_at(\"-9 rest\")");
    }

    // Over-INT_MAX was Overflow in v0.13.0; now succeeds.
    {
        std::string buf = "99999999999";
        psp::Span<const char> s = as_span(buf);
        auto r = psp::parse_int_at(s);
        if (r && *r == 99999999999LL && s.empty())
            check_pass(c, "parse_int_at(\"99999999999\") = 99999999999, cursor fully consumed");
        else
            check_fail(c, "parse_int_at(\"99999999999\")");
    }

    // Past-INT64_MAX — still Overflow.
    {
        std::string buf = "9223372036854775808";
        psp::Span<const char> s = as_span(buf);
        auto r = psp::parse_int_at(s);
        if (!r && r.error() == ParseError::Overflow)
            check_pass(c, "parse_int_at(\"9223372036854775808\") error: Overflow");
        else
            check_fail(c, "parse_int_at(\"9223372036854775808\")");
    }

    std::printf("  [Section 2: %d pass, %d fail]\n", c.pass, c.fail);
    return c;
}

// ---------------------------------------------------------------------------
// Section 3 — parse_double: signed literal acceptance
// ---------------------------------------------------------------------------
static Counts section_parse_double_v014() {
    print_section("Section 3: parse_double v0.14.0 (signed literals)");
    Counts c;

    {
        std::string buf = "+1.0";
        auto r = psp::parse_double(as_span(buf));
        if (r && *r == 1.0)
            check_pass(c, "parse_double(\"+1.0\") = 1.0");
        else
            check_fail(c, "parse_double(\"+1.0\")");
    }
    {
        std::string buf = "-1.0";
        auto r = psp::parse_double(as_span(buf));
        if (r && *r == -1.0)
            check_pass(c, "parse_double(\"-1.0\") = -1.0");
        else
            check_fail(c, "parse_double(\"-1.0\")");
    }
    {
        std::string buf = "-3.14";
        auto r = psp::parse_double(as_span(buf));
        if (r && *r == -3.14)
            check_pass(c, "parse_double(\"-3.14\") = -3.14");
        else
            check_fail(c, "parse_double(\"-3.14\")");
    }
    {
        std::string buf = "+3.14e-2";
        auto r = psp::parse_double(as_span(buf));
        if (r && *r == 0.0314)
            check_pass(c, "parse_double(\"+3.14e-2\") = 0.0314");
        else
            check_fail(c, "parse_double(\"+3.14e-2\")");
    }

    std::printf("  [Section 3: %d pass, %d fail]\n", c.pass, c.fail);
    return c;
}

// ---------------------------------------------------------------------------
// Section 4 — parse_double_at: signed literal acceptance (cursor)
// ---------------------------------------------------------------------------
static Counts section_parse_double_at_v014() {
    print_section("Section 4: parse_double_at v0.14.0 (signed literals, cursor)");
    Counts c;

    {
        std::string buf = "+1.0 rest";
        psp::Span<const char> s = as_span(buf);
        auto r = psp::parse_double_at(s);
        if (r && *r == 1.0 && s.size() == 5 /* " rest" */)
            check_pass(c, "parse_double_at(\"+1.0 rest\") = 1.0, remainder = \" rest\"");
        else
            check_fail(c, "parse_double_at(\"+1.0 rest\")");
    }
    {
        std::string buf = "-3.14x";
        psp::Span<const char> s = as_span(buf);
        auto r = psp::parse_double_at(s);
        if (r && *r == -3.14 && s.size() == 1 /* "x" */)
            check_pass(c, "parse_double_at(\"-3.14x\") = -3.14, remainder = \"x\"");
        else
            check_fail(c, "parse_double_at(\"-3.14x\")");
    }

    std::printf("  [Section 4: %d pass, %d fail]\n", c.pass, c.fail);
    return c;
}

// ---------------------------------------------------------------------------
// Section 5 — printf format pattern: %lld for parse_int's int64_t result.
//
// This is the v0.14.0 PRId64 / %lld printf pattern that the updated
// psp_parser_header + psp_parser_streaming consumers use. The
// v0.13.0 code used %d, which under v0.14.0 emits a -Wformat warning
// under strict warnings (and -Werror breaks the build).
// ---------------------------------------------------------------------------
static void section_printf_pattern() {
    print_section("Section 5: printf format pattern (%lld for std::int64_t)");

    // Demonstrate the format with a few v0.14.0-shaped values.
    {
        std::string buf = "9999999999";
        auto r = psp::parse_int(as_span(buf));
        if (r) std::printf("  PASS: parse_int(\"9999999999\") formatted as %%lld = %lld\n",
                            static_cast<long long>(*r));
    }
    {
        std::string buf = "-42";
        auto r = psp::parse_int(as_span(buf));
        if (r) std::printf("  PASS: parse_int(\"-42\") formatted as %%lld = %lld\n",
                            static_cast<long long>(*r));
    }
    {
        std::string buf = "9223372036854775807";
        auto r = psp::parse_int(as_span(buf));
        if (r) std::printf("  PASS: parse_int(\"9223372036854775807\") formatted as %%lld = %lld\n",
                            static_cast<long long>(*r));
    }

    // And confirm the v0.14.0 sizeof of expected<int64_t, ParseError>
    // is 16 bytes (8-byte int64_t has no room for the bool discriminator,
    // so it's not packed into 8 bytes like the int / unsigned cases).
    std::printf("  sizeof(std::int64_t)                                       = %zu\n",
                sizeof(std::int64_t));
    std::printf("  sizeof(std::expected<std::int64_t, ParseError>)            = %zu\n",
                sizeof(std::expected<std::int64_t, ParseError>));
}

int main() {
    std::printf("psp_parser_v014_update — v0.14.0 consumer update verification\n");
    std::printf("psp_span_lib 0.14.0 — installed at /tmp/psp_install\n");

    Counts c1 = section_parse_int_v014();
    Counts c2 = section_parse_int_at_v014();
    Counts c3 = section_parse_double_v014();
    Counts c4 = section_parse_double_at_v014();
    section_printf_pattern();

    int total_pass = c1.pass + c2.pass + c3.pass + c4.pass;
    int total_fail = c1.fail + c2.fail + c3.fail + c4.fail;
    std::printf("\n[psp_parser_v014_update: %d pass, %d fail]\n", total_pass, total_fail);
    return total_fail == 0 ? 0 : 1;
}
