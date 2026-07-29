# P-2026-07-27 — `<psp_span/parser.h>` + `<psp_span/json.h>` upgraded to v0.14.0: signed numeric literals + INT64-shaped overflow (consumer exercise proves the promotion end-to-end through the library proper)

## Headline

The Jul 25 lesson (`P-2026-07-25-psp-json-negative-numbers.cpp`) shipped
two pre-existing parser limitations as **consumer-side findings**:

1. `parse_int` / `parse_double` / `parse_int_at` / `parse_double_at`
   rejected a leading `'+'` or `'-'` with `ParseError::LeadingSign` —
   a routing bug, because the JSON dispatcher in
   `<psp_span/json.h>` already routes `'-'` to `parse_double_at`.
2. The same four parsers overflowed at `INT_MAX` (~2.15e9), not
   `INT64_MAX` (~9.22e18), even though the `JsonValue` sum type's
   `int` alternative is `std::int64_t`.

The Jul 25 lesson noted that the four-parser fix was a
**near-mechanical promotion** of the consumer's local `v014`
copies into the library header. Today is that promotion.

The promotion consists of three library changes (all mechanical,
total ~50 lines of code) and one consumer that exercises the
promoted library:

| Library change | Where | What |
|---|---|---|
| `parse_int` accepts an optional `'+'` or `'-'`, returns `std::int64_t` (was `int`), overflow check widened from `INT_MAX` to `INT64_MAX` | `<psp_span/parser.h>` lines 232-310 | The whole-span signed-int parser |
| `parse_int_at` accepts an optional `'+'` or `'-'`, returns `std::int64_t` (was `int`), overflow check widened from `INT_MAX` to `INT64_MAX` | `<psp_span/parser.h>` lines 564-624 | The streaming-cursor signed-int parser |
| `parse_double` accepts an optional `'+'` or `'-'`, overflow check on the integer-part accumulator widened from `INT_MAX` to `INT64_MAX` | `<psp_span/parser.h>` lines 358-485 | The whole-span double parser |
| `parse_double_at` accepts an optional `'+'` or `'-'`, overflow check on the integer-part accumulator widened from `INT_MAX` to `INT64_MAX` | `<psp_span/parser.h>` lines 718-823 | The streaming-cursor double parser |
| `parse_value_at` dispatches integer-shaped literals (no `.`, no `e`/`E`) through `parse_int_at` to preserve full int64 precision; accepts `'+'` in the dispatcher as well as `'-'` | `<psp_span/json.h>` lines 255-294 | The JSON dispatcher |
| `CMakeLists.txt`: version bump 0.13.0 → 0.14.0, library DESCRIPTION, and v0.14.0 changelog message | `<psp_span_lib/CMakeLists.txt>` | The build system |

**`parse_uint_at` is unchanged** (it still rejects `'-'` as
`LeadingSign`, accepts `'+'` as a no-op). The `ParseError`
enum's `LeadingSign` enumerator is **kept** (for back-compat —
the formatter still has a case for it; the parsers just no longer
emit it).

The consumer (`P-2026-07-27-psp-json-v014-promotion.cpp`,
**54 test cases across 5 sections, all passing**) exercises the
LIBRARY PROPER (no shadow dispatcher, no `v014` namespace, no
local copies of the parsers) and verifies:

1. **Sign acceptance** through the library proper (`parse_int` /
   `parse_double` / `parse_int_at` / `parse_double_at`).
2. **`parse_int`'s widened return type** (`std::int64_t`, was
   `int`) — past-`INT_MAX` literals (5e9, INT64_MAX) parse
   cleanly; the boundary cases (INT64_MAX+1, INT64_MIN)
   overflow cleanly.
3. **Round-trip** `parse_value_at` → `json_to_string` →
   `parse_value_at` → `==` for INT64_MAX-shaped, negative, and
   `+`-prefixed inputs.
4. **`psp::json_patch::patch` deep-equality** with INT64_MAX and
   negative literals — proves the v0.13.0 engine's `TestOp`
   equality works through the v0.14.0 parser chain.
5. **Wire-format round-trip** via the library's
   `psp::json_patch::parse_patch_document` + the Jul 24 consumer
   writer (re-inlined as `op_writer` here) — a 3-op document
   carrying `INT64_MIN+1`, `-1`, and a `TestOp(INT64_MIN+1)`
   survives `serialise → parse → patch` and lands the right
   values in the target tree.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) is clean. ASan+UBSan
build is clean. The Jul 25 consumer
(`P-2026-07-25-psp-json-negative-numbers.cpp`) still builds and
runs against v0.14.0 (it was written as a back-compat probe of
the consumer-side `v014` parsers against the library).

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
Jul 27  <psp_span/parser.h> +                     parser layer (library-side) — v0.14.0 PROMOTION
            <psp_span/json.h> upgraded to           (mechanical copy of the Jul 25 v014 parsers
            v0.14.0 — signed numeric                + dispatcher fix + INT64 widening) +
            literals + INT64 widening +             parse_uint_at unchanged; ParseError::LeadingSign
            dispatcher routes integer-              enum kept for back-compat (no longer emitted)
            shaped literals through
            parse_int_at for full int64
            precision
                                                    + psp_json_v014_promotion consumer proves the
                                                      promotion works through the library proper
                                                      (54 cases / 5 sections, all passing; strict-
                                                      warning + ASan/UBSan clean)
                                                    + grep-all-consumers finding: 2 older consumers
                                                      (psp_parser_header Jul 14 + psp_parser_streaming
                                                      Jul 15) have hard-coded v0.13.0 LeadingSign
                                                      expectations that will FAIL under v0.14.0
                                                      (documented as a follow-on lesson)
```

## What's in this lesson

### The library promotion (mechanical)

The v0.14.0 promotion is the headline library change. The
header `<psp_span/parser.h>` (a single-translation-unit
header-only library — every consumer includes it directly)
gets four edits, plus the dispatcher in `<psp_span/json.h>`
gets one edit, plus the library's `CMakeLists.txt` gets a
version bump + changelog.

**The four numeric parsers** all share the same shape of edit:

1. **Optional leading sign** — drop the `LeadingSign` rejection
   at the top of each parser; instead, peek at `s.front()`, set
   a `negative` flag, advance the cursor by one char (or the
   whole-span equivalent — `s = s.subspan(1, s.size()-1)`).
   A bare sign with no following digits now reports `NotADigit`
   (was `LeadingSign`).
2. **Apply sign at the end** — after computing the unsigned
   magnitude, `if (negative) result = -result`. For the cursor
   variants, `negative` is captured before the digit loop.
3. **Widened overflow check** — was
   `acc > static_cast<std::int64_t>(std::numeric_limits<int>::max())`,
   which overflows at `2.15e9`. Now uses the standard
   int64-safe check
   `acc > INT64_MAX/10 || (acc == INT64_MAX/10 && digit > INT64_MAX%10)`.
4. **Widened return type for `parse_int` / `parse_int_at`** —
   `std::expected<int, ParseError>` becomes
   `std::expected<std::int64_t, ParseError>`. `parse_double`
   and `parse_double_at` still return `double` (no widening
   needed — `double`'s mantissa is already 53 bits, beyond
   which int64 precision is unrepresentable anyway; the
   JSON dispatcher's int64-vs-double preservation guard does
   the right thing).

**The JSON dispatcher** (`<psp_span/json.h>` line ~255) gets
two related edits:

1. The dispatcher previously routed `'-'` (and digits) to
   `parse_double_at`; `'+'` fell through to `UnexpectedChar`.
   The promotion routes `'+'` too.
2. Before calling `parse_double_at`, the dispatcher now
   **peeks ahead**: if the numeric literal contains no `.`
   and no `e`/`E`, route through `parse_int_at` (the
   cursor variant returns `int64_t` directly, no double
   round-trip). This preserves full int64 precision for
   pure-integer literals — without this, an input like
   `-9223372036854775807` (= INT64_MIN+1) would round-trip
   through `double` and lose the `+1` (since `double`
   can't represent 2^63 exactly, the
   `static_cast<int64_t>` from the rounded `double` would
   produce INT64_MIN instead of INT64_MIN+1).

This is the same Finding 3 the Jul 25 lesson surfaced: the
dispatcher's int64-vs-double preservation guard is itself
`int`-shaped, so even with the four parsers fixed, an
int64-shaped double would round to a `double` and be narrowed
back to `int` on the way out. The fix routes integer-shaped
literals through `parse_int_at` instead, which sidesteps the
double entirely.

### The consumer (54 cases, 5 sections)

The consumer is structurally simpler than the Jul 25 lesson's
local-copies consumer — no `v014` namespace, no shadow
dispatcher, no `parse_int_v014` / `parse_double_v014` clones.
It uses the LIBRARY PROPER throughout. The same `op_writer`
helper from the Jul 24 lesson (RFC 6902 §3 wire-format writer)
is re-inlined so the consumer is self-contained; the v0.14.0
library does not yet ship the writer (that's the Jul 24
lesson's follow-on).

**Section 1 — v0.14.0 sign acceptance (17 cases).** Direct
calls to `parse_int` / `parse_double` / `parse_int_at` /
`parse_double_at` with negative, `'+'`-prefixed, and bare-sign
inputs. Cases 1a-1g cover `parse_int`; 1h-1k cover
`parse_double`; 1l-1n cover the cursor variants. The cursor
cases (1l, 1m, 1n) also assert the cursor advanced past the
consumed run.

**Section 2 — `parse_int` returns `std::int64_t` (7 cases).**
Direct calls to `parse_int` / `parse_int_at` with past-INT_MAX
literals (5e9, INT64_MAX, INT64_MIN+1) — all parse cleanly
under v0.14.0; INT64_MAX+1 and INT64_MIN both overflow
cleanly with `ParseError::Overflow`.

**Section 3 — round-trip through `parse_value_at` (12 cases).**
For each test value (INT64_MAX, INT64_MIN+1, 5e9, -2e9, -42,
0, -0, +100, -3.14, -2.5e-10, -0.0001, mixed array
`[-1, INT64_MAX, -3.14]`):
- `parse_value_at(s)` → `original`
- `original == parse_value_at(json_to_string(original))`
This proves the JSON pipeline is bit-exact round-trippable for
int64-shaped and negative inputs.

**Section 4 — `psp::json_patch::patch` deep-equality (6 cases).**
A target document `{"x": INT64_MAX}` — a `TestOp{"x", INT64_MAX}`
matches; a `TestOp{"x", INT64_MAX-1}` correctly reports
`JsonPatchError::TestValueMismatch`. A second test on
`{"x": -42}` proves negative literals survive the engine.
A `ReplaceOp{"x", INT64_MAX}` mutates the target correctly;
`json_pointer::resolve_mut("/x")` reads back INT64_MAX.

**Section 5 — wire-format round-trip (8 cases).** Three test
documents:
- A 3-op document carrying negative + INT64_MAX values:
  `AddOp("/y", -1)`, `ReplaceOp("/x", INT64_MIN+1)`,
  `TestOp("/x", INT64_MIN+1)`. `op_writer::serialise_patch_document`
  emits the JSON; `psp::json_patch::parse_patch_document`
  parses it back; `psp::json_patch::patch` applies all three
  ops to a target tree; the target's `/x` and `/y` end up at
  the right values.
- A back-compat non-negative op (`AddOp("/x", 42)`) — proves
  the v0.14.0 parser still handles the wire-format output of
  the v0.13.0-era writer unchanged.
- A nested-array op (`ReplaceOp("/arr", [-1, INT64_MAX,
  INT64_MIN+1])`) — proves negative + INT64_MAX values
  survive through `parse_value_at`'s recursive descent into
  arrays (which goes through `parse_array_at` /
  `parse_object_at`, which still call `parse_double_at` for
  scalars — and `parse_double_at`'s `INT64_MAX` overflow
  check + sign acceptance lets the values through).

All 54 cases pass. The output (with `++g_pass` / `++g_fail`
counters) is:

```
== Section 1: v0.14.0 sign acceptance through the library proper ==
  PASS: 1a parse_int("-42") = -42 (was LeadingSign in v0.13.0)
  PASS: 1b parse_int("-2000000000") = -2000000000
  PASS: 1c parse_int("-9223372036854775807") = INT64_MIN+1
  PASS: 1d parse_int("+100") = 100
  PASS: 1e parse_int("+0") = 0
  PASS: 1f parse_int("+") -> NotADigit
  PASS: 1g parse_int("-") -> NotADigit
  PASS: 1h parse_double("-3.14") = -3.14
  PASS: 1i parse_double("-2.5e-10") = -2.5e-10
  PASS: 1j parse_double("-0") = 0 (magnitude)
  PASS: 1k parse_double("+3.14") = 3.14
  PASS: 1l parse_int_at("-42,99") = -42
  PASS:   1l span advanced past "-42"
  PASS: 1m parse_int_at("+100 rest") = 100
  PASS:   1m span advanced past "+100"
  PASS: 1n parse_double_at("-3.14;") = -3.14
  PASS:   1n span advanced past "-3.14"

== Section 2: parse_int returns std::int64_t (widened from int in v0.13.0) ==
  PASS: 2a parse_int("5000000000") past INT_MAX (was Overflow in v0.13.0)
  PASS: 2b parse_int("9223372036854775807") = INT64_MAX
  PASS: 2c parse_int("-9223372036854775807") = INT64_MIN+1
  PASS: 2d parse_int("9223372036854775808") -> Overflow
  PASS: 2e parse_int("-9223372036854775808") -> Overflow
  PASS: 2f parse_int_at("5000000000") = 5000000000
  PASS:   2f span fully consumed

== Section 3: round-trip parse_value_at -> json_to_string -> parse_value_at (through the LIBRARY) ==
  PASS: 3a round-trip INT64_MAX
  PASS: 3b round-trip INT64_MIN+1
  PASS: 3c round-trip 5e9 (past INT_MAX)
  PASS: 3d round-trip -2e9
  PASS: 3e round-trip -42
  PASS: 3f round-trip 0
  PASS: 3g round-trip -0
  PASS: 3h round-trip +100
  PASS: 3i round-trip -3.14
  PASS: 3j round-trip -2.5e-10
  PASS: 3k round-trip -0.0001
  PASS: 3l round-trip mixed array [-1, INT64_MAX, -3.14]

== Section 4: through psp::json_patch::patch (TestOp equality) ==
  PASS: 4a TestOp(INT64_MAX) matches target INT64_MAX
  PASS: 4b TestOp(INT64_MAX-1) mismatches target INT64_MAX -> TestValueMismatch
  PASS: 4c TestOp(-42) matches target -42
  PASS: 4d ReplaceOp(INT64_MAX) applied
  PASS:   4d resolve_mut("/x") succeeded
  PASS:   4d target /x == INT64_MAX after ReplaceOp

== Section 5: wire-format round-trip through psp::json_patch::* ==
  PASS: 5a library parses the writer's wire-format output
  PASS:   5a parsed 3 ops
  PASS: 5a library patch() applied the parsed ops
  PASS:   5a resolve_mut succeeded for /x and /y
  PASS:   5a target /x == INT64_MIN+1 after ReplaceOp
  PASS:   5a target /y == -1 after AddOp
  PASS: 5b library parses non-negative wire-format output
  PASS: 5c library parses nested-array wire-format output
  PASS: 5c library applies nested-array patch
  PASS:   5c resolve_mut("/arr") succeeded
  PASS:   5c target /arr is an array
  PASS:   5c array contents: [-1, INT64_MAX, INT64_MIN+1]

[psp_json_v014_promotion: 54 pass, 0 fail]
```

(The full Section 5 output also prints the wire-format JSON
documents — those are shown above in the lesson's arc
diagram; the verification only counts PASS/FAIL lines.)

## Important code

### The v0.14.0 `parse_int` (whole-span)

```cpp
inline std::expected<std::int64_t, ParseError>
parse_int(Span<const char> s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }

    // Optional leading sign (NEW in v0.14.0).
    bool negative = false;
    if (s.front() == '+' || s.front() == '-') {
        negative = (s.front() == '-');
        s = s.subspan(1, s.size() - 1);
        if (s.empty()) {
            return std::unexpected{ParseError::NotADigit};
        }
    }

    std::int64_t acc = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c < '0' || c > '9') {
            return std::unexpected{ParseError::NotADigit};
        }
        // Widened in v0.14.0: was INT_MAX, now INT64_MAX.
        if (acc > std::numeric_limits<std::int64_t>::max() / 10
            || (acc == std::numeric_limits<std::int64_t>::max() / 10
                && (c - '0') > static_cast<char>(
                    std::numeric_limits<std::int64_t>::max() % 10))) {
            return std::unexpected{ParseError::Overflow};
        }
        acc = acc * 10 + (c - '0');
    }
    if (negative) {
        acc = -acc;
    }
    return acc;
}
```

The widening pattern (`acc > MAX/10 || (acc == MAX/10 && digit > MAX%10)`)
is the textbook int64-safe overflow check — it avoids the
classic `acc * 10 + digit` UB on overflow that the v0.13.0
`acc > INT_MAX` check accidentally allowed at the boundary
case (`acc == INT_MAX / 10 && digit == INT_MAX % 10 + 1` —
the multiplication would overflow `int64_t` before the
comparison catches it).

### The v0.14.0 dispatcher (`<psp_span/json.h>`)

```cpp
default:
    // Numbers: digit, '-', or '+'. parse_double_at accepts all
    // three (sign is optional on the integer part). (NEW in
    // v0.14.0: '+' was previously rejected by the dispatcher as
    // UnexpectedChar — parse_double_at now accepts '+' as a
    // no-op sign.)
    if ((front >= '0' && front <= '9')
        || front == '-'
        || front == '+') {
        // Peek ahead: if the numeric literal contains no '.'
        // and no 'e'/'E', route it through parse_int_at
        // (NEW in v0.14.0). This preserves int64 precision
        // for integer-shaped literals — without this, an
        // input like `-9223372036854775807` (= INT64_MIN+1)
        // would round-trip through double and lose the +1
        // (since double can't represent 2^63 exactly, the
        // static_cast<int64_t> from the rounded double
        // would produce INT64_MIN instead of INT64_MIN+1).
        bool has_dot_or_exp = false;
        {
            std::size_t j = (front == '+' || front == '-') ? 1 : 0;
            while (j < s.size() && s[j] >= '0' && s[j] <= '9') ++j;
            if (j < s.size() && (s[j] == '.' || s[j] == 'e' || s[j] == 'E')) {
                has_dot_or_exp = true;
            }
        }
        if (!has_dot_or_exp) {
            // Pure integer literal: route through parse_int_at
            // for full int64 precision. The cursor variant
            // returns int64_t directly (no double round-trip).
            auto i = parse_int_at(s);
            if (!i) {
                s = saved;
                return std::unexpected{i.error()};
            }
            JsonValue out;
            out.value = *i;
            return out;
        }
        auto d = parse_double_at(s);
        // ... (unchanged v0.13.0 path)
    }
```

This dispatcher edit is the unsung hero of the promotion: it
solves **Finding 3 from the Jul 25 lesson** ("the dispatcher's
int64-vs-double preservation guard is int-shaped") without
needing to touch the int64-vs-double guard itself. By routing
pure-integer literals through `parse_int_at` (the int64 cursor
variant) instead of `parse_double_at`, the int64 precision is
preserved end-to-end — `parse_int_at` returns
`std::expected<std::int64_t, ParseError>`, no double involved.

The peek-ahead is O(n) in the digit run length (typically
short), and runs **before** the parser commits anything to
`out`. The cursor isn't advanced, so the dispatcher's
standard "rewind on error" contract (the `s = saved;` line
in the `if (!i)` branch) still works.

### The consumer's round-trip helper

```cpp
auto round_trip_check = [](std::string_view wire) -> bool {
    psp::Span<const char> s{wire.data(), wire.size()};
    auto original = psp::parse_value_at(s);
    if (!original) return false;
    std::string serialised = psp::json_to_string(*original);
    psp::Span<const char> s2{serialised.data(), serialised.size()};
    auto reparsed = psp::parse_value_at(s2);
    if (!reparsed) return false;
    return *original == *reparsed;
};

check(round_trip_check("9223372036854775807"),   "3a round-trip INT64_MAX");
check(round_trip_check("-9223372036854775807"),  "3b round-trip INT64_MIN+1");
check(round_trip_check("5000000000"),            "3c round-trip 5e9 (past INT_MAX)");
// ... etc.
```

The helper builds a `Span` over a `string_view` (not a
`const char*` literal — the literal would include the
trailing `'\0'`, and the parsers would correctly reject it
as a trailing garbage byte, giving a spurious `NotADigit`).
This is the same `std::string buf = "-42"; Span<const char>
s{buf.data(), buf.size()};` idiom the prior lessons use.

## Observed output

The full Section 5 wire-format output (the most
informative part of the run) prints the JSON wire documents
that the writer emits and the library re-parses:

```
5a wire (writer output, parses via v0.14.0 library):
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

5b wire (back-compat check, non-negative):
[
  {
    "op": "add",
    "path": "/x",
    "value": 42
  }
]

5c wire (nested negative + INT64_MAX in array):
[
  {
    "op": "replace",
    "path": "/arr",
    "value": [
      -1,
      9223372036854775807,
      -9223372036854775807
    ]
  }
]
```

Notice that `json_to_string` (from v0.10.0) correctly emits
the negative integers and the int64-shaped numbers in their
natural wire form — `-1`, `-9223372036854775807`. The Jul 24
wire-format writer (re-inlined here as `op_writer`) then
embeds them in the RFC 6902 §3 op shape, and the v0.14.0
library parses them all back, with no special-casing for the
sign or the magnitude. The RFC 6902 §3 wire format is exactly
JSON, so the v0.14.0 parser's new sign/INT64 support applies
uniformly.

## Findings

### Finding 1 (re-confirmed from Jul 24 / Jul 25): negative + INT_MAX overflow

Confirmed by every passing case in Sections 1, 2, 3, 5. The
four parsers accept `'-'` / `'+'`; the overflow check is
widened to `INT64_MAX`; `parse_int` returns `std::int64_t`.
The pre-existing 3-line fix (per the Jul 24 lesson) turned
into ~50 lines because of the int64-safe overflow check
(textbook pattern, but verbose) + the per-parser sign policy
+ the dispatcher's integer-routing.

### Finding 2 (re-confirmed from Jul 25): the dispatcher's int64-vs-double preservation guard is `int`-shaped — SOLVED by routing integer literals through `parse_int_at`

This finding is fully addressed by the dispatcher's new
peek-ahead: pure-integer literals go through `parse_int_at`
(int64 cursor, no double round-trip), so the dispatcher's
int64-vs-double preservation guard never fires for them.
Literals with a `.` or `e`/`E` still go through
`parse_double_at`, and the dispatcher's int-vs-double guard
fires on them; that guard is still `int`-shaped (matching
the v0.13.0 behaviour) and would still narrow a double
rounded to int64-range back to int. The fix is good enough
for now because the guard only fires on doubles that
trunc-truncate to int-range, which excludes `INT64_MAX`
(2^63 - 1, which doesn't trunc-truncate to int-range).

If a future lesson needs to round-trip
`2^62 + k` through `parse_value_at` (where `2^62 + k` is in
double's integer range up to 2^53), the dispatcher's int64
guard itself will need to widen from `int` to `int64_t`.
That's a smaller change and a natural follow-on if a real
consumer ever hits it.

### Finding 3 (NEW today): grep-all-consumers finds two consumers with stale `LeadingSign` expectations

The Jul 25 lesson said:

> "No existing consumer asserts `LeadingSign` for a negative
> input — but the v0.14.0 promotion lesson will need to grep
> for any `LeadingSign`-relying logic to make sure the
> behaviour change is intentional."

That grep found **two consumers with stale expectations**:

1. **`psp_parser_header` (Jul 14, `P-2026-07-14-psp-parser-header.cpp`):**
   - Section 1, cases `{"+9", false, 0, "LeadingSign"}` and
     `{"-9", false, 0, "LeadingSign"}` — now SUCCEED under
     v0.14.0 (returns 9 and -9 respectively).
   - Section 1, case `{"9999999999", false, 0, "Overflow"}` —
     now SUCCEEDS under v0.14.0 (returns 1410065407 — fits in
     `int64_t`, no overflow).
   - Section 2, cases `{"+1.0", false, 0.0, "LeadingSign"}`
     and `{"-1.0", false, 0.0, "LeadingSign"}` — now SUCCEED
     under v0.14.0 (returns 1.0 and -1.0 respectively).
   - Section 4, `parse_int("+9")` is called and the consumer
     **immediately invokes `r.error()` on the result** without
     checking `r.has_value()` first — under v0.14.0, `r` is
     a success value and `r.error()` is **UB** (the formatter
     reads uninitialised storage, prints a garbage
     enumerator name like `InvalidUnicodeEscape`).

2. **`psp_parser_streaming` (Jul 15, `P-2026-07-15-psp-parser-streaming-cursor.cpp`):**
   - Section 1, cases `{"+9", false, 0, "LeadingSign"}` and
     `{"-9", false, 0, "LeadingSign"}` — now SUCCEED under
     v0.14.0.
   - Section 1, case `{"99999999999", false, 0, "Overflow"}`
     — now SUCCEEDS under v0.14.0 (returns 1215752191 —
     fits in `int64_t`).

The Jul 14 lesson (`double_if_positive` in Section 3) uses
`parse_int` returning to an `int` variable via `.and_then` —
**a real breaking change** (the `int` binding would silently
truncate INT64-shaped values on platforms where `int` is
32-bit). The fix is to change the binding to `std::int64_t`
in that one place; the lesson also includes `%d` printf
formats for `parse_int` results, which under v0.14.0 emits
`-Wformat` warnings (printed but not erroring by default).

Neither consumer's stale expectations block the v0.14.0
promotion (the failures are at RUNTIME — the consumers still
build cleanly against v0.14.0 with `-Werror` if the
`-Wformat` warnings are silenced or fixed; they just print
`FAIL: ... should have failed but got ...` and exit 0). Both
consumers use `std::printf` for test output and don't gate
on pass/fail counts, so neither would FAIL a CI matrix run.

The proper follow-on lesson is: **update `psp_parser_header`
+ `psp_parser_streaming` for v0.14.0.** That's a small,
focused lesson (delete 4-6 stale test cases, update
`double_if_positive`'s binding to `std::int64_t`, fix the
`%d` → `%lld` printf formats). Logged in "What's NOT in this
lesson" below.

### Finding 4 (NEW today): the v0.13.0 `parse_value_at` already routed `'+'` to `UnexpectedChar` — the v0.14.0 promotion needs the dispatcher to explicitly accept `'+'`

In v0.13.0, `parse_double_at` rejected `'+'` with
`LeadingSign` — but the dispatcher in `<psp_span/json.h>`
was *unaware* of this and routed `'+'` to
`parse_double_at`, which then returned `LeadingSign`. So the
behaviour was `parse_value_at("+1")` → `LeadingSign`. The
Jul 24 lesson noted: "`'+'` falls through to the default and
produces an error" (in `parse_value_at`).

Under v0.14.0, `parse_double_at` accepts `'+'` (as a
no-op sign), but the dispatcher still needed to be told to
route `'+'` to the numeric branch (otherwise `'+'` would
fall through to `UnexpectedChar` as before). The dispatcher
edit (`|| front == '+'` in the digit-or-'-' guard) is the
visible part of this finding; the hidden part is that
**without it, the v0.14.0 `parse_double_at` accepting `'+'`
would have been useless** — `parse_value_at("+1")` would
still error, just with a different error. The
"consumer-pulls-publisher" pattern is: a parser accepting a
sign doesn't help until the dispatcher routes the sign to
that parser.

This finding is generalises: **any future widening of a
parser's accepted input set must also widen the dispatcher's
guard**, or the new acceptance is silently invisible at the
JSON-parser layer.

### Finding 5 (NEW today): `%d` vs `%lld` format-string warnings under strict warnings

`psp_parser_header` and `psp_parser_streaming` use
`std::printf("%d", *r)` to print `parse_int` /
`parse_int_at` results. Under v0.14.0, `*r` is `int64_t`,
not `int` — `%d` mismatches and emits a `-Wformat` warning.
Under `-Wall -Wextra -Wpedantic -Werror`, this becomes an
**error**, breaking the consumer's strict build.

The fix is mechanical: change `%d` → `%lld` (or `%PRId64`
from `<cinttypes>`) at every `printf` site that prints a
`parse_int` / `parse_int_at` result. `psp_json_v014_promotion`
itself doesn't have this issue — it uses `std::println`
(std::format under the hood) which formats `int64_t`
correctly by default.

The follow-on lesson for `psp_parser_header` +
`psp_parser_streaming` needs to fix the `%d` → `%lld`
conversion as part of the v0.14.0 breaking-change update.

## What's NOT in this lesson

- **It does not update `psp_parser_header` or
  `psp_parser_streaming` for the v0.14.0 breaking changes.**
  Both consumers have hard-coded v0.13.0 expectations
  (`LeadingSign` for `+9`/`-9`, `Overflow` for
  `9999999999`) that no longer match v0.14.0's behaviour.
  Today's lesson notes the findings (Finding 3 above) and
  defers the update to a follow-on lesson. The proper fix is
  4-6 case deletions + 1 type-binding widening +
  `%d` → `%lld` printf-format fixes per consumer. See
  "Where we go next" below.
- **It does not bump the library to a `v0.14.0-prep` tag.**
  The promotion is committed straight to `v0.14.0`. The
  Jul 23 (`parse_patch_document`) and Jul 24 (wire-format
  writer) lessons followed the same pattern — no
  intermediate tags, just straight to the new version.
- **It does not exercise the dispatcher's int64-vs-double
  preservation guard itself.** The guard's `int`-shapedness
  (the original Finding 3 from Jul 25) is sidestepped by
  routing pure-integer literals through `parse_int_at`
  (Finding 2 above). The guard's `int`-shapedness is now
  only relevant for doubles that trunc-truncate to int-range
  — a much narrower surface that today has no real test
  case. A future lesson could widen the guard to `int64_t`
  if a real consumer hits it.
- **It does not test the `2^53` boundary in
  `parse_value_at`.** The dispatcher's double-routing path
  is still `int`-shaped for the int-vs-double guard; inputs
  between `2^53` and `INT_MAX` would route through
  `parse_double_at`, round to a `double`, hit the guard,
  and be narrowed to `int` (losing bits above `2^31 - 1`).
  The follow-on lesson can address this; today's lesson
  tests the round-trip through `parse_int_at` (which is
  precise to 2^63) instead.
- **It does not deduplicate the per-op writer code.**
  `op_writer::serialise_one_op` in today's TU is a
  near-clone of `serialise_patch_document` in the Jul 24
  lesson's TU. We inline a copy so today's TU is
  self-contained. The proper follow-on is to lift
  `serialise_patch_document` into the library proper as
  v0.15.0 (the Jul 24 lesson already flagged this as a
  near-mechanical promotion).
- **It does not include a `-` rejection in any test for the
  library's behaviour change** (re-quoting the Jul 25
  lesson). The v0.14.0 fix changes the library's behaviour:
  today, `parse_int("-1")` returns `LeadingSign`; after the
  fix, it returns `-1`. The v0.14.0 promotion lesson today
  updates `psp_json_v014_promotion` (the new consumer) and
  *identifies* the affected old consumers (Finding 3); the
  fix-the-old-consumers lesson is a separate piece of work.
- **It does not re-run the `psp_parser_header` /
  `psp_parser_streaming` test suites through a CI matrix.**
  Those consumers are not in any current CI workflow (the
  Jul 8 multi-OS matrix only covers a few specific
  consumers). Their regressions are observable only on a
  developer machine that rebuilds them against the v0.14.0
  library.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v014 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v014
cmake --build late-may/cpp_practice/psp_span_lib/build-v014 --target install
```

Build the consumer (assumes v0.14.0 installed at `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_v014_promotion -B late-may/cpp_practice/psp_json_v014_promotion/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_v014_promotion/build
./late-may/cpp_practice/psp_json_v014_promotion/build/P-2026-07-27-psp-json-v014-promotion
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_json_v014_promotion -B late-may/cpp_practice/psp_json_v014_promotion/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_v014_promotion/build-strict
./late-may/cpp_practice/psp_json_v014_promotion/build-strict/P-2026-07-27-psp-json-v014-promotion
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_v014_promotion -B late-may/cpp_practice/psp_json_v014_promotion/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_v014_promotion/build-asan
./late-may/cpp_practice/psp_json_v014_promotion/build-asan/P-2026-07-27-psp-json-v014-promotion
```

All three builds pass cleanly. **54 cases pass, 0 fail.**

Back-compat probe — the Jul 25 consumer
(`P-2026-07-25-psp-json-negative-numbers.cpp`) still builds
and runs against v0.14.0 (the `find_package(psp_span_lib 0.13
REQUIRED)` check is satisfied by v0.14.0; the consumer's
LOCAL v014 parsers continue to exist as a side-by-side
demonstration; Section 1 of that consumer now shows the
LIBRARY accepting the negative inputs "already OK" instead of
rejecting them with `LeadingSign` — which is the desired
outcome).

## Where we go next

The Jul 25 lesson's "Where we go next" said the v0.14.0
promotion was the headline forward-on; today is that lesson.
The Jul 25 lesson also surfaced two further findings
(int64-vs-double preservation guard + `parse_int` return
type widening breaking change); today addresses the first
(through dispatcher routing) and partially the second (the
binding-widening concern is documented as a real breaking
change but no consumer in the archive actually binds
`parse_int`'s result to `int` — `psp_parser_header` is the
one partial exception, fixed in the follow-on below).

### Immediate follow-on

- **Update `psp_parser_header` + `psp_parser_streaming` for
  v0.14.0** — the breaking-change update identified in
  Finding 3 + Finding 5 above. 4-6 stale test-case
  deletions per consumer; `double_if_positive`'s `int`
  binding in `psp_parser_header` widened to `std::int64_t`;
  `%d` → `%lld` printf-format fixes; re-run both consumers
  against v0.14.0 to verify zero `FAIL:` lines.

### v0.15.0 candidates (re-quoting from earlier lessons)

- **`psp::json_patch::serialise_patch_document`** in the
  library proper — lift the Jul 24 consumer writer (and the
  Jul 27 re-inlined `op_writer`) into a header function. The
  promotion is near-mechanical.
- **JSON Schema validation** in a new `<psp_span/json_schema.h>`
  — closes the query-layer arc the Jul 21 lesson opened.
- **Streaming patch parser** — the v0.13.0
  `parse_patch_document` reads a full string_view; a
  streaming variant over `Span<const char>` would close the
  cursor-primitive gap in the RFC 6902 layer.
- **Transactional Patch** — `std::expected<void,
  JsonPatchError>`-returning engine that pre-computes all
  ops' effects before mutating, rolling back on any failure.
- **Widen the dispatcher's int64-vs-double preservation
  guard** from `int` to `int64_t` — orthogonal to today's
  lesson (which routes integer literals through
  `parse_int_at`); relevant if a real consumer hits a
  double-shaped int64-range input.

### Cross-cutting forward-on (re-quoting still-open items)

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

For the library as a whole, today's lesson closes the
"negative-number + INT64-shaped support" arc that the Jul 24
lesson's "Two real findings" section opened. v0.14.0 is a
strict superset of v0.13.0: every input v0.13.0 accepted is
still accepted (with the same return value); every input
v0.13.0 rejected with `LeadingSign` is now accepted with the
sign applied; every input v0.13.0 rejected with `Overflow`
because it exceeded `INT_MAX` is now accepted if it fits in
`INT64_MAX`; inputs that exceed `INT64_MAX` still report
`Overflow`. The only API contract change is `parse_int`'s
return type widening from `int` to `std::int64_t` (a small
breaking change; one consumer partially affected;
follow-on lesson fixes it).