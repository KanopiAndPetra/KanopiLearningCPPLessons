# CMakeLists.txt — today's P-2026-11-14 lesson:
#
#   try_format_combination_grid — the grid past SINGLE options: what
#   happens when TWO options meet, and the per-implementation rule
#   that says what meeting them MEANS.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind (4 kinds).
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - Oct 27           the contract comparison against std::format_to_n.
#   - Oct 28           the ERROR TAXONOMY — a constexpr grammar scanner
#                      plus a libc++-specific message table.
#   - Oct 30           the TYPE-AWARE scanner — the argument's category
#                      decides the spec-vs-argument class.
#   - Nov 7            the POLICY TABLE — one mask table plus
#                      per-implementation override rows.
#   - Nov 12           the CARRIED VERDICT — the scanner's silence is
#                      carried into the fallback as silence, and the
#                      numeric ceiling joins the policy table.
#   - Nov 13           the UNICODE FILL RULE — the fill is decoded as a
#                      scalar value; the ill-formed fill is structural.
#   - TODAY            the COMBINATION GRID — Nov 13's next-step item
#                      (2), closed: the 21 PAIRS of options, the
#                      align/zero rule that only exists where two
#                      options meet, and the per-implementation
#                      boolean that says which wording is enforced.
#
# What today measures (see the .md for the detail):
#
#   - The 1,596-cell grid isolates ONE option per cell, so it cannot
#     see a rule that only exists where TWO options meet.  The
#     combination grid (21 pairs x 19 types x 7 rows = 2,793 cells)
#     finds SEVEN cells the single-option grid cannot see, and all
#     seven are the align option next to the '0' option.
#   - libc++ 21 IGNORES the '0' when an align option is present (the
#     pre-P2572R1 wording); libstdc++ 15 still validity-checks it (the
#     current wording).  So the difference is a per-implementation
#     boolean in the policy table.
#   - The chain's own question, answered: "{:+c}" on a char fails at
#     PHASE 2 (the allow mask), not phase 1 (the consume mask).
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-14-combination-grid
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-14-combination-grid

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-14-combination-grid
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  It is a libc++-only flag, so it is
# added for Clang only: the lesson is BUILT AND MEASURED under
# libstdc++ 15 too, and a flag g++ does not know would stop that.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fexperimental-library)
    add_link_options(-fexperimental-library)
endif()

# Optional strict-warning mode toggle.
option(ENABLE_STRICT_WARNINGS
       "Enable -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
       OFF)
if(ENABLE_STRICT_WARNINGS)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wconversion -Wsign-conversion)
endif()

# Optional ASan/UBSan mode toggle.
option(ENABLE_ASAN
       "Enable -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer"
       OFF)
if(ENABLE_ASAN)
    add_compile_options(
        -fsanitize=address -fsanitize=undefined
        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

add_executable(P-2026-11-14-combination-grid
               P-2026-11-14-combination-grid.cpp)
