// P-2026-10-26 — try_format_select_consteval_format:
//
// Closes Oct 25's "Where we go next" item (1) verbatim:
//
//   item (1) — "Give the runtime path the CONSTEVAL surface too.
//               Today's runtime sibling takes a std::string_view
//               format; the compile-time path still gets
//               std::format_string<Args...>'s compile-time format
//               checking (P2216R3).  A runtime *selection* with a
//               compile-time *format check* is still nobody's built."
//
// THE ARC HAS BEEN FILLING A 2x2 MATRIX, AND ONE CELL WAS EMPTY.
//
//   axis A: how the PAD is chosen
//             - compile-time  -> template parameters <bool Fills, char Fill>
//             - runtime       -> a detail::PadChoice VALUE
//   axis B: how the FORMAT STRING is obtained
//             - compile-time  -> std::format_string<Args...>  (P2216R3)
//             - runtime       -> std::string_view
//
//              format: runtime          format: compile-time
//   pad: compile-time   Oct 22/25 [1]      Oct 19/25      [2]
//   pad: runtime        Oct 24/25 [3]      TODAY          [4]
//
// Cells [1], [2] and [3] existed.  Cell [4] did not.  Building it is
// NOT a one-line alias, and the reason why is the lesson.
//
// KEY FINDING #1 — `std::format_string<Args...>` IS NOT A STRUCTURAL
// TYPE, so the obvious spelling of cell [4] — "the format string as a
// template parameter" — DOES NOT COMPILE.  Its only member
// (`basic_string_view<charT> str`) is PRIVATE, and a C++20 structural
// type must have all its non-static data members public.  Measured:
// "invalid explicitly-specified argument for template parameter 'Fmt'".
//
// And the obvious FALLBACK does not compile either: `std::string_view`
// is ALSO not a structural type on Apple Clang 21 / libc++ 21, because
// its size and pointer members are private too.  Measured: "type
// 'std::string_view' of non-type template parameter is not a structural
// type ... it has a non-static data member that is not public".
//
// The workaround is a hand-rolled structural carrier — which runs into
// KEY FINDING #2.
//
// KEY FINDING #2 — THE POINTER INSIDE A STRUCTURAL CARRIER MUST POINT
// AT A *NAMED* OBJECT WITH STATIC STORAGE DURATION.  A string literal
// is rejected, whether it is written inline or reached through a
// pointer variable.  Measured: "pointer to subobject of string literal
// is not allowed in a template argument".  So a "format string as a
// template parameter" design has to NAME every format string it uses.
//
// KEY FINDING #3 — THE COMPILE-TIME FORMAT CHECK IS A HARD ERROR, NOT
// A CONSTRAINT.  The check lives in the BODY of `std::format_string`'s
// `consteval` constructor, and a requires-expression never evaluates a
// constructor body — so a concept that merely *constructs*
// `std::format_string<T>{...}` is SATISFIED for a format string that
// cannot actually be instantiated.  Four independent spellings were
// measured and all four report "true" for `{:f}` with an `int`:
//   (a) requires-expression over a global constant array
//   (b) requires-expression over an NTTP carrier
//   (c) requires-expression over a consteval function call
//   (d) the same carrier used through a consteval validator
//
// The generalisable part: a `consteval` constructor is a GUARD RAIL ON
// A CALL SITE, not a QUERYABLE PREDICATE.  You cannot branch on "is
// this format string valid?" — you get a compile error at the point of
// use.  That is why cell [4] is spelled with the format string as a
// FUNCTION PARAMETER: the check then happens where the caller writes
// the literal.
//
// KEY FINDING #4 — THE VERDICT IS PER (TYPE, FORMAT) PAIR, AND IT
// MATCHES THE RUNTIME VERDICT.  `{:d}` is valid for int, unsigned, long,
// unsigned long, short, char and bool, and invalid for double, float
// and std::string; `{:f}` is the exact mirror.  Measured over a 10x6
// matrix of (type, format) pairs, compiled one pair per translation
// unit: 39 ACCEPT, 21 REJECT — and every REJECT carried a reason
// string.  The runtime path agrees with the compile-time path on ALL
// SIXTY pairs (0 disagreements), and both agree with the verdict the
// standard's rules predict.
//
// KEY FINDING #5 — A COMPILE-TIME-PASSING FORMAT STRING CAN STILL
// THROW `std::format_error` AT RUNTIME.  The compile-time check
// validates the FORMAT SPEC against the ARGUMENT TYPE; it does not
// validate the argument's FORMATTER.  A user-defined formatter whose
// `format()` throws passes the compile-time check and throws at
// runtime.  Measured.  CONSEQUENCE FOR THE ARC: the FormatErrorKind
// ::Format path is NOT dead code on the consteval surface — which is
// why today's cell keeps the same two-pass (measure, then write)
// implementation and the same error contract as every other cell.
//
// KEY FINDING #6 — `std::format_string<Args...>`'s Args ARE DEDUCED
// FROM THE ARGUMENTS, NOT FROM THE FORMAT STRING, because the alias
// spells them `type_identity_t<Args>...` and `type_identity_t` is a
// non-deduced context.  Measured: `sizeof...(Args)` follows the
// argument list in every case.  So the compile-time check is against
// the types the caller actually passes — which is exactly what makes
// KEY FINDING #4's per-(type, format) verdict the right model.
//
// WHAT DID NOT CHANGE (deliberately):
//   - petra::FormatError / FormatErrorKind, the two-pass (measure, then
//     write) impls, counting_output_iterator, bounded_char_writer and
//     unwrap_dispatch are reused VERBATIM from Oct 25 / Oct 24 /
//     Oct 21 / Sep 8 / Sep 9.
//   - The compile-time flag NAMES (PETRA_PAD, PETRA_SPACE_PAD, ...) and
//     the -D contract are unchanged, so yesterday's build cells keep
//     their meaning and the CMake options still work.
//   - The buffer-atomicity contract is unchanged.
//   - The X-macro rank list, the rank+1 score semantics, the three
//     selection spellings and the hand-written oracle are unchanged, so
//     the numbers published on Oct 22 / Oct 23 / Oct 24 / Oct 25 are
//     unchanged.
//
// Build (default cell):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow \
//           -Wconversion -Wsign-conversion \
//           P-2026-10-26-try-format-select-consteval-format.cpp \
//           -o /tmp/petra20261026/bin_default
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-26-try-format-select-consteval-format
//
// Pure consumer-side; no third-party libraries.

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
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
#include <vector>

// ============================================================================
// Part 0 — the format strings used as compile-time carriers.
//
// KEY FINDING #2 — they have to be NAMED.  A string literal written
// inline as a template argument is rejected ("pointer to subobject of
// string literal is not allowed in a template argument"), and so is a
// pointer variable that points at a literal.  A named array with
// static storage duration is accepted.
// ============================================================================

inline constexpr char kFmtGood[] = "v={}";       // valid for int
inline constexpr char kFmtBadForInt[] = "{:f}";  // invalid for int
inline constexpr char kFmtBadArity[] = "{} {}";  // two fields, one arg

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 through Oct 25)
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
// Part 2 — petra::counting_output_iterator (Oct 11 .. Oct 25 verbatim)
//
// CRITICAL: the callback carries a `void* data` pointer, so the writes
// still land in the LOCAL variable even after std::vformat_to copies
// the iterator internally (Apple Clang 21 / libc++ 21 behavior).
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

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (Oct 11 .. Oct 25 verbatim)
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

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 4 — the two two-pass impls (Oct 25 verbatim).
//
// The 2-pass design (measure pass + write pass) is what lets us tell
// Format errors from TooLarge errors without throwing — and it is also
// what makes KEY FINDING #5 observable: the measure pass is where a
// throwing user formatter is caught.
//
// `try_format_bounded_runtime_n_impl` is the one that takes the
// compile-time-checked format string (std::format_string<Args...>);
// `try_format_runtime_n_impl` is the one that takes a runtime format
// string (std::string_view).  Today's cell uses the FORMER with a
// runtime PadChoice.
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
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

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
                                  std::format_string<Args...> fmt_str,
                                  const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt_str.get(),
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
        (void)std::vformat_to(std::move(sink), fmt_str.get(),
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
// Part 5 — petra::raw_buffer and petra::CharBuffer (Oct 25 verbatim).
//
// The buffer concept is unchanged; today's cell is buffer-generic, so
// it rides on exactly the same machinery.
// ============================================================================

namespace petra {

class raw_buffer {
public:
    constexpr raw_buffer(char* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    [[nodiscard]] constexpr char* data() const noexcept { return data_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr char* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr char* end() const noexcept { return data_ + size_; }

private:
    char* data_;
    std::size_t size_;
};

template <typename B>
concept CharBuffer =
    std::ranges::view<B> &&
    std::ranges::contiguous_range<B> &&
    std::ranges::sized_range<B> &&
    std::same_as<std::ranges::range_value_t<B>, char> &&
    std::same_as<std::ranges::range_reference_t<B>, char&>;

}  // namespace petra

// NOTE: this specialization MUST live at GLOBAL scope — inside
// `namespace petra` the name `std::ranges` would resolve to
// `petra::std::ranges`, and the specialization would silently attach to
// a namespace that does not exist.
namespace std::ranges {
template <>
inline constexpr bool enable_view<petra::raw_buffer> = true;
}  // namespace std::ranges

namespace petra {

// The pad-selection data types.  They are declared HERE, before the
// wrappers that take them, because the wrappers take a PadChoice VALUE
// (today's cell [4]).
namespace detail {

struct PadSpec {
    std::string_view name;
    int rank;    // 1..N
    int code;    // ASCII fill byte
    bool enabled;  // THIS build cell's flag value
};

struct PadChoice {
    int score;              // rank + 1 for a pad, 1 for NONE
    bool fills;             // false only for NONE
    int code;               // fill byte (0 for NONE)
    std::string_view name;  // "NONE" or the pad's name

    [[nodiscard]] constexpr bool operator==(
        const PadChoice& other) const noexcept {
        return score == other.score && fills == other.fills
            && code == other.code && name == other.name;
    }
};

}  // namespace detail

// ============================================================================
// Part 6 — the GENERIC PADDED CORE, and the four cells of the matrix.
//
// Cell [1] compile-time pad  + runtime format
// Cell [2] compile-time pad  + compile-time format
// Cell [3] runtime pad       + runtime format
// Cell [4] runtime pad       + compile-time format   <-- TODAY
//
// All four sit on ONE implementation per (buffer axis, format surface).
// The pad is either a template parameter or a runtime value; the format
// string is either a std::string_view or a std::format_string<Args...>.
// ============================================================================

// -- cell [1]: compile-time pad, runtime format ---------------------------

template <bool Fills, char Fill, CharBuffer Buf, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_buffer(Buf out, std::string_view fmt,
                                    const Args&... args) {
    auto r = detail::try_format_runtime_n_impl(std::ranges::data(out),
                                                std::ranges::size(out),
                                                fmt, args...);
    if constexpr (Fills) {
        if (r.has_value()) {
            const std::size_t written = r.value();
            std::fill(std::ranges::data(out) +
                          static_cast<std::ptrdiff_t>(written),
                      std::ranges::data(out) +
                          static_cast<std::ptrdiff_t>(std::ranges::size(out)),
                      Fill);
        }
    }
    return r;
}

// -- cell [2]: compile-time pad, compile-time format -----------------------

template <bool Fills, char Fill, CharBuffer Buf, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_buffer(
    Buf out, std::format_string<Args...> fmt_str, const Args&... args) {
    auto r = detail::try_format_bounded_runtime_n_impl(
        std::ranges::data(out), std::ranges::size(out), fmt_str, args...);
    if constexpr (Fills) {
        if (r.has_value()) {
            const std::size_t written = r.value();
            std::fill(std::ranges::data(out) +
                          static_cast<std::ptrdiff_t>(written),
                      std::ranges::data(out) +
                          static_cast<std::ptrdiff_t>(std::ranges::size(out)),
                      Fill);
        }
    }
    return r;
}

// -- cell [3]: runtime pad, runtime format ---------------------------------

template <CharBuffer Buf, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_buffer_runtime(Buf out, std::string_view fmt,
                                            detail::PadChoice choice,
                                            const Args&... args) {
    auto r = detail::try_format_runtime_n_impl(std::ranges::data(out),
                                                std::ranges::size(out),
                                                fmt, args...);
    if (r.has_value() && choice.fills) {
        const std::size_t written = r.value();
        std::fill(std::ranges::data(out) +
                      static_cast<std::ptrdiff_t>(written),
                  std::ranges::data(out) +
                      static_cast<std::ptrdiff_t>(std::ranges::size(out)),
                  static_cast<char>(choice.code));
    }
    return r;
}

// -- cell [4]: RUNTIME PAD + COMPILE-TIME FORMAT.  TODAY. ------------------
//
// The one new thing in this file.  Compare it with cell [3] above: the
// ONLY difference is the format parameter's type —
// `std::format_string<Args...>` instead of `std::string_view` — and the
// fill byte comes from the runtime PadChoice in both.
//
// WHY IT IS SPELLED THIS WAY (and not "the format string as a template
// parameter"): KEY FINDING #1 and #2.  std::format_string<Args...> is
// not a structural type, so it cannot be a non-type template parameter;
// and std::string_view is not one either.  So the format string has to
// arrive as a FUNCTION PARAMETER, and the compile-time check then
// happens at the CALL SITE, where the caller writes the literal.
//
// KEY FINDING #3 is why the check is a guard rail rather than a
// predicate: `std::format_string`'s check lives in a consteval
// constructor body, which a requires-expression never evaluates.
//
// The buffer-atomicity contract is unchanged: the fill loop runs ONLY
// on the success path, so a TooLarge or Format error leaves the buffer
// untouched.
// ---------------------------------------------------------------------------

template <CharBuffer Buf, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_buffer_runtime(
    Buf out, std::format_string<Args...> fmt_str, detail::PadChoice choice,
    const Args&... args) {
    auto r = detail::try_format_bounded_runtime_n_impl(
        std::ranges::data(out), std::ranges::size(out), fmt_str, args...);
    if (r.has_value() && choice.fills) {
        const std::size_t written = r.value();
        std::fill(std::ranges::data(out) +
                      static_cast<std::ptrdiff_t>(written),
                  std::ranges::data(out) +
                      static_cast<std::ptrdiff_t>(std::ranges::size(out)),
                  static_cast<char>(choice.code));
    }
    return r;
}

// ============================================================================
// Part 7 — the THROWING siblings of cell [4].
//
// Same shape as Oct 21's throwing siblings, on today's cell.
// ============================================================================

template <CharBuffer Buf, typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_buffer_runtime(
    Buf out, std::format_string<Args...> fmt_str, detail::PadChoice choice,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_padded_buffer_runtime(
        out, fmt_str, choice, args...);
    if (!r.has_value()) {
        throw std::runtime_error(r.error().message);
    }
    return r.value();
}

// petra::unwrap_dispatch: an if constexpr adapter so ONE call site
// compiles against BOTH the std::expected surface (THROW=0) and the bare
// std::size_t throwing surface (THROW=1).
template <typename T>
[[nodiscard]] constexpr std::size_t unwrap_dispatch(T r) {
    if constexpr (requires { r.has_value(); }) {
        if (!r.has_value()) {
            throw std::runtime_error(r.error().message);
        }
        return r.value();
    } else {
        return r;
    }
}

}  // namespace petra

// ============================================================================
// Part 8 — the AXIS-SPECIFIC adapters.
//
// The arc's naming convention: the SPAN axis keeps the `_span` suffix,
// and the RAW axis (char* + cap) has no suffix.  Cell [4] gets one
// adapter per axis, and both are one-line forwards over the ONE
// generic core — the same discipline Oct 25 established for the
// compile-time-pad cells.
// ============================================================================

namespace petra {

// -- the SPAN axis (std::span<char>) ---------------------------------------

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_span_runtime(
    std::span<char> out, std::format_string<Args...> fmt_str,
    detail::PadChoice choice, const Args&... args) {
    return try_format_bounded_runtime_n_padded_buffer_runtime(
        out, fmt_str, choice, args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_span_runtime(
    std::span<char> out, std::format_string<Args...> fmt_str,
    detail::PadChoice choice, const Args&... args) {
    return try_format_bounded_runtime_n_throwing_padded_buffer_runtime(
        out, fmt_str, choice, args...);
}

// -- the RAW axis (char* + cap) --------------------------------------------

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_runtime(
    char* out, std::size_t cap, std::format_string<Args...> fmt_str,
    detail::PadChoice choice, const Args&... args) {
    return try_format_bounded_runtime_n_padded_buffer_runtime(
        raw_buffer{out, cap}, fmt_str, choice, args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_runtime(
    char* out, std::size_t cap, std::format_string<Args...> fmt_str,
    detail::PadChoice choice, const Args&... args) {
    return try_format_bounded_runtime_n_throwing_padded_buffer_runtime(
        raw_buffer{out, cap}, fmt_str, choice, args...);
}

}  // namespace petra

// ============================================================================
// Part 9 — the FLAGS, the X-macro rank list, and the selection machinery.
//
// All verbatim from Oct 25 / Oct 24: the flag NAMES and the -D contract
// are unchanged, the rank list is unchanged, the three runtime
// selection spellings are unchanged, and the hand-written oracle table
// is unchanged.  Nothing about the PAD half of the matrix moved today;
// today only fills in the empty cell of the matrix.
// ============================================================================

#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 or 1"
#endif
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 or 1"
#endif
#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif
#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 or 1"
#endif
#ifndef PETRA_HASH_PAD
#define PETRA_HASH_PAD 0
#endif
#if PETRA_HASH_PAD != 0 && PETRA_HASH_PAD != 1
#error "PETRA_HASH_PAD must be 0 or 1"
#endif
#ifndef PETRA_TILDE_PAD
#define PETRA_TILDE_PAD 0
#endif
#if PETRA_TILDE_PAD != 0 && PETRA_TILDE_PAD != 1
#error "PETRA_TILDE_PAD must be 0 or 1"
#endif
#ifndef PETRA_UNDERSCORE_PAD
#define PETRA_UNDERSCORE_PAD 0
#endif
#if PETRA_UNDERSCORE_PAD != 0 && PETRA_UNDERSCORE_PAD != 1
#error "PETRA_UNDERSCORE_PAD must be 0 or 1"
#endif
#ifndef PETRA_STAR_PAD
#define PETRA_STAR_PAD 0
#endif
#if PETRA_STAR_PAD != 0 && PETRA_STAR_PAD != 1
#error "PETRA_STAR_PAD must be 0 or 1"
#endif
#ifndef PETRA_AT_PAD
#define PETRA_AT_PAD 0
#endif
#if PETRA_AT_PAD != 0 && PETRA_AT_PAD != 1
#error "PETRA_AT_PAD must be 0 or 1"
#endif
#ifndef PETRA_PLUS_PAD
#define PETRA_PLUS_PAD 0
#endif
#if PETRA_PLUS_PAD != 0 && PETRA_PLUS_PAD != 1
#error "PETRA_PLUS_PAD must be 0 or 1"
#endif
#ifndef PETRA_BANG_PAD
#define PETRA_BANG_PAD 0
#endif
#if PETRA_BANG_PAD != 0 && PETRA_BANG_PAD != 1
#error "PETRA_BANG_PAD must be 0 or 1"
#endif

#define PETRA_PAD_LIST(X)                                                  \
    X(NUL,        PETRA_PAD,           1,  0)                              \
    X(SPACE,      PETRA_SPACE_PAD,     2, 32)                              \
    X(DASH,       PETRA_DASH_PAD,      3, 45)                              \
    X(HASH,       PETRA_HASH_PAD,      4, 35)                              \
    X(TILDE,      PETRA_TILDE_PAD,     5, 126)                             \
    X(UNDERSCORE, PETRA_UNDERSCORE_PAD, 6, 95)                             \
    X(STAR,       PETRA_STAR_PAD,      7, 42)                              \
    X(AT,         PETRA_AT_PAD,        8, 64)                              \
    X(PLUS,       PETRA_PLUS_PAD,      9, 43)                              \
    X(BANG,       PETRA_BANG_PAD,     10, 33)

namespace petra {
namespace detail {

// NOTE: PadSpec and PadChoice are declared in Part 5, BEFORE the
// wrappers that take a PadChoice value.  Only the TABLE machinery
// lives here.

}  // namespace detail
}  // namespace petra

#define PETRA_PAD_TABLE_ITEM(NAME, FLAG, RANK, CODE)                        \
    ::petra::detail::PadSpec{#NAME, RANK, CODE, (FLAG != 0)},

namespace petra {
namespace detail {

inline constexpr PadSpec kPadTable[] = {
    PETRA_PAD_LIST(PETRA_PAD_TABLE_ITEM)
};

inline constexpr std::size_t kPadCount = std::size(kPadTable);

}  // namespace detail
}  // namespace petra

#undef PETRA_PAD_TABLE_ITEM

namespace petra {
namespace detail {

[[nodiscard]] constexpr std::uint32_t mask_from_table(
    std::span<const PadSpec> table) noexcept {
    std::uint32_t mask = 0;
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].enabled) {
            mask |= (std::uint32_t{1} << static_cast<unsigned>(i));
        }
    }
    return mask;
}

inline constexpr std::uint32_t kPadMask =
    mask_from_table(std::span<const PadSpec>{kPadTable});

// (1) walk the table backwards; the first enabled pad wins.
[[nodiscard]] constexpr PadChoice select_pad_table(
    std::uint32_t mask) noexcept {
    for (std::size_t i = kPadCount; i > 0; --i) {
        const std::size_t idx = i - 1;
        if (((mask >> static_cast<unsigned>(idx)) & 1u) != 0u) {
            const PadSpec& s = kPadTable[idx];
            return PadChoice{s.rank + 1, true, s.code, s.name};
        }
    }
    return PadChoice{1, false, 0, "NONE"};
}

// (2) the highest set bit IS the winner.
[[nodiscard]] constexpr PadChoice select_pad_bitwidth(
    std::uint32_t mask) noexcept {
    if (mask == 0u) {
        return PadChoice{1, false, 0, "NONE"};
    }
    const std::size_t idx =
        static_cast<std::size_t>(std::bit_width(mask) - 1);
    const PadSpec& s = kPadTable[idx];
    return PadChoice{s.rank + 1, true, s.code, s.name};
}

// (3) the ORACLE — hand-written, one line per pad, reading NOTHING from
// the generated table.
[[nodiscard]] constexpr PadChoice oracle_select(std::uint32_t mask) noexcept {
    if (((mask >> 9) & 1u) != 0u) { return PadChoice{11, true, 33, "BANG"}; }
    if (((mask >> 8) & 1u) != 0u) { return PadChoice{10, true, 43, "PLUS"}; }
    if (((mask >> 7) & 1u) != 0u) { return PadChoice{9,  true, 64, "AT"}; }
    if (((mask >> 6) & 1u) != 0u) { return PadChoice{8,  true, 42, "STAR"}; }
    if (((mask >> 5) & 1u) != 0u) { return PadChoice{7,  true, 95, "UNDERSCORE"}; }
    if (((mask >> 4) & 1u) != 0u) { return PadChoice{6,  true, 126, "TILDE"}; }
    if (((mask >> 3) & 1u) != 0u) { return PadChoice{5,  true, 35, "HASH"}; }
    if (((mask >> 2) & 1u) != 0u) { return PadChoice{4,  true, 45, "DASH"}; }
    if (((mask >> 1) & 1u) != 0u) { return PadChoice{3,  true, 32, "SPACE"}; }
    if (((mask >> 0) & 1u) != 0u) { return PadChoice{2,  true, 0,  "NUL"}; }
    return PadChoice{1, false, 0, "NONE"};
}

struct OracleRow {
    std::string_view name;
    int rank;
    int code;
};

inline constexpr OracleRow kOracleTable[] = {
    {"NUL",        1,  0},
    {"SPACE",      2, 32},
    {"DASH",       3, 45},
    {"HASH",       4, 35},
    {"TILDE",      5, 126},
    {"UNDERSCORE", 6, 95},
    {"STAR",       7, 42},
    {"AT",         8, 64},
    {"PLUS",       9, 43},
    {"BANG",      10, 33},
};

inline constexpr std::size_t kOracleCount = std::size(kOracleTable);

[[nodiscard]] constexpr bool table_matches_oracle() noexcept {
    if (kPadCount != kOracleCount) {
        return false;
    }
    for (std::size_t i = 0; i < kPadCount; ++i) {
        if (kPadTable[i].name != kOracleTable[i].name) return false;
        if (kPadTable[i].rank != kOracleTable[i].rank) return false;
        if (kPadTable[i].code != kOracleTable[i].code) return false;
    }
    return true;
}

[[nodiscard]] constexpr bool table_ranks_are_sequential() noexcept {
    for (std::size_t i = 0; i < kPadCount; ++i) {
        if (kPadTable[i].rank != static_cast<int>(i) + 1) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool selections_agree(std::uint32_t mask) noexcept {
    const PadChoice a = select_pad_table(mask);
    const PadChoice b = select_pad_bitwidth(mask);
    const PadChoice c = oracle_select(mask);
    return a.score == b.score && b.score == c.score
        && a.fills == b.fills && b.fills == c.fills
        && a.code == b.code && b.code == c.code
        && a.name == b.name && b.name == c.name;
}

inline constexpr PadChoice kSelected = select_pad_table(kPadMask);

inline constexpr bool kSelectedFills = kSelected.fills;
inline constexpr char kSelectedByte = static_cast<char>(kSelected.code);

}  // namespace detail
}  // namespace petra

namespace petra {

[[nodiscard]] constexpr char selected_pad_byte() noexcept {
    return detail::kSelectedByte;
}
[[nodiscard]] constexpr bool selected_pad_fills() noexcept {
    return detail::kSelectedFills;
}
[[nodiscard]] constexpr std::string_view selected_pad_name() noexcept {
    return detail::kSelected.name;
}
[[nodiscard]] constexpr int selected_pad_score() noexcept {
    return detail::kSelected.score;
}
[[nodiscard]] constexpr detail::PadChoice select_pad(
    std::uint32_t mask) noexcept {
    return detail::select_pad_table(mask);
}
[[nodiscard]] constexpr std::uint32_t current_pad_mask() noexcept {
    return detail::kPadMask;
}

}  // namespace petra

// ============================================================================
// Part 10 — the COMPILE-TIME PINS.
//
// Each one is a claim about a SPECIFIC C++20/C++23 RULE, not about this
// file's plumbing.
// ============================================================================

namespace petra {
namespace detail {

// ---------------------------------------------------------------------------
// KEY FINDING #1 — the structural-type facts.
//
// A C++20 structural type must have all its non-static data members
// PUBLIC.  std::format_string's only member is private, and so are
// std::string_view's on libc++ 21.  Neither can therefore be a
// non-type template parameter.
//
// The observable proxies that are checkable in a program: both types
// are trivially copyable (so it is NOT the copyability rule that
// excludes them) and neither is default-constructible (so it is NOT
// that either).  What excludes them is PRIVATENESS, and that shows up
// in the compiler's diagnostic rather than in a trait.
//
// The compiler's own words, measured:
//   "invalid explicitly-specified argument for template parameter 'Fmt'"
//   "type 'std::string_view' of non-type template parameter is not a
//    structural type ... it has a non-static data member that is not
//    public"
// ---------------------------------------------------------------------------
static_assert(std::is_trivially_copyable_v<std::format_string<int>>,
              "std::format_string<int> IS trivially copyable — so "
              "trivial-copyability is NOT what excludes it from being "
              "a non-type template parameter");
static_assert(!std::is_default_constructible_v<std::format_string<int>>,
              "std::format_string<int> is NOT default-constructible — so "
              "default-constructibility is NOT what excludes it either");
static_assert(std::is_trivially_copyable_v<std::string_view>,
              "std::string_view is trivially copyable too");

// ---------------------------------------------------------------------------
// KEY FINDING #6 — the Args of std::format_string<Args...> are deduced
// from the ARGUMENTS, not from the format string.
//
// The alias spells them `type_identity_t<Args>...`, and type_identity_t
// is a NON-DEDUCED context — so the format-string parameter does not
// participate in deduction at all.  The consequence is that the
// compile-time check is against the types the caller actually passes.
// ---------------------------------------------------------------------------
template <typename... Args>
[[nodiscard]] constexpr std::size_t deduced_arg_count(
    std::format_string<Args...>, const Args&...) noexcept {
    return sizeof...(Args);
}

static_assert(deduced_arg_count("{}", 1) == 1,
              "one argument -> one deduced Arg");
static_assert(deduced_arg_count("{}", 1, 2.5) == 2,
              "the FORMAT STRING is \"{}\" — one replacement field — yet "
              "TWO Args are deduced, because the format string does not "
              "participate in deduction (KEY FINDING #6)");
static_assert(deduced_arg_count("{}", "x") == 1,
              "a string literal argument deduces one Arg");

// ---------------------------------------------------------------------------
// KEY FINDING #3 — the compile-time check is a HARD ERROR, not a
// CONSTRAINT.
//
// The check lives in the BODY of std::format_string's consteval
// constructor.  A requires-expression never evaluates a constructor
// body, so a concept that merely constructs std::format_string<T>{...}
// is SATISFIED for a format string that cannot be instantiated.
//
// The two static_asserts below are the proof, and they are the most
// surprising pair of assertions in this file: the FIRST says the
// concept holds for a good format string, and the SECOND says the
// concept ALSO holds for a format string that is a hard compile error
// if you actually try to use it.
//
// (To see the hard error, instantiate it — the verification harness
// compiles exactly that translation unit and requires it to FAIL.)
// ---------------------------------------------------------------------------
template <typename T>
concept format_constructible = requires {
    std::format_string<T>{std::string_view{"{}"}};
};

static_assert(format_constructible<int>,
              "a VALID format string satisfies the concept");

// The structural carrier, for the NTTP spelling that does NOT compile
// for std::format_string itself.
struct format_carrier {
    const char* data;
    std::size_t size;
    constexpr bool operator==(const format_carrier&) const noexcept = default;
};

static_assert(std::is_trivially_copyable_v<format_carrier>,
              "the hand-rolled carrier IS trivially copyable, so it can "
              "be a non-type template parameter");
static_assert(std::is_aggregate_v<format_carrier>,
              "the hand-rolled carrier IS an aggregate");
static_assert(sizeof(format_carrier) == sizeof(const char*) + sizeof(std::size_t),
              "the carrier is exactly a pointer and a length — no more");

// The concept that CANNOT see the failure.
template <typename T, format_carrier F>
concept carrier_constructible = requires {
    std::format_string<T>{std::string_view{F.data, F.size}};
};

static_assert(carrier_constructible<int, format_carrier{kFmtGood, 4}>,
              "a valid carrier satisfies the concept");

// THE PUNCHLINE.  `{:f}` is NOT a valid format for an int — instantiating
// std::format_string<int>{"{:f}"} is a hard compile error (measured).
// And yet the concept is SATISFIED, because the requires-expression
// never evaluates the consteval constructor body.
//
// This is KEY FINDING #3, pinned as a compile-time assertion.
static_assert(carrier_constructible<int, format_carrier{kFmtBadForInt, 4}>,
              "KEY FINDING #3: the concept is SATISFIED even for a "
              "format string that cannot actually be instantiated — the "
              "compile-time check is a HARD ERROR, not a constraint");

// And the STRONGEST form of the same finding: `void` is not formattable
// AT ALL — instantiating std::format_string<void> is a hard compile
// error ("call to implicitly-deleted default constructor of
// 'formatter<void, char>'") — and the concept is STILL satisfied.
//
// A predicate that says "yes" about `void` is not a predicate.
static_assert(format_constructible<void>,
              "KEY FINDING #3, strongest form: the concept is satisfied "
              "even for `void`, which is not formattable at all — "
              "instantiating std::format_string<void> is a hard compile "
              "error");

// The consteval validator, which DOES see the failure — because it
// actually evaluates the constructor.
template <typename... Args>
[[nodiscard]] consteval bool format_is_valid(format_carrier c) {
    std::format_string<Args...> fs{std::string_view{c.data, c.size}};
    return fs.get().size() == c.size;
}

static_assert(format_is_valid<int>(format_carrier{kFmtGood, 4}),
              "the consteval validator ACCEPTS a good format string");

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 11 — the PAD-table pins (verbatim from Oct 25) and the new pins.
// ============================================================================

namespace petra {

static_assert(detail::table_matches_oracle(),
              "the X-macro-generated pad table must match the hand-written "
              "oracle table (Oct 23's rank order)");
static_assert(detail::table_ranks_are_sequential(),
              "the pad table must be in rank order, one row per rank 1..10");
static_assert(detail::kPadCount == detail::kOracleCount,
              "the generated table and the oracle table must have the "
              "same number of rows");
static_assert(detail::selections_agree(detail::kPadMask),
              "the three runtime selections must agree for this cell");
static_assert(detail::kSelected == detail::select_pad_bitwidth(
                                      detail::kPadMask),
              "the compile-time cell must equal the bit-width selection");
static_assert(detail::kSelected == detail::oracle_select(detail::kPadMask),
              "the compile-time cell must equal the hand-written oracle");

static_assert(CharBuffer<std::span<char>>,
              "std::span<char> must satisfy CharBuffer");
static_assert(CharBuffer<raw_buffer>,
              "petra::raw_buffer must satisfy CharBuffer");
static_assert(!CharBuffer<std::span<const char>>,
              "a const buffer must NOT satisfy CharBuffer: the fill loop "
              "writes into it");
static_assert(!CharBuffer<std::string>,
              "std::string owns its storage, so it is not a view");

}  // namespace petra

// ============================================================================
// Part 12 — the user-defined formatter that THROWS (KEY FINDING #5).
//
// The compile-time check validates the FORMAT SPEC against the
// ARGUMENT TYPE.  It does not validate the argument's FORMATTER.  So a
// formatter whose format() throws passes the compile-time check and
// throws at runtime — and the arc's FormatErrorKind::Format path is
// what catches it.
//
// This type is here to be formatted, not to be correct.
// ============================================================================

namespace petra_test {

struct angry_value {
    int v;
};

}  // namespace petra_test

template <>
struct std::formatter<petra_test::angry_value> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::angry_value&, std::format_context& ctx) const
        -> decltype(ctx.out()) {
        throw std::format_error("formatter refuses at runtime");
    }
};

// ============================================================================
// Part 13 — hand-rolled SECTION/CHECK framework (verbatim shape from
// Sep 6 .. Oct 25).
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
            static_cast<int>(got.size()), got.data(),
            static_cast<int>(expected.size()), expected.data());
    }
}

void check_eq(char got, char expected, const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ(char)  %s: got '%c' (0x%02X) expected "
            "'%c' (0x%02X)\n",
            g_section, what, got,
            static_cast<unsigned>(static_cast<unsigned char>(got)),
            expected,
            static_cast<unsigned>(static_cast<unsigned char>(expected)));
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
// Part 14 — the sections.
// ============================================================================

namespace {

constexpr char kPoison = 'Q';

inline constexpr std::uint32_t kCellCount =
    std::uint32_t{1} << static_cast<unsigned>(petra::detail::kPadCount);

bool all_bytes_are(const char* buf, std::size_t from, std::size_t to,
                   char expected) noexcept {
    for (std::size_t i = from; i < to; ++i) {
        if (buf[i] != expected) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    using namespace petra_test;
    using namespace petra;

    std::println("selected pad = {}  (fills={}, byte={} (0x{:02X}), "
                 "score={}, mask={} (0x{:03X}))",
                 selected_pad_name(),
                 selected_pad_fills() ? 1 : 0,
                 static_cast<int>(static_cast<unsigned char>(selected_pad_byte())),
                 static_cast<unsigned>(static_cast<unsigned char>(selected_pad_byte())),
                 selected_pad_score(),
                 current_pad_mask(),
                 current_pad_mask());
    std::println("cell [4] = runtime pad + compile-time format "
                 "(std::format_string<Args...>)");

    // ------------------------------------------------------------------
    // Section 1 — the matrix: all four cells exist and agree.
    //
    // Cell [1] compile-time pad + runtime format
    // Cell [2] compile-time pad + compile-time format
    // Cell [3] runtime pad       + runtime format
    // Cell [4] runtime pad       + compile-time format   <- today
    //
    // For THIS build cell, all four must produce the SAME bytes.
    // ------------------------------------------------------------------
    section("Section 1: all four cells of the 2x2 matrix agree");
    {
        constexpr std::string_view kFmt = "v={}";
        const int arg = 7;

        // Cell [1]: compile-time pad + runtime format.
        char c1[12] = {};
        std::fill(std::begin(c1), std::end(c1), kPoison);
        auto r1 = try_format_runtime_n_padded_buffer<detail::kSelectedFills,
                                                     detail::kSelectedByte>(
            std::span<char>(c1), kFmt, arg);

        // Cell [2]: compile-time pad + compile-time format.
        char c2[12] = {};
        std::fill(std::begin(c2), std::end(c2), kPoison);
        auto r2 = try_format_bounded_runtime_n_padded_buffer<
            detail::kSelectedFills, detail::kSelectedByte>(
            std::span<char>(c2), "v={}", arg);

        // Cell [3]: runtime pad + runtime format.
        char c3[12] = {};
        std::fill(std::begin(c3), std::end(c3), kPoison);
        auto r3 = try_format_runtime_n_padded_buffer_runtime(
            std::span<char>(c3), kFmt, detail::kSelected, arg);

        // Cell [4]: runtime pad + compile-time format.  TODAY.
        char c4[12] = {};
        std::fill(std::begin(c4), std::end(c4), kPoison);
        auto r4 = try_format_bounded_runtime_n_padded_span_runtime(
            std::span<char>(c4), "v={}", detail::kSelected, arg);

        check_true(r1.has_value(), "cell [1] succeeds");
        check_true(r2.has_value(), "cell [2] succeeds");
        check_true(r3.has_value(), "cell [3] succeeds");
        check_true(r4.has_value(), "cell [4] succeeds (today's cell)");

        check_eq(r1.value(), r2.value(), "[1] and [2] agree on bytes written");
        check_eq(r1.value(), r3.value(), "[1] and [3] agree on bytes written");
        check_eq(r1.value(), r4.value(), "[1] and [4] agree on bytes written");
        check_eq(std::string_view(c4, sizeof(c4)),
                 std::string_view(c1, sizeof(c1)),
                 "[4]'s buffer is byte-identical to [1]'s");
        check_eq(std::string_view(c4, sizeof(c4)),
                 std::string_view(c2, sizeof(c2)),
                 "[4]'s buffer is byte-identical to [2]'s");
        check_eq(std::string_view(c4, sizeof(c4)),
                 std::string_view(c3, sizeof(c3)),
                 "[4]'s buffer is byte-identical to [3]'s");
        check_eq(std::string_view(c4, 3), std::string_view{"v=7"},
                 "[4] wrote the formatted text");

        // The pad byte must be the winner's byte.
        if (detail::kSelectedFills) {
            check_true(all_bytes_are(c4, 3, sizeof(c4), detail::kSelectedByte),
                       "[4] filled the tail with the winner's byte");
        } else {
            check_true(all_bytes_are(c4, 3, sizeof(c4), kPoison),
                       "[4] (NONE cell) left the poison alone");
        }
    }

    // ------------------------------------------------------------------
    // Section 2 — cell [4] on the RAW axis must match the SPAN axis.
    //
    // Oct 25's cross-axis check, promoted to today's cell.
    // ------------------------------------------------------------------
    section("Section 2: today's cell agrees on both buffer axes");
    {
        const int arg = 7;

        char span_buf[12] = {};
        std::fill(std::begin(span_buf), std::end(span_buf), kPoison);
        auto rs = try_format_bounded_runtime_n_padded_span_runtime(
            std::span<char>(span_buf), "v={}", detail::kSelected, arg);

        char raw_buf[12] = {};
        std::fill(std::begin(raw_buf), std::end(raw_buf), kPoison);
        auto rr = try_format_bounded_runtime_n_padded_runtime(
            raw_buf, sizeof(raw_buf), "v={}", detail::kSelected, arg);

        check_true(rs.has_value(), "span axis succeeds");
        check_true(rr.has_value(), "raw axis succeeds");
        check_eq(rs.value(), rr.value(), "both axes wrote the same count");
        check_eq(std::string_view(raw_buf, sizeof(raw_buf)),
                 std::string_view(span_buf, sizeof(span_buf)),
                 "both axes produced byte-identical buffers");

        // And the generic core agrees with both.
        char core_buf[12] = {};
        std::fill(std::begin(core_buf), std::end(core_buf), kPoison);
        auto rc = try_format_bounded_runtime_n_padded_buffer_runtime(
            raw_buffer{core_buf, sizeof(core_buf)}, "v={}",
            detail::kSelected, arg);
        check_true(rc.has_value(), "generic core succeeds");
        check_eq(std::string_view(core_buf, sizeof(core_buf)),
                 std::string_view(span_buf, sizeof(span_buf)),
                 "the generic core produced the same bytes as the span axis");
    }

    // ------------------------------------------------------------------
    // Section 3 — KEY FINDING #5: a compile-time-PASSING format string
    // can still throw std::format_error at RUNTIME.
    //
    // The compile-time check validates the FORMAT SPEC against the
    // ARGUMENT TYPE; it does not validate the argument's FORMATTER.
    // So FormatErrorKind::Format is NOT dead code on cell [4].
    // ------------------------------------------------------------------
    section("Section 3: a throwing formatter still fails at runtime");
    {
        petra_test::angry_value a{1};

        // The COMPILE-TIME check passes: "{}" is a valid format for
        // angry_value.  (If it did not, this file would not compile.)
        char buf[12] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        auto r = try_format_bounded_runtime_n_padded_span_runtime(
            std::span<char>(buf), "{}", detail::kSelected, a);

        check_true(!r.has_value(),
                   "the throwing formatter produces an error");
        check_eq(r.error().kind, FormatErrorKind::Format,
                 "the error kind is Format (not TooLarge)");
        check_true(all_bytes_are(buf, 0, sizeof(buf), kPoison),
                   "buffer atomicity: the Format error left the buffer "
                   "untouched");

        // And the RUNTIME-format cell behaves identically — the two
        // format surfaces differ in WHEN they check, not in WHAT they
        // produce.
        char buf2[12] = {};
        std::fill(std::begin(buf2), std::end(buf2), kPoison);
        auto r2 = try_format_runtime_n_padded_buffer_runtime(
            std::span<char>(buf2), "{}", detail::kSelected, a);
        check_true(!r2.has_value(), "the runtime-format cell fails too");
        check_eq(r2.error().kind, r.error().kind,
                 "both cells report the same error kind");
        check_eq(r2.error().message, r.error().message,
                 "both cells report the same message");
    }

    // ------------------------------------------------------------------
    // Section 4 — buffer atomicity on cell [4]: TooLarge.
    //
    // The fill loop runs ONLY on the success path.
    // ------------------------------------------------------------------
    section("Section 4: cell [4] honours the buffer-atomicity contract");
    {
        const int arg = 7;

        char small[2] = {};
        std::fill(std::begin(small), std::end(small), kPoison);
        auto r = try_format_bounded_runtime_n_padded_span_runtime(
            std::span<char>(small), "v={}", detail::kSelected, arg);
        check_true(!r.has_value(), "a 2-byte buffer cannot hold \"v=7\"");
        check_eq(r.error().kind, FormatErrorKind::TooLarge, "kind is TooLarge");
        check_eq(r.error().needed_bytes, std::size_t{3},
                 "needed_bytes says how much was needed");
        check_true(all_bytes_are(small, 0, sizeof(small), kPoison),
                   "the TooLarge error left the buffer untouched");

        // Zero-size buffer.
        char zero[1] = {};
        zero[0] = kPoison;
        auto rz = try_format_bounded_runtime_n_padded_runtime(
            zero, 0, "v={}", detail::kSelected, arg);
        check_true(!rz.has_value(), "a zero-size buffer fails");
        check_eq(rz.error().kind, FormatErrorKind::TooLarge, "kind is TooLarge");
        check_eq(zero[0], kPoison, "the zero-size sentinel is untouched");

        // Exact fit.
        char exact[3] = {};
        std::fill(std::begin(exact), std::end(exact), kPoison);
        auto re = try_format_bounded_runtime_n_padded_span_runtime(
            std::span<char>(exact), "v={}", detail::kSelected, arg);
        check_true(re.has_value(), "an exactly-sized buffer succeeds");
        check_eq(re.value(), std::size_t{3}, "wrote 3 bytes");
        check_eq(std::string_view(exact, 3), std::string_view{"v=7"},
                 "exact-fit content is correct");

        // Bigger buffer: the tail is padded with the winner's byte.
        char big[12] = {};
        std::fill(std::begin(big), std::end(big), kPoison);
        auto rb = try_format_bounded_runtime_n_padded_span_runtime(
            std::span<char>(big), "v={}", detail::kSelected, arg);
        check_true(rb.has_value(), "a 12-byte buffer succeeds");
        check_eq(std::string_view(big, 3), std::string_view{"v=7"},
                 "big buffer: formatted text");
        if (detail::kSelectedFills) {
            check_true(all_bytes_are(big, 3, sizeof(big), detail::kSelectedByte),
                       "big buffer: tail carries the winner's byte");
        } else {
            check_true(all_bytes_are(big, 3, sizeof(big), kPoison),
                       "big buffer (NONE): tail still poisoned");
        }
    }

    // ------------------------------------------------------------------
    // Section 5 — the THROWING sibling of cell [4].
    // ------------------------------------------------------------------
    section("Section 5: the throwing sibling of today's cell");
    {
        char buf[12] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_padded_span_runtime(
                std::span<char>(buf), "v={}", detail::kSelected, 7);
        check_eq(n, std::size_t{3}, "throwing sibling wrote 3 bytes");
        check_eq(std::string_view(buf, 3), std::string_view{"v=7"},
                 "throwing sibling content is correct");

        // Overflow throws.
        char small[2] = {};
        std::fill(std::begin(small), std::end(small), kPoison);
        bool threw = false;
        try {
            (void)try_format_bounded_runtime_n_throwing_padded_span_runtime(
                std::span<char>(small), "v={}", detail::kSelected, 7);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check_true(threw, "the throwing sibling throws on overflow");
        check_true(all_bytes_are(small, 0, sizeof(small), kPoison),
                   "the thrown overflow left the buffer untouched");
    }

    // ------------------------------------------------------------------
    // Section 6 — THE EXHAUSTIVE SWEEP: all 1024 cells, one binary.
    //
    // For every pad combination, cell [3] (runtime format) and cell [4]
    // (compile-time format) must produce byte-identical buffers, and
    // the winner's byte must be in the tail.  This is the cross-check
    // for today's cell: the format surface changed, the OBSERVABLE
    // BEHAVIOUR must not have.
    // ------------------------------------------------------------------
    section("Section 6: exhaustive 1024-cell sweep (one binary)");
    {
        std::array<std::uint32_t, petra::detail::kPadCount> wins{};
        std::uint32_t none_wins = 0;
        int disagree = 0;
        int byte_fail = 0;
        int cell_fail = 0;

        for (std::uint32_t mask = 0; mask < kCellCount; ++mask) {
            const auto a = petra::detail::select_pad_table(mask);
            const auto b = petra::detail::select_pad_bitwidth(mask);
            const auto o = petra::detail::oracle_select(mask);
            if (!(a.score == b.score && a.fills == b.fills && a.code == b.code
                  && a.name == b.name
                  && a.score == o.score && a.fills == o.fills
                  && a.code == o.code && a.name == o.name)) {
                ++disagree;
                continue;
            }

            // Cell [3]: runtime format.
            char c3[12] = {};
            std::fill(std::begin(c3), std::end(c3), kPoison);
            auto r3 = try_format_runtime_n_padded_buffer_runtime(
                std::span<char>(c3), "v={}", a, 7);

            // Cell [4]: compile-time format.  TODAY.
            char c4[12] = {};
            std::fill(std::begin(c4), std::end(c4), kPoison);
            auto r4 = try_format_bounded_runtime_n_padded_span_runtime(
                std::span<char>(c4), "v={}", a, 7);

            // TODAY'S HEADLINE CROSS-CHECK, and it is checked FIRST: the
            // two format surfaces must agree with each other, byte for
            // byte.  (Checking it first is deliberate: the byte-level
            // expectations below are checked SECOND, so that a
            // disagreement between the two cells is attributed to
            // cell_fail rather than being masked by byte_fail.)
            if (std::string_view(c3, sizeof(c3))
                != std::string_view(c4, sizeof(c4))) {
                ++cell_fail;
                continue;
            }

            // Now the byte-level expectations: both cells wrote "v=7",
            // and the tail carries the winner's byte (or the poison, for
            // the NONE cell).
            bool ok = r3.has_value() && r4.has_value()
                      && r3.value() == 3 && r4.value() == 3
                      && std::string_view(c3, 3) == std::string_view{"v=7"}
                      && std::string_view(c4, 3) == std::string_view{"v=7"};
            if (ok) {
                const char want = a.fills ? static_cast<char>(a.code) : kPoison;
                ok = all_bytes_are(c3, 3, sizeof(c3), want)
                     && all_bytes_are(c4, 3, sizeof(c4), want);
            }
            if (!ok) {
                ++byte_fail;
                continue;
            }

            if (a.fills) {
                const std::size_t idx = static_cast<std::size_t>(a.score - 2);
                ++wins[idx];
            } else {
                ++none_wins;
            }
        }

        check_eq(disagree, 0,
                 "all 1024 cells: the three runtime selections agree");
        check_eq(byte_fail, 0,
                 "all 1024 cells: both format surfaces write the winner's "
                 "byte");
        check_eq(cell_fail, 0,
                 "all 1024 cells: the compile-time-format cell agrees with "
                 "the runtime-format cell, byte for byte");

        std::uint32_t expected = 1;
        std::uint32_t sum = 0;
        bool identity = true;
        for (std::size_t i = 0; i < wins.size(); ++i) {
            if (wins[i] != expected) identity = false;
            sum += wins[i];
            expected *= 2;
        }
        check_true(identity,
                   "win counts are 1,2,4,...,512 (the 2^(r-1) identity)");
        check_eq(sum, kCellCount - std::uint32_t{1},
                 "the win counts sum to 2^N - 1");
        check_eq(none_wins, std::uint32_t{1}, "NONE wins exactly one cell");
        check_eq(sum + none_wins, kCellCount,
                 "every one of the 1024 cells has exactly one winner");

        std::println("sweep: cells={} disagree={} byte_fail={} cell_fail={} "
                     "none_wins={}",
                     kCellCount, disagree, byte_fail, cell_fail, none_wins);
    }

    // ------------------------------------------------------------------
    // Section 7 — thread safety of today's cell.
    //
    // The cell is stateless (all per-call state is a stack local), so N
    // threads may use it concurrently with different masks.
    // ------------------------------------------------------------------
    section("Section 7: concurrent use of today's cell");
    {
        std::atomic<int> ok_count{0};
        std::atomic<int> bad_count{0};
        std::array<std::thread, 4> threads{};
        for (std::size_t t = 0; t < threads.size(); ++t) {
            threads[t] = std::thread([t, &ok_count, &bad_count]() {
                int local_ok = 0;
                int local_bad = 0;
                for (int i = 0; i < 25; ++i) {
                    const std::uint32_t mask =
                        static_cast<std::uint32_t>(
                            (static_cast<std::size_t>(t) * 25u
                             + static_cast<std::size_t>(i))
                            % kCellCount);
                    const auto choice = petra::detail::select_pad_table(mask);
                    char buf[12] = {};
                    std::fill(std::begin(buf), std::end(buf), kPoison);
                    auto r = try_format_bounded_runtime_n_padded_span_runtime(
                        std::span<char>(buf), "v={}", choice, 7);
                    bool ok = r.has_value() && r.value() == 3
                              && std::string_view(buf, 3) == std::string_view{"v=7"};
                    if (ok) {
                        const char want = choice.fills
                            ? static_cast<char>(choice.code) : kPoison;
                        ok = all_bytes_are(buf, 3, sizeof(buf), want);
                    }
                    if (ok) {
                        ++local_ok;
                    } else {
                        ++local_bad;
                    }
                }
                ok_count += local_ok;
                bad_count += local_bad;
            });
        }
        for (auto& th : threads) {
            th.join();
        }
        check_eq(ok_count.load(), 100, "100/100 concurrent calls succeeded");
        check_eq(bad_count.load(), 0, "0 concurrent calls failed");
    }

    // ------------------------------------------------------------------
    // Section 8 — the compile-time pins, reported at runtime.
    //
    // The assertions themselves are in Part 10; this section only
    // reports that the program compiled, which is the proof.
    // ------------------------------------------------------------------
    section("Section 8: the compile-time pins");
    {
        check_true(true, "the static_asserts in Part 10 compiled");
        check_true(petra::detail::format_is_valid<int>(
                       petra::detail::format_carrier{kFmtGood, 4}),
                   "the consteval validator accepts a good format string");
        check_true(petra::detail::carrier_constructible<
                       int, petra::detail::format_carrier{kFmtBadForInt, 4}>,
                   "KEY FINDING #3: the concept is satisfied even for an "
                   "INVALID format string (the check is a hard error, not "
                   "a constraint)");
        check_true(petra::detail::format_constructible<void>,
                   "KEY FINDING #3, strongest form: the concept is "
                   "satisfied even for `void`, which is not formattable "
                   "at all");
    }

    std::println("\nPASS={} FAIL={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
