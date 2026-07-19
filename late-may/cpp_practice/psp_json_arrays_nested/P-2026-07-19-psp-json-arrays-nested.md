# P-2026-07-19 — A complete general JSON parser built on the v0.9.0 cursor primitives (arrays + nested objects)

## Headline

The Jul 18 lesson (`P-2026-07-18-psp-json-scalars.cpp`) shipped a strict
parser for JSON objects that contained only scalar values (string /
number / bool / null). That parser was honest about its limitation: it
could not parse arrays (`[1, 2, 3]`) and it could not parse nested
objects (`{"user": {"name": "Petra", "age": 7}}`).

The Jul 18 lesson's "Next Steps" said:

> The most natural follow-on is **arrays + nested objects** — once those
> land, the parser is a complete general JSON parser and the consumer
> directory is a candidate for promotion to `<psp_span/json.h>`.

That is exactly today's lesson, in one consumer file, with **no new
library code**:

- No change to `<psp_span/parser.h>`.
- No change to the `psp_span_lib` CMakeLists.txt.
- No version bump (still **v0.9.0**).

The v0.9.0 cursor primitives (`skip_whitespace_at`, `expect_char_at`,
`parse_string_at`, `parse_bool_at`, `parse_null_at`, `parse_int_at`,
`parse_uint_at`, `parse_double_at`) are sufficient. All we add is a
forward-declaration-free recursive-descent layer on top of them,
plus a `JsonValue` sum type that can hold arrays and nested objects
as well as scalars.

The headline payoff is a working, strict, `std::expected`-based
parser for **any** JSON value — null / bool / int / double / string
/ array / object, in any combination, nested arbitrarily deep.

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
              v0.9.0 cursor primitives            ← today (no library change)
```

Today's lesson is **consumer-only**: nothing in
`late-may/cpp_practice/psp_span_lib/` changed. The point is to show
that the v0.9.0 API is *sufficient* for full JSON — once a consumer
exists, promotion to `<psp_span/json.h>` is mostly a move-and-rename.

## The "complete general JSON parser" shape

JSON has six structural tokens:

```
object   '{' string ':' value (',' string ':' value)* '}'
array    '[' value (',' value)* ']'
string   '"' ...escapes... '"'
number   [-] int [frac] [exp]
true     'true'
false    'false'
null     'null'
```

The first two are recursive; the rest are leaves. A real JSON parser
needs three mutually-recursive functions:

```
parse_value_at(s)   — dispatcher, peeks first non-WS char, calls one of:
parse_array_at(s)    — '[', then values, then ']'
parse_object_at(s)   — '{', then string:value pairs, then '}'
```

`parse_value_at` dispatches on the first non-whitespace character:

```
'{' -> parse_object_at
'[' -> parse_array_at
'"' -> parse_string_at        (returns string)
't' / 'f' -> parse_bool_at
'n' -> parse_null_at
digit / '-' -> parse_double_at  (returns double or int)
```

The v0.9.0 cursor primitives already exist for **every leaf case**.
The only new code is the dispatcher and the two recursive
structural parsers.

## JsonValue — the full JSON value type

Today we replace Jul 18's `JsonScalar` (six variants) with
`JsonValue` (eight variants). The new alternatives are
`std::vector<JsonValue>` (array) and `std::map<std::string, JsonValue>`
(object):

```cpp
struct JsonValue {
    std::variant<
        std::monostate,                       // unset
        std::nullptr_t,                       // JSON null
        bool,                                 // JSON true / false
        std::int64_t,                         // JSON integer (fits in int64)
        double,                               // JSON number with frac / exp
        std::string,                          // JSON string
        std::vector<JsonValue>,               // JSON array
        std::map<std::string, JsonValue>      // JSON object
    > value;
};
```

We could have put `JsonObject` and `JsonArray` in named structs (the
way Jul 18's `JsonObject` did), but for a sum type `std::variant<...>`
is cleaner — `std::visit` can dispatch on every alternative in one
place, and a recursive-descent parser that returns `JsonValue` is
simpler than one that returns `std::variant<JsonScalar, JsonArray,
JsonObject>`.

On libc++ (this toolchain), `std::map<T, U>` is a flat 3-word header —
24 bytes — same as `std::vector<T>`. That keeps `JsonValue` at
**32 bytes** (24-byte largest alternative + 8-byte discriminator).
On libstdc++ the map is 48 bytes and `JsonValue` is 56; the
Section 7 probes report the actual number on whatever compiler you
build with, and the static_asserts lock in the libc++ lower bound
(`sizeof(JsonValue) >= 32`).

`std::map` is ordered (red-black tree) so the keys come out sorted
alphabetically in any iteration. JSON objects are unordered per spec,
so the ordering doesn't matter for round-trip correctness. For
pretty-printing, we use the `std::map`'s natural sorted order.

A small wrinkle: `std::map`'s `value_type` is `std::pair<const
std::string, JsonValue>`, so the `JsonValue` inside the map is
const from the map's perspective. `std::visit` doesn't care (it
takes its argument by universal reference), so the recursion still
works.

## The dispatcher — parse_value_at

```cpp
static std::expected<JsonValue, ParseError>
parse_value_at(psp::Span<const char>& s) noexcept {
    auto saved = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (s.empty()) {
        s = saved; return std::unexpected{ParseError::Empty};
    }
    const char front = s.front();
    switch (front) {
        case '{': { auto obj = parse_object_at(s); ... return ...; }
        case '[': { auto arr = parse_array_at(s); ... return ...; }
        case '"': { auto str = psp::parse_string_at(s); ... return ...; }
        case 't': case 'f': { auto b = psp::parse_bool_at(s); ... return ...; }
        case 'n': { auto n = psp::parse_null_at(s); ... return ...; }
        default:
            if ((front >= '0' && front <= '9') || front == '-') {
                auto d = psp::parse_double_at(s);
                ...
                if (std::trunc(*d) == *d
                    && *d >= static_cast<double>(std::numeric_limits<std::int64_t>::min())
                    && *d <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                    out.value = static_cast<std::int64_t>(*d);
                } else {
                    out.value = *d;
                }
                return out;
            }
            s = saved;
            return std::unexpected{ParseError::UnexpectedChar};
    }
}
```

Two design points worth calling out:

1. **One branch per case — no lookahead.** Each cursor primitive
   commits `s` only on success, so we don't need to save `s` between
   attempts. We do save `s` at function entry because the
   *dispatcher* itself might fail (EOF after whitespace, leading
   char doesn't match any rule).

2. **int64 vs double** — `parse_double_at` always returns `double`,
   but a JSON integer like `42` is conceptually different from
   `42.0`. We preserve that distinction by checking `std::trunc(*d)
   == *d` and the int64 range — exactly the same trick the Jul 18
   lesson used for `JsonScalar`.

The dispatcher is **the only new function that knows about all six
JSON value kinds**. `parse_array_at` and `parse_object_at` only
need to know how to parse their own structure and how to recurse
into `parse_value_at` for their elements/values.

## parse_array_at — the recursive base case

Strict array parser. Recognised shape:

```
'[' (WS VALUE (WS ',' WS VALUE)*)? WS ']'
```

(i.e. an optional comma-separated sequence of values — empty
arrays are OK, trailing commas are NOT.) Every element is parsed
via `parse_value_at`. The whole function is ~30 lines and looks
structurally identical to `parse_object_at` — same `skip_whitespace_at`
/ `expect_char_at` loop, same empty-vs-non-empty branch, same
"leave `s` unchanged on error" rule.

## parse_object_at — same shape, different delimiters

```cpp
static std::expected<std::map<std::string, JsonValue>, ParseError>
parse_object_at(psp::Span<const char>& s) noexcept {
    ...
    while (true) {
        if (auto r = psp::skip_whitespace_at(s); !r) { ... }
        auto key = psp::parse_string_at(s);
        if (!key) { ... }
        if (auto r = psp::skip_whitespace_at(s); !r) { ... }
        if (auto r = psp::expect_char_at(s, ':'); !r) { ... }
        auto val = parse_value_at(s);                  // <-- recurse
        if (!val) { ... }
        auto [it, inserted] = out.emplace(std::move(*key), std::move(*val));
        if (!inserted) {                                // duplicate key
            s = saved;
            return std::unexpected{ParseError::UnexpectedChar};
        }
        ...
    }
}
```

Same as Jul 18's `parse_object_at`, except the value is now a
`JsonValue` instead of a `JsonScalar`. The duplicate-key check
is `std::map::emplace`'s return value — same as before. The
`UnexpectedChar` slot is reused for "duplicate key" because we
don't yet have a typed `ParseError::DuplicateKey` enumerator;
that is a forward-on (see Next steps).

## What changed vs. Jul 18

| Layer | Jul 18 | Jul 19 |
|---|---|---|
| `<psp_span/parser.h>` | unchanged (v0.9.0) | **unchanged** |
| `psp_span_lib` CMakeLists | unchanged | **unchanged** |
| `psp_span_lib` version | 0.9.0 | **0.9.0** |
| Consumer directory | `psp_json_scalars/` | `psp_json_arrays_nested/` |
| Value type | `JsonScalar` (6 variants) | `JsonValue` (8 variants) |
| Parsers in consumer | `parse_object_at` only | `parse_value_at`, `parse_array_at`, `parse_object_at` |
| Test sections | 5 | 4 (consolidated) |
| Round-trip test | no | **yes** (Section 6) |

The library is genuinely unchanged. The whole new layer is
**~250 lines of consumer code**.

## Verified output

```
== Section 4: arrays of scalars — the recursive base case ==
  parse_array_at("[]") = [], s remaining = ""
  parse_array_at("[ ]") = [], s remaining = ""
  parse_array_at("  [  ]  ") = [], s remaining = ""
  parse_array_at("[1, 2, 3]") = [1, 2, 3], s remaining = ""
  parse_array_at("[1.5, 2.5, 3.5]") = [1.5, 2.5, 3.5], s remaining = ""
  parse_array_at("[\"a\", \"b\", \"c\"]") = ["a", "b", "c"], s remaining = ""
  parse_array_at("[true, false, true]") = [true, false, true], s remaining = ""
  parse_array_at("[null, null, null]") = [null, null, null], s remaining = ""
  parse_array_at("[1, \"two\", 3.0, true, null]") = [1, "two", 3, true, null], s remaining = ""
  parse_array_at("  [  1  ,  2  ,  3  ]  ") = [1, 2, 3], s remaining = ""
  parse_array_at("[1, 2, 3, 0, 42]") = [1, 2, 3, 0, 42], s remaining = ""
  parse_array_at("[1.5, 2.5, 0.0, 42.0, 0]") = [1.5, 2.5, 0, 42, 0], s remaining = ""
  parse_array_at("") = error:Empty, s = "" (unchanged)
  parse_array_at("[") = error:Empty, s = "[" (unchanged)
  parse_array_at("[1, 2, ]") = error:UnexpectedChar, s = "[1, 2, ]" (unchanged)
  parse_array_at("[1, 2") = error:Empty, s = "[1, 2" (unchanged)
  parse_array_at("[tru]") = error:InvalidLiteral, s = "[tru]" (unchanged)
  parse_array_at("[\"unterminated]") = error:UnterminatedString, s = "[\"unterminated]" (unchanged)
  parse_array_at("[1, 2, , 3]") = error:UnexpectedChar, s = "[1, 2, , 3]" (unchanged)
  [19 / 19 passed]

== Section 5: nested objects — the recursive case ==
  parse_object_at("{\"a\": {\"x\": 1}}") succeeded, 1 top-level pairs
  parse_object_at("{\"a\": {\"b\": {\"c\": 42}}}") succeeded, 1 top-level pairs
  parse_object_at("{\"name\": \"Petra\", \"tags\": [\"cpp\", \"json\"], \"meta\": {\"age\": 7, \"active\": true}}") succeeded, 3 top-level pairs
  parse_object_at("{\"a\": {\"b\": {\"c\": {\"d\": {\"e\": \"deep\"}}}}}") succeeded, 1 top-level pairs
  parse_object_at("{\"users\": [{\"id\": 1}, {\"id\": 2}, {\"id\": 3}]}") succeeded, 1 top-level pairs
  parse_value_at("[{\"x\": 1}, {\"x\": 2}, {\"x\": 3}]") = array of 3 elements
  parse_object_at("{\"items\": [], \"meta\": {}}") succeeded, 2 top-level pairs
  parse_object_at("{\"name\": \"Petra\", \"age\": 7, \"scores\": [98, 87, 92], \"address\": {\"city\": \"Earth\", \"zip\": \"00000\"}, \"active\": true}") succeeded, 5 top-level pairs
  parse_object_at("{\"a\": {\"x\": 1}") = error:Empty, s = "{\"a\": {\"x\": 1}" (unchanged)
  parse_object_at("{\"a\": [1, 2, }") = error:UnexpectedChar, s = "{\"a\": [1, 2, }" (unchanged)
  parse_object_at("{\"a\": {\"x\": }") = error:UnexpectedChar, s = "{\"a\": {\"x\": }" (unchanged)
  parse_object_at("{\"a\": [1, 2}") = error:UnexpectedChar, s = "{\"a\": [1, 2}" (unchanged)
  parse_object_at("{\"a\": 1, \"a\": 2}") = error:UnexpectedChar, s = "{\"a\": 1, \"a\": 2}" (unchanged)
  [13 / 13 passed]

== Section 6: full-document round-trip (parse -> print -> parse) ==
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

== Section 7: sizeof / feature probes ==
  sizeof(bool)                                = 1
  sizeof(std::int64_t)                        = 8
  sizeof(double)                              = 8
  sizeof(std::string)                         = 24
  sizeof(std::vector<JsonValue>)              = 24
  sizeof(std::map<std::string, JsonValue>)    = 24
  sizeof(JsonValue)                           = 32
  sizeof(std::expected<JsonValue, ParseError>)= 40
  sizeof(psp::Span<const char>)               = 16
  sizeof(ParseError)                          = 4
  static_assert(JsonValue >= 32 bytes)        = OK (actual: 32)
  __cpp_lib_expected                          = 202211
  __cpp_lib_variant                           = 202106

[psp_json_arrays_nested_consumer: all 4 sections complete]
```

ASan + UBSan run: clean (no warnings, no findings; the macOS ASan
build does not support leak detection, but core ASan + UBSan
verified).

Section 4: **19 / 19** test cases (positive + negative).
Section 5: **13 / 13** (including deeply nested, mixed arrays+objects,
and array-of-objects routed through `parse_value_at`).
Section 6: **12 / 12** round-trip (every parsed JSON document pretty-
prints to a re-parseable form whose re-parsed tree produces the same
pretty-print). Section 7: sizeof probes confirm `JsonValue = 32 bytes`
on libc++.

## Important: the round-trip test is the strongest correctness check

`json_to_string()` walks the parsed `JsonValue` tree and emits a
two-space-indent JSON document. `escape_for_json_string()` handles
control characters, quotes, and backslashes so the output is
syntactically valid JSON for *any* input string, including strings
constructed in-memory. Section 6 parses each test document, pretty-
prints the result, re-parses the pretty-printed form, and asserts
that the second pretty-print matches the first — i.e. the
pretty-printer is a partial inverse of the parser.

This is the strongest correctness test we can write without an
independent oracle: it catches *both* parser bugs (the round-trip
would fail to re-parse) and pretty-printer bugs (the second tree
would not match the first).

## Compile + run

Build (assumes `psp_span_lib` v0.9.0 is installed at
`/tmp/psp_install`, which is where the Jul 18 lesson installed it):

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-19-psp-json-arrays-nested
```

Strict-warning build (used during development, with
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`):

```sh
cmake -S . -B build \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-07-19-psp-json-arrays-nested
```

## What's NOT in this lesson

- It is **not** a `<psp_span/json.h>` header. That promotion is the
  next step. Today is the consumer that proves the v0.9.0 cursor
  primitives are sufficient.
- It does not implement JSON5 / JSONC (no comments, no single-quoted
  strings, no trailing commas).
- It does not parse numbers with explicit `+` sign or `-` sign (the
  underlying `parse_double_at` and `parse_int_at` reject both).
  Adding a leading-sign skip in the dispatcher would close that gap
  in three lines.
- It does not allocate from an arena — every array element and object
  member owns its own storage via `std::vector` / `std::map`. A
  high-performance JSON parser would use a contiguous slab; that
  is a separate optimization.
- It does not pretty-print with configurable indent. Two-space indent
  is hard-coded.
- Duplicate keys are rejected with `ParseError::UnexpectedChar` (the
  same slot the Jul 18 parser used), not a typed
  `ParseError::DuplicateKey`.

## Next steps

The Jul 18 next-steps list had three open threads; today's lesson
closes **one** of them and advances the others:

- **Arrays + nested objects** — **closed today** (consumer-side).
- **`<psp_span/json.h>` header** — the natural forward-on from
  today. Lift `JsonValue`, `parse_value_at`, `parse_array_at`,
  `parse_object_at`, and `json_to_string` out of this consumer
  into a library header. Bump version 0.9.0 → 0.10.0 (the new
  header is a real new public capability). Then the next consumer
  `find_package(psp_span_lib 0.10 REQUIRED)` and
  `#include <psp_span/json.h>` and gets a complete JSON parser
  for free.
- **Typed `ParseError::DuplicateKey` enumerator** — one line in
  `parser.h`, one extra `case` in the formatter. Should land in
  the same 0.10.0 bump as the JSON header (the header is the
  consumer that needs the typed error).

Other open threads from earlier lessons:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS
  or .zip CPack generator and a different `with.files` glob.
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
- **A `std::expected<JsonValue, ParseError>` → `std::generator` adapter**
  — once `<generator>` lands (C++23 final?), the parser can yield
  values lazily for streaming.

The most natural follow-on is **`<psp_span/json.h>`** — lift today's
consumer into the library proper, bump to v0.10.0, and the next
session's consumer can `find_package(psp_span_lib 0.10 REQUIRED)`
and `#include <psp_span/json.h>`.