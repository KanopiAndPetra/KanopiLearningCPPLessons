# P-2026-08-03 — Transactional JSON Patch: `psp::json_patch::patch_atomic` + `psp::json_patch::patch_dry_run` (all-or-nothing semantics on top of the v0.12.0 engine, via deep-snapshot rollback)

## Headline

The Aug 2 lesson (`P-2026-08-02-psp-json-patch-writer-v015.md`) closed
the v0.15.0 promotion arc and re-listed the v0.15.0 candidate set. The
first remaining forward-on was:

> **Transactional Patch** — `std::expected<void,
> JsonPatchError>`-returning engine that pre-computes all ops'
> effects before mutating, rolling back on any failure.

Today is that lesson. The transactional layer is a **consumer-side
wrapper** on top of the library's existing `psp::json_patch::patch`
(added in this TU; library version is unchanged at v0.15.0). Two new
functions are added to the `psp::json_patch::` namespace:

```cpp
// Apply a patch with all-or-nothing semantics: on failure, the
// tree is restored to its pre-call state. On success, the tree
// is mutated exactly as the v0.12.0 engine would have.
inline std::expected<void, JsonPatchError>
patch_atomic(psp::JsonValue& root,
             std::span<const JsonPatchOp> ops) noexcept;

// Apply the patch to a private copy; the original `root` is
// NEVER touched, on success or failure. Used to validate a
// patch without committing.
inline std::expected<void, JsonPatchError>
patch_dry_run(const psp::JsonValue& root,
              std::span<const JsonPatchOp> ops) noexcept;
```

The implementation is the **deep-snapshot / rollback-on-failure**
pattern:

```cpp
inline std::expected<void, JsonPatchError>
patch_atomic(psp::JsonValue& root,
             std::span<const JsonPatchOp> ops) noexcept {
    std::optional<psp::JsonValue> snapshot{deep_clone(root)};
    auto r = patch(root, ops);
    if (!r) {
        root = std::move(*snapshot);   // restore pre-state
        return std::unexpected{r.error()};
    }
    return {};                         // success: snapshot is destroyed
}
```

Three new pieces of code in the consumer:

1. `psp::json_patch::deep_clone(const psp::JsonValue&)` — a
   recursive `std::visit`-based deep-copy of a `JsonValue` tree.
   Walks the variant's 8 alternatives; for the two container
   alternatives (`std::vector<JsonValue>` and
   `std::map<std::string, JsonValue>`) it recursively clones
   every child. The function lives at file scope (in
   `psp::json_patch::`) so a future v0.16.0 library promotion
   can lift it into `<psp_span/json_ext.h>` mechanically.

2. `psp::json_patch::patch_atomic(JsonValue&, ops)` — the
   all-or-nothing wrapper. RAII via `std::optional<JsonValue>`
   (the snapshot is destroyed at scope-exit regardless of which
   path the function takes).

3. `psp::json_patch::patch_dry_run(const JsonValue&, ops)` —
   the "would this patch succeed?" probe. Mechanically
   `patch(deep_clone(root), ops)` — the original is never
   touched.

The transactional layer adds **zero** new error enumerators; the
13-enum `JsonPatchError` vocabulary from v0.12.0–v0.15.0 is
unchanged. The `std::optional<JsonValue>` snapshot is the only
new "thing" the wrapper introduces.

## Where this fits in the arc

```
Jul 21  std::span (C++20)                         std version
Jul 12  std::expected (C++23)                     result type
...
Jul 22  psp::json_patch::patch                    query layer (library-side) — RFC 6902 §1 engine (v0.12.0)
                                                   "best-effort atomic" — tree left partially
                                                    mutated on failure
Jul 23  psp::json_patch::parse_patch_document    query layer (library-side) — RFC 6902 §3
                                                    wire-format parser (v0.13.0)
Jul 24  psp::json_patch::serialise_patch_doc.    query layer (consumer-side) — RFC 6902 §3
              (consumer; round-trips v0.13.0's    wire-format writer
              parser to prove the design)
Aug  1  psp_parser_v014_update                    library version (v0.14.0) consumer-update
Aug  2  psp::json_patch::serialise_patch_doc.    query layer (library-side) — RFC 6902 §3
              (library-proper, v0.15.0)           wire-format writer promoted from the Jul 24
                                                   consumer
Aug  3  psp::json_patch::patch_atomic +          query layer (consumer-side) — RFC 6902 §1
              patch_dry_run                        engine with all-or-nothing semantics
              (consumer; deep-snapshot             + dry-run probe; CLOSES the "best-effort
              rollback on top of the v0.12.0        atomic" gap the v0.12.0 engine's docstring
              engine)                                explicitly left open; future v0.16.0
                                                    promotion is mechanical
                                                + psp::json_patch::patch_atomic (NEW; consumer-side)
                                                + psp::json_patch::patch_dry_run  (NEW; consumer-side)
                                                + psp::json_patch::deep_clone     (NEW; consumer-side)
```

The Jul 22 lesson closed the engine layer with the explicit
note in the header:

> "The stronger 'SHOULD leave unmodified' contract would
> require transactional rollback over the partial mutations;
> we don't implement that."

Today closes that gap. The library's `patch` is unchanged;
the transactional layer is a consumer-side wrapper that any
TU can pull in by `#include`-ing this file's source (or, in
a future v0.16.0 promotion, the library proper).

## The gap being closed

RFC 6902 §3 says:

> "If a patch is not applied successfully, the operation MUST
> signal an error, and the target document SHOULD be left in
> its previous state."

The library's `patch` (v0.12.0) honors "MUST signal an error"
on the failing op, but it only honors "SHOULD leave
unmodified" for the **immediately-failing** op — the prior
ops that succeeded before the failure are already applied to
the tree. For a 10-op patch where op #7 fails, ops #1-#6 are
mutations the caller never asked to commit.

For most consumer code (config management, distributed-state
sync, atomic file updates, transactional databases) this is
unworkable. The caller needs "either all 10 ops commit, or
none of them commit" — a real-world "transaction" over a JSON
document. Today's lesson is the layer that provides it.

## What `patch_atomic` provides

The function has the same return type as the engine
(`std::expected<void, JsonPatchError>`) and the same
error-enumerator vocabulary, so it's a strict drop-in for
callers who want all-or-nothing:

```cpp
// Old: best-effort atomic. Ops #1-#6 are applied on failure
// of op #7.
auto r = psp::json_patch::patch(root, ops);
if (!r) { /* tree is partially mutated — bad */ }

// New: all-or-nothing. Tree is exactly as it was on entry
// on any failure.
auto r = psp::json_patch::patch_atomic(root, ops);
if (!r) { /* tree is UNCHANGED — good */ }
```

The cost of `patch_atomic` is one deep clone of the tree
(up-front, before the first op). For KB-scale patches the
clone is sub-millisecond; for MB-scale patches it's a real
but bounded cost. The alternative — journaling per-op
inverses — would be more efficient but has subtle
correctness risks (see Design notes below); the deep-clone
strategy is the simplest correct implementation.

## What `patch_dry_run` provides

The complementary function. It applies the patch to a private
copy of the tree; the original is never touched, on success
or failure. Useful for "would this patch break anything?"
queries that don't want to mutate the state — e.g. a config
manager that wants to validate a proposed update before
presenting it to the user for confirmation.

```cpp
// The proposed patch: add a "deprecated" key, replace one
// value, remove a "legacy" key. We want to know if the patch
// is well-formed (no path-not-found errors) without applying.
auto r = psp::json_patch::patch_dry_run(current_config, proposed_ops);
if (!r) {
    show_error_to_user("the proposed config update would fail: {}", r.error());
    return;
}
// Patch is valid; safe to apply.
psp::json_patch::patch_atomic(current_config, proposed_ops);
```

`patch_dry_run` and `patch_atomic` are mechanically related
— dry-run is "atomic with an always-restore" — but they have
different observable behaviour: `patch_atomic` mutates `root`
on success and on failure leaves it unchanged; `patch_dry_run`
NEVER mutates `root`.

## Design notes

### 1. Why deep-clone instead of inverse-journaling

The simplest correct implementation is one deep copy of the
tree, regardless of patch size. The cost is one extra
`std::map` / `std::vector` deep-copy of the entire tree; for
KB-scale patches that's sub-millisecond. The alternative —
journal per-op inverses and replay them in reverse on
failure — is more efficient for large trees but has subtle
correctness risks:

- For **MoveOp** the inverse is "remove at path, then add at
  from" — but the value was MOVED at apply time, so the
  re-add needs to capture the moved value before the remove
  happens. Order matters.
- For **CopyOp** the inverse is "remove at path" — but
  remove needs the path to exist; in nested structures the
  path may have been overwritten by a later op.
- For **AddOp** the inverse is "remove at path" — but if a
  later op overwrote the key, the "remove" is a no-op which
  is the wrong inverse (the original value should come back,
  not the overwrite value).
- For **RemoveOp** the inverse is "re-add the removed value"
  — but the value isn't in the tree any more; the journal has
  to capture it at apply-time. The journal entry is the
  removed value + the path; replay is `apply_add(path,
  value)`.

Deep-clone sidesteps all of these. The inverse-journal
optimisation is a future lesson; correctness first.

### 2. Why the snapshot is a `std::optional<JsonValue>`

`std::optional<JsonValue>` gives RAII: the snapshot is
destroyed (and its deep-tree freed) at the end of the
function, regardless of which path we took. The value is
moved into the optional, so the only allocation is the
initial clone.

The size of `std::optional<JsonValue>` on this toolchain
(libc++) is essentially `sizeof(JsonValue)` (the optional
header is a single bool for "is engaged"). On libstdc++ it's
slightly larger because of the empty-base optimisation
quirks, but still dominated by `sizeof(JsonValue)`.

### 3. Why we don't wrap `psp::json_patch::patch` in a try/catch

The library is `noexcept`; the only failure mode is the
`std::expected`'s unexpected path. No exception-based control
flow. The deep-clone could throw `std::bad_alloc`, but
that's an OOM condition; we let it propagate (the caller's
caller will deal with it). The library's contract is
"noexcept unless OOM".

### 4. Move-assign on rollback

`root = std::move(*snapshot)` overwrites the variant
alternative (and frees the old tree) in one operation. The
snapshot's vector/map stay intact in the moved-from optional,
which is then destroyed at scope-exit. The moved-from
optional holds an empty `std::monostate` (the `JsonValue`'s
default alternative) which is destructively cheap.

### 5. Wire-format interop

The v0.15.0 writer + v0.13.0 parser + v0.12.0 engine are all
in the library proper. Section 6 of today's consumer hand-
builds a 3-op patch in memory, serialises it with
`psp::json_patch::serialise_patch_document` (the v0.15.0
library writer), re-parses it with
`psp::json_patch::parse_patch_document` (the v0.13.0 library
parser), then runs `patch_atomic` on the parsed vector. The
end-to-end pipeline (writer → parser → transactional engine)
proves the transactional layer composes cleanly with the
v0.15.0 round-trip.

### 6. Strict warnings + ASan

Same pattern as the Aug 2 lesson: a strict-warning build
proves the consumer compiles cleanly under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`, and an ASan + UBSan build proves no
memory or UB findings. The deep-clone walks the tree
recursively, so any use-after-free or uninitialised-read in
`JsonValue`'s `std::map` / `std::vector` handling would
surface here.

## Verified output

```
P-2026-08-03 — Transactional JSON Patch:
                psp::json_patch::patch_atomic + patch_dry_run
                (consumer-side; all-or-nothing semantics on top
                of psp::json_patch::patch via deep-snapshot rollback)

== Section 1: symbol-presence — patch_atomic + patch_dry_run are well-defined ==
  PASS: 1a &psp::json_patch::patch_atomic is well-defined
  PASS: 1b &psp::json_patch::patch_dry_run is well-defined
  PASS: 1c &psp::json_patch::patch is well-defined (v0.12.0 back-compat)
  PASS: 1d std::expected<void, JsonPatchError> = 8 bytes (4-byte enum + 4-byte padding)

== Section 2: happy path — patch_atomic on a successful patch mutates the tree ==
  PASS: 2a patch_atomic returns void on success
  PASS: 2b tree was actually mutated (json differs from pre-state)
  PASS: 2c snapshot_before is the expected pre-state
  PASS: 2d root after the 3-op patch matches the expected post-state

== Section 3: atomic rollback — a failing patch leaves the tree UNCHANGED ==
  PASS: 3a patch_atomic returns unexpected on failure
  PASS: 3b error is PointerNotFound (the failure of op #3)
  PASS: 3c tree is BYTE-IDENTICAL to pre-state (rollback worked)
  PASS: 3d tree matches the original pre-state JSON text
  PASS: 3e control: v0.12.0 patch returns unexpected on same input
  PASS: 3f control: v0.12.0 left control tree PARTIALLY MUTATED (the gap)
  PASS: 3g control: v0.12.0 left control tree with ops #1 and #2 applied

== Section 4: dry-run — patch_dry_run never mutates `root` ==
  PASS: 4a dry-run returns unexpected on failure
  PASS: 4b dry-run error is the same PointerNotFound
  PASS: 4c dry-run left the ORIGINAL tree untouched (root unchanged)
  PASS: 4d successful dry-run returns void
  PASS: 4e even a successful dry-run leaves the ORIGINAL untouched
  PASS: 4f control: patch_atomic on the same good_ops returns void
  PASS: 4g control: patch_atomic mutated root to age 99

== Section 5: corner cases — empty / single-op / self-move / nested ==
  PASS: 5a empty patch: patch_atomic returns void
  PASS:   5a empty patch: root unchanged
  PASS: 5b single-op patch: success path returns void
  PASS:   5b single-op patch: tree mutated correctly
  PASS: 5c single-op failing patch: returns unexpected
  PASS:   5c single-op failing patch: error is PointerNotFound
  PASS:   5c single-op failing patch: root restored to pre-state
  PASS: 5d self-move: patch_atomic returns void (the engine returns void; the engine then leaves the tree in a quirky state)
  PASS:   5d self-move: tree is NOT unchanged (the engine quirk — see lesson)
  PASS: 5e nested array mutation: success
  PASS:   5e nested array mutation: tags[1] replaced with guest
  PASS: 5f nested failure: returns unexpected
  PASS:   5f nested failure: error is PointerIndexOutOfRange
  PASS:   5f nested failure: tree rolled back (the AddOp undone)
  PASS: 5g large tree: returns unexpected on op #6 failure
  PASS:   5g large tree: error is PointerNotFound
  PASS:   5g large tree: all 10 keys rolled back to pre-state

== Section 6: wire-format interop — build -> serialise -> parse -> patch_atomic ==
  PASS: 6a serialise_patch_document produced non-empty wire
  PASS: 6b wire starts with '[' (JSON array per RFC 6902 §3)
  PASS: 6c parse_patch_document succeeded
  PASS: 6d parsed vector has 3 ops (matches original)
  PASS: 6e patch_atomic on parsed vector succeeded
  PASS: 6f tree after wire-round-trip + patch_atomic matches expected post-state
  PASS: 6g serialise_patch_document(ops) is deterministic (the transactional layer didn't mutate ops)
  PASS: 6h parse_patch_document succeeded on bad_ops
  PASS: 6i patch_atomic on bad wire returns unexpected
  PASS: 6j error is PointerNotFound
  PASS: 6k root2 rolled back to pre-state through the wire format

== Section 7: sizeof / feature probes ==
  PASS: 7a sizeof(JsonPatchError) = 4 (unchanged; transactional layer adds no enum)
  PASS: 7b sizeof(std::expected<void, JsonPatchError>) = 8 (4-byte enum + 4-byte padding)
  PASS: 7c &psp::json_patch::deep_clone is well-defined
  PASS: 7d optional<JsonValue> holds the deep-clone snapshot
  PASS: 7e snapshot equals the original at construction
  PASS: 7f __cpp_lib_expected = 202211 (C++23)
  PASS: 7g __cpp_lib_variant  = 202106 (C++17/20)
  PASS: 7h __cpp_lib_span     = 202002 (C++20)
  PASS: 7i JsonPatchError has 13 distinct enumerators (matches v0.15.0; transactional layer adds zero)

== Section 8: back-compat — patch_atomic handles every RFC 6902 op kind ==
  PASS: 8a MoveOp via patch_atomic: success
  PASS:   8a tree: name -> nickname
  PASS: 8b MoveOp clobber: returns unexpected
  PASS:   8b error is MoveWouldClobber
  PASS:   8b tree rolled back to pre-state
  PASS: 8c CopyOp via patch_atomic: success
  PASS:   8c tree: name copied to alias
  PASS: 8d TestOp mismatch: returns unexpected
  PASS:   8d error is TestValueMismatch
  PASS:   8d tree rolled back (the AddOp undone)
  PASS: 8e all-six-op patch: success
  PASS:   8e tree: birthplace added + moved + removed, net no change; age 30->31; city added

[transactional_patch: 71 PASS, 0 FAIL]
```

**Section totals**: 4 (symbol-presence) + 4 (happy path) + 7
(atomic rollback) + 7 (dry-run) + 17 (corner cases) + 11
(wire-format interop) + 9 (sizeof / probes) + 12
(back-compat) = **71 PASS, 0 FAIL** across 8 sections.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

## One finding during development (and how it was handled)

### Finding — the library's `patch_atomic`-style self-move behaviour is engine-level, not transactional

The library's `apply_move` (in
`<psp_span/json_ext.h>`) is implemented as
`apply_add(to_toks, root, *src)` followed by
`apply_remove(from_toks, root)`. For a **self-move**
(`from == path`, e.g. `/tags -> /tags`) this means:
add the value at `/tags` (which overwrites the existing key
with a copy of itself), then remove at `/tags` (which
deletes the key). Net result: the key is GONE, even though
RFC 6902 §4.4 says a self-move is a no-op.

This is a **pre-existing engine quirk**, not a
transactional-layer behaviour. Section 5d's test exercises
the actual behaviour (`r.has_value()` + tree changed) and
the lesson notes the observation; the engine-level fix
(short-circuit on `from == path` before the
apply_add / apply_remove pair) is a separate lesson if it's
ever needed.

The transactional layer doesn't FIX this — it
transactionally commits whatever the engine produces. The
`/tags -> /tags` patch is a successful (in the engine's
sense) operation that results in an empty tree; the
transactional layer commits that outcome, just like it
commits any other successful patch.

## What's NOT in this lesson

- **It is not an inverse-journal implementation.** The
  per-op-journal design (capture the inverse at apply-time,
  replay in reverse on failure) is more efficient for
  large trees but has the subtle correctness risks called
  out in Design Note 1. A future lesson could revisit this
  if the deep-clone cost becomes a bottleneck.
- **It is not a streaming / incremental patch.** The
  `patch` (and the transactional wrappers) apply a full
  in-memory `std::vector<JsonPatchOp>`. A streaming
  variant over a cursor of ops is a separate concern
  (orthogonal to transactional semantics).
- **It is not a fix for the engine's self-move quirk.** The
  finding above is documented; the engine fix is a separate
  lesson.
- **It does not bump the library.** The library version is
  unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift `deep_clone` + `patch_atomic` +
  `patch_dry_run` from this consumer into
  `<psp_span/json_ext.h>`; bump the version).
- **It does not change the wire format.** The
  v0.15.0 writer + v0.13.0 parser are unchanged.
  The transactional layer is a control-flow wrapper that
  sits between "parsed ops in memory" and "mutated
  JsonValue tree".
- **It does not add a new error enumerator.** All
  `JsonPatchError` values are inherited from the engine.
  The wrapper either commits (returns `{}`) or propagates
  the engine's error.
- **It does not change `psp::json_patch::patch`.** The
  v0.12.0 engine is unchanged at v0.15.0. The library
  gains a transactional *wrapper*; the engine is
  byte-for-byte identical to v0.12.0/v0.13.0/v0.14.0/
  v0.15.0.

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
cmake -S late-may/cpp_practice/transactional_patch -B late-may/cpp_practice/transactional_patch/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/transactional_patch/build
./late-may/cpp_practice/transactional_patch/build/P-2026-08-03-transactional-patch
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/transactional_patch -B late-may/cpp_practice/transactional_patch/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/transactional_patch/build-strict
./late-may/cpp_practice/transactional_patch/build-strict/P-2026-08-03-transactional-patch
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/transactional_patch -B late-may/cpp_practice/transactional_patch/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/transactional_patch/build-asan
./late-may/cpp_practice/transactional_patch/build-asan/P-2026-08-03-transactional-patch
```

All three builds pass cleanly. **71 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **transactional engine arc** — the
caller can now apply RFC 6902 patches with true
all-or-nothing semantics. The library version is unchanged at
v0.15.0; a future v0.16.0 promotion is mechanical
(three new functions lifted from this consumer into
`<psp_span/json_ext.h>`).

The remaining v0.15.0 candidates (re-quoting from the Aug 2
"v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer arc
  the Jul 21 lesson opened ("JSON Pointer → JSON Patch →
  JSON Schema"). Uses today's `patch_atomic` + `patch_dry_run`
  for atomicity (atomic schema-driven updates + dry-run
  validation).
- **Streaming patch parser** — the v0.13.0
  `parse_patch_document` reads a full `string_view`; a
  streaming variant over `Span<const char>` would close
  the cursor-primitive gap in the RFC 6902 layer.
- **Widen the dispatcher's int64-vs-double preservation
  guard** from `int` to `int64_t` — orthogonal to today's
  lesson; relevant if a real consumer hits a
  double-shaped int64-range input.
- **Engine-level self-move fix** — short-circuit
  `from == path` in `apply_move` to honor RFC 6902 §4.4's
  "self-move is a no-op" rule. Pre-existing engine
  quirk, flagged today.

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
- **Inverse-journal optimisation for `patch_atomic`** —
  per-op journal of inverses instead of a full
  deep-clone; relevant for MB-scale patches.

For the library as a whole, today's lesson is the
**canonical closing entry** for the transactional engine arc
that opened with the Jul 22 engine's docstring: "The
stronger 'SHOULD leave unmodified' contract would require
transactional rollback over the partial mutations; we don't
implement that." Today's `patch_atomic` + `patch_dry_run`
implement that contract as a consumer-side wrapper, with the
v0.16.0 promotion as a near-mechanical follow-on.
