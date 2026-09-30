// P-2026-09-30 — try_format_padded_space_pad_n_or_throw:
//
// Closes Sep 29's "Where we go next" item (1) verbatim — the
// PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells on the CHAR*+CAP ×
// BOUNDED-BUFFER axis (the composition of Sep 26's NUL-padded axis
// with Sep 29's SPACE-padded axis).  TODAY extends the SPACE-PAD
// axis (axis 6) to the NUL-padded sibling (axis 5) on the
// char*+cap side.
//
// The new compile-time flag pair is (PETRA_PAD, PETRA_SPACE_PAD) =
// (1, 1).  When BOTH are set, the natural composition rule is:
// "the LATER fill overwrites the EARLIER fill" — so the SPACE fill
// (axis 6) overwrites the NUL fill (axis 5), and bytes [written,
// cap) end up as ' ' on success.
//
// The compile-time flag triple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD) controls the dispatch via #if PETRA_THROW_ON_OVERFLOW
// × #if PETRA_PAD × #if PETRA_SPACE_PAD:
//
//   - (0,0,0): expected+untouched sibling (returns expected<size_t,
//     FormatError>; bytes [written, cap) UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//      RUNTIME:   petra::try_format_runtime_n (Sep 8)
//
//   - (0,1,0): expected+NUL-padded sibling (returns expected<size_t,
//     FormatError>; bytes [written, cap) zero-filled to '\0' on
//     success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_padded (Sep 25)
//      RUNTIME:   petra::try_format_runtime_n_padded (Sep 25)
//
//   - (0,0,1): expected+SPACE-padded sibling (returns expected<size_t,
//     FormatError>; bytes [written, cap) filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad (Sep 29)
//      RUNTIME:   petra::try_format_runtime_n_space_pad (Sep 29)
//
//   - (1,0,0): throwing+untouched sibling (returns std::size_t on
//     success, throws std::runtime_error on overflow; bytes
//     [written, cap) UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing (Sep 20)
//      RUNTIME:   petra::try_format_runtime_n_throwing (Sep 20)
//
//   - (1,1,0): throwing+NUL-padded sibling (returns std::size_t on
//     success, throws std::runtime_error on overflow; bytes
//     [written, cap) zero-filled to '\0' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_padded_throwing
//                (Sep 26)
//      RUNTIME:   petra::try_format_runtime_n_padded_throwing (Sep 26)
//
//   - (1,0,1): throwing+SPACE-padded sibling (NEW in Sep 29; returns
//     std::size_t on success, throws std::runtime_error on overflow;
//     bytes [written, cap) filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                (Sep 29)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad (Sep 29)
//
//   - (0,1,1): expected+NUL+SPACE-padded sibling (NEW today; returns
//     expected<size_t, FormatError>; bytes [written, cap) filled
//     with ' ' on success — the SPACE fill OVERWRITES the NUL fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad (Sep 29,
//                reused; the (0,0,1) and (0,1,1) cells share the SAME
//                sibling wrapper because SPACE overwrites NUL when
//                both are set — see Section 1's "PAD × SPACE_PAD
//                composition rule" pinning).
//      RUNTIME:   petra::try_format_runtime_n_space_pad (Sep 29, reused).
//
//   - (1,1,1): throwing+NUL+SPACE-padded sibling (NEW today; returns
//     std::size_t on success, throws std::runtime_error on overflow;
//     bytes [written, cap) filled with ' ' on success — the SPACE
//     fill OVERWRITES the NUL fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                (Sep 29, reused; the (1,0,1) and (1,1,1) cells share
//                the SAME sibling wrapper because SPACE overwrites
//                NUL when both are set).
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad
//                (Sep 29, reused).
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0 (expected+untouched — the prior contract
// across Sep 6 through Sep 29).
//
// KEY DESIGN DECISION — the (PAD=1, SPACE_PAD=1) cells reuse the
// Sep 29 (PAD=0, SPACE_PAD=1) sibling wrappers verbatim.  This is
// because the (PAD=1, SPACE_PAD=1) cells are functionally
// equivalent to the (PAD=0, SPACE_PAD=1) cells: when both PAD and
// SPACE_PAD are 1, the LATER fill (SPACE) overwrites the EARLIER
// fill (NUL).  The end state on success is bytes [written, cap) =
// ' '.  So the (PAD=1, SPACE_PAD=1) cells do NOT need their own
// NEW wrappers — they just reuse Sep 29's SPACE-padded siblings.
//
// SCOPE NOTE: This lesson is FOCUSED on the PAD × SPACE_PAD
// composition ONLY.  Sep 29's PETRA_PAD = 0 (untouched) branches
// require Sep 29's NEW SPACE-pad wrappers (which ARE included in
// this file verbatim).  When PETRA_PAD = 0 AND PETRA_SPACE_PAD =
// 0, the lesson's dispatcher works exactly like Sep 29's default
// (untouched) sibling.  When PETRA_PAD = 0 AND PETRA_SPACE_PAD =
// 1, the lesson's dispatcher works exactly like Sep 29's
// (PAD=0, SPACE_PAD=1) sibling.  When PETRA_PAD = 1 AND
// PETRA_SPACE_PAD = 0, the lesson's dispatcher works exactly like
// Sep 26's (PAD=1, SPACE_PAD=0) sibling.

#include <array>
#include <atomic>
#include <cstdio>
#include <cstddef>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 /
// Sep 29)
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
// Part 5 — petra::try_format_runtime_n (Sep 8 verbatim) +
// petra::try_format_bounded_runtime_n (Sep 9 verbatim) +
// petra::try_format_runtime_n_padded (Sep 25 verbatim) +
// petra::try_format_bounded_runtime_n_padded (Sep 25 verbatim).
// ============================================================================

namespace petra {

// Sep 8 verbatim — RUNTIME expected+untouched sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap,
                     std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 9 verbatim — CONSTEVAL expected+untouched sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 25 verbatim — RUNTIME expected+NUL-padded sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded(char* out, std::size_t cap,
                            std::string_view fmt,
                            const Args&... args) {
    auto r = try_format_runtime_n(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// Sep 25 verbatim — CONSTEVAL expected+NUL-padded sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded(char* out, std::size_t cap,
                                     std::format_string<Args...> fmt,
                                     const Args&... args) {
    auto r = try_format_bounded_runtime_n(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// ============================================================================
// Part 6 — petra::try_format_runtime_n_throwing (Sep 20 verbatim) +
// petra::try_format_bounded_runtime_n_throwing (Sep 20 verbatim) +
// petra::try_format_runtime_n_padded_throwing (Sep 26 verbatim) +
// petra::try_format_bounded_runtime_n_padded_throwing (Sep 26 verbatim).
// ============================================================================

// Sep 20 verbatim — RUNTIME throwing+untouched sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(char* buf, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(buf, cap, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

// Sep 20 verbatim — CONSTEVAL throwing+untouched sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing(char* buf, std::size_t cap,
                                       std::format_string<Args...> fmt,
                                       const Args&... args) {
    auto r = try_format_bounded_runtime_n(buf, cap, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

// Sep 26 verbatim — RUNTIME throwing+NUL-padded sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_padded_throwing(char* out, std::size_t cap,
                                     std::string_view fmt,
                                     const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, cap, fmt, args...);
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '\0';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Sep 26 verbatim — CONSTEVAL throwing+NUL-padded sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_padded_throwing(char* out, std::size_t cap,
                                              std::format_string<Args...> fmt,
                                              const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing(out, cap, fmt, args...);
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '\0';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// ============================================================================
// Part 7 — petra::try_format_bounded_runtime_n_space_pad /
// petra::try_format_runtime_n_space_pad /
// petra::try_format_bounded_runtime_n_throwing_space_pad /
// petra::try_format_runtime_n_throwing_space_pad (Sep 29 verbatim).
//
// KEY INSIGHT: the (PAD=1, SPACE_PAD=1) cells REUSE these
// wrappers verbatim because SPACE overwrites NUL when both are
// set.  See Section 1's "PAD × SPACE_PAD composition rule" pinning
// below.
// ============================================================================

// Sep 29 verbatim — RUNTIME surface of the BOUNDED-BUFFER ×
// EXPECTED × SPACE-PAD sibling on the char*+cap axis.  Wraps
// Sep 8's try_format_runtime_n verbatim and space-fills
// [written, cap) with ' ' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_space_pad(char* out, std::size_t cap,
                                std::string_view fmt,
                                const Args&... args) {
    auto r = try_format_runtime_n(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// Sep 29 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER ×
// EXPECTED × SPACE-PAD sibling on the char*+cap axis.  Wraps
// Sep 9's try_format_bounded_runtime_n verbatim and space-fills
// [written, cap) with ' ' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_space_pad(char* out, std::size_t cap,
                                         std::format_string<Args...> fmt,
                                         const Args&... args) {
    auto r = try_format_bounded_runtime_n(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// Sep 29 verbatim — RUNTIME surface of the BOUNDED-BUFFER ×
// THROWING × SPACE-PAD sibling on the char*+cap axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_space_pad(char* out, std::size_t cap,
                                         std::string_view fmt,
                                         const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, cap, fmt, args...);
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Sep 29 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER ×
// THROWING × SPACE-PAD sibling on the char*+cap axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_space_pad(
    char* out, std::size_t cap,
    std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing(out, cap, fmt, args...);
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
// Part 8 — TRY_FORMAT_BOUNDED_N_PAD_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW
// (Mirrors Sep 29's shape verbatim; now extended to cover ALL 8 cells
//  on the 3-bit flag tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
//  PETRA_SPACE_PAD).)
//
// The compile-time flag triple controls the dispatch via
//   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if PETRA_SPACE_PAD:
//
//   (0,0,0): expected+untouched sibling (returns expected<size_t,
//            FormatError>; bytes [written, cap) UNTOUCHED on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//             RUNTIME:   petra::try_format_runtime_n (Sep 8)
//
//   (0,1,0): expected+NUL-padded sibling (returns expected<size_t,
//            FormatError>; bytes [written, cap) zero-filled to '\0'
//            on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_padded (Sep 25)
//             RUNTIME:   petra::try_format_runtime_n_padded (Sep 25)
//
//   (0,0,1): expected+SPACE-padded sibling (returns expected<size_t,
//            FormatError>; bytes [written, cap) filled with ' ' on
//            success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad
//                       (Sep 29)
//             RUNTIME:   petra::try_format_runtime_n_space_pad (Sep 29)
//
//   (0,1,1): expected+NUL+SPACE-padded sibling (NEW today — REUSES
//            the (0,0,1) SPACE-padded sibling because SPACE overwrites
//            NUL when both are set; bytes [written, cap) end up as
//            ' ' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad
//                       (Sep 29 — reused verbatim; the (0,0,1) and
//                       (0,1,1) cells share the SAME sibling wrapper)
//             RUNTIME:   petra::try_format_runtime_n_space_pad (Sep 29
//                       — reused verbatim)
//
//   (1,0,0): throwing+untouched sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, cap) UNTOUCHED on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing
//                       (Sep 20)
//             RUNTIME:   petra::try_format_runtime_n_throwing (Sep 20)
//
//   (1,1,0): throwing+NUL-padded sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, cap) zero-filled to '\0' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_padded_throwing
//                       (Sep 26)
//             RUNTIME:   petra::try_format_runtime_n_padded_throwing
//                       (Sep 26)
//
//   (1,0,1): throwing+SPACE-padded sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, cap) filled with ' ' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                       (Sep 29)
//             RUNTIME:   petra::try_format_runtime_n_throwing_space_pad
//                       (Sep 29)
//
//   (1,1,1): throwing+NUL+SPACE-padded sibling (NEW today — REUSES
//            the (1,0,1) SPACE-padded sibling because SPACE overwrites
//            NUL when both are set; bytes [written, cap) end up as
//            ' ' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                       (Sep 29 — reused verbatim; the (1,0,1) and
//                       (1,1,1) cells share the SAME sibling wrapper)
//             RUNTIME:   petra::try_format_runtime_n_throwing_space_pad
//                       (Sep 29 — reused verbatim)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0 (expected+untouched — the prior contract
// across Sep 6 through Sep 29).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(...)'
// and capture the result.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.
//
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

// PETRA_SPACE_PAD defaults to 0 (no space-pad).
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

// Sanity check: PETRA_SPACE_PAD must be 0 or 1.
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

// --------------------------------------------------------------------------
// Dispatch table — 3-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD) → sibling wrapper.
//
// HEADLINE FINDING: the (0,1,1) and (1,1,1) cells reuse the (0,0,1)
// and (1,0,1) SPACE-padded sibling wrappers verbatim because the
// composition rule is "the LATER fill overwrites the EARLIER fill" —
// so when both PAD=1 and SPACE_PAD=1, the SPACE fill overwrites
// the NUL fill, and bytes [written, cap) end up as ' '.  This is
// the CLEANEST design: no NEW wrappers are needed for the (1,1)
// cells; they just dispatch to the (0,1) SPACE-padded wrappers.
// --------------------------------------------------------------------------

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
// (0, 0, 0) → expected+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 9's try_format_bounded_runtime_n.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)             \
                                           __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 8's try_format_runtime_n.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                     \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
// (0, 1, 0) → expected+NUL-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 25's
// try_format_bounded_runtime_n_padded.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_padded((out), (cap), (fmt)      \
                                                  __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 25's try_format_runtime_n_padded.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_padded((out), (cap), (fmt)              \
                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
// (0, 0, 1) → expected+SPACE-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 29's
// try_format_bounded_runtime_n_space_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_space_pad((out), (cap), (fmt)    \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 29's
// try_format_runtime_n_space_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_space_pad((out), (cap), (fmt)            \
                                             __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 1
// (0, 1, 1) → expected+NUL+SPACE-padded sibling path (NEW today).
//
// KEY INSIGHT: this cell REUSES the (0, 0, 1) SPACE-padded
// sibling wrappers verbatim because SPACE overwrites NUL when
// both are set.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to Sep 29's
// try_format_bounded_runtime_n_space_pad (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_space_pad((out), (cap), (fmt)    \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 29's
// try_format_runtime_n_space_pad (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_space_pad((out), (cap), (fmt)            \
                                             __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
// (1, 0, 0) → throwing+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 20's
// try_format_bounded_runtime_n_throwing.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing((out), (cap), (fmt)    \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 20's
// try_format_runtime_n_throwing.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
// (1, 1, 0) → throwing+NUL-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 26's
// try_format_bounded_runtime_n_padded_throwing.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_padded_throwing(                 \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 26's
// try_format_runtime_n_padded_throwing.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_padded_throwing((out), (cap), (fmt)      \
                                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
// (1, 0, 1) → throwing+SPACE-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 29's
// try_format_bounded_runtime_n_throwing_space_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad(             \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 29's
// try_format_runtime_n_throwing_space_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_space_pad((out), (cap), (fmt)   \
                                                      __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 1
// (1, 1, 1) → throwing+NUL+SPACE-padded sibling path (NEW today).
//
// KEY INSIGHT: this cell REUSES the (1, 0, 1) SPACE-padded
// sibling wrappers verbatim because SPACE overwrites NUL when
// both are set.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to Sep 29's
// try_format_bounded_runtime_n_throwing_space_pad (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad(             \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 29's
// try_format_runtime_n_throwing_space_pad (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_space_pad((out), (cap), (fmt)   \
                                                      __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_PAD × PETRA_SPACE_PAD

// ============================================================================
// Part 9 — hand-rolled SECTION/CHECK framework.
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
    //       PETRA_SPACE_PAD) flag triple pinned + PAD × SPACE_PAD
    //       composition rule pinned.
    SECTION("Section 1 - flag triple pinned + composition rule");
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
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0 (expected+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1 (expected+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1 (expected+NUL+SPACE-padded sibling — NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0 (throwing+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0 (throwing+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1 (throwing+SPACE-padded sibling)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1 (throwing+NUL+SPACE-padded sibling — NEW today)");
#endif
    CHECK(true);

    // 1c. Pin std::runtime_error / std::format_error relationship.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // ----- Section 2 — Byte-state contract on success for the
    //     CURRENT (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    //     PETRA_SPACE_PAD) flag triple.
    //
    // For all 8 cells, the byte-state contract on success is:
    //   - (?, 0, 0) → bytes [written, cap) are UNTOUCHED ('Q').
    //   - (?, 1, 0) → bytes [written, cap) are '\0' (NUL).
    //   - (?, 0, 1) → bytes [written, cap) are ' ' (SPACE).
    //   - (?, 1, 1) → bytes [written, cap) are ' ' (SPACE overwrites NUL).
    SECTION("Section 2 - Byte-state contract on success");
    {
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;  // "ABCDE"
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "ABCDE");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "ABCDE");
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
#if PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        // UNTOUCHED path: bytes [written, cap) are 'Q'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#elif PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
        // NUL-PADDED path: bytes [written, cap) are '\0'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '\0');
        }
#else
        // SPACE-PADDED path ((0,1) or (1,1) — SPACE overwrites NUL
        // when both are set): bytes [written, cap) are ' '.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], ' ');
        }
#endif
    }

    // 2c. cap=0 edge case on the SPACE-padded / NUL+SPACE-padded
    //     path: the 'for (i = written; i < cap; ++i)' loop has i <
    //     cap == 0 as the initial condition, so the loop body never
    //     executes.
    SECTION("Section 2c - cap=0 edge case");
    {
        char single_buf[1] = {'Q'};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(single_buf, 0, "");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 0u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(single_buf, 0, "");
            CHECK_EQ(n, 0u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2c - unexpected throw on cap=0+empty-fmt\n");
        }
#endif
        // The sentinel byte OUTSIDE the cap=0 writable region remains
        // untouched.
        CHECK_EQ(single_buf[0], 'Q');
    }

    // 2d. written==cap edge case: format 'ABCDEFGH' (8 bytes) into
    //     cap=8.  On any padding path, the 'for (i = written; i <
    //     cap; ++i)' loop has i == cap as the initial condition, so
    //     the loop body never executes.
    SECTION("Section 2d - written==cap edge case");
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "ABCDEFGH");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 8u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "ABCDEFGH");
            CHECK_EQ(n, 8u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2d - unexpected throw on full-buffer\n");
        }
#endif
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], "ABCDEFGH"[i]);
        }
    }

    // ----- Section 3 — Buffer atomicity on TooLarge.  On all eight
    //       flag triples, the buffer must be UNTOUCHED on overflow.
    SECTION("Section 3 - Buffer atomicity on TooLarge");
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        // Expected sibling path: r.error() contains FormatError.
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            buf, cap, "this is way too long to fit");
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
#else
        // Throwing sibling path: throws std::runtime_error.
        bool caught = false;
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
                buf, cap, "this is way too long to fit");
            (void)n;
        } catch (const std::runtime_error&) {
            caught = true;
        } catch (...) {
            // unexpected
        }
        CHECK(caught);
#endif
        // Buffer must be UNTOUCHED on overflow.
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
    }

    // ----- Section 4 — Byte-count contract on success.
    SECTION("Section 4 - Byte-count contract on success");
    {
        char buf[32];
        std::size_t cap = 32;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
        std::size_t expected_written = 13;  // "Hello, world!"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            buf, cap, "Hello, {}!", "world");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
                buf, cap, "Hello, {}!", "world");
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
        char buf[32];
        std::size_t cap = 32;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
        std::size_t expected_written = 11;  // "value is 42"
#if PETRA_THROW_ON_OVERFLOW == 0
        std::string_view fmt = "value is {}";
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, fmt, 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            std::string_view fmt = "value is {}";
            auto n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, fmt, 42);
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

    // ----- Section 6 — CONSTEVAL gate pinned.
    SECTION("Section 6 - CONSTEVAL gate requires literal fmt");
    {
#if PETRA_THROW_ON_OVERFLOW == 0
        using F1 = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using ExpectedFmt1 = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<F1, ExpectedFmt1>,
                      "CONSTEVAL macro must return expected<size_t, "
                      "FormatError> on the expected path");
#else
        using F1 = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<F1, std::size_t>,
                      "CONSTEVAL macro must return std::size_t on the "
                      "throwing path");
#endif
        CHECK(true);
    }

    // ----- Section 7 — RUNTIME macro accepts std::string_view.
    SECTION("Section 7 - RUNTIME macro accepts std::string_view");
    {
#if PETRA_THROW_ON_OVERFLOW == 0
        using F1 = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedFmt1 = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<F1, ExpectedFmt1>,
                      "RUNTIME macro must return expected<size_t, "
                      "FormatError> on the expected path");
#else
        using F1 = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
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
        char buf[16];
        std::size_t cap = 16;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
        std::string bad_fmt = "bad {";  // unterminated brace
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, std::string_view{bad_fmt});
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
#else
        bool caught = false;
        try {
            auto n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, std::string_view{bad_fmt});
            (void)n;
        } catch (...) {
            caught = true;
        }
        CHECK(caught);
#endif
        // Buffer must be UNTOUCHED on Format error (no byte at any
        // index was written).
        for (std::size_t i = 0; i < cap; ++i) {
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

    // ----- Section 10 — Concurrency: 4 threads × 25 concurrent calls.
    SECTION("Section 10 - Concurrency (4 threads x 25 calls)");
    {
        std::atomic<int> a_pass{0};
        std::atomic<int> a_fail{0};
        auto worker = [&a_pass, &a_fail]() {
            int lp = 0, lf = 0;
            char buf[32];
            for (int i = 0; i < 25; ++i) {
                for (std::size_t j = 0; j < 32; ++j) buf[j] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
                auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
                    buf, 32, "iteration {}", i);
                if (r.has_value() && r.value() > 0u) {
                    ++lp;
                } else {
                    ++lf;
                }
#else
                try {
                    auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
                        buf, 32, "iteration {}", i);
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
        char buf[16];
        std::size_t cap = 16;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "no args");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 7u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "no args");
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
        char buf[64];
        std::size_t cap = 64;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
        // "x=42, y=hello, z=3.14" -> 21 bytes
        std::size_t expected_written = 21;
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            buf, cap, "x={}, y={}, z={:.2f}", 42, "hello", 3.14);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
                buf, cap, "x={}, y={}, z={:.2f}", 42, "hello", 3.14);
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

    // ----- Section 13 — Macro is a parenthesized expression.
    SECTION("Section 13 - Macro returns parenthesized expression");
    {
        char buf[32];
        std::size_t cap = 32;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "5");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 1u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "5");
            CHECK_EQ(n, 1u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 13 - unexpected throw on success\n");
        }
#endif
    }

    // ----- Section 14 — Perpendicular-axis claim pinned via decltype
    //     checks on ALL EIGHT flag triples.
    SECTION("Section 14 - Perpendicular-axis decltype checks");
    {
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "(0,0,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "(0,1,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "(0,0,1) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,1) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 1
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "(0,1,1) CONSTEVAL must return expected<size_t, "
                      "FormatError> — reuses (0,0,1) SPACE-padded "
                      "sibling verbatim because SPACE overwrites NUL");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,1) RUNTIME must return expected<size_t, "
                      "FormatError> — reuses (0,0,1) SPACE-padded "
                      "sibling verbatim");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,0,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,0) RUNTIME must return std::size_t");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,1,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,0) RUNTIME must return std::size_t");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 && PETRA_SPACE_PAD == 1
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,0,1) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,1) RUNTIME must return std::size_t");
#else  // (1, 1, 1)
        using RA = decltype(TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<RA, std::size_t>,
                      "(1,1,1) CONSTEVAL must return std::size_t — "
                      "reuses (1,0,1) SPACE-padded sibling verbatim "
                      "because SPACE overwrites NUL");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,1) RUNTIME must return std::size_t — "
                      "reuses (1,0,1) SPACE-padded sibling verbatim");
#endif
        CHECK(true);
    }

    // ----- Section 15 — std::format_error dynamic_cast check.
    SECTION("Section 15 - std::format_error dynamic_cast check");
    {
        char buf[16];
        std::size_t cap = 16;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 1
        bool caught_runtime = false;
        bool dynamic_cast_is_nullptr = false;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, std::string_view{"bad {"});
        } catch (const std::runtime_error& e) {
            caught_runtime = true;
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

    // ----- Section 16 — NEW today: PAD × SPACE_PAD composition
    //     rule pinned on the (1, 1, 1) and (0, 1, 1) cells.  On
    //     these cells, bytes [written, cap) MUST be ' ' on success
    //     (NOT '\0' and NOT 'Q').  This pins that SPACE overwrites
    //     NUL when both flags are set.
    SECTION("Section 16 - SPACE overwrites NUL when PAD=1 & SPACE_PAD=1");
    {
#if (PETRA_PAD == 1 && PETRA_SPACE_PAD == 1)
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;  // "ABCDE"
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "ABCDE");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf, cap, "ABCDE");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 16 - unexpected throw on success\n");
        }
#endif
        // 8-CHECK block: bytes [5, 13) are all ' ' (NOT '\0', NOT 'Q').
        CHECK_EQ(buf[5],  ' ');
        CHECK_EQ(buf[6],  ' ');
        CHECK_EQ(buf[7],  ' ');
        CHECK_EQ(buf[8],  ' ');
        CHECK_EQ(buf[9],  ' ');
        CHECK_EQ(buf[10], ' ');
        CHECK_EQ(buf[11], ' ');
        CHECK_EQ(buf[12], ' ');
        // And NOT '\0' (explicit disambiguation from NUL-padded sibling).
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK(buf[i] != '\0');
        }
        // And NOT 'Q' (explicit disambiguation from untouched sibling).
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK(buf[i] != 'Q');
        }
#else
        // Out of scope for cells where PETRA_PAD != 1 OR PETRA_SPACE_PAD != 1.
        CHECK(true);
#endif
    }

    // ----- Section 17 — NEW today: (PAD=1, SPACE_PAD=1) vs
    //     (PAD=0, SPACE_PAD=1) are FUNCTIONALLY EQUIVALENT.  Both
    //     produce bytes [written, cap) = ' ' on success.  This
    //     section pins that the (0, 1, 1) cell dispatch invokes
    //     the same wrapper body as the (0, 0, 1) cell dispatch
    //     (the MACRO definition literally expands to the same
    //     function call — pinned here at the (1, 1, 1) cell by
    //     running a second-format operation and asserting bytes
    //     [written, cap) are ' ').
    SECTION("Section 17 - composition rule on (PAD=1, SPACE_PAD=1) is functional");
    {
#if PETRA_PAD == 1 && PETRA_SPACE_PAD == 1
        // Run a SECOND format after the Section 16 format.  This
        // pins that the byte-state contract (bytes [written, cap)
        // are ' ' on success) is reproducible across multiple
        // invocations of the (1, 1, 1) macro.
        char buf2[16];
        std::size_t cap2 = 16;
        for (std::size_t i = 0; i < cap2; ++i) buf2[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r2 = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf2, cap2, "HI");
        CHECK(r2.has_value());
        CHECK_EQ(r2.value(), 2u);
#else
        try {
            auto n2 = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(buf2, cap2, "HI");
            CHECK_EQ(n2, 2u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 17 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        CHECK_EQ(buf2[0], 'H');
        CHECK_EQ(buf2[1], 'I');
        // Verify the suffix: bytes [2, 16) are ' ' (the (1, 1, 1)
        // cell's composition rule pins SPACE over NUL).
        for (std::size_t i = 2; i < cap2; ++i) {
            CHECK_EQ(buf2[i], ' ');
        }
#else
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
