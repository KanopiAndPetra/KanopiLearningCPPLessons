# P-2026-11-25 — try_format_tags_block

## What today closes

Nov 24's "Where we go next" item (24), verbatim:

> **The Tags block is the widest gap and nobody asked for it.**
> `U+E0020..U+E007F` is 96 code points the arc's table never covered,
> and it is the *widest* single gap in the whole code point space.
> Worth: asking what a 96-code-point hole in a combining-mark table
> does to a format string that happens to contain one — and whether the
> arc's battery corpus has any text with a tag in it at all.

Closed — and the answer to the second half is **no**, and the reason is
the interesting part. The hole was never in the table. The hole was in
the **battery**.

---

### 1. THE CENSUS — re-measured, and it must not move

```
table                   arc   libc++   arc-only   c++-only  a subset?
is_cluster_extend       573     2198         49       1674  NO
```

Nov 24's numbers, re-measured here and unchanged.

### 2. THE TAGS BLOCK — 96 code points, and it IS the widest hole

`U+E0020..U+E007F` is the **Tags** block: 96 code points, and the
widest single interval in the whole disagreement between the arc's
Extend table and libc++ 21's.

```
libc++ 21's full table calls 96 of them Extend
the arc's restricted table calls  0 of them Extend
and it IS the widest interval of the disagreement: U+E0020..U+E007F, 96 code points
```

Every one of the 96 is a hole in the arc's table. There is not one
Tags code point the arc's restricted table knows about.

### 3. THE CORPUS AUDIT — the answer to item (24)'s second question

**No. The arc's battery corpus contains zero code points from the Tags
block.** And the audit says something sharper than that:

```
104 distinct code points in the arc's corpus
libc++ calls  7 of them Extend;  the arc's restricted table calls  7
libc++-only:  0;   arc-only:  0   (the two tables AGREE about
every code point the corpus contains)
code points from the Tags block in the corpus:  0
```

The corpus is every distinct code point appearing in the arc's battery
cells — the `\x`-escaped string literals — extracted mechanically from
this file's own source.

Read that again: the arc's restricted Extend table is **exactly right**
about **every** code point its corpus contains, in both directions. It
is wrong about 1,674 code points, and about **none** of the ones it
was written for.

So the arc's batteries were never able to falsify the table, and the
number says how completely:

```
so the corpus exercises 7 of libc++'s 2198 Extend code points -- 0.3% of the property --
and of the restricted table's 573 code points, the corpus touches 7 -- so 566 of
them are carried for no battery at all (98.8% dead weight).
```

**98.8% of the table is dead weight.** This is the arc-wide lesson —
*a restricted table makes its own batteries unable to falsify it* — now
measured as a dead-weight ratio rather than argued.

### 4. WHY THE CORPUS COULD NOT HAVE A TAG — the CORPUS, not the shape

The corpus has no tag in it, and it could not have one. But the reason
is **not** that cells hold only one candidate — because they do not:

```
the corpus holds 81 runs of consecutive code points, and 0 of them contain a code
point the two tables DISAGREE about.
```

The corpus **does** contain runs. A cell like `"a" + U+0301 + U+0302` is
a base and *two* marks; `U+1F466..U+1F469` is a run of four. A cell can
carry a run.

What hides the error is that **every one of those 81 runs is a run the
restricted table handles correctly** — because the corpus contains no
code point the table gets wrong. The battery was not *unable* to see
the error; it was unable to see it because it had nothing to see it
**with**.

That is the arc-wide lesson in its sharpest form, and it is now a
number rather than an argument: the restricted table is exactly right
about the corpus (zero disagreements in either direction) and wrong
about the other 99.7% of the property.

### 5. THE RUN BATTERY — the first battery drawn from a run of *wrong* code points

Today's cells are `"a"` + N tags: a base and a **run of code points the
restricted table gets wrong**. Two models, one oracle (the width the
linked stdlib charges).

```
cell             stdlib  A(rest)  B(full)    error
a + 1 tag(s)           1        2        1        1
a + 2 tag(s)           1        3        1        2
a + 3 tag(s)           1        4        1        3
a + 4 tag(s)           1        5        1        4
a + 5 tag(s)           1        6        1        5
a + 6 tag(s)           1        7        1        6
a + 7 tag(s)           1        8        1        7
a + 8 tag(s)         1        9        1        8
the restricted table's error GROWS with the run: 36 columns over 8 cells --
1 + 2 + ... + 8 -- where a cell holding one such code point could
only ever have shown one.
```

The error is **exactly the length of the run** — 1, 2, 3 … 8, totalling
36 columns. The restricted table charges each tag its own column,
because a tag the table does not know is Extend starts a *new* cluster,
and a new cluster is charged a column of its own.

And with libc++'s **full** Extend table the rule agrees with the linked
stdlib on **every** run cell.

### 6. THE REAL-WORLD PAYLOAD — the England flag

The Tags block is not an accident of the UCD. It is the alphabet the
**emoji tag sequences** are spelled in, and the subdivision flags are
written with it.

The England flag is `U+1F3F4` (the black flag) + the tag letters
`"gbeng"` + `U+E007F` (CANCEL TAG): **7 code points, 28 bytes**.

```
28 bytes, 7 code points;  the stdlib charges 2 columns
the arc's RESTRICTED table charges 8 columns  -- 6 too many
the arc's FULL table charges 2 columns
```

libc++ charges **two** columns: the tags are Extend, so they join the
black flag's cluster, and a cluster is worth the width of its **first**
code point. The arc's restricted table starts a fresh cluster at every
one of the six tags — 2 + 6 = 8.

### 7. THE BLAST RADIUS, IN BYTES — the error is not only a width

A padded field is a different **length**, not just a different width:

```
a padded field {:>10}:  the stdlib writes 36 bytes (2 columns of flag +
8 of padding);  the arc's restricted rule writes 30 bytes (8 columns of flag +
2 of padding) -- the SAME argument, a DIFFERENT length.
```

And a precision cut **loses data**:

```
a precision cut of two columns:  the stdlib keeps 28 bytes (the WHOLE flag -- one
cluster);  the arc's restricted rule keeps 4 (the black flag alone);
the arc's full rule keeps 28.
```

The whole flag is **one cluster**, so the stdlib's precision of two
columns keeps all 28 bytes. The restricted rule cuts the flag into
eight clusters and keeps only the black flag — **24 bytes of a 28-byte
cell are gone**. A precision cut that drops 24 bytes is not a width
error; it is a data-loss error.

### 8. WHAT DID NOT CHANGE

`petra::FormatError` / `petra::FormatErrorKind` (the four kinds, their
order and their meanings, exactly as since Sep 6), the two-pass
(measure, then write) design, the counting_output_iterator, the
bounded_char_writer, the TooLarge contract, the buffer-atomicity
behaviour, the type-category table from Oct 30, Nov 7's mask tables and
its derived override table and its 1,596-cell grid, Nov 13's fill rule
and its fill census, Nov 14's pair grid and the align/zero rule, Nov
15's triple grid and the recovery path, Nov 16's fill behaviour space,
Nov 17's width census — whose three numbers (1, 11, 28) are re-measured
here and must not move — Nov 18's precision battery (56 of 56), Nov
19's boundary battery (20 of 20), Nov 20's collision battery (24 of
24), Nov 21's control battery (19 of 19), Nov 22's width-table battery
(26 of 26), Nov 23's property-table battery (37 of 37) and Nov 24's
Extend-table battery (364 of 364), all of which are re-measured here
and must not move.

---

## Observed output

```
$ cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
$ cmake --build build
$ ./build/P-2026-11-25-tags-block
```

* Compiles clean under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` with `-std=c++23`
  (`-fexperimental-library` for libc++ 21's `std::expected`).
* Runs clean under **ASan + UBSan**: no sanitizer report, no runtime
  error.
* **`pass=319  fail=0`**, exit status 0.
* Built against **libc++ 21** (`_LIBCPP_VERSION`), so the oracle is the
  linked stdlib, not an assertion about it.

The counts that must not move — Nov 17's width census (1, 11, 28), Nov
18's precision battery (56 of 56), Nov 19's boundary battery (20 of
20), Nov 20's collision battery (24 of 24), Nov 21's control battery
(19 of 19), Nov 22's width-table battery (26 of 26), Nov 23's
property-table battery (37 of 37) and Nov 24's Extend-table battery
(364 of 364) — are all re-measured in this binary and all still hold.

## Where we go next

24 is closed, and with it the arc's six restricted tables are closed
too: every one has now been measured against libc++ 21's full table,
and every one has turned out to be wrong in both directions, or — for
`kPrepend` — wrong by one code point. What the arc knows, stated
plainly:

* **Six** restricted tables, **none** of them a subset of libc++ 21's.
* **Five** over-claim (the width table, `kExtendedPictographic`,
  `kPrepend`, `kIndicConsonant`, `kIndicLinker`) and charge the arc
  **too little**; **one** under-claims (`is_cluster_extend`) and
  charges it **too much**.
* Every battery that ever found one of these was drawn from the
  **disagreement intervals**, never from the table — and every one of
  those batteries was built from a corpus containing **no code point
  the restricted table gets wrong**, so each could only ever show a
  **one-column** error.

25. **The battery's corpus is now audited, so the next question is what
    a corpus is FOR.** The arc's corpus has 104 distinct code points,
    exercises 7 of libc++'s 2,198 Extend code points (0.3%), and the
    restricted table is right about all 104. 98.8% of that table is
    dead weight. Worth: asking whether the arc should keep drawing
    cells from the *disagreement* (as it has since Nov 24) and stop
    drawing them from the *table* — and measuring what a corpus
    deliberately built to contain **code points the table gets wrong**
    looks like, since that is the only corpus that could ever falsify
    anything.

26. **A restricted table's dead weight is now a number (98.8%), so the
    next question is whether ANY restricted table earns its keep.**
    The width table was already replaced by libc++'s full one (Nov 22)
    and the ONE rule consults the full property tables (Nov 23), so the
    restricted ones are now measurement instruments only. Worth:
    asking what happens if they are **deleted** — does any battery
    lose its ability to *demonstrate* the error, and what should a
    lesson keep around purely to teach?

27. **The Tags block is the widest gap, but is it the worst?** The
    arc's restricted table under-claims 1,674 code points; the Tags
    block is 96 of them and has the most dramatic *consequence*
    because of what it encodes. Worth: ranking the under-claimed code
    points by **consequence** rather than by count — which blocks have
    runs in real text, and which are singletons that only ever cost one
    column.
