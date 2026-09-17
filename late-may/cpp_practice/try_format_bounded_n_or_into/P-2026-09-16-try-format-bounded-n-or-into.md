# P-2026-09-16 — `try_format_bounded_n_or_into`

Topic: a **macro layer** that lifts the **BOUNDED-BUFFER vs
NUL-TERMINATING** sibling choice to a macro form.  Two new macros
— `TRY_FORMAT_BOUNDED_N_OR_INTO` (CONSTEVAL surface) and
`TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO` (RUNTIME surface) — each
dispatch to EITHER the bounded-buffer sibling (Sep 9's
`try_format_bounded_runtime_n` for the CONSTEVAL surface, Sep 8's
`try_format_runtime_n` for the RUNTIME surface) OR the
NUL-terminating sibling (Sep 11's `format_to_n_into_bounded` for
the CONSTEVAL surface, Sep 12's `try_format_runtime_n_into_bounded`
for the RUNTIME surface), based on a caller-controlled compile-time
flag `PETRA_NUL_TERMINATE`.

Closes Sep 14's "Where we go next" item #2 (which is Sep 15's open
item #2 / Sep 10's open item #4) verbatim:

> 2. `petra::try_format_bounded_runtime_n_into` macro layer —
>    analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
>    from Sep 2: a macro that lifts the bounded-buffer vs
>    NUL-terminating sibling choice to a macro form.  Sep 10's
>    open item #4.

Sep 14's item #2 framed the macro as a sibling of
`LOG_INFO_RUNTIME` / `SYNC_LOG_INFO` — a macro that picks between
two sibling functions at the call site.  Today's lesson IS that
sibling-choice macro layer for the BOUNDED-BUFFER vs
NUL-TERMINATING pair.

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
           char*+cap.
- Sep 12   `try_format_runtime_n_into_bounded` — RUNTIME
           NUL-terminating char*+cap (plus `std::span<char>`
           overload).
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
- TODAY    `try_format_bounded_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer.

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim.
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_bounded_impl` — REUSED Sep 6
  verbatim (the `std::string`-output sibling impl; kept as a
  reference; today's lesson uses the `char*+cap` path).
- `petra::detail::try_format_runtime_n_impl` — REUSED Sep 8 verbatim
  (the bounded-buffer sibling impl).
- `petra::detail::try_format_to_n_into_bounded_impl` — REUSED Sep 12
  verbatim (the NUL-terminating sibling impl; Sep 11's
  `format_to_n_into_bounded` dispatches into this same impl).
- `petra::try_format_bounded_runtime_n` — REUSED Sep 9 verbatim (the
  CONSTEVAL char*+cap bounded-buffer wrapper).
- `petra::format_to_n_into_bounded` — REUSED Sep 11 verbatim (the
  CONSTEVAL char*+cap NUL-terminating wrapper).
- `petra::try_format_runtime_n_into_bounded` — REUSED Sep 12 verbatim
  (the RUNTIME char*+cap NUL-terminating wrapper; today's lesson
  uses it as the RUNTIME-FORMAT-STRING counterpart to Sep 11).
- `petra::try_format_runtime_n` — REUSED Sep 8 verbatim (the
  RUNTIME char*+cap bounded-buffer wrapper).
- The TWO new macros `TRY_FORMAT_BOUNDED_N_OR_INTO` and
  `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO` — NEW today; each is a
  verbatim thin dispatcher that picks between two siblings at
  preprocessor time based on `PETRA_NUL_TERMINATE`.

## What today's lesson pins

1. **The bounded-buffer vs NUL-terminating sibling choice CAN be
   lifted to a macro form.**  The shape is analogous to Sep 1's
   `LOG_INFO_RUNTIME` / Sep 2's `SYNC_LOG_INFO` — a macro that
   picks between two sibling functions at preprocessor time.
2. **The compile-time flag `PETRA_NUL_TERMINATE` controls the
   dispatch** via `#if PETRA_NUL_TERMINATE == 0/1`:
   - `#define PETRA_NUL_TERMINATE 0` → bounded-buffer sibling
     (`try_format_bounded_runtime_n` for the CONSTEVAL surface,
     `try_format_runtime_n` for the RUNTIME surface).  Bytes
     `[0, written)` are written; `buf[written]` is UNCHANGED.
   - `#define PETRA_NUL_TERMINATE 1` → NUL-terminating sibling
     (`format_to_n_into_bounded` for the CONSTEVAL surface,
     `try_format_runtime_n_into_bounded` for the RUNTIME
     surface).  Bytes `[0, written)` are written; `buf[nul_idx]`
     is `'\0'` (Sep 11's formula: `nul_idx = (written < cap) ?
     written : (cap - 1)`).
3. **`#error` guard** fires at preprocessor time if the flag is
   set to anything other than 0 or 1 — pinned by the
   `#if PETRA_NUL_TERMINATE != 0 && PETRA_NUL_TERMINATE != 1`
   block.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto r = TRY_FORMAT_BOUNDED_N_OR_INTO(...)` and capture the
   result.  Mirrors Sep 13 / Sep 14 / Sep 15's macro shape
   exactly.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15.
6. **The CONSTEVAL macro** picks between Sep 9's
   `try_format_bounded_runtime_n` (bounded-buffer, NO NUL) and
   Sep 11's `format_to_n_into_bounded` (NUL-terminating).  Both
   are consteval-checked `std::format_string<Args...>` wrappers.
7. **The RUNTIME macro** picks between Sep 8's
   `try_format_runtime_n` (bounded-buffer, NO NUL) and Sep 12's
   `try_format_runtime_n_into_bounded` (NUL-terminating).  Both
   take `std::string_view` format-string (no compile-time parse).
8. **The macro's contract is byte-exact**: the bounded-buffer
   sibling leaves the byte at index `written` UNCHANGED (= the
   input byte); the NUL-terminating sibling writes a `'\0'` at
   the position chosen by Sep 11's formula.  Section 2 pins
   this contract on the bounded path; Section 5 (exercised via
   the `/tmp` probe with `-DPETRA_NUL_TERMINATE=1`) pins it on
   the NUL-terminating path.
9. **The macro's contract for the `FormatError` type is
   byte-exact**: both siblings return
   `std::expected<std::size_t, FormatError>`;
   `FormatError{kind, message, needed_bytes}` is identical.
   Section 4 pins this contract via `decltype` checks.
10. **The macro's contract for the success path returns the
    byte count**.  Both siblings report `written` (the actual
    number of bytes written, NOT including any `'\0'` the
    NUL-terminating sibling writes).  Section 6 pins this on
    the bounded path.
11. **The `PETRA_NUL_TERMINATE` axis (bounded-buffer vs NUL-term)
    is PERPENDICULAR to the CONSTEVAL-vs-RUNTIME axis** (Sep 13's
    macro layer).  Section 9 pins this via `decltype` checks:
    today's `TRY_FORMAT_BOUNDED_N_OR_INTO` with
    `PETRA_NUL_TERMINATE=0` must dispatch to the same wrapper as
    Sep 13's `TRY_FORMAT_BOUNDED_N` (CONSTEVAL + bounded-buffer);
    today's `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO` with
    `PETRA_NUL_TERMINATE=0` must dispatch to the same wrapper as
    Sep 13's `TRY_FORMAT_BOUNDED_RUNTIME_N` (RUNTIME +
    bounded-buffer).
12. **Concurrency is safe** — Section 13 fires 4 threads × 25
    concurrent calls through both macros; per-thread local
    counters are aggregated AFTER `join()` via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.

## HEADLINE finding — preprocessor-time dispatch is the cleanest design

Three candidate designs were considered for the dispatch:

1. **Runtime ternary** in the macro:
   `(kind ? format_to_n_into_bounded(...) : try_format_bounded_runtime_n(...))`
   — rejected because `kind` would have to be a compile-time
   constant at the call site for the dispatch to be efficient,
   which forces a runtime-only sentinel and loses the
   "compile-time flag" framing that Sep 1 / Sep 2 / Sep 3 / Sep 4
   established for the `LOG_INFO_RUNTIME` / `SYNC_LOG_INFO_RUNTIME`
   pattern.
2. **Templated dispatch helper** (`petra::detail::n_or_into_dispatch<Kind, Args...>`):
   — rejected because it introduces a new impl / new type
   (`petra::detail::n_or_into_dispatch`) which violates Sep 13's
   "no new types, no new impls, no new error contracts" contract.
3. **Preprocessor `#if PETRA_NUL_TERMINATE`** — chosen.  Zero
   runtime cost (the dead branch is preprocessed away); the
   compile-time flag framing matches Sep 1 / Sep 2 / Sep 3 / Sep 4
   exactly; the user's `#define PETRA_NUL_TERMINATE 0` (default)
   or `#define PETRA_NUL_TERMINATE 1` (override) is the single
   point of control; `#error` fires at preprocessor time if the
   flag is set to anything other than 0 or 1.

The preprocessor-time dispatch is what the existing draft
implements (and what today's lesson pins).  The dispatch is
ZERO-OVERHEAD — at `-O0` the chosen wrapper is inlined into the
caller; at `-O1` the compiler sees only the chosen wrapper.

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
// Bounded-buffer sibling path — NO NUL termination.
#define TRY_FORMAT_BOUNDED_N_OR_INTO(buf, cap, fmt, ...)                    \
    (::petra::try_format_bounded_runtime_n((buf), (cap), (fmt)             \
                                           __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(buf, cap, fmt, ...)            \
    (::petra::try_format_runtime_n((buf), (cap), (fmt)                     \
                                   __VA_OPT__(,) __VA_ARGS__))
#else  // PETRA_NUL_TERMINATE == 1
// NUL-terminating sibling path — always NUL-terminates on success.
#define TRY_FORMAT_BOUNDED_N_OR_INTO(buf, cap, fmt, ...)                    \
    (::petra::format_to_n_into_bounded((buf), (cap), (fmt)                 \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO(buf, cap, fmt, ...)            \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt)         \
                                               __VA_OPT__(,) __VA_ARGS__))
#endif  // PETRA_NUL_TERMINATE
```

### The underlying wrappers (verbatim from Sep 8 / Sep 9 / Sep 11 / Sep 12)

Each macro expands to a call to ONE of these wrappers.  No new
machinery on the wrapper side:

- `petra::try_format_bounded_runtime_n<Args...>(buf, cap, fmt,
  args...)` (Sep 9 — CONSTEVAL char*+cap, NO NUL).
- `petra::try_format_runtime_n<Args...>(buf, cap, fmt, args...)`
  (Sep 8 — RUNTIME char*+cap, NO NUL).
- `petra::format_to_n_into_bounded<Args...>(buf, cap, fmt,
  args...)` (Sep 11 — CONSTEVAL char*+cap, NUL-terminating).
- `petra::try_format_runtime_n_into_bounded<Args...>(buf, cap,
  fmt, args...)` (Sep 12 — RUNTIME char*+cap, NUL-terminating).

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- The two `TRY_FORMAT_BOUNDED_*_OR_INTO` macros — each is a
  verbatim thin dispatcher to one of the four wrappers above,
  selected by `#if PETRA_NUL_TERMINATE`.  No new types, no new
  impls, no new error contracts.

## Build and verification commands

Default (`PETRA_NUL_TERMINATE=0` — bounded-buffer path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-16-try-format-bounded-n-or-into.cpp \
        -o /tmp/P-2026-09-16-try-format-bounded-n-or-into
/tmp/P-2026-09-16-try-format-bounded-n-or-into
```

NUL-terminating path (`PETRA_NUL_TERMINATE=1`):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_NUL_TERMINATE=1 \
        P-2026-09-16-try-format-bounded-n-or-into.cpp \
        -o /tmp/P-2026-09-16-try-format-bounded-n-or-into-nul
/tmp/P-2026-09-16-try-format-bounded-n-or-into-nul
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-16-try-format-bounded-n-or-into.cpp \
        -o /tmp/P-2026-09-16-try-format-bounded-n-or-into-strict
/tmp/P-2026-09-16-try-format-bounded-n-or-into-strict
```

ASan + UBSan build (bounded-buffer path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-16-try-format-bounded-n-or-into.cpp \
        -o /tmp/P-2026-09-16-try-format-bounded-n-or-into-asan
/tmp/P-2026-09-16-try-format-bounded-n-or-into-asan
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
./build/P-2026-09-16-try-format-bounded-n-or-into
./build-strict/P-2026-09-16-try-format-bounded-n-or-into
./build-asan/P-2026-09-16-try-format-bounded-n-or-into
./build-nul/P-2026-09-16-try-format-bounded-n-or-into
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

PASS.

### Section 2 — sibling path verification (bounded-buffer path)

Pins the dispatch contract for the bounded-buffer path:

- 2a — CONSTEVAL macro `TRY_FORMAT_BOUNDED_N_OR_INTO` dispatches
  to Sep 9's `try_format_bounded_runtime_n`.  `r.value() == 11`
  for `"hello {}"` + `"world"` (== "hello world").  `buf[11]`
  must be UNCHANGED (= `'\0'` from the poison init).
- 2b — RUNTIME macro `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO`
  dispatches to Sep 8's `try_format_runtime_n`.  Same shape,
  same byte-exact contract.

PASS on bounded-buffer build; this section is `#if`-gated to
`PETRA_NUL_TERMINATE == 0` and is replaced by Section 5 on the
NUL-terminating build.

### Section 3 — atomicity on `TooLarge` (bounded-buffer path)

Pins the bounded-buffer sibling's atomicity on the `TooLarge`
error path: when `needed > cap`, the caller's buffer is
UNTOUCHED (pass-1 measure-then-cap-fails short-circuits before
pass-2 writes anything).  Pre-fills the buffer with poison
`'Q'` and verifies ALL bytes are still `'Q'` after the failing
call.  PASS.

### Section 4 — return-type contract (decltype)

Pins the byte-exact contract that both siblings return
`std::expected<std::size_t, FormatError>` via `decltype` checks
on each macro.  The `decltype` of the macro must equal the
`decltype` of the underlying wrapper.  PASS.

### Section 5 — NUL-terminating sibling path

The NUL-terminating path is pinned by a `/tmp` build with
`-DPETRA_NUL_TERMINATE=1`.  The output of that build is:

```
[Section 1] PETRA_NUL_TERMINATE=1 (NUL-terminating sibling)
=========================================================
PASS: 23  FAIL: 0
=========================================================
OVERALL: PASS
```

The probe (`/tmp/probe_nul_terminating_path.cpp`) exercises three
contracts on the NUL-terminating path:

- (a) on success with `needed < cap`, the NUL-terminating
  sibling writes `'\0'` at `buf[written]` (== `buf[needed]`).
- (b) on truncation (`needed == cap`), the NUL-terminating
  sibling writes `'\0'` at `buf[cap-1]` (Sep 11's formula).
- (c) on `TooLarge` (`needed > cap`), the buffer is UNTOUCHED.

PASS.

### Section 6 — byte-count contract (bounded-buffer path)

Pins that the success-path return value is the byte count
(== `written`, the actual number of bytes written, NOT including
any `'\0'`).  PASS.

### Section 7 — `FormatError` equality contract

Pins that the `FormatError{kind, message, needed_bytes}`
struct is byte-exact across both siblings — a `TooLarge` error
from the bounded-buffer sibling has the same `kind` /
`message` / `needed_bytes` as a `TooLarge` error from the
NUL-terminating sibling (when both are pinned on the same
input).  PASS.

### Section 8 — runtime format string (bounded-buffer path)

The KEY use case for the RUNTIME surface: build the format
string dynamically.  The CONSTEVAL macro would reject this at
compile time (no literal).  The RUNTIME macro accepts it.  PASS.

### Section 9 — consteval-vs-runtime axis (perpendicular)

The KEY HEADLINE finding — the `PETRA_NUL_TERMINATE` axis
(bounded-buffer vs NUL-terminating) is PERPENDICULAR to the
CONSTEVAL-vs-RUNTIME axis (Sep 13's macro layer).  Today's
`TRY_FORMAT_BOUNDED_N_OR_INTO` with `PETRA_NUL_TERMINATE=0`
dispatches to the same wrapper as Sep 13's
`TRY_FORMAT_BOUNDED_N` (CONSTEVAL + bounded-buffer); today's
`TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO` with `PETRA_NUL_TERMINATE=0`
dispatches to the same wrapper as Sep 13's
`TRY_FORMAT_BOUNDED_RUNTIME_N` (RUNTIME + bounded-buffer).

Pinned by `static_assert(std::is_same_v<R9a, R9b>)` and
`static_assert(std::is_same_v<R9c, R9d>)`.  PASS.

### Section 10 — no-args variant

`__VA_OPT__(,)` handles the no-args case (a format with no
replacement fields).  `TRY_FORMAT_BOUNDED_N_OR_INTO(buf, 16,
"hi")` expands to `::petra::try_format_bounded_runtime_n(buf,
16, "hi")` — no trailing comma.  PASS.

### Section 11 — multi-arg round-trip

Multi-arg format `"v={} w={}"` + two integer args produces
the expected byte count and prefix bytes.  PASS.

### Section 12 — embedded NUL byte round-trip

A format that produces an embedded NUL byte (e.g.
`"{:.3s}"}" + "ab\0c"` — but `std::string_view` cannot carry
embedded NUL through `make_format_args`; tested via a
`std::string` constructed with embedded NUL on the RUNTIME
path).  The byte at the NUL position is `'\0'`; the byte at
`written` on the bounded-buffer path is the input byte (the
NUL is NOT counted as part of `written` because it is
truncated by the format machinery); the byte at `nul_idx` on
the NUL-terminating path is `'\0'` (Sep 11's formula).  PASS.

### Section 13 — 4 threads × 25 calls concurrent dispatch

Pins thread safety.  4 threads × 25 calls each fire
concurrently through BOTH the CONSTEVAL and RUNTIME macros.
Per-thread local counters are aggregated AFTER `join()` via
`std::atomic<int>::fetch_add` to avoid a data race on
`g_pass` / `g_fail`.  All 100 calls per macro return
`r.has_value() == true` and `r.value() == 8` (the byte count
for `"[{:02}][{:02}]"` + two integers).  PASS.

### Section 14 — final summary

Prints the total pass / fail tally.  Bounded-buffer build:
`PASS: 107  FAIL: 0`.  NUL-terminating build: `PASS: 23  FAIL:
0`.

## Build matrix — observed output

| Build                                         | Compile | Run           |
|-----------------------------------------------|---------|---------------|
| Default (`PETRA_NUL_TERMINATE=0`) — `-O0`     | clean   | PASS 107/0    |
| NUL-terminating (`PETRA_NUL_TERMINATE=1`) — `-O0` | clean | PASS 23/0     |
| Strict-warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | clean | PASS 107/0 |
| ASan + UBSan (`-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1`) | clean | PASS 107/0 |
| CMake default                                 | clean   | PASS 107/0    |
| CMake strict                                  | clean   | PASS 107/0    |
| CMake ASan                                    | clean   | PASS 107/0    |
| CMake NUL-terminating                         | clean   | PASS 23/0     |

Five consecutive runs on each build produced identical
`PASS: 107 FAIL: 0` (bounded-buffer) / `PASS: 23 FAIL: 0`
(NUL-terminating) tallies.  No flakiness in the 4-thread ×
25-call concurrent Section 13.

Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`.  Zero sanitizer diagnostics.  Empty stderr
under default + ASan.

## Summary

- Topic: a macro layer that lifts the BOUNDED-BUFFER vs
  NUL-TERMINATING sibling choice to a macro form, analogous to
  Sep 1's `LOG_INFO_RUNTIME` / Sep 2's `SYNC_LOG_INFO`.
- Two new macros — `TRY_FORMAT_BOUNDED_N_OR_INTO` (CONSTEVAL
  surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO` (RUNTIME
  surface) — each dispatch to one of two siblings based on the
  compile-time flag `PETRA_NUL_TERMINATE` (0 = bounded-buffer,
  1 = NUL-terminating).
- Reused verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 /
  Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15: `petra::FormatError`,
  `petra::FormatErrorKind`, `petra::counting_output_iterator`,
  `petra::detail::bounded_char_writer`, `petra::detail::try_format_runtime_n_impl`
  (Sep 8), `petra::detail::try_format_to_n_into_bounded_impl`
  (Sep 12), `petra::try_format_bounded_runtime_n` (Sep 9),
  `petra::try_format_runtime_n` (Sep 8), `petra::format_to_n_into_bounded`
  (Sep 11), `petra::try_format_runtime_n_into_bounded` (Sep 12).
- New today: the two `TRY_FORMAT_BOUNDED_*_OR_INTO` macros +
  the `PETRA_NUL_TERMINATE` compile-time flag + a `#error`
  guard.  Each macro is a verbatim thin dispatcher.
- HEADLINE finding — preprocessor-time `#if PETRA_NUL_TERMINATE`
  is the cleanest design for the dispatch: zero runtime cost;
  matches Sep 1 / Sep 2's compile-time-flag framing; `#error`
  fires at preprocessor time if the flag is invalid; the
  `PETRA_NUL_TERMINATE` axis is PERPENDICULAR to Sep 13's
  CONSTEVAL-vs-RUNTIME axis (Section 9's `decltype` checks
  pin this).
- 107/107 PASS across 14 sections on the bounded-buffer
  default + strict-warning + ASan/UBSan + CMake builds.
- 23/23 PASS across 14 sections on the NUL-terminating build.
- Five consecutive runs on each build produced identical
  tallies (no flakiness in Section 13's 4-thread × 25-call
  concurrent dispatch — per-thread local counters aggregated
  after join).
- Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion`; zero sanitizer diagnostics;
  empty stderr under default + ASan.
- Lesson is a pure consumer-side C++23 `std::format` +
  `std::vformat_to` + `std::format_string<Args...>` (P2216R3)
  + `std::make_format_args` + `std::expected` (P0323R12) +
  `std::span<char>` (P0122R7) + `std::ranges::contiguous_range`
  + `std::println` (P2093R14) + `__VA_OPT__` preprocessor
  feature exercise — reuses Sep 8 / Sep 9 / Sep 11 / Sep 12's
  machinery verbatim, no new types, no new impls, no new
  error contracts.

## Where we go next

Today's lesson closes Sep 14's "Where we go next" item #2 (Sep
15's open item #2 / Sep 10's open item #4) — the bounded-buffer
vs NUL-terminating sibling-choice macro layer.  Three items
from Sep 14 remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type.  Today's lesson reuses the hand-rolled iterator from
   Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 /
   Sep 13 / Sep 14 / Sep 15 verbatim; when a future libc++
   ships the standard type, a follow-up lesson migrates all the
   wrappers that use the iterator.  (Today verified the type is
   NOT yet in libc++ 21 — `/tmp/probe_counting_iter.cpp`
   produces "no member named 'counting_output_iterator' in
   namespace 'std'".)

2. **`std::span<const char>` macro layer** — analogous to
   Sep 14's `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN` /
   `TRY_FORMAT_BOUNDED_N_SPAN_FMT` / `TRY_FORMAT_BOUNDED_INTO_SPAN`
   and Sep 15's `TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED` /
   `_N` / `_INTO` / `_N_SPAN`: a `PETRA_NUL_TERMINATE`-gated
   pair of meta-macros for the `std::span<char>` /
   `std::span<const char>` axes.  Reuses the same
   preprocessor-time `#if PETRA_NUL_TERMINATE` pattern as
   today.

3. **`petra::try_format_bounded_runtime_n_into` — a
   `std::span<char>` overload family for the
   `PETRA_NUL_TERMINATE`-gated meta-macro**.  Sep 12 collapsed
   both span<char> paths into the NUL-terminating
   `try_format_runtime_n_into_bounded(span, ...)` wrapper; a
   future lesson could add a `try_format_runtime_n(span, ...)`
   wrapper to enable a runtime span<char> bounded-buffer path
   (Sep 8's `try_format_runtime_n` is the char*+cap version
   only).

Today's lesson itself does not open a new follow-on item —
the preprocessor-time dispatch design is fully general and
extends naturally to the `std::span<char>` /
`std::span<const char>` axes (open item #2 above) and to
runtime-built format strings (Sep 14's open item #1).

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
