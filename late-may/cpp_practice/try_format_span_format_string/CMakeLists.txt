# CMakeLists.txt — today's P-2026-09-14 lesson:
#
#   try_format_span_format_string — adds a STD::SPAN<CONST CHAR>
#   FORMAT-STRING PARAMETER OVERLOAD FAMILY for the runtime
#   try_format_* wrappers built up across Sep 6 / Sep 8 / Sep 12,
#   plus a parallel macro layer over those overloads (mirroring
#   Sep 13's pattern).  Closes Sep 13's "Where we go next" item #3
#   verbatim.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter.
#   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
#              std::size_t cap (the RUNTIME std::string-output
#              wrapper).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
#              Sep 7 (char* + std::size_t cap + std::string_view fmt).
#   - Sep  9   try_format_bounded_runtime_n — CONSTEVAL char*+cap.
#   - Sep 10   try_format_bounded_runtime_n_span — CONSTEVAL
#              std::span<char> char*+cap.
#   - Sep 11   format_to_n_into_bounded — CONSTEVAL NUL-terminating
#              char*+cap.
#   - Sep 12   try_format_runtime_n_into_bounded — RUNTIME
#              NUL-terminating char*+cap (plus std::span<char>
#              overload).
#   - Sep 13   try_format_bounded_macros — eight TRY_FORMAT_BOUNDED_*
#              macros (CONSTEVAL vs RUNTIME sibling choice at the
#              call site).
#   - TODAY    try_format_span_format_string — std::span<const char>
#              overloads for the runtime try_format_* family +
#              four new macros (TRY_FORMAT_BOUNDED_SPAN,
#              _N_SPAN_FMT, _INTO_SPAN, _RUNTIME_N_SPAN_SPAN).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13
#     verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_bounded_impl — REUSED Sep 6
#     verbatim.
#   - petra::detail::try_format_runtime_n_impl — REUSED Sep 8 verbatim.
#   - petra::detail::try_format_to_n_into_bounded_impl — REUSED Sep 12
#     verbatim.
#   - petra::try_format_runtime_bounded (std::string_view) — REUSED
#     Sep 6 verbatim (kept as primary; today adds a parallel
#     std::span<const char> overload that forwards to the same impl).
#   - petra::try_format_runtime_n (std::string_view) — REUSED Sep 8
#     verbatim (kept as primary; today adds a parallel
#     std::span<const char> overload).
#   - petra::try_format_runtime_n_into_bounded (std::string_view) —
#     REUSED Sep 12 verbatim (kept; today adds a parallel
#     std::span<const char> overload for the format-string parameter
#     for both the (char*, size_t) output and the std::span<char>
#     output).
#   - The four new overloads accepting std::span<const char> for the
#     format string parameter — NEW today.
#   - The four new TRY_FORMAT_BOUNDED_*_SPAN / _N_SPAN_FMT / _INTO_SPAN /
#     _RUNTIME_N_SPAN_SPAN macros — NEW today.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-14-try-format-span-format-string
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-14-try-format-span-format-string
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-14-try-format-span-format-string
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-14-try-format-span-format-string.cpp \
#           -o /tmp/P-2026-09-14-try-format-span-format-string
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
# C++23 std::format + std::vformat_to + std::make_format_args +
# std::expected (P0323R12) + std::span<const char> (P0122R7,
# P2499R0 range ctor) + std::ranges::contiguous_range +
# std::println (P2093R14).  Pure consumer-side; no third-party
# libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-14-try-format-span-format-string
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

add_executable(P-2026-09-14-try-format-span-format-string
               P-2026-09-14-try-format-span-format-string.cpp)
