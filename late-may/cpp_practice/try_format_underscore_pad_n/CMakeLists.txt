# CMakeLists.txt — today's P-2026-10-14 lesson:
#
#   try_format_underscore_pad_n — the CHAR*+CAP × BOUNDED-BUFFER
#   × expected-vs-throwing TENTH axis (PETRA_UNDERSCORE_PAD).
#   TODAY introduces a NEW SEVENTH compile-time flag
#   (PETRA_UNDERSCORE_PAD) on top of Oct 12's 6-bit tuple
#   (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
#   PETRA_DASH_PAD, PETRA_HASH_PAD, PETRA_TILDE_PAD) on the
#   CHAR*+CAP × BOUNDED-BUFFER axis.  PETRA_UNDERSCORE_PAD=1
#   selects the UNDERSCORE-padded char*+cap sibling, which fills
#   bytes [written, cap) with '_' (ASCII 95) on success.
#
# Closes Oct 13's "Where we go next" item (1) verbatim — the
# CHAR*+CAP-side TENTH axis, picking '_' (ASCII 95) as a
# TOTALLY DIFFERENT pad character from Oct 12's '~' (ASCII 126),
# Oct 9's '#' (ASCII 35), Oct 3's '-' (ASCII 45), Sep 28's ' '
# (ASCII 32), and Sep 25's '\0' (ASCII 0).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time flag
# (PETRA_UNDERSCORE_PAD) on top of the Oct 12 / Oct 9 / Oct 3 /
# Sep 28 / Sep 25 helpers' pad-free axes.  When PETRA_PAD = 1,
# PETRA_SPACE_PAD = 1, PETRA_DASH_PAD = 1, PETRA_HASH_PAD = 1,
# or PETRA_TILDE_PAD = 1, the lesson's friendly-error fires at
# compile time with a clear pointer to those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Oct  9   try_format_hash_pad_n — the EIGHTH axis
#              (PETRA_HASH_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis (same shape as today).
#   - Oct 12   try_format_tilde_pad_n — the NINTH axis
#              (PETRA_TILDE_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.
#   - Oct 13   try_format_tilde_pad_span_n — Oct 12's
#              STD::SPAN<CHAR> MIRROR.
#   - TODAY    try_format_underscore_pad_n — the TENTH axis
#              (PETRA_UNDERSCORE_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.  Closes Oct 13's "Where
#              we go next" item (1) verbatim.
#
# KEY FINDING: the UNDERSCORE-pad char*+cap sibling is a THIN
# LAYER on top of Oct 12's TILDE-pad char*+cap sibling (a
# second-level wrapper — today's UNDERSCORE-pad sibling wraps
# Oct 12's TILDE-pad sibling, not the Sep 8 / Sep 9 / Sep 20 /
# Sep 28 / Oct 3 / Oct 9 underlying impls directly).
#
# The composition rule "the LATER fill overwrites the EARLIER
# fill" now has SIX layers:
# UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL.  When all
# six pad-flags are set, the end byte state is always '_'.
# Composition cells with (UNDERSCORE=1, TILDE=*) REUSE TODAY's
# NEW UNDERSCORE-pad wrappers verbatim.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-14-try-format-underscore-pad-n
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

# PETRA_UNDERSCORE_PAD controls the dispatch between untouched sibling
# (bytes [written, cap) UNTOUCHED on success — the prior contract from
# Sep 8 / Sep 9 / Sep 20) and UNDERSCORE-padded sibling (bytes
# [written, cap) filled with '_' on success).  When
# PETRA_UNDERSCORE_PAD=1 with PETRA_TILDE_PAD=1 (a future lesson's
# composition cell), the UNDERSCORE fill OVERWRITES the TILDE fill
# (composition rule — same pattern Oct 12 established on the
# TILDE-overwrites-HASH axis).
option(PETRA_UNDERSCORE_PAD
       "0 = no underscore-pad; 1 = UNDERSCORE-padded sibling"
       OFF)
if(PETRA_UNDERSCORE_PAD)
    add_compile_options(-DPETRA_UNDERSCORE_PAD=1)
else()
    add_compile_options(-DPETRA_UNDERSCORE_PAD=0)
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

add_executable(P-2026-10-14-try-format-underscore-pad-n
               P-2026-10-14-try-format-underscore-pad-n.cpp)