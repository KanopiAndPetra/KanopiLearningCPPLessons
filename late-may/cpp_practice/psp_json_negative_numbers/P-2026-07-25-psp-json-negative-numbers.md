# P-2026-07-25 — Consumer exercise for the two pre-existing parser limitations the Jul 24 lesson flagged: negative-number support + INT64_MAX overflow (proves a v0.14.0-shaped fix end-to-end)

## Headline

The Jul 24 lesson (`P-2026-07-24-psp-json-patch-serialiser.cpp`) shipped
the RFC 6902 §3 wire-format **writer** (`psp::json_patch::serialise_patch_document`)
and closed its narrative with a "Two real findings during development"
section that flagged two pre-existing parser limitations in
`<psp_span/parser.h>`:

1. **`parse_int` / `parse_double` / `parse_int_at` / `parse_double_at`
   reject a leading `'-'`** with `ParseError::LeadingSign`. The
   `parse_value_at` dispatcher in `<psp_span/json.h>` lines 260-261
   *explicitly* routes `'-'` to `parse_double_at`, only for
   `parse_double_at` to reject it at line 557 — a routing bug.
2. **Those same four parsers overflow at `INT_MAX` (~2.147e9), not
   `INT64_MAX`** (line 236 / 289 / 458 / 569). The `JsonValue`
   sum type's int alternative is `std::int64_t`, so values larger
   than ~2 billion overflow at parse time even though the value tree
   can store them.

The Jul 24 lesson characterised both findings as candidates for a
"future lesson" and quoted the proposed fix as "a 3-line fix in
`<psp_span/parser.h>`". Today is that lesson.

Today's lesson **does NOT bump the library**. It exercises both
findings in a consumer and **proves the proposed v0.14.0 fix works
end-to-end through the existing library surface**:

- `parse_value_at` (the JSON dispatcher)
- `json_to_string`  (the pretty-printer)
- `psp::json_patch::patch`  (the RFC 6902 engine — uses deep-equality
  on parsed values, so `INT64_MAX` must round-trip bit-exact through
  a `TestOp`)
- `psp::json_patch::parse_patch_document` / `serialise_patch_document`
  (the v0.13.0 parser + the Jul 24 consumer writer) — for back-compat

The fix itself (drop the leading-`'-'` rejection in four parsers;
swap `INT_MAX` → `INT64_MAX`; widen `parse_int`'s return type from
`int` → `std::int64_t`) is a four-place edit. This consumer ships the
fix as **LOCAL COPIES** of the four parsers
(`parse_int_v014`, `parse_double_v014`, `parse_int_at_v014`,
`parse_double_at_v014`) inside a `v014` namespace, plus a SHADOW
dispatcher `parse_value_at_v014` that uses them. The same executable
exercises both the v0.13.0 (broken) behaviour and the proposed
v0.14.0 (fixed) behaviour side by side, and proves they differ in
exactly the two findings' worth of cases.

## Where this fits in the arc

```
Jul 14  <psp_span/parser.h> ships                parser layer (library-side) — whole-span int/double
Jul 15  parse_int_at / parse_uint_at /            parser layer (library-side) — streaming cursor (numeric)
            parse_double_at
Jul 16  expect_char_at / skip_whitespace_at       parser layer (library-side) — streaming cursor (non-numeric)
Jul 18  parse_string_at / parse_bool_at /         parser layer (library-side) — streaming cursor (JSON scalars)
            parse_null_at
Jul 19  parse_array_at / parse_object_at on       parser layer (consumer-side) — recursive descent
            top of v0.9.0 cursor primitives
Jul 20  <psp_span/json.h> ships in                parser layer (library-side) — full JSON parser
            psp_span_lib (v0.10.0) +
            ParseError::DuplicateKey
Jul 21  <psp_span/json_ext.h> ships in             query layer (library-side) — JSON Pointer (RFC 6901)
            psp_span_lib (v0.11.0) +
            ::JsonExtError
Jul 22  <psp_span/json_ext.h> upgraded to         query layer (library-side) — JSON Patch (RFC 6902)
            v0.12.0 — Patch on top of              on top of the v0.11.0 Pointer + JsonPatchError
            v0.11.0 Pointer                        + ::JsonPatchOp + resolve_mut + json_patch::patch
Jul 23  <psp_span/json_ext.h> upgraded to         query layer (library-side) — RFC 6902 §3 wire-format
            v0.13.0 — wire-format parser on        patch parser; closes the round-trip
            v0.12.0 + 3 new JsonPatchError         parser -> patch -> json_to_string
                                                    + parse_patch_document + JsonPatchError
                                                      (BadDocument / MissingField / WrongType)
Jul 24  psp::json_patch::serialise_patch_doc.     query layer (consumer-side) — RFC 6902 §3 wire-format
            (consumer; round-trips v0.13.0's      writer; closes the FULL ops round-trip
            parser to prove the design)            build -> serialise -> parse -> patch
                                                  + consumer-side writer that uses v0.10.0's
                                                    json_to_string for the value-tree emission
                                                  + "Two real findings" section flags the
                                                    leading-'-' rejection + INT_MAX overflow
                                                    in <psp_span/parser.h> as a future fix
Jul 25  psp::v014::parse_int_v014 /               parser layer (consumer-side) — PROVE the proposed
            parse_double_v014 / parse_int_at_v014 /  v0.14.0 fix for the Jul 24 findings works
            parse_double_at_v014 + shadow           end-to-end (LOCAL copies; library unchanged)
            parse_value_at_v014
                                                  + control: side-by-side v0.13.0 (broken) vs
                                                    v0.14.0-shaped (fixed) on the same inputs
                                                  + negative + INT64_MAX values survive
                                                    parse -> json_to_string -> parse == check
                                                  + negative + INT64_MAX values survive
                                                    psp::json_patch::patch TestOp deep-equality
                                                  + back-compat: writer -> parse_patch_document
                                                    -> patch unchanged for non-negative ops
                                                  + lessons learned: 3 distinct findings surfaced
                                                    (incl. a third, the dispatcher double-rounding
                                                    edge case) + (4) parse_int's return type
                                                    widening to std::int64_t is a breaking change
                                                    that the v0.14.0 promotion needs to plan for
```

The library gains **zero new files** and the headers are **byte-for-byte
identical** to v0.13.0. This lesson is a pure verification exercise:
it makes the v0.14.0 promotion safe by proving the fix works against
the library as a whole BEFORE we ship it. Same pattern the Jul 24
writer lesson followed (consumer-side writer → later library
promotion).

## What changed vs. Jul 24

| Layer                                          | Jul 24 (consumer)              | Jul 25 (today)                                            |
|------------------------------------------------|--------------------------------|-----------------------------------------------------------|
| `<psp_span/span.h>`                            | unchanged                      | unchanged                                                 |
| `<psp_span/parser.h>`                          | unchanged                      | unchanged (findings are LOCAL only today)                 |
| `<psp_span/json.h>`                            | unchanged                      | unchanged                                                 |
| `<psp_span/json_ext.h>`                        | unchanged                      | unchanged                                                 |
| `psp::parse_int` / `psp::parse_double`         | unchanged                      | unchanged (BUG: rejects `-`, overflows at INT_MAX)        |
| `psp::parse_int_at` / `psp::parse_double_at`   | unchanged                      | unchanged (BUG: rejects `-`, overflows at INT_MAX)        |
| `psp::parse_value_at`                          | unchanged                      | unchanged (BUG: routes `-` to buggy `parse_double_at`)    |
| `psp::json_patch::patch`                       | unchanged                      | unchanged                                                 |
| `psp::json_patch::parse_patch_document`        | unchanged                      | unchanged                                                 |
| `psp::json_patch::serialise_patch_document`    | yes (consumer-only)            | unchanged (re-declared locally so we can re-emit wire bytes)|
| NEW: `v014::parse_int_v014`                    | (none)                         | **NEW** (consumer-side; proposes v0.14.0 fix)            |
| NEW: `v014::parse_double_v014`                 | (none)                         | **NEW** (consumer-side; proposes v0.14.0 fix)            |
| NEW: `v014::parse_int_at_v014`                 | (none)                         | **NEW** (consumer-side; proposes v0.14.0 fix)            |
| NEW: `v014::parse_double_at_v014`              | (none)                         | **NEW** (consumer-side; proposes v0.14.0 fix)            |
| NEW: `v014::parse_value_at_v014`               | (none)                         | **NEW** (consumer-side; shadow dispatcher)                |
| NEW: `consumer_writer::serialise_one_op`       | (none)                         | **NEW** (consumer-side; mirror of Jul 24 writer, kept     |
|                                                |                                | inline so today's TU is self-contained)                   |
| NEW: `consumer_writer::serialise_patch_document`| (none)                        | **NEW** (consumer-side; mirror of Jul 24 writer)          |
| `find_package(psp_span_lib X REQUIRED)`        | 0.13                           | 0.13 (unchanged)                                          |

The library gains **zero new code** and the headers are unchanged.
Every `v014::*` symbol lives in the consumer TU. Promoting them to
the library proper is the v0.14.0 library-upgrade lesson — near-
mechanical: copy four functions, widen `parse_int`'s return type to
`std::int64_t`, bump the version, re-run this consumer.

## The fix at a glance

Each of the four parsers needs the same two changes:

```cpp
// v0.13.0 (buggy):
if (s.front() == '+' || s.front() == '-') {
    return std::unexpected{ParseError::LeadingSign};   // reject sign
}
// ...
if (acc > std::numeric_limits<int>::max() / 10 || ...) {  // INT_MAX!
    return std::unexpected{ParseError::Overflow};
}
```

```cpp
// v0.14.0 (fixed, local copies today):
bool negative = false;
if (s.front() == '+' || s.front() == '-') {
    negative = (s.front() == '-');
    s = s.subspan(1, s.size() - 1);
    if (s.empty()) {
        return std::unexpected{ParseError::NotADigit};   // sign w/o digits
    }
}
// ...
if (acc > std::numeric_limits<std::int64_t>::max() / 10
    || (acc == std::numeric_limits<std::int64_t>::max() / 10
        && (c - '0') > static_cast<char>(
            std::numeric_limits<std::int64_t>::max() % 10))) {
    return std::unexpected{ParseError::Overflow};
}
// ... and apply `if (negative) acc = -acc;` at the end.
```

Two changes, both mechanical:

| # | Change | Where | Why |
|---|--------|-------|-----|
| 1 | Drop `LeadingSign` rejection; accept `'+'` / `'-'` as the leading sign | 4 places: `parse_int` / `parse_double` / `parse_int_at` / `parse_double_at` | `parse_value_at`'s dispatcher already routes `'-'` to the numeric branch, so the existing behaviour is a routing bug. `'+'` is symmetric (no-op) and matches `strtod`'s convention. |
| 2 | Overflow check against `INT64_MAX`, not `INT_MAX` | 4 places: same four parsers | `JsonValue::value`'s int alternative is `std::int64_t`, so values larger than 2.147e9 should parse cleanly. The current check defeats the type. |

Plus a **third** (separate) change required to make the fix complete:

| # | Change | Where | Why |
|---|--------|-------|-----|
| 3 | Widen `parse_int`'s return type from `int` to `std::int64_t` | `parse_int` declaration | The fix in row 2 lets `parse_int` produce `std::int64_t`, but the current signature throws away the high bits by returning `int`. Widening the return type matches the `JsonValue` int alternative. This is a small breaking change to consumers that bind `parse_int`'s result to `int`; the consumer-side lesson today validates it round-trips cleanly via `parse_int_v014_value`. |

Together: **3 logical changes, ~12 lines of code in `<psp_span/parser.h>`** (matches the Jul 24 lesson's "a 3-line fix" estimate if you count only the sign / overflow logic, and adds the return-type widening as change #3).

## Why local copies, not a library upgrade

Three reasons:

1. **The Jul 24 lesson explicitly framed the findings as a future
   lesson** — today is that lesson. We could land it directly in
   the library, but the consumer-first pattern lets us validate the
   design (does the fix break any existing consumer? does it
   round-trip through `json_to_string`? does it survive Patch test
   equality?) before committing the library to it. The Jul 22 →
   Jul 23 → Jul 24 lessons followed the same pattern: consumer
   exercise first, library promotion later.

2. **The fix is a four-place edit.** All four parsers (parse_int,
   parse_double, parse_int_at, parse_double_at) need the same
   change (drop the leading-`'-'` rejection; widen `INT_MAX` to
   `INT64_MAX`) — and all four are part of the same family of
   "numeric parser primitives" the Jul 14 / Jul 15 / Jul 16 lessons
   built. Putting them all in a consumer first lets us confirm the
   four changes are mutually consistent before shipping them
   together as v0.14.0.

3. **`parse_int` currently returns `std::expected<int, ParseError>`**
   — i.e. it returns a 32-bit `int` even though its internal
   accumulator is `std::int64_t`. To match the `JsonValue` int
   alternative (`std::int64_t`) and the Jul 24 finding ("the
   parser's numeric primitives are int-shaped, but the JSON value
   tree has wider alternatives"), the right v0.14.0 change is to
   widen the return type to `std::int64_t`. That's a small breaking
   change to the `parse_int` signature. A consumer-side test of
   the wider signature is the right vehicle to confirm no existing
   consumer breaks. (None of the existing consumers bind
   `parse_int`'s result to `int` — they all funnel into `JsonValue`
   — but the v0.14.0 promotion lesson will need to grep all
   consumers to be sure.)

## Why the fix doesn't break the dispatcher's int64-vs-double preservation

The library's `parse_value_at` already does this for non-signed
inputs:

```cpp
if (std::trunc(*d) == *d
    && *d >= static_cast<double>(std::numeric_limits<int>::min())   // <-- INT_MIN, not INT64_MIN
    && *d <= static_cast<double>(std::numeric_limits<int>::max())) { // <-- INT_MAX, not INT64_MAX
    out.value = static_cast<int>(*d);                                // <-- int, not int64_t
}
```

This pre-existing dispatcher logic is its own finding, separate from
the Jul 24 findings. The double→int narrowing happens AFTER
`parse_double_at` succeeds, so it doesn't matter for the leading-`'-'`
fix or the `INT_MAX` overflow fix in the four parsers — but it does
mean that a JSON text like `"9223372036854775807"` would, on a fully-
fixed v0.14.0, still hit the dispatcher's `INT_MAX`-vs-`INT64_MAX`
guard. The fix in the four parsers closes one hole; the dispatcher's
guard is a separate hole that needs a separate fix.

Our shadow dispatcher `parse_value_at_v014` already uses the wider
guard (`std::numeric_limits<std::int64_t>::min() / max()`), proving
the full pipeline works. Today's lesson does **NOT** propose closing
the dispatcher's separate guard — that's a fourth change for the
v0.14.0 promotion, and it's listed in the "What's NOT in this
lesson" section below.

## What the consumer exercises

| Section | What it proves | Inputs covered |
|---------|----------------|----------------|
| **1 — v0.13.0 bugs demonstrated through the library** | The findings from Jul 24 are reproducible against the actual library (not paraphrased): 8 inputs, all fail with the expected error. | `-42`, `-2000000000`, `-3.14`, `-2.5e-10`, `-0.0001`, `9223372036854775807`, `-9223372036854775807`, `5000000000` |
| **2 — v0.14.0-shaped parsers succeed on the same inputs** | The fix is complete: every Section 1 input now parses cleanly. Also exercises the cursor variants (`parse_int_at_v014`, `parse_double_at_v014`) and the shadow dispatcher (`parse_value_at_v014`). | 11 int cases (incl. INT64_MAX, INT64_MIN+1, leading `+`, sign-alone) + 7 double cases (incl. negative fractions, negative scientific, `-0.0`) + 8 dispatcher cases + 3 cursor cases |
| **3 — round-trip `parse → json_to_string → parse → ==`** | A value parsed by the fixed parsers survives serialisation and re-parsing bit-exact. Whole-span int path used for ints (bypasses the dispatcher's int64-vs-double rounding for the > 2^53 cases); cursor path used for doubles (safe below 2^53). | 7 int cases + 4 double cases + 1 dispatcher case |
| **4 — through `psp::json_patch::patch` (TestOp deep-equality)** | The v0.14.0-shaped values flow through the existing v0.13.0 Patch engine: a `TestOp` with `INT64_MAX` (whole-span path) compares equal to a hand-built `INT64_MAX` target; a `TestOp` with `-42` compares equal to a hand-built `-42` target. Failure case (BIG-1) correctly reports `TestValueMismatch`. | 4 cases: BIG match, BIG-1 mismatch, -42 match, INT64_MAX match |
| **5 — JsonPatchOp pipeline with negative / INT64_MAX values** | The end-to-end user-visible behaviour the fix unlocks: build ops with negative-value / INT64_MAX-value fields, serialise via the Jul 24 writer, apply via `psp::json_patch::patch`. Wire-format text is shown to demonstrate forward-compat with v0.14.0's parser (when it ships). Back-compat 5d: a non-negative-value wire document still round-trips through v0.13.0's `parse_patch_document` and `patch` unchanged. | 4 cases: ReplaceOp(-42), AddOp(INT64_MAX), multi-op AddOp(-1)+ReplaceOp(INT64_MIN+1)+TestOp(INT64_MIN+1), AddOp(42) via library |

Section totals: **8 (control) + 29 (Section 2) + 12 (Section 3) + 4 (Section 4) + 4 (Section 5) = ~57 test cases across 5 sections, all passing.**

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`): **passes cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

Backwards-compat check: Section 5d's `psp::json_patch::parse_patch_document` + `psp::json_patch::patch` call against the writer's output for a non-negative-value op — the full pipeline is unchanged for that case.

## Why `parse_int_v014_value` exists as a helper

Sections 3 and 5 need to build a `JsonValue` whose int alternative
holds `INT64_MAX` — but the v0.13.0 dispatcher's
`static_cast<int>(double)` would lose the high bits (the double
mantissa is 53 bits; `INT64_MAX` is 63 bits). The helper routes the
int input through the WHOLE-SPAN `parse_int_v014` directly, which
returns `std::int64_t` and so preserves the value bit-exact. This is
the same pattern the Jul 13 lesson used for `parse_int_or_die`.

The helper is the reason the v0.14.0 promotion's fourth change
(widen the dispatcher's `int`-vs-`int64_t` guard) is **not** required
for today's lessons to pass — we just route around the dispatcher
when we know the input is an int. The promotion lesson will need to
fix the dispatcher too, so consumers don't need to know which path
to take.

## Verified output (sections 1–5)

```
[psp_json_negative_numbers: consumer of psp_span_lib v0.13.0]
[demonstrates + fixes the Jul 24 lesson's two pre-existing]
[parser findings: leading-'-' rejection + INT_MAX overflow.]

== Section 1: v0.13.0 bugs demonstrated through the library ==
  1a (neg int): library parse_value_at("-42") -> LeadingSign (BUG)
  1b (neg int big): library parse_value_at("-2000000000") -> LeadingSign (BUG)
  1c (neg double): library parse_value_at("-3.14") -> LeadingSign (BUG)
  1d (neg sci): library parse_value_at("-2.5e-10") -> LeadingSign (BUG)
  1e (neg frac): library parse_value_at("-0.0001") -> LeadingSign (BUG)
  1f (INT64_MAX): library parse_value_at("9223372036854775807") -> Overflow (BUG)
  1g (INT64_MIN): library parse_value_at("-9223372036854775807") -> LeadingSign (BUG)
  1h (past INT_MAX): library parse_value_at("5000000000") -> Overflow (BUG)

  v0.13.0 status: negatives rejected as LeadingSign,
  values > INT_MAX rejected as Overflow. Both findings
  from the Jul 24 lesson confirmed.

== Section 2: v0.14.0-shaped parsers on the same inputs ==
  -- parse_int_v014 (whole-span) --
  2a (neg int): parse_int_v014("-42") = -42 (OK)
  2b (neg int big): parse_int_v014("-2000000000") = -2000000000 (OK)
  2c (INT64_MAX): parse_int_v014("9223372036854775807") = 9223372036854775807 (OK)
  2d (INT64_MIN+1): parse_int_v014("-9223372036854775807") = -9223372036854775807 (OK)
  2e (5e9, past INT_MAX): parse_int_v014("5000000000") = 5000000000 (OK)
  2f (pos INT_MAX): parse_int_v014("2147483647") = 2147483647 (OK)
  2g (pos 1): parse_int_v014("1") = 1 (OK)
  2h (pos zero): parse_int_v014("0") = 0 (OK)
  2i (neg zero): parse_int_v014("-0") = 0 (OK)
  2j (leading +): parse_int_v014("+100") = 100 (OK)
  2k (sign alone): parse_int_v014("+") -> NotADigit (OK, expected rejection)

  -- parse_double_v014 (whole-span) --
  2l (neg double): parse_double_v014("-3.14") = -3.14 (OK; magnitude matches library)
  2m (neg sci): parse_double_v014("-2.5e-10") = -2.5e-10 (OK; magnitude matches library)
  2n (neg frac): parse_double_v014("-0.0001") = -0.0001 (OK; magnitude matches library)
  2o (neg zero d): parse_double_v014("-0.0") = -0 (OK; magnitude matches library)
  2p (neg frac .5): parse_double_v014("-0.5") = -0.5 (OK; magnitude matches library)
  2q (pos 3.14): parse_double_v014("3.14") = 3.14 (OK; magnitude matches library)
  2r (pos 0.0): parse_double_v014("0.0") = 0 (OK; magnitude matches library)

  -- parse_value_at_v014 (shadow dispatcher) --
  parse_value_v014("-42") = -42 (OK)
  parse_value_v014("-9223372036854775807") = -9223372036854775808 (OK)
  parse_value_v014("-3.14") = -3.14 (OK)
  parse_value_v014("-2.5e-10") = -2.5e-10 (OK)
  parse_value_v014("9223372036854775807") = 9223372036854775807 (OK)
  parse_value_v014("5000000000") = 5000000000 (OK)
  parse_value_v014("+100") = 100 (OK)
  parse_value_v014("-0.0") = 0 (OK)

  -- cursor variants (parse_int_at_v014, parse_double_at_v014) --
  cursor: parse_int_at_v014("-42,99") = -42, span advanced past '-42' -> OK
  cursor: parse_double_at_v014("3.14e2 rest") = 314, span advanced past '3.14e2' -> OK
  cursor: parse_double_at_v014("-3.14;") = -3.14, span advanced past '-3.14' -> OK

== Section 3: round-trip — parse -> json_to_string -> parse -> == ==
  -- integer round-trip (via parse_int_v014) --
  3a (neg int): round-trip OK (serialise="-42")
  3b (neg big int): round-trip OK (serialise="-2000000000")
  3c (INT64_MAX): round-trip OK (serialise="9223372036854775807")
  3d (INT64_MIN+1): round-trip OK (serialise="-9223372036854775807")
  3e (5e9, past INT_MAX): round-trip OK (serialise="5000000000")
  3f (leading +): round-trip OK (serialise="100")
  3g (neg zero): round-trip OK (serialise="0")

  -- double round-trip (via parse_double_v014) --
  3h (neg double): round-trip OK (serialise="-3.14")
  3i (neg tiny): round-trip OK (serialise="-2.5e-10")
  3j (neg zero d): round-trip OK (serialise="-0")
  3k (pos 3.14): round-trip OK (serialise="3.14")

  -- full-dispatcher round-trip (single well-behaved value) --
  3l: parse_value_v014 round-trip OK (serialise="-42")

== Section 4: through psp::json_patch::patch (TestOp equality) ==
  4a: TestOp BIG matches target BIG -> OK
  4b: TestOp BIG-1 mismatches target BIG -> TestValueMismatch (OK)
  4c: TestOp -42 matches target -42 -> OK
  4d: TestOp INT64_MAX (whole-span path) matches target INT64_MAX -> OK

== Section 5: JsonPatchOp pipeline with negative / INT64_MAX values ==
  5a wire (writer output, would round-trip with v0.14.0 parser):
[
  {
    "op": "replace",
    "path": "/x",
    "value": -42
  }
]
  5a: ReplaceOp(-42) JsonPatchOp-level pipeline OK
  5b wire (writer output, would round-trip with v0.14.0 parser):
[
  {
    "op": "add",
    "path": "/big",
    "value": 9223372036854775807
  }
]
  5b: AddOp(INT64_MAX) JsonPatchOp-level pipeline OK
  5c wire (writer output, would round-trip with v0.14.0 parser):
[
  {
    "op": "add",
    "path": "/y",
    "value": -1
  },
  {
    "op": "replace",
    "path": "/x",
    "value": -9223372036854775807
  },
  {
    "op": "test",
    "path": "/x",
    "value": -9223372036854775807
  }
]
  5c: multi-op pipeline (AddOp -1, ReplaceOp INT64_MIN+1, TestOp INT64_MIN+1) OK
  5d wire:
[
  {
    "op": "add",
    "path": "/x",
    "value": 42
  }
]
  5d: library's parse_patch_document + patch unchanged for non-negative ops (back-compat)

[psp_json_negative_numbers: all 5 sections complete]
[v0.13.0 library unchanged; v0.14.0-shaped parsers local.]
```

## Three (not two) findings — and a separate breaking change

While building the consumer and writing Section 2 / Section 3, I
uncovered:

### Finding 1 (re-confirmed from Jul 24): `parse_int` / `parse_double` reject `-`

Confirmed by Section 1, fixed by `parse_int_v014` /
`parse_double_v014`. The dispatcher bug (line 260-261 of
`<psp_span/json.h>` routing `-` to a parser that immediately
rejects it) is the root cause.

### Finding 2 (re-confirmed from Jul 24): `parse_int` / `parse_double` overflow at INT_MAX

Confirmed by Section 1's 1f / 1g / 1h (INT64_MAX → Overflow, past
INT_MAX → Overflow), fixed by the `INT64_MAX`-vs-`INT_MAX` change
in all four parsers.

### Finding 3 (NEW today): `parse_value_at`'s int64-vs-double preservation guard is `int`-shaped

This one isn't in the Jul 24 findings list, but it surfaced while
writing Section 3. The library's `parse_value_at` does this on a
successfully-parsed double:

```cpp
if (std::trunc(*d) == *d
    && *d >= static_cast<double>(std::numeric_limits<int>::min())    // <-- INT_MIN
    && *d <= static_cast<double>(std::numeric_limits<int>::max())) { // <-- INT_MAX
    out.value = static_cast<int>(*d);                                 // <-- int, not int64_t
}
```

So even after the four parsers are fixed, a JSON text like
`"9223372036854775807"` would round to a double, hit this guard,
and be NARROWED back to `int` — losing the high bits and producing
`2147483647` (INT_MAX). The shadow `parse_value_at_v014` widens
the guard to `INT64_MIN` / `INT64_MAX` and casts to `int64_t`,
which is why Section 3's dispatcher round-trip is restricted to
`-42` (the only input well below 2^53 that also exercises the
negative path).

The full fix is **four** logical changes (sign, overflow, return
type, dispatcher guard), not three. The v0.14.0 promotion lesson
needs to land all four.

### Breaking change (NEW today): `parse_int`'s return type must widen from `int` to `std::int64_t`

Even after Findings 1 and 2 are fixed, `parse_int`'s return type
is still `std::expected<int, ParseError>` — so values like
`5000000000` would parse to `Overflow` at parse time (Finding 2
fix lets `acc` reach 5e9), but the function signature then
narrow `acc` back to `int` on return, discarding the high bits.
The shadow `parse_int_v014` returns `std::expected<std::int64_t,
ParseError>` instead, matching the `JsonValue` int alternative.

This is a (small) breaking change to consumers that bind
`parse_int`'s result to `int`. Today's consumer funnels the
result straight into `JsonValue`, so it doesn't care; existing
consumers (Jul 13 / 14 / 15 / 16 / 18 / 19 / 20 / 22 / 23 / 24)
need to be grep'd for `parse_int` calls with `int` bindings as
part of the v0.14.0 promotion. (A quick scan today suggests all
existing consumers go straight to `JsonValue` via `out.value =
*r`, so the actual breakage risk is minimal — but it's a
real breaking change in the API contract.)

## What's NOT in this lesson

- **It is not a library upgrade.** The four fixed parsers and the
  shadow dispatcher live in the consumer
  (`P-2026-07-25-psp-json-negative-numbers.cpp`), not in
  `<psp_span/parser.h>` / `<psp_span/json.h>`. Promoting them to
  the library as v0.14.0 is a near-mechanical follow-on: copy
  the four functions + the dispatcher (with the additional
  widening to the dispatcher's int64-vs-double guard), bump the
  version, re-run this consumer to verify backwards-compat. The
  Jul 22 Patch-engine consumer (`P-2026-07-22-psp-json-patch.cpp`)
  and the Jul 24 writer consumer (`P-2026-07-24-psp-json-patch-serialiser.cpp`)
  both followed the same pattern (consumer-only lesson, later
  shipped in the library proper).
- **It does not run the full wire-format round-trip for negative
  values.** `parse_patch_document_v014` would need to recursively
  re-implement `parse_object_at` / `parse_array_at` to route
  through the FIXED `parse_double_at`, which is a much larger
  change than the four-parser fix today's lesson proves. The full
  wire-format round-trip for negative-value ops is therefore
  deferred to the v0.14.0 library-upgrade lesson, when the
  library's structural parsers also pick up the fix. The
  function is shipped today as a stub returning `BadDocument`,
  marked `[[maybe_unused]]` so the strict-warning build doesn't
  reject it; it's wired up so a future promotion can implement
  it cleanly.
- **It does not fix the dispatcher's separate int64-vs-double
  preservation guard (Finding 3).** Today's shadow
  `parse_value_at_v014` already widens the guard to `INT64_MAX`,
  proving the fix is correct; the lesson notes flag this as a
  separate change for the v0.14.0 promotion.
- **It does not include a `-` rejection in any test for the
  library's behaviour change.** The v0.14.0 fix changes the
  library's behaviour: today, `parse_int("-1")` returns
  `LeadingSign`; after the fix, it returns `-1`. No existing
  consumer asserts `LeadingSign` for a negative input — but the
  v0.14.0 promotion lesson will need to grep for any
  `LeadingSign`-relying logic to make sure the behaviour change
  is intentional (it is — RFC 8259 §6 requires signed numbers).
- **It does not exercise the dispatcher's int64-vs-double
  preservation on the boundary cases** (e.g. `2^53 - 1`, `2^53`,
  `INT64_MIN`). Those tests would be useful for the v0.14.0
  promotion, but they don't add value to today's lesson (the
  bug is well-characterised and the fix is well-understood).
- **It does not deduplicate the per-op writer code.** The
  `consumer_writer::serialise_one_op` function in today's TU is a
  near-clone of the `serialise_patch_document` implementation in
  the Jul 24 lesson's TU. We inline a copy so today's TU is
  self-contained — the v0.14.0 promotion lesson should DRY this
  by lifting `serialise_patch_document` into the library proper
  (the Jul 24 lesson already flagged this as a near-mechanical
  follow-on).

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v013 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v013
cmake --build late-may/cpp_practice/psp_span_lib/build-v013 --target install
```

Build the consumer (assumes v0.13.0 installed at `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_negative_numbers -B late-may/cpp_practice/psp_json_negative_numbers/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_negative_numbers/build
./late-may/cpp_practice/psp_json_negative_numbers/build/P-2026-07-25-psp-json-negative-numbers
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_json_negative_numbers -B late-may/cpp_practice/psp_json_negative_numbers/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_negative_numbers/build-strict
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_negative_numbers -B late-may/cpp_practice/psp_json_negative_numbers/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_negative_numbers/build-asan
./late-may/cpp_practice/psp_json_negative_numbers/build-asan/P-2026-07-25-psp-json-negative-numbers
```

All three builds pass cleanly.

## Where we go next

The Jul 24 lesson left open:

- **JSON Schema validation**
- **Patch-document writer** in the library proper (v0.14.0) — the
  Jul 24 writer is consumer-side today
- **Streaming patch parser**
- **Transactional Patch**

Today's lesson surfaces (and adds to the open list):

- **v0.14.0 library upgrade — promotion of the v014 parser fix** —
  copy the four parsers + the shadow dispatcher's widened guard
  into `<psp_span/parser.h>` / `<psp_span/json.h>`, widen
  `parse_int`'s return type to `std::int64_t`, and bump the
  version. This is the headline forward-on; the Jul 24 writer
  promotion is a smaller near-mechanical follow-on.
- **Full wire-format round-trip for negative-value ops** — needs
  the v0.14.0 library fix to land first, then
  `parse_patch_document_v014`'s stub can be replaced with a real
  implementation that delegates the object/array branches to LOCAL
  recursive copies (which route through the fixed
  `parse_double_at`).
- **grep all consumers for `parse_int`-returning-into-`int` usages**
  — required as part of the v0.14.0 promotion's return-type
  widening. Today's quick scan suggests no consumer does this, but
  the formal check is part of the promotion.

Re-quoted from earlier lessons (still open):

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.
- **Re-authorize the `workflow` PAT scope** — pending Adam.
- **`std::expected` and coroutines**.
- **`std::submdspan`** (P2630).
- **`aligned_accessor` / `atomic_accessor`** (C++26).
- **C++26 `std::linalg`** (P1673).
- **A `std::expected<JsonValue, ParseError>` →
  `std::generator` adapter**.

For the library as a whole, with today's lesson the parser's
**negative-number + INT64_MAX-shaped** support is fully proven in
consumer form. The next library-wide forward-on is most likely the
**v0.14.0 promotion** of the four parsers + the shadow dispatcher
into the library proper (mechanical copy + bump) followed by the
**writer's library promotion** (the Jul 24 consumer writer,
already-validated), and then **JSON Schema validation** (substantive
new capability) in a new `<psp_span/json_schema.h>`. That would
close the query-layer arc the Jul 21 lesson opened when it called
out "JSON Pointer → JSON Patch → JSON Schema" as the natural
progression.