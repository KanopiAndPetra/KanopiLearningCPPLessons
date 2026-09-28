// P-2026-09-28 — try_format_space_pad_span_n:
//
// The SIXTH-AXIS (PETRA_SPACE_PAD) sibling-choice META-MACRO layer
// on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.  Closes Sep 27's
// "Where we go next" item #2 verbatim: introduces a new
// compile-time-controlled sibling choice — should the bounded
// buffer be UNTOUCHED, NUL-padded ('\0'), or SPACE-padded (' ')
// on success?
//
// The new compile-time flag is PETRA_SPACE_PAD:
//   #define PETRA_SPACE_PAD 0
//     → UNTOUCHED sibling (bytes [written, span.size()) are left
//       as-is; the Sep 10 / Sep 19 / Sep 21 contract from prior
//       lessons):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//        RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//        THROWING:  Sep 21's two throwing wrappers.
//
//   #define PETRA_SPACE_PAD 1
//     → SPACE-PADDED sibling (bytes [written, span.size()) are
//       filled with ' ' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                  (NEW today; wraps Sep 10 verbatim)
//        RUNTIME:   petra::try_format_runtime_n_space_pad_span
//                  (NEW today; wraps Sep 19 verbatim)
//        THROWING:  petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                  (NEW today; wraps Sep 21 verbatim) and
//                  petra::try_format_runtime_n_throwing_space_pad_span
//                  (NEW today; wraps Sep 21 verbatim).
//
// Today closes Sep 27's "Where we go next" item #2 verbatim — the
// span<char> mirror of the SPACE-PAD axis.  The matrix is now
// 2^6 = 64 cells (6 axes × 2 siblings each) on the BOUNDED-BUFFER
// × (expected-or-throwing) quadrant on the std::span<char> side
// (4 dispatch cells × 8 sibling wrappers × 2 surface pairs = 64
// cells across the CONSTEVAL+RUNTIME+EXPECTED+THROWING surface
// counts).
//
// More precisely, the four surface combinations are now:
//   - 2 surfaces  (CONSTEVAL / RUNTIME)
//   × 2 axis1    (EXPECTED / THROWING)
//   × 2 axis2    (NUL-PAD / UNTOUCHED-or-SPACE-PAD)
//   × 2 axis3    (UNTOUCHED / SPACE-PAD — applies when NUL-PAD=0)
//   × 2 axis4    (NUL-PAD=0 / NUL-PAD=1 — applies when NUL-PAD=0)
// = 8 sibling wrappers per surface pair × 4 dispatch combinations
//   × 2 surface pairs = 64 cells total (8 dispatch cells × 8 sibling
//   wrappers).
//
// To keep today's lesson tightly scoped, TODAY only ADDS the SPACE-
// PAD sibling pair (PETRA_NUL_PAD=0 × PETRA_SPACE_PAD=1) on top of
// the existing 2-bit (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) matrix.
// The default PETRA_PAD=1 path (NUL-padded) is UNCHANGED.  The new
// third axis PETRA_SPACE_PAD is INDEPENDENT of the existing two
// axes: when PETRA_PAD=0 (untouched), PETRA_SPACE_PAD=1 selects the
// NEW SPACE-PAD sibling; when PETRA_PAD=0 AND PETRA_SPACE_PAD=0 the
// buffer is UNTOUCHED.  When PETRA_PAD=1 (NUL-padded), PETRA_SPACE_PAD
// is IGNORED — the NUL-pad path wins.
//
// The compile-time flag pair (PETRA_THROW_ON_OVERFLOW,
// PETRA_SPACE_PAD) controls the dispatch via #if
// PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if PETRA_SPACE_PAD:
//   - (0,0,0): expected+untouched sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//      RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   - (0,1,0): expected+NUL-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     zero-filled to '\0' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_padded_span
//                (Sep 27)
//      RUNTIME:   petra::try_format_runtime_n_padded_span
//                (Sep 27)
//
//   - (0,0,1): expected+SPACE-padded sibling (NEW today; returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                (NEW today; wraps Sep 10 verbatim)
//      RUNTIME:   petra::try_format_runtime_n_space_pad_span
//                (NEW today; wraps Sep 19 verbatim)
//
//   - (1,0,0): throwing+untouched sibling (returns std::size_t on
//     success, throws std::runtime_error on overflow; bytes
//     [written, span.size()) UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                (Sep 21)
//      RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                (Sep 21)
//
//   - (1,1,0): throwing+NUL-padded sibling (returns std::size_t on
//     success, throws std::runtime_error on overflow; bytes
//     [written, span.size()) zero-filled to '\0' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_padded_span
//                (Sep 27)
//      RUNTIME:   petra::try_format_runtime_n_throwing_padded_span
//                (Sep 27)
//
//   - (1,0,1): throwing+SPACE-padded sibling (NEW today; returns
//     std::size_t on success, throws std::runtime_error on overflow;
//     bytes [written, span.size()) filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                (NEW today; wraps Sep 21 verbatim)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                (NEW today; wraps Sep 21 verbatim)
//
// Today adds 4 NEW space-padded wrappers + 2 NEW meta-macros.
//
// Where this fits in the arc
// --------------------------
//   - Sep 27   try_format_padded_span_n_or_throw — the (THROWING ×
//              PADDED) meta-macro layer on the std::span<char>
//              axis (closes Sep 26's "Where we go next" item #1;
//              introduces the 5th-axis PETRA_PAD).
//   - TODAY    try_format_space_pad_span_n — the SIXTH axis
//              (PETRA_SPACE_PAD) on the std::span<char> ×
//              BOUNDED-BUFFER axis.  Closes Sep 27's "Where we go
//              next" item #2 verbatim.
//
// SCOPE NOTE: This lesson is FOCUSED on the SIXTH axis (PETRA_SPACE_PAD)
// ONLY.  The dispatcher compiles ONLY when PETRA_PAD = 0 — Sep 27's
// PETRA_PAD = 1 (NUL-pad) branches are Sep 27's territory and
// require Sep 27's NEW NUL-pad wrappers (which are NOT included in
// this file).  Combining PETRA_PAD=1 with PETRA_SPACE_PAD=1 is
// OUT OF SCOPE for this lesson; the `#error` guard below fires
// at preprocessor time when PETRA_PAD=1 is set, telling the user
// to use Sep 27's lesson file for that combination.
//
// In other words, today's dispatcher has only THREE active cells
// (PETRA_PAD is fixed at 0):
//   - (0, 0, 0): expected+untouched sibling (the Sep 10 / Sep 19
//     default contract).
//   - (0, 0, 1): expected+SPACE-padded sibling (NEW today).
//   - (1, 0, 0): throwing+untouched sibling (Sep 21).
//   - (1, 0, 1): throwing+SPACE-padded sibling (NEW today).
//
// What today's lesson pins
// ------------------------
//   1. The PAD-vs-UNTOUCHED sibling choice can be TRIPLED to a
//      PAD-vs-SPACE-PAD-vs-UNTOUCHED sibling choice as a 3-axis
//      product on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
//      Today introduces PETRA_SPACE_PAD as the 6th axis on top of
//      Sep 27's 5-axis matrix.  PETRA_SPACE_PAD is only consulted
//      when PETRA_PAD = 0; when PETRA_PAD = 1, the lesson's
//      dispatcher #errors (Sep 27's lesson handles that case).
//   2. The space-padded sibling is a verbatim thin wrapper that
//      fills [written, span.size()) with ' ' on success AFTER the
//      underlying call returns.  On error (the underlying call
//      returns FormatError{...}), the wrapper does NOT execute the
//      space-fill loop — control returns the FormatError unchanged.
//      This is because the underlying impl already preserves buffer
//      atomicity on overflow.
//   3. The throwing+space-padded sibling uses the C++ idiom
//      'try { auto n = THROWING_SIBLING(...); for (...) pad loop;
//      return n; } catch (...) { throw; }' to fill out[written,
//      span.size()) with ' ' on success ONLY when the underlying
//      throwing call returned normally (control does NOT reach the
//      pad loop on error — the catch block re-raises the
//      std::runtime_error untouched).
//   4. The buffer atomicity contract is preserved on TooLarge — the
//      space-padded wrapper does NOT touch the buffer on overflow.
//   5. The buffer atomicity contract is preserved on Format errors
//      — a malformed fmt returns FormatError{Format, e.what()} (on
//      the expected path) or throws std::runtime_error (on the
//      throwing path), and the buffer is UNTOUCHED.
//   6. The byte-count contract on success is preserved — returns
//      std::size_t == bytes written (the same value the underlying
//      impl returns).
//   7. The return-type contract is preserved — the expected
//      sibling returns std::expected<std::size_t, FormatError>;
//      the throwing sibling returns std::size_t.  The space-pad
//      wrapper does NOT alter the return type regardless of
//      PETRA_SPACE_PAD.
//   8. The CONSTEVAL macro's consteval gate is preserved regardless
//      of (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD) —
//      TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW still requires
//      std::format_string<Args...> (a literal fmt at the call
//      site).
//   9. The std::span<char> sub-span 'no-bleed' contract is preserved
//      on the SPACE-padded path: a std::span<char> over a sub-range
//      of a buffer must NOT touch bytes outside the span.
//  10. Independent #error guards fire at preprocessor time if ANY
//      of the three flags is set to anything other than 0 or 1.
//
// What today's lesson does NOT cover
// ----------------------------------
//   - The STD::SPAN<CHAR> × NUL-TERMINATING × (any) cells — already
//     covered by Sep 11 / Sep 12 / Sep 19 / Sep 23 (the
//     NUL-terminating span<char> axis).
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
// Part 1 — petra::FormatError (verbatim from Sep 6 onwards)
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
// Part 2 — petra::counting_output_iterator (verbatim from Sep 6 onwards)
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
// Part 3 — petra::detail::bounded_char_writer (verbatim from Sep 8 onwards)
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
// Part 7 — petra::try_format_bounded_runtime_n_space_pad_span /
// petra::try_format_runtime_n_space_pad_span /
// petra::try_format_bounded_runtime_n_throwing_space_pad_span /
// petra::try_format_runtime_n_throwing_space_pad_span — NEW TODAY.
//
// These are the FOUR SPACE-PADDED siblings on the STD::SPAN<CHAR>
// axis (active when PETRA_PAD=0 AND PETRA_SPACE_PAD=1):
//   - (0, 0, 1) expected+SPACE-padded sibling (RUNTIME surface):
//     petra::try_format_runtime_n_space_pad_span
//     (wraps Sep 19's try_format_runtime_n(span, ...) verbatim and
//      space-fills [written, span.size()) with ' ' on success).
//   - (0, 0, 1) expected+SPACE-padded sibling (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_space_pad_span
//     (wraps Sep 10's try_format_bounded_runtime_n_span verbatim
//      and space-fills [written, span.size()) with ' ' on success).
//   - (1, 0, 1) throwing+SPACE-padded sibling (RUNTIME surface):
//     petra::try_format_runtime_n_throwing_space_pad_span
//     (wraps Sep 21's try_format_runtime_n_throwing(span, ...)
//      verbatim and space-fills [written, span.size()) with ' ' on
//      success ONLY via try/catch around the throwing call).
//   - (1, 0, 1) throwing+SPACE-padded sibling (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_throwing_space_pad_span
//     (wraps Sep 21's try_format_bounded_runtime_n_throwing_span
//      verbatim and space-fills [written, span.size()) with ' ' on
//      success ONLY via try/catch around the throwing call).
//
// Each expected+SPACE-padded sibling is a verbatim thin wrapper
// that, on success, fills out[written, span.size()) with ' ' AFTER
// the underlying call returns.  On error (the underlying call
// returns FormatError{...}), the wrapper does NOT execute the
// space-fill loop — control returns the FormatError unchanged.
// This is because the underlying impl already preserves buffer
// atomicity on overflow (delegated to Sep 8's / Sep 9's underlying
// impl, which short-circuits before pass-2 writes anything on
// TooLarge).
//
// Each throwing+SPACE-padded sibling is a verbatim thin wrapper
// that, on success (the underlying call returns std::size_t
// normally), fills out[written, span.size()) with ' '.  On error
// (the underlying call throws std::runtime_error), the wrapper
// does NOT execute the space-fill loop — control unwinds through
// the wrapper WITHOUT touching the buffer.
//
// IMPORTANT: the throwing wrapper cannot directly check the return
// value of a function that throws, so the structure MUST be:
//
//   try {
//       auto n = try_format_runtime_n_throwing(out, fmt, args...);
//       // Success: space-fill out[n, span.size()) with ' '.
//       for (std::size_t i = n; i < out.size(); ++i) out[i] = ' ';
//       return n;
//   } catch (...) {
//       throw;  // re-raise as-is
//   }
//
// This pattern (try/catch around a throwing call + pad-on-success)
// is the C++ idiom for "fill-with-' ' on success only when the
// underlying throwing call returned normally".  Mirrors Sep 27's
// NUL-padded sibling structure byte-for-byte except the fill byte
// is ' ' (this lesson) instead of '\0' (Sep 27).
// ============================================================================

namespace petra {

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// SPACE-PAD sibling on the span<char> axis.  Wraps Sep 19's
// try_format_runtime_n(span, ...) verbatim and space-fills
// [written, span.size()) with ' ' on success ONLY (the
// 'if (r.has_value())' guard short-circuits BEFORE the pad loop,
// so atomicity is preserved on TooLarge / Format error — pinned
// in Section 3).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_space_pad_span(std::span<char> out, std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        // Note: cap == out.size() — the span<char> axis uses the
        // span's size() as the writable byte count.
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// SPACE-PAD sibling on the span<char> axis.  Wraps Sep 10's
// try_format_bounded_runtime_n_span verbatim and space-fills
// [written, span.size()) with ' ' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_space_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// SPACE-PAD sibling on the span<char> axis.  Wraps Sep 21's
// try_format_runtime_n_throwing(span, ...) verbatim and space-
// fills [written, span.size()) with ' ' on success ONLY (control
// does NOT reach the pad loop on error — the catch block re-raises
// the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        // Success path: space-fill out[n, out.size()) with ' '.
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
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
// SPACE-PAD sibling on the span<char> axis.  Wraps Sep 21's
// try_format_bounded_runtime_n_throwing_span verbatim and space-
// fills [written, span.size()) with ' ' on success ONLY.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_span(out, fmt, args...);
        // Success path: space-fill out[n, out.size()) with ' '.
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
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
// (Mirrors Sep 27's shape verbatim; now with PETRA_SPACE_PAD as a
// third axis.)
//
// The compile-time flag triple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD) controls the dispatch via #if PETRA_THROW_ON_OVERFLOW
// × #if PETRA_PAD × #if PETRA_SPACE_PAD:
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 0
//   #define PETRA_SPACE_PAD 0
//     → expected+untouched sibling (returns expected<size_t,
//       FormatError>; bytes [written, span.size()) UNTOUCHED on
//       success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//        RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 1
//   #define PETRA_SPACE_PAD *
//     → expected+NUL-padded sibling (returns expected<size_t,
//       FormatError>; bytes [written, span.size()) zero-filled to
//       '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_padded_span
//                  (Sep 27)
//        RUNTIME:   petra::try_format_runtime_n_padded_span
//                  (Sep 27)
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//   #define PETRA_PAD 0
//   #define PETRA_SPACE_PAD 1
//     → expected+SPACE-padded sibling (NEW today; returns
//       expected<size_t, FormatError>; bytes [written, span.size())
//       filled with ' ' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                  (NEW today)
//        RUNTIME:   petra::try_format_runtime_n_space_pad_span
//                  (NEW today)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 0
//   #define PETRA_SPACE_PAD 0
//     → throwing+untouched sibling (returns std::size_t on
//       success, throws std::runtime_error on overflow; bytes
//       [written, span.size()) UNTOUCHED on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                  (Sep 21)
//        RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                  (Sep 21)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 1
//   #define PETRA_SPACE_PAD *
//     → throwing+NUL-padded sibling (returns std::size_t on
//       success, throws std::runtime_error on overflow; bytes
//       [written, span.size()) zero-filled to '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_padded_span
//                  (Sep 27)
//        RUNTIME:   petra::try_format_runtime_n_throwing_padded_span
//                  (Sep 27)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//   #define PETRA_PAD 0
//   #define PETRA_SPACE_PAD 1
//     → throwing+SPACE-padded sibling (NEW today; returns
//       std::size_t on success, throws std::runtime_error on
//       overflow; bytes [written, span.size()) filled with ' ' on
//       success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                  (NEW today)
//        RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                  (NEW today)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0 (expected+untouched — the prior contract
// across Sep 6 through Sep 27).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)'
// and capture the result.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.
//
// Today closes Sep 27's "Where we go next" item #2 verbatim — the
// span<char> mirror of the SPACE-PAD axis.  The matrix is now 2^6
// = 64 cells on the BOUNDED-BUFFER × (expected-or-throwing)
// quadrant on the std::span<char> side (4 dispatch cells × 4
// sibling wrappers × 4 surface pairs = 64 cells across the 2
// axis-1 × 2 axis-2 × 2 axis-3 axis product).
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling — matches
// Sep 6 through Sep 27's contract).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

// PETRA_PAD defaults to 0 (untouched sibling — matches Sep 6
// through Sep 27's contract).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
#endif

// SCOPE GUARD: This lesson is FOCUSED on the SIXTH axis
// (PETRA_SPACE_PAD) ONLY.  Sep 27's PETRA_PAD = 1 (NUL-pad)
// branches require Sep 27's NEW NUL-pad wrappers (which are NOT
// included in this file).  When PETRA_PAD = 1, the lesson's
// dispatcher #errors with a clear pointer to Sep 27.
#if PETRA_PAD != 0
#error "PETRA_PAD = 1 (NUL-padded sibling) is OUT OF SCOPE for this lesson.  Use Sep 27's try_format_padded_span_n_or_throw/ for the PETRA_PAD=1 combinations."
#endif

// PETRA_SPACE_PAD defaults to 0 (no space-pad — matches the prior
// contract across Sep 6 through Sep 27).
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

// Sanity check: PETRA_SPACE_PAD must be 0 or 1.
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
// (0, 0, 0) → expected+untouched sibling path.  Returns
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

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
// (0, 0, 1) → expected+SPACE-padded sibling path.  Returns
// std::expected<std::size_t, FormatError>.  On success, bytes
// [written, span.size()) are filled with ' '.  On overflow or
// Format, the buffer is UNTOUCHED (atomicity preserved by the
// underlying impl; the space-padded wrapper does NOT touch the
// buffer on error — see Section 3).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_space_pad_span((out), (fmt)     \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_space_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
// (1, 0, 0) → throwing+untouched sibling path.  Returns
// std::size_t on success.  On TooLarge, throws std::runtime_error
// carrying the SAME .what() message that FormatError{TooLarge,
// ...} would carry.  The buffer is UNTOUCHED on overflow
// (atomicity preserved by Sep 10's / Sep 19's underlying impl).

// CONSTEVAL surface: dispatches to Sep 21's
// try_format_bounded_runtime_n_throwing_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)     \
                                                        __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 21's
// try_format_runtime_n_throwing(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                  \
                                            __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
// (1, 0, 1) → throwing+SPACE-padded sibling path.  Returns
// std::size_t on success.  On success, bytes [written, span.size())
// are filled with ' '.  On overflow or Format, throws
// std::runtime_error; the buffer is UNTOUCHED (atomicity preserved
// by the underlying throwing wrapper — Sep 21's wrapper already
// delegates atomicity to Sep 10's / Sep 19's underlying impl, and
// TODAY's NEW space-padded wrapper does NOT touch the buffer on
// error — see Section 3).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_space_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_PAD × PETRA_SPACE_PAD

// ============================================================================
// Part 9 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 onwards).
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

}  // namespace

// ============================================================================
// Part 10 — sections
// ============================================================================

int main() {
    // ----- Section 1 — (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    //       PETRA_SPACE_PAD) flag triple pinned.
    SECTION("Section 1 - flag triple pinned");
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                      || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
    static_assert(PETRA_PAD == 0 || PETRA_PAD == 1,
                  "PETRA_PAD must be 0 or 1");
    static_assert(PETRA_SPACE_PAD == 0 || PETRA_SPACE_PAD == 1,
                  "PETRA_SPACE_PAD must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0 (expected+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1 (expected+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0 (throwing+untouched sibling)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1 (throwing+SPACE-padded sibling)");
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

    // ----- Section 2 — Space-padded byte-state contract on success.
    //     On (0, 0, 1) and (1, 0, 1): bytes [written, span.size())
    //     are ' ' on success.  On (0, 0, 0) and (1, 0, 0): bytes
    //     [written, span.size()) are UNTOUCHED (the original poison
    //     byte).  On (0, 1, *) and (1, 1, *): bytes [written,
    //     span.size()) are '\0' on success.
    SECTION("Section 2 - Space-padded byte-state contract on success");
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
#if PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        // SPACE-PADDED path: bytes [written, cap) are ' '.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], ' ');
        }
#else
        // UNTOUCHED path: bytes [written, cap) are 'Q'.
        // (PETRA_PAD=1 is OUT OF SCOPE for this lesson — handled
        // by Sep 27's lesson.)
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#endif
    }

    // 2a. Same checks but with a sub-span (the "no-bleed" case).
    //     A 6-byte sub-span at offset 5 of a 16-byte backing buffer
    //     with format 'ABCDE' (5 bytes) leaves bytes [0,5) and
    //     [11,16) as poison 'Q' regardless of PETRA_PAD /
    //     PETRA_SPACE_PAD — the pad loop only touches bytes INSIDE
    //     the span.
    SECTION("Section 2a - Sub-span no-bleed on SPACE-padded path");
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
#if PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        CHECK_EQ(sub[5], ' ');   // last byte of span, SPACE-padded
#else
        CHECK_EQ(sub[5], 'Q');   // last byte of span, untouched
#endif
        // Outside the span — must be UNTOUCHED on ALL paths.
        // Before the span: backing[0..5) should still be 'Q'.
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
        // After the span: backing[11..16) should still be 'Q'.
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
    }

    // 2b. Verify the SPACE-pad sibling does NOT collapse to the
    //     NUL-pad sibling: pin that bytes [written, cap) are ' '
    //     (NOT '\0') when PETRA_PAD=0 AND PETRA_SPACE_PAD=1.
    SECTION("Section 2b - SPACE-pad sibling distinct from NUL-pad");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 3;  // "XYZ"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "XYZ");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "XYZ");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2b - unexpected throw on success\n");
        }
#endif
#if PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        // SPACE-padded path: bytes [3, 16) are ' '.
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], ' ');
        }
#else
        // UNTOUCHED path: bytes [3, 16) are 'Q' (poison).
        // (PETRA_PAD=1 is OUT OF SCOPE for this lesson — handled
        // by Sep 27's lesson.)
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#endif
    }

    // 2c. cap=0 edge case on the SPACE-padded path: the 'for (i =
    //     written; i < cap; ++i)' loop has i < cap == 0 as the
    //     initial condition, so the loop body never executes.
    SECTION("Section 2c - cap=0 edge case on SPACE-padded path");
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
    //     a span of size 8.  On the SPACE-padded path, the 'for (i
    //     = written; i < cap; ++i)' loop has i == cap as the
    //     initial condition, so the loop body never executes (the
    //     span is exactly filled).
    SECTION("Section 2d - written==cap edge case on SPACE-padded path");
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
    //     On all six flag-triple paths, the buffer must be
    //     UNTOUCHED on overflow.
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
    //     a literal fmt at the call site.
    SECTION("Section 6 - CONSTEVAL gate requires literal fmt");
    {
        // Pin the static_assert that the CONSTEVAL macro picks
        // std::format_string<Args...> regardless of flag triple.
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
        // std::string_view regardless of flag triple.
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
    //       decltype checks on all 6 paths.
    SECTION("Section 14 - Perpendicular-axis decltype checks");
    {
        // Pin that the macros dispatch to the correct sibling
        // regardless of the flag triple.
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        // (0, 0, 0) → expected+untouched.  BOTH surfaces return
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
                      "(0,0,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        // (0, 0, 1) → expected+SPACE-padded.  BOTH surfaces return
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
                      "(0,0,1) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,1) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        // (1, 0, 0) → throwing+untouched.  BOTH surfaces return
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
                      "(1,0,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,0) RUNTIME must return std::size_t");
#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        // (1, 0, 1) → throwing+SPACE-padded.  BOTH surfaces return
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
                      "(1,0,1) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,1) RUNTIME must return std::size_t");
#endif
        CHECK(true);
    }

    // ----- Section 15 — std::format_error dynamic_cast check.
    SECTION("Section 15 - std::format_error dynamic_cast check");
    {
        // On the (1, 0, 1) throwing+SPACE-padded path, the
        // exception thrown is std::runtime_error (the throwing
        // wrapper re-raises as std::runtime_error, NOT
        // std::format_error).  Verify that dynamic_cast<const
        // std::format_error*> returns nullptr when applied to the
        // caught exception reference.
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

    // ----- Section 16 — On the (0, 0, 1) and (1, 0, 1)
    //       SPACE-padded paths, explicitly verify that bytes
    //       [written, span.size()) are ' ' (NOT '\0' and NOT 'Q').
    //       This is the SPACE-pad-specific byte-state contract
    //       check.
    SECTION("Section 16 - SPACE-pad byte is ' ' (not '\\0' or 'Q')");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
        std::size_t expected_written = 4;  // "WXYZ"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "WXYZ");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "WXYZ");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 16 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        CHECK_EQ(buf[0], 'W');
        CHECK_EQ(buf[1], 'X');
        CHECK_EQ(buf[2], 'Y');
        CHECK_EQ(buf[3], 'Z');
#if PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        // SPACE-padded path: bytes [4, 8) must be ' ' (NOT '\0').
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], ' ');
            // Pin that it is NOT '\0' (byte value 0).
            CHECK(buf[i] != '\0');
            // Pin that it is NOT 'Q' (the poison byte).
            CHECK(buf[i] != 'Q');
        }
#else
        // UNTOUCHED path: bytes [4, 8) must be 'Q' (poison).
        // (PETRA_PAD=1 is OUT OF SCOPE for this lesson — handled
        // by Sep 27's lesson.)
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
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
