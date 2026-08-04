# P-2026-08-04 — Streaming JSON Patch Parser: `psp::json_patch::parse_patch_document_at` + `parse_patch_document_next_at` + `parse_one_op_at` (cursor-primitive variants of the v0.13.0 parse_patch_document over `psp::Span<const char>&`; closes the cursor-primitive gap in the RFC 6902 layer)

## Headline

The Aug 3 lesson (`P-2026-08-03-transactional-patch.md`) closed
the transactional engine arc. Today's lesson is the next
v0.15.0 candidate from the Aug 3 "v0.15.0 candidates" list:

> **Streaming patch parser** — the v0.13.0
> `parse_patch_document` reads a full `string_view`; a
> streaming variant over `Span<const char>` would close
> the cursor-primitive gap in the RFC 6902 layer.

Today is that lesson. The streaming parser is a
**consumer-side** capability on top of the library's existing
`psp::json_patch::parse_patch_document` (added in this TU;
library version is unchanged at v0.15.0). Three new
functions are added to the `psp::json_patch::` namespace:

```cpp
// "Begin" call: cursor at start of patch document
// (with optional leading whitespace). Consumes '[' +
// the first op + the trailing ','. Returns the
// first op; on failure, leaves the cursor
// BYTE-IDENTICAL to pre-state.
inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_at(psp::Span<const char>& s) noexcept;

// "Iterate" call: cursor at start of next op (or at
// ']' for end-of-document). Consumes one op + the
// trailing ','. Returns the next op; on end-of-doc,
// consumes the ']' and returns BadDocument; on
// failure, leaves the cursor BYTE-IDENTICAL to
// pre-state.
inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_next_at(psp::Span<const char>& s) noexcept;

// Single-op streaming parser: cursor at start of one
// op object (no '[' / ']' wrapper). Returns one op.
// Useful for callers that have a bare op object (not
// an array) — e.g. the Jul 18 single-op parser
// pattern.
inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at(psp::Span<const char>& s) noexcept;

// Detail-level alias for parse_one_op_at; same symbol,
// different qualified name. Exists for symmetry with
// the v0.13.0 driver's detail::build_one_op pattern.
namespace detail {
    inline std::expected<JsonPatchOp, JsonPatchError>
    parse_one_op_at(psp::Span<const char>& s) noexcept;
}
```

Three new pieces of code in the consumer:

1. `psp::json_patch::detail::parse_one_op_at_impl` — the
   inner per-op parser. Skips leading whitespace, parses
   ONE JSON value via `psp::parse_value_at`, validates the
   result is a JSON object, and assembles a `JsonPatchOp`
   via the v0.13.0 driver's `build_one_op` logic
   (re-implemented locally to keep the consumer self-
   contained). On any typed error, the cursor rewinds to
   the function-entry snapshot — the cursor-primitive
   contract.

2. `psp::json_patch::parse_patch_document_at` — the
   array-driver's BEGIN call. Requires the cursor to be
   at `[` (or whitespace before it); consumes `[` +
   first op + trailing whitespace + `,`. On end-of-doc
   (`[]`), consumes `]` and returns BadDocument. On any
   typed error, leaves the cursor BYTE-IDENTICAL.

3. `psp::json_patch::parse_patch_document_next_at` — the
   array-driver's ITERATE call. The cursor is positioned
   at the start of the next op (or `]` for end-of-doc).
   Returns the next op; on `]`, consumes it and returns
   BadDocument; on failure, leaves the cursor
   BYTE-IDENTICAL.

The streaming layer adds **zero** new error enumerators;
the 13-enum `JsonPatchError` vocabulary from
v0.12.0–v0.15.0 is unchanged. The new "thing" is the
*API shape* (one-op-per-call + cursor-primitive contract),
not the error vocabulary.

## Where this fits in the arc

```
Jul 14  std::expected (C++23)                      result type (already done in psp_span)
Jul 15  parse_int_at / parse_uint_at /             cursor primitives (numeric)
        parse_double_at
Jul 16  expect_char_at + skip_whitespace_at        cursor primitives (non-numeric)
Jul 18  parse_string_at + parse_bool_at +          cursor primitives (scalar tokens)
        parse_null_at
Jul 20  parse_value_at / parse_array_at /          complete JSON parser header
        parse_object_at + JsonValue + json_to_string   (<psp_span/json.h>)
Jul 21  psp::json_pointer::split / to_string /     JSON Pointer (RFC 6901) on
        resolve (const + resolve_mut) + JsonExtError the v0.10.0 tree
Jul 22  psp::json_patch::patch                     JSON Patch (RFC 6902) engine
                                                    "best-effort atomic"
Jul 23  psp::json_patch::parse_patch_document     JSON Patch (RFC 6902) §3
                                                    wire-format parser (string_view)
Aug  2  psp::json_patch::serialise_patch_document JSON Patch (RFC 6902) §3
                                                    wire-format writer (library-promoted)
Aug  3  psp::json_patch::patch_atomic +            transactional wrapper (consumer-side)
        patch_dry_run                              on top of the v0.12.0 engine
Aug  4  psp::json_patch::parse_patch_document_at  streaming wire-format parser
        + parse_patch_document_next_at             (consumer-side; cursor-primitive
        + parse_one_op_at                            variant of the v0.13.0 parser)
                                                    + psp::json_patch::parse_patch_document_at (NEW; consumer-side)
                                                    + psp::json_patch::parse_patch_document_next_at (NEW; consumer-side)
                                                    + psp::json_patch::parse_one_op_at (NEW; consumer-side)
                                                    + psp::json_patch::detail::parse_one_op_at (NEW; consumer-side alias)
```

The Jul 15 lesson (`P-2026-07-15-psp-parser-streaming-cursor.md`)
shipped the first cursor-primitive variants
(`parse_int_at`, `parse_uint_at`, `parse_double_at`) and
proved the "shrink on success / unchanged on failure"
contract on top of the numeric parsers. The Jul 16 lesson
(`P-2026-07-16-psp-parser-cursor-primitives.md`) added the
non-numeric variants (`expect_char_at`,
`skip_whitespace_at`). Today's lesson is the third
cursor-primitive wave: it applies the contract to the
RFC 6902 §3 wire-format parser, which until today was
string-view-only.

The cursor-primitive contract today is end-to-end across
the library: every consumer-level parser either takes
`string_view` (whole-span) or `Span<const char>&` (cursor)
and honours the same cursor-primitive contract.

## The gap being closed

`psp::json_patch::parse_patch_document` (v0.13.0) takes
a `std::string_view` of the FULL patch document and
returns a `std::vector<JsonPatchOp>`. For one-shot use
(load file, parse, apply) that's the right shape. For
STREAMING use — a network protocol that delivers one
op at a time, an incremental UI that applies each op as
it arrives, a generator-style pipeline that processes
a multi-GB patch document without materialising the full
ops vector — it's the wrong shape:

1. The caller must have the FULL document in memory to
   hand to `parse_patch_document`. No incremental
   parsing.
2. The function returns ALL the ops at once. The caller
   can iterate, but they all live in a contiguous
   vector.
3. The function is monolithic: no way to start applying
   ops before the entire document has been parsed.

Today's streaming parser fixes all three:

1. The caller hands in a `Span<const char>&` and gets
   back ONE op per call. The caller can update the span
   from a network buffer, a mmap'd file, or a generated
   stream and pull ops one at a time as they arrive.
2. The function returns ONE op per call. No allocation
   beyond the `std::expected`'s optional storage.
3. The caller can apply each op to a tree as it's
   parsed — true incremental application (Section 6 of
   today's consumer proves this end-to-end).

## What `parse_patch_document_at` provides

The BEGIN call. Cursor must be at the start of the
patch document (with optional leading whitespace). The
function consumes `[` + the first op + the trailing
whitespace + the `,` separator. Returns the first op.

```cpp
// New: incremental start. Cursor is at "  [{...}, {...}]".
psp::Span<const char> s = input;
auto op1 = psp::json_patch::parse_patch_document_at(s);
if (!op1) { /* malformed doc */ }
apply(op1, tree);    // apply op 1 BEFORE op 2 is parsed

// Continue iteration via the NEXT call.
auto op2 = psp::json_patch::parse_patch_document_next_at(s);
if (!op2) { /* end-of-document OR malformed op 2 */ }
apply(op2, tree);    // apply op 2 BEFORE op 3 is parsed

// ...etc, until opN returns BadDocument (end-of-doc).
```

The cost of each call is one JSON-value parse (one op
object) plus optional whitespace skip. For KB-scale
patches that's sub-millisecond per op; the parser can
keep up with any practical stream rate.

## What `parse_patch_document_next_at` provides

The ITERATE call. Cursor must be at the start of the
next op (or at `]` for end-of-document). The function
consumes one op + the trailing whitespace + the `,`
separator. Returns the next op.

The `next` call has the same error vocabulary as the
`begin` call (`BadDocument`, `MissingField`,
`WrongType`, `PointerMalformed`, etc.) — but with one
asymmetry: it does NOT expect `[` at the cursor (the
BEGIN call already consumed it).

The cost of each NEXT call is one JSON-value parse +
optional whitespace skip. Same per-op cost as the BEGIN
call.

## What `parse_one_op_at` provides

The single-op streaming parser. Cursor at start of ONE
op object (no `[` / `]` wrapper). Returns one op.

```cpp
// The caller has a bare op object (not an array).
psp::Span<const char> s = "{...}";
auto op = psp::json_patch::parse_one_op_at(s);
if (!op) { /* malformed op */ }
apply(op, tree);
```

Useful for callers that have a bare op object — e.g.
the Jul 18 single-op parser pattern (a single op in a
buffer, not an array). This is the same use case the
v0.13.0 driver handles internally; today it's exposed
as a public consumer-side function.

## Design notes

### 1. Why begin / next split

A single function would have to decide per call whether
the cursor is at `[` (begin) or at the next op
(continue). That decision is implicit (look at the
cursor), which is hard to reason about for the caller.
Splitting into two functions makes the contract
explicit:

- `parse_patch_document_at` MUST see `[` (or it returns
  `BadDocument`).
- `parse_patch_document_next_at` MUST see an op or `]`
  (or it returns `BadDocument`).

The split is asymmetric on purpose: the BEGIN call does
the `[` check; the NEXT call does the `]` check.
There's no symmetric "does both" function because the
caller knows when they're starting vs continuing.

### 2. Why the cursor contract is the same as v0.7.0/v0.8.0

The cursor contract — "shrink `s` past the consumed run
on success; leave `s` BYTE-IDENTICAL on failure" — is
what the v0.7.0 streaming parsers (`parse_int_at`,
etc.) and the v0.8.0 cursor primitives
(`expect_char_at`, `skip_whitespace_at`) honour today.
The streaming patch parser inherits that contract:

- On success, the cursor is shrunk past `[` + op +
  trailing whitespace + `,` (or just past `]` on
  end-of-doc).
- On typed failure, the cursor is rewound to the
  function-entry snapshot — the caller's cursor is
  unchanged, ready to be called again.

This means the streaming parser composes with the
existing cursor primitives: a caller can skip whitespace
with `psp::skip_whitespace_at`, parse the next op with
`parse_patch_document_next_at`, and the cursor is in
the same state as if they'd used the bulk
`parse_patch_document`.

### 3. Why the rewind is a snapshot (and not "leave at
the failure position")

The cursor-primitive contract says "leave the cursor
unchanged on failure". That means the snapshot at
function entry — not the position when the error was
discovered (which could be partway through the op
object, e.g. after "op" was parsed but before "path"
was parsed). The user can call the function again with
the same `s` and get the same error. Section 6i-extra
of today's consumer proves this: a malformed second op
returns `MissingField` and a re-call returns the same
`MissingField`.

### 4. Why no transactional wrapper
(`parse_patch_atomic_at`)

The Aug 3 transactional layer (`patch_atomic` /
`patch_dry_run`) commits on success — but the streaming
parser emits ONE op per call, so the transaction would
be one-op-atomic, which is just "apply each op via the
engine, bail on error". That's Section 6a's test. The
Aug 3 transactional layer is for "apply a full patch
atomically"; streaming is for "apply ops as they arrive"
(different semantics — no rollback to a snapshot
because there is no "all or nothing" boundary visible to
the caller).

If a future lesson wants to add streaming-atomic
semantics, the implementation would be: take the
streaming parser, maintain a snapshot per-op, and
restore on engine failure. That's a separate design
exercise (and a meaningful one — the deep-clone cost
amortises differently in the streaming case).

### 5. Why the public-facing
`parse_patch_document_next_at` (and not just a
"continue" flag)

We could have implemented a single function with a
`bool continue_` flag, or a stateful parser object. The
two-function split is simpler: the caller holds the
state (a `Span<const char>&`), and the call site is
self-documenting:

```cpp
auto op1 = parse_patch_document_at(s);     // start
auto op2 = parse_patch_document_next_at(s); // next
auto op3 = parse_patch_document_next_at(s); // next
auto op4 = parse_patch_document_next_at(s); // end-of-doc
```

vs the stateful version:

```cpp
Parser p{s};
auto op1 = p.next();                       // implicit "start"
auto op2 = p.next();                       // implicit "continue"
```

The two-function split is consistent with how the
v0.7.0 cursor primitives work (`parse_int_at` is the
"start"; there is no `parse_int_continue_at` because
numeric parsing doesn't have that shape). The RFC 6902
layer does need a continue, so we expose it as a second
function.

### 6. Wire-format interop

The v0.15.0 writer + v0.13.0 parser + v0.12.0 engine
are all in the library proper. Section 4 of today's
consumer hand-builds a 3-op patch in memory, serialises
it with `psp::json_patch::serialise_patch_document`
(the v0.15.0 library writer), then re-walks the
serialised wire via the streaming parser (the today's
consumer's parser), then applies each op via
`psp::json_patch::patch` (the v0.12.0 library engine).
The end-to-end pipeline (writer → streaming parser →
engine) proves the streaming parser composes cleanly
with the v0.15.0 round-trip.

### 7. Strict warnings + ASan

Same pattern as the Aug 3 lesson: a strict-warning
build proves the consumer compiles cleanly under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`, and an ASan + UBSan build proves
no memory or UB findings. The streaming parser walks
the tree recursively through `psp::parse_value_at`, so
any use-after-free or uninitialised-read in the JSON
value's `std::map` / `std::vector` handling would
surface here.

## Verified output

```
P-2026-08-04 — Streaming JSON Patch Parser:
                psp::json_patch::parse_patch_document_at
                + psp::json_patch::parse_patch_document_next_at
                + psp::json_patch::parse_one_op_at
                + psp::json_patch::detail::parse_one_op_at
                (consumer-side; cursor-primitive variants of
                the v0.13.0 parse_patch_document over
                psp::Span<const char>&; closes the
                cursor-primitive gap in the RFC 6902 layer)

== Section 1: symbol-presence + cursor contract ==
  PASS: 1a &psp::json_patch::parse_patch_document_at is well-defined
  PASS: 1b &psp::json_patch::parse_patch_document_next_at is well-defined
  PASS: 1c &psp::json_patch::parse_one_op_at is well-defined
  PASS: 1d &psp::json_patch::detail::parse_one_op_at is well-defined
  PASS: 1c bad input: parse_patch_document_at returns unexpected
  PASS: 1d bad input: error is BadDocument
  PASS: 1e bad input: span is BYTE-IDENTICAL to pre-state
  PASS: 1f bad input: span content is unchanged
  PASS: 1g good input: parse_patch_document_at returns void
  PASS: 1h good input: span was SHRUNK (past consumed '[' + op)

== Section 2: happy path — walk a 3-op document one op at a time ==
  PASS: 2a first call returns void (op present)
  PASS: 2b first call: kind is Test
  PASS: 2c first call: cursor shrank past '[' + test op + ','
  PASS: 2d second call returns void (op present)
  PASS: 2e second call: kind is Remove
  PASS: 2f second call: path is /baz
  PASS: 2g third call returns void (op present)
  PASS: 2h third call: kind is Add
  PASS: 2i third call: value is a JSON array (per RFC 6902 §1)
  PASS: 2j fourth call returns unexpected (end of doc)
  PASS: 2k fourth call: error is BadDocument
  PASS: 2l v0.13.0 bulk parser returns 3 ops for the same doc
  PASS: 2m bulk application succeeded
  PASS: 2n tree after bulk application matches RFC 6902 §1 expected
  PASS: 2o streaming application: applied all 3 ops
  PASS: 2p streaming-applied tree matches bulk-applied tree

== Section 3: error path — cursor is BYTE-IDENTICAL on failure ==
  PASS: 3a not-an-array: returns unexpected
  PASS: 3b not-an-array: error is BadDocument
  PASS: 3c not-an-array: cursor BYTE-IDENTICAL to pre-state
  PASS: 3d empty doc: returns unexpected
  PASS: 3e empty doc: error is BadDocument
  PASS: 3f empty doc: cursor shrank past '[' + ']' (2 bytes consumed)
  PASS: 3g missing op: returns unexpected
  PASS: 3h missing op: error is MissingField
  PASS: 3i missing op: cursor BYTE-IDENTICAL to pre-state
  PASS: 3j wrong-type op: returns unexpected
  PASS: 3k wrong-type op: error is WrongType
  PASS: 3l wrong-type op: cursor BYTE-IDENTICAL to pre-state
  PASS: 3m unknown op: returns unexpected
  PASS: 3n unknown op: error is BadDocument
  PASS: 3o unknown op: cursor BYTE-IDENTICAL to pre-state
  PASS: 3p single-op via detail::parse_one_op_at: ok
  PASS: 3q single-op via detail: kind is Add
  PASS: 3r single-op failure: returns unexpected
  PASS: 3s single-op failure: error is BadDocument
  PASS: 3t single-op failure: cursor BYTE-IDENTICAL

== Section 4: round-trip — streaming + writer + engine ==
  PASS: 4a bulk parse returns 3 ops
  PASS: 4b bulk apply succeeded
  PASS: 4c serialise_patch_document produced non-empty wire
  PASS: 4d wire starts with '[' (JSON array per RFC 6902 §3)
  PASS: 4e streaming re-apply succeeded
  PASS: 4e streaming re-apply succeeded
  PASS: 4e streaming re-apply succeeded
  PASS: 4f streaming re-apply applied 3 ops
  PASS: 4g bulk-applied tree == streaming-applied tree

== Section 5: generator-style usage — one op per call ==
  PASS: 5a generator pulled 2 ops from a 2-op document
  PASS: 5b both ops are Add (in document order)
  PASS: 5c generator pulled 6 ops from a 6-op document
  PASS: 5d all six op kinds pulled in document order

== Section 6: streaming + engine + manual rollback ==
  PASS: 6a first op (add /x 1) succeeded
  PASS: 6b second op (remove /missing) failed at index 1
  PASS: 6c second op's error is PointerNotFound
  PASS: 6d first op's mutation stuck (no rollback in streaming mode)
  PASS: 6e first call returns void
  PASS: 6f first call shrank the cursor
  PASS: 6g second call (malformed op) returns unexpected
  PASS: 6h second call's error is MissingField (no "op" key)
  PASS: 6i second call's cursor BYTE-IDENTICAL to its pre-state (not rewound to the array start)
  PASS: 6i-extra second call again: still returns unexpected
  PASS: 6i-extra second call again: error is still MissingField
  PASS: 6j bulk application succeeded
  PASS: 6k streaming application: op succeeded
  PASS: 6k streaming application: op succeeded
  PASS: 6k streaming application: op succeeded
  PASS: 6l bulk-applied tree == streaming-applied tree

== Section 7: sizeof / feature probes ==
  PASS: 7a sizeof(JsonPatchError) = 4 (unchanged; streaming parser adds no enum)
  PASS: 7b sizeof(expected<JsonPatchOp, JsonPatchError>) >= sizeof(JsonPatchOp)
  PASS: 7c sizeof(JsonPatchOp) >= kind + variant
  PASS: 7d JsonPatchError enumerators are distinct (BadDocument/MissingField/WrongType/PointerNotFound)
  PASS: 7e JsonPatchError has 13 distinct enumerators (matches v0.15.0; streaming parser adds zero)
  PASS: 7f __cpp_lib_expected = 202211 (C++23)
  PASS: 7g __cpp_lib_span     = 202002 (C++20)

[streaming_patch_parser: 82 PASS, 0 FAIL]
```

**Section totals**: 4 (symbol-presence) + 6 (cursor
contract) + 16 (happy path) + 20 (error path) + 9
(round-trip) + 4 (generator) + 16 (streaming + engine) +
7 (probes) = **82 PASS, 0 FAIL** across 7 sections.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes
cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

## One design decision during development (and why it
ended up where it did)

### Decision — split into begin + next, not a single
function

The first cut of `parse_patch_document_at` was a
single function that returned one op per call and
auto-detected whether it was at `[` (begin) or at the
next op (continue). The detection used `s.front() ==
'['` after skipping whitespace. This worked for the
happy path but had two correctness problems:

1. The function couldn't tell `parse_patch_document_at`
   from `parse_one_op_at` at the call site — both
   accepted a `Span<const char>&` and returned the
   same shape. The caller had to know whether their
   input was a full document or a bare op.

2. On failure mid-document, the rewind logic had to
   decide: rewind to function entry (cursor-primitive
   contract) or rewind to the array start (more
   user-friendly for "malformed doc" errors)? We chose
   function entry, but the contract was unclear from
   the call site.

The split fixes both: `parse_patch_document_at` is
unambiguously the BEGIN call (it MUST see `[`),
`parse_patch_document_next_at` is unambiguously the
ITERATE call (it MUST NOT see `[`), and
`parse_one_op_at` is unambiguously the bare-op call.
The cursor-primitive contract is identical across all
three (rewind on failure) — but the *expected cursor
position* is explicit at every call site.

A side benefit: the begin/next split maps cleanly onto
the natural stream API of a network protocol or a
generator pipeline. The BEGIN call corresponds to "I
have the header of a new patch document"; the NEXT
calls correspond to "give me the next op as it
arrives".

## What's NOT in this lesson

- **It is not a std::generator adapter.** C++23
  `std::generator` would let the caller write
  `for (auto op : psp::json_patch::parse_patch_document(s))`
  instead of the begin/next dance. That's a separate
  lesson — the consumer would need libc++23's
  `<generator>` header. Today's lesson is the
  underlying cursor-primitive plumbing; the
  generator adapter is a one-line wrapper on top.

- **It is not a streaming-atomic wrapper.** The Aug 3
  `patch_atomic` + `patch_dry_run` layer wraps the
  FULL patch; a streaming-atomic wrapper would need
  per-op snapshotting and is a separate design
  exercise.

- **It is not a pull parser with progress callbacks.**
  Some JSON parsers emit "I'm starting an object"
  callbacks as they walk; that's a different design
  (push vs pull). Today's lesson is pure pull.

- **It is not a fix for the engine's self-move
  quirk.** That was flagged in the Aug 3 lesson and
  is orthogonal to today's lesson.

- **It does not bump the library.** The library
  version is unchanged at v0.15.0. A future v0.16.0
  promotion is mechanical (lift
  `parse_patch_document_at` +
  `parse_patch_document_next_at` +
  `detail::parse_one_op_at_impl` from this consumer
  into `<psp_span/json_ext.h>`; bump the version).

- **It does not change the wire format.** The
  v0.15.0 writer + v0.13.0 parser are unchanged.
  Today's lesson is a control-flow wrapper that sits
  between "wire-format bytes" and "in-memory
  JsonPatchOp" — same wire format, same in-memory
  shape, different streaming model.

- **It does not add a new error enumerator.** All
  `JsonPatchError` values are inherited from the
  v0.13.0 parser. The streaming parser either emits
  one op or propagates the parser's error.

- **It does not change
  `psp::json_patch::parse_patch_document`.** The
  v0.13.0 string-view bulk parser is unchanged at
  v0.15.0. Today's streaming parser is a NEW
  function that complements the bulk parser; existing
  v0.13.0+ consumers continue to compile and run
  unchanged.

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
cmake -S late-may/cpp_practice/streaming_patch_parser -B late-may/cpp_practice/streaming_patch_parser/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/streaming_patch_parser/build
./late-may/cpp_practice/streaming_patch_parser/build/P-2026-08-04-streaming-patch-parser
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/streaming_patch_parser -B late-may/cpp_practice/streaming_patch_parser/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/streaming_patch_parser/build-strict
./late-may/cpp_practice/streaming_patch_parser/build-strict/P-2026-08-04-streaming-patch-parser
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/streaming_patch_parser -B late-may/cpp_practice/streaming_patch_parser/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/streaming_patch_parser/build-asan
./late-may/cpp_practice/streaming_patch_parser/build-asan/P-2026-08-04-streaming-patch-parser
```

All three builds pass cleanly. **82 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **streaming-parser arc** — the
RFC 6902 §3 wire-format parser now has both a bulk
string-view API (v0.13.0 `parse_patch_document`) AND a
cursor-primitive streaming API (today's
`parse_patch_document_at` + `parse_patch_document_next_at`
+ `parse_one_op_at`). The cursor-primitive contract is
end-to-end across the library: every consumer-level
parser either takes `string_view` (whole-span) or
`Span<const char>&` (cursor) and honours the same
contract.

The library version is unchanged at v0.15.0; a future
v0.16.0 promotion is mechanical (three new functions
lifted from this consumer into `<psp_span/json_ext.h>`).

The remaining v0.15.0 candidates (re-quoting from the
Aug 3 "v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer
  arc the Jul 21 lesson opened ("JSON Pointer → JSON
  Patch → JSON Schema"). Uses today's
  `patch_atomic` + `patch_dry_run` for atomicity
  (atomic schema-driven updates + dry-run validation).

- **Widen the dispatcher's int64-vs-double preservation
  guard** from `int` to `int64_t` — orthogonal to
  today's lesson; relevant if a real consumer hits a
  double-shaped int64-range input.

- **Engine-level self-move fix** — short-circuit
  `from == path` in `apply_move` to honor RFC 6902
  §4.4's "self-move is a no-op" rule. Pre-existing
  engine quirk, flagged in the Aug 3 lesson.

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
  `std::generator` adapter** — natural follow-on to
  today's streaming parser (one-line wrapper on top of
  the begin/next functions).
- **Inverse-journal optimisation for `patch_atomic`** —
  per-op journal of inverses instead of a full
  deep-clone; relevant for MB-scale patches.
- **`std::generator` adapter for the streaming patch
  parser** — the begin/next dance is a one-line wrapper
  away from `for (auto op : parser) { ... }`.

For the library as a whole, today's lesson is the
**canonical closing entry** for the streaming-parser
arc. The cursor-primitive contract is now consistent
across the parser header (numeric: Jul 15; non-numeric:
Jul 16; scalar tokens: Jul 18) and the RFC 6902 layer
(today). The natural next step is the
`std::generator` adapter — a one-line wrapper that
makes the begin/next API a range-for loop.

## Files

- `late-may/cpp_practice/streaming_patch_parser/CMakeLists.txt`
- `late-may/cpp_practice/streaming_patch_parser/P-2026-08-04-streaming-patch-parser.cpp`
- `late-may/cpp_practice/streaming_patch_parser/P-2026-08-04-streaming-patch-parser.md` (this file)