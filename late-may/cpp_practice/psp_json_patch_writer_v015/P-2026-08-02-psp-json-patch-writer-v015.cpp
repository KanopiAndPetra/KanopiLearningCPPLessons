// P-2026-08-02 — Consumer of psp_span_lib v0.15.0 that exercises
// psp::json_patch::serialise_patch_document from the LIBRARY
// PROPER (the RFC 6902 §3 wire-format WRITER promoted from the
// Jul 24 consumer today).
//
// Where this fits in the arc
// --------------------------
// The Jul 24 lesson (P-2026-07-24-psp-json-patch-serialiser.cpp)
// designed and exercised the writer as a local function in a
// consumer TU, on top of psp_span_lib v0.13.0. It explicitly
// said:
//
//   "A future lesson can promote it into <psp_span/json_ext.h>
//    as the v0.14.0 half (the symmetric counterpart to the
//    v0.13.0 parser). For now, the design is exercised end-to-
//    end here so that the future library upgrade is mechanical."
//
// The Aug 1 lesson (P-2026-08-01-psp-parser-v014-update.md)
// closed the v0.14.0 promotion arc and listed v0.15.0 candidates:
//
//   - "psp::json_patch::serialise_patch_document in the
//      library proper — lift the Jul 24 consumer writer (and
//      the Jul 27 re-inlined op_writer) into a header function.
//      The promotion is near-mechanical."
//
// Today is that lesson. The library has been bumped to v0.15.0
// (a strict superset of v0.14.0; the only change is one
// additional inline function in <psp_span/json_ext.h>).
//
// The writer lives in the library now
// -----------------------------------
// Today's consumer drops the LOCAL copy of
// serialise_patch_document. We call psp::json_patch::serialise_patch_document
// from the library header; the same function exercised by the
// Jul 24 consumer is now visible to any consumer that
// `find_package(psp_span_lib 0.15 REQUIRED)`.
//
// The change is mechanical: the function code is byte-for-byte
// equivalent to the Jul 24 implementation, with the namespace
// and forward-declared dependencies already in scope from
// <psp_span/json_ext.h>. The promotion adds ~120 lines to
// <psp_span/json_ext.h> (the function body + an extensive
// documenting comment) and zero new error enumerators — the
// writer is infallible by design.
//
// What the consumer exercises
// ---------------------------
//
//   Section 1 — symbol-presence probe
//   Section 2 — per-op writer (every kind's field shape)
//   Section 3 — round-trip serialise -> parse -> serialise
//               fixed point (writer in library; parser in library)
//   Section 4 — every JsonValue alternative round-trips through
//               a serialised "value" field
//   Section 5 — full round-trip: build -> serialise -> parse ->
//               patch -> json_to_string (full RFC 6902 §1
//               example + a tree-mutation pipeline)
//   Section 6 — empty document symmetry
//   Section 7 — wire-format interop with v0.14.0 sign-accepted
//               values (negative + INT64_MAX-shaped ints round-
//               trip cleanly through the writer + parser + engine)
//   Section 8 — sizeof / feature probes
//   Section 9 — back-compat: Pointer + Patch + Patch-parser
//               halves from v0.14.0 unchanged
//
//   Section totals: ~50+ test cases across 9 sections, all
//   expected to pass.
//
// Build (assumes psp_span_lib v0.15.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-08-02-psp-json-patch-writer-v015
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-08-02-psp-json-patch-writer-v015

#include <psp_span/json_ext.h>
#include <psp_span/json.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// Helpers (test infrastructure — no patch code)
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        ++g_pass;
        std::printf("  PASS: %s\n", label);
    } else {
        ++g_fail;
        std::printf("  FAIL: %s\n", label);
    }
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

// Apply a patch document; abort on failure.
static void patch_or_die(psp::JsonValue& root,
                         const std::vector<JsonPatchOp>& ops,
                         const char* tag) {
    auto r = psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{ops});
    if (!r) {
        std::printf("  %s: patch -> %s\n", tag,
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
}

// ===========================================================================
// Section 1 — symbol-presence probe
// ===========================================================================
//
// Confirm the writer is a library-proper symbol. We can't
// take its address in a portable way through the public API
// (it's an inline function in a header; "address-of" resolves
// to the same inline body as a call), but we CAN take
// std::printf("%p") of a function-pointer obtained via a
// `&psp::json_patch::serialise_patch_document` line. If the
// function is undeclared (header bug), this won't compile. If
// it is, the address is well-defined. We use a
// runtime-detectable compile-time assertion: just declare a
// variable of the function-pointer type and check it compiles
// + the function is non-null.
//
// The point of this section: a future reader looking at the
// consumer can grep for "serialise_patch_document" and see
// that the consumer pulls the function from the library
// header — not from a local definition. The compile-time
// check is the assertion.

static void section1_symbol_presence() {
    print_section("Section 1: symbol-presence probe — writer is library-proper");

    // Take the address of the library function. This requires
    // a header that declares it AND the function being
    // non-ambiguous. If the function is undeclared, this line
    // won't compile.
    using writer_fn = std::string(*)(
        std::span<const JsonPatchOp>);
    writer_fn p = &psp::json_patch::serialise_patch_document;
    check(p != nullptr,
          "1a &psp::json_patch::serialise_patch_document is well-defined");

    // Also check the parser half is still callable.
    using parser_fn = std::expected<std::vector<JsonPatchOp>, JsonPatchError>(*)(
        std::string_view) noexcept;
    parser_fn q = &psp::json_patch::parse_patch_document;
    check(q != nullptr,
          "1b &psp::json_patch::parse_patch_document is well-defined (v0.13.0 half unchanged)");

    // And the engine.
    using engine_fn = std::expected<void, JsonPatchError>(*)(
        psp::JsonValue&,
        std::span<const JsonPatchOp>);
    engine_fn r = &psp::json_patch::patch;
    check(r != nullptr,
          "1c &psp::json_patch::patch is well-defined (v0.12.0 half unchanged)");

    // Round-trip call as the smoke test: serialise an empty
    // vector, parse it back, expect zero ops.
    std::vector<JsonPatchOp> empty;
    auto wire = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{empty});
    check(wire == "[]",
          "1d serialise_patch_document({}) == \"[]\"");

    auto parsed = psp::json_patch::parse_patch_document(wire);
    check(parsed.has_value() && parsed->empty(),
          "1e parse_patch_document(\"[]\") -> empty vector");
}

// ===========================================================================
// Section 2 — Per-op writer: every kind produces the right
//             field shape (from the LIBRARY, not a local copy)
// ===========================================================================
//
// One op of each kind, hand-built, then serialised via the
// library function. We eyeball the output: it must match the
// RFC 6902 §3 wire format. Same as Jul 24 Section 1, but the
// serialise_patch_document call resolves to the library
// header.

static void section2_per_op_shapes() {
    print_section("Section 2: per-op writer — every kind's field shape (library-proper)");

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

    std::string wire = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{ops});

    std::printf("  serialise_patch_document (library-proper):\n%s\n",
                wire.c_str());

    // Each op appears in the wire with its tag and expected
    // field set; we sanity-check that the wire contains each
    // op-name string.
    check(wire.find("\"add\"") != std::string::npos,
          "2a wire contains \"add\" op");
    check(wire.find("\"remove\"") != std::string::npos,
          "2b wire contains \"remove\" op");
    check(wire.find("\"replace\"") != std::string::npos,
          "2c wire contains \"replace\" op");
    check(wire.find("\"move\"") != std::string::npos,
          "2d wire contains \"move\" op");
    check(wire.find("\"copy\"") != std::string::npos,
          "2e wire contains \"copy\" op");
    check(wire.find("\"test\"") != std::string::npos,
          "2f wire contains \"test\" op");
}

// ===========================================================================
// Section 3 — Round-trip: serialise -> parse -> serialise
//             produces a fixed point (through the LIBRARY both
//             directions)
// ===========================================================================
//
// Fundamental invariant: parse(serialise(x)) == x. We verify
// by serialising the output once more and checking the second
// serialisation matches the first. The wire is a stable
// representation of the ops for the JSON Patch use case (order
// preserved, value trees preserved, paths preserved).

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

    // Re-serialise the parsed vector; it must match the first
    // serialisation byte-for-byte.
    auto second = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{*parsed});
    if (first != second) {
        std::printf("  %s: round-trip serialise mismatch:\n", tag);
        std::printf("    first:  %s\n", first.c_str());
        std::printf("    second: %s\n", second.c_str());
        std::exit(1);
    }
    std::printf("  %s: round-trip OK (%zu op(s), fixed point)\n",
                tag, ops.size());
}

static void section3_round_trip() {
    print_section("Section 3: round-trip — serialise -> parse -> serialise fixed point (library-proper both ways)");

    // (a) A heterogeneous op vector mixing kinds.
    std::vector<JsonPatchOp> a;
    a.push_back(JsonPatchOp{AddOp{"/x", parse_or_die("42")}});
    a.push_back(JsonPatchOp{ReplaceOp{"/x", parse_or_die("\"hello\"")}});
    a.push_back(JsonPatchOp{TestOp{"/x", parse_or_die("\"hello\"")}});
    a.push_back(JsonPatchOp{CopyOp{"/x", "/y"}});
    a.push_back(JsonPatchOp{RemoveOp{"/x"}});
    round_trip(a, "3a");

    // (b) Nested value: object inside add's "value".
    std::vector<JsonPatchOp> b;
    b.push_back(JsonPatchOp{AddOp{"/o", parse_or_die(
        R"({"k": [1, 2, {"deep": null}]})")}});
    round_trip(b, "3b");

    // (c) Move with /-/ "array end" path.
    std::vector<JsonPatchOp> c;
    c.push_back(JsonPatchOp{AddOp{"/arr/-", parse_or_die("\"tail\"")}});
    c.push_back(JsonPatchOp{MoveOp{"/arr/0", "/arr/-"}});
    round_trip(c, "3c");

    // (d) Paths containing "/" and "~0"/"~1" escapes.
    std::vector<JsonPatchOp> d;
    d.push_back(JsonPatchOp{AddOp{"/path/with/slash",
                                   parse_or_die("1")}});
    d.push_back(JsonPatchOp{ReplaceOp{"/path~1with~0tilde",
                                      parse_or_die("2")}});
    round_trip(d, "3d");

    // (e) Single op.
    std::vector<JsonPatchOp> e;
    e.push_back(JsonPatchOp{RemoveOp{"/only"}});
    round_trip(e, "3e");

    // (f) Empty vector.
    std::vector<JsonPatchOp> f;
    round_trip(f, "3f");
}

// ===========================================================================
// Section 4 — Value shapes: every JsonValue alternative round-trips
// ===========================================================================
//
// The writer embeds the JsonValue tree of an op's "value" field
// (for add/replace/test) into the output object. We verify every
// JsonValue alternative flows through the round-trip correctly.
//
// Notes:
//   - null, bool, double, string, array, object, nested: full
//     coverage.
//   - integer: under v0.14.0 the parse layer routes digit-led
//     ints through parse_double_at (and now accepts a leading
//     '+' or '-' since v0.14.0). INT64_MAX-shaped values
//     round-trip as int64 (int_part overflow at INT64_MAX in
//     v0.14.0). Values larger than INT64_MAX would be a double;
//     we stay within int64 range to keep the test clean.

static void section4_value_shapes() {
    print_section("Section 4: every JsonValue alternative round-trips (library-proper writer + parser)");

    struct ShapeCase {
        const char* tag;
        std::string value_text;
    };

    const ShapeCase cases[] = {
        {"4a (null)",       "null"},
        {"4b (true)",       "true"},
        {"4c (false)",      "false"},
        {"4d (int zero)",   "0"},
        {"4e (int 1)",      "1"},
        {"4f (int large)",  "2000000000"},            // near INT_MAX
        {"4g (int int64)",  "5000000000"},            // past INT_MAX; in int64 range
        {"4h (int INT64_MAX)", "9223372036854775807"},
        {"4i (double)",     "3.14159"},
        {"4j (sm double)",  "0.0001"},
        {"4k (empty str)",  "\"\""},
        {"4l (str /slash)", "\"/path/to/thing\""},
        {"4m (str escape)", "\"a\\\"b\""},
        {"4n (empty arr)",  "[]"},
        {"4o (empty obj)",  "{}"},
        {"4p (nested)",     R"({"k": [1, 2]})"},
        {"4q (deep nest)",  R"([1, [2, [3, [4]]]])"},
    };

    for (const auto& c : cases) {
        // Build an AddOp{"/v", value} -> serialise -> parse -> reserialise.
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/v", parse_or_die(c.value_text)}});
        round_trip(ops, c.tag);
    }
}

// ===========================================================================
// Section 5 — Full round-trip: build -> serialise -> parse ->
//             patch produces the same mutated tree as direct
//             build -> patch
// ===========================================================================
//
// The Jul 23 lesson verified parse -> patch -> json_to_string.
// The Jul 24 lesson added serialise -> parse -> reserialise.
// Today we put the FULL pipeline together with the writer in
// the library: build -> serialise -> parse -> patch ->
// json_to_string must produce the same outcome as direct
// build -> patch.

static void section5_full_pipeline() {
    print_section("Section 5: full round-trip — build -> serialise -> parse -> patch -> json_to_string (library-proper all the way)");

    // 5a — RFC 6902 §1 example.
    {
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{TestOp{"/baz",
                                         parse_or_die("\"qux\"")}});
        ops.push_back(JsonPatchOp{RemoveOp{"/baz"}});
        ops.push_back(JsonPatchOp{AddOp{"/baz",
                                        parse_or_die("[\"boo\", \"hoo\"]")}});

        auto wire = psp::json_patch::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  5a wire (RFC 6902 §1 example, library writer):\n%s\n",
                    wire.c_str());

        auto parsed = psp::json_patch::parse_patch_document(wire);
        if (!parsed) {
            std::printf("  5a: parse -> %s (UNEXPECTED)\n",
                        std::format("{}", parsed.error()).c_str());
            std::exit(1);
        }

        psp::JsonValue tree = parse_or_die(R"({"baz": "qux", "bar": "qux"})");
        patch_or_die(tree, *parsed, "5a");

        auto expected = parse_or_die(R"({"bar": "qux", "baz": ["boo", "hoo"]})");
        check(tree == expected,
              "5a full round-trip OK — tree matches RFC 6902 §1");
    }

    // 5b — pipeline proof: direct (build -> patch) and round-trip
    //      (build -> serialise -> parse -> patch) must produce the
    //      same mutated tree.
    {
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/users", parse_or_die("{}")}});
        ops.push_back(JsonPatchOp{AddOp{"/users/alice",
                                        parse_or_die(R"({"age": 30})")}});
        ops.push_back(JsonPatchOp{AddOp{"/users/bob",
                                        parse_or_die(R"({"age": 25})")}});
        ops.push_back(JsonPatchOp{ReplaceOp{"/users/bob/age",
                                           parse_or_die("26")}});
        ops.push_back(JsonPatchOp{CopyOp{"/users/alice",
                                         "/users/copy_of_alice"}});

        // Direct path.
        psp::JsonValue tree_a = parse_or_die("{}");
        patch_or_die(tree_a, ops, "5b direct");

        // Round-trip path.
        auto wire = psp::json_patch::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  5b wire (round-trip path):\n%s\n", wire.c_str());

        auto parsed = psp::json_patch::parse_patch_document(wire);
        if (!parsed) {
            std::printf("  5b: parse -> %s (UNEXPECTED)\n",
                        std::format("{}", parsed.error()).c_str());
            std::exit(1);
        }
        psp::JsonValue tree_b = parse_or_die("{}");
        patch_or_die(tree_b, *parsed, "5b round-trip");

        check(tree_a == tree_b,
              "5b pipeline OK — direct == round-trip (library writer + library parser + library engine)");
    }
}

// ===========================================================================
// Section 6 — empty document symmetry
// ===========================================================================
//
// The writer + parser form a fixed point for the empty
// document. Empty ops -> "[]" -> empty vector.

static void section6_empty_document() {
    print_section("Section 6: empty document symmetry (library-proper)");

    std::vector<JsonPatchOp> empty;
    auto wire = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{empty});
    check(wire == "[]",
          "6a empty ops -> \"[]\"");

    auto parsed = psp::json_patch::parse_patch_document(wire);
    check(parsed.has_value() && parsed->empty(),
          "6b empty doc re-parses to empty vector");

    // Applying the empty patch leaves the tree unchanged.
    psp::JsonValue tree = parse_or_die(R"({"k": 1})");
    auto original_str = psp::json_to_string(tree);
    patch_or_die(tree, *parsed, "6c");
    check(psp::json_to_string(tree) == original_str,
          "6c empty patch leaves tree unchanged");
}

// ===========================================================================
// Section 7 — Interop with v0.14.0 sign-accepted values
// ===========================================================================
//
// The writer embeds the JsonValue tree of an op's "value" field
// as a JSON document. If that JsonValue is a negative or
// INT64_MAX-shaped int (which only exist post-v0.14.0 in the
// parser layer), the writer must still emit the right bytes —
// the writer doesn't care about the source of the int; it just
// embeds the JsonValue tree as-is. The Aug 1 lesson fixed the
// parser to accept those values; today's writer doesn't need
// any change because the writer embeds the tree, not the wire
// text. This section proves that end-to-end.

static void section7_v014_interop() {
    print_section("Section 7: wire-format interop with v0.14.0 sign-accepted values (library-proper writer + parser + engine)");

    // 7a — negative int in an AddOp's "value" field.
    {
        psp::JsonValue v;
        v.value = static_cast<std::int64_t>(-42);
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/x", v}});
        auto wire = psp::json_patch::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  7a wire (negative int via library writer):\n%s\n",
                    wire.c_str());

        auto parsed = psp::json_patch::parse_patch_document(wire);
        check(parsed.has_value(), "7a parse(serialise(-42 AddOp)) succeeds");
        if (parsed) {
            check(parsed->size() == 1,
                  "  7a parsed 1 op");
            const auto& op0 = (*parsed)[0];
            check(op0.kind == OpKind::Add, "  7a op kind == Add");
            const auto& add = std::get<AddOp>(op0.data);
            check(std::holds_alternative<std::int64_t>(add.value.value),
                  "  7a value holds int64");
            if (std::holds_alternative<std::int64_t>(add.value.value)) {
                check(std::get<std::int64_t>(add.value.value) == -42,
                      "  7a value == -42");
            }
        }
    }

    // 7b — INT64_MAX-shaped int in a ReplaceOp's "value" field.
    {
        psp::JsonValue v;
        v.value = std::numeric_limits<std::int64_t>::max();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{ReplaceOp{"/x", v}});
        auto wire = psp::json_patch::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        std::printf("  7b wire (INT64_MAX via library writer):\n%s\n",
                    wire.c_str());

        auto parsed = psp::json_patch::parse_patch_document(wire);
        check(parsed.has_value(),
              "7b parse(serialise(INT64_MAX ReplaceOp)) succeeds");
        if (parsed) {
            const auto& op0 = (*parsed)[0];
            const auto& repl = std::get<ReplaceOp>(op0.data);
            check(std::holds_alternative<std::int64_t>(repl.value.value),
                  "  7b value holds int64");
            if (std::holds_alternative<std::int64_t>(repl.value.value)) {
                check(std::get<std::int64_t>(repl.value.value) ==
                          std::numeric_limits<std::int64_t>::max(),
                      "  7b value == INT64_MAX");
            }
        }
    }

    // 7c — apply the INT64_MAX value via patch() to confirm the
    //      engine accepts the round-tripped wire-format op.
    {
        psp::JsonValue v;
        v.value = std::numeric_limits<std::int64_t>::max();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{ReplaceOp{"/x", v}});
        auto wire = psp::json_patch::serialise_patch_document(
            std::span<const JsonPatchOp>{ops});
        auto parsed = psp::json_patch::parse_patch_document(wire);
        check(parsed.has_value(),
              "7c parse + apply INT64_MAX ReplaceOp succeeds");
        if (parsed) {
            psp::JsonValue tree;
            {
                std::map<std::string, psp::JsonValue> m;
                psp::JsonValue z;
                z.value = static_cast<std::int64_t>(0);
                m.emplace("x", std::move(z));
                tree.value = std::move(m);
            }
            patch_or_die(tree, *parsed, "7c");
            auto xv = psp::json_pointer::resolve_mut("/x", tree);
            check(xv.has_value(),
                  "  7c resolve_mut(\"/x\") succeeded");
            if (xv) {
                psp::JsonValue* p = *xv;
                check(p && std::holds_alternative<std::int64_t>(p->value)
                      && std::get<std::int64_t>(p->value) ==
                             std::numeric_limits<std::int64_t>::max(),
                      "  7c /x == INT64_MAX after patch");
            }
        }
    }
}

// ===========================================================================
// Section 8 — sizeof / feature probes
// ===========================================================================

static void section8_probes() {
    print_section("Section 8: sizeof / feature probes");

    std::printf("  sizeof(JsonPatchError)                          = %zu\n",
                sizeof(JsonPatchError));
    std::printf("  sizeof(JsonPatchOp)                             = %zu\n",
                sizeof(JsonPatchOp));
    std::printf("  sizeof(std::vector<JsonPatchOp>)                 = %zu\n",
                sizeof(std::vector<JsonPatchOp>));
    std::printf("  sizeof(std::string) (writer return type)        = %zu\n",
                sizeof(std::string));
    std::printf("  sizeof(std::expected<std::vector<JsonPatchOp>,) = %zu\n",
                sizeof(std::expected<std::vector<JsonPatchOp>, JsonPatchError>));

    std::printf("  Writer interface (v0.15.0 library-proper):\n");
    std::printf("    psp::json_patch::serialise_patch_document(span<JsonPatchOp>)\n");
    std::printf("      -> std::string (RFC 6902 §3 wire format)\n");
    std::printf("  Mirror image of the v0.13.0 parser:\n");
    std::printf("    psp::json_patch::parse_patch_document(string_view)\n");
    std::printf("      -> std::expected<vector<JsonPatchOp>, JsonPatchError>\n");
    std::printf("  Together they close the full ops round-trip in the library.\n");

#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                               = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_variant)
    std::printf("  __cpp_lib_variant                                = %ld\n",
                static_cast<long>(__cpp_lib_variant));
#endif
#if defined(__cpp_lib_span)
    std::printf("  __cpp_lib_span                                   = %ld\n",
                static_cast<long>(__cpp_lib_span));
#endif
}

// ===========================================================================
// Section 9 — Back-compat: Pointer + Patch + Patch-parser halves
//              from v0.13.0 + sign acceptance from v0.14.0 all
//              unchanged
// ===========================================================================

static void section9_backcompat() {
    print_section("Section 9: backwards compat — v0.13.0 + v0.14.0 halves unchanged");

    // 9a — Pointer half from v0.11.0.
    {
        psp::JsonValue doc = parse_or_die(R"({"a": {"b": [10, 20, 30]}})");
        auto r = psp::json_pointer::resolve("/a/b/0", doc);
        check(r.has_value()
              && std::get<std::int64_t>((**r).value) == 10,
              "9a resolve(/a/b/0) = 10 (Pointer half unchanged from v0.11.0)");
    }

    // 9b — Patch engine half from v0.12.0.
    {
        psp::JsonValue tree = parse_or_die(R"({"k": 1})");
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{ReplaceOp{"/k", parse_or_die("99")}});
        patch_or_die(tree, ops, "9b");
        check(psp::json_to_string(tree) == R"({
  "k": 99
})",
              "9b patch hand-built ReplaceOp (Patch engine half unchanged from v0.12.0)");
    }

    // 9c — Patch-parser half from v0.13.0 (canonical RFC 6902 §1).
    {
        const std::string wire = R"([
  {"op": "test",   "path": "/baz", "value": "qux"},
  {"op": "remove", "path": "/baz"},
  {"op": "add",    "path": "/baz", "value": ["boo","hoo"]}
])";
        auto parsed = psp::json_patch::parse_patch_document(wire);
        check(parsed.has_value() && parsed->size() == 3,
              "9c parse_patch_document on RFC 6902 §1 (Parser half unchanged from v0.13.0)");
    }

    // 9d — v0.14.0 sign-accepted values still parse through the
    //      unchanged parser.
    {
        const std::string wire = R"([{"op": "replace", "path": "/x", "value": -42}])";
        auto parsed = psp::json_patch::parse_patch_document(wire);
        check(parsed.has_value() && parsed->size() == 1,
              "9d parse_patch_document accepts a negative value (v0.14.0 sign acceptance unchanged)");
    }
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-02 — v0.15.0 RFC 6902 §3 wire-format WRITER:\n");
    std::printf("                psp::json_patch::serialise_patch_document\n");
    std::printf("                (promoted from Jul 24 consumer to <psp_span/json_ext.h>)\n");

    section1_symbol_presence();
    section2_per_op_shapes();
    section3_round_trip();
    section4_value_shapes();
    section5_full_pipeline();
    section6_empty_document();
    section7_v014_interop();
    section8_probes();
    section9_backcompat();

    std::printf("\n[psp_json_patch_writer_v015: %d PASS, %d FAIL]\n",
                g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
