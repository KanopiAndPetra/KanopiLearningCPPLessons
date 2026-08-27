# CMakeLists.txt — P-2026-08-27 lesson:
#
#   std::format_to_n_runtime_string_projected: a consumer-side
#   expected<size_t, LogError> projection of Aug 26's runtime-
#   format-string bounded-buffer wrapper. Closes the third new
#   follow-on item from Aug 26's "Where we go next" section:
#
#     > std::format_to_n_runtime_string for runtime-checked
#     > consumer-side std::expected<E, ...> projection — today's
#     > wrapper returns expected<size_t, format_error>. A natural
#     > addition is a transform_error overload that projects to a
#     > domain-specific error type (e.g. expected<size_t, MyError>
#     > where MyError::FormatError(FormatError) for the format
#     > failure, MyError::OutOfMemory for an allocation failure
#     > in pass 1, and MyError::BufferTooSmall for the truncation
#     > case). That's a consumer-side concern and probably doesn't
#     > warrant a separate lesson.
#
#   TODAY is that lesson. The fix to Aug 26's "probably doesn't
#   warrant a separate lesson" line: it does — there is a real
#   domain-ErrorType contract to design (what does "truncation"
#   mean — a success or a failure?), a non-trivial decision about
#   what the new error types are (Format, EmptyBuffer, Overflow
#   in today's case), and a focused tour of std::expected's
#   transform_error + manual rewrap in a real setting.
#
# Where this fits in the arc
# --------------------------
#   - Aug 22   std::print / std::println / std::format (C++23) —
#              the consteval-checked format-string surface
#   - Aug 24   std::format_to_n wrapped in std::expected<size_t,
#              format_error> (C++23) — bounded-buffer formatter
#              with a typed error channel
#   - Aug 25   std::format_to_n_into — NUL-terminating companion
#              to Aug 24
#   - Aug 26   std::format_to_n_runtime_string — runtime-format-
#              string companion: expected<size_t, format_error>
#              with parse / arg-index / type errors as runtime
#              std::format_error throws instead of compile errors
#   - TODAY    std::format_to_n_runtime_string_projected — same
#              wrapper BUT the error type is domain-specific
#              (LogError with Kind::Format / EmptyBuffer / Overflow)
#              instead of std::format_error. Demonstrates
#              std::expected::transform_error composition in a
#              non-trivial setting + the design decision about
#              how truncation maps to a domain error.
#
# What today covers
# -----------------
#   - Aug 26's wrapper is inlined verbatim in spirit (so today's
#     TU is self-contained — a single-file demonstration).
#   - A small domain ErrorType `LogError` with three failure
#     kinds: Format (carries the format_error as a payload),
#     EmptyBuffer (n == 0), Overflow (truncation — the Aug 26
#     wrapper silently returned would_be_written but the domain
#     treats that as a failure).
#   - A custom std::formatter<LogError> so test CHECKs can print
#     it. libc++ 21 has no built-in formatter for arbitrary
#     types — that's a small custom type the lesson implements.
#   - The transform_error functor MapToLogError: takes a const
#     std::format_error&, returns a bare LogError (not wrapped in
#     std::unexpected — libc++ 21's transform_error path wants
#     the functor to return the new error type, not unexpected<E>).
#   - The projected wrapper: same body as Aug 26 for pass 1 / pass 2,
#     plus a post-processing step that (a) rewrites the error type
#     via the transform_error functor and (b) detects overflow
#     by comparing would_be_written against n.
#   - The transform_error manual rewrap: libc++ 21's static_assert
#     rejects the "if the functor returns std::unexpected<E>" path,
#     so we construct `expected<size_t, LogError>{std::unexpect,
#     mapped}` explicitly. This is a real-world gotcha that the
#     Aug 26 lesson notes flagged when they said "transform_error
#     overload" — it's worth a focused demo.
#   - A type-erased `std::optional<FormatPayload>` payload: only
#     the Format kind carries one. EmptyBuffer / Overflow have no
#     payload. (Cheaper to construct; can't be class-hierarchy
#     polymorphic; std::variant would be heavier for this size.)
#   - The size-one-buffer edge: n == 1 means cap == 0, so the
#     wrapper writes zero chars + writes the NUL at out[0]. If
#     any format string has would_be >= 1, today classifies this
#     as Overflow (NOT EmptyBuffer — EmptyBuffer is only n == 0).
#
# Build (default — standard -O0 debug build):
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/P-2026-08-27-format-to-n-runtime-string-projected
#
# Strict-warning build:
#
#   cmake -S . -B build-strict \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-27-format-to-n-runtime-string-projected
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-27-format-to-n-runtime-string-projected

cmake_minimum_required(VERSION 3.16)
project(format_to_n_runtime_string_projected_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer exercise of std::format_to_n_runtime_string_projected — a consumer-side expected<size_t, LogError> PROJECTION of Aug 26's std::format_to_n_runtime_string. Closes the third new follow-on item from Aug 26's lesson notes. Composes with the Aug 26 wrapper via a custom transform_error functor (MapToLogError) that maps std::format_error to a domain LogError with three failure kinds: Format (carries a payload message), EmptyBuffer (n == 0), Overflow (truncation). libc++ 21's transform_error rejects the std::unexpected<E> return path with a static_assert — the functor must return a bare LogError, and today's wrapper manually constructs expected<size_t, LogError>{std::unexpect, mapped}. Truncation is detected by comparing would_be_written against n AFTER the transform — Aug 26's wrapper silently returned would_be_written on truncation but the domain treats that as a failure. Built on Apple Clang 21.0.0 / libc++."
    LANGUAGES CXX
)

# C++23 for std::vformat (P2216R3 runtime path), std::vformat_to,
# std::expected (P0323R12), and std::optional payload.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(P-2026-08-27-format-to-n-runtime-string-projected
    P-2026-08-27-format-to-n-runtime-string-projected.cpp
)

target_compile_options(P-2026-08-27-format-to-n-runtime-string-projected PRIVATE -fexceptions)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-27-format-to-n-runtime-string-projected PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-27-format-to-n-runtime-string-projected PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
