// P-2026-10-08 — try_format_padded_dash_pad_n_or_throw:
//
// Closes Oct 5's "Where we go next" item (1) verbatim — the
// PETRA_PAD = 1 × PETRA_DASH_PAD = 1 cells on the CHAR*+CAP ×
// BOUNDED-BUFFER axis (the COMPOSITION of Sep 25's / Sep 26's
// NUL-padded axis with Oct 3's DASH-padded axis).  TODAY extends
// the SEVENTH axis (PETRA_DASH_PAD) to the NUL-padded sibling
// (FIFTH axis) on the char*+cap side.  The matrix is now COMPLETE
// for ALL 16 cells of the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD, PETRA_DASH_PAD) 4-bit tuple on the char*+cap
// side.
//
// The new compile-time flag quadruple
// (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
// PETRA_DASH_PAD) controls the dispatch via #if
// PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
// PETRA_SPACE_PAD × #if PETRA_DASH_PAD:
//
//   - (0,0,0,0): expected+untouched sibling (returns
//     expected<size_t, FormatError>; bytes [written, cap)
//     UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//      RUNTIME:   petra::try_format_runtime_n (Sep 8)
//
//   - (0,1,0,0): expected+NUL-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, cap) zero-
//     filled with '\0' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_padded
//                (Sep 25)
//      RUNTIME:   petra::try_format_runtime_n_padded (Sep 25)
//
//   - (0,0,1,0): expected+SPACE-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, cap) filled
//     with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad
//                (Sep 29)
//      RUNTIME:   petra::try_format_runtime_n_space_pad (Sep 29)
//
//   - (0,0,0,1): expected+DASH-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, cap) filled
//     with '-' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad
//                (Oct 3)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad (Oct 3)
//
//   - (0,0,1,1): expected+SPACE+DASH-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, cap) filled
//     with '-' on success — the DASH fill OVERWRITES the SPACE
//     fill, so the (0,0,1,1) cell REUSES the (0,0,0,1) DASH-pad
//     sibling wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad
//                (Oct 3, REUSED across the (0,0,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad
//                (Oct 3, REUSED across the (0,0,*,1) cells)
//
//   - (0,1,1,0): expected+NUL+SPACE-padded sibling (NEW today —
//     the (0,1,1,0) cell REUSES the (0,0,1,0) SPACE-padded
//     sibling verbatim per Sep 30's "SPACE overwrites NUL"
//     composition rule):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad
//                (Sep 29, REUSED across the (0,*,1,0) cells)
//      RUNTIME:   petra::try_format_runtime_n_space_pad
//                (Sep 29, REUSED across the (0,*,1,0) cells)
//
//   - (0,1,0,1): expected+NUL+DASH-padded sibling (NEW today —
//     the (0,1,0,1) cell REUSES the (0,0,0,1) DASH-padded
//     sibling verbatim per TODAY's transitive composition rule
//     "DASH overwrites NUL when both PAD and DASH_PAD are set"):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad
//                (Oct 3, REUSED across the (0,*,0,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad
//                (Oct 3, REUSED across the (0,*,0,1) cells)
//
//   - (0,1,1,1): expected+NUL+SPACE+DASH-padded sibling (NEW
//     today — the (0,1,1,1) cell REUSES the (0,0,0,1) DASH-
//     padded sibling verbatim per TODAY's transitive composition
//     rule "DASH overwrites SPACE overwrites NUL when all three
//     are set"):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad
//                (Oct 3, REUSED across the (0,*,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad
//                (Oct 3, REUSED across the (0,*,*,1) cells)
//
//   - (1,0,0,0): throwing+untouched sibling (returns std::size_t
//     on success, throws std::runtime_error on overflow; bytes
//     [written, cap) UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing
//                (Sep 20)
//      RUNTIME:   petra::try_format_runtime_n_throwing (Sep 20)
//
//   - (1,1,0,0): throwing+NUL-padded sibling (returns std::size_t
//     on success, throws std::runtime_error on overflow; bytes
//     [written, cap) zero-filled with '\0' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_padded_throwing
//                (Sep 26)
//      RUNTIME:   petra::try_format_runtime_n_padded_throwing
//                (Sep 26)
//
//   - (1,0,1,0): throwing+SPACE-padded sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, cap) filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                (Sep 29)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad
//                (Sep 29)
//
//   - (1,0,0,1): throwing+DASH-padded sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, cap) filled with '-' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad
//                (Oct 3)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad
//                (Oct 3)
//
//   - (1,0,1,1): throwing+SPACE+DASH-padded sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, cap) filled with '-' on success
//     — the DASH fill OVERWRITES the SPACE fill, so the
//     (1,0,1,1) cell REUSES the (1,0,0,1) DASH-padded sibling
//     wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad
//                (Oct 3, REUSED across the (1,0,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad
//                (Oct 3, REUSED across the (1,0,*,1) cells)
//
//   - (1,1,1,0): throwing+NUL+SPACE-padded sibling (NEW today —
//     the (1,1,1,0) cell REUSES the (1,0,1,0) SPACE-padded
//     sibling verbatim per Sep 30's "SPACE overwrites NUL"
//     composition rule):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                (Sep 29, REUSED across the (1,*,1,0) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad
//                (Sep 29, REUSED across the (1,*,1,0) cells)
//
//   - (1,1,0,1): throwing+NUL+DASH-padded sibling (NEW today —
//     the (1,1,0,1) cell REUSES the (1,0,0,1) DASH-padded
//     sibling verbatim per TODAY's transitive composition rule
//     "DASH overwrites NUL when both PAD and DASH_PAD are set"):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad
//                (Oct 3, REUSED across the (1,*,0,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad
//                (Oct 3, REUSED across the (1,*,0,1) cells)
//
//   - (1,1,1,1): throwing+NUL+SPACE+DASH-padded sibling (NEW
//     today — the (1,1,1,1) cell REUSES the (1,0,0,1) DASH-
//     padded sibling verbatim per TODAY's transitive composition
//     rule "DASH overwrites SPACE overwrites NUL when all three
//     are set"):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad
//                (Oct 3, REUSED across the (1,*,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad
//                (Oct 3, REUSED across the (1,*,*,1) cells)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0 (expected+untouched
// — the prior contract across Sep 6 through Oct 5).
//
// KEY DESIGN DECISION — TODAY introduces NO new sibling wrappers.
// The composition rules mean:
//   1. The (0, 1, 0, 1) and (0, 1, 1, 1) cells REUSE the
//      (0, 0, 0, 1) DASH-pad sibling wrappers verbatim (DASH
//      overwrites NUL when both PAD and DASH_PAD are set, and
//      transitively DASH overwrites SPACE overwrites NUL when
//      all three are set).
//   2. The (1, 1, 0, 1) and (1, 1, 1, 1) cells REUSE the
//      (1, 0, 0, 1) throwing+DASH-pad sibling wrappers verbatim
//      per the same transitive composition rule.
//   3. The (0, 1, 1, 0) and (1, 1, 1, 0) cells REUSE the
//      (0, 0, 1, 0) and (1, 0, 1, 0) SPACE-pad sibling wrappers
//      verbatim per Sep 30's "SPACE overwrites NUL" composition
//      rule.
// The matrix is now COMPLETE for ALL 16 cells.
//
// PLATFORM NOTE: This file uses Sep 30's / Oct 1's / Oct 3's /
// Oct 5's PROVEN callback-based impl (the `cb` lambda +
// `std::move(counter)` pattern) — NOT the no-callback
// `counter.count()` pattern.  The no-callback pattern was found
// to NOT work on Apple Clang 21 / libc++ 21 because
// std::vformat_to internally COPIES the output iterator (the
// proxy's `it_` pointer ends up pointing to a copy, not the
// original), so `counter.count()` and `writer.written()` read
// 0 after the vformat_to call even though the proxy's
// operator= was invoked.  The callback pattern avoids this
// because the callback writes through a `void* data` pointer to
// a local `std::size_t needed` / `std::size_t written`, which
// the vformat_to copies do NOT see — only the local in the impl
// is updated.  See /tmp/probe_vformat_to_copy.cpp (from Oct 3)
// for the full investigation.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 /
// Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5)
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
// Part 2 — petra::counting_output_iterator (Sep 30 verbatim — callback-based)
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
// Part 3 — petra::detail::bounded_char_writer (Sep 30 verbatim —
// callback-based, increments `written_` ONLY on successful write)
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
// petra::try_format_bounded_runtime_n (Sep 9 verbatim).
// These are the expected+untouched siblings on the CHAR*+CAP axis.
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_runtime_n_padded (Sep 25 verbatim) +
// petra::try_format_bounded_runtime_n_padded (Sep 25 verbatim).
// These are the expected+NUL-padded siblings on the CHAR*+CAP axis.
// ============================================================================

namespace petra {

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

}  // namespace petra

// ============================================================================
// Part 7 — petra::try_format_runtime_n_throwing (Sep 20 verbatim) +
// petra::try_format_bounded_runtime_n_throwing (Sep 20 verbatim).
// These are the throwing+untouched siblings on the CHAR*+CAP axis.
// ============================================================================

namespace petra {

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

}  // namespace petra

// ============================================================================
// Part 8 — petra::try_format_runtime_n_padded_throwing (Sep 26 verbatim) +
// petra::try_format_bounded_runtime_n_padded_throwing (Sep 26 verbatim).
// These are the throwing+NUL-padded siblings on the CHAR*+CAP axis.
// ============================================================================

namespace petra {

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

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_padded_throwing(
    char* out, std::size_t cap,
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

}  // namespace petra

// ============================================================================
// Part 9 — petra::try_format_runtime_n_space_pad (Sep 29 verbatim) +
// petra::try_format_bounded_runtime_n_space_pad (Sep 29 verbatim) +
// petra::try_format_runtime_n_throwing_space_pad (Sep 29 verbatim) +
// petra::try_format_bounded_runtime_n_throwing_space_pad (Sep 29 verbatim).
// ============================================================================

namespace petra {

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
// Part 10 — petra::try_format_runtime_n_dash_pad (Oct 3 verbatim) +
// petra::try_format_bounded_runtime_n_dash_pad (Oct 3 verbatim) +
// petra::try_format_runtime_n_throwing_dash_pad (Oct 3 verbatim) +
// petra::try_format_bounded_runtime_n_throwing_dash_pad (Oct 3 verbatim).
//
// These are the FOUR DASH-padded sibling wrappers on the CHAR*+CAP ×
// BOUNDED-BUFFER axis.  TODAY's composition cells (the (0, 1, 0, 1),
// (0, 1, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1) cells) REUSE these
// wrappers VERBATIM (because DASH overwrites NUL when both PAD and
// DASH_PAD are set, and transitively DASH overwrites SPACE overwrites
// NUL when all three are set).
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_dash_pad(char* out, std::size_t cap,
                               std::string_view fmt,
                               const Args&... args) {
    auto r = try_format_runtime_n_space_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
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

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_dash_pad(char* out, std::size_t cap,
                                        std::format_string<Args...> fmt,
                                        const Args&... args) {
    auto r = try_format_bounded_runtime_n_space_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '-';
        }
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_dash_pad(char* out, std::size_t cap,
                                        std::string_view fmt,
                                        const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_space_pad(out, cap, fmt, args...);
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '-';
        }
        return n;
    } catch (...) {
        throw;
    }
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_dash_pad(
    char* out, std::size_t cap,
    std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_space_pad(
                out, cap, fmt, args...);
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
// Part 11 — TRY_FORMAT_BOUNDED_N_PAD_OR_THROW /
// TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW.
//
// The compile-time flag quadruple
// (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
// PETRA_DASH_PAD) controls the dispatch via #if
// PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
// PETRA_SPACE_PAD × #if PETRA_DASH_PAD.  This meta-macro
// dispatches on ALL 16 cells of the 4-bit tuple.  The (0, 1, 0,
// 1), (0, 1, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1) cells REUSE
// the (0, 0, 0, 1) and (1, 0, 0, 1) DASH-padded char*+cap
// sibling wrappers verbatim (the transitive composition rule).
// The (0, 1, 1, 0) and (1, 1, 1, 0) cells REUSE the (0, 0, 1, 0)
// and (1, 0, 1, 0) SPACE-padded char*+cap sibling wrappers
// verbatim (per Sep 30's rule).
// ============================================================================

#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (no NUL-pad) or 1 (NUL-padded)"
#endif

#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif

#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 (no dash-pad) or 1 (dash-padded)"
#endif

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (0, 0, 0, 0) — expected+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 9's try_format_bounded_runtime_n.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)             \
                                           __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 8's try_format_runtime_n.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                     \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (0, 1, 0, 0) — expected+NUL-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 25's
// try_format_bounded_runtime_n_padded.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_padded((out), (cap), (fmt)      \
                                                  __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 25's try_format_runtime_n_padded.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_padded((out), (cap), (fmt)              \
                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (0, 0, 1, 0) — expected+SPACE-padded sibling path.

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

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (0, 0, 0, 1) — expected+DASH-padded sibling path.

// CONSTEVAL surface: dispatches to Oct 3's
// try_format_bounded_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 3's
// try_format_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (0, 0, 1, 1) — expected+SPACE+DASH-padded sibling path.
// DASH overwrites SPACE — REUSES the (0, 0, 0, 1) DASH-padded
// sibling wrapper verbatim.

// CONSTEVAL surface: REUSES Oct 3's try_format_bounded_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 3's try_format_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (0, 1, 1, 0) — expected+NUL+SPACE-padded sibling path (NEW today).
// SPACE overwrites NUL — REUSES the (0, 0, 1, 0) SPACE-padded
// sibling wrapper verbatim (per Sep 30's "SPACE overwrites NUL"
// composition rule).

// CONSTEVAL surface: REUSES Sep 29's
// try_format_bounded_runtime_n_space_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_space_pad((out), (cap), (fmt)    \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Sep 29's try_format_runtime_n_space_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_space_pad((out), (cap), (fmt)            \
                                             __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (0, 1, 0, 1) — expected+NUL+DASH-padded sibling path (NEW today).
// DASH overwrites NUL — REUSES the (0, 0, 0, 1) DASH-padded
// sibling wrapper verbatim (per TODAY's transitive composition rule).

// CONSTEVAL surface: REUSES Oct 3's try_format_bounded_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 3's try_format_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (0, 1, 1, 1) — expected+NUL+SPACE+DASH-padded sibling path (NEW today).
// Transitive composition: DASH overwrites SPACE overwrites NUL
// — REUSES the (0, 0, 0, 1) DASH-padded sibling wrapper verbatim
// (per TODAY's transitive composition rule).

// CONSTEVAL surface: REUSES Oct 3's try_format_bounded_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 3's try_format_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (1, 0, 0, 0) — throwing+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 20's
// try_format_bounded_runtime_n_throwing.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 20's
// try_format_runtime_n_throwing.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (1, 1, 0, 0) — throwing+NUL-padded sibling path.

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

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (1, 0, 1, 0) — throwing+SPACE-padded sibling path.

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

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (1, 0, 0, 1) — throwing+DASH-padded sibling path.

// CONSTEVAL surface: dispatches to Oct 3's
// try_format_bounded_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad(               \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 3's
// try_format_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad((out), (cap), (fmt)    \
                                                     __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (1, 0, 1, 1) — throwing+SPACE+DASH-padded sibling path.
// DASH overwrites SPACE — REUSES the (1, 0, 0, 1) throwing+DASH-
// padded sibling wrapper verbatim.

// CONSTEVAL surface: REUSES Oct 3's try_format_bounded_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad(               \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 3's try_format_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad((out), (cap), (fmt)    \
                                                     __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (1, 1, 1, 0) — throwing+NUL+SPACE-padded sibling path (NEW today).
// SPACE overwrites NUL — REUSES the (1, 0, 1, 0) throwing+SPACE-
// padded sibling wrapper verbatim (per Sep 30's "SPACE overwrites
// NUL" composition rule).

// CONSTEVAL surface: REUSES Sep 29's
// try_format_bounded_runtime_n_throwing_space_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad(             \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Sep 29's
// try_format_runtime_n_throwing_space_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_space_pad((out), (cap), (fmt)   \
                                                      __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (1, 1, 0, 1) — throwing+NUL+DASH-padded sibling path (NEW today).
// DASH overwrites NUL — REUSES the (1, 0, 0, 1) throwing+DASH-
// padded sibling wrapper verbatim (per TODAY's transitive
// composition rule).

// CONSTEVAL surface: REUSES Oct 3's try_format_bounded_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad(               \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 3's try_format_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad((out), (cap), (fmt)    \
                                                     __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1
      //   && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (1, 1, 1, 1) — throwing+NUL+SPACE+DASH-padded sibling path (NEW today).
// Transitive composition: DASH overwrites SPACE overwrites NUL
// — REUSES the (1, 0, 0, 1) throwing+DASH-padded sibling wrapper
// verbatim (per TODAY's transitive composition rule).

// CONSTEVAL surface: REUSES Oct 3's try_format_bounded_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad(               \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 3's try_format_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad((out), (cap), (fmt)    \
                                                     __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_PAD × PETRA_SPACE_PAD
        // × PETRA_DASH_PAD

// ============================================================================
// Part 12 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
// Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 /
// Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 /
// Oct 4 / Oct 5).
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
// Part 13 — sections
// ============================================================================

int main() {
    // ----- Section 1 — (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    //       PETRA_SPACE_PAD, PETRA_DASH_PAD) flag quadruple pinned.
    SECTION("Section 1 - flag quadruple pinned");
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                      || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
    static_assert(PETRA_PAD == 0 || PETRA_PAD == 1,
                  "PETRA_PAD must be 0 or 1");
    static_assert(PETRA_SPACE_PAD == 0 || PETRA_SPACE_PAD == 1,
                  "PETRA_SPACE_PAD must be 0 or 1");
    static_assert(PETRA_DASH_PAD == 0 || PETRA_DASH_PAD == 1,
                  "PETRA_DASH_PAD must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 "
        "(expected+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 "
        "(expected+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 "
        "(expected+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 "
        "(expected+DASH-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 "
        "(expected+SPACE+DASH-padded sibling — DASH overwrites SPACE)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 "
        "(expected+NUL+SPACE-padded sibling — SPACE overwrites NUL)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 "
        "(expected+NUL+DASH-padded sibling — DASH overwrites NUL — NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 "
        "(expected+NUL+SPACE+DASH-padded sibling — transitive "
        "composition: DASH overwrites SPACE overwrites NUL — NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 "
        "(throwing+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 "
        "(throwing+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 "
        "(throwing+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 "
        "(throwing+DASH-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 "
        "(throwing+SPACE+DASH-padded sibling — DASH overwrites SPACE)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 "
        "(throwing+NUL+SPACE-padded sibling — SPACE overwrites NUL)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 "
        "(throwing+NUL+DASH-padded sibling — DASH overwrites NUL — NEW today)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 "
        "(throwing+NUL+SPACE+DASH-padded sibling — transitive "
        "composition: DASH overwrites SPACE overwrites NUL — NEW today)");
#endif
    CHECK(true);

    // 1b. std::runtime_error / std::format_error catch chain.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // ----- Section 2 — Byte-state contract on success.
    //     On (*, 1, 0, 1), (*, 1, 1, 1), (*, 0, 0, 1), (*, 0, 1, 1):
    //     bytes [written, cap) are '-' on success (DASH wins).
    //     On (*, 0, 1, 0), (*, 1, 1, 0): bytes [written, cap) are ' '
    //     on success (SPACE overwrites NUL or DASH is not set).
    //     On (*, 1, 0, 0): bytes [written, cap) are '\0' on success.
    //     On (*, 0, 0, 0): bytes [written, cap) are UNTOUCHED.
    SECTION("Section 2 - byte-state contract on success");
    {
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;  // "ABCDE"
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
#if PETRA_DASH_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all '-'
            // (covers DASH-overwrites-SPACE, DASH-overwrites-NUL,
            // and the transitive composition rule).
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '-');
            }
#elif PETRA_SPACE_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all ' '
            // (covers SPACE-overwrites-NUL composition and
            // SPACE-pad-only path).
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], ' ');
            }
#elif PETRA_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all '\0'
            // (NUL-pad-only path).
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '\0');
            }
#else
            // 11-CHECK block: bytes [5, 16) are UNTOUCHED ('Q').
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], 'Q');
            }
#endif
        }
#else  // PETRA_THROW_ON_OVERFLOW == 1
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        if (!threw) {
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
#if PETRA_DASH_PAD == 1
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '-');
            }
#elif PETRA_SPACE_PAD == 1
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], ' ');
            }
#elif PETRA_PAD == 1
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '\0');
            }
#else
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], 'Q');
            }
#endif
        }
#endif
    }

    // 2a. No-bleed: the char*+cap form must NOT write outside [0, cap).
    //     A 6-byte sub-region at offset 5 of a 16-byte backing buffer
    //     with format 'ABCDE' (5 bytes) leaves bytes [0, 5) and
    //     [11, 16) as poison 'Q' regardless of the flag 4-tuple.
    SECTION("Section 2a - no buffer bleed outside the cap");
    {
        constexpr std::size_t backing = 16;
        char backing_buf[backing];
        for (std::size_t i = 0; i < backing; ++i) backing_buf[i] = 'Q';
        char* sub = &backing_buf[5];
        std::size_t sub_cap = 6;
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            sub, sub_cap, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{5});
            CHECK_EQ(backing_buf[5], 'A');
            CHECK_EQ(backing_buf[6], 'B');
            CHECK_EQ(backing_buf[7], 'C');
            CHECK_EQ(backing_buf[8], 'D');
            CHECK_EQ(backing_buf[9], 'E');
            for (std::size_t i = 0; i < 5; ++i) {
                CHECK_EQ(backing_buf[i], 'Q');
            }
            for (std::size_t i = 11; i < backing; ++i) {
                CHECK_EQ(backing_buf[i], 'Q');
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                sub, sub_cap, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{5});
        if (!threw) {
            CHECK_EQ(backing_buf[5], 'A');
            CHECK_EQ(backing_buf[6], 'B');
            CHECK_EQ(backing_buf[7], 'C');
            CHECK_EQ(backing_buf[8], 'D');
            CHECK_EQ(backing_buf[9], 'E');
            for (std::size_t i = 0; i < 5; ++i) {
                CHECK_EQ(backing_buf[i], 'Q');
            }
            for (std::size_t i = 11; i < backing; ++i) {
                CHECK_EQ(backing_buf[i], 'Q');
            }
        }
#endif
    }

    // 2b. Concurrent dispatch: 4 threads × 25 calls each.
    //     Each thread formats into its own buffer with the SAME flag
    //     4-tuple.  All 100 calls must succeed (no flakiness).
    SECTION("Section 2b - concurrent dispatch");
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;
        constexpr int kTotal = kThreads * kCallsPerThread;
        char bufs[kTotal][16];
        std::atomic<int> ok_count{0};
        std::atomic<int> fail_count{0};
        std::array<std::thread, kThreads> threads;
        for (std::size_t t = 0; t < static_cast<std::size_t>(kThreads); ++t) {
            threads[t] = std::thread([&, t]() {
                int local_ok = 0;
                int local_fail = 0;
                for (int c = 0; c < kCallsPerThread; ++c) {
                    std::size_t idx = t * static_cast<std::size_t>(kCallsPerThread)
                                      + static_cast<std::size_t>(c);
                    char* buf = bufs[idx];
                    std::size_t cap = 16;
                    for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                        buf, cap, "ABCDE");
                    if (r.has_value() && r.value() == 5
                        && buf[0] == 'A' && buf[1] == 'B'
                        && buf[2] == 'C' && buf[3] == 'D'
                        && buf[4] == 'E') {
                        ++local_ok;
                    } else {
                        ++local_fail;
                    }
#else
                    std::size_t n = 0;
                    bool threw = false;
                    try {
                        n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                            buf, cap, "ABCDE");
                    } catch (...) {
                        threw = true;
                    }
                    if (!threw && n == 5
                        && buf[0] == 'A' && buf[1] == 'B'
                        && buf[2] == 'C' && buf[3] == 'D'
                        && buf[4] == 'E') {
                        ++local_ok;
                    } else {
                        ++local_fail;
                    }
#endif
                }
                ok_count.fetch_add(local_ok);
                fail_count.fetch_add(local_fail);
            });
        }
        for (auto& th : threads) th.join();
        CHECK_EQ(ok_count.load(), kTotal);
        CHECK_EQ(fail_count.load(), 0);
    }

    // 2c. cap=0 edge case.
    SECTION("Section 2c - cap=0 edge case");
    {
        char sentinel = 'Q';
        std::size_t cap = 0;
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            &sentinel, cap, "anything");
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        }
        CHECK_EQ(sentinel, 'Q');
#else
        bool threw = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                &sentinel, cap, "anything");
        } catch (...) {
            threw = true;
        }
        CHECK(threw);
        CHECK_EQ(sentinel, 'Q');
#endif
    }

    // 2d. written==cap edge case: full-buffer success.
    SECTION("Section 2d - written==cap edge case");
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDEFGH");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{8});
            for (std::size_t i = 0; i < 8; ++i) {
                CHECK_EQ(buf[i], "ABCDEFGH"[i]);
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "ABCDEFGH");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{8});
        for (std::size_t i = 0; i < 8; ++i) {
            CHECK_EQ(buf[i], "ABCDEFGH"[i]);
        }
#endif
    }

    // ----- Section 3 — Buffer atomicity on TooLarge.
    SECTION("Section 3 - Buffer atomicity on TooLarge");
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDEFGHIJ");
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        }
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#else
        bool threw = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "ABCDEFGHIJ");
        } catch (...) {
            threw = true;
        }
        CHECK(threw);
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#endif
    }

    // ----- Section 4 — Byte-count contract on success.
    SECTION("Section 4 - Byte-count contract on success");
    {
        char buf[32];
        std::size_t cap = 32;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
        // "Hello, world!" is 13 bytes.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "Hello, {}!", "world");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{13});
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "Hello, {}!", "world");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{13});
#endif
        for (std::size_t i = 0; i < 13; ++i) {
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
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "value is 42"[i]);
        }
    }

    // ----- Section 6 — CONSTEVAL gate pinned (decltype check).
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

    // ----- Section 8 — Format error atomicity on the RUNTIME surface.
    SECTION("Section 8 - Format error atomicity (RUNTIME)");
    {
        char buf[16];
        std::size_t cap = 16;
        for (std::size_t i = 0; i < cap; ++i) buf[i] = 'Q';
        std::string bad_fmt = "bad {";
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
    SECTION("Section 11 - No-args case via __VA_OPT__ comma");
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
    //     checks on ALL 16 cells of the flag 4-tuple.
    SECTION("Section 14 - Perpendicular-axis decltype checks (16 cells)");
    {
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
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
                      "(0,0,0,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,0,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
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
                      "(0,1,0,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,0,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
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
                      "(0,0,1,0) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,1,0) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
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
                      "(0,0,0,1) CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,0,1) RUNTIME must return expected<size_t, "
                      "FormatError>");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
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
                      "(0,0,1,1) CONSTEVAL must return expected<size_t, "
                      "FormatError> — reuses (0,0,0,1) DASH-padded "
                      "sibling verbatim because DASH overwrites SPACE");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,0,1,1) RUNTIME must return expected<size_t, "
                      "FormatError> — reuses (0,0,0,1) DASH-padded "
                      "sibling verbatim");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
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
                      "(0,1,1,0) CONSTEVAL must return expected<size_t, "
                      "FormatError> — reuses (0,0,1,0) SPACE-padded "
                      "sibling verbatim because SPACE overwrites NUL");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,1,0) RUNTIME must return expected<size_t, "
                      "FormatError> — reuses (0,0,1,0) SPACE-padded "
                      "sibling verbatim");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
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
                      "(0,1,0,1) CONSTEVAL must return expected<size_t, "
                      "FormatError> — reuses (0,0,0,1) DASH-padded "
                      "sibling verbatim because DASH overwrites NUL — "
                      "NEW today");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,0,1) RUNTIME must return expected<size_t, "
                      "FormatError> — reuses (0,0,0,1) DASH-padded "
                      "sibling verbatim — NEW today");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
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
                      "(0,1,1,1) CONSTEVAL must return expected<size_t, "
                      "FormatError> — reuses (0,0,0,1) DASH-padded "
                      "sibling verbatim because DASH overwrites SPACE "
                      "overwrites NUL (transitive) — NEW today");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "(0,1,1,1) RUNTIME must return expected<size_t, "
                      "FormatError> — reuses (0,0,0,1) DASH-padded "
                      "sibling verbatim — NEW today");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
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
                      "(1,0,0,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,0,0) RUNTIME must return std::size_t");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
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
                      "(1,1,0,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,0,0) RUNTIME must return std::size_t");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
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
                      "(1,0,1,0) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,1,0) RUNTIME must return std::size_t");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
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
                      "(1,0,0,1) CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,0,1) RUNTIME must return std::size_t");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
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
                      "(1,0,1,1) CONSTEVAL must return std::size_t — "
                      "reuses (1,0,0,1) DASH-padded sibling verbatim "
                      "because DASH overwrites SPACE");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,0,1,1) RUNTIME must return std::size_t — "
                      "reuses (1,0,0,1) DASH-padded sibling verbatim");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
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
                      "(1,1,1,0) CONSTEVAL must return std::size_t — "
                      "reuses (1,0,1,0) SPACE-padded sibling verbatim "
                      "because SPACE overwrites NUL");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,1,0) RUNTIME must return std::size_t — "
                      "reuses (1,0,1,0) SPACE-padded sibling verbatim");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
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
                      "(1,1,0,1) CONSTEVAL must return std::size_t — "
                      "reuses (1,0,0,1) DASH-padded sibling verbatim "
                      "because DASH overwrites NUL — NEW today");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,0,1) RUNTIME must return std::size_t — "
                      "reuses (1,0,0,1) DASH-padded sibling verbatim — "
                      "NEW today");
#else  // (1, 1, 1, 1)
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
                      "(1,1,1,1) CONSTEVAL must return std::size_t — "
                      "reuses (1,0,0,1) DASH-padded sibling verbatim "
                      "because DASH overwrites SPACE overwrites NUL "
                      "(transitive) — NEW today");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "(1,1,1,1) RUNTIME must return std::size_t — "
                      "reuses (1,0,0,1) DASH-padded sibling verbatim — "
                      "NEW today");
#endif
        CHECK(true);
    }

    // ----- Section 15 — std::format_error dynamic_cast check on the
    //     throwing path.
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

    // ----- Section 16 — NEW today: DASH-overwrites-NUL composition
    //     rule pinned on the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0,
    //     1), and (1, 1, 1, 1) cells.  On these cells, bytes
    //     [written, cap) MUST be '-' on success (NOT '\0', NOT ' ',
    //     and NOT 'Q').  This pins the transitive composition rule.
    SECTION("Section 16 - DASH overwrites NUL (and transitive SPACE — NUL)");
    {
#if PETRA_DASH_PAD == 1 && PETRA_PAD == 1
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
        // 11-CHECK block: bytes [5, 16) are all '-'
        // (DASH overwrites NUL — the transitive composition rule).
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '-');
            // And NOT '\0' (explicit disambiguation from NUL-padded sibling).
            CHECK(buf[i] != '\0');
            // And NOT ' ' (explicit disambiguation from SPACE-padded sibling).
            CHECK(buf[i] != ' ');
            // And NOT 'Q' (explicit disambiguation from untouched sibling).
            CHECK(buf[i] != 'Q');
        }
#else
        // Out of scope for cells where PETRA_DASH_PAD != 1 OR PETRA_PAD != 1.
        CHECK(true);
#endif
    }

    // ----- Section 17 — NEW today: SPACE-overwrites-NUL composition
    //     rule pinned on the (0, 1, 1, 0) and (1, 1, 1, 0) cells.  On
    //     these cells, bytes [written, cap) MUST be ' ' on success
    //     (NOT '\0' and NOT 'Q').
    SECTION("Section 17 - SPACE overwrites NUL (PAD=1, SPACE_PAD=1, DASH_PAD=0)");
    {
#if PETRA_PAD == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
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
                "FAIL: Section 17 - unexpected throw on success\n");
        }
#endif
        // 11-CHECK block: bytes [5, 16) are all ' '
        // (SPACE overwrites NUL — the composition rule).
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != 'Q');
        }
#else
        CHECK(true);
#endif
    }

    // ----- Section 18 — NEW today: composition rule is functional
    //     (the (PAD=1, DASH_PAD=1) cells produce bytes [written, cap)
    //     = '-' on success, REPRODUCIBLY across multiple invocations).
    SECTION("Section 18 - composition rule is reproducible across calls");
    {
#if PETRA_DASH_PAD == 1 && PETRA_PAD == 1
        // Second format after Section 16 to pin reproducibility.
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
                "FAIL: Section 18 - unexpected throw on success\n");
        }
#endif
        CHECK_EQ(buf2[0], 'H');
        CHECK_EQ(buf2[1], 'I');
        // Verify the suffix: bytes [2, 16) are '-'
        // (the (1, *, *, 1) cells' transitive composition rule).
        for (std::size_t i = 2; i < cap2; ++i) {
            CHECK_EQ(buf2[i], '-');
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