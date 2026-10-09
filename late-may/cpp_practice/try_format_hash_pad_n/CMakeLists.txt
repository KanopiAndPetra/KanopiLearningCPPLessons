# CMakeLists.txt — today's P-2026-10-09 lesson:
#
#   try_format_hash_pad_n — the EIGHTH axis (PETRA_HASH_PAD)
#   sibling-choice META-MACRO layer on the CHAR*+CAP ×
#   BOUNDED-BUFFER axis.  TODAY introduces a NEW FIFTH
#   compile-time flag (PETRA_HASH_PAD) on top of Oct 8's 4-bit
#   tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
#   PETRA_DASH_PAD).  PETRA_HASH_PAD=1 selects the HASH-padded
#   sibling, which fills bytes [written, cap) with '#' on
#   success.
#
# Closes Oct 8's "Where we go next" item (1) verbatim — the
# (PETRA_HASH_PAD=1) sibling-choice macro layer on the
# char*+cap side.  The matrix on the BOUNDED-BUFFER ×
# (expected-or-throwing) × (untouched-vs-SPACE-vs-DASH-vs-HASH)
# quadrant on the char*+cap side is now COMPLETE for all 16
# cells of the 4-bit tuple (PETRA_THROW_ON_OVERFLOW,
# PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD)
# (PETRA_PAD=0 only; the PETRA_PAD=1 cells live in Sep 25's /
# Sep 26's lessons).
#
# KEY DESIGN DECISION — the (0,0,0,1,1), (0,0,1,1,1), (1,0,0,1,1),
# and (1,0,1,1,1) cells REUSE the SAME HASH-pad sibling wrappers
# (the (0,0,1,1,1) and (1,0,1,1,1) cells share wrappers with the
# (0,0,0,1,1) and (1,0,0,1,1) cells because HASH overwrites DASH
# when both are set, and transitively HASH overwrites SPACE → DASH
# → NUL when all four pad-flags are set — the composition rule
# "the LATER fill overwrites the EARLIER fill").  No NEW wrappers
# are needed for the (0,0,1,1,1) and (1,0,1,1,1) cells; they just
# dispatch to the (0,0,0,1,1) and (1,0,0,1,1) HASH-pad wrappers.
# Likewise the (0,0,0,0,1) and (1,0,0,0,1) HASH-pad-only cells
# reuse the (0,0,0,1,1) and (1,0,0,1,1) HASH-pad wrappers
# verbatim — no DASH-pad intermediate, just (HASH=1, DASH=0)
# composes as if HASH had written last directly.
#
# Where this fits in the arc
# --------------------------
#   - Sep 28   try_format_space_pad_span_n — the SIXTH axis
#              (PETRA_SPACE_PAD) sibling-choice macro layer on
#              the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Sep 29   try_format_space_pad_n — the SIXTH axis
#              (PETRA_SPACE_PAD) sibling-choice macro layer on
#              the CHAR*+CAP × BOUNDED-BUFFER axis.
#   - Sep 30   try_format_padded_space_pad_n_or_throw — the
#              COMPOSITION of axis 5 (PETRA_PAD) and axis 6
#              (PETRA_SPACE_PAD) on the CHAR*+CAP × BOUNDED-
#              BUFFER axis.
#   - Oct  1   try_format_padded_space_pad_span_n_or_throw —
#              the STD::SPAN<CHAR> MIRROR of Sep 30.
#   - Oct  3   try_format_dash_pad_n — the SEVENTH axis
#              (PETRA_DASH_PAD) sibling-choice macro layer on
#              the CHAR*+CAP × BOUNDED-BUFFER axis.
#   - Oct  4   try_format_dash_pad_span_n — the SEVENTH axis
#              (PETRA_DASH_PAD) sibling-choice macro layer on
#              the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Oct  5   try_format_padded_dash_pad_span_n_or_throw — the
#              COMPOSITION of axis 5 and axis 7 on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Oct  8   try_format_padded_dash_pad_n_or_throw — the
#              COMPOSITION of axis 5 and axis 7 on the
#              CHAR*+CAP × BOUNDED-BUFFER axis (mirrors Oct 5).
#   - TODAY    try_format_hash_pad_n — the EIGHTH axis
#              (PETRA_HASH_PAD) sibling-choice macro layer on
#              the CHAR*+CAP × BOUNDED-BUFFER axis.  Closes
#              Oct 8's "Where we go next" item (1) verbatim.
#              KEY FINDING: the (HASH_PAD=1, DASH_PAD=1) cells
#              REUSE the (HASH_PAD=1, DASH_PAD=0) HASH-pad
#              sibling wrappers verbatim because HASH overwrites
#              DASH when both are set (the composition rule).
#              Likewise the (HASH_PAD=1, DASH_PAD=0, SPACE_PAD=1)
#              cells and the (HASH_PAD=1, DASH_PAD=1, SPACE_PAD=1)
#              cells REUSE the same wrappers verbatim via the
#              transitive composition rule (HASH > DASH > SPACE).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED
#     verbatim.
#   - petra::FormatErrorKind — REUSED verbatim.
#   - petra::counting_output_iterator — REUSED verbatim (with
#     the Oct 3 / Oct 8 callback-based design).
#   - petra::detail::bounded_char_writer — REUSED verbatim
#     (with the Oct 3 / Oct 8 callback-based design, where
#     `written_` is incremented ONLY on successful writes).
#   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim)
#     + petra::detail::try_format_bounded_runtime_n_impl
#     (Sep 9 verbatim) — REUSED verbatim with the proven
#     callback-based pattern.
#   - petra::try_format_runtime_n(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 8 verbatim.
#   - petra::try_format_bounded_runtime_n(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 9
#     verbatim.
#   - petra::try_format_runtime_n_throwing(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 20 verbatim.
#   - petra::try_format_bounded_runtime_n_throwing(char*,
#     std::size_t, std::format_string<Args...>, args...) —
#     REUSED Sep 20 verbatim.
#   - petra::try_format_runtime_n_space_pad(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 29 verbatim
#     (RUNTIME expected+SPACE-padded sibling wrapper).
#   - petra::try_format_bounded_runtime_n_space_pad(char*,
#     std::size_t, std::format_string<Args...>, args...) —
#     REUSED Sep 29 verbatim (CONSTEVAL expected+SPACE-padded
#     sibling wrapper).
#   - petra::try_format_runtime_n_throwing_space_pad(char*,
#     std::size_t, std::string_view, args...) — REUSED Sep 29
#     verbatim (RUNTIME throwing+SPACE-padded sibling wrapper).
#   - petra::try_format_bounded_runtime_n_throwing_space_pad(
#     char*, std::size_t, std::format_string<Args...>, args...)
#     — REUSED Sep 29 verbatim (CONSTEVAL throwing+SPACE-padded
#     sibling wrapper).
#   - petra::try_format_runtime_n_dash_pad(char*, std::size_t,
#     std::string_view, args...) — REUSED Oct 3 verbatim
#     (RUNTIME expected+DASH-padded sibling wrapper).
#   - petra::try_format_bounded_runtime_n_dash_pad(char*,
#     std::size_t, std::format_string<Args...>, args...) —
#     REUSED Oct 3 verbatim (CONSTEVAL expected+DASH-padded
#     sibling wrapper).
#   - petra::try_format_runtime_n_throwing_dash_pad(char*,
#     std::size_t, std::string_view, args...) — REUSED Oct 3
#     verbatim (RUNTIME throwing+DASH-padded sibling wrapper).
#   - petra::try_format_bounded_runtime_n_throwing_dash_pad(
#     char*, std::size_t, std::format_string<Args...>, args...)
#     — REUSED Oct 3 verbatim (CONSTEVAL throwing+DASH-padded
#     sibling wrapper).
#   - petra::try_format_runtime_n_hash_pad(char*, std::size_t,
#     std::string_view, args...) — NEW today (RUNTIME
#     expected+HASH-padded sibling wrapper; wraps Oct 3's
#     RUNTIME expected+DASH-padded sibling and hash-fills
#     [written, cap) with '#' on success ONLY).
#   - petra::try_format_bounded_runtime_n_hash_pad(char*,
#     std::size_t, std::format_string<Args...>, args...) —
#     NEW today (CONSTEVAL expected+HASH-padded sibling
#     wrapper; wraps Oct 3's CONSTEVAL expected+DASH-padded
#     sibling and hash-fills [written, cap) with '#' on success
#     ONLY).
#   - petra::try_format_runtime_n_throwing_hash_pad(char*,
#     std::size_t, std::string_view, args...) — NEW today
#     (RUNTIME throwing+HASH-padded sibling wrapper; wraps Oct
#     3's RUNTIME throwing+DASH-padded sibling and hash-fills
#     [written, cap) with '#' on success ONLY; re-raises
#     std::runtime_error untouched on error).
#   - petra::try_format_bounded_runtime_n_throwing_hash_pad(
#     char*, std::size_t, std::format_string<Args...>, args...)
#     — NEW today (CONSTEVAL throwing+HASH-padded sibling
#     wrapper; wraps Oct 3's CONSTEVAL throwing+DASH-padded
#     sibling and hash-fills [written, cap) with '#' on success
#     ONLY; re-raises std::runtime_error untouched on error).
#   - The TWO new meta-macros TRY_FORMAT_BOUNDED_N_PAD_OR_THROW
#     (CONSTEVAL surface) and TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW
#     (RUNTIME surface).  Each is a verbatim thin dispatcher
#     that picks between SIXTEEN sibling wrappers at preprocessor
#     time based on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#     PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD) 5-bit
#     tuple.  PETRA_PAD MUST be 0 in this lesson (the dispatcher
#     #errors with a clear pointer to Sep 25 / Sep 26 when
#     PETRA_PAD=1, since Sep 25's / Sep 26's NUL-padded wrappers
#     are out of scope for this lesson).  The (0,0,1,1,1) and
#     (1,0,1,1,1) cells REUSE the (0,0,0,1,1) and (1,0,0,1,1)
#     HASH-pad sibling wrappers verbatim (the "HASH overwrites
#     DASH" composition rule, transitively extended to "HASH
#     overwrites DASH overwrites SPACE").
#
# Build (default — expected+untouched sibling; PETRA_THROW=0,
# PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0,
# PETRA_HASH_PAD=0):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-10-09-try-format-hash-pad-n
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-10-09-try-format-hash-pad-n
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-10-09-try-format-hash-pad-n
#
# HASH-pad cells (PETRA_HASH_PAD=1):
#
#   cmake -S . -B build-hash-pad -DPETRA_HASH_PAD=ON
#   cmake --build build-hash-pad
#   ./build-hash-pad/P-2026-10-09-try-format-hash-pad-n
#
# DASH+HASH composition cells (PETRA_DASH_PAD=1,
# PETRA_HASH_PAD=1):
#
#   cmake -S . -B build-dash-hash \
#         -DPETRA_DASH_PAD=ON -DPETRA_HASH_PAD=ON
#   cmake --build build-dash-hash
#   ./build-dash-hash/P-2026-10-09-try-format-hash-pad-n
#
# Throwing HASH-pad cells (PETRA_THROW_ON_OVERFLOW=1,
# PETRA_HASH_PAD=1):
#
#   cmake -S . -B build-throw-hash \
#         -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_HASH_PAD=ON
#   cmake --build build-throw-hash
#   ./build-throw-hash/P-2026-10-09-try-format-hash-pad-n
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-10-09-try-format-hash-pad-n.cpp \
#           -o /tmp/P-2026-10-09-try-format-hash-pad-n
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic
#     -Werror -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address
#     -fsanitize=undefined -fno-omit-frame-pointer -O1
#   - flag cells: PETRA_THROW_ON_OVERFLOW × PETRA_PAD ×
#     PETRA_SPACE_PAD × PETRA_DASH_PAD × PETRA_HASH_PAD → 32
#     cells (2 × 2 × 2 × 2 × 2); PETRA_PAD=1 is a #error (out
#     of scope for this lesson).
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::atomic + std::is_same_v +
# std::format_error + std::runtime_error + dynamic_cast +
# std::is_base_of_v + std::array<std::thread, 4> + std::thread::join() +
# __VA_OPT__ preprocessor feature.  Pure consumer-side; no
# third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-09-try-format-hash-pad-n
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

# PETRA_PAD controls the dispatch between untouched sibling (bytes
# [written, cap) UNTOUCHED on success — the prior contract) and
# NUL-padded sibling (bytes [written, cap) zero-filled to '\0' on
# success).  MUST be 0 for this lesson; #error fires otherwise.
option(PETRA_PAD
       "0 = untouched sibling; 1 = NUL-padded sibling"
       OFF)
if(PETRA_PAD)
    add_compile_options(-DPETRA_PAD=1)
else()
    add_compile_options(-DPETRA_PAD=0)
endif()

# PETRA_SPACE_PAD controls the dispatch between untouched sibling
# (bytes [written, cap) UNTOUCHED on success — the prior contract)
# and SPACE-padded sibling (bytes [written, cap) filled with ' '
# on success).
option(PETRA_SPACE_PAD
       "0 = no space-pad; 1 = SPACE-padded sibling"
       OFF)
if(PETRA_SPACE_PAD)
    add_compile_options(-DPETRA_SPACE_PAD=1)
else()
    add_compile_options(-DPETRA_SPACE_PAD=0)
endif()

# PETRA_DASH_PAD controls the dispatch between untouched sibling
# (bytes [written, cap) UNTOUCHED on success — the prior contract)
# and DASH-padded sibling (bytes [written, cap) filled with '-'
# on success).  When both PETRA_SPACE_PAD and PETRA_DASH_PAD are
# set, the DASH fill OVERWRITES the SPACE fill (composition rule).
option(PETRA_DASH_PAD
       "0 = no dash-pad; 1 = DASH-padded sibling"
       OFF)
if(PETRA_DASH_PAD)
    add_compile_options(-DPETRA_DASH_PAD=1)
else()
    add_compile_options(-DPETRA_DASH_PAD=0)
endif()

# PETRA_HASH_PAD controls the dispatch between untouched sibling
# (bytes [written, cap) UNTOUCHED on success — the prior contract)
# and HASH-padded sibling (bytes [written, cap) filled with '#'
# on success).  When PETRA_HASH_PAD=1 with PETRA_DASH_PAD=1,
# the HASH fill OVERWRITES the DASH fill (composition rule).
# When all three (SPACE, DASH, HASH) are set, HASH wins
# transitively over DASH over SPACE.
option(PETRA_HASH_PAD
       "0 = no hash-pad; 1 = HASH-padded sibling"
       OFF)
if(PETRA_HASH_PAD)
    add_compile_options(-DPETRA_HASH_PAD=1)
else()
    add_compile_options(-DPETRA_HASH_PAD=0)
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

add_executable(P-2026-10-09-try-format-hash-pad-n
               P-2026-10-09-try-format-hash-pad-n.cpp)
