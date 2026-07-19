# CMakeLists.txt — consumer of psp_span_lib's cursor primitives
# (v0.9.0) demonstrating a complete general JSON parser with
# arrays + nested objects.
#
# This is the consumer for the Jul 19 lesson
# (P-2026-07-19-psp-json-arrays-nested.cpp). It exercises:
#
#   - psp::expect_char_at, psp::skip_whitespace_at
#   - psp::parse_string_at, parse_bool_at, parse_null_at
#   - psp::parse_int_at, parse_uint_at, parse_double_at
# together as a single recursive-descent JSON parser. No new
# library functions are required: the cursor primitives shipped
# in v0.9.0 are exactly sufficient.
#
# Mirror of the Jul 15 / Jul 16 / Jul 18 consumer CMakeLists.txt
# — same find_package() pattern. We require EXACTLY 0.9 (not
# "0.9 or newer") because we depend on the v0.9.0 cursor
# primitives — if the install tree were v0.8 (no parse_string_at),
# this consumer would fail to link.
#
# Usage (assumes psp_span_lib v0.9.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-19-psp-json-arrays-nested
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-19-psp-json-arrays-nested

cmake_minimum_required(VERSION 3.16)
project(psp_json_arrays_nested_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib's v0.9.0 cursor primitives — a complete general JSON parser with arrays + nested objects built on the existing library API (no new library code required)."
    LANGUAGES CXX
)

# Require >= 0.9 (not >= 0.8) because the consumer depends on the
# JSON-scalar-token primitives (parse_string_at, parse_bool_at,
# parse_null_at) that landed in v0.9.0. v0.8 does not have those
# primitives — the static_assert / link step would fail with a
# much less helpful error. SameMajorVersion compat lets the
# existing install tree (v0.9.0) satisfy ">= 0.9".
find_package(psp_span_lib 0.9 REQUIRED)

add_executable(P-2026-07-19-psp-json-arrays-nested
    P-2026-07-19-psp-json-arrays-nested.cpp
)

target_link_libraries(P-2026-07-19-psp-json-arrays-nested
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-19-psp-json-arrays-nested PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-19-psp-json-arrays-nested PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()