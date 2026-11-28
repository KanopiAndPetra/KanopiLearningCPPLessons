# P-2026-11-28 — try_format_linker_cost

**What do those 6 code points COST?  Delete the table and find out.**

Closes Nov 27's "Where we go next" item (26a).

> **"kIndicLinker" is the one table that does not earn its keep — so should
> it be DELETED?** It is right about 6 code points and wrong about 9, and
> all nine of its over-claims are code points libc++ calls `InCB=Extend`.
> Worth: asking what those 6 code points are, what they cost, and
> whether a lesson that keeps a table it has measured to be net-harmful
> is teaching something true or teaching something comfortable.

**CLOSED — and the answer is NO: do not delete it.** Nov 27's ledger
counted the wrong unit, and the -3 was a version rather than a defect.

---

## 1. THE QUESTION, MADE SHARP

Nov 27 measured the arc's `kIndicLinker` table against the linked
stdlib and got a ledger: 6 code points agree, 9 over-claim, 0
under-claim, net **−3**. That made it the only one of the arc's six
restricted tables that does not earn its keep, and it left the obvious
question on the table: *delete it, or keep a table you have measured to
be net-harmful?*

Two things about that ledger were worth sharpening, and today's lesson
sharpens both by measuring rather than arguing.

**The first: the unit.** The ledger counted **code points**. But a
linker is not a code point — a linker is what lets a *conjunct* form,
and a conjunct is charged in **columns**. Nov 27's own note said the
nine over-claims "are code points libc++ calls `InCB=Extend`", which is
the clue: the two properties differ in exactly one respect, and that
respect is GB9c.

**The second: the version.** "The linked stdlib says no" is not the
same as "the property is no". libc++ 21 carries Unicode 16.0.0's
`DerivedCoreProperties.txt`, and the arc's table is not a subset of
Unicode 16.0.0's `InCB=Linker` at all — it is a **virama list**. The
arc's 15 code points include 9 viramas that Unicode 16 does not call
linkers, and 2 of *those* became linkers in Unicode 17.0.0.

So the sharp version of the question is mechanical: **delete the table,
and count the columns.**

---

## 2. THE 15, AND WHO DISAGREES ABOUT EACH ONE

One row per code point in the arc's own table, with three authorities
asked about it: the arc's table, the linked stdlib's full tables, and
the two Unicode versions.

```
code point name                               arc libc++   U16   U17 extend
U+094D   DEVANAGARI SIGN VIRAMA             yes    yes   yes   yes    yes
U+09CD   BENGALI SIGN VIRAMA                yes    yes   yes   yes    yes
U+0ACD   GUJARATI SIGN VIRAMA               yes    yes   yes   yes    yes
U+0B4D   ORIYA SIGN VIRAMA                  yes    yes   yes   yes    yes
U+0C4D   TELUGU SIGN VIRAMA                 yes    yes   yes   yes    yes
U+0CCD   KANNADA SIGN VIRAMA                yes     no    no    no    yes
U+0D3B   MALAYALAM VERTICAL BAR VIRAMA      yes     no    no    no    yes
U+0D4D   MALAYALAM SIGN VIRAMA              yes    yes   yes   yes    yes
U+1B44   BALINESE ADEG ADEG                 yes     no    no   yes    yes
U+A9C0   JAVANESE PANGKON                   yes     no    no   yes    yes
U+11A34  ZANABAZAR SQUARE SIGN VIRAMA       yes     no    no    no    yes
U+11A35  ZANABAZAR SQUARE SIGN VIRAMA 2     yes     no    no    no    yes
U+11D44  MASARAM GONDI VIRAMA               yes     no    no    no    yes
U+11D45  MASARAM GONDI VIRAMA 2             yes     no    no    no    yes
U+11D97  GUNJALA GONDI VIRAMA               yes     no    no    no    yes
```

Two readings fall straight out of the table.

**Every one of the 15 is Extend to the linked stdlib.** The `extend`
column is `yes` fifteen times. So the Linker property adds nothing to
these code points *except* GB9c — the conjunct. That is the whole
difference between the two properties, and it is why the cost of a
linker has to be counted in columns.

**But the arc's own Extend table agrees with only 7 of the 15.** The
arc's `is_cluster_extend` covers U+094D, U+09CD, U+0ACD, U+0B4D,
U+0C4D, U+0CCD and U+0D4D, and not the other 8. So the arc's two
sibling tables — its Extend table and its Linker table — **disagree
about the same viramas**, and that disagreement is what Section 3
measures.

And the version arithmetic:

```
against the LINKED STDLIB (Unicode 16.0.0): 6 agree, 9 over-claim.
against Unicode 16.0.0's 6 linkers:            6 agree.
against Unicode 17.0.0's 20 linkers:           8 agree, 7 over-claim,
12 under-claim -- net +1.
so the table's NET is -3 against the linked stdlib and +1 against
the NEXT Unicode: the -3 was a VERSION, not a table.
```

**The -3 was a version.** Two of the nine "over-claims" — U+1B44
(Balinese adeg adeg) and U+A9C0 (Javanese pangkon) — *are* Linkers in
Unicode 17.0.0. So the table the arc has measured to be net-harmful is
net-**positive** (+1) against the next Unicode, and it misses 12 of
Unicode 17's 20 linkers besides. The arc's table was not wrong about
nine code points; it was wrong about a *release*.

---

## 3. THE COST, IN COLUMNS

The payload is the conjunct itself: **consonant + linker + consonant**,
where the flank is the script's own consonant when the arc's
consonant table has one for that script, and Devanagari's otherwise.
GB9c does not ask whether the two consonants share a script, so the
conjunct forms either way; the arc's restricted tables simply do not
know about every script.

Four charges for each of the 15:

* **stdlib** — the linked stdlib's own width, measured through
  `charged_width()`; the oracle.
* **product** — the arc's ONE rule (`gb::all`), i.e. libc++'s tables.
* **arc** — the arc's *old* model (`gb::all & ~gb::props_full`), i.e.
  the arc's own restricted tables — the model the linker table governs.
* **deleted** — the arc's old model with the linker table deleted
  (`gb::vac_linker`).

```
name                             stdlib  product   arc  deleted  verdict
DEVANAGARI SIGN VIRAMA                1        1     1        2  the arc agrees
BENGALI SIGN VIRAMA                   1        1     1        2  the arc agrees
GUJARATI SIGN VIRAMA                  1        1     1        2  the arc agrees
ORIYA SIGN VIRAMA                     1        1     1        2  the arc agrees
TELUGU SIGN VIRAMA                    1        1     1        2  the arc agrees
KANNADA SIGN VIRAMA                   2        2     1        2  the arc UNDER-charges
MALAYALAM VERTICAL BAR VIRAMA         2        2     1        3  the arc UNDER-charges
MALAYALAM SIGN VIRAMA                 1        1     1        2  the arc agrees
BALINESE ADEG ADEG                    2        2     1        3  the arc UNDER-charges
JAVANESE PANGKON                      2        2     1        3  the arc UNDER-charges
ZANABAZAR SQUARE SIGN VIRAMA          2        2     1        3  the arc UNDER-charges
ZANABAZAR SQUARE SIGN VIRAMA 2        2        2     1        3  the arc UNDER-charges
MASARAM GONDI VIRAMA                  2        2     1        3  the arc UNDER-charges
MASARAM GONDI VIRAMA 2                2        2     1        3  the arc UNDER-charges
GUNJALA GONDI VIRAMA                  2        2     1        3  the arc UNDER-charges

the arc's own model agrees with the stdlib on 6 of the 15 conjuncts;
with the linker table DELETED it agrees on 1.
deleting the table FIXES 1 conjunct, BREAKS 6, and moves 8 from
under-charging to over-charging -- the table's two siblings disagree about
the same viramas.
only 8 of the 15 have a consonant of their own script in the arc's
consonant table; the other 7 need a foreign flank to build a conjunct at all.
```

### The ledger, in the right unit

```
the arc's own model, WITH the linker table      6 conjuncts right,  9 wrong
the arc's own model, WITHOUT it (deleted)        1 conjunct right, 14 wrong
```

**Deleting the table makes the arc's rule WORSE.** It breaks 6 of the
15 conjuncts — every one of the 6 it was right about — and fixes
exactly 1 (U+0CCD, the Kannada virama, where the arc's over-claim was
costing a column). The other 8 do not get fixed either; they move from
*under*-charging to *over*-charging, because deleting the Linker
property leaves them to be judged by the arc's Extend table, which
does not list them.

So the code-point ledger and the column ledger disagree, and the
column ledger is the right one:

> The table is net **harmful** only if a virama is counted as a code
> point. It is net **helpful** if a conjunct is counted as a column.

The reason is visible in the `arc` column: the arc's rule charges 1
column for all 15 conjuncts, because its Linker table lists all 15 and
GB9c forms the conjunct. The stdlib charges 2 for the 9 it does not
call linkers. That is 9 under-charges against 6 agreements — and
deleting the table removes both the agreements and the under-charges,
leaving the arc with 14 wrong out of 15.

### Why the 6 are worth keeping

The 6 the arc is right about are the 6 viramas of the scripts its
batteries use — Devanagari, Bengali, Gujarati, Oriya, Telugu and
Malayalam. For those, the conjunct forms and the arc's rule charges 1
column, exactly like the stdlib. That agreement is the entire value of
the table, and it is worth 6 of 15 conjuncts. The other 9 are
viramas of scripts the arc's own consonant table does not carry at
all: Balinese, Javanese, Zanabazar Square, Masaram Gondi and Gunjala
Gondi need a *foreign* flank to build a conjunct, and for those the
arc's rule under-charges — which is a defect, but a defect of the
*consonant* table as much as of the linker table.

---

## 4. WHAT DID NOT CHANGE

`petra::FormatError` / `petra::FormatErrorKind` (the four kinds, their
order and their meanings, exactly as since Sep 6), the two-pass
(measure, then write) design, the counting_output_iterator, the
bounded_char_writer, the TooLarge contract, the buffer-atomicity
behaviour, the type-category table from Oct 30, Nov 7's mask tables and
its derived override table and its 1,596-cell grid, Nov 13's fill rule
and its fill census, Nov 14's pair grid and the align/zero rule, Nov 15's
triple grid and the recovery path, Nov 16's fill behaviour space, Nov 17's
width census — whose three numbers (1, 11, 28) are re-measured here and
must not move — Nov 18's precision battery (56 of 56), Nov 19's boundary
battery (20 of 20), Nov 20's collision battery (24 of 24), Nov 21's
control battery (19 of 19), Nov 22's width-table battery (26 of 26),
Nov 23's property-table battery (37 of 37), Nov 24's Extend-table
battery (364 of 364), and Nov 27's deletion battery (774 cells fixed,
289 broken, 11,399 unchanged) are all re-measured here and all still
hold.

The only change to the carried-forward arc is Section 25 itself, plus
the two helpers it adds: `in_range_set()` (membership in a
`WidthRange[]` array) and the `LinkerRow` table. No rule changed, no
bit changed, and `gb::all` still means the same thing.

---

## 5. OBSERVED OUTPUT

```
$ cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
$ cmake --build build
$ ./build/P-2026-11-28-linker-cost
```

* Compiles clean under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` with `-std=c++23`
  (`-fexperimental-library` for libc++ 21's `std::expected`).
* Runs clean under **ASan + UBSan**: no sanitizer report, no runtime
  error.
* **`pass=402  fail=0`**, exit status 0. The 385 assertions carried
  forward from Nov 27 still pass, plus 17 new ones.
* Built against **libc++ 21** (`_LIBCPP_VERSION`), so the oracle is the
  linked stdlib, not an assertion about it.

The counts that must not move — Nov 17's width census (1, 11, 28),
Nov 18's precision battery (56 of 56), Nov 19's boundary battery (20 of
20), Nov 20's collision battery (24 of 24), Nov 21's control battery
(19 of 19), Nov 22's width-table battery (26 of 26), Nov 23's
property-table battery (37 of 37), Nov 24's Extend-table battery (364 of
364) and Nov 27's deletion battery (774/289/11,399) — are all
re-measured in this binary and all still hold.

---

## 6. WHERE WE GO NEXT

26a is closed: the table is **not** deleted, because deleting it makes
the arc's rule worse (1 conjunct right out of 15, against 6 with the
table), and because the −3 that condemned it was a Unicode *version*
rather than a defect. The table is kept as an EXHIBIT — the product
reads libc++'s tables (Nov 27) — and what it exhibits is now measured:
the arc believed 15 viramas were linkers, 6 of them were, 2 more became
so in Unicode 17, and the arc's own Extend table agreed with only 7 of
the 15.

25, 27, 28 and 29 are still open, and today's lesson sharpens two of
them.

25. **The corpus is audited, so the next question is what a corpus is
    FOR.** Today's row table says the corpus's job is sharper than "the
    cells the batteries draw": the 774 over-claim cells are the only
    cells where deleting a table makes the rule *right*, and the 11,399
    under-claim cells are the only cells where deleting it changes
    *nothing*. Worth: measuring what a corpus deliberately built to
    contain code points the table gets WRONG looks like.

26b. **The arc's two restricted tables disagree about the same 8
    viramas.** The Extend table lists 7 of the linker table's 15, and
    the other 8 are exactly the ones that move from under-charging to
    over-charging when the linker table is deleted. Worth: asking what
    the arc's own rule does with a virama it calls a Linker but does not
    call Extend — whether the two tables can be reconciled into one
    table of viramas, and what that would cost the 6 agreements.

27. **The Tags block is the widest gap, but is it the worst?** The arc's
    restricted table under-claims 1,674 code points; the Tags block is
    96 of them. Worth: ranking the under-claimed code points by
    **consequence** rather than by count — which blocks have runs in
    real text, and which are singletons that only ever cost one column.

28. **The arc's ONE rule is now a clone of libc++ 21's tables.** Since
    Nov 22 and Nov 23 the shipped rule consults the oracle's data
    verbatim, so the arc's own product agrees with the stdlib by
    construction. Worth: asking what is left of the arc's rule that is
    *its own* — and whether a lesson that has replaced its own tables
    with the oracle's has stopped measuring anything at all.

29. **What should a lesson keep around purely to teach?** Today's answer
    for the six restricted tables is "a table you can point at and say
    *this is what the arc used to believe*". Worth: asking the same
    question of the arc's *batteries* — a battery drawn from a table the
    arc no longer believes in is a fossil, and Nov 25 showed the arc's
    corpus is exactly right about every code point its restricted tables
    get wrong.
