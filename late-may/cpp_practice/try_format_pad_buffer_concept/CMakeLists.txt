# CMakeLists.txt — today's P-2026-10-25 lesson:
#
#   try_format_pad_buffer_concept — the `char*`+`cap` axis ported onto the
#   SAME generic wrapper as the `std::span<char>` axis, via a C++20
#   concept over the output buffer.
#
# Where this fits in the arc
# --------------------------
#   - Sep 27 .. Oct 21  eleven lessons, one pad each; the `char*`+`cap`
#                      axis grew FOUR hand-written sibling wrappers per
#                      pad, while the `std::span<char>` axis grew its own
#                      separate set.
#   - Oct 22            the generic `template <bool Fills, char Fill>`
#                      wrapper family, on the span axis ONLY.
#   - Oct 23            the ELEVENTH pad (BANG); the runtime selector.
#   - Oct 24            the rank order as DATA; a runtime fill loop.
#   - TODAY             the OTHER axis gets the same generic wrapper.
#                      The two axes differed only in how the
#                      (pointer, length) pair was SPELLED, so the port
#                      is really about NAMING the pair as a type — which
#                      in C++20 means a concept:
#
#                        petra::CharBuffer
#                          = view && contiguous_range && sized_range
#                            && range_value_t == char
#                            && range_reference_t == char&
#
#                      `std::span<char>` satisfies it as-is; the raw axis
#                      gets `petra::raw_buffer`, which joins via one
#                      `std::ranges::enable_view` specialization.
#
cmake_minimum_required(VERSION 3.20)
project(P-2026-10-25-try-format-pad-buffer-concept
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

add_executable(P-2026-10-25-try-format-pad-buffer-concept
               P-2026-10-25-try-format-pad-buffer-concept.cpp)
