# CMakeLists.txt — psp_span_lib, a static library with explicit
# instantiations in the source tree, install rules, AND a CPack
# package descriptor that produces a redistributable .tar.gz.
#
# This is the "real library you can ship" shape. The Jun 28 lesson
# (`P-2026-06-28-cmake-interface-library.cpp`, `psp_span` as an
# `INTERFACE` target), the Jun 29 lesson
# (`P-2026-06-29-extern-template.cpp`, consumer-side `extern
# template` suppression), the Jun 30 lesson
# (`P-2026-06-30-psp-span-static-library.cpp`, the library owns
# its own explicit-instantiation TU), and the Jul 1 lesson
# (`P-2026-07-01-cmake-install-find-package.cpp`, install rules +
# find_package consumer) all set up today's forward-on:
# **the library can be PACKAGED into a redistributable archive
# with CPack, and a completely fresh consumer can install + use
# it from that archive alone.**
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
#
# Why CPack (today)?
# ------------------
# `cmake --install` puts the install tree on *this* machine.
# `cpack` packages that install tree into an archive that can be
# shipped to *another* machine. That second machine has no
# awareness of how psp_span_lib was built — only what's in the
# archive. So an archive must contain exactly what `cmake
# --install` produces, and nothing more.
#
# Today we:
#   1. add `include(CPack)` and a few `set(CPACK_* variables)` to
#      this file,
#   2. run `cpack -C Release` from the build dir to produce
#      `psp_span_lib-0.4.0-Darwin.tar.gz`,
#   3. prove a fresh consumer can install + find_package() the
#      library from the archive alone (no source tree, no build
#      dir, no /tmp/psp_install).

cmake_minimum_required(VERSION 3.16)
project(psp_span_lib
    VERSION 0.4.0
    DESCRIPTION "psp_span static library with explicit Span<int>, Span<const int>, Span<double> instantiations; installable; find_package()-able; CPack-redistributable"
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
#
# Note both forms:
#   $<BUILD_INTERFACE:...>  — when this library is used *as a build
#                              target* (i.e. consumers add_subdirectory()
#                              it, or link the .a directly during build).
#   $<INSTALL_INTERFACE:...> — when this library is used *as an installed
#                              target* (i.e. consumer find_package()'d it).
#
# Today (Jul 1) we fill in the INSTALL_INTERFACE for the first time.
# From now on the include path switches automatically based on how the
# consumer sees the library — build vs install.
target_include_directories(psp_span_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
)

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

# ===========================================================================
# Install rules (Jul 1 lesson — the headline forward-on from Jun 30).
# ===========================================================================
#
# A library that doesn't ship install rules is a *build target*, not a
# *package*. Today we promote psp_span_lib from the former to the
# latter. Three install lines below do all the work:
#
#   1. install(TARGETS ...)         — install the .a and its metadata.
#   2. install(FILES ...)            — install the public header.
#   3. install(EXPORT ...)           — generate and install the
#                                       psp_span_libConfig.cmake package
#                                       config + targets file so
#                                       `find_package(psp_span_lib)`
#                                       works for downstream projects.
#
# Layout on disk after `cmake --install build --prefix /tmp/psp_install`:
#
#   /tmp/psp_install/
#   ├── include/
#   │   └── psp_span/
#   │       └── span.h
#   └── lib/
#       ├── libpsp_span_lib.a
#       └── cmake/
#           └── psp_span_lib/
#               ├── psp_span_libConfig.cmake
#               ├── psp_span_libConfigVersion.cmake
#               └── psp_span_libTargets.cmake
#
# Three CMake subdirs for "cmake/" + package name + files. That
# nesting is what makes `find_package(psp_span_lib REQUIRED)` work
# from any consumer: it expects exactly this layout on each search
# path (CMAKE_PREFIX_PATH, /usr/local, etc.).
#
# Bumped version 0.2.0 -> 0.3.0 because we now ship a *different*
# thing. The version bump is real (a new public capability), not
# cosmetic.

include(GNUInstallDirs) # defines CMAKE_INSTALL_<dir> like LIBDIR, INCLUDEDIR.

# 1. Install the .a archive (and its associated metadata).
#    ARCHIVE is the right DESTINATION for a static library on
#    every platform; CMake maps it to lib/ on Linux/macOS and
#    to the right Windows path on Windows.
install(TARGETS psp_span_lib
    EXPORT psp_span_libTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 2. Install the public header.
#    psp::Span is a template, so the *header* is the public API;
#    there is no .so/.a symbol to import that doesn't also need
#    the header in the include path.
install(FILES include/psp_span/span.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/psp_span
)

# 3. Install the export set.
#    This generates and installs:
#       - psp_span_libTargets.cmake    (defines the IMPORTED targets)
#       - psp_span_libTargets-<config>.cmake (per-config variants, if any)
#    Combined with configure_package_config_file() below, it produces
#    the full find_package() interface.
install(EXPORT psp_span_libTargets
    FILE psp_span_libTargets.cmake
    NAMESPACE psp_span_lib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/psp_span_lib
)

# 4. Generate and install the package config file.
#    The <pkg>Config.cmake file is what consumers include implicitly
#    when they call find_package(psp_span_lib). It typically just
#    includes the targets file (and any optional dependencies).
include(CMakePackageConfigHelpers)

# configure_package_config_file() turns a template (psp_span_libConfig.cmake.in,
# which we write below) into a real <pkg>Config.cmake with all the
# install-dir variables substituted in. This is what lets consumers
# write target_link_libraries(... psp_span_lib::psp_span_lib) without
# ever knowing where the library actually lives on disk.
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/psp_span_libConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/psp_span_libConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/psp_span_lib
)

# install() the generated config file next to the targets file.
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/psp_span_libConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/psp_span_lib
)

# And the version file (compat with whatever version of psp_span_lib
# the consumer asked for).
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/psp_span_libConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/psp_span_libConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/psp_span_lib
)

message(STATUS "==================================================")
message(STATUS " psp_span_lib ${PROJECT_VERSION} configured.")
message(STATUS " Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS " ASan+UBSan:  ${ENABLE_ASAN}")
message(STATUS " Source dir:  ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS " Install dir: ${CMAKE_INSTALL_PREFIX}")
message(STATUS " Lib dir:     ${CMAKE_INSTALL_LIBDIR}")
message(STATUS " Inc dir:     ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "==================================================")

# ===========================================================================
# CPack — package the install tree into a redistributable archive
# ===========================================================================
#
# `include(CPack)` is the ONE LINE that turns the project into something
# `cpack` can package. After this line, CMake auto-generates a "package"
# target (which depends on "install") and reads CPACK_* variables from
# CMakeCache.txt to produce archives.
#
# We set five variables. Each one is documented inline because CPack
# defaults are 1990s-era ("any platform, any user, no version, no
# generator") and almost nobody wants the defaults.
#
# Variable cheatsheet
# -------------------
#   CPACK_PACKAGE_NAME          — base name of the produced archive.
#                                  Defaults to project name (psp_span_lib).
#                                  Set explicitly so the consumer script
#                                  can `tar xzf psp_span_lib-0.4.0-*.tar.gz`
#                                  without needing to glob.
#
#   CPACK_PACKAGE_VERSION       — defaults to ${PROJECT_VERSION}. Set
#                                  explicitly for the same reason: the
#                                  driver script reads this from the
#                                  archive filename.
#
#   CPACK_PACKAGE_VENDOR        — appears in the welcome/description
#                                  files inside the archive. macOS .pkg
#                                  and Debian .deb actually read it.
#
#   CPACK_GENERATOR             — semicolon-separated list. We use TGZ,
#                                  the cross-platform POSIX tarball
#                                  generator. CMake also ships DEB, RPM,
#                                  IFW (Qt installer), NSIS, NuGet,
#                                  WIX (Windows), and "productbuild"
#                                  (macOS .pkg). TGZ is the cheapest one
#                                  that works on every platform — it's
#                                  what most "source-distribution" sites
#                                  actually serve.
#
#                                  Note: TGZ runs `cmake --install .`
#                                  internally, then tars up the result.
#                                  So the archive contents == exactly the
#                                  install tree. If we add a new
#                                  install(FILES ...) line, it'll show up
#                                  in the archive automatically.
#
#   CPACK_PACKAGE_FILE_NAME      — overrides the default filename pattern
#                                  (`<name>-<version>-<system>`). We set
#                                  it explicitly to match what downstream
#                                  tooling expects: psp_span_lib-0.4.0-Darwin.tar.gz
#                                  (system name comes from uname -s on
#                                  CMake's side).
#
# What's NOT set (and why)
# ------------------------
#   CPACK_RESOURCE_FILE_LICENSE  — welcome file / license page in the
#                                  installer UI. We don't ship one yet
#                                  (psp_span_lib is in-tree).
#   CPACK_RESOURCE_FILE_README   — readme shown by installers. We have
#                                  README.md but CPack wants plain text
#                                  and we want this lesson to stay
#                                  focused on the .tar.gz flow.
#   CPACK_DEBIAN_PACKAGE_*       — Debian-specific knobs (section,
#                                  depends, maintainer). Only used when
#                                  CPACK_GENERATOR includes DEB.
#
# Bumped version 0.3.0 -> 0.4.0 because CPack is a new public
# capability (the package is now actually redistributable).
include(CPack)
set(CPACK_PACKAGE_NAME             "psp_span_lib")
set(CPACK_PACKAGE_VERSION          "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR           "Petra @ kanopi.local")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "psp::Span<T,Extent> static library — installable + find_package()-able + CPack-redistributable.")
set(CPACK_PACKAGE_HOMEPAGE_URL     "https://github.com/ArloNOppie/KanopiLearningCPPLessons")
set(CPACK_GENERATOR                "TGZ")
# Filename pattern: <name>-<version>-<system>.<ext>
# System = $(uname -s) on the build machine (CMake fills this in).
# So we get: psp_span_lib-0.4.0-Darwin.tar.gz on macOS,
#            psp_span_lib-0.4.0-Linux.tar.gz on Linux.
# Don't set CPACK_PACKAGE_FILE_NAME manually — let CMake compose it.

message(STATUS " CPack package:  ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
message(STATUS " CPack generator: ${CPACK_GENERATOR}")