# CMakeLists.txt — today's P-2026-08-21 lesson:
#
#   std::flat_map / std::flat_set with a CUSTOM KeyContainer and
#   MappedContainer (C++23) — std::deque storage and a hand-rolled
#   fixed-capacity static_vector<T, N>
#
# Where this fits in the arc
# --------------------------
#   - Jul  9   std::span     (C++20) — 1-D non-owning view
#   - Jul 10   std::mdspan   (C++23) — N-D non-owning view
#   - Jul 12   std::expected (C++23) — sum-type error channel
#   - Aug 20   std::flat_map family (C++23) — DEFAULT std::vector storage
#   - TODAY    std::flat_map family (C++23) — CUSTOM storage
#
# This closes the FIRST item on the Aug 20 lesson's "Natural follow-on
# lessons for the C++23 stdlib tour" list, verbatim:
#
#     "std::flat_map with a custom KeyContainer — supply a std::deque or
#      boost::container::small_vector as the underlying storage; the API
#      surface is unchanged but the reallocation / invalidation story
#      changes."
#
# Aug 20 asserted that. Today measures it:
#   - API unchanged      -> one generic exercise<Map>() template runs
#                           against all three back-ends, same output.
#   - Reallocation       -> heap allocations COUNTED through a replaced
#                           global operator new: 14 / 4 / 0.
#   - Invalidation       -> the ADDRESS of a mapped value compared
#                           before and after a tail insert.
#
# Plus one finding only fixed-capacity storage can surface: flat_map's
# insert path has NO strong exception guarantee — on throw the map is
# EMPTIED ([flat.map.modifiers]), not left unchanged.
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-21-flat-map-custom-containers
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-21-flat-map-custom-containers
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-21-flat-map-custom-containers
#
# NOTE on ASan and Section 5: the counting hook is a replaced GLOBAL
# operator new, so it sits ABOVE the sanitizer's malloc interception and
# the counts are unaffected by it. Measured: all three builds report the
# same 14 / 4 / 0, and all 88 checks pass identically on every build.
#
# NOTE on macOS: LeakSanitizer (ASAN_OPTIONS=detect_leaks=1) is NOT
# supported on darwin/arm64 — setting it aborts the process at startup.
# Run the ASan build with detect_leaks unset.

cmake_minimum_required(VERSION 3.16)
project(flat_map_custom_containers_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of C++23 std::flat_map / std::flat_set with custom KeyContainer + MappedContainer template arguments on Apple Clang 21.0.0 / libc++ — covers the KeyContainer contract ([flat.map.overview]/8-10), API parity across std::vector / std::deque / a hand-rolled fixed-capacity static_vector, measured allocation profiles via a replaced global operator new, measured reference-invalidation behaviour, the clear()-on-throw exception contract for capacity-exhausted storage, extract()/replace() and cross-storage migration, and flat_set / flat_multimap on custom storage."
    LANGUAGES CXX
)

# C++23 for std::flat_map (P0429R9) / std::flat_set (P1222R4).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-21-flat-map-custom-containers
    P-2026-08-21-flat-map-custom-containers.cpp
)

# Section 7 catches a std::length_error thrown out of the underlying
# container, so exceptions must stay enabled.
target_compile_options(P-2026-08-21-flat-map-custom-containers PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-21-flat-map-custom-containers PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-21-flat-map-custom-containers PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
