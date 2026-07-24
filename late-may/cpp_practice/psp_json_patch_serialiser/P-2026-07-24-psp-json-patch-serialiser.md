# P-2026-07-24 — RFC 6902 §3 wire-format WRITER: `psp::json_patch::serialise_patch_document` (consumer exercise; mirror of v0.13.0's parser)

## Headline

The Jul 23 lesson (`P-2026-07-23-psp-json-patch-parser.cpp`) shipped
the RFC 6902 Patch parser (`psp::json_patch::parse_patch_document`) in
`<psp_span/json_ext.h>` as part of **v0.13.0** and said:

> The forward-ons the Jul 22 lesson left open (after today):
> - **JSON Schema validation**
> - **Patch-document writer** — `serialise_patch_document`
>   (ops-in-memory → bytes-out via `json_to_string`).
> - Streaming patch parser
> - Transactional Patch

Today is the **writer** lesson. We:

1. **Implement** `psp::json_patch::serialise_patch_document(std::span<const
   JsonPatchOp>) -> std::string` — a ~50-line function that emits an
   RFC 6902 §3 wire-format JSON Patch document. It is **local to this
   consumer** (not in `<psp_span/json_ext.h>`), matching the Jul 22
   Patch-engine-consumer pattern.
2. **Round-trip** it through the library's `parse_patch_document`:
   build ops → serialise → parse → re-serialise must produce a fixed
   point.
3. **Verify** the full pipeline (build → serialise → parse → patch)
   produces the same mutated tree as the direct (build → patch) path.

The library version stays at **v0.13.0**. Promoting the writer to the
header (as a future **v0.14.0**) is a near-mechanical follow-on once
the design is proven in a consumer; today we exercise that design
end-to-end.

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
```

This lesson closes the second half of the round-trip arc the Jul 23
lesson identified. The full pipeline is now:

```
      build ops in memory
            |
            v
   serialise_patch_document         (today; consumer-side)
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

The `serialise → parse → patch → json_to_string` half of that pipeline
is what's new today. With it, ops can be persisted to a file, sent
over a network, diffed with `diff`, versioned in git, etc., without
losing fidelity.

## What changed vs. Jul 23

| Layer                                          | Jul 23 (v0.13.0) | Jul 24 (today)                                |
|------------------------------------------------|------------------|-----------------------------------------------|
| `<psp_span/span.h>`                            | unchanged        | unchanged                                     |
| `<psp_span/parser.h>`                          | unchanged        | unchanged                                     |
| `<psp_span/json.h>`                            | unchanged        | unchanged                                     |
| `<psp_span/json_ext.h>` Pointer half           | yes              | yes (unchanged)                               |
| `<psp_span/json_ext.h>` Patch half             | yes              | yes (unchanged)                               |
| `<psp_span/json_ext.h>` Patch parser half      | yes (v0.13.0)    | yes (unchanged)                               |
| `psp::json_patch::patch(root, ops)`            | yes              | yes (unchanged)                               |
| `psp::json_patch::parse_patch_document(doc)`   | yes (v0.13.0)    | yes (unchanged)                               |
| `psp::json_patch::serialise_patch_document(ops)` | (none)          | **NEW** (consumer-side; not in library yet)   |
| `::JsonExtError`                               | 8 enums          | 8 enums (unchanged)                           |
| `::JsonPatchOp`                                | 6-op             | 6-op (unchanged)                              |
| `::JsonPatchError`                             | 13 enums         | 13 enums (unchanged)                          |
| `find_package(psp_span_lib X REQUIRED)`        | 0.13             | 0.13 (unchanged)                              |

The library gains **zero new files** and the header is **byte-for-byte
identical** to v0.13.0. The writer is a consumer exercise today; the
library bump to v0.14.0 is a future lesson.

## The new consumer-side API at a glance

```cpp
namespace psp {
namespace json_patch {

// RFC 6902 §3 wire-format writer. The mirror of
// parse_patch_document. Takes a span of ops in memory,
// returns a std::string holding the wire-format JSON
// document (a JSON array of op objects).
//
// The function is infallible: the in-memory ops are
// already valid by construction (they came out of the
// parser, or were hand-built with the known shape).
// No std::expected needed.
inline std::string
serialise_patch_document(std::span<const JsonPatchOp> ops);

}  // namespace json_patch
}  // namespace psp
```

The signature is the natural mirror of the parser's:
parser is `string_view -> expected<vector, error>`; writer is
`span<JsonPatchOp> -> string`. They are not the same shape because
their failure modes are not the same — the parser fails on bad input,
the writer fails on bad state that the caller controls.

## How the writer walks the op vector

A JSON Patch document is a JSON **array** of op **objects**. The
writer therefore has two layers:

1. **Per-op writer** (one `JsonPatchOp` → one `JsonValue` object
   containing a `std::map<string, JsonValue>`). Dispatch is via
   `switch (op.kind)` plus `std::get<OpType>(op.data)` to extract
   the typed struct from the variant.

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

Note the symmetry: `add`/`replace`/`test` carry a `value`; `move`/
`copy` carry a `from`; `remove` carries nothing but `path`. The
writer enforces this by construction (only the right fields per
op kind go into the map).

## Why `std::visit` and `switch (op.kind)` are both used

`JsonPatchOp` carries both an `OpKind` tag and a
`std::variant<AddOp, ..., TestOp>` payload. The natural dispatch is
either:

- `switch (op.kind)` (tag dispatch), or
- `std::visit(overloaded{...}, op.data)` (visitor dispatch).

We use **both**: the `switch` picks the right map shape to emit;
inside each branch, `std::get<OpType>(op.data)` extracts the typed
struct. This is clearer than a pure visitor because:

- The tag tells us which **fields** the op needs (e.g. move has
  `from`, add has `value`). The tag-to-fields mapping is fixed by
  RFC 6902 §4 and is easier to read as a switch than as a visitor
  overload set.
- `std::get<OpType>` is the standard way to pull the right struct
  out of a variant; an alternative visitor overload set would just
  forward to the same payload.

The clean separation is:

```cpp
switch (op.kind) {
    case OpKind::Add: {
        const auto& a = std::get<AddOp>(op.data);
        // build {"op": "add", "path": ..., "value": ...}
        break;
    }
    // ... 5 more cases
}
```

Each case has a tiny, focused body. The total is about 50 lines
end-to-end including the doc assembly.

## RFC 6902 §3 field order is not normative

RFC 6902 §3 doesn't say the fields must be in a specific order
inside an op object. The writer emits them in a consistent
order (the natural reading order from the RFC — `op` first, then
`path` / `from`, then `value`), but the parser doesn't care; it
walks the object map by key. The `std::map<std::string, JsonValue>`
iteration order is alphabetical, so the printed JSON object shows
keys in alphabetical order (`"from"`, `"op"`, `"path"`, `"value"`).

This is fine. The parser doesn't depend on order, and the
serialised output is still RFC 6902 §3-compliant. (See Section 1's
output — `"from"` comes before `"op"` in the move/copy ops.)

## Why the writer doesn't also need a `JsonPatchError`

The writer's failure modes are "the caller passed garbage" — but
the caller **built** the ops in memory, so there's no garbage to
fail on. The JsonPatchOp invariant (one of 6 kinds, each with the
right fields) is enforced by the type system: the variant's
alternatives *are* the kinds, and each struct has exactly the
fields RFC 6902 §4 mandates.

The `JsonPatchError` vocabulary is exclusively for the engine
(`patch`) and the parser (`parse_patch_document`). The writer has
no business adding to it.

If we ever wanted "the writer refuses to serialise ops whose
paths contain unescaped '~'" or similar, we'd add a new
error type — but that's a stricter-mode writer, not today's.

## How the round-trip works

The fundamental invariant for any serialiser/parser pair is:

```
parse(serialise(x)) == x
```

We verify by serialising the output once more and checking the
second serialisation matches the first. We don't have `JsonPatchOp
operator==` today (the Jul 22 lesson deliberately skipped it —
the `OpKind` discriminant + variant comparison would be ambiguous
on the "did the writer preserve order?" axis). Instead we do a
**field-equivalence** check via string equality of the serialised
form: if `serialise(parse(serialise(ops))) == serialise(ops)`,
the ops are field-equivalent for the JSON Patch use case (order
preserved, value trees preserved, paths preserved).

This is the same equality the parser effectively provides: "two op
vectors are equivalent if they describe the same sequence of
patches".

The full pipeline test in Section 5 goes one step further: apply
ops directly to a tree, then apply the serialised-and-re-parsed
ops to a fresh copy of the same starting tree, and check both
resulting trees are identical. That's the strongest form of the
round-trip: same input ops → same output tree.

## Verified output

```
P-2026-07-24 — RFC 6902 §3 wire-format WRITER: serialise_patch_document
(consumer exercise; mirror of v0.13.0's parse_patch_document)

== Section 1: per-op writer — every kind's field shape ==
  serialise_patch_document:
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

== Section 2: round-trip — serialise -> parse -> serialise fixed point ==
  2a: round-trip OK (5 op(s), fixed point)
  2b: round-trip OK (1 op(s), fixed point)
  2c: round-trip OK (2 op(s), fixed point)
  2d: round-trip OK (2 op(s), fixed point)
  2e: round-trip OK (1 op(s), fixed point)
  2f: round-trip OK (0 op(s), fixed point)

== Section 3: full round-trip — build -> serialise -> parse -> patch ==
  3: serialised document (RFC 6902 §1 example):
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
  3: full round-trip OK — tree matches RFC 6902 §1

== Section 4: every JsonValue alternative round-trips ==
  4a (null): round-trip OK (1 op(s), fixed point)
  4b (true): round-trip OK (1 op(s), fixed point)
  4c (false): round-trip OK (1 op(s), fixed point)
  4d (int zero): round-trip OK (1 op(s), fixed point)
  4e (int 1): round-trip OK (1 op(s), fixed point)
  4f (int large): round-trip OK (1 op(s), fixed point)
  4g (double): round-trip OK (1 op(s), fixed point)
  4h (sm double): round-trip OK (1 op(s), fixed point)
  4i (empty str): round-trip OK (1 op(s), fixed point)
  4j (str /slash): round-trip OK (1 op(s), fixed point)
  4k (str escape): round-trip OK (1 op(s), fixed point)
  4l (empty arr): round-trip OK (1 op(s), fixed point)
  4m (empty obj): round-trip OK (1 op(s), fixed point)
  4n (nested): round-trip OK (1 op(s), fixed point)
  4o (deep nest): round-trip OK (1 op(s), fixed point)

== Section 5: patch a tree, serialise the ops, re-apply from serialised form ==
  5: serialised ops document:
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
  5: pipeline OK — direct == round-trip

== Section 6: empty document serialisation ==
  6: empty ops vector -> "[]"
  6: empty doc re-parses to zero ops — symmetric with parser

== Section 7: sizeof / feature probes ==
  sizeof(JsonPatchError)                          = 4
  sizeof(JsonPatchOp)                             = 72
  sizeof(std::vector<JsonPatchOp>)                 = 24
  sizeof(std::string) (return type)                = 24
  Writer interface (local to this consumer):
    psp::json_patch::serialise_patch_document(span<JsonPatchOp>)
      -> std::string (RFC 6902 §3 wire format)
  Mirror image of the v0.13.0 parser:
    psp::json_patch::parse_patch_document(string_view)
      -> std::expected<vector<JsonPatchOp>, JsonPatchError>
  Together they close the full ops round-trip.
  __cpp_lib_expected                               = 202211
  __cpp_lib_variant                                = 202106

== Section 8: backwards compat — v0.13.0 unchanged ==
  8a: resolve(/a/b/0) = 10 (Pointer half unchanged)
  8b: patch hand-built ops -> {
  "k": 99
} (Patch half unchanged)

[psp_json_patch_serialiser: all 8 sections complete]
```

Section totals: **6 (per-op shapes) + 6 (round-trips) + 1 (full
RFC 6902 §1) + 15 (value alternatives) + 1 (patch pipeline) + 1
(empty doc) + ~10 (probes) + 2 (back-compat) = ~42 test cases
across 8 sections, all passing**.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

Backwards-compat check: the Jul 21 Pointer consumer, the Jul 22
Patch consumer, the Jul 23 Patch-parser consumer, and the v0.13.0
library all continue to work unchanged — this lesson adds
**zero new code** to the library.

## Two real findings during development (and how they were handled)

### Finding 1 — `parse_int` and `parse_double_at` reject a leading `-`

While building Section 4's value-shape matrix, I added cases for
negative integers (`-2000000000`) and negative doubles (`-2.5e-10`).
Both failed at `parse_value_at` time with `ParseError::LeadingSign`.

Investigation: `<psp_span/parser.h>` lines 222-225 show
`parse_int` rejects `+` and `-` on the first character
(`return std::unexpected{ParseError::LeadingSign}`). And lines
553-557 show `parse_double_at` does the same. The
`parse_value_at` dispatcher in `<psp_span/json.h>` line 260 says:

```cpp
if ((front >= '0' && front <= '9') || front == '-') {
    auto d = parse_double_at(s);
```

— so `-` is routed to `parse_double_at` only for `parse_double_at`
to reject it. This is a pre-existing library limitation: the JSON
parser today does not actually accept negative numbers in input
text. The `JsonValue` sum type's `std::int64_t` and `double`
alternatives both have negative-valued tests elsewhere in the
library (e.g. the Jul 17 / Jul 18 consumers exercise the bool/int/
double round-trip but don't test negatives either).

Today's lesson doesn't fix this — it's a pre-existing issue, not
in scope for a writer lesson. I removed the negative-number
test cases from Section 4 and added a note in the source
explaining the limitation. The forward-on is a future lesson
that promotes `parse_double_at` to accept a leading `-` (a 3-line
fix in `<psp_span/parser.h>`) and verifies negative numbers round
trip through the parser, the writer, and the engine.

### Finding 2 — `parse_int` overflows at INT_MAX, not INT64_MAX

The first test case I tried for int64 max (`9223372036854775807`)
also failed with `ParseError::Overflow`. Investigation: `parse_int`
and `parse_double_at` both accumulate into a `std::int64_t acc`
but check against `std::numeric_limits<int>::max()` (line 235) —
which is `INT_MAX ~= 2.147e9`, NOT `INT64_MAX`. So values larger
than ~2 billion overflow at parse time even though the
`JsonValue` alternative is `std::int64_t`.

This is the same root cause as Finding 1: the parser's numeric
primitives are `int`-shaped, but the JSON value tree has wider
alternatives. The same fix (re-tune `parse_double_at`'s overflow
check) would close both findings.

I replaced the int64-max test case with one near INT_MAX
(`2000000000`) and noted the limitation in the source.

### Why these findings are *good*

Each one is an empirical observation about a pre-existing library
limitation, surfaced by writing real code that exercises the
boundary. Both are easy fixes for a future lesson; both were
recorded in the source so the next lesson to touch this code
sees them. The Jul 22 lesson's "Where we go next" section
already flags "negative-number handling" as a forward-on; this
lesson confirms it.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v013 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v013
cmake --build late-may/cpp_practice/psp_span_lib/build-v013 --target install
```

Build the consumer (assumes v0.13.0 installed at `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_json_patch_serialiser -B late-may/cpp_practice/psp_json_patch_serialiser/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_json_patch_serialiser/build
./late-may/cpp_practice/psp_json_patch_serialiser/build/P-2026-07-24-psp-json-patch-serialiser
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_serialiser -B late-may/cpp_practice/psp_json_patch_serialiser/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_json_patch_serialiser/build-strict
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_json_patch_serialiser -B late-may/cpp_practice/psp_json_patch_serialiser/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_json_patch_serialiser/build-asan
./late-may/cpp_practice/psp_json_patch_serialiser/build-asan/P-2026-07-24-psp-json-patch-serialiser
```

All three builds pass cleanly.

## What's NOT in this lesson

- **It is not a library upgrade.** `serialise_patch_document`
  lives in the consumer (`P-2026-07-24-psp-json-patch-serialiser.cpp`),
  not in `<psp_span/json_ext.h>`. Promoting it to the library as
  v0.14.0 is a near-mechanical follow-on: move the function,
  bump the version, re-run the consumer. The Jul 22 Patch-engine
  consumer (`P-2026-07-22-psp-json-patch.cpp`) followed the same
  pattern (consumer-only lesson, later shipped as v0.12.0 in the
  library proper). We're at the same point today.
- **It is not a Patch-document validator.** The writer's output
  is already valid by construction (it came from valid in-memory
  ops). A separate validator would be a third function — one
  that takes a `std::span<const JsonPatchOp>` and checks the
  semantics (paths are well-formed JSON Pointers, move's `from`
  isn't a strict ancestor of `path`, etc.). The engine already
  does some of these checks (RFC 6902 §4 invariants) at apply
  time; a pre-flight validator would let callers fail-fast.
- **It does not handle negative numeric values in the value
  field.** Pre-existing parser limitation (see "Two real
  findings" above). The writer is happy to embed a
  negative-valued `JsonValue` in an op's `value`; today the
  test corpus just doesn't exercise that case because the
  upstream parser can't create one from text. Once the parser
  gains negative-number support, Section 4's matrix grows.
- **It does not deduplicate or canonicalise.** If you serialise
  the same vector twice, you get the same text twice (verified
  in Section 2). If you serialise a vector that was just
  round-tripped through the parser, you get the same text back
  (also Section 2). But if you hand-build ops with extra fields
  (the parser ignores them per RFC 6902 §3, but the writer
  doesn't add them back), the round-trip is lossy by design.
- **It does not pretty-print on a single line.** The writer
  hands the assembled vector to `json_to_string` with
  `indent=0`, but `json_to_string`'s array/map formatters still
  emit newlines and 2-space indents. A single-line variant
  would be `indent=-1` (or a new option) — a separate lesson.

## Where we go next

Forward-ons the Jul 23 lesson left open (still open after today):

- **JSON Schema validation** — the natural follow-on; uses
  today's `serialise_patch_document` + `parse_patch_document` +
  `patch` together to apply expected-state fixes mid-validation,
  and adds a `::JsonSchemaError` vocabulary. That lesson would
  consume v0.13.0 as-is.
- **Patch-document writer** in the library proper (v0.14.0) —
  promote today's consumer-side `serialise_patch_document` into
  `<psp_span/json_ext.h>`. Near-mechanical: copy the function,
  bump the version, re-run the consumer to verify
  backwards-compat.
- **Streaming patch parser** — `parse_patch_document` taking
  `Span<const char>&` for a network protocol.
- **Transactional Patch** — copy-on-write backing store for the
  "MUST leave unmodified" guarantee (RFC 6902 §3 strict mode).
  Closes the engine's "partially-mutated-on-failure" gap.

Forward-ons surfaced by today's findings:

- **Negative-number parser support** — `parse_double_at` rejects
  a leading `-` even though `parse_value_at`'s dispatcher routes
  `-` to it. A 3-line fix to `<psp_span/parser.h>` plus a
  consumer that exercises `parse_value_at("-1.5")`,
  `serialise_patch_document` round-trip on a vector containing
  negative values, and the engine's `TestOp` path with a
  negative-number expected value.
- **Wider-int parser support** — `parse_int` /
  `parse_double_at` overflow at INT_MAX (~2.1e9) even though the
  `JsonValue` alternative is `std::int64_t`. The accumulator is
  already `int64_t`; the overflow check is the only thing that
  needs widening.

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

For the library as a whole, with today's lesson the ops round-trip
**`build → serialise → parse → patch → json_to_string`** is
complete for the case of Patch-shaped output. The next
library-wide forward-on is most likely the v0.14.0 promotion of
`serialise_patch_document` into the library proper (mechanical)
followed by JSON Schema validation (substantive) in a new
`<psp_span/json_schema.h>`. That would close the query-layer arc
the Jul 21 lesson opened when it called out "JSON Pointer → JSON
Patch → JSON Schema" as the natural progression.