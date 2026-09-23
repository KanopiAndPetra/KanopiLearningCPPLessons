# P-2026-09-23 — `try_format_span_n_or_throw_into`

Topic: a **META-MACRO LAYER** that lifts the **NUL-TERMINATING ×
RETURN-TYPE-vs-EXCEPTION** sibling choice to a meta-macro form on the
**STD::SPAN<CHAR>** axis.  Two new meta-macros —
`TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO` (CONSTEVAL surface) and
`TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO` (RUNTIME surface) —
each dispatch to EITHER the **expected** sibling (Sep 11's
`format_to_n_into_bounded(span, ...)` for the CONSTEVAL surface, Sep
12's `try_format_runtime_n_into_bounded(span, ...)` for the RUNTIME
surface; both return `std::expected<std::size_t, FormatError>` and
return `FormatError{TooLarge, ..., needed_bytes}` on overflow) OR
today's **NEW throwing sibling** (today's
`try_format_to_n_into_bounded_throwing_span` for the CONSTEVAL
surface, today's `try_format_runtime_n_into_bounded_throwing_span`
for the RUNTIME surface; both return `std::size_t` and throw
`std::runtime_error` on overflow or format error), based on a
caller-controlled compile-time flag `PETRA_THROW_ON_OVERFLOW`.

**Closes Sep 22's "Where we go next" item #1 verbatim**:

> 1. `span<char> × CONSTEVAL-or-RUNTIME × NUL-terminating ×
>    throwing` cells — the NUL-terminating span<char> throwing
>    sibling.  Each future lesson adds a verbatim thin throwing
>    wrapper composed on top of an existing NUL-terminating span
>    sibling + a new `#if PETRA_THROW_ON_OVERFLOW`-gated macro.
>    The natural follow-on is Sep 11's / Sep 12's NUL-terminating
>    span wrappers (Sep 11's CONSTEVAL `format_to_n_into_bounded(
>    span, ...)` + Sep 12's RUNTIME
>    `try_format_runtime_n_into_bounded(span, ...)`); each would
>    gain a `*_throwing` wrapper that re-raises as
>    `std::runtime_error`.

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
           char\*+cap (plus `std::span<char>` overload).
- Sep 12   `try_format_runtime_n_into_bounded` — RUNTIME
           NUL-terminating char\*+cap (plus `std::span<char>`
           overload).
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
           axis (the FOURTH axis; covers the char\*+cap ×
           bounded-buffer × expected-vs-throwing cells).
- Sep 21   `try_format_span_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the
           `std::span<char>` axis (the FOURTH axis on the
           span<char> side; covers the span<char> × bounded-buffer
           × expected-vs-throwing cells).
- Sep 22   `try_format_bounded_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the CHAR\*+CAP axis (covers the
           char\*+cap × NUL-terminating × expected-vs-throwing
           cells — closes Sep 21's "Where we go next" item #1
           verbatim).
- TODAY    `try_format_span_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the STD::SPAN<CHAR> axis (covers
           the span<char> × NUL-terminating × expected-vs-throwing
           cells — closes Sep 22's "Where we go next" item #1
           verbatim; together with Sep 22's char\*+cap lesson,
           the FOUR-AXIS BOUNDED-vs-NUL × expected-vs-throwing
           quadrant of the 16-cell matrix is now COMPLETE on BOTH
           the char\*+cap and span<char> axes).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21 — Sep 16's open item #1 documents this).
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_to_n_into_bounded_impl` — REUSED Sep 11
  verbatim (CONSTEVAL NUL-terminating impl).
- `petra::detail::try_format_runtime_n_into_bounded_impl` — REUSED
  Sep 12 verbatim (RUNTIME NUL-terminating impl).
- `petra::format_to_n_into_bounded(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 11 verbatim
  (CONSTEVAL NUL-terminating span sibling).
- `petra::try_format_runtime_n_into_bounded(std::span<char>,
  std::string_view, args...)` — REUSED Sep 12 verbatim (RUNTIME
  NUL-terminating span sibling).
- `petra::try_format_to_n_into_bounded_throwing_span(std::span<char>,
  std::format_string<Args...>, args...)` — **NEW today**: CONSTEVAL
  surface of the NUL-terminating × THROWING span<char> sibling;
  verbatim thin wrapper around Sep 11's
  `format_to_n_into_bounded(span, ...)` that re-raises
  `FormatError{...}` as `std::runtime_error` with the SAME
  `FormatError::message` (byte-exact).
- `petra::try_format_runtime_n_into_bounded_throwing_span(std::span<char>,
  std::string_view, args...)` — **NEW today**: RUNTIME surface of
  the NUL-terminating × THROWING span<char> sibling; verbatim thin
  wrapper around Sep 12's `try_format_runtime_n_into_bounded(span,
  ...)` that re-raises as `std::runtime_error` with byte-exact
  message.
- The TWO new meta-macros `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO`
  and `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO` — NEW today;
  each is a verbatim thin dispatcher that picks between two siblings
  at preprocessor time based on `PETRA_THROW_ON_OVERFLOW`.

## What today's lesson pins

1. **The NUL-TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling choice
   CAN be lifted to a meta-macro form on the span<char> axis.**
   The shape composes on top of Sep 11 / Sep 12's NUL-terminating
   span wrappers (the underlying primary wrappers) AND on top of
   Sep 21's expected-vs-throwing macro layer (the underlying
   dispatcher meta-macros — bounded-buffer variants).  Each
   TODAY's meta-macro is a verbatim thin wrapper around ONE OF
   TWO underlying wrappers:
   - NUL-terminating × expected:        Sep 11 / Sep 12 span overloads
   - NUL-terminating × throwing:        TODAY's NEW span throwing wrappers
   The bounded-buffer variants are NOT covered by today's macros
   (those are Sep 21's domain).
2. **The compile-time flag `PETRA_THROW_ON_OVERFLOW` controls the
   dispatch** via `#if PETRA_THROW_ON_OVERFLOW == 0/1`:
   - `#define PETRA_THROW_ON_OVERFLOW 0` → expected sibling
     (`std::expected<std::size_t, FormatError>` return type):
     - CONSTEVAL: `petra::format_to_n_into_bounded(span, ...)`
                  (Sep 11)
     - RUNTIME:   `petra::try_format_runtime_n_into_bounded(span, ...)`
                  (Sep 12)
   - `#define PETRA_THROW_ON_OVERFLOW 1` → throwing sibling
     (`std::size_t` return type; throws `std::runtime_error` on
     overflow or format error):
     - CONSTEVAL: `petra::try_format_to_n_into_bounded_throwing_span`
                  (NEW today; wraps Sep 11 verbatim)
     - RUNTIME:   `petra::try_format_runtime_n_into_bounded_throwing_span`
                  (NEW today; wraps Sep 12 verbatim)
3. **`#error` guard** fires at preprocessor time if the flag is
   set to anything other than 0 or 1.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO(...)` and
   capture the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16
   / Sep 19 / Sep 20 / Sep 21 / Sep 22's macro shape.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22.
6. **The CONSTEVAL macro picks between Sep 11's
   `format_to_n_into_bounded(span, ...)` (expected, returns
   `expected<size_t, FormatError>`) and today's NEW
   `try_format_to_n_into_bounded_throwing_span` (throwing, returns
   `size_t`).**  Both are consteval-checked
   `std::format_string<Args...>` wrappers.
7. **The RUNTIME macro picks between Sep 12's
   `try_format_runtime_n_into_bounded(span, ...)` (expected,
   returns `expected<size_t, FormatError>`) and today's NEW
   `try_format_runtime_n_into_bounded_throwing_span` (throwing,
   returns `size_t`).**  Both take `std::string_view` format-string
   (no compile-time parse).
8. **The macro's contract is byte-exact: the throwing sibling's
   `std::runtime_error::what()` is byte-exactly equal to the
   expected sibling's `FormatError::message` for the SAME
   `{needed, cap, effective}` triple.**  Pinned by Section 9 by
   calling both siblings with the SAME call args (using a
   `std::span<char>` over the same backing array) and comparing
   `expected_msg` (from `r.error().message` on the direct Sep 11 /
   Sep 12 span-wrapper call) to `thrown_msg` (from `e.what()` on
   the throwing sibling path).  The NUL-terminating impl carries
   an additional `"(effective writable is N)"` suffix in the
   `FormatError::message`; the throwing wrapper preserves this
   byte-exactly.
9. **The throwing sibling preserves the SAME buffer atomicity on
   TooLarge as the expected sibling** — the buffer is UNTOUCHED on
   overflow (Sep 11 / Sep 12 already pin this on the expected
   path; today's wrappers re-pin it on the throwing path because
   the new code path COULD theoretically diverge; Section 3c /
   3d pin this on the throwing path).
10. **The throwing sibling preserves the SAME NUL-placement
    contract on success** — NUL at `nul_idx = (written < cap) ?
    written : (cap - 1)`; Section 5 pins this on both paths (the
    truncation-edge case needed == cap returns effective_cap = cap - 1
    and puts NUL at cap - 1, byte-exactly as the expected sibling).
11. **The throwing sibling preserves the SAME byte-count contract
    on success** — returns the byte count written (the same value
    Sep 11's / Sep 12's expected siblings return); Section 4 pins
    this on both paths.
12. **The CONSTEVAL macro's consteval gate is preserved regardless
    of `PETRA_THROW_ON_OVERFLOW`** —
    `TRY_FORMAT_BOUNDED_SPAN_N_OR_THROW_INTO` still requires
    `std::format_string<Args...>` (a literal fmt at the call site).
    Section 6 pins the consteval gate by calling the macro with a
    literal fmt and a runtime-built `std::string` fmt; the
    runtime-built fmt REJECTS AT COMPILE TIME on the CONSTEVAL
    macro (verified by
    `/tmp/probe_consteval_rejects_runtime_fmt_span_into.cpp`
    producing `error: call to consteval function
    std::basic_format_string<char, int>::basic_format_string<std::string>
    is not a constant expression` with note
    `read of non-constexpr variable fmt_runtime is not allowed in
    a constant expression` / `in call to this->data()`).
13. **The `PETRA_THROW_ON_OVERFLOW` axis (expected vs throwing)
    is PERPENDICULAR to the `CONSTEVAL-vs-RUNTIME` axis** (Sep
    13's macro layer), AND to the `bounded-buffer-vs-NUL-
    terminating` axis (Sep 16's / Sep 19's macro layer).
    Today's meta-macros are at the INTERSECTION of
    `CONSTEVAL-or-RUNTIME` × `NUL-terminating` ×
    `expected-vs-throwing` on the STD::SPAN<CHAR> axis.
    Section 14 pins this via `decltype` checks on BOTH paths.
14. **`std::format_error` IS-A `std::runtime_error`** — pinned by
    `static_assert(std::is_base_of_v<std::runtime_error,
    std::format_error>)` succeeding.  This means the catch site
    must put `std::format_error&` BEFORE `std::runtime_error&` in
    the catch chain (C++ catch handlers are matched in order).
    Section 10 exploits this by checking the dynamic type via
    `dynamic_cast<const std::format_error*>(&e)` — the throwing
    wrapper re-raises as `std::runtime_error`, NOT
    `std::format_error`, so the dynamic cast returns `nullptr`.
    The throwing wrapper's caught `e.what()` on the RUNTIME
    surface (the `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_THROW_INTO`
    macro) was `"The format string terminates at a '{'"` for the
    test case `std::string_view{"bad {"}, 42` (verified by Section
    10's `std::println` output — see "Observed output" below).
15. **The `std::span<char>` sub-span 'no-bleed' contract is
    preserved on the throwing path** (Section 7) — a
    `std::span<char>` over a sub-range of a buffer must NOT touch
    bytes outside the span (mirrors Sep 10 / Sep 12 / Sep 19 /
    Sep 21's no-bleed contract; a 6-byte sub-span at offset 5 of
    a 16-byte buffer with format `"ABCDE"` leaves bytes [0,5) and
    [11,16) as poison `'Q'`).
16. **The `std::span<char>` properties are pinned in Section 1d**:
    `std::ranges::contiguous_range` +
    `std::ranges::sized_range` + `trivially_copyable` +
    `standard_layout` + `.data()->char*` + `.size()->std::size_t`.
17. **Empty `std::span<char>` behavior is pinned in Section 12**:
    an empty span with an empty fmt produces success (returns 0);
    an empty span with a non-empty fmt throws (or returns
    TooLarge, depending on the `PETRA_THROW_ON_OVERFLOW` axis).
    On the throwing path, the sentinel byte outside the empty
    span remains untouched (atomicity preserved).
18. **Concurrency is safe** — Section 13 fires 4 threads × 25
    concurrent calls through both macros; per-thread local
    counters are aggregated AFTER join() via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.  Five consecutive runs on both the
    default and throwing builds produced identical pass/fail
    tallies (no flakiness).

## Headline finding — the `PETRA_THROW_ON_OVERFLOW` axis is the
FOURTH axis on the span<char> side too

Together with Sep 22's char\*+cap lesson, today's lesson closes the
**FOURTH axis** (`expected vs throwing`) on the **NUL-terminating**
side of the matrix on **BOTH** the char\*+cap and span<char> axes.
The matrix is now COMPLETE on the BOUNDED-vs-NUL ×
expected-vs-throwing quadrant on both axes:

| axis | value |
|------|-------|
| 1 | char\*+cap vs span<char> |
| 2 | CONSTEVAL vs RUNTIME |
| 3 | bounded-buffer vs NUL-terminating |
| 4 | expected vs throwing (Sep 20 / Sep 21 / Sep 22 / TODAY) |

Each cell on the BOUNDED-vs-NUL × expected-vs-throwing quadrant is
now covered by Sep 19 / Sep 21 (bounded-buffer sibling-choice
macro layer on the span<char> axis) and Sep 16 / Sep 20 (same on
the char\*+cap axis), plus Sep 22 (NUL-terminating × expected-vs-
throwing meta-macro layer on char\*+cap) and TODAY (same on
span<char>).

The preprocessor-time dispatch design generalizes naturally to ANY
future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that discards the
output on overflow, a `PETRA_STRICT_FORMAT` axis that rejects
`{:#x}` style specifiers on the runtime path, etc.) — each new
axis is a new `#if PETRA_<NEW_FLAG>` block layered on top of the
existing axes.

## Reuses (verbatim from prior lessons)

- `petra::FormatError` / `petra::FormatErrorKind` / `petra::counting_output_iterator`
  / `petra::detail::bounded_char_writer` (Sep 6–Sep 22 verbatim).
- `petra::detail::try_format_to_n_into_bounded_impl` (Sep 11 verbatim) +
  `petra::detail::try_format_runtime_n_into_bounded_impl` (Sep 12 verbatim).
- `petra::format_to_n_into_bounded(char*, std::size_t, std::format_string<Args...>,
  args...)` (Sep 11 verbatim) + `petra::format_to_n_into_bounded(std::span<char>,
  std::format_string<Args...>, args...)` (Sep 11 verbatim).
- `petra::try_format_runtime_n_into_bounded(char*, std::size_t, std::string_view,
  args...)` (Sep 12 verbatim) + `petra::try_format_runtime_n_into_bounded(std::span<char>,
  std::string_view, args...)` (Sep 12 verbatim).
- `petra::try_format_runtime_n_throwing(char*, std::size_t, std::string_view,
  args...)` (Sep 20 verbatim) + `petra::try_format_bounded_runtime_n_throwing(char*,
  std::size_t, std::format_string<Args...>, args...)` (Sep 20 verbatim).
- `petra::try_format_to_n_into_bounded_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` (Sep 22 verbatim) +
  `petra::try_format_runtime_n_into_bounded_throwing(char*, std::size_t,
  std::string_view, args...)` (Sep 22 verbatim).

No new types beyond the two NEW span<char> throwing wrappers; no
new impls beyond today's two NEW thin span<char> wrappers; no new
error contracts.

## Build and verification commands

```bash
# Default build (expected sibling; PETRA_THROW_ON_OVERFLOW=0).
cmake -S . -B build
cmake --build build
./build/P-2026-09-23-try-format-span-n-or-throw-into
# → PASS: 136  FAIL: 0

# Strict-warning build (-Wall -Wextra -Wpedantic -Werror -Wshadow
# -Wconversion -Wsign-conversion).
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-23-try-format-span-n-or-throw-into
# → PASS: 136  FAIL: 0

# ASan + UBSan build.
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-23-try-format-span-n-or-throw-into
# → PASS: 136  FAIL: 0

# Throwing path build (PETRA_THROW_ON_OVERFLOW=1).
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
cmake --build build-throw
./build-throw/P-2026-09-23-try-format-span-n-or-throw-into
# → PASS: 120  FAIL: 0

# Direct compile (no CMake).
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-23-try-format-span-n-or-throw-into.cpp \
        -o /tmp/P-2026-09-23-try-format-span-n-or-throw-into
/tmp/P-2026-09-23-try-format-span-n-or-throw-into
# → PASS: 136  FAIL: 0
```

## Observed output (default build, expected sibling path)

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0 (expected sibling)
=========================================================
PASS: 136  FAIL: 0
=========================================================
OVERALL: PASS
```

## Observed output (throwing build, throwing sibling path)

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1 (throwing sibling)
[Section 10] caught.what = [The format string terminates at a '{']
=========================================================
PASS: 120  FAIL: 0
=========================================================
OVERALL: PASS
```

The Section 10 line is the throwing wrapper's `e.what()` from the
`dynamic_cast` test — the malformed fmt string `"bad {"` is
rejected at format time by `std::format_error` ("The format string
terminates at a '{'"), and the throwing wrapper re-raises the
message as `std::runtime_error` with byte-exact preservation.  The
`dynamic_cast<const std::format_error*>(&e)` returns `nullptr`,
confirming the dynamic type is `std::runtime_error` (NOT
`std::format_error`).

## Build matrix

- default build: `clang++ -std=c++23 -stdlib=libc++
  -fexperimental-library -O0`
- strict-warning build: default + `-Wall -Wextra -Wpedantic -Werror
  -Wshadow -Wconversion -Wsign-conversion`
- ASan/UBSan build: default + `-fsanitize=address
  -fsanitize=undefined -fno-omit-frame-pointer -O1`
- throwing path build: default + `-DPETRA_THROW_ON_OVERFLOW=1`

## Concurrency verification

Section 13 fires 4 threads × 25 calls = 100 concurrent calls
through both macros on each build.  Per-thread local `lp`/`lf`
counters are aggregated AFTER join() via
`std::atomic<int>::fetch_add` to avoid a data race on
`g_pass`/`g_fail`.  Five consecutive runs on each build produced
identical pass/fail tallies (no flakiness):

- default build: `PASS: 136  FAIL: 0` (5 of 5 runs)
- throwing build: `PASS: 120  FAIL: 0` (5 of 5 runs)

## Consteval-gate probe

`/tmp/probe_consteval_rejects_runtime_fmt_span_into.cpp` produces:

```
error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<std::string>' is not a constant expression
note: read of non-constexpr variable 'fmt_runtime' is not allowed in a constant expression
note: in call to 'this->data()'
```

— confirming that the CONSTEVAL macro's consteval gate is preserved
on the span<char> axis regardless of PETRA_THROW_ON_OVERFLOW
(matches Sep 10 / Sep 12 / Sep 19 / Sep 21 / Sep 22's behavior).

## Where we go next

Today's lesson closes Sep 22's "Where we go next" item #1 verbatim:
the `span<char> × CONSTEVAL-or-RUNTIME × NUL-terminating ×
throwing` cells (two NEW wrappers + two NEW macros).  Together with
Sep 22's char\*+cap lesson, the FOUR-AXIS BOUNDED-vs-NUL ×
expected-vs-throwing quadrant of the 16-cell matrix is now
COMPLETE on BOTH the char\*+cap and span<char> axes.  All 16
cells of the FOUR-AXIS matrix are now covered by the existing
lessons:

| axis 1 (buffer) | axis 2 (parse) | axis 3 (output) | axis 4 (error) | covered by |
|---|---|---|---|---|
| char\*+cap     | CONSTEVAL    | bounded-buffer | expected  | Sep  7 |
| char\*+cap     | CONSTEVAL    | bounded-buffer | throwing  | Sep 20 |
| char\*+cap     | CONSTEVAL    | NUL-terminating| expected  | Sep 11 |
| char\*+cap     | CONSTEVAL    | NUL-terminating| throwing  | Sep 22 |
| char\*+cap     | RUNTIME      | bounded-buffer | expected  | Sep  8 |
| char\*+cap     | RUNTIME      | bounded-buffer | throwing  | Sep 20 |
| char\*+cap     | RUNTIME      | NUL-terminating| expected  | Sep 12 |
| char\*+cap     | RUNTIME      | NUL-terminating| throwing  | Sep 22 |
| span<char>     | CONSTEVAL    | bounded-buffer | expected  | Sep 10 |
| span<char>     | CONSTEVAL    | bounded-buffer | throwing  | Sep 21 |
| span<char>     | CONSTEVAL    | NUL-terminating| expected  | Sep 11 |
| span<char>     | CONSTEVAL    | NUL-terminating| throwing  | TODAY |
| span<char>     | RUNTIME      | bounded-buffer | expected  | Sep 19 |
| span<char>     | RUNTIME      | bounded-buffer | throwing  | Sep 21 |
| span<char>     | RUNTIME      | NUL-terminating| expected  | Sep 12 |
| span<char>     | RUNTIME      | NUL-terminating| throwing  | TODAY |

Today's lesson does NOT close Sep 16's open item #1 — replacing
the hand-rolled `petra::counting_output_iterator` with
`std::counting_output_iterator` when libc++ ships it.  Today's
lesson reuses the hand-rolled iterator from Sep 6 / Sep 7 /
Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 /
Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 verbatim;
verified during lesson prep that `std::counting_output_iterator`
is NOT yet shipped by libc++ 21 (no
`__iterator/counting_output_iterator.h` header in the libc++ 21
include tree; no `counting_output_iterator` symbol in the
`<iterator>` header).

The preprocessor-time dispatch design generalizes naturally to
ANY future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that
discards the output on overflow, a `PETRA_STRICT_FORMAT` axis
that rejects `{:#x}` style specifiers on the runtime path, etc.)
— each new axis is a new `#if PETRA_<NEW_FLAG>` block layered on
top of the existing axes.  With the 16-cell matrix now COMPLETE,
future lessons can either (a) introduce a NEW axis (a 32-cell or
64-cell matrix), or (b) close cross-cutting infrastructure items
from the Aug 13 / Aug 15 / Aug 17 lessons (pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status badge
in README; vcpkg / Conan port for `psp_span_lib`; branch
protection requiring linear history).

## C++23 features exercised

- `std::format` + `std::vformat_to` + `std::format_string<Args...>`
  (P2216R3) + `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::span<char>` (P0122R7) + `std::ranges::contiguous_range` +
  `std::ranges::sized_range` + `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v`.
- `std::println` (P2093R14).
- `std::runtime_error` + `std::format_error` + `std::is_base_of_v` +
  `dynamic_cast`.
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.  Reuses Sep 6 / Sep 7 / Sep 8 /
Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22's machinery verbatim.