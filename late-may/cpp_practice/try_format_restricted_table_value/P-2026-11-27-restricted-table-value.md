# P-2026-11-27 — try_format_restricted_table_value

**What is a restricted table FOR?**

Closes Nov 25's "Where we go next" item (26).

> **A restricted table's dead weight is now a number (98.8%), so the next
> question is whether ANY restricted table earns its keep.** Worth: asking
> what happens if they are DELETED — does any battery lose its ability to
> *demonstrate* the error, and what should a lesson keep around purely to
> teach?

**CLOSED — and the answer is NO for one of the six.**

---

## 1. THE QUESTION, MADE SHARP

Nov 25 measured the arc's restricted Extend table's dead weight: 566 of its
573 code points (98.8%) are carried for no battery at all. Dead weight is a
number, but it is the wrong number. It measures how much of a table is
*unused*, not how much of a table is *worth keeping*.

The sharp version of the question is mechanical: **delete the table and
watch what happens.**

A restricted table can be deleted, and what a deleted table can answer is
exactly one thing: *"no code point has this property."* An unknown property
is never true. So the deletion has a ledger, and the ledger is the same for
all six tables:

* the **over-claims** (the arc says yes, libc++ says no) are **FIXED** —
  "no" is what libc++ says;
* the **correct claims** (both say yes) are **BROKEN** — "no" is not;
* the **under-claims** (libc++ says yes, the arc says no) **do not change
  at all** — "no" is what the arc already said.

So a table earns its keep exactly when it is right more often than it is
wrong, and the net worth of a table is

```
net = agree - over
```

---

## 2. THE LEDGER — six tables, the whole code point space

Measured over 0x0000..0x10FFFF, one pass per table, through the arc's own
rule:

```
table                     agree     over    under        net
kWidth2Restricted        174599      150     8343    +174449
kExtendedPictographic      2159      528     1378      +1631
kPrepend                     27        0        1        +27
kIndicConsonant             237       38        3       +199
kIndicLinker                  6        9        0         -3
is_cluster_extend           524       49     1674       +475
```

These are the Nov 22 / Nov 23 / Nov 24 census numbers, re-measured and
**not moved**: 174,599 + 150 = 174,749 width-2 code points in the arc's
table against libc++ 21's 182,942; `kExtendedPictographic` 2,687 vs 3,537;
`kPrepend` 27 vs 28; `kIndicConsonant` 275 vs 240; `kIndicLinker` 15 vs 6;
`is_cluster_extend` 573 vs 2,198.

### The one that does not earn its keep

```
kIndicLinker                  6        9        0         -3
```

`kIndicLinker` is RIGHT about **six** code points and WRONG about **nine**.
Deleting it would make the arc's rule **right nine times and wrong six
times** — a net *gain*. And Nov 23 already showed why: all nine of its
over-claims (`U+0CCD`, `U+0D3B`, `U+1B44`, `U+A9C0`, `U+11A34`, `U+11A35`,
`U+11D44`, `U+11D45`, `U+11D97`) are code points libc++ calls `InCB=Extend`,
not `InCB=Linker`.

The other five are net positive, and the width table is the extreme case:
+174,449, because the arc's restricted width table is wrong about only 150
of its 174,749 entries.

---

## 3. THE DELETION BATTERY — measured, not arithmetic

The ledger is arithmetic over a whole code point space. The battery is a
measurement: the cells are every code point where the two tables
DISAGREE, plus a sample of the code points where they AGREE, and each cell
goes through the arc's own rule twice — once with the table KEPT, once
with the table DELETED — against libc++ 21's answer as the oracle.

```
table                     cells    fixed   broken     same
kWidth2Restricted          8557      150       64     8343
kExtendedPictographic      1970      528       64     1378
kPrepend                     28        0       27        1
kIndicConsonant             105       38       64        3
kIndicLinker                 15        9        6        0
is_cluster_extend          1787       49       64     1674
over the six tables: 774 cells FIXED by the deletion, 289 BROKEN and 11399
unchanged -- the deletion is a net LOSS for five of the six.
```

Three things fall out of the measurement, and none of them is an argument:

* **every over-claim cell is fixed** — 774 of them, exactly the `over`
  column summed;
* **every under-claim cell is untouched** — 11,399 of them, exactly the
  `under` column summed: the deletion cannot fix what the table never
  claimed;
* **every agree cell is broken** — 289 of them (the agree cells are
  sampled at 64 per table, so the six agree populations are 174,599,
  2,159, 27, 237, 6 and 524 and only 289 of them are exercised).

The deletion is therefore not a repair. It is a **trade**: it converts
wrong answers into right ones at the rate of the over-claims, and converts
right answers into wrong ones at the rate of the agrees. For five tables the
second rate is larger.

---

## 4. THE PRODUCT TEST — the arc's own tables are EXHIBITS

There is a second deletion, and it is the more important one.

Since Nov 22 (the width) and Nov 23 (the five properties) the arc's ONE rule
consults **libc++ 21's full tables**, behind the `gb::width_full` and
`gb::props_full` bits. So the six restricted tables are not the product's
inputs at all — they are kept only so a battery can hold both sides of a
comparison. Deleting them therefore moves the product by **exactly zero
answers**:

```
table                    arc's table gone      libc++'s gone
kWidth2Restricted                       0               8493
kExtendedPictographic                   0               1906
kPrepend                                0                  1
kIndicConsonant                         0                 41
kIndicLinker                            0                  9
is_cluster_extend                       0               1723
```

Deleting **libc++'s** table instead moves the product on exactly the
disagreement code points — 8,493 + 1,906 + 1 + 41 + 9 + 1,723 — because the
rule falls back to the arc's own table and the arc's own table is wrong
there. That is the sharpest statement available of what libc++'s tables are
worth to the arc.

### The same thing in columns

```
payload                        stdlib    product arc's gone libc++ gone
the England flag (6 tags)           2          2          2          8
U+2630, a wide trigram              2          2          2          1
"a" + U+E0020, a tag                1          1          1          2
```

Reading the England flag row: libc++ charges it **two** columns, the
product charges two, deleting the arc's own table changes nothing (two),
and deleting libc++'s Extend table makes the arc fall back to its own
573-code-point table — which does not know the Tags block — and charges
**eight**. That is Nov 25's error, resurrected by deleting the oracle's
data instead of the arc's.

---

## 5. WHAT DID NOT CHANGE

`petra::FormatError` / `petra::FormatErrorKind` (the four kinds, their
order and their meanings, exactly as since Sep 6), the two-pass (measure,
then write) design, the counting_output_iterator, the bounded_char_writer,
the TooLarge contract, the buffer-atomicity behaviour, the type-category
table from Oct 30, Nov 7's mask tables and its derived override table and
its 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's pair
grid and the align/zero rule, Nov 15's triple grid and the recovery path,
Nov 16's fill behaviour space, Nov 17's width census — whose three numbers
(1, 11, 28) are re-measured here and must not move — Nov 18's precision
battery (56 of 56), Nov 19's boundary battery (20 of 20), Nov 20's
collision battery (24 of 24), Nov 21's control battery (19 of 19), Nov 22's
width-table battery (26 of 26), Nov 23's property-table battery (37 of 37)
and Nov 24's Extend-table battery (364 of 364), all of which are re-measured
here and must not move.

The only change to the carried-forward arc is the **deletion hooks** this
lesson needs: `gb::vac_*` (the arc's own table deleted → the property
becomes unknown) and `gb::gone_*` (libc++'s table deleted → the rule falls
back to the arc's own), plus `gb::full_*` (one property table switched to
libc++'s full table on its own, so a single table can be measured in
isolation). `gb::all` does not carry any of them, so every earlier section
behaves exactly as it did.

---

## 6. OBSERVED OUTPUT

```
$ cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
$ cmake --build build
$ ./build/P-2026-11-27-restricted-table-value
```

* Compiles clean under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` with `-std=c++23`
  (`-fexperimental-library` for libc++ 21's `std::expected`).
* Runs clean under **ASan + UBSan**: no sanitizer report, no runtime error.
* **`pass=385  fail=0`**, exit status 0. The 319 assertions carried forward
  from Nov 25 still pass, plus 66 new ones.
* Built against **libc++ 21** (`_LIBCPP_VERSION`), so the oracle is the
  linked stdlib, not an assertion about it.

The counts that must not move — Nov 17's width census (1, 11, 28), Nov 18's
precision battery (56 of 56), Nov 19's boundary battery (20 of 20), Nov 20's
collision battery (24 of 24), Nov 21's control battery (19 of 19), Nov 22's
width-table battery (26 of 26), Nov 23's property-table battery (37 of 37)
and Nov 24's Extend-table battery (364 of 364) — are all re-measured in
this binary and all still hold.

---

## 7. WHERE WE GO NEXT

26 is closed. The arc's six restricted tables are now **exhibits**: five of
them earn their keep (they are right more often than they are wrong, and
the deletion costs more than it saves) and one of them, `kIndicLinker`, does
not (right about 6, wrong about 9). And the product never reads any of
them, because the ONE rule consults libc++ 21's full tables.

25 and 27 are still open, and today's lesson sharpens both of them.

25. **The corpus is audited, so the next question is what a corpus is FOR.**
    Today's ledger says the corpus's job is sharper than "the cells the
    batteries draw": the 774 over-claim cells are the only cells where
    deleting a table makes the rule *right*, and the 11,399 under-claim
    cells are the only cells where deleting it changes *nothing*. A corpus
    built from the over-claims demonstrates a fix; a corpus built from the
    under-claims demonstrates a gap. Worth: measuring what a corpus
    deliberately built to contain code points the table gets WRONG looks
    like — now with a third kind of cell in the ledger, the 289 agree cells
    that a deletion BREAKS.

26a. **`kIndicLinker` is the one table that does not earn its keep — so
    should it be DELETED?** It is right about 6 code points and wrong about
    9, and all nine of its over-claims are code points libc++ calls
    `InCB=Extend`. Worth: asking what those 6 code points are, what they
    cost, and whether a lesson that keeps a table it has measured to be
    net-harmful is teaching something true or teaching something
    comfortable.

27. **The Tags block is the widest gap, but is it the worst?** The arc's
    restricted table under-claims 1,674 code points; the Tags block is 96 of
    them. Worth: ranking the under-claimed code points by **consequence**
    rather than by count — which blocks have runs in real text, and which
    are singletons that only ever cost one column. Today's ledger is the
    instrument for that: the 1,674 under-claims are already partitioned
    into the 11,399 unchanged cells and the 774 fixed ones.

28. **The arc's ONE rule is now a clone of libc++ 21's tables.** Since
    Nov 22 and Nov 23 the shipped rule consults the oracle's data
    verbatim, so the arc's own product agrees with the stdlib by
    construction. Worth: asking what is left of the arc's rule that is
    *its own* — and whether a lesson that has replaced its own tables with
    the oracle's has stopped measuring anything at all.

29. **What should a lesson keep around purely to teach?** Today's answer
    for the tables is "a table you can point at and say *this is what the
    arc used to believe*". Worth: asking the same question of the arc's
    *batteries* — a battery drawn from a table the arc no longer believes
    in is a fossil, and Nov 25 showed the arc's corpus is exactly right
    about every code point its restricted tables get wrong.
