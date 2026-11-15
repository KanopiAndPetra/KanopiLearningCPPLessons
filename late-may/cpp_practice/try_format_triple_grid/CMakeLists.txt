# CMakeLists.txt — today's P-2026-11-15 lesson:
#
#   try_format_triple_grid — the grid past PAIRS: what happens when
#   THREE options meet, and whether the fill's Unicode rule and the
#   align/zero rule are independent.
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
#   - Nov 14           the COMBINATION GRID — the 21 PAIRS of options,
#                      and the align/zero rule that only exists where
#                      two options meet.
#   - TODAY            the TRIPLE GRID — Nov 14's next-step item (1),
#                      closed: the 35 TRIPLES of options, and the ONE
#                      cell where the fill's rule and the align/zero
#                      rule meet — where the scanner loses the align.
#
# What today measures (see the .md for the detail):
#
#   - The triple grid (35 triples x 19 types x 7 rows = 4,655 cells)
#     asks whether any rule needs THREE options to meet.  A pair grid
#     cannot see such a rule, so Nov 14's silence was evidence, not
#     proof.
#   - The fill x align x zero intersection is where the two rules
#     meet.  On the RECOVERY path — the path an implementation takes
#     when it substitutes U+FFFD for an ill-formed fill — the scanner
#     consumed the alignment option and did not RECORD it, so the '0'
#     rule saw no align option.  Neither shipped implementation can
#     see this, so today measures it with a policy that can.
#   - The chain's own question, answered: the two rules are NOT
#     independent — but only in one cell, and only on one path.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-15-triple-grid
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-15-triple-grid

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-15-triple-grid
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

add_executable(P-2026-11-15-triple-grid
               P-2026-11-15-triple-grid.cpp)
