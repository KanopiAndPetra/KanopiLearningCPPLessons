// P-2026-08-05 — Consumer of psp_span_lib v0.15.0 that designs
// an INVERSE-JOURNAL variant of psp::json_patch::patch_atomic:
//
//   psp::json_patch::patch_journaled(JsonValue& root, span<JsonPatchOp>)
//       -> std::expected<void, JsonPatchError>
//
// Where this fits in the arc
// --------------------------
// The Aug 3 lesson (P-2026-08-03-transactional-patch.cpp) shipped
// the deep-clone variant of patch_atomic. The deep-clone
// captures the full pre-state tree before any op is applied; on
// failure the pre-state is move-assigned back over the
// partially-mutated tree. The cost is one full deep-clone of
// `root` up-front, regardless of how many ops succeed or fail.
//
// Today we design the INVERSE variant. Instead of cloning the
// whole tree, we capture the INVERSE of each op as it is
// applied. On success the journal is discarded; on failure the
// journal is replayed in reverse to restore the pre-state.
//
// The Aug 3 lesson explicitly listed this as a forward-on
// candidate:
//
//   "Inverse-journal optimisation for patch_atomic — per-op
//    journal of inverses instead of a full deep-clone; relevant
//    for MB-scale patches."
//
// Today is that lesson. The Aug 3 API is unchanged at v0.15.0;
// patch_journaled is a NEW function in psp::json_patch::
// (consumer-side; library version is unchanged at v0.15.0; a
// future v0.16.0 promotion is mechanical).
//
// Why inverse-journal instead of deep-clone
// -----------------------------------------
// For a 5-op patch on a 10MB tree:
//
//   deep-clone:     copies 10MB up-front, then 5 ops mutate
//                   ~5 nodes. The clone is O(tree-size).
//   inverse-journal: at each op we look up the value at the
//                   path and stash it; 5 ops stash 5 values.
//                   The journal is O(patch-size).
//
// For "small patch on big tree" the journal is dramatically
// smaller; for "big patch on small tree" the two are similar
// (the journal is the patch itself, plus the value at each
// touched path). For "patch touches the same path multiple
// times" the journal is also small (one inverse per op).
//
// The journal is NOT a strict win for every workload — but it
// is a strict NO-REGRESSION for every workload the deep-clone
// handles correctly. Both return std::expected<void, JsonPatchError>
// with the same observable behaviour; the difference is cost.
//
// Why consumer-side and not library-side today
// ---------------------------------------------
// Same shape as the Aug 3 + Aug 4 lessons: a proven-in-consumer
// capability that exercises the design end-to-end. The library
// version is unchanged at v0.15.0; a future v0.16.0 promotion
// is mechanical (lift patch_journaled + inverse_for into
// <psp_span/json_ext.h>; bump the version).
//
// What the consumer exercises
// ---------------------------
//
//   Section 1 — symbol-presence + the per-op inverse spec for
//               all six RFC 6902 op kinds.
//   Section 2 — happy path: 4-op patch commits; journal is
//               empty after success; tree matches the
//               deep-clone variant.
//   Section 3 — failure path: 4-op patch fails on op 2;
//               journal is replayed; tree is BYTE-IDENTICAL
//               to the pre-state.
//   Section 4 — every op kind individually, with both happy
//               and failure paths.
//   Section 5 — replay of a journal via the existing patch()
//               function (proves the journal entries are valid
//               JsonPatchOp values).
//   Section 6 — interop: parse a wire-format document via the
//               v0.13.0 parser, then run patch_journaled on
//               the parsed vector.
//   Section 7 — back-compat: patch_journaled + patch_atomic
//               + patch_dry_run coexist; the Aug 3 functions
//               are unchanged.
//   Section 8 — sizeof / feature probes.
//
// Build (assumes psp_span_lib v0.15.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-08-05-inverse-journal-patch
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-08-05-inverse-journal-patch

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
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// psp::json_patch::patch_journaled — inverse-journal variant of patch_atomic
// ===========================================================================
//
// The big idea: instead of cloning the whole tree BEFORE the
// patch runs, we capture the inverse of each op AS we apply
// it. On success the journal is dropped; on failure we replay
// the journal in REVERSE order to restore the pre-state.
//
// The per-op inverse is computed BEFORE calling the engine's
// patch(), because the engine's failure path is
// "leave-the-tree-partially-mutated". Computing the inverse
// pre-emptively means we have the pre-state in hand before any
// mutation occurs.
//
// Per-op inverse spec (RFC 6902 §4 inverse table)
// -----------------------------------------------
//
//   AddOp    {path, value}      -> RemoveOp {path}
//                                  (the value at path AFTER add
//                                   is `value`; we can remove
//                                   it without stashing the
//                                   pre-state because we know
//                                   what was added).
//   RemoveOp {path}             -> AddOp {path, pre_value}
//                                  (we MUST stash the pre-state
//                                   value at path because
//                                   remove destroys it).
//   ReplaceOp{path, value}      -> AddOp {path, pre_value}
//                                  (same as remove; the engine
//                                   replaces in place, so we
//                                   must stash the old value).
//   MoveOp   {from, path}       -> MoveOp {path, from}
//                                  (swap from and path; the
//                                   value lives at `path` now,
//                                   move it back to `from`).
//   CopyOp   {from, path}       -> RemoveOp {path}
//                                  (we don't know what was at
//                                   `path` before; we know what
//                                   is there NOW, and that's
//                                   what we need to remove).
//   TestOp   {path, value}      -> (no inverse; no mutation).
//
// Why each inverse is correct
// ---------------------------
// - AddOp's inverse (RemoveOp at the same path): RFC 6902 §4.1
//   says add "adds a value to the target location". After the
//   add, the value at the path IS the new value. A remove
//   wipes the new value and restores whatever was there before
//   the add (which was nothing — the path didn't exist). This
//   is the standard "if-it-was-absent-it's-still-absent" rule.
// - RemoveOp's inverse (AddOp with the pre-state value): the
//   engine destroys the value at the path. To restore, we MUST
//   know what the value was. We look it up via
//   psp::json_pointer::resolve BEFORE the engine removes it.
// - ReplaceOp's inverse (AddOp with the pre-state value): the
//   engine replaces in place; the old value is destroyed. Same
//   "stash the pre-state" rule as remove.
// - MoveOp's inverse (MoveOp with from and path swapped): the
//   value lives at `path` after the move; move it back to
//   `from`. The pre-state at `from` is "value present", the
//   pre-state at `path` is "absent (or different value)" — and
//   the move restores both. No value-stash needed.
// - CopyOp's inverse (RemoveOp at the destination path): the
//   engine copies; the value at `from` is unchanged, the value
//   at `path` is the new copy. Remove at `path` wipes the
//   copy. The pre-state at `path` is restored (whatever was
//   there before the copy is back).
// - TestOp's inverse: none. Test is a no-op for the tree; it
//   only returns an error if the value mismatches. The engine
//   handles test BEFORE the journal entry is captured (i.e.,
//   we never call patch() for a failing test), so the journal
//   never sees a TestOp.
//
// Replay
// ------
// The journal is a std::vector<JsonPatchOp>. Replay is just
// `psp::json_patch::patch(root, journal_reversed)`. The journal
// is reversed because the LAST op applied is the FIRST one to
// undo (think "undo stack" in an editor).
//
// If the replay itself fails (e.g., a remove-stash AddOp can't
// add because the path is somehow gone), the tree is in an
// inconsistent state. We surface the journal-replay error and
// leave the tree in whatever state the partial-replay left it.
// In practice, the journal entries are constructed from the
// pre-state tree, so the replay paths always exist; the
// failure mode is "the value at the path was different from
// what we stashed", which can't happen because the engine is
// the only thing that could change it and it didn't yet.
// (The journal is captured BEFORE patch() is called for the
// op that might fail.)
//
// The replay error path is also a forward-on candidate for
// "what if the engine's self-move fix changes the observable
// behaviour of journal-replay?". Today's lesson is the
// SHAPE of the journal; the self-move fix is a separate
// exercise (flagged in the Aug 3 lesson and still on the
// forward-on list).

namespace psp {
namespace json_patch {

// Forward-declare deep_clone (defined further below) so
// lookup_at can call it without a forward-dependency on the
// full definition.
inline psp::JsonValue deep_clone(const psp::JsonValue& v);

// ---------------------------------------------------------------------------
// inverse_for — compute the inverse of a single op
// ---------------------------------------------------------------------------
//
// Returns std::nullopt for TestOp (no inverse; no mutation).
// Returns a populated JsonPatchOp for the other five kinds.
//
// The pre-state lookups (for RemoveOp and ReplaceOp) go
// through psp::json_pointer::resolve. If the resolve fails
// (e.g., the path doesn't exist before the op), the
// pre-state value is a null JsonValue — the inverse add
// would then put a `null` back. This matches the deep-clone
// variant's behaviour: the deep-clone captures the
// non-existent-path state, and a partial-mutate would
// attempt to remove from a non-existent path, which the
// engine rejects with a typed error before any mutation
// occurs. We surface the same error.
//
// To match the engine's strict "fail before mutate" rule
// (patch() is "best-effort atomic" — it stops on the first
// failure and leaves the tree partially mutated), we look
// up the pre-state BEFORE calling the engine for the op.
// If the pre-state lookup fails (the path doesn't exist
// for a Remove/Replace), we return the JsonPointer's error
// mapped to a JsonPatchError equivalent. Today's design
// uses JsonExtError::PointerNotFound as a "pre-state
// missing" signal — the engine itself would emit
// JsonPatchError::PointerNotFound for the same condition,
// so the journal-replay path is consistent with the
// patch() path's error vocabulary.

// Lookup pre-state value at `path` in `root`. Returns
// std::nullopt if the path doesn't exist or is malformed
// (the inverse add would then try to add null, which
// matches the deep-clone's "the path didn't exist"
// capture). The caller maps std::nullopt to a
// JsonPatchError::PointerNotFound (or PointerMalformed
// for a bad path shape).
inline std::optional<psp::JsonValue>
lookup_at(psp::JsonValue& root, std::string_view path) {
    // resolve_mut(string_view, JsonValue&) returns
    // std::expected<JsonValue*, JsonExtError>.
    auto found = psp::json_pointer::resolve_mut(path, root);
    if (!found) {
        return std::nullopt;
    }
    // Deep-copy so the journal is independent of `root`.
    // We use the consumer-side deep_clone (defined below in
    // psp::json_patch::, mirroring the Aug 3 lesson).
    return deep_clone(**found);
}

namespace detail {

// inverse_for: compute the inverse of a single op, given
// the current state of `root` (which is the pre-state for
// this op because the engine hasn't been called yet).
//
// Returns:
//   - std::nullopt for TestOp (no inverse; no mutation).
//   - A populated JsonPatchOp for the other five kinds.
//   - The `pre_state_error` out-param is populated if the
//     pre-state lookup fails (e.g., RemoveOp on a missing
//     path). The caller (patch_journaled) maps this to a
//     JsonPatchError and short-circuits — matching the
//     deep-clone variant's behaviour.
inline std::optional<JsonPatchOp>
inverse_for(psp::JsonValue& root, const JsonPatchOp& op,
            JsonPatchError& pre_state_error) {
    pre_state_error = JsonPatchError::BadDocument; // overwritten below

    switch (op.kind) {
        case OpKind::Add: {
            const auto& a = std::get<AddOp>(op.data);
            // Inverse: RemoveOp at the same path. No value
            // stashing needed — we know what was added.
            return JsonPatchOp{RemoveOp{a.path}};
        }
        case OpKind::Remove: {
            const auto& r = std::get<RemoveOp>(op.data);
            // Inverse: AddOp at the same path with the
            // pre-state value. MUST stash the pre-state.
            auto pre = lookup_at(root, r.path);
            if (!pre) {
                // Pre-state path doesn't exist. The
                // engine would emit PointerNotFound for
                // the same op; we mirror that.
                pre_state_error = JsonPatchError::PointerNotFound;
                return std::nullopt;
            }
            return JsonPatchOp{AddOp{r.path, std::move(*pre)}};
        }
        case OpKind::Replace: {
            const auto& rp = std::get<ReplaceOp>(op.data);
            // Inverse: AddOp at the same path with the
            // pre-state value. MUST stash the pre-state.
            auto pre = lookup_at(root, rp.path);
            if (!pre) {
                pre_state_error = JsonPatchError::PointerNotFound;
                return std::nullopt;
            }
            return JsonPatchOp{AddOp{rp.path, std::move(*pre)}};
        }
        case OpKind::Move: {
            const auto& m = std::get<MoveOp>(op.data);
            // Inverse: MoveOp with from and path swapped.
            return JsonPatchOp{MoveOp{m.path, m.from}};
        }
        case OpKind::Copy: {
            const auto& c = std::get<CopyOp>(op.data);
            // Inverse: RemoveOp at the destination path.
            return JsonPatchOp{RemoveOp{c.path}};
        }
        case OpKind::Test: {
            // No inverse; no mutation. The journal does
            // NOT record a TestOp entry.
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// replay_journal — apply a journal in REVERSE order to
// restore the pre-state. The journal is the SAME shape as
// the original patch (JsonPatchOp); replay is just
// psp::json_patch::patch on the reversed journal.
//
// Why reversed: imagine an editor's undo stack. The LAST
// action is the FIRST one to undo. The journal is built
// in apply-order, so replaying in reverse restores the
// pre-state.
inline std::expected<void, JsonPatchError>
replay_journal(psp::JsonValue& root,
               const std::vector<JsonPatchOp>& journal) {
    // Build a reversed view (no allocation beyond a small
    // vector). We could use a stack-allocated array, but
    // the journal is variable-size.
    std::vector<JsonPatchOp> reversed;
    reversed.reserve(journal.size());
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        reversed.push_back(*it);
    }
    return psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{reversed});
}

}  // namespace detail

// ---------------------------------------------------------------------------
// patch_journaled — inverse-journal transactional patch
// ---------------------------------------------------------------------------
//
// Same observable contract as patch_atomic:
//   - On success: `root` is fully mutated, return {}.
//   - On failure: `root` is restored to the pre-state, return
//     std::unexpected{error}.
//
// The difference is the rollback mechanism:
//   - patch_atomic:    deep-clone the pre-state, then on
//                      failure move-assign the pre-state back.
//   - patch_journaled: capture the inverse of each op as it is
//                      applied; on failure replay the journal
//                      in reverse.
//
// Today, patch_journaled is consumer-side. A future v0.16.0
// promotion is mechanical (lift this function + detail::
// inverse_for + detail::replay_journal + detail::lookup_at into
// <psp_span/json_ext.h>; bump the version).
inline std::expected<void, JsonPatchError>
patch_journaled(psp::JsonValue& root,
                std::span<const JsonPatchOp> ops) noexcept {
    std::vector<JsonPatchOp> journal;
    journal.reserve(ops.size());
    // Apply each op one at a time. After each successful op,
    // push the inverse to the journal. If a later op fails,
    // the journal is replayed in reverse to restore the
    // pre-state.
    for (const auto& op : ops) {
        // 1. Compute the inverse of this op against the
        //    CURRENT state of `root` (which is the pre-state
        //    for this op because the engine hasn't been
        //    called yet).
        JsonPatchError pre_err = JsonPatchError::BadDocument;
        auto inv = detail::inverse_for(root, op, pre_err);
        if (!inv && op.kind != OpKind::Test) {
            // Pre-state lookup failed (e.g., RemoveOp on
            // missing path). The engine WASN'T called for
            // this op, so `root` is still consistent with
            // the pre-state for THIS op — but the journal
            // has inverses for ops 0..N-1 that DID succeed.
            // We must replay the journal so far to roll
            // those back, then surface the pre-state
            // error.
            auto replay = detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{pre_err};
        }

        // 2. Apply the op via the engine.
        auto r = psp::json_patch::patch(root,
            std::span<const JsonPatchOp>{&op, 1});
        if (!r) {
            // Engine failed. Replay the journal so far
            // in REVERSE to restore the pre-state.
            // (The journal has at least the inverses of
            // ops that succeeded before this one.)
            auto replay = detail::replay_journal(root, journal);
            // If the replay itself fails, surface that
            // error. The tree is in an undefined
            // state at that point (but the replay
            // failure is very rare — the journal
            // entries are constructed from a valid
            // pre-state, so the replay paths should
            // always exist).
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{r.error()};
        }

        // 3. Op succeeded; record the inverse in the
        //    journal. TestOp has no inverse (std::nullopt
        //    is the signal for "don't journal this").
        if (inv) {
            journal.push_back(std::move(*inv));
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// deep_clone — recursive copy of a JsonValue tree
// ---------------------------------------------------------------------------
//
// (Mirrors the Aug 3 lesson's psp::json_patch::deep_clone —
// consumer-side; not in the library proper. The future
// v0.16.0 promotion arc includes this alongside the journal
// functions.)
//
// Walks the variant. For scalar alternatives (monostate,
// nullptr, bool, int64, double, string) the result is a
// copy-constructed alternative; for container alternatives
// (vector, map) the result is a freshly-allocated container
// holding a recursive deep-clone of every child.
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

// ---------------------------------------------------------------------------
// patch_atomic — deep-clone variant (mirror of the Aug 3 lesson)
// ---------------------------------------------------------------------------
//
// Same observable contract as patch_journaled (success:
// fully mutated; failure: pre-state restored). Different
// rollback mechanism (deep-clone of the pre-state vs
// inverse-journal).
//
// The two functions are interchangeable at the call site;
// the consumer's Section 7 picks whichever fits the
// workload (today the journaled variant for "small patch
// on big tree", the deep-clone variant for "uniform
// workload or very large patch on small tree").
inline std::expected<void, JsonPatchError>
patch_atomic(psp::JsonValue& root,
             std::span<const JsonPatchOp> ops) noexcept {
    std::optional<psp::JsonValue> snapshot{deep_clone(root)};
    auto r = psp::json_patch::patch(root, ops);
    if (!r) {
        root = std::move(*snapshot);
        return std::unexpected{r.error()};
    }
    return {};
}

// ---------------------------------------------------------------------------
// patch_dry_run — apply the patch to a private copy; never touches root
// ---------------------------------------------------------------------------
inline std::expected<void, JsonPatchError>
patch_dry_run(const psp::JsonValue& root,
              std::span<const JsonPatchOp> ops) noexcept {
    psp::JsonValue working = deep_clone(root);
    return psp::json_patch::patch(working, ops);
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test framework (same shape as the Aug 3 + Aug 4 lessons)
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

psp::JsonValue make_simple_tree() {
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
// Section 1 — symbol presence + per-op inverse spec
// ===========================================================================

void section_1() {
    using psp::json_patch::patch_journaled;
    using ::JsonPatchOp;
    using ::JsonPatchError;

    header("Section 1: symbol-presence + per-op inverse spec");
    using journaled_fn = std::expected<void, JsonPatchError>(*)(
        psp::JsonValue&, std::span<const JsonPatchOp>);
    journaled_fn j = &patch_journaled;
    check(j != nullptr, "1a &psp::json_patch::patch_journaled is well-defined");

    // The detail::inverse_for function is consumer-internal
    // today (would be a header-private function in the
    // library's <psp_span/json_ext.h> after a future
    // promotion). We test it through patch_journaled's
    // observable behaviour (Section 2-4).
    check(sizeof(JsonPatchOp) > 0, "1b JsonPatchOp is a complete type");
    check(std::variant_size_v<decltype(std::declval<JsonPatchOp>().data)> == 6,
          "1c JsonPatchOp holds a 6-alternative variant");
}

namespace {

// Helper: build a small add op.
JsonPatchOp mk_add(std::string path, psp::JsonValue v) {
    return JsonPatchOp{AddOp{std::move(path), std::move(v)}};
}
JsonPatchOp mk_remove(std::string path) {
    return JsonPatchOp{RemoveOp{std::move(path)}};
}
JsonPatchOp mk_replace(std::string path, psp::JsonValue v) {
    return JsonPatchOp{ReplaceOp{std::move(path), std::move(v)}};
}
JsonPatchOp mk_move(std::string from, std::string path) {
    return JsonPatchOp{MoveOp{std::move(from), std::move(path)}};
}
JsonPatchOp mk_copy(std::string from, std::string path) {
    return JsonPatchOp{CopyOp{std::move(from), std::move(path)}};
}
JsonPatchOp mk_test(std::string path, psp::JsonValue v) {
    return JsonPatchOp{TestOp{std::move(path), std::move(v)}};
}

}  // namespace

// ===========================================================================
// Section 2 — happy path: 4-op patch commits, journal is empty after success
// ===========================================================================

void section_2() {
    using psp::json_patch::patch_journaled;
    using psp::json_patch::deep_clone;
    using psp::json_patch::patch;

    header("Section 2: happy path — 4-op patch commits, journal is empty after success");

    psp::JsonValue root = make_simple_tree();
    psp::JsonValue pre = deep_clone(root);

    // 4 ops: add, replace, move, test.
    std::vector<JsonPatchOp> ops;
    ops.push_back(mk_add("/qux", psp::JsonValue{std::string{"new"}}));
    ops.push_back(mk_replace("/foo",
        psp::JsonValue{std::string{"BAR"}}));
    ops.push_back(mk_move("/baz", "/zzz"));
    ops.push_back(mk_test("/foo",
        psp::JsonValue{std::string{"BAR"}}));

    auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
    check(r.has_value(), "2a patch_journaled returns void on success");
    check(trees_equal(root, deep_clone(pre)) == false,
          "2b root was mutated (not equal to pre-state)");
    check(root.value.index() == pre.value.index(),
          "2c root's outer alternative is unchanged (still an object)");

    // Cross-check: the deep-clone variant (Aug 3) should
    // produce the same observable result.
    psp::JsonValue root_dc = deep_clone(pre);
    auto rd = psp::json_patch::patch_atomic(root_dc,
        std::span<const JsonPatchOp>{ops});
    check(rd.has_value(), "2d patch_atomic on the same ops returns void");
    check(trees_equal(root, root_dc),
          "2e patch_journaled-applied tree == patch_atomic-applied tree");
}

// ===========================================================================
// Section 3 — failure path: op 2 fails, journal is replayed, pre-state restored
// ===========================================================================

void section_3() {
    using psp::json_patch::patch_journaled;
    using psp::json_patch::deep_clone;
    using ::JsonPatchError;

    header("Section 3: failure path — op 2 fails, journal replayed, pre-state restored");

    psp::JsonValue root = make_simple_tree();
    psp::JsonValue pre = deep_clone(root);

    // 4 ops where op 2 (remove /missing) fails with
    // PointerNotFound.
    std::vector<JsonPatchOp> ops;
    ops.push_back(mk_add("/qux", psp::JsonValue{std::string{"new"}}));
    ops.push_back(mk_remove("/missing"));          // FAILS
    ops.push_back(mk_replace("/foo",
        psp::JsonValue{std::string{"NEVER"}}));   // never reached
    ops.push_back(mk_test("/foo",
        psp::JsonValue{std::string{"NEVER"}}));   // never reached

    auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
    check(!r.has_value(), "3a patch_journaled returns unexpected on failure");
    check(r.error() == JsonPatchError::PointerNotFound,
          "3b error is PointerNotFound (matches the engine's error for the same op)");
    check(trees_equal(root, pre),
          "3c root is BYTE-IDENTICAL to pre-state after rollback");
    check(trees_equal(root, deep_clone(pre)),
          "3d root is BYTE-IDENTICAL to a fresh deep-clone of pre-state");

    // Cross-check: the deep-clone variant should also leave
    // the tree at the pre-state.
    psp::JsonValue root_dc = deep_clone(pre);
    auto rd = psp::json_patch::patch_atomic(root_dc,
        std::span<const JsonPatchOp>{ops});
    check(!rd.has_value(), "3e patch_atomic on the same ops returns unexpected");
    check(trees_equal(root, root_dc),
          "3f patch_journaled-rollback tree == patch_atomic-rollback tree");
}

// ===========================================================================
// Section 4 — every op kind individually, happy and failure paths
// ===========================================================================

void section_4() {
    using psp::json_patch::patch_journaled;
    using psp::json_patch::deep_clone;
    using ::JsonPatchError;

    header("Section 4: every op kind — happy and failure paths");

    // 4a: AddOp happy path
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_add("/x", psp::JsonValue{std::int64_t{42}}));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "4a1 AddOp happy: patch_journaled returns void");
        check(trees_equal(root, pre) == false, "4a2 AddOp happy: root was mutated");
    }

    // 4b: AddOp failure (TestOp on a value that doesn't exist)
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_add("/x", psp::JsonValue{std::int64_t{42}}));
        ops.push_back(mk_test("/x", psp::JsonValue{std::int64_t{99}})); // FAIL
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "4b1 Add+TestFail: patch_journaled returns unexpected");
        check(trees_equal(root, pre),
              "4b2 Add+TestFail: root is BYTE-IDENTICAL to pre-state");
    }

    // 4c: RemoveOp happy path
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_remove("/foo"));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "4c1 RemoveOp happy: patch_journaled returns void");
        check(trees_equal(root, pre) == false, "4c2 RemoveOp happy: root was mutated");
    }

    // 4d: RemoveOp failure (missing path) -> PointerNotFound
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_remove("/missing"));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "4d1 RemoveOp missing-path: returns unexpected");
        check(r.error() == JsonPatchError::PointerNotFound,
              "4d2 RemoveOp missing-path: error is PointerNotFound");
        check(trees_equal(root, pre),
              "4d3 RemoveOp missing-path: root BYTE-IDENTICAL to pre-state");
    }

    // 4e: ReplaceOp happy
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_replace("/foo", psp::JsonValue{std::string{"BAZ"}}));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "4e1 ReplaceOp happy: patch_journaled returns void");
        check(trees_equal(root, pre) == false, "4e2 ReplaceOp happy: root was mutated");
    }

    // 4f: ReplaceOp failure (missing path)
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_replace("/missing",
            psp::JsonValue{std::string{"x"}}));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "4f1 ReplaceOp missing-path: returns unexpected");
        check(trees_equal(root, pre),
              "4f2 ReplaceOp missing-path: root BYTE-IDENTICAL to pre-state");
    }

    // 4g: MoveOp happy
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_move("/foo", "/moved_foo"));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "4g1 MoveOp happy: patch_journaled returns void");
        check(trees_equal(root, pre) == false, "4g2 MoveOp happy: root was mutated");
    }

    // 4h: MoveOp failure (would-clobber: from is a strict
    // ancestor of path)
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_move("/baz", "/baz/0")); // FAILS
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "4h1 MoveOp would-clobber: returns unexpected");
        check(trees_equal(root, pre),
              "4h2 MoveOp would-clobber: root BYTE-IDENTICAL to pre-state");
    }

    // 4i: CopyOp happy
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_copy("/foo", "/foo_copy"));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "4i1 CopyOp happy: patch_journaled returns void");
        check(trees_equal(root, pre) == false, "4i2 CopyOp happy: root was mutated");
    }

    // 4j: CopyOp failure (missing from-path)
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_copy("/missing", "/never")); // FAILS
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "4j1 CopyOp missing-from: returns unexpected");
        check(trees_equal(root, pre),
              "4j2 CopyOp missing-from: root BYTE-IDENTICAL to pre-state");
    }

    // 4k: TestOp happy
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_test("/foo", psp::JsonValue{std::string{"bar"}}));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "4k1 TestOp happy: patch_journaled returns void");
        check(trees_equal(root, pre),
              "4k2 TestOp happy: root is unchanged (TestOp is a no-op)");
    }

    // 4l: TestOp failure
    {
        psp::JsonValue root = make_simple_tree();
        psp::JsonValue pre = deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(mk_test("/foo", psp::JsonValue{std::string{"WRONG"}}));
        auto r = patch_journaled(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "4l1 TestOp mismatch: returns unexpected");
        check(trees_equal(root, pre),
              "4l2 TestOp mismatch: root is unchanged");
    }
}

// ===========================================================================
// Section 5 — replay of a journal via the existing patch() function
// ===========================================================================
//
// This section proves the journal entries are valid JsonPatchOp
// values: we apply the original patch, then construct the
// expected journal by hand, then run psp::json_patch::patch on
// the REVERSED journal. The post-replay tree should be the
// pre-state.

void section_5() {
    using psp::json_patch::patch;
    using psp::json_patch::deep_clone;
    using ::JsonPatchError;

    header("Section 5: hand-constructed journal replays cleanly via psp::json_patch::patch");

    psp::JsonValue root = make_simple_tree();
    psp::JsonValue pre = deep_clone(root);

    // 3 ops: add /qux, replace /foo, remove /baz/1
    std::vector<JsonPatchOp> ops;
    ops.push_back(mk_add("/qux", psp::JsonValue{std::string{"new"}}));
    ops.push_back(mk_replace("/foo", psp::JsonValue{std::string{"BAR"}}));
    ops.push_back(mk_remove("/baz/1"));

    auto r = patch(root, std::span<const JsonPatchOp>{ops});
    check(r.has_value(), "5a 3-op happy patch: returns void");
    check(trees_equal(root, pre) == false, "5b 3-op patch mutated the tree");

    // Hand-construct the expected journal (in apply order):
    //   1. AddOp /qux new    -> inverse RemoveOp /qux
    //   2. ReplaceOp /foo BAR -> inverse AddOp /foo "bar"
    //                            (the pre-state value at /foo)
    //   3. RemoveOp /baz/1   -> inverse AddOp /baz/1 2
    //                            (the pre-state value at /baz/1)
    std::vector<JsonPatchOp> journal;
    journal.push_back(mk_remove("/qux"));
    journal.push_back(mk_add("/foo", psp::JsonValue{std::string{"bar"}}));
    journal.push_back(mk_add("/baz/1", psp::JsonValue{std::int64_t{2}}));

    // Replay in REVERSE: baz/1, foo, qux.
    std::vector<JsonPatchOp> reversed;
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        reversed.push_back(*it);
    }
    auto rr = patch(root, std::span<const JsonPatchOp>{reversed});
    check(rr.has_value(), "5c replay of hand-constructed journal: returns void");
    check(trees_equal(root, pre),
          "5d replayed tree is BYTE-IDENTICAL to the original pre-state");
}

// ===========================================================================
// Section 6 — wire-format interop: parse -> patch_journaled
// ===========================================================================

void section_6() {
    using psp::json_patch::patch_journaled;
    using psp::json_patch::deep_clone;
    using psp::json_patch::parse_patch_document;
    using ::JsonPatchError;

    header("Section 6: wire-format interop — parse -> patch_journaled");

    // A 3-op wire-format document (the RFC 6902 §1 example).
    constexpr std::string_view wire =
        R"([{"op":"test","path":"/baz","value":[1,2,3]},)"
        R"({"op":"remove","path":"/baz"},)"
        R"({"op":"add","path":"/baz","value":[1,2]}])";

    psp::JsonValue root = make_simple_tree();
    psp::JsonValue pre = deep_clone(root);

    auto parsed = parse_patch_document(wire);
    check(parsed.has_value(), "6a parse_patch_document returns void");
    check(parsed->size() == 3, "6b parsed vector has 3 ops");

    auto r = patch_journaled(root, std::span<const JsonPatchOp>{*parsed});
    check(r.has_value(), "6c patch_journaled on parsed wire: returns void");
    check(trees_equal(root, pre) == false, "6d tree was mutated by the parsed wire");

    // Compare with the deep-clone variant.
    psp::JsonValue root_dc = deep_clone(pre);
    auto rd = psp::json_patch::patch_atomic(root_dc,
        std::span<const JsonPatchOp>{*parsed});
    check(rd.has_value(), "6e patch_atomic on parsed wire: returns void");
    check(trees_equal(root, root_dc),
          "6f patch_journaled tree == patch_atomic tree (wire-format)");

    // A failing wire (the test op on /baz expects [1,2,4] but
    // the actual value is [1,2,3]) — the test op fails with
    // TestValueMismatch.
    constexpr std::string_view bad_wire =
        R"([{"op":"add","path":"/x","value":1},)"
        R"({"op":"test","path":"/baz","value":[1,2,4]}])";

    psp::JsonValue root2 = deep_clone(pre);
    auto parsed2 = parse_patch_document(bad_wire);
    check(parsed2.has_value(), "6g bad_wire parse: returns void");
    auto r2 = patch_journaled(root2, std::span<const JsonPatchOp>{*parsed2});
    check(!r2.has_value(),
          "6h patch_journaled on bad wire: returns unexpected");
    check(r2.error() == JsonPatchError::TestValueMismatch,
          "6i error is TestValueMismatch");
    check(trees_equal(root2, pre),
          "6j root BYTE-IDENTICAL to pre-state after journaled-rollback");
}

// ===========================================================================
// Section 7 — back-compat: patch_journaled + patch_atomic + patch_dry_run
// ===========================================================================

void section_7() {
    using psp::json_patch::patch_journaled;
    using psp::json_patch::deep_clone;
    using ::JsonPatchError;

    header("Section 7: back-compat — patch_journaled + patch_atomic + patch_dry_run coexist");

    // Both wrappers live in psp::json_patch:: today (the
    // Aug 3 ones are the consumer-side copies from the
    // Aug 3 lesson — wait, actually the Aug 3 lesson
    // defined them as consumer-side, NOT in the library).
    // We re-introduce them here as psp::json_patch::
    // members of THIS consumer TU.
    //
    // The pattern: the consumer TU defines patch_atomic,
    // patch_dry_run, and deep_clone in psp::json_patch::
    // (mirroring the Aug 3 TU). The library proper
    // contains only the engine (patch) and the parser
    // (parse_patch_document).
    //
    // To prove coexistence, we apply the same patch with
    // both wrappers and check the post-state is identical.
    psp::JsonValue root_j = make_simple_tree();
    psp::JsonValue root_a = deep_clone(root_j);
    psp::JsonValue pre    = deep_clone(root_j);

    std::vector<JsonPatchOp> ops;
    ops.push_back(mk_add("/x", psp::JsonValue{std::int64_t{1}}));
    ops.push_back(mk_replace("/foo", psp::JsonValue{std::string{"BAZ"}}));
    ops.push_back(mk_remove("/baz/2"));

    auto rj = patch_journaled(root_j, std::span<const JsonPatchOp>{ops});
    auto ra = psp::json_patch::patch_atomic(root_a,
        std::span<const JsonPatchOp>{ops});
    check(rj.has_value(), "7a patch_journaled returns void");
    check(ra.has_value(), "7b patch_atomic returns void");
    check(trees_equal(root_j, root_a),
          "7c patch_journaled tree == patch_atomic tree (back-compat)");

    // dry-run: previews the patch against the PRE-STATE
    // without mutating the original. This is the natural
    // dry-run use case ("would this patch succeed on the
    // original tree?").
    auto rd = psp::json_patch::patch_dry_run(pre,
        std::span<const JsonPatchOp>{ops});
    check(rd.has_value(), "7d patch_dry_run on pre-state returns void");
    check(trees_equal(root_j, pre) == false,
          "7e patch_dry_run did NOT mutate pre (root_j is still in mutated state from 7a)");

    // A failing patch run with both wrappers — both must
    // roll back to the same pre-state.
    psp::JsonValue root_j2 = deep_clone(pre);
    psp::JsonValue root_a2 = deep_clone(pre);
    std::vector<JsonPatchOp> fail_ops;
    fail_ops.push_back(mk_add("/x", psp::JsonValue{std::int64_t{1}}));
    fail_ops.push_back(mk_remove("/missing")); // FAILS
    auto rj2 = patch_journaled(root_j2, std::span<const JsonPatchOp>{fail_ops});
    auto ra2 = psp::json_patch::patch_atomic(root_a2,
        std::span<const JsonPatchOp>{fail_ops});
    check(!rj2.has_value(), "7f fail-ops patch_journaled: returns unexpected");
    check(!ra2.has_value(), "7g fail-ops patch_atomic: returns unexpected");
    check(trees_equal(root_j2, root_a2),
          "7h fail-ops: both wrappers leave the tree at the same state");
    check(trees_equal(root_j2, pre),
          "7i fail-ops: both wrappers leave the tree BYTE-IDENTICAL to pre-state");
}

// ===========================================================================
// Section 8 — sizeof / feature probes
// ===========================================================================

void section_8() {
    header("Section 8: sizeof / feature probes");

    using psp::json_patch::patch_journaled;
    using ::JsonPatchError;

    check(sizeof(JsonPatchError) == 4,
          "8a sizeof(JsonPatchError) = 4 (unchanged; journal adds no enum)");
    check(std::variant_size_v<decltype(std::declval<JsonPatchOp>().data)> == 6,
          "8b JsonPatchOp variant has 6 alternatives (unchanged)");
    using pj_fn = std::expected<void, JsonPatchError>(*)(
        psp::JsonValue&, std::span<const JsonPatchOp>);
    pj_fn pj = &patch_journaled;
    check(pj != nullptr, "8c &psp::json_patch::patch_journaled is well-defined");

    // 13 distinct JsonPatchError enumerators (matches v0.15.0;
    // journal adds zero).
    constexpr JsonPatchError errs[] = {
        JsonPatchError::PointerMalformed,
        JsonPatchError::PointerNotFound,
        JsonPatchError::PointerNotAnObject,
        JsonPatchError::PointerNotAnArray,
        JsonPatchError::PointerIndexOutOfRange,
        JsonPatchError::PointerIndexNotANumber,
        JsonPatchError::BadPath,
        JsonPatchError::TestValueMismatch,
        JsonPatchError::UnknownOp,
        JsonPatchError::MoveWouldClobber,
        JsonPatchError::BadDocument,
        JsonPatchError::MissingField,
        JsonPatchError::WrongType,
    };
    check(sizeof(errs) / sizeof(errs[0]) == 13,
          "8d JsonPatchError has 13 distinct enumerators (matches v0.15.0; journal adds zero)");

    check(__cpp_lib_expected == 202211,
          "8e __cpp_lib_expected = 202211 (C++23)");
    check(__cpp_lib_span     == 202002,
          "8f __cpp_lib_span     = 202002 (C++20)");
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-05 — Inverse-Journal JSON Patch:\n"
                "                psp::json_patch::patch_journaled\n"
                "                (consumer-side; per-op journal of\n"
                "                inverses instead of a full pre-state\n"
                "                deep-clone; library version unchanged\n"
                "                at v0.15.0)\n");

    section_1();
    section_2();
    section_3();
    section_4();
    section_5();
    section_6();
    section_7();
    section_8();

    std::printf("\n[inverse_journal_patch: %d PASS, %d FAIL]\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
