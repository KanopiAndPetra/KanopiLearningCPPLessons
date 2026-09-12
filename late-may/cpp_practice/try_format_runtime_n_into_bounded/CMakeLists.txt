# CMakeLists.txt — today's P-2026-09-12 lesson:
#
#   try_format_runtime_n_into_bounded — std::expected<std::size_t,
#   FormatError> wrapper around std::vformat_to that takes a caller-provided
#   byte cap AND a runtime std::string_view fmt AND ALWAYS NUL-terminates
#   the destination on the success path.  Closes Sep 11's "Where we go
#   next" item #5 — the RUNTIME-FORMAT-STRING sibling of Sep 11's
#   format_to_n_into_bounded.  Sep 11's wrapper takes
#   std::format_string<Args...> whose ctor is consteval; today's wrapper
#   takes std::string_view and catches std::format_error from the
#   std::vformat_to / std::make_format_args side at runtime, returning
#   FormatError{Format, e.what()}.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter (always returns the
#              would-be size, writes min(would-be, N-1) chars + NUL).
#   - Sep  6   try_format_runtime_bounded — std::expected<std::string,
#              FormatError> wrapper for std::vformat_to with cap
#              (std::string output, no NUL-termination, runtime fmt).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of Sep 7
#              (char* buffer + std::string_view fmt).
#   - Sep  9   try_format_bounded_runtime_n — the CONSTEVAL
#              BOUNDED-BUFFER combination of Sep 7 + Sep 8.
#   - Sep 10   try_format_bounded_runtime_n_span — the STDSPAN sibling
#              of Sep 9.
#   - Sep 11   format_to_n_into_bounded — the NUL-TERMINATING sibling
#              of Sep 9 / Sep 10, with a cap parameter (Aug 25's
#              NUL-termination contract + Sep 9's cap parameter).
#   - TODAY    try_format_runtime_n_into_bounded — the
#              RUNTIME-FORMAT-STRING sibling of Sep 11 (today's
#              wrapper takes std::string_view instead of
#              std::format_string<Args...>).  Same three-branch
#              measure-then-write pair, same NUL placement formula,
#              same span overload shape, same structured error
#              contract.  Allows dynamic format strings (from config,
#              from user input, from a templated string) at the cost
#              of losing the consteval compile-time check.
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 verbatim.
#   - petra::counting_output_iterator — REUSED Sep 6 / Sep 7 / Sep 8 /
#     Sep 9 / Sep 10 / Sep 11 verbatim.
#   - petra::detail::bounded_char_writer — REUSED Sep 8 / Sep 9 /
#     Sep 10 / Sep 11 verbatim.
#   - petra::detail::try_format_runtime_n_into_bounded_impl — NEW
#     today; a two-pass measure-then-write pair that mirrors Sep 11's
#     structure but uses std::string_view instead of
#     std::format_string<Args...>.  Reuses the three-branch logic
#     verbatim (cap == 0 / needed > cap / needed == cap / needed < cap).
#   - petra::try_format_runtime_n_into_bounded<Args...>(char* out,
#     std::size_t cap, std::string_view, const Args&... args) — NEW
#     today, the runtime-format-string NUL-terminating bounded
#     primary surface.
#   - petra::try_format_runtime_n_into_bounded<Args...>(std::span<char>
#     out, std::string_view, const Args&... args) — NEW today, the
#     STDSPAN overload (mirrors Sep 10's pattern).
#   - Section 18 pins the runtime format-error contract: a malformed
#     format string that the consteval sibling (Sep 11) would catch at
#     compile time is caught at runtime by today's wrapper, returning
#     FormatError{Format, e.what()}.  Verified with two probes saved in
#     /tmp:
#     - /tmp/probe_consteval_rejects_runtime_unterminated.cpp — produces
#       a hard error 'call to consteval function std::basic_format_string<char,
#       int>::basic_format_string<char[8]> is not a constant expression'
#       with note 'non-constexpr function '__throw_format_error' cannot
#       be used in a constant expression' (same __throw_format_error
#       source line Sep 7 / Sep 9 / Sep 10 / Sep 11 documented).
#     - /tmp/probe_runtime_accepts_unterminated.cpp — COMPILES CLEANLY
#       and prints 'PROBE OK: runtime FormatError: The format string
#       terminates at a '{' ' at runtime, confirming today's runtime
#       sibling has a strictly weaker compile-time surface than Sep 11's
#       consteval sibling.
#   - Section 19 pins the dynamic-format-string use case: the KEY
#     difference between today's wrapper and Sep 11's wrapper is that
#     today's accepts std::string_view, which can be built dynamically
#     (from a config file, from user input, from a templated string).
#     Sep 11's std::format_string<Args...> requires a string literal at
#     the call site, which forbids dynamic format strings.  Section 19
#     exercises two dynamic-format-string cases:
#     - "v=" + std::to_string(42) — built from a concatenation.
#     - "hello " + "world" — built from a std::string concatenation.
#     Both compile cleanly (no consteval ctor) and produce the expected
#     byte count.
#   - Section 1: toolchain + feature probes
#   - Section 2: exact-fit NUL at written (cap=8, format "1234567")
#   - Section 3: no-truncation NUL at written (cap=16, format
#     "hello {}", arg "world" → "hello world", 11 bytes)
#   - Section 4: truncation edge NUL at cap-1 (cap=5, format "ABCDE"
#     needs 5 → r.value()=4, NUL at buf[4])
#   - Section 5: TooLarge buffer untouched (cap=4, format "1234567"
#     needs 7 → TooLarge{needed_bytes=7}, all 4 buf bytes stay 'Q')
#   - Section 6: cap=0 empty format (cap=0, format "" → success, no NUL)
#   - Section 7: cap=0 non-empty format (cap=0, format "x" needs 1
#     → TooLarge{needed_bytes=1}, buffer untouched)
#   - Section 8: cap=1 only NUL (cap=1, format "x" needs 1 → truncation
#     edge, r.value()=0, NUL at buf[0], formatted byte never written)
#   - Section 9: format specifiers passthrough (v={} h={:#x} pi={:.3f})
#   - Section 10: atomicity on TooLarge (pre-fill buf with "v=42", fail
#     call with cap=8, format "{}", arg "hello world" needs 11 →
#     TooLarge, pre-fill survives)
#   - Section 11: span overload equivalence (char*+cap and span<char>
#     overloads produce identical byte counts and content)
#   - Section 12: sub-span no-bleed (16-byte buffer, sub-span at
#     offset 5 covering [5, 11); format "ABCDE" → buf[5,10)=ABCDE,
#     buf[10]='\0', bytes [0,5) and [11,16) untouched)
#   - Section 13: multi-arg round-trip (name={} age={:#x} pi={:.5f})
#   - Section 14: UTF-8 mid-codepoint byte round-trip (é is 2 bytes
#     in UTF-8, 6 bytes total)
#   - Section 15: embedded NUL byte round-trip (std::string with
#     embedded NUL → 3 bytes, NUL at buf[3])
#   - Section 16: byte-count invariant (wrapper reports r.value()==11
#     AND independent counting probe reports measured==11)
#   - Section 17: TooLarge boundary sweep (cap=3 TooLarge, cap=4
#     truncation edge, cap=5 full success for "v=42" needing 4)
#   - Section 18: runtime format-error contract (malformed format
#     strings → FormatError{Format, e.what()} at runtime; buffer is
#     UNTOUCHED on Format error)
#   - Section 19: dynamic-format-string use case (built dynamically
#     via std::string concatenation)
#   - Section 20: compile-time contrast (today's wrapper accepts
#     std::string_view at the call site; the implicit conversion from
#     a std::string_view literal is what distinguishes today's surface
#     from Sep 11's consteval surface)
#   - Section 21: 4 threads x 25 calls (each thread's own char[32]
#     buffer; zero mismatches across 100 total calls)
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-12-try-format-runtime-n-into-bounded
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-12-try-format-runtime-n-into-bounded
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-12-try-format-runtime-n-into-bounded
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-12-try-format-runtime-n-into-bounded.cpp \
#           -o /tmp/P-2026-09-12-try-format-runtime-n-into-bounded
#
# Build matrix
# ------------
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#
# C++23 std::format + std::vformat_to + std::make_format_args +
# std::expected (P0323R12) + std::span<char> (P0122R7) +
# std::ranges::contiguous_range + std::println (P2093R14).  Pure
# consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-12-try-format-runtime-n-into-bounded
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
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
    add_link_options(
        -fsanitize=address -fsanitize=undefined)
endif()

add_executable(P-2026-09-12-try-format-runtime-n-into-bounded
               P-2026-09-12-try-format-runtime-n-into-bounded.cpp)