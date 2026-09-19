# P-2026-09-19 — `try_format_span_n_or_into`

Topic: a **macro layer** that lifts the **BOUNDED-BUFFER vs
NUL-TERMINATING** sibling choice to a macro form on the
**STD::SPAN<CHAR> axis**.  Two new macros —
`TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO` (CONSTEVAL surface) and
`TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO` (RUNTIME surface) — each
dispatch to EITHER the bounded-buffer span sibling (Sep 10's
`try_format_bounded_runtime_n_span` for the CONSTEVAL surface, today's
NEW `try_format_runtime_n(span, ...)` forwarding wrapper for the
RUNTIME surface) OR the NUL-terminating span sibling (Sep 11's
`format_to_n_into_bounded(span, ...)` for the CONSTEVAL surface,
Sep 12's `try_format_runtime_n_into_bounded(span, ...)` for the
RUNTIME surface), based on a caller-controlled compile-time flag
`PETRA_NUL_TERMINATE`.

Closes Sep 16's "Where we go next" item #2 verbatim:

> 2. `std::span<const char>` macro layer — analogous to
>    Sep 14's `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN` /
>    `TRY_FORMAT_BOUNDED_N_SPAN_FMT` / `TRY_FORMAT_BOUNDED_INTO_SPAN`
>    and Sep 15's `TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED` /
>    `_N` / `_INTO` / `_N_SPAN`: a `PETRA_NUL_TERMINATE`-gated
>    pair of meta-macros for the `std::span<char>` /
>    `std::span<const char>` axes.  Reuses the same
>    preprocessor-time `#if PETRA_NUL_TERMINATE` pattern as
>    today.

Sep 16's item #2 framed the meta-macro as analogous to Sep 14's /
Sep 15's macro layers but gated on `PETRA_NUL_TERMINATE` for the
BOUNDED-BUFFER vs NUL-TERMINATING sibling choice.  Today's lesson
IS that meta-macro layer for the `std::span<char>` axis.

## Where this fits in the arc

- Aug 25   `std::format_to_n_into` — NUL-terminating bounded
           formatter WITHOUT a cap parameter (writes a complete
           `std::string`).
- Sep  6   `try_format_runtime_bounded` — `std::string_view` fmt +
           `std::size_t` cap (RUNTIME `std::string`-output wrapper).
- Sep  7   `try_format_bounded` — CONSTEVAL sibling of Sep 6.
- Sep  8   `try_format_runtime_n` — BOUNDED-BUFFER sibling of
           Sep 7 (char* + `std::size_t` cap + `std::string_view`
           fmt, NO NUL).
- Sep  9   `try_format_bounded_runtime_n` — CONSTEVAL char*+cap
           (NO NUL).
- Sep 10   `try_format_bounded_runtime_n_span` — CONSTEVAL
           `std::span<char>` char*+cap (NO NUL).
- Sep 11   `format_to_n_into_bounded` — CONSTEVAL NUL-terminating
           char*+cap (plus `std::span<char>` forwarding overload).
- Sep 12   `try_format_runtime_n_into_bounded` — RUNTIME
           NUL-terminating char*+cap (plus `std::span<char>`
           forwarding overload).
- Sep 13   `try_format_bounded_macros` — eight `TRY_FORMAT_BOUNDED_*`
           macros (CONSTEVAL vs RUNTIME sibling choice at the
           call site).
- Sep 14   `try_format_span_format_string` — `std::span<const char>`
           overloads for the runtime try_format_* family +
           four new macros.
- Sep 15   `try_format_consteval_span_format_string` — pins that
           the CONSTEVAL surface is incompatible with
           `std::span<const char>`; adds a CONSTEVAL-SHAPED
           RUNTIME PARSER family of four wrappers + four macros.
- Sep 16   `try_format_bounded_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer (char*+cap
           axis).
- TODAY    `try_format_span_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer on the
           `std::span<char>` axis.

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim.
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_n_impl` — REUSED Sep 8 verbatim
  (the bounded-buffer sibling impl).
- `petra::detail::try_format_to_n_into_bounded_impl` — REUSED Sep 12
  verbatim (the NUL-terminating sibling impl).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` — REUSED Sep 8 verbatim (char*+cap bounded-buffer).
- `petra::try_format_runtime_n(std::span<char>, std::string_view,
  args...)` — **NEW today**: verbatim thin forwarding wrapper to
  Sep 8's char*+cap impl; closes Sep 16's open item #3.
- `petra::try_format_bounded_runtime_n_span` — REUSED Sep 10 verbatim
  (CONSTEVAL span<char> bounded-buffer wrapper).
- `petra::format_to_n_into_bounded(span, ...)` — REUSED Sep 11 verbatim
  (CONSTEVAL span<char> NUL-terminating forwarding wrapper).
- `petra::try_format_runtime_n_into_bounded(span, ...)` — REUSED
  Sep 12 verbatim (RUNTIME span<char> NUL-terminating forwarding
  wrapper).
- The TWO new macros `TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO` and
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO` — NEW today; each is
  a verbatim thin dispatcher that picks between two siblings at
  preprocessor time based on `PETRA_NUL_TERMINATE`.

## What today's lesson pins

1. **The bounded-buffer vs NUL-terminating sibling choice CAN be
   lifted to a macro form on the `std::span<char>` axis.**  The
   shape is analogous to Sep 16's char*+cap macro layer — a macro
   that picks between two sibling wrappers at preprocessor time.
2. **The compile-time flag `PETRA_NUL_TERMINATE` controls the
   dispatch** via `#if PETRA_NUL_TERMINATE == 0/1`:
   - `#define PETRA_NUL_TERMINATE 0` → bounded-buffer span sibling:
     - CONSTEVAL: `petra::try_format_bounded_runtime_n_span` (Sep 10)
     - RUNTIME:  `petra::try_format_runtime_n` (Sep 8 char*+cap;
                  today's `try_format_runtime_n(span, ...)` NEW
                  wrapper FORWARDS to Sep 8's impl UNCHANGED).
   - `#define PETRA_NUL_TERMINATE 1` → NUL-terminating span sibling:
     - CONSTEVAL: `petra::format_to_n_into_bounded(span, ...)`
                  (Sep 11's span<char> forwarding overload).
     - RUNTIME:  `petra::try_format_runtime_n_into_bounded(span, ...)`
                  (Sep 12's span<char> forwarding overload).
3. **`#error` guard** fires at preprocessor time if the flag is
   set to anything other than 0 or 1 — pinned by the
   `#if PETRA_NUL_TERMINATE != 0 && PETRA_NUL_TERMINATE != 1`
   block.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto r = TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(...)` and capture
   the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16's macro
   shape exactly.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16.
6. **The CONSTEVAL macro** picks between Sep 10's
   `try_format_bounded_runtime_n_span` (bounded-buffer, NO NUL)
   and Sep 11's `format_to_n_into_bounded(span, ...)` (NUL-
   terminating).  Both are consteval-checked
   `std::format_string<Args...>` wrappers.
7. **The RUNTIME macro** picks between today's NEW
   `petra::try_format_runtime_n(span, ...)` forwarding wrapper
   (bounded-buffer, NO NUL; closes Sep 16's open item #3) and
   Sep 12's `try_format_runtime_n_into_bounded(span, ...)`
   (NUL-terminating).  Both take `std::string_view` format-string
   (no compile-time parse).
8. **The macro's contract is byte-exact**: the bounded-buffer
   span sibling leaves the byte at index `written` UNCHANGED (= the
   input byte); the NUL-terminating span sibling writes a `'\0'` at
   the position chosen by Sep 11's formula.  Section 2 (on the
   NUL build) pins (a) full-success NUL-at-written and (b)
   truncation-edge NUL-at-cap-minus-one; Section 3c pins (c)
   TooLarge atomicity (buffer UNTOUCHED on error).
9. **The macro's contract for the `FormatError` type is
   byte-exact**: both siblings return
   `std::expected<std::size_t, FormatError>`;
   `FormatError{kind, message, needed_bytes}` is identical.
   Section 4 pins this contract via `decltype` checks.
10. **The macro's contract for the success path returns the
    byte count**.  Both siblings report `written` (the actual
    number of bytes written, NOT including any `'\0'` the
    NUL-terminating span sibling writes).  Section 6 pins this on
    the bounded path.
11. **The `std::span<char>` sibling overloads do NOT bleed**
    (Section 7 pins this — a `std::span<char>` over a sub-range of
    a buffer must NOT touch bytes outside the span, mirroring
    Sep 10 / Sep 12's "no-bleed" contract).
12. **The `PETRA_NUL_TERMINATE` axis (bounded-buffer vs NUL-term)
    is PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis** (Sep 13's
    macro layer) **AND to the char*-vs-span axis** (Sep 10 /
    Sep 12's forwarding wrappers).  Today's meta-macros are at
    the INTERSECTION of all three axes: `span<char>` ×
    CONSTEVAL-or-RUNTIME × bounded-buffer-or-NUL-term.  Section
    10 pins the perpendicular-axis claim via `decltype` checks.
13. **Concurrency is safe** — Section 14 fires 4 threads × 25
    concurrent calls through both macros; per-thread local
    counters are aggregated AFTER `join()` via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.

## HEADLINE finding — the THREE-AXIS layer

Three independent axes now compose orthogonally at the meta-macro
layer:

1. **char*-vs-span axis** (Sep 10 / Sep 12's forwarding wrappers):
   the OUTPUT is either a `char*` + `std::size_t cap` (Sep 8 / Sep
   9 / Sep 11 / Sep 12 char*+cap path) or a `std::span<char>`
   (Sep 10 / Sep 11 / Sep 12 / today's NEW span<char> path).
2. **CONSTEVAL-vs-RUNTIME axis** (Sep 13's macro layer): the
   format-string is either a `std::format_string<Args...>` literal
   (consteval-checked at compile time) or a `std::string_view`
   (parsed at format time).
3. **bounded-buffer-vs-NUL-terminating axis** (Sep 16's macro
   layer, extended today to the span axis): the OUTPUT contract is
   either a bounded-buffer (bytes `[0, written)`, byte at `written`
   UNCHANGED) or NUL-terminating (bytes `[0, written)`, byte at
   `nul_idx = (written < cap) ? written : (cap - 1)` set to `'\0'`).

Today's lesson adds the third axis on the `std::span<char>` axis.
The preprocessor-time dispatch design (`#if PETRA_NUL_TERMINATE`)
extends naturally to the span<char> axis without any new machinery
— Sep 10's / Sep 11's / Sep 12's / today's NEW span<char>
forwarding wrappers already exist; today's lesson just plumbs the
meta-macro layer over them.

The dispatch is ZERO-OVERHEAD — at `-O0` the chosen wrapper is
inlined into the caller; at `-O1` the compiler sees only the
chosen wrapper.  The preprocessor-time `#if` is the cleanest design
because it (a) introduces NO runtime cost, (b) matches Sep 1 /
Sep 2 / Sep 3 / Sep 4's compile-time-flag framing, (c) lets the
`#error` guard fire at preprocessor time, and (d) is fully
composable with the OTHER two axes (the char*-vs-span axis and
the CONSTEVAL-vs-RUNTIME axis).

## Important code

### The two new macros — the public surface

```cpp
#ifndef PETRA_NUL_TERMINATE
#define PETRA_NUL_TERMINATE 0
#endif

#if PETRA_NUL_TERMINATE != 0 && PETRA_NUL_TERMINATE != 1
#error "PETRA_NUL_TERMINATE must be 0 (bounded-buffer) or 1 (NUL-terminating)"
#endif

#if PETRA_NUL_TERMINATE == 0
// Bounded-buffer sibling path — NO NUL termination.  The macro
// takes a std::span<char> for the OUTPUT (matching today's
// span-axis contract).
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(out, fmt, ...)                   \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(out, fmt, ...)           \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))
#else  // PETRA_NUL_TERMINATE == 1
// NUL-terminating sibling path — always NUL-terminates on success.
#define TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(out, fmt, ...)                   \
    (::petra::format_to_n_into_bounded((out), (fmt)                       \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO(out, fmt, ...)           \
    (::petra::try_format_runtime_n_into_bounded((out), (fmt)              \
                                                __VA_OPT__(,) __VA_ARGS__))
#endif  // PETRA_NUL_TERMINATE
```

### The new wrapper — `petra::try_format_runtime_n(span, ...)`

Today's ONLY new wrapper; a verbatim thin forwarding wrapper
mirroring Sep 10 / Sep 12's span<char> forwarding-wrapper shape.
Extracts `.data()` + `.size()` and dispatches to Sep 8's
char*+cap impl UNCHANGED:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(std::span<char> out, std::string_view fmt,
                     const Args&... args) {
    return try_format_runtime_n(out.data(), out.size(), fmt, args...);
}
```

No new impl, no new error contract.  Closes Sep 16's open item
#3.

### The underlying wrappers (verbatim from Sep 8 / Sep 10 / Sep 11 / Sep 12)

Each macro expands to a call to ONE of these wrappers.  No new
machinery on the wrapper side:

- `petra::try_format_bounded_runtime_n_span<Args...>(out_span,
  fmt, args...)` (Sep 10 — CONSTEVAL span<char>, NO NUL).
- `petra::try_format_runtime_n<Args...>(out_span, fmt, args...)`
  (Sep 8 char*+cap, NO NUL; today's NEW `try_format_runtime_n(span, ...)`
  forwards to this).
- `petra::format_to_n_into_bounded<Args...>(out_span, fmt, args...)`
  (Sep 11 — CONSTEVAL span<char>, NUL-terminating forwarding
  wrapper).
- `petra::try_format_runtime_n_into_bounded<Args...>(out_span,
  fmt, args...)` (Sep 12 — RUNTIME span<char>, NUL-terminating
  forwarding wrapper).

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21 — Sep 16's open item #1 documents this).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- The single forwarding wrapper `petra::try_format_runtime_n(span,
  ...)` — verbatim thin dispatcher to Sep 8's char*+cap impl.
- The two `TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO` /
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO` meta-macros —
  each a verbatim thin dispatcher to one of the four wrappers
  above, selected by `#if PETRA_NUL_TERMINATE`.  No new types
  (one new wrapper only), no new impls, no new error contracts.

## Build and verification commands

Default (`PETRA_NUL_TERMINATE=0` — bounded-buffer path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-19-try-format-span-n-or-into.cpp \
        -o /tmp/P-2026-09-19-try-format-span-n-or-into
/tmp/P-2026-09-19-try-format-span-n-or-into
```

NUL-terminating path (`PETRA_NUL_TERMINATE=1`):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_NUL_TERMINATE=1 \
        P-2026-09-19-try-format-span-n-or-into.cpp \
        -o /tmp/P-2026-09-19-try-format-span-n-or-into-nul
/tmp/P-2026-09-19-try-format-span-n-or-into-nul
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-19-try-format-span-n-or-into.cpp \
        -o /tmp/P-2026-09-19-try-format-span-n-or-into-strict
/tmp/P-2026-09-19-try-format-span-n-or-into-strict
```

ASan + UBSan build (bounded-buffer path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-19-try-format-span-n-or-into.cpp \
        -o /tmp/P-2026-09-19-try-format-span-n-or-into-asan
/tmp/P-2026-09-19-try-format-span-n-or-into-asan
```

CMake (default / strict / ASan / NUL-terminating) — see
`CMakeLists.txt` in this dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
cmake -S . -B build-nul -DPETRA_NUL_TERMINATE=1 \
                              && cmake --build build-nul
./build/P-2026-09-19-try-format-span-n-or-into
./build-strict/P-2026-09-19-try-format-span-n-or-into
./build-asan/P-2026-09-19-try-format-span-n-or-into
./build-nul/P-2026-09-19-try-format-span-n-or-into
```

## Section-by-section notes

### Section 1 — `PETRA_NUL_TERMINATE` compile-time flag pinned

Verifies the compile-time flag is pinned at the value the build
used.  Prints the chosen path so the output is auditable in the
log:

```
[Section 1] PETRA_NUL_TERMINATE=0 (bounded-buffer sibling)
```

or, for the NUL-terminating build:

```
[Section 1] PETRA_NUL_TERMINATE=1 (NUL-terminating sibling)
```

Also pins via `static_assert` that both macros dispatch to
`std::expected<std::size_t, FormatError>` and that
`std::span<char>` satisfies `std::ranges::contiguous_range` +
`std::ranges::sized_range` + trivially_copyable + standard_layout +
`.data()->char*` + `.size()->std::size_t`.  PASS.

### Section 2 — sibling path verification (both branches)

Two-branch test pinned by the same source compiled with
`PETRA_NUL_TERMINATE=0` (default) and `PETRA_NUL_TERMINATE=1`.

On the bounded-buffer path:

- 2a — CONSTEVAL macro `TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO`
  dispatches to Sep 10's `try_format_bounded_runtime_n_span`
  (bounded-buffer, NO NUL).  `r.value() == 11` for
  `"hello {}"` + `"world"` (== "hello world").  `buf[11]` must be
  UNCHANGED (= `'\0'` from the poison init).
- 2b — RUNTIME macro `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO`
  dispatches to today's NEW `petra::try_format_runtime_n(span,
  ...)` (bounded-buffer, NO NUL).
- 2c — NO NUL: poison with `'Q'` first, then verify ALL bytes
  after `written` remain `'Q'`.

On the NUL-terminating path (same source compiled with
`-DPETRA_NUL_TERMINATE=1`):

- 2a — CONSTEVAL macro dispatches to Sep 11's
  `format_to_n_into_bounded(span, ...)` (NUL-terminating).
  `buf[11]` IS written as `'\0'` by the NUL-terminating sibling.
- 2b — RUNTIME macro dispatches to Sep 12's
  `try_format_runtime_n_into_bounded(span, ...)` (NUL-terminating).
- 2c — NUL placement: poison with `'Q'`, then verify `buf[3] ==
  '\0'` (NUL placed by sibling at index `written`) and
  `buf[31] == 'Q'` (untouched poison AFTER the NUL).

### Section 3 — atomicity on `TooLarge` (bounded-buffer path)

Pins the bounded-buffer span sibling's atomicity on the `TooLarge`
error path: when `needed > cap`, the caller's buffer is UNTOUCHED
(pass-1 measure-then-cap-fails short-circuits before pass-2 writes
anything).  Pre-fills the buffer with poison `'Q'` and verifies
ALL bytes are still `'Q'` after the failing call.  PASS on the
bounded-buffer build; this section is `#if`-gated to
`PETRA_NUL_TERMINATE == 0`.

### Section 3c — atomicity on `TooLarge` (NUL-terminating path)

Pins the NUL-terminating span sibling's atomicity on the `TooLarge`
error path: the caller's buffer is UNTOUCHED (the sibling does NOT
write a spurious `'\0'` on the error path).  Pre-fills with poison
`'Q'` and verifies ALL bytes are still `'Q'` after the failing
call.  PASS on the NUL-terminating build (compiled with
`-DPETRA_NUL_TERMINATE=1`).

This section, together with Section 2's NUL-terminating branch,
pins all three NUL-placement contracts:

- (a) on success with `needed < cap`, the NUL-terminating span
  sibling writes `'\0'` at `buf[written]` (== `buf[needed]`).
- (b) on truncation (`needed == cap`), the NUL-terminating span
  sibling writes `'\0'` at `buf[cap-1]` (Sep 11's formula).
- (c) on `TooLarge` (`needed > cap`), the buffer is UNTOUCHED.

### Section 4 — return-type contract (decltype)

Pins the byte-exact contract that both meta-macros return
`std::expected<std::size_t, FormatError>` via `decltype` checks
on each macro.  Also pins the `FormatError{kind, message,
needed_bytes}` contract on the `TooLarge` error path.

### Section 5 — NUL-terminating sibling path pinned via separate build

Documents that the NUL-terminating path is pinned by SEPARATE
sections that exercise the NUL-terminating span sibling directly:
Section 2's `PETRA_NUL_TERMINATE=1` branch pins contracts (a) and
(b), and Section 3c pins contract (c).  All three contracts are
exercised when the same source is compiled with
`-DPETRA_NUL_TERMINATE=1`.

### Section 6 — byte-count contract (bounded-buffer path)

Pins that the success-path return value is the byte count
(== `written`, the actual number of bytes written, NOT including
any `'\0'`).  PASS on the bounded-buffer build.

### Section 7 — `std::span<char>` sub-span does NOT bleed

Pins the span "no-bleed" contract.  A 16-byte buffer with a
6-byte sub-span at offset 5 (covering `[5, 11)`) formatted with
`"ABCDE"` writes only bytes `[5, 10)` (== `"ABCDE"`); bytes
`[0, 5)` and `[11, 16)` remain poison `'Q'`.  Mirrors Sep 10 /
Sep 12's "no-bleed" contract.

### Section 8 — runtime format string (bounded-buffer path)

The KEY use case for the RUNTIME surface: build the format
string dynamically.  The CONSTEVAL macro would reject this at
compile time (no literal).  The RUNTIME macro accepts it.  PASS
on the bounded-buffer build.

### Section 9 — `FormatError` equality contract

Pins that the `FormatError{kind, message, needed_bytes}` struct
is byte-exact across both siblings — a `TooLarge` error from the
bounded-buffer sibling has the same `kind` / `message` /
`needed_bytes` contract as a `TooLarge` error from the
NUL-terminating sibling.  PASS on the bounded-buffer build.

### Section 10 — perpendicular-axis (PETRA_NUL_TERMINATE × CONSTEVAL-vs-RUNTIME)

The KEY HEADLINE finding — the `PETRA_NUL_TERMINATE` axis
(bounded-buffer vs NUL-terminating) is PERPENDICULAR to the
CONSTEVAL-vs-RUNTIME axis (Sep 13's macro layer), AND to the
char*-vs-span axis (Sep 10 / Sep 12's forwarding wrappers).
Today's `TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO` with
`PETRA_NUL_TERMINATE=0` dispatches to the same wrapper as the
helper `TRY_FORMAT_BOUNDED_N_SPAN` (CONSTEVAL + bounded-buffer +
span<char>; Sep 10's wrapper).  Today's
`TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO` with
`PETRA_NUL_TERMINATE=0` dispatches to the same wrapper as the
helper `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN` (RUNTIME + bounded-
buffer + span<char>; today's NEW `petra::try_format_runtime_n(span,
...)` wrapper).

Pinned by `static_assert(std::is_same_v<R10a, R10b>)` and
`static_assert(std::is_same_v<R10c, R10d>)`.  PASS.

### Section 11 — no-args variant

`__VA_OPT__(,)` handles the no-args case (a format with no
replacement fields).  `TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO(sp, "ping")`
expands to `::petra::try_format_bounded_runtime_n_span(sp, "ping")` —
no trailing comma.  PASS.

### Section 12 — multi-arg round-trip

Multi-arg format `"x={} y={} z={:.2f}"` + two integer args and a
double produces the expected byte count and prefix bytes.  PASS
on the bounded-buffer build.

### Section 13 — embedded NUL byte round-trip

A format that produces an embedded NUL byte (e.g.
`"hi\x00hi {}!"` runtime-built — a literal with an embedded NUL
would be truncated at the NUL by the literal-to-string-view
conversion).  The byte at the NUL position is `'\0'`; the byte
at `written` on the bounded-buffer path is the input byte (the
NUL is NOT counted as part of `written`).  PASS on the
bounded-buffer build.

### Section 14 — 4 threads × 25 calls concurrent dispatch

Pins thread safety.  4 threads × 25 calls each fire concurrently
through BOTH the CONSTEVAL and RUNTIME meta-macros.  Per-thread
local counters are aggregated AFTER `join()` via
`std::atomic<int>::fetch_add` to avoid a data race on `g_pass` /
`g_fail`.  All 100 calls per macro return `r.has_value() == true`
and `r.value() == 8` (the byte count for `"[{:02}][{:02}]"` + two
integers).  PASS.

### Section 15 — final summary

Prints the total pass / fail tally.  Bounded-buffer build:
`PASS: 154  FAIL: 0`.  NUL-terminating build: `PASS: 60  FAIL: 0`.

## Build matrix — observed output

| Build                                         | Compile | Run           |
|-----------------------------------------------|---------|---------------|
| Default (`PETRA_NUL_TERMINATE=0`) — `-O0`     | clean   | PASS 154/0    |
| NUL-terminating (`PETRA_NUL_TERMINATE=1`) — `-O0` | clean | PASS 60/0      |
| Strict-warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | clean | PASS 154/0 |
| ASan + UBSan (`-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1`) | clean | PASS 154/0 |
| CMake default                                 | clean   | PASS 154/0    |
| CMake strict                                  | clean   | PASS 154/0    |
| CMake ASan                                    | clean   | PASS 154/0    |
| CMake NUL-terminating                         | clean   | PASS 60/0     |

Five consecutive runs on each build produced identical
`PASS: 154 FAIL: 0` (bounded-buffer) / `PASS: 60 FAIL: 0`
(NUL-terminating) tallies.  No flakiness in the 4-thread ×
25-call concurrent Section 14.

Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`.  Zero sanitizer diagnostics.  Empty stderr
under default + ASan.

## Summary

- Topic: a macro layer that lifts the BOUNDED-BUFFER vs
  NUL-TERMINATING sibling choice to a macro form on the
  `std::span<char>` axis, analogous to Sep 16's char*+cap macro
  layer.
- Two new macros — `TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO` (CONSTEVAL
  surface) and `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO` (RUNTIME
  surface) — each dispatch to one of two span<char> siblings based
  on the compile-time flag `PETRA_NUL_TERMINATE` (0 = bounded-buffer,
  1 = NUL-terminating).
- Reused verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 /
  Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16:
  `petra::FormatError`, `petra::FormatErrorKind`,
  `petra::counting_output_iterator`,
  `petra::detail::bounded_char_writer`,
  `petra::detail::try_format_runtime_n_impl` (Sep 8),
  `petra::detail::try_format_to_n_into_bounded_impl` (Sep 12),
  `petra::try_format_runtime_n` (Sep 8 char*+cap),
  `petra::try_format_bounded_runtime_n_span` (Sep 10),
  `petra::format_to_n_into_bounded(span, ...)` (Sep 11),
  `petra::try_format_runtime_n_into_bounded(span, ...)` (Sep 12).
- New today: the single forwarding wrapper
  `petra::try_format_runtime_n(span, ...)` (closes Sep 16's open
  item #3) + the two `TRY_FORMAT_BOUNDED_SPAN_*_OR_INTO` meta-
  macros + the `PETRA_NUL_TERMINATE` compile-time flag + a
  `#error` guard.  The wrapper is a verbatim thin dispatcher
  (extracts `.data()` + `.size()` and dispatches to Sep 8's
  char*+cap impl UNCHANGED); each macro is a verbatim thin
  dispatcher to one of the four underlying span<char> wrappers.
- HEADLINE finding — the `PETRA_NUL_TERMINATE` axis (bounded-buffer
  vs NUL-term) is PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis
  (Sep 13's macro layer) AND to the char*-vs-span axis (Sep 10 /
  Sep 12's forwarding wrappers).  Today's meta-macros are at the
  INTERSECTION of all three axes: `span<char>` × CONSTEVAL-or-
  RUNTIME × bounded-buffer-or-NUL-term.  Preprocessor-time
  `#if PETRA_NUL_TERMINATE` is the cleanest design for the
  dispatch: zero runtime cost; matches Sep 1 / Sep 2's compile-
  time-flag framing; `#error` fires at preprocessor time if the
  flag is invalid; composes orthogonally with Sep 13's macro
  layer (Section 10's `decltype` checks pin this).
- 154/154 PASS across 15 sections on the bounded-buffer default +
  strict-warning + ASan/UBSan + CMake builds.
- 60/60 PASS across 15 sections on the NUL-terminating build (all
  three NUL-placement contracts pinned: (a) NUL-at-written on
  full-success, (b) NUL-at-cap-minus-one on truncation edge,
  (c) buffer UNTOUCHED on TooLarge).
- Five consecutive runs on each build produced identical tallies
  (no flakiness in Section 14's 4-thread × 25-call concurrent
  dispatch — per-thread local counters aggregated after join).
- Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion`; zero sanitizer diagnostics;
  empty stderr under default + ASan.
- Lesson is a pure consumer-side C++23 `std::format` +
  `std::vformat_to` + `std::format_string<Args...>` (P2216R3)
  + `std::make_format_args` + `std::expected` (P0323R12) +
  `std::span<char>` (P0122R7) + `std::ranges::contiguous_range`
  + `std::println` (P2093R14) + `__VA_OPT__` preprocessor
  feature exercise — reuses Sep 6 / Sep 7 / Sep 8 / Sep 9 /
  Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16's
  machinery verbatim, no third-party libraries, no library
  installation, no find_package.

## Where we go next

Today's lesson closes Sep 16's "Where we go next" item #2 (Sep
15's open item #2) — the `PETRA_NUL_TERMINATE`-gated pair of
meta-macros for the `std::span<char>` axis.  Today's lesson also
closes Sep 16's open item #3 — the missing bounded-buffer
RUNTIME span sibling `petra::try_format_runtime_n(span, ...)`
(now defined as a verbatim thin forwarding wrapper to Sep 8's
char*+cap impl).

Two items from Sep 16 remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type.  Today's lesson reuses the hand-rolled iterator from
   Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 /
   Sep 13 / Sep 14 / Sep 15 / Sep 16 verbatim; when a future
   libc++ ships the standard type, a follow-up lesson migrates
   all the wrappers that use the iterator.  (Today's lesson
   verified the type is NOT yet in libc++ 21 via the same
   static_assert path Sep 16 documented; the type remains
   unavailable in libc++ 21.)

The three-axis matrix is now COMPLETE for the bounded-buffer vs
NUL-terminating sibling choice at the meta-macro layer:

| axis | char*+cap | span<char> |
|------|-----------|------------|
| CONSTEVAL + bounded-buffer     | Sep 16 TRY_FORMAT_BOUNDED_N_OR_INTO | TODAY TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO |
| RUNTIME + bounded-buffer       | Sep 16 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO | TODAY TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO |
| CONSTEVAL + NUL-terminating    | Sep 16 TRY_FORMAT_BOUNDED_N_OR_INTO @ PETRA_NUL_TERMINATE=1 | TODAY TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=1 |
| RUNTIME + NUL-terminating      | Sep 16 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO @ PETRA_NUL_TERMINATE=1 | TODAY TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=1 |

The preprocessor-time dispatch design generalizes naturally to
ANY future axis (e.g. a new `std::span<char>` flag, a new
strict-mode flag, etc.) — each new axis is a new
`#if PETRA_<NEW_FLAG>` block layered on top of the existing
axes.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.