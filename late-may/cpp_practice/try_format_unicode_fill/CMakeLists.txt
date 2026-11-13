# CMakeLists.txt — today's P-2026-11-13 lesson:
#
#   try_format_unicode_fill — the fill is a Unicode scalar value, so the
#   scanner DECODES it instead of reading one byte, and the two
#   implementations disagree about which ill-formed fills are FATAL, so
#   that difference is a mask in the policy table.
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
#   - Nov 12           the CARRIED VERDICT — the scanner's silence is
#                      carried into the fallback as silence, and the
#                      numeric ceiling joins the policy table.
#   - TODAY            the UNICODE FILL RULE — Nov 12's next-step item
#                      (1), closed: the fill is decoded as a scalar
#                      value, the ill-formed fill becomes a structural
#                      failure, and the per-implementation difference
#                      joins the policy table as a mask.
#
# What today measures (see the .md for the detail):
#
#   - The arc's byte-oriented fill rule disagreed with the linked stdlib
#     on 8 cases under libc++ 21 (4 false positives, 4 false
#     negatives) and on 6 cases under libstdc++ 15 (6 false positives).
#     The scalar-value rule disagrees on NONE of them, under either.
#   - The two implementations disagree about which ill-formed fills
#     are fatal: libc++ 21 rejects every one of them; libstdc++ 15
#     rejects the overlong / surrogate / above-U+10FFFF ones and
#     recovers from the rest by substituting U+FFFD.  So the policy
#     table gains a MASK, not a bit.
#   - The WIDTH counts CODE POINTS, not bytes: a width of 10 with a
#     four-byte fill is 31 bytes.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-13-unicode-fill-rule
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-13-unicode-fill-rule

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-13-unicode-fill-rule
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

add_executable(P-2026-11-13-unicode-fill-rule
               P-2026-11-13-unicode-fill-rule.cpp)
