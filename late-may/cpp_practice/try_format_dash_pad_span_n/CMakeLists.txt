# CMakeLists.txt — today's P-2026-10-04 lesson:
#
#   try_format_dash_pad_span_n — the SEVENTH axis
#   (PETRA_DASH_PAD) sibling-choice META-MACRO layer on the
#   STD::SPAN<CHAR> × BOUNDED-BUFFER axis.  TODAY introduces a
#   NEW FOURTH compile-time flag (PETRA_DASH_PAD) on top of
#   Oct 1's 3-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#   PETRA_SPACE_PAD) — the STD::SPAN<CHAR> MIRROR of Oct 3's
#   char*+cap lesson.  PETRA_DASH_PAD=1 selects the DASH-padded
#   span<char> sibling, which fills bytes [written,
#   span.size()) with '-' on success.
#
# Closes Oct 3's "Where we go next" item (1) verbatim — the
# (PETRA_DASH_PAD=1) sibling-choice macro layer on the
# std::span<char> side.  The matrix on the BOUNDED-BUFFER ×
# (expected-or-throwing) × (untouched-vs-SPACE-vs-DASH)
# quadrant on the std::span<char> side is now COMPLETE for all
# 8 cells of the 3-bit tuple (PETRA_THROW_ON_OVERFLOW,
# PETRA_SPACE_PAD, PETRA_DASH_PAD) (PETRA_PAD=0 only; the
# PETRA_PAD=1 cells live in Sep 27's / Oct 1's lessons and are
# a #error in this lesson).
#
# KEY DESIGN DECISION — the (0,0,0,1), (0,0,1,1), (1,0,0,1),
# and (1,0,1,1) cells REUSE the SAME DASH-pad span<char>
# sibling wrappers (the (0,0,1,1) and (1,0,1,1) cells share
# wrappers with the (0,0,0,1) and (1,0,0,1) cells because
# DASH overwrites SPACE when both are set — the composition
# rule "the LATER fill overwrites the EARLIER fill").  No NEW
# wrappers are needed for the (0,0,1,1) and (1,0,1,1) cells;
# they just dispatch to the (0,0,0,1) and (1,0,0,1) DASH-pad
# span<char> wrappers.
#
# Where this fits in the arc
# --------------------------
#   - Sep 28   try_format_space_pad_span_n — the SIXTH axis
#              (PETRA_SPACE_PAD) sibling-choice macro layer on
#              the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Oct 1    try_format_padded_space_pad_span_n_or_throw —
#              the STD::SPAN<CHAR> MIRROR of Sep 30 (the
#              COMPOSITION of axis 5 and axis 6 on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis).
#   - Oct 3    try_format_dash_pad_n — the SEVENTH axis
#              (PETRA_DASH_PAD) sibling-choice macro layer on
#              the CHAR*+CAP × BOUNDED-BUFFER axis.
#   - TODAY    try_format_dash_pad_span_n — the STD::SPAN<CHAR>
#              MIRROR of Oct 3 (the SEVENTH axis on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis).  Closes
#              Oct 3's "Where we go next" item (1) verbatim.
#              KEY FINDING: the (DASH_PAD=1, SPACE_PAD=1) cells
#              REUSE the (DASH_PAD=1, SPACE_PAD=0) DASH-pad
#              span<char> sibling wrappers verbatim because
#              DASH overwrites SPACE when both are set (the
#              composition rule).
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED verbatim.
#   - petra::FormatErrorKind — REUSED verbatim.
#   - petra::counting_output_iterator — REUSED verbatim (with
#     the Sep 30 / Oct 1 / Oct 3 callback-based design).
#   - petra::detail::bounded_char_writer — REUSED verbatim
#     (with the Sep 30 / Oct 1 / Oct 3 callback-based design,
#     where `written_` is incremented ONLY on successful
#     writes).
#   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim)
#     + petra::detail::try_format_bounded_runtime_n_impl
#     (Sep 9 verbatim) — REUSED verbatim with the proven
#     callback-based pattern.
#   - petra::try_format_runtime_n(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 8 verbatim.
#   - petra::try_format_bounded_runtime_n(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 9
#     verbatim.
#   - petra::try_format_runtime_n(std::span<char>,
#     std::string_view, args...) — REUSED Sep 19 verbatim.
#   - petra::try_format_bounded_runtime_n_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 10
#     verbatim.
#   - petra::try_format_runtime_n_throwing(char*, std::size_t,
#     std::string_view, args...) — REUSED Sep 20 verbatim.
#   - petra::try_format_bounded_runtime_n_throwing(char*,
#     std::size_t, std::format_string<Args...>, args...) —
#     REUSED Sep 20 verbatim.
#   - petra::try_format_runtime_n_throwing(std::span<char>,
#     std::string_view, args...) — REUSED Sep 21 verbatim.
#   - petra::try_format_bounded_runtime_n_throwing_span(
#     std::span<char>, std::format_string<Args...>, args...) —
#     REUSED Sep 21 verbatim.
#   - petra::try_format_bounded_runtime_n_space_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) —
#     REUSED Sep 28 verbatim (CONSTEVAL expected+SPACE-padded
#     span<char> sibling wrapper).
#   - petra::try_format_runtime_n_space_pad_span(std::span<char>,
#     std::string_view, args...) — REUSED Sep 28 verbatim
#     (RUNTIME expected+SPACE-padded span<char> sibling
#     wrapper).
#   - petra::try_format_bounded_runtime_n_throwing_space_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) —
#     REUSED Sep 28 verbatim (CONSTEVAL throwing+SPACE-padded
#     span<char> sibling wrapper).
#   - petra::try_format_runtime_n_throwing_space_pad_span(
#     std::span<char>, std::string_view, args...) — REUSED Sep
#     28 verbatim (RUNTIME throwing+SPACE-padded span<char>
#     sibling wrapper).
#   - petra::try_format_bounded_runtime_n_dash_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) —
#     NEW today (CONSTEVAL expected+DASH-padded span<char>
#     sibling wrapper; wraps Sep 28's CONSTEVAL
#     expected+SPACE-padded span<char> sibling and overwrites
#     [written, span.size()) with '-' on success ONLY; also
#     used by the (0,0,1,1) dispatch cell per the composition
#     rule).
#   - petra::try_format_runtime_n_dash_pad_span(std::span<char>,
#     std::string_view, args...) — NEW today (RUNTIME
#     expected+DASH-padded span<char> sibling wrapper; wraps
#     Sep 28's RUNTIME expected+SPACE-padded span<char> sibling
#     and overwrites [written, span.size()) with '-' on success
#     ONLY; also used by the (0,0,1,1) dispatch cell per the
#     composition rule).
#   - petra::try_format_bounded_runtime_n_throwing_dash_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) —
#     NEW today (CONSTEVAL throwing+DASH-padded span<char>
#     sibling wrapper; wraps Sep 28's CONSTEVAL
#     throwing+SPACE-padded span<char> sibling and overwrites
#     [written, span.size()) with '-' on success ONLY;
#     re-raises std::runtime_error untouched on error; also
#     used by the (1,0,1,1) dispatch cell per the composition
#     rule).
#   - petra::try_format_runtime_n_throwing_dash_pad_span(
#     std::span<char>, std::string_view, args...) — NEW today
#     (RUNTIME throwing+DASH-padded span<char> sibling
#     wrapper; wraps Sep 28's RUNTIME throwing+SPACE-padded
#     span<char> sibling and overwrites [written, span.size())
#     with '-' on success ONLY; re-raises std::runtime_error
#     untouched on error; also used by the (1,0,1,1) dispatch
#     cell per the composition rule).
#   - The TWO meta-macros TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW
#     (CONSTEVAL surface) and
#     TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW (RUNTIME
#     surface).  Each is a verbatim thin dispatcher that picks
#     between EIGHT sibling wrappers at preprocessor time based
#     on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#     PETRA_SPACE_PAD, PETRA_DASH_PAD) 4-bit tuple.  PETRA_PAD
#     MUST be 0 in this lesson (the dispatcher #errors with a
#     clear pointer to Sep 27 / Oct 1 when PETRA_PAD=1, since
#     Sep 27's / Oct 1's NUL-padded span<char> wrappers are out
#     of scope for this lesson).  The (0,0,1,1) and (1,0,1,1)
#     cells REUSE the (0,0,0,1) and (1,0,0,1) DASH-padded
#     span<char> sibling wrappers verbatim (the "DASH
#     overwrites SPACE" composition rule).
#
# Build (default — expected+untouched sibling; PETRA_THROW=0,
# PETRA_PAD=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-10-04-try-format-dash-pad-span-n
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-10-04-try-format-dash-pad-span-n
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-10-04-try-format-dash-pad-span-n
#
# DASH-pad cells (PETRA_DASH_PAD=1):
#
#   cmake -S . -B build-dash-pad -DPETRA_DASH_PAD=ON
#   cmake --build build-dash-pad
#   ./build-dash-pad/P-2026-10-04-try-format-dash-pad-span-n
#
# SPACE+DASH composition cells (PETRA_SPACE_PAD=1,
# PETRA_DASH_PAD=1):
#
#   cmake -S . -B build-space-dash \
#         -DPETRA_SPACE_PAD=ON -DPETRA_DASH_PAD=ON
#   cmake --build build-space-dash
#   ./build-space-dash/P-2026-10-04-try-format-dash-pad-span-n
#
# Throwing DASH-pad cells (PETRA_THROW_ON_OVERFLOW=1,
# PETRA_DASH_PAD=1):
#
#   cmake -S . -B build-throw-dash \
#         -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_DASH_PAD=ON
#   cmake --build build-throw-dash
#   ./build-throw-dash/P-2026-10-04-try-format-dash-pad-span-n
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-10-04-try-format-dash-pad-span-n.cpp \
#           -o /tmp/P-2026-10-04-try-format-dash-pad-span-n
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic
#     -Werror -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address
#     -fsanitize=undefined -fno-omit-frame-pointer -O1
#   - flag cells: PETRA_THROW_ON_OVERFLOW × PETRA_PAD ×
#     PETRA_SPACE_PAD × PETRA_DASH_PAD → 16 cells (2 × 2 × 2
#     × 2); PETRA_PAD=1 is a #error (out of scope for this
#     lesson).
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::atomic + std::is_same_v +
# std::format_error + std::runtime_error + dynamic_cast +
# std::is_base_of_v + std::array<std::thread, 4> +
# std::thread::join() + std::span<char> (P0122R7) +
# std::ranges::contiguous_range + std::ranges::sized_range +
# std::is_trivially_copyable_v + std::is_standard_layout_v +
# __VA_OPT__ preprocessor feature.  Pure consumer-side; no
# third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-04-try-format-dash-pad-span-n
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
option(PETRA_THROW_ON_OVERFLOW
       "0 = expected sibling; 1 = throwing sibling"
       OFF)
if(PETRA_THROW_ON_OVERFLOW)
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=1)
else()
    add_compile_options(-DPETRA_THROW_ON_OVERFLOW=0)
endif()

# PETRA_PAD MUST be 0 for this lesson — Sep 27's / Oct 1's
# NUL-padded span<char> wrappers are out of scope.  The lesson
# code #errors if PETRA_PAD=1 anyway.  Exposed as a cache var so
# CMake will print a friendly error if the user accidentally
# sets it.
set(PETRA_PAD "0" CACHE STRING
    "PETRA_PAD must be 0 in this lesson (Sep 27 / Oct 1 cover PETRA_PAD=1)")
set_property(CACHE PETRA_PAD PROPERTY STRINGS "0" "1")
if(NOT PETRA_PAD STREQUAL "0")
    message(FATAL_ERROR
        "PETRA_PAD must be 0 in this lesson.  Sep 27's / Oct 1's "
        "NUL-padded span<char> wrappers are out of scope.")
endif()
add_compile_options(-DPETRA_PAD=0)

# PETRA_SPACE_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior
# contract) and SPACE-padded sibling (bytes [written, span.size())
# filled with ' ' on success).
option(PETRA_SPACE_PAD
       "0 = no space-pad; 1 = SPACE-padded sibling"
       OFF)
if(PETRA_SPACE_PAD)
    add_compile_options(-DPETRA_SPACE_PAD=1)
else()
    add_compile_options(-DPETRA_SPACE_PAD=0)
endif()

# PETRA_DASH_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior
# contract) and DASH-padded sibling (bytes [written, span.size())
# filled with '-' on success).  When both PETRA_SPACE_PAD and
# PETRA_DASH_PAD are set, the DASH fill OVERWRITES the SPACE fill
# (composition rule).
option(PETRA_DASH_PAD
       "0 = no dash-pad; 1 = DASH-padded sibling"
       OFF)
if(PETRA_DASH_PAD)
    add_compile_options(-DPETRA_DASH_PAD=1)
else()
    add_compile_options(-DPETRA_DASH_PAD=0)
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

add_executable(P-2026-10-04-try-format-dash-pad-span-n
               P-2026-10-04-try-format-dash-pad-span-n.cpp)
