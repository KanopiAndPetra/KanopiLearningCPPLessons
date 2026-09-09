# CMakeLists.txt — today's P-2026-09-09 lesson:
#
#   try_format_bounded_runtime_n — std::expected<std::size_t, FormatError>
#   wrapper around std::vformat_to that takes a caller-provided byte cap
#   AND a consteval-checked std::format_string<Args...> fmt.  Closes Sep
#   8's "Where we go next" item #1 — the CONSTEVAL sibling of Sep 8's
#   try_format_runtime_n (which accepts std::string_view fmt).
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23)
#   - Aug 24   std::format_to_n in std::expected (C++23)
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded formatter
#   - Aug 26   std::format_to_n_runtime_string — runtime-fmt wrapper
#   - Aug 27   format_to_n_runtime_string_projected — LogError projection
#   - Aug 28   std::ostream_print — std::print(ostream&, ...) adapter
#              closing the libc++ 21 missing-overload gap
#   - Aug 29   format_dynamic_args_store — runtime-format adapter
#              (explicit type list at the call site, runtime fmt)
#   - Aug 30   ostream_write_checked — partial-write detection for the
#              Aug 28 adapter (custom streambuf that refuses bytes)
#   - Aug 31   ostream_write_n_via_buf — std::format_to_n straight to
#              std::ostreambuf_iterator<char>
#   - Sep  1   ostream_print_log_macros — LOG_DEBUG/INFO/WARN/ERROR
#              sugar over the Aug 28 adapter.
#   - Sep  2   osyncstream_log_sink — std::osyncstream around the sink
#              pointer closes the two-phase-write concurrency gap.
#   - Sep  3   sync_log_file_line_macros — LOCSYNC_LOG_* macros that
#              prepend __FILE__:__LINE__ at the call site.
#   - Sep  4   sync_log_level_filter — PETRA_SYNC_LOG_LEVEL gate that
#              reduces gated levels to `((void)0)` no-ops at compile
#              time.
#   - Sep  5   try_format_expected — std::expected<std::string,
#              FormatError> wrapper for std::format / std::vformat.
#   - Sep  6   try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with a caller-
#              provided byte cap; two-pass measure-then-write pair
#              using a hand-rolled counting_output_iterator.
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#              std::expected<std::string, FormatError> wrapper for
#              std::vformat_to with std::format_string<Args...> fmt
#              (consteval-checked).
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of Sep 7.
#              std::expected<std::size_t, FormatError> wrapper that
#              writes directly into a caller-provided char* buffer
#              with std::string_view fmt.
#   - TODAY    try_format_bounded_runtime_n — the natural sibling of
#              P-2026-09-07's try_format_bounded with today's
#              bounded-buffer output location.  Combines:
#                - Sep 7's std::format_string<Args...> (consteval fmt check)
#                - Sep 8's char* output location
#                - Sep 8's std::expected<std::size_t, FormatError> return
#              Reuses petra::FormatError, petra::counting_output_iterator
#              and petra::detail::bounded_char_writer verbatim from
#              Sep 6 / Sep 7 / Sep 8.
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8's exact shape.
#   - petra::counting_output_iterator — REUSED Sep 6 / Sep 7 / Sep 8's
#     callback-based iterator verbatim.
#   - petra::detail::bounded_char_writer — REUSED Sep 8's callback-
#     based write sink verbatim.
#   - petra::try_format_bounded_runtime_n<Args...>(out, cap,
#     std::format_string<Args...>, args...) — the CONSTEVAL BOUNDED-BUFFER
#     sibling of Sep 6 / Sep 7 / Sep 8.
#   - The two-pass measure-then-write design: pass 1 measures via
#     std::vformat_to + counting_output_iterator; pass 2 (only on
#     pass-1 success + needed <= cap) writes via std::vformat_to +
#     bounded_char_writer.  The invariant: pass 2 writes exactly
#     `needed` bytes.
#   - The structured-error contract: TooLarge{kind, message,
#     needed_bytes} on pass-1 measure-then-cap-fails; Format{kind,
#     message} on std::format_error from the std::make_format_args
#     side.
#   - The consteval ctor of std::format_string<Args...> rejects
#     malformed format strings as HARD COMPILE ERRORS at the call
#     site.  The compile-time probes in /tmp/probe_consteval_* verify
#     this; the contrast probe /tmp/probe_runtime_accepts_unterminated
#     verifies Sep 8's runtime sibling accepts the same string.
#   - The bounded_char_writer's overflow behavior: bytes past `cap`
#     are silently dropped (matches std::format_to_n's n cap).
#   - Section 1: toolchain + feature probes
#   - Section 2: fits-exactly happy path
#   - Section 3: cap larger than needed (no truncation; bytes past
#     written are still the poison byte — proves bounded writer
#     stops at the cap and does NOT NUL-terminate, distinguishing
#     today from Aug 25's format_to_n_into)
#   - Section 4: TooLarge (cap < needed) — buffer is UNTOUCHED on
#     every TooLarge path
#   - Section 5: consteval fmt positive (valid format strings;
#     verify byte count + content for four cases)
#   - Section 6: atomicity (failing call leaves prior success
#     untouched)
#   - Section 7: multi-type args round-trip (string, int, hex int,
#     float with precision, string with width specifiers)
#   - Section 8: std::counting_output_iterator availability probe
#   - Section 9: UTF-8 mid-codepoint byte round-trip
#   - Section 10: embedded NUL byte round-trip
#   - Section 11: exact byte-count invariant (pass 2 written ==
#     pass 1 measured)
#   - Section 12: TooLarge boundary sweep (cap == needed-1, cap ==
#     needed, cap == needed+1) — buffer untouched on every TooLarge
#   - Section 13: FormatError equality / inequality
#   - Section 14: 50-round determinism on the size measurement
#   - Section 15: 4 threads x 25 calls each (each thread has its own
#     char[32] buffer; the wrapper is thread-safe as long as the
#     output buffer is not shared)
#   - Section 16: distinct from Sep 8's try_format_runtime_n
#     (consteval fmt vs runtime fmt; byte-count and error contract
#     agree)
#   - Section 17: bounded_char_writer never writes past the cap (5
#     writes into a 4-byte buffer; the 5th is dropped silently)
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-09-try-format-bounded-runtime-n
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-09-try-format-bounded-runtime-n
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-09-try-format-bounded-runtime-n
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-09-try-format-bounded-runtime-n.cpp \
#           -o /tmp/P-2026-09-09-try-format-bounded-runtime-n

cmake_minimum_required(VERSION 3.16)
project(try_format_bounded_runtime_n_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of petra::try_format_bounded_runtime_n — std::expected<std::size_t, petra::FormatError> wrapper around std::vformat_to with a caller-provided byte cap and a consteval-checked std::format_string<Args...> fmt. The CONSTEVAL BOUNDED-BUFFER sibling of P-2026-09-07's try_format_bounded + P-2026-09-08's try_format_runtime_n. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
    LANGUAGES CXX
)

# C++23 for std::format / std::vformat_to / std::expected (P0323R12) /
# std::format_string<Args...> (P2216R3) / std::print (P2093R14) /
# std::indirectly_writable / std::output_iterator.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable std::println on Apple Clang 21.0.0 / libc++ 21.  The <print>
# header's std::println / std::print are gated behind __cpp_lib_print in
# libc++ 21 — without -fexperimental-library the header is shipped but
# the names are not exported.  Same toolchain flag Sep 5 / Sep 6 /
# Sep 7 / Sep 8 use.
add_compile_options(-fexperimental-library)

# Main lesson TU.
add_executable(P-2026-09-09-try-format-bounded-runtime-n
    P-2026-09-09-try-format-bounded-runtime-n.cpp
)

target_compile_options(P-2026-09-09-try-format-bounded-runtime-n PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-09-try-format-bounded-runtime-n PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-09-try-format-bounded-runtime-n PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
