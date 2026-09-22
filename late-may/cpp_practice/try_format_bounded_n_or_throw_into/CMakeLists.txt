# CMakeLists.txt — today's P-2026-09-22 lesson:
#
#   try_format_bounded_n_or_throw_into — a META-MACRO LAYER that lifts the
#   NUL-TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling choice to a macro
#   form on the CHAR*+CAP axis.  Two new meta-macros —
#   TRY_FORMAT_BOUNDED_N_OR_THROW_INTO (CONSTEVAL surface) and
#   TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW_INTO (RUNTIME surface) — each
#   dispatch to EITHER Sep 11's format_to_n_into_bounded (the CONSTEVAL
#   expected sibling, returns std::expected<size_t, FormatError>) OR Sep
#   12's try_format_runtime_n_into_bounded (the RUNTIME expected sibling,
#   returns std::expected<size_t, FormatError>) when
#   PETRA_THROW_ON_OVERFLOW=0; OR today's NEW
#   petra::try_format_to_n_into_bounded_throwing (the CONSTEVAL throwing
#   sibling, throws std::runtime_error) OR today's NEW
#   petra::try_format_runtime_n_into_bounded_throwing (the RUNTIME
#   throwing sibling, throws std::runtime_error) when
#   PETRA_THROW_ON_OVERFLOW=1.
#
# Closes Sep 21's "Where we go next" item #1 verbatim:
#
#   > 1. `char*+cap × CONSTEVAL-or-RUNTIME × NUL-terminating ×
#   >    throwing` cells — the NUL-terminating char*+cap throwing
#   >    sibling.  Each future lesson adds a verbatim thin throwing
#   >    wrapper composed on top of an existing NUL-terminating sibling
#   >    + a new `#if PETRA_THROW_ON_OVERFLOW`-gated macro.  The natural
#   >    follow-on is Sep 11's / Sep 12's NUL-terminating wrappers
#   >    (Sep 11's CONSTEVAL `format_to_n_into_bounded` + Sep 12's
#   >    RUNTIME `try_format_runtime_n_into_bounded`); each would gain
#   >    a `*_throwing` wrapper that re-raises as `std::runtime_error`.
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
#   - Sep 19   try_format_span_n_or_into — the BOUNDED-BUFFER vs
#              NUL-TERMINATING sibling-choice macro layer on the
#              std::span<char> axis.
#   - Sep 20   try_format_bounded_n_or_throw — the RETURN-TYPE-vs-
#              EXCEPTION sibling-choice macro layer on the char*+cap
#              axis (the FOURTH axis; covers the char*+cap ×
#              bounded-buffer × expected-vs-throwing cells).
#   - Sep 21   try_format_span_n_or_throw — the RETURN-TYPE-vs-
#              EXCEPTION sibling-choice macro layer on the
#              std::span<char> axis (the FOURTH axis on the
#              span<char> side; covers the span<char> × bounded-buffer
#              × expected-vs-throwing cells).
#   - TODAY    try_format_bounded_n_or_throw_into — the NUL-
#              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
#              meta-macro layer on the CHAR*+CAP axis (covers the
#              char*+cap × NUL-terminating × expected-vs-throwing
#              cells — closes Sep 21's "Where we go next" item #1
#              verbatim).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
#     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_to_n_into_bounded_impl (Sep 11 verbatim)
#     + petra::detail::try_format_runtime_n_into_bounded_impl (Sep 12
#     verbatim) — REUSED verbatim.
#   - petra::format_to_n_into_bounded — REUSED Sep 11 verbatim (the
#     CONSTEVAL NUL-terminating expected sibling).
#   - petra::try_format_runtime_n_into_bounded — REUSED Sep 12 verbatim
#     (the RUNTIME NUL-terminating expected sibling).
#   - petra::try_format_to_n_into_bounded_throwing — NEW today (CONSTEVAL
#     surface of the NUL-terminating throwing sibling).
#   - petra::try_format_runtime_n_into_bounded_throwing — NEW today
#     (RUNTIME surface of the NUL-terminating throwing sibling).
#   - The TWO new meta-macros TRY_FORMAT_BOUNDED_N_OR_THROW_INTO and
#     TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW_INTO — NEW today; each is
#     a verbatim thin dispatcher that picks between two siblings at
#     preprocessor time based on PETRA_THROW_ON_OVERFLOW.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-22-try-format-bounded-n-or-throw-into
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-22-try-format-bounded-n-or-throw-into
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-22-try-format-bounded-n-or-throw-into
#
# Throwing path (the OTHER branch of PETRA_THROW_ON_OVERFLOW):
#
#   cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=1
#   cmake --build build-throw
#   ./build-throw/P-2026-09-22-try-format-bounded-n-or-throw-into
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-22-try-format-bounded-n-or-throw-into.cpp \
#           -o /tmp/P-2026-09-22-try-format-bounded-n-or-throw-into
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
# dynamic_cast + std::is_base_of_v + __VA_OPT__ preprocessor feature.
# Pure consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-22-try-format-bounded-n-or-throw-into
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

add_executable(P-2026-09-22-try-format-bounded-n-or-throw-into
               P-2026-09-22-try-format-bounded-n-or-throw-into.cpp)