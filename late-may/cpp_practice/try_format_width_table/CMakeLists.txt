# CMakeLists.txt — today's P-2026-11-22 lesson:
#
#   try_format_width_table — the arc's RESTRICTED width table was not a
#   restriction of libc++'s full one; it was a DIFFERENT table, and the
#   two disagree on 8,493 code points in 58 intervals, in BOTH
#   directions.  The arc now carries libc++ 21's FULL table (the same
#   110 packed entries) and the width table's PROVENANCE is pinned:
#   the oracle's table is exactly Unicode 16.0.0's EastAsianWidth.txt,
#   bracketed by two witness interval lists.
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
#   - Nov 15           the TRIPLE GRID — the 35 TRIPLES of options, and
#                      the ONE cell where the fill's rule and the
#                      align/zero rule meet.
#   - Nov 16           the FILL BEHAVIOUR SPACE — the fill's enforcement
#                      as ONE datum, and the first grid in the arc whose
#                      axis is the POLICY rather than the spec.
#   - Nov 17           the FIELD WIDTH — the width's UNIT, measured
#                      against the linked stdlib: bytes agree on 1 of 28
#                      cells, code points on 11 of 28, COLUMNS on 28 of
#                      28.
#   - Nov 18           the PRECISION — the same unit, the same table,
#                      and a cluster-boundary cut.  Plus UAX #29's GB11,
#                      which the arc's own ZWJ rule got wrong.
#   - Nov 19           the ONE COLUMN RULE — the two rules become one
#                      function, the defective rule is deleted, and the
#                      ONE rule gains GB9a, GB9b, GB9c, GB12/GB13 and
#                      GB6/GB7/GB8.
#   - Nov 20           the RULE ORDER — UAX #29's rules are an ORDERED
#                      list and the first match decides.  GB9c is
#                      consulted BEFORE GB11, and the conjunct CONSUMES
#                      its linker.  Two defects, measured on a battery
#                      of collision cells.
#   - Nov 21           the CONTROL CHARACTERS — GB3, GB4 and GB5, in
#                      UAX #29's order (before GB9), plus libc++ 21's
#                      ASCII FAST PATH, which charges an ASCII prefix
#                      per CODE UNIT.  Four more cells closed, and one
#                      more place where the ORACLE departs from the
#                      standard's rules.
#   - TODAY            the WIDTH TABLE — the arc's restricted table was
#                      not a restriction of libc++'s full one; it was a
#                      different table, and the two disagree on 8,493
#                      code points.  The arc now carries libc++'s FULL
#                      table, and the table's PROVENANCE is pinned to
#                      Unicode 16.0.0's EastAsianWidth.txt.
#
# What today measures (see the .md for the detail):
#
#   - The arc's RESTRICTED width table (54 ranges) and libc++ 21's FULL
#     one (110 packed entries) disagree on 8,493 code points in 58
#     intervals: 8,343 where the arc UNDER-counts and 150 where it
#     OVER-counts.  Neither table is a subset of the other.
#   - A battery drawn from the DISAGREEMENT intervals (not from the
#     table, as every earlier battery was) shows the restricted table
#     losing all 26 cells and the full table winning all 26.
#   - The table's PROVENANCE is pinned: the oracle's table is exactly
#     Unicode 16.0.0's EastAsianWidth.txt, bracketed by the 13 intervals
#     that 16.0.0 added and the 11 that 17.0.0 added.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-22-width-table
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-22-width-table

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-22-width-table
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

add_executable(P-2026-11-22-width-table
               P-2026-11-22-width-table.cpp)
