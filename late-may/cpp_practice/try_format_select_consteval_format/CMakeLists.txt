# CMakeLists.txt — today's P-2026-10-26 lesson:
#
#   try_format_select_consteval_format — the missing cell of the arc's
#   2x2 matrix: a RUNTIME pad SELECTION with a COMPILE-TIME format
#   CHECK.
#
# Where this fits in the arc
# --------------------------
#   - Sep 27 .. Oct 21  eleven lessons, one pad each.
#   - Oct 22            the generic `template <bool Fills, char Fill>`
#                      wrapper family, and the preprocessor priority
#                      chain.
#   - Oct 23            the ELEVENTH pad (BANG); the runtime selector.
#   - Oct 24            the rank order as DATA; a runtime fill loop.
#   - Oct 25            the buffer concept; the raw axis ported onto
#                      the generic wrapper.
#   - TODAY             the FOURTH cell of the matrix:
#
#                        axis A: how the PAD is chosen
#                                  compile-time  -> <bool Fills, char Fill>
#                                  runtime       -> a PadChoice VALUE
#                        axis B: how the FORMAT STRING is obtained
#                                  compile-time  -> std::format_string<Args...>
#                                  runtime       -> std::string_view
#
#                        cell [4] = runtime pad + compile-time format
#
#                      The three other cells already existed (Oct 22,
#                      Oct 19 and Oct 24); today's cell is the one that
#                      was still empty.
#
# Why it is NOT a one-line alias:
#
#   - `std::format_string<Args...>` is NOT a structural type, so it
#     cannot be a non-type template parameter -- and neither can
#     `std::string_view`.  The format string therefore has to arrive as
#     a FUNCTION PARAMETER, and the compile-time check happens at the
#     call site.
#   - The check lives in a `consteval` constructor body, which a
#     requires-expression never evaluates -- so it is a HARD ERROR, not
#     a queryable constraint.
#   - A compile-time-PASSING format string can still throw
#     `std::format_error` at runtime, because the argument's FORMATTER
#     is not checked at compile time.  The FormatErrorKind::Format path
#     therefore stays alive on the consteval surface.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-26-try-format-select-consteval-format
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# ---------------------------------------------------------------------------
# The ten pad flags.  ANY combination is legal.  Each maps to a rank
# (NUL=1 .. BANG=10); the highest-ranked enabled flag wins the chain.
# The flag NAMES and the -D contract are unchanged from Oct 22 onwards,
# so yesterday's build cells keep their meaning.
# ---------------------------------------------------------------------------
option(PETRA_PAD          "0 = no NUL-pad;       1 = NUL-padded"       OFF)
option(PETRA_SPACE_PAD    "0 = no space-pad;    1 = space-padded"    OFF)
option(PETRA_DASH_PAD     "0 = no dash-pad;     1 = dash-padded"     OFF)
option(PETRA_HASH_PAD     "0 = no hash-pad;     1 = hash-padded"     OFF)
option(PETRA_TILDE_PAD    "0 = no tilde-pad;    1 = tilde-padded"    OFF)
option(PETRA_UNDERSCORE_PAD "0 = no _-pad;      1 = underscore-padded" OFF)
option(PETRA_STAR_PAD     "0 = no star-pad;     1 = star-padded"     OFF)
option(PETRA_AT_PAD       "0 = no at-pad;       1 = at-padded"       OFF)
option(PETRA_PLUS_PAD     "0 = no plus-pad;     1 = plus-padded"     OFF)
option(PETRA_BANG_PAD     "0 = no bang-pad;     1 = bang-padded"     OFF)

foreach(flag
        PETRA_PAD PETRA_SPACE_PAD PETRA_DASH_PAD PETRA_HASH_PAD
        PETRA_TILDE_PAD PETRA_UNDERSCORE_PAD PETRA_STAR_PAD
        PETRA_AT_PAD PETRA_PLUS_PAD PETRA_BANG_PAD)
    if(${flag})
        add_compile_options(-D${flag}=1)
    else()
        add_compile_options(-D${flag}=0)
    endif()
endforeach()

# PETRA_THROW_ON_OVERFLOW controls the dispatch between the expected
# sibling (returns expected<size_t, FormatError>) and the bare
# std::size_t throwing surface (THROW=1).
option(PETRA_THROW_ON_OVERFLOW
       "0 = expected sibling; 1 = throwing sibling"
       OFF)
if(PETRA_THROW_ON_OVERFLOW)
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=1)
else()
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=0)
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
    add_link_options(
        -fsanitize=address -fsanitize=undefined)
endif()

add_executable(P-2026-10-26-try-format-select-consteval-format
               P-2026-10-26-try-format-select-consteval-format.cpp)
