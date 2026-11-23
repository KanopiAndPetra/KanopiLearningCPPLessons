# P-2026-11-23 — try_format_property_tables

## What today closes

Nov 22's "Where we go next" item (17), verbatim:

> **The other four restricted tables are still unexamined, and
> today's method is now written down.** `kExtendedPictographic`,
> `kPrepend`, `kIndicConsonant` and `kIndicLinker` are restricted the
> same way `kWidth2Ranges` was, and the width one turned out to be
> wrong on 8,493 code points in **both** directions. Worth carrying
> libc++'s full `extended_grapheme_cluster_table.h` (1,669 lines) and
> `indic_conjunct_break_table.h` and running today's census over each:
> the question is not "does the battery still agree" but "is the
> restricted table a subset of the full one at all". A table that is
> not a subset is not a restriction, and the battery drawn from the
> *disagreement intervals* is what finds it.

Closed — on all four — and **the answers are not the same answer.**

Three of the four restricted tables are **not restrictions at all**:
they disagree with libc++'s full tables in both directions, exactly as
Nov 22's width table did. The fourth **is** a genuine restriction — a
proper subset — and is *still* wrong, by exactly one code point.

And a fifth restricted table turned up on the way, one that was never
on the list: `is_cluster_extend`.

---

### 1. THE CENSUS — the question, asked of each table

Two tables, one property, the **whole** code point space (0x0000 to
0x10FFFF), exactly as Nov 22 measured the width table:

```
table                      arc  libc++  arc-only  c++-only  the arc's a subset?
kExtendedPictographic     2687    3537       528      1378  NO
kPrepend                    27      28         0         1  YES
kIndicConsonant            275     240        38         3  NO
kIndicLinker               15       6         9         0  NO
is_cluster_extend          573    2198        49      1674  NO
```

The disagreement intervals:

```
kExtendedPictographic   1378 code points libc++ only,  11 intervals;    528 arc only,  45 intervals
kPrepend                   1 code points libc++ only,   1 intervals;      0 arc only,   0 intervals
kIndicConsonant            3 code points libc++ only,   2 intervals;     38 arc only,   6 intervals
kIndicLinker               0 code points libc++ only,   0 intervals;      9 arc only,   7 intervals
is_cluster_extend       1674 code points libc++ only, 363 intervals;     49 arc only,   1 intervals
```

Read the `arc-only` column as **over-claims** and `c++-only` as
**under-claims**:

- **`kExtendedPictographic`** over-claims **528** code points and
  under-claims **1,378**. Its first over-claim is `U+219A..U+21A8` —
  the arc's table covers `0x2194..0x21AA` as "arrows", but Unicode
  16.0.0's `Extended_Pictographic` stops at `U+2199`. Its first
  under-claim is `U+2388`, then the whole `U+1F030..U+1F09F` block.
- **`kIndicConsonant`** over-claims **38** — the Kannada block
  `U+0C95..U+0CA8`, `U+0CAA..U+0CB3` and `U+0CB5..U+0CB9` (35 code
  points), plus `U+0CDE`, `U+0D04` and `U+0D3E` — and **no** Unicode
  release has ever listed any of them as `InCB=Consonant` (15.1, 16.0
  and 17.0 all have zero of them). It under-claims **3**: `U+09F0`,
  `U+09F1` and `U+0B5F`.
- **`kIndicLinker`** over-claims **9** — and libc++ is **right** about
  all nine. `U+0CCD`, `U+0D3B`, `U+1B44`, `U+A9C0`, `U+11A34`,
  `U+11A35`, `U+11D44`, `U+11D45`, `U+11D97`. In Unicode 16.0.0 every
  one of them is **`InCB=Extend`**, not `InCB=Linker`; two of them
  (`U+1B44`, `U+A9C0`) only became Linkers in 17.0.0.
- **`kPrepend`** is the one genuine restriction: a proper subset of
  libc++'s, missing **exactly one** code point — `U+113D1`, which
  Unicode **16.0.0** added (15.1.0 had 27, 16.0.0 has 28).

> **A restricted table makes its own batteries unable to falsify it.**
> That was Nov 22's lesson for the width table, and it turns out to be
> the arc-wide disease: **five** restricted tables, and **four** of
> them are not restrictions.

---

### 2. WHY NO BATTERY CAUGHT IT

The same reason as Nov 22, and it is worth restating because it now
has five instances rather than one.

Every battery in the arc's history was **drawn from the restricted
table** — the cells were chosen from the ranges the table covered, so
the table agreed with the stdlib on every cell, because the cells were
the table's own strong suit. The one battery that *was* drawn the other
way — Nov 22's width-table battery, drawn from the *disagreement
intervals* — is the one that found the defect.

---

### 3. THE BATTERY — 37 cells, two models, one oracle

The cells are drawn from the **disagreement intervals**, never from the
table. The oracle is the width the **linked stdlib** charges, measured
by asking it to pad the argument and counting the fill it wrote.

```
cell                                   stdlib  A(rest)  B(full)
U+2388 (libc++: EP, arc: not)               2        3        2
U+1F030 (libc++: EP, arc: not)              2        3        2
U+1F031 (libc++: EP, arc: not)              2        3        2
U+1F032 (libc++: EP, arc: not)              2        3        2
U+1F033 (libc++: EP, arc: not)              2        3        2
U+1F034 (libc++: EP, arc: not)              2        3        2
U+1F035 (libc++: EP, arc: not)              2        3        2
U+1F036 (libc++: EP, arc: not)              2        3        2
U+219A (arc: EP, libc++: not)               3        2        3
U+219B (arc: EP, libc++: not)               3        2        3
U+219C (arc: EP, libc++: not)               3        2        3
U+219D (arc: EP, libc++: not)               3        2        3
U+219E (arc: EP, libc++: not)               3        2        3
U+219F (arc: EP, libc++: not)               3        2        3
U+21A0 (arc: EP, libc++: not)               3        2        3
U+21A1 (arc: EP, libc++: not)               3        2        3
U+113D1 (libc++: Prepend, arc: not)         1        2        1
U+09F0 (libc++: Consonant, arc: not)        1        2        1
U+09F1 (libc++: Consonant, arc: not)        1        2        1
U+0B5F (libc++: Consonant, arc: not)        1        2        1
U+0C95 (arc: Consonant, libc++: not)        2        1        2
U+0C96 (arc: Consonant, libc++: not)        2        1        2
U+0C97 (arc: Consonant, libc++: not)        2        1        2
U+0C98 (arc: Consonant, libc++: not)        2        1        2
U+0C99 (arc: Consonant, libc++: not)        2        1        2
U+0C9A (arc: Consonant, libc++: not)        2        1        2
U+0C9B (arc: Consonant, libc++: not)        2        1        2
U+0C9C (arc: Consonant, libc++: not)        2        1        2
U+0CCD (arc: Linker, libc++: not)           2        1        2
U+0D3B (arc: Linker, libc++: not)           2        1        2
U+1B44 (arc: Linker, libc++: not)           2        1        2
U+A9C0 (arc: Linker, libc++: not)           2        1        2
U+11A34 (arc: Linker, libc++: not)          2        1        2
U+11A35 (arc: Linker, libc++: not)          2        1        2
U+11D44 (arc: Linker, libc++: not)          2        1        2
U+11D45 (arc: Linker, libc++: not)          2        1        2
U+11D97 (arc: Linker, libc++: not)          2        1        2

A  the RESTRICTED tables  agree on  0 of 37 cells
B  the FULL tables         agree on 37 of 37 cells
A wins 0 cells;  B wins 37 cells
```

The restricted tables lose **every** cell — all 37 — and the full
tables win **every** cell. Four families, one per table, and each
family's cell shape is the one that makes its property decisive:

| family | cell shape | the rule it exercises |
|---|---|---|
| `Extended_Pictographic` | `U+1F600 ZWJ X` | GB11 (`EP Extend* ZWJ × EP`) |
| `Prepend` | `U+113D1 U+0915` | GB9b (`Prepend ×`) |
| `InCB=Consonant` | `U+0915 U+094D X` | GB9c (the conjunct's right end) |
| `InCB=Linker` | `U+0915 X U+0915` | GB9c (the conjunct's linker) |

The GB11 cells are the sharpest: the *same* code point appears on both
sides of the ZWJ, so the cell is decided by that one code point's
`Extended_Pictographic`-ness and nothing else.

---

### 4. THE UNDER DIRECTION — and it is the bigger one

Nov 22's item (18) said the under-count is the dangerous one, and
asked for its blast radius to be measured rather than feared. Today's
battery has **both** directions, so it can be measured:

```
the arc charges TOO MUCH (visibly too wide): 12 cells
the arc charges TOO LITTLE (silently too narrow): 25 cells
```

**The silent one is the larger family** — 25 of 37 against 12 — and
the reason is the direction of the arc's errors. The arc's tables
over-*claim* (528 `Extended_Pictographic`, 38 `InCB=Consonant`, 9
`InCB=Linker` that libc++ does not have), and an over-claimed property
makes the arc **join** a cluster that libc++ **breaks**. A joined
cluster is worth the width of its *first* code point, so the arc
charges too little.

And the blast radius in **bytes**, which is where it actually bites:

```
"U+1F600 ZWJ U+219A" with a precision of 2
  the stdlib keeps 7 byte(s);  the arc's restricted rule keeps 10;  the full tables keep 7
```

The stdlib breaks the cluster after the ZWJ (`U+219A` is not
`Extended_Pictographic` in 16.0.0), so a precision of 2 keeps the
two-column `U+1F600` cluster and **drops** the `U+219A`. The arc's
restricted table calls `U+219A` `Extended_Pictographic`, so it joins
all three into one cluster, thinks the whole thing is two columns wide,
and keeps **every byte** — three bytes it should have dropped.

> A truncation that keeps the wrong bytes is worse than a padding that
> is off by one. That was Nov 22's closing line; today it is a
> measurement rather than an argument.

---

### 5. THE PROVENANCE — pinned, for all four

Nov 22's item (19) asked for the *other* tables' versions to be pinned
the way the width table's was. They can be, and the answer is a single
one: **libc++ 21's four tables are Unicode 16.0.0's.**

The witnesses are the intervals that separate two Unicode releases:

```
Extended_Pictographic: libc++ has 689 of the 689 code points 16.0.0 has and 17.0.0 dropped -> not 17.0.0's
Prepend: libc++ has all 28 code points of 16.0.0's (15.1.0 has 27) -> 16.0.0's
InCB=Consonant: libc++ has 0 of the 671 code points 17.0.0 added -> not 17.0.0's
InCB=Linker:    libc++ has 0 of the 14 code points 17.0.0 added -> not 17.0.0's
```

Each table is pinned by the release that **added** code points it
does *not* have, or **dropped** code points it *does*:

| table | witness | direction |
|---|---|---|
| `Extended_Pictographic` | the 689 code points 16.0.0 has and 17.0.0 dropped | libc++ has **all 689** → not 17.0.0 |
| `Prepend` | `U+113D1`, added in 16.0.0 | libc++ **has** it → at least 16.0.0 |
| `Prepend` | `U+11A3A`, dropped in 17.0.0 | libc++ **has** it → before 17.0.0 |
| `InCB=Consonant` | the 671 code points 17.0.0 added | libc++ has **none** → before 17.0.0 |
| `InCB=Linker` | the 14 code points 17.0.0 added | libc++ has **none** → before 17.0.0 |

The `Prepend` witnesses bracket it exactly: `U+113D1` is in 16.0.0 and
not in 15.1.0, and `U+11A3A` is in 16.0.0 and not in 17.0.0. libc++ has
both. So libc++'s `Prepend` table is **exactly** 16.0.0's — and
`Extended_Pictographic` is identical across 14.0.0, 15.0.0, 15.1.0 and
16.0.0, so it cannot narrow it further, but it excludes 17.0.0.

> **libc++ 21 ships FIVE tables out of ONE UCD release.** The width
> table (Nov 22, Unicode 16.0.0's `EastAsianWidth.txt`) and the four
> property tables (Unicode 16.0.0's `GraphemeBreakProperty.txt`,
> `emoji-data.txt` and `DerivedCoreProperties.txt`) are all from
> **Unicode 16.0.0**. The arc now knows where its oracle's data comes
> from, and the witness lists are in the source, so the day libc++
> regenerates against Unicode 17 this lesson **says so by name** instead
> of quietly losing cells.

#### The arc's own tables are a MIX

Asked the same question in both directions — *extra* is what the arc's
table has that the UCD's does not, *missing* is what the UCD's has that
the arc's does not:

```
table                  UCD release         extra  missing
kExtendedPictographic  16.0.0                528     1378
kExtendedPictographic  17.0.0                974     1135
kPrepend               15.1.0                  0        0
kPrepend               16.0.0                  0        1
kPrepend               17.0.0                  1        1
kIndicConsonant        16.0.0                 38        3
kIndicConsonant        17.0.0                 38      674
kIndicLinker           16.0.0                  9        0
kIndicLinker           17.0.0                  7       12
```

One row is all zeros: **`kPrepend` is Unicode 15.1.0's table, in
full.** The arc's `Prepend` table was copied from a UCD snapshot that
is one release behind its oracle's, and the only code point 16.0.0
added is the one it misses.

The others match **no** release: `kIndicConsonant`'s 38 extra
Kannada consonants are in no Unicode release's `InCB=Consonant` set
whatsoever, and its 3 missing ones are in 16.0.0's.

---

### 6. WHAT CHANGED IN THE SOURCE

| | before | after |
|---|---|---|
| the ONE rule's property lookups | the arc's five restricted tables | libc++ 21's **full** tables, behind the new `gb::props_full` bit |
| `Extended_Pictographic` | `kExtendedPictographic`, 32 hand-written ranges | libc++'s `kEgcEntries[1501]`, carried **verbatim** |
| `Prepend` | `kPrepend`, 15 ranges | the same table |
| `InCB=Consonant` | `kIndicConsonant`, 30 ranges | libc++'s `kIcbEntries[403]`, carried **verbatim** |
| `InCB=Linker` | `kIndicLinker`, 13 ranges | the same table |
| `Extend` | `is_cluster_extend()`, 15 ranges | the same table |
| the lookup | hand-written scans | libc++'s own `upper_bound` over the packed entries |
| the restricted tables | in use | kept behind the bit, so §21 can **measure** what they were worth |
| the provenance | none for these four | the UCD's 15.1.0 / 16.0.0 / 17.0.0 tables and the witness lists, carried in the source |

Both of libc++'s tables are carried **byte for byte**, packed the way
libc++ packs them, and searched the way libc++ searches them — an
`upper_bound` over the packed key `(code point << 11) | 0x7ff`, one
step back, then the range's upper bound. The EGC table's layout is
bits `[0,3]` property, `[4,10]` size, `[11,31]` lower bound; the ICB
table's is bits `[0,1]` property, `[2,10]` size, `[11,31]` lower bound
— libc++'s, and the two size fields differ because the two tables'
ranges differ in length.

**`Extend` came along for the ride, and it had to.** GB9c's left
context is `Consonant [Extend|Linker]* Linker`, and the nine code
points the arc's linker table over-claims are, in Unicode 16.0.0,
`Extend` — so a conjunct built on one of them is joined by GB9
(`Extend`) and not by GB9c (`Linker`), and the two rules are not the
same rule. The `Extend` table is the fifth restricted table, and it is
restricted too: **573** code points against libc++'s **2,198**, with
**1,674** missing and **49** over-claimed (the 49 are the `U+1ACF..
U+1AFF` block, which Unicode 17.0.0 added and 16.0.0 does not have).

---

### 7. WHAT DID NOT MOVE

Every earlier battery was re-measured and **not one number moved**:

| battery | before | after |
|---|---|---|
| Nov 17's width census (bytes / code points / columns) | 1 / 11 / 28 | **1 / 11 / 28** |
| Nov 17's width model vs the stdlib | 28 of 28 | **28 of 28** |
| Nov 18's precision battery | 56 of 56 | **56 of 56** |
| Nov 19's boundary battery | 20 of 20 | **20 of 20** |
| Nov 20's collision battery | 24 of 24 | **24 of 24** |
| Nov 21's control battery | 19 of 19 | **19 of 19** |
| Nov 22's width-table battery | 26 of 26 | **26 of 26** |

The reason is structural, not luck: **none of the code points in those
batteries lies in a disagreement interval.** The four restricted tables
were wrong about 2,406 code points between them, and the batteries had
never used any of them.

`petra::FormatError` / `petra::FormatErrorKind`, the two-pass
(measure, then write) design, the `counting_output_iterator`, the
`bounded_char_writer`, the TooLarge contract, the buffer-atomicity
behaviour, the type-category table, the mask tables, the fill rule,
the pair/triple grids, the fill behaviour space — all unchanged,
exactly as since Sep 6.

---

### 8. The build and the run

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-23-property-tables
```

`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`,
C++23, `-fexperimental-library` on Clang. **Clean, no warnings.** Exit 0
with `pass=255 fail=0`.

The sanitizer run — AddressSanitizer + UndefinedBehaviorSanitizer,
both on, no suppression:

```
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-11-23-property-tables
```

exits 0 with `pass=255 fail=0` and **no sanitizer report**.

The oracle is the toolchain's own `std::format` (libc++ 21): every
cell's expected width is measured by *running* it, never by reading the
standard.

---

## Where we go next

Nov 20's items (2)–(12), Nov 21's items (13)–(15) and Nov 22's items
(16) and (18) are closed. Item (17) is closed above, on all four
tables. Item (19) is closed for the four property tables, and the
width table's version was closed on Nov 22 — so the arc now knows the
Unicode version of all **five** of libc++ 21's tables. Two new ones:

20. **The arc's tables are now known to be a MIX of UCD releases, and
    the mix has a shape.** `kPrepend` is 15.1.0's table in full; the
    other three match *no* release; `kIndicConsonant` over-claims 38
    code points (35 Kannada, plus `U+0CDE`, `U+0D04` and `U+0D3E`) that
    **no** Unicode release has ever listed as `InCB=Consonant`, and
    misses 3 that 16.0.0 has. That is not a copy of a stale data file
    — it is a **hand-written** table. Worth: asking where it came
    from. The arc's `kIndicConsonant` claims the Kannada block the way
    a person would read "the Indic consonants" off a script chart, and
    the cost of that is 35 code points that were never consonants of
    any conjunct in any release.

21. **The `Extend` table is the fifth restricted table, and nobody
    listed it.** It is restricted (573 code points against libc++'s
    2,198), and its 49 over-claims are the `U+1ACF..U+1AFF` block,
    which Unicode **17.0.0** added. So the arc's `Extend` table is a
    *third* UCD vintage — 16.0.0 for the width, 15.1.0 for the
    Prepend, and something newer than 16.0.0 for the Extend. Worth:
    asking what a 573-code-point table can be *for*, when the full one
    is 2,198 and the arc's own batteries use 7 code points of it.
    The honest answer is probably "it was written for the batteries,
    and the batteries were drawn from it" — which is the arc-wide
    lesson, now five times over: **a restricted table makes its own
    batteries unable to falsify it.**
