// P-2026-10-22 — try_format_pad_priority_dispatch:
//
// Closes Oct 21's "Where we go next" item (2) verbatim — the
// UNIFIED pad dispatcher, built as a PREPROCESSOR PRIORITY CHAIN
// instead of an exhaustive 2^10 table.
//
// The prior eleven lessons each added ONE pad character with FOUR
// hand-written sibling wrappers per axis:
//
//   NONE -> NUL -> SPACE -> DASH -> HASH -> TILDE ->
//   UNDERSCORE -> STAR -> AT -> PLUS
//
// Each layer implemented the same three-line fill loop over the
// layer below it, so the CHAR*+CAP axis plus the STD::SPAN<CHAR>
// axis accumulated ~40 near-identical wrappers.  Composition was
// only ever *described* (PLUS > AT > STAR > ... > NUL), never
// *reachable*: every lesson #error'd when a second pad flag was
// set, precisely because no dispatcher existed to pick a winner.
//
// TODAY replaces that with:
//
//   1. ONE generic pair of wrapper templates parameterized by the
//      fill byte as a non-type template argument (Part 6), so the
//      "later fill overwrites the earlier fill" rule collapses
//      from ~40 hand-written layers into two loops.
//   2. A preprocessor PRIORITY CHAIN (Part 7) that scores every
//      pad flag as (enabled ? rank + 1 : 0), takes the MAX of the
//      ten scores with a variadic MAX2 expansion, and maps the
//      winning score to a fill byte + a human-readable name.
//   3. An INDEPENDENT constexpr C++ mirror of the same chain
//      (Part 8) plus a static_assert that the preprocessor and
//      constexpr implementations AGREE on the current build cell.
//
// The headline consequence: 2^10 = 1024 pad combinations collapse
// into TWO compiled cells (expected-vs-throwing).  The rank order
// is a strict total order, so max() is the right combinator —
// no exhaustive table is needed.  Today's in-file tests pin the
// current cell; a /tmp sweep over all 1024 combinations pins the
// preprocessor-vs-constexpr agreement exhaustively (see notes).
//
// KEY FINDING #1 — a preprocessor MAX is just a right-fold of MAX2.
//   PETRA_PAD_MAX2(a, b)  = ((a) > (b) ? (a) : (b))
//   PETRA_PAD_MAX3/MAX5/MAX10 fold it by halves.  A 1024-cell
//   #if/#elif table is not merely verbose, it is *wrong-shaped*:
//   a chain of pairwise comparisons is O(10) at compile time and
//   provably total, whereas a 2^10 table is 1024 lines of
//   near-identical preprocessor trivia that a reviewer must trust
//   by eye.
//
// KEY FINDING #2 — "no fill" and "NUL fill" are DIFFERENT states.
//   PETRA_PAD=1 (NUL pad) legitimately writes '\0' bytes; NONE
//   writes nothing.  Both have score-wise-low but distinct
//   meanings, so TODAY carries a second selection output
//   (PETRA_PAD_SELECTED_FILLS) alongside the fill byte
//   (PETRA_PAD_SELECTED_CODE).  Collapsing them into one would
//   silently turn "no fill" into "fill with NUL" — a contract
//   change hidden behind a refactor.  The wrapper uses
//   `if constexpr (Fills)` so the NONE cell generates no fill loop
//   at all (zero runtime cost, not a skipped branch).
//
// KEY FINDING #3 — the preprocessor chain and the constexpr chain
//   are TWO INDEPENDENT IMPLEMENTATIONS OF THE SAME SPEC.  Pinning
//   them against each other with a single static_assert in every
//   build catches "I edited the #if chain but forgot the C++
//   chain" at compile time for the cell you happen to be building
//   — and the /tmp sweep (1024 cells) catches it for every cell.
//
// PLATFORM NOTE: the callback-based iterator primitives are used
// verbatim from Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 19 /
// Oct 21 — NOT the no-callback pattern, because std::vformat_to
// internally COPIES the output iterator on Apple Clang 21 /
// libc++ 21.  See the platform note in those lessons.
//
// Build (default — expected+NO-pad cell: PETRA_THROW_ON_OVERFLOW=0
// and all ten pad flags 0):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-10-22-try-format-pad-priority-dispatch.cpp \
//           -o /tmp/P-2026-10-22-try-format-pad-priority-dispatch
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-22-try-format-pad-priority-dispatch
//
// COMPOSITION cell (all nine pad flags ON — legal for the first
// time; PLUS wins the priority chain):
//
//   cmake -S . -B build-all-pads -DPETRA_PLUS_PAD=ON \
//         -DPETRA_AT_PAD=ON -DPETRA_STAR_PAD=ON \
//         -DPETRA_UNDERSCORE_PAD=ON -DPETRA_TILDE_PAD=ON \
//         -DPETRA_HASH_PAD=ON -DPETRA_DASH_PAD=ON \
//         -DPETRA_SPACE_PAD=ON -DPETRA_PAD=ON
//   cmake --build build-all-pads
//   ./build-all-pads/P-2026-10-22-try-format-pad-priority-dispatch
//
// Pure consumer-side; no third-party libraries.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 through Oct 21)
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

    // NOTE: NO explicit copy ctor — the compiler-generated copy ctor
    // copies cb_/data_/count_.  When vformat_to internally COPIES the
    // iterator (Apple Clang 21 / libc++ 21 behavior), the proxy's
    // operator= points to the copy, but the copy's cb_ still calls
    // back through data_ to the LOCAL in the impl.  This is the
    // CORRECT callback pattern — see Oct 11's platform note.

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (Oct 11 / Oct 13 / Oct 15 /
// Oct 16 / Oct 19 / Oct 21 verbatim — callback-based)
//
// CRITICAL: bounded_char_writer IS the output iterator.  It accepts
// a callback in its ctor so that even after vformat_to internally
// COPIES the iterator, the writes go through a `void* data` pointer
// to the local in the impl.
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
// Sep 21 verbatim).  TODAY's padded wrappers are parameterized on the fill
// byte and sit directly on these — the eight intermediate pad layers
// from Sep 27 through Oct 21 are NOT needed (that is the point of
// today's lesson).
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

}  // namespace petra

// ============================================================================
// Part 6 — NEW today: the GENERIC padded span<char> wrappers.
//
//   template <bool Fills, char Fill, typename... Args>
//   std::expected<std::size_t, FormatError>
//   try_format_runtime_n_padded_span(std::span<char>, std::string_view, Args...)
//
// `Fills` is a SEPARATE non-type parameter rather than being encoded
// in `Fill` because NUL-pad (PETRA_PAD=1, writes '\0') and NONE
// (writes nothing) are different contracts with the same byte value.
// `if constexpr (Fills)` means the NONE cell emits NO fill loop at
// all — not a runtime branch that is always false.
//
// Four instantiations cover every (Fills, Fill) pair the dispatcher
// can select, and every throw/no-throw pairing: 2 (throw axis) x
// 2 (consteval axis) x 10 (pad selections) all come from these
// four templates.
// ============================================================================

namespace petra {

// NEW today — RUNTIME + EXPECTED padded span<char> sibling.
template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span(std::span<char> out,
                                 std::string_view fmt,
                                 const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if constexpr (Fills) {
        if (r.has_value()) {
            const std::size_t written = r.value();
            std::fill(out.begin() + static_cast<std::ptrdiff_t>(written),
                      out.end(), Fill);
        }
    } else {
        (void)r;
    }
    return r;
}

// NEW today — CONSTEVAL + EXPECTED padded span<char> sibling.
template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt_str, args...);
    if constexpr (Fills) {
        if (r.has_value()) {
            const std::size_t written = r.value();
            std::fill(out.begin() + static_cast<std::ptrdiff_t>(written),
                      out.end(), Fill);
        }
    } else {
        (void)r;
    }
    return r;
}

// NEW today — RUNTIME + THROWING padded span<char> sibling.
template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    const std::size_t n = try_format_runtime_n_throwing(out, fmt, args...);
    if constexpr (Fills) {
        std::fill(out.begin() + static_cast<std::ptrdiff_t>(n),
                  out.end(), Fill);
    }
    return n;
}

// NEW today — CONSTEVAL + THROWING padded span<char> sibling.
template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_span(
    std::span<char> out, std::format_string<Args...> fmt_str,
    const Args&... args) {
    const std::size_t n =
        try_format_bounded_runtime_n_throwing_span(out, fmt_str, args...);
    if constexpr (Fills) {
        std::fill(out.begin() + static_cast<std::ptrdiff_t>(n),
                  out.end(), Fill);
    }
    return n;
}

// NEW today — petra::unwrap_dispatch: an if constexpr adapter so ONE
// call site compiles against BOTH the std::expected surface (THROW=0)
// and the bare std::size_t throwing surface (THROW=1).  Only ONE branch
// is instantiated per cell, so the discarded branch is never even type
// checked (no risk of applying has_value() to a std::size_t).
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

// BEGIN PREPROCESSOR PRIORITY CHAIN
// ============================================================================
// Part 7 — THE PREPROCESSOR PRIORITY CHAIN (NEW today).
//
//   rank: NONE 0 | NUL 1 | SPACE 2 | DASH 3 | HASH 4 | TILDE 5 |
//         UNDERSCORE 6 | STAR 7 | AT 8 | PLUS 9
//
//   score(flag) = flag ? rank + 1 : 0,   score(NONE) = 1
//
// The +1 offset keeps NONE (score 1) strictly below every ENABLED pad
// (scores 2..10) while remaining above every DISABLED pad (score 0),
// so the max is well defined for all 2^10 combinations and the NONE
// cell cannot tie with NUL-pad.
//
// PETRA_PAD_SELECTED_FILLS: 0 for NONE, 1 for every enabled pad
//                           (including NUL-pad, which writes '\0').
// PETRA_PAD_SELECTED_CODE:   the winning fill byte as an int (0 for
//                            NONE and for NUL-pad — hence FILLS).
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

// --- scores (rank + 1 when enabled, 0 when disabled) -----------------------
#define PETRA_PAD_SCORE_NONE        1
#define PETRA_PAD_SCORE_NUL         (PETRA_PAD ? 2 : 0)
#define PETRA_PAD_SCORE_SPACE       (PETRA_SPACE_PAD ? 3 : 0)
#define PETRA_PAD_SCORE_DASH        (PETRA_DASH_PAD ? 4 : 0)
#define PETRA_PAD_SCORE_HASH        (PETRA_HASH_PAD ? 5 : 0)
#define PETRA_PAD_SCORE_TILDE       (PETRA_TILDE_PAD ? 6 : 0)
#define PETRA_PAD_SCORE_UNDERSCORE  (PETRA_UNDERSCORE_PAD ? 7 : 0)
#define PETRA_PAD_SCORE_STAR        (PETRA_STAR_PAD ? 8 : 0)
#define PETRA_PAD_SCORE_AT          (PETRA_AT_PAD ? 9 : 0)
#define PETRA_PAD_SCORE_PLUS        (PETRA_PLUS_PAD ? 10 : 0)

// --- max by halves: 10 comparisons instead of a 1024-row table -----------
#define PETRA_PAD_MAX2(a, b) ((a) > (b) ? (a) : (b))
#define PETRA_PAD_MAX3(a, b, c) PETRA_PAD_MAX2(PETRA_PAD_MAX2(a, b), c)
#define PETRA_PAD_MAX5(a, b, c, d, e) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX3(a, b, c), PETRA_PAD_MAX2(d, e))
#define PETRA_PAD_MAX10(a, b, c, d, e, f, g, h, i, j) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX5(a, b, c, d, e), \
                   PETRA_PAD_MAX5(f, g, h, i, j))

#define PETRA_PAD_SELECTED_SCORE                                          \
    PETRA_PAD_MAX10(PETRA_PAD_SCORE_NONE, PETRA_PAD_SCORE_NUL,             \
                    PETRA_PAD_SCORE_SPACE, PETRA_PAD_SCORE_DASH,          \
                    PETRA_PAD_SCORE_HASH, PETRA_PAD_SCORE_TILDE,          \
                    PETRA_PAD_SCORE_UNDERSCORE, PETRA_PAD_SCORE_STAR,     \
                    PETRA_PAD_SCORE_AT, PETRA_PAD_SCORE_PLUS)

// --- winner -> FILLS / CODE / NAME ---------------------------------------
// A disabled flag scores 0 and can never equal the selected score
// (>= 1), so no false match is possible.
#define PETRA_PAD_SELECTED_FILLS                                          \
    (PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_PLUS       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_AT         ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_STAR       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_UNDERSCORE ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_TILDE      ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_HASH       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_DASH       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_SPACE      ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_NUL        ? 1 : 0)

#define PETRA_PAD_SELECTED_CODE                                            \
    (PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_PLUS       ? 43 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_AT         ? 64 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_STAR       ? 42 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_UNDERSCORE ? 95 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_TILDE      ? 126 :       \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_HASH       ? 35 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_DASH       ? 45 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_SPACE      ? 32 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_NUL        ? 0 : 0)

#define PETRA_PAD_SELECTED_NAME                                            \
    (PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_PLUS       ? "PLUS" :     \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_AT         ? "AT" :       \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_STAR       ? "STAR" :     \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_UNDERSCORE ? "UNDERSCORE" : \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_TILDE      ? "TILDE" :    \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_HASH       ? "HASH" :     \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_DASH       ? "DASH" :     \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_SPACE      ? "SPACE" :    \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_NUL        ? "NUL" :      \
                                                                     "NONE")

// --- the meta-macro dispatcher (throw axis) ------------------------------
// Both macros are parenthesized expressions (NOT do { } while (0)) so
// the caller can capture the result, and both use __VA_OPT__(,) for the
// no-args case (the Sep 13 .. Oct 21 macro shape).
#if PETRA_THROW_ON_OVERFLOW == 0
#define TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_runtime_n_padded_span<                             \
         PETRA_PAD_SELECTED_FILLS != 0,                                   \
         static_cast<char>(PETRA_PAD_SELECTED_CODE)>((out), (fmt)          \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_padded_span<                    \
         PETRA_PAD_SELECTED_FILLS != 0,                                   \
         static_cast<char>(PETRA_PAD_SELECTED_CODE)>((out), (fmt)          \
         __VA_OPT__(,) __VA_ARGS__))
#else
#define TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_runtime_n_throwing_padded_span<                   \
         PETRA_PAD_SELECTED_FILLS != 0,                                   \
         static_cast<char>(PETRA_PAD_SELECTED_CODE)>((out), (fmt)          \
         __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_throwing_padded_span<          \
         PETRA_PAD_SELECTED_FILLS != 0,                                   \
         static_cast<char>(PETRA_PAD_SELECTED_CODE)>((out), (fmt)          \
         __VA_OPT__(,) __VA_ARGS__))
#endif
// END PREPROCESSOR PRIORITY CHAIN

// ============================================================================
// Part 8 — NEW today: the INDEPENDENT constexpr C++ mirror of the same
// priority chain, plus the static_assert that pins the two together.
//
// This block is deliberately a SECOND, hand-written implementation of
// the spec in Part 7: the #if chain folds ten preprocessor scores with
// MAX2; the constexpr chain folds ten C++ scores with a variadic
// max().  Both must select the same fill byte / name / fills-flag in
// EVERY build cell, and the /tmp sweep over all 1024 pad combinations
// proves that exhaustively.
// ============================================================================

namespace petra {
namespace detail {

constexpr int pad_score(bool enabled, int rank) noexcept {
    return enabled ? rank + 1 : 0;
}

// Right-fold max: the C++ mirror of the preprocessor's PETRA_PAD_MAX10.
//
// C++ RULE WORTH REMEMBERING: a function parameter pack is only legal
// in a function TEMPLATE.  Writing `int pad_score_max(int first,
// int... rest)` as a plain function is ill-formed — clang rejects it
// with "type 'int' of function parameter pack does not contain any
// unexpanded parameter packs" (verified on Apple Clang 21 and the
// /usr/bin/g++ alias, which is clang too).  So the pack is spelled
// with a type template pack `Ints...` and folded with a comma fold
// expression.
template <typename... Ints>
constexpr int pad_score_max(int first, Ints... rest) noexcept {
    // A binary comma fold over an explicit ternary — NOT std::max, whose
    // [[nodiscard]] const& return makes every discarded fold step warn
    // under -Wunused-result (8 warnings, observed before this rewrite).
    int m = first;
    (((static_cast<int>(rest) > m) ? (m = static_cast<int>(rest)) : 0),
     ...);
    return m;
}

// Rank order (same as the preprocessor chain, independently typed).
inline constexpr int kRankNul = 1;
inline constexpr int kRankSpace = 2;
inline constexpr int kRankDash = 3;
inline constexpr int kRankHash = 4;
inline constexpr int kRankTilde = 5;
inline constexpr int kRankUnderscore = 6;
inline constexpr int kRankStar = 7;
inline constexpr int kRankAt = 8;
inline constexpr int kRankPlus = 9;

inline constexpr int kScoreNone = 1;
inline constexpr int kScoreNul = pad_score(PETRA_PAD, kRankNul);
inline constexpr int kScoreSpace = pad_score(PETRA_SPACE_PAD, kRankSpace);
inline constexpr int kScoreDash = pad_score(PETRA_DASH_PAD, kRankDash);
inline constexpr int kScoreHash = pad_score(PETRA_HASH_PAD, kRankHash);
inline constexpr int kScoreTilde = pad_score(PETRA_TILDE_PAD, kRankTilde);
inline constexpr int kScoreUnderscore =
    pad_score(PETRA_UNDERSCORE_PAD, kRankUnderscore);
inline constexpr int kScoreStar = pad_score(PETRA_STAR_PAD, kRankStar);
inline constexpr int kScoreAt = pad_score(PETRA_AT_PAD, kRankAt);
inline constexpr int kScorePlus = pad_score(PETRA_PLUS_PAD, kRankPlus);

inline constexpr int kSelectedScore =
    pad_score_max(kScoreNone, kScoreNul, kScoreSpace, kScoreDash,
                  kScoreHash, kScoreTilde, kScoreUnderscore, kScoreStar,
                  kScoreAt, kScorePlus);

inline constexpr int kSelectedFills =
    (kSelectedScore == kScorePlus)        ? 1
    : (kSelectedScore == kScoreAt)        ? 1
    : (kSelectedScore == kScoreStar)      ? 1
    : (kSelectedScore == kScoreUnderscore)? 1
    : (kSelectedScore == kScoreTilde)     ? 1
    : (kSelectedScore == kScoreHash)      ? 1
    : (kSelectedScore == kScoreDash)      ? 1
    : (kSelectedScore == kScoreSpace)     ? 1
    : (kSelectedScore == kScoreNul)       ? 1
                                          : 0;

inline constexpr int kSelectedCode =
    (kSelectedScore == kScorePlus)        ? 43
    : (kSelectedScore == kScoreAt)        ? 64
    : (kSelectedScore == kScoreStar)      ? 42
    : (kSelectedScore == kScoreUnderscore)? 95
    : (kSelectedScore == kScoreTilde)     ? 126
    : (kSelectedScore == kScoreHash)      ? 35
    : (kSelectedScore == kScoreDash)      ? 45
    : (kSelectedScore == kScoreSpace)     ? 32
    : (kSelectedScore == kScoreNul)       ? 0
                                          : 0;

inline constexpr std::string_view kSelectedName =
    (kSelectedScore == kScorePlus)        ? std::string_view{"PLUS"}
    : (kSelectedScore == kScoreAt)        ? std::string_view{"AT"}
    : (kSelectedScore == kScoreStar)      ? std::string_view{"STAR"}
    : (kSelectedScore == kScoreUnderscore)? std::string_view{"UNDERSCORE"}
    : (kSelectedScore == kScoreTilde)     ? std::string_view{"TILDE"}
    : (kSelectedScore == kScoreHash)      ? std::string_view{"HASH"}
    : (kSelectedScore == kScoreDash)      ? std::string_view{"DASH"}
    : (kSelectedScore == kScoreSpace)     ? std::string_view{"SPACE"}
    : (kSelectedScore == kScoreNul)       ? std::string_view{"NUL"}
                                          : std::string_view{"NONE"};

}  // namespace detail

// Public accessors so a test (or a caller) can ask "which pad won?"
// without re-deriving the chain.
[[nodiscard]] constexpr char selected_pad_byte() noexcept {
    return static_cast<char>(detail::kSelectedCode);
}
[[nodiscard]] constexpr bool selected_pad_fills() noexcept {
    return detail::kSelectedFills != 0;
}
[[nodiscard]] constexpr std::string_view selected_pad_name() noexcept {
    return detail::kSelectedName;
}

// KEY FINDING #3 — the two independent implementations must agree.
static_assert(detail::kSelectedFills == PETRA_PAD_SELECTED_FILLS,
              "preprocessor PETRA_PAD_SELECTED_FILLS must match the "
              "constexpr chain");
static_assert(detail::kSelectedCode == PETRA_PAD_SELECTED_CODE,
              "preprocessor PETRA_PAD_SELECTED_CODE must match the "
              "constexpr chain");
static_assert(detail::kSelectedName == PETRA_PAD_SELECTED_NAME,
              "preprocessor PETRA_PAD_SELECTED_NAME must match the "
              "constexpr chain");

}  // namespace petra

// ============================================================================
// Part 9 — hand-rolled SECTION/CHECK framework (verbatim shape from
// Sep 6 .. Oct 21).
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
            g_section, what, got, static_cast<unsigned>(static_cast<unsigned char>(got)),
            expected, static_cast<unsigned>(static_cast<unsigned char>(expected)));
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
// Part 10 — sections.
//
// Every section is written against the SELECTED pad (whatever the
// preprocessor chain picked for this build cell), so the SAME source
// is the test for all 1024 pad combinations:
//   - selected_pad_fills() == false -> bytes [written, size) stay
//     poison 'Q' (the NONE contract).
//   - selected_pad_fills() == true  -> bytes [written, size) are all
//     selected_pad_byte() (including '\0' for the NUL cell).
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

}  // namespace

int main() {
    using namespace petra_test;
    using namespace petra;

    std::println("selected pad = {}  (fills={}, byte={} (0x{:02X}), score={})",
                 selected_pad_name(),
                 selected_pad_fills() ? 1 : 0,
                 static_cast<int>(static_cast<unsigned char>(selected_pad_byte())),
                 static_cast<unsigned>(static_cast<unsigned char>(selected_pad_byte())),
                 petra::detail::kSelectedScore);

    // ------------------------------------------------------------------
    // Section 1 — the rank table itself: every pad's rank, score when
    // enabled, and ASCII code is pinned by static_assert, and the
    // strict total order rank(PLUS) > rank(AT) > ... > rank(NUL) is
    // asserted pairwise.
    // ------------------------------------------------------------------
    section("Section 1: rank table + strict total order");
    {
        static_assert(petra::detail::kRankNul == 1, "NUL rank");
        static_assert(petra::detail::kRankSpace == 2, "SPACE rank");
        static_assert(petra::detail::kRankDash == 3, "DASH rank");
        static_assert(petra::detail::kRankHash == 4, "HASH rank");
        static_assert(petra::detail::kRankTilde == 5, "TILDE rank");
        static_assert(petra::detail::kRankUnderscore == 6, "UNDERSCORE rank");
        static_assert(petra::detail::kRankStar == 7, "STAR rank");
        static_assert(petra::detail::kRankAt == 8, "AT rank");
        static_assert(petra::detail::kRankPlus == 9, "PLUS rank");

        static_assert(petra::detail::kRankPlus > petra::detail::kRankAt, "");
        static_assert(petra::detail::kRankAt > petra::detail::kRankStar, "");
        static_assert(petra::detail::kRankStar > petra::detail::kRankUnderscore, "");
        static_assert(petra::detail::kRankUnderscore > petra::detail::kRankTilde, "");
        static_assert(petra::detail::kRankTilde > petra::detail::kRankHash, "");
        static_assert(petra::detail::kRankHash > petra::detail::kRankDash, "");
        static_assert(petra::detail::kRankDash > petra::detail::kRankSpace, "");
        static_assert(petra::detail::kRankSpace > petra::detail::kRankNul, "");

        // ASCII codes of the nine pad bytes (Oct 21's adjacency fact
        // is preserved: '+' == '*' + 1).
        static_assert('\0' == 0, "NUL");
        static_assert(' ' == 32, "SPACE");
        static_assert('#' == 35, "HASH");
        static_assert('-' == 45, "DASH");
        static_assert('*' == 42, "STAR");
        static_assert('+' == 43, "PLUS");
        static_assert('+' == '*' + 1, "'+' is adjacent to '*'");
        static_assert('@' == 64, "AT");
        static_assert('_' == 95, "UNDERSCORE");
        static_assert('~' == 126, "TILDE");
        static_assert(kPoison == 'Q', "poison byte");
        static_assert(kPoison != ' ' && kPoison != '#' && kPoison != '-'
                          && kPoison != '*' && kPoison != '+' && kPoison != '@'
                          && kPoison != '_' && kPoison != '~',
                      "poison byte must differ from all nine pad bytes");
        check_true(true, "rank table + ASCII codes verified at compile time");
    }

    // ------------------------------------------------------------------
    // Section 2 — the priority chain's own invariants for THIS cell:
    // the selected score is >= every enabled pad's score, the NONE
    // score never ties an enabled pad, and name/code/fills agree.
    // ------------------------------------------------------------------
    section("Section 2: priority-chain invariants for this cell");
    {
        const int sel = petra::detail::kSelectedScore;
        check_true(sel >= 1, "selected score >= 1 (NONE floor)");
        check_true(sel == PETRA_PAD_SELECTED_SCORE,
                   "preprocessor score == constexpr score");
        check_true((petra::detail::kSelectedFills != 0)
                       == (PETRA_PAD_SELECTED_FILLS != 0),
                   "FILLS agrees");
        check_true(petra::detail::kSelectedCode == PETRA_PAD_SELECTED_CODE,
                   "CODE agrees");
        check_true(petra::detail::kSelectedName == PETRA_PAD_SELECTED_NAME,
                   "NAME agrees");

        // NONE can only be selected when NO pad flag is set.
        const bool any_pad = (PETRA_PAD || PETRA_SPACE_PAD || PETRA_DASH_PAD
                              || PETRA_HASH_PAD || PETRA_TILDE_PAD
                              || PETRA_UNDERSCORE_PAD || PETRA_STAR_PAD
                              || PETRA_AT_PAD || PETRA_PLUS_PAD) != 0;
        if (any_pad) {
            check_true(sel != petra::detail::kScoreNone,
                       "a pad is enabled -> NONE must NOT win");
            check_true(selected_pad_fills(), "a pad is enabled -> FILLS=1");
            check_true(selected_pad_name() != std::string_view{"NONE"},
                       "a pad is enabled -> NAME != NONE");
        } else {
            check_true(sel == petra::detail::kScoreNone,
                       "no pad enabled -> NONE wins");
            check_true(!selected_pad_fills(),
                       "no pad enabled -> FILLS=0 (NO fill loop)");
            check_true(selected_pad_name() == std::string_view{"NONE"},
                       "no pad enabled -> NAME == NONE");
            check_true(selected_pad_byte() == '\0',
                       "no pad enabled -> CODE == 0");
        }

        // The winner is the highest-RANKED enabled flag: recompute the
        // expected name here with a THIRD, plainly-written chain.
        std::string_view want = "NONE";
        if (PETRA_PLUS_PAD)        { want = "PLUS"; }
        else if (PETRA_AT_PAD)      { want = "AT"; }
        else if (PETRA_STAR_PAD)    { want = "STAR"; }
        else if (PETRA_UNDERSCORE_PAD) { want = "UNDERSCORE"; }
        else if (PETRA_TILDE_PAD)   { want = "TILDE"; }
        else if (PETRA_HASH_PAD)    { want = "HASH"; }
        else if (PETRA_DASH_PAD)    { want = "DASH"; }
        else if (PETRA_SPACE_PAD)   { want = "SPACE"; }
        else if (PETRA_PAD)         { want = "NUL"; }
        check_true(selected_pad_name() == want,
                   "third chain agrees on the winning pad");
    }

    // ------------------------------------------------------------------
    // Section 3 — success path through the meta-macro: bytes after
    // `written` follow the SELECTED contract.
    // ------------------------------------------------------------------
    section("Section 3: meta-macro success path honors the selected pad");
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
    // Section 4 — the CONSTEVAL surface through the meta-macro (the
    // std::format_string<Args...> consteval gate still applies).
    // ------------------------------------------------------------------
    section("Section 4: CONSTEVAL meta-macro surface");
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
    // Section 5 — buffer atomicity on TooLarge: the fill loop must not
    // run on the error path (Fills is a compile-time bool, but the
    // `r.has_value()` guard is still what gates it).
    // ------------------------------------------------------------------
    section("Section 5: buffer UNTOUCHED on TooLarge");
    {
        char buf[8] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        auto r = try_format_runtime_n_padded_span<
            PETRA_PAD_SELECTED_FILLS != 0,
            static_cast<char>(PETRA_PAD_SELECTED_CODE)>(s, "ABCDEFGHIJ");
        check_true(!r.has_value(), "TooLarge: no value");
        check_eq(r.error().kind, FormatErrorKind::TooLarge, "TooLarge: kind");
        check_eq(r.error().needed_bytes, std::size_t{10},
                 "TooLarge: needed_bytes == 10");
        check_true(all_bytes_are(buf, 0, sizeof(buf), kPoison),
                   "TooLarge: all 8 bytes still poison");

#if PETRA_THROW_ON_OVERFLOW == 0
        check_true(true, "THROW=0: no exception on the expected surface");
#else
        char buf2[8] = {};
        std::fill(std::begin(buf2), std::end(buf2), kPoison);
        std::span<char> s2(buf2);
        bool threw = false;
        try {
            (void)try_format_runtime_n_throwing_padded_span<
                PETRA_PAD_SELECTED_FILLS != 0,
                static_cast<char>(PETRA_PAD_SELECTED_CODE)>(s2, "ABCDEFGHIJ");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check_true(threw, "THROW=1: TooLarge throws std::runtime_error");
        check_true(all_bytes_are(buf2, 0, sizeof(buf2), kPoison),
                   "THROW=1: buffer still poison after the throw");
#endif
    }

    // ------------------------------------------------------------------
    // Section 6 — buffer atomicity on Format error (malformed fmt) and
    // on sub-span no-bleed.
    // ------------------------------------------------------------------
    section("Section 6: Format error atomicity + sub-span no-bleed");
    {
        char buf[16] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        const std::string bad_fmt = "bad { ";
        try {
            (void)unwrap_dispatch(
                try_format_runtime_n_padded_span<
                    PETRA_PAD_SELECTED_FILLS != 0,
                    static_cast<char>(PETRA_PAD_SELECTED_CODE)>(s, bad_fmt));
            check_true(false, "Format error: expected a failure");
        } catch (const std::runtime_error&) {
            check_true(true, "Format error surfaced as runtime_error "
                             "(THROW=1) / expected (THROW=0 unwrap)");
        } catch (...) {
            check_true(false, "Format error: unexpected exception type");
        }
        check_true(all_bytes_are(buf, 0, sizeof(buf), kPoison),
                   "Format error: all 16 bytes still poison");

        // Sub-span no-bleed: 6-byte sub-span at offset 5 of a 16-byte
        // buffer, format "ABCDE" (5 bytes).
        char buf2[16] = {};
        std::fill(std::begin(buf2), std::end(buf2), kPoison);
        std::span<char> sub(buf2 + 5, 6);
        const std::size_t n2 =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(sub, "ABCDE"));
        check_eq(n2, std::size_t{5}, "sub-span: written == 5");
        check_eq(std::string_view(buf2 + 5, 5), std::string_view{"ABCDE"},
                 "sub-span: content");
        check_true(all_bytes_are(buf2, 0, 5, kPoison),
                   "sub-span: prefix [0..4] untouched");
        check_true(all_bytes_are(buf2, 11, 16, kPoison),
                   "sub-span: suffix [11..15] untouched");
        if (selected_pad_fills()) {
            check_eq(buf2[10], selected_pad_byte(),
                     "sub-span: in-span tail byte == selected pad");
        } else {
            check_eq(buf2[10], kPoison, "sub-span: in-span tail == poison");
        }
    }

    // ------------------------------------------------------------------
    // Section 7 — edge cases: written == span.size() (zero fill bytes)
    // and span.size() == 0 (TooLarge, sentinel untouched).
    // ------------------------------------------------------------------
    section("Section 7: written==size and size==0 edge cases");
    {
        char buf[5] = {};
        std::fill(std::begin(buf), std::end(buf), kPoison);
        std::span<char> s(buf);
        const std::size_t n =
            unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(s, "ABCDE"));
        check_eq(n, std::size_t{5}, "edge: written == span.size()");
        check_eq(std::string_view(buf, 5), std::string_view{"ABCDE"},
                 "edge: full-buffer content preserved (no fill bytes)");

        char zbuf[1] = {kPoison};
        std::span<char> zs(zbuf, 0);
        bool empty_failed = false;
        try {
            (void)unwrap_dispatch(
                TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(zs, "X"));
        } catch (const std::runtime_error&) {
            empty_failed = true;
        }
        check_true(empty_failed, "edge: span.size()==0 -> failure surfaced");
        check_eq(zbuf[0], kPoison, "edge: sentinel byte untouched");
    }

    // ------------------------------------------------------------------
    // Section 8 — the NUL-pad cell is the one that proves FILLS and
    // CODE are genuinely independent outputs: CODE==0 for BOTH NONE
    // and NUL, but FILLS differs.  Compile-time dispatch, no runtime
    // branch.
    // ------------------------------------------------------------------
    section("Section 8: NONE vs NUL are distinct contracts");
    {
        const bool is_none = (selected_pad_name() == std::string_view{"NONE"});
        const bool is_nul = (selected_pad_name() == std::string_view{"NUL"});
        check_true(!(is_none && is_nul), "NAME cannot be both NONE and NUL");
        if (is_nul) {
            check_true(selected_pad_byte() == '\0', "NUL cell: byte is '\\0'");
            check_true(selected_pad_fills(), "NUL cell: FILLS == 1");
            char buf[8] = {};
            std::fill(std::begin(buf), std::end(buf), kPoison);
            std::span<char> s(buf);
            const std::size_t n =
                unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(s, "AB"));
            check_eq(n, std::size_t{2}, "NUL cell: written == 2");
            check_true(all_bytes_are(buf, 2, 8, '\0'),
                       "NUL cell: tail bytes really are '\\0' (not poison)");
        } else if (is_none) {
            check_true(selected_pad_byte() == '\0', "NONE cell: byte is '\\0'");
            check_true(!selected_pad_fills(), "NONE cell: FILLS == 0");
        } else {
            check_true(selected_pad_fills(), "other cells: FILLS == 1");
            check_true(selected_pad_byte() != '\0',
                       "other cells: CODE != 0");
        }
    }

    // ------------------------------------------------------------------
    // Section 9 — thread safety: 4 threads x 25 calls, each on its own
    // buffer, tallying the selected contract.  100/100 expected.
    // ------------------------------------------------------------------
    section("Section 9: concurrent dispatch — 4 threads x 25 calls");
    {
        constexpr int kThreads = 4;
        constexpr int kCallsPerThread = 25;
        char buf[kThreads][16] = {};
        for (int t = 0; t < kThreads; ++t) {
            std::fill(std::begin(buf[t]), std::end(buf[t]), kPoison);
        }
        std::atomic<int> total_pass{0};
        std::atomic<int> total_fail{0};
        std::array<std::thread, kThreads> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads[static_cast<std::size_t>(t)] = std::thread([&, t]() {
                int local_pass = 0;
                int local_fail = 0;
                for (int i = 0; i < kCallsPerThread; ++i) {
                    std::span<char> s(buf[t]);
                    const std::size_t n = unwrap_dispatch(
                        TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(s, "t={},i={}",
                                                           t, i));
                    const std::size_t want_len =
                        std::string{"t="}.size() + std::to_string(t).size()
                        + std::string{",i="}.size() + std::to_string(i).size();
                    if (n != want_len) {
                        ++local_fail;
                        continue;
                    }
                    const char tail =
                        selected_pad_fills() ? selected_pad_byte() : kPoison;
                    if (all_bytes_are(buf[t], n, 16, tail)) {
                        ++local_pass;
                    } else {
                        ++local_fail;
                    }
                }
                total_pass.fetch_add(local_pass);
                total_fail.fetch_add(local_fail);
            });
        }
        for (auto& th : threads) {
            th.join();
        }
        check_eq(total_pass.load(), kThreads * kCallsPerThread,
                 "concurrent: all calls honor the selected contract");
        check_eq(total_fail.load(), 0, "concurrent: zero failures");
    }

    // ------------------------------------------------------------------
    // Section 10 — Summary.
    // ------------------------------------------------------------------
    section("Summary");
    std::println("  selected pad = {}", selected_pad_name());
    std::println("  selected byte = {}  fills = {}",
                 static_cast<int>(
                     static_cast<unsigned char>(selected_pad_byte())),
                 selected_pad_fills() ? "yes" : "no");
    std::println("  PASS={}  FAIL={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}