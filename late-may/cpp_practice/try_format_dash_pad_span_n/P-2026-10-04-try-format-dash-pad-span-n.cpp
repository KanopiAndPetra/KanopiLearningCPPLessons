// P-2026-10-04 — try_format_dash_pad_span_n:
//
// The SEVENTH-AXIS (PETRA_DASH_PAD) sibling-choice META-MACRO
// layer on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis — a NEW
// FOURTH compile-time flag on top of Oct 1's 3-bit tuple
// (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD).
//
// Closes Oct 3's "Where we go next" item (1) verbatim:
//
// > STD::SPAN<CHAR> MIRROR of today's DASH-pad lesson — would
// > be the span<char> mirror of today's char*+cap lesson (the
// > SEVENTH axis on the std::span<char> side).  Closes the
// > (0,0,0,1), (0,0,1,1), (1,0,0,1), and (1,0,1,1) cells on
// > the span<char> side.  Each future lesson adds verbatim thin
// > DASH-pad wrappers composed on top of today's DASH-pad
// > char*+cap wrappers.  (4 new dispatch cells per axis
// > extension.)
//
// The new compile-time flag is PETRA_DASH_PAD:
//   #define PETRA_DASH_PAD 0
//     → UNTOUCHED sibling (bytes [written, span.size()) are left
//       as-is; the Sep 10 / Sep 19 / Sep 21 contract from prior
//       lessons).
//   #define PETRA_DASH_PAD 1
//     → DASH-PADDED sibling (bytes [written, span.size()) are
//       filled with '-' on success).  When PETRA_SPACE_PAD=1 is
//       also set, the DASH fill OVERWRITES the SPACE fill from
//       the Sep 28 SPACE-padded sibling (the composition rule
//       "the LATER fill overwrites the EARLIER fill").
//
// The compile-time flag tuple (PETRA_THROW_ON_OVERFLOW,
// PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD) controls the
// dispatch via
//   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
//   PETRA_SPACE_PAD × #if PETRA_DASH_PAD:
//
// The PETRA_PAD=0 half (8 cells of the (THROW, SPACE_PAD,
// DASH_PAD) tuple) is the focus of today's lesson:
//
//   (0, 0, 0, 0): expected+untouched sibling (verbatim Sep 10/Sep 19)
//   (0, 0, 0, 1): expected+DASH-padded sibling (NEW today — wraps
//                  Sep 28's SPACE-padded sibling verbatim and
//                  dash-fills [written, span.size()) with '-' on
//                  success ONLY)
//   (0, 0, 1, 0): expected+SPACE-padded sibling (verbatim Sep 28)
//   (0, 0, 1, 1): expected+SPACE+DASH-padded sibling (REUSES the
//                  (0, 0, 0, 1) DASH-padded sibling verbatim — the
//                  composition rule "the LATER fill overwrites the
//                  EARLIER fill")
//   (1, 0, 0, 0): throwing+untouched sibling (verbatim Sep 21)
//   (1, 0, 0, 1): throwing+DASH-padded sibling (NEW today — wraps
//                  Sep 28's throwing+SPACE-padded sibling verbatim
//                  and dash-fills [written, span.size()) with '-' on
//                  success ONLY; re-raises std::runtime_error
//                  untouched on error)
//   (1, 0, 1, 0): throwing+SPACE-padded sibling (verbatim Sep 28)
//   (1, 0, 1, 1): throwing+SPACE+DASH-padded sibling (REUSES the
//                  (1, 0, 0, 1) DASH-padded sibling verbatim)
//
// The PETRA_PAD=1 cells (the other 8 cells of the 4-bit tuple)
// are a #error in this lesson — see Sep 25 / Sep 26 / Sep 27 /
// Oct 1 for the NUL-pad cells.  TODAY's lesson is the
// PETRA_PAD=0 half only.
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0 (expected+untouched —
// the prior contract across Sep 6 through Oct 3).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)'
// and capture the result.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <exception>
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
#include <utility>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 /
// Sep 29 / Sep 30 / Oct 1 / Oct 3)
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
// Part 2 — petra::counting_output_iterator (verbatim from Sep 6 onwards;
// callback-based per Sep 30 / Oct 1 / Oct 3 fix)
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
// Part 3 — petra::detail::bounded_char_writer (verbatim from Sep 8 onwards;
// callback-based per Sep 30 / Oct 1 / Oct 3 fix)
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

static_assert(std::indirectly_writable<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
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
// Part 5 — petra::try_format_runtime_n (Sep 8 verbatim char*+cap) +
// petra::try_format_bounded_runtime_n (Sep 9 verbatim char*+cap) +
// petra::try_format_runtime_n(span<char>) (Sep 19 verbatim) +
// petra::try_format_bounded_runtime_n_span (Sep 10 verbatim).
//
// These are the expected+untouched siblings on the CHAR*+CAP
// axis AND the STD::SPAN<CHAR> axis.  TODAY's NEW DASH-pad
// span<char> sibling wrappers (Part 8) REUSE these wrappers
// indirectly via Sep 28's SPACE-padded span<char> siblings
// (Part 8 of the Oct 1 file).
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
    return detail::try_format_runtime_n_impl(out.data(), out.size(),
                                              fmt, args...);
}

// Sep 10 verbatim — span<char> CONSTEVAL bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_span(std::span<char> out,
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out.data(),
                                                       out.size(),
                                                       fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_runtime_n_throwing (Sep 20 verbatim char*+cap)
// + petra::try_format_bounded_runtime_n_throwing (Sep 20 verbatim
// char*+cap) + petra::try_format_runtime_n_throwing(span<char>) (Sep 21
// verbatim) + petra::try_format_bounded_runtime_n_throwing_span (Sep 21
// verbatim).
//
// These are the throwing+untouched siblings on the CHAR*+CAP axis AND
// the STD::SPAN<CHAR> axis.  TODAY's NEW throwing+DASH-pad span<char>
// sibling wrappers (Part 8) REUSE these wrappers indirectly via Sep 28's
// throwing+SPACE-padded span<char> siblings.
// ============================================================================

namespace petra {

// Sep 20 verbatim — char*+cap RUNTIME throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(char* out, std::size_t cap,
                               std::string_view fmt,
                               const Args&... args) {
    auto r = try_format_runtime_n(out, cap, fmt, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

// Sep 20 verbatim — char*+cap CONSTEVAL throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing(char* out, std::size_t cap,
                                       std::format_string<Args...> fmt,
                                       const Args&... args) {
    auto r = try_format_bounded_runtime_n(out, cap, fmt, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

// Sep 21 verbatim — span<char> RUNTIME throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(std::span<char> out, std::string_view fmt,
                               const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

// Sep 21 verbatim — span<char> CONSTEVAL throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_span(std::span<char> out,
                                            std::format_string<Args...> fmt,
                                            const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

}  // namespace petra

// ============================================================================
// Part 7 — petra::try_format_bounded_runtime_n_space_pad_span /
// petra::try_format_runtime_n_space_pad_span /
// petra::try_format_bounded_runtime_n_throwing_space_pad_span /
// petra::try_format_runtime_n_throwing_space_pad_span — SEP 28 verbatim.
//
// TODAY's NEW DASH-pad span<char> sibling wrappers (Part 8) REUSE
// these SPACE-padded span<char> siblings verbatim — the DASH-pad
// wrapper calls the SPACE-pad sibling, which fills [written,
// span.size()) with ' ', and then the DASH-pad wrapper overwrites
// those ' ' bytes with '-' on success ONLY.
//
// KEY INSIGHT: the (SPACE_PAD=1, DASH_PAD=1) cells REUSE these
// DASH-pad wrappers (NOT these SPACE-pad wrappers) verbatim because
// DASH overwrites SPACE when both are set.  See Section 1's
// "DASH × SPACE_PAD composition rule" pinning below.
// ============================================================================

namespace petra {

// Sep 28 verbatim — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_space_pad_span(std::span<char> out, std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// Sep 28 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// SPACE-PAD sibling on the span<char> axis.
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

// Sep 28 verbatim — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Sep 28 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_span(out, fmt, args...);
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
// Part 8 — petra::try_format_bounded_runtime_n_dash_pad_span (NEW today) +
// petra::try_format_runtime_n_dash_pad_span (NEW today) +
// petra::try_format_bounded_runtime_n_throwing_dash_pad_span (NEW today)
// + petra::try_format_runtime_n_throwing_dash_pad_span (NEW today).
//
// TODAY's NEW DASH-pad span<char> sibling wrappers are thin layers
// on top of Sep 28's SPACE-pad span<char> siblings.  The
// composition rule "the LATER fill overwrites the EARLIER fill"
// applies:
//
//   1. The DASH-pad wrapper calls the SPACE-pad span<char> sibling,
//      which fills bytes [written, span.size()) with ' ' on success
//      (or short-circuits on error).
//   2. On success, the DASH-pad wrapper OVERWRITES those ' ' bytes
//      with '-' (the LATER fill).
//   3. On error (Format or TooLarge), the DASH-pad wrapper does NOT
//      touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
//
// (active when PETRA_PAD=0 AND PETRA_DASH_PAD=1):
//   - (0, 0, 0, 1) expected+DASH-padded span<char> sibling (RUNTIME
//     surface): petra::try_format_runtime_n_dash_pad_span
//     (wraps Sep 28's try_format_runtime_n_space_pad_span verbatim
//      and dash-fills [written, span.size()) with '-' on success).
//   - (0, 0, 0, 1) expected+DASH-padded span<char> sibling (CONSTEVAL
//     surface): petra::try_format_bounded_runtime_n_dash_pad_span
//     (wraps Sep 28's try_format_bounded_runtime_n_space_pad_span
//      verbatim and dash-fills [written, span.size()) with '-' on
//      success).
//   - (1, 0, 0, 1) throwing+DASH-padded span<char> sibling (RUNTIME
//     surface): petra::try_format_runtime_n_throwing_dash_pad_span
//     (wraps Sep 28's try_format_runtime_n_throwing_space_pad_span
//      verbatim and dash-fills [written, span.size()) with '-' on
//      success).
//   - (1, 0, 0, 1) throwing+DASH-padded span<char> sibling (CONSTEVAL
//     surface): petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//     (wraps Sep 28's
//      try_format_bounded_runtime_n_throwing_space_pad_span verbatim
//      and dash-fills [written, span.size()) with '-' on success).
//
// (active when PETRA_PAD=0 AND PETRA_SPACE_PAD=1 AND
//  PETRA_DASH_PAD=1):
//   - (0, 0, 1, 1) expected+SPACE+DASH-padded span<char> sibling:
//     REUSES the (0, 0, 0, 1) DASH-pad span<char> sibling wrapper
//     verbatim (the SPACE fill is overwritten by the DASH fill).
//   - (1, 0, 1, 1) throwing+SPACE+DASH-padded span<char> sibling:
//     REUSES the (1, 0, 0, 1) DASH-pad span<char> sibling wrapper
//     verbatim.
// ============================================================================

namespace petra {

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_runtime_n_space_pad_span verbatim and dash-fills
// [written, span.size()) with '-' on success ONLY (the dash-fill
// loop runs only when the SPACE-pad sibling returned normally).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_dash_pad_span(std::span<char> out, std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_space_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        // LATER fill (DASH) overwrites EARLIER fill (SPACE) from
        // the SPACE-pad sibling.  On error, r.has_value() is false
        // and the loop is skipped, preserving the buffer's
        // atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '-';
        }
    }
    return r;
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_bounded_runtime_n_space_pad_span verbatim and
// dash-fills [written, span.size()) with '-' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_dash_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_space_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '-';
        }
    }
    return r;
}

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_runtime_n_throwing_space_pad_span verbatim and
// dash-fills [written, span.size()) with '-' on success ONLY
// (control does NOT reach the dash-fill loop on error — the catch
// block re-raises the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_dash_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_space_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '-';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_bounded_runtime_n_throwing_space_pad_span verbatim
// and dash-fills [written, span.size()) with '-' on success ONLY
// (control does NOT reach the dash-fill loop on error — the catch
// block re-raises the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_dash_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_space_pad_span(
                out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '-';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 9 — TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW
// (Mirrors Oct 1's / Oct 3's shape verbatim; now extended to cover
//  the 3-bit (PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD,
//  PETRA_DASH_PAD) tuple on the STD::SPAN<CHAR> axis, with
//  PETRA_PAD=0 only — the PETRA_PAD=1 cells live in Sep 27's /
//  Oct 1's lessons and are a #error in this lesson.)
//
// The compile-time flag triple controls the dispatch via
//   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_SPACE_PAD × #if
//   PETRA_DASH_PAD:
//
//   (0, 0, 0, 0): expected+untouched sibling (returns
//            expected<size_t, FormatError>; bytes [written,
//            span.size()) UNTOUCHED on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//             RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   (0, 0, 0, 1): expected+DASH-padded sibling (returns
//            expected<size_t, FormatError>; bytes [written,
//            span.size()) filled with '-' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                       (NEW today)
//             RUNTIME:   petra::try_format_runtime_n_dash_pad_span
//                       (NEW today)
//
//   (0, 0, 1, 0): expected+SPACE-padded sibling (returns
//            expected<size_t, FormatError>; bytes [written,
//            span.size()) filled with ' ' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                       (Sep 28)
//             RUNTIME:   petra::try_format_runtime_n_space_pad_span (Sep 28)
//
//   (0, 0, 1, 1): expected+SPACE+DASH-padded sibling (returns
//            expected<size_t, FormatError>; bytes [written,
//            span.size()) end up as '-' on success — DASH overwrites
//            SPACE)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                       (NEW today — REUSED verbatim; the (0, 0, 0, 1)
//                       and (0, 0, 1, 1) cells share the SAME sibling
//                       wrapper per the composition rule)
//             RUNTIME:   petra::try_format_runtime_n_dash_pad_span
//                       (NEW today — REUSED verbatim)
//
//   (1, 0, 0, 0): throwing+untouched sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) UNTOUCHED on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                       (Sep 21)
//             RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                       (Sep 21)
//
//   (1, 0, 0, 1): throwing+DASH-padded sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) filled with '-' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                       (NEW today)
//             RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                       (NEW today)
//
//   (1, 0, 1, 0): throwing+SPACE-padded sibling (returns std::size_t
//            on success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) filled with ' ' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                       (Sep 28)
//             RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                       (Sep 28)
//
//   (1, 0, 1, 1): throwing+SPACE+DASH-padded sibling (returns
//            std::size_t on success, throws std::runtime_error on
//            overflow; bytes [written, span.size()) end up as '-' on
//            success — DASH overwrites SPACE)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                       (NEW today — REUSED verbatim; the (1, 0, 0, 1)
//                       and (1, 0, 1, 1) cells share the SAME sibling
//                       wrapper per the composition rule)
//             RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                       (NEW today — REUSED verbatim)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0 (expected+untouched —
// the prior contract across Sep 6 through Oct 3).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)'
// and capture the result.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

// PETRA_PAD defaults to 0 (untouched sibling).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
#endif

// PETRA_PAD=1 is out of scope for this lesson — it lives in
// Sep 27's / Oct 1's NUL-padded span<char> lessons.
#if PETRA_PAD != 0
#error "PETRA_PAD must be 0 in this lesson — Sep 27's / Oct 1's NUL-padded span<char> siblings are out of scope.  Re-run with -DPETRA_PAD=OFF."
#endif

// PETRA_SPACE_PAD defaults to 0 (no space-pad).
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

// Sanity check: PETRA_SPACE_PAD must be 0 or 1.
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

// PETRA_DASH_PAD defaults to 0 (no dash-pad).
#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif

// Sanity check: PETRA_DASH_PAD must be 0 or 1.
#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 (no dash-pad) or 1 (dash-padded)"
#endif

// --------------------------------------------------------------------------
// Dispatch table — 3-bit tuple (PETRA_THROW_ON_OVERFLOW,
// PETRA_SPACE_PAD, PETRA_DASH_PAD) with PETRA_PAD=0 → sibling
// wrapper.  (PETRA_PAD=1 is a #error above.)
//
// HEADLINE FINDING: the (0, 0, 1, 1) and (1, 0, 1, 1) cells reuse
// the (0, 0, 0, 1) and (1, 0, 0, 1) DASH-padded span<char>
// sibling wrappers verbatim because the composition rule is
// "the LATER fill overwrites the EARLIER fill" — so when both
// SPACE_PAD=1 and DASH_PAD=1, the DASH fill overwrites the SPACE
// fill, and bytes [written, span.size()) end up as '-'.  This is
// the CLEANEST design: no NEW wrappers are needed for the
// (SPACE_PAD=1, DASH_PAD=1) cells; they just dispatch to the
// (SPACE_PAD=0, DASH_PAD=1) DASH-padded wrappers.
// --------------------------------------------------------------------------

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (0, 0, 0) → expected+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 10's try_format_bounded_runtime_n_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)               \
                                                __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 19's try_format_runtime_n(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (0, 0, 1) → expected+DASH-padded sibling path (NEW today).

// CONSTEVAL surface: dispatches to TODAY's
// try_format_bounded_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's
// try_format_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (0, 1, 0) → expected+SPACE-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_space_pad_span((out), (fmt)     \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_space_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (0, 1, 1) → expected+SPACE+DASH-padded sibling path (NEW today).
//
// KEY INSIGHT: this cell REUSES the (0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// SPACE when both are set.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to TODAY's
// try_format_bounded_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's
// try_format_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (1, 0, 0) → throwing+untouched sibling path.

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

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (1, 0, 1) → throwing+DASH-padded sibling path (NEW today).

// CONSTEVAL surface: dispatches to TODAY's
// try_format_bounded_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's
// try_format_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (1, 1, 0) → throwing+SPACE-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_space_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (1, 1, 1) → throwing+SPACE+DASH-padded sibling path (NEW today).
//
// KEY INSIGHT: this cell REUSES the (1, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// SPACE when both are set.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to TODAY's
// try_format_bounded_runtime_n_throwing_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's
// try_format_runtime_n_throwing_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_SPACE_PAD × PETRA_DASH_PAD

// ============================================================================
// Part 10 — hand-rolled SECTION/CHECK framework.
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
// Part 11 — sections
// ============================================================================

int main() {
    // ----- Section 1 — (PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD,
    //       PETRA_DASH_PAD) flag triple pinned + DASH × SPACE_PAD
    //       composition rule pinned.
    SECTION("Section 1 - flag triple pinned + composition rule");
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                      || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
    static_assert(PETRA_SPACE_PAD == 0 || PETRA_SPACE_PAD == 1,
                  "PETRA_SPACE_PAD must be 0 or 1");
    static_assert(PETRA_DASH_PAD == 0 || PETRA_DASH_PAD == 1,
                  "PETRA_DASH_PAD must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=0, "
        "PETRA_DASH_PAD=0 (expected+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=0, "
        "PETRA_DASH_PAD=1 (expected+DASH-padded sibling — NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=1, "
        "PETRA_DASH_PAD=0 (expected+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=1, "
        "PETRA_DASH_PAD=1 (expected+SPACE+DASH-padded sibling — NEW today "
        "— DASH overwrites SPACE)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=0, "
        "PETRA_DASH_PAD=0 (throwing+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=0, "
        "PETRA_DASH_PAD=1 (throwing+DASH-padded sibling — NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=1, "
        "PETRA_DASH_PAD=0 (throwing+SPACE-padded sibling)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=1, "
        "PETRA_DASH_PAD=1 (throwing+SPACE+DASH-padded sibling — NEW today "
        "— DASH overwrites SPACE)");
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

    // ----- Section 2 — Byte-state contract on success for the
    //     CURRENT (PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD,
    //     PETRA_DASH_PAD) flag triple.
    //
    // For all 8 cells (PETRA_PAD=0), the byte-state contract on
    // success is:
    //   - (?, 0, 0) → bytes [written, span.size()) are UNTOUCHED ('Q').
    //   - (?, 0, 1) → bytes [written, span.size()) are '-' (DASH).
    //   - (?, 1, 0) → bytes [written, span.size()) are ' ' (SPACE).
    //   - (?, 1, 1) → bytes [written, span.size()) are '-' (DASH
    //     overwrites SPACE — composition rule).
    SECTION("Section 2 - Byte-state contract on success");
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
        // Verify the suffix per the byte-state contract.
#if PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
        // UNTOUCHED path: bytes [written, span.size()) are 'Q'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#elif PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
        // DASH-PADDED path: bytes [written, span.size()) are '-'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '-');
        }
#elif PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
        // SPACE-PADDED path: bytes [written, span.size()) are ' '.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], ' ');
        }
#else
        // SPACE+DASH-PADDED path: bytes [written, span.size()) are '-'
        // (DASH overwrites SPACE).
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '-');
        }
#endif
    }

    // 2a. Same checks but with a sub-span (the "no-bleed" case).
    //     A 6-byte sub-span at offset 5 of a 16-byte backing buffer
    //     with format 'ABCDE' (5 bytes) leaves bytes [0,5) and
    //     [11,16) as poison 'Q' regardless of the flag triple — the
    //     pad loop only touches bytes INSIDE the span.
    SECTION("Section 2a - Sub-span no-bleed on DASH x SPACE path");
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
#if PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
        CHECK_EQ(sub[5], 'Q');   // untouched
#elif PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
        CHECK_EQ(sub[5], '-');   // DASH-padded
#elif PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
        CHECK_EQ(sub[5], ' ');   // SPACE-padded
#else
        CHECK_EQ(sub[5], '-');   // composition: DASH overwrites SPACE
#endif
        // Outside the span — must be UNTOUCHED on ALL paths.
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
    }

    // 2c. cap=0 edge case: the 'for (i = written; i < cap; ++i)'
    //     loop has i < cap == 0 as the initial condition, so the
    //     loop body never executes.
    SECTION("Section 2c - cap=0 edge case on DASH x SPACE path");
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
    SECTION("Section 2d - written==cap edge case on DASH x SPACE path");
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
    //     On all 8 cells, the buffer must be UNTOUCHED on overflow.
    SECTION("Section 3 - Buffer atomicity on TooLarge");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "this is way too long to fit");
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
#else
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
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "value is 42"[i]);
        }
    }

    // ----- Section 6 — CONSTEVAL gate pinned.
    SECTION("Section 6 - CONSTEVAL gate requires literal fmt");
    {
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
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "no args");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 7u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "no args");
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
        std::size_t expected_written = 21;  // "x=42, y=hello, z=3.14"
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
    //       decltype checks on all 8 paths.
    SECTION("Section 14 - Perpendicular-axis decltype checks");
    {
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
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
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
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
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
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
                      "(0,1,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
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
                      "(0,1,1) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,1) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
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
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
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
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,1,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,0) RUNTIME must return std::size_t");
#else  // (1, 1, 1) — NEW today
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,1,1) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,1) RUNTIME must return std::size_t");
#endif
        CHECK(true);
    }

    // ----- Section 15 — std::format_error dynamic_cast check.
    SECTION("Section 15 - std::format_error dynamic_cast check");
    {
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
        CHECK(true);
#endif
    }

    // ----- Section 16 — DASH-pad byte-state distinctness check.
    //     When PETRA_DASH_PAD=1, bytes [written, span.size()) are
    //     '-' (NOT ' ', NOT '\0', NOT 'Q').  When PETRA_DASH_PAD=0
    //     and PETRA_SPACE_PAD=1, bytes are ' '.  When BOTH flags
    //     are set, the composition rule says DASH overwrites
    //     SPACE → bytes are '-'.
    SECTION("Section 16 - DASH-pad byte-state distinctness");
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
#if PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
        // UNTOUCHED path: bytes [4, 8) must be 'Q'.
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#elif PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
        // DASH-padded path: bytes [4, 8) must be '-' (NOT ' ',
        // NOT '\0', NOT 'Q').
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], '-');
            CHECK(buf[i] != ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != 'Q');
        }
#elif PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
        // SPACE-padded path: bytes [4, 8) must be ' ' (NOT '-',
        // NOT '\0').
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], ' ');
            CHECK(buf[i] != '-');
            CHECK(buf[i] != '\0');
        }
#else  // (?, 1, 1) — composition: DASH overwrites SPACE.
        // bytes [4, 8) must be '-' (DASH overwrites SPACE).
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], '-');
            CHECK(buf[i] != ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != 'Q');
        }
#endif
    }

    // ----- Section 17 — DASH-pad sub-span no-bleed check.
    //     A 6-byte sub-span at offset 5 of a 16-byte backing
    //     buffer with format 'ABCDE' (5 bytes) leaves bytes
    //     [0,5) and [11,16) as poison 'Q' regardless of the flag
    //     triple.  The pad loop only touches bytes INSIDE the
    //     span.
    SECTION("Section 17 - DASH-pad sub-span no-bleed");
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
                "FAIL: Section 17 - unexpected throw on success\n");
        }
#endif
        // Inside the span, prefix.
        CHECK_EQ(sub[0], 'A');
        CHECK_EQ(sub[1], 'B');
        CHECK_EQ(sub[2], 'C');
        CHECK_EQ(sub[3], 'D');
        CHECK_EQ(sub[4], 'E');
        // Inside the span, last byte (index 5 of the sub-span,
        // which is index 10 of the backing buffer).
#if PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
        CHECK_EQ(sub[5], 'Q');
#elif PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
        CHECK_EQ(sub[5], '-');
#elif PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
        CHECK_EQ(sub[5], ' ');
#else
        CHECK_EQ(sub[5], '-');   // DASH overwrites SPACE
#endif
        // Outside the span — must be UNTOUCHED on ALL paths.
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
    }

    std::println("===================");
    std::println("PASS: {}  FAIL: {}", g_pass, g_fail);
    std::println("===================");
    if (g_fail != 0) {
        std::println("OVERALL: FAIL");
        return 1;
    }
    std::println("OVERALL: PASS");
    return 0;
}
