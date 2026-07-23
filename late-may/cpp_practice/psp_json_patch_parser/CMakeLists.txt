# CMakeLists.txt — consumer of psp_span_lib v0.13.0's new
# psp::json_patch::parse_patch_document (RFC 6902 §3 wire-format
# parser).
#
# This is the consumer for the Jul 23 lesson
# (P-2026-07-23-psp-json-patch-parser.cpp). It exercises:
#
#   - psp::json_patch::parse_patch_document(string_view)
#   - The full round-trip: parse_patch_document -> std::vector<JsonPatchOp>
#     -> psp::json_patch::patch(JsonValue&, ops) -> mutated tree
#   - The three new typed errors (BadDocument, MissingField, WrongType)
#   - All six op shapes (add/remove/replace/move/copy/test) parsed
#     from wire format
#   - The "ignore unknown members" rule (RFC 6902 §3)
#   - Empty documents (parse OK + apply as no-op)
#
# All of those come from the upgraded header. The consumer has
# zero patch-code of its own — it is a thin test driver for the
# header-owned RFC 6902 §3 parser.
#
# We require EXACTLY 0.13 (not "0.12 or newer") because the consumer
# depends on psp::json_patch::parse_patch_document AND on the three
# new JsonPatchError enumerators (BadDocument, MissingField,
# WrongType), all of which landed in v0.13.0.
#
# Usage (assumes psp_span_lib v0.13.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-23-psp-json-patch-parser
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-23-psp-json-patch-parser

cmake_minimum_required(VERSION 3.16)
project(psp_json_patch_parser_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.13.0's psp::json_patch::parse_patch_document (RFC 6902 §3 wire-format parser); round-trip parser + patch engine; thin test driver."
    LANGUAGES CXX
)

# Require >= 0.13 (not >= 0.12) because the consumer depends on
# psp::json_patch::parse_patch_document AND on the three new
# JsonPatchError enumerators (BadDocument, MissingField, WrongType),
# all of which shipped in v0.13.0.
find_package(psp_span_lib 0.13 REQUIRED)

add_executable(P-2026-07-23-psp-json-patch-parser
    P-2026-07-23-psp-json-patch-parser.cpp
)

target_link_libraries(P-2026-07-23-psp-json-patch-parser
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-23-psp-json-patch-parser PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-23-psp-json-patch-parser PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()