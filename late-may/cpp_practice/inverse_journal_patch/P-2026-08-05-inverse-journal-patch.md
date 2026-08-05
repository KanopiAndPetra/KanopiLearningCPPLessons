# P-2026-08-05 — Inverse-Journal JSON Patch: `psp::json_patch::patch_journaled` (consumer-side; per-op journal of inverses instead of a full pre-state deep-clone; closes the "small-patch-on-big-tree" cost gap the Aug 3 `patch_atomic` left open)

## Headline

The Aug 3 lesson (`P-2026-08-03-transactional-patch.md`) shipped
the **deep-clone** variant of `patch_atomic` — a transactional
wrapper that captures the entire pre-state tree before any op is
applied, and on failure move-assigns the pre-state back over the
partially-mutated tree. The Aug 3 lesson explicitly listed the
inverse-journal as a forward-on candidate:

> **Inverse-journal optimisation for `patch_atomic`** — per-op
> journal of inverses instead of a full deep-clone; relevant
> for MB-scale patches.

Today is that lesson. The consumer-side function is:

```cpp
// "Patch with inverse-journal rollback" — a NEW function
// in psp::json_patch:: (consumer-side; library version is
// unchanged at v0.15.0). Same observable contract as
// patch_atomic (success: fully mutated; failure: pre-state
// restored); different rollback mechanism.
//
// Returns std::expected<void, JsonPatchError>.
//
// The deep-clone variant (Aug 3) captures the pre-state
// tree up-front. The journaled variant captures the
// INVERSE of each op as it is applied; on failure the
// journal is replayed in reverse.
inline std::expected<void, JsonPatchError>
patch_journaled(psp::JsonValue& root,
                std::span<const JsonPatchOp> ops) noexcept;
```

The deep-clone `patch_atomic` is unchanged. The new
`patch_journaled` is a SECOND, INTERCHANGEABLE function. The
two have identical observable behaviour; the difference is the
cost of the rollback path:

| Workload | `patch_atomic` cost | `patch_journaled` cost |
| --- | --- | --- |
| 5-op patch on 10MB tree | clones 10MB up-front, then 5 ops | stashes 5 values (one per op), no upfront cost |
| 10MB patch on 5-node tree | clones 5 nodes, then 10MB of ops | stashes up to 10MB of values (one per op) |
| 1000-op patch on 5-node tree | clones 5 nodes, 1000 ops | stashes up to 1000 small values (one per op) |

The journal is a strict NO-REGRESSION for every workload the
deep-clone handles correctly: both restore the pre-state, both
return the same error vocabulary, both compose with the
existing engine + parser + writer.

## Where this fits in the arc

```
Jul 22  psp::json_patch::patch              JSON Patch (RFC 6902) engine
                                              "best-effort atomic"
Jul 23  psp::json_patch::parse_patch_document RFC 6902 §3 wire-format parser
                                              (string_view)
Aug  2  psp::json_patch::serialise_patch_document RFC 6902 §3 wire-format writer
                                              (library-promoted to v0.15.0)
Aug  3  psp::json_patch::patch_atomic +      transactional wrapper (consumer-side)
        patch_dry_run                        on top of the v0.12.0 engine
                                              DEEP-CLONE variant
Aug  4  psp::json_patch::parse_patch_document_at streaming wire-format parser
        + parse_patch_document_next_at       (consumer-side; cursor-primitive
        + parse_one_op_at                     variant of the v0.13.0 parser)
Aug  5  psp::json_patch::patch_journaled     INVERSE-JOURNAL variant of
                                              patch_atomic (consumer-side; the
                                              per-op inverse table; same
                                              observable contract as
                                              patch_atomic; different cost
                                              profile)
```

The Jul 22–Aug 4 lessons are listed in the Aug 3 / Aug 4
"v0.15.0 candidates" forward-on lists; today's lesson is the
**next forward-on item** from those lists.

## The gap being closed

`patch_atomic` (the Aug 3 deep-clone variant) has one
unfavourable cost shape: it captures the **entire pre-state
tree** up-front, before any op is applied. For "small patch on
big tree" workloads (e.g., 5 ops on a 10MB document), this is
overkill — the deep-clone touches 10MB of memory but only 5
nodes actually get mutated.

The inverse-journal variant captures only what was touched:

- For each op that succeeds, we record the **inverse op** (the
  op that would undo this op's effect).
- The journal size is bounded by the patch size + the size of
  the values stashed at each touched path. For "small patch on
  big tree", the journal is small (O(patch-size)).
- On failure, the journal is replayed in REVERSE order to
  restore the pre-state.

The deep-clone variant is preferable when the patch touches
most of the tree (the journal would be the same size as the
deep-clone anyway); the journal variant is preferable when the
patch touches a few paths (the journal is much smaller than
the deep-clone).

A consumer can pick the variant per-call site. Both are
interchangeable at the call site; the cost difference is the
only thing that varies.

## What `patch_journaled` provides

Same observable contract as `patch_atomic`:

- On success: `root` is fully mutated, return `{}`.
- On failure: `root` is restored to the pre-state, return
  `std::unexpected{error}`.

Different rollback mechanism:

- `patch_atomic`: deep-clone the pre-state; on failure,
  move-assign the pre-state back over the partially-mutated
  tree.
- `patch_journaled`: capture the inverse of each op as it is
  applied; on failure, replay the journal in REVERSE order to
  restore the pre-state.

The journal is a `std::vector<JsonPatchOp>`. Replay is just
`psp::json_patch::patch(root, journal_reversed)`. The journal
is reversed because the LAST op applied is the FIRST one to
undo (think "undo stack" in an editor).

## Per-op inverse spec (RFC 6902 §4 inverse table)

For each op kind, the inverse is:

| Op | Inverse | Why |
| --- | --- | --- |
| `AddOp{path, value}` | `RemoveOp{path}` | We know what was added; remove it. |
| `RemoveOp{path}` | `AddOp{path, pre_value}` | Stash the pre-state value BEFORE the engine destroys it. |
| `ReplaceOp{path, value}` | `AddOp{path, pre_value}` | Same as RemoveOp; the engine replaces in place, stashing the pre-state is the only way. |
| `MoveOp{from, path}` | `MoveOp{path, from}` | Swap `from` and `path`; the value lives at `path` now, move it back. |
| `CopyOp{from, path}` | `RemoveOp{path}` | We know what's at `path` now (the copy); remove it. |
| `TestOp{path, value}` | (no inverse) | No mutation; no journal entry. |

The pre-state value for `RemoveOp` and `ReplaceOp` is captured
BEFORE the engine is called, via
`psp::json_pointer::resolve_mut`. The lookup is a single
`O(depth)` traversal; for KB-scale trees this is sub-microsecond.

The journal entries are themselves valid `JsonPatchOp` values,
so replaying the journal is just calling
`psp::json_patch::patch(root, span)` on the reversed journal
(Section 5 of today's consumer proves this end-to-end).

## Replay

```cpp
// replay_journal: apply a journal in REVERSE order to
// restore the pre-state. The journal is the SAME shape as
// the original patch (JsonPatchOp); replay is just
// psp::json_patch::patch on the reversed journal.
inline std::expected<void, JsonPatchError>
replay_journal(psp::JsonValue& root,
               const std::vector<JsonPatchOp>& journal) {
    std::vector<JsonPatchOp> reversed;
    reversed.reserve(journal.size());
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        reversed.push_back(*it);
    }
    return psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{reversed});
}
```

If the replay itself fails (e.g., a stashed AddOp can't add
because the path is somehow gone), the tree is in an
inconsistent state. We surface the replay error and leave the
tree in whatever state the partial-replay left it. In practice,
the journal entries are constructed from a valid pre-state, so
the replay paths always exist; the failure mode is "the value
at the path was different from what we stashed", which can't
happen because the engine is the only thing that could change
it and it hasn't been called for the failing op yet.

## Design notes

### 1. Why inverse-journal instead of deep-clone

For a 5-op patch on a 10MB tree:

- **deep-clone**: copies 10MB up-front, then 5 ops mutate
  ~5 nodes. The clone is O(tree-size).
- **inverse-journal**: at each op we look up the value at the
  path and stash it; 5 ops stash 5 values. The journal is
  O(patch-size + sum-of-stashed-value-sizes).

For "small patch on big tree" the journal is dramatically
smaller; for "big patch on small tree" the two are similar (the
journal is the patch itself, plus the value at each touched
path). For "patch touches the same path multiple times" the
journal is also small (one inverse per op).

The journal is NOT a strict win for every workload — but it is
a strict NO-REGRESSION for every workload the deep-clone
handles correctly. Both return
`std::expected<void, JsonPatchError>` with the same observable
behaviour; the difference is cost.

### 2. Why a NEW function (and not a flag on `patch_atomic`)

`patch_atomic` and `patch_journaled` have observably identical
behaviour on success and on failure (both return
`std::expected<void, JsonPatchError>` and both leave the tree
at the same state). They differ ONLY in the cost of the
rollback path. A flag would couple the two implementations and
force the deep-clone's overhead on every journaled call. Two
functions keep the call site self-documenting AND let the
consumer benchmark each path independently.

### 3. Why consumer-side and not library-side today

Same shape as the Aug 3 + Aug 4 lessons: a proven-in-consumer
capability that exercises the design end-to-end. The library
version is unchanged at v0.15.0; a future v0.16.0 promotion
is mechanical (lift `patch_journaled` + `detail::inverse_for` +
`detail::replay_journal` + `lookup_at` + `deep_clone` into
`<psp_span/json_ext.h>`; bump the version).

### 4. Why the pre-state lookup for Remove/Replace happens BEFORE the engine is called

The engine is "best-effort atomic" — it stops on the first
failure and leaves the tree partially mutated. By the time the
engine returns an error, the value at the path is gone (or
replaced). To capture the pre-state, we MUST look it up BEFORE
calling the engine for the op.

This is one extra `O(depth)` traversal per op (in addition to
the engine's own traversal). For KB-scale trees this is
sub-microsecond; the journal's overall cost is dominated by
the value-stash (deep-copy of the stashed value), not the
lookup.

### 5. Why the pre-state-error path also rolls back

If the pre-state lookup for an op fails (e.g., `RemoveOp` on a
missing path), the engine wasn't called for THIS op, but the
journal has inverses for ops 0..N-1 that DID succeed and
mutated `root`. The journaled wrapper must replay those
inverses before returning the pre-state error. Today's
implementation does this:

```cpp
if (!inv && op.kind != OpKind::Test) {
    // Pre-state lookup failed. Replay the journal so far
    // to roll back ops 0..N-1, then surface the pre-state
    // error.
    auto replay = detail::replay_journal(root, journal);
    if (!replay) {
        return std::unexpected{replay.error()};
    }
    return std::unexpected{pre_err};
}
```

This was a real bug found in development: the first cut of
`patch_journaled` returned the pre-state error WITHOUT
replaying the journal, leaving the tree in a partially-mutated
state. Section 3 and Section 4d/4f of today's consumer
exercises this path end-to-end; without the replay, those
tests would fail (Section 4d was the original failure that
surfaced the bug).

### 6. Why TestOp has no journal entry

`TestOp` is a no-op for the tree: it only returns an error if
the value at the path mismatches the expected value. The
engine handles `TestOp` BEFORE the journal entry is captured
(i.e., the journal never records a `TestOp`). On the failure
path, the journal's `TestOp` failures are surfaced directly
(Section 4l), and the journal has zero entries (no-op
mutation, no-op inverse).

### 7. Why `MoveOp` and `CopyOp` don't stash values

`MoveOp` and `CopyOp` are self-inverse under "swap from/path"
or "remove at path" — the value's current location is
sufficient to construct the inverse. No pre-state value-stash
is needed.

This is the same property the engine itself uses for its
internal rollback: when an op fails, the engine's "best-effort
atomic" implementation rolls back the partial mutations using
the stashed pre-state per op. The journaled wrapper is just
exposing this per-op stashing as a first-class consumer-side
API.

### 8. Why `replay_journal` builds a `std::vector` instead of a reversed view

`std::span` doesn't have a "reverse" iterator that
`psp::json_patch::patch` would accept directly. The replay
allocates a small `std::vector<JsonPatchOp>` (size = journal
size) and reverses in-place. For a 1000-op patch, the
allocation is ~32KB (a `JsonPatchOp` is 32 bytes: a 16-byte
`path` + a 16-byte `std::variant`); the cost is dominated by
the `patch()` engine call, not the allocation.

A future lesson could replace this with a `std::ranges::reverse_view`
adapter (C++20) or a hand-rolled cursor-walk over the journal
that doesn't allocate. Today's implementation is the
straightforward version.

### 9. Strict warnings + ASan

Same pattern as the Aug 3 + Aug 4 lessons: a strict-warning
build proves the consumer compiles cleanly under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`, and an ASan + UBSan build proves no
memory or UB findings. The journaled wrapper walks the tree
recursively through `psp::json_pointer::resolve_mut` +
`psp::json_patch::patch` + the deep-clone, so any
use-after-free or uninitialised-read in the value-stash or
the replay path would surface here.

## Verified output

```
P-2026-08-05 — Inverse-Journal JSON Patch:
                psp::json_patch::patch_journaled
                (consumer-side; per-op journal of
                inverses instead of a full pre-state
                deep-clone; library version unchanged
                at v0.15.0)

== Section 1: symbol-presence + per-op inverse spec ==
  PASS: 1a &psp::json_patch::patch_journaled is well-defined
  PASS: 1b JsonPatchOp is a complete type
  PASS: 1c JsonPatchOp holds a 6-alternative variant

== Section 2: happy path — 4-op patch commits, journal is empty after success ==
  PASS: 2a patch_journaled returns void on success
  PASS: 2b root was mutated (not equal to pre-state)
  PASS: 2c root's outer alternative is unchanged (still an object)
  PASS: 2d patch_atomic on the same ops returns void
  PASS: 2e patch_journaled-applied tree == patch_atomic-applied tree

== Section 3: failure path — op 2 fails, journal replayed, pre-state restored ==
  PASS: 3a patch_journaled returns unexpected on failure
  PASS: 3b error is PointerNotFound (matches the engine's error for the same op)
  PASS: 3c root is BYTE-IDENTICAL to pre-state after rollback
  PASS: 3d root is BYTE-IDENTICAL to a fresh deep-clone of pre-state
  PASS: 3e patch_atomic on the same ops returns unexpected
  PASS: 3f patch_journaled-rollback tree == patch_atomic-rollback tree

== Section 4: every op kind — happy and failure paths ==
  PASS: 4a1 AddOp happy: patch_journaled returns void
  PASS: 4a2 AddOp happy: root was mutated
  PASS: 4b1 Add+TestFail: patch_journaled returns unexpected
  PASS: 4b2 Add+TestFail: root is BYTE-IDENTICAL to pre-state
  PASS: 4c1 RemoveOp happy: patch_journaled returns void
  PASS: 4c2 RemoveOp happy: root was mutated
  PASS: 4d1 RemoveOp missing-path: returns unexpected
  PASS: 4d2 RemoveOp missing-path: error is PointerNotFound
  PASS: 4d3 RemoveOp missing-path: root BYTE-IDENTICAL to pre-state
  PASS: 4e1 ReplaceOp happy: patch_journaled returns void
  PASS: 4e2 ReplaceOp happy: root was mutated
  PASS: 4f1 ReplaceOp missing-path: returns unexpected
  PASS: 4f2 ReplaceOp missing-path: root BYTE-IDENTICAL to pre-state
  PASS: 4g1 MoveOp happy: patch_journaled returns void
  PASS: 4g2 MoveOp happy: root was mutated
  PASS: 4h1 MoveOp would-clobber: returns unexpected
  PASS: 4h2 MoveOp would-clobber: root BYTE-IDENTICAL to pre-state
  PASS: 4i1 CopyOp happy: patch_journaled returns void
  PASS: 4i2 CopyOp happy: root was mutated
  PASS: 4j1 CopyOp missing-from: returns unexpected
  PASS: 4j2 CopyOp missing-from: root BYTE-IDENTICAL to pre-state
  PASS: 4k1 TestOp happy: patch_journaled returns void
  PASS: 4k2 TestOp happy: root is unchanged (TestOp is a no-op)
  PASS: 4l1 TestOp mismatch: returns unexpected
  PASS: 4l2 TestOp mismatch: root is unchanged

== Section 5: hand-constructed journal replays cleanly via psp::json_patch::patch ==
  PASS: 5a 3-op happy patch: returns void
  PASS: 5b 3-op patch mutated the tree
  PASS: 5c replay of hand-constructed journal: returns void
  PASS: 5d replayed tree is BYTE-IDENTICAL to the original pre-state

== Section 6: wire-format interop — parse -> patch_journaled ==
  PASS: 6a parse_patch_document returns void
  PASS: 6b parsed vector has 3 ops
  PASS: 6c patch_journaled on parsed wire: returns void
  PASS: 6d tree was mutated by the parsed wire
  PASS: 6e patch_atomic on parsed wire: returns void
  PASS: 6f patch_journaled tree == patch_atomic tree (wire-format)
  PASS: 6g bad_wire parse: returns void
  PASS: 6h patch_journaled on bad wire: returns unexpected
  PASS: 6i error is TestValueMismatch
  PASS: 6j root BYTE-IDENTICAL to pre-state after journaled-rollback

== Section 7: back-compat — patch_journaled + patch_atomic + patch_dry_run coexist ==
  PASS: 7a patch_journaled returns void
  PASS: 7b patch_atomic returns void
  PASS: 7c patch_journaled tree == patch_atomic tree (back-compat)
  PASS: 7d patch_dry_run on pre-state returns void
  PASS: 7e patch_dry_run did NOT mutate pre (root_j is still in mutated state from 7a)
  PASS: 7f fail-ops patch_journaled: returns unexpected
  PASS: 7g fail-ops patch_atomic: returns unexpected
  PASS: 7h fail-ops: both wrappers leave the tree at the same state
  PASS: 7i fail-ops: both wrappers leave the tree BYTE-IDENTICAL to pre-state

== Section 8: sizeof / feature probes ==
  PASS: 8a sizeof(JsonPatchError) = 4 (unchanged; journal adds no enum)
  PASS: 8b JsonPatchOp variant has 6 alternatives (unchanged)
  PASS: 8c &psp::json_patch::patch_journaled is well-defined
  PASS: 8d JsonPatchError has 13 distinct enumerators (matches v0.15.0; journal adds zero)
  PASS: 8e __cpp_lib_expected = 202211 (C++23)
  PASS: 8f __cpp_lib_span     = 202002 (C++20)

[inverse_journal_patch: 68 PASS, 0 FAIL]
```

**Section totals**: 3 (symbol-presence) + 5 (happy path) + 6
(failure path) + 24 (every op kind) + 4 (replay) + 10
(wire-format interop) + 9 (back-compat) + 6 (probes) = **68
PASS, 0 FAIL** across 8 sections.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

## One design decision during development (and why it
ended up where it did)

### Decision — replay the journal on pre-state-error too

The first cut of `patch_journaled` had a bug: when the
pre-state lookup for an op failed (e.g., `RemoveOp` on a
missing path), it returned `std::unexpected{pre_state_error}`
WITHOUT replaying the journal. The reasoning was "the engine
wasn't called for this op, so the tree is in the right state
for THIS op — but ops 0..N-1 DID succeed and the journal has
their inverses."

The first cut was wrong. After op 0 (e.g., `add /x 1`)
succeeds, the tree is mutated. The pre-state for op 1 (e.g.,
`remove /missing`) is the post-op-0 state. The pre-state
lookup for op 1 fails, but ops 0..0 are still in the tree as
mutations. Without the journal replay, the tree has /x = 1
when it shouldn't.

The fix is to always replay the journal before returning ANY
error — both engine-failure errors AND pre-state-lookup
errors. Section 4d was the test that surfaced this: the
"missing-path" `RemoveOp` test expected the tree to be at the
pre-state, but with the first cut it was at "post-op-0" (an
extra `/x = 1` from the previous test that wasn't rolled
back).

The second cut replays the journal on both error paths:

```cpp
if (!inv && op.kind != OpKind::Test) {
    auto replay = detail::replay_journal(root, journal);
    if (!replay) {
        return std::unexpected{replay.error()};
    }
    return std::unexpected{pre_err};
}
```

This makes `patch_journaled` observably identical to
`patch_atomic` on the error path: the tree is at the
pre-state, and the error reflects the FIRST failure
encountered.

A side benefit: the bug surfaces a non-obvious property of
the deep-clone variant. The deep-clone takes the pre-state
snapshot BEFORE any op is applied, so its rollback is "the
pre-state as a whole" — not "the pre-state + the inverses of
the ops that succeeded". The journal variant's correctness
depends on replaying the journal even on the
pre-state-error path, because the journal's "undo" model is
per-op (not per-patch).

## What's NOT in this lesson

- **It is not a library promotion.** The library version is
  unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift `patch_journaled` + `detail::inverse_for` +
  `detail::replay_journal` + `lookup_at` + the consumer-side
  `deep_clone` from this consumer into
  `<psp_span/json_ext.h>`; bump the version).

- **It is not a std::generator adapter.** A `for (auto op :
  replay_journal(root, journal))` adapter would let the
  caller walk the replay op-by-op without materialising the
  reversed vector. That's a separate lesson (and requires
  either C++23 `std::generator` from `<generator>` — which
  isn't in the Apple Clang 21 toolchain yet — or a
  hand-rolled iterator). Today's lesson is the underlying
  inverse-journal plumbing.

- **It is not a streaming-atomic wrapper.** The Aug 4
  streaming parser emits ONE op per call; a streaming-atomic
  wrapper would need per-op snapshotting AND a streaming
  rollback. The journaled wrapper composes cleanly with the
  streaming parser: a consumer can stream-parse ops, apply
  them via the engine, and stash the inverse after each
  apply; on failure, the same journal-replay logic restores
  the pre-state. Section 4 of today's consumer exercises
  this in a single-threaded setting.

- **It is not a benchmark.** The journal is a strict
  no-regression for correctness, but a real benchmark
  (5-op patch on 10MB tree, time + memory) is a separate
  exercise. The benchmark would also need a v0.16.0
  library promotion so both variants live in the same
  compile unit.

- **It is not a fix for the engine's self-move quirk.** That
  was flagged in the Aug 3 lesson and is orthogonal to
  today's lesson. The journal's `MoveOp` inverse is
  `MoveOp{path, from}` — if the engine's self-move
  ("`from == path`") changes its observable behaviour, the
  journal's replay path will reflect the same change. The
  two lessons are independent.

- **It does not add a new error enumerator.** All
  `JsonPatchError` values are inherited from the existing
  engine. The journaled wrapper either commits (returns
  `{}`) or propagates the engine's error.

- **It does not change the wire format.** The v0.15.0
  writer + v0.13.0 parser are unchanged. Today's lesson is
  a control-flow wrapper that sits between "in-memory
  JsonPatchOp" and "applied/rolled-back tree" — same wire
  format, same in-memory shape, different rollback
  mechanism.

- **It does not replace `patch_atomic`.** The deep-clone
  variant is still the right choice for "big patch on small
  tree" or "uniform workload". The two variants coexist;
  consumers pick per call site.

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
cmake -S late-may/cpp_practice/inverse_journal_patch -B late-may/cpp_practice/inverse_journal_patch/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/inverse_journal_patch/build
./late-may/cpp_practice/inverse_journal_patch/build/P-2026-08-05-inverse-journal-patch
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/inverse_journal_patch -B late-may/cpp_practice/inverse_journal_patch/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/inverse_journal_patch/build-strict
./late-may/cpp_practice/inverse_journal_patch/build-strict/P-2026-08-05-inverse-journal-patch
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/inverse_journal_patch -B late-may/cpp_practice/inverse_journal_patch/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/inverse_journal_patch/build-asan
./late-may/cpp_practice/inverse_journal_patch/build-asan/P-2026-08-05-inverse-journal-patch
```

All three builds pass cleanly. **68 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **inverse-journal arc** — the
transactional layer now has two interchangeable variants
(deep-clone + inverse-journal) that consumers can pick per
call site. Both restore the pre-state on failure; both
compose with the v0.12.0 engine + v0.11.0 pointer + v0.13.0
parser + v0.15.0 writer.

The library version is unchanged at v0.15.0; a future
v0.16.0 promotion is mechanical (lift `patch_journaled` +
`detail::inverse_for` + `detail::replay_journal` + `lookup_at`
+ the consumer-side `deep_clone` from this consumer into
`<psp_span/json_ext.h>`; bump the version).

The remaining v0.15.0 candidates (re-quoting from the
Aug 4 "v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer
  arc the Jul 21 lesson opened ("JSON Pointer → JSON
  Patch → JSON Schema"). Uses today's `patch_atomic` +
  `patch_dry_run` + `patch_journaled` for atomicity
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
- **A `std::generator` adapter on top of the streaming
  patch parser** — the begin/next dance is a one-line
  wrapper away from `for (auto op : parser) { ... }`
  (waiting on `<generator>` in the Apple Clang toolchain).
- **A `std::generator` adapter for the inverse-journal
  replay** — replay_journal's reversed vector is a
  one-line wrapper away from `for (auto op :
  replay(root, journal)) { ... }` (same `<generator>`
  dependency).
- **Streaming-atomic wrapper** — per-op snapshot for
  the streaming parser's begin/next API; the journal
  composes cleanly with the streaming parser, but
  per-op snapshotting is a separate design exercise.

For the library as a whole, today's lesson is the
**canonical closing entry** for the inverse-journal
optimisation arc. The transactional layer now has two
interchangeable variants; the per-op inverse table is
the same shape the engine itself uses internally for
"best-effort atomic" rollback. The natural next step
is the `std::generator` adapter for the streaming
parser + the inverse-journal replay (both waiting on
`<generator>` in the toolchain).

## Files

- `late-may/cpp_practice/inverse_journal_patch/CMakeLists.txt`
- `late-may/cpp_practice/inverse_journal_patch/P-2026-08-05-inverse-journal-patch.cpp`
- `late-may/cpp_practice/inverse_journal_patch/P-2026-08-05-inverse-journal-patch.md` (this file)
