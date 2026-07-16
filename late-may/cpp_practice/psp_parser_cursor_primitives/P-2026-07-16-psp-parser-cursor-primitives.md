# P-2026-07-16 — non-numeric cursor primitives in `<psp_span/parser.h>` (v0.8.0)

## Headline

The Jul 15 lesson (`P-2026-07-15-psp-parser-streaming-cursor.cpp`)
shipped the **streaming numeric parsers** — `parse_int_at`,
`parse_uint_at`, `parse_double_at` — that take `Span<const char>&`
and shrink the span past the consumed digit run. The lesson flagged
two cursor-walking chores that still had to be done **inline** by
every consumer:

1. **Skipping leading whitespace** between tokens.
2. **Verifying (and consuming)** an expected delimiter character.

Today's lesson (v0.8.0) adds two small primitives to
`<psp_span/parser.h>` that handle both chores:

- **`psp::expect_char_at(Span<const char>&, char)`** — peek-and-consume
  a single expected char. On match: `s` shrinks by 1, returns
  `std::expected<bool, ParseError>{true}`. On mismatch: `s`
  unchanged, returns `std::unexpected{ParseError::UnexpectedChar}`.
  On empty input: `s` unchanged, returns
  `std::unexpected{ParseError::Empty}`.
- **`psp::skip_whitespace_at(Span<const char>&)`** — consume a
  leading run of ASCII whitespace (`' '`, `'\t'`, `'\n'`, `'\r'`).
  Always succeeds (zero-or-more whitespace is a valid result);
  returns `std::expected<bool, ParseError>{true}`.

The library version bumps from **0.7.0 → 0.8.0** because the new
primitives are a real new public capability, not a refactor. We also
add one new `ParseError` enumerator, `UnexpectedChar`, for the
delimiter-mismatch case.

This is the small "library-hygiene" lesson that the Jul 15 lesson's
"Next Steps" section explicitly identified as the most natural
follow-on: both primitives are 5-10 lines, but they clean up every
cursor-walking consumer (and they're the prerequisite for the
headline forward-on — a JSON parser built on the cursor primitives).

## Where this fits in the arc

```
Jun  9  std::move / rvalue refs                  move semantics
Jun 12  std::move_if_noexcept                     noexcept + move decision
Jun 13  enable_shared_from_this                   smart pointers + this
Jun 14  std::span (hand-rolled, psp::Span)        non-owning view, 1-D
Jun 15  std::visit + stateful visitors            variant visitation
Jun 16  Inventory multi-file                      separate compilation
Jun 28  psp_span as CMake INTERFACE library       installable target
Jun 30  psp_span_lib owns its instantiation TU    static library
Jul  1  install + find_package                    package config
Jul  3  CPack TGZ package                         redistributable
Jul  9  std::span (C++20)                         std version
Jul 12  std::expected (C++23)                     result type
Jul 13  wire std::expected into psp_span_lib      parser layer (consumer-side)
Jul 14  <psp_span/parser.h> in psp_span_lib       parser layer (library-side, whole-span)
Jul 15  streaming cursor (parse_*_at)             parser layer (library-side, cursor) — numeric
Jul 16  cursor primitives (expect_char_at,        parser layer (library-side, cursor) — non-numeric
              skip_whitespace_at)                 ← today
```

The Jul 15 lesson's "Next Steps" said:

> The most natural follow-on is `expect_char_at` and
> `skip_whitespace_at` as a small library-hygiene lesson — both are
> 5-line additions to `parser.h` and they clean up every cursor-walking
> consumer. After that, a JSON parser on the cursor primitives.

That is exactly today's lesson. After today, the **JSON parser built
on the cursor primitives** is the obvious next session — Section 4 of
this lesson is the seed of that parser (it already handles a flat
`{ "key": value, ... }` shape with full strict-validation, and the
whitespace / delimiter errors it catches are the ones a real JSON
parser would catch too).

## Why these primitives at all?

The Jul 15 lesson left every consumer writing inline boilerplate to
walk past whitespace and delimiters:

```cpp
// Jul 15 inline whitespace skip — Section 5 mixed-walk:
while (!s.empty() && s.front() == ' ') {
    s = s.subspan(1, s.size() - 1);
}

// Jul 15 inline delimiter check — Section 4 CSV-walk:
while (!s.empty() && (s.front() < '0' || s.front() > '9')) {
    s = s.subspan(1, s.size() - 1);
}
```

The whitespace loop is fine for plain spaces but misses `'\t'`,
`'\n'`, `'\r'`. The delimiter loop is LENIENT (it accepts ANY
non-digit), which means `"10 20"` (missing comma) silently parses
as `[10]` and the second number is just dropped on the floor.

With v0.8.0's primitives:

```cpp
// v0.8.0 — whitespace skip, all 4 ASCII whitespace chars:
psp::skip_whitespace_at(s);

// v0.8.0 — strict delimiter check:
auto c = psp::expect_char_at(s, ',');
if (!c) { /* missing-comma error, s unchanged, can recover */ }
```

The strict version catches malformed CSV the lenient version missed,
and the whitespace skipper handles all four ASCII whitespace chars
correctly (which the inline `== ' '` check didn't).

## The two new functions

### `psp::expect_char_at(Span<const char>&, char) -> std::expected<bool, ParseError>`

```cpp
inline std::expected<bool, ParseError>
expect_char_at(Span<const char>& s, char expected) noexcept;
```

- **On match**: `s` shrinks by 1, returns `std::expected<bool,
  ParseError>{true}`.
- **On mismatch** (non-empty `s` and `s.front() != expected`): `s`
  unchanged, returns `std::unexpected{ParseError::UnexpectedChar}`.
- **On empty `s`**: `s` unchanged, returns
  `std::unexpected{ParseError::Empty}`.

**Why `std::expected<bool, ...>` and not just `bool`?**

Returning the `bool` is enough for the happy path — the caller knows
which char they passed in. But by wrapping in `std::expected`, the
caller can ALSO recover from a missing delimiter: try a different
rule, log the error, return their own `std::unexpected`. Same shape
as the numeric parsers; same composability.

**Why a new `ParseError` enumerator (`UnexpectedChar`) instead of
reusing `NotADigit`?**

`NotADigit` would be misleading — there's no notion of "digit" in
`expect_char_at`. The semantic is "the front char wasn't the char I
expected," and `UnexpectedChar` says that exactly. The cost is one
more enum value (sizeof stays 4 bytes — it's still an `enum class`
with int underlying type).

### `psp::skip_whitespace_at(Span<const char>&) -> std::expected<bool, ParseError>`

```cpp
inline std::expected<bool, ParseError>
skip_whitespace_at(Span<const char>& s) noexcept;
```

- **Always succeeds**: consumes zero or more leading ASCII
  whitespace chars. Returns `std::expected<bool, ParseError>{true}`
  in all cases.
- **"ASCII whitespace"** = `' '`, `'\t'`, `'\n'`, `'\r'`. We do NOT
  include `'\v'` (vertical tab) or `'\f'` (form feed) — both are
  vanishingly rare in structured data, and a real lexer would use
  `std::isspace` / Unicode-aware whitespace handling. ASCII is what
  99% of JSON/CSV consumers need.

**Why return `std::expected<bool, ...>` if it never fails?**

Three reasons, in priority order:

1. **Consistency** with the other cursor primitives. Every other
   `*_at` in this header returns `std::expected<...>`, so
   `skip_whitespace_at` doing the same keeps the API uniform.
2. **Forward-compatibility**. If we ever want to make whitespace
   handling configurable (e.g. "treat NUL as whitespace too", "stop
   on `'\0'` for embedded NUL strings"), the return type is already
   in place to report a parse error.
3. **The success value `true` is informational** — it just means
   "consumed the leading whitespace successfully". The caller
   usually ignores it.

## Cursor mechanics — what didn't change

Both primitives use the same `psp::Span::subspan()` primitive (Jun 14)
to advance the cursor. No new span primitives needed:

```cpp
// expect_char_at shrink:
s = s.subspan(1, s.size() - 1);

// skip_whitespace_at shrink (i = count of leading whitespace chars):
s = s.subspan(i, s.size() - i);
```

That's the whole cursor-advancing story.

## Error-handling contract (the same two rules)

Both new primitives follow the same two-rule contract as the v0.7.0
streaming numeric parsers:

1. **On success, `s` shrinks** past the consumed prefix. The next
   character in `s` is whatever was originally after the consumed
   run.
2. **On failure, `s` is unchanged.** The caller can recover and try
   a different rule. The new `UnexpectedChar` error follows this
   contract — it's the first time the library has a `char`-shaped
   error (all prior errors were about the value being parsed, not
   the structural char being expected).

This is the property that makes the primitives **composable** —
Section 5 demonstrates the recovery pattern.

## Bumping the library version: 0.7.0 → 0.8.0

```cmake
project(psp_span_lib
    VERSION 0.8.0
    DESCRIPTION "psp_span static library with explicit Span<int>, Span<const int>, Span<double> instantiations + a parser header (parse_int/parse_double whole-span + parse_int_at/parse_uint_at/parse_double_at streaming numeric cursor + expect_char_at/skip_whitespace_at non-numeric cursor with std::expected<T, ParseError>); installable; find_package()-able; CPack-redistributable; bundles License + Readme"
)
```

Three things changed in `CMakeLists.txt`:

1. **VERSION** bumped from `0.7.0` to `0.8.0`.
2. **DESCRIPTION** updated to mention the new cursor primitives.
3. A version-bump comment added to the long top-of-file explanation
   block, alongside the existing `0.6 → 0.7` entry (Jul 15) and
   `0.5 → 0.6` entry (Jul 14).

The version bump is real, not cosmetic: any consumer that wants
the new primitives must use `find_package(psp_span_lib 0.8
REQUIRED)`. Both prior consumers (Jul 14 whole-span and Jul 15
streaming numeric) build and run unchanged against v0.8.0 — verified
empirically below.

## Section 1 — `expect_char_at`

```
== Section 1: psp::expect_char_at — peek-and-consume single char ==
  expect_char_at(",", ',') = true, s now = "" (advanced 1)
  expect_char_at(",rest", ',') = true, s now = "rest" (advanced 1)
  expect_char_at("x", 'x') = true, s now = "" (advanced 1)
  expect_char_at("xx", 'x') = true, s now = "x" (advanced 1)
  expect_char_at("\t", '\t') = true, s now = "" (advanced 1)
  expect_char_at("abc", ',') = error:UnexpectedChar, s = "abc" (unchanged)
  expect_char_at(",", 'x') = error:UnexpectedChar, s = "," (unchanged)
  expect_char_at("\t", ' ') = error:UnexpectedChar, s = "\t" (unchanged)
  expect_char_at("", ',') = error:Empty, s = "" (unchanged)
```

The interesting cases:

- **`",rest" → true, s = "rest"`** — match consumes the leading `,`,
  leaving the rest.
- **`"xx" → true, s = "x"`** — match consumes only the front `x`,
  leaves the rest. (Each call to `expect_char_at` is one char, not
  "consume while equal".)
- **`"\t" → match '\t'`** — works on any single byte; the contract
  is "compare-and-consume exactly one char", no special-casing of
  whitespace.
- **`"abc" → error:UnexpectedChar`** — front is `'a'`, not `','`;
  `s` is unchanged.
- **`"" → error:Empty`** — empty input is reported as `Empty` (not
  `UnexpectedChar`), matching the convention that "buffer ran out
  before we found what we wanted" is `Empty`.

## Section 2 — `skip_whitespace_at`

```
== Section 2: psp::skip_whitespace_at — consume ASCII whitespace run ==
  skip_whitespace_at(" ") -> "" (advanced 1)
  skip_whitespace_at("   ") -> "" (advanced 3)
  skip_whitespace_at("\t") -> "" (advanced 1)
  skip_whitespace_at("\n") -> "" (advanced 1)
  skip_whitespace_at("\r") -> "" (advanced 1)
  skip_whitespace_at("\t\n  \r\t") -> "" (advanced 6)
  skip_whitespace_at("   hello") -> "hello" (advanced 3)
  skip_whitespace_at("\t\tx") -> "x" (advanced 2)
  skip_whitespace_at(" \n abc") -> "abc" (advanced 3)
  skip_whitespace_at("\r\n42") -> "42" (advanced 2)
  skip_whitespace_at("hello") -> "hello" (advanced 0)
  skip_whitespace_at("42") -> "42" (advanced 0)
  skip_whitespace_at(",") -> "," (advanced 0)
  skip_whitespace_at("") -> "" (advanced 0)
```

All four ASCII whitespace chars are correctly consumed (`' '`,
`'\t'`, `'\n'`, `'\r'`). Mixed runs work — `"\t\n  \r\t"` is 6
whitespace chars and all 6 are skipped. The CRLF case `"\r\n42"`
skips both `'\r'` and `'\n'` correctly.

The zero-whitespace cases (`"hello"`, `"42"`, `","`, `""`) all
return success with advanced=0 — i.e. the function is a no-op
when there's nothing to skip.

## Section 3 — CSV walker rewrite (strict delimiters via `expect_char_at`)

```
== Section 3: CSV walker rewrite — strict delimiters via expect_char_at ==
  row = "10,20,30,40,50" -> [10, 20, 30, 40, 50]
  row = "1,2,3" -> [1, 2, 3]
  row = "100,200,300,400" -> [100, 200, 300, 400]
  row = "7" -> [7]
  row = "10 , 20 , 30" -> [10, 20, 30]
  row = "  1,  2,  3  " -> [1, 2, 3]
  row = "10 20" -> [10 ERROR(expect_char_at:'2')]  <-- malformed
  row = "10,,20" -> [10ERROR(parse_int_at:NotADigit)]  <-- malformed
```

The first six rows are well-formed (with various whitespace
flavors); the walker handles them all. The last two rows are
**malformed**:

- **`"10 20"`** — the missing comma is detected: after parsing `10`,
  the walker calls `skip_whitespace_at(s)` (which skips the space)
  and then `expect_char_at(s, ',')`, which fails because the front
  char is `'2'`. The walker reports
  `ERROR(expect_char_at:'2')` and marks the row malformed.
- **`"10,,20"`** — the empty-between-commas case: after parsing
  `10`, the walker consumes the comma, calls
  `skip_whitespace_at(s)` (no-op), then `parse_int_at(s)` which
  fails with `NotADigit` because the front char is `,`.

**The Jul 15 Section 4 CSV walker was LENIENT** — it accepted any
non-digit character between tokens, so it would silently swallow
`"10 20"` as `[10]` and drop `20` on the floor. The v0.8.0
walker is STRICT — it requires exactly one `,` between tokens and
catches both missing-comma and empty-between-comma cases. This is
the practical payoff of `expect_char_at`.

The walker code (with cursor primitives) is about 20 lines and
contains **no manual `while` loops** for whitespace or delimiter
handling — every step is a one-liner.

## Section 4 — JSON-ish walker (the headline demo)

```
== Section 4: JSON-ish walker — skip_whitespace_at + expect_char_at + parse_double_at ==
  parse_json_object("{\"x\": 42}") -> {"x": 42} (s remaining = "")
  parse_json_object("{\"x\": 42, \"y\": 3.14}") -> {"x": 42, "y": 3.14} (s remaining = "")
  parse_json_object("{\"a\": 1, \"b\": 2, \"c\": 3}") -> {"a": 1, "b": 2, "c": 3} (s remaining = "")
  parse_json_object("{}") -> {} (s remaining = "")
  parse_json_object("  {\"x\":42}  ") -> {"x": 42} (s remaining = "")
  parse_json_object("{\n  \"x\": 1,\n  \"y\": 2\n}") -> {"x": 1, "y": 2} (s remaining = "")
  parse_json_object("{\"x\" 42}") = error:UnexpectedChar (as expected)
  parse_json_object("{\"x\": 42,}") = error:UnexpectedChar (as expected)
  parse_json_object("{\"x\": 42") = error:Empty (as expected)
  parse_json_object("") = error:Empty (as expected)
  parse_json_object("{\"x\": \"string\"}") = error:NotADigit (as expected)
```

This is the headline forward-on from the Jul 15 lesson's "Next
Steps" — a JSON-ish walker built on the cursor primitives. The
walker handles the shape:

```
'{' STRING ':' VALUE (',' STRING ':' VALUE)* '}'
```

where `STRING` is `"key"` (no escape support yet — that's a future
`parse_string_at(s, '"')` primitive) and `VALUE` is parsed as a
`double` (which covers int values too via the integer-only branch
in `parse_double_at`).

**The walker body is about 25 lines**, mostly one-liners:

```cpp
auto ws = psp::skip_whitespace_at(s);        // initial whitespace
auto open = psp::expect_char_at(s, '{');     // opening brace
// ... scan key ...
auto colon = psp::expect_char_at(s, ':');    // colon
auto val = psp::parse_double_at(s);          // numeric value
// ... more pairs ...
auto close = psp::expect_char_at(s, '}');    // closing brace
```

Compare with the equivalent walker that would have to roll its own
whitespace skipper and delimiter checker (using `while` loops and
`if` chains): it'd be 50+ lines, with three different `while` loops
for whitespace, colon-checking, and brace-checking.

**The malformed-input cases are exactly what a real JSON parser
catches**:

- `"{\"x\" 42}"` — missing colon. The walker reads the key, then
  `skip_whitespace_at`, then `expect_char_at(s, ':')` fails with
  `UnexpectedChar` because the front char is `'4'`.
- `"{\"x\": 42,}"` — trailing comma. After parsing `42`, the walker
  sees the `,`, consumes it, loops back, tries to read another
  key. `expect_char_at(s, '"')` fails with `UnexpectedChar`
  because the front char is `'}'`.
- `"{\"x\": 42"` — missing close brace. After parsing `42`, the
  walker expects `,` or `}`. The buffer is empty, so
  `expect_char_at(s, ',')` fails with `Empty`.
- `""` — empty input. Initial `skip_whitespace_at` succeeds
  (no-op), then `expect_char_at(s, '{')` fails with `Empty`.
- `"{\"x\": \"string\"}"` — string value (not a number). The walker
  tries `parse_double_at`, which fails with `NotADigit` because the
  front char is `'"'`.

This is the prototype of a real JSON parser. The next lesson will
add `parse_string_at(s, '"')` (quoted-string cursor with escape
handling) and `parse_bool_at` / `parse_null_at`, and the walker
will grow into a real JSON object / array parser.

## Section 5 — Failure recovery (the "composable" payoff)

```
== Section 5: failure recovery — s unchanged on mismatch, can try again ==
  input=",abc" try={',', ';'} -> matched, s="abc" (advanced 1, want 1)  [OK]
  input=";abc" try={',', ';'} -> matched, s="abc" (advanced 1, want 1)  [OK]
  input=":abc" try={',', ';'} -> error:UnexpectedChar, s=":abc" (advanced 0, want 0)  [OK]
  input="x" try={'x', 'y'} -> matched, s="" (advanced 1, want 1)  [OK]
  input="" try={','} -> error:Empty, s="" (advanced 0, want 0)  [OK]
```

This is the structural payoff of the failure-leaves-s-unchanged
contract. The caller can ATTEMPT an `expect_char_at`, and if the
char isn't there, fall back to a different rule **without losing
the cursor position**. Same idea as the Jul 15 lesson's
"stream past the consumed prefix; on failure, the prefix is not
committed" rule for `parse_*_at`.

Example walkthrough for `input=";abc" try={',', ';'}`:

1. Try `expect_char_at(s, ',')`: front is `';'`, returns
   `UnexpectedChar`, `s` unchanged.
2. Try `expect_char_at(s, ';')`: front is `';'`, returns `true`,
   `s` shrinks to `"abc"`.
3. Done — outcome is "matched".

Without `expect_char_at`, this kind of "peek-and-decide" requires
hand-rolling `if (s.front() == ',' || s.front() == ';')` chains
that get hairy fast and tend to grow bugs (e.g. accidentally
consuming the wrong char in a fall-through branch).

## Section 6 — `sizeof` and toolchain probes

```
== Section 6: sizeof / feature probes ==
  sizeof(bool)                                       = 1
  sizeof(ParseError)                                 = 4
  sizeof(psp::Span<const char>)                      = 16
  sizeof(std::expected<bool, ParseError>)            = 8
  sizeof(std::expected<int, ParseError>)             = 8
  sizeof(std::expected<unsigned, ParseError>)       = 8
  sizeof(std::expected<double, ParseError>)          = 16
  ParseError names:
    -> "Empty"
    -> "LeadingSign"
    -> "NotADigit"
    -> "Overflow"
    -> "BadExponent"
    -> "MissingFraction"
    -> "UnexpectedChar"
  __cpp_lib_expected                                 = 202211
  __cpp_lib_span                                     = 202002
```

Three observations:

1. **`sizeof(std::expected<bool, ParseError>) == 8`** — matches
   `expected<int, ParseError>` because the EBO packing puts the
   bool discriminator into alignment slack (same as the Jul 14
   measurement, just with a 1-byte payload instead of 4-byte).
2. **`sizeof(ParseError) == 4`** even with the new `UnexpectedChar`
   enumerator. `enum class` with int underlying type stays at 4
   bytes regardless of enumerator count (1..many all fit in the
   same int).
3. **Toolchain confirms** `__cpp_lib_expected == 202211` (C++23
   final) and `__cpp_lib_span == 202002`. The ParseError
   formatter covers all 7 enumerators — verified by printing
   each one.

## The consumer's `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(psp_parser_cursor_primitives_consumer ...)

find_package(psp_span_lib 0.8 REQUIRED)   # bumped from 0.7

add_executable(P-2026-07-16-psp-parser-cursor-primitives
    P-2026-07-16-psp-parser-cursor-primitives.cpp
)
target_link_libraries(P-2026-07-16-psp-parser-cursor-primitives
    PRIVATE psp_span_lib::psp_span_lib
)
```

vs the Jul 15 consumer:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-16-psp-parser-cursor-primitives
```

The version requirement bumped from 0.7 to 0.8 because the cursor
primitives are new in v0.8.0. The rest is unchanged from the Jul 15
pattern.

## Build & run (what I actually ran)

```bash
# 1. Rebuild psp_span_lib 0.8.0 and install into /tmp/psp_install.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_span_lib
rm -rf build-cron
cmake -S . -B build-cron -DCMAKE_BUILD_TYPE=Release
cmake --build build-cron
cmake --install build-cron --prefix /tmp/psp_install

# 2. Configure + build the consumer with find_package.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_cursor_primitives
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-16-psp-parser-cursor-primitives

# 3. ASan build of the consumer.
cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-07-16-psp-parser-cursor-primitives

# 4. ASan build of psp_span_lib itself (proves the library's
#    single instantiation TU is clean too).
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_span_lib
cmake -S . -B build-asan-lib -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=ON
cmake --build build-asan-lib

# 5. Strict-warnings sanity check (the CMake build uses -Wall
#    -Wextra -Wpedantic via the library's PUBLIC compile_features,
#    but a direct clang++ invocation is the belt-and-suspenders
#    proof):
clang++ -Wall -Wextra -Wpedantic -std=c++23 -O2 \
    -I /tmp/psp_install/include \
    -c P-2026-07-16-psp-parser-cursor-primitives.cpp \
    -o /tmp/strict_check.o     # 0 warnings, exit 0

# 6. CPack: rebuild the redistributable archive.
cd build-cron && cpack -C Release
# Produces psp_span_lib-0.8.0-Darwin.tar.gz — confirmed by
# `tar tzf` to contain include/psp_span/parser.h with 15
# references to the new cursor primitives (expect_char_at /
# skip_whitespace_at).

# 7. "Ship-it" test: a fresh consumer install from the TGZ alone.
cp build-cron/psp_span_lib-0.8.0-Darwin.tar.gz /tmp/psptest_v08/
cd /tmp/psptest_v08 && tar xzf psp_span_lib-0.8.0-Darwin.tar.gz
# Configure with the extracted install tree (NOT /tmp/psp_install).
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_cursor_primitives
cmake -S . -B build-fresh -DCMAKE_PREFIX_PATH=/tmp/psptest_v08/psp_span_lib-0.8.0-Darwin
cmake --build build-fresh
./build-fresh/P-2026-07-16-psp-parser-cursor-primitives
# Configure finds v0.8.0, consumer builds, program runs to
# completion. End-to-end "ship-it" verified.

# 8. Backward-compat check: the Jul 14 and Jul 15 consumers
#    still work against v0.8.0 (they don't use the new
#    primitives and the span + numeric-parser layer is
#    unchanged).
clang++ -Wall -Wextra -Wpedantic -std=c++23 -O2 \
    -I /tmp/psp_install/include \
    ../psp_parser_header/P-2026-07-14-psp-parser-header.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/jul14_retest
/tmp/jul14_retest   # exit 0; the only warning is a pre-existing
                    # unused-function in the Jul 14 source, not
                    # related to today's changes.
clang++ -Wall -Wextra -Wpedantic -std=c++23 -O2 \
    -I /tmp/psp_install/include \
    ../psp_parser_streaming/P-2026-07-15-psp-parser-streaming-cursor.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/jul15_retest
/tmp/jul15_retest   # exit 0; no warnings.
```

All eight steps above compiled and ran to exit 0. ASan reports
no memory errors in any build. The Jul 14 and Jul 15 consumers
build and run unchanged against v0.8.0 — the only difference is
that they link against the same archive with additional header
content (parser.h is bigger but no new symbols).

## Bug caught during the lesson

Two bugs were caught while writing the program:

1. **Wrong expected `advanced` count in Section 2.** My initial test
   case was `{"\t\tx", "x", 3}` (claim: 3 chars skipped). But
   `"\t\tx"` is 2 tabs + x = 3 total chars; the function skips the
   2 tabs and leaves `x` (1 char), so `advanced = 3 - 1 = 2`. The
   expected value was wrong (I confused "consumed all whitespace"
   with "consumed all chars"). Fixed by correcting the assertion.

2. **`parse_json_object` discarding an error on the closing quote
   of the key.** The original Section 4 walker scanned the key
   text via a hand-written loop, then called
   `psp::expect_char_at(s, '"')` to consume the closing quote —
   but DIDN'T CHECK THE RETURN VALUE. When the closing quote was
   NOT at the front of `s` (because the scan had only advanced the
   scan-index, not the span), `expect_char_at` failed with
   `UnexpectedChar` and was silently dropped. The walker then
   tried to parse the colon and failed with the wrong error.
   Caught by the Section 4 output (every JSON input was failing
   with `UnexpectedChar`). Fixed by replacing the scan-then-call
   pattern with `s = s.subspan(key_start + 1, ...)`, which advances
   past both the key text AND the closing quote atomically, and
   removed the error-discarding call entirely. The lesson's
   `bug fixed during development` style: the assertion is the test
   harness, and the test failed → the bug was fixed → the test
   passed.

The lesson's value proposition includes the BUG stories, not just
the success cases — bugs are how the contract gets verified.

## What I didn't cover (next-session candidates)

- **`parse_string_at(s, '"')`** — the missing primitive for the
  JSON walker. Quoted-string cursor with escape handling. The
  Section 4 walker currently hand-rolls a non-escape-aware string
  scan; `parse_string_at` would replace it with one call.
- **`parse_bool_at(s)` / `parse_null_at(s)`** — JSON literal
  parsers. Same shape as `parse_*_at` but for `true` / `false` /
  `null`.
- **JSON parser built on the cursor primitives** — the headline
  forward-on from the Jul 15 lesson. Section 4 of THIS lesson is
  the seed of that parser. The next lesson would extend the
  walker to handle:
  - arrays (`[value, value, ...]`)
  - string values
  - bool / null values
  - nested objects
  That's the right size for one lesson.
- **Padded integer parsing (`parse_int_padded`)** — for hex /
  octal / binary literals. Different base, same cursor shape.
  Not strictly a forward-on for JSON, but a natural extension
  for any structured-data format that uses hex.
- **A test harness using Catch2 / doctest** — the library still
  has no formal unit tests; the lesson programs ARE the tests,
  but a proper `tests/` directory would catch regressions on
  future header edits. This is a "library hygiene" move, not a
  "new C++" move.
- **`std::expected` and coroutines** — `co_return std::expected<T,E>`
  composes with the parser layer for an async pipeline.

The most natural follow-on is **`parse_string_at` plus a real JSON
parser** — Section 4 is already most of the work; adding string /
bool / null / array support is the rest of one lesson.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        ├── psp_span_lib/                                         # MODIFIED (library)
        │   ├── CMakeLists.txt                                   #    v0.7.0 -> v0.8.0
        │   └── include/psp_span/
        │       └── parser.h                                      # MODIFIED (cursor primitives added)
        │                                                          #   + expect_char_at, skip_whitespace_at
        │                                                          #   + ParseError::UnexpectedChar
        │                                                          #   + std::formatter<ParseError> extended
        └── psp_parser_cursor_primitives/                         # NEW (the consumer)
            ├── CMakeLists.txt                                   #    consumer with find_package(psp_span_lib 0.8)
            ├── P-2026-07-16-psp-parser-cursor-primitives.cpp     #    the program (6 sections)
            └── P-2026-07-16-psp-parser-cursor-primitives.md      #    this file
```

The library gains two new functions in `parser.h`, one new
`ParseError` enumerator, and a version bump. The consumer directory
is brand new. The `psp_span_lib` install tree (the part that gets
bundled into CPack archives and shipped to consumers) now ships a
parser header with whole-span + streaming-numeric + non-numeric
cursor APIs:

```
include/psp_span/
├── span.h        (C++17 — psp::Span<T, Extent>)
└── parser.h      (C++23 — psp::parse_int/parse_double whole-span
                   + psp::parse_int_at/parse_uint_at/parse_double_at
                     streaming numeric cursor
                   + psp::expect_char_at/skip_whitespace_at
                     non-numeric cursor
                   with std::expected<T, ParseError>)
lib/
├── libpsp_span_lib.a
└── cmake/psp_span_lib/
    ├── psp_span_libConfig.cmake
    ├── psp_span_libConfigVersion.cmake
    └── psp_span_libTargets.cmake
```

The CPack TGZ `psp_span_lib-0.8.0-Darwin.tar.gz` contains the
above plus `License.txt` + `Readme.txt` at the install root.
Verified by extracting to a fresh `/tmp/psptest_v08` and rebuilding
the consumer against it — `find_package(psp_span_lib 0.8 REQUIRED)`
succeeded, consumer compiled, program ran.

## Next steps

The `psp_span_lib` release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs
  NSIS or .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.8.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform
  and SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow
  feature work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **`parse_string_at(s, '"')`** — quoted-string cursor with escape
  handling. The missing primitive for the JSON walker.
- **JSON parser built on the cursor primitives** — Section 4 of
  this lesson is the seed; the next lesson adds string / bool /
  null / array support.
- **`std::expected` and coroutines** — `co_return std::expected<T,E>`
  composes with the parser layer for an async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it, the Jul
  10 Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray)
  — the standard version of the Jul 11 hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear
  algebra built on mdspan.

The most natural follow-on is **`parse_string_at` plus a real JSON
parser built on the cursor primitives** — Section 4 is already most
of the work. After that, the JSON parser becomes a candidate for
shipping as a separate header (`<psp_span/json.h>`?) so the
streaming parser layer stays focused on the small numeric /
structural primitives.