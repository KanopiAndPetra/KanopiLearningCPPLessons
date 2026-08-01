# CMakeLists.txt — v0.14.0 consumer update verification.
#
# This is a small standalone consumer that re-verifies the v0.14.0
# breaking-change contract by calling the library proper. The two
# pre-existing consumers (psp_parser_header Jul 14, psp_parser_streaming
# Jul 15) were updated in-place on Aug 01 to satisfy the new contract;
# today's consumer re-states the contract in a single canonical
# PASS/FAIL table for the lesson's verification output.
#
# Usage (assumes psp_span_lib v0.14.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-01-psp-parser-v014-update
#
# Strict build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-01-psp-parser-v014-update
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-01-psp-parser-v014-update

cmake_minimum_required(VERSION 3.16)
project(psp_parser_v014_update_consumer
    VERSION 0.1.0
    DESCRIPTION "v0.14.0 consumer update verification — re-states the parse_int / parse_int_at / parse_double / parse_double_at v0.14.0 contract (signed literals + INT64 widening) in a single PASS/FAIL table."
    LANGUAGES CXX
)

# v0.14.0 is the version that introduced signed numeric literals +
# INT64 widening. The consumer requires at least v0.14.0; v0.13.0
# would fail every section.
find_package(psp_span_lib 0.14 REQUIRED)

add_executable(P-2026-08-01-psp-parser-v014-update
    P-2026-08-01-psp-parser-v014-update.cpp
)

target_link_libraries(P-2026-08-01-psp-parser-v014-update
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-01-psp-parser-v014-update PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-01-psp-parser-v014-update PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
