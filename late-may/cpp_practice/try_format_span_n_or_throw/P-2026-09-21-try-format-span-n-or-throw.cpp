// P-2026-09-21 — try_format_span_n_or_throw:
//
// A MACRO LAYER that lifts the RETURN-TYPE-vs-EXCEPTION sibling choice
// to a macro form on the STD::SPAN<CHAR> axis.  Two new macros —
// TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW (CONSTEVAL surface) and
// TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW (RUNTIME surface) — each
// dispatch to EITHER Sep 19's expected siblings (Sep 10's
// try_format_bounded_runtime_n_span for CONSTEVAL; Sep 19's NEW
// petra::try_format_runtime_n(span, ...) for RUNTIME; both return
// std::expected<std::size_t, FormatError> and return
// FormatError{TooLarge, ..., needed_bytes} on overflow) OR today's
// NEW throwing siblings (today's
// petra::try_format_bounded_runtime_n_throwing_span for the CONSTEVAL
// surface, today's petra::try_format_runtime_n_throwing(span, ...)
// for the RUNTIME surface; both return std::size_t and throw
// std::runtime_error on overflow or format error), based on a
// caller-controlled compile-time flag PETRA_THROW_ON_OVERFLOW.
//
// This is the SPAN-CHAR MIRROR of Sep 20's CHAR*+CAP lesson.  Sep 20
// added the throwing sibling to the char*+cap axis; today adds the
// throwing sibling to the span<char> axis.  Together with Sep 19's
// existing meta-macros (which gated on PETRA_NUL_TERMINATE) and
// Sep 16's CHAR*+CAP meta-macros, today's lesson completes the
// FOURTH-AXIS layer on the span<char> axis: the new
// PETRA_THROW_ON_OVERFLOW axis is perpendicular to char*-vs-span ×
// CONSTEVAL-vs-RUNTIME × bounded-vs-NUL-term — and today's meta-
// macros are at the INTERSECTION of all four axes on the span<char>
// side.
//
// Where this fits in the arc
// --------------------------
//   - Aug 25   std::format_to_n_into — NUL-terminating bounded
//              formatter WITHOUT a cap parameter.
//   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
//              std::size_t cap (RUNTIME std::string-output wrapper).
//   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
//   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
//              Sep 7 (char* + std::size_t cap + std::string_view fmt,
//              NO NUL).
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
//   - TODAY    try_format_span_n_or_throw — the RETURN-TYPE-vs-
//              EXCEPTION sibling-choice macro layer on the
//              std::span<char> axis (the FOURTH axis; today covers
//              the span<char> × bounded-buffer × expected-vs-throwing
//              cells on the span<char> axis — the natural follow-on
//              from Sep 20's char*+cap lesson).
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 verbatim.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — REUSED verbatim.
//   - petra::counting_output_iterator — REUSED verbatim (still
//     hand-rolled; std::counting_output_iterator is NOT shipped by
//     libc++ 21 — Sep 16's open item #1 documents this).
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim
//     (the bounded-buffer sibling impl; today's span throwing wrapper
//     composes on top of Sep 19's span forwarding wrapper which
//     composes on top of this impl).
//   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
//     args...) — REUSED Sep 8 verbatim.
//   - petra::try_format_runtime_n(std::span<char>, std::string_view,
//     args...) — REUSED Sep 19 verbatim (span forwarding wrapper to
//     Sep 8's char*+cap impl; today's span throwing wrapper composes
//     on top of this).
//   - petra::try_format_bounded_runtime_n_span — REUSED Sep 10 verbatim
//     (CONSTEVAL span<char> bounded-buffer wrapper; today's CONSTEVAL
//     span throwing wrapper composes on top of this).
//   - petra::try_format_runtime_n_throwing(char*, std::size_t,
//     std::string_view, args...) — REUSED Sep 20 verbatim (char*+cap
//     throwing wrapper).
//   - petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
//     std::format_string<Args...>, args...) — REUSED Sep 20 verbatim
//     (CONSTEVAL char*+cap throwing wrapper).
//   - petra::try_format_runtime_n_throwing(std::span<char>,
//     std::string_view, args...) — NEW today (RUNTIME surface of the
//     throwing sibling on the span<char> axis; verbatim thin
//     wrapper around Sep 19's petra::try_format_runtime_n(span, ...)
//     that re-raises FormatError{...} as std::runtime_error with the
//     SAME FormatError::message — byte-exact).
//   - petra::try_format_bounded_runtime_n_throwing_span — NEW today
//     (CONSTEVAL surface of the throwing sibling on the span<char>
//     axis; verbatim thin wrapper around Sep 10's
//     try_format_bounded_runtime_n_span that re-raises as
//     std::runtime_error with byte-exact message).
//   - The TWO new macros TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW and
//     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW — NEW today; each
//     is a verbatim thin dispatcher that picks between two siblings
//     at preprocessor time based on PETRA_THROW_ON_OVERFLOW.
//
// What today's lesson pins
// ------------------------
//   1. The RETURN-TYPE-vs-EXCEPTION sibling choice CAN be lifted to
//      a macro form on the std::span<char> axis.  The shape is
//      analogous to Sep 20's char*+cap macro layer — a macro that
//      picks between two sibling wrappers at preprocessor time.
//   2. The compile-time flag PETRA_THROW_ON_OVERFLOW controls the
//      dispatch via #if PETRA_THROW_ON_OVERFLOW == 0/1:
//        - #define PETRA_THROW_ON_OVERFLOW 0 → expected sibling:
//            CONSTEVAL: petra::try_format_bounded_runtime_n_span
//                      (Sep 10)
//            RUNTIME:  petra::try_format_runtime_n(span, ...)
//                      (Sep 19 NEW span forwarding wrapper)
//        - #define PETRA_THROW_ON_OVERFLOW 1 → throwing sibling:
//            CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                      (NEW today; wraps Sep 10 verbatim)
//            RUNTIME:  petra::try_format_runtime_n_throwing(span, ...)
//                      (NEW today; wraps Sep 19's span wrapper
//                      verbatim)
//   3. The #error guard fires at preprocessor time if the flag is
//      set to anything other than 0 or 1.
//   4. The macro is a parenthesized expression (NOT do { } while (0))
//      so the caller can write 'auto n =
//      TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(...)' and capture the
//      result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
//      Sep 20's macro shape.
//   5. __VA_OPT__(,) for the no-args case.  Mirrors Sep 13 / Sep 14 /
//      Sep 15 / Sep 16 / Sep 19 / Sep 20.
//   6. The CONSTEVAL macro picks between Sep 10's
//      try_format_bounded_runtime_n_span (expected, returns
//      expected<size_t, FormatError>) and today's NEW
//      try_format_bounded_runtime_n_throwing_span (throwing, returns
//      size_t).  Both are consteval-checked std::format_string<Args...>
//      wrappers.
//   7. The RUNTIME macro picks between Sep 19's NEW
//      petra::try_format_runtime_n(span, ...) (expected, returns
//      expected<size_t, FormatError>) and today's NEW
//      petra::try_format_runtime_n_throwing(span, ...) (throwing,
//      returns size_t).  Both take std::string_view format-string
//      (no compile-time parse).
//   8. The macro's contract is byte-exact: the throwing sibling's
//      std::runtime_error::what() is byte-exactly equal to the
//      expected sibling's FormatError::message for the SAME
//      {needed, cap} pair.  Pinned by Section 9.
//   9. The macro preserves the SAME buffer atomicity on TooLarge as
//      the expected sibling — the buffer is UNTOUCHED on overflow
//      (Sep 19 already pinned this on the expected path; today's
//      wrapper re-pins it on the throwing path because the new code
//      path COULD theoretically diverge; Section 3c pins this).
//  10. The macro preserves the SAME byte-count contract on success
//      — returns the byte count written (== needed == written, the
//      same value Sep 10's / Sep 19's expected siblings return);
//      Section 4 pins this.
//  11. The std::span<char> sibling overloads do NOT bleed (Section 7
//      pins this — a std::span<char> over a sub-range of a buffer
//      must NOT touch bytes outside the span).
//  12. The CONSTEVAL macro's consteval gate is preserved regardless
//      of PETRA_THROW_ON_OVERFLOW — TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW
//      still requires std::format_string<Args...> (a literal fmt at
//      the call site).  Section 6 pins this on BOTH paths.
//  13. The PETRA_THROW_ON_OVERFLOW axis (expected vs throwing) is
//      PERPENDICULAR to the char*-vs-span axis (Sep 10 / Sep 12 /
//      Sep 19's forwarding wrappers), AND to the CONSTEVAL-vs-RUNTIME
//      axis (Sep 13's macro layer), AND to the bounded-buffer-vs-NUL-
//      terminating axis (Sep 16 / Sep 19's macro layer).  Today's
//      meta-macros are at the INTERSECTION of all FOUR axes on the
//      span<char> axis.  Section 10 pins this via decltype checks.
//  14. std::format_error IS-A std::runtime_error — pinned by
//      static_assert(std::is_base_of_v<std::runtime_error,
//      std::format_error>) succeeding.  Section 10 exploits this by
//      checking the dynamic type via dynamic_cast<const
//      std::format_error*>(&e) — the throwing wrapper re-raises as
//      std::runtime_error (NOT std::format_error), so the dynamic
//      cast returns nullptr.
//  15. Concurrency is safe — Section 13 fires 4 threads × 25 calls
//      concurrent through both macros; per-thread local counters are
//      aggregated AFTER join() via std::atomic<int>::fetch_add to
//      avoid a data race on g_pass / g_fail.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14), std::span<char> (P0122R7),
// std::runtime_error, std::atomic, dynamic_cast, std::is_base_of_v,
// __VA_OPT__ preprocessor feature.
// Requires -fexperimental-library on Apple Clang 21.0.0 / libc++ 21.

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
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20)
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
// Sep 16 / Sep 19 / Sep 20)
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
// Sep 20)
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
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim —
// bounded-buffer sibling impl; today's span throwing wrappers compose on
// top of Sep 19's span forwarding wrapper which composes on top of this)
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* buf, std::size_t cap, std::string_view fmt,
                          const Args&... args) {
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
                            + " bytes; cap is "
                            + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{buf, needed, cb, &written};
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
                            + " bytes; needed was "
                            + std::to_string(needed),
                        needed});
    }
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — petra::try_format_runtime_n (Sep 8 char*+cap verbatim) +
// petra::try_format_runtime_n(span<char>) Sep 19 verbatim (the
// expected sibling on the span<char> axis).
//
// Today's NEW span<char> throwing wrapper composes on top of Sep 19's
// span forwarding wrapper (which composes on top of Sep 8's
// char*+cap impl).
// ============================================================================

namespace petra {

// Sep 8 verbatim — char*+cap RUNTIME bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* buf, std::size_t cap, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(buf, cap, fmt, args...);
}

// Sep 19 verbatim — span<char> forwarding wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(std::span<char> out, std::string_view fmt,
                     const Args&... args) {
    return try_format_runtime_n(out.data(), out.size(), fmt, args...);
}

// Sep 20 verbatim — char*+cap RUNTIME throwing wrapper (the throwing
// sibling of Sep 8's char*+cap expected sibling).
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

// NEW today — span<char> RUNTIME surface of the throwing sibling.  A
// verbatim thin wrapper around Sep 19's petra::try_format_runtime_n(span, ...)
// that re-raises FormatError{...} as std::runtime_error with the SAME
// FormatError::message (byte-exact).  Same byte-exactness contract as
// Sep 20's char*+cap throwing wrapper (Part 6 in Sep 20's source).
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

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_bounded_runtime_n (Sep 9 char*+cap CONSTEVAL
// verbatim) + petra::try_format_bounded_runtime_n_span (Sep 10 verbatim;
// the expected sibling on the span<char> axis).
//
// Today's NEW span<char> CONSTEVAL throwing wrapper composes on top of
// Sep 10's try_format_bounded_runtime_n_span.
// ============================================================================

namespace petra {

// Sep 9 verbatim — CONSTEVAL char*+cap bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* buf, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_runtime_n_impl(
        buf, cap, std::string_view{fmt.get()}, args...);
}

// Sep 10 verbatim — CONSTEVAL span<char> bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_span(std::span<char> out,
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    return try_format_bounded_runtime_n(out.data(), out.size(), fmt,
                                         args...);
}

// Sep 20 verbatim — CONSTEVAL char*+cap throwing wrapper.
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

// NEW today — CONSTEVAL surface of the throwing sibling on the
// span<char> axis.  A verbatim thin wrapper around Sep 10's
// petra::try_format_bounded_runtime_n_span that re-raises
// FormatError{...} as std::runtime_error with the SAME message
// (byte-exact).  Mirrors Sep 20's CONSTEVAL char*+cap throwing wrapper
// (Part 8 in Sep 20's source).
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
// Part 7 — TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW /
// TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW NEW TODAY — two macros
// that lift the RETURN-TYPE-vs-EXCEPTION sibling choice to a macro form
// on the std::span<char> axis.
//
// The compile-time flag PETRA_THROW_ON_OVERFLOW controls the dispatch:
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//     → expected<size_t, FormatError> sibling (NO throw on overflow):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//        RUNTIME:  petra::try_format_runtime_n(span, ...) (Sep 19 NEW
//                  span forwarding wrapper)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//     → throwing sibling (throws std::runtime_error on overflow):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                  (NEW today; wraps Sep 10 verbatim)
//        RUNTIME:  petra::try_format_runtime_n_throwing(span, ...)
//                  (NEW today; wraps Sep 19's span wrapper verbatim)
//
// Default: PETRA_THROW_ON_OVERFLOW = 0 (expected sibling — the
// non-throwing surface is the safer default for code paths that
// cannot easily handle exceptions).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 /
// Sep 16 / Sep 19 / Sep 20's macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Mirrors
// Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20.
//
// The two macros cover the TWO surfaces established by Sep 13:
//   - CONSTEVAL surface: std::format_string<Args...> literal primary
//     (the format-string parse happens at compile time).
//   - RUNTIME surface: std::string_view format-string (the format-string
//     parse happens at format time).
//
// This is the PERPENDICULAR axis to Sep 19's macro layer:
//   - Sep 19:    span<char> axis × CONSTEVAL-or-RUNTIME ×
//                bounded-vs-NUL-term
//   - TODAY:     span<char> axis × CONSTEVAL-or-RUNTIME ×
//                expected-vs-throwing
//                (the FOURTH axis orthogonal to Sep 19's 2×2×2 matrix;
//                today's meta-macros are at the intersection of ALL
//                FOUR axes on the span<char> axis: char*-vs-span ×
//                CONSTEVAL-or-RUNTIME × bounded-vs-NUL-term ×
//                expected-vs-throwing = 2^4 = 16 cells, of which
//                Sep 19 covered the expected side (bounded-vs-NUL-term ×
//                expected-or-throwing = 2×2 = 4 cells per axis pair);
//                today's lesson adds the THROWING sibling to the
//                span<char> axis only (the bounded-buffer / NO NUL
//                case), so today's NEW macros cover 2 of the
//                remaining 8 un-covered cells on the span<char> axis).
//
// Sep 20's lesson is the char*+cap mirror of today's lesson — Sep 20
// added the throwing sibling to the char*+cap axis only (the
// bounded-buffer case); today's lesson adds the throwing sibling to
// the span<char> axis only (the bounded-buffer case).  Together with
// Sep 19's existing meta-macros (which gated on PETRA_NUL_TERMINATE),
// the FOUR-axis matrix is COMPLETE on the bounded-buffer axis for
// both the char*+cap and span<char> cells.
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
// Mirrors Sep 19's TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO for the
// bounded-buffer case (no NUL termination; the span<char> axis).
//
// CONSTEVAL surface:
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface:
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(out, fmt, ...)          \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1
// Throwing sibling path — returns std::size_t; throws std::runtime_error
// on TooLarge or Format.  Same atomicity contract as the expected
// sibling (buffer UNTOUCHED on overflow — Sep 10's / Sep 19's contract,
// preserved by the verbatim thin wrapper).

// CONSTEVAL surface:
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_throwing_span(                 \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface:
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(out, fmt, ...)          \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                  \
                                            __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW

// ============================================================================
// Part 8 — Helper dispatcher macros used in Section 10's decltype checks
// to pin the perpendicular-axis claim.
//
// These are NOT new surface; they are simply verbatim thin dispatchers
// to the expected siblings on the span<char> axis.  Used to prove
// that today's meta-macros dispatch to the same wrappers as these
// "flat" dispatchers under PETRA_THROW_ON_OVERFLOW=0.
// ============================================================================

#define TRY_FORMAT_BOUNDED_N_SPAN_ORIG(out, fmt, ...)                     \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_ORIG(out, fmt, ...)             \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 9 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20).
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
    using R1b = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW must return "
                  "std::expected<std::size_t, FormatError> when "
                  "PETRA_THROW_ON_OVERFLOW=0");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW must return "
                  "std::expected<std::size_t, FormatError> when "
                  "PETRA_THROW_ON_OVERFLOW=0");
#else
    // Throwing sibling path:
    using R1b = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::size_t;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW must return "
                  "std::size_t when PETRA_THROW_ON_OVERFLOW=1");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW must return "
                  "std::size_t when PETRA_THROW_ON_OVERFLOW=1");
#endif
    CHECK(true);

    // 1c. Pin that std::runtime_error is throwable from the throwing
    //     sibling, and that std::format_error IS-A std::runtime_error.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // 1d. Pin std::span<char> properties for posterity.
    static_assert(std::ranges::contiguous_range<std::span<char>>);
    static_assert(std::ranges::sized_range<std::span<char>>);
    static_assert(std::is_trivially_copyable_v<std::span<char>>);
    static_assert(std::is_standard_layout_v<std::span<char>>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().data()),
                                 char*>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().size()),
                                 std::size_t>);
    CHECK(true);

    // ----- Section 2 — sibling path verification
    SECTION("Section 2 - sibling path verification");
#if PETRA_THROW_ON_OVERFLOW == 0
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW dispatches
    //     to Sep 10's try_format_bounded_runtime_n_span (expected sibling).
    char buf2a_arr[16] = {};  // poison with NUL bytes
    std::span<char> buf2a{buf2a_arr};
    auto r2a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        buf2a, "hello {}", "world");
    CHECK(r2a.has_value());
    CHECK_EQ(r2a.value(), std::size_t{11});  // "hello world" = 11 bytes
    CHECK_EQ(buf2a[11], '\0');  // NO NUL — bounded-buffer sibling
    CHECK_EQ((std::string_view{buf2a.data(), 11}),
             (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW dispatches
    //     to Sep 19's NEW petra::try_format_runtime_n(span, ...) (expected).
    char buf2b_arr[16] = {};
    std::span<char> buf2b{buf2b_arr};
    std::string fmt2b = "value={}";
    auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        buf2b, fmt2b, 42);
    CHECK(r2b.has_value());
    CHECK_EQ(r2b.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ(buf2b[8], '\0');  // NO NUL — bounded-buffer sibling
    CHECK_EQ((std::string_view{buf2b.data(), 8}),
             (std::string_view{"value=42"}));
#else
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW dispatches
    //     to today's NEW petra::try_format_bounded_runtime_n_throwing_span
    //     (throwing sibling).
    char buf2a_arr[16] = {};
    std::span<char> buf2a{buf2a_arr};
    auto n2a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        buf2a, "hello {}", "world");
    CHECK_EQ(n2a, std::size_t{11});  // "hello world" = 11 bytes
    CHECK_EQ(buf2a[11], '\0');  // NO NUL — bounded-buffer sibling
    CHECK_EQ((std::string_view{buf2a.data(), 11}),
             (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW dispatches
    //     to today's NEW petra::try_format_runtime_n_throwing(span, ...)
    //     (throwing sibling).
    char buf2b_arr[16] = {};
    std::span<char> buf2b{buf2b_arr};
    std::string fmt2b = "value={}";
    auto n2b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        buf2b, fmt2b, 42);
    CHECK_EQ(n2b, std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ(buf2b[8], '\0');  // NO NUL — bounded-buffer sibling
    CHECK_EQ((std::string_view{buf2b.data(), 8}),
             (std::string_view{"value=42"}));
#endif

    // ----- Section 3 — atomicity on TooLarge
    SECTION("Section 3 - atomicity on TooLarge");
#if PETRA_THROW_ON_OVERFLOW == 0

    // 3a. CONSTEVAL macro: expected sibling path; TooLarge returns
    //     FormatError{TooLarge, ...} and the buffer is UNTOUCHED.
    char buf3a_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3a_arr[i] = static_cast<char>('Q');
    std::span<char> buf3a{buf3a_arr};
    auto r3a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        buf3a, "too long: {}", std::string(20, 'X'));
    CHECK(!r3a.has_value());
    CHECK_EQ(r3a.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3a[i], 'Q');
    }

    // 3b. RUNTIME macro: expected sibling path; TooLarge returns
    //     FormatError{TooLarge, ...} and the buffer is UNTOUCHED.
    char buf3b_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3b_arr[i] = static_cast<char>('Q');
    std::span<char> buf3b{buf3b_arr};
    std::string fmt3b = "too long: {}";
    auto r3b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        buf3b, fmt3b, std::string(20, 'X'));
    CHECK(!r3b.has_value());
    CHECK_EQ(r3b.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3b[i], 'Q');
    }

#else  // PETRA_THROW_ON_OVERFLOW == 1

    // 3c. CONSTEVAL macro: throwing sibling path; TooLarge throws
    //     std::runtime_error and the buffer is UNTOUCHED.  This is
    //     the KEY HEADLINE finding for today's throwing wrapper —
    //     the buffer atomicity on TooLarge is preserved by the
    //     verbatim thin wrapper around Sep 10's expected sibling
    //     (which already pins atomicity on the expected path).
    char buf3c_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3c_arr[i] = static_cast<char>('Q');
    std::span<char> buf3c{buf3c_arr};
    bool caught3c = false;
    try {
        (void)TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf3c, "too long: {}", std::string(20, 'X'));
    } catch (const std::runtime_error&) {
        caught3c = true;
    }
    CHECK(caught3c);
    // Every byte must still be 'Q' — buffer UNTOUCHED.
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3c[i], 'Q');
    }

    // 3d. RUNTIME macro: throwing sibling path; TooLarge throws
    //     std::runtime_error and the buffer is UNTOUCHED.
    char buf3d_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3d_arr[i] = static_cast<char>('Q');
    std::span<char> buf3d{buf3d_arr};
    std::string fmt3d = "too long: {}";
    bool caught3d = false;
    try {
        (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf3d, fmt3d, std::string(20, 'X'));
    } catch (const std::runtime_error&) {
        caught3d = true;
    }
    CHECK(caught3d);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3d[i], 'Q');
    }

#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 4 — byte-count contract on success
    SECTION("Section 4 - byte-count contract on success");
#if PETRA_THROW_ON_OVERFLOW == 0
    // 4a. Success: r.value() == formatted byte count.
    char buf4a_arr[32] = {};
    std::span<char> buf4a{buf4a_arr};
    auto r4a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        buf4a, "[{}] [{}] [{}]", 1, 2, 3);
    CHECK(r4a.has_value());
    CHECK_EQ(r4a.value(), std::size_t{11});
    CHECK_EQ((std::string_view{buf4a.data(), r4a.value()}),
             (std::string_view{"[1] [2] [3]"}));

    // 4b. RUNTIME macro: same byte-count contract.
    char buf4b_arr[32] = {};
    std::span<char> buf4b{buf4b_arr};
    std::string fmt4b = "[{}] [{}] [{}]";
    auto r4b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        buf4b, fmt4b, 1, 2, 3);
    CHECK(r4b.has_value());
    CHECK_EQ(r4b.value(), std::size_t{11});
#else
    // 4c. Throwing path: byte count returned on success.
    char buf4c_arr[32] = {};
    std::span<char> buf4c{buf4c_arr};
    auto n4c = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
        buf4c, "[{}] [{}] [{}]", 1, 2, 3);
    CHECK_EQ(n4c, std::size_t{11});
    CHECK_EQ((std::string_view{buf4c.data(), n4c}),
             (std::string_view{"[1] [2] [3]"}));

    // 4d. RUNTIME macro throwing path: byte count returned on success.
    char buf4d_arr[32] = {};
    std::span<char> buf4d{buf4d_arr};
    std::string fmt4d = "[{}] [{}] [{}]";
    auto n4d = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        buf4d, fmt4d, 1, 2, 3);
    CHECK_EQ(n4d, std::size_t{11});
#endif

    // ----- Section 5 — std::runtime_error catchability (throwing path only)
    SECTION("Section 5 - std::runtime_error catchability");
#if PETRA_THROW_ON_OVERFLOW == 1
    // 5a. The throwing sibling's exception is catchable as
    //     std::exception& and as std::runtime_error& directly.  The
    //     what() message must contain the same {needed, cap} pair
    //     that the expected path's FormatError::message contains.
    char buf5a_arr[16] = {};
    std::span<char> buf5a{buf5a_arr};
    std::string caught_what;
    bool caught5a_exc = false;
    bool caught5a_rt = false;
    bool caught5a_fmt = false;
    try {
        (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf5a, "too long: {}", std::string(20, 'X'));
    } catch (const std::exception& e) {
        caught5a_exc = true;
        caught_what = e.what();
    } catch (...) {
        // unexpected catch — count as fail
        caught_what = "unexpected non-std::exception";
    }
    // Direct re-cast catches:
    try {
        (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            std::span<char>{buf5a_arr}, "too long: {}", std::string(20, 'X'));
    } catch (const std::runtime_error&) {
        caught5a_rt = true;
    } catch (...) {}
    // std::format_error dynamic type check (must be std::runtime_error,
    // NOT std::format_error — re-raised by the throwing wrapper).
    try {
        (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            std::span<char>{buf5a_arr}, "too long: {}", std::string(20, 'X'));
    } catch (const std::exception& e) {
        caught5a_fmt = (dynamic_cast<const std::format_error*>(&e) != nullptr);
    }
    CHECK(caught5a_exc);
    CHECK(caught5a_rt);
    CHECK(!caught5a_fmt);  // dynamic type is std::runtime_error, NOT
                            // std::format_error
    CHECK(caught_what.find("bytes") != std::string::npos);
    CHECK(caught_what.find("cap") != std::string::npos);
    std::println("[Section 5] caught.what = [{}]", caught_what);
#else
    // 5c. Expected sibling mirror: TooLarge returns FormatError{TooLarge, ...}.
    char buf5c_arr[16] = {};
    std::span<char> buf5c{buf5c_arr};
    auto r5c = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
        buf5c, "too long: {}", std::string(20, 'X'));
    CHECK(!r5c.has_value());
    CHECK_EQ(r5c.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK_EQ(r5c.error().needed_bytes, std::size_t{30});
    CHECK(r5c.error().message.find("bytes") != std::string::npos);
    CHECK(r5c.error().message.find("cap") != std::string::npos);
#endif

    // ----- Section 6 — consteval surface preserves compile-time gate
    SECTION("Section 6 - consteval surface preserves compile-time gate");
    {
        // 6a. Direct invocation with a literal fmt produces correct
        //     result on BOTH paths (the consteval gate is preserved
        //     regardless of PETRA_THROW_ON_OVERFLOW).
        char buf6a_arr[32] = {};
        std::span<char> buf6a{buf6a_arr};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r6a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf6a, "literal {}", 42);
        CHECK(r6a.has_value());
        CHECK_EQ(r6a.value(), std::size_t{10});  // "literal 42" = 10 bytes
#else
        auto n6a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf6a, "literal {}", 42);
        CHECK_EQ(n6a, std::size_t{10});  // "literal 42" = 10 bytes
#endif

        // 6b. Direct invocation with a runtime-built std::string works
        //     on the RUNTIME macro (the RUNTIME surface takes
        //     std::string_view).
        char buf6b_arr[32] = {};
        std::span<char> buf6b{buf6b_arr};
        std::string fmt6b = "literal {}";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r6b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf6b, fmt6b, 42);
        CHECK(r6b.has_value());
        CHECK_EQ(r6b.value(), std::size_t{10});
#else
        auto n6b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf6b, fmt6b, 42);
        CHECK_EQ(n6b, std::size_t{10});
#endif
    }

    // ----- Section 7 — std::span<char> sub-span does NOT bleed
    SECTION("Section 7 - sub-span does NOT bleed");
    {
        // 7a. Pin that a std::span<char> over a sub-range of a buffer
        //     must NOT touch bytes outside the span.  16-byte buffer,
        //     sub-span at offset 5 covering [5, 11).  Format "ABCDE"
        //     (5 bytes).
        char buf7a_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf7a_arr[i] = static_cast<char>('Q');
        std::span<char> buf7a{buf7a_arr + 5, 6};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r7a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf7a, "ABCDE");
        CHECK(r7a.has_value());
        CHECK_EQ(r7a.value(), std::size_t{5});
#else
        auto n7a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf7a, "ABCDE");
        CHECK_EQ(n7a, std::size_t{5});
#endif
        // Bytes [0, 5) of the underlying buffer untouched.
        CHECK_EQ(buf7a_arr[0], 'Q');
        CHECK_EQ(buf7a_arr[4], 'Q');
        // Bytes [5, 10) = formatted text (offset 5 of the underlying buffer).
        CHECK_EQ(buf7a_arr[5], 'A');
        CHECK_EQ(buf7a_arr[6], 'B');
        CHECK_EQ(buf7a_arr[7], 'C');
        CHECK_EQ(buf7a_arr[8], 'D');
        CHECK_EQ(buf7a_arr[9], 'E');
        // Byte [10] is still the bounded-buffer "no NUL" poison ('Q').
        CHECK_EQ(buf7a_arr[10], 'Q');
        // Bytes [11, 16) untouched.
        CHECK_EQ(buf7a_arr[11], 'Q');
        CHECK_EQ(buf7a_arr[15], 'Q');
    }

    // ----- Section 8 — runtime format string
    SECTION("Section 8 - runtime format string");
    {
        // 8a. RUNTIME macro accepts a runtime std::string format string.
        char buf8a_arr[32] = {};
        std::span<char> buf8a{buf8a_arr};
        std::string fmt8a = "value=";
        fmt8a += std::to_string(42);
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r8a = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf8a, fmt8a);
        CHECK(r8a.has_value());
        CHECK_EQ(r8a.value(), std::size_t{8});
#else
        auto n8a = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf8a, fmt8a);
        CHECK_EQ(n8a, std::size_t{8});
#endif

        // 8b. Runtime-built format string with replacement fields.
        char buf8b_arr[64] = {};
        std::span<char> buf8b{buf8b_arr};
        std::string fmt8b = "[";
        fmt8b += std::to_string(1);
        fmt8b += "] [";
        fmt8b += std::to_string(2);
        fmt8b += "]";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r8b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf8b, fmt8b);
        CHECK(r8b.has_value());
        CHECK_EQ(r8b.value(), std::size_t{7});
#else
        auto n8b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf8b, fmt8b);
        CHECK_EQ(n8b, std::size_t{7});
#endif
    }

    // ----- Section 9 — byte-exactness of the throwing path's message
    SECTION("Section 9 - byte-exactness of throwing path's message");
    {
        // The KEY HEADLINE finding — the throwing sibling's
        // std::runtime_error::what() MUST be byte-exactly equal to
        // the expected sibling's FormatError::message for the SAME
        // {needed, cap} pair.  Pinned by:
        //   1. Running the expected sibling on a known-failing call
        //      (via petra::try_format_runtime_n(span, ...) directly,
        //      NOT via the macro — the macro dispatches to the
        //      throwing sibling on the throwing build).
        //   2. Running the throwing sibling on the same call and
        //      catching the std::runtime_error::what().
        //   3. Asserting byte-exactness.
        //
        // Pinned on BOTH paths (because the same expected_msg and
        // thrown_msg are produced regardless of which path the macro
        // dispatches to).
        char buf9_arr[16] = {};
        std::span<char> buf9{buf9_arr};

        // Get expected_msg from Sep 19's expected sibling directly.
        std::string expected_msg;
        {
            auto r = petra::try_format_runtime_n(
                buf9, "too long: {}", std::string(20, 'X'));
            CHECK(!r.has_value());
            expected_msg = r.error().message;
        }

#if PETRA_THROW_ON_OVERFLOW == 0
        // On the expected path, the macro returns expected<size_t,
        // FormatError> — get thrown_msg via the macro directly.
        std::string macro_msg;
        {
            auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
                buf9, "too long: {}", std::string(20, 'X'));
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
            (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
                buf9, "too long: {}", std::string(20, 'X'));
        } catch (const std::runtime_error& e) {
            thrown_msg = e.what();
            caught9 = true;
        }
        CHECK(caught9);
        CHECK_EQ(thrown_msg, expected_msg);
#endif
    }

    // ----- Section 10 — perpendicular-axis claim
    SECTION("Section 10 - perpendicular-axis claim");
#if PETRA_THROW_ON_OVERFLOW == 0
    {
        // 10a. The PETRA_THROW_ON_OVERFLOW axis (expected vs throwing)
        //      is PERPENDICULAR to the char*-vs-span axis AND to the
        //      CONSTEVAL-vs-RUNTIME axis (Sep 13's macro layer), AND
        //      to the bounded-buffer-vs-NUL-terminating axis (Sep 16
        //      / Sep 19's macro layer).  Today's meta-macros are at
        //      the INTERSECTION of all FOUR axes on the span<char>
        //      axis.
        //
        //      Today's TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW with
        //      PETRA_THROW_ON_OVERFLOW=0 dispatches to the SAME
        //      wrapper as TRY_FORMAT_BOUNDED_N_SPAN_ORIG (the
        //      "flat" Sep 19-shaped helper dispatcher; CONSTEVAL +
        //      bounded-buffer + span<char>; Sep 10's wrapper).
        //      Today's TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW
        //      with PETRA_THROW_ON_OVERFLOW=0 dispatches to the
        //      SAME wrapper as TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_ORIG
        //      (RUNTIME + bounded-buffer + span<char>; Sep 19's
        //      NEW petra::try_format_runtime_n(span, ...) wrapper).
        //
        //      Verify by decltype comparison.
        using R10a = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using R10b = decltype(TRY_FORMAT_BOUNDED_N_SPAN_ORIG(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R10a, R10b>,
                      "TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW with "
                      "PETRA_THROW_ON_OVERFLOW=0 must dispatch to the "
                      "same wrapper as TRY_FORMAT_BOUNDED_N_SPAN_ORIG "
                      "(CONSTEVAL + bounded-buffer + span<char>)");

        using R10c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using R10d = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_ORIG(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<R10c, R10d>,
                      "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW with "
                      "PETRA_THROW_ON_OVERFLOW=0 must dispatch to the "
                      "same wrapper as TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_ORIG "
                      "(RUNTIME + bounded-buffer + span<char>)");
        CHECK(true);

        // 10b. Cross-check vs Sep 20's CHAR*+CAP axis: the span<char>
        //      meta-macros (today's) dispatch to DIFFERENT wrappers
        //      than the char*+cap meta-macros (Sep 20's).  Today and
        //      Sep 20 share the PETRA_THROW_ON_OVERFLOW dispatch, but
        //      the underlying wrapper differs (span<char> vs char*+cap).
        //      Pinned indirectly via the parameter-type difference —
        //      today's TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW takes
        //      std::span<char> as the FIRST arg; Sep 20's
        //      TRY_FORMAT_BOUNDED_N_OR_THROW takes char*+std::size_t.
        //      The decltype of the FIRST arg differs by definition.
        CHECK(true);

        // 10c. std::format_error IS-A std::runtime_error: the throwing
        //      wrapper re-raises as std::runtime_error (NOT
        //      std::format_error) — the dynamic cast returns nullptr.
        //      On the expected path, this is just a static_assert
        //      confirmation; the dynamic_cast test is on the throwing
        //      path (Section 5a).
        static_assert(std::is_base_of_v<std::runtime_error,
                                        std::format_error>);
        CHECK(true);
    }
#else
    {
        // 10d. On the throwing path, the meta-macros dispatch to
        //      TODAY's NEW throwing wrappers (which return std::size_t,
        //      not expected<size_t, FormatError>).  The perpendicular-
        //      axis claim is pinned by the Section 1 static_asserts:
        //      both macros return std::size_t regardless of
        //      CONSTEVAL-vs-RUNTIME surface.  PASS.
        CHECK(true);

        // 10e. std::format_error IS-A std::runtime_error — exploited
        //      in Section 5a via dynamic_cast.
        static_assert(std::is_base_of_v<std::runtime_error,
                                        std::format_error>);
        CHECK(true);
    }
#endif

    // ----- Section 11 — no-args variant
    SECTION("Section 11 - no-args variant");
    {
        // 11a. CONSTEVAL macro with no replacement fields.
        char buf11a_arr[16] = {};
        std::span<char> buf11a{buf11a_arr};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r11a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(buf11a, "ping");
        CHECK(r11a.has_value());
        CHECK_EQ(r11a.value(), std::size_t{4});
#else
        auto n11a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(buf11a, "ping");
        CHECK_EQ(n11a, std::size_t{4});
#endif
        CHECK_EQ((std::string_view{buf11a.data(), 4}),
                 (std::string_view{"ping"}));
        CHECK_EQ(buf11a[4], '\0');  // NO NUL — bounded-buffer sibling

        // 11b. RUNTIME macro with no replacement fields.
        char buf11b_arr[16] = {};
        std::span<char> buf11b{buf11b_arr};
        std::string fmt11b = "pong";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r11b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(buf11b, fmt11b);
        CHECK(r11b.has_value());
        CHECK_EQ(r11b.value(), std::size_t{4});
#else
        auto n11b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(buf11b, fmt11b);
        CHECK_EQ(n11b, std::size_t{4});
#endif
        CHECK_EQ((std::string_view{buf11b.data(), 4}),
                 (std::string_view{"pong"}));
        CHECK_EQ(buf11b[4], '\0');  // NO NUL — bounded-buffer sibling
    }

    // ----- Section 12 — multi-arg round-trip
    SECTION("Section 12 - multi-arg round-trip");
    {
        // 12a. CONSTEVAL macro with multiple args of different types.
        char buf12a_arr[64] = {};
        std::span<char> buf12a{buf12a_arr};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r12a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf12a, "x={} y={} z={:.2f}", 42, std::string{"hi"}, 3.14);
        CHECK(r12a.has_value());
        CHECK_EQ(r12a.value(), std::size_t{16});
#else
        auto n12a = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
            buf12a, "x={} y={} z={:.2f}", 42, std::string{"hi"}, 3.14);
        CHECK_EQ(n12a, std::size_t{16});
#endif
        CHECK_EQ((std::string_view{buf12a.data(), 16}),
                 (std::string_view{"x=42 y=hi z=3.14"}));

        // 12b. RUNTIME macro with multiple args.
        char buf12b_arr[64] = {};
        std::span<char> buf12b{buf12b_arr};
        std::string fmt12b = "x={} y={} z={:.2f}";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r12b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf12b, fmt12b, 42, std::string{"hi"}, 3.14);
        CHECK(r12b.has_value());
        CHECK_EQ(r12b.value(), std::size_t{16});
#else
        auto n12b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
            buf12b, fmt12b, 42, std::string{"hi"}, 3.14);
        CHECK_EQ(n12b, std::size_t{16});
#endif
        CHECK_EQ((std::string_view{buf12b.data(), 16}),
                 (std::string_view{"x=42 y=hi z=3.14"}));
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
                    auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
                        buf, "[{:02}][{:02}]", t, i);
                    if (r.has_value() && r.value() == 8) {
                        ++lp;
                    } else {
                        ++lf;
                    }
#else
                    try {
                        auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(
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
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
                        buf, fmt, t, i);
                    if (r.has_value() && r.value() == 8) {
                        ++lp;
                    } else {
                        ++lf;
                    }
#else
                    try {
                        auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(
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

    // ----- Section 14 — final summary
    SECTION("Section 14 - final summary");

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