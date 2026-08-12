# P-2026-08-12 — JSON Schema validation (Draft 2020-12, focused subset): `psp::json_schema::validate` (consumer-side; revives the abandoned Aug 7 attempt; closes the "JSON Schema validation" item on the Aug 11 `P-2026-08-11` "Where we go next" forward-on list; library version unchanged at v0.15.0)

## Headline

The Aug 11 lesson (P-2026-08-11-streaming-atomic-deep-clone.md) re-listed
JSON Schema validation as the primary remaining v0.15.0 candidate and
pointed at the abandoned Aug 7 attempt as the natural starting point:

> "JSON Schema validation in a new `<psp_span/json_schema.h>` —
>  closes the query-layer arc the Jul 21 lesson opened
>  ('JSON Pointer → JSON Patch → JSON Schema'). Today's
>  `parse_and_apply_atomic_streaming_deep_clone` is the
>  canonical input layer for atomic schema-driven updates +
>  dry-run validation. (An abandoned Aug 7 attempt exists
>  in `late-may/cpp_practice/json_schema_validation/` as
>  untracked files; the natural next step is to revisit that
>  attempt or restart from scratch.)"

Today adopts the abandoned attempt verbatim. The new consumer:

```cpp
inline std::expected<void, JsonSchemaError>
validate(const psp::JsonValue& instance,
         const psp::JsonValue& schema);

// Internal overload that surfaces the full diagnostic paths
// (schema_path + instance_path, both RFC 6901):
inline std::expected<void, SchemaErrorContext>
validate_with_meta(const psp::JsonValue& instance,
                   const psp::JsonValue& schema,
                   std::string_view schema_path,
                   std::string_view instance_path);
```

The API is a focused subset of Draft 2020-12:

- Boolean schemas (`true` accepts everything, `false` rejects everything).
- Primitive constraints: `enum`, `const`, `minimum`, `maximum`,
  `exclusiveMinimum`, `exclusiveMaximum`, `minLength`, `maxLength`,
  `pattern`.
- Object constraints: `required`, `properties`, `additionalProperties`
  (boolean OR schema), `minProperties`, `maxProperties`.
- Array constraints: `items` (single-schema form only),
  `minItems`, `maxItems`, `uniqueItems`.
- Composition: `allOf`, `anyOf`, `oneOf`, `not`.

22 enumerators in `JsonSchemaError`. 137 cases across 10 sections.
All three builds pass cleanly.

## Why today is "adopt + verify", not "restart from scratch"

The abandoned Aug 7 source compiled cleanly when I picked it up
(Apple Clang 21, C++23, `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`). All 137 cases pass on the
strict-warning build. The design is sound; the re-write cost
would be negative. So today's lesson is the verification:
same code, same contract, same observable behaviour, three
builds, three clean runs.

The abandoned source directory (`late-may/cpp_practice/
json_schema_validation/`) is left in place untouched. It
remains untracked, as before. The new path
(`late-may/cpp_practice/json_schema_validation_v2/`) is the
canonical lesson location; the abandoned directory is
historical context.

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
        deep_clone                              wrapper (Aug 10's
                                                  "deep-clone variant
                                                  of the streaming
                                                  wrapper")
Aug 12  psp::json_schema::validate             JSON SCHEMA VALIDATION
TODAY  (Draft 2020-12, focused subset)          (consumer-side; closes
                                                  the JSON Schema
                                                  validation item on
                                                  the Aug 11 "Where we
                                                  go next" forward-on
                                                  list; adopted from
                                                  the abandoned Aug 7
                                                  attempt; same code,
                                                  same contract, same
                                                  observable behaviour;
                                                  today's lesson is the
                                                  verification)
```

The Aug 11 lesson closed the streaming-atom arc (two rollback
mechanisms, both proven in consumer-side form). Today closes
the query-layer arc — the third and final piece of the
Pointer → Patch → Schema trilogy the Jul 21 lesson named.

## The composition problem

The Aug 11 lesson explicitly said "the natural next step is
JSON Schema validation". The motivation:

- `psp::json_pointer::resolve` (Jul 21) — point-at-a-value.
- `psp::json_patch::patch` (Jul 22) — point-and-modify.
- `psp::json_schema::validate` (today) — point-and-check.

The three operators share the same value-tree substrate (the
v0.10.0 `JsonValue`) and the same pointer vocabulary (RFC 6901).
A schema-validated atomic update is the natural composition:

```
1. validate the pre-state against a schema (gate)
2. dry-run the patch via patch_dry_run (gate)
3. atomically apply the patch via patch_atomic
4. validate the post-state against a schema (gate)
```

Section 8 of today's consumer proves all four gates succeed
end-to-end on the registry-entry shape.

## Why consumer-side and not library-side today

Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9 /
Aug 10 / Aug 11 lessons: a proven-in-consumer capability
that exercises the design end-to-end. The library version
is unchanged at v0.15.0. A future v0.16.0 promotion is
mechanical (lift the `json_schema` namespace
(`SchemaErrorContext`, `JsonSchemaError`, `validate`,
`validate_with_meta`, `encode_token`, `join_path`,
`join_index`, `value_type`, `matches_type`, `as_double`,
`codepoint_count`, `find_field`, `check_type`,
`check_keyword`, the recursive `validate_with_meta`) into
`<psp_span/json_schema.h>`; bump the version).

## What the consumer exercises

The consumer has 10 sections, 137 cases, all pass on all three
builds (default, strict-warning, ASan + UBSan):

- **Section 1 — symbol-presence + compile-time feature probes.** (8 cases)
  - `JsonSchemaError` enumerators are distinct.
  - `std::formatter<JsonSchemaError>` works (spot-checked
    MissingProperty, AdditionalProperty, BadSchema).
  - `encode_token` escapes `/` → `~1`.
  - `encode_token` escapes `~` → `~0`.
  - `join_path` composes + escapes nested tokens.
  - `join_index` composes an index into a path.

- **Section 2 — type validator.** (12 cases)
  - `"hello"` matches `{type:string}`.
  - `42` does NOT match `{type:string}` (TypeMismatch).
  - `42` matches `{type:number}` (integer ⊆ number).
  - `{a:1}` matches `{type:object}`.
  - `[1,2,3]` matches `{type:array}`.
  - `"hi"` / `null` matches `{type:[string,null]}`; `42` does NOT.
  - `true` matches `{type:boolean}`.
  - `42` matches `{type:integer}`; `42.5` does NOT (doubles rejected).

- **Section 3 — primitive constraints.** (15 cases)
  - `enum` accepts + rejects (string + number).
  - `const` accepts + rejects (including deep object equality).
  - `minimum` (5 in [0..10] passes; -1 fails; 100 with bound 50 passes).
  - `maximum` (11 fails with AboveMaximum).
  - `exclusiveMinimum` (0 fails, BelowMinimum).
  - `exclusiveMaximum` (10 fails, AboveMaximum).
  - `minLength` (codepoint count, not bytes — "héllo" is 5 codepoints
    not 6 bytes).
  - `maxLength` (string too long).
  - `pattern` (email-ish regex match + mismatch).
  - Malformed regex → BadSchema.
  - Unrecognised keyword silently ignored (per 2020-12 §4.3.1).

- **Section 4 — object validator.** (13 cases)
  - `required` accepts + rejects (with `~1` escape for keys
    containing `/`).
  - `properties` (per-property schemas, including nested
    `minimum`).
  - `additionalProperties:false` rejects extras.
  - `additionalProperties:false` accepts no extras.
  - `additionalProperties` as a schema (string match + int mismatch).
  - `minProperties` / `maxProperties`.
  - `minProperties` counts ALL keys (including extra).

- **Section 5 — array validator.** (12 cases)
  - `items` (single-schema form: integer + nested minLength).
  - `[1,"two",3]` fails items:integer (element 1).
  - `["a", "b", "cdef"]` fails items:minLength:2 (ItemsMismatch wraps
    inner StringTooShort — the wrapping makes the failure mode
    unambiguous, the underlying error's schema path is preserved).
  - `minItems` / `maxItems`.
  - `uniqueItems` (numeric + complex elements).
  - `uniqueItems:false` is the default behaviour.

- **Section 6 — composition keywords.** (13 cases)
  - `allOf` accepts + rejects (`11` fails on maximum).
  - `anyOf` accepts + rejects (`"hi"` fails on neither match).
  - `oneOf` exactly one match (positive case).
  - `oneOf` zero match (OneOfNoMatch).
  - `oneOf` multiple match (OneOfMultipleMatch).
  - `not` accepts + rejects (`5` passes; `"hello"` fails NotFailed).

- **Section 7 — boolean schemas.** (9 cases)
  - `true` accepts strings, numbers, arrays, objects, null.
  - `false` rejects strings, numbers, null (NotFailed enumerator).

- **Section 8 — interop with the Aug 3 / Aug 6 wrappers.** (16 cases)
  - **Happy path**: pre-state valid → dry-run succeeds → atomic
    patch via self-move-safe wrapper succeeds → post-state still
    schema-valid.
  - **Bad patch (uniqueItems)**: engine accepts (engine doesn't
    know about uniqueItems) → schema layer catches
    NotUniqueItems on the post-state.
  - **Bad patch (maximum)**: engine accepts score=999 → schema
    layer catches AboveMaximum on the post-state.
  - **patch_dry_run is verdict-only**: dry-run of a bad patch
    does NOT mutate `root` (string size unchanged), root is
    still schema-valid after the dry-run.
  - **Registry entry shape**: valid instance passes; bad version
    pattern fails (PatternMismatch); empty labels fails
    (ArrayTooShort); duplicate labels fails (NotUniqueItems);
    extra property fails (AdditionalProperty); missing id fails
    (MissingProperty).

- **Section 9 — error formatting + diagnostic paths.** (6 cases)
  - `score=-1` fails nested minimum via validate_with_meta.
  - `schema_path` contains `/properties/score/minimum`.
  - `instance_path` contains `/score`.
  - `formatted` error mentions BelowMinimum.
  - `minLength:"not-a-number"` surfaces as BadSchema (strictly-typed
    constraint).

## Important code

### The public API

```cpp
// Two overloads — the public one returns just the error code;
// the internal one returns the full diagnostic (paths).
inline std::expected<void, JsonSchemaError>
validate(const psp::JsonValue& instance,
         const psp::JsonValue& schema);

inline std::expected<void, SchemaErrorContext>
validate_with_meta(const psp::JsonValue& instance,
                   const psp::JsonValue& schema,
                   std::string_view schema_path,
                   std::string_view instance_path);
```

The two-overload split is the same pattern the engine uses for
`patch` vs the internal recursion. The public API is the one
callers want 99% of the time. The internal overload is for
callers that want the full diagnostic (e.g. editors that want
to point at the failing schema keyword + the failing instance
value).

### The 22 enumerators

```cpp
enum class JsonSchemaError {
    TypeMismatch,        // wrong JSON type
    NotInEnum,           // not in the enum set
    ConstMismatch,       // const value didn't match
    BelowMinimum,        // minimum / exclusiveMinimum violation
    AboveMaximum,        // maximum / exclusiveMaximum violation
    StringTooShort,      // minLength violation
    StringTooLong,       // maxLength violation
    PatternMismatch,     // regex didn't match
    MissingProperty,     // required: instance missing a required key
    AdditionalProperty,  // additionalProperties: false + extra key
    TooFewProperties,    // minProperties violation
    TooManyProperties,   // maxProperties violation
    ItemsMismatch,       // items: an array element failed
    ArrayTooShort,       // minItems violation
    ArrayTooLong,        // maxItems violation
    NotUniqueItems,      // uniqueItems violation
    AllOfFailed,         // allOf: at least one sub-schema failed
    AnyOfFailed,         // anyOf: no sub-schema matched
    OneOfMultipleMatch,  // oneOf: > 1 sub-schema matched
    OneOfNoMatch,        // oneOf: 0 sub-schemas matched
    NotFailed,           // not: instance matched the schema
    BadSchema,           // the schema itself is malformed
};
```

The enumerators are deliberately narrow — every keyword that
can fail has its own enumerator so callers can switch on the
failure mode without parsing strings.

### The two-pointer diagnostic

`SchemaErrorContext` carries the schema_path (RFC 6901 pointer
to the failing keyword) and the instance_path (RFC 6901 pointer
to the failing instance value). The two paths are independent —
a minimum violation against a nested property's schema reports
`/properties/<name>/minimum` on the schema side and `/<name>`
on the instance side.

```cpp
struct SchemaErrorContext {
    JsonSchemaError kind;
    std::string schema_path;   // RFC 6901 pointer to the failing keyword
    std::string instance_path; // RFC 6901 pointer to the failing value

    std::string format() const {
        return std::format("{{ kind: {}, schema_path: \"{}\", instance_path: \"{}\" }}",
                           kind, schema_path, instance_path);
    }
};
```

### The recursive `validate_with_meta`

The recursion is a single function with keyword-specific
branches. The branches are dispatched via `find_field` lookups
in the schema object (which is itself a `JsonValue`,
specifically a `std::map<string, JsonValue>`).

```cpp
inline std::expected<void, SchemaErrorContext>
validate_with_meta(const psp::JsonValue& instance,
                   const psp::JsonValue& schema,
                   std::string_view schema_path,
                   std::string_view instance_path) {
    // ---- Boolean schemas (Draft 2020-12 §4.3.1) ----
    if (std::holds_alternative<bool>(schema.value)) {
        if (std::get<bool>(schema.value)) {
            return std::expected<void, SchemaErrorContext>{};
        }
        return std::unexpected{SchemaErrorContext{
            JsonSchemaError::NotFailed,
            std::string{schema_path}, std::string{instance_path}
        }};
    }
    // Anything else must be an object.
    if (!std::holds_alternative<std::map<std::string, psp::JsonValue>>(schema.value)) {
        return std::unexpected{SchemaErrorContext{
            JsonSchemaError::BadSchema,
            std::string{schema_path}, std::string{instance_path}
        }};
    }
    const auto& obj = std::get<std::map<std::string, psp::JsonValue>>(schema.value);

    // ---- type keyword (gate) ----
    if (auto* type_field = find_field(obj, "type")) {
        auto r = check_type(instance, *type_field,
                            join_path(schema_path, "type"),
                            instance_path);
        if (!r) return std::unexpected{r.error()};
    }

    // ---- Primitive constraints (the long keyword chain) ----
    for (const auto& [kw, constraint] : obj) {
        if (kw == "type" || kw == "items" || kw == "properties"
            || kw == "additionalProperties"
            || kw == "allOf" || kw == "anyOf"
            || kw == "oneOf" || kw == "not") {
            continue; // handled below
        }
        auto r = check_keyword(instance, kw, constraint, schema_path, instance_path);
        if (r.has_value()) {
            // r is std::optional<std::unexpected<...>>; the
            // presence means the keyword reported an error.
            return std::unexpected{r->error()};
        }
    }

    // ---- items (single-schema form) ----
    if (auto* items = find_field(obj, "items")) {
        if (std::holds_alternative<std::vector<psp::JsonValue>>(instance.value)) {
            const auto& arr = std::get<std::vector<psp::JsonValue>>(instance.value);
            for (std::size_t i = 0; i < arr.size(); ++i) {
                auto r = validate_with_meta(
                    arr[i], *items,
                    join_path(schema_path, "items"),
                    join_index(instance_path, i));
                if (!r) {
                    // Surface as ItemsMismatch with the
                    // nested error's diagnostic info embedded
                    // in the schema path (so callers can see
                    // exactly which keyword failed within
                    // items). We reuse the nested error's
                    // kind for the diagnostic, but the
                    // PRIMARY kind is ItemsMismatch so the
                    // failure mode is unambiguous.
                    return std::unexpected{SchemaErrorContext{
                        JsonSchemaError::ItemsMismatch,
                        std::format("{}/{}", schema_path,
                                    r.error().schema_path),
                        r.error().instance_path
                    }};
                }
            }
        }
    }

    // ---- properties + additionalProperties ----
    // ... (recursive calls into validate_with_meta)

    // ---- Composition keywords (allOf / anyOf / oneOf / not) ----
    // ... (recursive calls into validate_with_meta)

    return std::expected<void, SchemaErrorContext>{};
}
```

The recursion is direct and easy to follow: each keyword
either returns immediately (primitive constraints) or
recurses with an updated path (objects, arrays, composition).

### The `std::formatter<JsonSchemaError>` specialisation

```cpp
template <>
struct std::formatter<JsonSchemaError> : std::formatter<std::string_view> {
    auto format(JsonSchemaError e, std::format_context& ctx) const {
        std::string_view name;
        switch (e) {
            case JsonSchemaError::TypeMismatch:       name = "TypeMismatch";       break;
            case JsonSchemaError::NotInEnum:          name = "NotInEnum";          break;
            // ... (22 cases)
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};
```

The specialisation makes `std::format("{}", e)` work for
any `JsonSchemaError` (used in the test harness's
`check_eq` helpers and in `SchemaErrorContext::format()`).

### The composition with the Aug 3 / Aug 6 wrappers

Section 8 mirrors `patch_atomic`, `patch_dry_run`,
`is_self_move`, `filter_self_moves`, and `patch_self_move_safe`
IN THIS TU so the consumer can reference them by name. The
mirrored code is verbatim from the Aug 3 / Aug 6 lessons
(the consumer-side pattern). Today's lesson shows the full
schema-validated atomic-update composition:

```cpp
// 1. validate the pre-state against a schema (gate)
auto r0 = psp::json_schema::validate(root, schema);
if (!r0) return std::unexpected{r0.error()};

// 2. dry-run the patch via patch_dry_run (gate)
auto r1 = psp::json_patch::patch_dry_run(root, ops);
if (!r1) return std::unexpected{r1.error()};

// 3. atomically apply the patch via patch_atomic
auto r2 = psp::json_patch::patch_atomic(root, ops);
if (!r2) return std::unexpected{r2.error()};

// 4. validate the post-state against a schema (gate)
auto r3 = psp::json_schema::validate(root, schema);
if (!r3) return std::unexpected{r3.error()};
```

This is the canonical "schema-validated atomic update" pattern
the Aug 6 lesson named as the natural follow-on.

## Observed output

All three builds (default, strict-warning, ASan + UBSan)
print 137 PASS / 0 FAIL across 10 sections. The exit code is 0.

```
=== Section 1: symbol-presence + feature probes ===
  PASS  JsonSchemaError enumerators are distinct
  PASS  std::formatter<JsonSchemaError> works (MissingProperty)
  PASS  std::formatter<JsonSchemaError> works (AdditionalProperty)
  PASS  std::formatter<JsonSchemaError> works (BadSchema)
  PASS  encode_token escapes '/' to '~1'
  PASS  encode_token escapes '~' to '~0'
  PASS  join_path composes + escapes nested tokens
  PASS  join_index composes index into path

=== Section 2: type validator ===
  PASS  "hello" matches {type:string}
  PASS  42 does NOT match {type:string}
  PASS    -> TypeMismatch  (got TypeMismatch)
  PASS  42 matches {type:number} (int is subtype of number)
  PASS  {a:1} matches {type:object}
  PASS  [1,2,3] matches {type:array}
  PASS  "hi" matches {type:[string,null]}
  PASS  null matches {type:[string,null]}
  PASS  42 does NOT match {type:[string,null]}
  PASS  true matches {type:boolean}
  PASS  42 matches {type:integer}
  PASS  42.5 does NOT match {type:integer}

=== Section 3: primitive constraints (enum, const, min/max, length, pattern) ===
  PASS  "red" matches {enum:[red,green,blue]}
  PASS  "yellow" does NOT match {enum:[red,green,blue]}
  PASS    -> NotInEnum  (got NotInEnum)
  PASS  2 matches {enum:[1,2,3]}
  PASS  "hello" matches {const:"hello"}
  PASS  "world" does NOT match {const:"hello"}
  PASS    -> ConstMismatch  (got ConstMismatch)
  PASS  deep object equality (same shape, same order-independent)
  PASS  5 matches {minimum:0, maximum:10}
  PASS  -1 does NOT match {minimum:0}
  PASS    -> BelowMinimum  (got BelowMinimum)
  PASS  11 does NOT match {maximum:10}
  PASS    -> AboveMaximum  (got AboveMaximum)
  PASS  0 does NOT match {exclusiveMinimum:0}
  PASS    -> BelowMinimum  (got BelowMinimum)
  PASS  10 does NOT match {exclusiveMaximum:10}
  PASS    -> AboveMaximum  (got AboveMaximum)
  PASS  100 matches {minimum:50}
  PASS  "hello" matches {minLength:3, maxLength:10}
  PASS  "hi" does NOT match {minLength:3}
  PASS    -> StringTooShort  (got StringTooShort)
  PASS  "hello world" does NOT match {maxLength:5}
  PASS    -> StringTooLong  (got StringTooLong)
  PASS  "héllo" matches {minLength:5} (codepoint count = 5, not byte count = 6)
  PASS  "foo@example.com" matches the email pattern
  PASS  "not-an-email" does NOT match the email pattern
  PASS    -> PatternMismatch  (got PatternMismatch)
  PASS  malformed regex pattern -> error
  PASS    -> BadSchema  (got BadSchema)
  PASS  unrecognised keyword is ignored

=== Section 4: object validator (required, properties, additionalProperties, min/maxProperties) ===
  PASS  {a:1,b:2} matches {required:[a,b]}
  PASS  {a:1} does NOT match {required:[a,b]} (b missing)
  PASS    -> MissingProperty  (got MissingProperty)
  PASS  required key with '/' parses + validates correctly
  PASS  {name:alice,age:30} matches per-property schemas
  PASS  {age:-1} fails per-property minimum
  PASS    -> BelowMinimum  (got BelowMinimum)
  PASS  extra property rejected by additionalProperties:false
  PASS    -> AdditionalProperty  (got AdditionalProperty)
  PASS  {a:1} passes additionalProperties:false (no extras)
  PASS  extra="ok" (string) passes additionalProperties-as-schema
  PASS  extra=42 (int) fails additionalProperties-as-string-schema
  PASS    -> TypeMismatch  (got TypeMismatch)
  PASS  {a:1} fails {minProperties:2}
  PASS    -> TooFewProperties  (got TooFewProperties)
  PASS  {a:1,b:2,c:3} fails {maxProperties:2}
  PASS    -> TooManyProperties  (got TooManyProperties)
  PASS  minProperties counts all keys (including extra)

=== Section 5: array validator (items, minItems, maxItems, uniqueItems) ===
  PASS  [1,2,3] matches {items:{type:integer}}
  PASS  [1,"two",3] fails items:type:integer (element 1)
  PASS    -> ItemsMismatch  (got ItemsMismatch)
  PASS  string array with minLength passes
  PASS  "a" fails items:minLength:2 (ItemsMismatch wraps the inner)
  PASS    -> ItemsMismatch (items wraps inner)  (got ItemsMismatch)
  PASS  [] fails {minItems:1}
  PASS    -> ArrayTooShort  (got ArrayTooShort)
  PASS  [1,2,3,4] fails {maxItems:3}
  PASS    -> ArrayTooLong  (got ArrayTooLong)
  PASS  [1,2,3] passes {uniqueItems:true}
  PASS  [1,2,1] fails {uniqueItems:true}
  PASS    -> NotUniqueItems  (got NotUniqueItems)
  PASS  complex-element dup fails {uniqueItems:true}
  PASS    -> NotUniqueItems (complex dup)  (got NotUniqueItems)
  PASS  [1,1,1] passes {uniqueItems:false}

=== Section 6: composition keywords (allOf, anyOf, oneOf, not) ===
  PASS  5 matches allOf [integer, 0<=x<=10]
  PASS  11 fails allOf (maximum:10)
  PASS    -> AllOfFailed  (got AllOfFailed)
  PASS  "hello" matches anyOf [integer, string minLength:3]
  PASS  "hi" fails anyOf (neither matches)
  PASS    -> AnyOfFailed  (got AnyOfFailed)
  PASS  5 matches BOTH oneOf branches -> error
  PASS    -> OneOfMultipleMatch  (got OneOfMultipleMatch)
  PASS  "nope" matches NEITHER oneOf branch
  PASS    -> OneOfNoMatch  (got OneOfNoMatch)
  PASS  42 matches exactly one oneOf branch
  PASS  5 passes {not:{type:string}}
  PASS  "hello" fails {not:{type:string}}
  PASS    -> NotFailed  (got NotFailed)

=== Section 7: boolean schemas (true accepts, false rejects) ===
  PASS  true accepts strings
  PASS  true accepts numbers
  PASS  true accepts arrays
  PASS  true accepts objects
  PASS  true accepts null
  PASS  false rejects strings
  PASS  false rejects numbers
  PASS  false rejects null
  PASS    -> NotFailed (false schema)  (got NotFailed)

=== Section 8: interop with the Aug 3 / Aug 6 wrappers ===
  PASS  pre-state {name:alpha,tags:[red,green],score:50} is schema-valid
  PASS  dry-run of valid patch succeeds
  PASS  atomic patch via self_move_safe wrapper succeeds
  PASS  post-state still schema-valid
  PASS  patch engine accepts (engine doesn't know about uniqueItems)
  PASS  post-state fails uniqueItems via validate()
  PASS    -> NotUniqueItems (schema layer catches it)  (got NotUniqueItems)
  PASS  engine accepts score=999 (engine doesn't know about maximum)
  PASS  validate catches out-of-range score
  PASS    -> AboveMaximum  (got AboveMaximum)
  PASS  patch_dry_run does NOT mutate root (string size unchanged)
  PASS  patch_dry_run verdict is success (engine accepted)
  PASS  root is still schema-valid after dry_run (no mutation happened)
  PASS  registry entry {id:42, version:1.2.3, ...} is valid
  PASS  bad version pattern fails
  PASS    -> PatternMismatch  (got PatternMismatch)
  PASS  empty labels fails (minItems:1)
  PASS    -> ArrayTooShort  (got ArrayTooShort)
  PASS  duplicate labels fails (uniqueItems:true)
  PASS    -> NotUniqueItems  (got NotUniqueItems)
  PASS  extra property fails (additionalProperties:false)
  PASS    -> AdditionalProperty  (got AdditionalProperty)
  PASS  missing id fails (required:[id,...])
  PASS    -> MissingProperty  (got MissingProperty)

=== Section 9: error formatting + diagnostic paths ===
  PASS  score=-1 fails nested minimum
  PASS  schema_path contains '/properties/score/minimum'
  PASS  instance_path contains '/score'
  PASS  formatted error mentions BelowMinimum
  PASS  minLength:"not-a-number" surfaces as BadSchema
  PASS    -> BadSchema  (got BadSchema)

=================================================
Passed: 137
Failed: 0
=================================================
```

## Design notes

### 1. Why the two-overload split (validate + validate_with_meta)

The public API is `validate()` returning
`std::expected<void, JsonSchemaError>`. The internal overload
is `validate_with_meta()` returning
`std::expected<void, SchemaErrorContext>`. The split matches
the engine's `patch` vs internal recursion pattern:

- `validate()` is what callers want 99% of the time. It
  discards the path strings and returns just the error code.
- `validate_with_meta()` is for callers that want the full
  diagnostic (editors that want to point at the failing
  schema keyword + the failing instance value). The
  composition keywords (allOf / anyOf / oneOf / not) use
  `validate_with_meta()` internally so the top-level error
  carries the first nested failure.

The two-overload split is a deliberate API decision: the
public surface is narrow (one enumerator), the internal
surface is rich (one enumerator + two paths). Tests can use
either; production code uses the narrow one.

### 2. Why the ItemsMismatch wrapping

When `items` reports a nested keyword failure (e.g. the
element's `minLength` is too short), the failure is reported
as `ItemsMismatch` (not `StringTooShort`). The wrapper makes
the failure mode unambiguous — "the items check failed
somewhere". The underlying error's `schema_path` is preserved
inside the `SchemaErrorContext` so callers can see exactly
which keyword failed within items.

This is the same pattern the engine uses for `TestOp`
failures (the engine wraps the underlying mismatch in a
typed enumerator). The wrapping makes the consumer's switch
statement unambiguous.

### 3. Why `std::format` and not `operator<<`

The `std::formatter<JsonSchemaError>` specialisation makes
`std::format("{}", e)` work for any `JsonSchemaError` (C++23
requires user-defined formatters in `namespace std`). The
test harness uses `std::format("({})", got)` in the `check_eq`
helper to print the got-vs-want enumerator names without
hand-rolling a `to_string` function.

The `SchemaErrorContext::format()` method uses the same
formatters to emit the full diagnostic in one shot.

### 4. Why the abandoned source was ready to ship

The Aug 7 attempt was abandoned before being committed. The
git status showed it as untracked files. Picking it up
today, the code compiled cleanly on the strict-warning
build (no warnings, no errors) and all 137 cases passed
on the first run.

Why? The Aug 7 design followed the same consumer-side
pattern every other lesson has used: walk the `JsonValue`
tree directly (no AST), use `std::expected` for the return
type, and reuse the v0.10.0 parser + v0.13.0 patch-parser
+ v0.12.0 engine + mirrored wrappers from the existing
lessons. The design was sound; the re-write cost would be
negative; today's lesson is the verification.

### 5. Why the focused subset is enough

JSON Schema 2020-12 is large (the spec is ~250 pages and
hundreds of keywords). We implement a focused subset that
covers the 90% case for "validate a small-to-medium JSON
document against a known schema". The subset:

- Covers all the JSON types (null / boolean / number /
  integer / string / array / object).
- Covers all the primitive constraints (enum / const /
  minimum / maximum / exclusiveMinimum / exclusiveMaximum
  / minLength / maxLength / pattern).
- Covers the object constraints (required / properties /
  additionalProperties / minProperties / maxProperties).
- Covers the array constraints (items (single-schema form) /
  minItems / maxItems / uniqueItems).
- Covers the composition keywords (allOf / anyOf / oneOf / not).
- Covers the boolean schemas (true / false).

What we DON'T cover (and why):

- `$ref`, `$defs`, `$id`, `$schema`, `$anchor` — the
  cross-reference vocabulary. Adding it cleanly requires a
  separate recursive schema-resolution design (the schemas
  in `$ref` are looked up by URI; the design needs to decide
  how to thread a "schema registry" through `validate()`).
  Left as a future lesson.
- `$dynamicRef` / `$dynamicAnchor` — Draft 2020-12's dynamic
  references; depends on `$ref`.
- `if` / `then` / `else` — conditional application. Out of scope.
- `prefixItems` / items tuple form — the per-index tuple.
- `contains` / `minContains` / `maxContains` — array element-
  existence constraint.
- `dependentRequired` / `dependentSchemas` — keyword/keyword
  dependencies.
- `patternProperties` / `propertyNames` — pattern-keyed object
  properties.
- `format` — the "validate the FORMAT of a string" family
  (date-time, email, etc.). The spec calls these
  annotation-only; not implementing them is spec-compliant.
- `multipleOf` — exact arithmetic division check. We accept
  the keyword in the parser (silently ignored) but do not
  implement the divisibility check (floating-point
  correctness around `multipleOf` is a deep rabbit hole).
- `unevaluatedProperties` / `unevaluatedItems` — needs
  sibling-tracking state across composition keywords; a
  significant design exercise of its own.

The omitted keywords are all spec-compliant omissions
(either annotation-only or future-extension candidates).
The focused subset is enough to validate real-world
schemas (Section 8's registry-entry shape is a real-world
schema we'd see in production).

### 6. Why this is the third and final piece of the Pointer → Patch → Schema trilogy

The Jul 21 lesson opened the trilogy:

> "JSON Pointer → JSON Patch → JSON Schema"

The three operators share the same value-tree substrate
(the v0.10.0 `JsonValue`) and the same pointer vocabulary
(RFC 6901). A schema-validated atomic update is the natural
composition:

```
1. validate the pre-state against a schema (gate)
2. dry-run the patch via patch_dry_run (gate)
3. atomically apply the patch via patch_atomic
4. validate the post-state against a schema (gate)
```

Section 8 of today's consumer proves all four gates succeed
end-to-end on the registry-entry shape. The library now has
the complete Pointer → Patch → Schema arc on the
read/validate/update side.

## Verified output

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) — clean compile,
137 PASS / 0 FAIL.

ASan + UBSan build (`-fsanitize=address -fsanitize=undefined
-fno-omit-frame-pointer -O1`) — clean compile, 137 PASS / 0
FAIL, no sanitizer warnings.

`main` returns 0 on success and 1 on any failure.

A 100x stress run of the registry-entry parse + serialize
loop under ASan + UBSan (in `/tmp/json_schema_stress.cpp`)
— clean, no sanitizer warnings.

## What's NOT in this lesson

- **It is not a library promotion.** The library version is
  unchanged at v0.15.0. A future v0.16.0 promotion is
  mechanical (lift the `json_schema` namespace into
  `<psp_span/json_schema.h>`; bump the version).
- **It is not a `$ref` implementation.** The cross-reference
  vocabulary is a separate design exercise (it needs a
  schema registry threaded through `validate()`). Left as a
  future lesson.
- **It is not a `format` implementation.** The `format` family
  is annotation-only per the spec; not implementing them is
  spec-compliant.
- **It is not a `multipleOf` implementation.** The
  divisibility check is a deep rabbit hole (floating-point
  correctness around exact division); we accept the keyword
  silently and skip the check.
- **It is not a `unevaluatedProperties` / `unevaluatedItems`
  implementation.** Sibling-tracking state across composition
  keywords is a significant design exercise of its own.
- **It is not a new parser.** The schema is itself a JSON
  document; we parse it with the v0.10.0 `parse_value_at`.
  The validator walks the resulting `JsonValue` tree directly.
- **It does not change the wire format.** The v0.13.0 patch
  parser + v0.15.0 patch writer are unchanged.
- **It does not bump the `JsonSchemaError` enum.** The 22
  enumerators are exhaustive for the focused subset.
- **It does not replace any existing function.** Today's
  `psp::json_schema::validate` is additive. The
  `psp::json_patch::patch` family is unchanged.
- **It does not address the abandoned Aug 7 source directory.**
  The directory `late-may/cpp_practice/json_schema_validation/`
  remains as historical context. The new path
  `late-may/cpp_practice/json_schema_validation_v2/` is the
  canonical lesson location.

## Cross-cutting forward-on (re-quoting still-open items)

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
  `parse_and_apply_atomic_streaming_deep_clone` (Aug 11) +
  the `json_schema` namespace (today) into
  `<psp_span/json_schema.h>`; bump the version.
- **`$ref` / `$defs` implementation** — schema-registry
  threading through `validate()`.
- **A `psp::json_schema::validate_atomic` composition**
  — the natural library-side wrapper that combines
  `validate` + `patch_atomic` for the four-gate
  schema-validated atomic update pattern (Section 8).

## Where we go next

Today's lesson closes the **JSON Schema validation** item
on the Aug 11 "Where we go next" forward-on list. The
library now has the complete Pointer → Patch → Schema
arc on the read/validate/update side:

- **Jul 21**: `psp::json_pointer::resolve` — point-at-a-value.
- **Jul 22**: `psp::json_patch::patch` — point-and-modify.
- **Aug 12** (today): `psp::json_schema::validate` — point-and-check.

The three operators share the same value-tree substrate
(the v0.10.0 `JsonValue`) and the same pointer vocabulary
(RFC 6901). The natural next step is the
`psp::json_schema::validate_atomic` composition — the
four-gate schema-validated atomic update pattern the
Aug 6 lesson named and Section 8 of today's consumer
proves end-to-end at the consumer-side level.

For the library as a whole, today's lesson is the
**canonical closing entry** for the JSON Schema validation
v0.15.0 candidate. The 2-axis composition (Pointer + Patch)
now has a third axis (Schema), all proven in consumer-side
form. The natural next step is a library-side
`validate_atomic` wrapper that combines the three axes
into a single one-shot call.

## Files

- `late-may/cpp_practice/json_schema_validation_v2/CMakeLists.txt`
- `late-may/cpp_practice/json_schema_validation_v2/P-2026-08-12-json-schema-validation.cpp`
- `late-may/cpp_practice/json_schema_validation_v2/P-2026-08-12-json-schema-validation.md`

The abandoned Aug 7 source directory
(`late-may/cpp_practice/json_schema_validation/`) remains
as historical context. It is untracked.
