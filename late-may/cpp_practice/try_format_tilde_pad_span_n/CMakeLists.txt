# CMakeLists.txt — today's P-2026-10-13 lesson:
#
#   try_format_tilde_pad_span_n — the STD::SPAN<CHAR> MIRROR of
#   Oct 12's try_format_tilde_pad_n.  TODAY introduces a NEW
#   SIXTH compile-time flag (PETRA_TILDE_PAD) on top of Oct 11's
#   5-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#   PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD) on the
#   STD::SPAN<CHAR> × BOUNDED-BUFFER axis.  PETRA_TILDE_PAD=1
#   selects the TILDE-padded span<char> sibling, which fills
#   bytes [written, span.size()) with '~' on success.
#
# Closes Oct 12's "Where we go next" item (1) verbatim — the
# STD::SPAN<CHAR> MIRROR of Oct 12's try_format_tilde_pad_n.
# TODAY closes the LAST open item from Oct 12's lesson on the
# STD::SPAN<CHAR> side; Oct 12 closed the CHAR*+CAP side
# (item (a) verbatim on the CHAR*+CAP side).  After today,
# the matrix on the (THROW × TILDE) 2-bit tuple is COMPLETE
# for ALL FOUR cells on BOTH axes (CHAR*+CAP and STD::SPAN<CHAR>).
#
# SCOPE: This lesson is NARROW.  It adds ONE compile-time flag
# (PETRA_TILDE_PAD) on top of the Oct 11 / Oct 4 / Sep 28 / Sep 27
# helpers' pad-free axes.  When PETRA_PAD = 1, PETRA_SPACE_PAD = 1,
# PETRA_DASH_PAD = 1, or PETRA_HASH_PAD = 1, the lesson's
# friendly-error fires at compile time with a clear pointer to
# those lessons.
#
# Where this fits in the arc
# --------------------------
#   - Oct  9   try_format_hash_pad_n — the EIGHTH axis
#              (PETRA_HASH_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis (same shape as today).
#   - Oct 11   try_format_hash_pad_span_n — Oct 9's STD::SPAN
#              <CHAR> MIRROR.
#   - Oct 12   try_format_tilde_pad_n — the NINTH axis
#              (PETRA_TILDE_PAD) on the CHAR*+CAP ×
#              BOUNDED-BUFFER axis.  Closes Oct 11's "Where
#              we go next" item (a) verbatim on the CHAR*+CAP
#              side.
#   - TODAY    try_format_tilde_pad_span_n — the NINTH axis
#              (PETRA_TILDE_PAD) on the STD::SPAN<CHAR> ×
#              BOUNDED-BUFFER axis.  Closes Oct 12's "Where
#              we go next" item (1) verbatim.  The CHAR*+CAP
#              and STD::SPAN<CHAR> axes now mirror each other
#              byte-for-byte on the (TILDE) axis.
#
# KEY FINDING: the TILDE-pad span<char> sibling is a THIN
# LAYER on top of Oct 11's HASH-pad span<char> sibling (a
# second-level wrapper — today's TILDE-pad sibling wraps
# Oct 11's HASH-pad sibling, not the Sep 10 / Sep 19 /
# Sep 21 / Sep 28 / Oct 4 underlying impls directly).  This
# is the same pattern Oct 12 used (TILDE layer on Oct 9's
# HASH-pad char*+cap sibling).
#
# The composition rule "the LATER fill overwrites the
# EARLIER fill" now has FIVE layers:
# TILDE > HASH > DASH > SPACE > NUL.  When all five
# pad-flags are set (TILDE, HASH, DASH_PAD, SPACE_PAD, PAD),
# the end byte state is always '~'.  Composition cells with
# (TILDE=1, HASH=*) and (TILDE=1, HASH=0) REUSE TODAY's NEW
# TILDE-pad wrappers verbatim (the (TILDE=1, HASH=*) cells
# and (TILDE=1, HASH=0) cell share wrappers because TILDE
# overwrites HASH, and HASH overwrites DASH overwrites
# SPACE).

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-13-try-format-tilde-pad-span-n
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
# [written, span.size()) UNTOUCHED on success — the prior contract from
# Sep 10 / Sep 19 / Sep 21) and TILDE-padded sibling (bytes [written,
# span.size()) filled with '~' on success).  When PETRA_TILDE_PAD=1 with
# PETRA_HASH_PAD=1 (a future lesson's composition cell), the TILDE fill
# OVERWRITES the HASH fill (composition rule — same pattern Oct 12
# established on the CHAR*+CAP side).
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

add_executable(P-2026-10-13-try-format-tilde-pad-span-n
               P-2026-10-13-try-format-tilde-pad-span-n.cpp)