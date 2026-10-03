// P-2026-10-03 — try_format_dash_pad_n:
//
// Closes Oct 1's "Where we go next" item (1) verbatim — the
// SEVENTH axis (PETRA_DASH_PAD) sibling-choice META-MACRO layer
// on the CHAR*+CAP × BOUNDED-BUFFER axis.  TODAY introduces a
// NEW FOURTH compile-time flag (PETRA_DASH_PAD) on top of
// Oct 1's 3-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD).  PETRA_DASH_PAD=1 selects the DASH-padded
// sibling, which fills bytes [written, cap) with '-' on
// success.
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
//   - (0,0,1,0): expected+SPACE-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, cap) filled
//     with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad
//                (Sep 29)
//      RUNTIME:   petra::try_format_runtime_n_space_pad (Sep 29)
//
//   - (0,0,0,1): expected+DASH-padded sibling (NEW today; returns
//     expected<size_t, FormatError>; bytes [written, cap) filled
//     with '-' on success — the DASH fill OVERWRITES the SPACE
//     fill from Sep 29's wrapper):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad
//                (NEW today)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad
//                (NEW today)
//
//   - (0,0,1,1): expected+SPACE+DASH-padded sibling (NEW today;
//     returns expected<size_t, FormatError>; bytes [written, cap)
//     filled with '-' on success — the DASH fill OVERWRITES the
//     SPACE fill, so the (0,0,1,1) cell REUSES the (0,0,0,1)
//     DASH-pad sibling wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad
//                (NEW today, REUSED across the (0,0,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad
//                (NEW today, REUSED across the (0,0,*,1) cells)
//
//   - (1,0,0,0): throwing+untouched sibling (returns std::size_t
//     on success, throws std::runtime_error on overflow; bytes
//     [written, cap) UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing
//                (Sep 20)
//      RUNTIME:   petra::try_format_runtime_n_throwing (Sep 20)
//
//   - (1,0,1,0): throwing+SPACE-padded sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, cap) filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad
//                (Sep 29)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad
//                (Sep 29)
//
//   - (1,0,0,1): throwing+DASH-padded sibling (NEW today;
//     returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, cap) filled with '-' on
//     success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad
//                (NEW today)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad
//                (NEW today)
//
//   - (1,0,1,1): throwing+SPACE+DASH-padded sibling (NEW today;
//     returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, cap) filled with '-' on
//     success — the DASH fill OVERWRITES the SPACE fill, so the
//     (1,0,1,1) cell REUSES the (1,0,0,1) DASH-pad sibling
//     wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad
//                (NEW today, REUSED across the (1,0,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad
//                (NEW today, REUSED across the (1,0,*,1) cells)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0 (expected+untouched
// — the prior contract across Sep 6 through Oct 1).
//
// KEY DESIGN DECISION — the DASH-pad sibling is a thin layer on
// top of Sep 29's SPACE-pad sibling.  The composition rule
// "the LATER fill overwrites the EARLIER fill" applies:
//   1. The DASH-pad wrapper calls the SPACE-pad sibling (Sep
//      29's wrapper), which fills bytes [written, cap) with
//      ' ' on success.
//   2. On success, the DASH-pad wrapper OVERWRITES those ' '
//      bytes with '-' (the LATER fill).
//   3. On error (Format or TooLarge), the DASH-pad wrapper does
//      NOT touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
// Because the DASH-pad wrapper REUSES the SPACE-pad sibling
// verbatim, the (PAD=0, SPACE_PAD=1, DASH_PAD=1) cells share
// the SAME DASH-pad sibling wrapper as the (PAD=0,
// SPACE_PAD=0, DASH_PAD=1) cells — the (0,0,1,1) and (1,0,1,1)
// cells are functionally equivalent to the (0,0,0,1) and
// (1,0,0,1) cells, just like Sep 30's / Oct 1's (PAD=1,
// SPACE_PAD=1) cells were functionally equivalent to the
// (PAD=0, SPACE_PAD=1) cells.
//
// SCOPE NOTE: This lesson is FOCUSED on the SEVENTH axis
// (PETRA_DASH_PAD) ONLY.  The (PAD=1) cells require Sep 25's
// / Sep 26's NUL-pad wrappers, which are NOT included in this
// file.  When PETRA_PAD = 1, the lesson's dispatcher #errors
// with a clear pointer to Sep 25 / Sep 26.  Similarly, the
// SPACE-pad branch is included for the (SPACE_PAD=1, DASH_PAD=0)
// cells (which reuse Sep 29's SPACE-pad sibling wrappers
// verbatim).
//
// PLATFORM NOTE: This file uses Sep 30's PROVEN callback-based
// impl (the `cb` lambda + `std::move(counter)` pattern) — NOT
// the no-callback `counter.count()` pattern from a prior
// untracked P-2026-10-02 draft.  The no-callback pattern was
// found to NOT work on Apple Clang 21 / libc++ 21 because
// std::vformat_to internally COPIES the output iterator (the
// proxy's `it_` pointer ends up pointing to a copy, not the
// original), so `counter.count()` and `writer.written()` read
// 0 after the vformat_to call even though the proxy's
// operator= was invoked (the count updates went to the
// discarded copy).  The callback pattern avoids this because
// the callback writes through a `void* data` pointer to a
// local `std::size_t needed` / `std::size_t written`, which
// the vformat_to copies do NOT see — only the local in the
// impl is updated.  See /tmp/probe_vformat_to_copy.cpp for
// the full investigation.

#include <array>
#include <atomic>
#include <cstdio>
#include <cstddef>
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
// Sep 29 / Sep 30 / Oct 1)
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
// callback-based, increments `written_` ONLY on successful write — matches
// the Sep 30 / Oct 1 design)
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
// Part 5 — petra::try_format_runtime_n (Sep 8 verbatim char*+cap) +
// petra::try_format_bounded_runtime_n (Sep 9 verbatim char*+cap).
//
// These are the expected+untouched siblings on the CHAR*+CAP axis.
// TODAY's NEW DASH-pad sibling wrappers (Part 7) REUSE these wrappers
// indirectly via Sep 29's SPACE-pad siblings (Part 6).
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
// Part 6 — petra::try_format_runtime_n_throwing (Sep 20 verbatim) +
// petra::try_format_bounded_runtime_n_throwing (Sep 20 verbatim).
//
// These are the throwing+untouched siblings on the CHAR*+CAP axis.
// TODAY's NEW throwing+DASH-pad sibling wrappers (Part 7) REUSE these
// indirectly via Sep 29's throwing+SPACE-pad siblings.
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
// Part 7 — petra::try_format_runtime_n_space_pad (Sep 29 verbatim) +
// petra::try_format_bounded_runtime_n_space_pad (Sep 29 verbatim) +
// petra::try_format_runtime_n_throwing_space_pad (Sep 29 verbatim) +
// petra::try_format_bounded_runtime_n_throwing_space_pad (Sep 29 verbatim).
//
// TODAY's NEW DASH-pad sibling wrappers (Part 8) REUSE these SPACE-pad
// sibling wrappers verbatim — the DASH-pad wrapper calls the SPACE-pad
// sibling, which fills [written, cap) with ' ', and then the DASH-pad
// wrapper overwrites those ' ' bytes with '-' on success.
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
// Part 8 — petra::try_format_runtime_n_dash_pad (NEW today) +
// petra::try_format_bounded_runtime_n_dash_pad (NEW today) +
// petra::try_format_runtime_n_throwing_dash_pad (NEW today) +
// petra::try_format_bounded_runtime_n_throwing_dash_pad (NEW today).
//
// TODAY's NEW DASH-pad sibling wrappers are thin layers on top of
// Sep 29's SPACE-pad siblings.  The composition rule "the LATER
// fill overwrites the EARLIER fill" applies:
//
//   1. The DASH-pad wrapper calls the SPACE-pad sibling, which
//      fills bytes [written, cap) with ' ' on success (or
//      short-circuits on error).
//   2. On success, the DASH-pad wrapper OVERWRITES those ' '
//      bytes with '-' (the LATER fill).
//   3. On error (Format or TooLarge), the DASH-pad wrapper does
//      NOT touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
//
// (active when PETRA_PAD=0 AND PETRA_DASH_PAD=1):
//   - (0, 0, 0, 1) expected+DASH-padded sibling (RUNTIME surface):
//     petra::try_format_runtime_n_dash_pad
//     (wraps Sep 29's try_format_runtime_n_space_pad verbatim
//      and dash-fills [written, cap) with '-' on success).
//   - (0, 0, 0, 1) expected+DASH-padded sibling (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_dash_pad
//     (wraps Sep 29's try_format_bounded_runtime_n_space_pad
//      verbatim and dash-fills [written, cap) with '-' on success).
//   - (1, 0, 0, 1) throwing+DASH-padded sibling (RUNTIME surface):
//     petra::try_format_runtime_n_throwing_dash_pad
//     (wraps Sep 29's try_format_runtime_n_throwing_space_pad
//      verbatim and dash-fills [written, cap) with '-' on
//      success).
//   - (1, 0, 0, 1) throwing+DASH-padded sibling (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_throwing_dash_pad
//     (wraps Sep 29's try_format_bounded_runtime_n_throwing_space_pad
//      verbatim and dash-fills [written, cap) with '-' on success).
//
// (active when PETRA_PAD=0 AND PETRA_SPACE_PAD=1 AND
//  PETRA_DASH_PAD=1):
//   - (0, 0, 1, 1) expected+SPACE+DASH-padded sibling: REUSES the
//     (0, 0, 0, 1) DASH-pad sibling wrapper verbatim (the SPACE
//     fill is overwritten by the DASH fill).
//   - (1, 0, 1, 1) throwing+SPACE+DASH-padded sibling: REUSES the
//     (1, 0, 0, 1) DASH-pad sibling wrapper verbatim.
// ============================================================================

namespace petra {

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the char*+cap axis.  Wraps Sep 29's
// try_format_runtime_n_space_pad verbatim and dash-fills
// [written, cap) with '-' on success ONLY (the dash-fill loop
// runs only when the SPACE-pad sibling returned normally).
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

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the char*+cap axis.  Wraps Sep 29's
// try_format_bounded_runtime_n_space_pad verbatim and dash-fills
// [written, cap) with '-' on success ONLY.
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

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the char*+cap axis.  Wraps Sep 29's
// try_format_runtime_n_throwing_space_pad verbatim and dash-fills
// [written, cap) with '-' on success ONLY (control does NOT
// reach the dash-fill loop on error — the catch block re-raises
// the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_dash_pad(char* out, std::size_t cap,
                                        std::string_view fmt,
                                        const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_space_pad(out, cap, fmt, args...);
        // Success path: dash-fill out[n, cap) with '-'.
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '-';
        }
        return n;
    } catch (...) {
        // Atomicity preserved: the underlying SPACE-pad throwing
        // wrapper did not touch the buffer (delegated to Sep 20's
        // throwing wrapper, which delegates to Sep 8's expected
        // impl, which short-circuits on TooLarge; and on Format
        // errors, pass-1's std::vformat_to throws
        // std::format_error before any writes).  Re-raise the
        // exception unchanged.
        throw;
    }
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the char*+cap axis.  Wraps Sep 29's
// try_format_bounded_runtime_n_throwing_space_pad verbatim and
// dash-fills [written, cap) with '-' on success ONLY.
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
        // Success path: dash-fill out[n, cap) with '-'.
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
// Part 9 — TRY_FORMAT_BOUNDED_N_PAD_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW
// (Mirrors Sep 30's shape verbatim; now with PETRA_DASH_PAD as a
// FOURTH axis.  The compile-time flag quadruple
// (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
// PETRA_DASH_PAD) controls the dispatch via #if
// PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
// PETRA_SPACE_PAD × #if PETRA_DASH_PAD.)
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling —
// matches the prior contract across Sep 6 through Oct 1).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

// PETRA_PAD defaults to 0 (no NUL-pad — matches the prior
// contract across Sep 6 through Oct 1).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (no NUL-pad) or 1 (NUL-padded)"
#endif

// PETRA_SPACE_PAD defaults to 0 (no space-pad — matches the
// prior contract across Sep 6 through Oct 1).
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

// Sanity check: PETRA_SPACE_PAD must be 0 or 1.
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

// PETRA_DASH_PAD defaults to 0 (no dash-pad — matches the prior
// contract across Sep 6 through Oct 1).
#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif

// Sanity check: PETRA_DASH_PAD must be 0 or 1.
#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 (no dash-pad) or 1 (dash-padded)"
#endif

// Guard: when PETRA_PAD=1, the lesson's dispatcher does NOT
// include the NUL-pad sibling wrappers (those live in Sep 25
// / Sep 26's lessons).  The dispatcher #errors with a clear
// pointer to the right lesson.
#if PETRA_PAD != 0
#error "This lesson (try_format_dash_pad_n) only supports PETRA_PAD=0.  For PETRA_PAD=1, use Sep 25's try_format_padded_n_or_into (expected+NUL-padded) or Sep 26's try_format_padded_n_or_throw (throwing+NUL-padded)."
#endif

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (0, 0, 0, 0) → expected+untouched sibling path.  Returns
// std::expected<std::size_t, FormatError>.  On TooLarge, the
// underlying impl returns FormatError{TooLarge, ..., needed_bytes}
// UNCHANGED; the buffer is UNTOUCHED on overflow.

// CONSTEVAL surface: dispatches to Sep 9's try_format_bounded_runtime_n.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)             \
                                           __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 8's try_format_runtime_n.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                     \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (0, 0, 1, 0) → expected+SPACE-padded sibling path.  Returns
// std::expected<std::size_t, FormatError>.  On success, bytes
// [written, cap) are filled with ' '.  On overflow or Format, the
// buffer is UNTOUCHED (atomicity preserved by the underlying impl;
// the space-padded wrapper does NOT touch the buffer on error).

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
// (0, 0, 0, 1) → expected+DASH-padded sibling path.  Returns
// std::expected<std::size_t, FormatError>.  On success, bytes
// [written, cap) are filled with '-'.  On overflow or Format, the
// buffer is UNTOUCHED (atomicity preserved by the underlying
// SPACE-pad wrapper; the dash-padded wrapper does NOT touch the
// buffer on error).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (0, 0, 1, 1) → expected+SPACE+DASH-padded sibling path.
// Returns std::expected<std::size_t, FormatError>.  On success,
// bytes [written, cap) are filled with '-' (the DASH fill
// OVERWRITES the SPACE fill from the SPACE-pad sibling, so the
// (0,0,1,1) cell REUSES the (0,0,0,1) DASH-pad sibling wrapper
// verbatim).  On overflow or Format, the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_dash_pad (the (0,0,1,1) and
// (0,0,0,1) cells share the SAME sibling wrapper because DASH
// overwrites SPACE when both are set — the composition rule).
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad((out), (cap), (fmt)     \
                                                    __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW try_format_runtime_n_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad((out), (cap), (fmt)             \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (1, 0, 0, 0) → throwing+untouched sibling path.  Returns
// std::size_t on success.  On TooLarge, throws std::runtime_error
// carrying the SAME .what() message that FormatError{TooLarge,
// ...} would carry.  The buffer is UNTOUCHED on overflow
// (atomicity preserved by Sep 8's / Sep 9's underlying impl).

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

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (1, 0, 1, 0) → throwing+SPACE-padded sibling path.  Returns
// std::size_t on success.  On success, bytes [written, cap) are
// filled with ' '.  On overflow or Format, throws
// std::runtime_error; the buffer is UNTOUCHED (atomicity
// preserved by the underlying throwing wrapper — Sep 20's
// wrapper already delegates atomicity to Sep 8's / Sep 9's
// underlying impl, and Sep 29's SPACE-pad wrapper does NOT touch
// the buffer on error).

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
// (1, 0, 0, 1) → throwing+DASH-padded sibling path.  Returns
// std::size_t on success.  On success, bytes [written, cap) are
// filled with '-'.  On overflow or Format, throws
// std::runtime_error; the buffer is UNTOUCHED (atomicity
// preserved by the underlying SPACE-pad throwing wrapper —
// delegated to Sep 29's wrapper, which delegates to Sep 20's
// throwing wrapper, which delegates to Sep 8's / Sep 9's
// underlying impl; TODAY's NEW DASH-pad wrapper does NOT touch
// the buffer on error).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad(               \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad((out), (cap), (fmt)    \
                                                     __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0
       //   && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (1, 0, 1, 1) → throwing+SPACE+DASH-padded sibling path.
// Returns std::size_t on success.  On success, bytes [written,
// cap) are filled with '-' (the DASH fill OVERWRITES the SPACE
// fill from the SPACE-pad sibling, so the (1,0,1,1) cell REUSES
// the (1,0,0,1) DASH-pad sibling wrapper verbatim).  On
// overflow or Format, throws std::runtime_error; the buffer is
// UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_throwing_dash_pad (the (1,0,1,1)
// and (1,0,0,1) cells share the SAME sibling wrapper because
// DASH overwrites SPACE when both are set — the composition
// rule).
#define TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(out, cap, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad(               \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW
// try_format_runtime_n_throwing_dash_pad.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(out, cap, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad((out), (cap), (fmt)    \
                                                     __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_PAD × PETRA_SPACE_PAD
        // × PETRA_DASH_PAD

// ============================================================================
// Part 10 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
// Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 /
// Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1).
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
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 "
        "(throwing+untouched sibling)");
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
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 "
        "(throwing+SPACE+DASH-padded sibling — DASH overwrites SPACE)");
#endif
    CHECK(true);

    // 1c. Pin std::runtime_error / std::format_error relationship.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // ----- Section 2 — DASH-padded byte-state contract on success.
    //     On (*, 0, *, 1): bytes [written, cap) are '-' on success
    //     (DASH wins over SPACE — composition rule).  On (*, 0, 0,
    //     0): bytes [written, cap) are UNTOUCHED (the original
    //     poison byte).  On (*, 0, 1, 0): bytes [written, cap) are
    //     ' ' on success.  On (*, 1, *, *): bytes [written, cap)
    //     are '\0' on success (OUT OF SCOPE for this lesson —
    //     handled by Sep 25's / Sep 26's lessons).
    SECTION("Section 2 - DASH-padded byte-state contract on success");
    {
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;  // "ABCDE"
        // Pre-fill with poison byte 'Q' so we can detect WRITES.
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            // 5-CHECK block: bytes [0, 5) are "ABCDE".
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
#if PETRA_DASH_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all '-'.
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '-');
            }
#elif PETRA_SPACE_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all ' '.
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], ' ');
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
            // 5-CHECK block: bytes [0, 5) are "ABCDE".
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
#if PETRA_DASH_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all '-'.
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '-');
            }
#elif PETRA_SPACE_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all ' '.
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], ' ');
            }
#else
            // 11-CHECK block: bytes [5, 16) are UNTOUCHED ('Q').
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], 'Q');
            }
#endif
        }
#endif
    }

    // 2a. DASH-pad sibling: no buffer bleed outside the cap.
    // (char*+cap form: pre-fill backing buffer with 'Q',
    // point the macro at a sub-region with explicit cap, and
    // verify the backing bytes outside the sub-region stay 'Q'.)
    SECTION("Section 2a - DASH-pad no buffer bleed (DASH path)");
    {
        constexpr std::size_t backing = 16;
        char backing_buf[backing];
        // Pre-fill the entire backing buffer with poison 'Q'.
        for (std::size_t i = 0; i < backing; ++i) {
            backing_buf[i] = 'Q';
        }
        // Format "ABCDE" (5 bytes) into a 6-byte sub-region at
        // &backing_buf[5] (cap=6).  Bytes [5, 10) of the backing
        // get "ABCDE", backing_buf[10] gets '-' on the DASH-pad
        // path.  Backing bytes [0, 5) and [11, 16) must remain
        // poison 'Q'.
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
            // No-bleed: bytes [0, 5) and [11, 16) stay 'Q'.
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

    // ----- Section 3 — DASH-pad buffer atomicity on TooLarge.
    //     When the formatted output does not fit in cap, the
    //     buffer is UNTOUCHED (the underlying impl returns
    //     TooLarge and the wrapper does NOT touch the buffer on
    //     error).
    SECTION("Section 3 - DASH-pad buffer atomicity on TooLarge");
    {
        char buf[8];
        std::size_t cap = 8;
        // Pre-fill with poison 'Q'.
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
        // "ABCDEFGHIJ" is 10 bytes — TooLarge for cap=8.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDEFGHIJ");
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        }
        // 8-CHECK block: every byte is still 'Q'.
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
        // 8-CHECK block: every byte is still 'Q'.
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#endif
    }

    // ----- Section 4 — DASH-pad byte-count contract on success.
    //     The return value is the number of bytes written by the
    //     underlying format (NOT the number of bytes padded).
    SECTION("Section 4 - DASH-pad byte-count contract on success");
    {
        char buf[32];
        std::size_t cap = 32;
        // "hello world" is 11 bytes.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "hello world");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{11});
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "hello world");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{11});
#endif
    }

    // ----- Section 5 — DASH-pad success with format args.
    //     Use a format string with replacement fields.
    SECTION("Section 5 - DASH-pad success with format args");
    {
        char buf[32];
        std::size_t cap = 32;
        // "x=42" is 4 bytes.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "x={}", 42);
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{4});
            CHECK_EQ(buf[0], 'x');
            CHECK_EQ(buf[1], '=');
            CHECK_EQ(buf[2], '4');
            CHECK_EQ(buf[3], '2');
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "x={}", 42);
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{4});
        CHECK_EQ(buf[0], 'x');
        CHECK_EQ(buf[1], '=');
        CHECK_EQ(buf[2], '4');
        CHECK_EQ(buf[3], '2');
#endif
    }

    // ----- Section 6 — DASH-pad Format error → no buffer writes.
    //     A malformed format string (e.g. "bad {") throws on the
    //     first std::vformat_to call (BEFORE any writes), so the
    //     buffer is UNTOUCHED.
    SECTION("Section 6 - DASH-pad Format error leaves buffer untouched");
    {
        char buf[16];
        std::size_t cap = 16;
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
        // "bad {" is malformed (unterminated replacement field).
        // Use a runtime std::string so the consteval gate does
        // not fire at compile time.
        std::string fmt = "bad {";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, fmt);
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
        }
        // 16-CHECK block: every byte is still 'Q'.
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#else
        bool threw = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, fmt);
        } catch (...) {
            threw = true;
        }
        CHECK(threw);
        for (std::size_t i = 0; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#endif
    }

    // ----- Section 7 — DASH-pad full-buffer success (written==cap).
    //     When the formatted output exactly fills the buffer,
    //     the DASH-fill loop has i == cap as the initial
    //     condition, so the loop body never executes.  The byte
    //     state contract is the same as the untouched contract
    //     (every byte is the formatted text, not '-').  This
    //     pins the DASH-fill loop's edge-case handling.
    SECTION("Section 7 - DASH-pad full-buffer success");
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
        // "ABCDEFGH" is 8 bytes — exactly fills cap=8.
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

    // ----- Section 8 — DASH-pad zero-cap edge case.
    //     When cap == 0, the underlying impl returns TooLarge
    //     (needed > 0 for any non-empty format).  The DASH-fill
    //     loop has i < cap == 0 as the initial condition, so
    //     the loop body never executes even on success (which
    //     can't happen with cap=0).  The buffer is UNTOUCHED.
    SECTION("Section 8 - DASH-pad zero-cap edge case");
    {
        // Pre-fill a sentinel byte with 'Q' to verify atomicity
        // (the cap=0 path cannot actually write to a real buffer
        // because the buffer has 0 writable bytes; we use a
        // single-byte sentinel just to detect any spurious write).
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

    // ----- Section 9 — DASH-pad DASH-overwrites-SPACE composition.
    //     When BOTH PETRA_SPACE_PAD and PETRA_DASH_PAD are 1,
    //     the DASH fill OVERWRITES the SPACE fill from the
    //     SPACE-pad sibling.  The end state is bytes [written,
    //     cap) = '-'.  This pins the composition rule
    //     "the LATER fill overwrites the EARLIER fill".
    SECTION("Section 9 - DASH-pad DASH-overwrites-SPACE composition");
#if PETRA_DASH_PAD == 1 && PETRA_SPACE_PAD == 1
    {
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;  // "ABCDE"
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            // 5-CHECK block: bytes [0, 5) are "ABCDE".
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
            // 11-CHECK block: bytes [5, 16) are all '-'
            // (DASH overwrites SPACE).
            for (std::size_t i = expected_written; i < cap; ++i) {
                CHECK_EQ(buf[i], '-');
            }
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
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(buf[i], "ABCDE"[i]);
        }
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '-');
        }
#endif
    }
#else
    {
        // Out of scope for the current (SPACE_PAD, DASH_PAD)
        // cell.  Pin a single CHECK for count consistency.
        CHECK(true);
    }
#endif

    // ----- Section 10 — DASH-pad concurrent dispatch.
    //     4 threads × 25 calls each, each call to a DIFFERENT
    //     flag-tuple dispatch.  All 100 calls must succeed (the
    //     dispatch is per-call; the static flag is per-TU).
    //     Because all 100 calls use the SAME flag-tuple, all
    //     must succeed with the SAME return value.
    SECTION("Section 10 - DASH-pad concurrent dispatch");
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
                    // Pre-fill with poison 'Q'.
                    for (std::size_t i = 0; i < cap; ++i) {
                        buf[i] = 'Q';
                    }
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

    // ----- Section 11 — DASH-pad distinct from SPACE and NUL.
    //     When PETRA_DASH_PAD=1, bytes [written, cap) are '-'
    //     (NOT ' ' and NOT '\0').  This pins the
    //     byte-state distinctness of the DASH-pad sibling from
    //     the SPACE-pad and NUL-pad siblings.
    SECTION("Section 11 - DASH-pad distinct from SPACE and NUL");
#if PETRA_DASH_PAD == 1
    {
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            for (std::size_t i = expected_written; i < cap; ++i) {
                // NOT ' ' (SPACE-pad), NOT '\0' (NUL-pad),
                // NOT 'Q' (untouched poison).
                CHECK(buf[i] == '-');
                CHECK(buf[i] != ' ');
                CHECK(buf[i] != '\0');
                CHECK(buf[i] != 'Q');
            }
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
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK(buf[i] == '-');
            CHECK(buf[i] != ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != 'Q');
        }
#endif
    }
#else
    {
        // Out of scope for the current (SPACE_PAD, DASH_PAD)
        // cell.  Pin a single CHECK for count consistency.
        CHECK(true);
    }
#endif

    // ----- Section 12 — DASH-pad DASH-overwrites-SPACE distinctness.
    //     When BOTH PETRA_SPACE_PAD and PETRA_DASH_PAD are 1,
    //     the DASH fill OVERWRITES the SPACE fill.  The end
    //     state is bytes [written, cap) = '-', which is
    //     distinct from ' ' (SPACE-pad-only) and '\0'
    //     (NUL-pad).
    SECTION("Section 12 - DASH-pad DASH-overwrites-SPACE distinctness");
#if PETRA_DASH_PAD == 1 && PETRA_SPACE_PAD == 1
    {
        char buf[16];
        std::size_t cap = 16;
        std::size_t expected_written = 5;
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            for (std::size_t i = expected_written; i < cap; ++i) {
                // '-', NOT ' ', NOT '\0', NOT 'Q'.
                CHECK(buf[i] == '-');
                CHECK(buf[i] != ' ');
                CHECK(buf[i] != '\0');
                CHECK(buf[i] != 'Q');
            }
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
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK(buf[i] == '-');
            CHECK(buf[i] != ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != 'Q');
        }
#endif
    }
#else
    {
        CHECK(true);
    }
#endif

    // ----- Section 13 — DASH-pad success with empty format string.
    //     An empty format string formats to 0 bytes.  The
    //     DASH-fill loop fills the ENTIRE buffer with '-'.
    SECTION("Section 13 - DASH-pad success with empty format string");
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, cap, "");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{0});
            for (std::size_t i = 0; i < cap; ++i) {
#if PETRA_DASH_PAD == 1
                CHECK_EQ(buf[i], '-');
#elif PETRA_SPACE_PAD == 1
                CHECK_EQ(buf[i], ' ');
#else
                CHECK_EQ(buf[i], 'Q');
#endif
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{0});
        for (std::size_t i = 0; i < cap; ++i) {
#if PETRA_DASH_PAD == 1
            CHECK_EQ(buf[i], '-');
#elif PETRA_SPACE_PAD == 1
            CHECK_EQ(buf[i], ' ');
#else
            CHECK_EQ(buf[i], 'Q');
#endif
        }
#endif
    }

    // ----- Section 14 — DASH-pad CONSTEVAL surface pin.
    //     The CONSTEVAL surface (TRY_FORMAT_BOUNDED_N_PAD_OR_THROW)
    //     requires std::format_string<Args...>, which has a
    //     consteval ctor.  This pins the CONSTEVAL surface by
    //     calling it with a literal fmt.
    SECTION("Section 14 - DASH-pad CONSTEVAL surface pin");
    {
        char buf[32];
        std::size_t cap = 32;
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            buf, cap, "CONSTEVAL test");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{14});
            for (std::size_t i = 0; i < 14; ++i) {
                CHECK_EQ(buf[i], "CONSTEVAL test"[i]);
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
                buf, cap, "CONSTEVAL test");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{14});
        for (std::size_t i = 0; i < 14; ++i) {
            CHECK_EQ(buf[i], "CONSTEVAL test"[i]);
        }
#endif
    }

    // ----- Section 15 — DASH-pad throwing sibling re-raises as
    //     std::runtime_error (NOT std::format_error).
    SECTION("Section 15 - DASH-pad throwing sibling catch chain");
#if PETRA_THROW_ON_OVERFLOW == 1
    {
        char buf[8];
        std::size_t cap = 8;
        for (std::size_t i = 0; i < cap; ++i) {
            buf[i] = 'Q';
        }
        // "ABCDEFGHIJ" is 10 bytes — TooLarge for cap=8.
        bool caught_runtime = false;
        bool caught_format = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
                buf, cap, "ABCDEFGHIJ");
        } catch (const std::format_error& e) {
            caught_format = true;
            (void)e;
        } catch (const std::runtime_error& e) {
            caught_runtime = true;
            (void)e;
        }
        CHECK(caught_runtime);
        CHECK(!caught_format);
    }
#else
    {
        CHECK(true);
    }
#endif

    // ----- Section 16 — DASH-pad return type decltype pin.
    //     The macro's return type matches the dispatched
    //     sibling's return type:
    //       - (0, 0, *, *): std::expected<std::size_t, FormatError>
    //       - (1, 0, *, *): std::size_t
    SECTION("Section 16 - DASH-pad return type decltype pin");
    {
        // The macro is a parenthesized expression; the caller's
        // deduced return type is the macro's expansion's type.
        // We just need the call to type-check.  If the macro
        // expansion is wrong, the decltype below will fail to
        // compile.
#if PETRA_THROW_ON_OVERFLOW == 0
        using ExpectedType = std::expected<std::size_t, petra::FormatError>;
        char buf[8];
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, std::size_t{8}, "x");
        static_assert(
            std::is_same_v<decltype(r), ExpectedType>,
            "RUNTIME macro must return std::expected<std::size_t, FormatError> "
            "when PETRA_THROW_ON_OVERFLOW=0");
        auto r2 = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            buf, std::size_t{8}, "y");
        static_assert(
            std::is_same_v<decltype(r2), ExpectedType>,
            "CONSTEVAL macro must return std::expected<std::size_t, FormatError> "
            "when PETRA_THROW_ON_OVERFLOW=0");
#else
        using SizeType = std::size_t;
        char buf[8];
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(
            buf, std::size_t{8}, "x");
        static_assert(
            std::is_same_v<decltype(r), SizeType>,
            "RUNTIME macro must return std::size_t "
            "when PETRA_THROW_ON_OVERFLOW=1");
        auto r2 = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(
            buf, std::size_t{8}, "y");
        static_assert(
            std::is_same_v<decltype(r2), SizeType>,
            "CONSTEVAL macro must return std::size_t "
            "when PETRA_THROW_ON_OVERFLOW=1");
#endif
        CHECK(true);
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
