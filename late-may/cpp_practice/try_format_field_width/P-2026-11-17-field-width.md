# P-2026-11-17 — try_format_field_width

## What today closes

Nov 16's "Where we go next" items (2) and (5), verbatim:

> **The chain item (3) — the width counts code points.** Still open,
> and now sharper: the width counts code points of the *argument*, and
> the fill is repeated to fill it. If the recovered fill is more than
> one code point (item 2), the fill's *width* is more than one column
> per repetition, and the arc's two-pass design has to agree with the
> stdlib on both units.

> **The chain item (6) — the `fill` is not the only place a Unicode
> scalar value can appear.** The width and precision are ASCII and the
> type char is ASCII, so the fill was the only place the scanner had to
> decode — but the *argument* is also a Unicode string, and the
> standard counts the width of a string argument in code points too.
> Worth measuring whether the two rules want the same table.

Both are closed, and the second one closed with a **correction** to the
arc's own Nov 13 finding. The premise of both items was "the standard
counts the width of a string argument in code points". It does not.
It counts **COLUMNS**.

And Nov 13's "KEY FINDING #4 — THE WIDTH COUNTS CODE POINTS, NOT
BYTES" — which the arc has carried forward ever since — was **right
for its battery and wrong as a rule**. Its battery was three euros.
The euro is one column wide, so "code points" and "columns" said the
same thing and the battery could not tell them apart.

The euro is what hid the rule.

## THE CORRECTION — the width's unit

[format.string.std]/12 (C++26's wording; C++23's /13 says the same
thing under a different paragraph number):

> For a sequence of characters in UTF-8, UTF-16, or UTF-32, an
> implementation should use as its field width the sum of the field
> widths of the first code point of each extended grapheme cluster.
> Extended grapheme clusters are defined by UAX #29 of the Unicode
> Standard. The following code points have a field width of 2:
>
> - (13.1) any code point with the East_Asian_Width="W" or
>   East_Asian_Width="F" property as described by UAX #44
> - (13.2) U+4DC0 – U+4DFF (Yijing Hexagram Symbols)
> - (13.3) U+1F300 – U+1F5FF (Miscellaneous Symbols and Pictographs)
> - (13.4) U+1F900 – U+1F9FF (Supplemental Symbols and Pictographs)
>
> The field width of all other code points is 1.

Three things in that paragraph, each of which the arc had wrong or
missing:

1. **The unit is the COLUMN, not the code point.** A code point is
   worth one column or two.
2. **The unit is the CLUSTER, not the code point.** The width is the
   field width of the cluster's *first* code point; the cluster's other
   code points are free.
3. **The table is a different table.** The fill's table is the Unicode
   scalar-value table (which code points decode). The width's table is
   the East_Asian_Width table (which code points are wide) plus three
   explicit override ranges. They are not the same table, so item (5)'s
   question — "do the two rules want the same table?" — has the answer
   **no**.

## THE MEASUREMENT — a census of three candidate units

One battery of 28 arguments, three candidate units, one oracle: the
width the **linked stdlib actually charges**, measured by letting it
pad the argument to a known width and counting the fill characters it
wrote.

```
    argument                     bytes   cps  cols  model stdlib
    ascii 'abc'                      3     3     3      3      3
    U+00E9 e-acute (N)               2     1     1      1      1
    U+20AC euro (N)                  3     1     1      1      1
    U+1F600 grinning (W)             4     1     2      2      2
    U+1F600 + FE0F                   7     2     2      2      2
    U+1F600 + FE0F + FE0F           10     3     2      2      2
    U+1F600+FE0F+U+1F600            11     3     4      4      4
    U+1F600 + FE0E (VS15)            7     2     2      2      2
    U+1F600 + ZWJ + U+1F600         11     3     2      2      2
    U+20AC + FE0F                    6     2     1      1      1
    U+4E00 + FE0F                    6     2     2      2      2
    'a' + FE0F                       4     2     1      1      1
    FE0F alone                       3     1     1      1      1
    U+4E00 CJK (W)                   3     1     2      2      2
    U+FF21 fullwidth A (F)           3     1     2      2      2
    U+FFFD replacement (A)           3     1     1      1      1
    U+4DC1 Yijing (/12)              3     1     2      2      2
    U+0301 combining acute           2     1     1      1      1
    'e' + U+0301                     3     2     1      1      1
    'a' + 2 combining marks          5     3     1      1      1
    family ZWJ (7 cp)               25     7     2      2      2
    two CJK                          6     2     4      4      4
    U+1F300 (/12)                    4     1     2      2      2
    U+1F900 (/12)                    4     1     2      2      2
    U+2764 heart (N)                 3     1     1      1      1
    U+2764 + FE0F                    6     2     1      1      1
    U+1F441 eye (W)                  4     1     2      2      2
    U+1F441 + FE0F                   7     2     2      2      2

    the units, over 28 cells:
      bytes       agree on  1 of 28 cells
      code points agree on 11 of 28 cells
      COLUMNS     agree on 28 of 28 cells
```

**Bytes agree on one cell. Code points agree on eleven. Columns agree
on all twenty-eight.**

The eleven code-point agreements are exactly the cells where every
code point is one column wide — the ASCII and narrow rows. The
seventeen disagreements are the wide rows, the combining-mark rows and
the ZWJ rows.

## KEY FINDING #1 — the wide code point is ONE code point and TWO columns

U+1F600 is four bytes and **one code point**, and the stdlib charges it
**two columns**. So does U+4E00 (a CJK ideograph, three bytes) and
U+FF21 (a fullwidth A, three bytes). The byte count and the code-point
count are both wrong; the column count is right.

The three ranges the standard names explicitly are measurable, and the
arc's table carries them:

- U+4DC0–U+4DFF (Yijing hexagrams) → two columns;
- U+1F300–U+1F5FF (Miscellaneous Symbols and Pictographs) → two;
- U+1F900–U+1F9FF (Supplemental Symbols and Pictographs) → two.

And U+2764 (a heavy black heart) is **one** column, even though it is
an emoji and even though it is adjacent to those ranges. EAW="N" is
EAW="N". The standard's rule is not "emoji are wide".

## KEY FINDING #2 — the unit is the GRAPHEME CLUSTER

This is the part that is not about wide characters at all, and it is
the part that makes the unit a *cluster* rather than a code point.

```
    'e' + U+0301                     3 bytes   2 cps   1 column
    'a' + 2 combining marks          5 bytes   3 cps   1 column
    family ZWJ (7 cp)               25 bytes   7 cps   2 columns
    U+1F600 + ZWJ + U+1F600         11 bytes   3 cps   2 columns
```

A combining acute accent joins the letter it follows, so "é" written
as `e` + U+0301 is **one** column, not two. A seven-code-point family
emoji is **two** columns, not seven. The width is the field width of
the cluster's **first** code point; everything after it in the cluster
is free.

The stdlib implements exactly this. libc++ 21 does it by walking the
extended grapheme cluster table and looking up the *first* code point
of each cluster in its width-estimation table
(`__format/parser_std_format_spec.h`,
`__estimate_column_width_grapheme_clustering`). libstdc++ 15 agrees
on every cell of the battery.

## KEY FINDING #3 — the FILL is assumed to be ONE column wide

[format.string.std]'s note says so:

> Since fill characters are assumed to have a field width of 1, use of
> a character with a different field width can produce misaligned
> output. The 🤡 (U+1f921 clown face) character has a field width of 2.

Measured (width 10, argument `"abc"`, which is three columns):

```
    ascii '*'      7 repetitions x 1 column(s) each -> the field is 10 columns wide (the width asked for was 10)
    U+1F600 (W)    7 repetitions x 2 column(s) each -> the field is 17 columns wide (the width asked for was 10)
    U+FF21 (F)     7 repetitions x 2 column(s) each -> the field is 17 columns wide (the width asked for was 10)
```

**The padding count does not see the fill's width.** All three fills
are repeated **seven** times, because the padding count is
`width − estimated_width(argument)` = 10 − 3 = 7, and the fill's own
width never enters it.

So a two-column fill pads the field **wider** than the width asked
for: 17 columns in a 10-column field. This is specified behaviour, not
a defect — the standard says the fill is *assumed* to be one column
wide. But it is the second half of Nov 16's chain item (3): the fill's
width and the argument's width are measured in the same unit, and the
fill's is *assumed* rather than computed.

Note also that this is a **misalignment**, not a different unit: the
fill's width is not "counted in code points" or "counted in columns",
it is simply *assumed to be 1*.

## KEY FINDING #4 — the recovery's SUBSTITUTION COUNT

Nov 16's chain asked how many U+FFFDs a recovery substitutes. That
was not measurable in Nov 16, because the arc's model only ever
compared *verdicts* (accept or reject), and the substitution count
does not change a verdict — it changes the **output**.

Measured, by counting the U+FFFD characters in the output (width 10,
argument `"abc"`):

```
    invalid-lead \x80      ACCEPT: 7 U+FFFD over 7 repetitions -> 1 per repetition
    truncated \xE2\x82     ACCEPT: 7 U+FFFD over 7 repetitions -> 1 per repetition
```

**One U+FFFD per repetition, whatever the maximal subpart's length.**
The `\xE2\x82` subpart is *two bytes*, and the recovery still
substitutes one U+FFFD for it — not two. That is Unicode's
"U+FFFD Substitution of Maximal Subparts" rule (Unicode 15 §5.22),
and it is what libstdc++ 15 does.

And U+FFFD is **one column wide**, so the recovered fill's width is
one, and the recovered field is exactly ten columns wide. The
per-byte substitution that Nov 16's chain worried about does not
happen, and cannot: it would make the field 17 columns wide.

libc++ 21 makes every ill-formed fill fatal, so there is no
substitution to count there.

## What changed in the code

- A new datum: `count_field_width()` — the field width of a string in
  COLUMNS, with a restricted `kWidth2Ranges` table (the EAW W/F ranges
  the battery needs, plus the standard's three explicit override
  bands) and a restricted `is_cluster_extend()` set.
- A new measurement: `charged_width()` — the width the **linked
  stdlib** charges, measured by counting the fill characters it wrote.
  This is the arc's usual "measure, don't assert" move: the model is
  tested against the implementation, not against a claim.
- Section 15, with four parts: the census, the fill's one-column
  assumption, the recovery's substitution count, and the linked-stdlib
  agreement over the whole battery.

## What did NOT change

Re-measured, and the output is **byte-identical** to Nov 16's for
Sections 1–14 under libc++ 21:

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order and their meanings, exactly as since Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the TooLarge
  contract, the buffer-atomicity behaviour.
- The type-category table from Oct 30, Nov 7's mask tables, its derived
  override table, and its 1,596-cell grid.
- Nov 13's fill rule and its fill census; Nov 14's pair grid and the
  align/zero rule; Nov 15's triple grid and the recovery path; Nov
  16's fill behaviour space and its 13-class partition.

The diff was run against Nov 16's own binary, compiled from
`origin/main`, and Sections 1–14 match exactly.

Note that Nov 13's Section 9.4 — "the width counts code points, not
bytes" — is still **there**, still passes, and is still *arithmetically
true* for the euro battery it used. It is simply no longer the rule.
Today does not rewrite it: the arc's discipline is that a carried
forward measurement stays as it was, and a correction is a new
measurement beside it.

## Build and run

The same source is built and measured under **both** toolchains, with
no source change between them:

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-17-field-width
```

## Verification

- Compiled with `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` — clean, no warnings, under both
  clang++ 21 (libc++ 21) and g++ 15 (libstdc++ 15).
- The program's own checks: **149 pass, 0 fail** under libc++ 21, and
  **148 pass, 0 fail** under libstdc++ 15 — the same source, the same
  assertions, one check fewer because the recovery's substitution
  count is libstdc++-specific (libc++ 21 never recovers).
- The 28-cell battery was scored three ways against the linked
  stdlib's own charged width: bytes agree on 1, code points on 11,
  columns on 28.
- The fill's padding count was measured on three fills (ASCII, a
  wide emoji, a fullwidth letter) and is 7 for all three; the field's
  real width was measured for each (10, 17, 17).
- The recovery's substitution count was measured by counting U+FFFD
  characters in the output, for a one-byte and a two-byte maximal
  subpart: one per repetition in both cases.
- Sections 1–14's output was diffed against Nov 16's own binary and is
  byte-identical.
- An ASan/UBSan build (`-fsanitize=address -fsanitize=undefined`) runs
  the same checks with no sanitizer report.

## Where we go next

1. **The width's table is RESTRICTED, and the restriction is visible
   but not yet bounded.** The arc's `kWidth2Ranges` carries the EAW
   W/F ranges the battery needs plus the standard's three explicit
   bands; libc++ 21's own table has 110 entries. Worth measuring how
   large the unmeasured part of the space is — and whether a *restricted*
   table can be made to agree with the stdlib on a much wider battery,
   or whether it needs the whole table.
2. **The cluster rules are RESTRICTED too.** `is_cluster_extend()`
   covers eight ranges and the ZWJ rule is simplified to "the code
   point after a ZWJ joins". UAX #29 has eleven rules. Worth asking
   whether the full grapheme-cluster algorithm is needed, or whether
   the *width* only ever depends on the first code point of a cluster
   — in which case a much simpler rule might do.
3. **The PRECISION has the same unit.** [format.string.std]/15: for
   string types the precision "specifies the longest prefix of the
   formatted argument to be included in the replacement field such that
   the field width of the prefix is no greater than the value of this
   option." So precision is measured in columns too — the same table,
   the same unit. Worth measuring, and worth asking whether the arc's
   scanner needs a second Unicode table for it.
4. **The arc's own two-pass design has to agree on both units.**
   Nov 16's chain said it plainly: the arc's cap is in BYTES and the
   width is in COLUMNS. That was true when the width was in code
   points; it is still true now, but the *number* changes — a
   two-column argument is fewer bytes than a one-column argument of
   the same width. Worth measuring the arc's `try_format_runtime_n`
   against the stdlib on wide arguments.
5. **The chain item (4) — the compile-time path could use the arc's
   own validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. Still open.
6. **The chain item (5) — the policy table is the natural place to
   put the *value* rules.** A value-dependent rule (does 65 fit in
   `char`?) has to live somewhere else again — probably a third mask,
   keyed by (category, type), selected only after the value is known.
   Still open.
7. **The chain item (6) — the dead bits are a property of the
   GRAMMAR, so they belong to the standard, not to the
   implementation.** Nov 16 measured that overlong, surrogate and
   above-U+10FFFF can never be recovered into a valid spec *because
   the alignment option cannot sit where the scanner looks for it*.
   Worth asking whether the fault mask should be reduced to its two
   live bits — and whether the *implementation's* mask is then not a
   mask at all but a single bit.
