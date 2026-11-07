# P-2026-11-07 — try_format_policy_table

## What today closes

Oct 30's `try_format_type_aware_spec` ended with a "Where we go next"
list. Item (2) is today's lesson, taken **verbatim**:

> **Item (2):** "The two policies are separate code paths today. They
> differ in exactly one boolean (does type `c` narrow the integral
> mask?). Worth collapsing them into one mask table with a per-impl
> override table, so adding a third implementation is a data change."

Oct 30 stated the premise and then asked whether the complexity was
worth it. **Measured: yes, and the extension point is narrower than
anyone guessed.**

---

## THE HEADLINE — the policies differ in one CELL, not one boolean

Oct 30 described the difference between the standard's rules and
libc++ 21's rules as "exactly one boolean". Stated as a matrix, that
is **one cell out of 133**:

```
    category      type 'c'    standard              libc++ 21
    bool          -           (not legal)           (not legal)
    charT         -           (not legal)           (not legal)
    integral      'c'         sign alt zero locale  locale
    floating      -           -                     -
    string        -           -                     -
    pointer       -           -                     -
```

So the override table cannot be keyed by *policy* — it has to be keyed
by **(category, presentation type)**. That is the shape the lesson
takes, and Section 2 proves it exhaustively: **266 comparisons**
(7 categories × 19 type chars × 2 policies) of the new table against
Oct 30's five mask functions, plus the 7 consume masks, with
**zero mismatches**.

The override table is one row:

```
    { category = integral, type = 'c', mask = locale }
```

---

## KEY FINDING #1 — the table can be DERIVED from a measurement

The lesson does not merely assert that libc++ 21's table has one row.
It **derives** the table from the linked stdlib's measured 1,596-cell
grid, then checks the derived table against the hand-written one, cell
by cell. A derived table that disagreed with the hand-written table
would fail the run.

```
  DERIVED override rows for "libc++ 21": 1
    { category = integral, type = 'c', mask = locale }   // measured on the int row

  the DERIVED table, re-run against the measured grid:
    agree 1596/1596
    hand-written vs derived: 0 mismatch(es)
```

And under libstdc++:

```
  DERIVED override rows for "libstdc++ 15": 0
    (none: this implementation's allow matrix IS the
     standard's, cell for cell)
```

**That second line is the important one.** libstdc++ 15's override
table is **empty**, which is the machine-checked form of "libstdc++
implements the standard". It is no longer a claim in the notes; it is
a number the program computes. **A number in a comment is not a check;
a number the program computes at runtime is.**

This is also the payoff of the whole exercise: *"adding a third
implementation is a data change"* is now an operation rather than a
slogan. The third implementation ("libstdc++ 15") is in the table
**with zero rows** — because it needs no override at all.

---

## KEY FINDING #2 — the 10 divergent cells are the SAME 10 in both builds

The grid is 1,596 cells: 7 categories × 228 specs, where each spec is
one of 12 single-option prefixes crossed with 19 presentation types.
Every cell is run through the **linked** stdlib, live — nothing about
the libc++ column is hard-coded.

```
  build                    measured stdlib   standard   libc++ 21
  clang++ / libc++ 21      libc++ 21         1586/1596  1596/1596
  g++-15  / libstdc++ 15   libstdc++ 15      1596/1596  1586/1596
```

The two numbers are **mirror images**, and the reason is that the
divergence is a property of the *rules*, not of the *build*:

```
    int  spec="+c"  standard=ACCEPT  libc++ 21=REJECT  linked stdlib says OK
    int  spec="-c"  standard=ACCEPT  libc++ 21=REJECT  linked stdlib says OK
    int  spec=" c"  standard=ACCEPT  libc++ 21=REJECT  linked stdlib says OK
    int  spec="#c"  standard=ACCEPT  libc++ 21=REJECT  linked stdlib says OK
    int  spec="0c"  standard=ACCEPT  libc++ 21=REJECT  linked stdlib says OK
    long spec="+c"  standard=ACCEPT  libc++ 21=REJECT  linked stdlib says OK
    ... (5 more, the same five for long)
```

Under libc++ the linked stdlib says **REJECT** on those 10 cells; under
libstdc++ it says **OK**. **Which side rejects them flips with the
toolchain; WHICH cells they are does not.** The program now reports
both facts separately, so they can never be confused:

```
  of those 10 cells, the linked stdlib sides with libc++ 21's
  model on 0 of them, and with the standard's model on 10.
```

The upstream cause is unchanged from Oct 30: the standard says the
sign / alternate-form / zero-padding options are valid "for
arithmetic types other than charT and bool **or when an integer
presentation type is specified**", and `c` **is** an integer
presentation type for `int` (Table 107). So `"{:+c}"` on an `int` is
**valid** per the standard. libc++ rejects it because its phase-2
narrowing to the bool-like mask is applied to `int` as well. Tracked
as **llvm/llvm-project#106136** and **microsoft/STL#4918**.

---

## KEY FINDING #3 — the CONSUME mask is a diagnostic knob; the ALLOW mask decides

This is the re-tune knob, and it is deliberately kept **separate** from
the allow mask. libc++ parses an option only if the *category's* mask
includes it, so an option outside the mask is left in the input and the
error is about the leftover. Section 5 measures what changing the
**consume** mask does to the arc's own output, holding the allow mask
fixed:

```
  cells compared: 1596   (rejected by both: 1162)
  accept/reject VERDICT changed:      0
  PUBLIC FormatErrorKind changed:      0
  public diagnostic DETAIL changed:    399
  internal failure enum changed:       294
```

Read those four numbers together — they are the lesson:

- The **verdict** never changes. The consume mask does not decide
  *whether* a spec is rejected.
- The **public `FormatErrorKind`** never changes. That is the arc's
  contract, and it has been 4 kinds since Sep 6.
- The **diagnostic detail** changes in 399 cells, and the **internal**
  failure enum changes in 294.

The third and fourth lines look contradictory until you notice the arc
has **two taxonomies**: `petra::FormatErrorKind` (4 public kinds) and
the scanner's internal failure enum (6 values). The consume mask moves
a cell from one *internal* value to another — `OptionNotAllowed` to
`TypeNotAllowed`, say — and the public kind does not move at all,
because both internal values map onto the same public
`FormatArgument`. **A caller cannot see the difference; only the
diagnostic text changes.**

(Oct 30's phase-1 mask was libc++'s, hard-wired for *both* policies —
an accident of history that the table makes explicit and
parameterised.)

---

## THREE BUGS THIS LESSON FOUND IN ITS OWN FIRST DRAFT

These are not hypotheticals. Each one was caught by **measuring**, and
each one is a lesson about how a self-checking program still has to be
read by a human.

### Bug 1 — `__GLIBCXX_RELEASE` is not a macro

The real libstdc++ macro is `_GLIBCXX_RELEASE` — **one** leading
underscore. The draft used the double-underscore spelling, which **no
compiler defines**:

```
$ echo | g++-15 -dM -E -x c++ - | grep -c '__GLIBCXX_RELEASE'
0
```

So the `#elif` branch was dead code, and under g++ `linked_policy()`
silently returned the `"standard"` policy. The program printed
*"built against an unknown standard library"* and modelled the wrong
implementation — **while every assertion still passed**, because
"standard" and "libstdc++ 15" happen to share an allow matrix today.
A typo that only a human eye could catch.

### Bug 2 — Section 3 measured the wrong quantity

The draft's "divergent cells" vector compared `model(standard)`
against the **linked stdlib**. That count is 10 under libc++ and **0**
under libstdc++ — it depends on the *build*. The run failed with
*"got 0, want 10"*.

But the lesson's actual claim is about the two **policies**
disagreeing, which is 10 cells in *both* builds, and the *same* 10.
Section 3 now measures the policy-vs-policy divergence
(build-invariant) **and** reports which side the linked stdlib sides
with (build-dependent), so the two can never be confused again.

### Bug 3 — Section 5 compared the internal enum, not the contract

The draft asserted *"the failure kind changes in 0 cells"* over the
6-value internal `SpecFailure`. But the internal kind **does** change,
in 294 cells — the draft's own assertion was simply false, and the run
failed with *"got 294, want 0"*. The **public** `FormatErrorKind` is
what is stable, in all 1,596 cells.

Asserting a claim about an internal enum while the prose talks about
the public one is how a check stops checking. Section 5 now reads the
public kind from the arc's **own classifier**
(`classify_format_failure`) rather than from a second, hand-written
copy of the internal→public mapping — one source of truth, exactly as
the rest of the lesson does.

---

## Verification

```
$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
      -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
      -Wsign-conversion -O0 -o lesson \
      P-2026-11-07-try-format-policy-table.cpp
(clean — no warnings)

$ ./lesson
...
=== summary ===
  pass=39  fail=0
```

**Strict warnings** are clean on **both** toolchains, under the
repository's own `ENABLE_STRICT_WARNINGS` set (`-Wall -Wextra
-Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`):

```
$ g++-15   -std=c++23 -Wall -Wextra -Wpedantic -Werror -Wshadow \
           -Wconversion -Wsign-conversion -O0 -o lesson_gcc  <src>
$ clang++  -std=c++23 -stdlib=libc++ -fexperimental-library \
           -Wall -Wextra -Wpedantic -Werror -Wshadow \
           -Wconversion -Wsign-conversion -O0 -o lesson_libc <src>
$ ./lesson_gcc   -> pass=39  fail=0
$ ./lesson_libc  -> pass=39  fail=0
```

**Sanitizers.** ASan + UBSan, clean (no diagnostics on stderr):

```
$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
      -Wall -Wextra -O1 -g -fsanitize=address,undefined \
      -fno-omit-frame-pointer -o lesson_asan <src>
$ ./lesson_asan
=== summary ===
  pass=39  fail=0
(no sanitizer diagnostics on stderr)
```

The libstdc++ side was verified with **UBSan in trap-on-error mode**
(`-fsanitize=undefined -fsanitize-undefined-trap-on-error
-fno-sanitize-recover=all`), which needs no sanitizer runtime library
and therefore works with the Homebrew gcc-15 toolchain — that
toolchain ships the sanitizer *headers* but no `libasan`/`libubsan`
runtime, so `-fsanitize=address` cannot link there:

```
$ g++-15 -std=c++23 -O1 -g -fsanitize=undefined \
      -fsanitize-undefined-trap-on-error -fno-sanitize-recover=all \
      -o lesson_ubsan <src>
$ ./lesson_ubsan
=== summary ===
  pass=39  fail=0
```

**The 39 in-program checks** cover: the four policy tables (base
permissions, integer-presentation types, consume masks, override rows),
the linked-implementation selection, the 266-cell equivalence against
Oct 30's five mask functions plus the 7 consume masks, the 2 divergent
matrix cells, the 1,596-cell grid against the linked stdlib, the
build-invariant 10-cell policy divergence and the build-dependent
"which side does the stdlib take", the derived-vs-hand-written override
table in both directions, the Section 5 four-number consume-mask
comparison, and the arc's own end-to-end paths (formatting, the two
error kinds, TooLarge, buffer atomicity, user-defined abstention).

**Every number in the notes above is printed by the program.** Nothing
here is a hand-copied constant.

---

## Notes / gotchas worth remembering

- **The real libstdc++ macro is `_GLIBCXX_RELEASE`, one underscore.**
  The double-underscore spelling is not defined by anything, and a
  `#elif defined(...)` on a non-existent macro is silently dead code.
- **Check that a preprocessor branch is reachable.** `g++ -dM -E` is
  the cheapest test there is, and it found a bug that every assertion
  in the program sailed past.
- **A build-dependent number is not a fact about the rules.** "10
  divergent cells" meant two different things depending on which
  stdlib you linked; the lesson's claim was about the *policies*, so
  that is what it now measures.
- **The arc has two taxonomies.** The public `FormatErrorKind` (4
  kinds) is the contract; the internal failure enum (6 values) is an
  implementation detail. They do not move together, and only one of
  them is a promise.
- **Compare the contract, not your own copy of it.** Reading the
  public kind out of the arc's own `classify_format_failure` beats
  re-deriving the mapping in the test.
- **A derived table that is not checked against a hand-written one is
  just a different kind of claim.** Today's table is derived from a
  measurement *and* diffed against the hand-written rows.
- **An override table keyed by (category, type) is a matrix cell, not
  a policy.** "The two policies differ in one boolean" was true but
  misleading: they differ in one *cell* of a 7 × 19 matrix.

---

## What did NOT change (deliberately)

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order, and their meanings are exactly as they have been since
  Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the TooLarge
  contract, and the buffer-atomicity behaviour.
- The type-category table from Oct 30, and the five mask functions it
  introduced. Section 2 checks the new table against them rather than
  deleting them.
- **No pad flags.** The pad axis is orthogonal to the policy table, so
  the pad options are not reproduced here.

---

## Where we go next

1. **Oct 30's item (1) is still open** — the fallback currently
   *discards* the scanner's verdict. Now that the scanner is
   type-aware, its verdict is worth more: a malformed spec should
   stay `FormatParse` even when the message is unrecognised, instead
   of falling back to `Format`. Worth measuring how many of the
   rejects would classify differently under a fallback that
   **carries** the scanner's verdict rather than replacing it. (The
   reject count is itself build-dependent — 1,172 under libc++, 1,162
   under libstdc++ — which is today finding #2 all over again.)
2. **Extend the grid past single options.** Every one of the 1,596
   cells isolates ONE option. The combinations are where the two
   phases interact — e.g. does `"{:+c}"` on a `char` fail at phase 1
   (sign not in the mask) or phase 2 (narrowing)? Worth measuring a
   combination grid.
3. **The compile-time path could use the arc's own validator.**
   `std::format_string` rejects `"{:+c}"` on this toolchain even
   though the standard allows it. If the arc's own `consteval`
   validator replaced it, the arc would accept what the standard
   accepts — and *reject* what the standard rejects even when libc++
   does not. Worth measuring the diagnostic quality of a `consteval`
   failure against `std::format_string`'s.
4. **The policy table is now the natural place to put the *value*
   rules.** The consume mask turned out to be a pure diagnostic knob,
   and the allow mask a pure verdict knob. A value-dependent rule
   (does `65` fit in `char`?) has to live somewhere else again —
   probably a third mask, keyed by (category, type), selected only
   after the value is known. Worth measuring whether that third table
   can be derived the same way this one was.
5. **Oct 30's items (3), (4), (5) and (6) are still open** — the
   compile-time path skipping Stage A, the hand-written
   `oracle_select` if-chain, the fifth matrix cell, and the
   `std::formatted_size` + `std::format_to` comparison.
