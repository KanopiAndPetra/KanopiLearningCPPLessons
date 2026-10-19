# P-2026-10-19 — try_format_at_pad_span_n

**Closes Oct 18's "Where we go next" item (2) verbatim — the
STD::SPAN\<CHAR\>-side NINTH pad-character axis (`PETRA_AT_PAD`).**

Today's lesson introduces `PETRA_AT_PAD` (a NEW NINTH compile-time
flag) on top of Oct 16's 8-bit tuple
(`PETRA_THROW_ON_OVERFLOW`, `PETRA_PAD`, `PETRA_SPACE_PAD`,
`PETRA_DASH_PAD`, `PETRA_HASH_PAD`, `PETRA_TILDE_PAD`,
`PETRA_UNDERSCORE_PAD`, `PETRA_STAR_PAD`) on the STD::SPAN\<CHAR\>
× BOUNDED-BUFFER axis.  `PETRA_AT_PAD=1` selects the AT-padded
`std::span<char>` sibling, which fills bytes
`[written, span.size())` with `'@'` (ASCII 0x40 = 64) on success.

The `'@'` byte is TOTALLY DIFFERENT from Oct 16's `'*'` (ASCII 42),
Oct 15's `'_'` (ASCII 95), Oct 13's `'~'` (ASCII 126), Oct 11's
`'#'` (ASCII 35), Oct 4's `'-'` (ASCII 45), Sep 28's `' '`
(ASCII 32), and Sep 27's `'\0'` (ASCII 0).  After today, the
STD::SPAN\<CHAR\> × BOUNDED-BUFFER matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_AT_PAD)` 2-bit tuple is COMPLETE
for ALL FOUR cells, mirroring Oct 18's CHAR\*+CAP-side closure of
the same NINTH-axis pad layer.

The composition rule "the LATER fill overwrites the EARLIER fill"
now has EIGHT layers on BOTH axes:
`AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`.
When all eight pad-flags are set, the end byte state is always
`'@'`.  Composition cells with `(AT=1, STAR=*)` REUSE TODAY's NEW
AT-pad wrappers verbatim because AT overwrites STAR (which
overwrites UNDERSCORE which overwrites TILDE which overwrites
HASH which overwrites DASH which overwrites SPACE).

## Where this fits in the arc

```
   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
              std::size_t cap (RUNTIME std::string-output wrapper).
   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER × CHAR*+CAP ×
              RUNTIME sibling (returns std::size_t via expected).
   - Sep  9   try_format_bounded_runtime_n — BOUNDED-BUFFER ×
              CHAR*+CAP × CONSTEVAL sibling.
   - Sep 10   try_format_bounded_runtime_n_span — BOUNDED-BUFFER ×
              STD::SPAN<CHAR> × CONSTEVAL sibling.
   - Sep 19   try_format_runtime_n(span<char>) — BOUNDED-BUFFER ×
              STD::SPAN<CHAR> × RUNTIME sibling.
   - Sep 20   try_format_runtime_n_throwing / ...throwing —
              BOUNDED-BUFFER × CHAR*+CAP × THROWING siblings.
   - Sep 21   ...throwing(span<char>) / ...throwing_span —
              BOUNDED-BUFFER × STD::SPAN<CHAR> × THROWING siblings.
   - Sep 25   try_format_padded_n_or_into — FIRST axis
              (PETRA_PAD) sibling-choice macro layer on the
              CHAR*+CAP side (NUL-fill).
   - Sep 27   try_format_padded_span_n_or_throw — the
              STD::SPAN<CHAR> MIRROR of Sep 25 (NUL-fill).
   - Sep 28   try_format_space_pad_n / try_format_space_pad_span_n —
              (PETRA_SPACE_PAD) sibling wrappers on BOTH axes.
   - Oct  3   try_format_dash_pad_n — (PETRA_DASH_PAD) sibling
              wrapper on the CHAR*+CAP side.
   - Oct  4   try_format_dash_pad_span_n — the STD::SPAN<CHAR>
              MIRROR of Oct 3.
   - Oct  9   try_format_hash_pad_n — (PETRA_HASH_PAD) sibling
              wrapper on the CHAR*+CAP side.
   - Oct 11   try_format_hash_pad_span_n — the STD::SPAN<CHAR>
              MIRROR of Oct 9.
   - Oct 12   try_format_tilde_pad_n — (PETRA_TILDE_PAD) sibling
              wrapper on the CHAR*+CAP side.
   - Oct 13   try_format_tilde_pad_span_n — the STD::SPAN<CHAR>
              MIRROR of Oct 12.
   - Oct 14   try_format_underscore_pad_n — (PETRA_UNDERSCORE_PAD)
              sibling wrapper on the CHAR*+CAP side.
   - Oct 15   try_format_underscore_pad_span_n — the
              STD::SPAN<CHAR> MIRROR of Oct 14.
   - Oct 16   try_format_star_pad_span_n — (PETRA_STAR_PAD)
              sibling wrapper on the STD::SPAN<CHAR> side; the
              EIGHTH axis.
   - Oct 17   try_format_star_pad_n — (PETRA_STAR_PAD) sibling
              wrapper on the CHAR*+CAP side; brings the CHAR*+
              axis in line with STD::SPAN<CHAR> on the EIGHTH
              axis.
   - Oct 18   try_format_at_pad_n — (PETRA_AT_PAD) sibling
              wrapper on the CHAR*+CAP side; the NINTH axis.
   - TODAY    try_format_at_pad_span_n — (PETRA_AT_PAD) sibling
              wrapper on the STD::SPAN<CHAR> side; the NINTH
              axis on STD::SPAN<CHAR>.  Closes Oct 18's "Where
              we go next" item (2) verbatim.  Today's pad
              character is '@' (ASCII 64), TOTALLY DIFFERENT
              from Oct 16's '*' (ASCII 42), Oct 15's '_'
              (ASCII 95), Oct 13's '~' (ASCII 126), Oct 11's
              '#' (ASCII 35), Oct 4's '-' (ASCII 45), Sep 28's
              ' ' (ASCII 32), and Sep 27's '\0' (ASCII 0).
```

## What's in today's lesson

TODAY adds ONE NEW compile-time flag (`PETRA_AT_PAD`) on top of
Oct 16's helpers' axes.  When `PETRA_PAD = 1`, `PETRA_SPACE_PAD = 1`,
`PETRA_DASH_PAD = 1`, `PETRA_HASH_PAD = 1`, `PETRA_UNDERSCORE_PAD = 1`,
`PETRA_TILDE_PAD = 1`, or `PETRA_STAR_PAD = 1`, the lesson's
friendly-error fires at compile time with a clear pointer to
those lessons.

**The FOUR NEW sibling wrappers, on the STD::SPAN\<CHAR\> ×
BOUNDED-BUFFER × expected-vs-throwing axis:**

1. `petra::try_format_runtime_n_at_pad_span(std::span<char>,
   std::string_view, args...)` — NEW today (RUNTIME surface of
   the BOUNDED-BUFFER × EXPECTED × AT-PAD sibling on the
   STD::SPAN\<CHAR\> axis; wraps Oct 16's
   `try_format_runtime_n_star_pad_span` verbatim and at-fills
   `[written, span.size())` with `'@'` on success ONLY).

2. `petra::try_format_bounded_runtime_n_at_pad_span(std::span<char>,
   std::format_string<Args...>, args...)` — NEW today (CONSTEVAL
   surface of the BOUNDED-BUFFER × EXPECTED × AT-PAD sibling on
   the STD::SPAN\<CHAR\> axis; wraps Oct 16's
   `try_format_bounded_runtime_n_star_pad_span` verbatim and
   at-fills `[written, span.size())` with `'@'` on success ONLY).

3. `petra::try_format_runtime_n_throwing_at_pad_span(std::span<char>,
   std::string_view, args...)` — NEW today (RUNTIME surface of
   the BOUNDED-BUFFER × THROWING × AT-PAD sibling on the
   STD::SPAN\<CHAR\> axis; wraps Oct 16's
   `try_format_runtime_n_throwing_star_pad_span` verbatim and
   at-fills `[written, span.size())` with `'@'` on success ONLY;
   re-raises `std::runtime_error` untouched on error).

4. `petra::try_format_bounded_runtime_n_throwing_at_pad_span(
   std::span<char>, std::format_string<Args...>, args...)` —
   NEW today (CONSTEVAL surface of the BOUNDED-BUFFER ×
   THROWING × AT-PAD sibling on the STD::SPAN\<CHAR\> axis;
   wraps Oct 16's
   `try_format_bounded_runtime_n_throwing_star_pad_span` verbatim
   and at-fills `[written, span.size())` with `'@'` on success
   ONLY; re-raises `std::runtime_error` untouched on error).

**The TWO NEW meta-macros that tie the (THROW, AT) 2-bit tuple
at the call site:**

1. `TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW(out, fmt, ...)` — NEW
   today; a verbatim thin dispatcher that picks between the
   2 expected × 2 throwing = 4 AT-pad span\<char\> sibling
   wrappers at preprocessor time based on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD)` 2-bit tuple.

2. `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(out, fmt, ...)`
   — NEW today; the RUNTIME-surface counterpart of the CONSTEVAL
   meta-macro above.

Each macro is a parenthesized expression (NOT `do { } while (0)`)
so the caller can write `auto n =
TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW(...)` and capture
the result.  Each macro uses `__VA_OPT__(,)` for the no-args case
(mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 /
Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 /
Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 /
Oct 11 / Oct 13 / Oct 15 / Oct 16 / Oct 18's macro shape).

## HEADLINE finding

**The AT-pad span\<char\> sibling is a thin layer on top of Oct
16's STAR-pad span\<char\> sibling** (literally: it calls Oct 16's
wrapper, then OVERWRITES the `'*'` fill with `'@'` on success).

The composition rule "the LATER fill overwrites the EARLIER fill"
now has EIGHT layers on BOTH axes:
`AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`
(when all eight pad-flags are set, the end byte state is always
`'@'`).  The `(AT=1, STAR=*)` cells REUSE the `(AT=1, STAR=0)`
AT-pad span\<char\> sibling wrappers verbatim — no NEW wrappers are
needed for the composition cells.

## KEY DESIGN DECISIONS

- **SCOPE: NARROW** — this lesson is INTENTIONALLY NARROW on the
  STD::SPAN\<CHAR\> × BOUNDED-BUFFER × expected-vs-throwing axis
  only (`PETRA_PAD=0`, `PETRA_SPACE_PAD=0`, `PETRA_DASH_PAD=0`,
  `PETRA_HASH_PAD=0`, `PETRA_TILDE_PAD=0`, `PETRA_UNDERSCORE_PAD=0`,
  `PETRA_STAR_PAD=0`; STAR is BAKED INTO Oct 16's wrapper, which
  today's AT wrappers layer on top of — so STAR is implicitly
  present as the IMMEDIATELY-PRECEDING layer in the composition
  chain).  The `(PAD=1)`, `(SPACE_PAD=1)`, `(DASH_PAD=1)`,
  `(HASH_PAD=1)`, `(TILDE_PAD=1)`, `(UNDERSCORE_PAD=1)`, and
  `(STAR_PAD=1)` cells require Sep 25 / Sep 27 / Sep 28 / Sep 29 /
  Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
  Oct 13 / Oct 15 / Oct 16's helpers, which are NOT included in
  this file.  When any of those flags is non-zero, the lesson's
  dispatcher `#error`s with a clear pointer to those lessons.

- **SELF-CONTAINED** — like Oct 16 / Oct 18, today's lesson is
  INTENTIONALLY SELF-CONTAINED.  It contains the FULL helper
  stack (FormatError, the iterator primitives, the bounded
  writer, the underlying impl, and the Oct 16 STAR-pad span
  wrappers needed to BUILD the AT-pad span wrappers from the same
  source).  This is what makes today's lesson a viable
  single-file standalone exercise — verifiable from a single
  compile + run.

- **PROVEN callback-based impl** — the file uses Oct 11's PROVEN
  callback-based impl (the `cb` lambda + `std::move` pattern) —
  NOT the no-callback pattern from a prior untracked
  P-2026-10-02 draft.  The no-callback pattern was found to NOT
  work on Apple Clang 21 / libc++ 21 because `std::vformat_to`
  internally COPIES the output iterator.  See the platform note
  in Oct 11 / Oct 15 / Oct 16's source for the full investigation.

## KEY DESIGN DECISIONS (byte-cell contracts)

- **Buffer atomicity on TooLarge** — the AT-pad span\<char\>
  sibling wrapper does NOT touch bytes on the overflow path
  (Oct 16's STAR-pad wrapper already pins this on the untouched
  path; TODAY's AT-pad wrapper re-pins it on the AT-pad path
  because the new `for (i = written; i < cap; ++i) out[i] = '@';`
  loop COULD theoretically diverge if the `has_value()` guard
  were mis-coded); Section 2a pins this with an 8-byte buffer +
  `"ABCDEFGHIJ"` format that yields 10-byte TooLarge and verifies
  every byte is still poison `'Q'`.

- **Buffer atomicity on Format errors** — a malformed fmt throws
  `std::format_error` at format time before any writes (delegated
  to Oct 16's STAR-pad wrapper, which delegates to the underlying
  Sep 10 / Sep 19 / Sep 21 impl); Section 3 pins this with
  `std::string bad_fmt = "bad { "` and a 16-byte poison-`'Q'`
  buffer, asserting every byte is still `'Q'`.

- **Byte-count contract on success** — returns `std::size_t ==
  bytes written` (the same value Oct 16's STAR-pad span\<char\>
  sibling's Sep 10 / Sep 19 underlying RUNTIME impl returns);
  Section 1 pins this with format `"x={}"` + 42 into a 16-byte
  poison-`'Q'` buffer (`written=4`, content `"x=42"`, bytes
  `[4..15]` all `'@'`).

- **Byte-state contract on success** — bytes `[written,
  span.size())` are `'@'` (`0x40`) on success — pinned by
  Section 1 with a poison-`'Q'` check on every byte at index >=
  written; byte-state distinctness from STAR / UNDERSCORE /
  TILDE / HASH / DASH / SPACE / NUL — bytes `[written,
  span.size())` are `'@'` (NOT `'*'` and NOT `'_'` and NOT `'~'`
  and NOT `'#'` and NOT `'-'` and NOT `' '` and NOT `'\0'` and
  NOT `'Q'`) — is pinned by Section 5 with an explicit
  8-distinctness check on every byte at index >= written.

- **Sub-span 'no-bleed' contract** — bytes outside the span's
  writable region are UNTOUCHED — is pinned by Section 2b with
  a 6-byte sub-span at offset 5 of a 16-byte buffer with format
  `"ABCDE"` (5 bytes), verifying bytes `[0,5)` are still poison
  `'Q'` AND bytes `[11,16)` are still poison `'Q'` AND the byte
  at index 10 (the last byte in the sub-span) is `'@'`.

- **`written == span.size()` edge case** — the
  `for (i = written; i < span.size(); ++i)` loop has
  `i == span.size()` as the initial condition, so the loop body
  never executes; Section 4a pins this with format `"ABCDE"` (5
  bytes) into a span of size 5 (full-buffer success; every byte
  == `'A'-'E'`).

- **`span.size() == 0` edge case** — the
  `for (i = written; i < span.size(); ++i)` loop has
  `i < span.size() == 0` as the initial condition, so the loop
  body never executes; Section 4b pins this with
  `span.size()=0` + format `"X"` → TooLarge (needed=1,
  `span.size()=0`); the sentinel `'Q'` byte outside the
  `span.size()=0` writable region remains untouched.

- **CONSTEVAL macro's consteval gate is preserved** regardless
  of the (THROW, AT) 2-bit tuple —
  `TRY_FORMAT_BOUNDED_SPAN_N_AT_OR_THROW` still requires
  `std::format_string<Args...>` (a literal fmt at the call site);
  pinned by Section 6's successful CONSTEVAL test
  (`CHECK_EQ(r.value(), 11)` for `"hello {}"` + `"world"`).

- **Throwing+AT-padded span\<char\> sibling preserves the SAME
  `std::runtime_error` byte-exactness** — TODAY's wrapper does NOT
  construct a new `std::runtime_error` message; the throw TYPE
  and `.what()` message are both delegated to Oct 16's
  throwing+STAR-padded span\<char\> wrapper (which delegates to
  the throwing+untouched Sep 21 wrapper); Section 7 pins this on
  the success path AND on the TooLarge-throwing path (asserts
  `'threw == true'` AND `'buf UNTOUCHED == all_Q'`).

- **`PETRA_TILDE_PAD` friendly-error uses `#if defined(...)`** —
  the macro is not defined in this file unless the user sets it;
  without `defined()` guard, undefined macros evaluate to 0 in
  `#if` expressions and the friendly-error would NOT trigger.

## Tests / Results

- **Section 1** — expected+AT-pad `(0, 1)` cell: bytes after
  written are `'@'`.  Confirms the AT-pad span\<char\> sibling's
  byte-state contract.

- **Section 2** — expected+AT-pad `(0, 1)` cell: buffer UNTOUCHED
  on TooLarge + sub-span no-bleed.  Confirms the buffer-atomicity
  on overflow contract AND the sub-span no-bleed contract.

- **Section 3** — expected+AT-pad `(0, 1)` cell: buffer UNTOUCHED
  on Format error.  Confirms the format-time error contract.

- **Section 4** — expected+AT-pad `(0, 1)` cell: `written ==
  span.size()` and `span.size()==0` edge cases.  Confirms the
  edge-case correctness of the AT-fill loop.

- **Section 5** — AT-pad `(0, 1)` cell: byte-state distinctness.
  Confirms `'@'` is distinct from `'*'`, `'_'`, `'~'`, `'#'`,
  `'-'`, `' '`, `'\0'`, and `'Q'`.

- **Section 6** — expected+AT-pad `(0, 1)` CONSTEVAL surface:
  same contract as RUNTIME.  Confirms the CONSTEVAL surface of
  the AT-pad span\<char\> sibling behaves identically.

- **Section 7** — throwing+AT-pad `(1, 1)` cell: success path +
  throws on TooLarge.  Confirms the throwing+AT-padded
  span\<char\> sibling's byte-state contract on success and
  throw-behavior on overflow.

- **Section 8** — throwing+AT-pad `(1, 1)` CONSTEVAL surface.
  Confirms the CONSTEVAL surface of the throwing+AT-padded
  span\<char\> sibling behaves identically.

- **Section 9** — macro `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_AT_OR_THROW`:
  works at preprocessor time.  Pins the parenthesized-expression
  shape and the `(THROW=0, AT=0) / (THROW=0, AT=1) /
  (THROW=1, AT=0) / (THROW=1, AT=1)` dispatch.

- **Section 10** — concurrent dispatch: 4 threads × 25 calls on
  the AT-pad path.  Pins thread-safety on the AT-pad span\<char\>
  sibling — each thread uses its own local counter (int via
  `std::atomic<int>::fetch_add`) to tally matching bytes, then
  aggregated after join.  100/100 PASS per run × 5 consecutive
  runs = 500 concurrent calls, zero failures.

- **Section 11** — `decltype` pinning on the (THROW, AT) 2-bit
  tuple.  Confirms return types match expectations on all four
  cells via `std::is_same_v<...>` `static_assert`s.

## Results across 5 consecutive runs

| Cell | THROW | AT | PASS (run 1) | run 2 | run 3 | run 4 | run 5 |
|------|-------|----|---------------|-------|-------|-------|-------|
| (0,0) | 0 | 0 | 100/0 | 100/0 | 100/0 | 100/0 | 100/0 |
| (0,1) | 0 | 1 | 100/0 | 100/0 | 100/0 | 100/0 | 100/0 |
| (1,0) | 1 | 0 |  99/0 |  99/0 |  99/0 |  99/0 |  99/0 |
| (1,1) | 1 | 1 |  99/0 |  99/0 |  99/0 |  99/0 |  99/0 |

Note: the `(THROW=1, AT=*)` cells run 1 fewer CHECK than the
`(THROW=0, AT=*)` cells because Section 9's macro test uses
`#if PETRA_THROW_ON_OVERFLOW == 0` to gate the
`check_true(n_macro.has_value())` call (which only compiles in
the THROW=0 cells).  The `(THROW=1, AT=*)` cells use the
`#else` branch (`check_eq(n_macro, 5)`).  This 1-CHECK
difference is by design and matches Oct 16's pattern exactly.

- 100/100 PASS on `(THROW=0, AT=0)` default build (5 consecutive
  runs all 100/0)
- 100/100 PASS on `(THROW=0, AT=1)` expected+AT-padded build (5
  consecutive runs all 100/0)
- 99/99 PASS on `(THROW=1, AT=0)` throwing+untouched build (5
  consecutive runs all 99/0)
- 99/99 PASS on `(THROW=1, AT=1)` throwing+AT-padded build (5
  consecutive runs all 99/0)
- 20 total runs across the 4 cells, zero failures, zero flakiness.

## Verification

- **Zero warnings** under
  `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`.
- **Zero sanitizer diagnostics** under
  `-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer`.
- **Empty stderr** under default + ASan/UBSan + strict-warning
  builds.

## Reproducing the build

Default build (expected + untouched; `PETRA_THROW=0`,
`PETRA_AT_PAD=0`):

```bash
cd late-may/cpp_practice/try_format_at_pad_span_n
cmake -S . -B build
cmake --build build
./build/P-2026-10-19-try-format-at-pad-span-n
```

AT-pad cell (`PETRA_AT_PAD=1`):

```bash
cmake -S . -B build-at -DPETRA_AT_PAD=ON
cmake --build build-at
./build-at/P-2026-10-19-try-format-at-pad-span-n
```

Throwing + AT-pad cell (`PETRA_THROW_ON_OVERFLOW=1`,
`PETRA_AT_PAD=1`):

```bash
cmake -S . -B build-throw-at \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_AT_PAD=ON
cmake --build build-throw-at
./build-throw-at/P-2026-10-19-try-format-at-pad-span-n
```

Strict-warning mode:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-19-try-format-at-pad-span-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 \
  ./build-asan/P-2026-10-19-try-format-at-pad-span-n
```

Strict + ASan combined build:

```bash
cmake -S . -B build-strict-asan \
      -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
cmake --build build-strict-asan
ASAN_OPTIONS=detect_leaks=0 \
  ./build-strict-asan/P-2026-10-19-try-format-at-pad-span-n
```

Friendly-error compatibility checks (out-of-scope composition
cells fire at compile time):

```bash
# Should #error with "PETRA_AT_PAD must be 0 or 1"
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -DPETRA_AT_PAD=2 \
        P-2026-10-19-try-format-at-pad-span-n.cpp -c -o /tmp/x.o

# Should #error with the STAR_PAD friendly-error
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -DPETRA_STAR_PAD=1 \
        P-2026-10-19-try-format-at-pad-span-n.cpp -c -o /tmp/x.o
```

## Where we go next

The NINTH pad character (`PETRA_AT_PAD`) is now closed on BOTH
axes (CHAR\*+CAP closed by Oct 18; STD::SPAN\<CHAR\> closed by
today).  The matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_AT_PAD)` 2-bit tuple is COMPLETE
for ALL FOUR cells on BOTH axes.  Combined with Oct 18 / Oct 17 /
Oct 16 / Oct 15 / Oct 13 / Oct 12 / Oct 9 / Oct 4 / Sep 28 / Sep 27
/ Sep 25, the BOUNDED-BUFFER × expected-vs-throwing ×
pad-character matrix reaches NINE pad characters on BOTH axes:

NONE (no fill) → NUL → SPACE → DASH → HASH → TILDE → UNDERSCORE
→ STAR → AT.

Future lessons can either:

1. **A TENTH-axis pad character on either side** — e.g.
   `PETRA_PLUS_PAD` (`'+'`, ASCII 43) or `PETRA_BANG_PAD`
   (`'!'`, ASCII 33).  Each such lesson would mirror Oct 12 /
   Oct 13 / Oct 14 / Oct 15 / Oct 16 / Oct 17 / Oct 18 / today's
   pattern exactly — a thin layer on top of today's AT-pad
   sibling, OVERWRITING the `'@'` fill with the new pad byte on
   success.  Closes the matrix on the `(THROW, NEW_PAD)` 2-bit
   tuple for that axis.

2. **A unified 9-bit-tuple dispatcher
   `(THROW × PAD × SPACE × DASH × HASH × TILDE × UNDERSCORE × STAR
   × AT)`** — one macro covers all composition cells across all
   nine pad flags, picking the right wrapper at preprocessor time
   via `__VA_OPT__(,)`-based dispatch.  This would close the
   composition matrix on both axes simultaneously (mirroring
   Oct 1's PAD × SPACE composition lesson on the span\<char\>
   side, and Sep 30 / Oct 5 / Oct 8 on the char\*+cap side).

3. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now that
   the NINTH axis is closed on BOTH axes.

The current matrix on the BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis reaches NINE pad
characters on BOTH axes (CHAR\*+CAP and STD::SPAN\<CHAR\>):
NONE (no fill) → NUL → SPACE → DASH → HASH → TILDE → UNDERSCORE
→ STAR → AT.