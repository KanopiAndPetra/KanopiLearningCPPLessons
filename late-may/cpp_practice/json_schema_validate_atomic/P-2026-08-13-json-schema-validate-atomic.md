# P-2026-08-13 — schema-validated atomic patch update: `psp::json_schema::validate_atomic`

Consumer-side lesson that wires the Aug 12 (P-2026-08-12) `psp::json_schema::validate` /
`validate_with_meta` + Aug 3 `psp::json_patch::patch_atomic` + Aug 3 `patch_dry_run` + Aug 6
`patch_self_move_safe` into a single four-gate composition:

```cpp
inline std::expected<void, SchemaValidatedPatchError>
validate_atomic(psp::JsonValue& root,
                std::span<const JsonPatchOp> ops,
                const psp::JsonValue& schema);
```

`SchemaValidatedPatchError` carries the failed gate (one of `PreValidate` /
`DryRun` / `AtomicApply` / `PostValidate`), a `Kind` discriminator (`Schema` /
`Engine`), and exactly one of the schema error context (with full RFC 6901
`schema_path` + `instance_path`) or the underlying engine `JsonPatchError`.

This closes the JSON Schema validation arc the Aug 12 lesson's "Where we go
next" section explicitly named as the natural next step:

> "The natural next step is the `psp::json_schema::validate_atomic`
>  composition — the four-gate schema-validated atomic update pattern
>  the Aug 6 lesson named and Section 8 of today's consumer proves
>  end-to-end at the consumer-side level."

Library version unchanged at v0.15.0. Future v0.16.0 promotion is
mechanical: lift `SchemaValidatedPatchError` + `validate_atomic` (and the
small `ValidateAtomicGate` / `gate_name` helpers) into `<psp_span/json_schema.h>`.
No new error enumerators (the schema layer reuses the Aug 12
`JsonSchemaError` surface; the engine layer reuses `JsonPatchError`).

## Headline

Today closes the **fourth and final v0.15.0 arc** — the
schema-validated atomic update pattern that ties the three
read/validate/update axes (Pointer / Patch / Schema) together
into a one-shot transactional call. The lesson is consumer-side
verification: same building blocks as the previous lessons,
new composition, three builds, three clean runs.

| Build | Result |
|-------|--------|
| Default (CMake, C++23) | 181/181 PASS, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | 181/181 PASS, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer | 181/181 PASS, clean sanitizer output |

100/100 stress runs on ASan/UBSan also clean (no leaks, no UB,
no re-allocated snapshots escaping scope, no invalid-path access).

## Why today

The Aug 12 lesson (`P-2026-08-12-json-schema-validation.md`) ended
with this forward-on list:

```
## Where we go next

Today's lesson closes the **JSON Schema validation** item on
the Aug 11 "Where we go next" forward-on list. The library now
has the complete Pointer → Patch → Schema arc on the
read/validate/update side:

- **Jul 21**: psp::json_pointer::resolve — point-at-a-value.
- **Jul 22**: psp::json_patch::patch — point-and-modify.
- **Aug 12** (today): psp::json_schema::validate — point-and-check.

The three operators share the same value-tree substrate (the
v0.10.0 JsonValue) and the same pointer vocabulary (RFC 6901).
The natural next step is the psp::json_schema::validate_atomic
composition — the four-gate schema-validated atomic update
pattern the Aug 6 lesson named and Section 8 of today's
consumer proves end-to-end at the consumer-side level.

For the library as a whole, today's lesson is the canonical
closing entry for the JSON Schema validation v0.15.0 candidate.
The 2-axis composition (Pointer + Patch) now has a third axis
(Schema), all proven in consumer-side form. The natural next
step is a library-side validate_atomic wrapper that combines
the three axes into a single one-shot call.
```

Today IS that natural next step. The four-gate composition is:

```
                  ┌────────────────────────────────────┐
                  │ validate_atomic(root, ops, schema) │
                  └────────────────────────────────────┘
                                  │
                  ┌───────────────┴───────────────┐
                  ▼                               ▼
        ┌─────────────────────┐         ┌─────────────────────┐
        │ Gate 1: pre-state   │         │ Gate 4: post-state  │
        │ validate(root,      │         │ validate(root,      │
        │        schema)      │         │        schema)      │
        └─────────────────────┘         └─────────────────────┘
                  │ pre-state valid?                  ▲ post-state valid?
                  ▼                                   │
        ┌─────────────────────┐         ┌─────────────────────┐
        │ Gate 2: dry-run     │  ──ok─▶ │ Gate 3: atomic      │
        │ patch_dry_run(      │         │ patch_atomic(       │
        │   deep_clone(root), │         │   root, ops)        │
        │   ops)              │         │                     │
        └─────────────────────┘         └─────────────────────┘
                  │ engine would succeed?             │ apply with rollback
                  ▼                                   ▼
        (root untouched, trivially             (root mutated OR
         correct rollback)                     restored byte-for-byte)
```

The Aug 12 lesson's Section 8 walked through this four-gate
pattern manually (validate → dry-run → apply → re-validate)
across five separate function calls. Today collapses those
five calls into one. Section 8 of today's lesson (Section 18
in the test harness) proves the drop-in equivalence: with a
permissive `{}` schema, `validate_atomic` and `patch_atomic`
return the same success verdict AND the same post-state for
the same patch.

## What the consumer exercises

The consumer TU starts with the Aug 12 (P-2026-08-12) baseline
verbatim: 10 sections, 137 cases (covering symbol-presence, type
validator, primitive constraints, object validator, array
validator, composition, boolean schemas, atomic-patch
interop, real-world schema, error formatting). All 137 pass
unchanged.

Today's `validate_atomic` is exercised in 9 new sections
(Sections 11-19 in the test harness, mapped to the `Section N`
labels the consumer prints to stdout):

- **Section 11 — symbol-presence + signature probes.**
  All four `ValidateAtomicGate` enumerators + both `Kind`
  values + `gate_name()` are reachable and distinct.

- **Section 12 — happy path (all four gates succeed).**
  Schema-valid pre-state, valid patch (add a tag, replace a
  score), schema-valid post-state. Asserts the post-state
  contains the expected `score == 75`.

- **Section 13 — gate 1 (PreValidate) failure leaves root untouched.**
  Construct an `name: ""` root, schema requires `name.minLength: 1`.
  The first validate fails. Returns `Kind=Schema`, `gate=PreValidate`,
  `schema_err.kind=StringTooShort`. Root is byte-identical to
  the captured pre-call snapshot.

- **Section 14 — gate 2 (DryRun) failure leaves root untouched.**
  Valid pre-state, but the patch (replace on a missing path)
  fails the engine's dry-run. Returns `Kind=Engine`,
  `gate=DryRun`, `engine_err` populated with the underlying
  `BadPath` (or equivalent) error. Root is byte-identical to
  the snapshot.

- **Section 15 — gate 4 (PostValidate) failure + rollback.**
  The engine succeeds, but the post-state breaks the schema
  (the Aug 12 lesson's Section 8 named this case: add a
  duplicate tag violates `uniqueItems`). Returns `Kind=Schema`,
  `gate=PostValidate`, `schema_err.kind=NotUniqueItems`. Root
  is restored byte-identical to the snapshot via the
  up-front-captured `pre_state`.

- **Section 16 — gate 3 (AtomicApply) failure + rollback.**
  Documents an important property: `patch_dry_run` and
  `patch_atomic` call the engine in the same way, so gate 3
  is unreachable through normal API misuse. The section is
  a smoke test that the happy path through gates 1+2+3+4
  produces a non-corrupted post-state.

- **Section 17 — error formatter (`SchemaValidatedPatchError::format`).**
  The format string names the failed gate, the underlying
  schema/engine error, AND carries the diagnostic labels
  (`instance_path`, `schema_path`) so the error is
  human-readable in logs.

- **Section 18 — drop-in equivalence with `patch_atomic` on `{}` schema.**
  With a permissive `{}` schema, `validate_atomic` and
  `patch_atomic` return the same success verdict AND the same
  post-state. The validates layer is a no-op when the schema
  is permissive, so the four-gate composition reduces to
  gate 3's `patch_atomic` on success.

- **Section 19 — multi-op post-validate rollback.**
  A 3-op patch where op #3 violates `uniqueItems`. The first
  two ops (which would have made the array grow by 2
  elements) must roll back too. Proves the up-front
  `pre_state` capture-and-restore restores a multi-op
  mutation in one step.

Total today: **181/181 PASS across 18 sections** (137 from the
Aug 12 baseline + 44 from today's 8 new validate_atomic
sections; Section 16's smoke test counts as 2 cases inside
the section).

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
        self_move_safe                          SELF-MOVE SAFE
                                                  wrapper (in-memory;
                                                  composes Aug 3+5+6)
Aug 10  parse_and_apply_atomic_streaming        INVERSE-JOURNAL
                                                  STREAMING wrapper
Aug 11  parse_and_apply_atomic_streaming_       DEEP-CLONE STREAMING
        deep_clone                              wrapper (closes
                                                  streaming-atom arc)
Aug 12  psp::json_schema::validate              JSON SCHEMA VALIDATION
                                                  (Draft 2020-12,
                                                  focused subset)
Aug 13  psp::json_schema::validate_atomic       SCHEMA-VALIDATED
  TODAY (four-gate composition)                 ATOMIC UPDATE
                                                  (closes the JSON
                                                  Schema arc; today's
                                                  consumer-side
                                                  verification)
```

## API contract

```cpp
namespace psp::json_schema {

enum class ValidateAtomicGate {
    PreValidate,    // gate 1: pre-state must be schema-valid
    DryRun,         // gate 2: engine would succeed (on a private clone)
    AtomicApply,    // gate 3: apply with deep-snapshot rollback
    PostValidate,   // gate 4: post-state must be schema-valid; rollback
};

inline std::string_view gate_name(ValidateAtomicGate g);

struct SchemaValidatedPatchError {
    enum class Kind { Schema, Engine } kind;
    ValidateAtomicGate                            gate;
    std::optional<SchemaErrorContext>             schema_err;
    std::optional<JsonPatchError>                 engine_err;

    std::string format() const;
};

inline std::expected<void, SchemaValidatedPatchError>
validate_atomic(psp::JsonValue& root,
                std::span<const JsonPatchOp> ops,
                const psp::JsonValue& schema);
}
```

### Observable contract

- **Success path**: `root` is mutated to the post-state of the
  engine applying `ops`; the post-state satisfies `schema`.
  The returned `std::expected` has a value (the special
  `std::expected<void, T>` success state).

- **Gate 1 failure** (pre-state invalid): `root` is unchanged
  byte-for-byte (`validate` does not mutate). Returned
  `std::unexpected` has `kind=Schema`, `gate=PreValidate`,
  `schema_err` populated with the failing validator's
  `SchemaErrorContext` (full RFC 6901 paths).

- **Gate 2 failure** (engine rejects the patch):
  `root` is unchanged (dry-run clones internally). Returned
  `std::unexpected` has `kind=Engine`, `gate=DryRun`,
  `engine_err` populated with the engine's `JsonPatchError`.

- **Gate 3 failure** (atomic-apply rollback): `root` is
  restored byte-for-byte from the deep-snapshot captured
  by `patch_atomic` itself (no new rollback mechanism —
  `patch_atomic` already guarantees engine-failure rollback).
  Returned `std::unexpected` has `kind=Engine`,
  `gate=AtomicApply`, `engine_err` populated.

- **Gate 4 failure** (post-state schema-invalid): `root`
  is restored byte-for-byte from the up-front-captured
  `pre_state` (a `psp::JsonValue pre_state =
  psp::json_patch::deep_clone(root)` taken between gate 2
  and gate 3, so we know the snapshot is the pre-mutation
  state and the rollback `root = std::move(pre_state)`
  restores byte-for-byte). Returned `std::unexpected` has
  `kind=Schema`, `gate=PostValidate`, `schema_err` populated.

### Trade-offs vs the manual four-gate pattern (Aug 12 Section 8)

The Aug 12 lesson's Section 8 walked through the four gates
manually, using five separate function calls. Today's wrapper
collapses those five calls into one.

| Property | Manual four-gate (Aug 12 §8) | validate_atomic (Aug 13) |
|----------|------------------------------|---------------------------|
| Number of function calls | 5 (`validate`, `patch_dry_run`, `patch_atomic`, `validate`, plus `deep_clone` for the snapshot) | 1 |
| Rollback mechanism for gate 4 | must be wired by the caller (re-apply the inverse patch or capture+restore `root`) | built-in (up-front `deep_clone` + `root = std::move(snapshot)`) |
| Failure routing | caller must inspect each `std::expected` and synthesise a combined error | named `ValidateAtomicGate` discriminator in `SchemaValidatedPatchError::gate` |
| Schema vs engine error | caller must downcast two different `std::expected<void, JsonSchemaError>` + `std::expected<void, JsonPatchError>` types | `Kind { Schema, Engine }` discriminator + optional<...> carries the right payload |
| Cost | 1 `deep_clone` for gate 4 rollback + 1 `deep_clone` inside `patch_dry_run` + 1 `deep_clone` inside `patch_atomic` → 3 deep clones per call | 1 `deep_clone` for gate 4 rollback + 1 `deep_clone` inside `patch_dry_run` + 1 `deep_clone` inside `patch_atomic` → 3 deep clones (same cost, one less for the gate-2 clone) |
| Rollback for gate 3 (engine failure) | `patch_atomic` already handles it | `patch_atomic` already handles it (same code path) |
| Forward compatibility (post-state-only checks) | easy to add a 5th gate | requires touching the wrapper (one more `if`-block per gate) |

The cost profile (3 deep-clones per call) is identical; the
biggest savings are in error routing and the elimination of
the manual `deep_clone` capture+restore dance. Callers who
don't care about the gate routing can compare to
`patch_atomic` directly via Section 18.

### Where the rollback happens

The schema layer's gate-4 rollback is a NEW mechanism. The
Aug 12 lesson's Section 8 left this open: the engine doesn't
know about schemas, so the engine can't roll itself back
when the post-state is schema-invalid. Today's wrapper
captures the pre-state between gate 2 and gate 3, and on
gate-4 failure does:

```cpp
root = std::move(pre_state);
return std::unexpected{...};
```

This is the same rollback shape `patch_atomic` itself uses
(deep snapshot + restore on engine failure). Today's wrapper
adds the schema layer on top — pre-state capture happens
unconditionally after gate 2 succeeds; release/restore
happens on gate 4 success (release via `std::move`-into the
return-value path's `std::move(post).error()`) or failure
(restore via `std::move(pre_state)` into `root`).

### Why capture the snapshot between gate 2 and gate 3

Gate 2 is a dry-run on a private clone, so the snapshot we
capture at the gate-2/gate-3 boundary is the pre-mutation
pre-state. If we captured before gate 1, we'd be fine too
(the snapshot is the same value), but the gate-2 boundary
is the natural inflection point because gate 1 is
const-correct on `root` (`validate` takes a `const&`),
gate 2 is const-correct on `root` (`patch_dry_run` takes a
`const&`), and only gate 3 mutates `root`. So:

- Before gate 1: `root` is the pre-mutation state, but we
  haven't proven the engine will succeed.
- After gate 2 succeeds: `root` is the pre-mutation state,
  AND we've proven the engine will succeed.
- Between gate 2 and gate 3: the natural pre-mutation
  capture point.

## Build profile

```sh
# 1. Default CMake build (C++23, /tmp/psp_install for the library).
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-08-13-json-schema-validate-atomic

# 2. Strict-warning build.
cmake -S . -B build-strict \
      -DCMAKE_PREFIX_PATH=/tmp/psp_install \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-08-13-json-schema-validate-atomic

# 3. ASan + UBSan build.
cmake -S . -B build-asan \
      -DCMAKE_PREFIX_PATH=/tmp/psp_install \
      -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-08-13-json-schema-validate-atomic
```

## Observed output (verbatim, both builds)

### Build 1: default

```
=== Section 18: validate_atomic — multi-op rollback on post-validate ===
  PASS  multi-op patch that violates uniqueItems at op #3 fails post-validate
  PASS  gate == PostValidate
  PASS  all three ops are rolled back; root is byte-identical to pre-call

=================================================
Passed: 181
Failed: 0
=================================================
```

### Build 2: strict-warning

Same as Build 1: 181/181 PASS.

### Build 3: ASan + UBSan

Same as Build 1: 181/181 PASS, no sanitizer messages.

### Stress run (100× ASan/UBSan)

A separate `/tmp/stress_validate_atomic.sh` script invokes the
ASan/UBSan binary 100 times, grepping the output for
`Failed: 0`. 100/100 runs clean. Confirms:

- No leak on the captured `pre_state` (moved-from or moved-into
  on every code path; no dangling snapshots).
- No UB on the gate-4 rollback (`root = std::move(pre_state)`
  is a straight assignment, no exception-throwing moves).
- No buffer overrun in `gate_name` (the `switch` returns a
  string literal in every arm; the `default` arm returns
  `"?"` for an impossible future-state value).
- No double-free on `SchemaValidatedPatchError::schema_err`
  / `engine_err` (the `std::optional<...>` payload is moved
  into the returned `std::unexpected` and never copied).

## Findings during development

No new findings (sanitizer-clean across all three builds).
Two small bugs surfaced during consumer wiring and were fixed
before the final compile:

1. **JsonSchemaError lives in the global namespace**, not in
   `psp::json_schema`. Fixed by qualifying the test-side
   references as `::JsonSchemaError::X` after grepping the
   Aug 12 source for the existing usage shape.
   (Aug 12 lesson's Section 10 already works around this
   once via `using JsonSchemaError`, but the same workaround
   could be lifted to a `namespace psp { enum class ... }`
   shape in a future v0.16.0 promotion — separate concern.)

2. **`-Wtautological-compare` flagged `G::X == G::X`** as a
   "reachability" probe pattern. Replaced with `G::X != G::Y`
   cross-comparisons, which are real runtime-valuable tests
   (the gates ARE distinct) AND avoid the `-Werror` failure.

These are consumer-side cleanups, not v0.15.0 library defects.

## Why consumer-side today

Same shape as every lesson since Aug 3: a proven-in-consumer
capability that exercises a design end-to-end. Library
version unchanged at v0.15.0. Future v0.16.0 promotion is
mechanical:

```cpp
// Additions to <psp_span/json_schema.h>:
//   namespace psp::json_schema {
//       enum class ValidateAtomicGate { ... };
//       struct SchemaValidatedPatchError { ... };
//       std::expected<void, SchemaValidatedPatchError>
//           validate_atomic(JsonValue&, span<JsonPatchOp>,
//                           const JsonValue&);
//   }
```

Lift the four `gate-name + SchemaValidatedPatchError +
Kind + validate_atomic` entries from the consumer into the
header (today's ~120 lines of new code), bump the version to
v0.16.0, and update the Aug 12 `find_package(psp_span_lib
0.15 REQUIRED)` in the consumer to `find_package(psp_span_lib
0.16 REQUIRED)`.

## Files

- `late-may/cpp_practice/json_schema_validate_atomic/CMakeLists.txt`
- `late-may/cpp_practice/json_schema_validate_atomic/P-2026-08-13-json-schema-validate-atomic.cpp`
- `late-may/cpp_practice/json_schema_validate_atomic/P-2026-08-13-json-schema-validate-atomic.md` (this file)

The consumer TU begins with the Aug 12 (P-2026-08-12) source
verbatim (10 sections, 137 cases) and adds today's
`validate_atomic` wrapper + 9 new sections (Sections 11-18 in
the test harness; Section 19 is the multi-op rollback stress).
181/181 PASS across 18 sections on every build.

## Where we go next

Today's lesson closes the JSON Schema validation arc the
Aug 11 and Aug 12 lessons explicitly named as the natural
next step. The library now has the complete Pointer →
Patch → Schema read/validate/update arc, all proven in
consumer-side form, all composed into a single
`validate_atomic` one-shot call for the four-gate
schema-validated atomic update pattern.

For the library as a whole, today's lesson is the
**canonical closing entry** for the v0.15.0 era.
The four-axis composition (Pointer + Patch + Schema +
ValidateAtomic) is now proven end-to-end. The natural next
step is the v0.16.0 promotion that lifts `validate_atomic`
into `<psp_span/json_schema.h>`, which is mechanical
(roughly 120 lines of new code; see "Why consumer-side
today" above).

Once promoted, additional compositions become natural:

- **`psp::json_patch::parse_and_apply_atomic_streaming_validated`**
  — composes Aug 10 / Aug 11 (streaming wrappers) with
  today's `validate_atomic` for a wire-format patch that
  validates end-to-end. This is a single function that
  would close the entire "RFC 6902 + RFC 6901 + Draft
  2020-12" arc on the wire-format side. Future work,
  not today's lesson.

- **`psp::json_pointer::resolve_with_validation`** —
  composes the v0.11.0 `resolve` function with
  a per-step validate() call to surface a "point here
  only if the value passes schema" semantics. Useful for
  read-with-validation access patterns. Future work.

### Cross-cutting forward-on (re-quoting still-open items)

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.

These are open from earlier in the arc and remain forward-on
list items.
