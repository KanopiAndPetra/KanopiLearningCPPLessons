# P-2026-10-04 — `try_format_dash_pad_span_n`

Topic: **the SEVENTH axis (PETRA_DASH_PAD) sibling-choice
META-MACRO layer on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis** —
the STD::SPAN<CHAR> MIRROR of Oct 3's `try_format_dash_pad_n` char*+cap
lesson.  TODAY introduces a NEW FOURTH compile-time flag
(PETRA_DASH_PAD) on top of Oct 1's 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD).

**Closes Oct 3's "Where we go next" item (1) verbatim**:

> STD::SPAN<CHAR> MIRROR of today's DASH-pad lesson — would
> be the span<char> mirror of today's char*+cap lesson (the
> SEVENTH axis on the std::span<char> side).  Closes the
> (0,0,0,1), (0,0,1,1), (1,0,0,1), and (1,0,1,1) cells on
> the span<char> side.  Each future lesson adds verbatim thin
> DASH-pad wrappers composed on top of today's DASH-pad
> char*+cap wrappers.  (4 new dispatch cells per axis
> extension.)

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-vs-DASH) quadrant on the `std::span<char>` side
is now COMPLETE for all 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD, PETRA_DASH_PAD) with
PETRA_PAD=0 (the PETRA_PAD=1 cells live in Sep 27's / Oct 1's
lessons and are a `#error` in this lesson).

## Key findings

- The DASH-pad span<char> sibling is a **thin layer on top of
  Sep 28's SPACE-pad span<char> sibling**.  The composition
  rule "the LATER fill overwrites the EARLIER fill" applies:
  the DASH-pad wrapper calls the SPACE-pad sibling (which
  fills bytes `[written, span.size())` with `' '` on success),
  then overwrites those `' '` bytes with `'-'` on success.

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
  the `(SPACE_PAD=0, DASH_PAD=1)` DASH-pad span<char>
  wrappers.

- The DASH-pad wrapper **preserves buffer atomicity on
  TooLarge** — the buffer is UNTOUCHED on overflow.  The
  expected sibling short-circuits before the dash-fill loop
  runs (`if (r.has_value())` guard); the throwing sibling
  re-raises the `std::runtime_error` untouched (the
  `catch (...) { throw; }` block in the throwing wrapper).

- The DASH-pad wrapper **preserves the SAME byte-count
  contract on success** — returns `std::size_t` (throwing
  sibling) or `expected<std::size_t, FormatError>` (expected
  sibling) equal to the bytes written by the underlying
  format (NOT the number of bytes padded).

- The DASH-pad wrapper's **byte-state contract on success**:
  bytes `[written, span.size())` are `'-'` (NOT `' '` and NOT
  `'\0'` and NOT `'Q'`).  Pinned by Section 16 with an
  explicit 4-distinctness check (`buf[i] == '-'`,
  `buf[i] != ' '`, `buf[i] != '\0'`, `buf[i] != 'Q'`) on the
  (0,0,0,1), (0,0,1,1), (1,0,0,1), and (1,0,1,1) cells.

- The DASH-pad wrapper **handles the `span.size()==0` edge
  case correctly** — the `for (i = written; i < cap; ++i)`
  loop has `i < cap == 0` as the initial condition, so the
  loop body never executes.  Section 2c pins this with a
  cap=0 + empty-fmt call (the sentinel 'Q' byte outside the
  cap=0 writable region remains untouched).

- The DASH-pad wrapper **handles the `written==cap` edge case
  correctly** — the `for (i = written; i < cap; ++i)` loop
  has `i == cap` as the initial condition, so the loop body
  never executes.  Section 2d pins this with format
  "ABCDEFGH" (8 bytes) into a span of size 8 (full-buffer
  success).

- The CONSTEVAL macro's **consteval gate is preserved
  regardless of the 3-bit tuple** —
  `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` still requires
  `std::format_string<Args...>` (a literal fmt at the call
  site).  Pinned by `/tmp/probe_consteval_dash_pad_span.cpp`
  producing `'error: call to consteval function
  std::basic_format_string<char, int>::basic_format_string<std::string>
  is not a constant expression'` with the note
  `'read of non-constexpr variable fmt_runtime is not allowed
  in a constant expression'`.

- The throwing+DASH-pad sibling **re-raises as
  `std::runtime_error` (NOT `std::format_error`)** — pinned
  by Section 15 with a
  `dynamic_cast<const std::format_error*>(&e)` check that
  returns `nullptr` (confirming the throwing wrapper
  re-raises as `std::runtime_error`).

- The DASH-pad span<char> sibling **preserves the std::span<char>
  sub-span "no-bleed" contract** — bytes outside the span
  are UNTOUCHED on ALL paths.  Pinned by Section 2a (a
  6-byte sub-span at offset 5 of a 16-byte backing buffer
  with format 'ABCDE' (5 bytes) leaves bytes [0,5) and
  [11,16) as poison 'Q' regardless of the flag triple) and
  Section 17 (an explicit sub-span no-bleed check with a
  different span offset).

## What today's lesson adds

- **FOUR new DASH-pad span<char> sibling wrappers**:
  `petra::try_format_runtime_n_dash_pad_span` (RUNTIME
  expected+DASH),
  `petra::try_format_bounded_runtime_n_dash_pad_span`
  (CONSTEVAL expected+DASH),
  `petra::try_format_runtime_n_throwing_dash_pad_span`
  (RUNTIME throwing+DASH),
  `petra::try_format_bounded_runtime_n_throwing_dash_pad_span`
  (CONSTEVAL throwing+DASH).  Each is a thin layer that
  calls the corresponding Sep 28 SPACE-pad span<char>
  sibling and overwrites `[written, span.size())` with `'-'`
  on success ONLY.  No new types, no new impls, no new
  error contracts beyond the underlying SPACE-pad sibling's
  contract.

- **TWO new meta-macros**:
  `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` (CONSTEVAL
  surface) and
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW` (RUNTIME
  surface).  Each is a verbatim thin dispatcher that picks
  between EIGHT sibling wrappers at preprocessor time based
  on the `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
  PETRA_SPACE_PAD, PETRA_DASH_PAD)` 4-bit tuple.
  `PETRA_PAD` MUST be 0 in this lesson (the dispatcher
  `#error`s with a clear pointer to Sep 27 / Oct 1 when
  `PETRA_PAD=1`, since Sep 27's / Oct 1's NUL-padded
  span<char> wrappers are out of scope for this lesson).
  Each macro is a parenthesized expression (NOT
  `do { } while (0)`) so the caller can write `auto n =
  TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(...)` and
  capture the result.  Each macro uses `__VA_OPT__(,)` for
  the no-args case.

## How today's lesson composes with the prior chain

- Sep 6/7/8/9 introduced the foundation:
  `try_format_runtime_n` (char*+cap RUNTIME, returns
  `expected<std::size_t, FormatError>`),
  `try_format_bounded_runtime_n` (char*+cap CONSTEVAL,
  returns `expected<std::size_t, FormatError>`), and the
  `counting_output_iterator` / `bounded_char_writer`
  machinery.

- Sep 10 introduced the std::span<char> CONSTEVAL surface:
  `try_format_bounded_runtime_n_span` (returns
  `expected<std::size_t, FormatError>`).

- Sep 19 introduced the std::span<char> RUNTIME surface:
  `try_format_runtime_n(span, ...)` (returns
  `expected<std::size_t, FormatError>`).

- Sep 20 introduced the THROWING char*+cap siblings:
  `try_format_runtime_n_throwing` and
  `try_format_bounded_runtime_n_throwing`.  These are the
  base of today's THROWING+DASH-pad path.

- Sep 21 introduced the THROWING span<char> siblings:
  `try_format_runtime_n_throwing(span, ...)` and
  `try_format_bounded_runtime_n_throwing_span`.  These are
  the base of today's THROWING+DASH-pad span<char> path.

- Sep 25 / Sep 26 / Sep 27 / Oct 1 introduced the
  NUL-padded siblings (PETRA_PAD=1 axis).  These are out
  of scope for today's lesson (PETRA_PAD=1 is a #error).

- Sep 28 introduced the SPACE-padded span<char> siblings
  (PETRA_SPACE_PAD=1 axis).  These are the base of today's
  DASH-pad path (DASH-pad composes on top of SPACE-pad).

- Sep 29 introduced the SPACE-padded char*+cap siblings
  (PETRA_SPACE_PAD=1 axis) on the char*+cap side.
  Parallel to Sep 28.

- Sep 30 introduced the PAD × SPACE_PAD COMPOSITION on the
  char*+cap side.

- Oct 1 introduced the PAD × SPACE_PAD COMPOSITION on the
  std::span<char> side (the SPAN<char> mirror of Sep 30).

- Oct 3 introduced the DASH-pad (PETRA_DASH_PAD=1 axis) on
  the char*+cap side.  The SEVENTH axis on the char*+cap
  side.  Today's lesson is the SPAN<char> mirror of Oct 3 —
  the SEVENTH axis on the std::span<char> side.

## Where we go next

TODAY's lesson closes Oct 3's "Where we go next" item (1)
verbatim: the SEVENTH axis (PETRA_DASH_PAD) sibling-choice
macro layer on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-vs-DASH) quadrant on the std::span<char>
side is now COMPLETE for all 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD, PETRA_DASH_PAD)
with PETRA_PAD=0.

TODAY's lesson does NOT cover the OTHER open items from
the matrix:

1. **PAD × DASH composition cells (PETRA_PAD=1,
   PETRA_DASH_PAD=1) on the STD::SPAN<CHAR> axis** — would
   close the (0,1,0,1), (0,1,1,1), (1,1,0,1), and
   (1,1,1,1) cells on the span<char> side (the PAD ×
   DASH_PAD composition cells).  Each future lesson adds
   verbatim thin DASH-pad wrappers composed on top of
   Sep 27's NUL-padded span<char> siblings + new
   `#if PETRA_DASH_PAD`-gated meta-macros.  (4 new dispatch
   cells per axis extension.)  This is the SPAN<char>
   mirror of Sep 30's char*+cap PAD × SPACE_PAD composition
   lesson, but for the DASH axis.

2. **PAD × SPACE × DASH composition cells (PETRA_PAD=1,
   PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1) on the
   STD::SPAN<CHAR> axis** — would close the (0,1,1,1) and
   (1,1,1,1) cells on the span<char> side (the triple
   composition cells where DASH overwrites SPACE overwrites
   NUL).  Each future lesson would compose the existing
   wrappers (Sep 27's NUL-padded span<char> + Sep 28's
   SPACE-padded span<char> + today's DASH-padded
   span<char>) and add `#if`-gated meta-macros for the
   triple-composition dispatch cells.  This is the
   SPAN<char> mirror of Oct 1's char*+cap PAD × SPACE_PAD
   composition lesson, but for the DASH axis.

3. **ADDITIONAL-PAD cells** (e.g. `PETRA_HASH_PAD` that
   pads with `'#'`, or `PETRA_UNDERSCORE_PAD` that pads
   with `'_'`) — would introduce an EIGHTH axis.  Each
   future lesson adds verbatim thin hash-pad wrappers
   composed on top of TODAY's DASH-pad wrappers + new
   `#if PETRA_HASH_PAD`-gated meta-macros.  (8 new dispatch
   cells per axis extension per side.)  The pattern is
   already well-established by Sep 28 / Sep 29 / Sep 30 /
   Oct 1 / Oct 3 / TODAY.

4. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited once the
   matrix is fully closed on both sides.

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add`
  (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` +
  `dynamic_cast` + `std::is_base_of_v` (catch-chain
  contract pinning).
- `std::array<std::thread, 4>` + `std::thread::join()`
  (concurrency).
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range` +
  `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v` (span<char> properties).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default (expected+untouched sibling; PETRA_THROW=0,
PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0):

```bash
cd late-may/cpp_practice/try_format_dash_pad_span_n
cmake -S . -B build
cmake --build build
./build/P-2026-10-04-try-format-dash-pad-span-n
```

Strict-warning build:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-04-try-format-dash-pad-span-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-04-try-format-dash-pad-span-n
```

DASH-pad cells (PETRA_DASH_PAD=1):

```bash
cmake -S . -B build-dash-pad -DPETRA_DASH_PAD=ON
cmake --build build-dash-pad
./build-dash-pad/P-2026-10-04-try-format-dash-pad-span-n
```

SPACE+DASH composition cells (PETRA_SPACE_PAD=1,
PETRA_DASH_PAD=1):

```bash
cmake -S . -B build-space-dash \
      -DPETRA_SPACE_PAD=ON -DPETRA_DASH_PAD=ON
cmake --build build-space-dash
./build-space-dash/P-2026-10-04-try-format-dash-pad-span-n
```

Throwing DASH-pad cells (PETRA_THROW_ON_OVERFLOW=1,
PETRA_DASH_PAD=1):

```bash
cmake -S . -B build-throw-dash \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_DASH_PAD=ON
cmake --build build-throw-dash
./build-throw-dash/P-2026-10-04-try-format-dash-pad-span-n
```

Direct compile (no CMake):

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-04-try-format-dash-pad-span-n.cpp \
        -o /tmp/P-2026-10-04-try-format-dash-pad-span-n
/tmp/P-2026-10-04-try-format-dash-pad-span-n
```

## Observed output

(0, 0, 0, 0) expected+untouched default build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 177  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 0, 1) expected+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (expected+DASH-padded sibling — NEW today)
=========================================================
PASS: 189  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 1, 0) expected+SPACE-padded build (regression — Sep 28):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (expected+SPACE-padded sibling)
=========================================================
PASS: 185  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 1, 1) expected+SPACE+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (expected+SPACE+DASH-padded sibling — NEW today — DASH overwrites SPACE)
=========================================================
PASS: 189  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 0, 0) throwing+untouched build (regression — Sep 21):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (throwing+untouched sibling)
=========================================================
PASS: 165  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 0, 1) throwing+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (throwing+DASH-padded sibling — NEW today)
=========================================================
PASS: 177  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 1, 0) throwing+SPACE-padded build (regression — Sep 28):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (throwing+SPACE-padded sibling)
=========================================================
PASS: 173  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 1, 1) throwing+SPACE+DASH-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (throwing+SPACE+DASH-padded sibling — NEW today — DASH overwrites SPACE)
=========================================================
PASS: 177  FAIL: 0
=========================================================
OVERALL: PASS
```

Five consecutive runs on each build produced identical
PASS/FAIL tallies (no flakiness in Section 10's 4-thread ×
25-call concurrent dispatch — per-thread local counters
aggregated after join via `std::atomic<int>::fetch_add`).

Note the count-equality pattern: the (0,0,1,1) and
(1,0,1,1) counts match the (0,0,0,1) and (1,0,0,1) counts
exactly (189 and 177 respectively).  This is the
**composition-rule signature**: the (0,0,1,1) and (1,0,1,1)
cells dispatch to the SAME sibling wrappers as the
(0,0,0,1) and (1,0,0,1) cells — no NEW wrappers were
added, no NEW test sections were added for the
composition-rule byte-state, so the PASS counts are the
same as the (0,0,0,1) and (1,0,0,1) counts.

The (0,0,0,0) count (177) is LESS than the (0,0,0,1) count
(189) because the (0,0,0,1) cell adds the DASH-pad-specific
checks in Sections 2 (16-CHECK DASH-pad suffix check vs
11-CHECK UNTOUCHED suffix check on Section 2's success
case), 16 (4-distinctness check on the (0,0,0,1) path),
and the composition-rule checks.  This is the SAME pattern
as Oct 3's char*+cap lesson.

Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`; zero sanitizer diagnostics; empty
stderr under default + ASan/UBSan + strict-warning builds
(verified with `cmake -S . -B build + cmake -S . -B
build-strict -DENABLE_STRICT_WARNINGS=ON + cmake -S . -B
build-asan -DENABLE_ASAN=ON + cmake -S . -B build-dash-pad
-DPETRA_DASH_PAD=ON + cmake -S . -B build-space-dash
-DPETRA_SPACE_PAD=ON -DPETRA_DASH_PAD=ON`).

The CONSTEVAL macro's consteval gate is preserved regardless
of the 3-bit tuple —
`TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` still requires
`std::format_string<Args...>` (a literal fmt at the call
site); pinned by `/tmp/probe_consteval_dash_pad_span.cpp`
producing
`'error: call to consteval function std::basic_format_string<char, int>::basic_format_string<std::string> is not a constant expression'`
with the note
`'read of non-constexpr variable fmt_runtime is not allowed
in a constant expression' / 'in call to this->data()'`.

The throwing+DASH-pad sibling preserves the SAME
`std::runtime_error` .what() byte-exactness — TODAY's
wrapper does NOT construct a new `std::runtime_error`
message; the throw TYPE and .what() message are both
delegated to Sep 28's throwing+SPACE-pad span<char> wrapper
(which delegates to Sep 21's throwing wrapper, which
constructs `std::runtime_error` with the SAME .what()`
message that `FormatError{TooLarge, ...}` would carry);
Section 15's `dynamic_cast<const std::format_error*>(&e)`
check returns `nullptr`, confirming that the throwing
wrapper re-raises as `std::runtime_error` (NOT
`std::format_error`).

The PETRA_PAD=1 friendly error is verified at CMake time
(via the `PETRA_PAD` cache var) AND at compile time (via
the `#if PETRA_PAD != 0` `#error` in the source).  Both
fire with a clear pointer to Sep 27 / Oct 1.

## How today's lesson is the SPAN<char> mirror of Oct 3's
char*+cap lesson

The structure of today's lesson is the SPAN<char> mirror of
Oct 3's char*+cap lesson.  The mapping is:

| Oct 3 char*+cap                          | Today span<char>                          |
|------------------------------------------|-------------------------------------------|
| `try_format_runtime_n` (Sep 8)           | `try_format_runtime_n(span)` (Sep 19)     |
| `try_format_bounded_runtime_n` (Sep 9)   | `try_format_bounded_runtime_n_span` (Sep 10) |
| `try_format_runtime_n_throwing` (Sep 20) | `try_format_runtime_n_throwing(span)` (Sep 21) |
| `try_format_bounded_runtime_n_throwing` (Sep 20) | `try_format_bounded_runtime_n_throwing_span` (Sep 21) |
| `try_format_runtime_n_space_pad` (Sep 29) | `try_format_runtime_n_space_pad_span` (Sep 28) |
| `try_format_bounded_runtime_n_space_pad` (Sep 29) | `try_format_bounded_runtime_n_space_pad_span` (Sep 28) |
| `try_format_runtime_n_throwing_space_pad` (Sep 29) | `try_format_runtime_n_throwing_space_pad_span` (Sep 28) |
| `try_format_bounded_runtime_n_throwing_space_pad` (Sep 29) | `try_format_bounded_runtime_n_throwing_space_pad_span` (Sep 28) |
| `try_format_runtime_n_dash_pad` (Oct 3)  | `try_format_runtime_n_dash_pad_span` (today) |
| `try_format_bounded_runtime_n_dash_pad` (Oct 3) | `try_format_bounded_runtime_n_dash_pad_span` (today) |
| `try_format_runtime_n_throwing_dash_pad` (Oct 3) | `try_format_runtime_n_throwing_dash_pad_span` (today) |
| `try_format_bounded_runtime_n_throwing_dash_pad` (Oct 3) | `try_format_bounded_runtime_n_throwing_dash_pad_span` (today) |
| `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW` (Oct 3) | `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` (today) |
| `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW` (Oct 3) | `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW` (today) |

The new wrappers' semantics are byte-for-byte identical
(they fill bytes `[written, span.size())` with `'-'` on
success ONLY; preserve buffer atomicity on error; preserve
the byte-count contract on success; preserve the consteval
gate on the CONSTEVAL surface; preserve the
`std::runtime_error` re-raise on the throwing surface).
The only differences are the parameter type (span<char> vs
char*+cap) and the std::span<char> sub-span "no-bleed"
contract, which the char*+cap lesson does not need to test
(the char*+cap lesson's "no buffer bleed" test is a
different shape because char*+cap has no sub-range concept
— it always uses the full cap from offset 0).

PLATFORM NOTE: This file uses Sep 30's / Oct 1's / Oct 3's
PROVEN callback-based impl (the 'cb' lambda +
`std::move(counter)` pattern) — NOT the no-callback
`counter.count()` pattern from a prior untracked
`P-2026-10-02` draft.  The no-callback pattern was found to
NOT work on Apple Clang 21 / libc++ 21 because
`std::vformat_to` internally COPIES the output iterator
(the proxy's `it_` pointer ends up pointing to a copy, not
the original), so `counter.count()` and `writer.written()`
read 0 after the vformat_to call even though the proxy's
`operator=` was invoked (the count updates went to the
discarded copy).  The callback pattern avoids this because
the callback writes through a `void* data` pointer to a
local `std::size_t needed` / `std::size_t written` in the
impl, which the vformat_to copies do NOT see — only the
local in the impl is updated.  See
`/tmp/probe_vformat_to_copy.cpp` (from Oct 3) for the full
investigation.
