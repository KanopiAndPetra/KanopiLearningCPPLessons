# CMakeLists.txt — today's P-2026-11-07 lesson:
#
#   try_format_policy_table — one mask table plus per-implementation
#   override rows, so adding a third implementation is a DATA change
#   rather than a code change.
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
#   - TODAY            the POLICY TABLE — Oct 30's next-step item (2),
#                      closed: the two policies (the standard's rules
#                      and libc++ 21's rules) collapse into one mask
#                      table plus a per-implementation override table.
#
# What today measures (see the .md for the detail):
#
#   - The two policies differ in EXACTLY ONE CELL of a 7 x 19
#     (category x presentation type) matrix: (integral, 'c').
#   - The new table is checked against Oct 30's five mask functions on
#     every one of 266 cells (7 categories x 19 types x 2 policies),
#     plus the 7 consume masks.  Zero mismatches.
#   - The override table is DERIVED from a measurement of the linked
#     stdlib over a 1,596-cell grid, then checked against the
#     hand-written table.  Derived: libc++ 21 -> 1 row; libstdc++ 15
#     -> 0 rows (which is the machine-checked form of "libstdc++
#     implements the standard").
#   - The 10 cells where the two POLICIES disagree are the SAME 10
#     under either toolchain.  Which side rejects them flips with the
#     build; WHICH cells they are does not.
#
# This lesson needs NO pad flags: the pad axis is orthogonal to the
# policy table, so the pad options are not reproduced here.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-07-try-format-policy-table
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-07-try-format-policy-table

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-07-try-format-policy-table
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

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

add_executable(P-2026-11-07-try-format-policy-table
               P-2026-11-07-try-format-policy-table.cpp)
