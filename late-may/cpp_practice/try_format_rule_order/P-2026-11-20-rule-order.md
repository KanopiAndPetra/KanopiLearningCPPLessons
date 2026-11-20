# P-2026-11-20 — try_format_rule_order

## What today closes

Nov 19's "Where we go next" item (10), verbatim:

> **NEW — the GB9a/GB9c interaction was found by ACCIDENT, and that is
> worth repeating deliberately.** The cell `ka` + U+0903 + virama + `ssa`
> is the only one in today's battery where two rules meet and *disagree*
> — GB9a says the spacing mark joins the preceding cluster, GB9c says
> the conjunct should form, and the stdlib resolves it by breaking the
> conjunct's left context. Worth searching for the other places where
> two cluster rules can meet and pull in opposite directions: GB9a vs
> GB9c is one; GB9 vs GB9c and GB11 vs GB9c are others.

Closed, and the search found the other two. Both are defects in the
arc's own **ONE** column rule, and both are the same lesson stated two
ways:

**UAX #29's rules are an ORDERED list, and a rule CONSUMES its
context.**

### 1. THE ORDER — GB9c comes BEFORE GB11

UAX #29 lists the rules in order, and the first rule that matches
decides the boundary. `GB9c` (the Indic conjunct) is listed **before**
`GB11` (the emoji ZWJ sequence). When a conjunct's left context is
live and a ZWJ follows, GB9c decides — not GB11.

The rule libc++ implements is UAX #29's GB9c in the form it had
**before** Unicode 18.0:

```
GB9c:  \p{InCB=Consonant} [ \p{InCB=Extend} \p{InCB=Linker} ]*
       \p{InCB=Linker} [ \p{InCB=Extend} \p{InCB=Linker} ]*
       ×  \p{InCB=Consonant}
```

Two things in that left context matter. One is that **a ZWJ is an
`InCB=Extend`** — so the conjunct *survives* a ZWJ. That is not a
reading of the standard: it is measured straight out of **libc++ 21's
own table**, by asking the table the question directly:

```
U+094D Devanagari virama         U+094D -> InCB=Linker
U+0938 DEVANAGARI LETTER SSA     U+0938 -> InCB=Consonant
U+0915 DEVANAGARI LETTER KA      U+0915 -> InCB=Consonant
U+200D ZWJ                       U+200D -> InCB=Extend
U+0301 COMBINING ACUTE           U+0301 -> InCB=Extend
U+0903 DEVANAGARI SIGN VISARGA   U+0903 -> InCB=none
```

So:

```
"ka" virama ZWJ "ssa"   →   ONE cluster
```

Nov 19's rule asked **GB11 first**. GB11's condition
(`ExtPict Extend* ZWJ × ExtPict`) failed — there is no pictographic on
either side — so the rule ended the cluster there, cutting a conjunct
in half. The arc charged **2** columns; the stdlib charges **1**.

### 2. THE CONSUMPTION — the conjunct spends its Linker

GB9c is not a "the cluster continues forever" rule. Once the conjunct
has formed, the `Linker` that made it is **used up**: the state returns
to `Consonant`, not to `Linker`. So the *next* consonant needs a linker
of its own:

```
"ka" virama "ssa" "ssa"   →   TWO clusters
```

Nov 19's rule kept `cb_linker` set forever, so the second `ssa` joined
a cluster that had already been closed. The arc charged **1** column;
the stdlib charges **2**.

## THE COLLISION BATTERY — four models, one oracle

One battery of **24 cells**, each one a place where **two** rules can
meet. Four models, one oracle: the width the **LINKED stdlib** charges,
measured by padding the argument to a known width and counting the fill
characters written.

```
  the collision battery -- four models, one oracle (libc++ 21):
    argument                 stdlib  A(old) B(cons)  C(ord)  D(ONE)
    ka virama ssa                 1       1       1       1       1
    ka virama ssa ssa             2       1       2       1       2
    ka virama ZWJ ssa             1       2       2       1       1
    ka virama ZWJ ZWJ ssa         1       2       2       1       1
    ka ZWJ ssa                    2       2       2       2       2
    ka ZWJ virama ssa             1       2       2       1       1
    ka virama ssa ZWJ ssa         2       2       2       1       2
    ka virama ZWJ ssa EP          3       4       4       3       3
    ka virama U+0301 ssa          1       1       1       1       1
    ka U+0903 virama ssa          2       2       2       2       2
    ka virama U+0903 ssa          2       2       2       2       2
    ka virama ZWJ U+0903 ssa      2       2       2       2       2
    ssa virama ssa                1       1       1       1       1
    ka virama ssa virama ssa      1       1       1       1       1
    ka virama ssa U+0903          1       1       1       1       1
    prepend ka virama ssa         2       2       2       2       2
    ka virama RI ssa              3       3       3       3       3
    ka virama RI RI               2       2       2       2       2
    ka virama space ssa           3       3       3       3       3
    ka virama EP ssa              4       4       4       4       4
    EP ZWJ ka virama ssa          3       3       3       3       3
    ka virama ZWJ EP              3       3       3       3       3
    virama ssa                    2       2       2       2       2
    EP ZWJ EP                     2       2       2       2       2

    the models, over 24 cells:
      A  Nov 19's rule        agree on 19 of 24 cells
      B  A + the Linker spent agree on 20 of 24 cells
      C  the ORDER fixed      agree on 22 of 24 cells
      D  today's ONE rule     agree on 24 of 24 cells
```

The four models are:

- **A** — Nov 19's rule exactly: GB11 asked before GB9c, and the
  conjunct's linker kept. **19 of 24.**
- **B** — A plus the consumption: the linker is spent. Fixes the
  second-consonant cells, leaves the ZWJ cells wrong. **20 of 24.**
- **C** — the order fixed: GB9c asked before GB11, the linker still
  kept. Fixes the ZWJ cells, leaves the second-consonant cells wrong.
  **22 of 24.**
- **D** — today's ONE rule: both. **24 of 24.**

Neither fix subsumes the other. C is worth more cells (it repairs four)
than B (one) — but B is the fix for a *different* cell, and a model
with only one of the two still loses cells. **Both are needed**, and
the program asserts it: `d_agree == cells.size()`, and each of B and C
is strictly better than A and strictly worse than D.

### What each change is worth on its own

Turning each of today's two changes **off** from the full rule and
counting the cells the model loses:

```
  what each of today's two changes is worth:
    the ORDER  (GB9c before GB11)              without it: 20 of 24 cells
    the CONSUMPTION (the Linker is spent)      without it: 22 of 24 cells
```

The order is worth **4** cells and the consumption **2**. Neither is
worthless, so neither can be dropped.

## THE CODE

### The two mask bits

The arc's `gb::` mask already exists so that a *deleted* rule can still
be **measured**. Today it grows two bits, for the same reason — each
defect has to be measurable, not merely describable:

```cpp
inline constexpr unsigned gb9c_first = 64u;
inline constexpr unsigned gb9c_consume = 128u;
inline constexpr unsigned all = gb9a | gb9b | gb9c | gb11 | gb12 | gb68 |
                                gb9c_first | gb9c_consume;
```

- `gb9c_first` — GB9c is consulted **before** GB11. With the bit off,
  the conjunct is consulted after — Nov 19's order, which is wrong.
- `gb9c_consume` — the conjunct **consumes** its context: the linker
  is spent, so a following consonant needs a linker of its own. With the
  bit off the linker is kept — also wrong.

### The early conjunct check

The conjunct check moves **up**, to where UAX #29's order puts it —
before the GB11 branch that used to break the cluster:

```cpp
// GB9c: the conjunct -- and it comes BEFORE GB11, because
// UAX #29's rules are an ORDERED list and GB9c is listed
// before GB11.  GB9c's left context is "Consonant [Extend
// Linker]* Linker [Extend Linker]*", and a ZWJ is an
// Extend, so a conjunct SURVIVES a ZWJ: "ka" virama ZWJ
// "ssa" is ONE cluster.  Nov 19's rule asked GB11 first,
// and cut the conjunct in two.
if ((rules & gb::gb9c) != 0u &&
    (rules & gb::gb9c_first) != 0u && cb_consonant &&
    cb_linker && is_indic_consonant(cp.value)) {
    // The conjunct forms, and it SPENDS its context: the
    // Linker is used up, so a following Consonant needs a
    // Linker of its own.
    if ((rules & gb::gb9c_consume) != 0u) cb_linker = false;
    last_was_zwj = false;   // the ZWJ's pending check is spent
    ep_run = false;
    i += cp.length;
    ri_run = 0;
    hangul_prev = HangulClass::None;
    continue;
}
```

Three things happen that did not happen before:

1. The conjunct is asked **before** GB11, so the ZWJ does not cut it.
2. `cb_linker = false` — the linker is spent.
3. `last_was_zwj = false` — the ZWJ's pending GB11 check is *spent* by
   the conjunct, so a later code point is not judged against a ZWJ that
   has already been answered.

The **late** conjunct check survives, gated on `gb9c_first == 0`, so
Nov 19's order is still measurable:

```cpp
// GB9c: the consonant after a linker -- the LATE check, for
// the OLD order only (Nov 19's rule): GB11 was asked first,
// so a conjunct after a ZWJ never got its chance to form.
if ((rules & gb::gb9c) != 0u &&
    (rules & gb::gb9c_first) == 0u && cb_consonant &&
    cb_linker && is_indic_consonant(cp.value)) {
    if ((rules & gb::gb9c_consume) != 0u) cb_linker = false;
    i += cp.length;
    ...
```

### Where the fix came from

The fix is not guessed; it is read out of the **linked stdlib's own
implementation**, which is the oracle the arc measures against.
`libc++ 21`'s `__format/unicode.h` carries the grapheme-cluster state
machine, and it answers both questions directly:

- **The order.** `__evaluate_GB9c_indic_conjunct_break` is a *separate*
  active rule from `__evaluate_GB11_emoji`, and the constructor arms
  GB9c as soon as the first code point is an `InCB=Consonant`. GB9c
  consumes its `Linker`: on `state == Linker` seeing a `Consonant`, it
  sets `__GB9c_indic_conjunct_break_state_ = __Consonant` — the linker
  is gone. And `__inCB_property::__Extend` is true for ZWJ, so the ZWJ
  does not disturb the context.

- **The consumption.** Same line, same meaning: after the conjunct
  fires, the state is `Consonant`, so a second consonant falls through
  to `__evaluate_none`, which breaks.

The arc's model had the same *rules* but the wrong *sequencing*, and it
had no notion of a rule spending its context at all.

### Why every cell starts with a non-ASCII code point

This is a measurement subtlety worth naming, because it would have
silently invalidated the whole battery. libc++'s width estimation has an
**ASCII fast path**: if the string begins with ASCII code units, it
charges one column per code unit and **never runs the cluster
algorithm at all**. So a pure-ASCII cell like `"\r\n"` measures the
fast path, not the rules:

```
CR LF                    bytes= 2 charged=2     ← the fast path: 2 code units
ka CRLF ka               charged=3               ← the cluster algorithm: 3
```

Every cell in the collision battery therefore starts with a non-ASCII
code point (a Devanagari `ka`, or `U+0600`), so the oracle is always
the grapheme-cluster algorithm and never the shortcut. The control
cells in 18.3 do the same.

## THE CELLS THE ONE RULE STILL LOSES

The control characters are UAX #29's **GB3/GB4/GB5**, which the arc
does not model at all:

- **GB3** — `CR × LF`: a CR and an LF are never separated by a
  boundary.
- **GB4** — `(Control | CR | LF) ÷`: a break **after** a control.
- **GB5** — `÷ (Control | CR | LF)`: a break **before** a control.

Those three rules are *also* an ordering question — GB4/GB5 sit
**before** GB9 in UAX #29's list, so an `Extend` after a control does
**not** join it. The arc's model has no notion of a control, so it
gets these cells wrong in both directions:

```
  the cells the ONE rule still loses (GB3/GB4/GB5):
    ka CRLF ka                    3       4  <- DIFFERS
    ka CRLF                       2       3  <- DIFFERS
    ka CRLF CRLF ka               4       6  <- DIFFERS
    ka NUL U+0301 ka              4       3  <- DIFFERS

    4 of 4 control cells still differ
```

The first three cells the arc **over**counts: it splits `CR LF` into
two clusters where GB3 keeps it as one. The last cell it **under**
counts: it lets the `U+0301` join the `NUL`'s cluster, where GB4 says
a control breaks on both sides and the Extend starts a fresh cluster.

These four cells are printed, not hidden, and the program asserts the
gap is still open (`control_lost > 0`) so that the next lesson cannot
silently forget them.

## THE ORACLE IS NOT THE CURRENT STANDARD

While checking the rule order against UAX #29 itself, the notes found
that **the standard revised GB9c**, and that **the linked stdlib has
not caught up**. UAX #29 revision 49 (Unicode 18.0.0, 2026-09-01)
records the change in its own Modifications section:

```
Rule GB9c:
    Change Rule GB9c in UAX #29 to
    \p{InCB=Linker} \p{InCB=Extend}* × \p{InCB=Consonant}. [187-C47]
```

The rule the arc implements — and the rule **libc++ 21 implements** — is
the *previous* one:

```
GB9c:  \p{InCB=Consonant} [ \p{InCB=Extend} \p{InCB=Linker} ]*
       \p{InCB=Linker} [ \p{InCB=Extend} \p{InCB=Linker} ]*
       ×  \p{InCB=Consonant}
```

The difference is the **leading Consonant**. The old rule's left
context must *begin* with an `InCB=Consonant`; the new rule's begins
with an `InCB=Linker` and needs no Consonant at all. So the two rules
disagree on exactly one kind of cell: **a Linker that is not preceded
by a Consonant.**

The battery already contains one:

```
    virama ssa                    2       2       2       2       2
```

`"ka" virama "ssa"` is ONE cluster under both rules — the `ka` is the
leading Consonant. But `"virama" "ssa"` has **no** leading Consonant:
under the old rule it is TWO clusters, and the oracle charges **2
columns** for it, and the arc's ONE rule charges **2 columns** for it
too. Under the revised Unicode 18.0 rule the left context would be
just the Linker, so the two code points would be **ONE** cluster, and
the width charged would be **1**.

That last step is a **derivation from the rule text**, not a
measurement — the linked stdlib still implements the old rule, so there
is nothing yet to measure it against. The arc deliberately does not
implement the new rule: the arc's contract is to agree with the
**linked stdlib**, which is its oracle, and the oracle has not moved.

libc++ 21's implementation makes the version it implements explicit.
Its constructor arms GB9c **only when the first code point is an
`InCB=Consonant`**:

```cpp
else if (__indic_conjunct_break::__get_property(__first_code_point) ==
         __inCB_property::__Consonant)
  __active_rule_ = __rule::__GB9c_indic_conjunct_break;
```

so a string that begins with a Linker never enters the conjunct rule
at all. That is the pre-18.0 rule, exactly.

This is not a defect in the arc — the arc's job is to agree with the
**linked stdlib**, which is its oracle — but it is a defect in the
*oracle*, and it is worth knowing which version of the standard the
oracle implements. It is also the second time this arc has had to
answer "which Unicode version does the oracle follow?", after the
width table's `EastAsianWidth.txt` provenance.

## WHAT DID NOT CHANGE

Everything else. Re-measured in the same run, with the same oracle:

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order and their meanings, exactly as since Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the `TooLarge`
  contract, the buffer-atomicity behaviour.
- The type-category table from Oct 30; Nov 7's mask tables and its
  derived override table and its 1,862-cell grid; Nov 13's fill rule
  and its fill census; Nov 14's pair grid and the align/zero rule;
  Nov 15's triple grid and the recovery path; Nov 16's fill behaviour
  space.
- **Nov 17's width census — 1, 11, 28.** Bytes agree on 1 of 28 cells,
  code points on 11 of 28, **COLUMNS on 28 of 28**.
- **Nov 18's precision battery — 56 of 56.** BYTES 25 of 56, CODE
  POINTS 42 of 56, COLUMNS 56 of 56.
- **Nov 19's boundary battery — 20 of 20.** A (the deleted rule) 11 of
  20, B (Nov 18's GB11 model) 13 of 20, C (the ONE rule) 20 of 20.
- Nov 19's per-rule worth census: GB9a 18 of 20, GB9b 19 of 20,
  GB9c 19 of 20, GB12 18 of 20, GB68 19 of 20.

The three numbers that had to move did not move, and the four new
numbers are new.

## HOW IT WAS BUILT AND CHECKED

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-20-rule-order

cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-11-20-rule-order
```

- **Apple Clang 21**, `-std=c++23`, `-fexperimental-library` (libc++
  21 needs it for `std::println` and `std::expected`).
- Strict warnings **on**: `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion`. Clean.
- **ASan + UBSan** build: clean, `pass=189 fail=0`.
- The oracle is the **linked stdlib** throughout — every number in this
  document is *measured* by padding to a known width and counting the
  fill characters written, never asserted.

```
=== summary ===
  pass=189  fail=0
```

## Where we go next

1. **The control characters are the next collision battery.** GB3,
   GB4 and GB5 are unmodelled, and the four cells in 18.3 show the arc
   wrong in both directions. The interesting part is the same lesson
   again: GB4/GB5 sit *before* GB9 in UAX #29's list, so the `Extend`
   after a `NUL` must **not** join it. Worth building the battery that
   asks about them: one row per rule, and one row per *ordering* of
   GB3/GB4/GB5 against GB9.
2. **The arc's `is_indic_consonant` and `is_indic_linker` tables are
   still RESTRICTED** — 30 and 13 ranges, against libc++'s full
   `indic_conjunct_break_table.h`. The conjunct rule is only as good
   as its tables, and no battery has been chosen to *break* them yet.
3. **The cluster tables are still restricted** — `kWidth2Ranges` 54
   ranges against libc++'s 104-entry `width_estimation_table.h`,
   `kExtendedPictographic` 32 ranges against the full emoji data. Every
   battery so far has agreed with the stdlib; none has been designed to
   disagree.
4. **The width's RESTRICTED table is still unmeasured on the
   precision's cells.** Nov 17 asked how large the unmeasured part of
   the space is; Nov 18 added 14 more arguments and Nov 19 more, and
   the restricted tables still agree with the stdlib on every cell of
   all three batteries. Still open: the FULL table's size, and whether
   the restricted one can ever agree on a battery chosen to break it.
5. **The arc's cap is in BYTES and both the width and the precision are
   in COLUMNS.** A precision of 2 can yield 0 bytes or 6 bytes or 11
   bytes depending on the argument, so the arc's two-pass design has to
   agree with the stdlib on a conversion between the two units that is
   not a constant factor. Worth measuring `try_format_runtime_n`
   against the stdlib on a battery of wide arguments with a precision.
   Today's rules make this *worse*: a regional-indicator pair is 8
   bytes and ONE column, and a Hangul jamo sequence is 9 bytes and TWO
   columns, so a precision of 1 keeps zero bytes in both cases.
6. **The compile-time path could use the arc's own validator.**
   `std::format_string` rejects `"{:+c}"` on this toolchain even
   though the standard allows it. Still open.
7. **The policy table is the natural place to put the *value* rules.**
   A value-dependent rule has to live somewhere else again. Still open.
8. **The dead bits are a property of the GRAMMAR.** Nov 16 measured
   that overlong, surrogate and above-U+10FFFF can never be recovered
   into a valid spec. Worth asking whether the fault mask should be
   reduced to its two live bits.
9. **The escaped type's precision is a separate rule, and it is NOT the
   same rule.** The `?` type's precision counts the columns of the
   *escaped* output, and the cut can land inside an escape sequence.
   The arc's scanner does not model the escaped output at all. Worth
   asking whether the escaped type needs its own column model, or
   whether the escape sequences' own widths make the rule unreachable
   in practice.
10. **NEW — a rule that is consulted but never SPENT is a bug waiting
    to happen.** Today found two instances of one mistake — the
    conjunct kept its linker after firing. The arc's other rules are
    all "this code point joins the cluster", which never has to be
    un-done. But the conjunct is the one rule whose *left context* is
    consumed, and it is the only rule in the arc that spans more than
    one code point. Worth asking which of the arc's other state
    variables — `ep_run`, `prepend_pending`, `ri_run`, `cb_consonant` —
    are likewise spent by the rule that set them, and whether any of
    them is currently kept alive when it should not be.
11. **NEW — the ORDER of the arc's own rule list is now measurable, and
    it is not the standard's order.** The arc's cluster splitter
    applies GB9's Extend before GB9a's SpacingMark, and GB9c's
    conjunct before GB11 — but the ZWJ branch sits *above* the
    conjunct check in the old order and *below* it in today's. Worth
    writing the arc's rule list out in UAX #29's order, side by side,
    and reading off every place the two differ — today's fix moved one
    rule, but a side-by-side list would show the rest.
12. **NEW — the ORACLE is not the current standard, and today's lesson
    found where.** UAX #29 rev 49 (Unicode 18.0.0, ballot 187-C47)
    **revised GB9c** to `\p{InCB=Linker} \p{InCB=Extend}* ×
    \p{InCB=Consonant}` — the leading Consonant is no longer required.
    libc++ 21 still implements the OLD rule, and the arc follows the
    oracle, so the two agree today. But the day libc++ updates, the
    cell `"virama" "ssa"` flips from 2 columns to 1, and today's
    battery row for it becomes a **regression detector**. Worth:
    (a) pinning the oracle's Unicode version somewhere visible — the
    arc's `EastAsianWidth.txt` provenance note is the precedent; and
    (b) writing the one cell as a named *known divergence* rather than
    a plain agreement, so the day the stdlib moves the lesson says so
    instead of quietly failing.
