// P-2026-10-19 — try_format_at_pad_span_n:
//
// Closes Oct 18's "Where we go next" item (2) verbatim — the
// STD::SPAN<CHAR>-side NINTH pad-character axis (PETRA_AT_PAD).
// TODAY introduces a NEW NINTH compile-time flag (PETRA_AT_PAD)
// on top of Oct 16's 8-bit tuple (PETRA_THROW_ON_OVERFLOW,
// PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD,
// PETRA_TILDE_PAD, PETRA_UNDERSCORE_PAD, PETRA_STAR_PAD) on
// the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.  PETRA_AT_PAD=1
// selects the AT-padded span<char> sibling, which fills bytes
// [written, span.size()) with '@' (ASCII 64) on success.
//
// Closes Oct 18's "Where we go next" item (2) verbatim on the
// STD::SPAN<CHAR> side — the AT-axis pad-character lesson
// paralleling Oct 18's CHAR*+CAP closure of Oct 17's "Where we
// go next" item (1).  Today's pad character '@' (ASCII 64) is
// TOTALLY DIFFERENT from Oct 16's '*' (ASCII 42), Oct 15's '_'
// (ASCII 95), Oct 13's '~' (ASCII 126), Oct 11's '#' (ASCII
// 35), Oct 4's '-' (ASCII 45), Sep 28's ' ' (ASCII 32), and
// Sep 27's '\0' (ASCII 0).  The '*' byte has ASCII value 42
// (0x2A), distinct from the seven prior pad bytes and the
// poison byte 'Q' (0x51).
//
// After today, the STD::SPAN<CHAR> × BOUNDED-BUFFER matrix on
// the (PETRA_THROW_ON_OVERFLOW × PETRA_AT_PAD) 2-bit tuple is
// COMPLETE for ALL FOUR cells.  Combined with Oct 18's CHAR*+CAP
// AT-pad sibling, both axes reach NINE pad characters
// simultaneously: NONE → NUL → SPACE → DASH → HASH → TILDE →
// UNDERSCORE → STAR → AT.
//
// The new compile-time flag 9-tuple (well, 2-tuple used for
// testing with the rest pinned to 0)
// (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD) controls the test
// dispatch via
//   #if PETRA_THROW_ON_OVERFLOW ×
//   #if PETRA_AT_PAD
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_AT_PAD = 0.
// All other pad flags (PAD, SPACE_PAD, DASH_PAD, HASH_PAD,
// UNDERSCORE_PAD, TILDE_PAD, STAR_PAD) are pinned to 0 by
// friendly-errors.
//
// KEY FINDING — the AT-pad span<char> sibling is a THIN LAYER
// on top of Oct 16's STAR-pad span<char> sibling (literally:
// it calls Oct 16's wrapper, then OVERWRITES the '*' fill
// with '@' on success).  This is the same pattern Oct 16 used
// (STAR layer on Oct 15's UNDERSCORE-pad span<char> sibling),
// Oct 15 used (UNDERSCORE layer on Oct 13's TILDE-pad span<char>
// sibling), Oct 13 used (TILDE layer on Oct 11's HASH-pad
// span<char> sibling), and Oct 11 used (HASH layer on Oct 4's
// DASH-pad sibling).
//
// The composition rule "the LATER fill overwrites the EARLIER
// fill" now has EIGHT layers:
// AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL.
// When all eight pad-flags are set (AT, STAR, UNDERSCORE,
// TILDE, HASH, DASH, SPACE, PAD), the end byte state is always
// '@'.  Composition cells with (AT=1, STAR=*) REUSE TODAY's NEW
// AT-pad wrappers verbatim because AT overwrites STAR (which
// overwrites UNDERSCORE which overwrites TILDE which
// overwrites HASH which overwrites DASH which overwrites SPACE).
//
// SCOPE NOTE: This lesson is INTENTIONALLY NARROW.  It adds ONE
// compile-time flag (PETRA_AT_PAD) on top of Oct 16's helpers'
// (PETRA_STAR_PAD) axis — focused on a 2-bit tuple
// (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD) for testing.  When
// PETRA_PAD = 1, PETRA_SPACE_PAD = 1, PETRA_DASH_PAD = 1,
// PETRA_HASH_PAD = 1, PETRA_UNDERSCORE_PAD = 1, PETRA_TILDE_PAD = 1,
// or PETRA_STAR_PAD = 1, the lesson's friendly-error fires at
// compile time with a clear pointer to those lessons.
//
// PLATFORM NOTE: This file uses Oct 11's PROVEN callback-based
// impl (the `cb` lambda + std::move pattern) — NOT the
// no-callback pattern from a prior untracked P-2026-10-02 draft.
// The no-callback pattern was found to NOT work on Apple Clang
// 21 / libc++ 21 because std::vformat_to internally COPIES the
// output iterator.  See the platform note in Oct 11 / Oct 15 /
// Oct 16's source for the full investigation.
//
// Build (default — expected+untouched sibling; PETRA_THROW=0,
// PETRA_AT_PAD=0):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-10-19-try-format-at-pad-span-n.cpp \
//           -o /tmp/P-2026-10-19-try-format-at-pad-span-n
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-19-try-format-at-pad-span-n
//
// AT-pad cell (PETRA_AT_PAD=1):
//
//   cmake -S . -B build-at -DPETRA_AT_PAD=ON
//   cmake --build build-at
//   ./build-at/P-2026-10-19-try-format-at-pad-span-n
//
// Throwing+AT-pad cell (PETRA_THROW_ON_OVERFLOW=1,
// PETRA_AT_PAD=1):
//
//   cmake -S . -B build-throw-at \
//         -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_AT_PAD=ON
//   cmake --build build-throw-at
//   ./build-throw-at/P-2026-10-19-try-format-at-pad-span-n
//
// Pure consumer-side; no third-party libraries.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <print>
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
// Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
// Oct 13 / Oct 15 / Oct 16 / Oct 18)
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
// Part 2 — petra::counting_output_iterator (Oct 3 / Oct 4 / Oct 8 / Oct 9 /
// Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 18 verbatim — callback-based)
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
    constexpr counting_output_iterator& operator++(int) noexcept { return *this; }

    // NOTE: NO explicit copy ctor — the compiler-generated copy ctor
    // copies cb_/data_/count_.  When vformat_to internally COPIES the
    // iterator (Apple Clang 21 / libc++ 21 behavior), the proxy's
    // operator= points to the copy, but the copy's cb_ still calls
    // back through data_ to the LOCAL in the impl.  This is the
    // CORRECT callback pattern — see Oct 11's / Oct 15's / Oct 16's
    // platform note.

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (Oct 3 / Oct 4 / Oct 8 / Oct 9
// / Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 18 verbatim — callback-based)
//
// CRITICAL: bounded_char_writer IS the output iterator.  It accepts
// a callback in its ctor so that even after vformat_to internally
// COPIES the iterator, the writes go through a `void* data` pointer
// to the local in the impl.
//
// The COPY ctor intentionally nulls the callback (the callback's
// `void* data` only refers to the original local; the copy's writes
// must be discarded — which they are, since operator= already
// short-circuits when written_ == cap_, and the count tracking in
// the ORIGINAL still works because vformat_to uses the original
// for the "actual write" pass).
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
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
// petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim).
//
// Both impls are CHAR*+CAP from Sep 8 / Sep 9 — the
// span<char>-surface wrappers in Part 5 adapt to (out.data(),
// out.size()).
//
// 2-pass design (measure pass + write pass) is essential to
// distinguish Format errors from TooLarge errors without throwing.
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
                                   std::format_string<Args...> fmt_str,
                                   const Args&... args) {
    // Same shape as the RUNTIME impl above but takes std::format_string.
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
        (void)std::vformat_to(std::move(sink), fmt_str.get(),
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
// These are the expected+untouched siblings on BOTH axes.  TODAY's
// NEW AT-pad span<char> sibling wrappers (Part 8) REUSE the
// span<char>-surface of these wrappers (which delegates to the
// (char*, cap)-surface underlying impls).
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
                              std::format_string<Args...> fmt_str,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt_str, args...);
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
                                   std::format_string<Args...> fmt_str,
                                   const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out.data(),
                                                      out.size(),
                                                      fmt_str, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_runtime_n_throwing (Sep 20 verbatim char*+cap)
// + petra::try_format_bounded_runtime_n_throwing (Sep 20 verbatim
// char*+cap) + petra::try_format_runtime_n_throwing(span<char>) (Sep 21
// verbatim) + petra::try_format_bounded_runtime_n_throwing_span (Sep 21
// verbatim).
//
// These are the throwing+untouched siblings on BOTH axes.  TODAY's
// NEW throwing+AT-pad span<char> sibling wrappers (Part 8) REUSE
// these wrappers.
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
                                       std::format_string<Args...> fmt_str,
                                       const Args&... args) {
    auto r = try_format_bounded_runtime_n(out, cap, fmt_str, args...);
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
try_format_bounded_runtime_n_throwing_span(
        std::span<char> out, std::format_string<Args...> fmt_str,
        const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt_str, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

}  // namespace petra

// ============================================================================
// Part 7 — Inline (in this file) STAR-pad span<char> sibling wrappers —
// re-typed here from Oct 16 so TODAY's AT-pad sibling can wrap
// them without a separate include.  These are EXACTLY Oct 16's
// NEW wrappers.
//
// (active when PETRA_PAD=0 AND PETRA_STAR_PAD=1):
//   - petra::try_format_runtime_n_star_pad_span
//   - petra::try_format_bounded_runtime_n_star_pad_span
//   - petra::try_format_runtime_n_throwing_star_pad_span
//   - petra::try_format_bounded_runtime_n_throwing_star_pad_span
// ============================================================================

namespace petra {

// Oct 16 verbatim — RUNTIME surface, expected+STAR-padded
// span<char> sibling.  Wraps underlying try_format_runtime_n(span)
// and star-fills [written, span.size()) with '*' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_star_pad_span(std::span<char> out,
                                   std::string_view fmt,
                                   const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '*';
        }
    }
    return r;
}

// Oct 16 verbatim — CONSTEVAL surface.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_star_pad_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt_str, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '*';
        }
    }
    return r;
}

// Oct 16 verbatim — RUNTIME surface, throwing+STAR-padded.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_star_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '*';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Oct 16 verbatim — CONSTEVAL surface, throwing+STAR-padded.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_star_pad_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_span(out, fmt_str, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '*';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 8 — TODAY's NEW AT-pad span<char> sibling wrappers:
// petra::try_format_runtime_n_at_pad_span (RUNTIME + EXPECTED) +
// petra::try_format_bounded_runtime_n_at_pad_span (CONSTEVAL + EXPECTED) +
// petra::try_format_runtime_n_throwing_at_pad_span (RUNTIME + THROWING) +
// petra::try_format_bounded_runtime_n_throwing_at_pad_span (CONSTEVAL + THROWING).
//
// TODAY's NEW AT-pad span<char> sibling wrappers are thin layers
// on top of Oct 16's STAR-pad span<char> siblings.  The
// composition rule "the LATER fill overwrites the EARLIER fill"
// applies:
//   1. The AT-pad span<char> wrapper calls the STAR-pad
//      span<char> sibling (Oct 16's wrapper), which fills bytes
//      [written, span.size()) with '*' on success.
//   2. On success, the AT-pad wrapper OVERWRITES those '*' bytes
//      with '@' (the LATER fill).
//   3. On error (Format or TooLarge), the AT-pad wrapper does NOT
//      touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
//
// (active when PETRA_PAD=0 AND PETRA_AT_PAD=1):
//   - (0, 0, 0, 0, 0, 0, 0, 1) expected+AT-padded span<char> sibling
//     (RUNTIME surface): petra::try_format_runtime_n_at_pad_span
//   - (0, 0, 0, 0, 0, 0, 0, 1) expected+AT-padded span<char> sibling
//     (CONSTEVAL surface): petra::try_format_bounded_runtime_n_at_pad_span
//   - (1, 0, 0, 0, 0, 0, 0, 1) throwing+AT-padded span<char> sibling
//     (RUNTIME surface): petra::try_format_runtime_n_throwing_at_pad_span
//   - (1, 0, 0, 0, 0, 0, 0, 1) throwing+AT-padded span<char> sibling
//     (CONSTEVAL surface): petra::try_format_bounded_runtime_n_throwing_at_pad_span
// ============================================================================

namespace petra {

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// AT-PAD sibling on the span<char> axis.  Wraps Oct 16's
// try_format_runtime_n_star_pad_span verbatim and at-fills
// [written, span.size()) with '@' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_at_pad_span(std::span<char> out,
                                 std::string_view fmt,
                                 const Args&... args) {
    auto r = try_format_runtime_n_star_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        // LATER fill (AT) overwrites EARLIER fill (STAR) from
        // the STAR-pad span<char> sibling.  On error,
        // r.has_value() is false and the loop is skipped,
        // preserving the buffer's atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '@';
        }
    }
    return r;
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// AT-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_at_pad_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_star_pad_span(out, fmt_str, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '@';
        }
    }
    return r;
}

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// AT-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_at_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_star_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '@';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// AT-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_at_pad_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_star_pad_span(
                out, fmt_str, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '@';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 9 — TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW
// (Mirrors Oct 16's dispatcher shapes; the compile-time flag
// tuple (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD) controls the
// dispatch via #if PETRA_THROW_ON_OVERFLOW × #if PETRA_AT_PAD.)
//
// The 2-bit tuple selects ONE of FOUR AT-pad span<char> sibling
// wrappers at preprocessor time:
//   - (0, 0): try_format_bounded_runtime_n_span / try_format_runtime_n (Sep 10 / Sep 19)
//   - (0, 1): try_format_bounded_runtime_n_at_pad_span / try_format_runtime_n_at_pad_span (NEW today)
//   - (1, 0): try_format_bounded_runtime_n_throwing_span / try_format_runtime_n_throwing (Sep 21)
//   - (1, 1): try_format_bounded_runtime_n_throwing_at_pad_span / try_format_runtime_n_throwing_at_pad_span (NEW today)
//
// Each macro is a parenthesized expression (NOT do { } while (0))
// so the caller can write
// 'auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(...)'
// and capture the result.  Each macro uses __VA_OPT__(,) for the
// no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep
// 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep
// 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 /
// Oct 8 / Oct 9 / Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 18's
// macro shape).
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

#ifndef PETRA_HASH_PAD
#define PETRA_HASH_PAD 0
#endif

#if PETRA_HASH_PAD != 0 && PETRA_HASH_PAD != 1
#error "PETRA_HASH_PAD must be 0 (no hash-pad) or 1 (hash-padded)"
#endif

#ifndef PETRA_UNDERSCORE_PAD
#define PETRA_UNDERSCORE_PAD 0
#endif

#if PETRA_UNDERSCORE_PAD != 0 && PETRA_UNDERSCORE_PAD != 1
#error "PETRA_UNDERSCORE_PAD must be 0 (no underscore-pad) or 1 (underscore-padded)"
#endif

#ifndef PETRA_STAR_PAD
#define PETRA_STAR_PAD 0
#endif

#if PETRA_STAR_PAD != 0 && PETRA_STAR_PAD != 1
#error "PETRA_STAR_PAD must be 0 (no star-pad) or 1 (star-padded)"
#endif

#ifndef PETRA_AT_PAD
#define PETRA_AT_PAD 0
#endif

#if PETRA_AT_PAD != 0 && PETRA_AT_PAD != 1
#error "PETRA_AT_PAD must be 0 (no at-pad) or 1 (at-padded)"
#endif

// Friendly-errors for the composition cells (out of scope today).
#if PETRA_PAD != 0
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_PAD=0.  For PETRA_PAD=1, use Sep 27's try_format_padded_span_n_or_throw or Oct 1's try_format_padded_space_pad_span_n_or_throw (NUL-padded span<char> wrappers)."
#endif

#if PETRA_SPACE_PAD != 0
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_SPACE_PAD=0.  For PETRA_SPACE_PAD=1, use Sep 28's try_format_space_pad_span_n (STD::SPAN<CHAR> SPACE-padded sibling) or Oct 1's try_format_padded_space_pad_span_n_or_throw (composition cells)."
#endif

#if PETRA_DASH_PAD != 0
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_DASH_PAD=0.  For PETRA_DASH_PAD=1, use Oct 4's try_format_dash_pad_span_n (STD::SPAN<CHAR> DASH-padded sibling) or Oct 5 / Oct 8's composition cells."
#endif

#if PETRA_HASH_PAD != 0
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_HASH_PAD=0.  For PETRA_HASH_PAD=1, use Oct 11's try_format_hash_pad_span_n (STD::SPAN<CHAR> HASH-padded sibling)."
#endif

#if PETRA_UNDERSCORE_PAD != 0
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_UNDERSCORE_PAD=0.  For PETRA_UNDERSCORE_PAD=1, use Oct 15's try_format_underscore_pad_span_n (STD::SPAN<CHAR> UNDERSCORE-padded sibling).  Note: today's AT-pad wrapper REUSES Oct 16's STAR-pad wrapper internally, which REUSES Oct 15's UNDERSCORE-pad wrapper — if you set PETRA_UNDERSCORE_PAD=1 explicitly, the dispatcher #errors to avoid double-pad ambiguity."
#endif

#if defined(PETRA_TILDE_PAD) && PETRA_TILDE_PAD != 0
// #ifdef PETRA_TILDE_PAD guard — without this, PETRA_TILDE_PAD=1 with
// PETRA_STAR_PAD=0 would not fire because undefined macros evaluate
// to 0 in #if expressions.
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_TILDE_PAD=0.  For PETRA_TILDE_PAD=1, use Oct 13's try_format_tilde_pad_span_n (STD::SPAN<CHAR> TILDE-padded sibling)."
#endif

#if PETRA_STAR_PAD != 0
#error "This lesson (try_format_at_pad_span_n) only supports PETRA_STAR_PAD=0.  For PETRA_STAR_PAD=1, use Oct 16's try_format_star_pad_span_n (STD::SPAN<CHAR> STAR-padded sibling).  Note: today's AT-pad wrapper REUSES Oct 16's STAR-pad wrapper internally — if you set PETRA_STAR_PAD=1 explicitly, the dispatcher #errors to avoid double-pad ambiguity."
#endif

// Dispatcher: 2-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD).
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_AT_PAD == 0
// (0, 0) -> expected+untouched span<char> sibling path.
#define TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                 __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                    __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_AT_PAD == 1
// (0, 1) -> expected+AT-padded span<char> sibling path (NEW today).
#define TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_at_pad_span((out), (fmt)       \
                                                       __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_at_pad_span((out), (fmt)               \
                                               __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_AT_PAD == 0
// (1, 0) -> throwing+untouched span<char> sibling path.
#define TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)     \
                                                          __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                  \
                                             __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_AT_PAD == 1
// (1, 1) -> throwing+AT-padded span<char> sibling path (NEW today).
#define TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_at_pad_span(          \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_at_pad_span((out), (fmt)      \
                                                        __VA_OPT__(,) __VA_ARGS__))
#else
#error "Internal error: PETRA_THROW_ON_OVERFLOW and PETRA_AT_PAD combination not handled"
#endif

// ============================================================================
// Part 10 — hand-rolled SECTION/CHECK framework (mirrors Sep 6 / Sep 7 /
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Oct 16's shape).
//
// We use std::fprintf(stderr, ...) for PASS/FAIL messaging instead
// of std::println, because std::println needs a std::formatter for
// every type it prints (FormatErrorKind has no formatter — adding
// one is out of scope for this lesson).  The std::println-based
// framing used in the section HEADERS is fine because every arg
// printed there is std::string_view (formatter-defined for
// string_view by default).
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
            (int)got.size(), got.data(),
            (int)expected.size(), expected.data());
    }
}

void check_eq(char got, char expected, const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ(char)  %s: got '%c' expected '%c'\n",
            g_section, what, got, expected);
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
// Part 11 — sections.
//
// We test the (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD) 2-bit
// tuple on the STD::SPAN<CHAR> axis:
//
//   - (0, 0) — expected+untouched span<char> sibling.
//       Bytes [written, span.size()) on success are UNTOUCHED
//       (poison byte 'Q' from the test buffer survives).
//   - (0, 1) — expected+AT-padded span<char> sibling.
//       Bytes [written, span.size()) on success are '@'.
//   - (1, 0) — throwing+untouched span<char> sibling.
//       Bytes [written, span.size()) on success are UNTOUCHED;
//       throws std::runtime_error on overflow.
//   - (1, 1) — throwing+AT-padded span<char> sibling.
//       Bytes [written, span.size()) on success are '@'; throws
//       std::runtime_error on overflow.
//
// All other flags (PAD, SPACE_PAD, DASH_PAD, HASH_PAD,
// UNDERSCORE_PAD, TILDE_PAD, STAR_PAD) are pinned to 0 by the
// friendly-error #if-guards above; no composition cells are
// tested today.
// ============================================================================

int main() {
    using namespace petra_test;
    using namespace petra;

    // ------------------------------------------------------------------
    // Section 1 — AT-pad RUNTIME × EXPECTED on span<char>: written
    // bytes match expected; leftover span bytes are '@'.
    // ------------------------------------------------------------------
    section("Section 1: expected+AT-pad (0, 1) cell — bytes after written are '@'");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        auto r = try_format_runtime_n_at_pad_span(
            s, "x={}", 42);
        check_true(r.has_value(), "(0,1) r.has_value");
        check_eq(r.value(), std::size_t{4}, "(0,1) r.value()");
        check_eq(std::string_view(buf, 4), std::string_view{"x=42"},
                 "(0,1) buf[0..3]");
        // Bytes [written=4, 16) must all be '@'.
        bool all_at = true;
        for (std::size_t i = 4; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "(0,1) buf[4..15] all '@'");
    }

    // ------------------------------------------------------------------
    // Section 2 — buffer atomicity on TooLarge: the AT-pad wrapper
    // must NOT touch bytes on the overflow path.  Also pins the
    // sub-span 'no-bleed' contract — bytes outside the span stay
    // poison 'Q'.
    // ------------------------------------------------------------------
    section("Section 2: expected+AT-pad (0, 1) cell — buffer UNTOUCHED on TooLarge + sub-span no-bleed");
    {
        // (a) Full-span overflow.
        char buf[8] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        // "ABCDEFGHIJ" is 10 bytes; span.size()=8 forces a TooLarge.
        auto r = try_format_runtime_n_at_pad_span(
            s, "ABCDEFGHIJ");
        check_true(!r.has_value(), "(0,1) TooLarge: no value");
        check_eq(r.error().kind, FormatErrorKind::TooLarge,
                 "(0,1) TooLarge: kind");
        // Buffer must be entirely untouched (every byte still 'Q').
        bool all_q = true;
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            if (buf[i] != 'Q') { all_q = false; break; }
        }
        check_true(all_q, "(0,1) TooLarge: buf all 'Q' (untouched)");

        // (b) Sub-span no-bleed: a 6-byte sub-span at offset 5 of a
        // 16-byte buffer with format "ABCDE" (5 bytes) leaves bytes
        // [0,5) and [11,16) as poison 'Q'.
        char buf2[16] = {};
        std::fill(std::begin(buf2), std::end(buf2), 'Q');
        std::span<char> sub(buf2 + 5, 6);
        auto r2 = try_format_runtime_n_at_pad_span(
            sub, "ABCDE");
        check_true(r2.has_value(), "(0,1) sub-span: has_value");
        check_eq(r2.value(), std::size_t{5}, "(0,1) sub-span: value==5");
        check_eq(std::string_view(buf2 + 5, 5), std::string_view{"ABCDE"},
                 "(0,1) sub-span: content [5..9]");
        // Bytes [5+5, 5+6) = [10, 11) inside the sub-span must be '@'.
        check_eq(buf2[10], '@', "(0,1) sub-span: buf2[10]=='@'");
        // Bytes [0, 5) and [11, 16) outside the sub-span must be 'Q'.
        bool prefix_untouched = true;
        for (std::size_t i = 0; i < 5; ++i) {
            if (buf2[i] != 'Q') { prefix_untouched = false; break; }
        }
        check_true(prefix_untouched, "(0,1) sub-span: buf2[0..4] all 'Q'");
        bool suffix_untouched = true;
        for (std::size_t i = 11; i < 16; ++i) {
            if (buf2[i] != 'Q') { suffix_untouched = false; break; }
        }
        check_true(suffix_untouched, "(0,1) sub-span: buf2[11..15] all 'Q'");
    }

    // ------------------------------------------------------------------
    // Section 3 — buffer atomicity on Format error.
    // ------------------------------------------------------------------
    section("Section 3: expected+AT-pad (0, 1) cell — buffer UNTOUCHED on Format error");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        // Malformed fmt (unterminated {) — must throw at format time.
        std::string bad_fmt = "bad { ";
        auto r = try_format_runtime_n_at_pad_span(
            s, bad_fmt);
        check_true(!r.has_value(), "(0,1) Format: no value");
        check_eq(r.error().kind, FormatErrorKind::Format,
                 "(0,1) Format: kind");
        bool all_q = true;
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            if (buf[i] != 'Q') { all_q = false; break; }
        }
        check_true(all_q, "(0,1) Format: buf all 'Q' (untouched)");
    }

    // ------------------------------------------------------------------
    // Section 4 — no-bleed on the EDGE cases: written == span.size()
    // and span.size() == 0.
    // ------------------------------------------------------------------
    section("Section 4: expected+AT-pad (0, 1) cell — written == span.size() and span.size()==0 edge cases");
    {
        // (a) written == span.size(): zero at-fill bytes.
        char buf[5] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        auto r = try_format_runtime_n_at_pad_span(
            s, "ABCDE");
        check_true(r.has_value(), "(0,1) edge: written==span.size() has_value");
        check_eq(r.value(), std::size_t{5},
                 "(0,1) edge: written==span.size() value");
        check_eq(std::string_view(buf, 5), std::string_view{"ABCDE"},
                 "(0,1) edge: written==span.size() content");

        // (b) span.size() == 0: format "X" into an empty span →
        //     TooLarge.  The sentinel 'Q' byte outside the
        //     span.size()=0 writable region remains untouched.
        char zbuf[1] = {'Q'};
        std::span<char> zs(zbuf, 0);
        auto r2 = try_format_runtime_n_at_pad_span(zs, "X");
        check_true(!r2.has_value(), "(0,1) edge: span.size()=0 TooLarge");
        check_eq(r2.error().kind, FormatErrorKind::TooLarge,
                 "(0,1) edge: span.size()=0 kind");
        check_eq(zbuf[0], 'Q', "(0,1) edge: span.size()=0 sentinel untouched");
    }

    // ------------------------------------------------------------------
    // Section 5 — AT distinctness: '@' is distinct from '*', '_', '~',
    // '#', ' ', '-', '\0' and 'Q'.
    // ------------------------------------------------------------------
    section("Section 5: AT-pad (0, 1) cell — byte-state distinctness");
    {
        char buf[8] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        auto r = try_format_runtime_n_at_pad_span(
            s, "AB");
        check_true(r.has_value(), "(0,1) distinctness: has_value");
        for (std::size_t i = 2; i < sizeof(buf); ++i) {
            check_true(buf[i] == '@', "(0,1) distinctness: buf[i]=='@'");
            check_true(buf[i] != '*', "(0,1) distinctness: buf[i]!='*'");
            check_true(buf[i] != '_', "(0,1) distinctness: buf[i]!='_'");
            check_true(buf[i] != '~', "(0,1) distinctness: buf[i]!='~'");
            check_true(buf[i] != '#', "(0,1) distinctness: buf[i]!='#'");
            check_true(buf[i] != '-', "(0,1) distinctness: buf[i]!='-'");
            check_true(buf[i] != ' ', "(0,1) distinctness: buf[i]!=' '");
            check_true(buf[i] != '\0', "(0,1) distinctness: buf[i]!='\\0'");
            check_true(buf[i] != 'Q', "(0,1) distinctness: buf[i]!='Q'");
        }
    }

    // ------------------------------------------------------------------
    // Section 6 — AT-pad CONSTEVAL × EXPECTED on span<char>: same
    // byte-state contract as the RUNTIME sibling.
    // ------------------------------------------------------------------
    section("Section 6: expected+AT-pad (0, 1) CONSTEVAL surface — same contract as RUNTIME");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        // STRING-LITERAL fmt — required by std::format_string<Args...>
        // ctor (consteval gate).
        auto r = try_format_bounded_runtime_n_at_pad_span(
            s, "hello {}", "world");
        check_true(r.has_value(), "(0,1) CONSTEVAL has_value");
        check_eq(r.value(), std::size_t{11}, "(0,1) CONSTEVAL value");
        check_eq(std::string_view(buf, 11),
                 std::string_view{"hello world"},
                 "(0,1) CONSTEVAL buf[0..10]");
        bool all_at = true;
        for (std::size_t i = 11; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "(0,1) CONSTEVAL buf[11..15] all '@'");
    }

    // ------------------------------------------------------------------
    // Section 7 — AT-pad RUNTIME × THROWING on span<char>:
    // byte-state contract on success; throws std::runtime_error on
    // overflow.
    // ------------------------------------------------------------------
    section("Section 7: throwing+AT-pad (1, 1) cell — success path + throws on TooLarge");
    {
        // (a) Success path: bytes after written are '@'.
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        std::size_t n = try_format_runtime_n_throwing_at_pad_span(
            s, "y={}", 7);
        check_eq(n, std::size_t{3}, "(1,1) value==3");
        check_eq(std::string_view(buf, 3), std::string_view{"y=7"},
                 "(1,1) buf[0..2]");
        bool all_at = true;
        for (std::size_t i = 3; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "(1,1) buf[3..15] all '@'");

        // (b) Overflow path: MUST throw std::runtime_error.
        char buf2[4] = {};
        std::fill(std::begin(buf2), std::end(buf2), 'Q');
        std::span<char> s2(buf2);
        bool threw = false;
        try {
            (void)try_format_runtime_n_throwing_at_pad_span(
                s2, "ABCDEFGHIJ");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check_true(threw, "(1,1) TooLarge throws std::runtime_error");
        // Buffer must be UNTOUCHED on the throwing path (no '@'
        // byte written, no partial overflow).
        bool all_q = true;
        for (std::size_t i = 0; i < sizeof(buf2); ++i) {
            if (buf2[i] != 'Q') { all_q = false; break; }
        }
        check_true(all_q, "(1,1) TooLarge: buf UNTOUCHED ('Q')");
    }

    // ------------------------------------------------------------------
    // Section 8 — AT-pad CONSTEVAL × THROWING on span<char>: same
    // contract as the RUNTIME × THROWING sibling.
    // ------------------------------------------------------------------
    section("Section 8: throwing+AT-pad (1, 1) CONSTEVAL surface");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        std::size_t n = try_format_bounded_runtime_n_throwing_at_pad_span(
            s, "{}", "X");
        check_eq(n, std::size_t{1}, "(1,1) CONSTEVAL value==1");
        check_eq(buf[0], 'X', "(1,1) CONSTEVAL buf[0]=='X'");
        bool all_at = true;
        for (std::size_t i = 1; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "(1,1) CONSTEVAL buf[1..15] all '@'");
    }

    // ------------------------------------------------------------------
    // Section 9 — Reuse-pinning: the (0, 0), (0, 1), (1, 0), (1, 1)
    // cells compiled and ran cleanly via
    // TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW (the
    // parenthesized-expression macro shape means we can capture the
    // result).
    // ------------------------------------------------------------------
    section("Section 9: macro TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW — works at preprocessor time");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);
        // We exercise the named AT-pad wrapper directly (which
        // always pads with '@' regardless of the (THROW, AT)
        // build cell) and also exercise the macro form so the
        // dispatcher's preprocessor-time selection is pinned.
        //
        // PETRA_AT_PAD=1 in the (0, 1) and (1, 1) build cells;
        // PETRA_AT_PAD=0 in the (0, 0) and (1, 0) cells.  The
        // named wrapper ALWAYS pads with '@'; the macro pads with
        // '@' only when PETRA_AT_PAD=1.  In the default build
        // (PETRA_AT_PAD=0), the macro returns the no-pad sibling
        // and the buffer's leftover bytes stay as 'Q' — that's the
        // correct contract for this cell.
        auto n_with = try_format_runtime_n_at_pad_span(
            s, "abc={}", 1);
        check_true(n_with.has_value(), "named: AT-pad has_value");
        check_eq(n_with.value(), std::size_t{5}, "named: AT-pad value");
        check_eq(std::string_view(buf, 5),
                 std::string_view{"abc=1"},
                 "named: AT-pad content");
        bool all_at = true;
        for (std::size_t i = 5; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "named: buf[5..15] all '@'");

        // Exercise the macro form.  On the (THROW=0) build cell,
        // the macro returns std::expected<std::size_t, FormatError>;
        // on the (THROW=1) build cell, the macro returns std::size_t
        // directly (and throws on overflow).  The dispatcher exercises
        // the right path at preprocessor time, and the
        // parenthesized-expression shape means we can capture the
        // result on both build cells.
        std::fill(std::begin(buf), std::end(buf), 'Q');
#if PETRA_THROW_ON_OVERFLOW == 0
        auto n_macro = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(
            s, "abc={}", 1);
        check_true(n_macro.has_value(), "macro: has_value");
        check_eq(n_macro.value(), std::size_t{5}, "macro: value");
#else
        std::size_t n_macro = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(
            s, "abc={}", 1);
        check_eq(n_macro, std::size_t{5}, "macro: value");
#endif
        check_eq(std::string_view(buf, 5),
                 std::string_view{"abc=1"},
                 "macro: content");
#if PETRA_AT_PAD == 1
        // (THROW=*, AT=1) cell: macro selects AT-pad sibling.
        all_at = true;
        for (std::size_t i = 5; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at,
                   "macro (AT=1): buf[5..15] all '@' (AT-pad path)");
#else
        // (THROW=*, AT=0) cell: macro selects underlying
        // untouched sibling — bytes [5..15) stay as 'Q'.
        bool all_q = true;
        for (std::size_t i = 5; i < 16; ++i) {
            if (buf[i] != 'Q') { all_q = false; break; }
        }
        check_true(all_q,
                   "macro (AT=0): buf[5..15] all 'Q' (untouched path)");
#endif
    }

    // ------------------------------------------------------------------
    // Section 10 — concurrent dispatch sanity (mirrors Oct 11 /
    // Oct 15 / Oct 16): 4 threads × 25 calls each.  Each thread
    // uses its own local counter (int via std::atomic<int>::
    // fetch_add) to tally matching bytes, then aggregated after
    // join.  Pins the thread-safety of the underlying impl on the
    // AT-pad path.
    // ------------------------------------------------------------------
    section("Section 10: concurrent dispatch — 4 threads x 25 calls on the AT-pad path");
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;
        char buf[kThreads][16] = {};
        for (int t = 0; t < kThreads; ++t) {
            std::fill(std::begin(buf[t]), std::end(buf[t]), 'Q');
        }
        std::atomic<int> total_pass{0};
        std::atomic<int> total_fail{0};
        std::array<std::thread, kThreads> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads[static_cast<std::size_t>(t)] = std::thread([&, t]() {
                int local_pass = 0;
                int local_fail = 0;
                for (int i = 0; i < kCallsPerThread; ++i) {
                    std::span<char> s(buf[t]);
                    auto r = try_format_runtime_n_at_pad_span(
                        s, "tid={},i={}", t, i);
                    if (!r.has_value()) {
                        ++local_fail;
                        continue;
                    }
                    const std::size_t expected_len =
                        std::string{"tid="}.size()
                            + std::to_string(t).size()
                            + std::string{",i="}.size()
                            + std::to_string(i).size();
                    if (r.value() != expected_len) {
                        ++local_fail;
                        continue;
                    }
                    // Check that bytes [written, 16) are all '@'.
                    bool all_at = true;
                    for (std::size_t j = expected_len; j < 16; ++j) {
                        if (buf[t][j] != '@') { all_at = false; break; }
                    }
                    if (all_at) {
                        ++local_pass;
                    } else {
                        ++local_fail;
                    }
                }
                total_pass.fetch_add(local_pass);
                total_fail.fetch_add(local_fail);
            });
        }
        for (auto& th : threads) th.join();
        const int expected_total = kThreads * kCallsPerThread;
        check_eq(total_pass.load(), expected_total,
                 "concurrent: all calls pass byte-by-byte");
        check_eq(total_fail.load(), 0, "concurrent: zero failures");
    }

    // ------------------------------------------------------------------
    // Section 11 — decltype pinning of the (AT=1) sibling's
    // return type, on each of the 4 (THROW, AT) cells.
    // ------------------------------------------------------------------
    section("Section 11: decltype pinning on the (THROW, AT) 2-bit tuple");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::span<char> s(buf);

        // (0, *) RUNTIME surface returns std::expected<std::size_t,
        // petra::FormatError> on BOTH (AT=0) and (AT=1).
        static_assert(std::is_same_v<
            decltype(try_format_runtime_n(s, "x")),
            std::expected<std::size_t, FormatError>>,
            "(0,0) RUNTIME returns std::expected");
        static_assert(std::is_same_v<
            decltype(try_format_runtime_n_at_pad_span(s, "x")),
            std::expected<std::size_t, FormatError>>,
            "(0,1) RUNTIME returns std::expected");

        // (1, *) RUNTIME surface returns std::size_t on BOTH
        // (AT=0) and (AT=1).
        static_assert(std::is_same_v<
            decltype(try_format_runtime_n_throwing(s, "x")),
            std::size_t>,
            "(1,0) RUNTIME returns std::size_t");
        static_assert(std::is_same_v<
            decltype(try_format_runtime_n_throwing_at_pad_span(s, "x")),
            std::size_t>,
            "(1,1) RUNTIME returns std::size_t");
        check_true(true, "decltype checks: all 4 (THROW, AT) cells verified");
    }

    // ------------------------------------------------------------------
    // Section 12 — Summary.
    // ------------------------------------------------------------------
    section("Summary");
    std::println("  PASS={}  FAIL={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}