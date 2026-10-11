// P-2026-10-11 — try_format_hash_pad_span_n:
//
// Closes Oct 9's "Where we go next" item (1) verbatim — the
// STD::SPAN<CHAR> MIRROR of Oct 9's try_format_hash_pad_n.  TODAY
// introduces a NEW FIFTH compile-time flag (PETRA_HASH_PAD) on
// top of Oct 5's 4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD, PETRA_DASH_PAD) on the STD::SPAN<CHAR> ×
// BOUNDED-BUFFER axis.  PETRA_HASH_PAD=1 selects the HASH-padded
// span<char> sibling, which fills bytes [written, span.size())
// with '#' on success.
//
// The new compile-time flag quintuple
// (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
// PETRA_DASH_PAD, PETRA_HASH_PAD) controls the dispatch via
// #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
// PETRA_SPACE_PAD × #if PETRA_DASH_PAD × #if PETRA_HASH_PAD:
//
//   - (0,0,0,0,0): expected+untouched span<char> sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//      RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   - (0,0,1,0,0): expected+SPACE-padded span<char> sibling
//     (returns expected<size_t, FormatError>; bytes [written,
//     span.size()) filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                (Sep 28)
//      RUNTIME:   petra::try_format_runtime_n_space_pad_span (Sep 28)
//
//   - (0,0,0,1,0): expected+DASH-padded span<char> sibling
//     (returns expected<size_t, FormatError>; bytes [written,
//     span.size()) filled with '-' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                (Oct 4)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4)
//
//   - (0,0,1,1,0): expected+SPACE+DASH-padded span<char> sibling
//     (returns expected<size_t, FormatError>; bytes [written,
//     span.size()) filled with '-' on success — the DASH fill
//     OVERWRITES the SPACE fill, so the (0,0,1,1,0) cell REUSES
//     the (0,0,0,1,0) DASH-pad span<char> sibling wrapper
//     verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                (Oct 4, REUSED across the (0,0,*,1,0) cells)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad_span
//                (Oct 4, REUSED across the (0,0,*,1,0) cells)
//
//   - (0,0,0,0,1): expected+HASH-padded span<char> sibling (NEW
//     today; returns expected<size_t, FormatError>; bytes
//     [written, span.size()) filled with '#' on success — wraps
//     Oct 4's expected+DASH-padded span<char> sibling, and the
//     HASH fill OVERWRITES the DASH fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_hash_pad_span
//                (NEW today)
//      RUNTIME:   petra::try_format_runtime_n_hash_pad_span
//                (NEW today)
//
//   - (0,0,0,1,1): expected+DASH+HASH-padded span<char> sibling
//     (NEW today; returns expected<size_t, FormatError>; bytes
//     [written, span.size()) filled with '#' on success — the
//     HASH fill OVERWRITES the DASH fill, so the (0,0,0,1,1)
//     cell REUSES the (0,0,0,0,1) HASH-pad span<char> sibling
//     wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_hash_pad_span
//                (NEW today, REUSED across the (0,0,0,1,*) cells)
//      RUNTIME:   petra::try_format_runtime_n_hash_pad_span
//                (NEW today, REUSED across the (0,0,0,1,*) cells)
//
//   - (0,0,1,0,1): expected+SPACE+HASH-padded span<char> sibling
//     (NEW today; returns expected<size_t, FormatError>; bytes
//     [written, span.size()) filled with '#' on success — the
//     HASH fill OVERWRITES both the SPACE fill and the DASH fill
//     (no DASH fill actually happened, so this composes
//     transitively as HASH > SPACE; the cell REUSES the
//     (0,0,0,0,1) HASH-pad span<char> sibling wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_hash_pad_span
//                (NEW today, REUSED across the (0,0,*,*,1) cells)
//      RUNTIME:   petra::try_format_runtime_n_hash_pad_span
//                (NEW today, REUSED across the (0,0,*,*,1) cells)
//
//   - (0,0,1,1,1): expected+SPACE+DASH+HASH-padded span<char>
//     sibling (NEW today; returns expected<size_t, FormatError>;
//     bytes [written, span.size()) filled with '#' on success —
//     the HASH fill OVERWRITES the DASH fill which OVERWROTE the
//     SPACE fill; the (0,0,1,1,1) cell REUSES the (0,0,0,0,1)
//     HASH-pad span<char> sibling wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_hash_pad_span
//                (NEW today, REUSED across all 4 HASH_PAD=1 cells)
//      RUNTIME:   petra::try_format_runtime_n_hash_pad_span
//                (NEW today, REUSED across all 4 HASH_PAD=1 cells)
//
//   - (1,0,0,0,0): throwing+untouched span<char> sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, span.size()) UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                (Sep 21)
//      RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                (Sep 21)
//
//   - (1,0,1,0,0): throwing+SPACE-padded span<char> sibling
//     (returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, span.size()) filled with '
//     ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                (Sep 28)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                (Sep 28)
//
//   - (1,0,0,1,0): throwing+DASH-padded span<char> sibling
//     (returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, span.size()) filled with '-'
//     on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                (Oct 4)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                (Oct 4)
//
//   - (1,0,1,1,0): throwing+SPACE+DASH-padded span<char> sibling
//     (the (1,0,1,1,0) cell REUSES the (1,0,0,1,0) DASH-pad
//     span<char> sibling wrapper verbatim; DASH overwrites SPACE):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED across the (1,0,*,1,0) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED across the (1,0,*,1,0) cells)
//
//   - (1,0,0,0,1): throwing+HASH-padded span<char> sibling (NEW
//     today; returns std::size_t on success, throws
//     std::runtime_error on overflow; bytes [written,
//     span.size()) filled with '#' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_hash_pad_span
//                (NEW today)
//      RUNTIME:   petra::try_format_runtime_n_throwing_hash_pad_span
//                (NEW today)
//
//   - (1,0,0,1,1): throwing+DASH+HASH-padded span<char> sibling
//     (NEW today; REUSES the (1,0,0,0,1) HASH-pad span<char>
//     sibling wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_hash_pad_span
//                (NEW today, REUSED across the (1,0,0,1,*) cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_hash_pad_span
//                (NEW today, REUSED across the (1,0,0,1,*) cells)
//
//   - (1,0,1,0,1): throwing+SPACE+HASH-padded span<char> sibling
//     (NEW today; REUSES the (1,0,0,0,1) HASH-pad span<char>
//     sibling wrapper verbatim — HASH overwrites SPACE since no
//     DASH):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_hash_pad_span
//                (NEW today, REUSED across all 4 throwing+HASH cells)
//      RUNTIME:   petra::try_format_runtime_n_throwing_hash_pad_span
//                (NEW today, REUSED across all 4 throwing+HASH cells)
//
//   - (1,0,1,1,1): throwing+SPACE+DASH+HASH-padded span<char>
//     sibling (NEW today; REUSES the (1,0,0,0,1) HASH-pad
//     span<char> sibling wrapper verbatim — transitive
//     composition: HASH overwrites DASH overwrites SPACE):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_hash_pad_span
//                (NEW today, REUSED)
//      RUNTIME:   petra::try_format_runtime_n_throwing_hash_pad_span
//                (NEW today, REUSED)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0, PETRA_HASH_PAD = 0
// (expected+untouched — the prior contract across Sep 6
// through Oct 9).
//
// KEY DESIGN DECISION — the HASH-pad span<char> sibling is a thin
// layer on top of Oct 4's DASH-pad span<char> sibling.  The
// composition rule "the LATER fill overwrites the EARLIER fill"
// applies:
//   1. The HASH-pad wrapper calls the DASH-pad span<char>
//      sibling (Oct 4's wrapper), which fills bytes [written,
//      span.size()) with '-' on success (via the underlying
//      SPACE-pad span<char> sibling, which fills with ' ', then
//      OVERWRITES with '-' on the DASH-pad path).
//   2. On success, the HASH-pad wrapper OVERWRITES those '-'
//      bytes with '#' (the LATER fill).
//   3. On error (Format or TooLarge), the HASH-pad wrapper does
//      NOT touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
// Because the HASH-pad span<char> wrapper REUSES the DASH-pad
// span<char> sibling verbatim, ALL FOUR (PAD=0, *, DASH=*, HASH=1)
// cells on the expected side AND ALL FOUR (PAD=0, *, DASH=*,
// HASH=1) cells on the throwing side share the SAME two HASH-pad
// span<char> sibling wrappers (one CONSTEVAL+RUNTIME expected
// pair, one CONSTEVAL+RUNTIME throwing pair) — the HASH-overwrites-
// DASH composition rule applies transitively.
//
// SCOPE NOTE: This lesson is FOCUSED on the EIGHTH axis
// (PETRA_HASH_PAD) ONLY on the PETRA_PAD=0 side (mirrors Oct 9's
// lesson scope).  The (PAD=1) cells require Sep 25's / Sep 26's /
// Sep 27's / Oct 1's / Oct 5's / Oct 8's NUL-pad wrappers, which
// are NOT included in this file.  When PETRA_PAD = 1, the
// lesson's dispatcher #errors with a clear pointer to those
// lessons.
//
// PLATFORM NOTE: This file uses Oct 3's / Oct 4's / Oct 9's PROVEN
// callback-based impl (the `cb` lambda + `std::move(counter)`
// pattern) — NOT the no-callback `counter.count()` pattern.  The
// no-callback pattern was found to NOT work on Apple Clang 21 /
// libc++ 21 because std::vformat_to internally COPIES the output
// iterator (the proxy's `it_` pointer ends up pointing to a copy,
// not the original), so `counter.count()` and `writer.written()`
// read 0 after the vformat_to call even though the proxy's
// operator= was invoked (the count updates went to the discarded
// copy).  The callback pattern avoids this because the callback
// writes through a `void* data` pointer to a local `std::size_t
// needed` / `std::size_t written`, which the vformat_to copies do
// NOT see — only the local in the impl is updated.  See
// /tmp/probe_vformat_to_copy.cpp for the full investigation.

#include <array>
#include <atomic>
#include <cstdio>
#include <cstddef>
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
// Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9)
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
// Part 2 — petra::counting_output_iterator (Oct 3 / Oct 4 / Oct 8 / Oct 9
// verbatim — callback-based)
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
// Part 3 — petra::detail::bounded_char_writer (Oct 3 / Oct 4 / Oct 8 / Oct 9
// verbatim — callback-based, increments `written_` ONLY on successful write)
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
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim).
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
// petra::try_format_bounded_runtime_n (Sep 9 verbatim char*+cap) +
// petra::try_format_runtime_n(span<char>) (Sep 19 verbatim) +
// petra::try_format_bounded_runtime_n_span (Sep 10 verbatim).
//
// These are the expected+untouched siblings on the CHAR*+CAP axis
// AND the STD::SPAN<CHAR> axis.  TODAY's NEW HASH-pad span<char>
// sibling wrappers (Part 9) REUSE these wrappers indirectly via
// Oct 4's DASH-padded span<char> siblings (Part 8) which reuse
// Sep 28's SPACE-padded span<char> siblings (Part 7).
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
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
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
// the STD::SPAN<CHAR> axis.  TODAY's NEW throwing+HASH-pad span<char>
// sibling wrappers (Part 9) REUSE these wrappers indirectly via
// Oct 4's throwing+DASH-padded span<char> siblings which reuse Sep
// 28's throwing+SPACE-padded span<char> siblings.
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
// TODAY's NEW HASH-pad span<char> sibling wrappers (Part 9) REUSE
// these SPACE-padded span<char> siblings INDIRECTLY via Oct 4's
// DASH-padded span<char> siblings (Part 8) which reuse these
// SPACE-padded span<char> siblings verbatim.  The HASH-pad wrapper
// (today) calls the DASH-pad span<char> sibling (Oct 4), which fills
// [written, span.size()) with '-' on success; then the HASH-pad
// wrapper OVERWRITES those '-' bytes with '#' on success ONLY.
//
// KEY INSIGHT: the (HASH_PAD=1, DASH_PAD=1) cells REUSE today's
// HASH-pad wrappers (NOT these SPACE-pad wrappers) verbatim because
// HASH overwrites DASH when both are set.  Likewise the
// (HASH_PAD=1, DASH_PAD=0, SPACE_PAD=1) and
// (HASH_PAD=1, DASH_PAD=1, SPACE_PAD=1) cells REUSE today's
// HASH-pad wrappers via the transitive composition rule
// HASH > DASH > SPACE.  See Section 1's "HASH × DASH_PAD
// composition rule" and Section 9's "HASH-overwrites-DASH
// composition" pinning below.
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
// Part 8 — petra::try_format_bounded_runtime_n_dash_pad_span (Oct 4
// verbatim) + petra::try_format_runtime_n_dash_pad_span (Oct 4
// verbatim) + petra::try_format_bounded_runtime_n_throwing_dash_pad_span
// (Oct 4 verbatim) + petra::try_format_runtime_n_throwing_dash_pad_span
// (Oct 4 verbatim).
//
// Oct 4's DASH-pad span<char> sibling wrappers are thin layers on
// top of Sep 28's SPACE-pad span<char> siblings.  TODAY's NEW
// HASH-pad span<char> sibling wrappers (Part 9) REUSE these
// DASH-pad span<char> siblings — the HASH-pad wrapper calls the
// DASH-pad sibling (Oct 4's wrapper), which fills bytes [written,
// span.size()) with '-' on success; then the HASH-pad wrapper
// OVERWRITES those '-' bytes with '#' on success ONLY.
//
// The composition rule "the LATER fill overwrites the EARLIER
// fill" applies:
//   1. The HASH-pad wrapper calls the DASH-pad span<char> sibling,
//      which fills bytes [written, span.size()) with '-' on success
//      (or short-circuits on error).
//   2. On success, the HASH-pad wrapper OVERWRITES those '-'
//      bytes with '#' (the LATER fill).
//   3. On error (Format or TooLarge), the HASH-pad wrapper does
//      NOT touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
//
// (active when PETRA_PAD=0 AND PETRA_DASH_PAD=1):
//   - (0, 0, 0, 1) expected+DASH-padded span<char> sibling (RUNTIME
//     surface): petra::try_format_runtime_n_dash_pad_span
//     (wraps Sep 28's try_format_runtime_n_space_pad_span verbatim
//      and dash-fills [written, span.size()) with '-' on success).
//   - (0, 0, 0, 1) expected+DASH-padded span<char> sibling
//     (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_dash_pad_span
//     (wraps Sep 28's try_format_bounded_runtime_n_space_pad_span
//      verbatim and dash-fills [written, span.size()) with '-' on
//      success).
//   - (1, 0, 0, 1) throwing+DASH-padded span<char> sibling (RUNTIME
//     surface): petra::try_format_runtime_n_throwing_dash_pad_span
//     (wraps Sep 28's try_format_runtime_n_throwing_space_pad_span
//      verbatim and dash-fills [written, span.size()) with '-' on
//      success).
//   - (1, 0, 0, 1) throwing+DASH-padded span<char> sibling
//     (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_throwing_dash_pad_span
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

// Oct 4 verbatim — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_dash_pad_span(std::span<char> out, std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_space_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '-';
        }
    }
    return r;
}

// Oct 4 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the span<char> axis.
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

// Oct 4 verbatim — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the span<char> axis.
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

// Oct 4 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the span<char> axis.
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
// Part 9 — petra::try_format_runtime_n_hash_pad_span (NEW today) +
// petra::try_format_bounded_runtime_n_hash_pad_span (NEW today) +
// petra::try_format_runtime_n_throwing_hash_pad_span (NEW today) +
// petra::try_format_bounded_runtime_n_throwing_hash_pad_span (NEW today).
//
// TODAY's NEW HASH-pad span<char> sibling wrappers are thin
// layers on top of Oct 4's DASH-padded span<char> siblings.  The
// composition rule "the LATER fill overwrites the EARLIER fill"
// applies:
//   1. The HASH-pad wrapper calls the DASH-pad span<char> sibling,
//      which fills bytes [written, span.size()) with '-' on success
//      (or short-circuits on error).
//   2. On success, the HASH-pad wrapper OVERWRITES those '-' bytes
//      with '#' (the LATER fill).
//   3. On error (Format or TooLarge), the HASH-pad wrapper does NOT
//      touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
//
// Because the HASH-pad span<char> wrapper REUSES the DASH-pad
// span<char> sibling verbatim, all 4 (PAD=0, *, DASH=*, HASH=1)
// cells on the expected side AND all 4 (PAD=0, *, DASH=*, HASH=1)
// cells on the throwing side share the SAME two HASH-pad span<char>
// sibling wrappers (one expected pair, one throwing pair) — the
// HASH-overwrites-DASH composition rule applies transitively to
// HASH overwrites SPACE → DASH, since the DASH-pad sibling already
// OVERWRITES the SPACE fill internally.
//
// (active when PETRA_PAD=0 AND PETRA_HASH_PAD=1):
//   - (0, 0, 0, 0, 1) expected+HASH-padded span<char> sibling
//     (RUNTIME surface):
//     petra::try_format_runtime_n_hash_pad_span
//     (wraps Oct 4's try_format_runtime_n_dash_pad_span verbatim
//      and hash-fills [written, span.size()) with '#' on success).
//   - (0, 0, 0, 0, 1) expected+HASH-padded span<char> sibling
//     (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_hash_pad_span
//     (wraps Oct 4's try_format_bounded_runtime_n_dash_pad_span
//      verbatim and hash-fills [written, span.size()) with '#' on
//      success).
//   - (1, 0, 0, 0, 1) throwing+HASH-padded span<char> sibling
//     (RUNTIME surface):
//     petra::try_format_runtime_n_throwing_hash_pad_span
//     (wraps Oct 4's try_format_runtime_n_throwing_dash_pad_span
//      verbatim and hash-fills [written, span.size()) with '#' on
//      success).
//   - (1, 0, 0, 0, 1) throwing+HASH-padded span<char> sibling
//     (CONSTEVAL surface):
//     petra::try_format_bounded_runtime_n_throwing_hash_pad_span
//     (wraps Oct 4's
//      try_format_bounded_runtime_n_throwing_dash_pad_span verbatim
//      and hash-fills [written, span.size()) with '#' on success).
//
// (active when PETRA_PAD=0 AND PETRA_SPACE_PAD=1 AND
//  PETRA_HASH_PAD=1 — REUSES (0, 0, 0, 0, 1) wrapper):
//   - (0, 0, 1, 0, 1) expected+SPACE+HASH-padded span<char>
//     sibling: REUSES the (0, 0, 0, 0, 1) HASH-pad span<char>
//     sibling wrapper verbatim (HASH > SPACE — the HASH fill
//     overwrites the SPACE fill transitively).
//   - (1, 0, 1, 0, 1) throwing+SPACE+HASH-padded span<char>
//     sibling: REUSES the (1, 0, 0, 0, 1) HASH-pad span<char>
//     sibling wrapper verbatim.
//
// (active when PETRA_PAD=0 AND PETRA_DASH_PAD=1 AND
//  PETRA_HASH_PAD=1 — REUSES (0, 0, 0, 0, 1) wrapper):
//   - (0, 0, 0, 1, 1) expected+DASH+HASH-padded span<char>
//     sibling: REUSES the (0, 0, 0, 0, 1) HASH-pad span<char>
//     sibling wrapper verbatim (HASH overwrites DASH when both
//     are set — the composition rule).
//   - (1, 0, 0, 1, 1) throwing+DASH+HASH-padded span<char>
//     sibling: REUSES the (1, 0, 0, 0, 1) HASH-pad span<char>
//     sibling wrapper verbatim.
//
// (active when PETRA_PAD=0 AND PETRA_SPACE_PAD=1 AND
//  PETRA_DASH_PAD=1 AND PETRA_HASH_PAD=1 — REUSES
//  (0, 0, 0, 0, 1) wrapper via transitive composition):
//   - (0, 0, 1, 1, 1) expected+SPACE+DASH+HASH-padded span<char>
//     sibling: REUSES the (0, 0, 0, 0, 1) HASH-pad span<char>
//     sibling wrapper verbatim (HASH overwrites DASH overwrites
//     SPACE — the transitive composition rule).
//   - (1, 0, 1, 1, 1) throwing+SPACE+DASH+HASH-padded span<char>
//     sibling: REUSES the (1, 0, 0, 0, 1) HASH-pad span<char>
//     sibling wrapper verbatim.
// ============================================================================

namespace petra {

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// HASH-PAD sibling on the span<char> axis.  Wraps Oct 4's
// try_format_runtime_n_dash_pad_span verbatim and hash-fills
// [written, span.size()) with '#' on success ONLY (the hash-fill
// loop runs only when the DASH-pad span<char> sibling returned
// normally).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_hash_pad_span(std::span<char> out,
                                    std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_dash_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        // LATER fill (HASH) overwrites EARLIER fill (DASH) from
        // the DASH-pad span<char> sibling.  On error, r.has_value()
        // is false and the loop is skipped, preserving the buffer's
        // atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '#';
        }
    }
    return r;
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// HASH-PAD sibling on the span<char> axis.  Wraps Oct 4's
// try_format_bounded_runtime_n_dash_pad_span verbatim and
// hash-fills [written, span.size()) with '#' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_hash_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_dash_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '#';
        }
    }
    return r;
}

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// HASH-PAD sibling on the span<char> axis.  Wraps Oct 4's
// try_format_runtime_n_throwing_dash_pad_span verbatim and
// hash-fills [written, span.size()) with '#' on success ONLY
// (control does NOT reach the hash-fill loop on error — the catch
// block re-raises the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_hash_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_dash_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '#';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// HASH-PAD sibling on the span<char> axis.  Wraps Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span verbatim
// and hash-fills [written, span.size()) with '#' on success ONLY
// (control does NOT reach the hash-fill loop on error — the catch
// block re-raises the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_hash_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_dash_pad_span(
                out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '#';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 10 — TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW
// (Mirrors Oct 4's / Oct 9's shape verbatim; now with PETRA_HASH_PAD
// as a FIFTH axis.  The compile-time flag quintuple
// (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
// PETRA_DASH_PAD, PETRA_HASH_PAD) controls the dispatch via
// #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
// PETRA_SPACE_PAD × #if PETRA_DASH_PAD × #if PETRA_HASH_PAD.)
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling —
// matches the prior contract across Sep 6 through Oct 9).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

// PETRA_PAD defaults to 0 (no NUL-pad — matches the prior
// contract across Sep 6 through Oct 9).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (no NUL-pad) or 1 (NUL-padded)"
#endif

// PETRA_SPACE_PAD defaults to 0 (no space-pad — matches the
// prior contract across Sep 6 through Oct 9).
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

// Sanity check: PETRA_SPACE_PAD must be 0 or 1.
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

// PETRA_DASH_PAD defaults to 0 (no dash-pad — matches the
// prior contract across Sep 6 through Oct 9).
#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif

// Sanity check: PETRA_DASH_PAD must be 0 or 1.
#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 (no dash-pad) or 1 (dash-padded)"
#endif

// PETRA_HASH_PAD defaults to 0 (no hash-pad — matches the
// prior contract across Sep 6 through Oct 9).
#ifndef PETRA_HASH_PAD
#define PETRA_HASH_PAD 0
#endif

// Sanity check: PETRA_HASH_PAD must be 0 or 1.
#if PETRA_HASH_PAD != 0 && PETRA_HASH_PAD != 1
#error "PETRA_HASH_PAD must be 0 (no hash-pad) or 1 (hash-padded)"
#endif

// Guard: when PETRA_PAD=1, the lesson's dispatcher does NOT
// include the NUL-pad span<char> sibling wrappers (those live
// in Sep 27's / Oct 1's / Oct 5's / Oct 8's lessons).  The
// dispatcher #errors with a clear pointer to the right lesson.
#if PETRA_PAD != 0
#error "This lesson (try_format_hash_pad_span_n) only supports PETRA_PAD=0.  For PETRA_PAD=1, use Sep 27's try_format_padded_span_n_or_throw (expected+NUL-padded) or Oct 5's try_format_padded_dash_pad_span_n_or_throw (PADxDASH composition) or Oct 8's try_format_padded_dash_pad_n_or_throw (PADxDASH char*+cap mirror)."
#endif

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
// (0, 0, 0, 0, 0) -> expected+untouched span<char> sibling path.
// Returns std::expected<std::size_t, FormatError>.  On TooLarge,
// the underlying impl returns FormatError{TooLarge, ...,
// needed_bytes} UNCHANGED; the buffer is UNTOUCHED on overflow.

// CONSTEVAL surface: dispatches to Sep 10's
// try_format_bounded_runtime_n_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)               \
                                                __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 19's try_format_runtime_n(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n((out), (fmt)                            \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
// (0, 0, 1, 0, 0) -> expected+SPACE-padded span<char> sibling path.
// Returns std::expected<std::size_t, FormatError>.  On success,
// bytes [written, span.size()) are filled with ' '.  On overflow
// or Format, the buffer is UNTOUCHED (atomicity preserved by the
// underlying impl; the space-padded wrapper does NOT touch the
// buffer on error).

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_space_pad_span((out), (fmt)     \
                                                          __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_space_pad_span((out), (fmt)             \
                                                  __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
// (0, 0, 0, 1, 0) -> expected+DASH-padded span<char> sibling path.
// Returns std::expected<std::size_t, FormatError>.  On success,
// bytes [written, span.size()) are filled with '-'.  On overflow
// or Format, the buffer is UNTOUCHED (atomicity preserved by the
// underlying SPACE-pad span<char> sibling; the dash-padded wrapper
// does NOT touch the buffer on error).

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
// (0, 0, 1, 1, 0) -> expected+SPACE+DASH-padded span<char> sibling
// path.  Returns std::expected<std::size_t, FormatError>.  On
// success, bytes [written, span.size()) are filled with '-' (the
// DASH fill OVERWRITES the SPACE fill from the SPACE-pad sibling,
// so the (0,0,1,1,0) cell REUSES the (0,0,0,1,0) DASH-pad
// span<char> sibling wrapper verbatim).  On overflow or Format,
// the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES Oct 4's
// try_format_bounded_runtime_n_dash_pad_span (the (0,0,1,1,0) and
// (0,0,0,1,0) cells share the SAME sibling wrapper because DASH
// overwrites SPACE when both are set -- the composition rule).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 4's try_format_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
// (0, 0, 0, 0, 1) -> expected+HASH-padded span<char> sibling path
// (NEW today).  Returns std::expected<std::size_t, FormatError>.
// On success, bytes [written, span.size()) are filled with '#'
// (the HASH fill OVERWRITES the DASH fill from the DASH-pad
// span<char> sibling at the wrapper level -- the DASH-pad sibling
// internally filled with '-' which OVERWROTE the SPACE fill from
// the SPACE-pad sibling, so the entire chain ends at '#').  On
// overflow or Format, the buffer is UNTOUCHED.

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_hash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_hash_pad_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_hash_pad_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 1
// (0, 0, 0, 1, 1) -> expected+DASH+HASH-padded span<char> sibling
// path (NEW today).  Returns std::expected<std::size_t,
// FormatError>.  On success, bytes [written, span.size()) are
// filled with '#' (the HASH fill OVERWRITES the DASH fill from
// the DASH-pad span<char> sibling, so the (0,0,0,1,1) cell REUSES
// the (0,0,0,0,1) HASH-pad span<char> sibling wrapper verbatim).
// On overflow or Format, the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_hash_pad_span (the (0,0,0,1,1) and
// (0,0,0,0,1) cells share the SAME sibling wrapper because HASH
// overwrites DASH when both are set -- the composition rule).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_hash_pad_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW
// try_format_runtime_n_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_hash_pad_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
// (0, 0, 1, 0, 1) -> expected+SPACE+HASH-padded span<char> sibling
// path (NEW today).  Returns std::expected<std::size_t,
// FormatError>.  On success, bytes [written, span.size()) are
// filled with '#' (the HASH fill OVERWRITES the SPACE fill at the
// wrapper level -- the HASH fill overwrites the DASH fill (which
// the DASH-pad sibling would have done with '-') which OVERWROTE
// the SPACE fill; the (0,0,1,0,1) cell REUSES the (0,0,0,0,1)
// HASH-pad span<char> sibling wrapper verbatim via the transitive
// composition rule HASH > DASH > SPACE, where no DASH fill
// actually occurs but the final state is still '#').  On overflow
// or Format, the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_hash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_hash_pad_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW try_format_runtime_n_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_hash_pad_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0
       //   && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
       //   && PETRA_HASH_PAD == 1
// (0, 0, 1, 1, 1) -> expected+SPACE+DASH+HASH-padded span<char>
// sibling path (NEW today).  Returns std::expected<std::size_t,
// FormatError>.  On success, bytes [written, span.size()) are
// filled with '#' (the HASH fill OVERWRITES the DASH fill which
// OVERWROTE the SPACE fill -- the transitive composition rule
// HASH > DASH > SPACE).  The (0,0,1,1,1) cell REUSES the
// (0,0,0,0,1) HASH-pad span<char> sibling wrapper verbatim.  On
// overflow or Format, the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_hash_pad_span (the (0,0,1,1,1) and
// (0,0,0,0,1) cells share the SAME sibling wrapper via the
// transitive composition rule HASH > DASH > SPACE).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_hash_pad_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW try_format_runtime_n_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_hash_pad_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW == 0 ... PETRA_HASH_PAD
        //         chain

// ============================================================================
// Second chain -- PETRA_THROW_ON_OVERFLOW == 1 (throwing sibling).
// Outer guard so the throwing chain's #else catch-all (for the
// (1,0,1,1,1) cell) ONLY fires when PETRA_THROW_ON_OVERFLOW == 1,
// NOT when PETRA_THROW_ON_OVERFLOW == 0 (which is handled by the
// first chain above).  Without this guard, building with
// PETRA_THROW_ON_OVERFLOW == 0 triggers the second chain's #else
// clause (because all of its #if / #elif conditions include
// PETRA_THROW_ON_OVERFLOW == 1), which would REDEFINE the macro
// to the throwing sibling and clobber the first chain's correct
// dispatch.
// ============================================================================

#if PETRA_THROW_ON_OVERFLOW == 1
#if PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
// (1, 0, 0, 0, 0) -> throwing+untouched span<char> sibling path.
// Returns std::size_t on success.  On TooLarge, throws
// std::runtime_error carrying the SAME .what() message that
// FormatError{TooLarge, ...} would carry.  The buffer is
// UNTOUCHED on overflow (atomicity preserved by Sep 10's / Sep
// 19's / Sep 21's underlying impl).

// CONSTEVAL surface: dispatches to Sep 21's
// try_format_bounded_runtime_n_throwing_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)      \
                                                         __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 21's
// try_format_runtime_n_throwing(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                   \
                                             __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
// (1, 0, 1, 0, 0) -> throwing+SPACE-padded span<char> sibling
// path.  Returns std::size_t on success.  On success, bytes
// [written, span.size()) are filled with ' '.  On overflow or
// Format, throws std::runtime_error; the buffer is UNTOUCHED
// (atomicity preserved by the underlying throwing wrapper --
// Sep 21's wrapper already delegates atomicity to Sep 10's /
// Sep 19's underlying impl, and Sep 28's SPACE-pad span<char>
// wrapper does NOT touch the buffer on error).

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad_span(        \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_space_pad_span((out), (fmt)    \
                                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
// (1, 0, 0, 1, 0) -> throwing+DASH-padded span<char> sibling
// path.  Returns std::size_t on success.  On success, bytes
// [written, span.size()) are filled with '-'.  On overflow or
// Format, throws std::runtime_error; the buffer is UNTOUCHED
// (atomicity preserved by the underlying SPACE-pad throwing
// wrapper -- delegated to Sep 28's wrapper, which delegates to
// Sep 21's throwing wrapper, which delegates to Sep 10's / Sep
// 19's underlying impl; Oct 4's DASH-pad span<char> wrapper
// does NOT touch the buffer on error).

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)     \
                                                         __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
// (1, 0, 1, 1, 0) -> throwing+SPACE+DASH-padded span<char>
// sibling path.  Returns std::size_t on success.  On success,
// bytes [written, span.size()) are filled with '-' (the DASH
// fill OVERWRITES the SPACE fill from the SPACE-pad sibling, so
// the (1,0,1,1,0) cell REUSES the (1,0,0,1,0) DASH-pad
// span<char> sibling wrapper verbatim).  On overflow or Format,
// throws std::runtime_error; the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span (the
// (1,0,1,1,0) and (1,0,0,1,0) cells share the SAME sibling
// wrapper because DASH overwrites SPACE when both are set --
// the composition rule).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES Oct 4's
// try_format_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)     \
                                                         __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
// (1, 0, 0, 0, 1) -> throwing+HASH-padded span<char> sibling path
// (NEW today).  Returns std::size_t on success.  On success,
// bytes [written, span.size()) are filled with '#'.  On overflow
// or Format, throws std::runtime_error; the buffer is UNTOUCHED
// (atomicity preserved by the underlying DASH-pad throwing
// span<char> wrapper -- delegated to Oct 4's wrapper, which
// delegates to Sep 28's throwing+SPACE-padded span<char> wrapper,
// which delegates to Sep 21's throwing wrapper, which delegates
// to Sep 10's / Sep 19's underlying impl; TODAY's NEW HASH-pad
// span<char> wrapper does NOT touch the buffer on error).

// CONSTEVAL surface: dispatches to TODAY's NEW
// try_format_bounded_runtime_n_throwing_hash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_hash_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to TODAY's NEW
// try_format_runtime_n_throwing_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_hash_pad_span((out), (fmt)     \
                                                         __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 1
// (1, 0, 0, 1, 1) -> throwing+DASH+HASH-padded span<char> sibling
// path (NEW today).  Returns std::size_t on success.  On success,
// bytes [written, span.size()) are filled with '#' (the HASH
// fill OVERWRITES the DASH fill from the DASH-pad span<char>
// sibling, so the (1,0,0,1,1) cell REUSES the (1,0,0,0,1)
// HASH-pad span<char> sibling wrapper verbatim).  On overflow or
// Format, throws std::runtime_error; the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_throwing_hash_pad_span (the
// (1,0,0,1,1) and (1,0,0,0,1) cells share the SAME sibling
// wrapper because HASH overwrites DASH when both are set -- the
// composition rule).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_hash_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW
// try_format_runtime_n_throwing_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_hash_pad_span((out), (fmt)     \
                                                         __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
// (1, 0, 1, 0, 1) -> throwing+SPACE+HASH-padded span<char> sibling
// path (NEW today).  Returns std::size_t on success.  On success,
// bytes [written, span.size()) are filled with '#' (the HASH
// fill OVERWRITES both the DASH fill (which the DASH-pad sibling
// would have done with '-') and the SPACE fill; the (1,0,1,0,1)
// cell REUSES the (1,0,0,0,1) HASH-pad span<char> sibling wrapper
// verbatim via the transitive composition rule HASH > DASH >
// SPACE -- no DASH fill actually occurs but the final state is
// still '#').  On overflow or Format, throws std::runtime_error;
// the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_throwing_hash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_hash_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW
// try_format_runtime_n_throwing_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_hash_pad_span((out), (fmt)     \
                                                         __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0
       //   && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
       //   && PETRA_HASH_PAD == 1
// (1, 0, 1, 1, 1) -> throwing+SPACE+DASH+HASH-padded span<char>
// sibling path (NEW today).  Returns std::size_t on success.  On
// success, bytes [written, span.size()) are filled with '#' (the
// HASH fill OVERWRITES the DASH fill which OVERWROTE the SPACE
// fill -- the transitive composition rule HASH > DASH > SPACE).
// The (1,0,1,1,1) cell REUSES the (1,0,0,0,1) HASH-pad
// span<char> sibling wrapper verbatim.  On overflow or Format,
// throws std::runtime_error; the buffer is UNTOUCHED.

// CONSTEVAL surface: REUSES TODAY's NEW
// try_format_bounded_runtime_n_throwing_hash_pad_span (the
// (1,0,1,1,1) and (1,0,0,0,1) cells share the SAME sibling
// wrapper via the transitive composition rule HASH > DASH >
// SPACE).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)               \
    (::petra::try_format_bounded_runtime_n_throwing_hash_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: REUSES TODAY's NEW
// try_format_runtime_n_throwing_hash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)       \
    (::petra::try_format_runtime_n_throwing_hash_pad_span((out), (fmt)     \
                                                         __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW == 1 ... PETRA_HASH_PAD
        //         chain

#endif  // PETRA_THROW_ON_OVERFLOW == 1 outer guard

// ============================================================================
// Part 11 -- hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
// Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 /
// Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 /
// Oct 4 / Oct 5 / Oct 8 / Oct 9).
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
// Part 12 -- sections
// ============================================================================

int main() {
    // ----- Section 1 -- (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    //       PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD)
    //       flag quintuple pinned.
    SECTION("Section 1 - flag quintuple pinned");
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                      || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
    static_assert(PETRA_PAD == 0 || PETRA_PAD == 1,
                  "PETRA_PAD must be 0 or 1");
    static_assert(PETRA_SPACE_PAD == 0 || PETRA_SPACE_PAD == 1,
                  "PETRA_SPACE_PAD must be 0 or 1");
    static_assert(PETRA_DASH_PAD == 0 || PETRA_DASH_PAD == 1,
                  "PETRA_DASH_PAD must be 0 or 1");
    static_assert(PETRA_HASH_PAD == 0 || PETRA_HASH_PAD == 1,
                  "PETRA_HASH_PAD must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0, PETRA_HASH_PAD=0 "
        "(expected+untouched span<char> sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0, PETRA_HASH_PAD=0 "
        "(expected+SPACE-padded span<char> sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1, PETRA_HASH_PAD=0 "
        "(expected+DASH-padded span<char> sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1, PETRA_HASH_PAD=0 "
        "(expected+SPACE+DASH-padded span<char> sibling -- "
        "DASH overwrites SPACE)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0, PETRA_HASH_PAD=1 "
        "(expected+HASH-padded span<char> sibling -- NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1, PETRA_HASH_PAD=1 "
        "(expected+DASH+HASH-padded span<char> sibling -- "
        "HASH overwrites DASH; NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0, PETRA_HASH_PAD=1 "
        "(expected+SPACE+HASH-padded span<char> sibling -- "
        "HASH overwrites SPACE transitively; NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0, PETRA_HASH_PAD=0 "
        "(throwing+untouched span<char> sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0, PETRA_HASH_PAD=0 "
        "(throwing+SPACE-padded span<char> sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1, PETRA_HASH_PAD=0 "
        "(throwing+DASH-padded span<char> sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1, PETRA_HASH_PAD=0 "
        "(throwing+SPACE+DASH-padded span<char> sibling -- "
        "DASH overwrites SPACE)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0, PETRA_HASH_PAD=1 "
        "(throwing+HASH-padded span<char> sibling -- NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1, PETRA_HASH_PAD=1 "
        "(throwing+DASH+HASH-padded span<char> sibling -- "
        "HASH overwrites DASH; NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0 && PETRA_HASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0, PETRA_HASH_PAD=1 "
        "(throwing+SPACE+HASH-padded span<char> sibling -- "
        "HASH overwrites SPACE transitively; NEW today)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1 && PETRA_HASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1, PETRA_HASH_PAD=0 "
        "(throwing+SPACE+DASH-padded span<char> sibling -- "
        "DASH overwrites SPACE)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1, PETRA_HASH_PAD=1 "
        "(throwing+SPACE+DASH+HASH-padded span<char> sibling -- "
        "HASH overwrites DASH overwrites SPACE transitively; "
        "NEW today)");
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

    // ----- Section 2 -- HASH-padded byte-state contract on success.
    //     On (*, 0, *, 0, 1) [HASH-pad-only]:
    //        bytes [written, span.size()) are '#' on success.
    //     On (*, 0, *, 1, 1) [DASH+HASH-pad]:
    //        bytes [written, span.size()) are '#' on success
    //        (HASH wins).
    //     On (*, 0, 1, *, 1) [any combo with HASH]:
    //        bytes [written, span.size()) are '#' on success
    //        (HASH wins).
    //     On (*, 0, *, 1, 0) [DASH only]: bytes [written,
    //        span.size()) are '-' on success.
    //     On (*, 0, 1, 0, 0) [SPACE only]: bytes [written,
    //        span.size()) are ' ' on success.
    //     On (*, 0, 0, 0, 0) [untouched]: bytes [written,
    //        span.size()) are UNTOUCHED ('Q' poison byte).
    //     On (*, 1, *, *, *) [NUL-pad]: OUT OF SCOPE for this
    //        lesson (lives in Sep 27's / Oct 1's lessons).
    SECTION("Section 2 - HASH-padded byte-state contract on success");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 5;  // "ABCDE"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            // 5-CHECK block: bytes [0, 5) are "ABCDE".
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
#if PETRA_HASH_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all '#' (HASH wins
            // over DASH and SPACE transitively).
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], '#');
            }
#elif PETRA_DASH_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all '-'.
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], '-');
            }
#elif PETRA_SPACE_PAD == 1
            // 11-CHECK block: bytes [5, 16) are all ' '.
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], ' ');
            }
#else
            // 11-CHECK block: bytes [5, 16) are UNTOUCHED ('Q').
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], 'Q');
            }
#endif
        }
#else  // PETRA_THROW_ON_OVERFLOW == 1
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
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
#if PETRA_HASH_PAD == 1
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], '#');
            }
#elif PETRA_DASH_PAD == 1
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], '-');
            }
#elif PETRA_SPACE_PAD == 1
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], ' ');
            }
#else
            for (std::size_t i = expected_written; i < buf.size(); ++i) {
                CHECK_EQ(buf[i], 'Q');
            }
#endif
        }
#endif
    }

    // 2a. HASH-pad sibling: no buffer bleed outside the span (the
    //     "no-bleed" case).  A 6-byte sub-span at offset 5 of a
    //     16-byte backing buffer with format 'ABCDE' (5 bytes)
    //     leaves bytes [0, 5) and [11, 16) as poison 'Q' regardless
    //     of the flag quintuple -- the pad loop only touches bytes
    //     INSIDE the span.
    SECTION("Section 2a - HASH-pad sub-span no-bleed");
    {
        constexpr std::size_t backing = 16;
        char backing_buf[backing];
        for (std::size_t i = 0; i < backing; ++i) {
            backing_buf[i] = 'Q';
        }
        // 6-byte sub-span at &backing_buf[5] (cap=6).  Format
        // "ABCDE" (5 bytes) leaves backing_buf[10] as '#' on the
        // HASH-pad path.  Backing bytes [0, 5) and [11, 16) must
        // remain poison 'Q'.
        std::span<char> sub{backing_buf + 5, 6};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(sub, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{5});
            CHECK_EQ(backing_buf[5], 'A');
            CHECK_EQ(backing_buf[6], 'B');
            CHECK_EQ(backing_buf[7], 'C');
            CHECK_EQ(backing_buf[8], 'D');
            CHECK_EQ(backing_buf[9], 'E');
            // backing_buf[10] is INSIDE the span, gets '#' on HASH-pad
            // path (or '-' on DASH-pad path, etc.).
#if PETRA_HASH_PAD == 1
            CHECK_EQ(backing_buf[10], '#');
#elif PETRA_DASH_PAD == 1
            CHECK_EQ(backing_buf[10], '-');
#elif PETRA_SPACE_PAD == 1
            CHECK_EQ(backing_buf[10], ' ');
#else
            CHECK_EQ(backing_buf[10], 'Q');
#endif
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
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(sub, "ABCDE");
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
#if PETRA_HASH_PAD == 1
            CHECK_EQ(backing_buf[10], '#');
#elif PETRA_DASH_PAD == 1
            CHECK_EQ(backing_buf[10], '-');
#elif PETRA_SPACE_PAD == 1
            CHECK_EQ(backing_buf[10], ' ');
#else
            CHECK_EQ(backing_buf[10], 'Q');
#endif
            for (std::size_t i = 0; i < 5; ++i) {
                CHECK_EQ(backing_buf[i], 'Q');
            }
            for (std::size_t i = 11; i < backing; ++i) {
                CHECK_EQ(backing_buf[i], 'Q');
            }
        }
#endif
    }

    // 2b. Same checks but with a sub-span of size 0 (the
    //     "no-write" case).  The pad loop has i < cap == 0 as
    //     the initial condition, so the loop body never executes.
    SECTION("Section 2b - HASH-pad zero-cap edge case");
    {
        char backing = 'Q';
        std::span<char> empty_sub{&backing, 0};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(empty_sub, "anything");
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        }
        CHECK_EQ(backing, 'Q');
#else
        bool threw = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(empty_sub, "anything");
        } catch (...) {
            threw = true;
        }
        CHECK(threw);
        CHECK_EQ(backing, 'Q');
#endif
    }

    // 2c. Same checks but with a sub-span of size 8 and format
    //     'ABCDEFGH' (full-buffer success; written==span.size()).
    SECTION("Section 2c - HASH-pad full-buffer success");
    {
        char backing[8];
        for (std::size_t i = 0; i < 8; ++i) {
            backing[i] = 'Q';
        }
        std::span<char> buf{backing, 8};
        // "ABCDEFGH" is 8 bytes -- exactly fills span.size()=8.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGH");
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
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGH");
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

    // ----- Section 3 -- HASH-pad buffer atomicity on TooLarge.
    SECTION("Section 3 - HASH-pad buffer atomicity on TooLarge");
    {
        char backing[8];
        std::span<char> buf{backing, 8};
        for (std::size_t i = 0; i < 8; ++i) {
            backing[i] = 'Q';
        }
        // "ABCDEFGHIJ" is 10 bytes -- TooLarge for span.size()=8.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGHIJ");
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        }
        // 8-CHECK block: every byte is still 'Q'.
        for (std::size_t i = 0; i < 8; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
#else
        bool threw = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGHIJ");
        } catch (...) {
            threw = true;
        }
        CHECK(threw);
        for (std::size_t i = 0; i < 8; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
#endif
    }

    // ----- Section 4 -- HASH-pad byte-count contract on success.
    SECTION("Section 4 - HASH-pad byte-count contract on success");
    {
        char backing[32];
        std::span<char> buf{backing, 32};
        // "hello world" is 11 bytes.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "hello world");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{11});
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "hello world");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{11});
#endif
    }

    // ----- Section 5 -- HASH-pad success with format args.
    SECTION("Section 5 - HASH-pad success with format args");
    {
        char backing[32];
        std::span<char> buf{backing, 32};
        // "x=42" is 4 bytes.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "x={}", 42);
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
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "x={}", 42);
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

    // ----- Section 6 -- HASH-pad Format error -> no buffer writes.
    SECTION("Section 6 - HASH-pad Format error leaves buffer untouched");
    {
        char backing[16];
        std::span<char> buf{backing, 16};
        for (std::size_t i = 0; i < 16; ++i) {
            backing[i] = 'Q';
        }
        std::string fmt = "bad {";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, fmt);
        CHECK(!r.has_value());
        if (!r.has_value()) {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
        }
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
#else
        bool threw = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, fmt);
        } catch (...) {
            threw = true;
        }
        CHECK(threw);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
#endif
    }

    // ----- Section 7 -- HASH-pad success with empty format string.
    SECTION("Section 7 - HASH-pad success with empty format string");
    {
        char backing[8];
        std::span<char> buf{backing, 8};
        for (std::size_t i = 0; i < 8; ++i) {
            backing[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{0});
            for (std::size_t i = 0; i < buf.size(); ++i) {
#if PETRA_HASH_PAD == 1
                CHECK_EQ(buf[i], '#');
#elif PETRA_DASH_PAD == 1
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
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{0});
        for (std::size_t i = 0; i < buf.size(); ++i) {
#if PETRA_HASH_PAD == 1
            CHECK_EQ(buf[i], '#');
#elif PETRA_DASH_PAD == 1
            CHECK_EQ(buf[i], '-');
#elif PETRA_SPACE_PAD == 1
            CHECK_EQ(buf[i], ' ');
#else
            CHECK_EQ(buf[i], 'Q');
#endif
        }
#endif
    }

    // ----- Section 8 -- HASH-pad CONSTEVAL surface pin.
    SECTION("Section 8 - HASH-pad CONSTEVAL surface pin");
    {
        char backing[32];
        std::span<char> buf{backing, 32};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "CONSTEVAL test");
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
            n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "CONSTEVAL test");
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

    // ----- Section 9 -- HASH-pad HASH-overwrites-DASH composition.
    //     When BOTH PETRA_DASH_PAD and PETRA_HASH_PAD are 1, the
    //     HASH fill OVERWRITES the DASH fill from the DASH-pad
    //     span<char> sibling.  The end state is bytes [written,
    //     span.size()) = '#'.  This pins the composition rule
    //     "the LATER fill overwrites the EARLIER fill".
    SECTION("Section 9 - HASH-pad HASH-overwrites-DASH composition");
#if PETRA_HASH_PAD == 1 && PETRA_DASH_PAD == 1
    {
        char backing[16];
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 5;  // "ABCDE"
        for (std::size_t i = 0; i < 16; ++i) {
            backing[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
            for (std::size_t i = expected_written; i < 16; ++i) {
                CHECK_EQ(buf[i], '#');
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(buf[i], "ABCDE"[i]);
        }
        for (std::size_t i = expected_written; i < 16; ++i) {
            CHECK_EQ(buf[i], '#');
        }
#endif
    }
#else
    {
        CHECK(true);
    }
#endif

    // ----- Section 10 -- HASH-pad HASH-overwrites-SPACE composition.
    //     When BOTH PETRA_SPACE_PAD and PETRA_HASH_PAD are 1 (and
    //     PETRA_DASH_PAD=0), the HASH fill OVERWRITES the SPACE fill
    //     at the wrapper level -- the HASH fill overwrites the DASH
    //     fill (which the DASH-pad sibling would have done with
    //     '-') which OVERWROTE the SPACE fill; the (0,0,1,0,1) and
    //     (1,0,1,0,0,1) cells REUSE the (0,0,0,0,1) and (1,0,0,0,1)
    //     HASH-pad span<char> sibling wrappers verbatim via the
    //     transitive composition rule HASH > DASH > SPACE.
    SECTION("Section 10 - HASH-pad HASH-overwrites-SPACE composition");
#if PETRA_HASH_PAD == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    {
        char backing[16];
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 5;  // "ABCDE"
        for (std::size_t i = 0; i < 16; ++i) {
            backing[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            CHECK_EQ(buf[0], 'A');
            CHECK_EQ(buf[1], 'B');
            CHECK_EQ(buf[2], 'C');
            CHECK_EQ(buf[3], 'D');
            CHECK_EQ(buf[4], 'E');
            for (std::size_t i = expected_written; i < 16; ++i) {
                CHECK_EQ(buf[i], '#');
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(buf[i], "ABCDE"[i]);
        }
        for (std::size_t i = expected_written; i < 16; ++i) {
            CHECK_EQ(buf[i], '#');
        }
#endif
    }
#else
    {
        CHECK(true);
    }
#endif

    // ----- Section 11 -- HASH-pad concurrent dispatch.
    SECTION("Section 11 - HASH-pad concurrent dispatch");
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;
        constexpr int kTotal = kThreads * kCallsPerThread;
        char backing[kTotal][16];
        std::array<std::span<char>, kTotal> spans;
        for (std::size_t i = 0; i < static_cast<std::size_t>(kTotal); ++i) {
            spans[i] = std::span<char>{backing[i], 16};
        }
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
                    std::span<char> buf = spans[idx];
                    for (std::size_t i = 0; i < 16; ++i) {
                        buf[i] = 'Q';
                    }
#if PETRA_THROW_ON_OVERFLOW == 0
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
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
                        n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
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

    // ----- Section 12 -- HASH-pad distinct from DASH, SPACE, NUL.
    SECTION("Section 12 - HASH-pad distinct from DASH, SPACE, NUL");
#if PETRA_HASH_PAD == 1
    {
        char backing[16];
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 5;
        for (std::size_t i = 0; i < 16; ++i) {
            backing[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            for (std::size_t i = expected_written; i < 16; ++i) {
                // NOT '-' (DASH-pad), NOT ' ' (SPACE-pad),
                // NOT '\0' (NUL-pad), NOT 'Q' (untouched poison).
                CHECK(buf[i] == '#');
                CHECK(buf[i] != '-');
                CHECK(buf[i] != ' ');
                CHECK(buf[i] != '\0');
                CHECK(buf[i] != 'Q');
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = expected_written; i < 16; ++i) {
            CHECK(buf[i] == '#');
            CHECK(buf[i] != '-');
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

    // ----- Section 13 -- HASH-pad HASH-overwrites-DASH distinctness.
    SECTION("Section 13 - HASH-pad HASH-overwrites-DASH distinctness");
#if PETRA_HASH_PAD == 1 && PETRA_DASH_PAD == 1
    {
        char backing[16];
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 5;
        for (std::size_t i = 0; i < 16; ++i) {
            backing[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            for (std::size_t i = expected_written; i < 16; ++i) {
                // '#', NOT '-', NOT ' ', NOT '\0', NOT 'Q'.
                CHECK(buf[i] == '#');
                CHECK(buf[i] != '-');
                CHECK(buf[i] != ' ');
                CHECK(buf[i] != '\0');
                CHECK(buf[i] != 'Q');
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = expected_written; i < 16; ++i) {
            CHECK(buf[i] == '#');
            CHECK(buf[i] != '-');
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

    // ----- Section 14 -- HASH-pad HASH-overwrites-SPACE transitive
    //                       composition distinctness.
    SECTION("Section 14 - HASH-pad HASH-overwrites-SPACE transitive composition");
#if PETRA_HASH_PAD == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    {
        char backing[16];
        std::span<char> buf{backing, 16};
        std::size_t expected_written = 5;
        for (std::size_t i = 0; i < 16; ++i) {
            backing[i] = 'Q';
        }
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), expected_written);
            for (std::size_t i = expected_written; i < 16; ++i) {
                // '#', NOT '-', NOT ' ', NOT '\0', NOT 'Q'.
                CHECK(buf[i] == '#');
                CHECK(buf[i] != '-');
                CHECK(buf[i] != ' ');
                CHECK(buf[i] != '\0');
                CHECK(buf[i] != 'Q');
            }
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, expected_written);
        for (std::size_t i = expected_written; i < 16; ++i) {
            CHECK(buf[i] == '#');
            CHECK(buf[i] != '-');
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

    // ----- Section 15 -- HASH-pad throwing sibling catch chain.
    SECTION("Section 15 - HASH-pad throwing sibling catch chain");
#if PETRA_THROW_ON_OVERFLOW == 1
    {
        char backing[8];
        std::span<char> buf{backing, 8};
        for (std::size_t i = 0; i < 8; ++i) {
            backing[i] = 'Q';
        }
        // "ABCDEFGHIJ" is 10 bytes -- TooLarge for span.size()=8.
        bool caught_runtime = false;
        bool caught_format = false;
        try {
            (void) TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGHIJ");
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

    // ----- Section 16 -- HASH-pad return type decltype pin.
    SECTION("Section 16 - HASH-pad return type decltype pin");
    {
#if PETRA_THROW_ON_OVERFLOW == 0
        using ExpectedType = std::expected<std::size_t, petra::FormatError>;
        char backing[8];
        std::span<char> buf{backing, 8};
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "x");
        static_assert(
            std::is_same_v<decltype(r), ExpectedType>,
            "RUNTIME macro must return std::expected<std::size_t, FormatError> "
            "when PETRA_THROW_ON_OVERFLOW=0");
        auto r2 = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "y");
        static_assert(
            std::is_same_v<decltype(r2), ExpectedType>,
            "CONSTEVAL macro must return std::expected<std::size_t, FormatError> "
            "when PETRA_THROW_ON_OVERFLOW=0");
#else
        using SizeType = std::size_t;
        char backing[8];
        std::span<char> buf{backing, 8};
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(buf, "x");
        static_assert(
            std::is_same_v<decltype(r), SizeType>,
            "RUNTIME macro must return std::size_t "
            "when PETRA_THROW_ON_OVERFLOW=1");
        auto r2 = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "y");
        static_assert(
            std::is_same_v<decltype(r2), SizeType>,
            "CONSTEVAL macro must return std::size_t "
            "when PETRA_THROW_ON_OVERFLOW=1");
#endif
        CHECK(true);
    }

    // ----- Section 17 -- HASH-pad multiple format args.
    SECTION("Section 17 - HASH-pad multiple format args");
    {
        char backing[32];
        std::span<char> buf{backing, 32};
        // "a=1,b=2" is 7 bytes.
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            buf, "a={},b={}", 1, 2);
        CHECK(r.has_value());
        if (r.has_value()) {
            CHECK_EQ(r.value(), std::size_t{7});
            CHECK_EQ(buf[0], 'a');
            CHECK_EQ(buf[1], '=');
            CHECK_EQ(buf[2], '1');
            CHECK_EQ(buf[3], ',');
            CHECK_EQ(buf[4], 'b');
            CHECK_EQ(buf[5], '=');
            CHECK_EQ(buf[6], '2');
        }
#else
        std::size_t n = 0;
        bool threw = false;
        try {
            n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, "a={},b={}", 1, 2);
        } catch (...) {
            threw = true;
        }
        CHECK(!threw);
        CHECK_EQ(n, std::size_t{7});
        CHECK_EQ(buf[0], 'a');
        CHECK_EQ(buf[1], '=');
        CHECK_EQ(buf[2], '1');
        CHECK_EQ(buf[3], ',');
        CHECK_EQ(buf[4], 'b');
        CHECK_EQ(buf[5], '=');
        CHECK_EQ(buf[6], '2');
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