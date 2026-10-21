# CMakeLists.txt — today's P-2026-10-21 lesson:
#
#   try_format_plus_pad_span_n — the STD::SPAN<CHAR> ×
#   BOUNDED-BUFFER × expected-vs-throwing TENTH pad-character
#   axis (PETRA_PLUS_PAD).  TODAY introduces a NEW TENTH
#   compile-time flag (PETRA_PLUS_PAD) on top of Oct 19's
#   9-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#   PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD,
#   PETRA_TILDE_PAD, PETRA_UNDERSCORE_PAD, PETRA_STAR_PAD,
#   PETRA_AT_PAD) on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   PETRA_PLUS_PAD=1 selects the PLUS-padded span<char> sibling,
#   which fills bytes [written, span.size()) with '+' (ASCII 43)
#   on success.
#
# Closes Oct 20's "Where we go next" item (1) verbatim — the
# STD::SPAN<CHAR>-side TENTH axis, picking '+' (ASCII 43) as a
# TOTALLY DIFFERENT pad character from Oct 19's '@' (ASCII 64),
# Oct 16's '*' (ASCII 42), Oct 15's '_' (ASCII 95), Oct 13's
# '~' (ASCII 126), Oct 11's '#' (ASCII 35), Oct 4's '-' (ASCII 45),
# Sep 28's ' ' (ASCII 32), and Sep 27's '\0' (ASCII 0).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time flag
# (PETRA_PLUS_PAD) on top of the Oct 19 / Oct 16 / Oct 15 / Oct 13 /
# Oct 11 / Oct 4 / Sep 28 / Sep 27 helpers' pad-free axes.  When
# PETRA_PAD = 1, PETRA_SPACE_PAD = 1, PETRA_DASH_PAD = 1,
# PETRA_HASH_PAD = 1, PETRA_UNDERSCORE_PAD = 1, PETRA_TILDE_PAD = 1,
# PETRA_STAR_PAD = 1, or PETRA_AT_PAD = 1, the lesson's
# friendly-error fires at compile time with a clear pointer to
# those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Oct 11   try_format_hash_pad_span_n — the SEVENTH axis
#              (PETRA_HASH_PAD) on the STD::SPAN<CHAR> ×
#              BOUNDED-BUFFER axis (same shape as today).
#   - Oct 13   try_format_tilde_pad_span_n — the SEVENTH axis
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
#              BOUNDED-BUFFER axis.
#   - Oct 19   try_format_at_pad_span_n — the NINTH
#              pad-character axis (PETRA_AT_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.  Today's
#              PLUS-pad wrapper sits directly on top of it.
#   - Oct 20   try_format_plus_pad_n — the TENTH pad-character
#              axis (PETRA_PLUS_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.  TODAY's lesson is the
#              STD::SPAN<CHAR> MIRROR of Oct 20.
#   - TODAY    try_format_plus_pad_span_n — the TENTH
#              pad-character axis (PETRA_PLUS_PAD) on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#              Closes Oct 20's "Where we go next" item (1)
#              verbatim — the STD::SPAN<CHAR>-side mirror of
#              Oct 20's CHAR*+CAP TENTH-axis lesson.  Today's
#              pad character is '+' (ASCII 43), TOTALLY
#              DIFFERENT from Oct 19's '@' (ASCII 64), Oct 16's
#              '*' (ASCII 42), Oct 15's '_' (ASCII 95), Oct 13's
#              '~' (ASCII 126), Oct 11's '#' (ASCII 35),
#              Oct 4's '-' (ASCII 45), Sep 28's ' ' (ASCII 32),
#              and Sep 27's '\0' (ASCII 0).
#
# KEY FINDING: the PLUS-pad span<char> sibling is a THIN LAYER
# on top of Oct 19's AT-pad span<char> sibling (a
# second-level wrapper — today's PLUS-pad sibling wraps Oct
# 19's AT-pad sibling, not the Sep 10 / Sep 19 / Sep 21 /
# Sep 27 / Sep 28 / Oct 4 / Oct 11 / Oct 13 / Oct 15 / Oct 16
# / Oct 19 underlying impls directly).
#
# The composition rule "the LATER fill overwrites the EARLIER
# fill" now has NINE layers:
# PLUS > AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL.
# When all nine pad-flags are set, the end byte state is
# always '+'.  Composition cells with (PLUS=1, AT=*) REUSE
# TODAY's NEW PLUS-pad wrappers verbatim.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-21-try-format-plus-pad-span-n
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
# (bytes [written, span.size()) UNTOUCHED on success — the prior
# contract from Sep 10 / Sep 19 / Sep 21) and PLUS-padded sibling
# (bytes [written, span.size()) filled with '+' on success).
# When PETRA_PLUS_PAD=1 with PETRA_AT_PAD=1 (a future lesson's
# composition cell), the PLUS fill OVERWRITES the AT fill
# (composition rule — same pattern Oct 19 established on the
# AT-overwrites-STAR axis).
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

add_executable(P-2026-10-21-try-format-plus-pad-span-n
               P-2026-10-21-try-format-plus-pad-span-n.cpp)
