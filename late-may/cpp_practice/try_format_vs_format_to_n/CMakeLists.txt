# CMakeLists.txt — today's P-2026-10-27 lesson:
#
#   try_format_vs_format_to_n — the arc's hand-rolled
#   expected<size_t, FormatError> contract, compared against the
#   standard library's std::format_to_n.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind.
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - TODAY            the contract COMPARISON — the arc's error
#                      contract against std::format_to_n, which closes
#                      Oct 26's next-step item (3) and Oct 22's
#                      item (4).
#
# What today measures (see the .md for the detail):
#
#   - std::format_to_n_result has EXACTLY TWO members (`out`, `size`)
#     and NO error channel at all — so the stdlib reports overflow by
#     convention and format errors by throwing.
#   - The arc's "size" is bytes WRITTEN; the stdlib's `size` is bytes
#     that WOULD HAVE BEEN WRITTEN.  They coincide only when the
#     output fits.
#   - The arc's "buffer untouched on error" atomicity is an ARTIFACT
#     of libc++'s buffer selection (a custom output iterator is
#     routed through an ALLOCATING buffer; `char*` is not), not a
#     property of the two-pass design.  The stdlib's format_to_n
#     writes into a `char*` and does NOT have it.
#   - The stdlib's `n` is SIGNED, and a negative `n` is silently
#     accepted as "capacity zero".
#
# This lesson needs NO pad flags: the pad axis is orthogonal to the
# error contract, so the pad options are NOT reproduced here.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-27-try-format-vs-format-to-n
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
    add_link_options(
        -fsanitize=address -fsanitize=undefined)
endif()

add_executable(P-2026-10-27-try-format-vs-format-to-n
               P-2026-10-27-try-format-vs-format-to-n.cpp)
