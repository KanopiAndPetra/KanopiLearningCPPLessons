# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that designs
# the JOURNAL-AWARE SELF-MOVE SAFE wrapper for the inverse-journal
# transactional layer:
#
#   psp::json_patch::patch_journaled_self_move_safe(
#       JsonValue& root, std::span<const JsonPatchOp> ops)
#       -> std::expected<void, JsonPatchError>
#
# This is the explicit forward-on from the Aug 6 self-move fix
# "What's NOT in this lesson" section. The Aug 6 lesson closed
# the engine self-move arc with a pre-filter wrapper
# (patch_self_move_safe), but flagged that the inverse-journal
# replay path (patch_journaled, Aug 5) does NOT use the same
# pre-filter — so a self-move that appears as a journal entry
# (i.e., the inverse of a cross-move, where the cross-move's
# own from == path) would still hit the v0.12.0 engine's
# self-delete bug during replay.
#
# Today's consumer solves this by composing the three Aug 3
# + Aug 5 + Aug 6 wrappers into a single
# patch_journaled_self_move_safe:
#
#   1. Pre-filter the input (drop self-moves; same as Aug 6).
#   2. Walk the filtered list, capturing the inverse of each
#      op as it's applied (same as Aug 5).
#   3. On failure, replay the journal in REVERSE (same as Aug 5).
#   4. ALSO: filter self-moves out of the JOURNAL before
#      replay, so a journal entry that becomes a self-move
#      (the cross-move's own undo) is dropped during replay.
#
# Step 4 is the new bit. The Aug 5 lesson's replay_journal
# forward the journal as-is to psp::json_patch::patch, which
# means a self-move in the journal hits the v0.12.0 engine's
# self-delete bug during replay. Today's wrapper runs the
# journal through filter_self_moves before replay, so the
# replay path is observably safe against self-moves.
#
# Same observable contract as patch_journaled:
#   - On success: root is fully mutated, return {}.
#   - On failure: root is restored to the pre-state, return
#     std::unexpected{error}.
# Plus the Aug 6 self-move rule:
#   - Self-moves in the user-facing input are dropped.
#   - Self-moves in the journal (replay-direction) are
#     dropped.
#
# Library version unchanged at v0.15.0; future v0.16.0
# promotion is mechanical (lift
# patch_journaled_self_move_safe + detail::filter_self_moves
# + detail::is_self_move into <psp_span/json_ext.h> OR
# replace with the one-line engine patch; bump the version).
#
# Where this fits in the arc
# --------------------------
# The Aug 6 lesson (P-2026-08-06-self-move-fix.md) closed
# the engine self-move arc with a pre-filter wrapper. The
# Aug 6 "What's NOT in this lesson" section explicitly
# flagged today's composition as future work:
#
#   "It does not repair the inverse-journal replay
#    interaction. A self-move that's part of a journal
#    (i.e., the inverse-journal's `MoveOp{path, from}` for
#    undoing a cross-move) becomes a self-move when `from
#    == path` (which is the cross-move's `path == from`
#    case, i.e., a self-move being undone). Today's
#    wrapper handles this transparently because the journal
#    entries go through `psp::json_patch::patch`, not
#    `patch_self_move_safe`. A future lesson could lift
#    `patch_self_move_safe` into the journal path so
#    journal replays also drop self-moves; the
#    observability/correctness story is the same as
#    today's wrapper (a self-move in a journal is a no-op
#    in the inverse-journal sense)."
#
# Today is that future lesson.
#
# What the consumer exercises
# ----------------------------
#   Section 1 — symbol-presence + the per-op pre-filter +
#               the journal-pre-filter spec.
#   Section 2 — the aug-6 self-move bug, exposed through the
#               journal: a self-move in the input is dropped
#               (no-op), a self-move in the journal (replay)
#               is also dropped.
#   Section 3 — every MoveOp shape (self / valid / clobber /
#               missing-from) through the journal.
#   Section 4 — interop with patch_atomic + patch_dry_run
#               (the Aug 3 deep-clone variant) and
#               patch_journaled (the Aug 5 inverse-journal
#               variant).
#   Section 5 — end-to-end: a self-move being UNDONE via
#               the journal (the case the Aug 6 lesson's
#               "What's NOT in this lesson" section
#               explicitly flagged).
#   Section 6 — sizeof / feature probes.
#
#   ~30+ cases across 6 sections, all expected to pass.
#
# Why consumer-side and not library-side today
# ---------------------------------------------
# Same shape as the Aug 3 / Aug 4 / Aug 5 / Aug 6 lessons:
# a proven-in-consumer wrapper that exercises the design
# end-to-end. The library version is unchanged at v0.15.0;
# a future v0.16.0 promotion is mechanical.
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-09-journal-self-move-safe
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-09-journal-self-move-safe
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-09-journal-self-move-safe

cmake_minimum_required(VERSION 3.16)
project(journal_self_move_safe_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that designs the JOURNAL-AWARE SELF-MOVE SAFE wrapper (psp::json_patch::patch_journaled_self_move_safe as a consumer-side composition of the Aug 3 patch_atomic + Aug 5 patch_journaled + Aug 6 patch_self_move_safe wrappers). Closes the journal-replay self-move gap the Aug 6 lesson's 'What's NOT in this lesson' section explicitly flagged. Library version unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.12.0 engine (the one with
# the self-move bug) + v0.13.0 parser + v0.15.0 writer + the
# v0.11.0 pointer, mirroring the Aug 6 lesson. The journal
# wrapper itself only needs the v0.12.0 engine + v0.11.0 pointer
# but pinning 0.15 keeps the consumer consistent with the
# Aug 2/3/4/5/6 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-09-journal-self-move-safe
    P-2026-08-09-journal-self-move-safe.cpp
)

target_link_libraries(P-2026-08-09-journal-self-move-safe
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-09-journal-self-move-safe PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-09-journal-self-move-safe PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
