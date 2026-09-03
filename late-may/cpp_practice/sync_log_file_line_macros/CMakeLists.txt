# CMakeLists.txt — today's P-2026-09-03 lesson:
#
#   sync_log_file_line_macros — file/line-aware SYNC_LOG_* macros
#   that capture __FILE__ and __LINE__ at the call site while
#   preserving the atomic-line property Sep 2's osyncstream commit
#   semantics give us.  Closes the SECOND of three new forward-on
#   items Sep 1's "Where we go next" section named (the
#   "__FILE__ / __LINE__ capture" item Sep 2 carried forward).
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
#   - Sep  2   osyncstream_log_sink — std::osyncstream around the sink
#              pointer closes the gap end-to-end (SYNC_LOG_* macros);
#              section 11 proves atomic concurrent line emission on
#              four threads / 250 calls / 1000 total commits.
#   - TODAY    sync_log_file_line_macros — LOCSYNC_LOG_* macros that
#              prepend __FILE__:__LINE__ at the call site while
#              routing the WHOLE line (level + file:line + payload)
#              through the SAME std::osyncstream so atomicity is
#              preserved; section 11 proves atomic concurrent
#              emission on 4 threads / 250 calls / 1000 total
#              commits; section 13 widens to 8 threads / 200 calls
#              / 1600 total commits; section 15 runs 5x determinism.
#
# What today covers
# -----------------
#   - Lifted Parts 1-4 from Sep 2 verbatim (petra::ostream_println,
#     petra::LogLevel / log_level_tag / log_sink, petra::sync_sink_target,
#     petra::sync_log_emit / sync_log_emit_runtime, the SYNC_LOG_*
#     macros).
#   - petra::sync_log_emit_loc / sync_log_emit_loc_runtime — the
#     file-and-line variant of sync_log_emit.  Both emit
#     "[LEVEL ] file:line payload\n" through a single
#     std::osyncstream so the committed line stays atomic
#     (P0053R7 commit semantics).
#   - LOCSYNC_LOG_DEBUG / LOCSYNC_LOG_INFO / LOCSYNC_LOG_WARN /
#     LOCSYNC_LOG_ERROR macros (consteval-checked — captures
#     __FILE__ and __LINE__ automatically; the call site never
#     names them) + LOCSYNC_LOG_*_RUNTIME runtime-format-string
#     siblings.
#   - do-while(0) wrapping for if/else hygiene; identical recipe
#     to Sep 1 / Sep 2.
#   - The platform finding inherited from Sep 2: std::osyncstream
#     on this Apple Clang 21 / libc++ 21 toolchain is gated
#     behind -fexperimental-library.  Sep 2 owns the finding;
#     today replicates only the build recipe.
#   - 16 sections / 51 CHECKs, all PASS on default + strict + ASan.
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-03-sync-log-file-line-macros
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-03-sync-log-file-line-macros
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-03-sync-log-file-line-macros

cmake_minimum_required(VERSION 3.16)
project(sync_log_file_line_macros_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of file/line-aware LOCSYNC_LOG_DEBUG/INFO/WARN/ERROR macros built on top of Sep 2's std::osyncstream-backed SYNC_LOG_* surface — covers the [LEVEL ] <file>:<line> <payload>\\n committed-line shape, automatic __FILE__ and __LINE__ capture at the call site, atomic concurrent emission under the longer prefix, the consteval-vs-runtime split at the LOCSYNC macro layer, do-while-0 macro hygiene, 4-thread / 250-call concurrent atomicity with file:line on every committed line, 8-thread / 200-call stress, 5x determinism round-trip. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
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
# fails to compile).  Sep 2 lesson's headline finding; the build
# recipe is replicated here unchanged.
add_compile_options(-fexperimental-library)

add_executable(P-2026-09-03-sync-log-file-line-macros
    P-2026-09-03-sync-log-file-line-macros.cpp
)

target_compile_options(P-2026-09-03-sync-log-file-line-macros PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-09-03-sync-log-file-line-macros PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-09-03-sync-log-file-line-macros PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
