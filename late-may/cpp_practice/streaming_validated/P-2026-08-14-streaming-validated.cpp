// P-2026-08-14 — Consumer of psp_span_lib v0.15.0 that designs the
// STREAMING + SCHEMA-VALIDATED wire-format JSON Patch wrapper:
//
//   psp::json_schema::parse_and_apply_atomic_streaming_validated(
//       JsonValue& root,
//       Span<const char>& doc,
//       const JsonValue& schema)
//       -> std::expected<std::size_t, SchemaValidatedStreamingPatchError>
//
// Where this fits in the arc
// --------------------------
//   - Aug 10: parse_and_apply_atomic_streaming    INVERSE-JOURNAL
//                                                STREAMING wrapper
//   - Aug 11: parse_and_apply_atomic_streaming_   DEEP-CLONE STREAMING
//             deep_clone                          wrapper (Aug 11's
//                                                 chosen rollback
//                                                 mechanism for
//                                                 today's composition)
//   - Aug 12: psp::json_schema::validate          JSON SCHEMA VALIDATION
//                                                 (Draft 2020-12,
//                                                 focused subset;
//                                                 22 enumerators;
//                                                 schema_path +
//                                                 instance_path
//                                                 RFC 6901)
//   - Aug 13: psp::json_schema::validate_atomic   SCHEMA-VALIDATED
//                                                 ATOMIC UPDATE
//                                                 (four-gate in-memory
//                                                 composition)
//   - Aug 14: parse_and_apply_atomic_streaming_   STREAMING + SCHEMA-
//     TODAY  validated                             VALIDATED wire-format
//                                                 atomic update (closes
//                                                 the entire
//                                                 "RFC 6902 + RFC 6901
//                                                 + Draft 2020-12"
//                                                 arc end-to-end)
//
// The Aug 13 lesson's "Where we go next" section explicitly named
// today as the natural next step:
//
//   > "parse_and_apply_atomic_streaming_validated — composes Aug
//   >  10 / Aug 11 (streaming wrappers) with today's
//   >  validate_atomic for a wire-format patch that validates
//   >  end-to-end. This is a single function that would close
//   >  the entire "RFC 6902 + RFC 6901 + Draft 2020-12" arc on
//   >  the wire-format side. Future work, not today's lesson."
//
// Today closes that gap. The composition is correct by
// construction: it reuses Aug 13's validate() /
// validate_with_meta() for both gates (pre-state and post-state)
// and Aug 11's deep-clone streaming wrapper for the streaming
// apply + rollback. The streaming + schema layer is now a
// single function call.
//
// The composition problem
// -----------------------
// Aug 11's `parse_and_apply_atomic_streaming_deep_clone` has
// this shape:
//
//   parse_and_apply_atomic_streaming_deep_clone(root, doc):
//       pre-check: doc starts with '['
//       pre = deep_clone(root)
//       for each streamed op:
//           filter self-moves
//           if engine fails: root = pre; return error
//           if parse fails mid-stream: root = pre; return error
//           if end-of-doc: return applied
//
// Today's wrapper adds the schema layer on top:
//
//   parse_and_apply_atomic_streaming_validated(root, doc, schema):
//       gate 1: validate(root, schema)                     (pre-state)
//       if gate 1 fails: return SchemaValidatedStreamingPatchError
//       pre-check: doc starts with '['
//       pre = deep_clone(root)
//       for each streamed op:
//           filter self-moves
//           if engine fails: root = pre; return error
//           if parse fails mid-stream: root = pre; return error
//           if end-of-doc: break
//       gate 2: validate(root, schema)                     (post-state)
//       if gate 2 fails: root = pre; return SchemaValidatedStreamingPatchError
//       return applied
//
// Why this is more than a one-line composition
// --------------------------------------------
// 1. Three gates (not four): Aug 13's validate_atomic has four
//    gates (pre-validate, dry-run, atomic-apply, post-validate)
//    because patch_atomic and patch_dry_run are SEPARATE engine
//    calls. Today's streaming wrapper is a SINGLE engine call
//    per op (no dry-run on a private clone — streaming does
//    not support a separate dry-run for free). So the gate
//    structure collapses from four gates to three gates:
//    pre-validate, streaming-apply (with deep-clone rollback),
//    post-validate.
//
// 2. The deep-clone snapshot is taken AFTER gate 1 succeeds and
//    BEFORE any op is applied. This is the same "capture the
//    pre-mutation state" pattern as Aug 13's validate_atomic —
//    just earlier in the call (Aug 13 captures after gate 2;
//    today captures after gate 1 because there is no gate 2).
//
// 3. The post-state rollback is to the captured pre_state, NOT
//    to the gate-1-validated pre-state, because by gate 4 we've
//    already mutated root. Same mechanism as Aug 13's gate 4.
//
// 4. The streaming + schema layer reuses Aug 11's streaming
//    parser (parse_patch_document_at / _next_at / parse_one_op_at)
//    + Aug 11's deep-clone rollback + Aug 13's validate() /
//    validate_with_meta() + Aug 13's SchemaErrorContext +
//    JsonSchemaError. No new code paths beyond the composition
//    itself + the new error type.
//
// Why consumer-side today
// -----------------------
// Same shape as Aug 10 / Aug 11 / Aug 13: a proven-in-consumer
// capability that exercises the design end-to-end. Library
// version unchanged at v0.15.0. Future v0.16.0 promotion is
// mechanical (lift SchemaValidatedStreamingPatchError +
// ValidatedStreamingGate + parse_and_apply_atomic_streaming_
// validated + the gate_name helper into a new
// <psp_span/json_schema.h>; bump the version).
//
// API contract
// ------------
//   namespace psp::json_schema {
//       enum class ValidatedStreamingGate { PreValidate, StreamingApply,
//                                           PostValidate };
//       inline std::string_view gate_name(ValidatedStreamingGate g);
//
//       struct SchemaValidatedStreamingPatchError {
//           enum class Kind { Schema, Engine } kind;
//           ValidatedStreamingGate                gate;
//           std::optional<SchemaErrorContext>     schema_err;
//           std::optional<JsonPatchError>         engine_err;
//           std::string format() const;
//       };
//
//       inline std::expected<std::size_t, SchemaValidatedStreamingPatchError>
//       parse_and_apply_atomic_streaming_validated(
//           psp::JsonValue& root,
//           psp::Span<const char>& doc,
//           const psp::JsonValue& schema);
//   }
//
// Observable contract:
//
// - Success: root is mutated to the post-state; the post-state
//   satisfies schema. Returns the applied op count (self-moves
//   do NOT count, same as Aug 10 / Aug 11).
//
// - Gate 1 (PreValidate) failure: root is unchanged (validate
//   takes const&); doc cursor unchanged; returns
//   std::unexpected{SchemaValidatedStreamingPatchError{kind=Schema,
//   gate=PreValidate, schema_err=...}}.
//
// - Gate 2 (StreamingApply) failure: root is restored to the
//   captured pre_state (deep-clone rollback); doc cursor is at
//   the failure point (cursor-primitive contract); returns
//   std::unexpected{SchemaValidatedStreamingPatchError{kind=Engine,
//   gate=StreamingApply, engine_err=...}}.
//
// - Gate 3 (PostValidate) failure: root is restored to the
//   captured pre_state (deep-clone rollback); doc cursor is past
//   ']' (full document consumed); returns
//   std::unexpected{SchemaValidatedStreamingPatchError{kind=Schema,
//   gate=PostValidate, schema_err=...}}.
//
// What the consumer exercises
// ---------------------------
//   Section 1 — symbol-presence + signature probes (the new
//               wrapper signature; the three ValidatedStreaming
//               Gate enumerators; gate_name).
//   Section 2 — happy path: a wire-format patch document with
//               2 ops that all succeed; schema-valid pre-state
//               and post-state. Returns applied count = 2.
//   Section 3 — gate 1 (PreValidate) failure: pre-state fails
//               the schema; root untouched; doc cursor
//               untouched. SchemaValidatedStreamingPatchError.
//   Section 4 — gate 2 (StreamingApply) failure mid-stream:
//               second op fails the engine; root restored to
//               pre_state; cursor at failure point.
//   Section 5 — gate 3 (PostValidate) failure: engine succeeds
//               for every op but the final state violates the
//               schema (the uniqueItems duplicate-add case).
//               Root is restored to pre_state via deep-clone.
//   Section 6 — drop-in equivalence on a permissive `{}` schema:
//               today's wrapper == Aug 11's streaming wrapper
//               on the success path AND on the rollback path
//               (a permissive schema makes gates 1 and 3 no-ops,
//               so today reduces to Aug 11).
//   Section 7 — multi-op gate-3 rollback: a 3-op wire-format
//               patch where op #3 makes the tree violate
//               uniqueItems. Ops #1 and #2 must roll back too;
//               root is byte-identical to pre_state.
//   Section 8 — error formatter (`SchemaValidatedStreamingPatch
//               Error::format`) is human-readable: names the
//               failed gate + the underlying schema/engine
//               error + the diagnostic labels.
//   Section 9 — sizeof / feature probes; design invariants.
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

#include <algorithm>
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
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// JsonSchemaError — typed failure payload for validate()
// ===========================================================================
// (Mirror of Aug 12's enum, lifted from the Aug 13 consumer. 22 enumerators.)
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

// (check_keyword stub removed — was unused. The keyword-level
// checks are inlined into validate_with_meta where they're
// actually consumed.)

// validate_with_meta — internal recursion; returns the full
// SchemaErrorContext (paths + kind). Mirrors Aug 12.
inline std::expected<void, SchemaErrorContext>
validate_with_meta(const psp::JsonValue& instance,
                   const psp::JsonValue& schema,
                   std::string_view instance_path,
                   std::string_view schema_path) {

    // type (single-string or array)
    if (!check_type(schema, "null") && matches_type(instance, "null")) {
        // fall through; will fail below
    }
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

        // properties: recurse
        if (find_field(schema, "properties", sv)) {
            if (const auto* props = std::get_if<std::map<std::string, psp::JsonValue>>(&sv->value); props) {
                for (const auto& [k, sub_schema] : *props) {
                    const psp::JsonValue* child = nullptr;
                    if (find_field(instance, k, child)) {
                        auto r = validate_with_meta(*child, sub_schema,
                                                    join_path(instance_path, k),
                                                    join_path(schema_path, std::string("properties") + "/" + k));
                        if (!r) return r;
                    }
                }
            }
        }

        // additionalProperties
        const psp::JsonValue* ap = nullptr;
        if (find_field(schema, "additionalProperties", ap)) {
            if (const auto* ab = std::get_if<bool>(&ap->value); ab && !*ab) {
                const psp::JsonValue* props_v = nullptr;
                std::set<std::string> known;
                if (find_field(schema, "properties", props_v)) {
                    if (const auto* props = std::get_if<std::map<std::string, psp::JsonValue>>(&props_v->value); props) {
                        for (const auto& [k, _] : *props) known.insert(k);
                    }
                }
                for (const auto& [k, _] : *obj) {
                    if (!known.contains(k)) {
                        return std::unexpected{SchemaErrorContext{
                            JsonSchemaError::AdditionalProperty,
                            std::string{schema_path} + "/additionalProperties",
                            join_path(instance_path, k)}};
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
        if (find_field(schema, "uniqueItems", sv)) {
            if (const auto* u = std::get_if<bool>(&sv->value); u && *u) {
                for (std::size_t i = 0; i < arr->size(); ++i) {
                    for (std::size_t j = i + 1; j < arr->size(); ++j) {
                        if ((*arr)[i] == (*arr)[j]) {
                            return std::unexpected{SchemaErrorContext{
                                JsonSchemaError::NotUniqueItems,
                                std::string{schema_path} + "/uniqueItems",
                                join_index(instance_path, j)}};
                        }
                    }
                }
            }
        }
        if (find_field(schema, "items", sv)) {
            for (std::size_t i = 0; i < arr->size(); ++i) {
                auto r = validate_with_meta((*arr)[i], *sv,
                                            join_index(instance_path, i),
                                            join_path(schema_path, "items"));
                if (!r) {
                    // Wrap ItemsMismatch around the underlying error.
                    auto inner = std::move(r).error();
                    SchemaErrorContext wrapped{
                        JsonSchemaError::ItemsMismatch,
                        std::string{schema_path} + "/items",
                        std::string{instance_path}};
                    return std::unexpected{wrapped};
                }
            }
        }
    }

    // Composition keywords: allOf / anyOf / oneOf / not
    const psp::JsonValue* cv = nullptr;
    if (find_field(schema, "allOf", cv)) {
        if (const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&cv->value); arr) {
            std::string allof_base = std::string{schema_path} + "/allOf";
            for (std::size_t i = 0; i < arr->size(); ++i) {
                auto r = validate_with_meta(instance, (*arr)[i],
                                            instance_path,
                                            join_index(allof_base, i));
                if (!r) return r;
            }
        }
    }
    if (find_field(schema, "anyOf", cv)) {
        if (const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&cv->value); arr) {
            bool any = false;
            std::string anyof_base = std::string{schema_path} + "/anyOf";
            for (std::size_t i = 0; i < arr->size(); ++i) {
                auto r = validate_with_meta(instance, (*arr)[i],
                                            instance_path,
                                            join_index(anyof_base, i));
                if (r) { any = true; break; }
            }
            if (!any) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::AnyOfFailed,
                    std::string{schema_path} + "/anyOf",
                    std::string{instance_path}}};
            }
        }
    }
    if (find_field(schema, "oneOf", cv)) {
        if (const auto* arr = std::get_if<std::vector<psp::JsonValue>>(&cv->value); arr) {
            std::size_t matches = 0;
            std::string oneof_base = std::string{schema_path} + "/oneOf";
            for (std::size_t i = 0; i < arr->size(); ++i) {
                auto r = validate_with_meta(instance, (*arr)[i],
                                            instance_path,
                                            join_index(oneof_base, i));
                if (r) ++matches;
            }
            if (matches == 0) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::OneOfNoMatch,
                    std::string{schema_path} + "/oneOf",
                    std::string{instance_path}}};
            }
            if (matches > 1) {
                return std::unexpected{SchemaErrorContext{
                    JsonSchemaError::OneOfMultipleMatch,
                    std::string{schema_path} + "/oneOf",
                    std::string{instance_path}}};
            }
        }
    }
    if (find_field(schema, "not", cv)) {
        auto r = validate_with_meta(instance, *cv, instance_path,
                                    join_path(schema_path, "not"));
        if (r) {
            return std::unexpected{SchemaErrorContext{
                JsonSchemaError::NotFailed,
                std::string{schema_path} + "/not",
                std::string{instance_path}}};
        }
    }

    return {};
}

// validate — narrow return type (just the JsonSchemaError code).
// Mirrors Aug 12 / Aug 13.
inline std::expected<void, JsonSchemaError>
validate(const psp::JsonValue& instance, const psp::JsonValue& schema) {
    auto r = validate_with_meta(instance, schema, "", "");
    if (!r) {
        return std::unexpected{std::move(r).error().kind};
    }
    return {};
}

}  // namespace json_schema
}  // namespace psp

// ===========================================================================
// Aug 11 streaming layer (deep-clone variant)
// ===========================================================================
//
// Mirrors the Aug 11 consumer-side helper namespace. Three
// streaming parser functions + the deep-clone helper + the
// `parse_and_apply_atomic_streaming_deep_clone` wrapper that
// today's composition reuses for the StreamingApply gate.

namespace psp {
namespace json_patch {

// deep_clone — recursive copy of a JsonValue tree.
inline psp::JsonValue deep_clone(const psp::JsonValue& v) {
    psp::JsonValue out;
    std::visit([&out](const auto& alt) {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            out.value.emplace<std::monostate>();
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            out.value.emplace<std::nullptr_t>(alt);
        } else if constexpr (std::is_same_v<T, bool>) {
            out.value.emplace<bool>(alt);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            out.value.emplace<std::int64_t>(alt);
        } else if constexpr (std::is_same_v<T, double>) {
            out.value.emplace<double>(alt);
        } else if constexpr (std::is_same_v<T, std::string>) {
            out.value.emplace<std::string>(alt);
        } else if constexpr (std::is_same_v<T, std::vector<psp::JsonValue>>) {
            std::vector<psp::JsonValue> cloned;
            cloned.reserve(alt.size());
            for (const auto& child : alt) {
                cloned.push_back(deep_clone(child));
            }
            out.value.emplace<std::vector<psp::JsonValue>>(std::move(cloned));
        } else if constexpr (std::is_same_v<T, std::map<std::string, psp::JsonValue>>) {
            std::map<std::string, psp::JsonValue> cloned;
            for (const auto& [k, child] : alt) {
                cloned.emplace(k, deep_clone(child));
            }
            out.value.emplace<std::map<std::string, psp::JsonValue>>(std::move(cloned));
        }
    }, v.value);
    return out;
}

namespace detail {

inline std::expected<JsonPatchOp, JsonPatchError>
build_one_op(const psp::JsonValue& v) noexcept {
    const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&v.value);
    if (!obj) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    auto it = obj->find("op");
    if (it == obj->end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto* op_str = std::get_if<std::string>(&it->second.value);
    if (!op_str) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    auto pit = obj->find("path");
    if (pit == obj->end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto* path = std::get_if<std::string>(&pit->second.value);
    if (!path) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    auto fit_from = obj->find("from");
    auto fit_value = obj->find("value");

    if (*op_str == "add") {
        if (fit_value == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        return JsonPatchOp{AddOp{*path, psp::json_patch::deep_clone(fit_value->second)}};
    }
    if (*op_str == "remove") {
        return JsonPatchOp{RemoveOp{*path}};
    }
    if (*op_str == "replace") {
        if (fit_value == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        return JsonPatchOp{ReplaceOp{*path, psp::json_patch::deep_clone(fit_value->second)}};
    }
    if (*op_str == "move") {
        if (fit_from == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        const auto* from = std::get_if<std::string>(&fit_from->second.value);
        if (!from) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{MoveOp{*path, *from}};
    }
    if (*op_str == "copy") {
        if (fit_from == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        const auto* from = std::get_if<std::string>(&fit_from->second.value);
        if (!from) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{CopyOp{*path, *from}};
    }
    if (*op_str == "test") {
        if (fit_value == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        return JsonPatchOp{TestOp{*path, psp::json_patch::deep_clone(fit_value->second)}};
    }
    return std::unexpected{JsonPatchError::UnknownOp};
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at_impl(psp::Span<const char>& s) noexcept {
    auto entry = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    auto v = psp::parse_value_at(s);
    if (!v) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    auto op = build_one_op(*v);
    if (!op) {
        s = entry;
        return std::unexpected{op.error()};
    }
    return op;
}

}  // namespace detail

inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at(psp::Span<const char>& s) noexcept {
    return psp::json_patch::detail::parse_one_op_at_impl(s);
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_at(psp::Span<const char>& s) noexcept {
    auto entry = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (s.empty() || s.front() != '[') {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    s = s.subspan(1);

    auto checkpoint = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    s = checkpoint;

    auto op = psp::json_patch::detail::parse_one_op_at_impl(s);
    if (!op) {
        s = entry;
        return std::unexpected{op.error()};
    }

    auto after = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    } else {
        s = after;
    }
    return op;
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_next_at(psp::Span<const char>& s) noexcept {
    auto entry = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    auto op = psp::json_patch::detail::parse_one_op_at_impl(s);
    if (!op) {
        s = entry;
        return std::unexpected{op.error()};
    }
    auto after = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    } else {
        s = after;
    }
    return op;
}

// is_self_move — a MoveOp where from == path is a no-op per RFC 6902 §4.4.
inline bool
is_self_move(const ::JsonPatchOp& op) noexcept {
    if (op.kind != ::OpKind::Move) return false;
    const auto& m = std::get< ::MoveOp>(op.data);
    return m.from == m.path;
}

// parse_and_apply_atomic_streaming_deep_clone — Aug 11's wrapper,
// lifted verbatim. Today's wrapper reuses the same deep-clone
// rollback mechanism for gate 2. Today's wrapper ALSO calls
// this from Section 6's drop-in equivalence test.
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming_deep_clone(psp::JsonValue& root,
                                            psp::Span<const char>& doc) noexcept {
    {
        auto probe = doc;
        if (auto r = psp::skip_whitespace_at(probe); !r) {
            return std::unexpected{JsonPatchError::BadDocument};
        }
        if (probe.empty() || probe.front() != '[') {
            return std::unexpected{JsonPatchError::BadDocument};
        }
    }

    psp::JsonValue pre = psp::json_patch::deep_clone(root);

    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        if (first.error() == JsonPatchError::BadDocument) {
            return std::size_t{0};
        }
        return std::unexpected{first.error()};
    }

    std::size_t applied = 0;
    JsonPatchOp op = *first;

    for (;;) {
        if (!psp::json_patch::is_self_move(op)) {
            auto r = psp::json_patch::patch(root,
                std::span<const JsonPatchOp>{&op, 1});
            if (!r) {
                root.value = pre.value;
                return std::unexpected{r.error()};
            }
            ++applied;
        }

        auto next = psp::json_patch::parse_patch_document_next_at(doc);
        if (!next) {
            if (next.error() == JsonPatchError::BadDocument) {
                return applied;
            }
            root.value = pre.value;
            return std::unexpected{next.error()};
        }
        op = *next;
    }
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// TODAY'S COMPOSITION: parse_and_apply_atomic_streaming_validated
// ===========================================================================
//
// Reuses Aug 11's deep-clone streaming parser + Aug 13's
// validate() / validate_with_meta(). Three gates (not four):
//   - PreValidate:  pre-state must be schema-valid
//   - StreamingApply: each op applied with deep-clone rollback
//   - PostValidate: post-state must be schema-valid; rollback
//                    to the up-front-captured pre_state on failure
//
// Same observable contract as Aug 11's deep-clone wrapper on the
// success path AND on the engine-failure rollback path. The
// schema layer adds gate 1 and gate 3 on top.

namespace psp {
namespace json_schema {

enum class ValidatedStreamingGate {
    PreValidate,      // gate 1: pre-state must be schema-valid
    StreamingApply,   // gate 2: each op applied with deep-clone rollback
    PostValidate,     // gate 3: post-state must be schema-valid
};

inline std::string_view gate_name(ValidatedStreamingGate g) {
    switch (g) {
        case ValidatedStreamingGate::PreValidate:    return "pre-validate";
        case ValidatedStreamingGate::StreamingApply: return "streaming-apply";
        case ValidatedStreamingGate::PostValidate:   return "post-validate";
    }
    return "?";
}

struct SchemaValidatedStreamingPatchError {
    enum class Kind { Schema, Engine } kind;
    ValidatedStreamingGate                       gate;
    std::optional<SchemaErrorContext>            schema_err;
    std::optional<JsonPatchError>                engine_err;

    std::string format() const {
        std::string out;
        out.append("parse_and_apply_atomic_streaming_validated[");
        out.append(std::string{gate_name(gate)});
        out.append("] failed: ");
        if (kind == Kind::Schema) {
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
        } else {
            if (engine_err) {
                out.append(std::format("{}", *engine_err));
            } else {
                out.append("engine error (no detail)");
            }
        }
        return out;
    }
};

inline std::expected<std::size_t, SchemaValidatedStreamingPatchError>
parse_and_apply_atomic_streaming_validated(
    psp::JsonValue& root,
    psp::Span<const char>& doc,
    const psp::JsonValue& schema) {

    // Gate 1 — pre-state must already satisfy the schema.
    // validate() does not mutate, so a gate-1 failure trivially
    // leaves root and the doc cursor untouched.
    {
        auto pre = psp::json_schema::validate_with_meta(root, schema, "", "");
        if (!pre) {
            return std::unexpected{SchemaValidatedStreamingPatchError{
                SchemaValidatedStreamingPatchError::Kind::Schema,
                ValidatedStreamingGate::PreValidate,
                std::move(pre).error(),
                std::nullopt}};
        }
    }

    // Pre-check: the document must start with '[' (with optional
    // leading whitespace). If not, it's a real parse failure —
    // NOT end-of-doc. This disambiguates the two BadDocument
    // cases the cursor-primitive parser returns indistinguishably.
    {
        auto probe = doc;
        if (auto r = psp::skip_whitespace_at(probe); !r) {
            return std::unexpected{SchemaValidatedStreamingPatchError{
                SchemaValidatedStreamingPatchError::Kind::Engine,
                ValidatedStreamingGate::StreamingApply,
                std::nullopt,
                JsonPatchError::BadDocument}};
        }
        if (probe.empty() || probe.front() != '[') {
            return std::unexpected{SchemaValidatedStreamingPatchError{
                SchemaValidatedStreamingPatchError::Kind::Engine,
                ValidatedStreamingGate::StreamingApply,
                std::nullopt,
                JsonPatchError::BadDocument}};
        }
    }

    // Capture the pre-state ONCE. This is the one-time cost of
    // the deep-clone variant. On any failure past gate 1, root
    // is restored from this snapshot. On success, it's dropped
    // (RAII).
    psp::JsonValue pre_state = psp::json_patch::deep_clone(root);

    // Gate 2 — streaming apply with deep-clone rollback on failure.
    // Reuses Aug 11's streaming parser + self-move filter.
    std::size_t applied = 0;

    // Begin: parse the first op. The BEGIN call sees '['.
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        if (first.error() == JsonPatchError::BadDocument) {
            // End-of-doc ('[]'): success with 0 ops. Then we
            // still need to validate gate 3 against the
            // unchanged root.
            goto post_validate;
        }
        // Real parse failure: no op was applied, no rollback
        // needed (root is the pre-state by construction).
        return std::unexpected{SchemaValidatedStreamingPatchError{
            SchemaValidatedStreamingPatchError::Kind::Engine,
            ValidatedStreamingGate::StreamingApply,
            std::nullopt,
            first.error()}};
    }

    {
        JsonPatchOp op = *first;
        for (;;) {
            if (!psp::json_patch::is_self_move(op)) {
                auto r = psp::json_patch::patch(root,
                    std::span<const JsonPatchOp>{&op, 1});
                if (!r) {
                    // Engine failed. Restore the pre-state.
                    root.value = pre_state.value;
                    return std::unexpected{SchemaValidatedStreamingPatchError{
                        SchemaValidatedStreamingPatchError::Kind::Engine,
                        ValidatedStreamingGate::StreamingApply,
                        std::nullopt,
                        r.error()}};
                }
                ++applied;
            }

            auto next = psp::json_patch::parse_patch_document_next_at(doc);
            if (!next) {
                if (next.error() == JsonPatchError::BadDocument) {
                    // End-of-doc: success, doc is fully applied.
                    break;
                }
                // Real parse failure. Restore the pre-state.
                root.value = pre_state.value;
                return std::unexpected{SchemaValidatedStreamingPatchError{
                    SchemaValidatedStreamingPatchError::Kind::Engine,
                    ValidatedStreamingGate::StreamingApply,
                    std::nullopt,
                    next.error()}};
            }
            op = *next;
        }
    }

post_validate:
    // Gate 3 — post-state must satisfy the schema. On failure,
    // restore root from the up-front captured pre_state.
    {
        auto post = psp::json_schema::validate_with_meta(root, schema, "", "");
        if (!post) {
            root.value = pre_state.value;
            return std::unexpected{SchemaValidatedStreamingPatchError{
                SchemaValidatedStreamingPatchError::Kind::Schema,
                ValidatedStreamingGate::PostValidate,
                std::move(post).error(),
                std::nullopt}};
        }
    }

    return applied;
}

}  // namespace json_schema
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

psp::JsonValue make_initial() {
    return psp::JsonValue{std::map<std::string, psp::JsonValue>{
        {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
            {"k", psp::JsonValue{std::int64_t(42)}},
        }}},
    }};
}

struct Doc {
    std::string text;
    psp::Span<const char> view() noexcept {
        return psp::Span<const char>{text.data(), text.size()};
    }
};

// find_path — returns true if the path exists in the tree, with the
// resolved value. Used for verifying state after apply/rollback.
bool find_path(const psp::JsonValue& root, std::string_view path,
               psp::JsonValue& out) {
    auto found = psp::json_pointer::resolve(std::string{path}, root);
    if (!found) return false;
    out.value = (*found)->value;
    return true;
}

bool path_exists(const psp::JsonValue& root, std::string_view path) {
    psp::JsonValue dummy;
    return find_path(root, path, dummy);
}

}  // namespace

int main() {
    std::printf("P-2026-08-14 — streaming + schema-validated JSON Patch\n"
                "  (closes the 'parse_and_apply_atomic_streaming_validated'\n"
                "  item on the Aug 13 (P-2026-08-13) 'Where we go next'\n"
                "  list; same observable contract as Aug 11's\n"
                "  parse_and_apply_atomic_streaming_deep_clone on the\n"
                "  success path; composes Aug 11's deep-clone streaming\n"
                "  apply with Aug 13's validate() / validate_atomic\n"
                "  four-gate composition; library version unchanged at\n"
                "  v0.15.0)\n");

    TestCounter t;

    // ----------------------------------------------------------------
    // Section 1 — symbol-presence + signature probes.
    // ----------------------------------------------------------------
    t.begin_section("symbol-presence + signature probes");

    using psp::json_schema::parse_and_apply_atomic_streaming_validated;
    using psp::json_schema::SchemaValidatedStreamingPatchError;
    using psp::json_schema::ValidatedStreamingGate;

    {
        auto fn = &parse_and_apply_atomic_streaming_validated;
        t.check(fn != nullptr,
                "1a &psp::json_schema::parse_and_apply_atomic_streaming_validated "
                "is well-defined");
    }

    t.check(std::is_same_v<
              std::remove_reference_t<decltype(parse_and_apply_atomic_streaming_validated(
                  std::declval<psp::JsonValue&>(),
                  std::declval<psp::Span<const char>&>(),
                  std::declval<const psp::JsonValue&>()))>,
              std::expected<std::size_t, SchemaValidatedStreamingPatchError>>,
          "1b parse_and_apply_atomic_streaming_validated signature matches "
          "std::expected<std::size_t, SchemaValidatedStreamingPatchError>");

    // All three ValidatedStreamingGate enumerators are distinct.
    t.check(ValidatedStreamingGate::PreValidate    != ValidatedStreamingGate::StreamingApply,
            "1c PreValidate != StreamingApply (gate enum has 3 distinct values)");
    t.check(ValidatedStreamingGate::StreamingApply != ValidatedStreamingGate::PostValidate,
            "1d StreamingApply != PostValidate (gate enum has 3 distinct values)");
    t.check(ValidatedStreamingGate::PreValidate    != ValidatedStreamingGate::PostValidate,
            "1e PreValidate != PostValidate (gate enum has 3 distinct values)");

    // gate_name returns a non-empty string for each gate.
    t.check(!psp::json_schema::gate_name(ValidatedStreamingGate::PreValidate).empty(),
            "1f gate_name(PreValidate) returns a non-empty string");
    t.check(!psp::json_schema::gate_name(ValidatedStreamingGate::StreamingApply).empty(),
            "1g gate_name(StreamingApply) returns a non-empty string");
    t.check(!psp::json_schema::gate_name(ValidatedStreamingGate::PostValidate).empty(),
            "1h gate_name(PostValidate) returns a non-empty string");

    // Both Kind values are reachable.
    t.check(SchemaValidatedStreamingPatchError::Kind::Schema !=
            SchemaValidatedStreamingPatchError::Kind::Engine,
            "1i Schema != Engine (Kind enum has 2 distinct values)");

    // ----------------------------------------------------------------
    // Section 2 — happy path: 2-op wire-format patch, all succeed.
    // ----------------------------------------------------------------
    t.begin_section("happy path — 2-op wire-format patch, schema-valid pre and post");

    {
        auto root = parse_json(R"({
            "name":  "alpha",
            "tags":  ["red", "green"],
            "score": 50
        })");
        auto schema = parse_json(R"({
            "type": "object",
            "required": ["name", "tags", "score"],
            "properties": {
                "name":  {"type": "string", "minLength": 1},
                "tags":  {"type": "array",  "items": {"type": "string"}, "uniqueItems": true},
                "score": {"type": "integer", "minimum": 0, "maximum": 100}
            }
        })");

        Doc d{std::string{R"([
            {"op": "add",     "path": "/tags/-", "value": "blue"},
            {"op": "replace", "path": "/score",  "value": 75}
        ])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);

        t.check(r.has_value(),
                "2a wire-format 2-op patch applies successfully");
        t.check_eq_i(r ? *r : 0, std::size_t{2},
                     "2b applied count == 2");
        t.check(psp::json_schema::validate(root, schema).has_value(),
                "2c post-state is schema-valid");
        bool got75 = false;
        if (auto pm = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value)) {
            auto it = pm->find("score");
            if (it != pm->end()) {
                if (auto pi = std::get_if<std::int64_t>(&it->second.value)) {
                    got75 = (*pi == 75);
                }
            }
        }
        t.check(got75, "2d score == 75 in post-state");
        bool tags3 = false;
        if (auto pm = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value)) {
            auto it = pm->find("tags");
            if (it != pm->end()) {
                if (auto pv = std::get_if<std::vector<psp::JsonValue>>(&it->second.value)) {
                    tags3 = (pv->size() == 3);
                }
            }
        }
        t.check(tags3, "2e tags array size == 3 in post-state");
    }

    // Empty document: success, 0 ops applied, schema-valid pre/post
    // means validate(root, schema) was called before and after.
    {
        auto root = parse_json(R"({"a": 1})");
        auto schema = parse_json(R"({"type": "object"})");
        Doc d{std::string{R"([])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);
        t.check(r.has_value(),
                "2f empty wire-format document applies successfully");
        t.check_eq_i(r ? *r : 0, std::size_t{0},
                     "2g applied count == 0");
    }

    // ----------------------------------------------------------------
    // Section 3 — gate 1 (PreValidate) failure: pre-state fails
    // the schema; root untouched; doc cursor untouched.
    // ----------------------------------------------------------------
    t.begin_section("gate 1 (PreValidate) failure — pre-state invalid");

    {
        auto root = parse_json(R"({"name": "", "score": 50})");
        psp::JsonValue snapshot = psp::json_patch::deep_clone(root);

        auto schema = parse_json(R"({
            "type": "object",
            "required": ["name", "score"],
            "properties": {"name": {"type": "string", "minLength": 1}}
        })");

        Doc d{std::string{R"([
            {"op": "replace", "path": "/score", "value": 99}
        ])"}};
        auto sp = d.view();
        std::size_t sp_before_size = sp.size();

        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);

        t.check(!r.has_value(),
                "3a gate-1 failure returns an error");
        if (!r) {
            t.check(r.error().kind ==
                    SchemaValidatedStreamingPatchError::Kind::Schema,
                    "3b error.kind == Schema");
            t.check(r.error().gate == ValidatedStreamingGate::PreValidate,
                    "3c error.gate == PreValidate");
            t.check(r.error().schema_err.has_value(),
                    "3d schema_err is populated");
            t.check(r.error().schema_err->kind == JsonSchemaError::StringTooShort,
                    "3e underlying schema error is StringTooShort");
        }
        t.check(root == snapshot,
                "3f root is byte-identical to the pre-call snapshot");
        t.check_eq_i(sp.size(), sp_before_size,
                     "3g doc cursor is unchanged (gate 1 doesn't consume)");
    }

    // ----------------------------------------------------------------
    // Section 4 — gate 2 (StreamingApply) failure mid-stream:
    // second op fails the engine; root restored to pre_state;
    // cursor at failure point.
    // ----------------------------------------------------------------
    t.begin_section("gate 2 (StreamingApply) failure mid-stream");

    {
        auto root = parse_json(R"({"x": {"k": 42}})");
        psp::JsonValue snapshot = psp::json_patch::deep_clone(root);

        auto schema = parse_json(R"({"type": "object"})");

        // First op succeeds (add /temp). Second op fails
        // (remove /nonexistent).
        Doc d{std::string{R"([
            {"op": "add",    "path": "/temp",   "value": "x"},
            {"op": "remove", "path": "/nonexistent"}
        ])"}};
        auto sp = d.view();
        std::size_t sp_before_size = sp.size();

        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);

        t.check(!r.has_value(),
                "4a gate-2 failure returns an error");
        if (!r) {
            t.check(r.error().kind ==
                    SchemaValidatedStreamingPatchError::Kind::Engine,
                    "4b error.kind == Engine");
            t.check(r.error().gate == ValidatedStreamingGate::StreamingApply,
                    "4c error.gate == StreamingApply");
            t.check(r.error().engine_err.has_value(),
                    "4d engine_err is populated");
            t.check(r.error().engine_err.value() == JsonPatchError::PointerNotFound,
                    "4e underlying engine error is PointerNotFound");
        }
        t.check(root == snapshot,
                "4f root is restored to the pre-state snapshot");
        t.check(sp.size() < sp_before_size,
                "4g doc cursor advanced past the first op (gate 2 consumed)");
    }

    // ----------------------------------------------------------------
    // Section 5 — gate 3 (PostValidate) failure: engine succeeds
    // for every op but the final state violates the schema.
    // Root is restored to pre_state via deep-clone.
    // ----------------------------------------------------------------
    t.begin_section("gate 3 (PostValidate) failure — post-state schema-invalid");

    {
        auto root = parse_json(R"({
            "name":  "alpha",
            "tags":  ["red", "green", "blue"],
            "score": 50
        })");
        psp::JsonValue snapshot = psp::json_patch::deep_clone(root);

        auto schema = parse_json(R"({
            "type": "object",
            "required": ["name", "tags", "score"],
            "properties": {
                "tags": {"type": "array", "items": {"type": "string"}, "uniqueItems": true}
            }
        })");
        // Add a duplicate tag — engine accepts, post-state breaks uniqueItems.
        Doc d{std::string{R"([
            {"op": "add", "path": "/tags/-", "value": "red"}
        ])"}};
        auto sp = d.view();

        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);

        t.check(!r.has_value(),
                "5a gate-3 failure returns an error");
        if (!r) {
            t.check(r.error().kind ==
                    SchemaValidatedStreamingPatchError::Kind::Schema,
                    "5b error.kind == Schema");
            t.check(r.error().gate == ValidatedStreamingGate::PostValidate,
                    "5c error.gate == PostValidate");
            t.check(r.error().schema_err.has_value(),
                    "5d schema_err is populated");
            t.check(r.error().schema_err->kind == JsonSchemaError::NotUniqueItems,
                    "5e underlying schema error is NotUniqueItems");
        }
        t.check(root == snapshot,
                "5f root is byte-identical to the pre-call snapshot (post-validate rollback works)");
    }

    // ----------------------------------------------------------------
    // Section 6 — drop-in equivalence on a permissive `{}` schema:
    // today's wrapper == Aug 11's streaming wrapper on the
    // success path AND on the rollback path.
    // ----------------------------------------------------------------
    t.begin_section("drop-in equivalence on {} schema (no constraints)");

    {
        auto root_a = parse_json(R"({"a": 1, "b": 2})");
        auto root_b = parse_json(R"({"a": 1, "b": 2})");
        auto schema = parse_json(R"({})");

        Doc da{std::string{R"([
            {"op": "replace", "path": "/a", "value": 99},
            {"op": "add",     "path": "/c", "value": 3}
        ])"}};
        auto sa = da.view();
        Doc db{std::string{R"([
            {"op": "replace", "path": "/a", "value": 99},
            {"op": "add",     "path": "/c", "value": 3}
        ])"}};
        auto sb = db.view();

        auto ra = psp::json_schema::parse_and_apply_atomic_streaming_validated(root_a, sa, schema);
        auto rb = psp::json_patch::parse_and_apply_atomic_streaming_deep_clone(root_b, sb);

        t.check(ra.has_value() && rb.has_value(),
                "6a both wrappers return success on a permissive {} schema");
        t.check_eq_i(ra ? *ra : 0, rb ? *rb : 0,
                     "6b applied count matches Aug 11's deep-clone streaming wrapper");
        t.check(root_a == root_b,
                "6c post-state matches Aug 11's deep-clone streaming wrapper byte-for-byte");
    }

    // Drop-in equivalence on the rollback path: 3rd op fails the
    // engine; today's wrapper and Aug 11's wrapper should both
    // restore root to pre-state.
    {
        auto root_a = make_initial();
        auto root_b = make_initial();
        auto schema = parse_json(R"({})");

        Doc da{std::string{R"([
            {"op": "add",    "path": "/temp", "value": "x"},
            {"op": "remove", "path": "/nonexistent"}
        ])"}};
        auto sa = da.view();
        Doc db{std::string{R"([
            {"op": "add",    "path": "/temp", "value": "x"},
            {"op": "remove", "path": "/nonexistent"}
        ])"}};
        auto sb = db.view();

        auto ra = psp::json_schema::parse_and_apply_atomic_streaming_validated(root_a, sa, schema);
        auto rb = psp::json_patch::parse_and_apply_atomic_streaming_deep_clone(root_b, sb);

        t.check(!ra.has_value() && !rb.has_value(),
                "6d both wrappers return an error on engine failure");
        t.check(root_a == root_b,
                "6e post-state rollback matches Aug 11's wrapper byte-for-byte");
    }

    // ----------------------------------------------------------------
    // Section 7 — multi-op gate-3 rollback: a 3-op wire-format
    // patch where op #3 makes the tree violate uniqueItems. Ops
    // #1 and #2 must roll back too; root is byte-identical to
    // pre_state.
    // ----------------------------------------------------------------
    t.begin_section("multi-op gate-3 rollback on post-validate");

    {
        auto root = parse_json(R"({"tags": ["a", "b"]})");
        psp::JsonValue snapshot = psp::json_patch::deep_clone(root);

        auto schema = parse_json(R"({
            "type": "object",
            "properties": {"tags": {"type": "array", "uniqueItems": true}}
        })");
        Doc d{std::string{R"([
            {"op": "add", "path": "/tags/-", "value": "c"},
            {"op": "add", "path": "/tags/-", "value": "d"},
            {"op": "add", "path": "/tags/-", "value": "a"}
        ])"}};
        auto sp = d.view();

        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);

        t.check(!r.has_value(),
                "7a multi-op patch that violates uniqueItems at op #3 fails post-validate");
        if (!r) {
            t.check(r.error().gate == ValidatedStreamingGate::PostValidate,
                    "7b gate == PostValidate");
        }
        t.check(root == snapshot,
                "7c all three ops are rolled back; root is byte-identical to pre-state");
    }

    // ----------------------------------------------------------------
    // Section 8 — error formatter (SchemaValidatedStreamingPatch
    // Error::format) is human-readable.
    // ----------------------------------------------------------------
    t.begin_section("error format() is human-readable");

    {
        auto root = parse_json(R"({"name": ""})");
        auto schema = parse_json(R"({
            "type": "object",
            "properties": {"name": {"type": "string", "minLength": 1}}
        })");
        Doc d{std::string{R"([
            {"op": "replace", "path": "/name", "value": "alpha"}
        ])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);
        t.check(!r.has_value(), "8a gate-1 failure returns an error");
        if (!r) {
            std::string msg = r.error().format();
            t.check(!msg.empty(),
                    "8b format() returns a non-empty string");
            t.check(msg.find("pre-validate") != std::string::npos,
                    "8c format() names the failed gate");
            t.check(msg.find("StringTooShort") != std::string::npos,
                    "8d format() names the underlying schema error");
            t.check(msg.find("instance_path") != std::string::npos,
                    "8e format() includes the diagnostic label 'instance_path'");
        }
    }

    {
        auto root = parse_json(R"({"x": 1})");
        auto schema = parse_json(R"({"type": "object"})");
        Doc d{std::string{R"([
            {"op": "remove", "path": "/missing"}
        ])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);
        t.check(!r.has_value(), "8f gate-2 failure returns an error");
        if (!r) {
            std::string msg = r.error().format();
            t.check(!msg.empty(),
                    "8g format() returns a non-empty string");
            t.check(msg.find("streaming-apply") != std::string::npos,
                    "8h format() names the failed gate (streaming-apply)");
        }
    }

    // ----------------------------------------------------------------
    // Section 9 — sizeof / feature probes; design invariants.
    // ----------------------------------------------------------------
    t.begin_section("sizeof + feature probes; design invariants");

    {
        // The deep-clone snapshot is taken ONCE per call, not
        // per-op. A wire-format patch with N ops allocates one
        // deep_clone before the first op; no per-op clone.
        auto root = make_initial();
        auto schema = parse_json(R"({"type": "object"})");
        Doc d{std::string{R"([
            {"op": "add", "path": "/a", "value": 1},
            {"op": "add", "path": "/b", "value": 2},
            {"op": "add", "path": "/c", "value": 3},
            {"op": "add", "path": "/d", "value": 4},
            {"op": "add", "path": "/e", "value": 5}
        ])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);
        t.check(r.has_value(),
                "9a 5-op wire-format patch applies successfully");
        t.check_eq_i(r ? *r : 0, std::size_t{5},
                     "9b applied count == 5");
        t.check(path_exists(root, "/a") &&
                path_exists(root, "/b") &&
                path_exists(root, "/c") &&
                path_exists(root, "/d") &&
                path_exists(root, "/e"),
                "9c all five /a, /b, /c, /d, /e are in post-state");
    }

    // Self-moves are filtered out by Aug 11's parser layer; today
    // inherits this behaviour. A 2-op wire-format patch with one
    // self-move applies 1 op (not 2).
    {
        auto root = make_initial();
        auto schema = parse_json(R"({"type": "object"})");
        Doc d{std::string{R"([
            {"op": "add",    "path": "/new", "value": "x"},
            {"op": "move",   "from": "/x/k", "path": "/x/k"}
        ])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);
        t.check(r.has_value(),
                "9d self-move is dropped; add still applies");
        t.check_eq_i(r ? *r : 0, std::size_t{1},
                     "9e applied count == 1 (self-move is dropped)");
    }

    // The schema layer catches violations the engine itself
    // can't see (the Aug 12 lesson's Section 8 + Aug 13 lesson's
    // Section 15 named this case). uniqueItems is the canonical
    // example: the engine adds the duplicate happily; the schema
    // layer rejects it post-apply.
    {
        auto root = parse_json(R"({"tags": ["a", "b"]})");
        auto schema = parse_json(R"({
            "type": "object",
            "properties": {"tags": {"type": "array", "uniqueItems": true}}
        })");
        Doc d{std::string{R"([
            {"op": "add", "path": "/tags/-", "value": "a"}
        ])"}};
        auto sp = d.view();
        auto r = psp::json_schema::parse_and_apply_atomic_streaming_validated(root, sp, schema);
        t.check(!r.has_value(),
                "9f schema layer catches uniqueItems violation (engine can't)");
        if (!r) {
            t.check(r.error().gate == ValidatedStreamingGate::PostValidate,
                    "9g gate == PostValidate (the engine's gate is silent)");
        }
    }

    // Final report
    std::printf("\n=================================================\n");
    std::printf("Passed: %d\n", t.passed);
    std::printf("Failed: %d\n", t.failed);
    std::printf("=================================================\n");
    return t.failed == 0 ? 0 : 1;
}
