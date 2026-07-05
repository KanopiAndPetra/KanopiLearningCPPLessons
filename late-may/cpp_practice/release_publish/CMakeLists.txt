# P-2026-07-05 — release_publish consumer
#
# This CMakeLists builds the release_publish verification driver against
# psp_span_lib v0.5.0 from a *GitHub Release URL*, not from a local source
# tree.
#
# Build flow (run from a fresh out-of-source build/):
#   1. cd release_publish/build && cmake .. -DCMAKE_PREFIX_PATH=<install-root>
#      (where <install-root> is the directory containing
#       lib/cmake/psp_span_lib/psp_span_libConfig.cmake — either the
#       locally-built install prefix, or the archive's extracted prefix)
#   2. cmake --build .
#   3. ./P-2026-07-05-github-release-publish
#
# At run time, the driver calls `gh release view` and `gh release download`
# (from the GitHub CLI), extracts the .tar.gz into ./release_verify_workdir/,
# then spawns a fresh child CMake configure+build against the extracted
# prefix and runs the resulting smoke binary. The child build verifies
# that the GitHub-hosted archive satisfies every CMake contract.

cmake_minimum_required(VERSION 3.16)
project(release_publish VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# We compile against the *installed* psp_span_lib, not its source tree.
# This is the only way to verify "the archive you ship is the archive
# users compile against".
#
# The driver hard-codes owner/repo/tag, so we don't need CMAKE_PREFIX_PATH
# to point at a specific build — the driver fetches the archive itself.
# But we still need psp::Span at compile time for THIS driver, so require
# the package on the configure side too.
find_package(psp_span_lib REQUIRED)

add_executable(P-2026-07-05-github-release-publish
    P-2026-07-05-github-release-publish.cpp)

target_link_libraries(P-2026-07-05-github-release-publish
    PRIVATE psp_span_lib::psp_span_lib)

target_compile_definitions(P-2026-07-05-github-release-publish
    PRIVATE PSP_SPAN_LIB_VERSION="${psp_span_lib_VERSION}")

# Strict warnings — match the cron session's standard compile flags.
target_compile_options(P-2026-07-05-github-release-publish PRIVATE
    -Wall -Wextra -Wpedantic)

# Be a good citizen when running out of a cache directory.
set_target_properties(P-2026-07-05-github-release-publish PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")