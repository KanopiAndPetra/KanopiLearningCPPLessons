# P-2026-11-18 — try_format_precision

## What today closes

Nov 17's "Where we go next" item (3), verbatim:

> **The PRECISION has the same unit.** [format.string.std]/15: for
> string types the precision "specifies the longest prefix of the
> formatted argument to be included in the replacement field such that
> the field width of the prefix is no greater than the value of this
> option." So precision is measured in columns too — the same table, the
> same unit. Worth measuring, and worth asking whether the arc's
> scanner needs a second Unicode table for it.

Closed, and the answer to its own question is **no**: the arc does not
need a second Unicode table. The precision's unit is the **COLUMN** —
the same field-width unit the width option uses — so the ONE table the
arc already carries (Nov 17's `kWidth2Ranges`) decides both rules.

What the precision needs that the width did not is the **CLUSTER
BOUNDARY**: the prefix is cut at an extended grapheme cluster boundary,
and a cluster is worth the width of its FIRST code point.

And a second thing fell out of the measurement, which is a **DEFECT in
the arc's own model** — see "THE DEFECT" below.

## THE RULE

[format.string.std]/15 (C++26's wording; C++23's /13 says the same
thing under a different paragraph number):

> The *precision* option is valid for floating-point and string types.
> …
>
> For string types, this option specifies the longest prefix of the
> formatted argument to be included in the replacement field such that
> the field width of the prefix is no greater than the value of this
> option.

Three words carry the whole rule: **"longest prefix"**, **"field
width"**, and **"no greater than"**. So:

- the precision's unit is the **field width**, i.e. the **column** — the
  same unit, and the same table, as the width option;
- the prefix is cut at a **cluster boundary**, and a cluster is worth
  the width of its **first** code point;
- the precision applies to the **argument**, before the width pads.

Note the asymmetry with the width: the width *pads* to a minimum, the
precision *truncates* to a maximum. One is a floor, the other is a
ceiling, and both are denominated in columns.

## THE MEASUREMENT — a census of three truncation rules

One battery of 56 cells (14 arguments × 4 precisions), three candidate
rules, one oracle: the output the **linked stdlib actually produces**,
measured and not asserted.

- **BYTES** — keep the first `p` bytes.
- **CODE POINTS** — keep the first `p` code points.
- **COLUMNS** — keep the longest prefix whose *field width* is no
  greater than `p`, cut at a cluster boundary.

```
    the rules, over 56 cells:
      BYTES       agree on 25 of 56 cells
      CODE POINTS agree on 42 of 56 cells
      COLUMNS     agree on 56 of 56 cells
```

The column rule reproduces the stdlib on **every** cell. The code-point
rule — the rule the arc's own Nov 13 finding implied — loses 14 cells,
and the byte rule loses 31.

The cells where the code-point rule loses are exactly the cells where
the two units differ: the wide code points (U+1F600, U+4E00, U+FF21,
U+4DC1) and the clusters (the combining mark, the ZWJ sequences).

## KEY FINDING #1 — THE CUT LANDS ON A CLUSTER BOUNDARY

`'e'` plus U+0301 is **one cluster**: one column, two code points,
three bytes. A precision of 1 keeps the **whole** three bytes:

```
  the truncation lands on a CLUSTER boundary:
    'e' + U+0301 is one cluster: 1 column, 2 code points, 3 bytes
    a precision of 1 keeps 3 bytes / 2 code points
    a two-column cluster and a precision of 1 keeps 0 bytes
```

Two things are proved by that one cell:

1. The precision does **not** split a cluster. It keeps the combining
   mark along with its base, because the rule counts the cluster, not
   the code point.
2. A cluster that does not fit is dropped **whole**. U+1F600 is two
   columns wide, so a precision of 1 keeps **zero** bytes — it is not
   cut in half either.

That second point is the same shape as Nov 17's fill finding: the
argument is never cut mid-unit, whether the unit is a code point, a
cluster, or a fill.

## KEY FINDING #2 — ONE TABLE, TWO RULES

The chain's question: *does the arc's scanner need a second Unicode
table for the precision?* **No.** The precision's rule is stated in
field-width units, and the field-width table is the one table the arc
already has:

```
  ONE table, TWO rules:
    U+1F600 is two columns wide, so a budget of 1 keeps 0 bytes and a budget of 2 keeps 4
```

The width asks "how many columns is this argument?", the precision asks
"how many columns may this argument keep?" — the same table answers
both.

## KEY FINDING #3 — THE PRECISION TRUNCATES BEFORE THE WIDTH PADS

The precision applies to the **argument**; the width then pads what is
left. So the padding count is computed from the **truncated** prefix,
not from the whole argument:

```
  the precision truncates BEFORE the width pads:
    "{:>8.2}" on "abc" + U+1F600 -> 8 bytes, 8 columns
```

`"abc"` plus U+1F600 is five columns wide. The precision of 2 keeps
`"ab"`, which is two columns, and the width of 8 pads that to eight
columns — six fill characters and two argument bytes.

## KEY FINDING #4 — THE ESCAPED TYPE'S PRECISION COUNTS THE ESCAPED OUTPUT

The `?` type copies the **escaped** string to the output, so its
precision is measured against the escaped text:

```
  the ESCAPED type's precision counts the columns of the ESCAPED output:
    "{:.3?}" on a"b -> 3 bytes
```

The escaped form of `a"b` is `"a\"b"`. A precision of 3 keeps the first
three columns — `"`, `a`, and the **backslash** — so the cut lands in
the middle of the escape sequence and the output is a truncated escape.
Both implementations agree, and both do it.

## THE DEFECT — UAX #29's GB11

Today's battery contains three cells that Nov 17's battery did not, and
they are the ones where UAX #29's **GB11** rule decides the answer:

> `Extended_Pictographic Extend* ZWJ` × `Extended_Pictographic`

The arc's Nov 17 rule said: *the code point after a ZWJ joins the
cluster*. It joined **unconditionally**, which is wrong in both
directions:

```
  THE DEFECT — UAX #29's GB11:
    'a' ZWJ U+1F600: the stdlib charges 3 columns
      the arc's rule (Nov 17) says 1
      GB11 says 3
    'man' ZWJ 'a': the stdlib charges 3 columns, GB11 says 3, the arc's rule says 2
```

- `'a'` ZWJ U+1F600: the `'a'` is **not** pictographic, so GB11 does
  not fire and the emoji starts a **new** cluster — 1 + 2 = **3**
  columns. The arc's rule said 1.
- `man` ZWJ `'a'`: the `'a'` after the ZWJ is **not** pictographic, so
  it does not join the man — 2 + 1 = **3** columns. The arc's rule said
  2.

The reason the defect survived Nov 17 is the same reason Nov 13's euro
hid the width's unit: **every ZWJ cell in Nov 17's battery was
pictographic-to-pictographic**, where the unconditional rule and GB11
happen to agree. The rule was only ever *measured* on the cells where
it is right.

Measured over today's battery:

```
  the linked stdlib ("libc++ 21"): the GB11 model agrees on 14 of 14 cells
    the arc's Nov 17 rule agrees on 12 of 14 cells
```

The model in today's source implements GB11 (via a restricted
`Extended_Pictographic` table, in the same restricted style as
Nov 17's width table), and the model is measured against the linked
stdlib on every cell.

## WHAT DID NOT CHANGE

`petra::FormatError` / `petra::FormatErrorKind` (the four kinds, their
order and their meanings, exactly as since Sep 6), the two-pass
(measure, then write) design, the `counting_output_iterator`, the
`bounded_char_writer`, the `TooLarge` contract, the buffer-atomicity
behaviour, the type-category table from Oct 30, Nov 7's mask tables and
its derived override table, its 1,596-cell grid, Nov 13's fill rule and
its fill census, Nov 14's pair grid and the align/zero rule, Nov 15's
triple grid and the recovery path, Nov 16's fill behaviour space, and
Nov 17's width census — whose three numbers (**1**, **11**, **28**) are
re-measured in Section 15 of this run and do not move.

## The source

`P-2026-11-18-precision.cpp` carries the whole arc forward verbatim
from Nov 17 and adds Section 16, which contains:

- `kExtendedPictographic` — the restricted Extended_Pictographic table
  GB11 needs;
- `is_extended_pictographic()` — the membership test;
- `Cluster` — one cluster's byte length and column width;
- `split_clusters()` — the UAX #29 cluster splitter (GB9 + GB11);
- `count_columns_gb11()` — the width of a string in columns;
- `prefix_bytes_for_precision()` — the standard's rule in the arc's own
  words: the byte length of the longest prefix whose field width fits
  the budget, cut on a cluster boundary;
- `section16()` — the seven measurements above.

## Build and run

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-18-precision
```

Strict warnings: `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`, C++23, no compiler extensions.

## Results

| build | stdlib | result |
|---|---|---|
| Apple Clang 21, strict | libc++ 21 | pass=171 fail=0 |
| GCC 15, strict | libstdc++ 15 | pass=170 fail=0 |
| CMake + ASan/UBSan | libc++ 21 | pass=171 fail=0, no sanitizer report |

(The one-check difference between the two toolchains is pre-existing: it
is Nov 17's recovery-substitution count, which libstdc++ runs and
libc++ does not.)

## Where we go next

1. **The GB11 fix belongs in the arc's OWN width rule, not beside it.**
   Today the GB11 model lives in `count_columns_gb11()` while the arc's
   Nov 17 rule still lives in `count_field_width()` — and the old rule
   is now known to be wrong on two cells of today's battery. The two
   should be ONE function, with the old rule deleted, and the whole
   28-cell width battery re-measured afterwards. Worth doing.
2. **The restricted tables are still unbounded.** Nov 17's
   `kWidth2Ranges` and today's `kExtendedPictographic` are both
   restricted to what the battery needs. libc++ 21 ships the full
   tables (`__format/width_estimation_table.h` has 275 lines and
   `__format/extended_grapheme_cluster_table.h` has 1,669). Worth
   measuring how far the restricted tables can be pushed before they
   need to be the real ones.
3. **The width's RESTRICTED table is still unmeasured on the precision's
   cells.** Nov 17's chain asked how large the unmeasured part of the
   space is; today's battery adds 14 more arguments and shows the
   restricted tables still agree with the stdlib on every cell of both
   batteries. Still open: the FULL table's size, and whether the
   restricted one can ever agree on a battery chosen to break it.
4. **The chain item (4) — the arc's cap is in BYTES and both the width
   and the precision are in COLUMNS.** Today made this concrete: a
   precision of 2 can yield 0 bytes or 6 bytes or 11 bytes depending on
   the argument, so the arc's two-pass design has to agree with the
   stdlib on a conversion between the two units that is not a constant
   factor. Worth measuring `try_format_runtime_n` against the stdlib on
   a battery of wide arguments with a precision.
5. **The chain item (5) — the compile-time path could use the arc's
   own validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. Still open.
6. **The chain item (6) — the policy table is the natural place to put
   the *value* rules.** A value-dependent rule has to live somewhere
   else again. Still open.
7. **The chain item (7) — the dead bits are a property of the
   GRAMMAR.** Nov 16 measured that overlong, surrogate and
   above-U+10FFFF can never be recovered into a valid spec. Worth
   asking whether the fault mask should be reduced to its two live
   bits.
8. **NEW — the escaped type's precision is a separate rule, and it is
   NOT the same rule.** The `?` type's precision counts the columns of
   the *escaped* output, and the cut can land inside an escape
   sequence. The arc's scanner does not model the escaped output at
   all. Worth asking whether the escaped type needs its own column
   model, or whether the escape sequences' own widths make the rule
   unreachable in practice.
