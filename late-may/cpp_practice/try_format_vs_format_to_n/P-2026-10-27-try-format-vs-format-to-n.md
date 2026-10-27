# P-2026-10-27 — try_format_vs_format_to_n

## What today closes

Oct 26's `try_format_select_consteval_format` ended with a "Where we
go next" list. Item (3) is today's lesson, taken **verbatim** — and
item (3) was itself carried over from Oct 22's item (4), so this
closes a thread that has been open for five lessons:

> **Item (3):** "Compare the hand-rolled `expected<size_t,
> FormatError>` contract against stdlib's `std::format_to_n` —
> carried over from Oct 22's item (4), still untried. Note that
> libc++ 21's `format_to_n_result` has exactly TWO members — `out` and
> `size` — and **no error-code member at all** (measured: "no member
> named 'ec' in 'std::format_to_n_result'"). So the two contracts are
> not directly comparable, and the differences need spelling out."

Oct 26 predicted the comparison would fail because the two result
types have different shapes. It does fail — but not for the reason
Oct 26 expected, and the interesting part is *where* it fails.

---

## THE HEADLINE — the arc's atomicity is an ARTIFACT, not a design

The arc has always claimed a **buffer-atomicity contract**: on an
error, the output buffer is left untouched. Oct 26 stated it as
"the buffer-atomicity contract is unchanged and is re-checked on
today's cell: a `TooLarge` error and a `Format` error both leave the
buffer untouched."

Today measured whether that guarantee is a property of the arc's
**two-pass design** (measure, then write) or of something else
entirely.

### The mechanism (read out of libc++ 21's own source)

libc++ picks a buffer class per output iterator
(`__format/buffer.h`, `__buffer_selector`):

| output iterator | buffer selected | behaviour |
| --- | --- | --- |
| `char*` / `__wrap_iter<char*>` | `__direct_iterator_buffer` | writes **directly** into the user's memory, character by character, as the format proceeds |
| anything else | `__iterator_buffer` (allocating) | accumulates the **whole** output in a 256-byte stack buffer (growing to the heap), then copies it out **once, at the very end** |

The concept that picks between them is `__enable_direct_output`, and
it is TRUE only for `char*`.

The arc's `bounded_char_writer` is a **custom** iterator, so libc++
routes it through the **allocating** buffer. A formatter that throws
mid-format therefore never reaches the copy-out step, and the user's
buffer is untouched.

**But that is a side effect of libc++'s buffer CHOICE, not a property
of the arc's two-pass design.**

### The control experiment

Same two-pass shape, same formatter, **only the sink differs**:

```
(a) arc's bounded_char_writer sink (custom iterator) : buffer "QQQQQ"
(b) same shape, plain char* sink                    : buffer "BAD9Q"
(c) std::format_to_n (char*)                        : buffer "HALF9Q"
```

(a) is clean; (b) and (c) are dirty. The write pass had already
started in all three — the formatter wrote its bytes and *then*
threw. The only difference between (a) and (b) is the sink's TYPE.

So: **the stdlib's `format_to_n`, whose entire purpose is to write
into a `char*`, does not have the arc's atomicity.** And the arc's
atomicity is implementation-dependent — it is not in the C++
standard, and it would evaporate if the arc's sink were `char*`.

### The experiment that nearly hid this

The first draft of this lesson used a formatter that **threw on every
call**. That formatter is caught by the **measure** pass, so the write
pass never ran — and the control showed *no difference at all*, which
looked like a refutation of the finding.

The fix: a formatter that **succeeds on the measure pass** (writing
nothing) and **writes-then-throws on the write pass**, so the write
pass is actually reached. That is `petra_test::throws_second` in the
source.

**Generalisable part: a control experiment that fails to produce a
difference is ambiguous — it may mean "no effect" or it may mean "the
effect never got a chance to happen".** Always check that the
interesting code path was reached.

---

## KEY FINDING #1 — the stdlib's result has NO error channel

`std::format_to_n_result<OutIt>` has exactly **two** members: `out`
(an iterator) and `size` (a count). Measured:

- a detection idiom for a member named `ec` reports **false**;
- `sizeof(std::format_to_n_result<char*>) == 16` — one pointer and one
  `ptrdiff_t`, and nothing else.

The arc's `std::expected<std::size_t, FormatError>` has an error
channel. So the two contracts cannot be compared member-for-member.
The stdlib has nowhere to put an error, which is why it:

- reports overflow **by convention** (`size` = bytes that *would have*
  been written), and
- reports a bad format string by **throwing** `std::format_error`.

Oct 26's note was right about this. The lesson is what it costs to
build an adapter (below).

---

## KEY FINDING #2 — the two "size"s mean DIFFERENT THINGS

This is the single most important thing to get right when translating
between the contracts.

- The arc's success value = bytes **WRITTEN**.
- The stdlib's `size` = bytes that **WOULD HAVE BEEN WRITTEN**.

They coincide exactly when the output fits, and the moment it does
not fit, **one of them becomes an ERROR and the other stays a
NUMBER**. Measured (cap 3, output `"12345"`):

```
arc    -> unexpected(TooLarge), needed_bytes = 5, buffer UNTOUCHED
stdlib -> {out = buf+3, size = 5},             buffer = "123"
```

---

## KEY FINDING #3 — the stdlib TRUNCATES, the arc REFUSES

Byte-for-byte on the same input:

| cap | arc buffer | stdlib buffer |
| --- | --- | --- |
| 3 | `"QQQ"` (nothing written) | `"123"` (partial write) |
| 5 | `"12345"` | `"12345"` |

The two agree on the **success** path and disagree on the **failure**
path — and they disagree about the **BUFFER**, not merely about the
return value.

The adapter in the source (`petra::detail::stdlib_format_to_n`) has
to *construct* the error channel the stdlib lacks: a thrown
`std::format_error` becomes `FormatErrorKind::Format`; `size > cap`
becomes `FormatErrorKind::TooLarge`. Note what the adapter **cannot
recover**: the bytes the stdlib already wrote before it discovered
the overflow.

---

## KEY FINDING #4 — the arc's atomicity is not uniform WITHIN the arc

There are four distinct error paths, and only three of them leave the
buffer untouched:

| path | detected when | buffer |
| --- | --- | --- |
| (a) TooLarge | **measure** pass, before any write | untouched — **structural** |
| (b) Format error | **measure** pass, before any write | untouched — **structural** |
| (c) Format error | **write** pass, mid-write | untouched — **only because of the buffering artifact** |
| (d) the post-write consistency check | **after** the write | **MODIFIED** |

(d) is the one that surprises. The arc's write pass ends with:

```cpp
if (written != needed) {
    return std::unexpected(FormatError{FormatErrorKind::TooLarge,
        "bounded_char_writer wrote N bytes; measure pass said M", needed});
}
```

That check fires **after** the write has already happened, so the
buffer holds the written bytes while the call reports an error.

It is reachable only with a **non-deterministic** formatter — one
whose output depends on its invocation count — because the arc runs
the formatter twice (KEY FINDING #5). Measured with `petra_test::flaky`
("OK9" on call 1, "BAD9" on call 2):

```
arc verdict   : error
arc error kind: TooLarge
arc message   : bounded_char_writer wrote 4 bytes; measure pass said 3
arc buffer    : "BAD9Q"     <-- MODIFIED
```

So the arc reports a **capacity** error caused by the *formatter*,
not by the buffer.

---

## KEY FINDING #5 — the arc calls the formatter TWICE; the stdlib ONCE

Measured with a formatter that counts its own invocations:

```
arc    formatter invocations = 2     (measure pass + write pass)
stdlib formatter invocations = 1
```

That is the price of the arc's contract: the measure pass exists so
that `TooLarge` can be reported **without** writing. It is not free,
and it is not invisible.

---

## KEY FINDING #6 — the stdlib's `n` is SIGNED, and a negative `n` is silently accepted

`format_to_n`'s second parameter is `iter_difference_t<OutIt>` =
`ptrdiff_t` — **signed**. Measured:

```cpp
std::format_to_n(buf, -1, "{}", 12345);
//   out advanced by 0, size = 5, buffer untouched, NO diagnostic
```

A negative capacity means **"capacity zero"**, not "error". The
arc's `cap` is `std::size_t` — **unsigned** — so a negative capacity
is not representable at all.

This is a genuine safety difference, and it is invisible in the type's
spelling: both are "a number of bytes".

---

## KEY FINDING #7 — the arc's error taxonomy looks richer and is actually poorer

`petra::FormatErrorKind` has **four** values — `FormatParse`,
`FormatArgument`, `Format`, `TooLarge` — but the implementation only
ever produces **two** of them. Every `std::format_error` becomes
`Format`, whatever its cause. Measured over four distinct stdlib
failure classes:

| failure | stdlib message | arc kind |
| --- | --- | --- |
| presentation type (`{:f}` + int) | "The type option contains an invalid value for an integer formatting argument" | `Format` |
| arity too large (`{} {}`, 1 arg) | "The argument index value is too large for the number of arguments supplied" | `Format` |
| argument index (`{5}`) | "The argument index value is too large for the number of arguments supplied" | `Format` |
| unmatched brace (`{`) | "The format string terminates at a '{'" | `Format` |

So the stdlib's error *type* is coarser (one exception) but its error
*information* is finer (four distinct messages). The arc keeps the
message but collapses the class into the kind — so the distinction is
**present in the message and absent from the kind**.

---

## KEY FINDING #8 — the public stdlib API is cell [2] only

The arc's 2×2 matrix has a runtime-format cell ([3]) that the public
stdlib **cannot reach**: `std::format_to_n` requires
`std::format_string<Args...>`, and there is **no** public
`std::vformat_to_n` on this toolchain (measured: "no member named
'vformat_to_n' in namespace 'std'").

So the arc's cell [3] has no public standard-library counterpart at
all, and the comparison this lesson makes is possible **only because
the arc already has a runtime-format path**.

Consequence: on the compile-time-format cell the stdlib's format
errors are **compile-time** errors, so `std::format_to_n`'s only
runtime failure mode is "a user formatter threw". The arc's
runtime-format cell additionally has "the format string is invalid"
and "the output does not fit" as runtime outcomes.

---

## KEY FINDING #9 — the consteval check is on the VALUE, not the TYPE's spelling

`std::format_to_n`'s third parameter is `std::format_string<Args...>`,
whose constructor is `consteval`. So the format string must be a
**constant expression**. Measured:

| spelling | verdict |
| --- | --- |
| an **inline** `std::string_view("{}")` | **ACCEPTED** — the literal is a constant expression, so the `consteval` constructor can be evaluated |
| a `std::string_view` arriving as a **function parameter** | **REJECTED** — "call to consteval function ... is not a constant expression" |

So "runtime format string" is **not a property of the type** — it is a
property of the **value**. The stdlib's bounded API will happily take
a `std::string_view`; it just has to be one the compiler can see is
constant.

This is Oct 26's KEY FINDING #3 seen from the other side: a
`consteval` constructor is a guard rail that fires at the **call
site**, and what it guards is *constant-evaluability*, not a type
category.

The first draft of this lesson's verification harness assumed the
type spelling decided it and wrote a probe that passed an inline
`std::string_view` expecting a rejection. It compiled. Both spellings
are now probed.

---

## The contract comparison

| aspect | arc (`expected`) | stdlib (`format_to_n`) |
| --- | --- | --- |
| error channel | yes (`FormatError`) | **NO** (throws `format_error`) |
| overflow | error `TooLarge` | `size > n`, no error |
| buffer on overflow | untouched | **PARTIAL WRITE** |
| buffer on format error | untouched* | **PARTIAL WRITE** |
| success value | bytes **WRITTEN** | bytes **WOULD-HAVE-BEEN** |
| capacity type | `size_t` (unsigned) | `ptrdiff_t` (**SIGNED**) |
| negative capacity | not representable | silently = 0 |
| formatter invocations | 2 (measure + write) | 1 |
| runtime format string | yes (cell [3]) | **NO public API** |
| compile-time check | only on cell [2] | always |

\* except the post-write consistency check, which fires after the write.

---

## The cross-check — the headline verification

Section 11 sweeps **5 format/argument pairs × 10 capacities = 50
cells**. Each cell is scored on **five independent axes**, each with
its own counter, so a failure names the claim it broke:

```
sweep: cells=50 disagree=0 byte_fail=0 atomicity_fail=0
       bleed_fail=0 needed_fail=0 three_way_fail=0
```

- `disagree` — the two contracts reached different verdicts
- `byte_fail` — both succeeded but the bytes differ
- `atomicity_fail` — the arc reported an error but modified the buffer
- `bleed_fail` — the arc succeeded but wrote past its reported count
- `needed_fail` — both failed but disagree on the needed byte count
- `three_way_fail` — the arc's runtime-format and compile-time-format
  paths disagree with each other

`atomicity_fail=0` over 50 cells is the claim "the arc's error paths
leave the buffer alone" stated as a **measurement** — and it is a
measurement with teeth, because KEY FINDING #4 shows one path (the
post-write consistency check) that does **not** leave the buffer
alone, and it is exercised separately in Section 7.

### A metric bug worth recording

The first draft scored "did the arc write past its reported count?"
using the index of the **first** modified byte. That index is **0 for
any written output** — the first written byte is by definition not
the poison — so the metric fired on 32 of 50 perfectly correct cells
(`bleed_fail=32`).

The question "is every byte at index >= `written` still untouched?"
needs the **last** modified index. Fixed, and the source says so.

---

## Verification

Everything below was run for real; all artifacts live under
`/tmp/petra20261027/`.

1. **Strict build cells — 4/4 clean.** `default`, `-Wshadow`,
   `-Wconversion -Wsign-conversion`, and `-Wpedantic`, each compiled
   with `-std=c++23 -stdlib=libc++ -fexperimental-library -O0 -Wall
   -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`,
   each required to exit 0 with `FAIL=0` and **empty stderr**.
2. **C++20 must be REJECTED.** `std::expected` is a C++23 library
   feature; a C++20 translation unit using it is rejected by the
   compiler, as it must be. (The lesson needs C++23, not because of
   language features but because of `std::expected`.)
3. **ASan + UBSan — clean.** `-fsanitize=address -fsanitize=undefined
   -fno-omit-frame-pointer`: zero diagnostics, **empty stderr**,
   `FAIL=0`.
4. **Compile-fail probes — 4/4 rejected with the expected
   diagnostic.**
   - `std::vformat_to_n` → "no member named 'vformat_to_n'" (there is
     no public runtime-format bounded variant).
   - `format_to_n_result<char*>::ec` → "no member named 'ec'".
   - a **runtime** `std::string_view` passed to `std::format_to_n` →
     "call to consteval function ... is not a constant expression".
   - `std::format_to_n(b, 8, "{:f}", 42)` → the bad presentation type
     is a **compile-time** error.
5. **Control probes — 3/3 compiled and printed the expected value.**
   - a negative `n` compiles and prints `size=5` with `out` unadvanced.
   - `sizeof(format_to_n_result<char*>) == 16`.
   - an **inline** `std::string_view("{}")` compiles and formats (KEY
     FINDING #9).
6. **Flakiness — 5 consecutive runs produced byte-identical output.**
7. **In-process exhaustive sweep — 50/50 cells** (Section 11):
   `disagree=0 byte_fail=0 atomicity_fail=0 bleed_fail=0
   needed_fail=0 three_way_fail=0`.
8. **Mutation test — the checks have teeth.** Three mutants, each
   required to make the program **exit non-zero**:
   - **Mutant A**: the write pass bypasses the custom sink and writes
     straight to the user's buffer. Result: the program fails — the
     atomicity checks fire.
   - **Mutant B**: the stdlib adapter mis-reports the needed byte
     count. Result: the program fails — `needed_fail` fires.
   - **Mutant C**: the arc reports one byte more than it wrote.
     Result: the program fails — `byte_fail` fires.
9. **CMake — 3/3 pass.** `default`, `ENABLE_STRICT_WARNINGS=ON`, and
   `ENABLE_STRICT_WARNINGS=ON + ENABLE_ASAN=ON` all configure, build
   and run clean, with **0 warnings** in every build log and `FAIL=0`
   on every run.
10. **No compiled binary is staged.** The lesson directory contains
    only the three intended files (`*.cpp`, `*.md`, `CMakeLists.txt`);
    every build output goes straight to `/tmp`.

**Harness summary: pass=27 fail=0.**

---

## Notes / gotchas worth remembering

- **`std::format_to_n_result` has no `ec`.** The stdlib's contract has
  no error channel at all. Anything you build on top of it has to
  *construct* one, and it cannot recover the bytes the stdlib already
  wrote before it noticed the overflow.
- **The stdlib's `size` is "would have been written", not "written".**
  The arc's success value is "written". They agree only when the
  output fits.
- **`format_to_n`'s `n` is signed.** A negative `n` is silently
  "capacity zero".
- **libc++'s buffer choice determines whether you get atomicity.**
  `char*` → direct write; any other iterator → accumulate-then-copy.
  A formatter that throws mid-format leaves a partial write in the
  `char*` case and nothing in the other.
- **A control experiment that shows no difference may be measuring
  nothing.** Make sure the interesting path was reached.
- **The index of the first modified byte is not a "did it overrun"
  metric.** Use the last modified index.
- **A parameter pack's shape matters when it appears next to a
  deduced class template.** `std::format_to_n` deduces its `_Args`
  from **forwarding** references, so it wants `format_string<int&>`
  for a non-const lvalue and `format_string<const int&>` for a const
  one. An adapter spelled `const Args&...` is rejected with "no
  known conversion from 'format_string<int>' to 'format_string<const
  int &>'". Measured; the adapter in this lesson uses `Args&&...` and
  `std::forward` to stay in step.
- **`/usr/bin/g++` on this machine is Apple clang**, so a "g++" result
  is not an independent second implementation. (Carried over from Oct
  22's notes; still true.)
- **The compile-time check is on the VALUE, not the type's spelling**
  (KEY FINDING #9). An inline `std::string_view("{}")` is a constant
  expression and is accepted by `std::format_to_n`; a
  `std::string_view` arriving as a function parameter is not. A
  verification probe written on the "type spelling" assumption will
  silently test the wrong thing — mine did, and it compiled.
- **`std::expected` is C++23.** libc++ 21 gates it on
  `_LIBCPP_STD_VER >= 23`, so a C++20 translation unit that uses it
  fails with "use of undeclared identifier 'std'" rather than a
  friendlier message.
- **A parameter pack's shape matters next to a deduced class
  template.** `std::format_to_n` deduces its `_Args` from **forwarding**
  references, so it wants `format_string<int&>` for a non-const
  lvalue and `format_string<const int&>` for a const one. An adapter
  spelled `const Args&...` is rejected with "no known conversion from
  'format_string<int>' to 'format_string<const int &>'". Measured; the
  adapter in this lesson uses `Args&&...` and `std::forward` to stay
  in step.

---

## What did NOT change (deliberately)

- `petra::FormatError` / `FormatErrorKind`, the two-pass (measure,
  then write) impls, `counting_output_iterator` and
  `bounded_char_writer` are reused **verbatim** from Oct 26 / Oct 25.
- **The arc's contract is not changed.** This lesson *measures* it
  against the standard library; it does not replace it.
- No pad-selection machinery is touched — the pad axis is orthogonal
  to the error contract.
- No `-D` flag names or CMake options from the pad arc are changed,
  so yesterday's build cells keep their meaning.

---

## Where we go next

1. **Decide what the arc's atomicity is actually FOR.** Today
   measured that it is real but conditional (it depends on the sink
   type, and it does not survive the post-write consistency check).
   The open question is whether the contract should be *documented as
   conditional* (with the artifact named) or *fixed* (by making the
   write pass write into a scratch buffer and copying out only on
   success — which costs a third pass).
2. **Give the arc's error taxonomy the resolution the stdlib has.**
   `FormatParse` and `FormatArgument` are unreachable today; the
   stdlib distinguishes four failure classes by message. Worth
   measuring whether libc++ exposes the class structurally (an error
   code, or distinguishable exception types) before deciding whether
   the arc should classify by message prefix.
3. **Make the sweep cover the ERROR paths** (carried over from Oct
   25's item (2) and Oct 26's item (2), still open). Section 11 sweeps
   the success cells; the `TooLarge` and `Format` paths are checked
   for a handful of cells only.
4. **Compare against `std::formatted_size` + `std::format_to`.** The
   stdlib has a two-function spelling of the arc's two-pass design
   (measure with `formatted_size`, then write with `format_to`), and
   that comparison would isolate how much of the arc's two-pass
   machinery is libc++'s and how much is the arc's.
5. **Revisit the oracle** (carried over from Oct 26's item (4)) — the
   hand-written `oracle_select` if-chain is still the one place a new
   pad must be added by hand.
6. **Check whether the matrix has a FIFTH cell** (carried over from
   Oct 26's item (6)). Today's cell [3] has no public stdlib
   counterpart at all, which is a fact about the matrix's shape worth
   folding into the notes.
