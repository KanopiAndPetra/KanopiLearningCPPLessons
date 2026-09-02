# CMakeLists.txt — today's P-2026-09-02 lesson:
#
#   osyncstream_log_sink — std::osyncstream-backed sink for the
#   LOG_* macros shipping as the FIRST of three new forward-on
#   items Sep 1's "Where we go next" section named ("std::osyncstream
#   around the sink pointer" — closes the concurrency gap Sep 1
#   documented and explicitly named the natural next step).
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
#   - Sep  1   ostream_print_log_macros — LOG_DEBUG/INFO/WARN/ERROR
#              sugar over the Aug 28 adapter; named std::osyncstream as
#              the natural fix for the two-phase concurrent-emission gap.
#   - TODAY    osyncstream_log_sink — std::osyncstream around the sink
#              pointer closes the gap end-to-end (SYNC_LOG_* macros);
#              section 11 proves atomic concurrent line emission on
#              four threads / 250 calls / 1000 total commits; section
#              13 widens to eight threads / 200 calls / 1600 total
#              commits.
#
# What today covers
# -----------------
#   - petra::LogLevel enum + petra::log_level_tag (5-char aligned
#     tags) + petra::log_sink()& (raw, default &std::cerr) lifted
#     from Sep 1.
#   - petra::sync_sink_target()& — function-static
#     std::reference_wrapper<std::ostream> swappable to std::cerr,
#     a heap ostringstream, a std::ofstream, etc.  (The first
#     lesson to expose the underlying sink through
#     std::reference_wrapper rather than a raw pointer.)
#   - petra::sync_log_emit / sync_log_emit_runtime — emit prefix
#     and payload through the SAME std::osyncstream so the explicit
#     sync.flush() at the end commits them as ONE atomic line
#     (P0053R7 commit semantics).
#   - SYNC_LOG_DEBUG / SYNC_LOG_INFO / SYNC_LOG_WARN / SYNC_LOG_ERROR
#     macros (consteval-checked — std::format_string<Args...> ctor
#     runs at compile time) + SYNC_LOG_*_RUNTIME siblings.
#   - do-while(0) wrapping for if/else hygiene; identical recipe to
#     Sep 1.
#   - The platform finding: std::osyncstream on this Apple Clang 21
#     toolchain is gated behind -fexperimental-library.  The lesson
#     documents the gating and adds the flag to both CMake and the
#     direct compile commands.
#   - 16 sections / 55 CHECKs, all PASS on default + strict + ASan.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-02-osyncstream-log-sink
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-02-osyncstream-log-sink
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-02-osyncstream-log-sink

cmake_minimum_required(VERSION 3.16)
project(osyncstream_log_sink_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::osyncstream-backed SYNC_LOG_DEBUG/INFO/WARN/ERROR macros built on top of Sep 1's LOG_* macros — covers std::osyncstream commit semantics for atomic line emission, the reference_wrapper sink target (default std::cerr, swappable to a test ostringstream), the consteval-vs-runtime split at the SYNC macro layer, do-while-0 macro hygiene, 4-thread / 250-call concurrent atomicity, 8-thread / 200-call stress, NUL byte and UTF-8 payload passthrough, 50-round determinism + 5-run round-trip stability. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
    LANGUAGES CXX
)

# C++23 for std::format / std::format_string<Args...>.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable std::osyncstream on Apple Clang 21 / libc++ 21.
# std::osyncstream is gated behind _LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM
# in libc++ 21; the flag -fexperimental-library flips
# __has_feature(experimental_library) which the libc++ config reads
# to default the macro to 1.  Without this flag std::osyncstream is
# NOT a complete type on this toolchain (Section 1's static_assert
# fails to compile).
add_compile_options(-fexperimental-library)

add_executable(P-2026-09-02-osyncstream-log-sink
    P-2026-09-02-osyncstream-log-sink.cpp
)

target_compile_options(P-2026-09-02-osyncstream-log-sink PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-02-osyncstream-log-sink PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-02-osyncstream-log-sink PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
