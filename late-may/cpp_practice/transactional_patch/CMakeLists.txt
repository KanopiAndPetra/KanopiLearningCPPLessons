# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that exercises
# a TRANSACTIONAL JSON Patch: a new "all-or-nothing" mode on top of
# psp::json_patch::patch that snapshots the JsonValue tree, runs the
# patch, and rolls back to the snapshot on the first failure.
#
# The transactional layer is implemented as a CONSUMER function
# (psp::json_patch::patch_atomic + patch_dry_run), exactly the same
# shape as the Jul 24 consumer writer and the Jul 25 v014 shadow
# parsers: a proven-in-consumer capability that today proves the
# design end-to-end. A future v0.16.0 promotion is the mechanical
# follow-on.
#
# Why transactional
# -----------------
# The library's existing psp::json_patch::patch is "best-effort
# atomic": it stops on the first op that fails and returns the
# error, but the tree is left in whatever state it had after
# partial application. RFC 6902 §3 says the target document
# SHOULD be left in its previous state on failure; the library
# honors that contract for the *immediately-failing* op but not
# for the *prior* ops that succeeded before the failure.
#
# Transactional Patch closes that gap: a single function call
# either applies ALL the ops cleanly or leaves the tree exactly
# as it was on entry. No "partial" state ever escapes.
#
# What's in this consumer
# -----------------------
#   Section 1 — happy-path: a 3-op patch on a small tree returns
#               void and the tree is fully mutated.
#   Section 2 — atomic rollback: a 3-op patch where the third
#               op fails leaves the tree UNCHANGED from its
#               pre-call state.
#   Section 3 — dry-run: the same failing patch through
#               patch_dry_run leaves the original tree untouched
#               AND returns the error.
#   Section 4 — corner cases: empty patch, single-op patch,
#               self-move copy, nested arrays, large tree.
#   Section 5 — wire-format interop: build -> serialise -> parse
#               -> patch_atomic -> json_to_string works the same
#               as the v0.15.0 round-trip, plus atomicity.
#   Section 6 — sizeof / feature probes.
#
# Design notes
# ------------
# The deep-clone approach is the simplest correct implementation
# and is what we use here. The cost is one extra copy of the
# entire tree on the heap (std::map / std::vector deep-copy). For
# the patch sizes this consumer exercises (KB-scale trees) the
# cost is sub-millisecond. The alternative — journaling per-op
# inverses — is more efficient but has correctness risks for
# MoveOp (where the inverse RemoveOp is straightforward but the
# ApplyOp's value capture must happen at apply-time) and CopyOp
# (where the inverse RemoveOp needs the copy-target location).
# Deep-clone wins on simplicity; the inverse-journal optimisation
# is a future lesson.
#
# Usage (assumes psp_span_lib v0.15.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-03-transactional-patch
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-03-transactional-patch
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-03-transactional-patch

cmake_minimum_required(VERSION 3.16)
project(transactional_patch_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that exercises a TRANSACTIONAL JSON Patch (psp::json_patch::patch_atomic + patch_dry_run as consumer-side functions) on top of the library's existing patch() engine. All-or-nothing semantics: snapshot the tree, run the patch, roll back on the first failure so the tree is left in its pre-call state."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.15.0 RFC 6902 §3 wire-format
# writer (psp::json_patch::serialise_patch_document) for the
# wire-format interop section. The transactional layer itself only
# needs the v0.12.0 engine, but pinning 0.15 keeps the consumer
# consistent with the Aug 2 lesson's "v0.15.0 candidates" set.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-03-transactional-patch
    P-2026-08-03-transactional-patch.cpp
)

target_link_libraries(P-2026-08-03-transactional-patch
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-03-transactional-patch PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-03-transactional-patch PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
