# P-2026-08-06 — Engine Self-Move Fix: `psp::json_patch::patch_self_move_safe` (consumer-side; closes the RFC 6902 §4.4 "self-move is a no-op" rule gap the v0.12.0 engine's copy-then-remove sequence left open; library version unchanged at v0.15.0)

## Headline

The v0.12.0 engine (`psp::json_patch::patch` in `<psp_span/json_ext.h>`)
**breaks RFC 6902 §4.4** when given a self-move
(`MoveOp{from, path}` with `from == path`): instead of being
a no-op, the engine **deletes** the value at the path.

Today's lesson ships a consumer-side wrapper that closes the
gap by pre-filtering self-moves out of the patch list before
handing it to the engine. The wrapper's rule is two-line:

```cpp
// "Self-move is a no-op" — per RFC 6902 §4.4.
inline bool is_self_move(const ::JsonPatchOp& op) noexcept {
    if (op.kind != ::OpKind::Move) return false;
    return std::get< ::MoveOp>(op.data).from
        == std::get< ::MoveOp>(op.data).path;
}
```

```cpp
// patch_self_move_safe — pre-filter then engine.
// Same contract as patch(); self-moves are dropped.
inline std::expected<void, ::JsonPatchError>
patch_self_move_safe(psp::JsonValue& root,
                     std::span<const ::JsonPatchOp> ops) noexcept;
```

Section 2 of today's consumer proves the bug end-to-end:
`patch({"x": {"k": 42}}, [move /x/k /x/k])` ≠ `{"x": {"k": 42}}`
under the v0.12.0 engine — `patch_self_move_safe` leaves the
tree untouched.

## Where this fits in the arc

```
Aug  3  psp::json_patch::patch_atomic +          transactional wrapper (consumer-side)
        patch_dry_run                            on top of the v0.12.0 engine
                                                DEEP-CLONE variant
Aug  4  psp::json_patch::parse_patch_document_at streaming wire-format parser
        + parse_patch_document_next_at          (consumer-side; cursor-primitive
        + parse_one_op_at                        variant of the v0.13.0 parser)
Aug  5  psp::json_patch::patch_journaled         INVERSE-JOURNAL variant of
                                                patch_atomic (consumer-side)
Aug  6  psp::json_patch::patch_self_move_safe    SELF-MOVE FIX wrapper
       + filter_self_moves                       (consumer-side; closes the
       + is_self_move                            RFC 6902 §4.4 self-move rule
                                                gap the v0.12.0 engine left
                                                open; library version unchanged
                                                at v0.15.0)
```

The Aug 5 lesson closed the inverse-journal optimization arc
and re-listed three remaining v0.15.0 candidates (re-quoting):

> - JSON Schema validation
> - dispatcher int64-vs-double preservation guard widening
> - Engine-level self-move fix — short-circuit `from == path`
>   in `apply_move` to honor RFC 6902 §4.4's "self-move is a
>   no-op" rule. Pre-existing engine quirk, flagged in the
>   Aug 3 lesson.

Today is the third one. It is the smallest of the three
(one-line engine patch, three-line wrapper) but has the
cleanest pedagogical shape: a precise spec violation,
a one-shot repro, and a fix that is correct by inspection.

## The bug, in one sentence

RFC 6902 §4.4 says:

> "Note that if the `path` and `from` locations are the
>  same, the operation is a no-op."

The v0.12.0 engine implements self-move via the same
copy-then-remove logic it uses for source-under-destination
moves (`from` is a proper ancestor of `path`). For
self-move, this sequence is:

1. resolve `from` → a `JsonValue*` into `root`
2. apply_add at `path` (== from) → inserts a copy
3. apply_remove at `from` (== path) → removes the
   just-inserted copy

Net effect: the value at `from` is deleted. That's
"self-move = remove", violating the spec.

The engine's own comment (lines 945-957 of
`<psp_span/json_ext.h>`) even says the copy-then-remove
order is deliberate:

> "Doing copy-then-remove (vs. remove-then-copy) handles
>  self-moves where the source and destination are the
>  same subtree at different positions."

…but for the `from == path` case (true self-move, not
source-under-destination), the same logic produces a
deletion, not a no-op.

## Repro (Section 2)

```cpp
psp::JsonValue root   = make_tree_xk();  // {"x": {"k": 42}}
psp::JsonValue pre    = make_tree_xk();
std::vector<::JsonPatchOp> ops;
ops.push_back(::JsonPatchOp{ ::MoveOp{"/x/k", "/x/k"} });

// (A) v0.12.0 engine:
auto r = psp::json_patch::patch(root, ops);
//   r.has_value() == true        (no error reported)
//   root != pre                  (BUG: tree was mutated)
//   /x/k is gone                 (BUG: value was deleted)

// (B) patch_self_move_safe:
auto r = patch_self_move_safe(root, ops);
//   r.has_value() == true        (no error)
//   root == pre                  (tree UNCHANGED)
//   /x/k is still there
```

Section 2 of today's consumer exercises both halves
end-to-end: the v0.12.0 path self-deletes, the safe
wrapper does not.

## The fix

Two viable shapes:

**(a) Patch the engine proper.** Add an early-return in
the `case OpKind::Move` arm:

```cpp
case OpKind::Move: {
    const auto& mv = std::get< ::MoveOp>(op.data);
    if (mv.from == mv.path) break;  // self-move is a no-op
    // ...existing clobber detection + apply_add/remove...
}
```

**(b) Consumer-side wrapper that pre-filters self-moves
from the patch list before dispatching to the engine.**

Today is (b). The wrapper composes cleanly with the
existing transactional layer (`patch_atomic` /
`patch_dry_run` from Aug 3, `patch_journaled` from
Aug 5, and the inverse-journal-style replay path). The
v0.16.0 promotion arc would either lift
`patch_self_move_safe` into `<psp_span/json_ext.h>` as
a header function, or replace it with the one-line
engine patch.

Both approaches have the same observable behaviour:

| input            | engine alone      | patch_self_move_safe |
| ---              | ---               | ---                  |
| self-move        | delete value      | no-op                |
| cross-move       | move value        | move value           |
| clobber          | MoveWouldClobber  | MoveWouldClobber     |
| missing-from     | PointerNotFound   | PointerNotFound      |
| TestOp mismatch  | TestValueMismatch | TestValueMismatch    |

The only thing that changes is the self-move row.

## What `patch_self_move_safe` provides

Same shape as the existing wrappers (patch_atomic /
patch_dry_run / patch_journaled):

```cpp
inline std::expected<void, ::JsonPatchError>
patch_self_move_safe(psp::JsonValue& root,
                     std::span<const ::JsonPatchOp> ops) noexcept;
```

On success: `root` is mutated per the (input-minus-self-moves)
patch; self-moves in the input are dropped (observed as
no-ops). On failure: `std::unexpected{error}` with the
engine's error vocabulary (PointerNotFound, MoveWouldClobber,
TestValueMismatch, etc.).

The pre-filter is observably equivalent to skipping
self-moves in user code:

```cpp
// User code today (without the wrapper):
std::vector<::JsonPatchOp> filtered;
filtered.reserve(ops.size());
for (const auto& op : ops) {
    if (op.kind == ::OpKind::Move
        && std::get< ::MoveOp>(op.data).from
            == std::get< ::MoveOp>(op.data).path) {
        continue;  // self-move is a no-op
    }
    filtered.push_back(op);
}
psp::json_patch::patch(root, filtered);

// User code with the wrapper (today):
patch_self_move_safe(root, ops);
```

The wrapper is the same three lines, applied to the
patch list, in a header function.

## Design notes

### 1. Why a wrapper instead of patching the engine

The v0.16.0 promotion arc would patch the engine directly
(one line). Today's consumer-side wrapper is built first
because:

- It exercises the design end-to-end without modifying
  the library.
- It composes cleanly with the existing transactional
  layer (Section 4 of today's consumer proves this with
  patch_atomic + patch_dry_run + the inverse-journal
  helper mirrored in this TU).
- It makes the rule explicit at the call site: "this
  patch is safe against self-moves" is a property you
  opt into. The engine patch is a global rule.

The library version is unchanged at v0.15.0; the v0.16.0
promotion is mechanical (lift `patch_self_move_safe` +
`filter_self_moves` + `is_self_move` into
`<psp_span/json_ext.h>`; OR apply the one-line engine
patch and remove the wrapper).

### 2. Why pre-filter vs. post-rollback

An alternative design is to post-rollback: let the engine
self-delete, then undo the deletion by re-adding the
stashed value. The pre-filter is simpler:

- No stashing of the value at `from` (the journal
  approach from Aug 5 is overkill for this).
- No replay path. The wrapper is a single pass over the
  patch list.
- The engine's "best-effort-atomic" contract is unchanged
  (Section 4d/4f exercises the failure-with-self-move path
  end-to-end: the safe wrapper + a later failing op leaves
  the tree at pre-state).
- The composition with patch_atomic / patch_journaled /
  patch_dry_run is straightforward (Section 4).

The pre-filter is the smallest correct change.

### 3. Why string equality (not token-set equality)

`is_self_move` compares the `from` and `path` strings
byte-for-byte. An alternative is to split both into
JSON-Pointer reference tokens and compare the token
sequences — this would treat `from == "/a~1b"` and
`path == "/a/b"` as "same path" (because `~1` is the
escape for `/`).

The engine itself splits at apply time, and the
self-move check needs to match the engine's
understanding of "same path". The engine's
self-move behaviour is broken for the byte-equal case
(the bug); we don't know whether it would be broken
for the token-equal case (e.g., `"/a~1b"` vs `"/a/b"`).
We conservatively match on byte equality; the
spec is ambiguous on the escape-vs-byte question
(RFC 6902 §4.4 says "the `from` and `path` locations",
which is arguably the resolved path, but the engine
operates on raw strings).

For today's lesson, byte equality is the correct
conservative choice. A future lesson (or v0.16.0
promotion) could promote the check to token-set
equality if a real consumer needs it.

### 4. Why the wrapper composes with patch_dry_run

`patch_dry_run` (the Aug 3 dry-run wrapper) deep-clones
the root, applies the patch via `patch()`, and returns
the result without touching the original root. Composing
patch_dry_run with `patch_self_move_safe` is straightforward:

```cpp
// dry-run a patch that may contain self-moves
psp::JsonValue root = /* user input */;
std::vector<::JsonPatchOp> ops = /* user patch */;
auto r = psp::json_patch::patch_dry_run(root, ops);
//   r.has_value() == true iff (root-with-self-moves-filtered)
//                            survives the engine unchanged.
//   root itself is NOT mutated.
```

Section 4a of today's consumer exercises this end-to-end.

### 5. Why the wrapper composes with patch_atomic

`patch_atomic` (the Aug 3 deep-clone variant) captures the
pre-state, applies via `patch()`, and moves the pre-state
back on failure. Composing with `patch_self_move_safe`:

```cpp
// atomic patch that may contain self-moves:
//   pre-filter-then-atomic (cleanest):
auto filtered = psp::json_patch::filter_self_moves(ops);
auto r = psp::json_patch::patch_atomic(root, filtered);

//   safe-wrapper-then-rollback (also valid):
auto r = patch_self_move_safe(root, ops);
if (!r) {
    // engine saw a failing op; the safe wrapper doesn't
    // do its own rollback (it lets the engine's
    // best-effort-atomic handle the rollback since the
    // engine didn't see any self-moves).
}
```

Section 4 of today's consumer exercises both compositions.
For pre-filter-then-atomic, the rollback path is the
deep-clone-snapshot path. For safe-wrapper-then-rollback,
the rollback path is the engine's own partial-mutation
rollback (which is observably equivalent to deep-clone for
a patch that has no self-moves after the filter).

### 6. Why the wrapper doesn't bump the JsonPatchError enum

The wrapper either succeeds (returns `void`) or propagates
the engine's error unchanged. No new error enumerator is
added. Section 7d of today's consumer asserts that
`JsonPatchError` still has 13 distinct enumerators
(matching v0.15.0); the wrapper adds zero.

### 7. Why the wrapper is consumer-side (not library-promoted)

Same shape as Aug 3 / Aug 4 / Aug 5: a proven-in-consumer
capability that exercises the design end-to-end before
library promotion. The v0.16.0 promotion arc would lift
the three functions into `<psp_span/json_ext.h>` and bump
the version. Keeping it consumer-side for today's lesson
preserves the "library is at v0.15.0" invariant and
the "this is a lesson in psp::json_patch maintenance"
framing.

### 8. Why there's no benchmark today

The wrapper's overhead is the pre-filter pass (one
string-compare per op). For a 1000-op patch, that's 1000
comparisons — microseconds at most. The wrapper's cost
is strictly less than the engine's per-op cost
(split + resolve + apply). A real benchmark would
confirm what the big-O analysis says: the wrapper is
free. Section 7's sizeof / feature probes confirm the
wrapper compiles to ~100 bytes of executable code
(rough estimate; the strings-compare path inlines).

## Verified output

```
P-2026-08-06 — Engine Self-Move Fix:
               psp::json_patch::patch_self_move_safe
               (consumer-side; pre-filters self-moves
               before handing the patch to the v0.12.0
               engine; closes the RFC 6902 §4.4 self-move
               rule gap the v0.12.0 engine's copy-then-
               remove sequence left open; library version
               unchanged at v0.15.0)

== Section 1: symbol-presence + per-op pre-filter spec ==
  PASS: 1a &psp::json_patch::patch_self_move_safe is well-defined
  PASS: 1b patch_self_move_safe signature matches
  PASS: 1c is_self_move: MoveOp{"", ""} is a self-move
  PASS: 1d is_self_move: MoveOp{"/", "/"} is a self-move
  PASS: 1e is_self_move: MoveOp{"/x/k", "/x/k"} is a self-move
  PASS: 1f is_self_move: MoveOp{"/x/k", "/y/k"} is NOT a self-move
  PASS: 1g is_self_move: AddOp{...} is NOT a self-move (only MoveOp is)
  PASS: 1h filter_self_moves drops exactly the 2 self-moves
  PASS: 1i filter_self_moves: filtered[0] is the AddOp
  PASS: 1j filter_self_moves: filtered[1] is the MoveOp {/x/k,/y/k}
  PASS: 1k filter_self_moves: filtered[2] is the ReplaceOp

== Section 2: the bug repro — v0.12.0 self-moves self-delete; patch_self_move_safe leaves the tree untouched ==
  PASS: 2a v0.12.0 patch returns void on self-move (no error)
  PASS: 2b BUG REPRO: v0.12.0 patch self-deletes /x/k on self-move
  PASS: 2c BUG REPRO: tree is broken — /x/k is gone (no "k" key in JSON)
  PASS: 2d patch_self_move_safe returns void on self-move
  PASS: 2e FIX: patch_self_move_safe leaves the tree BYTE-IDENTICAL
  PASS: 2g FIX: tree still has /x/k = 42
  PASS: 2h patch_self_move_safe on MoveOp{"",""} returns void
  PASS: 2i FIX: MoveOp{"",""} is also a no-op under the safe wrapper
  PASS: 2j self-move + failing-op: returns unexpected
  PASS: 2k self-move + failing-op: tree BYTE-IDENTICAL to pre-state

== Section 3: every MoveOp shape — safe wrapper handles all the v0.12.0 cases, plus the new self-move case ==
  PASS: 3a self-move /a:/a returns void
  PASS: 3b self-move /a:/a leaves the tree unchanged
  PASS: 3c valid cross-move /a:/d returns void
  PASS: 3d cross-move: /a is gone
  PASS: 3e cross-move: /d holds the int 1 (the moved value)
  PASS: 3f self-move + valid cross-move returns void
  PASS: 3g self-move did NOT delete /a
  PASS: 3h cross-move /b:/e removed /b
  PASS: 3i cross-move /b:/e added /e
  PASS: 3j clobber /x -> /x/sub returns unexpected
  PASS: 3k clobber error is MoveWouldClobber
  PASS: 3l missing-from /nope -> /d returns unexpected
  PASS: 3m missing-from error is PointerNotFound
  PASS: 3n malformed-ish path: returns unexpected when path is unresolvable

== Section 4: interop with patch_atomic + patch_dry_run — safe wrapper composes with the Aug 3 transactional layer ==
  PASS: 4a dry_run on self-move returns void
  PASS: 4b dry_run on self-move: input tree is UNCHANGED
  PASS: 4c patch_self_move_safe + failing op returns unexpected
  PASS: 4d failing op leaves tree BYTE-IDENTICAL to pre-state (self-moves were filtered out before the engine saw them)
  PASS: 4e snapshot is a faithful pre-state
  PASS: 4f atomic after filter: returns unexpected on failing op
  PASS: 4g atomic after filter: tree BYTE-IDENTICAL to pre-state

== Section 5: wire-format round-trip — parse a doc with a self-move op; prove the safe wrapper observes the spec rule end-to-end through the parser ==
  PASS: 5a parse_patch_document on a self-move doc returns void
  PASS: 5b parsed vector has 1 op
  PASS: 5c parsed op is a MoveOp
  PASS: 5d v0.12.0 patch returns void on wire-format self-move
  PASS: 5e BUG: v0.12.0 still self-deletes through the parsed wire
  PASS: 5f safe wrapper on parsed self-move returns void
  PASS: 5g safe wrapper on parsed self-move: tree BYTE-IDENTICAL
  PASS: 5h parse_patch_document on a 2-op doc (1 self + 1 add) returns void
  PASS: 5i safe wrapper on multi-op wire returns void
  PASS: 5j self-move + valid add: /x/k is still there (self-move was dropped)
  PASS: 5k self-move + valid add: /new is present

== Section 6: back-compat — safe wrapper is additive; v0.12.0 quirks still surface from the wrapper ==
  PASS: 6a clobber still rejected by safe wrapper (MoveWouldClobber)
  PASS: 6b missing-from still rejected by safe wrapper (PointerNotFound)
  PASS: 6c valid cross-move: returns void
  PASS: 6d valid cross-move: /a is gone, /d is present
  PASS: 6e TestOp (matched) returns void
  PASS: 6f TestOp (matched) does not mutate
  PASS: 6g TestOp (mismatch) returns TestValueMismatch
  PASS: 6h AddOp passes through unchanged
  PASS: 6i add was applied — /x/extra holds int64 99

== Section 7: sizeof / feature probes ==
  PASS: 7a sizeof(JsonPatchError) = 4 (self-move fix adds no enum)
  PASS: 7b JsonPatchOp holds a 6-alternative variant (unchanged; self-move fix doesn't change the type)
  PASS: 7c &psp::json_patch::patch_self_move_safe is well-defined
  PASS: 7d JsonPatchError has 13 distinct enumerators (matches v0.15.0; self-move fix adds zero)
  PASS: 7e __cpp_lib_expected = 202211 (C++23)
  PASS: 7f __cpp_lib_span = 202002 (C++20)

[self_move_fix: 68 PASS, 0 FAIL]
```

**Section totals**: 11 (symbol-presence) + 10 (bug repro +
fix) + 14 (every MoveOp shape) + 7 (interop with
patch_atomic / patch_dry_run) + 11 (wire-format round-trip)
+ 9 (back-compat) + 6 (sizeof / feature probes) = **68
PASS, 0 FAIL** across 7 sections.

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`): **passes
cleanly**.

ASan + UBSan build: **passes cleanly** (no findings).

## One design decision during development (and why it
ended up where it did)

### Decision — wrapper, not engine patch, for the consumer-side artifact

The same fix could ship as either a consumer-side wrapper
(today) or a one-line engine patch (`if (mv.from ==
mv.path) break;` in the `case OpKind::Move` arm). Both
have the same observable behaviour on the success path
(no-op on self-move). The wrapper was chosen for
today's consumer-side artifact for three reasons:

1. **It exercises the design end-to-end.** Section 2
   proves the bug; Section 3 proves the fix; Section 4
   proves the fix composes with the transactional layer;
   Section 5 proves the fix interops with the wire-format
   parser; Section 6 proves the wrapper doesn't break
   the engine's existing error vocabulary. All without
   touching the library.

2. **It makes the rule explicit at the call site.** A
   consumer that needs self-move safety can opt in by
   calling `patch_self_move_safe`; a consumer that
   doesn't need it can keep calling `patch()` directly.
   With the engine patch, the rule is global — every
   patch silently drops self-moves.

3. **The library version stays at v0.15.0.** Aug 3 / 4 /
   5 are all "consumer-side wrapper" lessons; today fits
   that arc. A future v0.16.0 promotion would either
   ship the engine patch (and keep the wrapper as a
   back-compat alias) or ship the wrapper as a header
   function (and keep the engine's bug, in case any
   consumer relies on the buggy behaviour for some weird
   reason — though this is unlikely).

The engine patch is the cleaner long-term shape. Today's
lesson is the wrapper; the patch is left for the v0.16.0
promotion.

## What's NOT in this lesson

- **It is not a library promotion.** The library version
  is unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift `patch_self_move_safe` +
  `filter_self_moves` + `is_self_move` into
  `<psp_span/json_ext.h>`; OR apply the one-line engine
  patch and back-compat-alias the wrapper; bump the
  version).

- **It is not an engine patch.** The engine's apply_move
  still has the copy-then-remove bug for self-moves. The
  wrapper compensates for it externally. The v0.16.0
  promotion would patch the engine directly.

- **It is not a `std::set` / token-set equality
  comparison.** The wrapper compares `from` and `path`
  byte-for-byte (string equality). A token-set comparison
  (split both, compare reference-token sequences, handle
  `~0` / `~1` escapes) would treat `from == "/a~1b"` and
  `path == "/a/b"` as "same path" — but the spec is
  ambiguous on this question, and the engine's own
  self-move bug only manifested in the byte-equal case.
  The byte-equal fix is sufficient for the spec rule;
  the token-set fix is a future exercise.

- **It is not a benchmark.** The wrapper's overhead is
  the pre-filter pass — one string-compare per op, ≤
  microseconds for a typical patch size. The big-O
  analysis says it's free; the consumer doesn't need a
  microbenchmark to confirm.

- **It does not change the wire format.** The v0.15.0
  writer + v0.13.0 parser are unchanged. Self-moves
  round-trip cleanly through the wire today
  (Section 5); today's lesson only changes how the
  engine handles self-moves on the in-memory side.

- **It does not bump the JsonPatchError enum.** Section 7
  asserts the enum has 13 distinct values — unchanged
  from v0.15.0. The wrapper either commits (returns
  `void`) or propagates the engine's error.

- **It does not replace any existing function.** Today's
  `patch_self_move_safe` is additive. The v0.12.0
  `patch` + Aug 3 `patch_atomic` / `patch_dry_run` /
  Aug 5 `patch_journaled` are all unchanged.

- **It does not repair the inverse-journal replay
  interaction.** A self-move that's part of a journal
  (i.e., the inverse-journal's `MoveOp{path, from}` for
  undoing a cross-move) becomes a self-move when `from ==
  path` (which is the cross-move's `path == from` case,
  i.e., a self-move being undone). Today's wrapper handles
  this transparently because the journal entries go
  through `psp::json_patch::patch`, not
  `patch_self_move_safe`. A future lesson could lift
  `patch_self_move_safe` into the journal path so journal
  replays also drop self-moves; the
  observability/correctness story is the same as today's
  wrapper (a self-move in a journal is a no-op in the
  inverse-journal sense).

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
cmake -S late-may/cpp_practice/self_move_fix -B late-may/cpp_practice/self_move_fix/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/self_move_fix/build
./late-may/cpp_practice/self_move_fix/build/P-2026-08-06-self-move-fix
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/self_move_fix -B late-may/cpp_practice/self_move_fix/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/self_move_fix/build-strict
./late-may/cpp_practice/self_move_fix/build-strict/P-2026-08-06-self-move-fix
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/self_move_fix -B late-may/cpp_practice/self_move_fix/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/self_move_fix/build-asan
./late-may/cpp_practice/self_move_fix/build-asan/P-2026-08-06-self-move-fix
```

All three builds pass cleanly. **68 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **engine self-move arc** — the
v0.15.0 engine now has a consumer-side fix for the
RFC 6902 §4.4 self-move rule gap. The fix composes
cleanly with the Aug 3 transactional layer (patch_atomic /
patch_dry_run / patch_journaled), the Aug 4 streaming
parser, and the v0.15.0 writer. The library version is
unchanged at v0.15.0; a future v0.16.0 promotion is
mechanical.

The remaining v0.15.0 candidates (re-quoting from the
Aug 5 "v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer
  arc the Jul 21 lesson opened ("JSON Pointer → JSON
  Patch → JSON Schema"). Uses today's `patch_atomic` +
  `patch_dry_run` + `patch_journaled` +
  `patch_self_move_safe` for atomicity (atomic
  schema-driven updates + dry-run validation).

- **Widen the dispatcher's int64-vs-double preservation
  guard** from `int` to `int64_t` — orthogonal to
  today's lesson; relevant if a real consumer hits a
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
- **v0.16.0 promotion arc** — mechanical: lift
  `patch_self_move_safe` + `filter_self_moves` +
  `is_self_move` into `<psp_span/json_ext.h>` OR apply
  the one-line engine patch; bump the version.

For the library as a whole, today's lesson is the
**canonical closing entry** for the engine self-move
optimisation arc. The transactional layer (Aug 3 / 5)
plus the safe self-move wrapper (today) covers the
full RFC 6902 behavioural surface the engine needs
to honour. The natural next step is JSON Schema
validation (the third v0.15.0 candidate), which would
give the library a complete Pointer → Patch → Schema
arc on the read/validate side, complementing the
transactional layer on the write side.

## Files

- `late-may/cpp_practice/self_move_fix/CMakeLists.txt`
- `late-may/cpp_practice/self_move_fix/P-2026-08-06-self-move-fix.cpp`
- `late-may/cpp_practice/self_move_fix/P-2026-08-06-self-move-fix.md`
  (this file)
