// P-2026-09-25 — try_format_padded_n_or_into:
//
// A NEW FIFTH AXIS that lifts the **PAD-VS-UNTOUCHED** sibling choice to
// a meta-macro form on the CHAR*+CAP × BOUNDED-BUFFER axis.  Two new
// meta-macros — TRY_FORMAT_BOUNDED_N_PAD (CONSTEVAL surface) and
// TRY_FORMAT_BOUNDED_RUNTIME_N_PAD (RUNTIME surface) — dispatch to
// EITHER the **untouched** sibling (Sep 9's try_format_bounded_runtime_n
// for the CONSTEVAL surface, Sep 8's try_format_runtime_n for the
// RUNTIME surface; both return std::size_t on success and leave the
// bytes [written, cap) UNTOUCHED on success) OR today's NEW
// **NUL-padded** sibling (today's
// petra::try_format_bounded_runtime_n_padded for the CONSTEVAL surface,
// today's petra::try_format_runtime_n_padded for the RUNTIME surface;
// both also return std::size_t on success but ZERO-FILL the bytes
// [written, cap) to '\0' on success), based on a caller-controlled
// compile-time flag PETRA_PAD.
//
// This is the FIFTH axis orthogonal to Sep 23's completed 16-cell
// matrix on the char*+cap × CONSTEVAL-or-RUNTIME × bounded-vs-NUL ×
// expected-vs-throwing quadrant.  Adding PETRA_PAD as a 5th axis
// extends the matrix from 16 cells to 32 cells (2^5).  Today's
// lesson covers 2 NEW cells on the char*+cap × CONSTEVAL-or-RUNTIME
// × bounded-buffer × expected × padded axis (the OTHER variant —
// throwing sibling on the padded axis — is left as Sep 25's "Where
// we go next" item #1, to be picked up in a future lesson).
//
// Today's lesson is the FIRST axis-extension after the 16-cell
// matrix was completed by Sep 23's lesson.  It demonstrates that
// the preprocessor-time dispatch design from Sep 13 / Sep 14 / Sep
// 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23
// generalizes naturally to a NEW axis: each new axis is a new
// #if PETRA_<NEW_FLAG> block layered on top of the existing axes,
// composes orthogonally with all prior axes, and adds 2x as many
// cells to the matrix (one new sibling per existing cell).
//
// What today adds (all verbatim thin wrappers / dispatchers):
//   - petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
//     std::format_string<Args...>, args...) — NEW today; wraps Sep 9's
//     try_format_bounded_runtime_n and zero-fills [written, cap) on
//     success.
//   - petra::try_format_runtime_n_padded(char*, std::size_t,
//     std::string_view, args...) — NEW today; wraps Sep 8's
//     try_format_runtime_n and zero-fills [written, cap) on success.
//   - The TWO new meta-macros TRY_FORMAT_BOUNDED_N_PAD (CONSTEVAL
//     surface) and TRY_FORMAT_BOUNDED_RUNTIME_N_PAD (RUNTIME surface).
//     Each is a verbatim thin dispatcher that picks between the
//     untouched sibling (PETRA_PAD=0) and the NUL-padded sibling
//     (PETRA_PAD=1) at preprocessor time.  Each macro is a parenthesized
//     expression (NOT do { } while (0)) so the caller can write
//     'auto n = TRY_FORMAT_BOUNDED_N_PAD(...)' and capture the result.
//     Each macro uses __VA_OPT__(,) for the no-args case (mirrors Sep 13
//     / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
//     Sep 23's macro shape).
//
// Where this fits in the arc
// --------------------------
//   - Aug 25   std::format_to_n_into — NUL-terminating bounded
//              formatter WITHOUT a cap parameter.
//   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
//              std::size_t cap (RUNTIME std::string-output wrapper).
//   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
//   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
//              Sep 7 (char* + std::size_t cap + std::string_view
//              fmt, NO NUL).
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
//              axis (the FOURTH axis; covers the char*+cap ×
//              bounded-buffer × expected-vs-throwing cells).
//   - Sep 21   try_format_span_n_or_throw — the RETURN-TYPE-vs-
//              EXCEPTION sibling-choice macro layer on the
//              std::span<char> axis (the FOURTH axis on the
//              span<char> side; covers the span<char> × bounded-
//              buffer × expected-vs-throwing cells).
//   - Sep 22   try_format_bounded_n_or_throw_into — the NUL-
//              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
//              meta-macro layer on the CHAR*+CAP axis (covers the
//              char*+cap × NUL-terminating × expected-vs-throwing
//              cells — closes Sep 21's "Where we go next" item #1
//              verbatim).
//   - Sep 23   try_format_span_n_or_throw_into — the NUL-
//              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
//              meta-macro layer on the STD::SPAN<CHAR> axis (covers
//              the span<char> × NUL-terminating × expected-vs-
//              throwing cells — closes Sep 22's "Where we go next"
//              item #1 verbatim; the FOUR-AXIS BOUNDED-vs-NUL ×
//              expected-vs-throwing quadrant of the 16-cell matrix
//              is now COMPLETE on BOTH the char*+cap and span<char>
//              axes).
//   - TODAY    try_format_padded_n_or_into — the FIFTH axis: the
//              PAD-vs-UNTOUCHED sibling-choice macro layer on the
//              CHAR*+CAP × BOUNDED-BUFFER axis (covers 2 NEW cells
//              on the char*+cap × CONSTEVAL-or-RUNTIME × bounded-
//              buffer × expected × padded axis; the matrix grows
//              from 16 to 32 cells on the BOUNDED-BUFFER × expected
//              quadrant on the char*+cap side).
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
//     Sep 23 verbatim (the wrappers below do not produce FormatError
//     directly — they only forward to existing wrappers, but the type
//     must be visible in scope for std::expected<size_t, FormatError>
//     to be instantiatable by the static_assert below).
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — REUSED verbatim.
//   - petra::counting_output_iterator — REUSED verbatim.
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
//     petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim)
//     — REUSED verbatim (the bounded-buffer underlying impls).
//   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
//     args...) — REUSED Sep 8 verbatim (the RUNTIME untouched sibling
//     that today's NEW padded wrapper composes on top of).
//   - petra::try_format_bounded_runtime_n(char*, std::size_t,
//     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim
//     (the CONSTEVAL untouched sibling that today's NEW padded
//     wrapper composes on top of).
//   - petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
//     std::format_string<Args...>, args...) — NEW today (CONSTEVAL
//     surface of the BOUNDED-BUFFER × PAD sibling on the char*+cap
//     axis; wraps Sep 9's try_format_bounded_runtime_n verbatim and
//     zero-fills [written, cap) to '\0' on success).
//   - petra::try_format_runtime_n_padded(char*, std::size_t,
//     std::string_view, args...) — NEW today (RUNTIME surface of
//     the BOUNDED-BUFFER × PAD sibling on the char*+cap axis; wraps
//     Sep 8's try_format_runtime_n verbatim and zero-fills
//     [written, cap) to '\0' on success).
//   - The TWO new meta-macros TRY_FORMAT_BOUNDED_N_PAD (CONSTEVAL
//     surface) and TRY_FORMAT_BOUNDED_RUNTIME_N_PAD (RUNTIME surface).
//     Each is a verbatim thin dispatcher that picks between the
//     untouched sibling (PETRA_PAD=0) and the NUL-padded sibling
//     (PETRA_PAD=1) at preprocessor time.  Default PETRA_PAD=0
//     (untouched sibling — the prior contract across Sep 6 through
//     Sep 23).
//
// What today's lesson pins
// ------------------------
//   1. The PAD-VS-UNTOUCHED sibling choice CAN be lifted to a meta-
//      macro form.  The shape composes on top of Sep 8's / Sep 9's
//      bounded-buffer underlying wrappers.  Each TODAY's meta-macro
//      is a verbatim thin wrapper around ONE OF TWO underlying
//      wrappers:
//        - bounded-buffer × untouched:        Sep 8 / Sep 9 char*+cap
//                                              wrappers
//        - bounded-buffer × NUL-padded:       TODAY's NEW padded
//                                              char*+cap wrappers
//      The NUL-terminating variants are NOT covered by today's macros
//      (the NUL-terminating sibling already places '\0' at the
//      truncation-edge or full-success position; adding a pad sibling
//      on the NUL-terminating axis would be redundant — the bytes
//      after the NUL are already whatever they were before the call).
//   2. The compile-time flag PETRA_PAD controls the dispatch via
//      #if PETRA_PAD == 0/1:
//        - #define PETRA_PAD 0 → untouched sibling (returns
//          std::size_t on success; bytes [written, cap) UNTOUCHED).
//            CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//            RUNTIME:   petra::try_format_runtime_n (Sep 8)
//        - #define PETRA_PAD 1 → NUL-padded sibling (returns
//          std::size_t on success; bytes [written, cap) zero-filled
//          to '\0').
//            CONSTEVAL: petra::try_format_bounded_runtime_n_padded
//                      (NEW today; wraps Sep 9 verbatim)
//            RUNTIME:   petra::try_format_runtime_n_padded
//                      (NEW today; wraps Sep 8 verbatim)
//   3. The #error guard fires at preprocessor time if the flag is
//      set to anything other than 0 or 1.
//   4. The macro is a parenthesized expression (NOT do { ... } while
//      (0)) so the caller can write
//      'auto n = TRY_FORMAT_BOUNDED_N_PAD(...)' and capture the
//      result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
//      Sep 20 / Sep 21 / Sep 22 / Sep 23's macro shape.
//   5. __VA_OPT__(,) for the no-args case.  Mirrors Sep 13 / Sep 14 /
//      Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23.
//   6. The CONSTEVAL macro picks between Sep 9's
//      try_format_bounded_runtime_n (untouched, returns std::size_t)
//      and today's NEW try_format_bounded_runtime_n_padded (NUL-padded,
//      returns std::size_t).  Both are consteval-checked
//      std::format_string<Args...> wrappers.
//   7. The RUNTIME macro picks between Sep 8's try_format_runtime_n
//      (untouched, returns std::size_t) and today's NEW
//      try_format_runtime_n_padded (NUL-padded, returns std::size_t).
//      Both take std::string_view format-string (no compile-time
//      parse).
//   8. The padded sibling preserves the SAME byte-count contract on
//      success — returns std::size_t == bytes written (the same value
//      Sep 8's / Sep 9's untouched siblings return).  Section 4 pins
//      this on both paths.
//   9. The padded sibling preserves the SAME atomicity on TooLarge
//      as the untouched sibling — the buffer is UNTOUCHED on overflow
//      (Sep 8 / Sep 9 already pin this on the untouched path;
//      today's wrappers re-pin it on the padded path because the
//      new code path COULD theoretically diverge; Section 3c pins
//      this on the padded path).
//  10. The padded sibling preserves the SAME consteval gate — the
//      CONSTEVAL macro still requires std::format_string<Args...>
//      (a literal fmt at the call site).  Section 6 pins the
//      consteval gate by calling the macro with a literal fmt and
//      a runtime-built std::string fmt; the runtime-built fmt
//      REJECTS AT COMPILE TIME on the CONSTEVAL macro (verified by
//      /tmp/probe_consteval_rejects_runtime_fmt_pad.cpp producing
//      the same 'error: call to consteval function ... is not a
//      constant expression' as Sep 13 / Sep 16 / Sep 19 / Sep 20 /
//      Sep 21 / Sep 22 / Sep 23's probes).
//  11. The PETRA_PAD axis (untouched vs NUL-padded) is PERPENDICULAR
//      to the FOUR PRIOR AXES — char*-vs-span (Sep 10 / Sep 12),
//      CONSTEVAL-vs-RUNTIME (Sep 13), bounded-buffer-vs-NUL-
//      terminating (Sep 16), and expected-vs-throwing (Sep 20).
//      Today's meta-macros are at the INTERSECTION of all FIVE
//      axes on the char*+cap × bounded-buffer × expected ×
//      CONSTEVAL-or-RUNTIME × untouched-or-padded cell.  Section
//      11's decltype checks pin this on BOTH paths.
//  12. The padded sibling's byte-state contract — bytes [written, cap)
//      are '\0' on success — is pinned by Section 2 (which pre-fills
//      the buffer with poison 'Q' bytes, runs the macro, and asserts
//      every byte at index >= written is '\0').
//  13. Concurrency is safe — Section 10 fires 4 threads × 25
//      concurrent calls through both macros; per-thread local
//      counters are aggregated AFTER join() via
//      std::atomic<int>::fetch_add to avoid a data race on
//      g_pass / g_fail.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::expected (P0323R12),
// std::println (P2093R14), std::atomic, std::is_same_v, __VA_OPT__
// preprocessor feature.  Requires -fexperimental-library on Apple
// Clang 21.0.0 / libc++ 21.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
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
// Sep 20 / Sep 21 / Sep 22 / Sep 23)
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
// Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23)
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
// Sep 20 / Sep 21 / Sep 22 / Sep 23)
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
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim — the
// BOUNDED-BUFFER underlying impl on the RUNTIME surface) +
// petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim — the
// BOUNDED-BUFFER underlying impl on the CONSTEVAL surface).  Today's NEW
// padded wrappers compose on top of Sep 8's / Sep 9's char*+cap wrappers,
// which compose on top of these impls.
//
// Note: this impl differs from Sep 11's try_format_to_n_into_bounded_impl —
// Sep 11 reserves one slot for the NUL on success, while today's impl
// treats cap as the FULL writable byte count (NO NUL).
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
// petra::try_format_bounded_runtime_n (Sep 9 verbatim).
//
// Today's NEW NUL-padded wrappers compose on top of these char*+cap
// untouched-sibling wrappers.
// ============================================================================

namespace petra {

// Sep 8 verbatim — RUNTIME untouched sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap,
                     std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 9 verbatim — CONSTEVAL untouched sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

// NEW today — CONSTEVAL surface of the BOUNDED-BUFFER × NUL-PADDED
// sibling.  A verbatim thin wrapper around Sep 9's
// try_format_bounded_runtime_n that, on success, zero-fills
// out[written, cap) to '\0' AFTER the underlying call.  On error
// (TooLarge / Format), the wrapper forwards the expected<size_t,
// FormatError> UNCHANGED without touching the buffer (atomicity
// preserved by the underlying impl).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded(char* out, std::size_t cap,
                                     std::format_string<Args...> fmt,
                                     const Args&... args) {
    auto r = try_format_bounded_runtime_n(out, cap, fmt, args...);
    if (r.has_value()) {
        // NUL-pad out[written, cap) to '\0'.
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// NEW today — RUNTIME surface of the BOUNDED-BUFFER × NUL-PADDED
// sibling.  A verbatim thin wrapper around Sep 8's try_format_runtime_n
// that, on success, zero-fills out[written, cap) to '\0' AFTER the
// underlying call.  On error, the wrapper forwards the
// expected<size_t, FormatError> UNCHANGED without touching the buffer.
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

}  // namespace petra

// ============================================================================
// Part 6 — TRY_FORMAT_BOUNDED_N_PAD / TRY_FORMAT_BOUNDED_RUNTIME_N_PAD
// NEW TODAY — two meta-macros that lift the PAD-VS-UNTOUCHED sibling
// choice to a meta-macro form on the char*+cap × BOUNDED-BUFFER axis.
//
// The compile-time flag PETRA_PAD controls the dispatch (PETRA_NUL_TERMINATE
// is implicitly 0 — the bounded-buffer cells are Sep 16's / Sep 20's domain;
// PETRA_THROW_ON_OVERFLOW is implicitly 0 — the throwing sibling is Sep
// 20's domain):
//
//   #define PETRA_PAD 0
//     → untouched sibling (returns std::size_t on success; bytes
//       [written, cap) UNTOUCHED on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//        RUNTIME:  petra::try_format_runtime_n (Sep 8)
//
//   #define PETRA_PAD 1
//     → NUL-padded sibling (returns std::size_t on success; bytes
//       [written, cap) zero-filled to '\0' on success):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_padded
//                  (NEW today; wraps Sep 9 verbatim)
//        RUNTIME:  petra::try_format_runtime_n_padded
//                  (NEW today; wraps Sep 8 verbatim)
//
// Default: PETRA_PAD = 0 (untouched sibling — matches Sep 6 through
// Sep 23's contract).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_N_PAD(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 /
// Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23's macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Mirrors
// Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
// Sep 23.
//
// Today is the FIRST axis-extension after Sep 23 closed the
// 16-cell matrix.  The matrix is now 32 cells (2^5) on the
// BOUNDED-BUFFER × expected quadrant on the char*+cap side.
// ============================================================================

// PETRA_PAD defaults to 0 (untouched sibling — the prior contract
// across Sep 6 through Sep 23).  The user can override before
// including this header (or at the top of the .cpp file).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
#endif

#if PETRA_PAD == 0
// Untouched sibling path — returns std::expected<std::size_t, FormatError>.
// On TooLarge, FormatError{TooLarge, ..., needed_bytes} is returned;
// the buffer is UNTOUCHED on overflow.

// CONSTEVAL surface: dispatches to Sep 9's try_format_bounded_runtime_n.
#define TRY_FORMAT_BOUNDED_N_PAD(out, cap, fmt, ...)                      \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)            \
                                           __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 8's try_format_runtime_n.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(out, cap, fmt, ...)              \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                    \
                                   __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_PAD == 1
// NUL-padded sibling path — returns std::expected<std::size_t,
// FormatError>.  On success, bytes [written, cap) are zero-filled to
// '\0'.  On overflow or Format, the buffer is UNTOUCHED (atomicity
// preserved by the underlying impl, and the padded wrapper does
// NOT touch the buffer on error — see Section 3c).

// CONSTEVAL surface: dispatches to today's NEW
// try_format_bounded_runtime_n_padded.
#define TRY_FORMAT_BOUNDED_N_PAD(out, cap, fmt, ...)                      \
    (::petra::try_format_bounded_runtime_n_padded((out), (cap), (fmt)     \
                                                  __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to today's NEW
// try_format_runtime_n_padded.
#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(out, cap, fmt, ...)              \
    (::petra::try_format_runtime_n_padded((out), (cap), (fmt)             \
                                          __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_PAD

// ============================================================================
// Part 7 — Helper dispatcher macros used in Section 11's decltype checks
// to pin the perpendicular-axis claim.
//
// These are NOT new surface; they are simply verbatim thin dispatchers
// to the untouched siblings on the BOUNDED-BUFFER char*+cap axis.
// Used to prove that today's meta-macros dispatch to the same
// wrappers as these "flat" dispatchers under PETRA_PAD=0.
// ============================================================================

#define TRY_FORMAT_BOUNDED_N_PAD_ORIG(out, cap, fmt, ...)                 \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)            \
                                           __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_ORIG(out, cap, fmt, ...)         \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                    \
                                   __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 8 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23).
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
// Part 9 — sections
// ============================================================================

int main() {
    // ----- Section 1 — PETRA_PAD compile-time flag pinned
    SECTION("Section 1 - PETRA_PAD compile-time flag");
    static_assert(PETRA_PAD == 0 || PETRA_PAD == 1,
                  "PETRA_PAD must be 0 or 1");
#if PETRA_PAD == 0
    std::println("[Section 1] PETRA_PAD=0 (untouched sibling)");
#else
    std::println("[Section 1] PETRA_PAD=1 (NUL-padded sibling)");
#endif
    CHECK(true);

    // 1b. Pin that the macros dispatch to the correct sibling based
    //     on PETRA_PAD.  Verify by checking the macro's expansion
    //     type matches the underlying wrapper.
#if PETRA_PAD == 0
    {
        // Untouched sibling path: BOTH surfaces return
        // expected<std::size_t, FormatError>.
        using R1b = decltype(TRY_FORMAT_BOUNDED_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using Expected1b = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<R1b, Expected1b>,
                      "TRY_FORMAT_BOUNDED_N_PAD must return "
                      "std::expected<std::size_t, FormatError> when "
                      "PETRA_PAD=0");

        using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R1c, Expected1b>,
                      "TRY_FORMAT_BOUNDED_RUNTIME_N_PAD must return "
                      "std::expected<std::size_t, FormatError> when "
                      "PETRA_PAD=0");
    }
#else
    {
        // NUL-padded sibling path: BOTH surfaces return
        // expected<std::size_t, FormatError>.
        using R1b = decltype(TRY_FORMAT_BOUNDED_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using Expected1b = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<R1b, Expected1b>,
                      "TRY_FORMAT_BOUNDED_N_PAD must return "
                      "std::expected<std::size_t, FormatError> when "
                      "PETRA_PAD=1");

        using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R1c, Expected1b>,
                      "TRY_FORMAT_BOUNDED_RUNTIME_N_PAD must return "
                      "std::expected<std::size_t, FormatError> when "
                      "PETRA_PAD=1");
    }
#endif
    CHECK(true);

    // ----- Section 2 — byte-state contract on success
    SECTION("Section 2 - byte-state contract on success");
    {
        // 2a. CONSTEVAL macro: pre-fill the buffer with poison 'Q'
        //     bytes; on success, every byte at index >= written must
        //     be '\0' (PETRA_PAD=1) OR must still be 'Q' (PETRA_PAD=0).
        char buf2a_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf2a_arr[i] = static_cast<char>('Q');
        char* buf2a = buf2a_arr;
        auto r2a = TRY_FORMAT_BOUNDED_N_PAD(
            buf2a, 16, "hello {}\n", "world");
        CHECK(r2a.has_value());
        CHECK_EQ(r2a.value(), std::size_t{12});  // "hello world\n" = 12 bytes
        // Bytes [0, 12) are the formatted output.
        CHECK_EQ((std::string_view{buf2a_arr, 12}),
                 (std::string_view{"hello world\n"}));
#if PETRA_PAD == 1
        // NUL-padded: bytes [12, 16) must be '\0'.
        for (std::size_t i = 12; i < 16; ++i) {
            CHECK_EQ(buf2a_arr[i], '\0');
        }
#else
        // Untouched: bytes [12, 16) must still be 'Q' (poison).
        for (std::size_t i = 12; i < 16; ++i) {
            CHECK_EQ(buf2a_arr[i], 'Q');
        }
#endif
    }

    // 2b. RUNTIME macro: same shape.
    {
        char buf2b_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf2b_arr[i] = static_cast<char>('Q');
        char* buf2b = buf2b_arr;
        std::string fmt2b = "x={}";
        auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            buf2b, 16, fmt2b, 42);
        CHECK(r2b.has_value());
        CHECK_EQ(r2b.value(), std::size_t{4});
        CHECK_EQ((std::string_view{buf2b_arr, 4}),
                 (std::string_view{"x=42"}));
#if PETRA_PAD == 1
        for (std::size_t i = 4; i < 16; ++i) {
            CHECK_EQ(buf2b_arr[i], '\0');
        }
#else
        for (std::size_t i = 4; i < 16; ++i) {
            CHECK_EQ(buf2b_arr[i], 'Q');
        }
#endif
    }

    // 2c. Edge case: cap == 0.  With empty fmt, success (returns 0)
    //     and the loop body never executes (no bytes to pad).
    {
        char buf2c_arr[1] = {};
        buf2c_arr[0] = 'Q';
        char* buf2c = buf2c_arr;
        auto r2c = TRY_FORMAT_BOUNDED_N_PAD(buf2c, 0, "");
        CHECK(r2c.has_value());
        CHECK_EQ(r2c.value(), std::size_t{0});
        // The 'Q' is OUTSIDE the cap=0 writable region; today's padded
        // wrapper never touches it (the loop condition `i < cap` is
        // false from the start).
        CHECK_EQ(buf2c_arr[0], 'Q');
    }

    // 2d. Edge case: full-buffer success (written == cap).  NUL-padded
    //     loop condition `i < cap` is false from the start, so no
    //     extra bytes are touched.
    {
        char buf2d_arr[8] = {};
        for (std::size_t i = 0; i < 8; ++i) buf2d_arr[i] = static_cast<char>('Q');
        char* buf2d = buf2d_arr;
        // "ABCDEFGH" (8 bytes) into cap=8: needed == cap, full-buffer
        // success.
        auto r2d = TRY_FORMAT_BOUNDED_N_PAD(buf2d, 8, "ABCDEFGH");
        CHECK(r2d.has_value());
        CHECK_EQ(r2d.value(), std::size_t{8});
        CHECK_EQ((std::string_view{buf2d_arr, 8}),
                 (std::string_view{"ABCDEFGH"}));
        // Every byte IS the formatted output — no bytes to pad.
        // The loop `for (i = 8; i < 8; ++i)` is empty.
    }

    // ----- Section 3 — atomicity on TooLarge
    SECTION("Section 3 - atomicity on TooLarge");
    {
        // 3a. CONSTEVAL macro: TooLarge returns FormatError{TooLarge, ...}
        //     and the buffer is UNTOUCHED.
        char buf3a_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3a_arr[i] = static_cast<char>('Q');
        char* buf3a = buf3a_arr;
        auto r3a = TRY_FORMAT_BOUNDED_N_PAD(
            buf3a, 16, "too long: {}\n", std::string(20, 'X'));
        CHECK(!r3a.has_value());
        CHECK_EQ(r3a.error().kind, petra::FormatErrorKind::TooLarge);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3a_arr[i], 'Q');
        }
    }

    // 3b. RUNTIME macro: TooLarge returns FormatError{TooLarge, ...}
    //     and the buffer is UNTOUCHED.
    {
        char buf3b_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3b_arr[i] = static_cast<char>('Q');
        char* buf3b = buf3b_arr;
        std::string fmt3b = "too long: {}\n";
        auto r3b = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            buf3b, 16, fmt3b, std::string(20, 'X'));
        CHECK(!r3b.has_value());
        CHECK_EQ(r3b.error().kind, petra::FormatErrorKind::TooLarge);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3b_arr[i], 'Q');
        }
    }

    // 3c. The HEADLINE finding on the padded path — the padded wrapper
    //     preserves atomicity on TooLarge.  Even though the padded
    //     wrapper has logic that COULD theoretically touch the buffer
    //     on success (`for (i = written; i < cap; ++i) out[i] = '\0';`),
    //     it does NOT execute on error (the `if (r.has_value())` guard
    //     short-circuits before the loop).  Section 3c pins this on
    //     the padded build by repeating the TooLarge test and
    //     asserting every byte is still 'Q'.
#if PETRA_PAD == 1
    {
        char buf3c_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3c_arr[i] = static_cast<char>('Q');
        char* buf3c = buf3c_arr;
        auto r3c = TRY_FORMAT_BOUNDED_N_PAD(
            buf3c, 16, "too long: {}\n", std::string(20, 'X'));
        CHECK(!r3c.has_value());
        // Every byte must still be 'Q' — atomicity preserved by the
        // `if (r.has_value())` short-circuit BEFORE the pad loop.
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3c_arr[i], 'Q');
        }
    }
#endif

    // ----- Section 4 — byte-count contract on success
    SECTION("Section 4 - byte-count contract on success");
    {
        char buf4a_arr[32] = {};
        char* buf4a = buf4a_arr;
        auto r4a = TRY_FORMAT_BOUNDED_N_PAD(
            buf4a, 32, "[{}] [{}] [{}]", 1, 2, 3);
        CHECK(r4a.has_value());
        CHECK_EQ(r4a.value(), std::size_t{11});
        CHECK_EQ((std::string_view{buf4a_arr, r4a.value()}),
                 (std::string_view{"[1] [2] [3]"}));
    }
    {
        char buf4b_arr[32] = {};
        char* buf4b = buf4b_arr;
        std::string fmt4b = "[{}] [{}] [{}]";
        auto r4b = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            buf4b, 32, fmt4b, 1, 2, 3);
        CHECK(r4b.has_value());
        CHECK_EQ(r4b.value(), std::size_t{11});
    }

    // ----- Section 5 — partial-buffer success (written < cap)
    SECTION("Section 5 - partial-buffer success (written < cap)");
    {
        // Verify the padded wrapper pads exactly [written, cap) to '\0'.
        char buf5_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf5_arr[i] = static_cast<char>('Q');
        char* buf5 = buf5_arr;
        // "hi" = 2 bytes into cap=16: pad region is [2, 16).
        auto r5 = TRY_FORMAT_BOUNDED_N_PAD(buf5, 16, "hi");
        CHECK(r5.has_value());
        CHECK_EQ(r5.value(), std::size_t{2});
        CHECK_EQ(buf5_arr[0], 'h');
        CHECK_EQ(buf5_arr[1], 'i');
#if PETRA_PAD == 1
        // All 14 bytes in [2, 16) must be '\0'.
        for (std::size_t i = 2; i < 16; ++i) {
            CHECK_EQ(buf5_arr[i], '\0');
        }
#else
        // Untouched: all 14 bytes in [2, 16) must still be 'Q'.
        for (std::size_t i = 2; i < 16; ++i) {
            CHECK_EQ(buf5_arr[i], 'Q');
        }
#endif
    }

    // ----- Section 6 — CONSTEVAL macro's consteval gate preserved
    SECTION("Section 6 - CONSTEVAL macro's consteval gate preserved");
    {
        // 6a. Literal fmt (passes consteval parse).
        char buf6a_arr[16] = {};
        char* buf6a = buf6a_arr;
        auto r6a = TRY_FORMAT_BOUNDED_N_PAD(buf6a, 16, "x={}", 42);
        CHECK(r6a.has_value());
        CHECK_EQ(r6a.value(), std::size_t{4});
        CHECK_EQ((std::string_view{buf6a_arr, 4}),
                 (std::string_view{"x=42"}));
    }
    // 6b. The RUNTIME macro accepts a runtime-built std::string fmt
    //     (it does NOT require a literal fmt).
    {
        char buf6b_arr[16] = {};
        char* buf6b = buf6b_arr;
        std::string fmt6b = "y={}";
        auto r6b = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(buf6b, 16, fmt6b, 7);
        CHECK(r6b.has_value());
        CHECK_EQ(r6b.value(), std::size_t{3});
    }
    // 6c. A runtime-built std::string fmt passed to the CONSTEVAL macro
    //     REJECTS AT COMPILE TIME (verified separately via
    //     /tmp/probe_consteval_rejects_runtime_fmt_pad.cpp).
    CHECK(true);

    // ----- Section 7 — multi-arg success path
    SECTION("Section 7 - multi-arg success path");
    {
        char buf7_arr[64] = {};
        char* buf7 = buf7_arr;
        auto r7 = TRY_FORMAT_BOUNDED_N_PAD(
            buf7, 64, "x={} y={} z={}", 42, "hi", 3.14);
        CHECK(r7.has_value());
        CHECK_EQ(r7.value(), std::size_t{16});
        CHECK_EQ((std::string_view{buf7_arr, 16}),
                 (std::string_view{"x=42 y=hi z=3.14"}));
    }
    {
        char buf7b_arr[64] = {};
        char* buf7b = buf7b_arr;
        std::string fmt7b = "x={} y={} z={}";
        auto r7b = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            buf7b, 64, fmt7b, 42, "hi", 3.14);
        CHECK(r7b.has_value());
        CHECK_EQ(r7b.value(), std::size_t{16});
    }

    // ----- Section 8 — Format error path
    SECTION("Section 8 - Format error path");
    {
        // A malformed fmt throws inside vformat_to; the impl catches
        // std::format_error and re-raises as
        // FormatError{Format, e.what()}.  The padded wrapper does NOT
        // touch the buffer on Format error (same atomicity as on
        // TooLarge).
        //
        // We test on the RUNTIME surface only (std::string_view fmt is
        // NOT consteval-parsed, so a malformed fmt reaches vformat_to
        // and throws std::format_error there).  The CONSTEVAL surface
        // uses std::format_string<Args...> which is consteval-parsed —
        // a malformed fmt would be REJECTED AT COMPILE TIME, so we
        // cannot test the runtime format-error path on the CONSTEVAL
        // surface.
        char buf8_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf8_arr[i] = static_cast<char>('Q');
        char* buf8 = buf8_arr;
        std::string fmt8 = "bad {";  // malformed: unterminated brace
        auto r8 = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            buf8, 16, fmt8, 42);
        CHECK(!r8.has_value());
        CHECK_EQ(r8.error().kind, petra::FormatErrorKind::Format);
        // Every byte must still be 'Q' — atomicity preserved.
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf8_arr[i], 'Q');
        }
    }

    // ----- Section 9 — no-args variant
    SECTION("Section 9 - no-args variant");
    {
        char buf9a_arr[16] = {};
        char* buf9a = buf9a_arr;
        auto r9a = TRY_FORMAT_BOUNDED_N_PAD(buf9a, 16, "ping");
        CHECK(r9a.has_value());
        CHECK_EQ(r9a.value(), std::size_t{4});
        CHECK_EQ((std::string_view{buf9a_arr, 4}),
                 (std::string_view{"ping"}));
    }
    {
        char buf9b_arr[16] = {};
        char* buf9b = buf9b_arr;
        std::string fmt9b = "pong";
        auto r9b = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(buf9b, 16, fmt9b);
        CHECK(r9b.has_value());
        CHECK_EQ(r9b.value(), std::size_t{4});
    }

    // ----- Section 10 — 4 threads × 25 calls concurrent dispatch
    SECTION("Section 10 - 4 threads x 25 calls concurrent dispatch");
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;

        // 10a. Concurrent calls to the CONSTEVAL macro.
        std::atomic<int> local_pass{0};
        std::atomic<int> local_fail{0};
        std::vector<std::thread> threads1;
        threads1.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads1.emplace_back([&local_pass, &local_fail, t]() {
                int lp = 0, lf = 0;
                char buf_arr[64] = {};
                for (int i = 0; i < kCallsPerThread; ++i) {
                    auto r = TRY_FORMAT_BOUNDED_N_PAD(
                        buf_arr, 64, "[{:02}][{:02}]", t, i);
                    if (r.has_value() && r.value() == 8) {
                        ++lp;
                    } else {
                        ++lf;
                    }
                }
                local_pass.fetch_add(lp);
                local_fail.fetch_add(lf);
            });
        }
        for (auto& th : threads1) th.join();
        CHECK_EQ(local_pass.load(), kThreads * kCallsPerThread);
        CHECK_EQ(local_fail.load(), 0);

        // 10b. Concurrent calls to the RUNTIME macro.
        std::atomic<int> local_pass2{0};
        std::atomic<int> local_fail2{0};
        std::vector<std::thread> threads2;
        threads2.reserve(kThreads);
        std::string fmt = "[{:02}][{:02}]";
        for (int t = 0; t < kThreads; ++t) {
            threads2.emplace_back([&local_pass2, &local_fail2, &fmt, t]() {
                int lp = 0, lf = 0;
                char buf_arr[64] = {};
                for (int i = 0; i < kCallsPerThread; ++i) {
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
                        buf_arr, 64, fmt, t, i);
                    if (r.has_value() && r.value() == 8) {
                        ++lp;
                    } else {
                        ++lf;
                    }
                }
                local_pass2.fetch_add(lp);
                local_fail2.fetch_add(lf);
            });
        }
        for (auto& th : threads2) th.join();
        CHECK_EQ(local_pass2.load(), kThreads * kCallsPerThread);
        CHECK_EQ(local_fail2.load(), 0);
    }

    // ----- Section 11 — perpendicular-axis claim
    SECTION("Section 11 - perpendicular-axis claim");
#if PETRA_PAD == 0
    {
        // 11a. Today's TRY_FORMAT_BOUNDED_N_PAD with PETRA_PAD=0
        //      dispatches to the SAME wrapper as
        //      TRY_FORMAT_BOUNDED_N_PAD_ORIG (CONSTEVAL + bounded-buffer
        //      + char*+cap; Sep 9's wrapper).  Today's
        //      TRY_FORMAT_BOUNDED_RUNTIME_N_PAD with PETRA_PAD=0
        //      dispatches to the SAME wrapper as
        //      TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_ORIG (RUNTIME +
        //      bounded-buffer + char*+cap; Sep 8's wrapper).
        using R11a = decltype(TRY_FORMAT_BOUNDED_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using R11b = decltype(TRY_FORMAT_BOUNDED_N_PAD_ORIG(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R11a, R11b>,
                      "TRY_FORMAT_BOUNDED_N_PAD with PETRA_PAD=0 must "
                      "dispatch to the same wrapper as "
                      "TRY_FORMAT_BOUNDED_N_PAD_ORIG");

        using R11c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using R11d = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_ORIG(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R11c, R11d>,
                      "TRY_FORMAT_BOUNDED_RUNTIME_N_PAD with PETRA_PAD=0 "
                      "must dispatch to the same wrapper as "
                      "TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_ORIG");
        CHECK(true);
    }
#else
    {
        // 11b. On the padded path, the meta-macros dispatch to TODAY's
        //      NEW padded wrappers (which return the SAME return type
        //      as the untouched wrappers — both return
        //      expected<std::size_t, FormatError>; the difference is
        //      ONLY the side-effect on the buffer).
        using R11a = decltype(TRY_FORMAT_BOUNDED_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using H11a = decltype(petra::try_format_bounded_runtime_n_padded(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R11a, H11a>,
                      "With PETRA_PAD=1, TRY_FORMAT_BOUNDED_N_PAD must "
                      "dispatch to petra::try_format_bounded_runtime_n_padded");

        using R11b = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_PAD(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using H11b = decltype(petra::try_format_runtime_n_padded(
            std::declval<char*>(),
            std::declval<std::size_t>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R11b, H11b>,
                      "With PETRA_PAD=1, TRY_FORMAT_BOUNDED_RUNTIME_N_PAD "
                      "must dispatch to petra::try_format_runtime_n_padded");
        CHECK(true);
    }
#endif

    // ----- Section 12 — final summary
    SECTION("Section 12 - final summary");
    std::println("=========================================================");
    std::println("PASS: {}  FAIL: {}", g_pass, g_fail);
    std::println("=========================================================");

    if (g_fail != 0) {
        std::fprintf(stderr, "OVERALL: FAIL (%d failures)\n", g_fail);
        std::exit(1);
    }
    std::println("OVERALL: PASS");
    return 0;
}
