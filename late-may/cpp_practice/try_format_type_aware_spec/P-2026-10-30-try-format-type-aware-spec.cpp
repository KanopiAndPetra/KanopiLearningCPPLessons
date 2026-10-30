// P-2026-10-30 — try_format_type_aware_spec:
//
// Closes Oct 28's "Where we go next" item (2) verbatim:
//
//   item (2) — "The grammar scanner is type-agnostic, and that is both its
//               strength and its limit.  It cannot know that "{:.2}" is
//               wrong for an `int`.  A type-aware scanner would need to
//               know each argument's category — which the *runtime* path
//               does not have, because `std::make_format_args` erases it.
//               Worth measuring whether a `constexpr` type-category table
//               is worth the complexity."
//
// THE QUESTION: Oct 28's scanner answers "is this format string
// well-formed?" and nothing else.  Everything the STRING cannot decide
// had to be settled by matching libc++'s MESSAGE TEXT, and the message
// table was 39-of-43 libc++-specific strings.  Can the arc decide the
// same questions from the ARGUMENT'S TYPE instead — and if so, what is
// the price?
//
// MEASURED ANSWER: yes, and the premise of Oct 28's item (2) was wrong.
// The runtime path DOES have the argument categories — not from
// `std::make_format_args` (which does erase them) but from the ARC'S
// OWN TEMPLATE.  `try_format_runtime_n_impl` is a template over
// `Args...`, so the category of every argument is a COMPILE-TIME fact
// long before the argument is handed to the stdlib.  The erasure
// happens downstream of the arc, not upstream of it.
//
// KEY FINDING #1 — libc++ VALIDATES IN TWO PHASES, AND THE PHASE-1 MASK
// IS DECIDED BY THE ARGUMENT'S CATEGORY.
//
//   PHASE 1 (parse):  the category picks the mask of options the parser
//                     will even CONSUME.  libc++'s `__fields_integral`,
//                     `__fields_floating_point`, `__fields_string` and
//                     `__fields_pointer` are exactly those masks.  An
//                     option OUTSIDE the mask is not parsed at all — it
//                     is left in the input, and the leftover text trips
//                     the "should consume the input" check.
//
//   PHASE 2 (validate): the SELECTED PRESENTATION TYPE can narrow the
//                     mask further.  libc++ calls `__validate` with
//                     `__fields_bool` = {locale, type} for the
//                     "char-like" presentation types, which is what
//                     rejects `{:+c}` on an `int`.
//
//   PHASE 3 (type):  the presentation type itself must be legal for the
//                     category (`__process_parsed_*`).
//
// The phase-1 masks are libc++'s; the phase-2 narrowing is libc++'s;
// but phase 2 is where libc++ and the STANDARD DISAGREE (see #3).
//
// KEY FINDING #2 — A constexpr TYPE-CATEGORY TABLE IS CHEAP, AND THE
// TWO-PHASE MODEL REPRODUCES libc++ EXACTLY.
//
// The category of an argument is one enum value, computable with a
// `constexpr` function over the argument's TYPE.  The whole validator
// is a handful of `constexpr` tables.  And the model is not a rough
// approximation: over a 1,596-cell grid (7 categories x 228 specs)
// the two-phase model predicts libc++ 21's accept/reject decision
// 1596/1596 — EXACTLY, not approximately.
//
// KEY FINDING #3 — libc++ 21 AND THE STANDARD DISAGREE IN EXACTLY 10
// CELLS, AND IT IS A KNOWN, STILL-OPEN UPSTREAM BUG.
//
// The standard says the sign / alternate-form / zero-padding options
// are valid "for arithmetic types other than charT and bool or when an
// integer presentation type is specified", and `c` IS an integer
// presentation type for `int` (Table 107).  So `"{:+c}"` on an `int`
// is VALID per the standard.  libc++ rejects it, because phase 2
// narrows the mask to `__fields_bool` whenever the type is `c` — the
// narrowing that is right for `charT` is applied to `int` as well.
//
// The divergence is exactly 10 cells: {sign, alternate, zero-padding} x
// {+, -, space, #, 0} x {int, long}.  Tracked upstream as
// llvm/llvm-project#106136 and microsoft/STL#4918.
//
// INDEPENDENT CONFIRMATION: the same 1,596-cell grid was run under
// libstdc++ 15 (g++-15, a DIFFERENT implementation).  The
// STANDARD-derived model agrees with libstdc++ 1596/1596 and with
// libc++ 1586/1596, differing on precisely those 10 cells.  So the
// model is right, libc++ is the outlier, and the outlier is upstream's
// problem, not the arc's.
//
// KEY FINDING #4 — THE TYPE-AWARE SCANNER REPLACES THE MESSAGE TABLE
// FOR THE WHOLE SPEC-VS-ARGUMENT CLASS.
//
// Over the 1,596-cell grid, Oct 28's classifier had to consult
// libc++'s message text for EVERY one of the 1,172 rejects, because
// its Stage A scanner is type-agnostic and therefore never fires on a
// syntactically valid spec.  Today's scanner classifies 1,172/1,172 of
// those rejects structurally — reading ZERO libc++ messages — and
// agrees with the message table's verdicts on all 1,172.
//
// The message table is NOT deleted: it is demoted to the fallback for
// failures that are not about the spec's shape at all — a value that
// does not fit the selected presentation type ("Integral value outside
// the range of the char type"), an argument index past the end of the
// argument list, and a user's own formatter throwing.
//
// KEY FINDING #5 — THE COMPILE-TIME PATH INHERITS THE DIVERGENCE.
//
// `std::format_string<Args...>` validates at compile time using
// libc++'s rules, so on THIS toolchain `"{:+c}"` on an `int` is a
// COMPILE ERROR even though the standard says it is valid.  The arc's
// compile-time path therefore cannot accept a spec the standard
// accepts.  That is a property of the LINKED STANDARD LIBRARY, not of
// the arc — and it is the strongest possible argument for keeping the
// arc's own validator independent of the stdlib's rules.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `FormatErrorKind` (the
// four kinds, their order, and their meanings are exactly as they have
// been since Sep 6), the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, and the buffer-atomicity behaviour.
//
// No pad flags: the pad axis is orthogonal to the error taxonomy, so
// the pad options are NOT reproduced here.

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError / petra::FormatErrorKind
//           (verbatim from Sep 6 through Oct 28).
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
//           (verbatim from Oct 11 .. Oct 28).
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
//           (verbatim from Oct 11 .. Oct 28).
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
// Part 4 — NEW TODAY: petra::detail::ArgCategory, the TYPE-CATEGORY TABLE.
//
// Oct 28's item (2) asked whether a constexpr type-category table is
// worth the complexity.  This is the table: one enum, one constexpr
// function, no machinery.
//
// The categories are the STANDARD's, not libc++'s: "bool", "charT",
// "integral types other than bool and charT", "floating-point",
// "string", "pointer".  The standard's rules are phrased in terms of
// exactly these distinctions, so the table is the vocabulary the
// rules are written in.
//
// `UserDefined` is the honest escape hatch: a type with a user's own
// `std::formatter` specialization has NO rules we can know, so the
// validator ABSTAINS for it rather than guessing.
// ============================================================================

namespace petra {
namespace detail {

enum class ArgCategory : unsigned char {
    Boolean,
    Character,       // charT
    Integral,        // integral types other than bool and charT
    FloatingPoint,
    String,
    Pointer,
    UserDefined,     // a formatter specialization: rules unknown
};

[[nodiscard]] constexpr std::string_view category_name(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:      return "bool";
    case ArgCategory::Character:    return "charT";
    case ArgCategory::Integral:     return "integral";
    case ArgCategory::FloatingPoint:return "floating-point";
    case ArgCategory::String:       return "string";
    case ArgCategory::Pointer:      return "pointer";
    case ArgCategory::UserDefined:  return "user-defined";
    }
    return "?";
}

// The category of a formatting argument, computed from its TYPE.
//
// Notes on the edges, all of them measured (Section 1):
//   - `char` is charT; `signed char` and `unsigned char` are NOT
//     (they are "integral types other than bool and charT"), which is
//     why `signed char` and `unsigned char` are Integral here and not
//     Character.
//   - `wchar_t`, `char8_t`, `char16_t` and `char32_t` are NOT
//     formattable in a narrow (char) format context: libc++ 21's
//     argument store static_asserts "the supplied type is not
//     formattable".  They are classified UserDefined so the validator
//     abstains rather than inventing rules for a type the stdlib
//     rejects outright.
//   - only `void*`, `const void*` and `std::nullptr_t` are the
//     pointer category.  `int*` has a `std::formatter` name but it is
//     not constructible, so the argument store rejects it too.
template <class T>
[[nodiscard]] constexpr ArgCategory category_of() noexcept {
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, bool>) {
        return ArgCategory::Boolean;
    } else if constexpr (std::is_same_v<U, char>) {
        return ArgCategory::Character;
    } else if constexpr (std::is_same_v<U, wchar_t> || std::is_same_v<U, char8_t>
                         || std::is_same_v<U, char16_t> || std::is_same_v<U, char32_t>) {
        return ArgCategory::UserDefined;
    } else if constexpr (std::is_integral_v<U>) {
        return ArgCategory::Integral;
    } else if constexpr (std::is_floating_point_v<U>) {
        return ArgCategory::FloatingPoint;
    } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>
                         || std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
        return ArgCategory::String;
    } else if constexpr (std::is_array_v<U>
                         && std::is_same_v<std::remove_extent_t<U>, char>) {
        return ArgCategory::String;
    } else if constexpr (std::is_same_v<U, void*> || std::is_same_v<U, const void*>
                         || std::is_same_v<U, std::nullptr_t>) {
        return ArgCategory::Pointer;
    } else {
        return ArgCategory::UserDefined;
    }
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — NEW TODAY: the TWO-PHASE MASK MODEL.
//
// The model is four constexpr tables and two constexpr functions.
//
// The OPTION bits are the options the standard's format-spec grammar
// can carry that are CATEGORY-DEPENDENT.  Fill-and-align and width are
// NOT here: they are legal for every category.
//
//   SIGN        the '+', '-' or ' ' option
//   ALTERNATE   the '#' option
//   ZERO        the '0' (zero-padding) option
//   PRECISION   the '.' precision option
//   LOCALE      the 'L' option
//
// PHASE 1 is libc++'s `__fields_*` tables, verbatim in meaning:
//
//   bool / charT / integral   __fields_integral
//                             {sign, alternate, zero, locale}
//   floating-point            __fields_floating_point
//                             {sign, alternate, zero, precision, locale}
//   string                    __fields_string    {precision}
//   pointer                   __fields_pointer   {zero}
//
// Note that `type` is legal for every category (every `__fields_*` has
// `__type_ = true`), so it is not a mask bit: the type CHARACTER's
// legality is phase 3.
//
// PHASE 2 is the narrowing.  libc++ narrows the mask to
// `__fields_bool` = {locale} for the "char-like" presentation types.
// The STANDARD's rule for the same options is different in exactly one
// place, and that difference is the whole of KEY FINDING #3:
//
//   standard: sign/alternate/zero are valid "for arithmetic types
//             other than charT and bool OR when an integer
//             presentation type is specified".  For `int`, `c` IS an
//             integer presentation type (Table 107), so the standard
//             ALLOWS them.
//   libc++:   narrows to {locale} whenever the type is `c`, for
//             integral types too.  So libc++ REJECTS them.
//
// The two policies therefore differ by ONE boolean, and the lesson
// carries both so the divergence can be measured rather than argued.
// ============================================================================

namespace petra {
namespace detail {

enum class Opt : unsigned char {
    None       = 0u,
    Sign       = 1u << 0,
    Alternate  = 1u << 1,
    ZeroPad    = 1u << 2,
    Precision  = 1u << 3,
    Locale     = 1u << 4,
};

using OptMask = unsigned char;

[[nodiscard]] constexpr OptMask mask_of(Opt o) noexcept {
    return static_cast<OptMask>(o);
}

[[nodiscard]] constexpr bool has(OptMask m, Opt o) noexcept {
    return (m & mask_of(o)) != 0;
}

// Which standard's rules to enforce.
enum class SpecPolicy : unsigned char {
    Standard,   // [format.string.std] as written
    Libcxx21,   // libc++ 21's phase-2 narrowing, bug included
};

[[nodiscard]] constexpr OptMask phase1_mask(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
    case ArgCategory::Character:
    case ArgCategory::Integral:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Locale);
    case ArgCategory::FloatingPoint:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Precision) | mask_of(Opt::Locale);
    case ArgCategory::String:
        return mask_of(Opt::Precision);
    case ArgCategory::Pointer:
        return mask_of(Opt::ZeroPad);
    case ArgCategory::UserDefined:
        return mask_of(Opt::None);
    }
    return mask_of(Opt::None);
}

// The integer presentation types available per category (the standard's
// Tables 107/108/109).  A selected type in this set is what makes the
// sign/alternate/zero options valid for bool and charT.
[[nodiscard]] constexpr bool is_integer_presentation_type(ArgCategory c, char t) noexcept {
    switch (c) {
    case ArgCategory::Boolean:   // Table 109
        return t == 'b' || t == 'B' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    case ArgCategory::Character: // Table 108 (charT): no 'c'
        return t == 'b' || t == 'B' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    case ArgCategory::Integral:  // Table 107
        return t == 'b' || t == 'B' || t == 'c' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    default:
        return false;
    }
}

// The presentation types legal for a category (phase 3).
[[nodiscard]] constexpr bool is_legal_type_char(ArgCategory c, char t) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
        return t == '\0' || t == 's' || t == 'b' || t == 'B' || t == 'o' || t == 'd'
            || t == 'x' || t == 'X';
    case ArgCategory::Character:
        return t == '\0' || t == 'c' || t == '?' || t == 'b' || t == 'B' || t == 'o'
            || t == 'd' || t == 'x' || t == 'X';
    case ArgCategory::Integral:
        return t == '\0' || t == 'b' || t == 'B' || t == 'o' || t == 'd' || t == 'x'
            || t == 'X' || t == 'c';
    case ArgCategory::FloatingPoint:
        return t == '\0' || t == 'a' || t == 'A' || t == 'e' || t == 'E' || t == 'f'
            || t == 'F' || t == 'g' || t == 'G';
    case ArgCategory::String:
        return t == '\0' || t == 's' || t == '?';
    case ArgCategory::Pointer:
        return t == '\0' || t == 'p' || t == 'P';
    case ArgCategory::UserDefined:
        return true;
    }
    return false;
}

// Is the type character legal for ANY category?  A character that is
// not is a malformed spec for every argument, which is a DIFFERENT
// failure from "legal type, wrong argument".
[[nodiscard]] constexpr bool is_known_type_char(char t) noexcept {
    if (t == '\0') return true;
    for (const ArgCategory c : {ArgCategory::Boolean, ArgCategory::Character,
                                 ArgCategory::Integral, ArgCategory::FloatingPoint,
                                 ArgCategory::String, ArgCategory::Pointer}) {
        if (is_legal_type_char(c, t)) return true;
    }
    return false;
}

// PHASE 2 — the mask that survives once the presentation type is known.
//
// libc++ 21: narrow to {locale} for bool with type "" or 's'; for
//   charT with type "", 'c' or '?'; and for integral types with type
//   'c' (the divergent rule).
// Standard:   narrow to {locale} for bool with a type that is not an
//   integer presentation type; for charT likewise; and NEVER for
//   integral types other than bool and charT, because those are
//   "arithmetic types other than charT and bool" and the sign /
//   alternate / zero options are valid for them unconditionally.
[[nodiscard]] constexpr OptMask phase2_mask(ArgCategory c, char type) noexcept {
    bool narrow = false;
    if (c == ArgCategory::Boolean) {
        narrow = !is_integer_presentation_type(c, type);
    } else if (c == ArgCategory::Character) {
        narrow = !is_integer_presentation_type(c, type);
    } else if (c == ArgCategory::Integral) {
        narrow = (type == 'c');
    }

    if (!narrow) return phase1_mask(c);
    return phase1_mask(c) & mask_of(Opt::Locale);
}

// The standard's base permission, independent of any selected type:
// sign/alternate/zero for arithmetic types other than charT and bool;
// zero also for pointers; precision for floating-point and string;
// locale for all arithmetic types (bool and charT ARE arithmetic).
[[nodiscard]] constexpr OptMask standard_base_mask(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
    case ArgCategory::Character:
        return mask_of(Opt::Locale);
    case ArgCategory::Integral:
        // No Precision: the standard allows a precision only for
        // floating-point and string types.
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Locale);
    case ArgCategory::FloatingPoint:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Locale) | mask_of(Opt::Precision);
    case ArgCategory::String:
        return mask_of(Opt::Precision);
    case ArgCategory::Pointer:
        return mask_of(Opt::ZeroPad);
    case ArgCategory::UserDefined:
        return mask_of(Opt::None);
    }
    return mask_of(Opt::None);
}

// The standard's effective mask for (category, selected type): the
// base mask, widened by the integer-presentation-type clause when the
// selected type is one.
[[nodiscard]] constexpr OptMask standard_effective_mask(ArgCategory c, char type) noexcept {
    OptMask m = standard_base_mask(c);
    if (is_integer_presentation_type(c, type)) {
        m = static_cast<OptMask>(m | mask_of(Opt::Sign) | mask_of(Opt::Alternate)
                                     | mask_of(Opt::ZeroPad));
    }
    return m;
}

// The effective mask for (category, selected presentation type) under
// the chosen policy.  This is the ONE function the validator consults:
// the standard and libc++ 21 differ in exactly one rule, and the two
// mask families are computed from the same tables.
[[nodiscard]] constexpr OptMask effective_mask(ArgCategory c, char type,
                                                SpecPolicy policy) noexcept {
    if (policy == SpecPolicy::Standard) return standard_effective_mask(c, type);
    return phase2_mask(c, type);
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 6 — NEW TODAY: petra::detail::SpecParser and petra::detail::SpecValidator.
//
// The parser walks ONE format-spec (the text between ':' and '}') and
// reports which options it carries and which presentation type it
// selects.
//
// The parser is MASK-AWARE, and that is the whole point.  libc++ parses
// an option only if the category's mask includes it; an option outside
// the mask is left in the input, where it either trips the
// "should consume the input" check or is misread as a WIDTH (which is
// how "{:0}" on a string becomes "The width option should not have a
// leading zero").  A parser that consumed every option regardless of
// the mask would report a different reason for the same failure.
//
// The validator then applies the phase-2 / phase-3 rules and returns a
// verdict:
//
//   None             the spec is fine for this category
//   Malformed        the spec is broken for EVERY argument
//   TypeNotAllowed   legal presentation type, wrong argument
//   OptionNotAllowed legal option, wrong argument
//   IndexOutOfRange  the field names an argument that does not exist
//   WidthArgNotInteger  a dynamic width/precision argument is not an
//                     integer type
//
// The split between Malformed and the two "NotAllowed" verdicts is the
// arc's FormatParse / FormatArgument line.
// ============================================================================

namespace petra {
namespace detail {

enum class SpecFailure : unsigned char {
    None,
    Malformed,
    TypeNotAllowed,
    OptionNotAllowed,
    IndexOutOfRange,
    WidthArgNotInteger,
};

struct SpecVerdict {
    SpecFailure failure{SpecFailure::None};
    Opt option{Opt::None};
    char type_char{'\0'};
    const char* detail{""};

    [[nodiscard]] bool accepted() const noexcept {
        return failure == SpecFailure::None;
    }
    // A malformed spec is the FORMAT STRING's fault; everything else
    // here is the ARGUMENT's fault.
    [[nodiscard]] bool is_parse_failure() const noexcept {
        return failure == SpecFailure::Malformed;
    }
};

[[nodiscard]] constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}
[[nodiscard]] constexpr bool is_align(char c) noexcept {
    return c == '<' || c == '>' || c == '^';
}
[[nodiscard]] constexpr bool is_sign(char c) noexcept {
    return c == '+' || c == '-' || c == ' ';
}

[[nodiscard]] constexpr const char* option_name(Opt o) noexcept {
    switch (o) {
    case Opt::Sign:      return "sign";
    case Opt::Alternate: return "alternate form";
    case Opt::ZeroPad:   return "zero-padding";
    case Opt::Precision: return "precision";
    case Opt::Locale:    return "locale-specific form";
    case Opt::None:      return "";
    }
    return "";
}

// The parsed shape of one format-spec.
struct ParsedSpec {
    OptMask options{mask_of(Opt::None)};
    char type_char{'\0'};
    bool has_nested{false};       // a dynamic width or precision
    std::size_t nested_arg{0};    // the argument index it names
    bool nested_is_manual{false};
};

// Parse one format-spec, given the category's PHASE-1 mask.
//
// `spec` is the text between ':' and the field's '}'.  The parser is
// mask-aware: an option the mask does not include is NOT consumed and
// is reported as an option the argument does not accept.
[[nodiscard]] constexpr ParsedSpec
parse_spec(std::string_view spec, ArgCategory category, SpecFailure& failure,
           Opt& bad_option) noexcept {
    ParsedSpec out{};
    const OptMask m = phase1_mask(category);
    std::size_t i = 0;
    const std::size_t n = spec.size();

    // fill-and-align.  Legal for every category.
    if (i + 1 < n && is_align(spec[i + 1])) {
        if (spec[i] == '{') {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        i += 2;
    } else if (i < n && is_align(spec[i])) {
        ++i;
    }

    // sign.
    if (i < n && is_sign(spec[i])) {
        if (!has(m, Opt::Sign)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Sign;
            return out;
        }
        out.options = static_cast<OptMask>(out.options | mask_of(Opt::Sign));
        ++i;
    }

    // alternate form.
    if (i < n && spec[i] == '#') {
        if (!has(m, Opt::Alternate)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Alternate;
            return out;
        }
        out.options = static_cast<OptMask>(out.options | mask_of(Opt::Alternate));
        ++i;
    }

    // zero-padding.  If the category does not accept it, the '0' stays
    // in the input and libc++ reads it as a WIDTH — which is why
    // "{:0}" on a string fails with "The width option should not have
    // a leading zero" rather than "does not allow zero-padding".
    if (i < n && spec[i] == '0') {
        if (!has(m, Opt::ZeroPad)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::ZeroPad;
            return out;
        }
        out.options = static_cast<OptMask>(out.options | mask_of(Opt::ZeroPad));
        ++i;
    }

    // width.
    if (i < n && spec[i] == '0') {
        failure = SpecFailure::Malformed;
        bad_option = Opt::None;
        return out;
    }
    if (i < n && is_digit(spec[i])) {
        while (i < n && is_digit(spec[i])) ++i;
    } else if (i < n && spec[i] == '{') {
        // dynamic width: a nested replacement field.
        ++i;
        const std::size_t id_start = i;
        while (i < n && is_digit(spec[i])) ++i;
        out.has_nested = true;
        out.nested_is_manual = (i > id_start);
        out.nested_arg = 0;
        for (std::size_t k = id_start; k < i; ++k) {
            const unsigned digit = static_cast<unsigned>(spec[k] - '0');
            if (out.nested_arg > (static_cast<std::size_t>(-1) - digit) / 10u) {
                failure = SpecFailure::IndexOutOfRange;
                bad_option = Opt::None;
                return out;
            }
            out.nested_arg = out.nested_arg * 10u + digit;
        }
        if (i >= n || spec[i] != '}') {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        ++i;
    }

    // precision.
    //
    // The GRAMMAR of a precision is checked FIRST, independently of the
    // mask: a '.' with no value after it is malformed for EVERY
    // argument, so it is a FormatParse, not an argument mismatch.  Only
    // a WELL-FORMED precision can be rejected for being disallowed.
    if (i < n && spec[i] == '.') {
        ++i;
        bool well_formed = false;
        if (i < n && is_digit(spec[i])) {
            while (i < n && is_digit(spec[i])) ++i;
            well_formed = true;
        } else if (i < n && spec[i] == '{') {
            // dynamic precision: a nested replacement field.
            ++i;
            const std::size_t id_start = i;
            while (i < n && is_digit(spec[i])) ++i;
            out.has_nested = true;
            out.nested_is_manual = (i > id_start);
            out.nested_arg = 0;
            for (std::size_t k = id_start; k < i; ++k) {
                const unsigned digit = static_cast<unsigned>(spec[k] - '0');
                if (out.nested_arg > (static_cast<std::size_t>(-1) - digit) / 10u) {
                    failure = SpecFailure::IndexOutOfRange;
                    bad_option = Opt::None;
                    return out;
                }
                out.nested_arg = out.nested_arg * 10u + digit;
            }
            if (i >= n || spec[i] != '}') {
                failure = SpecFailure::Malformed;
                bad_option = Opt::None;
                return out;
            }
            ++i;
            well_formed = true;
        }

        if (!well_formed) {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        if (!has(m, Opt::Precision)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Precision;
            return out;
        }
        out.options = static_cast<OptMask>(out.options | mask_of(Opt::Precision));
    }

    // locale-specific form.
    if (i < n && spec[i] == 'L') {
        if (!has(m, Opt::Locale)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Locale;
            return out;
        }
        out.options = static_cast<OptMask>(out.options | mask_of(Opt::Locale));
        ++i;
    }

    // presentation type.
    if (i < n && spec[i] != '}') {
        out.type_char = spec[i];
        ++i;
    }

    if (i != n) {
        failure = SpecFailure::Malformed;
        bad_option = Opt::None;
        return out;
    }
    return out;
}

// Validate one parsed spec against one argument category.
//
// The mask that decides the options is the POLICY's: the standard's
// effective mask, or libc++ 21's two-phase mask.  Both are computed
// from the same tables, so the divergence is ONE rule, not two models.
[[nodiscard]] constexpr SpecVerdict
validate_spec(ArgCategory category, const ParsedSpec& spec,
              SpecPolicy policy) noexcept {
    SpecVerdict v{};
    v.type_char = spec.type_char;

    if (category == ArgCategory::UserDefined) {
        // We know nothing about a user's formatter.  Abstain.
        return v;
    }

    // Phase 3: the presentation type itself.
    if (!is_legal_type_char(category, spec.type_char)) {
        v.failure = is_known_type_char(spec.type_char)
                         ? SpecFailure::TypeNotAllowed
                         : SpecFailure::Malformed;
        v.detail = "the presentation type is not valid for this argument";
        return v;
    }

    // Phase 2: the mask.
    const OptMask allowed = effective_mask(category, spec.type_char, policy);

    for (const Opt o : {Opt::Sign, Opt::Alternate, Opt::ZeroPad, Opt::Precision, Opt::Locale}) {
        if (!has(spec.options, o)) continue;
        if (!has(allowed, o)) {
            v.failure = SpecFailure::OptionNotAllowed;
            v.option = o;
            v.detail = "the argument does not accept this option";
            return v;
        }
    }
    return v;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 7 — NEW TODAY: petra::detail::TypeAwareScanner.
//
// Oct 28's GrammarScanner asked ONE question: "is this format string
// well-formed?".  Today's scanner asks the full question: "for each
// replacement field, is this spec valid for the argument it names?"
//
// It needs two things Oct 28's did not:
//
//   - the CATEGORY of every argument, and
//   - which argument each replacement field names.
//
// The second is the fiddly part, and it is where the standard's rules
// about argument numbering live:
//
//   - a field with an explicit index is MANUAL; a field without is
//     AUTOMATIC, and the two may not be mixed (in either order, and
//     the mixing rule applies to NESTED fields too — "{0:{}}" is an
//     error because the nested field is automatic inside a manual
//     field);
//   - in automatic mode the indices are assigned in order of
//     APPEARANCE, so an outer field takes the lower index and the
//     nested width/precision field inside it takes the next one
//     (measured: "{:{}}" with (42, 5) formats 42 with width 5, and
//     "{}{:{}}" with (42, 5, 7) formats 42 then 5-with-width-7).
//
// The scanner therefore tracks a single automatic counter, shared by
// outer and nested fields, and consumes one index per field in
// appearance order.
// ============================================================================

namespace petra {
namespace detail {

enum class ScanFailure : unsigned char {
    None,
    Malformed,          // the format string is broken for every argument
    TypeNotAllowed,     // legal presentation type, wrong argument
    OptionNotAllowed,   // legal option, wrong argument
    IndexOutOfRange,    // the field names an argument that does not exist
    WidthArgNotInteger, // a dynamic width/precision argument is not an integer
    MixedNumbering,     // manual and automatic numbering are mixed
};

struct ScanResult {
    ScanFailure failure{ScanFailure::None};
    Opt option{Opt::None};
    char type_char{'\0'};
    std::size_t field{0};       // which replacement field (0-based)
    std::size_t arg_index{0};   // which argument it names
    const char* detail{""};

    [[nodiscard]] bool ok() const noexcept {
        return failure == ScanFailure::None;
    }
    [[nodiscard]] bool is_parse_failure() const noexcept {
        return failure == ScanFailure::Malformed || failure == ScanFailure::MixedNumbering;
    }
};

// The category table the scanner consults.  A null category pointer
// means "no argument information", and the scanner then checks SYNTAX
// only (which is Oct 28's behaviour, and what the compile-time path
// needs).
struct CategoryTable {
    const ArgCategory* categories{nullptr};
    std::size_t count{0};

    [[nodiscard]] constexpr ArgCategory at(std::size_t i) const noexcept {
        if (categories == nullptr || i >= count) return ArgCategory::UserDefined;
        return categories[i];
    }
    [[nodiscard]] constexpr bool known() const noexcept {
        return categories != nullptr;
    }
};

class TypeAwareScanner {
public:
    constexpr TypeAwareScanner(std::string_view fmt, CategoryTable cats,
                               SpecPolicy policy) noexcept
        : fmt_(fmt), cats_(cats), policy_(policy) {}

    [[nodiscard]] constexpr ScanResult run() noexcept {
        ScanResult r{};
        std::size_t i = 0;
        while (i < fmt_.size()) {
            const char c = fmt_[i];
            if (c == '{') {
                if (i + 1 < fmt_.size() && fmt_[i + 1] == '{') {
                    i += 2;
                    continue;
                }
                ++i;
                if (!scan_field(i, r)) return r;
                continue;
            }
            if (c == '}') {
                if (i + 1 < fmt_.size() && fmt_[i + 1] == '}') {
                    i += 2;
                    continue;
                }
                r.failure = ScanFailure::Malformed;
                r.detail = "a '}' that is not part of an escape sequence";
                return r;
            }
            ++i;
        }
        return r;
    }

private:
    // Parse an argument index.  `manual` reports whether digits were
    // present; `value` receives the index (0 when automatic).
    constexpr bool parse_arg_id(std::size_t& i, bool& manual,
                                std::size_t& value) noexcept {
        const std::size_t start = i;
        while (i < fmt_.size() && is_digit(fmt_[i])) ++i;
        manual = (i > start);
        value = 0;
        if (manual) {
            for (std::size_t k = start; k < i; ++k) {
                const unsigned digit = static_cast<unsigned>(fmt_[k] - '0');
                if (value > (static_cast<std::size_t>(-1) - digit) / 10u) {
                    value = static_cast<std::size_t>(-1);
                    return true;  // overflow: out of range, reported later
                }
                value = value * 10u + digit;
            }
        }
        return true;
    }

    // The numbering mode may not be mixed, in either order, and the
    // rule applies to nested fields as well.
    constexpr bool note_numbering_mode(bool manual) noexcept {
        if (manual) {
            if (auto_seen_) {
                mixed_ = true;
                return false;
            }
            manual_seen_ = true;
        } else {
            if (manual_seen_) {
                mixed_ = true;
                return false;
            }
            auto_seen_ = true;
        }
        return true;
    }

    constexpr bool scan_field(std::size_t& i, ScanResult& r) noexcept {
        const std::size_t field_no = field_count_++;

        bool manual = false;
        std::size_t index = 0;
        if (!parse_arg_id(i, manual, index)) return false;
        if (!note_numbering_mode(manual)) {
            r.failure = ScanFailure::MixedNumbering;
            r.field = field_no;
            r.detail = "manual and automatic argument numbering are mixed";
            return false;
        }
        if (!manual) {
            index = next_auto_++;
        }

        if (i >= fmt_.size()) {
            r.failure = ScanFailure::Malformed;
            r.field = field_no;
            r.detail = "the replacement field has no terminator";
            return false;
        }

        if (fmt_[i] == ':') {
            ++i;
            const std::size_t spec_start = i;
            // Find the end of the spec: the field's '}'.  A nested
            // replacement field's '}' is consumed by the spec parser,
            // so scan for the LAST '}' before the field's terminator.
            std::size_t depth = 0;
            std::size_t j = i;
            while (j < fmt_.size()) {
                if (fmt_[j] == '{') {
                    ++depth;
                } else if (fmt_[j] == '}') {
                    if (depth == 0) break;
                    --depth;
                }
                ++j;
            }
            if (j >= fmt_.size()) {
                r.failure = ScanFailure::Malformed;
                r.field = field_no;
                r.detail = "the replacement field has no terminator";
                return false;
            }
            const std::string_view spec = fmt_.substr(spec_start, j - spec_start);

            const ArgCategory cat = cats_.at(index);
            SpecFailure sf = SpecFailure::None;
            Opt bad = Opt::None;
            const ParsedSpec parsed = parse_spec(spec, cat, sf, bad);
            if (sf != SpecFailure::None) {
                r.failure = to_scan_failure(sf);
                r.option = bad;
                r.field = field_no;
                r.arg_index = index;
                // A MALFORMED spec is broken for EVERY argument — it is a
                // grammar error, and it is a parse failure however the
                // argument is typed.  An OPTION or TYPE that the
                // argument does not accept is an argument failure: the
                // same spec is fine for a different argument.
                r.detail = (sf == SpecFailure::Malformed)
                               ? "the format spec is malformed"
                               : "the format spec is not valid for this argument";
                return false;
            }

            // Phase 2 / phase 3.
            const SpecVerdict v = validate_spec(cat, parsed, policy_);
            if (!v.accepted()) {
                r.failure = to_scan_failure(v.failure);
                r.option = v.option;
                r.type_char = v.type_char;
                r.field = field_no;
                r.arg_index = index;
                r.detail = v.detail;
                return false;
            }

            // The dynamic width/precision argument must exist and must
            // be an integer type.
            if (parsed.has_nested) {
                // The nested field's own numbering mode must agree with
                // the outer field's: a manual index inside an automatic
                // field (or the reverse) is the mixed-numbering error.
                // libc++ reports it either way round:
                //   manual outer, automatic nested -> "Using automatic
                //     argument numbering in manual argument numbering mode"
                //   automatic outer, manual nested -> "Using manual
                //     argument numbering in automatic argument numbering mode"
                if (parsed.nested_is_manual != manual) {
                    r.failure = ScanFailure::MixedNumbering;
                    r.field = field_no;
                    r.detail = "manual and automatic argument numbering are mixed";
                    return false;
                }

                std::size_t nested_index = parsed.nested_arg;
                if (!parsed.nested_is_manual) {
                    // The nested field takes the NEXT automatic index
                    // after the outer field's own.
                    nested_index = next_auto_++;
                }

                if (cats_.known() && nested_index >= cats_.count) {
                    r.failure = ScanFailure::IndexOutOfRange;
                    r.field = field_no;
                    r.arg_index = nested_index;
                    r.detail = "the argument index is past the end of the argument list";
                    return false;
                }
                const ArgCategory ncat = cats_.at(nested_index);
                if (cats_.known() && ncat != ArgCategory::UserDefined
                    && ncat != ArgCategory::Integral) {
                    r.failure = ScanFailure::WidthArgNotInteger;
                    r.field = field_no;
                    r.arg_index = nested_index;
                    r.detail = "a dynamic width or precision must be an integer argument";
                    return false;
                }
            }

            i = j + 1;  // past the field's '}'
            return true;
        }

        if (fmt_[i] == '}') {
            ++i;
            // A field with no spec still names an argument.
            if (cats_.known() && index >= cats_.count) {
                r.failure = ScanFailure::IndexOutOfRange;
                r.field = field_no;
                r.arg_index = index;
                r.detail = "the argument index is past the end of the argument list";
                return false;
            }
            return true;
        }

        r.failure = ScanFailure::Malformed;
        r.field = field_no;
        r.detail = "the argument index is not terminated by ':' or '}'";
        return false;
    }

    [[nodiscard]] static constexpr ScanFailure to_scan_failure(SpecFailure sf) noexcept {
        switch (sf) {
        case SpecFailure::None:              return ScanFailure::None;
        case SpecFailure::Malformed:         return ScanFailure::Malformed;
        case SpecFailure::TypeNotAllowed:    return ScanFailure::TypeNotAllowed;
        case SpecFailure::OptionNotAllowed:  return ScanFailure::OptionNotAllowed;
        case SpecFailure::IndexOutOfRange:   return ScanFailure::IndexOutOfRange;
        case SpecFailure::WidthArgNotInteger:return ScanFailure::WidthArgNotInteger;
        }
        return ScanFailure::Malformed;
    }

    std::string_view fmt_;
    CategoryTable cats_;
    SpecPolicy policy_;
    std::size_t next_auto_{0};
    std::size_t field_count_{0};
    bool manual_seen_{false};
    bool auto_seen_{false};
    bool mixed_{false};
};

[[nodiscard]] constexpr ScanResult
scan_format_string(std::string_view fmt, CategoryTable cats,
                    SpecPolicy policy) noexcept {
    return TypeAwareScanner{fmt, cats, policy}.run();
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 8 — the MESSAGE TABLE, carried forward from Oct 28 and DEMOTED.
//
// Oct 28's table was the primary mechanism for deciding FormatArgument.
// Today it is the FALLBACK: the type-aware scanner decides the whole
// spec-vs-argument class structurally, and the table is consulted only
// for failures that are not about the spec's shape at all.
//
// The table itself is unchanged (it is libc++ 21's message text, which
// is not part of any standard).
// ============================================================================

namespace petra {
namespace detail {

enum class MessageVerdict : unsigned char {
    Parse,
    Argument,
    Ambiguous,
    Unknown,
};

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
    "The format specification does not allow the sign option",
    "The format specifier does not allow the sign option",
    "The format specifier does not allow the alternate form option",
    "The format specifier does not allow the zero-padding option",
    "The format specifier does not allow the precision option",
    "The format specifier does not allow the locale-specific form option",
    "The format specifier does not allow the n option",
    "The format specifier uses an invalid value for the type option",
};

constexpr std::string_view kAmbiguousMessages[] = {
    "The format specifier should consume the input or end with a '}'",
    "The width option should not have a leading zero",
};

template <std::size_t N>
[[nodiscard]] constexpr bool
contains(const std::string_view (&table)[N], std::string_view needle) noexcept {
    for (const std::string_view entry : table) {
        if (entry == needle) return true;
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

[[nodiscard]] constexpr MessageVerdict
verdict_from_message(std::string_view message) noexcept {
    if (contains(kParseMessages, message)) return MessageVerdict::Parse;
    if (contains(kArgumentMessages, message)) return MessageVerdict::Argument;
    if (starts_with_any(kArgumentPrefixes, message)) return MessageVerdict::Argument;
    if (contains(kAmbiguousMessages, message)) return MessageVerdict::Ambiguous;
    return MessageVerdict::Unknown;
}

// ============================================================================
// Part 9 — the CLASSIFIER, now type-aware.
//
// Oct 28's classifier:
//
//   Stage A: the TYPE-AGNOSTIC grammar scanner.  A malformed string is
//            a FormatParse whatever the message says.
//   Stage B: the message table.  A well-formed string plus a known
//            message is a FormatArgument.
//   Fallback: Format.
//
// Today's classifier:
//
//   Stage A: the TYPE-AWARE scanner.  It answers BOTH questions — is
//            the string well-formed (FormatParse), and is the spec
//            valid for the argument it names (FormatArgument) — so it
//            decides the whole spec-vs-argument class WITHOUT reading
//            the message.
//   Stage B: the message table, for the failures that are not about
//            the spec's shape: a value that does not fit the selected
//            presentation type, an argument index past the end of the
//            list, a user's own formatter throwing.
//   Fallback: Format.
//
// The order matters and is unchanged in spirit: the scanner's verdict
// is about the STRING and the ARGUMENT, so it wins over the message.
// A message that says "parse" while the scanner says the spec is fine
// for this argument is a contradiction, and the arc falls back rather
// than assert a kind it cannot support — exactly as Oct 28 did.
// ============================================================================

struct Classification {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    bool used_message_table{false};   // did we have to read libc++'s text?
    const char* reason{""};
};

[[nodiscard]] inline Classification
classify_format_failure(std::string_view fmt, const char* what,
                         CategoryTable cats, SpecPolicy policy) noexcept {
    Classification c{};

    const ScanResult s = scan_format_string(fmt, cats, policy);

    switch (s.failure) {
    case ScanFailure::None:
        break;

    case ScanFailure::Malformed:
    case ScanFailure::MixedNumbering:
        // The FORMAT STRING is broken.  This is a parse failure
        // whatever the stdlib's message says.
        c.kind = FormatErrorKind::FormatParse;
        c.message = std::string{what} + " [format string: " + s.detail + "]";
        c.reason = s.detail;
        return c;

    case ScanFailure::TypeNotAllowed:
    case ScanFailure::OptionNotAllowed:
    case ScanFailure::IndexOutOfRange:
    case ScanFailure::WidthArgNotInteger:
        // The spec is well-formed, but it does not fit the ARGUMENT.
        // Decided structurally — no libc++ message consulted.
        c.kind = FormatErrorKind::FormatArgument;
        c.message = std::string{what} + " [argument " + std::to_string(s.arg_index)
                    + ": " + s.detail + "]";
        c.reason = s.detail;
        return c;
    }

    // The scanner found nothing wrong.  The failure must be about the
    // VALUE, or about something only the stdlib knows.  Consult the
    // message table as the fallback.
    c.used_message_table = true;
    switch (verdict_from_message(what)) {
    case MessageVerdict::Parse:
        c.kind = FormatErrorKind::Format;
        c.message = std::string{what} + " [scanner disagrees: string is well-formed]";
        c.reason = "the message says parse but the string is well-formed";
        return c;
    case MessageVerdict::Argument:
    case MessageVerdict::Ambiguous:
        c.kind = FormatErrorKind::FormatArgument;
        c.message = std::string{what};
        c.reason = "the stdlib's message identifies an argument mismatch";
        return c;
    case MessageVerdict::Unknown:
    default:
        c.kind = FormatErrorKind::Format;
        c.message = std::string{what};
        c.reason = "unrecognised message";
        return c;
    }
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 10 — the two two-pass impls.
//
// The ONLY change from Oct 28 is that the runtime path now knows the
// argument categories.  It knows them because `try_format_runtime_n_impl`
// is a template over `Args...`: the categories are a COMPILE-TIME fact,
// derived by `category_of<Args>()...`, and they are handed to the
// classifier as a table.
//
// This is the answer to Oct 28's item (2).  The item said the runtime
// path does not have the categories "because `std::make_format_args`
// erases it".  `std::make_format_args` does erase them — but it erases
// them INSIDE the stdlib, AFTER the arc's own template has already seen
// them.  The erasure is downstream of the arc, not upstream of it.
//
// Everything else — the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour — is unchanged.
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] constexpr std::array<ArgCategory, sizeof...(Args)>
categories_of() noexcept {
    return {category_of<Args>()...};
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt,
                          const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    constexpr std::array<ArgCategory, sizeof...(Args)> cats = categories_of<Args...>();
    const CategoryTable table{cats.data(), cats.size()};

    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        const Classification c =
            classify_format_failure(fmt, e.what(), table, SpecPolicy::Standard);
        return std::unexpected(FormatError{c.kind, c.message});
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
        const Classification c =
            classify_format_failure(fmt, e.what(), table, SpecPolicy::Standard);
        return std::unexpected(FormatError{c.kind, c.message});
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

    constexpr std::array<ArgCategory, sizeof...(Args)> cats = categories_of<Args...>();
    const CategoryTable table{cats.data(), cats.size()};

    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt_str.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        const Classification c =
            classify_format_failure(fmt_str.get(), e.what(), table,
                                    SpecPolicy::Standard);
        return std::unexpected(FormatError{c.kind, c.message});
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
        const Classification c =
            classify_format_failure(fmt_str.get(), e.what(), table,
                                    SpecPolicy::Standard);
        return std::unexpected(FormatError{c.kind, c.message});
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
// Part 11 — the test types.
//
// A user-defined formatter, to show that the validator ABSTAINS for a
// type whose rules it cannot know, rather than guessing.
// ============================================================================

namespace petra_test {

struct point {
    int x{0};
    int y{0};
};

}  // namespace petra_test

template <>
struct std::formatter<petra_test::point> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::point& p, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({},{})", p.x, p.y);
    }
};

// ============================================================================
// Part 12 — compile-time pins.
//
// Each static_assert is a claim about a SPECIFIC rule, not about this
// file.  They are the cheapest possible regression net: if a rule
// changes, the FILE DOES NOT COMPILE.
// ============================================================================

namespace petra {
namespace detail {

static_assert(category_of<bool>() == ArgCategory::Boolean);
static_assert(category_of<char>() == ArgCategory::Character);
static_assert(category_of<signed char>() == ArgCategory::Integral);
static_assert(category_of<unsigned char>() == ArgCategory::Integral);
static_assert(category_of<short>() == ArgCategory::Integral);
static_assert(category_of<int>() == ArgCategory::Integral);
static_assert(category_of<unsigned>() == ArgCategory::Integral);
static_assert(category_of<long>() == ArgCategory::Integral);
static_assert(category_of<long long>() == ArgCategory::Integral);
static_assert(category_of<float>() == ArgCategory::FloatingPoint);
static_assert(category_of<double>() == ArgCategory::FloatingPoint);
static_assert(category_of<long double>() == ArgCategory::FloatingPoint);
static_assert(category_of<const char*>() == ArgCategory::String);
static_assert(category_of<char*>() == ArgCategory::String);
static_assert(category_of<char[4]>() == ArgCategory::String);
static_assert(category_of<std::string>() == ArgCategory::String);
static_assert(category_of<std::string_view>() == ArgCategory::String);
static_assert(category_of<void*>() == ArgCategory::Pointer);
static_assert(category_of<const void*>() == ArgCategory::Pointer);
static_assert(category_of<std::nullptr_t>() == ArgCategory::Pointer);
static_assert(category_of<wchar_t>() == ArgCategory::UserDefined);
static_assert(category_of<char16_t>() == ArgCategory::UserDefined);
static_assert(category_of<petra_test::point>() == ArgCategory::UserDefined);

// The phase-1 masks are libc++'s `__fields_*` tables.
static_assert(has(phase1_mask(ArgCategory::Boolean), Opt::Sign));
static_assert(!has(phase1_mask(ArgCategory::Boolean), Opt::Precision));
static_assert(has(phase1_mask(ArgCategory::FloatingPoint), Opt::Precision));
static_assert(has(phase1_mask(ArgCategory::String), Opt::Precision));
static_assert(!has(phase1_mask(ArgCategory::String), Opt::Sign));
static_assert(has(phase1_mask(ArgCategory::Pointer), Opt::ZeroPad));
static_assert(!has(phase1_mask(ArgCategory::Pointer), Opt::Sign));

// The standard's base permissions.
static_assert(!has(standard_base_mask(ArgCategory::Boolean), Opt::Sign));
static_assert(has(standard_base_mask(ArgCategory::Boolean), Opt::Locale));
static_assert(!has(standard_base_mask(ArgCategory::Character), Opt::Sign));
static_assert(has(standard_base_mask(ArgCategory::Character), Opt::Locale));
static_assert(has(standard_base_mask(ArgCategory::Integral), Opt::Sign));
static_assert(has(standard_base_mask(ArgCategory::FloatingPoint), Opt::Precision));
static_assert(!has(standard_base_mask(ArgCategory::Integral), Opt::Precision));
static_assert(!has(standard_base_mask(ArgCategory::Boolean), Opt::Precision));
static_assert(!has(standard_base_mask(ArgCategory::Character), Opt::Precision));
static_assert(has(standard_base_mask(ArgCategory::String), Opt::Precision));
static_assert(!has(standard_base_mask(ArgCategory::String), Opt::Sign));
static_assert(has(standard_base_mask(ArgCategory::Pointer), Opt::ZeroPad));

// The integer-presentation-type tables.
static_assert(is_integer_presentation_type(ArgCategory::Integral, 'c'));
static_assert(!is_integer_presentation_type(ArgCategory::Character, 'c'));
static_assert(!is_integer_presentation_type(ArgCategory::Boolean, 'c'));
static_assert(is_integer_presentation_type(ArgCategory::Boolean, 'd'));
static_assert(is_integer_presentation_type(ArgCategory::Character, 'x'));

// THE DIVERGENCE, pinned from both sides.
static_assert(has(standard_effective_mask(ArgCategory::Integral, 'c'), Opt::Sign));
static_assert(!has(effective_mask(ArgCategory::Integral, 'c', SpecPolicy::Libcxx21),
                    Opt::Sign));
static_assert(has(effective_mask(ArgCategory::Integral, 'c', SpecPolicy::Libcxx21),
                   Opt::Locale));
// charT is NOT divergent: both policies narrow it.
static_assert(!has(standard_effective_mask(ArgCategory::Character, 'c'), Opt::Sign));
static_assert(!has(effective_mask(ArgCategory::Character, 'c', SpecPolicy::Libcxx21),
                    Opt::Sign));
// bool is NOT divergent: both policies narrow it for a non-integer type.
static_assert(!has(standard_effective_mask(ArgCategory::Boolean, 's'), Opt::Sign));
static_assert(!has(effective_mask(ArgCategory::Boolean, 's', SpecPolicy::Libcxx21),
                    Opt::Sign));
static_assert(has(standard_effective_mask(ArgCategory::Boolean, 'd'), Opt::Sign));
static_assert(has(effective_mask(ArgCategory::Boolean, 'd', SpecPolicy::Libcxx21),
                   Opt::Sign));

// The legal presentation types.
static_assert(is_legal_type_char(ArgCategory::Boolean, 's'));
static_assert(!is_legal_type_char(ArgCategory::Boolean, 'c'));
static_assert(!is_legal_type_char(ArgCategory::Character, 's'));
static_assert(is_legal_type_char(ArgCategory::Character, 'c'));
static_assert(is_legal_type_char(ArgCategory::Character, '?'));
static_assert(is_legal_type_char(ArgCategory::Integral, 'c'));
static_assert(!is_legal_type_char(ArgCategory::Integral, 's'));
static_assert(is_legal_type_char(ArgCategory::FloatingPoint, 'e'));
static_assert(!is_legal_type_char(ArgCategory::FloatingPoint, 'd'));
static_assert(is_legal_type_char(ArgCategory::String, 's'));
static_assert(is_legal_type_char(ArgCategory::String, '?'));
static_assert(is_legal_type_char(ArgCategory::Pointer, 'p'));
static_assert(is_legal_type_char(ArgCategory::Pointer, 'P'));
static_assert(!is_legal_type_char(ArgCategory::Pointer, 'x'));
// 'z' is legal for NO category.
static_assert(!is_known_type_char('z'));
static_assert(is_known_type_char('c'));

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 13 — the harness.
//
// Section 1  the type-category table, measured
// Section 2  the two-phase model, and the ONE rule that differs
// Section 3  the 1,596-cell grid: model vs libc++ vs the standard
// Section 4  the classifier: how many rejects need the message table?
// Section 5  the failures the type-aware scanner CANNOT see
// Section 6  the arc's own paths, end to end
// ============================================================================

namespace {

struct counters {
    std::size_t pass{0};
    std::size_t fail{0};

    void check(bool ok) noexcept {
        if (ok) ++pass; else ++fail;
    }
    void expect_eq(std::size_t got, std::size_t want, const char* what) {
        if (got == want) {
            ++pass;
        } else {
            ++fail;
            std::printf("  FAIL %s: got %zu, want %zu\n", what, got, want);
        }
    }
};

void section(const char* title) {
    std::printf("\n=== %s ===\n", title);
}

// The grid: 12 prefixes x 19 presentation types = 228 specs per
// category, over 7 categories = 1,596 cells.
constexpr std::string_view kPrefixes[] = {
    "", "+", "-", " ", "#", "0", "10", ".2", "L", "<", "*>10", "<10.2",
};
constexpr std::string_view kTypeChars[] = {
    "", "a", "A", "b", "B", "c", "d", "e", "f", "F", "g", "G",
    "o", "p", "s", "x", "X", "P", "?",
};

struct GridSpec {
    std::string text;
};

// Build the spec strings once.
std::vector<GridSpec> build_specs() {
    std::vector<GridSpec> out;
    for (const std::string_view prefix : kPrefixes) {
        for (const std::string_view type : kTypeChars) {
            out.push_back(GridSpec{std::string{prefix} + std::string{type}});
        }
    }
    return out;
}

// What the stdlib did with one spec.
enum class LibcOutcome { Ok, Reject };

// Run one spec through the ACTUAL stdlib and through the MODEL.
template <typename T>
LibcOutcome libc_verdict(std::string_view spec, T& value, std::string& message) {
    const std::string fmt = std::string{"{:"} + std::string{spec} + "}";
    try {
        (void)std::vformat(fmt, std::make_format_args(value));
        return LibcOutcome::Ok;
    } catch (const std::format_error& e) {
        message = e.what();
        return LibcOutcome::Reject;
    }
}

// The model's verdict for one spec against one category.
bool model_verdict(petra::detail::ArgCategory cat, std::string_view spec,
                    petra::detail::SpecPolicy policy, std::string& reason) {
    using namespace petra::detail;
    SpecFailure sf = SpecFailure::None;
    Opt bad = Opt::None;
    const ParsedSpec parsed = parse_spec(spec, cat, sf, bad);
    if (sf != SpecFailure::None) {
        reason = "spec parse";
        return false;
    }
    const SpecVerdict v = validate_spec(cat, parsed, policy);
    if (!v.accepted()) {
        reason = v.detail;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Section 1 — the type-category table.
// ---------------------------------------------------------------------------
void section1(counters& c) {
    section("Section 1 — the type-category table, measured");

    using namespace petra::detail;

    struct Row { const char* type_name; ArgCategory cat; };
    const Row rows[] = {
        {"bool",                 category_of<bool>()},
        {"char",                 category_of<char>()},
        {"signed char",          category_of<signed char>()},
        {"unsigned char",        category_of<unsigned char>()},
        {"short",                category_of<short>()},
        {"int",                  category_of<int>()},
        {"unsigned",             category_of<unsigned>()},
        {"long",                 category_of<long>()},
        {"long long",            category_of<long long>()},
        {"float",                category_of<float>()},
        {"double",               category_of<double>()},
        {"long double",          category_of<long double>()},
        {"const char*",          category_of<const char*>()},
        {"char*",                category_of<char*>()},
        {"char[4]",              category_of<char[4]>()},
        {"std::string",          category_of<std::string>()},
        {"std::string_view",     category_of<std::string_view>()},
        {"void*",                category_of<void*>()},
        {"const void*",          category_of<const void*>()},
        {"std::nullptr_t",       category_of<std::nullptr_t>()},
        {"wchar_t",              category_of<wchar_t>()},
        {"char16_t",             category_of<char16_t>()},
        {"petra_test::point",    category_of<petra_test::point>()},
    };

    for (const Row& r : rows) {
        std::printf("  %-18s -> %s\n", r.type_name,
                    std::string{category_name(r.cat)}.c_str());
    }

    // The two edges that are easy to get wrong.
    c.check(category_of<char>() == ArgCategory::Character);
    c.check(category_of<signed char>() == ArgCategory::Integral);
    c.check(category_of<char[4]>() == ArgCategory::String);
    c.check(category_of<petra_test::point>() == ArgCategory::UserDefined);
    std::printf("  note: char is charT, but signed/unsigned char are Integral;\n"
                "        a user-defined formatter is UserDefined, so the\n"
                "        validator ABSTAINS for it.\n");
}

// ---------------------------------------------------------------------------
// Section 2 — the two-phase model, and the one rule that differs.
// ---------------------------------------------------------------------------
void section2(counters& c) {
    section("Section 2 — the two-phase model, and the ONE rule that differs");

    using namespace petra::detail;

    std::printf("  %-16s %-28s %-28s\n", "category", "phase-1 mask (libc++)",
                "standard base mask");
    for (const ArgCategory cat : {ArgCategory::Boolean, ArgCategory::Character,
                                  ArgCategory::Integral, ArgCategory::FloatingPoint,
                                  ArgCategory::String, ArgCategory::Pointer}) {
        const OptMask p1 = phase1_mask(cat);
        const OptMask sb = standard_base_mask(cat);
        std::string p1s, sbs;
        if (has(p1, Opt::Sign)) p1s += "sign ";
        if (has(p1, Opt::Alternate)) p1s += "alt ";
        if (has(p1, Opt::ZeroPad)) p1s += "zero ";
        if (has(p1, Opt::Precision)) p1s += "prec ";
        if (has(p1, Opt::Locale)) p1s += "locale ";
        if (has(sb, Opt::Sign)) sbs += "sign ";
        if (has(sb, Opt::Alternate)) sbs += "alt ";
        if (has(sb, Opt::ZeroPad)) sbs += "zero ";
        if (has(sb, Opt::Precision)) sbs += "prec ";
        if (has(sb, Opt::Locale)) sbs += "locale ";
        std::printf("  %-16s %-28s %-28s\n",
                    std::string{category_name(cat)}.c_str(), p1s.c_str(), sbs.c_str());
    }

    std::printf("\n  the divergence, cell by cell (type 'c'):\n");
    for (const ArgCategory cat : {ArgCategory::Boolean, ArgCategory::Character,
                                  ArgCategory::Integral}) {
        const bool std_sign = has(standard_effective_mask(cat, 'c'), Opt::Sign);
        const bool libc_sign = has(effective_mask(cat, 'c', SpecPolicy::Libcxx21), Opt::Sign);
        std::printf("    %-10s standard=%-8s libc++=%-8s %s\n",
                    std::string{category_name(cat)}.c_str(),
                    std_sign ? "allows" : "rejects",
                    libc_sign ? "allows" : "rejects",
                    (std_sign != libc_sign) ? "<-- DIVERGENT" : "");
    }
    std::printf("    the divergence is ONE boolean: does type 'c' narrow the\n"
                "    integral mask?  The standard says no (Table 107 makes 'c'\n"
                "    an integer presentation type); libc++ 21 says yes.\n");

    c.check(has(standard_effective_mask(ArgCategory::Integral, 'c'), Opt::Sign));
    c.check(!has(effective_mask(ArgCategory::Integral, 'c', SpecPolicy::Libcxx21), Opt::Sign));
    c.check(has(standard_effective_mask(ArgCategory::Character, 'c'), Opt::Sign)
            == has(effective_mask(ArgCategory::Character, 'c', SpecPolicy::Libcxx21), Opt::Sign));
}

// ---------------------------------------------------------------------------
// Section 3 — the 1,596-cell grid.
//
// The grid is 12 single-option prefixes x 19 presentation types = 228
// specs per category, over 7 categories = 1,596 cells.
//
// Three things are compared on every cell:
//   1. what libc++ 21 (the LINKED stdlib) actually does — measured live;
//   2. what the arc's model says under libc++'s rules;
//   3. what the arc's model says under the STANDARD's rules.
//
// And a fourth, INDEPENDENT data point: the same grid was run under
// libstdc++ 15 (g++-15 — a different implementation of the same
// standard).  Its verdicts are embedded below as one 228-character
// string per category: 'A' = accepted, 'R' = rejected, in the same
// prefix-major / type-minor order the grid is generated in.
//
// THAT is the evidence that the model is right and libc++ is the
// outlier: the standard-derived model agrees with libstdc++ on every
// single cell, and differs from libc++ on exactly the 10 cells where
// libc++ is known to be wrong.
// ---------------------------------------------------------------------------

// libstdc++ 15's verdicts on the same grid, measured by the same
// generator (see the .md for the exact command).
constexpr std::string_view kLibstdcxxGrid[] = {
    // bool
    "ARRAARARRRRRARAAARRRRRAARARRRRRARRAARRRRRAARARRRRRARRAARRRRRAARARRRRRARRAARR"
    "RRRAARARRRRRARRAARRRRRAARARRRRRARRAARRARRAARARRRRRARAAARRRRRRRRRRRRRRRRRRRRR"
    "ARRAARARRRRRARAAARRARRAARARRRRRARAAARRARRAARARRRRRARAAARRRRRRRRRRRRRRRRRRRRR",
    // char
    "ARRAAAARRRRRARRAARARRRAARARRRRRARRAARRRRRAARARRRRRARRAARRRRRAARARRRRRARRAARR"
    "RRRAARARRRRRARRAARRRRRAARARRRRRARRAARRARRAAAARRRRRARRAARARRRRRRRRRRRRRRRRRRR"
    "ARRAAAARRRRRARRAARAARRAAAARRRRRARRAARAARRAAAARRRRRARRAARARRRRRRRRRRRRRRRRRRR",
    // int
    "ARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARR"
    "ARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRRRRRRRRRRRRRRRRRRRR"
    "ARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRRRRRRRRRRRRRRRRRRRR",
    // long
    "ARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARR"
    "ARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRRRRRRRRRRRRRRRRRRRR"
    "ARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRARRAAAARRRRRARRAARRRRRRRRRRRRRRRRRRRRR",
    // double
    "AAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRR"
    "AAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRR"
    "AAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRRAAARRRRAAAAARRRRRRR",
    // string
    "ARRRRRRRRRRRRRARRRARRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR"
    "RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRARRRRRRRRRRRRRARRRAARRRRRRRRRRRRRARRRA"
    "RRRRRRRRRRRRRRRRRRRARRRRRRRRRRRRRARRRAARRRRRRRRRRRRRARRRAARRRRRRRRRRRRRARRRA",
    // voidp
    "ARRRRRRRRRRRRARRRARRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR"
    "RRRRRRRRRRRRRRRRRRRARRRRRRRRRRRRARRRARARRRRRRRRRRRRARRRARRRRRRRRRRRRRRRRRRRR"
    "RRRRRRRRRRRRRRRRRRRARRRRRRRRRRRRARRRARARRRRRRRRRRRRARRRARRRRRRRRRRRRRRRRRRRR",
};
void section3(counters& c) {
    section("Section 3 — the 1,596-cell grid: model vs libc++ vs libstdc++");

    using namespace petra::detail;

    const std::vector<GridSpec> specs = build_specs();
    std::printf("  specs per category: %zu   categories: 7   cells: %zu\n",
                specs.size(), specs.size() * 7);

    std::size_t libc_ok{0}, libc_reject{0};
    std::size_t agree_libcxx_policy{0}, agree_standard_policy{0};
    std::size_t agree_libstdcxx{0};
    std::size_t libcxx_vs_standard{0};
    std::vector<std::string> divergent;

    // The seven categories, each with a representative argument.  The
    // index also selects that category's embedded libstdc++ row.
    const auto run = [&](const char* label, ArgCategory cat, auto& value,
                         std::size_t grid_row) {
        const std::string_view gxx_row = kLibstdcxxGrid[grid_row];
        std::size_t cell = 0;
        for (const GridSpec& spec : specs) {
            std::string message;
            const LibcOutcome outcome = libc_verdict(spec.text, value, message);
            if (outcome == LibcOutcome::Ok) ++libc_ok; else ++libc_reject;

            std::string reason;
            const bool model_std = model_verdict(cat, spec.text, SpecPolicy::Standard, reason);
            const bool model_libc = model_verdict(cat, spec.text, SpecPolicy::Libcxx21, reason);
            if (model_std == model_libc) ++libcxx_vs_standard;
            if (model_libc == (outcome == LibcOutcome::Ok)) ++agree_libcxx_policy;
            if (model_std == (outcome == LibcOutcome::Ok)) ++agree_standard_policy;
            else {
                divergent.push_back(std::string{label} + "  spec=\""
                                    + spec.text + "\"  libc++ says REJECT: " + message);
            }

            // The independent cross-check: the standard-derived model
            // against libstdc++ 15's measured verdicts.
            const bool gxx_ok = (gxx_row[cell] == 'A');
            if (model_std == gxx_ok) ++agree_libstdcxx;
            ++cell;
        }
    };

    bool b = true;
    char ch = 'A';
    int i = 42;
    unsigned u = 42u;
    long l = 42L;
    long long ll = 42LL;
    double d = 1.5;
    float f = 1.5f;
    std::string_view sv = "s";
    const void* vp = nullptr;

    run("bool", ArgCategory::Boolean, b, 0);
    run("char", ArgCategory::Character, ch, 1);
    run("int", ArgCategory::Integral, i, 2);
    run("long", ArgCategory::Integral, l, 3);
    run("double", ArgCategory::FloatingPoint, d, 4);
    run("string", ArgCategory::String, sv, 5);
    run("voidp", ArgCategory::Pointer, vp, 6);
    (void)u; (void)ll; (void)f;

    std::printf("\n  libc++ outcomes:   OK=%zu  REJECT=%zu\n", libc_ok, libc_reject);
    std::printf("  model(libc++ policy) vs libc++:   %zu/%zu agree\n",
                agree_libcxx_policy, libc_ok + libc_reject);
    std::printf("  model(standard)      vs libc++:   %zu/%zu agree\n",
                agree_standard_policy, libc_ok + libc_reject);
    std::printf("  model(standard)      vs libstdc++: %zu/%zu agree  <-- INDEPENDENT\n",
                agree_libstdcxx, libc_ok + libc_reject);
    std::printf("  the two policies differ on:       %zu cell(s)\n",
                libc_ok + libc_reject - libcxx_vs_standard);

    std::printf("\n  the divergent cells (libc++ rejects, the standard allows):\n");
    for (const std::string& div : divergent) {
        std::printf("    %s\n", div.c_str());
    }
    if (!divergent.empty()) {
        std::printf("    -> tracked upstream as llvm/llvm-project#106136 and\n"
                    "       microsoft/STL#4918: 'c' IS an integer presentation\n"
                    "       type (Table 107), so sign/alternate/zero are valid.\n");
    }
    std::printf("\n  THE INDEPENDENT CHECK.  The same grid was run under\n"
                "  libstdc++ 15 -- a different implementation of the same\n"
                "  standard -- and its verdicts are embedded in this file.\n"
                "  The standard-derived model agrees with libstdc++ on\n"
                "  %zu/%zu cells and disagrees with libc++ on exactly the\n"
                "  10 cells above.  That is what makes this a finding\n"
                "  about libc++ rather than about the model.\n",
                agree_libstdcxx, libc_ok + libc_reject);

    c.expect_eq(libc_ok + libc_reject, 1596, "grid cell count");
    c.expect_eq(agree_libcxx_policy, 1596, "model(libc++ policy) vs libc++");
    c.expect_eq(agree_standard_policy, 1586, "model(standard) vs libc++");
    // The independent cross-implementation check.
    c.expect_eq(agree_libstdcxx, 1596, "model(standard) vs libstdc++");
    c.expect_eq(libc_ok + libc_reject - libcxx_vs_standard, 10, "policy divergence count");
    c.expect_eq(divergent.size(), 10, "divergent cell count");
}

// ---------------------------------------------------------------------------
// Section 4 — the classifier: how many rejects need the message table?
// ---------------------------------------------------------------------------
void section4(counters& c) {
    section("Section 4 — the classifier: how many rejects need the message table?");

    using namespace petra::detail;

    const std::vector<GridSpec> specs = build_specs();

    std::size_t rejects{0};

    // Per-policy tallies.
    struct Tally {
        std::size_t structural{0};   // decided without reading the message
        std::size_t via_message{0};  // needed the message table
        std::size_t agree{0};        // and agreed with the table's verdict
        std::size_t disagree{0};
        std::vector<std::string> fallbacks;
    };
    Tally libcxx_tally;   // the arc models the LINKED stdlib's rules
    Tally standard_tally; // the arc models the STANDARD's rules

    const auto run = [&](const char* label, ArgCategory cat, auto& value) {
        const std::array<ArgCategory, 1> cats{cat};
        const CategoryTable table{cats.data(), cats.size()};
        for (const GridSpec& spec : specs) {
            std::string message;
            const LibcOutcome outcome = libc_verdict(spec.text, value, message);
            if (outcome == LibcOutcome::Ok) continue;
            ++rejects;

            // The classifier wants the WHOLE format string, not the
            // bare spec: it scans replacement fields.
            const std::string fmt = std::string{"{:"} + spec.text + "}";

            // What the MESSAGE TABLE alone would have said.
            const MessageVerdict mv = verdict_from_message(message);
            const petra::FormatErrorKind table_kind =
                (mv == MessageVerdict::Parse)   ? petra::FormatErrorKind::FormatParse
                : (mv == MessageVerdict::Argument || mv == MessageVerdict::Ambiguous)
                                                  ? petra::FormatErrorKind::FormatArgument
                                                  : petra::FormatErrorKind::Format;

            const auto tally = [&](Tally& t, SpecPolicy policy) {
                const Classification cl =
                    classify_format_failure(fmt, message.c_str(), table, policy);
                if (cl.used_message_table) {
                    ++t.via_message;
                    if (t.fallbacks.size() < 12) {
                        t.fallbacks.push_back(std::string{label} + " " + spec.text);
                    }
                } else {
                    ++t.structural;
                }
                if (table_kind == cl.kind) ++t.agree;
                else ++t.disagree;
            };

            tally(libcxx_tally, SpecPolicy::Libcxx21);
            tally(standard_tally, SpecPolicy::Standard);
        }
    };

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    run("bool", ArgCategory::Boolean, b);
    run("char", ArgCategory::Character, ch);
    run("int", ArgCategory::Integral, i);
    run("long", ArgCategory::Integral, l);
    run("double", ArgCategory::FloatingPoint, d);
    run("string", ArgCategory::String, sv);
    run("voidp", ArgCategory::Pointer, vp);

    std::printf("  rejects in the grid:                       %zu\n\n", rejects);

    std::printf("  POLICY = libc++ 21 (the rules of the LINKED stdlib)\n");
    std::printf("    decided STRUCTURALLY (no libc++ message):  %zu\n",
                libcxx_tally.structural);
    std::printf("    needed the message table (fallback):      %zu\n",
                libcxx_tally.via_message);
    std::printf("    agrees with the message table's verdict:  %zu\n",
                libcxx_tally.agree);
    std::printf("    disagrees with the message table:         %zu\n",
                libcxx_tally.disagree);

    std::printf("\n  POLICY = the STANDARD (portable rules)\n");
    std::printf("    decided STRUCTURALLY (no libc++ message):  %zu\n",
                standard_tally.structural);
    std::printf("    needed the message table (fallback):      %zu\n",
                standard_tally.via_message);
    std::printf("    agrees with the message table's verdict:  %zu\n",
                standard_tally.agree);
    std::printf("    disagrees with the message table:         %zu\n",
                standard_tally.disagree);
    if (!standard_tally.fallbacks.empty()) {
        std::printf("    the cells that fall back (all 10 are the\n"
                    "    divergent ones the standard ALLOWS):\n");
        for (const std::string& f : standard_tally.fallbacks) {
            std::printf("      %s\n", f.c_str());
        }
    }

    std::printf("\n  Oct 28's classifier needed the message table for ALL %zu\n"
                "  rejects, because its scanner was type-agnostic.  Today's\n"
                "  scanner decides %zu of them with NO message text at all —\n"
                "  when it models the stdlib it is actually linked against.\n",
                rejects, libcxx_tally.structural);

    c.expect_eq(rejects, 1172, "reject count");
    c.expect_eq(libcxx_tally.structural, 1172, "libc++ policy: structural");
    c.expect_eq(libcxx_tally.via_message, 0, "libc++ policy: via message");
    c.expect_eq(libcxx_tally.agree, 1172, "libc++ policy: agreement");
    c.expect_eq(libcxx_tally.disagree, 0, "libc++ policy: disagreement");
    // The standard model cannot decide the 10 divergent cells on its own.
    c.expect_eq(standard_tally.structural, 1162, "standard policy: structural");
    c.expect_eq(standard_tally.via_message, 10, "standard policy: via message");
    c.expect_eq(standard_tally.agree, 1172, "standard policy: agreement");
    c.expect_eq(standard_tally.disagree, 0, "standard policy: disagreement");
}

// ---------------------------------------------------------------------------
// Section 5 — the failures the type-aware scanner CANNOT see.
// ---------------------------------------------------------------------------
void section5(counters& c) {
    section("Section 5 — the failures the type-aware scanner CANNOT see");

    using namespace petra::detail;

    const std::array<ArgCategory, 1> cats{ArgCategory::Integral};
    const CategoryTable table{cats.data(), cats.size()};

    struct Case {
        const char* label;
        const char* fmt;
        const char* message;
    };
    const Case cases[] = {
        // The value does not fit the selected presentation type.  The
        // spec is fine; the VALUE is not.  Only the message knows.
        {"value does not fit the type", "{:c}",
         "Integral value outside the range of the char type"},
        // The field names an argument that does not exist.  The scanner
        // SEES this one, because the argument count is known.
        {"argument index past the end", "{5}",
         "The argument index value is too large for the number of arguments "
         "supplied"},
        // A user's own formatter threw.  The arc knows nothing about
        // it, so this lands on the SAFE FALLBACK (Format).
        {"a user's formatter throws", "{}",
         "user-defined formatter failed"},
    };

    for (const Case& cs : cases) {
        const Classification cl = classify_format_failure(
            cs.fmt, cs.message, table, SpecPolicy::Standard);
        std::printf("  %-32s fmt=%-6s -> kind=%s  used_message=%s\n",
                    cs.label, cs.fmt,
                    cl.kind == petra::FormatErrorKind::FormatParse ? "FormatParse"
                  : cl.kind == petra::FormatErrorKind::FormatArgument ? "FormatArgument"
                  : cl.kind == petra::FormatErrorKind::TooLarge ? "TooLarge" : "Format",
                    cl.used_message_table ? "yes" : "no");
    }
    std::printf("  -> the first and third are failures about the VALUE or\n"
                "     about something outside the spec: only the message\n"
                "     table can name them.  The second is visible to the\n"
                "     scanner, because the argument COUNT is known.\n");

    // The value-dependent case really is value-dependent: 65 fits, 300
    // does not, with the SAME spec.
    //
    // Note the distinction the lesson keeps making: `libc_verdict`
    // takes a BARE spec (it wraps it in "{:...}" itself), while
    // `classify_format_failure` takes the WHOLE format string.
    const std::array<ArgCategory, 1> int_cats{ArgCategory::Integral};
    const CategoryTable int_table{int_cats.data(), int_cats.size()};
    const std::string fmt = "{:c}";
    int small = 65;
    int large = 300;
    std::string m1, m2;
    const bool ok_small = (libc_verdict("c", small, m1) == LibcOutcome::Ok);
    const bool ok_large = (libc_verdict("c", large, m2) == LibcOutcome::Ok);
    const Classification cl_small =
        classify_format_failure(fmt, m1.c_str(), int_table, SpecPolicy::Standard);
    const Classification cl_large =
        classify_format_failure(fmt, m2.c_str(), int_table, SpecPolicy::Standard);
    std::printf("  {:c} with 65  -> libc++ %s   (no failure to classify)\n",
                ok_small ? "OK" : "REJECT");
    std::printf("  {:c} with 300 -> libc++ %s   arc kind=%s  used_message=%s\n",
                ok_large ? "OK" : "REJECT",
                cl_large.kind == petra::FormatErrorKind::FormatArgument ? "FormatArgument" : "Format",
                cl_large.used_message_table ? "yes" : "no");
    std::printf("  -> same spec, different value, different outcome: a purely\n"
                "     static validator cannot see this, so the message table\n"
                "     stays as the fallback.\n");

    c.check(ok_small);
    c.check(!ok_large);
    c.check(cl_small.used_message_table);
    c.check(cl_large.used_message_table);
    c.check(cl_large.kind == petra::FormatErrorKind::FormatArgument);
}

// ---------------------------------------------------------------------------
// Section 6 — the arc's own paths, end to end.
// ---------------------------------------------------------------------------
void section6(counters& c) {
    section("Section 6 — the arc's own paths, end to end");

    using petra::detail::try_format_runtime_n_impl;

    char buf[64];

    // A good call.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:>8.3f}", 3.14159);
        c.check(r.has_value());
        if (r) {
            c.check(*r == 8);
            c.check(std::string_view(buf, *r) == "   3.142");
            std::printf("  {:>8.3f} on 3.14159 -> \"%.*s\" (%zu bytes)\n",
                        static_cast<int>(*r), buf, *r);
        }
    }

    // A spec that does not fit the argument: an int with a precision.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:.2}", 42);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatArgument);
            std::printf("  {:.2} on an int -> kind=FormatArgument  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    // A spec that does not fit the argument: a string with a sign.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:+}", std::string_view{"x"});
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatArgument);
            std::printf("  {:+} on a string -> kind=FormatArgument  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    // A genuinely malformed format string.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:.}", 42);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatParse);
            std::printf("  {:.} on an int -> kind=FormatParse  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    // TooLarge is untouched.
    {
        const auto r = try_format_runtime_n_impl(buf, 3, "{}", 123456);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::TooLarge);
            c.check(r.error().needed_bytes == 6);
            std::printf("  cap=3 for \"123456\" -> kind=TooLarge needed=%zu\n",
                        r.error().needed_bytes);
        }
    }

    // The buffer is untouched on a failure.
    {
        char small[8] = {'X','X','X','X','X','X','X','X'};
        const auto r = try_format_runtime_n_impl(small, sizeof(small), "{:.2}", 42);
        c.check(!r.has_value());
        bool untouched = true;
        for (const char ch : small) {
            if (ch != 'X') untouched = false;
        }
        c.check(untouched);
        std::printf("  buffer atomicity on a Format error: %s\n",
                    untouched ? "preserved" : "CLOBBERED");
    }

    // The user-defined formatter: the validator abstains, the arc still
    // formats it.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{}", petra_test::point{3, 4});
        c.check(r.has_value());
        if (r) {
            c.check(std::string_view(buf, *r) == "(3,4)");
            std::printf("  a user-defined formatter -> \"%.*s\" (validator abstained)\n",
                        static_cast<int>(*r), buf);
        }
    }
}

}  // namespace

int main() {
    counters c;

    std::printf("P-2026-10-30 — try_format_type_aware_spec\n");
    std::printf("A type-aware format-spec validator: the argument's CATEGORY\n"
                "replaces libc++'s message text for the whole spec-vs-argument class.\n");

    section1(c);
    section2(c);
    section3(c);
    section4(c);
    section5(c);
    section6(c);

    std::printf("\n=== summary ===\n");
    std::printf("  pass=%zu  fail=%zu\n", c.pass, c.fail);
    return c.fail == 0 ? 0 : 1;
}
