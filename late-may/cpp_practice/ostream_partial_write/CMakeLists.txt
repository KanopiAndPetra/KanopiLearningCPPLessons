# CMakeLists.txt — today's P-2026-08-30 lesson:
#
#   ostream_write_checked — partial-write detection for the Aug 28
#   std::print(std::ostream&, ...) adapter. Returns
#   std::expected<size_t, petra::WriteError> with a PartialWrite
#   error kind carrying the requested/accepted byte split.
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23)
#   - Aug 24   std::format_to_n in std::expected (C++23)
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded formatter
#   - Aug 26   std::format_to_n_runtime_string — runtime-fmt wrapper
#   - Aug 27   format_to_n_runtime_string_projected — LogError projection
#   - Aug 28   std::ostream_print — the std::print(ostream&, ...) adapter
#              closing the libc++ 21 missing-overload gap
#   - TODAY    ostream_write_checked — closes the FIRST of the four new
#              follow-on items Aug 28's "Where we go next" listed: the
#              custom streambuf that fails to consume all chars. Aug 28
#              predicted its ostreambuf_iterator path "silently reports
#              end-of-stream and the adapter treats this as a successful
#              truncating write". Section 4 proves that prediction
#              correct; the rest of the lesson fixes it.
#
# What today covers
# -----------------
#   - petra::capped_streambuf — the non-consuming sink Aug 28 only
#     hypothesised. Refuses via overflow() -> eof() and via a short
#     xsputn() return, the two standard "downstream full" protocols.
#   - petra::counting_streambuf — a filter spliced between the ostream
#     and its real buffer that records offered vs. accepted bytes. This
#     is the durable mechanism: it is sink-agnostic and works for BOTH
#     adapter paths, neither of which reports a byte count natively.
#   - petra::scoped_counting_filter — RAII splice/unsplice that also
#     works around basic_ios::rdbuf(sb*) calling clear() (which would
#     otherwise silently launder both pre-existing and newly produced
#     stream error state).
#   - petra::WriteError{kind, message, requested, accepted} with kinds
#     Format and PartialWrite, plus a std::formatter specialisation
#     (libc++ 21 has no formatter for arbitrary user types).
#   - petra::ostream_write_checked / ostream_write_checked_via_buf and
#     their _runtime siblings — the consteval/runtime split Aug 28
#     established, forced by std::format_string's consteval ctor.
#
# Findings pinned by the sections
# -------------------------------
#   - The two Aug 28 adapter paths fail DIFFERENTLY: os.write sets
#     badbit on a short write; the ostreambuf_iterator path leaves the
#     stream reporting good() while dropping bytes (sections 3 and 4).
#   - failed() lives on the iterator std::vformat_to RETURNS, not the
#     one you pass in — vformat_to copies by value on every advance,
#     the same trap the Aug 25/26/28 custom iterators hit (section 5).
#   - The iterator ignores stream state in BOTH directions: it writes
#     bytes through a stream already in badbit, where os.write's sentry
#     would refuse (section 13), and it bypasses exceptions() masks
#     entirely (section 12).
#   - basic_ios::rdbuf(sb*) calls clear(), so a naive scoped filter
#     erases stream errors on install AND on restore (section 11).
#   - std::ostringstream::rdbuf()/std::ofstream::rdbuf() are 0-arg
#     members that HIDE basic_ios::rdbuf(streambuf*); calling
#     oss.rdbuf(&filter) is a hard compile error on libc++ 21
#     (section 11's closing note).
#   - The Format error path is atomic on libc++ 21 — zero bytes reach
#     the sink before the throw, verified up to a 20000-char prefix
#     (section 10). Pinned as an observation, not a standard guarantee.
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-30-ostream-partial-write
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-30-ostream-partial-write
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-30-ostream-partial-write
#
# NOTE on ASan output: the program writes a progress line per section
# to stderr and the PASS tally at exit. ASan/UBSan add nothing when the
# run is clean. Run with detect_leaks unset — LeakSanitizer is not
# supported on darwin/arm64 (probed: "detect_leaks is not supported on
# this platform"), so this lesson keeps every format argument in a
# named local rather than relying on the sanitizer to catch a leak.

cmake_minimum_required(VERSION 3.16)
project(ostream_partial_write_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise in partial-write detection for ostream format adapters on Apple Clang 21.0.0 / libc++ — covers a non-consuming capped_streambuf sink, a counting_streambuf accounting filter, an RAII scoped_counting_filter that survives basic_ios::rdbuf's clear(), and petra::ostream_write_checked / ostream_write_checked_via_buf returning std::expected<size_t, WriteError> with Format and PartialWrite kinds; pins the badbit-vs-silent asymmetry between the vformat->write and vformat_to->ostreambuf_iterator paths, the returned-iterator failed() flag, format_error atomicity by payload size, exception-mask bypass, UTF-8 mid-codepoint splits, and embedded NUL passthrough."
    LANGUAGES CXX
)

# C++23 for std::format / std::vformat_to (P2216R3) and std::expected (P0323R12).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-30-ostream-partial-write
    P-2026-08-30-ostream-partial-write.cpp
)

# The checked adapters catch std::format_error, and section 12 catches
# std::ios_base::failure, so exceptions must stay enabled.
target_compile_options(P-2026-08-30-ostream-partial-write PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-30-ostream-partial-write PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-30-ostream-partial-write PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
