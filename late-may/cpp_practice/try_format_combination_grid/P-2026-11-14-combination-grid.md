# P-2026-11-14 — try_format_combination_grid

## What today closes

Nov 13's "Where we go next" item (2), verbatim:

> **Nov 12's item (2) — extend the grid past single options.** Every
> one of the 1,596 cells isolates ONE option, and not one of them
> reaches the fallback. Worth measuring a combination grid — e.g. does
> `"{:+c}"` on a `char` fail at phase 1 (sign not in the mask) or
> phase 2 (narrowing)?

Closed, and it found a real defect. The combination grid is **21 option
PAIRS × 19 type chars × 7 argument rows = 2,793 cells**, and the arc's
model disagreed with **libc++ 21** on **SEVEN** of them.

All seven had the same shape: the **align option next to the `0`
option**. The 1,596-cell grid could not have seen this, because none
of its twelve prefixes contains an align option and a `0` option at
the same time — it has `"0"` on its own, and `"<"` / `"*>10"` /
`"<10.2"` on their own, but never the two together.

## THE HEADLINE — one rule, seven cells, zero disagreements

| the combination grid (2,793 cells) | libc++ 21 | libstdc++ 15 |
|---|---|---|
| the model with the **OLD** rule (the `0` is always validity-checked) | **7** disagreements | **0** disagreements |
| the model with **today's** rule (the `0` is ignored when an align option is present) | **0** disagreements | **0** disagreements |
| — of which the OLD rule rejected what the stdlib accepts | 7 | 0 |

The seven cells, all of them align + `0`:

```
bool | {:<0}     char | {:<0}      int  | {:<0c}
bool | {:<0s}    char | {:<0c}     long | {:<0c}
                  char | {:<0?}
```

Under libc++ 21 a `bool` **accepts** `"{:<0}"`. Under the old rule the
arc rejected it, because the arc asked the allow mask "does a bool
accept the zero-padding option?" and the answer is no.

## KEY FINDING #1 — the `0` option and an ALIGN option INTERACT

The two options meet in one place, and the standard has **two
wordings** of what happens there.

**C++20's wording** ([format.string.std], as quoted in P2510R3):

> If the 0 character and an align option both appear, the 0 character
> is ignored.

**The current wording** (P2572R1, which is what the standard says
today):

> The 0 option is valid for arithmetic types other than charT and bool
> or when an integer presentation type is specified. … this option
> pads the formatted argument by inserting the 0 character *n* times …
> where *n* is 0 if the align option is present and is the padding
> width otherwise.

The difference is not about **padding** — both wordings agree that an
align option makes the padding count zero. The difference is about
**validity**:

- C++20's wording: the `0` is *ignored*, so it is never
  validity-checked at all. A `bool` accepts `"{:<0}"`.
- The current wording: the `0` is still an **option**, and its
  validity is decided without reference to the align option. The align
  only zeroes the padding count. A `bool` still rejects `"{:<0}"`.

**libc++ 21 implements the C++20 wording. libstdc++ 15 implements the
current one.** Measured, not assumed: `"{:<0}"` on a `bool` is
**ACCEPTED** by libc++ 21 and **REJECTED** by libstdc++ 15.

And libc++ 21's source says so in one line.
`__format/parser_std_format_spec.h`, `__parse_zero_padding()`:

```cpp
if (*__begin != _CharT('0'))
  return false;

if (__alignment_ == __alignment::__default)
  __alignment_ = __alignment::__zero_padding;
++__begin;
```

The `0` only becomes the zero-padding alignment **when no align option
was given**. The check that follows is on the *alignment*, not on a
"did we see a `0`" flag:

```cpp
if (!__fields.__zero_padding_ && __alignment_ == __alignment::__zero_padding) {
  ... "The format specifier does not allow the zero-padding option"
}
```

So with an align option present, `__alignment_` is `__left` /
`__right` / `__center` and the zero-padding field check never fires.
The `0` is consumed from the input and then dropped on the floor.

That is the whole mechanism, and it is a *rule about two options
meeting*, which is exactly what a grid of single options cannot see.

## KEY FINDING #2 — the fix is a per-implementation datum, not a change to the rule

The arc's shape for this has been settled since Nov 7: **the standard
fixes the rule, each implementation fixes its own enforcement of it.**
So the `0`-with-align rule joins the policy table as a boolean:

```cpp
// ImplPolicy::zero_ignored_with_align
bool zero_ignored_with_align{false};
```

| policy row | `zero_ignored_with_align` | why |
|---|---|---|
| standard | **false** | the current wording keeps the option validity-checked |
| libc++ 21 | **true** | MEASURED: a bool accepts `"{:<0}"`; `__parse_zero_padding` only sets the zero-padding alignment when the alignment is still `__default` |
| libstdc++ 15 | **false** | MEASURED: a bool rejects `"{:<0}"` |

The scanner now records `ParsedSpec::has_align` — the align option has
no bit in the option mask (it is legal for every category), but it
changes what the `0` option *means* — and the zero-padding branch
drops the `0` from the option set when the align option is present
**and** the implementation ignores it.

## KEY FINDING #3 — the chain's own question, answered: PHASE 2

> does `"{:+c}"` on a `char` fail at phase 1 (sign not in the mask) or
> phase 2 (narrowing)?

**Phase 2 — the allow mask.** Measured, and printed by the program:

```
the chain's own question: "{:+c}" on a char
  the model's phase: phase 2 (the allow mask)
  the linked stdlib: REJECTS: The format specifier for a character does not allow the sign option
```

The reason the phase question is worth asking at all: the sign option
*is* in charT's **consume** mask, so the parser reads it — phase 1
passes. What rejects the spec is the **allow** mask, because `'c'` is
not an integer presentation type for charT (charT's integer
presentation types are `bBdoxX`, and charT's own `'c'` means "the
character", which is a different thing). So charT's allow mask is
`{locale}` alone and the sign option has nowhere to go.

libc++ 21 says the same thing in its own words — *"The format specifier
for a character does not allow the sign option"* — which is an
**argument** failure, not a parse failure, exactly as phase 2 says.

## KEY FINDING #4 — the phases over the whole grid

The combination grid also gives the arc a phase census it did not have
before, because the single-option grid was too small to carry one. Over
all 2,793 cells (libc++ 21, today's rule):

| the model's phase | cells |
|---|---|
| accept | 604 |
| phase 1 — the consume mask (the option is not even read) | 1,140 |
| phase 2 — the allow mask (the option is read, the argument refuses it) | 77 |
| phase 3 — the presentation type (the type char is not legal here) | 972 |
| structural (the spec is malformed) | 0 |

Phase 1 dominates, and that is the shape of the grammar rather than a
bug: an option that a category does not consume is left in the input,
and the leftover text is what trips libc++'s *"should consume the input
or end with a `}`"* check. The model's 1,140 phase-1 cells are exactly
the count of libc++'s two "the option was not consumed" messages over
the same grid — **1,064** *"should consume the input or end with a
`}`"* plus **76** *"the width option should not have a leading zero"*
is **1,140**, and the model says 1,140.

## What did NOT change (deliberately)

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order and their meanings are exactly as they have been since
  Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the `TooLarge`
  contract and the buffer-atomicity behaviour.
- The type-category table from Oct 30, Nov 7's mask tables, its derived
  override table, and **its 1,596-cell grid** — which is re-measured
  here and does not move. The program's own assertions for that grid
  (all 1,596 cells agreeing with the linked stdlib, the 10 divergent
  policy cells) still pass unchanged, which is the point: the new rule
  cannot touch a cell that has no align option and `0` option together.
- Nov 13's fill rule, its Unicode decoder, the ill-formed-fill mask,
  and the code-point width measurement.
- The message table from Oct 28. The align/zero rule is a **structural**
  rule now — the scanner decides it — so the message table is not
  consulted for it, and nothing had to be added to it.

## The build

`CMakeLists.txt` follows Nov 13's, with one change: the
`-fexperimental-library` flag is now added **only for Clang**. It is a
libc++-only flag, and the lesson is built and measured against
libstdc++ 15 as well, so an unconditional flag would have stopped
that. The same source, both toolchains, no edits.

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-14-combination-grid
```

## Verification

- Compiled with `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` — clean, no warnings.
- The program's own checks: **111 pass, 0 fail** under libc++ 21, and
  **108 pass, 0 fail** under libstdc++ 15 — the same source, the same
  assertions, three checks fewer because a few of the arc's checks are
  libc++-specific (the fill census, the derived override rows).
- The single-option grid's existing assertions (1,596 cells, the 10
  divergent policy cells, the derived override table, the numeric
  ceiling, the fill census) all still pass — the new rule did not move
  anything.
- An ASan/UBSan build (`-fsanitize=address -fsanitize=undefined`) runs
  the same checks with no sanitizer report.

## Where we go next

1. **The chain item (2) is now half-closed: pairs, not triples.** The
   combination grid measures PAIRS of the seven option slots. The
   first cell that needs all THREE of fill, align and `0` at once is
   the interesting one — the fill's Unicode rule and the align/zero
   rule have never met. Worth measuring a three-slot grid: 35 triples
   × 19 types × 7 rows = 4,655 cells, and asking whether the two rules
   are independent.
2. **The policy table now has four kinds of datum**, not one: mask
   rows (Nov 7), a scalar ceiling (Nov 12), a fault mask (Nov 13) and
   a boolean (today). Worth asking whether "one table per kind of
   per-implementation fact" is still the right shape, or whether the
   four kinds want one table with a tagged union per row.
3. **Nov 13's item (1), still open.** The width-counts-code-points
   rule is measured but not modelled: the arc's own padding — if it
   ever grows one — would have to count code points, not bytes, and
   the two-pass design's "measure then write" would have to agree with
   it on both units.
4. **Nov 13's item (3) — the compile-time path could use the arc's own
   validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. The arc's own
   `consteval` validator would accept what the standard accepts and
   reject what the standard rejects even when libc++ does not.
5. **Nov 13's item (4) — the policy table is the natural place to put
   the *value* rules.** A value-dependent rule (does 65 fit in
   `char`?) has to live somewhere else again — probably a third mask,
   keyed by (category, type), selected only after the value is known.
   Worth measuring whether that third table can be derived the same
   way Nov 7's was.
6. **Nov 13's item (5) — the `fill` is not the only place a Unicode
   scalar value can appear.** The width and precision are ASCII and the
   type char is ASCII, so the fill was the only place the scanner had
   to decode — but the *argument* is also a Unicode string, and the
   standard counts the width of a string argument in code points too.
   Worth measuring whether the two rules want the same table.
