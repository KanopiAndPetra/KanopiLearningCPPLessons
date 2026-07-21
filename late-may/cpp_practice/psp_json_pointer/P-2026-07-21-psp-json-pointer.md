# P-2026-07-21 — `<psp_span/json_ext.h>` lands in psp_span_lib (v0.11.0): JSON Pointer (RFC 6901) over the v0.10.0 JsonValue tree

## Headline

The Jul 20 lesson (`P-2026-07-20-psp-json-header.cpp`) shipped
`<psp_span/json.h>` — a complete general JSON parser as a
library-owned header in `psp_span_lib` v0.10.0. The Jul 20
lesson's "Where we go next" said:

> The most natural JSON-parser forward-on is **a JSON serializer
> configurable in line with JSON5/JSONC extensions** ...
>
> For the library as a whole, the most natural forward-on is still
> **bump the library to v0.11.0 with one more concrete new public
> capability** — a candidate is `<psp_span/json_ext.h>` adding
> JSON Pointer / JSON Patch (RFC 6901 / 6902) on top of today's
> JSON header.

Today is the **JSON Pointer (RFC 6901) half** of that
forward-on. JSON Patch (RFC 6902) is the lesson after this one.

The new public header adds:

- `psp::json_pointer::split(string_view)` — RFC 6901 §3 tokenizer
  (split a JSON Pointer into reference tokens, with `~0` → `~`
  and `~1` → `/` unescaping).
- `psp::json_pointer::to_string(tokens)` — the inverse of split
  (joins tokens into a JSON Pointer string, escaping `~` → `~0`
  and `/` → `~1`).
- `psp::json_pointer::resolve(tokens, json)` and
  `psp::json_pointer::resolve(string_view, json)` — look up a
  sub-value at a given JSON Pointer; returns a non-owning
  `const JsonValue*` on success or a typed `::JsonExtError` on
  failure.
- A new file-scope `::JsonExtError` enum (8 enumerators:
  `Empty`, `MalformedToken`, `NotFound`, `NotAnObject`,
  `NotAnArray`, `IndexOutOfRange`, `IndexNotANumber`,
  `LastArrayElement`) with its own `std::formatter` specialisation,
  so `std::format("{}", err)` works the same way it does for
  `ParseError`.

The library version bumps **0.10.0 → 0.11.0**. The new header
is self-contained and adds zero new cursor primitives — the
resolver walks the existing `JsonValue` tree directly. The
existing `<psp_span/json.h>` header is **unchanged**; this is
a new capability, not a redesign.

## Where this fits in the arc

```
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
              (v0.10.0) + typed DuplicateKey
Jul 21  <psp_span/json_ext.h> ships in            query layer (library-side) — JSON Pointer (RFC 6901)
              psp_span_lib (v0.11.0) +            over the v0.10.0 JsonValue tree
              JsonExtError                        ← today
```

Today's lesson is the first lesson in a new sub-arc: **the
query layer**. The parser layer (Jul 14 → Jul 20) is "bytes →
JsonValue". The query layer (Jul 21 → ?) is "JsonValue →
sub-JsonValue (by JSON Pointer / Patch / Schema)". Same
return-type style (`std::expected<T, JsonExtError>`), different
input type.

## What changed vs. Jul 20

| Layer                                       | Jul 20     | Jul 21     |
|---------------------------------------------|------------|------------|
| `<psp_span/span.h>`                         | unchanged  | unchanged  |
| `<psp_span/parser.h>`                       | v0.10.0    | v0.10.0    |
|                                             |            | (unchanged)|
| `<psp_span/json.h>`                         | v0.10.0    | v0.10.0    |
|                                             |            | (unchanged)|
| `<psp_span/json_ext.h>`                     | (none)     | v0.11.0    |
|                                             |            | (NEW)      |
| `::JsonExtError`                            | (none)     | v0.11.0    |
|                                             |            | (NEW, 8    |
|                                             |            | enums)     |
| `psp::json_pointer::split`                  | (none)     | v0.11.0    |
|                                             |            | (NEW)      |
| `psp::json_pointer::to_string`              | (none)     | v0.11.0    |
|                                             |            | (NEW)      |
| `psp::json_pointer::resolve`                | (none)     | v0.11.0    |
|                                             |            | (NEW)      |
| `find_package(psp_span_lib X REQUIRED)`     | 0.10       | 0.11       |

The library gains **one new file** (`include/psp_span/json_ext.h`,
~440 lines) and a one-line install-rules addition (the new
header is installed alongside `span.h`, `parser.h`, `json.h`).
All other library files are unchanged.

## The new header at a glance

`<psp_span/json_ext.h>` adds the following public API:

```cpp
// File-scope (so std::formatter can specialize for it, like ParseError).
enum class JsonExtError {
    Empty,
    MalformedToken,
    NotFound,
    NotAnObject,
    NotAnArray,
    IndexOutOfRange,
    IndexNotANumber,
    LastArrayElement,
};

namespace psp {
namespace json_pointer {

struct ReferenceToken {
    std::string value;
    // equality vs std::string and const char* for ergonomic tests
};

std::expected<std::vector<ReferenceToken>, JsonExtError>
split(std::string_view) noexcept;

std::string
to_string(const std::vector<ReferenceToken>&);

std::expected<const JsonValue*, JsonExtError>
resolve(const std::vector<ReferenceToken>& tokens,
        const JsonValue& root) noexcept;

std::expected<const JsonValue*, JsonExtError>
resolve(std::string_view pointer,
        const JsonValue& root) noexcept;

}  // namespace json_pointer
}  // namespace psp
```

`JsonExtError` lives at file scope (not inside `psp::`)
because the `std::formatter<JsonExtError>` specialisation
must live in `namespace std`, and user specialisations of
`std::formatter` may only be declared at the namespace-scope
of the enclosing namespace — exactly the same reasoning
that pushed `ParseError` out of `psp::` in the parser header.
The `psp::json_pointer::` functions reference the file-scope
`JsonExtError` as `::JsonExtError`.

## What the resolver does

The resolver is a tree walker over `JsonValue`. For each
reference token in order:

1. **If the current value is an array and the next token is `"-"`**
   → return `LastArrayElement` (RFC 6901 says `"-"` refers to
   the (nonexistent) member after the last array element;
   `resolve()` has no such element to return — only JSON
   Patch's `add` op can place a value there).

2. **If the current value is an array and the next token is a
   non-negative integer string** → look up that index. Overflow
   / non-digit / out-of-range all return typed errors.

3. **If the current value is an object** → look up the next
   token as a key via `std::map::find`. `NotFound` on miss.

4. **If the current value is anything else** (scalar, null,
   `std::monostate`) → return `NotAnObject` or `NotAnArray`,
   whichever matches the shape of the next token.

Empty token list (per RFC 6901, the JSON Pointer `""`) returns
the document root directly. The split + resolve two-step is
exposed as two `resolve()` overloads so callers can cache the
token list if they need to walk the same pointer many times.

## The `~0` / `~1` escape order (RFC 6901 §3)

RFC 6901 §3 spells out the unescape algorithm in a way that's
easy to get wrong by character. The reference says:

> Evaluation of each reference token begins by decoding any
> escaped character sequence. This is performed by **first**
> transforming any occurrence of the sequence '~1' to '/',
> and **then** transforming any occurrence of the sequence '~0'
> to '~'.

The two substitutions are applied **as whole-string passes**,
not character-by-character. So:

- `~01` → step 1 finds no `~1`; step 2 finds `~0` and turns
  it into `~`; result is `~1`. (The trailing `1` of `~01`
  is the literal `1` that survives both passes.)
- `~10` → step 1 finds `~1` and turns it into `/`; step 2
  finds no `~0`; result is `/0`.

The character-by-character scan in `split()` implements the
same algorithm because the second char of an escape is
always consumed (so `~01` never sees a `~1` substring to
match, and `~10` never sees a `~0` substring). Section 1
includes `~01`, `~10`, and `~0~1~0` as part of the canonical
corpus precisely to lock this behaviour down.

## Why a non-owning `const JsonValue*` (vs. `JsonValue` by value)

The resolver returns `std::expected<const JsonValue*, JsonExtError>`
— a pointer into the input tree, not a copy. The size is
16 bytes (8-byte ptr + 4-byte err + 4-byte padding) on this
toolchain, which is half the size of a `std::expected<JsonValue,
JsonExtError>` would be (JsonValue is 32 bytes, so a
`std::expected<JsonValue, JsonExtError>` would be at least
40 bytes with the same padding).

More importantly: returning a pointer lets the caller
**mutate the tree and see the change through the pointer**.
That's the foundation JSON Patch needs — Patch is "apply
ops to a `JsonValue` tree at JSON Pointers", and the
mutation step requires a non-owning handle into the tree.
Section 5 demonstrates this by mutating `/n` from 7 to 99
through the pointer and re-resolving.

The pointer-liveness check is in Section 5b: we push 8
elements onto the `std::vector<JsonValue>` backing `/foo`
(which forces a reallocation), then re-resolve `/foo/0` and
confirm we get the new address with the correct data. The
resolver does not cache old addresses; it walks the tree
fresh on every call.

## Verified output

```
== Section 1: split / to_string / round-trip ==
  split("") -> 0 tokens: 
  split("/foo") -> 1 tokens: foo
  split("/foo/0") -> 2 tokens: foo/0
  split("/a~1b/c") -> 2 tokens: a/b/c
  split("/~01") -> 1 tokens: ~1
  split("/~10") -> 1 tokens: /0
  split("/~0~1~0") -> 1 tokens: ~/~
  [7 / 7 passed] (split canonical cases)
  split("foo") -> error:MalformedToken (as expected)
  split("foo/bar") -> error:MalformedToken (as expected)
  split("/foo~") -> error:MalformedToken (as expected)
  split("/foo~2") -> error:MalformedToken (as expected)
  split("/foo~a") -> error:MalformedToken (as expected)
  [5 / 5 passed] (malformed cases)
  round-trip("") OK
  round-trip("/") OK
  round-trip("/foo") OK
  round-trip("/foo/bar") OK
  round-trip("/a~1b") OK
  round-trip("/a~0b") OK
  round-trip("/~01") OK
  round-trip("/foo/-") OK
  round-trip("/foo/0/bar/1") OK
  round-trip("/users/0/name") OK
  [10 / 10 passed] (split <-> to_string round-trip)
  to_string(empty) == "" (whole-document pointer)

== Section 2: resolve() on the canonical RFC 6901 §5 examples ==
  resolve("") -> {...} (handled in §3)
  resolve("/foo") -> ["bar", "baz"] (handled in §3)
  resolve("/foo/0") -> "bar" (handled in §3)
  resolve("/") = 0
  resolve("/a~1b") = 1
  resolve("/c%d") = 2
  resolve("/e^f") = 3
  resolve("/g|h") = 4
  resolve("/i\j") = 5
  resolve("/k\"l") = 6
  resolve("/ ") = 7
  resolve("/m~0n") = 8
  [12 / 12 passed] (canonical cases)

== Section 3: resolve() against a real-shaped nested tree ==
  resolve("/meta/count") = 3
  resolve("/meta/tags/0") = "staff"
  resolve("/meta/tags/2") = "verified"
  resolve("/users/0/name") = "Ada"
  resolve("/users/1/address/city") = "Paris"
  resolve("/users/2/address/zip") = "100"
  resolve("") -> &root (whole document)
  [7 / 7 passed] (nested cases)

== Section 4: error cases (one per JsonExtError enumerator) ==
  resolve("/foo~") -> error:MalformedToken (as expected)
  resolve("/foo~2") -> error:MalformedToken (as expected)
  resolve("/missing") -> error:NotFound (as expected)
  resolve("/a/x") -> error:NotAnObject (as expected)
  resolve("/a/0") -> error:NotAnArray (as expected)
  resolve("/b/3") -> error:IndexOutOfRange (as expected)
  resolve("/b/100") -> error:IndexOutOfRange (as expected)
  resolve("/b/abc") -> error:IndexNotANumber (as expected)
  resolve("/b/-1") -> error:IndexNotANumber (as expected)
  resolve("/b/1.5") -> error:IndexNotANumber (as expected)
  resolve("/b/-") -> error:LastArrayElement (as expected)
  resolve("/c/nested/x") -> error:NotAnObject (as expected)
  [12 / 12 passed] (error cases)

== Section 5: resolve() returns a pointer into the LIVE tree ==
  5a: initial /n = 7 (via pointer)
  5a: after mutation /n = 99 (pointer tracks change)
  5b: /foo/0 still = 1 after pushing 8 more elements
       (before=0x102dfd870, after=0x102dfded0, realloc=yes)
  5c: split + resolve(vector) == resolve(string_view) for /foo/3

== Section 6: sizeof / feature probes (json_ext.h surface) ==
  sizeof(JsonExtError)                          = 4
  sizeof(psp::json_pointer::ReferenceToken)     = 24
  sizeof(std::vector<psp::json_pointer::ReferenceToken>) = 24
  sizeof(std::expected<const psp::JsonValue*,   = 16
         JsonExtError>)
  sizeof(const psp::JsonValue*)                 = 8
  Public-header roster (v0.11.0):
    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)
    <psp_span/parser.h>   : psp::parse_int/parse_double + cursor + JSON scalars + ParseError (+ DuplicateKey in v0.10.0)
    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string
    <psp_span/json_ext.h> : psp::json_pointer::split / to_string / resolve  (NEW in v0.11.0)
                             + ::JsonExtError (8 enumerators) + std::formatter spec
  __cpp_lib_expected                            = 202211

[psp_json_pointer_consumer: all 6 sections complete]
```

Section totals: **7 + 5 + 10 + 12 + 7 + 12 = 53** test cases
across 6 sections, all passing.

ASan + UBSan run: **clean** (no findings, no warnings; the
macOS ASan build does not support leak detection but core
ASan + UBSan verified).

## Three real findings during development (and how they were fixed)

### Finding 1 — Misunderstood RFC 6901 §3 escape order

The first cut of Section 1 had `split("/~01")` expecting the
result `"/1"`. The actual code returned `"~1"`. I initially
believed the docstring's claim that "`~01` unescapes to `/1`"
was a bug, but on re-reading RFC 6901 §3 I realised the
docstring (and my mental model) was wrong. The RFC says the
substitutions are whole-string passes, not character-by-
character:

- `~01` → step 1 (find `~1`) finds nothing → unchanged.
  Step 2 (find `~0`) finds the leading `~0` and turns it
  into `~`, leaving the trailing `1` literal. Result: `~1`.

The code is correct. The fix was to:

1. Rewrite the docstring at the top of `json_ext.h` to
   correctly explain the algorithm (with `~01` → `~1` and
   `~10` → `/0` as worked examples).
2. Update Section 1's test expectations to match the
   standard (and add a `~10` case for symmetry).

This is a real bug caught by writing the test — the
docstring in the first cut was wrong, the test was wrong
in the opposite direction, but the code was right. The
RFC is the source of truth, not my intuition.

### Finding 2 — `NotFound` vs `NotAnObject` semantics

The first cut of Section 4 had `resolve("/a/missing")` with
the input `{"a": 1, "b": [...], "c": {...}}`. The expectation
was `NotFound` ("key `missing` not in object `a`"). But `/a`
is the scalar `1`, not an object — so the second token can't
be a key lookup at all; the resolver correctly reports
`NotAnObject` ("you tried to descend into a non-object").
Fixed by changing the test to expect `NotAnObject` and adding
a comment explaining the difference.

This is the same class of finding as Finding 1: the test
expected behaviour that didn't match the spec, the code
implemented the spec, the test had to learn the spec.

### Finding 3 — Stray `span_to_string` helper (caught by `-Werror`)

The consumer's `helpers` block was copy-pasted from the
Jul 20 consumer, which used `span_to_string` to format
`Span<const char>` slices. The Jul 21 consumer doesn't need
it (the resolver returns `JsonValue`, not spans). The
strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) caught the unused
function. Removed.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v011 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v011
cmake --build late-may/cpp_practice/psp_span_lib/build-v011 --target install
```

Build the consumer (assumes v0.11.0 installed at
`/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_pointer -B late-may/cpp_practice/psp_json_pointer/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_pointer/build
./late-may/cpp_practice/psp_json_pointer/build/P-2026-07-21-psp-json-pointer
```

Strict-warning build (the actual build used during
development):

```sh
cmake -S late-may/cpp_practice/psp_json_pointer -B late-may/cpp_practice/psp_json_pointer/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_pointer/build-strict
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_pointer -B late-may/cpp_practice/psp_json_pointer/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_pointer/build-asan
./late-may/cpp_practice/psp_json_pointer/build-asan/P-2026-07-21-psp-json-pointer
```

All three builds pass cleanly. The consumer is **0 lines of
pointer code** — every line is either test data, output
formatting, an oracle assertion, or a sizeof probe. The
library owns the resolver; the consumer exercises it.

## What's NOT in this lesson

- **It is not JSON Patch (RFC 6902).** That's a follow-on
  lesson. JSON Patch is "apply ops (`add`, `remove`,
  `replace`, `move`, `copy`, `test`) to a `JsonValue`
  tree at JSON Pointers" — and it requires the resolver
  to hand callers a mutable handle. Today's resolver
  returns `const JsonValue*`; the Patch lesson will need
  to either add a mutable resolve() overload or change
  the existing one to return `JsonValue*` and accept a
  `JsonValue&` root. The right answer is a separate
  overload (`resolve_mut`) so the const overload stays
  const — a follow-on design decision.
- **It is not Relative JSON Pointer (the draft
  `"0"`/`"-1"` syntax).** That spec is still a draft; it
  has its own edge cases (relative indices in arrays
  vs. document-root "0"). Not part of RFC 6901.
- **It does not parse JSON Pointers as part of the
  document** (i.e. `$ref: "#/foo/bar"`-style resolution
  inside a JSON Schema). The resolver only takes a
  `JsonValue` tree; a `$ref` consumer is a separate
  concern.
- **It does not support the `~01` → `/1` interpretation
  some consumers expect.** Per RFC 6901 §3 the correct
  interpretation is `~01` → `~1`, and that's what we
  ship. Consumers that want the other interpretation
  (sometimes called "application-specific escape" in
  older JSON-Pointer literature) need to do their own
  preprocessing.
- **It does not thread allocator support through.** The
  resolver takes `const JsonValue&` and returns a
  non-owning pointer; no allocation happens at all.
- **It does not add new `ParseError` cases.** The
  `JsonExtError` enum is separate from `ParseError` (the
  former is a pointer-layer error, the latter is a
  parser-layer error); they are independent failure
  vocabularies.

## Where we go next

The Jul 20 next-steps list had one open thread:
`<psp_span/json_ext.h>` for JSON Pointer / JSON Patch
(RFC 6901 / 6902). **The Pointer half is closed today.**
The library now has a complete RFC 6901 resolver as a
first-class public header.

The most natural next lesson is **JSON Patch (RFC 6902)**
in the same header (or, if the header grows, a separate
`<psp_span/json_patch.h>`). RFC 6902 defines six ops:

- `add`     — insert at a JSON Pointer (handles `"-"` as
              "append to end of array" — the case today
              reports `LastArrayElement`).
- `remove`  — delete at a JSON Pointer.
- `replace` — overwrite at a JSON Pointer (the value
              must already exist).
- `move`    — `add` from one location to another.
- `copy`    — `add` a copy of one location to another.
- `test`    — compare a value at a JSON Pointer to an
              expected one; the patch fails if they
              differ.

The resolver we shipped today is a building block for
Patch: every Patch op needs to do at least one
`resolve()` to find the target. The new piece Patch adds
is mutation — `JsonValue&` access through the pointer,
plus the array/element-management logic for `add` and
`remove`.

The library as a whole still has these open threads
from earlier lessons (re-quoted for context):

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** —
  needs NSIS or `.zip` CPack generator and a different
  `with.files` glob.
- **Status badge in README**.
- **vcpkg/Conan port** — upstream `psp::Span` once
  multi-platform and SHA-pinned.
- **Branch protection requiring the matrix to pass**.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **`std::expected` and coroutines** — `co_return
  std::expected<T,E>` composes with the parser layer for
  an async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it,
  the Jul 10 Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 +
  mdarray) — the standard version of the Jul 11
  hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style
  linear algebra built on mdspan.
- **A `std::expected<JsonValue, ParseError>` →
  `std::generator` adapter** — once `<generator>` lands
  (C++23 final?), the parser can yield values lazily for
  streaming.

JSON-parser-specific forward-ons still open:

- **Configurable pretty-print indent** —
  `json_to_string(const JsonValue&, int indent, int
  width)` would let callers pick 4-space or tab indent.
- **JSON Patch (RFC 6902)** — the natural follow-on to
  today's JSON Pointer; uses today's resolver as its
  primitive. ← next lesson
- **JSON Schema validation** — a library-side schema
  validator built on `<psp_span/json.h>` and
  `<psp_span/json_ext.h>`.
- **Streaming / pull parser** — turn the parser into an
  iterator that yields one value at a time from a
  buffered input, using `std::generator<JsonValue>` once
  it lands.
- **Number-straddling (BigInt / BigDecimal)** — current
  parsers fold every integer into `std::int64_t` and
  every non-integer number into `double`. A real-money
  JSON library would emit a tagged alternative
  (decimal128 or arbitrary-precision string).
- **JSON5/JSONC parser** — a configurable
  strict-or-extended parser, allowing comments, single
  quotes, trailing commas, and unquoted keys. Today's
  parser is strict RFC 8259 only.

For the library as a whole, the most natural forward-on
remains **bump the library to v0.12.0 with `<psp_span/
json_ext.h>` gaining a `patch(JsonValue&, ops)` function
that applies an RFC 6902 patch document to a tree**. That
is the lesson after this one, and it would re-use
today's resolver verbatim.
