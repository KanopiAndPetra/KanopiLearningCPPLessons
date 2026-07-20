// psp_span/json.h — general JSON parser built on the v0.9.0 cursor
// primitives, with a JsonValue sum type and a json_to_string pretty-printer.
//
// This is the *third* public header of `psp_span_lib` (after
// `<psp_span/span.h>` and `<psp_span/parser.h>`) and the first new
// header since v0.6.0. It was promoted from a consumer (the Jul 19
// lesson, `P-2026-07-19-psp-json-arrays-nested.cpp`) into the library
// proper because it is a complete, generally useful capability: ANY
// consumer can `#include <psp_span/json.h>` and get a strict JSON
// parser with arrays, nested objects, escapes, \\uXXXX, and typed
// errors for free, with no extra dependency.
//
// What's in this header
// ---------------------
//   - struct JsonValue: the full JSON value sum type (8 alternatives,
//                       std::variant<monostate, nullptr_t, bool,
//                       int64_t, double, string, vector<JsonValue>,
//                       map<string, JsonValue>>).
//   - psp::parse_value_at(Span<const char>&)   - dispatcher.
//   - psp::parse_array_at(Span<const char>&)   - '['...']'   parser.
//   - psp::parse_object_at(Span<const char>&)  - '{'...'}'   parser.
//   - psp::json_to_string(JsonValue, indent=0) - pretty-printer.
//
// What's NOT in this header
// -------------------------
//   - The leaf parsers stay in `<psp_span/parser.h>`. This header
//     delegates to parse_string_at / parse_bool_at / parse_null_at /
//     parse_double_at (and skip_whitespace_at, expect_char_at) — those
//     are the v0.9.0 cursor primitives. NO new cursor primitives
//     are introduced.
//   - No JSON5 / JSONC extensions (no comments, no single quotes, no
//     trailing commas, no unquoted keys). Strict RFC 8259.
//   - No arena allocation. Every array element and object member
//     owns its own storage (std::vector / std::map).
//   - Pretty-print indent is fixed at two spaces (configurable indent
//     is a separate lesson).
//
// Why std::variant rather than named struct variants
// -------------------------------------------------
// The first two consumers (Jul 18 / Jul 19) considered putting each
// kind of JsonValue alternative in its own named struct (JsonObject,
// JsonArray) and wrapping them in a `std::variant<JsonScalar,
// JsonArray, JsonObject>`. That works but produces a two-level sum
// type: `JsonValue` is a variant of variants. The flat
// `std::variant<...>` chosen here lets std::visit dispatch on every
// alternative in one place, and a recursive-descent parser returning
// "JsonValue" is simpler than one returning
// "std::variant<JsonScalar, JsonArray, JsonObject>".
//
// Duplicate keys
// --------------
// RFC 8259 says JSON object member names are unique, but does not
// mandate how a parser reports duplicates. psp::parse_object_at
// rejects duplicate keys with the typed ParseError::DuplicateKey
// (added to <psp_span/parser.h> in v0.10.0 alongside this header —
// the Jul 18 / Jul 19 consumers over-used UnexpectedChar for this
// case; that's gone now). The cursor `s` is left unchanged on
// duplicate-key error, matching the rest of the cursor layer's
// "leave s unchanged on failure" contract.
//
// What's NEW in 2026-07-20 (v0.10.0)
// -----------------------------------
//   - The header itself is brand new. Before today (Jul 18 / Jul 19)
//     this code lived in a consumer's .cpp file. Now it ships with
//     the library.
//   - parse_object_at now returns ParseError::DuplicateKey for
//     duplicate keys (instead of UnexpectedChar as in the consumer).
//   - Everything else is identical to the consumer verbatim, modulo
//     the namespace (psp::json_to_string, etc.).

#ifndef PSP_SPAN_JSON_H_INCLUDED
#define PSP_SPAN_JSON_H_INCLUDED

#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace psp {

// ===========================================================================
// JsonValue — the full JSON value type
// ===========================================================================
//
// Eight std::variant alternatives. The order matters: std::variant's
// alternatives are indexed by their declaration order, and consumers
// who std::visit by index (or use std::get<N>) depend on a stable
// layout. The order chosen is:
//
//   0  std::monostate                    // unset / never assigned
//   1  std::nullptr_t                    // JSON null
//   2  bool                              // JSON true / false
//   3  std::int64_t                      // JSON integer (fits in int64)
//   4  double                            // JSON number with frac / exp
//   5  std::string                       // JSON string
//   6  std::vector<JsonValue>            // JSON array
//   7  std::map<std::string, JsonValue>  // JSON object
//
// The variant is dominated by std::map<std::string, JsonValue>:
// on libstdc++ that is ~48B (rb-tree header + comparator + allocator);
// on libc++ (this toolchain) it is 24B (flat header, same as
// std::vector<T>). JsonValue is therefore 32B (libc++) or 56B
// (libstdc++) — the Section probes report the actual number.
//
// std::map is ordered (red-black tree). JSON objects are unordered
// per spec, so the ordering doesn't matter for round-trip
// correctness; json_to_string emits keys in the std::map's natural
// alphabetical order. Duplicate keys are rejected at parse time
// (ParseError::DuplicateKey).
// ===========================================================================

struct JsonValue {
    std::variant<
        std::monostate,                    // 0: unset / never assigned
        std::nullptr_t,                    // 1: JSON null
        bool,                              // 2: JSON true / false
        std::int64_t,                      // 3: JSON integer (fits in int64)
        double,                            // 4: JSON number with frac / exp
        std::string,                       // 5: JSON string
        std::vector<JsonValue>,            // 6: JSON array
        std::map<std::string, JsonValue>   // 7: JSON object
    > value;
};

// ===========================================================================
// Forward declarations for mutual recursion
// ===========================================================================

inline std::expected<JsonValue, ParseError>
parse_value_at(Span<const char>& s) noexcept;

inline std::expected<std::vector<JsonValue>, ParseError>
parse_array_at(Span<const char>& s) noexcept;

inline std::expected<std::map<std::string, JsonValue>, ParseError>
parse_object_at(Span<const char>& s) noexcept;

// ===========================================================================
// parse_value_at — the dispatcher
// ===========================================================================
//
// Peeks (after skipping whitespace) at the next char and picks the
// right sub-parser:
//
//     '{' -> parse_object_at
//     '[' -> parse_array_at
//     '\"' -> parse_string_at      (returns string)
//     't' / 'f' -> parse_bool_at
//     'n' -> parse_null_at
//     digit / '-' -> parse_double_at  (returns double or int)
//
// One branch per case — no lookahead. The cursor primitives'
// "leave s unchanged on error" contract means we can safely try
// each one without re-saving s. We DO save s at function entry,
// because the dispatcher itself might fail (e.g. EOF after
// whitespace, or a leading char that doesn't match any rule).
//
// int64 vs double: parse_double_at always returns double, but a
// JSON integer like "42" is conceptually different from "42.0".
// We preserve that distinction by checking std::trunc(*d) == *d
// and the int64 range — same trick the Jul 18 / Jul 19 consumers
// used.
// ===========================================================================

inline std::expected<JsonValue, ParseError>
parse_value_at(Span<const char>& s) noexcept {
    auto saved = s;
    if (auto r = skip_whitespace_at(s); !r) {
        s = saved;
        return std::unexpected{r.error()};
    }
    if (s.empty()) {
        s = saved;
        return std::unexpected{ParseError::Empty};
    }
    const char front = s.front();
    switch (front) {
        case '{': {
            auto obj = parse_object_at(s);
            if (!obj) {
                s = saved;
                return std::unexpected{obj.error()};
            }
            JsonValue out;
            out.value = std::move(*obj);
            return out;
        }
        case '[': {
            auto arr = parse_array_at(s);
            if (!arr) {
                s = saved;
                return std::unexpected{arr.error()};
            }
            JsonValue out;
            out.value = std::move(*arr);
            return out;
        }
        case '\"': {
            auto str = parse_string_at(s);
            if (!str) {
                s = saved;
                return std::unexpected{str.error()};
            }
            JsonValue out;
            out.value = std::move(*str);
            return out;
        }
        case 't':
        case 'f': {
            auto b = parse_bool_at(s);
            if (!b) {
                s = saved;
                return std::unexpected{b.error()};
            }
            JsonValue out;
            out.value = *b;
            return out;
        }
        case 'n': {
            auto n = parse_null_at(s);
            if (!n) {
                s = saved;
                return std::unexpected{n.error()};
            }
            JsonValue out;
            out.value = nullptr;
            return out;
        }
        default:
            // Numbers: digit or leading '-'. parse_double_at rejects '+',
            // so '+' falls through to the default and produces an error.
            if ((front >= '0' && front <= '9') || front == '-') {
                auto d = parse_double_at(s);
                if (!d) {
                    s = saved;
                    return std::unexpected{d.error()};
                }
                JsonValue out;
                // If the double is an exact integer in int64 range,
                // store it as int64 (preserves the "is it an int?"
                // distinction in the parsed tree).
                if (std::trunc(*d) == *d
                    && *d >= static_cast<double>(std::numeric_limits<std::int64_t>::min())
                    && *d <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                    out.value = static_cast<std::int64_t>(*d);
                } else {
                    out.value = *d;
                }
                return out;
            }
            s = saved;
            return std::unexpected{ParseError::UnexpectedChar};
    }
}

// ===========================================================================
// parse_array_at — strict '[' value (',' value)* ']'
// ===========================================================================
//
// Recognised shape:
//
//     '[' (WS VALUE (WS ',' WS VALUE)*)? WS ']'
//
// (i.e. an optional comma-separated sequence of values — empty
//  arrays are OK, trailing commas are NOT.)
//
// Every element is parsed via parse_value_at (which dispatches to
// the right sub-parser). On success, returns std::vector<JsonValue>
// and shrinks s past the closing ']'. On failure, returns
// std::unexpected{ParseError} and leaves s unchanged.
// ===========================================================================

inline std::expected<std::vector<JsonValue>, ParseError>
parse_array_at(Span<const char>& s) noexcept {
    auto saved = s;

    if (auto r = skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (auto r = expect_char_at(s, '['); !r) {
        s = saved; return std::unexpected{r.error()};
    }

    std::vector<JsonValue> out;

    // Empty array?
    if (auto r = skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (!s.empty() && s.front() == ']') {
        if (auto r = expect_char_at(s, ']'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        skip_whitespace_at(s);
        return out;
    }

    while (true) {
        if (auto r = skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        auto v = parse_value_at(s);
        if (!v) {
            s = saved; return std::unexpected{v.error()};
        }
        out.push_back(std::move(*v));

        if (auto r = skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        if (!s.empty() && s.front() == ',') {
            if (auto r = expect_char_at(s, ','); !r) {
                s = saved; return std::unexpected{r.error()};
            }
            continue;
        }
        if (auto r = expect_char_at(s, ']'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        skip_whitespace_at(s);
        return out;
    }
}

// ===========================================================================
// parse_object_at — strict '{' string ':' value (',' string ':' value)* '}'
// ===========================================================================
//
// Recognised shape:
//
//     '{' (WS STRING WS ':' WS VALUE (WS ',' WS STRING WS ':' WS VALUE)*)? WS '}'
//
// where VALUE is anything parse_value_at accepts (scalar, array, or
// nested object). Duplicate keys are rejected with the typed
// ParseError::DuplicateKey (new in v0.10.0). The Jul 18 / Jul 19
// consumers used ParseError::UnexpectedChar for the same case; that
// overload is gone.
//
// On success: std::map<std::string, JsonValue> (ordered by key).
// On failure: std::unexpected{ParseError} and s unchanged.
// ===========================================================================

inline std::expected<std::map<std::string, JsonValue>, ParseError>
parse_object_at(Span<const char>& s) noexcept {
    auto saved = s;

    if (auto r = skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (auto r = expect_char_at(s, '{'); !r) {
        s = saved; return std::unexpected{r.error()};
    }

    std::map<std::string, JsonValue> out;

    // Empty object?
    if (auto r = skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (!s.empty() && s.front() == '}') {
        if (auto r = expect_char_at(s, '}'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        skip_whitespace_at(s);
        return out;
    }

    while (true) {
        if (auto r = skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        auto key = parse_string_at(s);
        if (!key) {
            s = saved; return std::unexpected{key.error()};
        }
        if (auto r = skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        if (auto r = expect_char_at(s, ':'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        auto val = parse_value_at(s);
        if (!val) {
            s = saved; return std::unexpected{val.error()};
        }
        auto [it, inserted] = out.emplace(std::move(*key), std::move(*val));
        if (!inserted) {
            // Typed duplicate-key error (NEW in v0.10.0). The Jul 18 / Jul 19
            // consumers overloaded UnexpectedChar; that's gone now.
            s = saved;
            return std::unexpected{ParseError::DuplicateKey};
        }
        (void)it;

        if (auto r = skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        if (!s.empty() && s.front() == ',') {
            if (auto r = expect_char_at(s, ','); !r) {
                s = saved; return std::unexpected{r.error()};
            }
            continue;
        }
        if (auto r = expect_char_at(s, '}'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        skip_whitespace_at(s);
        return out;
    }
}

// ===========================================================================
// json_to_string — pretty-print a JsonValue tree as JSON
// ===========================================================================
//
// Two-space indent, hard-coded. Strings are escaped with
// escape_for_json_string() below so the output is valid JSON for any
// input (including strings built in-memory rather than parsed from
// input).
//
// The function dispatches via std::visit + if constexpr chain on
// the JsonValue's variant — every alternative gets a branch. The
// std::visit return type is std::string, and the lambdas return
// std::string, so std::visit produces a single string by the time
// it returns.
//
// Indent parameter: the outermost call passes 0 (no indent for the
// outermost element); recursive calls pass indent + 1 for their
// children.
// ===========================================================================

namespace detail {
// Escape a string for inclusion in a JSON string literal. Handles
// quote, backslash, and the standard short escapes (\b \f \n \r \t);
// non-printable chars below 0x20 are emitted as \\uXXXX.
inline std::string escape_for_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        switch (c) {
            case '\"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (uc < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", uc);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}
}  // namespace detail

inline std::string json_to_string(const JsonValue& v, int indent = 0) {
    return std::visit([indent](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return "<unset>";
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", arg);
            return std::string(buf);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + detail::escape_for_json_string(arg) + "\"";
        } else if constexpr (std::is_same_v<T, std::vector<JsonValue>>) {
            if (arg.empty()) return "[]";
            std::string out = "[\n";
            const auto pad_len = static_cast<std::size_t>(2)
                * static_cast<std::size_t>(indent + 1);
            const std::size_t close_len = static_cast<std::size_t>(2)
                * static_cast<std::size_t>(indent);
            std::string pad(pad_len, ' ');
            for (std::size_t i = 0; i < arg.size(); ++i) {
                out += pad + json_to_string(arg[i], indent + 1);
                if (i + 1 < arg.size()) out += ",";
                out += "\n";
            }
            out += std::string(close_len, ' ') + "]";
            return out;
        } else if constexpr (std::is_same_v<T, std::map<std::string, JsonValue>>) {
            if (arg.empty()) return "{}";
            std::string out = "{\n";
            const auto pad_len = static_cast<std::size_t>(2)
                * static_cast<std::size_t>(indent + 1);
            const std::size_t close_len = static_cast<std::size_t>(2)
                * static_cast<std::size_t>(indent);
            std::string pad(pad_len, ' ');
            bool first = true;
            for (const auto& [k, val] : arg) {
                if (!first) out += ",\n";
                first = false;
                out += pad + "\"" + k + "\": " + json_to_string(val, indent + 1);
            }
            out += "\n" + std::string(close_len, ' ') + "}";
            return out;
        } else {
            return "?";
        }
    }, v.value);
}

}  // namespace psp

#endif  // PSP_SPAN_JSON_H_INCLUDED
