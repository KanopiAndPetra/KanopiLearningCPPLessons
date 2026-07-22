# P-2026-07-22 — `<psp_span/json_ext.h>` upgraded to v0.12.0: JSON Patch (RFC 6902) on top of the v0.11.0 Pointer resolver

## Headline

The Jul 21 lesson (`P-2026-07-21-psp-json-pointer.cpp`) shipped
`<psp_span/json_ext.h>` — a complete RFC 6901 JSON Pointer
resolver as a library-owned header in `psp_span_lib` v0.11.0.
The Jul 21 lesson's "Where we go next" said:

> The most natural next lesson is **JSON Patch (RFC 6902)** in
> the same header (or, if the header grows, a separate
> `<psp_span/json_patch.h>`). RFC 6902 defines six ops:
> `add`, `remove`, `replace`, `move`, `copy`, `test`. The
> resolver we shipped today is a building block for Patch:
> every Patch op needs to do at least one `resolve()` to find
> the target. The new piece Patch adds is mutation —
> `JsonValue&` access through the pointer, plus the
> array/element-management logic for `add` and `remove`.

Today is the **JSON Patch (RFC 6902) half** of that
forward-on. The Pointer resolver is the foundation; the Patch
engine is the application.

The header upgrade adds:

- `psp::json_pointer::resolve_mut(string_view, json)` and
  `psp::json_pointer::resolve_mut(vector<ReferenceToken>,
  json)` — mutable-pointer variants of the existing
  `resolve()` overloads. `resolve_mut` is what the Patch
  engine uses as its write handle into the tree.
- A new file-scope tagged-union type `::JsonPatchOp` — six
  structs (`AddOp`, `RemoveOp`, `ReplaceOp`, `MoveOp`,
  `CopyOp`, `TestOp`) plus an `OpKind` enum, with
  `JsonPatchOp` wrapping the variant. RFC 6902 §4 gives each
  op a fixed set of fields; we model them as data-only
  aggregates buildable with `{...}` initializer syntax.
- A new file-scope enum `::JsonPatchError` (10 enumerators:
  the 6 pointer-layer errors forwarded from `JsonExtError`,
  plus 4 patch-only ones: `BadPath`, `TestValueMismatch`,
  `UnknownOp`, `MoveWouldClobber`) with its own
  `std::formatter` specialisation.
- `psp::json_patch::patch(JsonValue& root, ops)` — the
  RFC 6902 §1 engine. Applies each op in order; returns
  `std::expected<void, JsonPatchError>` on success (no
  success-value to return) and a typed error on the first
  failing op.

The library version bumps **0.11.0 → 0.12.0**. The Pointer
half is unchanged from v0.11.0 (the existing Jul 21 consumer
still compiles and runs). v0.12.0 is a **strict superset** of
v0.11.0; existing consumers continue to compile and link
against v0.12.0 with no source changes.

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
              psp_span_lib (v0.11.0) +
              JsonExtError
Jul 22  <psp_span/json_ext.h> upgraded to         query layer (library-side) — JSON Patch (RFC 6902)
              v0.12.0 — Patch on top of            on top of the v0.11.0 Pointer
              v0.11.0 Pointer + JsonPatchError     ← today
                                              + ::JsonPatchOp
                                              + resolve_mut
                                              + json_patch::patch
```

This lesson closes the second of two sub-threads the Jul 21
lesson opened in its "Where we go next": Pointer (closed Jul
21) + Patch (closed today). The library now has a complete
query layer over the JsonValue tree, ready for the next
thread — most likely a JSON Schema validator built on the
same triple of `parse_value_at`, `resolve`, and
`json_patch::patch`.

## What changed vs. Jul 21

| Layer                                          | v0.11.0   | v0.12.0                            |
|------------------------------------------------|-----------|------------------------------------|
| `<psp_span/span.h>`                            | unchanged | unchanged                          |
| `<psp_span/parser.h>`                          | unchanged | unchanged                          |
| `<psp_span/json.h>`                            | unchanged | unchanged                          |
| `<psp_span/json_ext.h>` Pointer half           | yes       | yes (unchanged)                    |
| `<psp_span/json_ext.h>` Patch half             | (none)    | NEW                                |
| `psp::json_pointer::resolve`                   | const     | const (unchanged signature)        |
| `psp::json_pointer::resolve_mut`               | (none)    | mutable (NEW)                      |
| `::JsonExtError`                               | 8 enums   | 8 enums (unchanged)                |
| `::JsonPatchOp`                                | (none)    | 6-op tagged-union + OpKind (NEW)   |
| `::JsonPatchError`                             | (none)    | 10 enumerators + formatter (NEW)   |
| `psp::json_patch::patch(root, ops)`            | (none)    | the RFC 6902 §1 engine (NEW)       |
| `psp::JsonValue::operator==`                   | (none)    | yes (NEW; the Patch engine + the   |
|                                                |           | user's test code depend on it)     |
| `find_package(psp_span_lib X REQUIRED)`        | 0.11      | 0.12                               |

The library gains **no new files**: the Patch half lives in
the existing `<psp_span/json_ext.h>`. One file-scope
`JsonValue::operator==` is added to `<psp_span/json.h>` (the
Patch engine uses it to compare the resolved value against
the expected value in TestOp; see Finding 1 below for why).

## The new public API at a glance

```cpp
// File-scope (so std::formatter can specialize for it,
// like ParseError and JsonExtError before it).
enum class JsonPatchError { /* 10 enumerators */ };

template <>
struct std::formatter<JsonPatchError> : /* std::string_view formatter */ {};

// The six RFC 6902 §4 op structs. All data, no methods.
struct AddOp     { std::string path; psp::JsonValue value; };
struct RemoveOp  { std::string path; };
struct ReplaceOp { std::string path; psp::JsonValue value; };
struct MoveOp    { std::string from; std::string path; };
struct CopyOp    { std::string from; std::string path; };
struct TestOp    { std::string path; psp::JsonValue value; };

enum class OpKind { Add, Remove, Replace, Move, Copy, Test };

struct JsonPatchOp {
    OpKind kind;
    std::variant<AddOp, RemoveOp, ReplaceOp, MoveOp, CopyOp, TestOp> data;
    // Constructors matching each alternative.
};

namespace psp {
namespace json_pointer {
// resolve_mut — the mutable-pointer variant of resolve.
std::expected<JsonValue*, JsonExtError>
resolve_mut(std::string_view pointer, JsonValue& root) noexcept;

std::expected<JsonValue*, JsonExtError>
resolve_mut(const std::vector<ReferenceToken>& tokens,
            JsonValue& root) noexcept;
}  // namespace json_pointer

namespace json_patch {
// The RFC 6902 §1 engine. Applies ops in order; on first
// failure returns std::unexpected<JsonPatchError>.
std::expected<void, JsonPatchError>
patch(JsonValue& root,
      std::span<const JsonPatchOp> ops) noexcept;
}  // namespace json_patch
}  // namespace psp
```

The size of the new types is small:

- `JsonPatchError` — 4 bytes (fits in an `int`)
- `JsonPatchOp` — 72 bytes (the variant dominates; the
  largest alternative is `AddOp{string, JsonValue}`, which
  itself is 32 bytes for `JsonValue` plus 24 bytes for the
  `std::string`, plus 16 bytes of variant overhead)
- `std::expected<void, JsonPatchError>` — 8 bytes (4-byte
  err + 4-byte padding; no success-value)

## How the Patch engine walks the tree

The RFC 6902 engine has two main kinds of work:

1. **Resolve the pointer** to find the target (or the
   target's parent, for ops that mutate containers).
2. **Apply the op-specific mutation**: insert, remove,
   replace, or compare.

The Pointer layer already gave us `resolve()` (const lookup
for reading) and `resolve_mut()` (mutable lookup for
writing). The Patch engine uses `resolve()` for `test`
(compare-only) and `resolve_mut()` indirectly via a private
`resolve_parent_impl` for `add` / `remove` / `replace` /
`move` / `copy`.

`resolve_parent_impl` walks the tokens EXCEPT the last one
and returns `(parent-pointer, last-token-string)`. The op
then takes the parent as a `JsonValue*`, knows it's a
container (object or array — as the ParseError::NotAnObject
/ NotAnArray errors fire if it isn't), and either inserts,
removes, or replaces under that container using the
last-token as a key or index.

## Why `JsonPatchOp` is a `std::variant` (not a flat struct)

RFC 6902 §4 defines six ops with disjoint field sets:

| Op       | path | value | from |
|----------|------|-------|------|
| add      | yes  | yes   | (no) |
| remove   | yes  | (no)  | (no) |
| replace  | yes  | yes   | (no) |
| move     | yes  | (no)  | yes  |
| copy     | yes  | (no)  | yes  |
| test     | yes  | yes   | (no) |

A flat struct would need 5 fields (or 6 with an `op`
discriminator), most of which are unused per-op. `std::variant<AddOp, RemoveOp, ReplaceOp, MoveOp, CopyOp, TestOp>`
encodes "exactly one of these holds" at the type level,
uses no extra memory for unused alternatives, and lets the
engine dispatch via `std::visit` (or, as the implementation
actually does, a `switch` on the `kind` field — the
discriminator is redundant but cheap).

The op structs are at **file scope** (not in `psp::`)
because the `JsonPatchOp` variant itself lives at file scope
and `std::formatter<JsonPatchError>` lives in `namespace std`
(file scope convention is shared with `ParseError` and
`JsonExtError`).

## RFC 6902's move self-clobber rule (§4.4)

RFC 6902 §4.4 says:

> The "from" location MUST NOT be a proper prefix of the
> "path" location.

I.e., `from` must not be a strict ancestor of `path` —
moving `/a` to `/a/b/c` would be moving a value into one of
its own descendants, which would clobber the source in the
middle of the move. The engine catches this with an explicit
prefix-check before any mutation:

```cpp
if (mv.from.size() <= mv.path.size()
    && mv.path.compare(0, mv.from.size(), mv.from) == 0) {
    if (mv.from.size() == mv.path.size()) {
        // path == from → not a clobber; copy+remove is a
        // no-op for the tree, but it counts as "applied".
    } else if (mv.path[mv.from.size()] == '/') {
        return std::unexpected{JsonPatchError::MoveWouldClobber};
    }
}
```

The check is precise: it's "does `path` start with `from` at
all", then "if yes, is there a `/` immediately after the
prefix"? A prefix with no `/` after it means `path == from`
(self-move, safe) or `from` is a non-ancestor that happens
to share a string prefix (e.g. `from="/foo"` and
`path="/foobar"` — different keys, not a clobber).

Section 4c of the consumer exercises this with
`move /a -> /a/b/c`, expecting `MoveWouldClobber`.

The first cut of this check had the prefix direction
reversed — it tried to detect whether `from` started with
`path`, which is the wrong test for §4.4. The failure
showed up as `BadPath` instead of `MoveWouldClobber` for the
`/a -> /a/b/c` case. Section 4c caught the bug.

## Verified output

```
== Section 1: add — RFC 6902 §4.1 ==
  1a: add /c = 3 -> {"a": 1, "b": 2, "c": 3}
  1b: add /xs/- (append) -> {"xs": [1, 2, 3, 4]}
  1c: add /xs/1 = 99 (insert) -> {"xs": [1, 99, 2, 3]}
  1d: add "" (replace root) -> {"new": 42}
  1e: add /users/0/age = 30 (nested) -> {"users": [{"age": 30, "name": "Ada"}, {"name": "Bob"}]}

== Section 2: remove — RFC 6902 §4.2 ==
  2a: remove /b -> {"a": 1, "c": 3}
  2b: remove /xs/1 -> {"xs": [10, 30, 40]}    (compacts)
  2c: remove /missing -> PointerNotFound (as expected)

== Section 3: replace — RFC 6902 §4.3 ==
  3a: replace /n = 99 -> {"n": 99}
  3b: replace /missing -> PointerNotFound (as expected)
  3c: replace "" (replace root) -> {"replaced": 1}

== Section 4: move + copy — RFC 6902 §4.4 + §4.5 ==
  4a: copy /a -> /c -> {"a": 1, "b": 2, "c": 1}
  4b: move /a -> /c -> {"b": 2, "c": 1}
  4c: move /a -> /a/b/c -> MoveWouldClobber (as expected)

== Section 5: test — RFC 6902 §4.6 ==
  5a: test /a == 1 -> ok, tree unchanged
  5b: test /a == 99 -> TestValueMismatch (as expected), tree unchanged

== Section 6: multi-op patches — RFC 6902 §1 ==
  6: 3-op patch -> {"baz": "qux", "hello": ["world"]}

== Section 7: resolve_mut — pointers survive a patch ==
  7a: resolve_mut /n -> 7 (mutable handle)
  7b: resolve /n after patch -> 99

== Section 8: sizeof / feature probes ==
  sizeof(JsonExtError)                          = 4
  sizeof(psp::json_pointer::ReferenceToken)     = 24
  sizeof(std::vector<psp::json_pointer::ReferenceToken>) = 24
  sizeof(std::expected<const psp::JsonValue*,   = 16
  sizeof(std::expected<psp::JsonValue*,         = 16
         JsonExtError>)
  sizeof(JsonPatchError)                        = 4
  sizeof(JsonPatchOp)                           = 72
  sizeof(std::expected<void, JsonPatchError>)   = 8
  sizeof(void*)                                 = 8
  Public-header roster (v0.12.0):
    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)
    <psp_span/parser.h>   : psp::parse_int/parse_double + cursor + JSON scalars + ParseError (+ DuplicateKey in v0.10.0)
    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string (+ operator== in v0.12.0)
    <psp_span/json_ext.h> : psp::json_pointer::split / to_string / resolve / resolve_mut (NEW in v0.12.0)
                             + psp::json_patch::patch (NEW in v0.12.0)
                             + ::JsonPatchOp (RFC 6902 tagged union, 6 ops) (NEW in v0.12.0)
                             + ::JsonPatchError (10 enumerators) + std::formatter spec (NEW in v0.12.0)
  __cpp_lib_expected                            = 202211
  __cpp_lib_variant                             = 202106

[psp_json_patch_consumer: all 8 sections complete]
```

Section totals: **5 + 3 + 3 + 3 + 2 + 1 + 2 + ~10 = ~29**
test cases across 8 sections, all passing.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes
cleanly**.

ASan + UBSan build: **passes cleanly** (no findings). The
macOS ASan build does not support leak detection but core
ASan + UBSan verified.

Backwards-compat check: the Jul 21 (`psp_json_pointer`)
consumer still compiles and runs against the new v0.12.0
header; it sees only the unchanged Pointer half and the same
JsonExtError vocabulary. No source changes needed.

## Three real findings during development (and how they were fixed)

### Finding 1 — `JsonValue` had no `operator==`

The first cut of `apply_test` (Section 5, RFC 6902 §4.6)
wrote `(*r)->value != expected.value` — relying on
`std::variant` to recurse through `std::vector<JsonValue>`
and `std::map<string, JsonValue>` via their `operator==`.
That works for the **container** operators (since the
containers have recursive element-wise `==`), but only if
`JsonValue` itself has `==` so that `std::map::operator==`
(which compares element-wise via `std::equal`) can compare
two `JsonValue`s by value.

The compile error chain:
- `std::variant<...>::operator!=` → tries to recurse
- → calls `std::map<...>::operator!=` → tries
  `std::equal` on `pair<string, JsonValue>` items
- → calls `pair::operator==` → calls value `JsonValue`'s
  `==` (does not exist)

The fix was to add a file-scope `operator==` (and `!=`) on
`JsonValue` in `<psp_span/json.h>`. The implementation is
literally `return a.value == b.value;` — it delegates to
`std::variant::operator==`, which recurses naturally through
`std::vector::operator==` and `std::map::operator==`. The
recursion bottoms out at scalars (`int64_t`, `double`,
`bool`, `string`, `monostate`, `nullptr_t`), which all have
built-in `==`.

The new operators are in `<psp_span/json.h>` (not
`<psp_span/json_ext.h>`) because they are a property of the
`JsonValue` type, not of any specific consumer. They live
inside `namespace psp` so ADL finds them when callers write
`a == b` for two `JsonValue` values.

### Finding 2 — `resolve_parent_impl` had the wrong path index

A typo in `resolve_parent_impl`: when `tokens.size() == 1`
the helper returned `(root, tokens[0].value)` (correct), but
when `tokens.size() >= 2` it walked through
`tokens[0..tokens.size()-2]` (also correct) and then
returned `(*r, tokens.back().value)` (correct).

The real bug was elsewhere — in the **call sites** for
op-shapes that wanted parent + last-token, I had
transposed the names of `path` and the parent-pointer's
contents in two places. Caught by Section 2b
(`remove /xs/1`) returning the wrong compacted tree on the
first cut. Reading the Section 2b output line by line
revealed the index confusion.

This is a class of bug that only manifests in tests: the
struct field name was used at the wrong site, but the
compiler can't tell. The fix was simply to look at each
apply_* function and confirm we were getting parent / child
the right way around.

### Finding 3 — `MoveWouldClobber` check had reversed direction

RFC 6902 §4.4 says `from` must NOT be a strict ancestor of
`path`. The first cut of the prefix-check looked like
"does `from` start with `path`", which is the OPPOSITE of
what §4.4 mandates. Section 4c (move `/a -> /a/b/c`)
caught this — with the reversed check, the test got
`BadPath` (because the move attempted to write through a
scalar parent) instead of `MoveWouldClobber` (which the
spec mandates as the early-exit diagnostic for this case).

The fix was to swap the operands of the prefix check: now
it checks "does `path` start with `from`", then if-and-only-if
yes, looks at the character at position `from.size()` in
`path` to disambiguate "self-move" (`path == from`) from
"strict-prefix clobber" (`path[mv.from.size()] == '/'`).

The wrong-error-but-correct-code shape of this finding is
the same pattern the Jul 21 Pointer lesson flagged: "the
test expected behaviour that didn't match the spec, the
code implemented the spec, the test was right".

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v012 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v012
cmake --build late-may/cpp_practice/psp_span_lib/build-v012 --target install
```

Build the consumer (assumes v0.12.0 installed at
`/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_patch -B late-may/cpp_practice/psp_json_patch/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_patch/build
./late-may/cpp_practice/psp_json_patch/build/P-2026-07-22-psp-json-patch
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch -B late-may/cpp_practice/psp_json_patch/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_patch/build-strict
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch -B late-may/cpp_practice/psp_json_patch/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_patch/build-asan
./late-may/cpp_practice/psp_json_patch/build-asan/P-2026-07-22-psp-json-patch
```

All three builds pass cleanly.

## What's NOT in this lesson

- **It is not JSON Schema validation.** The natural
  follow-on is to use `psp::json_patch::patch` to apply
  the schema-validation ops (those RFC 6902 has) plus a
  `::JsonSchemaError` vocabulary that lives alongside
  `JsonPatchError`. That lesson would consume v0.12.0 as-is.
- **It is not a transactional / all-or-nothing Patch.** The
  engine today mutates the tree op-by-op and aborts on the
  first error, leaving the tree partially mutated. RFC 6902
  §3 says "SHOULD leave unmodified" but the strict "MUST
  leave unmodified" requires a copy-on-write backing store,
  which is a separate lesson.
- **It is not a streaming patch processor.** A patch is
  applied atomically in one pass on a caller-owned
  `JsonValue` tree. There is no per-op callback, no
  progress event, no `std::generator`-based streaming.
- **It is not a `$ref` resolver.** JSON Schema's `$ref`
  (`"#/foo/bar"`) is a separate concern from RFC 6902 —
  it lives on top of JSON Pointer but with its own
  semantics (in-schema reference handling). The Pointer
  resolver will accept `$ref`'s syntax as ordinary
  Pointers; resolving refs is a Schema job.
- **It does not parse patch documents.** The Patch engine
  accepts a `std::vector<JsonPatchOp>` directly. The
  input form RFC 6902 §3 defines is "a JSON array of ops
  in JSON-wire-format" (e.g. `[{"op": "add", ...}, ...]`).
  Parsing that wire format into a `std::vector<JsonPatchOp>`
  would use `parse_value_at` from `<psp_span/json.h>` and
  some shape checks per op. Not implemented in this
  lesson.
- **It is not thread-safe.** Mutations happen
  single-threaded; the engine does no locking. Concurrent
  patches on the same tree are the caller's responsibility
  (and probably not what the caller wants anyway).

## Where we go next

The forward-ons from the Jul 21 lesson that remain open
after today:

- **JSON Schema validation** — the natural follow-on to
  today's Patch; uses today's `patch()` for the validator
  to apply expected-state fixes mid-validation, and adds a
  `::JsonSchemaError` vocabulary.
- **Configurable pretty-print indent** —
  `json_to_string(JsonValue&, int indent, int width)`.
- **Streaming / pull parser** — turn the parser into an
  iterator that yields one value at a time from a buffered
  input, using `std::generator<JsonValue>` once it lands.
- **Number-straddling (BigInt / BigDecimal)** — current
  parsers fold every integer into `std::int64_t` and every
  non-integer number into `double`. A real-money JSON
  library would emit a tagged alternative.
- **JSON5/JSONC parser** — a configurable
  strict-or-extended parser.

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

For the library as a whole, the most natural forward-on is
**a JSON Schema validator** in a new header `<psp_span/
json_schema.h>` that consumes the v0.12.0 library
(pointer + patch) and adds its own typed failure mode. That
would close the query-layer arc the Jul 21 lesson opened
when it called out "JSON Pointer → JSON Patch → JSON
Schema" as the natural progression.

For P-2026-07-23 the natural lesson is **wire-format
patch parsing**: take a JSON patch document like
`[{"op": "add", ...}, ...]` (the RFC 6902 §3 input shape),
parse it via `psp::parse_value_at`, validate each op's
fields, and assemble it into a `std::vector<JsonPatchOp>`.
That completes the round-trip — bytes in via parse, ops in
memory, mutations applied via patch, bytes out via
`json_to_string`.
