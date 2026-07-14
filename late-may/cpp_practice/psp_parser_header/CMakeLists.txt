# CMakeLists.txt — consumer of psp_span_lib via find_package.
#
# This is the "real consumer" pattern that the Jul 13 lesson noted as
# missing in its "What I didn't cover" section. Today's lesson (Jul 14)
# closes that loop: a CMakeLists.txt that does NOT hard-code the
# /tmp/psp_install path, but instead uses find_package() so the
# consumer can be relocated anywhere on disk without source edits.
#
# Usage (from a fresh shell — assumes psp_span_lib is already installed
# at /tmp/psp_install OR you set CMAKE_PREFIX_PATH to wherever you put
# the install tree):
#
#   cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_header
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-07-14-psp-parser-header
#
# What's propagated AUTOMATICALLY by the library's config (no manual
# include / lib paths here):
#
#   - CMAKE_CXX_STANDARD 23           (was 17, bumped today)
#   - The include path                 (PUBLIC $<INSTALL_INTERFACE:include>)
#   - libpsp_span_lib.a                (PUBLIC target_link_libraries)
#
# The consumer just links against psp_span_lib and the rest is plumbing.

cmake_minimum_required(VERSION 3.16)
project(psp_parser_header_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib's new <psp_span/parser.h> — exercises parse_int, parse_double, and ParseError via find_package()."
    LANGUAGES CXX
)

# find_package() reads the psp_span_libConfig.cmake from the install
# tree. It populates the IMPORTED target psp_span_lib::psp_span_lib
# (or, if NAMESPACE was elided in install(EXPORT ...), the target is
# named psp_span_lib — the library's CMakeLists.txt uses NAMESPACE
# psp_span_lib::, so we can use either form).
#
# REQUIRED makes the configure step fail loudly if the library is
# not installed at the prefix path. Without REQUIRED, the consumer
# would configure successfully but fail at build time, which is
# harder to debug.
find_package(psp_span_lib 0.6 REQUIRED)

add_executable(P-2026-07-14-psp-parser-header
    P-2026-07-14-psp-parser-header.cpp
)

# This single line gets the consumer:
#   - psp::parse_int / psp::parse_double  (inline in <psp_span/parser.h>)
#   - psp::Span<T, Extent>                (in <psp_span/span.h>)
#   - enum ParseError + formatter         (in <psp_span/parser.h>)
#   - cxx_std_23 propagation              (PUBLIC compile_features on the target)
#   - the include path                     (PUBLIC $<INSTALL_INTERFACE:include>)
#
# And the linker is happy because libpsp_span_lib.a has the explicit
# instantiations of psp::Span<int> / Span<const int> / Span<double>.
# The parser functions are `inline` in parser.h, so no archive symbols
# are needed for those — the compiler emits them at the call site
# from the header text.
target_link_libraries(P-2026-07-14-psp-parser-header
    PRIVATE psp_span_lib::psp_span_lib
)

# Sanitizer hook (mirror of the library's ENABLE_ASAN option). Useful
# when chasing a parser bug — easy to enable without touching the
# library build.
option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-07-14-psp-parser-header PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-07-14-psp-parser-header PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()