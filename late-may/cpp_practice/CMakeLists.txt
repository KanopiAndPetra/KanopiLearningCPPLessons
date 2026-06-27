# CMakeLists.txt — Petra, 2026-06-27.
#
# Purpose
# -------
# Replicate the Jun 16 Makefile build as a modern CMake build for
# the *exact same* source files: box.h, box.cpp, inventory.h,
# inventory.cpp, and the driver P-2026-06-27-cmake-build.cpp.
#
# Why CMake (vs the existing Makefile)
# ------------------------------------
# The Makefile works for one project on one machine. Real-world
# cross-platform builds (Linux, macOS, Windows MSVC, embedded
# cross-compilers, package managers like vcpkg/Conan) all expect
# CMake. This lesson shows the same program rebuilt with the
# industry-standard tool, and the things CMake makes easier than
# raw Make:
#
#   1. Out-of-source builds (build/ is auto-created, never mixed
#      with sources).
#   2. Generated dependency tracking (no manual -MMD/-MP dance).
#   3. Generator abstraction (Make, Ninja, Xcode, Visual Studio
#      all work without rewriting recipes).
#   4. Sanitizer / coverage / release presets (`cmake -B build`
#      with a cache var, no second target needed).
#   5. Install rules, find_package(), export() — all standard.
#
# Build it
# --------
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build -j
#   ./build/P-2026-06-27-cmake-build
#
#   # ASan build (one-shot):
#   cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
#   cmake --build build-asan -j
#   ./build-asan/P-2026-06-27-cmake-build
#
#   # Release build:
#   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-release -j
#
#   # Cleanup:
#   rm -rf build build-asan build-release

# ---------------------------------------------------------------------------
# 1. Project declaration + minimum CMake version.
# ---------------------------------------------------------------------------
# cmake_minimum_required(VERSION 3.16) — 3.16 added `target_precompile_headers`
# and is the floor most modern packages assume. Below that, `import_std`
# and C++20 features fall over. We're on cmake 4.3 here; this is comfortable.
cmake_minimum_required(VERSION 3.16)

# PROJECT_NAME: this is also the name of the top-level target's folder in
# IDE generators (Xcode, Visual Studio). PROJECT_VERSION is exposed as
# a compile-time #define if you ever want it in the binary.
project(PetraInventory
    VERSION 0.1.0
    DESCRIPTION "Multi-file Inventory lesson (Jun 16 source, CMake build)"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# 2. C++ standard.
# ---------------------------------------------------------------------------
# set(CMAKE_CXX_STANDARD 17) + set(CMAKE_CXX_STANDARD_REQUIRED ON) is the
# modern equivalent of passing -std=c++17. REQUIRED makes it a hard error if
# the chosen compiler doesn't have C++17; without it, CMake silently falls
# back to whatever the compiler's default is.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # prefer -std=c++17 over -std=gnu++17

# ---------------------------------------------------------------------------
# 3. Default build flags (Debug vs Release).
# ---------------------------------------------------------------------------
# CMAKE_BUILD_TYPE is a single string the user sets at configure time:
#   Debug, Release, RelWithDebInfo, MinSizeRel. We pick sensible flags per
#   type via `if()`, so the user only has to pass -DCMAKE_BUILD_TYPE=...
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# These are *added* to the global flags. CMake's defaults already
# include -g for Debug and -O3 for Release; we layer our strict
# warnings on top regardless of build type.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        # -Wshadow would be nice but breaks in libstdc++ headers we
        # don't touch. Leave it off for now.
    )
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# ---------------------------------------------------------------------------
# 4. Optional: AddressSanitizer + UBSan via a cache var.
# ---------------------------------------------------------------------------
# The pattern is "option(ENABLE_X) ... if(ENABLE_X) flags". Cache vars
# (set with CACHE) are remembered between configure runs and editable
# via `cmake -DENABLE_ASAN=ON -B build` or `ccmake build`.
option(ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)

if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            -fsanitize=address
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )
        add_link_options(
            -fsanitize=address
            -fsanitize=undefined
        )
        # ASan needs an optimization level of at least -O1 to fold
        # instrumentation. -O0 (CMake's Debug default) works too but
        # is slower; -Og is the "optimize for debugging" sweet spot.
        add_compile_options(-O1)
        message(STATUS "ASan + UBSan enabled")
    else()
        message(WARNING "ENABLE_ASAN=ON is not supported on this compiler; ignoring")
    endif()
endif()

# ---------------------------------------------------------------------------
# 5. The library target.
# ---------------------------------------------------------------------------
# add_library() compiles box.cpp and inventory.cpp into a single
# archive (.a on macOS/Linux, .lib on Windows). It's an OBJECT or
# STATIC library — pick STATIC for now (linked at build time, no
# dynamic loading).
#
# Why a library and not just a long add_executable?
#   1. Reusability. If we ever add a second driver that uses the
#      same Box/Inventory (e.g. a tests target), it just links
#      against petra_inventory and gets the symbols for free.
#   2. Clean dependency graph. CMake tracks `petra_inventory` as a
#      separate node; only files in this library recompile when
#      box.h or inventory.h change.
#
# The PUBLIC keyword on target_include_directories(...) makes
# "include the dir containing this target's headers" propagate to
# any consumer. Here, the consumers are the driver(s), so the
# driver can `#include "box.h"` without specifying a path.
add_library(petra_inventory STATIC
    box.cpp
    inventory.cpp
)
target_include_directories(petra_inventory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# 6. The driver executable.
# ---------------------------------------------------------------------------
# add_executable() compiles the driver .cpp and links it against
# petra_inventory. The CMake-generated link command resolves the
# Box and Inventory symbols out of libpetra_inventory.a.
add_executable(P-2026-06-27-cmake-build
    P-2026-06-27-cmake-build.cpp
)
target_link_libraries(P-2026-06-27-cmake-build
    PRIVATE petra_inventory
)

# ---------------------------------------------------------------------------
# 7. Tell CMake where the headers live so consumers (e.g. IDE
#    generators) can find them without an extra search path.
# ---------------------------------------------------------------------------
# This is documentation-y but useful: when you open this project
# in Xcode / Visual Studio / Qt Creator, the IDE uses this list
# to set up the include search paths automatically.
set_target_properties(petra_inventory PROPERTIES
    PUBLIC_HEADER "box.h;inventory.h"
)

# ---------------------------------------------------------------------------
# 8. Print a friendly summary at the end of configuration.
# ---------------------------------------------------------------------------
message(STATUS "==================================================")
message(STATUS " Project:        ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS " Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS " C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS " Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " Source dir:     ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS " Binary dir:     ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS " ASan + UBSan:   ${ENABLE_ASAN}")
message(STATUS "==================================================")