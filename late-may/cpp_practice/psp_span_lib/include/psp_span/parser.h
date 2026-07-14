// psp_span/parser.h — small numeric parsers built on top of psp::Span.
//
// This header is the *second* public header of `psp_span_lib` (the first
// being `<psp_span/span.h>`). It adds:
//
//   - enum class psp::ParseError    (the typed failure payload).
//   - std::expected<int, psp::ParseError>      psp::parse_int(Span<const char>).
//   - std::expected<double, psp::ParseError>   psp::parse_double(Span<const char>).
//   - std::formatter<psp::ParseError>          (so std::format / std::println work).
//
// The header is C++23 because it leans on std::expected (P0323R12,
// final in C++23, feature-test __cpp_lib_expected == 202211). The
// library itself is C++17 (psp::Span is C++17-compatible), so the
// consumer's compile standard is determined by which headers they
// include: including <psp_span/span.h> alone is C++17; including
// <psp_span/parser.h> requires C++23.
//
// This header is the natural forward-on from the Jul 13 lesson
// (P-2026-07-13-psp-parser-expected.cpp), which proved the *shape*
// (psp::Span<const char> -> std::expected<int, ParseError>) on a
// standalone consumer. Today that shape becomes part of the library
// proper: a downstream consumer that does `target_link_libraries(...
// psp_span_lib)` and `#include <psp_span/parser.h>` gets both parsers
// and the error formatting for free.
//
// What's intentionally NOT in this header:
//   - A JSON parser. Too much scope creep; the design is "tiny numeric
//     parsers that compose with the span layer".
//   - A stream-style parse_int_at(psp::Span<const char>&) that
//     advances a cursor. The Jul 13 lesson's parse_int consumes the
//     whole span; that's the right granularity for a first library
//     release. A streaming variant is a 1-line change when needed.
//   - The `parse_*_or` family (e.g. parse_int_or(s, default_value)).
//     std::expected has .value_or() already; consumers that want a
//     default can compose it themselves with .transform_error() or
//     .value_or().

#ifndef PSP_SPAN_PARSER_H_INCLUDED
#define PSP_SPAN_PARSER_H_INCLUDED

#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <string_view>

// ---------------------------------------------------------------------------
// IMPORTANT — file-scope namespace choice:
//
// The formatter specialization below MUST live in namespace std (because
// that's where std::formatter is declared). The C++ standard (and
// [namespace.std]) allows user specializations of std::formatter for
// user-defined types, BUT ONLY AT NAMESPACE SCOPE of the enclosing
// namespace — not inside a nested namespace like psp::.
//
// That means the enum ParseError also has to live at file scope (NOT
// inside `psp`), otherwise the formatter specialization would have to
// reference psp::ParseError from inside namespace std, which is a
// different kind of error.
//
// So:
//   - enum class ParseError              (file scope, not in any namespace)
//   - std::formatter<ParseError>         (specialization at namespace std)
//   - psp::parse_int(Span<const char>)   (the parser function is in psp::)
//   - psp::parse_double(Span<const char>)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// ParseError — the typed failure payload.
//
// Each enumerator is a distinct parse-failure mode. Using a typed
// enum (vs. std::error_code or just `int`) means:
//   - Comparisons and switches on the error are exhaustive-friendly.
//   - sizeof(ParseError) is 4 (the size of `int` — the default
//     underlying type for enum class on this toolchain).
//   - The C++23 std::expected<int, ParseError> is 8 bytes (matches
//     the Jul 12 measurement), so the whole result type fits in a
//     register.
// ---------------------------------------------------------------------------
enum class ParseError {
    Empty,           // input was zero-length
    LeadingSign,     // input begins with '+' or '-'
    NotADigit,       // input contains a non-digit (and non-decimal-point / non-'e' / non-'E' for parse_double)
    Overflow,        // accumulator would exceed the target type's range
    BadExponent,     // parse_double only: 'e'/'E' with no following digits
    MissingFraction, // parse_double only: '.' with no following digits
};

// ---------------------------------------------------------------------------
// std::formatter<ParseError> — so std::format("{}\n", err) works.
//
// User specializations of std::formatter MUST be declared in namespace
// std (the namespace of the primary template). Inheriting
// std::formatter<std::string_view> gets us the standard width / fill /
// alignment machinery; only the `format` member is overridden to map
// the enum to its name.
// ---------------------------------------------------------------------------
template <>
struct std::formatter<ParseError> : std::formatter<std::string_view> {
    auto format(ParseError e, std::format_context& ctx) const {
        std::string_view name = "?";
        switch (e) {
            case ParseError::Empty:           name = "Empty";           break;
            case ParseError::LeadingSign:     name = "LeadingSign";     break;
            case ParseError::NotADigit:       name = "NotADigit";       break;
            case ParseError::Overflow:        name = "Overflow";        break;
            case ParseError::BadExponent:     name = "BadExponent";     break;
            case ParseError::MissingFraction: name = "MissingFraction"; break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

namespace psp {

// ---------------------------------------------------------------------------
// parse_int — parse a non-empty sequence of decimal digits.
//
// Failure modes:
//   - Empty           -> Span is empty.
//   - LeadingSign     -> First char is '+' or '-'. We refuse both
//                        (sign-handling is a downstream policy choice,
//                        not the parser's job; rejecting them forces the
//                        caller to handle signs explicitly).
//   - NotADigit       -> Any char outside '0'..'9'.
//   - Overflow        -> Accumulator would exceed std::numeric_limits<int>::max().
//
// noexcept: every failure becomes an std::expected-returned error;
// the function never throws, so consumers can rely on it for the
// move_if_noexcept / strong-guarantee decisions (Jun 12 lesson).
// ---------------------------------------------------------------------------
inline std::expected<int, ParseError>
parse_int(Span<const char> s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() == '+' || s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }

    std::int64_t acc = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return std::unexpected{ParseError::NotADigit};
        }
        acc = acc * 10 + (c - '0');
        if (acc > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            return std::unexpected{ParseError::Overflow};
        }
    }
    return static_cast<int>(acc);
}

// ---------------------------------------------------------------------------
// parse_double — parse a double-precision float from a sequence of
// decimal digits, optionally containing a single '.' and an optional
// exponent ('e' or 'E' followed by decimal digits).
//
// Recognised shape (subset of strtod):
//
//   [-+]? DIGITS ( '.' DIGITS )? ( [eE] [-+]? DIGITS )?
//
// Examples that parse successfully:
//   "0", "1", "42", "3.14", ".5", "1.", "1e10", "1.5E-3", "0.0001"
//
// Examples that fail with a typed error:
//   ""           -> Empty
//   "+1"         -> LeadingSign   (we reject leading signs the same way parse_int does)
//   "1.2.3"      -> NotADigit     (a second '.' is not a digit)
//   "1e"         -> BadExponent
//   "1."         -> success (the '.' is optional-fraction here)
//   "."          -> MissingFraction ('.' alone has no digits on either side)
//   "999...e999" -> Overflow (caught at the int accumulation steps)
//
// Implementation notes:
//   - The integer and fractional parts are accumulated as int64 (so
//     we can range-check both without losing precision). The final
//     value is composed as int_part + frac_part / 10^k, with the
//     shift done by a tight power-of-10 loop.
//   - We do NOT implement full strtod semantics (no NaN, no Inf, no
//     hex floats, no locale-specific '.' vs ','). The point of the
//     header is the SHAPE, not full libc coverage.
//   - noexcept for the same reason parse_int is.
// ---------------------------------------------------------------------------
inline std::expected<double, ParseError>
parse_double(Span<const char> s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() == '+' || s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }

    // Phase 1: integer part (zero or more digits). Stops at '.' / 'e' / end.
    std::size_t i = 0;
    std::int64_t int_part = 0;
    bool        any_int_digits = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        int_part = int_part * 10 + (s[i] - '0');
        if (int_part >
            static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            return std::unexpected{ParseError::Overflow};
        }
        ++i;
        any_int_digits = true;
    }

    // Phase 2: optional fractional part (zero or more digits after '.').
    std::int64_t frac_part = 0;
    int          frac_digits = 0;
    if (i < s.size() && s[i] == '.') {
        ++i;
        bool any_frac_digits = false;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            frac_part = frac_part * 10 + (s[i] - '0');
            ++i;
            ++frac_digits;
            any_frac_digits = true;
        }
        // ".x" with no fractional digits is OK (e.g. "1." -> 1.0).
        // "." alone (no integer and no fractional digits) is MissingFraction.
        if (!any_int_digits && !any_frac_digits) {
            return std::unexpected{ParseError::MissingFraction};
        }
    } else if (!any_int_digits) {
        // No integer digits and no '.' either — nothing usable.
        // (Note: a string of just "e" or "E" falls into this path; the
        // exponent-handler below will catch it as BadExponent.)
        // We return NotADigit here rather than MissingFraction because
        // there's no decimal point at all.
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            // fall through to exponent phase which will report BadExponent.
        } else {
            return std::unexpected{ParseError::NotADigit};
        }
    }

    // Phase 3: optional exponent ('e' or 'E', optional '+'/'-', digits).
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
            exp_part = exp_part * 10 + (s[i] - '0');
            ++i;
            any_exp_digits = true;
        }
        if (!any_exp_digits) {
            return std::unexpected{ParseError::BadExponent};
        }
    }

    // Phase 4: trailing garbage? We consumed everything in s but might
    // have stopped early — anything left is an error.
    if (i != s.size()) {
        return std::unexpected{ParseError::NotADigit};
    }

    // Phase 5: compose the value. We shift the fractional part by
    // 10^frac_digits; the exponent adjusts the overall magnitude.
    //
    // value = (int_part + frac_part / 10^frac_digits) * 10^(exp_sign * exp_part)
    //
    // Rather than reaching for std::pow (which is a runtime math call
    // and not necessarily noexcept-stable across toolchains), we use a
    // tight power-of-10 multiplication in a small loop. The shift is
    // tiny for reasonable exponents (≤18 to stay in double precision
    // without overflow).
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

    return value;
}

}  // namespace psp

#endif  // PSP_SPAN_PARSER_H_INCLUDED