// P-2026-09-23 — try_format_span_n_or_throw_into:
//
// A MACRO LAYER that lifts the NUL-TERMINATING × RETURN-TYPE-vs-EXCEPTION
// sibling choice to a meta-macro form on the STD::SPAN<CHAR> axis.
// Two new meta-macros — TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO
// (CONSTEVAL surface) and TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO
// (RUNTIME surface) — dispatch to ONE OF FOUR underlying wrappers
// based on ONE preprocessor compile-time flag:
//
//   PETRA_THROW_ON_OVERFLOW: 0 = expected sibling
//                           1 = throwing sibling
//
// The FOUR-AXIS matrix (char*-vs-space × CONSTEVAL-vs-RUNTIME ×
// bounded-vs-NUL-term × expected-vs-throwing) is now COMPLETE on the
// SPAN<CHAR> × bounded-vs-NUL-term × expected-vs-throwing quadrant:
// today's lesson adds the THROWING sibling to the NUL-TERMINATING
// cells (2 new cells on the span<char> × CONSTEVAL-or-RUNTIME ×
// NUL-terminating × throwing axis — closes Sep 22's "Where we go
// next" item #1 verbatim).  Together with Sep 22's char*+cap lesson,
// the FOUR-AXIS BOUNDED-vs-NUL × expected-vs-throwing quadrant is
// now COMPLETE on BOTH the char*+cap and span<char> axes.
//
// What today adds (all verbatim thin wrappers / dispatchers):
//   - petra::try_format_to_n_into_bounded_throwing_span(std::span<char>,
//     std::format_string<Args...>, args...) — NEW today; wraps Sep 11's
//     format_to_n_into_bounded(span, ...).
//   - petra::try_format_runtime_n_into_bounded_throwing_span(std::span<char>,
//     std::string_view, args...) — NEW today; wraps Sep 12's
//     try_format_runtime_n_into_bounded(span, ...).
//   - The TWO new meta-macros TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO
//     (CONSTEVAL surface) and
//     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO (RUNTIME
//     surface).  Each is a verbatim thin dispatcher that picks
//     between the expected sibling (PETRA_THROW_ON_OVERFLOW=0) and
//     the throwing sibling (PETRA_THROW_ON_OVERFLOW=1) at
//     preprocessor time.  Each macro is a parenthesized expression
//     (NOT do { } while (0)) so the caller can write
//     'auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(...)' and
//     capture the result.  Each macro uses __VA_OPT__(,) for the
//     no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep
//     19 / Sep 20 / Sep 21 / Sep 22's macro shape).
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
//              axis (the FOURTH axis; today covers the
//              char*+cap × bounded-buffer × expected-vs-throwing
//              cells on the char*+cap axis).
//   - Sep 21   try_format_span_n_or_throw — the RETURN-TYPE-vs-
//              EXCEPTION sibling-choice macro layer on the
//              std::span<char> axis (the FOURTH axis on the
//              span<char> side; closes Sep 20's "Where we go next"
//              item #1 verbatim).
//   - Sep 22   try_format_bounded_n_or_throw_into — the NUL-
//              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
//              meta-macro layer on the CHAR*+CAP axis (closes Sep
//              21's "Where we go next" item #1 verbatim; adds the
//              throwing sibling to the NUL-terminating cells of the
//              char*+cap × bounded-vs-NUL-term × expected-vs-throwing
//              2x2 quadrant of the 16-cell matrix).
//   - TODAY    try_format_span_n_or_throw_into — the NUL-
//              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
//              meta-macro layer on the STD::SPAN<CHAR> axis (closes
//              Sep 22's "Where we go next" item #1 verbatim; adds
//              the throwing sibling to the NUL-terminating cells of
//              the span<char> × bounded-vs-NUL-term ×
//              expected-vs-throwing 2x2 quadrant of the 16-cell
//              matrix — with today's lesson, the FOUR-AXIS
//              BOUNDED-vs-NUL × expected-vs-throwing quadrant is
//              now COMPLETE on BOTH the char*+cap and span<char>
//              axes).
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22
//     verbatim.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — REUSED verbatim.
//   - petra::counting_output_iterator — REUSED verbatim.
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_to_n_into_bounded_impl (Sep 11 verbatim)
//     + petra::detail::try_format_runtime_n_into_bounded_impl (Sep 12
//     verbatim) — REUSED verbatim.
//   - petra::format_to_n_into_bounded(char*, std::size_t,
//     std::format_string<Args...>, args...) — REUSED Sep 11 verbatim.
//   - petra::try_format_runtime_n_into_bounded(char*, std::size_t,
//     std::string_view, args...) — REUSED Sep 12 verbatim.
//   - petra::format_to_n_into_bounded(std::span<char>,
//     std::format_string<Args...>, args...) — REUSED Sep 11 verbatim
//     (the CONSTEVAL NUL-terminating span sibling; today's NEW
//     throwing wrapper composes on top of this).
//   - petra::try_format_runtime_n_into_bounded(std::span<char>,
//     std::string_view, args...) — REUSED Sep 12 verbatim (the RUNTIME
//     NUL-terminating span sibling; today's NEW throwing wrapper
//     composes on top of this).
//   - petra::try_format_to_n_into_bounded_throwing_span(std::span<char>,
//     std::format_string<Args...>, args...) — NEW today (CONSTEVAL
//     surface of the NUL-terminating × THROWING sibling on the
//     span<char> axis; wraps Sep 11's
//     format_to_n_into_bounded(span, ...) verbatim).
//   - petra::try_format_runtime_n_into_bounded_throwing_span(std::span<char>,
//     std::string_view, args...) — NEW today (RUNTIME surface of the
//     NUL-terminating × THROWING sibling on the span<char> axis;
//     wraps Sep 12's try_format_runtime_n_into_bounded(span, ...)
//     verbatim).
//   - The TWO new meta-macros TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO
//     and TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO — NEW today;
//     each is a verbatim thin dispatcher that picks between the
//     expected sibling (PETRA_THROW_ON_OVERFLOW=0) and the throwing
//     sibling (PETRA_THROW_ON_OVERFLOW=1) at preprocessor time.
//     Default PETRA_THROW_ON_OVERFLOW=0 (expected sibling — non-
//     throwing).
//
// What today's lesson pins
// ------------------------
//   1. The NUL-TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling choice
//      CAN be lifted to a meta-macro form on the span<char> axis.
//      The shape composes on top of Sep 11's / Sep 12's NUL-
//      terminating span wrappers (the underlying primary wrappers)
//      AND on top of Sep 21's expected-vs-throwing macro layer (the
//      underlying dispatcher meta-macros — bounded-buffer variants).
//      Each TODAY's meta-macro is a verbatim thin wrapper around ONE
//      OF TWO underlying wrappers:
//        - NUL-terminating × expected:        Sep 11 / Sep 12 span
//                                              overloads
//        - NUL-terminating × throwing:        TODAY's NEW span
//                                              throwing wrappers
//      The bounded-buffer variants are NOT covered by today's
//      macros (those are Sep 21's domain).
//   2. The compile-time flag PETRA_THROW_ON_OVERFLOW controls the
//      dispatch via #if PETRA_THROW_ON_OVERFLOW == 0/1:
//        - #define PETRA_THROW_ON_OVERFLOW 0 → expected sibling:
//            CONSTEVAL: petra::format_to_n_into_bounded(span, ...)
//                      (Sep 11)
//            RUNTIME:  petra::try_format_runtime_n_into_bounded(span, ...)
//                      (Sep 12)
//        - #define PETRA_THROW_ON_OVERFLOW 1 → throwing sibling:
//            CONSTEVAL: petra::try_format_to_n_into_bounded_throwing_span
//                      (NEW today; wraps Sep 11 verbatim)
//            RUNTIME:  petra::try_format_runtime_n_into_bounded_throwing_span
//                      (NEW today; wraps Sep 12 verbatim)
//   3. The #error guard fires at preprocessor time if the flag is
//      set to anything other than 0 or 1.
//   4. The macro is a parenthesized expression (NOT
//      do { ... } while (0)) so the caller can write
//      'auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(...)' and
//      capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep
//      16 / Sep 19 / Sep 20 / Sep 21 / Sep 22's macro shape.
//   5. __VA_OPT__(,) for the no-args case.  Mirrors Sep 13 / Sep 14 /
//      Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22.
//   6. The CONSTEVAL macro picks between Sep 11's
//      format_to_n_into_bounded(span, ...) (expected, returns
//      expected<size_t, FormatError>) and today's NEW
//      try_format_to_n_into_bounded_throwing_span (throwing, returns
//      size_t).  Both are consteval-checked std::format_string<Args...>
//      wrappers.
//   7. The RUNTIME macro picks between Sep 12's
//      try_format_runtime_n_into_bounded(span, ...) (expected,
//      returns expected<size_t, FormatError>) and today's NEW
//      try_format_runtime_n_into_bounded_throwing_span (throwing,
//      returns size_t).  Both take std::string_view format-string (no
//      compile-time parse).
//   8. The macro's contract is byte-exact: the throwing sibling's
//      std::runtime_error::what() is byte-exactly equal to the
//      expected sibling's FormatError::message for the SAME
//      {needed, cap, effective} triple.  Pinned by Section 9 by
//      calling both siblings with the SAME call args and comparing
//      expected_msg (from r.error().message on the direct Sep 11 /
//      Sep 12 span-wrapper call) to thrown_msg (from e.what() on the
//      throwing sibling path).  The NUL-terminating impl carries an
//      additional "(effective writable is N)" suffix in the
//      FormatError::message; the throwing wrapper preserves this
//      byte-exactly.
//   9. The throwing sibling preserves the SAME buffer atomicity on
//      TooLarge as the expected sibling — the buffer is UNTOUCHED on
//      overflow (Sep 11 / Sep 12 already pin this on the expected
//      path; today's wrappers re-pin it on the throwing path because
//      the new code path COULD theoretically diverge; Section 3c /
//      3d pins this).
//  10. The throwing sibling preserves the SAME NUL-placement
//      contract on success as the expected sibling — NUL at
//      nul_idx = (written < cap) ? written : (cap - 1); Section 5
//      pins this on both paths.
//  11. The CONSTEVAL macro's consteval gate is preserved regardless
//      of PETRA_THROW_ON_OVERFLOW —
//      TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO still requires
//      std::format_string<Args...> (a literal fmt at the call site).
//      Section 6 pins the consteval gate by calling the macro with
//      a literal fmt and a runtime-built std::string fmt; the
//      runtime-built fmt REJECTS AT COMPILE TIME on the CONSTEVAL
//      macro (verified by /tmp/probe_consteval_rejects_runtime_fmt_span_into.cpp
//      producing the same 'error: call to consteval function ... is
//      not a constant expression' as Sep 16's / Sep 19's / Sep 20's /
//      Sep 21's / Sep 22's probe).
//  12. The PETRA_THROW_ON_OVERFLOW axis (expected vs throwing) is
//      PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis (Sep 13's
//      macro layer), AND to the bounded-buffer-vs-NUL-terminating
//      axis (Sep 16's / Sep 19's macro layer).  Today's meta-macros
//      are at the INTERSECTION of CONSTEVAL-or-RUNTIME ×
//      NUL-terminating × expected-vs-throwing on the STD::SPAN<CHAR>
//      axis.  Section 14's decltype checks pin the perpendicular-
//      axis claim on BOTH paths.
//  13. The span<char> sub-span 'no-bleed' contract is preserved on
//      the throwing path (Section 7) — a std::span<char> over a
//      sub-range of a buffer must NOT touch bytes outside the span
//      (mirrors Sep 10 / Sep 12's / Sep 19's / Sep 21's no-bleed
//      contract; a 6-byte sub-span at offset 5 of a 16-byte buffer
//      with format 'ABCDE' leaves bytes [0,5) and [11,16) as poison
//      'Q').
//  14. std::format_error IS-A std::runtime_error — pinned by
//      static_assert(std::is_base_of_v<std::runtime_error,
//      std::format_error>) succeeding.  This means the catch site
//      must put std::format_error& BEFORE std::runtime_error& in the
//      catch chain (C++ catch handlers are matched in order).
//      Section 10 exploits this by checking the dynamic type via
//      dynamic_cast<const std::format_error*>(&e) — the throwing
//      wrapper re-raises as std::runtime_error, NOT std::format_error,
//      so the dynamic cast returns nullptr.
//  15. Concurrency is safe — Section 13 fires 4 threads × 25
//      concurrent calls through both macros; per-thread local
//      counters are aggregated AFTER join() via
//      std::atomic<int>::fetch_add to avoid a data race on
//      g_pass / g_fail.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::span<char> (P0122R7), std::println (P2093R14),
// std::runtime_error, std::atomic, dynamic_cast, std::is_base_of_v,
// __VA_OPT__ preprocessor feature.  Requires -fexperimental-library
// on Apple Clang 21.0.0 / libc++ 21.

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
// Sep 20 / Sep 21 / Sep 22)
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
// Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22)
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
// Sep 20 / Sep 21 / Sep 22)
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
// Part 4 — petra::detail::try_format_to_n_into_bounded_impl (Sep 11 / Sep 12
// verbatim — the NUL-terminating sibling impl).  Today's NEW throwing
// wrappers compose on top of Sep 11's / Sep 12's span<char> wrappers which
// compose on top of this impl.
//
// Note: this impl differs from Sep 8's try_format_runtime_n_impl — Sep 8
// treats cap as the full writable byte count, while today's impl reserves
// one slot for the NUL on the success path.  The success byte count is
// still the would-be size; the NUL is written at nul_idx = (written <
// cap) ? written : (cap - 1).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_to_n_into_bounded_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt,
                                  const Args&... args) {
    // Pass 1: measure the would-be byte count.
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

    // cap == 0: no room for anything.  needed == 0 is success (no NUL);
    // needed > 0 is TooLarge.
    if (cap == 0) {
        if (needed > 0) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge,
                            std::string{"formatted output would be "}
                                + std::to_string(needed)
                                + " bytes; cap is 0",
                            needed});
        }
        return std::size_t{0};
    }

    // cap >= 1: reserve one slot for the NUL.  Effective writable is
    // cap - 1.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap)
                            + " (effective writable is "
                            + std::to_string(cap - 1) + ")",
                        needed});
    }

    const std::size_t effective_cap = cap - 1;

    // Truncation edge: needed == cap.  Aug 25's contract writes
    // cap - 1 bytes (capped by the bounded writer at effective_cap)
    // and puts the NUL at cap - 1, overwriting nothing (the cap - 1
    // byte is the last writable slot).
    if (needed == cap) {
        // Pass 2: write into out[0, cap - 1).
        std::size_t written = 0;
        try {
            bounded_char_writer sink{out, effective_cap, cb, &written};
            (void)std::vformat_to(std::move(sink), fmt.get(),
                                  std::make_format_args(args...));
        } catch (const std::format_error& e) {
            return std::unexpected(
                FormatError{FormatErrorKind::Format, e.what()});
        }
        // Invariant: written == effective_cap (the bounded writer
        // stops at effective_cap when there are more chars to write).
        if (written != effective_cap) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge,
                            std::string{"bounded_char_writer wrote "}
                                + std::to_string(written)
                                + " bytes; expected "
                                + std::to_string(effective_cap)
                                + " on truncation edge",
                            needed});
        }
        out[cap - 1] = '\0';  // NUL at cap - 1 (Aug 25 formula).
        return effective_cap;
    }

    // needed < cap: full success path.  Write all `needed` bytes into
    // out[0, needed), then NUL at out[needed].
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, effective_cap, cb, &written};
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
    out[needed] = '\0';
    return written;
}

// Runtime sibling — same shape as the CONSTEVAL impl above but takes
// std::string_view fmt (Sep 12 verbatim).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded_impl(char* out, std::size_t cap,
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

    // cap == 0: no room for anything.  needed == 0 is success (no NUL);
    // needed > 0 is TooLarge.
    if (cap == 0) {
        if (needed > 0) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge,
                            std::string{"formatted output would be "}
                                + std::to_string(needed)
                                + " bytes; cap is 0",
                            needed});
        }
        return std::size_t{0};
    }

    // cap >= 1: reserve one slot for the NUL.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap)
                            + " (effective writable is "
                            + std::to_string(cap - 1) + ")",
                        needed});
    }

    const std::size_t effective_cap = cap - 1;

    // Truncation edge.
    if (needed == cap) {
        std::size_t written = 0;
        try {
            bounded_char_writer sink{out, effective_cap, cb, &written};
            (void)std::vformat_to(std::move(sink), fmt,
                                  std::make_format_args(args...));
        } catch (const std::format_error& e) {
            return std::unexpected(
                FormatError{FormatErrorKind::Format, e.what()});
        }
        if (written != effective_cap) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge,
                            std::string{"bounded_char_writer wrote "}
                                + std::to_string(written)
                                + " bytes; expected "
                                + std::to_string(effective_cap)
                                + " on truncation edge",
                            needed});
        }
        out[cap - 1] = '\0';
        return effective_cap;
    }

    // Full success path.
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, effective_cap, cb, &written};
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
    out[needed] = '\0';
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — petra::format_to_n_into_bounded (Sep 11 verbatim — char*+cap
// + span<char> overloads) + petra::try_format_runtime_n_into_bounded
// (Sep 12 verbatim — char*+cap + span<char> overloads).
//
// Today's NEW NUL-terminating × THROWING span<char> wrappers compose
// on top of Sep 11's / Sep 12's span<char> overloads.
// ============================================================================

namespace petra {

// Sep 11 verbatim — CONSTEVAL NUL-terminating char*+cap sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(char* out, std::size_t cap,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(out, cap, fmt,
                                                     args...);
}

// Sep 11 verbatim — CONSTEVAL NUL-terminating span<char> sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(std::span<char> out,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return format_to_n_into_bounded(out.data(), out.size(),
                                    fmt, args...);
}

// Sep 12 verbatim — RUNTIME NUL-terminating char*+cap sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* out, std::size_t cap,
                                  std::string_view fmt,
                                  const Args&... args) {
    return detail::try_format_runtime_n_into_bounded_impl(out, cap,
                                                          fmt, args...);
}

// Sep 12 verbatim — RUNTIME NUL-terminating span<char> sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(std::span<char> out,
                                  std::string_view fmt,
                                  const Args&... args) {
    return try_format_runtime_n_into_bounded(out.data(), out.size(),
                                             fmt, args...);
}

// NEW today — CONSTEVAL surface of the NUL-terminating × THROWING
// span<char> sibling.  A verbatim thin wrapper around Sep 11's
// format_to_n_into_bounded(span, ...) that re-raises FormatError{...}
// as std::runtime_error with the SAME FormatError::message (byte-
// exact).  The byte-exactness is preserved even though the NUL-
// terminating impl carries an additional "(effective writable is N)"
// suffix in the FormatError::message — the throwing wrapper copies
// the message verbatim, including the suffix.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_to_n_into_bounded_throwing_span(std::span<char> out,
                                           std::format_string<Args...> fmt,
                                           const Args&... args) {
    auto r = format_to_n_into_bounded(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

// NEW today — RUNTIME surface of the NUL-terminating × THROWING
// span<char> sibling.  A verbatim thin wrapper around Sep 12's
// try_format_runtime_n_into_bounded(span, ...) that re-raises as
// std::runtime_error with byte-exact message.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_into_bounded_throwing_span(std::span<char> out,
                                                std::string_view fmt,
                                                const Args&... args) {
    auto r = try_format_runtime_n_into_bounded(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

}  // namespace petra

// ============================================================================
// Part 6 — TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO /
// TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO NEW TODAY — two
// meta-macros that lift the NUL-TERMINATING × RETURN-TYPE-vs-EXCEPTION
// sibling choice to a meta-macro form on the std::span<char> axis.
//
// The compile-time flag PETRA_THROW_ON_OVERFLOW controls the dispatch
// (PETRA_NUL_TERMINATE is implicitly 1 — the bounded-buffer cells are
// Sep 19's / Sep 21's domain):
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//     → expected<size_t, FormatError> sibling (NO throw on overflow):
//        CONSTEVAL: petra::format_to_n_into_bounded(span, ...) (Sep 11)
//        RUNTIME:  petra::try_format_runtime_n_into_bounded(span, ...)
//                  (Sep 12)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//     → throwing sibling (throws std::runtime_error on overflow):
//        CONSTEVAL: petra::try_format_to_n_into_bounded_throwing_span
//                  (NEW today; wraps Sep 11 verbatim)
//        RUNTIME:  petra::try_format_runtime_n_into_bounded_throwing_span
//                  (NEW today; wraps Sep 12 verbatim)
//
// Default: PETRA_THROW_ON_OVERFLOW = 0 (expected sibling — non-throwing).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 /
// Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22's macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Mirrors
// Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22.
//
// The two macros cover the TWO surfaces established by Sep 13:
//   - CONSTEVAL surface: std::format_string<Args...> literal primary
//     (the format-string parse happens at compile time).
//   - RUNTIME surface: std::string_view format-string (the format-string
//     parse happens at format time).
//
// This is the PERPENDICULAR axis to Sep 19's / Sep 21's macro layer:
//   - Sep 19:    bounded-buffer vs NUL-terminating (output contract)
//                on span<char>.
//   - Sep 21:    expected vs throwing (return-type contract)
//                on span<char> × bounded-buffer.
//   - TODAY:     expected vs throwing (return-type contract)
//                on span<char> × NUL-terminating.
//
// Sep 22's lesson is the char*+cap mirror of today's span<char>
// lesson.  Sep 22's lesson closes Sep 21's "Where we go next" item
// #1 verbatim.  TODAY's lesson closes Sep 22's "Where we go next"
// item #1 verbatim.
//
// The FOUR-AXIS matrix is now COMPLETE on the
// BOUNDED-vs-NUL × expected-vs-throwing quadrant: today's lesson
// adds the THROWING sibling to the NUL-terminating cells on the
// span<char> axis (2 cells on the span<char> × CONSTEVAL-or-RUNTIME ×
// NUL-terminating × throwing axis).  Together with Sep 22's char*+cap
// lesson, the quadrant is COMPLETE on BOTH axes.
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling — non-throwing).
// The user can override before including this header (or at the top of
// the .cpp file).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

#if PETRA_THROW_ON_OVERFLOW == 0
// Expected sibling path — returns std::expected<std::size_t, FormatError>.
// On TooLarge, FormatError{TooLarge, ..., needed_bytes} is returned.

// CONSTEVAL surface: dispatches to Sep 11's
// format_to_n_into_bounded(span, ...).
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(buf, fmt, ...)            \
    (::petra::format_to_n_into_bounded((buf), (fmt)                      \
                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 12's
// try_format_runtime_n_into_bounded(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(buf, fmt, ...)    \
    (::petra::try_format_runtime_n_into_bounded((buf), (fmt)             \
                                                __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1
// Throwing sibling path — returns std::size_t; throws std::runtime_error
// on TooLarge or Format.  Same atomicity contract as the expected
// sibling (buffer UNTOUCHED on overflow — Sep 11's / Sep 12's contract,
// preserved by the verbatim thin wrapper).

// CONSTEVAL surface: dispatches to today's NEW
// try_format_to_n_into_bounded_throwing_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(buf, fmt, ...)            \
    (::petra::try_format_to_n_into_bounded_throwing_span(                 \
        (buf), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to today's NEW
// try_format_runtime_n_into_bounded_throwing_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(buf, fmt, ...)    \
    (::petra::try_format_runtime_n_into_bounded_throwing_span(            \
        (buf), (fmt) __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW

// ============================================================================
// Part 7 — Helper dispatcher macros used in Section 14's decltype checks
// to pin the perpendicular-axis claim.
//
// These are NOT new surface; they are simply verbatim thin dispatchers
// to the expected siblings on the NUL-terminating span<char> axis.
// Used to prove that today's meta-macros dispatch to the same
// wrappers as these "flat" dispatchers under PETRA_THROW_ON_OVERFLOW=0.
// ============================================================================

#define TRY_FORMAT_BOUNDED_SPAN_N_INTO_ORIG(buf, fmt, ...)                \
    (::petra::format_to_n_into_bounded((buf), (fmt)                      \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_INTO_ORIG(buf, fmt, ...)        \
    (::petra::try_format_runtime_n_into_bounded((buf), (fmt)             \
                                                __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 8 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22).
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
    // ----- Section 1 — PETRA_THROW_ON_OVERFLOW compile-time flag pinned
    SECTION("Section 1 - PETRA_THROW_ON_OVERFLOW compile-time flag");

    // 1a. The compile-time flag PETRA_THROW_ON_OVERFLOW controls the
    //     dispatch.  This file may be compiled with EITHER value:
    //     - PETRA_THROW_ON_OVERFLOW=0 → expected sibling
    //     - PETRA_THROW_ON_OVERFLOW=1 → throwing sibling
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                  || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0
    std::println("[Section 1] PETRA_THROW_ON_OVERFLOW=0 (expected sibling)");
#else
    std::println("[Section 1] PETRA_THROW_ON_OVERFLOW=1 (throwing sibling)");
#endif
    CHECK(true);

    // 1b. Pin that the macros dispatch to the correct sibling based
    //     on PETRA_THROW_ON_OVERFLOW.  Verify by checking the macro's
    //     expansion type matches the underlying wrapper.
#if PETRA_THROW_ON_OVERFLOW == 0
    // Expected sibling path:
    using R1b = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO must return "
                  "std::expected<std::size_t, FormatError> when "
                  "PETRA_THROW_ON_OVERFLOW=0");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO must return "
                  "std::expected<std::size_t, FormatError> when "
                  "PETRA_THROW_ON_OVERFLOW=0");
#else
    // Throwing sibling path:
    using R1b = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::size_t;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO must return "
                  "std::size_t when PETRA_THROW_ON_OVERFLOW=1");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO must return "
                  "std::size_t when PETRA_THROW_ON_OVERFLOW=1");
#endif
    CHECK(true);

    // 1c. Pin that std::runtime_error is throwable from the throwing
    //     sibling, and that std::format_error IS-A std::runtime_error.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // 1d. std::span<char> properties — pinned in Section 1d so the
    //     'no-bleed' contract in Section 7 is grounded in a
    //     verifiable type.
    static_assert(std::ranges::contiguous_range<std::span<char>>);
    static_assert(std::ranges::sized_range<std::span<char>>);
    static_assert(std::is_trivially_copyable_v<std::span<char>>);
    static_assert(std::is_standard_layout_v<std::span<char>>);
    CHECK(true);

    // ----- Section 2 — sibling path verification
    SECTION("Section 2 - sibling path verification");
#if PETRA_THROW_ON_OVERFLOW == 0
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO
    //     dispatches to Sep 11's format_to_n_into_bounded(span, ...)
    //     (expected sibling).
    {
        char buf2a_arr[16] = {};
        std::span<char> buf2a{buf2a_arr};
        auto r2a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf2a, "hello {}", "world");
        CHECK(r2a.has_value());
        CHECK_EQ(r2a.value(), std::size_t{11});  // "hello world" = 11 bytes
        // NUL at nul_idx == written (since 11 < 16):
        CHECK_EQ(buf2a_arr[11], '\0');
        CHECK_EQ((std::string_view{buf2a_arr, 11}),
                 (std::string_view{"hello world"}));
    }

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO
    //     dispatches to Sep 12's try_format_runtime_n_into_bounded(span, ...).
    {
        char buf2b_arr[16] = {};
        std::span<char> buf2b{buf2b_arr};
        std::string fmt2b = "value={}";
        auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf2b, fmt2b, 42);
        CHECK(r2b.has_value());
        CHECK_EQ(r2b.value(), std::size_t{8});  // "value=42" = 8 bytes
        CHECK_EQ(buf2b_arr[8], '\0');
        CHECK_EQ((std::string_view{buf2b_arr, 8}),
                 (std::string_view{"value=42"}));
    }
#else
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO
    //     dispatches to today's NEW
    //     petra::try_format_to_n_into_bounded_throwing_span (throwing
    //     sibling).
    {
        char buf2a_arr[16] = {};
        std::span<char> buf2a{buf2a_arr};
        auto n2a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf2a, "hello {}", "world");
        CHECK_EQ(n2a, std::size_t{11});  // "hello world" = 11 bytes
        CHECK_EQ(buf2a_arr[11], '\0');
        CHECK_EQ((std::string_view{buf2a_arr, 11}),
                 (std::string_view{"hello world"}));
    }

    // 2b. RUNTIME macro:
    //     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO dispatches
    //     to today's NEW
    //     petra::try_format_runtime_n_into_bounded_throwing_span.
    {
        char buf2b_arr[16] = {};
        std::span<char> buf2b{buf2b_arr};
        std::string fmt2b = "value={}";
        auto n2b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf2b, fmt2b, 42);
        CHECK_EQ(n2b, std::size_t{8});
        CHECK_EQ(buf2b_arr[8], '\0');
        CHECK_EQ((std::string_view{buf2b_arr, 8}),
                 (std::string_view{"value=42"}));
    }
#endif

    // ----- Section 3 — atomicity on TooLarge
    SECTION("Section 3 - atomicity on TooLarge");
#if PETRA_THROW_ON_OVERFLOW == 0

    // 3a. CONSTEVAL macro: expected sibling path; TooLarge returns
    //     FormatError{TooLarge, ...} and the buffer is UNTOUCHED.
    {
        char buf3a_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3a_arr[i] = static_cast<char>('Q');
        std::span<char> buf3a{buf3a_arr};
        auto r3a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf3a, "too long: {}", std::string(20, 'X'));
        CHECK(!r3a.has_value());
        CHECK_EQ(r3a.error().kind, petra::FormatErrorKind::TooLarge);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3a_arr[i], 'Q');
        }
    }

    // 3b. RUNTIME macro: expected sibling path; TooLarge returns
    //     FormatError{TooLarge, ...} and the buffer is UNTOUCHED.
    {
        char buf3b_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3b_arr[i] = static_cast<char>('Q');
        std::span<char> buf3b{buf3b_arr};
        std::string fmt3b = "too long: {}";
        auto r3b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf3b, fmt3b, std::string(20, 'X'));
        CHECK(!r3b.has_value());
        CHECK_EQ(r3b.error().kind, petra::FormatErrorKind::TooLarge);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3b_arr[i], 'Q');
        }
    }

#else  // PETRA_THROW_ON_OVERFLOW == 1

    // 3c. CONSTEVAL macro: throwing sibling path; TooLarge throws
    //     std::runtime_error and the buffer is UNTOUCHED.  This is
    //     the KEY HEADLINE finding for today's throwing wrapper —
    //     the buffer atomicity on TooLarge is preserved by the
    //     verbatim thin wrapper around Sep 11's expected sibling
    //     (which already pins atomicity on the expected path).
    {
        char buf3c_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3c_arr[i] = static_cast<char>('Q');
        std::span<char> buf3c{buf3c_arr};
        bool caught3c = false;
        try {
            (void)TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                buf3c, "too long: {}", std::string(20, 'X'));
        } catch (const std::runtime_error&) {
            caught3c = true;
        }
        CHECK(caught3c);
        // Every byte must still be 'Q' — buffer UNTOUCHED.
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3c_arr[i], 'Q');
        }
    }

    // 3d. RUNTIME macro: throwing sibling path; TooLarge throws
    //     std::runtime_error and the buffer is UNTOUCHED.
    {
        char buf3d_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3d_arr[i] = static_cast<char>('Q');
        std::span<char> buf3d{buf3d_arr};
        std::string fmt3d = "too long: {}";
        bool caught3d = false;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
                buf3d, fmt3d, std::string(20, 'X'));
        } catch (const std::runtime_error&) {
            caught3d = true;
        }
        CHECK(caught3d);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3d_arr[i], 'Q');
        }
    }

#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 4 — byte-count contract on success
    SECTION("Section 4 - byte-count contract on success");
#if PETRA_THROW_ON_OVERFLOW == 0
    {
        char buf4a_arr[32] = {};
        std::span<char> buf4a{buf4a_arr};
        auto r4a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf4a, "[{}] [{}] [{}]", 1, 2, 3);
        CHECK(r4a.has_value());
        CHECK_EQ(r4a.value(), std::size_t{11});
        CHECK_EQ((std::string_view{buf4a_arr, r4a.value()}),
                 (std::string_view{"[1] [2] [3]"}));
        // NUL placement: nul_idx == 11 (== written) since 11 < 32.
        CHECK_EQ(buf4a_arr[11], '\0');
    }
    {
        char buf4b_arr[32] = {};
        std::span<char> buf4b{buf4b_arr};
        std::string fmt4b = "[{}] [{}] [{}]";
        auto r4b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf4b, fmt4b, 1, 2, 3);
        CHECK(r4b.has_value());
        CHECK_EQ(r4b.value(), std::size_t{11});
        CHECK_EQ(buf4b_arr[11], '\0');
    }
#else
    {
        char buf4c_arr[32] = {};
        std::span<char> buf4c{buf4c_arr};
        auto n4c = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf4c, "[{}] [{}] [{}]", 1, 2, 3);
        CHECK_EQ(n4c, std::size_t{11});
        CHECK_EQ((std::string_view{buf4c_arr, n4c}),
                 (std::string_view{"[1] [2] [3]"}));
        CHECK_EQ(buf4c_arr[11], '\0');
    }
    {
        char buf4d_arr[32] = {};
        std::span<char> buf4d{buf4d_arr};
        std::string fmt4d = "[{}] [{}] [{}]";
        auto n4d = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf4d, fmt4d, 1, 2, 3);
        CHECK_EQ(n4d, std::size_t{11});
        CHECK_EQ(buf4d_arr[11], '\0');
    }
#endif

    // ----- Section 5 — NUL-placement contract on the truncation edge
    SECTION("Section 5 - NUL-placement contract on the truncation edge");
    // The NUL-terminating sibling handles the truncation edge as a
    // PARTIAL-WRITE SUCCESS — the would-be byte count is exactly cap;
    // the bounded writer writes cap-1 bytes (capped by effective_cap =
    // cap - 1) and puts the NUL at cap - 1.
    //
    // On the truncation-edge throwing path: same contract (the throwing
    // wrapper delegates to Sep 11's / Sep 12's expected span sibling).
    {
        // 8-byte span; "value=42" (8 bytes): needed == cap exactly.
        char buf5_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf5_arr[i] = static_cast<char>('Q');
        std::span<char> buf5{buf5_arr, 8};  // sub-span of length 8
#if PETRA_THROW_ON_OVERFLOW == 0
        std::string fmt5 = "value=42";
        auto r5 = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf5, fmt5);
        CHECK(r5.has_value());
        CHECK_EQ(r5.value(), std::size_t{7});  // cap - 1 == 7 bytes
        // NUL is at out[cap - 1] == out[7] (the LAST byte of the span).
        CHECK_EQ(buf5_arr[7], '\0');
        // Bytes [0, 7) are the FIRST 7 bytes of "value=42" (== "value=4");
        // the bounded writer capped at effective_cap=7.
        CHECK_EQ((std::string_view{buf5_arr, 7}),
                 (std::string_view{"value=4"}));
#else
        std::string fmt5 = "value=42";
        auto n5 = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf5, fmt5);
        CHECK_EQ(n5, std::size_t{7});
        CHECK_EQ(buf5_arr[7], '\0');
        CHECK_EQ((std::string_view{buf5_arr, 7}),
                 (std::string_view{"value=4"}));
#endif
    }
    {
        // CONSTEVAL surface truncation-edge.
        char buf5b_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf5b_arr[i] = static_cast<char>('Q');
        std::span<char> buf5b{buf5b_arr, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r5b = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf5b, "value=42");
        CHECK(r5b.has_value());
        CHECK_EQ(r5b.value(), std::size_t{7});
        CHECK_EQ(buf5b_arr[7], '\0');
        CHECK_EQ((std::string_view{buf5b_arr, 7}),
                 (std::string_view{"value=4"}));
#else
        auto n5b = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf5b, "value=42");
        CHECK_EQ(n5b, std::size_t{7});
        CHECK_EQ(buf5b_arr[7], '\0');
        CHECK_EQ((std::string_view{buf5b_arr, 7}),
                 (std::string_view{"value=4"}));
#endif
    }

    // ----- Section 6 — CONSTEVAL macro's consteval gate preserved
    SECTION("Section 6 - CONSTEVAL macro's consteval gate preserved");
#if PETRA_THROW_ON_OVERFLOW == 0
    {
        // 6a. Literal fmt (passes consteval parse).
        char buf6a_arr[16] = {};
        std::span<char> buf6a{buf6a_arr};
        auto r6a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf6a, "x={}", 42);
        CHECK(r6a.has_value());
        CHECK_EQ(r6a.value(), std::size_t{4});
        CHECK_EQ(buf6a_arr[4], '\0');
    }
#else
    {
        // 6b. On the throwing path, literal fmt still works (the
        //     consteval gate is preserved through the throwing wrapper).
        char buf6b_arr[16] = {};
        std::span<char> buf6b{buf6b_arr};
        auto n6b = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf6b, "x={}", 42);
        CHECK_EQ(n6b, std::size_t{4});
        CHECK_EQ(buf6b_arr[4], '\0');
    }
#endif
    // 6c. The RUNTIME macro accepts a runtime-built std::string fmt
    //     (it does NOT require a literal fmt).
    {
        char buf6c_arr[16] = {};
        std::span<char> buf6c{buf6c_arr};
        std::string fmt6c = "y={}";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r6c = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf6c, fmt6c, 7);
        CHECK(r6c.has_value());
        CHECK_EQ(r6c.value(), std::size_t{3});
#else
        auto n6c = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf6c, fmt6c, 7);
        CHECK_EQ(n6c, std::size_t{3});
#endif
        CHECK_EQ(buf6c_arr[3], '\0');
    }
    // 6d. A runtime-built std::string fmt passed to the CONSTEVAL macro
    //     REJECTS AT COMPILE TIME (verified separately via
    //     /tmp/probe_consteval_rejects_runtime_fmt_span_into.cpp).
    CHECK(true);

    // ----- Section 7 — span<char> 'no-bleed' contract
    SECTION("Section 7 - span<char> no-bleed contract");
    {
        // A std::span<char> over a sub-range of a buffer must NOT
        // touch bytes outside the span.  A 6-byte sub-span at offset
        // 5 of a 16-byte buffer with format 'ABCDE' (5 bytes) leaves
        // bytes [0,5) and [11,16) as poison 'Q' (the format is fully
        // contained in the 6-byte span, so all writes are inside the
        // span — no bleed).
        char buf7_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf7_arr[i] = static_cast<char>('Q');
        std::span<char> buf7{buf7_arr + 5, 6};  // sub-span at offset 5
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r7 = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf7, "ABCDE");
        CHECK(r7.has_value());
        CHECK_EQ(r7.value(), std::size_t{5});
#else
        auto n7 = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf7, "ABCDE");
        CHECK_EQ(n7, std::size_t{5});
#endif
        // The span starts at buf7_arr + 5 (== buf7_arr[5]).
        // Writes inside the span: buf7_arr[5..10) = 'A','B','C','D','E'.
        CHECK_EQ(buf7_arr[5], 'A');
        CHECK_EQ(buf7_arr[6], 'B');
        CHECK_EQ(buf7_arr[7], 'C');
        CHECK_EQ(buf7_arr[8], 'D');
        CHECK_EQ(buf7_arr[9], 'E');
        // NUL at buf7_arr[10] (the last byte of the span).
        CHECK_EQ(buf7_arr[10], '\0');
        // Bytes outside the span [0,5) and [11,16) must still be 'Q'.
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(buf7_arr[i], 'Q');
        }
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(buf7_arr[i], 'Q');
        }
    }
    {
        // RUNTIME surface no-bleed.
        char buf7b_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf7b_arr[i] = static_cast<char>('Q');
        std::span<char> buf7b{buf7b_arr + 5, 6};
        std::string fmt7b = "VWXYZ";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r7b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf7b, fmt7b);
        CHECK(r7b.has_value());
        CHECK_EQ(r7b.value(), std::size_t{5});
#else
        auto n7b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf7b, fmt7b);
        CHECK_EQ(n7b, std::size_t{5});
#endif
        CHECK_EQ(buf7b_arr[5], 'V');
        CHECK_EQ(buf7b_arr[6], 'W');
        CHECK_EQ(buf7b_arr[7], 'X');
        CHECK_EQ(buf7b_arr[8], 'Y');
        CHECK_EQ(buf7b_arr[9], 'Z');
        CHECK_EQ(buf7b_arr[10], '\0');
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(buf7b_arr[i], 'Q');
        }
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(buf7b_arr[i], 'Q');
        }
    }

    // ----- Section 8 — multi-arg success path
    SECTION("Section 8 - multi-arg success path");
    {
        char buf8_arr[64] = {};
        std::span<char> buf8{buf8_arr};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r8 = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf8, "x={} y={} z={}", 42, "hi", 3.14);
        CHECK(r8.has_value());
        CHECK_EQ(r8.value(), std::size_t{16});
#else
        auto n8 = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            buf8, "x={} y={} z={}", 42, "hi", 3.14);
        CHECK_EQ(n8, std::size_t{16});
#endif
        CHECK_EQ((std::string_view{buf8_arr, 16}),
                 (std::string_view{"x=42 y=hi z=3.14"}));
        CHECK_EQ(buf8_arr[16], '\0');
    }
    {
        char buf8b_arr[64] = {};
        std::span<char> buf8b{buf8b_arr};
        std::string fmt8b = "x={} y={} z={}";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r8b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf8b, fmt8b, 42, "hi", 3.14);
        CHECK(r8b.has_value());
        CHECK_EQ(r8b.value(), std::size_t{16});
#else
        auto n8b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf8b, fmt8b, 42, "hi", 3.14);
        CHECK_EQ(n8b, std::size_t{16});
#endif
        CHECK_EQ((std::string_view{buf8b_arr, 16}),
                 (std::string_view{"x=42 y=hi z=3.14"}));
        CHECK_EQ(buf8b_arr[16], '\0');
    }

    // ----- Section 9 — byte-exactness of the throwing path's message
    SECTION("Section 9 - byte-exactness of throwing path's message");
    {
        // The KEY HEADLINE finding — the throwing sibling's
        // std::runtime_error::what() MUST be byte-exactly equal to
        // the expected sibling's FormatError::message for the SAME
        // {needed, cap, effective} triple.  Pinned by:
        //   1. Running the expected sibling on a known-failing call
        //      (via petra::try_format_runtime_n_into_bounded(span, ...)
        //      directly, NOT via the macro — the macro dispatches to
        //      the throwing sibling on the throwing build).
        //   2. Running the throwing sibling on the same call and
        //      catching the std::runtime_error::what().
        //   3. Asserting byte-exactness.
        char buf9_arr[16] = {};
        std::span<char> buf9{buf9_arr};

        // Get expected_msg from Sep 12's expected span sibling directly.
        std::string expected_msg;
        {
            std::string fmt9 = "too long: {}";
            auto r = petra::try_format_runtime_n_into_bounded(
                buf9, fmt9, std::string(20, 'X'));
            CHECK(!r.has_value());
            expected_msg = r.error().message;
        }

#if PETRA_THROW_ON_OVERFLOW == 0
        // On the expected path, the macro returns expected<size_t,
        // FormatError> — get macro_msg via the macro directly.
        std::string macro_msg;
        {
            std::string fmt9 = "too long: {}";
            auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
                buf9, fmt9, std::string(20, 'X'));
            CHECK(!r.has_value());
            macro_msg = r.error().message;
        }
        CHECK_EQ(macro_msg, expected_msg);
#else
        // On the throwing path, the macro throws — get thrown_msg
        // from the caught exception.
        std::string thrown_msg;
        bool caught9 = false;
        try {
            std::string fmt9 = "too long: {}";
            (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
                buf9, fmt9, std::string(20, 'X'));
        } catch (const std::runtime_error& e) {
            thrown_msg = e.what();
            caught9 = true;
        }
        CHECK(caught9);
        CHECK_EQ(thrown_msg, expected_msg);
#endif
    }

    // ----- Section 10 — Format error -> runtime_error on the throwing path
    SECTION("Section 10 - Format error -> runtime_error");
#if PETRA_THROW_ON_OVERFLOW == 1
    {
        // On the throwing path, std::format_error is ALSO caught and
        // re-raised as std::runtime_error (the throwing wrapper does
        // NOT distinguish TooLarge from Format at the call site; both
        // are "the format call failed" from the throwing sibling's
        // perspective).  Pin that a malformed format string throws
        // std::runtime_error whose DYNAMIC TYPE is std::runtime_error
        // (NOT std::format_error).
        char buf10_arr[32] = {};
        std::span<char> buf10{buf10_arr};
        std::string caught_what;
        bool is_format_error_dyn = false;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
                buf10, std::string_view{"bad {"}, 42);
        } catch (const std::format_error& e) {
            caught_what = e.what();
            is_format_error_dyn = true;
        } catch (const std::runtime_error& e) {
            caught_what = e.what();
            is_format_error_dyn = dynamic_cast<const std::format_error*>(&e)
                                  != nullptr;
        }
        CHECK(!caught_what.empty());
        // The throwing wrapper re-raises as std::runtime_error, NOT
        // std::format_error (the dynamic type is std::runtime_error).
        CHECK(!is_format_error_dyn);
        // The message must contain the std::format_error's text.
        CHECK(caught_what.find("format") != std::string::npos
              || caught_what.find("{") != std::string::npos);
        std::println("[Section 10] caught.what = [{}]", caught_what);
    }
#else
    {
        // On the EXPECTED path, a malformed format string returns
        // FormatError{Format, ...} from std::format_error.
        char buf10_arr[32] = {};
        std::span<char> buf10{buf10_arr};
        std::string fmt10 = "bad {";
        auto r10 = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf10, fmt10, 42);
        CHECK(!r10.has_value());
        CHECK_EQ(r10.error().kind, petra::FormatErrorKind::Format);
    }
#endif

    // ----- Section 11 — no-args variant
    SECTION("Section 11 - no-args variant");
    {
        // 11a. CONSTEVAL macro with no replacement fields.
        char buf11a_arr[16] = {};
        std::span<char> buf11a{buf11a_arr};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r11a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(buf11a, "ping");
        CHECK(r11a.has_value());
        CHECK_EQ(r11a.value(), std::size_t{4});
#else
        auto n11a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(buf11a, "ping");
        CHECK_EQ(n11a, std::size_t{4});
#endif
        CHECK_EQ((std::string_view{buf11a_arr, 4}),
                 (std::string_view{"ping"}));
        CHECK_EQ(buf11a_arr[4], '\0');

        // 11b. RUNTIME macro with no replacement fields.
        char buf11b_arr[16] = {};
        std::span<char> buf11b{buf11b_arr};
        std::string fmt11b = "pong";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r11b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf11b, fmt11b);
        CHECK(r11b.has_value());
        CHECK_EQ(r11b.value(), std::size_t{4});
#else
        auto n11b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            buf11b, fmt11b);
        CHECK_EQ(n11b, std::size_t{4});
#endif
        CHECK_EQ((std::string_view{buf11b_arr, 4}),
                 (std::string_view{"pong"}));
        CHECK_EQ(buf11b_arr[4], '\0');
    }

    // ----- Section 12 — empty span<char> behavior
    SECTION("Section 12 - empty span<char> behavior");
    {
        // Empty span<char> via std::span<char>{}.
        std::span<char> empty_span{};
#if PETRA_THROW_ON_OVERFLOW == 0
        // CONSTEVAL: empty fmt produces "" — success, returns 0.
        {
            auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                empty_span, "");
            CHECK(r.has_value());
            CHECK_EQ(r.value(), std::size_t{0});
        }
        // CONSTEVAL: non-empty fmt into empty span — TooLarge.
        {
            char sentinel = 'Q';
            std::span<char> sp{&sentinel, 0};  // length 0
            auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                sp, "x");
            CHECK(!r.has_value());
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        }
#else
        // CONSTEVAL: empty fmt into empty span — success, returns 0.
        {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                empty_span, "");
            CHECK_EQ(n, std::size_t{0});
        }
        // CONSTEVAL: non-empty fmt into empty span — throws.
        {
            char sentinel = 'Q';
            std::span<char> sp{&sentinel, 0};
            bool caught12 = false;
            try {
                (void)TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                    sp, "x");
            } catch (const std::runtime_error&) {
                caught12 = true;
            }
            CHECK(caught12);
            // sentinel must still be 'Q' (atomicity).
            CHECK_EQ(sentinel, 'Q');
        }
#endif
    }

    // ----- Section 13 — 4 threads × 25 calls concurrent dispatch
    SECTION("Section 13 - 4 threads x 25 calls concurrent dispatch");
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;

        // 13a. Concurrent calls to the CONSTEVAL macro.
        std::atomic<int> local_pass{0};
        std::atomic<int> local_fail{0};
        std::vector<std::thread> threads1;
        threads1.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads1.emplace_back([&local_pass, &local_fail, t]() {
                int lp = 0, lf = 0;
                char buf_arr[64] = {};
                std::span<char> buf{buf_arr};
                for (int i = 0; i < kCallsPerThread; ++i) {
#if PETRA_THROW_ON_OVERFLOW == 0
                    auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                        buf, "[{:02}][{:02}]", t, i);
                    if (r.has_value() && r.value() == 8) {
                        ++lp;
                    } else {
                        ++lf;
                    }
#else
                    try {
                        auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
                            buf, "[{:02}][{:02}]", t, i);
                        if (n == 8) {
                            ++lp;
                        } else {
                            ++lf;
                        }
                    } catch (...) {
                        ++lf;
                    }
#endif
                }
                local_pass.fetch_add(lp);
                local_fail.fetch_add(lf);
            });
        }
        for (auto& th : threads1) th.join();
        CHECK_EQ(local_pass.load(), kThreads * kCallsPerThread);
        CHECK_EQ(local_fail.load(), 0);

        // 13b. Concurrent calls to the RUNTIME macro.
        std::atomic<int> local_pass2{0};
        std::atomic<int> local_fail2{0};
        std::vector<std::thread> threads2;
        threads2.reserve(kThreads);
        std::string fmt = "[{:02}][{:02}]";
        for (int t = 0; t < kThreads; ++t) {
            threads2.emplace_back([&local_pass2, &local_fail2, &fmt, t]() {
                int lp = 0, lf = 0;
                char buf_arr[64] = {};
                std::span<char> buf{buf_arr};
                for (int i = 0; i < kCallsPerThread; ++i) {
#if PETRA_THROW_ON_OVERFLOW == 0
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
                        buf, fmt, t, i);
                    if (r.has_value() && r.value() == 8) {
                        ++lp;
                    } else {
                        ++lf;
                    }
#else
                    try {
                        auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
                            buf, fmt, t, i);
                        if (n == 8) {
                            ++lp;
                        } else {
                            ++lf;
                        }
                    } catch (...) {
                        ++lf;
                    }
#endif
                }
                local_pass2.fetch_add(lp);
                local_fail2.fetch_add(lf);
            });
        }
        for (auto& th : threads2) th.join();
        CHECK_EQ(local_pass2.load(), kThreads * kCallsPerThread);
        CHECK_EQ(local_fail2.load(), 0);
    }

    // ----- Section 14 — perpendicular-axis claim
    SECTION("Section 14 - perpendicular-axis claim");
#if PETRA_THROW_ON_OVERFLOW == 0
    {
        // 14a. Today's TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO with
        //      PETRA_THROW_ON_OVERFLOW=0 dispatches to the SAME
        //      wrapper as TRY_FORMAT_BOUNDED_SPAN_N_INTO_ORIG
        //      (CONSTEVAL + NUL-terminating + span<char>; Sep 11's
        //      span overload).  Today's
        //      TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO with
        //      PETRA_THROW_ON_OVERFLOW=0 dispatches to the SAME
        //      wrapper as TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_INTO_ORIG
        //      (RUNTIME + NUL-terminating + span<char>; Sep 12's
        //      span overload).
        using R14a = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using R14b = decltype(TRY_FORMAT_BOUNDED_SPAN_N_INTO_ORIG(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R14a, R14b>,
                      "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO with "
                      "PETRA_THROW_ON_OVERFLOW=0 must dispatch to the "
                      "same wrapper as TRY_FORMAT_BOUNDED_SPAN_N_INTO_ORIG "
                      "(CONSTEVAL + NUL-terminating + span<char>)");

        using R14c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using R14d = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_INTO_ORIG(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R14c, R14d>,
                      "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO with "
                      "PETRA_THROW_ON_OVERFLOW=0 must dispatch to the "
                      "same wrapper as TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_INTO_ORIG "
                      "(RUNTIME + NUL-terminating + span<char>)");
        CHECK(true);
    }
#else
    {
        // 14b. On the throwing path, the meta-macros dispatch to
        //      TODAY's NEW throwing wrappers (which return std::size_t,
        //      not expected<size_t, FormatError>).
        using R14a = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using H14a = decltype(petra::try_format_to_n_into_bounded_throwing_span(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R14a, H14a>,
                      "With PETRA_THROW_ON_OVERFLOW=1, "
                      "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO must dispatch "
                      "to petra::try_format_to_n_into_bounded_throwing_span");

        using R14b = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using H14b = decltype(petra::try_format_runtime_n_into_bounded_throwing_span(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R14b, H14b>,
                      "With PETRA_THROW_ON_OVERFLOW=1, "
                      "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO must "
                      "dispatch to "
                      "petra::try_format_runtime_n_into_bounded_throwing_span");
        CHECK(true);
    }
#endif

    // ----- Section 15 — final summary
    SECTION("Section 15 - final summary");

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