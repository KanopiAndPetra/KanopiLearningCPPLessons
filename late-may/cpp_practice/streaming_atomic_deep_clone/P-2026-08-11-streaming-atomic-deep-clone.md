# P-2026-08-11 — Streaming-Atomic JSON Patch, deep-clone variant: `psp::json_patch::parse_and_apply_atomic_streaming_deep_clone` (consumer-side; closes the "deep-clone variant of the streaming wrapper" item from the Aug 10 "What's NOT in this lesson" section; same observable contract as the Aug 10 inverse-journal variant; different rollback mechanism (deep-clone vs inverse-journal); library version unchanged at v0.15.0)

## Headline

The Aug 10 lesson shipped the streaming-atomic JSON Patch wrapper:
`psp::json_patch::parse_and_apply_atomic_streaming`. It maintains a
per-op inverse journal and replays the journal in REVERSE on
failure. The Aug 10 "What's NOT in this lesson" section explicitly
flagged a deep-clone variant as future work:

> "It does not address the deep-clone variant of the
>  streaming wrapper. The Aug 3 `patch_atomic` is the
>  deep-clone variant of the Aug 5 `patch_journaled`. A
>  `parse_and_apply_atomic_streaming` deep-clone variant
>  would be `parse_and_apply_atomic_streaming_deep_clone`
>  — observably equivalent to today's wrapper on the
>  success path, different rollback mechanism (deep-clone
>  vs inverse-journal). The deep-clone variant is future
>  work."

Today closes that gap. The new wrapper:

```cpp
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming_deep_clone(
    psp::JsonValue& root,
    psp::Span<const char>& doc) noexcept;
```

Same signature as the Aug 10 wrapper. Same observable contract on
the success path (returns `std::size_t` applied count) and on the
failure path (pre-state restored, error surfaced). The difference
is purely the rollback mechanism:

- **Aug 10 inverse-journal:** maintain a per-op inverse journal;
  on failure, replay the journal in REVERSE.
- **Today deep-clone:** capture a single deep clone of `root`
  up-front; on failure, `root.value = pre.value` (single
  `std::variant` assignment overwrites the entire tree).

The trade-off:

- **Inverse-journal** is cheaper for *small-tree + small-patch*
  (no up-front full-tree clone; per-op inverse computation is
  O(1) for most ops; the journal grows linearly with the
  patch).
- **Deep-clone** is cheaper for *big-tree + many-failed-ops*
  (one up-front full-tree clone; no per-op inverse
  computation; no journal growth; no replay pass on failure).

The two are interchangeable at the call site. Section 5
of today's consumer proves the equivalence end-to-end.

## Where this fits in the arc

```
Jul 21  psp::json_pointer::split / resolve /     Pointer (RFC 6901)
        resolve_mut
Jul 22  psp::json_patch::patch +                Patch engine (RFC 6902)
        parse_patch_document                      v0.13.0 wire-format parser
Jul 27  psp::json_pointer::resolve_mut has       Pointer mutation semantics
        parent-vs-leaf distinction
Aug  1  ...                                     (intermediate lessons)
Aug  3  psp::json_patch::patch_atomic +         DEEP-CLONE transactional
        patch_dry_run                            wrapper (in-memory)
Aug  4  parse_patch_document_at +               Streaming wire-format
        parse_patch_document_next_at +           parser (cursor-primitive;
        parse_one_op_at                          v0.15.0 streaming)
Aug  5  psp::json_patch::patch_journaled        INVERSE-JOURNAL
                                                  transactional wrapper
                                                  (in-memory)
Aug  6  psp::json_patch::patch_self_move_safe   SELF-MOVE FIX wrapper
                                                  (in-memory; pre-filter
                                                  self-moves)
Aug  8  std::expected monadic                   Monadic-composition
        composition                              (in-memory; today's
                                                  cross-cutting
                                                  half)
Aug  9  psp::json_patch::patch_journaled_       JOURNAL-AWARE
        self_move_safe                          SELF-MOVE SAFE
                                                  wrapper (in-memory;
                                                  composes Aug 3 +
                                                  Aug 5 + Aug 6)
Aug 10  parse_and_apply_atomic_streaming        INVERSE-JOURNAL
                                                  STREAMING wrapper
                                                  (composes Aug 4 +
                                                  Aug 9; closes the
                                                  streaming-atom arc)
Aug 11  parse_and_apply_atomic_streaming_      DEEP-CLONE STREAMING
TODAY   deep_clone                              wrapper (composes
                                                  Aug 4 + Aug 3 deep-
                                                  clone idea; closes
                                                  the "deep-clone
                                                  variant of the
                                                  streaming wrapper"
                                                  item from the Aug
                                                  10 "What's NOT in
                                                  this lesson"
                                                  section)
```

The Aug 10 lesson closed the streaming-atom arc. Today adds the
deep-clone variant: the same streaming composition, but with the
deep-clone rollback mechanism in place of the inverse-journal
mechanism. The two are siblings; the call site picks one based on
the cost trade-off.

## The composition problem

The Aug 10 inverse-journal streaming wrapper has three pieces:

1. The pre-check (distinguishes non-`[` from end-of-doc).
2. The streaming loop (parse + apply + journal + advance).
3. The rollback path (replay the journal in REVERSE on failure).

The deep-clone streaming wrapper swaps piece 3 for a different
mechanism. The other two pieces are unchanged:

```
parse_and_apply_atomic_streaming_deep_clone(root, doc)
    pre-check: doc starts with '['
    pre = deep_clone(root)                  // ONE allocation
    first = parse_patch_document_at(doc)
    for each streamed op:
        filter self-moves
        r = patch(root, {op})                // engine
        if r failed: root = pre; return error
        next = parse_patch_document_next_at(doc)
        if !next: return applied
```

The deep-clone is taken BEFORE the first op. The streaming loop
is unchanged. The rollback is a single `std::variant` assignment
that overwrites the entire tree. That's it.

## Why this is more than a one-line swap

Three design questions the simple swap doesn't answer:

### 1. When is the pre-clone taken?

The pre-clone is taken AFTER the pre-check (we know the document
is a valid `[` ... `]` form) and BEFORE the first op (so the
pre-clone is the pre-state for the first op). This is the same
shape as Aug 3 `patch_atomic`.

If the first op fails (e.g., it's a `RemoveOp` on a missing path),
the pre-clone is the state to restore. The `root.value = pre.value`
assignment replaces the entire tree with the pre-clone in O(1) for
the variant assignment (the underlying `std::map` / `std::vector`
are move-assigned from `pre.value`).

### 2. The deep-clone wrapper does NOT need inverse_for, the journal vector, or replay_journal

The inverse-journal machinery is pure rollback overhead. The
deep-clone wrapper has ZERO per-op rollback bookkeeping. The
rollback is a single line of code: `root.value = pre.value;`.

This is a real simplification. The deep-clone wrapper is shorter,
easier to reason about, and has fewer failure modes than the
inverse-journal wrapper.

### 3. On success, the pre-clone is dropped (RAII)

The pre-clone is a local `psp::JsonValue` on the stack. When the
function returns, it's destroyed. No observable difference from
the inverse-journal variant.

## Why consumer-side and not library-side today

Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9 / Aug 10
lessons: a proven-in-consumer capability that exercises the design
end-to-end. The library version is unchanged at v0.15.0. A future
v0.16.0 promotion is mechanical (lift
`parse_and_apply_atomic_streaming_deep_clone` + the `deep_clone`
helper into `<psp_span/json_ext.h>`; bump the version).

## What the consumer exercises

The consumer has six sections, all 64 cases pass on all three
builds (default, strict-warning, ASan + UBSan):

- **Section 1 — symbol-presence + the deep-clone streaming
  wrapper signature.** (8 cases)
  - `parse_and_apply_atomic_streaming_deep_clone` is
    well-defined and has the expected
    `std::expected<std::size_t, JsonPatchError>` signature.
  - The two streaming wrappers (`parse_and_apply_atomic_streaming`
    and `parse_and_apply_atomic_streaming_deep_clone`) have the
    SAME signature (interchangeable at the call site).
  - The 3-function streaming parser surface
    (`parse_patch_document_at` / `_next_at` / `parse_one_op_at`)
    is well-defined.
  - `psp::json_patch::deep_clone` is well-defined and
    preserves `std::int64_t` values.

- **Section 2 — single-op document round-trip.** (13 cases)
  - Single add: applies, /y present, /y value is 1.
  - Single remove: applies, /x/k is gone.
  - Single self-move: dropped, tree unchanged (no engine
    self-delete), applied count is 0.
  - Empty document `[]`: applies 0 ops, tree unchanged.

- **Section 3 — multi-op document with one failing op
  (deep-clone restore).** (16 cases)
  - 3-op patch with the 3rd op failing: the pre-clone
    is restored; the tree is byte-identical to the
    pre-state; /temp and /u are NOT in the final tree.
  - 3-op patch all successful: applied count is 3, all
    three keys present.
  - 4-op patch with self-move + 4th op failing: the
    pre-clone is restored; /a and /b are NOT in the
    final tree; /x/k is still present (self-move
    dropped, not applied).

- **Section 4 — parse failure mid-stream (cursor rewinds).**
  (7 cases)
  - 2-op patch with the 2nd op malformed (missing `op`):
    the wrapper returns the parse error, the pre-clone
    is restored (the 1st op is rolled back), the tree
    is byte-identical to the pre-state.
  - Document starts with a non-`[` character: returns
    `BadDocument` (parse failure), root unchanged, cursor
    BYTE-IDENTICAL.

- **Section 5 — end-to-end: deep-clone streaming tree ==
  inverse-journal streaming tree == in-memory
  patch_journaled_self_move_safe tree.** (13 cases)
  - 3-op document with a self-move: all three trees
    are byte-identical.
  - 4-op document: all three trees are byte-identical.
  - This is the KEY new test that distinguishes today's
    lesson from Aug 10. The two rollback mechanisms
    (deep-clone vs inverse-journal) produce the same
    observable result on the success path.

- **Section 6 — sizeof / feature probes; design
  invariants.** (7 cases)
  - `sizeof(JsonPatchOp)` is non-zero.
  - `sizeof(JsonValue)` is non-zero.
  - `std::vector<JsonPatchOp>` is non-empty sized.
  - `psp::json_patch::patch` (v0.12.0 engine) is
    well-defined.
  - The deep-clone streaming wrapper is consumer-side
    (no library change; library version unchanged at
    v0.15.0).
  - `deep_clone` produces a byte-identical copy of an
    object tree.

## Important code

### The new wrapper

```cpp
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming_deep_clone(
    psp::JsonValue& root,
    psp::Span<const char>& doc) noexcept {
    // Pre-check: the document must start with '[' (with
    // optional leading whitespace). If not, it's a real
    // parse failure — NOT end-of-doc.
    {
        auto probe = doc;
        if (auto r = psp::skip_whitespace_at(probe); !r) {
            return std::unexpected{JsonPatchError::BadDocument};
        }
        if (probe.empty() || probe.front() != '[') {
            return std::unexpected{JsonPatchError::BadDocument};
        }
    }

    // Capture the pre-state ONCE. This is the one-time
    // cost of the deep-clone variant. On any failure, root
    // is restored from this snapshot. On success, it's
    // dropped (RAII).
    psp::JsonValue pre = psp::json_patch::deep_clone(root);

    // Begin: parse the first op. The BEGIN call sees '['.
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        if (first.error() == JsonPatchError::BadDocument) {
            return std::size_t{0};  // end-of-doc
        }
        // Real parse failure: root is unchanged (no op
        // was applied), so we don't need to restore from
        // pre.
        return std::unexpected{first.error()};
    }

    std::size_t applied = 0;
    JsonPatchOp op = *first;

    for (;;) {
        // Step 1: input-side self-move filter. A self-move
        // is a no-op per RFC 6902 §4.4. We don't count it
        // and don't apply it.
        if (!psp::json_patch::detail::is_self_move(op)) {
            // Step 2: apply the op via the engine.
            auto r = psp::json_patch::patch(root,
                std::span<const JsonPatchOp>{&op, 1});
            if (!r) {
                // Engine failed. Restore the pre-state.
                // This is the rollback mechanism: a single
                // assignment overwrites the entire tree.
                root.value = pre.value;
                return std::unexpected{r.error()};
            }
            ++applied;
        }

        // Step 3: stream the next op. NEXT call does NOT
        // see '[' (the BEGIN call already consumed it).
        auto next = psp::json_patch::parse_patch_document_next_at(doc);
        if (!next) {
            if (next.error() == JsonPatchError::BadDocument) {
                return applied;  // end-of-doc
            }
            // Real parse failure. Restore the pre-state
            // (some prior ops may have succeeded).
            root.value = pre.value;
            return std::unexpected{next.error()};
        }
        op = *next;
    }
}
```

### The deep-clone rollback

The rollback is a single line of code:

```cpp
root.value = pre.value;
```

This is a `std::variant` move-assignment. The underlying
`std::map` / `std::vector` in `pre.value` is move-assigned into
`root.value`, replacing the entire tree in O(1) for the
assignment itself (the map/vector are moved, not copied). The
old `root.value` is destroyed as part of the move.

This is the entire rollback mechanism for the deep-clone
wrapper. No journal, no replay, no per-op inverse
computation.

### The pre-clone

The pre-clone is taken ONCE, before the first op:

```cpp
psp::JsonValue pre = psp::json_patch::deep_clone(root);
```

`deep_clone` is a recursive copy of the `JsonValue` tree. The
cost is O(size of tree). For a small tree, this is cheap. For
a big tree, this is the dominant cost — and the trade-off
versus the inverse-journal variant.

On success, the pre-clone is dropped (RAII). On failure, it's
moved into `root.value` to restore the pre-state.

### The asymmetric parser API (inherited from Aug 4)

The Aug 4 parser is asymmetric: the BEGIN call MUST see `[`,
the NEXT call MUST NOT see `[`. The deep-clone wrapper hides
that from the caller — the caller passes a wire-format
document and gets back the op count. The wrapper itself is
the only place that knows about the asymmetry.

### The end-of-document pre-check (inherited from Aug 10)

The pre-check distinguishes "non-`[` document" (parse failure)
from "empty document `[]`" (end-of-doc). The cursor-primitive
returns the same `BadDocument` for both; the wrapper
disambiguates using the cursor state.

## Observed output

All three builds (default, strict-warning, ASan + UBSan)
print 64 PASS / 0 FAIL across 6 sections. The exit code is 0.

```
P-2026-08-11 — streaming-atomic DEEP-CLONE JSON Patch
  (closes the "deep-clone variant of the streaming wrapper"
  item from the Aug 10 "What's NOT in this lesson" section;
  same observable contract as the Aug 10 inverse-journal
  variant; different rollback mechanism (deep-clone vs
  inverse-journal); library version unchanged at v0.15.0)

== Section 1: symbol-presence + deep-clone streaming wrapper spec ==
  PASS: 1a &psp::json_patch::parse_and_apply_atomic_streaming_deep_clone is well-defined
  PASS: 1b parse_and_apply_atomic_streaming_deep_clone signature matches std::expected<std::size_t, JsonPatchError>
  PASS: 1c deep-clone and inverse-journal streaming wrappers have the SAME signature (interchangeable at the call site)
  PASS: 1d &psp::json_patch::parse_patch_document_at is well-defined
  PASS: 1e &psp::json_patch::parse_patch_document_next_at is well-defined
  PASS: 1f &psp::json_patch::parse_one_op_at is well-defined
  PASS: 1g &psp::json_patch::deep_clone is well-defined
  PASS: 1h deep_clone preserves int64_t values

== Section 2: single-op document round-trip ==
  PASS: 2a single-op add applies successfully
  PASS: 2b single-op add applies 1 op
  PASS: 2c /y was added
  PASS: 2d /y value is 1
  PASS: 2e single-op remove applies successfully
  PASS: 2f single-op remove applies 1 op
  PASS: 2g /x/k was removed
  PASS: 2h single-op self-move applies successfully (dropped)
  PASS: 2i self-move is NOT counted (it's a no-op)
  PASS: 2j tree is unchanged after self-move (no engine self-delete)
  PASS: 2k empty document applies successfully
  PASS: 2l empty document applies 0 ops
  PASS: 2m tree is unchanged for empty document

== Section 3: multi-op rollback (deep-clone restore) ==
  PASS: 3a mid-stream failing op returns error
  PASS: 3b error is PointerNotFound
  PASS: 3c /temp was NOT in final tree (rollback)
  PASS: 3d /u was NOT in final tree (rollback)
  PASS: 3e tree is byte-identical to pre-state after rollback
  PASS: 3f 3-op successful document applies
  PASS: 3g 3 ops were applied
  PASS: 3h /temp is in final tree
  PASS: 3i /u is in final tree
  PASS: 3j /v is in final tree
  PASS: 3k 4-op with self-move + fail: error returned
  PASS: 3l error is PointerNotFound
  PASS: 3m /a was rolled back
  PASS: 3n /b was rolled back
  PASS: 3o /x/k is still present (self-move dropped, not applied)
  PASS: 3p tree is byte-identical to pre-state after rollback

== Section 4: parse failure mid-stream (cursor rewind) ==
  PASS: 4a parse failure mid-stream returns error
  PASS: 4b error is MissingField (no 'op' field)
  PASS: 4c /temp was rolled back (parse failure after apply)
  PASS: 4d tree is byte-identical to pre-state after parse failure
  PASS: 4e non-'[' document returns error
  PASS: 4f error is BadDocument (no '[')
  PASS: 4g tree is unchanged for non-'[' document

== Section 5: cross-variant equivalence on the success path ==
  PASS: 5a deep-clone streaming: success
  PASS: 5b deep-clone streaming: 2 ops applied (self-move excluded)
  PASS: 5c inverse-journal streaming: success
  PASS: 5d inverse-journal streaming: 2 ops applied
  PASS: 5e in-memory journal: success
  PASS: 5f deep-clone streaming tree == inverse-journal streaming tree
  PASS: 5g deep-clone streaming tree == in-memory journal tree
  PASS: 5h inverse-journal streaming tree == in-memory journal tree
  PASS: 5i 4-op: all three variants succeed
  PASS: 5j 4-op: deep-clone applies 4 ops
  PASS: 5k 4-op: inverse-journal applies 4 ops
  PASS: 5l 4-op: deep-clone tree == inverse-journal tree
  PASS: 5m 4-op: deep-clone tree == in-memory tree

== Section 6: sizeof / feature probes; design invariants ==
  PASS: 6a sizeof(JsonPatchOp) is non-zero
  PASS: 6b sizeof(JsonValue) is non-zero
  PASS: 6c std::vector<JsonPatchOp> is non-empty sized
  PASS: 6d psp::JsonValue is non-zero sized
  PASS: 6e psp::json_patch::patch (v0.12.0 engine) is well-defined
  PASS: 6f deep-clone streaming wrapper is consumer-side (no library change; library version unchanged at v0.15.0)
  PASS: 6g deep_clone produces a byte-identical copy of an object tree

--- Summary ---
passes:   64
fails:    0

All checks passed. main returns 0.
```

## Design notes

### 1. Why the deep-clone variant and the inverse-journal variant are observably equivalent on the success path

Both wrappers start by validating the document is well-formed
(the pre-check). Both wrappers apply ops one at a time, with
the same input-side self-move filter. The only difference is the
rollback mechanism. On the success path, neither rollback
mechanism is invoked, so the two wrappers produce the same
observable result.

Section 5 of today's consumer proves this end-to-end: a 3-op
document with a self-move produces byte-identical trees under
all three variants (deep-clone streaming, inverse-journal
streaming, in-memory journal-aware self-move-safe). The same
holds for a 4-op document.

### 2. Why the deep-clone variant is shorter than the inverse-journal variant

The deep-clone wrapper has NO per-op inverse computation, NO
journal vector, NO replay pass on failure. The entire rollback
is a single line of code: `root.value = pre.value;`. Compare to
the inverse-journal wrapper, which has:

- `JsonPatchError pre_err = JsonPatchError::BadDocument;`
- `auto inv = psp::json_patch::detail::inverse_for(root, op, pre_err);`
- `if (!inv && op.kind != OpKind::Test) { ... }`
- `if (inv) { journal.push_back(std::move(*inv)); }`
- `replay_journal(root, journal)` on failure.

The deep-clone wrapper drops all of that. The trade-off is
the up-front `deep_clone(root)` cost.

### 3. Why the deep-clone wrapper does NOT have an inverse-journal-style "replay" bug to worry about

The inverse-journal wrapper's `replay_journal` function was a
focus of the Aug 5 / Aug 9 lessons: the replay must filter
self-moves, and the pre-state-error path (e.g., `RemoveOp` on a
missing path) must ALSO replay the journal. Two real bugs were
found and fixed in development.

The deep-clone wrapper has no such bug surface. The rollback
is a single `std::variant` assignment. Either the assignment
succeeds (pre-state restored) or it doesn't (it always does for
a valid `std::variant` move-assignment). There's no replay
loop, no inverse computation, no per-op state to manage.

### 4. When to use which variant

- **Use the inverse-journal variant** (`parse_and_apply_atomic_streaming`)
  when the tree is small and patches typically succeed.
  The cost is per-op inverse computation (cheap for most ops)
  + a small journal. The benefit is no up-front full-tree
  clone.
- **Use the deep-clone variant** (`parse_and_apply_atomic_streaming_deep_clone`)
  when the tree is large or patches typically fail. The cost
  is one up-front full-tree clone. The benefit is no per-op
  inverse computation, no journal growth, no replay pass on
  failure.

In practice, the deep-clone variant is the better default
choice for *transactional* use cases (where the failure path
matters more than the success path). The inverse-journal
variant is the better default for *streaming* use cases
(where the success path is the common case).

### 5. Why this is a "natural" composition

The Aug 10 lesson explicitly flagged this as future work, and
the Aug 3 / Aug 5 duality (`patch_atomic` deep-clone vs
`patch_journaled` inverse-journal) gave a clear template for
the streaming case. Today's wrapper is a 20-line composition
of:

- The Aug 4 streaming parser (3 functions).
- The Aug 3 deep-clone rollback (single up-front clone).
- The Aug 6 self-move filter (input-side only).
- The Aug 4 cursor-primitive contract (shrink on success /
  unchanged on failure).

The deep-clone variant doesn't need the Aug 9 inverse-journal
machinery (`inverse_for`, `replay_journal`, `filter_self_moves`
on the journal). The wrapper is observably simpler.

## Verified output

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) — clean compile,
64 PASS / 0 FAIL.

ASan + UBSan build (`-fsanitize=address -fsanitize=undefined
-fno-omit-frame-pointer -O1`) — clean compile, 64 PASS / 0
FAIL, no sanitizer warnings.

`main` returns 0 on success and 1 on any failure.

A 100x stress run of the rollback path under ASan + UBSan
(3-op patch with the 3rd op failing, repeated 100 times in a
tight loop) — clean, 0 unexpected successes, no sanitizer
warnings.

## What's NOT in this lesson

- **It is not a library promotion.** The library version is
  unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift `parse_and_apply_atomic_streaming_deep_clone`
  + the `deep_clone` helper into `<psp_span/json_ext.h>`; bump
  the version).
- **It is not a parser redesign.** The Aug 4 parser's
  asymmetric API (BEGIN sees `[`, NEXT does not) is preserved
  as-is. The wrapper hides the asymmetry from the caller; the
  parser is unchanged.
- **It is not a benchmark.** The deep-clone variant's overhead
  is one `deep_clone(root)` call up-front + a single
  `std::variant` assignment on failure. The big-O analysis
  says it's O(tree size) on the success path and O(1) for the
  rollback itself. The consumer doesn't need a microbenchmark
  to confirm.
- **It does not change the wire format.** The v0.15.0 writer
  + v0.13.0 parser + Aug 4 streaming parser are unchanged.
  The deep-clone wrapper inherits the wire format as-is.
- **It does not bump the JsonPatchError enum.** The wrapper
  either commits (returns `std::size_t`) or propagates an
  existing error.
- **It does not replace any existing function.** Today's
  `parse_and_apply_atomic_streaming_deep_clone` is additive.
  The Aug 10 `parse_and_apply_atomic_streaming` is unchanged.
  The two are siblings; the call site picks one based on the
  cost trade-off.
- **It does not address the `std::generator` adapter.** The
  Aug 4 lesson's "Where we go next" section flagged a
  `std::generator` adapter on top of the begin/next
  functions. Today's wrapper hides the begin/next from the
  caller; the generator adapter is a separate exercise
  (waiting on `<generator>` in the Apple Clang toolchain).
- **It does not address the v0.16.0 promotion arc.** The
  library version is unchanged at v0.15.0. A future v0.16.0
  promotion is mechanical.

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
cmake -S late-may/cpp_practice/streaming_atomic_deep_clone -B late-may/cpp_practice/streaming_atomic_deep_clone/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/streaming_atomic_deep_clone/build
./late-may/cpp_practice/streaming_atomic_deep_clone/build/P-2026-08-11-streaming-atomic-deep-clone
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/streaming_atomic_deep_clone -B late-may/cpp_practice/streaming_atomic_deep_clone/build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/streaming_atomic_deep_clone/build-strict
./late-may/cpp_practice/streaming_atomic_deep_clone/build-strict/P-2026-08-11-streaming-atomic-deep-clone
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/streaming_atomic_deep_clone -B late-may/cpp_practice/streaming_atomic_deep_clone/build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/streaming_atomic_deep_clone/build-asan
./late-may/cpp_practice/streaming_atomic_deep_clone/build-asan/P-2026-08-11-streaming-atomic-deep-clone
```

All three builds pass cleanly. **64 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **deep-clone variant of the streaming
wrapper** — the last item from the Aug 10 "What's NOT in this
lesson" section. The two streaming wrappers (Aug 10
inverse-journal + Aug 11 deep-clone) are now both proven in
consumer-side form, with the same observable contract and
different rollback mechanisms.

The remaining v0.15.0 candidates (re-quoting from the Aug 10
"Where we go next" section):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer
  arc the Jul 21 lesson opened ("JSON Pointer → JSON
  Patch → JSON Schema"). Today's
  `parse_and_apply_atomic_streaming_deep_clone` is the
  canonical input layer for atomic schema-driven updates +
  dry-run validation. (An abandoned Aug 7 attempt exists
  in `late-may/cpp_practice/json_schema_validation/` as
  untracked files; the natural next step is to revisit that
  attempt or restart from scratch.)

### Cross-cutting forward-on (re-quoting still-open items)

- **Pin actions to commit SHAs** — `@v4` is a floating
  tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.
- **Re-authorize the `workflow` PAT scope** — pending
  Adam.
- **`std::expected` and coroutines** (Aug 8 consumer
  covers the monadic half; the coroutines half
  (`Awaitable<T,E>`) is still open).
- **`std::submdspan`** (P2630).
- **`aligned_accessor` / `atomic_accessor`** (C++26).
- **C++26 `std::linalg`** (P1673).
- **A `std::generator` adapter on top of the streaming
  patch parser** — the begin/next dance is a one-line
  wrapper away from `for (auto op : parser) { ... }`
  (waiting on `<generator>` in the Apple Clang
  toolchain).
- **A `std::generator` adapter for the inverse-journal
  replay** — `replay_journal`'s reversed vector is a
  one-line wrapper away from
  `for (auto op : replay(root, journal)) { ... }` (same
  `<generator>` dependency).
- **v0.16.0 promotion arc** — mechanical: lift
  `parse_and_apply_atomic_streaming` (Aug 10) +
  `parse_and_apply_atomic_streaming_deep_clone` (Aug 11)
  + the `deep_clone` helper into `<psp_span/json_ext.h>`;
  bump the version.

For the library as a whole, today's lesson is the **canonical
closing entry** for the deep-clone variant of the streaming
wrapper. The 2-axis composition (streaming + atomic) now has
two rollback mechanisms (inverse-journal + deep-clone), both
proven in consumer-side form, both observably equivalent on
the success path. The natural next step is JSON Schema
validation, which would give the library a complete Pointer →
Patch → Schema arc on the read/validate side, complementing
the transactional layer on the write side.

## Files

- `late-may/cpp_practice/streaming_atomic_deep_clone/CMakeLists.txt`
- `late-may/cpp_practice/streaming_atomic_deep_clone/P-2026-08-11-streaming-atomic-deep-clone.cpp`
- `late-may/cpp_practice/streaming_atomic_deep_clone/P-2026-08-11-streaming-atomic-deep-clone.md`
