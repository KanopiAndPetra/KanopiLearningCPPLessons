# P-2026-11-24 — try_format_extend_table

## What today closes

Nov 23's "Where we go next" item (21), verbatim:

> **The `Extend` table is the fifth restricted table, and nobody
> listed it.** It is restricted (573 code points against libc++'s
> 2,198), and its 49 over-claims are the `U+1ACF..U+1AFF` block, which
> Unicode **17.0.0** added. So the arc's `Extend` table is a *third* UCD
> vintage — 16.0.0 for the width, 15.1.0 for the Prepend, and
> something newer than 16.0.0 for the Extend. Worth: asking what a
> 573-code-point table can be *for*, when the full one is 2,198 and the
> arc's own batteries use 7 code points of it. The honest answer is
> probably "it was written for the batteries, and the batteries were
> drawn from it" — which is the arc-wide lesson, now five times over:
> **a restricted table makes its own batteries unable to falsify it.**

Closed — and the answer is that the table was written FOR the batteries,
and the batteries were drawn FROM it, and the table is wrong in **both**
directions, and the under-count is the big one.

And the direction is **new**. It is the first restricted table in the
arc whose error makes the arc charge **too much** rather than too
little.

---

### 1. THE CENSUS — the sixth table, on its own

The same question Nov 22 asked of the width table and Nov 23 asked of
the four property tables, asked of the sixth: is the restricted table a
subset of the full one at all?

```
table                   arc   libc++   arc-only   c++-only  a subset?
is_cluster_extend       573     2198         49       1674  NO
```

The arc's table is **not** a subset — in either direction, and by more
than Nov 22's width table was.

### 2. THE TWO DIRECTIONS ARE NOT SYMMETRIC

This is the shape, and it is different from every other table in the
arc:

```
arc-only  (the arc has it, libc++ does not): 49 code points, in 1 interval
c++-only  (libc++ has it, the arc does not): 1674 code points, in 363 intervals
```

* The **over-claim** is one interval: `U+1ACF..U+1AFF`, exactly 49
  code points, and it is the block **Unicode 17.0.0 added** to Extend.
  So on that one block the arc's table is *newer* than the oracle's —
  and libc++ 21 ships 16.0.0's data, which is what the arc has to
  match.
* The **under-claim** is 363 intervals — 1,674 code points, and they are
  not scattered: they are whole blocks the arc's table never covered.

### 3. WHY THE UNDER-COUNT MAKES THE ARC CHARGE **TOO MUCH**

This is the new part, and it follows from the arc's own column rule:
*a cluster is worth the width of its FIRST code point*.

* A property the arc **over-claims** (as all four property tables did)
  makes the arc **join** a cluster libc++ breaks. A joined cluster is
  worth the width of its *first* code point — so the arc charges too
  **little**. That was Nov 23's finding, 25 cells too narrow against 12
  too wide.
* The Extend table **under-claims** 1,674 code points, so the arc
  starts a **new** cluster where libc++ keeps one. A new cluster is
  charged a column of its **own** — so the arc charges too **much**.

The arc's Prepend table had exactly one such code point (`U+113D1`) on
Nov 23. The Extend table has 1,674, and they dominate.

### 4. THE BATTERY — 364 cells, two models, one oracle

The cells are drawn from the **disagreement intervals**, never from the
table — the first code point of every run of consecutive code points
where the two tables differ, with an ASCII base letter in front, so the
cell is exactly "base + candidate" and the candidate's Extend-ness
decides whether the two are ONE cluster or TWO.

The oracle is the width the **linked stdlib** charges, measured by
asking it to pad the argument and counting the fill it wrote.

```
cell                                     stdlib  A(rest)  B(full)
U+0483 (libc++: Extend, arc: not)             1        2        1
U+0591 (libc++: Extend, arc: not)             1        2        1
U+05BF (libc++: Extend, arc: not)             1        2        1
...
U+1ACF (arc: Extend, libc++: not)             2        1        2

A  the RESTRICTED tables  agree on  5 of 364 cells
B  the FULL tables         agree on 364 of 364 cells
the arc charges TOO MUCH (visibly too wide): 358 cells
the arc charges TOO LITTLE (silently too narrow):  1 cells
```

The restricted table loses 359 of 364 cells, and it loses them in the
*opposite* direction from the property tables: 358 too wide, 1 too
narrow.

### 5. THE FIVE CELLS THE ARC GETS RIGHT — and why

This is the sharpest result of the day, and it is the arc-wide lesson
**measured** rather than argued.

The arc's restricted Extend table is wrong about **every one** of the
363 cells in the c++-only family — the table says "not Extend" and
libc++ says "Extend" — and yet the arc's column count comes out right
on five of them. The reason is that **two other restricted tables of
the arc's own** join those code points anyway:

```
U+09BE   the arc's Extend table says NO;  libc++ says YES;
         the arc's SpacingMark table says YES, the Linker table says no
U+0A81   the arc's Extend table says NO;  libc++ says YES;
         the arc's SpacingMark table says YES, the Linker table says no
U+0D3B   the arc's Extend table says NO;  libc++ says YES;
         the arc's SpacingMark table says no , the Linker table says YES
U+A9C0   the arc's Extend table says NO;  libc++ says YES;
         the arc's SpacingMark table says no , the Linker table says YES
U+11D97  the arc's Extend table says NO;  libc++ says YES;
         the arc's SpacingMark table says no , the Linker table says YES
```

* **GB9a** (the arc's *SpacingMark* table) joins a spacing mark to the
  preceding cluster — two of the five.
* **GB9c** (the arc's *Linker* table) joins a linker to the preceding
  cluster — three of the five.

And the cancellation is exact: the arc's Linker table's nine
over-claims (Nov 23's `U+0CCD`, `U+0D3B`, `U+1B44`, `U+A9C0`,
`U+11A34`, `U+11A35`, `U+11D44`, `U+11D45`, `U+11D97`) are code points
libc++ calls **Extend** — so the table that is *wrong* about them is
the one that *saves* the Extend table.

> So the arc's restricted Extend table is right about **no code point
> on its own**. It gets five cells right because two other restricted
> tables of the arc's own are right about them too, and the errors
> cancel. **A battery drawn from the table's own ranges could never
> have found this** — the arc is right for the wrong reason, and the
> arc-wide lesson now has a mechanism, not just a slogan.

### 6. THE BLAST RADIUS, IN BYTES

The under-count does not just cost a column; it costs bytes. A precision
cut lands on a **cluster** boundary, so a mark the arc does not know to
be Extend is cut off.

The widest libc++-only interval is `U+E0020..U+E007F` (96 code points,
the Tags block) — so that is the cell:

```
"a" + U+E0020, precision 1:
  the stdlib keeps 5 bytes;  the arc's restricted rule keeps 1;  the full tables keep 5
"a" + U+1ACF, precision 1:
  the stdlib keeps 1 byte;  the arc's restricted rule keeps 4;  the full tables keep 1
```

* The **under-count** makes the arc keep **fewer** bytes than the
  stdlib — it cuts the mark off.
* The **over-count** makes the arc keep **more** bytes than the stdlib
  — it keeps the mark the stdlib dropped.

The two directions of one restricted table produce opposite byte-level
defects, and Nov 23's arc had only ever seen the "keeps too many" one.

### 7. WHAT THE TABLE WAS FOR — the blocks it never covered

Item (21) asked what a 573-code-point table is *for*. The answer,
measured: it covers the code points its batteries need and almost
nothing else.

The widest gaps — the intervals the arc's table never covered:

```
U+E0020..U+E007F    96 code points      U+1DA00..U+1DA36    55
U+1DA3B..U+1DA6C    50                  U+1CF00..U+1CF2D    46
U+0591..U+05BD      45 (Hebrew points)  U+0F99..U+0FBC      36
U+08E3..U+0902      32                  U+2DE0..U+2DFF      32
```

And the blocks, by name — 32 named blocks account for 417 of the 1,674:

```
Hebrew points (U+0591..U+05BD)      45      Khmer marks (U+17B4..U+17D3)   21
Arabic marks (U+064B..U+065F)      21      Tibetan marks (U+0F71..U+0F84) 19
Arabic Extended-A (U+08E3..U+0903) 32      Thai marks (U+0E31..U+0E4E)    16
Thaana (U+0730..U+074A)            27      Myanmar marks (U+102B..U+103E) 14
NKo (U+07A6..U+07B0)               11      Lao marks (U+0EB1..U+0EBC)      10
Tags (U+E0020..U+E007F)            96      Devanagari marks (U+093A..U+094F) 10
```

The arc's own table carries eight ranges and seven viramas — and one
of the eight, `0x1AB0..0x1AFF`, is a whole Unicode **block**, which is
where the 49 over-claims come from: the range ends at `U+1AFF`, which
Unicode 17.0.0 added, so one line of source bought 49 wrong answers.

---

## Observed output

```
$ cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
$ cmake --build build
$ ./build/P-2026-11-24-extend-table
```

* Compiles clean under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` with `-std=c++23`
  (`-fexperimental-library` for libc++ 21's `std::expected`).
* Runs clean under **ASan + UBSan**: no sanitizer report, no runtime
  error.
* **`pass=274  fail=0`**, exit status 0.
* Built against **libc++ 21** (`_LIBCPP_VERSION`), so the oracle is the
  linked stdlib, not an assertion about it.

The counts that must not move — Nov 17's width census (1, 11, 28),
Nov 18's precision battery (56 of 56), Nov 19's boundary battery (20 of
20), Nov 20's collision battery (24 of 24), Nov 21's control battery
(19 of 19), Nov 22's width-table battery (26 of 26) and Nov 23's
property-table battery (37 of 37) — are all re-measured in this binary
and all still hold.

## Where we go next

Nov 23's items (17) and (19) and (21) are closed, and with (21) the arc
has now examined **all six** of its restricted tables — the width table
and the five property tables. What the arc knows:

* **Six** restricted tables, **none** of them a subset of libc++ 21's
  full one.
* **Five** of the six over-claim (the width table, `kExtendedPictographic`,
  `kPrepend`, `kIndicConsonant`, `kIndicLinker`) and therefore charge
  the arc **too little**; **one** under-claims (`is_cluster_extend`) and
  therefore charges the arc **too much**.
* Every battery that ever found one of these was drawn from the
  **disagreement intervals**, never from the table.

22. **The arc's six restricted tables are now a closed set, and the
    next question is what the arc does with them.** They are kept only
    so a section can *measure* what the restriction was worth — and
    today's measurement shows the restriction is worth 359 wrong cells
    out of 364. Worth: auditing the arc's own batteries **cell by cell**
    against the full tables and listing every cell that was drawn from a
    restricted table's ranges — i.e. the batteries that could never have
    falsified anything.

23. **The five accidental agreements are a pattern, not a coincidence.**
    Two restricted tables (SpacingMark, Linker) join code points a third
    table (Extend) gets wrong, and the errors cancel. Worth: asking
    whether the arc's *other* property tables cancel each other the same
    way — the `kIndicConsonant`/`kIndicLinker` pair is a candidate,
    since a linker is also a virama and a consonant can be an Extend.

24. **The Tags block is the widest gap and nobody asked for it.**
    `U+E0020..U+E007F` is 96 code points the arc's table never
    covered, and it is the *widest* single gap in the whole code point
    space. Worth: asking what a 96-code-point hole in a combining-mark
    table does to a format string that happens to contain one — and
    whether the arc's battery corpus has any text with a tag in it at
    all.
