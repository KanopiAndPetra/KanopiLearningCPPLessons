# P-2026-11-22 — try_format_width_table

## What today closes

Nov 21's "Where we go next" item (16), verbatim:

> **The restricted tables have still never been broken.** Nov 20's
> items (2)–(4) all say the same thing from three directions: the
> `kWidth2Ranges`, `kExtendedPictographic`, `kPrepend`,
> `kIndicConsonant` and `kIndicLinker` tables are restricted to what
> the batteries need, and every battery so far has agreed with the
> stdlib. Today added the arc's first COMPLETE table, which shows the
> way: pick one restricted table, carry the full one from libc++, and
> find the cells where the two disagree.

Closed — on the **width** table — and the answer is not "the
restriction cost some cells". The answer is that **the restricted
table was not a restriction of the full one at all.**

---

### 1. THE FINDING — two tables, neither a subset of the other

The arc's own width table (`kWidth2Restricted`, 54 hand-written
ranges) and libc++ 21's (`kWidth2Packed`, the 110 packed entries from
`__format/width_estimation_table.h`) were compared over the **whole**
code point space, 0x0000 to 0x10FFFF:

```
the arc's RESTRICTED table    54 ranges, 174749 width-2 code points
libc++ 21's FULL table        110 packed entries, 182942 width-2 code points
they DISAGREE on   8493 code points, in 58 intervals
  the arc says ONE column, the full table says TWO:   8343 code points
  the arc says TWO columns, the full table says ONE:    150 code points
```

A *restricted* table should be a **subset** of the full one. This one
is not a subset in **either** direction: it misses 8,343 code points
that are wide and it marks 150 that are narrow.

The first eight disagreement intervals:

```
U+02630 .. U+02637     U+0268A .. U+0268F     U+02E9A .. U+02E9A
U+02EF4 .. U+02EFF     U+02FD6 .. U+02FEF     U+03097 .. U+03098
U+03100 .. U+03104     U+03130 .. U+03130
```

---

### 2. WHY NO BATTERY CAUGHT IT

This is the part worth keeping.

Every battery in the arc's history was **drawn from the restricted
table** — the cells were chosen from the ranges the table covered. So
the table agreed with the stdlib on every cell, because the cells
were the table's own strong suit.

> **A restricted table makes its own batteries unable to falsify it.**

Nov 21's lesson added the arc's first *complete* table (the 21 control
ranges) and said so explicitly: "pick one restricted table, carry the
full one from libc++, and find the cells where the two disagree." The
finding is that the batteries had to be **redrawn from the
disagreement intervals**, not from the table.

---

### 3. THE BATTERY — 26 cells, two models, one oracle

The oracle is the width the **linked stdlib** charges, measured by
asking it to pad the argument and counting the fill it wrote.

```
argument                 stdlib A(rest) B(full)
U+2630 trigram                2      1      2
U+268A monogram               2      1      2
U+A960 hangul jamo            2      1      2
U+16FE0 tangut mark           2      1      2
U+17000 tangut                2      1      2
U+1AFF0 kana ext-B            2      1      2
U+1B000 katakana              2      1      2
U+1B170 nushu                 2      1      2
U+1D300 monogram              2      1      2
U+1F004 mahjong               2      1      2
U+1F0CF joker                 2      1      2
U+1F18E abacus                2      1      2
U+1F210 squared CJK           2      1      2
U+1F7E0 orange disc           2      1      2
U+1F7F0 heavy equals          2      1      2
U+3248 circled ten            1      2      1
U+3249 circled twenty         1      2      1
U+1F6C6                       1      2      1
U+1F6CB                       1      2      1
U+1F6CD                       1      2      1
U+1F6D3                       1      2      1
U+1F6E0                       1      2      1
U+1F6EA                       1      2      1
U+1F6F0                       1      2      1
U+1F6F3                       1      2      1
U+2E9A                        1      2      1

  A  the RESTRICTED table  agree on  0 of 26 cells
  B  the FULL table        agree on 26 of 26 cells
  A wins 0 cells B loses; B wins 26 cells A loses
```

The restricted table loses **every** cell. It is not that it was
slightly off — it was wrong on every cell the battery was built to
ask about.

#### The two directions fail in OPPOSITE ways

```
15 cells where the arc UNDER-counts (too little padding): the restricted table agrees on  0
11 cells where the arc OVER-counts (too much padding): the restricted table agrees on  0
```

That is why the battery needs **both** families. An under-count charges
too little padding; an over-count charges too much. A battery built
from only one direction would not see the other at all.

---

### 4. THE PRECISION SHARES THE TABLE

Nov 18 measured that the precision's unit is the **column**, the same
unit the width uses — so the width's table **is** the precision's
table. The restricted table was therefore wrong for the precision too,
and the consequence is visible in the *bytes kept*, not just in a
count:

```
U+2630 (two columns) with precision 1 -> 0 byte(s)
U+2E9A (one column)  with precision 1 -> 3 byte(s)
the arc's restricted rule keeps 3 byte(s) of the two-column cluster; the full table keeps 0
```

The arc's old rule would have **kept** a two-column cluster under a
precision of 1 — because it believed the cluster was one column wide.
The stdlib keeps nothing. A truncation that keeps the wrong bytes is
worse than a padding that is off by one.

---

### 5. THE PROVENANCE — the oracle's Unicode version, pinned

[format.string.std]/12 defines the width by **reference to a data
file**:

> Any code point with the East_Asian_Width="W" or East_Asian_Width="F"
> Derived Extracted Property as described by UAX #44

So the table is not a design decision — it is a **copy of a data
file**, and a copy has to say which version of the data file. libc++'s
own header says:

```
The data is generated from
- https://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt
```

"latest" is not a version. So the version is **measured**, and pinned in
the source by **witnesses** — the intervals that separate one Unicode
release from the next:

| witness | meaning | oracle's table |
|---|---|---|
| `kUnicode16Only` | 134 code points, 13 intervals — in Unicode **16.0.0**'s file, not in 15.1.0's | has **all 134** |
| `kUnicode17Only` | 157 code points, 11 intervals — in Unicode **17.0.0**'s file, not in 16.0.0's | has **none** |

```
kUnicode16Only  134 code points in 13 intervals: the oracle's table has 134
kUnicode17Only  157 code points in 11 intervals: the oracle's table has   0
-> the oracle's table is Unicode 16.0.0's EastAsianWidth.txt
   (it has everything 16.0.0 has and nothing 17.0.0 has)
the arc's RESTRICTED table has   9 of the 134
```

The two witness lists **bracket** the version: the oracle's table has
everything 16.0.0 has and nothing 17.0.0 has. That is why the day
libc++ regenerates against Unicode 17, this lesson **says so by name**
— the `kUnicode17Only` check fails loudly instead of the battery
quietly losing cells.

The arc's own restricted table has only **9 of the 134** code points
that Unicode 16.0.0 added, which is the whole of the 8,343-code-point
under-count in one sentence.

---

### 6. WHAT CHANGED IN THE SOURCE

| | before | after |
|---|---|---|
| the ONE rule's width table | `kWidth2Ranges` — 54 hand-written ranges | `kWidth2Packed` — libc++ 21's **110 packed entries**, carried verbatim |
| the lookup | linear scan of 54 ranges | `upper_bound` over the packed entries, **the way libc++ does it** |
| the old table | in use | kept as `kWidth2Restricted`, behind the new `gb::width_full` bit, so §20 can **measure** what it was worth |
| the provenance | none | `kUnicode16Only` / `kUnicode17Only` witness lists |

The packing is libc++'s own: the low 14 bits are the size of the
range, the high 18 bits are the lower bound, so

```
lower = entry >> 14
upper = lower + (entry & 0x3fff)
```

and the table is searched with an `upper_bound` on the packed value
`(cp << 14) | 0x3fff`.

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

The reason is structural, not luck: **none of the code points in
those batteries lies in a disagreement interval.** The restricted table
was wrong about 8,493 code points, and the batteries had never used
any of them.

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
./build/P-2026-11-22-width-table
```

`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`,
C++23, `-fexperimental-library` on Clang. **Clean, no warnings.** Exit 0
with `pass=231 fail=0`.

The sanitizer run — AddressSanitizer + UndefinedBehaviorSanitizer,
both on, no suppression:

```
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-11-22-width-table
```

exits 0 with `pass=231 fail=0` and **no sanitizer report**.

The oracle is the toolchain's own `std::format` (libc++ 21): every
cell's expected width is measured by *running* it, never by reading the
standard.

---

## Where we go next

Nov 20's items (2)–(12) and Nov 21's items (13)–(15) stand. Item (16)
is closed above, on the width table. Three new ones:

17. **The other four restricted tables are still unexamined, and
    today's method is now written down.** `kExtendedPictographic`,
    `kPrepend`, `kIndicConsonant` and `kIndicLinker` are restricted
    the same way `kWidth2Ranges` was, and the width one turned out to
    be wrong on 8,493 code points in **both** directions. Worth
    carrying libc++'s full `extended_grapheme_cluster_table.h` (1,669
    lines) and `indic_conjunct_break_table.h` and running today's
    census over each: the question is not "does the battery still
    agree" but "is the restricted table a subset of the full one at
    all". A table that is not a subset is not a restriction, and the
    battery drawn from the *disagreement intervals* is what finds it.

18. **The UNDER direction is the dangerous one, and it is the one the
    arc's own table missed.** An over-count charges too much padding
    and the output is visibly too wide. An under-count charges too
    little, and the result is *silently* wrong — a string that fits
    where it should not, or a precision that keeps bytes it should
    have dropped. Worth: measuring the under-count's blast radius
    concretely, by asking what the stdlib writes for a width/precision
    pair where the arc's model would have under-charged.

19. **The width table's version is pinned by witnesses; the OTHER
    tables' versions are not.** Today's `kUnicode16Only` /
    `kUnicode17Only` lists are a *general* technique: the intervals
    that separate two Unicode releases are a cheap, exact, code-point
    -level signature of "which data file did this table come from".
    The grapheme cluster table and the Indic conjunct table come from
    the same UCD snapshot, and their versions can be pinned the same
    way — which matters because Nov 20 already found that libc++ 21
    implements the **pre-Unicode-18** GB9c rule while the standard
    has moved on. If the width table says 16.0.0 and the grapheme
    table says something else, that is a fact about the stdlib worth
    writing down.
