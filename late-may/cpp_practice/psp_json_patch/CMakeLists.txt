# CMakeLists.txt — consumer of psp_span_lib v0.12.0's upgraded
# <psp_span/json_ext.h> header (the fourth public header of the
# library, upgraded 2026-07-22 with the JSON Patch — RFC 6902 —
# half).
#
# This is the consumer for the Jul 22 lesson
# (P-2026-07-22-psp-json-patch.cpp). It exercises:
#
#   - psp::json_patch::patch(JsonValue&, ops)
#   - The six RFC 6902 ops (add / remove / replace / move / copy / test)
#   - ::JsonPatchOp (a tagged-union sum type per RFC 6902 §4)
#   - ::JsonPatchError (the typed failure payload)
#   - The "-" syntax for array append
#   - The std::formatter<JsonPatchError> specialisation
#
# All of those come from the upgraded header. The consumer has
# zero patch-code of its own — it is a thin test driver for the
# header-owned patch engine. (It does use the v0.10.0
# psp::parse_value_at to build the input trees + RFC 6901
# resolve to spot-check mutations; both are parser/pointer
# code, not patch code.)
#
# We require EXACTLY 0.12 (not "0.11 or newer") because the
# consumer depends on the JSON Patch function
# psp::json_patch::patch AND on ::JsonPatchError, both of
# which landed in v0.12.0.
#
# Usage (assumes psp_span_lib v0.12.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-22-psp-json-patch
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-22-psp-json-patch

cmake_minimum_required(VERSION 3.16)
project(psp_json_patch_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.12.0's upgraded <psp_span/json_ext.h> header — a thin test driver for the library-owned JSON Patch (RFC 6902) engine; no patch-code in the consumer."
    LANGUAGES CXX
)

# Require >= 0.12 (not >= 0.11) because the consumer depends on
# psp::json_patch::patch AND on ::JsonPatchError — both of which
# shipped in v0.12.0. v0.11.0 has the Pointer half only.
find_package(psp_span_lib 0.12 REQUIRED)

add_executable(P-2026-07-22-psp-json-patch
    P-2026-07-22-psp-json-patch.cpp
)

target_link_libraries(P-2026-07-22-psp-json-patch
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-22-psp-json-patch PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-22-psp-json-patch PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
