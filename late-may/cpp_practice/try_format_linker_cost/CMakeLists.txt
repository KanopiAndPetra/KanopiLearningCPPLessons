# CMakeLists.txt -- today's P-2026-11-28 lesson:
#
#   try_format_linker_cost -- what do those 6 code points COST?
#
#   Closes Nov 27's item (26a): "kIndicLinker is the one table that does
#   not earn its keep -- so should it be DELETED?  It is right about 6
#   code points and wrong about 9 ... Worth: asking what those 6 code
#   points are, what they cost, and whether a lesson that keeps a table
#   it has measured to be net-harmful is teaching something true or
#   teaching something comfortable."
#
#   Nov 27's ledger counted CODE POINTS and called the table net-harmful
#   (-3).  Today's lesson counts COLUMNS, because a linker is what lets
#   a conjunct FORM, and the two counts do not agree:
#
#     the arc's model, WITH the table     6 conjuncts right,  9 wrong
#     the arc's model, WITHOUT the table   1 conjunct right, 14 wrong
#
#   So deleting the table makes the arc's model WORSE, not better: the
#   code-point ledger measured the wrong unit.  And the -3 was a VERSION:
#   libc++ 21 carries Unicode 16.0.0's tables, and 2 of the 9
#   "over-claims" ARE Linkers in Unicode 17.0.0.
#
#   The table is an EXHIBIT (the product reads libc++'s tables), so the
#   lesson keeps it -- and today measures what it exhibits.
#
# Where this fits in the arc
# --------------------------
#   - Nov 22           the WIDTH TABLE -- the arc's restricted table is
#                      not a restriction of libc++'s full one.
#   - Nov 23           the PROPERTY TABLES -- the same census over the
#                      arc's FOUR remaining restricted tables.
#   - Nov 24           the EXTEND TABLE -- the sixth restricted table.
#   - Nov 25           the TAGS BLOCK -- the widest gap of all.
#   - Nov 27           the VALUE OF A RESTRICTED TABLE -- delete each
#                      of the six and measure what the deletion is
#                      worth.  Five earn their keep; kIndicLinker does
#                      NOT (right 6, wrong 9, net -3).
#   - TODAY            THE COST OF THE 6 -- the same table, measured in
#                      COLUMNS instead of code points, against TWO
#                      Unicode versions instead of one.
#
# What today measures (see the .md for the detail):
#
#   - THE 15, one row per code point: what the arc's table says, what
#     libc++ 21 says, what Unicode 16.0.0 says, what Unicode 17.0.0
#     says, and whether the code point is Extend.
#   - THE COST IN COLUMNS: "consonant + linker + consonant", charged
#     four ways -- the linked stdlib, the ONE rule, the arc's old model,
#     and the arc's old model with the linker table DELETED.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-28-linker-cost
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-28-linker-cost

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-28-linker-cost
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

add_executable(P-2026-11-28-linker-cost
               P-2026-11-28-linker-cost.cpp)
