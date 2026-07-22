// P-2026-07-22 — Consumer of <psp_span/json_ext.h>'s JSON Patch
// (RFC 6902) capability, the upgrade from the v0.11.0 Pointer
// (RFC 6901) half to the v0.12.0 Pointer + Patch half.
//
// Where this fits in the arc
// --------------------------
// The Jul 21 lesson (P-2026-07-21-psp-json-pointer.cpp) shipped
// <psp_span/json_ext.h> with the RFC 6901 Pointer half as
// psp_span_lib v0.11.0. The Jul 21 lesson's "Where we go next"
// said:
//
//   The most natural next lesson is **JSON Patch (RFC 6902)** in
//   the same header (or, if the header grows, a separate
//   <psp_span/json_patch.h>). RFC 6902 defines six ops: add,
//   remove, replace, move, copy, test. The resolver we shipped
//   today is a building block for Patch: every Patch op needs
//   to do at least one resolve() to find the target. The new
//   piece Patch adds is mutation — JsonValue& access through the
//   pointer, plus the array/element-management logic for add and
//   remove.
//
// Today is the RFC 6902 half. The library gains:
//
// | Layer                                | v0.11.0  | v0.12.0                              |
// |--------------------------------------|----------|--------------------------------------|
// | <psp_span/json_ext.h> Pointer half   | yes      | yes (unchanged)                      |
// | <psp_span/json_ext.h> Patch half     | (none)   | NEW                                  |
// | ::JsonExtError                       | 8 enums  | 8 enums (unchanged)                  |
// | ::JsonPatchOp                        | (none)   | 6-op tagged union + OpKind enum      |
// | ::JsonPatchError                     | (none)   | 10 enumerators + formatter spec      |
// | psp::json_pointer::resolve           | const    | const (unchanged)                    |
// | psp::json_pointer::resolve_mut       | (none)   | mutable, for Patch's write handle   |
// | psp::json_patch::patch(root, ops)    | (none)   | the RFC 6902 §1 engine              |
//
// The new header is C++23 (it includes <psp_span/json.h> which
// uses std::expected and std::variant, plus std::variant for
// JsonPatchOp). std::expected<void, JsonPatchError> is 8 bytes
// on this toolchain (4-byte err + 4-byte padding; no T).
//
// Build (assumes psp_span_lib v0.12.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-22-psp-json-patch
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-22-psp-json-patch

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

// Convenience: build a single-op patch and apply.
template <typename Op>
static std::expected<void, JsonPatchError>
apply_one(psp::JsonValue& root, Op&& op) {
    JsonPatchOp patch_op{std::forward<Op>(op)};
    std::vector<JsonPatchOp> ops;
    ops.push_back(std::move(patch_op));
    return psp::json_patch::patch(root, std::span<const JsonPatchOp>{ops});
}

// Build AddOp / RemoveOp / ReplaceOp / MoveOp / CopyOp / TestOp
// at the call site using {} aggregate-init (matches the
// JsonPatchOp constructors at file scope).
//
// The "value" JsonValue can be passed either as a sub-tree
// (std::int64_t{42}, std::string{"hi"}, std::vector<...>,
// std::map<...>) which is implicitly converted to JsonValue.

static AddOp        mk_add  (const char* path, psp::JsonValue v) { return AddOp{path, std::move(v)}; }
static RemoveOp     mk_rm   (const char* path)                    { return RemoveOp{path}; }
static ReplaceOp    mk_rep  (const char* path, psp::JsonValue v) { return ReplaceOp{path, std::move(v)}; }
static MoveOp       mk_mv   (const char* from, const char* to)  { return MoveOp{from, to}; }
static CopyOp       mk_cp   (const char* from, const char* to)  { return CopyOp{from, to}; }
static TestOp       mk_test (const char* path, psp::JsonValue v) { return TestOp{path, std::move(v)}; }

// ===========================================================================
// Section 1 — RFC 6902 §4.1: add
// ===========================================================================
//
// The add op inserts a value at path. RFC 6902 §4.1:
//   - If path is "" (or effectively the root), the whole
//     document is replaced.
//   - If the parent's last-token is "-" and the parent is an
//     array, the value is appended (this is the canonical use
//     for the "-" token — Pointer resolve() rejects it with
//     LastArrayElement, but Patch accepts it as the
//     "would-be-last-element here" position).
//   - Otherwise: integer index for arrays, key for objects.

static void section_add() {
    print_section("Section 1: add — RFC 6902 §4.1");

    // ---------- 1a. Add a missing object key ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1, "b": 2})");
        auto r = apply_one(root, mk_add("/c", psp::JsonValue{std::int64_t{3}}));
        if (!r) {
            std::printf("  FAIL: 1a add /c errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else if (psp::json_to_string(root) != "{\n  \"a\": 1,\n  \"b\": 2,\n  \"c\": 3\n}") {
            std::printf("  FAIL: 1a tree: %s\n", psp::json_to_string(root).c_str());
        } else {
            std::printf("  1a: add /c = 3 -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 1b. add with "-" appends to array ----------
    {
        psp::JsonValue root = parse_or_default(R"({"xs": [1, 2, 3]})");
        auto r = apply_one(root, mk_add("/xs/-", psp::JsonValue{std::int64_t{4}}));
        if (!r) {
            std::printf("  FAIL: 1b add /xs/- errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else if (psp::json_to_string(root) != "{\n  \"xs\": [\n    1,\n    2,\n    3,\n    4\n  ]\n}") {
            std::printf("  FAIL: 1b tree: %s\n", psp::json_to_string(root).c_str());
        } else {
            std::printf("  1b: add /xs/- (append) -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 1c. add with a specific index inserts ----------
    {
        psp::JsonValue root = parse_or_default(R"({"xs": [1, 2, 3]})");
        auto r = apply_one(root, mk_add("/xs/1", psp::JsonValue{std::int64_t{99}}));
        if (!r) {
            std::printf("  FAIL: 1c add /xs/1 errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            // After insertion: [1, 99, 2, 3].
            std::printf("  1c: add /xs/1 = 99 (insert) -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 1d. add at the document root replaces the whole tree ----------
    {
        psp::JsonValue root = parse_or_default(R"({"old": true})");
        psp::JsonValue replacement{std::map<std::string, psp::JsonValue>{
            {"new", psp::JsonValue{std::int64_t{42}}}
        }};
        auto r = apply_one(root, mk_add("", std::move(replacement)));
        if (!r) {
            std::printf("  FAIL: 1d add \"\" errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  1d: add \"\" (replace root) -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 1e. add into a nested array/object ----------
    {
        psp::JsonValue root = parse_or_default(
            R"({"users": [{"name": "Ada"}, {"name": "Bob"}]})");
        auto r = apply_one(root, mk_add("/users/0/age", psp::JsonValue{std::int64_t{30}}));
        if (!r) {
            std::printf("  FAIL: 1e add nested errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  1e: add /users/0/age = 30 (nested) -> %s\n",
                        psp::json_to_string(root).c_str());
        }
    }
}

// ===========================================================================
// Section 2 — RFC 6902 §4.2: remove
// ===========================================================================

static void section_remove() {
    print_section("Section 2: remove — RFC 6902 §4.2");

    // ---------- 2a. Remove an object key ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1, "b": 2, "c": 3})");
        auto r = apply_one(root, mk_rm("/b"));
        if (!r) {
            std::printf("  FAIL: 2a remove /b errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else if (psp::json_to_string(root) != "{\n  \"a\": 1,\n  \"c\": 3\n}") {
            std::printf("  FAIL: 2a tree: %s\n", psp::json_to_string(root).c_str());
        } else {
            std::printf("  2a: remove /b -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 2b. Remove an array element compacts the array ----------
    {
        psp::JsonValue root = parse_or_default(R"({"xs": [10, 20, 30, 40]})");
        auto r = apply_one(root, mk_rm("/xs/1"));
        if (!r) {
            std::printf("  FAIL: 2b remove /xs/1 errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            // After: [10, 30, 40].
            std::printf("  2b: remove /xs/1 -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 2c. Remove a missing key is an error ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1})");
        auto r = apply_one(root, mk_rm("/missing"));
        if (r) {
            std::printf("  FAIL: 2c remove /missing should have errored\n");
        } else if (r.error() != JsonPatchError::PointerNotFound) {
            std::printf("  FAIL: 2c wrong error: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  2c: remove /missing -> %s (as expected)\n",
                        std::format("{}", r.error()).c_str());
        }
    }
}

// ===========================================================================
// Section 3 — RFC 6902 §4.3: replace
// ===========================================================================

static void section_replace() {
    print_section("Section 3: replace — RFC 6902 §4.3");

    // ---------- 3a. Replace a value at an existing key ----------
    {
        psp::JsonValue root = parse_or_default(R"({"n": 1})");
        auto r = apply_one(root, mk_rep("/n", psp::JsonValue{std::int64_t{99}}));
        if (!r) {
            std::printf("  FAIL: 3a replace /n errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else if (psp::json_to_string(root) != "{\n  \"n\": 99\n}") {
            std::printf("  FAIL: 3a tree: %s\n", psp::json_to_string(root).c_str());
        } else {
            std::printf("  3a: replace /n = 99 -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 3b. Replace at a missing key is an error ----------
    {
        psp::JsonValue root = parse_or_default(R"({"n": 1})");
        auto r = apply_one(root, mk_rep("/missing", psp::JsonValue{std::int64_t{1}}));
        if (r) {
            std::printf("  FAIL: 3b replace /missing should have errored\n");
        } else if (r.error() != JsonPatchError::PointerNotFound) {
            std::printf("  FAIL: 3b wrong error: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  3b: replace /missing -> %s (as expected)\n",
                        std::format("{}", r.error()).c_str());
        }
    }

    // ---------- 3c. Replace root ----------
    {
        psp::JsonValue root = parse_or_default(R"([1, 2, 3])");
        psp::JsonValue replacement{std::map<std::string, psp::JsonValue>{
            {"replaced", psp::JsonValue{std::int64_t{1}}}
        }};
        auto r = apply_one(root, mk_rep("", std::move(replacement)));
        if (!r) {
            std::printf("  FAIL: 3c replace root errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  3c: replace \"\" (replace root) -> %s\n",
                        psp::json_to_string(root).c_str());
        }
    }
}

// ===========================================================================
// Section 4 — RFC 6902 §4.4 + §4.5: move + copy
// ===========================================================================

static void section_move_copy() {
    print_section("Section 4: move + copy — RFC 6902 §4.4 + §4.5");

    // ---------- 4a. Copy an object member ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1, "b": 2})");
        auto r = apply_one(root, mk_cp("/a", "/c"));
        if (!r) {
            std::printf("  FAIL: 4a copy errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  4a: copy /a -> /c -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 4b. Move an object member ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1, "b": 2})");
        auto r = apply_one(root, mk_mv("/a", "/c"));
        if (!r) {
            std::printf("  FAIL: 4b move errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  4b: move /a -> /c -> %s\n", psp::json_to_string(root).c_str());
        }
    }

    // ---------- 4c. Move into a strict ancestor must FAIL (would clobber) ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": {"b": 1}})");
        auto r = apply_one(root, mk_mv("/a", "/a/b/c"));
        if (r) {
            std::printf("  FAIL: 4c self-move did NOT error (RFC 6902 §4.4 violation)\n");
        } else if (r.error() != JsonPatchError::MoveWouldClobber) {
            std::printf("  FAIL: 4c wrong error: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  4c: move /a -> /a/b/c -> %s (as expected, no clobber)\n",
                        std::format("{}", r.error()).c_str());
        }
    }
}

// ===========================================================================
// Section 5 — RFC 6902 §4.6: test (the no-mutation op)
// ===========================================================================

static void section_test() {
    print_section("Section 5: test — RFC 6902 §4.6");

    // ---------- 5a. TestOp that matches: patch still succeeds ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1, "b": 2})");
        auto r = apply_one(root, mk_test("/a", psp::JsonValue{std::int64_t{1}}));
        if (!r) {
            std::printf("  FAIL: 5a test /a=1 errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  5a: test /a == 1 -> ok, tree unchanged: %s\n",
                        psp::json_to_string(root).c_str());
        }
    }

    // ---------- 5b. TestOp that mismatches: patch fails ----------
    {
        psp::JsonValue root = parse_or_default(R"({"a": 1, "b": 2})");
        auto r = apply_one(root, mk_test("/a", psp::JsonValue{std::int64_t{99}}));
        if (r) {
            std::printf("  FAIL: 5b test mismatch should have errored\n");
        } else if (r.error() != JsonPatchError::TestValueMismatch) {
            std::printf("  FAIL: 5b wrong error: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  5b: test /a == 99 -> %s (as expected), tree unchanged: %s\n",
                        std::format("{}", r.error()).c_str(),
                        psp::json_to_string(root).c_str());
        }
    }
}

// ===========================================================================
// Section 6 — RFC 6902 §1: multi-op patches
// ===========================================================================
//
// A patch document is an ARRAY of ops, applied in order. The
// state mutates op-by-op; later ops see the earlier
// mutations. We build a small array of ops and verify the
// composed result.

static void section_multi_op() {
    print_section("Section 6: multi-op patches — RFC 6902 §1");

    {
        // The RFC 6902 §3 intro example (lightly adapted to
        // our JsonValue tree):
        //
        //   patch = [
        //     {op: replace, path: /baz, value: "boo"},
        //     {op: add,     path: /hello, value: ["world"]},
        //     {op: replace, path: /baz, value: "qux"},
        //   ]
        //
        // applied to:
        //   { "baz": "boo", "hello": ["world", "!"] }
        //
        // expected:
        //   { "baz": "qux", "hello": ["world"] }
        // (the 2nd op removes "!" because "/hello" already
        // existed with an array; add into an existing array
        // at integer index N is exactly replace, but the
        // example uses /hello which is an object-style key
        // for an array, which in JSON Patch actually means
        // re-assigning that key to the new value if the
        // key already exists.)
        psp::JsonValue root = parse_or_default(
            R"({"baz": "boo", "hello": ["world", "!"]})");

        std::vector<JsonPatchOp> ops;
        // replace /baz = "boo"
        ops.push_back(JsonPatchOp{
            ReplaceOp{"/baz", psp::JsonValue{std::string{"boo"}}}});
        // add /hello = ["world"]
        ops.push_back(JsonPatchOp{
            AddOp{"/hello", psp::JsonValue{std::vector<psp::JsonValue>{
                psp::JsonValue{std::string{"world"}}}}}});
        // replace /baz = "qux"
        ops.push_back(JsonPatchOp{
            ReplaceOp{"/baz", psp::JsonValue{std::string{"qux"}}}});

        auto r = psp::json_patch::patch(root,
            std::span<const JsonPatchOp>{ops});
        if (!r) {
            std::printf("  FAIL: 6 multi-op errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            std::printf("  6: 3-op patch -> %s\n", psp::json_to_string(root).c_str());
        }
    }
}

// ===========================================================================
// Section 7 — resolve_mut + pointer round-trip after a patch
// ===========================================================================
//
// The whole point of `resolve_mut` is to give callers (Patch)
// a non-owning mutable handle to a sub-value. After patching,
// we want to be able to read the changed value at the same
// pointer that the patch wrote through.

static void section_resolve_mut_after_patch() {
    print_section("Section 7: resolve_mut — pointers survive a patch");

    psp::JsonValue root = parse_or_default(R"({"n": 7, "xs": [1, 2, 3]})");

    // resolve_mut /n -> JsonValue*. Should currently be 7.
    {
        auto r = psp::json_pointer::resolve_mut("/n", root);
        if (!r) {
            std::printf("  FAIL: 7a resolve_mut errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            const auto* got = std::get_if<std::int64_t>(&(*r)->value);
            if (!got || *got != 7) {
                std::printf("  FAIL: 7a initial /n = %lld, want 7\n",
                            got ? static_cast<long long>(*got) : -1);
            } else {
                std::printf("  7a: resolve_mut /n -> %lld (mutable handle)\n",
                            static_cast<long long>(*got));
            }
        }
    }

    // Apply patch: replace /n = 99.
    {
        auto r = apply_one(root, mk_rep("/n", psp::JsonValue{std::int64_t{99}}));
        if (!r) {
            std::printf("  FAIL: 7b patch errored: %s\n",
                        std::format("{}", r.error()).c_str());
        } else {
            // resolve /n again (via the const resolve) and
            // confirm the change is visible.
            auto r2 = psp::json_pointer::resolve("/n", root);
            if (!r2) {
                std::printf("  FAIL: 7b resolve after patch errored: %s\n",
                            std::format("{}", r2.error()).c_str());
            } else {
                const auto* got = std::get_if<std::int64_t>(&(*r2)->value);
                if (!got || *got != 99) {
                    std::printf("  FAIL: 7b /n after patch = %lld, want 99\n",
                                got ? static_cast<long long>(*got) : -1);
                } else {
                    std::printf("  7b: resolve /n after patch -> %lld\n",
                                static_cast<long long>(*got));
                }
            }
        }
    }
}

// ===========================================================================
// Section 8 — sizeof / feature probes (json_ext.h + json_patch.h surface)
// ===========================================================================

static void section_probes() {
    print_section("Section 8: sizeof / feature probes");

    std::printf("  sizeof(JsonExtError)                          = %zu\n",
                sizeof(JsonExtError));
    std::printf("  sizeof(psp::json_pointer::ReferenceToken)     = %zu\n",
                sizeof(psp::json_pointer::ReferenceToken));
    std::printf("  sizeof(std::vector<psp::json_pointer::ReferenceToken>) = %zu\n",
                sizeof(std::vector<psp::json_pointer::ReferenceToken>));
    std::printf("  sizeof(std::expected<const psp::JsonValue*,   = %zu\n",
                sizeof(std::expected<const psp::JsonValue*, JsonExtError>));
    std::printf("  sizeof(std::expected<psp::JsonValue*,         = %zu\n",
                sizeof(std::expected<psp::JsonValue*, JsonExtError>));
    std::printf("         JsonExtError>)\n");
    std::printf("  sizeof(JsonPatchError)                        = %zu\n",
                sizeof(JsonPatchError));
    std::printf("  sizeof(JsonPatchOp)                           = %zu\n",
                sizeof(JsonPatchOp));
    std::printf("  sizeof(std::expected<void, JsonPatchError>)   = %zu\n",
                sizeof(std::expected<void, JsonPatchError>));
    std::printf("  sizeof(void*)                                 = %zu\n", sizeof(void*));

    std::printf("  Public-header roster (v0.12.0):\n");
    std::printf("    <psp_span/span.h>     : psp::Span<T, Extent>            (C++17)\n");
    std::printf("    <psp_span/parser.h>   : psp::parse_int/parse_double + cursor + JSON scalars + ParseError (+ DuplicateKey in v0.10.0)\n");
    std::printf("    <psp_span/json.h>     : psp::JsonValue + parse_value_at / parse_array_at / parse_object_at + json_to_string\n");
    std::printf("    <psp_span/json_ext.h> : psp::json_pointer::split / to_string / resolve / resolve_mut\n");
    std::printf("                             + psp::json_patch::patch (NEW in v0.12.0)\n");
    std::printf("                             + ::JsonPatchOp (RFC 6902 tagged union, 6 ops)\n");
    std::printf("                             + ::JsonPatchError (10 enumerators) + std::formatter spec\n");

#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                            = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_variant)
    std::printf("  __cpp_lib_variant                             = %ld\n",
                static_cast<long>(__cpp_lib_variant));
#endif
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    section_add();
    section_remove();
    section_replace();
    section_move_copy();
    section_test();
    section_multi_op();
    section_resolve_mut_after_patch();
    section_probes();
    std::printf("\n[psp_json_patch_consumer: all 8 sections complete]\n");
    return 0;
}
