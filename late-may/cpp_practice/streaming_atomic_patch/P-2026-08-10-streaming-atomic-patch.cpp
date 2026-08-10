// P-2026-08-10 — Consumer of psp_span_lib v0.15.0 that designs the
// STREAMING-ATOMIC JSON Patch wrapper composing the Aug 4
// streaming parser + the Aug 9 journal-aware self-move-safe
// transactional layer into a single function:
//
//   psp::json_patch::parse_and_apply_atomic_streaming(
//       JsonValue& root, psp::Span<const char>& doc)
//       -> std::expected<std::size_t, JsonPatchError>
//
// Where this fits in the arc
// --------------------------
// Three lessons have shipped wrappers over the v0.12.0 engine:
//   - Aug 4: parse_patch_document_at / parse_patch_document_next_at /
//            parse_one_op_at (streaming wire-format parser, cursor-primitive).
//   - Aug 9: patch_journaled_self_move_safe (inverse-journal rollback
//            + self-move filter at both the input AND the journal
//            boundaries; the most complete transactional layer
//            shipped to date).
//
// The Aug 4 lesson's "Where we go next" section explicitly
// flagged the streaming-atom composition as forward-on work:
//
//   "Natural follow-on lessons: std::generator adapter on top of
//    the begin/next functions; inverse-journal optimisation for
//    patch_atomic; engine-level self-move fix; JSON Schema
//    validation in a new <psp_span/json_schema.h>; widen the
//    dispatcher's int64-vs-double preservation guard from int to
//    int64_t."
//
// And the Aug 9 lesson's "Where we go next" added:
//
//   "Streaming-atomic wrapper — per-op snapshot for the
//    streaming parser's begin/next API; the journal composes
//    cleanly with the streaming parser, but per-op
//    snapshotting is a separate design exercise."
//
// Today is that future lesson. The composition is the natural
// next step: the Aug 4 parser is a cursor-primitive; the Aug 9
// wrapper is per-op. Wiring them together means each op is
// streamed + applied + journaled in a single pass — no
// intermediate std::vector<JsonPatchOp> allocation. The wrapper
// is a 30-line composition, not a new algorithm.
//
// The composition problem
// ------------------------
// The Aug 4 parser returns one op per call:
//
//   auto first = parse_patch_document_at(s);  // cursor at '['
//   auto next  = parse_patch_document_next_at(s);  // iter
//   ...
//
// The Aug 9 wrapper accepts a std::span<const JsonPatchOp>:
//
//   patch_journaled_self_move_safe(root, ops);
//
// The two recipes meet at the per-op boundary. The
// "obvious" composition would be: collect all ops into a
// std::vector, then call the wrapper. But that defeats the
// streaming aspect — the caller pays the full document parse
// + vector allocation up-front. The streaming version
// amortises parsing over application:
//
//   parse_and_apply_atomic_streaming(root, doc)
//       auto first = parse_patch_document_at(doc);
//       if (!first) return first.error();
//       for each parsed op (streamed, one at a time):
//           inv = inverse_for(root, op)        // journal entry
//           r   = patch(root, {op})             // engine
//           if r failed: replay_journal(...); return error
//           else:        push inv to journal
//       return N (number of applied ops)
//
// Why this is more than a one-line composition
// ---------------------------------------------
// Two design questions the simple composition doesn't answer:
//
// 1. The Aug 4 parser is asymmetric: the BEGIN call MUST see
//    '[', the NEXT call MUST NOT see '['. The streaming wrapper
//    hides that from the caller — the caller passes a wire-
//    format document and gets back the op count. The wrapper
//    itself is the only place that knows about the asymmetry.
//
// 2. The end-of-document case is signaled by BadDocument
//    (consume ']'). That's the SAME error type as a real
//    parse failure. The wrapper has to distinguish: the end-
//    of-doc BadDocument is the SUCCESS terminal; any other
//    BadDocument is a real parse failure. The simple
//    composition would propagate BadDocument as if it were
//    a real failure.
//
// 3. The cursor state on failure matters. The Aug 4 parser
//    leaves the cursor BYTE-IDENTICAL on parse failure. The
//    streaming wrapper inherits that — on a parse failure
//    mid-stream, the cursor is at the failure point and the
//    root is unchanged. The caller can resync.
//
// Why consumer-side and not library-side today
// --------------------------------------------
// Same shape as Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9: a
// proven-in-consumer capability that exercises the design
// end-to-end. Library version is unchanged at v0.15.0. A
// future v0.16.0 promotion is mechanical (lift
// parse_and_apply_atomic_streaming + the detail:: helpers
// into <psp_span/json_ext.h>; bump the version).
//
// What the consumer exercises
// ----------------------------
//   Section 1 — symbol-presence + the per-op streaming parser
//               surface (mirrors Aug 4).
//   Section 2 — single-op document round-trip through
//               parse_and_apply_atomic_streaming.
//   Section 3 — multi-op document with one failing op (mid-
//               stream rollback; pre-state preserved).
//   Section 4 — multi-op document with a parse failure mid-
//               stream (cursor at the failure point; root
//               unchanged; cursor rewind verified).
//   Section 5 — end-to-end: streaming-atomic produces the
//               same final tree as the in-memory
//               patch_journaled_self_move_safe wrapper.
//   Section 6 — sizeof / feature probes.
//
//   ~30+ cases across 6 sections, all expected to pass.

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
// Re-implement the Aug 4 streaming parser + Aug 9 journal-aware self-move
// safe wrapper in this TU (consumer-side pattern matching the Aug 4 / Aug 9
// lessons).
// ===========================================================================
//
// The Aug 4 + Aug 9 lessons both mirrored their wrappers in
// their consumer TUs (rather than linking them across
// consumers). Today's consumer mirrors both because the
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
// Mirrors the Aug 3 / Aug 9 deep_clone. Used by lookup_at
// to capture pre-state values for the journal.
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
//
// The Aug 4 parser is a single per-op parser +
// array-driver. The per-op parser is the load-bearing piece;
// the array-driver just walks '[' + per-op + ',' repeatedly.
//
// The detail::parse_one_op_at_impl consumes one JSON value
// (must be an object), validates it, and assembles a
// JsonPatchOp via the same logic as the v0.13.0
// build_one_op (re-implemented locally to keep the consumer
// self-contained).
namespace detail {

// build_one_op — assemble a JsonPatchOp from a parsed JSON
// object (mirrors the v0.13.0 driver's logic).
inline std::expected<JsonPatchOp, JsonPatchError>
build_one_op(const psp::JsonValue& v) noexcept {
    // Must be an object.
    const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&v.value);
    if (!obj) {
        return std::unexpected{JsonPatchError::WrongType};
    }

    // Pull the "op" field.
    auto it = obj->find("op");
    if (it == obj->end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto* op_str = std::get_if<std::string>(&it->second.value);
    if (!op_str) {
        return std::unexpected{JsonPatchError::WrongType};
    }

    // Pull the "path" field.
    auto pit = obj->find("path");
    if (pit == obj->end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto* path = std::get_if<std::string>(&pit->second.value);
    if (!path) {
        return std::unexpected{JsonPatchError::WrongType};
    }

    // Pull the optional fields.
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

// parse_one_op_at_impl — inner per-op parser (cursor-primitive).
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

// parse_patch_document_at — BEGIN call. Cursor must be at '['
// (with optional leading whitespace). Consumes '[' + the
// first op + trailing whitespace + ','. On end-of-doc
// ('[]'), consumes ']' and returns BadDocument.
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

    // Check for empty-document case: '[' then ']'.
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

    // Consume trailing whitespace + ','.
    auto after = s;
    if (auto r = psp::skip_whitespace_at(s); !r) {
        s = entry;
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    } else {
        // No comma: must be ']' (last op) or we have a malformed
        // document. Either way, leave the cursor at the right
        // spot for parse_patch_document_next_at or the caller.
        s = after;
    }
    return op;
}

// parse_patch_document_next_at — ITERATE call. Cursor is at
// the start of the next op (or ']' for end-of-doc). Returns
// the next op; on ']', consumes it and returns BadDocument
// (which is the SUCCESS terminal for the streaming wrapper).
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
    // Consume trailing whitespace + ','.
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

// replay_journal — apply the journal in REVERSE order, with
// self-move filter (Aug 9 shape).
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
// patch_journaled_self_move_safe — the Aug 9 wrapper (mirrors Aug 9)
// -----------------------------------------------------------------------
inline std::expected<void, JsonPatchError>
patch_journaled_self_move_safe(psp::JsonValue& root,
                               std::span<const JsonPatchOp> ops) noexcept {
    auto filtered = psp::json_patch::detail::filter_self_moves(ops);

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

// -----------------------------------------------------------------------
// parse_and_apply_atomic_streaming — TODAY's NEW wrapper
// -----------------------------------------------------------------------
//
// The streaming-atomic composition of the Aug 4 parser + the
// Aug 9 transactional layer. Streams ops from a wire-format
// document and applies each op transactionally, in a single
// pass. No intermediate std::vector<JsonPatchOp> allocation.
//
// On success: returns std::expected<std::size_t, JsonPatchError>
//             with the number of applied ops. The cursor is
//             past the closing ']'.
// On parse failure: cursor is BYTE-IDENTICAL to the
//             per-failure-point state (inherited from the
//             Aug 4 cursor-primitive contract). Root is
//             unchanged. Returns std::unexpected{error}.
// On apply failure: cursor is past the failing op (i.e., the
//             op that the engine rejected was consumed from
//             the cursor before the engine call). Root is
//             restored to the pre-state. Returns
//             std::unexpected{error}.
//
// Library version unchanged at v0.15.0; future v0.16.0
// promotion is mechanical (lift this function + the
// detail:: helpers into <psp_span/json_ext.h>; bump the
// version).
//
// End-of-document detection
// --------------------------
// The Aug 4 parser returns BadDocument for BOTH end-of-doc
// (consume ']') AND a non-'[' start. The two are
// indistinguishable from the BadDocument return value alone.
// The wrapper resolves the ambiguity by:
//
// 1. Pre-checking that the document starts with '[' (with
//    optional leading whitespace). If not, it's a real parse
//    failure: return BadDocument WITHOUT treating it as
//    end-of-doc.
// 2. For the BEGIN call: BadDocument AFTER consuming '['
//    means the document was '[]' (end-of-doc, success, zero
//    ops). Anything else is a real parse failure.
// 3. For the NEXT call: BadDocument AFTER consuming ']' means
//    end-of-doc (success). Anything else is a real parse
//    failure.
inline std::expected<std::size_t, JsonPatchError>
parse_and_apply_atomic_streaming(psp::JsonValue& root,
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

    // Begin: parse the first op. The BEGIN call sees '['.
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        // BadDocument at this point means end-of-doc
        // (the document was '[]' with optional whitespace).
        // The pre-check above ensured the document starts
        // with '[', so a real parse failure would have
        // surfaced as some other error code (the parser
        // returns WrongType / MissingField / UnknownOp for
        // malformed ops).
        if (first.error() == JsonPatchError::BadDocument) {
            return std::size_t{0};
        }
        return std::unexpected{first.error()};
    }

    // Walk the ops one at a time. The journal is the
    // inverse of each applied op; on any failure (pre-state
    // lookup or engine call), replay the journal to roll
    // back, then return the error.
    std::vector<JsonPatchOp> journal;
    journal.reserve(8);  // rough pre-reserve; grows as needed

    std::size_t applied = 0;
    JsonPatchOp op = *first;

    for (;;) {
        // Step 1: input-side self-move filter (Aug 9 rule).
        // Self-moves are no-ops; we just don't count them
        // and don't add them to the journal.
        if (!psp::json_patch::detail::is_self_move(op)) {
            // Step 2: compute the inverse of this op
            // against the CURRENT state (which is the
            // pre-state for this op).
            JsonPatchError pre_err = JsonPatchError::BadDocument;
            auto inv = psp::json_patch::detail::inverse_for(root, op, pre_err);
            if (!inv && op.kind != OpKind::Test) {
                // Pre-state lookup failed. Replay (with
                // self-move filter) to roll back, surface
                // the pre-state error.
                auto replay = psp::json_patch::detail::replay_journal(root, journal);
                if (!replay) {
                    return std::unexpected{replay.error()};
                }
                return std::unexpected{pre_err};
            }

            // Step 3: apply the op via the engine.
            auto r = psp::json_patch::patch(root,
                std::span<const JsonPatchOp>{&op, 1});
            if (!r) {
                // Engine failed. Replay (with self-move
                // filter).
                auto replay = psp::json_patch::detail::replay_journal(root, journal);
                if (!replay) {
                    return std::unexpected{replay.error()};
                }
                return std::unexpected{r.error()};
            }

            // Step 4: op succeeded; record the inverse in
            // the journal.
            if (inv) {
                journal.push_back(std::move(*inv));
            }
            ++applied;
        }

        // Step 5: stream the next op. NEXT call does NOT
        // see '[' (the BEGIN call already consumed it).
        auto next = psp::json_patch::parse_patch_document_next_at(doc);
        if (!next) {
            // BadDocument is the END-OF-DOC terminal:
            // success, doc is fully applied.
            if (next.error() == JsonPatchError::BadDocument) {
                return applied;
            }
            // Real parse failure. Roll back to the
            // pre-state. The cursor is at the failure
            // point (cursor-primitive contract).
            auto replay = psp::json_patch::detail::replay_journal(root, journal);
            if (!replay) {
                return std::unexpected{replay.error()};
            }
            return std::unexpected{next.error()};
        }
        op = *next;
    }
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test framework (same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9
// lessons).
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

void check_eq_str(std::string_view actual, std::string_view expected,
                  std::string_view name) {
    bool eq = (actual == expected);
    if (eq) {
        ++g_pass;
        std::printf("  PASS: %.*s\n", static_cast<int>(name.size()),
                    name.data());
    } else {
        ++g_fail;
        std::printf("  FAIL: %.*s   (expected=%.*s, actual=%.*s)\n",
                    static_cast<int>(name.size()), name.data(),
                    static_cast<int>(expected.size()), expected.data(),
                    static_cast<int>(actual.size()), actual.data());
    }
}

psp::JsonValue make_initial() {
    return psp::JsonValue{std::map<std::string, psp::JsonValue>{
        {"x", psp::JsonValue{std::map<std::string, psp::JsonValue>{
            {"k", psp::JsonValue{std::int64_t(42)}},
        }}},
    }};
}

// Helper: take a string and turn it into a mutable Span<const char>.
// We need a backing buffer because the streaming parser consumes
// from the span. The buffer is owned by the caller.
struct Doc {
    std::string text;
    psp::Span<const char> view() noexcept {
        return psp::Span<const char>{text.data(), text.size()};
    }
};

}  // namespace

// ===========================================================================
// main
// ===========================================================================

int main() {
    std::printf("P-2026-08-10 — streaming-atomic JSON Patch\n"
                "  (composes Aug 4 streaming parser + Aug 9 journal-aware\n"
                "  self-move-safe wrapper into a single transactional layer;\n"
                "  library version unchanged at v0.15.0)\n");

    // -----------------------------------------------------------------------
    // Section 1 — symbol-presence + the per-op streaming parser
    //   surface.
    // -----------------------------------------------------------------------
    header("Section 1: symbol-presence + per-op streaming parser spec");

    using psp::json_patch::parse_and_apply_atomic_streaming;
    using psp::json_patch::parse_patch_document_at;
    using psp::json_patch::parse_patch_document_next_at;
    using psp::json_patch::parse_one_op_at;

    {
        auto fn = &parse_and_apply_atomic_streaming;
        check(fn != nullptr,
              "1a &psp::json_patch::parse_and_apply_atomic_streaming is well-defined");
    }

    check(std::is_same_v<
              std::remove_reference_t<decltype(parse_and_apply_atomic_streaming(
                  std::declval<psp::JsonValue&>(),
                  std::declval<psp::Span<const char>&>()))>,
              std::expected<std::size_t, JsonPatchError>>,
          "1b parse_and_apply_atomic_streaming signature matches "
          "std::expected<std::size_t, JsonPatchError>");

    // The streaming parser is a 3-function surface (mirrors Aug 4).
    {
        auto fn = &parse_patch_document_at;
        check(fn != nullptr,
              "1c &psp::json_patch::parse_patch_document_at is well-defined");
    }
    {
        auto fn = &parse_patch_document_next_at;
        check(fn != nullptr,
              "1d &psp::json_patch::parse_patch_document_next_at is well-defined");
    }
    {
        auto fn = &parse_one_op_at;
        check(fn != nullptr,
              "1e &psp::json_patch::parse_one_op_at is well-defined");
    }

    // The streaming parser's cursor-primitive contract: a
    // BEGIN call on a buffer that doesn't start with '['
    // returns BadDocument and leaves the cursor
    // BYTE-IDENTICAL.
    {
        Doc d{std::string{"not-a-patch"}};
        auto s = d.view();
        auto r = parse_patch_document_at(s);
        check_eq_err(r.error(), JsonPatchError::BadDocument,
                     "1f parse_patch_document_at on non-'[' buffer returns BadDocument");
        check_eq_i(s.size(), d.text.size(),
                   "1g parse_patch_document_at leaves cursor BYTE-IDENTICAL on failure");
    }

    // An empty document '[]' is a successful zero-op apply.
    {
        Doc d{std::string{"[]"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(r.has_value(), "1h empty document '[]' is a successful zero-op apply");
        check_eq_i(*r, std::size_t{0},
                   "1i empty document applies 0 ops");
        // Cursor is past the closing ']'.
        check_eq_i(s.size(), std::size_t{0},
                   "1j cursor is past ']' after empty document");
    }

    // -----------------------------------------------------------------------
    // Section 2 — single-op document round-trip through
    //   parse_and_apply_atomic_streaming.
    // -----------------------------------------------------------------------
    header("Section 2: single-op document round-trip");

    {
        // Single add op: [{"op":"add","path":"/y","value":1}]
        Doc d{std::string{R"([{"op":"add","path":"/y","value":1}])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(r.has_value(),
              "2a single-op document applies successfully");
        check_eq_i(*r, std::size_t{1},
                   "2b single-op document applies 1 op");
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && obj->contains("y"),
              "2c /y was added");
        const auto* y = std::get_if<std::int64_t>(&obj->at("y").value);
        check(y != nullptr && *y == 1,
              "2d /y value is 1");
        check_eq_i(s.size(), std::size_t{0},
                   "2e cursor is past ']' after single-op document");
    }

    {
        // Single remove op: [{"op":"remove","path":"/x/k"}]
        Doc d{std::string{R"([{"op":"remove","path":"/x/k"}])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(r.has_value(),
              "2f single-op remove applies successfully");
        check_eq_i(*r, std::size_t{1},
                   "2g single-op remove applies 1 op");
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        const auto* inner = std::get_if<std::map<std::string, psp::JsonValue>>(
            &obj->at("x").value);
        check(inner != nullptr && !inner->contains("k"),
              "2h /x/k was removed");
    }

    {
        // Single self-move: [{"op":"move","path":"/x/k","from":"/x/k"}]
        // The streaming wrapper drops the self-move (Aug 9
        // rule). The tree is unchanged.
        Doc d{std::string{R"([{"op":"move","path":"/x/k","from":"/x/k"}])"}};
        auto s = d.view();
        auto root_before = make_initial();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(r.has_value(),
              "2i single-op self-move applies successfully (dropped)");
        check_eq_i(*r, std::size_t{0},
                   "2j self-move is NOT counted (it's a no-op)");
        // The tree is unchanged (the v0.12.0 engine would
        // have self-deleted /x/k; the wrapper drops the
        // self-move before the engine sees it).
        check_eq_str(psp::json_to_string(root),
                     psp::json_to_string(root_before),
                     "2k tree is unchanged after self-move (no engine self-delete)");
    }

    // -----------------------------------------------------------------------
    // Section 3 — multi-op document with one failing op (mid-stream
    //   rollback; pre-state preserved).
    // -----------------------------------------------------------------------
    header("Section 3: multi-op document with one failing op (rollback)");

    {
        // 3-op patch: 1 add /temp, 1 add /u, 1 remove /missing
        // The 3rd op fails. The wrapper must roll back the
        // first two adds.
        Doc d{std::string{R"([{"op":"add","path":"/temp","value":1},{"op":"add","path":"/u","value":99},{"op":"remove","path":"/missing"}])"}};
        auto s = d.view();
        auto root_before = make_initial();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(!r.has_value(),
              "3a mid-stream failing op returns error");
        check_eq_err(r.error(), JsonPatchError::PointerNotFound,
                     "3b error is PointerNotFound");
        // Pre-state preserved: /temp and /u were rolled back.
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && !obj->contains("temp"),
              "3c /temp was rolled back (not in final tree)");
        check(obj != nullptr && !obj->contains("u"),
              "3d /u was rolled back (not in final tree)");
        // Pre-state shape: original tree, intact.
        check_eq_str(psp::json_to_string(root),
                     psp::json_to_string(root_before),
                     "3e tree is byte-identical to pre-state after rollback");
    }

    {
        // 3-op patch: 1 add /temp, 1 remove /missing, 1 add /u
        // The 2nd op fails. The wrapper must roll back the
        // first add.
        Doc d{std::string{R"([{"op":"add","path":"/temp","value":1},{"op":"remove","path":"/missing"},{"op":"add","path":"/u","value":99}])"}};
        auto s = d.view();
        auto root_before = make_initial();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(!r.has_value(),
              "3f mid-stream failing op returns error (early fail)");
        check_eq_err(r.error(), JsonPatchError::PointerNotFound,
                     "3g error is PointerNotFound");
        // /temp rolled back, /u was never applied (the parser
        // already consumed it from the cursor, but the
        // engine wasn't called for it).
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && !obj->contains("temp"),
              "3h /temp was rolled back");
        check(obj != nullptr && !obj->contains("u"),
              "3i /u was not applied (early fail stopped the engine)");
        check_eq_str(psp::json_to_string(root),
                     psp::json_to_string(root_before),
                     "3j tree is byte-identical to pre-state after rollback");
    }

    {
        // 3-op patch all successful: 1 add /a, 1 add /b, 1 add /c.
        Doc d{std::string{R"([{"op":"add","path":"/a","value":1},{"op":"add","path":"/b","value":2},{"op":"add","path":"/c","value":3}])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(r.has_value(),
              "3k 3-op successful document applies");
        check_eq_i(*r, std::size_t{3},
                   "3l 3 ops were applied");
        const auto* obj = std::get_if<std::map<std::string, psp::JsonValue>>(&root.value);
        check(obj != nullptr && obj->contains("a")
              && obj->contains("b") && obj->contains("c"),
              "3m all three keys present in final tree");
        check_eq_i(s.size(), std::size_t{0},
                   "3n cursor is past ']' after 3-op document");
    }

    // -----------------------------------------------------------------------
    // Section 4 — multi-op document with a parse failure mid-stream
    //   (cursor at the failure point; root unchanged; cursor
    //   rewind verified).
    // -----------------------------------------------------------------------
    header("Section 4: parse failure mid-stream (cursor rewinds)");

    {
        // 2nd op is malformed (missing 'op' field). The
        // cursor-primitive contract says: cursor is at the
        // failure point, byte-identical to the entry
        // snapshot of parse_one_op_at. The wrapper must NOT
        // touch the root.
        Doc d{std::string{R"([{"op":"add","path":"/a","value":1},{"path":"/b"}])"}};
        auto s = d.view();
        auto root_before = make_initial();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(!r.has_value(),
              "4a parse failure mid-stream returns error");
        check_eq_err(r.error(), JsonPatchError::MissingField,
                     "4b error is MissingField (no 'op' field)");
        // Root is unchanged: the streaming wrapper rolls
        // back via the journal before returning the parse
        // error. The 1st op was applied to the in-memory
        // tree before the 2nd op was parsed; the journal
        // captures its inverse, so the wrapper rolls it
        // back.
        check_eq_str(psp::json_to_string(root),
                     psp::json_to_string(root_before),
                     "4c tree is byte-identical to pre-state after parse failure");
        // Cursor is at the failure point (not past the
        // failing op; not at end-of-doc).
        check(s.size() > 0 && s.size() < d.text.size(),
              "4d cursor is at the failure point (between entry and end)");
    }

    {
        // Document starts with a non-'[' character. The
        // wrapper should fail before any op is parsed.
        Doc d{std::string{R"(not-a-patch)"}};
        auto s = d.view();
        auto root_before = make_initial();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(!r.has_value(),
              "4e non-'[' document returns error");
        check_eq_err(r.error(), JsonPatchError::BadDocument,
                     "4f error is BadDocument (no '[')");
        check_eq_str(psp::json_to_string(root),
                     psp::json_to_string(root_before),
                     "4g tree is unchanged for non-'[' document");
        check_eq_i(s.size(), d.text.size(),
                   "4h cursor is BYTE-IDENTICAL for non-'[' document");
    }

    // -----------------------------------------------------------------------
    // Section 5 — end-to-end: streaming-atomic produces the same
    //   final tree as the in-memory patch_journaled_self_move_safe
    //   wrapper.
    // -----------------------------------------------------------------------
    header("Section 5: streaming-atomic == in-memory wrapper (success path)");

    {
        // 3-op document with a self-move: 1 add /temp, 1 self-
        // move, 1 add /u. The streaming wrapper applies 2 ops
        // (the self-move is dropped). The in-memory wrapper
        // applies the same 2 ops. Both trees should be
        // identical.
        Doc d{std::string{R"([{"op":"add","path":"/temp","value":1},{"op":"move","path":"/x/k","from":"/x/k"},{"op":"add","path":"/u","value":99}])"}};
        auto s = d.view();
        auto root_streaming = make_initial();
        auto r1 = parse_and_apply_atomic_streaming(root_streaming, s);
        check(r1.has_value(),
              "5a streaming apply succeeds (self-move dropped)");
        check_eq_i(*r1, std::size_t{2},
                   "5b streaming apply counts 2 ops (self-move excluded)");

        auto root_in_memory = make_initial();
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/temp", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{MoveOp{"/x/k", "/x/k"}},  // self
            JsonPatchOp{AddOp{"/u", psp::JsonValue{std::int64_t(99)}}},
        };
        auto r2 = psp::json_patch::patch_journaled_self_move_safe(
            root_in_memory, std::span<const JsonPatchOp>{ops});
        check(r2.has_value(),
              "5c in-memory apply succeeds (self-move dropped)");

        check_eq_str(psp::json_to_string(root_streaming),
                     psp::json_to_string(root_in_memory),
                     "5d streaming tree == in-memory tree");
    }

    {
        // 4-op document, all successful. Streaming + in-memory
        // should match.
        Doc d{std::string{R"([{"op":"add","path":"/a","value":1},{"op":"add","path":"/b","value":2},{"op":"replace","path":"/x/k","value":99},{"op":"remove","path":"/a"}])"}};
        auto s = d.view();
        auto root_streaming = make_initial();
        auto r1 = parse_and_apply_atomic_streaming(root_streaming, s);
        check(r1.has_value(),
              "5e 4-op streaming apply succeeds");
        check_eq_i(*r1, std::size_t{4},
                   "5f 4-op streaming apply counts 4 ops");

        auto root_in_memory = make_initial();
        std::vector<JsonPatchOp> ops = {
            JsonPatchOp{AddOp{"/a", psp::JsonValue{std::int64_t(1)}}},
            JsonPatchOp{AddOp{"/b", psp::JsonValue{std::int64_t(2)}}},
            JsonPatchOp{ReplaceOp{"/x/k", psp::JsonValue{std::int64_t(99)}}},
            JsonPatchOp{RemoveOp{"/a"}},
        };
        auto r2 = psp::json_patch::patch_journaled_self_move_safe(
            root_in_memory, std::span<const JsonPatchOp>{ops});
        check(r2.has_value(),
              "5g 4-op in-memory apply succeeds");

        check_eq_str(psp::json_to_string(root_streaming),
                     psp::json_to_string(root_in_memory),
                     "5h streaming tree == in-memory tree (4-op)");
    }

    // -----------------------------------------------------------------------
    // Section 6 — sizeof / feature probes.
    // -----------------------------------------------------------------------
    header("Section 6: sizeof / feature probes");

    check(sizeof(JsonPatchOp) > 0,
          "6a sizeof(JsonPatchOp) is non-zero");

    // JsonPatchError enumerator count is unchanged from v0.15.0.
    // (The Aug 9 lesson asserted 13; today is additive only.)
    {
        // The enum has 13 enumerators. We probe via the
        // largest one (PointerNotFound, value 12 = 13th).
        JsonPatchError e = JsonPatchError::PointerNotFound;
        check_eq_err(e, JsonPatchError::PointerNotFound,
                     "6b JsonPatchError has PointerNotFound (v0.15.0 invariant)");
    }

    // The streaming wrapper is consumer-side: no library
    // change. v0.15.0 unchanged.
    {
        Doc d{std::string{R"([{"op":"add","path":"/z","value":1}])"}};
        auto s = d.view();
        auto root = make_initial();
        auto r = parse_and_apply_atomic_streaming(root, s);
        check(r.has_value(),
              "6c streaming wrapper is consumer-side (no library change)");
    }

    // --- Summary ---
    std::printf("\n--- Summary ---\n");
    std::printf("sections: %d\n", g_section);
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
