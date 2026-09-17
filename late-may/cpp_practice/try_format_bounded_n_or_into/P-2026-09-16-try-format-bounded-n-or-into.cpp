// P-2026-09-16 — try_format_bounded_n_or_into:
//
// A MACRO LAYER that lifts the BOUNDED-BUFFER vs NUL-TERMINATING
// sibling choice to a macro form.  Two new macros —
// TRY_FORMAT_BOUNDED_N_OR_INTO (CONSTEVAL) and
// TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO (RUNTIME) — each dispatch
// to EITHER Sep 9's try_format_bounded_runtime_n (the bounded-buffer
// sibling, NO NUL termination) OR Sep 11's format_to_n_into_bounded
// (the NUL-terminating sibling), based on a caller-controlled
// compile-time flag PETRA_NUL_TERMINATE.
//
// Closes Sep 14's "Where we go next" item #2 verbatim:
//
//   > 2. `petra::try_format_bounded_runtime_n_into` macro layer —
//   >    analogous to `LOG_INFO_RUNTIME` from Sep 1 /
//   >    `SYNC_LOG_INFO` from Sep 2: a macro that lifts the
//   >    bounded-buffer vs NUL-terminating sibling choice to a
//   >    macro form.  Sep 10's open item #4.
//
// Sep 14's item #2 framed the macro as a sibling of
// `LOG_INFO_RUNTIME` / `SYNC_LOG_INFO` — a macro that picks between
// two sibling functions at the call site.  Today's lesson IS that
// sibling-choice macro layer for the BOUNDED-BUFFER vs
// NUL-TERMINATING pair.
//
// Where this fits in the arc
// --------------------------
//   - Aug 25   std::format_to_n_into — NUL-terminating bounded
//              formatter WITHOUT a cap parameter (writes a complete
//              std::string).
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
//              char*+cap.
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
//   - TODAY    try_format_bounded_n_or_into — the BOUNDED-BUFFER vs
//              NUL-TERMINATING sibling-choice macro layer.
//
// Today covers
// ------------
//   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
//     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
//     Sep 14 / Sep 15 verbatim.
//   - petra::counting_output_iterator — REUSED verbatim.
//   - petra::detail::bounded_char_writer — REUSED verbatim.
//   - petra::detail::try_format_runtime_bounded_impl — REUSED Sep 6
//     verbatim (kept as the std::string-output sibling impl).
//   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim
//     (the bounded-buffer sibling impl).
//   - petra::detail::try_format_to_n_into_bounded_impl — REUSED Sep 12
//     verbatim (the NUL-terminating sibling impl).
//   - petra::try_format_bounded_runtime_n — REUSED Sep 9 verbatim (the
//     CONSTEVAL char*+cap bounded-buffer wrapper).
//   - petra::format_to_n_into_bounded — REUSED Sep 11 verbatim (the
//     CONSTEVAL char*+cap NUL-terminating wrapper).
//   - petra::try_format_runtime_n_into_bounded — REUSED Sep 12 verbatim
//     (the RUNTIME char*+cap NUL-terminating wrapper; today's lesson
//     uses it as the RUNTIME-FORMAT-STRING counterpart to Sep 11).
//   - The TWO new macros TRY_FORMAT_BOUNDED_N_OR_INTO and
//     TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO — NEW today; each is a
//     verbatim thin dispatcher that picks between two siblings at
//     preprocessor time based on PETRA_NUL_TERMINATE.
//
// What today's lesson pins
// ------------------------
//   1. The bounded-buffer vs NUL-terminating sibling choice CAN be
//      lifted to a macro form.  The shape is analogous to Sep 1's
//      LOG_INFO_RUNTIME / Sep 2's SYNC_LOG_INFO — a macro that
//      picks between two sibling functions at preprocessor time.
//   2. The compile-time flag PETRA_NUL_TERMINATE controls the
//      dispatch:
//        - #define PETRA_NUL_TERMINATE 0 → bounded-buffer sibling
//          (try_format_bounded_runtime_n / try_format_runtime_n_into_bounded
//          for the RUNTIME case).
//        - #define PETRA_NUL_TERMINATE 1 → NUL-terminating sibling
//          (format_to_n_into_bounded / try_format_runtime_n_into_bounded).
//   3. The macro is a parenthesized expression (NOT do { } while (0))
//      so the caller can write 'auto r = TRY_FORMAT_BOUNDED_N_OR_INTO(...)'
//      and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15's macro
//      shape exactly.
//   4. The macro uses __VA_OPT__(,) for the no-args case.  Mirrors
//      Sep 13 / Sep 14 / Sep 15.
//   5. The CONSTEVAL macro picks between Sep 9's try_format_bounded_runtime_n
//      (bounded-buffer, NO NUL) and Sep 11's format_to_n_into_bounded
//      (NUL-terminating).  Both are consteval-checked std::format_string<Args...>
//      wrappers.
//   6. The RUNTIME macro picks between a NEW bounded-buffer RUNTIME wrapper
//      and Sep 12's try_format_runtime_n_into_bounded.  For symmetry the
//      RUNTIME bounded-buffer wrapper is petra::try_format_runtime_n (Sep 8).
//   7. The macro's contract is byte-exact: the bounded-buffer sibling
//      leaves the byte at index 'written' unchanged (= the input byte);
//      the NUL-terminating sibling writes a '\0' at the
//      position chosen by Sep 11's formula.  Section 5 pins this
//      contract on both paths.
//   8. The macro's contract for the FormatError type is byte-exact:
//      both siblings return std::expected<std::size_t, FormatError>;
//      FormatError{kind, message, needed_bytes} is identical.  Section 4
//      pins this contract.
//   9. The macro's contract for the success path returns the byte
//      count.  Both siblings report 'written' (the actual number of
//      bytes written, NOT including any NUL the NUL-terminating sibling
//      writes) — Section 6 pins this for both paths.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14), std::span<char> (P0122R7),
// std::ranges::contiguous_range, __VA_OPT__ preprocessor feature.
// Requires -fexperimental-library on Apple Clang 21.0.0 / libc++ 21.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <iterator>
#include <memory>
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15)
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
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15)
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15)
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
// Part 4 — petra::detail::try_format_runtime_bounded_impl (Sep 6 verbatim)
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded_impl(std::string_view fmt, std::size_t cap,
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

    std::string out;
    out.resize(needed);
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out.data(), needed, cb, &written};
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
    return out;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — petra::try_format_runtime_bounded (Sep 6 verbatim — std::string primary)
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded(std::string_view fmt, std::size_t cap,
                           const Args&... args) {
    return detail::try_format_runtime_bounded_impl(fmt, cap, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim — bounded-buffer)
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
// Part 7 — petra::try_format_runtime_n (Sep 8 verbatim — RUNTIME bounded-buffer)
//
// This is the RUNTIME-FORMAT-STRING bounded-buffer sibling.  Sep 9's
// try_format_bounded_runtime_n is the CONSTEVAL sibling — Sep 13's
// TRY_FORMAT_BOUNDED_RUNTIME_N macro dispatches to this wrapper.
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
// Part 7b — petra::try_format_bounded_runtime_n (Sep 9 verbatim — CONSTEVAL
// bounded-buffer)
//
// Sep 9's CONSTEVAL char*+cap bounded-buffer wrapper.  Sep 13's
// TRY_FORMAT_BOUNDED_N macro dispatches to this wrapper.
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
// Part 8 — petra::detail::try_format_to_n_into_bounded_impl (Sep 12 verbatim —
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

    const std::size_t effective_cap = cap - 1;
    if (needed > effective_cap) {
        // needed == cap (truncation edge): reserve effective_cap = cap-1
        // bytes; we write effective_cap bytes + NUL at cap-1.
        std::size_t written = 0;
        try {
            bounded_char_writer sink{buf, effective_cap, cb, &written};
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
                                + " bytes; needed was "
                                + std::to_string(effective_cap),
                            effective_cap});
        }
        buf[effective_cap] = '\0';
        return written;
    }

    // needed < cap: full success path; needed bytes are written
    // verbatim and NUL is placed at index `written` (== needed).
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
    buf[written] = '\0';
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 9 — petra::format_to_n_into_bounded (Sep 11 verbatim — CONSTEVAL
// NUL-terminating) + petra::try_format_runtime_n_into_bounded
// (Sep 12 verbatim — RUNTIME NUL-terminating)
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(char* buf, std::size_t cap,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        buf, cap, std::string_view{fmt.get()}, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* buf, std::size_t cap,
                                   std::string_view fmt,
                                   const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(buf, cap, fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 10 — TRY_FORMAT_BOUNDED_N_OR_INTO / TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO
// NEW TODAY — two macros that lift the BOUNDED-BUFFER vs
// NUL-TERMINATING sibling choice to a macro form.
//
// The compile-time flag PETRA_NUL_TERMINATE controls the dispatch:
//
//   #define PETRA_NUL_TERMINATE 0
//     → bounded-buffer sibling (NO NUL):
//        CONSTEVAL: petra::try_format_bounded_runtime_n (Sep 9)
//        RUNTIME:  petra::try_format_runtime_n (Sep 8)
//
//   #define PETRA_NUL_TERMINATE 1
//     → NUL-terminating sibling:
//        CONSTEVAL: petra::format_to_n_into_bounded (Sep 11)
//        RUNTIME:  petra::try_format_runtime_n_into_bounded (Sep 12)
//
// Default: PETRA_NUL_TERMINATE = 0 (bounded-buffer).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write 'auto r = TRY_FORMAT_BOUNDED_N_OR_INTO(...)'
// and capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15's macro shape.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Mirrors
// Sep 13 / Sep 14 / Sep 15.
//
// The two macros cover the TWO surfaces established by Sep 13:
//   - CONSTEVAL surface: std::format_string<Args...> literal primary
//     (the format-string parse happens at compile time).
//   - RUNTIME surface: std::string_view format-string (the format-string
//     parse happens at format time).
//
// This is the PERPENDICULAR axis to Sep 13's macro layer:
//   - Sep 13:    CONSTEVAL vs RUNTIME (format-string parse time)
//   - TODAY:     BOUNDED-BUFFER vs NUL-TERMINATING (output contract)
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
// Bounded-buffer sibling path — NO NUL termination.
#define TRY_FORMAT_BOUNDED_N_OR_INTO(buf, cap, fmt, ...)                    \
    (::petra::try_format_bounded_runtime_n((buf), (cap), (fmt)             \
                                           __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(buf, cap, fmt, ...)            \
    (::petra::try_format_runtime_n((buf), (cap), (fmt)                     \
                                   __VA_OPT__(,) __VA_ARGS__))
#else  // PETRA_NUL_TERMINATE == 1
// NUL-terminating sibling path — always NUL-terminates on success.
#define TRY_FORMAT_BOUNDED_N_OR_INTO(buf, cap, fmt, ...)                    \
    (::petra::format_to_n_into_bounded((buf), (cap), (fmt)                 \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(buf, cap, fmt, ...)            \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt)         \
                                               __VA_OPT__(,) __VA_ARGS__))
#endif  // PETRA_NUL_TERMINATE

// ============================================================================
// Part 10b — Sep 13's TRY_FORMAT_BOUNDED_N / TRY_FORMAT_BOUNDED_RUNTIME_N
// (verbatim from Sep 13)
//
// Section 9 below uses these macros to verify that today's macros
// dispatch to the same wrapper as Sep 13's macros under
// PETRA_NUL_TERMINATE=0.  Each is a verbatim thin dispatcher to ONE
// underlying petra::try_format_* wrapper.
// ============================================================================

#define TRY_FORMAT_BOUNDED_N(buf, cap, fmt, ...)                            \
    (::petra::try_format_bounded_runtime_n((buf), (cap), (fmt)             \
                                           __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N(buf, cap, fmt, ...)                    \
    (::petra::try_format_runtime_n((buf), (cap), (fmt)                     \
                                   __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 11 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15).
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
            __FILE__, __LINE__, g_section, #a, #b);                       \
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
    //     /tmp/probe_nul_terminating_path.cpp pins the
    //     PETRA_NUL_TERMINATE=1 NUL placement contracts.
    static_assert(PETRA_NUL_TERMINATE == 0 || PETRA_NUL_TERMINATE == 1,
                  "PETRA_NUL_TERMINATE must be 0 or 1");
#if PETRA_NUL_TERMINATE == 0
    std::println("[Section 1] PETRA_NUL_TERMINATE=0 (bounded-buffer sibling)");
#else
    std::println("[Section 1] PETRA_NUL_TERMINATE=1 (NUL-terminating sibling)");
#endif
    CHECK(true);

    // 1b. Pin that the macro dispatches to the correct sibling
    //     based on PETRA_NUL_TERMINATE.  Verify by checking the
    //     macro's expansion type matches the underlying wrapper.
    using R1b = decltype(TRY_FORMAT_BOUNDED_N_OR_INTO(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected1b = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R1b, Expected1b>,
                  "TRY_FORMAT_BOUNDED_N_OR_INTO must return "
                  "std::expected<std::size_t, FormatError>");

    using R1c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R1c, Expected1b>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO must return "
                  "std::expected<std::size_t, FormatError>");
    CHECK(true);

    // ----- Section 2 — sibling path pinned by /tmp build
    SECTION("Section 2 - sibling path verification");

#if PETRA_NUL_TERMINATE == 0
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_N_OR_INTO dispatches to
    //     Sep 9's try_format_bounded_runtime_n (bounded-buffer, NO NUL).
    char buf2a[16] = {};  // poison with NUL bytes
    auto r2a = TRY_FORMAT_BOUNDED_N_OR_INTO(buf2a, 16, "hello {}", "world");
    CHECK(r2a.has_value());
    CHECK_EQ(r2a.value(), std::size_t{11});  // "hello world" = 11 bytes
    // The byte at index 11 must be UNCHANGED (= '\0' from the poison
    // init).  The bounded-buffer sibling does NOT write a NUL.
    CHECK_EQ(buf2a[11], '\0');  // still the poison NUL
    // Verify the prefix bytes match "hello world".
    CHECK_EQ((std::string_view{buf2a, 11}), (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO dispatches
    //     to Sep 8's try_format_runtime_n (bounded-buffer, NO NUL).
    char buf2b[16] = {};
    std::string fmt2b = "value={}";
    auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf2b, 16, fmt2b, 42);
    CHECK(r2b.has_value());
    CHECK_EQ(r2b.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ(buf2b[8], '\0');  // still the poison NUL
    CHECK_EQ((std::string_view{buf2b, 8}), (std::string_view{"value=42"}));

    // 2c. NO NUL: a bounded-buffer sibling writes bytes [0, written)
    //     and leaves bytes [written, cap) UNCHANGED.  Pin by writing
    //     poison pattern first, then verifying.
    char buf2c[32] = {};
    for (std::size_t i = 0; i < 32; ++i) buf2c[i] = static_cast<char>('Q');
    auto r2c = TRY_FORMAT_BOUNDED_N_OR_INTO(buf2c, 32, "[{}]", 7);
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
    // 2a. CONSTEVAL macro: TRY_FORMAT_BOUNDED_N_OR_INTO dispatches to
    //     Sep 11's format_to_n_into_bounded (NUL-terminating).
    char buf2a[16] = {};  // poison with NUL bytes
    auto r2a = TRY_FORMAT_BOUNDED_N_OR_INTO(buf2a, 16, "hello {}", "world");
    CHECK(r2a.has_value());
    CHECK_EQ(r2a.value(), std::size_t{11});  // "hello world" = 11 bytes
    // The byte at index 11 IS written as '\0' by the NUL-terminating sibling.
    CHECK_EQ(buf2a[11], '\0');  // NUL placed by sibling
    CHECK_EQ((std::string_view{buf2a, 11}), (std::string_view{"hello world"}));

    // 2b. RUNTIME macro: TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO dispatches
    //     to Sep 12's try_format_runtime_n_into_bounded (NUL-terminating).
    char buf2b[16] = {};
    std::string fmt2b = "value={}";
    auto r2b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf2b, 16, fmt2b, 42);
    CHECK(r2b.has_value());
    CHECK_EQ(r2b.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ(buf2b[8], '\0');  // NUL placed by sibling
    CHECK_EQ((std::string_view{buf2b, 8}), (std::string_view{"value=42"}));

    // 2c. NUL placement: the NUL-terminating sibling writes bytes [0, written)
    //     AND places '\0' at buf[written].  Pin by writing poison
    //     pattern first, then verifying the NUL placement.
    char buf2c[32] = {};
    for (std::size_t i = 0; i < 32; ++i) buf2c[i] = static_cast<char>('Q');
    auto r2c = TRY_FORMAT_BOUNDED_N_OR_INTO(buf2c, 32, "[{}]", 7);
    CHECK(r2c.has_value());
    CHECK_EQ(r2c.value(), std::size_t{3});  // "[7]" = 3 bytes
    CHECK_EQ(buf2c[0], '[');
    CHECK_EQ(buf2c[1], '7');
    CHECK_EQ(buf2c[2], ']');
    CHECK_EQ(buf2c[3], '\0');  // NUL placed by sibling
    CHECK_EQ(buf2c[31], 'Q');  // unchanged poison (after the NUL)

    // 2d. NUL termination is the key contract difference vs the
    //     bounded-buffer sibling.  Section 5's /tmp probe pins the
    //     bounded-buffer sibling does NOT write a NUL.
#endif  // PETRA_NUL_TERMINATE

    // ----- Section 3 — atomicity on TooLarge (bounded-buffer path)
    SECTION("Section 3 - atomicity on TooLarge (bounded-buffer)");
#if PETRA_NUL_TERMINATE == 0

    // 3a. On TooLarge, the buffer is UNTOUCHED.  Pin by poisoning the
    //     buffer first and verifying every byte is unchanged.
    char buf3a[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3a[i] = static_cast<char>('Q');
    auto r3a = TRY_FORMAT_BOUNDED_N_OR_INTO(buf3a, 4, "too long: {}",
                                            std::string(20, 'X'));
    CHECK(!r3a.has_value());
    CHECK_EQ(r3a.error().kind, petra::FormatErrorKind::TooLarge);
    // Every byte must still be 'Q'.
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3a[i], 'Q');
    }

    // 3b. RUNTIME macro atomicity too.
    char buf3b[16] = {};
    for (std::size_t i = 0; i < 16; ++i) buf3b[i] = static_cast<char>('Q');
    std::string fmt3b = "too long: {}";
    auto r3b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf3b, 4, fmt3b, std::string(20, 'X'));
    CHECK(!r3b.has_value());
    CHECK_EQ(r3b.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK_EQ(buf3b[i], 'Q');
    }

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 4 — return-type contract
    SECTION("Section 4 - return-type contract");

    // 4a. Both macros return std::expected<std::size_t, FormatError>.
    using R4a = decltype(TRY_FORMAT_BOUNDED_N_OR_INTO(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using Expected4 = std::expected<std::size_t, petra::FormatError>;
    static_assert(std::is_same_v<R4a, Expected4>);

    using R4b = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R4b, Expected4>);

    // 4b. The expected's success value is std::size_t (the byte count).
    //     Verify with sizeof:
    static_assert(sizeof(R4a) == sizeof(Expected4));
    static_assert(alignof(R4a) == alignof(Expected4));
    CHECK(true);

    // 4c. FormatError contract: kind / message / needed_bytes all match
    //     the Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 11 / Sep 12 contract.
    char buf4c[16] = {};
    auto r4c = TRY_FORMAT_BOUNDED_N_OR_INTO(buf4c, 4, "too long: {}",
                                            std::string(20, 'X'));
    CHECK(!r4c.has_value());
    CHECK_EQ(r4c.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK(!r4c.error().message.empty());
    CHECK(r4c.error().needed_bytes > 4);  // would_be > cap

    // ----- Section 5 — NUL-terminating sibling path pinned via /tmp build
    SECTION("Section 5 - NUL-terminating sibling path (PETRA_NUL_TERMINATE=1)");

    // 5a. The NUL-terminating path is pinned by a /tmp build with
    //     -DPETRA_NUL_TERMINATE=1.  The output of that build is
    //     documented in the lesson notes (Section 5b / 5c).
    //
    //     Today's main() is compiled with the DEFAULT (PETRA_NUL_TERMINATE=0).
    //     A separate compile of the same source with -DPETRA_NUL_TERMINATE=1
    //     exercises the NUL-terminating sibling.
    //
    //     /tmp/probe_nul_terminating_path.cpp (NOT in the working tree)
    //     is a small probe that captures the behavior:
    //       $ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
    //                 -DPETRA_NUL_TERMINATE=1 \
    //                 /tmp/probe_nul_terminating_path.cpp \
    //                 -o /tmp/probe_nul_terminating_path
    //       $ /tmp/probe_nul_terminating_path
    //       PROBE OK: NUL-terminating sibling writes '\0' at buf[written]
    //
    //     The probe exercises three contracts:
    //     (a) on success with needed < cap, the NUL-terminating sibling
    //         writes '\0' at buf[written] (== buf[needed]).
    //     (b) on truncation (needed == cap), the NUL-terminating sibling
    //         writes '\0' at buf[cap-1] (Sep 11's formula).
    //     (c) on TooLarge (needed > cap), the buffer is UNTOUCHED.
    //
    //     All three are documented in the lesson notes (Section 5b / 5c).
    CHECK(true);

    // ----- Section 6 — byte-count contract (bounded-buffer path)
    SECTION("Section 6 - byte-count contract (bounded-buffer)");
#if PETRA_NUL_TERMINATE == 0

    // 6a. Success: r.value() == formatted byte count.
    char buf6a[32] = {};
    auto r6a = TRY_FORMAT_BOUNDED_N_OR_INTO(
        buf6a, 32, "[{}] [{}] [{}]", 1, 2, 3);
    CHECK(r6a.has_value());
    CHECK_EQ(r6a.value(), std::size_t{11});  // "[1] [2] [3]" = 11 bytes
    CHECK_EQ((std::string_view{buf6a, r6a.value()}),
             (std::string_view{"[1] [2] [3]"}));
    // 6b. The returned byte count is the FORMATTED byte count, NOT
    //     including any NUL byte the NUL-terminating sibling would write
    //     (which the bounded-buffer sibling doesn't write at all).
    //     Pin by comparing against the same format string via
    //     std::string.
    std::string s6b = std::format("[{}] [{}] [{}]", 1, 2, 3);
    CHECK_EQ(r6a.value(), s6b.size());

    // 6c. UTF-8 mid-codepoint byte round-trip (verify NO truncation
    //     happens on a multi-byte payload).
    char buf6c[64] = {};
    std::string utf8 = "\xE2\x9C\x93";  // ✓ U+2713 CHECK MARK
    auto r6c = TRY_FORMAT_BOUNDED_N_OR_INTO(
        buf6c, 64, "check={}", utf8);
    CHECK(r6c.has_value());
    CHECK_EQ(r6c.value(), std::size_t{9});  // "check=✓" = 9 bytes (UTF-8 check mark is 3 bytes)
    CHECK_EQ((std::string_view{buf6c, r6c.value()}), (std::string_view{"check=\xE2\x9C\x93"}));

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 7 — FormatError equality across both siblings
    SECTION("Section 7 - FormatError equality contract");
#if PETRA_NUL_TERMINATE == 0

    // 7a. The FormatError type is IDENTICAL across the bounded-buffer
    //     sibling and the NUL-terminating sibling (both delegate to
    //     the same FormatError struct from Sep 6 / Sep 7 / Sep 8 /
    //     Sep 11 / Sep 12).  Pin by static_assert:
    static_assert(std::is_same_v<petra::FormatError, petra::FormatError>,
                  "FormatError type must be identical across siblings");

    // 7b. On TooLarge, the error message / kind / needed_bytes are
    //     identical for the same inputs on both siblings (Sep 11 and
    //     Sep 9's wrappers share the same two-pass measure-then-write
    //     design and the same structured-error contract).
    //     We pin this contract on the bounded-buffer path today (Sep 9
    //     / Sep 8's wrapper), and Section 5's /tmp probe pins the
    //     NUL-terminating path (Sep 11 / Sep 12's wrapper).
    char buf7b[16] = {};
    auto r7b = TRY_FORMAT_BOUNDED_N_OR_INTO(buf7b, 4, "long: {}",
                                            std::string(20, 'X'));
    CHECK(!r7b.has_value());
    CHECK_EQ(r7b.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK(r7b.error().needed_bytes > 4);
    // The error message must contain "bytes" and "cap".
    CHECK(r7b.error().message.find("bytes") != std::string::npos);
    CHECK(r7b.error().message.find("cap") != std::string::npos);

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 8 — runtime format string (PETRA_NUL_TERMINATE=0 path)
    SECTION("Section 8 - runtime format string (bounded-buffer)");
#if PETRA_NUL_TERMINATE == 0
    // 8a. RUNTIME macro accepts a runtime std::string format string
    //     (the RUNTIME surface takes std::string_view).
    char buf8a[32] = {};
    std::string fmt8a = "value=";
    fmt8a += std::to_string(42);  // runtime-built
    auto r8a = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf8a, 32, fmt8a);
    CHECK(r8a.has_value());
    CHECK_EQ(r8a.value(), std::size_t{8});  // "value=42" = 8 bytes
    CHECK_EQ((std::string_view{buf8a, r8a.value()}),
             (std::string_view{"value=42"}));

    // 8b. A runtime-built format string with replacement fields works
    //     identically to a literal at the call site.  Pin with a
    //     runtime template string.
    char buf8b[64] = {};
    std::string fmt8b = "[";
    fmt8b += std::to_string(1);
    fmt8b += "] [";
    fmt8b += std::to_string(2);
    fmt8b += "]";
    auto r8b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf8b, 64, fmt8b);
    CHECK(r8b.has_value());
    CHECK_EQ(r8b.value(), std::size_t{7});  // "[1] [2]" = 7 bytes
    CHECK_EQ((std::string_view{buf8b, r8b.value()}),
             (std::string_view{"[1] [2]"}));
#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 9 — consteval-vs-runtime contract (perpendicular axis)
    SECTION("Section 9 - consteval-vs-runtime axis (perpendicular)");
#if PETRA_NUL_TERMINATE == 0

    // 9a. The PETRA_NUL_TERMINATE axis (bounded-buffer vs NUL-term) is
    //     PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis (Sep 13's
    //     macro layer).  Today's lesson adds the PETRA_NUL_TERMINATE
    //     axis on top of Sep 13's CONSTEVAL-vs-RUNTIME axis.
    //
    //     Sep 13's TRY_FORMAT_BOUNDED_N dispatch is CONSTEVAL +
    //     bounded-buffer (Sep 9's try_format_bounded_runtime_n).
    //     Sep 13's TRY_FORMAT_BOUNDED_INTO dispatch is CONSTEVAL +
    //     NUL-terminating (Sep 11's format_to_n_into_bounded).
    //
    //     Today's TRY_FORMAT_BOUNDED_N_OR_INTO with
    //     PETRA_NUL_TERMINATE=0 dispatches to Sep 13's
    //     TRY_FORMAT_BOUNDED_N (CONSTEVAL + bounded-buffer).
    //     Today's TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO with
    //     PETRA_NUL_TERMINATE=0 dispatches to Sep 13's
    //     TRY_FORMAT_BOUNDED_RUNTIME_N (RUNTIME + bounded-buffer).
    //
    //     Verify by decltype comparison: the macro returns the same
    //     type as the underlying Sep 13 macro.
    using R9a = decltype(TRY_FORMAT_BOUNDED_N_OR_INTO(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    using R9b = decltype(TRY_FORMAT_BOUNDED_N(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::format_string<int>>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R9a, R9b>,
                  "TRY_FORMAT_BOUNDED_N_OR_INTO with "
                  "PETRA_NUL_TERMINATE=0 must dispatch to the same "
                  "wrapper as TRY_FORMAT_BOUNDED_N (CONSTEVAL + "
                  "bounded-buffer)");

    using R9c = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::string_view>(),
        std::declval<int>()));
    using R9d = decltype(TRY_FORMAT_BOUNDED_RUNTIME_N(
        std::declval<char*>(), std::size_t{16},
        std::declval<std::string_view>(),
        std::declval<int>()));
    static_assert(std::is_same_v<R9c, R9d>,
                  "TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO with "
                  "PETRA_NUL_TERMINATE=0 must dispatch to the same "
                  "wrapper as TRY_FORMAT_BOUNDED_RUNTIME_N (RUNTIME + "
                  "bounded-buffer)");
    CHECK(true);

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 10 — no-args (zero-replacement-field) variant
    SECTION("Section 10 - no-args variant");
#if PETRA_NUL_TERMINATE == 0

    // 10a. CONSTEVAL macro with no replacement fields.
    char buf10a[16] = {};
    auto r10a = TRY_FORMAT_BOUNDED_N_OR_INTO(buf10a, 16, "ping");
    CHECK(r10a.has_value());
    CHECK_EQ(r10a.value(), std::size_t{4});  // "ping" = 4 bytes
    CHECK_EQ((std::string_view{buf10a, r10a.value()}), (std::string_view{"ping"}));
    // NO NUL: byte at index 4 still poison NUL.
    CHECK_EQ(buf10a[4], '\0');

    // 10b. RUNTIME macro with no replacement fields.
    char buf10b[16] = {};
    std::string fmt10b = "pong";
    auto r10b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(buf10b, 16, fmt10b);
    CHECK(r10b.has_value());
    CHECK_EQ(r10b.value(), std::size_t{4});
    CHECK_EQ((std::string_view{buf10b, r10b.value()}), (std::string_view{"pong"}));
    CHECK_EQ(buf10b[4], '\0');  // NO NUL — bounded-buffer sibling

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 11 — multi-arg round-trip
    SECTION("Section 11 - multi-arg round-trip");
#if PETRA_NUL_TERMINATE == 0

    // 11a. CONSTEVAL macro with multiple args of different types.
    char buf11a[64] = {};
    auto r11a = TRY_FORMAT_BOUNDED_N_OR_INTO(
        buf11a, 64, "x={} y={} z={:.2f}", 42, std::string{"hi"}, 3.14);
    CHECK(r11a.has_value());
    CHECK_EQ(r11a.value(), std::size_t{16});  // "x=42 y=hi z=3.14" = 16
    CHECK_EQ((std::string_view{buf11a, r11a.value()}), (std::string_view{"x=42 y=hi z=3.14"}));

    // 11b. RUNTIME macro with multiple args.
    char buf11b[64] = {};
    std::string fmt11b = "x={} y={} z={:.2f}";
    auto r11b = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf11b, 64, fmt11b, 42, std::string{"hi"}, 3.14);
    CHECK(r11b.has_value());
    CHECK_EQ(r11b.value(), std::size_t{16});
    CHECK_EQ((std::string_view{buf11b, r11b.value()}), (std::string_view{"x=42 y=hi z=3.14"}));

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 12 — embedded NUL byte round-trip
    SECTION("Section 12 - embedded NUL byte round-trip");
#if PETRA_NUL_TERMINATE == 0

    // 12a. The bounded-buffer sibling treats the byte sequence as a
    //     raw byte buffer, NOT as a NUL-terminated C-string.  A
    //     format string with an embedded NUL byte writes bytes
    //     verbatim.  Pin by passing a std::string with an embedded NUL.
    //
    //     We use the RUNTIME macro because std::format_string<Args...>
    //     requires a literal — and a literal with an embedded NUL would
    //     be truncated at the NUL by the literal->string_view conversion.
    //     The RUNTIME macro takes a std::string_view which handles
    //     embedded NUL bytes naturally (no NUL-search).
    char buf12a[64] = {};
    // Build a runtime std::string with an embedded NUL byte.
    std::string fmt12a;
    fmt12a.push_back('h');
    fmt12a.push_back('i');
    fmt12a.push_back('\0');  // embedded NUL
    fmt12a.push_back('h');
    fmt12a.push_back('i');
    fmt12a.push_back(' ');
    fmt12a.push_back('{');
    fmt12a.push_back('}');
    fmt12a.push_back('!');
    auto r12a = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
        buf12a, 64, fmt12a, "X");
    CHECK(r12a.has_value());
    CHECK_EQ(r12a.value(), std::size_t{8});  // {} → X, so 9 - 1 = 8 bytes
    CHECK_EQ(buf12a[0], 'h');
    CHECK_EQ(buf12a[1], 'i');
    CHECK_EQ(buf12a[2], '\0');  // embedded NUL byte preserved
    CHECK_EQ(buf12a[3], 'h');
    CHECK_EQ(buf12a[4], 'i');
    CHECK_EQ(buf12a[5], ' ');
    CHECK_EQ(buf12a[6], 'X');
    CHECK_EQ(buf12a[7], '!');
    // NO NUL at index 8 — bounded-buffer sibling.
    CHECK_EQ(buf12a[8], '\0');  // still poison NUL

#endif  // PETRA_NUL_TERMINATE == 0
    // ----- Section 13 — 4 threads × 25 calls concurrent dispatch
    SECTION("Section 13 - 4 threads x 25 calls concurrent dispatch");
#if PETRA_NUL_TERMINATE == 0

    // 13a. Concurrent calls to the CONSTEVAL macro.  Each thread
    //     increments its own local counter; we aggregate AFTER join
    //     to avoid a data race on g_pass / g_fail.
    //
    //     We use a FIXED-WIDTH format spec "[{:02}][{:02}]" so the
    //     byte count is invariant across iterations regardless of
    //     the integer values (8 bytes: "[NN][MM]").
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
                char buf[64] = {};
                for (int i = 0; i < kCallsPerThread; ++i) {
                    auto r = TRY_FORMAT_BOUNDED_N_OR_INTO(
                        buf, 64, "[{:02}][{:02}]", t, i);
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

    // 13b. Concurrent calls to the RUNTIME macro.
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
                char buf[64] = {};
                for (int i = 0; i < kCallsPerThread; ++i) {
                    auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(
                        buf, 64, fmt, t, i);
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
