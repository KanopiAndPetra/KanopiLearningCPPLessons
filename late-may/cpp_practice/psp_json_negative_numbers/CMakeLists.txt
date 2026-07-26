# CMakeLists.txt — consumer of psp_span_lib v0.13.0 that exercises the
# two pre-existing parser limitations the Jul 24 lesson flagged in
# its "Two real findings" section:
#
#   1. parse_int / parse_double / parse_int_at / parse_double_at
#      reject a leading '-' (ParseError::LeadingSign).
#
#   2. parse_int / parse_double / parse_int_at / parse_double_at
#      overflow at INT_MAX (~2.147e9), not INT64_MAX, even though
#      the JsonValue sum type's int alternative is std::int64_t.
#
# Today's lesson does NOT bump the library. It exercises both
# findings in a consumer and PROVES the proposed fix (a 12-line
# change to <psp_span/parser.h>) works end-to-end through:
#
#   - parse_value_at (the JSON dispatcher)
#   - json_to_string  (the pretty-printer)
#   - psp::json_patch::patch  (the RFC 6902 engine — uses
#     deep-equality on the parsed values, so INT64_MAX must
#     round-trip bit-exact through a TestOp)
#   - psp::json_patch::parse_patch_document / serialise_patch_doc.
#     (the v0.13.0 parser + the Jul 24 consumer writer)
#
# The fix itself (parser.h: drop the leading-'-' rejection, swap
# INT_MAX -> INT64_MAX) is a 4-place edit; this consumer proves
# the fix works against the library as a whole BEFORE we ship it
# as v0.14.0. The fix is shipped as LOCAL COPIES of the four
# parsers in the consumer (parse_int_v014, parse_double_v014,
# parse_int_at_v014, parse_double_at_v014) and the local copies
# are linked into a SHADOW dispatcher (parse_value_at_v014).
# The shadow dispatcher is what the consumer exercises — NOT the
# library's parse_value_at. This way the same executable exercises
# both the v0.13.0 (broken) behaviour and the proposed v0.14.0
# (fixed) behaviour, side by side, and proves they differ in
# exactly the two findings' worth of cases.
#
# Why a consumer and not a library upgrade?
# -----------------------------------------
# Same pattern as the Jul 24 writer lesson: prove the design end-
# to-end in a consumer first, then ship the v0.14.0 promotion as
# a focused library upgrade once the consumer's verification is
# done. The Jul 24 lesson explicitly flagged both findings as
# candidates for the next lesson to touch the parser; this lesson
# is that lesson.
#
# Usage (assumes psp_span_lib v0.13.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-25-psp-json-negative-numbers
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-25-psp-json-negative-numbers

cmake_minimum_required(VERSION 3.16)
project(psp_json_negative_numbers_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.13.0 that demonstrates the negative-number + INT64_MAX-overflow findings the Jul 24 lesson flagged, and proves a v0.14.0-shaped local fix to parse_int / parse_double / parse_int_at / parse_double_at works end-to-end through parse_value_at, json_to_string, psp::json_patch::patch, psp::json_patch::parse_patch_document, and the Jul 24 consumer's serialise_patch_document."
    LANGUAGES CXX
)

# Require >= 0.13 because the consumer exercises psp::json_patch::patch
# (v0.12.0) and psp::json_patch::parse_patch_document (v0.13.0) for
# the round-trip test of the negative-number findings. The local
# v0.14.0-shaped parser copies are in the consumer itself; no library
# changes are required.
find_package(psp_span_lib 0.13 REQUIRED)

add_executable(P-2026-07-25-psp-json-negative-numbers
    P-2026-07-25-psp-json-negative-numbers.cpp
)

target_link_libraries(P-2026-07-25-psp-json-negative-numbers
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-25-psp-json-negative-numbers PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-25-psp-json-negative-numbers PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
