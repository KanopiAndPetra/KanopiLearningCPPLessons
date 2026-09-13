# CMakeLists.txt — today's P-2026-09-13 lesson:
#
#   try_format_bounded_macros — a MACRO LAYER (eight TRY_FORMAT_BOUNDED_*
#   macros) that lifts the CONSTEVAL vs RUNTIME sibling choice at the
#   call site for all the bounded-format wrappers built up across
#   Aug 25 / Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12.
#   Closes Sep 12's "Where we go next" item #2 — a runtime-format-string
#   sibling of today's wrapper.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter.
#   - Sep  6   try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with cap
#              (std::string output, no NUL-termination, runtime fmt).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of Sep 7
#              (char* buffer + std::string_view fmt).
#   - Sep  9   try_format_bounded_runtime_n — the CONSTEVAL
#              BOUNDED-BUFFER combination of Sep 7 + Sep 8.
#   - Sep 10   try_format_bounded_runtime_n_span — the STDSPAN sibling
#              of Sep 9.
#   - Sep 11   format_to_n_into_bounded — the NUL-TERMINATING sibling
#              of Sep 9 / Sep 10, with a cap parameter.
#   - Sep 12   try_format_runtime_n_into_bounded — the
#              RUNTIME-FORMAT-STRING sibling of Sep 11.
#   - TODAY    try_format_bounded_macros — the MACRO LAYER over all
#              eight wrappers above.  Eight macros:
#                TRY_FORMAT_BOUNDED
#                TRY_FORMAT_BOUNDED_RUNTIME
#                TRY_FORMAT_BOUNDED_N
#                TRY_FORMAT_BOUNDED_RUNTIME_N
#                TRY_FORMAT_BOUNDED_INTO
#                TRY_FORMAT_BOUNDED_RUNTIME_INTO
#                TRY_FORMAT_BOUNDED_N_SPAN
#                TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 verbatim.
#   - petra::counting_output_iterator — REUSED Sep 6 / Sep 7 / Sep 8 /
#     Sep 9 / Sep 10 / Sep 11 / Sep 12 verbatim.
#   - petra::detail::bounded_char_writer — REUSED Sep 8 / Sep 9 /
#     Sep 10 / Sep 11 / Sep 12 verbatim.
#   - petra::try_format_runtime_bounded — REUSED Sep 6 verbatim
#     (the runtime std::string-output wrapper).
#   - petra::try_format_bounded — REUSED Sep 7 verbatim (the consteval
#     std::string-output wrapper).
#   - petra::try_format_runtime_n — REUSED Sep 8 verbatim (the runtime
#     char*+cap wrapper, no NUL).
#   - petra::try_format_bounded_runtime_n — REUSED Sep 9 verbatim (the
#     consteval char*+cap wrapper, no NUL).
#   - petra::try_format_bounded_runtime_n_span — REUSED Sep 10 verbatim
#     (the consteval std::span<char> wrapper, no NUL).
#   - petra::format_to_n_into_bounded — REUSED Sep 11 verbatim (the
#     consteval char*+cap NUL-terminating wrapper).
#   - petra::try_format_runtime_n_into_bounded — REUSED Sep 12 verbatim
#     (the runtime char*+cap NUL-terminating wrapper, plus its
#     std::span<char> overload).
#   - The eight TRY_FORMAT_BOUNDED_* macros — NEW today; each is a
#     verbatim thin dispatcher to one of the eight wrappers above.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-13-try-format-bounded-macros
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-13-try-format-bounded-macros
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-13-try-format-bounded-macros
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-13-try-format-bounded-macros.cpp \
#           -o /tmp/P-2026-09-13-try-format-bounded-macros
#
# Build matrix
# ------------
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::span<char> (P0122R7) + std::ranges::contiguous_range +
# std::println (P2093R14).  Pure consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-13-try-format-bounded-macros
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

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

add_executable(P-2026-09-13-try-format-bounded-macros
               P-2026-09-13-try-format-bounded-macros.cpp)
