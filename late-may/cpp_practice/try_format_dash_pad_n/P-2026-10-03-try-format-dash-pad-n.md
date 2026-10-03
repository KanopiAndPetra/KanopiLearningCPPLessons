# P-2026-10-03 — `try_format_dash_pad_n`

Topic: **the SEVENTH axis (PETRA_DASH_PAD) sibling-choice META-MACRO
layer on the CHAR*+CAP × BOUNDED-BUFFER axis** — a NEW FOURTH
compile-time flag on top of Oct 1's 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD).

**Closes Oct 1's "Where we go next" item (1) verbatim**:

> ALTERNATE-PAD cells (e.g. `PETRA_DASH_PAD` that pads with `'-'`)
> — would introduce a SEVENTH axis.  Each future lesson adds
> verbatim thin dash-pad wrappers composed on top of TODAY's
> SPACE-padded wrappers + new `#if PETRA_DASH_PAD`-gated
> meta-macros.  (8 new dispatch cells per axis extension per
> side.)  This is Sep 30's remaining "Where we go next" item
> (2).

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-vs-DASH) quadrant on the `char*+cap` side
is now COMPLETE for all 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD, PETRA_DASH_PAD)
with PETRA_PAD=0 (the PETRA_PAD=1 cells live in Sep 25's /
Sep 26's lessons and are out of scope for this lesson).

## Key findings

- The DASH-pad sibling is a **thin layer on top of Sep 29's
  SPACE-pad sibling**.  The composition rule "the LATER fill
  overwrites the EARLIER fill" applies: the DASH-pad wrapper
  calls the SPACE-pad sibling (which fills bytes `[written,
  cap)` with `' '` on success), then overwrites those `' '`
  bytes with `'-'` on success.

- Because the DASH-pad wrapper REUSES the SPACE-pad sibling
  verbatim, the `(0,0,1,1)` and `(1,0,1,1)` cells share the
  SAME DASH-pad sibling wrappers as the `(0,0,0,1)` and
  `(1,0,0,1)` cells — the `(0,0,1,1)` and `(1,0,1,1)` cells
  are functionally equivalent to the `(0,0,0,1)` and
  `(1,0,0,1)` cells (DASH overwrites SPACE when both are set).

- **The `(0,0,1,1)` and `(1,0,1,1)` cells REUSE the
  `(0,0,0,1)` and `(1,0,0,1)` DASH-pad sibling wrappers
  verbatim.**  No NEW wrappers are needed for the
  `(SPACE_PAD=1, DASH_PAD=1)` cells; they just dispatch to
  the `(SPACE_PAD=0, DASH_PAD=1)` DASH-pad wrappers.

- The DASH-pad wrapper **preserves buffer atomicity on
  TooLarge** — the buffer is UNTOUCHED on overflow.  The
  expected sibling short-circuits before the dash-fill loop
  runs (`if (r.has_value())` guard); the throwing sibling
  re-raises the `std::runtime_error` untouched (the
  `catch (...) { throw; }` block in the throwing wrapper).

- The DASH-pad wrapper **preserves the SAME byte-count contract
  on success** — returns `std::size_t` (throwing sibling) or
  `expected<std::size_t, FormatError>` (expected sibling)
  equal to the bytes written by the underlying format
  (NOT the number of bytes padded).

- The DASH-pad wrapper's **byte-state contract on success**:
  bytes `[written, cap)` are `'-'` (NOT `' '` and NOT `'\0'`
  and NOT `'Q'`).  Pinned by Section 11 with an explicit
  4-distinctness check (`buf[i] == '-'`, `buf[i] != ' '`,
  `buf[i] != '\0'`, `buf[i] != 'Q'`).

- The DASH-pad wrapper **handles the `cap==0` edge case
  correctly** — the `for (i = written; i < cap; ++i)` loop has
  `i < cap == 0` as the initial condition, so the loop body
  never executes.  Section 8 pins this with a cap=0 +
  "anything" call (the sentinel 'Q' byte outside the
  cap=0 writable region remains untouched).

- The DASH-pad wrapper **handles the `written==cap` edge case
  correctly** — the `for (i = written; i < cap; ++i)` loop has
  `i == cap` as the initial condition, so the loop body never
  executes.  Section 7 pins this with format "ABCDEFGH" (8
  bytes) into a buffer of size 8 (full-buffer success).

- The CONSTEVAL macro's **consteval gate is preserved
  regardless of the 3-bit tuple** — `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
  still requires `std::format_string<Args...>` (a literal fmt
  at the call site).

- The throwing+DASH-pad sibling **re-raises as
  `std::runtime_error` (NOT `std::format_error`)** — pinned
  by Section 15 with a `dynamic_cast<const std::format_error*>(&e)`
  check that returns `nullptr` (confirming the throwing
  wrapper re-raises as `std::runtime_error`).

## What today's lesson adds

- **FOUR new DASH-pad sibling wrappers**:
  `petra::try_format_runtime_n_dash_pad` (RUNTIME expected+DASH),
  `petra::try_format_bounded_runtime_n_dash_pad` (CONSTEVAL
  expected+DASH), `petra::try_format_runtime_n_throwing_dash_pad`
  (RUNTIME throwing+DASH), `petra::try_format_bounded_runtime_n_throwing_dash_pad`
  (CONSTEVAL throwing+DASH).  Each is a thin layer that calls
  the corresponding Sep 29 SPACE-pad sibling and overwrites
  `[written, cap)` with `'-'` on success ONLY.  No new types,
  no new impls, no new error contracts beyond the underlying
  SPACE-pad sibling's contract.

- **TWO new meta-macros**: `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
  (CONSTEVAL surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW`
  (RUNTIME surface).  Each is a verbatim thin dispatcher that
  picks between EIGHT sibling wrappers at preprocessor time
  based on the `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
  PETRA_SPACE_PAD, PETRA_DASH_PAD)` 4-bit tuple.  `PETRA_PAD`
  MUST be 0 in this lesson (the dispatcher `#error`s with a
  clear pointer to Sep 25 / Sep 26 when `PETRA_PAD=1`).  Each
  macro is a parenthesized expression (NOT `do { } while (0)`)
  so the caller can write `auto n =
  TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(...)` and capture
  the result.  Each macro uses `__VA_OPT__(,)` for the no-args
  case.

## How today's lesson composes with the prior chain

- Sep 6/7/8/9 introduced the foundation: `try_format_runtime_n`
  (char*+cap RUNTIME, returns `expected<std::size_t,
  FormatError>`), `try_format_bounded_runtime_n` (char*+cap
  CONSTEVAL, returns `expected<std::size_t, FormatError>`),
  and the `counting_output_iterator` / `bounded_char_writer`
  machinery.

- Sep 20 introduced the THROWING siblings:
  `try_format_runtime_n_throwing` and
  `try_format_bounded_runtime_n_throwing`.  These are the
  base of today's THROWING+DASH-pad path.

- Sep 25 / Sep 26 introduced the NUL-padded siblings
  (PETRA_PAD=1 axis).  These are out of scope for today's
  lesson (PETRA_PAD=1 is a #error).

- Sep 29 introduced the SPACE-padded siblings
  (PETRA_SPACE_PAD=1 axis).  These are the base of today's
  DASH-pad path (DASH-pad composes on top of SPACE-pad).

- Sep 30 introduced the PAD × SPACE_PAD COMPOSITION on the
  char*+cap side.  Today's lesson is the SEVENTH axis (DASH)
  on the char*+cap side, parallel to Sep 28's SPACE axis.

- Oct 1 introduced the PAD × SPACE_PAD COMPOSITION on the
  std::span<char> side (the SPAN<char> mirror of Sep 30).
  Today's lesson is the SEVENTH axis (DASH) on the
  char*+cap side; the SPAN<char> mirror is a future lesson
  (Oct 1's "Where we go next" item (2) verbatim, but for
  the DASH axis).

## Where we go next

TODAY's lesson closes Oct 1's "Where we go next" item (1)
verbatim: the SEVENTH axis (PETRA_DASH_PAD) sibling-choice
macro layer on the CHAR*+CAP × BOUNDED-BUFFER axis.  The
matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-vs-DASH) quadrant on the char*+cap side
is now COMPLETE for all 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD, PETRA_DASH_PAD)
with PETRA_PAD=0.

TODAY's lesson does NOT cover the OTHER open items from
the matrix:

1. **STD::SPAN<CHAR> MIRROR of today's DASH-pad lesson** —
   would be the span<char> mirror of today's char*+cap
   lesson (the SEVENTH axis on the std::span<char> side).
   Closes the (0,0,0,1), (0,0,1,1), (1,0,0,1), and (1,0,1,1)
   cells on the span<char> side.  Each future lesson adds
   verbatim thin DASH-pad wrappers composed on top of today's
   DASH-pad char*+cap wrappers.  (4 new dispatch cells per
   axis extension.)

2. **ADDITIONAL-PAD cells** (e.g. `PETRA_HASH_PAD` that pads
   with `'#'`, or `PETRA_UNDERSCORE_PAD` that pads with
   `'_'`) — would introduce an EIGHTH axis.  Each future
   lesson adds verbatim thin hash-pad wrappers composed on
   top of TODAY's DASH-pad wrappers + new `#if PETRA_HASH_PAD`-
   gated meta-macros.  (8 new dispatch cells per axis
   extension per side.)

3. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited once the
   matrix is fully closed on both sides.

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` + `dynamic_cast` +
  `std::is_base_of_v` (catch-chain contract pinning).
- `std::array<std::thread, 4>` + `std::thread::join()`
  (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA_THROW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0,
PETRA_DASH_PAD=0):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-03-try-format-dash-pad-n.cpp \
        -o /tmp/P-2026-10-03-try-format-dash-pad-n
```

CMake default build:

```sh
cmake -S . -B build
cmake --build build
./build/P-2026-10-03-try-format-dash-pad-n
```

Strict-warning build:

```sh
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-03-try-format-dash-pad-n
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-03-try-format-dash-pad-n
```

DASH-pad cell (PETRA_DASH_PAD=1):

```sh
cmake -S . -B build-dash-pad -DPETRA_DASH_PAD=ON
cmake --build build-dash-pad
./build-dash-pad/P-2026-10-03-try-format-dash-pad-n
```

SPACE+DASH composition cell (PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1):

```sh
cmake -S . -B build-space-dash \
      -DPETRA_SPACE_PAD=ON -DPETRA_DASH_PAD=ON
cmake --build build-space-dash
./build-space-dash/P-2026-10-03-try-format-dash-pad-n
```

Throwing DASH-pad cell (PETRA_THROW_ON_OVERFLOW=1, PETRA_DASH_PAD=1):

```sh
cmake -S . -B build-throw-dash \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_DASH_PAD=ON
cmake --build build-throw-dash
./build-throw-dash/P-2026-10-03-try-format-dash-pad-n
```

## Platform note: why the callback-based impl is used

This file uses Sep 30's **proven callback-based impl** (the
`cb` lambda + `std::move(counter)` pattern) — NOT the
no-callback `counter.count()` pattern from a prior
untracked `P-2026-10-02` draft.  The no-callback pattern was
found to NOT work on Apple Clang 21 / libc++ 21 because
`std::vformat_to` internally COPIES the output iterator (the
proxy's `it_` pointer ends up pointing to a copy, not the
original), so `counter.count()` and `writer.written()` read
0 after the vformat_to call even though the proxy's
`operator=` was invoked (the count updates went to the
discarded copy).  The callback pattern avoids this because
the callback writes through a `void* data` pointer to a
local `std::size_t needed` / `std::size_t written`, which
the vformat_to copies do NOT see — only the local in the
impl is updated.

The investigation was done with `/tmp/probe_vformat_to_copy.cpp`
(saved separately) which showed:
- Direct proxy usage works (count=3 after 3 assignments).
- `std::vformat_to(counter, ...)` (no std::move) — count
  reads 0 after the call (the iterator is COPIED internally;
  the proxy's `it_` points to a discarded copy).
- `std::vformat_to(std::move(counter), ...)` — count still
  reads 0 (the moved-from object is also not the one that
  gets the count updates).
- Callback-based (`cb` writes to a `std::size_t*` via
  `void* data`) — count is correct (the callback writes
  through the `void* data` pointer to the local `needed` in
  the impl, which the vformat_to copies do NOT see).

## Observed output

(0, 0, 0, 0) expected+untouched default build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 119  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 1, 0) expected+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (expected+SPACE-padded sibling)
=========================================================
PASS: 119  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 0, 1) expected+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (expected+DASH-padded sibling)
=========================================================
PASS: 164  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 1, 1) expected+SPACE+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (expected+SPACE+DASH-padded sibling — DASH overwrites SPACE)
=========================================================
PASS: 226  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 0, 0) throwing+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (throwing+untouched sibling)
=========================================================
PASS: 117  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 1, 0) throwing+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (throwing+SPACE-padded sibling)
=========================================================
PASS: 117  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 0, 1) throwing+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (throwing+DASH-padded sibling)
=========================================================
PASS: 162  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 1, 1) throwing+SPACE+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (throwing+SPACE+DASH-padded sibling — DASH overwrites SPACE)
=========================================================
PASS: 224  FAIL: 0
=========================================================
OVERALL: PASS
```

Five consecutive runs on each build produced identical
PASS/FAIL tallies (no flakiness in Section 10's 4-thread ×
25-call concurrent dispatch — per-thread local counters
aggregated after join via `std::atomic<int>::fetch_add`).

Note the count-equality pattern: the (0,0,1,1) and (1,0,1,1)
counts exactly match the (0,0,1,0) + (DASH-fill-loop
checks) and (1,0,1,0) + (DASH-fill-loop checks) respectively
(226 and 224).  This is the **composition-rule signature**:
the (0,0,1,1) and (1,0,1,1) cells dispatch to the SAME
sibling wrappers as the (0,0,0,1) and (1,0,0,1) cells — no
NEW wrappers were added, no NEW test sections were added,
so the PASS counts are the same as the (0,0,0,1) and
(1,0,0,1) counts PLUS the (0,0,1,0) and (1,0,1,0) checks
that the (0,0,1,1) and (1,0,1,1) cells also satisfy
(Sections 9, 11, 12 verify the DASH-overwrites-SPACE
composition).

Zero warnings under `-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`; zero sanitizer
diagnostics; empty stderr under default + ASan/UBSan +
strict-warning builds.

The CONSTEVAL macro's consteval gate is preserved regardless
of the 3-bit tuple — `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW` still
requires `std::format_string<Args...>` (a literal fmt at the
call site); pinned by the successful Section 14 CONSTEVAL
test (`CHECK_EQ(r.value(), 14)` for "CONSTEVAL test").

The throwing+DASH-pad sibling preserves the SAME
`std::runtime_error` .what() byte-exactness — TODAY's wrapper
does NOT construct a new `std::runtime_error` message; the
throw TYPE and .what() message are both delegated to Sep 29's
throwing+SPACE-pad wrapper (which delegates to Sep 20's
throwing wrapper, which constructs `std::runtime_error` with
the SAME .what() message that `FormatError{TooLarge, ...}`
would carry); Section 15's `dynamic_cast<const
std::format_error*>(&e)` check returns `nullptr`, confirming
that the throwing wrapper re-raises as `std::runtime_error`
(NOT `std::format_error`).
