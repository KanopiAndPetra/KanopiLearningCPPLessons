# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that designs the
# STREAMING-ATOMIC JSON Patch wrapper composing the Aug 4 streaming
# parser + the Aug 9 journal-aware self-move-safe transactional layer
# into a single function:
#
#   psp::json_patch::parse_and_apply_atomic_streaming(
#       JsonValue& root, psp::Span<const char>& doc)
#       -> std::expected<std::size_t, JsonPatchError>
#
# The streaming-atomic composition is the natural next step from the
# Aug 4 streaming parser + the Aug 9 transactional layer. The Aug 4
# lesson's "Where we go next" section and the Aug 9 lesson's "Where
# we go next" section both explicitly flagged this composition as
# forward-on work.
#
# Where this fits in the arc
# --------------------------
#   - Aug 4: parse_patch_document_at / parse_patch_document_next_at /
#            parse_one_op_at (streaming wire-format parser, cursor-
#            primitive).
#   - Aug 9: patch_journaled_self_move_safe (inverse-journal rollback
#            + self-move filter at both the input AND the journal
#            boundaries; the most complete transactional layer shipped
#            to date).
#   - Aug 10: parse_and_apply_atomic_streaming (today; composes
#            streaming parse + atomic apply in a single pass; no
#            intermediate std::vector<JsonPatchOp> allocation).
#
# Why consumer-side and not library-side today
# --------------------------------------------
# Same shape as Aug 3 / Aug 4 / Aug 5 / Aug 6 / Aug 9: a proven-in-
# consumer capability that exercises the design end-to-end. Library
# version is unchanged at v0.15.0; future v0.16.0 promotion is
# mechanical (lift parse_and_apply_atomic_streaming + the detail::
# helpers into <psp_span/json_ext.h>; bump the version).
#
# What the consumer exercises
# ----------------------------
#   Section 1 — symbol-presence + the per-op streaming parser
#               surface (mirrors Aug 4).
#   Section 2 — single-op document round-trip through
#               parse_and_apply_atomic_streaming.
#   Section 3 — multi-op document with one failing op (mid-stream
#               rollback; pre-state preserved).
#   Section 4 — multi-op document with a parse failure mid-stream
#               (cursor at the failure point; root unchanged; cursor
#               rewind verified).
#   Section 5 — end-to-end: streaming-atomic produces the same
#               final tree as the in-memory
#               patch_journaled_self_move_safe wrapper.
#   Section 6 — sizeof / feature probes.
#
#   ~30+ cases across 6 sections, all expected to pass.
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-10-streaming-atomic-patch
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-10-streaming-atomic-patch
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-10-streaming-atomic-patch

cmake_minimum_required(VERSION 3.16)
project(streaming_atomic_patch_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that designs the STREAMING-ATOMIC JSON Patch wrapper (psp::json_patch::parse_and_apply_atomic_streaming as a consumer-side composition of the Aug 4 streaming parser + Aug 9 journal-aware self-move-safe wrapper). Streams a wire-format patch document and applies each op transactionally in a single pass; no intermediate std::vector<JsonPatchOp> allocation. Library version unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.12.0 engine + v0.13.0 parser
# + v0.15.0 writer + the v0.11.0 pointer, mirroring the Aug 9
# lesson. The streaming wrapper itself only needs the v0.12.0
# engine + v0.11.0 pointer, but pinning 0.15 keeps the consumer
# consistent with the Aug 2/3/4/5/6/9 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-10-streaming-atomic-patch
    P-2026-08-10-streaming-atomic-patch.cpp
)

target_link_libraries(P-2026-08-10-streaming-atomic-patch
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, etc.
set_target_properties(P-2026-08-10-streaming-atomic-patch PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-10-streaming-atomic-patch PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-10-streaming-atomic-patch PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
