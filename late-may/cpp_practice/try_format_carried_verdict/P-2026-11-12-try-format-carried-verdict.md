# P-2026-11-12 — try_format_carried_verdict

## What today closes

Nov 7's "Where we go next" item (1), verbatim:

> **the fallback currently *discards* the scanner's verdict.** Now that
> the scanner is type-aware, its verdict is worth more: a malformed spec
> should stay `FormatParse` even when the message is unrecognised,
> instead of falling back to `Format`. Worth measuring how many of the
> rejects would classify differently under a fallback that **carries**
> the scanner's verdict rather than replacing it. (The reject count is
> itself build-dependent — 1,172 under libc++, 1,162 under libstdc++ —
> which is today's finding #2 all over again.)

That item is the third link in a chain that has been open since Oct 28:
Oct 28 item (1) ("make the fallback *carry* the scanner's verdict
rather than discard it") → Oct 30 item (1) (same words, inherited) →
Nov 7 item (1) (same words, inherited again) → today.

Today's source is Nov 7's source with three changes:

- the fallback has two shapes, and the lesson **measures** the
  difference between them instead of asserting it;
- the policy table gains a **scalar** per-implementation datum — the
  numeric ceiling — because the standard fixes no ceiling and each
  implementation picks its own;
- the scanner gains one rule (a number too large for this
  implementation's parser is a malformed *format string*), so the
  magnitude family is decided structurally, with no message at all.

## THE HEADLINE — the grid cannot see this defect

The chain item asks for a number: *how many of the rejects would
classify differently?* Here it is, measured on both builds.

**Over the 1,596-cell grid: 0.** Under libc++ the grid has 1,172
rejects, under libstdc++ 1,162, and **none of them change their public
kind** — and none of them even *reach* the fallback.

That is not a null result; it is the finding. Every one of the grid's
rejects is decided by the arc's own scanner (stage A), so stage B — the
fallback — never runs on a single grid cell. A change to the fallback
cannot move a grid cell, because the fallback is not what decides them.
The grid is structurally blind to this defect, which is exactly why the
defect survived three lessons.

So the lesson **extends the measurement** to the families the grid
omits — the numeric magnitude family and the encoding family — and
there the count is not zero:

- **libc++ 21:** 10 rejects in the census; the ceiling rule moves 4 of
  them, the carried verdict moves 1 more; **5 in total**.
- **libstdc++ 15:** 9 rejects in the census; the ceiling rule moves 4;
  the carried verdict moves **0**; **4 in total**.

Two numbers, two builds, two different stories — and the difference
between them is the most interesting thing in this lesson.

## KEY FINDING #1 — the scanner's silence is not a contradiction

Nov 7's classifier, when the stdlib's message says "parse" and the
scanner found nothing:

```cpp
    case MessageVerdict::Parse:
        c.kind = FormatErrorKind::Format;
        c.message = std::string{what} + " [scanner disagrees: string is well-formed]";
        c.reason = "the message says parse but the string is well-formed";
```

The scanner never said the string was well-formed. It said it had **no
objection**. The scanner models a *subset* of `[format.string.std]`:

- it knows the grammar,
- it knows which options each argument category accepts,
- it knows the argument's category,

and it knows **nothing** about the value of an argument, about Unicode,
or about how large a number the implementation's parser can hold. For
every one of those, the scanner is silent — and silence was being read
as an affirmation, which is how a parse verdict the stdlib earned got
thrown away.

The fix keeps both behaviours in the binary and measures the difference
(the two differ in exactly one branch):

```cpp
enum class Fallback : unsigned char { Discard, Carry };
```

`Carry` treats the scanner's "no objection" as silence and lets the
parse verdict survive into `FormatParse`. `Discard` is Nov 7's
behaviour, kept verbatim so the lesson can print both columns.

## KEY FINDING #2 — the standard fixes no ceiling, so the ceiling is an implementation limit

`[format.string.std]` gives the grammar:

```
width:     positive-integer
precision: . nonnegative-integer
```

and then says only *"the value of the positive-integer is interpreted as
a decimal integer and used as the value of the option"*. There is no
upper bound anywhere in the standard. The implementations impose their
own, and each one is visible in its own source:

- **libc++ 21** — `__format/format_string.h`: `__number_max = INT32_MAX`
- **libstdc++ 15** — `format`: `__parse_integer()` returns
  `unsigned short`

Measured on the linked stdlib (the precision is the safe probe — see
the gotchas below):

| probe | libc++ 21 | libstdc++ 15 |
|---|---|---|
| largest precision literal accepted | 2,147,483,647 | 65,535 |
| largest width literal accepted | ≥ 1,048,577 | 65,535 |
| width at ceiling + 1 | rejected | rejected |
| argument index at the ceiling | "too large for the number of arguments supplied" | "invalid arg-id in format string" |
| argument index over the ceiling | "The numeric value of the format specifier is too large" | "invalid arg-id in format string" |

**The shape is different from Nov 7's.** Nov 7's per-implementation
difference was a *mask cell*, keyed by (category, presentation type),
because the two implementations disagreed about one cell of a 7 × 19
matrix. The ceiling is not a cell: it is a **scalar**, and the policy
table grows a second kind of datum:

```cpp
    std::optional<std::uint64_t> max_number{std::nullopt};   // nullopt = no ceiling
    const char* ceiling_note{""};
```

with the "standard" row carrying `std::nullopt` — because the standard
really does impose no ceiling, and a policy row that invented one would
be a claim about the standard that the standard does not make.

The hand-written row is **checked against the measurement**, the same
discipline as Nov 7's derived override table: libc++ 21 → 2,147,483,647
and libstdc++ 15 → 65,535 are both numbers the program computes, not
numbers in a comment.

## KEY FINDING #3 — the ceiling rule is what makes "even when the message is unrecognised" true

This is the load-bearing repair, and it is *not* the carried verdict.

A width, a precision, an argument index or a nested index that is too
large for the implementation's parser is a malformed **format string** —
the stdlib rejects it while it parses, before it looks at the argument
list. So the arc's scanner can decide it structurally, with no message
involved at all:

```
width literal over the ceiling      today: Format          ->  +ceiling: FormatParse
precision literal over the ceiling  today: Format          ->  +ceiling: FormatParse
argument index over the ceiling     today: FormatArgument  ->  +ceiling: FormatParse
nested index over the ceiling       today: FormatArgument  ->  +ceiling: FormatParse
```

Under libstdc++ every one of those four cases arrives with a message
the arc's table does not recognise ("format error: invalid width or
precision in format-spec", "format error: invalid arg-id in format
string"). The message alone cannot classify them. The **rule** can.

That is the chain item's requirement — "a malformed spec should stay
`FormatParse` even when the message is unrecognised" — satisfied under
the build where the message is *least* informative.

## KEY FINDING #4 — the carried verdict is worth exactly what the message vocabulary is worth

The carried verdict and the ceiling rule are two different repairs, and
the census separates them into two columns so the lesson knows which
one moved which cell.

- **libc++ 21:** the carried verdict moves **1** cell — the invalid
  UTF-8 fill, whose message ("The format specifier contains malformed
  Unicode characters") *is* in the arc's message table.
- **libstdc++ 15:** the carried verdict moves **0** cells — because
  libstdc++ accepts that fill (so there is no failure to rescue), and
  because every other libstdc++ message is unrecognised by an arc whose
  table was written from libc++'s vocabulary.

So the honest conclusion is:

> Carrying the verdict is **necessary but not sufficient**. It rescues a
> family only where the stdlib's message vocabulary can name it. Where
> the vocabulary is missing, the scanner has to carry the *rule* — which
> is what the ceiling does.

And the arg-id row above shows the same thing from the other side:
libc++ has two distinct messages (one for "past the end of the argument
list", one for "too large to be a number"), libstdc++ has **one**
message for both. A stdlib that conflates two failures cannot be
classified from its message, so the arc's own rule has to separate them
— and it does: the index *at* the ceiling is `FormatArgument` (it is
past the end of the argument list) and the index *over* the ceiling is
`FormatParse` (it is not a number the parser can hold).

## KEY FINDING #5 — the arc still has a false positive: a multi-byte fill

`[format.string.std]`/3: *"For a format specification in UTF-8, UTF-16,
or UTF-32, the fill character corresponds to a single Unicode scalar
value."*

The arc's scanner is **byte-oriented**: it reads the fill as one byte.
So a four-byte fill (U+1F921) is a malformed spec to the arc and a
valid one to libc++. The census prints it as a FALSE POSITIVE — the
one case where the arc's scanner rejects something the stdlib accepts:

```
a multi-byte UTF-8 fill             ACCEPT  Malformed   FALSE POSITIVE ...
```

This lesson measures it and leaves it visible rather than pretending
the encoding family is done. Fixing it needs a real UTF-8 decoder and
a scalar-value check, which is a bigger piece of work than the ceiling
rule and belongs in its own lesson (see "Where we go next").

## A BUG THIS LESSON FOUND IN ITS OWN FIRST DRAFT — the two-gigabyte memset

The first draft measured the width ceiling by binary search over the
width literals, with a throwing output iterator to stop the output.
That was wrong, and the sanitizer run found it.

A width is a **padding count**. libc++ does not write the padding one
character at a time into the output iterator; it fills its internal
buffer with `std::fill_n` — one `memset` of the whole width — *before*
it hands a single byte to the iterator. So a width of 2^31 means a
2 GiB allocation and a 2 GiB memset, and the throwing sink never gets
a chance to stop it. The first draft's run took over two minutes under
ASan before it was killed.

The fix is in two parts:

- the **precision** is the safe probe, because a precision *truncates*:
  a literal of 2^31 costs three bytes of output;
- the **width** is measured only inside a safety bound (`kSafeWidthBound`
  = 1 MiB), and above the bound only its **rejection side** is checked —
  which costs nothing, because an over-ceiling width is rejected while
  the format string is parsed, before any padding exists.

The lesson now runs in 0.2 s under both toolchains and in 0.3 s under
ASan/UBSan.

The lesson in it: *a probe that looks safe because it stops the output
can still be unsafe if the library buffers internally.* The throwing
sink was never the thing that needed fixing; the assumption about what
it intercepted was.

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
| Section 8 — grid rejects that change kind | **0** | **0** |
| Section 8 — census rejects | 10 | 9 |
| Section 8 — moved by the ceiling rule | 4 | 4 |
| Section 8 — moved by the carried verdict | **1** | **0** |
| Section 8 — false positives | 1 | 1 |
| summary | pass=57 fail=0 | pass=56 fail=0 |

The two pass counts differ by one because the libstdc++ build has one
fewer census reject (it accepts the invalid UTF-8 fill) and one
different assertion (the arg-id messages are "Unknown" there, not
"Argument"/"Parse").

Sanitizer run:

```
clang++ -std=c++23 -fexperimental-library -fsanitize=address,undefined \
        -fno-omit-frame-pointer -g -O1
./lesson_san      -> exit 0, pass=57 fail=0, 0 sanitizer reports
```

and the same thing through the lesson's own CMake presets:

```
cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
cmake --build build
./build/P-2026-11-12-try-format-carried-verdict   -> exit 0, pass=57 fail=0
```

The libstdc++ sanitizer build is **not** available on this machine: the
Homebrew GCC 15.2.0_1 ships no `libasan.dylib` / `libubsan.dylib`
(`g++-15 -print-file-name=libasan.dylib` returns the bare name), so
`-fsanitize=address` cannot link. That is an environment limitation, not
a source problem; the same source builds and runs clean under g++-15
without sanitizers.

## Notes / gotchas worth remembering

- **A precision truncates; a width pads.** That one sentence decides
  which of the two is safe to probe at extreme values. Precision
  truncates the output to at most `min(precision, length)` bytes; a
  width pads the output to *at least* `width` bytes, and libc++ does the
  padding with one `memset` into an internal buffer before the output
  iterator sees anything.
- **An over-ceiling literal is rejected while the format string is
  parsed** — before the argument list is looked at. So the rejection
  side of the magnitude family is free to measure: no padding, no
  allocation, no output.
- **libstdc++ uses one message for two different failures.** "format
  error: invalid arg-id in format string" covers an argument index past
  the end of the list *and* an argument index too large for the parser.
  A message table cannot separate them; a rule can.
- **The stdlib's message vocabulary is libc++-shaped.** The arc's table
  was written from libc++'s messages, so it recognises libc++ and
  nothing else. Under libstdc++ the fallback has nothing to carry.
- **`std::optional` in a `constexpr` policy table** works fine: the
  "standard" row carries `std::nullopt` and the two implementation rows
  carry their measured numbers.
- **The grid is not the world.** 1,596 cells, each isolating ONE
  option, and not one of them reaches the fallback. Every family this
  lesson is about lives outside the grid.

## What did NOT change (deliberately)

- `petra::FormatError` and `petra::FormatErrorKind` — the four kinds,
  their order and their meanings are exactly as they have been since
  Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the `TooLarge`
  contract and the buffer-atomicity behaviour.
- The type-category table from Oct 30, Nov 7's mask tables, its
  derived-override-table machinery, and the 1,596-cell grid — all
  re-measured here, and all unchanged (Sections 3, 4 and 5 print the
  same numbers as Nov 7 did).
- The public contract of the scanner: the new internal failure value
  (`NumberTooLarge`) maps onto the **public** `FormatParse`, which is
  the kind the magnitude family always belonged to. The internal enum
  grew from 7 values to 8; the public enum did not move.
- The lesson's path convention and the file naming: a new dated
  directory `try_format_carried_verdict/`, with the lesson's own
  `CMakeLists.txt` following Nov 7's.

## Where we go next

1. **The Unicode fill rule — new today, and now measured.** The
   standard says the fill "corresponds to a single Unicode scalar
   value"; the arc's scanner reads one byte. The census has one FALSE
   POSITIVE because of it. Next: decode the fill as a UTF-8 scalar
   value, validate it, and re-measure the false-positive count — which
   should also fix the *invalid* UTF-8 fill, turning today's
   message-table rescue into a structural rule.
2. **Nov 7's item (2) — extend the grid past single options.** Every
   one of the 1,596 cells isolates ONE option. The combinations are
   where the phases interact — e.g. does `"{:+c}"` on a `char` fail at
   phase 1 (sign not in the mask) or phase 2 (narrowing)? Worth
   measuring a combination grid.
3. **Nov 7's item (3) — the compile-time path could use the arc's own
   validator.** `std::format_string` rejects `"{:+c}"` on this
   toolchain even though the standard allows it. If the arc's own
   `consteval` validator replaced it, the arc would accept what the
   standard accepts — and *reject* what the standard rejects even when
   libc++ does not. Worth measuring the diagnostic quality of a
   `consteval` failure against `std::format_string`'s.
4. **Nov 7's item (4) — the policy table is now the natural place to
   put the *value* rules.** Today's census shows exactly why: `"{:c}"`
   with 300 is a `FormatArgument` under libc++ (its message is in the
   table) and a generic `Format` under libstdc++ (its message is not).
   A value-dependent rule (does 65 fit in `char`?) has to live
   somewhere else again — probably a third mask, keyed by (category,
   type), selected only after the value is known. Worth measuring
   whether that third table can be derived the same way Nov 7's was.
   The ceiling scalar is a precedent: a policy datum that is not a
   mask cell.
5. **Nov 7's item (5) — Oct 30's items (3), (4), (5) and (6) are still
   open** — the compile-time path skipping Stage A, the hand-written
   `oracle_select` if-chain, the fifth matrix cell, and the
   `std::formatted_size` + `std::format_to` comparison.
