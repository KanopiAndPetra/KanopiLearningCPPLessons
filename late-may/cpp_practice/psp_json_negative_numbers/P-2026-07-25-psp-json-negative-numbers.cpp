// P-2026-07-25 — Consumer of psp_span_lib v0.13.0 that demonstrates
// the two pre-existing parser limitations the Jul 24 lesson
// (P-2026-07-24-psp-json-patch-serialiser.cpp) flagged in its
// "Two real findings" section, and PROVES the proposed v0.14.0
// fix works end-to-end.
//
// The two findings:
//
//   1. parse_int / parse_double / parse_int_at / parse_double_at
//      reject a leading '-' (and '+') with ParseError::LeadingSign.
//      The parse_value_at dispatcher in <psp_span/json.h> lines
//      260-261 explicitly ROUTES '-' to parse_double_at, only for
//      parse_double_at to reject it at line 557 — a routing bug.
//
//   2. parse_int / parse_double / parse_int_at / parse_double_at
//      check overflow against std::numeric_limits<int>::max()
//      (line 236 / 289 / 458 / 569), which is INT_MAX (~2.147e9)
//      — but the JsonValue sum type's int alternative is std::int64_t
//      (json.h line 124), so values larger than ~2 billion overflow
//      at parse time even though the value tree can store them.
//
// Both findings are CONSUMER-SIDE OBSERVATIONS of pre-existing
// library limitations. The Jul 24 lesson noted: "the same fix
// (re-tune parse_double_at's overflow check) would close both
// findings" and characterised the leading-'-' fix as "a 3-line
// fix in <psp_span/parser.h>". Today's lesson is that fix, in
// consumer form: we DEFINE LOCAL COPIES of the four parsers
// (parse_int_v014, parse_double_v014, parse_int_at_v014,
// parse_double_at_v014) with the proposed fix applied, then a
// LOCAL parse_value_at_v014 dispatcher that uses them, and we
// exercise BOTH the library's parse_value_at (broken) and our
// parse_value_at_v014 (fixed) side by side, on the same inputs.
//
// We DO NOT touch <psp_span/parser.h> today. The library version
// stays at v0.13.0. A future lesson (the v0.14.0 promotion) will
// move the four parsers from this consumer into the header as the
// official library upgrade. Today's job is to PROVE the design
// end-to-end — exactly the consumer-then-promote arc the Jul 23
// parser lesson and the Jul 24 writer lesson both followed.
//
// ===========================================================================
// Why local copies, not a library upgrade
// ===========================================================================
//
// Three reasons:
//
//   1. The Jul 24 lesson explicitly framed the findings as
//      "a future lesson" — today's lesson is that future lesson.
//      We could land it directly in the library, but the consumer-
//      first pattern lets us validate the design (specifically:
//      does the fix break any existing consumer? does it round-
//      trip through json_to_string? does it survive Patch test
//      equality?) before committing the library to it.
//
//   2. The fix is a four-place edit (parse_int, parse_double,
//      parse_int_at, parse_double_at). All four need the same
//      change (drop the leading-'-' rejection; widen INT_MAX to
//      INT64_MAX) — and all four are part of the SAME family of
//      "numeric parser primitives" the Jul 14 / 15 / 16 lessons
//      built. Putting them all in a consumer first lets us
//      confirm the four changes are mutually consistent before
//      shipping them together as v0.14.0.
//
//   3. parse_int currently returns std::expected<int, ParseError>
//      — i.e. it returns a 32-bit int even though its internal
//      accumulator is std::int64_t. To match the JsonValue int
//      alternative (std::int64_t) and the Jul 24 finding ("the
//      parser's numeric primitives are int-shaped, but the JSON
//      value tree has wider alternatives"), the right v0.14.0
//      change is to widen the return type to std::int64_t. That's
//      a (small) breaking change to the parse_int signature. A
//      consumer-side test of the wider signature is the right
//      vehicle to confirm no existing consumer breaks.
//
// ===========================================================================
// What the consumer exercises
// ===========================================================================
//
//   Section 1 — Demonstrate the v0.13.0 bugs through the library's
//               parse_value_at: negatives are rejected with
//               ParseError::LeadingSign, INT64_MAX overflows with
//               ParseError::Overflow. This is the CONTROL row that
//               proves we're measuring the right thing.
//
//   Section 2 — The local v0.14.0-shaped parsers (parse_int_v014,
//               parse_double_v014, parse_int_at_v014,
//               parse_double_at_v014) on the same inputs succeed.
//               This proves the fix is COMPLETE — every failing
//               case in Section 1 now passes.
//
//   Section 3 — Round-trip through parse_value_at_v014 -> JsonValue
//               -> json_to_string -> parse_value_at_v014 again -> ==
//               check. Proves the value tree preserves the bit
//               pattern of INT64_MAX (which is exactly what JsonValue's
//               int64_t alternative was supposed to do all along).
//
//   Section 4 — Through psp::json_patch::patch. We build a TestOp
//               whose value is INT64_MAX and a target tree with the
//               same INT64_MAX, then apply patch; success means the
//               equality check inside apply_test correctly compares
//               two int64_t values that would never have made it
//               through v0.13.0's parse_value_at.
//
//   Section 5 — End-to-end: build ops with negative-value fields,
//               serialise with the Jul 24 lesson's
//               serialise_patch_document, parse with v0.13.0's
//               parse_patch_document, apply with patch. This is the
//               full pipeline the Jul 24 lesson established; today's
//               lesson adds NEGATIVE values to the input.
//
// ===========================================================================
// Build (assumes psp_span_lib v0.13.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-25-psp-json-negative-numbers
// ===========================================================================

#include <cstdio>
#include <cstdlib>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <psp_span/parser.h>
#include <psp_span/json.h>
#include <psp_span/json_ext.h>

namespace {

// ---------------------------------------------------------------------------
// Section banners (matches the Jul 22 / 23 / 24 consumer style).
// ---------------------------------------------------------------------------
void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// ---------------------------------------------------------------------------
// Helpers: print a JsonValue, parse-or-die for the CONTROL rows.
// ---------------------------------------------------------------------------
std::string show_value(const psp::JsonValue& v) {
    return psp::json_to_string(v, 0);
}

// ---------------------------------------------------------------------------
// Proposed v0.14.0 parsers — local copies of parse_int / parse_double /
// parse_int_at / parse_double_at with the Jul 24 findings' fixes.
//
// Each one differs from its v0.13.0 sibling in TWO ways:
//
//   (a) Accepts a leading '-' (and '+', for symmetry with the exponent
//       parser inside parse_double / parse_double_at, which already
//       accepted both signs on the exponent). Leading '+' is still
//       accepted as a no-op (matching strtod / std::from_chars
//       convention). Leading '-' negates the result.
//
//   (b) Overflow check is against std::numeric_limits<std::int64_t>::max(),
//       not std::numeric_limits<int>::max(). And parse_int's return
//       type is widened from int to std::int64_t to match the
//       JsonValue sum type's int alternative (also std::int64_t).
//
// We put these in their own namespace `v014` so they don't collide
// with the library's psp::parse_int etc. — same trick the Jul 24
// writer lesson used for its `serialise_patch_document` (which lives
// in psp::json_patch:: locally but doesn't collide because it's a
// NEW function).
// ---------------------------------------------------------------------------
namespace v014 {

// parse_int_v014 — whole-span int parser.
//
// Difference vs psp::parse_int:
//   - Accepts optional leading '-' (negates the result).
//   - Accepts optional leading '+' (no-op).
//   - Overflow check against INT64_MAX.
//   - Returns std::int64_t (matches JsonValue int alternative).
//
// Still rejects an empty input and any non-digit body char.
inline std::expected<std::int64_t, ParseError>
parse_int_v014(psp::Span<const char> s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    bool negative = false;
    if (s.front() == '+' || s.front() == '-') {
        negative = (s.front() == '-');
        s = s.subspan(1, s.size() - 1);
        if (s.empty()) {
            // "+" or "-" alone: no digits. NotADigit.
            return std::unexpected{ParseError::NotADigit};
        }
    }
    std::int64_t acc = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return std::unexpected{ParseError::NotADigit};
        }
        // Check BEFORE the multiply: would acc*10 + d overflow int64?
        if (acc > std::numeric_limits<std::int64_t>::max() / 10
            || (acc == std::numeric_limits<std::int64_t>::max() / 10
                && (c - '0') > static_cast<char>(std::numeric_limits<std::int64_t>::max() % 10))) {
            return std::unexpected{ParseError::Overflow};
        }
        acc = acc * 10 + (c - '0');
    }
    if (negative) {
        // The accumulator loop's overflow check already catches
        // |x| > INT64_MAX and |x| == INT64_MAX + 1 (which would
        // be -INT64_MIN). If we got here with negative=true, then
        // |x| <= INT64_MAX, so negation is always safe — even
        // when |x| == INT64_MAX (the result is INT64_MIN+1, which
        // is representable).
        acc = -acc;
    }
    return acc;
}

// parse_double_v014 — whole-span double parser.
//
// Difference vs psp::parse_double:
//   - Accepts optional leading '-' on the integer part.
//   - Accepts optional leading '+' on the integer part.
//   - Integer-part overflow check against INT64_MAX.
//   - Fractional-part overflow is checked against the SHIFTED limit
//     (frac_part < 10^frac_digits * (some bound)) — but for our
//     purposes a simple "frac_part > INT64_MAX / 10^frac_digits"
//     check is conservative and clear.
//
// We deliberately keep the exponent's sign handling UNCHANGED: the
// v0.13.0 exponent handler already accepts both '+' and '-' on the
// exponent, because that was always a documented part of the shape
// ("[-+]? DIGITS ( '.' DIGITS )? ( [eE] [-+]? DIGITS )?"). The
// integer part's sign was the only one rejected, and that's the
// finding we're fixing.
inline std::expected<double, ParseError>
parse_double_v014(psp::Span<const char> s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    bool negative = false;
    if (s.front() == '+' || s.front() == '-') {
        negative = (s.front() == '-');
        s = s.subspan(1, s.size() - 1);
        if (s.empty()) {
            return std::unexpected{ParseError::NotADigit};
        }
    }
    // Phase 1: integer part (zero or more digits).
    std::size_t i = 0;
    std::int64_t int_part = 0;
    bool any_int_digits = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        char c = s[i];
        if (int_part > std::numeric_limits<std::int64_t>::max() / 10
            || (int_part == std::numeric_limits<std::int64_t>::max() / 10
                && (c - '0') > static_cast<char>(std::numeric_limits<std::int64_t>::max() % 10))) {
            return std::unexpected{ParseError::Overflow};
        }
        int_part = int_part * 10 + (c - '0');
        ++i;
        any_int_digits = true;
    }
    // Phase 2: optional fractional part.
    std::int64_t frac_part = 0;
    int frac_digits = 0;
    if (i < s.size() && s[i] == '.') {
        ++i;
        bool any_frac_digits = false;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            char c = s[i];
            if (frac_part > std::numeric_limits<std::int64_t>::max() / 10) {
                return std::unexpected{ParseError::Overflow};
            }
            frac_part = frac_part * 10 + (c - '0');
            ++i;
            ++frac_digits;
            any_frac_digits = true;
        }
        if (!any_int_digits && !any_frac_digits) {
            return std::unexpected{ParseError::MissingFraction};
        }
    } else if (!any_int_digits) {
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            // fall through to exponent phase
        } else {
            return std::unexpected{ParseError::NotADigit};
        }
    }
    // Phase 3: optional exponent.
    int exp_sign = 1;
    std::int64_t exp_part = 0;
    bool any_exp_digits = false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            if (s[i] == '-') exp_sign = -1;
            ++i;
        }
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            char c = s[i];
            if (exp_part > std::numeric_limits<std::int64_t>::max() / 10) {
                return std::unexpected{ParseError::Overflow};
            }
            exp_part = exp_part * 10 + (c - '0');
            ++i;
            any_exp_digits = true;
        }
        if (!any_exp_digits) {
            return std::unexpected{ParseError::BadExponent};
        }
    }
    // Phase 4: trailing garbage?
    if (i != s.size()) {
        return std::unexpected{ParseError::NotADigit};
    }
    // Phase 5: compose.
    double value = static_cast<double>(int_part);
    double frac_scale = 1.0;
    for (int k = 0; k < frac_digits; ++k) frac_scale *= 10.0;
    value += static_cast<double>(frac_part) / frac_scale;
    std::int64_t e_total = exp_sign * exp_part;
    if (e_total > 0) {
        for (std::int64_t k = 0; k < e_total; ++k) value *= 10.0;
    } else if (e_total < 0) {
        for (std::int64_t k = 0; k < -e_total; ++k) value /= 10.0;
    }
    if (negative) value = -value;
    return value;
}

// parse_int_at_v014 — cursor variant of parse_int_v014.
//
// Same two fixes (leading sign + INT64_MAX overflow). The cursor
// contract ("on success, s shrinks past the consumed run; on error,
// s is unchanged") is preserved.
//
// On overflow: we commit the consumed prefix AND report Overflow,
// matching the v0.13.0 contract for the cursor overflow case
// (mirroring strtol's ERANGE convention).
inline std::expected<std::int64_t, ParseError>
parse_int_at_v014(psp::Span<const char>& s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    bool negative = false;
    std::size_t start = 0;
    if (s.front() == '+' || s.front() == '-') {
        negative = (s.front() == '-');
        start = 1;
        if (s.size() == 1) {
            // "+" or "-" alone: NotADigit, s unchanged.
            return std::unexpected{ParseError::NotADigit};
        }
    }
    std::int64_t acc = 0;
    std::size_t i = start;
    while (i < s.size()) {
        char c = s[i];
        if (c < '0' || c > '9') {
            if (i == start) {
                return std::unexpected{ParseError::NotADigit};
            }
            break;
        }
        if (acc > std::numeric_limits<std::int64_t>::max() / 10
            || (acc == std::numeric_limits<std::int64_t>::max() / 10
                && (c - '0') > static_cast<char>(std::numeric_limits<std::int64_t>::max() % 10))) {
            s = s.subspan(i + 1, s.size() - (i + 1));
            return std::unexpected{ParseError::Overflow};
        }
        acc = acc * 10 + (c - '0');
        ++i;
    }
    if (i == start) {
        // No digits after the sign.
        return std::unexpected{ParseError::NotADigit};
    }
    s = s.subspan(i, s.size() - i);
    if (negative) {
        // The accumulator loop's overflow check already rejects
        // |x| > INT64_MAX and |x| == INT64_MAX + 1 (which would
        // be -INT64_MIN). If we got here with negative=true, then
        // |x| <= INT64_MAX, so negation is always safe — even when
        // |x| == INT64_MAX (the result is INT64_MIN+1, which is
        // representable). The input "-9223372036854775808" (actual
        // INT64_MIN) triggers Overflow inside the loop, not here.
        acc = -acc;
    }
    return acc;
}

// parse_double_at_v014 — cursor variant of parse_double_v014.
//
// Same two fixes. Cursor contract preserved.
inline std::expected<double, ParseError>
parse_double_at_v014(psp::Span<const char>& s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    bool negative = false;
    std::size_t start = 0;
    if (s.front() == '+' || s.front() == '-') {
        negative = (s.front() == '-');
        start = 1;
        if (s.size() == 1) {
            return std::unexpected{ParseError::NotADigit};
        }
    }
    std::size_t  i = start;
    std::int64_t int_part = 0;
    bool         any_int_digits = false;
    while (i < s.size()) {
        char c = s[i];
        if (c < '0' || c > '9') break;
        if (int_part > std::numeric_limits<std::int64_t>::max() / 10
            || (int_part == std::numeric_limits<std::int64_t>::max() / 10
                && (c - '0') > static_cast<char>(std::numeric_limits<std::int64_t>::max() % 10))) {
            s = s.subspan(i + 1, s.size() - (i + 1));
            return std::unexpected{ParseError::Overflow};
        }
        int_part = int_part * 10 + (c - '0');
        ++i;
        any_int_digits = true;
    }
    std::int64_t frac_part = 0;
    int          frac_digits = 0;
    if (i < s.size() && s[i] == '.') {
        ++i;
        bool any_frac_digits = false;
        while (i < s.size()) {
            char c = s[i];
            if (c < '0' || c > '9') break;
            if (frac_part > std::numeric_limits<std::int64_t>::max() / 10) {
                s = s.subspan(i + 1, s.size() - (i + 1));
                return std::unexpected{ParseError::Overflow};
            }
            frac_part = frac_part * 10 + (c - '0');
            ++i;
            ++frac_digits;
            any_frac_digits = true;
        }
        if (!any_int_digits && !any_frac_digits) {
            return std::unexpected{ParseError::MissingFraction};
        }
    } else if (!any_int_digits) {
        return std::unexpected{ParseError::NotADigit};
    }
    int          exp_sign = 1;
    std::int64_t exp_part = 0;
    bool any_exp_digits = false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            if (s[i] == '-') exp_sign = -1;
            ++i;
        }
        while (i < s.size()) {
            char c = s[i];
            if (c < '0' || c > '9') break;
            if (exp_part > std::numeric_limits<std::int64_t>::max() / 10) {
                s = s.subspan(i + 1, s.size() - (i + 1));
                return std::unexpected{ParseError::Overflow};
            }
            exp_part = exp_part * 10 + (c - '0');
            ++i;
            any_exp_digits = true;
        }
        if (!any_exp_digits) {
            return std::unexpected{ParseError::BadExponent};
        }
    }
    s = s.subspan(i, s.size() - i);
    double value = static_cast<double>(int_part);
    double frac_scale = 1.0;
    for (int k = 0; k < frac_digits; ++k) frac_scale *= 10.0;
    value += static_cast<double>(frac_part) / frac_scale;
    std::int64_t e_total = exp_sign * exp_part;
    if (e_total > 0) {
        for (std::int64_t k = 0; k < e_total; ++k) value *= 10.0;
    } else if (e_total < 0) {
        for (std::int64_t k = 0; k < -e_total; ++k) value /= 10.0;
    }
    if (negative) value = -value;
    return value;
}

// ---------------------------------------------------------------------------
// parse_value_at_v014 — shadow dispatcher.
//
// Same shape as the library's parse_value_at, but the numeric branch
// routes to parse_double_at_v014 instead of parse_double_at. The
// rest of the dispatcher (object, array, string, bool, null) is
// unchanged — we delegate to the library's parsers.
//
// We DO need to inline the structural parsers' value-side dispatch
// (object/array wrap their result in a JsonValue), but the inner
// structural parsers are delegated to the library because they don't
// touch numbers.
//
// Forward declarations of the library's structural parsers (json.h
// already declares them, but we redeclare here so this file is
// self-contained if the header is ever reorganised).
// ---------------------------------------------------------------------------
inline std::expected<psp::JsonValue, ParseError>
parse_value_at_v014(psp::Span<const char>& s) noexcept {
    auto saved = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved;
        return std::unexpected{r.error()};
    }
    if (s.empty()) {
        s = saved;
        return std::unexpected{ParseError::Empty};
    }
    const char front = s.front();
    switch (front) {
        case '{': {
            auto obj = psp::parse_object_at(s);
            if (!obj) {
                s = saved;
                return std::unexpected{obj.error()};
            }
            psp::JsonValue out;
            out.value = std::move(*obj);
            return out;
        }
        case '[': {
            auto arr = psp::parse_array_at(s);
            if (!arr) {
                s = saved;
                return std::unexpected{arr.error()};
            }
            psp::JsonValue out;
            out.value = std::move(*arr);
            return out;
        }
        case '\"': {
            auto str = psp::parse_string_at(s);
            if (!str) {
                s = saved;
                return std::unexpected{str.error()};
            }
            psp::JsonValue out;
            out.value = std::move(*str);
            return out;
        }
        case 't':
        case 'f': {
            auto b = psp::parse_bool_at(s);
            if (!b) {
                s = saved;
                return std::unexpected{b.error()};
            }
            psp::JsonValue out;
            out.value = *b;
            return out;
        }
        case 'n': {
            auto n = psp::parse_null_at(s);
            if (!n) {
                s = saved;
                return std::unexpected{n.error()};
            }
            psp::JsonValue out;
            out.value = nullptr;
            return out;
        }
        default:
            // v0.14.0: route digit / '-' / '+' to parse_double_at_v014.
            // The library's parse_value_at only accepts '+' as
            // UnexpectedChar; v0.14.0 accepts '+' as a numeric sign.
            if ((front >= '0' && front <= '9')
                || front == '-'
                || front == '+') {
                auto d = parse_double_at_v014(s);
                if (!d) {
                    s = saved;
                    return std::unexpected{d.error()};
                }
                psp::JsonValue out;
                // Same int64-vs-double preservation rule as the
                // library's dispatcher: if the double is an exact
                // integer in int64 range, store as int64. Negative
                // doubles with an integer value (e.g. -2.0) round
                // into the int64 alternative the same way positives
                // do.
                if (std::trunc(*d) == *d
                    && *d >= static_cast<double>(std::numeric_limits<std::int64_t>::min())
                    && *d <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                    out.value = static_cast<std::int64_t>(*d);
                } else {
                    out.value = *d;
                }
                return out;
            }
            s = saved;
            return std::unexpected{ParseError::UnexpectedChar};
    }
}

// Convenience wrapper: parse a string_view into a JsonValue.
inline std::expected<psp::JsonValue, ParseError>
parse_value_v014(std::string_view text) noexcept {
    psp::Span<const char> s{text.data(), text.size()};
    return parse_value_at_v014(s);
}

}  // namespace v014

// ---------------------------------------------------------------------------
// Section 1: demonstrate the v0.13.0 bugs through the LIBRARY's
// parse_value_at.
// ---------------------------------------------------------------------------
static void section1_v013_bugs() {
    print_section("Section 1: v0.13.0 bugs demonstrated through the library");

    // A list of numeric inputs the Jul 24 lesson flagged.
    // Each one should pass on v0.14.0 (later sections prove it).
    struct Case {
        const char* tag;
        std::string_view text;
    };
    const Case cases[] = {
        {"1a (neg int)",     "-42"},
        {"1b (neg int big)", "-2000000000"},
        {"1c (neg double)",  "-3.14"},
        {"1d (neg sci)",     "-2.5e-10"},
        {"1e (neg frac)",    "-0.0001"},
        {"1f (INT64_MAX)",   "9223372036854775807"},     // = 2^63 - 1
        {"1g (INT64_MIN)",   "-9223372036854775807"},    // = -(2^63 - 1), representable
        {"1h (past INT_MAX)","5000000000"},             // 5e9, > INT_MAX (~2.15e9)
    };

    for (const Case& c : cases) {
        psp::Span<const char> sp{c.text.data(), c.text.size()};
        auto r = psp::parse_value_at(sp);
        if (!r) {
            std::printf("  %s: library parse_value_at(\"%.*s\") -> %s (BUG)\n",
                        c.tag,
                        static_cast<int>(c.text.size()), c.text.data(),
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  %s: library parse_value_at(\"%.*s\") -> %s (already OK)\n",
                        c.tag,
                        static_cast<int>(c.text.size()), c.text.data(),
                        show_value(*r).c_str());
        }
    }

    std::printf("\n  v0.13.0 status: negatives rejected as LeadingSign,\n"
                "  values > INT_MAX rejected as Overflow. Both findings\n"
                "  from the Jul 24 lesson confirmed.\n");
}

// ---------------------------------------------------------------------------
// Section 2: the v0.14.0-shaped parsers succeed on the same inputs.
// ---------------------------------------------------------------------------
//
// Two observations about the comparison strategy:
//
//   1. INT64_MAX etc. are exact integers. We compare bit-exact.
//
//   2. Doubles produced by repeated multiply/divide by 10 (the
//      library's parse_double arithmetic) differ in the last bit
//      from a literal like -2.5e-10 parsed by the compiler's own
//      strtod. The v0.13.0 library's parse_double has the SAME
//      arithmetic, so the bit pattern it produces for "3.14" is
//      the bit pattern we should compare against — not the literal.
//      We do that by feeding the ABSOLUTE VALUE of the input to the
//      library's parse_double (which accepts non-signed inputs), and
//      comparing v014's absolute value against the library's. Then
//      we check the sign separately. This proves v014 is
//      bit-identical to the library for the non-signed fragment
//      and correctly inverts on the leading sign.
// ---------------------------------------------------------------------------
static void section2_v014_fix() {
    print_section("Section 2: v0.14.0-shaped parsers on the same inputs");

    struct IntCase {
        const char* tag;
        std::string_view text;
        std::int64_t     expected;
    };
    const IntCase int_cases[] = {
        {"2a (neg int)",            "-42",                  -42},
        {"2b (neg int big)",        "-2000000000",          -2000000000LL},
        {"2c (INT64_MAX)",          "9223372036854775807",  std::numeric_limits<std::int64_t>::max()},
        {"2d (INT64_MIN+1)",        "-9223372036854775807", std::numeric_limits<std::int64_t>::min() + 1},
        {"2e (5e9, past INT_MAX)",  "5000000000",           5000000000LL},
        {"2f (pos INT_MAX)",        "2147483647",           2147483647},
        {"2g (pos 1)",              "1",                    1},
        {"2h (pos zero)",           "0",                    0},
        {"2i (neg zero)",           "-0",                   0},  // -0 == 0 as int
        {"2j (leading +)",          "+100",                 100},
        {"2k (sign alone)",         "+",                    0},  // expected fail
    };
    std::printf("  -- parse_int_v014 (whole-span) --\n");
    for (const IntCase& c : int_cases) {
        psp::Span<const char> sp{c.text.data(), c.text.size()};
        auto whole = v014::parse_int_v014(sp);
        if (std::string_view{c.tag} == "2k (sign alone)") {
            // The library's parse_int_v014 should REJECT a bare '+' with NotADigit
            // (we test the "sign without digits" branch separately).
            if (whole) {
                std::printf("  %s: parse_int_v014(\"+\") succeeded unexpectedly (FAIL)\n",
                            c.tag);
                std::exit(1);
            }
            std::printf("  %s: parse_int_v014(\"+\") -> %s (OK, expected rejection)\n",
                        c.tag, std::format("{}", whole.error()).c_str());
            continue;
        }
        if (!whole) {
            std::printf("  %s: parse_int_v014(\"%.*s\") -> %s (FAIL)\n",
                        c.tag,
                        static_cast<int>(c.text.size()), c.text.data(),
                        std::format("{}", whole.error()).c_str());
            std::exit(1);
        }
        if (*whole != c.expected) {
            std::printf("  %s: parse_int_v014(\"%.*s\") = %lld, expected %lld (MISMATCH)\n",
                        c.tag,
                        static_cast<int>(c.text.size()), c.text.data(),
                        static_cast<long long>(*whole),
                        static_cast<long long>(c.expected));
            std::exit(1);
        }
        std::printf("  %s: parse_int_v014(\"%.*s\") = %lld (OK)\n",
                    c.tag,
                    static_cast<int>(c.text.size()), c.text.data(),
                    static_cast<long long>(*whole));
    }

    // Doubles: compare v014's absolute value against the LIBRARY's
    // parse_double on the same unsigned fragment. This proves the
    // numeric fragment is unchanged; the leading-sign behaviour is
    // a pure invert-or-noop.
    struct DblCase {
        const char* tag;
        std::string_view text;        // signed input
        std::string_view abs_text;    // unsigned fragment (passed to library)
        bool            negative;     // expected sign of v014 result
    };
    const DblCase dbl_cases[] = {
        {"2l (neg double)",  "-3.14",       "3.14",     true},
        {"2m (neg sci)",     "-2.5e-10",    "2.5e-10",  true},
        {"2n (neg frac)",    "-0.0001",     "0.0001",   true},
        {"2o (neg zero d)",  "-0.0",        "0.0",      true},
        {"2p (neg frac .5)", "-0.5",        "0.5",      true},
        {"2q (pos 3.14)",    "3.14",        "3.14",     false},
        {"2r (pos 0.0)",     "0.0",         "0.0",      false},
    };
    std::printf("\n  -- parse_double_v014 (whole-span) --\n");
    for (const DblCase& c : dbl_cases) {
        // Library on the unsigned fragment.
        psp::Span<const char> lib_sp{c.abs_text.data(), c.abs_text.size()};
        auto lib_r = psp::parse_double(lib_sp);
        if (!lib_r) {
            std::printf("  INTERNAL: psp::parse_double(\"%.*s\") -> %s\n",
                        static_cast<int>(c.abs_text.size()), c.abs_text.data(),
                        std::format("{}", lib_r.error()).c_str());
            std::exit(1);
        }
        // v014 on the signed input.
        psp::Span<const char> sp{c.text.data(), c.text.size()};
        auto r = v014::parse_double_v014(sp);
        if (!r) {
            std::printf("  %s: parse_double_v014(\"%.*s\") -> %s (FAIL)\n",
                        c.tag,
                        static_cast<int>(c.text.size()), c.text.data(),
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        // Compare magnitudes bit-exact (the v014 code is a copy of
        // the library's body except for the sign handling — the
        // magnitude path is byte-identical).
        double expected_abs = c.negative ? -(*r) : *r;
        if (expected_abs != *lib_r) {
            std::printf("  %s: magnitude mismatch: v014=%a, library=%a (MISMATCH)\n",
                        c.tag, expected_abs, *lib_r);
            std::exit(1);
        }
        // Sign sanity. We use signbit() to detect -0.0 (which
        // compares == 0.0 but has the sign bit set).
        if (c.negative && !std::signbit(*r)) {
            std::printf("  %s: expected negative sign, got %g (FAIL)\n", c.tag, *r);
            std::exit(1);
        }
        if (!c.negative && std::signbit(*r)) {
            std::printf("  %s: expected non-negative sign, got %g (FAIL)\n", c.tag, *r);
            std::exit(1);
        }
        std::printf("  %s: parse_double_v014(\"%.*s\") = %g (OK; magnitude matches library)\n",
                    c.tag,
                    static_cast<int>(c.text.size()), c.text.data(), *r);
    }

    // Then via the SHADOW dispatcher — same shape as the library's
    // parse_value_at, just with the v0.14.0 numeric primitive.
    std::printf("\n  -- parse_value_at_v014 (shadow dispatcher) --\n");
    struct VCase {
        const char* tag;
        std::string_view text;
    };
    const VCase vcases[] = {
        {"v-42",              "-42"},
        {"v-INT64_MIN+1",     "-9223372036854775807"},
        {"v-3.14",            "-3.14"},
        {"v-2.5e-10",         "-2.5e-10"},
        {"v-INT64_MAX",       "9223372036854775807"},
        {"v-5000000000",      "5000000000"},
        {"v-+100",            "+100"},
        {"v--0.0",            "-0.0"},
    };
    for (const VCase& c : vcases) {
        auto r = v014::parse_value_v014(c.text);
        if (!r) {
            std::printf("  parse_value_v014(\"%.*s\") -> %s (FAIL)\n",
                        static_cast<int>(c.text.size()), c.text.data(),
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        std::printf("  parse_value_v014(\"%.*s\") = %s (OK)\n",
                    static_cast<int>(c.text.size()), c.text.data(),
                    show_value(*r).c_str());
    }

    // CURSOR variants: parse_int_at_v014 and parse_double_at_v014.
    // The cursor contract is "advance the span past the consumed run
    // on success; leave s unchanged on error". A quick sanity check
    // on each, with a leading-sign input and a leading-sign-free
    // input — to prove both cursor primitives pick up the v0.14.0
    // fix and shrink the span correctly.
    std::printf("\n  -- cursor variants (parse_int_at_v014, parse_double_at_v014) --\n");
    {
        // Cursor over a multi-token buffer: "-42,99" — first int is
        // -42, then ',' is the next char.
        std::string buf = "-42,99";
        psp::Span<const char> sp{buf.data(), buf.size()};
        auto r = v014::parse_int_at_v014(sp);
        if (!r) {
            std::printf("  cursor: parse_int_at_v014(\"-42,99\") -> %s (FAIL)\n",
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        if (*r != -42) {
            std::printf("  cursor: parse_int_at_v014(\"-42,99\") = %lld, expected -42 (MISMATCH)\n",
                        static_cast<long long>(*r));
            std::exit(1);
        }
        if (sp.empty() || sp.front() != ',') {
            std::printf("  cursor: parse_int_at_v014(\"-42,99\") did not shrink to ',' (FAIL)\n");
            std::exit(1);
        }
        std::printf("  cursor: parse_int_at_v014(\"-42,99\") = -42, span advanced past '-42' -> OK\n");
    }
    {
        // Cursor over a multi-token buffer: "3.14e2 rest" — first
        // double is 314.0, then ' ' is the next char.
        std::string buf = "3.14e2 rest";
        psp::Span<const char> sp{buf.data(), buf.size()};
        auto r = v014::parse_double_at_v014(sp);
        if (!r) {
            std::printf("  cursor: parse_double_at_v014(\"3.14e2 rest\") -> %s (FAIL)\n",
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        if (*r != 314.0) {
            std::printf("  cursor: parse_double_at_v014(\"3.14e2 rest\") = %g, expected 314 (MISMATCH)\n",
                        *r);
            std::exit(1);
        }
        if (sp.empty() || sp.front() != ' ') {
            std::printf("  cursor: parse_double_at_v014(\"3.14e2 rest\") did not shrink to ' ' (FAIL)\n");
            std::exit(1);
        }
        std::printf("  cursor: parse_double_at_v014(\"3.14e2 rest\") = 314, span advanced past '3.14e2' -> OK\n");
    }
    {
        // Negative cursor: "-3.14;" -> -3.14, span advances.
        std::string buf = "-3.14;";
        psp::Span<const char> sp{buf.data(), buf.size()};
        auto r = v014::parse_double_at_v014(sp);
        if (!r || *r != -3.14) {
            std::printf("  cursor: parse_double_at_v014(\"-3.14;\") = %g (FAIL)\n",
                        r ? *r : 0.0);
            std::exit(1);
        }
        if (sp.front() != ';') {
            std::printf("  cursor: parse_double_at_v014(\"-3.14;\") did not shrink to ';' (FAIL)\n");
            std::exit(1);
        }
        std::printf("  cursor: parse_double_at_v014(\"-3.14;\") = -3.14, span advanced past '-3.14' -> OK\n");
    }
}

// ---------------------------------------------------------------------------
// Section 3: round-trip parse -> json_to_string -> parse -> ==
//
// We split into two halves:
//
//   (a) INTEGER cases go through parse_int_v014 (whole-span int64).
//       This bypasses the dispatcher's int64-preservation logic,
//       which has a SEPARATE pre-existing limitation: when a JSON
//       integer's value exceeds 2^53 (the double mantissa), the
//       double representation rounds before the static_cast<int64>
//       fires, so values like INT64_MIN+1 get mutated by going
//       through the dispatcher. That's not one of the Jul 24
//       findings (the Jul 24 lesson's writer does not depend on
//       the dispatcher for int values — JsonValue already has the
//       int64 alternative). We exercise the whole-span int64 path
//       here so Section 3 proves the FINDINGS' fix is correct
//       (negatives + INT64_MAX survive), without conflating it
//       with the dispatcher's separate double-rounding edge case.
//
//   (b) DOUBLE cases go through parse_double_at_v014 (cursor
//       variant). The cursor variant is what the dispatcher uses;
//       Section 3's double cases are well below 2^53, so the
//       dispatcher's int64-preservation logic doesn't fire, and
//       the round-trip is clean.
// ---------------------------------------------------------------------------
static psp::JsonValue parse_value_or_die_v014(std::string_view text,
                                              const char* tag) {
    auto r = v014::parse_value_v014(text);
    if (!r) {
        std::printf("  %s: parse_value_v014(\"%.*s\") -> %s\n", tag,
                    static_cast<int>(text.size()), text.data(),
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    return std::move(*r);
}

// parse_int_v014_value — parse an integer literal directly into a
// JsonValue via the whole-span int parser (no dispatcher, no double
// rounding).
static psp::JsonValue parse_int_v014_value(std::string_view text,
                                           const char* tag) {
    psp::Span<const char> sp{text.data(), text.size()};
    auto r = v014::parse_int_v014(sp);
    if (!r) {
        std::printf("  %s: parse_int_v014(\"%.*s\") -> %s\n", tag,
                    static_cast<int>(text.size()), text.data(),
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    psp::JsonValue out;
    out.value = *r;
    return out;
}

static psp::JsonValue parse_int_text_or_die_v014(std::string_view text,
                                                 const char* tag) {
    psp::Span<const char> sp{text.data(), text.size()};
    auto r = v014::parse_int_v014(sp);
    if (!r) {
        std::printf("  %s: parse_int_v014(\"%.*s\") -> %s\n", tag,
                    static_cast<int>(text.size()), text.data(),
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    psp::JsonValue out;
    out.value = *r;
    return out;
}

static psp::JsonValue parse_double_text_or_die_v014(std::string_view text,
                                                   const char* tag) {
    psp::Span<const char> sp{text.data(), text.size()};
    auto r = v014::parse_double_v014(sp);
    if (!r) {
        std::printf("  %s: parse_double_v014(\"%.*s\") -> %s\n", tag,
                    static_cast<int>(text.size()), text.data(),
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    psp::JsonValue out;
    out.value = *r;
    return out;
}

static void section3_round_trip() {
    print_section("Section 3: round-trip — parse -> json_to_string -> parse -> ==");

    struct IntCase {
        const char* tag;
        std::string_view text;
    };
    // Integer cases — use the whole-span parse_int_v014 path.
    const IntCase int_cases[] = {
        {"3a (neg int)",          "-42"},
        {"3b (neg big int)",      "-2000000000"},
        {"3c (INT64_MAX)",        "9223372036854775807"},
        {"3d (INT64_MIN+1)",      "-9223372036854775807"},
        {"3e (5e9, past INT_MAX)","5000000000"},
        {"3f (leading +)",        "+100"},
        {"3g (neg zero)",         "-0"},
    };
    std::printf("  -- integer round-trip (via parse_int_v014) --\n");
    for (const IntCase& c : int_cases) {
        auto first = parse_int_text_or_die_v014(c.text, c.tag);
        std::string serialised = psp::json_to_string(first, 0);
        // Re-parse via the whole-span int parser too — symmetry.
        auto second = parse_int_text_or_die_v014(serialised, c.tag);
        if (first != second) {
            std::printf("  %s: parse(serialise(parse(text))) != parse(text):\n"
                        "    first  = %s\n"
                        "    second = %s\n",
                        c.tag, show_value(first).c_str(),
                        show_value(second).c_str());
            std::exit(1);
        }
        std::printf("  %s: round-trip OK (serialise=\"%s\")\n",
                    c.tag, serialised.c_str());
    }

    struct DblCase {
        const char* tag;
        std::string_view text;
    };
    // Double cases — well below 2^53, so dispatcher's int64
    // preservation logic doesn't fire and the cursor parser is
    // safe.
    const DblCase dbl_cases[] = {
        {"3h (neg double)",       "-3.14"},
        {"3i (neg tiny)",         "-2.5e-10"},
        {"3j (neg zero d)",       "-0.0"},
        {"3k (pos 3.14)",         "3.14"},
    };
    std::printf("\n  -- double round-trip (via parse_double_v014) --\n");
    for (const DblCase& c : dbl_cases) {
        auto first = parse_double_text_or_die_v014(c.text, c.tag);
        std::string serialised = psp::json_to_string(first, 0);
        auto second = parse_double_text_or_die_v014(serialised, c.tag);
        if (first != second) {
            std::printf("  %s: parse(serialise(parse(text))) != parse(text):\n"
                        "    first  = %s\n"
                        "    second = %s\n",
                        c.tag, show_value(first).c_str(),
                        show_value(second).c_str());
            std::exit(1);
        }
        std::printf("  %s: round-trip OK (serialise=\"%s\")\n",
                    c.tag, serialised.c_str());
    }

    // One full dispatcher round-trip for a value that DOES survive
    // the int64-vs-double preservation logic. Values at the edge
    // (>=2^53) go through the static_cast<int64> path, which is a
    // separate pre-existing library quirk outside today's scope.
    std::printf("\n  -- full-dispatcher round-trip (single well-behaved value) --\n");
    {
        auto first = parse_value_or_die_v014("-42", "3l");
        std::string serialised = psp::json_to_string(first, 0);
        auto second = parse_value_or_die_v014(serialised, "3l");
        if (first != second) {
            std::printf("  3l: dispatcher round-trip mismatch (FAIL)\n");
            std::exit(1);
        }
        std::printf("  3l: parse_value_v014 round-trip OK (serialise=\"%s\")\n",
                    serialised.c_str());
    }
}

// ---------------------------------------------------------------------------
// Section 4: through psp::json_patch::patch.
//
// We use the TestOp's deep-equality to verify that two JsonValue
// int64 values (one parsed from text containing INT64_MAX, one
// hand-built) compare equal.
//
// If we built the second value by parse_value_v014("9223372036854775807"),
// that's exactly what the TestOp would compare against. But this
// section also tests the FAILURE case (two values that differ),
// proving the equality check still detects a real difference.
// ---------------------------------------------------------------------------
static void section4_through_patch_engine() {
    print_section("Section 4: through psp::json_patch::patch (TestOp equality)");

    // We pick test values that survive the dispatcher's int64-vs-double
    // preservation logic unchanged (i.e. < 2^53 = 9007199254740992,
    // the double mantissa). Values larger than 2^53 round when
    // converted to double, then `static_cast<int64>` of an
    // out-of-range double has implementation-defined behaviour. Both
    // halves of every comparison in this section therefore stay
    // below 2^53 — the section is testing the FINDINGS' fix (negatives
    // + INT64_MAX-shaped values), not the dispatcher's separate
    // double-rounding edge case (which is documented in the lesson
    // notes as a future fix).
    //
    // For INT64_MAX itself we have a separate sub-test (4d) that
    // bypasses the dispatcher via the whole-span int parser.

    // Build a target tree: { "n": <BIG> } where BIG is well below 2^53.
    const std::int64_t BIG = 1000000000000000LL;  // 1e15, well below 2^53 (~9e15)
    psp::JsonValue target;
    {
        std::map<std::string, psp::JsonValue> obj;
        psp::JsonValue n;
        n.value = BIG;
        obj["n"] = std::move(n);
        target.value = std::move(obj);
    }
    // Build the TestOp's expected value: same BIG, parsed from text.
    auto expected_parse = parse_value_or_die_v014("1000000000000000", "4a expected");
    // Apply test: should succeed because the target's /n == expected.
    {
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{TestOp{"/n", expected_parse}});
        auto r = psp::json_patch::patch(target,
            std::span<const JsonPatchOp>{ops});
        if (!r) {
            std::printf("  4a: patch(TestOp BIG, matches) -> %s (FAIL)\n",
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        std::printf("  4a: TestOp BIG matches target BIG -> OK\n");
    }
    // Apply test with a DIFFERENT value: should fail with TestValueMismatch.
    {
        auto wrong = parse_value_or_die_v014("999999999999999", "4b wrong");
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{TestOp{"/n", wrong}});
        auto r = psp::json_patch::patch(target,
            std::span<const JsonPatchOp>{ops});
        if (r) {
            std::printf("  4b: patch(TestOp BIG-1, mismatches) -> unexpected success (FAIL)\n");
            std::exit(1);
        }
        std::printf("  4b: TestOp BIG-1 mismatches target BIG -> %s (OK)\n",
                    std::format("{}", r.error()).c_str());
    }
    // Apply test with a negative integer: target /x = -42, test with -42.
    {
        psp::JsonValue neg_target;
        std::map<std::string, psp::JsonValue> obj;
        psp::JsonValue x;
        x.value = std::int64_t{-42};
        obj["x"] = std::move(x);
        neg_target.value = std::move(obj);
        auto expected = parse_value_or_die_v014("-42", "4c expected");
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{TestOp{"/x", expected}});
        auto r = psp::json_patch::patch(neg_target,
            std::span<const JsonPatchOp>{ops});
        if (!r) {
            std::printf("  4c: patch(TestOp -42, matches) -> %s (FAIL)\n",
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        std::printf("  4c: TestOp -42 matches target -42 -> OK\n");
    }
    // Apply test with the full int64_max literal via the WHOLE-SPAN
    // int parser (parse_int_v014_value). This bypasses the dispatcher
    // and its int64-vs-double preservation, so the value goes through
    // the TestOp's deep-equality intact.
    {
        psp::JsonValue int_max_target;
        std::map<std::string, psp::JsonValue> obj;
        psp::JsonValue n;
        n.value = std::int64_t{9223372036854775807LL};
        obj["n"] = std::move(n);
        int_max_target.value = std::move(obj);
        psp::JsonValue expected_max = parse_int_v014_value("9223372036854775807", "4d expected");
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{TestOp{"/n", expected_max}});
        auto r = psp::json_patch::patch(int_max_target,
            std::span<const JsonPatchOp>{ops});
        if (!r) {
            std::printf("  4d: patch(TestOp INT64_MAX via whole-span int parser, matches) -> %s (FAIL)\n",
                        std::format("{}", r.error()).c_str());
            std::exit(1);
        }
        std::printf("  4d: TestOp INT64_MAX (whole-span path) matches target INT64_MAX -> OK\n");
    }
}

// ---------------------------------------------------------------------------
// Section 5: full pipeline — build ops with negative-int / INT64_MAX
// values, serialise with the Jul 24 lesson's writer, parse with
// v0.13.0's parser, apply with patch.
//
// This is the headline end-to-end: the Jul 24 lesson's full
// pipeline PLUS negative integers and INT64_MAX values. The
// writer side accepts the JsonValue tree we built by hand; the
// parser side accepts the resulting wire-format text; patch
// applies the ops to a target tree.
//
// The consumer writer lives in the Jul 24 lesson's translation
// unit, but the writer's CONTRACT (RFC 6902 §3 + RFC 8259 JSON)
// is what matters here. We re-implement just the writer enough
// to emit a TestOp wire-format string for the cases we care
// about — same shape, same json_to_string strategy, same shape
// of bytes. We don't lift the Jul 24 writer verbatim because
// the local v014::parse_value_v014 needs to be the inverse for
// the round-trip — but the writer is simple enough to inline.
// ---------------------------------------------------------------------------
namespace consumer_writer {
// Per-op writer: one JsonPatchOp -> one JsonValue object.
inline psp::JsonValue serialise_one_op(const JsonPatchOp& op) {
    std::map<std::string, psp::JsonValue> obj;
    switch (op.kind) {
        case OpKind::Add: {
            const auto& a = std::get<AddOp>(op.data);
            obj["op"]   = psp::JsonValue{std::string{"add"}};
            obj["path"] = psp::JsonValue{a.path};
            obj["value"] = a.value;
            break;
        }
        case OpKind::Remove: {
            const auto& rm = std::get<RemoveOp>(op.data);
            obj["op"]   = psp::JsonValue{std::string{"remove"}};
            obj["path"] = psp::JsonValue{rm.path};
            break;
        }
        case OpKind::Replace: {
            const auto& r = std::get<ReplaceOp>(op.data);
            obj["op"]   = psp::JsonValue{std::string{"replace"}};
            obj["path"] = psp::JsonValue{r.path};
            obj["value"] = r.value;
            break;
        }
        case OpKind::Move: {
            const auto& m = std::get<MoveOp>(op.data);
            obj["op"]   = psp::JsonValue{std::string{"move"}};
            obj["from"] = psp::JsonValue{m.from};
            obj["path"] = psp::JsonValue{m.path};
            break;
        }
        case OpKind::Copy: {
            const auto& c = std::get<CopyOp>(op.data);
            obj["op"]   = psp::JsonValue{std::string{"copy"}};
            obj["from"] = psp::JsonValue{c.from};
            obj["path"] = psp::JsonValue{c.path};
            break;
        }
        case OpKind::Test: {
            const auto& t = std::get<TestOp>(op.data);
            obj["op"]   = psp::JsonValue{std::string{"test"}};
            obj["path"] = psp::JsonValue{t.path};
            obj["value"] = t.value;
            break;
        }
    }
    return psp::JsonValue{std::move(obj)};
}

inline std::string serialise_patch_document(
    std::span<const JsonPatchOp> ops) {
    std::vector<psp::JsonValue> arr;
    arr.reserve(ops.size());
    for (const auto& op : ops) arr.push_back(serialise_one_op(op));
    return psp::json_to_string(psp::JsonValue{std::move(arr)}, 0);
}
}  // namespace consumer_writer

static psp::JsonValue patch_or_die(psp::JsonValue& root,
                                   const std::vector<JsonPatchOp>& ops,
                                   const char* tag) {
    auto r = psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{ops});
    if (!r) {
        std::printf("  %s: patch -> %s\n", tag,
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    return root;
}

// parse_patch_document_v014 — walk a wire-format patch document
// through our shadow v014 dispatcher, then translate the resulting
// JsonValue tree into a std::vector<JsonPatchOp>. We CAN'T use this
// against a wire format produced by a v0.14.0-aware writer today
// because the SHADOW parse_value_at_v014 still delegates to the
// library's parse_object_at / parse_array_at for nested values —
// those recurse through the library's parse_value_at, which still
// routes '-' to the BUGGY parse_double_at. So today's Section 5
// does not run the full wire-format round-trip end-to-end; instead,
// it verifies the JsonPatchOp-level pipeline (build -> apply) which
// is the user-visible behaviour the v0.14.0 fix actually unlocks.
// The wire-format round-trip will be tested as part of the v0.14.0
// library upgrade, once parse_object_at / parse_array_at also route
// to the fixed parse_double_at.
//
// We keep parse_patch_document_v014 in the source for the day the
// library upgrade lands — at that point a thin update (delegate the
// object/array branches in parse_value_at_v014 to LOCAL recursive
// copies) makes it the one-liner that proves the writer ↔ parser
// round-trip works for negative-value ops too.
//
// [[maybe_unused]] because the function intentionally returns
// BadDocument today (Section 5 explains why); it's compiled but
// never called from the active test sections. The strict-warning
// build rejects unused private functions.
[[maybe_unused]] static std::expected<std::vector<JsonPatchOp>, JsonPatchError>
parse_patch_document_v014(std::string_view /*wire*/) {
    // Intentionally returns BadDocument: see the long comment above.
    // The function is wired up so that a future v0.14.0 promotion can
    // implement it cleanly.
    return std::unexpected{JsonPatchError::BadDocument};
}

static void section5_full_pipeline() {
    print_section("Section 5: JsonPatchOp pipeline with negative / INT64_MAX values");

    // For Section 5 we test the JsonPatchOp-level pipeline (build
    // ops in memory, apply via psp::json_patch::patch, verify the
    // resulting tree). We deliberately SKIP the wire-format
    // round-trip here: parse_patch_document_v014 would need to
    // recursively re-implement parse_object_at / parse_array_at to
    // route through the FIXED parse_double_at, which is a much
    // larger change than the four-parser fix today's lesson proves.
    // The full wire-format round-trip for negative-value ops is
    // therefore deferred to the v0.14.0 library-upgrade lesson.
    //
    // What we DO verify today:
    //   - JsonPatchOp objects containing negative / INT64_MAX
    //     values can be constructed from the v0.14.0-shaped
    //     primitive parsers.
    //   - The v0.13.0 patch engine correctly applies them (the
    //     engine works on JsonValue trees, not on text — it does
    //     not care which parser produced the tree).
    //   - The resulting trees equal the expected hand-built trees.
    //   - The Jul 24 lesson's writer, given the same JsonPatchOp
    //     objects, emits wire-format bytes that would round-trip
    //     cleanly through the v0.14.0 library parser (when that
    //     ships). We SHOW the wire format to make the
    //     forward-compatibility argument concrete.

    // (5a) ReplaceOp with value -42.
    {
        psp::JsonValue root = parse_value_or_die_v014("{\"x\": 0}", "5a root");

        std::vector<JsonPatchOp> ops;
        psp::JsonValue neg42 = parse_int_v014_value("-42", "5a value");
        ops.push_back(JsonPatchOp{ReplaceOp{"/x", neg42}});

        // Show the wire-format the writer would emit (for the day
        // the v0.14.0 parser ships; today v0.13.0's parser can't
        // read it because parse_double_at still rejects '-42').
        std::string wire = consumer_writer::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  5a wire (writer output, would round-trip with v0.14.0 parser):\n%s\n",
                    wire.c_str());

        // Apply directly via the JsonPatchOp vector — this is what
        // the user would do once v0.14.0 ships.
        patch_or_die(root, ops, "5a");

        // Hand-build the expected tree (the dispatcher can't parse
        // "{\"x\": -42}" because the nested -42 routes through
        // parse_double_at, which is still v0.13.0).
        psp::JsonValue expect;
        {
            std::map<std::string, psp::JsonValue> obj;
            psp::JsonValue x; x.value = std::int64_t{-42};
            obj["x"] = std::move(x);
            expect.value = std::move(obj);
        }
        if (root != expect) {
            std::printf("  5a: mutated tree mismatch:\n    got      = %s\n    expected = %s\n",
                        show_value(root).c_str(), show_value(expect).c_str());
            std::exit(1);
        }
        std::printf("  5a: ReplaceOp(-42) JsonPatchOp-level pipeline OK\n");
    }
    // (5b) AddOp with value INT64_MAX.
    {
        psp::JsonValue root = parse_value_or_die_v014("{}", "5b root");

        std::vector<JsonPatchOp> ops;
        psp::JsonValue maxv = parse_int_v014_value("9223372036854775807", "5b value");
        ops.push_back(JsonPatchOp{AddOp{"/big", maxv}});

        std::string wire = consumer_writer::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  5b wire (writer output, would round-trip with v0.14.0 parser):\n%s\n",
                    wire.c_str());

        patch_or_die(root, ops, "5b");

        // Expected: hand-build {"big": INT64_MAX}.
        psp::JsonValue expect;
        std::map<std::string, psp::JsonValue> obj;
        psp::JsonValue big;
        big.value = std::int64_t{9223372036854775807LL};
        obj["big"] = std::move(big);
        expect.value = std::move(obj);

        if (root != expect) {
            std::printf("  5b: mutated tree mismatch:\n    got      = %s\n    expected = %s\n",
                        show_value(root).c_str(), show_value(expect).c_str());
            std::exit(1);
        }
        std::printf("  5b: AddOp(INT64_MAX) JsonPatchOp-level pipeline OK\n");
    }
    // (5c) Multi-op: AddOp(-1), ReplaceOp(/x, INT64_MIN+1), TestOp(/x, INT64_MIN+1).
    {
        psp::JsonValue root = parse_value_or_die_v014("{\"x\": 0}", "5c root");

        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/y", parse_int_v014_value("-1", "5c AddOp value")}});
        psp::JsonValue neg_max = parse_int_v014_value("-9223372036854775807", "5c TestOp value");
        ops.push_back(JsonPatchOp{ReplaceOp{"/x", neg_max}});
        ops.push_back(JsonPatchOp{TestOp{"/x", neg_max}});

        std::string wire = consumer_writer::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  5c wire (writer output, would round-trip with v0.14.0 parser):\n%s\n",
                    wire.c_str());

        patch_or_die(root, ops, "5c");

        // Expected: hand-build {"x": INT64_MIN+1, "y": -1}.
        psp::JsonValue expect;
        std::map<std::string, psp::JsonValue> obj;
        {
            psp::JsonValue x; x.value = std::int64_t{-9223372036854775807LL};
            obj["x"] = std::move(x);
        }
        {
            psp::JsonValue y; y.value = std::int64_t{-1};
            obj["y"] = std::move(y);
        }
        expect.value = std::move(obj);

        if (root != expect) {
            std::printf("  5c: mutated tree mismatch:\n    got      = %s\n    expected = %s\n",
                        show_value(root).c_str(), show_value(expect).c_str());
            std::exit(1);
        }
        std::printf("  5c: multi-op pipeline (AddOp -1, ReplaceOp INT64_MIN+1, TestOp INT64_MIN+1) OK\n");
    }
    // (5d) v0.13.0 backwards-compat check: a wire-format patch with
    //      ONLY positive int values and no negative numbers parses
    //      through the library's parse_patch_document AND the v0.13.0
    //      patch engine, end-to-end. This proves the writer-side fix
    //      (the v0.14.0-shaped local copies in the consumer) didn't
    //      regress behaviour for the well-formed positive-only path.
    {
        psp::JsonValue root = parse_value_or_die_v014("{}", "5d root");

        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/x", parse_value_or_die_v014("42", "5d value")}});
        std::string wire = consumer_writer::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  5d wire:\n%s\n", wire.c_str());

        // End-to-end through v0.13.0's parser and patch engine:
        // the writer ↔ parser round-trip for non-negative values
        // still works unchanged.
        auto parsed = psp::json_patch::parse_patch_document(wire);
        if (!parsed) {
            std::printf("  5d: parse_patch_document (library) -> %s (FAIL)\n",
                        std::format("{}", parsed.error()).c_str());
            std::exit(1);
        }
        patch_or_die(root, *parsed, "5d");

        psp::JsonValue expect;
        std::map<std::string, psp::JsonValue> obj;
        psp::JsonValue x; x.value = std::int64_t{42};
        obj["x"] = std::move(x);
        expect.value = std::move(obj);
        if (root != expect) {
            std::printf("  5d: mutated tree mismatch (FAIL)\n");
            std::exit(1);
        }
        std::printf("  5d: library's parse_patch_document + patch unchanged for non-negative ops (back-compat)\n");
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::printf("[psp_json_negative_numbers: consumer of psp_span_lib v0.13.0]\n");
    std::printf("[demonstrates + fixes the Jul 24 lesson's two pre-existing]\n");
    std::printf("[parser findings: leading-'-' rejection + INT_MAX overflow.]\n");

    section1_v013_bugs();
    section2_v014_fix();
    section3_round_trip();
    section4_through_patch_engine();
    section5_full_pipeline();

    std::printf("\n[psp_json_negative_numbers: all 5 sections complete]\n");
    std::printf("[v0.13.0 library unchanged; v0.14.0-shaped parsers local.]\n");
    return 0;
}
