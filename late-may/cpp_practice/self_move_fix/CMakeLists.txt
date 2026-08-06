# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that designs
# the SELF-MOVE FIX for psp::json_patch::patch:
#
#   psp::json_patch::patch_self_move_safe(JsonValue& root, span<JsonPatchOp>)
#       -> std::expected<void, JsonPatchError>
#
# Instead of patching the engine proper (a one-line change in
# <psp_span/json_ext.h>), today's consumer wraps the engine with a
# pre-filter that drops self-moves (MoveOp{from, path} with from == path)
# before handing the patch list to the v0.12.0 engine. This:
#
#   - Closes the RFC 6902 §4.4 "self-move is a no-op" rule gap that
#     the v0.12.0 engine's copy-then-remove sequence left open.
#   - Composes cleanly with the Aug 3 transactional layer
#     (patch_atomic + patch_dry_run) and the Aug 5 inverse-journal
#     layer (patch_journaled) — both of which call the engine
#     internally.
#   - Is consumer-side; the library version is unchanged at v0.15.0.
#     A future v0.16.0 promotion would either lift
#     `patch_self_move_safe` as a header function, or apply the
#     one-line engine patch (early-return on from == path).
#
# Where this fits in the arc
# --------------------------
# The Aug 5 lesson (P-2026-08-05-inverse-journal-patch.md) closed
# the inverse-journal optimization arc and re-listed three
# remaining v0.15.0 candidates (re-quoting):
#
#   - JSON Schema validation
#   - dispatcher int64-vs-double preservation guard widening
#   - Engine-level self-move fix — short-circuit from == path in
#     apply_move to honor RFC 6902 §4.4's "self-move is a no-op"
#     rule. Pre-existing engine quirk, flagged in the Aug 3 lesson.
#
# Today is the third one. It is the SMALLEST of the three candidates
# (one-line bug fix in the engine) but it has the nicest pedagogical
# shape: a clear spec violation, a precise diagnostic test, and a
# three-line fix (is_self_move + filter_self_moves + wrapper) that
# the consumer proves end-to-end.
#
# What the consumer exercises
# ----------------------------
#   Section 1 — symbol-presence + the per-op pre-filter spec
#   Section 2 — the bug repro: v0.12.0 self-moves self-delete;
#               patch_self_move_safe leaves the tree untouched.
#   Section 3 — every MoveOp shape (self / valid / clobber / missing)
#   Section 4 — interop with patch_atomic + patch_dry_run + the Aug 5
#               inverse-journal wrapper (mirrored in this TU).
#   Section 5 — wire-format round-trip (parse a doc with a self-move op
#               and prove the safe wrapper handles it).
#   Section 6 — back-compat with the v0.12.0 engine quirks (clobber,
#               missing-from, test mismatch all still surface).
#   Section 7 — sizeof / feature probes.
#
#   ~40+ cases across 7 sections, all expected to pass.
#
# Why consumer-side and not library-side today
# ---------------------------------------------
# Same shape as the Aug 3 + Aug 4 + Aug 5 lessons: a
# proven-in-consumer capability that exercises the design end-to-end.
# The library version is unchanged at v0.15.0; a future v0.16.0
# promotion is mechanical (lift patch_self_move_safe +
# filter_self_moves + is_self_move into <psp_span/json_ext.h>;
# bump the version).
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-06-self-move-fix
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-06-self-move-fix
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-06-self-move-fix

cmake_minimum_required(VERSION 3.16)
project(self_move_fix_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that designs the SELF-MOVE FIX for psp::json_patch::patch (psp::json_patch::patch_self_move_safe as a consumer-side wrapper): pre-filters self-moves (MoveOp{from,path} with from == path) before handing the patch to the v0.12.0 engine. Closes the RFC 6902 §4.4 'self-move is a no-op' rule gap. Library version unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.12.0 engine (the one with
# the self-move bug) + v0.13.0 parser + v0.15.0 writer + the
# v0.11.0 pointer. The safe wrapper itself only needs the
# v0.12.0 engine, but pinning 0.15 keeps the consumer consistent
# with the Aug 2/3/4/5 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-06-self-move-fix
    P-2026-08-06-self-move-fix.cpp
)

target_link_libraries(P-2026-08-06-self-move-fix
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-06-self-move-fix PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-06-self-move-fix PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
