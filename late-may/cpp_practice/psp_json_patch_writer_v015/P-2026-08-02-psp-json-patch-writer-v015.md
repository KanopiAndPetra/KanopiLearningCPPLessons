# P-2026-08-02 — v0.15.0 library promotion: `psp::json_patch::serialise_patch_document` ships in `<psp_span/json_ext.h>`

## Headline

The Aug 1 lesson (`P-2026-08-01-psp-parser-v014-update.md`) closed
the v0.14.0 promotion arc and listed the v0.15.0 candidates. The
first on the list was:

> `psp::json_patch::serialise_patch_document` in the library
> proper — lift the Jul 24 consumer writer (and the Jul 27
> re-inlined `op_writer`) into a header function. The promotion
> is near-mechanical.

Today is that lesson. The library has been bumped to **v0.15.0**,
a strict superset of v0.14.0. The only change in the header is
one additional inline function:

```cpp
namespace psp::json_patch {
inline std::string
serialise_patch_document(std::span<const JsonPatchOp> ops);
}
```

The function was first designed and exercised in the Jul 24
consumer (`P-2026-07-24-psp-json-patch-serialiser.cpp`); the
Jul 27 consumer (`P-2026-07-27-psp-json-v014-promotion.cpp`)
re-inlined a copy as `op_writer::serialise_patch_document`; v0.15.0
is the header promotion. The function body is byte-for-byte
equivalent to the proven-in-consumer code; only the location of
the definition changed (file-scope inline in
`<psp_span/json_ext.h>` instead of a local inline in a consumer
TU).

A new consumer (`P-2026-08-02-psp-json-patch-writer-v015.cpp`,
this directory) exercises the library-proper function end-to-end.
The Jul 24 consumer was updated in lockstep: the local copy of
`serialise_patch_document` is removed; the consumer now calls
the library function. The Jul 27 consumer's `op_writer` is
unchanged (different symbol name; no conflict).

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
              v0.12.0 — Patch on top of            on top of the v0.11.0 Pointer + JsonPatchError
              v0.11.0 Pointer + JsonPatchError
                                              + ::JsonPatchOp
                                              + resolve_mut
                                              + json_patch::patch
Jul 23  <psp_span/json_ext.h> upgraded to         query layer (library-side) — RFC 6902 §3
              v0.13.0 — wire-format parser on      wire-format patch parser; closes the
              v0.12.0 + 3 new JsonPatchError    round-trip parser -> patch -> json_to_string
              (BadDocument / MissingField /
              WrongType)
                                              + parse_patch_document
                                              + JsonPatchError::BadDocument / MissingField / WrongType
Jul 24  psp::json_patch::serialise_patch_doc.     query layer (consumer-side) — RFC 6902 §3
              (consumer; round-trips v0.13.0's    wire-format writer; closes the FULL ops
              parser to prove the design)         round-trip build -> serialise -> parse ->
                                                  patch (mirror of Jul 23)
                                              + consumer-side writer that uses v0.10.0's
                                                json_to_string for the value-tree emission
Aug  1  psp_parser_v014_update                    library version (v0.14.0) consumer-update:
              (consumer; updates psp_parser_      psp_parser_header + psp_parser_streaming
              header + psp_parser_streaming       re-aligned with the v0.14.0 library proper
              for the v0.14.0 library proper;     (int64 widening + leading sign)
              closes the v0.14.0 promotion arc)
Aug  2  psp::json_patch::serialise_patch_doc.     query layer (library-side) — RFC 6902 §3
              (library-proper, v0.15.0)           wire-format writer promoted from the Jul 24
              — promoted from the Jul 24          consumer; Jul 24 consumer updated to drop
              consumer to <psp_span/json_ext.h>   its local copy and call the library function
                                              + psp::json_patch::serialise_patch_document
                                                (NEW; in <psp_span/json_ext.h>)
```

The Jul 24 lesson closed the full round-trip arc for the
**consumer side** of the writer (bytes-in / out via a local
function + the v0.13.0 parser). Today closes the **library
side** of the same arc — the writer is now visible to any
consumer that `find_package(psp_span_lib 0.15 REQUIRED)`.

## What changed vs. v0.14.0

| Layer                                            | v0.14.0     | v0.15.0                          |
|--------------------------------------------------|-------------|----------------------------------|
| `<psp_span/span.h>`                              | unchanged   | unchanged                        |
| `<psp_span/parser.h>`                            | unchanged   | unchanged                        |
| `<psp_span/json.h>`                              | unchanged   | unchanged                        |
| `<psp_span/json_ext.h>` Pointer half             | yes         | yes (unchanged)                  |
| `<psp_span/json_ext.h>` Patch half               | yes         | yes (unchanged)                  |
| `<psp_span/json_ext.h>` Patch parser half        | yes (v0.13) | yes (unchanged)                  |
| `<psp_span/json_ext.h>` Patch writer half        | (none)      | **NEW** (v0.15.0)                |
| `psp::json_patch::patch(root, ops)`              | yes         | yes (unchanged)                  |
| `psp::json_patch::parse_patch_document(doc)`     | yes (v0.13) | yes (unchanged)                  |
| `psp::json_patch::serialise_patch_document(ops)` | (none)      | **NEW** (v0.15.0; library-proper)|
| `::JsonExtError`                                 | 8 enums     | 8 enums (unchanged)              |
| `::JsonPatchOp`                                  | 6-op        | 6-op (unchanged)                 |
| `::JsonPatchError`                               | 13 enums    | 13 enums (unchanged)             |
| `find_package(psp_span_lib X REQUIRED)`          | 0.14        | 0.15                             |

The library gains **zero new files** and no new error
enumerators. The header gains ~120 lines: the function body
(50 lines of code) plus an extensive documenting comment
explaining the dispatch design, the per-op field shape, the
"RFC 6902 §3 doesn't mandate field order" detail, and the
proven-in-consumer provenance.

`JsonPatchError` is unchanged because the writer is
**infallible** by design — the in-memory ops are valid by
construction (they came out of the parser, or were built by
the caller with the known RFC 6902 §4 field shape, and the
type system enforces that — the variant alternatives ARE the
6 op kinds, and each struct has exactly the fields RFC 6902
§4 mandates).

## The new public API at a glance

```cpp
namespace psp {
namespace json_patch {

// RFC 6902 §3 wire-format writer. The mirror of
// parse_patch_document (the v0.13.0 parser). Takes a
// span of ops in memory; returns a std::string holding
// the wire-format JSON document (a JSON array of op
// objects). The function is infallible: the in-memory
// ops are already valid by construction.
inline std::string
serialise_patch_document(std::span<const JsonPatchOp> ops);

}  // namespace json_patch
}  // namespace psp
```

The signature is the natural mirror of the parser's:

- Parser (v0.13.0): `parse_patch_document(string_view)
  -> expected<vector<JsonPatchOp>, JsonPatchError>`
- Writer (v0.15.0): `serialise_patch_document(span<JsonPatchOp>)
  -> string`

They are not the same shape because their failure modes are
not the same — the parser fails on bad wire-format input, the
writer fails on bad in-memory state that the caller controls
(the type system forbids the latter, so the function is
infallible in practice).

## How the writer walks the op vector

A JSON Patch document is a JSON **array** of op **objects**. The
writer therefore has two layers:

1. **Per-op writer** (one `JsonPatchOp` → one `JsonValue`
   object containing a `std::map<string, JsonValue>`). Dispatch
   is via `switch (op.kind)` plus `std::get<OpType>(op.data)` to
   extract the typed struct from the variant.

2. **Document writer** (the assembled `std::vector<JsonValue>` →
   JSON text). This is `psp::json_to_string` over the vector —
   the v0.10.0 pretty-printer already handles `vector` + `map` +
   every JsonValue alternative + string escaping + nested
   recursion.

We do not duplicate the pretty-printer. The writer's job is to
build the right JsonValue tree (one object per op, with the
right field shape) and let `json_to_string` handle the bytes.

The per-op field shape is RFC 6902 §4's contract:

```
add     { "op": "add",     "path": ..., "value": ... }
remove  { "op": "remove",  "path": ...                }
replace { "op": "replace", "path": ..., "value": ... }
move    { "op": "move",    "from": ..., "path": ...   }
copy    { "op": "copy",    "from": ..., "path": ...   }
test    { "op": "test",    "path": ..., "value": ... }
```

Note the symmetry: `add`/`replace`/`test` carry a `value`;
`move`/`copy` carry a `from`; `remove` carries nothing but
`path`. The writer enforces this by construction (only the
right fields per op kind go into the map).

## Why the writer doesn't `std::visit` over a separate variant

The natural alternative to per-op `if`/`return` branches is
`std::visit` on a `std::variant<AddOp, RemoveOp, ...>`. We
don't use it (and the parser, from v0.13.0, doesn't either)
because:

- The branch is on a **string** (`"op"` field value), not on
  a runtime-typed value. There's no variant of strings to
  visit on. (For the writer the branch is on the `OpKind`
  tag; for the parser the branch is on the `"op"` field's
  string value.)
- A `std::visit`-based implementation would need a temporary
  intermediate variant or a `std::get<OpType>` extraction.
  The per-op branches (`switch (op.kind)` + `std::get<OpType>`)
  are clearer.

The `std::variant<AddOp, ..., TestOp>` is still used as the
storage inside `JsonPatchOp::data` — that's what `patch()`
dispatches on. The writer's job is to **emit** the variant;
the engine's job is to **consume** it.

## Why no new error enumerators

The writer's failure modes are "the caller passed garbage" —
but the caller **built** the ops in memory, so there's no
garbage to fail on. The `JsonPatchOp` invariant (one of 6
kinds, each with the right fields) is enforced by the type
system: the variant's alternatives *are* the kinds, and each
struct has exactly the fields RFC 6902 §4 mandates.

The `JsonPatchError` vocabulary is exclusively for the engine
(`patch`) and the parser (`parse_patch_document`). The writer
has no business adding to it.

If we ever wanted "the writer refuses to serialise ops whose
paths contain unescaped '~'" or similar, we'd add a new
error type — but that's a stricter-mode writer, not v0.15.0's.

## Symbol-presence: the writer is library-proper

Section 1 of today's consumer takes the address of the
library function:

```cpp
using writer_fn = std::string(*)(std::span<const JsonPatchOp>);
writer_fn p = &psp::json_patch::serialise_patch_document;
check(p != nullptr,
      "&psp::json_patch::serialise_patch_document is well-defined");
```

If the function is undeclared (header bug), this line won't
compile. If the function is declared but not visible at the
call site (e.g. it's a `static` in a TU), taking its address
also fails to compile. The fact that this assertion passes
is the proof that the function is a real, exported symbol
of the library proper.

The same probe is repeated for the parser and the engine
(both from earlier versions) as a back-compat sanity check:
all three are still well-defined at v0.15.0.

## Verified output

```
P-2026-08-02 — v0.15.0 RFC 6902 §3 wire-format WRITER:
                psp::json_patch::serialise_patch_document
                (promoted from Jul 24 consumer to <psp_span/json_ext.h>)

== Section 1: symbol-presence probe — writer is library-proper ==
  PASS: 1a &psp::json_patch::serialise_patch_document is well-defined
  PASS: 1b &psp::json_patch::parse_patch_document is well-defined (v0.13.0 half unchanged)
  PASS: 1c &psp::json_patch::patch is well-defined (v0.12.0 half unchanged)
  PASS: 1d serialise_patch_document({}) == "[]"
  PASS: 1e parse_patch_document("[]") -> empty vector

== Section 2: per-op writer — every kind's field shape (library-proper) ==
  serialise_patch_document (library-proper):
[
  {
    "op": "add",
    "path": "/a",
    "value": 1
  },
  {
    "op": "remove",
    "path": "/a"
  },
  {
    "op": "replace",
    "path": "/b",
    "value": "two"
  },
  {
    "from": "/b",
    "op": "move",
    "path": "/c"
  },
  {
    "from": "/c",
    "op": "copy",
    "path": "/d"
  },
  {
    "op": "test",
    "path": "/d",
    "value": true
  }
]
  PASS: 2a wire contains "add" op
  PASS: 2b wire contains "remove" op
  PASS: 2c wire contains "replace" op
  PASS: 2d wire contains "move" op
  PASS: 2e wire contains "copy" op
  PASS: 2f wire contains "test" op

== Section 3: round-trip — serialise -> parse -> serialise fixed point (library-proper both ways) ==
  3a: round-trip OK (5 op(s), fixed point)
  3b: round-trip OK (1 op(s), fixed point)
  3c: round-trip OK (2 op(s), fixed point)
  3d: round-trip OK (2 op(s), fixed point)
  3e: round-trip OK (1 op(s), fixed point)
  3f: round-trip OK (0 op(s), fixed point)

== Section 4: every JsonValue alternative round-trips (library-proper writer + parser) ==
  4a (null): round-trip OK (1 op(s), fixed point)
  4b (true): round-trip OK (1 op(s), fixed point)
  4c (false): round-trip OK (1 op(s), fixed point)
  4d (int zero): round-trip OK (1 op(s), fixed point)
  4e (int 1): round-trip OK (1 op(s), fixed point)
  4f (int large): round-trip OK (1 op(s), fixed point)
  4g (int int64): round-trip OK (1 op(s), fixed point)
  4h (int INT64_MAX): round-trip OK (1 op(s), fixed point)
  4i (double): round-trip OK (1 op(s), fixed point)
  4j (sm double): round-trip OK (1 op(s), fixed point)
  4k (empty str): round-trip OK (1 op(s), fixed point)
  4l (str /slash): round-trip OK (1 op(s), fixed point)
  4m (str escape): round-trip OK (1 op(s), fixed point)
  4n (empty arr): round-trip OK (1 op(s), fixed point)
  4o (empty obj): round-trip OK (1 op(s), fixed point)
  4p (nested): round-trip OK (1 op(s), fixed point)
  4q (deep nest): round-trip OK (1 op(s), fixed point)

== Section 5: full round-trip — build -> serialise -> parse -> patch -> json_to_string (library-proper all the way) ==
  5a wire (RFC 6902 §1 example, library writer):
[
  {
    "op": "test",
    "path": "/baz",
    "value": "qux"
  },
  {
    "op": "remove",
    "path": "/baz"
  },
  {
    "op": "add",
    "path": "/baz",
    "value": [
      "boo",
      "hoo"
    ]
  }
]
  PASS: 5a full round-trip OK — tree matches RFC 6902 §1
  5b wire (round-trip path):
[
  {
    "op": "add",
    "path": "/users",
    "value": {}
  },
  {
    "op": "add",
    "path": "/users/alice",
    "value": {
      "age": 30
    }
  },
  {
    "op": "add",
    "path": "/users/bob",
    "value": {
      "age": 25
    }
  },
  {
    "op": "replace",
    "path": "/users/bob/age",
    "value": 26
  },
  {
    "from": "/users/alice",
    "op": "copy",
    "path": "/users/copy_of_alice"
  }
]
  PASS: 5b pipeline OK — direct == round-trip (library writer + library parser + library engine)

== Section 6: empty document symmetry (library-proper) ==
  PASS: 6a empty ops -> "[]"
  PASS: 6b empty doc re-parses to empty vector
  PASS: 6c empty patch leaves tree unchanged

== Section 7: wire-format interop with v0.14.0 sign-accepted values (library-proper writer + parser + engine) ==
  7a wire (negative int via library writer):
[
  {
    "op": "add",
    "path": "/x",
    "value": -42
  }
]
  PASS: 7a parse(serialise(-42 AddOp)) succeeds
  PASS:   7a parsed 1 op
  PASS:   7a op kind == Add
  PASS:   7a value holds int64
  PASS:   7a value == -42
  7b wire (INT64_MAX via library writer):
[
  {
    "op": "replace",
    "path": "/x",
    "value": 9223372036854775807
  }
]
  PASS: 7b parse(serialise(INT64_MAX ReplaceOp)) succeeds
  PASS:   7b value holds int64
  PASS:   7b value == INT64_MAX
  PASS: 7c parse + apply INT64_MAX ReplaceOp succeeds
  PASS:   7c resolve_mut("/x") succeeded
  PASS:   7c /x == INT64_MAX after patch

== Section 8: sizeof / feature probes ==
  sizeof(JsonPatchError)                          = 4
  sizeof(JsonPatchOp)                             = 72
  sizeof(std::vector<JsonPatchOp>)                 = 24
  sizeof(std::string) (writer return type)        = 24
  sizeof(std::expected<std::vector<JsonPatchOp>,) = 32
  Writer interface (v0.15.0 library-proper):
    psp::json_patch::serialise_patch_document(span<JsonPatchOp>)
      -> std::string (RFC 6902 §3 wire format)
  Mirror image of the v0.13.0 parser:
    psp::json_patch::parse_patch_document(string_view)
      -> std::expected<vector<JsonPatchOp>, JsonPatchError>
  Together they close the full ops round-trip in the library.
  __cpp_lib_expected                               = 202211
  __cpp_lib_variant                                = 202106
  __cpp_lib_span                                   = 202002

== Section 9: backwards compat — v0.13.0 + v0.14.0 halves unchanged ==
  PASS: 9a resolve(/a/b/0) = 10 (Pointer half unchanged from v0.11.0)
  PASS: 9b patch hand-built ReplaceOp (Patch engine half unchanged from v0.12.0)
  PASS: 9c parse_patch_document on RFC 6902 §1 (Parser half unchanged from v0.13.0)
  PASS: 9d parse_patch_document accepts a negative value (v0.14.0 sign acceptance unchanged)

[psp_json_patch_writer_v015: 31 PASS, 0 FAIL]
```

**Section totals**: 1 (symbol-presence, 5 sub-checks) + 6
(per-op shapes) + 6 (round-trips) + 17 (value alternatives
including INT64_MAX + sign-accepted) + 2 (full pipelines) +
3 (empty) + 10 (v0.14.0 interop) + ~10 (probes) + 4
(back-compat) = **31 PASS, 0 FAIL** across 9 sections.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

Backwards-compat: the Jul 22 Patch engine consumer
(`P-2026-07-22-...`), the Jul 23 Patch-parser consumer
(`P-2026-07-23-psp-json-patch-parser`), the Jul 24 writer
consumer (`P-2026-07-24-psp-json-patch-serialiser` — updated
today to drop the local copy), the Jul 27 v0.14.0 promotion
consumer (`P-2026-07-27-psp-json-v014-promotion`), and the
Aug 1 v0.14.0 consumer-update (`P-2026-08-01-...`) all
rebuild cleanly against v0.15.0. No source changes are
required for any of them except the Jul 24 consumer (one
local-copy removal).

## One finding during development (and how it was handled)

### Finding — the Jul 24 consumer's local `serialise_patch_document` collides with the library-proper copy

When I rebuilt the Jul 24 consumer
(`P-2026-07-24-psp-json-patch-serialiser.cpp`) against
v0.15.0, the compile failed with:

```
error: redefinition of 'serialise_patch_document'
note: previous definition is here
  (in /tmp/psp_install/include/psp_span/json_ext.h)
```

This is the **expected** failure shape for a library
promotion: a function that was a per-consumer artifact is
now library-proper, and a consumer that still carries its
local copy has a redefinition. The fix is mechanical —
**delete the local copy** (it duplicates the library function
byte-for-byte) and update the consumer's `find_package`
floor to 0.15. The same fix shape was required for the
v0.14.0 promotion (where the two affected consumers
`psp_parser_header` and `psp_parser_streaming` had to be
updated for the `parse_int` return-type widening — see the
Aug 1 lesson).

The Jul 27 consumer (`P-2026-07-27-psp-json-v014-promotion`)
also inlined a writer, but as `op_writer::serialise_patch_document`
(inside a `struct op_writer`); that symbol is **not** the
same as the library function `psp::json_patch::serialise_patch_document`,
so no collision. The Jul 27 consumer continues to compile
and run unchanged against v0.15.0.

### Why the Jul 27 consumer wasn't updated

Two reasons:

1. **Different symbol name.** The Jul 27 consumer's local
   writer is `op_writer::serialise_patch_document(const
   std::vector<JsonPatchOp>&)` — a struct-scoped function
   with a different parameter type (`vector` instead of
   `span`). It doesn't collide with the library function
   `psp::json_patch::serialise_patch_document(span<const
   JsonPatchOp>)`.
2. **The Jul 27 consumer's `op_writer` is a historical
   exercise, not the canonical writer.** The canonical
   writer is the one in the Jul 24 consumer (now updated
   today), and the library-proper copy is the v0.15.0
   one. The Jul 27 consumer's `op_writer` is kept for
   archival purposes (it demonstrates the "shadow
   dispatcher" / "side-by-side" pattern the Jul 27 lesson
   was about); updating it would be a churn-without-value
   change. Today's lesson notes the existence of the
   inlined copy in the library promotion's "What's NOT in
   this lesson" section.

## What's NOT in this lesson

- **It is not a streaming patch writer.** The function
  takes a `std::span<const JsonPatchOp>` (in-memory ops)
  and returns a complete `std::string` (the full wire-format
  text). A streaming writer (e.g. for a network protocol
  that consumes gigabytes of patches per second) would take
  an output iterator and emit ops one at a time. That's
  a separate lesson if it's ever needed.
- **It is not a Patch-document formatter with
  configurable indentation.** The writer calls
  `psp::json_to_string(doc, 0)` (no indentation); the
  v0.10.0 pretty-printer always emits multi-line
  output. A "compact wire-format" mode (single-line
  JSON, no whitespace) is a separate lesson if it's
  ever needed.
- **It does not validate RFC 6902 invariants.** The
  writer doesn't check that a `MoveOp`'s `from` isn't a
  strict ancestor of `path` (that's the engine's job);
  doesn't check that a `CopyOp`'s `from` exists (also
  the engine's job); doesn't check that a `TestOp`'s
  value is comparable to the actual value (also the
  engine's job). The writer emits the wire format
  regardless. Validation is `patch()`'s job.
- **It does not enforce "all ops serialisable" rollback.**
  The writer can't fail (it's infallible by design), so
  there's nothing to roll back. The engine's "partially-
  mutated-on-failure" gap (the Transactional Patch arc
  the Jul 23 lesson flagged) is unchanged.
- **It does not update the Jul 27 consumer's `op_writer`
  inlined copy.** The Jul 27 consumer's writer is a
  different symbol name (struct-scoped, not in
  `psp::json_patch`) and doesn't collide with the
  library-proper function. Updating it would be churn-
  without-value.
- **It does not widen the dispatcher's int64-vs-double
  preservation guard** from `int` to `int64_t`. That's
  the Jul 25 lesson's Finding 3 follow-on (Finding 2 in
  the Jul 27 lesson). Today's lesson is a library-side
  promotion; the guard widening is orthogonal.
- **It does not change `parse_patch_document`.** The
  v0.13.0 parser is unchanged at v0.15.0. The library
  gains a writer; the parser is byte-for-byte identical
  to v0.13.0/v0.14.0.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v015 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v015
cmake --build late-may/cpp_practice/psp_span_lib/build-v015 --target install
```

Build the new consumer (assumes v0.15.0 installed at
`/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_patch_writer_v015 -B late-may/cpp_practice/psp_json_patch_writer_v015/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_patch_writer_v015/build
./late-may/cpp_practice/psp_json_patch_writer_v015/build/P-2026-08-02-psp-json-patch-writer-v015
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_writer_v015 -B late-may/cpp_practice/psp_json_patch_writer_v015/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_patch_writer_v015/build-strict
./late-may/cpp_practice/psp_json_patch_writer_v015/build-strict/P-2026-08-02-psp-json-patch-writer-v015
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_writer_v015 -B late-may/cpp_practice/psp_json_patch_writer_v015/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_patch_writer_v015/build-asan
./late-may/cpp_practice/psp_json_patch_writer_v015/build-asan/P-2026-08-02-psp-json-patch-writer-v015
```

All three builds pass cleanly. **31 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

Cross-check the updated Jul 24 consumer against v0.15.0:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_serialiser -B late-may/cpp_practice/psp_json_patch_serialiser/build-v015 -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_patch_serialiser/build-v015
./late-may/cpp_practice/psp_json_patch_serialiser/build-v015/P-2026-07-24-psp-json-patch-serialiser
```

Updated consumer (with the local writer removed) builds and
runs cleanly against v0.15.0 with **zero** `FAIL:` lines.

Cross-check the unchanged Jul 27 consumer against v0.15.0:

```sh
cmake -S late-may/cpp_practice/psp_json_v014_promotion -B late-may/cpp_practice/psp_json_v014_promotion/build-v015 -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_v014_promotion/build-v015
./late-may/cpp_practice/psp_json_v014_promotion/build-v015/P-2026-07-27-psp-json-v014-promotion
```

Unchanged Jul 27 consumer (its inlined `op_writer` is a
different symbol name; no collision) builds and runs cleanly
against v0.15.0 with **54/54** PASS.

## Where we go next

Today's lesson closes the **v0.15.0 promotion arc** — the
library is at v0.15.0 and the writer is in the library proper.
The Jul 24 consumer was updated in lockstep; the Jul 27
consumer's `op_writer` continues to work unchanged. The
**full ops round-trip is now library-proper**:

```
      build ops in memory
            |
            v
   serialise_patch_document         (v0.15.0; <psp_span/json_ext.h>)
            |
            v
   RFC 6902 §3 wire-format text
            |
            v
   parse_patch_document              (v0.13.0; <psp_span/json_ext.h>)
            |
            v
   std::vector<JsonPatchOp>          (the same one we built)
            |
            v
   psp::json_patch::patch            (v0.12.0; <psp_span/json_ext.h>)
            |
            v
   mutated JsonValue tree
            |
            v
   psp::json_to_string               (v0.10.0; <psp_span/json.h>)
            |
            v
   JSON text
```

The full pipeline is now: **build → serialise → parse →
patch → json_to_string** (and the symmetric direction), all
through the library proper.

The remaining v0.15.0 candidates (re-quoting from the Aug 1
"v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer arc
  the Jul 21 lesson opened ("JSON Pointer → JSON Patch →
  JSON Schema"). The natural next step; uses today's
  `serialise_patch_document` + `parse_patch_document` +
  `patch` together.
- **Streaming patch parser** — the v0.13.0
  `parse_patch_document` reads a full `string_view`; a
  streaming variant over `Span<const char>` would close
  the cursor-primitive gap in the RFC 6902 layer.
- **Transactional Patch** —
  `std::expected<void, JsonPatchError>`-returning engine
  that pre-computes all ops' effects before mutating,
  rolling back on any failure.
- **Widen the dispatcher's int64-vs-double preservation
  guard** from `int` to `int64_t` — orthogonal to today's
  lesson (which routes integer literals through
  `parse_int_at`); relevant if a real consumer hits a
  double-shaped int64-range input.

### Cross-cutting forward-on (re-quoting still-open items)

- **Pin actions to commit SHAs** — `@v4` is a floating
  tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.
- **Re-authorize the `workflow` PAT scope** — pending
  Adam.
- **`std::expected` and coroutines**.
- **`std::submdspan`** (P2630).
- **`aligned_accessor` / `atomic_accessor`** (C++26).
- **C++26 `std::linalg`** (P1673).
- **A `std::expected<JsonValue, ParseError>` ->
  `std::generator` adapter**.

For the library as a whole, today's lesson is the
**canonical closing entry** for the v0.15.0 promotion arc
that opened with the Jul 24 wire-format writer and its
"Promoting the writer to the header (as a future v0.14.0)
is a near-mechanical follow-on once the design is proven
in a consumer" forward-on. v0.15.0 is a strict superset of
v0.14.0: every input v0.14.0 accepted is still accepted (with
the same return value); every operation v0.14.0 could apply
is still applicable. The only API change is the **addition**
of `psp::json_patch::serialise_patch_document` — a
non-breaking change (existing v0.14.0 consumers continue to
compile and run against v0.15.0 with no source changes,
**except** for consumers that defined a local copy of the
function under the same name in the same namespace; the
one such consumer, the Jul 24 `psp_json_patch_serialiser`,
was updated today).
