# CMakeLists.txt — today's P-2026-08-22 lesson:
#
#   std::print / std::println / std::format / std::vprint_unicode (C++23)
#   — type-safe, consteval-checked, terminal-flush, FILE*-based output
#
# Where this fits in the arc
# --------------------------
#   - Jul  9   std::span     (C++20) — 1-D non-owning view
#   - Jul 10   std::mdspan   (C++23) — N-D non-owning view
#   - Jul 12   std::expected (C++23) — sum-type error channel
#   - Aug 20   std::flat_map family (C++23) — DEFAULT std::vector storage
#   - Aug 21   std::flat_map family (C++23) — CUSTOM KeyContainer
#   - TODAY    std::print / std::println / std::format (C++23)
#              the third remaining item on the Aug 20 lesson's
#              "Natural follow-on lessons for the C++23 stdlib tour" list.
#
# Aug 20 explicitly listed three follow-on topics; Aug 21 closed the
# first ("std::flat_map with a custom KeyContainer"). Today's lesson
# closes the third — "std::print (C++23), __cpp_lib_print == 202207 is
# available in this toolchain, so this one is unblocked and ready."
# Today's commit is that follow-on lesson.
#
# What today covers
# -----------------
#   - The CONSTEVAL format-string check (P2216R3) — the difference
#     between std::print and printf, demonstrated with a malformed
#     format string that compiles cleanly in printf but errors at
#     compile time under std::print.
#   - Basic types, width/fill/alignment/sign/base/precision specifiers.
#   - Range formatters (P2286R6): vector, list, map (yes, map's
#     '{key: value}' syntax is automatic).
#   - std::chrono formatters (%F, %T, %H:%M:%S, %z, %Y-%m-%d).
#   - Locale-aware formatters: the 'L' specifier, and what fails
#     when std::locale{} is the default "C" locale.
#   - The runtime path: vprint_unicode / vprint_nonunicode /
#     make_format_args — and the lvalue requirement that libc++ 21
#     imposes.
#   - The FILE* contract: std::print writes to C streams, NOT to
#     std::ostream; std::print(std::stringstream{}, ...) fails to
#     compile in this libc++ 21. This is the most surprising part
#     of std::print's surface.
#   - Custom std::formatter specialization — the bridge from a
#     domain enum to std::print("{}").
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-22-std-print-cpp23
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-22-std-print-cpp23
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-22-std-print-cpp23
#
# NOTE on ASan output: the program writes a header, several
# timestamped lines, and a final TOTAL summary. ASan/UBSan also
# append a sanitizer summary at exit; this is expected and clean.
# Run the ASan build with detect_leaks unset (not supported on
# darwin/arm64).

cmake_minimum_required(VERSION 3.16)
project(std_print_cpp23_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of C++23 std::print / std::println / std::format / std::vprint_unicode / std::make_format_args / std::formatter on Apple Clang 21.0.0 / libc++ — covers the consteval-checked format string (P2216R3), the full width / fill / alignment / sign / base / precision surface, range formatters (P2286R6), std::chrono formatters, locale-aware formatters (L specifier), the runtime vprint_unicode / vprint_nonunicode path with the libc++ 21 lvalue requirement, the FILE* contract (not ostream), and custom std::formatter specialization bridging a domain enum to std::print."
    LANGUAGES CXX
)

# C++23 for std::print (P2093R14) / std::format (P2216R3) / std::chrono::formatter (P1361R2 / P0355R7).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-22-std-print-cpp23
    P-2026-08-22-std-print-cpp23.cpp
)

target_compile_options(P-2026-08-22-std-print-cpp23 PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-22-std-print-cpp23 PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-22-std-print-cpp23 PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
