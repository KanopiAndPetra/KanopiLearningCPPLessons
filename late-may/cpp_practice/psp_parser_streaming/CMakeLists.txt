# CMakeLists.txt — consumer of psp_span_lib's new streaming cursor API.
#
# This is the consumer for the Jul 15 lesson
# (P-2026-07-15-psp-parser-streaming-cursor.cpp). It exercises the three
# new streaming primitives in <psp_span/parser.h>:
#
#   - psp::parse_int_at(Span<const char>&)
#   - psp::parse_uint_at(Span<const char>&)
#   - psp::parse_double_at(Span<const char>&)
#
# Mirror of the Jul 14 lesson's consumer
# (psp_parser_header/CMakeLists.txt) — the only difference is the
# version requirement bumped from 0.6 to 0.7 because the streaming
# cursor API is new in 0.7.0.
#
# Usage (assumes psp_span_lib is already installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-15-psp-parser-streaming-cursor
#
# ASan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-07-15-psp-parser-streaming-cursor

cmake_minimum_required(VERSION 3.16)
project(psp_parser_streaming_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib's streaming cursor parser API (parse_int_at / parse_uint_at / parse_double_at) — exercises find_package() against v0.7.0."
    LANGUAGES CXX
)

# Version bump from 0.6 (Jul 14, whole-span) to 0.7 (Jul 15, streaming).
# Same SameMajorVersion compat policy the library has had since v0.1,
# so any 0.x find_package() call satisfies this — but pinning to 0.7
# here proves the new API exists in the installed tree.
find_package(psp_span_lib 0.7 REQUIRED)

add_executable(P-2026-07-15-psp-parser-streaming-cursor
    P-2026-07-15-psp-parser-streaming-cursor.cpp
)

target_link_libraries(P-2026-07-15-psp-parser-streaming-cursor
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-15-psp-parser-streaming-cursor PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-15-psp-parser-streaming-cursor PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()