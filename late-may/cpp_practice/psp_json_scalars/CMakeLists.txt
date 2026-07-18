# CMakeLists.txt — consumer of psp_span_lib's JSON scalar token
# cursors (parse_string_at, parse_bool_at, parse_null_at), shipped
# in v0.9.0.
#
# This is the consumer for the Jul 18 lesson
# (P-2026-07-18-psp-json-scalars.cpp). It exercises:
#
#   - psp::parse_string_at(Span<const char>&)
#   - psp::parse_bool_at(Span<const char>&)
#   - psp::parse_null_at(Span<const char>&)
# plus the new ParseError variants UnterminatedString,
# InvalidEscape, InvalidUnicodeEscape, and InvalidLiteral.
#
# Mirror of the Jul 15 / Jul 16 consumer CMakeLists.txt — same
# find_package() pattern, version requirement bumped from 0.8
# (Jul 16) to 0.9 (Jul 18).
#
# Usage (assumes psp_span_lib v0.9.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-18-psp-json-scalars
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-18-psp-json-scalars

cmake_minimum_required(VERSION 3.16)
project(psp_json_scalars_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib's JSON scalar token cursors (parse_string_at, parse_bool_at, parse_null_at) — exercises find_package() against v0.9.0."
    LANGUAGES CXX
)

# Version bump from 0.8 (Jul 16, non-numeric cursor primitives) to
# 0.9 (Jul 18, JSON scalar token cursors). Same SameMajorVersion
# compat policy the library has had since v0.1, so any 0.x
# find_package() call satisfies this — but pinning to 0.9 here
# proves the new scalar API exists in the installed tree.
find_package(psp_span_lib 0.9 REQUIRED)

add_executable(P-2026-07-18-psp-json-scalars
    P-2026-07-18-psp-json-scalars.cpp
)

target_link_libraries(P-2026-07-18-psp-json-scalars
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-18-psp-json-scalars PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-18-psp-json-scalars PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
