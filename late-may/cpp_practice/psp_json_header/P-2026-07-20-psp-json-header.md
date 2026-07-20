# P-2026-07-20 — `<psp_span/json.h>` lands in psp_span_lib (v0.10.0): the Jul 19 consumer's parser is now a library capability

## Headline

The Jul 19 lesson (`P-2026-07-19-psp-json-arrays-nested.cpp`) shipped a
complete general JSON parser as a **consumer-side** capability: the
parser, `JsonValue`, `json_to_string`, and the recursive-descent
plumbing all lived in that consumer's `.cpp` file.

The Jul 19 lesson's "Next steps" said:

> The natural forward-on is **`<psp_span/json.h>`** — lift today's
> consumer into the library proper, bump to v0.10.0, and the next
> session's consumer can `find_package(psp_span_lib 0.10 REQUIRED)`
> and `#include <psp_span/json.h>`.

That is exactly today's lesson, in one new consumer + one new
library header:

- `<psp_span/json.h>` is a **new public header** of `psp_span_lib`
  (the third header, after `span.h` and `parser.h`).
- A new **typed `ParseError::DuplicateKey` enumerator** lands in
  `<psp_span/parser.h>` alongside the new header (the Jul 18 / Jul 19
  consumers over-used `UnexpectedChar` for the duplicate-key case;
  that's gone now).
- The library version bumps **0.9.0 → 0.10.0**.
- A new consumer directory `psp_json_header/` exercises the
  header-owned parser. That consumer has **zero parser code of
  its own** — every line is either a test driver, an oracle
  assertion, or a sizeof probe. Compared to the Jul 19 consumer
  (~250 lines of parser + test), today's consumer is ~150 lines
  and ALL of it is test infrastructure.

The headline payoff: any consumer that does
`find_package(psp_span_lib 0.10 REQUIRED)` and
`#include <psp_span/json.h>` gets a complete strict-JSON parser
(scalars + arrays + nested objects + escapes + `\uXXXX`) with
typed errors, for free, with no extra dependency.

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
              skip_whitespace_at)
Jul 18  JSON scalar tokens (parse_string_at,      parser layer (library-side, cursor) — JSON scalars
              parse_bool_at, parse_null_at)
Jul 19  arrays + nested objects on top of the     parser layer (consumer-side) — recursive descent
              v0.9.0 cursor primitives
Jul 20  <psp_span/json.h> ships in psp_span_lib   parser layer (library-side) — full JSON parser
              (v0.10.0) + typed DuplicateKey     ← today
```

Today's lesson **closes the Jul 19 next-steps arc**. The library now
owns the complete general JSON parser; consumers `#include` it.

## What changed vs. Jul 19

| Layer                                  | Jul 19                      | Jul 20                                   |
|----------------------------------------|-----------------------------|------------------------------------------|
| `<psp_span/parser.h>`                  | v0.9.0                      | v0.10.0 (+ `DuplicateKey` enumerator)   |
| `<psp_span/json.h>`                    | (none)                      | v0.10.0 (NEW)                            |
| `JsonValue` location                   | consumer `.cpp`             | `<psp_span/json.h>`                      |
| `parse_value_at` / `parse_array_at` /  | consumer `.cpp`             | `<psp_span/json.h>`                      |
|      `parse_object_at`                 |                             |                                          |
| `json_to_string`                       | consumer `.cpp`             | `<psp_span/json.h>` (incl. `detail::escape_for_json_string`) |
| Duplicate-key error type               | `ParseError::UnexpectedChar` (overload) | `ParseError::DuplicateKey` (typed, NEW) |
| Test sections in this consumer         | 4                           | 4                                        |
| Corpus of tests                        | (same JSON docs)            | identical                                |
| Lines of parser code in consumer       | ~250                        | 0                                        |

The library gains **one new file** (`include/psp_span/json.h`,
~640 lines) and **two-line delta to `parser.h`** (one new enumerator +
one new formatter case). The header itself is otherwise unchanged.

## The new header at a glance

`<psp_span/json.h>` adds five things to the public API:

```cpp
namespace psp {
    struct JsonValue {
        std::variant<
            std::monostate,                  // 0: unset
            std::nullptr_t,                  // 1: JSON null
            bool,                            // 2: JSON true / false
            std::int64_t,                    // 3: JSON integer (fits in int64)
            double,                          // 4: JSON number with frac / exp
            std::string,                     // 5: JSON string
            std::vector<JsonValue>,          // 6: JSON array
            std::map<std::string, JsonValue> // 7: JSON object
        > value;
    };

    inline std::expected<JsonValue, ParseError>
    parse_value_at(psp::Span<const char>&) noexcept;

    inline std::expected<std::vector<JsonValue>, ParseError>
    parse_array_at(psp::Span<const char>&) noexcept;

    inline std::expected<std::map<std::string, JsonValue>, ParseError>
    parse_object_at(psp::Span<const char>&) noexcept;

    inline std::string
    json_to_string(const JsonValue&, int indent = 0);
}
```

All five are `inline` in the header (no new `.cpp` is needed — the
header is self-contained). The three parsers commit the cursor
`Span<const char>&` only on success, leaving it unchanged on any
parse error. That's the same contract the v0.9.0 cursor primitives
shipped with — and it's why the new header needs **zero new
cursor primitives**: everything it does is composition of the
existing `skip_whitespace_at`, `expect_char_at`, `parse_string_at`,
`parse_bool_at`, `parse_null_at`, and `parse_double_at`.

## What the parser does (recap)

Recognised shape (RFC 8259 strict, JSON5/JSONC extensions are
intentionally out of scope):

```
value    := object | array | string | number | true | false | null
object   := '{' (string ':' value (',' string ':' value)*)? '}'
array    := '[' (value (',' value)*)? ']'
string   := '"' ...escapes... '"'   (full JSON escapes, including \uXXXX + surrogate pairs)
number   := [-] int [frac] [exp]
```

Three mutually-recursive functions (`parse_value_at` dispatches to
`parse_object_at` and `parse_array_at`, both of which call back into
`parse_value_at` for their elements/values). Everything else is
delegation to the v0.9.0 cursor primitives.

The full recursive-descent structure was already documented in the
Jul 19 lesson (`P-2026-07-19-psp-json-arrays-nested.md`); today's
lesson is the promotion, not a redesign. The code that lands in
the header today is identical to the consumer code from Jul 19,
modulo:

- Live in `psp::` (header namespace), not in the consumer.
- `parse_object_at` returns `ParseError::DuplicateKey` for
  duplicate keys, **not** the Jul 19 overload of
  `UnexpectedChar`.
- `detail::escape_for_json_string` moved into `psp::detail` so
  the public header has a small private helper for the pretty-
  printer's escaping.

## The new typed `DuplicateKey`

The Jul 18 and Jul 19 consumers both rejected duplicate keys, but
reported the failure as `ParseError::UnexpectedChar`:

```cpp
auto [it, inserted] = out.emplace(std::move(*key), std::move(*val));
if (!inserted) {
    s = saved;
    return std::unexpected{ParseError::UnexpectedChar};  // <-- Jul 19
}
```

That worked, but was lossy — a duplicate key and a stray `[`
look identical to the caller. With v0.10.0 we add a real
`ParseError::DuplicateKey` enumerator:

```cpp
enum class ParseError {
    Empty,
    LeadingSign,
    NotADigit,
    Overflow,
    BadExponent,
    MissingFraction,
    UnexpectedChar,
    UnterminatedString,
    InvalidEscape,
    InvalidUnicodeEscape,
    InvalidLiteral,
    DuplicateKey,    // <-- NEW in v0.10.0
};
```

and the formatter gets one extra case:

```cpp
case ParseError::DuplicateKey: name = "DuplicateKey"; break;
```

The Jul 20 consumer's Section 1 final case asserts that this
typed error actually surfaces: parsing
`{"a": 1, "a": 2}` returns `ParseError::DuplicateKey` (and leaves
`s` unchanged), not the old overload. That assertion is the test
that proves the header really carries the new fidelity, not just
the old one with a rename.

## Verified output

```
== Section 1: header-driven parse_value_at + DuplicateKey arrives ==
  parse_value_at("[]") = [], s remaining = ""
  parse_value_at("  [  ]  ") = [], s remaining = ""
  parse_value_at("[1, 2, 3]") = [1, 2, 3], s remaining = ""
  parse_value_at("[1.5, 2.5, 3.5]") = [1.5, 2.5, 3.5], s remaining = ""
  parse_value_at("[\"a\", \"b\", \"c\"]") = ["a", "b", "c"], s remaining = ""
  parse_value_at("[true, false, true]") = [true, false, true], s remaining = ""
  parse_value_at("[null, null, null]") = [null, null, null], s remaining = ""
  parse_value_at("[1, \"two\", 3.0, true, null]") = [1, "two", 3, true, null], s remaining = ""
  parse_value_at("") = error:Empty, s = "" (unchanged)
  parse_value_at("[") = error:Empty, s = "[" (unchanged)
  parse_value_at("[1, 2, ]") = error:UnexpectedChar, s = "[1, 2, ]" (unchanged)
  parse_value_at("[1, 2") = error:Empty, s = "[1, 2" (unchanged)
  parse_value_at("[tru]") = error:InvalidLiteral, s = "[tru]" (unchanged)
  parse_value_at("[\"unterminated]") = error:UnterminatedString, s = "[\"unterminated]" (unchanged)
  parse_value_at("[1, 2, , 3]") = error:UnexpectedChar, s = "[1, 2, , 3]" (unchanged)
  [15 / 15 passed]
  duplicate-key input gave the typed error: DuplicateKey (s unchanged)
  [16 / 16 passed incl. dup-key]

== Section 2: nested objects via header parser ==
  parse_value_at("{\"a\": {\"x\": 1}}") = object with 1 top-level pairs
  parse_value_at("{\"a\": {\"b\": {\"c\": 42}}}") = object with 1 top-level pairs
  parse_value_at("{\"name\": \"Petra\", \"tags\": [\"cpp\", \"json\"], \"meta\": {\"age\": 7, \"active\": true}}") = object with 3 top-level pairs
  parse_value_at("{\"a\": {\"b\": {\"c\": {\"d\": {\"e\": \"deep\"}}}}}") = object with 1 top-level pairs
  parse_value_at("{\"users\": [{\"id\": 1}, {\"id\": 2}, {\"id\": 3}]}") = object with 1 top-level pairs
  parse_value_at("[{\"x\": 1}, {\"x\": 2}, {\"x\": 3}]") = array of 3 elements
  parse_value_at("{\"items\": [], \"meta\": {}}") = object with 2 top-level pairs
  parse_value_at("{\"name\": \"Petra\", \"age\": 7, \"scores\": [98, 87, 92], \"address\": {\"city\": \"Earth\", \"zip\": \"00000\"}, \"active\": true}") = object with 5 top-level pairs
  parse_value_at("{\"a\": {\"x\": 1}") = error:Empty, s = "{\"a\": {\"x\": 1}" (unchanged)
  parse_value_at("{\"a\": [1, 2, }") = error:UnexpectedChar, s = "{\"a\": [1, 2, }" (unchanged)
  parse_value_at("{\"a\": {\"x\": }") = error:UnexpectedChar, s = "{\"a\": {\"x\": }" (unchanged)
  parse_value_at("{\"a\": [1, 2}") = error:UnexpectedChar, s = "{\"a\": [1, 2}" (unchanged)
  parse_value_at("{\"a\": 1, \"a\": 2}") = error:DuplicateKey, s = "{\"a\": 1, \"a\": 2}" (unchanged)
  [13 / 13 passed]

== Section 3: full-document round-trip (parse -> print -> parse) ==
  OK: "empty object" (input 2 bytes -> 2 bytes pretty)
  OK: "empty array" (input 2 bytes -> 2 bytes pretty)
  OK: "bare string" (input 7 bytes -> 7 bytes pretty)
  OK: "bare number" (input 2 bytes -> 2 bytes pretty)
  OK: "bare bool" (input 4 bytes -> 4 bytes pretty)
  OK: "bare null" (input 4 bytes -> 4 bytes pretty)
  OK: "flat object" (input 43 bytes -> 51 bytes pretty)
  OK: "flat array" (input 34 bytes -> 48 bytes pretty)
  OK: "nested" (input 51 bytes -> 91 bytes pretty)
  OK: "list of users" (input 63 bytes -> 115 bytes pretty)
  OK: "escapes" (input 50 bytes -> 54 bytes pretty)
  OK: "unicode" (input 36 bytes -> 25 bytes pretty)
  [12 / 12 passed]

== Section 4: sizeof / feature probes (header-owned JsonValue) ==
  sizeof(bool)                                  = 1
  sizeof(std::int64_t)                          = 8
  sizeof(double)                                = 8
  sizeof(std::string)                           = 24
  sizeof(std::vector<psp::JsonValue>)          = 24
  sizeof(std::map<std::string, psp::JsonValue>)= 24
  sizeof(psp::JsonValue)                       = 32
  sizeof(std::expected<psp::JsonValue, ParseError>)= 40
  sizeof(psp::Span<const char>)                 = 16
  sizeof(ParseError)                            = 4
  static_assert(JsonValue >= 32 bytes)          = OK (actual: 32)
  Public-header roster (v0.10.0):
    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)
    <psp_span/parser.h>   : psp::parse_int/parse_double + whole-span + cursor + JSON-scalar token cursors + ParseError (+ DuplicateKey in v0.10.0)
    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string  (NEW in v0.10.0)
  __cpp_lib_expected                            = 202211
  __cpp_lib_variant                             = 202106

[psp_json_header_consumer: all 4 sections complete]
```

Section 1: **16 / 16** test cases (15 base cases + 1 explicit
duplicate-key assertion that the typed error arrives).
Section 2: **13 / 13** (nested objects / arrays / duplicate-key
detection at end-to-end level). Section 3: **12 / 12** round-trip.
Section 4: sizeof probes confirm `JsonValue = 32 bytes` on libc++
(same as Jul 19's measurement, since the type is identical).

ASan + UBSan run: **clean** (no warnings, no findings; the macOS
ASan build does not support leak detection, but core ASan + UBSan
verified).

## ASan bug found during development (and fixed)

Initial Section 1 duplicate-key check wrote:

```cpp
psp::Span<const char> s = as_span(std::string("{\"a\": 1, \"a\": 2}"));
auto r = psp::parse_value_at(s);
```

This **passed** under Release builds but **failed** under ASan
with `stack-use-after-scope` on the second character of the
input, because the temporary `std::string` returned by the inner
`std::string(...)` call was destroyed at the end of the full-
expression, leaving `s` pointing at freed-on-stack storage.

This is exactly the kind of bug ASan catches that the Release
build (and unit tests) wouldn't. The fix was to bind the input
to a local first:

```cpp
const std::string dup_input = R"({"a": 1, "a": 2})";
psp::Span<const char> s = as_span(dup_input);
auto r = psp::parse_value_at(s);
```

After the fix, both Release and ASan builds are clean and the
dup-key test still surfaces the typed `DuplicateKey` error. The
fix is documented in the consumer's Section 1 with a comment that
explains why the binding is required.

This is a **real bug** caught by ASan — not an ASan false-positive.
It is the same class of bug as the Jul 19 consumer's
`span_to_string` helper accidentally working only because callers
happened to hold the `std::string` alive; the dup-key check was
the first time the consumer built a span over a temporary.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v010
cmake --build late-may/cpp_practice/psp_span_lib/build-v010
cmake --build late-may/cpp_practice/psp_span_lib/build-v010 --target install
```

Build the consumer (assumes v0.10.0 installed at `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_header -B late-may/cpp_practice/psp_json_header/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_header/build
./late-may/cpp_practice/psp_json_header/build/P-2026-07-20-psp-json-header
```

Strict-warning build (the actual build used during development, with
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`):

```sh
cmake -S late-may/cpp_practice/psp_json_header -B late-may/cpp_practice/psp_json_header/build \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_header/build
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_header -B late-may/cpp_practice/psp_json_header/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_header/build-asan
./late-may/cpp_practice/psp_json_header/build-asan/P-2026-07-20-psp-json-header
```

## What's NOT in this lesson

- It is **not** a JSON5 / JSONC parser. No comments, no single-
  quoted strings, no trailing commas.
- It does not parse numbers with explicit `+` sign — the
  underlying `parse_double_at` rejects both `-` and `+`. (Adding
  a leading-sign skip in `parse_value_at` would close that gap
  in three lines; left as a future lesson.)
- It does not allocate from an arena — every `std::vector`
  element and `std::map` member owns its own storage.
- It does not pretty-print with configurable indent. Two-space
  indent is hard-coded into `json_to_string`.
- It does not ship a serializer (the reciprocal operation). The
  JSON value tree can be inspected, modified, and re-printed via
  `json_to_string`; a typed serializer (taking a `JsonValue&`
  and rewriting it in canonical form, or extracting a typed field
  per RFC 8259 schema) would be a separate lesson.
- It does not add new `ParseError` cases beyond `DuplicateKey`.
  The other error modes are unchanged from Jul 18 / Jul 19.

## Where we go next

The Jul 19 next-steps list had two open threads: `<psp_span/json.h>`
and the typed `DuplicateKey`. **Both closed today.** The library
now has a complete general JSON parser as a first-class public
header.

The library as a whole still has these open threads from earlier
lessons (re-quoted for context):

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS
  or `.zip` CPack generator and a different `with.files` glob.
- **Status badge in README**.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform
  and SHA-pinned.
- **Branch protection requiring the matrix to pass**.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **`std::expected` and coroutines** — `co_return std::expected<T,E>`
  composes with the parser layer for an async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it, the Jul 10
  Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray) — the
  standard version of the Jul 11 hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear algebra
  built on mdspan.
- **A `std::expected<JsonValue, ParseError>` → `std::generator`
  adapter** — once `<generator>` lands (C++23 final?), the parser
  can yield values lazily for streaming.

JSON-parser-specific forward-ons still open:

- **Configurable pretty-print indent** — `json_to_string(const
  JsonValue&, int indent, int width)` would let callers pick
  4-space or tab indent.
- **Streaming / pull parser** — turn the parser into an iterator
  that yields one value at a time from a buffered input, using
  `std::generator<JsonValue>` once it lands. Today's parsers all
  return the complete parsed tree; a pull variant would suit very
  large inputs where materialising the whole tree in memory is
  impractical.
- **JSON Pointer (RFC 6901)** — a `find(ptr, json)` /
  `resolve(ptr, json)` function pair over the parsed tree.
- **JSON Patch (RFC 6902)** — apply patch ops on the parsed tree.
- **JSON Schema validation** — a library-side schema validator
  built on `<psp_span/json.h>`.
- **Number-straddling (BigInt / BigDecimal)** — current parsers
  fold every integer into `std::int64_t` and every non-integer
  number into `double`. A real-money JSON library would emit a
  tagged alternative (decimal128 or arbitrary-precision string).

The most natural JSON-parser forward-on is **a JSON serializer
configurable in line with JSON5/JSONC extensions** (a follow-on
to today's strict parser): the parser is now a public capability,
so a serializer of comparable fidelity belongs next to it.

For the library as a whole, the most natural forward-on is still
**bump the library to v0.11.0 with one more concrete new public
capability** — a candidate is `<psp_span/json_ext.h>` adding
JSON Pointer / JSON Patch (RFC 6901 / 6902) on top of today's
JSON header.
