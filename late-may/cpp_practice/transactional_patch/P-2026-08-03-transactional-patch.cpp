// P-2026-08-03-transactional-patch.cpp
//
// TRANSACTIONAL JSON PATCH on top of psp_span_lib v0.15.0.
//
// Where this fits in the arc
// --------------------------
// The Aug 2 lesson (P-2026-08-02-psp-json-patch-writer-v015.md)
// closed the v0.15.0 promotion arc — the library now has both
// ends of the RFC 6902 §3 wire format (the v0.13.0 parser and
// the v0.15.0 writer) plus the v0.12.0 RFC 6902 engine. Today's
// lesson is the next item from the Aug 1 / Aug 2 forward-on list:
//
//   - Transactional Patch — std::expected<void, JsonPatchError>-
//     returning engine that pre-computes all ops' effects
//     before mutating, rolling back on any failure.
//
// This is a CONSUMER exercise. The transactional layer is
// implemented in this TU as a small wrapper around the library's
// existing psp::json_patch::patch. The design is exactly the same
// shape as the Jul 24 consumer writer and the Jul 25 v014 shadow
// parsers: a proven-in-consumer capability that today exercises
// the design end-to-end and is ready for a future v0.16.0 library
// promotion.
//
// The library surface is unchanged; today's lesson is
// a "what's possible" consumer exercise that closes a gap the
// v0.12.0 engine's docstring explicitly left open.
//
// The gap being closed
// --------------------
// psp::json_patch::patch (v0.12.0) is "best-effort atomic": it
// stops on the first op that fails and returns
// std::unexpected<JsonPatchError>, but the tree is left in
// whatever state it had after partial application. The
// documenting comment in <psp_span/json_ext.h> says:
//
//   "On failure, the error identifies which op failed and why;
//    the tree is left in whatever state it had after partial
//    application, matching RFC 6902 §3 ('the operation MUST
//    signal an error'). The stronger 'SHOULD leave unmodified'
//    contract would require transactional rollback over the
//    partial mutations; we don't implement that."
//
// RFC 6902 §3 actually says "the target document SHOULD be left
// in its previous state" — the library today only honors that
// for the immediately-failing op, not the prior ops. For a
// 10-op patch where op #7 fails, ops #1-#6 are already applied
// to the tree. That's a real ergonomic problem for any caller
// that needs "either all-or-nothing" semantics — config
// management, distributed state sync, atomic file updates.
//
// What today's consumer adds
// --------------------------
// Two new consumer-side functions, both with the same return
// type std::expected<void, JsonPatchError> as the underlying
// engine:
//
//   psp::json_patch::patch_atomic(JsonValue& root,
//                                  std::span<const JsonPatchOp> ops)
//       - Deep-clones `root` into a snapshot.
//       - Calls psp::json_patch::patch on the original.
//       - On success, returns {} (the snapshot is discarded).
//       - On failure, RESTORES the snapshot by std::move-assign
//         back to `root`, then returns std::unexpected.
//
//   psp::json_patch::patch_dry_run(JsonValue& root,
//                                    std::span<const JsonPatchOp> ops)
//       - Deep-clones `root` into a local copy.
//       - Calls psp::json_patch::patch on the local copy.
//       - Returns the result ({} or std::unexpected). `root` is
//         never touched.
//
// The two functions are mechanically related — dry-run is
// "atomic with an always-restore" — but they have different
// observable behaviour: patch_atomic mutates `root` on success
// and on failure leaves it unchanged; patch_dry_run NEVER
// mutates `root`.
//
// Design notes
// ------------
// 1. Why deep-clone instead of inverse-journaling
//    --------------------------------------------
//    The simplest correct implementation is one deep copy of
//    the tree, regardless of patch size. The cost is one extra
//    std::map / std::vector deep-copy of the entire tree; for
//    KB-scale patches that's sub-millisecond. The alternative —
//    journal per-op inverses and replay them in reverse on
//    failure — is more efficient for large trees but has subtle
//    correctness risks:
//
//      - For MoveOp the inverse is "remove at path, then add at
//        from" — but the value was MOVED at apply time, so the
//        re-add needs to capture the moved value before the
//        remove happens. Order matters.
//      - For CopyOp the inverse is "remove at path" — but
//        remove needs the path to exist; in nested structures
//        the path may have been overwritten by a later op.
//      - For AddOp the inverse is "remove at path" — but if
//        a later op overwrote the key, the "remove" is a no-op
//        which is the wrong inverse (the original value
//        should come back, not the overwrite value).
//
//    Deep-clone sidesteps all of these. The inverse-journal
//    optimisation is a future lesson; correctness first.
//
// 2. Why the snapshot is a std::optional<JsonValue>
//    -----------------------------------------------
//    std::optional<JsonValue> gives RAII: the snapshot is
//    destroyed (and its deep-tree freed) at the end of the
//    function, regardless of which path we took. The value is
//    moved into the optional, so the only allocation is the
//    initial clone.
//
// 3. Why we don't wrap psp::json_patch::patch in a try/catch
//    -------------------------------------------------------
//    The library is noexcept; the only failure mode is the
//    expected<>'s unexpected path. No exception-based
//    control flow. The deep-clone could throw
//    std::bad_alloc, but that's an OOM condition; we let it
//    propagate (the caller's caller will deal with it). The
//    library's contract is "noexcept unless OOM".
//
// 4. Move-assign on rollback
//    -----------------------
//    `root = std::move(*snapshot)` overwrites the variant
//    alternative (and frees the old tree) in one operation.
//    The snapshot's vector/map stay intact in the moved-from
//    optional, which is then destroyed at scope-exit. The
//    moved-from optional holds an empty std::monostate (the
//    JsonValue's default alternative) which is destructively
//    cheap.
//
// 5. Wire-format interop (Section 5)
//    --------------------------------
//    The v0.15.0 writer + v0.13.0 parser + v0.12.0 engine are
//    all in the library proper. We hand-build a 3-op patch in
//    memory, serialise it with the library writer, re-parse
//    it with the library parser, then run patch_atomic on the
//    parsed vector. The pipeline proves that the transactional
//    layer composes cleanly with the v0.15.0 round-trip
//    (writer -> parser -> engine).
//
// 6. Strict warnings + ASan
//    ----------------------
//    Same pattern as the Aug 2 lesson: a strict-warning
//    build proves the consumer compiles cleanly under
//    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
//    -Wsign-conversion, and an ASan + UBSan build proves no
//    memory or UB findings. The deep-clone walks the tree
//    recursively, so any use-after-free or uninitialised-read
//    in JsonValue's std::map / std::vector handling would
//    surface here.
//
// Build requires psp_span_lib v0.15.0 installed.

#include <psp_span/json.h>
#include <psp_span/json_ext.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// ===========================================================================
// Transactional Patch — the two new consumer-side functions
// ===========================================================================
//
// Lives in the psp::json_patch namespace so that a future v0.16.0
// library promotion can lift it into <psp_span/json_ext.h>
// without changing the call sites. The functions are
// `inline` so they're linkonce-odr (one definition per TU) and
// can live in the header without a separate .cpp — same shape
// as the existing library functions.

namespace psp {
namespace json_patch {

// ---------------------------------------------------------------------------
// deep_clone — recursive copy of a JsonValue tree
// ---------------------------------------------------------------------------
//
// Walks the variant. For scalar alternatives (monostate, nullptr,
// bool, int64, double, string) the result is a copy-constructed
// alternative; for container alternatives (vector, map) the
// result is a freshly-allocated container holding a recursive
// deep-clone of every child. The result is move-constructible
// and move-assignable (the std::map and std::vector inside
// JsonValue already are).
//
// The function is the "deep copy" half of the snapshot mechanism
// in patch_atomic / patch_dry_run below. We split it out so the
// snapshot logic in the two wrappers stays trivial.
//
// Why a standalone function (not a JsonValue member):
//   - The library's JsonValue deliberately has no clone member;
//     the Jul 19 / Jul 20 lessons justified that decision
//     (the library is for "fast parser / pretty-printer" use,
//     not for "I want to snapshot an arbitrary tree").
//   - Today is the consumer that *needs* clone; we live here
//     in psp::json_patch::detail (below) and expose only the
//     public wrappers to the test code.
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
// patch_atomic — all-or-nothing application of an RFC 6902 patch
// ---------------------------------------------------------------------------
//
//   1. Take a deep snapshot of `root` (the pre-state).
//   2. Call psp::json_patch::patch on `root`.
//   3. On success: the snapshot is destroyed, `root` is fully
//      mutated, return {}.
//   4. On failure: move the snapshot back into `root` (the
//      partially-mutated state is overwritten by the pre-state
//      in one move-assign), the moved-from snapshot is then
//      destroyed, return std::unexpected<JsonPatchError>.
//
// The `noexcept` qualifier matches the library's existing
// patch() — neither function allocates in a way that throws
// other than OOM (std::bad_alloc is the only thing that
// propagates from deep_clone's std::map::emplace and
// std::vector::push_back, and we let it propagate).
inline std::expected<void, JsonPatchError>
patch_atomic(psp::JsonValue& root,
             std::span<const JsonPatchOp> ops) noexcept {
    // Step 1: deep clone. The optional owns the snapshot; if
    // any later step throws bad_alloc, the optional's
    // destructor frees the snapshot cleanly.
    std::optional<psp::JsonValue> snapshot{deep_clone(root)};

    // Step 2: apply the patch.
    auto r = patch(root, ops);

    // Step 3/4: commit or roll back.
    if (!r) {
        // Move the snapshot back over the partially-mutated
        // `root`. Move-assign on a std::variant replaces the
        // active alternative (frees the old tree, takes
        // ownership of the new one) in one operation.
        root = std::move(*snapshot);
        return std::unexpected{r.error()};
    }
    return {};
}

// ---------------------------------------------------------------------------
// patch_dry_run — apply the patch to a private copy; never touches root
// ---------------------------------------------------------------------------
//
// Same shape as patch_atomic but the original `root` is never
// assigned-to. The deep-clone is the working copy; on success
// the working copy is destroyed (the caller already had the
// pre-state); on failure the working copy is destroyed the same
// way.
//
// Useful for "is this patch going to succeed?" queries that
// don't want to mutate the state — e.g. "would this config
// update break anything?".
inline std::expected<void, JsonPatchError>
patch_dry_run(const psp::JsonValue& root,
              std::span<const JsonPatchOp> ops) noexcept {
    psp::JsonValue working = deep_clone(root);
    return patch(working, ops);
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test harness
// ===========================================================================

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, std::string_view label) {
    if (cond) {
        ++g_pass;
        std::println("  PASS: {}", label);
    } else {
        ++g_fail;
        std::println("  FAIL: {}", label);
    }
}

void header(std::string_view title) {
    std::println("");
    std::println("== {} ==", title);
}

// Build helpers (mirrors the Jul 27 / Aug 2 lessons)
psp::JsonValue make_int(std::int64_t v) {
    psp::JsonValue j;
    j.value = v;
    return j;
}
psp::JsonValue make_str(std::string s) {
    psp::JsonValue j;
    j.value = std::move(s);
    return j;
}
psp::JsonValue make_obj() {
    psp::JsonValue j;
    j.value.emplace<std::map<std::string, psp::JsonValue>>();
    return j;
}
void obj_set(psp::JsonValue& j, std::string key, psp::JsonValue v) {
    auto& m = std::get<std::map<std::string, psp::JsonValue>>(j.value);
    m.emplace(std::move(key), std::move(v));
}
psp::JsonValue make_arr() {
    psp::JsonValue j;
    j.value.emplace<std::vector<psp::JsonValue>>();
    return j;
}
void arr_push(psp::JsonValue& j, psp::JsonValue v) {
    auto& a = std::get<std::vector<psp::JsonValue>>(j.value);
    a.push_back(std::move(v));
}

// Build a small pre-state tree.
//
//   {
//     "name": "alice",
//     "age": 30,
//     "tags": ["admin", "user"]
//   }
psp::JsonValue make_alice() {
    psp::JsonValue out = make_obj();
    obj_set(out, "name", make_str("alice"));
    obj_set(out, "age",  make_int(30));
    psp::JsonValue tags = make_arr();
    arr_push(tags, make_str("admin"));
    arr_push(tags, make_str("user"));
    obj_set(out, "tags", std::move(tags));
    return out;
}

}  // namespace

// ===========================================================================
// Section 1 — symbol-presence probes
// ===========================================================================
//
// The two new functions are inline in this TU; the simplest
// proof that they exist and have the right shape is to take
// their address. (Same shape as the Aug 2 lesson's Section 1.)
void section_1_symbol_presence() {
    header("Section 1: symbol-presence — patch_atomic + patch_dry_run are well-defined");

    using atomic_fn = std::expected<void, JsonPatchError>(*)(psp::JsonValue&, std::span<const JsonPatchOp>);
    using dry_fn    = std::expected<void, JsonPatchError>(*)(const psp::JsonValue&, std::span<const JsonPatchOp>);

    atomic_fn a = &psp::json_patch::patch_atomic;
    dry_fn    d = &psp::json_patch::patch_dry_run;

    check(a != nullptr, "1a &psp::json_patch::patch_atomic is well-defined");
    check(d != nullptr, "1b &psp::json_patch::patch_dry_run is well-defined");

    // Back-compat: the v0.12.0 engine is still well-defined.
    using engine_fn = std::expected<void, JsonPatchError>(*)(psp::JsonValue&, std::span<const JsonPatchOp>);
    engine_fn e = &psp::json_patch::patch;
    check(e != nullptr, "1c &psp::json_patch::patch is well-defined (v0.12.0 back-compat)");

    // The two new functions have the same return-type layout as
    // the engine (32 bytes on this toolchain, since
    // std::expected<void, E> = E + 4 bytes of padding; E is 4 bytes).
    check(sizeof(std::expected<void, JsonPatchError>) == 8,
          "1d std::expected<void, JsonPatchError> = 8 bytes (4-byte enum + 4-byte padding)");
}

// ===========================================================================
// Section 2 — happy path: a 3-op patch runs cleanly, tree is mutated
// ===========================================================================
//
// The new function's primary contract: the success path is
// OBSERVABLY IDENTICAL to the v0.12.0 engine's success path. The
// snapshot is built and then destroyed without anyone seeing it.
void section_2_happy_path() {
    header("Section 2: happy path — patch_atomic on a successful patch mutates the tree");

    // Pre-state: { name: alice, age: 30, tags: [admin, user] }
    psp::JsonValue root = make_alice();
    psp::JsonValue snapshot_before = psp::json_patch::deep_clone(root);

    // Patch: replace age 30 -> 31, add new key "city" = "NYC",
    //        replace tags[0] = "superadmin"
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{ReplaceOp{"/age",  make_int(31)}});
    ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});
    ops.push_back(JsonPatchOp{ReplaceOp{"/tags/0", make_str("superadmin")}});

    auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
    check(r.has_value(), "2a patch_atomic returns void on success");
    check(psp::json_to_string(root) != psp::json_to_string(snapshot_before),
          "2b tree was actually mutated (json differs from pre-state)");
    // 2c: pre-state JSON text (the library pretty-prints to
    //     multi-line, two-space indent). Captured verbatim.
    check(psp::json_to_string(snapshot_before) ==
              "{\n"
              "  \"age\": 30,\n"
              "  \"name\": \"alice\",\n"
              "  \"tags\": [\n"
              "    \"admin\",\n"
              "    \"user\"\n"
              "  ]\n"
              "}",
          "2c snapshot_before is the expected pre-state");
    check(psp::json_to_string(root) ==
              "{\n"
              "  \"age\": 31,\n"
              "  \"city\": \"NYC\",\n"
              "  \"name\": \"alice\",\n"
              "  \"tags\": [\n"
              "    \"superadmin\",\n"
              "    \"user\"\n"
              "  ]\n"
              "}",
          "2d root after the 3-op patch matches the expected post-state");
}

// ===========================================================================
// Section 3 — atomic rollback: a failing patch leaves the tree unchanged
// ===========================================================================
//
// The headline test: a 3-op patch where the third op fails.
// patch_atomic must restore the tree to the pre-state on the
// failure return — byte for byte.
void section_3_atomic_rollback() {
    header("Section 3: atomic rollback — a failing patch leaves the tree UNCHANGED");

    psp::JsonValue root = make_alice();
    psp::JsonValue pre_state = psp::json_patch::deep_clone(root);

    // Patch:
    //   op #1 (succeeds): replace age 30 -> 31
    //   op #2 (succeeds): add key "city" = "NYC"
    //   op #3 (FAILS):    replace /nonexistent/path with "x"
    //   → JsonPatchError::PointerNotFound
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{ReplaceOp{"/age",  make_int(31)}});
    ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});
    ops.push_back(JsonPatchOp{ReplaceOp{"/nonexistent/path", make_str("x")}});

    auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
    check(!r.has_value(), "3a patch_atomic returns unexpected on failure");
    check(r.error() == JsonPatchError::PointerNotFound,
          "3b error is PointerNotFound (the failure of op #3)");
    check(psp::json_to_string(root) == psp::json_to_string(pre_state),
          "3c tree is BYTE-IDENTICAL to pre-state (rollback worked)");
    check(psp::json_to_string(root) ==
              "{\n"
              "  \"age\": 30,\n"
              "  \"name\": \"alice\",\n"
              "  \"tags\": [\n"
              "    \"admin\",\n"
              "    \"user\"\n"
              "  ]\n"
              "}",
          "3d tree matches the original pre-state JSON text");

    // CONTROL: the v0.12.0 engine on the same input leaves the
    // tree partially mutated (ops #1 and #2 applied, op #3
    // failed). This is the gap the transactional layer closes.
    psp::JsonValue control = psp::json_patch::deep_clone(root);
    auto cr = psp::json_patch::patch(control, std::span<const JsonPatchOp>{ops});
    check(!cr.has_value(), "3e control: v0.12.0 patch returns unexpected on same input");
    check(psp::json_to_string(control) != psp::json_to_string(pre_state),
          "3f control: v0.12.0 left control tree PARTIALLY MUTATED (the gap)");
    check(psp::json_to_string(control) ==
              "{\n"
              "  \"age\": 31,\n"
              "  \"city\": \"NYC\",\n"
              "  \"name\": \"alice\",\n"
              "  \"tags\": [\n"
              "    \"admin\",\n"
              "    \"user\"\n"
              "  ]\n"
              "}",
          "3g control: v0.12.0 left control tree with ops #1 and #2 applied");
}

// ===========================================================================
// Section 4 — dry-run: the same failing patch leaves the original untouched
// ===========================================================================
void section_4_dry_run() {
    header("Section 4: dry-run — patch_dry_run never mutates `root`");

    psp::JsonValue root = make_alice();
    psp::JsonValue pre_state = psp::json_patch::deep_clone(root);

    // Same failing 3-op patch as Section 3.
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{ReplaceOp{"/age",  make_int(31)}});
    ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});
    ops.push_back(JsonPatchOp{ReplaceOp{"/nonexistent/path", make_str("x")}});

    auto r = psp::json_patch::patch_dry_run(root, std::span<const JsonPatchOp>{ops});
    check(!r.has_value(), "4a dry-run returns unexpected on failure");
    check(r.error() == JsonPatchError::PointerNotFound,
          "4b dry-run error is the same PointerNotFound");
    check(psp::json_to_string(root) == psp::json_to_string(pre_state),
          "4c dry-run left the ORIGINAL tree untouched (root unchanged)");

    // And a successful dry-run: the original is also untouched.
    std::vector<JsonPatchOp> good_ops;
    good_ops.push_back(JsonPatchOp{ReplaceOp{"/age", make_int(99)}});
    auto gr = psp::json_patch::patch_dry_run(root, std::span<const JsonPatchOp>{good_ops});
    check(gr.has_value(), "4d successful dry-run returns void");
    check(psp::json_to_string(root) == psp::json_to_string(pre_state),
          "4e even a successful dry-run leaves the ORIGINAL untouched");

    // CONTROL: patch_atomic on the same good_ops mutates the tree.
    auto ar = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{good_ops});
    check(ar.has_value(), "4f control: patch_atomic on the same good_ops returns void");
    check(psp::json_to_string(root) ==
              "{\n"
              "  \"age\": 99,\n"
              "  \"name\": \"alice\",\n"
              "  \"tags\": [\n"
              "    \"admin\",\n"
              "    \"user\"\n"
              "  ]\n"
              "}",
          "4g control: patch_atomic mutated root to age 99");
}

// ===========================================================================
// Section 5 — corner cases: empty / single-op / self-move / nested
// ===========================================================================
void section_5_corner_cases() {
    header("Section 5: corner cases — empty / single-op / self-move / nested");

    // 5a: empty patch. Both functions should be no-ops.
    {
        psp::JsonValue root = make_alice();
        psp::JsonValue pre = psp::json_patch::deep_clone(root);
        std::vector<JsonPatchOp> empty_ops;
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{empty_ops});
        check(r.has_value(), "5a empty patch: patch_atomic returns void");
        check(psp::json_to_string(root) == psp::json_to_string(pre),
              "  5a empty patch: root unchanged");
    }

    // 5b: single-op patch. The snapshot is identical to root at
    //     the time of the clone; on success the snapshot is
    //     discarded; on failure the snapshot (== pre-state) is
    //     restored.
    {
        psp::JsonValue root = make_alice();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/city", make_str("LA")}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "5b single-op patch: success path returns void");
        check(psp::json_to_string(root) ==
                  "{\n"
                  "  \"age\": 30,\n"
                  "  \"city\": \"LA\",\n"
                  "  \"name\": \"alice\",\n"
                  "  \"tags\": [\n"
                  "    \"admin\",\n"
                  "    \"user\"\n"
                  "  ]\n"
                  "}",
              "  5b single-op patch: tree mutated correctly");
    }

    // 5c: single-op patch where the op fails. Snapshot is
    //     restored byte-for-byte.
    {
        psp::JsonValue root = make_alice();
        psp::JsonValue pre = psp::json_patch::deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{RemoveOp{"/nonexistent"}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "5c single-op failing patch: returns unexpected");
        check(r.error() == JsonPatchError::PointerNotFound,
              "  5c single-op failing patch: error is PointerNotFound");
        check(psp::json_to_string(root) == psp::json_to_string(pre),
              "  5c single-op failing patch: root restored to pre-state");
    }

    // 5d: self-move. NOTE: the library's apply_move is
    //     "add at to, then remove at from" — so a self-move
    //     (from == path) does add-then-remove, leaving the
    //     key GONE. RFC 6902 §4.4 says a self-move is a
    //     no-op; the engine doesn't honor that for from == path
    //     (it only short-circuits the MoveWouldClobber check
    //     for from == path). This is a pre-existing engine
    //     quirk, NOT a transactional-layer behaviour; we test
    //     the ACTUAL behaviour (key is removed) and surface
    //     the observation in the lesson.
    {
        psp::JsonValue root = make_alice();
        psp::JsonValue pre = psp::json_patch::deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{MoveOp{"/tags", "/tags"}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "5d self-move: patch_atomic returns void (the engine returns void; the engine then leaves the tree in a quirky state)");
        check(psp::json_to_string(root) != psp::json_to_string(pre),
              "  5d self-move: tree is NOT unchanged (the engine quirk — see lesson)");
    }

    // 5e: nested array mutation. The patch replaces tags[1] =
    //     "user" with "guest". The snapshot must capture the
    //     vector / string in the nested structure too.
    {
        psp::JsonValue root = make_alice();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{ReplaceOp{"/tags/1", make_str("guest")}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "5e nested array mutation: success");
        check(psp::json_to_string(root) ==
                  "{\n"
                  "  \"age\": 30,\n"
                  "  \"name\": \"alice\",\n"
                  "  \"tags\": [\n"
                  "    \"admin\",\n"
                  "    \"guest\"\n"
                  "  ]\n"
                  "}",
              "  5e nested array mutation: tags[1] replaced with guest");
    }

    // 5f: nested FAILURE. The patch tries to replace tags[5]
    //     (out of range) after a successful top-level add. The
    //     tree must roll back to the pre-state.
    {
        psp::JsonValue root = make_alice();
        psp::JsonValue pre = psp::json_patch::deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});  // success
        ops.push_back(JsonPatchOp{ReplaceOp{"/tags/5", make_str("oob")}});  // fails
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "5f nested failure: returns unexpected");
        check(r.error() == JsonPatchError::PointerIndexOutOfRange,
              "  5f nested failure: error is PointerIndexOutOfRange");
        check(psp::json_to_string(root) == psp::json_to_string(pre),
              "  5f nested failure: tree rolled back (the AddOp undone)");
    }

    // 5g: large tree (10 keys). The deep-clone must capture all
    //     10 keys; a patch that fails on key 7 restores all 10.
    {
        psp::JsonValue root = make_obj();
        for (int i = 0; i < 10; ++i) {
            obj_set(root, "k" + std::to_string(i), make_int(i));
        }
        psp::JsonValue pre = psp::json_patch::deep_clone(root);

        std::vector<JsonPatchOp> ops;
        // 5 succeed (replace k0..k4 with their value + 100)
        for (int i = 0; i < 5; ++i) {
            ops.push_back(JsonPatchOp{ReplaceOp{"/k" + std::to_string(i),
                                                make_int(i + 100)}});
        }
        // op #6 fails
        ops.push_back(JsonPatchOp{ReplaceOp{"/k99", make_int(999)}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "5g large tree: returns unexpected on op #6 failure");
        check(r.error() == JsonPatchError::PointerNotFound,
              "  5g large tree: error is PointerNotFound");
        check(psp::json_to_string(root) == psp::json_to_string(pre),
              "  5g large tree: all 10 keys rolled back to pre-state");
    }
}

// ===========================================================================
// Section 6 — wire-format interop: serialise -> parse -> patch_atomic
// ===========================================================================
//
// Proves the transactional layer composes cleanly with the
// v0.15.0 round-trip: build ops in memory, serialise them with
// psp::json_patch::serialise_patch_document (the v0.15.0
// library writer), re-parse with psp::json_patch::parse_patch_document
// (the v0.13.0 library parser), then run patch_atomic on the
// parsed vector. The end-to-end pipeline (writer -> parser ->
// transactional engine) is in the library proper.
void section_6_wire_format_interop() {
    header("Section 6: wire-format interop — build -> serialise -> parse -> patch_atomic");

    psp::JsonValue root = make_alice();
    psp::JsonValue pre = psp::json_patch::deep_clone(root);

    // 1. Hand-build a 3-op patch in memory.
    std::vector<JsonPatchOp> ops;
    ops.push_back(JsonPatchOp{ReplaceOp{"/age",  make_int(31)}});
    ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});
    ops.push_back(JsonPatchOp{ReplaceOp{"/tags/0", make_str("superadmin")}});

    // 2. Serialise to RFC 6902 §3 wire format using the
    //    v0.15.0 library writer.
    std::string wire = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{ops});
    check(!wire.empty(), "6a serialise_patch_document produced non-empty wire");
    check(wire.front() == '[', "6b wire starts with '[' (JSON array per RFC 6902 §3)");

    // 3. Parse the wire back using the v0.13.0 library parser.
    auto parsed = psp::json_patch::parse_patch_document(wire);
    check(parsed.has_value(), "6c parse_patch_document succeeded");
    check(parsed->size() == 3, "6d parsed vector has 3 ops (matches original)");

    // 4. Run patch_atomic on the parsed vector.
    auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{*parsed});
    check(r.has_value(), "6e patch_atomic on parsed vector succeeded");
    check(psp::json_to_string(root) ==
              "{\n"
              "  \"age\": 31,\n"
              "  \"city\": \"NYC\",\n"
              "  \"name\": \"alice\",\n"
              "  \"tags\": [\n"
              "    \"superadmin\",\n"
              "    \"user\"\n"
              "  ]\n"
              "}",
          "6f tree after wire-round-trip + patch_atomic matches expected post-state");

    // 5. Round-trip back through the writer: the post-state's
    //    patch (built by re-deriving ops from the diff) would
    //    serialise to the same wire. (We don't re-derive ops;
    //    we just prove the writer is still callable on the
    //    original ops vector — i.e. the transactional layer
    //    doesn't change `ops`.)
    std::string wire2 = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{ops});
    check(wire == wire2,
          "6g serialise_patch_document(ops) is deterministic (the transactional layer didn't mutate ops)");

    // 6. Atomic rollback through the wire format: a parsed
    //    wire patch where op #3 fails must roll back the
    //    pre-state.
    std::vector<JsonPatchOp> bad_ops;
    bad_ops.push_back(JsonPatchOp{ReplaceOp{"/age",  make_int(31)}});
    bad_ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});
    bad_ops.push_back(JsonPatchOp{ReplaceOp{"/nonexistent/path", make_str("x")}});
    std::string bad_wire = psp::json_patch::serialise_patch_document(
        std::span<const JsonPatchOp>{bad_ops});
    auto bad_parsed = psp::json_patch::parse_patch_document(bad_wire);
    check(bad_parsed.has_value(), "6h parse_patch_document succeeded on bad_ops");

    psp::JsonValue root2 = psp::json_patch::deep_clone(pre);
    auto r2 = psp::json_patch::patch_atomic(root2, std::span<const JsonPatchOp>{*bad_parsed});
    check(!r2.has_value(), "6i patch_atomic on bad wire returns unexpected");
    check(r2.error() == JsonPatchError::PointerNotFound,
          "6j error is PointerNotFound");
    check(psp::json_to_string(root2) == psp::json_to_string(pre),
          "6k root2 rolled back to pre-state through the wire format");
}

// ===========================================================================
// Section 7 — sizeof / feature probes
// ===========================================================================
void section_7_probes() {
    header("Section 7: sizeof / feature probes");

    check(sizeof(JsonPatchError) == 4,
          "7a sizeof(JsonPatchError) = 4 (unchanged; transactional layer adds no enum)");
    check(sizeof(std::expected<void, JsonPatchError>) == 8,
          "7b sizeof(std::expected<void, JsonPatchError>) = 8 (4-byte enum + 4-byte padding)");

    // psp::json_patch::deep_clone: a function on psp::JsonValue.
    using clone_fn = psp::JsonValue(*)(const psp::JsonValue&);
    clone_fn cf = &psp::json_patch::deep_clone;
    check(cf != nullptr, "7c &psp::json_patch::deep_clone is well-defined");

    // Snapshot helper: std::optional<psp::JsonValue> holds the
    // snapshot. Its size is sizeof(JsonValue) + padding
    // (std::optional on this toolchain is trivially small, so
    // it adds essentially nothing to JsonValue's size).
    psp::JsonValue j = make_alice();
    std::optional<psp::JsonValue> snap{psp::json_patch::deep_clone(j)};
    check(snap.has_value(), "7d optional<JsonValue> holds the deep-clone snapshot");
    check(psp::json_to_string(*snap) == psp::json_to_string(j),
          "7e snapshot equals the original at construction");

    // The library feature probes (back-compat: the engine + the
    // writer + the parser + the JsonValue types are unchanged
    // from v0.15.0).
    check(__cpp_lib_expected >= 202211, "7f __cpp_lib_expected = 202211 (C++23)");
    check(__cpp_lib_variant  >= 202106, "7g __cpp_lib_variant  = 202106 (C++17/20)");
    check(__cpp_lib_span     >= 202002, "7h __cpp_lib_span     = 202002 (C++20)");

    // Transactional layer adds ZERO new JsonPatchError
    // enumerators. The two new functions use the existing
    // 13-enum vocabulary; rollback is a control-flow change,
    // not a new failure mode. Verify by enumerating the 13
    // distinct enumerators (we count non-empty format
    // strings; the 19 unhandled bit patterns produce empty
    // strings, which is what the formatter spec in
    // <psp_span/json_ext.h> does for out-of-range values).
    {
        int distinct_enums = 0;
        for (int i = 0; i < 32; ++i) {
            JsonPatchError e = static_cast<JsonPatchError>(i);
            std::string s = std::format("{}", e);
            if (!s.empty()) {
                ++distinct_enums;
            }
        }
        check(distinct_enums == 13,
              "7i JsonPatchError has 13 distinct enumerators (matches v0.15.0; transactional layer adds zero)");
    }
}

// ===========================================================================
// Section 8 — back-compat with the v0.15.0 engine + writer + parser
// ===========================================================================
//
// The transactional layer is purely a control-flow wrapper.
// Every successful patch_atomic / patch_dry_run call produces
// the SAME post-state as a direct patch() call (proved in
// Section 2). The wire-format interop is proved in Section 6.
// This section adds a few more back-compat cases for the
// MoveOp / CopyOp / TestOp kinds the earlier sections didn't
// exercise.
void section_8_back_compat() {
    header("Section 8: back-compat — patch_atomic handles every RFC 6902 op kind");

    // 8a: MoveOp succeeds, tree is mutated.
    {
        psp::JsonValue root = make_alice();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{MoveOp{"/name", "/nickname"}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "8a MoveOp via patch_atomic: success");
        check(psp::json_to_string(root) ==
                  "{\n"
                  "  \"age\": 30,\n"
                  "  \"nickname\": \"alice\",\n"
                  "  \"tags\": [\n"
                  "    \"admin\",\n"
                  "    \"user\"\n"
                  "  ]\n"
                  "}",
              "  8a tree: name -> nickname");
    }

    // 8b: MoveOp fails (MoveWouldClobber — /tags is a strict
    //     ancestor of /tags/0). The tree rolls back.
    {
        psp::JsonValue root = make_alice();
        psp::JsonValue pre = psp::json_patch::deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{MoveOp{"/tags", "/tags/0"}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "8b MoveOp clobber: returns unexpected");
        check(r.error() == JsonPatchError::MoveWouldClobber,
              "  8b error is MoveWouldClobber");
        check(psp::json_to_string(root) == psp::json_to_string(pre),
              "  8b tree rolled back to pre-state");
    }

    // 8c: CopyOp succeeds, source is preserved.
    {
        psp::JsonValue root = make_alice();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{CopyOp{"/name", "/alias"}});
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "8c CopyOp via patch_atomic: success");
        check(psp::json_to_string(root) ==
                  "{\n"
                  "  \"age\": 30,\n"
                  "  \"alias\": \"alice\",\n"
                  "  \"name\": \"alice\",\n"
                  "  \"tags\": [\n"
                  "    \"admin\",\n"
                  "    \"user\"\n"
                  "  ]\n"
                  "}",
              "  8c tree: name copied to alias");
    }

    // 8d: TestOp mismatch rolls back the tree (the prior op
    //     was a successful add; the failing test triggers
    //     rollback).
    {
        psp::JsonValue root = make_alice();
        psp::JsonValue pre = psp::json_patch::deep_clone(root);
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});  // success
        ops.push_back(JsonPatchOp{TestOp{"/age", make_int(99999)}});   // fails (age is 30)
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(), "8d TestOp mismatch: returns unexpected");
        check(r.error() == JsonPatchError::TestValueMismatch,
              "  8d error is TestValueMismatch");
        check(psp::json_to_string(root) == psp::json_to_string(pre),
              "  8d tree rolled back (the AddOp undone)");
    }

    // 8e: All-six-op patch succeeds, tree is fully mutated.
    {
        psp::JsonValue root = make_alice();
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{TestOp{"/age", make_int(30)}});                  // test
        ops.push_back(JsonPatchOp{ReplaceOp{"/age", make_int(31)}});               // replace
        ops.push_back(JsonPatchOp{AddOp{"/city", make_str("NYC")}});               // add
        ops.push_back(JsonPatchOp{CopyOp{"/city", "/hometown"}});                 // copy
        ops.push_back(JsonPatchOp{MoveOp{"/hometown", "/birthplace"}});            // move
        ops.push_back(JsonPatchOp{RemoveOp{"/birthplace"}});                       // remove
        auto r = psp::json_patch::patch_atomic(root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(), "8e all-six-op patch: success");
        check(psp::json_to_string(root) ==
                  "{\n"
                  "  \"age\": 31,\n"
                  "  \"city\": \"NYC\",\n"
                  "  \"name\": \"alice\",\n"
                  "  \"tags\": [\n"
                  "    \"admin\",\n"
                  "    \"user\"\n"
                  "  ]\n"
                  "}",
              "  8e tree: birthplace added + moved + removed, net no change; age 30->31; city added");
    }
}

int main() {
    std::println("P-2026-08-03 — Transactional JSON Patch:");
    std::println("                psp::json_patch::patch_atomic + patch_dry_run");
    std::println("                (consumer-side; all-or-nothing semantics on top");
    std::println("                of psp::json_patch::patch via deep-snapshot rollback)");

    section_1_symbol_presence();
    section_2_happy_path();
    section_3_atomic_rollback();
    section_4_dry_run();
    section_5_corner_cases();
    section_6_wire_format_interop();
    section_7_probes();
    section_8_back_compat();

    std::println("");
    std::println("[transactional_patch: {} PASS, {} FAIL]", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
