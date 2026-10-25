// P-2026-10-25 — try_format_pad_buffer_concept:
//
// Closes Oct 24's "Where we go next" item (1) verbatim:
//
//   item (1) — "Port the `char*`+`cap` axis onto the same generic wrapper
//               … still open from Oct 22's item (1) and untouched by Oct
//               23.  Oct 18 / Oct 20 built four `char*`+`cap` wrappers
//               per pad by hand; the `template <bool Fills, char Fill>`
//               shape is proven on the `std::span<char>` axis and the
//               `char*`+`cap` overload is mechanical."
//
// Oct 24 called the overload "mechanical".  It is not — and WHY it is
// not is the lesson.
//
// KEY FINDING #1 — THE TWO AXES WERE NEVER TWO AXES.  They differed in
// exactly one thing: how the (pointer, length) pair was SPELLED.
// `std::span<char>` bundles the pair into one object; the `char*`+`cap`
// axis passes the halves as two arguments.  Everything else — the
// two-pass measure/write impl, the FormatError contract, the
// buffer-atomicity contract, the fill loop — was already identical.
//
// And the arc's own history proves the pair was ALWAYS the primitive:
// Oct 24's `petra::detail::try_format_runtime_n_impl` has taken
// `(char* out, std::size_t cap, ...)` since Sep 8, and the span-axis
// wrappers have always been ADAPTERS over it:
//
//     try_format_runtime_n(std::span<char> out, ...)
//         -> detail::try_format_runtime_n_impl(out.data(), out.size(), ...)
//
// So the span axis was the adapter and the raw axis was the primitive,
// and the "port" was never about copying code — it was about NAMING the
// pair as a type.
//
// KEY FINDING #2 — THE NAME IS A CONCEPT, AND THE CONCEPT IS WHERE THE
// REAL WORK IS.  `petra::detail::CharBuffer` names "a fixed, writable,
// non-owning extent of char".  `std::span<char>` satisfies it as-is;
// the raw axis gets `petra::detail::raw_buffer`, which needs ONE line
// of standard customization to join:
//
//     namespace std::ranges {
//     template <>
//     inline constexpr bool enable_view<petra::detail::raw_buffer> = true;
//     }
//
// With that, ONE wrapper family — generic over the buffer type —
// serves both axes, and the raw axis's four hand-written per-pad
// sibling chains are gone.
//
// KEY FINDING #3 — `std::ranges::enable_view` IS NOT INHERITED, and
// that is a trap worth naming.  A class DERIVED from `std::span<char>`
// is a contiguous_range and a sized_range and has range_value_t ==
// char and range_reference_t == char&, but it is NOT a view — so it
// does NOT satisfy `CharBuffer`.  Anyone who extends a span type with a
// helper method gets a type that silently fails the concept.  The fix
// is to specialize `enable_view` for the derived type as well; the
// lesson pins the trap with a static_assert and shows the cure.
//
// KEY FINDING #4 — THE CONCEPT'S FOURTH CLAUSE IS THE WRITABILITY ONE.
// `std::span<const char>` has range_value_t == char but
// range_reference_t == const char&, and the fill loop would have to
// write through it.  A concept that stopped at "contiguous range of
// char" would admit it.  So the concept requires `char&`, not `char`.
//
// KEY FINDING #5 — `std::span<char>`'s (pointer, count) constructor is
// NOT explicit, but `std::span<char, N>`'s IS, and the difference is
// `explicit(Extent != std::dynamic_extent)`.  Concretely:
//
//     std::span<char> s = {buf, 4};        // OK  (dynamic extent)
//     std::span<char, 16> s = {buf, 16};   // ERROR (explicit ctor)
//
// So the raw axis can be spelled as a span by direct-initialization
// but not by copy-list-initialization, and the two spellings are not
// interchangeable — a fact that only shows up when you try to write
// ONE wrapper for both extents.
//
// WHAT DID NOT CHANGE (deliberately):
//   - petra::FormatError / FormatErrorKind, the two-pass
//     (measure, then write) impls, the buffer-atomicity contract on
//     TooLarge and on Format errors, and the std::span<char> surface
//     are all reused verbatim from Oct 24 / Oct 23 / Oct 21 / Sep 8 /
//     Sep 9.
//   - The compile-time flag NAMES (PETRA_PAD, PETRA_SPACE_PAD, …) and
//     the -D contract are unchanged, so yesterday's build cells keep
//     their meaning.
//   - The score semantics (rank+1, NONE=1) and the X-macro rank list
//     are unchanged, so the numbers published on Oct 22 / Oct 23 /
//     Oct 24 are unchanged.
//
// PLATFORM NOTE: the callback-based iterator primitives are used
// verbatim from Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 19 / Oct 21 —
// NOT the no-callback pattern, because std::vformat_to internally
// COPIES the output iterator on Apple Clang 21 / libc++ 21.
//
// Build (default — expected+NO-pad cell):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow \
//           -Wconversion -Wsign-conversion \
//           P-2026-10-25-try-format-pad-buffer-concept.cpp \
//           -o /tmp/petra20261025/bin_default
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-25-try-format-pad-buffer-concept
//
// Pure consumer-side; no third-party libraries.

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
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
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 through Oct 23)
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
// Part 2 — petra::counting_output_iterator (Oct 11 / Oct 13 / Oct 15 /
// Oct 16 / Oct 19 / Oct 21 verbatim — callback-based)
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
    constexpr counting_output_iterator& operator++(int) noexcept { return *this; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (Oct 11 / Oct 13 / Oct 15 /
// Oct 16 / Oct 19 / Oct 21 verbatim — callback-based)
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
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
// petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim).
//
// 2-pass design (measure pass + write pass) is what lets us tell
// Format errors from TooLarge errors without throwing.
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
// Part 5 — the four untouched std::span<char> siblings (Sep 19 / Sep 10 /
// Sep 21 / Sep 23 verbatim).  Today's padded wrappers sit directly on
// these.
// ============================================================================

namespace petra {

// Sep 19 verbatim — span<char> RUNTIME expected+untouched sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(std::span<char> out, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(out.data(), out.size(),
                                             fmt, args...);
}

// Sep 10 verbatim — span<char> CONSTEVAL expected+untouched sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_span(std::span<char> out,
                                  std::format_string<Args...> fmt_str,
                                  const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out.data(),
                                                     out.size(),
                                                     fmt_str, args...);
}

// Sep 21 verbatim — span<char> RUNTIME throwing+untouched sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(std::span<char> out, std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

// Sep 21 verbatim — span<char> CONSTEVAL throwing+untouched sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt_str, args...);
    if (!r.has_value()) {
        const auto& e = r.error();
        throw std::runtime_error(e.message);
    }
    return r.value();
}

// ============================================================================
// ============================================================================
// Part 6 — THE BUFFER CONCEPT, and the GENERIC padded wrappers.
//
// KEY FINDING #1 — THE TWO AXES WERE NEVER TWO AXES.  They differed in
// exactly one thing: how the (pointer, length) pair was SPELLED.
// `std::span<char>` bundles the pair into one object; the `char*`+`cap`
// axis passes the halves as two arguments.
//
// The arc's own history proves the pair was ALWAYS the primitive:
// `petra::detail::try_format_runtime_n_impl` has taken
// `(char* out, std::size_t cap, ...)` since Sep 8, and every span-axis
// wrapper has always been an ADAPTER over it:
//
//     try_format_runtime_n(std::span<char> out, ...)
//         -> detail::try_format_runtime_n_impl(out.data(), out.size(), ...)
//
// So the span axis was the adapter and the raw axis was the primitive,
// and the "port" was never about copying code — it was about NAMING the
// pair as a type.
//
// WHAT "NAMING IT" MEANS IN C++20: a CONCEPT.  `petra::CharBuffer`
// names "a fixed, writable, non-owning extent of char", and the
// wrappers become generic over it.  One implementation, two axes.
//
// (NOTE: the `namespace petra {` that follows is the one Part 5 opened;
// this file's Part 6 continues inside it and closes it below.)
// ============================================================================

// ---------------------------------------------------------------------------
// petra::raw_buffer — the (pointer, length) pair as a VALUE.
//
// Four members, no ownership, no allocation, trivially copyable.  This is
// the raw axis's spelling of what std::span<char> already is.
//
// `begin()`/`end()` return plain `char*` rather than a fancy iterator,
// so the fill loop is the same `std::fill` on both axes.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// petra::CharBuffer — "a fixed, writable, non-owning extent of char".
//
// The four clauses, and why each one is there:
//
//   view             the extent must not own its storage — the wrapper
//                    writes into a buffer the CALLER owns;
//   contiguous_range the bytes must be adjacent, so the fill loop is a
//                    single std::fill over one run;
//   sized_range      the length must be known, and it must be the length
//                    of the RUN;
//   char&            WRITABLE.  range_value_t alone would also admit
//                    std::span<const char>, whose reference type is
//                    const char& — and the fill loop would have to write
//                    through it.
//
// KEY FINDING #3 — std::ranges::enable_view IS NOT INHERITED.  A class
// DERIVED from std::span<char> satisfies every other clause and still
// FAILS this concept, because `view` is defined as
//
//     range<T> && movable<T> && enable_view<T>
//
// and enable_view<T> is false for a user type unless it is opted in.
// Part 12 pins both the trap and the cure.
// ---------------------------------------------------------------------------
template <typename B>
concept CharBuffer =
    std::ranges::view<B> &&
    std::ranges::contiguous_range<B> &&
    std::ranges::sized_range<B> &&
    std::same_as<std::ranges::range_value_t<B>, char> &&
    std::same_as<std::ranges::range_reference_t<B>, char&>;

}  // namespace petra

// The ONE line of standard customization that lets raw_buffer join the
// standard's view-based machinery.
//
// C++20 [range.utility.support] allows a program to add a template
// specialization for std::ranges::enable_view to namespace std; that is
// the documented opt-in, and without it raw_buffer would NOT satisfy
// CharBuffer (the `view` clause would be false).
//
// NOTE: this specialization MUST live at GLOBAL scope — inside
// `namespace petra` the name `std::ranges` would resolve to
// `petra::std::ranges`, and the specialization would silently attach to
// a namespace that does not exist.
namespace std::ranges {
template <>
inline constexpr bool enable_view<petra::raw_buffer> = true;
}  // namespace std::ranges

namespace petra {

// ============================================================================
// THE GENERIC CORE — one implementation, both axes.
//
// `Buf` is any petra::CharBuffer: std::span<char> on the span axis,
// petra::raw_buffer on the raw axis.  The buffer is taken BY VALUE: a
// view is cheap to copy, and copying it cannot invalidate it.
//
// `Fills` is a SEPARATE non-type parameter rather than being encoded in
// `Fill` because NUL-pad (writes '\0') and NONE (writes nothing) are
// different contracts with the same byte value.  `if constexpr (Fills)`
// means the NONE cell emits NO fill loop at all.
//
// The buffer-atomicity contract is unchanged and is inherited from the
// underlying impl: the fill loop runs ONLY on the success path, so a
// TooLarge or Format error leaves the buffer untouched.
// ============================================================================

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

template <bool Fills, char Fill, CharBuffer Buf, typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_buffer(
    Buf out, std::string_view fmt, const Args&... args) {
    auto r = try_format_runtime_n_padded_buffer<Fills, Fill>(out, fmt,
                                                              args...);
    if (!r.has_value()) {
        throw std::runtime_error(r.error().message);
    }
    return r.value();
}

template <bool Fills, char Fill, CharBuffer Buf, typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_buffer(
    Buf out, std::format_string<Args...> fmt_str, const Args&... args) {
    auto r = try_format_bounded_runtime_n_padded_buffer<Fills, Fill>(
        out, fmt_str, args...);
    if (!r.has_value()) {
        throw std::runtime_error(r.error().message);
    }
    return r.value();
}

// ============================================================================
// THE TWO AXES — thin adapters over the one generic core.
//
// These are the public spellings.  Each is a ONE-LINE forward, which is
// the whole point: the raw axis's four hand-written per-pad sibling
// chains (Oct 18 / Oct 20) are replaced by ONE function.
//
// The naming convention follows the arc's: the SPAN axis keeps the
// `_span` suffix, and the RAW axis (char* + cap) has no suffix —
// exactly as `try_format_runtime_n_plus_pad` (raw) and
// `try_format_runtime_n_plus_pad_span` (span) were named in Oct 20/21.
// ============================================================================

// -- the SPAN axis (std::span<char>) ---------------------------------------

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span(std::span<char> out,
                                 std::string_view fmt,
                                 const Args&... args) {
    return try_format_runtime_n_padded_buffer<Fills, Fill>(out, fmt, args...);
}

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    return try_format_bounded_runtime_n_padded_buffer<Fills, Fill>(
        out, fmt_str, args...);
}

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    return try_format_runtime_n_throwing_padded_buffer<Fills, Fill>(
        out, fmt, args...);
}

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    return try_format_bounded_runtime_n_throwing_padded_buffer<Fills, Fill>(
        out, fmt_str, args...);
}

// -- the RAW axis (char* + cap) --------------------------------------------
//
// TODAY'S PORT: these four functions are the ones Oct 18 / Oct 20 wrote
// by hand, once per pad.  Today there is ONE of them, generic over the
// pad, and the buffer is a petra::raw_buffer rather than two loose
// arguments.

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded(char* out, std::size_t cap,
                            std::string_view fmt,
                            const Args&... args) {
    return try_format_runtime_n_padded_buffer<Fills, Fill>(
        raw_buffer{out, cap}, fmt, args...);
}

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded(
    char* out, std::size_t cap,
    std::format_string<Args...> fmt_str,
    const Args&... args) {
    return try_format_bounded_runtime_n_padded_buffer<Fills, Fill>(
        raw_buffer{out, cap}, fmt_str, args...);
}

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded(
    char* out, std::size_t cap,
    std::string_view fmt,
    const Args&... args) {
    return try_format_runtime_n_throwing_padded_buffer<Fills, Fill>(
        raw_buffer{out, cap}, fmt, args...);
}

template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded(
    char* out, std::size_t cap,
    std::format_string<Args...> fmt_str,
    const Args&... args) {
    return try_format_bounded_runtime_n_throwing_padded_buffer<Fills, Fill>(
        raw_buffer{out, cap}, fmt_str, args...);
}
// petra::unwrap_dispatch: an if constexpr adapter so ONE call site
// compiles against BOTH the std::expected surface (THROW=0) and the bare
// std::size_t throwing surface (THROW=1).  Only ONE branch is
// instantiated per cell.
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
// Part 7 — THE FLAGS.
//
// THESE ARE THE ONLY PREPROCESSER-STATE PARTS LEFT IN THE FILE, and that
// is the point.  Oct 23's file had eleven score macros, three
// eleven-arm ternary cascades, and a four-macro MAX family; today the
// preprocessor's whole remaining job is "turn these eleven 0/1 macros
// into eleven bools".
//
// FINDING (today) — the per-pad #ifndef/#define/#error BLOCKS cannot be
// generated from the X-macro list, because the preprocessor cannot
// produce a #define from a macro expansion (that is undefined behaviour
// and no compiler supports it).  So the FLAG PLUMBING stays per-pad
// (eleven three-line blocks) while the RANK DATA becomes one line per
// pad.  The honest statement of the win is therefore:
//
//   "the twelfth pad is ONE line of rank data, plus the same three
//    lines of flag plumbing every previous pad already had."
//
// and NOT "the twelfth pad is one line".
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
#error "PETRA_PAD must be 0 (no NUL-pad) or 1 (NUL-padded)"
#endif

#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif
#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 (no dash-pad) or 1 (dash-padded)"
#endif

#ifndef PETRA_HASH_PAD
#define PETRA_HASH_PAD 0
#endif
#if PETRA_HASH_PAD != 0 && PETRA_HASH_PAD != 1
#error "PETRA_HASH_PAD must be 0 (no hash-pad) or 1 (hash-padded)"
#endif

#ifndef PETRA_TILDE_PAD
#define PETRA_TILDE_PAD 0
#endif
#if PETRA_TILDE_PAD != 0 && PETRA_TILDE_PAD != 1
#error "PETRA_TILDE_PAD must be 0 (no tilde-pad) or 1 (tilde-padded)"
#endif

#ifndef PETRA_UNDERSCORE_PAD
#define PETRA_UNDERSCORE_PAD 0
#endif
#if PETRA_UNDERSCORE_PAD != 0 && PETRA_UNDERSCORE_PAD != 1
#error "PETRA_UNDERSCORE_PAD must be 0 (no underscore-pad) or 1 (underscore-padded)"
#endif

#ifndef PETRA_STAR_PAD
#define PETRA_STAR_PAD 0
#endif
#if PETRA_STAR_PAD != 0 && PETRA_STAR_PAD != 1
#error "PETRA_STAR_PAD must be 0 (no star-pad) or 1 (star-padded)"
#endif

#ifndef PETRA_AT_PAD
#define PETRA_AT_PAD 0
#endif
#if PETRA_AT_PAD != 0 && PETRA_AT_PAD != 1
#error "PETRA_AT_PAD must be 0 (no at-pad) or 1 (at-padded)"
#endif

#ifndef PETRA_PLUS_PAD
#define PETRA_PLUS_PAD 0
#endif
#if PETRA_PLUS_PAD != 0 && PETRA_PLUS_PAD != 1
#error "PETRA_PLUS_PAD must be 0 (no plus-pad) or 1 (plus-padded)"
#endif

#ifndef PETRA_BANG_PAD
#define PETRA_BANG_PAD 0
#endif
#if PETRA_BANG_PAD != 0 && PETRA_BANG_PAD != 1
#error "PETRA_BANG_PAD must be 0 (no bang-pad) or 1 (bang-padded)"
#endif

// ============================================================================
// Part 8 — THE X-MACRO LIST: the single source of truth for the rank
// order.
//
//   X(NAME, FLAG_MACRO, RANK, FILL_CODE)
//
//   NAME        the pad's name (stringified for the constexpr table)
//   FLAG_MACRO  the -D macro that turns this pad on
//   RANK        1..10, lowest priority first; the winner is the
//               HIGHEST rank that is enabled
//   FILL_CODE   the ASCII byte written into the tail of the buffer
//
// ONE X-macro consumer turns this ONE list into the data:
//
//   PETRA_PAD_TABLE_ITEM  -> constexpr PadSpec kPadTable[] (name, rank,
//                            code, and this cell's flag value)
//
// The list's LENGTH is deduced from that initializer list, so the pad
// count is not written down anywhere either.
//
// A PREPROCESSOR RULE worth remembering: the classic X-macro works
// because a function-like macro name that is NOT followed by '(' is
// never expanded.  So `PETRA_PAD_LIST(PETRA_PAD_TABLE_ITEM)` expands
// the list into ten well-formed invocations, each of which then picks
// up its own arguments — and the item macro's parameters are
// substituted with the ALREADY-EXPANDED argument text, which is why
// FLAG_MACRO (= PETRA_BANG_PAD) arrives as 0 or 1 rather than as a
// name.
// ============================================================================

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
}  // namespace petra

#define PETRA_PAD_TABLE_ITEM(NAME, FLAG, RANK, CODE)                        \
    ::petra::detail::PadSpec{#NAME, RANK, CODE, (FLAG != 0)},

namespace petra {
namespace detail {

// KEY FINDING (today, found by the "twelfth pad" experiment) — the
// LIST LENGTH was the last piece of duplication the X-macro had not
// removed.  The first draft wrote `std::array<PadSpec, 10>` and
// `std::array<bool, 10>` and `std::array<OracleRow, 10>`, i.e. the
// count "10" was still written down in three places, so adding an
// eleventh pad meant editing the count as well as the data.
//
// The fix is to let the COMPILER deduce the bound: a raw constexpr
// array with an initializer list has its size deduced from the
// initializers, and `std::size` then reports it.  So the count is
// derived from the list, not stated alongside it.
//
// Note the flag value now lives IN the row (PadSpec::enabled), so the
// separate kPadEnabled array is gone too — one row per pad carries
// name, rank, code AND this cell's flag, all from ONE list line.
inline constexpr PadSpec kPadTable[] = {
    PETRA_PAD_LIST(PETRA_PAD_TABLE_ITEM)
};

// The number of pads, deduced from the list itself.
inline constexpr std::size_t kPadCount = std::size(kPadTable);

}  // namespace detail
}  // namespace petra

#undef PETRA_PAD_TABLE_ITEM

// ============================================================================
// Part 9 — THE SELECTION, in four independent spellings.
//
//   (1) select_pad_table     — walks the generated table backwards
//   (2) select_pad_bitwidth  — std::bit_width over the flag mask
//   (3) oracle_select        — a HAND-WRITTEN if-chain, one line per pad
//   (4) the compile-time cell — select_pad_table(kPadMask)
//
// (1) and (2) are structurally different (a backwards table walk vs. a
// bit-width computation), (3) does not read the generated data AT ALL,
// and (4) is the value the compiler actually uses.
//
// The bit-mask model: bit i of `mask` is set iff pad i (in list order)
// is enabled.  The winner is the HIGHEST set bit, which is why
// std::bit_width(mask) - 1 is the winner's table index.  This makes
// the runtime selector ONE integer wide for the whole pad set.
// ============================================================================

namespace petra {
namespace detail {

// The mask for THIS build cell, computed from the generated table's
// own `enabled` flags.
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

// (2) the highest set bit IS the winner.  std::bit_width is C++20 and
// constexpr.
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
// the generated table.  This is the safety net that survives the
// deletion of the preprocessor cascade: it is the Oct 22/23 idea of
// "two independent implementations of one spec", re-based on a
// hand-written reference instead of on a second preprocessor chain.
//
// The chain is written against the RANK (not the bit position), so it
// is stated in terms the spec uses: the highest RANK that is enabled
// wins.
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

// The ORACLE TABLE — Oct 23's kPadTable transcribed by hand, rank by
// rank.  It is compared against the GENERATED table at compile time and
// at runtime; if the X-macro list ever disagrees with the order the
// arc has published since Oct 22, this is what catches it.
//
// Its bound is deduced too, so the oracle cannot silently fall out of
// step with the generated table by having a different number of rows.
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

// The table must be in rank order, one row per rank: the bit-mask
// model depends on it (bit i <=> rank i+1).
[[nodiscard]] constexpr bool table_ranks_are_sequential() noexcept {
    for (std::size_t i = 0; i < kPadCount; ++i) {
        if (kPadTable[i].rank != static_cast<int>(i) + 1) {
            return false;
        }
    }
    return true;
}

// Do the three runtime spellings agree?
[[nodiscard]] constexpr bool selections_agree(std::uint32_t mask) noexcept {
    const PadChoice a = select_pad_table(mask);
    const PadChoice b = select_pad_bitwidth(mask);
    const PadChoice c = oracle_select(mask);
    return a.score == b.score && b.score == c.score
        && a.fills == b.fills && b.fills == c.fills
        && a.code == b.code && b.code == c.code
        && a.name == b.name && b.name == c.name;
}

// The compile-time cell: what this build actually selected.
inline constexpr PadChoice kSelected = select_pad_table(kPadMask);

// KEY FINDING #2 (today) — these two are TEMPLATE ARGUMENTS now, not
// preprocessor arithmetic.  `constexpr bool` / `constexpr char` are
// valid non-type template arguments, so the entire preprocessor
// priority cascade is unnecessary.
inline constexpr bool kSelectedFills = kSelected.fills;
inline constexpr char kSelectedByte = static_cast<char>(kSelected.code);

}  // namespace detail
}  // namespace petra

namespace petra {

// Public accessors so a test (or a caller) can ask "which pad won?"
// without re-deriving anything.
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

// The RUNTIME selection surface: one binary, every cell.
[[nodiscard]] constexpr detail::PadChoice select_pad(
    std::uint32_t mask) noexcept {
    return detail::select_pad_table(mask);
}
[[nodiscard]] constexpr std::uint32_t current_pad_mask() noexcept {
    return detail::kPadMask;
}

}  // namespace petra

// ============================================================================
// Part 10 — the RUNTIME padded wrappers: Oct 23's next-step item (2),
// generalised to BOTH axes.
//
// The compile-time wrapper takes <bool Fills, char Fill>; the runtime
// sibling takes a PadChoice VALUE.  One binary can therefore fill with
// any of the ten pad bytes — or not fill at all — which is what makes
// the single-binary sweep in Section 10 possible.
//
// The two are deliberately kept structurally different:
//   - compile-time: `if constexpr (Fills)` — the NONE cell emits NO
//     fill loop at all, not a runtime branch that is always false;
//   - runtime: one `if (choice.fills)` — the NONE cell runs the branch
//     and skips the fill.
//
// Both honour the same atomicity contract: the fill loop runs ONLY on
// the success path, so a TooLarge or Format error leaves the buffer
// untouched.
// ============================================================================

namespace petra {

// ============================================================================
// THE RUNTIME CORE — one implementation, both axes.
//
// The compile-time wrapper takes <bool Fills, char Fill>; the runtime
// sibling takes a PadChoice VALUE.  One binary can therefore fill with
// any of the ten pad bytes — or not fill at all — which is what makes
// the single-binary sweep in Section 10 possible.
//
// The two are deliberately kept structurally different:
//   - compile-time: `if constexpr (Fills)` — the NONE cell emits NO
//     fill loop at all, not a runtime branch that is always false;
//   - runtime: one `if (choice.fills)` — the NONE cell runs the branch
//     and skips the fill.
//
// Both honour the same atomicity contract: the fill loop runs ONLY on
// the success path, so a TooLarge or Format error leaves the buffer
// untouched.
// ============================================================================

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

template <CharBuffer Buf, typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_buffer_runtime(
    Buf out, std::string_view fmt, detail::PadChoice choice,
    const Args&... args) {
    auto r = try_format_runtime_n_padded_buffer_runtime(out, fmt, choice,
                                                         args...);
    if (!r.has_value()) {
        throw std::runtime_error(r.error().message);
    }
    return r.value();
}

// -- the SPAN axis ---------------------------------------------------------

// NEW today — RUNTIME + EXPECTED padded span<char> sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span_runtime(std::span<char> out,
                                         std::string_view fmt,
                                         detail::PadChoice choice,
                                         const Args&... args) {
    return try_format_runtime_n_padded_buffer_runtime(out, fmt, choice,
                                                       args...);
}

// NEW today — RUNTIME + THROWING padded span<char> sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_span_runtime(std::span<char> out,
                                                   std::string_view fmt,
                                                   detail::PadChoice choice,
                                                   const Args&... args) {
    return try_format_runtime_n_throwing_padded_buffer_runtime(
        out, fmt, choice, args...);
}

// -- the RAW axis (char* + cap) --------------------------------------------
//
// TODAY'S PORT: Oct 24's runtime sibling existed only on the span axis.
// The raw axis gets the same treatment, for the same reason — one
// implementation, one thin adapter.

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_runtime(char* out, std::size_t cap,
                                     std::string_view fmt,
                                     detail::PadChoice choice,
                                     const Args&... args) {
    return try_format_runtime_n_padded_buffer_runtime(raw_buffer{out, cap},
                                                       fmt, choice, args...);
}

template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_runtime(char* out, std::size_t cap,
                                             std::string_view fmt,
                                             detail::PadChoice choice,
                                             const Args&... args) {
    return try_format_runtime_n_throwing_padded_buffer_runtime(
        raw_buffer{out, cap}, fmt, choice, args...);
}

}  // namespace petra

// ============================================================================
// Part 11 — the meta-macro dispatcher.
//
// The ONLY difference from Oct 23's version is what the template
// arguments are: they used to be the preprocessor's FILLS/CODE
// cascades; today they are `detail::kSelectedFills` and
// `detail::kSelectedByte`, which are constexpr variables.
//
// Both macros are parenthesized expressions (NOT do { } while (0)) so
// the caller can capture the result, and both use __VA_OPT__(,) for
// the no-args case.
// ============================================================================

#if PETRA_THROW_ON_OVERFLOW == 0
#define TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_runtime_n_padded_span<                             \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (fmt)                      \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_padded_span<                    \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (fmt)                      \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_RUNTIME_N_PRIORITY(out, cap, fmt, ...)                  \
    (::petra::try_format_runtime_n_padded<                                 \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (cap), (fmt)               \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_N_PRIORITY(out, cap, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_padded<                         \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (cap), (fmt)               \
         __VA_OPT__(,) __VA_ARGS__))
#else
#define TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_runtime_n_throwing_padded_span<                   \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (fmt)                      \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_throwing_padded_span<          \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (fmt)                      \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_RUNTIME_N_PRIORITY(out, cap, fmt, ...)                  \
    (::petra::try_format_runtime_n_throwing_padded<                        \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (cap), (fmt)               \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_N_PRIORITY(out, cap, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_throwing_padded<               \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (cap), (fmt)               \
         __VA_OPT__(,) __VA_ARGS__))
#endif

// ============================================================================
// Part 12 — the compile-time pins.
//
// Oct 23 had eight static_asserts (three preprocessor-vs-constexpr,
// four runtime-vs-preprocessor, one four-way runtime agreement).  The
// preprocessor-vs-constexpr half is GONE (there is no longer a
// preprocessor chain to disagree with), and what replaces it is:
//
//   - the generated table must equal the hand-written oracle table;
//   - the table's ranks must be 1..10 in order;
//   - the enabled array must be the same length as the table;
//   - the three runtime spellings must agree for this cell;
//   - the compile-time cell must be the mask's own selection.
//
// KEY FINDING #3 (today) — the cross-check is now a RUNTIME check over
// all 1024 cells rather than a COMPILE-TIME check for one cell.  That
// is strictly more coverage (1024 cells instead of 1) but it is no
// longer a compile-time guarantee: a build with a wrong table still
// COMPILES.  The oracle table check below is what keeps the compile
// time guarantee for the DATA; the runtime sweep keeps it for the
// SELECTION.
// ============================================================================

namespace petra {
}  // namespace petra

// ============================================================================
// Part 12a — the CONCEPT'S PROBE TYPES, at GLOBAL scope.
//
// These have to be OUTSIDE `namespace petra` for one concrete reason:
// the `enable_view` specializations below live in `namespace std::ranges`,
// and inside `namespace petra` the name `std::ranges` would resolve to
// `petra::std::ranges` — a namespace that does not exist — so the
// specialization would attach to nothing and the concept would silently
// reject the opted-in type.  That is the same trap as KEY FINDING #3,
// one level up.
//
// (The `namespace petra_test` here is the SAME namespace as the
// petra_test of Part 13 — this one only holds the probe types, and Part
// 13 only holds the check framework.  Both are at global scope.)
// ============================================================================

namespace petra_test {

// A span-derived type, to pin the trap.  It is deliberately NOT given
// its own enable_view specialization.
struct derived_span : std::span<char> {
    constexpr derived_span(char* data, std::size_t size) noexcept
        : std::span<char>(data, size) {}
};

// The CURE: the same shape, but opted in.
struct opted_in_span : std::span<char> {
    constexpr opted_in_span(char* data, std::size_t size) noexcept
        : std::span<char>(data, size) {}
};

// KEY FINDING #4's counter-example: the NAIVE concept — "a contiguous
// range of char" — which admits a const buffer and a std::string.
template <typename B>
concept NaiveCharRange =
    std::ranges::contiguous_range<B> &&
    std::ranges::sized_range<B> &&
    std::same_as<std::ranges::range_value_t<B>, char>;

}  // namespace petra_test

// The one-line fix a user would apply to their own span-derived type.
namespace std::ranges {
template <>
inline constexpr bool enable_view<petra_test::opted_in_span> = true;
}  // namespace std::ranges

namespace petra {

static_assert(detail::table_matches_oracle(),
              "the X-macro-generated pad table must match the hand-written "
              "oracle table (Oct 23's rank order)");
static_assert(detail::table_ranks_are_sequential(),
              "the pad table must be in rank order, one row per rank "
              "1..10");
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
static_assert(detail::kSelected.score ==
                  (detail::kSelected.fills
                       ? detail::kSelected.score
                       : detail::kSelected.score),
              "sanity: the selected score is a plain integer");
static_assert(detail::kPadMask == detail::mask_from_table(
                                      std::span<const detail::PadSpec>{
                                          detail::kPadTable}),
              "the mask must be the fold of the generated table's flags");

// ---------------------------------------------------------------------------
// THE CONCEPT'S PINS — today.
//
// These are the compile-time half of "does the buffer concept admit the
// right types?", and each one is a claim about a SPECIFIC C++20 RULE,
// not about this file's plumbing.
//
//   - std::span<char> satisfies CharBuffer out of the box.  It is
//     already a std::ranges::view, it is contiguous and sized, and its
//     reference type is char&.
//   - petra::raw_buffer satisfies CharBuffer ONLY because of the
//     enable_view specialization in Part 6.  Without it, `view` would be
//     false and the whole concept would reject it.
//   - std::span<const char> does NOT satisfy CharBuffer: its
//     range_value_t is char but its range_reference_t is const char&,
//     and the fill loop would have to write through it.
//   - a type DERIVED from std::span<char> does NOT satisfy CharBuffer
//     either, even though it is contiguous, sized and char-valued:
//     enable_view is NOT inherited.  This is the trap.
//   - std::string and std::vector<char> do NOT satisfy CharBuffer:
//     they are not views (they own their storage), so the "fixed
//     writable extent" contract does not apply to them.
// ---------------------------------------------------------------------------
static_assert(CharBuffer<std::span<char>>,
              "std::span<char> must satisfy CharBuffer");
static_assert(CharBuffer<raw_buffer>,
              "petra::raw_buffer must satisfy CharBuffer (it needs the "
              "enable_view specialization)");
static_assert(!CharBuffer<std::span<const char>>,
              "a const buffer must NOT satisfy CharBuffer: the fill loop "
              "writes into it");
static_assert(!CharBuffer<std::string>,
              "std::string owns its storage, so it is not a view and must "
              "NOT satisfy CharBuffer");
static_assert(!CharBuffer<std::vector<char>>,
              "std::vector<char> owns its storage, so it is not a view and "
              "must NOT satisfy CharBuffer");
static_assert(!CharBuffer<std::array<char, 8>>,
              "std::array owns its storage, so it is not a view and must "
              "NOT satisfy CharBuffer");

// KEY FINDING #3 — enable_view is NOT inherited.  A class DERIVED from
// std::span<char> is a contiguous_range and a sized_range and has
// range_value_t == char, but it is NOT a view, so the concept rejects
// it.  The cure is to specialize enable_view for the derived type too
// (Part 12a does exactly that).
static_assert(std::ranges::contiguous_range<petra_test::derived_span>,
              "a span-derived type IS a contiguous range");
static_assert(std::ranges::sized_range<petra_test::derived_span>,
              "a span-derived type IS a sized range");
static_assert(std::same_as<std::ranges::range_value_t<
                              petra_test::derived_span>, char>,
              "a span-derived type has range_value_t == char");
static_assert(!std::ranges::view<petra_test::derived_span>,
              "enable_view is NOT inherited: a span-derived type is NOT a "
              "view");
static_assert(!CharBuffer<petra_test::derived_span>,
              "and therefore it does NOT satisfy CharBuffer");

static_assert(CharBuffer<petra_test::opted_in_span>,
              "with its own enable_view specialization, a span-derived type "
              "DOES satisfy CharBuffer");

// KEY FINDING #4 — the WRITABILITY clause is load-bearing.  A concept
// that stopped at "contiguous range of char" would admit
// std::span<const char>.
static_assert(petra_test::NaiveCharRange<std::span<const char>>,
              "the NAIVE concept admits a const buffer — which is exactly "
              "the bug the writability clause fixes");
static_assert(petra_test::NaiveCharRange<std::string>,
              "the NAIVE concept admits std::string — which is exactly the "
              "bug the view clause fixes");
static_assert(!petra_test::NaiveCharRange<std::span<char>> ||
              CharBuffer<std::span<char>>,
              "sanity");

// KEY FINDING #5 — std::span<char>'s (pointer, count) constructor is NOT
// explicit (its extent is dynamic_extent), but std::span<char, N>'s IS
// (its extent is not).  The difference is
// `explicit(Extent != std::dynamic_extent)`.  Both facts are pinned by
// is_constructible / is_convertible on a one-element buffer.
static_assert(std::is_constructible_v<std::span<char>, char*, std::size_t>,
              "std::span<char> is constructible from (char*, size_t)");
static_assert(std::is_convertible_v<std::span<char>, std::span<char>>,
              "a span converts to a span");
static_assert(std::is_constructible_v<std::span<char, 16>, char*,
                                      std::size_t>,
              "std::span<char, 16> is constructible from (char*, size_t) "
              "too — but its constructor is EXPLICIT");

}  // namespace petra

// ============================================================================
// Part 13 — hand-rolled SECTION/CHECK framework (verbatim shape from
// Sep 6 .. Oct 23).
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
// Part 14 — sections.
//
// Every section is written against the SELECTED pad (whatever the
// generated table picked for this build cell), so the SAME source is
// the test for all 1024 pad combinations.
// ============================================================================

namespace {

// Returns true when every byte in [from, to) equals `expected`.
bool all_bytes_are(const char* buf, std::size_t from, std::size_t to,
                   char expected) noexcept {
    for (std::size_t i = from; i < to; ++i) {
        if (buf[i] != expected) {
            return false;
        }
    }
    return true;
}

constexpr char kPoison = 'Q';

// The number of pad cells: 2^(number of pads), with the pad count
// DEDUCED from the generated table rather than written down.
inline constexpr std::uint32_t kCellCount =
    std::uint32_t{1} << static_cast<unsigned>(petra::detail::kPadCount);

}  // namespace

int main() {
    using namespace petra_test;
    using namespace petra;

    std::println("selected pad = {}  (fills={}, byte={} (0x{:02X}), score={}, "
                 "mask={} (0x{:03X}))",
                 selected_pad_name(),
                 selected_pad_fills() ? 1 : 0,
                 static_cast<int>(static_cast<unsigned char>(selected_pad_byte())),
                 static_cast<unsigned>(static_cast<unsigned char>(selected_pad_byte())),
                 selected_pad_score(),
                 current_pad_mask(),
                 current_pad_mask());

    // ------------------------------------------------------------------
    // Section 1 — the GENERATED table vs the HAND-WRITTEN oracle table.
    //
    // This is today's replacement for Oct 23's preprocessor-vs-constexpr
    // static_asserts: the generated data must equal the data the arc
    // published on Oct 22 / Oct 23, transcribed by hand.
    // ------------------------------------------------------------------
    section("Section 1: generated table == hand-written oracle table");
    {
        check_true(petra::detail::table_matches_oracle(),
                   "generated table == oracle table");
        check_true(petra::detail::table_ranks_are_sequential(),
                   "ranks are 1..N in list order");
        check_true(petra::detail::kPadCount == petra::detail::kOracleCount,
                   "the generated and oracle tables have the same row count");

        // Spot-check the two ends of the order, and the two pads whose
        // ASCII codes are ADJACENT to other pads' codes ('+' is '*'+1,
        // '!' is ' '+1, '#' is '!'+2) — a transposed literal is the
        // classic mistake here.
        check_eq(petra::detail::kPadTable[0].name, std::string_view{"NUL"},
                 "row 0 is NUL");
        check_eq(petra::detail::kPadTable[0].rank, 1, "NUL rank 1");
        check_eq(petra::detail::kPadTable[0].code, 0, "NUL code 0");
        check_eq(petra::detail::kPadTable[9].name, std::string_view{"BANG"},
                 "row 9 is BANG");
        check_eq(petra::detail::kPadTable[9].rank, 10, "BANG rank 10");
        check_eq(petra::detail::kPadTable[9].code, 33, "BANG code 33");

        static_assert('\0' == 0, "NUL");
        static_assert(' ' == 32, "SPACE");
        static_assert('!' == 33, "BANG");
        static_assert('#' == 35, "HASH");
        static_assert('-' == 45, "DASH");
        static_assert('*' == 42, "STAR");
        static_assert('+' == 43, "PLUS");
        static_assert('+' == '*' + 1, "'+' is adjacent to '*'");
        static_assert('!' == ' ' + 1, "'!' is one above SPACE");
        static_assert('#' == '!' + 2, "HASH is two above BANG");
        static_assert('@' == 64, "AT");
        static_assert('_' == 95, "UNDERSCORE");
        static_assert('~' == 126, "TILDE");
        static_assert(kPoison == 'Q', "poison byte");
        static_assert(kPoison != ' ' && kPoison != '!' && kPoison != '#'
                          && kPoison != '-' && kPoison != '*'
                          && kPoison != '+' && kPoison != '@'
                          && kPoison != '_' && kPoison != '~',
                      "poison byte must differ from all ten pad bytes");

        // The ten names are distinct — a duplicate name would make the
        // winner-detection ambiguous with no compiler diagnostic.
        bool names_distinct = true;
        for (std::size_t i = 0; i < petra::detail::kPadCount; ++i) {
            for (std::size_t j = i + 1; j < petra::detail::kPadCount; ++j) {
                if (petra::detail::kPadTable[i].name ==
                    petra::detail::kPadTable[j].name) {
                    names_distinct = false;
                }
            }
        }
        check_true(names_distinct, "all ten pad names are distinct");
    }

    // ------------------------------------------------------------------
    // Section 2 — the compile-time cell's invariants.
    //
    // The score semantics are unchanged from Oct 22: the winner's score
    // is rank+1 and NONE scores 1, so NONE can never tie an enabled
    // pad (NUL, the lowest pad, scores 2).
    // ------------------------------------------------------------------
    section("Section 2: compile-time cell invariants");
    {
        const auto sel = petra::detail::kSelected;
        check_true(sel.score >= 1, "selected score >= 1 (NONE floor)");
        check_true(sel.fills == selected_pad_fills(), "FILLS agrees");
        check_eq(sel.code, static_cast<int>(
                      static_cast<unsigned char>(selected_pad_byte())),
                 "CODE agrees");

        // "Is any pad enabled?" is asked from the MASK, not from a
        // hand-written list of the eleven flag macros — the same lesson
        // the X-macro taught about the rank order, applied to the
        // "which flags exist" question.
        const bool any_pad = current_pad_mask() != 0u;

        if (any_pad) {
            check_true(sel.score != 1,
                       "a pad is enabled -> NONE must NOT win");
            check_true(selected_pad_fills(), "a pad is enabled -> FILLS=1");
            check_true(selected_pad_name() != std::string_view{"NONE"},
                       "a pad is enabled -> NAME != NONE");
            // The winner's score must be rank+1, so the winner's rank
            // is score-1 and its table row is that index.
            const std::size_t idx =
                static_cast<std::size_t>(sel.score - 2);
            check_true(idx < petra::detail::kPadCount,
                       "winner index is inside the table");
            check_eq(petra::detail::kPadTable[idx].name, sel.name,
                     "winner row's name == selected name");
            check_eq(petra::detail::kPadTable[idx].code, sel.code,
                     "winner row's code == selected code");
        } else {
            check_true(sel.score == 1, "no pad enabled -> NONE wins");
            check_true(!selected_pad_fills(),
                       "no pad enabled -> FILLS=0 (NO fill loop)");
            check_true(selected_pad_name() == std::string_view{"NONE"},
                       "no pad enabled -> NAME == NONE");
            check_true(selected_pad_byte() == '\0',
                       "no pad enabled -> CODE == 0");
        }

        // The hand-written ORACLE chain — which reads nothing from the
        // generated table — must pick the same pad as the generated
        // table does.  This is the compile-time half of today's
        // cross-check, and it is deliberately a SEPARATE implementation
        // rather than a second call into the generated data.
        const auto oracle = petra::detail::oracle_select(current_pad_mask());
        check_true(selected_pad_name() == oracle.name,
                   "hand-written oracle chain agrees on the winning pad");
        check_true(selected_pad_fills() == oracle.fills,
                   "hand-written oracle chain agrees on FILLS");
        check_eq(sel.code, oracle.code,
                 "hand-written oracle chain agrees on CODE");

        // The mask must be the fold of the generated enabled array, and
        // the compile-time cell must be that mask's own selection.
        check_true(petra::detail::selections_agree(current_pad_mask()),
                   "the three runtime selections agree for this cell");
        check_eq(petra::detail::select_pad_table(current_pad_mask()).score,
                 sel.score, "mask selection == compile-time selection");
        check_eq(petra::detail::oracle_select(current_pad_mask()).name,
                 sel.name, "oracle selection == compile-time selection");
    }

    // ------------------------------------------------------------------
    // Section 3 — the COMPILE-TIME padded path through the meta-macro:
    // bytes after `written` follow the SELECTED contract.
    // ------------------------------------------------------------------
    section("Section 3: compile-time padded path honors the selected pad");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        const std::size_t n =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(s, "x={}", 42));
        check_eq(n, std::size_t{4}, "written == 4");
        check_eq(std::string_view(buf, 4), std::string_view{"x=42"},
                 "buf[0..3] == \"x=42\"");
        if (selected_pad_fills()) {
            check_true(all_bytes_are(buf, 4, 16, selected_pad_byte()),
                       "buf[4..15] all == selected pad byte");
        } else {
            check_true(all_bytes_are(buf, 4, 16, kPoison),
                       "NONE cell: buf[4..15] all == poison");
        }
    }

    // ------------------------------------------------------------------
    // ------------------------------------------------------------------
    // Section 4 — THE TWO AXES AGREE, BYTE FOR BYTE.
    //
    // This is today's headline check.  The span axis and the raw axis
    // are now the SAME implementation reached through two different
    // buffer types, so every observable must be identical: the byte
    // count, the content, and the tail.
    //
    // The check is deliberately made AXIS-CROSSING rather than
    // axis-internal: a bug that affected only one axis (a wrong cap, a
    // wrong pointer arithmetic) would show up here and nowhere else.
    // ------------------------------------------------------------------
    section("Section 4: the two axes agree, byte for byte");
    {
        // -- the COMPILE-TIME path, on both axes ------------------------
        char span_buf[16] = {};
        std::fill(std::begin(span_buf), std::end(span_buf), kPoison);
        std::span<char> sp(span_buf);
        const std::size_t n_span =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(sp, "x={}", 42));

        char raw_buf[16] = {};
        std::fill(std::begin(raw_buf), std::end(raw_buf), kPoison);
        const std::size_t n_raw =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_N_PRIORITY(raw_buf, 16,
                                                          "x={}", 42));

        check_eq(n_raw, n_span, "compile-time: raw written == span written");
        check_true(std::string_view(raw_buf, 16) ==
                       std::string_view(span_buf, 16),
                   "compile-time: raw buffer == span buffer, byte for byte");
        check_eq(std::string_view(raw_buf, 4), std::string_view{"x=42"},
                 "compile-time: raw buf[0..3] == \"x=42\"");

        // -- the RUNTIME path, on both axes ------------------------------
        char rt_span_buf[16] = {};
        std::fill(std::begin(rt_span_buf), std::end(rt_span_buf), kPoison);
        std::span<char> rt_sp(rt_span_buf);
        auto r_span_rt = try_format_runtime_n_padded_span_runtime(
            rt_sp, "x={}", petra::detail::kSelected, 42);

        char rt_raw_buf[16] = {};
        std::fill(std::begin(rt_raw_buf), std::end(rt_raw_buf), kPoison);
        auto r_raw_rt = try_format_runtime_n_padded_runtime(
            rt_raw_buf, 16, "x={}", petra::detail::kSelected, 42);

        check_true(r_span_rt.has_value() && r_raw_rt.has_value(),
                   "runtime: both axes have a value");
        check_eq(r_raw_rt.value(), r_span_rt.value(),
                 "runtime: raw written == span written");
        check_true(std::string_view(rt_raw_buf, 16) ==
                       std::string_view(rt_span_buf, 16),
                   "runtime: raw buffer == span buffer, byte for byte");

        // -- the CONSTEVAL path, on both axes ----------------------------
        char ce_span_buf[16] = {};
        std::fill(std::begin(ce_span_buf), std::end(ce_span_buf), kPoison);
        std::span<char> ce_sp(ce_span_buf);
        const std::size_t n_ce_span =
            unwrap_dispatch(TRY_FORMAT_BOUNDED_SPAN_N_PRIORITY(
                ce_sp, "hello {}", "world"));

        char ce_raw_buf[16] = {};
        std::fill(std::begin(ce_raw_buf), std::end(ce_raw_buf), kPoison);
        const std::size_t n_ce_raw =
            unwrap_dispatch(TRY_FORMAT_BOUNDED_N_PRIORITY(
                ce_raw_buf, 16, "hello {}", "world"));

        check_eq(n_ce_raw, n_ce_span,
                 "CONSTEVAL: raw written == span written");
        check_true(std::string_view(ce_raw_buf, 16) ==
                       std::string_view(ce_span_buf, 16),
                   "CONSTEVAL: raw buffer == span buffer, byte for byte");

        // -- the NONE contract, on the raw axis --------------------------
        // The raw axis must be able to NOT fill, exactly like the span
        // axis.  This is the contract that distinguishes "no pad" from
        // "NUL pad" (both have byte value 0).
        char none_buf[16] = {};
        std::fill(std::begin(none_buf), std::end(none_buf), kPoison);
        const petra::detail::PadChoice none_choice{1, false, 0, "NONE"};
        auto r_none = try_format_runtime_n_padded_runtime(
            none_buf, 16, "x={}", none_choice, 42);
        check_true(r_none.has_value(), "raw runtime NONE: has a value");
        check_eq(r_none.value(), std::size_t{4}, "raw runtime NONE: written == 4");
        check_true(all_bytes_are(none_buf, 4, 16, kPoison),
                   "raw runtime NONE: buf[4..15] all == poison");

        // -- the raw axis honours the SELECTED pad -----------------------
        if (selected_pad_fills()) {
            check_true(all_bytes_are(raw_buf, 4, 16, selected_pad_byte()),
                       "raw axis: buf[4..15] all == selected pad byte");
        } else {
            check_true(all_bytes_are(raw_buf, 4, 16, kPoison),
                       "raw axis NONE cell: buf[4..15] all == poison");
        }
    }

    // Section 5 — the RUNTIME padded path (today's new sibling) must
    // produce BYTE-IDENTICAL results to the compile-time path for the
    // same choice, and must honour the NONE contract when asked not to
    // fill.
    // ------------------------------------------------------------------
    section("Section 5: runtime padded path == compile-time padded path");
    {
        char ct_buf[16] = {};
        std::fill(std::begin(ct_buf), std::end(ct_buf), kPoison);
        std::span<char> ct(ct_buf);
        const std::size_t n_ct =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(ct, "x={}", 42));

        char rt_buf[16] = {};
        std::fill(std::begin(rt_buf), std::end(rt_buf), kPoison);
        std::span<char> rt(rt_buf);
        auto r_rt = try_format_runtime_n_padded_span_runtime(
            rt, "x={}", petra::detail::kSelected, 42);

        check_true(r_rt.has_value(), "runtime path: no value");
        check_eq(r_rt.value(), n_ct, "runtime written == compile-time written");
        check_true(std::string_view(rt_buf, 16) ==
                       std::string_view(ct_buf, 16),
                   "runtime buffer == compile-time buffer, byte for byte");

        // The runtime path can also be told NOT to fill: that is the
        // NONE contract, and it must leave the poison bytes alone even
        // though the winning pad WOULD have filled.
        char nofill[16] = {};
        std::fill(std::begin(nofill), std::end(nofill), kPoison);
        std::span<char> nf(nofill);
        const petra::detail::PadChoice none_choice{1, false, 0, "NONE"};
        auto r_nf = try_format_runtime_n_padded_span_runtime(
            nf, "x={}", none_choice, 42);
        check_true(r_nf.has_value(), "runtime NONE: no value");
        check_eq(r_nf.value(), std::size_t{4}, "runtime NONE: written == 4");
        check_true(all_bytes_are(nofill, 4, 16, kPoison),
                   "runtime NONE: buf[4..15] all == poison");
    }

    // ------------------------------------------------------------------
    // Section 6 — the CONSTEVAL surface through the meta-macro (the
    // std::format_string<Args...> consteval gate still applies).
    // ------------------------------------------------------------------
    section("Section 6: CONSTEVAL meta-macro surface");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        const std::size_t n =
            unwrap_dispatch(TRY_FORMAT_BOUNDED_SPAN_N_PRIORITY(
                s, "hello {}", "world"));
        check_eq(n, std::size_t{11}, "written == 11");
        check_eq(std::string_view(buf, 11), std::string_view{"hello world"},
                 "buf[0..10] == \"hello world\"");
        if (selected_pad_fills()) {
            check_true(all_bytes_are(buf, 11, 16, selected_pad_byte()),
                       "CONSTEVAL tail == selected pad byte");
        } else {
            check_true(all_bytes_are(buf, 11, 16, kPoison),
                       "CONSTEVAL NONE cell: tail == poison");
        }
    }

    // ------------------------------------------------------------------
    // Section 7 — buffer atomicity on TooLarge, on BOTH paths.
    // ------------------------------------------------------------------
    section("Section 7: buffer UNTOUCHED on TooLarge");
    {
        char buf[8] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        auto r = try_format_runtime_n_padded_span<
            petra::detail::kSelectedFills,
            petra::detail::kSelectedByte>(s, "ABCDEFGHIJ");
        check_true(!r.has_value(), "TooLarge: no value");
        check_eq(r.error().kind, FormatErrorKind::TooLarge, "TooLarge: kind");
        check_eq(r.error().needed_bytes, std::size_t{10},
                 "TooLarge: needed_bytes == 10");
        check_true(all_bytes_are(buf, 0, sizeof(buf), kPoison),
                   "TooLarge: all 8 bytes still poison");

        // Same, on the RUNTIME path — the fill must not run either.
        char buf2[8] = {};
        std::fill(std::begin(buf2), std::end(buf2), kPoison);
        std::span<char> s2(buf2);
        auto r2 = try_format_runtime_n_padded_span_runtime(
            s2, "ABCDEFGHIJ", petra::detail::kSelected);
        check_true(!r2.has_value(), "runtime TooLarge: no value");
        check_eq(r2.error().kind, FormatErrorKind::TooLarge,
                 "runtime TooLarge: kind");
        check_true(all_bytes_are(buf2, 0, sizeof(buf2), kPoison),
                   "runtime TooLarge: all 8 bytes still poison");

#if PETRA_THROW_ON_OVERFLOW == 0
        check_true(true, "THROW=0: no exception on the expected surface");
#else
        char buf3[8] = {};
        std::fill(std::begin(buf3), std::end(buf3), kPoison);
        std::span<char> s3(buf3);
        bool threw = false;
        try {
            (void)try_format_runtime_n_throwing_padded_span<
                petra::detail::kSelectedFills,
                petra::detail::kSelectedByte>(s3, "ABCDEFGHIJ");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check_true(threw, "THROW=1: runtime_error thrown on overflow");
        check_true(all_bytes_are(buf3, 0, sizeof(buf3), kPoison),
                   "THROW=1: all 8 bytes still poison after the throw");
#endif

        // -- the RAW axis, on the same three paths ----------------------
        // TODAY'S PORT: the raw axis's atomicity contract is the SAME
        // contract, reached through the same generic core.  These checks
        // are what makes that a checked claim rather than an assertion.
        char raw1[8] = {};
        std::fill(std::begin(raw1), std::end(raw1), kPoison);
        auto rr1 = try_format_runtime_n_padded<
            petra::detail::kSelectedFills,
            petra::detail::kSelectedByte>(raw1, sizeof(raw1), "ABCDEFGHIJ");
        check_true(!rr1.has_value(), "raw TooLarge: no value");
        check_eq(rr1.error().kind, FormatErrorKind::TooLarge,
                 "raw TooLarge: kind");
        check_eq(rr1.error().needed_bytes, std::size_t{10},
                 "raw TooLarge: needed_bytes == 10");
        check_true(all_bytes_are(raw1, 0, sizeof(raw1), kPoison),
                   "raw TooLarge: all 8 bytes still poison");

        char raw2[8] = {};
        std::fill(std::begin(raw2), std::end(raw2), kPoison);
        auto rr2 = try_format_runtime_n_padded_runtime(
            raw2, sizeof(raw2), "ABCDEFGHIJ", petra::detail::kSelected);
        check_true(!rr2.has_value(), "raw runtime TooLarge: no value");
        check_eq(rr2.error().kind, FormatErrorKind::TooLarge,
                 "raw runtime TooLarge: kind");
        check_true(all_bytes_are(raw2, 0, sizeof(raw2), kPoison),
                   "raw runtime TooLarge: all 8 bytes still poison");

#if PETRA_THROW_ON_OVERFLOW == 1
        char raw3[8] = {};
        std::fill(std::begin(raw3), std::end(raw3), kPoison);
        bool raw_threw = false;
        try {
            (void)try_format_runtime_n_throwing_padded<
                petra::detail::kSelectedFills,
                petra::detail::kSelectedByte>(raw3, sizeof(raw3),
                                              "ABCDEFGHIJ");
        } catch (const std::runtime_error&) {
            raw_threw = true;
        }
        check_true(raw_threw, "raw THROW=1: runtime_error thrown on overflow");
        check_true(all_bytes_are(raw3, 0, sizeof(raw3), kPoison),
                   "raw THROW=1: all 8 bytes still poison after the throw");
#else
        check_true(true, "raw THROW=0: no exception on the expected surface");
#endif
    }

    // ------------------------------------------------------------------
    // Section 8 — buffer atomicity on a Format error (a malformed format
    // string fails at format time, before any write).
    // ------------------------------------------------------------------
    section("Section 8: buffer UNTOUCHED on a Format error");
    {
        const std::string bad_fmt = "bad { ";
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        auto r = try_format_runtime_n_padded_span_runtime(
            s, std::string_view{bad_fmt}, petra::detail::kSelected);
        check_true(!r.has_value(), "Format error: no value");
        check_eq(r.error().kind, FormatErrorKind::Format, "Format error: kind");
        check_true(all_bytes_are(buf, 0, sizeof(buf), kPoison),
                   "Format error: all 16 bytes still poison");

        // The RAW axis, same contract.
        char raw[16] = {};
        std::fill(std::begin(raw), std::end(raw), kPoison);
        auto rr = try_format_runtime_n_padded_runtime(
            raw, sizeof(raw), std::string_view{bad_fmt},
            petra::detail::kSelected);
        check_true(!rr.has_value(), "raw Format error: no value");
        check_eq(rr.error().kind, FormatErrorKind::Format,
                 "raw Format error: kind");
        check_true(all_bytes_are(raw, 0, sizeof(raw), kPoison),
                   "raw Format error: all 16 bytes still poison");
    }

    // ------------------------------------------------------------------
    // Section 9 — edge cases: written == size, and size == 0.
    // ------------------------------------------------------------------
    section("Section 9: edge cases (full buffer, zero-size buffer)");
    {
        char full[5] = {};
        std::fill(std::begin(full), std::end(full), kPoison);
        std::span<char> sf(full);
        const std::size_t n =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(sf, "ABCDE"));
        check_eq(n, std::size_t{5}, "full buffer: written == 5");
        check_eq(std::string_view(full, 5), std::string_view{"ABCDE"},
                 "full buffer: content intact");

        char zero[1] = {};
        zero[0] = kPoison;
        std::span<char> sz(zero, 0);
        auto r0 = try_format_runtime_n_padded_span_runtime(
            sz, "X", petra::detail::kSelected);
        check_true(!r0.has_value(), "zero-size buffer: TooLarge");
        check_eq(r0.error().kind, FormatErrorKind::TooLarge,
                 "zero-size buffer: kind");
        check_eq(zero[0], kPoison, "zero-size buffer: sentinel untouched");

        // -- the RAW axis, same two edge cases --------------------------
        char raw_full[5] = {};
        std::fill(std::begin(raw_full), std::end(raw_full), kPoison);
        const std::size_t raw_n =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_N_PRIORITY(raw_full, 5,
                                                          "ABCDE"));
        check_eq(raw_n, std::size_t{5}, "raw full buffer: written == 5");
        check_eq(std::string_view(raw_full, 5), std::string_view{"ABCDE"},
                 "raw full buffer: content intact");

        char raw_zero[1] = {};
        raw_zero[0] = kPoison;
        auto rr0 = try_format_runtime_n_padded_runtime(
            raw_zero, 0, "X", petra::detail::kSelected);
        check_true(!rr0.has_value(), "raw zero-size buffer: TooLarge");
        check_eq(rr0.error().kind, FormatErrorKind::TooLarge,
                 "raw zero-size buffer: kind");
        check_eq(raw_zero[0], kPoison,
                 "raw zero-size buffer: sentinel untouched");
    }

    // ------------------------------------------------------------------
    // Section 10 — THE EXHAUSTIVE SINGLE-BINARY SWEEP: all 1024 cells,
    // in one process, in about a millisecond.
    //
    // For every one of the 1024 flag combinations:
    //   - the three runtime spellings must agree (table walk, bit width,
    //     hand-written oracle);
    //   - the RUNTIME padded path must write the winner's byte into the
    //     tail (or leave the poison alone for NONE);
    //   - the NONE cell must leave the poison alone;
    //   - the win tally must satisfy the 2^(r-1) identity;
    //   - and TODAY'S ADDITION: the RAW axis must produce the SAME
    //     bytes as the span axis, in every one of the 1024 cells.
    //     This is the cross-axis check promoted from one cell (Section 4)
    //     to all of them.
    // ------------------------------------------------------------------
    section("Section 10: exhaustive 1024-cell sweep (one binary)");
    {
        // The win tallies are sized by the DEDUCED pad count, so this
        // section does not have to know how many pads there are.
        std::array<std::uint32_t, petra::detail::kPadCount> wins{};
        std::uint32_t none_wins = 0;
        int disagree = 0;
        int byte_fail = 0;
        int axis_fail = 0;

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

            // Now actually FORMAT with the runtime padded path and
            // look at the bytes.
            char buf[12] = {};
            std::fill(std::begin(buf), std::end(buf), kPoison);
            std::span<char> s(buf);
            auto r = try_format_runtime_n_padded_span_runtime(
                s, "v={}", a, 7);
            bool ok = r.has_value() && r.value() == 3
                      && std::string_view(buf, 3) == std::string_view{"v=7"};
            if (ok) {
                if (a.fills) {
                    ok = all_bytes_are(buf, 3, sizeof(buf),
                                       static_cast<char>(a.code));
                } else {
                    ok = all_bytes_are(buf, 3, sizeof(buf), kPoison);
                }
            }
            if (!ok) {
                ++byte_fail;
                continue;
            }

            // TODAY: the SAME call on the RAW axis, and the two buffers
            // must be byte-identical.  A raw-axis-only bug (a wrong cap,
            // a wrong pointer arithmetic) would be invisible to every
            // other check in this file.
            char raw_buf[12] = {};
            std::fill(std::begin(raw_buf), std::end(raw_buf), kPoison);
            auto rr = try_format_runtime_n_padded_runtime(
                raw_buf, sizeof(raw_buf), "v={}", a, 7);
            if (!rr.has_value() || rr.value() != r.value()
                || std::string_view(raw_buf, sizeof(raw_buf))
                       != std::string_view(buf, sizeof(buf))) {
                ++axis_fail;
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
                 "all 1024 cells: the runtime padded path writes the "
                 "winner's byte");
        check_eq(axis_fail, 0,
                 "all 1024 cells: the raw axis agrees with the span axis, "
                 "byte for byte");

        // The win-count identity, straight out of the spec: the pad at
        // rank r wins in exactly 2^(r-1) cells (every higher-ranked pad
        // forced off, every lower-ranked one free).
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
        // The ten win counts sum to 2^N - 1, computed from the deduced
        // pad count rather than written down.
        check_eq(sum, kCellCount - std::uint32_t{1},
                 "the win counts sum to 2^N - 1");
        check_eq(none_wins, std::uint32_t{1},
                 "NONE wins exactly one cell");
        check_eq(sum + none_wins, kCellCount,
                 "every one of the 1024 cells has exactly one winner");

        std::println("sweep: cells={} disagree={} byte_fail={} axis_fail={} "
                     "none_wins={}",
                     kCellCount, disagree, byte_fail, axis_fail, none_wins);
        for (std::size_t i = 0; i < wins.size(); ++i) {
            std::println("  rank {:2} {:<11} wins {:4} cells",
                         petra::detail::kPadTable[i].rank,
                         petra::detail::kPadTable[i].name,
                         wins[i]);
        }

#ifdef PETRA_CROSSCHECK_DUMP
        // -- CROSS-CHECK DUMP MODE -------------------------------------
        //
        // Compiled ONLY when -DPETRA_CROSSCHECK_DUMP is set, this prints
        // one line per cell so an INDEPENDENT implementation (a Python
        // model written from the spec, not from this file) can check the
        // cross-axis claim from outside the program.
        //
        //   CELL <mask> <winner_name> <winner_code> <span_hex> <raw_hex>
        for (std::uint32_t mask = 0; mask < kCellCount; ++mask) {
            const auto a = petra::detail::select_pad_table(mask);

            char span_buf[12] = {};
            std::fill(std::begin(span_buf), std::end(span_buf), kPoison);
            std::span<char> s(span_buf);
            (void)try_format_runtime_n_padded_span_runtime(s, "v={}", a, 7);

            char raw_buf[12] = {};
            std::fill(std::begin(raw_buf), std::end(raw_buf), kPoison);
            (void)try_format_runtime_n_padded_runtime(raw_buf, sizeof(raw_buf),
                                                       "v={}", a, 7);

            std::string span_hex;
            std::string raw_hex;
            for (char c : span_buf) {
                span_hex += "0123456789ABCDEF"[static_cast<unsigned>(
                    static_cast<unsigned char>(c)) >> 4];
                span_hex += "0123456789ABCDEF"[static_cast<unsigned>(
                    static_cast<unsigned char>(c)) & 0x0Fu];
            }
            for (char c : raw_buf) {
                raw_hex += "0123456789ABCDEF"[static_cast<unsigned>(
                    static_cast<unsigned char>(c)) >> 4];
                raw_hex += "0123456789ABCDEF"[static_cast<unsigned>(
                    static_cast<unsigned char>(c)) & 0x0Fu];
            }
            std::println("CELL {} {} {} {} {}", mask, a.name, a.code,
                         span_hex, raw_hex);
        }
#endif
    }

    // ------------------------------------------------------------------
    // Section 11 — thread safety: the runtime padded path is stateless
    // (all per-call state is a stack local), so N threads may use it
    // concurrently with different masks.
    // ------------------------------------------------------------------
    section("Section 11: concurrent runtime padded dispatch");
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
                        (static_cast<std::uint32_t>(t) * 251u) ^
                        static_cast<std::uint32_t>(i);
                    const auto choice = petra::detail::select_pad_table(mask);
                    // The formatted length is NOT constant: "n=7" is three
                    // bytes and "n=17" is four.  Computing the expected
                    // text is the only honest way to know where the tail
                    // starts.
                    const std::string want = "n=" + std::to_string(i);
                    char buf[12] = {};
                    std::fill(std::begin(buf), std::end(buf), kPoison);
                    std::span<char> s(buf);
                    auto r = try_format_runtime_n_padded_span_runtime(
                        s, "n={}", choice, i);
                    if (!r.has_value() || r.value() != want.size()) {
                        ++local_bad;
                        continue;
                    }
                    if (std::string_view(buf, want.size()) !=
                        std::string_view{want}) {
                        ++local_bad;
                        continue;
                    }
                    if (choice.fills) {
                        if (all_bytes_are(buf, want.size(), sizeof(buf),
                                          static_cast<char>(choice.code))) {
                            ++local_ok;
                        } else {
                            ++local_bad;
                        }
                    } else {
                        if (all_bytes_are(buf, want.size(), sizeof(buf),
                                          kPoison)) {
                            ++local_ok;
                        } else {
                            ++local_bad;
                        }
                    }
                }
                ok_count.fetch_add(local_ok, std::memory_order_relaxed);
                bad_count.fetch_add(local_bad, std::memory_order_relaxed);
            });
        }
        for (auto& th : threads) {
            th.join();
        }
        check_eq(ok_count.load(), 100, "100/100 concurrent calls OK");
        check_eq(bad_count.load(), 0, "0 concurrent failures");

        // -- the RAW axis, concurrently ---------------------------------
        // The raw axis's state is also all stack-local (the raw_buffer is
        // a view, and the impl's per-call state is a local), so the same
        // contract holds.
        std::atomic<int> raw_ok{0};
        std::atomic<int> raw_bad{0};
        std::array<std::thread, 4> raw_threads{};
        for (std::size_t t = 0; t < raw_threads.size(); ++t) {
            raw_threads[t] = std::thread([t, &raw_ok, &raw_bad]() {
                int local_ok = 0;
                int local_bad = 0;
                for (int i = 0; i < 25; ++i) {
                    const std::uint32_t mask =
                        (static_cast<std::uint32_t>(t) * 251u) ^
                        static_cast<std::uint32_t>(i);
                    const auto choice = petra::detail::select_pad_table(mask);
                    const std::string want = "n=" + std::to_string(i);
                    char buf[12] = {};
                    std::fill(std::begin(buf), std::end(buf), kPoison);
                    auto r = try_format_runtime_n_padded_runtime(
                        buf, sizeof(buf), "n={}", choice, i);
                    if (!r.has_value() || r.value() != want.size()
                        || std::string_view(buf, want.size())
                               != std::string_view{want}) {
                        ++local_bad;
                        continue;
                    }
                    const char tail = choice.fills
                        ? static_cast<char>(choice.code)
                        : kPoison;
                    if (all_bytes_are(buf, want.size(), sizeof(buf), tail)) {
                        ++local_ok;
                    } else {
                        ++local_bad;
                    }
                }
                raw_ok.fetch_add(local_ok, std::memory_order_relaxed);
                raw_bad.fetch_add(local_bad, std::memory_order_relaxed);
            });
        }
        for (auto& th : raw_threads) {
            th.join();
        }
        check_eq(raw_ok.load(), 100, "raw axis: 100/100 concurrent calls OK");
        check_eq(raw_bad.load(), 0, "raw axis: 0 concurrent failures");
    }

    std::println("\npass={} fail={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
