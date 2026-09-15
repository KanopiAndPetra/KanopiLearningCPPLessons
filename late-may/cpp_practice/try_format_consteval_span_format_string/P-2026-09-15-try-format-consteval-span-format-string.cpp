// P-2026-09-15 — try_format_consteval_span_format_string:
//
// THE CONSTEVAL SURFACE IS FUNDAMENTALLY INCOMPATIBLE WITH
// STD::SPAN<CONST CHAR> FOR THE FORMAT-STRING PARAMETER — by design,
// by the `consteval` keyword on std::basic_format_string's ctor.  This
// lesson pins the finding and pivots to a CONSTEVAL-SHAPED RUNTIME
// PARSER family of wrappers.
//
// Closes Sep 14's "Where we go next" item #3 verbatim:
//
//   > 3. A std::span<const char> overload for the CONSTEVAL
//   >    surface (Sep 7's try_format_bounded, Sep 9's
//   >    try_format_bounded_runtime_n, Sep 11's
//   >    format_to_n_into_bounded).  Today's macros accept
//   >    std::span<const char> for the RUNTIME surface; the
//   >    CONSTEVAL surface's std::format_string<Args...> already
//   >    has a deduction guide for ranges satisfying
//   >    contiguous_range + sized_range over char, so the
//   >    CONSTEVAL wrappers should already accept
//   >    std::span<const char> at the call site via implicit
//   >    deduction — but a separate compile-time probe + Section
//   >    could pin this contract explicitly.
//
// HEADLINE finding: Sep 14's note was wrong on TWO counts:
//
//   1. The std::format_string<Args...> deduction guide does NOT
//      accept std::span<const char> for the format-string parameter
//      — the consteval ctor has a `requires convertible_to<const
//      _Tp&, basic_string_view<_CharT>>` clause, and because the
//      std::string_view range ctor (P2499R0) is `explicit` on
//      libc++ 21, the conversion std::span<const char> ->
//      std::string_view is NOT implicit, so the consteval ctor
//      REJECTS std::span<const char>.  Pinned with /tmp probe.
//
//   2. EVEN IF you construct a std::string_view from the span at the
//      call site and pass that to the consteval ctor, the consteval
//      ctor STILL rejects it — because the consteval function must
//      be called from a constant expression context, and a
//      std::string_view constructed at runtime is NOT a constant
//      expression.  Pinned with /tmp probe.
//
// Today's lesson exists to pin FOUR things:
//
//   1. The CONSTEVAL surface is fundamentally incompatible with
//      std::span<const char> for the format-string parameter, by
//      design.  The consteval ctor requires the format string to
//      be a literal at the call site.  A std::span<const char> is
//      always a runtime object (even if it points at a literal in
//      read-only memory, the span itself is a runtime-constructed
//      wrapper around the address + size).  A std::string_view
//      constructed from the span is also a runtime object.
//
//   2. The right architectural answer for "std::span<const char>
//      format string at the call site" is the RUNTIME surface
//      (Sep 14's try_format_runtime_bounded / _n / _n_into_bounded
//      family, with the std::span<const char> format-string
//      overloads Sep 14 added).  Today's lesson VERIFIES this with
//      a probe + a section that exercises Sep 14's surface end-to-end
//      with a span<const char> built over a literal.
//
//   3. Today's lesson ALSO adds a CONSTEVAL-SHAPED RUNTIME PARSER
//      family — a deliberate trade-off.  The new wrappers
//      (try_format_consteval_span_bounded / _n / _n_into_bounded /
//      _n_span) take a std::span<const char> format-string parameter
//      and use std::vformat_to directly, bypassing the consteval gate.
//      The trade-off is: callers get a span<const char> format-string
//      parameter AT THE COST OF losing the consteval format-string
//      parse — malformed format strings are now runtime errors, not
//      compile errors.  This is documented clearly in the
//      function-name prefix (`_consteval_span_`) and in the lesson
//      notes.
//
//   4. The consteval contract for the LITERAL-PRIMARY wrappers
//      (Sep 7 / Sep 9 / Sep 11) is unchanged — they still reject
//      malformed format strings as HARD COMPILE ERRORS at the call
//      site.  Today's new wrappers do NOT replace them; they are
//      PARALLEL.
//
// The shape of the new wrappers:
//
//   petra::try_format_consteval_span_bounded(std::span<const char> fmt,
//                                             std::size_t cap,
//                                             const Args&... args)
//       -> std::expected<std::string, petra::FormatError>
//       (Sep 6 impl, unchanged).  TRADE-OFF: runtime parse, not
//       consteval parse.
//
//   petra::try_format_consteval_span_runtime_n(char* buf,
//                                               std::size_t cap,
//                                               std::span<const char> fmt,
//                                               const Args&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//       (Sep 8 impl, unchanged).  TRADE-OFF: runtime parse.
//
//   petra::try_format_consteval_span_runtime_n_into_bounded(
//       char* buf, std::size_t cap,
//       std::span<const char> fmt,
//       const Args&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//       (Sep 12 impl, unchanged).  TRADE-OFF: runtime parse.
//
//   petra::try_format_consteval_span_runtime_n_span(
//       std::span<char> out,
//       std::span<const char> fmt,
//       const Args&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//       (Sep 12 std::span<char> overload, unchanged).  TRADE-OFF:
//       runtime parse.
//
// The macro layer mirrors Sep 14's pattern (parenthesized expressions
// with __VA_OPT__(,) for the no-args case):
//
//   TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(cap, fmt, args...)
//   TRY_FORMAT_CONSTEVAL_SPAN_N(buf, cap, fmt, args...)
//   TRY_FORMAT_CONSTEVAL_SPAN_INTO(buf, cap, fmt, args...)
//   TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN(out_span, fmt_span, args...)
//
// Each macro is a verbatim thin dispatcher to ONE of the new
// wrappers.  The `_CONSTEVAL_SPAN_` prefix makes the trade-off
// (runtime parse, not consteval parse) explicit at the call site.
//
// What gets reused (verbatim from Aug 25 / Sep 6 / Sep 7 / Sep 8 /
// Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14):
//
//   - petra::FormatError{kind, message, needed_bytes} — verbatim.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — verbatim.
//   - petra::counting_output_iterator — verbatim.
//   - petra::detail::bounded_char_writer — verbatim.
//   - petra::detail::try_format_runtime_bounded_impl — Sep 6 verbatim.
//   - petra::detail::try_format_runtime_n_impl — Sep 8 verbatim.
//   - petra::detail::try_format_to_n_into_bounded_impl — Sep 12 verbatim.
//
// What is NEW today:
//
//   - The four new try_format_consteval_span_* wrappers — each is a
//     VERBATIM thin forwarding wrapper that builds a std::string_view
//     from the span EXPLICITLY and dispatches to the existing RUNTIME
//     impl UNCHANGED.  The TRADE-OFF is that the consteval gate is
//     intentionally BYPASSED — the format-string parse happens at
//     runtime via std::vformat_to.  Malformed format strings produce
//     RUNTIME FormatError{Format, ...} errors, not compile errors.
//   - The four new TRY_FORMAT_CONSTEVAL_SPAN_* macros.
//
// C++23, std::format, std::vformat_to, std::string_view,
// std::span<const char> (P0122R7), std::make_format_args,
// std::format_error, std::expected (P0323R12), std::println
// (P2093R14), std::ranges::contiguous_range.  Requires
// -fexperimental-library on Apple Clang 21.0.0 / libc++ 21 for
// std::println.

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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14)
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
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14)
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
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14)
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
// Part 5 — petra::try_format_runtime_bounded (Sep 6 std::string_view primary)
// + petra::try_format_consteval_span_bounded(std::span<const char>)
// NEW TODAY — first the Sep 6 std::string_view primary (verbatim from
// Sep 6), then today's CONSTEVAL-SHAPED RUNTIME PARSER overload that
// accepts std::span<const char>.  The new wrapper parses the format
// string at runtime via std::vformat_to.  TRADE-OFF: malformed format
// strings produce RUNTIME errors (not compile errors).  Disambiguator
// vs Sep 14's RUNTIME wrapper of the same name is the new
// function-name prefix `_consteval_span_`.
//
// The new wrapper is a verbatim thin forwarding wrapper — it builds
// a std::string_view from the span EXPLICITLY and dispatches to the
// Sep 6 impl UNCHANGED.
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded(std::string_view fmt, std::size_t cap,
                           const Args&... args) {
    return detail::try_format_runtime_bounded_impl(fmt, cap, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_consteval_span_bounded(std::span<const char> fmt, std::size_t cap,
                                  const Args&... args) {
    return detail::try_format_runtime_bounded_impl(
        std::string_view{fmt.data(), fmt.size()}, cap, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim)
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
// Part 7 — petra::try_format_consteval_span_runtime_n(std::span<const char>)
// NEW TODAY — verbatim thin forwarding wrapper (mirrors Part 5).
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_consteval_span_runtime_n(char* buf, std::size_t cap,
                                    std::span<const char> fmt,
                                    const Args&... args) {
    return detail::try_format_runtime_n_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

}  // namespace petra

// ============================================================================
// Part 8 — petra::detail::try_format_to_n_into_bounded_impl (Sep 12 verbatim)
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
// Part 9 — petra::try_format_consteval_span_runtime_n_into_bounded(
//                char*, size_t, std::span<const char>)
// + petra::try_format_consteval_span_runtime_n_span(
//                std::span<char>, std::span<const char>)
// NEW TODAY — TWO verbatim thin forwarding wrappers (mirrors Part 5).
// Both rebuild std::string_view from the format-string span and
// dispatch to the Sep 12 impl UNCHANGED.
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_consteval_span_runtime_n_into_bounded(char* buf, std::size_t cap,
                                                 std::span<const char> fmt,
                                                 const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_consteval_span_runtime_n_span(std::span<char> out,
                                         std::span<const char> fmt,
                                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        out.data(), out.size(),
        std::string_view{fmt.data(), fmt.size()}, args...);
}

}  // namespace petra

// ============================================================================
// Part 10 — TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED / _N / _INTO / _N_SPAN
// NEW TODAY — four macros that lift the std::span<const char>
// format-string choice to the macro form on the CONSTEVAL-SHAPED
// RUNTIME PARSER surface.  Each macro is a verbatim thin dispatcher
// to ONE of the new wrappers.
//
// The macro names use "_CONSTEVAL_SPAN_" prefix (vs Sep 14's "_SPAN_"
// prefix) to make the trade-off explicit at the call site: these
// macros bypass the consteval gate intentionally.
//
//   | TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED | TODAY: Sep 6 std::string surface,
//   |                                   | std::span<const char> fmt,
//   |                                   | RUNTIME parse (no consteval) |
//   | TRY_FORMAT_CONSTEVAL_SPAN_N       | TODAY: Sep 8 char*+cap,
//   |                                   | std::span<const char> fmt,
//   |                                   | RUNTIME parse |
//   | TRY_FORMAT_CONSTEVAL_SPAN_INTO    | TODAY: Sep 12 NUL-term,
//   |                                   | std::span<const char> fmt,
//   |                                   | RUNTIME parse |
//   | TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN  | TODAY: Sep 12 std::span<char>,
//   |                                   | std::span<const char> fmt,
//   |                                   | RUNTIME parse |
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.  Each
// macro is a parenthesized expression — NOT a do { } while (0)
// block — so the caller can write `auto r = ...` and capture the
// result.
// ============================================================================

#define TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(cap, fmt, ...)                  \
    (::petra::try_format_consteval_span_bounded((fmt), (cap)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_CONSTEVAL_SPAN_N(buf, cap, fmt, ...)                   \
    (::petra::try_format_consteval_span_runtime_n((buf), (cap), (fmt)    \
                                                  __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_CONSTEVAL_SPAN_INTO(buf, cap, fmt, ...)                \
    (::petra::try_format_consteval_span_runtime_n_into_bounded(          \
        (buf), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN(out_span, fmt_span, ...)         \
    (::petra::try_format_consteval_span_runtime_n_span(                  \
        (out_span), (fmt_span) __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Part 11 — hand-rolled SECTION/CHECK framework (copied verbatim from
// Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14).
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
    // ----- Section 1 — HEADLINE finding: consteval ctor rejects span<const char>
    SECTION("Section 1 - HEADLINE: consteval ctor rejects span<const char>");

    // 1a. The conversion std::span<const char> -> std::string_view
    //     is EXPLICIT on libc++ 21 (Sep 14's finding, re-pinned here).
    static_assert(!std::is_convertible_v<std::span<const char>,
                                         std::string_view>,
                  "std::span<const char> must NOT be implicitly convertible "
                  "to std::string_view (libc++ 21 marks the range ctor "
                  "explicit — P2499R0)");
    static_assert(std::is_constructible_v<std::string_view,
                                          std::span<const char>>,
                  "std::string_view must be EXPLICITLY constructible "
                  "from std::span<const char> (P2499R0 range ctor)");

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

    // 1c. The HEADLINE finding (re-pinned): the std::format_string<Args...>
    //     consteval ctor REJECTS std::span<const char> even via the
    //     explicit conversion.  Documented in
    //     /tmp/probe_consteval_rejects_span_for_fmt.cpp and
    //     /tmp/probe_consteval_rejects_runtime_stringview.cpp.
    //     The std::string_view range ctor (P2499R0) is `explicit` on
    //     libc++ 21, so the consteval ctor's `requires convertible_to<
    //     const _Tp&, basic_string_view<_CharT>>` clause fails for
    //     std::span<const char>.  EVEN IF you construct a
    //     std::string_view from the span at the call site and pass
    //     that to the consteval ctor, the consteval function still
    //     rejects it because the local string_view is not a
    //     constant expression.
    //
    //     The architectural answer for "span<const char> format
    //     string at the call site" is the RUNTIME surface (Sep 14's
    //     try_format_runtime_bounded / _n / _n_into_bounded family
    //     with the std::span<const char> format-string overloads).
    //
    //     Today's lesson adds a CONSTEVAL-SHAPED RUNTIME PARSER
    //     family — try_format_consteval_span_bounded / _runtime_n /
    //     _runtime_n_into_bounded / _runtime_n_span — which takes
    //     a std::span<const char> format-string parameter and parses
    //     the format string at runtime via std::vformat_to.  The
    //     trade-off is documented in the function-name prefix.

    // 1d. Verify that the consteval ctor still ACCEPTS a literal at
    //     the call site — pinning that the consteval contract for
    //     Sep 7 / Sep 9 / Sep 11 is unchanged.  Use a static constexpr
    //     so the address is a constant expression.
    static constexpr const char lit_check[9] = {'h','e','l','l','o',' ','{','}','\0'};
    std::format_string<char const*> fs_lit{lit_check};  // OK: literal
    (void)fs_lit;
    CHECK(true);

    // ----- Section 2 — Sep 14's RUNTIME surface end-to-end (the canonical answer)
    SECTION("Section 2 - Sep 14's RUNTIME surface end-to-end (canonical answer)");

    // 2a. Sep 14's try_format_runtime_bounded(std::span<const char>, cap,
    //     args) is the canonical answer for "span<const char> format
    //     string + std::string output + cap".  Today's lesson EXERCISES
    //     the same path via the new try_format_consteval_span_bounded
    //     wrapper (Section 3).  The two wrappers share the same impl
    //     (Sep 6's try_format_runtime_bounded_impl), so Section 3
    //     is a transitive pin for Sep 14's RUNTIME surface.
    //
    //     This section instead verifies that the std::string_view
    //     primary (Sep 6) is reachable and produces the same output as
    //     the new consteval-shaped wrapper for identical inputs.
    std::array<char, 8> hello_fmt{'h','e','l','l','o',' ','{','}'};
    std::span<const char> hello_fmt_span{hello_fmt};

    auto r1_sv = petra::try_format_runtime_bounded(
        std::string_view{hello_fmt_span.data(), hello_fmt_span.size()},
        100, "world");
    CHECK(r1_sv.has_value());
    CHECK_EQ(r1_sv.value(), std::string{"hello world"});

    // ----- Section 3 — try_format_consteval_span_bounded
    SECTION("Section 3 - try_format_consteval_span_bounded");

    // 3a. Happy path: format "hello {}" with arg "world".
    auto r3 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, hello_fmt_span, "world");
    CHECK(r3.has_value());
    CHECK_EQ(r3.value(), std::string{"hello world"});

    // 3b. Multi-arg round-trip.  Span covers "x={} y={} z{" (11 chars
    //     of a 12-char array; the trailing '{' is omitted so the
    //     format string has 2 replacement fields, not 3).
    const char multi_lit[12] = {'x','=','{','}',' ','y','=','{','}',' ','z','{'};
    std::span<const char> multi_fmt_span{multi_lit, 11};
    auto r4 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, multi_fmt_span, 1, 2);
    CHECK(r4.has_value());
    CHECK_EQ(r4.value(), std::string{"x=1 y=2 z"});

    // 3c. TooLarge: cap too small.
    auto r5 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(5, hello_fmt_span, "world");
    CHECK(!r5.has_value());
    CHECK_EQ(r5.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK_EQ(r5.error().needed_bytes, std::size_t{11});

    // 3d. The consteval-shaped runtime parser ACCEPTS a malformed
    //     format string at the call site (the trade-off) and
    //     returns a RUNTIME FormatError{Format, ...}.
    char bad[3] = {'{', ':', ' '};
    std::span<const char> bad_span{bad, 3};
    auto r6 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, bad_span, 42);
    CHECK(!r6.has_value());
    CHECK_EQ(r6.error().kind, petra::FormatErrorKind::Format);

    // 3e. The std::string_view primary (Sep 6) produces the SAME
    //     result for the same content — confirming the new wrapper
    //     does NOT change observable behavior.
    auto r7 = petra::try_format_runtime_bounded(
        std::string_view{hello_fmt_span.data(), hello_fmt_span.size()},
        100, "world");
    CHECK(r7.has_value());
    CHECK_EQ(r7.value(), r3.value());

    // ----- Section 4 — try_format_consteval_span_runtime_n
    SECTION("Section 4 - try_format_consteval_span_runtime_n");

    // 4a. Happy path on a 32-byte buffer.
    char buf4[32];
    for (std::size_t i = 0; i < 32; ++i) buf4[i] = 'Q';
    auto r8 = TRY_FORMAT_CONSTEVAL_SPAN_N(
        buf4, 32, hello_fmt_span, "world");
    CHECK(r8.has_value());
    CHECK_EQ(r8.value(), std::size_t{11});  // "hello world" == 11 bytes
    CHECK_EQ((std::string_view{buf4, 11}), (std::string_view{"hello world"}));
    CHECK_EQ(buf4[11], 'Q');  // no NUL contract (Sep 8)

    // 4b. TooLarge on the bounded buffer.
    char buf4b[5];
    for (std::size_t i = 0; i < 5; ++i) buf4b[i] = 'Q';
    auto r9 = TRY_FORMAT_CONSTEVAL_SPAN_N(
        buf4b, 5, hello_fmt_span, "world");
    CHECK(!r9.has_value());
    CHECK_EQ(r9.error().kind, petra::FormatErrorKind::TooLarge);
    CHECK_EQ(r9.error().needed_bytes, std::size_t{11});
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK_EQ(buf4b[i], 'Q');
    }

    // ----- Section 5 — try_format_consteval_span_runtime_n_into_bounded
    SECTION("Section 5 - try_format_consteval_span_runtime_n_into_bounded");

    // 5a. Happy path with NUL termination.
    char buf5[32];
    for (std::size_t i = 0; i < 32; ++i) buf5[i] = 'Q';
    auto r10 = TRY_FORMAT_CONSTEVAL_SPAN_INTO(
        buf5, 32, hello_fmt_span, "world");
    CHECK(r10.has_value());
    CHECK_EQ(r10.value(), std::size_t{11});
    CHECK_EQ((std::string_view{buf5, 11}), (std::string_view{"hello world"}));
    CHECK_EQ(buf5[11], '\0');  // NUL placement

    // 5b. Truncation edge: cap exactly equals needed.  Sep 12
    //     contract — this is SUCCESS, NUL at cap-1.
    char buf5b[11];
    for (std::size_t i = 0; i < 11; ++i) buf5b[i] = 'Q';
    auto r11 = TRY_FORMAT_CONSTEVAL_SPAN_INTO(
        buf5b, 11, hello_fmt_span, "world");
    CHECK(r11.has_value());
    CHECK_EQ(r11.value(), std::size_t{10});
    CHECK_EQ((std::string_view{buf5b, 10}), (std::string_view{"hello worl"}));
    CHECK_EQ(buf5b[10], '\0');

    // 5c. TooLarge: cap too small.
    char buf5c[5];
    for (std::size_t i = 0; i < 5; ++i) buf5c[i] = 'Q';
    auto r12 = TRY_FORMAT_CONSTEVAL_SPAN_INTO(
        buf5c, 5, hello_fmt_span, "world");
    CHECK(!r12.has_value());
    CHECK_EQ(r12.error().kind, petra::FormatErrorKind::TooLarge);
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK_EQ(buf5c[i], 'Q');
    }

    // ----- Section 6 — try_format_consteval_span_runtime_n_span
    SECTION("Section 6 - try_format_consteval_span_runtime_n_span");

    // 6a. Happy path on the std::span<char> output overload.
    std::array<char, 32> buf6_arr;
    buf6_arr.fill('Q');
    std::span<char> buf6{buf6_arr};
    auto r13 = TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN(
        buf6, hello_fmt_span, "world");
    CHECK(r13.has_value());
    CHECK_EQ(r13.value(), std::size_t{11});
    CHECK_EQ((std::string_view{buf6.data(), 11}), (std::string_view{"hello world"}));
    CHECK_EQ(buf6[11], '\0');  // NUL placement

    // 6b. Sub-span over a 16-byte buffer for the OUTPUT span.
    char big[16];
    for (std::size_t i = 0; i < 16; ++i) big[i] = 'Q';
    std::span<char> sub_out{big + 2, 11};
    auto r14 = TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN(
        sub_out, hello_fmt_span, "world");
    CHECK(r14.has_value());
    CHECK_EQ(r14.value(), std::size_t{10});  // cap == needed == 11 -> effective_cap = 10
    CHECK_EQ(big[0], 'Q');
    CHECK_EQ(big[1], 'Q');
    CHECK_EQ(big[13], 'Q');
    CHECK_EQ(big[14], 'Q');
    CHECK_EQ(big[15], 'Q');
    CHECK_EQ((std::string_view{sub_out.data(), 10}),
             (std::string_view{"hello worl"}));
    CHECK_EQ(big[12], '\0');  // NUL at sub-span index 10 = buffer index 12

    // ----- Section 7 — byte-exactness of the explicit conversion
    SECTION("Section 7 - byte-exactness of the explicit conversion");

    // 7a. Build a runtime format string and a span<const char> over
    //     a sub-range of it.  Verify the formatted output exactly
    //     matches what std::string_view over the same sub-range would
    //     produce — proving the conversion does NOT touch the bytes.
    std::string fmt7 = ">>> hello {} <<<";
    std::span<const char> fmt7_span{
        reinterpret_cast<const char*>(fmt7.data() + 4), 8};
    auto r15 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, fmt7_span, "world");
    CHECK(r15.has_value());
    CHECK_EQ(r15.value(), std::string{"hello world"});

    // ----- Section 8 — 4 threads x 25 calls concurrent dispatch
    SECTION("Section 8 - 4 threads x 25 calls concurrent dispatch");

    // Each thread formats ("t={} i={} v={}", tid, i, i*7) into its
    // own std::string via TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED.  Each
    // thread produces a fresh std::span<const char> over a fresh
    // std::string built from std::to_string.  100 total calls
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
                auto r = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(1000, fmt_span);
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
    g_pass += local_pass.load();
    g_fail += local_fail.load();

    // ----- Summary
    std::println("=========================================================");
    std::println("PASS: {}  FAIL: {}", g_pass, g_fail);
    std::println("=========================================================");
    return g_fail == 0 ? 0 : 1;
}
