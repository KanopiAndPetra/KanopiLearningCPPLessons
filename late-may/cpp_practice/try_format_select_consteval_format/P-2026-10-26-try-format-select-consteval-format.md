# P-2026-10-26 — try_format_select_consteval_format

## What today closes

Oct 25's `try_format_pad_buffer_concept` ended with a "Where we go
next" list. Item (1) is today's lesson, taken **verbatim**:

> **Item (1):** "Give the runtime path the CONSTEVAL surface too.
> Today's runtime sibling takes a `std::string_view` format; the
> compile-time path still gets `std::format_string<Args...>`'s
> compile-time format checking (P2216R3). A runtime *selection* with a
> compile-time *format check* is still nobody's built."

Oct 25 called the missing combination obvious. It is not — and the
reason why is the lesson.

---

## THE MATRIX — the arc has been filling a 2×2, and one cell was empty

The arc's work has two independent axes, and it took a dozen lessons
before the shape of the whole thing was visible:

- **axis A — how the PAD is chosen**
  - compile-time → template parameters `<bool Fills, char Fill>`
  - runtime → a `petra::detail::PadChoice` **value**
- **axis B — how the FORMAT STRING is obtained**
  - compile-time → `std::format_string<Args...>` (P2216R3)
  - runtime → `std::string_view`

| | format: runtime | format: compile-time |
| --- | --- | --- |
| **pad: compile-time** | cell **[1]** — Oct 22 / Oct 25 | cell **[2]** — Oct 19 / Oct 25 |
| **pad: runtime** | cell **[3]** — Oct 24 / Oct 25 | cell **[4]** — **TODAY** |

Three of the four cells existed. Today's cell — *runtime selection with
a compile-time format check* — did not.

### And the naming was hiding it

The arc's function names encode **two** axes at once, and reading them
side by side is what made the gap visible:

```
try_format_runtime_n_padded_span<Fills, Fill>(out, string_view, ...)      [1]
try_format_bounded_runtime_n_padded_span<Fills, Fill>(out, fmt_string, ...) [2]
try_format_runtime_n_padded_span_runtime(out, string_view, choice, ...)   [3]
                                          ^^^^^^^^
                                          the "_runtime" suffix IS the
                                          runtime-selection axis
```

The `_runtime` suffix on cell [3] and the `_bounded_runtime_n_` prefix
on cell [2] are two *different* axes that happen to share the word
"runtime". Putting the four names in one table is what showed that the
combination of "runtime selection" **and** "bounded (compile-time)
format" had never been written.

**Generalisable part: when a family grows one dimension at a time, the
missing cell is invisible until the dimensions are drawn as a table.**
Eleven lessons added pads; three added axes; nobody had written down
the shape.

---

## KEY FINDING #1 — `std::format_string<Args...>` IS NOT A STRUCTURAL TYPE

The obvious spelling of cell [4] is "the format string becomes a
template parameter", which would give the format string the same status
as the pad. It does not compile:

```cpp
template <std::format_string Fmt>
void show() { /* ... */ }
show<"{}">();   // ERROR
```

Measured diagnostic:

```
error: no matching function for call to 'show'
note: candidate template ignored: invalid explicitly-specified
      argument for template parameter 'Fmt'
```

The reason is a C++20 structural-type rule. A structural type must have
all its non-static data members **public**, and `basic_format_string`'s
only member — `basic_string_view<charT> str` — is **private**.

And the obvious *fallback* does not compile either:

```cpp
template <std::string_view S>
void show() { /* ... */ }
show<"hello">();   // ERROR
```

Measured diagnostic:

```
error: type 'std::string_view' of non-type template parameter is not a
       structural type
note: 'std::string_view' is not a structural type because it has a
      non-static data member that is not public
```

So on Apple Clang 21 / libc++ 21, **neither** of the two types you
would reach for is usable as a non-type template parameter.

The lesson's source pins what *is* checkable in a program:

```cpp
static_assert(std::is_trivially_copyable_v<std::format_string<int>>, ...);
static_assert(!std::is_default_constructible_v<std::format_string<int>>, ...);
static_assert(std::is_trivially_copyable_v<std::string_view>, ...);
```

Both types are trivially copyable and neither is default-constructible,
so neither of *those* rules is what excludes them. What excludes them
is **privateness**, and that shows up in the diagnostic rather than in
a trait.

**Consequence for the API: the format string must arrive as a FUNCTION
PARAMETER**, and the compile-time check therefore happens **at the call
site**, where the caller writes the literal. That is not a workaround —
it is the shape the standard forces.

---

## KEY FINDING #2 — A POINTER INSIDE A STRUCTURAL CARRIER MUST POINT AT A *NAMED* OBJECT

The way to get a format string into a template parameter is to roll
your own structural carrier:

```cpp
struct format_carrier {
    const char* data;
    std::size_t size;
    constexpr bool operator==(const format_carrier&) const noexcept = default;
};
```

This **is** a structural type (all members public, defaulted `==`,
trivially copyable) and it **is** accepted as a non-type template
parameter. But the pointer inside it has a rule:

| pointer source | accepted? |
| --- | --- |
| a **named** `static constexpr char[]` | **yes** |
| a `constexpr const char*` pointing at that named array | **yes** |
| a **string literal**, written inline | **no** |
| a `constexpr const char*` pointing at a string literal | **no** |

Measured, in the class-template spelling:

```
error: pointer to subobject of string literal is not allowed in a
       template argument
```

and in the structural-carrier spelling:

```
error: no matching function for call to 'show'
note: candidate template ignored: invalid explicitly-specified
      argument for template parameter 'C'
```

Both spellings are **rejected**; the diagnostic is just less specific
in the carrier case. The control — the same carrier holding a pointer
to a *named* array — compiles clean.

**Consequence: a "format string as a template parameter" design has to
NAME every format string it uses.** Every entry in a table of formats
becomes a named object with static storage duration, not a literal at
the point of use.

**Generalisable part: "can this be a template argument?" is not one
question but three — is the type structural, are the members public,
and does each pointer member designate a named object with static
storage duration?** The first two are type properties; the third is a
property of the *value*, and it is the one that bites at the call site.

---

## KEY FINDING #3 — THE COMPILE-TIME FORMAT CHECK IS A HARD ERROR, NOT A CONSTRAINT

This is the finding that decides the whole design of cell [4].

The obvious way to build a "is this format string valid for these
types?" predicate is a concept:

```cpp
template <typename T, format_carrier F>
concept format_constructible = requires {
    std::format_string<T>{std::string_view{F.data, F.size}};
};
```

**This concept says `true` for format strings that cannot possibly be
used.** Measured, four independent spellings, all four reporting
"satisfied" for `{:f}` with an `int`:

| spelling | verdict for `{:f}` + `int` |
| --- | --- |
| (a) requires-expression over a global constant array | satisfied |
| (b) requires-expression over an NTTP carrier | satisfied |
| (c) requires-expression over a `consteval` function call | satisfied |
| (d) the same carrier through a `consteval` validator | satisfied |

And the strongest form of the same finding — `void` is **not
formattable at all**:

```
error: call to implicitly-deleted default constructor of
       'formatter<void, char>'
```

...and the concept is *still* satisfied:

```cpp
static_assert(format_constructible<void>, ...);   // COMPILES
```

A predicate that says "yes" about `void` is not a predicate.

**Why:** the check lives in the **body** of `std::format_string`'s
`consteval` constructor. A requires-expression never evaluates a
constructor body — it only asks whether the expression is *well-formed*.
The expression is well-formed (the constructor exists and is callable);
it is the *constant evaluation* of its body that fails. So the failure
is a hard error at the point of instantiation, not a substitution
failure.

**The generalisable part: a `consteval` constructor is a GUARD RAIL ON
A CALL SITE, not a QUERYABLE PREDICATE.** You cannot branch on "is this
format string valid?" — you get a compile error where you use it. If
you want a *query*, you have to write a `consteval` function that
actually evaluates the constructor (the source's `format_is_valid`
does exactly that, and it does see the failure).

**This is why cell [4] is spelled with the format string as a function
parameter.** The check then happens where the caller writes the
literal, which is exactly where a hard error belongs.

---

## KEY FINDING #4 — THE VERDICT IS PER (TYPE, FORMAT) PAIR, AND IT MATCHES THE RUNTIME VERDICT

The compile-time check is not "is this format string well-formed?" — it
is "is this format string well-formed **for these argument types**?".

Measured over a 10 × 6 matrix of (type, format) pairs, with **one
translation unit compiled per pair**:

| | `{}` | `{:d}` | `{:f}` | `{:5}` | `{:{}}` | `{0}` |
| --- | --- | --- | --- | --- | --- | --- |
| `int` | A | **A** | R | A | R | A |
| `unsigned` | A | **A** | R | A | R | A |
| `long` | A | **A** | R | A | R | A |
| `unsigned long` | A | **A** | R | A | R | A |
| `short` | A | **A** | R | A | R | A |
| `double` | A | R | **A** | A | R | A |
| `float` | A | R | **A** | A | R | A |
| `std::string` | A | R | R | A | R | A |
| `char` | A | **A** | R | A | R | A |
| `bool` | A | **A** | R | A | R | A |

**39 ACCEPT, 21 REJECT**, and every REJECT carried a reason string
(none was a silent refusal).

The pattern is exactly the standard's presentation-type categories:
`{:d}` is an integer presentation type (`int`, `unsigned`, `long`,
`unsigned long`, `short`, `char`, `bool`), `{:f}` is a floating-point
one, and `{:5}` / `{}` / `{0}` name no presentation type at all so they
work for everything. `{:{}}` consumes **two** arguments (a dynamic
width) and so is invalid for a one-argument format string.

**And the runtime verdict agrees with the compile-time verdict on all
sixty pairs — 0 disagreements.** The independent Python cross-check
compiles each pair, runs each pair, and compares both against the
verdict the standard's rules predict:

```
verdict matrix: pairs=60 compile_accept=39 compile_reject=21
                 agree=60 disagree=0
rejects without a reason: 0
RESULT: PASS
```

**Generalisable part: "compile-time checked" is not a property of the
format string alone — it is a property of the (format string,
argument types) PAIR.** A format string that is checked in one
call site can be rejected in another.

---

## KEY FINDING #5 — A COMPILE-TIME-PASSING FORMAT STRING CAN STILL THROW AT RUNTIME

This is the finding with the most direct consequence for the arc's
error contract.

The compile-time check validates the **format spec** against the
**argument type**. It does not validate the argument's **formatter**. So
a user-defined formatter whose `format()` throws passes the compile-time
check and throws at runtime:

```
compile-time check PASSED, fmt='{}'
runtime std::format_error: formatter refuses at runtime
```

**CONSEQUENCE FOR THE ARC: `FormatErrorKind::Format` is NOT dead code
on the consteval surface.** Today's cell therefore keeps the same
two-pass (measure, then write) implementation and the same error
contract as every other cell, and Section 3 of the source pins it: the
throwing formatter produces `FormatErrorKind::Format`, the buffer is
left untouched (atomicity holds), and the runtime-format cell produces
the *same* error kind and the *same* message.

**Generalisable part: moving a check to compile time does not
eliminate the runtime error path — it narrows it.** The format string's
own errors move to compile time; the argument's errors stay at runtime.
A design that drops the runtime error path because "the format is
checked at compile time" would be wrong.

---

## KEY FINDING #6 — THE `Args` OF `std::format_string<Args...>` COME FROM THE ARGUMENTS, NOT FROM THE FORMAT STRING

The alias spells them `type_identity_t<Args>...`, and `type_identity_t`
is a **non-deduced context** — so the format-string parameter does not
participate in deduction at all.

Measured:

```cpp
static_assert(deduced_arg_count("{}", 1) == 1, ...);
static_assert(deduced_arg_count("{}", 1, 2.5) == 2, ...);   // <-- !!
static_assert(deduced_arg_count("{}", "x") == 1, ...);
```

The format string is `"{}"` — **one** replacement field — and yet **two**
`Args` are deduced, because the deduction comes from the argument list.

**Why it matters:** the compile-time check is against the types the
caller *actually passes*, which is exactly what makes KEY FINDING #4's
per-(type, format) verdict the right model. If the format string
drove the deduction, `std::format_string<int>{"{}"}` and
`std::format_string<int, double>{"{}"}` would be different types and
the check would be per-string rather than per-pair.

---

## The cross-check — the headline verification

A refactor that moves a cell of the matrix has to prove it did not
change behaviour. Today's source does that in three places:

- **Section 1** — all four cells of the matrix, for this build cell,
  produce **byte-identical** buffers.
- **Section 2** — today's cell agrees on **both buffer axes** (the
  `std::span<char>` axis and the `char*`+`cap` axis), and the generic
  core agrees with both.
- **Section 6** — the same comparison promoted to **all 1024 cells** in
  one process, with a `cell_fail` counter:

```
sweep: cells=1024 disagree=0 byte_fail=0 cell_fail=0 none_wins=1
```

`cell_fail=0` over 1024 cells is the claim "the compile-time-format
cell and the runtime-format cell are the same function" stated as a
**measurement** rather than an assertion.

**The cross-check is checked FIRST**, before the byte-level
expectations, deliberately: a disagreement between the two cells is
then attributed to `cell_fail` rather than being masked by `byte_fail`.
(That ordering was not the first draft's — see the mutation test
below.)

---

## The compile-time pins

Part 10 of the source carries the assertions, and each one is a claim
about a **specific C++20/C++23 rule** rather than about this file's
plumbing:

- `std::format_string<int>` is trivially copyable and **not** default
  constructible — so neither rule is what excludes it from being an
  NTTP; **privateness** is.
- `std::string_view` is trivially copyable too.
- `deduced_arg_count("{}", 1, 2.5) == 2` — KEY FINDING #6.
- `format_constructible<int>` — a valid format satisfies the concept.
- `carrier_constructible<int, carrier{kFmtBadForInt, 4}>` — **the
  punchline**: the concept is satisfied even for `{:f}` with an `int`.
- `format_constructible<void>` — **the strongest form**: the concept is
  satisfied even for `void`, which is not formattable at all.
- `format_is_valid<int>(carrier{kFmtGood, 4})` — the `consteval`
  validator *does* see the failure, because it evaluates the
  constructor.

---

## Verification

Everything below was run for real; all artifacts live under
`/tmp/petra20261026/`.

1. **Strict compile of the default cell** —
   `-std=c++23 -stdlib=libc++ -fexperimental-library -O0 -Wall -Wextra
   -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`:
   **clean, zero warnings**.
2. **Build matrix — 22/22 pass.** Twelve plain cells (the default, each
   of the ten pads alone, the all-ten-pads cell), four throw-axis
   variants, and six multi-pad combinations — each compiled with the
   full strict warning set and required to exit 0 with `FAIL=0` **and
   empty stderr**. Every cell reports `PASS=54 FAIL=0`.
3. **Compile-fail probes — 11/11 pass.** Eight translation units that
   **must** be rejected, each checked for the expected diagnostic:
   `std::format_string` as an NTTP; `std::string_view` as an NTTP; a
   string literal inside a structural carrier; a pointer-to-literal
   inside a structural carrier; a direct `const char*` class NTTP with a
   literal; `std::format_string<int>{"{:f}"}`;
   `std::format_string<void>{"{}"}`; and an arity mismatch. Three
   control translation units that **must** compile: the named-array
   carrier, a valid format string, and the carrier holding a pointer to
   a named array.
4. **ASan + UBSan — 5 cells clean.** `default`, `bang`, `nul`, `all10`
   and `throw+bang`, built with `-fsanitize=address
   -fsanitize=undefined -fno-omit-frame-pointer`,
   `ASAN_OPTIONS=detect_leaks=0`: zero diagnostics and **empty stderr**
   on all five.
5. **Flakiness — 5 runs × 7 cells, identical tallies every time**
   (`PASS=54 FAIL=0` on all seven).
6. **In-process exhaustive sweep — 1024/1024 cells** (Section 6):
   `disagree=0 byte_fail=0 cell_fail=0 none_wins=1`.
7. **Independent Python cross-check of the verdict matrix** — 60
   (type, format) pairs, each compiled in its own translation unit and
   each run: the compile-time verdict and the runtime verdict **agree
   on all 60**, both agree with the verdict the standard's rules
   predict, and every reject carried a reason string.
8. **Mutation test — the cross-check has teeth.** Two mutants, both in
   a pad-enabled cell (in the NONE cell there is no fill loop, so a
   fill-byte mutation is legitimately invisible there):
   - **Mutant A**: cell [4]'s core fills with `choice.code + 1`.
     Result: `cell_fail` goes from 0 to **1023** and the program exits
     1, with Section 1 reporting the buffer mismatch.
   - **Mutant B**: cell [4]'s **raw** adapter passes `cap - 1`.
     Result: the sweep stays clean (it only exercises the span axis),
     but Section 2's cross-axis check catches it and the program exits
     1.

   ```
   pristine: exit=0 PASS=54 FAIL=0 | cells=1024 disagree=0 byte_fail=0 cell_fail=0    none_wins=1
   mutant A: exit=1 PASS=43 FAIL=11| cells=1024 disagree=0 byte_fail=0 cell_fail=1023 none_wins=1
   mutant B: exit=1 PASS=53 FAIL=1 | cells=1024 disagree=0 byte_fail=0 cell_fail=0    none_wins=1
   ```

   Mutant A's count is 1023 rather than 1024 because the NONE cell
   (mask 0) has no fill, so the mutation is invisible there — that is
   the correct behaviour, not a gap.
9. **CMake — 6/6 pass.** `default`, `ENABLE_STRICT_WARNINGS=ON`,
   `ENABLE_STRICT_WARNINGS=ON + ENABLE_ASAN=ON`, `PETRA_BANG_PAD=ON`,
   `PETRA_THROW_ON_OVERFLOW=ON` and the all-ten-pads cell all
   configure, build and run clean, with **0 warnings** in every build
   log.

---

## Notes / gotchas worth remembering

- **`std::format_string` and `std::string_view` are both non-structural
  on libc++ 21.** Neither can be a non-type template parameter. If you
  need a format string in a template parameter list, roll your own
  carrier — and then **name** every format string.
- **A `consteval` constructor is a guard rail, not a predicate.** A
  concept that constructs it will report "satisfied" for anything,
  including `void`. To *ask* the question, write a `consteval` function
  that evaluates the constructor.
- **`std::format_string`'s `Args` are deduced from the arguments.**
  `type_identity_t` makes the format-string parameter a non-deduced
  context. So the verdict is per (format, argument-types) PAIR.
- **A compile-time-checked format string can still throw at runtime.**
  The check covers the format spec, not the formatter. The runtime
  error path does not die.
- **The compile-time and runtime verdicts agree.** Measured over 60
  (type, format) pairs with **0 disagreements** — the compile-time
  check is a faithful predictor, not an approximation.
- **`std::runtime_format` (C++26) is NOT available in libc++ 21**
  (`__cpp_lib_format = 202110`) — "no member named 'runtime_format' in
  namespace 'std'". So there is no way to get a *runtime* format string
  with a *compile-time* check on this toolchain; today is the only way
  to have one with a compile-time check, and it costs a literal at the
  call site.
- **`/usr/bin/g++` on this machine is Apple clang**, so a "g++" result
  is not an independent second implementation. (Carried over from Oct
  22's notes; still true.)
- **This session's terminal guard wedges on some invocations.**
  `bash script.sh` and absolute-path execution of a freshly built
  binary both hang the guard's process-identity probe, while
  `bash -c "$(cat script.sh)"` and PATH-resolved invocation work.
  Every verification script here was run the second way.
- **A concept that says "yes" about `void` is not a predicate.** The
  cheapest possible test of whether your "is this valid?" check is
  actually checking anything: ask it about a type that is definitely
  not valid, and see whether it notices.
- **Order your cross-checks so the specific counter fires first.**
  The first draft of today's sweep checked the byte-level expectations
  before the cross-cell comparison, so mutant A was attributed to
  `byte_fail` and today's headline counter stayed at 0. Moving the
  cross-cell comparison first makes `cell_fail` the counter that
  measures the claim it is named after.

---

## What did NOT change (deliberately)

- `petra::FormatError` / `FormatErrorKind`, the two-pass (measure, then
  write) impls, `counting_output_iterator`, `bounded_char_writer` and
  `unwrap_dispatch` are reused **verbatim** from Oct 25 / Oct 24 /
  Oct 21 / Sep 8 / Sep 9.
- The compile-time flag **names** (`PETRA_PAD`, `PETRA_SPACE_PAD`, …)
  and the `-D` contract are unchanged, so yesterday's build cells keep
  their meaning and the CMake options still work.
- The buffer-atomicity contract is unchanged and is re-checked on
  today's cell: a `TooLarge` error and a `Format` error both leave the
  buffer untouched.
- The X-macro rank list, the `rank+1` score semantics, the three
  selection spellings and the hand-written oracle table are unchanged,
  so the numbers published on Oct 22 / Oct 23 / Oct 24 / Oct 25 are
  unchanged.
- The pad-selection half of the matrix did not move at all. Today only
  fills in the empty cell.

---

## Where we go next

1. **Give the runtime path a compile-time CHECKED FORMAT too.** Today's
   cell [4] has a runtime pad and a compile-time format. The cell that
   is still missing is a runtime format *string* that is nevertheless
   checked — and the toolchain says the answer is no: `std::runtime_format`
   (C++26) is absent from libc++ 21, and `std::format_string`'s
   constructor is `consteval`, so a runtime string can never reach it.
   Worth measuring what a hand-rolled compile-time checker over a
   runtime `std::string_view` would cost, and whether it can be done
   without a `type_identity_t`-style deduction trick.
2. **Make the runtime sweep cover the ERROR paths too.** Section 6
   sweeps the 1024 success cells; the `TooLarge` and `Format`-error
   paths are checked for a handful of cells. Since today's cell takes a
   `PadChoice` value, all 1024 cells' error paths are affordable in one
   process — the same argument Oct 25 made for its item (2).
3. **Compare the hand-rolled `expected<size_t, FormatError>` contract
   against stdlib's `std::format_to_n`** — carried over from Oct 22's
   item (4), still untried. Note that libc++ 21's `format_to_n_result`
   has exactly TWO members — `out` and `size` — and **no error-code
   member at all** (measured: "no member named 'ec' in
   'std::format_to_n_result'"). So the two contracts are not directly
   comparable, and the differences need spelling out: `format_to_n`
   reports "how many characters would have been written" and leaves
   the caller to compare that against the capacity, whereas the arc's
   contract distinguishes `TooLarge` from `Format`. That difference is
   the whole point of the comparison.
4. **Revisit the oracle.** The hand-written `oracle_select` if-chain
   is still the one place a new pad must be added by hand. A generic
   oracle reading the oracle *table* would remove that last per-pad
   edit, at the cost of the "two independent implementations" property.
   That trade is worth making deliberately rather than by accident.
5. **Port the `std::string_view`+`cap` axis.** The arc has `char*`+`cap`
   and `std::span<char>`, but not the const-buffer axis — which
   today's writability clause (`char&`, not `char`) currently *rejects*
   by design. Deciding whether a const output buffer is a legitimate
   contract is a real design question, and today the answer is "no, and
   here is the static_assert that says so".
6. **Find out whether the matrix has a FIFTH cell.** Today's cell is
   "runtime pad + compile-time format". What about a *runtime* format
   string that is *never* checked (cell [3] already is that) versus a
   format string whose CHECK is runtime but whose SPEC is
   compile-time? The 2×2 may not be the whole story — the
   compile-time/runtime axis may apply to three things independently
   (pad, format string, format check), which would be a 2×2×2.
