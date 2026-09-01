# CMakeLists.txt — today's P-2026-09-01 lesson:
#
#   ostream_print_log_macros — std::print-compatible LOG_DEBUG /
#   LOG_INFO / LOG_WARN / LOG_ERROR macros built on top of the
#   Aug 28 consumer-side std::print(std::ostream&, ...) adapter.
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
#   - Aug 30   ostream_write_checked — partial-write detection for the
#              Aug 28 adapter (custom streambuf that refuses bytes)
#   - Aug 31   ostream_write_n_via_buf — std::format_to_n straight to
#              std::ostreambuf_iterator<char>
#   - TODAY    ostream_print_log_macros — LOG_DEBUG / LOG_INFO / LOG_WARN
#              / LOG_ERROR macros on top of the Aug 28 adapter; closes
#              the FOURTH of the four new follow-on items Aug 28's
#              "Where we go next" listed: "std::print-compatible log
#              macros — LOG_INFO('value={}', 42) sugar over the adapter."
#
# What today covers
# -----------------
#   - petra::LogLevel enum (Debug/Info/Warn/Error) with a 5-char
#     aligned tag ("DEBUG", "INFO ", "WARN ", "ERROR") so log lines
#     align in columns.
#   - petra::log_sink() returns std::ostream*&, defaulting to
#     std::cerr, swappable to a heap / test ostringstream.
#   - LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR macros — consteval-
#     checked (std::format_string<Args...>) at the call site; the
#     format string is a literal and parse / argument-mismatch
#     errors are HARD COMPILE ERRORS at the macro call site.
#   - LOG_DEBUG_RUNTIME / LOG_INFO_RUNTIME / LOG_WARN_RUNTIME /
#     LOG_ERROR_RUNTIME macros — runtime-format-string sibling taking
#     (std::string_view fmt, std::format_args args).
#   - do-while(0) wrapping so `if (x) LOG_INFO(...); else ...;`
#     binds correctly.
#   - The consteval gate's contract — confirmed via a `static_assert`
#     probe in section 7.
#   - Macro-hygiene: comma-in-arguments (std::pair<int,int>), NUL
#     byte in payload, UTF-8 in payload, sink-pointer reset to
#     nullptr silences output.
#   - 15 sections / 41 CHECKs, all PASS on default + strict + ASan.
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-01-ostream-print-log-macros
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-01-ostream-print-log-macros
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-01-ostream-print-log-macros

cmake_minimum_required(VERSION 3.16)
project(ostream_print_log_macros_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::print-compatible LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR macros built on top of the Aug 28 std::print(std::ostream&, ...) consumer-side adapter — covers the consteval-vs-runtime split at the macro layer, do-while-0 macro hygiene, sink-pointer swap (default std::cerr, swappable to std::ostringstream for tests), aligned 5-char log-level tags, runtime-format-string sibling LOG_*_RUNTIME, NUL byte and UTF-8 payload passthrough, 50-round determinism stress. C++23, Apple Clang 21.0.0 / libc++."
    LANGUAGES CXX
)

# C++23 for std::format / std::format_string<Args...>.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-09-01-ostream-print-log-macros
    P-2026-09-01-ostream-print-log-macros.cpp
)

target_compile_options(P-2026-09-01-ostream-print-log-macros PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-01-ostream-print-log-macros PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-01-ostream-print-log-macros PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()