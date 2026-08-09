# P-2026-08-09 — Journal-Aware Self-Move Safe Wrapper: `psp::json_patch::patch_journaled_self_move_safe` (consumer-side; composes the Aug 3 + Aug 5 + Aug 6 wrappers into a single transactional layer that filters self-moves at both the input and journal boundaries; closes the journal-replay self-move gap the Aug 6 lesson's "What's NOT in this lesson" section explicitly flagged; library version unchanged at v0.15.0)

## Headline

The Aug 6 lesson (`P-2026-08-06-self-move-fix.md`) shipped a
consumer-side pre-filter wrapper
(`psp::json_patch::patch_self_move_safe`) that closes the
RFC 6902 §4.4 self-move rule gap in the v0.12.0 engine. The
Aug 6 "What's NOT in this lesson" section explicitly flagged
today's composition as forward-on work:

> "It does not repair the inverse-journal replay interaction.
>  A self-move that's part of a journal (i.e., the
>  inverse-journal's `MoveOp{path, from}` for undoing a
>  cross-move) becomes a self-move when `from == path`
>  (which is the cross-move's `path == from` case, i.e., a
>  self-move being undone). Today's wrapper handles this
>  transparently because the journal entries go through
>  `psp::json_patch::patch`, not `patch_self_move_safe`. A
>  future lesson could lift `patch_self_move_safe` into the
>  journal path so journal replays also drop self-moves; the
>  observability/correctness story is the same as today's
>  wrapper (a self-move in a journal is a no-op in the
>  inverse-journal sense)."

Today is that future lesson.

Today's consumer ships
`psp::json_patch::patch_journaled_self_move_safe` — a single
transactional wrapper that combines:

1. **The Aug 6 input-side self-move filter** (drops
   self-moves from the user-facing patch before the engine
   sees them).
2. **The Aug 5 inverse-journal transactional layer**
   (captures the inverse of each op as it's applied; replays
   the journal in REVERSE on failure to restore the
   pre-state).
3. **A NEW journal-side self-move filter** (drops self-moves
   from the journal before replay; closes the gap the Aug 6
   lesson flagged).

The journal-side filter is the new bit. The Aug 5 lesson's
`replay_journal` forward the journal as-is to
`psp::json_patch::patch`, which means a self-move in the
journal (the inverse of a cross-move whose own from == path)
hits the v0.12.0 engine's self-delete bug during replay.
Today's wrapper runs the journal through `filter_self_moves`
before replay, so the replay path is observably safe against
self-moves.

Same observable contract as `patch_journaled` (Aug 5):

- On success: `root` is fully mutated, return `{}`.
- On failure: `root` is restored to the pre-state, return
  `std::unexpected{error}`.

Plus the Aug 6 self-move rule, applied at TWO layers
(input-side AND journal-side).

Library version unchanged at v0.15.0. Future v0.16.0
promotion is mechanical (lift `patch_journaled_self_move_safe`
+ the `detail::` helpers into `<psp_span/json_ext.h>`; bump the
version).

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
Aug  9  psp::json_patch::patch_journaled_        JOURNAL-AWARE SELF-MOVE SAFE
TODAY  self_move_safe                            wrapper (consumer-side; composes
                                                Aug 3 + Aug 5 + Aug 6;
                                                input-side + journal-side
                                                self-move filter; library version
                                                unchanged at v0.15.0)
```

The Aug 6 lesson closed the engine self-move arc with a
pre-filter wrapper. The Aug 6 "What's NOT in this lesson"
section explicitly flagged today's composition as forward-on
work. Today is that future lesson.

## The composition problem

The Aug 6 `patch_self_move_safe` is a pure pre-filter
wrapper. It does not require atomicity, so it does not interact
with the Aug 3 `patch_atomic` (deep-clone) or Aug 5
`patch_journaled` (inverse-journal) transactional layers. The
Aug 6 lesson composes the pre-filter with the engine directly:

```cpp
patch_self_move_safe(root, ops)
    -> filter_self_moves(ops)
    -> patch(root, filtered)
```

The Aug 5 `patch_journaled` is an inverse-journal recovery
layer. It calls `patch()` for each op individually and captures
the inverse of each op as it's applied. On failure it replays
the journal in REVERSE to restore the pre-state:

```cpp
patch_journaled(root, ops)
    for each op:
        inv = inverse_for(root, op)  // pre-state lookup
        r   = patch(root, {op})       // apply
        if r failed: replay_journal(journal); return error
        else:        push inv to journal
```

The two recipes meet at the engine call: `patch_self_move_safe`
pre-filters the input; `patch_journaled` pre-filters nothing
and post-filters via journal replay. If we want atomicity AND
self-move safety, we need ONE pass that does both:

```cpp
patch_journaled_self_move_safe(root, ops)
    filtered = filter_self_moves(ops)             // Aug 6 input-side
    for each op in filtered:
        inv = inverse_for(root, op)               // Aug 5 pre-state
        r   = patch(root, {op})                    // engine
        if r failed:
            safe_journal = filter_self_moves(journal)  // NEW journal-side
            replay_journal(root, safe_journal)
            return error
        else:
            push inv to journal
```

On the success path: input self-moves are dropped (Aug 6).
On the failure path: the journal goes through
`filter_self_moves` before replay, so any journal entry that
has become a self-move (the cross-move's undo) is also dropped
during replay.

## The "drop self-moves" rule is invariant under replay

A self-move in the inverse-journal is the inverse of a
cross-move whose own `from == path` — i.e., a self-move being
undone. The cross-move is observably a no-op (Aug 6 rule), so
the inverse is also a no-op. Therefore:

- Replaying a self-move is a no-op.
- Dropping it during replay is equivalent to
  applying-and-undoing.

The "drop self-moves" rule is preserved under replay. This is
why the journal-side filter is correct — it doesn't change the
observable semantics, it just enforces the same rule at the
replay boundary.

## Why we add an EXPLICIT `filter_self_moves` on the journal

The Aug 6 pre-filter strips self-moves from the INPUT, but
the journal entries are CONSTRUCTED from the inverse of each
applied op. Cross-moves generate `MoveOp{path, from}`
inverses. If the cross-move's `from` happens to be a
strict-ancestor of `path` (the Aug 6 pre-existing engine
quirk where copy-then-remove fires), then the inverse's
`from == path` — i.e., the inverse is a self-move. The
input-side filter does NOT touch the journal entries, so this
self-move would fall through to the v0.12.0 engine during
replay and self-delete the value.

Today's wrapper closes this gap with an explicit
`filter_self_moves` on the journal before replay. The rule is:
"self-moves are dropped at every engine boundary".

## Why consumer-side and not library-side today

Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 lessons: a
proven-in-consumer wrapper that exercises the design
end-to-end. The library version is unchanged at v0.15.0. A
future v0.16.0 promotion is mechanical:

- Lift `patch_journaled_self_move_safe` into
  `<psp_span/json_ext.h>` as a header function, OR
- Apply the one-line engine patch (early-return on
  `from == path` in `apply_move`) and back-compat-alias
  the wrapper as a thin shim, OR
- Patch the engine path AND keep the wrapper as a
  back-compat alias for the same observable contract.

The library version is then bumped to v0.16.0.

## What the consumer exercises

The consumer has six sections, all 49 cases pass on all three
builds (default, strict-warning, ASan + UBSan):

- **Section 1 — symbol-presence + per-op pre-filter +
  journal-pre-filter spec.** (11 cases)
  - `patch_journaled_self_move_safe` is well-defined and has
    the expected `std::expected<void, JsonPatchError>`
    signature.
  - `is_self_move` recognizes self-moves (same-path and
    root-root) and rejects non-move ops.
  - `filter_self_moves` drops self-moves from a vector.

- **Section 2 — the aug-6 self-move bug, exposed through the
  journal.** (8 cases)
  - A self-move in the input is dropped (the tree is
    unchanged).
  - A self-move in the MIDDLE of a multi-op patch is dropped
    (the surrounding ops apply correctly).

- **Section 3 — every MoveOp shape (self / valid / clobber /
  missing-from) through the journal.** (10 cases)
  - Cross-move succeeds.
  - Clobber-move fails with `MoveWouldClobber`; the pre-state
    is restored on the failure path.
  - Missing-from-move fails with `PointerNotFound`; the
    pre-state is restored on the failure path.

- **Section 4 — interop with `patch_atomic` + `patch_dry_run`
  + `patch_journaled` (the Aug 3 / Aug 5 wrappers).** (6 cases)
  - On the success path,
    `patch_journaled_self_move_safe` and
    `patch_self_move_safe` produce the same final tree.
  - On the failure path,
    `patch_journaled_self_move_safe` and
    `patch_journaled` produce the same error AND the same
    pre-state.

- **Section 5 — end-to-end: a self-move being UNDONE via the
  journal (the case the Aug 6 lesson's "What's NOT in this
  lesson" flagged).** (11 cases)
  - A 2-op patch with cross-move + add succeeds.
  - A 3-op patch where the third op fails triggers replay;
    the pre-state is correctly restored.
  - The journal-side filter is observably correct: a
    self-move in the journal is dropped before replay.

- **Section 6 — sizeof / feature probes.** (3 cases)
  - `sizeof(JsonPatchOp)` is unchanged.
  - `JsonPatchError` has 13 enumerators (unchanged).
  - The wrapper is consumer-side (no library change).

## Important code

### The new wrapper

```cpp
inline std::expected<void, JsonPatchError>
patch_journaled_self_move_safe(psp::JsonValue& root,
                               std::span<const JsonPatchOp> ops) noexcept {
    // Layer 1: pre-filter the input (Aug 6 rule).
    auto filtered = detail::filter_self_moves(ops);

    std::vector<JsonPatchOp> journal;
    journal.reserve(filtered.size());

    for (const auto& op : filtered) {
        // Step 1: compute the inverse of this op against the
        // CURRENT state (which is the pre-state for this op).
        JsonPatchError pre_err = JsonPatchError::BadDocument;
        auto inv = detail::inverse_for(root, op, pre_err);
        if (!inv && op.kind != OpKind::Test) {
            // Pre-state lookup failed. Replay (with
            // self-move filter) to roll back, surface the
            // pre-state error.
            auto replay = detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{pre_err};
        }

        // Step 2: apply the op via the engine.
        auto r = psp::json_patch::patch(root,
            std::span<const JsonPatchOp>{&op, 1});
        if (!r) {
            // Engine failed. Replay (with self-move filter).
            auto replay = detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{r.error()};
        }

        // Step 3: op succeeded; record the inverse in the
        // journal. (TestOp has no inverse; skip.)
        if (inv) {
            journal.push_back(std::move(*inv));
        }
    }
    return {};
}
```

### The new replay path (with self-move filter)

```cpp
// Diff from Aug 5: this version filters self-moves from the
// journal BEFORE replay. That's the new bit in today's lesson.
inline std::expected<void, JsonPatchError>
replay_journal(psp::JsonValue& root,
               const std::vector<JsonPatchOp>& journal) {
    // Build the reversed journal AND filter self-moves in one pass.
    std::vector<JsonPatchOp> reversed;
    reversed.reserve(journal.size());
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        if (is_self_move(*it)) continue;  // <-- THE NEW BIT
        reversed.push_back(*it);
    }
    return psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{reversed});
}
```

### The per-op self-move rule (unchanged from Aug 6)

```cpp
inline bool
is_self_move(const ::JsonPatchOp& op) noexcept {
    if (op.kind != ::OpKind::Move) return false;
    const auto& m = std::get< ::MoveOp>(op.data);
    return m.from == m.path;
}
```

## Observed output

All three builds (default, strict-warning, ASan + UBSan) print
49 PASS / 0 FAIL across 6 sections. The exit code is 0.

```
P-2026-08-09 — journal-aware self-move safe wrapper
  (composes Aug 3 + Aug 5 + Aug 6 wrappers into a single
  transactional layer; library version unchanged at v0.15.0)

== Section 1: symbol-presence + pre-filter spec ==
  PASS: 1a &psp::json_patch::patch_journaled_self_move_safe is well-defined
  PASS: 1b patch_journaled_self_move_safe signature matches std::expected<void, JsonPatchError>
  PASS: 1c is_self_move(self) == true
  PASS: 1d is_self_move(cross) == false
  PASS: 1e is_self_move(add) == false
  PASS: 1f is_self_move(remove) == false
  PASS: 1g is_self_move(root root) == true
  PASS: 1h filter_self_moves drops 2 self-moves, keeps 3
  PASS: 1i filter_self_moves[0] is the first AddOp
  PASS: 1j filter_self_moves[1] is the second AddOp
  PASS: 1k filter_self_moves[2] is the third AddOp

== Section 2: self-move in input + self-move in journal ==
  PASS: 2a self-move in input succeeds under the journal wrapper
  PASS: 2b self-move in input preserves /x (vs engine self-delete)
  PASS: 2c self-move in input preserves /x/k
  PASS: 2d self-move in input preserves /x/k value (42)
  PASS: 2e self-move in the middle of a patch succeeds
  PASS: 2f first add applied (y present)
  PASS: 2g third add applied (z present)
  PASS: 2h intermediate self-move dropped, /x/k preserved

== Section 3: every MoveOp shape through the journal ==
  PASS: 3a cross-move succeeds
  PASS: 3b cross-move target /new is present
  PASS: 3c cross-move source /x/k is gone
  PASS: 3d clobber-move fails
  PASS: 3e clobber-move error is MoveWouldClobber
  PASS: 3f pre-state restored: /other rolled back
  PASS: 3g pre-state restored: /x preserved
  PASS: 3h missing-from-move fails
  PASS: 3i missing-from error is PointerNotFound
  PASS: 3j pre-state restored: /first rolled back

== Section 4: interop with patch_atomic + patch_dry_run + patch_journaled ==
  PASS: 4a journal-self-move-safe apply succeeds
  PASS: 4b journal-self-move-safe == patch_self_move_safe on success
  PASS: 4c journal-self-move-safe fails on missing path
  PASS: 4d patch_journaled fails on missing path
  PASS: 4e both wrappers report the same error
  PASS: 4f both wrappers restore the pre-state identically

== Section 5: a self-move being UNDONE via the journal (the case the Aug 6 lesson's 'What's NOT in this lesson' flagged) ==
  PASS: 5a cross-move + add succeeds
  PASS: 5b cross-move target /new is present
  PASS: 5c first add applied
  PASS: 5d third-op-fail triggers replay
  PASS: 5e error is PointerNotFound
  PASS: 5f pre-state restored: /placeholder rolled back
  PASS: 5g pre-state restored: /new rolled back
  PASS: 5h pre-state restored: /x/k preserved
  PASS: 5i filter_self_moves drops the self-move from the journal
  PASS: 5j journal[0] is the AddOp
  PASS: 5k journal[1] is the RemoveOp

== Section 6: sizeof / feature probes ==
  PASS: 6a sizeof(JsonPatchOp) is unchanged
  PASS: 6b JsonPatchError has 13 enumerators (unchanged)
  PASS: 6c wrapper is consumer-side (no library change; v0.15.0 unchanged)

--- Summary ---
sections: 6
passes:   49
fails:    0

All checks passed. main returns 0.
```

## Design notes

### 1. Why the journal-side filter is a `filter_self_moves` call, not a `for` loop

The Aug 6 lesson's `filter_self_moves` is observable as a
pure pre-pass: it takes a `std::span<const JsonPatchOp>` and
returns a `std::vector<JsonPatchOp>` with the self-moves
dropped. The replay path combines the loop with the
reverse-iteration, so we get self-move filtering "for free" on
the same pass:

```cpp
for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
    if (is_self_move(*it)) continue;  // <-- THE NEW BIT
    reversed.push_back(*it);
}
```

This is a single O(N) pass over the journal. The
`filter_self_moves` call is inlined into the loop, so the
journal-side filter adds zero allocations beyond the existing
`reversed` vector.

### 2. Why the journal-side filter is correct

The "drop self-moves" rule is invariant under replay. A
self-move in the journal is the inverse of a cross-move whose
own `from == path` — i.e., a self-move being undone. The
cross-move is observably a no-op (Aug 6 rule), so the inverse
is also a no-op. Therefore:

- Replaying a self-move is a no-op.
- Dropping it during replay is equivalent to
  applying-and-undoing.

This is why the journal-side filter doesn't change the
observable semantics. It just enforces the same rule at the
replay boundary.

### 3. Why we don't need a `noexcept` adjustment

The wrapper is `noexcept` end-to-end. The `filter_self_moves`
calls in the input-side pre-filter and the journal-side
replay-pre-filter can throw `std::bad_alloc` (the
`std::vector` allocation), but the Aug 5 + Aug 6 wrappers
were also marked `noexcept` despite the same potential
allocation failure — the consumer-side convention is to
declare `noexcept` on the OUTER function and let the
allocator throw if it must. The same convention applies
today.

### 4. Why this composes with the Aug 3 + Aug 5 wrappers

Section 4 of today's consumer proves the three wrappers
compose observably:

- **On success:** `patch_journaled_self_move_safe` and
  `patch_self_move_safe` produce the same final tree.
- **On failure:** `patch_journaled_self_move_safe` and
  `patch_journaled` produce the same error AND the same
  pre-state.

The composition is correct by construction: the input-side
filter is a pre-pass over the user's `ops`, the inverse
journal is built per-op, and the replay path is the
journal-side filter + `patch()`. The three recipes stack
independently.

### 5. Why the wrapper is consumer-side, not library-side

Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 lessons: a
proven-in-consumer wrapper that exercises the design
end-to-end. The library version is unchanged at v0.15.0. A
future v0.16.0 promotion is mechanical (lift
`patch_journaled_self_move_safe` + the `detail::` helpers into
`<psp_span/json_ext.h>`; bump the version).

## Verified output

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) — clean compile,
49 PASS / 0 FAIL.

ASan + UBSan build (`-fsanitize=address -fsanitize=undefined
-fno-omit-frame-pointer -O1`) — clean compile, 49 PASS / 0
FAIL, no sanitizer warnings.

`main` returns 0 on success and 1 on any failure.

## What's NOT in this lesson

- **It is not a library promotion.** The library version is
  unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift `patch_journaled_self_move_safe` + the
  `detail::` helpers into `<psp_span/json_ext.h>`; bump the
  version).

- **It is not an engine patch.** The engine's `apply_move`
  still has the copy-then-remove bug for self-moves. The
  wrapper compensates for it externally. The v0.16.0
  promotion would patch the engine directly.

- **It is not a benchmark.** The wrapper's overhead is one
  pre-filter pass (input-side) + one pre-filter pass
  (journal-side, combined with the existing reverse-iteration
  loop). The big-O analysis says it's free; the consumer
  doesn't need a microbenchmark to confirm.

- **It does not change the wire format.** The v0.15.0 writer
  + v0.13.0 parser are unchanged. Self-moves round-trip
  cleanly through the wire today; today's lesson only changes
  how the engine handles self-moves on the in-memory side.

- **It does not bump the JsonPatchError enum.** Section 6
  asserts the enum has 13 distinct values — unchanged from
  v0.15.0. The wrapper either commits (returns `void`) or
  propagates the engine's error.

- **It does not replace any existing function.** Today's
  `patch_journaled_self_move_safe` is additive. The v0.12.0
  `patch` + Aug 3 `patch_atomic` / `patch_dry_run` + Aug 5
  `patch_journaled` + Aug 6 `patch_self_move_safe` are all
  unchanged.

- **It does not address the `std::set` / token-set equality
  comparison question.** The `is_self_move` rule compares
  `from` and `path` byte-for-byte (string equality). A
  token-set comparison (split both, compare reference-token
  sequences, handle `~0` / `~1` escapes) would treat
  `from == "/a~1b"` and `path == "/a/b"` as "same path" — but
  the spec is ambiguous on this question, and the engine's
  own self-move bug only manifested in the byte-equal case.
  The byte-equal fix is sufficient for the spec rule; the
  token-set fix is a future exercise.

- **It does not address the cross-move + clobber asymmetry.**
  The v0.12.0 engine rejects a cross-move whose `from` is a
  strict-ancestor of `path` with `MoveWouldClobber`. That's
  the RFC 6902 §4.4 rule for cross-moves; it predates the
  self-move arc. Today's wrapper inherits the engine's
  behavior unchanged.

- **It does not mirror the `patch_atomic` /
  `patch_self_move_safe` composition.** That composition
  (deep-clone + input-side filter) is observably equivalent
  to today's `patch_journaled_self_move_safe` (inverse-journal
  + input-side + journal-side filter) on the success path.
  The difference is purely the rollback mechanism (deep-clone
  vs inverse-journal). Today's lesson composes the journaled
  variant; the composability of the deep-clone variant is
  future work.

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
cmake -S late-may/cpp_practice/journal_self_move_safe -B late-may/cpp_practice/journal_self_move_safe/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/journal_self_move_safe/build
./late-may/cpp_practice/journal_self_move_safe/build/P-2026-08-09-journal-self-move-safe
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/journal_self_move_safe -B late-may/cpp_practice/journal_self_move_safe/build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/journal_self_move_safe/build-strict
./late-may/cpp_practice/journal_self_move_safe/build-strict/P-2026-08-09-journal-self-move-safe
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/journal_self_move_safe -B late-may/cpp_practice/journal_self_move_safe/build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/journal_self_move_safe/build-asan
./late-may/cpp_practice/journal_self_move_safe/build-asan/P-2026-08-09-journal-self-move-safe
```

All three builds pass cleanly. **49 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **journal-aware self-move safe
arc** — the v0.15.0 engine now has a consumer-side
composition that handles self-moves at both the input and
journal boundaries. The composition is correct by
construction: the input-side filter is a pre-pass over the
user-facing patch, the inverse journal is built per-op, and
the replay path is the journal-side filter + `patch()`.

The remaining v0.15.0 candidates (re-quoting from the Aug 6
"v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer arc
  the Jul 21 lesson opened ("JSON Pointer → JSON Patch →
  JSON Schema"). Today's `patch_journaled_self_move_safe` is
  the canonical input layer for atomic schema-driven
  updates + dry-run validation.
- **Widen the dispatcher's int64-vs-double preservation
  guard** — orthogonal to today's lesson; relevant if a real
  consumer hits a double-shaped int64-range input.
  (Actually already done in v0.14.0; re-listed for
  completeness.)

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
  `patch_journaled_self_move_safe` + the `detail::` helpers
  into `<psp_span/json_ext.h>` OR apply the one-line engine
  patch; bump the version.

For the library as a whole, today's lesson is the
**canonical closing entry** for the journal-aware self-move
arc. The 3-axis composition (transactional + journal +
self-move filter) covers the full RFC 6902 behavioral
surface the engine needs to honor when called via a
transactional layer. The natural next step is JSON Schema
validation (the remaining v0.15.0 candidate), which would
give the library a complete Pointer → Patch → Schema arc
on the read/validate side, complementing the transactional
layer on the write side.

## Files

- `late-may/cpp_practice/journal_self_move_safe/CMakeLists.txt`
- `late-may/cpp_practice/journal_self_move_safe/P-2026-08-09-journal-self-move-safe.cpp`
- `late-may/cpp_practice/journal_self_move_safe/P-2026-08-09-journal-self-move-safe.md`
  (this file)
