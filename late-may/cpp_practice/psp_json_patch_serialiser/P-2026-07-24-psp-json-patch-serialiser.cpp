// P-2026-07-24 — Consumer of psp_span_lib v0.13.0 that implements
// and exercises the RFC 6902 §3 wire-format WRITER — the mirror
// image of the Jul 23 lesson's parser
// (psp::json_patch::parse_patch_document). Today:
//
//   - We define `serialise_patch_document(ops)` locally — takes a
//     std::span<const JsonPatchOp>, returns a std::string. The
//     output is a JSON Patch document (a JSON array of op
//     objects) in the RFC 6902 §3 wire format, ready to feed
//     straight into parse_patch_document.
//
//   - We round-trip: build ops -> serialise -> re-parse with
//     parse_patch_document -> verify the re-parsed vector is
//     field-equivalent to the original.
//
//   - We exercise every op shape (add/remove/replace/move/copy/
//     test), every value-shape (null/bool/int/double/string/array/
//     object/nested), the empty vector (=> "[]"), and a couple
//     of structural edge cases (paths containing "/" or "-",
//     the RFC 6901 "array end" pointer "/-", etc.).
//
// Where this fits in the arc
// --------------------------
// The Jul 23 lesson (P-2026-07-23-psp-json-patch-parser.cpp)
// closed the round-trip end-to-end for the parse half:
//
//    bytes in via parse_value_at  -> JsonValue tree
//    tree walked field by field   -> std::vector<JsonPatchOp>
//    patch() applied              -> mutated JsonValue tree
//    bytes out via json_to_string -> round-trippable text
//
// What it DID NOT cover was the **opposite direction** for the
// ops themselves:
//
//    ops in memory (hand-built or just-applied)
//       -> serialise_patch_document
//       -> bytes out (RFC 6902 §3 wire-format string)
//       -> parse_patch_document (the v0.13.0 reader)
//       -> ops in memory (re-parsed, field-equivalent to original)
//
// That's today's lesson. We do not bump the library version —
// serialise_patch_document lives in the consumer today. A
// future lesson can promote it into <psp_span/json_ext.h> as
// the v0.14.0 half (the symmetric counterpart to the v0.13.0
// parser). For now, the design is exercised end-to-end here so
// that the future library upgrade is mechanical.
//
// Why a consumer and not a library upgrade?
// -----------------------------------------
// Two reasons:
//
//   1. The Jul 23 lesson already shipped the parser half in
//      <psp_span/json_ext.h>. Adding the writer half in the same
//      header would push it past 1300 lines — a lot of code to
//      land in one lesson. Splitting parser (Jul 23) and writer
//      (today's exploration; future v0.14.0 promotion) gives each
//      half its own focused review.
//
//   2. The writer's correctness is best proved by **round-tripping
//      through the library's own parser**. A consumer that does
//      this end-to-end is the right vehicle: it would catch any
//      mismatch between the writer's output and the parser's
//      expectations immediately. Writing the writer inside the
//      library and testing it against a consumer is harder to
//      keep honest.
//
// What the writer does
// --------------------
// A JSON Patch document is a JSON ARRAY of op OBJECTS. The
// writer therefore has two layers:
//
//   1. Per-op writer (one JsonPatchOp -> one JsonValue object).
//      This is where std::visit dispatches on the kind field.
//
//   2. Document writer (vector<JsonPatchOp> -> JSON array
//      string). This is where json_to_string does the heavy
//      lifting over a std::vector<JsonValue> we build by
//      hand. The writer builds the right JsonValue tree
//      (vector<map<string, JsonValue>>) and hands it to
//      json_to_string.
//
// The per-op writer is the interesting part. Each op kind has
// a fixed shape (RFC 6902 §4):
//
//   add     { "op": "add",     "path": ..., "value": ... }
//   remove  { "op": "remove",  "path": ...                }
//   replace { "op": "replace", "path": ..., "value": ... }
//   move    { "op": "move",    "from": ..., "path": ...   }
//   copy    { "op": "copy",    "from": ..., "path": ...   }
//   test    { "op": "test",    "path": ..., "value": ... }
//
// std::visit over the variant data member + a switch on the
// OpKind tag gives us a clean dispatch. (We use both because
// the tag is the obvious switch key, and visit is the natural
// way to get the typed struct out of the variant.)
//
// Build (assumes psp_span_lib v0.13.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-24-psp-json-patch-serialiser
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-24-psp-json-patch-serialiser

#include <psp_span/json_ext.h>
#include <psp_span/json.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
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
// serialise_patch_document — RFC 6902 §3 wire-format writer
// ===========================================================================
//
// Lives at file scope (the same scope as parse_patch_document
// would when it lives in the library) so std::format can
// specialise for any error type we might add. Returns
// std::string because the writer is infallible (the in-memory
// ops are already valid by construction — they came out of the
// parser or were built by the caller with known shape).
//
// The function:
//
//   1. Builds a std::vector<JsonValue>, one JsonValue per op.
//      Each JsonValue holds a std::map<std::string, JsonValue>
//      of the right field shape (kind-tagged).
//   2. Hands the vector to psp::json_to_string, which pretty-
//      prints it as a JSON array.
//
// json_to_string's existing pretty-printer is exactly the right
// primitive here — it already handles null/bool/int/double/
// string/array/object/nested-recursion and the surrounding
// "[ ... ]" structure. We reuse it; we do not duplicate it.

namespace psp::json_patch {

inline std::string serialise_patch_document(
    std::span<const JsonPatchOp> ops) {
    std::vector<psp::JsonValue> out;
    out.reserve(ops.size());

    for (const auto& op : ops) {
        std::map<std::string, psp::JsonValue> obj;

        // The "op" tag is always a string. We dispatch on
        // op.kind to know which *other* fields to emit.
        switch (op.kind) {
            case OpKind::Add: {
                const auto& a = std::get<AddOp>(op.data);
                obj["op"]   = psp::JsonValue{std::string{"add"}};
                obj["path"] = psp::JsonValue{a.path};
                obj["value"] = a.value;  // copy the JsonValue tree
                break;
            }
            case OpKind::Remove: {
                const auto& r = std::get<RemoveOp>(op.data);
                obj["op"]   = psp::JsonValue{std::string{"remove"}};
                obj["path"] = psp::JsonValue{r.path};
                break;
            }
            case OpKind::Replace: {
                const auto& r = std::get<ReplaceOp>(op.data);
                obj["op"]   = psp::JsonValue{std::string{"replace"}};
                obj["path"] = psp::JsonValue{r.path};
                obj["value"] = r.value;
                break;
            }
            case OpKind::Move: {
                const auto& m = std::get<MoveOp>(op.data);
                obj["op"]   = psp::JsonValue{std::string{"move"}};
                obj["from"] = psp::JsonValue{m.from};
                obj["path"] = psp::JsonValue{m.path};
                break;
            }
            case OpKind::Copy: {
                const auto& c = std::get<CopyOp>(op.data);
                obj["op"]   = psp::JsonValue{std::string{"copy"}};
                obj["from"] = psp::JsonValue{c.from};
                obj["path"] = psp::JsonValue{c.path};
                break;
            }
            case OpKind::Test: {
                const auto& t = std::get<TestOp>(op.data);
                obj["op"]   = psp::JsonValue{std::string{"test"}};
                obj["path"] = psp::JsonValue{t.path};
                obj["value"] = t.value;
                break;
            }
        }

        out.push_back(psp::JsonValue{std::move(obj)});
    }

    // Hand the assembled vector-of-objects to the v0.10.0
    // pretty-printer. json_to_string walks the std::vector
    // alternative of the JsonValue variant, recurses into
    // each element's std::map, and emits the RFC 6902 §3
    // wire-format text.
    psp::JsonValue doc{std::move(out)};
    return psp::json_to_string(doc, 0);
}

}  // namespace psp::json_patch

// ===========================================================================
// Helpers (test infrastructure — no patch code).
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// Parse a JSON value (used to build input trees we then patch).
static psp::JsonValue parse_or_die(const std::string& s) {
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
// Field-equivalence check
// ===========================================================================
//
// The round-trip test (write -> parse -> compare) needs to
// assert that the re-parsed ops are field-equivalent to the
// originals. We don't have operator== on JsonPatchOp today
// (the Jul 22 lesson deliberately skipped it — the OpKind
// discriminant + variant comparison would be ambiguous on the
// "did the writer preserve order?" axis). Instead we do a
// field-by-field check per op kind.
//
// The check is "string equality of the serialised form": if
// serialise(parse_round_trip(serialise(ops))) == serialise(ops),
// the ops are field-equivalent for the JSON Patch use case
// (order of ops is preserved, value trees are preserved, paths
// are preserved). This is the same equality the parser
// effectively provides: "two op vectors are equivalent if they
// describe the same sequence of patches".

static bool ops_equivalent(const std::vector<JsonPatchOp>& a,
                            const std::vector<JsonPatchOp>& b) {
    auto a_str = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{a});
    auto b_str = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{b});
    return a_str == b_str;
}

// ===========================================================================
// Section 1 — Per-op writer: every kind produces the right
//             field shape
// ===========================================================================
//
// One op of each kind, hand-built, then serialised. We eyeball
// the output: it must match the RFC 6902 §3 wire format. This
// is the basic smoke test.

static void section1_per_op_shapes() {
    print_section("Section 1: per-op writer — every kind's field shape");

    std::vector<JsonPatchOp> ops;

    ops.push_back(JsonPatchOp{AddOp{
        "/a",
        parse_or_die("1")
    }});
    ops.push_back(JsonPatchOp{RemoveOp{"/a"}});
    ops.push_back(JsonPatchOp{ReplaceOp{
        "/b",
        parse_or_die("\"two\"")
    }});
    ops.push_back(JsonPatchOp{MoveOp{"/b", "/c"}});
    ops.push_back(JsonPatchOp{CopyOp{"/c", "/d"}});
    ops.push_back(JsonPatchOp{TestOp{
        "/d",
        parse_or_die("true")
    }});

    std::printf("  serialise_patch_document:\n%s\n",
        psp::json_patch::serialise_patch_document(
            std::span<const JsonPatchOp>{ops}).c_str());
}

// ===========================================================================
// Section 2 — Round-trip: serialise -> parse -> serialise
//             produces a fixed point
// ===========================================================================
//
// The fundamental invariant for any serialiser/parser pair:
// parse(serialise(x)) == x. We verify by serialising the
// output once more and checking the second serialisation
// matches the first. (We don't have JsonPatchOp == today, so
// "match" means "same serialised bytes".)

static void round_trip(const std::vector<JsonPatchOp>& ops,
                       const char* tag) {
    auto first = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{ops});

    // Parse the serialised form.
    auto parsed = psp::json_patch::parse_patch_document(first);
    if (!parsed) {
        std::printf("  %s: parse(serialise) -> %s (UNEXPECTED)\n",
                    tag, std::format("{}", parsed.error()).c_str());
        std::exit(1);
    }
    if (parsed->size() != ops.size()) {
        std::printf("  %s: round-trip size mismatch: %zu -> %zu\n",
                    tag, ops.size(), parsed->size());
        std::exit(1);
    }
    if (!ops_equivalent(ops, *parsed)) {
        std::printf("  %s: round-trip field mismatch:\n", tag);
        std::printf("    first serialisation:\n%s\n", first.c_str());
        std::printf("    parsed then serialised:\n%s\n",
            psp::json_patch::serialise_patch_document(
                std::span<const JsonPatchOp>{*parsed}).c_str());
        std::exit(1);
    }
    std::printf("  %s: round-trip OK (%zu op(s), fixed point)\n",
                tag, ops.size());
}

static void section2_round_trip() {
    print_section("Section 2: round-trip — serialise -> parse -> serialise fixed point");

    // (a) A heterogeneous op vector mixing kinds.
    std::vector<JsonPatchOp> a;
    a.push_back(JsonPatchOp{AddOp{"/x", parse_or_die("42")}});
    a.push_back(JsonPatchOp{ReplaceOp{"/x", parse_or_die("\"hello\"")}});
    a.push_back(JsonPatchOp{TestOp{"/x", parse_or_die("\"hello\"")}});
    a.push_back(JsonPatchOp{CopyOp{"/x", "/y"}});
    a.push_back(JsonPatchOp{RemoveOp{"/x"}});
    round_trip(a, "2a");

    // (b) Nested value: object inside add's "value".
    std::vector<JsonPatchOp> b;
    b.push_back(JsonPatchOp{AddOp{"/o", parse_or_die(
        R"({"k": [1, 2, {"deep": null}]})")}});
    round_trip(b, "2b");

    // (c) Move with /-/ "array end" path.
    std::vector<JsonPatchOp> c;
    c.push_back(JsonPatchOp{AddOp{"/arr/-", parse_or_die("\"tail\"")}});
    c.push_back(JsonPatchOp{MoveOp{"/arr/0", "/arr/-"}});
    round_trip(c, "2c");

    // (d) Paths containing "/" and "~0"/"~1" escapes.
    std::vector<JsonPatchOp> d;
    d.push_back(JsonPatchOp{AddOp{"/path/with/slash",
                                   parse_or_die("1")}});
    d.push_back(JsonPatchOp{ReplaceOp{"/path~1with~0tilde",
                                      parse_or_die("2")}});
    round_trip(d, "2d");

    // (e) Single op.
    std::vector<JsonPatchOp> e;
    e.push_back(JsonPatchOp{RemoveOp{"/only"}});
    round_trip(e, "2e");

    // (f) Empty vector.
    std::vector<JsonPatchOp> f;
    round_trip(f, "2f");
}

// ===========================================================================
// Section 3 — Full round-trip: build ops -> serialise -> parse
//             -> patch -> check the tree matches the expected
//             outcome
// ===========================================================================
//
// The Jul 23 lesson verified parse -> patch -> json_to_string.
// Today we add the symmetric direction: build -> serialise ->
// parse -> patch -> json_to_string. The combined round-trip
// (build -> serialise -> parse -> patch) must produce the
// same mutated tree as the direct build -> patch.

static psp::JsonValue patch_or_die(psp::JsonValue& root,
                                   const std::vector<JsonPatchOp>& ops,
                                   const char* tag) {
    auto r = psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{ops});
    if (!r) {
        std::printf("  %s: patch -> %s\n", tag,
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    return root;
}

static void section3_full_round_trip() {
    print_section("Section 3: full round-trip — build -> serialise -> parse -> patch");

    // Build the RFC 6902 §1 example as ops in memory.
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{TestOp{"/baz",
                                     parse_or_die("\"qux\"")}});
    ops.push_back(JsonPatchOp{RemoveOp{"/baz"}});
    ops.push_back(JsonPatchOp{AddOp{"/baz",
                                    parse_or_die("[\"boo\", \"hoo\"]")}});

    // Serialise them.
    auto doc = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{ops});
    std::printf("  3: serialised document (RFC 6902 §1 example):\n%s\n",
                doc.c_str());

    // Parse the serialised form (proves the writer's output is
    // also acceptable input to the library parser).
    auto parsed = psp::json_patch::parse_patch_document(doc);
    if (!parsed) {
        std::printf("  3: parse(serialise) -> %s (UNEXPECTED)\n",
                    std::format("{}", parsed.error()).c_str());
        std::exit(1);
    }

    // Apply the re-parsed ops to the RFC 6902 §1 starting tree.
    psp::JsonValue tree = parse_or_die(R"({"baz": "qux", "bar": "qux"})");
    patch_or_die(tree, *parsed, "3");

    // Verify the result matches the canonical §1 outcome.
    auto expected = parse_or_die(R"({"bar": "qux", "baz": ["boo", "hoo"]})");
    if (!(tree == expected)) {
        std::printf("  3: tree mismatch:\n");
        std::printf("    got:      %s\n", psp::json_to_string(tree).c_str());
        std::printf("    expected: %s\n", psp::json_to_string(expected).c_str());
        std::exit(1);
    }
    std::printf("  3: full round-trip OK — tree matches RFC 6902 §1\n");
}

// ===========================================================================
// Section 4 — Value shapes: every JsonValue alternative round-trips
// ===========================================================================
//
// The writer emits the value of add/replace/test ops by
// embedding the JsonValue tree into the output object. The
// consumer needs to verify every JsonValue alternative
// (null/bool/int/double/string/array/object/nested) survives
// the round-trip.

static void section4_value_shapes() {
    print_section("Section 4: every JsonValue alternative round-trips");

    struct ShapeCase {
        const char* tag;
        std::string value_text;  // source for parse_or_die
    };

    // Notes on the JsonValue alternatives:
    //   - null, bool, double, string, array, object: full
    //     coverage.
    //   - integer: routed through parse_double_at by
    //     parse_value_at (the v0.10.0 dispatcher uses
    //     parse_double_at for both digit-led and '-'-led
    //     numeric inputs). parse_double_at currently REJECTS
    //     a leading '-' (returns LeadingSign). This is a
    //     pre-existing library limitation, not in scope today
    //     — we cover positive integers only.
    //   - int64-range values that fit inside int (INT_MAX
    //     ~= 2.147e9) round-trip as int64; values above that
    //     (but within int64 range) round-trip as double
    //     because parse_double_at overflows at INT_MAX. Also
    //     a pre-existing library limitation, not in scope.
    const ShapeCase cases[] = {
        {"4a (null)",       "null"},
        {"4b (true)",       "true"},
        {"4c (false)",      "false"},
        {"4d (int zero)",   "0"},
        {"4e (int 1)",      "1"},
        {"4f (int large)",  "2000000000"},            // near INT_MAX
        {"4g (double)",     "3.14159"},
        {"4h (sm double)",  "0.0001"},
        {"4i (empty str)",  "\"\""},
        {"4j (str /slash)", "\"a/b/c\""},
        {"4k (str escape)", "\"line1\\nline2\\t\\\"\""},
        {"4l (empty arr)",  "[]"},
        {"4m (empty obj)",  "{}"},
        {"4n (nested)",     R"({"k": [1, null, "x", {"y": []}]})"},
        {"4o (deep nest)",  R"([[[[[[42]]]]]])"},
    };

    for (const auto& c : cases) {
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/v", parse_or_die(c.value_text)}});
        round_trip(ops, c.tag);
    }
}

// ===========================================================================
// Section 5 — Patch-then-serialise: a working pipeline
// ===========================================================================
//
// The motivating use case: a server keeps ops in memory; the
// client asks for "what's your pending diff?"; the server
// serialises the ops, sends them; the client parses and
// applies. Today we exercise one half of that: we patch a
// tree, capture the ops that did the patching, then
// serialise them and check the output is well-formed RFC
// 6902 §3.
//
// We seed /users first so subsequent add ops can find the
// parent container. (The Jul 22 patch engine follows RFC
// 6902 §4.1 strictly: add to a missing parent is an error.)
static void section5_patch_then_serialise() {
    print_section("Section 5: patch a tree, serialise the ops, re-apply from serialised form");

    // Step 1: build the ops to apply (the parent /users is
    // pre-seeded so subsequent adds find a container).
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{AddOp{"/users",
                                    parse_or_die("{}")}});
    ops.push_back(JsonPatchOp{AddOp{"/users/alice",
                                    parse_or_die(R"({"age": 30})")}});
    ops.push_back(JsonPatchOp{AddOp{"/users/bob",
                                    parse_or_die(R"({"age": 25})")}});
    ops.push_back(JsonPatchOp{ReplaceOp{"/users/bob/age",
                                        parse_or_die("26")}});
    ops.push_back(JsonPatchOp{CopyOp{"/users/alice",
                                     "/users/copy_of_alice"}});

    psp::JsonValue tree_a = parse_or_die("{}");
    patch_or_die(tree_a, ops, "5a (direct)");

    // Step 2: serialise the ops.
    auto doc = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{ops});
    std::printf("  5: serialised ops document:\n%s\n", doc.c_str());

    // Step 3: parse and re-apply from the serialised form.
    auto parsed = psp::json_patch::parse_patch_document(doc);
    if (!parsed) {
        std::printf("  5: parse -> %s (UNEXPECTED)\n",
                    std::format("{}", parsed.error()).c_str());
        std::exit(1);
    }
    psp::JsonValue tree_b = parse_or_die("{}");
    patch_or_die(tree_b, *parsed, "5b (re-applied)");

    // Step 4: both trees must be identical.
    if (!(tree_a == tree_b)) {
        std::printf("  5: trees differ after round-trip:\n");
        std::printf("    direct:    %s\n", psp::json_to_string(tree_a).c_str());
        std::printf("    round-trip: %s\n", psp::json_to_string(tree_b).c_str());
        std::exit(1);
    }
    std::printf("  5: pipeline OK — direct == round-trip\n");
}

// ===========================================================================
// Section 6 — Empty document serialisation
// ===========================================================================

static void section6_empty_document() {
    print_section("Section 6: empty document serialisation");

    std::vector<JsonPatchOp> empty;
    auto doc = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{empty});
    std::printf("  6: empty ops vector -> \"%s\"\n", doc.c_str());
    if (doc != "[]") {
        std::printf("  6: expected \"[]\"\n");
        std::exit(1);
    }

    // The parser must accept the empty doc as zero ops.
    auto parsed = psp::json_patch::parse_patch_document(doc);
    if (!parsed) {
        std::printf("  6: parse(\"%s\") -> %s (UNEXPECTED)\n",
                    doc.c_str(),
                    std::format("{}", parsed.error()).c_str());
        std::exit(1);
    }
    if (!parsed->empty()) {
        std::printf("  6: expected empty re-parsed vector, got %zu\n",
                    parsed->size());
        std::exit(1);
    }
    std::printf("  6: empty doc re-parses to zero ops — symmetric with parser\n");
}

// ===========================================================================
// Section 7 — sizeof / feature probes
// ===========================================================================

static void section7_probes() {
    print_section("Section 7: sizeof / feature probes");

    std::printf("  sizeof(JsonPatchError)                          = %zu\n",
                sizeof(JsonPatchError));
    std::printf("  sizeof(JsonPatchOp)                             = %zu\n",
                sizeof(JsonPatchOp));
    std::printf("  sizeof(std::vector<JsonPatchOp>)                 = %zu\n",
                sizeof(std::vector<JsonPatchOp>));
    std::printf("  sizeof(std::string) (return type)                = %zu\n",
                sizeof(std::string));

    std::printf("  Writer interface (local to this consumer):\n");
    std::printf("    psp::json_patch::serialise_patch_document(span<JsonPatchOp>)\n");
    std::printf("      -> std::string (RFC 6902 §3 wire format)\n");
    std::printf("  Mirror image of the v0.13.0 parser:\n");
    std::printf("    psp::json_patch::parse_patch_document(string_view)\n");
    std::printf("      -> std::expected<vector<JsonPatchOp>, JsonPatchError>\n");
    std::printf("  Together they close the full ops round-trip.\n");

#ifdef __cpp_lib_expected
    std::printf("  __cpp_lib_expected                               = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#ifdef __cpp_lib_variant
    std::printf("  __cpp_lib_variant                                = %ld\n",
                static_cast<long>(__cpp_lib_variant));
#endif
}

// ===========================================================================
// Section 8 — Backwards compat: v0.13.0 parser/header unchanged
// ===========================================================================
//
// Today only adds a consumer-side writer; the library version
// stays at v0.13.0. We confirm by re-running a tiny slice of
// the Jul 22 Patch engine surface (the part that proves the
// v0.12.0 Patch half is still reachable).

static void section8_backcompat() {
    print_section("Section 8: backwards compat — v0.13.0 unchanged");

    psp::JsonValue doc = parse_or_die(R"({"a": {"b": [10, 20, 30]}})");
    auto r = psp::json_pointer::resolve("/a/b/0", doc);
    if (!r) {
        std::printf("  8a: resolve(/a/b/0) -> %s (UNEXPECTED)\n",
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    if (std::get<std::int64_t>((**r).value) != 10) {
        std::printf("  8a: resolve(/a/b/0) != 10\n");
        std::exit(1);
    }
    std::printf("  8a: resolve(/a/b/0) = 10 (Pointer half unchanged)\n");

    psp::JsonValue tree = parse_or_die(R"({"k": 1})");
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{ReplaceOp{"/k", parse_or_die("99")}});
    auto pr = psp::json_patch::patch(tree,
        std::span<const JsonPatchOp>{ops});
    if (!pr) {
        std::printf("  8b: patch -> %s\n", std::format("{}", pr.error()).c_str());
        std::exit(1);
    }
    std::printf("  8b: patch hand-built ops -> %s (Patch half unchanged)\n",
                psp::json_to_string(tree).c_str());
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-07-24 — RFC 6902 §3 wire-format WRITER: "
                "serialise_patch_document (consumer exercise; "
                "mirror of v0.13.0's parse_patch_document)\n");

    section1_per_op_shapes();
    section2_round_trip();
    section3_full_round_trip();
    section4_value_shapes();
    section5_patch_then_serialise();
    section6_empty_document();
    section7_probes();
    section8_backcompat();

    std::printf("\n[psp_json_patch_serialiser: all 8 sections complete]\n");
    return 0;
}