# CMakeLists.txt — today's P-2026-10-05 lesson:
#
#   try_format_padded_dash_pad_span_n_or_throw — the PETRA_PAD = 1 ×
#   PETRA_DASH_PAD = 1 cells on the STD::SPAN<CHAR> ×
#   BOUNDED-BUFFER axis (the STD::SPAN<CHAR> MIRROR of Sep 30's
#   PAD × SPACE_PAD composition lesson, but for the DASH axis).
#   TODAY extends the SEVENTH axis (PETRA_DASH_PAD) to the
#   NUL-padded sibling (FIFTH axis) on the std::span<char> side.
#
# Closes Oct 4's "Where we go next" item (1) verbatim — the
# (PETRA_PAD=1, PETRA_DASH_PAD=1) composition cells on the
# std::span<char> axis.  The matrix on the BOUNDED-BUFFER ×
# (expected-or-throwing) × (untouched-vs-NUL-vs-SPACE-vs-DASH)
# quadrant on the std::span<char> side is now COMPLETE for ALL 16
# cells of the 4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
# PETRA_SPACE_PAD, PETRA_DASH_PAD).
#
# KEY DESIGN DECISION — the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0,
# 1), and (1, 1, 1, 1) cells REUSE the (0, 0, 0, 1) and (1, 0, 0,
# 1) DASH-padded span<char> sibling wrappers verbatim because
# DASH overwrites NUL when both flags are set (and transitively
# DASH overwrites SPACE overwrites NUL when all three are set).
# No NEW wrappers are needed for these composition cells; they
# just dispatch to Oct 4's DASH-padded siblings.  The (0, 1, 1,
# 0) and (1, 1, 1, 0) cells REUSE Sep 28's (0, 0, 1, 0) and
# (1, 0, 1, 0) SPACE-padded span<char> sibling wrappers verbatim
# (Oct 1's "SPACE overwrites NUL" composition rule).
#
# Where this fits in the arc
# --------------------------
#   - Sep 27   try_format_padded_span_n_or_throw — the STD::SPAN<CHAR>
#              MIRROR of Sep 26's (THROWING × PADDED) meta-macro layer.
#   - Sep 28   try_format_space_pad_span_n — the SIXTH axis
#              (PETRA_SPACE_PAD) sibling-choice macro layer on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - Oct  1   try_format_padded_space_pad_span_n_or_throw — the
#              COMPOSITION of axis 5 and axis 6 on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis (the SPAN<CHAR>
#              mirror of Sep 30's char*+cap PAD × SPACE_PAD lesson).
#   - Oct  4   try_format_dash_pad_span_n — the SEVENTH axis
#              (PETRA_DASH_PAD) sibling-choice macro layer on the
#              STD::SPAN<CHAR> × BOUNDED-BUFFER axis.
#   - TODAY    try_format_padded_dash_pad_span_n_or_throw — the
#              STD::SPAN<CHAR> MIRROR of Oct 4's lesson extended
#              to the PETRA_PAD=1 half.  Closes Oct 4's "Where we
#              go next" item (1) verbatim.  The matrix on the
#              BOUNDED-BUFFER × (expected-or-throwing) ×
#              (untouched-vs-NUL-vs-SPACE-vs-DASH) quadrant on
#              the std::span<char> side is now COMPLETE for ALL
#              16 cells of the 4-bit tuple.
#
# What today covers
# -----------------
#   - petra::FormatError{kind, message, needed_bytes} — REUSED verbatim.
#   - petra::FormatErrorKind — REUSED verbatim.
#   - petra::counting_output_iterator — REUSED verbatim.
#   - petra::detail::bounded_char_writer — REUSED verbatim.
#   - petra::detail::try_format_runtime_n_impl (Sep 8 verbatim)
#     + petra::detail::try_format_bounded_runtime_n_impl
#     (Sep 9 verbatim) — REUSED verbatim.
#   - petra::try_format_runtime_n(char*, std::size_t, std::string_view,
#     args...) — REUSED Sep 8 verbatim.
#   - petra::try_format_bounded_runtime_n(char*, std::size_t,
#     std::format_string<Args...>, args...) — REUSED Sep 9 verbatim.
#   - petra::try_format_runtime_n(std::span<char>, std::string_view,
#     args...) — REUSED Sep 19 verbatim.
#   - petra::try_format_bounded_runtime_n_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 10 verbatim.
#   - petra::try_format_runtime_n_throwing(std::span<char>,
#     std::string_view, args...) — REUSED Sep 21 verbatim.
#   - petra::try_format_bounded_runtime_n_throwing_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 21 verbatim.
#   - petra::try_format_bounded_runtime_n_padded_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 27 verbatim
#     (CONSTEVAL expected+NUL-padded span<char> wrapper).
#   - petra::try_format_runtime_n_padded_span(std::span<char>,
#     std::string_view, args...) — REUSED Sep 27 verbatim
#     (RUNTIME expected+NUL-padded span<char> wrapper).
#   - petra::try_format_bounded_runtime_n_throwing_padded_span(
#     std::span<char>, std::format_string<Args...>, args...) — REUSED
#     Sep 27 verbatim (CONSTEVAL throwing+NUL-padded span<char>
#     wrapper).
#   - petra::try_format_runtime_n_throwing_padded_span(std::span<char>,
#     std::string_view, args...) — REUSED Sep 27 verbatim (RUNTIME
#     throwing+NUL-padded span<char> wrapper).
#   - petra::try_format_bounded_runtime_n_space_pad_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Sep 28 verbatim
#     (CONSTEVAL expected+SPACE-padded span<char> wrapper; ALSO used
#     by the (0, 1, 1, 0) dispatch cell per Oct 1's composition rule).
#   - petra::try_format_runtime_n_space_pad_span(std::span<char>,
#     std::string_view, args...) — REUSED Sep 28 verbatim (RUNTIME
#     expected+SPACE-padded span<char> wrapper; ALSO used by the
#     (0, 1, 1, 0) dispatch cell per Oct 1's composition rule).
#   - petra::try_format_bounded_runtime_n_throwing_space_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) — REUSED
#     Sep 28 verbatim (CONSTEVAL throwing+SPACE-padded span<char>
#     wrapper; ALSO used by the (1, 1, 1, 0) dispatch cell per
#     Oct 1's composition rule).
#   - petra::try_format_runtime_n_throwing_space_pad_span(
#     std::span<char>, std::string_view, args...) — REUSED Sep 28
#     verbatim (RUNTIME throwing+SPACE-padded span<char> wrapper;
#     ALSO used by the (1, 1, 1, 0) dispatch cell per Oct 1's
#     composition rule).
#   - petra::try_format_bounded_runtime_n_dash_pad_span(std::span<char>,
#     std::format_string<Args...>, args...) — REUSED Oct 4 verbatim
#     (CONSTEVAL expected+DASH-padded span<char> wrapper; ALSO used
#     by the (0, 1, 0, 1), (0, 1, 1, 1), (0, 0, 1, 1) dispatch cells
#     per the composition rule).
#   - petra::try_format_runtime_n_dash_pad_span(std::span<char>,
#     std::string_view, args...) — REUSED Oct 4 verbatim (RUNTIME
#     expected+DASH-padded span<char> wrapper; ALSO used by the
#     (0, 1, 0, 1), (0, 1, 1, 1), (0, 0, 1, 1) dispatch cells per
#     the composition rule).
#   - petra::try_format_bounded_runtime_n_throwing_dash_pad_span(
#     std::span<char>, std::format_string<Args...>, args...) — REUSED
#     Oct 4 verbatim (CONSTEVAL throwing+DASH-padded span<char>
#     wrapper; ALSO used by the (1, 1, 0, 1), (1, 1, 1, 1), (1, 0, 1,
#     1) dispatch cells per the composition rule).
#   - petra::try_format_runtime_n_throwing_dash_pad_span(std::span<char>,
#     std::string_view, args...) — REUSED Oct 4 verbatim (RUNTIME
#     throwing+DASH-padded span<char> wrapper; ALSO used by the
#     (1, 1, 0, 1), (1, 1, 1, 1), (1, 0, 1, 1) dispatch cells per
#     the composition rule).
#   - The TWO meta-macros TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW
#     (CONSTEVAL surface) and TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW
#     (RUNTIME surface).  Each is a verbatim thin dispatcher that
#     picks between SIXTEEN sibling wrappers at preprocessor time
#     based on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
#     PETRA_SPACE_PAD, PETRA_DASH_PAD) 4-bit tuple.  The (0, 1, 0,
#     1), (0, 1, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1) cells REUSE
#     the (0, 0, 0, 1) and (1, 0, 0, 1) DASH-padded span<char>
#     sibling wrappers verbatim (the "DASH overwrites NUL"
#     composition rule, transitively "DASH overwrites SPACE
#     overwrites NUL").  The (0, 1, 1, 0) and (1, 1, 1, 0) cells
#     REUSE the (0, 0, 1, 0) and (1, 0, 1, 0) SPACE-padded
#     span<char> sibling wrappers verbatim (Oct 1's "SPACE
#     overwrites NUL" composition rule).
#
# Build (default — expected+untouched sibling; PETRA_PAD=0,
# PETRA_THROW=0, PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build-strict
#   ./build-strict/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
#
# Other flag cells (e.g. PETRA_PAD=1, PETRA_DASH_PAD=1):
#
#   cmake -S . -B build-padded-dash-pad \
#         -DPETRA_PAD=ON -DPETRA_DASH_PAD=ON
#   cmake --build build-padded-dash-pad
#   ./build-padded-dash-pad/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
#
# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section):
#
#   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
#           P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw.cpp \
#           -o /tmp/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
#
# Build matrix
#   - default build: clang++ -std=c++23 -stdlib=libc++
#     -fexperimental-library -O0
#   - strict-warning build: default + -Wall -Wextra -Wpedantic
#     -Werror -Wshadow -Wconversion -Wsign-conversion
#   - ASan/UBSan build: default + -fsanitize=address
#     -fsanitize=undefined -fno-omit-frame-pointer -O1
#   - flag cells: PETRA_THROW_ON_OVERFLOW × PETRA_PAD ×
#     PETRA_SPACE_PAD × PETRA_DASH_PAD → 16 cells (2 × 2 × 2 × 2).
#
# C++23 std::format + std::vformat_to + std::format_string<Args...>
# (P2216R3) + std::make_format_args + std::expected (P0323R12) +
# std::println (P2093R14) + std::atomic + std::is_same_v +
# std::format_error + std::runtime_error + dynamic_cast +
# std::is_base_of_v + std::span<char> (P0122R7) +
# std::ranges::contiguous_range + std::ranges::sized_range +
# std::is_trivially_copyable_v + std::is_standard_layout_v +
# __VA_OPT__ preprocessor feature.  Pure consumer-side; no
# third-party libraries.

cmake_minimum_required(VERSION 3.20)
project(P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
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

# PETRA_PAD controls the dispatch between untouched sibling (bytes
# [written, span.size()) UNTOUCHED on success — the prior contract) and
# NUL-padded sibling (bytes [written, span.size()) zero-filled to '\0' on
# success).  When both PETRA_PAD and PETRA_SPACE_PAD are set,
# the SPACE fill OVERWRITES the NUL fill (composition rule, Oct 1).
# When both PETRA_PAD and PETRA_DASH_PAD are set, the DASH fill
# OVERWRITES the NUL fill (composition rule, today).
option(PETRA_PAD
       "0 = untouched sibling; 1 = NUL-padded sibling"
       OFF)
if(PETRA_PAD)
    add_compile_options(-DPETRA_PAD=1)
else()
    add_compile_options(-DPETRA_PAD=0)
endif()

# PETRA_SPACE_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior contract)
# and SPACE-padded sibling (bytes [written, span.size()) filled with ' '
# on success).  When both PETRA_PAD and PETRA_SPACE_PAD are set,
# the SPACE fill OVERWRITES the NUL fill (composition rule, Oct 1).
option(PETRA_SPACE_PAD
       "0 = no space-pad; 1 = SPACE-padded sibling"
       OFF)
if(PETRA_SPACE_PAD)
    add_compile_options(-DPETRA_SPACE_PAD=1)
else()
    add_compile_options(-DPETRA_SPACE_PAD=0)
endif()

# PETRA_DASH_PAD controls the dispatch between untouched sibling
# (bytes [written, span.size()) UNTOUCHED on success — the prior contract)
# and DASH-padded sibling (bytes [written, span.size()) filled with '-'
# on success).  When both PETRA_SPACE_PAD and PETRA_DASH_PAD are set,
# the DASH fill OVERWRITES the SPACE fill (composition rule, Oct 4).
# When both PETRA_PAD and PETRA_DASH_PAD are set, the DASH fill
# OVERWRITES the NUL fill (composition rule, today).  When all three
# are set, the DASH fill OVERWRITES the SPACE fill OVERWRITES the
# NUL fill (transitive composition rule, today).
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

add_executable(P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
               P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw.cpp)
