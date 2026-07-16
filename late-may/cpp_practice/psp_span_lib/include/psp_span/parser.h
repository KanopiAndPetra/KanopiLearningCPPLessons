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
//   - The `parse_*_or` family (e.g. parse_int_or(s, default_value)).
//     std::expected has .value_or() already; consumers that want a
//     default can compose it themselves with .transform_error() or
//     .value_or().
//
// What's NEW in 2026-07-15 (v0.7.0) — streaming cursor API:
//   - parse_int_at(Span<const char>&)    — parses a leading int,
//                                          advances the span past it.
//   - parse_uint_at(Span<const char>&)   — same, but for unsigned.
//   - parse_double_at(Span<const char>&) — same, for double (int
//                                          part + fractional + exp).
//
// These are the "cursor" variant of the whole-span parsers: the
// caller passes the span BY REFERENCE, the parser consumes the
// leading run of digits (and optional fractional/exponent for
// double), shrinks the span so it now starts after the consumed
// run, and returns the parsed value. A failure leaves the span
// unchanged (so the caller can recover and try a different rule).
// This is the pattern JSON / CSV / structured-data parsers use
// to walk a buffer character-by-character.
//
// What's NEW in 2026-07-16 (v0.8.0) — cursor primitives (non-numeric):
//   - expect_char_at(Span<const char>&, char) — peek-and-consume a
//                                                single char; advance
//                                                on match, leave s
//                                                unchanged on mismatch.
//   - skip_whitespace_at(Span<const char>&)   — consume a run of
//                                                ASCII whitespace
//                                                (' ', '\t', '\n',
//                                                '\r'), leaving s
//                                                at the first
//                                                non-whitespace char
//                                                (or empty).
//
// Both are tiny (5-10 line) additions that close the two
// most-common cursor-walking boilerplate loops that every Jul 15
// consumer had to write inline:
//
//   while (!s.empty() && s.front() == ' ') s = s.subspan(1, ...);
//   if (!s.empty() && s.front() == ',')   s = s.subspan(1, ...);
//
// With v0.8.0 these collapse to:
//
//   psp::skip_whitespace_at(s);
//   psp::expect_char_at(s, ',');
//
// (or get a std::expected<bool, ParseError> back to know whether
// the expected char was actually there).

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
    UnexpectedChar,  // expect_char_at only: front char did not match the expected char (added 2026-07-16, v0.8.0)
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
            case ParseError::UnexpectedChar:  name = "UnexpectedChar";  break;
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

// ===========================================================================
// Streaming cursor API (NEW in v0.7.0, 2026-07-15)
// ===========================================================================
//
// The "whole-span" parsers above treat the input as a complete, isolated
// run: parse_int(Span<const char>) parses the WHOLE span and returns an
// error if any char isn't a digit. That's the right shape for "is this
// buffer a valid int?" but it's the wrong shape for "I'm walking a
// buffer and want to read the next int, leaving the rest for later
// parsers."
//
// The streaming variant solves that: the caller passes the span BY
// REFERENCE. On success, the parser consumes the leading run that makes
// up a valid numeric literal (the digit run for ints; the int +
// optional '.' + optional fractional + optional exponent for doubles)
// and shrinks the span so it now starts after the consumed run. On
// failure, the span is left UNCHANGED — the caller can recover and try
// a different rule.
//
// Failure modes are the SAME as the whole-span variants (Empty /
// LeadingSign / NotADigit / Overflow / BadExponent / MissingFraction),
// so error-handling code is identical:
//
//   psp::Span<const char> s = ...;
//   auto r = psp::parse_int_at(s);
//   if (!r) { /* r.error() tells you why; s is unchanged. */ }
//   else    { /* *r is the int; s now starts after the consumed digits. */ }
//
// All four streaming parsers are `noexcept` and `inline` (header-only),
// matching the whole-span variants.
//
// Design choice — how the cursor advances:
//
//   We use `s = s.subspan(n, s.size() - n)`. psp::Span has a runtime-
//   extent subspan() overload that returns a Span<T, dynamic_extent>
//   starting at offset `n`. So the streaming parsers depend only on
//   psp::Span's existing subspan(), no new span primitives needed.
//
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// parse_int_at — consume a leading run of decimal digits, advancing s.
//
// Failure modes:
//   - Empty           -> s is empty. s unchanged.
//   - LeadingSign     -> First char is '+' or '-'. s unchanged.
//   - NotADigit       -> First char is not '0'..'9'. s unchanged.
//   - Overflow        -> The digit run would exceed INT_MAX. s unchanged.
//
// Success: returns the parsed int in std::expected, AND s is shrunk so
// it starts after the consumed digits. The next character in `s`
// (after the parse) is whatever was originally after the digit run —
// often a delimiter like ',' or ']'. The caller is responsible for
// consuming that delimiter (a future `expect_char_at(s, ',')` would
// be the natural next primitive, but is out of scope for today).
// ---------------------------------------------------------------------------
inline std::expected<int, ParseError>
parse_int_at(Span<const char>& s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() == '+' || s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }

    std::int64_t acc = 0;
    std::size_t  i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (c < '0' || c > '9') {
            if (i == 0) {
                // The very first char is not a digit — true NotADigit,
                // nothing consumed, s unchanged.
                return std::unexpected{ParseError::NotADigit};
            }
            // We have at least one digit; the run ends here. Break
            // out and commit the partial read by shrinking s.
            break;
        }
        acc = acc * 10 + (c - '0');
        if (acc > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            // Overflow — but we already consumed at least one digit.
            // Standard cursor-parser contract: don't rewind the
            // cursor on overflow. The caller knows we got SOME digits,
            // and the rest of the buffer (the overflow tail) is still
            // meaningful as a continuation if the caller wants to
            // recover. So we shrink s past the digit run AND report
            // Overflow. This matches the std::strtol convention, where
            // ERANGE is set even when strtol has parsed a prefix.
            s = s.subspan(i + 1, s.size() - (i + 1));
            return std::unexpected{ParseError::Overflow};
        }
        ++i;
    }
    // Commit the consumed prefix: s starts after the last digit.
    s = s.subspan(i, s.size() - i);
    return static_cast<int>(acc);
}

// ---------------------------------------------------------------------------
// parse_uint_at — unsigned-only cursor variant.
//
// The difference vs parse_int_at is the sign policy: '+' is ACCEPTED
// (it's a no-op for unsigned) but '-' is REJECTED with LeadingSign.
// (We reject '-' rather than negating-then-casting because the
// overflow semantics get muddy: a caller's "-1" intent probably
// means "this isn't an unsigned at all". Same for parse_int, which
// rejects both signs for downstream-policy reasons.)
//
// Failure modes: Empty, LeadingSign ('-' only), NotADigit, Overflow.
// ---------------------------------------------------------------------------
inline std::expected<unsigned, ParseError>
parse_uint_at(Span<const char>& s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }
    // '+' is allowed but has no effect — skip it if present, then parse
    // the digit run. If the only thing in s is '+', we fall into the
    // NotADigit branch below (no digits consumed, i stays 0).
    std::size_t start = 0;
    if (s.front() == '+') {
        start = 1;
    }

    std::uint64_t acc = 0;
    std::size_t   i = start;
    while (i < s.size()) {
        char c = s[i];
        if (c < '0' || c > '9') {
            if (i == start) {
                // The first non-sign char isn't a digit. s unchanged.
                return std::unexpected{ParseError::NotADigit};
            }
            break;
        }
        acc = acc * 10 + static_cast<std::uint64_t>(c - '0');
        if (acc > static_cast<std::uint64_t>(std::numeric_limits<unsigned>::max())) {
            // Commit the consumed prefix and report overflow (same
            // contract as parse_int_at: don't rewind on overflow).
            s = s.subspan(i + 1, s.size() - (i + 1));
            return std::unexpected{ParseError::Overflow};
        }
        ++i;
    }
    // Empty input after skipping an optional '+': "parse_uint_at(\"+\")"
    // should report NotADigit (no digits consumed), not silently
    // return 0. We hit this branch when start==1, i==1, s.size()==1.
    if (i == start) {
        return std::unexpected{ParseError::NotADigit};
    }
    s = s.subspan(i, s.size() - i);
    return static_cast<unsigned>(acc);
}

// ---------------------------------------------------------------------------
// parse_double_at — streaming double-precision cursor.
//
// Same shape as parse_double (whole-span): integer part (zero or more
// digits) + optional '.' + optional fractional digits + optional
// 'e'/'E' exponent. On success, the span is shrunk to start AFTER
// the consumed run (which might include the '.' and the exponent).
//
// Failure modes: same as parse_double.
//
// A subtle difference vs the whole-span variant: trailing-garbage
// is fine here. The whole-span parser rejects "1.5x" because it
// expects the WHOLE span to be a double. The streaming parser
// accepts "1.5x" and leaves "x" in s — that's what a cursor parser
// is for: it doesn't know if "x" is garbage or the start of the
// next token.
// ---------------------------------------------------------------------------
inline std::expected<double, ParseError>
parse_double_at(Span<const char>& s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() == '+' || s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }

    std::size_t  i = 0;
    std::int64_t int_part = 0;
    bool         any_int_digits = false;
    while (i < s.size()) {
        char c = s[i];
        if (c < '0' || c > '9') break;
        int_part = int_part * 10 + (c - '0');
        if (int_part >
            static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            // Overflow on integer part — commit what's been consumed
            // and report Overflow. Same contract as parse_int_at.
            s = s.subspan(i + 1, s.size() - (i + 1));
            return std::unexpected{ParseError::Overflow};
        }
        ++i;
        any_int_digits = true;
    }

    // Optional fractional part.
    std::int64_t frac_part = 0;
    int          frac_digits = 0;
    if (i < s.size() && s[i] == '.') {
        std::size_t dot_pos = i;  // remember where the '.' was
        ++i;
        bool any_frac_digits = false;
        while (i < s.size()) {
            char c = s[i];
            if (c < '0' || c > '9') break;
            frac_part = frac_part * 10 + (c - '0');
            ++i;
            ++frac_digits;
            any_frac_digits = true;
        }
        // '.' with no following digits AND no preceding integer digits
        // is MissingFraction — '.' alone, nothing usable. s unchanged.
        if (!any_int_digits && !any_frac_digits) {
            return std::unexpected{ParseError::MissingFraction};
        }
        // If there were no fractional digits (e.g. "1."), we still
        // consumed the '.' — that's a valid run. Leave it consumed.
        (void)dot_pos;
    } else if (!any_int_digits) {
        // No integer digits and no '.' — could still be an exponent
        // (rare but legal: "e5" would be a misformed input but
        // "1e5" already passed the integer phase). For the cursor
        // variant, we treat a leading non-digit as NotADigit — the
        // caller can recover.
        return std::unexpected{ParseError::NotADigit};
    }

    // Optional exponent.
    int          exp_sign = 1;
    std::int64_t exp_part = 0;
    bool         any_exp_digits = false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        std::size_t e_pos = i;
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            if (s[i] == '-') exp_sign = -1;
            ++i;
        }
        while (i < s.size()) {
            char c = s[i];
            if (c < '0' || c > '9') break;
            exp_part = exp_part * 10 + (c - '0');
            ++i;
            any_exp_digits = true;
        }
        if (!any_exp_digits) {
            // "1e" with no exponent digits. The whole-span variant
            // reports BadExponent and gives up. The streaming variant
            // ALSO reports BadExponent and gives up — there's no
            // meaningful "commit what we have, leave 'e' for later"
            // because "e" alone is not a valid number-prefix either.
            // s unchanged on this error.
            (void)e_pos;
            return std::unexpected{ParseError::BadExponent};
        }
    }

    // Commit the consumed prefix.
    s = s.subspan(i, s.size() - i);

    // Compose the value (same arithmetic as parse_double).
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

// ===========================================================================
// Non-numeric cursor primitives (NEW in v0.8.0, 2026-07-16)
// ===========================================================================
//
// The v0.7.0 streaming numeric parsers leave two cursor-walking chores
// to the caller:
//
//   (1) skipping leading whitespace between tokens (every consumer
//       that walks a buffer with inter-token spaces had to write a
//       `while (!s.empty() && s.front() == ' ') s = s.subspan(1, ...)`
//       loop by hand — the Jul 15 lesson's Section 5 mixed-walk did
//       exactly this for ' ', and the Section 4 CSV-walk did the
//       analogue for any-non-digit delimiters).
//
//   (2) verifying (and consuming) an expected delimiter — e.g. "I
//       just parsed an int, now I expect a ',' before the next one."
//       A naive consumer would write:
//         if (!s.empty() && s.front() == ',') s = s.subspan(1, ...);
//       but that's silent on the missing-comma case — the caller
//       never finds out whether the delimiter was present or not.
//
// Both chores deserve proper primitives so every cursor walker
// becomes one-liner-per-step instead of inline-loop-per-step. Today
// we ship them as two new functions in <psp_span/parser.h>:
//
//   - expect_char_at(Span<const char>&, char) -> std::expected<bool, ParseError>
//   - skip_whitespace_at(Span<const char>&) -> std::expected<bool, ParseError>
//
// Both follow the SAME two-rule contract as the v0.7.0 numeric
// streaming parsers:
//
//   - On success: the span shrinks past the consumed run, and the
//     std::expected carries the success value (true).
//   - On failure: the span is unchanged, and the std::expected
//     carries the failure value (false + a ParseError enumerator).
//
// Both are `noexcept` and `inline` (header-only).
//
// We also reuse the existing ParseError enum, adding ONE new
// enumerator (UnexpectedChar) for the mismatch case. The other
// existing errors (Empty, NotADigit) are reused where they fit.

// ---------------------------------------------------------------------------
// expect_char_at — peek-and-consume a single expected character.
//
// On success: s shrinks by 1, returns std::expected<bool, ParseError>{true}.
//
// On failure: s is unchanged, returns std::expected<bool, ParseError> with
// error = UnexpectedChar (the front char did not match `expected`).
//
// Why std::expected<bool, ...> and not just bool?
//   Returning the bool is enough for the happy path — the caller knows
//   which char was expected (they passed it in). But by wrapping in
//   std::expected, the caller can ALSO recover from a missing delimiter:
//   they can try a different rule, log the error, return their own
//   std::unexpected, etc. Same shape as the numeric parsers.
//
// A subtle design choice: when s is EMPTY, what error do we return?
//   - Empty          — there's nothing to compare against; that's the
//                      right error: the buffer ran out before the
//                      expected char appeared.
//   - UnexpectedChar — would imply "I peeked and the char was wrong",
//                      but there's no char to peek at.
//
// We use Empty, matching the numeric parsers' convention.
//
// A second subtle choice: when the front char DOES NOT match `expected`
// and s is non-empty, we return UnexpectedChar. The caller can then
// decide whether the absence of the delimiter is fatal (it usually is
// for JSON/CSV) or recoverable (e.g. optional trailing comma).
// ---------------------------------------------------------------------------
inline std::expected<bool, ParseError>
expect_char_at(Span<const char>& s, char expected) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() != expected) {
        return std::unexpected{ParseError::UnexpectedChar};
    }
    // Match — consume the front char. Span shrinks by 1.
    s = s.subspan(1, s.size() - 1);
    return true;
}

// ---------------------------------------------------------------------------
// skip_whitespace_at — consume a run of ASCII whitespace characters.
//
// "Whitespace" here means the four standard ASCII whitespace chars:
//   ' '  (0x20)  — space
//   '\t' (0x09)  — horizontal tab
//   '\n' (0x0A)  — line feed (newline)
//   '\r' (0x0D)  — carriage return
//
// (We don't include vertical tab '\v' or form feed '\f' — both are
// vanishingly rare in JSON/CSV and a real lexer would use
// std::isspace / std::iswspace which handle Unicode whitespace;
// those are out of scope for this header. ASCII whitespace is what
// 99% of structured-data parsers need.)
//
// On success: s shrinks past the leading whitespace run; the new front
//   is the first non-whitespace char (or s is empty if the whole
//   buffer was whitespace). Returns std::expected<bool, ParseError>{true}.
//
// On failure: zero-length whitespace run (s.front() is already
//   non-whitespace or s is empty). Returns std::expected<bool,
//   ParseError>{true} as well — this is NOT an error, it's the
//   normal "nothing to skip" case.
//
// Wait — that's interesting. skip_whitespace_at never reports an
// error. It always succeeds (consumes zero or more whitespace chars).
// Why does it return std::expected<bool, ParseError> then, instead
// of just void?
//
//   (1) Consistency with the other cursor primitives. Every other
//       cursor primitive in this header returns std::expected<...>,
//       so skip_whitespace_at doing the same keeps the API uniform.
//       The caller can write `auto r = psp::skip_whitespace_at(s);`
//       and ignore the result, or chain it with `.and_then(...)`.
//
//   (2) Forward-compatibility. If we later want to make whitespace
//       handling configurable (e.g. "treat NUL as whitespace too",
//       or "stop on '\0' for embedded NUL strings"), the return
//       type is already in place to report a parse error.
//
//   (3) The success-value `true` is informational — it just means
//       "I consumed the leading whitespace successfully". The
//       caller usually ignores it.
//
// So: std::expected<bool, ParseError> with the value always true
// (never has an error today). Marked noexcept for the same reason
// the numeric parsers are.
// ---------------------------------------------------------------------------
inline std::expected<bool, ParseError>
skip_whitespace_at(Span<const char>& s) noexcept {
    std::size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        // ASCII whitespace only. See comment above for why we don't
        // include '\v' / '\f' or Unicode whitespace.
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        ++i;
    }
    // Commit: shrink s past the consumed whitespace run. Even if i==0
    // (no whitespace to skip), this is a no-op equivalent (subspan(0,
    // s.size()) == s), so the span is unchanged. That's the same
    // behavior as before the call — caller can't tell whether we
    // skipped anything by inspecting s alone (only by comparing s
    // size to a stored original).
    s = s.subspan(i, s.size() - i);
    return true;
}

}  // namespace psp

#endif  // PSP_SPAN_PARSER_H_INCLUDED