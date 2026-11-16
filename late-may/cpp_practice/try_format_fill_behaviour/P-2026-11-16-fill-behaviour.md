# P-2026-11-16 — try_format_fill_behaviour

## What today closes

Nov 15's "Where we go next" item (1), verbatim:

> **The chain item (2) — the policy table's FOURTH kind of datum.**
> The table now has mask rows (Nov 7), a scalar ceiling (Nov 12), a
> fault mask (Nov 13), a boolean (Nov 14) and — today — a *second*
> boolean, `recovery_keeps_align`. That is two booleans of the same
> shape but different meaning. Worth asking whether the fill's two
> per-implementation facts want to be one datum: a single
> `FillBehaviour` struct holding the fatal mask and the recovery rule
> together, so the fill's whole enforcement is one value.

Closed, and then MEASURED — which is the arc's way. The question was
not "can these two fields live in one struct?" (of course they can);
it was "**how much behaviour does that one value actually carry?**"

The answer is a number: **13**.

## THE CHANGE — the fill's enforcement is one value

The fill's per-implementation facts were scattered across three fields
of `FillPolicy`: a rule switch (`unicode`), a fault mask (`fatal`),
and a recovery rule (`recovery_keeps_align`). They are now one value,
`FillBehaviour`, and the scanner asks it **one question**:

```cpp
// What the implementation does with an ILL-FORMED fill.
enum class FillOutcome : unsigned char {
    WellFormed,   // the bytes decode to a Unicode scalar value
    Fatal,        // ill-formed, and this implementation rejects it
    Recoverable,  // ill-formed, and this implementation recovers from it
};

struct FillBehaviour {
    bool unicode{true};               // the RULE
    std::uint8_t fatal{0};            // the ENFORCEMENT: a five-bit mask
    bool recovery_keeps_align{true};  // the ENFORCEMENT: the recovery rule

    [[nodiscard]] constexpr FillOutcome outcome(FillFault f) const noexcept {
        if (f == FillFault::None) return FillOutcome::WellFormed;
        return is_fatal(f) ? FillOutcome::Fatal : FillOutcome::Recoverable;
    }
};
```

And in `parse_spec()`, the scanner no longer knows how the rule and
the enforcement interact:

```cpp
const Utf8CodePoint cp = decode_utf8(spec.substr(i));

// NEW TODAY: ask the fill's ONE value ONE question.  The
// scanner does not know how the rule and the enforcement
// interact -- the value does.
const FillOutcome outcome = fill.outcome(cp.fault);

if (outcome == FillOutcome::WellFormed) {
    ...
} else if (outcome == FillOutcome::Fatal) {
    ...
} else {
    // the recovery path
}
```

## THE MEASUREMENT — the first grid whose axis is the POLICY

Every grid in the arc so far has varied the **spec**. Today's grid
varies the **policy**: the fill's enforcement space is

```
2 rules  x  32 fatal masks  x  2 recovery rules  =  128 candidates
```

Each candidate is scored by its **signature**: its verdict on every
cell of one fixed battery —

```
7 fills  x  2 (align or not)  x  2 ('0' or not)  x  5 arguments  =  140 cells
```

The fills are one well-formed ASCII fill, one well-formed 4-byte fill,
and one fill per **fault kind** (invalid lead, truncated, overlong,
surrogate, above-U+10FFFF). The arguments are a bool, a char, an int,
a double and a string — chosen because the `'0'` option is *valid* for
some (category, type) pairs and *not* for others, and that is what
makes the alignment option observable at all.

Everything except the fill's enforcement and the align/zero wording is
pinned to the **standard's** policy, so the measurement is the same
under both toolchains.

### The result

```
the space: 128 candidate values (2 rules x 32 fatal masks x 2 recovery rules)
the behaviours:
  under the C++23 align/zero wording: 5
  under the C++20 align/zero wording: 8
  over the whole space:              13
```

**128 candidates, 13 behaviours.** The datum is smaller than its
fields suggest, and the collapses are structured, not random.

## KEY FINDING #1 — three of the five fault bits are DEAD

Toggling one bit of the fatal mask at a time, over all 256
(fill behaviour, align/zero wording) points, and counting the
candidates whose behaviour changes:

```
truncated      128 of 256 candidates change (960 cells)
invalid lead   128 of 256 candidates change (960 cells)
overlong         0 of 256 candidates change (0 cells)
surrogate        0 of 256 candidates change (0 cells)
out of range     0 of 256 candidates change (0 cells)
```

**Three of the five bits can never be spent.** And for those three the
reason is structural, not an artefact of the battery.

The recovery path looks for the alignment option at
`spec[i + cp.length]` — immediately after the **maximal subpart**. For
the three dead faults, the maximal subpart is the **lead byte alone**:

- an overlong fill (`"\xC0\xAF"`) decodes to a 1-byte maximal subpart,
  but the *fill* is 2 bytes, so `spec[i+1]` is still a continuation
  byte of the ill-formed sequence;
- a surrogate fill (`"\xED\xA0\x80"`) and an above-U+10FFFF fill
  (`"\xF4\x90\x80\x80"`) are the same story — maximal subpart 1 byte,
  fill 3 and 4 bytes respectively.

So the byte where the scanner looks for the alignment option is always
part of the ill-formed sequence, never the alignment option. The
recovery path can never recover *into* a valid spec for those three
faults — and therefore "fatal" and "recoverable" are **the same
verdict** for them, whatever the mask says.

**The liveness is per byte sequence, not per fault kind.** The other
two faults are live in this battery because the battery contains fills
whose maximal subpart IS the whole fill: a truncated fill
(`"\xE2\x82"`, maximal subpart 2 bytes = the fill) and an invalid-lead
fill (`"\x80"`, maximal subpart 1 byte = the fill). For those, the
alignment option really does follow the maximal subpart, and the
recovery path can reach a valid spec.

But a truncated fill can also have a maximal subpart *shorter* than
the fill — `"\xE2\x28"`, where the second byte cannot continue the
sequence — and such a fill is dead in exactly the same way. So the
honest statement is narrower than "two fault kinds are live": **three
of the five bits are dead for every byte sequence, and the two live
ones are live because those particular fills happen to be exactly
their own maximal subpart.** In this battery the mask's effective rank
is two bits out of five — and the two live bits are exactly the two
faults where recovery is reachable at all.

## KEY FINDING #2 — the recovery rule is INERT under the C++23 wording

The chain's second boolean, `recovery_keeps_align`, toggled on its
own:

```
the recovery rule, toggled on its own:
  under the C++23 wording: 0 of 128 candidates change
  under the C++20 wording: 48 of 128 candidates change
```

**Zero.** Under the current wording of [format.string.std] the `'0'`
option is validity-checked *regardless* of whether an alignment option
is present, so whether the scanner recorded the alignment option
changes no verdict at all. The recovery rule is a **latent** fact: it
is only observable under the C++20 wording, where the `'0'` is ignored
when an align option is present.

This is the real answer to "do these two facts want to be one datum?"
They do — but not because they are *similar* (one is a mask, one is a
boolean). They want to be one datum because **neither is measurable
alone**:

- the fatal mask's three dead bits are dead because of the *grammar*
  (where the alignment option may sit);
- the recovery rule is invisible because of the *other* per-
  implementation fact (which wording of the align/zero rule this
  implementation enforces).

That is why the class table has 13 entries and not 128, and why it has
13 rather than 16: the two wordings' behaviour sets do not overlap
(the program asserts this), but within each wording the recovery rule
folds candidates together wherever it is inert.

## KEY FINDING #3 — the byte rule's enforcement is entirely INERT

The byte rule (`unicode = false`) reads the fill as **one byte**. It
never decodes, so it never finds a fault, so it never has a fault to
be fatal about and never has a recovery to keep an alignment option
through. All 64 (mask, recovery) variants therefore collapse into a
**single** class per wording:

```
class  0:  64 candidates
    C++23 wording, byte rule (the enforcement is inert)
class  1:  64 candidates
    C++20 wording, byte rule (the enforcement is inert)
```

64 candidates, one behaviour. The byte rule does not enforce *anything*
about ill-formed fills — it simply never looks at them. (This is also
why Nov 13's census found the byte rule disagreeing with the stdlib 6
times: the byte rule cannot see a 4-byte fill at all.)

## The class table

The 13 classes, with the tag naming what their members have in common
— and, where the members *disagree* about the recovery rule while
behaving identically, printing that disagreement:

```
class  0:  64 candidates   C++23 wording, byte rule (the enforcement is inert)
class  1:  64 candidates   C++20 wording, byte rule (the enforcement is inert)
class  2:  16 candidates   C++23, scalar, truncated=recoverable, invalid-lead=recoverable
class  3:   8 candidates   C++20, scalar, truncated=recoverable, invalid-lead=recoverable, recovery=loses the align
class  4:   8 candidates   C++20, scalar, truncated=recoverable, invalid-lead=recoverable, recovery=keeps the align
class  5:  16 candidates   C++23, scalar, truncated=fatal, invalid-lead=recoverable
class  6:   8 candidates   C++20, scalar, truncated=fatal, invalid-lead=recoverable, recovery=loses the align
class  7:   8 candidates   C++20, scalar, truncated=fatal, invalid-lead=recoverable, recovery=keeps the align
class  8:  16 candidates   C++23, scalar, truncated=recoverable, invalid-lead=fatal
class  9:   8 candidates   C++20, scalar, truncated=recoverable, invalid-lead=fatal, recovery=loses the align
class 10:   8 candidates   C++20, scalar, truncated=recoverable, invalid-lead=fatal, recovery=keeps the align
class 11:  16 candidates   C++23, scalar, truncated=fatal, invalid-lead=fatal
class 12:  16 candidates   C++20, scalar, truncated=fatal, invalid-lead=fatal
```

Read the C++23 half: four scalar classes, each holding **both**
recovery-rule variants (16 candidates each, since 8 masks × 2 recovery
values) — the recovery rule is inert, so the two variants are the same
behaviour. Read the C++20 half: each of the three classes with a
recoverable fault splits in two (8 and 8), and only the all-fatal class
(12) stays whole. The table *shows* the coupling.

## What changed in the code

- `FillPolicy` → `FillBehaviour`, with `outcome()` — the one question.
- `parse_spec()` asks `fill.outcome(cp.fault)` instead of testing
  `cp.fault == FillFault::None` and `fill.is_fatal(...)` separately.
- A new `FillOutcome` enum: `WellFormed` / `Fatal` / `Recoverable`.
- Section 14: the policy grid (128 candidates × 140 cells), the class
  table, the dead-bit census and the coupling census.

## What did NOT change

Re-measured, and the output is **byte-identical** to Nov 15's for
Sections 1–13 under libc++ 21:

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order and their meanings, exactly as since Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the TooLarge
  contract, the buffer-atomicity behaviour.
- The type-category table from Oct 30, Nov 7's mask tables, its derived
  override table, and its 1,596-cell grid.
- Nov 13's fill rule and its fill census; Nov 14's pair grid and the
  align/zero rule; Nov 15's triple grid and the recovery path.

The diff was run against Nov 15's own binary, compiled from
`origin/main`, and Sections 1–13 match exactly.

## Build and run

The same source is built and measured under **both** toolchains, with
no source change between them:

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-16-fill-behaviour
```

## Verification

- Compiled with `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` — clean, no warnings, under both
  clang++ 21 (libc++ 21) and g++ 15 (libstdc++ 15).
- The program's own checks: **134 pass, 0 fail** under libc++ 21, and
  **131 pass, 0 fail** under libstdc++ 15 — the same source, the same
  assertions, three checks fewer because a few of the arc's checks are
  libc++-specific (the fill census, the derived override rows).
- The 128-candidate space was scored over a 140-cell battery; the
  13-class partition was measured, not assumed.
- The dead-bit census toggles each of the five fault bits over all 256
  points and counts the candidates whose behaviour changes.
- The linked stdlib was probed on all 140 battery cells under the
  linked implementation's own policy: the model agrees on **140 of
  140** under libc++ 21 and **140 of 140** under libstdc++ 15.
- Sections 1–13's output was diffed against Nov 15's own binary and is
  byte-identical.
- An ASan/UBSan build (`-fsanitize=address -fsanitize=undefined`) runs
  the same checks with no sanitizer report.

## Where we go next

1. **The chain item (2) — is the "recovery" the right word?** Today's
   fix assumes recovery means *substitute U+FFFD for the maximal
   subpart and carry on*. That is what libstdc++ 15 measurably does,
   but the standard does not require it: an implementation could
   substitute U+FFFD for *each* ill-formed byte, or reject the spec
   after the substitution. If the substitution is per-byte, the
   recovered fill is several code points and the *width* changes. The
   arc's decoder knows the maximal subpart's length; it does not yet
   know how many U+FFFDs the implementation substitutes. **Today's
   dead-bit finding sharpens this**: for three of the five faults the
   recovered fill can never be followed by an alignment option, so the
   number of U+FFFDs never gets to matter for them.
2. **The chain item (3) — the width counts code points.** Still open,
   and now sharper: the width counts code points of the *argument*,
   and the fill is repeated to fill it. If the recovered fill is more
   than one code point (item 2), the fill's *width* is more than one
   column per repetition, and the arc's two-pass design has to agree
   with the stdlib on both units.
3. **The chain item (4) — the compile-time path could use the arc's
   own validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. The arc's own
   `consteval` validator would accept what the standard accepts.
4. **The chain item (5) — the policy table is the natural place to
   put the *value* rules.** A value-dependent rule (does 65 fit in
   `char`?) has to live somewhere else again — probably a third mask,
   keyed by (category, type), selected only after the value is known.
5. **The chain item (6) — the `fill` is not the only place a Unicode
   scalar value can appear.** The width and precision are ASCII and
   the type char is ASCII, so the fill was the only place the scanner
   had to decode — but the *argument* is also a Unicode string, and the
   standard counts the width of a string argument in code points too.
   Worth measuring whether the two rules want the same table.
6. **NEW — the dead bits are a property of the GRAMMAR, so they
   belong to the standard, not to the implementation.** Today measured
   that overlong, surrogate and above-U+10FFFF can never be recovered
   into a valid spec *because the alignment option cannot sit where the
   scanner looks for it*. That argument is about [format.string.std]'s
   fill-and-align grammar, so it is the same for every implementation.
   Worth asking whether the fault mask should be reduced to its two
   live bits — and whether the *implementation's* mask is then not a
   mask at all but a single bit.
7. **NEW — the fill's value is not a closed value.** Today's
   measurement shows the fill's enforcement cannot be evaluated without
   the align/zero rule, because one of the fill's own fields is inert
   or live depending on it. If the two really are one fact, maybe the
   *whole* enforcement should be one value: a `FillBehaviour` that
   carries the align/zero wording inside it, so the scanner is handed
   something it can evaluate without consulting a second datum.
