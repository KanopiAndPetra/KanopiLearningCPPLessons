# CMakeLists.txt — today's P-2026-10-16 lesson:
#
#   try_format_star_pad_span_n — the STD::SPAN<CHAR> ×
#   BOUNDED-BUFFER × expected-vs-throwing EIGHTH pad-character
#   axis (PETRA_STAR_PAD).  TODAY introduces a NEW EIGHTH
#   compile-time flag (PETRA_STAR_PAD) on top of Oct 15's
#   7-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#   PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD,
#   PETRA_UNDERSCORE_PAD, PETRA_TILDE_PAD) on the STD::SPAN<CHAR>
#   × BOUNDED-BUFFER axis.  PETRA_STAR_PAD=1 selects the
#   STAR-padded span<char> sibling, which fills bytes
#   [written, span.size()) with '*' (ASCII 42) on success.
#
# Closes Oct 15's "Where we go next" item (1) verbatim — the
# STD::SPAN<CHAR>-side EIGHTH axis, picking '*' (ASCII 42) as a
# TOTALLY DIFFERENT pad character from Oct 15's '_' (ASCII 95),
# Oct 13's '~' (ASCII 126), Oct 11's '#' (ASCII 35), Oct 4's '-'
# (ASCII 45), Sep 28's ' ' (ASCII 32), and Sep 27's '\0' (ASCII 0).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time flag
# (PETRA_STAR_PAD) on top of the Oct 15 / Oct 13 / Oct 11 / Oct 4 /
# Sep 28 / Sep 27 helpers' pad-free axes.  When PETRA_PAD = 1,
# PETRA_SPACE_PAD = 1, PETRA_DASH_PAD = 1, PETRA_HASH_PAD = 1,
# PETRA_UNDERSCORE_PAD = 1, or PETRA_TILDE_PAD = 1, the lesson's
# friendly-error fires at compile time with a clear pointer to
# those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Oct 11   try_format_hash_pad_span_n — the EIGHTH axis
#              (PETRA_HASH_PAD) on the STD::SPAN<CHAR> ×
#              BOUNDED-BUFFER axis (same shape as today).
#   - Oct 13   try_format_tilde_pad_span_n — the NINTH axis
#              (PETRA_TILDE_PAD) on the STD::SPAN<CHAR> ×
#              BOUNDED-BUFFER axis.
#   - Oct 14   try_format_underscore_pad_n — the TENTH axis
#              (PETRA_UNDERSCORE_PAD) on the CHAR*+CAP side.
#   - Oct 15   try_format_underscore_pad_span_n — the SEVENTH
#              pad-character axis (PETRA_UNDERSCORE_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - TODAY    try_format_star_pad_span_n — the EIGHTH
#              pad-character axis (PETRA_STAR_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.  Closes
#              Oct 15's "Where we go next" item (1) verbatim.
#
# KEY FINDING: the STAR-pad span<char> sibling is a THIN LAYER
# on top of Oct 15's UNDERSCORE-pad span<char> sibling (a
# second-level wrapper — today's STAR-pad sibling wraps Oct
# 15's UNDERSCORE-pad sibling, not the Sep 10 / Sep 19 / Sep 21 /
# Sep 27 / Sep 28 / Oct 4 / Oct 11 / Oct 13 / Oct 15 underlying
# impls directly).
#
# The composition rule "the LATER fill overwrites the EARLIER
# fill" now has SEVEN layers:
# STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL.  When
# all seven pad-flags are set, the end byte state is always '*'.
# Composition cells with (STAR=1, UNDERSCORE=*) REUSE TODAY's NEW
# STAR-pad wrappers verbatim.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-16-try-format-star-pad-span-n
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

# PETRA_STAR_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior
# contract from Sep 10 / Sep 19 / Sep 21) and STAR-padded sibling
# (bytes [written, span.size()) filled with '*' on success).
# When PETRA_STAR_PAD=1 with PETRA_UNDERSCORE_PAD=1 (a future
# lesson's composition cell), the STAR fill OVERWRITES the
# UNDERSCORE fill (composition rule — same pattern Oct 15
# established on the UNDERSCORE-overwrites-TILDE axis).
option(PETRA_STAR_PAD
       "0 = no star-pad; 1 = STAR-padded sibling"
       OFF)
if(PETRA_STAR_PAD)
    add_compile_options(-DPETRA_STAR_PAD=1)
else()
    add_compile_options(-DPETRA_STAR_PAD=0)
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

add_executable(P-2026-10-16-try-format-star-pad-span-n
               P-2026-10-16-try-format-star-pad-span-n.cpp)
