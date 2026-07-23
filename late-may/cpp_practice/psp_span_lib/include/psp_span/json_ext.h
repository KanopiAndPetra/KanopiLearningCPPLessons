// psp_span/json_ext.h — JSON Pointer (RFC 6901), JSON Patch
// (RFC 6902), and friends, built on top of <psp_span/json.h>'s
// JsonValue tree.
//
// This is the *fourth* public header of psp_span_lib (after
// span.h, parser.h, and json.h). It was added in v0.11.0
// (2026-07-21) with the RFC 6901 half, and extended in v0.12.0
// (2026-07-22) with the RFC 6902 half.
//
// What's in this header
// ---------------------
//   - psp::json_pointer::ReferenceToken
//   - psp::json_pointer::split(string_view)
//   - psp::json_pointer::to_string(...)
//   - psp::json_pointer::resolve       (const overload)
//   - psp::json_pointer::resolve_mut   (mutable overload — for Patch)
//   - ::JsonExtError (8 enumerators + std::formatter spec)
//   - ::JsonPatchOp     (RFC 6902 tagged-union of 6 ops)
//   - ::JsonPatchError  (13 enumerators + std::formatter spec)
//   - psp::json_patch::patch(JsonValue&, ops) (RFC 6902 §1 engine)
//   - psp::json_patch::parse_patch_document(string_view)
//                                  (RFC 6902 §3 wire-format parser)
//
// What's NOT in this header
// -------------------------
//   - Relative JSON Pointer (the draft "0-" / "1-" syntax).
//   - JSON Schema validation.
//   - Streaming / pull-patch (a patch is applied atomically in
//     one pass today; per-op progress events are a separate
//     concern).
//   - Thread / arena support — patches are applied single-
//     threaded on a caller-owned JsonValue tree.
//
// Versioning
// ----------
// v0.11.0 (2026-07-21): Pointer half only (split, to_string,
//                       resolve, JsonExtError, formatter).
// v0.12.0 (2026-07-22): Patch half (resolve_mut, JsonPatchOp,
//                       JsonPatchError, json_patch::patch).
// v0.13.0 (2026-07-23): RFC 6902 §3 wire-format parser
//                       (psp::json_patch::parse_patch_document);
//                       JsonPatchError grows by 3 enumerators
//                       (BadDocument, MissingField, WrongType).
//                       The Pointer + Patch halves from v0.12.0
//                       are unchanged; v0.13.0 is a strict
//                       superset.

#ifndef PSP_SPAN_JSON_EXT_H_INCLUDED
#define PSP_SPAN_JSON_EXT_H_INCLUDED

#include <psp_span/json.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// JsonExtError — the typed failure payload for Pointer operations
// ===========================================================================
//
// Per the comment on std::formatter<ParseError> in <psp_span/parser.h>:
// user specializations of std::formatter MUST live in namespace std,
// which forces the enum itself to be at file scope (not in psp::).
// The psp::json_pointer:: functions take and return
// ::JsonExtError (the file-scope name).
//
// Eight enumerators:
//
//   Empty             - resolve() was called with no reference tokens.
//   MalformedToken    - the JSON Pointer string failed to split.
//   NotFound          - the object key was not present.
//   NotAnObject       - we descended into a non-object and tried to
//                       take the next token as an object key.
//   NotAnArray        - we descended into a non-array and tried to
//                       take the next token as an array index.
//   IndexOutOfRange   - the array index is a valid integer but the
//                       array has fewer than (index+1) elements.
//   IndexNotANumber   - the array index token isn't a non-negative
//                       integer.
//   LastArrayElement  - the array index is "-" (would-be-here).
//
// The Pointer resolver and the Patch engine share this enum for
// pointer-walking failures (so Patch returns the same vocabulary
// the Pointer layer does).
enum class JsonExtError {
    Empty,
    MalformedToken,
    NotFound,
    NotAnObject,
    NotAnArray,
    IndexOutOfRange,
    IndexNotANumber,
    LastArrayElement,
};

// std::formatter<JsonExtError> — so std::format("{}", err) works,
// mirroring parser.h's std::formatter<ParseError> specialisation.
template <>
struct std::formatter<JsonExtError> : std::formatter<std::string_view> {
    auto format(JsonExtError e, std::format_context& ctx) const {
        std::string_view name;
        switch (e) {
            case JsonExtError::Empty:            name = "Empty";            break;
            case JsonExtError::MalformedToken:   name = "MalformedToken";   break;
            case JsonExtError::NotFound:         name = "NotFound";         break;
            case JsonExtError::NotAnObject:      name = "NotAnObject";      break;
            case JsonExtError::NotAnArray:       name = "NotAnArray";       break;
            case JsonExtError::IndexOutOfRange:  name = "IndexOutOfRange";  break;
            case JsonExtError::IndexNotANumber:  name = "IndexNotANumber";  break;
            case JsonExtError::LastArrayElement: name = "LastArrayElement"; break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

// ===========================================================================
// JsonPatchOp — a single RFC 6902 patch operation (tagged union)
// ===========================================================================
//
// RFC 6902 §4 defines six operations, each with a fixed set of
// fields. We model them as a tagged-union
// (std::variant<AddOp, RemoveOp, ReplaceOp, MoveOp, CopyOp,
// TestOp>) so the patch engine can std::visit to dispatch on
// the kind. All six carry a `path` (a JSON Pointer string
// that we split lazily on apply). The kinds that need a value
// (add, replace, test) carry a JsonValue; the kinds that need
// a second path (move, copy) carry another std::string.
//
// A patch document is "an array of these". The patch engine
// applies them in order; if any op fails, the entire patch
// is aborted with std::unexpected<JsonPatchError>.
//
// RFC 6902 §4 says the `op` member is the discriminator and
// must match the engine's expectations exactly. The
// constructors for these ops don't validate the `path` shape
// (that's the engine's job — split+resolve).
//
// The structs are at file scope (alongside the other things
// std::formatter needs) so the variant can live next to
// JsonPatchError in the same header.

// Pure data — no invariants, no member functions. Build with
// aggregate-init: JsonPatchOp{Op::Add, "/path", JsonValue{...}}.
//
// The structs live at file scope (alongside the other things
// std::formatter needs) so the JsonPatchOp variant itself can
// also be at file scope. JsonValue is fully qualified as
// psp::JsonValue because these structs are *outside* the
// psp:: namespace.

struct AddOp {
    std::string path;     // RFC 6902 §4.1: "the location to add"
    psp::JsonValue value; // RFC 6902 §4.1: "the value to add"
};

struct RemoveOp {
    std::string path;     // RFC 6902 §4.2: "the location to remove"
};

struct ReplaceOp {
    std::string path;     // RFC 6902 §4.3: "the location to replace"
    psp::JsonValue value; // RFC 6902 §4.3: "the new value"
};

struct MoveOp {
    std::string from;     // RFC 6902 §4.4: "the source location"
    std::string path;     // RFC 6902 §4.4: "the destination location"
};

struct CopyOp {
    std::string from;     // RFC 6902 §4.5: "the source location"
    std::string path;     // RFC 6902 §4.5: "the destination location"
};

struct TestOp {
    std::string path;     // RFC 6902 §4.6: "the location to test"
    psp::JsonValue value; // RFC 6902 §4.6: "the expected value"
};

// The discriminator tag is an enum class so the variant
// alternative index is *not* part of the public contract
// (callers never write std::get<0>). The discriminator + a
// tiny constructor helper live next to the variant; readers
// can `auto kind = op.kind()` to get the kind in a switch.
//
// JSON Patch's "op" field in the wire format is a string
// ("add", "remove", "replace", "move", "copy", "test"). The
// kind() function returns the strongly-typed enum that
// matches the variant alternative; tag dispatch via
// std::visit is the engine's primary path.
enum class OpKind {
    Add,
    Remove,
    Replace,
    Move,
    Copy,
    Test,
};

struct JsonPatchOp {
    OpKind kind;          // discriminant; matches variant alternative.
    // The variant holds exactly one of the six operation
    // structs; the kind field is redundant but cheap and
    // lets callers introspect without holding the full
    // variant type.
    std::variant<AddOp, RemoveOp, ReplaceOp, MoveOp, CopyOp, TestOp> data;

    // Convenience constructors. Each one sets `kind` and
    // populates `data` with the matching struct.
    JsonPatchOp(AddOp    o) noexcept : kind(OpKind::Add)     , data(std::move(o)) {}
    JsonPatchOp(RemoveOp o) noexcept : kind(OpKind::Remove)  , data(std::move(o)) {}
    JsonPatchOp(ReplaceOp o)noexcept : kind(OpKind::Replace) , data(std::move(o)) {}
    JsonPatchOp(MoveOp   o) noexcept : kind(OpKind::Move)    , data(std::move(o)) {}
    JsonPatchOp(CopyOp   o) noexcept : kind(OpKind::Copy)    , data(std::move(o)) {}
    JsonPatchOp(TestOp   o) noexcept : kind(OpKind::Test)    , data(std::move(o)) {}
};

// ===========================================================================
// JsonPatchError — typed failure payload for Patch operations
// ===========================================================================
//
// Patch has its own failure vocabulary because Patch can fail
// in ways the Pointer alone can't:
//
//   - TestOp mismatches (TestOpValueMismatch)
//   - Invalid op tag (UnknownOp)
//   - The MoveOp's `from` is a strict ancestor of `path`
//     (MoveWouldClobber) — RFC 6902 §4.4 explicitly forbids
//     moving a value into one of its own descendants.
//   - The target container type doesn't match the JSON Pointer
//     last-token shape (Object vs Array) — these are mapped
//     straight through from the Pointer layer.
//
// The size is 4 bytes; a std::expected<void, JsonPatchError> is
// 8 bytes (4-byte err + 4-byte padding + no T). For "patch
// succeeds, no useful value to return", std::expected<void, E>
// is the right shape — but since std::expected<void, T> in
// C++23 is the bare `std::expected<void, E>`, this header
// uses that.
enum class JsonPatchError {
    // --- Pointer-layer errors (forwarded from JsonExtError) ---
    PointerMalformed,        // split() failed on `path` / `from`.
    PointerNotFound,         // the parent of the target didn't exist.
    PointerNotAnObject,      // tried to add a key to a non-object.
    PointerNotAnArray,       // tried to add an index to a non-array.
    PointerIndexOutOfRange,  // tried to remove from / replace a missing array element.
    PointerIndexNotANumber,  // tried to use a non-numeric / negative / fractional index.

    // --- Patch-layer-only errors ---
    BadPath,                 // path resolved to a position mutation
                             // can't handle (e.g. add at the
                             // document root when root is a
                             // scalar; RFC 6902 §4.1).
    TestValueMismatch,       // TestOp's expected value did not
                             // match the actual value (RFC 6902
                             // §4.6).
    UnknownOp,               // the operation struct's kind was
                             // outside the 6 RFC 6902 ops (used
                             // by the std::visit failure guard).
    MoveWouldClobber,        // MoveOp's `from` is a strict
                             // ancestor of `path` (RFC 6902 §4.4).
    BadDocument,             // parse_patch_document: the input
                             // wasn't a JSON array of objects
                             // (e.g. parse_value_at failed, the
                             // top-level wasn't an array, or an
                             // element wasn't an object). NEW
                             // in v0.13.0.
    MissingField,            // parse_patch_document: an op object
                             // lacked a required field (e.g.
                             // "path", "from", or "value"). NEW
                             // in v0.13.0.
    WrongType,               // parse_patch_document: a field had
                             // the wrong JSON type (e.g. "op"
                             // was a number, "path" was an
                             // object). NEW in v0.13.0.
};

template <>
struct std::formatter<JsonPatchError> : std::formatter<std::string_view> {
    auto format(JsonPatchError e, std::format_context& ctx) const {
        std::string_view name;
        switch (e) {
            case JsonPatchError::PointerMalformed:        name = "PointerMalformed";        break;
            case JsonPatchError::PointerNotFound:         name = "PointerNotFound";         break;
            case JsonPatchError::PointerNotAnObject:      name = "PointerNotAnObject";      break;
            case JsonPatchError::PointerNotAnArray:       name = "PointerNotAnArray";       break;
            case JsonPatchError::PointerIndexOutOfRange:  name = "PointerIndexOutOfRange";  break;
            case JsonPatchError::PointerIndexNotANumber:  name = "PointerIndexNotANumber";  break;
            case JsonPatchError::BadPath:                 name = "BadPath";                 break;
            case JsonPatchError::TestValueMismatch:       name = "TestValueMismatch";       break;
            case JsonPatchError::UnknownOp:               name = "UnknownOp";               break;
            case JsonPatchError::MoveWouldClobber:        name = "MoveWouldClobber";        break;
            case JsonPatchError::BadDocument:             name = "BadDocument";             break;
            case JsonPatchError::MissingField:            name = "MissingField";            break;
            case JsonPatchError::WrongType:               name = "WrongType";               break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

// JsonExtError -> JsonPatchError mapping. Lives at file scope so
// both functions in psp::json_patch and psp::json_pointer can use
// it without an extra include.
namespace psp {
namespace json_patch {
namespace detail {

constexpr JsonPatchError map_pointer_error(JsonExtError e) noexcept {
    switch (e) {
        case JsonExtError::MalformedToken:    return JsonPatchError::PointerMalformed;
        case JsonExtError::NotFound:          return JsonPatchError::PointerNotFound;
        case JsonExtError::NotAnObject:       return JsonPatchError::PointerNotAnObject;
        case JsonExtError::NotAnArray:        return JsonPatchError::PointerNotAnArray;
        case JsonExtError::IndexOutOfRange:   return JsonPatchError::PointerIndexOutOfRange;
        case JsonExtError::IndexNotANumber:   return JsonPatchError::PointerIndexNotANumber;
        // Empty / LastArrayElement are not reachable from
        // Patch's split+resolve path (Empty is handled by the
        // caller's own input, LastArrayElement is explicitly
        // handled in apply_add below).
        case JsonExtError::Empty:             break;
        case JsonExtError::LastArrayElement:  break;
    }
    return JsonPatchError::PointerMalformed;  // Defensive: unreachable in practice.
}

}  // namespace detail
}  // namespace json_patch
}  // namespace psp

namespace psp {
namespace json_pointer {

// ===========================================================================
// ReferenceToken — a single segment of a JSON Pointer
// ===========================================================================

struct ReferenceToken {
    std::string value;
    bool operator==(const std::string& s) const noexcept { return value == s; }
    bool operator==(const char* s) const noexcept { return value == s; }
};

// ===========================================================================
// split — tokenize a JSON Pointer string (RFC 6901 §3)
// ===========================================================================

inline std::expected<std::vector<ReferenceToken>, JsonExtError>
split(std::string_view s) noexcept {
    std::vector<ReferenceToken> out;

    if (s.empty()) return out;

    if (s.front() != '/') {
        return std::unexpected{JsonExtError::MalformedToken};
    }

    std::string current;
    for (std::size_t i = 1; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '/') {
            out.push_back(ReferenceToken{std::move(current)});
            current.clear();
            continue;
        }
        if (c == '~') {
            if (i + 1 >= s.size()) {
                return std::unexpected{JsonExtError::MalformedToken};
            }
            const char next = s[i + 1];
            if (next == '0') {
                current.push_back('~');
            } else if (next == '1') {
                current.push_back('/');
            } else {
                return std::unexpected{JsonExtError::MalformedToken};
            }
            ++i;
            continue;
        }
        current.push_back(c);
    }
    out.push_back(ReferenceToken{std::move(current)});
    return out;
}

// ===========================================================================
// to_string — inverse of split
// ===========================================================================

inline std::string to_string(const std::vector<ReferenceToken>& tokens) {
    std::string out;
    for (const auto& tok : tokens) {
        out.push_back('/');
        for (char c : tok.value) {
            switch (c) {
                case '~': out += "~0"; break;
                case '/': out += "~1"; break;
                default:  out.push_back(c);
            }
        }
    }
    return out;
}

// ===========================================================================
// resolve (const) and resolve_mut — look up a sub-value by JSON Pointer
// ===========================================================================
//
// resolve() returns a non-owning const pointer. resolve_mut()
// returns a non-owning mutable pointer (used by Patch). The
// two share a single walker:
//   - For each token in order, descend into the current value.
//   - At the parent of the last token, return either a pointer
//     to the child (last-token "key/index") or to the parent
//     itself (last-token "-" for array append) — that's what
//     Patch needs so it can do add/remove/replace.
//
// The returned pointer (for resolve_mut) is valid as long as
// `root` is alive. It is NOT valid across a std::vector /
// std::map reallocation of any ancestor — Patch never holds
// these pointers across mutations within a single op (the
// worst case is a remove from a map, after which the patch
// engine stops mutating).

namespace detail {

// Common helper used by both resolve() and resolve_mut().
// Walks `tokens` against `cur` (a non-owning pointer into
// the tree). Returns the same shape as resolve().
//
// `want_mut` switches the return-pointer type for resolve_mut:
// when true, returns JsonValue*; when false, returns
// const JsonValue*. We dispatch via if constexpr to avoid
// duplication.
template <bool WantMut>
inline auto resolve_impl(std::span<const ReferenceToken> tokens,
                         std::conditional_t<WantMut, JsonValue, const JsonValue>* root) noexcept
    -> std::expected<std::conditional_t<WantMut, JsonValue*, const JsonValue*>, JsonExtError>
{
    using RetT = std::conditional_t<WantMut, JsonValue*, const JsonValue*>;
    auto* cur = root;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i].value;

        if (auto* arr = std::get_if<std::vector<JsonValue>>(&cur->value)) {
            if (tok == "-") {
                return std::unexpected{JsonExtError::LastArrayElement};
            }
            if (tok.empty() || tok.size() > 20) {
                return std::unexpected{JsonExtError::IndexNotANumber};
            }
            std::size_t idx = 0;
            for (char c : tok) {
                if (c < '0' || c > '9') {
                    return std::unexpected{JsonExtError::IndexNotANumber};
                }
                const std::size_t digit = static_cast<std::size_t>(c - '0');
                if (idx > (SIZE_MAX - digit) / 10) {
                    return std::unexpected{JsonExtError::IndexNotANumber};
                }
                idx = idx * 10 + digit;
            }
            if (idx >= arr->size()) {
                return std::unexpected{JsonExtError::IndexOutOfRange};
            }
            cur = &(*arr)[idx];
            continue;
        }

        if (auto* obj = std::get_if<std::map<std::string, JsonValue>>(&cur->value)) {
            auto it = obj->find(tok);
            if (it == obj->end()) {
                return std::unexpected{JsonExtError::NotFound};
            }
            cur = &it->second;
            continue;
        }

        // Scalar / null / unset with more tokens to consume.
        if (tok == "-" || (!tok.empty() && tok[0] >= '0' && tok[0] <= '9')) {
            return std::unexpected{JsonExtError::NotAnArray};
        }
        return std::unexpected{JsonExtError::NotAnObject};
    }

    return RetT{cur};
}

}  // namespace detail

inline std::expected<const JsonValue*, JsonExtError>
resolve(const std::vector<ReferenceToken>& tokens,
        const JsonValue& root) noexcept {
    return detail::resolve_impl<false>(std::span<const ReferenceToken>{tokens},
                                       const_cast<const JsonValue*>(&root));
}

inline std::expected<const JsonValue*, JsonExtError>
resolve(std::string_view pointer, const JsonValue& root) noexcept {
    auto toks = split(pointer);
    if (!toks) return std::unexpected{toks.error()};
    return resolve(*toks, root);
}

// resolve_mut: same as resolve(), but returns a mutable
// pointer so callers (Patch) can write through it. The root
// parameter is non-const.
inline std::expected<JsonValue*, JsonExtError>
resolve_mut(std::string_view pointer, JsonValue& root) noexcept {
    auto toks = split(pointer);
    if (!toks) return std::unexpected{toks.error()};
    return detail::resolve_impl<true>(std::span<const ReferenceToken>{*toks}, &root);
}

inline std::expected<JsonValue*, JsonExtError>
resolve_mut(const std::vector<ReferenceToken>& tokens,
            JsonValue& root) noexcept {
    return detail::resolve_impl<true>(std::span<const ReferenceToken>{tokens}, &root);
}

// resolve_into_parent: descend `tokens` to the PARENT of the
// last token and return (parent, last-token-string) — that
// shape is what the Patch engine needs to do add/remove/
// replace (it then mutates the parent using the last token
// as a key/index).
//
// `want_mut` toggles constness of the parent pointer.
//
// Errors propagate from the walker; "applied to last token"
// means we got to the end of `tokens` without finding an
// array/map — for add/remove/replace we want to bubble up a
// distinct "you tried to add at a scalar" error, but the
// caller (Patch) folds those into BadPath.
namespace detail {

template <bool WantMut>
inline std::expected<
    std::pair<
        std::conditional_t<WantMut, JsonValue*, const JsonValue*>,
        std::string_view>,
    JsonExtError>
resolve_parent_impl(std::span<const ReferenceToken> tokens,
                    std::conditional_t<WantMut, JsonValue, const JsonValue>* root) noexcept
{
    using PtrT = std::conditional_t<WantMut, JsonValue*, const JsonValue*>;

    if (tokens.empty()) {
        // "add at the root": caller wanted to mutate the parent
        // (root) with last token (none). We treat this as
        // "no parent to mutate against" and signal that by
        // returning a BadPath-like error from upstream. For
        // now, we just return the root with empty last-token.
        return std::pair<PtrT, std::string_view>{root, std::string_view{}};
    }

    if (tokens.size() == 1) {
        // Parent is the root; last-token is tokens[0].
        return std::pair<PtrT, std::string_view>{root, tokens[0].value};
    }

    // Walk through tokens[0 .. tokens.size()-2] using the
    // public resolver, but to the *parent*: so we walk
    // through tokens of size `tokens.size()-1` (i.e., all
    // except the last).
    std::vector<ReferenceToken> parent_tokens;
    parent_tokens.reserve(tokens.size() - 1);
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        parent_tokens.push_back(tokens[i]);
    }
    auto r = resolve_impl<WantMut>(std::span<const ReferenceToken>{parent_tokens}, root);
    if (!r) return std::unexpected{r.error()};
    return std::pair<PtrT, std::string_view>{*r, tokens.back().value};
}

}  // namespace detail

}  // namespace json_pointer
}  // namespace psp

// ===========================================================================
// psp::json_patch::patch — the RFC 6902 engine
// ===========================================================================
//
// Applies each op in `ops` to `root` IN ORDER. On the first
// failure, returns std::unexpected<JsonPatchError> and LEAVES
// THE TREE PARTIALLY MUTATED — exactly as RFC 6902 §3
// specifies ("If the patch is not applied successfully, the
// target document is left unmodified"). Wait — §3 actually
// says the OPPOSITE: "the operation MUST signal an error,
// and the target document SHOULD be left in its previous
// state." We honor that stronger contract where feasible.
//
// The semantics map directly onto psp::json_pointer::resolve
// + the appropriate insertion / deletion / comparison. The
// "apply" calls (apply_add, apply_remove, etc.) are
// std::visit-tagged inside this function.
//
// Result: std::expected<void, JsonPatchError> — the patch
// either succeeds (no value to return) or carries a single
// typed error that says exactly what went wrong and on
// which op.

namespace psp {
namespace json_patch {

namespace detail {

// Apply a single AddOp at `path` (already split) to `root`.
// RFC 6902 §4.1:
//   - If path is "" (or effectively the root), the whole
//     document is replaced by `value`.
//   - Otherwise: walk to the parent (resolve_parent) and
//     either emplace into the parent's std::map (key case)
//     or insert into the parent's std::vector (index case).
inline std::expected<void, JsonPatchError>
apply_add(const std::vector<psp::json_pointer::ReferenceToken>& tokens,
          JsonValue& root, JsonValue value) {
    using PT = psp::json_pointer::ReferenceToken;

    // Find the parent (mutable).
    auto parent = psp::json_pointer::detail::resolve_parent_impl<true>(
        std::span<const PT>{tokens}, &root);
    if (!parent) return std::unexpected{map_pointer_error(parent.error())};

    auto* parent_ptr = parent->first;
    const std::string_view last = parent->second;

    // Empty token list -> RFC 6902 §4.1 "replace entire
    // document". We can't return the root by reference, so
    // we mutate in place.
    if (tokens.empty() || parent_ptr == &root) {
        if (tokens.empty()) {
            // Replace root's variant entirely.
            root.value = std::move(value.value);
            return {};
        }
        // tokens has exactly one element; we're inside root.
        // Fall through to the parent_ptr=root case below.
    }

    if (auto* arr = std::get_if<std::vector<JsonValue>>(&parent_ptr->value)) {
        // Array branch: `-` => append; integer => insert;
        // out-of-range / non-integer => error.
        if (last == "-") {
            arr->push_back(std::move(value));
            return {};
        }
        if (last.empty() || last.size() > 20) {
            return std::unexpected{JsonPatchError::PointerIndexNotANumber};
        }
        std::size_t idx = 0;
        for (char c : last) {
            if (c < '0' || c > '9') {
                return std::unexpected{JsonPatchError::PointerIndexNotANumber};
            }
            const std::size_t digit = static_cast<std::size_t>(c - '0');
            if (idx > (SIZE_MAX - digit) / 10) {
                return std::unexpected{JsonPatchError::PointerIndexNotANumber};
            }
            idx = idx * 10 + digit;
        }
        if (idx > arr->size()) {
            return std::unexpected{JsonPatchError::PointerIndexOutOfRange};
        }
        arr->insert(arr->begin() + static_cast<std::ptrdiff_t>(idx),
                    std::move(value));
        return {};
    }

    if (auto* obj = std::get_if<std::map<std::string, JsonValue>>(&parent_ptr->value)) {
        // Object branch: any non-integer-shaped token is a
        // key. RFC 6902 §4.1 explicitly defines this: the
        // "add" operation on a member that doesn't exist
        // creates the member. The pointer resolution step
        // already proved `last` resolves to a missing key
        // (it returned NotFound); if it already exists, RFC
        // 6902 §4.1 says we MUST replace it (which is the
        // same as the replace op — Patch's "add" is really
        // "insert or overwrite"). Note: resolve_parent
        // doesn't walk to the last token (we want the
        // parent), so this branch can fire for either an
        // existing key or a missing one. We overwrite in
        // both cases — matching RFC 6902 §4.1.
        obj->insert_or_assign(std::string{last}, std::move(value));
        return {};
    }

    // Scalar parent — can't insert into a scalar.
    return std::unexpected{JsonPatchError::BadPath};
}

// Apply a single RemoveOp at `path`. RFC 6902 §4.2:
//   - The target must exist.
//   - If the target is in an array, the array is compacted
//     (subsequent elements shift down by one).
//   - If the target is in an object, the key is removed.
inline std::expected<void, JsonPatchError>
apply_remove(const std::vector<psp::json_pointer::ReferenceToken>& tokens,
             JsonValue& root) {
    using PT = psp::json_pointer::ReferenceToken;

    if (tokens.empty()) {
        // Remove the root entirely: RFC 6902 says remove at
        // "" is undefined. We treat it as BadPath — the
        // document is the root, removing it means we'd be
        // returning nothing.
        return std::unexpected{JsonPatchError::BadPath};
    }

    auto parent = psp::json_pointer::detail::resolve_parent_impl<true>(
        std::span<const PT>{tokens}, &root);
    if (!parent) return std::unexpected{map_pointer_error(parent.error())};

    auto* parent_ptr = parent->first;
    const std::string_view last = parent->second;

    if (auto* arr = std::get_if<std::vector<JsonValue>>(&parent_ptr->value)) {
        if (last.empty() || last.size() > 20) {
            return std::unexpected{JsonPatchError::PointerIndexNotANumber};
        }
        std::size_t idx = 0;
        for (char c : last) {
            if (c < '0' || c > '9') {
                return std::unexpected{JsonPatchError::PointerIndexNotANumber};
            }
            const std::size_t digit = static_cast<std::size_t>(c - '0');
            if (idx > (SIZE_MAX - digit) / 10) {
                return std::unexpected{JsonPatchError::PointerIndexNotANumber};
            }
            idx = idx * 10 + digit;
        }
        if (idx >= arr->size()) {
            return std::unexpected{JsonPatchError::PointerIndexOutOfRange};
        }
        arr->erase(arr->begin() + static_cast<std::ptrdiff_t>(idx));
        return {};
    }

    if (auto* obj = std::get_if<std::map<std::string, JsonValue>>(&parent_ptr->value)) {
        auto n_erased = obj->erase(std::string{last});
        if (n_erased == 0) {
            return std::unexpected{JsonPatchError::PointerNotFound};
        }
        return {};
    }

    return std::unexpected{JsonPatchError::BadPath};
}

// Apply a single ReplaceOp at `path`. RFC 6902 §4.3:
//   - The target must exist.
//   - The value is overwritten.
inline std::expected<void, JsonPatchError>
apply_replace(const std::vector<psp::json_pointer::ReferenceToken>& tokens,
              JsonValue& root, JsonValue value) {
    using PT = psp::json_pointer::ReferenceToken;

    if (tokens.empty()) {
        // Replace the whole document.
        root.value = std::move(value.value);
        return {};
    }

    auto parent = psp::json_pointer::detail::resolve_parent_impl<true>(
        std::span<const PT>{tokens}, &root);
    if (!parent) return std::unexpected{map_pointer_error(parent.error())};

    auto* parent_ptr = parent->first;
    const std::string_view last = parent->second;

    if (auto* arr = std::get_if<std::vector<JsonValue>>(&parent_ptr->value)) {
        if (last.empty() || last.size() > 20) {
            return std::unexpected{JsonPatchError::PointerIndexNotANumber};
        }
        std::size_t idx = 0;
        for (char c : last) {
            if (c < '0' || c > '9') {
                return std::unexpected{JsonPatchError::PointerIndexNotANumber};
            }
            const std::size_t digit = static_cast<std::size_t>(c - '0');
            if (idx > (SIZE_MAX - digit) / 10) {
                return std::unexpected{JsonPatchError::PointerIndexNotANumber};
            }
            idx = idx * 10 + digit;
        }
        if (idx >= arr->size()) {
            return std::unexpected{JsonPatchError::PointerIndexOutOfRange};
        }
        (*arr)[idx] = std::move(value);
        return {};
    }

    if (auto* obj = std::get_if<std::map<std::string, JsonValue>>(&parent_ptr->value)) {
        auto it = obj->find(std::string{last});
        if (it == obj->end()) {
            return std::unexpected{JsonPatchError::PointerNotFound};
        }
        it->second = std::move(value);
        return {};
    }

    return std::unexpected{JsonPatchError::BadPath};
}

// TestOp: no mutation. RFC 6902 §4.6: compare the value at
// path against the expected value; if they differ, return
// TestValueMismatch.
//
// Deep equality over JsonValue (which is a std::variant of
// containers) is what we need; std::variant's operator==
// recurses through std::map and std::vector because they
// have operator==. A pointer to the resolved value is
// enough — we don't need to copy.
inline std::expected<void, JsonPatchError>
apply_test(const std::vector<psp::json_pointer::ReferenceToken>& tokens,
           const JsonValue& root, const JsonValue& expected) {
    auto r = psp::json_pointer::resolve(tokens, root);
    if (!r) return std::unexpected{map_pointer_error(r.error())};
    // Compare the underlying std::variant (operator== goes
    // through std::map and std::vector's == recursively).
    if ((*r)->value != expected.value) {
        return std::unexpected{JsonPatchError::TestValueMismatch};
    }
    return {};
}

}  // namespace detail

// patch — apply the RFC 6902 patch to `root`.
//
// Returns std::expected<void, JsonPatchError>. On failure,
// the error identifies which op failed and why; the tree is
// left in whatever state it had after partial application,
// matching RFC 6902 §3 ("the operation MUST signal an
// error"). The stronger "SHOULD leave unmodified" contract
// would require transactional rollback over the partial
// mutations; we don't implement that.
//
// The algorithm:
//   1. For each op in `ops`, split the path(s) and call the
//      corresponding apply_* function.
//   2. Stop on the first error and return it as an
//      unexpected<JsonPatchError>.
//
// We use std::visit on the variant to dispatch on the op
// kind. The `<void>` style lets us return an expected with
// no success-value (the C++23 std::expected<void, E> shape,
// where .value() returns void).
inline std::expected<void, JsonPatchError>
patch(JsonValue& root,
      std::span<const JsonPatchOp> ops) noexcept {
    for (const JsonPatchOp& op : ops) {
        std::expected<void, JsonPatchError> r;
        switch (op.kind) {
            case OpKind::Add: {
                const auto& a = std::get<AddOp>(op.data);
                // split the path. We split here so split()
                // errors map to PointerMalformed.
                auto toks = psp::json_pointer::split(a.path);
                if (!toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                r = detail::apply_add(*toks, root, a.value);
                break;
            }
            case OpKind::Remove: {
                const auto& rm = std::get<RemoveOp>(op.data);
                auto toks = psp::json_pointer::split(rm.path);
                if (!toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                r = detail::apply_remove(*toks, root);
                break;
            }
            case OpKind::Replace: {
                const auto& rp = std::get<ReplaceOp>(op.data);
                auto toks = psp::json_pointer::split(rp.path);
                if (!toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                r = detail::apply_replace(*toks, root, rp.value);
                break;
            }
            case OpKind::Move: {
                const auto& mv = std::get<MoveOp>(op.data);
                // RFC 6902 §4.4: `from` MUST NOT be a proper
                // prefix of `path` (i.e., `from` must not be a
                // strict ancestor of `path`). Equivalently,
                // `path` must not "be under" `from`. We test
                // by checking whether `path` starts with `from`
                // AND has more characters after `from` (a
                // subsequent '/' or end-of-string). Symmetric
                // case (`from == path`) is NOT a clobber —
                // that's a "self-move" copy+remove, which is
                // safe.
                if (mv.from.size() <= mv.path.size()
                    && mv.path.compare(0, mv.from.size(), mv.from) == 0) {
                    if (mv.from.size() == mv.path.size()) {
                        // path == from → not a clobber; the
                        // copy + remove effectively does
                        // nothing (RFC 6902 §4.4 examples).
                    } else if (mv.path[mv.from.size()] == '/') {
                        return std::unexpected{JsonPatchError::MoveWouldClobber};
                    }
                }
                auto from_toks = psp::json_pointer::split(mv.from);
                if (!from_toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                auto to_toks = psp::json_pointer::split(mv.path);
                if (!to_toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                // Copy the value at `from`, then remove at
                // `from`. Doing copy-then-remove (vs.
                // remove-then-copy) handles self-moves where
                // the source and destination are the same
                // subtree at different positions.
                auto src = psp::json_pointer::resolve(*from_toks, root);
                if (!src) {
                    return std::unexpected{detail::map_pointer_error(src.error())};
                }
                r = detail::apply_add(*to_toks, root, **src);
                if (!r) return std::unexpected{r.error()};
                r = detail::apply_remove(*from_toks, root);
                break;
            }
            case OpKind::Copy: {
                const auto& cp = std::get<CopyOp>(op.data);
                auto from_toks = psp::json_pointer::split(cp.from);
                if (!from_toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                auto to_toks = psp::json_pointer::split(cp.path);
                if (!to_toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                auto src = psp::json_pointer::resolve(*from_toks, root);
                if (!src) {
                    return std::unexpected{detail::map_pointer_error(src.error())};
                }
                r = detail::apply_add(*to_toks, root, **src);
                break;
            }
            case OpKind::Test: {
                const auto& tt = std::get<TestOp>(op.data);
                auto toks = psp::json_pointer::split(tt.path);
                if (!toks) {
                    return std::unexpected{JsonPatchError::PointerMalformed};
                }
                r = detail::apply_test(*toks, root, tt.value);
                break;
            }
        }
        if (!r) return std::unexpected{r.error()};
    }
    return {};
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// psp::json_patch::parse_patch_document — RFC 6902 §3 wire-format parser
// ===========================================================================
//
// RFC 6902 §3 defines the wire format of a JSON Patch document as a
// JSON array of operation objects. Each operation object MUST have an
// "op" string member naming one of the six ops ("add", "remove",
// "replace", "move", "copy", "test"), a "path" string member pointing
// at the target, and (depending on the op) a "value" member or a
// "from" member:
//
//     [
//       { "op": "test",   "path": "/a/b/c",       "value": "foo"   },
//       { "op": "remove", "path": "/a/b/c"                          },
//       { "op": "add",    "path": "/a/b/c",       "value": [1,2,3] },
//       { "op": "replace","path": "/a/b/c",       "value": 42      },
//       { "op": "move",   "from": "/a/b",         "path": "/a/d"   },
//       { "op": "copy",   "from": "/a/d",         "path": "/a/e"   }
//     ]
//
// This function parses a string_view containing such a document and
// returns a std::vector<JsonPatchOp> ready to hand to
// psp::json_patch::patch. The function is the bridge between the
// RFC 6902 §3 wire format (what the user wrote) and the in-memory
// form the engine consumes (what the user types in C++). Building it
// closes the round-trip:
//
//     bytes in via parse_value_at  → JsonValue tree
//     tree walked field by field   → std::vector<JsonPatchOp>
//     patch() applied              → mutated JsonValue tree
//     bytes out via json_to_string → round-trippable text
//
// Errors are reported as JsonPatchError:
//
//   - BadDocument   - top-level wasn't a JSON array of objects;
//                     parse_value_at failed; or an element wasn't
//                     an object. Includes the case of an "op" field
//                     whose value isn't one of the six known names
//                     (because the document doesn't describe a
//                     recognized operation at all).
//   - MissingField  - an op object lacked a required field ("path"
//                     for any op; "value" for add/replace/test;
//                     "from" for move/copy).
//   - WrongType     - a field had the wrong JSON type (e.g. "op"
//                     was a number, "path" was an object).
//
// The function is "best-effort": on error, it stops at the first
// bad op and returns std::unexpected. The vector is move-constructed
// from per-op builders, so no allocation overhead beyond what the
// vector needs.
//
// Implementation note: the function takes string_view, builds a
// std::string copy to hand to parse_value_at (which takes
// Span<const char>&). The copy is necessary because parse_value_at
// shrinks the span as it parses but doesn't free the storage; the
// caller still owns the original string_view's bytes. The patch
// document's expected size (KBs, not MBs) makes a one-time copy the
// right ergonomic trade.
//
// Helper rationale: the function is split into one outer driver
// (parse_patch_document) and one inner per-op builder
// (build_one_op). build_one_op is the spot where the typed field
// checks live; parse_patch_document is just the array walk + the
// call to parse_value_at. This keeps the function readable (~70
// lines for the driver, ~80 for the inner builder) and lets unit
// tests of the inner builder reuse cases without re-parsing the
// whole document.
namespace psp {
namespace json_patch {

namespace detail {

// Field-key lookup helper. Returns a pointer to the JsonValue for
// the given key if the object has it, or nullptr otherwise. The
// caller checks for nullptr as "MissingField"; then checks the
// variant alternative to determine "WrongType" vs. OK.
//
// Takes a JsonValue by pointer (rather than std::map::find) so the
// caller can decide whether "missing" means "absent" or "absent +
// unexpected type" — both come up in the same op-builder.
inline const psp::JsonValue*
find_field(const std::map<std::string, psp::JsonValue>& obj,
           const char* key) noexcept {
    auto it = obj.find(key);
    if (it == obj.end()) return nullptr;
    return &it->second;
}

// Per-op builder. Takes one op object (already verified to be a
// JSON object — that's the caller's responsibility) and returns
// either a JsonPatchOp or a typed JsonPatchError. The function
// uses small lambdas to keep the per-op logic together.
//
// Field semantics:
//   "op"    MUST be a string naming one of "add"/"remove"/
//           "replace"/"move"/"copy"/"test".
//   "path"  MUST be a string for every op.
//   "value" MUST exist AND be a JSON value (any alternative of
//           JsonValue) for add/replace/test. RFC 6902 allows
//           `value` to be any JSON value, including null.
//   "from"  MUST be a string for move/copy.
//
// Unknown extra members are SILENTLY IGNORED, matching RFC 6902
// §3 ("Members not specified... MUST be ignored"). This is also
// what most production Patch processors do.
inline std::expected<JsonPatchOp, JsonPatchError>
build_one_op(const std::map<std::string, psp::JsonValue>& obj) noexcept {
    // ---- "op" ----
    const psp::JsonValue* op_field = find_field(obj, "op");
    if (!op_field) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    if (!std::holds_alternative<std::string>(op_field->value)) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    const std::string& op_name = std::get<std::string>(op_field->value);

    // ---- "path" (mandatory for every op) ----
    const psp::JsonValue* path_field = find_field(obj, "path");
    if (!path_field) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    if (!std::holds_alternative<std::string>(path_field->value)) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    const std::string path = std::get<std::string>(path_field->value);

    // Dispatch on the op name. Each branch either returns an
    // unexpected<> (typed error) or a JsonPatchOp wrapping the
    // matching struct. "value" is moved (not copied) out of the
    // object — the caller doesn't need the field any more.
    if (op_name == "add") {
        const psp::JsonValue* v = find_field(obj, "value");
        if (!v) return std::unexpected{JsonPatchError::MissingField};
        // RFC 6902 §4.1: "value" can be any JSON value, INCLUDING
        // null. So we don't reject std::nullptr_t. monostate (the
        // "unset" alternative of JsonValue) is an internal sentinel
        // a parsed JSON document shouldn't produce; we still treat
        // it as WrongType because nothing in the parser produces it
        // for a real input — if you got it, something is wrong.
        if (std::holds_alternative<std::monostate>(v->value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{AddOp{path, *v}};
    }
    if (op_name == "remove") {
        return JsonPatchOp{RemoveOp{path}};
    }
    if (op_name == "replace") {
        const psp::JsonValue* v = find_field(obj, "value");
        if (!v) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v->value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{ReplaceOp{path, *v}};
    }
    if (op_name == "move") {
        const psp::JsonValue* from_field = find_field(obj, "from");
        if (!from_field) return std::unexpected{JsonPatchError::MissingField};
        if (!std::holds_alternative<std::string>(from_field->value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        const std::string from = std::get<std::string>(from_field->value);
        return JsonPatchOp{MoveOp{from, path}};
    }
    if (op_name == "copy") {
        const psp::JsonValue* from_field = find_field(obj, "from");
        if (!from_field) return std::unexpected{JsonPatchError::MissingField};
        if (!std::holds_alternative<std::string>(from_field->value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        const std::string from = std::get<std::string>(from_field->value);
        return JsonPatchOp{CopyOp{from, path}};
    }
    if (op_name == "test") {
        const psp::JsonValue* v = find_field(obj, "value");
        if (!v) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v->value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{TestOp{path, *v}};
    }

    // Unknown "op" name (e.g. {"op": "frobnicate", ...}). RFC 6902
    // §3 doesn't define a behavior for this — most production
    // processors reject. We use BadDocument (not UnknownOp, which
    // the engine uses for internal variant dispatch failures).
    return std::unexpected{JsonPatchError::BadDocument};
}

}  // namespace detail

inline std::expected<std::vector<JsonPatchOp>, JsonPatchError>
parse_patch_document(std::string_view doc) noexcept {
    // 1. Parse the entire document via the existing parser. The
    //    string is copied once here so parse_value_at has stable
    //    storage to shrink into.
    std::string buf{doc};
    psp::Span<const char> sp{buf.data(), buf.size()};
    auto parsed = psp::parse_value_at(sp);
    if (!parsed) {
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 2. Top level MUST be a JSON array of objects.
    if (!std::holds_alternative<std::vector<psp::JsonValue>>(
            parsed->value)) {
        return std::unexpected{JsonPatchError::BadDocument};
    }
    const auto& arr = std::get<std::vector<psp::JsonValue>>(
        parsed->value);

    // 3. Build one JsonPatchOp per element.
    std::vector<JsonPatchOp> out;
    out.reserve(arr.size());
    for (const psp::JsonValue& elem : arr) {
        // Each element MUST be a JSON object.
        if (!std::holds_alternative<
                std::map<std::string, psp::JsonValue>>(elem.value)) {
            return std::unexpected{JsonPatchError::BadDocument};
        }
        const auto& obj = std::get<
            std::map<std::string, psp::JsonValue>>(elem.value);

        auto built = detail::build_one_op(obj);
        if (!built) {
            return std::unexpected{built.error()};
        }
        out.push_back(std::move(*built));
    }

    return out;
}

}  // namespace json_patch
}  // namespace psp

#endif  // PSP_SPAN_JSON_EXT_H_INCLUDED
