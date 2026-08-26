# CMakeLists.txt — P-2026-08-26 lesson:
#
#   std::format_to_n_runtime_string: a runtime-format-string
#   bounded-buffer formatter wrapper that returns
#   std::expected<size_t, std::format_error>. Companion to Aug 25's
#   format_to_n_into that closes the second new follow-on item from
#   Aug 25's "Where we go next" section:
#
#     > A companion overload that takes a runtime format string (and
#     > returns expected on parse error) is a natural addition when
#     > the format string comes from external input (config files,
#     > user templates). The error type would still be
#     > std::format_error. Today is NOT that lesson — the focus is
#     > the NUL-termination contract.
#
#   TODAY is that lesson: the runtime-format-string companion to
#   format_to_n_into. The 3Aug 25 lesson's compile-time-validated
#   `std::format_string<Args...>` parameter becomes a runtime
#   `std::string_view` here, and the failures that the consteval gate
#   caught at compile time (parse errors, type mismatches) become
#   runtime std::format_error throws that the wrapper converts to
#   std::unexpected.
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23) — the
#              consteval-checked format-string surface
#   - Aug 24   std::format_to_n wrapped in std::expected<size_t,
#              format_error> (C++23) — bounded-buffer formatter with
#              a typed error channel; raw `format_to_n` does NOT
#              NUL-terminate. Same format_string<Args...> consteval
#              gate as Aug 22.
#   - Aug 25   std::format_to_n_into — companion to Aug 24 that
#              NUL-terminates. Same consteval gate.
#   - TODAY    std::format_to_n_runtime_string — SAME bounded-buffer
#              NUL-terminating contract as Aug 25, but with a runtime
#              format string (std::string_view) instead of a
#              consteval-checked std::format_string<Args...>. The
#              parse-error and type-mismatch errors that the consteval
#              gate caught at compile time on the Aug 22/24/25 surface
#              become runtime std::format_error throws here.
#
# What today covers
# -----------------
#   - A wrapper that takes std::string_view fmt and forwards to
#     std::vformat_to (C++23 P2216R3 / P2732R0 runtime path) instead
#     of std::format_to_n (compile-time P2502R2 surface).
#   - The two-pass design: pass 1 measures the would-be size by
#     formatting into a counting-style iterator; pass 2 writes into
#     the bounded buffer via a custom bounded_output_iterator that
#     stops accepting writes after `n - 1` chars. The `n - 1` cap
#     reserves one slot for the NUL — same shape as Aug 25.
#   - The bounded_output_iterator: a stateful output iterator that
#     mutates an external count via pointer (because std::vformat_to
#     COPIES the iterator on each advance, so a member count would
#     not survive back to the caller — see Section 16 for the
#     trail-of-copies lesson).
#   - Three failure modes that the consteval gate on the Aug 22/24/25
#     surface would have caught at compile time, but that surface here
#     as runtime errors:
#       a) Bad parse: dangling/unterminated '{' or invalid specifier
#          (std::vformat throws std::format_error
#           "The format string terminates at a '{'").
#       b) Bad argument index: {:5} when only one arg is supplied
#          (std::format_error "argument index value too large").
#       c) Bad type: {} expecting a float that gets a string
#          (std::format_error "argument index value too large" — the
#          same error message re-used by libc++ 21).
#   - The strict signedness contract: pass 1 returns a signed
#     iter_difference_t of the counting iterator (we promote to
#     std::size_t explicitly). Pass 2's output iterator also has a
#     signed iter_difference_t<It>.
#   - The n == 0 and n == 1 edges — same shape as Aug 25.
#   - The first-runtime-error ASan finding: a copy-on-advance
#     bounded_output_iterator that stored its state in members
#     (not via external pointer) silently lost writes because every
#     increment() inside std::vformat_to COPIED the iterator and
#     mutated the copy. External pointer fixes it.
#
# Build (default — standard -O0 debug build):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-26-format-to-n-runtime-string-expected
#
# Strict-warning build:
#
#   cmake -S . -B build-strict \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-26-format-to-n-runtime-string-expected
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-26-format-to-n-runtime-string-expected

cmake_minimum_required(VERSION 3.16)
project(format_to_n_runtime_string_expected_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::format_to_n_runtime_string — a RUNTIME format-string bounded-buffer formatter wrapper (P2216R3 runtime path + P0323R12) that returns std::expected<size_t, std::format_error>. Companion to Aug 25's format_to_n_into that closes the runtime-format-string follow-on from Aug 25's lesson notes. Same NUL-termination contract as Aug 25, but accepts std::string_view as the format string. Three runtime errors that the consteval gate on the Aug 22/24/25 wrapper surface caught at compile time appear here as std::format_error throws: bad parse (unterminated '{'), bad argument index (out-of-range specifier), bad type (string for a float specifier). Two-pass design: pass 1 measures the would-be size via std::vformat_to with a counting iterator (no allocations); pass 2 writes via a custom bounded_output_iterator that stops after n-1 chars (reserves one slot for the NUL). The bounded_output_iterator carries its counter through an external pointer because std::vformat_to makes a copy of the iterator at every advance — a member counter would be writing into a copy. Built on Apple Clang 21.0.0 / libc++."
    LANGUAGES CXX
)

# C++23 for std::vformat (P2216R3 runtime path), std::vformat_to,
# std::make_format_args, std::counted_iterator, std::move_iterator,
# std::format_error, and std::expected (P0323R12).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-26-format-to-n-runtime-string-expected
    P-2026-08-26-format-to-n-runtime-string-expected.cpp
)

target_compile_options(P-2026-08-26-format-to-n-runtime-string-expected PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-26-format-to-n-runtime-string-expected PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-26-format-to-n-runtime-string-expected PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
