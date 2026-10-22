# P-2026-10-22 — try_format_pad_priority_dispatch

## What today closes

Oct 21's `try_format_plus_pad_span_n` ended with a "Where we go next"
list. Item (2) was:

> **A unified 10-bit-tuple dispatcher `(THROW × PAD × SPACE × DASH ×
> HASH × TILDE × UNDERSCORE × STAR × AT × PLUS)`** — one macro covering
> all composition cells across all ten pad flags, picking the right
> wrapper at preprocessor time. Because the composition rule is a
> strict total order (`PLUS > AT > STAR > UNDERSCORE > TILDE > HASH >
> DASH > SPACE > NUL`), such a dispatcher can select the
> *highest-ranked* enabled pad flag with a simple preprocessor priority
> chain rather than an exhaustive 2^10 table — worth working out
> explicitly in a lesson.

**That is today's lesson, verbatim.**

For eleven lessons (Sep 27 → Oct 21) each pad character arrived as a
thin layer over the previous one, and every lesson *closed the door on
composition* with a friendly `#error`: "if you set `PETRA_AT_PAD=1`
alongside `PETRA_PLUS_PAD=1`, use Oct 19's lesson instead." The
composition rule was described in every commit message and reachable in
**zero** of the 1024 combinations.

Today's lesson makes composition reachable and proves the dispatcher is
the *right* shape — a max, not a table.

## The problem being solved

Eleven lessons × ~4 hand-written wrappers per axis produced roughly 40
near-identical functions that all do the same three-line fill loop with
a different `char` literal:

```cpp
for (std::size_t i = written; i < cap; ++i) {
    out[i] = '@';   // or '+', '*', '_', '~', '#', '-', ' ', '\0'
}
```

To reach the 1024 compositions you would need either

* **1024 `#if`/`#elif` rows** — correct but a reviewer must trust 1024
  lines of preprocessor trivia by eye, and the mapping is written out
  by hand rather than derived; or
* **a priority chain** — score each flag, take the max.

Today's lesson takes the second option and *measures* that it is the
same answer, dramatically cheaper, and provably total.

## KEY FINDING #1 — a preprocessor MAX is a right-fold of MAX2

```c
#define PETRA_PAD_MAX2(a, b) ((a) > (b) ? (a) : (b))
#define PETRA_PAD_MAX3(a, b, c) PETRA_PAD_MAX2(PETRA_PAD_MAX2(a, b), c)
#define PETRA_PAD_MAX5(a, b, c, d, e) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX3(a, b, c), PETRA_PAD_MAX2(d, e))
#define PETRA_PAD_MAX10(a, b, c, d, e, f, g, h, i, j) \
    PETRA_PAD_MAX2(PETRA_PAD_MAX5(a, b, c, d, e), PETRA_PAD_MAX5(f, g, h, i, j))
```

Fold by halves: 10 values → 2 `MAX5`s → 1 `MAX2`. Nine comparisons
instead of a 1024-row ladder.

The rank table is deliberately offset by one:

```
score(flag) = flag ? rank + 1 : 0          rank: NUL 1 … PLUS 9
score(NONE) = 1
```

so the max is well defined in **all** 1024 cells:

* every disabled pad scores 0,
* NONE scores 1,
* every enabled pad scores 2..10.

NONE therefore sits strictly above "all disabled" and strictly below
"any enabled" — it can never tie an enabled pad, which is what makes
the winner-detection ternaries below safe.

**Measured, not asserted.** `/tmp/petra20261022/cross_check.py`
generates both implementations of the same spec and cross-checks them
against a third, independent Python model:

| implementation | header size | mechanism |
| --- | ---: | --- |
| priority chain (`MAX2` fold) | 24 lines | 9 score macros + fold + name cascade |
| exhaustive ladder | 1055 lines | 512 `#if`/`#elif` rows |

**44× smaller**, and the three-way cross-check (Python model vs chain
vs ladder) over all 512 pad combinations:

```
chain header  : 24 lines (9 score macros + MAX2 fold + name cascade)
ladder header : 1055 lines (44.0x larger)
three-way agreement: 512/512
```

Note the ladder is not a strawman in *cost* — it preprocesses in
roughly the same time for a single cell, because the preprocessor
skips the untaken `#elif` arms. What the ladder costs is **reviewable
surface**: 1055 lines a human must trust by eye, versus 24 lines whose
correctness follows from `max`.

## KEY FINDING #2 — "no fill" and "NUL fill" are DIFFERENT states

This is the bug the naive dispatcher hides. `PETRA_PAD=1` legitimately
writes `'\0'` bytes; no pad at all writes nothing. Both have fill byte
`0`. A dispatcher that returns only "the byte" cannot distinguish them,
and collapsing them would silently turn the no-fill contract into a
NUL-fill contract — a behavioural change hidden behind a refactor.

So today carries **two** selection outputs:

```c
PETRA_PAD_SELECTED_FILLS   /* 0 for NONE, 1 for every enabled pad
                              (including NUL) */
PETRA_PAD_SELECTED_CODE    /* the winning fill byte; 0 for NONE and NUL */
```

and the wrapper takes them as two separate template parameters:

```cpp
template <bool Fills, char Fill, typename... Args>
std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span(std::span<char> out,
                                 std::string_view fmt, const Args&... args);
```

`if constexpr (Fills)` means the NONE cell emits **no fill loop at
all** — not a runtime branch that is always false. Section 8 of the
program pins the distinction directly: in the NUL cell the tail bytes
really are `'\0'`; in the NONE cell they are still poison `'Q'`.

Four template instantiations (runtime/consteval × expected/throwing)
cover every cell the dispatcher can select — including all 512 pad
combinations, which is the point.

## KEY FINDING #3 — the two chains must be pinned against each other

The preprocessor chain (`#if`-folded scores) and the C++ chain
(`constexpr` `pad_score_max` over a fold expression) are **two
independent implementations of the same specification**. Three
`static_assert`s run in *every* build:

```cpp
static_assert(detail::kSelectedFills == PETRA_PAD_SELECTED_FILLS, ...);
static_assert(detail::kSelectedCode  == PETRA_PAD_SELECTED_CODE,  ...);
static_assert(detail::kSelectedName  == PETRA_PAD_SELECTED_NAME,  ...);
```

They catch "I edited the `#if` chain but forgot the constexpr chain" at
compile time for whatever cell you happen to be building — and because
the sweep compiles **all 1024 cells** (nine pad flags × the throw
axis), they catch it for *every* cell.

## A C++ rule I got wrong first

My first draft of the constexpr mirror was:

```cpp
constexpr int pad_score_max(int first, int... rest) { ... }   // WRONG
```

Clang rejects it:

```
error: type 'int' of function parameter pack does not contain any
       unexpanded parameter packs
```

**A function parameter pack is only legal in a function *template*.**
Verified on Apple Clang 21 and on `/usr/bin/g++` (which is a clang
symlink on this machine, so both agree). The fix is a type template
pack plus a fold expression:

```cpp
template <typename... Ints>
constexpr int pad_score_max(int first, Ints... rest) noexcept {
    int m = first;
    (((static_cast<int>(rest) > m) ? (m = static_cast<int>(rest)) : 0), ...);
    return m;
}
```

A first attempt used `std::max` inside the fold
(`(std::max(first, rest), ...)`), which compiled but emitted **8
`-Wunused-result` warnings** — `std::max` returns `[[nodiscard]] const&`,
and every discarded fold step counts as an ignored return. With
`-Werror` that is a hard failure, so the explicit ternary is the
correct spelling, not just the quieter one.

## Rank table and byte codes

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

Section 1 pins every rank and every ASCII code with `static_assert`,
including Oct 21's adjacency fact `'+' == '*' + 1`, and pins the
strict total order pairwise
(`rank(PLUS) > rank(AT) > … > rank(NUL)`).

## The dispatcher

```c
#define TRY_FORMAT_RUNTIME_SPAN_N_PRIORITY(out, fmt, ...)            \
    (::petra::try_format_runtime_n_padded_span<                       \
         PETRA_PAD_SELECTED_FILLS != 0,                              \
         static_cast<char>(PETRA_PAD_SELECTED_CODE)>((out), (fmt)     \
         __VA_OPT__(,) __VA_ARGS__))
```

Both surfaces (`..._RUNTIME_...` and `..._BOUNDED_...`, expected and
throwing) are parenthesized expressions, so the caller captures the
result — the Sep 13 … Oct 21 macro shape. `petra::unwrap_dispatch`
adapts the two possible return types so **one** test call site compiles
against both.

## Sections in the program

| # | what it pins |
| --: | --- |
| 1 | rank table, ASCII codes, strict total order (compile-time) |
| 2 | chain invariants + a third, plainly-written chain as a referee |
| 3 | meta-macro success path honours the selected pad |
| 4 | consteval (`std::format_string`) surface through the macro |
| 5 | buffer UNTOUCHED on TooLarge (both throw cells included) |
| 6 | Format-error atomicity + sub-span no-bleed |
| 7 | `written == span.size()` and `span.size() == 0` edge cases |
| 8 | NONE vs NUL are distinct contracts |
| 9 | 4 threads × 25 calls concurrent dispatch |
| 10 | summary |

Every section is written against **the selected pad**, so the same
source file is the test for all 1024 build cells.

## Build and run

Default (expected, no pad):

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-22-try-format-pad-priority-dispatch
```

The composition cell — legal for the first time in this arc:

```bash
cmake -S . -B build-all-pads \
      -DPETRA_PAD=ON -DPETRA_SPACE_PAD=ON -DPETRA_DASH_PAD=ON \
      -DPETRA_HASH_PAD=ON -DPETRA_TILDE_PAD=ON \
      -DPETRA_UNDERSCORE_PAD=ON -DPETRA_STAR_PAD=ON \
      -DPETRA_AT_PAD=ON -DPETRA_PLUS_PAD=ON
cmake --build build-all-pads
./build-all-pads/P-2026-10-22-try-format-pad-priority-dispatch
```

Strict warnings and sanitizers:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake -S . -B build-asan -DENABLE_ASAN=ON
```

Or directly:

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow \
        -Wconversion -Wsign-conversion \
        P-2026-10-22-try-format-pad-priority-dispatch.cpp -o /tmp/petra
```

## Observed output

Default cell (`PETRA_THROW_ON_OVERFLOW=0`, no pad flags):

```
selected pad = NONE  (fills=0, byte=0 (0x00), score=1)
...
  selected pad = NONE
  selected byte = 0  fills = no
  PASS=38  FAIL=0
```

All-pads cell (all nine flags on — PLUS wins):

```
selected pad = PLUS  (fills=1, byte=43 (0x2B), score=10)
...
  selected pad = PLUS
  selected byte = 43  fills = yes
  PASS=37  FAIL=0
```

Throw axis, all pads:

```
  selected pad = PLUS
  selected byte = 43  fills = yes
  PASS=38  FAIL=0
```

NUL cell (`PETRA_PAD=1`) is the one that exercises the FILLS/CODE split:

```
  selected pad = NUL
  selected byte = 0  fills = yes
  PASS=39  FAIL=0
```

PASS counts differ per cell because Sections 5, 6 and 7 run different
assertion sets on the throw axis versus the expected axis, and Section
8 runs extra assertions only in the NONE/NUL cells. Zero FAIL on every
cell.

## Verification performed

1. **Strict-warning builds** — `-Wall -Wextra -Wpedantic -Werror
   -Wshadow -Wconversion -Wsign-conversion` on nine distinct
   flag combinations (default, all nine pads, PLUS, NUL, SPACE, AT+STAR,
   throw×3). Zero warnings.
2. **Exhaustive 1024-cell sweep** — one `clang++ -fsyntax-only` per
   combination of the nine pad flags × the throw axis. Every cell must
   compile clean under `-Werror`, which also means all three
   `static_assert`s (the two-chains-agree check) pass in all 1024
   cells. Result: **1024/1024**.
3. **Runtime matrix** — 26 builds actually executed (13 distinct pad
   combinations × 2 throw settings), each required to exit 0 with
   `FAIL=0`. Result: **26/26**.
4. **ASan + UBSan** — four representative cells built with
   `-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer`
   and run with `ASAN_OPTIONS=detect_leaks=0`. Zero diagnostics,
   **empty stderr** on all four.
5. **Three-way cross-check** — Python model vs preprocessor priority
   chain vs exhaustive 1055-line `#if` ladder, all 512 pad
   combinations: **512/512 agree**.

Verification scripts and every generated artifact live under
`/tmp/petra20261022/`. No compiled binary is ever staged; the build
outputs go straight to `/tmp`.

## Notes / gotchas worth remembering

* **`/usr/bin/g++` on this machine is Apple clang**, not GCC. Don't
  read a "g++" result as an independent second implementation.
* A macro invocation `FOO` is only expanded when followed by `(`. To
  print a function-like macro's value, wrap it in a second macro:
  `#define ANSWER FOO`. This broke two earlier cross-check harnesses,
  which appeared to disagree when actually both were echoing the
  expanded *body*.
* Comparing object-like macro values with `clang++ -E -P` gives you the
  expanded text, not the value. Evaluate inside `#if`, or compile and
  use the process exit code, when you want the value.
* A `#define` whose body spans multiple physical lines needs a trailing
  backslash on *every* line, including inside the `#if` that compares
  it. A missing backslash surfaces as "unterminated function-like
  macro invocation" pointing at an unrelated line.
* Two of my harness bugs were **tuple-unpacking order** errors
  (`for n, _, b, _, _ in FLAGS` where the second slot is `bit`, not
  `rank`). Both produced plausible-looking output rather than an
  exception: the chain and the ladder disagreed, but in a way that
  looked like a real bug in the chain. When two independent
  implementations of a spec disagree, suspect the *harness* before
  suspecting either implementation — and make one of the three
  implementations trivially simple (the Python model) so it cannot be
  wrong in an interesting way.

## Where we go next

1. **Port the CHAR\*+CAP axis onto the same generic wrapper** — Oct 18 /
   Oct 20 built four `char*`+`cap` wrappers per pad; today proves the
   `template <bool Fills, char Fill>` shape works for the span axis.
   A `char*`+`cap` overload is a small, mechanical follow-up that would
   let both axes share one dispatcher.

2. **An ELEVENTH pad** (`PETRA_BANG_PAD`, `'!'`, ASCII 33). Adding a
   pad to the chain is now: one rank constant, one score macro, one
   ternary arm in FILLS/CODE/NAME, one entry in the constexpr mirror,
   and one `static_assert`. The 1024-cell sweep is the regression net.
   Worth doing at least one new pad this way to prove the *shape*
   generalises rather than just the current nine.

3. **Make the dispatcher runtime-configurable** — the ten flags are
   compile-time constants today. A `constexpr` selection function
   already exists in Part 8, so a runtime `PadConfig` struct + a
   templated fill loop is the natural next step, and it would let a
   single binary serve all 1024 cells instead of 1024 compiles.

4. **Left-`char`+`cap` axis parity for `std::format_to_n`** — the
   bounded-buffer wrapper has been hand-rolled since Sep 8; stdlib has
   `std::format_to_n` now. Comparing the two error contracts
   (`expected<size_t, FormatError>` vs `output_buffer_iterator` with
   `result.output_size()`) would be a genuinely interesting lesson.