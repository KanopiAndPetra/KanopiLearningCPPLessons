# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that
# exercises the freshly-promoted
# psp::json_patch::serialise_patch_document (the RFC 6902 §3
# wire-format writer) in the LIBRARY PROPER.
#
# This is the consumer for today's lesson
# (P-2026-08-02-psp-json-patch-writer-v015.cpp). It exercises:
#
#   - psp::json_patch::serialise_patch_document(span<JsonPatchOp>)
#       -> std::string  (RFC 6902 §3 wire-format text)
#       — pulled from <psp_span/json_ext.h>, NOT a local copy
#   - The full ops round-trip through the library's own
#     writer + parser (parse_patch_document from v0.13.0) +
#     re-serialise produces a fixed point
#   - Every JsonPatchOp kind (add/remove/replace/move/copy/test)
#     serialised via the library function
#   - Every JsonValue alternative (null/bool/int/double/string/
#     array/object/nested) flowing through a serialised "value"
#     field
#   - The full patch pipeline: build ops -> serialise -> parse ->
#     patch -> json_to_string must produce the same mutated tree
#     as direct build -> patch
#   - Empty documents (serialise "[]" -> parse -> empty vector)
#   - Interop with v0.14.0 sign-accepted values: a serialised
#     wire with a leading '-' or INT64_MAX-shaped int parses
#     cleanly through the same library the writer is in
#   - Symbol-presence: the function resolves through
#     find_package() (i.e. it's a real library symbol, not a
#     local TU inline)
#
# Compared to the Jul 24 consumer
# (P-2026-07-24-psp-json-patch-serialiser.cpp), today's consumer
# drops the LOCAL copy of serialise_patch_document and pulls the
# function from the library header. The function is now
# library-proper.
#
# Usage (assumes psp_span_lib v0.15.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-02-psp-json-patch-writer-v015
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-02-psp-json-patch-writer-v015

cmake_minimum_required(VERSION 3.16)
project(psp_json_patch_writer_v015_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 exercising psp::json_patch::serialise_patch_document (the RFC 6902 §3 wire-format writer) from the library proper; round-trip writer + parser through the SAME library; full ops + value + pipeline coverage; symbol-presence probe confirms the writer is library-proper (not a local copy)."
    LANGUAGES CXX
)

# Require >= 0.15 — today's lesson depends on the writer
# living in the library proper. The parser (v0.13.0) and the
# sign acceptance (v0.14.0) are both subsets, so requiring
# 0.15 is the right floor.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-02-psp-json-patch-writer-v015
    P-2026-08-02-psp-json-patch-writer-v015.cpp
)

target_link_libraries(P-2026-08-02-psp-json-patch-writer-v015
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-02-psp-json-patch-writer-v015 PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-02-psp-json-patch-writer-v015 PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
