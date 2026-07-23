# P-2026-07-23 — `<psp_span/json_ext.h>` upgraded to v0.13.0: RFC 6902 §3 wire-format patch parser (`psp::json_patch::parse_patch_document`)

## Headline

The Jul 22 lesson (`P-2026-07-22-psp-json-patch.cpp`) shipped the
RFC 6902 Patch engine (`psp::json_patch::patch`) and its "Where we
go next" said:

> For P-2026-07-23 the natural lesson is **wire-format patch
> parsing**: take a JSON patch document like `[{"op": "add", ...},
> ...]` (the RFC 6902 §3 input shape), parse it via
> `psp::parse_value_at`, validate each op's fields, and assemble
> it into a `std::vector<JsonPatchOp>`.

Today is that lesson. The header upgrade adds:

- `psp::json_patch::parse_patch_document(std::string_view)`
  returning `std::expected<std::vector<JsonPatchOp>,
  JsonPatchError>`. The function parses a JSON Patch document
  (RFC 6902 §3 wire format — a JSON array of op objects) using
  `psp::parse_value_at`, validates each op's field shape, and
  builds the matching `JsonPatchOp` variant. The returned vector
  is ready to hand directly to `psp::json_patch::patch`.
- Three new file-scope enumerators on `::JsonPatchError`:
  `BadDocument` (the input wasn't an array of objects; or the
  `"op"` name was unrecognized), `MissingField` (a required
  field was absent: `"path"` for every op; `"value"` for
  add/replace/test; `"from"` for move/copy), `WrongType` (a
  field had the wrong JSON type — e.g. `"op"` was a number,
  `"path"` was an object).
- One file-scope `psp::json_patch::detail::build_one_op(map)
  -> std::expected<JsonPatchOp, JsonPatchError>` helper that
  the parser delegates to per op. It's the spot where the
  typed field-shape checks live.

The library version bumps **0.12.0 → 0.13.0**. The Pointer and
Patch halves are unchanged from v0.12.0; v0.13.0 is a **strict
superset**. Existing consumers continue to compile and run
with no source changes (verified — the Jul 21 Pointer consumer
and the Jul 22 Patch consumer both rebuild cleanly against
v0.13.0).

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
              v0.11.0 Pointer + JsonPatchError
                                              + ::JsonPatchOp
                                              + resolve_mut
                                              + json_patch::patch
Jul 23  <psp_span/json_ext.h> upgraded to         query layer (library-side) — RFC 6902 §3
              v0.13.0 — wire-format parser on      wire-format patch parser; closes the
              v0.12.0 + 3 new JsonPatchError    round-trip parser -> patch -> json_to_string
              (BadDocument / MissingField /      ← today
              WrongType)
                                              + parse_patch_document
```

This lesson closes the second of two arcs the Jul 22 lesson
identified. The natural forward-on from today — given the
**bytes → parse → patch → bytes** round-trip is now complete —
is a JSON Schema validator that exercises the same pipeline
(`parse_value_at` + `resolve` + `patch`) the Jul 22 lesson
flagged.

## What changed vs. Jul 22

| Layer                                          | v0.12.0   | v0.13.0                            |
|------------------------------------------------|-----------|------------------------------------|
| `<psp_span/span.h>`                            | unchanged | unchanged                          |
| `<psp_span/parser.h>`                          | unchanged | unchanged                          |
| `<psp_span/json.h>`                            | unchanged | unchanged                          |
| `<psp_span/json_ext.h>` Pointer half           | yes       | yes (unchanged)                    |
| `<psp_span/json_ext.h>` Patch half             | yes       | yes (unchanged)                    |
| `<psp_span/json_ext.h>` Patch parser half      | (none)    | NEW                                |
| `psp::json_patch::patch(root, ops)`            | yes       | yes (unchanged)                    |
| `psp::json_patch::parse_patch_document(doc)`   | (none)    | NEW                                |
| `psp::json_patch::detail::build_one_op(obj)`   | (none)    | NEW (private helper)               |
| `::JsonExtError`                               | 8 enums   | 8 enums (unchanged)                |
| `::JsonPatchOp`                                | 6-op      | 6-op (unchanged)                   |
| `::JsonPatchError`                             | 10 enums  | 13 enums (+3: BadDocument,         |
|                                                |           |          MissingField, WrongType)  |
| `find_package(psp_span_lib X REQUIRED)`        | 0.12      | 0.13                               |

The library gains **no new files**: the patch parser lives in
the existing `<psp_span/json_ext.h>` (alongside the Patch
engine from v0.12.0). The `JsonPatchError` enum grows in
place; its `std::formatter` specialisation adds the three new
cases.

## The new public API at a glance

```cpp
namespace psp {
namespace json_patch {
// The RFC 6902 §3 wire-format parser. Takes a JSON Patch
// document (a JSON array of op objects) as a string_view;
// returns a std::vector<JsonPatchOp> ready to hand to
// psp::json_patch::patch.
std::expected<std::vector<JsonPatchOp>, JsonPatchError>
parse_patch_document(std::string_view doc) noexcept;
}  // namespace json_patch
}  // namespace psp

// File-scope (so std::formatter can specialize for it).
// New enumerators in v0.13.0 (in addition to the 10 from
// v0.12.0):
enum class JsonPatchError {
    // ... existing 10 enumerators unchanged ...
    BadDocument,             // NEW
    MissingField,            // NEW
    WrongType,               // NEW
};
```

The new function lives in `psp::json_patch` alongside the
existing `patch()` — the natural pair. The new enumerators
extend the existing `JsonPatchError` vocabulary: the engine
already had `PointerMalformed`, `PointerNotFound`, etc. for
Pointer-layer failures; the parser adds `BadDocument`,
`MissingField`, `WrongType` for **wire-format** failures.
The two sets are disjoint, so callers can pattern-match on
the enumerator to decide "is this an engine error or a
document-shape error?".

## How the patch parser walks the tree

The wire format is:

```json
[
  { "op": "test",   "path": "/baz", "value": "qux"     },
  { "op": "remove", "path": "/baz"                     },
  { "op": "add",    "path": "/baz", "value": ["boo","hoo"] }
]
```

The parser has three stages:

1. **Parse the document.** `parse_value_at` turns the
   `string_view` into a `JsonValue` (a
   `std::vector<JsonValue>` of `std::map<string, JsonValue>`s).
   On `parse_value_at` failure: `BadDocument`.
2. **Verify the top-level is an array of objects.** A
   successful `parse_value_at` doesn't guarantee the input
   was a JSON array — it could have been a single object or
   a scalar. The parser checks `holds_alternative<vector>`
   and, per element, `holds_alternative<map>`. Mismatch:
   `BadDocument`.
3. **Build one `JsonPatchOp` per object.** This is
   `detail::build_one_op`'s job. It walks the map's keys,
   enforces the per-op field shape (`"op"` must be a
   string in the known set; `"path"` must be a string;
   `"value"` must exist for add/replace/test; `"from"`
   must exist and be a string for move/copy), and constructs
   the right struct (`AddOp{...}`, `RemoveOp{...}`, ...).
   The unknown-op-name case maps to `BadDocument` too —
   "this isn't a recognised Patch operation at all" is the
   same kind of failure as "this isn't a Patch document at
   all".

A subtle detail: the parser copies the `string_view` once
into a `std::string` before handing it to
`parse_value_at`. The copy is necessary because
`parse_value_at` takes `Span<const char>&` and shrinks the
span as it parses — but the span doesn't own the storage;
the caller does. Passing the original `string_view` directly
would either require the caller to guarantee the storage
outlives the parse (the standard library's `string_view`
doesn't promise that across the span's lifetime) or take a
`Span<const char>&` instead. The one-time copy is the
right ergonomic trade for the typical patch document size
(KBs, not MBs).

## Why the parser doesn't `std::visit` over a separate variant

The op structs are mutually exclusive — exactly one of
`AddOp`, `RemoveOp`, etc. holds. The natural alternative
to per-op `if`/`return` branches is `std::visit` on a
`std::variant<AddOp, RemoveOp, ...>`. We don't use it
because:

- The branch is on a **string** (`"op"` field value), not
  on a runtime-typed value. There's no variant of strings
  to visit on.
- A `std::visit` based implementation would need a
  temporary intermediate variant — build a generic "op"
  variant first, then visit. The per-op branches
  (`if (op_name == "add") { ... }`) are clearer.

The `std::variant<AddOp, ..., TestOp>` is still used as the
storage inside `JsonPatchOp::data` — that's what `patch()`
dispatches on. The parser's job is to **fill** that variant
correctly; the engine's job is to **consume** it. The two
halves of v0.13.0 mirror that split.

## RFC 6902 §3's "ignore unknown members" rule

Section 3 says: *"Other members of the operation object
SHALL be ignored."* The parser silently drops extras
(verified in Section 5 of the consumer: an op with extra
`"why"` and `"timestamp"` fields parses to a clean
`AddOp{path=/a}`). This matches what every production
Patch processor does.

The rationale: a Patch document is often produced by a
tool that tags ops with metadata (timestamps, user IDs,
trace IDs, ...). A consumer that rejects those because
they're "unknown" would be unusable. RFC 6902's choice to
require ignoring them is what makes interop possible.

The `MissingField` enumerator doesn't fire on optional
fields that happen to be absent (RFC 6902 doesn't have any
optional fields — every field is mandatory for its op
kind). It fires when a **mandatory** field is missing.

## Verified output

```
== Section 1: round-trip — RFC 6902 §1 example ==
  1: parsed 3 op(s) -> [test, remove, add]
  1: tree -> {
  "bar": "qux",
  "baz": [
    "boo",
    "hoo"
  ]
}

== Section 2: all six ops in one document ==
  2: parsed 6 op(s) -> [test, add, replace, copy, move, remove]
  2: tree -> {
  "n": 42
}

== Section 3: nested JSON values in 'value' ==
  3: parsed 2 op(s) -> [add, add]
  3: tree -> {
  "biscuits": [
    {
      "name": "Ginger Wale"
    }
  ]
}

== Section 4: malformed documents ==
  4a (object at top): -> BadDocument (as expected)
  4b (scalar at top): -> BadDocument (as expected)
  4c (malformed JSON): -> BadDocument (as expected)
  4d (array of numbers): -> BadDocument (as expected)
  4e (unknown op name): -> BadDocument (as expected)
  4f (add missing path): -> MissingField (as expected)
  4g (remove missing path): -> MissingField (as expected)
  4h (replace missing path): -> MissingField (as expected)
  4i (test missing path): -> MissingField (as expected)
  4j (move missing path): -> MissingField (as expected)
  4k (copy missing path): -> MissingField (as expected)
  4l (add missing value): -> MissingField (as expected)
  4m (replace missing value): -> MissingField (as expected)
  4n (test missing value): -> MissingField (as expected)
  4o (move missing from): -> MissingField (as expected)
  4p (copy missing from): -> MissingField (as expected)
  4q (op not a string): -> WrongType (as expected)
  4r (path not a string): -> WrongType (as expected)
  4s (from not a string): -> WrongType (as expected)

== Section 5: unknown extra members ignored ==
  5: parsed AddOp{path=/a} ignoring 'why' + 'timestamp'

== Section 6: empty documents ==
  6a: empty doc -> empty ops vector, tree unchanged: {
  "k": 1
}
  6b (empty string): -> BadDocument (as expected)

== Section 7: sizeof / feature probes ==
  sizeof(JsonPatchError)                          = 4
  sizeof(JsonPatchOp)                             = 72
  sizeof(std::vector<JsonPatchOp>)                 = 24
  sizeof(std::expected<std::vector<JsonPatchOp>,) = 32
  Public-header roster (v0.13.0):
    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)
    <psp_span/parser.h>   : psp::parse_int/parse_double + cursor + JSON scalars + ParseError (+ DuplicateKey in v0.10.0)
    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string + operator==/!= (v0.12.0)
    <psp_span/json_ext.h> : psp::json_pointer::split / to_string / resolve / resolve_mut
                             + psp::json_patch::patch (RFC 6902 §1 engine)
                             + psp::json_patch::parse_patch_document (RFC 6902 §3 wire-format parser) (NEW in v0.13.0)
                             + ::JsonPatchOp (RFC 6902 tagged union, 6 ops)
                             + ::JsonPatchError (13 enumerators, +3 in v0.13.0) + std::formatter spec
  __cpp_lib_expected                               = 202211
  __cpp_lib_variant                                = 202106

== Section 8: backwards compat — Pointer + Patch halves from v0.12.0 unchanged ==
  8a: resolve(/a/b/0) = 10 (Pointer half unchanged)
  8b: patch + resolve hand-built JsonPatchOp -> 99 (Patch half unchanged)

[psp_json_patch_parser: all 8 sections complete]
```

Section totals: **3 + 6 + 2 + 19 + 1 + 2 + ~10 + 2 = ~45**
test cases across 8 sections, all passing.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes
cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

Backwards-compat check: the Jul 21 (`psp_json_pointer`) and
Jul 22 (`psp_json_patch`) consumers both rebuild cleanly
against the new v0.13.0 header (re-built to
`build-v013/` directories to confirm); they see only the
unchanged Pointer + Patch halves and the same `JsonExtError`
+ `JsonPatchError` vocabulary, with three new enumerators
they don't use. No source changes needed.

## One real finding during development (and how it was fixed)

### Finding — `std::expected<vector<T>, E>` is 32 bytes, not 8

The sizeof probe in Section 7 reports
`sizeof(std::expected<std::vector<JsonPatchOp>, JsonPatchError>)`
as **32 bytes**. That's larger than expected for an
`expected` with a small error enum. The reason: the
`expected<T, E>` is laid out as `union { T value; E error; }`,
and on this toolchain (libc++) the union is sized by its
largest member — the `vector<JsonPatchOp>` (24 bytes —
3 pointers for data/size/capacity) plus an 8-byte
discriminator byte (bool) + 0 bytes of padding (the vector
already aligns to 8). 24 + 8 = 32.

This is the standard "largest alternative wins" rule for
the union part of `expected`. The size is fine — Patch
documents are typically a handful of ops; the vector
overhead is 3 pointers regardless of size.

What the finding taught us: `parse_patch_document` returns
a vector by value (not by reference, not by pointer). The
return is by `std::expected` so the caller can pattern-match
on success / failure; on success the vector moves into the
caller (RVO or move-construction, depending on the call
context). No deep-copy of the vector's contents happens
unless the caller explicitly does so.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v013 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v013
cmake --build late-may/cpp_practice/psp_span_lib/build-v013 --target install
```

Build the consumer (assumes v0.13.0 installed at
`/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_patch_parser -B late-may/cpp_practice/psp_json_patch_parser/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_patch_parser/build
./late-may/cpp_practice/psp_json_patch_parser/build/P-2026-07-23-psp-json-patch-parser
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_parser -B late-may/cpp_practice/psp_json_patch_parser/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_patch_parser/build-strict
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_parser -B late-may/cpp_practice/psp_json_patch_parser/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_patch_parser/build-asan
./late-may/cpp_practice/psp_json_patch_parser/build-asan/P-2026-07-23-psp-json-patch-parser
```

All three builds pass cleanly.

## What's NOT in this lesson

- **It is not a streaming patch parser.** The function
  takes `std::string_view` and copies it once before
  parsing; a streaming version would take
  `Span<const char>&` directly. The copy is the right
  trade for typical sizes (KBs); for a network protocol
  that consumes gigabytes of patches per second, the
  copy would be wrong.
- **It is not a Patch-document writer.** The function
  goes bytes-in → ops-in-memory. The reverse direction
  (ops-in-memory → bytes-out, e.g. as
  `serialise_patch_document(ops) -> string`) would be a
  thin wrapper around `json_to_string` over a synthesized
  `JsonValue` array — a separate lesson.
- **It does not apply the patch.** Today just parses.
  The Jul 22 lesson's `psp::json_patch::patch` is the
  apply. The consumer combines the two to demonstrate the
  full round-trip.
- **It does not validate RFC 6902 invariants.** A Patch
  document is required to have at least one op; the
  parser returns an empty vector for `[]` (which is a
  valid no-op patch, applied or not). RFC 6902 §3 says
  "an array of Operation objects", which doesn't
  preclude an empty array. We don't add a "patch must
  have at least one op" check because the engine
  already handles zero-ops cleanly (it iterates zero
  times).
- **It does not parse patch documents that aren't
  JSON arrays of objects.** The wire format is fixed by
  RFC 6902 §3 to be an array; everything else is
  rejected with `BadDocument`. JSON Patch documents in
  YAML or other serialisations are out of scope.
- **It does not enforce "all ops applied" rollback.**
  RFC 6902 §3 says "The patch SHOULD leave unmodified
  in case of failure"; we honour that only in the
  loose sense — the engine aborts on first failure, so
  the tree is partially mutated. The
  copy-on-write-backed "MUST leave unmodified" version
  is a separate lesson (the Jul 22 lesson explicitly
  flagged this as a future arc).

## Where we go next

The forward-ons the Jul 22 lesson left open (after today):

- **JSON Schema validation** — the natural follow-on;
  uses today's `parse_patch_document` + `patch` together
  to apply expected-state fixes mid-validation, and adds
  a `::JsonSchemaError` vocabulary. That lesson would
  consume v0.13.0 as-is.
- **Patch-document writer** — `serialise_patch_document`
  (ops-in-memory → bytes-out via `json_to_string`).
- **Streaming patch parser** — `parse_patch_document`
  taking `Span<const char>&` for a network protocol.
- **Transactional Patch** — copy-on-write backing store
  for the "MUST leave unmodified" guarantee (RFC 6902 §3
  strict mode). Closes the engine's
  "partially-mutated-on-failure" gap.

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

For the library as a whole, with today's lesson the
round-trip **`parse_value_at → patch → json_to_string`** is
complete for the case of Patch-shaped input. The next
library-wide forward-on is most likely a JSON Schema
validator in a new header `<psp_span/json_schema.h>`
that consumes v0.13.0 (parser + pointer + patch +
patch-parser) and adds its own typed failure mode.
That would close the query-layer arc the Jul 21 lesson
opened when it called out "JSON Pointer → JSON Patch
→ JSON Schema" as the natural progression.