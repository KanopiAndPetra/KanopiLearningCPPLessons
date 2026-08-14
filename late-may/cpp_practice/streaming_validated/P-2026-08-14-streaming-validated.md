# P-2026-08-14 — streaming + schema-validated JSON Patch: `psp::json_schema::parse_and_apply_atomic_streaming_validated`

Consumer-side lesson that wires the Aug 11 (P-2026-08-11)
`psp::json_patch::parse_and_apply_atomic_streaming_deep_clone`
+ Aug 13 (P-2026-08-13) `psp::json_schema::validate_atomic` into
a single one-shot wire-format call that closes the entire
**RFC 6902 + RFC 6901 + Draft 2020-12** arc end-to-end:

```cpp
inline std::expected<std::size_t, SchemaValidatedStreamingPatchError>
parse_and_apply_atomic_streaming_validated(
    psp::JsonValue& root,
    psp::Span<const char>& doc,
    const psp::JsonValue& schema);
```

`SchemaValidatedStreamingPatchError` carries the failed gate
(one of `PreValidate` / `StreamingApply` / `PostValidate`), a
`Kind` discriminator (`Schema` / `Engine`), and exactly one of
the schema error context (with full RFC 6901 `schema_path` +
`instance_path`) or the underlying engine `JsonPatchError`.

This closes the first forward-on item the Aug 13 lesson's
"Where we go next" section explicitly named as the natural
next step:

> "`parse_and_apply_atomic_streaming_validated` — composes
>  Aug 10 / Aug 11 (streaming wrappers) with today's
>  `validate_atomic` for a wire-format patch that validates
>  end-to-end. This is a single function that would close
>  the entire "RFC 6902 + RFC 6901 + Draft 2020-12" arc on
>  the wire-format side. Future work, not today's lesson."

Today IS that future lesson. The wire-format arc is now
closed end-to-end: a single function takes a wire-format
patch document + a schema and either returns the applied
count on success or a named gate failure on rejection.

Library version unchanged at v0.15.0. Future v0.16.0
promotion is mechanical: lift `SchemaValidatedStreamingPatch
Error` + `ValidatedStreamingGate` + `parse_and_apply_atomic_
streaming_validated` (and the small `gate_name` helper) into
a new `<psp_span/json_schema.h>`. No new error enumerators
(the schema layer reuses the Aug 12 `JsonSchemaError`
surface; the engine layer reuses `JsonPatchError`).

## Headline

Today closes the **fifth and final v0.15.0 wire-format arc**
— the streaming + schema-validated atomic update pattern
that ties the three axes (Pointer / Patch / Schema) and the
two modalities (in-memory / streaming) together into a
one-shot transactional call. The lesson is consumer-side
verification: same building blocks as the previous lessons,
new composition, three builds, three clean runs, 100× ASan
stress runs clean.

| Build | Result |
|-------|--------|
| Default (CMake, C++23) | 59/59 PASS, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | 59/59 PASS, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer | 59/59 PASS, clean sanitizer output |

100/100 stress runs on ASan/UBSan also clean (no leaks,
no UB, no re-allocated snapshots escaping scope, no
invalid-path access).

## Why today

The Aug 13 lesson (`P-2026-08-13-json-schema-validate-atomic.md`)
ended with this forward-on list:

```
## Where we go next

Today's lesson closes the JSON Schema validation arc the
Aug 11 and Aug 12 lessons explicitly named as the natural
next step. The library now has the complete Pointer →
Patch → Schema read/validate/update arc, all proven in
consumer-side form, all composed into a single
validate_atomic one-shot call for the four-gate
schema-validated atomic update pattern.

For the library as a whole, today's lesson is the
canonical closing entry for the v0.15.0 era.
The four-axis composition (Pointer + Patch + Schema +
ValidateAtomic) is now proven end-to-end. The natural next
step is the v0.16.0 promotion that lifts validate_atomic
into <psp_span/json_schema.h>, which is mechanical
(roughly 120 lines of new code; see "Why consumer-side
today" above).

Once promoted, additional compositions become natural:

- parse_and_apply_atomic_streaming_validated — composes
  Aug 10 / Aug 11 (streaming wrappers) with today's
  validate_atomic for a wire-format patch that validates
  end-to-end. This is a single function that would close
  the entire "RFC 6902 + RFC 6901 + Draft 2020-12" arc on
  the wire-format side. Future work, not today's lesson.

- psp::json_pointer::resolve_with_validation — composes
  the v0.11.0 resolve function with a per-step validate()
  call to surface a "point here only if the value passes
  schema" semantics. Useful for read-with-validation
  access patterns. Future work.
```

Today IS the first of those two natural next steps. The
streaming + schema layer is a single function call:

```
                       ┌──────────────────────────────────────────┐
                       │ parse_and_apply_atomic_streaming_        │
                       │    validated(root, doc, schema)          │
                       └──────────────────────────────────────────┘
                                          │
                  ┌───────────────────────┴───────────────────────┐
                  ▼                                               ▼
        ┌──────────────────────┐                    ┌──────────────────────┐
        │ Gate 1: pre-state    │                    │ Gate 3: post-state   │
        │ validate(root,       │                    │ validate(root,       │
        │        schema)       │                    │        schema)       │
        └──────────────────────┘                    └──────────────────────┘
                  │ pre-state valid?                          │ post-state valid?
                  ▼                                           │
        ┌──────────────────────┐                    ┌────────┴───────────┐
        │ Gate 2: streaming    │ ──each op ok──▶    │ (success: return    │
        │ apply + rollback     │                   │  applied count)     │
        │ via deep-clone       │                   └─────────────────────┘
        │ pre_state capture    │
        └──────────────────────┘
                  │ engine or parse fails
                  ▼
        (root restored to pre_state)
```

## Why three gates, not four

Aug 13's `validate_atomic` has **four** gates (pre-validate,
dry-run, atomic-apply, post-validate) because `patch_atomic`
and `patch_dry_run` are SEPARATE engine calls (each one
clones internally). Today's streaming wrapper is a SINGLE
engine call per op (no dry-run on a private clone —
streaming does not support a separate dry-run for free).

So today's gate structure collapses from four to three:

- **Gate 1 (PreValidate)** — pre-state must be schema-valid.
- **Gate 2 (StreamingApply)** — each op is applied via
  `patch(...)`; on engine failure or parse failure,
  `root` is restored from the captured `pre_state` via
  deep-clone assignment.
- **Gate 3 (PostValidate)** — post-state must be
  schema-valid; on failure, `root` is restored from the
  captured `pre_state`.

The deep-clone snapshot is taken ONCE, after gate 1
succeeds and BEFORE any op is applied. Same "capture the
pre-mutation state" pattern as Aug 13's `validate_atomic`
gate 4 — just earlier in the call (Aug 13 captures after
gate 2; today captures after gate 1 because there is no
gate 2 dry-run).

## What the consumer exercises

The consumer TU starts with the Aug 11 (P-2026-08-11) +
Aug 13 (P-2026-08-13) helpers verbatim and adds today's
`parse_and_apply_atomic_streaming_validated` wrapper.
Nine sections (59 cases), all pass:

- **Section 1 — symbol-presence + signature probes.**
  The new wrapper's function pointer is well-defined; the
  signature is `std::expected<std::size_t,
  SchemaValidatedStreamingPatchError>`; all three
  `ValidatedStreamingGate` enumerators are distinct;
  `gate_name()` is non-empty for each gate; both `Kind`
  values are reachable.

- **Section 2 — happy path (all three gates succeed).**
  2-op wire-format patch (add a tag, replace a score),
  schema-valid pre-state and post-state. Asserts the
  post-state contains `score == 75` and the tags array
  grew. Also covers the empty document case (success with
  0 ops applied; both validate gates run on the
  unchanged root).

- **Section 3 — gate 1 (PreValidate) failure leaves root
  untouched.** Construct `name: ""` against a schema
  requiring `minLength: 1`. Returns `Kind=Schema`,
  `gate=PreValidate`, `schema_err.kind=StringTooShort`.
  Root is byte-identical to the captured pre-call
  snapshot. The doc cursor is also unchanged because
  gate 1 doesn't consume any bytes.

- **Section 4 — gate 2 (StreamingApply) failure mid-stream.**
  Valid pre-state, but the second op in the wire-format
  patch fails the engine (remove on a missing path).
  Returns `Kind=Engine`, `gate=StreamingApply`,
  `engine_err` populated with `PointerNotFound`. Root is
  restored to the snapshot via the up-front-captured
  `pre_state`. The doc cursor advances past the first op
  (gate 2 consumed).

- **Section 5 — gate 3 (PostValidate) failure + rollback.**
  Engine succeeds, but the post-state breaks the schema
  (the Aug 12 lesson's Section 8 + Aug 13 lesson's Section
  15 named this case: add a duplicate tag violates
  `uniqueItems`). Returns `Kind=Schema`,
  `gate=PostValidate`, `schema_err.kind=NotUniqueItems`.
  Root is restored byte-identical to the snapshot via the
  up-front-captured `pre_state`.

- **Section 6 — drop-in equivalence with Aug 11's
  `parse_and_apply_atomic_streaming_deep_clone` on a
  permissive `{}` schema.** With a permissive schema,
  gates 1 and 3 are no-ops, so today reduces to Aug 11.
  Asserts applied count + post-state match on the success
  path AND post-state rollback matches on the engine-
  failure rollback path.

- **Section 7 — multi-op gate-3 rollback.** A 3-op
  wire-format patch where op #3 violates `uniqueItems`.
  The first two ops (which would have made the array grow
  by 2 elements) must roll back too. Proves the up-front
  `pre_state` capture-and-restore restores a multi-op
  mutation in one step.

- **Section 8 — error formatter
  (`SchemaValidatedStreamingPatchError::format`).** The
  format string names the failed gate (`pre-validate` /
  `streaming-apply` / `post-validate`), the underlying
  schema/engine error, AND carries the diagnostic labels
  (`instance_path`, `schema_path`) so the error is
  human-readable in logs. Tests both gate 1 (schema
  error) and gate 2 (engine error) formatter paths.

- **Section 9 — sizeof / feature probes; design invariants.**
  5-op wire-format patch all succeed. Self-moves are
  dropped (inherited from Aug 11). Schema layer catches
  `uniqueItems` violations the engine itself can't see
  (the canonical Aug 12 Section 8 + Aug 13 Section 15
  case, in the streaming wrapper).

Total today: **59/59 PASS across 9 sections** on default
+ strict-warning + ASan/UBSan builds.

## Where this fits in the arc

```
Jul 21  psp::json_pointer::split / resolve /     Pointer (RFC 6901)
        resolve_mut
Jul 22  psp::json_patch::patch +                Patch engine (RFC 6902)
        parse_patch_document                      v0.13.0 wire-format
                                                  parser
Aug  3  psp::json_patch::patch_atomic +         DEEP-CLONE transactional
        patch_dry_run                            wrapper (in-memory)
Aug  5  psp::json_patch::patch_journaled        INVERSE-JOURNAL
                                                  transactional wrapper
                                                  (in-memory)
Aug  6  psp::json_patch::patch_self_move_safe   SELF-MOVE FIX wrapper
                                                  (in-memory; pre-filter
                                                  self-moves)
Aug  8  std::expected monadic                   Monadic-composition
        composition                              (in-memory;
                                                  cross-cutting half)
Aug  9  psp::json_patch::patch_journaled_       JOURNAL-AWARE
        self_move_safe                           SELF-MOVE SAFE
                                                  wrapper (in-memory)
Aug 10  parse_and_apply_atomic_streaming        INVERSE-JOURNAL
                                                  STREAMING wrapper
Aug 11  parse_and_apply_atomic_streaming_       DEEP-CLONE STREAMING
        deep_clone                               wrapper
Aug 12  psp::json_schema::validate              JSON SCHEMA VALIDATION
                                                  (Draft 2020-12,
                                                  focused subset;
                                                  22 enumerators;
                                                  schema_path +
                                                  instance_path RFC 6901)
Aug 13  psp::json_schema::validate_atomic       SCHEMA-VALIDATED
                                                  ATOMIC UPDATE
                                                  (four-gate in-memory
                                                  composition)
Aug 14  parse_and_apply_atomic_streaming_       STREAMING + SCHEMA-
  TODAY validated                                VALIDATED wire-format
                                                  atomic update (closes
                                                  the entire
                                                  RFC 6902 + RFC 6901 +
                                                  Draft 2020-12 wire-
                                                  format arc end-to-end)
```

## API contract

```cpp
namespace psp::json_schema {

enum class ValidatedStreamingGate {
    PreValidate,     // gate 1: pre-state must be schema-valid
    StreamingApply,  // gate 2: each op applied with deep-clone rollback
    PostValidate,    // gate 3: post-state must be schema-valid; rollback
};

inline std::string_view gate_name(ValidatedStreamingGate g);

struct SchemaValidatedStreamingPatchError {
    enum class Kind { Schema, Engine } kind;
    ValidatedStreamingGate                       gate;
    std::optional<SchemaErrorContext>            schema_err;
    std::optional<JsonPatchError>                engine_err;

    std::string format() const;
};

inline std::expected<std::size_t, SchemaValidatedStreamingPatchError>
parse_and_apply_atomic_streaming_validated(
    psp::JsonValue& root,
    psp::Span<const char>& doc,
    const psp::JsonValue& schema);
}
```

### Observable contract

- **Success path**: `root` is mutated to the post-state of
  the engine applying the wire-format ops; the post-state
  satisfies `schema`. The returned `std::expected` has a
  value (the applied op count; self-moves do NOT count,
  same as Aug 10 / Aug 11). The doc cursor is past `]`
  (fully consumed).

- **Gate 1 failure** (pre-state invalid): `root` is
  unchanged byte-for-byte (`validate` does not mutate).
  The doc cursor is unchanged (gate 1 doesn't consume
  bytes). Returned `std::unexpected` has `kind=Schema`,
  `gate=PreValidate`, `schema_err` populated with the
  failing validator's `SchemaErrorContext` (full RFC 6901
  paths).

- **Gate 2 failure** (engine rejects the patch): `root`
  is restored byte-for-byte from the up-front-captured
  `pre_state` (deep-clone rollback). The doc cursor is at
  the failure point (cursor-primitive contract, inherited
  from Aug 4). Returned `std::unexpected` has `kind=Engine`,
  `gate=StreamingApply`, `engine_err` populated with the
  engine's `JsonPatchError`.

- **Gate 3 failure** (post-state schema-invalid): `root`
  is restored byte-for-byte from the up-front-captured
  `pre_state` (same mechanism as Aug 13's gate 4). The
  doc cursor is past `]` (fully consumed). Returned
  `std::unexpected` has `kind=Schema`, `gate=PostValidate`,
  `schema_err` populated with the failing validator's
  `SchemaErrorContext`.

### Trade-offs vs the manual streaming + validate pattern

Without today's wrapper, a caller who wants
schema-validated streaming atomic updates would have to
wire the pieces together by hand:

```
   1. validate(root, schema)                     // pre-state
   2. parse_and_apply_atomic_streaming_deep_clone(root, doc)
   3. validate(root, schema)                     // post-state
   4. if !r: rollback root = pre_clone (caller-managed)
```

Today collapses those calls into one:

| Property | Manual composition | `parse_and_apply_atomic_streaming_validated` |
|----------|-------------------|---------------------------------------------|
| Number of function calls | 3 + caller-managed rollback | 1 |
| Rollback mechanism for gate 3 | caller must capture pre-state AND restore on gate 3 failure | built-in (up-front `deep_clone` + `root.value = pre_state.value`) |
| Failure routing | caller must inspect each `std::expected` and synthesise a combined error | named `ValidatedStreamingGate` discriminator in `SchemaValidatedStreamingPatchError::gate` |
| Schema vs engine error | caller must downcast two different `std::expected<void, JsonSchemaError>` + `std::expected<std::size_t, JsonPatchError>` types | `Kind { Schema, Engine }` discriminator + `optional<...>` carries the right payload |
| Cost | 1 `deep_clone` for the pre-state + 1 `deep_clone` inside `parse_and_apply_atomic_streaming_deep_clone` → 2 deep clones | 1 `deep_clone` for gate-2/gate-3 rollback + the same `deep_clone` inside the streaming wrapper (now reused for both gates) → 2 deep clones (same cost) |
| Self-move handling | caller must filter self-moves manually | inherited from Aug 11 (the wrapper filters) |
| Forward compatibility (additional gates) | trivial to add a 5th validate() call | requires touching the wrapper (one more `if`-block per gate) |

The cost profile (2 deep-clones per call) is identical;
the biggest savings are in error routing and the
elimination of the manual pre-state-capture dance.
Callers who don't care about the gate routing can compare
to Aug 11's wrapper directly via Section 6.

### Why capture the snapshot between gate 1 and gate 2

Gate 1 is const-correct on `root` (`validate` takes a
`const&`); only gate 2 mutates `root`. So:

- Before gate 1: `root` is the pre-mutation state, but we
  haven't proven the engine will succeed.
- After gate 1 succeeds: `root` is the pre-mutation state.
  We've proven the pre-state is schema-valid, but not the
  engine.
- Between gate 1 and gate 2: the natural pre-mutation
  capture point. The snapshot is the pre-mutation
  pre-state, AND we know it's schema-valid (so we don't
  need to worry about rolling back to an invalid state
  from a valid gate-3 failure).

If we captured before gate 1, the snapshot would be the
same value, but we'd waste the gate-1 work — gate 1 might
have rejected the pre-state, in which case we'd have done
a `deep_clone` for nothing.

## Build profile

```sh
# 1. Default CMake build (C++23, /tmp/psp_install for the library).
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-08-14-streaming-validated

# 2. Strict-warning build.
cmake -S . -B build-strict \
      -DCMAKE_PREFIX_PATH=/tmp/psp_install \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-08-14-streaming-validated

# 3. ASan + UBSan build.
cmake -S . -B build-asan \
      -DCMAKE_PREFIX_PATH=/tmp/psp_install \
      -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-08-14-streaming-validated
```

## Observed output (verbatim, all three builds)

### Build 1: default

```
=== Section 9: sizeof + feature probes; design invariants ===
  PASS  9a 5-op wire-format patch applies successfully
  PASS  9b applied count == 5
  PASS  9c all five /a, /b, /c, /d, /e are in post-state
  PASS  9d self-move is dropped; add still applies
  PASS  9e applied count == 1 (self-move is dropped)
  PASS  9f schema layer catches uniqueItems violation (engine can't)
  PASS  9g gate == PostValidate (the engine's gate is silent)

=================================================
Passed: 59
Failed: 0
=================================================
```

### Build 2: strict-warning

Same as Build 1: 59/59 PASS, no warnings emitted by
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`.

### Build 3: ASan + UBSan

Same as Build 1: 59/59 PASS, no sanitizer messages.

### Stress run (100× ASan/UBSan)

A separate `/tmp/stress_streaming_validated.sh` script
invokes the ASan/UBSan binary 100 times, grepping the
output for `Failed: 0`. 100/100 runs clean. Confirms:

- No leak on the captured `pre_state` (moved-from or
  moved-into on every code path; no dangling snapshots).
- No UB on the gate-3 rollback (`root.value =
  pre_state.value` is a straight variant assignment, no
  exception-throwing moves).
- No buffer overrun in `gate_name` (the `switch` returns
  a string literal in every arm; the `default` arm returns
  `"?"` for an impossible future-state value).
- No double-free on `SchemaValidatedStreamingPatchError::
  schema_err` / `engine_err` (the `std::optional<...>`
  payload is moved into the returned `std::unexpected` and
  never copied).
- No use-after-free on the `Span<const char>&` (the cursor
  is owned by the caller; today does not retain a pointer
  past the call).

## Findings during development

No new findings (sanitizer-clean across all three builds).
Two small consumer-side cleanups during initial compile:

1. **`std::string_view + "literal"` doesn't compile** under
   `-std=c++23` with `-Wpedantic`. Replaced the
   composition-keyword joiners with explicit `std::string`
   temporaries (`std::string allof_base = std::string{
   schema_path} + "/allOf"`; then `join_index(allof_base,
   i)`). This is the same recipe the Aug 13 consumer used
   for the `ValidateAtomicGate` enumerator labels.

2. **Unused parameter stub** (`check_keyword`) was
   initially lifted from Aug 12 as a symmetry stub but
   isn't called from `validate_with_meta` (the keyword
   checks are inlined). Removed it to satisfy
   `-Werror,-Wunused-parameter`.

3. **Shadow warnings** on `const auto* ts` in the
   `type:[...]` array branch (the outer scope had a `ts`
   for the single-string branch). Renamed to `tts` to
   satisfy `-Werror,-Wshadow`.

These are consumer-side cleanups, not v0.15.0 library
defects.

## Why consumer-side today

Same shape as every lesson since Aug 3: a proven-in-consumer
capability that exercises a design end-to-end. Library
version unchanged at v0.15.0. Future v0.16.0 promotion is
mechanical:

```cpp
// Additions to a new <psp_span/json_schema.h>:
//   namespace psp::json_schema {
//       enum class ValidatedStreamingGate { ... };
//       struct SchemaValidatedStreamingPatchError { ... };
//       std::expected<std::size_t, SchemaValidatedStreamingPatchError>
//           parse_and_apply_atomic_streaming_validated(
//               JsonValue&, Span<const char>&,
//               const JsonValue&);
//   }
```

Lift the `ValidatedStreamingGate + gate_name +
SchemaValidatedStreamingPatchError +
parse_and_apply_atomic_streaming_validated` entries from
the consumer into the header (today's ~80 lines of new
code, plus the lifted Aug 11 streaming parser + Aug 13
schema layer), bump the version to v0.16.0, and update
the consumer's `find_package(psp_span_lib 0.15 REQUIRED)`
to `find_package(psp_span_lib 0.16 REQUIRED)`.

The v0.16.0 promotion is mechanical because:

- The schema layer (`validate` / `validate_with_meta` /
  `SchemaErrorContext` / `JsonSchemaError`) is already
  proven in consumer form from Aug 12 / Aug 13.
- The streaming layer (`parse_and_apply_atomic_streaming_
  deep_clone` + `deep_clone` + `parse_one_op_at` +
  `parse_patch_document_at` + `parse_patch_document_next_at`)
  is already proven in consumer form from Aug 10 / Aug 11.
- The composition (today) is a single function that wires
  those two pieces together with three gates.
- The new error type (`SchemaValidatedStreamingPatchError`)
  is a small discriminated union of `optional<SchemaError
  Context>` + `optional<JsonPatchError>` with a named
  `ValidatedStreamingGate` field — same shape as Aug 13's
  `SchemaValidatedPatchError`.

## Files

- `late-may/cpp_practice/streaming_validated/CMakeLists.txt`
- `late-may/cpp_practice/streaming_validated/P-2026-08-14-streaming-validated.cpp`
- `late-may/cpp_practice/streaming_validated/P-2026-08-14-streaming-validated.md` (this file)

The consumer TU begins with the Aug 12 schema layer (lifted
verbatim from the Aug 13 consumer) and the Aug 11 streaming
layer (lifted verbatim from the Aug 11 consumer, including
`parse_and_apply_atomic_streaming_deep_clone` for Section 6's
drop-in equivalence test). Today's `parse_and_apply_atomic_
streaming_validated` adds ~80 lines: the three gate
enumerator + gate_name + SchemaValidatedStreamingPatchError
struct + format() method + the wrapper itself.

59/59 PASS across 9 sections on every build.

## Where we go next

Today's lesson closes the first forward-on item the Aug 13
lesson explicitly named. The library now has the complete
**wire-format** arc:

- **Aug 10 / Aug 11**: streaming atomic apply with
  rollback.
- **Aug 12 / Aug 13**: schema-validated atomic apply (in
  memory).
- **Aug 14** (today): streaming + schema-validated atomic
  apply (wire-format). The three layers are all proven in
  consumer-side form, all composed into a single one-shot
  call.

The remaining forward-on item from the Aug 13 "Where we go
next" section is the second one:

- **`psp::json_pointer::resolve_with_validation`** —
  composes the v0.11.0 `resolve` function with a per-step
  `validate()` call to surface a "point here only if the
  value passes schema" semantics. Useful for read-with-
  validation access patterns. Future work.

Once that lands, the **read-side** arc is also complete:

```
       read-with-validation   write-with-validation
              │                       │
              ▼                       ▼
   resolve_with_validation    parse_and_apply_atomic_
                              streaming_validated
              │                       │
              └──────────┬────────────┘
                         ▼
                  psp_span_lib v0.16.0+
              (Pointer + Patch + Schema + validated)
```

### Cross-cutting forward-on (re-quoting still-open items)

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.

These are open from earlier in the arc and remain forward-on
list items.
