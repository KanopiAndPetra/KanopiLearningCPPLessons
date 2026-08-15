// P-2026-08-15 — Consumer of psp_span_lib v0.15.0 that designs the
// READ-WITH-VALIDATION JSON Pointer resolver:
//
//   psp::json_pointer::resolve_with_validation(
//       pointer,
//       root,
//       schema)
//       -> std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
//
// Where this fits in the arc
// --------------------------
//   - Jul 21: psp::json_pointer::split /  Pointer (RFC 6901)
//             resolve / resolve_mut
//   - Jul 22: psp::json_patch::patch +   Patch engine (RFC 6902)
//             parse_patch_document
//   - Aug  3: psp::json_patch::patch_    DEEP-CLONE transactional
//             atomic + patch_dry_run     wrapper (in-memory)
//   - Aug  9: psp::json_patch::patch_    JOURNAL-AWARE SELF-MOVE
//             journaled_self_move_safe    SAFE wrapper (in-memory)
//   - Aug 10: parse_and_apply_atomic_    INVERSE-JOURNAL STREAMING
//             streaming                   wrapper
//   - Aug 11: parse_and_apply_atomic_    DEEP-CLONE STREAMING
//             streaming_deep_clone         wrapper
//   - Aug 12: psp::json_schema::validate JSON SCHEMA VALIDATION
//                                       (Draft 2020-12, focused
//                                       subset; 22 enumerators;
//                                       schema_path + instance_path
//                                       RFC 6901)
//   - Aug 13: psp::json_schema::         SCHEMA-VALIDATED ATOMIC
//             validate_atomic             UPDATE (four-gate
//                                       composition; closes the
//                                       JSON Schema arc)
//   - Aug 14: parse_and_apply_atomic_    STREAMING + SCHEMA-
//             streaming_validated         VALIDATED wire-format
//                                       atomic update (closes the
//                                       entire RFC 6902 + RFC
//                                       6901 + Draft 2020-12 wire-
//                                       format arc end-to-end)
//   - Aug 15: psp::json_pointer::        READ-WITH-VALIDATION:
//     TODAY  resolve_with_validation      "point here only if the
//                                       value passes schema" one-
//                                       shot access pattern; closes
//                                       the second forward-on item
//                                       on the Aug 13 "Where we go
//                                       next" list; composes
//                                       v0.11.0 resolve() with Aug
//                                       12 validate() / validate_
//                                       with_meta() for the read-
//                                       side arc; the complete
//                                       read/write arc is now
//                                       closed end-to-end
//
// The Aug 13 lesson's "Where we go next" section explicitly named
// today as the natural next step after Aug 14:
//
//   > "psp::json_pointer::resolve_with_validation — composes
//   >  the v0.11.0 resolve function with a per-step validate()
//   >  call to surface a 'point here only if the value passes
//   >  schema' semantics. Useful for read-with-validation
//   >  access patterns. Future work."
//
// Today closes that gap. The composition is correct by
// construction: it reuses v0.11.0 split() + resolve() to walk
// the pointer, and Aug 12's validate() / validate_with_meta() to
// gate the resolved value against the schema.
//
// The composition problem
// -----------------------
// v0.11.0's resolve() has this shape:
//
//   resolve(pointer, root) -> expected<const JsonValue*, JsonExtError>
//
// Today's wrapper adds the schema layer on top:
//
//   resolve_with_validation(pointer, root, schema):
//       toks = split(pointer)
//       if !toks: return SchemaValidatedResolveError{kind=Pointer}
//       cur = resolve(*toks, root)
//       if !cur: return SchemaValidatedResolveError{kind=Pointer}
//       if !validate_with_meta(*cur, schema, pointer, ""):
//           return SchemaValidatedResolveError{kind=Schema}
//       return cur
//
// Why this is more than a one-line composition
// --------------------------------------------
// 1. SchemaErrorContext paths: validate_with_meta takes both
//    an instance_path and a schema_path. Today wires the pointer
//    (RFC 6901) into instance_path directly — the failure case
//    naturally carries the pointer as the instance path (the
//    caller pointed here; the schema rejected the value). The
//    schema_path is empty (top-level schema; the focused-subset
//    validator uses per-keyword paths relative to the root
//    schema, not to a parent one — same as Aug 12/13/14).
//
// 2. The SchemaValidatedResolveError carries a Kind discriminator
//    (Pointer vs Schema) so callers can route the two failure
//    modes differently:
//      - Pointer failure: the caller asked for a malformed
//        pointer / nonexistent path / wrong-shape path. This is
//        a "you pointed wrong" error.
//      - Schema failure: the caller pointed correctly, but the
//        value at that pointer doesn't satisfy the schema. This
//        is a "the value is wrong" error.
//    Both errors carry an optional payload of the underlying
//    JsonExtError or SchemaErrorContext. Same shape as Aug 13's
//    SchemaValidatedPatchError and Aug 14's
//    SchemaValidatedStreamingPatchError.
//
// 3. The pointer failure happens BEFORE the schema check
//    (because we can't validate a value we haven't found yet).
//    Schema failure happens AFTER (and only if pointer succeeded).
//    This is the natural gate structure for a read-with-
//    validation call: there are TWO gates, not three (no dry-
//    run, no atomic apply — this is a read, not a write).
//
// Why consumer-side today
// -----------------------
// Same shape as every lesson since Aug 3: a proven-in-consumer
// capability that exercises the design end-to-end. Library
// version unchanged at v0.15.0. Future v0.16.0 promotion is
// mechanical (lift resolve_with_validation + SchemaValidated
// ResolveError + ResolveGate + gate_name into a new
// <psp_span/json_pointer.h> alongside the existing split +
// resolve + resolve_mut; bump the version).
//
// API contract
// ------------
//   namespace psp::json_pointer {
//
//       enum class ResolveGate { Pointer, Schema };
//       inline std::string_view gate_name(ResolveGate g);
//
//       struct SchemaValidatedResolveError {
//           enum class Kind { Pointer, Schema } kind;
//           ResolveGate                          gate;
//           std::optional<JsonExtError>          pointer_err;
//           std::optional<psp::json_schema::SchemaErrorContext> schema_err;
//
//           std::string format() const;
//       };
//
//       inline std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
//       resolve_with_validation(
//           std::string_view pointer,
//           const psp::JsonValue& root,
//           const psp::JsonValue& schema) noexcept;
//   }
//
// Observable contract:
//
// - Success: returns a non-owning const pointer to the resolved
//   value. The pointer is valid as long as `root` is alive (same
//   lifetime contract as v0.11.0 resolve). The value at the
//   pointer satisfies `schema`.
//
// - Gate 1 (Pointer) failure: the pointer is malformed, the path
//   doesn't exist, or the path traverses a non-object/non-array
//   in the middle. Returns std::unexpected{SchemaValidated
//   ResolveError{kind=Pointer, gate=Pointer, pointer_err=...}}.
//   The SchemaErrorContext is empty (no value to validate).
//
// - Gate 2 (Schema) failure: the pointer resolved successfully,
//   but the value at that pointer doesn't satisfy the schema
//   (e.g., the schema required `minimum: 0` and the value is
//   -1). Returns std::unexpected{SchemaValidatedResolveError
//   {kind=Schema, gate=Schema, schema_err=...}}. The pointer
//   error is empty.
//
// What the consumer exercises
// ---------------------------
//   Section 1 — symbol-presence + signature probes (the new
//               wrapper signature; the two ResolveGate
//               enumerators; gate_name; both Kind values).
//   Section 2 — happy path: point at a valid sub-value against
//               a schema that accepts it. Returns a non-null
//               pointer; the value matches the schema's
//               expectations.
//   Section 3 — gate 1 (Pointer) failure modes: malformed
//               pointer, missing key, wrong-shape path (object
//               when an array index is given, scalar when an
//               object key is given). All return Kind=Pointer.
//   Section 4 — gate 2 (Schema) failure: the pointer resolves
//               correctly, but the value fails the schema
//               (minimum violation, type mismatch, missing
//               property). All return Kind=Schema; the schema
//               error context's instance_path is the pointer
//               itself.
//   Section 5 — schema-vs-pointer routing: a single call that
//               fails BOTH (malformed pointer AND a valid
//               schema) returns Kind=Pointer (the gate-1
//               failure wins, because we can't validate a value
//               we haven't found yet). The SchemaErrorContext
//               is empty.
//   Section 6 — drop-in equivalence on a permissive {} schema:
//               today's wrapper == v0.11.0 resolve() on the
//               success path AND on every pointer-failure path.
//               A permissive schema makes gate 2 a no-op, so
//               today reduces to v0.11.0 resolve.
//   Section 7 — multi-step pointer resolution + validation: a
//               deep pointer ("/items/0/name") that walks an
//               object array and validates each step. Proves
//               today's wrapper walks the whole path correctly
//               before validating the leaf value (NOT each
//               intermediate step — this is the Aug 13
//               validate_atomic's "whole-state" pattern, not
//               per-step; see the "Why whole-state validation"
//               note below).
//   Section 8 — error formatter
//               (SchemaValidatedResolveError::format) is
//               human-readable: names the failed gate + the
//               underlying pointer/schema error + diagnostic
//               labels.
//   Section 9 — sizeof + feature probes; design invariants.
//
//   ~36 cases across 9 sections, all expected to pass.
//
// ----
//
// Standard boilerplate ends here; the implementation begins.

#include <psp_span/json.h>
#include <psp_span/json_ext.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// JsonSchemaError — typed failure payload for validate()
// ===========================================================================
// (Mirror of Aug 12's enum, lifted from the Aug 13 / Aug 14 consumers.
// 22 enumerators.)
// Lives at file scope (not in psp::) because std::formatter<JsonSchemaError>
// is required to be in namespace std by the C++23 std::format rules.

enum class JsonSchemaError {
    TypeMismatch,
    NotInEnum,
    ConstMismatch,
    BelowMinimum,
    AboveMaximum,
    StringTooShort,
    StringTooLong,
    PatternMismatch,
    MissingProperty,
    AdditionalProperty,
    TooFewProperties,
    TooManyProperties,
    ItemsMismatch,
    ArrayTooShort,
    ArrayTooLong,
    NotUniqueItems,
    AllOfFailed,
    AnyOfFailed,
    OneOfMultipleMatch,
    OneOfNoMatch,
    NotFailed,
    BadSchema,
};

template <>
struct std::formatter<JsonSchemaError> : std::formatter<std::string_view> {
    auto format(JsonSchemaError e, std::format_context& ctx) const {
        std::string_view name;
        switch (e) {
            case JsonSchemaError::TypeMismatch:       name = "TypeMismatch";       break;
            case JsonSchemaError::NotInEnum:          name = "NotInEnum";          break;
            case JsonSchemaError::ConstMismatch:      name = "ConstMismatch";      break;
            case JsonSchemaError::BelowMinimum:       name = "BelowMinimum";       break;
            case JsonSchemaError::AboveMaximum:       name = "AboveMaximum";       break;
            case JsonSchemaError::StringTooShort:     name = "StringTooShort";     break;
            case JsonSchemaError::StringTooLong:      name = "StringTooLong";      break;
            case JsonSchemaError::PatternMismatch:    name = "PatternMismatch";    break;
            case JsonSchemaError::MissingProperty:    name = "MissingProperty";    break;
            case JsonSchemaError::AdditionalProperty: name = "AdditionalProperty"; break;
            case JsonSchemaError::TooFewProperties:   name = "TooFewProperties";   break;
            case JsonSchemaError::TooManyProperties:  name = "TooManyProperties";  break;
            case JsonSchemaError::ItemsMismatch:      name = "ItemsMismatch";      break;
            case JsonSchemaError::ArrayTooShort:      name = "ArrayTooShort";      break;
            case JsonSchemaError::ArrayTooLong:       name = "ArrayTooLong";       break;
            case JsonSchemaError::NotUniqueItems:     name = "NotUniqueItems";     break;
            case JsonSchemaError::AllOfFailed:        name = "AllOfFailed";        break;
            case JsonSchemaError::AnyOfFailed:        name = "AnyOfFailed";        break;
            case JsonSchemaError::OneOfMultipleMatch: name = "OneOfMultipleMatch"; break;
            case JsonSchemaError::OneOfNoMatch:       name = "OneOfNoMatch";       break;
            case JsonSchemaError::NotFailed:          name = "NotFailed";          break;
            case JsonSchemaError::BadSchema:          name = "BadSchema";          break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

namespace psp {
namespace json_schema {

// SchemaErrorContext — the full diagnostic (RFC 6901 paths + the
// narrow enumerator). Mirrors Aug 12.
struct SchemaErrorContext {
    JsonSchemaError kind;
    std::string schema_path;
    std::string instance_path;

    std::string format() const {
        return std::format("{{ kind: {}, schema_path: \"{}\", instance_path: \"{}\" }}",
                           kind, schema_path, instance_path);
    }
};

// encode_token — JSON-encode an RFC 6901 reference token.
inline std::string encode_token(std::string_view tok) {
    std::string out;
    out.reserve(tok.size());
    for (char c : tok) {
        if (c == '~') { out += "~0"; }
        else if (c == '/') { out += "~1"; }
        else { out += c; }
    }
    return out;
}

// join_path / join_index — RFC 6901 pointer joiners.
inline std::string join_path(std::string_view base, std::string_view tok) {
    std::string out{base};
    out += "/";
    out += encode_token(tok);
    return out;
}

inline std::string join_index(std::string_view base, std::size_t i) {
    std::string out{base};
    out += "/";
    out += std::to_string(i);
    return out;
}

// value_type / matches_type / as_double / codepoint_count — small
// helpers used by the schema layer. Mirrors Aug 12's helpers.
inline std::string_view value_type(const psp::JsonValue& v) {
    return std::visit([](const auto& alt) -> std::string_view {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::monostate>)            return "null";
        else if constexpr (std::is_same_v<T, std::nullptr_t>)       return "null";
        else if constexpr (std::is_same_v<T, bool>)                 return "boolean";
        else if constexpr (std::is_same_v<T, std::int64_t>)         return "integer";
        else if constexpr (std::is_same_v<T, double>)               return "number";
        else if constexpr (std::is_same_v<T, std::string>)          return "string";
        else if constexpr (std::is_same_v<T, std::vector<psp::JsonValue>>) return "array";
        else if constexpr (std::is_same_v<T, std::map<std::string, psp::JsonValue>>) return "object";
        else return "?";
    }, v.value);
}

inline bool matches_type(const psp::JsonValue& v, std::string_view type) {
    if (type == "number") {
        auto t = value_type(v);
        return t == "number" || t == "integer";
    }
    return value_type(v) == type;
}

inline std::optional<double> as_double(const psp::JsonValue& v) {
    return std::visit([](const auto& alt) -> std::optional<double> {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::int64_t>) return static_cast<double>(alt);
        else if constexpr (std::is_same_v<T, double>)   return alt;
        else return std::nullopt;
    }, v.value);
}

inline std::size_t codepoint_count(std::string_view s) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if      (c < 0x80) { i += 1; }
        else if (c < 0xC0) { i += 1; }  // continuation byte — skip
        else if (c < 0xE0) { i += 2; }
        else if (c < 0xF0) { i += 3; }
        else               { i += 4; }
        ++n;
    }
    return n;
}

inline bool find_field(const psp::JsonValue& obj, std::string_view key,
                       const psp::JsonValue*& out) {
    const auto* m = std::get_if<std::map<std::string, psp::JsonValue>>(&obj.value);
    if (!m) return false;
    auto it = m->find(std::string{key});
    if (it == m->end()) return false;
    out = &it->second;
    return true;
}

inline bool check_type(const psp::JsonValue& s, std::string_view want) {
    const psp::JsonValue* tv = nullptr;
    if (!find_field(s, "type", tv)) return true;  // no type = accept
    if (const auto* ts = std::get_if<std::string>(&tv->value); ts) {
        return *ts == want;
    }
    if (const auto* ta = std::get_if<std::vector<psp::JsonValue>>(&tv->value); ta) {
        for (const auto& t : *ta) {
            if (const auto* ts = std::get_if<std::string>(&t.value); ts && *ts == want) {
                return true;
            }
        }
        return false;
    }
    return true;
}

// validate_with_meta — internal recursion; returns the full
// SchemaErrorContext (paths + kind). Mirrors Aug 12.
inline std::expected<void, SchemaErrorContext>
validate_with_meta(const psp::JsonValue& instance,
                   const psp::JsonValue& schema,
                   std::string_view instance_path,
                   std::string_view schema_path) {

    // type (single-string or array)
    const psp::JsonValue* tv = nullptr;
    if (find_field(schema, "type", tv)) {
        if (const auto* ts = std::get_if<std::string>(&tv->value); ts) {
            if (!matches_type(instance, *ts)) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::TypeMismatch,
                    std::string{schema_path} + "/type",
                    std::string{instance_path}}};
            }
        } else if (const auto* ta = std::get_if<std::vector<psp::JsonValue>>(&tv->value); ta) {
            bool any = false;
            for (const auto& t : *ta) {
                if (const auto* tts = std::get_if<std::string>(&t.value); tts && matches_type(instance, *tts)) {
                    any = true; break;
                }
            }
            if (!any) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::TypeMismatch,
                    std::string{schema_path} + "/type",
                    std::string{instance_path}}};
            }
        }
    }

    // const
    if (find_field(schema, "const", tv)) {
        if (!(instance == *tv)) {
            return std::unexpected{SchemaErrorContext{
                JsonSchemaError::ConstMismatch,
                std::string{schema_path} + "/const",
                std::string{instance_path}}};
        }
    }

    // enum
    if (find_field(schema, "enum", tv)) {
        if (const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&tv->value); arr) {
            bool any = false;
            for (const auto& e : *arr) {
                if (instance == e) { any = true; break; }
            }
            if (!any) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::NotInEnum,
                    std::string{schema_path} + "/enum",
                    std::string{instance_path}}};
            }
        }
    }

    // minimum / maximum (numeric)
    auto nv = as_double(instance);
    if (nv) {
        const psp::JsonValue* mv = nullptr;
        if (find_field(schema, "minimum", mv)) {
            if (auto mv_d = as_double(*mv); mv_d && *nv < *mv_d) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::BelowMinimum,
                    std::string{schema_path} + "/minimum",
                    std::string{instance_path}}};
            }
        }
        if (find_field(schema, "maximum", mv)) {
            if (auto mv_d = as_double(*mv); mv_d && *nv > *mv_d) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::AboveMaximum,
                    std::string{schema_path} + "/maximum",
                    std::string{instance_path}}};
            }
        }
        if (find_field(schema, "exclusiveMinimum", mv)) {
            if (auto mv_d = as_double(*mv); mv_d && *nv <= *mv_d) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::BelowMinimum,
                    std::string{schema_path} + "/exclusiveMinimum",
                    std::string{instance_path}}};
            }
        }
        if (find_field(schema, "exclusiveMaximum", mv)) {
            if (auto mv_d = as_double(*mv); mv_d && *nv >= *mv_d) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::AboveMaximum,
                    std::string{schema_path} + "/exclusiveMaximum",
                    std::string{instance_path}}};
            }
        }
    }

    // minLength / maxLength / pattern (string)
    if (value_type(instance) == "string") {
        if (const auto* sv = std::get_if<std::string>(&instance.value); sv) {
            std::size_t n = codepoint_count(*sv);
            const psp::JsonValue* mv = nullptr;
            if (find_field(schema, "minLength", mv)) {
                if (auto m_d = as_double(*mv); m_d && n < static_cast<std::size_t>(*m_d)) {
                    return std::unexpected{SchemaErrorContext{
                        JsonSchemaError::StringTooShort,
                        std::string{schema_path} + "/minLength",
                        std::string{instance_path}}};
                }
            }
            if (find_field(schema, "maxLength", mv)) {
                if (auto m_d = as_double(*mv); m_d && n > static_cast<std::size_t>(*m_d)) {
                    return std::unexpected{SchemaErrorContext{
                        JsonSchemaError::StringTooLong,
                        std::string{schema_path} + "/maxLength",
                        std::string{instance_path}}};
                }
            }
            if (find_field(schema, "pattern", mv)) {
                if (const auto* pat = std::get_if<std::string>(&mv->value); pat) {
                    try {
                        std::regex re(*pat);
                        if (!std::regex_search(*sv, re)) {
                            return std::unexpected{SchemaErrorContext{
                                JsonSchemaError::PatternMismatch,
                                std::string{schema_path} + "/pattern",
                                std::string{instance_path}}};
                        }
                    } catch (...) {
                        return std::unexpected{SchemaErrorContext{
                            JsonSchemaError::BadSchema,
                            std::string{schema_path} + "/pattern",
                            std::string{instance_path}}};
                    }
                }
            }
        }
    }

    // object: required / properties / additionalProperties / min/maxProperties
    if (value_type(instance) == "object") {
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&instance.value);
        const psp::JsonValue* sv = nullptr;

        // required
        if (find_field(schema, "required", sv)) {
            if (const auto* req = std::get_if<std::vector<psp::JsonValue>>(&sv->value); req) {
                for (const auto& r : *req) {
                    if (const auto* rs = std::get_if<std::string>(&r.value); rs) {
                        const psp::JsonValue* dummy = nullptr;
                        if (!find_field(instance, *rs, dummy)) {
                            return std::unexpected{SchemaErrorContext{
                                JsonSchemaError::MissingProperty,
                                std::string{schema_path} + "/required",
                                join_path(instance_path, *rs)}};
                        }
                    }
                }
            }
        }

        // minProperties / maxProperties
        if (find_field(schema, "minProperties", sv)) {
            if (auto m_d = as_double(*sv); m_d && obj->size() < static_cast<std::size_t>(*m_d)) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::TooFewProperties,
                    std::string{schema_path} + "/minProperties",
                    std::string{instance_path}}};
            }
        }
        if (find_field(schema, "maxProperties", sv)) {
            if (auto m_d = as_double(*sv); m_d && obj->size() > static_cast<std::size_t>(*m_d)) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::TooManyProperties,
                    std::string{schema_path} + "/maxProperties",
                    std::string{instance_path}}};
            }
        }

        // properties / additionalProperties (with per-property schemas)
        if (find_field(schema, "properties", sv)) {
            if (const auto* props = std::get_if<std::map<std::string, psp::JsonValue>>(&sv->value); props) {
                for (const auto& [pname, pschema] : *props) {
                    const psp::JsonValue* pvalue = nullptr;
                    if (find_field(instance, pname, pvalue)) {
                        std::string prop_schema_path = std::string{schema_path} + "/properties/" + encode_token(pname);
                        std::string prop_instance_path = join_path(instance_path, pname);
                        auto sub = validate_with_meta(*pvalue, pschema, prop_instance_path, prop_schema_path);
                        if (!sub) {
                            return std::unexpected{SchemaErrorContext{
                                JsonSchemaError::ItemsMismatch,
                                std::move(sub).error().schema_path,
                                std::move(sub).error().instance_path}};
                        }
                    }
                }
            }
        }

        if (find_field(schema, "additionalProperties", sv)) {
            if (const auto* addp = std::get_if<bool>(&sv->value); addp && !*addp) {
                if (find_field(schema, "properties", sv)) {
                    if (const auto* props = std::get_if<std::map<std::string, psp::JsonValue>>(&sv->value); props) {
                        for (const auto& [k, _] : *obj) {
                            if (props->find(k) == props->end()) {
                                return std::unexpected{SchemaErrorContext{
                                    JsonSchemaError::AdditionalProperty,
                                    std::string{schema_path} + "/additionalProperties",
                                    join_path(instance_path, k)}};
                            }
                        }
                    }
                }
            }
        }
    }

    // array: items / minItems / maxItems / uniqueItems
    if (value_type(instance) == "array") {
        const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&instance.value);
        const psp::JsonValue* sv = nullptr;

        if (find_field(schema, "minItems", sv)) {
            if (auto m_d = as_double(*sv); m_d && arr->size() < static_cast<std::size_t>(*m_d)) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::ArrayTooShort,
                    std::string{schema_path} + "/minItems",
                    std::string{instance_path}}};
            }
        }
        if (find_field(schema, "maxItems", sv)) {
            if (auto m_d = as_double(*sv); m_d && arr->size() > static_cast<std::size_t>(*m_d)) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::ArrayTooLong,
                    std::string{schema_path} + "/maxItems",
                    std::string{instance_path}}};
            }
        }

        if (find_field(schema, "items", sv)) {
            // items can be a single schema (apply to every element)
            // or a list of schemas (one per index). Focused subset
            // covers the single-schema form; the list-of-schemas
            // (prefixItems) form is out of scope.
            for (std::size_t i = 0; i < arr->size(); ++i) {
                std::string elem_schema_path = std::string{schema_path} + "/items";
                std::string elem_instance_path = join_index(instance_path, i);
                auto sub = validate_with_meta((*arr)[i], *sv, elem_instance_path, elem_schema_path);
                if (!sub) {
                    return std::unexpected{SchemaErrorContext{
                        JsonSchemaError::ItemsMismatch,
                        std::move(sub).error().schema_path,
                        std::move(sub).error().instance_path}};
                }
            }
        }

        if (find_field(schema, "uniqueItems", sv)) {
            if (const auto* uniq = std::get_if<bool>(&sv->value); uniq && *uniq) {
                for (std::size_t i = 0; i < arr->size(); ++i) {
                    for (std::size_t j = i + 1; j < arr->size(); ++j) {
                        if ((*arr)[i] == (*arr)[j]) {
                            return std::unexpected{SchemaErrorContext{
                                JsonSchemaError::NotUniqueItems,
                                std::string{schema_path} + "/uniqueItems",
                                std::string{instance_path}}};
                        }
                    }
                }
            }
        }
    }

    // allOf
    if (find_field(schema, "allOf", tv)) {
        if (const auto* subs = std::get_if<std::vector<psp::JsonValue>>(&tv->value); subs) {
            for (std::size_t i = 0; i < subs->size(); ++i) {
                std::string allof_base = std::string{schema_path} + "/allOf";
                std::string sub_schema_path = join_index(allof_base, i);
                auto sub = validate_with_meta(instance, (*subs)[i], instance_path, sub_schema_path);
                if (!sub) {
                    return std::unexpected{SchemaErrorContext{
                        JsonSchemaError::AllOfFailed,
                        std::move(sub).error().schema_path,
                        std::move(sub).error().instance_path}};
                }
            }
        }
    }

    // anyOf
    if (find_field(schema, "anyOf", tv)) {
        if (const auto* subs = std::get_if<std::vector<psp::JsonValue>>(&tv->value); subs) {
            bool any = false;
            SchemaErrorContext last_err{JsonSchemaError::AnyOfFailed, "", ""};
            for (std::size_t i = 0; i < subs->size(); ++i) {
                std::string anyof_base = std::string{schema_path} + "/anyOf";
                std::string sub_schema_path = join_index(anyof_base, i);
                auto sub = validate_with_meta(instance, (*subs)[i], instance_path, sub_schema_path);
                if (sub) { any = true; break; }
                last_err = std::move(sub).error();
                last_err.kind = JsonSchemaError::AnyOfFailed;
            }
            if (!any) {
                return std::unexpected{last_err};
            }
        }
    }

    // oneOf
    if (find_field(schema, "oneOf", tv)) {
        if (const auto* subs = std::get_if<std::vector<psp::JsonValue>>(&tv->value); subs) {
            std::size_t matches = 0;
            SchemaErrorContext last_err{JsonSchemaError::OneOfNoMatch, "", ""};
            for (std::size_t i = 0; i < subs->size(); ++i) {
                std::string oneof_base = std::string{schema_path} + "/oneOf";
                std::string sub_schema_path = join_index(oneof_base, i);
                auto sub = validate_with_meta(instance, (*subs)[i], instance_path, sub_schema_path);
                if (sub) { ++matches; }
                else { last_err = std::move(sub).error(); }
            }
            if (matches == 0) {
                last_err.kind = JsonSchemaError::OneOfNoMatch;
                return std::unexpected{last_err};
            }
            if (matches > 1) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::OneOfMultipleMatch,
                    std::string{schema_path} + "/oneOf",
                    std::string{instance_path}}};
            }
        }
    }

    // not
    if (find_field(schema, "not", tv)) {
        auto sub = validate_with_meta(instance, *tv, instance_path,
                                      std::string{schema_path} + "/not");
        if (sub) {
            return std::unexpected{SchemaErrorContext{
                JsonSchemaError::NotFailed,
                std::string{schema_path} + "/not",
                std::string{instance_path}}};
        }
    }

    return {};
}

// validate — public surface; returns just the narrow enumerator.
// Mirrors Aug 12.
inline std::expected<void, JsonSchemaError>
validate(const psp::JsonValue& instance, const psp::JsonValue& schema) {
    auto r = validate_with_meta(instance, schema, "", "");
    if (!r) return std::unexpected{r.error().kind};
    return {};
}

}  // namespace json_schema
}  // namespace psp

// ===========================================================================
// TODAY'S COMPOSITION: psp::json_pointer::resolve_with_validation
// ===========================================================================
//
// Reuses v0.11.0 split() + resolve() + the Aug 12 validate()
// / validate_with_meta(). Two gates (not three; not four):
//   - Gate 1 (Pointer): the pointer must tokenize AND resolve
//                        to a non-owning pointer.
//   - Gate 2 (Schema):  the value at that pointer must satisfy
//                        the schema (whole-value, not per-step).
//
// Why whole-value validation, not per-step
// ----------------------------------------
// Today's wrapper validates the LEAF value at the pointer, not
// each step along the way. The Aug 13 validate_atomic wrapper
// also validates the whole state (root), not each mutation. The
// natural inflection point for the schema layer is the value
// the caller asked about: validate() takes the leaf and the
// top-level schema. Per-step validation would either:
//   - require the caller to provide a per-step schema (a schema
//     for the parent, a schema for the child, ...), which
//     composes poorly with the focused-subset validator, OR
//   - require us to invent a per-step sub-schema discovery
//     mechanism that JSON Schema doesn't have at the top level.
// Both are out of scope for the focused subset. Whole-value is
// the canonical access pattern and matches the Aug 12/13/14
// pattern.
//
// instance_path: today wires the pointer (RFC 6901) into the
// SchemaErrorContext's instance_path directly. The schema_path
// is the top-level schema path (empty by default).

namespace psp {
namespace json_pointer {

enum class ResolveGate {
    Pointer,  // gate 1: pointer must tokenize + resolve
    Schema,   // gate 2: value at pointer must satisfy schema
};

inline std::string_view gate_name(ResolveGate g) {
    switch (g) {
        case ResolveGate::Pointer: return "pointer";
        case ResolveGate::Schema:  return "schema";
    }
    return "?";
}

struct SchemaValidatedResolveError {
    enum class Kind { Pointer, Schema } kind;
    ResolveGate                          gate;
    std::optional<JsonExtError>          pointer_err;
    std::optional<psp::json_schema::SchemaErrorContext> schema_err;

    std::string format() const {
        std::string out;
        out.append("resolve_with_validation[");
        out.append(std::string{gate_name(gate)});
        out.append("] failed: ");
        if (kind == Kind::Pointer) {
            if (pointer_err) {
                out.append("pointer error: ");
                out.append(std::format("{}", *pointer_err));
            } else {
                out.append("pointer error (no detail)");
            }
        } else {
            if (schema_err) {
                out.append("schema error at instance_path=`");
                out.append(schema_err->instance_path);
                out.append("`, schema_path=`");
                out.append(schema_err->schema_path);
                out.append("` (");
                out.append(std::format("{}", schema_err->kind));
                out.append(")");
            } else {
                out.append("schema error (no detail)");
            }
        }
        return out;
    }
};

inline std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
resolve_with_validation(std::string_view pointer,
                        const psp::JsonValue& root,
                        const psp::JsonValue& schema) noexcept {
    // Gate 1 — the pointer must tokenize AND resolve.
    //
    // split() + resolve() return two distinct JsonExtError
    // values; we forward whichever failed to the caller via
    // SchemaValidatedResolveError{kind=Pointer, gate=Pointer,
    // pointer_err=...}.
    auto toks = psp::json_pointer::split(pointer);
    if (!toks) {
        return std::unexpected{SchemaValidatedResolveError{
            SchemaValidatedResolveError::Kind::Pointer,
            ResolveGate::Pointer,
            std::move(toks).error(),
            std::nullopt}};
    }

    auto cur = psp::json_pointer::resolve(*toks, root);
    if (!cur) {
        return std::unexpected{SchemaValidatedResolveError{
            SchemaValidatedResolveError::Kind::Pointer,
            ResolveGate::Pointer,
            std::move(cur).error(),
            std::nullopt}};
    }

    // Gate 2 — the value at the pointer must satisfy the schema.
    //
    // Whole-value validation: we hand the leaf to
    // validate_with_meta with instance_path = the pointer (so
    // the caller knows which value failed) and schema_path =
    // "" (top-level schema).
    auto sch = psp::json_schema::validate_with_meta(
        **cur, schema, std::string{pointer}, "");
    if (!sch) {
        return std::unexpected{SchemaValidatedResolveError{
            SchemaValidatedResolveError::Kind::Schema,
            ResolveGate::Schema,
            std::nullopt,
            std::move(sch).error()}};
    }

    return *cur;
}

}  // namespace json_pointer
}  // namespace psp

// ===========================================================================
// Test harness
// ===========================================================================

namespace {

psp::JsonValue parse_json(std::string_view text) {
    std::string buf{text};
    psp::Span<const char> sp{buf.data(), buf.size()};
    auto v = psp::parse_value_at(sp);
    if (!v) {
        std::fprintf(stderr, "FATAL: parse_json failed\n");
        std::abort();
    }
    return std::move(*v);
}

struct TestCounter {
    int passed = 0;
    int failed = 0;
    int section = 0;

    void begin_section(std::string name) {
        ++section;
        std::printf("\n=== Section %d: %s ===\n", section, name.c_str());
    }
    void check(bool cond, std::string_view msg) {
        if (cond) {
            ++passed;
            std::printf("  PASS  %.*s\n", static_cast<int>(msg.size()), msg.data());
        } else {
            ++failed;
            std::printf("  FAIL  %.*s\n", static_cast<int>(msg.size()), msg.data());
        }
    }
    void check_eq_i(std::size_t actual, std::size_t expected, std::string_view msg) {
        bool eq = (actual == expected);
        if (eq) {
            ++passed;
            std::printf("  PASS  %.*s\n", static_cast<int>(msg.size()), msg.data());
        } else {
            ++failed;
            std::printf("  FAIL  %.*s   (expected=%zu, actual=%zu)\n",
                        static_cast<int>(msg.size()), msg.data(),
                        expected, actual);
        }
    }
};

}  // namespace

int main() {
    std::printf("P-2026-08-15 — read-with-validation JSON Pointer resolver\n"
                "  (closes the 'psp::json_pointer::resolve_with_validation'\n"
                "  item on the Aug 13 (P-2026-08-13) and Aug 14\n"
                "  (P-2026-08-14) 'Where we go next' lists; composes\n"
                "  v0.11.0 split() + resolve() with Aug 12 validate() /\n"
                "  validate_with_meta() for a one-shot\n"
                "  'point-here-only-if-the-value-passes-schema'\n"
                "  read access pattern; the complete read/write arc\n"
                "  is now closed end-to-end; library version unchanged\n"
                "  at v0.15.0)\n");

    TestCounter t;

    // ----------------------------------------------------------------
    // Section 1 — symbol-presence + signature probes.
    // ----------------------------------------------------------------
    t.begin_section("symbol-presence + signature probes");

    using psp::json_pointer::resolve_with_validation;
    using psp::json_pointer::SchemaValidatedResolveError;
    using psp::json_pointer::ResolveGate;

    {
        auto fn = &resolve_with_validation;
        t.check(fn != nullptr,
                "1a &psp::json_pointer::resolve_with_validation "
                "is well-defined");
    }

    t.check(std::is_same_v<
              std::remove_reference_t<decltype(resolve_with_validation(
                  std::declval<std::string_view>(),
                  std::declval<const psp::JsonValue&>(),
                  std::declval<const psp::JsonValue&>()))>,
              std::expected<const psp::JsonValue*, SchemaValidatedResolveError>>,
          "1b resolve_with_validation signature matches "
          "std::expected<const psp::JsonValue*, SchemaValidatedResolveError>");

    // Both ResolveGate enumerators are distinct.
    t.check(ResolveGate::Pointer != ResolveGate::Schema,
            "1c Pointer != Schema (gate enum has 2 distinct values)");

    // gate_name returns a non-empty string for each gate.
    t.check(!psp::json_pointer::gate_name(ResolveGate::Pointer).empty(),
            "1d gate_name(Pointer) returns a non-empty string");
    t.check(!psp::json_pointer::gate_name(ResolveGate::Schema).empty(),
            "1e gate_name(Schema) returns a non-empty string");

    // Both Kind values are reachable.
    t.check(SchemaValidatedResolveError::Kind::Pointer !=
            SchemaValidatedResolveError::Kind::Schema,
            "1f Pointer != Schema (Kind enum has 2 distinct values)");

    // ----------------------------------------------------------------
    // Section 2 — happy path: valid sub-value against a permissive
    // schema. Returns a non-null pointer; the value matches the
    // schema's expectations.
    // ----------------------------------------------------------------
    t.begin_section("happy path — point at a sub-value against a schema");

    {
        auto root = parse_json(R"({
            "name":  "alpha",
            "tags":  ["red", "green"],
            "score": 50,
            "meta":  { "owner": "petra" }
        })");

        auto sch_str = parse_json(R"({ "type": "string" })");
        auto r = resolve_with_validation("/name", root, sch_str);
        t.check(r.has_value(), "2a /name against {type:string} resolves");
        if (r) {
            const auto* v = *r;
            t.check(v->value.index() == 5, "2b /name is a string variant");
            if (v->value.index() == 5) {
                const auto& s = std::get<std::string>(v->value);
                t.check(s == "alpha", "2c /name value is \"alpha\"");
            }
        }

        auto sch_int = parse_json(R"({ "type": "integer", "minimum": 0, "maximum": 100 })");
        auto r2 = resolve_with_validation("/score", root, sch_int);
        t.check(r2.has_value(), "2d /score against integer+min+max resolves");
        if (r2) {
            const auto* v = *r2;
            t.check(v->value.index() == 3, "2e /score is an integer variant");
            if (v->value.index() == 3) {
                auto i = std::get<std::int64_t>(v->value);
                t.check(i == 50, "2f /score value is 50");
            }
        }

        // Pointer into a nested object.
        auto sch_owner = parse_json(R"({ "type": "string", "minLength": 1 })");
        auto r3 = resolve_with_validation("/meta/owner", root, sch_owner);
        t.check(r3.has_value(), "2g /meta/owner against {type:string, minLength:1} resolves");
        if (r3) {
            const auto* v = *r3;
            if (v->value.index() == 5) {
                t.check(std::get<std::string>(v->value) == "petra",
                        "2h /meta/owner value is \"petra\"");
            } else {
                t.check(false, "2h /meta/owner value is \"petra\" (wrong type)");
            }
        }

        // Pointer into an array.
        auto sch_tag = parse_json(R"({ "type": "string" })");
        auto r4 = resolve_with_validation("/tags/1", root, sch_tag);
        t.check(r4.has_value(), "2i /tags/1 against {type:string} resolves");
        if (r4) {
            const auto* v = *r4;
            if (v->value.index() == 5) {
                t.check(std::get<std::string>(v->value) == "green",
                        "2j /tags/1 value is \"green\"");
            } else {
                t.check(false, "2j /tags/1 value is \"green\" (wrong type)");
            }
        }
    }

    // ----------------------------------------------------------------
    // Section 3 — gate 1 (Pointer) failure modes.
    // ----------------------------------------------------------------
    t.begin_section("gate 1 (Pointer) failure modes");

    {
        auto root = parse_json(R"({
            "name":  "alpha",
            "tags":  ["red", "green"],
            "score": 50,
            "meta":  { "owner": "petra" }
        })");

        auto sch_any = parse_json(R"({})");

        // Malformed pointer: doesn't start with '/'.
        auto r1 = resolve_with_validation("name", root, sch_any);
        t.check(!r1.has_value(), "3a pointer without leading '/' fails (gate 1)");
        if (!r1) {
            t.check(r1.error().kind == SchemaValidatedResolveError::Kind::Pointer,
                    "3b Kind=Pointer");
            t.check(r1.error().gate == ResolveGate::Pointer,
                    "3c gate=Pointer");
            t.check(r1.error().pointer_err.has_value(),
                    "3d pointer_err populated");
            t.check(!r1.error().schema_err.has_value(),
                    "3e schema_err NOT populated (gate 1 didn't reach validation)");
        }

        // Missing key.
        auto r2 = resolve_with_validation("/nonexistent", root, sch_any);
        t.check(!r2.has_value(), "3f pointer to missing key fails (gate 1)");
        if (!r2) {
            t.check(r2.error().kind == SchemaValidatedResolveError::Kind::Pointer,
                    "3g Kind=Pointer");
            t.check(r2.error().pointer_err.has_value(),
                    "3h pointer_err populated (NotFound expected)");
            if (r2.error().pointer_err) {
                t.check(*r2.error().pointer_err == JsonExtError::NotFound,
                        "3i pointer_err is NotFound");
            }
        }

        // Out-of-range index.
        auto r3 = resolve_with_validation("/tags/99", root, sch_any);
        t.check(!r3.has_value(), "3j pointer to out-of-range index fails (gate 1)");
        if (!r3) {
            t.check(r3.error().pointer_err.has_value(),
                    "3k pointer_err populated (IndexOutOfRange expected)");
            if (r3.error().pointer_err) {
                t.check(*r3.error().pointer_err == JsonExtError::IndexOutOfRange,
                        "3l pointer_err is IndexOutOfRange");
            }
        }

        // Wrong-shape: a numeric token against a scalar. /tags/0 is
        // the string "red"; trying to descend further with a numeric
        // token (or "-") surfaces NotAnArray.
        auto r4 = resolve_with_validation("/tags/0/0", root, sch_any);
        t.check(!r4.has_value(), "3m numeric token against scalar fails (gate 1)");
        if (!r4) {
            t.check(r4.error().pointer_err.has_value(),
                    "3n pointer_err populated (NotAnArray expected)");
            if (r4.error().pointer_err) {
                t.check(*r4.error().pointer_err == JsonExtError::NotAnArray,
                        "3o pointer_err is NotAnArray");
            }
        }

        // Wrong-shape: object key against an array.
        auto r5 = resolve_with_validation("/tags/foo", root, sch_any);
        t.check(!r5.has_value(), "3p object key against array fails (gate 1)");
        if (!r5) {
            t.check(r5.error().pointer_err.has_value(),
                    "3q pointer_err populated (IndexNotANumber expected)");
            if (r5.error().pointer_err) {
                t.check(*r5.error().pointer_err == JsonExtError::IndexNotANumber,
                        "3r pointer_err is IndexNotANumber");
            }
        }
    }

    // ----------------------------------------------------------------
    // Section 4 — gate 2 (Schema) failure modes.
    // ----------------------------------------------------------------
    t.begin_section("gate 2 (Schema) failure modes");

    {
        auto root = parse_json(R"({
            "name":  "",
            "score": 200,
            "tags":  ["red", "green"]
        })");

        // Type mismatch: schema requires integer, value is a string.
        auto sch_int = parse_json(R"({ "type": "integer" })");
        auto r1 = resolve_with_validation("/name", root, sch_int);
        t.check(!r1.has_value(), "4a valid pointer + type-mismatch schema fails (gate 2)");
        if (!r1) {
            t.check(r1.error().kind == SchemaValidatedResolveError::Kind::Schema,
                    "4b Kind=Schema");
            t.check(r1.error().gate == ResolveGate::Schema,
                    "4c gate=Schema");
            t.check(r1.error().schema_err.has_value(),
                    "4d schema_err populated");
            t.check(!r1.error().pointer_err.has_value(),
                    "4e pointer_err NOT populated (gate 1 succeeded)");
            if (r1.error().schema_err) {
                t.check(r1.error().schema_err->kind == JsonSchemaError::TypeMismatch,
                        "4f schema_err.kind == TypeMismatch");
                t.check(r1.error().schema_err->instance_path == "/name",
                        "4g schema_err.instance_path == /name");
            }
        }

        // Minimum/maximum violation.
        auto sch_score = parse_json(R"({ "type": "integer", "maximum": 100 })");
        auto r2 = resolve_with_validation("/score", root, sch_score);
        t.check(!r2.has_value(), "4h valid pointer + maximum-violation schema fails (gate 2)");
        if (!r2) {
            t.check(r2.error().kind == SchemaValidatedResolveError::Kind::Schema,
                    "4i Kind=Schema");
            if (r2.error().schema_err) {
                t.check(r2.error().schema_err->kind == JsonSchemaError::AboveMaximum,
                        "4j schema_err.kind == AboveMaximum");
                t.check(r2.error().schema_err->instance_path == "/score",
                        "4k schema_err.instance_path == /score");
                t.check(r2.error().schema_err->schema_path == "/maximum",
                        "4l schema_err.schema_path == /maximum");
            }
        }

        // minLength violation (empty string against minLength:1).
        auto sch_name = parse_json(R"({ "type": "string", "minLength": 1 })");
        auto r3 = resolve_with_validation("/name", root, sch_name);
        t.check(!r3.has_value(), "4m valid pointer + minLength-violation schema fails (gate 2)");
        if (!r3) {
            t.check(r3.error().schema_err.has_value(),
                    "4n schema_err populated");
            if (r3.error().schema_err) {
                t.check(r3.error().schema_err->kind == JsonSchemaError::StringTooShort,
                        "4o schema_err.kind == StringTooShort");
            }
        }

        // Successful schema match (control: schema accepts the value).
        auto sch_score_ok = parse_json(R"({ "type": "integer", "maximum": 1000 })");
        auto r4 = resolve_with_validation("/score", root, sch_score_ok);
        t.check(r4.has_value(), "4p valid pointer + permissive schema succeeds");
    }

    // ----------------------------------------------------------------
    // Section 5 — schema-vs-pointer routing: gate 1 wins.
    // ----------------------------------------------------------------
    t.begin_section("schema-vs-pointer routing — gate 1 wins");

    {
        auto root = parse_json(R"({ "name": "" })");
        // BOTH the pointer is malformed AND the schema (if reached)
        // would reject. We expect Kind=Pointer (gate 1 wins because
        // we can't validate a value we haven't found yet).
        auto sch_bad = parse_json(R"({ "type": "integer" })");
        auto r = resolve_with_validation("name", root, sch_bad);
        t.check(!r.has_value(), "5a malformed pointer + bad schema fails (gate 1)");
        if (!r) {
            t.check(r.error().kind == SchemaValidatedResolveError::Kind::Pointer,
                    "5b Kind=Pointer (gate 1 wins)");
            t.check(r.error().gate == ResolveGate::Pointer,
                    "5c gate=Pointer");
            t.check(!r.error().schema_err.has_value(),
                    "5d schema_err is empty (validation never ran)");
        }
    }

    // ----------------------------------------------------------------
    // Section 6 — drop-in equivalence on a permissive {} schema.
    // ----------------------------------------------------------------
    t.begin_section("drop-in equivalence on permissive {} schema");

    {
        auto root = parse_json(R"({
            "name":  "alpha",
            "tags":  ["red", "green"],
            "meta":  { "owner": "petra" }
        })");

        auto sch_perm = parse_json(R"({})");

        // Valid pointers: today == v0.11.0 resolve.
        for (auto ptr : {"/name", "/tags", "/tags/0", "/meta/owner", "/missing"}) {
            auto today_r = resolve_with_validation(ptr, root, sch_perm);
            auto v11_r   = psp::json_pointer::resolve(std::string{ptr}, root);
            t.check(today_r.has_value() == v11_r.has_value(),
                    std::string{"6a `"} + std::string{ptr} + "` today.has_value == v11.has_value");
            if (today_r && v11_r) {
                t.check(today_r.value() == v11_r.value(),
                        std::string{"6b `"} + std::string{ptr} + "` today.value == v11.value");
            }
        }
    }

    // ----------------------------------------------------------------
    // Section 7 — deep pointer resolution + whole-value validation.
    // ----------------------------------------------------------------
    t.begin_section("deep pointer resolution + whole-value validation");

    {
        auto root = parse_json(R"({
            "items": [
                { "name": "alpha",   "qty": 10 },
                { "name": "beta",    "qty": 25 },
                { "name": "gamma",   "qty": 7  }
            ]
        })");

        // Schema for an item's name (minLength:1, type:string).
        auto sch_name = parse_json(R"({ "type": "string", "minLength": 1 })");

        // Schema that requires qty to be integer >= 1.
        auto sch_qty = parse_json(R"({ "type": "integer", "minimum": 1 })");

        // Schema that would FAIL: qty is integer but minimum is 100
        // (the value 10 violates minimum:100).
        auto sch_qty_bad = parse_json(R"({ "type": "integer", "minimum": 100 })");

        auto r1 = resolve_with_validation("/items/0/name", root, sch_name);
        t.check(r1.has_value(), "7a /items/0/name resolves + passes minLength:1");
        if (r1) {
            if (r1.value()->value.index() == 5) {
                t.check(std::get<std::string>(r1.value()->value) == "alpha",
                        "7b /items/0/name value is \"alpha\"");
            } else {
                t.check(false, "7b /items/0/name value is \"alpha\" (wrong type)");
            }
        }

        auto r2 = resolve_with_validation("/items/2/qty", root, sch_qty);
        t.check(r2.has_value(), "7c /items/2/qty resolves + passes minimum:1");
        if (r2) {
            if (r2.value()->value.index() == 3) {
                t.check(std::get<std::int64_t>(r2.value()->value) == 7,
                        "7d /items/2/qty value is 7");
            } else {
                t.check(false, "7d /items/2/qty value is 7 (wrong type)");
            }
        }

        auto r3 = resolve_with_validation("/items/0/qty", root, sch_qty_bad);
        t.check(!r3.has_value(), "7e /items/0/qty resolves but fails minimum:100");
        if (!r3) {
            t.check(r3.error().kind == SchemaValidatedResolveError::Kind::Schema,
                    "7f Kind=Schema");
            if (r3.error().schema_err) {
                t.check(r3.error().schema_err->instance_path == "/items/0/qty",
                        "7g schema_err.instance_path == /items/0/qty");
                t.check(r3.error().schema_err->kind == JsonSchemaError::BelowMinimum,
                        "7h schema_err.kind == BelowMinimum");
            }
        }

        // Out-of-range deep pointer: gate 1 wins.
        auto r4 = resolve_with_validation("/items/99/name", root, sch_name);
        t.check(!r4.has_value(), "7i out-of-range deep pointer fails (gate 1)");
        if (!r4) {
            t.check(r4.error().kind == SchemaValidatedResolveError::Kind::Pointer,
                    "7j Kind=Pointer");
        }
    }

    // ----------------------------------------------------------------
    // Section 8 — error formatter.
    // ----------------------------------------------------------------
    t.begin_section("error formatter — SchemaValidatedResolveError::format()");

    {
        auto root = parse_json(R"({ "score": 200 })");
        auto sch_max = parse_json(R"({ "type": "integer", "maximum": 100 })");

        auto r1 = resolve_with_validation("/score", root, sch_max);
        t.check(!r1.has_value(), "8a score against maximum:100 fails");
        if (!r1) {
            auto fmt = r1.error().format();
            t.check(fmt.find("resolve_with_validation[schema]") != std::string::npos,
                    "8b format() names the failed gate (schema)");
            t.check(fmt.find("AboveMaximum") != std::string::npos,
                    "8c format() includes the schema error kind");
            t.check(fmt.find("/score") != std::string::npos,
                    "8d format() includes the instance_path");
        }

        auto r2 = resolve_with_validation("/missing", root, sch_max);
        t.check(!r2.has_value(), "8e missing key fails");
        if (!r2) {
            auto fmt = r2.error().format();
            t.check(fmt.find("resolve_with_validation[pointer]") != std::string::npos,
                    "8f format() names the failed gate (pointer)");
            t.check(fmt.find("NotFound") != std::string::npos,
                    "8g format() includes the pointer error");
        }
    }

    // ----------------------------------------------------------------
    // Section 9 — sizeof + feature probes; design invariants.
    // ----------------------------------------------------------------
    t.begin_section("sizeof + feature probes; design invariants");

    {
        t.check(sizeof(psp::JsonValue) > 0,
                "9a psp::JsonValue has non-zero size");

        // The wrapper signature returns expected<const psp::JsonValue*, ...>
        // — the same pointer type as v0.11.0 resolve. The lifetime
        // contract is the same: valid as long as root is alive.
        using ReturnT = std::remove_reference_t<decltype(resolve_with_validation(
            std::declval<std::string_view>(),
            std::declval<const psp::JsonValue&>(),
            std::declval<const psp::JsonValue&>()))>;
        using V11ReturnT = std::remove_reference_t<decltype(
            psp::json_pointer::resolve(std::declval<std::string_view>(),
                                       std::declval<const psp::JsonValue&>()))>;
        t.check(std::is_same_v<typename ReturnT::value_type, typename V11ReturnT::value_type>,
                "9b today's value_type matches v0.11.0 resolve's value_type");
    }

    // =================================================================
    std::printf("\n=================================================\n");
    std::printf("Passed: %d\n", t.passed);
    std::printf("Failed: %d\n", t.failed);
    std::printf("=================================================\n");
    return t.failed == 0 ? 0 : 1;
}