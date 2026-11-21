# P-2026-11-21 — try_format_control_chars

## What today closes

Nov 20's "Where we go next" item (1), verbatim:

> **The control characters are the next collision battery.** GB3,
> GB4 and GB5 are unmodelled, and the four cells in 18.3 show the arc
> wrong in both directions. The interesting part is the same lesson
> again: GB4/GB5 sit *before* GB9 in UAX #29's list, so the `Extend`
> after a `NUL` must **not** join it. Worth building the battery that
> asks about them: one row per rule, and one row per *ordering* of
> GB3/GB4/GB5 against GB9.

Closed — one row per rule, and one row per *ordering* — and the
ordering is the lesson again.

**UAX #29's rules are an ORDERED list, and the first match decides.**
Nov 20 moved GB9c above GB11; today moves GB3/GB4/GB5 above GB9.

And the battery found a second thing, which is not in the arc at all:
the **oracle** — libc++ 21 — takes a shortcut that the standard's rules
do not describe, and the shortcut is visible in exactly the cells this
lesson is about.

---

### 1. THE CONTROL RULES — GB3, GB4 and GB5

UAX #29 gives three of its break rules to the *control* characters —
the code points whose grapheme-break property is `Control`, `CR` or `LF`:

| rule | meaning |
|---|---|
| **GB3** | `CR × LF` — do not break between a CR and an LF |
| **GB4** | `(Control | CR | LF) ÷` — break **after** a control |
| **GB5** | `÷ (Control | CR | LF)` — break **before** a control |

Nov 20's rule modelled none of them, and the four open cells in §18.3
showed it wrong in **both** directions:

```
ka CRLF ka        stdlib 3   the arc said 4   (GB3 unmodelled)
ka NUL U+0301 ka  stdlib 4   the arc said 3   (GB4/GB5 unmodelled)
```

### 2. THE ORDER — GB3/GB4/GB5 come BEFORE GB9

GB3, GB4 and GB5 are rules 3, 4 and 5. GB9 is rule 9. So GB4 wins
over GB9, and the arc's rule gets it backwards:

```
"ka" NUL U+0301 "ka"
    GB4   the NUL ends its cluster          -> NUL | U+0301
    GB9   the Extend joins the preceding ... -> NUL U+0301   (WRONG)
```

A control breaks on **both** sides: an `Extend` after a `NUL` starts a
**new** cluster, and a `Prepend` does not swallow a control that
follows it.

This is the same lesson Nov 20 taught, one rule list further down, and
it is why the battery has a row per *ordering* and not just a row per
rule: a rule that is right but consulted in the wrong place is still
wrong.

### 3. THE ASCII FAST PATH — the oracle is not the standard

libc++ 21 does not cluster an ASCII prefix at all. In
`__format/parser_std_format_spec.h`, `__estimate_column_width` walks
the string like this:

```cpp
auto __it = __str.begin();
if (__format_spec::__is_ascii(*__it)) {
    do {
      --__maximum;
      ++__it;
      if (__it == __str.end()) return {__str.size(), __str.end()};
      if (__maximum == 0) {
        if (__format_spec::__is_ascii(*__it))
          return {static_cast<size_t>(__it - __str.begin()), __it};
        break;
      }
    } while (__format_spec::__is_ascii(*__it));
    --__it;
    ++__maximum;
}
```

Read it slowly. While the string starts with ASCII code units, libc++
charges **one column per code unit** and never runs the cluster
algorithm over them. Only the *last* ASCII code unit before the first
non-ASCII one is handed back, because it might join what follows.

The shortcut assumes an ASCII code point never joins another. But GB3
says a CR and an LF **do** — so the linked stdlib charges a CR LF pair
**two** columns at the start of an ASCII string and **one** column in
the middle of a non-ASCII-leading one:

```
  argument         stdlib     pure      ONE
  CR LF                 2        1        2
  a CRLF a              4        3        4
  a CRLF CRLF a         6        4        6
  ka CRLF ka            3        3        3
```

The arc's ONE rule follows the **linked stdlib** — that has been the
arc's oracle since the first lesson — so today's rule models the fast
path too, and the difference between "the rules" and "the shortcut" is
printed rather than assumed.

And the fast path has a second consequence: it cuts at a **code
unit**, not at a cluster boundary. The stdlib's *precision* can
therefore cut a CR LF pair **in half**:

```
"a" CRLF "a" with precision 2 -> 2 byte(s) a \x0D
```

---

## THE CONTROL BATTERY — four models, one oracle

19 cells. Four models, one oracle: the width the linked stdlib charges.

* **A** — Nov 20's rule: no control rules, no fast path.
* **B** — the control rules, but consulted *after* GB9 (the wrong order).
* **C** — the control rules in UAX #29's order, no fast path.
* **D** — today's ONE rule: the order **and** the fast path.

```
  the control battery -- four models, one oracle (libc++ 21):
    argument               stdlib  A(old) B(late)  C(ord)  D(ONE)
    ka CRLF ka                  3       4       3       3       3
    ka CRLF                     2       3       2       2       2
    ka CRLF CRLF ka             4       6       4       4       4
    ka NUL U+0301 ka            4       3       3       4       4
    ka CR U+0301 ka             4       3       3       4       4
    ka U+0301 NUL ka            3       3       3       3       3
    ka NUL NUL ka               4       4       4       4       4
    ka TAB ka                   3       3       3       3       3
    ka ESC ka                   3       3       3       3       3
    ka SHY ka                   3       3       3       3       3
    ka LS ka                    3       3       3       3       3
    ka NEL ka                   3       3       3       3       3
    ka U+0600 NUL ka            4       3       4       4       4
    a CRLF a                    4       4       3       3       4
    CR LF                       2       2       1       1       2
    a CRLF CRLF a               6       6       4       4       6
    a NUL a                     3       3       3       3       3
    a CRLF ka                   4       4       3       3       4
    a CRLF U+0301 a             5       4       3       4       5

    the models, over 19 cells:
      A  Nov 20's rule         agree on 12 of 19 cells
      B  the WRONG order       agree on 12 of 19 cells
      C  UAX #29's order       agree on 14 of 19 cells
      D  today's ONE rule      agree on 19 of 19 cells
      A wins 4 cells B loses; B wins 4 cells A loses
```

Read the two "12"s together: **A and B lose the same number of cells
and not the same cells.** A is wrong about the control rules; B is
wrong about the *order* and about the fast path. Neither fix subsumes
the other — which is exactly what a per-*ordering* row is for.

### What each change is worth on its own

Each of today's five mask bits is measured by switching it off and
re-running the 19 cells:

```
  what each of today's changes is worth:
    GB3  (CR x LF)                             without it: 16 of 19 cells
    GB4  (break AFTER a control)               without it: 16 of 19 cells
    GB5  (break BEFORE a control)              without it: 18 of 19 cells
    the ORDER (GB4/GB5 before GB9)             without it: 16 of 19 cells
    the ASCII FAST PATH                        without it: 14 of 19 cells
```

Every one of them is worth at least one cell, so none of them is
decoration.

---

## THE CODE

### The five new mask bits

```cpp
inline constexpr unsigned gb3 = 256u;
inline constexpr unsigned gb4 = 512u;
inline constexpr unsigned gb5 = 1024u;
inline constexpr unsigned gb_control_first = 2048u;
inline constexpr unsigned ascii_fast = 4096u;
```

`gb3`, `gb4` and `gb5` are the rules themselves. `gb_control_first` is
not a rule — it is *where* the rules are consulted, and it is a bit so
that the battery can measure the difference between "the rule" and "the
rule in the right place". `ascii_fast` is not a UAX #29 rule at all; it
is in the mask because the arc's ONE rule models the **linked stdlib**,
not the standard.

### The control table — the arc's first COMPLETE table

```cpp
constexpr CtrlRange kControlRanges[] = {
    {0x0000u, 0x0009u, CtrlClass::control},   // the C0 controls
    {0x000Au, 0x000Au, CtrlClass::lf},         // LINE FEED
    ...
    {0xE01F0u, 0xE0FFFu, CtrlClass::control},  // the tags and variation selectors
};
```

The arc's other tables (`kWidth2Ranges`, `kExtendedPictographic`,
`kPrepend`, `kIndicConsonant`, `kIndicLinker`) are RESTRICTED to what
the batteries need. This one is **complete**: 21 ranges, read straight
out of libc++ 21's own `__format/extended_grapheme_cluster_table.h`,
because the whole point of the table is to be the three properties the
control rules are about.

### Where the control check goes

The check is one helper, consulted from two places — and *which* place
is the bit:

```cpp
enum class CtrlAction : unsigned char { none, brk, join };

[[nodiscard]] constexpr CtrlAction control_action(char32_t prev_cp,
                                                  char32_t cp,
                                                  unsigned rules) noexcept {
    const CtrlClass prev = ctrl_class(prev_cp);
    const CtrlClass cur = ctrl_class(cp);
    // GB3 comes FIRST, so it beats GB4's "break after a CR".
    if ((rules & gb::gb3) != 0u && prev == CtrlClass::cr &&
        cur == CtrlClass::lf)
        return CtrlAction::join;
    // GB4: break AFTER a control.
    if ((rules & gb::gb4) != 0u && prev != CtrlClass::none)
        return CtrlAction::brk;
    // GB5: break BEFORE a control.
    if ((rules & gb::gb5) != 0u && cur != CtrlClass::none)
        return CtrlAction::brk;
    return CtrlAction::none;
}
```

With `gb_control_first` **on**, the check runs at the **top** of the
cluster loop — before GB9c, before GB9, before the Prepend handling.
With the bit **off**, the same check runs just *after* GB9, which is
where the arc's rule put it, and the Extend after a NUL joins the
NUL's cluster. The two placements are the two models B and C.

### The ASCII fast path

```cpp
[[nodiscard]] std::size_t count_columns(std::string_view s,
                                        unsigned rules = gb::all) {
    if ((rules & gb::ascii_fast) != 0u && !s.empty() && is_ascii_byte(s[0])) {
        std::size_t k = 0;
        while (k < s.size() && is_ascii_byte(s[k])) ++k;
        // All ASCII: one column per code unit, and no cluster at all.
        if (k == s.size()) return k;
        // The last ASCII code unit is handed back to the cluster rules.
        const std::size_t head = k - 1u;
        return head + cluster_columns(s.substr(head), rules);
    }
    return cluster_columns(s, rules);
}
```

`head + cluster_columns(s.substr(head))` is libc++'s arithmetic
transcribed: the ASCII prefix is `head` code units at one column each,
and the **last** ASCII code unit goes back into the cluster algorithm
together with everything after it.

### The precision's fast-path cut

`prefix_bytes_for_precision` mirrors libc++'s `__truncate`, which uses
the same estimator with the precision as the budget — so the cut lands
on a **code unit** inside the ASCII prefix:

```cpp
for (;;) {
    --max;
    ++it;
    if (it == s.size()) return s.size();
    if (max == 0) {
        if (is_ascii_byte(s[it])) return it;
        break;
    }
    if (!is_ascii_byte(s[it])) break;
}
```

which is why `"a" CRLF "a"` with precision 2 is two bytes (`a`, `0x0D`)
and not three (`a`, `0x0D`, `0x0A`).

---

## THE CELLS NOV 20 STILL LOST — CLOSED

Nov 20 left four cells open and asserted that the gap was still open,
so the next lesson could not quietly forget them. Today they agree,
and the assertion is flipped:

```
  Nov 20's control cells -- CLOSED by Nov 21:
    ka CRLF ka                    3       3
    ka CRLF                       2       2
    ka CRLF CRLF ka               4       4
    ka NUL U+0301 ka              4       4

    0 of 4 control cells still differ
```

---

## WHAT DID NOT CHANGE

Re-measured in this build, and none of them moved:

* the type-category table (Oct 30) and Nov 7's mask tables, its derived
  override table and its 1,862-cell grid — **1862/1862**;
* Nov 13's fill rule and its fill census — **140 of 140**;
* Nov 14's pair grid and the align/zero rule;
* Nov 15's triple grid and the recovery path;
* Nov 16's fill behaviour space;
* Nov 17's width census — the three numbers **1, 11, 28** (bytes, code
  points, columns; the model agrees on **28 of 28**);
* Nov 18's precision battery — **56 of 56**;
* Nov 19's boundary battery — **20 of 20**;
* Nov 20's collision battery — **24 of 24**;
* `petra::FormatError` / `petra::FormatErrorKind` (the four kinds,
  their order and their meanings, exactly as since Sep 6), the two-pass
  (measure, then write) design, the counting_output_iterator, the
  bounded_char_writer, the TooLarge contract and the
  buffer-atomicity behaviour.

The lesson's own summary: **pass=210 fail=0**.

---

## HOW IT WAS BUILT AND CHECKED

```
clang++ -std=c++23 -fexperimental-library \
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
        -Werror -O1 -g -o /tmp/nov21_strict P-2026-11-21-control-chars.cpp
```

clean, no warnings. The CMake build (`CMakeLists.txt` in this
directory) is the same source with the same flags; the binaries were
built into `/tmp` and nothing was staged from a build directory.

The sanitizer run — AddressSanitizer + UndefinedBehaviorSanitizer, both
on, no suppression:

```
cmake -S . -B /tmp/nov21-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build /tmp/nov21-asan
/tmp/nov21-asan/P-2026-11-21-control-chars
```

exits 0 with `pass=210 fail=0` and no sanitizer report.

The oracle is the toolchain's own `std::format` (libc++ 21): every
cell's expected width is measured by *running* it, never by reading the
standard. The control table's 21 ranges were dumped out of libc++ 21's
`__format/extended_grapheme_cluster_table.h`, and the fast path was
read out of `__format/parser_std_format_spec.h`.

---

## Where we go next

Nov 20's items (2)–(12) stand, exactly as they were; item (1) is
closed above. Four new ones:

13. **The ORACLE has a shortcut, and the arc now follows it — so the
    arc's rule is no longer "the standard's rules".** That is a real
    change of contract and it deserves to be visible in the code, not
    only in a comment: the `ascii_fast` bit is a *model* of libc++'s
    implementation, and it will change if libc++ changes. Worth
    writing down the oracle's Unicode version and libc++ version in
    one place in the source (the `EastAsianWidth.txt` provenance note
    is the precedent) so that a future stdlib move shows up as a
    **named divergence** instead of a quiet battery failure.
14. **The fast path is only half of the story: it applies to the
    PREFIX, and the rest of the string is clustered.** A string can
    therefore have its first N columns measured by one rule and its
    last M by another. Worth asking what the *arc* should call that
    boundary — it is not a cluster boundary and not a code point
    boundary, and the arc's `count_columns` now has to know about it in
    two places (the width and the precision).
15. **Every ASCII prefix can be cut in half by a precision.** Today's
    battery has one such cell (`"a" CRLF "a"` at precision 2). The
    general question: which *cluster-forming* ASCII sequences exist at
    all? Answer today: exactly one, GB3's CR x LF — but the fast path
    would cut any cluster the ASCII rules ever grow, so the question
    should be re-asked whenever the rules change.
16. **The restricted tables have still never been broken.** Nov 20's
    items (2)–(4) all say the same thing from three directions: the
    `kWidth2Ranges`, `kExtendedPictographic`, `kPrepend`,
    `kIndicConsonant` and `kIndicLinker` tables are restricted to what
    the batteries need, and every battery so far has agreed with the
    stdlib. Today added the arc's first COMPLETE table, which shows
    the way: pick one restricted table, carry the full one from
    libc++, and find the cells where the two disagree.
