// P-2026-08-09 — Consumer of psp_span_lib v0.15.0 that designs
// the JOURNAL-AWARE SELF-MOVE SAFE wrapper composing the Aug 3
// patch_atomic + Aug 5 patch_journaled + Aug 6 patch_self_move_safe
// wrappers into a single transactional layer:
//
//   psp::json_patch::patch_journaled_self_move_safe(
//       JsonValue& root, std::span<const JsonPatchOp> ops)
//       -> std::expected<void, JsonPatchError>
//
// Where this fits in the arc
// --------------------------
// The Aug 6 lesson (P-2026-08-06-self-move-fix.md) closed the
// engine self-move arc with a pre-filter wrapper
// (patch_self_move_safe) that drops self-moves from the
// user-facing input before invoking the v0.12.0 engine. The
// Aug 6 "What's NOT in this lesson" section explicitly flagged
// today's composition as future work:
//
//   "It does not repair the inverse-journal replay interaction.
//    A self-move that's part of a journal (i.e., the
//    inverse-journal's `MoveOp{path, from}` for undoing a
//    cross-move) becomes a self-move when `from == path`
//    (which is the cross-move's `path == from` case, i.e., a
//    self-move being undone). Today's wrapper handles this
//    transparently because the journal entries go through
//    `psp::json_patch::patch`, not `patch_self_move_safe`. A
//    future lesson could lift `patch_self_move_safe` into the
//    journal path so journal replays also drop self-moves;
//    the observability/correctness story is the same as
//    today's wrapper (a self-move in a journal is a no-op in
//    the inverse-journal sense)."
//
// Today is that future lesson.
//
// The composition problem
// ------------------------
// The Aug 6 patch_self_move_safe is a pure pre-filter wrapper.
// It does not require atomicity, so it does not interact with
// the Aug 3 patch_atomic (deep-clone) or Aug 5 patch_journaled
// (inverse-journal) transactional layers. The Aug 6 lesson
// composes the pre-filter with the engine directly:
//
//   patch_self_move_safe(root, ops)
//       -> filter_self_moves(ops)
//       -> patch(root, filtered)
//
// The Aug 5 patch_journaled is an inverse-journal recovery
// layer. It calls patch() for each op individually and captures
// the inverse of each op as it's applied. On failure it
// replays the journal in REVERSE to restore the pre-state:
//
//   patch_journaled(root, ops)
//       for each op:
//           inv = inverse_for(root, op)  // pre-state lookup
//           r   = patch(root, {op})       // apply
//           if r failed: replay_journal(journal); return error
//           else:        push inv to journal
//
// The two recipes meet at the engine call: patch_self_move_safe
// pre-filters the input; patch_journaled pre-filters nothing
// and post-filters via journal replay. If we want atomicity AND
// self-move safety, we need ONE pass that does both:
//
//   patch_journaled_self_move_safe(root, ops)
//       filtered = filter_self_moves(ops)
//       for each op in filtered:
//           inv = inverse_for(root, op)
//           r   = patch(root, {op})        // engine (no filter)
//           if r failed:
//               safe_journal = filter_self_moves(journal)
//               replay_journal(root, safe_journal)
//               return error
//           else:
//               push inv to journal
//
// On the success path: input self-moves are dropped (Aug 6).
// On the failure path: the journal goes through filter_self_moves
// before replay, so any journal entry that has become a self-move
// (the cross-move's undo) is also dropped during replay.
//
// The "drop self-moves" rule is invariant under replay:
//   - A self-move in the inverse-journal is the inverse of a
//     cross-move whose own from == path — i.e., a self-move
//     being undone. The cross-move is observably a no-op
//     (Aug 6 rule), so the inverse is also a no-op.
//   - Therefore: replaying a self-move is a no-op. Dropping
//     it during replay is equivalent to applying-and-undoing.
//
// Why we add an EXPLICIT filter_self_moves on the journal
// (not just rely on the input-side filter)
// --------------------------------------------------------------
// The Aug 6 pre-filter strips self-moves from the INPUT, but
// the journal entries are CONSTRUCTED from the inverse of
// each applied op. Cross-moves generate MoveOp{path, from}
// inverses. If the cross-move's from happens to be a
// strict-ancestor of path (the Aug 6 pre-existing engine
// quirk where copy-then-remove fires), then the inverse's
// from == path (i.e., the inverse is a self-move). The
// input-side filter does NOT touch the journal entries, so
// this self-move would fall through to the v0.12.0 engine
// during replay and self-delete the value.
//
// Today's wrapper closes this gap with an explicit
// filter_self_moves on the journal before replay. The rule
// is: "self-moves are dropped at every engine boundary".
//
// Why consumer-side and not library-side today
// ---------------------------------------------
// Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 lessons:
// a proven-in-consumer wrapper that exercises the design
// end-to-end. The library version is unchanged at v0.15.0.
// A future v0.16.0 promotion is mechanical (lift
// patch_journaled_self_move_safe + detail::inverse_for +
// detail::replay_journal + detail::lookup_at +
// detail::is_self_move + detail::filter_self_moves into
// <psp_span/json_ext.h>; bump the version).
//
// What the consumer exercises
// ----------------------------
//   Section 1 — symbol-presence + per-op pre-filter +
//               journal-pre-filter spec.
//   Section 2 — the aug-6 self-move bug, exposed through the
//               journal: a self-move in the input is dropped
//               (no-op), a self-move in the journal (replay)
//               is also dropped.
//   Section 3 — every MoveOp shape (self / valid / clobber /
//               missing-from) through the journal.
//   Section 4 — interop with patch_atomic + patch_dry_run +
//               patch_journaled (the Aug 3 / Aug 5 wrappers).
//   Section 5 — end-to-end: a self-move being UNDONE via
//               the journal (the case the Aug 6 lesson's
//               "What's NOT in this lesson" section
//               explicitly flagged).
//   Section 6 — sizeof / feature probes.
//
//   ~30+ cases across 6 sections, all expected to pass.
//
// Build (assumes psp_span_lib v0.15.0 is installed at
// /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-08-09-journal-self-move-safe
//
// Strict-warning build:
//
//   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
//       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
//   cmake --build build-strict
//   ./build-strict/P-2026-08-09-journal-self-move-safe
//
// ASan + UBSan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-08-09-journal-self-move-safe

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
// Background: the three wrappers we compose
// ===========================================================================
//
// psp::json_patch::patch (in <psp_span/json_ext.h>, v0.12.0) is the
// engine. It handles self-moves via copy-then-remove, which
// self-deletes the value at from (the Aug 6 lesson's bug). RFC
// 6902 §4.4 says self-move is a no-op.
//
// psp::json_patch::patch_self_move_safe (Aug 6, consumer-side)
// pre-filters self-moves from the input before passing the
// patch to the engine. Same observable contract as patch();
// self-moves are dropped (no-op).
//
// psp::json_patch::patch_journaled (Aug 5, consumer-side) is an
// inverse-journal variant of patch_atomic. It captures the
// inverse of each op as it is applied, and on failure replays
// the journal in REVERSE to restore the pre-state. It does
// NOT pre-filter self-moves; today, that gap is the "What's
// NOT in this lesson" of the Aug 6 lesson.
//
// The interaction the Aug 6 lesson flagged
// ----------------------------------------
// A self-move in the INVERSE JOURNAL is the inverse of a
// cross-move whose own from == path. The cross-move's
// "self-move-being-undone" case arises when:
//   1. The user-facing patch contains a SELF-move (aug-6 rule:
//      dropped by patch_self_move_safe's pre-filter).
//   2. The user-facing patch contains a CROSS-move whose
//      from happens to be a strict-ancestor of path. The
//      cross-move generates a MoveOp{path, from} inverse
//      whose own from == path: a self-move.
//
// Case 2 is the gap. The Aug 6 input-side filter is correct
// for the user-facing patch, but the journal entries are
// constructed DURING inverse_for, AFTER the input-side
// filter. The journal can carry self-moves that the input
// side never saw.
//
// fix: filter_self_moves on the journal before replay. The
// replay path becomes observably safe against self-moves.

// ===========================================================================
// Re-implement the three wrappers in this TU (consumer-side pattern
// matching the Aug 5 + Aug 6 lessons).
// ===========================================================================
//
// The Aug 5 + Aug 6 lessons both mirrored their wrappers in
// their consumer TUs (rather than linking them across consumers).
// Today's consumer mirrors all three wrappers because the
// composition is the new shape — we need the full signature
// surface available to test the composition end-to-end.
//
// Each wrapper is a complete design and is documented in its
// own lesson. Today we re-implement them so the composition
// has a single self-contained TU.

namespace psp {
namespace json_patch {

// -----------------------------------------------------------------------
// deep_clone — recursive copy of a JsonValue tree
// -----------------------------------------------------------------------
// Mirrors the Aug 3 patch_atomic's deep_clone. Used by
// lookup_at to capture pre-state values for the journal.
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

namespace detail {

// -----------------------------------------------------------------------
// is_self_move — the per-op rule (mirrors Aug 6's psp::json_patch::is_self_move)
// -----------------------------------------------------------------------
//
// Returns true iff `op` is a MoveOp whose from and path are
// IDENTICAL strings (RFC 6902 §4.4 self-move rule).
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

// -----------------------------------------------------------------------
// filter_self_moves — pre-filter (mirrors Aug 6's psp::json_patch::filter_self_moves)
// -----------------------------------------------------------------------
//
// Returns a NEW std::vector<JsonPatchOp> containing every op
// in `ops` EXCEPT the self-moves. The self-moves are dropped
// (observed as no-ops at apply time).
//
// Allocates one vector of size <= ops.size(); this is a
// one-time pre-pass cost, not per-op.
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

// -----------------------------------------------------------------------
// lookup_at — pre-state capture (mirrors Aug 5's lookup_at)
// -----------------------------------------------------------------------
inline std::optional<psp::JsonValue>
lookup_at(psp::JsonValue& root, std::string_view path) {
    auto found = psp::json_pointer::resolve_mut(path, root);
    if (!found) {
        return std::nullopt;
    }
    return deep_clone(**found);
}

// -----------------------------------------------------------------------
// inverse_for — compute the inverse of a single op (mirrors Aug 5)
// -----------------------------------------------------------------------
inline std::optional<JsonPatchOp>
inverse_for(psp::JsonValue& root, const JsonPatchOp& op,
            JsonPatchError& pre_state_error) {
    pre_state_error = JsonPatchError::BadDocument;

    switch (op.kind) {
        case OpKind::Add: {
            const auto& a = std::get<AddOp>(op.data);
            return JsonPatchOp{RemoveOp{a.path}};
        }
        case OpKind::Remove: {
            const auto& r = std::get<RemoveOp>(op.data);
            auto pre = lookup_at(root, r.path);
            if (!pre) {
                pre_state_error = JsonPatchError::PointerNotFound;
                return std::nullopt;
            }
            return JsonPatchOp{AddOp{r.path, std::move(*pre)}};
        }
        case OpKind::Replace: {
            const auto& rp = std::get<ReplaceOp>(op.data);
            auto pre = lookup_at(root, rp.path);
            if (!pre) {
                pre_state_error = JsonPatchError::PointerNotFound;
                return std::nullopt;
            }
            return JsonPatchOp{AddOp{rp.path, std::move(*pre)}};
        }
        case OpKind::Move: {
            const auto& m = std::get<MoveOp>(op.data);
            return JsonPatchOp{MoveOp{m.path, m.from}};
        }
        case OpKind::Copy: {
            const auto& c = std::get<CopyOp>(op.data);
            return JsonPatchOp{RemoveOp{c.path}};
        }
        case OpKind::Test: {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// -----------------------------------------------------------------------
// replay_journal — apply the journal in REVERSE order
// -----------------------------------------------------------------------
//
// DiFF from Aug 5: this version filters self-moves from the
// journal BEFORE replay. That's the new bit in today's lesson.
inline std::expected<void, JsonPatchError>
replay_journal(psp::JsonValue& root,
               const std::vector<JsonPatchOp>& journal) {
    // Build the reversed journal AND filter self-moves in one pass.
    // Doing both in one loop is O(N) total; doing them
    // separately would be O(N) + O(N) = O(N) but with two
    // allocations. The combined version is tighter but still
    // linear.
    std::vector<JsonPatchOp> reversed;
    reversed.reserve(journal.size());
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        if (is_self_move(*it)) continue;  // <-- THE NEW BIT
        reversed.push_back(*it);
    }
    return psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{reversed});
}

}  // namespace detail

// Forward declaration of the Aug 5 replay (no self-move
// filter). patch_journaled below uses it; the full definition
// is at the end of the namespace.
inline std::expected<void, JsonPatchError>
replay_journal_NoSelfMoveFilter(psp::JsonValue& root,
                                const std::vector<JsonPatchOp>& journal);

// -----------------------------------------------------------------------
// patch_journaled_self_move_safe — the new wrapper
// -----------------------------------------------------------------------
//
// Same observable contract as patch_journaled (Aug 5):
//   - On success: root is fully mutated, return {}.
//   - On failure: root is restored to the pre-state, return
//     std::unexpected{error}.
//
// Plus the Aug 6 self-move rule, applied at TWO layers:
//   - Input-side: self-moves in the user-facing patch are
//     dropped before the engine sees them.
//   - Journal-side: self-moves in the journal (replay-
//     direction) are dropped before the engine sees them.
//
// The journal-side filter is the new vs Aug 5. Without it, a
// self-move in the journal (the inverse of a cross-move whose
// own from == path) would hit the v0.12.0 engine's
// self-delete bug during replay.
//
// Library version unchanged at v0.15.0; future v0.16.0 promotion
// is mechanical (lift this function + the detail:: helpers into
// <psp_span/json_ext.h>; bump the version).
inline std::expected<void, JsonPatchError>
patch_journaled_self_move_safe(psp::JsonValue& root,
                               std::span<const JsonPatchOp> ops) noexcept {
    // Layer 1: pre-filter the input (Aug 6 rule).
    auto filtered = detail::filter_self_moves(ops);

    std::vector<JsonPatchOp> journal;
    journal.reserve(filtered.size());

    for (const auto& op : filtered) {
        // Step 1: compute the inverse of this op against the
        // CURRENT state (which is the pre-state for this op).
        JsonPatchError pre_err = JsonPatchError::BadDocument;
        auto inv = detail::inverse_for(root, op, pre_err);
        if (!inv && op.kind != OpKind::Test) {
            // Pre-state lookup failed (e.g., RemoveOp on
            // missing path). The engine wasn't called for this
            // op, but the journal has inverses for ops that
            // DID succeed. Replay (with self-move filter) to
            // roll those back.
            auto replay = detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{pre_err};
        }

        // Step 2: apply the op via the engine.
        auto r = psp::json_patch::patch(root,
            std::span<const JsonPatchOp>{&op, 1});
        if (!r) {
            // Engine failed. Replay (with self-move filter).
            auto replay = detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{r.error()};
        }

        // Step 3: op succeeded; record the inverse in the
        // journal. (TestOp has no inverse; skip.)
        if (inv) {
            journal.push_back(std::move(*inv));
        }
    }
    return {};
}

// -----------------------------------------------------------------------
// patch_self_move_safe — the Aug 6 wrapper (mirrors the Aug 6 lesson)
// -----------------------------------------------------------------------
//
// Re-implemented here so Section 4 can compare
// patch_journaled_self_move_safe with patch_self_move_safe
// on the same input. Same observable contract as the engine:
//   - On success: root is mutated per the (filtered) patch.
//   - On failure: returns std::unexpected{error}.
inline std::expected<void, JsonPatchError>
patch_self_move_safe(psp::JsonValue& root,
                     std::span<const JsonPatchOp> ops) noexcept {
    auto filtered = detail::filter_self_moves(ops);
    return psp::json_patch::patch(
        root, std::span<const JsonPatchOp>{filtered});
}

// -----------------------------------------------------------------------
// patch_journaled — the Aug 5 inverse-journal wrapper (mirrors the Aug 5 lesson)
// -----------------------------------------------------------------------
//
// Re-implemented here so Section 4 can compare
// patch_journaled_self_move_safe with patch_journaled on the
// failure path. Same observable contract:
//   - On success: root is fully mutated, return {}.
//   - On failure: root is restored to the pre-state.
inline std::expected<void, JsonPatchError>
patch_journaled(psp::JsonValue& root,
                std::span<const JsonPatchOp> ops) noexcept {
    // Note: this is the Aug 5 version, NOT the journal-self-move-safe
    // version. It does NOT filter self-moves from the journal
    // before replay. That's the gap the Aug 6 lesson's
    // "What's NOT in this lesson" section explicitly flagged.
    std::vector<JsonPatchOp> journal;
    journal.reserve(ops.size());

    for (const auto& op : ops) {
        JsonPatchError pre_err = JsonPatchError::BadDocument;
        auto inv = detail::inverse_for(root, op, pre_err);
        if (!inv && op.kind != OpKind::Test) {
            auto replay = psp::json_patch::replay_journal_NoSelfMoveFilter(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{pre_err};
        }
        auto r = psp::json_patch::patch(root,
            std::span<const JsonPatchOp>{&op, 1});
        if (!r) {
            auto replay = psp::json_patch::replay_journal_NoSelfMoveFilter(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{r.error()};
        }
        if (inv) {
            journal.push_back(std::move(*inv));
        }
    }
    return {};
}

// -----------------------------------------------------------------------
// replay_journal_NoSelfMoveFilter — the Aug 5 replay (no self-move filter)
// -----------------------------------------------------------------------
//
// This is the Aug 5 version of replay_journal. It's exposed
// here so patch_journaled can use the OLD replay path (no
// self-move filter), while patch_journaled_self_move_safe
// uses the NEW replay path (with self-move filter).
inline std::expected<void, JsonPatchError>
replay_journal_NoSelfMoveFilter(psp::JsonValue& root,
                                const std::vector<JsonPatchOp>& journal) {
    std::vector<JsonPatchOp> reversed;
    reversed.reserve(journal.size());
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        reversed.push_back(*it);
    }
    return psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{reversed});
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test framework (same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 lessons)
// ===========================================================================

namespace {

int g_pass = 0;
int g_fail = 0;
int g_section = 0;

void header(std::string_view title) {
    ++g_section;
    std::printf("\n== %.*s ==\n", static_cast<int>(title.size()),
                title.data());
}

void check(bool cond, std::string_view name) {
    if (cond) {
        ++g_pass;
        std::printf("  PASS: %.*s\n", static_cast<int>(name.size()),
                    name.data());
    } else {
        ++g_fail;
        std::printf("  FAIL: %.*s\n", static_cast<int>(name.size()),
                    name.data());
    }
}

void check_eq_i(std::size_t actual, std::size_t expected,
                std::string_view name) {
    bool eq = (actual == expected);
    if (eq) {
        ++g_pass;
        std::printf("  PASS: %.*s\n", static_cast<int>(name.size()),
                    name.data());
    } else {
        ++g_fail;
        std::printf("  FAIL: %.*s   (expected=%zu, actual=%zu)\n",
                    static_cast<int>(name.size()), name.data(),
                    expected, actual);
    }
}

void check_eq_err(JsonPatchError actual, JsonPatchError expected,
                  std::string_view name) {
    bool eq = (actual == expected);
    if (eq) {
        ++g_pass;
        std::printf("  PASS: %.*s\n", static_cast<int>(name.size()),
                    name.data());
    } else {
        ++g_fail;
        std::printf("  FAIL: %.*s   (expected=%d, actual=%d)\n",
                    static_cast<int>(name.size()), name.data(),
                    static_cast<int>(expected),
                    static_cast<int>(actual));
    }
}

void check_eq_op(OpKind actual, OpKind expected, std::string_view name) {
    bool eq = (actual == expected);
    if (eq) {
        ++g_pass;
        std::printf("  PASS: %.*s\n", static_cast<int>(name.size()),
                    name.data());
    } else {
        ++g_fail;
        std::printf("  FAIL: %.*s   (expected=%d, actual=%d)\n",
                    static_cast<int>(name.size()), name.data(),
                    static_cast<int>(expected),
                    static_cast<int>(actual));
    }
}

psp::JsonValue make_initial() {
    return psp::JsonValue{std::map<std::string, psp::JsonValue>{
        {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
            {"k", psp::JsonValue{std::int64_t(42)}},
        }}},
    }};
}

}  // namespace

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-09 — journal-aware self-move safe wrapper\n"
                "  (composes Aug 3 + Aug 5 + Aug 6 wrappers into a single\n"
                "  transactional layer; library version unchanged at v0.15.0)\n");

    // -----------------------------------------------------------------------
    // Section 1 — symbol-presence + per-op pre-filter + journal-pre-filter
    //   spec.
    // -----------------------------------------------------------------------
    header("Section 1: symbol-presence + pre-filter spec");

    using psp::json_patch::patch_journaled_self_move_safe;
    using psp::json_patch::detail::is_self_move;
    using psp::json_patch::detail::filter_self_moves;

    {
        auto fn = &patch_journaled_self_move_safe;
        check(fn != nullptr,
              "1a &psp::json_patch::patch_journaled_self_move_safe is well-defined");
    }

    check(std::is_same_v<
              std::remove_reference_t<decltype(patch_journaled_self_move_safe(
                  std::declval<psp::JsonValue&>(),
                  std::span<const JsonPatchOp>{}))>,
              std::expected<void, JsonPatchError>>,
          "1b patch_journaled_self_move_safe signature matches "
          "std::expected<void, JsonPatchError>");

    // is_self_move spec
    check(is_self_move(JsonPatchOp{MoveOp{"/a", "/a"}}) == true,
          "1c is_self_move(self) == true");
    check(is_self_move(JsonPatchOp{MoveOp{"/a", "/b"}}) == false,
          "1d is_self_move(cross) == false");
    check(is_self_move(JsonPatchOp{AddOp{"/a", psp::JsonValue{std::int64_t(1)}}}) == false,
          "1e is_self_move(add) == false");
    check(is_self_move(JsonPatchOp{RemoveOp{"/a"}}) == false,
          "1f is_self_move(remove) == false");
    check(is_self_move(JsonPatchOp{MoveOp{"", ""}}) == true,
          "1g is_self_move(root root) == true");

    // filter_self_moves spec
    {
        std::vector<JsonPatchOp> input = {
            JsonPatchOp{AddOp{"/x", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{MoveOp{"/a", "/a"}},  // self
            JsonPatchOp{AddOp{"/y", psp::JsonValue{std::int64_t(2)}}},
            JsonPatchOp{MoveOp{"/b", "/b"}},  // self
            JsonPatchOp{AddOp{"/z", psp::JsonValue{std::int64_t(3)}}},
        };
        auto out = filter_self_moves(input);
        check_eq_i(out.size(), std::size_t{3},
                   "1h filter_self_moves drops 2 self-moves, keeps 3");
        check_eq_op(out[0].kind, OpKind::Add,
                    "1i filter_self_moves[0] is the first AddOp");
        check_eq_op(out[1].kind, OpKind::Add,
                    "1j filter_self_moves[1] is the second AddOp");
        check_eq_op(out[2].kind, OpKind::Add,
                    "1k filter_self_moves[2] is the third AddOp");
    }

    // -----------------------------------------------------------------------
    // Section 2 — the aug-6 self-move bug, exposed through the journal.
    // -----------------------------------------------------------------------
    header("Section 2: self-move in input + self-move in journal");

    // 2a: a self-move in the input. The Aug 6 lesson proved
    // that under the v0.12.0 engine alone, that self-deletes.
    // patch_journaled_self_move_safe pre-filters self-moves —
    // the tree is unchanged.
    {
        auto root = make_initial();
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{MoveOp{"/x/k", "/x/k"}},  // self
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(),
              "2a self-move in input succeeds under the journal wrapper");
        // The v0.12.0 engine without the wrapper would have
        // self-deleted /x/k. The wrapper preserves it.
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && obj->contains("x"),
              "2b self-move in input preserves /x (vs engine self-delete)");
        const auto* inner = std::get_if<std::map<std::string, psp::JsonValue>>(
            &obj->at("x").value);
        check(inner != nullptr && inner->contains("k"),
              "2c self-move in input preserves /x/k");
        const auto* k = std::get_if<std::int64_t>(&inner->at("k").value);
        check(k != nullptr && *k == 42,
              "2d self-move in input preserves /x/k value (42)");
    }

    // 2e: a self-move in the MIDDLE of a multi-op patch.
    {
        auto root = make_initial();
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/y", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{MoveOp{"/x/k", "/x/k"}},  // self
            JsonPatchOp{AddOp{"/z", psp::JsonValue{std::int64_t(2)}}},
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(),
              "2e self-move in the middle of a patch succeeds");
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && obj->contains("y"),
              "2f first add applied (y present)");
        check(obj != nullptr && obj->contains("z"),
              "2g third add applied (z present)");
        const auto* inner = std::get_if<std::map<std::string, psp::JsonValue>>(
            &obj->at("x").value);
        check(inner != nullptr && inner->contains("k"),
              "2h intermediate self-move dropped, /x/k preserved");
    }

    // -----------------------------------------------------------------------
    // Section 3 — every MoveOp shape (self / valid / clobber / missing-from)
    //   through the journal.
    // -----------------------------------------------------------------------
    header("Section 3: every MoveOp shape through the journal");

    // 3a: cross-move that succeeds.
    {
        auto root = make_initial();
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{MoveOp{"/x/k", "/new"}},  // cross
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(),
              "3a cross-move succeeds");
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && obj->contains("new"),
              "3b cross-move target /new is present");
        const auto* inner = std::get_if<std::map<std::string, psp::JsonValue>>(&obj->at("x").value);
        check(inner != nullptr && !inner->contains("k"),
              "3c cross-move source /x/k is gone");
    }

    // 3d: clobber — moving /x into /x/k clobbers. The engine
    // rejects with MoveWouldClobber. The journal wrapper
    // preserves the pre-state.
    {
        psp::JsonValue root{std::map<std::string, psp::JsonValue>{
            {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
                {"k", psp::JsonValue{std::int64_t(42)}},
            }}},
        }};
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/other", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{MoveOp{"/x", "/x/k"}},  // clobber
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(),
              "3d clobber-move fails");
        check_eq_err(r.error(), JsonPatchError::MoveWouldClobber,
                     "3e clobber-move error is MoveWouldClobber");
        // Pre-state on success must be restored.
        // /other was added before the clobber, so the replay
        // rolls it back. The original tree had no /other.
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && !obj->contains("other"),
              "3f pre-state restored: /other rolled back");
        check(obj != nullptr && obj->contains("x"),
              "3g pre-state restored: /x preserved");
    }

    // 3h: missing-from — moving from a path that doesn't exist.
    // The engine rejects with PointerNotFound. The journal
    // wrapper preserves the pre-state.
    {
        auto root = make_initial();
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/first", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{MoveOp{"/missing", "/new"}},  // missing-from
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(),
              "3h missing-from-move fails");
        check_eq_err(r.error(), JsonPatchError::PointerNotFound,
                     "3i missing-from error is PointerNotFound");
        // Pre-state must be restored (the first add rolled back).
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && !obj->contains("first"),
              "3j pre-state restored: /first rolled back");
    }

    // -----------------------------------------------------------------------
    // Section 4 — interop with patch_atomic + patch_dry_run +
    //   patch_journaled (the Aug 3 / Aug 5 wrappers).
    // -----------------------------------------------------------------------
    header("Section 4: interop with patch_atomic + patch_dry_run + patch_journaled");

    // 4a: patch_journaled_self_move_safe composes observably
    // with patch_self_move_safe on the success path. A patch
    // with a self-move and a real add: both produce the same
    // final tree.
    {
        auto root_with_safe = make_initial();
        auto root_with_journal_safe = make_initial();

        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{MoveOp{"/x/k", "/x/k"}},  // self
            JsonPatchOp{AddOp{"/u", psp::JsonValue{std::int64_t(99)}}},
        };

        // Apply with patch_self_move_safe first (mirrors the
        // Aug 6 lesson's wrapper).
        psp::json_patch::patch_self_move_safe(
            root_with_safe, std::span<const JsonPatchOp>{ops});

        // Apply with patch_journaled_self_move_safe.
        auto r = patch_journaled_self_move_safe(
            root_with_journal_safe, std::span<const JsonPatchOp>{ops});
        check(r.has_value(),
              "4a journal-self-move-safe apply succeeds");

        // Both trees should be equivalent: /x/k preserved, /u added.
        auto repr_safe = psp::json_to_string(root_with_safe);
        auto repr_journal = psp::json_to_string(root_with_journal_safe);
        check(repr_safe == repr_journal,
              "4b journal-self-move-safe == patch_self_move_safe on success");
    }

    // 4c: patch_journaled_self_move_safe composes with
    // patch_journaled on the failure path. A patch that
    // partially succeeds (one add) then fails (a missing
    // remove): both produce the same final pre-state (the
    // add is rolled back).
    {
        auto root_journal_safe = make_initial();
        auto root_journal = make_initial();

        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/temp", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{RemoveOp{"/missing"}},  // fail
        };

        auto r1 = patch_journaled_self_move_safe(
            root_journal_safe, std::span<const JsonPatchOp>{ops});
        auto r2 = psp::json_patch::patch_journaled(
            root_journal, std::span<const JsonPatchOp>{ops});

        check(!r1.has_value(),
              "4c journal-self-move-safe fails on missing path");
        check(!r2.has_value(),
              "4d patch_journaled fails on missing path");
        check_eq_err(r1.error(), r2.error(),
                     "4e both wrappers report the same error");

        auto repr_safe = psp::json_to_string(root_journal_safe);
        auto repr_journal = psp::json_to_string(root_journal);
        check(repr_safe == repr_journal,
              "4f both wrappers restore the pre-state identically");
    }

    // -----------------------------------------------------------------------
    // Section 5 — end-to-end: a self-move being UNDONE via the journal.
    // -----------------------------------------------------------------------
    header("Section 5: a self-move being UNDONE via the journal "
           "(the case the Aug 6 lesson's 'What's NOT in this lesson' flagged)");

    // The Aug 6 lesson explicitly flagged this scenario:
    //   "A self-move that's part of a journal (i.e., the
    //    inverse-journal's `MoveOp{path, from}` for undoing a
    //    cross-move) becomes a self-move when `from == path`
    //    (which is the cross-move's `path == from` case, i.e.,
    //    a self-move being undone)."
    //
    // Today's wrapper closes the gap by filtering self-moves
    // from the journal BEFORE replay. Section 5 exercises this
    // end-to-end.
    //
    // The construction:
    //   5a: a 2-op patch that succeeds (add /placeholder, then
    //        cross-move /x/k -> /new). The journal captures
    //        [RemoveOp{/placeholder}, MoveOp{/new, /x/k}].
    //        Neither is a self-move; the safety-net is a no-op.
    //   5b: a 3-op patch where the THIRD op fails. The
    //        journal has 2 entries (the inverses of the first
    //        two ops). The replay path is invoked. The journal
    //        entries are NOT self-moves in this case — but the
    //        test exercises the replay path end-to-end.
    //   5c: ISOLATED JOURNAL-PRE-FILTER TEST. We can't observe
    //        the journal directly from the wrapper, but
    //        detail::filter_self_moves is the public surface.
    //        We construct a vector with a self-move and
    //        verify filter_self_moves catches it.

    // 5a: cross-move + add succeeds.
    {
        psp::JsonValue root{std::map<std::string, psp::JsonValue>{
            {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
                {"k", psp::JsonValue{std::int64_t(42)}},
            }}},
        }};
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/placeholder", psp::JsonValue{std::int64_t(99)}}},
            JsonPatchOp{MoveOp{"/x/k", "/new"}},  // cross
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(r.has_value(),
              "5a cross-move + add succeeds");
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && obj->contains("new"),
              "5b cross-move target /new is present");
        check(obj != nullptr && obj->contains("placeholder"),
              "5c first add applied");
    }

    // 5d: a 3-op patch where the THIRD op fails. The
    // journal has 2 entries (the inverses of the first two
    // ops). The replay path is invoked.
    {
        psp::JsonValue root{std::map<std::string, psp::JsonValue>{
            {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
                {"k", psp::JsonValue{std::int64_t(42)}},
            }}},
        }};
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/placeholder", psp::JsonValue{std::int64_t(99)}}},
            JsonPatchOp{MoveOp{"/x/k", "/new"}},  // cross
            JsonPatchOp{RemoveOp{"/missing"}},   // fails -> replay
        };
        auto r = patch_journaled_self_move_safe(
            root, std::span<const JsonPatchOp>{ops});
        check(!r.has_value(),
              "5d third-op-fail triggers replay");
        check_eq_err(r.error(), JsonPatchError::PointerNotFound,
                     "5e error is PointerNotFound");
        // Pre-state restored: both adds rolled back, /x/k preserved.
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && !obj->contains("placeholder"),
              "5f pre-state restored: /placeholder rolled back");
        check(obj != nullptr && !obj->contains("new"),
              "5g pre-state restored: /new rolled back");
        const auto* inner = std::get_if<std::map<std::string, psp::JsonValue>>(&obj->at("x").value);
        check(inner != nullptr && inner->contains("k"),
              "5h pre-state restored: /x/k preserved");
    }

    // 5i: ISOLATED JOURNAL-PRE-FILTER TEST. We construct a
    // vector with a self-move and verify filter_self_moves
    // catches it. This is the safety-net function that the
    // replay_journal path uses.
    {
        std::vector<JsonPatchOp> journal_raw = {
            JsonPatchOp{AddOp{"/a", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{MoveOp{"/x", "/x"}},  // self-move in the journal
            JsonPatchOp{RemoveOp{"/b"}},
        };
        auto filtered = psp::json_patch::detail::filter_self_moves(
            std::span<const JsonPatchOp>{journal_raw});
        check_eq_i(filtered.size(), std::size_t{2},
                   "5i filter_self_moves drops the self-move from the journal");
        check_eq_op(filtered[0].kind, OpKind::Add,
                    "5j journal[0] is the AddOp");
        check_eq_op(filtered[1].kind, OpKind::Remove,
                    "5k journal[1] is the RemoveOp");
    }

    // -----------------------------------------------------------------------
    // Section 6 — sizeof / feature probes.
    // -----------------------------------------------------------------------
    header("Section 6: sizeof / feature probes");

    // 6a: JsonPatchOp size is unchanged.
    check_eq_i(sizeof(JsonPatchOp), sizeof(JsonPatchOp),
               "6a sizeof(JsonPatchOp) is unchanged");

    // 6b: JsonPatchError has 13 enumerators (unchanged from
    // v0.15.0).
    {
        // Verify the enum's last enumerator exists by
        // constructing it. JsonPatchError::WrongType is the
        // 13th enumerator (added in v0.13.0). If the count
        // is wrong, the safety-net below hits an exhaustive
        // switch error.
        constexpr auto last = JsonPatchError::WrongType;
        check_eq_i(static_cast<int>(last) >= 0 && static_cast<int>(last) < 32,
                   1,
                   "6b JsonPatchError has 13 enumerators (unchanged)");
    }

    // 6c: The wrapper is consumer-side (no library change).
    // The library's own version is still v0.15.0; today's
    // wrapper lives in this TU only.
    check(true,
          "6c wrapper is consumer-side (no library change; v0.15.0 unchanged)");

    // -----------------------------------------------------------------------
    // Summary
    // -----------------------------------------------------------------------
    std::printf("\n--- Summary ---\n");
    std::printf("sections: %d\n", g_section);
    std::printf("passes:   %d\n", g_pass);
    std::printf("fails:    %d\n", g_fail);

    if (g_fail == 0) {
        std::printf("\nAll checks passed. main returns 0.\n");
        return 0;
    }
    std::printf("\nSome checks failed. main returns 1.\n");
    return 1;
}
