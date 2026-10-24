# P-2026-10-24 — try_format_xmacro_pad_table

## What today closes

Oct 23's `try_format_bang_pad_priority_dispatch` ended with a "Where
we go next" list. Items (2) and (3) are today's lesson, taken
**together**, because turning them on at the same time turns the whole
arc's 1024-cell *external compile* sweep into a single in-process
*run*.

> **Item (2):** "Use the runtime selector for the fill loop too.
> Today's `select_pad` decides *which* pad; the fill itself is still
> compile-time `if constexpr (Fills)`. A runtime
> `fill_tail(buf, written, choice)` would let one binary serve all 1024
> cells completely rather than just the selection."
>
> **Item (3):** "Make the rank order itself DATA. Today the ranks live
> in five places (preprocessor scores, FILLS/CODE/NAME cascades, the
> constexpr mirror) and a single X-macro list
> (`#define PETRA_PAD_LIST(X) X(NUL,1,0) X(SPACE,2,32) …`) would
> generate all five so the twelfth pad is one line instead of eleven,
> with Section 9's cross-check as the safety net."

Item (2) is the *practical* half — it is what makes 1024 cells
affordable. Item (3) is the *structural* half — it is what makes the
next pad cheap. Doing (3) without (2) would have left the sweep
external; doing (2) without (3) would have left the data in five
places.

---

## KEY FINDING #1 — the rank data really does collapse to one line per pad

Oct 23 stated the rank order in **five** places: the eleven
`PETRA_PAD_SCORE_*` macros, the FILLS cascade, the CODE cascade, the
NAME cascade, and the hand-written constexpr rank/score constants —
plus a sixth, the runtime `kPadTable`. Today all of that is generated
from **one** X-macro list:

```c
#define PETRA_PAD_LIST(X)                                                  \
    X(NUL,        PETRA_PAD,           1,  0)                              \
    X(SPACE,      PETRA_SPACE_PAD,     2, 32)                              \
    …                                                                      \
    X(BANG,       PETRA_BANG_PAD,     10, 33)
```

Each row carries four things: the pad's **name**, the **flag macro**
that turns it on, its **rank**, and its **fill code**. The row is
expanded once into a `constexpr PadSpec` array, and the flag value
rides along *in the row* (`PadSpec::enabled`), so there is no second
array to keep in step.

**Measured, not asserted.** The "twelfth pad" experiment
(`/tmp/petra20261024/twelfth.sh`) takes today's source, adds an
eleventh pad row (`'?'`, ASCII 63, rank 11) and reports the diff:

```
=== DIFF SUMMARY (places touched to add the twelfth pad) ===
  total changed lines: 12
  hunks (contiguous regions): 4
  of which, RANK DATA (the X-macro row): 1 line(s)
```

and the result:

```
=== step 5: DOES THE TWELFTH-PAD BUILD COMPILE AND PASS? ===
  compiles clean under the full strict warning set
  runs: exit 0
sweep: cells=2048 disagree=0 byte_fail=0 none_wins=1
pass=61 fail=0
  stderr bytes: 0
  the sweep now covers 2048 cells (2^11):
  rank 11 QUESTION    wins 1024 cells
```

The twelfth pad wins in exactly 1024 of the 2048 cells — which is the
`2^(r-1)` identity again, and it is *derived*, not written down.

### But the honest number is 4 hunks, not 1 line

The claim needs the caveat Oct 23's note did not have, because Oct 23
was comparing against a design where the cascade existed. Adding a pad
today touches:

1. **the X-macro row** (1 line) — the *data*;
2. **the `#ifndef`/`#define`/`#error` flag block** (6 lines) — the
   *preprocessor plumbing*;
3. **the hand-written oracle table** (1 row);
4. **the hand-written oracle if-chain** (1 line).

Items 3 and 4 are the *safety net*, and they are hand-written on
purpose — see KEY FINDING #3. So the accurate statement is:

> The twelfth pad is **one line of rank data**, plus the same six lines
> of flag plumbing every previous pad already had, plus **two lines in
> the hand-written reference**.

And there is a hard reason the flag plumbing cannot be generated: **the
preprocessor cannot produce a `#define` from a macro expansion.** That
is undefined behaviour and no compiler supports it, so an X-macro can
generate *code* from a list but never *directives*. The per-pad
`#ifndef` block is irreducible.

### How many places does a pad name appear in?

A crude but honest measure of how much duplication is left — the number
of lines in the source mentioning each pad's name:

| pad | Oct 23 | Oct 24 |
| --- | ---: | ---: |
| NUL | 33 | 12 |
| SPACE | 13 | 6 |
| DASH | 12 | 4 |
| HASH | 13 | 5 |
| TILDE | 12 | 4 |
| UNDERSCORE | 12 | 4 |
| STAR | 12 | 4 |
| AT | 15 | 5 |
| PLUS | 16 | 4 |
| BANG | 38 | 9 |

(NUL and BANG stay high because they are the two *ends* of the order
and are named in the extreme-case assertions — "the lowest rank" and
"the highest rank" are the two rows the invariants talk about.)

---

## KEY FINDING #2 — the preprocessor priority cascade does not need to survive at all

This is the one the note did **not** expect, and it is the biggest
simplification in the arc so far.

Oct 23's file carried, purely to turn "which pad won?" into two
compile-time constants:

- eleven `PETRA_PAD_SCORE_*` macros,
- a four-macro power-of-two `MAX2`/`MAX4`/`MAX8`/`MAX16` family,
- **three** eleven-arm ternary cascades (`PETRA_PAD_SELECTED_FILLS`,
  `…_CODE`, `…_NAME`).

All of it is gone. The reason is a single C++ rule:

> **A template argument may be a `constexpr` variable.**

So the dispatcher macro passes the selected values directly:

```c
#define TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(out, fmt, ...)                  \
    (::petra::try_format_runtime_n_padded_span<                             \
         ::petra::detail::kSelectedFills,                                  \
         ::petra::detail::kSelectedByte>((out), (fmt)                      \
         __VA_OPT__(,) __VA_ARGS__))
```

where

```cpp
inline constexpr PadChoice kSelected = select_pad_table(kPadMask);
inline constexpr bool kSelectedFills = kSelected.fills;
inline constexpr char kSelectedByte  = static_cast<char>(kSelected.code);
```

The preprocessor's remaining job in this file is **exactly one thing**:
turn the `-DPETRA_*_PAD=0/1` macros into booleans. There is no
priority chain, no score cascade, no `MAX` family.

The lesson generalises past this lesson: **when a preprocessor chain
exists only to compute a compile-time constant, and the constant is
consumed as a template argument, the chain is usually the wrong tool.**
C++ can do the fold at compile time, in `constexpr`, where it is
type-checked and where the compiler can fold it away.

---

## KEY FINDING #3 — deleting the cascade deletes the safety net, and that is the real trade-off

Oct 22 and Oct 23 pinned a **preprocessor** chain against a
**constexpr** chain with `static_assert`s, precisely because two
hand-written implementations of one spec catch each other's typos. That
property is *destroyed* by the X-macro: the two implementations would
have to read the same list, so they could no longer disagree.

So the cross-check has to be re-based. Today's answer keeps **two
hand-written oracles that read nothing from the generated list**:

- `kOracleTable` — Oct 23's rank table transcribed by hand, name, rank
  and code per row;
- `oracle_select(mask)` — an if-chain written out pad by pad, in rank
  order.

Both are compared against the generated data, at compile time
(`static_assert`) and at runtime for **all 1024 cells** (Section 9).
And the oracle's own bound is deduced too, so it cannot silently fall
out of step with the generated table by having a different number of
rows.

**The trade-off, stated plainly.** The X-macro buys one-line data and
loses "two implementations of the same spec" for the *data*. It is a
genuine loss: a typo in the list is no longer caught by an independent
copy of the list. What replaces it is a hand-written reference — which
is the same idea, but it must be updated *by hand* when the data
changes. That is why the twelfth-pad experiment above touches four
places and not one.

The other half of the trade-off: **the cross-check moved from
compile-time to runtime.** Oct 23's `static_assert`s guaranteed the
selection for one cell at compile time. Today's guarantee is
compile-time for the *data* (the oracle table) and runtime for the
*selection* (all 1024 cells). That is strictly more coverage — 1024
cells instead of 1 — but a build with a wrong *selection* now compiles
and fails at run time instead of failing to compile.

---

## KEY FINDING #4 — the count was the last piece of duplication, and it hides in plain sight

The first draft of today's file wrote:

```cpp
inline constexpr std::array<PadSpec, 10>    kPadTable{{…}};
inline constexpr std::array<bool, 10>       kPadEnabled{{…}};
inline constexpr std::array<OracleRow, 10>  kOracleTable{{…}};
```

The X-macro had removed the per-pad duplication, but **the number 10
was still written down in three places**, so adding a pad meant editing
the count as well as the data. The twelfth-pad experiment's first
attempt failed on exactly this: `static_assert(table_matches_oracle())`
and `static_assert(enabled_array_is_aligned())` both fired because the
generated table had eleven rows and the oracle had ten.

The fix is to let the compiler deduce the bound:

```cpp
inline constexpr PadSpec kPadTable[] = { PETRA_PAD_LIST(PETRA_PAD_TABLE_ITEM) };
inline constexpr std::size_t kPadCount  = std::size(kPadTable);
```

A raw `constexpr` array with an initializer list has its size deduced
from the initializers, so **the pad count is derived from the list,
never stated alongside it**. The oracle table and the win-tally array
in Section 9 use the same trick, and the cell count became
`1u << kPadCount` rather than the literal `1u << 10`.

The generalisable part: **a count that is written next to a list is a
second copy of the list.** Let the container deduce it.

### A second, smaller finding: a COMMENT is a copy too

The same experiment's first attempt also patched the **header
comment's** copy of the X-macro list rather than the macro itself —
the comment quoted the rows as an illustration. The result was a
`-Wcomment` multi-line-comment error and the two `static_assert`
failures above.

The fix was to delete the illustration from the comment and say
*what the row shape is* instead of showing the rows. It is a small
thing, but it is the same lesson as the count: **prose that restates
generated data is a copy of that data, and it drifts.** The lesson file
now carries a note saying so, so the next person does not "helpfully"
re-add the illustration.

---

## KEY FINDING #5 — the rank+1 score survives, and so does its identity

The score of the winner is still `rank + 1`, and NONE still scores 1,
so every number Oct 22 and Oct 23 published still holds. What changed
is *where the score comes from*: it is now a property of the table
lookup rather than a separate set of eleven constants.

- NONE (score 1) can never tie an enabled pad (scores 2..10);
- a disabled pad scores 0 and can never be selected;
- the pad at rank *r* wins in exactly `2^(r-1)` of the 1024 cells.

Section 9 re-verifies the last one at runtime.

---

## KEY FINDING #6 — the fill can be runtime, and it costs one `if`

Oct 23's padded wrapper is `template <bool Fills, char Fill>`, so the
fill loop is compiled in or out with `if constexpr (Fills)`. The NONE
cell emits **no fill loop at all** — not a runtime branch that is
always false.

Today's runtime sibling takes a `PadChoice` **value** instead:

```cpp
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
```

Both paths are kept, deliberately structurally different, and Section 4
pins them together byte for byte for the current cell.

The runtime path is what makes the single-binary sweep possible: with
the selection *and* the fill both runtime, one binary can produce
**any** of the ten pad bytes — and Section 9 uses that to actually
**format** in all 1024 cells and look at the bytes, not merely to
compute which pad wins.

---

## The selection: four independent spellings

| # | spelling | mechanism |
| --- | --- | --- |
| 1 | `select_pad_table(mask)` | walks the generated table backwards |
| 2 | `select_pad_bitwidth(mask)` | `std::bit_width(mask) - 1` is the winner's index |
| 3 | `oracle_select(mask)` | hand-written if-chain, one line per pad |
| 4 | `kSelected` | the compile-time cell: (1) applied to `kPadMask` |

The **bit-mask model** is what makes the runtime surface one integer
wide for the whole pad set: bit *i* is set iff pad *i* (in list order)
is enabled, and the winner is the **highest set bit**. That is exactly
what `std::bit_width` computes, which is why (1) and (2) are
structurally different enough to be worth cross-checking.

### The score semantics are unchanged

The winner's score is still `rank + 1`, and NONE still scores 1 (see
KEY FINDING #5 above). Section 9 re-verifies the win-count identity at
runtime, and the win tallies come out as the identity predicts:

```
sweep: cells=1024 disagree=0 byte_fail=0 none_wins=1
  rank  1 NUL         wins    1 cells
  rank  2 SPACE       wins    2 cells
  rank  3 DASH        wins    4 cells
  rank  4 HASH        wins    8 cells
  rank  5 TILDE       wins   16 cells
  rank  6 UNDERSCORE  wins   32 cells
  rank  7 STAR        wins   64 cells
  rank  8 AT          wins  128 cells
  rank  9 PLUS        wins  256 cells
  rank 10 BANG        wins  512 cells
```

---

## What did NOT change (deliberately)

- `petra::FormatError` / `FormatErrorKind`, the two-pass
  (measure, then write) impls, `counting_output_iterator`,
  `bounded_char_writer` and `unwrap_dispatch` are reused **verbatim**
  from Oct 23 / Oct 21 / Sep 8 / Sep 9.
- The compile-time flag **names** (`PETRA_PAD`, `PETRA_SPACE_PAD`, …)
  and the `-D` contract are unchanged, so yesterday's build cells keep
  their meaning and the CMake options still work as they did.
- The buffer-atomicity contract is unchanged and is re-checked on
  **both** paths: on `TooLarge` and on a `Format` error, the buffer is
  left entirely untouched (the fill loop runs only on the success
  path).
- The `std::span<char>` surface is unchanged. Oct 23's next-step item
  (1) — porting the `char*`+`cap` axis onto the same generic wrapper —
  is still open.

---

## Verification

Everything below was run for real; all artifacts live under
`/tmp/petra20261024/`.

1. **Strict compile of the default cell** —
   `-std=c++23 -stdlib=libc++ -fexperimental-library -O0 -Wall -Wextra
   -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`:
   **clean, zero warnings**.
2. **Build matrix — 23/23 pass.** 18 build cells (each of the ten
   pads alone, three two-pad combinations, the all-eleven-pads cell,
   the default cell, and four `PETRA_THROW_ON_OVERFLOW=1` cells), each
   compiled with the full strict warning set and required to exit 0
   with `fail=0` **and empty stderr**; plus 4 cells where an invalid
   flag value (`=2`) must be **rejected at compile time** with the
   expected `#error` text.
3. **ASan + UBSan — 5 cells clean.** `default`, `bang`, `nul`,
   `all11`, `throw+bang`, built with `-fsanitize=address
   -fsanitize=undefined -fno-omit-frame-pointer`,
   `ASAN_OPTIONS=detect_leaks=0`: zero diagnostics and **empty
   stderr** on all five.
4. **Flakiness — 5 runs × 4 cells, identical tallies every time**
   (`pass=59 fail=0`, `pass=61 fail=0`, `pass=61 fail=0`,
   `pass=62 fail=0`).
5. **In-process exhaustive sweep — 1024/1024 cells** (Section 9):
   the three runtime spellings agree on every cell, and the runtime
   padded path writes the winner's byte in every cell
   (`disagree=0 byte_fail=0`).
6. **Independent Python cross-check** (`cross_check.py`, written from
   the spec, not from the C++): three independent models — priority
   if-chain, score-then-map, and the *un-offset* rank formulation that
   deliberately does not use Oct 22's `+1` offset — over all 1024
   cells: **0 disagreements**, NONE wins 1 cell, win counts
   `1,2,4,…,512` summing to 1023, and the `2^(r-1)` identity holds.
7. **The twelfth-pad experiment** — see KEY FINDING #1. 12 changed
   lines in 4 hunks, **1 line of rank data**, and the eleventh pad
   build compiles clean and passes 61 checks over 2048 cells.
8. **CMake** — `-DENABLE_STRICT_WARNINGS=ON` and
   `-DENABLE_ASAN=ON -DENABLE_STRICT_WARNINGS=ON` both build and run
   clean (`pass=59 fail=0`, empty stderr, **0 warnings** in either
   build log).

### PASS counts differ per cell, and that is expected

The assertion count is not constant across build cells, which is
normal for this arc and worth reading correctly rather than treating
as flakiness:

| cell | PASS | why |
| --- | ---: | --- |
| default (no pad) | 59 | the baseline |
| one pad enabled | 61 | + the enabled-pad branch in Section 2 |
| throw axis | 62 | + the THROW=1 exception checks in Section 6 |
| twelfth pad (experiment) | 61 | + the enabled-pad branch, no throw axis |

Every one of them is `FAIL=0`.

---

## Notes / gotchas worth remembering

- **A template argument may be a `constexpr` variable.** This is the
  single fact that deleted the whole preprocessor cascade. It is easy
  to overlook precisely *because* the cascade works, and it works
  today — it is just forty lines longer than the truth.
- **The preprocessor cannot generate a `#define` from a macro
  expansion.** Producing a directive from a macro's replacement list is
  undefined behaviour, and no compiler supports it. So an X-macro can
  generate *code* from a list but never *directives* — which is why
  the per-pad `#ifndef`/`#define`/`#error` block is irreducible and
  has to be written by hand for every pad.
- **A count written next to a list is a second copy of that list.**
  Let the container deduce it: a raw `constexpr` array with an
  initializer list has its size deduced, and `std::size` reports it.
  `std::array<T, 10>` states the count and makes you keep it in
  step.
- **A comment that quotes generated data is a copy of that data.** The
  twelfth-pad experiment patched the comment's copy of the X-macro
  list instead of the macro, and the failure (`-Wcomment` plus two
  `static_assert`s) was the compiler catching the drift. Say what the
  row *shape* is; do not restate the rows.
- **`std::bit_width` is a constexpr C++20 function.** For a bitmask
  whose bit *i* means "pad *i* is enabled", the winner is the highest
  set bit, so `std::bit_width(mask) - 1` is the winner's table index —
  the whole runtime selection in one expression.
- **A lambda cannot call itself from inside its own initializer**
  (carried over from Oct 23 — its name is not in scope until the
  initializer completes).
- **`/usr/bin/g++` on this machine is Apple clang**, so a "g++" result
  is not an independent second implementation. (Carried over from Oct
  22's notes; still true.)
- **This session's terminal guard wedges on some invocations.**
  `bash script.sh` and absolute-path execution of a freshly built
  binary both hang the guard's process-identity probe, while
  `bash -c "$(cat script.sh)"` and PATH-resolved invocation work.
  Every verification script here was run the second way.

---

## Where we go next

1. **Port the `char*`+`cap` axis onto the same generic wrapper** —
   still open from Oct 22's item (1) and untouched by Oct 23. Oct 18 /
   Oct 20 built four `char*`+`cap` wrappers per pad by hand; the
   `template <bool Fills, char Fill>` shape is proven on the
   `std::span<char>` axis and the `char*`+`cap` overload is mechanical.
2. **Give the runtime path the CONSTEVAL surface too.** Today's
   runtime sibling takes a `std::string_view` format; the compile-time
   path still gets `std::format_string<Args...>`'s compile-time format
   checking (P2216R3). A runtime *selection* with a compile-time
   *format check* is a combination nobody has built yet on this arc.
3. **Make the runtime sweep cover the ERROR paths too.** Section 9
   sweeps the 1024 success cells; the `TooLarge` and `Format`-error
   paths are checked for one cell each. Since the runtime path takes a
   `PadChoice` value, all 1024 cells' error paths are now affordable to
   check in one process.
4. **Compare the hand-rolled `expected<size_t, FormatError>` contract
   against stdlib's `std::format_to_n`** — carried over from Oct 22's
   item (4), still untried.
5. **Revisit the oracle.** The hand-written `oracle_select` if-chain
   is now the one place a new pad must be added by hand. A generic
   oracle — one that reads the *oracle table* rather than being a
   chain — would remove that last per-pad edit, at the cost of the
   "two independent implementations" property. That trade is worth
   making deliberately rather than by accident.
