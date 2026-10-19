# CMakeLists.txt — today's P-2026-10-19 lesson:
#
#   try_format_at_pad_span_n — the STD::SPAN<CHAR> ×
#   BOUNDED-BUFFER × expected-vs-throwing NINTH pad-character
#   axis (PETRA_AT_PAD).  TODAY introduces a NEW NINTH
#   compile-time flag (PETRA_AT_PAD) on top of Oct 16's
#   8-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#   PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD,
#   PETRA_TILDE_PAD, PETRA_UNDERSCORE_PAD, PETRA_STAR_PAD)
#   on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   PETRA_AT_PAD=1 selects the AT-padded span<char> sibling,
#   which fills bytes [written, span.size()) with '@' (ASCII 64)
#   on success.
#
# Closes Oct 18's "Where we go next" item (2) verbatim — the
# STD::SPAN<CHAR>-side NINTH axis, picking '@' (ASCII 64) as a
# TOTALLY DIFFERENT pad character from Oct 16's '*' (ASCII 42),
# Oct 15's '_' (ASCII 95), Oct 13's '~' (ASCII 126), Oct 11's
# '#' (ASCII 35), Oct 4's '-' (ASCII 45), Sep 28's ' ' (ASCII 32),
# and Sep 27's '\0' (ASCII 0).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time flag
# (PETRA_AT_PAD) on top of the Oct 16 / Oct 15 / Oct 13 / Oct 11 /
# Oct 4 / Sep 28 / Sep 27 helpers' pad-free axes.  When
# PETRA_PAD = 1, PETRA_SPACE_PAD = 1, PETRA_DASH_PAD = 1,
# PETRA_HASH_PAD = 1, PETRA_UNDERSCORE_PAD = 1, PETRA_TILDE_PAD = 1,
# or PETRA_STAR_PAD = 1, the lesson's friendly-error fires at
# compile time with a clear pointer to those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Oct 11   try_format_hash_pad_span_n — the EIGHTH axis
#              (PETRA_HASH_PAD) on the STD::SPAN<CHAR> ×
#              BOUNDED-BUFFER axis (same shape as today).
#   - Oct 13   try_format_tilde_pad_span_n — the NINTH axis
#              (PETRA_TILDE_PAD) on the STD::SPAN<CHAR> ×
#              BOUNDED-BUFFER axis.
#   - Oct 15   try_format_underscore_pad_span_n — the SEVENTH
#              pad-character axis (PETRA_UNDERSCORE_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Oct 16   try_format_star_pad_span_n — the EIGHTH
#              pad-character axis (PETRA_STAR_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Oct 18   try_format_at_pad_n — the NINTH pad-character
#              axis (PETRA_AT_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.  Today's lesson is the
#              STD::SPAN<CHAR> MIRROR of Oct 18.
#   - TODAY    try_format_at_pad_span_n — the NINTH
#              pad-character axis (PETRA_AT_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#              Closes Oct 18's "Where we go next" item (2)
#              verbatim — the STD::SPAN<CHAR>-side mirror of
#              Oct 18's CHAR*+CAP NINTH-axis lesson.  Today's
#              pad character is '@' (ASCII 64), TOTALLY
#              DIFFERENT from Oct 16's '*' (ASCII 42), Oct 15's
#              '_' (ASCII 95), Oct 13's '~' (ASCII 126),
#              Oct 11's '#' (ASCII 35), Oct 4's '-' (ASCII 45),
#              Sep 28's ' ' (ASCII 32), and Sep 27's '\0'
#              (ASCII 0).
#
# KEY FINDING: the AT-pad span<char> sibling is a THIN LAYER
# on top of Oct 16's STAR-pad span<char> sibling (a
# second-level wrapper — today's AT-pad sibling wraps Oct
# 16's STAR-pad sibling, not the Sep 10 / Sep 19 / Sep 21 /
# Sep 27 / Sep 28 / Oct 4 / Oct 11 / Oct 13 / Oct 15 / Oct 16
# underlying impls directly).
#
# The composition rule "the LATER fill overwrites the EARLIER
# fill" now has EIGHT layers:
# AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL.
# When all eight pad-flags are set, the end byte state is
# always '@'.  Composition cells with (AT=1, STAR=*) REUSE
# TODAY's NEW AT-pad wrappers verbatim.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-19-try-format-at-pad-span-n
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

# PETRA_AT_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior
# contract from Sep 10 / Sep 19 / Sep 21) and AT-padded sibling
# (bytes [written, span.size()) filled with '@' on success).
# When PETRA_AT_PAD=1 with PETRA_STAR_PAD=1 (a future lesson's
# composition cell), the AT fill OVERWRITES the STAR fill
# (composition rule — same pattern Oct 16 established on the
# STAR-overwrites-UNDERSCORE axis).
option(PETRA_AT_PAD
       "0 = no at-pad; 1 = AT-padded sibling"
       OFF)
if(PETRA_AT_PAD)
    add_compile_options(-DPETRA_AT_PAD=1)
else()
    add_compile_options(-DPETRA_AT_PAD=0)
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

add_executable(P-2026-10-19-try-format-at-pad-span-n
               P-2026-10-19-try-format-at-pad-span-n.cpp)