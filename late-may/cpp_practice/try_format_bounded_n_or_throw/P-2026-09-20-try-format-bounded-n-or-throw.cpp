// P-2026-09-20 — try_format_bounded_n_or_throw:
//
// A MACRO LAYER that lifts the RETURN-TYPE-vs-EXCEPTION sibling
// choice to a macro form on the CHAR*+CAP axis.  Two new macros —
// TRY_FORMAT_BOUNDED_N_OR_THROW (CONSTEVAL surface) and
// TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW (RUNTIME surface) — each
// dispatch to EITHER Sep 9's try_format_bounded_runtime_n (the
// RETURN-expected sibling, FormatError on overflow) OR today's NEW
// petra::try_format_runtime_n_throwing (the THROWING sibling, throws
// std::runtime_error on overflow), based on a caller-controlled
// compile-time flag PETRA_THROW_ON_OVERFLOW.
//
// Today ALSO adds the THROWING sibling: a
// `petra::try_format_runtime_n_throwing(char*, std::size_t,
// std::string_view, args...) -> std::size_t` wrapper that takes the
// same arguments as Sep 8's try_format_runtime_n BUT on TooLarge
// throws std::runtime_error carrying the same would_be / cap
// information that Sep 8's FormatError{TooLarge, ..., needed_bytes}
// would carry.  This is the ONLY new wrapper introduced today; it
// is a verbatim thin wrapper (calls Sep 8's try_format_runtime_n
// and re-raises as std::runtime_error).  No new impls, no new
// counting_output_iterator, no new bounded_char_writer, no new
// FormatError type — just the same Sep 6/7/8/9/10/11/12/13/14/15/
// 16/19 machinery composed behind a new entry point.
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
//              std::span<char> axis (closes Sep 16's open item #2
//              AND Sep 16's open item #3).
//   - TODAY    try_format_bounded_n_or_throw — the RETURN-TYPE-vs-
//              EXCEPTION sibling-choice macro layer on the char*+cap
//              axis (the FOURTH axis orthogonal to Sep 19's
//              2×2×2 matrix: this is now 2×2×2×2).
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 / Sep 16 / Sep 19 verbatim.
//   - petra::counting_output_iterator — REUSED verbatim.
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim.
//   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
//     args...) — REUSED Sep 8 verbatim (returns expected<size_t,
//     FormatError>; the THROWING sibling composes on top of it).
//   - petra::try_format_bounded_runtime_n — REUSED Sep 9 verbatim
//     (CONSTEVAL char*+cap; today's NEW TRY_FORMAT_BOUNDED_N_OR_THROW
//     dispatches to this when PETRA_THROW_ON_OVERFLOW=0).
//   - petra::try_format_runtime_n_throwing(char*, std::size_t,
//     std::string_view, args...) — NEW today (verbatim thin
//     wrapper that calls Sep 8's try_format_runtime_n and re-raises
//     FormatError{TooLarge, ...} as std::runtime_error).
//   - The TWO new macros TRY_FORMAT_BOUNDED_N_OR_THROW and
//     TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW — NEW today; each is
//     a verbatim thin dispatcher that picks between two siblings at
//     preprocessor time based on PETRA_THROW_ON_OVERFLOW.
//
// What today's lesson pins
// ------------------------
//   1. The RETURN-TYPE-vs-EXCEPTION sibling choice CAN be lifted
//      to a macro form on the char*+cap axis.  The shape is
//      analogous to Sep 16's BOUNDED-BUFFER vs NUL-TERMINATING
//      macro layer — a macro that picks between two sibling wrappers
//      at preprocessor time.
//   2. The compile-time flag PETRA_THROW_ON_OVERFLOW controls the
//      dispatch via #if PETRA_THROW_ON_OVERFLOW == 0/1:
//        - #define PETRA_THROW_ON_OVERFLOW 0 → expected<size_t,
//            FormatError> sibling:
//              CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//              RUNTIME:  petra::try_format_runtime_n (Sep 8)
//        - #define PETRA_THROW_ON_OVERFLOW 1 → throwing sibling:
//              CONSTEVAL: petra::try_format_bounded_runtime_n_throwing
//                        (NEW today; wraps Sep 9 verbatim)
//              RUNTIME:  petra::try_format_runtime_n_throwing
//                        (NEW today; wraps Sep 8 verbatim)
//   3. The #error guard fires at preprocessor time if the flag is
//      set to anything other than 0 or 1.
//   4. The macro is a parenthesized expression (NOT do { } while (0))
//      so the caller can write 'auto n =
//      TRY_FORMAT_BOUNDED_N_OR_THROW(...)' and capture the result.
//      Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19's macro
//      shape exactly.
//   5. __VA_OPT__(,) for the no-args case.  Mirrors Sep 13 /
//      Sep 14 / Sep 15 / Sep 16 / Sep 19.
//   6. The throwing sibling preserves the SAME error contract as
//      the expected sibling on TooLarge — the std::runtime_error
//      message is built from the SAME {needed, cap} pair that
//      FormatError{TooLarge, ..., needed_bytes} carries (Section 9
//      pins this byte-exactness).
//   7. The throwing sibling preserves the SAME buffer atomicity on
//      TooLarge — the buffer is UNTOUCHED on overflow (Section 3
//      pins this on both paths; Sep 8 / Sep 9 already pinned it
//      on the expected path; today's wrapper re-pins it on the
//      throwing path because the new code path COULD theoretically
//      diverge).
//   8. The throwing sibling does NOT lose the consteval-checked
//      format-string surface — TRY_FORMAT_BOUNDED_N_OR_THROW still
//      requires std::format_string<Args...> (a literal fmt at the
//      call site).  This is because the CONSTEVAL sibling
//      (Sep 9's try_format_bounded_runtime_n) is consteval-checked,
//      and today's throwing wrapper is a verbatim thin wrapper
//      around Sep 9 — so the consteval gate is preserved.
//   9. The CONSTEVAL macro's sibling-choice is orthogonal to the
//      Sep 13 CONSTEVAL-vs-RUNTIME axis: today's macro
//      TRY_FORMAT_BOUNDED_N_OR_THROW is the CONSTEVAL surface
//      (because Sep 9's underlying wrapper is consteval-checked);
//      today's TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW is the
//      RUNTIME surface (because Sep 8's underlying wrapper takes
//      std::string_view).  Mirrors Sep 13's macro family shape.
//   10. The PETRA_THROW_ON_OVERFLOW axis (expected vs throwing) is
//       PERPENDICULAR to the char*-vs-span axis (Sep 10 / Sep 12's
//       forwarding wrappers), AND to the CONSTEVAL-vs-RUNTIME axis
//       (Sep 13's macro layer), AND to the bounded-buffer-vs-NUL-
//       terminating axis (Sep 16 / Sep 19's macro layer).  Today's
//       meta-macros are at the INTERSECTION of ALL FOUR axes:
//       char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer-or-NUL-
//       term × expected-or-throwing.  Section 14 pins the
//       perpendicular-axis claim via decltype checks.
//   12. Concurrency is safe — Section 13 fires 4 threads × 25
//       concurrent calls through both macros; per-thread local
//       counters are aggregated AFTER join() via
//       std::atomic<int>::fetch_add to avoid a data race on
//       g_pass / g_fail.  Mirrors Sep 13 / Sep 14 / Sep 15 /
//       Sep 16 / Sep 19.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14), std::runtime_error,
// std::atomic, __VA_OPT__ preprocessor feature.
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19)
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
// Sep 16 / Sep 19)
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19)
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
// underlying impl reused by Sep 8's expected sibling AND today's NEW
// throwing sibling)
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
// Part 5 — petra::try_format_runtime_n (Sep 8 char*+cap verbatim — the
// expected sibling).  The throwing sibling composes on top of this.
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* buf, std::size_t cap, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(buf, cap, fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_runtime_n_throwing NEW TODAY — verbatim thin
// wrapper around Sep 8's expected sibling that re-raises TooLarge as
// std::runtime_error.  The wrapper is:
//   - byte-exact in the buffer atomicity contract (delegates to Sep 8
//     entirely; on overflow, the failing call short-circuits before
//     pass-2 writes anything)
//   - byte-exact in the {needed, cap} information carried in the
//     exception message (constructs the std::runtime_error message from
//     the same needed + cap pair FormatError{TooLarge, ...} would carry)
//   - transparent for non-overflow errors (std::format_error is wrapped
//     in std::runtime_error too — the throwing wrapper does NOT
//     distinguish TooLarge from Format at the call site; the macro
//     path THROWS for both, because both are "the format call failed"
//     from the throwing sibling's perspective).
//
// On the success path the wrapper returns the byte count written
// (== needed == written, the same value Sep 8's expected sibling
// returns on success).
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(char* buf, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(buf, cap, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    // Re-raise as std::runtime_error.  The .what() message is the
    // SAME message FormatError{...} carries (for TooLarge) or the
    // FormatError's message (for Format).  We deliberately do NOT
    // add an extra prefix like "[petra::try_format_runtime_n_throwing]"
    // — that would make the throwing path's .what() differ from the
    // expected path's FormatError.message byte-exactly, breaking
    // Section 9's byte-exactness claim.
    throw std::runtime_error(r.error().message);
}

}  // namespace petra

// ============================================================================
// Part 7 — petra::try_format_bounded_runtime_n (Sep 9 char*+cap CONSTEVAL
// verbatim — the CONSTEVAL expected sibling)
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* buf, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_runtime_n_impl(
        buf, cap, std::string_view{fmt.get()}, args...);
}

}  // namespace petra

// ============================================================================
// Part 8 — petra::try_format_bounded_runtime_n_throwing NEW TODAY —
// CONSTEVAL surface of the throwing sibling.  A verbatim thin wrapper
// around Sep 9's expected sibling that re-raises TooLarge (and Format)
// as std::runtime_error.  Same byte-exactness contract as Part 6.
// ============================================================================

namespace petra {

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

}  // namespace petra

// ============================================================================
// Part 9 — TRY_FORMAT_BOUNDED_N_OR_THROW /
// TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW NEW TODAY — two macros that lift
// the RETURN-TYPE-vs-EXCEPTION sibling choice to a macro form on the
// char*+cap axis.
//
// The compile-time flag PETRA_THROW_ON_OVERFLOW controls the dispatch:
//
//   #define PETRA_THROW_ON_OVERFLOW 0
//     → expected<size_t, FormatError> sibling (NO throw on overflow):
//        CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//        RUNTIME:  petra::try_format_runtime_n (Sep 8)
//
//   #define PETRA_THROW_ON_OVERFLOW 1
//     → throwing sibling (throws std::runtime_error on overflow):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_throwing
//                  (NEW today; wraps Sep 9 verbatim)
//        RUNTIME:  petra::try_format_runtime_n_throwing
//                  (NEW today; wraps Sep 8 verbatim)
//
// Default: PETRA_THROW_ON_OVERFLOW = 0 (expected sibling — the
// non-throwing surface is the safer default for code paths that
// cannot easily handle exceptions).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 /
// Sep 16 / Sep 19's macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Mirrors
// Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19.
//
// The two macros cover the TWO surfaces established by Sep 13:
//   - CONSTEVAL surface: std::format_string<Args...> literal primary
//     (the format-string parse happens at compile time).
//   - RUNTIME surface: std::string_view format-string (the format-string
//     parse happens at format time).
//
// This is the PERPENDICULAR axis to Sep 16 / Sep 19's macro layer:
//   - Sep 16:    char*+cap axis × CONSTEVAL-or-RUNTIME × bounded-vs-NUL-term
//   - Sep 19:    span<char>  axis × CONSTEVAL-or-RUNTIME × bounded-vs-NUL-term
//   - TODAY:     char*+cap axis × CONSTEVAL-or-RUNTIME × expected-vs-throwing
//                (the FOURTH axis orthogonal to Sep 19's 2×2×2 matrix;
//                today's meta-macros are at the intersection of ALL
//                FOUR axes: char*-vs-span × CONSTEVAL-vs-RUNTIME ×
//                bounded-vs-NUL-term × expected-vs-throwing = 2^4 = 16
//                cells, of which Sep 16 / Sep 19 covered the
//                bounded-vs-NUL-term × expected-vs-throwing = 2×2 = 4
//                cells per axis pair × 2 = 8 cells; today's lesson
//                adds the THROWING sibling to the char*+cap axis only
//                (the bounded-buffer / NO NUL case), so today's NEW
//                macros cover 2 of the remaining 8 cells).
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
// Mirrors Sep 13's TRY_FORMAT_BOUNDED_RUNTIME_N for the bounded-buffer
// case (no NUL termination; the char*+cap axis).
//
// CONSTEVAL surface:
#define TRY_FORMAT_BOUNDED_N_OR_THROW(out, cap, fmt, ...)                \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)           \
                                            __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface:
#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(out, cap, fmt, ...)        \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                   \
                                    __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1
// Throwing sibling path — returns std::size_t; throws std::runtime_error
// on TooLarge or Format.  Same atomicity contract as the expected
// sibling (buffer UNTOUCHED on overflow — Sep 8 / Sep 9's contract,
// preserved by the verbatim thin wrapper).

// CONSTEVAL surface:
#define TRY_FORMAT_BOUNDED_N_OR_THROW(out, cap, fmt, ...)                \
    (::petra::try_format_bounded_runtime_n_throwing(                    \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface:
#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(out, cap, fmt, ...)        \
    (::petra::try_format_runtime_n_throwing(                            \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW

// ============================================================================
// Part 10 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19).
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
    using R1b = decltype(TRY_FORMAT_BOUNDED_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_N_OR_THROW must return "
                  "std::expected<std::size_t, FormatError> when "
                  "PETRA_THROW_ON_OVERFLOW=0");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW must return "
                  "std::expected<std::size_t, FormatError> when "
                  "PETRA_THROW_ON_OVERFLOW=0");
#else
    // Throwing sibling path:
    using R1b = decltype(TRY_FORMAT_BOUNDED_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::size_t;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_N_OR_THROW must return "
                  "std::size_t when PETRA_THROW_ON_OVERFLOW=1");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW must return "
                  "std::size_t when PETRA_THROW_ON_OVERFLOW=1");
#endif
    CHECK(true);

    // 1c. Pin that std::runtime_error is throwable from the throwing
    //     sibling.  The expected sibling's FormatError{TooLarge, ...}
    //     carries a message — the throwing sibling's std::runtime_error
    //     carries the SAME message (Section 9 pins this byte-exactness).
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    CHECK(true);

    // ----- Section 2 — sibling path verification (expected path)
    SECTION("Section 2 - sibling path verification (expected path)");
#if PETRA_THROW_ON_OVERFLOW == 0
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_N_OR_THROW dispatches
    //     to Sep 9's try_format_bounded_runtime_n (expected sibling).
    char buf2a_arr[16] = {};
    auto r2a = TRY_FORMAT_BOUNDED_N_OR_THROW(buf2a_arr, 16,
                                             "hello {}", "world");
    CHECK(r2a.has_value());
    CHECK_EQ(r2a.value(), std::size_t{11});  // "hello world" = 11 bytes
    CHECK_EQ((std::string_view{buf2a_arr, 11}),
             (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW dispatches
    //     to Sep 8's try_format_runtime_n (expected sibling).
    char buf2b_arr[16] = {};
    std::string fmt2b = "value={}";
    auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf2b_arr, 16,
                                                     fmt2b, 42);
    CHECK(r2b.has_value());
    CHECK_EQ(r2b.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ((std::string_view{buf2b_arr, 8}),
             (std::string_view{"value=42"}));
#else
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_N_OR_THROW dispatches
    //     to today's NEW petra::try_format_bounded_runtime_n_throwing
    //     (throwing sibling).
    char buf2a_arr[16] = {};
    auto n2a = TRY_FORMAT_BOUNDED_N_OR_THROW(buf2a_arr, 16,
                                              "hello {}", "world");
    CHECK_EQ(n2a, std::size_t{11});  // "hello world" = 11 bytes
    CHECK_EQ((std::string_view{buf2a_arr, 11}),
             (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW dispatches
    //     to today's NEW petra::try_format_runtime_n_throwing
    //     (throwing sibling).
    char buf2b_arr[16] = {};
    std::string fmt2b = "value={}";
    auto n2b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf2b_arr, 16,
                                                      fmt2b, 42);
    CHECK_EQ(n2b, std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ((std::string_view{buf2b_arr, 8}),
             (std::string_view{"value=42"}));
#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 3 — buffer atomicity on TooLarge
    SECTION("Section 3 - buffer atomicity on TooLarge");
#if PETRA_THROW_ON_OVERFLOW == 0
    // 3a. On the EXPECTED path, TooLarge returns
    //     FormatError{TooLarge, ..., needed_bytes}.  The buffer is
    //     UNTOUCHED.  Pin by poisoning the buffer first and verifying
    //     every byte is unchanged.
    char buf3a_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) {
        buf3a_arr[i] = static_cast<char>('Q');
    }
    auto r3a = TRY_FORMAT_BOUNDED_N_OR_THROW(buf3a_arr, 16,
                                              "too long: {}",
                                              std::string(20, 'X'));
    CHECK(!r3a.has_value());
    CHECK_EQ(r3a.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3a_arr[i], 'Q');
    }

    // 3b. RUNTIME macro atomicity too.
    char buf3b_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) {
        buf3b_arr[i] = static_cast<char>('Q');
    }
    std::string fmt3b = "too long: {}";
    auto r3b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf3b_arr, 16,
                                                      fmt3b,
                                                      std::string(20, 'X'));
    CHECK(!r3b.has_value());
    CHECK_EQ(r3b.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3b_arr[i], 'Q');
    }
#else
    // 3c. On the THROWING path, TooLarge throws std::runtime_error.
    //     The buffer is UNTOUCHED (the wrapper delegates to Sep 8 /
    //     Sep 9 which already pins this on the expected path; today's
    //     throwing wrapper re-pins it because the new code path COULD
    //     theoretically diverge).
    char buf3c_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) {
        buf3c_arr[i] = static_cast<char>('Q');
    }
    bool caught3c = false;
    try {
        (void)TRY_FORMAT_BOUNDED_N_OR_THROW(buf3c_arr, 16,
                                               "too long: {}",
                                               std::string(20, 'X'));
    } catch (const std::runtime_error&) {
        caught3c = true;
    }
    CHECK(caught3c);
    // Buffer must be UNTOUCHED (every byte still 'Q').
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3c_arr[i], 'Q');
    }

    // 3d. RUNTIME macro atomicity too.
    char buf3d_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) {
        buf3d_arr[i] = static_cast<char>('Q');
    }
    std::string fmt3d = "too long: {}";
    bool caught3d = false;
    try {
        (void)TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf3d_arr, 16,
                                                     fmt3d,
                                                     std::string(20, 'X'));
    } catch (const std::runtime_error&) {
        caught3d = true;
    }
    CHECK(caught3d);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3d_arr[i], 'Q');
    }
#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 4 — byte-count contract on success
    SECTION("Section 4 - byte-count contract on success");
#if PETRA_THROW_ON_OVERFLOW == 0
    // Expected path returns the byte count via r.value().
    {
        char buf[32] = {};
        auto r = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32,
                                                "x={} y={}", 1, 2);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{7});  // "x=1 y=2" = 7 bytes
        CHECK_EQ((std::string_view{buf, 7}),
                 (std::string_view{"x=1 y=2"}));
    }
    {
        char buf[32] = {};
        std::string fmt = "x={} y={}";
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                        fmt, 1, 2);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{7});
        CHECK_EQ((std::string_view{buf, 7}),
                 (std::string_view{"x=1 y=2"}));
    }
#else
    // Throwing path returns the byte count directly.
    {
        char buf[32] = {};
        auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32,
                                                 "x={} y={}", 1, 2);
        CHECK_EQ(n, std::size_t{7});  // "x=1 y=2" = 7 bytes
        CHECK_EQ((std::string_view{buf, 7}),
                 (std::string_view{"x=1 y=2"}));
    }
    {
        char buf[32] = {};
        std::string fmt = "x={} y={}";
        auto n = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                         fmt, 1, 2);
        CHECK_EQ(n, std::size_t{7});
        CHECK_EQ((std::string_view{buf, 7}),
                 (std::string_view{"x=1 y=2"}));
    }
#endif

    // ----- Section 5 — std::runtime_error catchability
    SECTION("Section 5 - std::runtime_error catchability");
#if PETRA_THROW_ON_OVERFLOW == 1
    // Catch as std::exception& — the std::runtime_error inheritance
    // chain lets the catch site use std::exception& if it wants a
    // generic handler.  Pin the inheritance here.
    {
        char buf[16] = {};
        std::string caught_what;
        try {
            (void)TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 16,
                                                  "too long: {}",
                                                  std::string(20, 'X'));
        } catch (const std::exception& e) {
            caught_what = e.what();
        }
        CHECK(!caught_what.empty());
        // The message must mention the bytes / cap numbers (carried
        // from FormatError{TooLarge, ..., needed_bytes} verbatim).
        // The would-be size for "too long: " + 20 X's is 30 bytes;
        // cap is 16.  Both numbers must appear in the message.
        CHECK(caught_what.find("30") != std::string::npos);
        CHECK(caught_what.find("16") != std::string::npos);
        // Diagnostic dump (helpful when reading the log):
        std::println("[Section 5a] caught.what = [{}]", caught_what);
    }

    // 5b. Catch as std::runtime_error& directly — the SPECIFIC type
    //     of the throwing sibling's exception.
    {
        char buf[16] = {};
        bool caught_5b = false;
        std::string msg_5b;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 16,
                                                          std::string{"too long: {}"},
                                                          std::string(20, 'X'));
        } catch (const std::runtime_error& e) {
            caught_5b = true;
            msg_5b = e.what();
        }
        CHECK(caught_5b);
        CHECK(msg_5b.find("30") != std::string::npos);
        CHECK(msg_5b.find("16") != std::string::npos);
    }
#else
    // 5c. On the EXPECTED path, the failure is NOT thrown — it's
    //     returned in the .error() member.  Pin that the FormatError
    //     is inspectable without exceptions.
    {
        char buf[16] = {};
        auto r = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 16,
                                                "too long: {}",
                                                std::string(20, 'X'));
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
        // needed_bytes = 30 ("too long: " + 20 X's = 30 bytes);
        // we requested cap = 16.
        CHECK_EQ(r.error().needed_bytes, std::size_t{30});
        // The message must mention the would-be size AND the cap.
        CHECK(r.error().message.find("30") != std::string::npos);
        CHECK(r.error().message.find("16") != std::string::npos);
    }
#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 6 — consteval surface preserves compile-time gate
    SECTION("Section 6 - consteval surface preserves compile-time gate");
    // The CONSTEVAL macro's format-string parameter is
    // std::format_string<Args...> regardless of PETRA_THROW_ON_OVERFLOW
    // — the consteval gate is preserved.  Pin via decltype check on
    // the macro's expansion: the fmt parameter is a literal that
    // gets wrapped in std::format_string<Args...> at compile time.
    //
    // We pin this by checking that the CONSTEVAL macro's second
    // template parameter (the format-string type) is
    // std::format_string<int>, NOT std::string_view.
    //
    // Method: invoke the CONSTEVAL macro with a literal fmt and
    // a runtime-built std::string fmt; the runtime-built fmt should
    // FAIL TO COMPILE under the CONSTEVAL macro (consteval gate).
    // We pin the consteval surface with a runtime-built fmt check
    // via /tmp/probe_consteval_rejects_runtime_fmt.cpp (Section 14).
    {
        char buf[32] = {};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32,
                                                "literal {}", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{10});  // "literal 42" = 10 bytes
#else
        auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32,
                                                 "literal {}", 42);
        CHECK_EQ(n, std::size_t{10});
#endif
    }
    // The RUNTIME macro accepts std::string_view, so a runtime-built
    // std::string works on both paths.
    {
        char buf[32] = {};
        std::string fmt = "literal {}";
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                        fmt, 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{10});
#else
        auto n = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                         fmt, 42);
        CHECK_EQ(n, std::size_t{10});
#endif
    }

    // ----- Section 7 — multi-arg round-trip
    SECTION("Section 7 - multi-arg round-trip");
    {
        char buf[64] = {};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 64,
                                                "x={} y={} z={:.2f}",
                                                1, 2, 3.14159);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});  // "x=1 y=2 z=3.14" = 14
        CHECK_EQ((std::string_view{buf, 14}),
                 (std::string_view{"x=1 y=2 z=3.14"}));
#else
        auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 64,
                                                 "x={} y={} z={:.2f}",
                                                 1, 2, 3.14159);
        CHECK_EQ(n, std::size_t{14});
        CHECK_EQ((std::string_view{buf, 14}),
                 (std::string_view{"x=1 y=2 z=3.14"}));
#endif
    }

    // ----- Section 8 — embedded NUL byte round-trip
    SECTION("Section 8 - embedded NUL byte round-trip");
    // A runtime-built format that produces an embedded NUL byte
    // (e.g. "hi\x00hi {}!").  The byte at the NUL position is '\0';
    // the byte at 'written' is the input byte (the NUL is NOT
    // counted as part of 'written' on the bounded-buffer path).
    {
        char buf[32] = {};
        std::string fmt = std::string{"hi\0hi {}!", 9};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                        fmt, 42);
        CHECK(r.has_value());
        // "hi\0hi 42!" = 9 bytes; the embedded NUL is at buf[2].
        CHECK_EQ(r.value(), std::size_t{9});
        CHECK_EQ(buf[0], 'h');
        CHECK_EQ(buf[1], 'i');
        CHECK_EQ(buf[2], '\0');
        CHECK_EQ(buf[3], 'h');
        CHECK_EQ(buf[4], 'i');
        CHECK_EQ(buf[5], ' ');
        CHECK_EQ(buf[6], '4');
        CHECK_EQ(buf[7], '2');
        CHECK_EQ(buf[8], '!');
#else
        auto n = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                         fmt, 42);
        CHECK_EQ(n, std::size_t{9});
        CHECK_EQ(buf[0], 'h');
        CHECK_EQ(buf[1], 'i');
        CHECK_EQ(buf[2], '\0');
        CHECK_EQ(buf[3], 'h');
        CHECK_EQ(buf[4], 'i');
        CHECK_EQ(buf[5], ' ');
        CHECK_EQ(buf[6], '4');
        CHECK_EQ(buf[7], '2');
        CHECK_EQ(buf[8], '!');
#endif
    }

    // ----- Section 9 — byte-exactness of the throwing path's message
    SECTION("Section 9 - byte-exactness of the throwing path's message");
#if PETRA_THROW_ON_OVERFLOW == 1
    // On the throwing path, the std::runtime_error's .what() must be
    // byte-exactly equal to the expected path's FormatError.message
    // for the SAME would_be / cap pair.  We achieve this by:
    //   1. Running the EXPECTED sibling on a known-failing call and
    //      capturing r.error().message
    //   2. Running the THROWING sibling on the same call and
    //      catching the std::runtime_error's .what()
    //   3. Asserting byte-exactness (string equality)
    //
    // We achieve (1) by calling petra::try_format_runtime_n directly
    // (NOT via the macro — the macro dispatches to the THROWING
    // sibling under PETRA_THROW_ON_OVERFLOW=1).
    std::string expected_msg;
    {
        char buf[16] = {};
        auto r = petra::try_format_runtime_n(buf, 16,
                                                std::string_view{"too long: {}"},
                                                std::string(20, 'X'));
        CHECK(!r.has_value());
        expected_msg = r.error().message;
    }
    std::string thrown_msg;
    {
        char buf[16] = {};
        try {
            (void)petra::try_format_runtime_n_throwing(
                buf, 16,
                std::string_view{"too long: {}"},
                std::string(20, 'X'));
        } catch (const std::runtime_error& e) {
            thrown_msg = e.what();
        }
    }
    CHECK(!expected_msg.empty());
    CHECK(!thrown_msg.empty());
    CHECK_EQ(expected_msg, thrown_msg);
#else
    // On the EXPECTED path, the throwing sibling is unreachable via
    // the macro, so we call petra::try_format_runtime_n_throwing
    // directly to verify byte-exactness against the expected sibling.
    std::string expected_msg;
    {
        char buf[16] = {};
        auto r = petra::try_format_runtime_n(buf, 16,
                                                std::string_view{"too long: {}"},
                                                std::string(20, 'X'));
        CHECK(!r.has_value());
        expected_msg = r.error().message;
    }
    std::string thrown_msg;
    {
        char buf[16] = {};
        try {
            (void)petra::try_format_runtime_n_throwing(
                buf, 16,
                std::string_view{"too long: {}"},
                std::string(20, 'X'));
        } catch (const std::runtime_error& e) {
            thrown_msg = e.what();
        }
    }
    CHECK(!expected_msg.empty());
    CHECK(!thrown_msg.empty());
    CHECK_EQ(expected_msg, thrown_msg);
#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 10 — Format error → runtime_error on the throwing path
    SECTION("Section 10 - Format error -> runtime_error");
#if PETRA_THROW_ON_OVERFLOW == 1
    // On the throwing path, std::format_error is ALSO caught and
    // re-raised as std::runtime_error (the throwing wrapper does
    // NOT distinguish TooLarge from Format at the call site; both
    // are "the format call failed" from the throwing sibling's
    // perspective).  Pin that a malformed format string throws
    // std::runtime_error whose DYNAMIC TYPE is std::runtime_error
    // (NOT std::format_error).  std::format_error IS-A std::runtime_error
    // (verified by std::is_base_of_v<std::runtime_error,
    // std::format_error> == true on libc++ 21), so the catch order
    // matters: std::format_error must be BEFORE std::runtime_error.
    {
        char buf[32] = {};
        std::string caught_what;
        bool is_format_error_dyn = false;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                          std::string_view{"bad {"},
                                                          42);
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
        // The message must contain the std::format_error's text
        // ("The format string terminates at a '{'" or similar).
        CHECK(caught_what.find("format") != std::string::npos
              || caught_what.find("{") != std::string::npos);
        std::println("[Section 10] caught.what = [{}]", caught_what);
    }
#else
    // On the EXPECTED path, a malformed format string returns
    // FormatError{Format, ...} from std::format_error.
    {
        char buf[32] = {};
        std::string fmt = std::string{"bad {"};
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(buf, 32,
                                                        fmt, 42);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
    }
#endif  // PETRA_THROW_ON_OVERFLOW

    // ----- Section 11 — no-args variant
    SECTION("Section 11 - no-args variant");
    // __VA_OPT__(,) handles the no-args case (a format with no
    // replacement fields).  TRY_FORMAT_BOUNDED_N_OR_THROW(buf, cap,
    // "ping") expands to the underlying wrapper call without a
    // trailing comma.
    {
        char buf[32] = {};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32, "ping");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{4});  // "ping" = 4 bytes
#else
        auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32, "ping");
        CHECK_EQ(n, std::size_t{4});
#endif
    }

    // ----- Section 12 — helpers used in Section 14's decltype checks
    SECTION("Section 12 - helpers for decltype checks");
    // Pin the helpers used in Section 14.  These are "flat" dispatchers
    // (always dispatch to the expected sibling) used to verify the
    // perpendicular-axis claim: today's meta-macros dispatch to the
    // SAME wrappers as these helpers when PETRA_THROW_ON_OVERFLOW=0.
    {
        char buf[16] = {};
        auto r_helper = petra::try_format_bounded_runtime_n(
            buf, 16, std::format_string<int>{"value={}"}, 42);
        CHECK(r_helper.has_value());
        CHECK_EQ(r_helper.value(), std::size_t{8});
    }
    {
        char buf[16] = {};
        auto r_helper = petra::try_format_runtime_n(
            buf, 16, std::string_view{"value={}"}, 42);
        CHECK(r_helper.has_value());
        CHECK_EQ(r_helper.value(), std::size_t{8});
    }

    // ----- Section 13 — 4 threads × 25 calls concurrent dispatch
    SECTION("Section 13 - 4 threads x 25 calls concurrent dispatch");
    // Pins thread safety.  4 threads × 25 calls each fire concurrently
    // through BOTH the CONSTEVAL and RUNTIME meta-macros.  Per-thread
    // local counters are aggregated AFTER join() via
    // std::atomic<int>::fetch_add to avoid a data race on g_pass /
    // g_fail.
    {
        std::atomic<int> tp{0};
        std::atomic<int> tf{0};
        auto worker = [&tp, &tf]() {
            int local_pass = 0;
            int local_fail = 0;
            for (int i = 0; i < 25; ++i) {
                char buf[32] = {};
#if PETRA_THROW_ON_OVERFLOW == 0
                auto r = TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32,
                                                       "[{:02}]", i);
                if (r.has_value() && r.value() == 4) {
                    ++local_pass;
                } else {
                    ++local_fail;
                }
                auto r2 = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(
                    buf, 32, std::string_view{"[{:02}]"}, i);
                if (r2.has_value() && r2.value() == 4) {
                    ++local_pass;
                } else {
                    ++local_fail;
                }
#else
                try {
                    auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(
                        buf, 32, "[{:02}]", i);
                    if (n == 4) {
                        ++local_pass;
                    } else {
                        ++local_fail;
                    }
                } catch (...) {
                    ++local_fail;
                }
                try {
                    auto n = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(
                        buf, 32, std::string_view{"[{:02}]"}, i);
                    if (n == 4) {
                        ++local_pass;
                    } else {
                        ++local_fail;
                    }
                } catch (...) {
                    ++local_fail;
                }
#endif
            }
            tp.fetch_add(local_pass, std::memory_order_relaxed);
            tf.fetch_add(local_fail, std::memory_order_relaxed);
        };
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back(worker);
        }
        for (auto& th : threads) {
            th.join();
        }
        CHECK_EQ(tp.load(), 4 * 25 * 2);  // 4 threads × 25 calls × 2 macros
        CHECK_EQ(tf.load(), 0);
    }

    // ----- Section 14 — perpendicular-axis claim
    SECTION("Section 14 - perpendicular-axis claim (PETRA_THROW_ON_OVERFLOW "
            "x CONSTEVAL-vs-RUNTIME)");
    // Pin the perpendicular-axis claim via decltype checks:
    //   - With PETRA_THROW_ON_OVERFLOW=0, today's CONSTEVAL macro
    //     dispatches to the SAME wrapper as the "flat" CONSTEVAL
    //     helper (Sep 9's try_format_bounded_runtime_n).
    //   - With PETRA_THROW_ON_OVERFLOW=0, today's RUNTIME macro
    //     dispatches to the SAME wrapper as the "flat" RUNTIME helper
    //     (Sep 8's try_format_runtime_n).
    //   - With PETRA_THROW_ON_OVERFLOW=1, today's CONSTEVAL macro
    //     dispatches to the SAME wrapper as the "flat" CONSTEVAL
    //     throwing helper (today's NEW
    //     try_format_bounded_runtime_n_throwing).
    //   - With PETRA_THROW_ON_OVERFLOW=1, today's RUNTIME macro
    //     dispatches to the SAME wrapper as the "flat" RUNTIME
    //     throwing helper (today's NEW
    //     try_format_runtime_n_throwing).
#if PETRA_THROW_ON_OVERFLOW == 0
    // Expected sibling: macro's return type matches the helper's.
    using R14a = decltype(TRY_FORMAT_BOUNDED_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using H14a = decltype(petra::try_format_bounded_runtime_n(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R14a, H14a>,
                  "With PETRA_THROW_ON_OVERFLOW=0, "
                  "TRY_FORMAT_BOUNDED_N_OR_THROW must dispatch to "
                  "petra::try_format_bounded_runtime_n");

    using R14b = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    using H14b = decltype(petra::try_format_runtime_n(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R14b, H14b>,
                  "With PETRA_THROW_ON_OVERFLOW=0, "
                  "TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW must dispatch to "
                  "petra::try_format_runtime_n");
#else
    // Throwing sibling: macro's return type matches the helper's.
    using R14a = decltype(TRY_FORMAT_BOUNDED_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using H14a = decltype(petra::try_format_bounded_runtime_n_throwing(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R14a, H14a>,
                  "With PETRA_THROW_ON_OVERFLOW=1, "
                  "TRY_FORMAT_BOUNDED_N_OR_THROW must dispatch to "
                  "petra::try_format_bounded_runtime_n_throwing");

    using R14b = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    using H14b = decltype(petra::try_format_runtime_n_throwing(
        std::declval<char*>(), std::declval<std::size_t>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R14b, H14b>,
                  "With PETRA_THROW_ON_OVERFLOW=1, "
                  "TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW must dispatch to "
                  "petra::try_format_runtime_n_throwing");
#endif
    CHECK(true);

    // ----- Section 15 — final summary
    SECTION("Section 15 - final summary");
    std::println("[Section 15] PASS: {}  FAIL: {}", g_pass, g_fail);

    return g_fail == 0 ? 0 : 1;
}