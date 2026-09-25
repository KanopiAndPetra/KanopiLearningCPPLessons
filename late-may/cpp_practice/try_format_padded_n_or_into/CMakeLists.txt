# CMakeLists.txt — today's P-2026-09-25 lesson:
#
#   try_format_padded_n_or_into — a NEW FIFTH AXIS: the PAD-vs-UNTOUCHED
#   sibling-choice macro layer on the CHAR*+CAP × BOUNDED-BUFFER axis.
#   Two new meta-macros — TRY_FORMAT_BOUNDED_N_PAD (CONSTEVAL surface)
#   and TRY_FORMAT_BOUNDED_RUNTIME_N_PAD (RUNTIME surface) — each
#   dispatch to EITHER Sep 9's try_format_bounded_runtime_n (CONSTEVAL
#   surface, returns expected<size_t, FormatError>, leaves [written, cap)
#   UNTOUCHED) or Sep 8's try_format_runtime_n (RUNTIME surface,
#   returns expected<size_t, FormatError>, leaves [written, cap)
#   UNTOUCHED) when PETRA_PAD=0; OR today's NEW
#   petra::try_format_bounded_runtime_n_padded (CONSTEVAL surface,
#   zero-fills [written, cap) to '\0' on success) or today's NEW
#   petra::try_format_runtime_n_padded (RUNTIME surface, zero-fills
#   [written, cap) to '\0' on success) when PETRA_PAD=1.
#
# Closes Sep 23's "Where we go next" item (the FOURTH-AXIS "open item
# for a future axis-extension lesson") — this is the FIRST axis-
# extension after the 16-cell matrix was completed.  The matrix grows
# from 16 to 32 cells on the BOUNDED-BUFFER × expected quadrant on
# the char*+cap side.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter.
#   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
#              std::size_t cap (RUNTIME std::string-output wrapper).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
#              Sep 7 (char* + std::size_t cap + std::string_view
#              fmt, NO NUL).
#   - Sep  9   try_format_bounded_runtime_n — CONSTEVAL char*+cap
#              (NO NUL).
#   - Sep 10   try_format_bounded_runtime_n_span — CONSTEVAL
#              std::span<char> char*+cap (NO NUL).
#   - Sep 11   format_to_n_into_bounded — CONSTEVAL NUL-terminating
#              char*+cap (plus std::span<char> overload).
#   - Sep 12   try_format_runtime_n_into_bounded — RUNTIME
#              NUL-terminating char*+cap (plus std::span<char>
#              overload).
#   - Sep 13   try_format_bounded_macros — eight TRY_FORMAT_BOUNDED_*
#              macros (CONSTEVAL vs RUNTIME sibling choice at the
#              call site).
#   - Sep 14   try_format_span_format_string — std::span<const char>
#              overloads for the runtime try_format_* family + four
#              new macros.
#   - Sep 15   try_format_consteval_span_format_string — pins that
#              the CONSTEVAL surface is incompatible with
#              std::span<const char>; adds a CONSTEVAL-SHAPED
#              RUNTIME PARSER family of four wrappers + four macros.
#   - Sep 16   try_format_bounded_n_or_into — the BOUNDED-BUFFER vs
#              NUL-TERMINATING sibling-choice macro layer (char*+cap
#              axis).
#   - Sep 19   try_format_span_n_or_into — the BOUNDED-BUFFER vs
#              NUL-TERMINATING sibling-choice macro layer on the
#              std::span<char> axis.
#   - Sep 20   try_format_bounded_n_or_throw — the RETURN-TYPE-vs-
#              EXCEPTION sibling-choice macro layer on the char*+cap
#              axis (the FOURTH axis; covers the char*+cap ×
#              bounded-buffer × expected-vs-throwing cells).
#   - Sep 21   try_format_span_n_or_throw — the RETURN-TYPE-vs-
#              EXCEPTION sibling-choice macro layer on the
#              std::span<char> axis (the FOURTH axis on the
#              span<char> side; covers the span<char> × bounded-
#              buffer × expected-vs-throwing cells).
#   - Sep 22   try_format_bounded_n_or_throw_into — the NUL-
#              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
#              meta-macro layer on the CHAR*+CAP axis (covers the
#              char*+cap × NUL-terminating × expected-vs-throwing
#              cells — closes Sep 21's "Where we go next" item #1
#              verbatim).
#   - Sep 23   try_format_span_n_or_throw_into — the NUL-
#              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
#              meta-macro layer on the STD::SPAN<CHAR> axis (covers
#              the span<char> × NUL-terminating × expected-vs-
#              throwing cells — closes Sep 22's "Where we go next"
#              item #1 verbatim; the FOUR-AXIS BOUNDED-vs-NUL ×
#              expected-vs-throwing quadrant of the 16-cell matrix
#              is now COMPLETE on BOTH the char*+cap and span<char>
#              axes).
#   - TODAY    try_format_padded_n_or_into — the FIFTH axis: the
#              PAD-vs-UNTOUCHED sibling-choice macro layer on the
#              CHAR*+CAP × BOUNDED-BUFFER axis (covers 2 NEW cells
#              on the char*+cap × CONSTEVAL-or-RUNTIME × bounded-
#              buffer × expected × padded axis; the matrix grows
#              from 16 to 32 cells on the BOUNDED-BUFFER × expected
#              quadrant on the char*+cap side).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
#     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
#     Sep 23 verbatim (the wrappers below do not produce FormatError
#     directly — they only forward to existing wrappers, but the type
#     must be visible in scope for std::expected<size_t, FormatError>
#     to be instantiatable by the static_assert below).
#   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
#     TooLarge} — REUSED verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
#     petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim)
#     — REUSED verbatim (the bounded-buffer underlying impls).
#   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
#     args...) — REUSED Sep 8 verbatim (the RUNTIME untouched sibling
#     that today's NEW padded wrapper composes on top of).
#   - petra::try_format_bounded_runtime_n(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim
#     (the CONSTEVAL untouched sibling that today's NEW padded
#     wrapper composes on top of).
#   - petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
#     std::format_string<Args...>, args...) — NEW today (CONSTEVAL
#     surface of the BOUNDED-BUFFER × PAD sibling on the char*+cap
#     axis; wraps Sep 9's try_format_bounded_runtime_n verbatim and
#     zero-fills [written, cap) to '\0' on success).
#   - petra::try_format_runtime_n_padded(char*, std::size_t,
#     std::string_view, args...) — NEW today (RUNTIME surface of
#     the BOUNDED-BUFFER × PAD sibling on the char*+cap axis; wraps
#     Sep 8's try_format_runtime_n verbatim and zero-fills
#     [written, cap) to '\0' on success).
#   - The TWO new meta-macros TRY_FORMAT_BOUNDED_N_PAD (CONSTEVAL
#     surface) and TRY_FORMAT_BOUNDED_RUNTIME_N_PAD (RUNTIME surface).
#     Each is a verbatim thin dispatcher that picks between the
#     untouched sibling (PETRA_PAD=0) and the NUL-padded sibling
#     (PETRA_PAD=1) at preprocessor time.  Default PETRA_PAD=0
#     (untouched sibling — the prior contract across Sep 6 through
#     Sep 23).
#
# Build (default — -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-25-try-format-padded-n-or-into
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-25-try-format-padded-n-or-into
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-25-try-format-padded-n-or-into
#
# NUL-padded path (the OTHER branch of PETRA_PAD):
#
#   cmake -S . -B build-pad -DPETRA_PAD=ON
#   cmake --build build-pad
#   ./build-pad/P-2026-09-25-try-format-padded-n-or-into
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-25-try-format-padded-n-or-into.cpp \
#           -o /tmp/P-2026-09-25-try-format-padded-n-or-into
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#   - NUL-padded path build: default + -DPETRA_PAD=1
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::atomic + std::is_same_v +
# __VA_OPT__ preprocessor feature.  Pure consumer-side; no third-party
# libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-25-try-format-padded-n-or-into
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# PETRA_PAD controls the dispatch between untouched sibling
# (returns expected<size_t, FormatError>, leaves [written, cap)
# UNTOUCHED on success) and NUL-padded sibling (zero-fills
# [written, cap) to '\0' on success).  Default = 0 (untouched
# sibling — matches Sep 6 through Sep 23's contract).
option(PETRA_PAD
       "0 = untouched sibling; 1 = NUL-padded sibling"
       OFF)
if(PETRA_PAD)
    add_compile_options(-DPETRA_PAD=1)
else()
    add_compile_options(-DPETRA_PAD=0)
endif()

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

add_executable(P-2026-09-25-try-format-padded-n-or-into
               P-2026-09-25-try-format-padded-n-or-into.cpp)
