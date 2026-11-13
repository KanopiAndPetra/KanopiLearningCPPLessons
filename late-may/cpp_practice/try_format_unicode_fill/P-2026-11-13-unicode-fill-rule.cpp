// P-2026-11-13 — try_format_unicode_fill:
//
// Closes Nov 12's "Where we go next" item (1) verbatim:
//
//   item (1) — "The Unicode fill rule — new today, and now measured.
//               The standard says the fill 'corresponds to a single
//               Unicode scalar value'; the arc's scanner is
//               BYTE-oriented: it reads the fill as one byte, so a
//               four-byte fill (U+1F921) is a malformed spec to the
//               arc and a valid one to libc++.  This lesson MEASURES
//               the false positive and leaves it visible; fixing it
//               needs a real UTF-8 decoder, which is the next link in
//               the chain."
//
// THE DEFECT.  [format.string.std]/3: "For a format specification in
// UTF-8, UTF-16, or UTF-32, the fill character corresponds to a
// single Unicode scalar value."  The fill is a CHARACTER, not a byte.
//
// The arc's scanner read ONE byte and asked "is the byte after it an
// alignment option?".  A four-byte fill therefore has no alignment
// byte after it, so the scanner fell through and called the spec
// malformed.  The stdlib accepts it.  That is a FALSE POSITIVE, and
// it was the last one the arc had.
//
// But the fix is not a one-line change, and the reason is the
// interesting part: the two implementations DISAGREE about what an
// ill-formed fill means, and they disagree in more than one way.
//
// KEY FINDING #1 — THE FILL IS A SCALAR VALUE, SO THE SCANNER NEEDS
// A UTF-8 DECODER, NOT A BIGGER BYTE WINDOW.
//
//   The scanner decodes one UTF-8 scalar value at the fill position,
//   using the rules of Unicode 15 Table 3-7 read BACKWARDS: the table
//   says which byte sequences are well-formed, and this says which
//   of the others it is -- truncated, invalid lead byte, overlong,
//   surrogate, out of range, bad continuation byte.
//
//   The decoder also reports the MAXIMAL SUBPART: how many bytes the
//   broken sequence occupies before the fault.  That number is what
//   an implementation that substitutes U+FFFD would replace, so it
//   is what the scanner has to skip.
//
// KEY FINDING #2 — THE TWO IMPLEMENTATIONS DISAGREE ABOUT ILL-FORMED
// FILLS, AND THE DIFFERENCE IS A MASK, NOT A BIT.
//
//   libc++ 21 rejects EVERY ill-formed fill: "The format specifier
//   contains malformed Unicode characters".
//
//   libstdc++ 15 makes only SOME of them fatal.  Overlong, surrogate
//   and above-U+10FFFF fills it rejects; the ones that merely stop
//   early -- truncated, or a lead byte that cannot start a sequence
//   -- it RECOVERS from, by substituting U+FFFD and carrying on.
//
//   So the policy table gains a MASK of the faults that are FATAL,
//   and the arc's scanner consults it.  This is the same shape as
//   Nov 7's per-implementation override rows, and the same shape as
//   Nov 12's numeric ceiling: the standard fixes the rule, each
//   implementation fixes its own enforcement of it.
//
// KEY FINDING #3 — THE ILL-FORMED FILL NO LONGER NEEDS THE MESSAGE
// TABLE.  IT IS A STRUCTURAL RULE NOW.
//
//   Nov 12's carried verdict rescued the invalid-UTF-8 fill under
//   libc++ ONLY because libc++'s message happened to be in the arc's
//   libc++-shaped table.  Today the scanner rejects the fill on its
//   own, so the verdict is FormatParse in BOTH builds -- and the
//   census's "moved by the carried verdict" count for the fill family
//   goes to ZERO, because the carry has nothing left to carry.
//
// KEY FINDING #4 — THE WIDTH COUNTS CODE POINTS, NOT BYTES.
//
//   A width of 10 with a four-byte fill produces THIRTY-ONE bytes:
//   ten code points, of which seven are the fill.  The arc's cap is
//   in BYTES (it is a buffer cap, and it is measured by the stdlib's
//   own output), so the arc is right and the two units must not be
//   confused: the WIDTH is a count of code points, the CAP is a count
//   of bytes.
//
// THE ANSWER TO THE CHAIN ITEM'S OWN QUESTION.
//
//   "decode the fill as a UTF-8 scalar value, validate it, and
//    re-measure the false-positive count".
//
//   All three, and both rules are measured against the LINKED stdlib
//   at once:
//
//     the byte-oriented rule   disagrees about the multi-byte fills
//                              (the arc rejects, the stdlib accepts)
//                              AND about the ill-formed ones (the arc
//                              accepts, the stdlib rejects)
//     the scalar-value rule     disagrees about NONE of them
//
//   The counts are in Section 9, and they are DIFFERENT under the two
//   toolchains -- because libc++ and libstdc++ disagree about which
//   ill-formed fills are fatal.  The same source is correct against
//   either, which is the point of the policy table.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings are exactly as they
// have been since Sep 6), the two-pass (measure, then write) design,
// the counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables, its derived override table, and
// its 1,596-cell grid -- which is re-measured here and must not move.
//
//
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError / petra::FormatErrorKind
//           (verbatim from Sep 6 through Oct 30).
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
//           (verbatim from Oct 11 .. Oct 30).
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
//           (verbatim from Oct 11 .. Oct 30).
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
// Part 4 — petra::detail::ArgCategory and the type-category table.
//           (verbatim from Oct 30.)
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
// Part 5 — NEW TODAY: THE POLICY TABLE.
//
// This is the whole point of the lesson, so it is worth being precise
// about what "a table" means here.
//
// Oct 30's model had FIVE mask functions:
//
//   phase1_mask(cat)               libc++'s PARSE (consume) mask
//   standard_base_mask(cat)       the standard's base permissions
//   standard_effective_mask(c,t)  the standard's mask for (cat, type)
//   phase2_mask(cat, type)        libc++'s mask for (cat, type)
//   effective_mask(cat, type, p)  the if/else between the two models
//
// The table below replaces all five with:
//
//   TWO shared data tables      the standard's BASE permissions per
//                               category, and the integer-presentation
//                               types per category;
//   ONE per-implementation      a list of ROWS, each row an override
//   ROW list                    of one (category, type) cell.
//
// The split of the old model into "base" + "integer-presentation-type
// widening" is what makes the override table expressible at all: the
// STANDARD's rules fall out of the base table plus the widening rule,
// and an implementation's difference is a cell in the resulting
// matrix.
//
// The rows are keyed by (category, type), not by (category, boolean).
// That is the difference between "one boolean between two code paths"
// and "one row in one table", and it is what lets a third
// implementation be added without touching a branch.
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

// The mask is a SCOPED ENUM, not a bare unsigned char, for one
// reason: an operator cannot be overloaded for a built-in type.  The
// enumerators carry the same bit values as Opt's, so a mask and an
// option remain interchangeable bit for bit — and `mask_of` is the one
// place that conversion happens.
enum class OptMask : unsigned char {
    None      = 0u,
    Sign      = 1u << 0,
    Alternate = 1u << 1,
    ZeroPad   = 1u << 2,
    Precision = 1u << 3,
    Locale    = 1u << 4,
};

[[nodiscard]] constexpr OptMask mask_of(Opt o) noexcept {
    return static_cast<OptMask>(static_cast<unsigned char>(o));
}

[[nodiscard]] constexpr OptMask operator|(OptMask a, OptMask b) noexcept {
    return static_cast<OptMask>(static_cast<unsigned char>(a)
                                 | static_cast<unsigned char>(b));
}

[[nodiscard]] constexpr OptMask operator&(OptMask a, OptMask b) noexcept {
    return static_cast<OptMask>(static_cast<unsigned char>(a)
                                 & static_cast<unsigned char>(b));
}

[[nodiscard]] constexpr bool has(OptMask m, Opt o) noexcept {
    return (m & mask_of(o)) != OptMask::None;
}

// A mask, rendered as the options it contains.  Used by Section 2 to
// print the EXACT difference between two policies — printing only the
// sign bit (as the draft did) under-reports a three-bit difference.
[[nodiscard]] std::string mask_text(OptMask m) {
    std::string s;
    const auto add = [&s, m](Opt o, const char* name) {
        if (!has(m, o)) return;
        if (!s.empty()) s += ' ';
        s += name;
    };
    add(Opt::Sign, "sign");
    add(Opt::Alternate, "alt");
    add(Opt::ZeroPad, "zero");
    add(Opt::Precision, "prec");
    add(Opt::Locale, "locale");
    if (s.empty()) s = "(none)";
    return s;
}

constexpr OptMask kMNone     = mask_of(Opt::None);
constexpr OptMask kMSign     = mask_of(Opt::Sign);
constexpr OptMask kMAlternate = mask_of(Opt::Alternate);
constexpr OptMask kMZero     = mask_of(Opt::ZeroPad);
constexpr OptMask kMPrecision = mask_of(Opt::Precision);
constexpr OptMask kMLocale   = mask_of(Opt::Locale);

// The options the standard's "or when an integer presentation type is
// specified" clause ADDS to a category's base permissions.
constexpr OptMask kIntegerTypeWidening = kMSign | kMAlternate | kMZero;

// ---- TABLE 1: the standard's BASE permissions, per category. ---------------
//
//   bool / charT      locale only: the sign, alternate and zero options
//                     are valid for them only when an integer
//                     presentation type is specified, and that is the
//                     widening rule's job, not the base's.
//   integral          sign alt zero locale
//   floating-point    sign alt zero prec locale
//   string            prec
//   pointer           zero
//   user-defined      nothing: the validator abstains for it.
struct BaseRow {
    ArgCategory category;
    OptMask base;
};

constexpr BaseRow kBaseTable[] = {
    {ArgCategory::Boolean,       kMLocale},
    {ArgCategory::Character,     kMLocale},
    {ArgCategory::Integral,      kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::FloatingPoint, kMSign | kMAlternate | kMZero | kMPrecision | kMLocale},
    {ArgCategory::String,        kMPrecision},
    {ArgCategory::Pointer,       kMZero},
    {ArgCategory::UserDefined,   kMNone},
};

[[nodiscard]] constexpr OptMask base_mask(ArgCategory c) noexcept {
    for (const BaseRow& row : kBaseTable) {
        if (row.category == c) return row.base;
    }
    return kMNone;
}

// ---- TABLE 2: the integer-presentation types, per category. ---------------
//
// The standard's Tables 107/108/109.  Note that 'c' is an integer
// presentation type for INTEGRAL but NOT for charT: charT's own 'c'
// means "the character", which is a different thing.
//
// A selected type in this set is what makes the sign / alternate /
// zero options valid for bool and charT.
struct IntPresRow {
    ArgCategory category;
    const char* types;
};

constexpr IntPresRow kIntPresTable[] = {
    {ArgCategory::Boolean,   "bBdoxX"},
    {ArgCategory::Character, "bBdoxX"},
    {ArgCategory::Integral,  "bBcdoxX"},
};

[[nodiscard]] constexpr bool is_integer_presentation_type(ArgCategory c, char t) noexcept {
    if (t == '\0') return false;
    for (const IntPresRow& row : kIntPresTable) {
        if (row.category != c) continue;
        for (const char* p = row.types; *p != '\0'; ++p) {
            if (*p == t) return true;
        }
    }
    return false;
}

// ---- TABLE 3: the CONSUME mask, per category. -----------------------------
//
// The options the PARSER will consume for this category.  An option
// outside the consume mask is not parsed at all: it is left in the
// input, and the leftover text is what trips libc++'s "should consume
// the input or end with a '}'" check.
//
// This is libc++'s `__fields_*` set, and the measurement shows
// libstdc++ 15 consumes the SAME set (see Part 13, Section 5).
struct ConsumeRow {
    ArgCategory category;
    OptMask mask;
};

constexpr ConsumeRow kSharedConsumeTable[] = {
    {ArgCategory::Boolean,       kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::Character,     kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::Integral,      kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::FloatingPoint, kMSign | kMAlternate | kMZero | kMPrecision | kMLocale},
    {ArgCategory::String,        kMPrecision},
    {ArgCategory::Pointer,       kMZero},
};

// ---- TABLE 4: the ALLOW mask, per category. -------------------------------
//
// The options the argument actually ACCEPTS, before any
// implementation-specific narrowing.  This is the standard's base mask
// widened by the integer-presentation-type clause.
[[nodiscard]] constexpr OptMask allow_mask(ArgCategory c, char type) noexcept {
    OptMask m = base_mask(c);
    if (is_integer_presentation_type(c, type)) {
        m = m | kIntegerTypeWidening;
    }
    return m;
}

// ---- TABLE 5: the per-implementation OVERRIDE rows. -----------------------
//
// One row overrides one (category, type) cell of the allow matrix.
//
// `any_type` rows apply to every type char of the category; otherwise
// the row applies only to `type_char`.
struct AllowRow {
    ArgCategory category;
    char type_char;
    bool any_type;
    OptMask mask;
};

// libc++ 21 narrows the integral mask to {locale} whenever the
// selected presentation type is 'c'.  The standard does not, because 'c'
// IS an integer presentation type for integral (Table 107), so the
// sign / alternate / zero options are valid there.
//
// This is the upstream bug: llvm/llvm-project#106136 and
// microsoft/STL#4918.
constexpr AllowRow kLibcxx21AllowRows[] = {
    {ArgCategory::Integral, 'c', false, kMLocale},
};

// ---- the fill's Unicode rule (NEW TODAY). -------------------------------
//
// [format.string.std]/3: "For a format specification in UTF-8, UTF-16,
// or UTF-32, the fill character corresponds to a single Unicode scalar
// value."  So the fill is a CHARACTER, not a byte.
//
// The two implementations differ about what an ill-formed fill means,
// and they differ in MORE THAN ONE WAY: libc++ 21 rejects every
// ill-formed fill, while libstdc++ 15 rejects the ones that decode to
// a value that is not a scalar value (overlong, surrogate, above
// U+10FFFF) and the ones with a bad continuation byte, and TOLERATES
// the ones that merely do not decode (truncated, invalid lead byte) by
// substituting U+FFFD.
//
// So the per-implementation datum is a MASK of the faults that are
// rejected -- the same shape as Nov 7's override rows and Nov 12's
// numeric ceiling: the standard fixes the rule, each implementation
// fixes its own enforcement of it.
enum class FillFault : unsigned char {
    None,
    // The sequence stops early: either the input ran out, or a byte
    // that cannot continue it came next.  What was consumed is the
    // MAXIMAL SUBPART (Unicode D93b): the longest prefix that could
    // still be completed into a well-formed sequence.
    Truncated,
    // The lead byte cannot start any well-formed sequence at all:
    // a continuation byte (0x80..0xBF) or 0xF8..0xFF.
    InvalidLead,
    // A complete sequence that names a value below the minimum for
    // its length: a non-minimal encoding of a smaller value.
    Overlong,
    // A complete sequence that names U+D800..U+DFFF.
    Surrogate,
    // A complete sequence that names a value above U+10FFFF.
    OutOfRange,
};

constexpr std::uint8_t kFaultTruncated   = 1u << 0;
constexpr std::uint8_t kFaultInvalidLead = 1u << 1;
constexpr std::uint8_t kFaultOverlong    = 1u << 2;
constexpr std::uint8_t kFaultSurrogate   = 1u << 3;
constexpr std::uint8_t kFaultOutOfRange  = 1u << 4;

constexpr std::uint8_t kFaultAll =
    kFaultTruncated | kFaultInvalidLead | kFaultOverlong | kFaultSurrogate
    | kFaultOutOfRange;

[[nodiscard]] constexpr std::uint8_t fault_bit(FillFault f) noexcept {
    switch (f) {
    case FillFault::None:             return 0;
    case FillFault::Truncated:        return kFaultTruncated;
    case FillFault::InvalidLead:      return kFaultInvalidLead;
    case FillFault::Overlong:         return kFaultOverlong;
    case FillFault::Surrogate:        return kFaultSurrogate;
    case FillFault::OutOfRange:       return kFaultOutOfRange;
    }
    return 0;
}

[[nodiscard]] constexpr const char* fault_name(FillFault f) noexcept {
    switch (f) {
    case FillFault::None:             return "None";
    case FillFault::Truncated:        return "Truncated";
    case FillFault::InvalidLead:      return "InvalidLead";
    case FillFault::Overlong:         return "Overlong";
    case FillFault::Surrogate:        return "Surrogate";
    case FillFault::OutOfRange:       return "OutOfRange";
    }
    return "?";
}

// How the fill is read, and which ill-formed fills are fatal.
struct FillPolicy {
    // Whether the fill is decoded as a Unicode scalar value at all.
    // The OLD rule read one byte; the NEW rule reads a scalar value.
    bool unicode{true};
    // Which ill-formed fills this implementation treats as FATAL.  The
    // rest it RECOVERS from, by substituting U+FFFD for the maximal
    // subpart and carrying on.
    std::uint8_t fatal{0};

    [[nodiscard]] constexpr bool is_fatal(FillFault f) const noexcept {
        return (fatal & fault_bit(f)) != 0;
    }
};

// One decoded scalar value, and what was wrong with it.
struct Utf8CodePoint {
    char32_t value{0};
    std::size_t length{0};    // bytes consumed (the maximal subpart)
    FillFault fault{FillFault::None};
};

// Decode ONE UTF-8 scalar value at the front of `s`.
//
// The rules are Unicode 15 Table 3-7 (Well-Formed UTF-8 Byte
// Sequences), read from the other end: the table says which byte
// sequences are WELL-FORMED, and this says which of the others it is.
//
// `length` is the number of bytes the MAXIMAL SUBPART occupies: the
// bytes consumed before the fault.  That is what an implementation
// that substitutes U+FFFD would replace, so it is what the scanner
// has to skip.
[[nodiscard]] constexpr Utf8CodePoint decode_utf8(std::string_view s) noexcept {
    Utf8CodePoint out{};
    if (s.empty()) return out;

    const auto b0 = static_cast<unsigned char>(s[0]);
    if (b0 < 0x80u) {
        out.value = static_cast<char32_t>(b0);
        out.length = 1;
        return out;
    }

    // The shape of the sequence the lead byte announces, and the range
    // the SECOND byte has to be in.  The bounds are the ones in
    // Unicode 15 Table 3-7: E0's second byte must be A0..BF (else the
    // encoding is overlong), ED's must be 80..9F (else it names a
    // surrogate), F0's must be 90..BF (overlong), F4's must be
    // 80..8F (above U+10FFFF).
    std::size_t want = 0;
    char32_t value = 0;
    unsigned char lo2 = 0x80u;
    unsigned char hi2 = 0xBFu;
    FillFault bound = FillFault::None;

    if (b0 >= 0xC0u && b0 <= 0xDFu) {
        // 0xC0 and 0xC1 are OVERLONG-ONLY leads: they can only ever
        // encode a value below U+0080, and the completion check below
        // is what catches them.
        want = 2;
        value = static_cast<char32_t>(b0 & 0x1Fu);
    } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
        want = 3;
        value = static_cast<char32_t>(b0 & 0x0Fu);
        if (b0 == 0xE0u) {
            lo2 = 0xA0u;
            bound = FillFault::Overlong;
        } else if (b0 == 0xEDu) {
            hi2 = 0x9Fu;
            bound = FillFault::Surrogate;
        }
    } else if (b0 >= 0xF0u && b0 <= 0xF7u) {
        want = 4;
        value = static_cast<char32_t>(b0 & 0x07u);
        if (b0 == 0xF0u) {
            lo2 = 0x90u;
            bound = FillFault::Overlong;
        } else if (b0 == 0xF4u) {
            hi2 = 0x8Fu;
            bound = FillFault::OutOfRange;
        }
    } else {
        // A continuation byte, or 0xF8..0xFF: never a lead byte.
        out.fault = FillFault::InvalidLead;
        out.length = 1;
        return out;
    }

    std::size_t k = 1;
    for (; k < want && k < s.size(); ++k) {
        const auto b = static_cast<unsigned char>(s[k]);
        if (b < 0x80u || b > 0xBFu) {
            // A byte that cannot continue the sequence: the sequence
            // stops here, and what came before is the maximal subpart.
            break;
        }
        if (k == 1 && bound != FillFault::None && (b < lo2 || b > hi2)) {
            // The second byte is outside the lead's range, so the
            // sequence is overlong, a surrogate, or out of range.  The
            // maximal subpart is the LEAD BYTE ALONE: "E0 80" is not a
            // prefix of any well-formed sequence.
            out.fault = bound;
            out.length = 1;
            return out;
        }
        value = static_cast<char32_t>((value << 6)
                                       | static_cast<char32_t>(b & 0x3Fu));
    }

    if (k < want) {
        // The sequence stops early.  What was consumed is a valid
        // prefix, so it IS the maximal subpart.
        out.fault = FillFault::Truncated;
        out.length = k;
        return out;
    }

    // The sequence is complete: check that it names a scalar value.
    const char32_t min_value =
        want == 2 ? 0x80u : (want == 3 ? 0x800u : 0x10000u);
    if (value < min_value) {
        out.fault = FillFault::Overlong;
        out.length = 1;
        return out;
    }
    if (value >= 0xD800u && value <= 0xDFFFu) {
        out.fault = FillFault::Surrogate;
        out.length = 1;
        return out;
    }
    if (value > 0x10FFFFu) {
        out.fault = FillFault::OutOfRange;
        out.length = 1;
        return out;
    }

    out.value = value;
    out.length = want;
    return out;
}

// Count the scalar values in a UTF-8 string.  Used by Section 9 to
// check what the stdlib counts when it counts a WIDTH.
[[nodiscard]] constexpr std::size_t count_code_points(std::string_view s) noexcept {
    std::size_t n = 0;
    std::size_t i = 0;
    while (i < s.size()) {
        const Utf8CodePoint cp = decode_utf8(s.substr(i));
        if (cp.length == 0) break;
        i += cp.length;
        ++n;
    }
    return n;
}

// ---- The implementations the arc knows about. ------------------------------
//
// "standard"      the portable rules of [format.string.std]
// "libc++ 21"     libc++ 21's rules, bug included
// "libstdc++ 15"  libstdc++ 15's rules (measured: the standard's)
struct ImplPolicy {
    const char* name;
    const AllowRow* allow_rows;
    std::size_t allow_count;
    const ConsumeRow* consume_rows;
    std::size_t consume_count;

    // The largest numeric literal this implementation can represent --
    // the width, the precision, or an argument index.  std::nullopt means
    // "these rules fix no ceiling": the standard's grammar admits any
    // positive-integer, and only the implementation's parser objects.
    //
    // Note the SHAPE: Nov 7's per-implementation difference was a mask
    // CELL, keyed by (category, type).  This one is a SCALAR.  The
    // override-row machinery still applies to the masks; this is a second
    // kind of policy datum, and it needs a second kind of lookup.
    std::optional<std::uint64_t> max_number{std::nullopt};

    // Where the ceiling comes from, in the implementation's own words.
    const char* ceiling_note{""};

    // NEW TODAY: how this implementation reads the fill.
    //
    // The standard fixes the RULE -- the fill is a single Unicode
    // scalar value -- but not the ENFORCEMENT, and the two
    // implementations enforce it differently.  So the fill's rule is
    // the standard's and lives in the scanner; what lives here is
    // which ill-formed fills this implementation rejects.
    FillPolicy fill{};
};

constexpr ImplPolicy kImpls[] = {
    {"standard",     nullptr,                  0, kSharedConsumeTable, 6,
     std::nullopt,
     "the grammar admits any positive-integer: no ceiling",
     // The standard: the fill is a single scalar value, so every
     // ill-formed fill is a malformed format string.
     FillPolicy{true, kFaultAll}},
    {"libc++ 21",    kLibcxx21AllowRows,       1, kSharedConsumeTable, 6,
     std::uint64_t{2147483647},
     "libc++ 21: __format/format_string.h, __number_max = INT32_MAX",
     // MEASURED: libc++ 21 rejects every ill-formed fill with
     // "The format specifier contains malformed Unicode characters".
     FillPolicy{true, kFaultAll}},
    {"libstdc++ 15", nullptr,                  0, kSharedConsumeTable, 6,
     std::uint64_t{65535},
     "libstdc++ 15: format, __parse_integer() returns unsigned short",
     // MEASURED: libstdc++ 15 makes overlong, surrogate and
     // above-U+10FFFF fills FATAL, and RECOVERS from the truncated and
     // invalid-lead ones by substituting U+FFFD.
     FillPolicy{true, kFaultOverlong | kFaultSurrogate | kFaultOutOfRange}},
};

constexpr std::size_t kImplCount = sizeof(kImpls) / sizeof(kImpls[0]);

[[nodiscard]] constexpr std::size_t impl_index_of(const char* name) noexcept {
    for (std::size_t i = 0; i < kImplCount; ++i) {
        std::size_t j = 0;
        for (; kImpls[i].name[j] != '\0' && name[j] != '\0'; ++j) {
            if (kImpls[i].name[j] != name[j]) break;
        }
        if (kImpls[i].name[j] == '\0' && name[j] == '\0') return i;
    }
    return kImplCount;
}

// The effective ALLOW mask for (category, type) under one policy: the
// standard's matrix, then the policy's override rows.
[[nodiscard]] constexpr OptMask
policy_allow_mask(ArgCategory c, char type, const ImplPolicy& impl) noexcept {
    OptMask m = allow_mask(c, type);
    for (std::size_t i = 0; i < impl.allow_count; ++i) {
        const AllowRow& row = impl.allow_rows[i];
        if (row.category != c) continue;
        if (!row.any_type && row.type_char != type) continue;
        m = row.mask;
    }
    return m;
}

// The CONSUME mask for a category under one policy.
[[nodiscard]] constexpr OptMask
policy_consume_mask(ArgCategory c, const ImplPolicy& impl) noexcept {
    OptMask m = kMNone;
    for (std::size_t i = 0; i < impl.consume_count; ++i) {
        if (impl.consume_rows[i].category == c) m = impl.consume_rows[i].mask;
    }
    return m;
}

// The implementation the arc should model: the one it is LINKED
// against.  This is what makes the same source correct under both
// toolchains with no source change at all.
[[nodiscard]] constexpr const ImplPolicy& linked_policy() noexcept {
#if defined(_LIBCPP_VERSION)
    return kImpls[impl_index_of("libc++ 21")];
#elif defined(_GLIBCXX_RELEASE)
    // NOTE: the macro is _GLIBCXX_RELEASE — ONE leading underscore.
    // The double-underscore spelling is not defined by any compiler,
    // so the draft's `#elif` was dead code and g++ silently got the
    // "standard" policy.  Verified with `g++ -dM -E`.
    return kImpls[impl_index_of("libstdc++ 15")];
#else
    return kImpls[impl_index_of("standard")];
#endif
}

// ---- The presentation types, per category (phase 3). ----------------------
//
// The legal presentation types.  A type char that is legal for NO
// category is a malformed spec for EVERY argument, which is a
// DIFFERENT failure from "legal type, wrong argument".
struct LegalTypeRow {
    ArgCategory category;
    const char* types;
};

constexpr LegalTypeRow kLegalTypeTable[] = {
    {ArgCategory::Boolean,       "sbBdoxX"},
    {ArgCategory::Character,     "c?bBdoxX"},
    {ArgCategory::Integral,      "bBdoxXc"},
    {ArgCategory::FloatingPoint, "aAeEfFgG"},
    {ArgCategory::String,        "s?"},
    {ArgCategory::Pointer,       "pP"},
};

[[nodiscard]] constexpr bool is_legal_type_char(ArgCategory c, char t) noexcept {
    if (c == ArgCategory::UserDefined) return true;
    if (t == '\0') return true;
    for (const LegalTypeRow& row : kLegalTypeTable) {
        if (row.category != c) continue;
        for (const char* p = row.types; *p != '\0'; ++p) {
            if (*p == t) return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr bool is_known_type_char(char t) noexcept {
    if (t == '\0') return true;
    for (const LegalTypeRow& row : kLegalTypeTable) {
        for (const char* p = row.types; *p != '\0'; ++p) {
            if (*p == t) return true;
        }
    }
    return false;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 6 — Oct 30's FIVE MASK FUNCTIONS, carried forward VERBATIM as the
//           reference implementation.
//
// This namespace exists so the lesson can PROVE that the new table is
// exactly equivalent to the two code paths it replaces.  It is the
// oracle, not the implementation.
//
// The bug in llvm/llvm-project#106136 lives in `phase2_mask` below, in
// the `else if (c == ArgCategory::Integral) narrow = (type == 'c');`
// line.  The new table moves it into a row.
// ============================================================================

namespace petra {
namespace reference {

using detail::ArgCategory;
using detail::Opt;
using detail::OptMask;

using detail::has;
using detail::mask_of;

// libc++'s PARSE (consume) mask — `__fields_*`.
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

[[nodiscard]] constexpr bool is_integer_presentation_type(ArgCategory c, char t) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
        return t == 'b' || t == 'B' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    case ArgCategory::Character:
        return t == 'b' || t == 'B' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    case ArgCategory::Integral:
        return t == 'b' || t == 'B' || t == 'c' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    default:
        return false;
    }
}

// NOTE: the calls below are QUALIFIED on purpose.  Oct 30's code calls
// `is_integer_presentation_type(c, type)` unqualified, and in this file
// that name exists TWICE: Oct 30's switch-based copy (here) and the new
// table-driven copy in `petra::detail`.  Unqualified, the call would be
// ambiguous -- ordinary lookup finds this one, and argument-dependent
// lookup finds the detail one, because ArgCategory lives there.  The
// qualification keeps Oct 30's function body byte-for-byte and makes the
// oracle's independence explicit.
[[nodiscard]] constexpr OptMask phase2_mask(ArgCategory c, char type) noexcept {
    bool narrow = false;
    if (c == ArgCategory::Boolean) {
        narrow = !reference::is_integer_presentation_type(c, type);
    } else if (c == ArgCategory::Character) {
        narrow = !reference::is_integer_presentation_type(c, type);
    } else if (c == ArgCategory::Integral) {
        narrow = (type == 'c');
    }

    if (!narrow) return phase1_mask(c);
    return phase1_mask(c) & mask_of(Opt::Locale);
}

[[nodiscard]] constexpr OptMask standard_base_mask(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
    case ArgCategory::Character:
        return mask_of(Opt::Locale);
    case ArgCategory::Integral:
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

[[nodiscard]] constexpr OptMask standard_effective_mask(ArgCategory c, char type) noexcept {
    OptMask m = standard_base_mask(c);
    if (reference::is_integer_presentation_type(c, type)) {
        m = static_cast<OptMask>(m | mask_of(Opt::Sign) | mask_of(Opt::Alternate)
                                     | mask_of(Opt::ZeroPad));
    }
    return m;
}

enum class SpecPolicy : unsigned char {
    Standard,
    Libcxx21,
};

[[nodiscard]] constexpr OptMask effective_mask(ArgCategory c, char type,
                                                SpecPolicy policy) noexcept {
    if (policy == SpecPolicy::Standard) return standard_effective_mask(c, type);
    return phase2_mask(c, type);
}

}  // namespace reference
}  // namespace petra

// ============================================================================
// Part 7 — the spec parser and validator, now driven by the policy table.
//
// The shapes are Oct 30's.  What changed is where the masks come from:
//
//   parse_spec    takes the CONSUME mask explicitly (the parser is
//                 mask-aware, and the mask is now per-implementation);
//   validate_spec takes the ALLOW mask explicitly.
//
// Both are still constexpr.
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
    // NEW TODAY: the number is too large for THIS implementation's
    // parser.  It is a malformed FORMAT STRING, not a bad argument:
    // the format string cannot be a format string for any arguments.
    NumberTooLarge,
};

struct SpecVerdict {
    SpecFailure failure{SpecFailure::None};
    Opt option{Opt::None};
    char type_char{'\0'};
    const char* detail{""};

    [[nodiscard]] bool accepted() const noexcept {
        return failure == SpecFailure::None;
    }
    [[nodiscard]] bool is_parse_failure() const noexcept {
        return failure == SpecFailure::Malformed
            || failure == SpecFailure::NumberTooLarge;
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

struct ParsedSpec {
    OptMask options{kMNone};
    char type_char{'\0'};
    bool has_nested{false};
    std::size_t nested_arg{0};
    bool nested_is_manual{false};
};

// Accumulate one decimal digit into `value`, saturating at UINT64_MAX.
//
// `too_large` is set when the value has passed the implementation's
// ceiling, or when it has overflowed 64 bits -- which passes every
// ceiling there is.  With no ceiling (the standard's rules), `too_large`
// is never set and the value simply saturates.
[[nodiscard]] constexpr std::uint64_t
append_digit(std::uint64_t value, char c, bool& too_large,
             const std::optional<std::uint64_t>& max_number) noexcept {
    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) {
        too_large = true;
        return std::numeric_limits<std::uint64_t>::max();
    }
    value = value * 10u + digit;
    if (max_number.has_value() && value > *max_number) too_large = true;
    return value;
}

// Parse one format-spec against a CONSUME mask.
//
// The GRAMMAR of a precision is checked FIRST, independently of the
// mask: a '.' with no value after it is malformed for EVERY argument,
// so it is a FormatParse, not an argument mismatch.
[[nodiscard]] constexpr ParsedSpec
parse_spec(std::string_view spec, OptMask m, SpecFailure& failure,
           Opt& bad_option,
           const std::optional<std::uint64_t>& max_number,
           const FillPolicy& fill) noexcept {
    ParsedSpec out{};
    std::size_t i = 0;
    const std::size_t n = spec.size();

    // fill-and-align.  Legal for every category.
    //
    // NEW TODAY: the fill is a CHARACTER -- a single Unicode scalar
    // value -- and not a byte.  So the scanner DECODES it, and an
    // ill-formed fill is a malformed spec unless this implementation
    // tolerates it (libstdc++ 15 substitutes U+FFFD and carries on).
    if (fill.unicode) {
        if (i < n) {
            const Utf8CodePoint cp = decode_utf8(spec.substr(i));
            if (cp.fault == FillFault::None) {
                // A well-formed scalar value.  It is the FILL if an
                // alignment option follows it; if it IS the alignment
                // option, it is that alone.
                if (i + cp.length < n && is_align(spec[i + cp.length])) {
                    if (cp.value == U'{' || cp.value == U'}') {
                        failure = SpecFailure::Malformed;
                        bad_option = Opt::None;
                        return out;
                    }
                    i += cp.length + 1;
                } else if (cp.length == 1 && is_align(spec[i])) {
                    ++i;
                }
            } else if (fill.is_fatal(cp.fault)) {
                // This implementation makes this kind of ill-formed
                // fill FATAL, so the SPEC is malformed.
                failure = SpecFailure::Malformed;
                bad_option = Opt::None;
                return out;
            } else {
                // This implementation RECOVERS: it substitutes U+FFFD
                // for the maximal subpart.  The fill is then one code
                // point, and it must still be followed by an alignment
                // option -- otherwise the spec has no valid reading.
                if (i + cp.length < n && is_align(spec[i + cp.length])) {
                    i += cp.length + 1;
                } else {
                    failure = SpecFailure::Malformed;
                    bad_option = Opt::None;
                    return out;
                }
            }
        }
    } else if (i + 1 < n && is_align(spec[i + 1])) {
        // The OLD, byte-oriented rule.  Kept so the lesson can
        // MEASURE the difference instead of asserting it.
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
        out.options = out.options | mask_of(Opt::Sign);
        ++i;
    }

    // alternate form.
    if (i < n && spec[i] == '#') {
        if (!has(m, Opt::Alternate)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Alternate;
            return out;
        }
        out.options = out.options | mask_of(Opt::Alternate);
        ++i;
    }

    // zero-padding.  If the category does not accept it, the '0' stays
    // in the input and libc++ reads it as a WIDTH.
    if (i < n && spec[i] == '0') {
        if (!has(m, Opt::ZeroPad)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::ZeroPad;
            return out;
        }
        out.options = out.options | mask_of(Opt::ZeroPad);
        ++i;
    }

    // width.
    if (i < n && spec[i] == '0') {
        failure = SpecFailure::Malformed;
        bad_option = Opt::None;
        return out;
    }
    if (i < n && is_digit(spec[i])) {
        std::uint64_t value = 0;
        bool too_large = false;
        while (i < n && is_digit(spec[i])) {
            value = append_digit(value, spec[i], too_large, max_number);
            ++i;
        }
        if (too_large) {
            failure = SpecFailure::NumberTooLarge;
            bad_option = Opt::None;
            return out;
        }
    } else if (i < n && spec[i] == '{') {
        ++i;
        const std::size_t id_start = i;
        while (i < n && is_digit(spec[i])) ++i;
        out.has_nested = true;
        out.nested_is_manual = (i > id_start);
        out.nested_arg = 0;
        std::uint64_t nested_value = 0;
        bool nested_too_large = false;
        for (std::size_t k = id_start; k < i; ++k) {
            nested_value = append_digit(nested_value, spec[k], nested_too_large,
                                        max_number);
        }
        if (nested_too_large) {
            failure = SpecFailure::NumberTooLarge;
            bad_option = Opt::None;
            return out;
        }
        if (nested_value
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            failure = SpecFailure::IndexOutOfRange;
            bad_option = Opt::None;
            return out;
        }
        out.nested_arg = static_cast<std::size_t>(nested_value);
        if (i >= n || spec[i] != '}') {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        ++i;
    }

    // precision.
    if (i < n && spec[i] == '.') {
        ++i;
        bool well_formed = false;
        if (i < n && is_digit(spec[i])) {
            std::uint64_t value = 0;
            bool too_large = false;
            while (i < n && is_digit(spec[i])) {
                value = append_digit(value, spec[i], too_large, max_number);
                ++i;
            }
            if (too_large) {
                failure = SpecFailure::NumberTooLarge;
                bad_option = Opt::None;
                return out;
            }
            well_formed = true;
        } else if (i < n && spec[i] == '{') {
            ++i;
            const std::size_t id_start = i;
            while (i < n && is_digit(spec[i])) ++i;
            out.has_nested = true;
            out.nested_is_manual = (i > id_start);
            out.nested_arg = 0;
            std::uint64_t nested_value = 0;
            bool nested_too_large = false;
            for (std::size_t k = id_start; k < i; ++k) {
                nested_value = append_digit(nested_value, spec[k], nested_too_large,
                                            max_number);
            }
            if (nested_too_large) {
                failure = SpecFailure::NumberTooLarge;
                bad_option = Opt::None;
                return out;
            }
            if (nested_value
                > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                failure = SpecFailure::IndexOutOfRange;
                bad_option = Opt::None;
                return out;
            }
            out.nested_arg = static_cast<std::size_t>(nested_value);
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
        out.options = out.options | mask_of(Opt::Precision);
    }

    // locale-specific form.
    if (i < n && spec[i] == 'L') {
        if (!has(m, Opt::Locale)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Locale;
            return out;
        }
        out.options = out.options | mask_of(Opt::Locale);
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

// Validate one parsed spec against an ALLOW mask.
[[nodiscard]] constexpr SpecVerdict
validate_spec(ArgCategory category, const ParsedSpec& spec,
              OptMask allowed) noexcept {
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
// Part 8 — the type-aware scanner (Oct 30's shape, policy-driven).
// ============================================================================

namespace petra {
namespace detail {

enum class ScanFailure : unsigned char {
    None,
    Malformed,
    TypeNotAllowed,
    OptionNotAllowed,
    IndexOutOfRange,
    WidthArgNotInteger,
    MixedNumbering,
    // NEW TODAY: the number is too large for THIS implementation.
    NumberTooLarge,
};

struct ScanResult {
    ScanFailure failure{ScanFailure::None};
    Opt option{Opt::None};
    char type_char{'\0'};
    std::size_t field{0};
    std::size_t arg_index{0};
    const char* detail{""};

    [[nodiscard]] bool ok() const noexcept {
        return failure == ScanFailure::None;
    }
    [[nodiscard]] bool is_parse_failure() const noexcept {
        return failure == ScanFailure::Malformed
            || failure == ScanFailure::MixedNumbering
            || failure == ScanFailure::NumberTooLarge;
    }
};

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
                               const ImplPolicy& policy) noexcept
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
    constexpr bool parse_arg_id(std::size_t& i, bool& manual,
                                std::size_t& value) noexcept {
        const std::size_t start = i;
        while (i < fmt_.size() && is_digit(fmt_[i])) ++i;
        manual = (i > start);
        value = 0;
        arg_id_too_large_ = false;
        if (manual) {
            std::uint64_t acc = 0;
            for (std::size_t k = start; k < i; ++k) {
                acc = append_digit(acc, fmt_[k], arg_id_too_large_,
                                   policy_.max_number);
            }
            value = static_cast<std::size_t>(acc);
        }
        return true;
    }

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
        if (arg_id_too_large_) {
            // The index does not FIT in the implementation's parser.
            // That is a malformed FORMAT STRING -- the stdlib rejects it
            // before it looks at the argument list -- and not an index
            // that merely points past the end of it.
            r.failure = ScanFailure::NumberTooLarge;
            r.field = field_no;
            r.detail = "the argument index is too large for this implementation";
            return false;
        }
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
            const ParsedSpec parsed =
                parse_spec(spec, policy_consume_mask(cat, policy_), sf, bad,
                           policy_.max_number, policy_.fill);
            if (sf != SpecFailure::None) {
                r.failure = to_scan_failure(sf);
                r.option = bad;
                r.field = field_no;
                r.arg_index = index;
                r.detail = (sf == SpecFailure::Malformed)
                               ? "the format spec is malformed"
                           : (sf == SpecFailure::NumberTooLarge)
                               ? "the number in the format spec is too large "
                                 "for this implementation"
                               : "the format spec is not valid for this argument";
                return false;
            }

            const SpecVerdict v =
                validate_spec(cat, parsed, policy_allow_mask(cat, parsed.type_char, policy_));
            if (!v.accepted()) {
                r.failure = to_scan_failure(v.failure);
                r.option = v.option;
                r.type_char = v.type_char;
                r.field = field_no;
                r.arg_index = index;
                r.detail = v.detail;
                return false;
            }

            if (parsed.has_nested) {
                if (parsed.nested_is_manual != manual) {
                    r.failure = ScanFailure::MixedNumbering;
                    r.field = field_no;
                    r.detail = "manual and automatic argument numbering are mixed";
                    return false;
                }

                std::size_t nested_index = parsed.nested_arg;
                if (!parsed.nested_is_manual) {
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

            i = j + 1;
            return true;
        }

        if (fmt_[i] == '}') {
            ++i;
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
        case SpecFailure::NumberTooLarge:    return ScanFailure::NumberTooLarge;
        }
        return ScanFailure::Malformed;
    }

    std::string_view fmt_;
    CategoryTable cats_;
    const ImplPolicy& policy_;
    std::size_t next_auto_{0};
    std::size_t field_count_{0};
    bool manual_seen_{false};
    bool auto_seen_{false};
    bool mixed_{false};
    bool arg_id_too_large_{false};
};

[[nodiscard]] constexpr ScanResult
scan_format_string(std::string_view fmt, CategoryTable cats,
                    const ImplPolicy& policy) noexcept {
    return TypeAwareScanner{fmt, cats, policy}.run();
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 9 — the MESSAGE TABLE, carried forward from Oct 28 and DEMOTED.
//
// Oct 30's table was libc++ 21's message text, which is not part of any
// standard.  The table is unchanged; it is still the fallback for
// failures that are not about the spec's shape.
//
// Note the measurement in Part 13: libstdc++ 15's messages are a
// DIFFERENT set of strings, so under a libstdc++ build the fallback
// fires on more cells.  That is a property of the MESSAGES, not of the
// policy table, and it is exactly why the arc's own classification
// does not depend on them.
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
// Part 10 — the CLASSIFIER (Oct 30's shape, policy-driven).
//
//   Stage A: the TYPE-AWARE scanner.  It answers BOTH questions — is the
//            string well-formed (FormatParse), and is the spec valid for
//            the argument it names (FormatArgument) — so it decides the
//            whole spec-vs-argument class WITHOUT reading the message.
//   Stage B: the message table, for the failures that are not about the
//            spec's shape: a value that does not fit the selected
//            presentation type, an argument index past the end of the
//            list, a user's own formatter throwing.
//   Fallback: Format.
// ============================================================================

// The two shapes of the fallback.
//
//   Discard  the old one.  The scanner's SILENCE is read as an
//            affirmation — "scanner disagrees: string is well-formed" —
//            and a parse verdict the scanner never earned is thrown
//            away.
//
//   Carry    the new one.  The scanner's silence is carried into the
//            decision AS SILENCE.  The scanner models a subset of
//            [format.string.std], so "no objection" is not "affirmed
//            well-formed", and a parse verdict from the stdlib survives
//            into FormatParse.
//
// The two differ in exactly one branch.  Keeping them side by side is
// what lets the lesson MEASURE the difference instead of asserting it.
enum class Fallback : unsigned char { Discard, Carry };

struct Classification {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    bool used_message_table{false};
    const char* reason{""};
    // The scanner's verdict, CARRIED.  Not a decision: the record of
    // what the scanner said, so that a caller (and the lesson) can see
    // whether the fallback ran at all.
    ScanFailure scanner_verdict{ScanFailure::None};
};

[[nodiscard]] inline Classification
classify_format_failure(std::string_view fmt, const char* what,
                         CategoryTable cats, const ImplPolicy& policy,
                         Fallback fallback = Fallback::Carry) noexcept {
    Classification c{};

    const ScanResult s = scan_format_string(fmt, cats, policy);
    c.scanner_verdict = s.failure;

    switch (s.failure) {
    case ScanFailure::None:
        // The scanner had NO OBJECTION.  It did not affirm anything:
        // it models a subset of the rules, and this failure is not in
        // the subset it models.  Carry that forward as silence.
        break;

    case ScanFailure::Malformed:
    case ScanFailure::MixedNumbering:
    case ScanFailure::NumberTooLarge:
        c.kind = FormatErrorKind::FormatParse;
        c.message = std::string{what} + " [format string: " + s.detail + "]";
        c.reason = s.detail;
        return c;

    case ScanFailure::TypeNotAllowed:
    case ScanFailure::OptionNotAllowed:
    case ScanFailure::IndexOutOfRange:
    case ScanFailure::WidthArgNotInteger:
        c.kind = FormatErrorKind::FormatArgument;
        c.message = std::string{what} + " [argument " + std::to_string(s.arg_index)
                    + ": " + s.detail + "]";
        c.reason = s.detail;
        return c;
    }

    c.used_message_table = true;
    switch (verdict_from_message(what)) {
    case MessageVerdict::Parse:
        if (fallback == Fallback::Carry) {
            // The stdlib says the FORMAT STRING is broken.  The scanner
            // had no opinion.  Silence is not a contradiction, so the
            // verdict survives.
            c.kind = FormatErrorKind::FormatParse;
            c.message = std::string{what} + " [the scanner had no rule for this]";
            c.reason = "the stdlib reports a parse failure the scanner did not model";
            return c;
        }
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
// Part 11 — the two-pass runtime implementation.
//
// Unchanged from Oct 30 apart from one thing: the classifier is now
// given the LINKED implementation's policy rather than a hard-wired
// "standard".  That is the point of the table — the same source is
// correct against whichever stdlib it is linked with.
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
    const ImplPolicy& policy = linked_policy();

    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        const Classification c =
            classify_format_failure(fmt, e.what(), table, policy);
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
            classify_format_failure(fmt, e.what(), table, policy);
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
// Part 12 — the test type (a user-defined formatter) and the
//           compile-time pins.
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

namespace petra {
namespace detail {

// The type-category table (Oct 30's pins).
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

// The base table.
static_assert(!has(base_mask(ArgCategory::Boolean), Opt::Sign));
static_assert(has(base_mask(ArgCategory::Boolean), Opt::Locale));
static_assert(!has(base_mask(ArgCategory::Character), Opt::Sign));
static_assert(has(base_mask(ArgCategory::Character), Opt::Locale));
static_assert(has(base_mask(ArgCategory::Integral), Opt::Sign));
static_assert(has(base_mask(ArgCategory::FloatingPoint), Opt::Precision));
static_assert(!has(base_mask(ArgCategory::Integral), Opt::Precision));
static_assert(has(base_mask(ArgCategory::String), Opt::Precision));
static_assert(!has(base_mask(ArgCategory::String), Opt::Sign));
static_assert(has(base_mask(ArgCategory::Pointer), Opt::ZeroPad));
static_assert(!has(base_mask(ArgCategory::Pointer), Opt::Sign));

// The integer-presentation types.
static_assert(is_integer_presentation_type(ArgCategory::Integral, 'c'));
static_assert(!is_integer_presentation_type(ArgCategory::Character, 'c'));
static_assert(!is_integer_presentation_type(ArgCategory::Boolean, 'c'));
static_assert(is_integer_presentation_type(ArgCategory::Boolean, 'd'));
static_assert(is_integer_presentation_type(ArgCategory::Character, 'x'));

// THE DIVERGENCE, pinned from both sides.
static_assert(has(allow_mask(ArgCategory::Integral, 'c'), Opt::Sign));
static_assert(!has(policy_allow_mask(ArgCategory::Integral, 'c',
                                      kImpls[impl_index_of("libc++ 21")]),
                    Opt::Sign));
static_assert(has(policy_allow_mask(ArgCategory::Integral, 'c',
                                     kImpls[impl_index_of("libc++ 21")]),
                   Opt::Locale));
// charT is NOT divergent: both policies narrow it.
static_assert(!has(allow_mask(ArgCategory::Character, 'c'), Opt::Sign));
static_assert(!has(policy_allow_mask(ArgCategory::Character, 'c',
                                     kImpls[impl_index_of("libc++ 21")]),
                    Opt::Sign));
// bool is NOT divergent.
static_assert(!has(allow_mask(ArgCategory::Boolean, 's'), Opt::Sign));
static_assert(!has(policy_allow_mask(ArgCategory::Boolean, 's',
                                     kImpls[impl_index_of("libc++ 21")]),
                    Opt::Sign));
static_assert(has(allow_mask(ArgCategory::Boolean, 'd'), Opt::Sign));
static_assert(has(policy_allow_mask(ArgCategory::Boolean, 'd',
                                     kImpls[impl_index_of("libc++ 21")]),
                   Opt::Sign));
// libstdc++ 15's table is EMPTY, so it agrees with the standard.
static_assert(kImpls[impl_index_of("libstdc++ 15")].allow_count == 0);

// The legal presentation types.
static_assert(is_legal_type_char(ArgCategory::Boolean, 's'));
static_assert(!is_legal_type_char(ArgCategory::Boolean, 'c'));
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
static_assert(!is_known_type_char('z'));
static_assert(is_known_type_char('c'));

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 13 — the harness.
//
// Section 1  the policy table: the base table, the int-pres table, the
//            consume table, and the per-impl override rows
// Section 2  EQUIVALENCE: the new table against Oct 30's five mask
//            functions, over all 7 x 19 x 2 cells
// Section 3  the 1,596-cell grid: model vs the LINKED stdlib
// Section 4  DERIVATION: the override table derived from the measured
//            grid, checked against the hand-written table
// Section 5  the consume mask vs the allow mask: what each one changes
// Section 6  the arc's own paths, end to end
// Section 7  the numeric ceiling: measured on the linked stdlib,
//            then checked against the hand-written policy row
// Section 8  THE CENSUS: the carried verdict against the discarding
//            fallback, over the grid and over the families the grid
//            omits
// Section 9  the arc's own paths, extended
// ============================================================================

namespace {

using namespace petra;
using namespace petra::detail;

constexpr std::size_t kPrefixCount = 12;
constexpr std::size_t kTypeCount = 19;
constexpr std::size_t kRowCount = 7;
constexpr std::size_t kCellCount = kPrefixCount * kTypeCount;   // 228
constexpr std::size_t kGridCells = kRowCount * kCellCount;       // 1596

// The grid's shape is a fact about the GRID, so it is checked once,
// here — not typed into six different assertions further down.
static_assert(kCellCount == 228, "12 prefixes x 19 type chars");
static_assert(kGridCells == 1596, "7 rows x 228 cells");

constexpr const char* kPrefixes[] = {
    "", "+", "-", " ", "#", "0", "10", ".2", "L", "<", "*>10", "<10.2",
};

struct TypeChar {
    const char* text;
    char ch;
};

constexpr TypeChar kTypeChars[] = {
    {"", '\0'}, {"a", 'a'}, {"A", 'A'}, {"b", 'b'}, {"B", 'B'}, {"c", 'c'},
    {"d", 'd'}, {"e", 'e'}, {"f", 'f'}, {"F", 'F'}, {"g", 'g'}, {"G", 'G'},
    {"o", 'o'}, {"p", 'p'}, {"s", 's'}, {"x", 'x'}, {"X", 'X'}, {"P", 'P'},
    {"?", '?'},
};

// The index of a prefix in kPrefixes, found at compile time.
[[nodiscard]] constexpr std::size_t prefix_index(const char* p) noexcept {
    for (std::size_t i = 0; i < kPrefixCount; ++i) {
        std::size_t j = 0;
        for (; kPrefixes[i][j] != '\0' && p[j] != '\0'; ++j) {
            if (kPrefixes[i][j] != p[j]) break;
        }
        if (kPrefixes[i][j] == '\0' && p[j] == '\0') return i;
    }
    return kPrefixCount;
}

constexpr std::size_t kSignPrefix     = prefix_index("+");
constexpr std::size_t kAltPrefix      = prefix_index("#");
constexpr std::size_t kZeroPrefix     = prefix_index("0");
constexpr std::size_t kPrecPrefix     = prefix_index(".2");
constexpr std::size_t kLocalePrefix   = prefix_index("L");
constexpr std::size_t kEmptyPrefix    = prefix_index("");

static_assert(kSignPrefix != kPrefixCount);
static_assert(kAltPrefix != kPrefixCount);
static_assert(kZeroPrefix != kPrefixCount);
static_assert(kPrecPrefix != kPrefixCount);
static_assert(kLocalePrefix != kPrefixCount);
static_assert(kEmptyPrefix == 0);

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

// The grid rows: one per representative argument.  Rows 2 and 3 are
// both the INTEGRAL category, which is why the divergence shows up in
// two rows of the grid and one cell of the matrix.
struct GridRow {
    const char* label;
    ArgCategory category;
};

constexpr GridRow kRows[kRowCount] = {
    {"bool",   ArgCategory::Boolean},
    {"char",   ArgCategory::Character},
    {"int",    ArgCategory::Integral},
    {"long",   ArgCategory::Integral},
    {"double", ArgCategory::FloatingPoint},
    {"string", ArgCategory::String},
    {"voidp",  ArgCategory::Pointer},
};

[[nodiscard]] std::string spec_text(std::size_t prefix_i, std::size_t type_i) {
    return std::string{kPrefixes[prefix_i]} + std::string{kTypeChars[type_i].text};
}

enum class Outcome { Ok, Reject };

template <typename T>
Outcome measure(const std::string& fmt, T& value, std::string& message) {
    try {
        (void)std::vformat(fmt, std::make_format_args(value));
        return Outcome::Ok;
    } catch (const std::format_error& e) {
        message = e.what();
        return Outcome::Reject;
    }
}

// The model's verdict for one spec against one category, under one
// policy.  `reason` names the failing option, for the diagnostics.
bool model_verdict(ArgCategory cat, std::string_view spec,
                    const ImplPolicy& policy, std::string& reason) {
    SpecFailure sf = SpecFailure::None;
    Opt bad = Opt::None;
    const ParsedSpec parsed =
        parse_spec(spec, policy_consume_mask(cat, policy), sf, bad,
                   policy.max_number, policy.fill);
    if (sf != SpecFailure::None) {
        reason = (sf == SpecFailure::Malformed) ? "malformed" : "option not allowed";
        return false;
    }
    const SpecVerdict v =
        validate_spec(cat, parsed, policy_allow_mask(cat, parsed.type_char, policy));
    if (!v.accepted()) {
        reason = v.detail;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Section 1 — the policy table.
// ---------------------------------------------------------------------------
void section1(counters& c) {
    section("Section 1 — the policy table");

    std::printf("  TABLE 1: the standard's BASE permissions, per category\n");
    for (const BaseRow& row : kBaseTable) {
        std::string m;
        if (has(row.base, Opt::Sign))     m += "sign ";
        if (has(row.base, Opt::Alternate)) m += "alt ";
        if (has(row.base, Opt::ZeroPad))   m += "zero ";
        if (has(row.base, Opt::Precision)) m += "prec ";
        if (has(row.base, Opt::Locale))    m += "locale ";
        std::printf("    %-16s %s\n",
                    std::string{category_name(row.category)}.c_str(), m.c_str());
    }

    std::printf("\n  TABLE 2: the integer-presentation types, per category\n");
    for (const IntPresRow& row : kIntPresTable) {
        std::printf("    %-16s \"%s\"\n",
                    std::string{category_name(row.category)}.c_str(), row.types);
    }

    std::printf("\n  TABLE 3: the CONSUME mask, per category\n");
    for (const ConsumeRow& row : kSharedConsumeTable) {
        std::string m;
        if (has(row.mask, Opt::Sign))     m += "sign ";
        if (has(row.mask, Opt::Alternate)) m += "alt ";
        if (has(row.mask, Opt::ZeroPad))   m += "zero ";
        if (has(row.mask, Opt::Precision)) m += "prec ";
        if (has(row.mask, Opt::Locale))    m += "locale ";
        std::printf("    %-16s %s\n",
                    std::string{category_name(row.category)}.c_str(), m.c_str());
    }

    std::printf("\n  TABLE 4: the ALLOW override rows, per implementation\n");
    for (std::size_t i = 0; i < kImplCount; ++i) {
        const ImplPolicy& impl = kImpls[i];
        std::printf("    %-14s rows=%zu\n", impl.name, impl.allow_count);
        for (std::size_t r = 0; r < impl.allow_count; ++r) {
            const AllowRow& row = impl.allow_rows[r];
            std::printf("      -> category=%-10s type=%-4s mask=%s\n",
                        std::string{category_name(row.category)}.c_str(),
                        row.any_type ? "any" : (row.type_char == '\0'
                                                 ? "(none)" :
                                                 std::string(1, row.type_char).c_str()),
                        row.mask == kMLocale ? "locale" : "(other)");
        }
    }

    std::printf("\n  the linked implementation is \"%s\"\n", linked_policy().name);
    std::printf("  (so the arc models the stdlib it is actually linked with,\n"
                "   and the SAME source is correct under both toolchains.)\n");

    c.expect_eq(kImpls[impl_index_of("libc++ 21")].allow_count, 1,
                "libc++ 21 override rows");
    c.expect_eq(kImpls[impl_index_of("standard")].allow_count, 0,
                "standard override rows");
    c.expect_eq(kImpls[impl_index_of("libstdc++ 15")].allow_count, 0,
                "libstdc++ 15 override rows");
    c.check(linked_policy().name != nullptr);
}

// ---------------------------------------------------------------------------
// Section 2 — EQUIVALENCE with Oct 30's five mask functions.
//
// The new table must agree with the old code paths on EVERY cell:
// 7 categories x 19 type chars x 2 policies = 266 comparisons, plus the
// 7 consume masks.
// ---------------------------------------------------------------------------
void section2(counters& c) {
    section("Section 2 — EQUIVALENCE: the table vs Oct 30's five mask functions");

    std::size_t compared = 0;
    std::size_t mismatches = 0;

    for (const GridRow& row : kRows) {
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            const char type = kTypeChars[t].ch;

            const OptMask std_new =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("standard")]);
            const OptMask std_old =
                reference::standard_effective_mask(row.category, type);
            ++compared;
            if (std_new != std_old) {
                ++mismatches;
                std::printf("  MISMATCH standard %s type '%c': table=%u old=%u\n",
                            std::string{category_name(row.category)}.c_str(), type,
                            static_cast<unsigned>(std_new),
                            static_cast<unsigned>(std_old));
            }

            const OptMask libc_new =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("libc++ 21")]);
            const OptMask libc_old = reference::phase2_mask(row.category, type);
            ++compared;
            if (libc_new != libc_old) {
                ++mismatches;
                std::printf("  MISMATCH libc++ 21 %s type '%c': table=%u old=%u\n",
                            std::string{category_name(row.category)}.c_str(), type,
                            static_cast<unsigned>(libc_new),
                            static_cast<unsigned>(libc_old));
            }
        }
    }

    std::size_t consume_compared = 0;
    for (const GridRow& row : kRows) {
        const OptMask new_c = policy_consume_mask(row.category, linked_policy());
        const OptMask old_c = reference::phase1_mask(row.category);
        ++consume_compared;
        if (new_c != old_c) {
            ++mismatches;
            std::printf("  MISMATCH consume %s: table=%u old=%u\n",
                        std::string{category_name(row.category)}.c_str(),
                        static_cast<unsigned>(new_c), static_cast<unsigned>(old_c));
        }
    }

    std::printf("  allow-mask cells compared: %zu  (7 categories x 19 types x 2 policies)\n",
                compared);
    std::printf("  consume masks compared: %zu\n", consume_compared);
    std::printf("  mismatches: %zu\n", mismatches);
    std::printf("  -> the table is EXACTLY equivalent to the two code paths it\n"
                "     replaces.  Not \"close\": every cell.\n");

    // Now the shape of the difference, which is the actual finding.
    std::printf("\n  the cells where the two policies DISAGREE:\n");
    std::size_t divergent_cells = 0;
    for (const GridRow& row : kRows) {
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            const char type = kTypeChars[t].ch;
            const OptMask a =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("standard")]);
            const OptMask b =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("libc++ 21")]);
            if (a == b) continue;
            ++divergent_cells;
            std::printf("    category=%-10s type='%c'  standard=[%s]  libc++=[%s]\n",
                        std::string{category_name(row.category)}.c_str(), type,
                        mask_text(a).c_str(), mask_text(b).c_str());
        }
    }
    std::printf("  -> %zu divergent cell(s) out of %zu.\n", divergent_cells, compared);

    c.expect_eq(compared, 266, "allow-mask cells compared");
    c.expect_eq(consume_compared, 7, "consume masks compared");
    c.expect_eq(mismatches, 0, "equivalence mismatches");
    c.expect_eq(divergent_cells, 2, "divergent cells (integral and long are both Integral)");
}

// ---------------------------------------------------------------------------
// Section 3 — the 1,596-cell grid, measured against the LINKED stdlib.
// ---------------------------------------------------------------------------
void section3(counters& c) {
    section("Section 3 — the 1,596-cell grid vs the LINKED stdlib");

    const ImplPolicy& linked = linked_policy();

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    std::size_t libc_ok = 0;
    std::size_t libc_reject = 0;
    std::size_t agree_standard = 0;
    std::size_t agree_libcxx = 0;

    // The cells where the two POLICIES disagree.  This is the
    // BUILD-INVARIANT quantity: the same 10 cells under libc++ and
    // under libstdc++.  (The draft measured model(standard) against
    // the LINKED STDLIB instead, which is 10 cells under libc++ and 0
    // under libstdc++ — a build-dependent number masquerading as a
    // fact about the rules.)
    std::vector<std::string> divergent;

    // Of those divergent cells, how many the LINKED STDLIB sided with
    // libc++ 21's model rather than the standard's model.  This one IS
    // build-dependent, and it is the mirror image of the two agreement
    // counts above.
    std::size_t stdlib_sides_with_libcxx = 0;

    const auto run = [&](const char* label, ArgCategory cat, auto& value) {
        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string fmt = "{:" + spec_text(p, t) + "}";
                std::string message;
                const Outcome outcome = measure(fmt, value, message);
                if (outcome == Outcome::Ok) ++libc_ok; else ++libc_reject;

                std::string reason;
                const bool model_std =
                    model_verdict(cat, spec_text(p, t),
                                  kImpls[impl_index_of("standard")], reason);
                const bool model_libc =
                    model_verdict(cat, spec_text(p, t),
                                  kImpls[impl_index_of("libc++ 21")], reason);

                if (model_std == (outcome == Outcome::Ok)) ++agree_standard;
                if (model_libc == (outcome == Outcome::Ok)) ++agree_libcxx;

                // The DIVERGENCE is between the two POLICIES, not
                // between a policy and the linked stdlib.  The linked
                // stdlib only tells us which side the BUILD happens
                // to be on.
                if (model_std != model_libc) {
                    const bool stdlib_agrees_with_libcxx =
                        (model_libc == (outcome == Outcome::Ok));
                    if (stdlib_agrees_with_libcxx) ++stdlib_sides_with_libcxx;
                    divergent.push_back(std::string{label} + "  spec=\""
                                        + spec_text(p, t) + "\"  standard="
                                        + (model_std ? "ACCEPT" : "REJECT")
                                        + "  libc++ 21=" + (model_libc ? "ACCEPT" : "REJECT")
                                        + "  linked stdlib says "
                                        + (outcome == Outcome::Ok ? "OK" : "REJECT"));
                }
            }
        }
    };

    run("bool",   ArgCategory::Boolean,       b);
    run("char",   ArgCategory::Character,     ch);
    run("int",    ArgCategory::Integral,      i);
    run("long",   ArgCategory::Integral,      l);
    run("double", ArgCategory::FloatingPoint, d);
    run("string", ArgCategory::String,        sv);
    run("voidp",  ArgCategory::Pointer,       vp);

    const std::size_t total = libc_ok + libc_reject;

    std::printf("  linked stdlib: %s\n", linked.name);
    std::printf("  grid cells: %zu   stdlib outcomes: OK=%zu REJECT=%zu\n",
                total, libc_ok, libc_reject);
    std::printf("  model(standard)  vs stdlib: %zu/%zu agree\n", agree_standard, total);
    std::printf("  model(libc++ 21) vs stdlib: %zu/%zu agree\n", agree_libcxx, total);

    if (!divergent.empty()) {
        std::printf("\n  the cells where the two POLICIES disagree:\n");
        for (const std::string& div : divergent) {
            std::printf("    %s\n", div.c_str());
        }
        std::printf("    -> the known upstream divergence: 'c' IS an integer\n"
                    "       presentation type for integral (Table 107), so the\n"
                    "       sign / alternate / zero options are valid there.\n");
    }

    std::printf("\n  of those %zu cells, the linked stdlib sides with libc++ 21's\n"
                "  model on %zu of them, and with the standard's model on %zu.\n",
                divergent.size(), stdlib_sides_with_libcxx,
                divergent.size() - stdlib_sides_with_libcxx);

    std::printf("\n  The same source, built against the other stdlib, produces the\n"
                "  mirror image of these numbers.  Which implementation is\n"
                "  'the stdlib' is a property of the BUILD; the 10 cells are a\n"
                "  property of the RULES.\n");

    c.expect_eq(total, kGridCells, "grid cell count");
    // BUILD-INVARIANT: the two policies disagree on exactly 10 cells,
    // and it is the SAME 10 under either toolchain.
    c.expect_eq(divergent.size(), 10, "policy-vs-policy divergent cells");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(agree_libcxx, kGridCells, "model(libc++ 21) vs libc++");
        c.expect_eq(agree_standard, kGridCells - 10, "model(standard) vs libc++");
        c.expect_eq(stdlib_sides_with_libcxx, 10, "libc++ sides with libc++ 21");
    } else {
        c.expect_eq(agree_standard, kGridCells, "model(standard) vs libstdc++");
        c.expect_eq(agree_libcxx, kGridCells - 10, "model(libc++ 21) vs libstdc++");
        c.expect_eq(stdlib_sides_with_libcxx, 0, "libstdc++ sides with the standard");
    }
}

// ---------------------------------------------------------------------------
// Section 4 — DERIVATION.
//
// The override table is derived from the measured grid, not asserted.
// For each (category, type) cell the lesson asks: what mask would make
// the model agree with the stdlib on every prefix?  The answer is
// derived, then checked against the hand-written table.
//
// This is what makes "adding a third implementation is a data change"
// an operation rather than a slogan.
// ---------------------------------------------------------------------------
void section4(counters& c) {
    section("Section 4 — DERIVATION: the override table, derived from the measurement");

    const ImplPolicy& linked = linked_policy();

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    // For each grid row and type char, the DERIVED allow mask: the set
    // of options the stdlib accepts for that (category, type) pair.
    //
    // A cell is "legal" if the stdlib accepts the bare type char for
    // that argument; if it does not, the type is not legal for the
    // category and no mask can explain the row.
    struct Derived {
        bool legal{false};
        OptMask mask{kMNone};
    };
    Derived derived[kRowCount][kTypeCount];

    const auto derive = [&](const char* label, std::size_t row, auto& value) {
        (void)label;
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            const std::string fmt = "{:" + std::string{kTypeChars[t].text} + "}";
            std::string message;
            const Outcome outcome = measure(fmt, value, message);
            if (outcome != Outcome::Ok) continue;   // type not legal here
            derived[row][t].legal = true;

            OptMask m = kMNone;
            const std::pair<std::size_t, Opt> probes[] = {
                {kSignPrefix, Opt::Sign},
                {kAltPrefix,  Opt::Alternate},
                {kZeroPrefix, Opt::ZeroPad},
                {kPrecPrefix, Opt::Precision},
                {kLocalePrefix, Opt::Locale},
            };
            for (const auto& [prefix_i, opt] : probes) {
                const std::string spec = std::string{kPrefixes[prefix_i]}
                                      + std::string{kTypeChars[t].text};
                const std::string f = "{:" + spec + "}";
                std::string msg;
                if (measure(f, value, msg) == Outcome::Ok) m = m | mask_of(opt);
            }
            derived[row][t].mask = m;
        }
    };

    derive("bool",   0, b);
    derive("char",   1, ch);
    derive("int",    2, i);
    derive("long",   3, l);
    derive("double", 4, d);
    derive("string", 5, sv);
    derive("voidp",  6, vp);

    // Rows 2 and 3 are both the INTEGRAL category: their derived masks
    // must agree, or the model is not category-shaped.
    std::size_t row_disagreements = 0;
    for (std::size_t t = 0; t < kTypeCount; ++t) {
        if (derived[2][t].legal != derived[3][t].legal
            || derived[2][t].mask != derived[3][t].mask) {
            ++row_disagreements;
        }
    }
    std::printf("  the int row and the long row are both INTEGRAL: their\n"
                "  derived masks agree on %zu of %zu type chars\n",
                kTypeCount - row_disagreements, kTypeCount);

    // The DERIVED override table: the cells where the standard's model
    // disagrees with the measurement.
    //
    // The table is keyed by (category, type), NOT by grid row.  Rows 2
    // and 3 are both the INTEGRAL category, so they measure the SAME
    // matrix cell twice; the table must still hold ONE row for it.
    // That is the difference between a table and a log.
    std::vector<std::string> derived_rows;
    std::vector<AllowRow> overrides;
    std::vector<std::pair<ArgCategory, char>> emitted;

    for (std::size_t row = 0; row < kRowCount; ++row) {
        const ArgCategory cat = kRows[row].category;
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            if (!derived[row][t].legal) continue;
            const char type = kTypeChars[t].ch;
            const OptMask std_mask = allow_mask(cat, type);
            if (std_mask == derived[row][t].mask) continue;

            bool already = false;
            for (const auto& e : emitted) {
                if (e.first == cat && e.second == type) { already = true; break; }
            }
            if (already) continue;
            emitted.emplace_back(cat, type);

            derived_rows.push_back(
                std::string{"{ category = "} + std::string{category_name(cat)}
                + ", type = '" + std::string(1, type == '\0' ? ' ' : type)
                + "', mask = " + (derived[row][t].mask == kMLocale ? "locale" : "other")
                + " }   // measured on the " + kRows[row].label + " row");
            overrides.push_back(
                AllowRow{cat, type, false, derived[row][t].mask});
        }
    }

    std::printf("\n  DERIVED override rows for \"%s\": %zu\n", linked.name,
                derived_rows.size());
    for (const std::string& r : derived_rows) {
        std::printf("    %s\n", r.c_str());
    }
    if (derived_rows.empty()) {
        std::printf("    (none: this implementation's allow matrix IS the\n"
                    "     standard's, cell for cell)\n");
    }

    // Now the check that matters: the derived table must REPRODUCE the
    // whole measured grid.
    //
    // The derived table is expressed as a per-(category, type) override
    // list, and the model is re-run with it.
    const ImplPolicy derived_policy{
        "derived", overrides.data(), overrides.size(),
        kSharedConsumeTable, 6,
        linked_policy().max_number, linked_policy().ceiling_note};

    std::size_t agree_derived = 0;
    std::size_t total = 0;
    const auto rerun = [&](ArgCategory cat, auto& value) {
        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string fmt = "{:" + spec_text(p, t) + "}";
                std::string message;
                const Outcome outcome = measure(fmt, value, message);
                std::string reason;
                const bool model =
                    model_verdict(cat, spec_text(p, t), derived_policy, reason);
                ++total;
                if (model == (outcome == Outcome::Ok)) ++agree_derived;
            }
        }
    };
    rerun(ArgCategory::Boolean,       b);
    rerun(ArgCategory::Character,     ch);
    rerun(ArgCategory::Integral,      i);
    rerun(ArgCategory::Integral,      l);
    rerun(ArgCategory::FloatingPoint, d);
    rerun(ArgCategory::String,        sv);
    rerun(ArgCategory::Pointer,       vp);

    std::printf("\n  the DERIVED table, re-run against the measured grid:\n");
    std::printf("    agree %zu/%zu\n", agree_derived, total);

    // And the derived table must MATCH the hand-written table.
    const ImplPolicy& handwritten = linked_policy();
    std::size_t row_mismatch = 0;
    if (handwritten.allow_count != overrides.size()) {
        ++row_mismatch;
        std::printf("    row-count differs: hand-written %zu, derived %zu\n",
                    handwritten.allow_count, overrides.size());
    }
    for (std::size_t r = 0; r < handwritten.allow_count && r < overrides.size(); ++r) {
        if (handwritten.allow_rows[r].category != overrides[r].category
            || handwritten.allow_rows[r].type_char != overrides[r].type_char
            || handwritten.allow_rows[r].mask != overrides[r].mask) {
            ++row_mismatch;
            std::printf("    row %zu differs: hand-written {%s, '%c', %u} "
                        "derived {%s, '%c', %u}\n", r,
                        std::string{category_name(handwritten.allow_rows[r].category)}.c_str(),
                        handwritten.allow_rows[r].type_char,
                        static_cast<unsigned>(handwritten.allow_rows[r].mask),
                        std::string{category_name(overrides[r].category)}.c_str(),
                        overrides[r].type_char,
                        static_cast<unsigned>(overrides[r].mask));
        }
    }

    std::printf("    hand-written vs derived: %zu mismatch(es)\n", row_mismatch);
    std::printf("    -> the table is DERIVED from a measurement and CHECKED\n"
                "       against the hand-written one.  A number in a comment\n"
                "       is not a check; a number the program computes is.\n");

    c.expect_eq(row_disagreements, 0, "int/long derived-mask agreement");
    c.expect_eq(agree_derived, kGridCells, "derived table vs measured grid");
    c.expect_eq(row_mismatch, 0, "derived vs hand-written override table");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(derived_rows.size(), 1, "libc++ 21 derived override rows");
    } else {
        c.expect_eq(derived_rows.size(), 0, "libstdc++ 15 derived override rows");
    }
}

// ---------------------------------------------------------------------------
// Section 5 — the consume mask vs the allow mask.
//
// What does each mask actually change?
// ---------------------------------------------------------------------------
void section5(counters& c) {
    section("Section 5 — the consume mask is a diagnostic knob; the allow mask decides");

    // The "everything" consume mask: the parser consumes every option
    // it knows about, whatever the category.
    const OptMask kConsumeEverything =
        kMSign | kMAlternate | kMZero | kMPrecision | kMLocale;

    // A consume table in which EVERY category consumes every option.
    // UserDefined is deliberately absent: an unknown category has no
    // opinion, so its consume mask stays empty.
    constexpr ConsumeRow kWideConsumeTable[] = {
        {ArgCategory::Boolean,       kConsumeEverything},
        {ArgCategory::Character,     kConsumeEverything},
        {ArgCategory::Integral,      kConsumeEverything},
        {ArgCategory::FloatingPoint, kConsumeEverything},
        {ArgCategory::String,        kConsumeEverything},
        {ArgCategory::Pointer,       kConsumeEverything},
    };

    // Two policies that differ ONLY in their consume mask.  Neither has
    // an override row, so both resolve the same ALLOW mask (the
    // standard's) — which is the whole point of the comparison.
    const ImplPolicy narrow_consume{
        "narrow-consume", nullptr, 0, kSharedConsumeTable, 6};
    const ImplPolicy wide_consume{
        "wide-consume", nullptr, 0, kWideConsumeTable, 6};

    // The PUBLIC contract is what a caller sees:
    //
    //   petra::FormatErrorKind — 4 kinds, unchanged since Sep 6.
    //   the diagnostic DETAIL  — the scanner's detail string, which the
    //                            classifier embeds verbatim in the
    //                            error message.
    //
    // The arc ALSO has an internal failure enum (6 values) that the
    // classifier maps onto the public kinds.  Section 5 measures all
    // three, because they do NOT move together: the consume mask moves
    // the internal enum and the detail, and leaves the public kind
    // exactly where it was.
    std::size_t verdict_diff = 0;
    std::size_t kind_diff = 0;
    std::size_t detail_diff = 0;
    std::size_t internal_diff = 0;
    std::size_t total = 0;
    std::size_t rejected = 0;

    // The stdlib's message.  It is irrelevant to the KIND for a
    // scan-failure cell — stage A of the classifier decides the kind
    // structurally — and it is the SAME on both sides, so it cannot
    // bias the comparison.
    const char* kWhat = "the format specifier is invalid";

    const auto compare = [&](ArgCategory cat) {
        // One argument in the list, of the category under test, so the
        // scanner can resolve the field's category.
        const ArgCategory one[] = {cat};
        const CategoryTable table{one, 1};

        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string spec = spec_text(p, t);
                const std::string fmt = "{:" + spec + "}";

                // The arc's OWN scanner, once per consume policy.
                const ScanResult narrow =
                    scan_format_string(fmt, table, narrow_consume);
                const ScanResult wide =
                    scan_format_string(fmt, table, wide_consume);
                ++total;

                // (1) The VERDICT: is the spec still accepted?
                if (narrow.ok() != wide.ok()) { ++verdict_diff; continue; }
                if (narrow.ok()) continue;   // accepted by both: no error
                ++rejected;

                // (2) The PUBLIC kind, read from the arc's OWN
                //     classifier — not from a second copy of the
                //     internal->public mapping.
                const Classification cn = classify_format_failure(
                    fmt, kWhat, table, narrow_consume);
                const Classification cw = classify_format_failure(
                    fmt, kWhat, table, wide_consume);
                if (cn.kind != cw.kind) ++kind_diff;

                // (3) The PUBLIC diagnostic detail.
                if (std::strcmp(narrow.detail, wide.detail) != 0) ++detail_diff;

                // (4) The INTERNAL enum.  Reported, not asserted stable.
                if (narrow.failure != wide.failure) ++internal_diff;
            }
        }
    };

    compare(ArgCategory::Boolean);
    compare(ArgCategory::Character);
    compare(ArgCategory::Integral);
    compare(ArgCategory::Integral);
    compare(ArgCategory::FloatingPoint);
    compare(ArgCategory::String);
    compare(ArgCategory::Pointer);

    std::printf("  cells compared: %zu   (rejected by both: %zu)\n", total, rejected);
    std::printf("  accept/reject VERDICT changed:      %zu\n", verdict_diff);
    std::printf("  PUBLIC FormatErrorKind changed:      %zu\n", kind_diff);
    std::printf("  public diagnostic DETAIL changed:    %zu\n", detail_diff);
    std::printf("  internal failure enum changed:       %zu\n", internal_diff);
    std::printf("  -> the consume mask decides WHICH option the parser\n"
                "     blames, not WHETHER the spec is rejected.  The ALLOW\n"
                "     mask decides the verdict.  So the consume mask can be\n"
                "     retuned per implementation without touching the arc's\n"
                "     contract.\n");
    std::printf("  -> the PUBLIC kind does not move, even where the INTERNAL\n"
                "     enum does: the internal values it moves between both\n"
                "     map onto the same public kind.  A caller cannot see\n"
                "     the difference; only the diagnostic text changes.\n");
    std::printf("  -> Oct 30's phase-1 mask was libc++'s, hard-wired for\n"
                "     BOTH policies.  The table makes it explicit.\n");

    c.expect_eq(total, kGridCells, "cells compared");
    c.check(rejected > 0);
    c.expect_eq(verdict_diff, 0, "verdict changes from the consume mask");
    // The PUBLIC contract is what must not move.
    c.expect_eq(kind_diff, 0, "public FormatErrorKind changes from the consume mask");
    // The internal enum and the diagnostic detail are expected to move.
    c.check(internal_diff > 0);
    c.check(detail_diff > 0);
}

// ---------------------------------------------------------------------------
// Section 6 — the arc's own paths, end to end.
// ---------------------------------------------------------------------------
void section6(counters& c) {
    section("Section 6 — the arc's own paths, end to end");

    using petra::detail::try_format_runtime_n_impl;

    char buf[64];

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

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:.2}", 42);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatArgument);
            std::printf("  {:.2} on an int -> kind=FormatArgument  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:+}", std::string_view{"x"});
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatArgument);
            std::printf("  {:+} on a string -> kind=FormatArgument  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:.}", 42);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatParse);
            std::printf("  {:.} on an int -> kind=FormatParse  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

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

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{}",
                                                 petra_test::point{3, 4});
        c.check(r.has_value());
        if (r) {
            c.check(std::string_view(buf, *r) == "(3,4)");
            std::printf("  a user-defined formatter -> \"%.*s\" (validator abstained)\n",
                        static_cast<int>(*r), buf);
        }
    }
}

// ============================================================================
// Section 7 — THE NUMERIC CEILING.
//
// The standard's grammar ([format.string.std]) is
//
//     width:     positive-integer
//     precision: . nonnegative-integer
//
// and the standard says only "the value of the positive-integer is
// interpreted as a decimal integer and used as the value of the option".
// There is NO upper bound anywhere in the standard, so the ceiling is an
// IMPLEMENTATION's limit, and each implementation has its own:
//
//     libc++ 21      __format/format_string.h:  __number_max = INT32_MAX
//     libstdc++ 15   format:  __parse_integer() returns unsigned short
//
// This section MEASURES the ceiling of the linked stdlib and CHECKS it
// against the hand-written policy row -- the same discipline as Nov 7's
// derived override table.  A number in a comment is not a check; a
// number the program computes is.
// ============================================================================

// A sink that throws after `limit` bytes.
//
// A width of 2^31 would otherwise ask for a two-gigabyte string; a
// precision of 2^31 on a double would ask for two billion significant
// digits.  Both are legitimate format strings, so the only safe way to
// ask "does this spec PARSE?" is to stop the output the moment it
// starts.
struct sink_stop : std::runtime_error {
    sink_stop() : std::runtime_error("sink stop") {}
};

class aborting_sink {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;

    explicit aborting_sink(std::size_t limit) noexcept : limit_(limit) {}

    aborting_sink& operator*() noexcept { return *this; }
    aborting_sink& operator++() noexcept { return *this; }
    aborting_sink& operator++(int) noexcept { return *this; }
    aborting_sink& operator=(char) {
        if (++written_ > limit_) throw sink_stop{};
        return *this;
    }

    [[nodiscard]] std::size_t written() const noexcept { return written_; }

private:
    std::size_t written_{0};
    std::size_t limit_{0};
};

// Does this WIDTH literal parse?  Parsing happens before any output, so
// there are only two outcomes -- accepted or rejected -- and the sink
// only stops the padding.
[[nodiscard]] bool width_literal_parses(std::uint64_t w) {
    const std::string fmt = "{:" + std::to_string(w) + "}";
    const std::string s = "x";
    try {
        aborting_sink sink{1};
        (void)std::vformat_to(sink, fmt, std::make_format_args(s));
        return true;
    } catch (const sink_stop&) {
        return true;   // the spec parsed; we stopped the output
    } catch (const std::format_error&) {
        return false;  // the spec was rejected before any output
    }
}

// The same question for a PRECISION literal.  On a string the precision
// truncates, so the output stays tiny; the sink is still there because
// the question is about parsing, not about output.
[[nodiscard]] bool precision_literal_parses(std::uint64_t p) {
    const std::string fmt = "{:." + std::to_string(p) + "}";
    const std::string s = "abc";
    try {
        aborting_sink sink{1};
        (void)std::vformat_to(sink, fmt, std::make_format_args(s));
        return true;
    } catch (const sink_stop&) {
        return true;
    } catch (const std::format_error&) {
        return false;
    }
}

// The largest literal the linked stdlib accepts.  The predicate is
// monotone: a literal at or below the ceiling parses, one above it does
// not.
[[nodiscard]] std::uint64_t
largest_literal(bool (*parses)(std::uint64_t)) {
    std::uint64_t hi = std::uint64_t{1} << 40;
    if (!parses(1)) return 0;
    if (parses(hi)) return hi;   // no ceiling inside the search range
    std::uint64_t lo = 1;
    while (hi - lo > 1) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (parses(mid)) lo = mid; else hi = mid;
    }
    return lo;
}

// The WIDTH ceiling, measured inside a SAFE range.
//
// A width is a PADDING count.  Accepting a width of 2^31 means writing
// two billion padding characters, and libc++ does that in ONE memset
// before it hands a single byte to the output iterator -- so the
// throwing sink cannot stop it.  The width's acceptance side is
// therefore only measured up to a safety bound; above it, the width is
// checked on its REJECTION side, which costs nothing, because an
// over-ceiling width is rejected while the format string is parsed.
constexpr std::uint64_t kSafeWidthBound = std::uint64_t{1} << 20;  // 1 MiB

// The largest width the linked stdlib accepts, searching only up to
// `bound` so that no probe ever pads more than `bound` characters.
// Returns `bound + 1` when the boundary is above the bound.
[[nodiscard]] std::uint64_t
largest_width_within(std::uint64_t bound) {
    if (!width_literal_parses(1)) return 0;
    std::uint64_t lo = 1;
    std::uint64_t hi = bound + 1;
    if (width_literal_parses(hi)) return hi;   // no boundary in this range
    while (hi - lo > 1) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (width_literal_parses(mid)) lo = mid; else hi = mid;
    }
    return lo;
}

// The argument packs the census needs.  One pack per argument category,
// plus a two-int pack for the nested width argument.
enum class Pack { OneBool, OneChar, OneInt, OneBigInt, OneLong, OneDouble,
                   OneString, OneVoidp, TwoInts };

[[nodiscard]] CategoryTable pack_cats(Pack pack) {
    switch (pack) {
    case Pack::OneBool: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Boolean};
        return {a.data(), a.size()};
    }
    case Pack::OneChar: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Character};
        return {a.data(), a.size()};
    }
    case Pack::OneInt:
    case Pack::OneBigInt:
    case Pack::OneLong: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Integral};
        return {a.data(), a.size()};
    }
    case Pack::OneDouble: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::FloatingPoint};
        return {a.data(), a.size()};
    }
    case Pack::OneString: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::String};
        return {a.data(), a.size()};
    }
    case Pack::OneVoidp: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Pointer};
        return {a.data(), a.size()};
    }
    case Pack::TwoInts: {
        static constexpr std::array<ArgCategory, 2> a{ArgCategory::Integral,
                                                      ArgCategory::Integral};
        return {a.data(), a.size()};
    }
    }
    return {};
}

struct ProbeResult {
    bool accepted{false};
    std::string message;
};

// Run one format string against the LINKED stdlib, with an output sink
// that stops after one byte, so that no case can materialise a huge
// result.
[[nodiscard]] ProbeResult probe_stdlib(const std::string& fmt, Pack pack) {
    try {
        switch (pack) {
        case Pack::OneBool: {
            const bool b = true;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(b));
            break;
        }
        case Pack::OneChar: {
            const char ch = 'x';
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(ch));
            break;
        }
        case Pack::OneInt: {
            const int i = 7;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(i));
            break;
        }
        case Pack::OneBigInt: {
            const int i = 300;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(i));
            break;
        }
        case Pack::OneLong: {
            const long l = 7;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(l));
            break;
        }
        case Pack::OneDouble: {
            const double d = 1.5;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(d));
            break;
        }
        case Pack::OneString: {
            const std::string s = "abc";
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(s));
            break;
        }
        case Pack::OneVoidp: {
            const void* p = nullptr;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(p));
            break;
        }
        case Pack::TwoInts: {
            const int a = 7;
            const int b = 3;
            (void)std::vformat_to(aborting_sink{1}, fmt,
                                  std::make_format_args(a, b));
            break;
        }
        }
        return {true, {}};
    } catch (const sink_stop&) {
        return {true, {}};
    } catch (const std::format_error& e) {
        return {false, e.what()};
    }
}

[[nodiscard]] const char* kind_name(FormatErrorKind k) {
    switch (k) {
    case FormatErrorKind::Format:         return "Format";
    case FormatErrorKind::FormatParse:    return "FormatParse";
    case FormatErrorKind::FormatArgument: return "FormatArgument";
    case FormatErrorKind::TooLarge:       return "TooLarge";
    }
    return "?";
}

[[nodiscard]] const char* scan_name(ScanFailure f) {
    switch (f) {
    case ScanFailure::None:               return "None";
    case ScanFailure::Malformed:          return "Malformed";
    case ScanFailure::TypeNotAllowed:     return "TypeNotAllowed";
    case ScanFailure::OptionNotAllowed:   return "OptionNotAllowed";
    case ScanFailure::IndexOutOfRange:    return "IndexOutOfRange";
    case ScanFailure::WidthArgNotInteger: return "WidthArgNotInteger";
    case ScanFailure::MixedNumbering:     return "MixedNumbering";
    case ScanFailure::NumberTooLarge:     return "NumberTooLarge";
    }
    return "?";
}

[[nodiscard]] const char* verdict_name(MessageVerdict v) {
    switch (v) {
    case MessageVerdict::Parse:     return "Parse";
    case MessageVerdict::Argument:  return "Argument";
    case MessageVerdict::Ambiguous: return "Ambiguous";
    case MessageVerdict::Unknown:   return "Unknown";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Section 7 — the ceiling, measured.
// ---------------------------------------------------------------------------
void section7(counters& c) {
    section("Section 7 — the numeric ceiling: measured, then checked");

    const ImplPolicy& linked = linked_policy();

    // The PRECISION is the safe way to measure the number parser: a
    // precision TRUNCATES, so a literal of 2^31 costs three bytes.
    const std::uint64_t measured_precision = largest_literal(&precision_literal_parses);

    // The WIDTH pads, so it is only measured inside the safety bound.
    const std::uint64_t measured_width = largest_width_within(kSafeWidthBound);

    std::printf("  linked stdlib: %s\n", linked.name);
    std::printf("  the standard's grammar fixes NO ceiling:\n");
    std::printf("    width:     positive-integer\n");
    std::printf("    precision: . nonnegative-integer\n");
    std::printf("  MEASURED on the linked stdlib:\n");
    std::printf("    largest precision literal accepted: %llu  (exact)\n",
                static_cast<unsigned long long>(measured_precision));
    std::printf("    largest width literal accepted:     %llu  (searched up to %llu)\n",
                static_cast<unsigned long long>(measured_width),
                static_cast<unsigned long long>(kSafeWidthBound));
    std::printf("  the policy row says: %s\n", linked.ceiling_note);

    // The policy row is checked against the EXACT measurement.
    if (linked.max_number.has_value()) {
        c.expect_eq(measured_precision, *linked.max_number,
                    "the measured ceiling vs the hand-written policy row");
    } else {
        c.check(std::strcmp(linked.name, "standard") == 0);
    }

    // The WIDTH is subject to the same limit, and that is checked on its
    // rejection side: a width over the ceiling is rejected while the
    // format string is parsed, which costs nothing -- no padding is
    // ever materialised.
    const std::uint64_t over = measured_precision + 1;
    c.check(!width_literal_parses(over));
    c.check(measured_width <= measured_precision);
    std::printf("  a width of %llu is rejected by the linked stdlib: %s\n",
                static_cast<unsigned long long>(over),
                width_literal_parses(over) ? "NO" : "yes");
    std::printf("  -> the width and the precision come out of the SAME number\n"
                "     parser, so they share the ceiling; the source says so,\n"
                "     the measurement pins the number, and the width's\n"
                "     rejection side confirms that the width is bound by it.\n");

    // The ARGUMENT INDEX.  The standard's grammar has no ceiling for it
    // either, and both implementations parse it with the same number
    // parser as the width, so the arc applies the same number to it.
    //
    // What the STDLIB says about an index over the ceiling is worth
    // printing, because the two implementations cannot say the same
    // thing about it: libc++ has a separate message for "too large to
    // be a number" and for "past the end of the argument list", and
    // libstdc++ has ONE message for both.
    const ProbeResult at_probe =
        probe_stdlib("{" + std::to_string(measured_precision) + "}", Pack::OneInt);
    const ProbeResult over_probe =
        probe_stdlib("{" + std::to_string(over) + "}", Pack::OneInt);

    std::printf("\n  the argument index, which uses the same number parser:\n");
    std::printf("    index %llu (at the ceiling):    %s\n",
                static_cast<unsigned long long>(measured_precision),
                at_probe.accepted ? "accepted" : at_probe.message.c_str());
    std::printf("      the arc's message table calls that: %s\n",
                verdict_name(verdict_from_message(at_probe.message.c_str())));
    std::printf("    index %llu (over the ceiling):   %s\n",
                static_cast<unsigned long long>(over),
                over_probe.accepted ? "accepted" : over_probe.message.c_str());
    std::printf("      the arc's message table calls that: %s\n",
                verdict_name(verdict_from_message(over_probe.message.c_str())));

    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        // libc++ distinguishes the two failures by message.
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(at_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Argument),
                    "libc++: an index at the ceiling is an argument failure");
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(over_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Parse),
                    "libc++: an index over the ceiling is a parse failure");
    } else {
        // libstdc++ uses ONE message for both, so the MESSAGE cannot
        // tell them apart -- only the scanner's own rule can.  That is
        // the whole argument for putting the ceiling in the policy.
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(at_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Unknown),
                    "libstdc++: an index at the ceiling is unrecognised");
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(over_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Unknown),
                    "libstdc++: an index over the ceiling is unrecognised too");
    }
}

// ---------------------------------------------------------------------------
// Section 8 — THE CENSUS.
//
// Every case where the arc and the stdlib can disagree, with THREE
// classifications side by side:
//
//   today      the scanner without the ceiling rule, and the fallback
//              that DISCARDS the scanner's verdict (Nov 7's behaviour)
//   +ceiling   the scanner WITH the ceiling rule, same discarding
//              fallback
//   +carried   the ceiling rule AND the fallback that CARRIES the
//              scanner's verdict
//
// The middle column is the point: the ceiling rule and the carried
// verdict are two different repairs, and the lesson has to know which
// one moved which cell.
// ---------------------------------------------------------------------------
void section8(counters& c) {
    section("Section 8 — the census: what the carried verdict actually changes");

    const ImplPolicy& linked = linked_policy();

    // "Today" is the same implementation with the ceiling rule switched
    // OFF -- which is exactly what a policy without a ceiling means.
    const ImplPolicy today{
        linked.name, linked.allow_rows, linked.allow_count,
        linked.consume_rows, linked.consume_count,
        std::nullopt, linked.ceiling_note, linked.fill};

    // The ceiling is measured through the PRECISION: a precision
    // truncates, so the measurement never materialises a gigabyte of
    // padding the way a width would.
    const std::uint64_t ceiling = largest_literal(&precision_literal_parses);
    const std::string at = std::to_string(ceiling);
    const std::string big = std::to_string(ceiling + 1);

    struct Case {
        const char* label;
        std::string fmt;
        Pack pack;
    };

    const std::vector<Case> cases = {
        {"width literal at the safe bound",    "{:1048576}",         Pack::OneString},
        {"width literal over the ceiling",      "{:" + big + "}",     Pack::OneString},
        {"precision literal at the ceiling",    "{:." + at + "}",     Pack::OneString},
        {"precision literal over the ceiling",  "{:." + big + "}",   Pack::OneString},
        {"argument index at the ceiling",       "{" + at + "}",       Pack::OneInt},
        {"argument index over the ceiling",     "{" + big + "}",      Pack::OneInt},
        {"nested index over the ceiling",       "{0:{" + big + "}}",  Pack::TwoInts},
        {"argument index past the end",         "{1}",                Pack::OneInt},
        {"value out of range for 'c'",          "{:c}",               Pack::OneBigInt},
        {"invalid UTF-8 fill",                  "{:\x80<5}",          Pack::OneString},
        {"a multi-byte UTF-8 fill",             "{:\xF0\x9F\xA4\x91^5}", Pack::OneString},
        {"fill is '{'",                          "{:{<5}",             Pack::OneString},
        {"precision on an integer",             "{:.2}",              Pack::OneInt},
    };

    std::size_t rejects = 0;
    std::size_t changed_by_ceiling = 0;
    std::size_t changed_by_carry = 0;
    std::size_t changed_total = 0;
    std::size_t false_positives = 0;

    std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", "case", "stdlib",
                "scanner (today)", "today", "+ceiling", "+carried");

    for (const Case& cs : cases) {
        const ProbeResult pr = probe_stdlib(cs.fmt, cs.pack);
        const CategoryTable cats = pack_cats(cs.pack);
        const ScanResult s_today = scan_format_string(cs.fmt, cats, today);

        if (!pr.accepted) {
            ++rejects;
            const Classification a = classify_format_failure(
                cs.fmt, pr.message.c_str(), cats, today, Fallback::Discard);
            const Classification b = classify_format_failure(
                cs.fmt, pr.message.c_str(), cats, linked, Fallback::Discard);
            const Classification cc = classify_format_failure(
                cs.fmt, pr.message.c_str(), cats, linked, Fallback::Carry);
            if (a.kind != b.kind) ++changed_by_ceiling;
            if (b.kind != cc.kind) ++changed_by_carry;
            if (a.kind != cc.kind) ++changed_total;
            std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", cs.label,
                        "reject", scan_name(s_today.failure), kind_name(a.kind),
                        kind_name(b.kind), kind_name(cc.kind));
        } else {
            // The stdlib accepts it.  Does the arc's scanner?
            const ScanResult s_new = scan_format_string(cs.fmt, cats, linked);
            if (!s_new.ok()) {
                ++false_positives;
                std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", cs.label,
                            "ACCEPT", scan_name(s_new.failure), "FALSE POSITIVE",
                            "FALSE POSITIVE", "FALSE POSITIVE");
            } else {
                std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", cs.label,
                            "accept", "None", "-", "-", "-");
            }
        }
    }

    // The chain item's own question, asked of the 1,596-cell grid first:
    // how many of the rejects would classify differently?
    //
    // The answer is zero, and the reason is the finding: the grid never
    // reaches the fallback.  Every reject in the grid is decided by the
    // scanner, so the grid is structurally blind to this defect.
    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    std::size_t grid_rejects = 0;
    std::size_t grid_reached_fallback = 0;
    std::size_t grid_changed = 0;

    const auto run = [&](ArgCategory cat, auto& value) {
        const ArgCategory one[] = {cat};
        const CategoryTable table{one, 1};
        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string fmt = "{:" + spec_text(p, t) + "}";
                std::string message;
                const Outcome outcome = measure(fmt, value, message);
                if (outcome == Outcome::Ok) continue;
                ++grid_rejects;
                const Classification a = classify_format_failure(
                    fmt, message.c_str(), table, today, Fallback::Discard);
                const Classification cc = classify_format_failure(
                    fmt, message.c_str(), table, linked, Fallback::Carry);
                if (a.used_message_table) ++grid_reached_fallback;
                if (a.kind != cc.kind) ++grid_changed;
            }
        }
    };

    run(ArgCategory::Boolean,       b);
    run(ArgCategory::Character,     ch);
    run(ArgCategory::Integral,      i);
    run(ArgCategory::Integral,      l);
    run(ArgCategory::FloatingPoint, d);
    run(ArgCategory::String,        sv);
    run(ArgCategory::Pointer,       vp);

    std::printf("\n  the 1,596-cell grid: %zu rejects, %zu of them reach the\n"
                "  fallback, %zu change their public kind.\n",
                grid_rejects, grid_reached_fallback, grid_changed);
    std::printf("  -> the grid CANNOT see this defect.  Every one of its\n"
                "     rejects is decided by the scanner, so the fallback\n"
                "     never runs, and a change to the fallback cannot move\n"
                "     a single cell.  Measuring the defect needs the\n"
                "     families the grid omits.\n");

    std::printf("\n  the census: %zu rejects; the ceiling rule moved %zu, the\n"
                "  carried verdict moved %zu; %zu in total.\n",
                rejects, changed_by_ceiling, changed_by_carry, changed_total);
    std::printf("  the arc's scanner rejects what the stdlib accepts in %zu case(s).\n",
                false_positives);
    std::printf("  -> the ceiling rule is what makes \"even when the message is\n"
                "     unrecognised\" true: the scanner decides the magnitude\n"
                "     family with no message at all.  The carried verdict is\n"
                "     what rescues the families the scanner cannot model --\n"
                "     and only where the stdlib's message vocabulary lets it.\n");
    std::printf("  -> and the FILL family no longer needs it at all: the\n"
                "     scanner now reads the fill as a Unicode scalar value,\n"
                "     so the ill-formed fill is a STRUCTURAL failure.  That is\n"
                "     why the carried verdict moves nothing here.\n");

    c.expect_eq(grid_changed, 0, "the grid does not move");
    c.expect_eq(grid_reached_fallback, 0, "the grid never reaches the fallback");
    c.check(rejects > 0);
    c.check(changed_total > 0);
    c.check(changed_by_ceiling > 0);
}

// ---------------------------------------------------------------------------
// Section 9 — THE FILL AS A CHARACTER.
//
// Nov 12's next-step item (1), asked three things: decode the fill as
// a UTF-8 scalar value, validate it, and re-measure the
// false-positive count.  This section does all three, and it measures
// the OLD rule and the NEW rule side by side -- because the point is
// not that the new rule is nicer, it is that the new rule AGREES with
// the stdlib where the old one did not.
// ---------------------------------------------------------------------------
void section9(counters& c) {
    section("Section 9 — the fill as a character");

    const ImplPolicy& linked = linked_policy();

    // ---- 9.1 the decoder, on its own ------------------------------------
    struct DecodeCase {
        const char* label;
        const char* bytes;
        std::size_t in_len;   // how many bytes to feed the decoder
        FillFault fault;
        std::size_t out_len;  // how many bytes it consumes
        char32_t value;
    };
    const DecodeCase decode_cases[] = {
        {"ASCII 'A'",              "A",                 1, FillFault::None, 1, U'A'},
        {"U+00E9 e-acute",         "\xC3\xA9",          2, FillFault::None, 2, 0x00E9u},
        {"U+20AC euro",            "\xE2\x82\xAC",      3, FillFault::None, 3, 0x20ACu},
        {"U+1F911 money-with-wings","\xF0\x9F\xA4\x91",  4, FillFault::None, 4, 0x1F911u},
        {"U+10FFFF, the last one", "\xF4\x8F\xBF\xBF",  4, FillFault::None, 4, 0x10FFFFu},
        {"a lone continuation",    "\x80",              1, FillFault::InvalidLead, 1, 0},
        {"0xFE, never a lead",     "\xFE",              1, FillFault::InvalidLead, 1, 0},
        {"0xC0, overlong-only lead","\xC0",              1, FillFault::Truncated, 1, 0},
        {"0xF5, wants a 4th byte", "\xF5",              1, FillFault::Truncated, 1, 0},
        {"a truncated 3-byte",     "\xE2\x82",          2, FillFault::Truncated, 2, 0},
        {"a truncated 4-byte",     "\xF0\x9F\xA4",      3, FillFault::Truncated, 3, 0},
        {"a byte that cannot continue","\xE2\x28",       2, FillFault::Truncated, 1, 0},
        {"an overlong 2-byte",     "\xC0\xAF",          2, FillFault::Overlong, 1, 0},
        {"an overlong 3-byte",     "\xE0\x80",          2, FillFault::Overlong, 1, 0},
        {"an overlong 4-byte",     "\xF0\x80",          2, FillFault::Overlong, 1, 0},
        {"a surrogate U+D800",     "\xED\xA0",          2, FillFault::Surrogate, 1, 0},
        {"above U+10FFFF",         "\xF4\x90",          2, FillFault::OutOfRange, 1, 0},
    };

    std::printf("  the decoder -- Unicode 15 Table 3-7, read backwards:\n");
    std::printf("    %-27s %-6s %-16s %s\n", "bytes", "len", "fault", "value");
    for (const DecodeCase& dc : decode_cases) {
        const Utf8CodePoint cp =
            decode_utf8(std::string_view{dc.bytes, dc.in_len});
        c.expect_eq(static_cast<std::size_t>(cp.fault),
                    static_cast<std::size_t>(dc.fault),
                    "the decoder's fault");
        c.expect_eq(cp.length, dc.out_len, "the decoder's length");
        if (dc.fault == FillFault::None) {
            c.expect_eq(static_cast<std::size_t>(cp.value),
                        static_cast<std::size_t>(dc.value),
                        "the decoder's value");
        }
        std::printf("    %-27s %-6zu %-16s U+%04lX\n", dc.label, cp.length,
                    fault_name(cp.fault),
                    static_cast<unsigned long>(cp.value));
    }

    // ---- 9.2 the fill census: the OLD rule vs the NEW rule --------------
    //
    // Every case where the fill can be ill-formed or multi-byte, with
    // THREE verdicts side by side: the linked stdlib, the arc's OLD
    // byte-oriented scanner, and the arc's NEW scalar-value scanner.
    struct FillCase {
        const char* label;
        const char* fmt;
    };
    const FillCase fill_cases[] = {
        {"an ASCII fill",            "{:*<5}"},
        {"a 2-byte fill (U+00E9)",   "{:\xC3\xA9<5}"},
        {"a 3-byte fill (U+20AC)",   "{:\xE2\x82\xAC<5}"},
        {"a 4-byte fill (U+1F911)",  "{:\xF0\x9F\xA4\x91^5}"},
        {"U+10FFFF as a fill",       "{:\xF4\x8F\xBF\xBF<5}"},
        {"a lone continuation byte", "{:\x80<5}"},
        {"0xFE as a fill",           "{:\xFE<5}"},
        {"0xC0 as a fill",           "{:\xC0<5}"},
        {"0xF5 as a fill",           "{:\xF5<5}"},
        {"a truncated 3-byte fill",  "{:\xE2\x82<5}"},
        {"a truncated 4-byte fill",  "{:\xF0\x9F\xA4<5}"},
        {"a bad continuation byte",  "{:\xE2\x28\xA1<5}"},
        {"an overlong 2-byte fill",  "{:\xC0\xAF<5}"},
        {"an overlong 3-byte fill",  "{:\xE0\x80\x80<5}"},
        {"an overlong 4-byte fill",  "{:\xF0\x80\x80\x80<5}"},
        {"a surrogate fill",         "{:\xED\xA0\x80<5}"},
        {"a value above U+10FFFF",   "{:\xF4\x90\x80\x80<5}"},
    };

    // The OLD rule: the same scanner with the fill read as one byte.
    ImplPolicy byte_rule = linked;
    byte_rule.fill.unicode = false;

    const CategoryTable cats = pack_cats(Pack::OneString);

    std::size_t old_false_positives = 0;
    std::size_t old_false_negatives = 0;
    std::size_t new_false_positives = 0;
    std::size_t new_false_negatives = 0;

    std::printf("\n  the fill census -- the stdlib, the OLD byte rule, the NEW"
                " scalar rule:\n");
    std::printf("    %-27s %-8s %-14s %-14s\n", "case", "stdlib",
                "byte rule", "scalar rule");

    for (const FillCase& fc : fill_cases) {
        const ProbeResult pr = probe_stdlib(fc.fmt, Pack::OneString);
        const ScanResult s_old = scan_format_string(fc.fmt, cats, byte_rule);
        const ScanResult s_new = scan_format_string(fc.fmt, cats, linked);

        const bool stdlib_ok = pr.accepted;
        const bool old_ok = s_old.ok();
        const bool new_ok = s_new.ok();

        if (!old_ok && stdlib_ok) ++old_false_positives;
        if (old_ok && !stdlib_ok) ++old_false_negatives;
        if (!new_ok && stdlib_ok) ++new_false_positives;
        if (new_ok && !stdlib_ok) ++new_false_negatives;

        const auto verdict = [](bool ok) -> const char* {
            return ok ? "accept" : "reject";
        };
        std::printf("    %-27s %-8s %-14s %-14s\n", fc.label,
                    verdict(stdlib_ok), verdict(old_ok), verdict(new_ok));
    }

    std::printf("\n  the byte-oriented rule disagrees with the linked stdlib"
                " %zu time(s):\n"
                "    %zu false positive(s) (the arc rejects, the stdlib accepts)\n"
                "    %zu false negative(s) (the arc accepts, the stdlib rejects)\n",
                old_false_positives + old_false_negatives, old_false_positives,
                old_false_negatives);
    std::printf("  the scalar-value rule disagrees with the linked stdlib"
                " %zu time(s):\n"
                "    %zu false positive(s), %zu false negative(s)\n",
                new_false_positives + new_false_negatives, new_false_positives,
                new_false_negatives);
    std::printf("  -> the fill is a scalar value, so the scanner DECODES it.\n"
                "     The old rule guessed a byte and asked whether the NEXT\n"
                "     BYTE was an alignment option; a four-byte fill has no\n"
                "     such byte, so the arc called a valid spec malformed.\n");

    c.expect_eq(new_false_positives, 0, "the scalar rule has no false positives");
    c.expect_eq(new_false_negatives, 0, "the scalar rule has no false negatives");
    c.check(old_false_positives + old_false_negatives > 0);

    // ---- 9.3 the ill-formed fill is a STRUCTURAL rule now ---------------
    //
    // Under libc++ this used to be rescued by the message table: the
    // stdlib said "The format specifier contains malformed Unicode
    // characters", that message was in the arc's libc++-shaped table,
    // and the carried verdict carried the parse verdict through.  Now
    // the scanner rejects the fill on its own, so the message table is
    // not consulted at all.
    {
        const std::string fmt = "{:\x80<5}";
        const ProbeResult pr = probe_stdlib(fmt, Pack::OneString);
        std::printf("\n  an ill-formed fill, decided by the scanner:\n");
        if (pr.accepted) {
            std::printf("    the linked stdlib ACCEPTS it (it substitutes"
                        " U+FFFD)\n");
            const ScanResult s = scan_format_string(fmt, cats, linked);
            c.check(s.ok());
        } else {
            std::printf("    the linked stdlib rejects it: %s\n",
                        pr.message.c_str());
            const Classification cl = classify_format_failure(
                fmt, pr.message.c_str(), cats, linked, Fallback::Carry);
            c.check(!cl.used_message_table);
            c.check(cl.scanner_verdict == ScanFailure::Malformed);
            c.expect_eq(static_cast<std::size_t>(cl.kind),
                        static_cast<std::size_t>(FormatErrorKind::FormatParse),
                        "an ill-formed fill is a FormatParse");
            std::printf("    the scanner says: %s\n", cl.reason);
            std::printf("    the message table was consulted: %s\n",
                        cl.used_message_table ? "yes" : "NO");
        }
    }

    // ---- 9.4 the width counts CODE POINTS, not bytes --------------------
    //
    // A width of 10 with a four-byte fill produces 31 bytes: ten code
    // points, seven of which are the fill.  The arc's cap is in BYTES
    // -- it is a buffer cap, measured from the stdlib's own output --
    // so the arc is right, and the two units must not be confused.
    {
        const auto format_bytes = [](std::string_view fmt, std::string& arg) {
            std::string out;
            std::vformat_to(std::back_inserter(out), fmt,
                             std::make_format_args(arg));
            return out;
        };

        std::string three_euros = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
        const std::string out5 = format_bytes("{:*^5}", three_euros);
        std::printf("\n  the width counts code points, not bytes:\n");
        std::printf("    \"{:*^5}\" with 3 euros -> %zu bytes, %zu code points\n",
                    out5.size(), count_code_points(out5));
        c.expect_eq(count_code_points(out5), 5,
                    "a width of 5 is five code points");
        c.expect_eq(out5.size(), 11, "three 3-byte euros plus two fill bytes");

        std::string abc = "abc";
        const std::string out10 = format_bytes("{:\xF0\x9F\xA4\x91^10}", abc);
        std::printf("    \"{:\xF0\x9F\xA4\x91^10}\" with \"abc\" -> %zu bytes,"
                    " %zu code points\n",
                    out10.size(), count_code_points(out10));
        c.expect_eq(count_code_points(out10), 10,
                    "a width of 10 is ten code points");
        c.expect_eq(out10.size(), 31, "seven four-byte fills plus three bytes");
    }
}

// ---------------------------------------------------------------------------
// Section 10 — the arc's own paths, extended.
// ---------------------------------------------------------------------------
void section10(counters& c) {
    section("Section 10 — the arc's own paths, extended");

    using petra::detail::try_format_runtime_n_impl;

    const bool libcxx = std::strcmp(linked_policy().name, "libc++ 21") == 0;
    // The ceiling is measured through the PRECISION: a precision
    // truncates, so the measurement never materialises a gigabyte of
    // padding the way a width would.
    const std::uint64_t ceiling = largest_literal(&precision_literal_parses);
    const std::string over = std::to_string(ceiling + 1);

    char buf[64];

    // A width over the ceiling: a malformed FORMAT STRING, in both
    // builds, whichever message the stdlib happens to use.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf),
                                                 "{:" + over + "}", 42);
        c.check(!r.has_value());
        if (!r) {
            std::printf("  a width over the ceiling      -> %s\n",
                        kind_name(r.error().kind));
            c.expect_eq(static_cast<std::size_t>(r.error().kind),
                        static_cast<std::size_t>(FormatErrorKind::FormatParse),
                        "a width over the ceiling is a FormatParse");
        }
    }

    // An argument index over the ceiling: also a malformed format
    // string, and this one used to be an ARGUMENT failure.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf),
                                                 "{" + over + "}", 42);
        c.check(!r.has_value());
        if (!r) {
            std::printf("  an index over the ceiling      -> %s\n",
                        kind_name(r.error().kind));
            c.expect_eq(static_cast<std::size_t>(r.error().kind),
                        static_cast<std::size_t>(FormatErrorKind::FormatParse),
                        "an index over the ceiling is a FormatParse");
        }
    }

    // A value that does not fit the presentation type.  This one is the
    // arc's known gap: the scanner cannot see VALUES, and libstdc++'s
    // message is not in the arc's table.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:c}", 300);
        c.check(!r.has_value());
        if (!r) {
            std::printf("  a value out of range for 'c'   -> %s\n",
                        kind_name(r.error().kind));
            if (libcxx) {
                c.expect_eq(static_cast<std::size_t>(r.error().kind),
                            static_cast<std::size_t>(FormatErrorKind::FormatArgument),
                            "libc++: a value out of range is a FormatArgument");
            } else {
                c.expect_eq(static_cast<std::size_t>(r.error().kind),
                            static_cast<std::size_t>(FormatErrorKind::Format),
                            "libstdc++: a value out of range is still generic");
            }
        }
    }

    // An invalid UTF-8 fill.  libc++ rejects it (the fill must be a
    // single Unicode scalar value) and the carried verdict keeps it a
    // parse failure; libstdc++ accepts the byte.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf),
                                                 "{:\x80<5}", std::string{"abc"});
        if (libcxx) {
            c.check(!r.has_value());
            if (!r) {
                std::printf("  an invalid UTF-8 fill        -> %s\n",
                            kind_name(r.error().kind));
                c.expect_eq(static_cast<std::size_t>(r.error().kind),
                            static_cast<std::size_t>(FormatErrorKind::FormatParse),
                            "libc++: an invalid UTF-8 fill is a FormatParse");
            }
        } else {
            c.check(r.has_value());
            std::printf("  an invalid UTF-8 fill        -> accepted by libstdc++ 15\n");
        }
    }
}

}  // namespace

int main() {
    counters c;

    std::printf("P-2026-11-13 — try_format_unicode_fill\n");
    std::printf("The fill is a Unicode scalar value, so the scanner DECODES it --\n"
                "and which ill-formed fills are fatal is a per-implementation\n"
                "mask in the policy table.\n");
#if defined(_LIBCPP_VERSION)
    std::printf("built against libc++ %d\n", _LIBCPP_VERSION);
#elif defined(_GLIBCXX_RELEASE)
    std::printf("built against libstdc++ %d\n", _GLIBCXX_RELEASE);
#else
    std::printf("built against an unknown standard library\n");
#endif

    section1(c);
    section2(c);
    section3(c);
    section4(c);
    section5(c);
    section6(c);
    section7(c);
    section8(c);
    section9(c);
    section10(c);

    std::printf("\n=== summary ===\n");
    std::printf("  pass=%zu  fail=%zu\n", c.pass, c.fail);
    return c.fail == 0 ? 0 : 1;
}
