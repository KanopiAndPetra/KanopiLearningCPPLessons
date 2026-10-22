# CMakeLists.txt — today's P-2026-10-22 lesson:
#
#   try_format_pad_priority_dispatch — the UNIFIED pad dispatcher,
#   built as a PREPROCESSOR PRIORITY CHAIN rather than an exhaustive
#   2^10 (= 1024) #if table.
#
# Where this fits in the arc
# --------------------------
#   - Sep 27 .. Oct 21  eleven lessons, each adding ONE pad character
#                      (NUL, SPACE, DASH, HASH, TILDE, UNDERSCORE,
#                      STAR, AT, PLUS) with FOUR hand-written sibling
#                      wrappers per axis.  Composition was described
#                      but unreachable: each lesson #error'd when a
#                      second pad flag was set.
#   - TODAY            the composition cells become REACHABLE.  One
#                      generic `template <bool Fills, char Fill>`
#                      wrapper family plus a max()-over-scores
#                      priority chain selects the highest-ranked
#                      enabled pad at compile time, so 1024 pad
#                      combinations collapse to TWO compiled cells
#                      (expected vs throwing).
#
# KEY IDEA:  the pad precedence is a STRICT TOTAL ORDER
# (PLUS > AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL),
# so "highest-ranked enabled flag" is just a max() over ten integer
# scores — not a lookup table.  The preprocessor chain (MAX2 folded
# four times) and an INDEPENDENT constexpr C++ chain are pinned
# against each other with static_asserts in every build.
#
# Ten pad flags are now LEGAL in any combination (no friendly-#error
# cells any more — that restriction was the whole point of closing).
#
cmake_minimum_required(VERSION 3.20)
project(P-2026-10-22-try-format-pad-priority-dispatch
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# ---------------------------------------------------------------------------
# The ten pad flags.  ANY combination is legal — that is today's headline.
# Each maps to a rank (NUL=1 ... PLUS=9); the highest-ranked enabled flag
# wins the priority chain.
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

foreach(flag
        PETRA_PAD PETRA_SPACE_PAD PETRA_DASH_PAD PETRA_HASH_PAD
        PETRA_TILDE_PAD PETRA_UNDERSCORE_PAD PETRA_STAR_PAD
        PETRA_AT_PAD PETRA_PLUS_PAD)
    if(${flag})
        add_compile_options(-D${flag}=1)
    else()
        add_compile_options(-D${flag}=0)
    endif()
endforeach()

# PETRA_THROW_ON_OVERFLOW controls the dispatch between the expected
# sibling (returns expected<size_t, FormatError>) and the throwing
# sibling (returns std::size_t, throws std::runtime_error on overflow).
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

add_executable(P-2026-10-22-try-format-pad-priority-dispatch
               P-2026-10-22-try-format-pad-priority-dispatch.cpp)