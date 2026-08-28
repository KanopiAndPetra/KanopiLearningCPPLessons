# CMakeLists.txt — today's P-2026-08-28 lesson:
#
#   std::ostream_print — a consumer-side adapter that closes the
#   std::print(std::ostream&, ...) gap left by libc++ 21, plus
#   the bounded std::expected<size_t, std::format_error> sibling.
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23)
#   - Aug 24   std::format_to_n in std::expected (C++23)
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded formatter
#   - Aug 26   std::format_to_n_runtime_string — runtime-format-string wrapper
#   - Aug 27   std::format_to_n_runtime_string_projected — LogError projection
#   - TODAY    std::ostream_print (C++23) — std::print(ostream&, ...)
#              adapter that closes the missing-overload gap noted in
#              both Aug 22 and Aug 27's "Where we go next" sections.
#              Today's adapter goes through std::vformat /
#              std::vformat_to (P2216R3) and uses either a
#              vformat -> std::string -> os.write (zero-
#              cleverness) path or an ostreambuf_iterator ->
#              rdbuf() (zero-allocation) path. Plus a bounded
#              sibling in the same expected<size_t, format_error>
#              shape the Aug 24/25/26 chain established.
#
# What today covers
# -----------------
#   - petra::ostream_print(ostream&, fmt, args...) — consteval-checked
#     (literal fmt); goes through std::vformat -> os.write.
#   - petra::ostream_println(ostream&, fmt, args...) — same shape, '\n'
#     appended.
#   - petra::ostream_print_runtime(ostream&, fmt, format_args) —
#     runtime-format-string entry point; required because
#     std::format_string<Args...> ctor is consteval and rejects
#     a runtime std::string_view.
#   - petra::ostream_print_via_buf(ostream&, fmt, args...) and its
#     _runtime sibling — zero-allocation alternative that pipes
#     through std::ostreambuf_iterator<char>.
#   - petra::ostream_write_n(ostream&, cap, fmt, args...) — the
#     expected<size_t, std::format_error> bounded sibling with
#     the two-pass (measure, write) design.
#   - The consteval-vs-runtime split: a single std::format_string
#     ctor is consteval-evaluated, so a runtime fmt string and a
#     literal fmt string are reachable only through DIFFERENT
#     entry points. This is the same shape of separation
#     P2093R14 §5.2 describes for std::print.
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-28-ostream-print-cpp23
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-28-ostream-print-cpp23
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-28-ostream-print-cpp23
#
# NOTE on ASan output: the program writes progress lines for
# each section to stderr; ASan/UBSan also append a sanitizer
# summary at exit. The summary is empty when everything is
# clean — no leaks, no UB, no buffer overruns. Run the ASan
# build with detect_leaks unset (not supported on darwin/arm64).

cmake_minimum_required(VERSION 3.16)
project(ostream_print_cpp23_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of a std::print(std::ostream&, ...) adapter on Apple Clang 21.0.0 / libc++ — covers the missing P2093R14 ostream-overload gap via petra::ostream_print / ostream_println / ostream_print_runtime / ostream_print_via_buf / ostream_write_n; the consteval-vs-runtime split on std::format_string; the two-pass measure-then-write design for the bounded sibling; streambuf iterator paths; format_error propagation vs expected<size_t, format_error>; UTF-8 byte-by-byte transmission; NUL byte passthrough; std::ofstream sinks; seekp-after-write round-trips."
    LANGUAGES CXX
)

# C++23 for std::format / std::vformat_to (P2216R3) / std::expected (P0323R12).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-28-ostream-print-cpp23
    P-2026-08-28-ostream-print-cpp23.cpp
)

target_compile_options(P-2026-08-28-ostream-print-cpp23 PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-28-ostream-print-cpp23 PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-28-ostream-print-cpp23 PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
