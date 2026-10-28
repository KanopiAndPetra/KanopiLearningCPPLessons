# P-2026-10-28 — try_format_error_taxonomy

## What today closes

Oct 27's `try_format_vs_format_to_n` ended with a "Where we go
next" list. Item (2) is today's lesson, taken **verbatim**:

> **Item (2):** "Give the arc's error taxonomy the resolution the
> stdlib has. `FormatParse` and `FormatArgument` are unreachable
> today; the stdlib distinguishes four failure classes by message.
> Worth measuring whether libc++ exposes the class structurally (an
> error code, or distinguishable exception types) before deciding
> whether the arc should classify by message prefix."

Oct 27 predicted the question was whether to classify by message
prefix. The answer is **yes, but not by message prefix alone** — and
the reason is the interesting part.

---

## THE HEADLINE — the stdlib has NO structural error class at all

The arc's `petra::FormatErrorKind` has had **four** values since
Sep 6:

```cpp
enum class FormatErrorKind : unsigned char {
    FormatParse,      // <-- DEAD until today
    FormatArgument,   // <-- DEAD until today
    Format,
    TooLarge,
};
```

For twenty-two lessons the implementation only ever produced **two**
of them. `FormatParse` and `FormatArgument` had no code path that
could reach them.

Oct 27 asked whether libc++ exposes the failure class *structurally*.
**Measured: it does not, in any way.**

- `std::format_error` is the **only** exception type the format
  machinery throws. Measured: **65 `__throw_format_error` call sites**
  in libc++ 21's `__format/` headers, and every one of them throws
  that one type. Nothing in `__format/` throws anything else.
- It is a bare `runtime_error` subclass. It carries **no
  `error_code`** (measured by a detection idiom — `has_code<
  std::format_error>` is `false`), and its only payload is the message
  string.
- There is **no subclass** of it anywhere in libc++, so
  `catch (my_derived&)` catches nothing (measured at runtime:
  `caught_derived=0 caught_base=1`, and `typeid(e) ==
  typeid(std::format_error)` exactly).
- `std::format_error` is not `final`, so a user *can* subclass it —
  but that is useless, because nothing in the library ever throws a
  subclass.
- The failure class therefore survives **only in the message text**,
  and libc++ 21 has **43 distinct message strings** for it (measured
  by scanning the headers).

So the class is not a type, a code, or an enum. **It is a string.**

That is the constraint today's lesson is built inside: the arc has to
*build* the classifier the standard library does not ship.

---

## KEY FINDING #1 — the arc's classifier is TWO-STAGE and DEFENSIVE

The message table is libc++-specific and therefore fragile, so the
arc does not lean on it alone:

```cpp
[[nodiscard]] FormatError
classify_format_failure(std::string_view fmt, const char* what) noexcept {
    const GrammarResult g = scan_format_string(fmt);
    if (!g.well_formed()) {
        return FormatError{FormatErrorKind::FormatParse, ...};
    }
    switch (verdict_from_message(what)) {
    case MessageVerdict::Parse:     return FormatError{FormatErrorKind::Format, ...};
    case MessageVerdict::Argument:  return FormatError{FormatErrorKind::FormatArgument, ...};
    case MessageVerdict::Ambiguous: return FormatError{FormatErrorKind::FormatArgument, ...};
    case MessageVerdict::Unknown:   return FormatError{FormatErrorKind::Format, ...};
    }
}
```

**Stage A** is a `constexpr` grammar scanner written today (Part 4 of
the source). It answers one question about the *format string alone*:
is it well-formed? It does not look at the arguments and it does not
look at any stdlib message.

**Stage B** is a libc++-specific message table (Part 5). It has
**three** buckets, not two: `Parse`, `Argument`, and `Ambiguous`.

**The fallback is the point.** An unrecognised message degrades to
`Format` — the arc's pre-existing catch-all. So:

> A future libc++ that rewords a message can only make the arc **less
> precise**. It can never make the arc **wrong** about which kind of
> thing failed.

That is the property the arc's contract needs: a diagnostic that
degrades to "unknown" rather than to "wrong".

### The message table must match by PREFIX, not exactly

libc++'s **runtime** messages interpolate the argument's category:

```
The type option contains an invalid value for an integer formatting argument
The type option contains an invalid value for a floating-point formatting argument
The type option contains an invalid value for a string formatting argument
The type option contains an invalid value for a bool formatting argument
The type option contains an invalid value for a character formatting argument
The type option contains an invalid value for a pointer formatting argument
```

Six failures, six different strings, one rule. An exact-match table
would need all six spelled out; the arc stores the **prefix**
`"The type option contains an invalid value for "` and the second
prefix `"The format specifier for "`.

---

## KEY FINDING #2 — ONE MESSAGE, SEVERAL FAILURES (and the scanner is what separates them)

libc++'s runtime parser is **type-directed**: it parses the spec in
the context of the argument's type, and when the spec does not fit
that type it stops early and reports:

```
The format specifier should consume the input or end with a '}'
```

That one string is emitted for **four different failures**. Measured
(Section 6 of the program):

```
unknown type char  "{:q}"   -> The format specifier should consume the input or end with a '}'
precision on an int "{:.2}"  -> The format specifier should consume the input or end with a '}'
sign on a string    "{:+}"   -> The format specifier should consume the input or end with a '}'
locale on a string  "{:L}"   -> The format specifier should consume the input or end with a '}'
```

So the message alone cannot classify anything. **The grammar scanner
is what separates them** — and the resolution rule is short:

> A format string that scans as **well-formed** cannot have failed
> for a syntactic reason. If it failed anyway, the fault is the
> argument's.

That is why the arc's `Ambiguous` bucket resolves to
`FormatArgument` and not to `Format`.

There is a **second** ambiguous message, for a different reason:

```
The width option should not have a leading zero
```

is emitted both for a genuinely malformed spec (`"{:00x}"`, where the
zero-padding flag and the width collide) **and** for a well-formed
spec whose *argument* rejects zero-padding (`"{:0}"` applied to a
string — the `0` falls through to the width parser, which then
rejects the leading zero). Again the scanner tells them apart: the
first scans as malformed, the second scans as well-formed.

Measured over the corpus: **3 ambiguous cells, 3 resolved by the
scanner, 0 left undecided.**

---

## KEY FINDING #3 — the two arc paths have DIFFERENT TAXONOMIES

The compile-time path's format string has already been validated by
`std::format_string`'s `consteval` constructor, so **a runtime
`FormatParse` is unreachable there**. Measured: the compile-time path
produces `FormatArgument` and `Format`, never `FormatParse`.

But there is a subtler consequence, and it is the reason the arc
needs the runtime path at all:

### The stdlib's own messages differ between the two paths

For **the same failure** — a sign option on a `bool`:

| path | message |
| --- | --- |
| compile-time | `The format specifier does not allow the sign option` |
| runtime | `The format specifier for a bool does not allow the sign option` |

The runtime form interpolates the category; the compile-time form
does not. And the sign option has **three** spellings in libc++ 21
depending on which internal check fires first:

```
"The format specification does not allow the sign option"   (compile-time, __parse)
"The format specifier does not allow the sign option"        (compile-time, __validate)
"The format specifier for a bool does not allow the sign option"  (runtime, __validate)
```

**Consequence: a message table calibrated on one path does not
transfer to the other.** The arc's table carries all three.

---

## KEY FINDING #4 — the FALLBACK is load-bearing, and it is not a corner case

There is a third failure mode that is neither a parse error nor an
argument error: **a user's own formatter throws `std::format_error`
from inside `format()`**.

```cpp
template <>
struct std::formatter<petra_test::user_throws> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::user_throws&,
                std::format_context& ctx) const -> decltype(ctx.out()) {
        throw std::format_error("the user's own formatter failed");
    }
};
```

The format string is perfectly well-formed. The argument is fine.
The failure is neither. Without the fallback the classifier would
have to invent a kind; with it, the answer is `Format` — which is
exactly what `Format` has always meant in the arc.

Measured: the user's message survives verbatim in the error, and the
kind is `Format`.

---

## The classifier

```
              format string
                   |
        +----------+-----------+
        |                      |
   scans malformed?        scans well-formed
        |                      |
        v                      v
   FormatParse         message table (libc++-specific)
                              |
        +---------+-----------+-----------+
        |         |           |           |
      Parse    Argument    Ambiguous    Unknown
        |         |           |           |
        |         |           |           |
        v         v           v           v
    (fallback) FormatArgument FormatArgument  Format
```

The `Parse` verdict arriving at the fallback is the "the two
classifiers disagree" case: the message says "parse" but the string
scans as well-formed. The arc does **not** assert a kind it cannot
support — it falls back to `Format`.

---

## The corpus and the cross-check

**33 cells**, 12 valid and 21 failing. Every failing cell's expected
kind is written **by hand from [format.string.std]** — the oracle is
written before the classifier, not derived from it.

The headline sweep (Section 10):

```
sweep: cells=33 disagree=0 byte_fail=0 atomicity_fail=0 bleed_fail=0
       kind_fail=0 kind_expect_fail=0 unknown_msg=0
kinds produced: FormatParse=11 FormatArgument=9 Format=1 TooLarge=0
```

- `disagree=0` — the grammar scanner and the message table reached
  the same verdict on every cell where the message decides.
- `atomicity_fail=0` — every error path still leaves the buffer
  untouched.
- `kind_expect_fail=0` — every kind matches the hand-written oracle.
- `unknown_msg=0` — every message the stdlib produced is in the
  table.
- **`FormatParse=11` and `FormatArgument=9`** — both previously-dead
  kinds are now produced, in quantity.

### The independent cross-check

The verification harness contains a **second implementation** of the
classifier — in Python, written from the standard's grammar and from
the measured libc++ message set, *not* transcribed from the C++. It
re-derives the kind for every failing cell from the format string and
the message alone:

```
crosscheck: cells=33 checked=21 disagree=0
```

**21 of 21 failing cells agree between two independently written
implementations.**

### The three classifiers disagree — and every disagreement is type-directed

The interesting part is where they *do* differ. The scanner sees only
the string; libc++'s parser sees the string **in the context of the
argument's type**. So the scanner cannot know that `"{:.2}"` is
wrong for an `int` but right for a `double`. Every disagreement
between the scanner and libc++ is that kind, and the arc's rule
("a well-formed string that failed anyway is the argument's fault")
is what resolves all of them.

---

## Verification

Everything below was run for real; all artifacts live under
`/tmp/petra20261028/`.

1. **Strict-warning build cells — 7/7 clean.** `default` (`-O0`),
   `-O2`, `noopt`, `-Wshadow`, `-Wconversion -Wsign-conversion`,
   `-Wpedantic`, and `-fsanitize=address -fsanitize=undefined`, each
   compiled with `-std=c++23 -stdlib=libc++ -fexperimental-library
   -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
   -Wsign-conversion`, each required to exit 0 with `fail=0` and
   **empty stderr**.
2. **ASan + UBSan — clean.** Zero diagnostics, empty stderr,
   `fail=0`.
3. **C++20 must be REJECTED.** `std::expected` is a C++23 library
   feature; a C++20 translation unit using it fails to compile, as it
   must be.
4. **Compile-fail probes — 5/5 rejected with the expected
   diagnostic.**
   - `std::format_error::code()` → "no member named 'code'" (there is
     no error-code channel).
   - `std::format_string<int> = "{:f}"` → "not a constant expression"
     (the consteval check).
   - `std::format_string<bool> = "{:+}"` → "does not allow the sign
     option" (the compile-time spelling).
   - `std::format_string<int> = "{"` → the parse error.
   - and one that must be **accepted**: an inline `std::string_view`
     argument.
5. **Runtime probe — the stdlib throws exactly `format_error`.**
   `caught_derived=0 caught_base=1 exact_type=1`: a user's subclass
   of `format_error` catches nothing, and `typeid` says the thrown
   type is exactly `std::format_error`.
6. **Mutation tests — 3/3 mutants killed.** Each was required to make
   the program exit non-zero:
   - **Mutant A**: the classifier always says `Format` (today's lesson
     undone) → the `FormatParse` / `FormatArgument` expectations
     fire.
   - **Mutant B**: Stage A is disabled (`if (false)`) → the scanner no
     longer rescues the ambiguous messages → the `FormatParse`
     expectations fire.
   - **Mutant C**: the `Ambiguous` branch falls back to `Format`
     instead of resolving via the scanner → the `zero-width-on-string`
     cell's oracle fires.
7. **Independent Python cross-check — 21/21 agree** (above).
8. **Message-table coverage — 39 of libc++'s 43 messages.** Measured
   by scanning libc++'s headers and comparing against the table's
   entries (the adjacent C++ string literals are joined first, since
   `"a" "b"` is one message). The **4 uncovered** messages are all
   about `std::format` **handles** — "Not a handle", "Invalid
   argument", "Re-targeting handle not supported", "Handle should use
   `__compile_time_validate_handle_argument`" — and the arc never
   uses a handle, so none of them is a live gap.
9. **Flakiness — 5 consecutive runs are byte-identical.**
10. **CMake — 3/3 pass.** `default`, `ENABLE_STRICT_WARNINGS=ON`, and
    `ENABLE_STRICT_WARNINGS=ON + ENABLE_ASAN=ON` all configure, build
    and run clean, with **0 warnings** in every build log and `fail=0`
    on every run.
11. **No compiled binary is staged.** The lesson directory contains
    only the three intended files (`*.cpp`, `*.md`, `CMakeLists.txt`);
    every build output goes straight to `/tmp`.

**Harness summary: pass=26 fail=0.**
**Program summary: pass=61 fail=0.**

---

## Notes / gotchas worth remembering

- **`std::format_error` is the only error type, and it carries no
  error code.** The failure class is in the **message**, so any
  classification that wants a class has to parse a string.
- **libc++'s runtime messages interpolate the argument's category.**
  Match them by **prefix**, not exactly.
- **The compile-time and runtime messages are different strings for
  the same failure**, and the sign option has three spellings.
  Calibrate a table on both paths.
- **libc++'s runtime parser is type-directed**, so several distinct
  failures share one message. A string-only scanner cannot see the
  difference — that is what the arc's two-stage design is for.
- **A user's formatter can throw `std::format_error`.** That failure
  is neither a parse nor an argument error, so the catch-all kind is
  load-bearing.
- **`std::format_error` is not `final`**, so it is subclassable — but
  nothing in libc++ throws a subclass, so subclassing buys nothing.
- **`std::format_string`'s consteval constructor validates the
  TYPE of a width/precision argument but not its VALUE.** A negative
  dynamic width compiles and fails at runtime ("An argument index may
  not have a negative value").
- **The `consteval` check fires at the call site**, so a compile-time
  path cannot be exercised with a spec the compiler already rejected.
- **`std::vformat_to` needs `format_args`** — use
  `std::make_format_args`, whose arguments must be **lvalues**.
- **`std::make_format_args()` with no arguments is legal** and
  produces "The argument index value is too large..." — which is how
  the `no-argument-supplied` cell works.

---

## What did NOT change (deliberately)

- `petra::FormatError` / `petra::FormatErrorKind` are **verbatim**
  from Sep 6 — the four values, their order and their meanings are
  unchanged. Only the *reachability* of two of them changed.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the
  `TooLarge` contract and the buffer-atomicity behaviour are
  unchanged.
- The arc's success path is unchanged: it still writes exactly what
  it reports, and it still writes no null terminator.
- The compile-time path's **kind set** is narrowed, not widened: it
  can now produce `FormatArgument` where it previously produced
  `Format`, and it can never produce `FormatParse`.
- No pad flags: the pad axis is orthogonal to the error taxonomy, so
  the pad options are NOT reproduced here.

---

## Where we go next

1. **The message table is a libc++-specific artefact, and the
   fallback is what keeps it safe — but how SAFE is it?** Today's
   fallback is "unrecognised message → `Format`". The sharper
   version is to make the fallback *carry* the scanner's verdict
   rather than discard it: if the message is unknown but the string
   scans as malformed, the failure is still a parse failure, and
   `Format` throws that information away. Worth measuring how many
   of libc++'s 43 messages the arc's table actually covers — measured
   today at **39 of 43**, with the 4 uncovered ones all
   handle-related and unreachable from the arc's surface.
2. **The grammar scanner is type-agnostic, and that is both its
   strength and its limit.** It cannot know that `"{:.2}"` is wrong
   for an `int`. A type-aware scanner would need to know each
   argument's category — which the *runtime* path does not have,
   because `std::make_format_args` erases it. Worth measuring
   whether a `constexpr` type-category table is worth the complexity.
3. **The compile-time path's narrower taxonomy is a feature that
   should be USED, not just documented.** If the compiler has already
   validated the string, the arc's runtime path could skip Stage A
   entirely on that path — one less scan per call. Worth measuring
   what that saves.
4. **Revisit the oracle** (carried over from Oct 26's item (4) and
   Oct 27's item (5)) — the hand-written `oracle_select` if-chain is
   still the one place a new pad must be added by hand.
5. **Check whether the matrix has a FIFTH cell** (carried over from
   Oct 26's item (6)) — today's cell [3] has no public stdlib
   counterpart at all.
6. **Compare against `std::formatted_size` + `std::format_to`**
   (carried over from Oct 27's item (4)) — the stdlib has a
   two-function spelling of the arc's two-pass design, and comparing
   would isolate how much of the arc's machinery is libc++'s.
