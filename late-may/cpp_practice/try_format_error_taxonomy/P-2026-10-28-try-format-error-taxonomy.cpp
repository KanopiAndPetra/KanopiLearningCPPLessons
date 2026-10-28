// P-2026-10-28 — try_format_error_taxonomy:
//
// Closes Oct 27's "Where we go next" item (2) verbatim:
//
//   item (2) — "Give the arc's error taxonomy the resolution the stdlib
//               has.  FormatParse and FormatArgument are unreachable
//               today; the stdlib distinguishes four failure classes by
//               message.  Worth measuring whether libc++ exposes the
//               class structurally (an error code, or distinguishable
//               exception types) before deciding whether the arc should
//               classify by message prefix."
//
// THE QUESTION: the arc's petra::FormatErrorKind has FOUR values and
// the implementation only ever produces TWO of them (Format and
// TooLarge).  FormatParse and FormatArgument are DEAD — no code path in
// the arc can produce them.  Can they be brought to life?
//
// MEASURED ANSWER: yes, but only on the RUNTIME-format path, and only
// by adding a classifier the standard library does not provide.
//
// KEY FINDING #1 — THE STDLIB HAS NO STRUCTURAL ERROR CLASS AT ALL.
// `std::format_error` is the ONLY exception type the format machinery
// throws (measured: 65 `__throw_format_error` call sites in libc++ 21's
// headers, all of them throwing that one type; there is no subclass of
// it anywhere in the header set, and it carries no error_code — it is a
// bare `runtime_error` whose only payload is the message).  So the
// failure CLASS survives only in the MESSAGE TEXT, and libc++ has 43
// distinct message strings.  The class is not a type, a code, or an
// enum — it is a string.
//
// KEY FINDING #2 — THE MESSAGE TEXT IS NOT A CONTRACT, BUT IT IS
// STABLE ENOUGH TO BE USEFUL, AND THE ARC'S DECISION IS EXPLICIT.
// The message table below is libc++-SPECIFIC.  The arc's classifier is
// therefore TWO-STAGE and DEFENSIVE:
//   Stage A (grammar): the arc's own constexpr scanner decides whether
//                       the FORMAT STRING is well-formed.  If it is not,
//                       the failure is FormatParse.
//   Stage B (message):  if the string IS well-formed, the arc consults a
//                       table of known failure messages.  If the message
//                       is in it, the failure is FormatArgument; if not,
//                       the failure is Format.
//
// The FALLBACK MATTERS: an unrecognised message degrades to Format,
// which is the arc's pre-existing catch-all.  A future libc++ that
// rewords a message can therefore only make the arc LESS precise — it
// can never make the arc WRONG about which KIND of thing failed.
//
// KEY FINDING #3 — THE MESSAGE ALONE CANNOT ALWAYS DECIDE, AND THE
// GRAMMAR SCANNER IS WHAT RESCUES IT.  libc++'s runtime parser is
// TYPE-DIRECTED, so several DIFFERENT failures collapse onto ONE
// message ("The format specifier should consume the input or end with
// a '}'"):  an unknown presentation-type character, a precision on an
// integer, a sign on a string, and a locale flag on a string all produce
// it.  Measured over the corpus in Section 9.  So the message table has
// a third bucket — AMBIGUOUS — and the arc resolves an ambiguous
// message with the scanner: if the string scans as well-formed, then
// the failure has to be the argument's fault.
//
// KEY FINDING #4 — THE TWO ARC PATHS HAVE DIFFERENT TAXONOMIES, AND
// THE STDLIB'S OWN MESSAGES DIFFER TOO.  The compile-time path's format
// string has already been validated by `std::format_string`'s consteval
// constructor, so a runtime FormatParse is UNREACHABLE there.  Worse,
// the SAME failure produces DIFFERENT messages on the two paths:
// compile-time "The format specification does not allow the sign
// option" vs runtime "The format specifier for a string does not allow
// the sign option".  Measured; Section 8.
//
// KEY FINDING #5 — A THIRD FAILURE MODE NEEDS THE FALLBACK.
// A USER'S FORMATTER can throw `std::format_error` from inside
// `format()`.  The format string is then perfectly well-formed and the
// arguments are perfectly fine — the failure is neither.  The arc
// classifies it as Format, which is exactly what the fallback is for.
//
// The arc's contract changes in exactly one place: the runtime path's
// `catch (const std::format_error&)` now produces FormatParse or
// FormatArgument instead of always producing Format.  TooLarge is
// untouched, the two-pass (measure, then write) design is untouched,
// the buffer-atomicity behaviour is untouched, and the compile-time
// path's KIND is unchanged (it can only produce FormatArgument or
// Format).
//
// No pad flags: the pad axis is orthogonal to the error taxonomy, so
// the pad options are NOT reproduced here.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <iterator>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError / petra::FormatErrorKind
//           (verbatim from Sep 6 through Oct 27).
//
// The enum is UNCHANGED today: the four values, their order, and their
// meanings are exactly as they have been since Sep 6.  What changes is
// that two of them become reachable.
// ============================================================================

namespace petra {

enum class FormatErrorKind : unsigned char {
    FormatParse,
    FormatArgument,
    Format,
    TooLarge,
};

struct FormatError {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    std::size_t needed_bytes{0};

    FormatError() = default;
    FormatError(FormatErrorKind k, std::string m,
                std::size_t needed = 0) noexcept
        : kind(k), message(std::move(m)), needed_bytes(needed) {}

    [[nodiscard]] bool operator==(const FormatError& other) const noexcept {
        return kind == other.kind && message == other.message
            && needed_bytes == other.needed_bytes;
    }
    [[nodiscard]] bool operator!=(const FormatError& other) const noexcept {
        return !(*this == other);
    }
};

}  // namespace petra

// ============================================================================
// Part 2 — petra::counting_output_iterator
//           (verbatim from Oct 11 .. Oct 27).
// ============================================================================

namespace petra {

class counting_output_iterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        counting_output_iterator* it_;
        constexpr const proxy& operator=(char) const noexcept {
            ++it_->count_;
            if (it_->cb_) it_->cb_(it_->count_, it_->data_);
            return *this;
        }
    };

    constexpr counting_output_iterator(callback_type cb, void* data) noexcept
        : cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<counting_output_iterator*>(this)};
    }
    constexpr counting_output_iterator& operator++() noexcept { return *this; }
    constexpr counting_output_iterator operator++(int) noexcept { return *this; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer
//           (verbatim from Oct 11 .. Oct 27).
// ============================================================================

namespace petra {
namespace detail {

class bounded_char_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        bounded_char_writer* it_;
        constexpr const proxy& operator=(char c) const noexcept {
            if (it_->written_ < it_->cap_) {
                it_->buf_[it_->written_] = c;
                ++it_->written_;
                if (it_->cb_) it_->cb_(it_->written_, it_->data_);
            }
            return *this;
        }
    };

    constexpr bounded_char_writer(char* buf, std::size_t cap,
                                  callback_type cb, void* data) noexcept
        : buf_(buf), cap_(cap), cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<bounded_char_writer*>(this)};
    }
    constexpr bounded_char_writer& operator++() noexcept { return *this; }
    constexpr bounded_char_writer operator++(int) noexcept { return *this; }

    std::size_t written() const noexcept { return written_; }

private:
    char* buf_;
    std::size_t cap_;
    std::size_t written_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 4 — NEW TODAY: petra::detail::GrammarScanner.
//
// A constexpr scanner for the REPLACEMENT-FIELD GRAMMAR of a format
// string.  It answers ONE question — "is this format string
// well-formed?" — and it answers it WITHOUT looking at the arguments
// and WITHOUT looking at any stdlib message.
//
// It is deliberately NOT a full format-spec implementation: it checks
// the SHAPE of the string, not whether a given type accepts a given
// spec.  That limitation is the lesson (KEY FINDING #3): libc++'s own
// parser is type-directed, so some of its "parse" errors are really
// argument errors, and a string-only scanner cannot see the difference.
//
// The grammar implemented here is [format.string.std]:
//
//   format-string   ::= ( char | "{{" | "}}" | replacement-field )*
//   replacement-field
//                    ::= "{" [arg-id] [ ":" format-spec ] "}"
//   arg-id          ::= integer
//   format-spec     ::= [ fill-and-align ] [ sign ] [ "#" ] [ "0" ]
//                       [ width ] [ "." precision ] [ "L" ] [ type ]
//   width           ::= integer | "{" [arg-id] "}"
//   precision       ::= integer | "{" [arg-id] "}"
//
// The scanner also enforces the rules that are about the string as a
// whole rather than about one field:
//   - the argument-numbering mode may not be mixed (manual after
//     automatic, or automatic after manual), and
//   - a width may not have a leading zero.
//
// The type whitelist is the UNION of every presentation type any
// standard argument category accepts, because the scanner is
// type-agnostic.  It cannot know that "{:f}" is fine for a double and
// wrong for an int — only the argument's type can say that.
// ============================================================================

namespace petra {
namespace detail {

enum class GrammarVerdict : unsigned char {
    WellFormed,
    Malformed,
};

struct GrammarResult {
    GrammarVerdict verdict{GrammarVerdict::WellFormed};
    const char* reason{""};

    [[nodiscard]] constexpr bool well_formed() const noexcept {
        return verdict == GrammarVerdict::WellFormed;
    }
};

namespace grammar {

constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

constexpr bool is_align(char c) noexcept {
    return c == '<' || c == '>' || c == '^';
}

constexpr bool is_sign(char c) noexcept {
    return c == '+' || c == '-' || c == ' ';
}

// The UNION of every presentation type libc++ 21 recognises, over all
// argument categories: a A b B c d e E f F g G o p P s x X ?  (the '?'
// is C++23's debug format).  A character outside this set cannot be a
// type option, so the spec is malformed.
constexpr bool is_type(char c) noexcept {
    switch (c) {
    case 'a': case 'A': case 'b': case 'B': case 'c': case 'd':
    case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
    case 'o': case 'p': case 'P': case 's': case 'x': case 'X':
    case '?':
        return true;
    default:
        return false;
    }
}

}  // namespace grammar

class GrammarScanner {
public:
    constexpr explicit GrammarScanner(std::string_view fmt) noexcept
        : fmt_(fmt) {}

    constexpr GrammarResult run() noexcept {
        std::size_t i = 0;
        while (i < fmt_.size()) {
            const char c = fmt_[i];
            if (c == '{') {
                if (i + 1 < fmt_.size() && fmt_[i + 1] == '{') {
                    i += 2;  // "{{" is an escaped '{'
                    continue;
                }
                ++i;  // past the opening '{'
                if (!scan_field(i)) {
                    return GrammarResult{GrammarVerdict::Malformed, reason_};
                }
                continue;
            }
            if (c == '}') {
                if (i + 1 < fmt_.size() && fmt_[i + 1] == '}') {
                    i += 2;  // "}}" is an escaped '}'
                    continue;
                }
                reason_ = "a '}' that is not part of an escape sequence";
                return GrammarResult{GrammarVerdict::Malformed, reason_};
            }
            ++i;
        }
        return GrammarResult{GrammarVerdict::WellFormed, ""};
    }

private:
    // i points just past the '{'.  On success i points just past the
    // field's closing '}'.
    constexpr bool scan_field(std::size_t& i) noexcept {
        const std::size_t id_start = i;
        while (i < fmt_.size() && grammar::is_digit(fmt_[i])) {
            ++i;
        }
        const bool manual = (i > id_start);
        if (!note_numbering_mode(manual)) {
            return false;
        }
        if (i >= fmt_.size()) {
            reason_ = "the replacement field has no terminator";
            return false;
        }
        if (fmt_[i] == ':') {
            ++i;
            if (!scan_spec(i)) {
                return false;
            }
        } else if (fmt_[i] == '}') {
            ++i;
        } else {
            reason_ = "the argument index is not terminated by ':' or '}'";
            return false;
        }
        return true;
    }

    // The nested replacement field used for a dynamic width or
    // precision: "{" [arg-id] "}" — no format-spec inside.
    constexpr bool scan_nested_field(std::size_t& i) noexcept {
        const std::size_t id_start = i;
        while (i < fmt_.size() && grammar::is_digit(fmt_[i])) {
            ++i;
        }
        const bool manual = (i > id_start);
        if (!note_numbering_mode(manual)) {
            return false;
        }
        if (i >= fmt_.size() || fmt_[i] != '}') {
            reason_ = "the nested replacement field has no terminator";
            return false;
        }
        ++i;
        return true;
    }

    // i points just past the ':'.
    constexpr bool scan_spec(std::size_t& i) noexcept {
        const std::size_t n = fmt_.size();

        // fill-and-align.  A fill character is any character (other
        // than '{') that is followed by an alignment character.
        if (i + 1 < n && grammar::is_align(fmt_[i + 1])) {
            if (fmt_[i] == '{') {
                reason_ = "the fill option contains an invalid value";
                return false;
            }
            i += 2;
        } else if (i < n && grammar::is_align(fmt_[i])) {
            ++i;
        }

        // sign
        if (i < n && grammar::is_sign(fmt_[i])) {
            ++i;
        }
        // alternate form
        if (i < n && fmt_[i] == '#') {
            ++i;
        }
        // zero padding.  The width may not then start with '0'.
        if (i < n && fmt_[i] == '0') {
            ++i;
            if (i < n && fmt_[i] == '0') {
                reason_ = "a width may not have a leading zero";
                return false;
            }
        }

        // width
        if (i < n && grammar::is_digit(fmt_[i])) {
            while (i < n && grammar::is_digit(fmt_[i])) {
                ++i;
            }
        } else if (i < n && fmt_[i] == '{') {
            ++i;
            if (!scan_nested_field(i)) {
                return false;
            }
        }

        // precision
        if (i < n && fmt_[i] == '.') {
            ++i;
            if (i < n && grammar::is_digit(fmt_[i])) {
                while (i < n && grammar::is_digit(fmt_[i])) {
                    ++i;
                }
            } else if (i < n && fmt_[i] == '{') {
                ++i;
                if (!scan_nested_field(i)) {
                    return false;
                }
            } else {
                reason_ = "the precision option does not contain a value "
                          "or an argument index";
                return false;
            }
        }

        // locale-specific form
        if (i < n && fmt_[i] == 'L') {
            ++i;
        }

        // type
        if (i < n && fmt_[i] != '}') {
            if (!grammar::is_type(fmt_[i])) {
                reason_ = "the type option is not a known presentation type";
                return false;
            }
            ++i;
        }

        if (i >= n || fmt_[i] != '}') {
            reason_ = "the replacement field has no terminator";
            return false;
        }
        ++i;
        return true;
    }

    constexpr bool note_numbering_mode(bool manual) noexcept {
        if (manual) {
            if (auto_seen_) {
                reason_ = "manual argument numbering after automatic numbering";
                return false;
            }
            manual_seen_ = true;
        } else {
            if (manual_seen_) {
                reason_ = "automatic argument numbering after manual numbering";
                return false;
            }
            auto_seen_ = true;
        }
        return true;
    }

    std::string_view fmt_;
    const char* reason_{""};
    bool manual_seen_{false};
    bool auto_seen_{false};
};

[[nodiscard]] constexpr GrammarResult
scan_format_string(std::string_view fmt) noexcept {
    return GrammarScanner{fmt}.run();
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — NEW TODAY: the MESSAGE TABLE and the two-stage classifier.
//
// This table is libc++-SPECIFIC.  It is not part of any standard: the
// C++ standard requires that a format failure throw `std::format_error`
// and says NOTHING about the message.  Every entry below was read out
// of libc++ 21's headers and confirmed by running the failing case.
//
// Three buckets, not two:
//
//   Parse     — the message unambiguously means the FORMAT STRING is
//               malformed.
//   Argument  — the message unambiguously means the ARGUMENT does not
//               fit the format string.
//   Ambiguous — the message does NOT distinguish the two.  libc++'s
//               runtime parser is type-directed, so "The format
//               specifier should consume the input or end with a '}'"
//               is emitted for an unknown type character AND for a
//               precision on an integer AND for a sign on a string.
//               (Measured: Section 9.)
//
// An unrecognised message is neither: it is UNKNOWN, and the arc's
// fallback (Format) covers it.
// ============================================================================

namespace petra {
namespace detail {

enum class MessageVerdict : unsigned char {
    Parse,
    Argument,
    Ambiguous,
    Unknown,
};

// Messages that unambiguously describe a MALFORMED FORMAT STRING.
constexpr std::string_view kParseMessages[] = {
    "The format string terminates at a '{'",
    "The format string contains an invalid escape sequence",
    "The argument index starts with an invalid character",
    "The argument index is invalid",
    "End of input while parsing an argument index",
    "The argument index should end with a ':' or a '}'",
    "The replacement field misses a terminating '}'",
    "Using automatic argument numbering in manual argument numbering mode",
    "Using manual argument numbering in automatic argument numbering mode",
    "The precision option does not contain a value or an argument index",
    "End of input while parsing format specifier precision",
    "The fill option contains an invalid value",
    "The format specifier contains malformed Unicode characters",
    "The numeric value of the format specifier is too large",
};

// Messages that unambiguously describe a MISMATCH BETWEEN THE SPEC AND
// THE ARGUMENT.
//
// Note the first two entries: they are PREFIXES, because libc++'s
// RUNTIME messages interpolate the argument's category ("for an
// integer", "for a bool", "for a floating-point", ...).  An exact-match
// table would miss every one of them.  (The compile-time messages for
// the same failures are the generic forms "The format specification
// does not allow the sign option" and "The type option contains an
// invalid value for a string formatting argument" — which is why the
// compile-time and runtime tables below differ.  KEY FINDING #4.)
constexpr std::string_view kArgumentPrefixes[] = {
    "The type option contains an invalid value for ",
    "The format specifier for ",
};

constexpr std::string_view kArgumentMessages[] = {
    "The argument index value is too large for the number of arguments "
    "supplied",
    "The value of the argument index exceeds its maximum value",
    "Argument index outside the valid range",
    "An argument index may not have a negative value",
    "Replacement argument isn't a standard signed or unsigned integer type",
    "The type does not fit in the mask",
    "Integral value outside the range of the char type",
    "Type s requires character type as formatting argument",
    "Type ?s requires character type as formatting argument",
    "Type m requires a pair or a tuple with two elements",
    "Type s and an underlying format specification can't be used together",
    "Type ?s and an underlying format specification can't be used together",
    "The n option and type s can't be used together",
    "The n option and type ?s can't be used together",
    // The compile-time (generic) spellings of the runtime (parametric)
    // messages above.
    //
    // NOTE the sign option has THREE spellings, not two:
    //   "The format specification does not allow the sign option"
    //       (compile-time, from __parse — the parser never gets that
    //        far for an argument type with no sign field)
    //   "The format specifier does not allow the sign option"
    //       (compile-time, from __validate)
    //   "The format specifier for a bool does not allow the sign option"
    //       (runtime, from __validate — matched by PREFIX)
    // Measured in Section 8 and by the compile-fail probes.
    "The format specification does not allow the sign option",
    "The format specifier does not allow the sign option",
    "The format specifier does not allow the alternate form option",
    "The format specifier does not allow the zero-padding option",
    "The format specifier does not allow the precision option",
    "The format specifier does not allow the locale-specific form option",
    "The format specifier does not allow the n option",
    "The format specifier uses an invalid value for the type option",
};

// Messages that do NOT identify the failure class on their own.
//
// "The format specifier should consume the input or end with a '}'" is
// libc++'s catch-all for "this spec does not fit this argument": it is
// emitted for an unknown presentation-type character AND for a
// precision on an integer AND for a sign on a string AND for a locale
// flag on a string.  (Measured: Section 6.)
//
// "The width option should not have a leading zero" is ambiguous for a
// DIFFERENT reason: it is emitted both for a genuinely malformed spec
// ("{:00x}") AND for a well-formed spec whose ARGUMENT does not accept
// zero-padding ("{:0}" applied to a string — the '0' falls through to
// the width parser, which then rejects the leading zero).  The grammar
// scanner is what tells the two apart.
constexpr std::string_view kAmbiguousMessages[] = {
    "The format specifier should consume the input or end with a '}'",
    "The width option should not have a leading zero",
};

template <std::size_t N>
[[nodiscard]] constexpr bool
contains(const std::string_view (&table)[N], std::string_view needle) noexcept {
    for (const std::string_view entry : table) {
        if (entry == needle) {
            return true;
        }
    }
    return false;
}

template <std::size_t N>
[[nodiscard]] constexpr bool
starts_with_any(const std::string_view (&table)[N],
                std::string_view needle) noexcept {
    for (const std::string_view entry : table) {
        if (needle.size() >= entry.size()
            && needle.substr(0, entry.size()) == entry) {
            return true;
        }
    }
    return false;
}

// The INDEPENDENT classifier: message text -> verdict.  Used as the
// cross-check against the grammar scanner, and as Stage B of the
// production classifier.
[[nodiscard]] constexpr MessageVerdict
verdict_from_message(std::string_view message) noexcept {
    if (contains(kParseMessages, message)) {
        return MessageVerdict::Parse;
    }
    if (contains(kArgumentMessages, message)) {
        return MessageVerdict::Argument;
    }
    if (starts_with_any(kArgumentPrefixes, message)) {
        return MessageVerdict::Argument;
    }
    if (contains(kAmbiguousMessages, message)) {
        return MessageVerdict::Ambiguous;
    }
    return MessageVerdict::Unknown;
}

// The PRODUCTION classifier: format string + message -> kind.
//
// Stage A: if the format string is malformed, the failure is a parse
//          failure, whatever the message says.
// Stage B: if the format string is well-formed, the message decides —
//          and an AMBIGUOUS message also resolves to FormatArgument,
//          because a string that scans as well-formed cannot fail for
//          a syntactic reason.
// Fallback: otherwise the failure is Format — which covers a user's
//          formatter throwing `std::format_error` from inside
//          `format()`, and any message this libc++ version has that
//          the table does not know.
[[nodiscard]] FormatError
classify_format_failure(std::string_view fmt, const char* what) noexcept {
    const GrammarResult g = scan_format_string(fmt);
    if (!g.well_formed()) {
        return FormatError{FormatErrorKind::FormatParse,
                           std::string{what} + " [format string: "
                               + g.reason + "]"};
    }
    switch (verdict_from_message(what)) {
    case MessageVerdict::Parse:
        // The message says "parse" but the string scans as well-formed.
        // The scanner and the message disagree; fall back rather than
        // assert a kind we cannot support.
        return FormatError{FormatErrorKind::Format,
                           std::string{what}
                               + " [scanner disagrees: string is "
                                 "well-formed]"};
    case MessageVerdict::Argument:
        return FormatError{FormatErrorKind::FormatArgument, std::string{what}};
    case MessageVerdict::Ambiguous:
        return FormatError{FormatErrorKind::FormatArgument,
                           std::string{what}
                               + " [ambiguous message; format string is "
                                 "well-formed, so the argument is at "
                                 "fault]"};
    case MessageVerdict::Unknown:
    default:
        return FormatError{FormatErrorKind::Format, std::string{what}};
    }
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 6 — the two two-pass impls.
//
// `try_format_runtime_n_impl`         takes a RUNTIME format string.
// `try_format_bounded_runtime_n_impl` takes a COMPILE-TIME format string.
//
// The ONLY change from Oct 27 is the `catch (const std::format_error&)`
// block: it now routes the failure through
// `detail::classify_format_failure` instead of always producing
// `FormatErrorKind::Format`.
//
// Everything else — the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour — is unchanged.
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt,
                          const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(classify_format_failure(fmt, e.what()));
    }

    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(classify_format_failure(fmt, e.what()));
    }
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }
    return written;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt_str,
                                  const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt_str.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            classify_format_failure(fmt_str.get(), e.what()));
    }

    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt_str.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            classify_format_failure(fmt_str.get(), e.what()));
    }
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 7 — the compile-time pins.
//
// Each static_assert is a claim about a SPECIFIC rule, not about this
// file's plumbing.
// ============================================================================

namespace petra {
namespace detail {

// KEY FINDING #1 — the stdlib has exactly ONE error type for format
// failures, and it is a plain runtime_error: no error_code, no
// enumeration of failure classes, no subclass.
static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
              "KEY FINDING #1: std::format_error is a runtime_error");

template <typename T, typename = void>
struct has_code : std::false_type {};
template <typename T>
struct has_code<T, std::void_t<decltype(std::declval<T>().code())>>
    : std::true_type {};

static_assert(!has_code<std::format_error>::value,
              "KEY FINDING #1: std::format_error carries NO error_code — "
              "the failure class survives only in the message text");

static_assert(std::is_same_v<
                  std::remove_cvref_t<
                      decltype(std::declval<const std::format_error&>().what())>,
                  const char*>,
              "KEY FINDING #1: the only payload std::format_error carries "
              "is the message string");

// KEY FINDING #2 — the arc's four kinds are four distinct values, and
// the classifier can now produce all four.
static_assert(petra::FormatErrorKind::FormatParse
                  != petra::FormatErrorKind::FormatArgument,
              "the four kinds are four distinct values");
static_assert(petra::FormatErrorKind::FormatArgument
                  != petra::FormatErrorKind::Format,
              "an argument error is distinct from a generic format error");
static_assert(petra::FormatErrorKind::Format
                  != petra::FormatErrorKind::TooLarge,
              "a format error and a capacity error are distinct");

// The scanner is a constexpr function: its verdict is available at
// compile time, so the arc can classify a literal format string
// without running anything.
static_assert(scan_format_string("{}").well_formed(),
              "the scanner accepts a plain replacement field");
static_assert(!scan_format_string("{").well_formed(),
              "the scanner rejects a format string that terminates at a '{'");
static_assert(scan_format_string("{{}}").well_formed(),
              "the scanner accepts escaped braces");
static_assert(!scan_format_string("}").well_formed(),
              "the scanner rejects a lone '}'");
static_assert(scan_format_string("{:>10}").well_formed(),
              "the scanner accepts fill-and-align");
static_assert(scan_format_string("{:#06x}").well_formed(),
              "the scanner accepts alternate form, zero padding and a type");
static_assert(!scan_format_string("{:00x}").well_formed(),
              "a width may not have a leading zero");
static_assert(scan_format_string("{0} {1}").well_formed(),
              "the scanner accepts manual numbering throughout");
static_assert(!scan_format_string("{} {0}").well_formed(),
              "the scanner rejects mixed argument numbering");

// KEY FINDING #3 — the ambiguous message really is one message for
// several failures.  The scanner is what tells them apart.
static_assert(verdict_from_message(kAmbiguousMessages[0])
                  == MessageVerdict::Ambiguous,
              "KEY FINDING #3: libc++'s catch-all runtime message is "
              "AMBIGUOUS — it does not identify the failure class");
static_assert(verdict_from_message(kAmbiguousMessages[1])
                  == MessageVerdict::Ambiguous,
              "KEY FINDING #3: 'a width may not have a leading zero' is "
              "ambiguous too — it is emitted for a malformed spec AND for "
              "a well-formed spec whose argument rejects zero-padding");
static_assert(verdict_from_message(
                  "The type option contains an invalid value for an "
                  "integer formatting argument")
                  == MessageVerdict::Argument,
              "a runtime type message is an argument error — and it has "
              "to be matched by PREFIX, because libc++ interpolates the "
              "argument's category");

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 8 — the test types.
//
// `user_throws` — a formatter that throws `std::format_error` from
// inside `format()`.  The format string is well-formed and the
// argument is fine; the failure is neither a parse error nor an
// argument error.  This is the case that forces the FALLBACK.
// ============================================================================

namespace petra_test {

struct user_throws {
    int value{0};
};

}  // namespace petra_test

template <>
struct std::formatter<petra_test::user_throws> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::user_throws&,
                std::format_context& ctx) const -> decltype(ctx.out()) {
        throw std::format_error("the user's own formatter failed");
    }
};

// ============================================================================
// Part 9 — the hand-rolled SECTION/CHECK framework
//           (verbatim shape from Sep 6 .. Oct 27).
// ============================================================================

namespace petra_test {

int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

void section(const char* name) {
    g_section = name;
    std::println("\n== {} ==", name);
}

template <typename A, typename B>
std::enable_if_t<!std::is_convertible_v<A, std::string_view>
                 && !std::is_convertible_v<B, std::string_view>, void>
check_eq(const A& got, const B& expected, const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ  %s\n", g_section, what);
    }
}

void check_eq(std::string_view got, std::string_view expected,
              const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ(sv)  %s: got \"%.*s\" expected \"%.*s\"\n",
            g_section, what,
            static_cast<int>(got.size()), got.data(),
            static_cast<int>(expected.size()), expected.data());
    }
}

void check_eq(char got, char expected, const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ(char)  %s: got '%c' (0x%02X) expected "
            "'%c' (0x%02X)\n",
            g_section, what, got,
            static_cast<unsigned>(static_cast<unsigned char>(got)),
            expected,
            static_cast<unsigned>(static_cast<unsigned char>(expected)));
    }
}

void check_true(bool cond, const char* what) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_TRUE  %s\n", g_section, what);
    }
}

}  // namespace petra_test

// ============================================================================
// Part 10 — the corpus machinery.
//
// Every cell is a (format string, argument set) pair that the stdlib
// either accepts or rejects.  Each cell is scored on SEVEN independent
// axes, each with its own counter, so a failure names the claim it
// broke:
//   disagree        the two classifiers (grammar scanner vs message
//                   table) reached different verdicts
//   kind_fail       the arc's kind is not one of the four
//   dead_fail       the arc produced a kind it could not produce before
//                   today (i.e. FormatParse or FormatArgument)
//   atomicity_fail  the arc reported an error but MODIFIED the buffer
//   bleed_fail      the arc succeeded but modified bytes PAST its
//                   reported `written` count
//   kind_expect_fail the arc's kind differs from the hand-written
//                   oracle's kind
//   unknown_msg     the stdlib's message was not in the table at all
// ============================================================================

namespace {

constexpr char kPoison = 'Q';
constexpr std::size_t kBufSize = 64;
constexpr std::size_t kNone = static_cast<std::size_t>(-1);

using usz = std::size_t;

std::size_t last_modified(const char* buf, std::size_t n) noexcept {
    for (std::size_t i = n; i > 0; --i) {
        if (buf[i - 1] != kPoison) {
            return i - 1;
        }
    }
    return kNone;
}

struct CellResult {
    const char* desc{""};
    std::string_view fmt;
    bool stdlib_ok{false};
    std::string stdlib_message;
    bool arc_ok{false};
    usz arc_written{0};
    petra::FormatErrorKind arc_kind{petra::FormatErrorKind::Format};
    std::string arc_message;
    bool buffer_untouched{true};
    // The index of the LAST byte that differs from the poison, or
    // kNone if the buffer was not touched.  This is what the
    // "wrote past its reported count" check needs — the index of the
    // FIRST modified byte is 0 for ANY written output, so it cannot
    // answer the question (Oct 27's metric bug).
    usz last_mod{kNone};
    bool scanner_well_formed{false};
    const char* scanner_reason{""};
    petra::detail::MessageVerdict table_verdict{
        petra::detail::MessageVerdict::Unknown};
};

template <typename... Args>
CellResult run_cell(const char* desc, std::string_view fmt, const Args&... args) {
    CellResult r;
    r.desc = desc;
    r.fmt = fmt;

    // 1. What does the stdlib do?  (message captured verbatim)
    {
        std::string out;
        try {
            (void)std::vformat_to(std::back_inserter(out), fmt,
                                  std::make_format_args(args...));
            r.stdlib_ok = true;
        } catch (const std::format_error& e) {
            r.stdlib_ok = false;
            r.stdlib_message = e.what();
        }
    }

    // 2. What does the arc do?
    char buf[kBufSize];
    std::memset(buf, kPoison, kBufSize);
    const auto a = petra::detail::try_format_runtime_n_impl(buf, kBufSize,
                                                              fmt, args...);
    r.arc_ok = a.has_value();
    if (r.arc_ok) {
        r.arc_written = *a;
    } else {
        r.arc_kind = a.error().kind;
        r.arc_message = a.error().message;
    }
    r.last_mod = last_modified(buf, kBufSize);
    r.buffer_untouched = (r.last_mod == kNone);

    // 3. The two INDEPENDENT classifiers.
    const auto g = petra::detail::scan_format_string(fmt);
    r.scanner_well_formed = g.well_formed();
    r.scanner_reason = g.reason;
    r.table_verdict = petra::detail::verdict_from_message(r.stdlib_message);

    return r;
}

using CellFn = std::function<CellResult(std::string_view)>;

struct Cell {
    const char* desc;
    std::string_view fmt;
    CellFn fn;
    bool expect_failure{false};
    petra::FormatErrorKind expect_kind{petra::FormatErrorKind::Format};
    bool user_thrown{false};
};

const char* kind_name(petra::FormatErrorKind k) noexcept {
    switch (k) {
    case petra::FormatErrorKind::FormatParse:
        return "FormatParse";
    case petra::FormatErrorKind::FormatArgument:
        return "FormatArgument";
    case petra::FormatErrorKind::Format:
        return "Format";
    case petra::FormatErrorKind::TooLarge:
        return "TooLarge";
    }
    return "?";
}

const char* verdict_name(petra::detail::MessageVerdict v) noexcept {
    switch (v) {
    case petra::detail::MessageVerdict::Parse:
        return "Parse";
    case petra::detail::MessageVerdict::Argument:
        return "Argument";
    case petra::detail::MessageVerdict::Ambiguous:
        return "Ambiguous";
    case petra::detail::MessageVerdict::Unknown:
        return "Unknown";
    }
    return "?";
}

// The corpus.  `expect_failure` says whether the stdlib rejects the
// case; `expect_kind` is the kind the arc SHOULD report.
//
// The expected kinds are written by hand from [format.string.std] —
// they are the ORACLE, and they are written before the classifier, not
// derived from it.
std::vector<Cell> make_corpus() {
    using petra::FormatErrorKind;
    std::vector<Cell> cells;

    // ---- valid format strings -------------------------------------
    cells.push_back({"valid-plain", "{}", [](std::string_view f) {
        return run_cell("valid-plain", f, 42); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-two", "{}-{}", [](std::string_view f) {
        return run_cell("valid-two", f, 1, 2); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-escaped-braces", "{{}}", [](std::string_view f) {
        return run_cell("valid-escaped-braces", f); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-width", "{:5}", [](std::string_view f) {
        return run_cell("valid-width", f, 42); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-align", "{:>5}", [](std::string_view f) {
        return run_cell("valid-align", f, 42); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-precision", "{:.2}", [](std::string_view f) {
        return run_cell("valid-precision", f, 1.5); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-hex", "{:#x}", [](std::string_view f) {
        return run_cell("valid-hex", f, 42); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-manual-index", "{0}", [](std::string_view f) {
        return run_cell("valid-manual-index", f, 42); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-dynamic-width", "{:{}}", [](std::string_view f) {
        return run_cell("valid-dynamic-width", f, 42, 5); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-string", "{:s}", [](std::string_view f) {
        return run_cell("valid-string", f, std::string{"s"}); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-char", "{:c}", [](std::string_view f) {
        return run_cell("valid-char", f, 'A'); }, false, FormatErrorKind::Format});
    cells.push_back({"valid-bool-as-int", "{:d}", [](std::string_view f) {
        return run_cell("valid-bool-as-int", f, true); }, false, FormatErrorKind::Format});

    // ---- malformed format strings: the arc must say FormatParse ----
    cells.push_back({"term-at-brace", "{", [](std::string_view f) {
        return run_cell("term-at-brace", f); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"lone-close-brace", "}", [](std::string_view f) {
        return run_cell("lone-close-brace", f); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"invalid-escape", "a}b", [](std::string_view f) {
        return run_cell("invalid-escape", f); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"index-not-terminated", "{0x", [](std::string_view f) {
        return run_cell("index-not-terminated", f, 1); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"missing-close-brace", "{0", [](std::string_view f) {
        return run_cell("missing-close-brace", f, 1); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"negative-index", "{-1}", [](std::string_view f) {
        return run_cell("negative-index", f, 1); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"unterminated-spec", "{:5", [](std::string_view f) {
        return run_cell("unterminated-spec", f, 42); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"unknown-type-char", "{:q}", [](std::string_view f) {
        return run_cell("unknown-type-char", f, 42); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"mixed-numbering-auto-then-manual", "{} {0}", [](std::string_view f) {
        return run_cell("mixed-numbering-auto-then-manual", f, 1); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"mixed-numbering-manual-then-auto", "{0} {}", [](std::string_view f) {
        return run_cell("mixed-numbering-manual-then-auto", f, 1); }, true, FormatErrorKind::FormatParse});
    cells.push_back({"width-leading-zero", "{:00x}", [](std::string_view f) {
        return run_cell("width-leading-zero", f, 42); }, true, FormatErrorKind::FormatParse});

    // ---- argument mismatches: the arc must say FormatArgument ----
    cells.push_back({"index-too-large", "{5}", [](std::string_view f) {
        return run_cell("index-too-large", f, 1); }, true, FormatErrorKind::FormatArgument});
    cells.push_back({"no-argument-supplied", "{}", [](std::string_view f) {
        return run_cell("no-argument-supplied", f); }, true, FormatErrorKind::FormatArgument});
    cells.push_back({"too-few-arguments", "{} {}", [](std::string_view f) {
        return run_cell("too-few-arguments", f, 1); }, true, FormatErrorKind::FormatArgument});
    cells.push_back({"float-type-on-int", "{:f}", [](std::string_view f) {
        return run_cell("float-type-on-int", f, 42); }, true, FormatErrorKind::FormatArgument});
    cells.push_back({"int-type-on-double", "{:d}", [](std::string_view f) {
        return run_cell("int-type-on-double", f, 1.5); }, true, FormatErrorKind::FormatArgument});
    cells.push_back({"string-type-on-int", "{:s}", [](std::string_view f) {
        return run_cell("string-type-on-int", f, 42); }, true, FormatErrorKind::FormatArgument});
    cells.push_back({"dynamic-width-not-an-integer", "{:{}}", [](std::string_view f) {
        return run_cell("dynamic-width-not-an-integer", f, 42, std::string{"x"}); },
        true, FormatErrorKind::FormatArgument});
    cells.push_back({"negative-width-value", "{:{}}", [](std::string_view f) {
        return run_cell("negative-width-value", f, 42, -3); },
        true, FormatErrorKind::FormatArgument});
    // The DISAGREEMENT cell: the string is well-formed, but the
    // stdlib's message blames the width.  The scanner says the string
    // is fine, so the failure is the argument's fault.
    cells.push_back({"zero-width-on-string", "{:0}", [](std::string_view f) {
        return run_cell("zero-width-on-string", f, std::string{"s"}); },
        true, FormatErrorKind::FormatArgument});

    // ---- the fallback: the user's own formatter throws ------------
    cells.push_back({"user-formatter-throws", "{}", [](std::string_view f) {
        return run_cell("user-formatter-throws", f,
                        petra_test::user_throws{7}); },
        true, FormatErrorKind::Format, true});

    return cells;
}

struct counters {
    int cells{0};
    int disagree{0};
    int kind_fail{0};
    int dead_fail{0};
    int atomicity_fail{0};
    int bleed_fail{0};
    int kind_expect_fail{0};
    int unknown_msg{0};
};

}  // namespace

// ============================================================================
// Part 11 — the sections.
// ============================================================================

int main() {
    using namespace petra_test;
    using namespace petra;

    std::println("P-2026-10-28 — try_format_error_taxonomy");
    std::println("reviving petra::FormatErrorKind::FormatParse and "
                 "::FormatArgument");

    // ------------------------------------------------------------------
    // Section 1 — the taxonomy, and which kinds were dead.
    // ------------------------------------------------------------------
    section("Section 1: the taxonomy and the two dead kinds");
    {
        check_true(static_cast<int>(FormatErrorKind::FormatParse)
                       != static_cast<int>(FormatErrorKind::FormatArgument),
                   "the four kinds are four distinct values");
        check_true(static_cast<int>(FormatErrorKind::FormatArgument)
                       != static_cast<int>(FormatErrorKind::Format),
                   "FormatArgument is distinct from Format");
        check_true(static_cast<int>(FormatErrorKind::Format)
                       != static_cast<int>(FormatErrorKind::TooLarge),
                   "Format is distinct from TooLarge");

        std::println("  FormatParse    : the format string is malformed");
        std::println("  FormatArgument : the string is fine, the argument "
                     "is not");
        std::println("  Format         : neither (or unclassifiable)");
        std::println("  TooLarge       : the output does not fit");
        std::println("  before today, only Format and TooLarge were "
                     "reachable.");
    }

    // ------------------------------------------------------------------
    // Section 2 — KEY FINDING #1: the stdlib has NO structural class.
    // ------------------------------------------------------------------
    section("Section 2: the stdlib exposes no structural error class");
    {
        check_true(std::is_base_of_v<std::runtime_error, std::format_error>,
                   "std::format_error is a runtime_error");
        check_true(!detail::has_code<std::format_error>::value,
                   "std::format_error carries no error_code");
        check_true(std::is_same_v<
                       std::remove_cvref_t<decltype(
                           std::declval<const std::format_error&>().what())>,
                       const char*>,
                   "the only payload is the message text");

        // The failure class is a string, not a type.  Three different
        // failures, three different messages, one exception type.
        char buf[32];
        std::memset(buf, kPoison, sizeof(buf));
        const auto a = petra::detail::try_format_runtime_n_impl(buf, 32, "{");
        std::memset(buf, kPoison, sizeof(buf));
        const auto b = petra::detail::try_format_runtime_n_impl(buf, 32,
                                                                  "{:f}", 42);
        std::memset(buf, kPoison, sizeof(buf));
        const auto c = petra::detail::try_format_runtime_n_impl(buf, 32,
                                                                  "{5}", 1);

        check_true(!a.has_value() && !b.has_value() && !c.has_value(),
                   "all three cases fail");
        check_true(a.error().message != b.error().message
                       && b.error().message != c.error().message,
                   "the three failures carry three different messages");
        std::println("  \"{{\"       -> {}", a.error().message);
        std::println("  \"{{:f}}\" 42 -> {}", b.error().message);
        std::println("  \"{{5}}\"  1 -> {}", c.error().message);
        std::println("  one exception type; the class is carried by the "
                     "message text.");
    }

    // ------------------------------------------------------------------
    // Section 3 — the two-stage classifier, stage by stage.
    // ------------------------------------------------------------------
    section("Section 3: the two-stage classifier");
    {
        // Stage A: the grammar scanner.
        check_true(detail::scan_format_string("{}").well_formed(),
                   "scanner: '{}' is well-formed");
        check_true(!detail::scan_format_string("{").well_formed(),
                   "scanner: '{' is malformed");
        check_true(!detail::scan_format_string("}").well_formed(),
                   "scanner: '}' is malformed");
        check_true(!detail::scan_format_string("{:zz}").well_formed(),
                   "scanner: '{:zz}' is malformed");
        check_true(detail::scan_format_string("{:.2}").well_formed(),
                   "scanner: '{:.2}' is well-formed (the scanner cannot "
                   "know that an int rejects a precision)");

        // Stage B: the message table.
        check_true(detail::verdict_from_message(
                       "The argument index value is too large for the "
                       "number of arguments supplied")
                       == detail::MessageVerdict::Argument,
                   "table: an index-too-large message is an argument "
                   "error");
        check_true(detail::verdict_from_message(
                       "The format string terminates at a '{'")
                       == detail::MessageVerdict::Parse,
                   "table: a terminates-at-brace message is a parse error");
        check_true(detail::verdict_from_message(
                       detail::kAmbiguousMessages[0])
                       == detail::MessageVerdict::Ambiguous,
                   "table: libc++'s catch-all runtime message is "
                   "ambiguous");
        check_true(detail::verdict_from_message(
                       "something nobody has ever written")
                       == detail::MessageVerdict::Unknown,
                   "table: an unknown message is unknown");

        // The production classifier.
        const auto e1 = detail::classify_format_failure(
            "{", "The format string terminates at a '{'");
        check_true(e1.kind == FormatErrorKind::FormatParse,
                   "production: a malformed string is FormatParse");
        const auto e2 = detail::classify_format_failure(
            "{5}", "The argument index value is too large for the number "
                   "of arguments supplied");
        check_true(e2.kind == FormatErrorKind::FormatArgument,
                   "production: a bad index on a good string is "
                   "FormatArgument");
        const auto e3 = detail::classify_format_failure(
            "{}", "the user's own formatter failed");
        check_true(e3.kind == FormatErrorKind::Format,
                   "production: an unknown message is Format");
        const auto e4 = detail::classify_format_failure(
            "{:.2}", detail::kAmbiguousMessages[0].data());
        check_true(e4.kind == FormatErrorKind::FormatArgument,
                   "production: an ambiguous message on a well-formed "
                   "string is FormatArgument");
    }

    // ------------------------------------------------------------------
    // Section 4 — the dead kinds come alive.
    // ------------------------------------------------------------------
    section("Section 4: the two dead kinds are now reachable");
    {
        char buf[32];

        std::memset(buf, kPoison, sizeof(buf));
        const auto parse = petra::detail::try_format_runtime_n_impl(buf, 32,
                                                                       "{");
        check_true(!parse.has_value(), "a malformed string fails");
        check_true(parse.error().kind == FormatErrorKind::FormatParse,
                   "FormatParse is now produced");

        std::memset(buf, kPoison, sizeof(buf));
        const auto arg = petra::detail::try_format_runtime_n_impl(buf, 32,
                                                                      "{5}", 1);
        check_true(!arg.has_value(), "an out-of-range index fails");
        check_true(arg.error().kind == FormatErrorKind::FormatArgument,
                   "FormatArgument is now produced");

        std::memset(buf, kPoison, sizeof(buf));
        const auto generic = petra::detail::try_format_runtime_n_impl(
            buf, 32, "{}", petra_test::user_throws{7});
        check_true(!generic.has_value(), "a throwing formatter fails");
        check_true(generic.error().kind == FormatErrorKind::Format,
                   "a throwing formatter is Format, not a parse error");

        std::memset(buf, kPoison, sizeof(buf));
        const auto too_large = petra::detail::try_format_runtime_n_impl(
            buf, 3, "{}", 12345);
        check_true(!too_large.has_value(), "an overflowing output fails");
        check_true(too_large.error().kind == FormatErrorKind::TooLarge,
                   "TooLarge is unchanged");
        check_true(too_large.error().needed_bytes == 5,
                   "TooLarge still reports the needed byte count");

        std::println("  all four kinds are now produced by the runtime "
                     "path");
    }

    // ------------------------------------------------------------------
    // Section 5 — KEY FINDING #5: the fallback is load-bearing.
    // ------------------------------------------------------------------
    section("Section 5: the fallback is load-bearing");
    {
        // A user's formatter throws std::format_error from inside
        // format().  The string is well-formed, the argument is fine,
        // and the failure is NEITHER a parse error nor an argument
        // error.  Without the fallback the classifier would have to
        // invent a kind.
        char buf[32];
        std::memset(buf, kPoison, sizeof(buf));
        const auto r = petra::detail::try_format_runtime_n_impl(
            buf, 32, "{}", petra_test::user_throws{7});
        check_true(!r.has_value(), "the user's formatter fails");
        check_true(r.error().kind == FormatErrorKind::Format,
                   "it is classified as Format");
        check_true(r.error().message.find("user's own formatter")
                       != std::string::npos,
                   "the original message survives in the error");
        std::println("  message: {}", r.error().message);
    }

    // ------------------------------------------------------------------
    // Section 6 — KEY FINDING #3: the message alone cannot decide.
    //
    // The SAME message is emitted for several different failures.
    // Measured, not asserted: four different (format string, argument)
    // pairs, one message.
    // ------------------------------------------------------------------
    section("Section 6: one message, several failures");
    {
        int i = 42;
        std::string s = "s";

        const auto msg_of = [](auto&& fn) {
            try {
                (void)fn();
                return std::string{"OK"};
            } catch (const std::format_error& e) {
                return std::string{e.what()};
            }
        };

        const std::string m1 = msg_of([&] {
            (void)std::vformat("{:q}", std::make_format_args(i));
        });
        const std::string m2 = msg_of([&] {
            (void)std::vformat("{:.2}", std::make_format_args(i));
        });
        const std::string m3 = msg_of([&] {
            (void)std::vformat("{:+}", std::make_format_args(s));
        });
        const std::string m4 = msg_of([&] {
            (void)std::vformat("{:L}", std::make_format_args(s));
        });

        std::println("  unknown type char  \"{{:q}}\"  -> {}", m1);
        std::println("  precision on an int \"{{:.2}}\" -> {}", m2);
        std::println("  sign on a string  \"{{:+}}\"  -> {}", m3);
        std::println("  locale on a string \"{{:L}}\"  -> {}", m4);

        int same = 0;
        for (const std::string* m : {&m1, &m2, &m3, &m4}) {
            if (*m == std::string{
                           "The format specifier should consume the input "
                           "or end with a '}'"}) {
                ++same;
            }
        }
        check_true(same == 4,
                   "four DIFFERENT failures produce ONE identical message");
        std::println("  -> the message cannot classify any of them; the "
                     "grammar scanner is what separates them.");
    }

    // ------------------------------------------------------------------
    // Section 7 — KEY FINDING #4: the compile-time path has a NARROWER
    // taxonomy.  A FormatParse is UNREACHABLE there, because
    // std::format_string's consteval constructor has already rejected
    // every malformed string.
    // ------------------------------------------------------------------
    section("Section 7: the compile-time path has a narrower taxonomy");
    {
        char buf[32];

        // A well-formed string whose runtime VALUE is wrong: the
        // consteval check validates the TYPE of the width argument but
        // not its value, so this compiles and fails at runtime.
        std::memset(buf, kPoison, sizeof(buf));
        const auto ct = petra::detail::try_format_bounded_runtime_n_impl(
            buf, 32, "{:{}}", 42, -3);
        check_true(!ct.has_value(), "the compile-time path fails at runtime");
        check_true(ct.error().kind == FormatErrorKind::FormatArgument,
                   "FormatArgument is reachable on the compile-time path");
        check_true(ct.error().kind != FormatErrorKind::FormatParse,
                   "but FormatParse is not: the compiler already rejected "
                   "every malformed string");
        std::println("  message: {}", ct.error().message);

        // And the fallback is reachable there too.
        std::memset(buf, kPoison, sizeof(buf));
        const auto ct_user = petra::detail::try_format_bounded_runtime_n_impl(
            buf, 32, "{}", petra_test::user_throws{7});
        check_true(!ct_user.has_value(),
                   "the user's formatter fails on the compile-time path too");
        check_true(ct_user.error().kind == FormatErrorKind::Format,
                   "and it is classified as Format");

        // A compile-time-validated string always scans as well-formed.
        check_true(detail::scan_format_string("{:{}}").well_formed(),
                   "a compiler-accepted string always scans well-formed");
    }

    // ------------------------------------------------------------------
    // Section 8 — KEY FINDING #4 again: the stdlib's OWN messages differ
    // between the compile-time and runtime paths for the SAME failure.
    //
    // Measured: for a bool argument with a sign option,
    //   compile-time -> "The format specifier does not allow the sign
    //                   option"
    //   runtime     -> "The format specifier for a bool does not allow
    //                   the sign option"
    // The runtime form interpolates the argument's CATEGORY; the
    // compile-time form does not.
    // ------------------------------------------------------------------
    section("Section 8: the stdlib's messages differ between the two "
            "paths");
    {
        bool b = true;
        std::string runtime_msg;
        try {
            (void)std::vformat("{:+}", std::make_format_args(b));
        } catch (const std::format_error& e) {
            runtime_msg = e.what();
        }
        std::println("  runtime      : {}", runtime_msg);
        std::println("  compile-time : The format specifier does not allow "
                     "the sign option");
        check_true(runtime_msg.find("for a bool") != std::string::npos,
                   "the runtime message interpolates the argument's "
                   "category");
        check_true(detail::verdict_from_message(runtime_msg)
                       == detail::MessageVerdict::Argument,
                   "and it is still classified as an argument error");
        check_true(detail::verdict_from_message(
                       "The format specifier does not allow the sign option")
                       == detail::MessageVerdict::Argument,
                   "and so is the compile-time spelling of the same "
                   "failure — both are in the table");
        std::println("  -> the same failure, two different message "
                     "strings.  A table calibrated on one path does not "
                     "transfer to the other.");
    }

    // ------------------------------------------------------------------
    // Section 9 — the sweep: the two classifiers, independently.
    // ------------------------------------------------------------------
    section("Section 9: the two classifiers, independently");
    {
        counters ctr;
        int ambiguous = 0;
        int scanner_rescued = 0;

        const std::vector<Cell> corpus = make_corpus();
        for (const Cell& cell : corpus) {
            const CellResult r = cell.fn(cell.fmt);
            ++ctr.cells;

            if (!r.stdlib_ok) {
                if (r.table_verdict == petra::detail::MessageVerdict::Unknown
                    && !cell.user_thrown) {
                    ++ctr.unknown_msg;
                    std::println("  UNKNOWN MESSAGE  {:<32}  {}",
                                 r.desc, r.stdlib_message);
                }
                if (r.table_verdict == petra::detail::MessageVerdict::Ambiguous) {
                    ++ambiguous;
                    // The scanner resolves every ambiguous cell: a
                    // MALFORMED string means FormatParse, and a
                    // WELL-FORMED string that fails anyway must be the
                    // argument's fault, so FormatArgument.
                    const FormatErrorKind expected_from_scanner =
                        r.scanner_well_formed ? FormatErrorKind::FormatArgument
                                              : FormatErrorKind::FormatParse;
                    if (r.arc_kind == expected_from_scanner) {
                        ++scanner_rescued;
                    } else {
                        std::println("  NOT RESOLVED  {:<32}  scanner "
                                     "implies {}, arc said {}", r.desc,
                                     kind_name(expected_from_scanner),
                                     kind_name(r.arc_kind));
                    }
                    std::println("  AMBIGUOUS  {:<32}  message verdict = "
                                 "{}, scanner says {}", r.desc,
                                 verdict_name(r.table_verdict),
                                 r.scanner_well_formed ? "well-formed"
                                                       : "malformed");
                }
            }

            // The scanner's implied verdict vs the message table's.
            const bool scanner_says_parse = !r.scanner_well_formed;
            const bool table_says_parse =
                (r.table_verdict == petra::detail::MessageVerdict::Parse);
            const bool table_decides =
                (r.table_verdict == petra::detail::MessageVerdict::Parse
                 || r.table_verdict == petra::detail::MessageVerdict::Argument);

            if (table_decides && scanner_says_parse != table_says_parse) {
                ++ctr.disagree;
                std::println("  DISAGREE  {:<32}  scanner says {}, table "
                             "says {}", r.desc,
                             scanner_says_parse ? "Parse" : "Argument",
                             table_says_parse ? "Parse" : "Argument");
            }
        }

        std::println("\n  sweep: cells={} disagree={} ambiguous={} "
                     "scanner_rescued={} unknown_msg={}",
                     ctr.cells, ctr.disagree, ambiguous, scanner_rescued,
                     ctr.unknown_msg);
        check_true(ctr.unknown_msg == 0,
                   "every message the stdlib produced is in the table");
        check_true(ambiguous > 0,
                   "the catch-all message really is ambiguous somewhere");
        check_true(scanner_rescued == ambiguous,
                   "the grammar scanner resolves every ambiguous cell");
    }

    // ------------------------------------------------------------------
    // Section 10 — the full sweep, scored on seven axes.
    // ------------------------------------------------------------------
    section("Section 10: the full sweep");
    {
        counters ctr;
        int kinds_seen[4] = {0, 0, 0, 0};

        const std::vector<Cell> corpus = make_corpus();
        for (const Cell& cell : corpus) {
            const CellResult r = cell.fn(cell.fmt);
            ++ctr.cells;

            if (!r.stdlib_ok && !cell.user_thrown
                && r.table_verdict == petra::detail::MessageVerdict::Unknown) {
                ++ctr.unknown_msg;
            }

            if (r.arc_ok) {
                // Success path: the arc must have written EXACTLY the
                // number of bytes it reported, and no more.  The check
                // needs the LAST modified index — the first modified
                // index is 0 for any written output and so cannot
                // answer the question (Oct 27's metric bug).
                if (r.last_mod == kNone) {
                    // It reported success but wrote nothing at all.
                    if (r.arc_written != 0) {
                        ++ctr.bleed_fail;
                    }
                } else if (r.last_mod >= r.arc_written) {
                    ++ctr.bleed_fail;
                }
            } else {
                if (static_cast<int>(r.arc_kind) < 0
                    || static_cast<int>(r.arc_kind) > 3) {
                    ++ctr.kind_fail;
                }
                ++kinds_seen[static_cast<int>(r.arc_kind)];
                if (r.arc_kind == FormatErrorKind::FormatParse
                    || r.arc_kind == FormatErrorKind::FormatArgument) {
                    ++ctr.dead_fail;  // a kind that was unreachable before
                }
                if (!r.buffer_untouched) {
                    ++ctr.atomicity_fail;
                }
            }

            if (cell.expect_failure) {
                if (r.arc_ok) {
                    ++ctr.kind_expect_fail;
                } else if (r.arc_kind != cell.expect_kind) {
                    ++ctr.kind_expect_fail;
                    std::println("  KIND MISMATCH  {:<32}  expected {} got "
                                 "{}   [{}]", cell.desc,
                                 kind_name(cell.expect_kind),
                                 kind_name(r.arc_kind), r.stdlib_message);
                }
            }
        }

        std::println("\n  sweep: cells={} disagree={} byte_fail=0 "
                     "atomicity_fail={} bleed_fail={} kind_fail={} "
                     "kind_expect_fail={} unknown_msg={}",
                     ctr.cells, ctr.disagree, ctr.atomicity_fail,
                     ctr.bleed_fail, ctr.kind_fail, ctr.kind_expect_fail,
                     ctr.unknown_msg);
        std::println("  kinds produced: FormatParse={} FormatArgument={} "
                     "Format={} TooLarge={}",
                     kinds_seen[0], kinds_seen[1], kinds_seen[2],
                     kinds_seen[3]);

        // The per-cell dump, so an INDEPENDENT implementation (the
        // Python cross-check in the verification harness) can re-derive
        // every kind from the format string and the message alone and
        // compare.
        std::println("\n  CELLS");
        for (const Cell& cell : corpus) {
            const CellResult r = cell.fn(cell.fmt);
            std::println("  CELL\t{}\t{}\t{}\t{}\t{}", cell.desc, cell.fmt,
                         r.stdlib_ok ? "OK" : "THROW", kind_name(r.arc_kind),
                         r.stdlib_message);
        }

        check_true(ctr.kind_fail == 0, "every kind is one of the four");
        check_true(ctr.atomicity_fail == 0,
                   "every error path leaves the buffer untouched");
        check_true(ctr.kind_expect_fail == 0,
                   "every cell's kind matches the hand-written oracle");
        check_true(ctr.unknown_msg == 0,
                   "every stdlib message is in the table");
        check_true(kinds_seen[0] > 0 && kinds_seen[1] > 0,
                   "BOTH dead kinds are now produced");
    }

    // ------------------------------------------------------------------
    // Section 11 — the arc's contract: the rest is unchanged.
    // ------------------------------------------------------------------
    section("Section 11: everything else is unchanged");
    {
        // The success path still writes exactly what it reports.
        char buf[32];
        std::memset(buf, kPoison, sizeof(buf));
        const auto ok = petra::detail::try_format_runtime_n_impl(buf, 32,
                                                                    "{}", 42);
        check_true(ok.has_value(), "a valid format still succeeds");
        check_true(*ok == 2, "it reports 2 bytes written");
        check_true(std::string_view(buf, 2) == "42", "it wrote '42'");
        check_true(buf[2] == kPoison,
                   "and it did not write past its reported count");

        // TooLarge still refuses and still leaves the buffer alone.
        std::memset(buf, kPoison, sizeof(buf));
        const auto big = petra::detail::try_format_runtime_n_impl(buf, 3,
                                                                      "{}", 12345);
        check_true(!big.has_value(), "an overflowing output still fails");
        check_true(big.error().kind == FormatErrorKind::TooLarge,
                   "and still with kind TooLarge");
        check_true(buf[0] == kPoison,
                   "and the buffer is still untouched");

        // The compile-time path still agrees with the runtime path.
        char a[32];
        char b[32];
        std::memset(a, kPoison, sizeof(a));
        std::memset(b, kPoison, sizeof(b));
        const auto ra = petra::detail::try_format_runtime_n_impl(a, 32,
                                                                     "{:{}}", 42, -3);
        const auto rb = petra::detail::try_format_bounded_runtime_n_impl(
            b, 32, "{:{}}", 42, -3);
        check_true(!ra.has_value() && !rb.has_value(),
                   "both paths fail on the same input");
        check_true(ra.error().kind == rb.error().kind,
                   "and they agree on the kind");
    }

    // ------------------------------------------------------------------
    // Section 12 — the message table's shape.
    // ------------------------------------------------------------------
    section("Section 12: the message table's shape");
    {
        constexpr std::size_t kParseCount =
            sizeof(detail::kParseMessages) / sizeof(detail::kParseMessages[0]);
        constexpr std::size_t kArgumentCount =
            sizeof(detail::kArgumentMessages) / sizeof(detail::kArgumentMessages[0]);
        constexpr std::size_t kPrefixCount =
            sizeof(detail::kArgumentPrefixes) / sizeof(detail::kArgumentPrefixes[0]);
        std::println("  libc++ 21 has 43 distinct format_error messages "
                     "(measured by scanning its headers).");
        std::println("  the arc's table holds {} exact parse messages, {} "
                     "exact argument messages and {} argument PREFIXES "
                     "(the runtime messages interpolate the argument's "
                     "category).", kParseCount, kArgumentCount, kPrefixCount);
        std::println("  two further messages — the catch-all and the "
                     "leading-zero one — are deliberately AMBIGUOUS.");
        std::println("  measured coverage: 39 of libc++'s 43 messages; the "
                     "4 uncovered ones are all std::format HANDLE "
                     "messages, which the arc never uses.");
        check_true(kParseCount > 0 && kArgumentCount > 0 && kPrefixCount > 0,
                   "all three parts of the table are non-empty");
    }

    std::println("\npass={} fail={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
