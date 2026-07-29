# CMakeLists.txt — consumer of psp_span_lib v0.14.0 that EXERCISES the
# v0.14.0 promotion directly (no local parser copies; uses the library
# proper). The Jul 25 lesson (P-2026-07-25-psp-json-negative-numbers.cpp)
# proved the design end-to-end against v0.13.0 using LOCAL COPIES of the
# four parsers in a `v014` namespace; today's lesson uses the LIBRARY's
# own parse_int / parse_double / parse_int_at / parse_double_at /
# parse_value_at and confirms:
#
#   1. The four parsers accept a leading '+' or '-' (was LeadingSign
#      in v0.13.0; now NotADigit for bare sign, accepted-with-digits
#      otherwise).
#   2. parse_int / parse_int_at return std::int64_t (was int).
#   3. The full JSON pipeline (parse_value_at -> json_to_string ->
#      parse -> ==) round-trips negative + INT64_MAX-shaped values
#      bit-exact through the LIBRARY proper (no shadow dispatcher).
#   4. psp::json_patch::patch (the RFC 6902 engine) uses
#      deep-equality on negative + INT64_MAX-shaped values, so a
#      TestOp with INT64_MAX compares equal to a hand-built
#      INT64_MAX target. (This exercises the v0.13.0 engine
#      against the v0.14.0 parser chain.)
#   5. The Jul 25 local `v014` consumer's proofs all hold against
#      the library proper.
#
# The promotion is purely a library change; this consumer is
# structurally simpler than the Jul 25 one (no v014 namespace, no
# shadow dispatcher, no consumer_writer re-inline).
#
# Usage (assumes psp_span_lib v0.14.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-27-psp-json-v014-promotion
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
#   ./build-asan/P-2026-07-27-psp-json-v014-promotion

cmake_minimum_required(VERSION 3.16)
project(psp_json_v014_promotion_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.14.0 (the SIGNED-NUMERIC-LITERALS promotion). Exercises parse_int / parse_double / parse_int_at / parse_double_at directly through the LIBRARY proper (no local copies), and verifies that the JSON pipeline + JSON Patch engine handle negative + INT64_MAX-shaped values cleanly."
    LANGUAGES CXX
)

# Require >= 0.14 because we depend on the widened parse_int return
# type (std::int64_t) and the optional-sign acceptance in all four
# numeric parsers.
find_package(psp_span_lib 0.14 REQUIRED)

add_executable(P-2026-07-27-psp-json-v014-promotion
    P-2026-07-27-psp-json-v014-promotion.cpp
)

target_link_libraries(P-2026-07-27-psp-json-v014-promotion
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-27-psp-json-v014-promotion PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-27-psp-json-v014-promotion PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()