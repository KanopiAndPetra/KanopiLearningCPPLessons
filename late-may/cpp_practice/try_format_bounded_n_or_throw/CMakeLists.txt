# CMakeLists.txt — today's P-2026-09-20 lesson:
#
#   try_format_bounded_n_or_throw — a MACRO LAYER that lifts the
#   RETURN-TYPE-vs-EXCEPTION sibling choice to a macro form on the
#   CHAR*+CAP axis.  Two new macros — TRY_FORMAT_BOUNDED_N_OR_THROW
#   (CONSTEVAL surface) and TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW
#   (RUNTIME surface) — each dispatch to EITHER Sep 9's
#   try_format_bounded_runtime_n (the RETURN-expected sibling,
#   FormatError on overflow) OR today's NEW
#   petra::try_format_runtime_n_throwing (the THROWING sibling, throws
#   std::runtime_error on overflow), based on a caller-controlled
#   compile-time flag PETRA_THROW_ON_OVERFLOW.
#
# This is the FOURTH axis orthogonal to Sep 19's matrix.  Today's
# meta-macros are at the INTERSECTION of ALL FOUR axes:
#   char*-vs-span × CONSTEVAL-or-RUNTIME × bounded-vs-NUL-term × expected-vs-throwing.
# Today's lesson covers 2 of the 16 cells (the char*+cap × CONSTEVAL-
# or-RUNTIME × bounded-buffer × expected-or-throwing cells); the
# remaining 14 cells are left for future lessons.
#
# Closes (partially) Sep 19's "Where we go next" chain by adding a
# new dimension to the meta-macro matrix.  Does NOT close the
# remaining open items: Sep 16's open item #1 (replace hand-rolled
# counting_output_iterator with std::counting_output_iterator when
# libc++ ships it) remains open — std::counting_output_iterator is
# NOT yet shipped by libc++ 21 (verified during lesson prep).
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
#              char*+cap.
#   - Sep 12   try_format_runtime_n_into_bounded — RUNTIME
#              NUL-terminating char*+cap.
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
#   - Sep 19   try_format_span_n_or_into — the BOUNDED-BUFFER vs
#              NUL-TERMINATING sibling-choice macro layer on the
#              std::span<char> axis.
#   - TODAY    try_format_bounded_n_or_throw — the RETURN-TYPE-vs-
#              EXCEPTION sibling-choice macro layer on the char*+cap
#              axis (the FOURTH axis; today covers the
#              bounded-buffer × expected-or-throwing cells on the
#              char*+cap axis).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
#     Sep 14 / Sep 15 / Sep 16 / Sep 19 verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim.
#   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
#     args...) — REUSED Sep 8 verbatim (returns expected<size_t,
#     FormatError>; the THROWING sibling composes on top of it).
#   - petra::try_format_bounded_runtime_n — REUSED Sep 9 verbatim
#     (CONSTEVAL char*+cap; today's NEW TRY_FORMAT_BOUNDED_N_OR_THROW
#     dispatches to this when PETRA_THROW_ON_OVERFLOW=0).
#   - petra::try_format_runtime_n_throwing(char*, std::size_t,
#     std::string_view, args...) — NEW today (verbatim thin
#     wrapper that calls Sep 8's try_format_runtime_n and re-raises
#     FormatError{...} as std::runtime_error).
#   - petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
#     std::format_string<Args...>, args...) — NEW today (CONSTEVAL
#     surface of the throwing sibling).
#   - The TWO new macros TRY_FORMAT_BOUNDED_N_OR_THROW and
#     TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW — NEW today; each is
#     a verbatim thin dispatcher that picks between two siblings at
#     preprocessor time based on PETRA_THROW_ON_OVERFLOW.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-20-try-format-bounded-n-or-throw
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-20-try-format-bounded-n-or-throw
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-20-try-format-bounded-n-or-throw
#
# Throwing path (the OTHER branch of PETRA_THROW_ON_OVERFLOW):
#
#   cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=1
#   cmake --build build-throw
#   ./build-throw/P-2026-09-20-try-format-bounded-n-or-throw
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-20-try-format-bounded-n-or-throw.cpp \
#           -o /tmp/P-2026-09-20-try-format-bounded-n-or-throw
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#   - throwing path build: default + -DPETRA_THROW_ON_OVERFLOW=1
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::runtime_error + std::atomic +
# __VA_OPT__ preprocessor feature.
# Pure consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-20-try-format-bounded-n-or-throw
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# PETRA_THROW_ON_OVERFLOW controls the dispatch between expected
# sibling (returns std::expected<size_t, FormatError>) and throwing
# sibling (throws std::runtime_error on overflow).  Default = 0
# (expected sibling — the non-throwing surface is the safer default
# for code paths that cannot easily handle exceptions).
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

add_executable(P-2026-09-20-try-format-bounded-n-or-throw
               P-2026-09-20-try-format-bounded-n-or-throw.cpp)