# CMakeLists.txt — today's P-2026-11-25 lesson:
#
#   try_format_tags_block — the arc's WIDEST gap (the Tags block,
#   U+E0020..U+E007F, 96 code points) turned out to be not a gap in the
#   TABLE but a gap in the BATTERY:
#
#     libc++ 21's full Extend table : 96 of the 96 tags are Extend
#     the arc's restricted table      :  0 of the 96 tags are Extend
#
#   And the arc's own battery corpus contains ZERO code points from the
#   Tags block, because every cell the arc has ever drawn has the same
#   SHAPE -- a base and exactly ONE candidate -- so a cell can never
#   carry a RUN.  The Tags block is precisely the block whose real-world
#   use IS a run: the emoji tag sequences.
#
#     the England flag (U+1F3F4 + "gbeng" + U+E007F, 28 bytes):
#       libc++ charges   2 columns  (one cluster, width of its first)
#       the arc's FULL   2 columns  (today's ONE rule -- correct)
#       the arc's RESTRICTED table charges 8 columns, and writes a
#       30-BYTE field where the stdlib writes a 36-byte one.
#
#   The lesson: a restricted table is exactly right about its corpus
#   (7 of libc++'s 2,198 Extend code points) and wrong about the other
#   99.7% -- and a one-candidate battery can only ever show a ONE-column
#   error, however wide the hole is.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind (4 kinds).
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - Oct 27           the contract comparison against std::format_to_n.
#   - Oct 28           the ERROR TAXONOMY -- a constexpr grammar scanner
#                      plus a libc++-specific message table.
#   - Oct 30           the TYPE-AWARE scanner -- the argument's category
#                      decides the spec-vs-argument class.
#   - Nov 7            the POLICY TABLE -- one mask table plus
#                      per-implementation override rows.
#   - Nov 12           the CARRIED VERDICT -- the scanner's silence is
#                      carried into the fallback as silence, and the
#                      numeric ceiling joins the policy table.
#   - Nov 13           the UNICODE FILL RULE -- the fill is decoded as a
#                      scalar value; the ill-formed fill is structural.
#   - Nov 14           the COMBINATION GRID -- the 21 PAIRS of options,
#                      and the align/zero rule that only exists where
#                      two options meet.
#   - Nov 15           the TRIPLE GRID -- the 35 TRIPLES of options, and
#                      the ONE cell where the fill's rule and the
#                      align/zero rule meet.
#   - Nov 16           the FILL BEHAVIOUR SPACE -- the fill's enforcement
#                      as ONE datum, and the first grid in the arc whose
#                      axis is the POLICY rather than the spec.
#   - Nov 17           the FIELD WIDTH -- the width's UNIT, measured
#                      against the linked stdlib: bytes agree on 1 of 28
#                      cells, code points on 11 of 28, COLUMNS on 28 of 28.
#   - Nov 18           the PRECISION -- the same unit, the same table,
#                      and a cluster-boundary cut.  Plus UAX #29's GB11,
#                      which the arc's own ZWJ rule got wrong.
#   - Nov 19           the ONE COLUMN RULE -- the two rules become one
#                      function, the defective rule is deleted, and the
#                      ONE rule gains GB9a, GB9b, GB9c, GB12/GB13 and
#                      GB6/GB7/GB8.
#   - Nov 20           the RULE ORDER -- UAX #29's rules are an ORDERED
#                      list and a rule CONSUMES its context: GB9c comes
#                      BEFORE GB11, and the conjunct spends its Linker.
#   - Nov 21           the CONTROL CHARACTERS -- GB3, GB4 and GB5, in
#                      UAX #29's order (before GB9), plus libc++ 21's
#                      ASCII FAST PATH, which charges an ASCII prefix
#                      per CODE UNIT.
#   - Nov 22           the WIDTH TABLE -- the arc's restricted table was
#                      not a restriction of libc++'s full one; it was a
#                      different table, and the two disagree on 8,493
#                      code points.  The arc now carries libc++'s FULL
#                      table, and the table's PROVENANCE is pinned to
#                      Unicode 16.0.0's EastAsianWidth.txt.
#   - Nov 23           the PROPERTY TABLES -- the same census over the
#                      arc's FOUR remaining restricted tables, with
#                      libc++'s full extended grapheme cluster table
#                      (1,501 entries) and Indic conjunct break table
#                      (403 entries) carried verbatim.  Three of the
#                      four are not restrictions at all; the fourth is
#                      one code point short; and all four of libc++'s
#                      tables are Unicode 16.0.0's.
#   - Nov 24           the EXTEND TABLE -- the sixth restricted table,
#                      and the FIRST one whose error makes the arc
#                      charge TOO MUCH rather than too little.
#   - TODAY            the TAGS BLOCK -- the widest gap of all, and the
#                      first battery drawn from a RUN rather than from a
#                      single code point.
#
# What today measures (see the .md for the detail):
#
#   - The census: re-measured and unmoved (573 / 2,198 / 49 / 1,674).
#   - The Tags block: all 96 code points are libc++-Extend, none are
#     arc-Extend, and it IS the widest single interval of the
#     disagreement.
#   - The CORPUS AUDIT: the arc's own battery cells contain ZERO Tags
#     code points, and the restricted table is right about every code
#     point the corpus does contain.
#   - The RUN battery: "a" + N tags, two models, one oracle.  The
#     restricted table's error is EXACTLY the length of the run -- 1, 2,
#     3, ... 8 -- where a one-candidate cell could only ever show one.
#   - The real-world payload: the England flag, and the blast radius in
#     BYTES (a padded field and a precision cut).
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-25-tags-block
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-25-tags-block

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-25-tags-block
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

add_executable(P-2026-11-25-tags-block
               P-2026-11-25-tags-block.cpp)
