# CMakeLists.txt — today's P-2026-09-26 lesson:
#
#   try_format_padded_n_or_throw — a 2-AXIS (× 2-SIBLING) META-MACRO
#   layer that lifts BOTH the RETURN-TYPE-vs-EXCEPTION sibling
#   choice AND the PAD-VS-UNTOUCHED sibling choice to a meta-macro
#   form on the CHAR*+CAP × BOUNDED-BUFFER axis.  Two new meta-macros
#   — TRY_FORMAT_BOUNDED_N_PAD_OR_THROW (CONSTEVAL surface) and
#   TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW (RUNTIME surface) —
#   each dispatch to ONE OF FOUR sibling wrappers at preprocessor
#   time based on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) 2-bit
#   pair.
#
# Closes Sep 25's "Where we go next" item #1 verbatim — the
# (THROWING × PADDED) cells.  The 2^5 = 32-cell matrix on the
# BOUNDED-BUFFER × (expected-or-throwing) quadrant on the char*+cap
# side is now COMPLETE.
#
# Where this fits in the arc
# --------------------------
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded
#              formatter WITHOUT a cap parameter.
#   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
#              std::size_t cap (RUNTIME std::string-output wrapper).
#   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
#   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER sibling of
#              Sep 7 (char* + std::size_t cap + std::string_view fmt,
#              NO NUL).
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
#              std::span<char> axis.
#   - Sep 22   try_format_bounded_n_or_throw_into — the NUL-
#              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
#              meta-macro layer on the CHAR*+CAP axis.
#   - Sep 23   try_format_span_n_or_throw_into — the NUL-
#              TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
#              meta-macro layer on the STD::SPAN<CHAR> axis.
#   - Sep 25   try_format_padded_n_or_into — the FIFTH axis: the
#              PAD-VS-UNTOUCHED sibling-choice macro layer on the
#              CHAR*+CAP × BOUNDED-BUFFER axis (covers 2 NEW cells
#              on the char*+cap × CONSTEVAL-or-RUNTIME × bounded-
#              buffer × expected × padded axis; the matrix grows
#              from 16 to 32 cells on the BOUNDED-BUFFER × expected
#              quadrant on the char*+cap side).
#   - TODAY    try_format_padded_n_or_throw — the FIFTH axis
#              (PAD-VS-UNTOUCHED) × FOURTH axis (EXPECTED-VS-
#              THROWING) COMBINED META-MACRO layer on the CHAR*+CAP
#              × BOUNDED-BUFFER axis (closes Sep 25's "Where we go
#              next" item #1 verbatim — covers 4 NEW cells on the
#              char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer ×
#              throwing × untouched-or-padded axis; the matrix
#              grows from 32 to 64 cells on the BOUNDED-BUFFER
#              quadrant on the char*+cap side).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED Sep 6 /
#     Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
#     Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
#     Sep 23 / Sep 25 verbatim (the wrappers below do not produce
#     FormatError directly — they only forward to existing wrappers,
#     but the type must be visible in scope for std::expected<size_t,
#     FormatError> to be instantiatable by the static_assert below).
#   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
#     TooLarge} — REUSED verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
#     petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim)
#     — REUSED verbatim (the bounded-buffer underlying impls).
#   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
#     args...) — REUSED Sep 8 verbatim.
#   - petra::try_format_bounded_runtime_n(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim.
#   - petra::try_format_runtime_n_padded(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 25 verbatim.
#   - petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 25 verbatim.
#   - petra::try_format_runtime_n_throwing(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 20 verbatim.
#   - petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 20 verbatim.
#   - petra::try_format_runtime_n_padded_throwing(char*, std::size_t,
#     std::string_view, args...) — NEW today (RUNTIME surface of the
#     BOUNDED-BUFFER × THROWING × PADDED sibling on the char*+cap
#     axis; wraps Sep 20's RUNTIME throwing wrapper verbatim and
#     zero-fills [written, cap) to '\0' on success AFTER the
#     underlying call completes).
#   - petra::try_format_bounded_runtime_n_padded_throwing(char*,
#     std::size_t, std::format_string<Args...>, args...) — NEW today
#     (CONSTEVAL surface of the BOUNDED-BUFFER × THROWING × PADDED
#     sibling on the char*+cap axis; wraps Sep 20's CONSTEVAL
#     throwing wrapper verbatim and zero-fills [written, cap) to '\0'
#     on success AFTER the underlying call completes).
#   - The TWO new meta-macros TRY_FORMAT_BOUNDED_N_PAD_OR_THROW
#     (CONSTEVAL surface) and TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW
#     (RUNTIME surface).  Each is a verbatim thin dispatcher that
#     picks between FOUR sibling wrappers at preprocessor time based
#     on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD) 2-bit pair.  Each
#     macro is a parenthesized expression (NOT do { } while (0)) so
#     the caller can write 'auto n =
#     TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(...)' and capture the result.
#     Each macro uses __VA_OPT__(,) for the no-args case (mirrors Sep
#     13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep
#     22 / Sep 23 / Sep 25's macro shape).
#
# Build (default — expected+untouched sibling; both flags 0):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-09-26-try-format-padded-n-or-throw
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-09-26-try-format-padded-n-or-throw
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-09-26-try-format-padded-n-or-throw
#
# Throwing path (PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0):
#
#   cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
#   cmake --build build-throw
#   ./build-throw/P-2026-09-26-try-format-padded-n-or-throw
#
# Padded path (PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1):
#
#   cmake -S . -B build-pad -DPETRA_PAD=ON
#   cmake --build build-pad
#   ./build-pad/P-2026-09-26-try-format-padded-n-or-throw
#
# Throwing+Padded path (PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1):
#
#   cmake -S . -B build-throw-pad -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PAD=ON
#   cmake --build build-throw-pad
#   ./build-throw-pad/P-2026-09-26-try-format-padded-n-or-throw
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-09-26-try-format-padded-n-or-throw.cpp \
#           -o /tmp/P-2026-09-26-try-format-padded-n-or-throw
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic -Werror
#     -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address -fsanitize=undefined
#     -fno-omit-frame-pointer -O1
#   - throwing path build: default + -DPETRA_THROW_ON_OVERFLOW=1
#   - padded path build: default + -DPETRA_PAD=1
#   - throwing+padded path build: default + -DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_PAD=1
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::atomic + std::is_same_v +
# std::format_error + std::runtime_error + dynamic_cast +
# std::is_base_of_v + __VA_OPT__ preprocessor feature.  Pure
# consumer-side; no third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-09-26-try-format-padded-n-or-throw
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and a few other C++23 features.  Add it unconditionally.
add_compile_options(-fexperimental-library)
add_link_options(-fexperimental-library)

# PETRA_THROW_ON_OVERFLOW controls the dispatch between expected sibling
# (returns expected<size_t, FormatError>) and throwing sibling
# (returns std::size_t, throws std::runtime_error on overflow).
# Default = 0 (expected — matches Sep 6 through Sep 25's contract).
option(PETRA_THROW_ON_OVERFLOW
       "0 = expected sibling; 1 = throwing sibling"
       OFF)
if(PETRA_THROW_ON_OVERFLOW)
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=1)
else()
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=0)
endif()

# PETRA_PAD controls the dispatch between untouched sibling
# (bytes [written, cap) UNTOUCHED on success) and NUL-padded sibling
# (bytes [written, cap) zero-filled to '\0' on success).  Default = 0
# (untouched — matches Sep 6 through Sep 25's contract).
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

add_executable(P-2026-09-26-try-format-padded-n-or-throw
               P-2026-09-26-try-format-padded-n-or-throw.cpp)