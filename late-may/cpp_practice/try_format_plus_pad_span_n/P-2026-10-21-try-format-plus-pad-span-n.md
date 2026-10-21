# P-2026-10-21 — try_format_plus_pad_span_n

**Closes Oct 20's "Where we go next" item (1) verbatim — the
STD::SPAN\<CHAR\>-side TENTH pad-character axis (`PETRA_PLUS_PAD`).**

Today's lesson introduces `PETRA_PLUS_PAD` (a NEW TENTH
compile-time flag) on top of Oct 19's 9-bit tuple
(`PETRA_THROW_ON_OVERFLOW`, `PETRA_PAD`, `PETRA_SPACE_PAD`,
`PETRA_DASH_PAD`, `PETRA_HASH_PAD`, `PETRA_TILDE_PAD`,
`PETRA_UNDERSCORE_PAD`, `PETRA_STAR_PAD`, `PETRA_AT_PAD`) on
the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.
`PETRA_PLUS_PAD=1` selects the PLUS-padded `std::span<char>`
sibling, which fills bytes `[written, span.size())` with `'+'`
(ASCII 0x2B = 43) on success.

The `'+'` byte is TOTALLY DIFFERENT from Oct 19's `'@'` (ASCII 64),
Oct 16's `'*'` (ASCII 42), Oct 15's `'_'` (ASCII 95), Oct 13's
`'~'` (ASCII 126), Oct 11's `'#'` (ASCII 35), Oct 4's `'-'`
(ASCII 45), Sep 28's `' '` (ASCII 32), and Sep 27's `'\0'`
(ASCII 0).  After today, the STD::SPAN\<CHAR\> × BOUNDED-BUFFER
matrix on the `(PETRA_THROW_ON_OVERFLOW × PETRA_PLUS_PAD)`
2-bit tuple is COMPLETE for ALL FOUR cells, mirroring Oct 20's
CHAR\*+CAP-side closure of the same TENTH-axis pad layer.

The composition rule "the LATER fill overwrites the EARLIER
fill" now has NINE layers on BOTH axes:
`PLUS > AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`.
When all nine pad-flags are set, the end byte state is always
`'+'`.  Composition cells with `(PLUS=1, AT=*)` REUSE TODAY's
NEW PLUS-pad wrappers verbatim because PLUS overwrites AT
(which overwrites STAR which overwrites UNDERSCORE which
overwrites TILDE which overwrites HASH which overwrites DASH
which overwrites SPACE).

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
   - Oct 19   try_format_at_pad_span_n — (PETRA_AT_PAD) sibling
              wrapper on the STD::SPAN<CHAR> side; the NINTH
              axis on STD::SPAN<CHAR>.  TODAY's PLUS-pad
              sibling sits on top of it.
   - Oct 20   try_format_plus_pad_n — (PETRA_PLUS_PAD) sibling
              wrapper on the CHAR*+CAP side; the TENTH axis.
   - TODAY    try_format_plus_pad_span_n — (PETRA_PLUS_PAD)
              sibling wrapper on the STD::SPAN<CHAR> side; the
              TENTH axis on STD::SPAN<CHAR>.  Closes Oct 20's
              "Where we go next" item (1) verbatim.  Today's
              pad character is '+' (ASCII 43), TOTALLY
              DIFFERENT from Oct 19's '@' (ASCII 64), Oct 16's
              '*' (ASCII 42), Oct 15's '_' (ASCII 95), Oct 13's
              '~' (ASCII 126), Oct 11's '#' (ASCII 35),
              Oct 4's '-' (ASCII 45), Sep 28's ' ' (ASCII 32),
              and Sep 27's '\0' (ASCII 0).
```

## Concepts exercised

**1. `std::format_string<Args...>` (P2216R3) — the consteval
format-string gate.** The CONSTEVAL-surface wrappers take
`std::format_string<Args...>`, whose converting constructor is
`consteval`.  That means a malformed or ill-typed format string
is a **compile-time** diagnostic, not a runtime `std::format_error` —
the class only exists if the format string validates.  The
RUNTIME-surface wrappers take `std::string_view`, where a
malformed format string is a **runtime** `std::format_error`
that the impl catches and converts into `FormatErrorKind::Format`.
Section 3 pins the runtime half; Section 6 pins the consteval half.

**2. The two-pass `std::vformat_to` protocol.** The underlying
impl (Sep 8, reused verbatim) formats **twice**:

- Pass 1 uses `petra::counting_output_iterator` — an output
  iterator that writes nothing and only counts.  It survives
  `std::vformat_to`'s internal iterator *copy* because it
  holds no mutable state of its own: all state lives behind a
  `void (*)(std::size_t, void*)` callback plus a `void*`.
- If `needed > cap`, the impl returns `TooLarge` **before pass
  2 ever writes a byte**.  *This* is what gives the buffer its
  atomicity-on-error contract: Sections 2, 3 and 7 all pin
  "every byte is still the poison byte `'Q'`" after a failed
  call.
- Pass 2 uses `petra::detail::bounded_char_writer`, which
  silently *drops* any byte past `span.size()` rather than
  overrunning.  The impl then asserts `written == needed` as
  a consistency check.

**3. `std::expected` (P0323R12) vs `std::runtime_error` — two
error surfaces over one impl.** The expected surface returns
`std::expected<std::size_t, FormatError>` with a structured
`kind` + `message` + `needed_bytes`; the throwing surface
converts that into `std::runtime_error` with a `"TooLarge: "`
or `"Format: "` message prefix.  Today's throwing wrappers
**re-raise untouched** (`catch (...) { throw; }`) — they
never catch & rewrap, so the exception type and `.what()` bytes
are byte-exact with what the underlying impl constructed.
Section 7 pins this.

**4. The "LATER fill overwrites the EARLIER fill" composition
rule.** Every pad wrapper is a thin layer:

```cpp
template <typename... Args>
std::expected<std::size_t, FormatError>
try_format_runtime_n_plus_pad_span(std::span<char> out,
                                   std::string_view fmt,
                                   const Args&... args) {
    auto r = try_format_runtime_n_at_pad_span(out, fmt, args...);
    if (r.has_value()) {                     // guard: buffer atomicity
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) out[i] = '+';
    }
    return r;
}
```

Five lines of new logic buy the entire TENTH axis on
`std::span<char>`.  The `if (r.has_value())` guard is the
load-bearing line: drop it and the fill loop would run on
the error path, stamping `'+'` over a buffer that the impl
promised to leave untouched.

**5. `__VA_OPT__(,)` in a parenthesized-expression macro.**
The dispatcher is an expression (not `do { } while (0)`),
so the caller can capture the result:
`auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PLUS_OR_THROW(...)`.
`__VA_OPT__(,)` supplies the comma only when `__VA_ARGS__`
is non-empty, which makes the no-args case `fmt` alone legal.
TODAY adds TWO meta-macros (CONSTEVAL surface +
RUNTIME surface), mirroring Oct 16 / Oct 19's shape verbatim.

**6. Adjacent-literal hazard: `'+'` = `'*'` + 1.** Today pins
this in Section 5 with `static_assert('+' == '*' + 1)` so the
adjacency is an explicit, documented fact rather than a latent
off-by-one bug.  The three prior pad characters that are
adjacent to each other (`+` = 43 vs `*` = 42; `!` = 33 vs
` ` = 32; `/` = 47 vs `.` = 46, etc.) all warrant this kind of
static_assert pinning in their respective lessons — today's
lesson pins the load-bearing fact and joins the pattern.

## Key design decisions

- **NARROW SCOPE.** Only `PETRA_PLUS_PAD` (and `PETRA_THROW_ON_OVERFLOW`)
  are live; the other eight pad flags `#error` at compile time
  with pointers to the lessons that own those cells.  A
  10-bit-tuple dispatcher would need 2^10 cells — this
  lesson keeps the matrix at 2 × 2 = 4 and keeps the file a
  viable single-file exercise.
- **SELF-CONTAINED.** The file does **not** `#include` Oct 19's
  source.  It contains the full helper stack (FormatError,
  both iterator primitives, the impl, the eight prior pad
  wrappers, the four AT-pad wrappers) plus the four NEW
  PLUS-pad wrappers, so the whole thing verifies from a
  single `clang++` invocation.
- **COMPILE-TIME ASCII PROOF.** `'+'` is ASCII 43 and `'*'` is
  ASCII 42 — **adjacent codes**.  An off-by-one literal typo
  between the two is invisible in review and in a casual
  buffer eyeball.  Section 5 pins the literal codes with ten
  `static_assert`s (including `static_assert('+' == '*' + 1)`)
  so the adjacentness is an explicit, documented fact rather
  than a latent bug.
- **ATOMICITY RE-PINNED, NOT ASSUMED.** The new
  `for (i = written; i < span.size(); ++i) out[i] = '+';` loop
  *could* diverge from the underlying impl's promise if the
  `has_value()` guard were mis-coded.  Sections 2, 3 and 7
  independently verify every byte is still `'Q'` after a
  `TooLarge` and after a `Format` error, on both the expected
  and the throwing surface.
- **CONCURRENCY.** The wrapper chain is stateless — all
  per-call state is a stack local.  Section 10 runs 4 threads
  × 25 calls into thread-local buffers and tallies 100/100
  all-`'+'` tails.

## The FOUR new wrappers

| wrapper | surface | on success | on error |
|---|---|---|---|
| `try_format_runtime_n_plus_pad_span` | RUNTIME × EXPECTED | `std::expected<std::size_t, FormatError>`; bytes `[written, span.size())` = `'+'` | returns the underlying `TooLarge` / `Format` error; buffer untouched |
| `try_format_bounded_runtime_n_plus_pad_span` | CONSTEVAL × EXPECTED | same, via `std::format_string<Args...>` | same |
| `try_format_runtime_n_throwing_plus_pad_span` | RUNTIME × THROWING | `std::size_t`; bytes `[written, span.size())` = `'+'` | `throw;` re-raises the underlying `std::runtime_error` untouched |
| `try_format_bounded_runtime_n_throwing_plus_pad_span` | CONSTEVAL × THROWING | same, via `std::format_string<Args...>` | same |

Plus two meta-macros:

```cpp
TRY_FORMAT_BOUNDED_SPAN_N_PLUS_OR_THROW(out, fmt, ...)              // CONSTEVAL
TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PLUS_OR_THROW(out, fmt, ...)      // RUNTIME
```

which select one of four wrappers at preprocessor time from
the `(PETRA_THROW_ON_OVERFLOW, PETRA_PLUS_PAD)` 2-bit tuple.

## The (THROW × PLUS) 2-bit matrix

| cell | `PETRA_THROW_ON_OVERFLOW` | `PETRA_PLUS_PAD` | wrapper selected | bytes `[written, span.size())` on success |
|---|---|---|---|---|
| (0, 0) | 0 | 0 | `try_format_runtime_n(span<char>)` (Sep 19) | untouched `'Q'` |
| (0, 1) | 0 | 1 | `try_format_runtime_n_plus_pad_span` (NEW) | `'+'` |
| (1, 0) | 1 | 0 | `try_format_runtime_n_throwing(span<char>)` (Sep 21) | untouched `'Q'` |
| (1, 1) | 1 | 1 | `try_format_runtime_n_throwing_plus_pad_span` (NEW) | `'+'` |

## Observed output

Default build (`PETRA_THROW_ON_OVERFLOW=0`, `PETRA_PLUS_PAD=0`):

```
== Section 1: expected+PLUS-pad (0, 1) cell — bytes after written are '+' ==
== Section 2: expected+PLUS-pad (0, 1) cell — buffer UNTOUCHED on TooLarge + sub-span no-bleed ==
== Section 3: expected+PLUS-pad (0, 1) cell — buffer UNTOUCHED on Format error ==
== Section 4: expected+PLUS-pad (0, 1) cell — written == span.size() and span.size()==0 edge cases ==
== Section 5: PLUS-pad (0, 1) cell — byte-state distinctness ==
== Section 6: expected+PLUS-pad (0, 1) CONSTEVAL surface — same contract as RUNTIME ==
== Section 7: throwing+PLUS-pad (1, 1) cell — success path + throws on TooLarge ==
== Section 8: throwing+PLUS-pad (1, 1) CONSTEVAL surface ==
== Section 9: macro TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PLUS_OR_THROW — works at preprocessor time ==
== Section 10: concurrent dispatch — 4 threads x 25 calls on the PLUS-pad path ==
== Section 11: decltype pinning on the (THROW, PLUS) 2-bit tuple ==
== Summary ==
  PASS=106  FAIL=0
```

## Verification performed

**Compiles clean under strict warnings, all four flag cells:**

```
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion \
        [cell flags] P-2026-10-21-try-format-plus-pad-span-n.cpp -o <out>
```

**Test tallies — 100% PASS on every configuration, zero stderr bytes:**

| configuration | build | run | result |
|---|---|---|---|
| default | ok | 0 | PASS=106 FAIL=0 |
| `-DPETRA_PLUS_PAD=ON` | ok | 0 | PASS=106 FAIL=0 |
| `-DPETRA_THROW_ON_OVERFLOW=ON` | ok | 0 | PASS=105 FAIL=0 |
| `-DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PLUS_PAD=ON` | ok | 0 | PASS=105 FAIL=0 |
| `ENABLE_STRICT_WARNINGS=ON` | ok | 0 | PASS=106 FAIL=0, 0 warnings |
| `ENABLE_ASAN=ON` | ok | 0 | PASS=106 FAIL=0, 0 sanitizer diagnostics |
| `STRICT + THROW + PLUS` | ok | 0 | PASS=105 FAIL=0, 0 warnings |
| `ASAN + THROW + PLUS` | ok | 0 | PASS=105 FAIL=0, 0 sanitizer diagnostics |

(The (1, \*) cells have 105 instead of 106 because the throwing
surface skips the section-9 macro check on the `(THROW=0,
PLUS=*)` branch — the macro's `if (n_macro.has_value()) check
collapses to a no-op on the throwing branch, where the macro
returns a bare `std::size_t`.)

**Flakiness:** the default build PASS=106/FAIL=0 has been
verified reproducible across multiple runs.

**ASan + UBSan:** `ASAN_OPTIONS=detect_leaks=0 ./...` on the
`THROW + PLUS` cell — zero diagnostics, empty stderr.

**Friendly-error (compile-time) checks** — all fire as expected:

```
-DPETRA_PLUS_PAD=2            -> "PETRA_PLUS_PAD must be 0 (no plus-pad) or 1 (plus-padded)"
-DPETRA_THROW_ON_OVERFLOW=2   -> "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
-DPETRA_PAD=1                 -> pointer to Sep 27 / Oct 1 (NUL-padded span<char> wrappers)
-DPETRA_SPACE_PAD=1           -> pointer to Sep 28 / Oct 1 (SPACE-padded span<char> sibling)
-DPETRA_DASH_PAD=1            -> pointer to Oct 4 / Oct 5 / Oct 8 (DASH-padded span<char> sibling)
-DPETRA_HASH_PAD=1            -> pointer to Oct 11 (HASH-padded span<char> sibling)
-DPETRA_UNDERSCORE_PAD=1      -> pointer to Oct 15 (UNDERSCORE-padded span<char> sibling)
-DPETRA_STAR_PAD=1            -> pointer to Oct 16 (STAR-padded span<char> sibling)
-DPETRA_AT_PAD=1              -> pointer to Oct 19 (AT-padded span<char> sibling)
```

**All temporary artifacts were kept under `/tmp`** — every
CMake build tree and the binary live in `/tmp`; nothing
compiled was staged.

## Rebuilding

Default build:

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-21-try-format-plus-pad-span-n
```

PLUS-pad cell:

```bash
cmake -S . -B build-plus -DPETRA_PLUS_PAD=ON
cmake --build build-plus
./build-plus/P-2026-10-21-try-format-plus-pad-span-n
```

Throwing + PLUS-pad cell:

```bash
cmake -S . -B build-throw-plus \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PLUS_PAD=ON
cmake --build build-throw-plus
./build-throw-plus/P-2026-10-21-try-format-plus-pad-span-n
```

Strict-warning mode:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-21-try-format-plus-pad-span-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ./build-asan/P-2026-10-21-try-format-plus-pad-span-n
```

## Where we go next

The STD::SPAN\<CHAR\> TENTH pad character (`PETRA_PLUS_PAD`)
is now closed across ALL FOUR cells of the
`(PETRA_THROW_ON_OVERFLOW × PETRA_PLUS_PAD)` 2-bit tuple.
Combined with Oct 20's CHAR\*+CAP PLUS-pad sibling, both
axes reach TEN pad characters simultaneously: NONE → NUL →
SPACE → DASH → HASH → TILDE → UNDERSCORE → STAR → AT → PLUS.

Future lessons can either:

1. **An ELEVENTH-axis pad character on either side** — e.g.
   `PETRA_BANG_PAD` (`'!'`, ASCII 33).  Each such lesson
   would mirror Oct 19 / Oct 20 / today's pattern exactly —
   a thin layer on top of today's PLUS-pad sibling,
   OVERWRITING the `'+'` fill with the new pad byte on
   success.

2. **A unified 10-bit-tuple dispatcher `(THROW × PAD × SPACE ×
   DASH × HASH × TILDE × UNDERSCORE × STAR × AT × PLUS)`** —
   one macro covering all composition cells across all ten
   pad flags, picking the right wrapper at preprocessor time.
   Because the composition rule is a strict total order
   (`PLUS > AT > STAR > UNDERSCORE > TILDE > HASH > DASH >
   SPACE > NUL`), such a dispatcher can select the
   *highest-ranked* enabled pad flag with a simple
   preprocessor priority chain rather than an exhaustive
   2^10 table — worth working out explicitly in a lesson.

3. **A flush-style wrapper that bypasses the AT-pad
   intermediate** — i.e. a `PETRA_PLUS_PAD=1` cell that calls
   the underlying span<char> wrapper directly (skipping the
   AT-pad wrapper, which would then become dead code on that
   cell).  This would lose the composition guarantee in
   exchange for one fewer virtual layer; not recommended
   unless profiler data shows the layers cost something.
