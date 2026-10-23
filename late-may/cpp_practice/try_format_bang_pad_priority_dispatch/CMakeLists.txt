# CMakeLists.txt — today's P-2026-10-23 lesson:
#
#   try_format_bang_pad_priority_dispatch — the ELEVENTH pad
#   (PETRA_BANG_PAD, '!', ASCII 33), added to prove the Oct 22
#   priority-chain SHAPE generalises rather than just the nine pads
#   it happened to be written for.
#
# Where this fits in the arc
# --------------------------
#   - Sep 27 .. Oct 21  eleven lessons, one pad each, four
#                      hand-written sibling wrappers per axis;
#                      composition described but unreachable.
#   - Oct 22            the generic `template <bool Fills, char Fill>`
#                      wrapper family plus the preprocessor priority
#                      chain; 1024 pad combinations collapse to two
#                      compiled cells.
#   - TODAY             one more pad, added the way Oct 22 said it
#                      should be added, and the cost of adding it
#                      MEASURED rather than asserted.  Also builds
#                      the runtime selector Oct 22 listed as
#                      next-step item (3), which turns the 1024-cell
#                      external compile sweep into a single in-process
#                      loop.
#
# KEY IDEA: appending a pad at the TOP of the rank order renumbers
# nothing, because the score is `rank + 1`.  Adding BANG (rank 10)
# means BANG scores 11 and no existing score moves — verified by
# compiling yesterday's source and today's under all 512 shared flag
# combinations and comparing the reported (name, fills, byte, score).
#
# The eleven pad flags are legal in any combination.  Each maps to a
# rank (NUL=1 ... BANG=10); the highest-ranked enabled flag wins.
#
cmake_minimum_required(VERSION 3.20)
project(P-2026-10-23-try-format-bang-pad-priority-dispatch
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# ---------------------------------------------------------------------------
# The eleven pad flags.  ANY combination is legal.  Each maps to a rank
# (NUL=1 .. BANG=10); the highest-ranked enabled flag wins the chain.
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

add_executable(P-2026-10-23-try-format-bang-pad-priority-dispatch
               P-2026-10-23-try-format-bang-pad-priority-dispatch.cpp)