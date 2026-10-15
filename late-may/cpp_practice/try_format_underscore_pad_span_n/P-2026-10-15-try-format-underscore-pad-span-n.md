# P-2026-10-15 — try_format_underscore_pad_span_n

**Closes Oct 14's "Where we go next" item (1) verbatim — the
STD::SPAN\<CHAR\> MIRROR of Oct 14's try_format_underscore_pad_n.**
Today's lesson introduces `PETRA_UNDERSCORE_PAD` (a NEW SEVENTH
compile-time flag on top of Oct 13's 6-bit tuple
`(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
PETRA_DASH_PAD, PETRA_HASH_PAD, PETRA_TILDE_PAD)`) on the
STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.  `PETRA_UNDERSCORE_PAD=1`
selects the UNDERSCORE-padded span\<char\> sibling, which fills
bytes `[written, span.size())` with `'_'` (ASCII 95) on success.

The `'_'` character is **TOTALLY DIFFERENT** from Oct 13's
`'~'` (126), Oct 11's `'#'` (35), Oct 4's `'-'` (45), Sep 28's
`' '` (32), and Sep 27's `'\0'` (0).  The `'_'` byte has ASCII
value `95` (0x5F), distinct from the four prior pad bytes and the
poison byte `'Q'` (0x51).

After today, the matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_UNDERSCORE_PAD)` 2-bit tuple is
COMPLETE for ALL FOUR cells on BOTH axes (CHAR\*+CAP and
STD::SPAN\<CHAR\>).  The CHAR\*+CAP axis was closed by Oct 14
(4 cells = 2 × 2); the STD::SPAN\<CHAR\> axis is closed by today
(4 cells = 2 × 2).  Both axes share the SAME wrapper(s) per cell:
`(0,0)` → Sep 8/10/19 underlying untouched sibling;
`(0,1)` → TODAY's NEW UNDERSCORE-pad expected sibling;
`(1,0)` → Sep 20/21 underlying throwing untouched sibling;
`(1,1)` → TODAY's NEW UNDERSCORE-pad throwing sibling.

## Where this fits in the arc

| Lesson       | Topic                                            |
|--------------|--------------------------------------------------|
| Sep 10       | `try_format_bounded_runtime_n_span` (STD::SPAN\<CHAR\> × CONSTEVAL × EXPECTED) |
| Sep 19       | `try_format_runtime_n(span, ...)` (STD::SPAN\<CHAR\> × RUNTIME × EXPECTED) |
| Sep 21       | `try_format_runtime_n_throwing(span, ...)` (STD::SPAN\<CHAR\> × RUNTIME × THROWING) |
| Sep 27       | NUL-pad (PETRA_PAD) span\<char\>                 |
| Sep 28       | SPACE-pad (PETRA_SPACE_PAD) span\<char\>         |
| Oct 1        | NUL × SPACE composition on span\<char\>          |
| Oct 4        | DASH-pad (PETRA_DASH_PAD) span\<char\>           |
| Oct 5 / Oct 8| PAD × DASH composition on span\<char\>           |
| Oct 11       | HASH-pad (PETRA_HASH_PAD) span\<char\>           |
| Oct 13       | TILDE-pad (PETRA_TILDE_PAD) span\<char\>         |
| Oct 14       | UNDERSCORE-pad (PETRA_UNDERSCORE_PAD) char\*+cap |
| **TODAY**    | **UNDERSCORE-pad (PETRA_UNDERSCORE_PAD) span\<char\>** |

## Headline findings

1. **The UNDERSCORE-pad span\<char\> sibling is a thin layer on
   top of Oct 13's TILDE-pad span\<char\> sibling.**  Today's
   UNDERSCORE-pad wrapper calls Oct 13's wrapper, then
   OVERWRITES the `'~'` fill with `'_'` on success.  This is the
   same pattern Oct 13 used (TILDE layer on Oct 11's HASH-pad
   span\<char\> sibling) and Oct 11 used (HASH layer on Oct 4's
   DASH-pad span\<char\> sibling).

2. **The composition rule "the LATER fill overwrites the
   EARLIER fill" now has SIX layers:
   `UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`.**  When
   ALL SIX pad-flags are set, the end byte state is always
   `'_'`.  Composition cells with `(UNDERSCORE=1, TILDE=*)`
   REUSE TODAY's NEW UNDERSCORE-pad wrappers verbatim (the
   `(UNDERSCORE=1, TILDE=*)` cell and `(UNDERSCORE=1, TILDE=0)`
   cell share wrappers because UNDERSCORE overwrites TILDE,
   which overwrites HASH which overwrites DASH which overwrites
   SPACE).

3. **No new error contracts are introduced.**  The new
   wrappers preserve the SAME contract as Oct 13's TILDE-pad
   span\<char\> wrappers:
   - **Success path:** returns the byte count via
     `expected<size_t, FormatError>` (EXPECTED siblings) or
     `std::size_t` (THROWING siblings).  Bytes
     `[written, span.size())` are `'_'`.
   - **TooLarge path:** returns
     `unexpected{FormatError{TooLarge, ..., needed_bytes}}`
     (EXPECTED siblings) or throws `std::runtime_error` with
     the same message (THROWING siblings).  Bytes
     `[0, span.size())` are UNTOUCHED (no partial `'_'` fill
     — the buffer's atomicity-on-error contract is preserved).
   - **Format error path:** returns
     `unexpected{FormatError{Format, ...}}` or throws
     `std::runtime_error` similarly.  Buffer UNTOUCHED.
   - **Sub-span no-bleed:** bytes outside the span's writable
     region are UNTOUCHED (a 6-byte sub-span at offset 5 of
     a 16-byte buffer leaves bytes `[0,5)` and `[11,16)` as
     poison `'Q'` — pinned by Section 2b).

4. **The CONSTEVAL gate is preserved regardless of the
   `(THROW, UNDERSCORE)` 2-bit tuple** —
   `TRY_FORMAT_BOUNDED_SPAN_N_UNDERSCORE_OR_THROW` requires
   `std::format_string<Args...>` (a literal fmt at the call
   site).  Both the CONSTEVAL-surface and the RUNTIME-surface
   dispatcher macros are provided.

5. **The macros `TRY_FORMAT_BOUNDED_SPAN_N_UNDERSCORE_OR_THROW`
   (CONSTEVAL surface) and
   `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_UNDERSCORE_OR_THROW`
   (RUNTIME surface) are verbatim thin dispatchers** that
   pick between FOUR sibling wrappers at preprocessor time
   based on the `(PETRA_THROW_ON_OVERFLOW,
   PETRA_UNDERSCORE_PAD)` 2-bit tuple.  Each macro is a
   parenthesized expression (NOT `do { } while (0)`), so the
   caller can write
   `auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_UNDERSCORE_OR_THROW(...)`
   and capture the result.  Each macro uses `__VA_OPT__(,)` for
   the no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep
   16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
   Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3
   / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 / Oct 13 / Oct 14's
   macro shape).

6. **The STD::SPAN\<CHAR\> × BOUNDED-BUFFER matrix on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_UNDERSCORE_PAD)` 2-bit tuple
   is now COMPLETE for all four cells.**  The CHAR\*+CAP
   side was closed by Oct 14 (4 cells = 2 × 2); the
   STD::SPAN\<CHAR\> side is closed by today (4 cells = 2 ×
   2).  Both axes share the SAME wrapper(s) per cell:
   `(0,0)` → Sep 8/10/19 underlying untouched sibling;
   `(0,1)` → TODAY's NEW UNDERSCORE-pad expected sibling;
   `(1,0)` → Sep 20/21 underlying throwing untouched sibling;
   `(1,1)` → TODAY's NEW UNDERSCORE-pad throwing sibling.

## Important code

The four NEW sibling wrappers follow Oct 13's TILDE-pad-shape
verbatim.  The RUNTIME / EXPECTED span\<char\> sibling is the
canonical example:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_underscore_pad_span(std::span<char> out,
                                         std::string_view fmt,
                                         const Args&... args) {
    auto r = try_format_runtime_n_tilde_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        // LATER fill (UNDERSCORE) overwrites EARLIER fill
        // (TILDE) from the TILDE-pad span<char> sibling.  On
        // error, r.has_value() is false and the loop is
        // skipped, preserving the buffer's
        // atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '_';
        }
    }
    return r;
}
```

The CONSTEVAL sibling is a thin forwarding wrapper:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_underscore_pad_span(
        std::span<char> out, std::format_string<Args...> fmt,
        const Args&... args) {
    auto r = try_format_bounded_runtime_n_tilde_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '_';
        }
    }
    return r;
}
```

The THROWING sibling uses the same idiom Oct 13 established, so
the `std::runtime_error` thrown by the underlying TILDE-pad
span\<char\> sibling is re-raised untouched:

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_underscore_pad_span(
        std::span<char> out, std::string_view fmt,
        const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_tilde_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '_';
        }
        return n;
    } catch (...) {
        throw;  // re-raise unchanged
    }
}
```

The CONSTEVAL + RUNTIME dispatcher macros:

```cpp
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_UNDERSCORE_PAD == 0
#define TRY_FORMAT_BOUNDED_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)                \
                                                 __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                             \
                                    __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_UNDERSCORE_PAD == 1
#define TRY_FORMAT_BOUNDED_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_underscore_pad_span((out), (fmt)      \
                                                           __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_underscore_pad_span((out), (fmt)              \
                                                   __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_UNDERSCORE_PAD == 0
#define TRY_FORMAT_BOUNDED_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)       \
                                                          __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                    \
                                             __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_UNDERSCORE_PAD == 1
#define TRY_FORMAT_BOUNDED_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_underscore_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_UNDERSCORE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_underscore_pad_span((out), (fmt)     \
                                                           __VA_OPT__(,) __VA_ARGS__))
#endif
```

## Where this lesson diverges from Oct 13 / Oct 14

| Aspect              | Oct 13 / Oct 14                                | TODAY                                       |
|---------------------|------------------------------------------------|---------------------------------------------|
| Pad character       | `'~'` (126)                                    | `'_'` (95)                                  |
| Composed on top of  | Oct 11's HASH-pad span\<char\> sibling (`'#'` = 35) | Oct 13's TILDE-pad span\<char\> sibling (`'~'` = 126) |
| 2-bit-tuple surface | `(THROW, TILDE)` only                          | `(THROW, UNDERSCORE)` only                  |
| Composition rule    | `TILDE > HASH > DASH > SPACE > NUL`            | `UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL` |
| Other flags         | PAD / SPACE_PAD / DASH_PAD / HASH_PAD must be 0 | PAD / SPACE_PAD / DASH_PAD / HASH_PAD / TILDE_PAD must be 0 |
| `#error` pointer    | → Sep 27 / Sep 28 / Oct 1 / Oct 4 / Oct 5 / Oct 8 / Oct 11 | → Sep 27 / Sep 28 / Oct 1 / Oct 4 / Oct 5 / Oct 8 / Oct 11 / Oct 13 |
| Surfaces provided   | CONSTEVAL+RUNTIME (Oct 13); RUNTIME only (Oct 14) | BOTH CONSTEVAL + RUNTIME surfaces (TODAY)    |
| Axis number         | SIXTH pad character on CHAR\*+CAP / span\<char\> | SEVENTH pad character on span\<char\> only  |

The narrower scope (PAD / SPACE_PAD / DASH_PAD / HASH_PAD /
TILDE_PAD must all be 0) is the right move for one focused
lesson.  All those composition cells REUSE the Sep 27 / Sep 28 /
Oct 1 / Oct 4 / Oct 5 / Oct 8 / Oct 11 / Oct 13 helpers; a
future lesson can introduce an 8-bit-tuple (THROW × PAD × SPACE
× DASH × HASH × TILDE × UNDERSCORE) dispatcher that composes
all seven flags following the same pattern.

## Observed test results (79 CHECKs across 11 sections)

- **(0, 0)** expected+untouched: **79/79 PASS** (default cell)
- **(0, 1)** expected+UNDERSCORE-padded: **79/79 PASS**
  (`-DPETRA_UNDERSCORE_PAD=1`)
- **(1, 0)** throwing+untouched: **79/79 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=1`)
- **(1, 1)** throwing+UNDERSCORE-padded: **79/79 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_UNDERSCORE_PAD=1`)

Five consecutive runs on each cell produced identical 79/79
PASS, 0 FAIL tallies (no flakiness).  Section 10's
concurrent dispatch test (4 threads × 25 calls each =
100 calls per cell) produces 100/100 PASS per cell × 4
cells × 5 runs = 2,000 concurrent calls, all PASS.

- `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`: **zero warnings** (verified on
  `build-strict` for all four `(THROW, UNDERSCORE)` cells)
- `-fsanitize=address -fsanitize=undefined -O1
  -fno-omit-frame-pointer`: **zero sanitizer diagnostics**
  (verified on `build-asan` for all four cells)

## Friendly-error firing (verified)

- `-DPETRA_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_span_n) only supports
  PETRA_PAD=0.  For PETRA_PAD=1, use Sep 27's
  try_format_padded_span_n_or_throw or Oct 1's
  try_format_padded_space_pad_span_n_or_throw (NUL-padded
  span\<char\> wrappers)."`
- `-DPETRA_SPACE_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_span_n) only supports
  PETRA_SPACE_PAD=0.  For PETRA_SPACE_PAD=1, use Sep 28's
  try_format_space_pad_span_n (STD::SPAN\<CHAR\> SPACE-padded
  sibling) or Oct 1's
  try_format_padded_space_pad_span_n_or_throw (composition
  cells)."`
- `-DPETRA_DASH_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_span_n) only supports
  PETRA_DASH_PAD=0.  For PETRA_DASH_PAD=1, use Oct 4's
  try_format_dash_pad_span_n (STD::SPAN\<CHAR\> DASH-padded
  sibling) or Oct 5 / Oct 8's composition cells."`
- `-DPETRA_HASH_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_span_n) only supports
  PETRA_HASH_PAD=0.  For PETRA_HASH_PAD=1, use Oct 11's
  try_format_hash_pad_span_n (STD::SPAN\<CHAR\> HASH-padded
  sibling).  Note: today's UNDERSCORE-pad wrapper REUSES Oct
  13's TILDE-pad wrapper internally, which REUSES Oct 11's
  HASH-pad wrapper — if you set PETRA_HASH_PAD=1 explicitly,
  the dispatcher #errors to avoid double-pad ambiguity."`
- `-DPETRA_TILDE_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_span_n) only supports
  PETRA_TILDE_PAD=0.  For PETRA_TILDE_PAD=1, use Oct 13's
  try_format_tilde_pad_span_n (STD::SPAN\<CHAR\> TILDE-padded
  sibling).  Note: today's UNDERSCORE-pad wrapper REUSES Oct
  13's TILDE-pad wrapper internally — if you set
  PETRA_TILDE_PAD=1 explicitly, the dispatcher #errors to
  avoid double-pad ambiguity."`
- `-DPETRA_UNDERSCORE_PAD=2`: fires
  `"PETRA_UNDERSCORE_PAD must be 0 (no underscore-pad) or 1
  (underscore-padded)"`
- `-DPETRA_THROW_ON_OVERFLOW=2`: fires
  `"PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1
  (throwing)"`
- `-DPETRA_UNDERSCORE_PAD=1 -DPETRA_TILDE_PAD=1`: fires the
  `PETRA_TILDE_PAD` error (composite guard correctly
  redirects to the `PETRA_TILDE_PAD` axis — the file inlines
  Oct 13's TILDE-pad sibling, so the dispatcher's
  `(THROW × TILDE)` 2-bit tuple is already baked in; an
  explicit `PETRA_TILDE_PAD=1` would create
  double-pad ambiguity).

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`
- `std::expected<T, E>` (P0323R12)
- `std::println` (P2093R14)
- `std::atomic` + `std::array<std::thread, 4>` +
  `std::thread::join()` for the concurrent-dispatch test
- `std::is_same_v<T1, T2>` for decltype pinning of all four
  `(THROW, UNDERSCORE)` cells
- `std::format_error` + `std::runtime_error` +
  `std::string` + `std::string_view` + `std::to_string`
- `std::algorithm::fill` (for the poison-byte setup)
- `std::fprintf` (the PASS/FAIL messaging uses `fprintf`
  to avoid the formatter specialization requirement)
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range` +
  `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v` (via the static_asserts in
  Parts 2 and 3)
- `std::indirectly_writable` + `std::output_iterator`
  (for the `counting_output_iterator` /
  `bounded_char_writer` static_asserts)
- `__VA_OPT__(,)` preprocessor feature (C++20)
- `#error` preprocessor feature
- `#elif defined(...)` preprocessor feature (used for the
  `PETRA_TILDE_PAD` friendly-error guard, since the macro
  is not defined in this file unless the user sets it)

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA\_THROW=0, PETRA\_UNDERSCORE\_PAD=0):

```bash
cd late-may/cpp_practice/try_format_underscore_pad_span_n
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-15-try-format-underscore-pad-span-n.cpp \
        -o /tmp/P-2026-10-15-try-format-underscore-pad-span-n
/tmp/P-2026-10-15-try-format-underscore-pad-span-n
```

Or, via CMake:

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-15-try-format-underscore-pad-span-n
```

UNDERSCORE-pad cell (PETRA\_UNDERSCORE\_PAD=1):

```bash
cmake -S . -B build-understand -DPETRA_UNDERSCORE_PAD=ON
cmake --build build-understand
./build-understand/P-2026-10-15-try-format-underscore-pad-span-n
```

Throwing+UNDERSCORE-pad cell (PETRA\_THROW\_ON\_OVERFLOW=1,
PETRA\_UNDERSCORE\_PAD=1):

```bash
cmake -S . -B build-throw-underscore \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_UNDERSCORE_PAD=ON
cmake --build build-throw-underscore
./build-throw-underscore/P-2026-10-15-try-format-underscore-pad-span-n
```

Strict-warning build:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-15-try-format-underscore-pad-span-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-15-try-format-underscore-pad-span-n
```

## Where we go next

The SEVENTH pad character (PETRA\_UNDERSCORE\_PAD) is now
closed on the STD::SPAN\<CHAR\> side (today).  The matrix on
the `(PETRA\_THROW\_ON\_OVERFLOW × PETRA\_UNDERSCORE\_PAD)`
2-bit tuple is COMPLETE for ALL FOUR cells on BOTH axes
(CHAR\*+CAP closed by Oct 14; STD::SPAN\<CHAR\> closed by
today).  Future lessons can either:

1. **An EIGHTH-axis pad character** — e.g.
   `PETRA_STAR_PAD` (`'*'`, ASCII 42) or
   `PETRA_AT_PAD` (`'@'`, ASCII 64).  Each such lesson
   would mirror Oct 12 / Oct 13 / Oct 14 / today's pattern
   exactly — a thin layer on top of today's UNDERSCORE-pad
   sibling, OVERWRITING the `'_'` fill with the new pad byte
   on success.  Closes the matrix on the
   `(THROW, NEW_PAD)` 2-bit tuple for BOTH axes.
2. **A unified 8-bit-tuple dispatcher
   `(THROW × PAD × SPACE × DASH × HASH × TILDE × UNDERSCORE
   × NEW_PAD)`** — one macro covers all composition cells
   across all seven pad flags, picking the right wrapper
   at preprocessor time via `__VA_OPT__(,)`-based dispatch.
3. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now that
   the EIGHTH + TENTH axes are fully closed on BOTH sides.

The current matrix on the BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis now reaches SEVEN
pad characters on BOTH sides (CHAR\*+CAP and STD::SPAN\<CHAR\>):
NONE (no fill) → NUL (Sep 25 / Sep 27) → SPACE (Sep 28 /
Sep 29) → DASH (Oct 3 / Oct 4) → HASH (Oct 9 / Oct 11) →
TILDE (Oct 12 / Oct 13) → UNDERSCORE (Oct 14 / today).  The
SEVENTH-axis lesson (Oct 14 on CHAR\*+CAP / today on
STD::SPAN\<CHAR\>) is the FINAL axis in this pad-character
arc at the moment.  Future expansion would target an EIGHTH
character on one or both axes.