# CMakeLists.txt — today's P-2026-08-20 lesson:
#
#   std::flat_map / std::flat_set / std::flat_multimap / std::flat_multiset
#   (C++23) — cache-friendly sorted-vector associative containers
#
# Where this fits in the arc
# --------------------------
#   - Jul  9   std::span     (C++20) — 1-D non-owning view
#   - Jul 10   std::mdspan   (C++23) — N-D non-owning view
#   - Jul 12   std::expected (C++23) — sum-type error channel
#   - today    std::flat_map family (C++23) — cache-friendly
#     TODAY    sorted-vector associative containers (4 containers,
#              transparent comparator, heterogeneous lookup, bulk
#              insert, extract+replace, custom comparator)
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-20-std-flat-map-cpp23
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-20-std-flat-map-cpp23
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-20-std-flat-map-cpp23
#
# -O2 empirics build (the canonical numbers — flat_map wins bulk
# insert and lookup at -O2):
#
#   cmake -S . -B build-O2 -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-O2
#   ./build-O2/P-2026-08-20-std-flat-map-cpp23

cmake_minimum_required(VERSION 3.16)
project(std_flat_map_cpp23_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of the C++23 std::flat_map / std::flat_set / std::flat_multimap / std::flat_multiset family on Apple Clang 21.0.0 / libc++ — covers the minimum viable API, the sorted-vector storage model, iterator-category (random_access), bulk insert + sorted_unique + sorted_equivalent tags, transparent comparator + heterogeneous lookup, custom comparator (case-insensitive), extract+replace ownership transfer, try_emplace / emplace_hint / insert_or_assign, the four-container family, and empirics vs std::map."
    LANGUAGES CXX
)

# C++23 for std::flat_map (P0429R9) and the rest of the C++23 surface
# used in this consumer.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-20-std-flat-map-cpp23
    P-2026-08-20-std-flat-map-cpp23.cpp
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-20-std-flat-map-cpp23 PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-20-std-flat-map-cpp23 PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
