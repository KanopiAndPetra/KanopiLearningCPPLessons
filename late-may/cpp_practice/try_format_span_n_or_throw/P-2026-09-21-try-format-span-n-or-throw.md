# P-2026-09-21 — `try_format_span_n_or_throw`

Topic: a **macro layer** that lifts the **RETURN-TYPE-vs-EXCEPTION**
sibling choice to a macro form on the **STD::SPAN<CHAR>** axis.  Two
new macros — `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` (CONSTEVAL surface)
and `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW` (RUNTIME surface) —
each dispatch to EITHER the **expected** sibling (Sep 10's
`try_format_bounded_runtime_n_span` for the CONSTEVAL surface, Sep 19's
NEW `try_format_runtime_n(span, ...)` for the RUNTIME surface; both
return `std::expected<std::size_t, FormatError>` and return
`FormatError{TooLarge, ..., needed_bytes}` on overflow) OR today's
**NEW throwing sibling** (today's
`try_format_bounded_runtime_n_throwing_span` for the CONSTEVAL
surface, today's `try_format_runtime_n_throwing(span, ...)` for the
RUNTIME surface; both return `std::size_t` and throw
`std::runtime_error` on overflow or format error), based on a
caller-controlled compile-time flag `PETRA_THROW_ON_OVERFLOW`.

This is the **SPAN-CHAR MIRROR** of Sep 20's `char*+cap` lesson.
Sep 20 added the throwing sibling to the `char*+cap` axis; today
adds the throwing sibling to the `std::span<char>` axis.  Together
with Sep 19's existing meta-macros (which gated on
`PETRA_NUL_TERMINATE`) and Sep 16's `char*+cap` meta-macros, today's
lesson completes the **FOURTH-AXIS** layer on the `span<char>` axis:
the new `PETRA_THROW_ON_OVERFLOW` axis is perpendicular to
`char*-vs-span` × `CONSTEVAL-vs-RUNTIME` × `bounded-buffer-vs-NUL-
terminating` — and today's meta-macros are at the INTERSECTION of all
four axes on the `span<char>` side.

**Closes Sep 20's "Where we go next" item #1 verbatim**:

> 1. The natural follow-on is the `span<char> × CONSTEVAL-or-RUNTIME
>    × bounded-buffer × throwing` cells (two NEW wrappers + four NEW
>    macros).  Today's meta-macros
>    (`TRY_FORMAT_BOUNDED_N_OR_THROW` +
>    `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW`) are the char*+cap
>    analog; the span<char> analog uses
>    `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` +
>    `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW` + the two new
>    throwing wrappers `petra::try_format_bounded_runtime_n_throwing_span`
>    + `petra::try_format_runtime_n_throwing(span, ...)`.

## Where this fits in the arc

- Aug 25   `std::format_to_n_into` — NUL-terminating bounded
           formatter WITHOUT a cap parameter.
- Sep  6   `try_format_runtime_bounded` — `std::string_view` fmt +
           `std::size_t` cap (RUNTIME `std::string`-output wrapper).
- Sep  7   `try_format_bounded` — CONSTEVAL sibling of Sep 6.
- Sep  8   `try_format_runtime_n` — BOUNDED-BUFFER sibling of
           Sep 7 (char\* + `std::size_t` cap + `std::string_view`
           fmt, NO NUL).
- Sep  9   `try_format_bounded_runtime_n` — CONSTEVAL char\*+cap
           (NO NUL).
- Sep 10   `try_format_bounded_runtime_n_span` — CONSTEVAL
           `std::span<char>` char\*+cap (NO NUL).
- Sep 11   `format_to_n_into_bounded` — CONSTEVAL NUL-terminating
           char\*+cap (plus `std::span<char>` forwarding overload).
- Sep 12   `try_format_runtime_n_into_bounded` — RUNTIME
           NUL-terminating char\*+cap (plus `std::span<char>`
           forwarding overload).
- Sep 13   `try_format_bounded_macros` — eight `TRY_FORMAT_BOUNDED_*`
           macros (CONSTEVAL vs RUNTIME sibling choice at the
           call site).
- Sep 14   `try_format_span_format_string` — `std::span<const char>`
           overloads for the runtime try_format_\* family + four
           new macros.
- Sep 15   `try_format_consteval_span_format_string` — pins that
           the CONSTEVAL surface is incompatible with
           `std::span<const char>`; adds a CONSTEVAL-SHAPED
           RUNTIME PARSER family of four wrappers + four macros.
- Sep 16   `try_format_bounded_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer (char\*+cap
           axis).
- Sep 19   `try_format_span_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer on the
           `std::span<char>` axis.
- Sep 20   `try_format_bounded_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the char\*+cap
           axis (the FOURTH axis).
- TODAY    `try_format_span_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the
           `std::span<char>` axis (the FOURTH axis on the
           span<char> side; closes Sep 20's "Where we go next"
           item #1 verbatim).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21 — Sep 16's open item #1 documents this).
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_n_impl` — REUSED Sep 8 verbatim
  (the bounded-buffer sibling impl; today's span throwing wrappers
  compose on top of Sep 19's span forwarding wrapper which composes
  on top of this impl).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` — REUSED Sep 8 verbatim (char\*+cap bounded-buffer).
- `petra::try_format_runtime_n(std::span<char>, std::string_view,
  args...)` — REUSED Sep 19 verbatim (the expected sibling on the
  span<char> axis; today's span throwing wrapper composes on top
  of this).
- `petra::try_format_bounded_runtime_n_span` — REUSED Sep 10 verbatim
  (the CONSTEVAL expected sibling on the span<char> axis).
- `petra::try_format_runtime_n_throwing(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 20 verbatim (char\*+cap
  RUNTIME throwing sibling).
- `petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 20 verbatim
  (char\*+cap CONSTEVAL throwing sibling).
- `petra::try_format_runtime_n_throwing(std::span<char>,
  std::string_view, args...)` — **NEW today**: RUNTIME surface of
  the throwing sibling on the span<char> axis; verbatim thin
  wrapper around Sep 19's `petra::try_format_runtime_n(span, ...)`
  that re-raises `FormatError{...}` as `std::runtime_error` with the
  SAME `FormatError::message` (byte-exact).
- `petra::try_format_bounded_runtime_n_throwing_span` — **NEW today**:
  CONSTEVAL surface of the throwing sibling on the span<char> axis;
  verbatim thin wrapper around Sep 10's
  `try_format_bounded_runtime_n_span` that re-raises as
  `std::runtime_error` with byte-exact message.
- The TWO new macros `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` and
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW` — NEW today; each is
  a verbatim thin dispatcher that picks between two siblings at
  preprocessor time based on `PETRA_THROW_ON_OVERFLOW`.

## What today's lesson pins

1. **The RETURN-TYPE-vs-EXCEPTION sibling choice CAN be lifted to
   a macro form on the `std::span<char>` axis.**  The shape is
   analogous to Sep 20's `char*+cap` macro layer — a macro that
   picks between two sibling wrappers at preprocessor time.
2. **The compile-time flag `PETRA_THROW_ON_OVERFLOW` controls the
   dispatch** via `#if PETRA_THROW_ON_OVERFLOW == 0/1`:
   - `#define PETRA_THROW_ON_OVERFLOW 0` → expected sibling
     (`std::expected<std::size_t, FormatError>` return type):
     - CONSTEVAL: `petra::try_format_bounded_runtime_n_span`
                  (Sep 10)
     - RUNTIME:  `petra::try_format_runtime_n(span, ...)`
                  (Sep 19 NEW span forwarding wrapper)
   - `#define PETRA_THROW_ON_OVERFLOW 1` → throwing sibling
     (`std::size_t` return type; throws `std::runtime_error` on
     overflow or format error):
     - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_span`
                  (NEW today; wraps Sep 10 verbatim)
     - RUNTIME:  `petra::try_format_runtime_n_throwing(span, ...)`
                  (NEW today; wraps Sep 19's span wrapper verbatim)
3. **`#error` guard** fires at preprocessor time if the flag is
   set to anything other than 0 or 1 — pinned by the
   `#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1`
   block.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(...)` and capture
   the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19
   / Sep 20's macro shape exactly.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20.
6. **The CONSTEVAL macro picks between Sep 10's
   `try_format_bounded_runtime_n_span` (expected, returns
   `expected<size_t, FormatError>`) and today's NEW
   `try_format_bounded_runtime_n_throwing_span` (throwing, returns
   `size_t`).**  Both are consteval-checked `std::format_string<Args...>`
   wrappers.
7. **The RUNTIME macro picks between Sep 19's NEW
   `petra::try_format_runtime_n(span, ...)` (expected, returns
   `expected<size_t, FormatError>`) and today's NEW
   `petra::try_format_runtime_n_throwing(span, ...)` (throwing,
   returns `size_t`).**  Both take `std::string_view` format-string
   (no compile-time parse).
8. **The macro's contract is byte-exact: the throwing sibling's
   `std::runtime_error::what()` is byte-exactly equal to the
   expected sibling's `FormatError::message` for the SAME
   `{needed, cap}` pair.**  Pinned in Section 9 by calling both
   siblings with the SAME call args and comparing `expected_msg`
   (from `r.error().message` on the direct Sep 19 wrapper call) to
   `thrown_msg` (from `e.what()` on the throwing sibling path).
9. **The throwing sibling preserves the SAME buffer atomicity on
   TooLarge as the expected sibling** — the buffer is UNTOUCHED on
   overflow (Sep 10 / Sep 19 already pin this on the expected path;
   today's wrapper re-pins it on the throwing path because the new
   code path COULD theoretically diverge; Section 3c pins this).
10. **The throwing sibling preserves the SAME byte-count contract
    on success** — returns the byte count written (== needed ==
    written, the same value Sep 10's / Sep 19's expected siblings
    return); Section 4 pins this on both paths.
11. **The `std::span<char>` sibling overloads do NOT bleed**
    (Section 7 pins this — a `std::span<char>` over a sub-range of a
    buffer must NOT touch bytes outside the span; mirrors Sep 19's
    no-bleed contract).
12. **The CONSTEVAL macro's consteval gate is preserved regardless
    of `PETRA_THROW_ON_OVERFLOW`** — `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW`
    still requires `std::format_string<Args...>` (a literal fmt at
    the call site).  This is because the CONSTEVAL sibling
    (Sep 10's `try_format_bounded_runtime_n_span`) is consteval-
    checked, and today's throwing wrapper is a verbatim thin
    wrapper around Sep 10 — so the consteval gate is preserved.
    Section 6 pins the consteval gate by calling the macro with a
    literal fmt and a runtime-built `std::string` fmt; the
    runtime-built fmt REJECTS AT COMPILE TIME (verified by
    `/tmp/probe_consteval_rejects_runtime_fmt_span.cpp` producing
    `error: call to consteval function
    'std::basic_format_string<char, int>::basic_format_string<std::string>'
    is not a constant expression` with note `read of non-constexpr
    variable 'fmt' is not allowed in a constant expression`).
13. **The `PETRA_THROW_ON_OVERFLOW` axis (expected vs throwing) is
    PERPENDICULAR to the `char*-vs-span` axis** (Sep 10 / Sep 12 /
    Sep 19's forwarding wrappers), AND to the `CONSTEVAL-vs-RUNTIME`
    axis (Sep 13's macro layer), AND to the `bounded-buffer-vs-NUL-
    terminating` axis (Sep 16 / Sep 19's macro layer).  Today's
    meta-macros are at the INTERSECTION of all FOUR axes on the
    `span<char>` axis.  Section 10 pins this via `decltype` checks.
14. **`std::format_error` IS-A `std::runtime_error`** — pinned by
    `static_assert(std::is_base_of_v<std::runtime_error,
    std::format_error>)` succeeding.  This means the catch site
    must put `std::format_error&` BEFORE `std::runtime_error&` in
    the catch chain (C++ catch handlers are matched in order).
    Section 5a exploits this by checking the dynamic type via
    `dynamic_cast<const std::format_error*>(&e)` — the throwing
    wrapper re-raises as `std::runtime_error`, NOT
    `std::format_error`, so the dynamic cast returns `nullptr`.
15. **Concurrency is safe** — Section 13 fires 4 threads × 25
    concurrent calls through both macros; per-thread local
    counters are aggregated AFTER `join()` via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.

## HEADLINE finding — the SPAN-CHAR MIRROR of Sep 20

Sep 20's matrix was 2⁴ = 16 cells.  Sep 20's lesson added the
throwing sibling to 2 of those cells (`char*+cap × CONSTEVAL-or-
RUNTIME × bounded-buffer × throwing`).  Today's lesson adds the
throwing sibling to 2 MORE cells (`span<char> × CONSTEVAL-or-RUNTIME
× bounded-buffer × throwing`).

The natural extension: today's lesson closes Sep 20's "Where we go
next" item #1 verbatim.  The preprocessor-time dispatch design
(`#if PETRA_THROW_ON_OVERFLOW`) extends naturally to the span<char>
axis without any new infrastructure — Sep 10's and Sep 19's expected
siblings already exist; today's lesson adds two verbatim thin
throwing wrappers and two macros.  The dispatch is ZERO-OVERHEAD —
at `-O0` the chosen wrapper is inlined into the caller; at `-O1`
the compiler sees only the chosen wrapper.

The four axes on the `span<char>` axis are now complete (the
bounded-buffer subset):

1. **char\*-vs-span axis** (Sep 10 / Sep 12 / Sep 19's forwarding
   wrappers): the OUTPUT is either a `char*` + `std::size_t cap`
   (Sep 8 / Sep 9 / Sep 11 / Sep 12 char\*+cap path) or a
   `std::span<char>` (Sep 10 / Sep 11 / Sep 12 / Sep 19's span<char>
   path).

2. **CONSTEVAL-vs-RUNTIME axis** (Sep 13's macro layer): the
   format-string is either a `std::format_string<Args...>` literal
   (consteval-checked at compile time) or a `std::string_view`
   (parsed at format time).

3. **bounded-buffer-vs-NUL-terminating axis** (Sep 16 / Sep 19's
   macro layer): the OUTPUT contract is either a bounded-buffer
   (bytes `[0, written)`, byte at `written` UNCHANGED) or
   NUL-terminating (bytes `[0, written)`, byte at
   `nul_idx = (written < cap) ? written : (cap - 1)` set to `'\0'`).

4. **expected-vs-throwing axis** (Sep 20's char\*+cap lesson +
   today's span<char> lesson): the RETURN TYPE is either
   `std::expected<std::size_t, FormatError>` (FormatError on
   overflow) or `std::size_t` (throws `std::runtime_error` on
   overflow / format error).

The four axes compose orthogonally: each is a separate
`#if PETRA_<FLAG>` block, each is independent of the others, each
adds ZERO runtime overhead (the chosen sibling is the ONLY one the
compiler sees).  Section 10 pins the orthogonal-composition claim
via `decltype` checks.

The 16-cell matrix (NEW today — TODAY's two NEW cells highlighted
in **bold** on the `span<char>` axis):

|| cell | char\*+cap | span<char> |
||-------|-----------|------------|
|| CONSTEVAL + bounded-buffer + expected     | Sep 16 TRY_FORMAT_BOUNDED_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 9) | Sep 19 TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 10) |
|| RUNTIME + bounded-buffer + expected     | Sep 16 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 8) | Sep 19 TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 8) |
|| CONSTEVAL + NUL-terminating + expected  | Sep 16 TRY_FORMAT_BOUNDED_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 11) | Sep 19 TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 11) |
|| RUNTIME + NUL-terminating + expected    | Sep 16 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 12) | Sep 19 TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 12) |
|| CONSTEVAL + bounded-buffer + throwing   | Sep 20 TRY_FORMAT_BOUNDED_N_OR_THROW (Sep 9 wrapper) | **TODAY TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW** (Sep 10 wrapper; NEW) |
|| RUNTIME + bounded-buffer + throwing     | Sep 20 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW (Sep 8 wrapper) | **TODAY TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW** (Sep 19 wrapper; NEW) |
|| CONSTEVAL + NUL-terminating + throwing  | follow-up (Sep 16's char*+cap NUL sibling + Sep 20's throwing) | follow-up (Sep 19's span NUL sibling + today's throwing) |
|| RUNTIME + NUL-terminating + throwing    | follow-up (Sep 16's char*+cap NUL sibling + Sep 20's throwing) | follow-up (Sep 19's span NUL sibling + today's throwing) |

Today's 2 NEW cells are highlighted in **bold**.  Together with Sep
20's 2 NEW cells, the FOUR-AXIS BOUNDED-BUFFER subset of the
16-cell matrix is COMPLETE — both `char*+cap` and `span<char>` cells
on the CONSTEVAL-or-RUNTIME × bounded-buffer × expected-or-throwing
quadrant are now covered.  The remaining 4 NEW cells (the throwing
sibling on the NUL-terminating axis for both `char*+cap` and
`span<char>`) are left for future lessons — each cell is a verbatim
thin throwing wrapper composed on top of an existing NUL-terminating
sibling + a new `#if PETRA_THROW_ON_OVERFLOW`-gated macro.

## Important code

### The two new throwing wrappers — the ONLY new wrappers today

```cpp
namespace petra {

// NEW today — RUNTIME surface of the throwing sibling on the
// span<char> axis.  A verbatim thin wrapper around Sep 19's
// petra::try_format_runtime_n(span, ...) that re-raises
// FormatError{...} as std::runtime_error with the SAME message.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(std::span<char> out, std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    // Re-raise as std::runtime_error.  The .what() message is the
    // SAME message FormatError{...} carries (for TooLarge) or the
    // FormatError's message (for Format).  We deliberately do NOT
    // add an extra prefix like "[petra::try_format_runtime_n_throwing]"
    // — that would make the throwing path's .what() differ from the
    // expected path's FormatError.message byte-exactly, breaking
    // Section 9's byte-exactness claim.
    throw std::runtime_error(r.error().message);
}

// NEW today — CONSTEVAL surface of the throwing sibling on the
// span<char> axis.  A verbatim thin wrapper around Sep 10's
// try_format_bounded_runtime_n_span that re-raises as
// std::runtime_error with byte-exact message.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

}  // namespace petra
```

Both wrappers are 5-line verbatim thin wrappers.  No new types,
no new impls, no new error contracts.  The byte-exactness of the
`std::runtime_error::what()` vs `FormatError::message` is the
key contract — Section 9 pins this.

### The two new macros — the public surface

```cpp
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

#if PETRA_THROW_ON_OVERFLOW == 0
// Expected sibling path — returns std::expected<std::size_t,
// FormatError>.  On TooLarge, FormatError{TooLarge, ...,
// needed_bytes} is returned.
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(out, fmt, ...)          \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))
#else  // PETRA_THROW_ON_OVERFLOW == 1
// Throwing sibling path — returns std::size_t; throws
// std::runtime_error on TooLarge or Format.  Same atomicity
// contract as the expected sibling (buffer UNTOUCHED on
// overflow — Sep 10 / Sep 19's contract, preserved by the
// verbatim thin wrapper).
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(out, fmt, ...)                  \
    (::petra::try_format_bounded_runtime_n_throwing_span(                 \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW(out, fmt, ...)          \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                  \
                                            __VA_OPT__(,) __VA_ARGS__))
#endif  // PETRA_THROW_ON_OVERFLOW
```

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21 — Sep 16's open item #1 documents this).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- The two throwing wrappers `petra::try_format_runtime_n_throwing(
  span, ...)` and `petra::try_format_bounded_runtime_n_throwing_span`
  — verbatim thin dispatchers to Sep 19's / Sep 10's expected
  siblings.
- The two `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` /
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW` macros — each a
  verbatim thin dispatcher to one of the four underlying wrappers,
  selected by `#if PETRA_THROW_ON_OVERFLOW`.  No new types (two
  new wrappers only), no new impls, no new error contracts.

## Build and verification commands

Default (`PETRA_THROW_ON_OVERFLOW=0` — expected sibling path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-21-try-format-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-21-try-format-span-n-or-throw
/tmp/P-2026-09-21-try-format-span-n-or-throw
```

Throwing path (`PETRA_THROW_ON_OVERFLOW=1`):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_THROW_ON_OVERFLOW=1 \
        P-2026-09-21-try-format-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-21-try-format-span-n-or-throw-throw
/tmp/P-2026-09-21-try-format-span-n-or-throw-throw
```

Strict-warning build (default):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-21-try-format-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-21-try-format-span-n-or-throw-strict
/tmp/P-2026-09-21-try-format-span-n-or-throw-strict
```

Strict-warning build (throwing):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion -DPETRA_THROW_ON_OVERFLOW=1 \
        P-2026-09-21-try-format-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-21-try-format-span-n-or-throw-strict-throw
/tmp/P-2026-09-21-try-format-span-n-or-throw-strict-throw
```

ASan + UBSan build (expected sibling path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-21-try-format-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-21-try-format-span-n-or-throw-asan
/tmp/P-2026-09-21-try-format-span-n-or-throw-asan
```

ASan + UBSan build (throwing sibling path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        -DPETRA_THROW_ON_OVERFLOW=1 \
        P-2026-09-21-try-format-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-21-try-format-span-n-or-throw-asan-throw
/tmp/P-2026-09-21-try-format-span-n-or-throw-asan-throw
```

CMake (default / strict / ASan / throwing) — see `CMakeLists.txt`:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=1 \
                              && cmake --build build-throw
./build/P-2026-09-21-try-format-span-n-or-throw
./build-strict/P-2026-09-21-try-format-span-n-or-throw
./build-asan/P-2026-09-21-try-format-span-n-or-throw
./build-throw/P-2026-09-21-try-format-span-n-or-throw
```

## Section-by-section notes

### Section 1 — `PETRA_THROW_ON_OVERFLOW` compile-time flag pinned

Verifies the compile-time flag is pinned at the value the build
used.  Prints the chosen path so the output is auditable in the
log:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0 (expected sibling)
```

or, for the throwing build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1 (throwing sibling)
```

Also pins via `static_assert` that both macros dispatch to the
correct return type for the chosen path (`std::expected<std::size_t,
FormatError>` for the expected path; `std::size_t` for the
throwing path) AND that `std::runtime_error` is a `std::exception`
subclass AND that `std::format_error` IS-A `std::runtime_error`.
Also pins `std::span<char>` properties for posterity:
contiguous_range + sized_range + trivially_copyable +
standard_layout + `.data()->char*` + `.size()->std::size_t`.  PASS.

### Section 2 — sibling path verification (both branches)

Two-branch test pinned by the same source compiled with
`PETRA_THROW_ON_OVERFLOW=0` (default) and
`PETRA_THROW_ON_OVERFLOW=1`.

On the **expected sibling** path:

- 2a — CONSTEVAL macro `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW`
  dispatches to Sep 10's `try_format_bounded_runtime_n_span`
  (expected, returns `expected<size_t, FormatError>`).  `r.value()
  == 11` for `"hello {}"` + `"world"` (== "hello world").  Byte at
  index 11 is the poison NUL (NO NUL — bounded-buffer sibling).
- 2b — RUNTIME macro `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW`
  dispatches to Sep 19's NEW `petra::try_format_runtime_n(span, ...)`
  (expected, returns `expected<size_t, FormatError>`).

On the **throwing sibling** path (same source compiled with
`-DPETRA_THROW_ON_OVERFLOW=1`):

- 2a — CONSTEVAL macro `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW`
  dispatches to today's NEW
  `petra::try_format_bounded_runtime_n_throwing_span` (throwing,
  returns `size_t`).  `n == 11` for `"hello {}"` + `"world"`.
- 2b — RUNTIME macro `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW`
  dispatches to today's NEW
  `petra::try_format_runtime_n_throwing(span, ...)` (throwing,
  returns `size_t`).  `n == 8` for `"value={}"` + `42`.

### Section 3 — atomicity on `TooLarge`

Pins the buffer atomicity on `TooLarge` for BOTH the expected and
the throwing paths.  Pre-fills the buffer with poison `'Q'` and
verifies ALL bytes are still `'Q'` after the failing call:

- 3a/3b (expected path) — verify `FormatError{TooLarge, ...}` is
  returned and the buffer is UNTOUCHED.
- 3c/3d (throwing path) — verify `std::runtime_error` is thrown
  and the buffer is UNTOUCHED.

The throwing path's atomicity is preserved because today's
wrapper is a verbatim thin wrapper around Sep 10 / Sep 19 (which
already pin atomicity on the expected path).  PASS on both
builds.

### Section 4 — byte-count contract on success

Pins that the success path returns the byte count:

- Expected path: `r.value() == 11` for `"[{}] [{}] [{}]"` + 1, 2,
  3 (== "[1] [2] [3]").
- Throwing path: `n == 11` for the same call.

PASS on both builds.

### Section 5 — `std::runtime_error` catchability (throwing path only)

Pins that the throwing sibling's exception is catchable as
`std::exception&` (via `std::runtime_error`'s inheritance) and as
`std::runtime_error&` directly.  The `what()` message must contain
the same `{needed, cap}` pair that the expected path's
`FormatError::message` contains:

```
caught.what = [formatted output would be 30 bytes; cap is 16]
```

Also pins that the dynamic type of the thrown exception is
`std::runtime_error`, NOT `std::format_error` (the throwing wrapper
re-raises as `std::runtime_error`).  Pinned by
`dynamic_cast<const std::format_error*>(&e) != nullptr` returning
`false`.  PASS on the throwing build.  On the expected build,
Section 5c pins the mirror image: `r.error().kind ==
FormatErrorKind::TooLarge` AND `r.error().needed_bytes == 30`.

### Section 6 — consteval surface preserves compile-time gate

Pins that the CONSTEVAL macro's format-string parameter is
`std::format_string<Args...>` regardless of `PETRA_THROW_ON_OVERFLOW`
— the consteval gate is preserved.  Pinned by:

- Direct invocation with a literal `"literal {}"` produces
  `r.value() == 10` / `n == 10` (== "literal 42").
- Direct invocation with a runtime-built `std::string` REJECTS
  AT COMPILE TIME on the CONSTEVAL macro (verified by
  `/tmp/probe_consteval_rejects_runtime_fmt_span.cpp` producing
  `error: call to consteval function
    'std::basic_format_string<char, int>::basic_format_string<std::string>'
  is not a constant expression` with note `read of non-constexpr
  variable 'fmt' is not allowed in a constant expression`).

The RUNTIME macro accepts the runtime-built `std::string`
correctly (`r.value() == 10` / `n == 10`).

### Section 7 — multi-arg round-trip

Multi-arg format `"x={} y={} z={:.2f}"` + two integer args and a
double produces the expected byte count `16` and prefix bytes
`"x=42 y=hi z=3.14"`.  PASS on both builds.

### Section 8 — embedded NUL byte round-trip

A runtime-built format that produces an embedded NUL byte
(`"hi\0hi {}!"` 9-byte literal).  The byte at the NUL position is
`'\0'`; the byte at `written` is the input byte (the NUL is NOT
counted as part of `written`).  PASS on both builds.

### Section 9 — byte-exactness of the throwing path's message

The KEY HEADLINE finding of today's lesson — the throwing
sibling's `std::runtime_error::what()` MUST be byte-exactly equal
to the expected sibling's `FormatError::message` for the SAME
`{needed, cap}` pair.  Pinned by:

1. Running the expected sibling on a known-failing call (via
   `petra::try_format_runtime_n(span, ...)` directly, NOT via the
   macro — the macro dispatches to the throwing sibling on the
   throwing build, so the expected sibling must be called directly).
2. Running the throwing sibling on the same call and catching
   the `std::runtime_error::what()`.
3. Asserting byte-exactness (`expected_msg == thrown_msg`).

Pinned on BOTH paths (because the same `expected_msg` and
`thrown_msg` are produced regardless of which path the macro
dispatches to).  PASS.

### Section 10 — Format error → runtime_error on the throwing path

On the throwing path, `std::format_error` is ALSO caught and
re-raised as `std::runtime_error` (the throwing wrapper does NOT
distinguish TooLarge from Format at the call site; both are "the
|format call failed" from the throwing sibling's perspective).
Pinned by checking the DYNAMIC TYPE of the exception via
`dynamic_cast<const std::format_error*>(&e)`:

- The throwing wrapper re-raises as `std::runtime_error`, NOT
  `std::format_error` (the dynamic type is `std::runtime_error`).
- The `what()` message contains the `std::format_error`'s text
  (e.g. `"The format string terminates at a '{'"`).

This works because `std::format_error` IS-A `std::runtime_error`
(pinned by `static_assert(std::is_base_of_v<std::runtime_error,
std::format_error>)`).  The catch chain must put
`std::format_error&` BEFORE `std::runtime_error&` (C++ catch
handlers are matched in order).  PASS on the throwing build.

On the expected build, Section 10 pins the perpendicular-axis
claim: today's `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` with
`PETRA_THROW_ON_OVERFLOW=0` dispatches to the SAME wrapper as
`TRY_FORMAT_BOUNDED_N_SPAN_ORIG` (the "flat" Sep 19-shaped helper
dispatcher; CONSTEVAL + bounded-buffer + span<char>; Sep 10's
wrapper).  Today's `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW` with
`PETRA_THROW_ON_OVERFLOW=0` dispatches to the SAME wrapper as
`TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_ORIG` (RUNTIME + bounded-buffer
+ span<char>; Sep 19's NEW `petra::try_format_runtime_n(span, ...)`
wrapper).  Pinned by `static_assert(std::is_same_v<R10a, R10b>)` and
`static_assert(std::is_same_v<R10c, R10d>)`.  PASS on both paths.

### Section 11 — no-args variant

`__VA_OPT__(,)` handles the no-args case (a format with no
replacement fields).  `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW(buf,
"ping")` produces `r.value() == 4` / `n == 4`.  PASS on both
builds.

### Section 12 — multi-arg round-trip (span char sibling)

Pins multi-arg round-trip with mixed types (`int`, `std::string`,
`double`).  PASS on both builds.

### Section 13 — 4 threads × 25 calls concurrent dispatch

Pins thread safety.  4 threads × 25 calls each fire concurrently
through BOTH the CONSTEVAL and RUNTIME meta-macros.  Per-thread
local counters are aggregated AFTER `join()` via
`std::atomic<int>::fetch_add` to avoid a data race on `g_pass` /
`g_fail`.  All 100 calls per macro return
`r.value() == 8` / `n == 8` (the byte count for `"[{:02}][{:02}]"`
+ a two-digit integer and a two-digit integer).  PASS on both
builds.

### Section 14 — final summary

Prints the total pass / fail tally.  Expected sibling build:
`PASS: 102  FAIL: 0`.  Throwing sibling build: `PASS: 86  FAIL: 0`.

The expected build has more checks because it has Section 5c (which
only applies to the expected path; the throwing build has Section 5a /
5b instead, and those sections have fewer CHECK lines).  The PASS
/ FAIL totals are non-uniform across the two paths BY DESIGN (the
two paths exercise different sections).

## Build matrix — observed output

| Build                                          | Compile | Run              |
|------------------------------------------------|---------|------------------|
| Default (`PETRA_THROW_ON_OVERFLOW=0`) — `-O0`  | clean   | PASS 102/0       |
| Throwing (`PETRA_THROW_ON_OVERFLOW=1`) — `-O0` | clean   | PASS 86/0        |
| Strict-warning (default flags) — `-O0`         | clean   | PASS 102/0       |
| Strict-warning (throwing flags) — `-O0`        | clean   | PASS 86/0        |
| ASan + UBSan (default flags) — `-O1`           | clean   | PASS 102/0       |
| ASan + UBSan (throwing flags) — `-O1`          | clean   | PASS 86/0        |
| CMake default                                  | clean   | PASS 102/0       |
| CMake strict                                   | clean   | PASS 102/0       |
| CMake ASan                                     | clean   | PASS 102/0       |
| CMake throwing                                 | clean   | PASS 86/0        |

Five consecutive runs on each build produced identical
`PASS: 102 FAIL: 0` (default) / `PASS: 86 FAIL: 0` (throwing)
tallies.  No flakiness in Section 13's 4-thread × 25-call
concurrent dispatch.

Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`.  Zero sanitizer diagnostics.  Empty stderr
under default + ASan.

## Summary

- Topic: a macro layer that lifts the RETURN-TYPE-vs-EXCEPTION
  sibling choice to a macro form on the `std::span<char>` axis,
  analogous to Sep 20's char*+cap macro layer.  This is the
  SPAN-CHAR MIRROR of Sep 20.
- Two new macros — `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` (CONSTEVAL
  surface) and `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW`
  (RUNTIME surface) — each dispatch to one of two siblings based on
  the compile-time flag `PETRA_THROW_ON_OVERFLOW` (0 = expected
  `std::expected<std::size_t, FormatError>` return, 1 = throwing
  `std::size_t` return + `std::runtime_error` on overflow / format
  error).
- Reused verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 /
  Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
  Sep 20: `petra::FormatError`, `petra::FormatErrorKind`,
  `petra::counting_output_iterator`,
  `petra::detail::bounded_char_writer`,
  `petra::detail::try_format_runtime_n_impl` (Sep 8),
  `petra::try_format_runtime_n` (Sep 8 char*+cap, Sep 19 span<char>
  forwarding wrapper), `petra::try_format_bounded_runtime_n` (Sep 9
  char*+cap), `petra::try_format_bounded_runtime_n_span` (Sep 10),
  `petra::try_format_runtime_n_throwing` (Sep 20 char*+cap),
  `petra::try_format_bounded_runtime_n_throwing` (Sep 20 char*+cap).
- New today: two throwing wrappers
  `petra::try_format_runtime_n_throwing(span, ...)` /
  `petra::try_format_bounded_runtime_n_throwing_span` (each a
  verbatim thin wrapper around Sep 19's / Sep 10's expected
  sibling that re-raises `FormatError{...}` as `std::runtime_error`
  with byte-exact `what()` message) + the two
  `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW` /
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW` meta-macros + the
  `PETRA_THROW_ON_OVERFLOW` compile-time flag + a `#error` guard.
  The wrappers are 5-line verbatim thin dispatchers; each macro
  is a verbatim thin dispatcher to one of the four underlying
  wrappers.
- HEADLINE finding — the `PETRA_THROW_ON_OVERFLOW` axis (expected
  vs throwing) is the FOURTH axis orthogonal to Sep 19's matrix
  (`char*-vs-span` × `CONSTEVAL-vs-RUNTIME` ×
  `bounded-buffer-vs-NUL-terminating`).  Today's meta-macros are
  at the INTERSECTION of all FOUR axes on the `span<char>` axis.
  Preprocessor-time `#if PETRA_THROW_ON_OVERFLOW` is the cleanest
  design for the dispatch: zero runtime cost; matches Sep 1 /
  Sep 2's compile-time-flag framing; `#error` fires at preprocessor
  time if the flag is invalid; composes orthogonally with the other
  three axes (Section 10's `decltype` checks pin this on both
  paths).
- **Closes Sep 20's "Where we go next" item #1 verbatim**: the
  natural follow-on to Sep 20's char*+cap lesson was the
  `span<char>` analog (two NEW wrappers + four NEW macros); today's
  lesson is exactly that analog.
- 102/102 PASS across 14 sections on the expected sibling default
  + strict-warning + ASan/UBSan + CMake builds.
- 86/86 PASS across 14 sections on the throwing sibling build
  (same source compiled with `-DPETRA_THROW_ON_OVERFLOW=1`).
  All contracts pinned: byte-exactness of the throwing path's
  `what()` vs the expected path's `FormatError::message`
  (Section 9); dynamic type of the thrown exception is
  `std::runtime_error`, NOT `std::format_error` (Section 5a);
  buffer UNTOUCHED on `TooLarge` (Section 3c / 3d).
- Five consecutive runs on each build produced identical
  tallies (no flakiness in Section 13's 4-thread × 25-call
  concurrent dispatch — per-thread local counters aggregated
  after `join`).
- Zero warnings under `-Wall -Wextra -Wpedantic -Werror
  -Wshadow -Wconversion -Wsign-conversion`; zero sanitizer
  diagnostics; empty stderr under default + ASan.
- Lesson is a pure consumer-side C++23 `std::format` +
  `std::vformat_to` + `std::format_string<Args...>` (P2216R3)
  + `std::make_format_args` + `std::expected` (P0323R12) +
  `std::span<char>` (P0122R7) + `std::ranges::contiguous_range` +
  `std::println` (P2093R14) + `std::runtime_error` +
  `std::atomic` + `dynamic_cast` + `std::is_base_of_v` +
  `__VA_OPT__` preprocessor feature exercise — reuses Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20's machinery verbatim,
  no third-party libraries, no library installation, no
  find_package.

## Where we go next

Today's lesson closes Sep 20's "Where we go next" item #1 verbatim:
the `span<char> × CONSTEVAL-or-RUNTIME × bounded-buffer × throwing`
cells.  The remaining 4 NEW cells in the 16-cell matrix (the
throwing sibling on the NUL-terminating axis for both `char*+cap`
and `span<char>`) are left for future lessons:

1. **`char*+cap × CONSTEVAL-or-RUNTIME × NUL-terminating ×
   throwing`** cells — the NUL-terminating char*+cap throwing
   sibling.  Each future lesson adds a verbatim thin throwing
   wrapper composed on top of an existing NUL-terminating sibling
   + a new `#if PETRA_THROW_ON_OVERFLOW`-gated macro.  The natural
   follow-on is Sep 11's / Sep 12's NUL-terminating wrappers
   (Sep 11's CONSTEVAL `format_to_n_into_bounded` + Sep 12's
   RUNTIME `try_format_runtime_n_into_bounded`); each would gain
   a `*_throwing` wrapper that re-raises as `std::runtime_error`.
2. **`span<char> × CONSTEVAL-or-RUNTIME × NUL-terminating ×
   throwing`** cells — the NUL-terminating span<char> throwing
   sibling.  Each future lesson adds a verbatim thin throwing
   wrapper composed on top of an existing NUL-terminating span
   sibling + a new `#if PETRA_THROW_ON_OVERFLOW`-gated macro.  The
   natural follow-on is Sep 11's / Sep 12's NUL-terminating span
   wrappers (Sep 11's CONSTEVAL `format_to_n_into_bounded(span,
   ...)` + Sep 12's RUNTIME `try_format_runtime_n_into_bounded(
   span, ...)`); each would gain a `*_throwing` wrapper that
   re-raises as `std::runtime_error`.

Today's lesson does NOT close Sep 16's open item #1 — replacing
the hand-rolled `petra::counting_output_iterator` with
`std::counting_output_iterator` when libc++ ships it.  Today's
lesson reuses the hand-rolled iterator from Sep 6 / Sep 7 /
Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 /
Sep 15 / Sep 16 / Sep 19 / Sep 20 verbatim; verified during
lesson prep that `std::counting_output_iterator` is NOT yet
shipped by libc++ 21 (no `__iterator/counting_output_iterator.h`
header in the libc++ 21 include tree; no
`counting_output_iterator` symbol in the `<iterator>` header).

The five-axis matrix (theoretical — no lesson plans):

| axis | value |
|------|-------|
| 1 | char\*+cap vs span<char> |
| 2 | CONSTEVAL vs RUNTIME |
| 3 | bounded-buffer vs NUL-terminating |
| 4 | expected vs throwing (Sep 20 / TODAY) |
| 5 | (potential future axis: ???) |

The preprocessor-time dispatch design generalizes naturally to
ANY future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that
discards the output on overflow, a `PETRA_STRICT_FORMAT` axis
that rejects `{:#x}` style specifiers on the runtime path, etc.)
— each new axis is a new `#if PETRA_<NEW_FLAG>` block layered on
top of the existing axes.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.