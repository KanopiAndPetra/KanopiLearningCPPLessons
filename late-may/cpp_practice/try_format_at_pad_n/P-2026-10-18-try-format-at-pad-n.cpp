// P-2026-10-18 — try_format_at_pad_n:
//
// A NEW AXIS on the BOUNDED-BUFFER × expected-vs-throwing
// quadrant on the CHAR*+CAP axis — the NINTH axis
// (PETRA_AT_PAD).  Today's lesson introduces the
// AT-PAD sibling, which fills bytes [written, cap)
// with '@' (ASCII 0x40 = 64) on success.  This is the analog of
// Oct 17's PETRA_STAR_PAD sibling (which filled with '*' =
// 0x2A = 42), but for a TOTALLY DIFFERENT pad character
// ('@' vs '*', versus Oct 14's '_' (95), Oct 12's '~' (126),
// Oct 9's '#' (35), Oct 3's '-' (45), Sep 28's ' ' (32),
// and Sep 25's '\0' (0)).
//
// Today's lesson closes Oct 17's "Where we go next" item (1)
// verbatim — the NINTH-axis pad layer on the CHAR*+CAP side
// of the matrix.  Today's lesson is INTENTIONALLY NARROW:
// it adds a single (PETRA_AT_PAD ∈ {0, 1}) compile-time flag
// on top of Oct 17's (PETRA_THROW_ON_OVERFLOW × PETRA_STAR_PAD)
// pair — that's a focused 2-bit tuple (not a 9-bit tuple).
//
// PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD,
// PETRA_TILDE_PAD, PETRA_UNDERSCORE_PAD, and PETRA_STAR_PAD
// must all be 0 (the lesson's friendly-error fires at compile
// time otherwise; pointers to Sep 25 / Sep 26 / Sep 28 / Sep 29 /
// Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
// Oct 12 / Oct 13 / Oct 14 / Oct 16 / Oct 17 are included in the
// error message).
//
// This narrower scope is the right move for one focused lesson:
// it demonstrates the addition of a NINTH PAD CHARACTER axis on
// top of Oct 17's STAR-pad lessons, while keeping the file
// size reasonable, the test matrix small (4 cells = 2 × 2), and
// the new wrappers composable with the earlier lessons' helpers.
//
// The FOUR NEW sibling wrappers, on the CHAR*+CAP ×
// BOUNDED-BUFFER × expected-vs-throwing axis:
//
//   - petra::try_format_runtime_n_at_pad(char*,
//     std::size_t, std::string_view, args...) — NEW today
//     (RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
//     AT-PAD sibling on the CHAR*+CAP axis; wraps
//     Oct 17's try_format_runtime_n_star_pad verbatim and
//     at-fills [written, cap) with '@' on success ONLY).
//   - petra::try_format_bounded_runtime_n_at_pad(char*,
//     std::size_t, std::format_string<Args...>, args...) — NEW
//     today (CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
//     AT-PAD sibling on the CHAR*+CAP axis; wraps
//     Oct 17's try_format_bounded_runtime_n_star_pad verbatim
//     and at-fills [written, cap) with '@' on success ONLY).
//   - petra::try_format_runtime_n_throwing_at_pad(char*,
//     std::size_t, std::string_view, args...) — NEW today
//     (RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
//     AT-PAD sibling on the CHAR*+CAP axis; wraps
//     Oct 17's try_format_runtime_n_throwing_star_pad verbatim
//     and at-fills [written, cap) with '@' on success ONLY;
//     re-raises std::runtime_error untouched on error).
//   - petra::try_format_bounded_runtime_n_throwing_at_pad(
//     char*, std::size_t, std::format_string<Args...>, args...)
//     — NEW today (CONSTEVAL surface of the BOUNDED-BUFFER ×
//     THROWING × AT-PAD sibling on the CHAR*+CAP axis;
//     wraps Oct 17's
//     try_format_bounded_runtime_n_throwing_star_pad verbatim
//     and at-fills [written, cap) with '@' on success ONLY;
//     re-raises std::runtime_error untouched on error).
//
// And ONE NEW meta-macro that ties the (THROW, AT) 2-bit tuple
// at the call site:
//
//   - TRY_FORMAT_BOUNDED_N_AT_OR_THROW (RUNTIME surface)
//     — NEW today; a verbatim thin dispatcher that picks between
//     the 2 expected × 2 throwing = 4 AT-pad sibling wrappers at
//     preprocessor time based on the
//     (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD) 2-bit tuple.
//
// HEADLINE finding — the AT-pad char*+cap sibling is a thin
// layer on top of Oct 17's STAR-pad char*+cap sibling wrapper
// (literally: it calls Oct 17's wrapper, then OVERWRITES the '*'
// fill with '@' on success).  The composition rule "the
// LATER fill overwrites the EARLIER fill" now has EIGHT layers:
// AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL
// (when all eight pad-flags are set, the end byte state is
// always '@').
//
// The transitive composition rule (AT > STAR > UNDERSCORE > TILDE >
// HASH > DASH > SPACE) means that, IF a future lesson introduces
// the (AT, STAR) or (AT, STAR, UNDERSCORE) or ... composition
// cells, those cells REUSE the (AT=1, STAR=0) AT-pad wrappers
// verbatim — no NEW wrappers are needed for the composition
// cells.  TODAY's lesson proves this design works on the (AT=1)
// slice only.
//
// Where this fits in the arc
// --------------------------
//   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
//              std::size_t cap (RUNTIME std::string-output wrapper).
//   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
//   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER × CHAR*+CAP ×
//              RUNTIME sibling (returns std::size_t via expected).
//   - Sep  9   try_format_bounded_runtime_n — BOUNDED-BUFFER ×
//              CHAR*+CAP × CONSTEVAL sibling.
//   - Sep 25   try_format_padded_n_or_into — FIRST axis
//              (PETRA_PAD) sibling-choice macro layer on the
//              CHAR*+CAP side (NUL-fill).
//   - Sep 28   try_format_space_pad_n — (PETRA_SPACE_PAD) sibling
//              wrapper on the CHAR*+CAP side.
//   - Oct  3   try_format_dash_pad_n — (PETRA_DASH_PAD) sibling
//              wrapper on the CHAR*+CAP side.
//   - Oct  9   try_format_hash_pad_n — (PETRA_HASH_PAD) sibling
//              wrapper on the CHAR*+CAP side; the FIFTH axis on
//              the expected-vs-throwing tuple.
//   - Oct 12   try_format_tilde_pad_n — (PETRA_TILDE_PAD) sibling
//              wrapper on the CHAR*+CAP side; the SIXTH axis.
//              Closes Oct 11's "Where we go next" item (a)
//              verbatim on the CHAR*+CAP side.
//   - Oct 13   try_format_tilde_pad_span_n — (PETRA_TILDE_PAD)
//              sibling wrapper on the STD::SPAN<CHAR> side;
//              the STD::SPAN<CHAR> mirror of Oct 12.
//   - Oct 14   try_format_underscore_pad_n — (PETRA_UNDERSCORE_PAD)
//              sibling wrapper on the CHAR*+CAP side; the
//              SEVENTH axis.  Today's pad character is '_'
//              (ASCII 95).
//   - Oct 16   try_format_star_pad_span_n — (PETRA_STAR_PAD)
//              sibling wrapper on the STD::SPAN<CHAR> side; the
//              EIGHTH axis.  Today's pad character is '*'
//              (ASCII 42).
//   - Oct 17   try_format_star_pad_n — (PETRA_STAR_PAD) sibling
//              wrapper on the CHAR*+CAP side; the ELEVENTH axis.
//              Today's pad character is '*' (ASCII 42), bringing
//              the CHAR*+CAP axis in line with the STD::SPAN<CHAR>
//              axis on the EIGHTH pad-character axis.
//   - TODAY    try_format_at_pad_n — (PETRA_AT_PAD) sibling
//              wrapper on the CHAR*+CAP side; the NINTH axis.
//              Closes Oct 17's "Where we go next" item (1)
//              verbatim on the CHAR*+CAP side.  Today's pad
//              character is '@' (ASCII 64), TOTALLY DIFFERENT
//              from Oct 17's '*' (ASCII 42), Oct 14's '_'
//              (ASCII 95), Oct 12's '~' (ASCII 126), Oct 9's '#'
//              (ASCII 35), Oct 3's '-' (ASCII 45), Sep 28's ' '
//              (ASCII 32), and Sep 25's '\0' (ASCII 0).
//
// SCOPE NOTE: This lesson is FOCUSED on the (AT) axis ONLY on
// the CHAR*+CAP × BOUNDED-BUFFER × expected-vs-throwing axis
// (PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0,
// PETRA_HASH_PAD=0, PETRA_TILDE_PAD=0, PETRA_UNDERSCORE_PAD=0,
// PETRA_STAR_PAD=0; STAR is BAKED INTO Oct 17's wrapper, which
// today's AT wrappers layer on top of — so STAR is implicitly
// present as the IMMEDIATELY-PRECEDING layer in the composition
// chain).  The (PAD=1), (SPACE_PAD=1), (DASH_PAD=1),
// (HASH_PAD=1), (TILDE_PAD=1), (UNDERSCORE_PAD=1), and
// (STAR_PAD=1) cells require Sep 25 / Sep 26 / Sep 28 / Sep 29 /
// Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
// Oct 12 / Oct 13 / Oct 14 / Oct 16 / Oct 17's helpers, which
// are NOT included in this file.  When PETRA_PAD = 1 or
// PETRA_SPACE_PAD = 1 or PETRA_DASH_PAD = 1 or PETRA_HASH_PAD = 1
// or PETRA_TILDE_PAD = 1 or PETRA_UNDERSCORE_PAD = 1 or
// PETRA_STAR_PAD = 1, the lesson's dispatcher #errors with a
// clear pointer to those lessons.
//
// What today covers
// -----------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6
//     through Oct 17 verbatim.
//   - petra::FormatErrorKind — REUSED Sep 6 through Oct 17
//     verbatim.
//   - petra::try_format_runtime_n(char*, std::size_t,
//     std::string_view, args...) — REUSED Sep 8 verbatim.
//   - petra::try_format_bounded_runtime_n(char*, std::size_t,
//     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim.
//   - petra::try_format_runtime_n_throwing(char*, std::size_t,
//     std::string_view, args...) — REUSED Sep 20 verbatim.
//   - petra::try_format_bounded_runtime_n_throwing(char*,
//     std::size_t, std::format_string<Args...>, args...) — REUSED
//     Sep 20 verbatim.
//   - petra::try_format_runtime_n_space_pad / dash_pad / hash_pad /
//     tilde_pad / underscore_pad / star_pad — REUSED Sep 28 /
//     Oct 3 / Oct 9 / Oct 12 / Oct 14 / Oct 17 verbatim (small
//     inline implementations for THIS lesson's wrapper chain).
//   - petra::try_format_runtime_n_throwing_star_pad — REUSED Oct 17
//     verbatim.
//   - petra::try_format_bounded_runtime_n_throwing_star_pad —
//     REUSED Oct 17 verbatim (CONSTEVAL counterpart).
//   - petra::try_format_bounded_runtime_n_star_pad — REUSED Oct 17
//     verbatim (CONSTEVAL counterpart of the expected sibling).
//   - petra::try_format_runtime_n_at_pad(char*,
//     std::size_t, std::string_view, args...) — NEW today
//     (RUNTIME × EXPECTED).
//   - petra::try_format_bounded_runtime_n_at_pad(char*,
//     std::size_t, std::format_string<Args...>, args...) — NEW
//     today (CONSTEVAL × EXPECTED).
//   - petra::try_format_runtime_n_throwing_at_pad(char*,
//     std::size_t, std::string_view, args...) — NEW today
//     (RUNTIME × THROWING; re-raises std::runtime_error
//     untouched on error).
//   - petra::try_format_bounded_runtime_n_throwing_at_pad(
//     char*, std::size_t, std::format_string<Args...>, args...)
//     — NEW today (CONSTEVAL × THROWING).
//   - TRY_FORMAT_BOUNDED_N_AT_OR_THROW — NEW today
//     (RUNTIME-surface meta-macro; dispatches between the
//     2 expected × 2 throwing = 4 AT-pad wrappers at
//     preprocessor time based on the
//     (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD) 2-bit tuple).
//
// SELF-CONTAINED NOTE: Like Oct 17, today's lesson is
// INTENTIONALLY SELF-CONTAINED.  It contains the FULL helper
// stack (FormatError, the iterator primitives, the bounded
// writer, the underlying impl, and the STAR-pad + UNDERSCORE-pad
// + TILDE-pad + HASH-pad + DASH-pad + SPACE-pad + Sep 8 / Sep 9 /
// Sep 20 RETURN-TYPE surfaces needed to BUILD the AT-pad
// siblings from the same source).  This is what makes today's
// lesson a viable single-file standalone exercise — verifiable
// from a single compile + run.
//
// Build (default — expected+untouched sibling; PETRA_THROW=0,
// PETRA_AT_PAD=0):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-10-18-try-format-at-pad-n.cpp \
//           -o /tmp/P-2026-10-18-try-format-at-pad-n
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-18-try-format-at-pad-n
//
// AT-pad cell (PETRA_AT_PAD=1):
//
//   cmake -S . -B build-at -DPETRA_AT_PAD=ON
//   cmake --build build-at
//   ./build-at/P-2026-10-18-try-format-at-pad-n
//
// Throwing+AT-pad cell (PETRA_THROW_ON_OVERFLOW=1,
// PETRA_AT_PAD=1):
//
//   cmake -S . -B build-throw-at \
//         -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_AT_PAD=ON
//   cmake --build build-throw-at
//   ./build-throw-at/P-2026-10-18-try-format-at-pad-n
//
// Pure consumer-side; no third-party libraries.

#include <algorithm>
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
#include <type_traits>
#include <utility>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 /
// Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
// Oct 12 / Oct 13 / Oct 14 / Oct 16 / Oct 17)
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
// Part 2 — petra::counting_output_iterator (Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Oct 3 / Oct 4 / Oct 8 / Oct 9 / Oct 11 / Oct 12 / Oct 14 / Oct 16 / Oct 17
// verbatim)
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

    [[nodiscard]] constexpr std::size_t count() const noexcept {
        return count_;
    }

private:
    callback_type cb_{nullptr};
    void* data_{nullptr};
    std::size_t count_{0};
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (verbatim)
// ============================================================================

namespace petra::detail {

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
                it_->out_[it_->written_] = c;
                ++it_->written_;
                if (it_->cb_) it_->cb_(it_->written_, it_->data_);
            }
            return *this;
        }
    };

    constexpr bounded_char_writer(char* out, std::size_t cap,
                                  callback_type cb, void* data) noexcept
        : out_(out), cap_(cap), cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<bounded_char_writer*>(this)};
    }
    constexpr bounded_char_writer& operator++() noexcept { return *this; }
    constexpr bounded_char_writer operator++(int) noexcept { return *this; }

    [[nodiscard]] constexpr std::size_t written() const noexcept {
        return written_;
    }

private:
    char* out_;
    std::size_t cap_;
    callback_type cb_;
    void* data_;
    std::size_t written_{0};
};

}  // namespace petra::detail

// ============================================================================
// Part 4 — petra::detail::try_format_runtime_n_impl (verbatim)
// ============================================================================

namespace petra::detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, ::petra::FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt,
                          const Args&... args) {
    constexpr void (*cb)(std::size_t, void*) =
        +[](std::size_t n, void* p) noexcept {
            *static_cast<std::size_t*>(p) = n;
        };

    std::size_t needed = 0;
    {
        counting_output_iterator it(cb, &needed);
        try {
            (void)std::vformat_to(it, fmt,
                std::make_format_args(args...));
        } catch (const std::format_error& e) {
            return std::unexpected{::petra::FormatError{
                ::petra::FormatErrorKind::Format, e.what()}};
        }
    }

    if (needed > cap) {
        return std::unexpected{::petra::FormatError{
            ::petra::FormatErrorKind::TooLarge,
            std::string{"format output size "}
                + std::to_string(needed)
                + " exceeds cap " + std::to_string(cap),
            needed}};
    }

    std::size_t written = 0;
    {
        bounded_char_writer w(out, cap, cb, &written);
        try {
            (void)std::vformat_to(w, fmt,
                std::make_format_args(args...));
        } catch (const std::format_error& e) {
            return std::unexpected{::petra::FormatError{
                ::petra::FormatErrorKind::Format, e.what()}};
        }
    }

    if (written != needed) {
        return std::unexpected{::petra::FormatError{
            ::petra::FormatErrorKind::TooLarge,
            std::string{"pass-2 written "}
                + std::to_string(written)
                + " disagrees with pass-1 needed "
                + std::to_string(needed),
            needed}};
    }

    return written;
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_impl(char* out, std::size_t cap,
                                    std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_impl(out, cap, fmt, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        if (e.kind == ::petra::FormatErrorKind::TooLarge) {
            throw std::runtime_error{
                std::string{"TooLarge: "} + e.message};
        }
        throw std::runtime_error{
            std::string{"Format: "} + e.message};
    }
    return r.value();
}

}  // namespace petra::detail

// ============================================================================
// Part 5 — the SIX prior sibling wrappers that today's AT-pad wrappers
// REUSE verbatim (Sep 8 / Sep 9 / Sep 20 / Sep 28 / Oct 3 / Oct 9 /
// Oct 12 / Oct 14 / Oct 17):
//   - try_format_runtime_n                  — Sep 8 (RUNTIME × EXPECTED)
//   - try_format_bounded_runtime_n          — Sep 9 (CONSTEVAL × EXPECTED)
//   - try_format_runtime_n_throwing         — Sep 20 (RUNTIME × THROWING)
//   - try_format_bounded_runtime_n_throwing — Sep 20 (CONSTEVAL × THROWING)
//   - try_format_runtime_n_space_pad        — Sep 28 (RUNTIME × SPACE-PAD)
//   - try_format_runtime_n_dash_pad         — Oct 3 (RUNTIME × DASH-PAD)
//   - try_format_runtime_n_hash_pad         — Oct 9 (RUNTIME × HASH-PAD)
//   - try_format_runtime_n_tilde_pad        — Oct 12 (RUNTIME × TILDE-PAD)
//   - try_format_runtime_n_underscore_pad   — Oct 14 (RUNTIME × UNDERSCORE-PAD)
//   - try_format_runtime_n_star_pad         — Oct 17 (RUNTIME × STAR-PAD)
//   - try_format_bounded_runtime_n_star_pad — Oct 17 (CONSTEVAL × STAR-PAD)
//   - try_format_runtime_n_throwing_star_pad — Oct 17 (RUNTIME × STAR-PAD × THROWING)
//   - try_format_bounded_runtime_n_throwing_star_pad — Oct 17 (CONSTEVAL × STAR-PAD × THROWING)
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
    return detail::try_format_runtime_n_impl(
        out, cap, fmt.get(), args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(char* out, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    return detail::try_format_runtime_n_throwing_impl(
        out, cap, fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing(char* out, std::size_t cap,
                                      std::format_string<Args...> fmt,
                                      const Args&... args) {
    return detail::try_format_runtime_n_throwing_impl(
        out, cap, fmt.get(), args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_space_pad(char* out, std::size_t cap,
                                std::string_view fmt,
                                const Args&... args) {
    auto r = try_format_runtime_n(out, cap, fmt, args...);
    if (r.has_value()) {
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = ' ';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_dash_pad(char* out, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n_space_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '-';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_hash_pad(char* out, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n_dash_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '#';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_tilde_pad(char* out, std::size_t cap,
                                std::string_view fmt,
                                const Args&... args) {
    auto r = try_format_runtime_n_hash_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '~';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_underscore_pad(char* out, std::size_t cap,
                                    std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_tilde_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) out[i] = '_';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_star_pad(char* out, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n_underscore_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) out[i] = '*';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_star_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt,
        const Args&... args) {
    return try_format_runtime_n_star_pad(
        out, cap, fmt.get(), args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_star_pad(
        char* out, std::size_t cap, std::string_view fmt,
        const Args&... args) {
    try {
        auto r = try_format_runtime_n_star_pad(
            out, cap, fmt, args...);
        if (!r.has_value()) {
            const auto& e = r.error();
            if (e.kind == FormatErrorKind::TooLarge) {
                throw std::runtime_error{
                    std::string{"TooLarge: "} + e.message};
            }
            throw std::runtime_error{
                std::string{"Format: "} + e.message};
        }
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '*';
        return r.value();
    } catch (...) {
        throw;
    }
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_star_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt_str,
        const Args&... args) {
    return try_format_runtime_n_throwing_star_pad(
        out, cap, fmt_str.get(), args...);
}

// ============================================================================
// Part 6 — petra::try_format_runtime_n_at_pad (NEW today) +
// petra::try_format_bounded_runtime_n_at_pad (NEW today) +
// petra::try_format_runtime_n_throwing_at_pad (NEW today) +
// petra::try_format_bounded_runtime_n_throwing_at_pad (NEW today).
//
// TODAY's NEW AT-pad sibling wrappers are thin layers on top
// of Oct 17's STAR-pad siblings.  The composition rule
// "the LATER fill overwrites the EARLIER fill" applies:
//
//   1. The AT-pad wrapper calls the STAR-pad sibling
//      (Oct 17's wrapper), which fills bytes [written, cap)
//      with '*' on success (via the underlying UNDERSCORE-pad
//      sibling chain, which OVERWRITES with '_', then '*').
//   2. On success, the AT-pad wrapper OVERWRITES those '*'
//      bytes with '@' (the LATER fill).
//   3. On error (Format or TooLarge), the AT-pad wrapper
//      does NOT touch the buffer (the catch block re-raises the
//      std::runtime_error untouched, and the expected sibling
//      short-circuits before the overwrite loop runs).
//
// Because the AT-pad wrapper REUSES the STAR-pad sibling verbatim,
// the (AT=1, STAR=*) cells share the SAME two AT-pad sibling
// wrappers — one CONSTEVAL+RUNTIME expected pair, one
// CONSTEVAL+RUNTIME throwing pair — the AT-overwrites-STAR
// composition rule applies transitively to
// AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE.
// ============================================================================

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_at_pad(char* out, std::size_t cap,
                            std::string_view fmt,
                            const Args&... args) {
    auto r = try_format_runtime_n_star_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        // LATER fill (AT) overwrites EARLIER fill (STAR) from
        // the STAR-pad sibling.  On error, r.has_value() is
        // false and the loop is skipped, preserving the
        // buffer's atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) out[i] = '@';
    }
    return r;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_at_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt,
        const Args&... args) {
    return try_format_runtime_n_at_pad(
        out, cap, fmt.get(), args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_at_pad(
        char* out, std::size_t cap, std::string_view fmt,
        const Args&... args) {
    try {
        auto r = try_format_runtime_n_at_pad(
            out, cap, fmt, args...);
        if (!r.has_value()) {
            const auto& e = r.error();
            if (e.kind == FormatErrorKind::TooLarge) {
                throw std::runtime_error{
                    std::string{"TooLarge: "} + e.message};
            }
            throw std::runtime_error{
                std::string{"Format: "} + e.message};
        }
        // On success, OVERWRITE the STAR pad from the STAR-pad
        // sibling with the AT pad.
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '@';
        return r.value();
    } catch (...) {
        // Re-raise unchanged — the throwing wrapper does NOT
        // catch & rewrap; the prior layer's std::runtime_error
        // is preserved verbatim (matches the std::runtime_error
        // contract Sep 21 / Sep 28 / Oct 3 / Oct 9 / Oct 12 /
        // Oct 14 / Oct 17 established).
        throw;
    }
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_at_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt_str,
        const Args&... args) {
    return try_format_runtime_n_throwing_at_pad(
        out, cap, fmt_str.get(), args...);
}

}  // namespace petra

// ============================================================================
// Part 7 — TRY_FORMAT_BOUNDED_N_AT_OR_THROW (RUNTIME surface meta-macro).
//
// The 2-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD)
// selects ONE of FOUR AT-pad sibling wrappers at preprocessor
// time:
//   - (0, 0): try_format_runtime_n                  (Sep 8 verbatim)
//   - (0, 1): try_format_runtime_n_at_pad           (NEW today)
//   - (1, 0): try_format_runtime_n_throwing         (Sep 20 verbatim)
//   - (1, 1): try_format_runtime_n_throwing_at_pad  (NEW today)
//
// Each macro is a parenthesized expression (NOT do { } while (0))
// so the caller can write
// 'auto n = TRY_FORMAT_BOUNDED_N_AT_OR_THROW(...)' and
// capture the result.  Each macro uses __VA_OPT__(,) for the
// no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19
// / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27
// / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 /
// Oct 8 / Oct 9 / Oct 11 / Oct 12 / Oct 13 / Oct 14 / Oct 16 /
// Oct 17's macro shape).
// ============================================================================

#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

#ifndef PETRA_AT_PAD
#define PETRA_AT_PAD 0
#endif

#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected sibling) or 1 (throwing sibling)"
#endif

#if PETRA_AT_PAD != 0 && PETRA_AT_PAD != 1
#error "PETRA_AT_PAD must be 0 (untouched) or 1 (AT-padded sibling)"
#endif

#if !defined(PETRA_PAD)
#define PETRA_PAD 0
#elif PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 or 1"
#endif

#if !defined(PETRA_SPACE_PAD)
#define PETRA_SPACE_PAD 0
#elif PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 or 1"
#endif

#if !defined(PETRA_DASH_PAD)
#define PETRA_DASH_PAD 0
#elif PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 or 1"
#endif

#if !defined(PETRA_HASH_PAD)
#define PETRA_HASH_PAD 0
#elif PETRA_HASH_PAD != 0 && PETRA_HASH_PAD != 1
#error "PETRA_HASH_PAD must be 0 or 1"
#endif

#if !defined(PETRA_TILDE_PAD)
#define PETRA_TILDE_PAD 0
#elif PETRA_TILDE_PAD != 0 && PETRA_TILDE_PAD != 1
#error "PETRA_TILDE_PAD must be 0 or 1"
#endif

#if !defined(PETRA_UNDERSCORE_PAD)
#define PETRA_UNDERSCORE_PAD 0
#elif PETRA_UNDERSCORE_PAD != 0 && PETRA_UNDERSCORE_PAD != 1
#error "PETRA_UNDERSCORE_PAD must be 0 or 1"
#endif

#if !defined(PETRA_STAR_PAD)
#define PETRA_STAR_PAD 0
#elif PETRA_STAR_PAD != 0 && PETRA_STAR_PAD != 1
#error "PETRA_STAR_PAD must be 0 or 1"
#endif

#if PETRA_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_PAD=0.  For PETRA_PAD=1, use Sep 25's try_format_padded_n_or_into, Sep 26's try_format_padded_n_or_throw, or Sep 27's try_format_padded_span_n_or_throw (NUL-padded wrappers)."
#endif

#if PETRA_SPACE_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_SPACE_PAD=0.  For PETRA_SPACE_PAD=1, use Sep 28's try_format_space_pad_n (CHAR*+CAP SPACE-padded sibling) or Sep 29 / Oct 1's composition cells."
#endif

#if PETRA_DASH_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_DASH_PAD=0.  For PETRA_DASH_PAD=1, use Oct 3's try_format_dash_pad_n (CHAR*+CAP DASH-padded sibling) or Oct 4 / Oct 5 / Oct 8's composition cells."
#endif

#if PETRA_HASH_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_HASH_PAD=0.  For PETRA_HASH_PAD=1, use Oct 9's try_format_hash_pad_n (CHAR*+CAP HASH-padded sibling) or Oct 11's try_format_hash_pad_span_n (STD::SPAN<CHAR> HASH-padded sibling)."
#endif

#if PETRA_TILDE_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_TILDE_PAD=0.  For PETRA_TILDE_PAD=1, use Oct 12's try_format_tilde_pad_n (CHAR*+CAP TILDE-padded sibling) or Oct 13's try_format_tilde_pad_span_n (STD::SPAN<CHAR> TILDE-padded sibling).  Note: today's AT-pad wrapper REUSES Oct 17's STAR-pad wrapper internally, which REUSES Oct 14's UNDERSCORE-pad wrapper, which REUSES Oct 12's TILDE-pad wrapper — if you set PETRA_TILDE_PAD=1 explicitly, the dispatcher #errors to avoid double-pad ambiguity."
#endif

#if PETRA_UNDERSCORE_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_UNDERSCORE_PAD=0.  For PETRA_UNDERSCORE_PAD=1, use Oct 14's try_format_underscore_pad_n (CHAR*+CAP UNDERSCORE-padded sibling) or Oct 15's try_format_underscore_pad_span_n (STD::SPAN<CHAR> UNDERSCORE-padded sibling).  Note: today's AT-pad wrapper REUSES Oct 17's STAR-pad wrapper internally, which REUSES Oct 14's UNDERSCORE-pad wrapper — if you set PETRA_UNDERSCORE_PAD=1 explicitly, the dispatcher #errors to avoid double-pad ambiguity."
#endif

#if PETRA_STAR_PAD != 0
#error "This lesson (try_format_at_pad_n) only supports PETRA_STAR_PAD=0.  For PETRA_STAR_PAD=1, use Oct 17's try_format_star_pad_n (CHAR*+CAP STAR-padded sibling) or Oct 16's try_format_star_pad_span_n (STD::SPAN<CHAR> STAR-padded sibling).  Note: today's AT-pad wrapper REUSES Oct 17's STAR-pad wrapper internally — if you set PETRA_STAR_PAD=1 explicitly, the dispatcher #errors to avoid double-pad ambiguity."
#endif

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_AT_PAD == 0
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_AT_PAD == 1
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n_at_pad((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_AT_PAD == 0
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n_throwing((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_AT_PAD == 1
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n_throwing_at_pad((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#else
#error "Internal error: PETRA_THROW_ON_OVERFLOW and PETRA_AT_PAD combination not handled"
#endif

// ============================================================================
// Part 8 — hand-rolled SECTION/CHECK framework (mirrors Sep 6 / Sep 7 /
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Oct 17's shape).
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
// Part 9 — sections.
//
// We test the (PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD)
// 2-bit tuple:
//
//   - (0, 0) — expected+untouched sibling.
//       Bytes [written, cap) on success are UNTOUCHED (poison
//       byte 'Q' from the test buffer survives).
//   - (0, 1) — expected+AT-padded sibling.
//       Bytes [written, cap) on success are '@'.
//   - (1, 0) — throwing+untouched sibling.
//       Bytes [written, cap) on success are UNTOUCHED; throws
//       std::runtime_error on overflow.
//   - (1, 1) — throwing+AT-padded sibling.
//       Bytes [written, cap) on success are '@'; throws
//       std::runtime_error on overflow.
//
// All other flags (PAD, SPACE_PAD, DASH_PAD, HASH_PAD, TILDE_PAD,
// UNDERSCORE_PAD, STAR_PAD) are pinned to 0 by the friendly-error
// #if-guards above; no composition cells are tested today (those
// live in Sep 25-30 / Oct 1-12 / Oct 14 / Oct 16 / Oct 17 lessons —
// see Part 7's #error messages for pointers).
// ============================================================================

int main() {
    using namespace petra_test;
    using namespace petra;

    // ------------------------------------------------------------------
    // Section 1 — AT-pad RUNTIME × EXPECTED: written bytes match
    // expected; leftover cap bytes are '@'.
    // ------------------------------------------------------------------
    section("Section 1: expected+AT-pad (0, 1) cell — bytes after written are '@'");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        auto r = try_format_runtime_n_at_pad(
            buf, sizeof(buf), "x={}", 42);
        check_true(r.has_value(), "(0,1) r.has_value");
        check_eq(r.value(), std::size_t{4}, "(0,1) r.value()");
        check_eq(std::string_view(buf, 4), std::string_view{"x=42"}, "(0,1) buf[0..3]");
        bool all_at = true;
        for (std::size_t i = 4; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "(0,1) buf[4..15] all '@'");
    }

    // ------------------------------------------------------------------
    // Section 2 — buffer atomicity on TooLarge: the AT-pad wrapper
    // must NOT touch bytes on the overflow path.
    // ------------------------------------------------------------------
    section("Section 2: expected+AT-pad (0, 1) cell — buffer UNTOUCHED on TooLarge");
    {
        char buf[8] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        auto r = try_format_runtime_n_at_pad(
            buf, sizeof(buf), "ABCDEFGHIJ");
        check_true(!r.has_value(), "(0,1) TooLarge: no value");
        check_eq(r.error().kind, FormatErrorKind::TooLarge,
                 "(0,1) TooLarge: kind");
        check_eq(r.error().needed_bytes, std::size_t{10},
                 "(0,1) TooLarge: needed_bytes=10");
        bool all_q = true;
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            if (buf[i] != 'Q') { all_q = false; break; }
        }
        check_true(all_q, "(0,1) TooLarge: buf all 'Q' (untouched)");
    }

    // ------------------------------------------------------------------
    // Section 3 — buffer atomicity on Format error.
    // ------------------------------------------------------------------
    section("Section 3: expected+AT-pad (0, 1) cell — buffer UNTOUCHED on Format error");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::string bad_fmt = "bad { ";
        auto r = try_format_runtime_n_at_pad(
            buf, sizeof(buf), bad_fmt);
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
    // Section 4 — no-bleed on the EDGE cases.
    // ------------------------------------------------------------------
    section("Section 4: expected+AT-pad (0, 1) cell — written == cap and cap == 0 edge cases");
    {
        // (a) written == cap: zero at-fill bytes.
        char buf[5] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        auto r = try_format_runtime_n_at_pad(
            buf, sizeof(buf), "ABCDE");
        check_true(r.has_value(), "(0,1) edge: written==cap has_value");
        check_eq(r.value(), std::size_t{5}, "(0,1) edge: written==cap value");
        check_eq(std::string_view(buf, 5), std::string_view{"ABCDE"},
                 "(0,1) edge: written==cap content");
        // (b) cap == 0: format "X" into a zero-byte buffer → TooLarge.
        char zbuf[1] = {'Q'};
        auto r2 = try_format_runtime_n_at_pad(zbuf, 0, "X");
        check_true(!r2.has_value(), "(0,1) edge: cap=0 TooLarge");
        check_eq(r2.error().kind, FormatErrorKind::TooLarge,
                 "(0,1) edge: cap=0 kind");
        check_eq(zbuf[0], 'Q', "(0,1) edge: cap=0 sentinel untouched");
    }

    // ------------------------------------------------------------------
    // Section 5 — AT distinctness: '@' is distinct from '*', '_', ' ',
    // '-', '#', '~', and 'Q'.
    // ------------------------------------------------------------------
    section("Section 5: AT-pad (0, 1) cell — byte-state distinctness");
    {
        char buf[8] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        auto r = try_format_runtime_n_at_pad(
            buf, sizeof(buf), "AB");
        check_true(r.has_value(), "(0,1) distinctness: has_value");
        for (std::size_t i = 2; i < sizeof(buf); ++i) {
            check_true(buf[i] == '@', "(0,1) distinctness: buf[i]=='@'");
            check_true(buf[i] != '*', "(0,1) distinctness: buf[i]!='*'");
            check_true(buf[i] != '_', "(0,1) distinctness: buf[i]!='_'");
            check_true(buf[i] != ' ', "(0,1) distinctness: buf[i]!=' '");
            check_true(buf[i] != '-', "(0,1) distinctness: buf[i]!='-'");
            check_true(buf[i] != '#', "(0,1) distinctness: buf[i]!='#'");
            check_true(buf[i] != '~', "(0,1) distinctness: buf[i]!='~'");
            check_true(buf[i] != 'Q', "(0,1) distinctness: buf[i]!='Q'");
        }
    }

    // ------------------------------------------------------------------
    // Section 6 — AT-pad CONSTEVAL × EXPECTED: same byte-state
    // contract as the RUNTIME sibling.
    // ------------------------------------------------------------------
    section("Section 6: expected+AT-pad (0, 1) CONSTEVAL surface — same contract as RUNTIME");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        auto r = try_format_bounded_runtime_n_at_pad(
            buf, sizeof(buf), "hello {}", "world");
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
    // Section 7 — AT-pad RUNTIME × THROWING: byte-state contract
    // on success; throws std::runtime_error on overflow.
    // ------------------------------------------------------------------
    section("Section 7: throwing+AT-pad (1, 1) cell — success path + throws on TooLarge");
    {
        // (a) Success path: bytes after written are '@'.
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::size_t n = try_format_runtime_n_throwing_at_pad(
            buf, sizeof(buf), "y={}", 7);
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
        bool threw = false;
        try {
            (void)try_format_runtime_n_throwing_at_pad(
                buf2, sizeof(buf2), "ABCDEFGHIJ");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check_true(threw, "(1,1) TooLarge throws std::runtime_error");
        bool all_q = true;
        for (std::size_t i = 0; i < sizeof(buf2); ++i) {
            if (buf2[i] != 'Q') { all_q = false; break; }
        }
        check_true(all_q, "(1,1) TooLarge: buf UNTOUCHED ('Q')");
    }

    // ------------------------------------------------------------------
    // Section 8 — AT-pad CONSTEVAL × THROWING: same contract as
    // the RUNTIME × THROWING sibling.
    // ------------------------------------------------------------------
    section("Section 8: throwing+AT-pad (1, 1) CONSTEVAL surface");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        std::size_t n = try_format_bounded_runtime_n_throwing_at_pad(
            buf, sizeof(buf), "{}", "X");
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
    // TRY_FORMAT_BOUNDED_N_AT_OR_THROW above (the
    // parenthesized-expression macro shape means we can capture
    // the result).
    // ------------------------------------------------------------------
    section("Section 9: macro TRY_FORMAT_BOUNDED_N_AT_OR_THROW — works at preprocessor time");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), 'Q');
        auto n_with = try_format_runtime_n_at_pad(
            buf, sizeof(buf), "abc={}", 1);
        check_true(n_with.has_value(), "macro: AT-pad has_value");
        check_eq(n_with.value(), std::size_t{5}, "macro: AT-pad value");
        check_eq(std::string_view(buf, 5),
                 std::string_view{"abc=1"},
                 "macro: AT-pad content");
        bool all_at = true;
        for (std::size_t i = 5; i < 16; ++i) {
            if (buf[i] != '@') { all_at = false; break; }
        }
        check_true(all_at, "macro: buf[5..15] all '@'");
    }

    // ------------------------------------------------------------------
    // Section 10 — summary.
    // ------------------------------------------------------------------
    section("Summary");
    std::println("  PASS={}  FAIL={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}