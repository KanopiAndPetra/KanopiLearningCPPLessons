# CMakeLists.txt — P-2026-08-25 lesson:
#
#   std::format_to_n_into: a NUL-terminating bounded-buffer formatter
#   wrapper that returns std::expected<size_t, std::format_error>
#   — the natural complement to Aug 24's format_to_expected.
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23) — the
#              consteval-checked format-string surface
#   - Aug 24   std::format_to_n wrapped in std::expected<size_t,
#              format_error> (C++23) — bounded-buffer formatter with
#              a typed error channel; raw `format_to_n` does NOT
#              NUL-terminate.
#   - TODAY    std::format_to_n_into — companion wrapper that
#              NUL-terminates. Closes the Aug 24 lesson's first new
#              follow-on item: "A small companion that writes
#              min(r.value(), N-1) chars + a NUL would close the
#              'I just want a string' use case".
#
# What today covers
# -----------------
#   - A wrapper that composes std::format_to_n (P2216R3) with
#     std::expected<T, E> (P0323R12) AND always NUL-terminates the
#     destination buffer (when n > 0).
#   - The "would-be" semantics: r.value() is the chars that WOULD HAVE
#     been written, NOT the chars actually written — inherited from
#     std::format_to_n.
#   - NUL placement: at result.size when no truncation, at out[n-1]
#     when truncated (because cap == n-1 < n).
#   - Error path NUL: on std::format_error, out[0] is set to '\0'
#     so the caller can always print the buffer as a C-string.
#   - The n == 0 edge: no NUL written, but the would-be size IS
#     reported (same contract as the n >= 1 path).
#   - Heterogeneous dest types: char*, std::span<char>,
#     std::array<char,N>::data (via the span overload).
#   - Composition with std::expected's monadic ops (and_then,
#     transform, transform_error).
#   - Distinction from Aug 24's format_to_expected: today's wrapper
#     writes a NUL; Aug 24's does not. The two are interchangeable
#     at the call site if the caller wants a NUL-terminated buffer.
#   - Strict signedness handling: std::format_to_n's second parameter
#     is std::iter_difference_t<Out> (signed); the wrapper bridges
#     to/from std::size_t (unsigned) explicitly to keep strict
#     warnings silent.
#
# Build (default — standard -O0 debug build):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-25-format-to-n-into-expected
#
# Strict-warning build:
#
#   cmake -S . -B build-strict \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-25-format-to-n-into-expected
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-25-format-to-n-into-expected

cmake_minimum_required(VERSION 3.16)
project(format_to_n_into_expected_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::format_to_n_into — a NUL-terminating bounded-buffer formatter wrapper (P2216R3 + P0323R12). Companion to Aug 24's format_to_expected that closes the 'I just want a string' use case: always writes a NUL at the right slot regardless of truncation. Built on Apple Clang 21.0.0 / libc++. The wrapper handles the n == 0 edge (no NUL, but reports the would-be size), the n == 1 edge (only the NUL is written), the truncation edge (NUL at out[n-1]), and the error edge (NUL at out[0]). Strict signedness handling for std::iter_difference_t<Out> (signed) <-> std::size_t (unsigned)."
    LANGUAGES CXX
)

# C++23 for std::format_to_n (P2216R3) and std::expected (P0323R12).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-25-format-to-n-into-expected
    P-2026-08-25-format-to-n-into-expected.cpp
)

target_compile_options(P-2026-08-25-format-to-n-into-expected PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-25-format-to-n-into-expected PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-25-format-to-n-into-expected PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
