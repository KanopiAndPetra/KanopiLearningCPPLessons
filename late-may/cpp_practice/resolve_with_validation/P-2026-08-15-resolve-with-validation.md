# P-2026-08-15 — read-with-validation JSON Pointer: `psp::json_pointer::resolve_with_validation`

Consumer-side lesson that wires the v0.11.0
`psp::json_pointer::split` + `resolve` with the Aug 12
(P-2026-08-12) `psp::json_schema::validate` /
`validate_with_meta` into a single one-shot **read-with-validation**
access pattern that closes the entire
**RFC 6901 + RFC 6902 + Draft 2020-12** read-side arc
end-to-end:

```cpp
inline std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
resolve_with_validation(std::string_view pointer,
                        const psp::JsonValue& root,
                        const psp::JsonValue& schema) noexcept;
```

`SchemaValidatedResolveError` carries the failed gate
(one of `Pointer` / `Schema`), a `Kind` discriminator
(`Pointer` / `Schema`), and exactly one of the underlying
`JsonExtError` (pointer failure) or
`psp::json_schema::SchemaErrorContext` (schema failure —
with full RFC 6901 `schema_path` + `instance_path`).

This closes the second forward-on item the Aug 13 lesson's
"Where we go next" section explicitly named as the natural
next step after Aug 14:

> "`psp::json_pointer::resolve_with_validation` —
>  composes the v0.11.0 `resolve` function with a
>  per-step `validate()` call to surface a 'point here
>  only if the value passes schema' semantics. Useful
>  for read-with-validation access patterns. Future work."

Today IS that future lesson. The read-side arc is now
complete end-to-end: a single function takes a
JSON Pointer + a schema and either returns a non-owning
const pointer to the (schema-valid) sub-value on success
or a named gate failure on rejection.

Library version unchanged at v0.15.0. Future v0.16.0
promotion is mechanical: lift `ResolveGate` + `gate_name` +
`SchemaValidatedResolveError` + `resolve_with_validation`
into a new `<psp_span/json_pointer.h>` alongside the
existing `split` + `resolve` + `resolve_mut`. No new
error enumerators (the pointer layer reuses v0.11.0
`JsonExtError`; the schema layer reuses the Aug 12
`JsonSchemaError` surface).

## Headline

Today closes the **fifth and final v0.15.0 read-side arc**
— the read-with-validation access pattern that ties the
three axes (Pointer / Patch / Schema) and the two
modalities (read / write) together into a single one-shot
call. The lesson is consumer-side verification: same
building blocks as the previous lessons, new composition,
three builds, three clean runs, 100× ASan stress runs
clean.

| Build | Result |
|-------|--------|
| Default (CMake, C++23) | 82/82 PASS, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | 82/82 PASS, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer | 82/82 PASS, clean sanitizer output |

100/100 stress runs on ASan/UBSan also clean (no leaks,
no UB, no use-after-free on the resolved pointer, no
double-free on the `schema_err` / `pointer_err` optionals).

## Why today

The Aug 13 lesson (`P-2026-08-13-json-schema-validate-atomic.md`)
ended with this forward-on list (re-quoted from the Aug 14
lesson's `Where we go next`):

```
- psp::json_pointer::resolve_with_validation — composes
  the v0.11.0 resolve function with a per-step validate()
  call to surface a "point here only if the value passes
  schema" semantics. Useful for read-with-validation
  access patterns. Future work.
```

Today IS that natural next step. The read + schema layer
is a single function call:

```
                       ┌──────────────────────────────────────┐
                       │ resolve_with_validation(pointer,     │
                       │                    root, schema)    │
                       └──────────────────────────────────────┘
                                          │
                  ┌───────────────────────┴───────────────────────┐
                  ▼                                               ▼
        ┌──────────────────────┐                    ┌──────────────────────┐
        │ Gate 1: pointer      │                    │ Gate 2: schema       │
        │ tokenize + resolve   │ ──pointer ok──▶    │ validate_with_meta   │
        │ (split, resolve)     │                    │ (*cur, schema)       │
        └──────────────────────┘                    └──────────────────────┘
                  │ pointer failure                          │ schema failure
                  ▼                                           ▼
        (returns SchemaValidatedResolveError{       (returns SchemaValidatedResolveError{
           kind=Pointer,                              kind=Schema,
           gate=Pointer,                              gate=Schema,
           pointer_err=...})                          schema_err=...})
```

## Why two gates, not more

Today is a READ, not a write. So the gate structure
collapses to its minimum:

- **Gate 1 (Pointer)** — the pointer must tokenize
  (`split`) AND resolve to a non-owning pointer
  (`resolve`). If either fails, we can't validate a
  value we haven't found yet.
- **Gate 2 (Schema)** — the value at the pointer must
  satisfy the schema (whole-value, not per-step).

No `DryRun`, no `AtomicApply`, no `PostValidate` (there's
no mutation to roll back; this is a read).

## Why whole-value validation, not per-step

The natural inflection point for the schema layer is the
value the caller asked about: `validate()` takes the leaf
and the top-level schema. Per-step validation would either:

- require the caller to provide a per-step schema (a
  schema for the parent, a schema for the child, ...),
  which composes poorly with the focused-subset
  validator, OR
- require us to invent a per-step sub-schema discovery
  mechanism that JSON Schema doesn't have at the top level.

Both are out of scope for the focused subset. Whole-value
is the canonical access pattern and matches the Aug 12/13/14
pattern (where `validate` also validates the whole state,
not each step along the way).

## What the consumer exercises

The consumer TU starts with the Aug 12 schema layer
(lifted verbatim from the Aug 14 consumer) and adds today's
`resolve_with_validation` wrapper. Nine sections (82 cases),
all pass:

- **Section 1 — symbol-presence + signature probes.**
  The new wrapper's function pointer is well-defined; the
  signature is `std::expected<const psp::JsonValue*,
  SchemaValidatedResolveError>`; both `ResolveGate`
  enumerators are distinct; `gate_name()` is non-empty for
  each gate; both `Kind` values are reachable.

- **Section 2 — happy path.** Four valid-pointer cases
  (string sub-value, integer sub-value, deep nested
  object key, array index). All return a non-null pointer
  to the expected value. Each value matches its schema.

- **Section 3 — gate 1 (Pointer) failure modes.** Five
  distinct pointer-failure modes: malformed pointer
  (no leading `/`), missing key (`/nonexistent`),
  out-of-range index (`/tags/99`), numeric token against
  a scalar (`/tags/0/0` after `/tags/0` resolves to a
  string), object key against an array (`/tags/foo`).
  All return `Kind=Pointer`, `gate=Pointer`, with the
  right `JsonExtError` enumerator.

- **Section 4 — gate 2 (Schema) failure modes.** Three
  distinct schema-failure modes: type mismatch
  (schema `integer` against string `""`), maximum
  violation (schema `maximum: 100` against `200`),
  minLength violation (schema `minLength: 1` against
  `""`). All return `Kind=Schema`, `gate=Schema`, with
  the right `SchemaErrorContext.kind` and the right
  `instance_path == <the pointer>`. Plus a control case
  where the schema accepts the value.

- **Section 5 — schema-vs-pointer routing.** A malformed
  pointer + a schema that would reject. The wrapper
  returns `Kind=Pointer` (gate 1 wins); the
  `SchemaErrorContext` is empty (validation never ran
  because there was no value to validate).

- **Section 6 — drop-in equivalence with v0.11.0
  `resolve` on a permissive `{}` schema.** Five pointers
  (valid + invalid). With a permissive schema, gate 2
  is a no-op, so today reduces to v0.11.0 `resolve`.
  Asserts `has_value()` parity and pointer-equality on
  the success path.

- **Section 7 — deep pointer resolution + whole-value
  validation.** Two successful deep-walk cases
  (`/items/0/name`, `/items/2/qty`) and one deep-walk
  schema-failure case (`/items/0/qty` against
  `minimum: 100`, value is `10`). Plus an out-of-range
  deep pointer that fails at gate 1
  (`/items/99/name`).

- **Section 8 — error formatter
  (`SchemaValidatedResolveError::format`).** The format
  string names the failed gate (`pointer` / `schema`),
  the underlying pointer/schema error, AND carries the
  diagnostic labels (`instance_path` for schema, the
  enumerator name for pointer) so the error is
  human-readable in logs. Tests both gate 1 (pointer
  error) and gate 2 (schema error) formatter paths.

- **Section 9 — sizeof / feature probes; design
  invariants.** Asserts `JsonValue` has non-zero size;
  asserts today's `value_type` matches v0.11.0
  `resolve`'s `value_type` (same `const psp::JsonValue*`
  non-owning pointer lifetime contract).

Total today: **82/82 PASS across 9 sections** on default
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
        validated                                VALIDATED wire-format
                                                  atomic update (closes
                                                  the entire wire-format
                                                  arc end-to-end)
Aug 15  psp::json_pointer::                     READ-WITH-VALIDATION
 TODAY resolve_with_validation                  pointer resolver
                                                  (closes the second
                                                  forward-on item from
                                                  Aug 13; the complete
                                                  read/write arc is now
                                                  closed end-to-end)
```

## API contract

```cpp
namespace psp::json_pointer {

enum class ResolveGate {
    Pointer,  // gate 1: pointer must tokenize + resolve
    Schema,   // gate 2: value at pointer must satisfy schema
};

inline std::string_view gate_name(ResolveGate g);

struct SchemaValidatedResolveError {
    enum class Kind { Pointer, Schema } kind;
    ResolveGate                          gate;
    std::optional<JsonExtError>          pointer_err;
    std::optional<psp::json_schema::SchemaErrorContext> schema_err;

    std::string format() const;
};

inline std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
resolve_with_validation(std::string_view pointer,
                        const psp::JsonValue& root,
                        const psp::JsonValue& schema) noexcept;
}
```

### Observable contract

- **Success path**: returns a non-owning `const psp::JsonValue*`
  pointer to the sub-value at the pointer. The value satisfies
  `schema`. The pointer's lifetime is the same as v0.11.0
  `resolve`: valid as long as `root` is alive.

- **Gate 1 failure** (pointer doesn't tokenize, or doesn't
  resolve): returns `std::unexpected` with `kind=Pointer`,
  `gate=Pointer`, `pointer_err` populated with the
  underlying `JsonExtError` (MalformedToken / NotFound /
  IndexOutOfRange / IndexNotANumber / NotAnArray /
  NotAnObject). `schema_err` is empty.

- **Gate 2 failure** (pointer resolved, but value fails
  schema): returns `std::unexpected` with `kind=Schema`,
  `gate=Schema`, `schema_err` populated with the failing
  validator's `SchemaErrorContext` (full RFC 6901 paths —
  `instance_path == <the pointer>`,
  `schema_path == <the failing keyword>`). `pointer_err`
  is empty.

### Trade-offs vs the manual resolve + validate pattern

Without today's wrapper, a caller who wants read-with-
validation would have to wire the pieces together by hand:

```
   1. resolve(pointer, root)                     // get value
   2. validate(*resolve_result, schema)          // validate
   3. if !r: synthesize error
```

Today collapses those calls into one:

| Property | Manual composition | `resolve_with_validation` |
|----------|-------------------|---------------------------|
| Number of function calls | 2 + caller-managed error synthesis | 1 |
| Failure routing | caller must inspect two `std::expected` types and synthesize a combined error | named `ResolveGate` discriminator in `SchemaValidatedResolveError::gate` |
| Pointer vs schema error | caller must downcast `std::expected<const JsonValue*, JsonExtError>` + `std::expected<void, JsonSchemaError>` | `Kind { Pointer, Schema }` discriminator + `optional<...>` carries the right payload |
| Diagnostic paths | caller must construct `SchemaErrorContext` themselves if they want RFC 6901 paths | built-in (the pointer IS the instance_path; the failing keyword IS the schema_path) |
| Cost | 1 resolve walk + 1 validate walk | 1 resolve walk + 1 validate walk (same cost) |
| Pointer-error-vs-schema-error ordering | caller must decide which to report when both fail (today's gate-1-wins convention) | gate 1 wins (can't validate a value we haven't found) |

The biggest savings are in error routing (one named gate
discriminator instead of two type-distinct `std::expected`s)
and the built-in RFC 6901 path propagation.

## Build profile

```sh
# 1. Default CMake build (C++23, /tmp/psp_install for the library).
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-08-15-resolve-with-validation

# 2. Strict-warning build.
cmake -S . -B build-strict \
      -DCMAKE_PREFIX_PATH=/tmp/psp_install \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-08-15-resolve-with-validation

# 3. ASan + UBSan build.
cmake -S . -B build-asan \
      -DCMAKE_PREFIX_PATH=/tmp/psp_install \
      -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-08-15-resolve-with-validation
```

## Observed output (verbatim, all three builds)

### Build 1: default

```
=== Section 9: sizeof + feature probes; design invariants ===
  PASS  9a psp::JsonValue has non-zero size
  PASS  9b today's value_type matches v0.11.0 resolve's value_type

=================================================
Passed: 82
Failed: 0
=================================================
```

### Build 2: strict-warning

Same as Build 1: 82/82 PASS, no warnings emitted by
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`.

### Build 3: ASan + UBSan

Same as Build 1: 82/82 PASS, no sanitizer messages.

### Stress run (100× ASan/UBSan)

A separate `/tmp/stress_resolve_with_validation.sh` script
invokes the ASan/UBSan binary 100 times, grepping the
output for `runtime error` / `AddressSanitizer` /
`LeakSanitizer` / `UndefinedBehaviorSanitizer` /
`SUMMARY:` / `use-after-free` / `heap-buffer-overflow` /
`stack-buffer-overflow` and for `Failed: 0`. 100/100 runs
clean. Confirms:

- No leak on the returned `const JsonValue*` (it's a
  non-owning pointer; the underlying `JsonValue` is the
  caller's `root`, which is on the caller's stack).
- No UB on the gate-1-vs-gate-2 routing (the gate-1
  short-circuit means validation never runs on a pointer
  that didn't resolve).
- No buffer overrun in `gate_name` (the `switch` returns
  a string literal in every arm; the `default` arm
  returns `"?"` for an impossible future-state value).
- No double-free on `SchemaValidatedResolveError::
  schema_err` / `pointer_err` (the `std::optional<...>`
  payload is moved into the returned `std::unexpected`
  and never copied).
- No use-after-free on the returned pointer (the
  pointer's lifetime is tied to `root`; today's wrapper
  does not retain a copy).

## Findings during development

Two small consumer-side findings during initial compile:

1. **`return cur` doesn't compile** under `-std=c++23`.
   `cur` is `std::expected<const JsonValue*, JsonExtError>`
   from `psp::json_pointer::resolve`, not the bare pointer.
   Replaced with `return *cur` (the expected has been
   checked at this point; we want the bare pointer to
   return). This is the same recipe the wrapping library
   uses internally (resolve returns a `expected<const
   JsonValue*, JsonExtError>`; consumers unwrap with
   `*r`).

2. **`JsonExtError` is at file scope, not in `psp::`.**
   The library's `JsonExtError` enum lives outside any
   namespace (same as `JsonSchemaError` — required for
   the std::formatter specialization to be in namespace
   `std`). Initial test code used `psp::JsonExtError`;
   the compiler complained. Removed the `psp::` qualifier
   to match the library's convention.

3. **Test fix**: my initial Section 3 case for
   `NotAnArray` used `/0` against an object root. v0.11.0
   `resolve` tries `obj->find("0")` for that case
   (because the root is an object, not an array), which
   returns `NotFound` (no key "0"), NOT `NotAnArray`.
   To get a `NotAnArray` you need a numeric (or "-")
   token against a scalar. Replaced with `/tags/0/0`
   (where `/tags/0` is the string `"red"`, and `/0`
   against a string is `NotAnArray`). This is a test
   fix, not a library defect.

These are consumer-side cleanups, not v0.15.0 library
defects.

## Why consumer-side today

Same shape as every lesson since Aug 3: a proven-in-consumer
capability that exercises a design end-to-end. Library
version unchanged at v0.15.0. Future v0.16.0 promotion is
mechanical:

```cpp
// Additions to <psp_span/json_pointer.h>:
//   namespace psp::json_pointer {
//       enum class ResolveGate { Pointer, Schema };
//       struct SchemaValidatedResolveError { ... };
//       std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
//           resolve_with_validation(std::string_view pointer,
//                                   const psp::JsonValue& root,
//                                   const psp::JsonValue& schema);
//   }
```

Lift the `ResolveGate + gate_name + SchemaValidated
ResolveError + resolve_with_validation` entries from
the consumer into the header (today's ~80 lines of new
code, plus the lifted Aug 12 schema layer), bump the
version to v0.16.0, and update the consumer's
`find_package(psp_span_lib 0.15 REQUIRED)` to
`find_package(psp_span_lib 0.16 REQUIRED)`.

The v0.16.0 promotion is mechanical because:

- The schema layer (`validate` / `validate_with_meta` /
  `SchemaErrorContext` / `JsonSchemaError`) is already
  proven in consumer form from Aug 12 / Aug 13 / Aug 14.
- The pointer layer (`split` + `resolve` + `resolve_mut`)
  is already proven in consumer form from v0.11.0 (Jul
  21).
- The composition (today) is a single function that
  wires those two pieces together with two gates.
- The new error type (`SchemaValidatedResolveError`) is
  a small discriminated union of `optional<JsonExtError>`
  + `optional<SchemaErrorContext>` with a named
  `ResolveGate` field — same shape as Aug 13's
  `SchemaValidatedPatchError` and Aug 14's
  `SchemaValidatedStreamingPatchError`.

## Files

- `late-may/cpp_practice/resolve_with_validation/CMakeLists.txt`
- `late-may/cpp_practice/resolve_with_validation/P-2026-08-15-resolve-with-validation.cpp`
- `late-may/cpp_practice/resolve_with_validation/P-2026-08-15-resolve-with-validation.md` (this file)

The consumer TU begins with the Aug 12 schema layer
(lifted verbatim from the Aug 14 consumer, including
the `JsonSchemaError` 22 enumerators, the
`SchemaErrorContext`, the `validate_with_meta` recursion,
and the encode_token / join_path / join_index helpers).
Today's `resolve_with_validation` adds ~80 lines: the
two-gate enumerator + gate_name + SchemaValidated
ResolveError struct + format() method + the wrapper
itself.

82/82 PASS across 9 sections on every build.

## Where we go next

Today's lesson closes the **second and final forward-on
item** the Aug 13 lesson explicitly named. The library
now has the complete **read-side** AND **write-side**
arc:

- **Read-side** (today): `resolve` (v0.11.0) +
  `resolve_with_validation` (today).
- **Write-side** (Aug 3 – Aug 14): the four-axis
  composition (Pointer + Patch + Schema + ValidateAtomic
  + StreamingValidated) is now proven end-to-end.

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

The complete **RFC 6901 + RFC 6902 + Draft 2020-12**
arc is now closed end-to-end on BOTH the read side and
the write side. No new forward-on list items from this
lesson's "where we go next" — the arc is closed.

### Cross-cutting forward-on (re-quoting still-open items)

- **Pin actions to commit SHAs** — `@v4` is a floating
  tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.
- **std::generator adapter on top of the Aug 4 streaming
  parser** — waiting on `<generator>` in the Apple Clang
  toolchain.
- **v0.16.0 promotion** — the mechanical lift of
  validate_atomic + parse_and_apply_atomic_streaming_
  validated + resolve_with_validation into
  `<psp_span/json_schema.h>` and
  `<psp_span/json_pointer.h>`.

These are open from earlier in the arc and remain
forward-on list items.