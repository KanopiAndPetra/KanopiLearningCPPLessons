# CMakeLists.txt — today's P-2026-09-05 lesson:
#
#   try_format_expected — std::expected<std::string, FormatError>
#   wrapper around std::format / std::vformat that catches
#   std::format_error and converts it into a structured error type.
#   Pairs a consteval entry point (std::format_string<Args...>) with
#   a runtime entry point (std::string_view + std::make_format_args).
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
#   - TODAY    try_format_expected — std::expected<std::string,
#              FormatError> wrapper for std::format / std::vformat;
#              consteval sibling catches format errors at compile time,
#              runtime sibling catches std::format_error at format
#              time and converts it to FormatError{kind, message};
#              the headline platform finding is that libc++ 21 does
#              NOT export std::format_parse_error or
#              std::format_argument_error — only the base
#              std::format_error is shipped — so the wrapper's
#              FormatErrorKind collapses to Format on this toolchain.
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message} — a structured error type
#     for std::format_error.  FormatErrorKind has three values
#     (FormatParse, FormatArgument, Format) mapped to the three
#     C++23 exception subclasses; the Format kind is the only one
#     actually observed on libc++ 21 (the platform finding).
#   - petra::try_format_runtime<Args...>(fmt, args...) — runtime-
#     format-string wrapper, takes std::string_view fmt and
#     forwards to std::vformat with std::make_format_args.  Returns
#     std::expected<std::string, FormatError>; never throws.
#   - petra::try_format<Args...>(fmt, args...) — consteval sibling,
#     takes std::format_string<Args...> fmt (the ctor is consteval
#     so malformed format strings are HARD COMPILE ERRORS).
#     Returns std::expected<std::string, FormatError>.
#   - Section 1: toolchain + feature probes
#   - Section 2: try_format_runtime happy path (multiple types)
#   - Section 3: try_format consteval sibling happy path
#   - Section 4: parse-failure cases — unterminated {:, too-few
#     args, too-many args (silently accepted), bare closing brace,
#     nested replacement field (valid in C++23 P2738R2)
#   - Section 5: atomicity — failing call leaves prior successful
#     value untouched
#   - Section 6: consteval runtime-error happy path
#   - Section 7: consteval-gate probe via static_assert
#   - Section 8: platform subclass finding — std::type_info::name()
#     confirms libc++ 21 only ships std::format_error
#   - Section 9: FormatError inspection — equality, empty message,
#     large (20000 char) message, NUL byte passthrough, UTF-8
#     passthrough, copy equality
#   - Section 10: UTF-8 mid-codepoint byte round-trip + embedded
#     NUL byte round-trip via try_format_runtime
#   - Section 11: 50-round determinism — failing fmt messages
#     stable per-fmt across rounds; 50 happy-path rounds consistent
#   - Section 12: 4 threads × 50 calls each on try_format_runtime
#     happy path; 4 threads × 50 failing calls each on the error
#     path; zero mismatches
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-05-try-format-expected
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-05-try-format-expected
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-05-try-format-expected
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-05-try-format-expected.cpp \
#           -o /tmp/P-2026-09-05-try-format-expected

cmake_minimum_required(VERSION 3.16)
project(try_format_expected_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of petra::try_format and petra::try_format_runtime — std::expected<std::string, petra::FormatError> wrapper around std::format / std::vformat that catches std::format_error and converts it into a structured error type. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
    LANGUAGES CXX
)

# C++23 for std::format / std::format_string<Args...> /
# std::expected (P0323R12) / std::print (P2093R14).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable std::println on Apple Clang 21.0.0 / libc++ 21.  The
# <print> header's std::println / std::print are gated behind
# __cpp_lib_print in libc++ 21 — without -fexperimental-library
# the header is shipped but the names are not exported.  This
# is the same toolchain flag Sep 2 / Sep 3 / Sep 4 lessons use
# for std::osyncstream (which has its own _LIBCPP_HAS_EXPERIMENTAL
# gate in <syncstream>).
add_compile_options(-fexperimental-library)

# Main lesson TU.
add_executable(P-2026-09-05-try-format-expected
    P-2026-09-05-try-format-expected.cpp
)

target_compile_options(P-2026-09-05-try-format-expected PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-05-try-format-expected PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-05-try-format-expected PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
