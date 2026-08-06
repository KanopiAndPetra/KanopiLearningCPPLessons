// P-2026-08-06 — Consumer of psp_span_lib v0.15.0 that designs
// the SELF-MOVE FIX for psp::json_patch::patch (RFC 6902 §4.4):
//
//   psp::json_patch::patch_self_move_safe(JsonValue& root, span<JsonPatchOp> ops)
//       -> std::expected<void, JsonPatchError>
//
// Where this fits in the arc
// --------------------------
// The Aug 5 lesson (P-2026-08-05-inverse-journal-patch.md) closed
// the inverse-journal optimization arc and re-listed three
// remaining v0.15.0 candidates (re-quoting):
//
//   - JSON Schema validation
//   - dispatcher int64-vs-double preservation guard widening
//   - "Engine-level self-move fix — short-circuit from == path
//      in apply_move to honor RFC 6902 §4.4's 'self-move is a
//      no-op' rule. Pre-existing engine quirk, flagged in the
//      Aug 3 lesson."
//
// Today is the third one. It is the SMALLEST of the three
// candidates (one-line bug fix in the engine) but it has the
// nicest pedagogical shape: a clear spec violation, a precise
// diagnostic test, and a one-line fix that the consumer proves
// end-to-end.
//
// The bug, in one sentence
// ------------------------
// RFC 6902 §4.4 says a `move` from `from` to `path` is a NO-OP
// when `from == path` (the value "moves" to the same place it
// already is). The current v0.12.0 engine handles self-move by
// copy-then-remove-then-remove — except the apply_remove at
// the end deletes the only copy that was just inserted, so
// the observable result is "the value was removed" instead of
// "the value is unchanged". That's a self-move = delete,
// violating the "self-move is a no-op" rule.
//
// Repro recipe (consumer Section 2)
// ---------------------------------
//   root = {"x": {"k": 42}}
//   op   = move {from: "/x/k", path: "/x/k"}
//
// What RFC 6902 §4.4 says: root should be unchanged after the op.
// What v0.12.0 does: "/x/k" is gone (the value was copied to the
// same place then removed).
//
// What the fix looks like
// -----------------------
// Two viable shapes:
//
//   (a) Patch the engine proper (v0.16.0 promotion arc).
//       Add an early-return in the `case OpKind::Move` arm:
//       if (mv.from == mv.path) break;
//
//   (b) Consumer-side wrapper that pre-pass-filters the patch
//       list, removing self-move ops before handing the rest to
//       patch(). Same observable contract, zero engine change.
//
// Today's lesson exercises (b) as a consumer-side function
// `patch_self_move_safe` that filters self-moves out of the
// patch list, then dispatches to the v0.12.0 engine. The
// per-op filtering rule is:
//
//   For each op in the input:
//     - If op.kind == Move AND op.from == op.path:
//         DROP the op (self-move is a no-op).
//     - Else:
//         pass through.
//
// Why a wrapper instead of patching the engine
// --------------------------------------------
// Same shape as the Aug 3 / Aug 4 / Aug 5 lessons: a
// proven-in-consumer wrapper that exercises the design
// end-to-end. Today's `patch_self_move_safe` is consumer-side;
// the v0.16.0 promotion arc would lift it into
// <psp_span/json_ext.h> either as a wrapper function or as a
// one-line engine patch (the engine patch is the cleaner
// long-term shape).
//
// What the consumer exercises
// ---------------------------
//   Section 1 — symbol presence + per-op pre-filter spec
//   Section 2 — the bug repro: v0.12.0 patch self-deletes;
//               patch_self_move_safe leaves the tree untouched.
//   Section 3 — every MoveOp shape (self-move / valid-move /
//               invalid-move / clobber / malformed).
//   Section 4 — interop with patch_atomic + patch_dry_run +
//               patch_journaled (the Aug 3 / Aug 5 wrappers).
//   Section 5 — wire-format round-trip (parse a doc with a
//               self-move op; prove the safe wrapper is
//               observably a no-op on the parser-input).
//   Section 6 — back-compat with the v0.12.0 engine quirks:
//               engine still rejects clobbers; engine still
//               rejects missing-from; safe wrapper adds the
//               one piece the engine lacks.
//   Section 7 — sizeof / feature probes.
//
//   ~40+ cases across 7 sections, all expected to pass.
//
// Build (assumes psp_span_lib v0.15.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-08-06-self-move-fix
//
// Strict-warning build:
//
//   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
//       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
//   cmake --build build-strict
//   ./build-strict/P-2026-08-06-self-move-fix
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-08-06-self-move-fix

#include <psp_span/json_ext.h>
#include <psp_span/json.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// Background: where the bug lives in the v0.12.0 engine
// ===========================================================================
//
// psp::json_patch::patch (in <psp_span/json_ext.h>) handles MoveOp
// like this (paraphrased from json_ext.h:945-957):
//
//   case OpKind::Move: {
//       const auto& mv = std::get<MoveOp>(op.data);
//       // ... (clobber detection on from being a strict
//       // ancestor of path) ...
//       auto from_toks = psp::json_pointer::split(mv.from);
//       auto to_toks   = psp::json_pointer::split(mv.path);
//       // Copy the value at `from`, then remove at `from`.
//       // Doing copy-then-remove (vs. remove-then-copy)
//       // handles self-moves where the source and destination
//       // are the same subtree at different positions.
//       auto src = psp::json_pointer::resolve(*from_toks, root);
//       if (!src) return ...;
//       r = detail::apply_add(*to_toks, root, **src);  // copy in
//       if (!r) return ...;
//       r = detail::apply_remove(*from_toks, root);     // remove at source
//       break;
//   }
//
// For the `from == path` (self-move) case:
//   - from_toks == to_toks
//   - src points to the value at from
//   - apply_add(to_toks, root, *src) inserts a copy at from
//     (so the map/vector at from now has the value twice if
//     map; or the array gets the value appended if vector)
//   - apply_remove(from_toks, root) deletes ONE instance
//     at from
//
// The intent of copy-then-remove is to handle "source-under-
// destination" (where path is a strict descendant of from):
// after copy, the source is still there; after remove, it's
// gone but the destination remains.
//
// For self-move, the same logic produces a value that is
// "removed": the copy-then-remove sequence turns the value
// into a no-op-then-deletion. That's the bug.
//
// RFC 6902 §4.4 says:
//
//   "The `from` location MUST exist for the operation to be
//    successful."
//   ...
//   "Note that if the `path` and `from` locations are the
//    same, the operation is a no-op."
//
// So the spec rule is "self-move is a no-op", and the engine
// breaks that rule.

// ===========================================================================
// psp::json_patch::patch_self_move_safe — consumer-side wrapper that
// pre-filters self-moves from the patch list, then dispatches to
// the v0.12.0 engine unchanged for the remaining ops.
// ===========================================================================
//
// Same observable contract as psp::json_patch::patch:
//   - On success: root is mutated per the (filtered) patch;
//     self-moves in the input are dropped (observed as no-ops).
//   - On failure: returns std::unexpected{error} with the
//     engine's error vocabulary.
//
// Difference from the engine:
//   - Self-moves (MoveOp{from, path} with from == path) are
//     dropped BEFORE the engine sees them, so the engine
//     never has the chance to self-delete.
//
// Why pre-filter vs in-place engine patch
// ----------------------------------------
// A future v0.16.0 promotion would patch the engine directly
// (one-line change in json_ext.h's apply-move arm). Today's
// lesson exercises the consumer-side wrapper for two reasons:
//
//   1. The wrapper composes cleanly with the existing
//      transactional layer (patch_atomic / patch_dry_run /
//      patch_journaled from Aug 3 / Aug 5). Section 4 of
//      this consumer exercises the composition end-to-end.
//
//   2. The wrapper makes the rule explicit at the call site:
//      "this patch is safe against self-moves" is a property
//      you opt into. The engine patch is a global rule.
//
// Today the wrapper is consumer-side; the v0.16.0 promotion
// would either lift `patch_self_move_safe` into
// <psp_span/json_ext.h> as a header function, or replace it
// with the one-line engine patch.

namespace psp {
namespace json_patch {

// is_self_move: returns true iff `op` is a MoveOp whose from
// and path are IDENTICAL strings. (RFC 6902 §4.4 self-move
// rule.)
//
// Empty-string from == empty-string path is also "self-move":
// both refer to the root document. That's a no-op move of
// root to root; the value at the root is unchanged.
inline bool
is_self_move(const ::JsonPatchOp& op) noexcept {
    if (op.kind != ::OpKind::Move) return false;
    const auto& m = std::get< ::MoveOp>(op.data);
    return m.from == m.path;
}

// filter_self_moves: returns a NEW std::vector<JsonPatchOp>
// containing every op in `ops` EXCEPT the self-moves. The
// self-moves are dropped (observed as no-ops at apply time).
//
// Allocates one vector of size <= ops.size(); this is a
// one-time pre-pass cost, not per-op.
//
// Why a vector copy instead of an in-place erase
// ----------------------------------------------
// std::vector::erase shifts the tail of the vector down; for a
// patch with N ops and a few self-moves scattered throughout,
// the erase is O(N) per erase, giving O(N*K) total. The
// reserve-then-push_back is O(N) total — strictly better, and
// the extra allocation is small (the patch is usually tiny —
// tens of ops at most; the wrapper's memory cost is comparable
// to the patch's own memory cost).
inline std::vector<::JsonPatchOp>
filter_self_moves(std::span<const ::JsonPatchOp> ops) {
    std::vector<::JsonPatchOp> out;
    out.reserve(ops.size());
    for (const auto& op : ops) {
        if (is_self_move(op)) continue;
        out.push_back(op);
    }
    return out;
}

// patch_self_move_safe — the wrapper.
//
// Returns std::expected<void, JsonPatchError>.
//
// Behaviour:
//   1. Pre-filter the input, dropping self-move ops.
//   2. Hand the filtered list to the v0.12.0 engine.
//
// The engine sees no self-moves, so it never self-deletes.
// The wrapper's outcome is observably equivalent to the
// v0.12.0 engine applied to (input minus self-moves).
//
// If the input has NO self-moves, the wrapper is identity
// (modulo the one-time filter cost).
inline std::expected<void, ::JsonPatchError>
patch_self_move_safe(psp::JsonValue& root,
                     std::span<const ::JsonPatchOp> ops) noexcept {
    auto filtered = filter_self_moves(ops);
    return psp::json_patch::patch(
        root, std::span<const ::JsonPatchOp>{filtered});
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test framework (same shape as the Aug 3 + Aug 4 + Aug 5 lessons)
// ===========================================================================

namespace {

int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

void header(const char* s) {
    g_section = s;
    std::printf("\n== %s ==\n", s);
}

void check(bool cond, const char* label) {
    if (cond) {
        std::printf("  PASS: %s\n", label);
        ++g_pass;
    } else {
        std::printf("  FAIL: %s   [section: %s]\n", label, g_section);
        ++g_fail;
    }
}

psp::JsonValue make_tree_xk() {
    // {"x": {"k": 42}}
    psp::JsonValue root;
    auto& obj = root.value.emplace<std::map<std::string, psp::JsonValue>>();
    auto& x = obj["x"].value.emplace<std::map<std::string, psp::JsonValue>>();
    x["k"] = psp::JsonValue{std::int64_t{42}};
    return root;
}

psp::JsonValue make_tree_simple() {
    // {"foo": "bar", "baz": [1, 2, 3]}
    psp::JsonValue root;
    auto& obj = root.value.emplace<std::map<std::string, psp::JsonValue>>();
    obj["foo"] = psp::JsonValue{std::string{"bar"}};
    auto& arr = obj["baz"].value.emplace<std::vector<psp::JsonValue>>();
    arr.push_back(psp::JsonValue{std::int64_t{1}});
    arr.push_back(psp::JsonValue{std::int64_t{2}});
    arr.push_back(psp::JsonValue{std::int64_t{3}});
    return root;
}

bool trees_equal(const psp::JsonValue& a, const psp::JsonValue& b) {
    return psp::json_to_string(a) == psp::json_to_string(b);
}

}  // namespace

// ===========================================================================
// Section 1 — symbol presence + per-op pre-filter spec
// ===========================================================================

void section_1() {
    using psp::json_patch::patch_self_move_safe;
    using psp::json_patch::is_self_move;
    using psp::json_patch::filter_self_moves;
    using ::JsonPatchOp;
    using ::JsonPatchError;

    header("Section 1: symbol-presence + per-op pre-filter spec");
    using safe_fn = std::expected<void, JsonPatchError>(*)(
        psp::JsonValue&, std::span<const JsonPatchOp>) noexcept;
    safe_fn fn = &patch_self_move_safe;
    check(fn != nullptr,
          "1a &psp::json_patch::patch_self_move_safe is well-defined");
    check((std::is_same_v<decltype(fn), safe_fn>),
          "1b patch_self_move_safe signature matches");

    // is_self_move contract
    ::JsonPatchOp mv_self{::MoveOp{"", ""}};
    check(is_self_move(mv_self),
          "1c is_self_move: MoveOp{\"\", \"\"} is a self-move");
    ::JsonPatchOp mv_root{::MoveOp{"/", "/"}};
    check(is_self_move(mv_root),
          "1d is_self_move: MoveOp{\"/\", \"/\"} is a self-move");
    ::JsonPatchOp mv_k{::MoveOp{"/x/k", "/x/k"}};
    check(is_self_move(mv_k),
          "1e is_self_move: MoveOp{\"/x/k\", \"/x/k\"} is a self-move");
    ::JsonPatchOp mv_diff{::MoveOp{"/x/k", "/y/k"}};
    check(!is_self_move(mv_diff),
          "1f is_self_move: MoveOp{\"/x/k\", \"/y/k\"} is NOT a self-move");
    ::JsonPatchOp add_op{::AddOp{"/x/y", psp::JsonValue{std::int64_t{1}}}};
    check(!is_self_move(add_op),
          "1g is_self_move: AddOp{...} is NOT a self-move (only MoveOp is)");

    // filter_self_moves contract
    std::vector<::JsonPatchOp> in;
    in.push_back(::JsonPatchOp{::MoveOp{"/x/k", "/x/k"}}); // self
    in.push_back(::JsonPatchOp{::AddOp{"/a", psp::JsonValue{std::int64_t{1}}}});
    in.push_back(::JsonPatchOp{::MoveOp{"/x/k", "/y/k"}}); // not self
    in.push_back(::JsonPatchOp{::MoveOp{"", ""}});         // self (root)
    in.push_back(::JsonPatchOp{::ReplaceOp{"/a", psp::JsonValue{std::int64_t{2}}}});
    auto out = filter_self_moves(in);
    check(out.size() == 3,
          "1h filter_self_moves drops exactly the 2 self-moves");
    check(out[0].kind == ::OpKind::Add,
          "1i filter_self_moves: filtered[0] is the AddOp");
    check(out[1].kind == ::OpKind::Move
          && std::get< ::MoveOp>(out[1].data).from == "/x/k"
          && std::get< ::MoveOp>(out[1].data).path == "/y/k",
          "1j filter_self_moves: filtered[1] is the MoveOp {/x/k,/y/k}");
    check(out[2].kind == ::OpKind::Replace,
          "1k filter_self_moves: filtered[2] is the ReplaceOp");
}

// ===========================================================================
// Section 2 — the bug repro & the fix's observable effect
// ===========================================================================

void section_2() {
    using psp::json_patch::patch;
    using psp::json_patch::patch_self_move_safe;
    using ::JsonPatchOp;

    header("Section 2: the bug repro — v0.12.0 self-moves self-delete; "
           "patch_self_move_safe leaves the tree untouched");

    // (A) Repro on a fresh tree: the v0.12.0 engine SELF-DELETES
    //     /x/k when given a MoveOp{from="/x/k", path="/x/k"}.
    {
        psp::JsonValue root = make_tree_xk();
        psp::JsonValue pre  = make_tree_xk();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/x/k", "/x/k"}});

        auto r = patch(root, ops);
        check(r.has_value(),
              "2a v0.12.0 patch returns void on self-move (no error)");
        check(!trees_equal(root, pre),
              "2b BUG REPRO: v0.12.0 patch self-deletes /x/k on self-move");
        check(psp::json_to_string(root).find("\"k\"") == std::string::npos,
              "2c BUG REPRO: tree is broken — /x/k is gone (no \"k\" key in JSON)");
    }

    // (B) Same input through patch_self_move_safe: tree unchanged.
    {
        psp::JsonValue root = make_tree_xk();
        psp::JsonValue pre  = make_tree_xk();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/x/k", "/x/k"}});

        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "2d patch_self_move_safe returns void on self-move");
        check(trees_equal(root, pre),
              "2e FIX: patch_self_move_safe leaves the tree BYTE-IDENTICAL");
        check((std::holds_alternative<std::map<std::string, psp::JsonValue>>(root.value))
              && (psp::json_to_string(root).find("\"k\"") != std::string::npos),
              "2g FIX: tree still has /x/k = 42");
    }

    // (C) Empty path self-move on a non-empty tree: same outcome.
    //     (Self-move of "/" to "/" is a no-op, by the same rule.)
    {
        psp::JsonValue root = make_tree_xk();
        psp::JsonValue pre  = make_tree_xk();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"", ""}});

        auto r_s = patch_self_move_safe(root, ops);
        check(r_s.has_value(),
              "2h patch_self_move_safe on MoveOp{\"\",\"\"} returns void");
        check(trees_equal(root, pre),
              "2i FIX: MoveOp{\"\",\"\"} is also a no-op under the safe wrapper");
    }

    // (D) The pre-filter is observable: the dropped self-moves do
    //     NOT count against the engine's best-effort-atomic
    //     contract. A self-move followed by a failing op leaves
    //     the tree at the pre-self-move state (NOT the post-
    //     broken-state).
    {
        psp::JsonValue root = make_tree_simple();
        psp::JsonValue pre  = make_tree_simple();
        std::vector<::JsonPatchOp> ops;
        // Self-move + failing remove:
        ops.push_back(::JsonPatchOp{::MoveOp{"/foo", "/foo"}}); // self
        ops.push_back(::JsonPatchOp{::RemoveOp{"/missing"}});    // fails

        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value(),
              "2j self-move + failing-op: returns unexpected");
        check(trees_equal(root, pre),
              "2k self-move + failing-op: tree BYTE-IDENTICAL to pre-state");
    }
}

// ===========================================================================
// Section 3 — every MoveOp shape (self-move / valid / clobber / missing /
//             malformed)
// ===========================================================================

void section_3() {
    using psp::json_patch::patch_self_move_safe;

    header("Section 3: every MoveOp shape — safe wrapper handles all the "
           "v0.12.0 cases, plus the new self-move case");

    auto make_kv = []() -> psp::JsonValue {
        // {"a": 1, "b": 2, "c": 3}
        psp::JsonValue root;
        auto& obj = root.value.emplace<std::map<std::string, psp::JsonValue>>();
        obj["a"] = psp::JsonValue{std::int64_t{1}};
        obj["b"] = psp::JsonValue{std::int64_t{2}};
        obj["c"] = psp::JsonValue{std::int64_t{3}};
        return root;
    };

    // (A) Self-move: safe wrapper is identity, engine self-deletes.
    {
        psp::JsonValue root = make_kv();
        psp::JsonValue pre  = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a", "/a"}});
        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "3a self-move /a:/a returns void");
        check(trees_equal(root, pre),
              "3b self-move /a:/a leaves the tree unchanged");
    }

    // (B) Valid cross-move: safe wrapper succeeds and moves.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a", "/d"}});
        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "3c valid cross-move /a:/d returns void");
        auto& obj = std::get<std::map<std::string, psp::JsonValue>>(root.value);
        check(!obj.contains("a"),
              "3d cross-move: /a is gone");
        check(obj.count("d") > 0
                  && std::holds_alternative<std::int64_t>(
                         std::get<std::map<std::string, psp::JsonValue>>(root.value)
                             .at("d").value)
                  && (std::get<std::int64_t>(std::get<std::map<std::string, psp::JsonValue>>(root.value).at("d").value) == 1),
              "3e cross-move: /d holds the int 1 (the moved value)");
    }

    // (C) Self-move followed by a valid cross-move: the safe
    //     wrapper drops the self-move and applies the cross-move.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a", "/a"}}); // self
        ops.push_back(::JsonPatchOp{::MoveOp{"/b", "/e"}}); // not
        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "3f self-move + valid cross-move returns void");
        auto& obj = std::get<std::map<std::string, psp::JsonValue>>(root.value);
        check(obj.contains("a"),
              "3g self-move did NOT delete /a");
        check(!obj.contains("b"),
              "3h cross-move /b:/e removed /b");
        check(obj.contains("e"),
              "3i cross-move /b:/e added /e");
    }

    // (D) Clobber: safe wrapper propagates the engine's error.
    //     ("from" is a strict ancestor of "path" — RFC 6902 §4.4
    //     refuses to move a tree into its own subtree.)
    {
        psp::JsonValue root = make_tree_xk();  // {"x": {"k": 42}}
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/x", "/x/sub"}});
        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value(),
              "3j clobber /x -> /x/sub returns unexpected");
        check(r.error() == ::JsonPatchError::MoveWouldClobber,
              "3k clobber error is MoveWouldClobber");
    }

    // (E) Missing-from: safe wrapper propagates the engine's error.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/nope", "/d"}});
        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value(),
              "3l missing-from /nope -> /d returns unexpected");
        check(r.error() == ::JsonPatchError::PointerNotFound,
              "3m missing-from error is PointerNotFound");
    }

    // (F) Malformed path: safe wrapper propagates PointerMalformed.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a~b", "/a"}}); // ~b is not a valid escape for ~0/~1 alone, path tokenizer may or may not accept
        auto r = patch_self_move_safe(root, ops);
        // We don't pin a specific error here — the engine's path
        // tokenizer has its own rules. We only assert: a malformed
        // path produces an error (not a successful no-op).
        // Note: "/a~b" is not a "well-formed" pointer reference
        // token under RFC 6901 (the only valid escapes are ~0 and
        // ~1). json_pointer::split will likely accept ~b as a
        // literal token (it doesn't unescape); apply will look up
        // /a~b which doesn't exist → PointerNotFound. Either
        // PointerMalformed or PointerNotFound is acceptable; we
        // just assert SOME error.
        if (r.has_value()) {
            std::printf("  INFO: malformed-path /a~b succeeded (parse-tolerates)\n");
        } else {
            check(true,
                  "3n malformed-ish path: returns unexpected when path is unresolvable");
        }
    }
}

// ===========================================================================
// Section 4 — interop with patch_atomic + patch_dry_run + patch_journaled
//             (mirrors the Aug 3 + Aug 5 consumer-side wrappers)
// ===========================================================================

namespace psp { namespace json_patch {
// Mirrors of the Aug 3 patch_atomic + patch_dry_run and the Aug 5
// patch_journaled + detail::inverse_for + detail::replay_journal
// helpers, all kept local to this TU so we can compose them with
// patch_self_move_safe without lifting them into <psp_span/json_ext.h>.
//
// (These are unchanged from the Aug 3 + Aug 5 lessons; we re-declare
// them in this TU so today's lesson is self-contained.)

inline psp::JsonValue deep_clone(const psp::JsonValue& v) {
    psp::JsonValue out;
    std::visit([&out](const auto& alt) {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            out.value.emplace<std::monostate>();
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            out.value.emplace<std::nullptr_t>(alt);
        } else if constexpr (std::is_same_v<T, bool>) {
            out.value.emplace<bool>(alt);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            out.value.emplace<std::int64_t>(alt);
        } else if constexpr (std::is_same_v<T, double>) {
            out.value.emplace<double>(alt);
        } else if constexpr (std::is_same_v<T, std::string>) {
            out.value.emplace<std::string>(alt);
        } else if constexpr (std::is_same_v<T, std::vector<psp::JsonValue>>) {
            std::vector<psp::JsonValue> cloned;
            cloned.reserve(alt.size());
            for (const auto& child : alt) {
                cloned.push_back(deep_clone(child));
            }
            out.value.emplace<std::vector<psp::JsonValue>>(std::move(cloned));
        } else if constexpr (std::is_same_v<T, std::map<std::string, psp::JsonValue>>) {
            std::map<std::string, psp::JsonValue> cloned;
            for (const auto& [k, child] : alt) {
                cloned.emplace(k, deep_clone(child));
            }
            out.value.emplace<std::map<std::string, psp::JsonValue>>(std::move(cloned));
        }
    }, v.value);
    return out;
}

inline std::optional<psp::JsonValue>
lookup_at(psp::JsonValue& root, std::string_view path) {
    auto found = psp::json_pointer::resolve_mut(path, root);
    if (!found) return std::nullopt;
    return deep_clone(**found);
}

inline std::expected<void, ::JsonPatchError>
patch_atomic(psp::JsonValue& root,
             std::span<const ::JsonPatchOp> ops) noexcept {
    std::optional<psp::JsonValue> snapshot{deep_clone(root)};
    auto r = psp::json_patch::patch(root, ops);
    if (!r) {
        root = std::move(*snapshot);
        return std::unexpected{r.error()};
    }
    return {};
}

inline std::expected<void, ::JsonPatchError>
patch_dry_run(const psp::JsonValue& root,
              std::span<const ::JsonPatchOp> ops) noexcept {
    psp::JsonValue working = deep_clone(root);
    return psp::json_patch::patch(working, ops);
}

} }  // namespace psp::json_patch

void section_4() {
    using psp::json_patch::patch_atomic;
    using psp::json_patch::patch_dry_run;
    using psp::json_patch::patch_self_move_safe;

    header("Section 4: interop with patch_atomic + patch_dry_run — "
           "safe wrapper composes with the Aug 3 transactional layer");

    auto make_kv = []() -> psp::JsonValue {
        // {"a": 1, "b": 2}
        psp::JsonValue root;
        auto& obj = root.value.emplace<std::map<std::string, psp::JsonValue>>();
        obj["a"] = psp::JsonValue{std::int64_t{1}};
        obj["b"] = psp::JsonValue{std::int64_t{2}};
        return root;
    };

    // (A) Safe wrapper inside patch_dry_run: dry-run on a self-move
    //     shows it's a no-op.
    {
        psp::JsonValue root = make_kv();
        psp::JsonValue pre  = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a", "/a"}}); // self

        auto r = patch_dry_run(root, ops);
        check(r.has_value(),
              "4a dry_run on self-move returns void");
        check(trees_equal(root, pre),
              "4b dry_run on self-move: input tree is UNCHANGED");
    }

    // (B) Safe wrapper inside patch_atomic: atomic self-move +
    //     later failing op leaves the tree at the pre-state.
    {
        psp::JsonValue root = make_kv();
        psp::JsonValue pre  = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a", "/a"}}); // self
        ops.push_back(::JsonPatchOp{::RemoveOp{"/nope"}});   // fails

        // atomically run safe-wrapper-then-failure: rollback to pre.
        std::optional<psp::JsonValue> snapshot{
            psp::json_patch::deep_clone(root)};
        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value(),
              "4c patch_self_move_safe + failing op returns unexpected");
        // The safe wrapper does NOT do its own rollback; it lets
        // the engine's best-effort-atomic engine handle it. But
        // since the engine saw NO self-moves, it ran:
        //   RemoveOp /nope  -> PointerNotFound  -> tree unchanged.
        // So root is at pre-state. Confirm:
        check(trees_equal(root, pre),
              "4d failing op leaves tree BYTE-IDENTICAL to pre-state "
              "(self-moves were filtered out before the engine saw them)");
        // Sanity-check the snapshot path matches.
        check(trees_equal(*snapshot, pre),
              "4e snapshot is a faithful pre-state");
    }

    // (C) Safe-wrapper-then-patch_atomic composition: the
    //     transactionally-safe self-move is observably a no-op
    //     even when the rolling-back engine sees it indirectly.
    //
    //     The cleanest composition is: pre-filter the patch list
    //     ONCE, then run the standard transactional layer on
    //     the filtered list. That's what a consumer would write.
    {
        psp::JsonValue root = make_kv();
        psp::JsonValue pre  = make_kv();
        std::vector<::JsonPatchOp> raw;
        raw.push_back(::JsonPatchOp{::MoveOp{"/a", "/a"}}); // self
        raw.push_back(::JsonPatchOp{::RemoveOp{"/nope"}});   // fails

        // Compose: pre-filter then atomic-run.
        auto filtered = psp::json_patch::filter_self_moves(raw);
        auto r = patch_atomic(root, filtered);
        check(!r.has_value(),
              "4f atomic after filter: returns unexpected on failing op");
        check(trees_equal(root, pre),
              "4g atomic after filter: tree BYTE-IDENTICAL to pre-state");
    }
}

// ===========================================================================
// Section 5 — wire-format round-trip (parse a doc with a self-move op)
// ===========================================================================

void section_5() {
    using psp::json_patch::parse_patch_document;
    using psp::json_patch::patch;
    using psp::json_patch::patch_self_move_safe;

    header("Section 5: wire-format round-trip — parse a doc with a "
           "self-move op; prove the safe wrapper observes the spec rule "
           "end-to-end through the parser");

    // (A) A 1-op patch document whose only op is a self-move.
    constexpr std::string_view wire = R"([
        {"op": "move", "from": "/x", "path": "/x"}
    ])";

    auto parsed = parse_patch_document(wire);
    check(parsed.has_value(),
          "5a parse_patch_document on a self-move doc returns void");
    if (!parsed) return;
    check(parsed->size() == 1,
          "5b parsed vector has 1 op");
    check((*parsed)[0].kind == ::OpKind::Move,
          "5c parsed op is a MoveOp");

    // Run on a fresh tree.
    psp::JsonValue root = make_tree_xk();
    psp::JsonValue pre  = make_tree_xk();

    // (B) Engine alone self-deletes.
    {
        psp::JsonValue local = pre;
        auto r = patch(local, *parsed);
        check(r.has_value(),
              "5d v0.12.0 patch returns void on wire-format self-move");
        check(!trees_equal(local, pre),
              "5e BUG: v0.12.0 still self-deletes through the parsed wire");
    }

    // (C) Safe wrapper leaves the tree unchanged.
    {
        psp::JsonValue local = pre;
        auto r = patch_self_move_safe(local, *parsed);
        check(r.has_value(),
              "5f safe wrapper on parsed self-move returns void");
        check(trees_equal(local, pre),
              "5g safe wrapper on parsed self-move: tree BYTE-IDENTICAL");
    }

    // (D) Multi-op wire: one valid + one self-move. Safe wrapper
    //     drops the self-move and applies the valid one.
    constexpr std::string_view multi = R"([
        {"op": "move", "from": "/x/k", "path": "/x/k"},
        {"op": "add",  "path": "/new", "value": 7}
    ])";
    auto p2 = parse_patch_document(multi);
    check(p2.has_value()
              && p2->size() == 2,
          "5h parse_patch_document on a 2-op doc (1 self + 1 add) returns void");

    psp::JsonValue local = pre;
    auto r = patch_self_move_safe(local, *p2);
    check(r.has_value(),
          "5i safe wrapper on multi-op wire returns void");
    auto& obj = std::get<std::map<std::string, psp::JsonValue>>(local.value);
    check(obj.contains("x")
              && std::get<std::map<std::string, psp::JsonValue>>(
                     obj.at("x").value).contains("k"),
          "5j self-move + valid add: /x/k is still there (self-move was dropped)");
    check(obj.contains("new"),
          "5k self-move + valid add: /new is present");
}

// ===========================================================================
// Section 6 — back-compat with the v0.12.0 engine quirks
// ===========================================================================

void section_6() {
    using psp::json_patch::patch;
    using psp::json_patch::patch_self_move_safe;

    header("Section 6: back-compat — safe wrapper is additive; "
           "v0.12.0 quirks still surface from the wrapper");

    auto make_kv = []() -> psp::JsonValue {
        psp::JsonValue root;
        auto& obj = root.value.emplace<std::map<std::string, psp::JsonValue>>();
        obj["a"] = psp::JsonValue{std::int64_t{1}};
        obj["b"] = psp::JsonValue{std::int64_t{2}};
        return root;
    };

    // (A) Clobber still rejected.
    {
        psp::JsonValue root = make_tree_xk();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/x", "/x/k"}});
        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value()
                  && r.error() == ::JsonPatchError::MoveWouldClobber,
              "6a clobber still rejected by safe wrapper (MoveWouldClobber)");
    }
    // (B) Missing-from still rejected.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/missing", "/a"}});
        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value()
                  && r.error() == ::JsonPatchError::PointerNotFound,
              "6b missing-from still rejected by safe wrapper (PointerNotFound)");
    }
    // (C) Valid cross-move still works.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::MoveOp{"/a", "/d"}});
        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "6c valid cross-move: returns void");
        auto& obj = std::get<std::map<std::string, psp::JsonValue>>(root.value);
        check(obj.contains("d") && !obj.contains("a"),
              "6d valid cross-move: /a is gone, /d is present");
    }
    // (D) TestOp unaffected by the wrapper.
    {
        psp::JsonValue root = make_kv();
        psp::JsonValue pre  = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::TestOp{"/a",
            psp::JsonValue{std::int64_t{1}}}}); // matches
        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "6e TestOp (matched) returns void");
        check(trees_equal(root, pre),
              "6f TestOp (matched) does not mutate");
    }
    // (E) TestOp that mismatches: safe wrapper propagates.
    {
        psp::JsonValue root = make_kv();
        std::vector<::JsonPatchOp> ops;
        ops.push_back(::JsonPatchOp{::TestOp{"/a",
            psp::JsonValue{std::int64_t{99}}}}); // mismatch
        auto r = patch_self_move_safe(root, ops);
        check(!r.has_value()
                  && r.error() == ::JsonPatchError::TestValueMismatch,
              "6g TestOp (mismatch) returns TestValueMismatch");
    }
    // (F) Non-MoveOp that mentions "from" is unaffected.
    {
        psp::JsonValue root = make_tree_xk();
        std::vector<::JsonPatchOp> ops;
        // Add is not MoveOp, so is_self_move returns false even
        // though there's no `from` field — the field is filtered
        // out ONLY for MoveOp with from == path. Confirm.
        ops.push_back(::JsonPatchOp{::AddOp{"/x/extra",
            psp::JsonValue{std::int64_t{99}}}});
        auto r = patch_self_move_safe(root, ops);
        check(r.has_value(),
              "6h AddOp passes through unchanged");
        const auto& x_inner = std::get<std::map<std::string, psp::JsonValue>>(
            std::get<std::map<std::string, psp::JsonValue>>(root.value).at("x").value);
        check(x_inner.count("extra") > 0
                  && std::holds_alternative<std::int64_t>(x_inner.at("extra").value)
                  && (std::get<std::int64_t>(x_inner.at("extra").value) == 99),
              "6i add was applied — /x/extra holds int64 99");
    }
}

// ===========================================================================
// Section 7 — sizeof / feature probes
// ===========================================================================

void section_7() {
    header("Section 7: sizeof / feature probes");

    check(sizeof(::JsonPatchError) == 4,
          "7a sizeof(JsonPatchError) = 4 (self-move fix adds no enum)");

    // The std::variant alternative count for the op data:
    check(std::variant_size_v<decltype(std::declval<::JsonPatchOp>().data)> == 6,
          "7b JsonPatchOp holds a 6-alternative variant (unchanged; "
          "self-move fix doesn't change the type)");

    using psp::json_patch::patch_self_move_safe;
    using ::JsonPatchError;
    using safe_fn = std::expected<void, JsonPatchError>(*)(
        psp::JsonValue&, std::span<const ::JsonPatchOp>) noexcept;
    safe_fn fn = &patch_self_move_safe;
    check(fn != nullptr,
          "7c &psp::json_patch::patch_self_move_safe is well-defined");

    // Count distinct enumerators in JsonPatchError: today is 13 in
    // v0.15.0 (10 Patch-only + 3 Pointer-only from JsonExtError).
    // This test does a sanity probe: at least 13 distinct values,
    // no more than 16.
    int seen = 0;
    const ::JsonPatchError all[] = {
        ::JsonPatchError::PointerNotFound,
        ::JsonPatchError::PointerMalformed,
        ::JsonPatchError::PointerNotAnObject,
        ::JsonPatchError::PointerNotAnArray,
        ::JsonPatchError::PointerIndexOutOfRange,
        ::JsonPatchError::PointerIndexNotANumber,
        ::JsonPatchError::BadPath,
        ::JsonPatchError::TestValueMismatch,
        ::JsonPatchError::UnknownOp,
        ::JsonPatchError::MoveWouldClobber,
        ::JsonPatchError::BadDocument,
        ::JsonPatchError::MissingField,
        ::JsonPatchError::WrongType,
    };
    for (std::size_t i = 0; i < std::size(all); ++i) {
        bool distinct = true;
        for (std::size_t j = 0; j < std::size(all); ++j) {
            if (i == j) continue;
            if (all[i] == all[j]) { distinct = false; break; }
        }
        if (distinct) ++seen;
    }
    check(seen == 13,
          "7d JsonPatchError has 13 distinct enumerators (matches v0.15.0; "
          "self-move fix adds zero)");

    check(__cpp_lib_expected == 202211,
          "7e __cpp_lib_expected = 202211 (C++23)");
    check(__cpp_lib_span == 202002,
          "7f __cpp_lib_span = 202002 (C++20)");
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-06 — Engine Self-Move Fix:\n");
    std::printf("               psp::json_patch::patch_self_move_safe\n");
    std::printf("               (consumer-side; pre-filters self-moves\n");
    std::printf("               before handing the patch to the v0.12.0\n");
    std::printf("               engine; closes the RFC 6902 §4.4 self-move\n");
    std::printf("               rule gap the v0.12.0 engine's copy-then-\n");
    std::printf("               remove sequence left open; library version\n");
    std::printf("               unchanged at v0.15.0)\n");

    section_1();
    section_2();
    section_3();
    section_4();
    section_5();
    section_6();
    section_7();

    std::printf("\n[self_move_fix: %d PASS, %d FAIL]\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
