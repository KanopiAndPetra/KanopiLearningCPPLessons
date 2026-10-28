# CMakeLists.txt — today's P-2026-10-28 lesson:
#
#   try_format_error_taxonomy — reviving petra::FormatErrorKind's two
#   DEAD kinds (FormatParse and FormatArgument), which no code path
#   could produce since Sep 6.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind (4 kinds).
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - Oct 27           the contract comparison against std::format_to_n.
#   - TODAY            the ERROR TAXONOMY — the stdlib has no
#                      structural error class (one exception type, 43
#                      distinct messages), so the arc grows its own
#                      two-stage classifier: a constexpr grammar
#                      scanner (Stage A) plus a libc++-specific message
#                      table (Stage B), with a SAFE FALLBACK.
#
# What today measures (see the .md for the detail):
#
#   - std::format_error is the ONLY type the format machinery throws:
#     no error_code, no subclass, 43 distinct message strings.
#   - The arc's classifier is TWO-STAGE: the grammar scanner decides
#     FormatParse; the message table decides FormatArgument; anything
#     else falls back to Format (the arc's pre-existing catch-all).
#   - The two classifiers are INDEPENDENT and they DISAGREE — because
#     libc++'s format-spec parser is TYPE-DIRECTED, so some of its
#     "parse" errors are really argument errors.
#   - The compile-time path has a NARROWER taxonomy: a FormatParse is
#     unreachable there, because std::format_string's consteval
#     constructor already rejected every malformed string.
#
# This lesson needs NO pad flags: the pad axis is orthogonal to the
# error taxonomy, so the pad options are NOT reproduced here.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-28-try-format-error-taxonomy
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

add_executable(P-2026-10-28-try-format-error-taxonomy
               P-2026-10-28-try-format-error-taxonomy.cpp)
