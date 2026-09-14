// P-2026-09-14 — try_format_span_format_string:
// A STD::SPAN<CONST CHAR> FORMAT-STRING PARAMETER OVERLOAD FAMILY for the
// runtime try_format_* wrappers built up across Sep 6 / Sep 8 / Sep 12,
// plus the Sep 13 TRY_FORMAT_BOUNDED_RUNTIME / _N / _INTO / _N_SPAN
// macros.  Closes Sep 13's "Where we go next" item #3 verbatim:
//
//   > 3. A std::span<const char> overload for the format string
//   >    parameter.  Today's macros take std::string_view for the
//   >    runtime surface (which already accepts std::span<const
//   >    char>-like inputs via implicit conversion).  Sep 10's open
//   >    item #5.
//
// Today's lesson exists to pin FOUR things:
//
//   1. The conversion from std::span<const char> to std::string_view
//      is EXPLICIT (NOT implicit) on libc++ 21 — pinned by the
//      static_assert std::is_convertible_v<std::span<const char>,
//      std::string_view> failing at compile time AND the parallel
//      static_assert std::is_constructible_v<std::string_view,
//      std::span<const char>> succeeding.  This CONTRADICTS Sep 13's
//      note which assumed the conversion was implicit.  The exact
//      std::string_view ctor is the C++23 range ctor (P2499R0):
//      `constexpr explicit basic_string_view(_Range&& __r)` — the
//      `explicit` keyword blocks implicit conversion but the
//      conversion is still well-defined when explicitly invoked
//      (e.g. `std::string_view{span}` or via the new overload
//      functions in today's lesson).
//
//   2. The conversion is BYTE-EXACT (no terminating-NUL is required,
//      no allocation happens, no UTF-8 / encoding transformation is
//      performed) — pinned by passing a std::span<const char> over a
//      sub-range of a larger buffer that intentionally has no NUL
//      anywhere within or after it (Sections 2 and 3).
//
//   3. Adding explicit std::span<const char> overloads for the runtime
//      try_format_runtime_bounded / _n / _n_into_bounded family
//      provides (a) a more honest signature for callers that already
//      hold a std::span<const char> (the explicit conversion above
//      would otherwise force every call site to write
//      `std::string_view{span}` explicitly), and (b) compile-time
//      rejection of mutations to the format string (std::span<const
//      char> carries the const through).  The new overloads are
//      VERBATIM thin forwarding wrappers — they extract .data() +
//      .size() from the span, build a std::string_view EXPLICITLY,
//      and dispatch to the existing impls UNCHANGED.
//
//   4. The four new TRY_FORMAT_BOUNDED_SPAN / _N_SPAN_FMT / _INTO_SPAN /
//      _RUNTIME_N_SPAN_SPAN macros lift the std::span<const char>
//      format-string choice to the macro form — mirroring Sep 13's
//      pattern of one macro per underlying wrapper overload.
//
// The shape of the new overloads:
//
//   petra::try_format_runtime_bounded(std::span<const char> fmt,
//                                     std::size_t cap, const Args&... args)
//       -> std::expected<std::string, petra::FormatError>
//       (Sep 6 impl, unchanged).
//
//   petra::try_format_runtime_n(std::span<const char> fmt,
//                               char* buf, std::size_t cap,
//                               const Args&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//       (Sep 8 impl, unchanged).
//
//   petra::try_format_runtime_n_into_bounded(std::span<const char> fmt,
//                                            char* buf, std::size_t cap,
//                                            const Args&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//       (Sep 12 impl, unchanged).
//
//   petra::try_format_runtime_n_into_bounded(std::span<const char> fmt,
//                                            std::span<char> out,
//                                            const Args&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//       (Sep 12 span<char> overload, unchanged).
//
// And a parallel macro layer (today's new macros):
//
//   TRY_FORMAT_BOUNDED_SPAN(cap, span, args...)
//       -> std::expected<std::string, petra::FormatError>
//       expands to ::petra::try_format_runtime_bounded(span, cap, args...)
//       (Sep 6 impl via the new span<const char> overload).
//
//   TRY_FORMAT_BOUNDED_N_SPAN_FMT(out_span, cap, fmt_span, args...)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to ::petra::try_format_runtime_n(fmt_span, out_span.data(),
//                                                out_span.size(), args...)
//       (Sep 8 impl via the new span<const char> overload).
//
//   TRY_FORMAT_BOUNDED_INTO_SPAN(out_buf, cap, fmt_span, args...)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to ::petra::try_format_runtime_n_into_bounded(fmt_span,
//                                                             out_buf, cap,
//                                                             args...)
//       (Sep 12 impl via the new span<const char> overload).
//
//   TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN(out_span, fmt_span, args...)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to ::petra::try_format_runtime_n_into_bounded(fmt_span,
//                                                             out_span,
//                                                             args...)
//       (Sep 12 span<char> overload via the new span<const char> overload).
//
// The consteval-vs-runtime sibling split mirrors Sep 13's pattern;
// the span<const char> overloads live ONLY on the runtime surface —
// the consteval surface's std::format_string<Args...> ctor already
// accepts std::span<const char> via implicit conversion AND via the
// deduction guide (a format string literal is already a const char*
// so std::span<const char>{literal, N} works through deduction).
//
// What gets reused (verbatim from Aug 25 / Sep 6 / Sep 7 / Sep 8 /
// Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13):
//
//   - petra::FormatError{kind, message, needed_bytes} — verbatim.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — verbatim.
//   - petra::counting_output_iterator — verbatim.
//   - petra::detail::bounded_char_writer — verbatim.
//   - petra::detail::try_format_runtime_bounded_impl — Sep 6 verbatim.
//   - petra::detail::try_format_runtime_n_impl — Sep 8 verbatim.
//   - petra::detail::try_format_to_n_into_bounded_impl — Sep 12 verbatim.
//   - petra::try_format_runtime_bounded — Sep 6 verbatim (kept as the
//     std::string_view primary; today adds a parallel std::span<const char>
//     overload that forwards to the same impl).
//   - petra::try_format_runtime_n — Sep 8 verbatim (kept as the
//     std::string_view primary; today adds a parallel std::span<const char>
//     overload).
//   - petra::try_format_runtime_n_into_bounded (char*, size_t, ...)
//     — Sep 12 verbatim (kept; today adds a parallel std::span<const char>
//     overload for the format-string parameter).
//   - petra::try_format_runtime_n_into_bounded (std::span<char>, ...)
//     — Sep 12 verbatim (kept; today adds a parallel std::span<const char>
//     overload for the format-string parameter).
//
// What is NEW today:
//
//   - The four new overloads accepting std::span<const char> for the
//     format string parameter (each is a verbatim thin forwarding
//     wrapper).
//   - The four new TRY_FORMAT_BOUNDED_*_SPAN / _SPAN_FMT macros — each
//     expands to exactly ONE of the new overloads.
//
// C++23, std::format, std::vformat_to, std::string_view,
// std::span<const char> (P0122R7), std::make_format_args,
// std::format_error, std::expected (P0323R12), std::println (P2093R14),
// std::ranges::contiguous_range.  Requires -fexperimental-library on
// Apple Clang 21.0.0 / libc++ 21 for std::println.

#include <array>
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13)
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
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13)
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13)
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
    auto cb2 = [](std::size_t n, void* data) noexcept {
        if (data) *static_cast<std::size_t*>(data) = n;
    };
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out.data(), needed, cb2, &written};
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
// Part 5 — petra::try_format_runtime_bounded (Sep 6 std::string_view primary,
// verbatim; today adds the std::span<const char> overload after it).
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
// Part 6 — petra::try_format_runtime_bounded(std::span<const char>, ...)
// NEW TODAY — a verbatim thin forwarding wrapper that builds a
// std::string_view from the span and dispatches to the Sep 6 impl
// UNCHANGED.  The span<const char> parameter is the same byte-exact
// content as std::string_view; we just don't trust the caller to have
// a NUL terminator and we keep the const through the API surface.
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded(std::span<const char> fmt, std::size_t cap,
                           const Args&... args) {
    return detail::try_format_runtime_bounded_impl(
        std::string_view{fmt.data(), fmt.size()}, cap, args...);
}

}  // namespace petra

// ============================================================================
// Part 7 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim)
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
// Part 8 — petra::try_format_runtime_n (Sep 8 std::string_view primary,
// verbatim; today adds the std::span<const char> overload after it).
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
// Part 9 — petra::try_format_runtime_n(std::span<const char>, ...)
// NEW TODAY — verbatim thin forwarding wrapper (mirrors Part 6's pattern).
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* buf, std::size_t cap,
                     std::span<const char> fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

}  // namespace petra

// ============================================================================
// Part 10 — petra::detail::try_format_to_n_into_bounded_impl (Sep 12 verbatim)
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
// Part 11 — petra::try_format_runtime_n_into_bounded (Sep 12 verbatim;
// std::string_view primary on the format-string parameter, char*+cap
// output, plus the std::span<char> output overload).
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* buf, std::size_t cap,
                                  std::string_view fmt,
                                  const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(buf, cap, fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(std::span<char> out, std::string_view fmt,
                                  const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        out.data(), out.size(), fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 12 — petra::try_format_runtime_n_into_bounded(std::span<const char>, ...)
// NEW TODAY — TWO verbatim thin forwarding wrappers (mirrors Part 6's
// pattern).  One for the (char*, cap) output pair; one for the
// std::span<char> output.  Both rebuild std::string_view from the
// format-string span and dispatch to the Sep 12 impl UNCHANGED.
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* buf, std::size_t cap,
                                  std::span<const char> fmt,
                                  const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(std::span<char> out,
                                  std::span<const char> fmt,
                                  const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        out.data(), out.size(),
        std::string_view{fmt.data(), fmt.size()}, args...);
}

}  // namespace petra

// ============================================================================
// Part 13 — TRY_FORMAT_BOUNDED_SPAN / _N_SPAN_FMT / _INTO_SPAN / _RUNTIME_N_SPAN_SPAN
// NEW TODAY — four macros that lift the std::span<const char>
// format-string choice to the macro form.  Each macro is a verbatim
// thin dispatcher to ONE of the new overloads above.
//
// The macro names are intentionally distinct from Sep 13's eight
// macros so the call site makes the choice explicit:
//   - TRY_FORMAT_BOUNDED              -> Sep 7 std::string surface (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_RUNTIME      -> Sep 6 std::string surface (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_SPAN         -> TODAY: Sep 6 std::string surface,
//                                        std::span<const char> fmt
//   - TRY_FORMAT_BOUNDED_N            -> Sep 9 char*+cap (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_RUNTIME_N    -> Sep 8 char*+cap (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_N_SPAN_FMT   -> TODAY: Sep 8 char*+cap,
//                                        std::span<const char> fmt
//   - TRY_FORMAT_BOUNDED_INTO         -> Sep 11 char*+cap NUL-term (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_RUNTIME_INTO -> Sep 12 char*+cap NUL-term (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_INTO_SPAN    -> TODAY: Sep 12 char*+cap NUL-term,
//                                        std::span<const char> fmt
//   - TRY_FORMAT_BOUNDED_N_SPAN       -> Sep 10 std::span<char> char*+cap (Sep 13 macro)
//   - TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN
//                                      -> TODAY: Sep 12 std::span<char> char*+cap
//                                         NUL-term, std::span<const char> fmt
//
// Each macro uses __VA_OPT__(,) to handle the no-args case (a format
// with no replacement fields) — without __VA_OPT__ a macro like
// TRY_FORMAT_BOUNDED_SPAN(100, fmt) would expand to
// petra::try_format_runtime_bounded(100, fmt, ) which has a trailing
// comma and is a syntax error.  __VA_OPT__(,) provides the comma
// only when __VA_ARGS__ is non-empty.
//
// Each macro is a parenthesized expression — NOT a do { } while (0)
// block — so the caller can write `auto r = TRY_FORMAT_BOUNDED_SPAN(...);`
// and capture the result.  Mirrors Sep 13's macro shape exactly.
// ============================================================================

#define TRY_FORMAT_BOUNDED_SPAN(cap, fmt, ...)                              \
    (::petra::try_format_runtime_bounded((fmt), (cap) __VA_OPT__(,)        \
                                         __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_N_SPAN_FMT(buf, cap, fmt, ...)                   \
    (::petra::try_format_runtime_n((buf), (cap), (fmt) __VA_OPT__(,)       \
                                   __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_INTO_SPAN(buf, cap, fmt, ...)                    \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt)        \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN(out_span, fmt_span, ...)     \
    (::petra::try_format_runtime_n_into_bounded((out_span), (fmt_span)     \
                                                __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 14 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13).
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
// Part 15 — sections
// ============================================================================

int main() {
    // ----- Section 1 — std::span<const char> properties + explicit conversion
    SECTION("Section 1 - std::span<const char> properties + EXPLICIT conversion to std::string_view");

    // 1a. The headline finding: the conversion std::span<const char>
    //     -> std::string_view is EXPLICIT on libc++ 21.  This is a
    //     CONTRADICTION of Sep 13's note which assumed the conversion
    //     was implicit.  Pinned by the negative std::is_convertible_v
    //     static_assert and the parallel positive
    //     std::is_constructible_v static_assert.
    static_assert(!std::is_convertible_v<std::span<const char>,
                                         std::string_view>,
                  "std::span<const char> must NOT be implicitly convertible "
                  "to std::string_view (libc++ 21 marks the range ctor "
                  "explicit — P2499R0)");
    static_assert(std::is_constructible_v<std::string_view,
                                          std::span<const char>>,
                  "std::string_view must be EXPLICITLY constructible "
                  "from std::span<const char> (P2499R0 range ctor)");
    static_assert(std::is_convertible_v<std::string_view,
                                        std::span<const char>>,
                  "std::string_view must be implicitly convertible "
                  "to std::span<const char> (the other direction IS "
                  "implicit on libc++ 21)");

    // 1b. std::span<const char> properties — pinned for posterity.
    static_assert(std::ranges::contiguous_range<std::span<const char>>,
                  "std::span<const char> must model "
                  "std::ranges::contiguous_range");
    static_assert(std::ranges::sized_range<std::span<const char>>,
                  "std::span<const char> must model "
                  "std::ranges::sized_range");
    static_assert(std::is_trivially_copyable_v<std::span<const char>>,
                  "std::span<const char> must be trivially copyable");
    static_assert(std::is_standard_layout_v<std::span<const char>>,
                  "std::span<const char> must be standard-layout");
    static_assert(std::is_same_v<decltype(std::declval<std::span<const char>>().data()),
                                 const char*>,
                  "std::span<const char>::data() must return const char*");
    static_assert(std::is_same_v<decltype(std::declval<std::span<const char>>().size()),
                                 std::size_t>,
                  "std::span<const char>::size() must return std::size_t");
    CHECK(true);  // all static_asserts above already fired at compile time

    // 1c. Explicit conversion: build a span<const char> from a 6-byte
    //     literal "hello!" (no NUL needed inside or after — the span
    //     stores .size() == 6).  Convert to std::string_view EXPLICITLY
    //     (because the ctor is explicit) and verify .size() and the
    //     byte content.  Without the explicit form, this line would
    //     fail with the same diagnostic we saw at compile time.
    const char hello_lit[6] = {'h','e','l','l','o','!'};
    std::span<const char> hello_span{hello_lit};
    CHECK_EQ(hello_span.size(), std::size_t{6});
    std::string_view hello_sv{hello_span};  // EXPLICIT conversion
    CHECK_EQ(hello_sv.size(), std::size_t{6});
    CHECK_EQ(std::string{hello_sv}, std::string{"hello!"});

    // 1d. Verify that implicit assignment fails to compile — the line
    //     below is commented out and is documented for posterity.
    //     std::string_view hello_sv_implicit = hello_span;  // ERROR:
    //     no viable conversion from 'std::span<const char>' to
    //     'std::string_view' (P2499R0 range ctor is explicit).
    CHECK(true);

    // ----- Section 2 — try_format_runtime_bounded(std::span<const char>)
    SECTION("Section 2 - try_format_runtime_bounded(std::span<const char>)");

    // 2a. Happy path: format "hello {}" with arg "world", expect
    //     std::expected<std::string, FormatError>{"hello world"}.
    std::array<char, 8> hello_fmt{'h','e','l','l','o',' ','{','}'};
    std::span<const char> hello_fmt_span{hello_fmt};
    auto r1 = TRY_FORMAT_BOUNDED_SPAN(100, hello_fmt_span, "world");
    CHECK(r1.has_value());
    CHECK_EQ(r1.value(), std::string{"hello world"});

    // 2b. Runtime-format-string acceptance: build a fmt at runtime
    //     (string concatenation + std::to_string) and pass via span.
    //     This is impossible on the CONSTEVAL surface (Sep 7) where
    //     std::format_string<Args...>'s ctor is consteval.
    std::string runtime_fmt = "v=" + std::to_string(42);
    std::span<const char> runtime_fmt_span{
        reinterpret_cast<const char*>(runtime_fmt.data()),
        runtime_fmt.size()};
    auto r2 = TRY_FORMAT_BOUNDED_SPAN(100, runtime_fmt_span);
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::string{"v=42"});

    // 2c. Multi-arg round-trip with runtime format string.
    std::string multi_fmt = "x={} y={} z={}";
    std::span<const char> multi_fmt_span{
        reinterpret_cast<const char*>(multi_fmt.data()),
        multi_fmt.size()};
    auto r3 = TRY_FORMAT_BOUNDED_SPAN(100, multi_fmt_span, 1, 2, 3);
    CHECK(r3.has_value());
    CHECK_EQ(r3.value(), std::string{"x=1 y=2 z=3"});

    // ----- Section 3 — sub-span over a 16-byte buffer (no NUL anywhere)
    SECTION("Section 3 - sub-span over a 16-byte buffer (no NUL anywhere)");

    // 3a. The byte-exactness claim: std::span<const char>{buf, 6}
    //     over a sub-range of a 16-byte buffer with NO NUL inside
    //     the span and NO NUL anywhere else.  This pins that the
    //     conversion to std::string_view does NOT require or insert
    //     a NUL — the .size() carries the length.
    char buffer[16];
    for (std::size_t i = 0; i < 16; ++i) buffer[i] = 'Q';
    // Place "hello " (6 bytes) at offset 5.
    const char hello_str[6] = {'h','e','l','l','o',' '};
    for (std::size_t i = 0; i < 6; ++i) buffer[5 + i] = hello_str[i];
    // Sub-span over the 6 bytes we just placed.
    std::span<const char> hello_sub_span{buffer + 5, 6};
    // Confirm there's NO NUL anywhere in the buffer (the bytes after
    // the span are still 'Q', not '\0').
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK(buffer[i] != '\0');
    }
    auto r4 = TRY_FORMAT_BOUNDED_SPAN(100, hello_sub_span);
    CHECK(r4.has_value());
    CHECK_EQ(r4.value(), std::string{"hello "});

    // ----- Section 4 — try_format_runtime_n(std::span<const char>)
    SECTION("Section 4 - try_format_runtime_n(std::span<const char>)");

    // 4a. Happy path on a 32-byte buffer (not NUL-terminating — Sep 8
    //     shape).  Verifies the macro forwards correctly.
    char buf4[32];
    for (std::size_t i = 0; i < 32; ++i) buf4[i] = 'Q';
    auto r5 = TRY_FORMAT_BOUNDED_N_SPAN_FMT(
        buf4, 32, std::span<const char>{hello_fmt}, "world");
    CHECK(r5.has_value());
    CHECK_EQ(r5.value(), std::size_t{11});  // "hello world" == 11 bytes
    CHECK_EQ((std::string_view{buf4, 11}), (std::string_view{"hello world"}));
    // Pin the "no NUL" contract: bytes past written are poison.
    CHECK_EQ(buf4[11], 'Q');
    CHECK_EQ(buf4[31], 'Q');

    // 4b. TooLarge on the bounded buffer.  Same shape as Sep 8 / Sep 9.
    char buf4b[5];
    for (std::size_t i = 0; i < 5; ++i) buf4b[i] = 'Q';
    auto r6 = TRY_FORMAT_BOUNDED_N_SPAN_FMT(
        buf4b, 5, std::span<const char>{hello_fmt}, "world");
    CHECK(!r6.has_value());
    CHECK_EQ(r6.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK_EQ(r6.error().needed_bytes, std::size_t{11});
    // Pin atomicity: every byte still 'Q'.
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK_EQ(buf4b[i], 'Q');
    }

    // ----- Section 5 — try_format_runtime_n_into_bounded(std::span<const char>)
    SECTION("Section 5 - try_format_runtime_n_into_bounded(std::span<const char>)");

    // 5a. Happy path with NUL termination.  Sep 12 shape + today's
    //     std::span<const char> fmt parameter.
    char buf5[32];
    for (std::size_t i = 0; i < 32; ++i) buf5[i] = 'Q';
    auto r7 = TRY_FORMAT_BOUNDED_INTO_SPAN(
        buf5, 32, std::span<const char>{hello_fmt}, "world");
    CHECK(r7.has_value());
    CHECK_EQ(r7.value(), std::size_t{11});
    CHECK_EQ((std::string_view{buf5, 11}), (std::string_view{"hello world"}));
    CHECK_EQ(buf5[11], '\0');  // NUL placement

    // 5b. Truncation edge: cap exactly equals needed.  Sep 12
    //     contract — this is SUCCESS, NUL at cap-1.
    char buf5b[11];
    for (std::size_t i = 0; i < 11; ++i) buf5b[i] = 'Q';
    auto r8 = TRY_FORMAT_BOUNDED_INTO_SPAN(
        buf5b, 11, std::span<const char>{hello_fmt}, "world");
    CHECK(r8.has_value());
    CHECK_EQ(r8.value(), std::size_t{10});  // effective_cap = cap-1 = 10
    CHECK_EQ((std::string_view{buf5b, 10}), (std::string_view{"hello worl"}));
    CHECK_EQ(buf5b[10], '\0');

    // 5c. TooLarge: cap too small.  Buffer untouched on error path.
    char buf5c[5];
    for (std::size_t i = 0; i < 5; ++i) buf5c[i] = 'Q';
    auto r9 = TRY_FORMAT_BOUNDED_INTO_SPAN(
        buf5c, 5, std::span<const char>{hello_fmt}, "world");
    CHECK(!r9.has_value());
    CHECK_EQ(r9.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK_EQ(buf5c[i], 'Q');
    }

    // ----- Section 6 — try_format_runtime_n_into_bounded(std::span<char>,
    //                                                   std::span<const char>)
    SECTION("Section 6 - try_format_runtime_n_into_bounded(std::span<char>, std::span<const char>)");

    // 6a. Happy path on the std::span<char> output overload.  Verifies
    //     the macro dispatches to the (std::span<char>, std::span<const char>)
    //     overload, not to the (char*, size_t, std::span<const char>)
    //     overload.
    std::array<char, 32> buf6_arr;
    buf6_arr.fill('Q');
    std::span<char> buf6{buf6_arr};
    auto r10 = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN(
        buf6, std::span<const char>{hello_fmt}, "world");
    CHECK(r10.has_value());
    CHECK_EQ(r10.value(), std::size_t{11});
    CHECK_EQ((std::string_view{buf6.data(), 11}), (std::string_view{"hello world"}));
    CHECK_EQ(buf6[11], '\0');

    // 6b. Sub-span over a 16-byte buffer for the OUTPUT span.  Pins
    //     that std::span<char> does NOT bleed outside the span — Sep 10
    //     already pinned this; today's lesson pins that the same is
    //     true with the new std::span<const char> format-string overload.
    //     We allocate a 16-byte buffer with poison 'Q', place an
    //     11-byte sub-span at offset 2, format "hello world" (11 bytes)
    //     into it, hit the truncation edge (cap == needed == 11) and
    //     verify the NUL is placed at the sub-span boundary (index 10
    //     of the sub-span = byte 12 of the underlying buffer), and
    //     verify bytes [0,2) and [13,16) are still 'Q'.
    char big[16];
    for (std::size_t i = 0; i < 16; ++i) big[i] = 'Q';
    std::span<char> sub_out{big + 2, 11};
    auto r11 = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN(
        sub_out, std::span<const char>{hello_fmt}, "world");
    CHECK(r11.has_value());
    // Truncation edge (Sep 12's contract): cap == needed == 11.
    // effective_cap = cap - 1 = 10.  written = 10.  NUL at index 10.
    CHECK_EQ(r11.value(), std::size_t{10});
    // Bytes [0,2) and [13,16) untouched.
    CHECK_EQ(big[0], 'Q');
    CHECK_EQ(big[1], 'Q');
    CHECK_EQ(big[13], 'Q');
    CHECK_EQ(big[14], 'Q');
    CHECK_EQ(big[15], 'Q');
    // The first 10 bytes of the sub-span are "hello worl" (truncated).
    CHECK_EQ((std::string_view{sub_out.data(), 10}),
             (std::string_view{"hello worl"}));
    // NUL at sub_out[10] = big[12].
    CHECK_EQ(big[12], '\0');

    // ----- Section 7 — byte-exactness of the explicit conversion
    SECTION("Section 7 - byte-exactness of the explicit conversion");

    // 7a. Build a runtime format string and a span<const char> over
    //     a sub-range of it.  Verify the formatted output exactly
    //     matches what std::string_view over the same sub-range would
    //     produce — proving the conversion does NOT touch the bytes.
    std::string fmt7 = ">>> hello {} <<<";
    std::span<const char> fmt7_span{
        reinterpret_cast<const char*>(fmt7.data() + 4), 8};
    // fmt7_span points at "hello {}" (8 bytes — no trailing space).
    auto r12 = TRY_FORMAT_BOUNDED_SPAN(100, fmt7_span, "world");
    CHECK(r12.has_value());
    CHECK_EQ(r12.value(), std::string{"hello world"});

    // 7b. The std::string_view primary produces the SAME result for
    //     the same content — confirming the new overload is a verbatim
    //     forwarding wrapper with no observable behavior change.
    std::string_view fmt7_sv{fmt7.data() + 4, 8};
    auto r13 = petra::try_format_runtime_bounded(fmt7_sv, 100, "world");
    CHECK(r13.has_value());
    CHECK_EQ(r13.value(), r12.value());
    CHECK_EQ(r12.value(), r13.value());

    // ----- Section 8 — overload resolution: explicit conversion vs new overload
    SECTION("Section 8 - overload resolution: explicit conversion vs new overload");

    // 8a. The std::span<const char> -> std::string_view conversion
    //     is EXPLICIT (Section 1 pinned this).  Without today's new
    //     overload, a call site holding a std::span<const char> would
    //     be FORCED to construct a std::string_view at the call site
    //     (`std::string_view{span}`) — today's overload removes that
    //     burden by accepting the span directly.
    static_assert(!std::is_convertible_v<std::span<const char>,
                                         std::string_view>,
                  "explicit conversion confirmed (no implicit conversion)");
    static_assert(std::is_constructible_v<std::string_view,
                                          std::span<const char>>,
                  "explicit ctor exists");

    // 8b. A std::span<const char> built over a string literal is
    //     passed through both APIs.  Both produce identical results —
    //     the new overload does NOT change observable behavior.
    constexpr const char* lit = "literal {}";
    std::span<const char> lit_span{
        reinterpret_cast<const char*>(lit), 10};
    auto r14_sv = petra::try_format_runtime_bounded(
        std::string_view{lit_span.data(), lit_span.size()}, 100, "X");
    auto r14_sp = petra::try_format_runtime_bounded(lit_span, 100, "X");
    CHECK(r14_sv.has_value());
    CHECK(r14_sp.has_value());
    CHECK_EQ(r14_sv.value(), r14_sp.value());
    CHECK_EQ(r14_sp.value(), std::string{"literal X"});

    // ----- Section 9 — runtime format error via std::span<const char>
    SECTION("Section 9 - runtime format error via std::span<const char>");

    // 9a. Malformed format string ("{:" with no closing brace) is
    //     accepted by the runtime surface and returned as
    //     FormatError{Format, ...}.  Pinned via span<const char>.
    char bad[3] = {'{', ':', ' '};
    std::span<const char> bad_span{bad, 3};
    auto r15 = TRY_FORMAT_BOUNDED_SPAN(100, bad_span, 42);
    CHECK(!r15.has_value());
    CHECK_EQ(r15.error().kind, petra::FormatErrorKind::Format);

    // ----- Section 10 — 4 threads x 25 calls concurrent dispatch
    SECTION("Section 10 - 4 threads x 25 calls concurrent dispatch");

    // Each thread formats ("t={} i={} v={}", tid, i, i*7) into its
    // own std::string via TRY_FORMAT_BOUNDED_SPAN.  Each thread
    // produces a fresh runtime format string (built from std::to_string)
    // and a fresh span<const char> over that string.  100 total calls
    // across 4 threads; per-thread local counters aggregated after
    // join to avoid a data race on g_pass / g_fail.
    std::vector<std::thread> threads;
    std::atomic<int> local_pass{0};
    std::atomic<int> local_fail{0};
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([t, &local_pass, &local_fail]() {
            for (int i = 0; i < 25; ++i) {
                std::string fmt = "t=" + std::to_string(t)
                                + " i=" + std::to_string(i)
                                + " v=" + std::to_string(i * 7);
                std::span<const char> fmt_span{
                    reinterpret_cast<const char*>(fmt.data()),
                    fmt.size()};
                auto r = TRY_FORMAT_BOUNDED_SPAN(1000, fmt_span);
                std::string expected = "t=" + std::to_string(t)
                                     + " i=" + std::to_string(i)
                                     + " v=" + std::to_string(i * 7);
                if (r.has_value() && r.value() == expected) {
                    ++local_pass;
                } else {
                    ++local_fail;
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    CHECK_EQ(local_pass.load(), 100);
    CHECK_EQ(local_fail.load(), 0);
    // Per-thread result counters are aggregated AFTER the join to
    // avoid a data race on g_pass / g_fail from concurrent threads.
    g_pass += local_pass.load();
    g_fail += local_fail.load();

    // ----- Summary
    std::println("=========================================================");
    std::println("PASS: {}  FAIL: {}", g_pass, g_fail);
    std::println("=========================================================");
    return g_fail == 0 ? 0 : 1;
}
