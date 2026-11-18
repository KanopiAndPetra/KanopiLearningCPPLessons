# CMakeLists.txt — today's P-2026-11-18 lesson:
#
#   try_format_precision — the PRECISION is counted in COLUMNS too: it
#   is the longest prefix of the argument whose field width is no
#   greater than the precision, and the cut lands on an extended
#   grapheme cluster boundary.
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
#   - TODAY            the PRECISION — the same unit, the same table,
#                      and a cluster-boundary cut.  Plus UAX #29's GB11,
#                      which the arc's own ZWJ rule got wrong.
#
# What today measures (see the .md for the detail):
#
#   - The precision's unit is the COLUMN: it is the longest prefix
#     whose FIELD WIDTH is no greater than the precision, so the
#     width's table is the precision's table -- ONE table, TWO rules.
#   - The cut lands on a CLUSTER BOUNDARY: 'e' + U+0301 is one cluster
#     of one column, so a precision of 1 keeps the whole three bytes.
#   - The precision truncates BEFORE the width pads.
#   - The '?' type's precision counts the columns of the ESCAPED
#     output, and can cut an escape sequence in half.
#   - UAX #29's GB11: a pictographic joins the preceding cluster only
#     after a pictographic Extend* ZWJ.  The arc's Nov 17 rule joined
#     unconditionally, and was wrong in both directions.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-18-precision
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-18-precision

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-18-precision
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

add_executable(P-2026-11-18-precision
               P-2026-11-18-precision.cpp)
