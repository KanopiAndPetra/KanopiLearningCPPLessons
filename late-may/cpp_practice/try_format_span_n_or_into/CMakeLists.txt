# CMakeLists.txt — today's P-2026-09-19 lesson:
#
#   try_format_span_n_or_into — a MACRO LAYER that lifts the
#   BOUNDED-BUFFER vs NUL-TERMINATING sibling choice to a macro form
#   on the STD::SPAN<CHAR> axis.  Two new macros —
#   TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO (CONSTEVAL) and
#   TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO (RUNTIME) — each dispatch
#   to EITHER Sep 10's try_format_bounded_runtime_n_span (the
#   bounded-buffer sibling, NO NUL termination) OR Sep 12's
#   try_format_runtime_n_into_bounded(span, ...) (the NUL-terminating
#   sibling), based on a caller-controlled compile-time flag
#   PETRA_NUL_TERMINATE.
#
# Closes Sep 16's "Where we go next" item #2 verbatim.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter.
#   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
#              std::size_t cap (RUNTIME std::string-output wrapper).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
#              Sep 7 (char* + std::size_t cap + std::string_view fmt,
#              NO NUL).
#   - Sep  9   try_format_bounded_runtime_n — CONSTEVAL char*+cap
#              (NO NUL).
#   - Sep 10   try_format_bounded_runtime_n_span — CONSTEVAL
#              std::span<char> char*+cap (NO NUL).
#   - Sep 11   format_to_n_into_bounded — CONSTEVAL NUL-terminating
#              char*+cap (plus std::span<char> overload).
#   - Sep 12   try_format_runtime_n_into_bounded — RUNTIME
#              NUL-terminating char*+cap (plus std::span<char>
#              overload).
#   - Sep 13   try_format_bounded_macros — eight TRY_FORMAT_BOUNDED_*
#              macros (CONSTEVAL vs RUNTIME sibling choice at the
#              call site).
#   - Sep 14   try_format_span_format_string — std::span<const char>
#              overloads for the runtime try_format_* family + four
#              new macros.
#   - Sep 15   try_format_consteval_span_format_string — pins that
#              the CONSTEVAL surface is incompatible with
#              std::span<const char>; adds a CONSTEVAL-SHAPED
#              RUNTIME PARSER family of four wrappers + four macros.
#   - Sep 16   try_format_bounded_n_or_into — the BOUNDED-BUFFER vs
#              NUL-TERMINATING sibling-choice macro layer (char*+cap
#              axis).
#   - TODAY    try_format_span_n_or_into — the BOUNDED-BUFFER vs
#              NUL-TERMINATING sibling-choice macro layer on the
#              std::span<char> axis (closes Sep 16's open item #2).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
#     Sep 14 / Sep 15 / Sep 16 verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim
#     (the bounded-buffer sibling impl).
#   - petra::detail::try_format_to_n_into_bounded_impl — REUSED Sep 12
#     verbatim (the NUL-terminating sibling impl).
#   - petra::try_format_runtime_n(span, ...) — NEW today (verbatim thin
#     forwarding wrapper to Sep 8's char*+cap impl; closes Sep 16's
#     open item #3).
#   - petra::try_format_bounded_runtime_n_span — REUSED Sep 10 verbatim
#     (CONSTEVAL span<char> bounded-buffer wrapper).
#   - petra::format_to_n_into_bounded(span, ...) — REUSED Sep 11 verbatim
#     (CONSTEVAL span<char> NUL-terminating wrapper).
#   - petra::try_format_runtime_n_into_bounded(span, ...) — REUSED
#     Sep 12 verbatim (RUNTIME span<char> NUL-terminating wrapper).
#   - The TWO new macros TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO and
#     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO — NEW today; each is
#     a verbatim thin dispatcher that picks between two siblings at
#     preprocessor time based on PETRA_NUL_TERMINATE.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-19-try-format-span-n-or-into
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-19-try-format-span-n-or-into
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-19-try-format-span-n-or-into
#
# NUL-terminating path (the OTHER branch of PETRA_NUL_TERMINATE):
#
#   cmake -S . -B build-nul -DPETRA_NUL_TERMINATE=1
#   cmake --build build-nul
#   ./build-nul/P-2026-09-19-try-format-span-n-or-into
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-19-try-format-span-n-or-into.cpp \
#           -o /tmp/P-2026-09-19-try-format-span-n-or-into
#
# Build matrix
# ------------
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#   - NUL-terminating path build: default + -DPETRA_NUL_TERMINATE=1
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::span<char> (P0122R7) + std::ranges::contiguous_range +
# std::println (P2093R14) + __VA_OPT__ preprocessor feature.
# Pure consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-19-try-format-span-n-or-into
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# PETRA_NUL_TERMINATE controls the dispatch between bounded-buffer
# sibling and NUL-terminating sibling.  Default = 0 (bounded-buffer).
# Set to 1 to switch to NUL-terminating path.
option(PETRA_NUL_TERMINATE
       "0 = bounded-buffer sibling; 1 = NUL-terminating sibling"
       OFF)
if(PETRA_NUL_TERMINATE)
    add_compile_options(-DPETRA_NUL_TERMINATE=1)
else()
    add_compile_options(-DPETRA_NUL_TERMINATE=0)
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

add_executable(P-2026-09-19-try-format-span-n-or-into
               P-2026-09-19-try-format-span-n-or-into.cpp)