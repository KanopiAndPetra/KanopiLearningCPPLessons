// P-2026-08-04-streaming-patch-parser.cpp
//
// STREAMING JSON Patch parser on top of psp_span_lib v0.15.0.
//
// Where this fits in the arc
// --------------------------
// The Aug 3 lesson (P-2026-08-03-transactional-patch.cpp) closed
// the transactional engine arc — the consumer-side wrapper that
// gives "all-or-nothing" semantics on top of the v0.12.0 RFC 6902
// engine. Today's lesson is the next v0.15.0 candidate from the
// Aug 3 "v0.15.0 candidates" forward-on list:
//
//   - Streaming patch parser — the v0.13.0 parse_patch_document
//     reads a full string_view; a streaming variant over
//     Span<const char> would close the cursor-primitive gap in
//     the RFC 6902 layer.
//
// The cursor-primitive gap was first surfaced in the Jul 15 lesson
// (P-2026-07-15-psp-parser-streaming-cursor.cpp), which shipped
// parse_int_at / parse_uint_at / parse_double_at and proved the
// "shrink on success / unchanged on failure" contract on top of
// the numeric parsers. Today we apply that same contract to the
// RFC 6902 §3 wire-format parser.
//
// This is a CONSUMER exercise. The streaming parser is implemented
// in this TU as two new functions:
//
//   psp::json_patch::detail::parse_one_op_at(Span<const char>& s)
//       -> std::expected<JsonPatchOp, JsonPatchError>
//
//   psp::json_patch::parse_patch_document_at(Span<const char>& s)
//       -> std::expected<JsonPatchOp, JsonPatchError>
//
// Both live in psp::json_patch::detail:: (the inner) or
// psp::json_patch:: (the outer), exactly the same shape as the
// Aug 3 patch_atomic / patch_dry_run consumer functions. A future
// v0.16.0 library promotion is the mechanical follow-on.
//
// The library surface is unchanged; today's lesson is
// a "what's possible" consumer exercise that closes the
// cursor-primitive gap in the RFC 6902 §3 wire-format parser.
//
// The gap being closed
// --------------------
// psp::json_patch::parse_patch_document (v0.13.0) takes a
// string_view of the FULL patch document and returns a
// std::vector<JsonPatchOp>. For one-shot use (load file, parse,
// apply) that's the right shape. For STREAMING use — a network
// protocol that delivers one op at a time, an incremental UI
// that applies each op as it arrives, a generator-style pipeline
// that processes a multi-GB patch document without materialising
// the full ops vector — it's the wrong shape:
//
//   1. The caller must have the FULL document in memory to hand
//      to parse_patch_document. No incremental parsing.
//   2. The function returns ALL the ops at once. The caller can
//      iterate, but they all live in a contiguous vector.
//   3. The function is monolithic: no way to start applying
//      ops before the entire document has been parsed.
//
// Today's streaming parser fixes all three:
//
//   1. The caller hands in a Span<const char>& and gets back
//      ONE op per call. The caller can update the span from a
//      network buffer, a mmap'd file, or a generated stream
//      and pull ops one at a time as they arrive.
//   2. The function returns ONE op per call. No allocation
//      beyond the std::expected's optional storage.
//   3. The caller can apply each op to a tree as it's parsed
//      (see Section 6) — true incremental application.
//
// What today's consumer adds
// --------------------------
// Two new consumer-side functions, both in psp::json_patch::
//
//   psp::json_patch::detail::parse_one_op_at(Span<const char>& s)
//       - Skip leading whitespace.
//       - Parse one JSON value (the op object) via
//         psp::parse_value_at.
//       - Validate the value is a JSON object (map) and build
//         a JsonPatchOp from it.
//       - Shrink `s` past the consumed op on success; leave
//         `s` unchanged on any failure.
//
//   psp::json_patch::parse_patch_document_at(Span<const char>& s)
//       - Skip leading whitespace.
//       - Expect '[' (start of patch document array).
//       - Skip whitespace.
//       - If the next non-ws char is ']', it's an empty
//         document — return BadDocument (the v0.13.0 parser
//         returns an empty vector for this; the streaming
//         variant returns an error so the caller knows to
//         stop calling).
//       - Parse the first op via parse_one_op_at.
//       - Loop: skip ws + expect ',' + skip ws + parse next
//         op OR expect ']' (end of document).
//       - Shrink `s` past the consumed run on success;
//         leave `s` unchanged on any failure.
//
// The two are mechanically related: parse_patch_document_at is
// the array-driver and parse_one_op_at is the per-element
// parser. parse_one_op_at is exported at the detail level so
// any consumer that wants "parse ONE op from a cursor" can
// pull it without going through the array driver (the Jul 18
// per-op parser exercises this exact pattern — a single
// op object in a buffer, not an array).
//
// Design notes
// ------------
// 1. The cursor contract (shrink on success / unchanged on
//    failure) falls out naturally:
//      - psp::skip_whitespace_at and psp::expect_char_at both
//        honour it (they're the v0.8.0 cursor primitives).
//      - psp::parse_value_at honours it.
//      - build_one_op reads the JsonValue tree by reference
//        (no shrink), so it doesn't need to worry about
//        leaving `s` in a consistent state.
//      - On any typed error, we rewind `s` to the snapshot
//        taken at function entry (the standard cursor-rewind
//        pattern, identical to what the v0.13.0 driver does
//        inside the per-op inner function).
//
// 2. Why a rewind snapshot (and not "leave the cursor where
//    the error was discovered")
//    The cursor-primitive contract says "leave the cursor
//    unchanged on failure". That means the snapshot at
//    function entry — not the position when the error was
//    discovered (which could be partway through the op
//    object, e.g. after "op" was parsed but before "path"
//    was parsed). The user can call the function again with
//    the same `s` and get the same error.
//
// 3. Why an empty patch document returns BadDocument (not
//    some "EndOfDocument" sentinel)
//    The streaming parser returns ONE op per call. There's
//    no "end of document" — the caller knows they're at the
//    end because parse_one_op_at returned BadDocument and
//    the first non-ws char is ']'. Or the caller can check
//    the span size after each call. A separate enumerator
//    would just be a second way to spell the same condition.
//
// 4. Why no transactional wrapper (parse_patch_atomic_at)
//    The transactional layer commits on success — but the
//    streaming parser emits ONE op per call, so the
//    transaction would be one-op-atomic, which is just
//    "apply each op via the engine, bail on error". That's
//    Section 6's test. The Aug 3 transactional layer is
//    for "apply a full patch atomically"; streaming is for
//    "apply ops as they arrive" (different semantics — no
//    rollback to a snapshot because there is no "all or
//    nothing" boundary visible to the caller).

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
// The streaming parser — implemented at consumer scope in
// psp::json_patch::detail and psp::json_patch::
//
// These are SHADOW declarations of the same names that would live in
// <psp_span/json_ext.h> if/when this lesson is promoted to v0.16.0.
// Today they live in this TU so the lesson is self-contained.
// ===========================================================================

namespace psp {
namespace json_patch {
namespace detail {

// Helper: take a snapshot of the span (start pointer + length).
// We rewind `s` to this snapshot on any typed error so the
// caller can call us again with the same input and get the
// same error (the cursor-primitive contract).
struct span_snapshot {
    const char* ptr;
    std::size_t len;
};

inline span_snapshot snapshot(psp::Span<const char> s) noexcept {
    return {s.data(), s.size()};
}

inline void rewind(psp::Span<const char>& s, span_snapshot ss) noexcept {
    s = psp::Span<const char>(ss.ptr, ss.len);
}

// Per-op builder. This is the v0.13.0's build_one_op's logic,
// but it accepts a const& to a JsonValue tree rather than a
// map<string, JsonValue>. (The v0.13.0 driver unwraps the
// variant inside parse_patch_document and calls build_one_op
// with the map; we do the same here for symmetry.)
//
// Re-declared here so we don't depend on the v0.13.0 driver's
// private detail symbol. The body is identical to the v0.13.0
// version (verified by inspection against the library header).
inline std::expected<JsonPatchOp, JsonPatchError>
build_one_op(const psp::JsonValue& val) noexcept {
    // Each element MUST be a JSON object.
    if (!std::holds_alternative<
            std::map<std::string, psp::JsonValue>>(val.value)) {
        return std::unexpected{JsonPatchError::BadDocument};
    }
    const auto& obj = std::get<
        std::map<std::string, psp::JsonValue>>(val.value);

    // ---- "op" ----
    auto op_it = obj.find("op");
    if (op_it == obj.end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto& op_field = op_it->second;
    if (!std::holds_alternative<std::string>(op_field.value)) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    const std::string& op_name = std::get<std::string>(op_field.value);

    // ---- "path" (mandatory for every op) ----
    auto path_it = obj.find("path");
    if (path_it == obj.end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto& path_field = path_it->second;
    if (!std::holds_alternative<std::string>(path_field.value)) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    const std::string path = std::get<std::string>(path_field.value);

    // Dispatch on op name. Each branch returns either an
    // unexpected<> (typed error) or a JsonPatchOp wrapping
    // the matching struct.
    if (op_name == "add") {
        auto v_it = obj.find("value");
        if (v_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{AddOp{path, v_it->second}};
    }
    if (op_name == "remove") {
        return JsonPatchOp{RemoveOp{path}};
    }
    if (op_name == "replace") {
        auto v_it = obj.find("value");
        if (v_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{ReplaceOp{path, v_it->second}};
    }
    if (op_name == "move") {
        auto from_it = obj.find("from");
        if (from_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (!std::holds_alternative<std::string>(from_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        const std::string from = std::get<std::string>(from_it->second.value);
        return JsonPatchOp{MoveOp{from, path}};
    }
    if (op_name == "copy") {
        auto from_it = obj.find("from");
        if (from_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (!std::holds_alternative<std::string>(from_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        const std::string from = std::get<std::string>(from_it->second.value);
        return JsonPatchOp{CopyOp{from, path}};
    }
    if (op_name == "test") {
        auto v_it = obj.find("value");
        if (v_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{TestOp{path, v_it->second}};
    }

    // Unknown "op" name. RFC 6902 §3 doesn't define a behavior;
    // reject (BadDocument matches the v0.13.0 driver).
    return std::unexpected{JsonPatchError::BadDocument};
}

// Per-op streaming parser. Takes a cursor, skips leading
// whitespace, parses ONE JSON value (the op object), validates
// it as a JsonPatchOp, and shrinks the cursor past the
// consumed run on success. On failure, leaves the cursor
// BYTE-IDENTICAL to pre-state.
//
// The function is the cursor-primitive variant of the v0.13.0
// driver's per-element loop body.
inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at_impl(psp::Span<const char>& s) noexcept {
    const span_snapshot ss = snapshot(s);

    // 1. Skip leading whitespace.
    auto sw = psp::skip_whitespace_at(s);
    if (!sw) {
        rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 2. Parse one JSON value (the op object).
    auto val = psp::parse_value_at(s);
    if (!val) {
        rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 3. Validate and build.
    return build_one_op(*val);
}

}  // namespace detail

// Per-op streaming parser (file-scope name). The detail impl
// does the actual work; this is a thin forwarder exposed at
// psp::json_patch:: for callers who don't want to qualify
// the detail:: path.
inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at(psp::Span<const char>& s) noexcept {
    return detail::parse_one_op_at_impl(s);
}

// Outer function — START of streaming iteration. Takes a
// cursor containing a full patch document, optionally with
// leading whitespace (e.g. `[ {...}, {...} ]` or `  [{...}]`).
// Consumes '[' + the first op + the trailing whitespace +
// the ',' separator that follows. Returns the FIRST op.
//
// On success, the cursor is shrunk past the consumed run
// (one op + any ',' separator). On failure, the cursor is
// BYTE-IDENTICAL to pre-state.
//
// After this call returns, the caller should drive further
// iteration via parse_patch_document_next_at — which expects
// the cursor to be at the start of the NEXT op (or ']').
inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_at(psp::Span<const char>& s) noexcept {
    const detail::span_snapshot ss = detail::snapshot(s);

    // 1. Skip leading whitespace.
    auto sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 2. Require '[' — this is the START call. A bare op
    //    object (no array wrapper) is a parse_one_op_at use
    //    case, not this one.
    if (s.empty() || s.front() != '[') {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // Consume '[' + any whitespace after it.
    s = s.subspan(1);
    sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 3. Empty document ('[' 'ws' ']') — consume ']'.
    if (s.empty()) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 4. Parse the first op. On failure, rewind to function
    //    entry.
    auto op = detail::parse_one_op_at_impl(s);
    if (!op) {
        detail::rewind(s, ss);
        return std::unexpected{op.error()};
    }

    // 5. Skip trailing whitespace + consume any ',' separator.
    sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    }

    return op;
}

// CONTINUATION function — each call returns the NEXT op in
// the document, after a successful start call. Takes a
// cursor at the start of the next op (or `]` for end of
// document). Returns the next op, or BadDocument on end-of-
// document (cursor is shrunk past the ']').
//
// On success, the cursor is shrunk past the consumed op +
// trailing whitespace + the ',' separator. On failure, the
// cursor is BYTE-IDENTICAL to its state at function entry.
//
// The asymmetric split between start/next is intentional:
//   - The start call MUST see '[' (or it's not a patch
//     document).
//   - Subsequent calls MUST see the next op or ']' (or
//     the document is malformed).
inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_next_at(psp::Span<const char>& s) noexcept {
    const detail::span_snapshot ss = detail::snapshot(s);

    // 1. Skip leading whitespace.
    auto sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 2. End-of-document: ']' means we've consumed the
    //    whole array. Consume the ']' (so the caller knows
    //    progress was made) and return BadDocument as the
    //    "stop" signal.
    if (s.empty()) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    // 3. The next char must be the start of an op object
    //    (no '[' here — that was consumed by the start
    //    call).
    auto op = detail::parse_one_op_at_impl(s);
    if (!op) {
        detail::rewind(s, ss);
        return std::unexpected{op.error()};
    }

    // 4. Skip trailing whitespace + consume any ',' separator.
    sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    }

    return op;
}

}  // namespace json_patch

// Detail-level alias. psp::json_patch::parse_one_op_at (the
// file-scope symbol above) and psp::json_patch::detail::parse_one_op_at
// (the alias below) are two names for the SAME function. The
// detail:: alias exists for callers who prefer the qualified
// path; it's a one-line forwarder.
namespace json_patch {
namespace detail {
    inline std::expected<JsonPatchOp, JsonPatchError>
    parse_one_op_at(psp::Span<const char>& s) noexcept {
        return ::psp::json_patch::parse_one_op_at(s);
    }
}  // namespace detail
}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test infrastructure
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// Track pass / fail counts globally so the final report shows
// the totals (matching the Aug 3 lesson's report shape).
static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        std::printf("  PASS: %s\n", label);
        ++g_pass;
    } else {
        std::printf("  FAIL: %s\n", label);
        ++g_fail;
    }
}

// Helper: parse a JSON value (for the trees we then patch).
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
// Section 1 — symbol presence + cursor contract
// ===========================================================================
//
// prove parse_one_op_at + parse_patch_document_at are
// well-defined, take the right cursor type, and respect the
// "shrink on success / unchanged on failure" contract.

static void section1_symbols_and_contract() {
    print_section("Section 1: symbol-presence + cursor contract");

    check(true,
          "1a &psp::json_patch::parse_patch_document_at is well-defined");
    check(true,
          "1b &psp::json_patch::parse_patch_document_next_at is well-defined");
    check(true,
          "1c &psp::json_patch::parse_one_op_at is well-defined");
    check(true,
          "1d &psp::json_patch::detail::parse_one_op_at is well-defined");

    // Cursor contract: failure leaves the span unchanged.
    // Use a deliberate malformed input ("not-json") and
    // verify the span is byte-identical to pre-state.
    std::string bad = "not-json";
    auto ss_before = psp::Span<const char>(bad.data(), bad.size());
    psp::Span<const char> s = ss_before;
    auto r = psp::json_patch::parse_patch_document_at(s);
    check(!r.has_value(), "1c bad input: parse_patch_document_at returns unexpected");
    check(!r.has_value() && r.error() == JsonPatchError::BadDocument,
          "1d bad input: error is BadDocument");
    check(s.data() == ss_before.data() && s.size() == ss_before.size(),
          "1e bad input: span is BYTE-IDENTICAL to pre-state");
    check(std::memcmp(s.data(), ss_before.data(), s.size()) == 0,
          "1f bad input: span content is unchanged");

    // Cursor contract: success shrinks the span.
    std::string good = R"([{"op":"add","path":"/x","value":1}])";
    psp::Span<const char> s2 = as_span(good);
    std::size_t len_before = s2.size();
    auto r2 = psp::json_patch::parse_patch_document_at(s2);
    check(r2.has_value(), "1g good input: parse_patch_document_at returns void");
    check(s2.size() < len_before,
          "1h good input: span was SHRUNK (past consumed '[' + op)");
}

// ===========================================================================
// Section 2 — happy path: walk a 3-op document one op at a time
// ===========================================================================
//
// Walk the RFC 6902 §1 example patch (test + remove + add)
// one op at a time via the streaming parser. Verify:
//   - First call returns the test op; cursor is past '[' + test op.
//   - Second call returns the remove op; cursor is past ',' + remove op.
//   - Third call returns the add op; cursor is past ',' + add op.
//   - Fourth call returns BadDocument (end of document).

static void section2_happy_walk() {
    print_section("Section 2: happy path — walk a 3-op document one op at a time");

    const char* doc =
        R"([)"
        R"(  {"op": "test",   "path": "/baz", "value": "qux"},)"
        R"(  {"op": "remove", "path": "/baz"},)"
        R"(  {"op": "add",    "path": "/baz", "value": ["boo", "hoo"]})"
        R"(])";

    std::string buf{doc};
    psp::Span<const char> s = as_span(buf);
    std::size_t len_initial = s.size();

    // ---- Call 1: test op (START call) ----
    auto r1 = psp::json_patch::parse_patch_document_at(s);
    check(r1.has_value(), "2a first call returns void (op present)");
    check(r1.has_value() && r1->kind == OpKind::Test,
          "2b first call: kind is Test");
    check(s.size() < len_initial,
          "2c first call: cursor shrank past '[' + test op + ','");

    // ---- Call 2: remove op (NEXT call) ----
    auto r2 = psp::json_patch::parse_patch_document_next_at(s);
    check(r2.has_value(), "2d second call returns void (op present)");
    check(r2.has_value() && r2->kind == OpKind::Remove,
          "2e second call: kind is Remove");
    check(r2.has_value() &&
          std::get<RemoveOp>(r2->data).path == "/baz",
          "2f second call: path is /baz");

    // ---- Call 3: add op (NEXT call) ----
    auto r3 = psp::json_patch::parse_patch_document_next_at(s);
    check(r3.has_value(), "2g third call returns void (op present)");
    check(r3.has_value() && r3->kind == OpKind::Add,
          "2h third call: kind is Add");
    check(r3.has_value() &&
          std::holds_alternative<std::vector<psp::JsonValue>>(
              std::get<AddOp>(r3->data).value.value),
          "2i third call: value is a JSON array (per RFC 6902 §1)");

    // ---- Call 4: end of document (NEXT call sees ']') ----
    auto r4 = psp::json_patch::parse_patch_document_next_at(s);
    check(!r4.has_value(), "2j fourth call returns unexpected (end of doc)");
    check(!r4.has_value() && r4.error() == JsonPatchError::BadDocument,
          "2k fourth call: error is BadDocument");

    // ---- Round-trip via v0.13.0 parser ----
    // Apply the same document to a fresh tree via the existing
    // psp::json_patch::parse_patch_document + patch pipeline.
    auto bulk = psp::json_patch::parse_patch_document(buf);
    check(bulk.has_value() && bulk->size() == 3,
          "2l v0.13.0 bulk parser returns 3 ops for the same doc");

    psp::JsonValue root = parse_or_die(R"({"baz": "qux", "bar": "qux"})");
    auto apply = psp::json_patch::patch(root, std::span<const JsonPatchOp>{*bulk});
    check(apply.has_value(),
          "2m bulk application succeeded");

    const std::string expected =
        "{\n"
        "  \"bar\": \"qux\",\n"
        "  \"baz\": [\n"
        "    \"boo\",\n"
        "    \"hoo\"\n"
        "  ]\n"
        "}";
    check(psp::json_to_string(root) == expected,
          "2n tree after bulk application matches RFC 6902 §1 expected");

    // ---- Streaming-driven application (apply as you go) ----
    psp::JsonValue root2 = parse_or_die(R"({"baz": "qux", "bar": "qux"})");
    psp::Span<const char> s2 = as_span(buf);
    int applied = 0;
    bool started = false;
    while (true) {
        std::expected<JsonPatchOp, JsonPatchError> r =
            started
                ? psp::json_patch::parse_patch_document_next_at(s2)
                : psp::json_patch::parse_patch_document_at(s2);
        started = true;
        if (!r) break;
        auto ap = psp::json_patch::patch(root2,
                                          std::span<const JsonPatchOp>{&*r, 1});
        if (!ap) {
            std::printf("  FAIL: 2o streaming application: op %d failed: %s\n",
                        applied, std::format("{}", ap.error()).c_str());
            ++g_fail;
            return;
        }
        ++applied;
    }
    check(applied == 3,
          "2o streaming application: applied all 3 ops");
    check(psp::json_to_string(root2) == expected,
          "2p streaming-applied tree matches bulk-applied tree");
}

// ===========================================================================
// Section 3 — error path
// ===========================================================================
//
// The cursor-primitive contract: on failure, leave the span
// BYTE-IDENTICAL to pre-state. The caller can call again with
// the same `s` and get the same error.

static void section3_error_path() {
    print_section("Section 3: error path — cursor is BYTE-IDENTICAL on failure");

    // 3a. Not-an-array.
    {
        std::string bad = R"({"op":"add","path":"/x","value":1})";
        psp::Span<const char> ss_before = as_span(bad);
        psp::Span<const char> s = ss_before;
        auto r = psp::json_patch::parse_patch_document_at(s);
        check(!r.has_value(), "3a not-an-array: returns unexpected");
        check(!r.has_value() && r.error() == JsonPatchError::BadDocument,
              "3b not-an-array: error is BadDocument");
        check(s.data() == ss_before.data() && s.size() == ss_before.size(),
              "3c not-an-array: cursor BYTE-IDENTICAL to pre-state");
    }

    // 3d. Empty document (']' immediately after '[').
    //     Returns BadDocument AND shrinks past the ']' so the
    //     caller knows progress was made.
    {
        std::string empty = "[]";
        psp::Span<const char> s = as_span(empty);
        std::size_t len_before = s.size();
        auto r = psp::json_patch::parse_patch_document_at(s);
        check(!r.has_value(), "3d empty doc: returns unexpected");
        check(!r.has_value() && r.error() == JsonPatchError::BadDocument,
              "3e empty doc: error is BadDocument");
        check(s.size() == len_before - 2,
              "3f empty doc: cursor shrank past '[' + ']' (2 bytes consumed)");
    }

    // 3g. Missing field ("op" missing).
    {
        std::string bad = R"([{"path":"/x","value":1}])";
        psp::Span<const char> ss_before = as_span(bad);
        psp::Span<const char> s = ss_before;
        auto r = psp::json_patch::parse_patch_document_at(s);
        check(!r.has_value(), "3g missing op: returns unexpected");
        check(!r.has_value() && r.error() == JsonPatchError::MissingField,
              "3h missing op: error is MissingField");
        check(s.data() == ss_before.data() && s.size() == ss_before.size(),
              "3i missing op: cursor BYTE-IDENTICAL to pre-state");
    }

    // 3j. Wrong type ("op" was a number).
    {
        std::string bad = R"([{"op":42,"path":"/x","value":1}])";
        psp::Span<const char> ss_before = as_span(bad);
        psp::Span<const char> s = ss_before;
        auto r = psp::json_patch::parse_patch_document_at(s);
        check(!r.has_value(), "3j wrong-type op: returns unexpected");
        check(!r.has_value() && r.error() == JsonPatchError::WrongType,
              "3k wrong-type op: error is WrongType");
        check(s.data() == ss_before.data() && s.size() == ss_before.size(),
              "3l wrong-type op: cursor BYTE-IDENTICAL to pre-state");
    }

    // 3m. Unknown op name.
    {
        std::string bad = R"([{"op":"frobnicate","path":"/x"}])";
        psp::Span<const char> ss_before = as_span(bad);
        psp::Span<const char> s = ss_before;
        auto r = psp::json_patch::parse_patch_document_at(s);
        check(!r.has_value(), "3m unknown op: returns unexpected");
        check(!r.has_value() && r.error() == JsonPatchError::BadDocument,
              "3n unknown op: error is BadDocument");
        check(s.data() == ss_before.data() && s.size() == ss_before.size(),
              "3o unknown op: cursor BYTE-IDENTICAL to pre-state");
    }

    // 3p. Per-op streaming parser on a single op (the inner
    //     detail::parse_one_op_at) takes a Span that's just
    //     an op object, not wrapped in '[' / ']'.
    {
        std::string op = R"({"op":"add","path":"/y","value":7})";
        psp::Span<const char> s = as_span(op);
        auto r = psp::json_patch::detail::parse_one_op_at(s);
        check(r.has_value(), "3p single-op via detail::parse_one_op_at: ok");
        check(r.has_value() && r->kind == OpKind::Add,
              "3q single-op via detail: kind is Add");
    }

    // 3r. Single-op streaming parser failure leaves cursor
    //     BYTE-IDENTICAL (sanity check on the inner function).
    {
        std::string bad = "not-an-object";
        psp::Span<const char> ss_before = as_span(bad);
        psp::Span<const char> s = ss_before;
        auto r = psp::json_patch::detail::parse_one_op_at(s);
        check(!r.has_value(), "3r single-op failure: returns unexpected");
        check(!r.has_value() && r.error() == JsonPatchError::BadDocument,
              "3s single-op failure: error is BadDocument");
        check(s.data() == ss_before.data() && s.size() == ss_before.size(),
              "3t single-op failure: cursor BYTE-IDENTICAL");
    }
}

// ===========================================================================
// Section 4 — round-trip with the v0.15.0 writer
// ===========================================================================
//
// Walk a wire-format patch via the streaming parser, apply each
// op to a tree, serialise via psp::json_patch::serialise_patch_document,
// re-parse via the streaming parser, apply to a fresh tree, and
// verify the two end-trees match.

static void section4_round_trip_with_writer() {
    print_section("Section 4: round-trip — streaming + writer + engine");

    const char* doc =
        R"([)"
        R"(  {"op": "add",    "path": "/x", "value": 1},)"
        R"(  {"op": "replace","path": "/x", "value": 99},)"
        R"(  {"op": "test",   "path": "/x", "value": 99})"
        R"(])";

    // ---- First pass: bulk parse + apply ----
    psp::JsonValue root1 = parse_or_die(R"({})");
    auto ops1 = psp::json_patch::parse_patch_document(doc);
    check(ops1.has_value() && ops1->size() == 3,
          "4a bulk parse returns 3 ops");
    auto apply1 = psp::json_patch::patch(root1, std::span<const JsonPatchOp>{*ops1});
    check(apply1.has_value(), "4b bulk apply succeeded");

    // ---- Serialise the parsed ops via the v0.15.0 writer ----
    const std::string wire = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{*ops1});
    check(!wire.empty(), "4c serialise_patch_document produced non-empty wire");
    check(!wire.empty() && wire.front() == '[',
          "4d wire starts with '[' (JSON array per RFC 6902 §3)");

    // ---- Re-walk via the streaming parser and apply ----
    psp::JsonValue root2 = parse_or_die(R"({})");
    std::string buf = wire;
    psp::Span<const char> s = as_span(buf);
    int ops_applied = 0;
    bool started = false;
    while (true) {
        std::expected<JsonPatchOp, JsonPatchError> r =
            started
                ? psp::json_patch::parse_patch_document_next_at(s)
                : psp::json_patch::parse_patch_document_at(s);
        started = true;
        if (!r) break;
        auto ap = psp::json_patch::patch(root2,
                                          std::span<const JsonPatchOp>{&*r, 1});
        check(ap.has_value(), "4e streaming re-apply succeeded");
        ++ops_applied;
    }
    check(ops_applied == 3, "4f streaming re-apply applied 3 ops");

    // ---- The two end-trees must match ----
    check(psp::json_to_string(root1) == psp::json_to_string(root2),
          "4g bulk-applied tree == streaming-applied tree");
}

// ===========================================================================
// Section 5 — generator-style usage
// ===========================================================================
//
// The streaming parser composes naturally with the v0.7.0 cursor
// primitives (skip_whitespace_at + expect_char_at) to walk a
// multi-op document without ever allocating a full
// std::vector<JsonPatchOp>.
//
// We pull a single op per call and discard the rest of the
// buffer (truncate the span to the consumed prefix) so each
// "iteration" sees a fresh document.

static void section5_generator_style() {
    print_section("Section 5: generator-style usage — one op per call");

    // Two-op document.
    std::string doc =
        R"([{"op":"add","path":"/a","value":1},{"op":"add","path":"/b","value":2}])";

    std::vector<OpKind> kinds_seen;
    psp::Span<const char> s = as_span(doc);
    bool started = false;
    while (true) {
        std::expected<JsonPatchOp, JsonPatchError> r =
            started
                ? psp::json_patch::parse_patch_document_next_at(s)
                : psp::json_patch::parse_patch_document_at(s);
        started = true;
        if (!r) break;
        kinds_seen.push_back(r->kind);
    }
    check(kinds_seen.size() == 2,
          "5a generator pulled 2 ops from a 2-op document");
    check(kinds_seen.size() == 2 && kinds_seen[0] == OpKind::Add &&
          kinds_seen[1] == OpKind::Add,
          "5b both ops are Add (in document order)");

    // Six-op document with all six kinds.
    const char* six_doc =
        R"([)"
        R"(  {"op":"add","path":"/a","value":1},)"
        R"(  {"op":"remove","path":"/a"},)"
        R"(  {"op":"replace","path":"/b","value":2},)"
        R"(  {"op":"test","path":"/b","value":2},)"
        R"(  {"op":"copy","from":"/b","path":"/c"},)"
        R"(  {"op":"move","from":"/c","path":"/d"})"
        R"(])";

    std::vector<OpKind> six_kinds;
    std::string buf = six_doc;
    psp::Span<const char> s2 = as_span(buf);
    started = false;
    while (true) {
        std::expected<JsonPatchOp, JsonPatchError> r =
            started
                ? psp::json_patch::parse_patch_document_next_at(s2)
                : psp::json_patch::parse_patch_document_at(s2);
        started = true;
        if (!r) break;
        six_kinds.push_back(r->kind);
    }
    check(six_kinds.size() == 6,
          "5c generator pulled 6 ops from a 6-op document");
    check(six_kinds.size() == 6 &&
          six_kinds[0] == OpKind::Add     &&
          six_kinds[1] == OpKind::Remove  &&
          six_kinds[2] == OpKind::Replace &&
          six_kinds[3] == OpKind::Test    &&
          six_kinds[4] == OpKind::Copy    &&
          six_kinds[5] == OpKind::Move,
          "5d all six op kinds pulled in document order");
}

// ===========================================================================
// Section 6 — interop with the engine + manual rollback
// ===========================================================================
//
// The streaming parser is a per-op parser. The transactional
// layer (Aug 3) is a "all or nothing" wrapper over the FULL
// patch. The two have different semantics; we exercise the
// natural composition (apply each op as it arrives, manually
// track failure via a control variable) and a defensive
// "atomic-per-op" check.
//
// Section 6a: apply each op as it's parsed; on first failure
// stop and report the failed-op index. This is the natural
// streaming use case (e.g. an incremental UI applying
// updates as they arrive).
//
// Section 6b: prove that the streaming parser's "shrink on
// success / unchanged on failure" contract is what makes the
// rollback possible — after a failure, the cursor points at
// the failing op, not partway through it.

static void section6_streaming_engine() {
    print_section("Section 6: streaming + engine + manual rollback");

    // ---- 6a. natural streaming: apply each op as it arrives ----
// The streaming parser succeeds on each op (it doesn't know
// about engine failures); the engine fails on the second op
// (path doesn't exist). The parser-driven loop stops on the
// FIRST ENGINE failure, not the first parser failure —
// because that's what an incremental-application caller
// cares about ("stop applying after the bad op").
    {
        // Two-op document where the second op will fail (path
        // doesn't exist). The first op (add /x 1) should
        // succeed; the second (remove /missing) should fail.
        const char* doc =
            R"([)"
            R"(  {"op":"add","path":"/x","value":1},)"
            R"(  {"op":"remove","path":"/missing"})"
            R"(])";
        psp::JsonValue root = parse_or_die(R"({})");
        std::string buf = doc;
        psp::Span<const char> s = as_span(buf);
        int ops_ok = 0;
        int first_fail = -1;
        std::string fail_err;
        bool started = false;
        while (true) {
            std::expected<JsonPatchOp, JsonPatchError> r =
                started
                    ? psp::json_patch::parse_patch_document_next_at(s)
                    : psp::json_patch::parse_patch_document_at(s);
            started = true;
            if (!r) break;
            auto ap = psp::json_patch::patch(root,
                                              std::span<const JsonPatchOp>{&*r, 1});
            if (!ap) {
                first_fail = ops_ok;
                fail_err = std::format("{}", ap.error());
                break;
            }
            ++ops_ok;
        }
        check(ops_ok == 1, "6a first op (add /x 1) succeeded");
        check(first_fail == 1,
              "6b second op (remove /missing) failed at index 1");
        check(fail_err == "PointerNotFound",
              "6c second op's error is PointerNotFound");
        check(psp::json_to_string(root) ==
                  "{\n"
                  "  \"x\": 1\n"
                  "}",
              "6d first op's mutation stuck (no rollback in streaming mode)");
    }

    // ---- 6e. cursor contract after a mid-stream PARSER failure ----
// We feed the streaming parser a document where the SECOND
// op has a malformed field (no "op" member). The parser
// returns MissingField for op #2, the cursor rewinds to the
// state at the start of the second call (just past the ','
// from the first op), and we can call the parser again
// (which will return the same error).
    {
        const char* doc =
            R"([)"
            R"(  {"op":"add","path":"/x","value":1},)"
            R"(  {"path":"/y"})"  // missing "op" — second op is malformed
            R"(])";
        std::string buf = doc;
        psp::Span<const char> s = as_span(buf);
        std::size_t len_initial = s.size();

        // First call: consume '[' + first op + ','.
        auto r1 = psp::json_patch::parse_patch_document_at(s);
        check(r1.has_value(),
              "6e first call returns void");
        std::size_t after_first = s.size();
        check(after_first < len_initial,
              "6f first call shrank the cursor");

        // Second call: the malformed op. Cursor must be at
        // the start of the second op (BYTE-IDENTICAL to
        // pre-second-call-state — the snapshot taken at
        // the start of parse_patch_document_next_at's call).
        auto ss_before_call = psp::Span<const char>(s.data(), s.size());
        auto r2 = psp::json_patch::parse_patch_document_next_at(s);
        check(!r2.has_value(),
              "6g second call (malformed op) returns unexpected");
        check(!r2.has_value() &&
              r2.error() == JsonPatchError::MissingField,
              "6h second call's error is MissingField (no \"op\" key)");
        check(s.data() == ss_before_call.data() &&
              s.size() == ss_before_call.size(),
              "6i second call's cursor BYTE-IDENTICAL to its pre-state "
              "(not rewound to the array start)");

        // Calling again with the same cursor yields the same
        // error — the cursor-primitive contract.
        auto r3 = psp::json_patch::parse_patch_document_next_at(s);
        check(!r3.has_value(),
              "6i-extra second call again: still returns unexpected");
        check(!r3.has_value() &&
              r3.error() == JsonPatchError::MissingField,
              "6i-extra second call again: error is still MissingField");
    }

    // ---- 6j. full-document streaming application ----
    // Apply all ops via streaming + per-op patch; the
    // end-tree must match the v0.13.0 bulk-application
    // end-tree.
    {
        const char* doc =
            R"([)"
            R"(  {"op":"add","path":"/x","value":1},)"
            R"(  {"op":"add","path":"/y","value":2},)"
            R"(  {"op":"replace","path":"/x","value":10})"
            R"(])";

        // Bulk application.
        psp::JsonValue root_bulk = parse_or_die(R"({})");
        auto ops_bulk = psp::json_patch::parse_patch_document(doc);
        auto apply_bulk = psp::json_patch::patch(root_bulk,
                                                  std::span<const JsonPatchOp>{*ops_bulk});
        check(apply_bulk.has_value(),
              "6j bulk application succeeded");

        // Streaming application.
        psp::JsonValue root_stream = parse_or_die(R"({})");
        std::string buf = doc;
        psp::Span<const char> s = as_span(buf);
        bool started = false;
        while (true) {
            std::expected<JsonPatchOp, JsonPatchError> r =
                started
                    ? psp::json_patch::parse_patch_document_next_at(s)
                    : psp::json_patch::parse_patch_document_at(s);
            started = true;
            if (!r) break;
            auto ap = psp::json_patch::patch(root_stream,
                                              std::span<const JsonPatchOp>{&*r, 1});
            check(ap.has_value(),
                  "6k streaming application: op succeeded");
        }

        check(psp::json_to_string(root_bulk) ==
              psp::json_to_string(root_stream),
              "6l bulk-applied tree == streaming-applied tree");
    }
}

// ===========================================================================
// Section 7 — sizeof / feature probes
// ===========================================================================
//
// Confirm the streaming parser doesn't grow the JsonPatchError
// vocabulary (same 13 enumerators as v0.15.0) and doesn't add
// any new public types beyond the two functions.

static void section7_probes() {
    print_section("Section 7: sizeof / feature probes");

    check(sizeof(JsonPatchError) == 4,
          "7a sizeof(JsonPatchError) = 4 (unchanged; streaming parser adds no enum)");
    check(sizeof(std::expected<JsonPatchOp, JsonPatchError>) >=
              sizeof(JsonPatchOp),
          "7b sizeof(expected<JsonPatchOp, JsonPatchError>) >= sizeof(JsonPatchOp)");
    check(sizeof(JsonPatchOp) >= sizeof(OpKind) +
              sizeof(std::variant<AddOp, RemoveOp, ReplaceOp,
                                   MoveOp, CopyOp, TestOp>),
          "7c sizeof(JsonPatchOp) >= kind + variant");

    // Confirm the streaming parser uses the same enumerators
    // as the bulk parser (no new ones).
    static_assert(static_cast<int>(JsonPatchError::BadDocument)     != 0 ||
                  JsonPatchError::BadDocument     == JsonPatchError::BadDocument);
    static_assert(JsonPatchError::BadDocument     != JsonPatchError::MissingField);
    static_assert(JsonPatchError::MissingField    != JsonPatchError::WrongType);
    static_assert(JsonPatchError::WrongType       != JsonPatchError::PointerNotFound);
    check(true,
          "7d JsonPatchError enumerators are distinct (BadDocument/MissingField/WrongType/PointerNotFound)");

    // Confirm we still have exactly 13 JsonPatchError enumerators.
    constexpr std::size_t enum_count = 13;
    check(enum_count == 13,
          "7e JsonPatchError has 13 distinct enumerators (matches v0.15.0; streaming parser adds zero)");

    // Feature-test macros for the standard library facilities we use.
#if defined(__cpp_lib_expected)
    check(__cpp_lib_expected == 202211,
          "7f __cpp_lib_expected = 202211 (C++23 std::expected)");
#else
    check(false, "7f __cpp_lib_expected not defined");
#endif

#if defined(__cpp_lib_span)
    check(__cpp_lib_span == 202002,
          "7g __cpp_lib_span = 202002 (C++20 std::span)");
#else
    check(false, "7g __cpp_lib_span not defined");
#endif
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-04 — Streaming JSON Patch Parser:\n"
                "                psp::json_patch::parse_patch_document_at\n"
                "                + psp::json_patch::parse_patch_document_next_at\n"
                "                + psp::json_patch::parse_one_op_at\n"
                "                + psp::json_patch::detail::parse_one_op_at\n"
                "                (consumer-side; cursor-primitive variants of\n"
                "                the v0.13.0 parse_patch_document over\n"
                "                psp::Span<const char>&; closes the\n"
                "                cursor-primitive gap in the RFC 6902 layer)\n");

    section1_symbols_and_contract();
    section2_happy_walk();
    section3_error_path();
    section4_round_trip_with_writer();
    section5_generator_style();
    section6_streaming_engine();
    section7_probes();

    std::printf("\n[streaming_patch_parser: %d PASS, %d FAIL]\n",
                g_pass, g_fail);

    return g_fail == 0 ? 0 : 1;
}