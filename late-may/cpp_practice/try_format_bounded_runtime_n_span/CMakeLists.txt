# CMakeLists.txt — today's P-2026-09-10 lesson:
#
#   try_format_bounded_runtime_n_span — std::expected<std::size_t, FormatError>
#   wrapper around std::vformat_to that takes a caller-provided
#   std::span<char> buffer AND a consteval-checked
#   std::format_string<Args...> fmt.  Closes Sep 9's "Where we go next"
#   item #3 — the std::span<char> sibling of Sep 9's char*+size_t
#   surface, analogous to Aug 25's format_to_n_into(std::span<char>,
#   ...) span overload.  Aug 25's lesson is the NUL-terminating
#   sibling; today is the raw (no-NUL-terminating) sibling.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter with std::span<char> overload
#   - Sep  6   try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with cap
#              (std::string output).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of Sep 7
#              (char* buffer + std::string_view fmt).
#   - Sep  9   try_format_bounded_runtime_n — the CONSTEVAL
#              BOUNDED-BUFFER combination of Sep 7 + Sep 8.
#   - TODAY    try_format_bounded_runtime_n_span — the STDSPAN sibling
#              of Sep 9.  Mirrors Aug 25's pattern of lifting the raw
#              (char*, size_t) surface to a (std::span<char>) primary
#              overload.
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 verbatim.
#   - petra::counting_output_iterator — REUSED Sep 6 / Sep 7 / Sep 8 /
#     Sep 9 verbatim (still hand-rolled — std::counting_output_iterator
#     is NOT shipped by libc++ 21; Sep 9's static_assert is the
#     migration trigger).
#   - petra::detail::bounded_char_writer — REUSED Sep 8 / Sep 9 verbatim.
#   - petra::detail::try_format_bounded_runtime_n_impl — REUSED Sep 9
#     verbatim.
#   - petra::try_format_bounded_runtime_n<Args...>(char* out,
#     std::size_t cap, std::format_string<Args...>, args...) — KEPT as
#     a forwarding shim so Sep 9's tests continue to work.
#   - petra::try_format_bounded_runtime_n<Args...>(std::span<char> out,
#     std::format_string<Args...>, args...) — the NEW span primary
#     overload.  Thin forwarder that extracts .data() + .size() and
#     calls the existing impl.
#   - The sub-span no-bleed surface: a std::span<char> over a sub-range
#     of the buffer must NOT touch bytes outside the span.  Section 12
#     pins this with a 6-byte sub-span at offset 5 of a 16-byte buffer.
#   - The raw (no-NUL-terminating) shape: bytes past `written` are
#     left untouched.  Section 16 distinguishes today from Aug 25's
#     format_to_n_into span overload (which DOES NUL-terminate).
#   - std::span<char> must satisfy std::ranges::contiguous_range /
#     std::ranges::sized_range, must be trivially copyable, must have
#     .data() -> char* and .size() -> std::size_t.  Section 1 pins
#     these.
#   - Section 1: toolchain + feature probes
#   - Section 2: fits-exactly happy path
#   - Section 3: span larger than needed (no truncation; bytes past
#     written are still the poison byte — proves bounded writer
#     stops at the cap and does NOT NUL-terminate, distinguishing
#     today from Aug 25's format_to_n_into span overload)
#   - Section 4: TooLarge (span smaller than needed) — buffer is
#     UNTOUCHED on every TooLarge path
#   - Section 5: consteval fmt positive (valid format strings; verify
#     byte count + content for four cases)
#   - Section 6: atomicity (failing call leaves prior success
#     untouched; span version preserves the previous bytes in the
#     shared underlying buffer)
#   - Section 7: multi-type args round-trip (string, int, hex int,
#     float with precision, string with width specifiers)
#   - Section 8: UTF-8 mid-codepoint byte round-trip
#   - Section 9: embedded NUL byte round-trip
#   - Section 10: exact byte-count invariant (pass 2 written ==
#     pass 1 measured)
#   - Section 11: TooLarge boundary sweep (span size == needed-1,
#     span size == needed, span size == needed+1)
#   - Section 12: sub-span no-bleed (span over a sub-range does
#     NOT touch bytes outside the span)
#   - Section 13: char*-and-cap + span overloads produce identical
#     results for identical buffer + format + args
#   - Section 14: 50-round determinism on the size measurement
#   - Section 15: 4 threads x 25 calls each (each thread has its own
#     char[32] buffer; each call constructs its own std::span<char>
#     over that buffer)
#   - Section 16: distinct from Aug 25's format_to_n_into span
#     overload (NO NUL-termination)
#   - Section 17: bounded_char_writer never writes past the cap (5
#     writes into a 4-byte buffer; the 5th is dropped silently)
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-10-try-format-bounded-runtime-n-span
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-10-try-format-bounded-runtime-n-span
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-10-try-format-bounded-runtime-n-span
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
#           -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span

cmake_minimum_required(VERSION 3.16)
project(try_format_bounded_runtime_n_span_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of petra::try_format_bounded_runtime_n (std::span<char> overload) — std::expected<std::size_t, petra::FormatError> wrapper around std::vformat_to that takes a std::span<char> + a consteval-checked std::format_string<Args...> fmt. The STDSPAN sibling of P-2026-09-09's try_format_bounded_runtime_n (which takes char*+size_t). C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
    LANGUAGES CXX
)

# C++23 for std::format / std::vformat_to / std::expected (P0323R12) /
# std::format_string<Args...> (P2216R3) / std::print (P2093R14) /
# std::span<char> (P0122R7) / std::ranges::contiguous_range /
# std::indirectly_writable / std::output_iterator.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable std::println on Apple Clang 21.0.0 / libc++ 21.  The <print>
# header's std::println / std::print are gated behind __cpp_lib_print in
# libc++ 21 — without -fexperimental-library the header is shipped but
# the names are not exported.  Same toolchain flag Sep 5 / Sep 6 /
# Sep 7 / Sep 8 / Sep 9 use.
add_compile_options(-fexperimental-library)

# Main lesson TU.
add_executable(P-2026-09-10-try-format-bounded-runtime-n-span
    P-2026-09-10-try-format-bounded-runtime-n-span.cpp
)

target_compile_options(P-2026-09-10-try-format-bounded-runtime-n-span PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-10-try-format-bounded-runtime-n-span PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-10-try-format-bounded-runtime-n-span PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
