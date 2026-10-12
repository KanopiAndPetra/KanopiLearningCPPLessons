# CMakeLists.txt — today's P-2026-10-12 lesson:
#
#   try_format_tilde_pad_n — the NINTH AXIS (PETRA_TILDE_PAD)
#   sibling-choice macro layer on the CHAR*+CAP ×
#   BOUNDED-BUFFER axis.  TODAY introduces a NEW SIXTH
#   compile-time flag (PETRA_TILDE_PAD) on top of Oct 9's
#   5-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#   PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD).
#   PETRA_TILDE_PAD=1 selects the TILDE-padded char*+cap
#   sibling, which fills bytes [written, cap) with '~' on
#   success.
#
# Closes Oct 11's "Where we go next" item (a) verbatim on the
# CHAR*+CAP side — the NINTH axis (Pad character) was
# explicitly named in Oct 11's "future lessons can …
# introduce a NINTH axis (e.g. PETRA_UNDERSCORE_PAD,
# PETRA_TILDE_PAD, or some other pad character)" note.  The
# '~' character was chosen as a TOTALLY DIFFERENT pad byte
# from '#' (Oct 9) and '-' (Oct 3) to demonstrate that
# the (PAD) axis is genuinely a character-level axis (not
# just a HASH/DASH-SPECIFIC axis).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time
# flag (PETRA_TILDE_PAD) on top of the Sep 25 / Sep 26 / Sep
# 27 / Sep 28 / Sep 29 / Oct 1 / Oct 3 / Oct 4 / Oct 5 /
# Oct 8 / Oct 9 / Oct 11 helpers' character-free axes.  When
# PETRA_PAD = 1, PETRA_SPACE_PAD = 1, PETRA_DASH_PAD = 1, or
# PETRA_HASH_PAD = 1, the lesson's friendly-error fires at
# compile time with a clear pointer to those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Oct  9   try_format_hash_pad_n — the EIGHTH axis
#              (PETRA_HASH_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis (same shape as today).
#   - Oct 11   try_format_hash_pad_span_n — Oct 9's STD::SPAN
#              <CHAR> MIRROR.
#   - TODAY    try_format_tilde_pad_n — the NINTH axis
#              (PETRA_TILDE_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.  Closes Oct 11's "Where
#              we go next" item (a) verbatim on the CHAR*+CAP
#              side.
#
# KEY FINDING: the TILDE-pad char*+cap sibling is a THIN
# LAYER on top of Oct 9's HASH-pad char*+cap sibling (a
# second-level wrapper — today's TILDE-pad sibling wraps
# Oct 9's HASH-pad sibling, not the Sep 8 / Sep 9 / Sep 20
# throw-or-expected underlying impls directly).  This is the
# same pattern Oct 9 used (HASH layer on Oct 3's DASH-pad
# sibling).
#
# The composition rule "the LATER fill overwrites the
# EARLIER fill" now has FIVE layers:
# TILDE > HASH > DASH > SPACE > NUL.  When all five
# pad-flags are set (TILDE, HASH, DASH, SPACE, PAD), the end
# byte state is always '~'.  Composition cells with
# (TILDE=1, HASH=1) and (TILDE=1, HASH=0) REUSE TODAY's NEW
# TILDE-pad wrappers verbatim (the (TILDE=1, HASH=1) cell
# and (TILDE=1, HASH=0) cell share wrappers because TILDE
# overwrites HASH, and HASH overwrites DASH overwrites
# SPACE).

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-12-try-format-tilde-pad-n
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

# PETRA_TILDE_PAD controls the dispatch between untouched sibling (bytes
# [written, cap) UNTOUCHED on success — the prior contract from Sep 8 /
# Sep 9 / Sep 20) and TILDE-padded sibling (bytes [written, cap) filled
# with '~' on success).  When PETRA_TILDE_PAD=1 with PETRA_HASH_PAD=1
# (a future lesson's composition cell), the TILDE fill OVERWRITES the
# HASH fill (composition rule — same pattern Oct 9 established).
option(PETRA_TILDE_PAD
       "0 = no tilde-pad; 1 = TILDE-padded sibling"
       OFF)
if(PETRA_TILDE_PAD)
    add_compile_options(-DPETRA_TILDE_PAD=1)
else()
    add_compile_options(-DPETRA_TILDE_PAD=0)
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

add_executable(P-2026-10-12-try-format-tilde-pad-n
               P-2026-10-12-try-format-tilde-pad-n.cpp)
