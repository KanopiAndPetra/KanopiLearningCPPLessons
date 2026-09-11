# CMakeLists.txt — today's P-2026-09-11 lesson:
#
#   format_to_n_into_bounded — std::expected<std::size_t, FormatError>
#   wrapper around std::vformat_to that takes a caller-provided byte
#   cap AND a consteval-checked std::format_string<Args...> fmt AND
#   ALWAYS NUL-terminates the destination on the success path.  Closes
#   Sep 10's "Where we go next" item #3 — the NUL-TERMINATING BOUNDED
#   sibling of Sep 9 / Sep 10's try_format_bounded_runtime_n[_span].
#   Aug 25's format_to_n_into is the NUL-terminating sibling WITHOUT
#   a cap parameter (it writes a complete std::string); today is the
#   bounded sibling with a cap parameter.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter (always returns the
#              would-be size, writes min(would-be, N-1) chars + NUL).
#   - Sep  6   try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with cap
#              (std::string output, no NUL-termination).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of Sep 7
#              (char* buffer + std::string_view fmt).
#   - Sep  9   try_format_bounded_runtime_n — the CONSTEVAL
#              BOUNDED-BUFFER combination of Sep 7 + Sep 8.
#   - Sep 10   try_format_bounded_runtime_n_span — the STDSPAN sibling
#              of Sep 9.
#   - TODAY    format_to_n_into_bounded — the NUL-TERMINATING sibling
#              of Sep 9 / Sep 10, with a cap parameter (Aug 25's
#              NUL-termination contract + Sep 9's cap parameter).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 verbatim.
#   - petra::counting_output_iterator — REUSED Sep 6 / Sep 7 / Sep 8 /
#     Sep 9 / Sep 10 verbatim.
#   - petra::detail::bounded_char_writer — REUSED Sep 8 / Sep 9 / Sep 10
#     verbatim.
#   - petra::detail::try_format_to_n_into_bounded_impl — NEW today;
#     a two-pass measure-then-write pair that RESERVES ONE SLOT for
#     the NUL (effective_cap = cap - 1).  Differs from Sep 9's
#     impl because Sep 9 does not reserve a NUL slot.
#   - petra::format_to_n_into_bounded<Args...>(char* out, std::size_t cap,
#     std::format_string<Args...>, Args&&...) — NEW today, the
#     NUL-terminating bounded primary surface.
#   - petra::format_to_n_into_bounded<Args...>(std::span<char> out, ...) —
#     NEW today, the STDSPAN overload (mirrors Sep 10's pattern).
#
# What today does NOT do
# ----------------------
#   - Does NOT use std::counting_output_iterator (still pending
#     libc++ 21's standard library surface).
#   - Does NOT compose with the macro layer (Sep 1 / Sep 2's
#     LOG_INFO_RUNTIME / SYNC_LOG_INFO pattern); a follow-on could
#     add TRY_FORMAT_BOUNDED_INTO_RUNTIME / _CONSTEVAL macros that
#     expand to today's wrapper on the consteval path and Aug 25's
#     format_to_n_into on the runtime path.
#   - Does NOT add a runtime-format-string sibling (Sep 8's pattern);
#     today's wrapper takes std::format_string<Args...> (consteval
#     check at the call site).  A runtime sibling would be analogous
#     to Sep 8's try_format_runtime_n.
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
# std::println (P2093R14).  Pure consumer-side; no third-party
# libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-11-format-to-n-into-bounded
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

add_executable(P-2026-09-11-format-to-n-into-bounded
               P-2026-09-11-format-to-n-into-bounded.cpp)
