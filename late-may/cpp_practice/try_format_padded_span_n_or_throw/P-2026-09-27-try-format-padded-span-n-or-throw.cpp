// P-2026-09-27 — try_format_padded_span_n_or_throw:
//
// The STD::SPAN<CHAR> mirror of Sep 26's char*+cap lesson.  A 2-AXIS
// (× 2-SIBLING) META-MACRO layer that lifts BOTH the
// RETURN-TYPE-vs-EXCEPTION sibling choice AND the PAD-vs-UNTOUCHED
// sibling choice to a meta-macro form on the STD::SPAN<CHAR> ×
// BOUNDED-BUFFER axis.  Two new meta-macros —
// TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW (CONSTEVAL surface) and
// TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW (RUNTIME surface) —
// each dispatch to ONE OF FOUR sibling wrappers at preprocessor
// time based on TWO caller-controlled compile-time flags
// (PETRA_THROW_ON_OVERFLOW × PETRA_PAD):
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 0
//     → expected + untouched sibling (returns expected<size_t,
//       FormatError>; bytes [written, span.size()) UNTOUCHED on
//       success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//        RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 1
//     → expected + NUL-padded sibling (returns expected<size_t,
//       FormatError>; bytes [written, span.size()) zero-filled to
//       '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_padded_span
//                  (NEW today; wraps Sep 10 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_padded_span
//                  (NEW today; wraps Sep 19 verbatim)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 0
//     → throwing + untouched sibling (returns std::size_t on
//       success, throws std::runtime_error on overflow; bytes
//       [written, span.size()) UNTOUCHED on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                  (Sep 21; wraps Sep 10 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                  (Sep 21; wraps Sep 19 verbatim)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 1
//     → throwing + NUL-padded sibling (returns std::size_t on
//       success, throws std::runtime_error on overflow; bytes
//       [written, span.size()) zero-filled to '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_padded_span
//                  (NEW today; wraps Sep 21 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_throwing_padded_span
//                  (NEW today; wraps Sep 21 verbatim)
//
// Today ALSO adds the 4 NEW padded wrappers (the padded siblings of
// Sep 10's / Sep 19's / Sep 21's bounded-buffer span<char>
// wrappers).  Each is a verbatim thin wrapper composed on top of an
// existing span<char> wrapper.  The expected+padded wrappers
// (PETRA_PAD=1, PETRA_THROW=0) compose on top of Sep 10's / Sep 19's
// expected untouched wrappers; the throwing+padded wrappers
// (PETRA_PAD=1, PETRA_THROW=1) compose on top of Sep 21's throwing
// untouched wrappers.  All four zero-fill out[written,
// span.size()) to '\0' on success AFTER the underlying call
// completes — exactly mirroring Sep 25's / Sep 26's char*+cap shape.
//
// Where this fits in the arc
// --------------------------
//   - Aug 25   std::format_to_n_into — NUL-terminating bounded
//              formatter WITHOUT a cap parameter.
//   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
//              std::size_t cap (RUNTIME std::string-output wrapper).
//   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
//   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
//              Sep 7 (char* + std::size_t cap + std::string_view fmt,
//              NO NUL).
//   - Sep  9   try_format_bounded_runtime_n — CONSTEVAL char*+cap
//              (NO NUL).
//   - Sep 10   try_format_bounded_runtime_n_span — CONSTEVAL
//              std::span<char> char*+cap (NO NUL).
//   - Sep 11   format_to_n_into_bounded — CONSTEVAL NUL-terminating
//              char*+cap (plus std::span<char> overload).
//   - Sep 12   try_format_runtime_n_into_bounded — RUNTIME
//              NUL-terminating char*+cap (plus std::span<char>
//              overload).
//   - Sep 13   try_format_bounded_macros — eight TRY_FORMAT_BOUNDED_*
//              macros (CONSTEVAL vs RUNTIME sibling choice at the
//              call site).
//   - Sep 14   try_format_span_format_string — std::span<const char>
//              overloads for the runtime try_format_* family + four
//              new macros.
//   - Sep 15   try_format_consteval_span_format_string — pins that
//              the CONSTEVAL surface is incompatible with
//              std::span<const char>; adds a CONSTEVAL-SHAPED
//              RUNTIME PARSER family of four wrappers + four macros.
//   - Sep 16   try_format_bounded_n_or_into — the BOUNDED-BUFFER vs
//              NUL-TERMINATING sibling-choice macro layer (char*+cap
//              axis).
//   - Sep 19   try_format_span_n_or_into — the BOUNDED-BUFFER vs
//              NUL-TERMINATING sibling-choice macro layer on the
//              std::span<char> axis.
//   - Sep 20   try_format_bounded_n_or_throw — the RETURN-TYPE-vs-
//              EXCEPTION sibling-choice macro layer on the char*+cap
//              axis.
//   - Sep 21   try_format_span_n_or_throw — the RETURN-TYPE-vs-
//              EXCEPTION sibling-choice macro layer on the
//              std::span<char> axis.
//   - Sep 22   try_format_bounded_n_or_throw_into — the NUL-
//              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
//              meta-macro layer on the CHAR*+CAP axis.
//   - Sep 23   try_format_span_n_or_throw_into — the NUL-
//              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
//              meta-macro layer on the STD::SPAN<CHAR> axis.
//   - Sep 25   try_format_padded_n_or_into — the PAD-VS-UNTOUCHED
//              sibling-choice macro layer on the CHAR*+CAP ×
//              BOUNDED-BUFFER axis.
//   - Sep 26   try_format_padded_n_or_throw — the PAD-VS-UNTOUCHED
//              × EXPECTED-VS-THROWING COMBINED meta-macro layer on
//              the CHAR*+CAP × BOUNDED-BUFFER axis (closes Sep 25's
//              "Where we go next" item #1).
//   - TODAY    try_format_padded_span_n_or_throw — the STD::SPAN<CHAR>
//              MIRROR of Sep 26's char*+cap lesson; closes Sep 26's
//              "Where we go next" item #1 verbatim (the span<char>
//              mirror of Sep 26's 64-cell matrix on the char*+cap
//              side).  Today adds 4 NEW padded wrappers + 2 NEW
//              meta-macros; the span<char> × bounded-buffer ×
//              expected-or-throwing × untouched-or-padded matrix is
//              now 16 cells (4 dispatch cells × 4 sibling wrappers).
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
//     Sep 23 / Sep 25 / Sep 26 verbatim (the wrappers below do not
//     produce FormatError directly — they only forward to existing
//     wrappers, but the type must be visible in scope for
//     std::expected<size_t, FormatError> to be instantiatable by the
//     static_assert below).
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — REUSED verbatim.
//   - petra::counting_output_iterator — REUSED verbatim.
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
//     petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim)
//     — REUSED verbatim.
//   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
//     args...) — REUSED Sep 8 verbatim (RUNTIME char*+cap untouched
//     wrapper).
//   - petra::try_format_bounded_runtime_n(char*, std::size_t,
//     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim
//     (CONSTEVAL char*+cap untouched wrapper).
//   - petra::try_format_runtime_n(std::span<char>, std::string_view,
//     args...) — REUSED Sep 19 verbatim (RUNTIME span<char>
//     untouched wrapper).
//   - petra::try_format_bounded_runtime_n_span(std::span<char>,
//     std::format_string<Args...>, args...) — REUSED Sep 10 verbatim
//     (CONSTEVAL span<char> untouched wrapper).
//   - petra::try_format_runtime_n_throwing(std::span<char>,
//     std::string_view, args...) — REUSED Sep 21 verbatim (RUNTIME
//     span<char> throwing untouched wrapper).
//   - petra::try_format_bounded_runtime_n_throwing_span(std::span<char>,
//     std::format_string<Args...>, args...) — REUSED Sep 21 verbatim
//     (CONSTEVAL span<char> throwing untouched wrapper).
//   - petra::try_format_bounded_runtime_n_padded_span(std::span<char>,
//     std::format_string<Args...>, args...) — NEW today (CONSTEVAL
//     span<char> expected+padded wrapper; wraps Sep 10 verbatim and
//     zero-fills [written, span.size()) to '\0' on success).
//   - petra::try_format_runtime_n_padded_span(std::span<char>,
//     std::string_view, args...) — NEW today (RUNTIME span<char>
//     expected+padded wrapper; wraps Sep 19 verbatim and zero-fills
//     [written, span.size()) to '\0' on success).
//   - petra::try_format_bounded_runtime_n_throwing_padded_span(
//     std::span<char>, std::format_string<Args...>, args...) — NEW
//     today (CONSTEVAL span<char> throwing+padded wrapper; wraps
//     Sep 21 verbatim and zero-fills [written, span.size()) to '\0'
//     on success only via try/catch around the throwing call).
//   - petra::try_format_runtime_n_throwing_padded_span(std::span<char>,
//     std::string_view, args...) — NEW today (RUNTIME span<char>
//     throwing+padded wrapper; wraps Sep 21 verbatim and zero-fills
//     [written, span.size()) to '\0' on success only via try/catch
//     around the throwing call).
//   - The TWO new meta-macros TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW
//     (CONSTEVAL surface) and TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW
//     (RUNTIME surface).  Each is a verbatim thin dispatcher that
//     picks between FOUR sibling wrappers at preprocessor time based
//     on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) 2-bit pair.  Each
//     macro is a parenthesized expression (NOT do { } while (0)) so
//     the caller can write 'auto n =
//     TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)' and capture the
//     result.  Each macro uses __VA_OPT__(,) for the no-args case
//     (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 /
//     Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26's macro shape).
//
// What today's lesson pins
// ------------------------
//   1. The (THROWING-vs-EXPECTED) × (PADDED-vs-UNTOUCHED) sibling
//      choice CAN be lifted to a meta-macro form as a 2-axis
//      product on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis — the
//      exact mirror of Sep 26's char*+cap lesson.  The shape
//      composes on top of Sep 10's / Sep 19's bounded-buffer
//      underlying wrappers via TWO levels of dispatch:
//      level 1: Sep 25's padded wrappers (char*+cap) and TODAY's
//               NEW padded wrappers (span<char>) — each sibling
//               dispatches on ONE axis (PAD or THROW).
//      level 2: TODAY's meta-macros dispatch on BOTH axes via a
//               nested #if pair (PETRA_THROW first, then PETRA_PAD
//               inside each branch).
//   2. The compile-time flag pair (PETRA_THROW_ON_OVERFLOW,
//      PETRA_PAD) controls the dispatch via #if PETRA_THROW_ON_OVERFLOW
//      × #if PETRA_PAD:
//        - (0,0): expected+untouched (Sep 10 / Sep 19 span<char>
//          wrappers).
//        - (0,1): expected+padded (NEW today; span<char> wrappers).
//        - (1,0): throwing+untouched (Sep 21 span<char> wrappers).
//        - (1,1): throwing+padded (NEW today; span<char> wrappers).
//   3. Independent #error guards fire at preprocessor time if
//      EITHER flag is set to anything other than 0 or 1.
//   4. The throwing+padded sibling preserves the SAME buffer
//      atomicity on TooLarge as the throwing+untouched sibling —
//      the buffer is UNTOUCHED on overflow.
//   5. The throwing+padded sibling preserves the SAME buffer
//      atomicity on Format errors — a malformed fmt throws
//      std::runtime_error and the buffer is UNTOUCHED.
//   6. The padded sibling preserves the SAME byte-count contract on
//      success — returns the byte count written (== needed ==
//      written, the same value the underlying impl returns).
//   7. The padded sibling preserves the SAME return-type contract —
//      both surfaces return std::expected<std::size_t, FormatError>
//      regardless of PETRA_PAD on the expected path; both return
//      std::size_t regardless of PETRA_PAD on the throwing path.
//   8. The CONSTEVAL macro's consteval gate is preserved regardless
//      of (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) —
//      TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW still requires
//      std::format_string<Args...> (a literal fmt at the call site).
//   9. The std::span<char> sub-span 'no-bleed' contract is preserved
//      on the padded path: a std::span<char> over a sub-range of a
//      buffer must NOT touch bytes outside the span.
//   10. The std::span<char> properties are preserved: contiguous +
//       sized range + trivially copyable + standard layout +
//       .data()->char* + .size()->std::size_t.
//
// What today's lesson does NOT cover
// ----------------------------------
//   - The STD::SPAN<CHAR> × NUL-TERMINATING × (any) cells — already
//     covered by Sep 11 / Sep 12 / Sep 19 / Sep 23 (the
//     NUL-terminating span<char> axis).  Mirrors Sep 26's open
//     item #2.
//   - The STD::SPAN<CHAR> × ALTERNATE-PAD cells — would introduce a
//     SIXTH axis (e.g. PETRA_SPACE_PAD axis that pads with ' '
//     instead of '\0').  Mirrors Sep 26's open item #3.
//   - The STD::SPAN<CONST CHAR> axis — out of scope; Sep 14's /
//     Sep 15's lessons already establish that the CONSTEVAL surface
//     is incompatible with std::span<const char>.

// ============================================================================
// Standard library includes
// ============================================================================

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <format>
#include <iterator>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26)
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
// Part 2 — petra::counting_output_iterator (verbatim from Sep 6 / Sep 7 /
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
// Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26)
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

    std::size_t count() const noexcept { return count_; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

static_assert(std::indirectly_writable<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::output_iterator for const char&");

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (verbatim from Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26)
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
                if (it_->cb_) {
                    it_->cb_(it_->written_, it_->data_);
                }
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

static_assert(std::indirectly_writable<bounded_char_writer, const char&>,
              "petra::detail::bounded_char_writer must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<bounded_char_writer, const char&>,
              "petra::detail::bounded_char_writer must satisfy "
              "std::output_iterator for const char&");

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
// petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim).
//
// The bounded-buffer underlying impls.  TODAY's NEW throwing+padded
// span<char> wrappers compose on top of Sep 21's NEW throwing
// span<char> wrappers, which compose on top of Sep 19's / Sep 10's
// span<char> wrappers, which compose on top of Sep 8's / Sep 9's
// char*+cap wrappers, which compose on top of these impls.
//
// Note: this impl differs from Sep 11's try_format_to_n_into_bounded_impl —
// Sep 11 reserves one slot for the NUL on success, while today's
// impl treats cap as the FULL writable byte count (NO NUL).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                           std::string_view fmt,
                           const Args&... args) {
    // Pass 1: measure the would-be byte count.
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // needed > cap: TooLarge.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    // Pass 2: write into out[0, cap).
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
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
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    // Same shape as the RUNTIME impl above but takes std::format_string.
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
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
        (void)std::vformat_to(std::move(sink), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
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
// Part 5 — petra::try_format_runtime_n (Sep 8 char*+cap verbatim) +
// petra::try_format_bounded_runtime_n (Sep 9 char*+cap CONSTEVAL verbatim) +
// petra::try_format_runtime_n(span<char>) (Sep 19 verbatim) +
// petra::try_format_bounded_runtime_n_span (Sep 10 verbatim).
//
// These are the expected+untouched siblings on the CHAR*+CAP axis AND
// the STD::SPAN<CHAR> axis.  TODAY's NEW padded wrappers compose on
// top of THESE wrappers.
// ============================================================================

namespace petra {

// Sep 8 verbatim — char*+cap RUNTIME bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 9 verbatim — char*+cap CONSTEVAL bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 19 verbatim — span<char> RUNTIME bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(std::span<char> out, std::string_view fmt,
                     const Args&... args) {
    return try_format_runtime_n(out.data(), out.size(), fmt, args...);
}

// Sep 10 verbatim — span<char> CONSTEVAL bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_span(std::span<char> out,
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    return try_format_bounded_runtime_n(out.data(), out.size(), fmt,
                                         args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_runtime_n_throwing(span<char>) (Sep 21
// verbatim) + petra::try_format_bounded_runtime_n_throwing_span (Sep 21
// verbatim).
//
// These are the throwing+untouched siblings on the STD::SPAN<CHAR>
// axis.  TODAY's NEW throwing+padded span<char> wrappers compose on
// top of THESE wrappers.
// ============================================================================

namespace petra {

// Sep 21 verbatim — span<char> RUNTIME throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(std::span<char> out, std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

// Sep 21 verbatim — span<char> CONSTEVAL throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

}  // namespace petra

// ============================================================================
// Part 7 — petra::try_format_bounded_runtime_n_padded_span /
// petra::try_format_runtime_n_padded_span /
// petra::try_format_bounded_runtime_n_throwing_padded_span /
// petra::try_format_runtime_n_throwing_padded_span — NEW TODAY.
//
// These are the FOUR padded siblings on the STD::SPAN<CHAR> axis:
//   - (0, 1) expected+padded sibling (RUNTIME surface):
//     petra::try_format_runtime_n_padded_span
//     (wraps Sep 19's try_format_runtime_n(span, ...) verbatim and
//      zero-fills [written, span.size()) to '\0' on success).
//   - (0, 1) expected+padded sibling (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_padded_span
//     (wraps Sep 10's try_format_bounded_runtime_n_span verbatim
//      and zero-fills [written, span.size()) to '\0' on success).
//   - (1, 1) throwing+padded sibling (RUNTIME surface):
//     petra::try_format_runtime_n_throwing_padded_span
//     (wraps Sep 21's try_format_runtime_n_throwing(span, ...)
//      verbatim and zero-fills [written, span.size()) to '\0' on
//      success ONLY via try/catch around the throwing call).
//   - (1, 1) throwing+padded sibling (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_throwing_padded_span
//     (wraps Sep 21's try_format_bounded_runtime_n_throwing_span
//      verbatim and zero-fills [written, span.size()) to '\0' on
//      success ONLY via try/catch around the throwing call).
//
// Each expected+padded sibling is a verbatim thin wrapper that, on
// success, zero-fills out[written, span.size()) to '\0' AFTER the
// underlying call returns.  On error (the underlying call returns
// FormatError{...}), the wrapper does NOT execute the zero-fill loop
// — control returns the FormatError unchanged.  This is because the
// underlying impl already preserves buffer atomicity on overflow
// (delegated to Sep 8's / Sep 9's underlying impl, which short-
// circuits before pass-2 writes anything on TooLarge).
//
// Each throwing+padded sibling is a verbatim thin wrapper that, on
// success (the underlying call returns std::size_t normally), zero-
// fills out[written, span.size()) to '\0'.  On error (the underlying
// call throws std::runtime_error), the wrapper does NOT execute the
// zero-fill loop — control unwinds through the wrapper WITHOUT
// touching the buffer.
//
// IMPORTANT: the throwing wrapper cannot directly check the return
// value of a function that throws, so the structure MUST be:
//
//   try {
//       auto n = try_format_runtime_n_throwing(out, fmt, args...);
//       // Success: zero-fill out[n, span.size()) to '\0'.
//       for (std::size_t i = n; i < out.size(); ++i) out[i] = '\0';
//       return n;
//   } catch (...) {
//       throw;  // re-raise as-is
//   }
//
// This pattern (try/catch around a throwing call + pad-on-success)
// is the C++ idiom for "zero-fill on success only when the
// underlying throwing call returned normally".  Note that Sep 25's
// expected-sibling pattern (`if (r.has_value()) ...`) does NOT work
// here because the throwing sibling returns std::size_t (NOT
// std::expected), and on TooLarge the throwing sibling throws
// std::runtime_error (does NOT return normally at all).
// ============================================================================

namespace petra {

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// PADDED sibling on the span<char> axis.  Wraps Sep 19's
// try_format_runtime_n(span, ...) verbatim and zero-fills [written,
// span.size()) to '\0' on success ONLY (the 'if (r.has_value())'
// guard short-circuits BEFORE the pad loop, so atomicity is
// preserved on TooLarge / Format error — pinned in Section 3).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span(std::span<char> out, std::string_view fmt,
                                 const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        // Note: cap == out.size() — the span<char> axis uses the
        // span's size() as the writable byte count.
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// PADDED sibling on the span<char> axis.  Wraps Sep 10's
// try_format_bounded_runtime_n_span verbatim and zero-fills
// [written, span.size()) to '\0' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_span(std::span<char> out,
                                         std::format_string<Args...> fmt,
                                         const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// PADDED sibling on the span<char> axis.  Wraps Sep 21's
// try_format_runtime_n_throwing(span, ...) verbatim and zero-fills
// [written, span.size()) to '\0' on success ONLY (control does NOT
// reach the pad loop on error — the catch block re-raises the
// std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_span(std::span<char> out,
                                          std::string_view fmt,
                                          const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        // Success path: zero-fill out[n, out.size()) to '\0'.
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '\0';
        }
        return n;
    } catch (...) {
        // Atomicity preserved: the underlying throwing wrapper did
        // not touch the buffer (delegated to Sep 19's / Sep 10's
        // expected impl, which short-circuits on TooLarge; and on
        // Format errors, pass-1's std::vformat_to throws
        // std::format_error before any writes).  Re-raise the
        // exception unchanged.
        throw;
    }
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// PADDED sibling on the span<char> axis.  Wraps Sep 21's
// try_format_bounded_runtime_n_throwing_span verbatim and zero-
// fills [written, span.size()) to '\0' on success ONLY.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_span(out, fmt, args...);
        // Success path: zero-fill out[n, out.size()) to '\0'.
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '\0';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 8 — TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW
// NEW TODAY — two 2-axis meta-macros that lift BOTH the
// RETURN-TYPE-vs-EXCEPTION sibling choice AND the PAD-vs-UNTOUCHED
// sibling choice to a meta-macro form on the STD::SPAN<CHAR> ×
// BOUNDED-BUFFER axis.
//
// The compile-time flag pair (PETRA_THROW_ON_OVERFLOW, PETRA_PAD)
// controls the dispatch via #if PETRA_THROW_ON_OVERFLOW ×
// #if PETRA_PAD:
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 0
//     → expected+untouched sibling (returns expected<size_t,
//       FormatError>; bytes [written, span.size()) UNTOUCHED on
//       success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//        RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 1
//     → expected+NUL-padded sibling (returns expected<size_t,
//       FormatError>; bytes [written, span.size()) zero-filled to
//       '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_padded_span
//                  (NEW today; wraps Sep 10 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_padded_span
//                  (NEW today; wraps Sep 19 verbatim)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 0
//     → throwing+untouched sibling (returns std::size_t on success,
//       throws std::runtime_error on overflow; bytes [written,
//       span.size()) UNTOUCHED on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                  (Sep 21; wraps Sep 10 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                  (Sep 21; wraps Sep 19 verbatim)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 1
//     → throwing+NUL-padded sibling (returns std::size_t on
//       success, throws std::runtime_error on overflow; bytes
//       [written, span.size()) zero-filled to '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_padded_span
//                  (NEW today; wraps Sep 21 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_throwing_padded_span
//                  (NEW today; wraps Sep 21 verbatim)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0
// (expected+untouched — the prior contract across Sep 6 through
// Sep 26).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 /
// Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
// Sep 26's macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.
//
// Today closes Sep 26's "Where we go next" item #1 verbatim — the
// span<char> mirror of Sep 26's char*+cap (THROWING × PADDED)
// cells.  The 2^5 = 32-cell matrix on the BOUNDED-BUFFER ×
// (expected-or-throwing) quadrant on the std::span<char> side is
// now COMPLETE (4 dispatch cells × 4 sibling wrappers = 16 cells).
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling — matches
// Sep 6 through Sep 26's contract).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

// PETRA_PAD defaults to 0 (untouched sibling — matches Sep 6 through
// Sep 26's contract).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
#endif

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0
// (0, 0) → expected+untouched sibling path.  Returns
// std::expected<std::size_t, FormatError>.  On TooLarge, the
// underlying impl returns FormatError{TooLarge, ..., needed_bytes}
// UNCHANGED; the buffer is UNTOUCHED on overflow.

// CONSTEVAL surface: dispatches to Sep 10's try_format_bounded_runtime_n_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)               \
                                                __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 19's try_format_runtime_n(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1
// (0, 1) → expected+padded sibling path.  Returns
// std::expected<std::size_t, FormatError>.  On success, bytes
// [written, span.size()) are zero-filled to '\0'.  On overflow or
// Format, the buffer is UNTOUCHED (atomicity preserved by the
// underlying impl; the padded wrapper does NOT touch the buffer on
// error — see Section 3).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_padded_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_padded_span((out), (fmt)        \
                                                      __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_padded_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_padded_span((out), (fmt)               \
                                              __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0
// (1, 0) → throwing+untouched sibling path.  Returns std::size_t
// on success.  On TooLarge, throws std::runtime_error carrying the
// SAME .what() message that FormatError{TooLarge, ...} would carry.
// The buffer is UNTOUCHED on overflow (atomicity preserved by
// Sep 10's / Sep 19's underlying impl).

// CONSTEVAL surface: dispatches to Sep 21's NEW
// try_format_bounded_runtime_n_throwing_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)     \
                                                        __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 21's NEW
// try_format_runtime_n_throwing(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                  \
                                            __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1
// (1, 1) → throwing+padded sibling path.  Returns std::size_t on
// success.  On success, bytes [written, span.size()) are zero-
// filled to '\0'.  On overflow or Format, throws std::runtime_error;
// the buffer is UNTOUCHED (atomicity preserved by the underlying
// throwing wrapper — Sep 21's wrapper already delegates atomicity
// to Sep 10's / Sep 19's underlying impl, and TODAY's NEW padded
// wrapper does NOT touch the buffer on error — see Section 3).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_throwing_padded_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_padded_span(          \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_throwing_padded_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_padded_span((out), (fmt)     \
                                                        __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_PAD

// ============================================================================
// Part 9 — Helper dispatcher macros used in Section 14's decltype checks
// to pin the perpendicular-axis claim.
//
// These are NOT new surface; they are simply verbatim thin
// dispatchers to the (1, 1) throwing+padded siblings on the
// BOUNDED-BUFFER span<char> axis.  Used to prove that today's
// meta-macros dispatch to the same wrappers as these "flat"
// dispatchers under PETRA_THROW_ON_OVERFLOW=1 + PETRA_PAD=1.
// ============================================================================

#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW_ORIG(out, fmt, ...)         \
    (::petra::try_format_bounded_runtime_n_throwing_padded_span(          \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW_ORIG(out, fmt, ...) \
    (::petra::try_format_runtime_n_throwing_padded_span((out), (fmt)     \
                                                        __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 10 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26).
// ============================================================================

namespace {

int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

#define SECTION(name) do { g_section = name; } while (0)

#define CHECK(cond) do {                                                 \
    if ((cond)) {                                                         \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr,                                             \
            "FAIL: %s:%d  [%s]  CHECK(%s)\n",                            \
            __FILE__, __LINE__, g_section, #cond);                       \
    }                                                                    \
} while (0)

#define CHECK_EQ(a, b) do {                                              \
    auto _a = (a); auto _b = (b);                                        \
    if ((_a == _b)) {                                                    \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr,                                             \
            "FAIL: %s:%d  [%s]  CHECK_EQ(%s, %s)\n",                     \
            __FILE__, __LINE__, g_section, #a, #b);                      \
    }                                                                    \
} while (0)

}  // namespace// ============================================================================
// Part 11 — sections
// ============================================================================

int main() {
    // ----- Section 1 — (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) flag pair
    //       pinned.
    SECTION("Section 1 - (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) flag pair");
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                      || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
    static_assert(PETRA_PAD == 0 || PETRA_PAD == 1,
                  "PETRA_PAD must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0 "
        "(expected+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1 "
        "(expected+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0 "
        "(throwing+untouched sibling)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1 "
        "(throwing+NUL-padded sibling)");
#endif
    CHECK(true);

    // 1b. Pin std::span<char> properties for posterity.
    static_assert(std::ranges::contiguous_range<std::span<char>>);
    static_assert(std::ranges::sized_range<std::span<char>>);
    static_assert(std::is_trivially_copyable_v<std::span<char>>);
    static_assert(std::is_standard_layout_v<std::span<char>>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().data()),
                                 char*>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().size()),
                                 std::size_t>);
    CHECK(true);

    // 1c. Pin std::runtime_error / std::format_error relationship.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // ----- Section 2 — Padded byte-state contract on success.
    //     On (0, 1) and (1, 1): bytes [written, span.size()) are
    //     '\0' on success.  On (0, 0) and (1, 0): bytes [written,
    //     span.size()) are UNTOUCHED (the original poison byte).
    SECTION("Section 2 - Padded byte-state contract on success");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
        std::size_t cap = buf.size();  // == 16
        std::size_t expected_written = 5;  // "ABCDE"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        CHECK_EQ(buf[0], 'A');
        CHECK_EQ(buf[1], 'B');
        CHECK_EQ(buf[2], 'C');
        CHECK_EQ(buf[3], 'D');
        CHECK_EQ(buf[4], 'E');
        // Verify the suffix.
#if PETRA_PAD == 1
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '\0');
        }
#else
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#endif
    }

    // 2a. Same checks but with a sub-span (the "no-bleed" case).
    //     A 6-byte sub-span at offset 5 of a 16-byte backing buffer
    //     with format 'ABCDE' (5 bytes) leaves bytes [0,5) and
    //     [11,16) as poison 'Q' regardless of (PETRA_PAD) — the
    //     pad loop only touches bytes INSIDE the span.
    SECTION("Section 2a - Sub-span no-bleed on padded path");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> sub{backing + 5, 6};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(sub, "ABCDE");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 5u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(sub, "ABCDE");
            CHECK_EQ(n, 5u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2a - unexpected throw on success\n");
        }
#endif
        // Prefix inside the span.
        CHECK_EQ(sub[0], 'A');
        CHECK_EQ(sub[1], 'B');
        CHECK_EQ(sub[2], 'C');
        CHECK_EQ(sub[3], 'D');
        CHECK_EQ(sub[4], 'E');
        // Suffix inside the span.
#if PETRA_PAD == 1
        CHECK_EQ(sub[5], '\0');  // last byte of span, NUL-padded
#else
        CHECK_EQ(sub[5], 'Q');   // last byte of span, untouched
#endif
        // Outside the span — must be UNTOUCHED on BOTH paths.
        // Before the span: backing[0..5) should still be 'Q'.
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
        // After the span: backing[11..16) should still be 'Q'.
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
    }

    // 2c. cap=0 edge case on the padded path: the 'for (i = written;
    //     i < cap; ++i)' loop has i < cap == 0 as the initial
    //     condition, so the loop body never executes.
    SECTION("Section 2c - cap=0 edge case on padded path");
    {
        char backing[1] = {'Q'};
        std::span<char> empty_span{backing, 0};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(empty_span, "");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 0u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(empty_span, "");
            CHECK_EQ(n, 0u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2c - unexpected throw on cap=0+empty-fmt\n");
        }
#endif
        // The sentinel byte OUTSIDE the cap=0 writable region
        // remains untouched.
        CHECK_EQ(backing[0], 'Q');
    }

    // 2d. written==cap edge case: format 'ABCDEFGH' (8 bytes) into
    //     a span of size 8.  On the padded path, the 'for (i =
    //     written; i < cap; ++i)' loop has i == cap as the initial
    //     condition, so the loop body never executes (the span is
    //     exactly filled).
    SECTION("Section 2d - written==cap edge case on padded path");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGH");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 8u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGH");
            CHECK_EQ(n, 8u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2d - unexpected throw on full-buffer\n");
        }
#endif
        for (std::size_t i = 0; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], "ABCDEFGH"[i]);
        }
    }

    // ----- Section 3 — Buffer atomicity on TooLarge.
    //     On all four flag pairs, the buffer must be UNTOUCHED on
    //     overflow.
    SECTION("Section 3 - Buffer atomicity on TooLarge");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        // Expected sibling path: r.error() contains FormatError.
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "this is way too long to fit");
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
#else
        // Throwing sibling path: throws std::runtime_error.
        bool caught = false;
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                buf, "this is way too long to fit");
            (void)n;
        } catch (const std::runtime_error&) {
            caught = true;
        } catch (...) {
            // unexpected
        }
        CHECK(caught);
#endif
        // Buffer must be UNTOUCHED on overflow.
        for (std::size_t i = 0; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
    }

    // ----- Section 4 — Byte-count contract on success.
    SECTION("Section 4 - Byte-count contract on success");
    {
        char backing[32] = {};
        for (std::size_t i = 0; i < 32; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 32};
        std::size_t expected_written = 13;  // "Hello, world!"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "Hello, {}!", "world");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                buf, "Hello, {}!", "world");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 4 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "Hello, world!"[i]);
        }
    }

    // ----- Section 5 — RUNTIME surface sanity check.
    SECTION("Section 5 - RUNTIME surface sanity check");
    {
        char backing[32] = {};
        for (std::size_t i = 0; i < 32; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 32};
        std::size_t expected_written = 11;  // "value is 42"
#if PETRA_THROW_ON_OVERFLOW == 0
        std::string_view fmt = "value is {}";
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            buf, fmt, 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            std::string_view fmt = "value is {}";
            auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, fmt, 42);
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 5 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "value is 42"[i]);
        }
    }

    // ----- Section 6 — CONSTEVAL gate pinned (latter part of
    //     section).  The CONSTEVAL macro's consteval gate requires
    //     a literal fmt at the call site.  This is verified by
    //     /tmp/probe_consteval_rejects_runtime_fmt_span_pad_throw.cpp
    //     (Pinned separately — see the lesson's "Consteval-gate
    //     probe" section.)
    SECTION("Section 6 - CONSTEVAL gate requires literal fmt");
    {
        // Pin the static_assert that the CONSTEVAL macro picks
        // std::format_string<Args...> regardless of flag pair.
#if PETRA_THROW_ON_OVERFLOW == 0
        using F1 = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using ExpectedFmt1 = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<F1, ExpectedFmt1>,
                      "CONSTEVAL macro must return expected<size_t, "
                      "FormatError> on the expected path");
#else
        using F1 = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<F1, std::size_t>,
                      "CONSTEVAL macro must return std::size_t on the "
                      "throwing path");
#endif
        CHECK(true);
    }

    // ----- Section 7 — RUNTIME macro accepts std::string_view (NOT
    //     consteval-checked).
    SECTION("Section 7 - RUNTIME macro accepts std::string_view");
    {
        // Pin the static_assert that the RUNTIME macro picks
        // std::string_view regardless of flag pair.
#if PETRA_THROW_ON_OVERFLOW == 0
        using F1 = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedFmt1 = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<F1, ExpectedFmt1>,
                      "RUNTIME macro must return expected<size_t, "
                      "FormatError> on the expected path");
#else
        using F1 = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<F1, std::size_t>,
                      "RUNTIME macro must return std::size_t on the "
                      "throwing path");
#endif
        CHECK(true);
    }

    // ----- Section 8 — Format error: atomicity preserved on the
    //     RUNTIME surface.
    SECTION("Section 8 - Format error atomicity (RUNTIME)");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
        std::string bad_fmt = "bad {";  // unterminated brace
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            buf, std::string_view{bad_fmt});
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
#else
        bool caught = false;
        try {
            auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, std::string_view{bad_fmt});
            (void)n;
        } catch (...) {
            caught = true;
        }
        CHECK(caught);
#endif
        // Buffer must be UNTOUCHED on Format error (no byte at any
        // index was written).
        for (std::size_t i = 0; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
    }

    // ----- Section 9 — std::format_error IS-A std::runtime_error.
    SECTION("Section 9 - std::format_error IS-A std::runtime_error");
    {
        // Pinned by static_assert (compile-time) — this is a
        // fundamental property of the C++23 standard library.
        static_assert(
            std::is_base_of_v<std::runtime_error, std::format_error>,
            "std::format_error must derive from std::runtime_error");
        CHECK(true);
    }

    // ----- Section 10 — Concurrency: 4 threads × 25 concurrent
    //       calls.
    SECTION("Section 10 - Concurrency (4 threads x 25 calls)");
    {
        std::atomic<int> a_pass{0};
        std::atomic<int> a_fail{0};
        auto worker = [&a_pass, &a_fail]() {
            int lp = 0, lf = 0;
            char backing[32] = {};
            std::span<char> buf{backing, 32};
            for (int i = 0; i < 25; ++i) {
                for (std::size_t j = 0; j < buf.size(); ++j) buf[j] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
                auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                    buf, "iteration {}", i);
                if (r.has_value() && r.value() > 0u) {
                    ++lp;
                } else {
                    ++lf;
                }
#else
                try {
                    auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                        buf, "iteration {}", i);
                    if (n > 0u) {
                        ++lp;
                    } else {
                        ++lf;
                    }
                } catch (...) {
                    ++lf;
                }
#endif
            }
            a_pass.fetch_add(lp, std::memory_order_relaxed);
            a_fail.fetch_add(lf, std::memory_order_relaxed);
        };
        std::array<std::thread, 4> threads;
        for (auto& t : threads) {
            t = std::thread(worker);
        }
        for (auto& t : threads) {
            t.join();
        }
        CHECK_EQ(a_pass.load(), 100);
        CHECK_EQ(a_fail.load(), 0);
    }

    // ----- Section 11 — No-args case: __VA_OPT__(,) works.
    SECTION("Section 11 - No-args case via __VA_OPT__(,)");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf,
                                                       "no args");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 7u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf,
                                                           "no args");
            CHECK_EQ(n, 7u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 11 - unexpected throw on no-args\n");
        }
#endif
        for (std::size_t i = 0; i < 7; ++i) {
            CHECK_EQ(buf[i], "no args"[i]);
        }
    }

    // ----- Section 12 — Multiple args + value formatting.
    SECTION("Section 12 - Multiple args + value formatting");
    {
        char backing[64] = {};
        for (std::size_t i = 0; i < 64; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 64};
        // "x=42, y=hello, z=3.14" -> 21 bytes
        std::size_t expected_written = 21;
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "x={}, y={}, z={:.2f}", 42, "hello", 3.14);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                buf, "x={}, y={}, z={:.2f}", 42, "hello", 3.14);
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 12 - unexpected throw on success\n");
        }
#endif
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "x=42, y=hello, z=3.14"[i]);
        }
    }

    // ----- Section 13 — Macro is a parenthesized expression (can be
    //       captured as 'auto n = MACRO(...)').
    SECTION("Section 13 - Macro returns parenthesized expression");
    {
        char backing[32] = {};
        for (std::size_t i = 0; i < 32; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 32};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "5");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 1u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "5");
            CHECK_EQ(n, 1u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 13 - unexpected throw on success\n");
        }
#endif
    }

    // ----- Section 14 — Perpendicular-axis claim pinned via
    //       decltype checks on BOTH paths.
    SECTION("Section 14 - Perpendicular-axis decltype checks");
    {
        // Pin that the macros dispatch to the correct sibling
        // regardless of the flag pair.  Verify by checking that
        // decltype(MACRO(...)) matches the expected return type
        // for each (THROW, PAD) combination.
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0
        // (0, 0) → expected+untouched.  BOTH surfaces return
        // expected<size_t, FormatError>.
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "(0,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1
        // (0, 1) → expected+padded.  BOTH surfaces return
        // expected<size_t, FormatError>.
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "(0,1) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0
        // (1, 0) → throwing+untouched.  BOTH surfaces return
        // std::size_t.
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0) RUNTIME must return std::size_t");
#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1
        // (1, 1) → throwing+padded.  BOTH surfaces return
        // std::size_t.
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,1) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1) RUNTIME must return std::size_t");
#endif
        CHECK(true);
    }

    // ----- Section 15 — std::format_error dynamic_cast check.
    SECTION("Section 15 - std::format_error dynamic_cast check");
    {
        // On the (1, 1) throwing+padded path, the exception thrown
        // is std::runtime_error (the throwing wrapper re-raises as
        // std::runtime_error, NOT std::format_error).  Verify that
        // dynamic_cast<const std::format_error*> returns nullptr
        // when applied to the caught exception reference.
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
#if PETRA_THROW_ON_OVERFLOW == 1
        bool caught_runtime = false;
        bool dynamic_cast_is_nullptr = false;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, std::string_view{"bad {"});
        } catch (const std::runtime_error& e) {
            caught_runtime = true;
            // The throwing wrapper re-raises as std::runtime_error
            // (NOT std::format_error).  dynamic_cast to
            // std::format_error* must return nullptr.
            dynamic_cast_is_nullptr =
                (dynamic_cast<const std::format_error*>(&e) == nullptr);
        } catch (...) {
            // unexpected
        }
        CHECK(caught_runtime);
        CHECK(dynamic_cast_is_nullptr);
#else
        // On the expected path, this section is not exercised —
        // just pin CHECK(true) so the count is consistent.
        CHECK(true);
#endif
    }

    std::println("=========================================================");
    std::println("PASS: {}  FAIL: {}", g_pass, g_fail);
    std::println("=========================================================");
    if (g_fail != 0) {
        std::println("OVERALL: FAIL");
        return 1;
    }
    std::println("OVERALL: PASS");
    return 0;
}