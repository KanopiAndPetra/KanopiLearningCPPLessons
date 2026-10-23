# P-2026-10-23 — try_format_bang_pad_priority_dispatch

## What today closes

Oct 22's `try_format_pad_priority_dispatch` ended with a "Where we go
next" list. Item (2) was:

> **An ELEVENTH pad** (`PETRA_BANG_PAD`, `'!'`, ASCII 33). Adding a
> pad to the chain is now: one rank constant, one score macro, one
> ternary arm in FILLS/CODE/NAME, one entry in the constexpr mirror,
> and one `static_assert`. The 1024-cell sweep is the regression net.
> Worth doing at least one new pad this way to prove the *shape*
> generalises rather than just the current nine.

**That is today's lesson, verbatim** — and item (3) gets pulled part of
the way in as well, because it turns out to be the natural way to
*verify* item (2).

The point of item (2) was never "add another `if`". It was: *if a
tenth pad cost what a tenth pad should cost, the shape is real.* So
today measures the cost instead of asserting it.

## Rank table: the eleventh pad, and what it did not disturb

| rank | flag | byte | code |
| ---: | --- | :---: | ---: |
| — | (none) | — | (no fill) |
| 1 | `PETRA_PAD` | `\0` | 0 |
| 2 | `PETRA_SPACE_PAD` | ` ` | 32 |
| 3 | `PETRA_DASH_PAD` | `-` | 45 |
| 4 | `PETRA_HASH_PAD` | `#` | 35 |
| 5 | `PETRA_TILDE_PAD` | `~` | 126 |
| 6 | `PETRA_UNDERSCORE_PAD` | `_` | 95 |
| 7 | `PETRA_STAR_PAD` | `*` | 42 |
| 8 | `PETRA_AT_PAD` | `@` | 64 |
| 9 | `PETRA_PLUS_PAD` | `+` | 43 |
| 10 | `PETRA_BANG_PAD` *(new)* | `!` | 33 |

## KEY FINDING #1 — appending at the top renumbers nothing

The score is `rank + 1` (Oct 22's deliberate one-offset so NONE can
score 1 and still lose to every enabled pad). Appending BANG at rank
10 means BANG scores 11 and **no existing score moves**. The
pre-existing 512 flag combinations therefore have to produce byte-
identical answers, and they do.

**Measured, not asserted.** Two independent implementations over all
1024 combinations, `/tmp/petra20261023/cross_check.py`:

```
ten-pad vs eleven-pad over all 512 BANG-off cells: identical
cells checked                              : 1024
chain vs if-chain disagreements (11 pads)   : 0
chain vs un-offset disagreements (11 pads)  : 0
if-chain vs un-offset disagreements         : 0
BANG wins                                  : 512 (expected 512)
NONE wins                                  : 1 (expected 1)
  rank  1  NUL         wins    1 cells
  rank  2  SPACE       wins    2 cells
  ...
  rank 10  BANG        wins  512 cells
CROSS-CHECK: PASS
```

The win tallies are worth a second look, because they fall out of the
spec rather than being checked into it: the pad at rank `r` wins in
exactly `2^(r-1)` cells (every higher-ranked pad forced off, every
lower-ranked one free). The ten tallies are 1, 2, 4, …, 512, summing
to 1023, with NONE taking the single remaining cell. Section 9 of the
program asserts the same identity at runtime, and the Python model
reproduces it independently.

**The lesson I did not expect.** A pad inserted in the *middle* of the
order would have edited every score above it. And a rank-order-
insensitive encoding — "last flag set wins", say — would have silently
changed the meaning of existing build cells with no compiler
diagnostic at all. The `rank + 1` scheme is not just compact; the
*append* discipline is what makes today's change reviewable as a
diff that touches nothing else.

## KEY FINDING #2 — max is idempotent, so an odd arity needs a repeat

Oct 22 hand-wrote `MAX2`/`MAX3`/`MAX5`/`MAX10` to fold ten scores.
Eleven scores would have demanded a hand-written `MAX11` — a fifth
macro differing from `MAX10` by one nesting level.

But `max(a, …, k, a, a, a, a, a) == max(a, …, k)`: duplicating an
argument cannot change a maximum. So today replaces the hand-rolled
odd sizes with the **power-of-two family** and pads up to the next
power of two with a repeat:

```c
#define PETRA_PAD_MAX2(a, b) ((a) > (b) ? (a) : (b))
#define PETRA_PAD_MAX4(a, b, c, d) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX2(a, b), PETRA_PAD_MAX2(c, d))
#define PETRA_PAD_MAX8(a, b, c, d, e, f, g, h) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX4(a, b, c, d), PETRA_PAD_MAX4(e, f, g, h))
#define PETRA_PAD_MAX16(...) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX8(...), PETRA_PAD_MAX8(...))

#define PETRA_PAD_SELECTED_SCORE                                          \
    PETRA_PAD_MAX2(PETRA_PAD_MAX8(NONE, NUL, SPACE, DASH,                 \
                                  HASH, TILDE, UNDERSCORE, STAR),         \
                   PETRA_PAD_MAX4(AT, PLUS, BANG, NONE))
```

Four macros cover any arity up to 16; three cover up to 8. The family
grows logarithmically rather than gaining a member per arity, and the
pad is *sound by the idempotence law* rather than by inspection.
Section 2 checks the padded fold against an unpadded one numerically
on every build.

### A preprocessor bug I hit twice, and the diagnostic that lied about it

My first draft spelled `MAX8` the obvious way:

```c
#define PETRA_PAD_MAX8(a, b, c, d, e, f, g, h) \
    PETRA_PAD_MAX4(PETRA_PAD_MAX4(a, b, c, d), PETRA_PAD_MAX4(e, f, g, h))
```

and clang rejected the perfectly innocent `MAX8(1,2,3,4,5,6,7,8)`:

```
error: too few arguments provided to function-like macro invocation
note: expanded from macro 'PETRA_PAD_MAX8'
note: macro 'PETRA_PAD_MAX4' defined here
```

**Two** mistakes were stacked behind that one message:

1. The family must be composed with the **base** combinator at each
   level. `MAX4`-of-`MAX4`s leaves a bare `MAX4` token in the
   replacement list, and the rescan mis-attaches the outer
   parentheses. `MAX2(MAX4(..), MAX4(..))` carries identical
   information and rescans cleanly.
2. Arity accounting by eye is bad, and my eye was. I passed *eight
   plain scores **and** a nested `MAX4`* to the eight-argument
   `MAX8` — nine arguments to a macro that takes eight.

Both surfaced as the same "too few/many arguments" diagnostic, pointing
at a macro several lines away from the mistake. Verified with
standalone probes: `MAX4` with plain arguments is fine;
`MAX2(MAX4(..), MAX4(..))` is fine; `MAX8`-with-`MAX4`-of-`MAX4`s fails
even with plain arguments.

The generalisable part: **a nested preprocessor combinator has to be
checked at its use site, not at its definition, and the diagnostic
points at the innermost macro rather than at the call that miscounted.**
Same family as Oct 22's "a parameter pack is only legal in a function
TEMPLATE" note — a spelling that reads right, stopped only by the
compiler.

## KEY FINDING #3 — the runtime selector retires the external sweep

Oct 22's flags were macros, so each of the 1024 cells needed its own
`clang++` invocation; that is why yesterday's verification harness was
a shell loop. Today takes Oct 22's next-step item (3) — a runtime
`PadFlags` struct plus `constexpr select_pad()` — which means **one
binary now answers "which pad wins?" for all 1024 cells**:

```cpp
struct PadFlags { bool nul, space, dash, hash, tilde,
                  underscore, star, at, plus, bang; };

[[nodiscard]] constexpr PadChoice select_pad(PadFlags f) noexcept;
```

Four independent implementations live side by side:

| # | spelling | structure |
| --: | --- | --- |
| A | `select_pad_if_chain` | plain if-chain, high rank first |
| B | `select_pad_score_max` | score everything, max, map back |
| C | `select_pad_unoffset` | **no** `+1` offset — rank 0 is NONE |
| D | `select_pad_table` | walks a `(rank, code, name)` array |

C is the interesting one: it deliberately does *not* use Oct 22's
`+1` offset, so a shared assumption about the offset cannot hide in
both. D is the closest thing to the lookup-table dispatcher that Oct 22
argued against, and its agreement is the evidence that the chain is not
a table in disguise.

Section 9 enumerates all 1024 masks and checks all four agree on every
field:

```
== Section 9: EXHAUSTIVE 1024-cell runtime cross-check ==
  1024 cells: agree=1024  BANG-wins=512  NONE-wins=1
```

That is the same coverage as yesterday's 1024-process external sweep,
at roughly a millisecond of runtime instead of 1024 compiler
invocations.

The static_asserts pin all of it per build cell — three
preprocessor-vs-constexpr (Oct 22's), four runtime-vs-preprocessor
(new), and one four-way runtime agreement.

## Sections in the program

| # | what it pins |
| --: | --- |
| 1 | rank table, ten ASCII codes, strict total order, distinct names |
| 2 | chain invariants, **padded vs unpadded max** (idempotence), runtime agreement |
| 3 | meta-macro success path honours the selected pad |
| 4 | consteval (`std::format_string`) surface through the macro |
| 5 | buffer UNTOUCHED on TooLarge (both throw cells included) |
| 6 | Format-error atomicity + sub-span no-bleed |
| 7 | `written == span.size()` and `span.size() == 0` edge cases |
| 8 | NONE vs NUL vs **BANG** are three distinct contracts |
| 9 | **exhaustive 1024-cell cross-check of four selectors** |
| 10 | 4 threads × 25 calls concurrent dispatch |
| 11 | summary |

Every section is written against **the selected pad**, so the same
source file is the test for every build cell.

Section 8 is where the new pad earns its place: Oct 22 pinned NONE vs
NUL because both have `CODE == 0` and differ only in `FILLS`. BANG is
the opposite corner — `FILLS == 1` with a non-zero `CODE` — so the
section now covers all three shapes and asserts they are pairwise
distinguishable.

## Build and run

Default (expected, no pad):

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-23-try-format-bang-pad-priority-dispatch
```

The cell that exercises today's new code — BANG wins over all ten
prior pads:

```bash
cmake -S . -B build-bang -DENABLE_STRICT_WARNINGS=ON -DPETRA_BANG_PAD=ON \
      -DPETRA_PLUS_PAD=ON -DPETRA_AT_PAD=ON -DPETRA_STAR_PAD=ON \
      -DPETRA_UNDERSCORE_PAD=ON -DPETRA_TILDE_PAD=ON -DPETRA_HASH_PAD=ON \
      -DPETRA_DASH_PAD=ON -DPETRA_SPACE_PAD=ON -DPETRA_PAD=ON
cmake --build build-bang
./build-bang/P-2026-10-23-try-format-bang-pad-priority-dispatch
```

Strict warnings and sanitizers:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake -S . -B build-asan   -DENABLE_ASAN=ON
```

Or directly:

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow \
        -Wconversion -Wsign-conversion \
        P-2026-10-23-try-format-bang-pad-priority-dispatch.cpp -o /tmp/petra
```

## Observed output

Default cell (`PETRA_THROW_ON_OVERFLOW=0`, no pad flags):

```
selected pad = NONE  (fills=0, byte=0 (0x00), score=1)

== Section 1: rank table + strict total order ==
== Section 2: priority-chain invariants for this cell ==
== Section 3: meta-macro success path honors the selected pad ==
== Section 4: CONSTEVAL meta-macro surface ==
== Section 5: buffer UNTOUCHED on TooLarge ==
== Section 6: Format error atomicity + sub-span no-bleed ==
== Section 7: written==size and size==0 edge cases ==
== Section 8: NONE vs NUL vs BANG are three distinct contracts ==
== Section 9: EXHAUSTIVE 1024-cell runtime cross-check ==
  1024 cells: agree=1024  BANG-wins=512  NONE-wins=1
== Section 10: concurrent dispatch — 4 threads x 25 calls ==
== Summary ==
  selected pad = NONE
  selected byte = 0  fills = no
  BANG enabled  = no  (rank 10, byte '!' = 33)
  PASS=58  FAIL=0
```

BANG cell:

```
selected pad = BANG  (fills=1, byte=33 (0x21), score=11)
...
  selected pad = BANG
  selected byte = 33  fills = yes
  BANG enabled  = yes  (rank 10, byte '!' = 33)
  PASS=58  FAIL=0
```

All-eleven-pads cell (BANG wins, as it must):

```
selected pad = BANG  (fills=1, byte=33 (0x21), score=11)
```

## Verification performed

1. **Strict-warning builds** — `-Wall -Wextra -Wpedantic -Werror
   -Wshadow -Wconversion -Wsign-conversion` on 28 distinct flag
   combinations (14 pad cells × 2 throw settings). Zero warnings.
2. **Exhaustive 2048-cell compile sweep** — one `clang++ -fsyntax-only`
   per combination of the ten pad flags × the throw axis. Every cell
   must compile clean under `-Werror`, which also means all eight
   `static_assert`s pass in every cell. Result: **2048/2048**.
3. **Runtime matrix** — 28 builds actually executed, each required to
   exit 0 with `FAIL=0` and **empty stderr**. Result: **28/28**.
4. **ASan + UBSan** — five representative cells (default, BANG, all
   eleven, throw+BANG, throw+all-eleven) with
   `-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer`,
   run with `ASAN_OPTIONS=detect_leaks=0`. Zero diagnostics, **empty
   stderr** on all five.
5. **Three-way cross-check** — Python model vs preprocessor priority
   chain vs un-offset rank formulation, plus the ten-pad vs
   eleven-pad equivalence over all 512 BANG-off cells: **0
   disagreements** across 1024 cells.
6. **Flakiness** — five consecutive runs on four cells produced
   identical tallies every time (Section 10's 4-thread × 25-call
   concurrent dispatch × 5 runs × 4 cells = 500 concurrent calls).
7. **CMake** — `-DENABLE_STRICT_WARNINGS=ON` builds and runs clean for
   both the default cell and the all-eleven-pads cell, empty stderr.

Harness summary: `pass=38 fail=0`.

### PASS counts differ per cell, and that is expected

The assertion count is not constant across build cells, which is
normal for this arc and worth reading correctly rather than treating
as flakiness:

| cell | PASS | why |
| --- | ---: | --- |
| none / space / dash / hash / tilde / under / star / at / plus | 55 | a filling cell skips Section 8's NONE branch |
| none (throw axis) | 59 | + the THROW=1 TooLarge checks in Section 5 |
| bang, all11, bang+plus, bang+nul | 58 | + Section 8's BANG branch |
| bang + all its variants on the throw axis | 59 | BANG branch + throw checks |

Every one of them is `FAIL=0`.

Verification scripts and every generated artifact live under
`/tmp/petra20261023/`. No compiled binary is ever staged; build outputs
go straight to `/tmp`.

## Notes / gotchas worth remembering

* **A lambda cannot call itself from inside its own initializer.**
  `auto max4 = [&](...){ return max2(...); };` where `max4`'s body
  calls a sibling lambda captured by reference is fine, but making
  `max4` recurse into `max4` is not: the name is not in scope until
  the initializer completes. Clang says
  `candidate function not viable: requires 4 arguments, but 2 were
  provided` — which reads like an arity bug, not a scoping one. Spell
  the power-of-two family out at each level instead.
* **`-Wshadow` caught a real one.** `const int want = ...` inside a
  block shadowed the `std::string_view want = "NONE"` from the referee
  chain above it. Renamed to `want_score`. Worth keeping the strict
  flag set on precisely because the shadowing was harmless-looking.
* **macOS bash is 3.2.** `set -u` plus an empty array expansion
  (`"${arr[@]}"` when `arr=()`) is an *unbound variable* error, not a
  no-op. Pass build flags through a plain string and let word
  splitting do the work.
* **Never `chmod +x` a script you are about to run in the same
  command** in an unattended session — it trips an approval prompt.
  Invoke it as `bash script.sh`.
* `/usr/bin/g++` on this machine is Apple clang, so a "g++" result is
  not an independent second implementation. (Carried over from Oct 22's
  notes; still true.)

## Where we go next

1. **Port the `char*`+`cap` axis onto the same generic wrapper** —
   still open from Oct 22's item (1). Oct 18 / Oct 20 built four
   `char*`+`cap` wrappers per pad by hand; the `template <bool Fills,
   char Fill>` shape has been proven on the `std::span<char>` axis and
   the `char*`+`cap` overload is mechanical.
2. **Use the runtime selector for the fill loop too.** Today's
   `select_pad` decides *which* pad; the fill itself is still
   compile-time `if constexpr (Fills)`. A runtime `fill_tail(buf,
   written, choice)` would let one binary serve all 1024 cells
   completely — including the fill, not just the selection.
3. **Make the rank order itself data.** Today the ranks live in five
   places (preprocessor scores, FILLS cascade, CODE cascade, NAME
   cascade, constexpr mirror). A single X-macro list
   (`#define PETRA_PAD_LIST(X) X(NUL,1,0) X(SPACE,2,32) …`) would
   generate all five, so the twelfth pad is one line instead of
   eleven. The cross-check in Section 9 is the safety net for
   attempting it.
4. **Compare the hand-rolled `expected<size_t, FormatError>` contract
   against stdlib's `std::format_to_n`** — carried over from Oct 22's
   item (4).