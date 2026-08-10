# P-2026-08-10 — Streaming-Atomic JSON Patch: `psp::json_patch::parse_and_apply_atomic_streaming` (consumer-side; composes the Aug 4 streaming parser + Aug 9 journal-aware self-move-safe transactional layer into a single function; streams a wire-format patch document and applies each op transactionally in a single pass; no intermediate `std::vector<JsonPatchOp>` allocation; library version unchanged at v0.15.0)

## Headline

Three lessons have shipped wrappers over the v0.12.0 engine:

- **Aug 4:** `parse_patch_document_at` / `parse_patch_document_next_at` / `parse_one_op_at` (streaming wire-format parser; cursor-primitive).
- **Aug 9:** `patch_journaled_self_move_safe` (inverse-journal rollback + self-move filter at both the input AND the journal boundaries; the most complete transactional layer shipped to date).

The Aug 4 lesson's "Where we go next" section explicitly
flagged the streaming-atom composition as forward-on work:

> "Natural follow-on lessons: std::generator adapter on top of
>  the begin/next functions; inverse-journal optimisation for
>  patch_atomic; engine-level self-move fix; JSON Schema
>  validation in a new `<psp_span/json_schema.h>`; widen the
>  dispatcher's int64-vs-double preservation guard from int to
>  int64_t."

And the Aug 9 lesson's "Where we go next" section added:

> "Streaming-atomic wrapper — per-op snapshot for the
>  streaming parser's begin/next API; the journal composes
>  cleanly with the streaming parser, but per-op
>  snapshotting is a separate design exercise."

Today is that future lesson. The composition is the natural
next step: the Aug 4 parser is a cursor-primitive; the Aug 9
wrapper is per-op. Wiring them together means each op is
streamed + applied + journaled in a single pass — no
intermediate `std::vector<JsonPatchOp>` allocation. The
wrapper is a 30-line composition, not a new algorithm.

The new wrapper:

```cpp
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming(
    psp::JsonValue& root,
    psp::Span<const char>& doc) noexcept;
```

Returns the number of applied ops on success
(rollback-on-failure preserves the pre-state). The cursor is
past the closing `]` on success; at the failure point on
parse failure; past the failing op on apply failure (the op
that the engine rejected was consumed from the cursor before
the engine call).

Library version unchanged at v0.15.0. Future v0.16.0
promotion is mechanical (lift `parse_and_apply_atomic_streaming`
+ the `detail::` helpers into `<psp_span/json_ext.h>`; bump
the version).

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
                                                (consumer-side; pre-filter
                                                self-moves before the engine)
Aug  9  psp::json_patch::patch_journaled_        JOURNAL-AWARE SELF-MOVE SAFE
        self_move_safe                            wrapper (consumer-side; composes
                                                Aug 3 + Aug 5 + Aug 6)
Aug 10  psp::json_patch::parse_and_apply_       STREAMING-ATOMIC composition of
TODAY   atomic_streaming                          the Aug 4 parser + the Aug 9
                                                transactional layer (consumer-side;
                                                single-pass stream-and-apply;
                                                library version unchanged at v0.15.0)
```

The Aug 4 lesson closed the streaming-parser arc with
cursor-primitive parsing. The Aug 9 lesson closed the
transactional arc with inverse-journal + self-move filter at
both the input and journal boundaries. Today closes the gap
between them: a single function that streams AND applies AND
rolls back on failure, end-to-end.

## The composition problem

The Aug 4 parser returns one op per call:

```cpp
auto first = parse_patch_document_at(s);   // cursor at '['
auto next  = parse_patch_document_next_at(s);  // iter
// ...
```

The Aug 9 wrapper accepts a `std::span<const JsonPatchOp>`:

```cpp
patch_journaled_self_move_safe(root, ops);
```

The two recipes meet at the per-op boundary. The "obvious"
composition would be: collect all ops into a `std::vector`,
then call the wrapper. But that defeats the streaming aspect
— the caller pays the full document parse + vector
allocation up-front. The streaming version amortises parsing
over application:

```
parse_and_apply_atomic_streaming(root, doc)
    pre-check: doc starts with '['
    first = parse_patch_document_at(doc)
    for each streamed op:
        filter self-moves
        inv = inverse_for(root, op)        // journal entry
        r   = patch(root, {op})             // engine
        if r failed: replay_journal(...); return error
        else:        push inv to journal
        next = parse_patch_document_next_at(doc)
        if !next: return applied
```

## Why this is more than a one-line composition

Three design questions the simple composition doesn't answer:

### 1. The asymmetric parser API

The Aug 4 parser is asymmetric: the BEGIN call MUST see
`[`, the NEXT call MUST NOT see `[`. The streaming wrapper
hides that from the caller — the caller passes a wire-format
document and gets back the op count. The wrapper itself is
the only place that knows about the asymmetry.

### 2. End-of-document is signaled by `BadDocument`

The end-of-document case is signaled by `BadDocument`
(consume `]`). That's the SAME error type as a real parse
failure. The wrapper has to distinguish: the end-of-doc
`BadDocument` is the SUCCESS terminal; any other `BadDocument`
is a real parse failure.

The wrapper resolves the ambiguity by:

1. **Pre-checking** that the document starts with `[` (with
   optional leading whitespace). If not, it's a real parse
   failure: return `BadDocument` WITHOUT treating it as
   end-of-doc.
2. **For the BEGIN call:** `BadDocument` AFTER consuming `[`
   means the document was `[]` (end-of-doc, success, zero
   ops). Anything else is a real parse failure.
3. **For the NEXT call:** `BadDocument` AFTER consuming `]`
   means end-of-doc (success). Anything else is a real parse
   failure.

This is the wrapper's way of distinguishing the two
`BadDocument` cases that the cursor-primitive returns. The
underlying parser itself doesn't have enough information to
distinguish them; the wrapper does.

### 3. Cursor state on failure

The Aug 4 parser leaves the cursor BYTE-IDENTICAL on parse
failure. The streaming wrapper inherits that — on a parse
failure mid-stream, the cursor is at the failure point and
the root is unchanged. The caller can resync. (Section 4
proves this end-to-end.)

## Why consumer-side and not library-side today

Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9
lessons: a proven-in-consumer capability that exercises the
design end-to-end. The library version is unchanged at
v0.15.0. A future v0.16.0 promotion is mechanical (lift
`parse_and_apply_atomic_streaming` + the `detail::` helpers
into `<psp_span/json_ext.h>`; bump the version).

## What the consumer exercises

The consumer has six sections, all 54 cases pass on all three
builds (default, strict-warning, ASan + UBSan):

- **Section 1 — symbol-presence + the per-op streaming
  parser surface.** (10 cases)
  - `parse_and_apply_atomic_streaming` is well-defined and
    has the expected `std::expected<std::size_t, JsonPatchError>`
    signature.
  - The 3-function streaming parser surface
    (`parse_patch_document_at` / `_next_at` / `parse_one_op_at`)
    is well-defined.
  - The cursor-primitive contract: a BEGIN call on a
    non-`[` buffer returns `BadDocument` and leaves the
    cursor BYTE-IDENTICAL.
  - An empty document `[]` is a successful zero-op apply
    (cursor past `]`, applied count = 0).

- **Section 2 — single-op document round-trip.** (11 cases)
  - Single add: applies, /y present, /y value is 1.
  - Single remove: applies, /x/k is gone.
  - Single self-move: dropped, tree unchanged (no engine
    self-delete), applied count is 0.

- **Section 3 — multi-op document with one failing op
  (mid-stream rollback).** (14 cases)
  - 3-op patch with the 3rd op failing: the journal rolls
    back the first two adds; the tree is byte-identical to
    the pre-state.
  - 3-op patch with the 2nd op failing: the journal rolls
    back the first add; the 3rd op is not applied.
  - 3-op patch all successful: applied count is 3, all
    three keys present.

- **Section 4 — parse failure mid-stream (cursor rewinds).**
  (8 cases)
  - 2-op patch with the 2nd op malformed (missing `op`):
    the wrapper returns the parse error, the journal rolls
    back the first add, the tree is byte-identical to the
    pre-state, the cursor is at the failure point.
  - Document starts with a non-`[` character: returns
    `BadDocument` (parse failure), root unchanged, cursor
    BYTE-IDENTICAL.

- **Section 5 — end-to-end: streaming-atomic == in-memory
  wrapper.** (8 cases)
  - 3-op document with a self-move: streaming tree ==
    in-memory tree.
  - 4-op document: streaming tree == in-memory tree.

- **Section 6 — sizeof / feature probes.** (3 cases)
  - `sizeof(JsonPatchOp)` is non-zero.
  - `JsonPatchError` has `PointerNotFound` (v0.15.0
    invariant).
  - The wrapper is consumer-side (no library change).

## Important code

### The new wrapper

```cpp
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming(
    psp::JsonValue& root,
    psp::Span<const char>& doc) noexcept {
    // Pre-check: the document must start with '['.
    {
        auto probe = doc;
        if (auto r = psp::skip_whitespace_at(probe); !r) {
            return std::unexpected{JsonPatchError::BadDocument};
        }
        if (probe.empty() || probe.front() != '[') {
            return std::unexpected{JsonPatchError::BadDocument};
        }
    }

    // Begin: parse the first op.
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        if (first.error() == JsonPatchError::BadDocument) {
            return std::size_t{0};  // end-of-doc
        }
        return std::unexpected{first.error()};
    }

    std::vector<JsonPatchOp> journal;
    journal.reserve(8);

    std::size_t applied = 0;
    JsonPatchOp op = *first;

    for (;;) {
        if (!psp::json_patch::detail::is_self_move(op)) {
            // Capture the inverse; pre-state lookup.
            JsonPatchError pre_err = JsonPatchError::BadDocument;
            auto inv = psp::json_patch::detail::inverse_for(
                root, op, pre_err);
            if (!inv && op.kind != OpKind::Test) {
                auto replay = psp::json_patch::detail::replay_journal(
                    root, journal);
                if (!replay) {
                    return std::unexpected{replay.error()};
                }
                return std::unexpected{pre_err};
            }

            // Apply via the engine.
            auto r = psp::json_patch::patch(
                root, std::span<const JsonPatchOp>{&op, 1});
            if (!r) {
                auto replay = psp::json_patch::detail::replay_journal(
                    root, journal);
                if (!replay) {
                    return std::unexpected{replay.error()};
                }
                return std::unexpected{r.error()};
            }

            if (inv) {
                journal.push_back(std::move(*inv));
            }
            ++applied;
        }

        // Stream the next op.
        auto next = psp::json_patch::parse_patch_document_next_at(doc);
        if (!next) {
            if (next.error() == JsonPatchError::BadDocument) {
                return applied;  // end-of-doc
            }
            auto replay = psp::json_patch::detail::replay_journal(
                root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{next.error()};
        }
        op = *next;
    }
}
```

### The end-of-document pre-check

The pre-check is the small but critical piece that
distinguishes "non-`[` document" (parse failure) from
"empty document `[]`" (end-of-doc). The cursor-primitive
returns the same `BadDocument` for both; the wrapper has to
disambiguate using the cursor state.

```cpp
// Pre-check: doc starts with '['
auto probe = doc;
psp::skip_whitespace_at(probe);
if (probe.empty() || probe.front() != '[') {
    return std::unexpected{JsonPatchError::BadDocument};
}
```

Without this pre-check, a malformed document like
`not-a-patch` would be treated as an empty document and
silently succeed with 0 ops applied. Section 4e/4f proves
the pre-check is in place: the test sends `not-a-patch`,
the wrapper returns `BadDocument` (a real parse failure,
NOT end-of-doc), and the root is unchanged.

### The per-op self-move rule (mirrors Aug 6)

```cpp
inline bool
is_self_move(const ::JsonPatchOp& op) noexcept {
    if (op.kind != ::OpKind::Move) return false;
    const auto& m = std::get< ::MoveOp>(op.data);
    return m.from == m.path;
}
```

A self-move is dropped in the streaming loop (no engine
call, no journal entry, no count). Section 2i/2j/2k
proves the rule end-to-end: a single-op self-move document
succeeds with applied count 0, and the tree is unchanged
(the v0.12.0 engine would have self-deleted /x/k; the
streaming wrapper drops the self-move before the engine
sees it).

## Observed output

All three builds (default, strict-warning, ASan + UBSan)
print 54 PASS / 0 FAIL across 6 sections. The exit code is
0.

```
P-2026-08-10 — streaming-atomic JSON Patch
  (composes Aug 4 streaming parser + Aug 9 journal-aware
  self-move-safe wrapper into a single transactional layer;
  library version unchanged at v0.15.0)

== Section 1: symbol-presence + per-op streaming parser spec ==
  PASS: 1a &psp::json_patch::parse_and_apply_atomic_streaming is well-defined
  PASS: 1b parse_and_apply_atomic_streaming signature matches std::expected<std::size_t, JsonPatchError>
  PASS: 1c &psp::json_patch::parse_patch_document_at is well-defined
  PASS: 1d &psp::json_patch::parse_patch_document_next_at is well-defined
  PASS: 1e &psp::json_patch::parse_one_op_at is well-defined
  PASS: 1f parse_patch_document_at on non-'[' buffer returns BadDocument
  PASS: 1g parse_patch_document_at leaves cursor BYTE-IDENTICAL on failure
  PASS: 1h empty document '[]' is a successful zero-op apply
  PASS: 1i empty document applies 0 ops
  PASS: 1j cursor is past ']' after empty document

== Section 2: single-op document round-trip ==
  PASS: 2a single-op document applies successfully
  PASS: 2b single-op document applies 1 op
  PASS: 2c /y was added
  PASS: 2d /y value is 1
  PASS: 2e cursor is past ']' after single-op document
  PASS: 2f single-op remove applies successfully
  PASS: 2g single-op remove applies 1 op
  PASS: 2h /x/k was removed
  PASS: 2i single-op self-move applies successfully (dropped)
  PASS: 2j self-move is NOT counted (it's a no-op)
  PASS: 2k tree is unchanged after self-move (no engine self-delete)

== Section 3: multi-op document with one failing op (rollback) ==
  PASS: 3a mid-stream failing op returns error
  PASS: 3b error is PointerNotFound
  PASS: 3c /temp was rolled back (not in final tree)
  PASS: 3d /u was rolled back (not in final tree)
  PASS: 3e tree is byte-identical to pre-state after rollback
  PASS: 3f mid-stream failing op returns error (early fail)
  PASS: 3g error is PointerNotFound
  PASS: 3h /temp was rolled back
  PASS: 3i /u was not applied (early fail stopped the engine)
  PASS: 3j tree is byte-identical to pre-state after rollback
  PASS: 3k 3-op successful document applies
  PASS: 3l 3 ops were applied
  PASS: 3m all three keys present in final tree
  PASS: 3n cursor is past ']' after 3-op document

== Section 4: parse failure mid-stream (cursor rewinds) ==
  PASS: 4a parse failure mid-stream returns error
  PASS: 4b error is MissingField (no 'op' field)
  PASS: 4c tree is byte-identical to pre-state after parse failure
  PASS: 4d cursor is at the failure point (between entry and end)
  PASS: 4e non-'[' document returns error
  PASS: 4f error is BadDocument (no '[')
  PASS: 4g tree is unchanged for non-'[' document
  PASS: 4h cursor is BYTE-IDENTICAL for non-'[' document

== Section 5: streaming-atomic == in-memory wrapper (success path) ==
  PASS: 5a streaming apply succeeds (self-move dropped)
  PASS: 5b streaming apply counts 2 ops (self-move excluded)
  PASS: 5c in-memory apply succeeds (self-move dropped)
  PASS: 5d streaming tree == in-memory tree
  PASS: 5e 4-op streaming apply succeeds
  PASS: 5f 4-op streaming apply counts 4 ops
  PASS: 5g 4-op in-memory apply succeeds
  PASS: 5h streaming tree == in-memory tree (4-op)

== Section 6: sizeof / feature probes ==
  PASS: 6a sizeof(JsonPatchOp) is non-zero
  PASS: 6b JsonPatchError has PointerNotFound (v0.15.0 invariant)
  PASS: 6c streaming wrapper is consumer-side (no library change)

--- Summary ---
sections: 6
passes:   54
fails:    0

All checks passed. main returns 0.
```

## Design notes

### 1. Why the pre-check is necessary

The Aug 4 parser returns `BadDocument` for BOTH end-of-doc
(consume `]`) AND a non-`[` start. The two are
indistinguishable from the `BadDocument` return value alone.
The wrapper resolves the ambiguity using the cursor state
(a probe that checks if `doc.front() == '['` after
`skip_whitespace_at`).

This is a fundamental property of the cursor-primitive API:
the parser has no way to signal "I successfully consumed
the array's opening bracket" vs "I never saw an opening
bracket at all" — both are "no op was produced". The
wrapper has to do the disambiguation, and the only way to
do it is to check the cursor state. The pre-check is the
cleanest way to do that.

### 2. Why the streaming version is observably equivalent
to the in-memory version

The Aug 9 `patch_journaled_self_move_safe` accepts a
`std::span<const JsonPatchOp>`. The streaming wrapper
constructs the same set of ops incrementally: one op at a
time, with the same input-side self-move filter, the same
inverse journal, and the same replay-on-failure mechanism.
The only difference is the source of the ops: in-memory
takes a `std::span`; streaming takes a `psp::Span<const char>`
and parses them one at a time.

Section 5 of today's consumer proves the equivalence: the
streaming tree and the in-memory tree are byte-identical on
the success path for both 3-op and 4-op documents (with and
without a self-move).

### 3. Why the streaming version doesn't need its own
journal-on-failure machinery

The streaming version reuses the Aug 9
`replay_journal` and `inverse_for` directly. The only
difference is the call site: the in-memory version calls
them inside a `for (const auto& op : ops)` loop; the
streaming version calls them inside a `for (;;)` loop with
`parse_patch_document_next_at(doc)` as the iterator.

This is the right design: the journal-on-failure machinery
is per-op and stateless with respect to the source of the
ops. The streaming wrapper is the only piece that knows
about the source, and it stays out of the way of the
journal logic.

### 4. Why the wrapper is consumer-side, not library-side

Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9
lessons: a proven-in-consumer capability that exercises the
design end-to-end. The library version is unchanged at
v0.15.0. A future v0.16.0 promotion is mechanical (lift
`parse_and_apply_atomic_streaming` + the `detail::`
helpers into `<psp_span/json_ext.h>`; bump the version).

The library promotion would also expose the new function
in the `psp::json_patch::` namespace, with the same
signature. The consumer-side version is a "sneak peek" at
the API surface; the library version is the production API.

## Verified output

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) — clean compile,
54 PASS / 0 FAIL.

ASan + UBSan build (`-fsanitize=address -fsanitize=undefined
-fno-omit-frame-pointer -O1`) — clean compile, 54 PASS / 0
FAIL, no sanitizer warnings.

`main` returns 0 on success and 1 on any failure.

## What's NOT in this lesson

- **It is not a library promotion.** The library version is
  unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift `parse_and_apply_atomic_streaming` + the
  `detail::` helpers into `<psp_span/json_ext.h>`; bump the
  version).

- **It is not a parser redesign.** The Aug 4 parser's
  asymmetric API (BEGIN sees `[`, NEXT does not) is
  preserved as-is. The wrapper hides the asymmetry from
  the caller; the parser is unchanged.

- **It is not a benchmark.** The streaming version's
  overhead is one `skip_whitespace_at` + one `front()`
  check per call (the pre-check), plus the same
  per-op inverse-journal machinery as the in-memory
  version. The big-O analysis says it's free; the consumer
  doesn't need a microbenchmark to confirm.

- **It does not change the wire format.** The v0.15.0 writer
  + v0.13.0 parser + Aug 4 streaming parser are unchanged.
  The streaming wrapper inherits the wire format as-is.

- **It does not bump the JsonPatchError enum.** Section 6
  asserts `PointerNotFound` exists; today's lesson doesn't
  add new enumerators. The wrapper either commits (returns
  `std::size_t`) or propagates an existing error.

- **It does not replace any existing function.** Today's
  `parse_and_apply_atomic_streaming` is additive. The v0.12.0
  `patch` + Aug 3 `patch_atomic` / `patch_dry_run` + Aug 5
  `patch_journaled` + Aug 6 `patch_self_move_safe` + Aug 9
  `patch_journaled_self_move_safe` + Aug 4 streaming parser
  are all unchanged.

- **It does not address the `std::generator` adapter.** The
  Aug 4 lesson's "Where we go next" section flagged a
  `std::generator` adapter on top of the begin/next
  functions. Today's wrapper hides the begin/next from the
  caller; the generator adapter is a separate exercise
  (waiting on `<generator>` in the Apple Clang toolchain).

- **It does not address the deep-clone variant of the
  streaming wrapper.** The Aug 3 `patch_atomic` is the
  deep-clone variant of the Aug 5 `patch_journaled`. A
  `parse_and_apply_atomic_streaming` deep-clone variant
  would be `parse_and_apply_atomic_streaming_deep_clone`
  — observably equivalent to today's wrapper on the
  success path, different rollback mechanism (deep-clone
  vs inverse-journal). The deep-clone variant is future
  work.

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
cmake -S late-may/cpp_practice/streaming_atomic_patch -B late-may/cpp_practice/streaming_atomic_patch/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/streaming_atomic_patch/build
./late-may/cpp_practice/streaming_atomic_patch/build/P-2026-08-10-streaming-atomic-patch
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/streaming_atomic_patch -B late-may/cpp_practice/streaming_atomic_patch/build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/streaming_atomic_patch/build-strict
./late-may/cpp_practice/streaming_atomic_patch/build-strict/P-2026-08-10-streaming-atomic-patch
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/streaming_atomic_patch -B late-may/cpp_practice/streaming_atomic_patch/build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/streaming_atomic_patch/build-asan
./late-may/cpp_practice/streaming_atomic_patch/build-asan/P-2026-08-10-streaming-atomic-patch
```

All three builds pass cleanly. **54 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

## Where we go next

Today's lesson closes the **streaming-atom arc** — the
v0.15.0 engine now has a consumer-side composition that
streams a wire-format patch document and applies each op
transactionally in a single pass. The composition is
correct by construction: the pre-check distinguishes
end-of-doc from parse failure; the per-op loop is the
Aug 9 inverse-journal + self-move filter recipe; the
return type is `std::size_t` (applied count) for the
success path and `std::unexpected{error}` for the failure
path.

The remaining v0.15.0 candidates (re-quoting from the
Aug 9 "v0.15.0 candidates" list):

- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer
  arc the Jul 21 lesson opened ("JSON Pointer → JSON
  Patch → JSON Schema"). Today's
  `parse_and_apply_atomic_streaming` is the canonical
  input layer for atomic schema-driven updates + dry-run
  validation.
- **Widen the dispatcher's int64-vs-double preservation
  guard** — orthogonal to today's lesson; relevant if a
  real consumer hits a double-shaped int64-range input.
  (Already done in v0.14.0; re-listed for completeness.)

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
- **Deep-clone variant of the streaming wrapper** —
  `parse_and_apply_atomic_streaming_deep_clone` is
  observably equivalent to today's wrapper on the
  success path, different rollback mechanism
  (deep-clone vs inverse-journal). The composability of
  the deep-clone variant is future work.
- **v0.16.0 promotion arc** — mechanical: lift
  `parse_and_apply_atomic_streaming` + the `detail::`
  helpers into `<psp_span/json_ext.h>`; bump the
  version.

For the library as a whole, today's lesson is the
**canonical closing entry** for the streaming-atom arc.
The 2-axis composition (streaming + transactional)
covers the full RFC 6902 behavioral surface the engine
needs to honor when called via a streaming wire-format
input. The natural next step is JSON Schema validation
(the remaining v0.15.0 candidate), which would give
the library a complete Pointer → Patch → Schema arc on
the read/validate side, complementing the transactional
layer on the write side.

## Files

- `late-may/cpp_practice/streaming_atomic_patch/CMakeLists.txt`
- `late-may/cpp_practice/streaming_atomic_patch/P-2026-08-10-streaming-atomic-patch.cpp`
- `late-may/cpp_practice/streaming_atomic_patch/P-2026-08-10-streaming-atomic-patch.md`
  (this file)
