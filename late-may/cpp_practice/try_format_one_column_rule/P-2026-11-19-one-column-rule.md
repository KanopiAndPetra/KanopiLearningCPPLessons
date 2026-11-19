# P-2026-11-19 — try_format_one_column_rule

## What today closes

Nov 18's "Where we go next" item (1), verbatim:

> **The GB11 fix belongs in the arc's OWN width rule, not beside it.**
> Today the GB11 model lives in `count_columns_gb11()` while the arc's
> Nov 17 rule still lives in `count_field_width()` — and the old rule
> is now known to be wrong on two cells of today's battery. The two
> should be ONE function, with the old rule deleted, and the whole
> 28-cell width battery re-measured afterwards. Worth doing.

Closed, and the merge paid for itself twice over.

**Closed.** There is **ONE** column rule now: `count_columns()`. Nov 17's
rule — the one that joined the code point after a ZWJ unconditionally —
is **deleted**. The 28-cell width battery and the 56-cell precision
battery were both re-measured with the merged rule, and their numbers
did not move: **1, 11, 28** for the width, and **56 of 56** for the
precision.

**And the merge exposed five more missing rules.** While the two rules
sat side by side, the arc had no way to ask "what else does the cluster
model get wrong?" — one rule was the width's rule, the other was the
precision's, and the battery for each was already fully covered. With
both in one function, a single battery of 20 cells can be scored
against all three models at once, and the answer is that the arc was
missing **five** of UAX #29's cluster rules:

- **GB9a** — `× SpacingMark` — a spacing mark joins the *preceding*
  cluster.
- **GB9b** — `Prepend ×` — a prepend character joins the *following*
  cluster.
- **GB9c** — the Indic conjunct: consonant, linker (virama),
  consonant is **one** cluster.
- **GB12/GB13** — the regional indicators pair up, so a flag's two
  halves are **one** cluster.
- **GB6/GB7/GB8** — the Hangul syllable sequences: `L V T` composes
  into **one** cluster.

With all five added, the ONE rule agrees with the linked stdlib on
**every** cell of the boundary battery.

## THE MERGE — one function, one rule

Yesterday the arc carried two column-counting functions:

```cpp
// Nov 17's rule: the code point after a ZWJ joins, unconditionally.
[[nodiscard]] constexpr std::size_t count_field_width(std::string_view s) noexcept;

// Nov 18's model: UAX #29's GB9 + GB11.
[[nodiscard]] std::size_t count_columns_gb11(std::string_view s);
```

Two functions meant two tables of truth, and — worse — they disagreed.
Nov 17's rule said `'a' ZWJ U+1F600` is **one** column; Nov 18's model
said **three**; the stdlib said **three**. Yesterday the arc *measured*
the disagreement instead of *resolving* it, and left both functions in
the file.

Today there is one:

```cpp
[[nodiscard]] std::size_t count_columns(std::string_view s,
                                        unsigned rules = gb::all) {
    std::size_t columns = 0;
    for (const Cluster& cl : split_clusters(s, rules)) columns += cl.columns;
    return columns;
}
```

Every call site in the arc — the width census, the fill census, the
precision census, the defect check — now names `count_columns()`. There
is no second opinion left to take.

### The rules mask

The mask (`gb::gb9a`, `gb::gb9b`, `gb::gb9c`, `gb::gb11`, `gb::gb12`,
`gb::gb68`) exists for one reason: so the **deleted** rule can still be
*measured* without being *run*. Turning the `gb11` bit off reproduces
Nov 17's defective rule exactly — the code point after a ZWJ joins
unconditionally — which is what model **A** in the battery below is.
The arc's own rule is `gb::all`; the mask is a measuring instrument,
not a configuration knob.

## THE BOUNDARY BATTERY — three models, one oracle

One battery of 20 cells, three models, one oracle: **the width the
linked stdlib actually charges**, measured by padding the argument to a
known width and counting the fill characters written.

```
  the boundary battery -- three models, one oracle (libc++ 21):
    argument                     stdlib  A(old) B(GB11)  C(ONE)
    ascii 'abc'                       3       3       3       3
    U+1F600 grinning (W)              2       2       2       2
    RI pair (GB12)                    1       2       2       1
    RI four (GB12)                    2       4       4       2
    'a' + RI                          2       2       2       2
    U+0600 prepend + 'a' (GB9b)       1       2       2       1
    ka virama ssa (GB9c)              1       2       2       1
    ka virama                         1       1       1       1
    ka virama + 'a'                   2       2       2       2
    'a' ZWJ U+1F600 (GB11)            3       1       3       3
    'man' ZWJ 'a' (GB11)              3       2       3       3
    EP ZWJ EP                         2       2       2       2
    family ZWJ (7 cp)                 2       2       2       2
    'e' + U+0301 (GB9)                1       1       1       1
    thumb + skin tone (GB9)           2       2       2       2
    keycap 1 FE0F 20E3 (GB9)          1       1       1       1
    hangul syllable U+AC00            2       2       2       2
    hangul jamo L V T (GB6/7/8)       2       4       4       2
    ka + U+0903 (GB9a)                1       2       2       1
    ka U+0903 + virama ssa            2       3       3       2

    the models, over 20 cells:
      A  Nov 17's DELETED rule agree on 11 of 20 cells
      B  Nov 18's GB11 model    agree on 13 of 20 cells
      C  today's ONE rule      agree on 20 of 20 cells
```

The three models are:

- **A** — Nov 17's deleted rule: GB9 only, with the ZWJ joining
  unconditionally.
- **B** — Nov 18's model: GB9 + GB11.
- **C** — today's ONE rule: GB6/7/8 + GB9 + GB9a + GB9b + GB9c +
  GB11 + GB12/GB13.

The oracle is measured, not asserted: `std::vformat` pads the argument
to 40 columns with a fill character that cannot occur in the argument,
and the number of fill characters written is subtracted.

The same battery, scored against **libstdc++ 15** instead, gives the
same three numbers — 11, 13, 20 — with no source change.

## KEY FINDING #1 — THE DELETED RULE WAS WRONG ON NINE CELLS, NOT TWO

Yesterday's battery found two cells where the old rule was wrong. The
wider battery finds **nine**:

| cell | stdlib | A (deleted) | rule |
|---|---|---|---|
| `'a' ZWJ U+1F600` | 3 | 1 | GB11: the emoji after a ZWJ joins only after a *pictographic* ZWJ |
| `'man' ZWJ 'a'` | 3 | 2 | GB11: a non-pictographic after a ZWJ does **not** join |
| RI pair | 1 | 2 | GB12/GB13: the two halves of a flag are one cluster |
| RI four | 2 | 4 | GB12/GB13: the run pairs up, two clusters, not four |
| `U+0600` + `'a'` | 1 | 2 | GB9b: the prepend joins the following cluster |
| ka + virama + ssa | 1 | 2 | GB9c: the Indic conjunct is one cluster |
| hangul jamo L V T | 2 | 4 | GB6/7/8: the Hangul syllable is one cluster |
| ka + U+0903 | 1 | 2 | GB9a: the spacing mark joins the preceding cluster |
| ka + U+0903 + virama + ssa | 2 | 3 | GB9a + GB9c: the spacing mark **breaks** the conjunct |

The first two are the ones Nov 18 found. The other seven are new, and
they are new **because the merge made them askable**.

Two of them are worth naming closely, because they are the same rule
seen from two sides:

- **`ka` + virama + `ssa`** (GB9c): the deleted rule charged **two**
  columns — one for the consonant, one for the consonant after the
  virama — because it treated the virama as a standalone cluster. The
  stdlib charges **one**. The virama does not make a cluster of its
  own; it *links* two clusters into one.
- **`ka` + U+0903 + virama + `ssa`** (GB9a + GB9c): the stdlib charges
  **two**, and so does today's rule — but for a subtle reason. The
  spacing mark joins the preceding cluster (GB9a), *and* it breaks the
  conjunct's left context, so the final `ssa` does **not** join. The
  cell is the one place where GB9a and GB9c **interact**, and it is
  why the arc's `split_clusters` resets `cb_consonant` when a spacing
  mark arrives.

## KEY FINDING #2 — EACH RULE IS WORTH EXACTLY WHAT IT PAYS

Turning each rule off, one at a time, and re-scoring the battery:

```
  what each rule is worth (the rule turned OFF):
    GB9a  x SpacingMark                    without it: 18 of 20 cells
    GB9b  Prepend x                        without it: 19 of 20 cells
    GB9c  the Indic conjunct               without it: 19 of 20 cells
    GB12  the regional-indicator pairs     without it: 18 of 20 cells
    GB68  the Hangul syllable sequences    without it: 19 of 20 cells
```

GB9a and GB12 are worth **two** cells each; GB9b, GB9c and GB68 are
worth one each.

This is the check that a rule is real rather than decorative. A rule
that were worth nothing could be dropped with no change in the
measurement — and the measurement says each of the five changes the
answer somewhere.

Note the shape of the two-celI rules: GB9a is worth two because it
both *joins* a spacing mark and *breaks* the conjunct after it, and
GB12 is worth two because it governs a *run* — four regional indicators
are two clusters, not four.

## KEY FINDING #3 — A CLUSTER IS WORTH THE WIDTH OF ITS FIRST CODE POINT

That phrase is the arc's, from Nov 17, and it is doing more work than it
looks. Every one of the cluster rules is a rule about **where a cluster
ends**, and the *width* of a cluster never changes — only its
*boundaries* do.

So the regional-indicator pair is **one** column wide, because its
first code point (U+1F1FA) is one column wide. Four regional indicators
are two clusters, so two columns. The Indic conjunct is one column
wide, because its first code point (the consonant) is one column wide.
The prepend is one column wide, because the *prepend* is the first code
point of its cluster — and U+0600 is a narrow character.

Contrast with the emoji, whose first code point is two columns wide:
U+1F600 is two columns, and every cluster that starts with it is two
columns wide, however many code points follow. The Hangul jamo
sequence `L V T` is two columns wide, because U+1100 (the `L`) is a
wide character — the *composition* is what makes it one cluster, and
the *width* comes from the first code point.

That is why a cluster's width is not additive over its members: `e` plus
U+0301 is one column, not two, and U+1F600 plus FE0F plus U+1F600 is
four columns, not five.

## THE TABLES ARE STILL RESTRICTED

Every table the ONE rule carries is restricted to what the batteries
need — the arc's standing practice, and the reason the restriction is
*visible* rather than assumed:

```
  the ONE rule's tables (all RESTRICTED):
    kWidth2Ranges            54 ranges
    kExtendedPictographic    32 ranges
    kPrepend                 15 ranges
    kIndicLinker             13 ranges
    kIndicConsonant          30 ranges
```

libc++ 21 ships the real ones: `__format/width_estimation_table.h` has
275 lines and `__format/extended_grapheme_cluster_table.h` has 1,669.
So the arc's restricted tables are roughly a fifth the size of libc++'s
cluster table and a fifth of its width table, and the arc's model still
agrees with libc++ on every cell of all three batteries.

## WHAT DID NOT CHANGE

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order and their meanings, exactly as since Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the `TooLarge`
  contract, the buffer-atomicity behaviour.
- The type-category table (Oct 30), Nov 7's mask tables and its derived
  override table and its 1,596-cell grid, Nov 13's fill rule and its
  fill census, Nov 14's pair grid and the align/zero rule, Nov 15's
  triple grid and the recovery path, Nov 16's fill behaviour space.
- Nov 17's width census — its three numbers (**1**, **11**, **28**) are
  re-measured here and did not move.
- Nov 18's precision census — **56 of 56** for the column rule, re-
  measured here.

## THE CODE

The merged rule, in one function and one loop:

```cpp
[[nodiscard]] std::vector<Cluster> split_clusters(std::string_view s,
                                                  unsigned rules) {
    std::vector<Cluster> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const std::size_t start = i;
        const Utf8CodePoint first = decode_utf8(s.substr(i));
        if (first.length == 0) break;
        i += first.length;
        const std::size_t columns = is_width_two(first.value) ? 2u : 1u;

        // The cluster's state, for the rules that look BACK.
        bool ep_run = is_extended_pictographic(first.value);
        bool last_was_zwj = false;
        bool ep_before_zwj = false;
        bool prepend_pending = ((rules & gb::gb9b) != 0u) &&
                              is_prepend(first.value);
        bool cb_consonant = ((rules & gb::gb9c) != 0u) &&
                            is_indic_consonant(first.value);
        bool cb_linker = false;
        HangulClass hangul_prev = ((rules & gb::gb68) != 0u)
                                       ? hangul_class(first.value)
                                       : HangulClass::None;
        std::size_t ri_run = is_regional_indicator(first.value) ? 1u : 0u;

        while (i < s.size()) {
            const Utf8CodePoint cp = decode_utf8(s.substr(i));
            if (cp.length == 0) break;

            // GB9c's LINKER: a virama joins the cluster, and marks the
            // cluster as "a consonant with a linker in it".
            if ((rules & gb::gb9c) != 0u && is_indic_linker(cp.value)) {
                cb_linker = true;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB9: an Extend joins the preceding cluster.
            if (is_cluster_extend(cp.value)) { /* ... */ }
            // GB9a: a SpacingMark joins the preceding cluster -- and it
            // BREAKS the conjunct's left context.
            if ((rules & gb::gb9a) != 0u && is_spacing_mark(cp.value)) {
                cb_consonant = false;
                cb_linker = false;
                /* ... */
            }
            // GB9: a ZWJ joins the preceding cluster.
            if (cp.value == kZwj) { /* ... */ }
            // GB11: the code point after a ZWJ.
            if (last_was_zwj) {
                if ((rules & gb::gb11) != 0u &&
                    !(ep_before_zwj && is_extended_pictographic(cp.value))) {
                    break;   // GB11's condition fails: the cluster ends
                }
                /* ... */
            }
            // GB6/GB7/GB8: the Hangul syllable sequences.
            if ((rules & gb::gb68) != 0u) {
                const HangulClass cur = hangul_class(cp.value);
                if (cur != HangulClass::None) {
                    if (!hangul_joins(hangul_prev, cur)) break;
                    hangul_prev = cur;
                    /* ... */
                }
            }
            // GB9b: a Prepend joins the FOLLOWING cluster.
            if (prepend_pending) { /* ... */ }
            // GB9c: the consonant after a linker.
            if ((rules & gb::gb9c) != 0u && cb_consonant && cb_linker &&
                is_indic_consonant(cp.value)) { /* ... */ }
            // GB12/GB13: the regional indicators pair up, so the
            // boundary between two of them depends on the parity of the
            // run: an RI joins the preceding one when the run so far is
            // ODD, and starts a new cluster when it is EVEN.
            if ((rules & gb::gb12) != 0u && is_regional_indicator(cp.value)) {
                if (ri_run % 2u == 1u) { ++ri_run; /* ... */ continue; }
                break;   // the run's parity says: a NEW cluster
            }
            break;
        }
        out.push_back(Cluster{i - start, columns});
    }
    return out;
}
```

Three details worth naming:

**The GB11 branch is where the deleted rule lived.** Yesterday's
`count_field_width` had one line — `prev_was_zwj` made the code point
after a ZWJ join, always. Today's version asks a *question* instead:
did a pictographic precede the ZWJ (`ep_before_zwj`), and is the code
point after it pictographic? Only then does the cluster continue.

**The `gb9c` branch needs two pieces of state, not one.** `cb_linker`
says "a virama has been seen inside this cluster" and `cb_consonant`
says "this cluster started with a consonant". GB9c needs *both*: the
consonant must come first, and there must be a linker between them. A
virama on its own is not a conjunct — `ka virama` is one column, and
`ka virama + 'a'` is two, because the trailing `'a'` is not a
consonant and so does not join.

**GB12's rule is about parity, not adjacency.** "The next RI joins"
is the wrong statement; the right one is "the next RI joins when the
run so far is odd". Four regional indicators are two clusters, not one
and not four, and the parity is what says so.

### The new tables

```cpp
constexpr WidthRange kPrepend[] = {
    {0x0600u, 0x0605u},   // Arabic number signs
    {0x06DDu, 0x06DDu},   // Arabic end of ayah
    ...
};

[[nodiscard]] constexpr bool is_regional_indicator(char32_t cp) noexcept {
    return cp >= 0x1F1E6u && cp <= 0x1F1FFu;
}

constexpr WidthRange kIndicLinker[] = {
    {0x094Du, 0x094Du},   // Devanagari virama
    {0x09CDu, 0x09CDu},   // Bengali virama
    ...
};
```

The viramas are also added to the arc's `is_cluster_extend` set: in
Unicode they *are* Extend code points, so GB9 already joins them to
the preceding consonant. GB9c then joins the consonant that follows.

The Hangul classes are computed, not tabulated — a Hangul syllable
`U+AC00..U+D7A3` is **LV** when `(cp - 0xAC00) % 28 == 0` and **LVT**
otherwise, which is the standard's own 28-jamo layout.

## HOW IT WAS BUILT AND CHECKED

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
cmake --build build
./build/P-2026-11-19-one-column-rule
```

- **Compiler:** Apple Clang 21.0.0, `-std=c++23`,
  `-fexperimental-library` (libc++ only).
- **Strict warnings:** `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion` — **clean, no warnings**.
- **Linked standard library:** libc++ 21 (`_LIBCPP_VERSION` 220106).
- **Result:** `pass=180  fail=0`.
- **ASan + UBSan** (`-fsanitize=address -fsanitize=undefined`): builds
  clean, runs clean, `pass=180  fail=0`, no sanitizer report.
- **Second standard library:** rebuilt against **libstdc++ 15**
  (Homebrew GCC 15), no source change: `pass=179  fail=0` (one fewer
  check — libc++ 21's fatal-malformed-fill behaviour has no libstdc++
  counterpart), and the boundary battery gives the **same** 11 / 13 / 20.

## Where we go next

1. **The restricted tables are still unbounded.** Nov 17's
   `kWidth2Ranges` and Nov 18's `kExtendedPictographic` are both
   restricted to what the battery needs, and today's three new tables
   are restricted the same way. libc++ 21 ships the full tables
   (`__format/width_estimation_table.h` has 275 lines and
   `__format/extended_grapheme_cluster_table.h` has 1,669). Worth
   measuring how far the restricted tables can be pushed before they
   need to be the real ones — the arc's 54 + 32 + 15 + 13 + 30 ranges
   agree with libc++ on every cell of every battery, but no battery has
   been chosen to *break* them.
2. **The chain item (3) — the width's RESTRICTED table is still
   unmeasured on the precision's cells.** Nov 17's chain asked how
   large the unmeasured part of the space is; Nov 18 added 14 more
   arguments and today's 20 more, and the restricted tables still agree
   with the stdlib on every cell of all three batteries. Still open: the
   FULL table's size, and whether the restricted one can ever agree on
   a battery chosen to break it.
3. **The chain item (4) — the arc's cap is in BYTES and both the width
   and the precision are in COLUMNS.** A precision of 2 can yield 0
   bytes or 6 bytes or 11 bytes depending on the argument, so the arc's
   two-pass design has to agree with the stdlib on a conversion between
   the two units that is not a constant factor. Worth measuring
   `try_format_runtime_n` against the stdlib on a battery of wide
   arguments with a precision. Today's rules make this *worse*: a
   regional-indicator pair is 8 bytes and ONE column, and a Hangul jamo
   sequence is 9 bytes and TWO columns, so a precision of 1 keeps zero
   bytes in both cases.
4. **The chain item (5) — the compile-time path could use the arc's own
   validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. Still open.
5. **The chain item (6) — the policy table is the natural place to put
   the *value* rules.** A value-dependent rule has to live somewhere
   else again. Still open.
6. **The chain item (7) — the dead bits are a property of the
   GRAMMAR.** Nov 16 measured that overlong, surrogate and
   above-U+10FFFF can never be recovered into a valid spec. Worth
   asking whether the fault mask should be reduced to its two live
   bits.
7. **The chain item (8) — the escaped type's precision is a separate
   rule, and it is NOT the same rule.** The `?` type's precision counts
   the columns of the *escaped* output, and the cut can land inside an
   escape sequence. The arc's scanner does not model the escaped output
   at all. Worth asking whether the escaped type needs its own column
   model, or whether the escape sequences' own widths make the rule
   unreachable in practice.
8. **NEW — the battery is the instrument, and it is still small.** The
   merge is what found five rules; a *bigger* battery might find more.
   UAX #29's rules the arc still does not model at all: **GB1/GB2**
   (CR ÷ LF and the reverse — the stdlib charges two columns for
   `"\r\n"`, which is two clusters), **GB3/GB4/GB5** (Control, CR, LF
   always break), **GB10** (the emoji modifiers, which the arc's
   Extend table covers), and **GB999** (the default break). Worth
   building the battery that asks about them: the same "three models,
   one oracle" shape, one row per rule.
9. **NEW — the ONE rule's clusters are worth their FIRST code point's
   width, but the arc has never measured what "first" means for a
   cluster whose first code point is a prepend.** Today's `U+0600 + 'a'`
   cell shows the answer is the prepend's own width (one column), but
   the arc's `kPrepend` table is 15 ranges of *narrow* characters. A
   prepend that is two columns wide would make the cluster two columns
   wide — and no cell of today's battery tests that. Worth measuring.
10. **NEW — the GB9a/GB9c interaction was found by ACCIDENT, and that
    is worth repeating deliberately.** The cell `ka` + U+0903 + virama +
    `ssa` is the only one in today's battery where two rules meet and
    *disagree* — GB9a says the spacing mark joins the preceding
    cluster, GB9c says the conjunct should form, and the stdlib
    resolves it by breaking the conjunct's left context. Worth
    searching for the other places where two cluster rules can meet and
    pull in opposite directions: GB9a vs GB9c is one; GB9 vs GB9c and
    GB11 vs GB9c are others.
