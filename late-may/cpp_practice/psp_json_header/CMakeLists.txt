# CMakeLists.txt — consumer of psp_span_lib v0.10.0's new
# <psp_span/json.h> header (the third public header of the
# library, added 2026-07-20 alongside the v0.9.0 -> v0.10.0
# version bump and the new typed ParseError::DuplicateKey).
#
# This is the consumer for the Jul 20 lesson
# (P-2026-07-20-psp-json-header.cpp). It exercises:
#
#   - psp::parse_value_at / psp::parse_array_at / psp::parse_object_at
#   - psp::json_to_string
#   - psp::JsonValue
#   - the typed ParseError::DuplicateKey (new with this version)
#
# All of those come from the new header. The consumer has zero
# parser code of its own — it is a thin test driver for the
# header-owned parser. Compare with the Jul 19 consumer
# (P-2026-07-19-psp-json-arrays-nested.cpp) which had ~250
# lines of parser code in the .cpp file.
#
# Mirror of the Jul 19 consumer CMakeLists.txt — same find_package
# pattern. We require EXACTLY 0.10 (not "0.9 or newer") because
# the consumer depends on the <psp_span/json.h> header AND on
# ParseError::DuplicateKey, both of which landed in v0.10.0.
#
# Usage (assumes psp_span_lib v0.10.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-20-psp-json-header
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-20-psp-json-header

cmake_minimum_required(VERSION 3.16)
project(psp_json_header_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.10.0's new <psp_span/json.h> header — a thin test driver for the library-owned complete general JSON parser; no parser code in the consumer."
    LANGUAGES CXX
)

# Require >= 0.10 (not >= 0.9) because the consumer depends on the
# <psp_span/json.h> header AND on ParseError::DuplicateKey — both
# of which shipped in v0.10.0. v0.9.0 lacks both.
find_package(psp_span_lib 0.10 REQUIRED)

add_executable(P-2026-07-20-psp-json-header
    P-2026-07-20-psp-json-header.cpp
)

target_link_libraries(P-2026-07-20-psp-json-header
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-20-psp-json-header PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-20-psp-json-header PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
