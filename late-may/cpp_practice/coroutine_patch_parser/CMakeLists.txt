# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that adds today's
# P-2026-08-17 lesson:
#
#   psp::json_patch::parse_patch_ops(span)
#       -> generator<JsonPatchOp>
#
#   (a hand-rolled coroutine generator that wraps the Aug 4
#    streaming JSON Patch parser; the bridge is the C++20
#    <coroutine> header because <generator> (P2502R2) is NOT
#    in this Apple Clang 21.0.0 toolchain)
#
# Where this fits in the arc
# --------------------------
#   - Aug  4: psp::json_patch::parse_patch_      CURSOR-PRIMITIVE
#             document_at + parse_patch_document_  STREAMING PARSER
#             next_at + parse_one_op_at            (Begin/Next split)
#   - Aug 10: parse_and_apply_atomic_streaming   INVERSE-JOURNAL
#                                              STREAMING (Begin+Next loop)
#   - Aug 11: parse_and_apply_atomic_streaming_  DEEP-CLONE STREAMING
#             deep_clone                        (Begin+Next loop)
#   - Aug 12: psp::json_schema::validate        JSON SCHEMA
#                                              VALIDATION
#   - Aug 13: psp::json_schema::validate_atomic  SCHEMA-VALIDATED
#                                              ATOMIC UPDATE
#   - Aug 14: parse_and_apply_atomic_streaming_  STREAMING + SCHEMA
#             validated                          VALIDATED
#   - Aug 15: psp::json_pointer::               READ-WITH-VALIDATION
#             resolve_with_validation
#   - Aug 17: generator<JsonPatchOp> (hand-    HAND-ROLLED COROUTINE
#     TODAY   rolled; <generator> not yet in      GENERATOR ADAPTER
#             this Apple Clang 21.0.0)            ON TOP OF THE AUG 4
#                                               BEGIN/NEXT SPLIT
#                                               (closes the "std::
#                                               generator adapter on
#                                               top of the Aug 4
#                                               begin/next functions,
#                                               waiting on <generator>
#                                               in the Apple Clang
#                                               toolchain" forward-on
#                                               item the Aug 4 lesson
#                                               explicitly named)
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-17-coroutine-patch-parser
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-17-coroutine-patch-parser
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-17-coroutine-patch-parser

cmake_minimum_required(VERSION 3.16)
project(coroutine_patch_parser_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that adds today's P-2026-08-17 lesson: a hand-rolled coroutine generator<JsonPatchOp> on top of the Aug 4 streaming JSON Patch parser. Closes the 'std::generator adapter on top of the Aug 4 begin/next functions, waiting on <generator> in the Apple Clang toolchain' forward-on item by working around the missing <generator> header with a C++20 <coroutine>-based template that mirrors the P2502R2 std::generator<T> surface."
    LANGUAGES CXX
)

find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-17-coroutine-patch-parser
    P-2026-08-17-coroutine-patch-parser.cpp
)

target_link_libraries(P-2026-08-17-coroutine-patch-parser
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, std::span (and the
# library's std::expected usage). The generator itself uses
# <coroutine> which is C++20.
set_target_properties(P-2026-08-17-coroutine-patch-parser PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-17-coroutine-patch-parser PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-17-coroutine-patch-parser PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
