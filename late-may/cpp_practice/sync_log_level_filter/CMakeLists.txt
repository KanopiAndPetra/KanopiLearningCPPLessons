# CMakeLists.txt — today's P-2026-09-04 lesson:
#
#   sync_log_level_filter — PETRA_SYNC_LOG_LEVEL compile-time gate
#   for the Sep 2 SYNC_LOG_* and Sep 3 LOCSYNC_LOG_* macros.  A
#   level above the gate reduces the macro to `((void)0)` so a
#   release build pays zero cost for filtered-out levels.  Closes
#   the THIRD and final follow-on item Sep 1's "Where we go next"
#   section named (the "level-filter compile-time gate" item Sep
#   1 / Sep 2 / Sep 3 carried forward).
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
#   - Sep  3   sync_log_file_line_macros — LOCSYNC_LOG_* macros that
#              prepend __FILE__:__LINE__ at the call site while
#              routing the WHOLE line (level + file:line + payload)
#              through the SAME std::osyncstream so atomicity is
#              preserved; section 11 proves atomic concurrent
#              emission on 4 threads / 250 calls / 1000 total
#              commits; section 13 widens to 8 threads / 200 calls
#              / 1600 total commits; section 15 runs 5x determinism.
#   - TODAY    sync_log_level_filter — PETRA_SYNC_LOG_LEVEL gate that
#              reduces gated levels to `((void)0)` no-ops at compile
#              time; default gate=0 matches Sep 2 / Sep 3 behaviour
#              exactly; release builds set -DPETRA_SYNC_LOG_LEVEL=2
#              to drop DEBUG + INFO at compile time; the lesson's
#              main TU tests gate=0 (Sections 1-16) and a separate
#              gate-verify TU tests every gate value (0/1/2/3) and
#              confirms the captured-line count matches the
#              gate-emit rule (16/12/8/4).
#
# What today covers
# -----------------
#   - Lifted Parts 1-6 from Sep 2 / Sep 3 verbatim (petra::LogLevel /
#     log_level_tag / log_sink, petra::sync_sink_target, petra::sync_log_emit /
#     sync_log_emit_runtime, petra::sync_log_emit_loc /
#     sync_log_emit_loc_runtime, and the SYNC_LOG_* + LOCSYNC_LOG_*
#     macros).
#   - Part 7 — the PETRA_SYNC_LOG_LEVEL #define (default 0) +
#     the `#if PETRA_SYNC_LOG_LEVEL <= N` gates on each SYNC_LOG_*
#     and LOCSYNC_LOG_* macro.  Below the gate the macro is
#     `((void)0)`; at or above the gate the macro is unchanged from
#     Sep 2 / Sep 3.
#   - petra::sync_log_level()& runtime diagnostic accessor returning
#     the COMPILE-TIME gate value (read-only — the gate is fixed
#     for the lifetime of the binary; no runtime setter).
#   - Section 14 macro-stringify probe via the double-indirection
#     stringification trick (PETRA_STRINGIFY_INNER(x) / PETRA_STRINGIFY(x))
#     + a PETRA_PROBE_SYNC_LOG_*(void) probe macro.  Static-asserts
#     prove the macro expansion matches the expected string at the
#     current gate.  At gate=0 all four macros expand to the
#     full do-block emit form; at gate>0 some macros expand to
#     `((void)0)`.
#   - The platform finding inherited from Sep 2: std::osyncstream
#     on this Apple Clang 21 / libc++ 21 toolchain is gated behind
#     -fexperimental-library.  Sep 2 owns the finding; today
#     replicates only the build recipe.
#   - The LIFETIME-FREE LVALUE finding the lesson pins: the runtime
#     format-string macros (Sep 1 / Sep 2 / Sep 3) require
#     std::make_format_args(lvalue) because make_format_args takes
#     lvalue-refs.  At higher gates the macros are `((void)0)` so
#     the lvalue locals become unused; the strict-warning build
#     fails.  Production code must gate the lvalue declarations
#     behind `#if PETRA_SYNC_LOG_LEVEL <= N` too.  The gate-verify
#     TU documents this with `(void)d_val;` discard statements
#     keyed on the gate.
#   - 16 sections / 93 CHECKs in the main TU; 1 CHECK in the
#     gate-verify TU; all PASS on default + strict + ASan builds
#     (main TU at gate=0) and on gate=0/1/2/3 builds (gate-verify
#     TU).
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-04-sync-log-level-filter
#   ./build/P-2026-09-04-sync-log-level-filter-gate-verify-0
#   ./build/P-2026-09-04-sync-log-level-filter-gate-verify-1
#   ./build/P-2026-09-04-sync-log-level-filter-gate-verify-2
#   ./build/P-2026-09-04-sync-log-level-filter-gate-verify-3
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-09-04-sync-log-level-filter
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-04-sync-log-level-filter
#
# Higher-gate build (the dead-strip finding, manually verified):
#
#   clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
#           -DPETRA_SYNC_LOG_LEVEL=2 -Wall -Wextra -Wpedantic -Werror \
#           -Wshadow -Wconversion -Wsign-conversion \
#           P-2026-09-04-sync-log-level-filter-gate-verify.cpp \
#           -o /tmp/gate-verify-2 && /tmp/gate-verify-2
#   # expected: -- captured line count = 8 (expected 8)

cmake_minimum_required(VERSION 3.16)
project(sync_log_level_filter_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of the PETRA_SYNC_LOG_LEVEL compile-time gate for the Sep 2 SYNC_LOG_* and Sep 3 LOCSYNC_LOG_* macros — covers the [LEVEL ] file:line payload\\n shape unchanged at gate=0, the #if PETRA_SYNC_LOG_LEVEL <= N preprocessor gate that reduces gated levels to ((void)0) no-ops, the petra::sync_log_level() runtime diagnostic accessor, the macro-stringify probe via the double-indirection stringification trick + PETRA_PROBE_SYNC_LOG_* helper macros, the static_assert identity check on the macro expansion, the gate-verify driver at all four gate values (16/12/8/4 emitted lines), the lifetime-free lvalue finding for std::make_format_args + gated macro combinations. C++23, Apple Clang 21.0.0 / libc++ 21 / -fexperimental-library."
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
# NOT a complete type on this toolchain.  Sep 2 lesson's headline
# finding; the build recipe is replicated here unchanged.
add_compile_options(-fexperimental-library)

# Main lesson TU — gate=0 baseline (matches Sep 2 / Sep 3 behaviour).
add_executable(P-2026-09-04-sync-log-level-filter
    P-2026-09-04-sync-log-level-filter.cpp
)

target_compile_options(P-2026-09-04-sync-log-level-filter PRIVATE -fexceptions)

# Gate-verify driver — built at every gate value (0/1/2/3) and
# asserts the captured-line count matches the gate-emit rule.
# Each build sets PETRA_SYNC_LOG_LEVEL via target_compile_definitions.
foreach(gate IN ITEMS 0 1 2 3)
    add_executable(P-2026-09-04-sync-log-level-filter-gate-verify-${gate}
        P-2026-09-04-sync-log-level-filter-gate-verify.cpp
    )
    target_compile_options(P-2026-09-04-sync-log-level-filter-gate-verify-${gate}
        PRIVATE -fexceptions)
    target_compile_definitions(P-2026-09-04-sync-log-level-filter-gate-verify-${gate}
        PRIVATE PETRA_SYNC_LOG_LEVEL=${gate})
endforeach()

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    foreach(target
            P-2026-09-04-sync-log-level-filter
            P-2026-09-04-sync-log-level-filter-gate-verify-0
            P-2026-09-04-sync-log-level-filter-gate-verify-1
            P-2026-09-04-sync-log-level-filter-gate-verify-2
            P-2026-09-04-sync-log-level-filter-gate-verify-3)
        target_compile_options(${target} PRIVATE
            -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address -fsanitize=undefined
        )
    endforeach()
endif()
