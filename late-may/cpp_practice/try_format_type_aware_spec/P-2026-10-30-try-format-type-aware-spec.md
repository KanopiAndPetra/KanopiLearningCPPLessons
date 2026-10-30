# P-2026-10-30 — try_format_type_aware_spec

## What today closes

Oct 28's `try_format_error_taxonomy` ended with a "Where we go next"
list. Item (2) is today's lesson, taken **verbatim**:

> **Item (2):** "The grammar scanner is type-agnostic, and that is both
> its strength and its limit. It cannot know that `"{:.2}"` is wrong
> for an `int`. A type-aware scanner would need to know each argument's
> category — which the *runtime* path does not have, because
> `std::make_format_args` erases it. Worth measuring whether a
> `constexpr` type-category table is worth the complexity."

Oct 28 stated the premise and then asked whether the complexity was
worth it. **Measured: the premise is wrong, and the complexity is
small.**

---

## THE HEADLINE — the premise was wrong, and the price is ~40 lines of tables

Oct 28 said the runtime path does not have the argument's category,
"because `std::make_format_args` erases it."

That is true, but it is true **downstream of the arc**:

```cpp
template <typename... Args>
Expected<size_t, FormatError>
try_format_runtime_n_impl(char* out, size_t cap, const char* fmt, Args&... args)
```

`try_format_runtime_n_impl` is a template over `Args...`. The category
of every argument is a **compile-time fact** long before the argument
is handed to `std::make_format_args`. The erasure happens downstream of
the arc, not upstream of it — so the arc never needed to fight it.

The price is one `constexpr` function over the argument's type:

```cpp
template <typename T>
constexpr ArgCategory category_of() noexcept {
    if constexpr (std::same_as<T, bool>)               return ArgCategory::Boolean;
    else if constexpr (std::same_as<T, char>)          return ArgCategory::Character;
    else if constexpr (std::integral<T>)               return ArgCategory::Integral;
    else if constexpr (std::floating_point<T>)         return ArgCategory::FloatingPoint;
    else if constexpr (std::convertible_to<T, std::string_view>) return ArgCategory::String;
    else if constexpr (std::is_pointer_v<T>)           return ArgCategory::Pointer;
    else                                               return ArgCategory::UserDefined;
}
```

Section 1 of the program prints that table **measured**, for 23 types
(here abridged — the program prints all 23, including `short`,
`unsigned`, `long`, `long double`, `char*`, and `void*`):

```
  bool               -> bool
  char               -> charT
  signed char        -> integral
  unsigned char      -> integral
  int                -> integral
  long long          -> integral
  float              -> floating-point
  double             -> floating-point
  const char*        -> string
  char[4]            -> string
  std::string        -> string
  std::string_view   -> string
  void*              -> pointer
  const void*        -> pointer
  std::nullptr_t     -> pointer
  wchar_t            -> user-defined
  char16_t           -> user-defined
  petra_test::point  -> user-defined
```

Two details the table makes visible:

- `char` is `charT`, but `signed char` and `unsigned char` are
  **integral**. That is not a detail the arc gets to choose — it is
  what the standard says, and getting it wrong would mis-classify
  every `signed char` argument.
- A **user-defined** type is `UserDefined`, and the validator
  **abstains** for it: the arc does not know that user's formatter, so
  it must not claim to. Section 6 shows the abstention working: a
  user-defined formatter's output `"(3,4)"` passes through untouched.

---

## KEY FINDING #1 — libc++ validates in TWO PHASES, and the phase-1 mask is decided by the argument's CATEGORY

Reading libc++ 21's `__format/parser_std_format_spec.h` gives the
shape of the real validator. It is not one check; it is three, in
order:

**PHASE 1 (parse).** The category picks the mask of options the
parser will even **CONSUME**. libc++'s masks are literally named after
the categories:

| libc++ mask | used by |
|---|---|
| `__fields_integral` | bool, charT, integral |
| `__fields_floating_point` | float, double, long double |
| `__fields_string` | string |
| `__fields_pointer` | pointer |

An option **outside** the mask is not parsed at all — it is left in the
input, and the leftover text trips the "should consume the input"
check. That is why `"{:+}"` on a `std::string` produces *"The format
specifier should consume the input or end with a '}'"* rather than
*"the sign option is not allowed here"*: libc++ never got as far as
naming the option.

**PHASE 2 (validate).** The **selected presentation type** can narrow
the mask further. libc++ calls `__validate` with `__fields_bool`
= {locale, type} for the "char-like" presentation types. That
narrowing is what rejects `"{:+c}"` on an `int`.

**PHASE 3 (type).** The presentation type itself must be legal for the
category (`__process_parsed_*`).

Section 2 of the program prints the two masks side by side:

```
  category         phase-1 mask (libc++)        standard base mask
  bool             sign alt zero locale         locale
  charT            sign alt zero locale         locale
  integral         sign alt zero locale         sign alt zero locale
  floating-point   sign alt zero prec locale    sign alt zero prec locale
  string           prec                         prec
  pointer          zero                         zero
```

The phase-1 masks are libc++'s; the phase-2 narrowing is libc++'s; but
phase 2 is exactly where libc++ and the standard **disagree** — see
finding #3.

---

## KEY FINDING #2 — the two-phase model reproduces libc++ EXACTLY: 1596/1596

The model is not a rough approximation. Over a grid of **1,596 cells**
— 7 categories × 228 specs, where each spec is one of 12 single-option
prefixes (`""`, `"+"`, `"-"`, `" "`, `"#"`, `"0"`, `"10"`, `".2"`, `"L"`,
`"<"`, `"*>10"`, `"<10.2"`) crossed with each of 19 presentation types
(`""`, `a`, `A`, `b`, `B`, `c`, `d`, `e`, `f`, `F`, `g`, `G`, `o`, `p`,
`s`, `x`, `X`, `P`, `?`) — the model predicts libc++ 21's accept/reject
decision on **every single cell**:

```
  libc++ outcomes:   OK=424  REJECT=1172
  model(libc++ policy) vs libc++:   1596/1596 agree
```

The 1,172 rejects are the interesting half: they are exactly the
cells where libc++ refuses, and the model refuses them **for the right
reason** — the category's phase-1 mask does not contain that option,
or the phase-2 narrowing rejects the presentation type.

---

## KEY FINDING #3 — libc++ 21 and the standard disagree in EXACTLY 10 cells, and it is a known upstream bug

The standard says the sign / alternate-form / zero-padding options are
valid "for arithmetic types other than charT and bool **or when an
integer presentation type is specified**". `c` **is** an integer
presentation type for `int` (Table 107). So `"{:+c}"` on an `int` is
**valid** per the standard.

libc++ rejects it, because phase 2 narrows the mask to `__fields_bool`
whenever the type is `c` — the narrowing that is *right* for `charT` is
applied to `int` as well.

The divergence is exactly **10 cells**: {sign, alternate, zero-padding}
× {`+`, `-`, space, `#`, `0`} × {`int`, `long`}.

```
    int  spec="+c"  libc++ says REJECT: The format specifier for an integer does not allow the sign option
    int  spec="-c"  libc++ says REJECT: ...
    int  spec=" c"  libc++ says REJECT: ...
    int  spec="#c"  libc++ says REJECT: The format specifier for an integer does not allow the alternate form option
    int  spec="0c"  libc++ says REJECT: The format specifier for an integer does not allow the zero-padding option
    long spec="+c"  libc++ says REJECT: ...
    ... (5 more, the same five for long)
```

Tracked upstream as **llvm/llvm-project#106136** and
**microsoft/STL#4918**.

### The independent cross-check — this is what makes it a finding about libc++

"libstdc++ agrees with my model" would not be enough: it could mean
both are wrong the same way. So the same 1,596-cell grid was run under
**libstdc++ 15** — a *different implementation of the same standard* —
and its verdicts are **embedded in the .cpp** (228 characters per
category: `A` = accepted, `R` = rejected, in the same prefix-major /
type-minor order). The program checks the model against them live:

```
  model(standard)      vs libc++:   1586/1596 agree
  model(standard)      vs libstdc++: 1596/1596 agree  <-- INDEPENDENT
```

So the standard-derived model agrees with libstdc++ on **every** cell
and differs from libc++ on **exactly** the 10 cells above. That is
what makes this a finding about libc++ rather than about the model.

**The lesson keeps both policies**, because the arc has to work against
whichever stdlib it is linked against:

- `SpecPolicy::Libcxx21` — libc++ 21's rules. Reproduces the linked
  stdlib exactly.
- `SpecPolicy::Standard` — the portable rules. Matches libstdc++.

---

## KEY FINDING #4 — the type-aware scanner replaces the message table for the whole spec-vs-argument class

Oct 28's classifier had to consult libc++'s **message text** for
**every one** of the 1,172 rejects, because its Stage A scanner is
type-agnostic and therefore never fires on a syntactically valid spec.
The message table was 39-of-43 libc++-specific strings — the arc's
error taxonomy was welded to one implementation's string literals.

Today's scanner classifies **1,172 of 1,172** rejects **structurally**,
reading **zero** libc++ messages:

```
  POLICY = libc++ 21 (the rules of the LINKED stdlib)
    decided STRUCTURALLY (no libc++ message):  1172
    needed the message table (fallback):      0
    agrees with the message table's verdict:  1172
    disagrees with the message table:         0
```

Under the **standard** policy the 10 divergent cells have to fall
back to the message table (the standard allows them, so there is
nothing structurally wrong with the spec to point at) — 1,162
structural, 10 via message, and **1,172/1,172** agreement with the
table's verdict.

**The message table is NOT deleted.** It is **demoted** to the fallback
for failures that are not about the spec's shape at all:

```
  value does not fit the type      fmt={:c}   -> kind=FormatArgument  used_message=yes
  argument index past the end      fmt={5}    -> kind=FormatArgument  used_message=no
  a user's formatter throws        fmt={}     -> kind=Format  used_message=yes
```

Read those three rows carefully — they are the boundary of what a
static validator can know:

1. **The value does not fit the type.** `"{:c}"` with `65` is
   accepted; `"{:c}"` with `300` is rejected with *"Integral value
   outside the range of the char type"*. **Same spec, different value,
   different outcome.** A purely static validator cannot see this, so
   the message table stays as the fallback. (Section 5 measures it.)
2. **The argument index is past the end.** The scanner **does** see
   this one — the argument *count* is known — so it decides it
   structurally, with `used_message=no`.
3. **A user's own formatter throws.** Nothing in the spec says so; the
   validator abstains on user-defined types and the fallback catches
   it, landing on `Format` — the arc's pre-existing catch-all.

---

## KEY FINDING #5 — the compile-time path inherits the divergence

`std::format_string<Args...>` validates at compile time using
libc++'s rules, so on this toolchain `"{:+c}"` on an `int` is a
**compile error** even though the standard says it is valid:

```
ct27.cpp:9:39: error: call to consteval function
  'std::basic_format_string<char, int>::basic_format_string<char[6]>' is not a constant expression
    9 |     const std::format_string<int> f = "{:+c}";
      |                                       ^
/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/__format/parser_std_format_spec.h:454:9: note:
  non-constexpr function '__throw_format_error' cannot be used in a constant expression
  454 |         std::__throw_format_error("The format specifier does not allow the sign option");
```

That is a property of the **linked standard library**, not of the arc
— and it is the strongest possible argument for keeping the arc's own
validator independent of the stdlib's rules. The arc's validator can
say `"{:+c}"` is fine for an `int`; the stdlib cannot.

---

## The classifier

The arc's `FormatErrorKind` (four values, unchanged since Sep 6) is
now fed by a type-aware scanner instead of a type-agnostic one:

```
   format string
        |
        v
   scan_format_string()      -- type-AWARE: for each replacement field,
        |                       resolve the argument's CATEGORY, then
        |                       parse the spec against that category's
        |                       phase-1 mask, then validate the
        |                       presentation type against phase 2.
        |
        +-- structural failure --> FormatParse / FormatArgument
        |                           (decided WITHOUT reading the message)
        |
        +-- no structural failure
                |
                v
        message table (the fallback) --> FormatArgument / Format
```

The scanner is a **two-phase** parse, mirroring what libc++ actually
does:

```cpp
// Phase 1: parse the spec against the CATEGORY's mask.
const ParsedSpec parsed = parse_spec(spec, cat, sf, bad);
if (sf != SpecFailure::None) { /* structural failure */ }

// Phase 2: validate the PRESENTATION TYPE against the narrowed mask.
const SpecVerdict v = validate_spec(cat, parsed, policy);
```

### The grammar is checked BEFORE the mask, and that ordering matters

A precision with no value after the dot (`"{:.}"`) is malformed for
**every** argument — it is a grammar error, so it is a parse failure
however the argument is typed. A **well-formed** precision that the
argument does not accept (`"{:.2}"` on an `int`) is an *argument*
failure: the same spec is fine for a `double`.

The program measures both, and libc++ agrees:

```
  int    {.:}    REJECT  The format specifier should consume the input or end with a '}'
  double {.:}    REJECT  The precision option does not contain a value or an argument index
  string {.:}    REJECT  The precision option does not contain a value or an argument index
  int    {:.2}   REJECT  The format specifier should consume the input or end with a '}'
  string {:.2}   OK      "s"
```

and the arc's classification follows the same distinction:

```
  {:.} on an int  -> kind=FormatParse   [format string: the format spec is malformed]
  {:.2} on an int -> kind=FormatArgument [argument 0: the format spec is not valid for this argument]
```

Note the asymmetry, and it is a *real* asymmetry in libc++: for an
`int` the precision is not in the category's phase-1 mask, so libc++
never parses it and the leftover `.` trips the consume-check; for a
`double` the mask has precision, so libc++ parses it and reports the
missing value directly. The arc distinguishes them where libc++'s
message cannot.

---

## The corpus and the cross-check

- **1,596 cells** = 7 categories × 228 specs. Each spec is one
  single-option prefix × one presentation type, so every option is
  isolated and no cell is ambiguous.
- **Two policies** are kept in the same binary and both are measured:
  libc++ 21's rules and the standard's rules.
- **One independent oracle**: libstdc++ 15's 1,596 measured verdicts,
  embedded in the .cpp and re-checked at every run. The program
  prints the agreement count, so a corrupted embed would be caught by
  the run rather than by a claim in a comment.
- **The libc++ grid is measured live**, not hard-coded: every cell is
  actually run through `std::vformat`, so the "libc++ says" column
  reflects this toolchain's stdlib.

---

## Verification

```
$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
      -Wall -Wextra -O0 -o lesson P-2026-10-30-try-format-type-aware-spec.cpp
(clean — no warnings)

$ ./lesson
...
=== summary ===
  pass=43  fail=0
```

**Strict warnings.** `-Wall -Wextra` produce no diagnostics, and the
repository's own `ENABLE_STRICT_WARNINGS` mode is clean too:

```
$ cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
$ cmake --build build
[100%] Built target P-2026-10-30-try-format-type-aware-spec
$ ./build/P-2026-10-30-try-format-type-aware-spec
=== summary ===
  pass=43  fail=0
```

That strict set is `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`. It caught one real defect on the way
(a loop variable named `d` shadowing the `double d` in the same
function) — fixed by renaming it to `div`.

**Sanitizers.** ASan + UBSan, clean:

```
$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
      -Wall -Wextra -O1 -g -fsanitize=address,undefined \
      -fno-omit-frame-pointer -o lesson_asan \
      P-2026-10-30-try-format-type-aware-spec.cpp
$ ./lesson_asan
=== summary ===
  pass=43  fail=0
(no sanitizer diagnostics on stderr)
```

**The 43 in-program checks** cover: the category table (23 types), the
two masks per category, the phase-1/phase-2/phase-3 split, the grid
counts (1596 cells, 424 OK, 1172 rejects), the three agreement counts
(1596 / 1586 / 1596), the divergence count (10), the classifier's
structural / via-message / agreement / disagreement counts under both
policies, the three fallback classes, the value-dependent case, and the
arc's own end-to-end paths (formatting, the two error kinds, TooLarge,
buffer atomicity, user-defined abstention).

**A transcription bug caught by the embed check.** The libstdc++
verdicts were first embedded by hand, splitting each 228-character
row across several string literals. Two errors followed: the chunks
were given trailing commas (making 21 array elements instead of 7) and
the verification script itself mis-split the array. Both were caught
because the program *measures* the agreement count at runtime — the
first version printed 1011/1596 and failed its own assertion. The
final embed is generated from the TSV programmatically and re-verified
byte-for-byte against it. **A number in a comment is not a check; a
number the program computes at runtime is.**

---

## Notes / gotchas worth remembering

- **The phase-1 mask decides what gets CONSUMED, not just what gets
  accepted.** That is why libc++'s message for a bad option on a
  `std::string` is "should consume the input" rather than "the sign
  option is not allowed" — the option was never parsed.
- **`char` is `charT`; `signed char` and `unsigned char` are
  integral.** A category table that lumps `char` with the other
  integer types mis-classifies every `char` argument.
- **Precision is only meaningful for floating-point and string.** An
  integer category must not grant the precision option — but a
  *malformed* precision is malformed for everyone, so the grammar
  check has to come **before** the mask check.
- **`std::make_format_args` erases the types, but that is the
  stdlib's problem, not the arc's.** The arc's own template sees
  `Args...` before any erasure happens.
- **A static validator cannot see a value.** `"{:c}"` is valid for
  `65` and invalid for `300`. Anything value-dependent has to stay on
  the message-text path.
- **A user-defined formatter is not a category the arc can police.**
  The validator must **abstain**, and fall back, rather than guess.
- **Nested fields have their own numbering rules.** A nested width or
  precision consumes the next automatic argument index, and the
  nested field's numbering mode must match the outer field's (mixing
  them is the mixed-numbering error). A dynamic width argument must
  be a signed/unsigned integer **lvalue**.
- **`{:{}}` (automatic nesting) does not resolve**; `{0:{1}}` (manual
  nesting on both levels) does.
- **The compile-time path and the runtime path can disagree with each
  other**, because only one of them is `consteval`. On this toolchain
  `std::format_string<int>` rejects `"{:+c}"` at compile time.

---

## What did NOT change (deliberately)

- `petra::FormatError` / `petra::FormatErrorKind` — the four kinds,
  their order, and their meanings are exactly as they have been since
  Sep 6.
- The two-pass (measure, then write) design, the
  `counting_output_iterator`, the `bounded_char_writer`, the TooLarge
  contract, and the buffer-atomicity behaviour.
- The **message table itself**. It is demoted from the primary
  classifier of the spec-vs-argument class to the fallback for
  value-dependent and user-defined failures — but it is still there,
  still matching by prefix, and still the safe path for the four
  libc++ messages the arc does not otherwise cover.
- **No pad flags.** The pad axis is orthogonal to the error taxonomy,
  so the pad options are not reproduced here.

---

## Where we go next

1. **Oct 28's item (1) is still open** — the fallback currently
   *discards* the scanner's verdict. Now that the scanner is
   type-aware, its verdict is worth more: a malformed spec should
   stay `FormatParse` even when the message is unrecognised, instead
   of falling back to `Format`. Worth measuring how many of the
   1,172 rejects would classify differently under a fallback that
   **carries** the scanner's verdict rather than replacing it.
2. **The two policies are separate code paths today.** They differ in
   exactly one boolean (does type `c` narrow the integral mask?).
   Worth collapsing them into one mask table with a per-impl
   override table, so adding a third implementation is a data change.
3. **Extend the grid past single options.** Every one of the 1,596
   cells isolates ONE option. The combinations are where the two
   phases interact — e.g. does `"{:+c}"` on a `char` fail at phase 1
   (sign not in the mask) or phase 2 (narrowing)? Worth measuring a
   combination grid.
4. **The compile-time path could use the arc's own validator.**
   `std::format_string` rejects `"{:+c}"` on this toolchain even
   though the standard allows it. If the arc's own `consteval`
   validator replaced it, the arc would accept what the standard
   accepts — and *reject* what the standard rejects even when libc++
   does not. Worth measuring the diagnostic quality of a `consteval`
   failure against `std::format_string`'s.
5. **Oct 28's items (3), (4), (5) and (6) are still open** — the
   compile-time path skipping Stage A, the hand-written
   `oracle_select` if-chain, the fifth matrix cell, and the
   `std::formatted_size` + `std::format_to` comparison.
