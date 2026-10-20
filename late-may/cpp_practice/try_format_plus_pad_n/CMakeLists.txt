# CMakeLists.txt — today's P-2026-10-20 lesson:
#
#   try_format_plus_pad_n — the CHAR*+CAP × BOUNDED-BUFFER
#   × expected-vs-throwing TENTH axis (PETRA_PLUS_PAD).
#   TODAY introduces a NEW TENTH compile-time flag
#   (PETRA_PLUS_PAD) on top of Oct 18's 9-bit tuple
#   (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
#   PETRA_DASH_PAD, PETRA_HASH_PAD, PETRA_TILDE_PAD,
#   PETRA_UNDERSCORE_PAD, PETRA_STAR_PAD, PETRA_AT_PAD) on
#   the CHAR*+CAP × BOUNDED-BUFFER axis.  PETRA_PLUS_PAD=1
#   selects the PLUS-padded char*+cap sibling, which fills
#   bytes [written, cap) with '+' (ASCII 43) on success.
#
# Closes Oct 19's "Where we go next" item (1) verbatim — the
# CHAR*+CAP-side TENTH axis, picking '+' (ASCII 43) as a
# TOTALLY DIFFERENT pad character from Oct 18's '@'
# (ASCII 64), Oct 17's '*' (ASCII 42), Oct 14's '_'
# (ASCII 95), Oct 12's '~' (ASCII 126), Oct 9's '#'
# (ASCII 35), Oct 3's '-' (ASCII 45), Sep 28's ' ' (ASCII
# 32), and Sep 25's '\0' (ASCII 0).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time flag
# (PETRA_PLUS_PAD) on top of the Oct 18 / Oct 17 / Oct 14 /
# Oct 12 / Oct 9 / Oct 3 / Sep 28 / Sep 25 helpers' pad-free
# axes.  When PETRA_PAD = 1, PETRA_SPACE_PAD = 1,
# PETRA_DASH_PAD = 1, PETRA_HASH_PAD = 1, PETRA_TILDE_PAD = 1,
# PETRA_UNDERSCORE_PAD = 1, PETRA_STAR_PAD = 1, or
# PETRA_AT_PAD = 1, the lesson's friendly-error fires at
# compile time with a clear pointer to those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Sep 25   try_format_padded_n_or_into — the FIRST pad axis
#              (PETRA_PAD, NUL-fill) on the CHAR*+CAP side.
#   - Sep 28   try_format_space_pad_n — (PETRA_SPACE_PAD) sibling
#              wrapper on the CHAR*+CAP side.
#   - Oct  3   try_format_dash_pad_n — (PETRA_DASH_PAD) sibling
#              wrapper on the CHAR*+CAP side.
#   - Oct  9   try_format_hash_pad_n — (PETRA_HASH_PAD) sibling
#              wrapper on the CHAR*+CAP side.
#   - Oct 12   try_format_tilde_pad_n — (PETRA_TILDE_PAD) sibling
#              wrapper on the CHAR*+CAP side.
#   - Oct 14   try_format_underscore_pad_n — (PETRA_UNDERSCORE_PAD)
#              sibling wrapper on the CHAR*+CAP side.
#   - Oct 17   try_format_star_pad_n — (PETRA_STAR_PAD) sibling
#              wrapper on the CHAR*+CAP side ('*', ASCII 42).
#   - Oct 18   try_format_at_pad_n — (PETRA_AT_PAD) sibling
#              wrapper on the CHAR*+CAP side ('@', ASCII 64).
#              TODAY's layer sits directly on top of it.
#   - Oct 19   try_format_at_pad_span_n — Oct 18's
#              STD::SPAN<CHAR> mirror; its "Where we go next"
#              item (1) is the TENTH pad character, which is
#              exactly what today closes on the CHAR*+CAP side.
#   - TODAY    try_format_plus_pad_n — the TENTH axis
#              (PETRA_PLUS_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.  Today's pad character is
#              '+' (ASCII 43).
#
# KEY FINDING #1: the PLUS-pad char*+cap sibling is a THIN
# LAYER on top of Oct 18's AT-pad char*+cap sibling (it calls
# Oct 18's wrapper, then OVERWRITES the '@' fill with '+' on
# success).  The composition rule "the LATER fill overwrites
# the EARLIER fill" now has NINE layers:
# PLUS > AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE
# > NUL.  When all nine pad-flags are set, the end byte state
# is always '+'.
#
# KEY FINDING #2: '+' (ASCII 43) is ADJACENT to '*' (ASCII
# 42), Oct 17's pad byte.  An off-by-one literal typo is
# invisible in review and in a casual buffer eyeball, so
# today's Section 5 pins the literal codes with static_assert
# AND sweeps the runtime byte state for all nine prior pad
# bytes plus the poison byte.
#
# KEY FINDING #3: today's Section 9 calls
# TRY_FORMAT_BOUNDED_N_PLUS_OR_THROW for real in ALL FOUR
# cells, going through petra::unwrap_dispatch's if constexpr
# adapter so ONE call site compiles against BOTH the
# std::expected surface and the throwing std::size_t surface.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-20-try-format-plus-pad-n
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# PETRA_THROW_ON_OVERFLOW controls the dispatch between expected sibling
# (returns expected<size_t, FormatError>) and throwing sibling
# (returns std::size_t, throws std::runtime_error on overflow).
option(PETRA_THROW_ON_OVERFLOW
       "0 = expected sibling; 1 = throwing sibling"
       OFF)
if(PETRA_THROW_ON_OVERFLOW)
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=1)
else()
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=0)
endif()

# PETRA_PLUS_PAD controls the dispatch between untouched sibling
# (bytes [written, cap) UNTOUCHED on success — the prior contract from
# Sep 8 / Sep 9 / Sep 20) and PLUS-padded sibling (bytes
# [written, cap) filled with '+' on success).  When
# PETRA_PLUS_PAD=1, the PLUS fill OVERWRITES the AT fill from
# Oct 18's wrapper (composition rule — same pattern Oct 18
# established on the AT-overwrites-STAR axis).
option(PETRA_PLUS_PAD
       "0 = no plus-pad; 1 = PLUS-padded sibling"
       OFF)
if(PETRA_PLUS_PAD)
    add_compile_options(-DPETRA_PLUS_PAD=1)
else()
    add_compile_options(-DPETRA_PLUS_PAD=0)
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

add_executable(P-2026-10-20-try-format-plus-pad-n
               P-2026-10-20-try-format-plus-pad-n.cpp)