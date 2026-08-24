# CMakeLists.txt — P-2026-08-24 lesson:
#
#   std::format_to_n wrapped in std::expected<size_t, format_error>
#   — a 9-line consumer-side helper that composes two C++23 facilities
#
# Where this fits in the arc
# --------------------------
#   - Jul  9   std::span      (C++20) — 1-D non-owning view
#   - Jul 10   std::mdspan    (C++23) — N-D non-owning view
#   - Jul 12   std::expected  (C++23) — sum-type error channel
#   - Aug 20   std::flat_map family (C++23) — DEFAULT std::vector storage
#   - Aug 21   std::flat_map family (C++23) — CUSTOM KeyContainer
#   - Aug 22   std::print / std::println / std::format (C++23) — the
#              consteval-checked format-string surface
#   - TODAY    std::format_to_n + std::expected<size_t, format_error>
#              (C++23) — bounded-buffer formatting with a typed error
#              channel. Closes the explicit Aug 22 follow-on:
#              "A format_to_n_into style consumer-side helper that
#               returns std::expected<std::size_t, FormatError>".
#
# What today covers
# -----------------
#   - A wrapper that composes std::format_to_n (P2216R3) with
#     std::expected<T, E> (P0323R12).
#   - "Would-be" semantics: r.value() is the chars that WOULD HAVE
#     been written, NOT the chars actually written — even on
#     truncation. This is the documented std::format_to_n contract.
#   - Heterogeneous destinations: char*, std::array<char,N>::iterator,
#     std::vector<char>::iterator, std::span<char>::iterator, and
#     std::back_insert_iterator<std::string>.
#   - back_inserter DOES respect the N cap (it grows the underlying
#     std::string, but std::format_to_n still writes AT MOST N chars).
#   - Composition with expected's monadic ops: and_then, transform,
#     transform_error.
#   - std::format_error's surface: it's a runtime_error, NOT a
#     system_error — there is no .code(); callers must .what() it.
#   - A round-trip pipeline (parse_int -> format_to_expected) where
#     the error type is std::format_error throughout — one channel.
#   - 50-round determinism stress.
#
# Build (default — standard -O0 debug build):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-24-format-to-n-expected
#
# Strict-warning build:
#
#   cmake -S . -B build-strict \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-24-format-to-n-expected
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-24-format-to-n-expected

cmake_minimum_required(VERSION 3.16)
project(format_to_n_expected_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::format_to_n (P2216R3) wrapped in std::expected<size_t, std::format_error> (P0323R12) on Apple Clang 21.0.0 / libc++. Composes two C++23 facilities into a 9-line bounded-buffer formatter with a typed error channel, exercises the 'would-be size' semantics, heterogeneous destinations (char*, std::array, std::vector, std::span, back_insert_iterator), composition with std::expected's monadic ops (and_then / transform / transform_error), std::format_error's hierarchy (runtime_error, not system_error), and a parse-then-format round-trip with one error channel end-to-end."
    LANGUAGES CXX
)

# C++23 for std::format_to_n (P2216R3) and std::expected (P0323R12).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-24-format-to-n-expected
    P-2026-08-24-format-to-n-expected.cpp
)

target_compile_options(P-2026-08-24-format-to-n-expected PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-24-format-to-n-expected PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-24-format-to-n-expected PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
