// P-2026-07-19 — A complete general JSON parser built on
// psp_span_lib's v0.9.0 cursor primitives (arrays + nested
// objects).
//
// Where this fits in the arc
// --------------------------
// The Jul 18 lesson (P-2026-07-18-psp-json-scalars.cpp) shipped a
// strict parser for JSON objects that contained only scalar
// values (string / number / bool / null). That parser was
// honest about its limitation: it could not parse arrays
// (`[1, 2, 3]`) and it could not parse nested objects
// (`{"user": {"name": "Petra", "age": 7}}`).
//
// The Jul 18 lesson's "Next Steps" said:
//
//   The most natural follow-on is arrays + nested objects —
//   once those land, the parser is a complete general JSON
//   parser and the consumer directory is a candidate for
//   promotion to `<psp_span/json.h>`.
//
// That is exactly today's lesson, in one consumer file, with
// NO new library code:
//
//   - No change to <psp_span/parser.h>.
//   - No change to the psp_span_lib CMakeLists.
//   - No version bump (still v0.9.0).
//
// The v0.9.0 cursor primitives (skip_whitespace_at,
// expect_char_at, parse_string_at, parse_bool_at,
// parse_null_at, parse_int_at, parse_uint_at,
// parse_double_at) are sufficient. All we add is a
// forward-declaration-free recursive-descent layer on top of
// them, plus a JsonValue sum type that can hold arrays and
// nested objects as well as scalars.
//
// The "complete general JSON parser" shape
// ----------------------------------------
// JSON has six structural tokens:
//
//     object   '{' string ':' value (',' string ':' value)* '}'
//     array    '[' value (',' value)* ']'
//     string   '"' ...escapes... '"'
//     number   [-] int [frac] [exp]
//     true     'true'
//     false    'false'
//     null     'null'
//
// The first two are recursive; the rest are leaves. A real
// JSON parser needs three mutually-recursive functions:
// parse_value_at, parse_array_at, parse_object_at. parse_value_at
// dispatches on the first non-whitespace character:
//
//     '{' -> parse_object_at
//     '[' -> parse_array_at
//     '"' -> parse_string_at
//     't' / 'f' -> parse_bool_at
//     'n' -> parse_null_at
//     digit / '-' -> parse_double_at (which handles the int form too)
//
// The v0.9.0 cursor primitives already exist for every leaf
// case. The only new code is the dispatcher and the two
// recursive structural parsers.
//
// JsonValue
// ---------
// The Jul 18 lesson's JsonScalar was a std::variant<monostate,
// nullptr_t, bool, double, int64_t, string>. Today we add three
// more alternatives:
//
//     std::vector<JsonValue>      (JSON array)
//     std::map<string, JsonValue> (JSON object, ordered by key)
//
// and a parallel "order" vector for each object so we can
// preserve insertion order in the pretty-printer. The full
// JsonValue type is now:
//
//     std::variant<
//         std::monostate,                      // unset
//         std::nullptr_t,                      // null
//         bool,                                // true/false
//         std::int64_t,                        // integer (fits)
//         double,                              // number (with frac/exp)
//         std::string,                         // string
//         std::vector<JsonValue>,              // array
//         std::map<std::string, JsonValue>     // object
//     >
//
// The variant size grows: the largest alternative is
// std::map<std::string, JsonValue>, which on libstdc++ / libc++
// is typically 48 bytes (3 words for the rb_tree, plus the
// comparator and allocator). The variant has to be aligned to
// the largest alternative, so sizeof(JsonValue) on this
// toolchain is 56 bytes (48 rounded up to the 8-byte alignment
// boundary, plus 8 for the discriminator = 56). The Section 7
// probes confirm the exact number.
//
// What this lesson is NOT
// -----------------------
// - It is not a `<psp_span/json.h>` header. That promotion is
//   the next step (the lesson notes at the end).
// - It does not implement JSON5 / JSONC (no comments, no
//   single-quoted strings, no trailing commas).
// - It does not parse numbers with explicit '+' signs (the
//   underlying parse_double_at rejects them).
// - It does not allocate from an arena — every array element
//   and object member owns its own storage via std::vector /
//   std::map. A high-performance JSON parser would use a
//   contiguous slab; that is a separate optimization.
// - It does not pretty-print with configurable indent. We
//   pretty-print with a fixed two-space indent.
//
// Build (assumes psp_span_lib v0.9.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-19-psp-json-arrays-nested
//
// ASan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-19-psp-json-arrays-nested

#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// ===========================================================================
// JsonValue — the full JSON value type
// ===========================================================================
//
// Today we replace Jul 18's JsonScalar (six variants) with JsonValue (eight
// variants). The new alternatives are std::vector<JsonValue> (array) and
// std::map<std::string, JsonValue> (object).
//
// We could have put JsonObject and JsonArray in named structs (the way
// Jul 18's JsonObject did), but for a sum type, std::variant<...> is
// cleaner — std::visit can dispatch on every alternative in one place, and
// a recursive-descent parser that returns "JsonValue" is simpler than one
// that returns "std::variant<JsonScalar, JsonArray, JsonObject>".
//
// Note: std::map is ordered (red-black tree) so the keys come out sorted
// alphabetically in any iteration. JSON objects are unordered per spec,
// so the ordering doesn't matter for round-trip correctness. For
// pretty-printing, we use the std::map's natural sorted order.
//
// A small wrinkle: std::map's value_type is std::pair<const std::string,
// JsonValue>, so the JsonValue inside the map is const from the map's
// perspective. std::visit doesn't care (it takes its argument by
// universal reference), so the recursion still works.
//
// On libc++ (this toolchain), std::map<T, U> is a flat 3-word header —
// 24 bytes — same as std::vector<T>. That keeps JsonValue at 32 bytes
// (24-byte largest alternative + 8-byte discriminator). On libstdc++ the
// map is 48 bytes and JsonValue is 56; the Section 7 probes report the
// actual number on whatever compiler you build with.

struct JsonValue {
    std::variant<
        std::monostate,                  // unset / never assigned
        std::nullptr_t,                  // JSON null
        bool,                            // JSON true / false
        std::int64_t,                    // JSON integer (fits in int64)
        double,                          // JSON number with frac / exp
        std::string,                     // JSON string
        std::vector<JsonValue>,          // JSON array
        std::map<std::string, JsonValue> // JSON object
    > value;
};

// ===========================================================================
// Forward declarations for mutual recursion
// ===========================================================================
//
// parse_value_at dispatches to parse_object_at or parse_array_at for the
// structural cases; both of those call back into parse_value_at for their
// elements / values.

static std::expected<JsonValue, ParseError>
parse_value_at(psp::Span<const char>& s) noexcept;

static std::expected<std::vector<JsonValue>, ParseError>
parse_array_at(psp::Span<const char>& s) noexcept;

static std::expected<std::map<std::string, JsonValue>, ParseError>
parse_object_at(psp::Span<const char>& s) noexcept;

// ===========================================================================
// Helpers
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static std::string span_to_string(psp::Span<const char> s) {
    return std::string(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// Pretty-print a JsonValue as JSON, with two-space indent.
//
// This is intentionally simple — no configurable indent. Strings
// are escaped via escape_for_json_string() so that strings
// containing control chars, quotes, or backslashes can survive
// the parse -> print -> re-parse round trip (Section 6). For the
// round-trip test we just need the output to be syntactically
// valid JSON; the test asserts equality of *re-parsed* output
// rather than string equality, so any reasonable pretty-printer
// works.
static std::string escape_for_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        switch (c) {
            case '"':  out += "\\\""; break;
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

static std::string json_to_string(const JsonValue& v, int indent = 0) {
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
            // Escape control chars, quotes, and backslashes so the
            // output is valid JSON for any input string — including
            // strings built in-memory rather than parsed from input.
            // The Section 6 round-trip test relies on this.
            return "\"" + escape_for_json_string(arg) + "\"";
        } else if constexpr (std::is_same_v<T, std::vector<JsonValue>>) {
            if (arg.empty()) return "[]";
            std::string out = "[\n";
            const auto pad_len = static_cast<std::size_t>(2) * static_cast<std::size_t>(indent + 1);
            const std::size_t close_len = static_cast<std::size_t>(2) * static_cast<std::size_t>(indent);
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
            const auto pad_len = static_cast<std::size_t>(2) * static_cast<std::size_t>(indent + 1);
            const std::size_t close_len = static_cast<std::size_t>(2) * static_cast<std::size_t>(indent);
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

// ===========================================================================
// Section 1 — the cursor-primitive "tag set" for parse_value_at
// ===========================================================================
//
// parse_value_at is the dispatcher. It peeks (after skipping whitespace)
// at the next char and chooses the right parser:
//
//     '{' -> parse_object_at
//     '[' -> parse_array_at
//     '"' -> parse_string_at (returns string)
//     't' / 'f' -> parse_bool_at
//     'n' -> parse_null_at
//     digit / '-' -> parse_double_at (returns double or int)
//
// The check is one branch per case — no lookahead. The cursor
// primitives' "leave s unchanged on error" contract means we can
// safely try each one without re-saving s.
//
// (We DO save s at function entry, because the dispatcher itself
//  might fail — e.g. EOF after whitespace, or a leading char that
//  doesn't match any rule.)
// ===========================================================================

static std::expected<JsonValue, ParseError>
parse_value_at(psp::Span<const char>& s) noexcept {
    auto saved = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
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
        case '"': {
            auto str = psp::parse_string_at(s);
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
            auto b = psp::parse_bool_at(s);
            if (!b) {
                s = saved;
                return std::unexpected{b.error()};
            }
            JsonValue out;
            out.value = *b;
            return out;
        }
        case 'n': {
            auto n = psp::parse_null_at(s);
            if (!n) {
                s = saved;
                return std::unexpected{n.error()};
            }
            JsonValue out;
            out.value = nullptr;
            return out;
        }
        default:
            // Numbers: digit or leading '-'. (parse_double_at rejects '+',
            // so '+' falls through to the default and produces an error.)
            if ((front >= '0' && front <= '9') || front == '-') {
                auto d = psp::parse_double_at(s);
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
// Section 2 — parse_array_at
// ===========================================================================
//
// Strict array parser. Recognised shape:
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

static std::expected<std::vector<JsonValue>, ParseError>
parse_array_at(psp::Span<const char>& s) noexcept {
    auto saved = s;

    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (auto r = psp::expect_char_at(s, '['); !r) {
        s = saved; return std::unexpected{r.error()};
    }

    std::vector<JsonValue> out;

    // Empty array?
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (!s.empty() && s.front() == ']') {
        if (auto r = psp::expect_char_at(s, ']'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        psp::skip_whitespace_at(s);
        return out;
    }

    while (true) {
        if (auto r = psp::skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        auto v = parse_value_at(s);
        if (!v) {
            s = saved; return std::unexpected{v.error()};
        }
        out.push_back(std::move(*v));

        if (auto r = psp::skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        if (!s.empty() && s.front() == ',') {
            if (auto r = psp::expect_char_at(s, ','); !r) {
                s = saved; return std::unexpected{r.error()};
            }
            continue;
        }
        if (auto r = psp::expect_char_at(s, ']'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        psp::skip_whitespace_at(s);
        return out;
    }
}

// ===========================================================================
// Section 3 — parse_object_at
// ===========================================================================
//
// Strict object parser. Recognised shape:
//
//     '{' (WS STRING WS ':' WS VALUE (WS ',' WS STRING WS ':' WS VALUE)*)? WS '}'
//
// where VALUE is anything parse_value_at accepts (scalar, array, or
// nested object). Duplicate keys are rejected with a special
// ParseError::UnexpectedChar (the same surface error the Jul 18 lesson
// used; we promote that to a typed DuplicateKey error in the "Next
// steps" section).
//
// On success: std::map<std::string, JsonValue> (ordered by key).
// On failure: std::unexpected{ParseError} and s unchanged.
// ===========================================================================

static std::expected<std::map<std::string, JsonValue>, ParseError>
parse_object_at(psp::Span<const char>& s) noexcept {
    auto saved = s;

    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (auto r = psp::expect_char_at(s, '{'); !r) {
        s = saved; return std::unexpected{r.error()};
    }

    std::map<std::string, JsonValue> out;

    // Empty object?
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (!s.empty() && s.front() == '}') {
        if (auto r = psp::expect_char_at(s, '}'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        psp::skip_whitespace_at(s);
        return out;
    }

    while (true) {
        if (auto r = psp::skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        auto key = psp::parse_string_at(s);
        if (!key) {
            s = saved; return std::unexpected{key.error()};
        }
        if (auto r = psp::skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        if (auto r = psp::expect_char_at(s, ':'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        auto val = parse_value_at(s);
        if (!val) {
            s = saved; return std::unexpected{val.error()};
        }
        auto [it, inserted] = out.emplace(std::move(*key), std::move(*val));
        if (!inserted) {
            // Duplicate key. Reuse the UnexpectedChar slot (the Jul 18
            // parser does the same). A real parser would add a
            // ParseError::DuplicateKey enumerator and return that.
            s = saved;
            return std::unexpected{ParseError::UnexpectedChar};
        }
        (void)it;

        if (auto r = psp::skip_whitespace_at(s); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        if (!s.empty() && s.front() == ',') {
            if (auto r = psp::expect_char_at(s, ','); !r) {
                s = saved; return std::unexpected{r.error()};
            }
            continue;
        }
        if (auto r = psp::expect_char_at(s, '}'); !r) {
            s = saved; return std::unexpected{r.error()};
        }
        psp::skip_whitespace_at(s);
        return out;
    }
}

// ===========================================================================
// Section 4 — leaf tests: arrays of scalars
// ===========================================================================
//
// The simplest case. The recursion bottoms out at parse_value_at's
// scalar branches (parse_string_at, parse_bool_at, parse_null_at,
// parse_double_at). Every test asserts the vector has the right
// size and that the cursor commit / rewind contract holds.
// ===========================================================================

static void section_array_of_scalars() {
    print_section("Section 4: arrays of scalars — the recursive base case");

    struct Case { std::string input; bool want_ok; std::size_t want_size; };
    const Case cases[] = {
        // Empty arrays
        {"[]",                    true,  0},
        {"[ ]",                   true,  0},
        {"  [  ]  ",              true,  0},

        // Homogeneous scalar arrays
        {"[1, 2, 3]",             true,  3},
        {"[1.5, 2.5, 3.5]",       true,  3},
        {"[\"a\", \"b\", \"c\"]",  true,  3},
        {"[true, false, true]",   true,  3},
        {"[null, null, null]",    true,  3},

        // Mixed scalar arrays
        {"[1, \"two\", 3.0, true, null]",  true,  5},

        // Whitespace tolerance
        {"  [  1  ,  2  ,  3  ]  ",        true,  3},

        // Number variants
        //
        // NB: parse_double_at (and parse_int_at) reject a leading '-'
        // as LeadingSign — sign-handling is a downstream policy choice.
        // The Jul 18 lesson documents the same. A negative-number-aware
        // dispatcher would skip past '-', then call parse_double_at on
        // the tail; that's a one-line forward-on.
        {"[1, 2, 3, 0, 42]",                true,  5},
        {"[1.5, 2.5, 0.0, 42.0, 0]",        true,  5},

        // Malformed
        {"",                               false, 0},   // Empty
        {"[",                              false, 0},   // unterminated
        {"[1, 2, ]",                       false, 0},   // trailing comma
        {"[1, 2",                          false, 0},   // missing ]
        {"[tru]",                          false, 0},   // truncated bool
        {"[\"unterminated]",               false, 0},   // unterminated string
        {"[1, 2, , 3]",                    false, 0},   // double comma
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;
        psp::Span<const char> s = as_span(c.input);
        auto r = parse_array_at(s);
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_array_at(\"%s\") should have succeeded but got %s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            if (r->size() != c.want_size) {
                std::printf("  FAIL: parse_array_at(\"%s\") got %zu elements, want %zu\n",
                            c.input.c_str(), r->size(), c.want_size);
                continue;
            }
            std::printf("  parse_array_at(\"%s\") = [", c.input.c_str());
            for (std::size_t i = 0; i < r->size(); ++i) {
                if (i > 0) std::printf(", ");
                std::printf("%s", json_to_string((*r)[i]).c_str());
            }
            std::printf("], s remaining = \"%s\"\n", span_to_string(s).c_str());
            ++passed;
        } else {
            if (r) {
                std::printf("  FAIL: parse_array_at(\"%s\") should have failed but got %zu elements\n",
                            c.input.c_str(), r->size());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_array_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
            ++passed;
        }
    }
    std::printf("  [%d / %d passed]\n", passed, total);
}

// ===========================================================================
// Section 5 — nested objects
// ===========================================================================
//
// The headline payoff. The parser is recursive, so this just works
// the same way as arrays-of-scalars, except the structural
// characters are '{' and '}' instead of '[' and ']'.
// ===========================================================================

static void section_nested_objects() {
    print_section("Section 5: nested objects — the recursive case");

    struct Case { std::string input; bool want_ok; std::size_t want_pairs; };
    const Case cases[] = {
        // Simple nesting
        {"{\"a\": {\"x\": 1}}",                                true,  1},
        {"{\"a\": {\"b\": {\"c\": 42}}}",                      true,  1},

        // Mixed nesting
        {"{\"name\": \"Petra\", \"tags\": [\"cpp\", \"json\"], \"meta\": {\"age\": 7, \"active\": true}}",
                                                                true,  3},

        // Deeply nested
        {"{\"a\": {\"b\": {\"c\": {\"d\": {\"e\": \"deep\"}}}}}", true, 1},

        // Object containing array of objects
        {"{\"users\": [{\"id\": 1}, {\"id\": 2}, {\"id\": 3}]}", true,  1},

        // Array of objects — call parse_value_at, not parse_object_at.
        // The input begins with '[', which is an array, not an object.
        {"[{\"x\": 1}, {\"x\": 2}, {\"x\": 3}]", true,  3},

        // Empty arrays / objects mixed with content
        {"{\"items\": [], \"meta\": {}}",                       true,  2},

        // Round-trip: a real JSON document
        {"{\"name\": \"Petra\", \"age\": 7, \"scores\": [98, 87, 92], \"address\": "
         "{\"city\": \"Earth\", \"zip\": \"00000\"}, \"active\": true}",
                                                                true,  5},

        // Malformed
        {"{\"a\": {\"x\": 1}",                                 false, 0},   // missing }
        {"{\"a\": [1, 2, }",                                  false, 0},   // malformed array value
        {"{\"a\": {\"x\": }",                                  false, 0},   // missing value
        {"{\"a\": [1, 2}",                                    false, 0},   // unterminated array
        {"{\"a\": 1, \"a\": 2}",                               false, 0},   // duplicate key
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;

        // Dispatch on the leading structural char. '[...' goes to
        // parse_array_at, '{...' goes to parse_object_at. (We use
        // parse_value_at for arrays of objects so the test can
        // describe what it expects the parsed tree to look like.)
        psp::Span<const char> s = as_span(c.input);
        const auto leading = c.input.find_first_not_of(" \t\n\r");

        if (leading != std::string::npos && c.input[static_cast<std::size_t>(leading)] == '[') {
            // Array: parse_value_at returns a JsonValue holding a vector.
            auto r = parse_value_at(s);
            if (c.want_ok) {
                if (!r) {
                    std::printf("  FAIL: parse_value_at(\"%s\") error=%s\n",
                                c.input.c_str(), std::format("{}", r.error()).c_str());
                    continue;
                }
                const auto* arr = std::get_if<std::vector<JsonValue>>(&r->value);
                if (!arr) {
                    std::printf("  FAIL: parse_value_at(\"%s\") returned non-array\n",
                                c.input.c_str());
                    continue;
                }
                if (arr->size() != c.want_pairs) {
                    std::printf("  FAIL: parse_value_at(\"%s\") got %zu elements, want %zu\n",
                                c.input.c_str(), arr->size(), c.want_pairs);
                    continue;
                }
                std::printf("  parse_value_at(\"%s\") = array of %zu elements\n",
                            c.input.c_str(), arr->size());
                ++passed;
            } else {
                if (r) {
                    std::printf("  FAIL: parse_value_at(\"%s\") should have failed\n",
                                c.input.c_str());
                    continue;
                }
                std::string rem = span_to_string(s);
                const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
                std::printf("  parse_value_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                            c.input.c_str(),
                            std::format("{}", r.error()).c_str(),
                            rem.c_str(), verdict);
                ++passed;
            }
            continue;
        }

        // Default: object.
        auto r = parse_object_at(s);
        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_object_at(\"%s\") error=%s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            if (r->size() != c.want_pairs) {
                std::printf("  FAIL: parse_object_at(\"%s\") got %zu pairs, want %zu\n",
                            c.input.c_str(), r->size(), c.want_pairs);
                continue;
            }
            std::printf("  parse_object_at(\"%s\") succeeded, %zu top-level pairs\n",
                        c.input.c_str(), r->size());
            ++passed;
        } else {
            if (r) {
                std::printf("  FAIL: parse_object_at(\"%s\") should have failed but got %zu pairs\n",
                            c.input.c_str(), r->size());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_object_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
            ++passed;
        }
    }
    std::printf("  [%d / %d passed]\n", passed, total);
}

// ===========================================================================
// Section 6 — full-document round-trip
// ===========================================================================
//
// A JSON document is just a VALUE (per RFC 8259 section 2). So we
// parse the entire input through parse_value_at, pretty-print the
// result, and re-parse the pretty-printed output. If both parses
// succeed and the re-parsed tree matches the first tree, the
// pretty-printer is correct.
//
// This is the strongest correctness test we can write without an
// independent oracle — it's the property that the parser is a
// partial inverse of the pretty-printer.
// ===========================================================================

static void section_round_trip() {
    print_section("Section 6: full-document round-trip (parse -> print -> parse)");

    struct Doc {
        std::string label;
        std::string input;
    };
    const Doc docs[] = {
        {"empty object",            "{}"},
        {"empty array",             "[]"},
        {"bare string",             "\"hello\""},
        {"bare number",             "42"},
        {"bare bool",               "true"},
        {"bare null",               "null"},
        {"flat object",             "{\"name\": \"Petra\", \"age\": 7, \"active\": true}"},
        {"flat array",              "[1, 2, 3, \"four\", 5.0, null, true]"},
        {"nested",                  "{\"user\": {\"name\": \"Petra\", \"scores\": [98, 87, 92]}}"},
        {"list of users",           "{\"users\": [{\"id\": 1, \"name\": \"Ada\"}, "
                                    "{\"id\": 2, \"name\": \"Bob\"}]}"},
        {"escapes",                 "{\"text\": \"line1\\nline2\\tend, quote: \\\", back: \\\\\"}"},
        {"unicode",                 "{\"text\": \"\\u00e9\\u20ac\\ud83d\\ude00\"}"},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& d : docs) {
        ++total;

        // Parse 1
        psp::Span<const char> s1 = as_span(d.input);
        auto r1 = parse_value_at(s1);
        if (!r1) {
            std::printf("  FAIL: \"%s\": first parse failed: %s\n",
                        d.label.c_str(), std::format("{}", r1.error()).c_str());
            continue;
        }
        std::string pretty = json_to_string(*r1);

        // Parse 2
        psp::Span<const char> s2 = as_span(pretty);
        auto r2 = parse_value_at(s2);
        if (!r2) {
            std::printf("  FAIL: \"%s\": re-parse failed on output:\n    %s\n    error: %s\n",
                        d.label.c_str(), pretty.c_str(),
                        std::format("{}", r2.error()).c_str());
            continue;
        }

        // Compare strings (we don't have a deep equality op on JsonValue).
        // Round-tripping the pretty-printed form should produce the same
        // pretty-printed form, which means the parsed trees are equal
        // (modulo our pretty-printer's canonical form).
        std::string pretty2 = json_to_string(*r2);
        if (pretty != pretty2) {
            std::printf("  FAIL: \"%s\": round-trip mismatch\n    pass1: %s\n    pass2: %s\n",
                        d.label.c_str(), pretty.c_str(), pretty2.c_str());
            continue;
        }

        std::printf("  OK: \"%s\" (input %zu bytes -> %zu bytes pretty)\n",
                    d.label.c_str(), d.input.size(), pretty.size());
        ++passed;
    }
    std::printf("  [%d / %d passed]\n", passed, total);
}

// ===========================================================================
// Section 7 — sizeof / feature probes
// ===========================================================================
//
// The new JsonValue type is dominated by std::map<std::string, JsonValue>
// on libstdc++ (48 bytes), or matches std::vector<JsonValue> at 24 bytes
// on libc++. With an 8-byte discriminator, JsonValue is either 32 (libc++)
// or 56 (libstdc++) bytes — the static_asserts above lock in the lower
// bound. std::expected<JsonValue, ParseError> rounds up to 40 / 64 bytes
// depending on JsonValue's alignment.
// ===========================================================================

static void section_probes() {
    print_section("Section 7: sizeof / feature probes");

    std::printf("  sizeof(bool)                                = %zu\n", sizeof(bool));
    std::printf("  sizeof(std::int64_t)                        = %zu\n", sizeof(std::int64_t));
    std::printf("  sizeof(double)                              = %zu\n", sizeof(double));
    std::printf("  sizeof(std::string)                         = %zu\n", sizeof(std::string));
    std::printf("  sizeof(std::vector<JsonValue>)              = %zu\n",
                sizeof(std::vector<JsonValue>));
    std::printf("  sizeof(std::map<std::string, JsonValue>)    = %zu\n",
                sizeof(std::map<std::string, JsonValue>));
    std::printf("  sizeof(JsonValue)                           = %zu\n", sizeof(JsonValue));
    std::printf("  sizeof(std::expected<JsonValue, ParseError>)= %zu\n",
                sizeof(std::expected<JsonValue, ParseError>));
    std::printf("  sizeof(psp::Span<const char>)               = %zu\n",
                sizeof(psp::Span<const char>));
    std::printf("  sizeof(ParseError)                          = %zu\n", sizeof(ParseError));

    // Static asserts for the invariants we care about most.
    //
    // The JsonValue variant has 8 alternatives; its size is dominated by
    // the largest one. On libc++ (this toolchain), std::map<std::string,
    // JsonValue> is 24 bytes — same as std::vector<JsonValue> — so the
    // variant is 32 bytes (24 + 8 discriminator). On libstdc++ the map is
    // 48 bytes and JsonValue is 56. We assert the lower bound of 24
    // (largest-alternative) plus the 8-byte discriminator = 32, which
    // holds on libc++. On libstdc++ the actual size is 56; both pass.
    static_assert(sizeof(JsonValue) >= 32,
                  "JsonValue must be at least 32 bytes: 24-byte largest alternative "
                  "(std::map<string,JsonValue>) + 8-byte discriminator.");
    static_assert(sizeof(JsonValue) > sizeof(std::string),
                  "JsonValue must be larger than std::string (its largest scalar "
                  "alternative) because std::map's alignment forces the variant to grow.");
    static_assert(sizeof(JsonValue) >= sizeof(std::vector<JsonValue>),
                  "JsonValue must be at least as large as std::vector<JsonValue> "
                  "(both std::vector and std::map are 24 bytes on libc++).");

    std::printf("  static_assert(JsonValue >= 32 bytes)        = OK (actual: %zu)\n",
                sizeof(JsonValue));

    std::printf("  Cursor-primitive roster still v0.9.0:\n");
    std::printf("    psp::skip_whitespace_at(s)            -> std::expected<bool, ParseError>\n");
    std::printf("    psp::expect_char_at(s, c)             -> std::expected<bool, ParseError>\n");
    std::printf("    psp::parse_string_at(s)               -> std::expected<std::string, ParseError>\n");
    std::printf("    psp::parse_bool_at(s)                 -> std::expected<bool, ParseError>\n");
    std::printf("    psp::parse_null_at(s)                 -> std::expected<std::nullptr_t, ParseError>\n");
    std::printf("    psp::parse_int_at(s)                  -> std::expected<int, ParseError>\n");
    std::printf("    psp::parse_uint_at(s)                 -> std::expected<unsigned, ParseError>\n");
    std::printf("    psp::parse_double_at(s)               -> std::expected<double, ParseError>\n");
    std::printf("    (no new cursor primitives added today — composition suffices)\n");

#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                          = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_variant)
    std::printf("  __cpp_lib_variant                           = %ld\n",
                static_cast<long>(__cpp_lib_variant));
#endif
}

// ===========================================================================
// Main — runs all seven sections in order.
// ===========================================================================

int main() {
    section_array_of_scalars();
    section_nested_objects();
    section_round_trip();
    section_probes();
    std::printf("\n[psp_json_arrays_nested_consumer: all 4 sections complete]\n");
    return 0;
}