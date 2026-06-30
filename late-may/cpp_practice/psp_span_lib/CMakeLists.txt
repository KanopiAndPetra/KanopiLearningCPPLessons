# CMakeLists.txt — psp_span_lib, a static library with explicit
# instantiations in the source tree.
#
# This is the "real library" shape that the Jun 28 lesson
# (`P-2026-06-28-cmake-interface-library.cpp`, `psp_span` as an
# `INTERFACE` target) and the Jun 29 lesson
# (`P-2026-06-29-extern-template.cpp`, consumer-side `extern
# template` suppression) set up. Jun 30 (today) is the natural
# next step: the library owns its own explicit-instantiation TU.
#
# Why STATIC and not INTERFACE?
# ------------------------------
# psp::Span<T, Extent> is a template, so the *header* (include/psp_span/span.h)
# has to stay in the include path of every consumer — that's a
# property of templates, not of the target kind. The choice between
# INTERFACE and STATIC is about whether the library ships COMPILED
# OBJECTS or just USAGE REQUIREMENTS.
#
# - INTERFACE  (Jun 28): no .o files, no .a. Consumers parse the
#   full header in every TU and emit strong definitions for the
#   methods they call. Compile time is slower (every TU parses
#   the template) but link time is the same.
#
# - STATIC     (Jun 30): one .cpp file (src/psp_span_inst.cpp)
#   holds the explicit instantiations of the most-common
#   specializations, which compile to a libpsp_span_lib.a archive.
#   Consumers can `extern template class psp::Span<int>;` to
#   suppress their own instantiations (per Jun 29) and rely on
#   the archive to satisfy the linker. Compile time is faster
#   for consumers that opt in.
#
# The archive has just *one* .o (psp_span_inst.o), and that .o
# has every member of every pre-instantiated specialization.
# This makes the "core" of the library a single ~2KB object file.
# Consumers that don't opt in still parse the full header and
# emit strong definitions of their own — the archive is OPT-IN
# speedup, not a prerequisite.

cmake_minimum_required(VERSION 3.16)
project(psp_span_lib
    VERSION 0.2.0
    DESCRIPTION "psp_span static library with explicit Span<int>, Span<const int>, Span<double> instantiations"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Per-target (not global) compile options. STATIC libraries
# inherit these when they are built into consumers — that's the
# standard CMake `add_compile_options` vs `target_compile_options`
# distinction.
add_library(psp_span_lib STATIC
    src/psp_span_inst.cpp
)

# PUBLIC include path: consumers can `#include <psp_span/span.h>`.
# `PUBLIC` (vs INTERFACE) is technically unnecessary here (no
# downstream target that ALSO exports the include dir exists), but
# it makes the intent obvious: "this is the public API of the
# library, and anyone who links against us gets it for free."
target_include_directories(psp_span_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # $<INSTALL_INTERFACE:include>  — set below in the install
        # rules once we have a stable install layout.
)

# PUBLIC compile features: consumers get -std=c++17 automatically
# when they link against psp_span_lib. Same pattern as Jun 28.
target_compile_features(psp_span_lib PUBLIC cxx_std_17)

# Position-independent code. macOS and Linux both default to
# non-PIC for static archives, but PIC is harmless for static
# libs and means the same .a can be linked into a shared library
# later without rebuilding. Standard hygiene.
set_target_properties(psp_span_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PUBLIC_HEADER include/psp_span/span.h
)

# Strict warnings on the library itself. Consumers get whatever
# warnings they configure.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(psp_span_lib PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# ---- ASan/UBSan support for the library itself ----
# Mirror the Jun 27 / Jun 28 pattern: an option enables sanitizers
# for everything we build, including psp_span_lib's one TU.
option(ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)
if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(psp_span_lib PRIVATE
            -fsanitize=address -fsanitize=undefined
            -fno-omit-frame-pointer -O1
        )
    endif()
endif()

message(STATUS "==================================================")
message(STATUS " psp_span_lib ${PROJECT_VERSION} configured.")
message(STATUS " Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS " ASan+UBSan:  ${ENABLE_ASAN}")
message(STATUS " Source dir:  ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "==================================================")
