# CMakeLists.txt — today's P-2026-10-30 lesson:
#
#   try_format_type_aware_spec — replacing libc++'s MESSAGE TEXT with the
#   argument's TYPE CATEGORY for the whole spec-vs-argument class.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind (4 kinds).
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - Oct 27           the contract comparison against std::format_to_n.
#   - Oct 28           the ERROR TAXONOMY — a two-stage classifier: a
#                      constexpr GRAMMAR scanner (Stage A) plus a
#                      libc++-specific MESSAGE TABLE (Stage B), with a
#                      safe fallback.
#   - TODAY            the TYPE-AWARE scanner — Oct 28's item (2),
#                      closed.  The message table is demoted to a
#                      fallback; the argument's CATEGORY decides the
#                      spec-vs-argument class instead.
#
# What today measures (see the .md for the detail):
#
#   - libc++ validates a format spec in TWO phases: phase 1 picks the
#     mask of options the parser will CONSUME (the mask is decided by
#     the argument's CATEGORY), phase 2 lets the SELECTED PRESENTATION
#     TYPE narrow that mask further.
#   - Oct 28's premise was wrong: the runtime path DOES have the
#     argument categories.  std::make_format_args erases them, but the
#     erasure happens DOWNSTREAM of the arc's own template, so the
#     category of every argument is a compile-time fact.
#   - Over a 1,596-cell grid (7 categories x 228 specs) the two-phase
#     model predicts libc++ 21's accept/reject decision 1596/1596.
#   - The standard-derived model agrees with libstdc++ 15 on 1596/1596
#     and with libc++ on 1586/1596 — the 10-cell difference is a known
#     upstream libc++ bug (llvm/llvm-project#106136, microsoft/STL#4918).
#   - The type-aware scanner classifies 1172/1172 of the grid's rejects
#     STRUCTURALLY, reading zero libc++ messages.
#
# This lesson needs NO pad flags: the pad axis is orthogonal to the
# error taxonomy, so the pad options are NOT reproduced here.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-30-try-format-type-aware-spec
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

add_executable(P-2026-10-30-try-format-type-aware-spec
               P-2026-10-30-try-format-type-aware-spec.cpp)
