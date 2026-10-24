// P-2026-10-24 — try_format_xmacro_pad_table:
//
// Closes Oct 23's "Where we go next" items (2) and (3) TOGETHER, because
// turning them together turns the whole arc's 1024-cell EXTERNAL compile
// sweep into a single in-process RUN:
//
//   item (2) — "use the runtime selector for the FILL LOOP too … a
//              runtime fill_tail(buf, written, choice) would let one
//              binary serve all 1024 cells completely rather than just
//              the selection".
//   item (3) — "make the rank order itself DATA … a single X-macro list
//              (#define PETRA_PAD_LIST(X) X(NUL,1,0) X(SPACE,2,32) …)
//              would generate all five so the twelfth pad is one line
//              instead of eleven, with Section 9's cross-check as the
//              safety net".
//
// TODAY'S LESSON IS ABOUT WHAT "DATA" ACTUALLY BUYS, and the answer is
// not the one the note expected:
//
// KEY FINDING #1 — the RANK DATA really does collapse to ONE line per
// pad.  Oct 23 stated the rank order in FIVE places: the eleven
// PETRA_PAD_SCORE_* macros, the FILLS cascade, the CODE cascade, the
// NAME cascade, and the hand-written constexpr rank/score constants
// (plus a sixth, the runtime kPadTable).  Today all of that is
// generated from ONE X-macro list, PETRA_PAD_LIST, whose rows have the
// shape X(NAME, FLAG_MACRO, RANK, FILL_CODE) — so the list itself is
// the only place the order is written down.
//
//   NOTE: this comment deliberately does NOT restate the list's rows.
//   An earlier draft of this file DID quote them here, and the
//   "twelfth pad" experiment (see the notes) then patched the COMMENT
//   copy instead of the macro — which failed to compile with a
//   -Wcomment multi-line-comment error and two static_assert failures.
//   Restating generated data in prose is how it drifts out of sync, so
//   the rows live in the macro and nowhere else.
//
// A twelfth pad is ONE line in that list.  The X-macro generates the
// constexpr kPadTable (name/rank/code/flag), and the table's LENGTH is
// deduced from the list itself — so the data is written down in
// exactly one place.
//
// KEY FINDING #2 (the one the note did NOT expect) — the preprocessor
// PRIORITY CASCADE does not need to survive in any form, because a
// TEMPLATE ARGUMENT MAY BE A CONSTEXPR VARIABLE.  Oct 23 spent three
// eleven-arm ternary cascades (FILES, CODE, NAME) plus a
// power-of-two MAX family purely to turn "which pad won?" into two
// compile-time constants.  Today those constants are computed by a
// constexpr function over the generated table, and the dispatcher macro
// passes them as template arguments directly:
//
//   try_format_runtime_n_padded_span<detail::kSelectedFills,
//                                     detail::kSelectedByte>(…)
//
// So the preprocessor's remaining job is EXACTLY ONE THING: turn the
// -DPETRA_*_PAD=0/1 macros into the `enabled` field of each table row.
// The 40 lines of MAX2/MAX4/MAX8/MAX16 and the 33 cascade arms are
// gone.
//
// KEY FINDING #3 — deleting the cascade deletes the SAFETY NET, and
// that is the real trade-off.  Oct 22/23 pinned a preprocessor chain
// against a constexpr chain with static_asserts precisely because two
// hand-written implementations of one spec catch each other's typos.
// An X-macro makes the two implementations IMPOSSIBLE to write
// independently (they would share the list), so the cross-check has
// to be re-based on something else.  Today's answer: keep TWO
// HAND-WRITTEN ORACLES that do not read the list at all —
// kOracleTable (Oct 23's rank table transcribed by hand) and
// oracle_select (an if-chain written out pad by pad) — and check
// them against the generated data for ALL 1024 cells at runtime.
//
// KEY FINDING #4 — the COUNT was the last piece of duplication, and it
// hides in plain sight.  The first draft wrote std::array<PadSpec, 10>
// and std::array<bool, 10> and std::array<OracleRow, 10>, i.e. the
// number 10 was still written down in three places.  The fix is to let
// the COMPILER deduce the bound (a raw constexpr array with an
// initializer list has its size deduced from the initializers, and
// std::size reports it), so the pad count is derived from the list
// rather than stated alongside it.  The generalisable part: a count
// written next to a list is a second copy of that list.
//
// KEY FINDING #5 — the rank+1 SCORE survives, and so does its
// identity.  The score of the winner is still rank+1 (NONE still
// scores 1), so every number Oct 22 and Oct 23 published is unchanged;
// but the score is now a PROPERTY of the table lookup rather than a
// separate set of eleven constants.  The win-count identity (the pad
// at rank r wins in exactly 2^(r-1) of the 1024 cells) is re-verified
// at runtime over all 1024 cells.
//
// KEY FINDING #6 — the FILL can be runtime, and it costs one `if`
// instead of a template parameter.  Oct 23's padded wrapper takes
// <bool Fills, char Fill>, so the fill loop is compiled in or out.
// Today's runtime sibling takes a PadChoice VALUE, so ONE binary can
// fill with ANY of the ten pad bytes (or not fill at all) — which is
// what makes the single-binary sweep possible.  The compile-time path
// is still there, still `if constexpr`, and Section 4 pins the two
// against each other byte for byte.
//
// WHAT DID NOT CHANGE (deliberately):
//   - petra::FormatError / FormatErrorKind, the two-pass
//     (measure, then write) impls, the buffer-atomicity contract on
//     TooLarge and on Format errors, and the std::span<char> surface
//     are all reused verbatim from Oct 23 / Oct 21 / Sep 8 / Sep 9.
//   - The compile-time flag NAMES (PETRA_PAD, PETRA_SPACE_PAD, …) and
//     the -D contract are unchanged, so yesterday's build cells keep
//     their meaning.
//   - The score semantics (rank+1, NONE=1) are unchanged, so the
//     numbers published on Oct 22 and Oct 23 are unchanged.
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
//           P-2026-10-24-try-format-xmacro-pad-table.cpp \
//           -o /tmp/petra20261024/bin_default
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-24-try-format-xmacro-pad-table
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
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

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
// Part 6 — the GENERIC compile-time padded span<char> wrappers.
//
//   template <bool Fills, char Fill, typename... Args>
//   std::expected<std::size_t, FormatError>
//   try_format_runtime_n_padded_span(std::span<char>, std::string_view, Args...)
//
// `Fills` is a SEPARATE non-type parameter rather than being encoded in
// `Fill` because NUL-pad (writes '\0') and NONE (writes nothing) are
// different contracts with the same byte value.  `if constexpr (Fills)`
// means the NONE cell emits NO fill loop at all.
//
// VERBATIM from Oct 22 / Oct 23 — today's lesson does not change this
// path, it ADDS a runtime sibling next to it and pins the two together.
// ============================================================================

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

// ============================================================================
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
// Part 10 — the RUNTIME padded wrappers: today's answer to Oct 23's
// next-step item (2).
//
// The compile-time wrapper takes <bool Fills, char Fill>; the runtime
// sibling takes a PadChoice VALUE.  One binary can therefore fill with
// any of the ten pad bytes — or not fill at all — which is what makes
// the single-binary sweep in Section 9 possible.
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

// NEW today — RUNTIME + EXPECTED padded span<char> sibling.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span_runtime(std::span<char> out,
                                         std::string_view fmt,
                                         detail::PadChoice choice,
                                         const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value() && choice.fills) {
        const std::size_t written = r.value();
        std::fill(out.begin() + static_cast<std::ptrdiff_t>(written),
                  out.end(), static_cast<char>(choice.code));
    }
    return r;
}

// NEW today — RUNTIME + THROWING padded span<char> sibling.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_span_runtime(std::span<char> out,
                                                   std::string_view fmt,
                                                   detail::PadChoice choice,
                                                   const Args&... args) {
    const std::size_t n =
        try_format_runtime_n_throwing(out, fmt, args...);
    if (choice.fills) {
        std::fill(out.begin() + static_cast<std::ptrdiff_t>(n),
                  out.end(), static_cast<char>(choice.code));
    }
    return n;
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
    // Section 4 — the RUNTIME padded path (today's new sibling) must
    // produce BYTE-IDENTICAL results to the compile-time path for the
    // same choice, and must honour the NONE contract when asked not to
    // fill.
    // ------------------------------------------------------------------
    section("Section 4: runtime padded path == compile-time padded path");
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
    // Section 5 — the CONSTEVAL surface through the meta-macro (the
    // std::format_string<Args...> consteval gate still applies).
    // ------------------------------------------------------------------
    section("Section 5: CONSTEVAL meta-macro surface");
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
    // Section 6 — buffer atomicity on TooLarge, on BOTH paths.
    // ------------------------------------------------------------------
    section("Section 6: buffer UNTOUCHED on TooLarge");
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
    }

    // ------------------------------------------------------------------
    // Section 7 — buffer atomicity on a Format error (a malformed format
    // string fails at format time, before any write).
    // ------------------------------------------------------------------
    section("Section 7: buffer UNTOUCHED on a Format error");
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
    }

    // ------------------------------------------------------------------
    // Section 8 — edge cases: written == size, and size == 0.
    // ------------------------------------------------------------------
    section("Section 8: edge cases (full buffer, zero-size buffer)");
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
    }

    // ------------------------------------------------------------------
    // Section 9 — THE EXHAUSTIVE SINGLE-BINARY SWEEP: all 1024 cells,
    // in one process, in about a millisecond.
    //
    // For every one of the 1024 flag combinations:
    //   - the three runtime spellings must agree (table walk, bit width,
    //     hand-written oracle);
    //   - the RUNTIME padded path must write the winner's byte into the
    //     tail (or leave the poison alone for NONE);
    //   - the NONE cell must leave the poison alone;
    //   - the win tally must satisfy the 2^(r-1) identity.
    // ------------------------------------------------------------------
    section("Section 9: exhaustive 1024-cell sweep (one binary)");
    {
        // The win tallies are sized by the DEDUCED pad count, so this
        // section does not have to know how many pads there are.
        std::array<std::uint32_t, petra::detail::kPadCount> wins{};
        std::uint32_t none_wins = 0;
        int disagree = 0;
        int byte_fail = 0;

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

        std::println("sweep: cells={} disagree={} byte_fail={} none_wins={}",
                     kCellCount, disagree, byte_fail, none_wins);
        for (std::size_t i = 0; i < wins.size(); ++i) {
            std::println("  rank {:2} {:<11} wins {:4} cells",
                         petra::detail::kPadTable[i].rank,
                         petra::detail::kPadTable[i].name,
                         wins[i]);
        }
    }

    // ------------------------------------------------------------------
    // Section 10 — thread safety: the runtime padded path is stateless
    // (all per-call state is a stack local), so N threads may use it
    // concurrently with different masks.
    // ------------------------------------------------------------------
    section("Section 10: concurrent runtime padded dispatch");
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
    }

    std::println("\npass={} fail={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
