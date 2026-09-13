// P-2026-09-13 — try_format_bounded_macros: a MACRO LAYER that lifts the
// CONSTEVAL vs RUNTIME sibling choice at the call site for all the
// bounded-format wrappers built up across Aug 25 / Sep 6 / Sep 7 /
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12.  Closes Sep 12's "Where we
// go next" item #2 verbatim:
//
//   > petra::try_format_expected_bounded macro layer — analogous to
//   > LOG_INFO_RUNTIME from Sep 1 / SYNC_LOG_INFO from Sep 2: a
//   > TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt, args...) macro that
//   > lifts the consteval vs runtime sibling choice to a macro form.
//
// Today's lesson exists to pin three things:
//
//   1. The CONSTEVAL vs RUNTIME sibling distinction is the same
//      pattern Sep 1 vs Sep 2 established for logging macros
//      (LOG_INFO vs LOG_INFO_RUNTIME): the consteval surface catches
//      malformed format strings as HARD COMPILE ERRORS via
//      std::format_string<Args...>'s consteval ctor; the runtime
//      surface accepts std::string_view and catches std::format_error
//      at format time.  The MACRO is a dispatch layer the caller can
//      pick at the call site without naming the underlying function.
//
//   2. The macro layer covers all FOUR bounded-format axes built up
//      across the arc:
//        - BOUNDED (std::string output): Sep 6 runtime + Sep 7 consteval.
//        - BOUNDED_N (char*+cap output): Sep 8 runtime + Sep 9 consteval.
//        - BOUNDED_INTO (char*+cap NUL-terminating): Sep 12 runtime +
//          Sep 11 consteval.
//      Plus the std::span<char> overloads for the BOUNDED_N family.
//
//   3. The macro layer is a VERBATIM thin dispatcher — no new impls,
//      no new types, no new error contracts.  Each macro expands to
//      EXACTLY ONE existing petra::try_format_* call.  The lesson is
//      a thin syntax-sugar surface over the seven wrappers already
//      shipped.
//
// The shape:
//
//   TRY_FORMAT_BOUNDED(cap, fmt, args...)
//       -> std::expected<std::string, petra::FormatError>
//       expands to petra::try_format_bounded<Args...>(cap, fmt, args...)
//       (Sep 7 — CONSTEVAL surface; std::format_string<Args...> fmt).
//
//   TRY_FORMAT_BOUNDED_RUNTIME(cap, fmt, args)
//       -> std::expected<std::string, petra::FormatError>
//       expands to petra::try_format_runtime_bounded<Args...>(cap,
//       fmt, args) (Sep 6 — RUNTIME surface; std::string_view fmt +
//       std::format_args args).
//
//   TRY_FORMAT_BOUNDED_N(buf, cap, fmt, args...)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to petra::try_format_bounded_runtime_n<Args...>(buf,
//       cap, fmt, args...) (Sep 9 — CONSTEVAL char*+cap surface).
//
//   TRY_FORMAT_BOUNDED_RUNTIME_N(buf, cap, fmt, args)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to petra::try_format_runtime_n<Args...>(buf, cap,
//       fmt, args) (Sep 8 — RUNTIME char*+cap surface; NO NUL).
//
//   TRY_FORMAT_BOUNDED_INTO(buf, cap, fmt, args...)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to petra::format_to_n_into_bounded<Args...>(buf, cap,
//       fmt, args...) (Sep 11 — CONSTEVAL NUL-terminating char*+cap
//       surface).
//
//   TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, cap, fmt, args)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to petra::try_format_runtime_n_into_bounded<Args...>
//       (buf, cap, fmt, args) (Sep 12 — RUNTIME NUL-terminating
//       char*+cap surface).
//
//   TRY_FORMAT_BOUNDED_N_SPAN(span, fmt, args...)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to petra::try_format_bounded_runtime_n_span<Args...>
//       (span, fmt, args...) (Sep 10 — CONSTEVAL std::span<char>
//       char*+cap surface).
//
//   TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(span, fmt, args)
//       -> std::expected<std::size_t, petra::FormatError>
//       expands to petra::try_format_runtime_n_into_bounded<Args...>
//       (span, fmt, args) (Sep 12's span<char> overload — RUNTIME
//       NUL-terminating std::span<char> surface).
//
// The consteval-vs-runtime sibling split mirrors Sep 1's
// LOG_INFO / LOG_INFO_RUNTIME pattern at the call site:
//
//   | LOG_INFO          | CONSTEVAL | std::format_string<Args...> |
//   | LOG_INFO_RUNTIME  | RUNTIME   | std::string_view + format_args |
//
// Today's lesson ports that pattern to the bounded-format wrappers:
//
//   | TRY_FORMAT_BOUNDED                | CONSTEVAL | Sep 7's try_format_bounded |
//   | TRY_FORMAT_BOUNDED_RUNTIME        | RUNTIME   | Sep 6's try_format_runtime_bounded |
//   | TRY_FORMAT_BOUNDED_N              | CONSTEVAL | Sep 9's try_format_bounded_runtime_n |
//   | TRY_FORMAT_BOUNDED_RUNTIME_N      | RUNTIME   | Sep 8's try_format_runtime_n |
//   | TRY_FORMAT_BOUNDED_INTO           | CONSTEVAL | Sep 11's format_to_n_into_bounded |
//   | TRY_FORMAT_BOUNDED_RUNTIME_INTO   | RUNTIME   | Sep 12's try_format_runtime_n_into_bounded |
//   | TRY_FORMAT_BOUNDED_N_SPAN         | CONSTEVAL | Sep 10's try_format_bounded_runtime_n_span |
//   | TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN | RUNTIME   | Sep 12's span<char> overload |
//
// What gets reused (verbatim from Aug 25 / Sep 6 / Sep 7 / Sep 8 /
// Sep 9 / Sep 10 / Sep 11 / Sep 12):
//
//   - petra::FormatError{kind, message, needed_bytes} — verbatim.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — verbatim.
//   - petra::try_format_runtime_bounded — Sep 6 verbatim.
//   - petra::try_format_bounded — Sep 7 verbatim.
//   - petra::try_format_runtime_n — Sep 8 verbatim.
//   - petra::try_format_bounded_runtime_n — Sep 9 verbatim.
//   - petra::try_format_bounded_runtime_n_span — Sep 10 verbatim.
//   - petra::format_to_n_into_bounded — Sep 11 verbatim.
//   - petra::try_format_runtime_n_into_bounded — Sep 12 verbatim.
//
// What is NEW today:
//
//   - The eight TRY_FORMAT_BOUNDED_* macros — each is a VERBATIM
//     thin dispatcher to one of the eight wrappers above.
//
// The consteval gate fires at std::basic_format_string's ctor.  A
// separate compile probe (saved in
// /tmp/probe_macro_consteval_rejects_unterminated.cpp) produces this
// hard error on TRY_FORMAT_BOUNDED(100, "{:", 42):
//
//   error: call to consteval function
//          'std::basic_format_string<char, int>::basic_format_string<char[3]>'
//          is not a constant expression
//   note:  non-constexpr function '__throw_format_error' cannot be
//          used in a constant expression
//   note:  __throw_format_error("The replacement field misses a
//          terminating '}'")
//
// The corresponding probe against the runtime sibling
// (/tmp/probe_macro_runtime_accepts_unterminated.cpp) COMPILES
// CLEANLY and returns FormatError{Format, ...} at runtime.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14), std::span<char> (P0122R7),
// std::ranges::contiguous_range.  Requires -fexperimental-library on
// Apple Clang 21.0.0 / libc++ 21 for std::println.

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
// Sep 10 / Sep 11 / Sep 12)
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
// Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12)
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
// Sep 10 / Sep 11 / Sep 12)
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
// Part 4 — try_format_runtime_bounded (Sep 6 verbatim, the RUNTIME
// std::string-output wrapper).
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
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }
    out.resize(written);
    return out;
}

}  // namespace detail

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded(std::size_t cap, std::string_view fmt,
                           const Args&... args) {
    return detail::try_format_runtime_bounded_impl(fmt, cap, args...);
}

}  // namespace petra

// ============================================================================
// Part 5 — try_format_bounded (Sep 7 verbatim, the CONSTEVAL
// std::string-output wrapper).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_bounded_impl(std::format_string<Args...> fmt, std::size_t cap,
                        const Args&... args) {
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
    out.resize(written);
    return out;
}

}  // namespace detail

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_bounded(std::size_t cap, std::format_string<Args...> fmt,
                   const Args&... args) {
    return detail::try_format_bounded_impl(fmt, cap, args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — try_format_runtime_n (Sep 8 verbatim, the RUNTIME
// char*+cap wrapper — NO NUL).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt,
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
        bounded_char_writer sink{out, needed, cb, &written};
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

}  // namespace detail

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap,
                     std::string_view fmt, const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 7 — try_format_bounded_runtime_n (Sep 9 verbatim, the CONSTEVAL
// char*+cap wrapper — NO NUL).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt,
                                  const Args&... args) {
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
                            + " bytes; cap is "
                            + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, needed, cb, &written};
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

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 8 — try_format_bounded_runtime_n_span (Sep 10 verbatim, the
// CONSTEVAL std::span<char> char*+cap wrapper — NO NUL).
// ============================================================================

namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_span(std::span<char> out,
                                  std::format_string<Args...> fmt,
                                  const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out.data(),
                                                     out.size(),
                                                     fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 9 — format_to_n_into_bounded (Sep 11 verbatim, the CONSTEVAL
// NUL-terminating char*+cap wrapper).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_to_n_into_bounded_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt,
                                  const Args&... args) {
    // Pass 1: measure.
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

    // cap == 0: degenerate.
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
                            + " bytes; cap is "
                            + std::to_string(cap)
                            + " (effective writable is "
                            + std::to_string(cap - 1)
                            + ")",
                        needed});
    }

    const std::size_t effective_cap = cap - 1;

    // Truncation edge: needed == cap.
    if (needed == cap) {
        std::size_t written = 0;
        try {
            bounded_char_writer sink{out, effective_cap, cb, &written};
            (void)std::vformat_to(std::move(sink), fmt.get(),
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

    // needed < cap: full success.
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

}  // namespace detail

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(char* out, std::size_t cap,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(out, cap, fmt, args...);
}

}  // namespace petra

// ============================================================================
// Part 10 — try_format_runtime_n_into_bounded (Sep 12 verbatim, the
// RUNTIME NUL-terminating char*+cap wrapper, plus its std::span<char>
// overload).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded_impl(char* out, std::size_t cap,
                                       std::string_view fmt,
                                       const Args&... args) {
    // Pass 1: measure.
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

    // cap == 0: degenerate.
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
                            + " bytes; cap is "
                            + std::to_string(cap)
                            + " (effective writable is "
                            + std::to_string(cap - 1)
                            + ")",
                        needed});
    }

    const std::size_t effective_cap = cap - 1;

    // Truncation edge: needed == cap.
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

    // needed < cap: full success.
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

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* out, std::size_t cap,
                                  std::string_view fmt,
                                  const Args&... args) {
    return detail::try_format_runtime_n_into_bounded_impl(out, cap,
                                                          fmt, args...);
}

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
// Part 11 — THE MACRO LAYER (NEW today).
//
// Eight macros, each a verbatim thin dispatcher to one of the eight
// wrappers above.  The CONSTEVAL macros expand to a call site that
// takes std::format_string<Args...> (consteval parse); the RUNTIME
// macros expand to a call site that takes std::string_view (runtime
// parse).
//
// The CONSTEVAL macros REQUIRE a string literal at the call site
// (because std::format_string<Args...> has a consteval ctor).  The
// RUNTIME macros ACCEPT a runtime-built std::string_view at the call
// site (because std::string_view has no compile-time gate).
//
// Each macro is wrapped in do { ... } while (0) for if/else hygiene,
// mirroring the Sep 1 / Sep 2 / Sep 3 / Sep 4 macro pattern.
//
// The CONSTEVAL macros DO NOT wrap their consteval-parse call site
// in a try/catch — the parse error becomes a HARD COMPILE ERROR via
// std::basic_format_string's consteval ctor (same finding Sep 7 /
// Sep 9 / Sep 10 / Sep 11 documented).
//
// The RUNTIME macros DO wrap their call site in a try/catch — the
// std::format_error thrown at format time is converted to
// FormatError{Format, ...} by the underlying wrapper.
//
// The macros use __VA_OPT__ to handle the no-args case (C++20 /
// C++23).  Without __VA_OPT__, a macro like TRY_FORMAT_BOUNDED(cap,
// "hi") would expand to a one-arg call which the underlying wrapper
// would reject — __VA_OPT__ provides a clean "" fallback so the
// zero-arg overload is reachable.
// ============================================================================

// ----- CONSTEVAL surface: std::format_string<Args...> fmt -------------------
//
// The CONSTEVAL macros take (cap [or buf,cap or span], fmt, args...)
// where fmt MUST be a string literal at the call site (because
// std::format_string<Args...>'s ctor is consteval) and args... are
// the variadic replacement-field args.  The macros use __VA_OPT__ to
// handle the no-args case (a format with no replacement fields).
// Without __VA_OPT__, a macro like TRY_FORMAT_BOUNDED(cap, "hi")
// would expand to petra::try_format_bounded(cap, "hi", ) -- which
// has a trailing comma and is a syntax error.  __VA_OPT__(,)
// provides the comma only when __VA_ARGS__ is non-empty.
//
// Each macro is a parenthesized expression — NOT a do { } while (0)
// block.  This lets the caller write `auto r = TRY_FORMAT_BOUNDED(...);`
// and capture the result.  The downside is that the macros cannot be
// used as the body of an if/else without an extra { ... } around them.
// The user can wrap them in { ... } if needed (e.g.
// `if (cond) { TRY_FORMAT_BOUNDED_RUNTIME(...); } else { ... }`).

#define TRY_FORMAT_BOUNDED(cap, fmt, ...)                                  \
    (::petra::try_format_bounded((cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_N(buf, cap, fmt, ...)                           \
    (::petra::try_format_bounded_runtime_n((buf), (cap), (fmt)            \
                                           __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_INTO(buf, cap, fmt, ...)                        \
    (::petra::format_to_n_into_bounded((buf), (cap), (fmt)                \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_N_SPAN(span, fmt, ...)                          \
    (::petra::try_format_bounded_runtime_n_span((span), (fmt)             \
                                                __VA_OPT__(,) __VA_ARGS__))

// ----- RUNTIME surface: std::string_view fmt + variadic args -------------
//
// The RUNTIME macros take (cap [or buf,cap or span], fmt, args...)
// where fmt is std::string_view and args... is variadic.  The macros
// forward the args directly to the underlying wrapper -- the
// underlying wrapper takes `const Args&...` and calls
// std::make_format_args(args...) INSIDE the function body, where the
// args become lvalues via the const-ref parameter binding.
//
// The macros use __VA_OPT__ to handle the no-args case (a format with
// no replacement fields).  Without __VA_OPT__, a macro like
// TRY_FORMAT_BOUNDED_RUNTIME(cap, "hi") would expand to
// petra::try_format_runtime_bounded(cap, "hi") -- which is correct --
// but TRY_FORMAT_BOUNDED_RUNTIME_N(buf, cap, "hi") would expand to
// petra::try_format_runtime_n(buf, cap, "hi", ) -- which has a
// trailing comma and is a syntax error.  __VA_OPT__(,) provides the
// comma only when __VA_ARGS__ is non-empty.

#define TRY_FORMAT_BOUNDED_RUNTIME(cap, fmt, ...)                          \
    (::petra::try_format_runtime_bounded((cap), (fmt) __VA_OPT__(,)        \
                                         __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N(buf, cap, fmt, ...)                   \
    (::petra::try_format_runtime_n((buf), (cap), (fmt) __VA_OPT__(,)       \
                                   __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, cap, fmt, ...)                \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt)        \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(span, fmt, ...)                  \
    (::petra::try_format_runtime_n_into_bounded((span), (fmt) __VA_OPT__(,)\
                                                __VA_ARGS__))

// ============================================================================
// Part 12 — hand-rolled SECTION/CHECK framework (copied verbatim from Aug 28
// / 30 / 31, Sep 1, Sep 2, Sep 3, Sep 4, Sep 5, Sep 6, Sep 7, Sep 8, Sep 9,
// Sep 10, Sep 11, Sep 12).
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
// Part 13 — sections
// ============================================================================

// poison_buf(N): returns a vector of N bytes all set to 'Q' so any
// unintended write is detectable.
static std::vector<char> poison_buf(std::size_t n) {
    std::vector<char> v(n, 'Q');
    return v;
}

// ------------------------------------------------------------------
// Section 1 — toolchain + feature probes
// ------------------------------------------------------------------
// Pins down the libc++ 21 feature surface the macros depend on:
// __cpp_lib_format, __cpp_lib_expected, __cpp_lib_span, and the
// FormatError return-type surface.

void section_1_sizeof_and_probes() {
    SECTION("1.sizeof+probes");
    CHECK(__cpp_lib_format == 202110);
    CHECK(__cpp_lib_expected == 202211);
    CHECK(__cpp_lib_span == 202002);
    CHECK(sizeof(std::size_t) == 8);
    CHECK(std::is_default_constructible_v<petra::FormatError>);
    CHECK(std::is_nothrow_move_constructible_v<petra::FormatError>);
    CHECK(static_cast<unsigned char>(petra::FormatErrorKind::FormatParse) == 0);
    CHECK(static_cast<unsigned char>(petra::FormatErrorKind::FormatArgument) == 1);
    CHECK(static_cast<unsigned char>(petra::FormatErrorKind::Format) == 2);
    CHECK(static_cast<unsigned char>(petra::FormatErrorKind::TooLarge) == 3);
}

// ------------------------------------------------------------------
// Section 2 — TRY_FORMAT_BOUNDED_RUNTIME (RUNTIME std::string surface)
// ------------------------------------------------------------------
// Verifies the Sep 6 sibling is reachable through the runtime macro
// with a runtime-built fmt and a runtime-built arg.

void section_2_runtime_string_basic() {
    SECTION("2.runtime-string-basic");
    auto r = TRY_FORMAT_BOUNDED_RUNTIME(64, std::string_view{"v={}"},
                                        42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == "v=42");
}

// ------------------------------------------------------------------
// Section 3 — TRY_FORMAT_BOUNDED (CONSTEVAL std::string surface)
// ------------------------------------------------------------------
// Verifies the Sep 7 sibling is reachable through the consteval
// macro with a literal fmt.  Returns the same shape as Sep 6.

void section_3_consteval_string_basic() {
    SECTION("3.consteval-string-basic");
    auto r = TRY_FORMAT_BOUNDED(64, "v={}", 42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == "v=42");
}

// ------------------------------------------------------------------
// Section 4 — runtime macro accepts dynamic format strings
// ------------------------------------------------------------------
// The HEADLINE use case for the runtime surface: build the format
// string dynamically.  Sep 7's consteval wrapper would reject this
// at compile time (no literal).  The runtime wrapper accepts it.

void section_4_runtime_dynamic_fmt() {
    SECTION("4.runtime-dynamic-fmt");
    std::string dynamic_fmt = std::string{"v="} + std::to_string(42);
    auto r = TRY_FORMAT_BOUNDED_RUNTIME(64,
                                        std::string_view{dynamic_fmt},
                                        42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == "v=42");

    std::string dynamic_fmt2 = std::string{"hello "} + "world";
    auto r2 = TRY_FORMAT_BOUNDED_RUNTIME(64,
                                         std::string_view{dynamic_fmt2},
                                         42);
    CHECK(r2.has_value());
    CHECK(r2.has_value() && r2.value() == "hello world");
}

// ------------------------------------------------------------------
// Section 5 — TRY_FORMAT_BOUNDED_N (CONSTEVAL char*+cap surface)
// ------------------------------------------------------------------
// Verifies the Sep 9 sibling is reachable through the consteval
// macro with a literal fmt and a char*+cap buffer.  No NUL.

void section_5_consteval_n_basic() {
    SECTION("5.consteval-n-basic");
    char buf[16] = {};
    auto r = TRY_FORMAT_BOUNDED_N(buf, 16, "v={}", 42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == 4);
    CHECK((std::string_view{buf, 4} == "v=42"));
    // No NUL on success path -- Sep 9's contract (zero-init buffer).
    CHECK(buf[4] == '\0');
}

// ------------------------------------------------------------------
// Section 6 — TRY_FORMAT_BOUNDED_RUNTIME_N (RUNTIME char*+cap surface)
// ------------------------------------------------------------------
// Verifies the Sep 8 sibling is reachable through the runtime macro
// with a runtime-built fmt and a char*+cap buffer.  No NUL.

void section_6_runtime_n_basic() {
    SECTION("6.runtime-n-basic");
    auto buf = poison_buf(16);
    auto r = TRY_FORMAT_BOUNDED_RUNTIME_N(buf.data(), 16,
                                          std::string_view{"v={}"},
                                          42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == 4);
    CHECK((std::string_view{buf.data(), 4} == "v=42"));
    // No NUL on success path -- Sep 8's contract.
    CHECK(buf[4] == 'Q');
}

// ------------------------------------------------------------------
// Section 7 — TRY_FORMAT_BOUNDED_INTO (CONSTEVAL NUL-terminating
// char*+cap surface).
// ------------------------------------------------------------------
// Verifies the Sep 11 sibling is reachable through the consteval
// macro with a literal fmt and a char*+cap buffer.  NUL on success.

void section_7_consteval_into_basic() {
    SECTION("7.consteval-into-basic");
    char buf[16] = {};
    auto r = TRY_FORMAT_BOUNDED_INTO(buf, 16, "v={}", 42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == 4);
    CHECK((std::string_view{buf, 4} == "v=42"));
    CHECK(buf[4] == '\0');
}

// ------------------------------------------------------------------
// Section 8 — TRY_FORMAT_BOUNDED_RUNTIME_INTO (RUNTIME NUL-terminating
// char*+cap surface).
// ------------------------------------------------------------------
// Verifies the Sep 12 sibling is reachable through the runtime
// macro with a runtime-built fmt and a char*+cap buffer.  NUL on
// success.

void section_8_runtime_into_basic() {
    SECTION("8.runtime-into-basic");
    auto buf = poison_buf(16);
    auto r = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf.data(), 16,
                                             std::string_view{"v={}"},
                                             42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == 4);
    CHECK((std::string_view{buf.data(), 4} == "v=42"));
    CHECK(buf[4] == '\0');
}

// ------------------------------------------------------------------
// Section 9 — TRY_FORMAT_BOUNDED_N_SPAN (CONSTEVAL std::span<char>
// char*+cap surface, NO NUL).
// ------------------------------------------------------------------
// Verifies the Sep 10 sibling is reachable through the consteval
// macro with a literal fmt and a std::span<char> buffer.  No NUL.

void section_9_consteval_span_basic() {
    SECTION("9.consteval-span-basic");
    char buf[16] = {};
    std::span<char> sp{buf, 16};
    auto r = TRY_FORMAT_BOUNDED_N_SPAN(sp, "v={}", 42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == 4);
    CHECK((std::string_view{buf, 4} == "v=42"));
    // No NUL on success path -- Sep 10's contract.
    CHECK(buf[4] == '\0');  // zero-init, not macro-written
}

// ------------------------------------------------------------------
// Section 10 — TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN (RUNTIME
// NUL-terminating std::span<char> surface).
// ------------------------------------------------------------------
// Verifies the Sep 12 std::span<char> overload is reachable through
// the runtime macro with a runtime-built fmt.  NUL on success.

void section_10_runtime_span_basic() {
    SECTION("10.runtime-span-basic");
    auto buf = poison_buf(16);
    std::span<char> sp{buf.data(), 16};
    auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(sp,
                                               std::string_view{"v={}"},
                                               42);
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == 4);
    CHECK((std::string_view{buf.data(), 4} == "v=42"));
    CHECK(buf[4] == '\0');
}

// ------------------------------------------------------------------
// Section 11 — atomicity on TooLarge: every macro preserves the
// caller's buffer untouched on the failure path.
// ------------------------------------------------------------------
// Pre-poison the buffer; call the macro with a TooLarge-triggering
// format; verify the buffer is still poisoned.

void section_11_atomicity_on_too_large() {
    SECTION("11.atomicity-on-too-large");

    // TRY_FORMAT_BOUNDED_RUNTIME_N: pre-poison → TooLarge → buffer
    // untouched.
    {
        auto buf = poison_buf(4);
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N(buf.data(), 3,
                                              std::string_view{"v={}"},
                                              42);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        for (std::size_t i = 0; i < 4; ++i) CHECK(buf[i] == 'Q');
    }

    // TRY_FORMAT_BOUNDED_N: pre-poison → TooLarge → buffer untouched.
    {
        auto buf = poison_buf(4);
        auto r = TRY_FORMAT_BOUNDED_N(buf.data(), 3, "v={}", 42);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        for (std::size_t i = 0; i < 4; ++i) CHECK(buf[i] == 'Q');
    }

    // TRY_FORMAT_BOUNDED_INTO: pre-poison → TooLarge → buffer
    // untouched.
    {
        auto buf = poison_buf(4);
        auto r = TRY_FORMAT_BOUNDED_INTO(buf.data(), 3, "v={}", 42);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        for (std::size_t i = 0; i < 4; ++i) CHECK(buf[i] == 'Q');
    }

    // TRY_FORMAT_BOUNDED_RUNTIME_INTO: pre-poison → TooLarge → buffer
    // untouched.
    {
        auto buf = poison_buf(4);
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf.data(), 3,
                                                 std::string_view{"v={}"},
                                                 42);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        for (std::size_t i = 0; i < 4; ++i) CHECK(buf[i] == 'Q');
    }
}

// ------------------------------------------------------------------
// Section 12 — truncation edge on the NUL-terminating macros
// ------------------------------------------------------------------
// cap == needed exactly → truncation-success.  The NUL-terminating
// macros write cap-1 bytes and NUL at cap-1; the no-NUL macros
// return needed bytes written and leave the rest untouched.

void section_12_truncation_edge() {
    SECTION("12.truncation-edge");

    // TRY_FORMAT_BOUNDED_INTO: cap=5, "ABCDE" needs 5 → truncation
    // edge, r.value()=4, NUL at buf[4].
    {
        auto buf = poison_buf(5);
        auto r = TRY_FORMAT_BOUNDED_INTO(buf.data(), 5, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(buf[0] == 'A');
        CHECK(buf[1] == 'B');
        CHECK(buf[2] == 'C');
        CHECK(buf[3] == 'D');
        CHECK(buf[4] == '\0');
    }

    // TRY_FORMAT_BOUNDED_RUNTIME_INTO: cap=5, "ABCDE" needs 5 →
    // truncation edge, r.value()=4, NUL at buf[4].
    {
        auto buf = poison_buf(5);
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf.data(), 5,
                                                 std::string_view{"ABCDE"},
                                                 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(buf[0] == 'A');
        CHECK(buf[1] == 'B');
        CHECK(buf[2] == 'C');
        CHECK(buf[3] == 'D');
        CHECK(buf[4] == '\0');
    }

    // TRY_FORMAT_BOUNDED_N: cap=5, "ABCDE" needs 5 → r.value()=5, no
    // NUL written.
    {
        auto buf = poison_buf(5);
        auto r = TRY_FORMAT_BOUNDED_N(buf.data(), 5, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        CHECK((std::string_view{buf.data(), 5} == "ABCDE"));
    }

    // TRY_FORMAT_BOUNDED_RUNTIME_N: cap=5, "ABCDE" needs 5 →
    // r.value()=5, no NUL written.
    {
        auto buf = poison_buf(5);
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N(buf.data(), 5,
                                              std::string_view{"ABCDE"},
                                              42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        CHECK((std::string_view{buf.data(), 5} == "ABCDE"));
    }
}

// ------------------------------------------------------------------
// Section 13 — runtime format-error contract: malformed format
// strings caught at runtime as FormatError{Format, e.what()}.
// ------------------------------------------------------------------
// Pinned by a /tmp probe saved outside the working tree:
// /tmp/probe_macro_consteval_rejects_unterminated.cpp -- produces a
// hard compile error on the CONSTEVAL macro with a malformed fmt.
// /tmp/probe_macro_runtime_accepts_unterminated.cpp -- COMPILES
// CLEANLY and prints a runtime FormatError{Format, ...} on the
// RUNTIME macro with the same malformed fmt.

void section_13_runtime_format_error() {
    SECTION("13.runtime-format-error");

    // TRY_FORMAT_BOUNDED_RUNTIME: malformed fmt → FormatError{Format,
    // ...} at runtime.
    char buf[16] = {};
    auto r = TRY_FORMAT_BOUNDED_RUNTIME(16, std::string_view{"hello {"},
                                        42);
    CHECK(!r.has_value());
    CHECK(!r.has_value() && r.error().kind
          == petra::FormatErrorKind::Format);
    CHECK(!r.has_value() && r.error().message.find("'{") != std::string::npos);
    // Buffer untouched.
    CHECK(buf[0] == '\0');

    // TRY_FORMAT_BOUNDED_RUNTIME_N: malformed fmt → FormatError at
    // runtime.
    auto buf2 = poison_buf(16);
    auto r2 = TRY_FORMAT_BOUNDED_RUNTIME_N(buf2.data(), 16,
                                           std::string_view{"hello {"},
                                           42);
    CHECK(!r2.has_value());
    CHECK(!r2.has_value() && r2.error().kind
          == petra::FormatErrorKind::Format);
    // Buffer untouched.
    for (std::size_t i = 0; i < 16; ++i) CHECK(buf2[i] == 'Q');

    // TRY_FORMAT_BOUNDED_RUNTIME_INTO: malformed fmt → FormatError
    // at runtime.
    auto buf3 = poison_buf(16);
    auto r3 = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf3.data(), 16,
                                              std::string_view{"hello {"},
                                              42);
    CHECK(!r3.has_value());
    CHECK(!r3.has_value() && r3.error().kind
          == petra::FormatErrorKind::Format);
    // Buffer untouched.
    for (std::size_t i = 0; i < 16; ++i) CHECK(buf3[i] == 'Q');

    // TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN: malformed fmt → FormatError
    // at runtime.
    auto buf4 = poison_buf(16);
    std::span<char> sp4{buf4.data(), 16};
    auto r4 = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(sp4,
                                                std::string_view{"hello {"},
                                                42);
    CHECK(!r4.has_value());
    CHECK(!r4.has_value() && r4.error().kind
          == petra::FormatErrorKind::Format);
    // Buffer untouched.
    for (std::size_t i = 0; i < 16; ++i) CHECK(buf4[i] == 'Q');
}

// ------------------------------------------------------------------
// Section 14 — return type verification
// ------------------------------------------------------------------
// Each macro produces the EXACT same return type as the underlying
// wrapper.  Pinned by decltype checks at the call site.

void section_14_return_type_verification() {
    SECTION("14.return-type-verification");

    // TRY_FORMAT_BOUNDED → std::expected<std::string, petra::FormatError>.
    {
        auto r = TRY_FORMAT_BOUNDED(16, "{}", 42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::string,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_RUNTIME → std::expected<std::string,
    // petra::FormatError>.
    {
        auto r = TRY_FORMAT_BOUNDED_RUNTIME(16,
                                            std::string_view{"{}"},
                                            42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::string,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_N → std::expected<std::size_t,
    // petra::FormatError>.
    {
        char buf[16] = {};
        auto r = TRY_FORMAT_BOUNDED_N(buf, 16, "{}", 42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::size_t,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_RUNTIME_N → std::expected<std::size_t,
    // petra::FormatError>.
    {
        char buf[16] = {};
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N(buf, 16,
                                              std::string_view{"{}"},
                                              42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::size_t,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_INTO → std::expected<std::size_t,
    // petra::FormatError>.
    {
        char buf[16] = {};
        auto r = TRY_FORMAT_BOUNDED_INTO(buf, 16, "{}", 42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::size_t,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_RUNTIME_INTO → std::expected<std::size_t,
    // petra::FormatError>.
    {
        char buf[16] = {};
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, 16,
                                                 std::string_view{"{}"},
                                                 42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::size_t,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_N_SPAN → std::expected<std::size_t,
    // petra::FormatError>.
    {
        char buf[16] = {};
        std::span<char> sp{buf, 16};
        auto r = TRY_FORMAT_BOUNDED_N_SPAN(sp, "{}", 42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::size_t,
                                            petra::FormatError>>));
    }

    // TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN → std::expected<std::size_t,
    // petra::FormatError>.
    {
        char buf[16] = {};
        std::span<char> sp{buf, 16};
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(sp,
                                                   std::string_view{"{}"},
                                                   42);
        CHECK((std::is_same_v<decltype(r),
                              std::expected<std::size_t,
                                            petra::FormatError>>));
    }
}

// ------------------------------------------------------------------
// Section 15 — no-args (zero-replacement-field) variants
// ------------------------------------------------------------------
// Each macro accepts a format with no replacement fields and no
// args.  __VA_OPT__ provides a clean fallback so the macro
// expansion produces a valid one-arg call to the underlying
// wrapper.

void section_15_no_args_variants() {
    SECTION("15.no-args-variants");

    auto r = TRY_FORMAT_BOUNDED(64, "hello world");
    CHECK(r.has_value());
    CHECK(r.has_value() && r.value() == "hello world");

    auto r2 = TRY_FORMAT_BOUNDED_RUNTIME(64,
                                         std::string_view{"hello world"},
                                         42);
    CHECK(r2.has_value());
    CHECK(r2.has_value() && r2.value() == "hello world");

    char buf[16] = {};
    auto r3 = TRY_FORMAT_BOUNDED_N(buf, 16, "hello world");
    CHECK(r3.has_value());
    CHECK(r3.has_value() && r3.value() == 11);
    CHECK((std::string_view{buf, 11} == "hello world"));

    char buf2[16] = {};
    auto r4 = TRY_FORMAT_BOUNDED_RUNTIME_N(buf2, 16,
                                           std::string_view{"hello world"},
                                           42);
    CHECK(r4.has_value());
    CHECK(r4.has_value() && r4.value() == 11);
    CHECK((std::string_view{buf2, 11} == "hello world"));

    char buf3[16] = {};
    auto r5 = TRY_FORMAT_BOUNDED_INTO(buf3, 16, "hello world");
    CHECK(r5.has_value());
    CHECK(r5.has_value() && r5.value() == 11);
    CHECK((std::string_view{buf3, 11} == "hello world"));
    CHECK(buf3[11] == '\0');

    char buf4[16] = {};
    auto r6 = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf4, 16,
                                              std::string_view{"hello world"},
                                              42);
    CHECK(r6.has_value());
    CHECK(r6.has_value() && r6.value() == 11);
    CHECK((std::string_view{buf4, 11} == "hello world"));
    CHECK(buf4[11] == '\0');

    char buf5[16] = {};
    std::span<char> sp5{buf5, 16};
    auto r7 = TRY_FORMAT_BOUNDED_N_SPAN(sp5, "hello world");
    CHECK(r7.has_value());
    CHECK(r7.has_value() && r7.value() == 11);
    CHECK((std::string_view{buf5, 11} == "hello world"));

    char buf6[16] = {};
    std::span<char> sp6{buf6, 16};
    auto r8 = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(sp6,
                                                std::string_view{"hello world"},
                                                42);
    CHECK(r8.has_value());
    CHECK(r8.has_value() && r8.value() == 11);
    CHECK((std::string_view{buf6, 11} == "hello world"));
    CHECK(buf6[11] == '\0');
}

// ------------------------------------------------------------------
// Section 16 — multi-arg round-trip
// ------------------------------------------------------------------
// The macros transparently forward std::make_format_args(args...) for
// the runtime surface, so multi-arg format strings work end-to-end.

void section_16_multi_arg_round_trip() {
    SECTION("16.multi-arg-round-trip");

    auto r = TRY_FORMAT_BOUNDED(64, "n={} a={:#x} pi={:.5f}",
                                42, 42, 3.14159);
    CHECK(r.has_value());
    CHECK(r.has_value()
          && r.value() == "n=42 a=0x2a pi=3.14159");

    auto r2 = TRY_FORMAT_BOUNDED_RUNTIME(64,
                                         std::string_view{"n={} a={:#x} pi={:.5f}"},
                                         42, 42, 3.14159);
    CHECK(r2.has_value());
    CHECK(r2.has_value()
          && r2.value() == "n=42 a=0x2a pi=3.14159");

    char buf[64] = {};
    auto r3 = TRY_FORMAT_BOUNDED_INTO(buf, 64,
                                      "n={} a={:#x} pi={:.5f}",
                                      42, 42, 3.14159);
    CHECK(r3.has_value());
    CHECK(r3.has_value() && r.value().size() == r3.value());
    CHECK((std::string_view{buf, r3.value()}
          == "n=42 a=0x2a pi=3.14159"));
    CHECK(buf[r3.value()] == '\0');
}

// ------------------------------------------------------------------
// Section 17 — consteval-vs-runtime sibling contrast
// ------------------------------------------------------------------
// Verifies the consteval macro rejects malformed format strings as
// HARD COMPILE ERRORS via std::basic_format_string's consteval ctor.
// The runtime sibling accepts the same string and converts the
// std::format_error to a structured FormatError{Format, ...} at
// runtime.
//
// The compile-time probe is saved in /tmp (NOT in the working tree):
// /tmp/probe_macro_consteval_rejects_unterminated.cpp -- produces a
// hard error.  /tmp/probe_macro_runtime_accepts_unterminated.cpp --
// COMPILES CLEANLY and prints the runtime FormatError.  Section 17
// only verifies the runtime side at runtime; the compile-time side
// is verified in /tmp.

void section_17_consteval_vs_runtime_contrast() {
    SECTION("17.consteval-vs-runtime-contrast");

    // Runtime side: TRY_FORMAT_BOUNDED_RUNTIME accepts "hello {" at
    // compile time and returns FormatError{Format, ...} at runtime.
    auto r = TRY_FORMAT_BOUNDED_RUNTIME(16,
                                        std::string_view{"hello {"},
                                        42);
    CHECK(!r.has_value());
    CHECK(!r.has_value() && r.error().kind
          == petra::FormatErrorKind::Format);
    CHECK(!r.has_value() && r.error().message.find("'{")
          != std::string::npos);

    // Malformed fmt ":.3f}" (missing leading "{") is a parse error,
    // surfacing as FormatError{Format, ...}.
    auto r2 = TRY_FORMAT_BOUNDED_RUNTIME(16,
                                         std::string_view{":.3f}"},
                                         42);
    CHECK(!r2.has_value());
    CHECK(!r2.has_value() && r2.error().kind
          == petra::FormatErrorKind::Format);
}

// ------------------------------------------------------------------
// Section 18 — 4 threads x 25 calls (concurrent macro dispatch).
// ------------------------------------------------------------------
// Each thread calls one of the macros in a tight loop.  All threads
// use their own local buffer; the macros are thread-safe because the
// underlying wrappers don't share mutable state.

void section_18_concurrent_dispatch() {
    SECTION("18.concurrent-dispatch");
    constexpr int kThreads = 4;
    constexpr int kCalls = 25;

    // Per-thread result counters; aggregated AFTER the join to avoid
    // a data race on g_pass / g_fail from concurrent threads.
    int local_pass[kThreads] = {0, 0, 0, 0};
    int local_fail[kThreads] = {0, 0, 0, 0};

    auto worker = [&](int tid) {
        for (int i = 0; i < kCalls; ++i) {
            char buf[32] = {};
            auto r = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, 32,
                                                     std::string_view{
                                                         "t={} i={} v={}"},
                                                     tid, i, i * 7);
            if (!r.has_value()) ++local_fail[tid];
            if (r.has_value() && r.value() != 0
                && std::string_view{buf, r.value()}
                   != [&] {
                       std::string s = "t=" + std::to_string(tid)
                                       + " i=" + std::to_string(i)
                                       + " v=" + std::to_string(i * 7);
                       return s;
                   }()) {
                ++local_fail[tid];
            }
            if (r.has_value() && buf[r.value()] != '\0') ++local_fail[tid];
            if (r.has_value()) ++local_pass[tid];
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) th.join();

    // Aggregate after join — no concurrent access.
    for (int t = 0; t < kThreads; ++t) {
        g_pass += local_pass[t];
        g_fail += local_fail[t];
    }

    // kThreads * kCalls = 100 calls.
    CHECK(g_fail == 0);
}

// ============================================================================
// main
// ============================================================================

int main() {
    section_1_sizeof_and_probes();
    section_2_runtime_string_basic();
    section_3_consteval_string_basic();
    section_4_runtime_dynamic_fmt();
    section_5_consteval_n_basic();
    section_6_runtime_n_basic();
    section_7_consteval_into_basic();
    section_8_runtime_into_basic();
    section_9_consteval_span_basic();
    section_10_runtime_span_basic();
    section_11_atomicity_on_too_large();
    section_12_truncation_edge();
    section_13_runtime_format_error();
    section_14_return_type_verification();
    section_15_no_args_variants();
    section_16_multi_arg_round_trip();
    section_17_consteval_vs_runtime_contrast();
    section_18_concurrent_dispatch();

    std::println("PASS  {} / {}", g_pass, g_pass + g_fail);
    std::println("FAIL  {} / {}", g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
