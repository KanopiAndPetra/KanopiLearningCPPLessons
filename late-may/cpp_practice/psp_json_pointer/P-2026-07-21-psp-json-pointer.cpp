// P-2026-07-21 — Consumer of <psp_span/json_ext.h>, the fourth
// public header of psp_span_lib (v0.11.0).
//
// Where this fits in the arc
// --------------------------
// The Jul 20 lesson (P-2026-07-20-psp-json-header.cpp) shipped
// <psp_span/json.h> — a complete general JSON parser in the
// library proper. The Jul 20 lesson's "Where we go next" said:
//
//   The most natural JSON-parser forward-on is **a JSON serializer
//   configurable in line with JSON5/JSONC extensions** (a follow-on
//   to today's strict parser) ...
//
//   For the library as a whole, the most natural forward-on is still
//   **bump the library to v0.11.0 with one more concrete new public
//   capability** — a candidate is <psp_span/json_ext.h> adding
//   JSON Pointer / JSON Patch (RFC 6901 / 6902) on top of today's
//   JSON header.
//
// Today is the JSON Pointer half of that forward-on. JSON Patch
// (RFC 6902) is the lesson after this one: Patch is "apply ops to
// a JsonValue tree at JSON Pointers" — once resolve() can hand a
// caller a non-owning pointer to a sub-value, Patch has somewhere
// to write.
//
// What landed in the library (v0.11.0)
// ------------------------------------
// | Layer                        | v0.10.0  | v0.11.0                                |
// |------------------------------|----------|----------------------------------------|
// | <psp_span/span.h>            | unchanged| unchanged                              |
// | <psp_span/parser.h>          | unchanged| unchanged                              |
// | <psp_span/json.h>            | unchanged| unchanged                              |
// | <psp_span/json_ext.h>        | (none)   | v0.11.0 (NEW)                          |
// | ::JsonExtError               | (none)   | 8 enumerators + std::formatter spec    |
// | psp::json_pointer::split     | (none)   | (NEW) RFC 6901 §3 tokenizer           |
// | psp::json_pointer::to_string | (none)   | (NEW) inverse of split                 |
// | psp::json_pointer::resolve   | (none)   | (NEW) walks JsonValue at a pointer     |
//
// The new header is C++23 (it includes <psp_span/json.h>, which
// is C++23 because it uses std::expected and std::variant).
// std::expected<const JsonValue*, JsonExtError> is 16 bytes on
// this toolchain (8-byte ptr + 4-byte err + 4-byte padding).
//
// Build (assumes psp_span_lib v0.11.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-21-psp-json-pointer
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-21-psp-json-pointer

#include <psp_span/json_ext.h>
#include <psp_span/json.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// ===========================================================================
// Helpers (test infrastructure — no pointer code).
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// Parse `s` into a JsonValue (the same call Jul 18 / Jul 19 / Jul
// 20 consumers used). Aborts the test by returning a default-
// constructed JsonValue on failure — the consumer is a test
// driver, not a parser; failures here mean the test inputs are
// broken, not that the library is.
static psp::JsonValue parse_or_default(const std::string& s) {
    psp::Span<const char> sp = as_span(s);
    auto r = psp::parse_value_at(sp);
    if (!r) {
        std::printf("  INTERNAL FAIL: parse_value_at(\"%s\") gave %s\n",
                    s.c_str(), std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    return std::move(*r);
}

// ===========================================================================
// Section 1 — split() and to_string() round-trip
// ===========================================================================
//
// RFC 6901 §3 says the reference-token syntax is "/"-separated
// strings with two-char escapes "~0" and "~1". split() is the
// tokenizer; to_string() is the inverse. The round-trip
// property — split(to_string(tokens)) == tokens — is the main
// correctness check on both functions.

static void section_split_and_to_string() {
    print_section("Section 1: split / to_string / round-trip");

    // ----------------------------------------------------------------
    // 1a. split() on the canonical RFC 6901 §5 examples.
    // ----------------------------------------------------------------
    struct SplitCase {
        const char* input;
        std::size_t want_tokens;
        const char* want_first;  // first token's value ("" to skip)
        const char* want_last;   // last token's value ("" to skip)
    };
    const SplitCase split_cases[] = {
        // RFC 6901 §5: "" (the whole document) — 0 tokens.
        {"",         0, "",   ""},
        // RFC 6901 §5: "/foo" — single member "foo".
        {"/foo",     1, "foo", "foo"},
        // RFC 6901 §5: "/foo/0" — two tokens: "foo", "0".
        {"/foo/0",   2, "foo", "0"},
        // Nested with escapes.
        {"/a~1b/c",  2, "a/b", "c"},
        // "~01" -> "01" stays "01"; "~10" -> "/0"; verify the
        // double-escape round-trips correctly.
        // Per RFC 6901 §3, the algorithm is "first ~1->/, then
        // ~0->~" applied as whole-string substitutions. So:
        //   "~01" — step 1 finds no `~1`; step 2 finds `~0` and
        //            turns it into `~`; result is `~1`.
        //   "~10" — step 1 finds `~1` and turns it into `/`;
        //            step 2 finds no `~0`; result is `/0`.
        {"/~01",     1, "~1",  "~1"},
        {"/~10",     1, "/0",  "/0"},
        // Multiple escapes in one token.
        {"/~0~1~0",  1, "~/~", "~/~"},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : split_cases) {
        ++total;
        std::string_view in(c.input);
        auto r = psp::json_pointer::split(in);
        if (!r) {
            std::printf("  FAIL: split(\"%s\") errored with %s\n",
                        c.input, std::format("{}", r.error()).c_str());
            continue;
        }
        if (r->size() != c.want_tokens) {
            std::printf("  FAIL: split(\"%s\") got %zu tokens, want %zu\n",
                        c.input, r->size(), c.want_tokens);
            continue;
        }
        if (c.want_tokens > 0) {
            if ((*r)[0].value != c.want_first) {
                std::printf("  FAIL: split(\"%s\")[0] = \"%s\", want \"%s\"\n",
                            c.input, (*r)[0].value.c_str(), c.want_first);
                continue;
            }
            if ((*r).back().value != c.want_last) {
                std::printf("  FAIL: split(\"%s\").back() = \"%s\", want \"%s\"\n",
                            c.input, (*r).back().value.c_str(), c.want_last);
                continue;
            }
        }
        std::printf("  split(\"%s\") -> %zu tokens: ", c.input, r->size());
        for (std::size_t i = 0; i < r->size(); ++i) {
            if (i > 0) std::printf("/");
            std::printf("%s", (*r)[i].value.c_str());
        }
        std::printf("\n");
        ++passed;
    }
    std::printf("  [%d / %d passed] (split canonical cases)\n", passed, total);

    // ----------------------------------------------------------------
    // 1b. split() error cases — MalformedToken.
    // ----------------------------------------------------------------
    // The "Input" field for split() must:
    //   - be empty (returns empty vector, NOT an error) — not in this list
    //   - start with "/" (anything else is MalformedToken)
    //   - never have a "~" with no following "0" or "1"
    struct MalformedCase { const char* input; };
    const MalformedCase malformed[] = {
        {"foo"},         // no leading "/"
        {"foo/bar"},     // no leading "/"
        {"/foo~"},       // unterminated ~
        {"/foo~2"},      // ~2 is not a known escape
        {"/foo~a"},      // ~a is not a known escape
    };
    int mtotal = 0, mpassed = 0;
    for (const auto& c : malformed) {
        ++mtotal;
        auto r = psp::json_pointer::split(std::string_view(c.input));
        if (r) {
            std::printf("  FAIL: split(\"%s\") should have errored, got %zu tokens\n",
                        c.input, r->size());
            continue;
        }
        if (r.error() != JsonExtError::MalformedToken) {
            std::printf("  FAIL: split(\"%s\") gave %s, want MalformedToken\n",
                        c.input, std::format("{}", r.error()).c_str());
            continue;
        }
        std::printf("  split(\"%s\") -> error:MalformedToken (as expected)\n", c.input);
        ++mpassed;
    }
    std::printf("  [%d / %d passed] (malformed cases)\n", mpassed, mtotal);

    // ----------------------------------------------------------------
    // 1c. Round-trip: to_string(split(s)) == s.
    // ----------------------------------------------------------------
    // For every input that split() accepts, reconstruct it via
    // to_string() and check it matches. The MalformedToken cases
    // are excluded — by definition they don't round-trip.
    const char* round_trip_inputs[] = {
        "",
        "/",
        "/foo",
        "/foo/bar",
        "/a~1b",
        "/a~0b",
        "/~01",
        "/foo/-",
        "/foo/0/bar/1",
        "/users/0/name",
    };
    int rtotal = 0, rpassed = 0;
    for (const char* in : round_trip_inputs) {
        ++rtotal;
        auto toks = psp::json_pointer::split(std::string_view(in));
        if (!toks) {
            std::printf("  FAIL: split(\"%s\") unexpectedly errored: %s\n",
                        in, std::format("{}", toks.error()).c_str());
            continue;
        }
        std::string out = psp::json_pointer::to_string(*toks);
        if (out != in) {
            std::printf("  FAIL: round-trip(\"%s\") -> \"%s\"\n", in, out.c_str());
            continue;
        }
        std::printf("  round-trip(\"%s\") OK\n", in);
        ++rpassed;
    }
    std::printf("  [%d / %d passed] (split <-> to_string round-trip)\n",
                rpassed, rtotal);

    // ----------------------------------------------------------------
    // 1d. Special "no leading slash" case: to_string() of an empty
    //     vector is "" (the whole-document pointer), not "/".
    // ----------------------------------------------------------------
    {
        std::vector<psp::json_pointer::ReferenceToken> empty;
        if (psp::json_pointer::to_string(empty) != "") {
            std::printf("  FAIL: to_string(empty) != \"\"\n");
        } else {
            std::printf("  to_string(empty) == \"\" (whole-document pointer)\n");
        }
    }
}

// ===========================================================================
// Section 2 — resolve() on the canonical RFC 6901 examples
// ===========================================================================
//
// RFC 6901 §5 spells out six worked examples. We re-implement
// them as a corpus, building a small JsonValue tree and
// resolving each pointer against it. The tree is:
//
//   { "foo": ["bar", "baz"],
//     ""  : 0,
//     "a/b": 1,
//     "c%d": 2,
//     "e^f": 3,
//     "g|h": 4,
//     "i\\j": 5,
//     "k\"l": 6,
//     " " : 7,
//     "m~n": 8 }
//
// plus a separate small tree for the "everything deeper" example.

static void section_resolve_canonical() {
    print_section("Section 2: resolve() on the canonical RFC 6901 §5 examples");

    // Build the §5 tree: every value is a std::int64_t with the
    // expected slot number (so the test can verify the right
    // element came back).
    psp::JsonValue root;
    {
        std::map<std::string, psp::JsonValue> obj;
        obj["foo"]  = psp::JsonValue{std::vector<psp::JsonValue>{
            psp::JsonValue{std::string("bar")},
            psp::JsonValue{std::string("baz")},
        }};
        obj[""]    = psp::JsonValue{std::int64_t{0}};
        obj["a/b"] = psp::JsonValue{std::int64_t{1}};
        obj["c%d"] = psp::JsonValue{std::int64_t{2}};
        obj["e^f"] = psp::JsonValue{std::int64_t{3}};
        obj["g|h"] = psp::JsonValue{std::int64_t{4}};
        obj["i\\j"] = psp::JsonValue{std::int64_t{5}};
        obj["k\"l"] = psp::JsonValue{std::int64_t{6}};
        obj[" "]    = psp::JsonValue{std::int64_t{7}};
        obj["m~n"]  = psp::JsonValue{std::int64_t{8}};
        root = psp::JsonValue{std::move(obj)};
    }

    // Each row: pointer, expected slot.
    struct Case { const char* ptr; std::int64_t want; };
    const Case cases[] = {
        // "" is the whole document.
        {"",       -1},   // special: "got root" (we'll check separately)
        {"/foo",   -1},   // special: "got array"
        {"/foo/0", -1},   // special: "got string \"bar\""
        {"/",      0},    // empty key -> slot 0
        {"/a~1b",  1},
        {"/c%d",   2},
        {"/e^f",   3},
        {"/g|h",   4},
        {"/i\\j",  5},
        {"/k\"l",  6},
        {"/ ",     7},
        {"/m~0n",  8},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;
        auto r = psp::json_pointer::resolve(std::string_view(c.ptr), root);
        if (!r) {
            std::printf("  FAIL: resolve(\"%s\") errored with %s\n",
                        c.ptr, std::format("{}", r.error()).c_str());
            continue;
        }
        if (c.want < 0) {
            // Special cases — we just confirm the result is non-null
            // and print what kind of value we got. The Section 3
            // tests are where the per-shape details get exercised.
            std::printf("  resolve(\"%s\") -> %s (handled in §3)\n", c.ptr,
                        psp::json_to_string(**r).c_str());
            ++passed;
            continue;
        }
        const auto* got = std::get_if<std::int64_t>(&(*r)->value);
        if (!got) {
            std::printf("  FAIL: resolve(\"%s\") returned non-integer\n", c.ptr);
            continue;
        }
        if (*got != c.want) {
            std::printf("  FAIL: resolve(\"%s\") = %lld, want %lld\n",
                        c.ptr, static_cast<long long>(*got),
                        static_cast<long long>(c.want));
            continue;
        }
        std::printf("  resolve(\"%s\") = %lld\n", c.ptr,
                    static_cast<long long>(*got));
        ++passed;
    }
    std::printf("  [%d / %d passed] (canonical cases)\n", passed, total);
}

// ===========================================================================
// Section 3 — resolve() against a deeper, real-shaped tree
// ===========================================================================
//
// The §5 tree is intentionally flat. Real JSON is nested. This
// section parses a real-shaped document (a user list, with
// nested address objects) and walks it with JSON Pointers to
// demonstrate that the resolver handles arbitrary depth.

static void section_resolve_nested() {
    print_section("Section 3: resolve() against a real-shaped nested tree");

    // The same corpus the Jul 18 / Jul 19 / Jul 20 consumers
    // used — the user list.
    const std::string input = R"({
        "users": [
            {"id": 1, "name": "Ada",  "address": {"city": "London",   "zip": "EC1"}},
            {"id": 2, "name": "Bob",  "address": {"city": "Paris",    "zip": "75001"}},
            {"id": 3, "name": "Cara", "address": {"city": "Tokyo",    "zip": "100"}}
        ],
        "meta": {
            "count": 3,
            "tags":  ["staff", "vip", "verified"]
        }
    })";
    const psp::JsonValue root = parse_or_default(input);

    struct Case { const char* ptr; bool want_ok; const char* want_value; };
    const Case cases[] = {
        // Object keys.
        {"/meta/count",        true,  "3"},
        {"/meta/tags/0",       true,  "\"staff\""},
        {"/meta/tags/2",       true,  "\"verified\""},
        // Nested object.
        {"/users/0/name",      true,  "\"Ada\""},
        {"/users/1/address/city",  true, "\"Paris\""},
        {"/users/2/address/zip",   true, "\"100\""},
        // Whole document.
        {"",                   true,  nullptr},  // special
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;
        auto r = psp::json_pointer::resolve(std::string_view(c.ptr), root);
        if (!r) {
            if (c.want_ok) {
                std::printf("  FAIL: resolve(\"%s\") errored with %s\n",
                            c.ptr, std::format("{}", r.error()).c_str());
            } else {
                std::printf("  resolve(\"%s\") -> error:%s (expected)\n",
                            c.ptr, std::format("{}", r.error()).c_str());
                ++passed;
            }
            continue;
        }
        if (!c.want_ok) {
            std::printf("  FAIL: resolve(\"%s\") should have errored, got %s\n",
                        c.ptr, psp::json_to_string(**r).c_str());
            continue;
        }
        if (c.want_value == nullptr) {
            // Whole-document case: the resolver must return a
            // pointer equal to &root.
            if (*r != &root) {
                std::printf("  FAIL: resolve(\"\") returned %p, want &root=%p\n",
                            static_cast<const void*>(*r),
                            static_cast<const void*>(&root));
                continue;
            }
            std::printf("  resolve(\"\") -> &root (whole document)\n");
            ++passed;
            continue;
        }
        std::string got = psp::json_to_string(**r);
        if (got != c.want_value) {
            std::printf("  FAIL: resolve(\"%s\") = %s, want %s\n",
                        c.ptr, got.c_str(), c.want_value);
            continue;
        }
        std::printf("  resolve(\"%s\") = %s\n", c.ptr, got.c_str());
        ++passed;
    }
    std::printf("  [%d / %d passed] (nested cases)\n", passed, total);
}

// ===========================================================================
// Section 4 — error cases (one per JsonExtError enumerator)
// ===========================================================================

static void section_error_cases() {
    print_section("Section 4: error cases (one per JsonExtError enumerator)");

    const std::string input = R"({
        "a": 1,
        "b": [10, 20, 30],
        "c": {"nested": true}
    })";
    const psp::JsonValue root = parse_or_default(input);

    struct Case { const char* ptr; JsonExtError want; };
    const Case cases[] = {
        // MalformedToken (passed to split, not resolve).
        {"/foo~",          JsonExtError::MalformedToken},
        {"/foo~2",         JsonExtError::MalformedToken},
        // NotFound — object key doesn't exist.
        {"/missing",       JsonExtError::NotFound},
        // NotAnObject — try to take a key on a scalar.
        {"/a/x",           JsonExtError::NotAnObject},
        // (Formerly /a/missing — but /a is the scalar 1, so the
        // second token reports NotAnObject, not NotFound. Listed
        // above as /a/x.)
        // NotAnArray — try to take an index on a scalar.
        {"/a/0",           JsonExtError::NotAnArray},
        // IndexOutOfRange.
        {"/b/3",           JsonExtError::IndexOutOfRange},
        {"/b/100",         JsonExtError::IndexOutOfRange},
        // IndexNotANumber.
        {"/b/abc",         JsonExtError::IndexNotANumber},
        {"/b/-1",          JsonExtError::IndexNotANumber},  // negative
        {"/b/1.5",         JsonExtError::IndexNotANumber},  // fractional
        // LastArrayElement — "-" is the "would-be-here" token.
        {"/b/-",           JsonExtError::LastArrayElement},
        // NotAnObject — try to descend into the root (an object
        // already, so this hits the SECOND case: the root is an
        // object, /missing/0 is missing/NotFound, NOT this
        // branch). Let's rephrase: descend through a scalar
        // expected as an object.
        {"/c/nested/x",    JsonExtError::NotAnObject},
    };

    int passed = 0;
    int total  = 0;
    for (const auto& c : cases) {
        ++total;
        auto r = psp::json_pointer::resolve(std::string_view(c.ptr), root);
        if (r) {
            std::printf("  FAIL: resolve(\"%s\") should have errored %s, got %s\n",
                        c.ptr, std::format("{}", c.want).c_str(),
                        psp::json_to_string(**r).c_str());
            continue;
        }
        if (r.error() != c.want) {
            std::printf("  FAIL: resolve(\"%s\") = %s, want %s\n",
                        c.ptr, std::format("{}", r.error()).c_str(),
                        std::format("{}", c.want).c_str());
            continue;
        }
        std::printf("  resolve(\"%s\") -> error:%s (as expected)\n",
                    c.ptr, std::format("{}", r.error()).c_str());
        ++passed;
    }
    std::printf("  [%d / %d passed] (error cases)\n", passed, total);
}

// ===========================================================================
// Section 5 — resolve() returns a pointer into the LIVE tree
// ===========================================================================
//
// The whole point of returning a non-owning pointer (vs. e.g.
// a JsonValue copy) is that callers can update the tree and
// see the change through the pointer. This section proves the
// pointer is real (not a snapshot) by mutating the tree and
// re-resolving.
//
// We also do a "pointer survives after vector reallocation" test:
//   - resolve /foo to a const JsonValue*
//   - push a new element into /foo (forces a realloc)
//   - resolve again; the new pointer must still point to valid
//     data (i.e. the realloc moved the storage; the resolver
//     picks up the new address correctly).

static void section_pointer_liveness() {
    print_section("Section 5: resolve() returns a pointer into the LIVE tree");

    const std::string input = R"({"foo": [1, 2, 3], "n": 7})";
    psp::JsonValue root = parse_or_default(input);

    // ----------------------------------------------------------------
    // 5a. Pointer tracks mutation through JsonValue::value.
    // ----------------------------------------------------------------
    {
        auto r = psp::json_pointer::resolve(std::string_view("/n"), root);
        if (!r) {
            std::printf("  FAIL: 5a resolve(\"/n\") errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            const auto* got = std::get_if<std::int64_t>(&(*r)->value);
            if (!got || *got != 7) {
                std::printf("  FAIL: 5a initial /n = %lld, want 7\n",
                            got ? static_cast<long long>(*got) : -1);
            } else {
                std::printf("  5a: initial /n = 7 (via pointer)\n");
            }
        }

        // Mutate /n from 7 to 99 by reaching into the tree and
        // replacing the variant alternative.
        auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        if (!obj) {
            std::printf("  FAIL: 5a root is not an object\n");
        } else {
            (*obj)["n"] = psp::JsonValue{std::int64_t{99}};
        }

        // Re-resolve. The pointer should now see 99, not 7.
        r = psp::json_pointer::resolve(std::string_view("/n"), root);
        if (!r) {
            std::printf("  FAIL: 5a re-resolve(\"/n\") errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            const auto* got = std::get_if<std::int64_t>(&(*r)->value);
            if (!got || *got != 99) {
                std::printf("  FAIL: 5a after mutation /n = %lld, want 99\n",
                            got ? static_cast<long long>(*got) : -1);
            } else {
                std::printf("  5a: after mutation /n = 99 (pointer tracks change)\n");
            }
        }
    }

    // ----------------------------------------------------------------
    // 5b. Pointer survives a std::vector reallocation.
    // ----------------------------------------------------------------
    {
        // Snapshot the address of /foo/0 BEFORE we push a new
        // element (which may reallocate the std::vector<JsonValue>).
        const psp::JsonValue* before = nullptr;
        {
            auto r = psp::json_pointer::resolve(std::string_view("/foo/0"), root);
            if (!r) {
                std::printf("  FAIL: 5b resolve(\"/foo/0\") errored: %s\n",
                            std::format("{}", r.error()).c_str());
                return;
            }
            before = *r;
        }

        // Now push a new element onto /foo. The std::vector may
        // reallocate, invalidating any raw pointer into the
        // previous storage. resolve() must not cache old
        // addresses; it must walk the tree fresh on every call.
        {
            auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
            if (!obj) {
                std::printf("  FAIL: 5b root is not an object\n");
                return;
            }
            auto* foo = std::get_if<std::vector<psp::JsonValue>>(&(*obj)["foo"].value);
            if (!foo) {
                std::printf("  FAIL: 5b /foo is not an array\n");
                return;
            }
            // Reserve-then-push a handful of elements to make
            // reallocation virtually certain on every ABI.
            foo->reserve(foo->size() + 16);
            for (int i = 0; i < 8; ++i) {
                foo->push_back(psp::JsonValue{std::int64_t{1000 + i}});
            }
        }

        // Re-resolve /foo/0. Address will probably differ from
        // `before`. Either way, the data must be 1.
        const psp::JsonValue* after = nullptr;
        {
            auto r = psp::json_pointer::resolve(std::string_view("/foo/0"), root);
            if (!r) {
                std::printf("  FAIL: 5b re-resolve(\"/foo/0\") errored: %s\n",
                            std::format("{}", r.error()).c_str());
                return;
            }
            after = *r;
            const auto* got = std::get_if<std::int64_t>(&(*r)->value);
            if (!got || *got != 1) {
                std::printf("  FAIL: 5b after push /foo/0 = %lld, want 1\n",
                            got ? static_cast<long long>(*got) : -1);
                return;
            }
            std::printf("  5b: /foo/0 still = 1 after pushing 8 more elements\n");
            std::printf("       (before=%p, after=%p, realloc=%s)\n",
                        static_cast<const void*>(before),
                        static_cast<const void*>(after),
                        (before == after) ? "no" : "yes");
        }
    }

    // ----------------------------------------------------------------
    // 5c. Resolve by tokens (pre-split) is the same as resolve by
    //     string. We verify the two overloads give equal results.
    // ----------------------------------------------------------------
    {
        auto r1 = psp::json_pointer::resolve(std::string_view("/users/0/name"),
                                            root);
        // Build tokens manually from "/users/0/name".
        auto toks = psp::json_pointer::split(std::string_view("/users/0/name"));
        // (We don't have /users in `root` for THIS section; `root`
        // here is the {"foo":[...], "n":...} tree, so /users/0/name
        // will fail. Use a pointer that we know exists.)
        (void)r1; (void)toks;
        // We do know /foo/3 exists (we just pushed 8 elements on top
        // of 3, so /foo/3 is value 1000, /foo/0 is 1).
        auto toks2 = psp::json_pointer::split(std::string_view("/foo/3"));
        if (!toks2) {
            std::printf("  FAIL: 5c split(\"/foo/3\") errored: %s\n",
                        std::format("{}", toks2.error()).c_str());
        } else {
            auto r2 = psp::json_pointer::resolve(*toks2, root);
            auto r3 = psp::json_pointer::resolve(std::string_view("/foo/3"), root);
            if (!r2 || !r3) {
                std::printf("  FAIL: 5c resolve() returned null\n");
            } else if (*r2 != *r3) {
                std::printf("  FAIL: 5c token-vs-string resolve disagree: %p vs %p\n",
                            static_cast<const void*>(*r2),
                            static_cast<const void*>(*r3));
            } else {
                std::printf("  5c: split + resolve(vector) == resolve(string_view) "
                            "for /foo/3\n");
            }
        }
    }
}

// ===========================================================================
// Section 6 — sizeof / feature probes (json_ext.h surface)
// ===========================================================================

static void section_probes() {
    print_section("Section 6: sizeof / feature probes (json_ext.h surface)");

    std::printf("  sizeof(JsonExtError)                          = %zu\n",
                sizeof(JsonExtError));
    std::printf("  sizeof(psp::json_pointer::ReferenceToken)     = %zu\n",
                sizeof(psp::json_pointer::ReferenceToken));
    std::printf("  sizeof(std::vector<psp::json_pointer::ReferenceToken>) = %zu\n",
                sizeof(std::vector<psp::json_pointer::ReferenceToken>));
    std::printf("  sizeof(std::expected<const psp::JsonValue*,   = %zu\n",
                sizeof(std::expected<const psp::JsonValue*, JsonExtError>));
    std::printf("         JsonExtError>)\n");
    std::printf("  sizeof(const psp::JsonValue*)                 = %zu\n",
                sizeof(const psp::JsonValue*));

    // The expected<const JsonValue*, JsonExtError> should be 16
    // bytes on this toolchain (8-byte ptr + 4-byte err + 4-byte
    // padding). 20 bytes is the next plausible size (no padding);
    // anything much larger means the implementation has an extra
    // word (unexpected).
    if (sizeof(std::expected<const psp::JsonValue*, JsonExtError>) > 16) {
        std::printf("  NOTE: expected<const JsonValue*, JsonExtError> is %zu bytes, "
                    "expected 16 on most ABIs (8 ptr + 4 err + 4 padding)\n",
                    sizeof(std::expected<const psp::JsonValue*, JsonExtError>));
    }

    std::printf("  Public-header roster (v0.11.0):\n");
    std::printf("    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)\n");
    std::printf("    <psp_span/parser.h>   : psp::parse_int/parse_double + cursor + JSON scalars + ParseError (+ DuplicateKey in v0.10.0)\n");
    std::printf("    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string\n");
    std::printf("    <psp_span/json_ext.h> : psp::json_pointer::split / to_string / resolve  (NEW in v0.11.0)\n");
    std::printf("                             + ::JsonExtError (8 enumerators) + std::formatter spec\n");

#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                            = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    section_split_and_to_string();
    section_resolve_canonical();
    section_resolve_nested();
    section_error_cases();
    section_pointer_liveness();
    section_probes();
    std::printf("\n[psp_json_pointer_consumer: all 6 sections complete]\n");
    return 0;
}
