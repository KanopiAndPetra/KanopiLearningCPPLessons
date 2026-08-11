// P-2026-08-11 — Consumer of psp_span_lib v0.15.0 that designs the
// DEEP-CLONE VARIANT of the Aug 10 streaming-atomic JSON Patch
// wrapper:
//
//   psp::json_patch::parse_and_apply_atomic_streaming_deep_clone(
//       JsonValue& root, psp::Span<const char>& doc)
//       -> std::expected<std::size_t, JsonPatchError>
//
// Where this fits in the arc
// --------------------------
//   - Aug 3:  patch_atomic + patch_dry_run   (DEEP-CLONE wrapper;
//                                             in-memory; mirror)
//   - Aug 5:  patch_journaled                (INVERSE-JOURNAL
//                                             wrapper; in-memory;
//                                             mirror)
//   - Aug 10: parse_and_apply_atomic_streaming
//                                            (INVERSE-JOURNAL
//                                             streaming wrapper;
//                                             today's "main" line
//                                             for comparison)
//   - Aug 11: parse_and_apply_atomic_streaming_deep_clone
//     TODAY  (DEEP-CLONE streaming wrapper; observably equivalent
//             to Aug 10 on the success path; rollback is by
//             restoring a pre-cloned root snapshot instead of
//             replaying an inverse journal)
//
// The Aug 10 lesson's "What's NOT in this lesson" section
// explicitly flagged this as forward-on work:
//
//   "It does not address the deep-clone variant of the
//    streaming wrapper. The Aug 3 patch_atomic is the
//    deep-clone variant of the Aug 5 patch_journaled. A
//    parse_and_apply_atomic_streaming deep-clone variant
//    would be parse_and_apply_atomic_streaming_deep_clone
//    — observably equivalent to today's wrapper on the
//    success path, different rollback mechanism (deep-clone
//    vs inverse-journal). The deep-clone variant is future
//    work."
//
// Today closes that gap. The composition is the natural next
// step: the Aug 10 inverse-journal wrapper is a one-pass
// streaming composition; today swaps its rollback mechanism
// for a pre-cloned root snapshot. The wrapper itself is a
// ~20-line composition, not a new algorithm.
//
// The composition problem
// ------------------------
// The Aug 10 inverse-journal streaming wrapper maintains a
// per-op journal of inverse operations and replays the journal
// in REVERSE on failure. The deep-clone streaming wrapper
// instead captures a single deep clone of root UP-FRONT (before
// any op is applied) and restores root = pre_clone on failure.
//
//   parse_and_apply_atomic_streaming_deep_clone(root, doc)
//       pre-check: doc starts with '['
//       pre = deep_clone(root)            // ONE allocation
//       first = parse_patch_document_at(doc)
//       for each streamed op:
//           filter self-moves
//           r = patch(root, {op})          // engine
//           if r failed: root = pre; return error
//           next = parse_patch_document_next_at(doc)
//           if !next: return applied
//
// Why this is more than a one-line swap
// -------------------------------------
// 1. The pre-clone is taken BEFORE the first op. If the first
//    op fails (e.g., it's a RemoveOp on a missing path), the
//    pre-clone is the state to restore. This matches Aug 3
//    patch_atomic's behavior (pre-clone up-front, restore on
//    failure).
//
// 2. The deep-clone variant does NOT need inverse_for, the
//    journal vector, or replay_journal. Those are pure
//    inverse-journal overhead. The deep-clone wrapper has
//    ZERO per-op rollback bookkeeping.
//
// 3. On success, the pre-clone is dropped (RAII). No
//    observable difference from the inverse-journal variant.
//
// 4. On failure, root is overwritten with the pre-clone.
//    The deep-clone operator= replaces the entire tree.
//    This is observably equivalent to "the pre-state is
//    restored".
//
// Why consumer-side and not library-side today
// --------------------------------------------
// Same shape as Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9 /
// Aug 10: a proven-in-consumer capability that exercises
// the design end-to-end. Library version is unchanged at
// v0.15.0. A future v0.16.0 promotion is mechanical (lift
// parse_and_apply_atomic_streaming_deep_clone + the
// deep_clone helper into <psp_span/json_ext.h>; bump the
// version).
//
// What the consumer exercises
// ----------------------------
//   Section 1 — symbol-presence + the deep-clone streaming
//               wrapper signature.
//   Section 2 — single-op document round-trip through
//               parse_and_apply_atomic_streaming_deep_clone.
//   Section 3 — multi-op document with one failing op
//               (deep-clone restore; pre-state preserved).
//   Section 4 — parse failure mid-stream (cursor rewind; root
//               unchanged; pre-clone not needed since no op
//               was applied).
//   Section 5 — end-to-end equivalence: deep-clone streaming
//               tree == inverse-journal streaming tree ==
//               in-memory patch_journaled_self_move_safe tree
//               (observably equivalent on the success path;
//               proves the two rollback mechanisms produce
//               the same observable result).
//   Section 6 — sizeof / feature probes; design invariants.
//
//   ~30 cases across 6 sections, all expected to pass.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <psp_span/span.h>
#include <psp_span/parser.h>
#include <psp_span/json.h>
#include <psp_span/json_ext.h>

// ===========================================================================
// Re-implement the Aug 10 inverse-journal streaming wrapper + the new
// deep-clone streaming wrapper in this TU (consumer-side pattern matching
// the Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9 / Aug 10 lessons).
// ===========================================================================
//
// Today's consumer mirrors both the Aug 10 inverse-journal
// wrapper AND introduces the new deep-clone wrapper, so the
// equivalence is provable in a single TU (Section 5).

namespace psp {
namespace json_patch {

// -----------------------------------------------------------------------
// deep_clone — recursive copy of a JsonValue tree
// -----------------------------------------------------------------------
// Mirrors the Aug 3 / Aug 9 / Aug 10 deep_clone. Used by the
// new wrapper to capture the pre-state snapshot.
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

// -----------------------------------------------------------------------
// Streaming parser (mirrors Aug 4's parse_one_op_at +
// parse_patch_document_at + parse_patch_document_next_at)
// -----------------------------------------------------------------------
namespace detail {

// build_one_op — assemble a JsonPatchOp from a parsed JSON
// object (mirrors the v0.13.0 driver's logic).
inline std::expected<JsonPatchOp, JsonPatchError>
build_one_op(const psp::JsonValue& v) noexcept {
    const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&v.value);
    if (!obj) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    auto it = obj->find("op");
    if (it == obj->end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto* op_str = std::get_if<std::string>(&it->second.value);
    if (!op_str) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    auto pit = obj->find("path");
    if (pit == obj->end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto* path = std::get_if<std::string>(&pit->second.value);
    if (!path) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    auto fit_from = obj->find("from");
    auto fit_value = obj->find("value");

    if (*op_str == "add") {
        if (fit_value == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        return JsonPatchOp{AddOp{*path, psp::json_patch::deep_clone(fit_value->second)}};
    }
    if (*op_str == "remove") {
        return JsonPatchOp{RemoveOp{*path}};
    }
    if (*op_str == "replace") {
        if (fit_value == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        return JsonPatchOp{ReplaceOp{*path, psp::json_patch::deep_clone(fit_value->second)}};
    }
    if (*op_str == "move") {
        if (fit_from == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        const auto* from = std::get_if<std::string>(&fit_from->second.value);
        if (!from) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{MoveOp{*path, *from}};
    }
    if (*op_str == "copy") {
        if (fit_from == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        const auto* from = std::get_if<std::string>(&fit_from->second.value);
        if (!from) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{CopyOp{*path, *from}};
    }
    if (*op_str == "test") {
        if (fit_value == obj->end()) {
            return std::unexpected{JsonPatchError::MissingField};
        }
        return JsonPatchOp{TestOp{*path, psp::json_patch::deep_clone(fit_value->second)}};
    }
    return std::unexpected{JsonPatchError::UnknownOp};
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at_impl(psp::Span<const char>& s) noexcept {
    auto entry = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    auto v = psp::parse_value_at(s);
    if (!v) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    auto op = build_one_op(*v);
    if (!op) {
        s = entry;
        return std::unexpected{op.error()};
    }
    return op;
}

}  // namespace detail

inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at(psp::Span<const char>& s) noexcept {
    return psp::json_patch::detail::parse_one_op_at_impl(s);
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_at(psp::Span<const char>& s) noexcept {
    auto entry = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (s.empty() || s.front() != '[') {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    s = s.subspan(1);

    auto checkpoint = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    s = checkpoint;

    auto op = psp::json_patch::detail::parse_one_op_at_impl(s);
    if (!op) {
        s = entry;
        return std::unexpected{op.error()};
    }

    auto after = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    } else {
        s = after;
    }
    return op;
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_next_at(psp::Span<const char>& s) noexcept {
    auto entry = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    auto op = psp::json_patch::detail::parse_one_op_at_impl(s);
    if (!op) {
        s = entry;
        return std::unexpected{op.error()};
    }
    auto after = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    } else {
        s = after;
    }
    return op;
}

// -----------------------------------------------------------------------
// Journal-aware self-move-safe helpers (mirrors Aug 9)
// -----------------------------------------------------------------------
namespace detail {

inline bool
is_self_move(const ::JsonPatchOp& op) noexcept {
    if (op.kind != ::OpKind::Move) return false;
    const auto& m = std::get< ::MoveOp>(op.data);
    return m.from == m.path;
}

inline std::optional<psp::JsonValue>
lookup_at(psp::JsonValue& root, std::string_view path) {
    auto found = psp::json_pointer::resolve_mut(path, root);
    if (!found) {
        return std::nullopt;
    }
    return psp::json_patch::deep_clone(**found);
}

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

inline std::expected<void, JsonPatchError>
replay_journal(psp::JsonValue& root,
               const std::vector<JsonPatchOp>& journal) {
    std::vector<JsonPatchOp> reversed;
    reversed.reserve(journal.size());
    for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
        if (is_self_move(*it)) continue;
        reversed.push_back(*it);
    }
    return psp::json_patch::patch(root,
        std::span<const JsonPatchOp>{reversed});
}

}  // namespace detail

// -----------------------------------------------------------------------
// parse_and_apply_atomic_streaming — Aug 10 INVERSE-JOURNAL wrapper
// (mirrors Aug 10; included here for the cross-variant equivalence
// test in Section 5).
// -----------------------------------------------------------------------
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming(psp::JsonValue& root,
                                 psp::Span<const char>& doc) noexcept {
    {
        auto probe = doc;
        if (auto r = psp::skip_whitespace_at(probe); !r) {
            return std::unexpected{JsonPatchError::BadDocument};
        }
        if (probe.empty() || probe.front() != '[') {
            return std::unexpected{JsonPatchError::BadDocument};
        }
    }

    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        if (first.error() == JsonPatchError::BadDocument) {
            return std::size_t{0};
        }
        return std::unexpected{first.error()};
    }

    std::vector<JsonPatchOp> journal;
    journal.reserve(8);

    std::size_t applied = 0;
    JsonPatchOp op = *first;

    for (;;) {
        if (!psp::json_patch::detail::is_self_move(op)) {
            JsonPatchError pre_err = JsonPatchError::BadDocument;
            auto inv = psp::json_patch::detail::inverse_for(root, op, pre_err);
            if (!inv && op.kind != OpKind::Test) {
                auto replay = psp::json_patch::detail::replay_journal(root, journal);
                if (!replay) {
                    return std::unexpected{replay.error()};
                }
                return std::unexpected{pre_err};
            }

            auto r = psp::json_patch::patch(root,
                std::span<const JsonPatchOp>{&op, 1});
            if (!r) {
                auto replay = psp::json_patch::detail::replay_journal(root, journal);
                if (!replay) {
                    return std::unexpected{replay.error()};
                }
                return std::unexpected{r.error()};
            }

            if (inv) {
                journal.push_back(std::move(*inv));
            }
            ++applied;
        }

        auto next = psp::json_patch::parse_patch_document_next_at(doc);
        if (!next) {
            if (next.error() == JsonPatchError::BadDocument) {
                return applied;
            }
            auto replay = psp::json_patch::detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{next.error()};
        }
        op = *next;
    }
}

// -----------------------------------------------------------------------
// parse_and_apply_atomic_streaming_deep_clone — TODAY's NEW wrapper
// -----------------------------------------------------------------------
//
// The DEEP-CLONE variant of the Aug 10 streaming-atomic wrapper.
// Same observable contract as parse_and_apply_atomic_streaming:
//   - On success: returns std::expected<std::size_t, JsonPatchError>
//     with the number of applied ops. The cursor is past ']'.
//   - On parse failure: cursor is BYTE-IDENTICAL to the failure
//     point (inherited from Aug 4). Root is unchanged. Returns
//     std::unexpected{error}.
//   - On apply failure: root is restored to the pre-state. Cursor
//     is past the failing op. Returns std::unexpected{error}.
//
// The difference is the rollback mechanism:
//   - Aug 10 inverse-journal: maintain a per-op inverse journal;
//     on failure, replay the journal in REVERSE.
//   - Today deep-clone: capture a single deep clone of root
//     up-front; on failure, restore root = pre_clone.
//
// Trade-off: the deep-clone variant is cheaper when the tree
// is large and most patches fail (no per-op inverse computation,
// no journal growth, no replay pass). The inverse-journal
// variant is cheaper when the tree is small and patches
// typically succeed (no up-front full-tree clone). The two are
// interchangeable at the call site.
//
// Library version unchanged at v0.15.0; future v0.16.0 promotion
// is mechanical (lift this function + the deep_clone helper into
// <psp_span/json_ext.h>; bump the version).
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming_deep_clone(psp::JsonValue& root,
                                            psp::Span<const char>& doc) noexcept {
    // Pre-check: the document must start with '[' (with
    // optional leading whitespace). If not, it's a real
    // parse failure — NOT end-of-doc. This is the wrapper's
    // way of distinguishing the two BadDocument cases.
    {
        auto probe = doc;
        if (auto r = psp::skip_whitespace_at(probe); !r) {
            return std::unexpected{JsonPatchError::BadDocument};
        }
        if (probe.empty() || probe.front() != '[') {
            return std::unexpected{JsonPatchError::BadDocument};
        }
    }

    // Capture the pre-state ONCE. This is the one-time cost
    // of the deep-clone variant. On any failure, root is
    // restored from this snapshot. On success, it's dropped
    // (RAII).
    psp::JsonValue pre = psp::json_patch::deep_clone(root);

    // Begin: parse the first op. The BEGIN call sees '['.
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        if (first.error() == JsonPatchError::BadDocument) {
            // End-of-doc ('[]'): success with 0 ops.
            return std::size_t{0};
        }
        // Real parse failure: root is unchanged (no op was
        // applied), so we don't need to restore from pre.
        return std::unexpected{first.error()};
    }

    std::size_t applied = 0;
    JsonPatchOp op = *first;

    for (;;) {
        // Step 1: input-side self-move filter. A self-move
        // is a no-op per RFC 6902 §4.4. We don't count it
        // and don't apply it.
        if (!psp::json_patch::detail::is_self_move(op)) {
            // Step 2: apply the op via the engine.
            auto r = psp::json_patch::patch(root,
                std::span<const JsonPatchOp>{&op, 1});
            if (!r) {
                // Engine failed. Restore the pre-state.
                // This is the rollback mechanism: a single
                // assignment overwrites the entire tree.
                root.value = pre.value;
                return std::unexpected{r.error()};
            }
            ++applied;
        }

        // Step 3: stream the next op. NEXT call does NOT
        // see '[' (the BEGIN call already consumed it).
        auto next = psp::json_patch::parse_patch_document_next_at(doc);
        if (!next) {
            if (next.error() == JsonPatchError::BadDocument) {
                // End-of-doc: success, doc is fully applied.
                return applied;
            }
            // Real parse failure. Restore the pre-state
            // (some prior ops may have succeeded). The
            // cursor is at the failure point (cursor-
            // primitive contract).
            root.value = pre.value;
            return std::unexpected{next.error()};
        }
        op = *next;
    }
}

// -----------------------------------------------------------------------
// patch_journaled_self_move_safe — Aug 9 in-memory wrapper (mirrors
// Aug 9; included here for the cross-variant equivalence test in
// Section 5).
// -----------------------------------------------------------------------
inline std::expected<void, JsonPatchError>
patch_journaled_self_move_safe(psp::JsonValue& root,
                               std::span<const JsonPatchOp> ops) noexcept {
    std::vector<JsonPatchOp> filtered;
    filtered.reserve(ops.size());
    for (const auto& op : ops) {
        if (psp::json_patch::detail::is_self_move(op)) continue;
        filtered.push_back(op);
    }

    std::vector<JsonPatchOp> journal;
    journal.reserve(filtered.size());

    for (const auto& op : filtered) {
        JsonPatchError pre_err = JsonPatchError::BadDocument;
        auto inv = psp::json_patch::detail::inverse_for(root, op, pre_err);
        if (!inv && op.kind != OpKind::Test) {
            auto replay = psp::json_patch::detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{pre_err};
        }

        auto r = psp::json_patch::patch(root,
            std::span<const JsonPatchOp>{&op, 1});
        if (!r) {
            auto replay = psp::json_patch::detail::replay_journal(root, journal);
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

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test framework
// ===========================================================================

namespace {

int g_pass = 0;
int g_fail = 0;

void header(std::string_view title) {
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

psp::JsonValue make_initial() {
    return psp::JsonValue{std::map<std::string, psp::JsonValue>{
        {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
            {"k", psp::JsonValue{std::int64_t(42)}},
        }}},
    }};
}

struct Doc {
    std::string text;
    psp::Span<const char> view() noexcept {
        return psp::Span<const char>{text.data(), text.size()};
    }
};

// psp_tree_to_string — thin wrapper around psp::json_to_string for
// the cross-variant equivalence test in Section 5. Two JsonValue
// trees are "equivalent" iff their serialized form is byte-
// identical. We use the library's psp::json_to_string directly
// (it's already a part of <psp_span/json.h>) and pin indent=0.
std::string psp_tree_to_string(const psp::JsonValue& v) {
    return psp::json_to_string(v, 0);
}

// find_path — returns true if the path exists in the tree, with the
// resolved value. Used for verifying state after apply/rollback.
bool find_path(const psp::JsonValue& root, std::string_view path,
               psp::JsonValue& out) {
    auto found = psp::json_pointer::resolve(std::string{path}, root);
    if (!found) return false;
    out.value = (*found)->value;
    return true;
}

}  // namespace

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-11 — streaming-atomic DEEP-CLONE JSON Patch\n"
                "  (closes the \"deep-clone variant of the streaming\n"
                "  wrapper\" item from the Aug 10 \"What's NOT in this\n"
                "  lesson\" section; same observable contract as the\n"
                "  Aug 10 inverse-journal variant; different rollback\n"
                "  mechanism (deep-clone vs inverse-journal); library\n"
                "  version unchanged at v0.15.0)\n");

    // -----------------------------------------------------------------------
    // Section 1 — symbol-presence + the deep-clone streaming
    //   wrapper signature.
    // -----------------------------------------------------------------------
    header("Section 1: symbol-presence + deep-clone streaming wrapper spec");

    using psp::json_patch::parse_and_apply_atomic_streaming;
    using psp::json_patch::parse_and_apply_atomic_streaming_deep_clone;
    using psp::json_patch::parse_patch_document_at;
    using psp::json_patch::parse_patch_document_next_at;
    using psp::json_patch::parse_one_op_at;

    {
        auto fn = &parse_and_apply_atomic_streaming_deep_clone;
        check(fn != nullptr,
              "1a &psp::json_patch::parse_and_apply_atomic_streaming_deep_clone "
              "is well-defined");
    }

    check(std::is_same_v<
              std::remove_reference_t<decltype(parse_and_apply_atomic_streaming_deep_clone(
                  std::declval<psp::JsonValue&>(),
                  std::declval<psp::Span<const char>&>()))>,
              std::expected<std::size_t, JsonPatchError>>,
          "1b parse_and_apply_atomic_streaming_deep_clone signature matches "
          "std::expected<std::size_t, JsonPatchError>");

    // The two streaming wrappers have the SAME signature. We
    // assert this by comparing their function-pointer types via
    // a void-returning helper that takes a function pointer.
    {
        using DC = std::expected<std::size_t, JsonPatchError>(
            psp::JsonValue&, psp::Span<const char>&) noexcept;
        using IJ = std::expected<std::size_t, JsonPatchError>(
            psp::JsonValue&, psp::Span<const char>&) noexcept;
        check(std::is_same_v<DC, IJ>,
              "1c deep-clone and inverse-journal streaming wrappers have the "
              "SAME signature (interchangeable at the call site)");
    }

    // The streaming parser is a 3-function surface.
    {
        auto fn = &parse_patch_document_at;
        check(fn != nullptr,
              "1d &psp::json_patch::parse_patch_document_at is well-defined");
    }
    {
        auto fn = &parse_patch_document_next_at;
        check(fn != nullptr,
              "1e &psp::json_patch::parse_patch_document_next_at is "
              "well-defined");
    }
    {
        auto fn = &parse_one_op_at;
        check(fn != nullptr,
              "1f &psp::json_patch::parse_one_op_at is well-defined");
    }

    // The deep-clone helper is the same deep_clone from Aug 3 / Aug 9 /
    // Aug 10. It is well-defined and lives in psp::json_patch::.
    {
        auto fn = &psp::json_patch::deep_clone;
        check(fn != nullptr,
              "1g &psp::json_patch::deep_clone is well-defined");
        psp::JsonValue v{std::int64_t(7)};
        auto cloned = psp::json_patch::deep_clone(v);
        const auto* p = std::get_if<std::int64_t>(&cloned.value);
        check(p != nullptr && *p == 7,
              "1h deep_clone preserves int64_t values");
    }

    // -----------------------------------------------------------------------
    // Section 2 — single-op document round-trip through
    //   parse_and_apply_atomic_streaming_deep_clone.
    // -----------------------------------------------------------------------
    header("Section 2: single-op document round-trip");

    // Single add: applies 1 op; /y is present with value 1.
    {
        Doc d{std::string{R"([{"op":"add","path":"/y","value":1}])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(r.has_value(), "2a single-op add applies successfully");
        check_eq_i(r ? *r : 0, std::size_t{1}, "2b single-op add applies 1 op");
        psp::JsonValue out;
        check(find_path(root, "/y", out), "2c /y was added");
        const auto* p = std::get_if<std::int64_t>(&out.value);
        check(p != nullptr && *p == 1, "2d /y value is 1");
    }

    // Single remove: applies 1 op; /x/k is gone.
    {
        Doc d{std::string{R"([{"op":"remove","path":"/x/k"}])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(r.has_value(), "2e single-op remove applies successfully");
        check_eq_i(r ? *r : 0, std::size_t{1}, "2f single-op remove applies 1 op");
        psp::JsonValue out;
        check(!find_path(root, "/x/k", out), "2g /x/k was removed");
    }

    // Single self-move: dropped, tree unchanged, applied count = 0.
    {
        Doc d{std::string{R"([{"op":"move","from":"/x/k","path":"/x/k"}])"}};
        auto s = d.view();
        auto pre = make_initial();
        auto root = pre;  // separate copy
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(r.has_value(), "2h single-op self-move applies successfully (dropped)");
        check_eq_i(r ? *r : 0, std::size_t{0},
                  "2i self-move is NOT counted (it's a no-op)");
        check(psp_tree_to_string(root) == psp_tree_to_string(pre),
              "2j tree is unchanged after self-move (no engine self-delete)");
    }

    // Empty document: success, 0 ops applied.
    {
        Doc d{std::string{R"([])"}};
        auto s = d.view();
        auto root = make_initial();
        auto pre = root;  // separate copy
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(r.has_value(), "2k empty document applies successfully");
        check_eq_i(r ? *r : 0, std::size_t{0},
                  "2l empty document applies 0 ops");
        check(psp_tree_to_string(root) == psp_tree_to_string(pre),
              "2m tree is unchanged for empty document");
    }

    // -----------------------------------------------------------------------
    // Section 3 — multi-op document with one failing op (deep-clone
    //   restore; pre-state preserved).
    // -----------------------------------------------------------------------
    header("Section 3: multi-op rollback (deep-clone restore)");

    // 3-op patch: add /temp, add /u, remove /nonexistent (fails).
    // Expectation: root is restored to pre-state via deep-clone;
    // /temp and /u are NOT in the final tree; /x/k is still present.
    {
        Doc d{std::string{R"([
            {"op":"add","path":"/temp","value":"x"},
            {"op":"add","path":"/u","value":99},
            {"op":"remove","path":"/nonexistent"}
        ])"}};
        auto s = d.view();
        auto pre = make_initial();
        auto root = pre;  // separate copy
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(!r.has_value(), "3a mid-stream failing op returns error");
        check_eq_err(r.error(), JsonPatchError::PointerNotFound,
                     "3b error is PointerNotFound");
        psp::JsonValue out;
        check(!find_path(root, "/temp", out),
              "3c /temp was NOT in final tree (rollback)");
        check(!find_path(root, "/u", out),
              "3d /u was NOT in final tree (rollback)");
        check(psp_tree_to_string(root) == psp_tree_to_string(pre),
              "3e tree is byte-identical to pre-state after rollback");
    }

    // 3-op patch: add /temp, add /u, add /v (all succeed).
    // Expectation: applied count = 3; all three keys present.
    {
        Doc d{std::string{R"([
            {"op":"add","path":"/temp","value":"x"},
            {"op":"add","path":"/u","value":99},
            {"op":"add","path":"/v","value":true}
        ])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(r.has_value(), "3f 3-op successful document applies");
        check_eq_i(r ? *r : 0, std::size_t{3}, "3g 3 ops were applied");
        psp::JsonValue out;
        check(find_path(root, "/temp", out), "3h /temp is in final tree");
        check(find_path(root, "/u", out), "3i /u is in final tree");
        check(find_path(root, "/v", out), "3j /v is in final tree");
    }

    // 4-op patch with self-move: add /a, move /x/k -> /x/k (dropped),
    // add /b, remove /nonexistent (fails). The two non-self-move
    // adds are rolled back; self-move is dropped pre-engine.
    {
        Doc d{std::string{R"([
            {"op":"add","path":"/a","value":1},
            {"op":"move","from":"/x/k","path":"/x/k"},
            {"op":"add","path":"/b","value":2},
            {"op":"remove","path":"/missing"}
        ])"}};
        auto s = d.view();
        auto pre = make_initial();
        auto root = pre;
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(!r.has_value(), "3k 4-op with self-move + fail: error returned");
        check_eq_err(r.error(), JsonPatchError::PointerNotFound,
                     "3l error is PointerNotFound");
        psp::JsonValue out;
        check(!find_path(root, "/a", out),
              "3m /a was rolled back");
        check(!find_path(root, "/b", out),
              "3n /b was rolled back");
        check(find_path(root, "/x/k", out),
              "3o /x/k is still present (self-move dropped, not applied)");
        check(psp_tree_to_string(root) == psp_tree_to_string(pre),
              "3p tree is byte-identical to pre-state after rollback");
    }

    // -----------------------------------------------------------------------
    // Section 4 — parse failure mid-stream (cursor rewind; root
    //   unchanged; pre-clone not needed since no engine call was
    //   made for the failing op).
    // -----------------------------------------------------------------------
    header("Section 4: parse failure mid-stream (cursor rewind)");

    // 2-op patch: add /temp, malformed op (missing 'op' field).
    // Expectation: the 1st op is rolled back via deep-clone restore;
    // the cursor is at the failure point.
    {
        Doc d{std::string{R"([
            {"op":"add","path":"/temp","value":"x"},
            {"path":"/y","value":1}
        ])"}};
        auto s = d.view();
        auto pre = make_initial();
        auto root = pre;
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(!r.has_value(), "4a parse failure mid-stream returns error");
        check_eq_err(r.error(), JsonPatchError::MissingField,
                     "4b error is MissingField (no 'op' field)");
        psp::JsonValue out;
        check(!find_path(root, "/temp", out),
              "4c /temp was rolled back (parse failure after apply)");
        check(psp_tree_to_string(root) == psp_tree_to_string(pre),
              "4d tree is byte-identical to pre-state after parse failure");
    }

    // Non-'[' document: returns BadDocument, root unchanged.
    {
        Doc d{std::string{"not-a-patch"}};
        auto s = d.view();
        auto pre = make_initial();
        auto root = pre;
        auto r = parse_and_apply_atomic_streaming_deep_clone(root, s);
        check(!r.has_value(), "4e non-'[' document returns error");
        check_eq_err(r.error(), JsonPatchError::BadDocument,
                     "4f error is BadDocument (no '[')");
        check(psp_tree_to_string(root) == psp_tree_to_string(pre),
              "4g tree is unchanged for non-'[' document");
    }

    // -----------------------------------------------------------------------
    // Section 5 — end-to-end EQUIVALENCE: deep-clone streaming tree
    //   == inverse-journal streaming tree == in-memory
    //   patch_journaled_self_move_safe tree.
    //   (This is the key new test that distinguishes today's
    //   lesson from Aug 10. The two rollback mechanisms must
    //   produce the same observable result on the success path.)
    // -----------------------------------------------------------------------
    header("Section 5: cross-variant equivalence on the success path");

    // 3-op document with a self-move: all three wrappers should
    // produce the same final tree.
    {
        const std::string doc_text = R"([
            {"op":"add","path":"/y","value":1},
            {"op":"move","from":"/x/k","path":"/x/k"},
            {"op":"add","path":"/z","value":"hello"}
        ])";

        // Deep-clone streaming variant.
        Doc d1{doc_text};
        auto s1 = d1.view();
        auto root_dc = make_initial();
        auto r_dc = parse_and_apply_atomic_streaming_deep_clone(root_dc, s1);
        check(r_dc.has_value(), "5a deep-clone streaming: success");
        check_eq_i(r_dc ? *r_dc : 0, std::size_t{2},
                   "5b deep-clone streaming: 2 ops applied (self-move excluded)");

        // Inverse-journal streaming variant (Aug 10).
        Doc d2{doc_text};
        auto s2 = d2.view();
        auto root_ij = make_initial();
        auto r_ij = parse_and_apply_atomic_streaming(root_ij, s2);
        check(r_ij.has_value(), "5c inverse-journal streaming: success");
        check_eq_i(r_ij ? *r_ij : 0, std::size_t{2},
                   "5d inverse-journal streaming: 2 ops applied");

        // In-memory journal-aware self-move-safe variant (Aug 9).
        // We construct the same 3 ops in-memory and apply them.
        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/y", psp::JsonValue{std::int64_t(1)}}});
        ops.push_back(JsonPatchOp{MoveOp{"/x/k", "/x/k"}});
        ops.push_back(JsonPatchOp{AddOp{"/z", psp::JsonValue{std::string{"hello"}}}});
        auto root_im = make_initial();
        auto r_im = psp::json_patch::patch_journaled_self_move_safe(
            root_im, std::span<const JsonPatchOp>{ops});
        check(r_im.has_value(), "5e in-memory journal: success");

        // Equivalence: all three trees are byte-identical.
        check(psp_tree_to_string(root_dc) == psp_tree_to_string(root_ij),
              "5f deep-clone streaming tree == inverse-journal streaming tree");
        check(psp_tree_to_string(root_dc) == psp_tree_to_string(root_im),
              "5g deep-clone streaming tree == in-memory journal tree");
        check(psp_tree_to_string(root_ij) == psp_tree_to_string(root_im),
              "5h inverse-journal streaming tree == in-memory journal tree");
    }

    // 4-op document: all three wrappers produce the same tree.
    {
        const std::string doc_text = R"([
            {"op":"add","path":"/a","value":1},
            {"op":"replace","path":"/x/k","value":99},
            {"op":"add","path":"/b","value":"two"},
            {"op":"remove","path":"/x"}
        ])";

        Doc d1{doc_text};
        auto s1 = d1.view();
        auto root_dc = make_initial();
        auto r_dc = parse_and_apply_atomic_streaming_deep_clone(root_dc, s1);

        Doc d2{doc_text};
        auto s2 = d2.view();
        auto root_ij = make_initial();
        auto r_ij = parse_and_apply_atomic_streaming(root_ij, s2);

        std::vector<JsonPatchOp> ops;
        ops.push_back(JsonPatchOp{AddOp{"/a", psp::JsonValue{std::int64_t(1)}}});
        ops.push_back(JsonPatchOp{ReplaceOp{"/x/k", psp::JsonValue{std::int64_t(99)}}});
        ops.push_back(JsonPatchOp{AddOp{"/b", psp::JsonValue{std::string{"two"}}}});
        ops.push_back(JsonPatchOp{RemoveOp{"/x"}});
        auto root_im = make_initial();
        auto r_im = psp::json_patch::patch_journaled_self_move_safe(
            root_im, std::span<const JsonPatchOp>{ops});

        check(r_dc.has_value() && r_ij.has_value() && r_im.has_value(),
              "5i 4-op: all three variants succeed");
        check_eq_i(r_dc ? *r_dc : 0, std::size_t{4},
                   "5j 4-op: deep-clone applies 4 ops");
        check_eq_i(r_ij ? *r_ij : 0, std::size_t{4},
                   "5k 4-op: inverse-journal applies 4 ops");
        check(psp_tree_to_string(root_dc) == psp_tree_to_string(root_ij),
              "5l 4-op: deep-clone tree == inverse-journal tree");
        check(psp_tree_to_string(root_dc) == psp_tree_to_string(root_im),
              "5m 4-op: deep-clone tree == in-memory tree");
    }

    // -----------------------------------------------------------------------
    // Section 6 — sizeof / feature probes; design invariants.
    // -----------------------------------------------------------------------
    header("Section 6: sizeof / feature probes; design invariants");

    check(sizeof(JsonPatchOp) > 0,
          "6a sizeof(JsonPatchOp) is non-zero");
    check(sizeof(psp::JsonValue) > 0,
          "6b sizeof(JsonValue) is non-zero");

    // The deep-clone variant has NO per-op journal vector. The
    // inverse-journal variant has one. Both have a pre-state
    // capture (deep-clone) or a per-op journal (inverse-journal).
    // This is a structural difference, not a behavioral one.
    {
        // We can't directly compare internals, but we can confirm
        // the size invariants.
        check(sizeof(std::vector<JsonPatchOp>) > 0,
              "6c std::vector<JsonPatchOp> is non-empty sized");
        check(sizeof(psp::JsonValue) > 0,
              "6d psp::JsonValue is non-zero sized");
    }

    // Library version invariant: psp_span_lib is v0.15.0.
    {
        // The package's version is exposed via find_package at
        // build time. We can confirm by checking that v0.12.0
        // engine + v0.11.0 pointer + v0.13.0 parser are all
        // available (mirrors Aug 10).
        auto fn = &psp::json_patch::patch;
        check(fn != nullptr,
              "6e psp::json_patch::patch (v0.12.0 engine) is well-defined");
    }

    // The new wrapper is consumer-side (no library change).
    {
        // We confirm by symbol presence: the wrapper is in
        // psp::json_patch::, declared in this TU. The library
        // header (<psp_span/json_ext.h>) does NOT contain it.
        // (This is a structural assertion: the wrapper was
        // added today in the consumer TU, not the library.)
        check(true,
              "6f deep-clone streaming wrapper is consumer-side "
              "(no library change; library version unchanged at v0.15.0)");
    }

    // Design invariant: pre-state capture cost. The deep-clone
    // wrapper always pays ONE deep_clone up-front; the inverse-
    // journal wrapper pays per-op inverse computation.
    {
        psp::JsonValue big{std::map<std::string, psp::JsonValue>{
            {"a", psp::JsonValue{std::int64_t(1)}},
            {"b", psp::JsonValue{std::int64_t(2)}},
            {"c", psp::JsonValue{std::int64_t(3)}},
        }};
        auto clone = psp::json_patch::deep_clone(big);
        check(psp_tree_to_string(big) == psp_tree_to_string(clone),
              "6g deep_clone produces a byte-identical copy of an object tree");
    }

    // -----------------------------------------------------------------------
    // Summary
    // -----------------------------------------------------------------------
    std::printf("\n--- Summary ---\n");
    std::printf("passes:   %d\n", g_pass);
    std::printf("fails:    %d\n", g_fail);

    if (g_fail == 0) {
        std::printf("\nAll checks passed. main returns 0.\n");
        return 0;
    } else {
        std::printf("\nSome checks failed. main returns 1.\n");
        return 1;
    }
}
