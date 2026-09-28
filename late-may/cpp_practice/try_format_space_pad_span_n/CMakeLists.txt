# CMakeLists.txt — today's P-2026-09-28 lesson:
#
#   try_format_space_pad_span_n — the SIXTH-AXIS (PETRA_SPACE_PAD)
#   sibling-choice META-MACRO layer that introduces SPACE-padded
#   siblings on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#
# SCOPE NOTE: This lesson is FOCUSED on the SIXTH axis
# (PETRA_SPACE_PAD) ONLY.  Sep 27's PETRA_PAD = 1 (NUL-pad)
# branches are Sep 27's territory and require Sep 27's NEW NUL-pad
# wrappers (which are NOT included in this file).  When PETRA_PAD =
# 1, the lesson's dispatcher #errors with a clear pointer to Sep 27.
#
# Where this fits in the arc
# --------------------------
#   - Sep 27   try_format_padded_span_n_or_throw — the (THROWING ×
#              PADDED) meta-macro layer on the std::span<char>
#              axis (closes Sep 26's "Where we go next" item #1;
#              introduces the 5th-axis PETRA_PAD).
#   - TODAY    try_format_space_pad_span_n — the SIXTH axis
#              (PETRA_SPACE_PAD) on the std::span<char> ×
#              BOUNDED-BUFFER axis.  Closes Sep 27's "Where we go
#              next" item #2 verbatim.
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6
#     onwards verbatim.
#   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
#     TooLarge} — REUSED verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
#     petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim)
#     — REUSED verbatim.
#   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
#     args...) — REUSED Sep 8 verbatim.
#   - petra::try_format_bounded_runtime_n(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim.
#   - petra::try_format_runtime_n(std::span<char>, std::string_view,
#     args...) — REUSED Sep 19 verbatim.
#   - petra::try_format_bounded_runtime_n_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 10 verbatim.
#   - petra::try_format_runtime_n_throwing(std::span<char>,
#     std::string_view, args...) — REUSED Sep 21 verbatim.
#   - petra::try_format_bounded_runtime_n_throwing_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 21 verbatim.
#   - petra::try_format_bounded_runtime_n_space_pad_span(std::span<char>,
#     std::format_string<Args...>, args...) — NEW today (CONSTEVAL
#     span<char> expected+SPACE-padded wrapper; wraps Sep 10 verbatim
#     and space-fills [written, span.size()) with ' ' on success).
#   - petra::try_format_runtime_n_space_pad_span(std::span<char>,
#     std::string_view, args...) — NEW today (RUNTIME span<char>
#     expected+SPACE-padded wrapper; wraps Sep 19 verbatim and
#     space-fills [written, span.size()) with ' ' on success).
#   - petra::try_format_bounded_runtime_n_throwing_space_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) — NEW
#     today (CONSTEVAL span<char> throwing+SPACE-padded wrapper;
#     wraps Sep 21 verbatim and space-fills [written, span.size())
#     with ' ' on success ONLY via try/catch around the throwing
#     call).
#   - petra::try_format_runtime_n_throwing_space_pad_span(
#     std::span<char>, std::string_view, args...) — NEW today
#     (RUNTIME span<char> throwing+SPACE-padded wrapper; wraps Sep
#     21 verbatim and space-fills [written, span.size()) with ' '
#     on success ONLY via try/catch around the throwing call).
#   - The TWO new meta-macros TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW
#     (CONSTEVAL surface) and
#     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW (RUNTIME
#     surface).  Each is a verbatim thin dispatcher that picks
#     between FOUR sibling wrappers at preprocessor time based on
#     the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)
#     3-bit tuple.  PETRA_PAD MUST be 0 in this lesson; the
#     dispatcher #errors when PETRA_PAD = 1.
#
# Build (default — expected+untouched sibling; PETRA_PAD=0,
# PETRA_THROW=0, PETRA_SPACE_PAD=0):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-28-try-format-space-pad-span-n
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-28-try-format-space-pad-span-n
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-28-try-format-space-pad-span-n
#
# Expected+SPACE-padded path (PETRA_THROW=0, PETRA_SPACE_PAD=1):
#
#   cmake -S . -B build-space-pad -DPETRA_SPACE_PAD=ON
#   cmake --build build-space-pad
#   ./build-space-pad/P-2026-09-28-try-format-space-pad-span-n
#
# Throwing+untouched path (PETRA_THROW=1, PETRA_SPACE_PAD=0):
#
#   cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
#   cmake --build build-throw
#   ./build-throw/P-2026-09-28-try-format-space-pad-span-n
#
# Throwing+SPACE-padded path (PETRA_THROW=1, PETRA_SPACE_PAD=1):
#
#   cmake -S . -B build-throw-space-pad -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_SPACE_PAD=ON
#   cmake --build build-throw-space-pad
#   ./build-throw-space-pad/P-2026-09-28-try-format-space-pad-span-n
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-28-try-format-space-pad-span-n.cpp \
#           -o /tmp/P-2026-09-28-try-format-space-pad-span-n
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#   - throwing path build: default + -DPETRA_THROW_ON_OVERFLOW=1
#   - SPACE-padded path build: default + -DPETRA_SPACE_PAD=1
#   - throwing+SPACE-padded path build: default + -DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_SPACE_PAD=1
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::atomic + std::is_same_v +
# std::format_error + std::runtime_error + dynamic_cast +
# std::is_base_of_v + std::span<char> (P0122R7) +
# std::ranges::contiguous_range + __VA_OPT__ preprocessor feature.
# Pure consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-28-try-format-space-pad-span-n
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
# Default = 0 (expected — matches Sep 6 through Sep 27's contract).
option(PETRA_THROW_ON_OVERFLOW
       "0 = expected sibling; 1 = throwing sibling"
       OFF)
if(PETRA_THROW_ON_OVERFLOW)
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=1)
else()
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=0)
endif()

# PETRA_SPACE_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior
# contract) and SPACE-padded sibling (bytes [written, span.size())
# filled with ' ' on success).  Default = 0 (untouched).
option(PETRA_SPACE_PAD
       "0 = untouched sibling; 1 = SPACE-padded sibling"
       OFF)
if(PETRA_SPACE_PAD)
    add_compile_options(-DPETRA_SPACE_PAD=1)
else()
    add_compile_options(-DPETRA_SPACE_PAD=0)
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

add_executable(P-2026-09-28-try-format-space-pad-span-n
               P-2026-09-28-try-format-space-pad-span-n.cpp)
