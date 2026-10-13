# P-2026-10-13 — try_format_tilde_pad_span_n

**Closes Oct 12's "Where we go next" item (1) verbatim — the
STD::SPAN\<CHAR\> MIRROR of Oct 12's try_format_tilde_pad_n.**
Today's lesson introduces `PETRA_TILDE_PAD` (a NEW SIXTH
compile-time flag on top of Oct 11's 5-bit tuple
`(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
PETRA_DASH_PAD, PETRA_HASH_PAD)`) on the STD::SPAN\<CHAR\> ×
BOUNDED-BUFFER axis.  `PETRA_TILDE_PAD=1` selects the
TILDE-padded span\<char\> sibling, which fills bytes
`[written, span.size())` with `'~'` on success.

The `'~'` character is **TOTALLY DIFFERENT** from Oct 11's
`'#'` (the HASH-pad fill), Oct 4's `'-'` (the DASH-pad fill),
Sep 28's `' '` (the SPACE-pad fill), and Sep 27's `'\0'`
(the NUL-pad fill).  The `'~'` byte has ASCII value `126`
(0x7E), distinct from the four prior pad bytes and the poison
byte `'Q'` (0x51).

After today, the matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_TILDE_PAD)` 2-bit tuple is
COMPLETE for ALL FOUR cells on BOTH axes (CHAR\*+CAP and
STD::SPAN\<CHAR\>).  The CHAR\*+CAP axis was closed by Oct 12;
the STD::SPAN\<CHAR\> axis is closed by today.

## Where this fits in the arc

| Lesson              | Topic                                            |
|---------------------|--------------------------------------------------|
| Sep 10              | `try_format_bounded_runtime_n_span` (STD::SPAN\<CHAR\> × CONSTEVAL × EXPECTED) |
| Sep 19              | `try_format_runtime_n(span, ...)` (STD::SPAN\<CHAR\> × RUNTIME × EXPECTED) |
| Sep 21              | `try_format_runtime_n_throwing(span, ...)` (STD::SPAN\<CHAR\> × RUNTIME × THROWING) |
| Sep 27              | NUL-pad (PETRA_PAD) span\<char\>                 |
| Sep 28              | SPACE-pad (PETRA_SPACE_PAD) span\<char\>         |
| Oct 1               | NUL × SPACE composition on span\<char\>          |
| Oct 4               | DASH-pad (PETRA_DASH_PAD) span\<char\>           |
| Oct 5 / Oct 8       | PAD × DASH composition on span\<char\>           |
| Oct 11              | HASH-pad (PETRA_HASH_PAD) span\<char\>           |
| **TODAY**           | **TILDE-pad (PETRA_TILDE_PAD) span\<char\>**     |

## Headline findings

1. **The TILDE-pad span\<char\> sibling is a thin layer on top
   of Oct 11's HASH-pad span\<char\> sibling.**  Today's
   TILDE-pad wrapper calls Oct 11's wrapper, then OVERWRITES
   the `'#'` fill with `'~'` on success.  This is the same
   pattern Oct 12 used (TILDE layer on Oct 9's HASH-pad
   char\*+cap sibling).

2. **The composition rule "the LATER fill overwrites the
   EARLIER fill" now has FIVE layers:
   `TILDE > HASH > DASH > SPACE > NUL`.**  When ALL FIVE
   pad-flags are set, the end byte state is always `'~'`.
   Composition cells with `(TILDE=1, HASH=*)` REUSE TODAY's
   NEW TILDE-pad wrappers verbatim (the `(TILDE=1, HASH=*)`
   cell and `(TILDE=1, HASH=0)` cell share wrappers because
   TILDE overwrites HASH, and HASH overwrites DASH
   overwrites SPACE).

3. **No new error contracts are introduced.**  The new
   wrappers preserve the SAME contract as Oct 11's HASH-pad
   span\<char\> wrappers:
   - **Success path:** returns the byte count via
     `expected<size_t, FormatError>` (EXPECTED siblings) or
     `std::size_t` (THROWING siblings).  Bytes
     `[written, span.size())` are `'~'`.
   - **TooLarge path:** returns
     `unexpected{FormatError{TooLarge, ..., needed_bytes}}`
     (EXPECTED siblings) or throws `std::runtime_error` with
     the same message (THROWING siblings).  Bytes
     `[0, span.size())` are UNTOUCHED (no partial `'~'` fill
     — the buffer's atomicity-on-error contract is preserved).
   - **Format error path:** returns
     `unexpected{FormatError{Format, ...}}` or throws
     `std::runtime_error` similarly.  Buffer UNTOUCHED.
   - **Sub-span no-bleed:** bytes outside the span's writable
     region are UNTOUCHED (a 6-byte sub-span at offset 5 of
     a 16-byte buffer leaves bytes `[0,5)` and `[11,16)` as
     poison `'Q'` — pinned by Section 2b).

4. **The CONSTEVAL gate is preserved regardless of the
   `(THROW, TILDE)` 2-bit tuple** —
   `TRY_FORMAT_BOUNDED_SPAN_N_TILDE_OR_THROW` requires
   `std::format_string<Args...>` (a literal fmt at the call
   site).  Both the CONSTEVAL-surface and the RUNTIME-surface
   dispatcher macros are provided.

5. **The macros `TRY_FORMAT_BOUNDED_SPAN_N_TILDE_OR_THROW`
   (CONSTEVAL surface) and
   `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_TILDE_OR_THROW`
   (RUNTIME surface) are verbatim thin dispatchers** that
   pick between FOUR sibling wrappers at preprocessor time
   based on the `(PETRA_THROW_ON_OVERFLOW, PETRA_TILDE_PAD)`
   2-bit tuple.  Each macro is a parenthesized expression
   (NOT `do { } while (0)`), so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_TILDE_OR_THROW(...)`
   and capture the result.  Each macro uses `__VA_OPT__(,)`
   for the no-args case (mirrors Sep 13 / Sep 14 / Sep 15 /
   Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 /
   Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 /
   Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
   Oct 12's macro shape).

6. **The STD::SPAN\<CHAR\> × BOUNDED-BUFFER matrix on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_TILDE_PAD)` 2-bit tuple
   is now COMPLETE for all four cells.**  The CHAR\*+CAP
   side was closed by Oct 12 (4 cells = 2 × 2); the
   STD::SPAN\<CHAR\> side is closed by today (4 cells = 2 ×
   2).  Both axes share the SAME wrapper(s) per cell:
   `(0,0)` → Sep 8/10/19 underlying untouched sibling;
   `(0,1)` → TODAY's NEW TILDE-pad expected sibling;
   `(1,0)` → Sep 20/21 underlying throwing untouched
   sibling; `(1,1)` → TODAY's NEW TILDE-pad throwing
   sibling.

## Important code

The four NEW sibling wrappers follow Oct 11's HASH-pad-shape
verbatim.  The RUNTIME / EXPECTED span\<char\> sibling is the
canonical example:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_tilde_pad_span(std::span<char> out,
                                     std::string_view fmt,
                                     const Args&... args) {
    auto r = try_format_runtime_n_hash_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        // LATER fill (TILDE) overwrites EARLIER fill (HASH)
        // from the HASH-pad span<char> sibling.  On error,
        // r.has_value() is false and the loop is skipped,
        // preserving the buffer's atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '~';
        }
    }
    return r;
}
```

The CONSTEVAL sibling is a thin forwarding wrapper:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_tilde_pad_span(
        std::span<char> out, std::format_string<Args...> fmt,
        const Args&... args) {
    auto r = try_format_bounded_runtime_n_hash_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '~';
        }
    }
    return r;
}
```

The THROWING sibling uses the same idiom Oct 11 established,
so the `std::runtime_error` thrown by the underlying
HASH-pad span\<char\> sibling is re-raised untouched:

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_tilde_pad_span(
        std::span<char> out, std::string_view fmt,
        const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_hash_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '~';
        }
        return n;
    } catch (...) {
        throw;  // re-raise unchanged
    }
}
```

The CONSTEVAL + RUNTIME dispatcher macros:

```cpp
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_TILDE_PAD == 0
#define TRY_FORMAT_BOUNDED_SPAN_N_TILDE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)                \
                                                 __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_TILDE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                             \
                                    __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_TILDE_PAD == 1
#define TRY_FORMAT_BOUNDED_SPAN_N_TILDE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_tilde_pad_span((out), (fmt)      \
                                                           __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_TILDE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_tilde_pad_span((out), (fmt)              \
                                                   __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_TILDE_PAD == 0
#define TRY_FORMAT_BOUNDED_SPAN_N_TILDE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)       \
                                                          __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_TILDE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                    \
                                             __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_TILDE_PAD == 1
#define TRY_FORMAT_BOUNDED_SPAN_N_TILDE_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_tilde_pad_span(         \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_TILDE_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_tilde_pad_span((out), (fmt)     \
                                                           __VA_OPT__(,) __VA_ARGS__))
#endif
```

## Where this lesson diverges from Oct 11 / Oct 12

| Aspect              | Oct 11 / Oct 12                                | TODAY                                       |
|---------------------|-----------------------------------------------|---------------------------------------------|
| Pad character       | `'#'` (35)                                    | `'~'` (126)                                 |
| Composed on top of  | Oct 4's DASH-pad span\<char\> sibling (`'-'` = 45) | Oct 11's HASH-pad span\<char\> sibling (`'#'` = 35) |
| 2-bit-tuple surface | `(THROW, HASH)` only                          | `(THROW, TILDE)` only                       |
| Composition rule    | `HASH > DASH > SPACE > NUL`                   | `TILDE > HASH > DASH > SPACE > NUL`         |
| Other flags         | PAD / SPACE_PAD / DASH_PAD must be 0          | PAD / SPACE_PAD / DASH_PAD / HASH_PAD must be 0 |
| `#error` pointer    | → Sep 27 / Oct 1 / Oct 5 / Oct 8              | → Sep 27 / Sep 28 / Oct 1 / Oct 4 / Oct 5 / Oct 8 / Oct 11 |
| Surfaces provided   | CONSTEVAL+RUNTIME (Oct 11); RUNTIME only (Oct 12) | BOTH CONSTEVAL + RUNTIME surfaces (TODAY)    |

The narrower scope (PAD / SPACE_PAD / DASH_PAD / HASH_PAD
must all be 0) is the right move for one focused lesson.
All those composition cells REUSE the Sep 27 / Sep 28 / Oct
1 / Oct 4 / Oct 5 / Oct 8 / Oct 11 helpers; a future lesson
can introduce a 7-bit-tuple (THROW × PAD × SPACE × DASH ×
HASH × TILDE) dispatcher that composes all six flags
following the same pattern.

## Observed test results (79 CHECKs across 11 sections)

- **(0, 0)** expected+untouched: **79/79 PASS** (default cell)
- **(0, 1)** expected+TILDE-padded: **79/79 PASS**
  (`-DPETRA_TILDE_PAD=1`)
- **(1, 0)** throwing+untouched: **79/79 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=1`)
- **(1, 1)** throwing+TILDE-padded: **79/79 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_TILDE_PAD=1`)

Five consecutive runs on each cell produced identical 79/79
PASS, 0 FAIL tallies (no flakiness).  Section 10's
concurrent dispatch test (4 threads × 25 calls each =
100 calls per cell) produces 100/100 PASS per cell × 4
cells × 5 runs = 2,000 concurrent calls, all PASS.

- `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`: **zero warnings** (verified on
  `build-strict` for all four `(THROW, TILDE)` cells)
- `-fsanitize=address -fsanitize=undefined -O1
  -fno-omit-frame-pointer`: **zero sanitizer diagnostics**
  (verified on `build-asan` for all four cells)

## Friendly-error firing (verified)

- `-DPETRA_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_span_n) only supports
  PETRA_PAD=0.  For PETRA_PAD=1, use Sep 27's
  try_format_padded_span_n_or_throw or Oct 1's
  try_format_padded_space_pad_span_n_or_throw (NUL-padded
  span<char> wrappers)."`
- `-DPETRA_SPACE_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_span_n) only supports
  PETRA_SPACE_PAD=0.  For PETRA_SPACE_PAD=1, use Sep 28's
  try_format_space_pad_span_n (STD::SPAN<CHAR> SPACE-padded
  sibling) or Oct 1's
  try_format_padded_space_pad_span_n_or_throw (composition
  cells)."`
- `-DPETRA_DASH_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_span_n) only supports
  PETRA_DASH_PAD=0.  For PETRA_DASH_PAD=1, use Oct 4's
  try_format_dash_pad_span_n (STD::SPAN<CHAR> DASH-padded
  sibling) or Oct 5 / Oct 8's composition cells."`
- `-DPETRA_HASH_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_span_n) only supports
  PETRA_HASH_PAD=0.  For PETRA_HASH_PAD=1, use Oct 11's
  try_format_hash_pad_span_n (STD::SPAN<CHAR> HASH-padded
  sibling).  Note: today's TILDE-pad wrapper REUSES Oct
  11's HASH-pad wrapper internally — if you set
  PETRA_HASH_PAD=1 explicitly, the dispatcher #errors to
  avoid double-pad ambiguity."`
- `-DPETRA_TILDE_PAD=2`: fires
  `"PETRA_TILDE_PAD must be 0 (no tilde-pad) or 1
  (tilde-padded)"`
- `-DPETRA_THROW_ON_OVERFLOW=2`: fires
  `"PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1
  (throwing)"`

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`
- `std::expected<T, E>` (P0323R12)
- `std::println` (P2093R14)
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range`
- `std::indirectly_writable<I, T>` /
  `std::output_iterator<I, T>` iterator-concept
  static_asserts (C++20 iterator concepts applied to the
  proxy-based writer)
- `std::atomic<int>` + `std::atomic::fetch_add` for
  concurrent dispatch aggregation
- `std::array<std::thread, 4>` + `std::thread::join()`
- `std::is_same_v<T1, T2>` for decltype pinning of all four
  `(THROW, TILDE)` cells
- `std::format_error` + `std::runtime_error` +
  `std::string` + `std::string_view` + `std::to_string`
- `std::algorithm::fill` (for the poison-byte setup)
- `std::fprintf` (the PASS/FAIL messaging uses `fprintf`
  to avoid the formatter specialization requirement)
- `__VA_OPT__(,)` preprocessor feature (C++20)
- `#error` preprocessor feature

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA\_THROW=0, PETRA\_TILDE\_PAD=0):

```bash
cd late-may/cpp_practice/try_format_tilde_pad_span_n
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-13-try-format-tilde-pad-span-n.cpp \
        -o /tmp/P-2026-10-13-tilde-span.out
/tmp/P-2026-10-13-tilde-span.out
```

Or, via CMake:

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-13-try-format-tilde-pad-span-n
```

TILDE-pad cell (PETRA\_TILDE\_PAD=1):

```bash
cmake -S . -B build-tilde -DPETRA_TILDE_PAD=ON
cmake --build build-tilde
./build-tilde/P-2026-10-13-try-format-tilde-pad-span-n
```

Throwing+TILDE-pad cell (PETRA\_THROW\_ON\_OVERFLOW=1,
PETRA\_TILDE\_PAD=1):

```bash
cmake -S . -B build-throw-tilde \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_TILDE_PAD=ON
cmake --build build-throw-tilde
./build-throw-tilde/P-2026-10-13-try-format-tilde-pad-span-n
```

Strict-warning build:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-13-try-format-tilde-pad-span-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-13-try-format-tilde-pad-span-n
```

## Where we go next

The NINTH axis is now closed on BOTH the CHAR\*+CAP side
(Oct 12) and the STD::SPAN\<CHAR\> side (today).  The
matrix on the `(PETRA_THROW_ON_OVERFLOW × PETRA_TILDE_PAD)`
2-bit tuple is COMPLETE for ALL FOUR cells on BOTH axes.
Future lessons can either:

1. **NTH axis (a different pad character on the
   `char*+cap` / `std::span<char>` axis)** — e.g.
   `PETRA_UNDERSCORE_PAD` (`'_'`, ASCII 95) or
   `PETRA_STAR_PAD` (`'*'`, ASCII 42).  Each such lesson
   would mirror Oct 12 / today's pattern exactly.
2. **A unified 7-bit-tuple dispatcher
   `(THROW × PAD × SPACE × DASH × HASH × TILDE × NEW)`** —
   one macro covers all composition cells across all six
   pad flags, picking the right wrapper at preprocessor
   time via `__VA_OPT__(,)`-based dispatch.
3. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now
   that the EIGHTH + NINTH axes are fully closed on BOTH
   sides.

The current matrix on the BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis now reaches SIX
pad characters: ` ` (NONE = no fill) → ` ` (Sep 28 / Sep 29)
→ `-` (Oct 3 / Oct 4) → `#` (Oct 9 / Oct 11) → `~` (Oct 12 /
today).  A future NTH-axis pad character (e.g. `_` or `*`)
would close the next pad layer on either side.