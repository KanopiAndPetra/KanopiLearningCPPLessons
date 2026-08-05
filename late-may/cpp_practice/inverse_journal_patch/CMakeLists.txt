# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that designs
# an INVERSE-JOURNAL variant of psp::json_patch::patch_atomic:
#
#   psp::json_patch::patch_journaled(JsonValue& root, span<JsonPatchOp>)
#       -> std::expected<void, JsonPatchError>
#
# Instead of deep-cloning the pre-state tree (the Aug 3 lesson's
# approach), today's consumer captures the INVERSE of each op as it
# is applied. On success the journal is discarded; on failure the
# journal is replayed in reverse to restore the pre-state.
#
# Where this fits in the arc
# --------------------------
# The Aug 3 lesson (P-2026-08-03-transactional-patch.md) shipped
# the deep-clone variant of patch_atomic + patch_dry_run and
# explicitly listed inverse-journaling as a forward-on candidate:
#
#   "Inverse-journal optimisation for patch_atomic — per-op
#    journal of inverses instead of a full deep-clone; relevant
#    for MB-scale patches."
#
# Today is that lesson. The deep-clone variant stays in
# psp::json_patch::patch_atomic (the Aug 3 API is unchanged);
# the inverse-journal variant lives in
# psp::json_patch::patch_journaled (a NEW function in this
# consumer TU). The two are interchangeable at the call site
# for the "small-patch" case; the journal shines for
# "big-tree-small-patch" (a 10MB tree, a 5-op patch — the
# deep-clone touches 10MB; the journal touches 5 nodes).
#
# What the consumer exercises
# ----------------------------
#
#   Section 1 — symbol-presence + the per-op inverse spec for
#               all six RFC 6902 op kinds (add / remove / replace
#               / move / copy / test).
#   Section 2 — happy path: a successful 4-op patch on a small
#               tree commits; the journal is empty after success;
#               the journaled-applied tree matches the deep-clone-
#               applied tree.
#   Section 3 — failure path: a 4-op patch where op 2 fails; the
#               journal is replayed; the tree is BYTE-IDENTICAL
#               to the pre-state.
#   Section 4 — every op kind individually, with both happy and
#               failure paths.
#   Section 5 — replay of a journal via the existing patch()
#               function (proves the journal entries are valid
#               JsonPatchOp values).
#   Section 6 — interop: parse a wire-format document via the
#               v0.13.0 parser, then run patch_journaled on the
#               parsed vector.
#   Section 7 — back-compat: patch_journaled + patch_atomic +
#               patch_dry_run coexist; the Aug 3 functions
#               are unchanged.
#   Section 8 — sizeof / feature probes.
#
#   Section totals: ~70+ test cases across 8 sections, all
#   expected to pass.
#
# Why consumer-side and not library-side today
# ---------------------------------------------
# Same shape as the Aug 3 + Aug 4 lessons: a proven-in-consumer
# capability that exercises the design end-to-end. The library
# version is unchanged at v0.15.0. A future v0.16.0 promotion
# is mechanical (lift patch_journaled + the per-op inverse
# helper into <psp_span/json_ext.h>; bump the version).
#
# Why a NEW function (and not a flag on patch_atomic)
# ---------------------------------------------------
# patch_atomic and patch_journaled have observably identical
# behaviour on success and on failure (both return
# std::expected<void, JsonPatchError> and both leave the tree
# at the same state). They differ ONLY in the cost of the
# rollback path. A flag would couple the two implementations
# and force the deep-clone's overhead on every journaled call.
# Two functions keep the call site self-documenting AND let
# the consumer benchmark each path independently.
#
# Build (assumes psp_span_lib v0.15.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-05-inverse-journal-patch
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-05-inverse-journal-patch
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-05-inverse-journal-patch

cmake_minimum_required(VERSION 3.16)
project(inverse_journal_patch_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that designs an INVERSE-JOURNAL variant of psp::json_patch::patch_atomic (psp::json_patch::patch_journaled as a consumer-side function): per-op journal of inverses instead of a full pre-state deep-clone. Library version is unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.12.0 engine + v0.11.0
# pointer + v0.13.0 parser + v0.15.0 writer + the Aug 3
# patch_atomic + deep_clone for the side-by-side comparison.
# The journaled wrapper itself only needs the v0.12.0 engine,
# but pinning 0.15 keeps the consumer consistent with the
# Aug 2/3/4 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-05-inverse-journal-patch
    P-2026-08-05-inverse-journal-patch.cpp
)

target_link_libraries(P-2026-08-05-inverse-journal-patch
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-05-inverse-journal-patch PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-05-inverse-journal-patch PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
