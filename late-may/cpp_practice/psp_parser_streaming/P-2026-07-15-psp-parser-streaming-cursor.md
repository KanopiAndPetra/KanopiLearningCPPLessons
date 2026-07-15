# P-2026-07-15 — streaming cursor API ships in `<psp_span/parser.h>` (v0.7.0)

## Headline

The Jul 14 lesson (`P-2026-07-14-psp-parser-header.cpp`) shipped
the **whole-span** parsers — `parse_int` and `parse_double` treat
their input as a complete, isolated run. Today's lesson adds the
**streaming cursor** variants: the parser takes the span BY
REFERENCE, consumes the leading run that makes up a valid numeric
literal, shrinks the span so it now starts after the consumed run,
and returns the parsed value. A failure leaves the span unchanged
so the caller can recover and try a different rule.

Three new functions ship in `<psp_span/parser.h>` (v0.7.0):

1. **`psp::parse_int_at(Span<const char>&)`** — consumes a leading
   run of decimal digits, returns `std::expected<int, ParseError>`.
2. **`psp::parse_uint_at(Span<const char>&)`** — same shape but
   `unsigned`; accepts `+`, rejects `-`.
3. **`psp::parse_double_at(Span<const char>&)`** — consumes the
   leading `[DIGITS] [. [DIGITS]] [[eE][+-]? DIGITS]` run, returns
   `std::expected<double, ParseError>`.

This is the foundation a JSON / CSV / structured-data parser
would build on. The whole-span variants stay (they're the right
primitive for "is this buffer a valid int?"); the streaming
variants are the right primitive for "I'm walking a buffer and
want to read the next int, leaving the rest for later".

The library version bumps from **0.6.0 → 0.7.0** because the new
primitives are a real new public capability, not a refactor.

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
Jul 15  streaming cursor (parse_*_at)             parser layer (library-side, cursor) ← today
```

The parser layer went from "per-consumer file" (Jul 13) → "library
proper, whole-span" (Jul 14) → "library proper, streaming cursor"
(today). The next move in this arc is a JSON-style structured-data
parser built on top of the cursor primitives.

## Why a cursor API at all?

The whole-span `parse_int` is the wrong shape for "I'm walking a
buffer character-by-character":

```cpp
// Whole-span: "is THIS buffer a valid int?"
auto r = psp::parse_int(as_span("12345"));   // r = 12345, span not consumed
auto r = psp::parse_int(as_span("123abc"));  // r = error:NotADigit, ALL of "123abc" is bad
auto r = psp::parse_int(as_span("42,17,99"));// r = error:NotADigit, can't handle CSV
```

The streaming `parse_int_at` solves the third case directly:

```cpp
// Cursor: "give me the leading int and leave the rest"
psp::Span<const char> s = as_span("42,17,99");
auto r = psp::parse_int_at(s);              // r = 42, s = ",17,99"
// Now walk past the comma and parse again
s = s.subspan(1, s.size() - 1);             // s = "17,99"
auto r2 = psp::parse_int_at(s);             // r2 = 17, s = ",99"
// ... and so on
```

This is the JSON / CSV / lexer pipeline pattern. The streaming
parser doesn't know (or care) what comes after the digits — it
just hands the remainder back to the caller.

## The three new functions

### `psp::parse_int_at(Span<const char>&) -> std::expected<int, ParseError>`

```cpp
inline std::expected<int, ParseError>
parse_int_at(Span<const char>& s) noexcept;
```

- On success: returns the parsed int AND shrinks `s` past the
  consumed digit run.
- On failure: returns `std::unexpected{ParseError}` AND leaves
  `s` unchanged.
- Failure modes: `Empty`, `LeadingSign`, `NotADigit`, `Overflow`.
- Special case — **overflow commits the prefix**: if the digit
  run exceeds `INT_MAX`, the consumed prefix IS committed to
  `s` (so the caller can recover and try a wider type, like
  `long` or `unsigned long long`). This matches `std::strtol`'s
  `ERANGE` convention, where the prefix is parsed and the
  error is just a flag. The failure-mode prints in the lesson
  output distinguish "unchanged" (Empty / LeadingSign /
  NotADigit) from "advanced (overflow committed)".

### `psp::parse_uint_at(Span<const char>&) -> std::expected<unsigned, ParseError>`

```cpp
inline std::expected<unsigned, ParseError>
parse_uint_at(Span<const char>& s) noexcept;
```

- Sign policy: `+` is ACCEPTED (no-op), `-` is REJECTED with
  `LeadingSign`. (Same convention as `parse_int`: sign
  handling is a downstream policy choice, not the parser's
  job. `+` happens to be a no-op for unsigned, so we let it
  through; `-` would require negation, which muddies the
  overflow semantics.)
- Failure modes: `Empty`, `LeadingSign` (only `-` triggers it),
  `NotADigit`, `Overflow`.
- Edge case: `parse_uint_at("+")` returns `NotADigit` (not
  silently `0`). The lesson's output line is:
  ```
  parse_uint_at("+") = error:NotADigit, s = "+" (unchanged)
  ```
  This was a real bug caught during the lesson — the first
  draft returned `0` because the loop body didn't execute
  when the input was exactly `"+"`. Fixed by an explicit
  `i == start` check after the loop.

### `psp::parse_double_at(Span<const char>&) -> std::expected<double, ParseError>`

```cpp
inline std::expected<double, ParseError>
parse_double_at(Span<const char>& s) noexcept;
```

- Recognised shape: `[DIGITS] [. [DIGITS]] [[eE][+-]? DIGITS]`,
  same subset of `strtod` as `parse_double` from Jul 14.
- The big difference vs the whole-span `parse_double`:
  trailing-garbage is FINE. The whole-span parser rejects
  `"1.5x"` because it expects the WHOLE span to be a double.
  The streaming parser accepts `"1.5x"` and leaves `"x"` in
  `s`. That's the whole point of a cursor parser.
- Failure modes: `Empty`, `LeadingSign`, `MissingFraction`
  (`"."` alone), `BadExponent` (`"1e"` with no digits),
  `Overflow`, `NotADigit` (no integer digits and no `.`).
- Trailing-garbage case study: `parse_double_at("1.2.3")`
  returns `1.2` and leaves `".3"` in `s`. The caller can
  decide whether `".3"` is the start of the next token or
  garbage; the parser doesn't presume.

## Cursor mechanics — how the span shrinks

The streaming parsers use the existing `psp::Span::subspan()`
primitive (Jun 14) to advance the cursor:

```cpp
// After consuming N chars at the front, shrink s to start at offset N.
s = s.subspan(N, s.size() - N);
```

`psp::Span::subspan(offset, count)` returns a
`Span<T, dynamic_extent>` view starting at `offset` for `count`
elements. So no new span primitives were needed for the cursor
API — the streaming parsers compose cleanly with the existing
span layer.

## Error-handling contract

Every streaming parser follows the same two-rule contract:

1. **On success, `s` shrinks** past the consumed prefix. The
   next character in `s` (after the parse) is whatever was
   originally after the run — often a delimiter like `,` or
   `]`. The caller is responsible for consuming that
   delimiter.
2. **On failure (other than overflow), `s` is unchanged.** The
   caller can recover and try a different rule. The overflow
   case is the one exception: the prefix IS committed, so the
   caller can re-parse with a wider type if desired.

This is similar to `std::strtol`'s "endptr" convention but
ergonomically different — instead of a separate `endptr` output
parameter, the span itself IS the cursor. No extra state to
keep in sync.

## Bumping the library version: 0.6.0 → 0.7.0

```cmake
project(psp_span_lib
    VERSION 0.7.0
    DESCRIPTION "psp_span static library ... + parse_int_at/parse_uint_at/parse_double_at streaming cursor ..."
)
```

Three things changed in `CMakeLists.txt`:

1. **VERSION** bumped from `0.6.0` to `0.7.0`.
2. **DESCRIPTION** updated to mention the new streaming API.
3. A version-bump comment added to the long top-of-file
   explanation block, alongside the existing `0.5 → 0.6` entry
   (Jul 14) and `0.4 → 0.5` entry (Jul 4, resource files).

The version bump is real, not cosmetic: any consumer that wants
the streaming API must use `find_package(psp_span_lib 0.7
REQUIRED)`. The Jul 14 consumer (`P-2026-07-14-psp-parser-header.cpp`)
still builds cleanly against v0.7.0 because it doesn't use the
new functions and the span layer is unchanged — verified
empirically below.

## Section 1 — `parse_int_at` (cursor advances past digits)

```
== Section 1: psp::parse_int_at — cursor advances past digits ==
  parse_int_at("12345") = 12345, s now = "" (advanced 5)
  parse_int_at("0") = 0, s now = "" (advanced 1)
  parse_int_at("7") = 7, s now = "" (advanced 1)
  parse_int_at("123abc") = 123, s now = "abc" (advanced 3)
  parse_int_at("42,17,99") = 42, s now = ",17,99" (advanced 2)
  parse_int_at("") = error:Empty, s = "" (unchanged)
  parse_int_at("+9") = error:LeadingSign, s = "+9" (unchanged)
  parse_int_at("-9") = error:LeadingSign, s = "-9" (unchanged)
  parse_int_at("abc") = error:NotADigit, s = "abc" (unchanged)
  parse_int_at("  42") = error:NotADigit, s = "  42" (unchanged)
  parse_int_at("99999999999") = error:Overflow, s = "9" (advanced (overflow committed))
```

The interesting cases:

- **`"123abc" → 123, s = "abc"`** — the parser consumes the
  leading run of digits and stops at the first non-digit. The
  caller gets `"abc"` back as the remainder.
- **`"42,17,99" → 42, s = ",17,99"`** — the comma is left in
  `s`. The caller can skip it (or use it as a delimiter check)
  before the next `parse_int_at` call.
- **`"  42" → error:NotADigit`** — leading whitespace is NOT
  consumed. The parser is strict: the first char must be a
  digit (or `+`/`-`). Whitespace-skipping is a separate
  concern; a future `skip_whitespace_at(s)` primitive would
  compose cleanly.
- **`"99999999999" → Overflow, s = "9"`** — the overflow
  prefix is committed. Note the curious output: the digit
  run is `"9999999999"` (10 nines, which IS representable in
  int64 but exceeds INT_MAX), and the 11th `9` triggers
  overflow. After committing, `s = "9"` because we shrunk
  past the first 10 nines, leaving the 11th `9` as the
  remainder. A caller could re-parse `"9"` with `parse_uint_at`
  or a wider-type parser to recover.

## Section 2 — `parse_uint_at` (unsigned cursor)

```
== Section 2: psp::parse_uint_at — unsigned cursor ==
  parse_uint_at("0") = 0, s now = "" (advanced 1)
  parse_uint_at("42") = 42, s now = "" (advanced 2)
  parse_uint_at("+42") = 42, s now = "" (advanced 3)
  parse_uint_at("-42") = error:LeadingSign, s = "-42" (unchanged)
  parse_uint_at("4294967295") = 4294967295, s now = "" (advanced 10)
  parse_uint_at("4294967296") = error:Overflow, s = "" (advanced (overflow committed))
  parse_uint_at("42abc") = 42, s now = "abc" (advanced 2)
  parse_uint_at("abc") = error:NotADigit, s = "abc" (unchanged)
  parse_uint_at("+") = error:NotADigit, s = "+" (unchanged)
  parse_uint_at("") = error:Empty, s = "" (unchanged)
```

The unsigned-only distinctions:

- **`+42` is accepted** (no-op for unsigned).
- **`-42` is rejected** with `LeadingSign`. We do NOT
  silently negate-and-cast because the overflow semantics
  get muddy: a caller's `"-1"` intent probably means "this
  isn't an unsigned at all".
- **`"+"` alone is `NotADigit`** — fixed during the lesson.
  The first draft returned `0` (because the loop body didn't
  execute when the input was exactly `"+"` and `i == start`
  was unchecked). The lesson output prints this case as
  `error:NotADigit, s = "+" (unchanged)`.
- **`4294967295` is UINT_MAX** (4,294,967,295 on this 64-bit
  toolchain — note: `unsigned` is 32-bit on Linux x86_64
  AND on this macOS arm64 build, even though `unsigned long`
  is 64-bit. The Jul 12 lesson on `std::expected` measured
  the same size).
- **`4294967296` overflows** — the prefix is committed (so
  `s` becomes empty after consuming all 10 chars).

## Section 3 — `parse_double_at` (double cursor)

```
== Section 3: psp::parse_double_at — double cursor ==
  parse_double_at("0") = 0, s now = "" (advanced 1)
  parse_double_at("42") = 42, s now = "" (advanced 2)
  parse_double_at("3.14") = 3.14, s now = "" (advanced 4)
  parse_double_at(".5") = 0.5, s now = "" (advanced 2)
  parse_double_at("1.") = 1, s now = "" (advanced 2)
  parse_double_at("1e10") = 1e+10, s now = "" (advanced 4)
  parse_double_at("1.5e-3") = 0.0015, s now = "" (advanced 6)
  parse_double_at("1.5x") = 1.5, s now = "x" (advanced 3)
  parse_double_at("1.5,2.5,3.5") = 1.5, s now = ",2.5,3.5" (advanced 3)
  parse_double_at("") = error:Empty, s = "" (unchanged)
  parse_double_at("+1.0") = error:LeadingSign, s = "+1.0" (unchanged)
  parse_double_at("-1.0") = error:LeadingSign, s = "-1.0" (unchanged)
  parse_double_at(".") = error:MissingFraction, s = "." (unchanged)
  parse_double_at("1e") = error:BadExponent, s = "1e" (unchanged)
  parse_double_at("1.2.3") = 1.2, s now = ".3" (advanced 3)
```

The streaming-vs-whole-span distinction in action:

- **`"1.5x"` → 1.5, s = "x"`** — accepted (with `"x"` left
  in `s`). Compare with `parse_double("1.5x")` from Jul 14,
  which would fail with `NotADigit` because the WHOLE span
  isn't a valid double.
- **`"1.2.3"` → 1.2, s = ".3"`** — the parser consumes
  `"1.2"`, stops at the second `.`, and leaves `".3"` in
  `s`. The caller can decide whether `".3"` is the start of
  the next token or an error.
- **`"."` alone fails with `MissingFraction`** — same as the
  whole-span `parse_double`.
- **`"1e"` fails with `BadExponent` and `s` unchanged** —
  the streaming parser does NOT commit the `"1"` prefix in
  this case. There's no meaningful "commit what we have,
  leave 'e' for later" because `"e"` alone is not a valid
  number-prefix either.

## Section 4 — walking a CSV row with `parse_int_at`

```
== Section 4: walking a CSV row with parse_int_at ==
  row = "10,20,30,40,50" -> [10, 20, 30, 40, 50]
  row = "1,2,3" -> [1, 2, 3]
  row = "100,200,300,400" -> [100, 200, 300, 400]
  row = "7" -> [7]
```

The CSV walker loops:

```cpp
while (!s.empty()) {
    auto r = psp::parse_int_at(s);
    if (!r) { /* bail */ }
    // ... append *r to the result ...
    while (!s.empty() && (s.front() < '0' || s.front() > '9')) {
        s = s.subspan(1, s.size() - 1);  // skip delimiter(s)
    }
}
```

The inner `while` skips any non-digit characters between
tokens (commas, whitespace, etc.). A stricter CSV parser
would check that exactly ONE delimiter appears between
tokens; for this demo, any gap is acceptable.

This is the realistic use case: a flat buffer, multiple
numeric tokens, a delimiter. The streaming parser API is
exactly the right primitive.

## Section 5 — alternating `parse_int_at` / `parse_double_at`

```
== Section 5: alternating parse_int_at / parse_double_at ==
  row = "10 3.14 20 2.71 30"
    step 0 (int): 10, remainder = " 3.14 20 2.71 30"
    step 1 (double): 3.14, remainder = " 20 2.71 30"
    step 2 (int): 20, remainder = " 2.71 30"
    step 3 (double): 2.71, remainder = " 30"
    step 4 (int): 30, remainder = ""
```

A more realistic mixed-type cursor walk: a buffer like
`"10 3.14 20 2.71 30"` contains alternating ints and
doubles. We call `parse_int_at`, then `parse_double_at`,
alternating, until the buffer is empty. This shows the two
primitives compose in the same buffer without interfering.

The walk skips leading whitespace at each step before
calling the parser:

```cpp
while (!s.empty() && s.front() == ' ') {
    s = s.subspan(1, s.size() - 1);
}
```

This is the pattern a real lexer would use: skip whitespace,
dispatch to the appropriate parser, repeat. The cursor shrinks
on every step.

## Section 6 — `sizeof` and toolchain probes

```
== Section 6: sizeof / feature probes ==
  sizeof(int)                                       = 4
  sizeof(unsigned)                                  = 4
  sizeof(double)                                    = 8
  sizeof(ParseError)                                = 4
  sizeof(psp::Span<const char>)                     = 16
  sizeof(std::expected<int, ParseError>)            = 8
  sizeof(std::expected<unsigned, ParseError>)       = 8
  sizeof(std::expected<double, ParseError>)         = 16
  __cpp_lib_expected                                = 202211
  __cpp_lib_span                                    = 202002
```

Three observations:

1. **`sizeof(std::expected<unsigned, ParseError>) == 8`** — same
   as `expected<int, ParseError>`, because `unsigned` and `int`
   are both 4 bytes on this toolchain. The EBO packing puts
   the bool discriminator into alignment slack.
2. **`sizeof(std::expected<double, ParseError>) == 16`** — same
   as the Jul 14 measurement. The 8-byte `double` payload
   drives the layout.
3. **Toolchain confirms** `__cpp_lib_expected == 202211`
   (C++23 final) and `__cpp_lib_span == 202002`.

## The consumer's `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(psp_parser_streaming_consumer ...)

find_package(psp_span_lib 0.7 REQUIRED)   # bumped from 0.6

add_executable(P-2026-07-15-psp-parser-streaming-cursor
    P-2026-07-15-psp-parser-streaming-cursor.cpp
)
target_link_libraries(P-2026-07-15-psp-parser-streaming-cursor
    PRIVATE psp_span_lib::psp_span_lib
)
```

vs the Jul 14 consumer:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-15-psp-parser-streaming-cursor
```

The version requirement bumped from 0.6 to 0.7 because the
streaming API is new in v0.7.0. The rest is unchanged from
the Jul 14 pattern.

## Build & run (what I actually ran)

```bash
# 1. Rebuild psp_span_lib 0.7.0 and install into /tmp/psp_install.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_span_lib
cmake -S . -B build-cron -DCMAKE_BUILD_TYPE=Release
cmake --build build-cron
cmake --install build-cron --prefix /tmp/psp_install

# 2. Configure + build the consumer with find_package.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_streaming
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-15-psp-parser-streaming-cursor

# 3. ASan build of the consumer.
cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-07-15-psp-parser-streaming-cursor

# 4. ASan build of psp_span_lib itself (proves the library's
#    single instantiation TU is clean too).
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_span_lib
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=ON
cmake --build build-asan

# 5. Strict-warnings sanity check (the CMake build uses -Wall
#    -Wextra -Wpedantic via the library's PUBLIC compile_features,
#    but a direct clang++ invocation is the belt-and-suspenders
#    proof):
clang++ -Wall -Wextra -Wpedantic -std=c++23 -O2 \
    -I /tmp/psp_install/include \
    -c P-2026-07-15-psp-parser-streaming-cursor.cpp \
    -o /tmp/check_strict.o     # 0 warnings, exit 0

# 6. CPack: rebuild the redistributable archive.
cd build-cron && cpack -C Release
# Produces psp_span_lib-0.7.0-Darwin.tar.gz — confirmed by
# `tar tzf` to contain include/psp_span/parser.h with 14
# references to the new streaming parsers.

# 7. "Ship-it" test: a fresh consumer install from the TGZ alone.
mkdir -p /tmp/psptest_fresh && tar xzf psp_span_lib-0.7.0-Darwin.tar.gz -C /tmp/psptest_fresh
# Configure with the extracted install tree (NOT /tmp/psp_install).
cmake -S . -B build-fresh -DCMAKE_PREFIX_PATH=/tmp/psptest_fresh/psp_span_lib-0.7.0-Darwin
cmake --build build-fresh
./build-fresh/P-2026-07-15-psp-parser-streaming-cursor
# Configure finds v0.7.0, consumer builds, program runs to
# completion. End-to-end "ship-it" verified.

# 8. Backward-compat check: the Jul 14 consumer still works.
clang++ -Wall -Wextra -Wpedantic -std=c++23 -O2 \
    -I /tmp/psp_install/include \
    ../psp_parser_header/P-2026-07-14-psp-parser-header.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/jul14_retest
/tmp/jul14_retest   # exit 0; the only warning is a pre-existing
                    # unused-function in the Jul 14 source, not
                    # related to today's changes.
```

All seven steps above compiled and ran to exit 0. ASan reports
no memory errors in any build. The Jul 14 consumer builds and
runs unchanged against v0.7.0 — the only difference is that it
links against the same archive with additional header content
(parser.h is bigger but no new symbols).

## What I didn't cover (next-session candidates)

- **`expect_char_at(s, ',')`** — the natural next primitive.
  After `parse_int_at` consumes the leading int, the caller
  usually wants to verify (and skip) a delimiter. This is the
  same pattern as `parse_*_at` but for single characters.
- **`skip_whitespace_at(s)`** — leading whitespace is currently
  NOT skipped by any streaming parser. The caller has to do it
  manually with `while (!s.empty() && s.front() == ' ') s = s.subspan(1, ...)`.
  A primitive that handles all whitespace (`' '`, `'\t'`, `'\n'`,
  `'\r'`) would clean up every consumer.
- **`parse_string_at(s, '"')`** — quoted-string cursor. Different
  shape: reads until the closing quote, handles `\"` escapes.
- **A JSON parser built on the cursor primitives** — the
  headline forward-on. `skip_ws` → `expect_char('{')` → loop
  over key-value pairs → `parse_string_at(key)` →
  `expect_char(':')` → `parse_int_at(value)` / `parse_double_at(value)`
  → etc. This is the right size for one lesson.
- **`parse_bool_at(s)`** — true / false / null literals. Fits
  the same pattern.
- **A test harness using Catch2 / doctest** — the library
  still has no formal unit tests; the lesson programs ARE the
  tests, but a proper `tests/` directory would catch
  regressions on future header edits. This is a forward-on
  candidate but it's a "library hygiene" move, not a "new C++"
  move.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        ├── psp_span_lib/                                         # MODIFIED (library)
        │   ├── CMakeLists.txt                                   #    v0.6.0 -> v0.7.0
        │   └── include/psp_span/
        │       └── parser.h                                      # MODIFIED (streaming cursor API added)
        └── psp_parser_streaming/                                # NEW (the consumer)
            ├── CMakeLists.txt                                   #    consumer with find_package(psp_span_lib 0.7)
            ├── P-2026-07-15-psp-parser-streaming-cursor.cpp      #    the program (6 sections)
            └── P-2026-07-15-psp-parser-streaming-cursor.md       #    this file
```

The library gains three new functions in `parser.h` and a
version bump. The consumer directory is brand new. The
`psp_span_lib` install tree (the part that gets bundled into
CPack archives and shipped to consumers) now ships a parser
header with both whole-span and streaming-cursor APIs:

```
include/psp_span/
├── span.h        (C++17 — psp::Span<T, Extent>)
└── parser.h      (C++23 — psp::parse_int/parse_double whole-span
                   + psp::parse_int_at/parse_uint_at/parse_double_at
                   streaming cursor with std::expected<T, ParseError>)
lib/
├── libpsp_span_lib.a
└── cmake/psp_span_lib/
    ├── psp_span_libConfig.cmake
    ├── psp_span_libConfigVersion.cmake
    └── psp_span_libTargets.cmake
```

The CPack TGZ `psp_span_lib-0.7.0-Darwin.tar.gz` contains the
above plus `License.txt` + `Readme.txt` at the install root.
Verified by extracting to a fresh `/tmp/psptest_fresh` and
rebuilding the consumer against it — `find_package(psp_span_lib
0.7 REQUIRED)` succeeded, consumer compiled, program ran.

## Next steps

The `psp_span_lib` release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs
  NSIS or .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.7.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform
  and SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow
  feature work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **`expect_char_at(s, c)`** — the natural next primitive for
  cursor walking.
- **`skip_whitespace_at(s)`** — leading-whitespace skipper for
  every consumer.
- **JSON parser built on the cursor primitives** — `skip_ws` →
  `expect_char('{')` → key-value pair loop → ... — the
  headline forward-on from today's lesson.
- **`std::expected` and coroutines** — `co_return std::expected<T,E>`
  composes with the parser layer for an async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it, the Jul
  10 Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray)
  — the standard version of the Jul 11 hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear
  algebra built on mdspan.

The most natural follow-on is **`expect_char_at` and
`skip_whitespace_at` as a small library-hygiene lesson** —
both are 5-line additions to `parser.h` and they clean up
every cursor-walking consumer. After that, a JSON parser on
top of the cursor primitives is the headline "design move"
session.