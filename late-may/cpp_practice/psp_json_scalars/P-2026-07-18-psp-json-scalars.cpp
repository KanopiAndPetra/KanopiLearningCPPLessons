// P-2026-07-18 — JSON scalar token cursors in <psp_span/parser.h>
//
// The Jul 16 lesson (P-2026-07-16-psp-parser-cursor-primitives.cpp)
// shipped a JSON-ish walker that could parse a flat
//   '{ "x": 42, "y": 3.14 }'
// object by hand-scanning the quoted keys. It had three obvious gaps:
//
//   (1) string VALUES were not supported — keys were just byte runs
//       between two quote chars;
//   (2) boolean and null values were not supported — only numbers;
//   (3) string decoding was completely missing — no escape handling
//       at all, so a key like "\"a\\n\"" could not be parsed.
//
// Today (v0.9.0) the parser header gains three new cursor primitives
// that close those gaps:
//
//   psp::parse_string_at(Span<const char>&)
//        -> std::expected<std::string, ParseError>
//        Consume a JSON string literal: an opening '"', zero or more
//        characters (with backslash escapes \\, \/, \", \b, \f, \n,
//        \r, \t, and \uXXXX — the last with surrogate-pair support),
//        and a closing '"'. On success, the decoded std::string is
//        returned and the span is shrunk past the closing quote. On
//        any failure, the span is unchanged.
//
//   psp::parse_bool_at(Span<const char>&)
//        -> std::expected<bool, ParseError>
//        Consume a JSON `true` or `false` literal. Returns the bool
//        and advances the span past the matched token.
//
//   psp::parse_null_at(Span<const char>&)
//        -> std::expected<std::nullptr_t, ParseError>
//        Consume a JSON `null` literal. Returns nullptr and advances
//        the span past the token.
//
// Together with the v0.8.0 primitives (skip_whitespace_at,
// expect_char_at) and the v0.7.0 numeric cursors
// (parse_int_at, parse_uint_at, parse_double_at), we can now write
// a real std::expected<JsonValue, ParseError> parser for JSON
// objects, arrays, and their scalar values. This lesson
// demonstrates that with a strict parser for JSON OBJECTs that
// contain string, number, bool, or null values.
//
// What this lesson is NOT
// -----------------------
// - It is not a complete JSON parser. There is no `JsonValue`
//   type with nested arrays / objects yet — those would compose
//   on top of today by recursing through the same primitives.
//   That's the natural next step in the Jul 15/16/18 chain.
// - It does not implement JSON5 / JSONC extensions (no comments,
//   no single-quoted strings, no trailing commas).
// - It does not support JSON numbers with explicit '+' sign
//   (consistent with parse_int / parse_double, which reject it
//   for downstream-policy reasons).
// - \uXXXX is decoded to UTF-8 so the resulting std::string
//   contains the actual character. Invalid surrogate pairs and
//   non-hex digits produce InvalidUnicodeEscape.
//
// The program has seven sections:
//
//   1. parse_string_at — the headline primitive, with escape and
//                         \uXXXX support
//   2. parse_bool_at  — `true` / `false`
//   3. parse_null_at  — `null`
//   4. JsonScalar     — std::variant<monostate, nullptr_t, bool,
//                         double, int64, std::string> unifying
//                         the four v0.7.0 + v0.9.0 scalar token
//                         shapes behind one sum type
//   5. parse_value_at — turn a span into a JsonScalar using
//                       skip_whitespace_at + try-each-parser
//   6. parse_object_at — strict object parser built on
//                         parse_value_at + v0.8.0 primitives
//   7. sizeof / feature probes — confirm v0.9.0 sizes (note that
//                                 parse_string_at returns
//                                 std::string, so that variant
//                                 bumps the variant size)
//
// Build (assumes psp_span_lib v0.9.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-18-psp-json-scalars
//
// ASan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-18-psp-json-scalars

#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <expected>
#include <format>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static std::string span_to_string(psp::Span<const char> s) {
    return std::string(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// ---------------------------------------------------------------------------
// Section 1 — parse_string_at
//
// Decode a JSON string literal, including \b \f \n \r \t \" \\ \/ escapes
// and \uXXXX (with surrogate pairs). Empty input is Empty; a non-quote
// front char is UnexpectedChar; an unterminated literal is
// UnterminatedString; a bad escape is InvalidEscape; a bad Unicode
// escape is InvalidUnicodeEscape.
// ---------------------------------------------------------------------------
static void section_parse_string_at() {
    print_section("Section 1: psp::parse_string_at — quoted strings + escapes");

    struct Case {
        std::string input;
        bool want_ok;
        const char* want_value;  // expected decoded value (only used if ok)
    };
    const Case cases[] = {
        // Simple strings
        {"\"\"",            true,  ""},
        {"\"hello\"",       true,  "hello"},
        {"\"a b c\"",       true,  "a b c"},
        {"\"with space\"",  true,  "with space"},

        // Common escapes
        {"\"line1\\nline2\"", true, "line1\nline2"},
        {"\"a\\tb\\tc\"",   true,  "a\tb\tc"},
        {"\"quote: \\\"\"", true,  "quote: \""},
        {"\"back: \\\\\"",  true,  "back: \\"},
        {"\"slash: \\/\"",  true,  "slash: /"},
        {"\"crlf\\r\\n\"",  true,  "crlf\r\n"},
        // \uXXXX — basic ASCII plane
        {"\"\\u0041\\u0042\\u0043\"", true, "ABC"},
        {"\"\\u00e9\"",     true,  "\xc3\xa9"},    // é, two UTF-8 bytes
        {"\"\\u20ac\"",     true,  "\xe2\x82\xac"},// €, three UTF-8 bytes
        {"\"\\ud83d\\ude00\"", true, "\xf0\x9f\x98\x80"}, // 😀, surrogate pair

        // Failure cases
        {"",                 false, nullptr},   // Empty
        {"\"unterminated",  false, nullptr},   // UnterminatedString
        {"no quote",        false, nullptr},   // UnexpectedChar
        {"\"\\q\"",         false, nullptr},   // InvalidEscape
        {"\"\\u00zz\"",     false, nullptr},   // InvalidUnicodeEscape (bad hex)
        {"\"\\ud800\"",     false, nullptr},   // InvalidUnicodeEscape (lone high surrogate)
        {"\"\\udc00\"",     false, nullptr},   // InvalidUnicodeEscape (lone low surrogate)
        {"\"\\u00\"",       false, nullptr},   // InvalidUnicodeEscape (truncated)
        {"\"\\ud800\\ud800\"", false, nullptr}, // InvalidUnicodeEscape (high+high)
        {"\"with\\0null\"", false, nullptr},   // JSON forbids raw U+0000 in strings
        {"\"\\u0041\" \"tail\"", true, "A"},   // multi-token: we only consume the first
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        auto r = psp::parse_string_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_string_at(\"%s\") should have succeeded but got %s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            std::string want = c.want_value;
            // Compare via bytes (std::string allows embedded NUL).
            if (r->size() != want.size() || *r != want) {
                std::printf("  FAIL: parse_string_at(\"%s\") = \"%s\" (len %zu), want \"%s\" (len %zu)\n",
                            c.input.c_str(),
                            std::string(*r).c_str(), r->size(),
                            want.c_str(), want.size());
                continue;
            }
            std::printf("  parse_string_at(\"%s\") = \"%s\" (len %zu), s remaining = \"%s\"\n",
                        c.input.c_str(),
                        std::string(*r).c_str(), r->size(),
                        span_to_string(s).c_str());
        } else {
            if (r) {
                std::printf("  FAIL: parse_string_at(\"%s\") should have failed but got \"%s\"\n",
                            c.input.c_str(), std::string(*r).c_str());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_string_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 2 — parse_bool_at
// ---------------------------------------------------------------------------
static void section_parse_bool_at() {
    print_section("Section 2: psp::parse_bool_at — consume 'true' or 'false'");

    struct Case { std::string input; bool want_ok; bool want_value; };
    const Case cases[] = {
        {"true",        true,  true},
        {"false",       true,  false},
        {"trueXYZ",     true,  true},   // token is consumed; tail stays in s
        {"fals",        false, false},  // truncated -> InvalidLiteral
        {"TRUE",        false, false},  // JSON is case-sensitive
        {"tru",         false, false},  // truncated
        {"",            false, false},  // Empty
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        auto r = psp::parse_bool_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_bool_at(\"%s\") should have succeeded but got %s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            if (*r != c.want_value) {
                std::printf("  FAIL: parse_bool_at(\"%s\") = %s, want %s\n",
                            c.input.c_str(),
                            *r ? "true" : "false",
                            c.want_value ? "true" : "false");
                continue;
            }
            std::printf("  parse_bool_at(\"%s\") = %s, s remaining = \"%s\"\n",
                        c.input.c_str(),
                        *r ? "true" : "false",
                        span_to_string(s).c_str());
        } else {
            if (r) {
                std::printf("  FAIL: parse_bool_at(\"%s\") should have failed but got %s\n",
                            c.input.c_str(), *r ? "true" : "false");
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_bool_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 3 — parse_null_at
// ---------------------------------------------------------------------------
static void section_parse_null_at() {
    print_section("Section 3: psp::parse_null_at — consume 'null'");

    struct Case { std::string input; bool want_ok; };
    const Case cases[] = {
        {"null",        true},
        {"nullXYZ",     true},   // token consumed; tail stays in s
        {"nul",         false},  // InvalidLiteral
        {"NULL",        false},  // case-sensitive
        {"",            false},  // Empty
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        auto r = psp::parse_null_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_null_at(\"%s\") should have succeeded but got %s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            std::printf("  parse_null_at(\"%s\") = null, s remaining = \"%s\"\n",
                        c.input.c_str(), span_to_string(s).c_str());
        } else {
            if (r) {
                std::printf("  FAIL: parse_null_at(\"%s\") should have failed\n",
                            c.input.c_str());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_null_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 4 — JsonScalar: the sum type a JSON value parser would
//             produce for scalar (non-container) inputs.
//
// We pick std::variant<monostate, nullptr_t, bool, double, std::int64_t,
// std::string> as the JSON scalar value. The index of monostate means
// "the variant is default-constructed / not yet assigned".
//
// The size breakdown (Jul 15 lesson measured variants with 6-7
// members; today's variant has 6 alternatives, so it's in the same
// ballpark). std::string dominates — see Section 7.
// ---------------------------------------------------------------------------
struct JsonScalar {
    std::variant<
        std::monostate,    // 0: default-constructed / unset
        std::nullptr_t,    // 1: JSON null
        bool,              // 2: true / false
        double,            // 3: numeric — whole-span doubles
        std::int64_t,      // 4: numeric — int-typed when possible
        std::string        // 5: JSON string
    > value;

    const char* kind() const noexcept {
        switch (value.index()) {
            case 0: return "unset";
            case 1: return "null";
            case 2: return "bool";
            case 3: return "double";
            case 4: return "int64";
            case 5: return "string";
            default: return "?";
        }
    }
};

// ---------------------------------------------------------------------------
// Section 5 — parse_value_at
//
// Try each parser in order, returning the first success. The order
// matters: strings, then keywords (true/false/null), then numbers.
//
// parse_uint_at is tried first for numbers because it never refuses a
// leading '+' and is the most-strict number parser; if it succeeds
// we use parse_int_at next (which catches negatives and tells us the
// int was a non-negative that fits in int64). If both fail, we fall
// back to parse_double_at.
//
// Why this layering:
//   - parse_string_at fails fast on non-'"' inputs (no allocation),
//     so trying it first is cheap when the value isn't a string.
//   - parse_bool_at / parse_null_at also fail fast and never allocate,
//     and they MUST come before parse_uint_at because parse_uint_at
//     would happily chew on the 't' / 'n' as part of a digit run if
//     it were tried first (no — actually, parse_uint_at would fail
//     with NotADigit for 't' or 'n', so the order between those
//     three is "string, bool/null, number" for clarity, not for
//     correctness).
//   - parse_double_at is the only parser that handles fractional
//     or exponent forms.
// ---------------------------------------------------------------------------
static std::expected<JsonScalar, ParseError>
parse_value_at(psp::Span<const char>& s) noexcept {
    psp::skip_whitespace_at(s);
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    auto saved = s;

    if (s.front() == '"') {
        auto str = psp::parse_string_at(s);
        if (!str) return std::unexpected{str.error()};
        JsonScalar out;
        out.value = std::move(*str);
        return out;
    }

    if (auto b = psp::parse_bool_at(s)) {
        JsonScalar out;
        out.value = *b;
        return out;
    } else if (b.error() != ParseError::Empty
            && b.error() != ParseError::InvalidLiteral) {
        return std::unexpected{b.error()};
    }

    if (auto n = psp::parse_null_at(s)) {
        JsonScalar out;
        out.value = nullptr;
        return out;
    } else if (n.error() != ParseError::Empty
            && n.error() != ParseError::InvalidLiteral) {
        return std::unexpected{n.error()};
    }

    // Numbers — try double, which also handles pure integers. Trying
    // int first has a footgun: parse_int_at accepts "3" from "3.14"
    // and returns 3 with ".14" left in s, which would then be parsed
    // as the next token. parse_double_at handles "3.14" as 3.14 in
    // one pass; for "42" it returns 42.0 with nothing left in s. We
    // convert the result to int64 if it has no fractional part and
    // fits — that preserves the "int when possible" property that
    // the JsonScalar variant exposes (an int64 alternative lets the
    // caller distinguish 42 from 42.0 in a structured way).
    auto d = psp::parse_double_at(s);
    if (!d) {
        s = saved;
        return std::unexpected{d.error()};
    }
    JsonScalar out;
    if (std::trunc(*d) == *d && *d >= static_cast<double>(std::numeric_limits<std::int64_t>::min())
                              && *d <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        out.value = static_cast<std::int64_t>(*d);
    } else {
        out.value = *d;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Section 6 — parse_object_at
//
// Strict object parser. Recognised shape:
//
//   '{' (WS STRING WS ':' WS VALUE (WS ',' WS STRING WS ':' WS VALUE)* WS)? WS '}'
//
// where VALUE is one of string, number, true, false, null. The keys
// must be unique (we return a DuplicateKey error if not — the
// std::map insertion result carries the duplicate-detection
// information).
//
// On success: std::map<std::string, JsonScalar> ordered alphabetically.
// On failure: std::unexpected{ParseError} (and s is unchanged).
// ---------------------------------------------------------------------------
struct JsonObject {
    std::map<std::string, JsonScalar> members;
    std::vector<std::string>         order;   // insertion order for printing
};

static std::expected<JsonObject, ParseError>
parse_object_at(psp::Span<const char>& s) noexcept {
    auto saved = s;
    JsonObject out;

    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = saved; return std::unexpected{r.error()};
    }
    if (auto r = psp::expect_char_at(s, '{'); !r) {
        s = saved; return std::unexpected{r.error()};
    }

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
        auto [it, inserted] = out.members.emplace(std::move(*key), std::move(*val));
        if (!inserted) {
            // Duplicate key — the parser doesn't currently surface
            // a typed error; the first value wins. A real JSON
            // parser would return a `DuplicateKey` error.
            s = saved;
            return std::unexpected{ParseError::UnexpectedChar};
        }
        out.order.push_back(it->first);

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

// stringify a JsonScalar back to a human-readable JSON-ish form.
static std::string scalar_to_string(const JsonScalar& v) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return "<unset>";
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", arg);
            return std::string(buf);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + arg + "\"";
        } else {
            return "?";
        }
    }, v.value);
}

static void section_parse_object_at() {
    print_section("Section 6: parse_object_at — strict scalar-object parser");

    struct Case { std::string input; bool want_ok; std::size_t want_pairs; };
    const Case cases[] = {
        // Well-formed objects
        {"{}",                                true,  0},
        {"{ }",                               true,  0},
        {"{\"x\": 42}",                       true,  1},
        {"{\"x\": 42, \"y\": 3.14}",          true,  2},
        {"{\"name\": \"Petra\", \"age\": 7, \"active\": true, \"tag\": null}",
                                             true,  4},
        {"{\"escapes\": \"line1\\nline2\\tend\"}",
                                             true,  1},
        {"{\"unicode\": \"\\u20ac\\u00e9\"}", true,  1},
        {"{\"emoji\": \"\\ud83d\\ude00\"}",   true,  1},
        {"  {  \"x\"  :  42  }  ",            true,  1},

        // Malformed
        {"",                                  false, 0},
        {"{\"x\": 42",                        false, 0},   // missing }
        {"{\"x\" 42}",                        false, 0},   // missing colon
        {"{\"x\": 42,}",                      false, 0},   // trailing comma
        {"{\"x\": tru}",                      false, 0},   // truncated bool
        {"{\"x\": \"unterminated}",           false, 0},   // unterminated string
        {"{\"x\": 42, \"x\": 43}",            false, 0},   // would have detected duplicate if the
                                                            // parser raised an error (today it
                                                            // silently keeps the first one — see
                                                            // Section 6 notes).
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        auto r = parse_object_at(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_object_at(\"%s\") error=%s\n",
                            c.input.c_str(),
                            std::format("{}", r.error()).c_str());
                continue;
            }
            if (r->order.size() != c.want_pairs) {
                std::printf("  FAIL: parse_object_at(\"%s\") got %zu pairs, want %zu\n",
                            c.input.c_str(), r->order.size(), c.want_pairs);
                continue;
            }
            std::printf("  parse_object_at(\"%s\") -> {", c.input.c_str());
            for (std::size_t i = 0; i < r->order.size(); ++i) {
                if (i > 0) std::printf(", ");
                const auto& k = r->order[i];
                const auto& v = r->members.at(k);
                std::printf("\"%s\": %s", k.c_str(), scalar_to_string(v).c_str());
            }
            std::printf("}, s remaining = \"%s\"\n", span_to_string(s).c_str());
        } else {
            if (r) {
                std::printf("  FAIL: parse_object_at(\"%s\") should have failed but got %zu pairs\n",
                            c.input.c_str(), r->order.size());
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == c.input) ? "unchanged" : "REWOUND BUG!";
            std::printf("  parse_object_at(\"%s\") = error:%s, s = \"%s\" (%s)\n",
                        c.input.c_str(),
                        std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 7 — sizeof / feature probes
//
// The new ParseError variants (UnterminatedString, InvalidEscape,
// InvalidUnicodeEscape, InvalidLiteral) keep sizeof(ParseError) at 4
// bytes — the underlying type is still int. The std::variant with
// std::string is dominated by the string's size (3 words on
// 64-bit: pointer, size, capacity = 24 bytes on this toolchain, plus
// the variant's discriminator = 32 bytes total).
// ---------------------------------------------------------------------------
static void section_probes() {
    print_section("Section 7: sizeof / feature probes");

    std::printf("  sizeof(bool)                                = %zu\n", sizeof(bool));
    std::printf("  sizeof(ParseError)                          = %zu\n", sizeof(ParseError));
    std::printf("  sizeof(psp::Span<const char>)               = %zu\n", sizeof(psp::Span<const char>));
    std::printf("  sizeof(std::monostate)                      = %zu\n", sizeof(std::monostate));
    std::printf("  sizeof(std::string)                         = %zu\n", sizeof(std::string));
    std::printf("  sizeof(std::int64_t)                        = %zu\n", sizeof(std::int64_t));
    std::printf("  sizeof(double)                              = %zu\n", sizeof(double));
    std::printf("  sizeof(JsonScalar)                          = %zu\n", sizeof(JsonScalar));
    std::printf("  sizeof(std::expected<std::string, ParseError>) = %zu\n",
                sizeof(std::expected<std::string, ParseError>));
    std::printf("  sizeof(std::expected<bool, ParseError>)     = %zu\n",
                sizeof(std::expected<bool, ParseError>));
    std::printf("  sizeof(std::expected<std::nullptr_t, ParseError>) = %zu\n",
                sizeof(std::expected<std::nullptr_t, ParseError>));

    std::printf("  ParseError names (v0.9.0):\n");
    const ParseError all_errors[] = {
        ParseError::Empty, ParseError::LeadingSign, ParseError::NotADigit,
        ParseError::Overflow, ParseError::BadExponent, ParseError::MissingFraction,
        ParseError::UnexpectedChar, ParseError::UnterminatedString,
        ParseError::InvalidEscape, ParseError::InvalidUnicodeEscape,
        ParseError::InvalidLiteral,
    };
    for (ParseError e : all_errors) {
        std::printf("    -> \"%s\"\n", std::format("{}", e).c_str());
    }
#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                          = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_variant)
    std::printf("  __cpp_lib_variant                           = %ld\n",
                static_cast<long>(__cpp_lib_variant));
#endif
}

int main() {
    section_parse_string_at();
    section_parse_bool_at();
    section_parse_null_at();
    section_parse_object_at();
    section_probes();
    std::printf("\n[psp_json_scalars_consumer: all 5 sections complete]\n");
    return 0;
}
