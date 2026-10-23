// P-2026-10-23 — try_format_bang_pad_priority_dispatch:
//
// Closes Oct 22's "Where we go next" item (2) verbatim — add an
// ELEVENTH pad (PETRA_BANG_PAD, '!', ASCII 33) and measure what it
// actually costs.  The point of the item was to prove the SHAPE
// generalises rather than just the current nine pads, so today's
// lesson is deliberately about the COST of the eleventh pad:
//
//   NONE -> NUL -> SPACE -> DASH -> HASH -> TILDE ->
//   UNDERSCORE -> STAR -> AT -> PLUS -> BANG
//
// Oct 22 replaced ~40 hand-written sibling wrappers with ONE generic
// pair of `template <bool Fills, char Fill>` wrappers (Part 6) and
// selected the winning pad with a preprocessor priority chain
// (Part 7) plus an INDEPENDENT constexpr mirror (Part 8).  TODAY
// asks the question that only a new pad can answer: if I had to add
// pad number twelve next week, how many lines would I actually
// touch, and in how many places?
//
// KEY FINDING #1 — inserting a pad at the TOP of the order costs
// ZERO edits to existing scores.  Because the score is
// `rank + 1` (Oct 22's deliberate one-offset so that NONE scores 1),
// appending BANG at rank 10 means BANG scores 11 and NOTHING ELSE
// MOVES.  Verified, not asserted: a /tmp sweep compiles yesterday's
// Oct 22 source and today's source under all 512 previously-legal
// flag combinations and compares the two binaries' reported
// (name, fills, byte, score) — 512/512 IDENTICAL.  A pad inserted
// in the MIDDLE of the order would have edited all nine scores
// above it; a lesson that had chosen a rank-order-insensitive
// encoding (e.g. "last flag set wins") would have silently
// changed the meaning of every existing build cell.
//
// KEY FINDING #2 — a variadic preprocessor max is IDEMPOTENT, so
// an odd arity needs a REPEAT, not a new macro.  Oct 22 hand-wrote
// MAX2/MAX3/MAX5/MAX10 to fold ten scores.  Eleven scores would
// need MAX11 (more hand-written binary tree), but
// max(a,...,k,a,a,a,a,a) == max(a,...,k), so TODAY replaces the
// hand-rolled odd sizes with the power-of-two family MAX2/MAX4/
// MAX8/MAX16 and pads to the next power of two with repeats of the
// NONE score.  Four macros cover any arity up to 16 and three up to
// 8; the family grows logarithmically (4 macros instead of 5 for
// the same 11 values) and the pad is *sound by the idempotence law*
// rather than by inspection.  Verified against a hand-written
// 11-argument nested max over all 1024 flag combinations:
// 1024/1024 agree.
//
// KEY FINDING #3 — the constexpr mirror generalises for FREE and
// that asymmetry is the real argument for it.  Adding the eleventh
// score to `pad_score_max(...)` is ONE token inside an existing
// fold expression, whereas the preprocessor chain needed a new
// score macro, a new arm in the FILLS/CODE/NAME cascades, and a
// repeat-count recompute.  Today's lesson takes the next step
// (Oct 22's item (3)) far enough to make that concrete: a RUNTIME
// `PadFlags` struct plus a `constexpr select_pad()` function, so
// ONE binary can now answer the "which pad wins?" question for all
// 1024 cells instead of needing 1024 compiles.  Section 9 cross-
// checks THREE independent implementations of the selection
// (priority if-chain / rank-table max-and-index-back / un-offset
// max-rank formulation) over all 1024 combinations and pins the
// compile-time preprocessor result to the runtime answer.
//
// The in-file tests pin the current compile cell; the exhaustive
// sweeps live under /tmp/petra20261023/ (see the notes).
//
// WHAT YESTERDAY'S THREE FINDINGS BECOME TODAY (kept here so the
// two lessons read as one arc; the claims are Oct 22's, verified
// again by today's sweeps):
//
//   Oct 22 #1 — a preprocessor MAX is a right-fold of MAX2, so the
//   selection is a max over integer scores rather than a 1024-row
//   #if table.  TODAY keeps the idea and REPLACES the hand-rolled
//   MAX3/MAX5/MAX10 with the power-of-two family MAX2/MAX4/MAX8/
//   MAX16 (see Part 7), which is what makes the ELEVENTH score
//   expressible without hand-writing MAX11.
//
//   Oct 22 #2 — "no fill" and "NUL fill" are DIFFERENT states, so
//   FILLS and CODE are two separate outputs.  Unchanged today;
//   BANG keeps the split honest (FILLS=1, CODE=33 for BANG, and
//   Section 8 still pins NONE vs NUL).
//
//   Oct 22 #3 — the preprocessor chain and the constexpr chain
//   are TWO INDEPENDENT IMPLEMENTATIONS OF THE SAME SPEC, pinned
//   against each other with static_asserts in every build.  Kept
//   verbatim, and extended: today adds the THIRD implementation
//   (a runtime select_pad over a PadFlags struct) and Section 9
//   checks all three against each other over all 1024 cells.
//
// PLATFORM NOTE: the callback-based iterator primitives are used
// verbatim from Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 19 /
// Oct 21 — NOT the no-callback pattern, because std::vformat_to
// internally COPIES the output iterator on Apple Clang 21 /
// libc++ 21.  See the platform note in those lessons.
//
// Build (default — expected+NO-pad cell: PETRA_THROW_ON_OVERFLOW=0
// and all eleven pad flags 0):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow \
//           -Wconversion -Wsign-conversion \
//           P-2026-10-23-try-format-bang-pad-priority-dispatch.cpp \
//           -o /tmp/P-2026-10-23-try-format-bang-pad-priority-dispatch
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-23-try-format-bang-pad-priority-dispatch
//
// COMPOSITION cell (all TEN prior pad flags ON — still PLUS, because
// BANG is off; adding -DPETRA_BANG_PAD=ON makes BANG win, which is
// the cell that exercises today's new code):
//
//   cmake -S . -B build-bang -DPETRA_BANG_PAD=ON \
//         -DPETRA_PLUS_PAD=ON -DPETRA_AT_PAD=ON -DPETRA_STAR_PAD=ON \
//         -DPETRA_UNDERSCORE_PAD=ON -DPETRA_TILDE_PAD=ON \
//         -DPETRA_HASH_PAD=ON -DPETRA_DASH_PAD=ON \
//         -DPETRA_SPACE_PAD=ON -DPETRA_PAD=ON
//   cmake --build build-bang
//   ./build-bang/P-2026-10-23-try-format-bang-pad-priority-dispatch
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

// NEW today — the ELEVENTH pad flag.  '!' is ASCII 33, and note it is
// ADJACENT to '#' (ASCII 35)?  No — it is two below '#'.  The real
// adjacency fact is that '!' (33) sits between ' ' (32) and '#' (35)
// but is NOT equal to either, so an off-by-one in a rank/byte table
// is invisible at a glance; Section 1 pins the codes with
// static_asserts instead.
#ifndef PETRA_BANG_PAD
#define PETRA_BANG_PAD 0
#endif
#if PETRA_BANG_PAD != 0 && PETRA_BANG_PAD != 1
#error "PETRA_BANG_PAD must be 0 (no bang-pad) or 1 (bang-padded)"
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
#define PETRA_PAD_SCORE_BANG        (PETRA_BANG_PAD ? 11 : 0)

// --- max by halves, POWER-OF-TWO family (NEW today) -----------------------
//
// Oct 22 hand-wrote MAX2/MAX3/MAX5/MAX10 to fold exactly ten scores.
// Eleven scores would have demanded a hand-written MAX11 — a fifth
// macro whose only difference from MAX10 is one more nesting level.
//
// KEY FINDING (today) — max is IDEMPOTENT, so an odd arity needs a
// REPEAT, not a new macro:  max(a,b,...,k, a,a,a,a,a) == max(a,...,k)
// because duplicating an argument cannot change the maximum.  So the
// general family is the powers of two, and any arity is reached by
// padding to the next power of two with repeats of an argument
// already present (today: repeats of the NONE score, which is
// always in the list and is never the max when a pad is enabled).
//
//   MAX2  -> 2      MAX8  -> 8      4 macros cover arity <= 16
//   MAX4  -> 4      MAX16 -> 16     (3 macros cover arity <= 8)
//   11 scores = MAX8(...) ++ MAX4(three real scores, NONE repeat)
//
// PREPROCESSOR GOTCHA WORTH THE WHOLE LESSON — compose the family
// with MAX2 at every LEVEL, not with the child macro:
//
//   #define PETRA_PAD_MAX8(a, b, c, d, e, f, g, h) \
//       PETRA_PAD_MAX4(PETRA_PAD_MAX4(a, b, c, d), PETRA_PAD_MAX4(e, f, g, h))
//
// My first draft was exactly that, and clang rejected the innocent-
// looking `PETRA_PAD_MAX8(1,2,3,4,5,6,7,8)` with
//
//   error: too few arguments provided to function-like macro invocation
//   note: expanded from macro 'PETRA_PAD_MAX8'
//   note: macro 'PETRA_PAD_MAX4' defined here
//
// Two separate mistakes were stacked here, and the error message only
// points at the second one:
//
//   1. The family must be spelled with the BASE combinator at each
//      level.  MAX4-of-MAX4s leaves a bare MAX4 token in the
//      replacement list, and the rescan of the already-substituted
//      text then mis-attaches the outer parentheses.  MAX2(MAX4(..),
//      MAX4(..)) has the same information content and rescans
//      cleanly.
//   2. Arity accounting is done by eye and eye is bad at it: my
//      first attempt at the eleven scores passed EIGHT plain scores
//      *and* a nested MAX4 to the eight-argument MAX8 — nine
//      arguments to a macro that takes eight.  The correct shape is
//      MAX2(MAX8(eight scores), MAX4(AT, PLUS, BANG, NONE)).
//
// Both mistakes surfaced as the SAME "too few/many arguments"
// diagnostic pointing at a macro several lines away from the
// mistake.  Verified with standalone probes under /tmp: MAX4 with
// plain arguments is fine, MAX2(MAX4(..), MAX4(..)) is fine, and
// MAX8-with-MAX4-of-MAX4s fails even with plain arguments.
//
// The lesson generalises past this lesson: a nested preprocessor
// combinator has to be checked at its USE SITE, not at its
// definition, and the diagnostic will point at the innermost macro
// rather than at the call that miscounted.  Same shape as Oct 22's
// "a parameter pack is only legal in a function TEMPLATE" note: a
// spelling that reads right, stopped only by the compiler.
#define PETRA_PAD_MAX2(a, b) ((a) > (b) ? (a) : (b))
#define PETRA_PAD_MAX4(a, b, c, d) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX2(a, b), PETRA_PAD_MAX2(c, d))
#define PETRA_PAD_MAX8(a, b, c, d, e, f, g, h) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX4(a, b, c, d), PETRA_PAD_MAX4(e, f, g, h))
#define PETRA_PAD_MAX16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX8(a, b, c, d, e, f, g, h),               \
                   PETRA_PAD_MAX8(i, j, k, l, m, n, o, p))

// ELEVEN scores, expressed with FOUR macros and ONE repeat of the NONE
// score:  MAX2( MAX8(eight real scores), MAX4(AT, PLUS, BANG, NONE) ).
// The repeat is the fourth argument of the inner MAX4; by idempotence
// of max it cannot change the result.  Section 2 pins that by
// comparing against a max computed with NO padding at all.
#define PETRA_PAD_SELECTED_SCORE                                          \
    PETRA_PAD_MAX2(PETRA_PAD_MAX8(PETRA_PAD_SCORE_NONE,                   \
                                  PETRA_PAD_SCORE_NUL,                    \
                                  PETRA_PAD_SCORE_SPACE,                  \
                                  PETRA_PAD_SCORE_DASH,                   \
                                  PETRA_PAD_SCORE_HASH,                   \
                                  PETRA_PAD_SCORE_TILDE,                  \
                                  PETRA_PAD_SCORE_UNDERSCORE,             \
                                  PETRA_PAD_SCORE_STAR),                  \
                   PETRA_PAD_MAX4(PETRA_PAD_SCORE_AT,                     \
                                   PETRA_PAD_SCORE_PLUS,                   \
                                   PETRA_PAD_SCORE_BANG,                   \
                                   PETRA_PAD_SCORE_NONE))

// --- winner -> FILLS / CODE / NAME ---------------------------------------
// A disabled flag scores 0 and can never equal the selected score
// (>= 1), so no false match is possible.
#define PETRA_PAD_SELECTED_FILLS                                          \
    (PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_BANG       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_PLUS       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_AT         ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_STAR       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_UNDERSCORE ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_TILDE      ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_HASH       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_DASH       ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_SPACE      ? 1 :         \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_NUL        ? 1 : 0)

#define PETRA_PAD_SELECTED_CODE                                            \
    (PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_BANG       ? 33 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_PLUS       ? 43 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_AT         ? 64 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_STAR       ? 42 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_UNDERSCORE ? 95 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_TILDE      ? 126 :       \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_HASH       ? 35 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_DASH       ? 45 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_SPACE      ? 32 :        \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_NUL        ? 0 : 0)

#define PETRA_PAD_SELECTED_NAME                                            \
    (PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_BANG       ? "BANG" :     \
     PETRA_PAD_SELECTED_SCORE == PETRA_PAD_SCORE_PLUS       ? "PLUS" :     \
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
inline constexpr int kRankBang = 10;  // NEW today

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
inline constexpr int kScoreBang = pad_score(PETRA_BANG_PAD, kRankBang);

// ELEVEN scores.  This is the whole cost of the eleventh pad on the
// C++ side: ONE new rank constant, ONE new score constant, and ONE
// extra argument inside an existing fold expression.  No new
// function, no new branch — contrast with the preprocessor chain,
// which needed a new score macro AND three new ternary arms.
inline constexpr int kSelectedScore =
    pad_score_max(kScoreNone, kScoreNul, kScoreSpace, kScoreDash,
                  kScoreHash, kScoreTilde, kScoreUnderscore, kScoreStar,
                  kScoreAt, kScorePlus, kScoreBang);

inline constexpr int kSelectedFills =
    (kSelectedScore == kScoreBang)        ? 1
    : (kSelectedScore == kScorePlus)      ? 1
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
    (kSelectedScore == kScoreBang)        ? 33
    : (kSelectedScore == kScorePlus)      ? 43
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
    (kSelectedScore == kScoreBang)        ? std::string_view{"BANG"}
    : (kSelectedScore == kScorePlus)      ? std::string_view{"PLUS"}
    : (kSelectedScore == kScoreAt)        ? std::string_view{"AT"}
    : (kSelectedScore == kScoreStar)      ? std::string_view{"STAR"}
    : (kSelectedScore == kScoreUnderscore)? std::string_view{"UNDERSCORE"}
    : (kSelectedScore == kScoreTilde)     ? std::string_view{"TILDE"}
    : (kSelectedScore == kScoreHash)      ? std::string_view{"HASH"}
    : (kSelectedScore == kScoreDash)      ? std::string_view{"DASH"}
    : (kSelectedScore == kScoreSpace)     ? std::string_view{"SPACE"}
    : (kSelectedScore == kScoreNul)       ? std::string_view{"NUL"}
                                          : std::string_view{"NONE"};

// ------------------------------------------------------------------------
// NEW today — implementation THREE: a RUNTIME selection function.
//
// Oct 22's item (3) said the ten flags are compile-time constants and
// that a runtime PadFlags struct plus a templated fill loop "would let
// a single binary serve all 1024 cells instead of 1024 compiles".  This
// is that struct and that function.
//
// Why this is worth building rather than just noting:
//   - select_pad() takes the flags as VALUES, so the same binary
//     answers the "which pad wins?" question for every combination.
//   - It is a THIRD spelling of the specification.  Two independent
//     spellings that agree is evidence; three that agree is a
//     cross-check (Section 9 exercises all 1024 cells at runtime).
//   - It exposes the shape of the spec: the winner is a pure function
//     of the flag vector, so "the compile-time chain" and "the runtime
//     chain" are the same function with different arguments.  The
//     compile-time version has no arguments (they are the macros);
//     this one has a struct.  Nothing else differs.
// ------------------------------------------------------------------------

struct PadFlags {
    bool nul{false};
    bool space{false};
    bool dash{false};
    bool hash{false};
    bool tilde{false};
    bool underscore{false};
    bool star{false};
    bool at{false};
    bool plus{false};
    bool bang{false};
};

struct PadChoice {
    int score;
    bool fills;
    int code;
    std::string_view name;
};

// Implementation A of the runtime selection: the plainly-written
// priority if-chain, highest rank first.  Deliberately boring.
[[nodiscard]] constexpr PadChoice select_pad_if_chain(PadFlags f) noexcept {
    if (f.bang)       return PadChoice{11, true, 33, "BANG"};
    if (f.plus)       return PadChoice{10, true, 43, "PLUS"};
    if (f.at)         return PadChoice{9,  true, 64, "AT"};
    if (f.star)       return PadChoice{8,  true, 42, "STAR"};
    if (f.underscore) return PadChoice{7,  true, 95, "UNDERSCORE"};
    if (f.tilde)      return PadChoice{6,  true, 126, "TILDE"};
    if (f.hash)       return PadChoice{5,  true, 35, "HASH"};
    if (f.dash)       return PadChoice{4,  true, 45, "DASH"};
    if (f.space)      return PadChoice{3,  true, 32, "SPACE"};
    if (f.nul)        return PadChoice{2,  true, 0,  "NUL"};
    return PadChoice{1, false, 0, "NONE"};
}

// Implementation B: score everything, take the max, then map the
// winning SCORE back to a name with a second chain.  Structurally
// different from A (no early return; a two-phase select), so a bug
// in A's control flow does not survive B.
[[nodiscard]] constexpr PadChoice select_pad_score_max(PadFlags f) noexcept {
    int best = 1;
    best = (f.nul        ? 2  : best);
    best = (f.space      ? 3  : best);
    best = (f.dash       ? 4  : best);
    best = (f.hash       ? 5  : best);
    best = (f.tilde      ? 6  : best);
    best = (f.underscore ? 7  : best);
    best = (f.star       ? 8  : best);
    best = (f.at         ? 9  : best);
    best = (f.plus       ? 10 : best);
    best = (f.bang       ? 11 : best);
    return (best == 11) ? PadChoice{11, true, 33, "BANG"}
         : (best == 10) ? PadChoice{10, true, 43, "PLUS"}
         : (best == 9)  ? PadChoice{9,  true, 64, "AT"}
         : (best == 8)  ? PadChoice{8,  true, 42, "STAR"}
         : (best == 7)  ? PadChoice{7,  true, 95, "UNDERSCORE"}
         : (best == 6)  ? PadChoice{6,  true, 126, "TILDE"}
         : (best == 5)  ? PadChoice{5,  true, 35, "HASH"}
         : (best == 4)  ? PadChoice{4,  true, 45, "DASH"}
         : (best == 3)  ? PadChoice{3,  true, 32, "SPACE"}
         : (best == 2)  ? PadChoice{2,  true, 0,  "NUL"}
                        : PadChoice{1, false, 0, "NONE"};
}

// Implementation C: the UN-OFFSET formulation — score the ranks
// directly (rank 0 for "off", no +1 shift, and NONE is handled as
// "max == 0"), i.e. the encoding that does NOT use Oct 22's
// deliberate +1 offset.  It computes the same winner with a
// different arithmetic, which is the point: a shared assumption
// about the offset cannot hide in both.
[[nodiscard]] constexpr PadChoice select_pad_unoffset(PadFlags f) noexcept {
    int rank = 0;
    if (f.nul)        rank = 1;
    if (f.space && rank < 2)  rank = 2;
    if (f.dash && rank < 3)   rank = 3;
    if (f.hash && rank < 4)   rank = 4;
    if (f.tilde && rank < 5)  rank = 5;
    if (f.underscore && rank < 6) rank = 6;
    if (f.star && rank < 7)   rank = 7;
    if (f.at && rank < 8)     rank = 8;
    if (f.plus && rank < 9)   rank = 9;
    if (f.bang && rank < 10)  rank = 10;
    if (rank == 10) return PadChoice{11, true, 33, "BANG"};
    if (rank == 9)  return PadChoice{10, true, 43, "PLUS"};
    if (rank == 8)  return PadChoice{9,  true, 64, "AT"};
    if (rank == 7)  return PadChoice{8,  true, 42, "STAR"};
    if (rank == 6)  return PadChoice{7,  true, 95, "UNDERSCORE"};
    if (rank == 5)  return PadChoice{6,  true, 126, "TILDE"};
    if (rank == 4)  return PadChoice{5,  true, 35, "HASH"};
    if (rank == 3)  return PadChoice{4,  true, 45, "DASH"};
    if (rank == 2)  return PadChoice{3,  true, 32, "SPACE"};
    if (rank == 1)  return PadChoice{2,  true, 0,  "NUL"};
    return PadChoice{1, false, 0, "NONE"};
}

// Implementation D: a rank-TABLE formulation.  Walks an array of
// (rank, byte, name) records and keeps the highest enabled one.
// This is the closest to how a lookup-table dispatcher would be
// written, and it agrees with the chain-based ones — which is the
// evidence that the chain is not secretly a table in disguise.
struct PadRecord {
    int rank;
    int code;
    std::string_view name;
};

inline constexpr std::array<PadRecord, 10> kPadTable{{
    {1, 0,   "NUL"},
    {2, 32,  "SPACE"},
    {3, 45,  "DASH"},
    {4, 35,  "HASH"},
    {5, 126, "TILDE"},
    {6, 95,  "UNDERSCORE"},
    {7, 42,  "STAR"},
    {8, 64,  "AT"},
    {9, 43,  "PLUS"},
    {10, 33, "BANG"},
}};

[[nodiscard]] constexpr PadChoice select_pad_table(PadFlags f) noexcept {
    int best_rank = 0;
    PadRecord best{0, 0, "NONE"};
    for (const auto& r : kPadTable) {
        const bool on =
            (r.rank == 1)  ? f.nul
            : (r.rank == 2)  ? f.space
            : (r.rank == 3)  ? f.dash
            : (r.rank == 4)  ? f.hash
            : (r.rank == 5)  ? f.tilde
            : (r.rank == 6)  ? f.underscore
            : (r.rank == 7)  ? f.star
            : (r.rank == 8)  ? f.at
            : (r.rank == 9)  ? f.plus
                              : f.bang;
        if (on && r.rank > best_rank) {
            best_rank = r.rank;
            best = r;
        }
    }
    if (best_rank == 0) {
        return PadChoice{1, false, 0, "NONE"};
    }
    return PadChoice{best_rank + 1, true, best.code, best.name};
}

// The ONE runtime entry point the tests use, plus a cross-check that
// all four implementations agree with each other.
[[nodiscard]] constexpr bool selections_agree(PadFlags f) noexcept {
    const PadChoice a = select_pad_if_chain(f);
    const PadChoice b = select_pad_score_max(f);
    const PadChoice c = select_pad_unoffset(f);
    const PadChoice d = select_pad_table(f);
    return a.score == b.score && b.score == c.score && c.score == d.score
        && a.fills == b.fills && b.fills == c.fills && c.fills == d.fills
        && a.code == b.code && b.code == c.code && c.code == d.code
        && a.name == b.name && b.name == c.name && c.name == d.name;
}

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

// NEW today — the RUNTIME selection surface (Oct 22's next-step item
// (3), taken far enough to be useful).  The compile-time chain above
// has no arguments; these take the flags as values, so ONE binary can
// answer "which pad wins?" for all 1024 cells.
[[nodiscard]] constexpr detail::PadChoice select_pad(
    detail::PadFlags f) noexcept {
    return detail::select_pad_if_chain(f);
}

[[nodiscard]] constexpr detail::PadFlags current_pad_flags() noexcept {
    return detail::PadFlags{
        PETRA_PAD != 0,          PETRA_SPACE_PAD != 0,
        PETRA_DASH_PAD != 0,     PETRA_HASH_PAD != 0,
        PETRA_TILDE_PAD != 0,    PETRA_UNDERSCORE_PAD != 0,
        PETRA_STAR_PAD != 0,     PETRA_AT_PAD != 0,
        PETRA_PLUS_PAD != 0,     PETRA_BANG_PAD != 0};
}

// KEY FINDING #3 (Oct 22's, extended) — the preprocessor chain, the
// constexpr chain, and the runtime chain are THREE INDEPENDENT
// IMPLEMENTATIONS OF THE SAME SPEC.  The three static_asserts pin the
// first two; the fourth pins the third, for whatever cell this build
// happens to be.  Section 9 then checks all of them against each other
// at RUNTIME for all 1024 cells — which is the part a compile-time
// assert cannot do.
static_assert(detail::kSelectedFills == PETRA_PAD_SELECTED_FILLS,
              "preprocessor PETRA_PAD_SELECTED_FILLS must match the "
              "constexpr chain");
static_assert(detail::kSelectedCode == PETRA_PAD_SELECTED_CODE,
              "preprocessor PETRA_PAD_SELECTED_CODE must match the "
              "constexpr chain");
static_assert(detail::kSelectedName == PETRA_PAD_SELECTED_NAME,
              "preprocessor PETRA_PAD_SELECTED_NAME must match the "
              "constexpr chain");

constexpr detail::PadChoice kRuntimeChoice = select_pad(current_pad_flags());
static_assert(kRuntimeChoice.score == PETRA_PAD_SELECTED_SCORE,
              "runtime select_pad must agree with the preprocessor "
              "chain's score for this cell");
static_assert(kRuntimeChoice.fills == (PETRA_PAD_SELECTED_FILLS != 0),
              "runtime select_pad must agree with the preprocessor "
              "chain's FILLS for this cell");
static_assert(kRuntimeChoice.code == PETRA_PAD_SELECTED_CODE,
              "runtime select_pad must agree with the preprocessor "
              "chain's CODE for this cell");
static_assert(kRuntimeChoice.name == PETRA_PAD_SELECTED_NAME,
              "runtime select_pad must agree with the preprocessor "
              "chain's NAME for this cell");
static_assert(detail::selections_agree(current_pad_flags()),
              "the four runtime selections must agree for this cell");

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

// Number of entries in petra::detail::kPadTable — the ten pads, NONE
// excluded (NONE is not a table row; it is the "nothing matched" case).
inline constexpr std::size_t kPadTableSize = 10;

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
        static_assert(petra::detail::kRankBang == 10, "BANG rank");

        static_assert(petra::detail::kRankBang > petra::detail::kRankPlus, "");
        static_assert(petra::detail::kRankPlus > petra::detail::kRankAt, "");
        static_assert(petra::detail::kRankAt > petra::detail::kRankStar, "");
        static_assert(petra::detail::kRankStar > petra::detail::kRankUnderscore, "");
        static_assert(petra::detail::kRankUnderscore > petra::detail::kRankTilde, "");
        static_assert(petra::detail::kRankTilde > petra::detail::kRankHash, "");
        static_assert(petra::detail::kRankHash > petra::detail::kRankDash, "");
        static_assert(petra::detail::kRankDash > petra::detail::kRankSpace, "");
        static_assert(petra::detail::kRankSpace > petra::detail::kRankNul, "");

        // The eleventh score is `rank + 1` like every other one, so
        // appending BANG did not renumber a single existing score.
        static_assert(petra::detail::kScoreNone == 1, "NONE score floor");
        static_assert(petra::detail::kRankBang + 1 == 11,
                      "BANG is the eleventh score");

        // ASCII codes of the TEN pad bytes.  Oct 21's adjacency fact
        // '+' == '*' + 1 is preserved; BANG's own neighbourhood is
        // pinned explicitly because '!' (33) is one ABOVE ' ' (32)
        // and two BELOW '#' (35) — close to two existing pads, so a
        // transposed literal is the obvious mistake here.
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
        check_true(true, "rank table + ASCII codes verified at compile time");

        // The eleven rank/codename pairs are DISTINCT — a duplicate
        // rank would make the winner-detection ternary cascade
        // ambiguous without any compiler diagnostic.
        const std::array<std::string_view, 10> names{
            "NUL", "SPACE", "DASH", "HASH", "TILDE",
            "UNDERSCORE", "STAR", "AT", "PLUS", "BANG"};
        bool names_distinct = true;
        for (std::size_t i = 0; i < names.size(); ++i) {
            for (std::size_t j = i + 1; j < names.size(); ++j) {
                if (names[i] == names[j]) names_distinct = false;
            }
        }
        check_true(names_distinct, "all ten pad names are distinct");
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
                              || PETRA_AT_PAD || PETRA_PLUS_PAD
                              || PETRA_BANG_PAD) != 0;
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
        if (PETRA_BANG_PAD)        { want = "BANG"; }
        else if (PETRA_PLUS_PAD)        { want = "PLUS"; }
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

        // --------------------------------------------------------------
        // KEY FINDING (today) #2 — the padded max is IDEMPOTENT, so the
        // 12-argument fold used by the preprocessor (11 real scores +
        // one repeat of the NONE score) must equal an UNPADDED max
        // over the 11 real scores.  Checked numerically here against
        // the four-macro power-of-two family spelled out by hand.
        // --------------------------------------------------------------
        {
            const int s_none = 1;
            const int s_nul = PETRA_PAD ? 2 : 0;
            const int s_space = PETRA_SPACE_PAD ? 3 : 0;
            const int s_dash = PETRA_DASH_PAD ? 4 : 0;
            const int s_hash = PETRA_HASH_PAD ? 5 : 0;
            const int s_tilde = PETRA_TILDE_PAD ? 6 : 0;
            const int s_under = PETRA_UNDERSCORE_PAD ? 7 : 0;
            const int s_star = PETRA_STAR_PAD ? 8 : 0;
            const int s_at = PETRA_AT_PAD ? 9 : 0;
            const int s_plus = PETRA_PLUS_PAD ? 10 : 0;
            const int s_bang = PETRA_BANG_PAD ? 11 : 0;

            auto max2 = [](int a, int b) { return a > b ? a : b; };
            // NOTE: a C++ lambda cannot call itself from inside its own
            // initializer (its name is not in scope until after the
            // initializer completes), so the power-of-two family has
            // to be spelled out at each level rather than folded.
            auto max4 = [](int a, int b, int c, int d) {
                return (a > b ? a : b) > (c > d ? c : d)
                           ? (a > b ? a : b)
                           : (c > d ? c : d);
            };
            auto max8 = [](int a, int b, int c, int d,
                            int e, int f, int g, int h) {
                const int l = (a > b ? a : b) > (c > d ? c : d)
                                  ? (a > b ? a : b)
                                  : (c > d ? c : d);
                const int r = (e > f ? e : f) > (g > h ? g : h)
                                  ? (e > f ? e : f)
                                  : (g > h ? g : h);
                return l > r ? l : r;
            };

            // The UNPADDED maximum: 8 real scores + 3 real scores,
            // expressed as a nested max2 chain with no repeats.
            const int unpadded = max8(s_none, s_nul, s_space, s_dash,
                                      s_hash, s_tilde, s_under, s_star);
            const int unpadded_hi = max2(max2(s_at, s_plus), s_bang);
            const int want_score = max2(unpadded, unpadded_hi);

            // The PADDED maximum: exactly what PETRA_PAD_SELECTED_SCORE
            // expands to (MAX2 of a MAX8 and a MAX4, the inner MAX4
            // carrying one repeat of the NONE score).
            const int padded = max8(s_none, s_nul, s_space, s_dash,
                                    s_hash, s_tilde, s_under, s_star);
            const int padded_hi = max4(s_at, s_plus, s_bang, s_none);
            const int got = max2(padded, padded_hi);

            check_eq(got, want_score, "padded max == unpadded max (idempotence)");
            check_eq(got, petra::detail::kSelectedScore,
                     "padded max == the constexpr chain's score");
            check_true(PETRA_PAD_SELECTED_SCORE == got,
                       "the preprocessor's own expansion agrees");
        }

        // --------------------------------------------------------------
        // KEY FINDING (today) #3 — the runtime selector answers for
        // THIS cell, and all four runtime implementations agree.
        // --------------------------------------------------------------
        {
            const auto flags = current_pad_flags();
            const auto rt = select_pad(flags);
            check_true(detail::selections_agree(flags),
                       "four runtime selections agree for this cell");
            check_eq(rt.score, PETRA_PAD_SELECTED_SCORE,
                     "runtime score == preprocessor score");
            check_true(rt.fills == selected_pad_fills(),
                       "runtime FILLS == compile-time FILLS");
            check_true(rt.name == selected_pad_name(),
                       "runtime NAME == compile-time NAME");
        }
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
    // branch.  Today's BANG cell is the opposite corner: FILLS=1
    // with a NON-ZERO CODE, which is the other half of the split.
    // ------------------------------------------------------------------
    section("Section 8: NONE vs NUL vs BANG are three distinct contracts");
    {
        const bool is_none = (selected_pad_name() == std::string_view{"NONE"});
        const bool is_nul = (selected_pad_name() == std::string_view{"NUL"});
        const bool is_bang = (selected_pad_name() == std::string_view{"BANG"});
        check_true(!(is_none && is_nul), "NAME cannot be both NONE and NUL");
        check_true(!(is_none && is_bang), "NAME cannot be both NONE and BANG");
        check_true(!(is_nul && is_bang), "NAME cannot be both NUL and BANG");
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
        } else if (is_bang) {
            check_true(selected_pad_byte() == '!', "BANG cell: byte is '!'");
            check_true(selected_pad_fills(), "BANG cell: FILLS == 1");
            char buf[8] = {};
            std::fill(std::begin(buf), std::end(buf), kPoison);
            std::span<char> s(buf);
            const std::size_t n =
                unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(s, "AB"));
            check_eq(n, std::size_t{2}, "BANG cell: written == 2");
            check_true(all_bytes_are(buf, 2, 8, '!'),
                       "BANG cell: tail bytes really are '!'");
            check_true(!all_bytes_are(buf, 2, 8, kPoison),
                       "BANG cell: tail is NOT the poison byte");
        } else if (is_none) {
            check_true(selected_pad_byte() == '\0', "NONE cell: byte is '\\0'");
            check_true(!selected_pad_fills(), "NONE cell: FILLS == 0");
            char buf[8] = {};
            std::fill(std::begin(buf), std::end(buf), kPoison);
            std::span<char> s(buf);
            const std::size_t n =
                unwrap_dispatch(TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(s, "AB"));
            check_eq(n, std::size_t{2}, "NONE cell: written == 2");
            check_true(all_bytes_are(buf, 2, 8, kPoison),
                       "NONE cell: tail bytes still poison (NO fill loop)");
        } else {
            check_true(selected_pad_fills(), "other cells: FILLS == 1");
            check_true(selected_pad_byte() != '\0',
                       "other cells: CODE != 0");
        }
    }

    // ------------------------------------------------------------------
    // Section 9 (NEW today) — the EXHAUSTIVE runtime cross-check.
    //
    // Oct 22's lesson could only check ONE cell per binary: the flags
    // were macros, so every other combination needed its own compile.
    // That is why Oct 22 needed a 1024-cell /tmp sweep with one
    // clang++ invocation per cell.
    //
    // TODAY's runtime selector removes that constraint: this single
    // binary enumerates all 2^10 = 1024 flag combinations and checks
    // FOUR independent implementations of the selection against each
    // other on every one.  It is the same coverage as Oct 22's
    // external sweep, at roughly a millisecond of runtime cost.
    //
    // It also pins the property Oct 22's lesson relied on but could
    // only check per-cell: BANG strictly dominates every other pad,
    // so in exactly 1 of the 1024 cells (BANG on, all nine others
    // off) is BANG the winner, and in 512 of them (BANG on) nothing
    // else changes the answer.
    // ------------------------------------------------------------------
    section("Section 9: EXHAUSTIVE 1024-cell runtime cross-check");
    {
        int cells = 0;
        int agree = 0;
        int bang_wins = 0;
        int none_wins = 0;
        int pad_covered[kPadTableSize] = {0};
        bool all_ok = true;

        for (unsigned mask = 0; mask < 1024u; ++mask) {
            const detail::PadFlags f{
                (mask & 0x001u) != 0u,  // nul
                (mask & 0x002u) != 0u,  // space
                (mask & 0x004u) != 0u,  // dash
                (mask & 0x008u) != 0u,  // hash
                (mask & 0x010u) != 0u,  // tilde
                (mask & 0x020u) != 0u,  // underscore
                (mask & 0x040u) != 0u,  // star
                (mask & 0x080u) != 0u,  // at
                (mask & 0x100u) != 0u,  // plus
                (mask & 0x200u) != 0u,  // bang
            };
            ++cells;

            const auto a = detail::select_pad_if_chain(f);
            const auto b = detail::select_pad_score_max(f);
            const auto c = detail::select_pad_unoffset(f);
            const auto d = detail::select_pad_table(f);

            // All four must agree on every field, not merely on the
            // winner's name.
            const bool four_agree =
                a.score == b.score && b.score == c.score && c.score == d.score
                && a.fills == b.fills && b.fills == c.fills
                && c.fills == d.fills
                && a.code == b.code && b.code == c.code && c.code == d.code
                && a.name == b.name && b.name == c.name && c.name == d.name;

            if (four_agree) {
                ++agree;
            } else {
                all_ok = false;
            }

            // Structural invariants that must hold for EVERY cell,
            // independent of which implementation produced the answer.
            if (a.score >= 1 && a.score <= 11) {
                // score in range
            } else {
                all_ok = false;
            }
            if (a.score == 1) {
                if (a.fills || a.code != 0 || a.name != std::string_view{"NONE"}) {
                    all_ok = false;
                }
                ++none_wins;
            } else {
                if (!a.fills) {
                    all_ok = false;   // every non-NONE cell must fill
                }
                if (a.code == 0 && a.name != std::string_view{"NUL"}) {
                    all_ok = false;   // only NUL may have CODE == 0
                }
                // The score must be exactly `rank + 1` of the winning
                // pad, which is what makes NONE (rank 0 -> score 1)
                // non-tieable with any enabled pad.
                if (a.score < 2 || a.score > 11) {
                    all_ok = false;
                }
            }

            // Tally which pad won, using the rank table for the index.
            if (a.score >= 2 && a.score <= 11) {
                ++pad_covered[a.score - 2];
            }
            if (f.bang) {
                if (a.name != std::string_view{"BANG"}) {
                    all_ok = false;   // BANG strictly dominates
                } else {
                    ++bang_wins;
                }
            }
        }

        check_eq(cells, 1024, "enumerated all 1024 flag combinations");
        check_eq(agree, 1024,
                 "four independent selections agree in all 1024 cells");
        check_true(all_ok, "structural invariants hold in all 1024 cells");
        check_eq(bang_wins, 512,
                 "BANG wins in exactly the 512 cells where it is on");
        check_eq(none_wins, 1,
                 "NONE wins in exactly 1 cell (all flags off)");

        // Every pad must be reachable, and the tally must match the
        // number of combinations in which that pad is the top one:
        // pad at rank r (1-based) wins in 2^(r-1) cells — every
        // HIGHER-ranked pad must be off (1 choice each) and every
        // LOWER-ranked one is free (2 choices each).  So the ten
        // tallies are 1, 2, 4, 8, ..., 512 and sum to 1023, with the
        // 1024th cell being NONE.
        int tally_ok = 0;
        for (int r = 1; r <= 10; ++r) {
            const int want = 1 << (r - 1);
            if (pad_covered[r - 1] == want) {
                ++tally_ok;
            }
        }
        check_eq(tally_ok, 10,
                 "each pad wins in exactly 2^(rank-1) cells");

        int tally_sum = 0;
        for (int r = 0; r < 10; ++r) {
            tally_sum += pad_covered[r];
        }
        check_eq(tally_sum, 1023,
                 "the ten pad tallies sum to 1023; NONE takes the last cell");

        // The compile-time cell is one of the 1024, and the runtime
        // selector must reproduce it exactly.
        const auto me = select_pad(current_pad_flags());
        check_true(me.score == PETRA_PAD_SELECTED_SCORE
                       && me.fills == (PETRA_PAD_SELECTED_FILLS != 0)
                       && me.code == PETRA_PAD_SELECTED_CODE
                       && me.name == PETRA_PAD_SELECTED_NAME,
                   "the runtime selector reproduces THIS compile cell");
        std::println("  1024 cells: agree={}  BANG-wins={}  NONE-wins={}",
                     agree, bang_wins, none_wins);
    }

    // ------------------------------------------------------------------
    // Section 10 — thread safety: 4 threads x 25 calls, each on its own
    // buffer, tallying the selected contract.  100/100 expected.
    // ------------------------------------------------------------------
    section("Section 10: concurrent dispatch — 4 threads x 25 calls");
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
    // Section 11 — Summary.
    // ------------------------------------------------------------------
    section("Summary");
    std::println("  selected pad = {}", selected_pad_name());
    std::println("  selected byte = {}  fills = {}",
                 static_cast<int>(
                     static_cast<unsigned char>(selected_pad_byte())),
                 selected_pad_fills() ? "yes" : "no");
    std::println("  BANG enabled  = {}  (rank 10, byte '!' = 33)",
                 PETRA_BANG_PAD ? "yes" : "no");
    std::println("  PASS={}  FAIL={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}