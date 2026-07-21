# CMakeLists.txt — consumer of psp_span_lib v0.11.0's new
# <psp_span/json_ext.h> header (the fourth public header of the
# library, added 2026-07-21 alongside the v0.10.0 -> v0.11.0
# version bump).
#
# This is the consumer for the Jul 21 lesson
# (P-2026-07-21-psp-json-pointer.cpp). It exercises:
#
#   - psp::json_pointer::split     (RFC 6901 §3 tokenizer)
#   - psp::json_pointer::to_string (inverse of split)
#   - psp::json_pointer::resolve   (JSON Pointer lookup)
#   - ::JsonExtError                (8 enumerators)
#   - The std::formatter<JsonExtError> specialisation
#
# All of those come from the new header. The consumer has zero
# pointer-code of its own — it is a thin test driver for the
# header-owned resolver. (It does use the v0.10.0
# psp::parse_value_at to build the input trees; that's parser
# code, not pointer code.)
#
# We require EXACTLY 0.11 (not "0.10 or newer") because the
# consumer depends on the <psp_span/json_ext.h> header AND on
# ::JsonExtError, both of which landed in v0.11.0.
#
# Usage (assumes psp_span_lib v0.11.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-21-psp-json-pointer
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-21-psp-json-pointer

cmake_minimum_required(VERSION 3.16)
project(psp_json_pointer_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.11.0's new <psp_span/json_ext.h> header — a thin test driver for the library-owned JSON Pointer (RFC 6901) resolver; no pointer-code in the consumer."
    LANGUAGES CXX
)

# Require >= 0.11 (not >= 0.10) because the consumer depends on the
# <psp_span/json_ext.h> header AND on ::JsonExtError — both of
# which shipped in v0.11.0. v0.10.0 lacks both.
find_package(psp_span_lib 0.11 REQUIRED)

add_executable(P-2026-07-21-psp-json-pointer
    P-2026-07-21-psp-json-pointer.cpp
)

target_link_libraries(P-2026-07-21-psp-json-pointer
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-21-psp-json-pointer PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-21-psp-json-pointer PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
