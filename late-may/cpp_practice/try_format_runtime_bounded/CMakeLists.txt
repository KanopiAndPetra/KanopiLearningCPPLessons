# CMakeLists.txt — today's P-2026-09-06 lesson:
#
#   try_format_runtime_bounded — std::expected<std::string, FormatError>
#   wrapper around std::vformat_to that measures the byte count first via
#   a hand-rolled counting_output_iterator (libc++ 21 does NOT ship
#   std::counting_output_iterator at all — neither gated nor hidden, just
#   absent from the libc++ 21 source tree), and only writes when the
#   formatted output fits the user-provided cap. Returns
#   std::unexpected<FormatError> on either std::format_error or a
#   count-overruns-cap condition with a structured TooLarge variant that
#   carries the actual byte count that would have been written.
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
#              time; closed the LAST of Sep 1's three follow-on items.
#   - Sep  5   try_format_expected — std::expected<std::string,
#              FormatError> wrapper for std::format / std::vformat;
#              consteval sibling catches format errors at compile time,
#              runtime sibling catches std::format_error at format
#              time and converts it to FormatError{kind, message};
#              today's FormatError REUSES Sep 5's {kind, message} shape
#              and ADDS a fourth kind (TooLarge) plus a needed_bytes
#              field for the bounded wrapper.
#   - TODAY    try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with a caller-
#              provided byte cap; two-pass measure-then-write pair using
#              a hand-rolled counting_output_iterator; the headline
#              platform finding is that std::counting_output_iterator is
#              NOT shipped by libc++ 21 (verified by recursive grep
#              across /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/).
#
# What today covers
# -----------------
#   - petra::counting_output_iterator — a callback-based forward-output
#     iterator modelled on P2738R2 / [format.range]'s example shape; the
#     callback is a function pointer (void(*)(size_t, void*)) plus a
#     user-data slot so the count escapes regardless of how many copies
#     std::vformat_to makes of the iterator.
#   - petra::FormatError{kind, message, needed_bytes} — a structured
#     error type that REUSES Sep 5's FormatErrorKind enum (the three
#     C++23 exception subclasses) and ADDS TooLarge as the fourth value
#     for the bounded wrapper's specific failure mode.
#   - petra::try_format_runtime_bounded<Args...>(cap, fmt, args...) —
#     the bounded wrapper; runtime-format-string form (matching Sep 5's
#     try_format_runtime shape).  Returns std::expected<std::string,
#     petra::FormatError>; never throws.
#   - Section 1: toolchain + feature probes
#   - Section 2: fits-exactly happy path (cap == needed)
#   - Section 3: cap larger than needed (no truncation)
#   - Section 4: cap smaller than needed → TooLarge
#   - Section 5: std::format_error caught and routed to Format kind
#   - Section 6: atomicity — failing call leaves prior success untouched
#   - Section 7: multi-type args round-trip
#   - Section 8: std::counting_output_iterator availability probe
#   - Section 9: UTF-8 mid-codepoint byte round-trip
#   - Section 10: embedded NUL byte round-trip
#   - Section 11: exact byte-count invariant — measure pass == write pass
#   - Section 12: TooLarge boundary sweep at cap = needed-2..needed+95
#   - Section 13: FormatError equality / inequality
#   - Section 14: 50-round determinism on the size measurement
#   - Section 15: 4 threads x 25 calls each — every result matches expected
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-06-try-format-runtime-bounded
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-06-try-format-runtime-bounded
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-06-try-format-runtime-bounded
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-06-try-format-runtime-bounded.cpp \
#           -o /tmp/P-2026-09-06-try-format-runtime-bounded

cmake_minimum_required(VERSION 3.16)
project(try_format_runtime_bounded_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of petra::try_format_runtime_bounded — std::expected<std::string, petra::FormatError> wrapper around std::vformat_to with a caller-provided byte cap. Uses a hand-rolled counting_output_iterator because std::counting_output_iterator is NOT shipped by libc++ 21 (neither gated nor hidden — recursive grep across the libc++ 21 header tree returns zero matches). C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
    LANGUAGES CXX
)

# C++23 for std::format / std::vformat_to / std::format_string<Args...> /
# std::expected (P0323R12) / std::print (P2093R14) /
# std::indirectly_writable / std::output_iterator.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable std::println on Apple Clang 21.0.0 / libc++ 21.  The <print>
# header's std::println / std::print are gated behind __cpp_lib_print in
# libc++ 21 — without -fexperimental-library the header is shipped but
# the names are not exported.  This is the same toolchain flag Sep 5
# uses (P-2026-09-05-try-format-expected lesson).
add_compile_options(-fexperimental-library)

# Main lesson TU.
add_executable(P-2026-09-06-try-format-runtime-bounded
    P-2026-09-06-try-format-runtime-bounded.cpp
)

target_compile_options(P-2026-09-06-try-format-runtime-bounded PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-06-try-format-runtime-bounded PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-06-try-format-runtime-bounded PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()