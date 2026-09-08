# CMakeLists.txt — today's P-2026-09-08 lesson:
#
#   try_format_runtime_n — std::expected<std::size_t, FormatError>
#   wrapper around std::vformat_to that writes directly into a
#   caller-provided char* buffer (the std::format_to_n shape — NOT
#   std::format / std::format_to_string).  Closes Sep 7's "Where
#   we go next" item #1 — the BOUNDED-BUFFER sibling of Sep 7's
#   try_format_bounded (which returns std::string on success).
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
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6's
#              try_format_runtime_bounded; takes a std::format_string<Args...>
#              whose ctor is consteval, so malformed format strings are
#              HARD COMPILE ERRORS at the call site.
#   - TODAY    try_format_runtime_n — BOUNDED-BUFFER sibling of Sep 7's
#              try_format_bounded.  Today's wrapper returns
#              std::expected<std::size_t, petra::FormatError> and writes
#              into a caller-provided char* buffer (the std::format_to_n
#              shape — NOT std::format).  The format string is
#              std::string_view (the RUNTIME sibling — the consteval
#              sibling of this surface is Sep 7's open item #4 and is a
#              separate lesson).  Reuses petra::FormatError verbatim
#              from Sep 6 / Sep 7, and adds petra::detail::bounded_char_writer
#              — a callback-based output iterator that sinks through
#              out[written] = c up to a caller-provided cap, with the
#              same counter-via-callback design as Sep 6's
#              counting_output_iterator (the counter lives externally
#              because std::vformat_to copies the iterator internally).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSES Sep 6 /
#     Sep 7's exact shape (FormatParse / FormatArgument / Format /
#     TooLarge).
#   - petra::counting_output_iterator — REUSES Sep 6 / Sep 7's
#     callback-based iterator verbatim.
#   - petra::detail::bounded_char_writer — NEW.  A callback-based
#     output iterator that writes through out[written] = c and stops
#     accepting bytes once written == cap (mirrors std::format_to_n's
#     n cap).  Satisfies std::indirectly_writable / std::output_iterator
#     for const char& (the const qualifier on operator= is required by
#     std::indirectly_writable's const_cast<const proxy&&> = char
#     check).
#   - petra::try_format_runtime_n<Args...>(out, cap, std::string_view,
#     args...) — the BOUNDED-BUFFER sibling of Sep 6 / Sep 7.
#   - The two-pass measure-then-write design: pass 1 measures via
#     std::vformat_to + counting_output_iterator; pass 2 (only on
#     pass-1 success + needed <= cap) writes via std::vformat_to +
#     bounded_char_writer.  The invariant: pass 2 writes exactly
#     `needed` bytes (Section 11 verifies this with a direct
#     measurement probe).
#   - The structured-error contract from Sep 6: TooLarge{kind, message,
#     needed_bytes} on pass-1 measure-then-cap-fails; Format{kind,
#     message} on std::format_error.
#   - The bounded_char_writer's overflow behavior: bytes past `cap`
#     are silently dropped (matches std::format_to_n's n cap).  Section
#     17 exercises this directly.
#   - Section 1: toolchain + feature probes
#   - Section 2: fits-exactly happy path
#   - Section 3: cap larger than needed (no truncation; bytes past
#     written are still the poison byte — proves bounded writer stops
#     at the cap and does NOT NUL-terminate, distinguishing today
#     from Aug 25's format_to_n_into)
#   - Section 4: TooLarge (cap < needed) — buffer is UNTOUCHED on the
#     failure path (every byte still the poison byte 'Q')
#   - Section 5: Format error path (std::format_error caught in pass 1;
#     buffer is UNTOUCHED)
#   - Section 6: atomicity (failing call leaves prior success
#     untouched)
#   - Section 7: multi-type args round-trip (string, int, hex int,
#     float with precision, string with width specifiers)
#   - Section 8: std::counting_output_iterator availability probe
#   - Section 9: UTF-8 mid-codepoint byte round-trip
#   - Section 10: embedded NUL byte round-trip (uses std::string with
#     explicit length because std::string_view stops at the first NUL)
#   - Section 11: exact byte-count invariant (pass 2 written == pass
#     1 measured)
#   - Section 12: TooLarge boundary sweep (cap == needed-1, cap ==
#     needed, cap == needed+1) — buffer untouched on every TooLarge
#   - Section 13: FormatError equality / inequality
#   - Section 14: 50-round determinism on the size measurement
#   - Section 15: 4 threads x 25 calls each (each thread has its own
#     char[32] buffer; the wrapper is thread-safe as long as the
#     output buffer is not shared)
#   - Section 16: distinct from Sep 7's try_format_bounded (size_t
#     return + char* output location vs Sep 7's std::string return)
#   - Section 17: bounded_char_writer never writes past the cap (5
#     writes into a 4-byte buffer; the 5th is dropped silently)
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-08-try-format-runtime-n
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-08-try-format-runtime-n
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-08-try-format-runtime-n
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-08-try-format-runtime-n.cpp \
#           -o /tmp/P-2026-09-08-try-format-runtime-n

cmake_minimum_required(VERSION 3.16)
project(try_format_runtime_n_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of petra::try_format_runtime_n — std::expected<std::size_t, petra::FormatError> wrapper around std::vformat_to that writes directly into a caller-provided char* buffer (the std::format_to_n shape). The BOUNDED-BUFFER sibling of P-2026-09-07's try_format_bounded. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
    LANGUAGES CXX
)

# C++23 for std::format / std::vformat_to / std::expected (P0323R12) /
# std::print (P2093R14) / std::indirectly_writable / std::output_iterator.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable std::println on Apple Clang 21.0.0 / libc++ 21.  The <print>
# header's std::println / std::print are gated behind __cpp_lib_print in
# libc++ 21 — without -fexperimental-library the header is shipped but
# the names are not exported.  Same toolchain flag Sep 5 / Sep 6 /
# Sep 7 use.
add_compile_options(-fexperimental-library)

# Main lesson TU.
add_executable(P-2026-09-08-try-format-runtime-n
    P-2026-09-08-try-format-runtime-n.cpp
)

target_compile_options(P-2026-09-08-try-format-runtime-n PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-08-try-format-runtime-n PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-08-try-format-runtime-n PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()