# CMakeLists.txt — today's P-2026-08-31 lesson:
#
#   ostream_write_n_via_buf — a std::expected<size_t, FormatError>
#   wrapper that composes std::format_to_n (P2216R3) directly with
#   std::ostreambuf_iterator<char> as the output iterator. Zero
#   intermediate std::string allocation on the write path; the
#   constrained-cap family of the Aug 24/25/26/28 chain lifts over
#   to an ostream sink.
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23)
#   - Aug 24   std::format_to_n in std::expected (C++23)
#   - Aug 25   std::format_to_n_into — NUL-terminating bounded formatter
#   - Aug 26   std::format_to_n_runtime_string — runtime-fmt wrapper
#   - Aug 27   format_to_n_runtime_string_projected — LogError projection
#   - Aug 28   std::ostream_print — std::print(ostream&, ...) adapter
#              closing the libc++ 21 missing-overload gap
#   - Aug 30   ostream_write_checked — partial-write detection for the
#              Aug 28 adapter (custom streambuf that refuses bytes)
#   - TODAY    ostream_write_n_via_buf — std::format_to_n straight to
#              std::ostreambuf_iterator<char> (zero-allocation write);
#              closes the THIRD of the four new follow-on items Aug 28's
#              "Where we go next" listed: "std::format_to_n straight to
#              ostream streambuf — P2216R3 doesn't ship this overload
#              either; the same bounded_ostream_writer machinery from
#              Aug 28 could be lifted to format_to_n's runtime path."
#
# What today covers
# -----------------
#   - petra::ostream_write_n_via_buf_runtime(ostream&, cap, fmt, args) —
#     runtime-format-string entry point returning
#     std::expected<std::size_t, petra::FormatError> with an
#     expected<size_t, format_error> shape on the call-site cap and a
#     PartialWrite kind carrying sink-side offered/accepted split when
#     the downstream buffer refuses bytes.
#   - petra::ostream_write_n_via_buf(ostream&, cap, fmt, args) — the
#     consteval-checked sibling that forwards to the runtime entry
#     point via std::make_format_args (the same consteval-vs-runtime
#     split Aug 28 and Aug 30 established).
#   - The interaction between std::format_to_n's iterator-side cap
#     (the n argument) and a streambuf's own capacity: each caps the
#     write independently, and a real sink can saturate one without
#     touching the other. Section 5 maps the four regimes (cap<text,
#     cap==text, sink<text, sink==text, both) on the same payload.
#   - The same counting_streambuf filter Aug 30 ships, lifted into
#     this TU so the call-site reports a real sink-side byte count
#     even when std::format_to_n reports only its iterator-side cap.
#   - A std::format_to_n-with-format_error semantic probe: the
#     format_to_n iterator-side cap does NOT suppress parse errors
#     (the parser runs before any byte moves), so a malformed format
#     string throws std::format_error with zero bytes written even
#     when n is large.
#   - The first finding of the lesson: std::format_to_n accepts
#     std::ostreambuf_iterator<char> as its Out parameter DIRECTLY
#     (P2216R3 specifies output_iterator requirements, ostreambuf_
#     iterator models output_iterator), so today's wrapper is one
#     std::format_to_n call, not a hand-rolled bounded_ostream_writer
#     pair. The Aug 28 path that did need a custom iterator did so
#     because std::vformat_to has no n parameter — today's wrapper
#     uses the right tool for the cap-on-iterator case.
#   - The second finding: the would-be size returned by std::format_to_n
#     is the size the formatter WANTED to write, not the size that
#     actually landed. On a sink that caps below the iterator cap,
#     std::format_to_n still reports the full would-be; the sink-side
#     PartialWrite kind fills in the actual byte count. The two are
#     complementary, not redundant.
#
# Build (default — the standard -O0 debug build for this lesson):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-31-ostream-write-n-via-buf
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-31-ostream-write-n-via-buf
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-31-ostream-write-n-via-buf

cmake_minimum_required(VERSION 3.16)
project(ostream_write_n_via_buf_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::format_to_n straight to std::ostreambuf_iterator<char> on Apple Clang 21.0.0 / libc++ — covers the bounded zero-allocation sibling to the Aug 28 ostream adapter, returning std::expected<size_t, FormatError>; the iterator-side-cap vs sink-side-cap independence; the counting_streambuf filter reused from Aug 30 for sink-side offered/accepted accounting; format_error atomicity; runtime-format-string entry point; consteval-vs-runtime split; std::ofstream sink; UTF-8 mid-codepoint truncation; embedded NUL passthrough; size_t vs streamsize strictness handling; 50-round determinism stress."
    LANGUAGES CXX
)

# C++23 for std::format_to_n (P2216R3) and std::expected (P0323R12).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-31-ostream-write-n-via-buf
    P-2026-08-31-ostream-write-n-via-buf.cpp
)

target_compile_options(P-2026-08-31-ostream-write-n-via-buf PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-31-ostream-write-n-via-buf PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-31-ostream-write-n-via-buf PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
