# P-2026-11-15 — try_format_triple_grid

## What today closes

Nov 14's "Where we go next" item (1), verbatim:

> **The chain item (2) is now half-closed: pairs, not triples.** The
> combination grid measures PAIRS of the seven option slots. The first
> cell that needs all THREE of fill, align and `0` at once is the
> interesting one — the fill's Unicode rule and the align/zero rule
> have never met. Worth measuring a three-slot grid: 35 triples × 19
> types × 7 rows = 4,655 cells, and asking whether the two rules are
> independent.

Closed. The triple grid is **35 option TRIPLES × 19 type chars × 7
argument rows = 4,655 cells**, and the fill × align × zero
intersection is **7 fill-and-align variants × 2 (the `0` or not) × 19
type chars × 7 rows = 1,862 cells**.

The answer to the chain's question — *are the two rules independent?* —
is **no**, but only in **one cell**, and only on **one path**. The
triple grid found no new rule at all; the intersection found a **real
defect in the arc's own scanner** that neither shipped implementation
can see.

## KEY FINDING #1 — no rule needs THREE options to meet

| the triple grid (4,655 cells) | libc++ 21 | libstdc++ 15 |
|---|---|---|
| the model with **today's** rule | **0** disagreements | **0** disagreements |
| the model with the **OLD** rule (the `0` is always validity-checked) | **14** disagreements | **0** disagreements |

The fourteen cells are all one family, exactly as at the pair level:

```
bool | {:<010}    bool | {:<0L}     char | {:<010c}   char | {:<0L}
bool | {:<010s}   bool | {:<0Ls}    char | {:<010?}   char | {:<0Lc}
                                          char | {:<0L?}   int  | {:<010c}
                                                                        int  | {:<0Lc}
```

**Five** of the 35 triples carry both the align option and the `0`
option — the third slot can be any of the other five — so the family
is 5 × 19 × 7 = **665 cells**, and **14** of them are where the OLD
rule's answer differs.

The lesson's own assertion checks this structurally rather than by
counting: *every* OLD-rule divergence must be in an align+zero triple,
and the count of align+zero triples must be exactly 5. It passes.

So the pair grid's answer was still the answer one level down. Nov 14's
silence was **evidence, not proof**, and today it is proof.

## KEY FINDING #2 — the fill's rule and the align/zero rule are NOT independent

This is the defect, and it is in the **arc**, not in either stdlib.

The scanner records that an align option was present
(`ParsedSpec::has_align`), because the `0` rule needs to know it. On
the **well-formed** fill path it records it. On the **recovery** path —
the path an implementation takes when it substitutes U+FFFD for an
ill-formed fill and carries on — it consumed the fill **and** the
alignment option and recorded **neither**.

```
// parse_spec(), the recovery path, BEFORE today:
if (i + cp.length < n && is_align(spec[i + cp.length])) {
    i += cp.length + 1;              // consumed the align...
}                                    // ...and never recorded it
```

So for a spec like `"{:\x80<0}"` the arc believed that **no** align
option was present, and applied the `0` rule as if there were none.
The rule that was supposed to be independent of the fill's rule was
not: **the fill's rule changed the zero's rule's answer**.

### Why it survived two lessons

Neither shipped implementation can see it:

| | ill-formed fill | the `0` with an align |
|---|---|---|
| **libc++ 21** | **fatal** — every ill-formed fill is rejected | ignored (the C++20 wording) |
| **libstdc++ 15** | **recovered** — U+FFFD is substituted | always validity-checked (the current wording) |

libc++ 21 never reaches the recovery path. libstdc++ 15 reaches it, but
its `0` rule does not consult the align option at all. The two halves
of the defect are in **different** implementations, so **no single
shipped stdlib can exhibit it**. Invisible is not the same as correct.

### The measurement that can see it

Section 13.3 builds the **synthesized policy** that has *both* halves —
it recovers from an ill-formed fill (like libstdc++ 15) **and** ignores
the `0` when an align option is present (like libc++ 21) — and runs
Nov 14's scanner and today's scanner over it:

```
the cells where Nov 14's scanner and today's scanner differ: 4/76
{:<0}  | Nov 14's scanner says REJECT, today's scanner says ACCEPT, the U+FFFD reference says ACCEPT
{:<0}  | Nov 14's scanner says REJECT, today's scanner says ACCEPT, the U+FFFD reference says ACCEPT
{:<0s} | Nov 14's scanner says REJECT, today's scanner says ACCEPT, the U+FFFD reference says ACCEPT
{:<0s} | Nov 14's scanner says REJECT, today's scanner says ACCEPT, the U+FFFD reference says ACCEPT
```

**Four** cells out of 76. The fix is one line — record the align option
on the recovery path — and it is a new **per-implementation datum**,
`FillPolicy::recovery_keeps_align`, so the lesson can *measure* the
difference instead of asserting it. (The same trick
`FillPolicy::unicode` plays for the OLD byte-oriented fill rule.)

## KEY FINDING #3 — the recovery really is a U+FFFD substitution

Measured against the stdlib that recovers (libstdc++ 15): an
ill-formed fill and the **same spec with the fill replaced by U+FFFD**
were compared on **76 cells**, and the stdlib judged them differently
on **0** of them.

That is the invariant the fix restores: after the recovery, the
ill-formed spec and the U+FFFD spec *are* the same spec, so the model
must judge them the same. Today's scanner does; Nov 14's did not.

The two shipped stdlibs were also probed directly, outside the lesson:

```
libstdc++ 15   "{:\x80<5}"  ACCEPT  61 62 EF BF BD EF BF BD EF BF BD   (U+FFFD padding, left-aligned)
libstdc++ 15   "{:\x80>5}"  ACCEPT  EF BF BD EF BF BD EF BF BD 61 62   (U+FFFD padding, right-aligned)
libstdc++ 15   "{:\xC0\xAF<5}"  REJECT  format error: failed to parse format-spec
libc++ 21      "{:\x80<5}"  REJECT  The format specifier contains malformed Unicode characters
```

The alignment option **survives** the recovery — `>` right-aligns and
`<` left-aligns, with the recovered U+FFFD as the fill — which is
exactly what today's scanner now records and what Nov 14's did not.

## The measurement that did NOT find anything

The triple grid's own question — "is there a rule that needs three
options to meet?" — was answered **no**. The model's verdict agrees
with the linked stdlib on **4,655 of 4,655** cells under both
toolchains. The intersection grid agrees on **1,862 of 1,862** cells,
including the **532** cells under libstdc++ 15 where the stdlib
*recovers* from an ill-formed fill.

A negative result, measured rather than assumed, and it is the
interesting kind: the arc's rule set is **complete at the pair level**,
so the pair grid was the right grid. The defect was not a missing
rule — it was a rule that was *coupled* to another rule by a path
nobody had exercised.

## What changed in the code

One line of real behaviour, in `parse_spec()`:

```cpp
// the recovery path, AFTER today:
if (i + cp.length < n && is_align(spec[i + cp.length])) {
    if (fill.recovery_keeps_align) {
        out.has_align = true;         // NEW: the align SURVIVES the recovery
    }
    i += cp.length + 1;
}
```

Plus the new per-implementation datum it consults:

```cpp
struct FillPolicy {
    bool unicode{true};
    std::uint8_t fatal{0};
    bool recovery_keeps_align{true};   // NEW
};
```

## What did NOT change

Re-measured, and the output is **byte-identical** to Nov 14's for
Sections 1–11 under both toolchains:

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order and their meanings, exactly as since Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the TooLarge
  contract, the buffer-atomicity behaviour.
- The type-category table from Oct 30, Nov 7's mask tables, its derived
  override table, and its 1,596-cell grid.
- Nov 13's fill rule and its fill census; Nov 14's pair grid and the
  align/zero rule.

The diff was run against Nov 14's own binary, compiled from
`origin/main`, and the sections 1–11 output matches exactly.

## Build and run

The same source is built and measured under **both** toolchains, with
no source change between them:

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-15-triple-grid
```

## Verification

- Compiled with `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` — clean, no warnings.
- The program's own checks: **125 pass, 0 fail** under libc++ 21, and
  **122 pass, 0 fail** under libstdc++ 15 — the same source, the same
  assertions, three checks fewer because a few of the arc's checks are
  libc++-specific (the fill census, the derived override rows).
- The triple grid's **4,655** cells and the intersection's **1,862**
  cells were measured against the linked stdlib under both toolchains.
- The recovery-vs-U+FFFD invariant was measured on **76** cells under
  libstdc++ 15 (libc++ 21 never recovers, so there is nothing to
  compare).
- An ASan/UBSan build (`-fsanitize=address -fsanitize=undefined`) runs
  the same checks with no sanitizer report.
- Sections 1–11's output was diffed against Nov 14's own binary and is
  byte-identical.

## Where we go next

1. **The chain item (2) — the policy table's FOURTH kind of datum.**
   The table now has mask rows (Nov 7), a scalar ceiling (Nov 12), a
   fault mask (Nov 13), a boolean (Nov 14) and — today — a *second*
   boolean, `recovery_keeps_align`. That is two booleans of the same
   shape but different meaning. Worth asking whether the fill's two
   per-implementation facts want to be one datum: a single
   `FillBehaviour` struct holding the fatal mask and the recovery
   rule together, so the fill's whole enforcement is one value.
2. **The chain item (2) — is the "recovery" the right word?** Today's
   fix assumes recovery means *substitute U+FFFD for the maximal
   subpart and carry on*. That is what libstdc++ 15 measurably does,
   but the standard does not require it: an implementation could
   substitute U+FFFD for *each* ill-formed byte, or reject the spec
   after the substitution. If the substitution is per-byte, the
   recovered fill is several code points and the *width* changes. The
   arc's decoder knows the maximal subpart's length; it does not yet
   know how many U+FFFDs the implementation substitutes.
3. **The chain's item (3) — the width counts code points.** Still
   open, and now sharper: the width counts code points of the
   *argument*, and the fill is repeated to fill it. If the recovered
   fill is more than one code point (item 2), the fill's *width* is
   more than one column per repetition, and the arc's two-pass design
   has to agree with the stdlib on both units.
4. **The chain's item (4) — the compile-time path could use the arc's
   own validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. The arc's own
   `consteval` validator would accept what the standard accepts.
5. **The chain's item (5) — the policy table is the natural place to
   put the *value* rules.** A value-dependent rule (does 65 fit in
   `char`?) has to live somewhere else again — probably a third mask,
   keyed by (category, type), selected only after the value is known.
6. **The chain's item (6) — the `fill` is not the only place a Unicode
   scalar value can appear.** The width and precision are ASCII and
   the type char is ASCII, so the fill was the only place the scanner
   had to decode — but the *argument* is also a Unicode string, and the
   standard counts the width of a string argument in code points too.
   Worth measuring whether the two rules want the same table.
