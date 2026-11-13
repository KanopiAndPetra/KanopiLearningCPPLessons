# P-2026-11-13 — try_format_unicode_fill

## What today closes

Nov 12's "Where we go next" item (1), verbatim:

> **The Unicode fill rule — new today, and now measured.** The standard
> says the fill "corresponds to a single Unicode scalar value"; the
> arc's scanner is BYTE-oriented: it reads the fill as one byte, so a
> four-byte fill (U+1F921) is a malformed spec to the arc and a valid
> one to libc++. This lesson MEASURES the false positive and leaves it
> visible; fixing it needs a real UTF-8 decoder, which is the next
> link in the chain.

Closed. Today the arc has a real UTF-8 decoder, the fill is read as a
scalar value, and the false positive is gone — **under both toolchains,
with zero disagreements of any kind.**

The item also said the fix "should also fix the *invalid* UTF-8 fill,
turning today's message-table rescue into a structural rule". It did:
the ill-formed fill is now decided by the arc's own scanner, with no
message table consulted at all.

## THE HEADLINE — one rule, zero disagreements

Nov 12's census measured the arc's scanner against the linked stdlib on
a family the 1,596-cell grid cannot see. The fill was one of those
families, and it was the last one that disagreed.

Today the same census runs with **two** fill rules side by side: the
old byte-oriented one and the new scalar-value one, each against the
**linked** stdlib.

| the fill census | libc++ 21 | libstdc++ 15 |
|---|---|---|
| byte-oriented rule — disagreements with the stdlib | **8** | **6** |
| — of which false positives (arc rejects, stdlib accepts) | 4 | 6 |
| — of which false negatives (arc accepts, stdlib rejects) | 4 | 0 |
| **scalar-value rule — disagreements with the stdlib** | **0** | **0** |
| — false positives | 0 | 0 |
| — false negatives | 0 | 0 |

Eight and six are not the same eight and six. Under libc++ the byte
rule is wrong in *both* directions; under libstdc++ it is wrong only in
one. The reason is the next finding.

## KEY FINDING #1 — the fill is a CHARACTER, so the scanner must DECODE it

[format.string.std]/3: *"For a format specification in UTF-8, UTF-16,
or UTF-32, the fill character corresponds to a single Unicode scalar
value."*

The arc's scanner read **one byte** and asked *"is the byte after it an
alignment option?"*. A four-byte fill has no such byte, so the arc
called a perfectly valid spec malformed. That was the false positive
Nov 12 left visible.

The fix is a UTF-8 decoder, and it is `constexpr` so the scanner stays
`constexpr` — the arc's scanner has always been usable in a constant
expression, and it still is.

The decoder reads Unicode 15 Table 3-7 (Well-Formed UTF-8 Byte
Sequences) **backwards**. The table says which byte sequences are
well-formed; the decoder says which of the *others* it is:

| fault | what it is | the maximal subpart |
|---|---|---|
| `Truncated` | the sequence stops early — the input ran out, or a byte that cannot continue it came next | the bytes consumed (1–3) |
| `InvalidLead` | the lead byte cannot start any well-formed sequence at all (a continuation byte 0x80–0xBF, or 0xF8–0xFF) | 1 |
| `Overlong` | a complete sequence naming a value below the minimum for its length (0xC0/0xC1 are overlong-only leads) | 1 |
| `Surrogate` | a complete sequence naming U+D800–U+DFFF | 1 |
| `OutOfRange` | a complete sequence naming a value above U+10FFFF | 1 |

Two details that are easy to get wrong, and that the lesson's own
first draft got wrong before the assertions caught them:

- **The maximal subpart of an overlong 2-byte sequence is 1 byte, not
  2.** "E0 80" is not a prefix of any well-formed sequence — E0's
  second byte must be A0–BF — so the maximal subpart of "E0 80 80" is
  the lead byte alone. Same for "ED A0" (ED's second byte must be
  80–9F, or the sequence names a surrogate) and "F4 90" (F4's second
  byte must be 80–8F, or the value is above U+10FFFF).
- **The maximal subpart of a truncated sequence is what was consumed.**
  "E2 82" is a valid prefix — E2's second byte may be 80–BF — so it
  *could* still be completed, and the subpart is 2 bytes.

The maximal subpart matters because it is exactly what an
implementation that substitutes U+FFFD has to replace. The scanner has
to know how many bytes to skip.

## KEY FINDING #2 — the two implementations disagree about ill-formed fills, and the difference is a MASK

This is the finding that makes the lesson more than a bug fix.

**libc++ 21 rejects every ill-formed fill**, with one message:
*"The format specifier contains malformed Unicode characters"*. It
rejects a lone continuation byte, 0xFE, 0xC0, a truncated 3-byte
sequence, a bad continuation byte, an overlong encoding, a surrogate,
and a value above U+10FFFF. All eight, unconditionally.

**libstdc++ 15 is not uniform.** It makes only *some* of them fatal:

| ill-formed fill | libc++ 21 | libstdc++ 15 |
|---|---|---|
| a lone continuation byte (0x80) | reject | **accept** (U+FFFD) |
| 0xFE (never a lead byte) | reject | **accept** (U+FFFD) |
| 0xC0 (overlong-only lead) | reject | **accept** (U+FFFD) |
| a truncated 3-byte sequence | reject | **accept** (U+FFFD) |
| a truncated 4-byte sequence | reject | **accept** (U+FFFD) |
| a bad continuation byte | reject | reject |
| an overlong encoding | reject | reject |
| a surrogate | reject | reject |
| a value above U+10FFFF | reject | reject |

The libstdc++ pattern is exactly the one the Unicode standard's
*maximal subpart* rule describes: libstdc++ **recovers** from a
sequence that merely stops early — it substitutes U+FFFD and carries
on — but it **rejects** a sequence that decodes to a value that is not
a scalar value at all (overlong, surrogate, above U+10FFFF), and it
rejects a bad continuation byte.

So the per-implementation datum is not a bit. It is a **mask** of
which fault classes are fatal:

```cpp
struct FillPolicy {
    bool unicode{true};              // read the fill as a scalar value
    std::uint8_t fatal{0};           // which ill-formed fills are fatal
};
```

with libc++ 21's row carrying every fault and libstdc++ 15's row
carrying only the overlong / surrogate / out-of-range ones. This is
the same shape as Nov 7's per-implementation override rows and Nov
12's numeric ceiling: **the standard fixes the rule, each
implementation fixes its own enforcement of it** — and the policy
table is where that difference lives, so one source is correct against
either stdlib.

That is also why the byte-oriented rule's error count is 8 under
libc++ and 6 under libstdc++: the byte rule happens to reject the
ill-formed fills that libstdc++ also rejects (6 of them, no false
negatives), but it accepts the ones libstdc++ also accepts — and
libc++ rejects all 8, so the byte rule is wrong in both directions
there.

## KEY FINDING #3 — the ill-formed fill is a STRUCTURAL rule now, so the message table is not consulted

Nov 12's carried verdict rescued the invalid-UTF-8 fill under libc++
**only because libc++'s message happened to be in the arc's
libc++-shaped message table**. That was a rescue that depended on the
message vocabulary, and it did nothing under libstdc++.

Today the scanner rejects the fill on its own:

```
  an ill-formed fill, decided by the scanner:
    the linked stdlib rejects it: The format specifier contains malformed Unicode characters
    the scanner says: the format spec is malformed
    the message table was consulted: NO
```

The consequence shows up in Section 8's census, which is the arc's
standing regression test:

| Section 8 — the census | Nov 12 (libc++) | today (libc++) | Nov 12 (libstdc++) | today (libstdc++) |
|---|---|---|---|---|
| census rejects | 10 | 10 | 10 | 10 |
| moved by the ceiling rule | 4 | 4 | 5 | 5 |
| **moved by the carried verdict** | **1** | **0** | 0 | 0 |
| **false positives (arc rejects, stdlib accepts)** | **1** | **0** | **1** | **0** |

The carried verdict now moves **nothing** in the census — because the
one family that needed rescuing is now decided structurally. The
false positive that has been in this arc since Nov 12 is gone.

## KEY FINDING #4 — the WIDTH counts CODE POINTS, not bytes

A width of 10 with a four-byte fill produces **31 bytes**:

```
  the width counts code points, not bytes:
    "{:*^5}" with 3 euros    -> 11 bytes, 5 code points
    "{:🤑^10}" with "abc"   -> 31 bytes, 10 code points
```

Ten code points — seven of them the fill — and thirty-one bytes.
Three euros plus two ASCII fill bytes is 11 bytes for a width of 5.

This is not a bug in anything, and it is worth stating precisely
because the arc has two different units in it:

- the **width** is a count of **code points**;
- the arc's **cap** is a count of **bytes**, because it is a buffer
  cap and the two-pass design measures the stdlib's actual output.

The arc is right on both counts, but they are different units and
conflating them would be a bug. A fill of four bytes multiplies the
output size without changing the width at all.

## A BUG THIS LESSON FOUND IN ITS OWN FIRST DRAFT — the maximal subpart

The first version of the decoder reported the *number of bytes it
examined* rather than the maximal subpart, and it classified "E2 82"
as a bad continuation rather than a truncated sequence. The lesson's
own assertions caught it — six FAIL lines on the first run — and the
fix is the table in KEY FINDING #1 above.

The bug was worth finding because it is the *same* bug in the stdlibs: the
maximal subpart is what a recovering implementation replaces, and
getting it wrong means replacing the wrong number of bytes — or, in
the arc's case, skipping the wrong number of bytes when it looks for
the alignment option.

The arc's case is easy to see. The spec of `"{:\xE2\x82<5}"` is
`"\xE2\x82<5"`. If the scanner consumed only ONE byte of the
truncated prefix, the next character would be `0x82` — not an
alignment option — and the scan would fail. Consuming the maximal
subpart (2 bytes) puts the scanner exactly on the `<`, which is where
the alignment option is.

What the *output* shows is a different thing, and it is worth stating
precisely because it is easy to guess wrong: libstdc++ substitutes
**one** U+FFFD for the whole maximal subpart, and then repeats that
one code point to satisfy the width. All four of these produce the
same 9 bytes (`"abc"` plus two U+FFFD, because `"abc"` is 3 code
points and the width is 5):

| the fill | maximal subpart | the output |
|---|---|---|
| `"\xE2\x82"` (2 bytes) | 2 | `"abc"` + 2× U+FFFD, 9 bytes |
| `"\xE2"` (1 byte) | 1 | `"abc"` + 2× U+FFFD, 9 bytes |
| `"\x80"` (1 byte) | 1 | `"abc"` + 2× U+FFFD, 9 bytes |
| `"\xF0\x9F\xA4"` (3 bytes) | 3 | `"abc"` + 2× U+FFFD, 9 bytes |

The number of U+FFFDs in the output is decided by the WIDTH, not by
the length of the broken prefix. The maximal subpart decides how many
bytes of the *spec* are consumed — which is what the scanner needs.

## Verification

Builds (both with `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`, the exact flag set the lesson's
`CMakeLists.txt` uses):

```
clang++ -std=c++23 -fexperimental-library   -> clean, exit 0
g++-15  -std=c++23                          -> clean, exit 0
```

Runs — every number below was printed by the program, not typed here:

| measurement | libc++ 21 | libstdc++ 15 |
|---|---|---|
| Section 3 — grid cells / stdlib rejects | 1,596 / 1,172 | 1,596 / 1,162 |
| Section 3 — model(standard) vs stdlib | 1,586/1,596 | 1,596/1,596 |
| Section 3 — model(libc++ 21) vs stdlib | 1,596/1,596 | 1,586/1,596 |
| Section 4 — derived override rows | 1 | 0 |
| Section 4 — derived table vs measured grid | 1,596/1,596 | 1,596/1,596 |
| Section 4 — hand-written vs derived | 0 mismatches | 0 mismatches |
| Section 5 — public kind changed by the consume mask | 0 / 1,596 | 0 / 1,596 |
| Section 7 — precision ceiling (exact) | 2,147,483,647 | 65,535 |
| Section 7 — policy row vs measurement | match | match |
| Section 8 — grid rejects that reach the fallback | 0 | 0 |
| Section 8 — grid rejects that change kind | 0 | 0 |
| Section 8 — census rejects | 10 | 10 |
| Section 8 — moved by the ceiling rule | 4 | 5 |
| **Section 8 — moved by the carried verdict** | **0** | **0** |
| **Section 8 — false positives** | **0** | **0** |
| **Section 9 — byte rule disagreements** | **8** (4 FP, 4 FN) | **6** (6 FP, 0 FN) |
| **Section 9 — scalar rule disagreements** | **0** | **0** |
| Section 9 — the message table consulted for an ill-formed fill | **NO** | n/a (the stdlib accepts it) |
| summary | pass=106 fail=0 | pass=103 fail=0 |

The two pass counts differ by three, and the program shows where: the
per-section counts are identical through Section 8 (49 passes each),
and the difference is in the two sections that BRANCH on the stdlib's
own behaviour:

- Section 9.3 — under libc++ the stdlib rejects the ill-formed fill,
  so the lesson takes the branch with three assertions (the message
  table was not consulted, the scanner's verdict was `Malformed`, and
  the public kind is `FormatParse`); under libstdc++ the stdlib
  accepts it, so the branch has one assertion.
- Section 10 — under libc++ the arc's own runtime path has two
  assertions for the invalid UTF-8 fill; under libstdc++ it has one,
  because the stdlib accepts that format string at all.

Sanitizer run:

```
clang++ -std=c++23 -fexperimental-library -fsanitize=address,undefined \
        -fno-omit-frame-pointer -g -O1
./lesson3_asan      -> exit 0, pass=106 fail=0, 0 sanitizer reports
```

and the same thing through the lesson's own CMake presets:

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
cmake --build build
./build/P-2026-11-13-unicode-fill-rule   -> exit 0, pass=106 fail=0
```

The libstdc++ sanitizer build is **not** available on this machine: the
Homebrew GCC 15.2.0_1 ships no `libasan.dylib` / `libubsan.dylib`
(`g++-15 -print-file-name=libasan.dylib` returns the bare name), so
`-fsanitize=address` cannot link. That is an environment limitation, not
a source problem; the same source builds and runs clean under g++-15
without sanitizers.

## Notes / gotchas worth remembering

- **The maximal subpart is not "the bytes examined".** It is the
  longest prefix that could still be completed into a well-formed
  sequence. For an overlong encoding that is usually *one byte* — the
  lead — because "E0 80" can never be completed.
- **A byte that cannot continue a sequence ends the sequence; it does
  not make it invalid.** "E2 82" followed by "<" is a *truncated*
  sequence with a 2-byte maximal subpart, not a bad continuation.
- **0xC0 and 0xC1 are overlong-only leads.** They can only ever encode
  a value below U+0080, so the overlong check has to be at *completion*,
  not at the second byte.
- **E0's second byte must be A0–BF, ED's must be 80–9F, F0's must be
  90–BF, F4's must be 80–8F.** These four bounds are what separate
  "overlong", "surrogate" and "above U+10FFFF" from each other.
- **The width is in code points; the cap is in bytes.** A four-byte
  fill with a width of 10 is 31 bytes. Two units, one format string.
- **libstdc++ 15 recovers from an ill-formed fill by substituting
  U+FFFD; libc++ 21 rejects it.** The same source must model both,
  which is what the policy mask is for.
- **libc++'s message table is libc++-shaped.** The arc's table was
  written from libc++'s messages, so under libstdc++ the fallback has
  nothing to carry. Today the fill family does not need it at all.
- **The grid is not the world.** 1,596 cells, each isolating ONE
  option, and not one of them reaches the fallback. Every family this
  lesson is about lives outside the grid.

## What did NOT change (deliberately)

- `petra::FormatError` and `petra::FormatErrorKind` — the four kinds,
  their order and their meanings are exactly as they have been since
  Sep 6.
- The two-pass (measure, then write) design, the `counting_output_iterator`,
  the `bounded_char_writer`, the `TooLarge` contract and the
  buffer-atomicity behaviour.
- The type-category table from Oct 30, Nov 7's mask tables, its derived
  override table, and the 1,596-cell grid — all re-measured here, and
  all unchanged (Sections 3, 4 and 5 print the same numbers as Nov 12
  did).
- The public contract of the scanner: the ill-formed fill maps onto the
  **existing** `SpecFailure::Malformed`, which the arc already mapped
  onto the **public** `FormatParse`. `SpecFailure` and `ScanFailure`
  did not grow at all — the new taxonomy is a separate enum
  (`petra::detail::FillFault`, 6 values) that lives beside the policy
  table rather than inside the scanner's verdict. The public enum did
  not move.
- The lesson's path convention and the file naming: a new dated
  directory `try_format_unicode_fill/`, with the lesson's own
  `CMakeLists.txt` following Nov 12's.

## Where we go next

1. **The width-counts-code-points rule is now MEASURED but not yet
   MODELLED.** The arc delegates the actual formatting to the stdlib,
   so the width only matters to the arc as a count of bytes the
   stdlib reports. But the arc's *own* padding — if it ever grows one
   — would have to count code points, not bytes, and the two-pass
   design's "measure then write" would have to agree with it on both
   units. Worth measuring what a byte-counting padding would produce
   against what the stdlib produces, for a multi-byte fill.
2. **Nov 12's item (2) — extend the grid past single options.** Every
   one of the 1,596 cells isolates ONE option, and not one of them
   reaches the fallback. Worth measuring a combination grid — e.g.
   does `"{:+c}"` on a `char` fail at phase 1 (sign not in the mask) or
   phase 2 (narrowing)?
3. **Nov 12's item (3) — the compile-time path could use the arc's own
   validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. If the arc's own
   `consteval` validator replaced it, the arc would accept what the
   standard accepts — and *reject* what the standard rejects even when
   libc++ does not. Worth measuring the diagnostic quality of a
   `consteval` failure against `std::format_string`'s.
4. **Nov 12's item (4) — the policy table is now the natural place to
   put the *value* rules.** Today's census shows exactly why: `"{:c}"`
   with 300 is a `FormatArgument` under libc++ (its message is in the
   table) and a generic `Format` under libstdc++ (its message is not).
   A value-dependent rule (does 65 fit in `char`?) has to live
   somewhere else again — probably a third mask, keyed by (category,
   type), selected only after the value is known. Worth measuring
   whether that third table can be derived the same way Nov 7's was.
   The ceiling scalar is a precedent: a policy datum that is not a
   mask cell.
5. **The `fill` is not the only place a Unicode scalar value can
   appear in a format spec.** The width and precision are ASCII, and
   the type char is ASCII, so today the fill is the only place the
   scanner has to decode. But the *argument* is also a Unicode string,
   and the standard counts the width of a string argument in code
   points too — so the arc's `FormatError` for a string argument that
   is too long is a value-dependent rule of exactly the shape item 4
   describes. Worth measuring whether the two rules want the same
   table.
6. **Nov 12's item (5) — Oct 30's items (3), (4), (5) and (6) are still
   open** — the compile-time path skipping Stage A, the hand-written
   `oracle_select` if-chain, the fifth matrix cell, and the
   `std::formatted_size` + `std::format_to` comparison.
