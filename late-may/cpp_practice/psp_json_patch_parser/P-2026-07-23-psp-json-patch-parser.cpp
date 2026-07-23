// P-2026-07-23 — Consumer of psp_span_lib v0.13.0's new
// psp::json_patch::parse_patch_document. RFC 6902 §3 wire-format
// parser on top of the v0.12.0 Patch engine + v0.10.0 JsonValue
// tree.
//
// Where this fits in the arc
// --------------------------
// The Jul 22 lesson (P-2026-07-22-psp-json-patch.cpp) shipped the
// RFC 6902 Patch engine (psp::json_patch::patch) and explicitly
// said the natural next lesson was **wire-format patch parsing**:
//
//   For P-2026-07-23 the natural lesson is wire-format patch
//   parsing: take a JSON patch document like
//   [{"op": "add", ...}, ...] (the RFC 6902 §3 input shape),
//   parse it via psp::parse_value_at, validate each op's fields,
//   and assemble it into a std::vector<JsonPatchOp>.
//
// Today is that lesson. The library gains:
//
//   psp::json_patch::parse_patch_document(string_view) — takes
//     a JSON Patch document (an array of op objects in the
//     RFC 6902 §3 wire format), parses it via parse_value_at,
//     validates each op's field shape, and assembles a
//     std::vector<JsonPatchOp>. The returned vector is ready to
//     hand directly to psp::json_patch::patch — closing the
//     round-trip:
//
//        bytes in via parse_value_at  -> JsonValue tree
//        tree walked field by field   -> std::vector<JsonPatchOp>
//        patch() applied              -> mutated JsonValue tree
//        bytes out via json_to_string -> round-trippable text
//
//   ::JsonPatchError grows from 10 to 13 enumerators, adding:
//     - BadDocument  (top-level wasn't an array of objects; "op"
//                     was an unrecognized name)
//     - MissingField (e.g. no "path", "from", or "value" as
//                     appropriate)
//     - WrongType    (e.g. "op" was a number, "path" was an
//                     object)
//
// The Pointer + Patch halves from v0.12.0 are unchanged.
// v0.13.0 is a strict superset of v0.12.0.
//
// Build (assumes psp_span_lib v0.13.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-23-psp-json-patch-parser
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-23-psp-json-patch-parser

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
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// Helpers (test infrastructure — no patch code).
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// Parse a JSON value (used to build the input trees we then patch).
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

// Parse a patch document via parse_patch_document, but tolerate
// failure. On success, returns the vector; on failure, prints a
// one-line report and returns the typed error.
static std::vector<JsonPatchOp>
parse_doc_or_report(const std::string& doc,
                    const char* tag) {
    auto r = psp::json_patch::parse_patch_document(doc);
    if (!r) {
        std::printf("  %s: parse_patch_document -> %s\n",
                    tag, std::format("{}", r.error()).c_str());
        return {};
    }
    std::printf("  %s: parsed %zu op(s) -> [", tag, r->size());
    for (std::size_t i = 0; i < r->size(); ++i) {
        const auto& op = (*r)[i];
        switch (op.kind) {
            case OpKind::Add:     std::printf("add");     break;
            case OpKind::Remove:  std::printf("remove");  break;
            case OpKind::Replace: std::printf("replace"); break;
            case OpKind::Move:    std::printf("move");    break;
            case OpKind::Copy:    std::printf("copy");    break;
            case OpKind::Test:    std::printf("test");    break;
        }
        if (i + 1 < r->size()) std::printf(", ");
    }
    std::printf("]\n");
    return std::move(*r);
}

// Apply a parsed patch document to a JsonValue tree and print the
// result. On failure, print the typed error and leave the tree as
// it was after the partial application (matches the engine's
// "best-effort, aborts on first error" contract).
static void apply_doc(psp::JsonValue& root, const std::vector<JsonPatchOp>& ops,
                      const char* tag) {
    auto r = psp::json_patch::patch(root, std::span<const JsonPatchOp>{ops});
    if (!r) {
        std::printf("  %s: patch -> %s\n", tag,
                    std::format("{}", r.error()).c_str());
        std::printf("  %s: tree after partial: %s\n", tag,
                    psp::json_to_string(root).c_str());
        return;
    }
    std::printf("  %s: tree -> %s\n", tag,
                psp::json_to_string(root).c_str());
}

// ===========================================================================
// Section 1 — Round-trip: parse the RFC 6902 §1 example patch
// ===========================================================================
//
// RFC 6902 §1's example patch is:
//
//   [
//     { "op": "test",   "path": "/baz", "value": "qux" },
//     { "op": "remove", "path": "/baz"                },
//     { "op": "add",    "path": "/baz", "value": ["boo", "hoo"] }
//   ]
//
// applied to {"baz": "qux", "bar": "qux"} yields
// {"bar": "qux", "baz": ["boo", "hoo"]}.
// We exercise this end-to-end through parse_patch_document + patch.

static void section1_canonical_example() {
    print_section("Section 1: round-trip — RFC 6902 §1 example");

    const char* doc =
        R"([)"
        R"(  {"op": "test",   "path": "/baz", "value": "qux"},)"
        R"(  {"op": "remove", "path": "/baz"},)"
        R"(  {"op": "add",    "path": "/baz", "value": ["boo", "hoo"]})"
        R"(])";

    auto ops = parse_doc_or_report(doc, "1");
    if (ops.empty()) return;

    psp::JsonValue root = parse_or_die(R"({"baz": "qux", "bar": "qux"})");
    apply_doc(root, ops, "1");
}

// ===========================================================================
// Section 2 — All six ops, one of each in a single document
// ===========================================================================
//
// A single document with one op of every kind. Exercises the
// dispatch on op-name inside build_one_op: add, remove, replace,
// move, copy, test all in sequence. The test op is first so we
// know the rest start from a known state.

static void section2_all_six_ops() {
    print_section("Section 2: all six ops in one document");

    const char* doc =
        R"([)"
        R"(  {"op": "test",   "path": "/n", "value": 1},)"
        R"(  {"op": "add",    "path": "/n", "value": 99},)"
        R"(  {"op": "replace","path": "/n", "value": 42},)"
        R"(  {"op": "copy",   "from": "/n", "path": "/copy"},)"
        R"(  {"op": "move",   "from": "/copy", "path": "/moved"},)"
        R"(  {"op": "remove", "path": "/moved"})"
        R"(])";

    auto ops = parse_doc_or_report(doc, "2");
    if (ops.empty()) return;

    psp::JsonValue root = parse_or_die(R"({"n": 1})");
    apply_doc(root, ops, "2");
}

// ===========================================================================
// Section 3 — Round-trip: a non-trivial multi-op patch from RFC 6902 §A
// ===========================================================================
//
// RFC 6902 appendix A's example:
//
//   [
//     {"op": "add", "path": "/biscuits", "value": []},
//     {"op": "add", "path": "/biscuits/0", "value": {"name": "Ginger Wale"}}
//   ]
//
// applied to {} yields {"biscuits": [{"name": "Ginger Wale"}]}.
// Tests that parse_patch_document correctly handles nested JSON
// values in the "value" field (the second op's value is itself
// a JSON object).

static void section3_nested_values() {
    print_section("Section 3: nested JSON values in 'value'");

    const char* doc =
        R"([)"
        R"(  {"op": "add", "path": "/biscuits", "value": []},)"
        R"(  {"op": "add", "path": "/biscuits/0", "value": )"
        R"(   {"name": "Ginger Wale"}})"
        R"(])";

    auto ops = parse_doc_or_report(doc, "3");
    if (ops.empty()) return;

    psp::JsonValue root = parse_or_die("{}");
    apply_doc(root, ops, "3");
}

// ===========================================================================
// Section 4 — Error path: malformed documents
// ===========================================================================
//
// Documents that should fail to parse, with the expected typed
// error. Each one proves the parser's failure detection is
// precise enough to distinguish BadDocument / MissingField /
// WrongType.

static void expect_parse_failure(const std::string& doc,
                                 const char* tag,
                                 JsonPatchError expected) {
    auto r = psp::json_patch::parse_patch_document(doc);
    if (r) {
        std::printf("  %s: expected %s but parse SUCCEEDED\n",
                    tag, std::format("{}", expected).c_str());
        std::exit(1);
    }
    if (r.error() != expected) {
        std::printf("  %s: expected %s, got %s\n",
                    tag,
                    std::format("{}", expected).c_str(),
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    std::printf("  %s: -> %s (as expected)\n",
                tag, std::format("{}", r.error()).c_str());
}

static void section4_errors() {
    print_section("Section 4: malformed documents");

    // Top-level isn't an array.
    expect_parse_failure(R"({"op": "add", "path": "/a", "value": 1})",
                         "4a (object at top)", JsonPatchError::BadDocument);
    // Top-level is a scalar.
    expect_parse_failure(R"(42)",
                         "4b (scalar at top)", JsonPatchError::BadDocument);
    // parse_value_at fails: malformed JSON.
    expect_parse_failure(R"([{"op": "add", "path": )",
                         "4c (malformed JSON)", JsonPatchError::BadDocument);
    // Array element isn't an object.
    expect_parse_failure(R"([1, 2, 3])",
                         "4d (array of numbers)", JsonPatchError::BadDocument);
    // Unknown op name.
    expect_parse_failure(R"([{"op": "frobnicate", "path": "/a"}])",
                         "4e (unknown op name)", JsonPatchError::BadDocument);

    // Missing "path" field on every op kind.
    expect_parse_failure(R"([{"op": "add", "value": 1}])",
                         "4f (add missing path)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "remove"}])",
                         "4g (remove missing path)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "replace", "value": 1}])",
                         "4h (replace missing path)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "test", "value": 1}])",
                         "4i (test missing path)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "move", "from": "/a"}])",
                         "4j (move missing path)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "copy", "from": "/a"}])",
                         "4k (copy missing path)", JsonPatchError::MissingField);

    // Missing "value" / "from" per op.
    expect_parse_failure(R"([{"op": "add", "path": "/a"}])",
                         "4l (add missing value)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "replace", "path": "/a"}])",
                         "4m (replace missing value)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "test", "path": "/a"}])",
                         "4n (test missing value)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "move", "path": "/a"}])",
                         "4o (move missing from)", JsonPatchError::MissingField);
    expect_parse_failure(R"([{"op": "copy", "path": "/a"}])",
                         "4p (copy missing from)", JsonPatchError::MissingField);

    // Wrong type for fields.
    expect_parse_failure(R"([{"op": 1, "path": "/a"}])",
                         "4q (op not a string)", JsonPatchError::WrongType);
    expect_parse_failure(R"([{"op": "add", "path": 42, "value": 1}])",
                         "4r (path not a string)", JsonPatchError::WrongType);
    expect_parse_failure(R"([{"op": "move", "from": 42, "path": "/a"}])",
                         "4s (from not a string)", JsonPatchError::WrongType);
}

// ===========================================================================
// Section 5 — Unknown extra members are silently ignored (RFC 6902 §3)
// ===========================================================================

static void section5_unknown_members() {
    print_section("Section 5: unknown extra members ignored");

    // Both extra members and known members present. The parser
    // should produce a valid op and silently drop the extras.
    const char* doc =
        R"([)"
        R"(  {"op": "add", "path": "/a", "value": 1, "why": "petra", )"
        R"(   "timestamp": 1712345678})"
        R"(])";

    auto r = psp::json_patch::parse_patch_document(doc);
    if (!r) {
        std::printf("  5: parse -> %s (UNEXPECTED)\n",
                    std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    if (r->size() != 1) {
        std::printf("  5: expected 1 op, got %zu\n", r->size());
        std::exit(1);
    }
    if (r->at(0).kind != OpKind::Add) {
        std::printf("  5: expected Add kind\n");
        std::exit(1);
    }
    const auto& add = std::get<AddOp>(r->at(0).data);
    if (add.path != "/a") {
        std::printf("  5: expected /a, got %s\n", add.path.c_str());
        std::exit(1);
    }
    std::printf("  5: parsed AddOp{path=/a} ignoring 'why' + 'timestamp'\n");
}

// ===========================================================================
// Section 6 — Empty documents
// ===========================================================================

static void section6_empty_documents() {
    print_section("Section 6: empty documents");

    // Empty array is a valid patch document that applies zero
    // operations. parse_patch_document should return an empty
    // vector; patch should be a no-op.
    auto r1 = psp::json_patch::parse_patch_document(std::string_view{"[]"});
    if (!r1) {
        std::printf("  6a: parse -> %s (UNEXPECTED)\n",
                    std::format("{}", r1.error()).c_str());
        std::exit(1);
    }
    if (!r1->empty()) {
        std::printf("  6a: expected empty vector, got %zu ops\n", r1->size());
        std::exit(1);
    }
    psp::JsonValue root = parse_or_die(R"({"k": 1})");
    auto patch_r = psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{*r1});
    if (!patch_r) {
        std::printf("  6a: patch -> %s (UNEXPECTED)\n",
                    std::format("{}", patch_r.error()).c_str());
        std::exit(1);
    }
    std::printf("  6a: empty doc -> empty ops vector, tree unchanged: %s\n",
                psp::json_to_string(root).c_str());

    // Empty string fails to parse (BadDocument).
    expect_parse_failure("", "6b (empty string)", JsonPatchError::BadDocument);
}

// ===========================================================================
// Section 7 — Type probes
// ===========================================================================

static void section7_probes() {
    print_section("Section 7: sizeof / feature probes");

    std::printf("  sizeof(JsonPatchError)                          = %zu\n",
                sizeof(JsonPatchError));
    std::printf("  sizeof(JsonPatchOp)                             = %zu\n",
                sizeof(JsonPatchOp));
    std::printf("  sizeof(std::vector<JsonPatchOp>)                 = %zu\n",
                sizeof(std::vector<JsonPatchOp>));
    std::printf("  sizeof(std::expected<std::vector<JsonPatchOp>,) = %zu\n",
                sizeof(std::expected<std::vector<JsonPatchOp>,
                                     JsonPatchError>));
    std::printf("  Public-header roster (v0.13.0):\n");
    std::printf("    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)\n");
    std::printf("    <psp_span/parser.h>   : psp::parse_int/parse_double + cursor + JSON scalars + ParseError (+ DuplicateKey in v0.10.0)\n");
    std::printf("    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string + operator==/!= (v0.12.0)\n");
    std::printf("    <psp_span/json_ext.h> : psp::json_pointer::split / to_string / resolve / resolve_mut\n");
    std::printf("                             + psp::json_patch::patch (RFC 6902 §1 engine)\n");
    std::printf("                             + psp::json_patch::parse_patch_document (RFC 6902 §3 wire-format parser) (NEW in v0.13.0)\n");
    std::printf("                             + ::JsonPatchOp (RFC 6902 tagged union, 6 ops)\n");
    std::printf("                             + ::JsonPatchError (13 enumerators, +3 in v0.13.0) + std::formatter spec\n");

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
// Section 8 — Backwards compat: Jul 21 (Pointer) + Jul 22 (Patch)
//             consumers still compile and run unchanged against
//             v0.13.0
// ===========================================================================
//
// We re-run a subset of the Jul 21 / Jul 22 surface here to prove
// the v0.13.0 header is a strict superset.

static psp::JsonValue resolve_through_pointer(psp::JsonValue& root,
                                              const std::string& ptr) {
    auto r = psp::json_pointer::resolve(ptr, root);
    if (!r) {
        std::printf("  INTERNAL FAIL: resolve(\"%s\") -> %s\n",
                    ptr.c_str(), std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    return std::move(**r);
}

static void section8_backcompat() {
    print_section("Section 8: backwards compat — Pointer + Patch halves from v0.12.0 unchanged");

    // (Pointer half from v0.11.0)
    psp::JsonValue doc = parse_or_die(R"({"a": {"b": [10, 20, 30]}})");
    auto b0 = resolve_through_pointer(doc, "/a/b/0");
    if (!std::holds_alternative<std::int64_t>(b0.value)
        || std::get<std::int64_t>(b0.value) != 10) {
        std::printf("  8a: resolve(/a/b/0) != 10\n");
        std::exit(1);
    }
    std::printf("  8a: resolve(/a/b/0) = 10 (Pointer half unchanged)\n");

    // (Patch half from v0.12.0 — directly use JsonPatchOp builders)
    psp::JsonValue tree = parse_or_die(R"({"k": 1})");
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{ReplaceOp{"/k", parse_or_die("99")}});
    auto r = psp::json_patch::patch(tree,
        std::span<const JsonPatchOp>{ops});
    if (!r) {
        std::printf("  8b: patch -> %s\n", std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    auto k = resolve_through_pointer(tree, "/k");
    if (std::get<std::int64_t>(k.value) != 99) {
        std::printf("  8b: tree /k != 99\n");
        std::exit(1);
    }
    std::printf("  8b: patch + resolve hand-built JsonPatchOp -> 99 (Patch half unchanged)\n");
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-07-23 — psp_span_lib v0.13.0 consumer: "
                "psp::json_patch::parse_patch_document (RFC 6902 §3)\n");

    section1_canonical_example();
    section2_all_six_ops();
    section3_nested_values();
    section4_errors();
    section5_unknown_members();
    section6_empty_documents();
    section7_probes();
    section8_backcompat();

    std::printf("\n[psp_json_patch_parser: all 8 sections complete]\n");
    return 0;
}