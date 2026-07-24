# CMakeLists.txt — consumer of psp_span_lib v0.13.0 that
# implements and exercises psp::json_patch::serialise_patch_document,
# the RFC 6902 §3 wire-format WRITER (mirror image of the v0.13.0
# parser from the Jul 23 lesson).
#
# This is the consumer for the Jul 24 lesson
# (P-2026-07-24-psp-json-patch-serialiser.cpp). It exercises:
#
#   - psp::json_patch::serialise_patch_document(span<JsonPatchOp>)
#       -> std::string  (RFC 6902 §3 wire-format text)
#   - The full ops round-trip:
#       build ops -> serialise -> parse (via v0.13.0) -> serialise
#       must produce a fixed point
#   - Every JsonPatchOp kind (add/remove/replace/move/copy/test)
#     serialized then re-parsed
#   - Every JsonValue alternative (null/bool/int/double/string/array/
#     object/nested) round-tripping through a serialised value
#     field
#   - Empty documents (serialise "[]" -> parse -> empty vector)
#   - The patch-then-serialise pipeline: apply ops -> serialise
#     ops -> re-apply from serialised form -> same mutated tree
#
# The writer itself is implemented locally in the consumer file
# (NOT in <psp_span/json_ext.h>) — same pattern as the Jul 22
# Patch consumer. The writer is a candidate for a future v0.14.0
# library upgrade; for today the consumer exercises the design
# end-to-end and validates the round-trip.
#
# Usage (assumes psp_span_lib v0.13.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-24-psp-json-patch-serialiser
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-24-psp-json-patch-serialiser

cmake_minimum_required(VERSION 3.16)
project(psp_json_patch_serialiser_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.13.0 implementing psp::json_patch::serialise_patch_document (RFC 6902 §3 wire-format writer); round-trip writer + parser; thin test driver for the writer design."
    LANGUAGES CXX
)

# Require >= 0.13 (not >= 0.12) because the consumer depends on
# psp::json_patch::parse_patch_document (the v0.13.0 parser half
# the writer is round-tripping against). The writer itself is
# local to the consumer, so no library API is required for it —
# only the parser half.
find_package(psp_span_lib 0.13 REQUIRED)

add_executable(P-2026-07-24-psp-json-patch-serialiser
    P-2026-07-24-psp-json-patch-serialiser.cpp
)

target_link_libraries(P-2026-07-24-psp-json-patch-serialiser
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-24-psp-json-patch-serialiser PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-24-psp-json-patch-serialiser PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()