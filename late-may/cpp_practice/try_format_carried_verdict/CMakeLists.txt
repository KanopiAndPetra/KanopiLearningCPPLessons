# CMakeLists.txt — today's P-2026-11-12 lesson:
#
#   try_format_carried_verdict — the fallback CARRIES the scanner's
#   verdict instead of discarding it, and the numeric ceiling (which the
#   standard does not fix, and each implementation chooses for itself)
#   becomes a per-implementation datum in the policy table.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind (4 kinds).
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - Oct 27           the contract comparison against std::format_to_n.
#   - Oct 28           the ERROR TAXONOMY — a constexpr grammar scanner
#                      plus a libc++-specific message table.
#   - Oct 30           the TYPE-AWARE scanner — the argument's category
#                      decides the spec-vs-argument class.
#   - Nov 7            the POLICY TABLE — one mask table plus
#                      per-implementation override rows.
#   - TODAY            the CARRIED VERDICT — Nov 7's next-step item (1),
#                      closed: the scanner's silence is carried into the
#                      fallback as silence, and the numeric ceiling joins
#                      the policy table as a per-implementation scalar.
#
# What today measures (see the .md for the detail):
#
#   - The ceiling of the LINKED stdlib, measured by binary search over
#     the format string, then CHECKED against the hand-written policy
#     row: libc++ 21 -> 2147483647 (INT32_MAX), libstdc++ 15 -> 65535
#     (unsigned short).  The standard fixes no ceiling at all.
#   - The 1,596-cell grid moves by ZERO cells: every one of its rejects
#     is decided by the scanner, so the grid never reaches the fallback
#     and cannot see this defect.
#   - The census, over the families the grid omits: the ceiling rule
#     moves 4 cells under either toolchain; the carried verdict moves 1
#     more under libc++ (where the message vocabulary lets it) and 0
#     under libstdc++ (where every message is unrecognised).
#   - One FALSE POSITIVE the arc still has: a multi-byte UTF-8 fill is
#     a malformed spec to the byte-oriented scanner and a valid one to
#     libc++.  Measured, and left visible.
#
# This lesson needs NO pad flags: the pad axis is orthogonal to the
# carried verdict, so the pad options are not reproduced here.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-12-try-format-carried-verdict
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-12-try-format-carried-verdict

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-12-try-format-carried-verdict
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# Optional strict-warning mode toggle.
option(ENABLE_STRICT_WARNINGS
       "Enable -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
       OFF)
if(ENABLE_STRICT_WARNINGS)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wconversion -Wsign-conversion)
endif()

# Optional ASan/UBSan mode toggle.
option(ENABLE_ASAN
       "Enable -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer"
       OFF)
if(ENABLE_ASAN)
    add_compile_options(
        -fsanitize=address -fsanitize=undefined
        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

add_executable(P-2026-11-12-try-format-carried-verdict
               P-2026-11-12-try-format-carried-verdict.cpp)
