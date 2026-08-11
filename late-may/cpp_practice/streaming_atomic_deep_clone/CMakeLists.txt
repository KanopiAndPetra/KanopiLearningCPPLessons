# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that designs the
# DEEP-CLONE VARIANT of the Aug 10 streaming-atomic JSON Patch
# wrapper:
#
#   psp::json_patch::parse_and_apply_atomic_streaming_deep_clone(
#       JsonValue& root, psp::Span<const char>& doc)
#       -> std::expected<std::size_t, JsonPatchError>
#
# The deep-clone variant mirrors the Aug 10 streaming wrapper in
# observable contract (success: applied count; failure: pre-state
# restored, error surfaced; cursor state matches Aug 10) but uses a
# different rollback mechanism: capture a deep clone of root ONCE
# up-front; on any failure, restore root = pre_clone and return
# the error. The inverse-journal variant (Aug 10) is cheaper for
# small-tree + small-patch; the deep-clone variant is cheaper for
# big-tree + many-failed-ops (no per-op inverse computation, no
# journal growth, no replay pass).
#
# Where this fits in the arc
# --------------------------
#   - Aug 3: patch_atomic + patch_dry_run    (DEEP-CLONE wrapper;
#                                              in-memory; mirror)
#   - Aug 5: patch_journaled                 (INVERSE-JOURNAL
#                                              wrapper; in-memory;
#                                              mirror)
#   - Aug 10: parse_and_apply_atomic_streaming (INVERSE-JOURNAL
#                                               streaming wrapper)
#   - Aug 11: parse_and_apply_atomic_streaming_deep_clone
#           TODAY  (DEEP-CLONE streaming wrapper; closes the
#                   "deep-clone variant of the streaming wrapper"
#                   item from the Aug 10 "What's NOT in this
#                   lesson" section)
#
# Why consumer-side and not library-side today
# --------------------------------------------
# Same shape as Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9 / Aug 10:
# a proven-in-consumer capability that exercises the design
# end-to-end. Library version is unchanged at v0.15.0; future
# v0.16.0 promotion is mechanical (lift
# parse_and_apply_atomic_streaming_deep_clone + the detail::
# helpers into <psp_span/json_ext.h>; bump the version).
#
# What the consumer exercises
# ----------------------------
#   Section 1 — symbol-presence + the deep-clone streaming
#               wrapper signature.
#   Section 2 — single-op document round-trip through
#               parse_and_apply_atomic_streaming_deep_clone.
#   Section 3 — multi-op document with one failing op
#               (deep-clone restore; pre-state preserved).
#   Section 4 — parse failure mid-stream (cursor rewind; root
#               unchanged; deep-clone restore not needed since
#               no op was applied).
#   Section 5 — end-to-end: deep-clone streaming tree ==
#               inverse-journal streaming tree ==
#               in-memory patch_journaled_self_move_safe tree
#               (observably equivalent on the success path).
#   Section 6 — sizeof / feature probes; design invariants.
#
#   ~30 cases across 6 sections, all expected to pass.
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-11-streaming-atomic-deep-clone
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-11-streaming-atomic-deep-clone
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-11-streaming-atomic-deep-clone

cmake_minimum_required(VERSION 3.16)
project(streaming_atomic_deep_clone_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that designs the DEEP-CLONE variant of the Aug 10 streaming-atomic JSON Patch wrapper (psp::json_patch::parse_and_apply_atomic_streaming_deep_clone as a consumer-side composition). Same observable contract as the Aug 10 inverse-journal variant; rollback is by restoring a pre-cloned root snapshot instead of replaying an inverse journal. Library version unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.12.0 engine + v0.11.0 pointer
# + the consumer-side wrappers, mirroring the Aug 10 lesson. The
# deep-clone streaming wrapper itself only needs the v0.12.0 engine
# + v0.11.0 pointer, but pinning 0.15 keeps the consumer consistent
# with the Aug 3/4/5/6/9/10 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-11-streaming-atomic-deep-clone
    P-2026-08-11-streaming-atomic-deep-clone.cpp
)

target_link_libraries(P-2026-08-11-streaming-atomic-deep-clone
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, etc.
set_target_properties(P-2026-08-11-streaming-atomic-deep-clone PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-11-streaming-atomic-deep-clone PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-11-streaming-atomic-deep-clone PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
