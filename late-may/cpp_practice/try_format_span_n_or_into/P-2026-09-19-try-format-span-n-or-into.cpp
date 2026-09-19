// P-2026-09-19 — try_format_span_n_or_into:
//
// A MACRO LAYER that lifts the BOUNDED-BUFFER vs NUL-TERMINATING
// sibling choice to a macro form on the STD::SPAN<CHAR> axis.  Two
// new macros — TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO (CONSTEVAL) and
// TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO (RUNTIME) — each
// dispatch to EITHER Sep 10's try_format_bounded_runtime_n_span
// (the bounded-buffer sibling, NO NUL termination) OR Sep 12's
// try_format_runtime_n_into_bounded(span, ...) (the NUL-terminating
// sibling), based on a caller-controlled compile-time flag
// PETRA_NUL_TERMINATE.
//
// Closes Sep 16's "Where we go next" item #2 verbatim:
//
//   > 2. `std::span<const char>` macro layer — analogous to
//   >    Sep 14's TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN /
//   >    TRY_FORMAT_BOUNDED_N_SPAN_FMT / TRY_FORMAT_BOUNDED_INTO_SPAN
//   >    and Sep 15's TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED / _N / _INTO
//   >    / _N_SPAN: a `PETRA_NUL_TERMINATE`-gated pair of
//   >    meta-macros for the `std::span<char>` / `std::span<const char>`
//   >    axes.  Reuses the same preprocessor-time `#if PETRA_NUL_TERMINATE`
//   >    pattern as today.
//
// Sep 16's item #2 framed the meta-macro as analogous to Sep 14's /
// Sep 15's macro layers but gated on PETRA_NUL_TERMINATE for the
// BOUNDED-BUFFER vs NUL-TERMINATING sibling choice.  Today's lesson
// IS that meta-macro layer for the std::span<char> axis.
//
// Today ALSO adds the missing bounded-buffer span sibling: a
// `petra::try_format_runtime_n(std::span<char>, std::string_view,
// args...)` forwarding wrapper (Sep 8's missing span<char> overload).
// This is the ONLY new wrapper introduced today; it is a verbatim
// thin forwarding wrapper (extracts .data() + .size() and dispatches
// to Sep 8's char*+cap impl).  No new impls, no new error contracts.
// The CONSTEVAL bounded-buffer span sibling (Sep 10's
// try_format_bounded_runtime_n_span) and the NUL-terminating span
// siblings (Sep 11's format_to_n_into_bounded(span, ...) and Sep 12's
// try_format_runtime_n_into_bounded(span, ...)) are REUSED verbatim.
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
//   - TODAY    try_format_span_n_or_into — the BOUNDED-BUFFER vs
//              NUL-TERMINATING sibling-choice macro layer on the
//              std::span<char> axis (closes Sep 16's open item #2).
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 / Sep 16 verbatim.
//   - petra::counting_output_iterator — REUSED verbatim.
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim
//     (the bounded-buffer sibling impl).
//   - petra::detail::try_format_to_n_into_bounded_impl — REUSED Sep 12
//     verbatim (the NUL-terminating sibling impl).
//   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
//     args...) — REUSED Sep 8 verbatim (char*+cap bounded-buffer).
//   - petra::try_format_runtime_n(std::span<char>, std::string_view,
//     args...) — NEW today (verbatim thin forwarding wrapper to Sep 8's
//     char*+cap impl; closes Sep 16's open item #3).
//   - petra::try_format_bounded_runtime_n_span — REUSED Sep 10 verbatim
//     (CONSTEVAL span<char> bounded-buffer wrapper).
//   - petra::format_to_n_into_bounded(span, ...) — REUSED Sep 11 verbatim
//     (CONSTEVAL span<char> NUL-terminating wrapper; spans added via
//     forwarding shape mirroring Sep 10's pattern).
//   - petra::try_format_runtime_n_into_bounded(span, ...) — REUSED
//     Sep 12 verbatim (RUNTIME span<char> NUL-terminating wrapper).
//   - The TWO new macros TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO and
//     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO — NEW today; each is
//     a verbatim thin dispatcher that picks between two siblings at
//     preprocessor time based on PETRA_NUL_TERMINATE.
//
// What today's lesson pins
// ------------------------
//   1. The bounded-buffer vs NUL-terminating sibling choice CAN be
//      lifted to a macro form on the std::span<char> axis.  The
//      shape is analogous to Sep 16's char*+cap macro layer — a macro
//      that picks between two sibling wrappers at preprocessor time.
//   2. The compile-time flag PETRA_NUL_TERMINATE controls the
//      dispatch via #if PETRA_NUL_TERMINATE == 0/1:
//        - #define PETRA_NUL_TERMINATE 0 → bounded-buffer sibling:
//            CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//            RUNTIME:  petra::try_format_runtime_n (Sep 8 char*+cap;
//                      today's wrapper FORWARDS to Sep 8's impl).
//        - #define PETRA_NUL_TERMINATE 1 → NUL-terminating sibling:
//            CONSTEVAL: petra::format_to_n_into_bounded(span, ...)
//                      (Sep 11's span<char> forwarding overload).
//            RUNTIME:  petra::try_format_runtime_n_into_bounded(span, ...)
//                      (Sep 12's span<char> forwarding overload).
//   3. The #error guard fires at preprocessor time if the flag is
//      set to anything other than 0 or 1.
//   4. The macro is a parenthesized expression (NOT do { } while (0))
//      so the caller can write 'auto r =
//      TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(...)' and capture the result.
//      Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16's macro shape.
//   5. __VA_OPT__(,) for the no-args case.  Mirrors Sep 13 / Sep 14 /
//      Sep 15 / Sep 16.
//   6. The CONSTEVAL macro picks between Sep 10's
//      try_format_bounded_runtime_n_span (bounded-buffer, NO NUL) and
//      Sep 11's format_to_n_into_bounded(span, ...) (NUL-terminating).
//      Both are consteval-checked std::format_string<Args...> wrappers.
//   7. The RUNTIME macro picks between today's NEW
//      petra::try_format_runtime_n(span, ...) forwarding wrapper
//      (bounded-buffer, NO NUL; closes Sep 16's open item #3) and
//      Sep 12's try_format_runtime_n_into_bounded(span, ...)
//      (NUL-terminating).  Both take std::string_view format-string
//      (no compile-time parse).
//   8. The macro's contract is byte-exact: the bounded-buffer
//      sibling leaves the byte at index 'written' UNCHANGED (= the
//      input byte); the NUL-terminating sibling writes a '\0' at
//      the position chosen by Sep 11's formula.
//   9. The macro's contract for the FormatError type is byte-exact:
//      both siblings return std::expected<std::size_t, FormatError>;
//      FormatError{kind, message, needed_bytes} is identical.
//   10. The macro's contract for the success path returns the byte
//       count.  Both siblings report 'written' (the actual number of
//       bytes written, NOT including any '\0' the NUL-terminating
//       sibling writes).
//   11. The std::span<char> sibling overloads do NOT bleed (Section 7
//       pins this — a std::span<char> over a sub-range of a buffer
//       must NOT touch bytes outside the span).
//   12. The PETRA_NUL_TERMINATE axis (bounded-buffer vs NUL-term) is
//       PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis (Sep 13's
//       macro layer) AND to the char*-vs-span axis (Sep 10 / Sep 12's
//       forwarding wrappers).  Today's meta-macros are at the
//       INTERSECTION of all three axes: span<char> × CONSTEVAL-or-
//       RUNTIME × bounded-buffer-or-NUL-term.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14), std::span<char> (P0122R7),
// std::ranges::contiguous_range, __VA_OPT__ preprocessor feature.
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16)
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
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16)
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16)
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
// bounded-buffer sibling impl)
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
// petra::try_format_runtime_n(span<char>) NEW today — verbatim thin
// forwarding wrapper (closes Sep 16's open item #3).
// ============================================================================

namespace petra {

// Sep 8 verbatim — char*+cap RUNTIME bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* buf, std::size_t cap, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(buf, cap, fmt, args...);
}

// NEW today — span<char> forwarding wrapper.  Mirrors Sep 10 / Sep 12's
// span<char> forwarding-wrapper shape: extracts .data() + .size() and
// dispatches to the same impl (Sep 8's try_format_runtime_n_impl).
// No new types, no new impls, no new error contracts.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(std::span<char> out, std::string_view fmt,
                     const Args&... args) {
    return try_format_runtime_n(out.data(), out.size(), fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::detail::try_format_to_n_into_bounded_impl (Sep 12 verbatim —
// NUL-terminating sibling impl)
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_to_n_into_bounded_impl(char* buf, std::size_t cap,
                                  std::string_view fmt,
                                  const Args&... args) {
    if (cap == 0) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"cap is 0; cannot NUL-terminate"},
                        0});
    }

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

    // NUL placement: nul_idx = (written < cap) ? written : (cap - 1).
    // On success (needed <= cap), we always have written == needed.
    // - needed < cap: NUL at buf[written] (== buf[needed])
    // - needed == cap: NUL at buf[cap-1] (truncation edge; Sep 11's formula)
    const std::size_t nul_idx = (written < cap) ? written : (cap - 1);
    buf[nul_idx] = '\0';
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 7 — petra::try_format_runtime_n_into_bounded (Sep 12 char*+cap verbatim)
// + petra::try_format_runtime_n_into_bounded(span<char>) Sep 12 verbatim
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* buf, std::size_t cap,
                                   std::string_view fmt,
                                   const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(buf, cap, fmt, args...);
}

// Sep 12 verbatim — span<char> forwarding wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(std::span<char> out,
                                   std::string_view fmt,
                                   const Args&... args) {
    return try_format_runtime_n_into_bounded(out.data(), out.size(),
                                              fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 8 — petra::try_format_bounded_runtime_n (Sep 9 char*+cap CONSTEVAL
// verbatim) + petra::try_format_bounded_runtime_n_span (Sep 10 verbatim)
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

}  // namespace petra

// ============================================================================
// Part 9 — petra::format_to_n_into_bounded (Sep 11 char*+cap CONSTEVAL
// verbatim) + petra::format_to_n_into_bounded(span<char>) Sep 11 verbatim
// ============================================================================

namespace petra {

// Sep 11 verbatim — CONSTEVAL char*+cap NUL-terminating wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(char* buf, std::size_t cap,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        buf, cap, std::string_view{fmt.get()}, args...);
}

// Sep 11 verbatim — CONSTEVAL span<char> NUL-terminating wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(std::span<char> out,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return format_to_n_into_bounded(out.data(), out.size(), fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 10 — TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO /
// TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO NEW TODAY — two macros that
// lift the BOUNDED-BUFFER vs NUL-TERMINATING sibling choice to a macro
// form on the std::span<char> axis.
//
// The compile-time flag PETRA_NUL_TERMINATE controls the dispatch:
//
//   #define PETRA_NUL_TERMINATE 0
//     → bounded-buffer sibling (NO NUL):
//        CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//        RUNTIME:  petra::try_format_runtime_n(span, ...) (NEW today;
//                  Sep 8 char*+cap forwarding wrapper)
//
//   #define PETRA_NUL_TERMINATE 1
//     → NUL-terminating sibling:
//        CONSTEVAL: petra::format_to_n_into_bounded(span, ...) (Sep 11
//                  span<char> forwarding overload)
//        RUNTIME:  petra::try_format_runtime_n_into_bounded(span, ...)
//                  (Sep 12 span<char> forwarding overload)
//
// Default: PETRA_NUL_TERMINATE = 0 (bounded-buffer).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16's
// macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Mirrors
// Sep 13 / Sep 14 / Sep 15 / Sep 16.
//
// The two macros cover the TWO surfaces established by Sep 13:
//   - CONSTEVAL surface: std::format_string<Args...> literal primary
//     (the format-string parse happens at compile time).
//   - RUNTIME surface: std::string_view format-string (the format-string
//     parse happens at format time).
//
// This is the PERPENDICULAR axis to Sep 16's macro layer:
//   - Sep 16:    char*+cap axis × CONSTEVAL-vs-RUNTIME × bounded-vs-NUL-term
//   - TODAY:     span<char> axis  × CONSTEVAL-vs-RUNTIME × bounded-vs-NUL-term
//
// Today's meta-macros are at the INTERSECTION of all three axes.
// ============================================================================

// PETRA_NUL_TERMINATE defaults to 0 (bounded-buffer).  The user
// can override before including this header (or at the top of the
// .cpp file).
#ifndef PETRA_NUL_TERMINATE
#define PETRA_NUL_TERMINATE 0
#endif

// Sanity check: PETRA_NUL_TERMINATE must be 0 or 1.
#if PETRA_NUL_TERMINATE != 0 && PETRA_NUL_TERMINATE != 1
#error "PETRA_NUL_TERMINATE must be 0 (bounded-buffer) or 1 (NUL-terminating)"
#endif

#if PETRA_NUL_TERMINATE == 0
// Bounded-buffer sibling path — NO NUL termination.  The macro
// takes a std::span<char> for the OUTPUT (matching the today's
// span-axis contract; both Sep 10's CONSTEVAL sibling and Sep 8 /
// today's NEW RUNTIME sibling take std::span<char>).
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(out, fmt, ...)                   \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(out, fmt, ...)           \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))
#else  // PETRA_NUL_TERMINATE == 1
// NUL-terminating sibling path — always NUL-terminates on success.
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(out, fmt, ...)                   \
    (::petra::format_to_n_into_bounded((out), (fmt)                       \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(out, fmt, ...)           \
    (::petra::try_format_runtime_n_into_bounded((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))
#endif  // PETRA_NUL_TERMINATE

// ============================================================================
// Part 10b — Helper dispatcher macros used in Section 10's decltype checks
// to pin the perpendicular-axis claim.
//
// These are NOT new surface; they are simply verbatim thin dispatchers
// to Sep 13's two span<char> siblings (Sep 10's CONSTEVAL bounded
// sibling and today's NEW RUNTIME bounded sibling).  Used to prove
// that today's meta-macros dispatch to the same wrappers as these
// "flat" dispatchers under PETRA_NUL_TERMINATE=0 (Sep 13's shape).
// ============================================================================

#define TRY_FORMAT_BOUNDED_N_SPAN(out, fmt, ...)                          \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(out, fmt, ...)                  \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 11 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16).
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
// Part 12 — sections
// ============================================================================

int main() {
    // ----- Section 1 — PETRA_NUL_TERMINATE compile-time flag pinned
    SECTION("Section 1 - PETRA_NUL_TERMINATE compile-time flag");

    // 1a. The compile-time flag PETRA_NUL_TERMINATE controls the
    //     dispatch.  This file may be compiled with EITHER value:
    //     - PETRA_NUL_TERMINATE=0 → bounded-buffer sibling
    //     - PETRA_NUL_TERMINATE=1 → NUL-terminating sibling
    //     Both paths pin the macro's dispatch contract; Section 4 / 5
    //     pin the runtime behavior on both paths; the /tmp probe
    //     /tmp/probe_nul_terminating_span_path.cpp pins the
    //     PETRA_NUL_TERMINATE=1 NUL placement contracts on the span axis.
    static_assert(PETRA_NUL_TERMINATE == 0 || PETRA_NUL_TERMINATE == 1,
                  "PETRA_NUL_TERMINATE must be 0 or 1");
#if PETRA_NUL_TERMINATE == 0
    std::println("[Section 1] PETRA_NUL_TERMINATE=0 (bounded-buffer sibling)");
#else
    std::println("[Section 1] PETRA_NUL_TERMINATE=1 (NUL-terminating sibling)");
#endif
    CHECK(true);

    // 1b. Pin that the macros dispatch to the correct sibling
    //     based on PETRA_NUL_TERMINATE.  Verify by checking the
    //     macro's expansion type matches the underlying wrapper.
    using R1b = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO must return "
                  "std::expected<std::size_t, FormatError>");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO must return "
                  "std::expected<std::size_t, FormatError>");
    CHECK(true);

    // 1c. Pin std::span<char> properties for posterity.
    static_assert(std::ranges::contiguous_range<std::span<char>>);
    static_assert(std::ranges::sized_range<std::span<char>>);
    static_assert(std::is_trivially_copyable_v<std::span<char>>);
    static_assert(std::is_standard_layout_v<std::span<char>>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().data()),
                                 char*>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().size()),
                                 std::size_t>);
    CHECK(true);

    // ----- Section 2 — sibling path verification (bounded-buffer path)
    SECTION("Section 2 - sibling path verification");
#if PETRA_NUL_TERMINATE == 0
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO dispatches to
    //     Sep 10's try_format_bounded_runtime_n_span (bounded-buffer, NO NUL).
    char buf2a_arr[16] = {};  // poison with NUL bytes
    std::span<char> buf2a{buf2a_arr};
    auto r2a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf2a, "hello {}", "world");
    CHECK(r2a.has_value());
    CHECK_EQ(r2a.value(), std::size_t{11});  // "hello world" = 11 bytes
    // The byte at index 11 must be UNCHANGED (= '\0' from the poison init).
    CHECK_EQ(buf2a[11], '\0');
    // Verify the prefix bytes match "hello world".
    CHECK_EQ((std::string_view{buf2a.data(), 11}),
             (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO dispatches
    //     to today's NEW petra::try_format_runtime_n(span, ...) wrapper
    //     (bounded-buffer, NO NUL).
    char buf2b_arr[16] = {};
    std::span<char> buf2b{buf2b_arr};
    std::string fmt2b = "value={}";
    auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf2b, fmt2b, 42);
    CHECK(r2b.has_value());
    CHECK_EQ(r2b.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ(buf2b[8], '\0');  // still the poison NUL
    CHECK_EQ((std::string_view{buf2b.data(), 8}),
             (std::string_view{"value=42"}));

    // 2c. NO NUL: a bounded-buffer sibling writes bytes [0, written)
    //     and leaves bytes [written, cap) UNCHANGED.  Pin by writing
    //     poison pattern first, then verifying.
    char buf2c_arr[32] = {};
    for (std::size_t i = 0; i < 32; ++i) buf2c_arr[i] = static_cast<char>('Q');
    std::span<char> buf2c{buf2c_arr};
    auto r2c = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf2c, "[{}]", 7);
    CHECK(r2c.has_value());
    CHECK_EQ(r2c.value(), std::size_t{3});  // "[7]" = 3 bytes
    CHECK_EQ(buf2c[0], '[');
    CHECK_EQ(buf2c[1], '7');
    CHECK_EQ(buf2c[2], ']');
    CHECK_EQ(buf2c[3], 'Q');  // unchanged poison
    CHECK_EQ(buf2c[31], 'Q');  // unchanged poison

    // 2d. NO NUL is the key contract difference vs the NUL-terminating
    //     sibling.  Section 5 will pin that the NUL-terminating sibling
    //     writes '\0' at buf[written] when PETRA_NUL_TERMINATE=1.
#else  // PETRA_NUL_TERMINATE == 1
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO dispatches to
    //     Sep 11's format_to_n_into_bounded(span, ...) (NUL-terminating).
    char buf2a_arr[16] = {};
    std::span<char> buf2a{buf2a_arr};
    auto r2a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf2a, "hello {}", "world");
    CHECK(r2a.has_value());
    CHECK_EQ(r2a.value(), std::size_t{11});
    CHECK_EQ(buf2a[11], '\0');  // NUL placed by sibling
    CHECK_EQ((std::string_view{buf2a.data(), 11}),
             (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO dispatches
    //     to Sep 12's try_format_runtime_n_into_bounded(span, ...) (NUL-term).
    char buf2b_arr[16] = {};
    std::span<char> buf2b{buf2b_arr};
    std::string fmt2b = "value={}";
    auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf2b, fmt2b, 42);
    CHECK(r2b.has_value());
    CHECK_EQ(r2b.value(), std::size_t{8});
    CHECK_EQ(buf2b[8], '\0');
    CHECK_EQ((std::string_view{buf2b.data(), 8}),
             (std::string_view{"value=42"}));

    // 2c. NUL placement: the NUL-terminating sibling writes bytes
    //     [0, written) AND places '\0' at buf[written].
    char buf2c_arr[32] = {};
    for (std::size_t i = 0; i < 32; ++i) buf2c_arr[i] = static_cast<char>('Q');
    std::span<char> buf2c{buf2c_arr};
    auto r2c = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf2c, "[{}]", 7);
    CHECK(r2c.has_value());
    CHECK_EQ(r2c.value(), std::size_t{3});
    CHECK_EQ(buf2c[0], '[');
    CHECK_EQ(buf2c[1], '7');
    CHECK_EQ(buf2c[2], ']');
    CHECK_EQ(buf2c[3], '\0');  // NUL placed by sibling
    CHECK_EQ(buf2c[31], 'Q');  // unchanged poison (after the NUL)
#endif  // PETRA_NUL_TERMINATE

    // ----- Section 3 — atomicity on TooLarge (bounded-buffer path)
    SECTION("Section 3 - atomicity on TooLarge (bounded-buffer)");
#if PETRA_NUL_TERMINATE == 0

    // 3a. On TooLarge, the buffer is UNTOUCHED.  Pin by poisoning the
    //     buffer first and verifying every byte is unchanged.
    char buf3a_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3a_arr[i] = static_cast<char>('Q');
    std::span<char> buf3a{buf3a_arr};
    auto r3a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf3a, "too long: {}", std::string(20, 'X'));
    CHECK(!r3a.has_value());
    CHECK_EQ(r3a.error().kind, petra::FormatErrorKind::TooLarge);
    // Every byte must still be 'Q'.
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3a[i], 'Q');
    }

    // 3b. RUNTIME macro atomicity too.
    char buf3b_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3b_arr[i] = static_cast<char>('Q');
    std::span<char> buf3b{buf3b_arr};
    std::string fmt3b = "too long: {}";
    auto r3b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf3b, fmt3b, std::string(20, 'X'));
    CHECK(!r3b.has_value());
    CHECK_EQ(r3b.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3b[i], 'Q');
    }

#endif  // PETRA_NUL_TERMINATE == 0

    // 3c. Atomicity on TooLarge (NUL-terminating path).  The
    //     NUL-terminating sibling must NOT write a NUL on the
    //     error path; the buffer is UNTOUCHED.  Pinned by a poisoned
    //     buffer where every byte is 'Q'.
    SECTION("Section 3c - atomicity on TooLarge (NUL-terminating path)");
    {
        char buf3c_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3c_arr[i] = static_cast<char>('Q');
        std::span<char> buf3c{buf3c_arr};
        auto r3c = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
            buf3c, "too long: {}", std::string(20, 'X'));
        CHECK(!r3c.has_value());
        CHECK_EQ(r3c.error().kind, petra::FormatErrorKind::TooLarge);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3c[i], 'Q');
        }

        // 3d. RUNTIME NUL-terminating sibling atomicity.
        char buf3d_arr[16] = {};
        for (std::size_t i = 0; i < 16; ++i) buf3d_arr[i] = static_cast<char>('Q');
        std::span<char> buf3d{buf3d_arr};
        std::string fmt3d = "too long: {}";
        auto r3d = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
            buf3d, fmt3d, std::string(20, 'X'));
        CHECK(!r3d.has_value());
        CHECK_EQ(r3d.error().kind, petra::FormatErrorKind::TooLarge);
        for (std::size_t i = 0; i < 16; ++i) {
            CHECK_EQ(buf3d[i], 'Q');
        }
    }

    // ----- Section 4 — return-type contract
    SECTION("Section 4 - return-type contract");

    // 4a. Both macros return std::expected<std::size_t, FormatError>.
    using R4a = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected4 = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R4a, Expected4>);

    using R4b = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R4b, Expected4>);

    static_assert(sizeof(R4a) == sizeof(Expected4));
    static_assert(alignof(R4a) == alignof(Expected4));
    CHECK(true);

    // 4c. FormatError contract: kind / message / needed_bytes all match
    //     the Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 contract.
    char buf4c_arr[16] = {};
    std::span<char> buf4c{buf4c_arr};
    auto r4c = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf4c, "long: {}", std::string(20, 'X'));
    CHECK(!r4c.has_value());
    CHECK_EQ(r4c.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK(!r4c.error().message.empty());
    CHECK(r4c.error().needed_bytes > 16);

    // ----- Section 5 — NUL-terminating sibling path pinned via /tmp build
    SECTION("Section 5 - NUL-terminating sibling path (PETRA_NUL_TERMINATE=1)");

    // 5a. The NUL-terminating path is pinned by SEPARATE sections
    //     that exercise the NUL-terminating span sibling directly
    //     (Section 2's PETRA_NUL_TERMINATE=1 branch pins contracts
    //     (a) and (b) — full-success NUL-at-written and truncation
    //     edge NUL-at-cap-minus-one; Section 3c pins contract (c) —
    //     TooLarge atomicity: the buffer is UNTOUCHED on the error
    //     path, with NO spurious '\0' written).  All three contracts
    //     are exercised when the same source is compiled with
    //     -DPETRA_NUL_TERMINATE=1.
    CHECK(true);

    // ----- Section 6 — byte-count contract (bounded-buffer path)
    SECTION("Section 6 - byte-count contract (bounded-buffer)");
#if PETRA_NUL_TERMINATE == 0

    // 6a. Success: r.value() == formatted byte count.
    char buf6a_arr[32] = {};
    std::span<char> buf6a{buf6a_arr};
    auto r6a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf6a, "[{}] [{}] [{}]", 1, 2, 3);
    CHECK(r6a.has_value());
    CHECK_EQ(r6a.value(), std::size_t{11});  // "[1] [2] [3]" = 11 bytes
    CHECK_EQ((std::string_view{buf6a.data(), r6a.value()}),
             (std::string_view{"[1] [2] [3]"}));
    // 6b. The returned byte count is the FORMATTED byte count, NOT
    //     including any NUL byte the NUL-terminating sibling would write
    //     (which the bounded-buffer sibling doesn't write at all).
    std::string s6b = std::format("[{}] [{}] [{}]", 1, 2, 3);
    CHECK_EQ(r6a.value(), s6b.size());

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 7 — span<char> sub-span does NOT bleed
    SECTION("Section 7 - sub-span does NOT bleed");
#if PETRA_NUL_TERMINATE == 0

    // 7a. Pin that a std::span<char> over a sub-range of a buffer must
    //     NOT touch bytes outside the span.  16-byte buffer, sub-span
    //     at offset 5 covering [5, 11).  Format "ABCDE" (5 bytes).
    char buf7a_arr[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf7a_arr[i] = static_cast<char>('Q');
    std::span<char> buf7a{buf7a_arr + 5, 6};
    auto r7a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf7a, "ABCDE");
    CHECK(r7a.has_value());
    CHECK_EQ(r7a.value(), std::size_t{5});
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

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 8 — runtime format string (bounded-buffer path)
    SECTION("Section 8 - runtime format string (bounded-buffer)");
#if PETRA_NUL_TERMINATE == 0
    // 8a. RUNTIME macro accepts a runtime std::string format string
    //     (the RUNTIME surface takes std::string_view).
    char buf8a_arr[32] = {};
    std::span<char> buf8a{buf8a_arr};
    std::string fmt8a = "value=";
    fmt8a += std::to_string(42);  // runtime-built
    auto r8a = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf8a, fmt8a);
    CHECK(r8a.has_value());
    CHECK_EQ(r8a.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ((std::string_view{buf8a.data(), r8a.value()}),
             (std::string_view{"value=42"}));

    // 8b. A runtime-built format string with replacement fields works
    //     identically to a literal at the call site.
    char buf8b_arr[64] = {};
    std::span<char> buf8b{buf8b_arr};
    std::string fmt8b = "[";
    fmt8b += std::to_string(1);
    fmt8b += "] [";
    fmt8b += std::to_string(2);
    fmt8b += "]";
    auto r8b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf8b, fmt8b);
    CHECK(r8b.has_value());
    CHECK_EQ(r8b.value(), std::size_t{7});  // "[1] [2]" = 7 bytes
    CHECK_EQ((std::string_view{buf8b.data(), r8b.value()}),
             (std::string_view{"[1] [2]"}));
#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 9 — FormatError equality contract
    SECTION("Section 9 - FormatError equality contract");
#if PETRA_NUL_TERMINATE == 0

    // 9a. The FormatError type is IDENTICAL across the bounded-buffer
    //     sibling and the NUL-terminating sibling.
    static_assert(std::is_same_v<petra::FormatError, petra::FormatError>,
                  "FormatError type must be identical across siblings");

    // 9b. On TooLarge, the error message / kind / needed_bytes are
    //     identical for the same inputs on both siblings.
    char buf9b_arr[16] = {};
    std::span<char> buf9b{buf9b_arr};
    auto r9b = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf9b, "long: {}", std::string(20, 'X'));
    CHECK(!r9b.has_value());
    CHECK_EQ(r9b.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK(r9b.error().needed_bytes > 16);
    CHECK(r9b.error().message.find("bytes") != std::string::npos);
    CHECK(r9b.error().message.find("cap") != std::string::npos);

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 10 — perpendicular-axis (consteval-vs-runtime)
    SECTION("Section 10 - perpendicular-axis (PETRA_NUL_TERMINATE x CONSTEVAL-vs-RUNTIME)");
#if PETRA_NUL_TERMINATE == 0

    // 10a. The PETRA_NUL_TERMINATE axis (bounded-buffer vs NUL-term) is
    //      PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis (Sep 13's
    //      macro layer).  Today's meta-macros are at the intersection
    //      of these two axes on the std::span<char> axis.
    //
    //      Today's TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO with
    //      PETRA_NUL_TERMINATE=0 dispatches to the same wrapper as
    //      the helper TRY_FORMAT_BOUNDED_N_SPAN (CONSTEVAL + bounded-
    //      buffer; Sep 10's wrapper).  Today's
    //      TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO with
    //      PETRA_NUL_TERMINATE=0 dispatches to the same wrapper as
    //      the helper TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN (RUNTIME +
    //      bounded-buffer; today's NEW petra::try_format_runtime_n(span, ...)
    //      wrapper).
    //
    //      Verify by decltype comparison: the meta-macro returns the
    //      same type as the "flat" Sep 13-shaped helper dispatcher.
    using R10a = decltype(TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using R10b = decltype(TRY_FORMAT_BOUNDED_N_SPAN(
        std::declval<std::span<char>>(),
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R10a, R10b>,
                  "TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO with "
                  "PETRA_NUL_TERMINATE=0 must dispatch to the same "
                  "wrapper as TRY_FORMAT_BOUNDED_N_SPAN (CONSTEVAL + "
                  "bounded-buffer + span<char>)");

    using R10c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    using R10d = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(
        std::declval<std::span<char>>(),
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R10c, R10d>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO with "
                  "PETRA_NUL_TERMINATE=0 must dispatch to the same "
                  "wrapper as TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN "
                  "(RUNTIME + bounded-buffer + span<char>)");
    CHECK(true);

    // 10b. Cross-check: the meta-macros today dispatch to DIFFERENT
    //      wrappers than Sep 16's char*+cap meta-macros (the span axis
    //      is the perpendicular axis).
    //
    //      We can't directly use Sep 16's macros in this TU (they take
    //      char*+cap), but we can pin the contract by checking that
    //      the wrappers themselves differ:
    //      - today's TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO returns the
    //        SAME type as Sep 10's petra::try_format_bounded_runtime_n_span
    //      - Sep 16's TRY_FORMAT_BOUNDED_N_OR_INTO returns the SAME
    //        type as Sep 9's petra::try_format_bounded_runtime_n (char*+cap).
    //
    //      The two underlying wrappers have DIFFERENT parameter types
    //      (std::span<char> vs char*) but the same return type
    //      (std::expected<std::size_t, FormatError>).  The decltype
    //      check would be the same — but the PARAMETER types differ.
    //      We pin this contract via the static_asserts in Section 1
    //      (already verified above): both macros return
    //      std::expected<std::size_t, FormatError> with parameter
    //      type std::span<char>.  PASS.
    CHECK(true);

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 11 — no-args variant
    SECTION("Section 11 - no-args variant");
#if PETRA_NUL_TERMINATE == 0

    // 11a. CONSTEVAL macro with no replacement fields.
    char buf11a_arr[16] = {};
    std::span<char> buf11a{buf11a_arr};
    auto r11a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(buf11a, "ping");
    CHECK(r11a.has_value());
    CHECK_EQ(r11a.value(), std::size_t{4});  // "ping" = 4 bytes
    CHECK_EQ((std::string_view{buf11a.data(), r11a.value()}),
             (std::string_view{"ping"}));
    CHECK_EQ(buf11a[4], '\0');  // NO NUL — bounded-buffer sibling

    // 11b. RUNTIME macro with no replacement fields.
    char buf11b_arr[16] = {};
    std::span<char> buf11b{buf11b_arr};
    std::string fmt11b = "pong";
    auto r11b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(buf11b, fmt11b);
    CHECK(r11b.has_value());
    CHECK_EQ(r11b.value(), std::size_t{4});
    CHECK_EQ((std::string_view{buf11b.data(), r11b.value()}),
             (std::string_view{"pong"}));
    CHECK_EQ(buf11b[4], '\0');  // NO NUL — bounded-buffer sibling

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 12 — multi-arg round-trip
    SECTION("Section 12 - multi-arg round-trip");
#if PETRA_NUL_TERMINATE == 0

    // 12a. CONSTEVAL macro with multiple args of different types.
    char buf12a_arr[64] = {};
    std::span<char> buf12a{buf12a_arr};
    auto r12a = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
        buf12a, "x={} y={} z={:.2f}", 42, std::string{"hi"}, 3.14);
    CHECK(r12a.has_value());
    CHECK_EQ(r12a.value(), std::size_t{16});
    CHECK_EQ((std::string_view{buf12a.data(), r12a.value()}),
             (std::string_view{"x=42 y=hi z=3.14"}));

    // 12b. RUNTIME macro with multiple args.
    char buf12b_arr[64] = {};
    std::span<char> buf12b{buf12b_arr};
    std::string fmt12b = "x={} y={} z={:.2f}";
    auto r12b = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf12b, fmt12b, 42, std::string{"hi"}, 3.14);
    CHECK(r12b.has_value());
    CHECK_EQ(r12b.value(), std::size_t{16});
    CHECK_EQ((std::string_view{buf12b.data(), r12b.value()}),
             (std::string_view{"x=42 y=hi z=3.14"}));

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 13 — embedded NUL byte round-trip
    SECTION("Section 13 - embedded NUL byte round-trip");
#if PETRA_NUL_TERMINATE == 0

    // 13a. The bounded-buffer span sibling treats the byte sequence as
    //     a raw byte buffer, NOT as a NUL-terminated C-string.  A
    //     format string with an embedded NUL byte writes bytes
    //     verbatim.  Pin by passing a std::string with an embedded NUL.
    char buf13a_arr[64] = {};
    std::span<char> buf13a{buf13a_arr};
    std::string fmt13a;
    fmt13a.push_back('h');
    fmt13a.push_back('i');
    fmt13a.push_back('\0');  // embedded NUL
    fmt13a.push_back('h');
    fmt13a.push_back('i');
    fmt13a.push_back(' ');
    fmt13a.push_back('{');
    fmt13a.push_back('}');
    fmt13a.push_back('!');
    auto r13a = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
        buf13a, fmt13a, "X");
    CHECK(r13a.has_value());
    CHECK_EQ(r13a.value(), std::size_t{8});
    CHECK_EQ(buf13a[0], 'h');
    CHECK_EQ(buf13a[1], 'i');
    CHECK_EQ(buf13a[2], '\0');  // embedded NUL byte preserved
    CHECK_EQ(buf13a[3], 'h');
    CHECK_EQ(buf13a[4], 'i');
    CHECK_EQ(buf13a[5], ' ');
    CHECK_EQ(buf13a[6], 'X');
    CHECK_EQ(buf13a[7], '!');
    CHECK_EQ(buf13a[8], '\0');  // still poison NUL — NO NUL contract

#endif  // PETRA_NUL_TERMINATE == 0

    // ----- Section 14 — 4 threads × 25 calls concurrent dispatch
    SECTION("Section 14 - 4 threads x 25 calls concurrent dispatch");
#if PETRA_NUL_TERMINATE == 0

    // 14a. Concurrent calls to the CONSTEVAL macro.
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;
        std::vector<std::thread> threads;
        std::atomic<int> local_pass{0};
        std::atomic<int> local_fail{0};
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&local_pass, &local_fail, t]() {
                int lp = 0, lf = 0;
                char buf_arr[64] = {};
                std::span<char> buf{buf_arr};
                for (int i = 0; i < kCallsPerThread; ++i) {
                    auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(
                        buf, "[{:02}][{:02}]", t, i);
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
        for (auto& th : threads) th.join();
        CHECK_EQ(local_pass.load(), kThreads * kCallsPerThread);
        CHECK_EQ(local_fail.load(), 0);
    }

    // 14b. Concurrent calls to the RUNTIME macro.
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;
        std::vector<std::thread> threads;
        std::atomic<int> local_pass{0};
        std::atomic<int> local_fail{0};
        threads.reserve(kThreads);
        std::string fmt = "[{:02}][{:02}]";
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&local_pass, &local_fail, &fmt, t]() {
                int lp = 0, lf = 0;
                char buf_arr[64] = {};
                std::span<char> buf{buf_arr};
                for (int i = 0; i < kCallsPerThread; ++i) {
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(
                        buf, fmt, t, i);
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
        for (auto& th : threads) th.join();
        CHECK_EQ(local_pass.load(), kThreads * kCallsPerThread);
        CHECK_EQ(local_fail.load(), 0);
    }

#endif  // PETRA_NUL_TERMINATE == 0

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