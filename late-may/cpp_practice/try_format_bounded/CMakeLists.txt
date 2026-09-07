# CMakeLists.txt — today's P-2026-09-07 lesson:
#
#   try_format_bounded — std::expected<std::string, FormatError>
#   wrapper around std::format that takes a caller-provided byte cap
#   AND a consteval-checked std::format_string<Args...> fmt.  Closes
#   Sep 6's "Where we go next" item #1 — the CONSTEVAL sibling of
#   P-2026-09-06's try_format_runtime_bounded.
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
#              runtime sibling catches std::format_error at format time
#              and converts it to FormatError{kind, message}.
#   - Sep  6   try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with a caller-
#              provided byte cap; two-pass measure-then-write pair
#              using a hand-rolled counting_output_iterator; the
#              headline platform finding is that std::counting_output_iterator
#              is NOT shipped by libc++ 21 (verified by recursive grep
#              across /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/).
#   - TODAY    try_format_bounded — CONSTEVAL sibling of Sep 6's
#              try_format_runtime_bounded; takes a std::format_string<Args...>
#              whose ctor is consteval, so malformed format strings are
#              HARD COMPILE ERRORS at the call site (the runtime
#              FormatError{Format, ...} branch becomes unreachable for
#              parse / argument errors; only TooLarge survives as a
#              runtime error in the bounded wrapper).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSES Sep 6's
#     exact shape (FormatParse / FormatArgument / Format / TooLarge).
#   - petra::counting_output_iterator — REUSES Sep 6's hand-rolled
#     callback-based iterator verbatim.
#   - petra::try_format_bounded<Args...>(cap, std::format_string<Args...>,
#     args...) — the CONSTEVAL sibling of Sep 6's wrapper.
#   - The headline finding: malformed format strings become HARD
#     COMPILE ERRORS on the consteval path.  Section 5 (positive) and
#     a separate compile-time probe (saved in /tmp) pin this.
#   - The second finding: arg-type mismatches between the format
#     string and the args also become HARD COMPILE ERRORS (e.g.
#     "{:d}" with a std::string arg — verified with a separate probe).
#   - Section 1: toolchain + feature probes
#   - Section 2: fits-exactly happy path (cap == needed)
#   - Section 3: cap larger than needed (no truncation)
#   - Section 4: cap smaller than needed → TooLarge
#   - Section 5: consteval fmt positive — std::format parity
#   - Section 6: atomicity — failing call leaves prior success untouched
#   - Section 7: multi-type args round-trip
#   - Section 8: std::counting_output_iterator availability probe
#   - Section 9: UTF-8 mid-codepoint byte round-trip
#   - Section 10: embedded NUL byte round-trip
#   - Section 11: exact byte-count invariant
#   - Section 12: TooLarge boundary sweep
#   - Section 13: FormatError equality / inequality
#   - Section 14: 50-round determinism
#   - Section 15: 4 threads x 25 calls each
#   - Section 16: format-side-runtime (width passthrough)
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-07-try-format-bounded
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-07-try-format-bounded
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-07-try-format-bounded
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-07-try-format-bounded.cpp \
#           -o /tmp/P-2026-09-07-try-format-bounded

cmake_minimum_required(VERSION 3.16)
project(try_format_bounded_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of petra::try_format_bounded — std::expected<std::string, petra::FormatError> wrapper around std::format with a caller-provided byte cap AND a consteval-checked std::format_string<Args...> fmt. The CONSTEVAL sibling of P-2026-09-06's try_format_runtime_bounded. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
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
# the names are not exported.  Same toolchain flag Sep 5 / Sep 6 use.
add_compile_options(-fexperimental-library)

# Main lesson TU.
add_executable(P-2026-09-07-try-format-bounded
    P-2026-09-07-try-format-bounded.cpp
)

target_compile_options(P-2026-09-07-try-format-bounded PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-07-try-format-bounded PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-07-try-format-bounded PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
