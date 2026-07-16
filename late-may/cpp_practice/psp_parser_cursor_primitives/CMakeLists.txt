# CMakeLists.txt — consumer of psp_span_lib's new cursor primitives
# (expect_char_at, skip_whitespace_at), shipped in v0.8.0.
#
# This is the consumer for the Jul 16 lesson
# (P-2026-07-16-psp-parser-cursor-primitives.cpp). It exercises the
# two new non-numeric streaming primitives in <psp_span/parser.h>:
#
#   - psp::expect_char_at(Span<const char>&, char)
#   - psp::skip_whitespace_at(Span<const char>&)
#
# Mirror of the Jul 15 lesson's consumer (psp_parser_streaming/) —
# same shape, same find_package() pattern, version requirement bumped
# from 0.7 (Jul 15, numeric streaming parsers) to 0.8 (Jul 16,
# non-numeric cursor primitives).
#
# Usage (assumes psp_span_lib is already installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-16-psp-parser-cursor-primitives
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-16-psp-parser-cursor-primitives

cmake_minimum_required(VERSION 3.16)
project(psp_parser_cursor_primitives_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib's non-numeric cursor primitives (expect_char_at, skip_whitespace_at) — exercises find_package() against v0.8.0."
    LANGUAGES CXX
)

# Version bump from 0.7 (Jul 15, numeric streaming parsers) to 0.8
# (Jul 16, non-numeric cursor primitives). Same SameMajorVersion
# compat policy the library has had since v0.1, so any 0.x
# find_package() call satisfies this — but pinning to 0.8 here
# proves the new cursor API exists in the installed tree.
find_package(psp_span_lib 0.8 REQUIRED)

add_executable(P-2026-07-16-psp-parser-cursor-primitives
    P-2026-07-16-psp-parser-cursor-primitives.cpp
)

target_link_libraries(P-2026-07-16-psp-parser-cursor-primitives
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-16-psp-parser-cursor-primitives PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-16-psp-parser-cursor-primitives PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()