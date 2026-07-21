// psp_span/json_ext.h — JSON Pointer (RFC 6901) and friends,
// built on top of <psp_span/json.h>'s JsonValue tree.
//
// This is the *fourth* public header of psp_span_lib (after
// span.h, parser.h, and json.h). It was added in v0.11.0
// (2026-07-21) because:
//
//   - The library now owns a complete general JSON parser
//     (<psp_span/json.h>, v0.10.0).
//   - The next natural capability over a parsed JSON tree is
//     *addressing* a sub-value without writing per-shape
//     accessors — JSON Pointer (RFC 6901) is the standardised
//     way to do that, and the natural foundation for JSON
//     Patch (RFC 6902), which is the lesson after this one.
//
// What's in this header
// ---------------------
//   - psp::json_pointer::ReferenceToken        (a single token in a
//                                                JSON Pointer string,
//                                                with ~1 / ~0 already
//                                                unescaped).
//   - psp::json_pointer::split(string_view)    (split a JSON Pointer
//                                                string into its
//                                                reference tokens).
//   - psp::json_pointer::to_string(...)        (the inverse —
//                                                reconstruct a JSON
//                                                Pointer string from
//                                                a sequence of tokens,
//                                                applying ~0/~1 escapes).
//   - psp::json_pointer::resolve(view, json)   (look up a value at a
//                                                given JSON Pointer;
//                                                returns the *index*
//                                                of the value plus a
//                                                pointer-like handle
//                                                so the caller can
//                                                inspect the matched
//                                                sub-tree).
//   - psp::JsonExtError (an error enum: Empty, MalformedToken,
//                         NotFound, NotAnObject, NotAnArray,
//                         IndexOutOfRange, IndexNotANumber,
//                         LastArrayElement).
//
// What's NOT in this header
// -------------------------
//   - JSON Patch (RFC 6902). That is a follow-on lesson, not part
//     of this one. The JsonValue tree is already mutable (it's not
//     const), so Patch is a natural next step.
//   - Relative JSON Pointer (the draft "-i" / "0-" syntax). The
//     spec is still a draft and it has its own edge cases; left
//     for later.
//   - Any allocation from a shared arena. The resolver returns a
//     non-owning handle, so no allocation happens at all.
//
// Why a separate header (json_ext.h vs json.h)
// --------------------------------------------
// The json.h header is a *parser + pretty-printer* — it maps a
// span of bytes to a JsonValue tree. json_ext.h is a *query
// language* over that tree. Different concerns, different header,
// different version bump: json_ext.h is a v0.11.0 capability, and
// nothing in json.h changed.
//
// Error model
// -----------
// JSON Pointer (RFC 6901 §4) defines the resolution failure modes:
//
//   - The pointer is malformed (e.g. doesn't start with "/", or
//     has an unterminated ~-escape). That's psp::JsonExtError::MalformedToken.
//   - The pointer is well-formed but the value at that location
//     doesn't exist: NotFound, NotAnObject, NotAnArray,
//     IndexOutOfRange, IndexNotANumber, LastArrayElement.
//   - The pointer is empty: psp::JsonExtError::Empty (RFC 6901
//     says the empty string resolves to the whole document, so
//     this should only fire if you pass a deliberately empty
//     token list to resolve() directly).
//
// resolve() returns std::expected<const JsonValue*, JsonExtError>
// — a non-owning pointer into the input tree on success, a typed
// error on failure. That mirrors the v0.10.0 cursor-parser style
// (std::expected<T, ParseError>), but the failure payload is its
// own type (JsonExtError) so callers can std::format it without
// pulling in ParseError.
//
// Reference token syntax
// ----------------------
// JSON Pointer splits its argument on "/" — every token between
// two "/"s is a "reference token". Two-character escapes:
//
//   "~0"  ->  "~"
//   "~1"  ->  "/"
//
// In that order: the RFC says "first transform any occurrence
// of '~1' to '/', then transform any occurrence of '~0' to '~'".
// Because the input is scanned as a whole string in each step,
// `~01` becomes `~1` (step 1 finds no `~1`, so the string is
// unchanged; step 2 finds the `~0` at position 0 and turns it
// into `~`, leaving the trailing `1`). `~10` becomes `/0`
// (step 1 turns `~1` into `/`; step 2 finds no `~0`).
//
// The character-by-character scan in split() implements the
// same algorithm because:
//   - when we see `~0`, the next char is consumed, so the
//     trailing `1` of `~01` is left as a literal `1`;
//   - when we see `~1`, the next char is consumed, so the
//     trailing `0` of `~10` is left as a literal `0`.
// RFC 6901 §3 spells this out; we implement it in the
// documented order.
//
// The "-" token
// -------------
// RFC 6901 §4 says "-" inside an array refers to "the (nonexistent)
// member after the last array element". This is meaningful for
// JSON Patch (you can "add" a new last element with "-"), but for
// pure resolve() it's a LastArrayElement error because there is no
// such element to return. Patch will handle the same token
// specially.

#ifndef PSP_SPAN_JSON_EXT_H_INCLUDED
#define PSP_SPAN_JSON_EXT_H_INCLUDED

#include <psp_span/json.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <vector>

// ===========================================================================
// JsonExtError — the typed failure payload for json_ext.h operations
// ===========================================================================
//
// Per the comment on std::formatter<ParseError> in <psp_span/parser.h>:
// user specializations of std::formatter MUST live in namespace std,
// which forces the enum itself to be at file scope (not in psp::).
// The psp::json_pointer:: functions take and return
// ::JsonExtError (the file-scope name).
//
// Seven enumerators, in the rough order in which RFC 6901 §4 spells them
// out (plus a leading MalformedToken for tokenizer failures).
//
//   Empty             - resolve() was called with no reference tokens.
//                       Per RFC 6901, the JSON Pointer "" (empty string)
//                       resolves to the whole document, so the token list
//                       is also allowed to be empty and the resolver
//                       returns the document root. The Empty error
//                       therefore only fires when resolve() is called
//                       directly with an empty vector of tokens (which
//                       split() never produces, but the function accepts
//                       it explicitly).
//
//   MalformedToken    - the JSON Pointer string failed to split (e.g.
//                       contains an unterminated ~-escape such as "foo~").
//
//   NotFound          - the object key was not present in the object
//                       (std::map::find failed). This is the most common
//                       JSON Pointer failure in practice.
//
//   NotAnObject       - we descended into a non-object (an array,
//                       scalar, or null) and tried to take the next
//                       token as an object key. RFC 6901 §4: "If the
//                       currently referenced value is not an object, the
//                       error shall be returned."
//
//   NotAnArray        - we descended into a non-array and tried to
//                       take the next token as an array index.
//
//   IndexOutOfRange   - the array index is a valid integer, but the
//                       array has fewer than (index+1) elements.
//
//   IndexNotANumber   - the array index token isn't a non-negative
//                       integer. Negative or fractional indices are
//                       rejected (RFC 6901 §4: array indices are
//                       "non-negative decimal integers" or "-").
//
//   LastArrayElement  - the array index is "-" (the "would-be-here"
//                       token from RFC 6901). resolve() has no such
//                       element to return; only a Patch add can.
//
// All seven fit in 4 bytes (sizeof = 4); std::expected<const JsonValue*,
// JsonExtError> is therefore 16 bytes (8-byte ptr + 4-byte err + 4-byte
// padding on most ABIs).
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
// (User specializations of std::formatter MUST be in namespace std.)
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

namespace psp {
namespace json_pointer {

// ===========================================================================
// ReferenceToken — a single segment of a JSON Pointer
// ===========================================================================
//
// A reference token is a UTF-8 string with "~0" and "~1" already
// unescaped. Storing the unescaped form means the resolver's hot
// path is just a key lookup or an index conversion; the unescape
// happens once in split().
//
// Stored as std::string rather than std::string_view because
// split() builds the tokens from a fresh string and there's no
// caller-supplied backing store to keep alive.
struct ReferenceToken {
    std::string value;
    bool operator==(const std::string& s) const noexcept { return value == s; }
    bool operator==(const char* s) const noexcept { return value == s; }
};

// ===========================================================================
// split — tokenize a JSON Pointer string (RFC 6901 §3)
// ===========================================================================
//
// Splits `s` on "/" and unescapes each token (RFC 6901 §3,
// "Evaluation"): "~1" -> "/", "~0" -> "~", applied in that
// order. Per RFC 6901 the empty string means "the whole
// document", so split("") returns an empty vector — the
// resolver's "no tokens" branch returns the document root
// directly.
//
// Errors:
//   - MalformedToken: `s` contains an unterminated "~" sequence
//     (e.g. "foo~" — the "~" is at end-of-string with no
//     following 0 or 1). Detected as: we see a "~" but the
//     next char doesn't exist or isn't '0' or '1'.
//
// Per RFC 6901 §3, "~" alone (no following 0/1) is the only
// MalformedToken case. Everything else (including "~2" or
// "~ab") falls into the same bucket because the only
// well-formed escapes are "~0" and "~1".
inline std::expected<std::vector<ReferenceToken>, JsonExtError>
split(std::string_view s) noexcept {
    std::vector<ReferenceToken> out;

    // RFC 6901: "" (empty string) means "the whole document" — no
    // tokens, not even a "root" token. split() returns an empty
    // vector; resolve() will return the input root directly.
    if (s.empty()) return out;

    // RFC 6901: every JSON Pointer starts with "/". The first "/"
    // is a delimiter, not a token. (Note: split() is also called
    // for "" and "" would return no tokens; here we require the
    // leading "/" for non-empty input.)
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
            ++i;  // consume the digit
            continue;
        }
        current.push_back(c);
    }
    out.push_back(ReferenceToken{std::move(current)});
    return out;
}

// ===========================================================================
// to_string — the inverse of split
// ===========================================================================
//
// Joins `tokens` into a JSON Pointer string. Empty `tokens`
// produces "" (the whole document). The leading "/" of the
// first token is implicit — we never write "/" between tokens
// when there's a "current" already in progress; for an empty
// token list the result is "" (not "/").
//
// Each token is escaped on output: "~" -> "~0", "/" -> "~1",
// applied in that order. The order matters: a literal "~1"
// in a key would round-trip as "/1" through split() (because
// "~1" unescapes to "/"), and to_string() therefore re-escapes
// that "/" back to "~1".
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
// resolve — look up a sub-value by JSON Pointer
// ===========================================================================
//
// Walks `root` following each token. Returns a non-owning
// pointer into `root` for the resolved sub-value on success,
// or a typed JsonExtError on failure. The returned pointer
// is valid for as long as `root` is alive (which the caller
// owns).
//
// The token list is most commonly obtained from split(); an
// empty token list resolves to the root itself, matching RFC
// 6901's "" case.
//
// Dispatch:
//   - If the next token is "-" and the current value is an
//     array, return LastArrayElement (per the design note
//     above: "-" has no element to return; only Patch can
//     handle it).
//   - If the current value is an object and the next token
//     is a key, look it up via std::map::find. NotFound
//     on miss, NotAnObject if the current value isn't an
//     object (or array — we don't try keys on arrays, that
//     would mask the difference between "missing key" and
//     "wrong kind").
//   - If the current value is an array, treat the next
//     token as a non-negative integer index. "-" goes
//     through the LastArrayElement branch above. A
//     non-digit token gives IndexNotANumber. An out-of-
//     range index gives IndexOutOfRange. A valid index
//     descends into that element.
//   - If the current value is a scalar / null / unset, the
//     next token gives NotAnObject / NotAnArray (whichever
//     matches the token's *kind* — but we report a single
//     "NotAnObject" or "NotAnArray" based on whether the
//     next token is "-" or a non-negative integer or
//     something else).
//
// Non-mutating. The returned JsonValue* points into `root`;
// the caller can read it but should not modify it (JsonValue
// is by-value in the tree today; if a future lesson makes it
// mutable-in-place, the same handle will become writable).
inline std::expected<const JsonValue*, JsonExtError>
resolve(const std::vector<ReferenceToken>& tokens,
        const JsonValue& root) noexcept {
    const JsonValue* cur = &root;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i].value;

        // Check array-indexing first because arrays AND objects
        // can be hit by an index-shaped token; we want the
        // array dispatch to take precedence.
        if (const auto* arr = std::get_if<std::vector<JsonValue>>(&cur->value)) {
            // RFC 6901 §4: "-" inside an array refers to the
            // (nonexistent) member after the last element.
            // resolve() has no such element to return; Patch
            // will handle it.
            if (tok == "-") {
                return std::unexpected{JsonExtError::LastArrayElement};
            }
            // All non-negative decimal digits -> parse as
            // size_t. Anything else (including empty) is
            // IndexNotANumber.
            if (tok.empty() || tok.size() > 20) {
                return std::unexpected{JsonExtError::IndexNotANumber};
            }
            std::size_t idx = 0;
            for (char c : tok) {
                if (c < '0' || c > '9') {
                    return std::unexpected{JsonExtError::IndexNotANumber};
                }
                const std::size_t digit = static_cast<std::size_t>(c - '0');
                // Tight overflow check: 10 * idx + digit > SIZE_MAX.
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

        if (const auto* obj = std::get_if<std::map<std::string, JsonValue>>(&cur->value)) {
            // Object-key lookup. "-" is treated as a literal
            // key here, not as "last element" — that only
            // applies inside arrays.
            auto it = obj->find(tok);
            if (it == obj->end()) {
                return std::unexpected{JsonExtError::NotFound};
            }
            cur = &it->second;
            continue;
        }

        // cur is a scalar / null / unset. We have more
        // tokens to consume; report the kind mismatch.
        // We don't try to be smart about which kind of
        // token we got — both NotAnObject and NotAnArray
        // are "you tried to descend into something you
        // can't".
        if (tok == "-" || (!tok.empty() && tok[0] >= '0' && tok[0] <= '9')) {
            return std::unexpected{JsonExtError::NotAnArray};
        }
        return std::unexpected{JsonExtError::NotAnObject};
    }

    return cur;
}

// Convenience overload: parse a JSON Pointer string and resolve
// in one call. Returns the same expected type as resolve().
// MalformedToken bubbles up from split().
inline std::expected<const JsonValue*, JsonExtError>
resolve(std::string_view pointer, const JsonValue& root) noexcept {
    auto toks = split(pointer);
    if (!toks) return std::unexpected{toks.error()};
    return resolve(*toks, root);
}

}  // namespace json_pointer
}  // namespace psp

#endif  // PSP_SPAN_JSON_EXT_H_INCLUDED
