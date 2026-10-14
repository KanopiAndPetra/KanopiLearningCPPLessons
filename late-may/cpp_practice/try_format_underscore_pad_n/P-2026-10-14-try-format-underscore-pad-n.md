# P-2026-10-14 — try_format_underscore_pad_n

**Closes Oct 13's "Where we go next" item (1) verbatim — the
CHAR\*+CAP TENTH-axis pad layer, picking a new pad character
on the CHAR\*+CAP × BOUNDED-BUFFER axis.**

Today's lesson introduces `PETRA_UNDERSCORE_PAD` (a NEW
SEVENTH compile-time flag on top of Oct 13's 6-bit tuple
`(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
PETRA_DASH_PAD, PETRA_HASH_PAD, PETRA_TILDE_PAD)`) on the
CHAR\*+CAP × BOUNDED-BUFFER axis.  `PETRA_UNDERSCORE_PAD=1`
selects the UNDERSCORE-padded char\*+cap sibling, which fills
bytes `[written, cap)` with `'_'` (ASCII 95) on success.

The `'_'` character is **TOTALLY DIFFERENT** from Oct 12's
`'~'` (126), Oct 9's `'#'` (35), Oct 3's `'-'` (45), Sep 28's
`' '` (32), and Sep 25's `'\0'` (0).  The `'_'` byte has ASCII
value `95` (0x5F), distinct from the five prior pad bytes and
the poison byte `'Q'` (0x51).

After today, the CHAR\*+CAP × BOUNDED-BUFFER matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_UNDERSCORE_PAD)` 2-bit tuple
is COMPLETE for ALL FOUR cells.  The TENTH axis is closed on
the CHAR\*+CAP side (today); the STD::SPAN\<CHAR\> side
remains open (a future lesson can mirror today's pattern
exactly per Oct 14's "Where we go next" item (1)).

## Where this fits in the arc

| Lesson              | Topic                                            |
|---------------------|--------------------------------------------------|
| Sep 8               | `try_format_runtime_n` (CHAR\*+CAP × RUNTIME × EXPECTED) |
| Sep 9               | `try_format_bounded_runtime_n` (CHAR\*+CAP × CONSTEVAL × EXPECTED) |
| Sep 25              | NUL-pad (PETRA\_PAD) char\*+cap                 |
| Sep 28              | SPACE-pad (PETRA\_SPACE\_PAD) char\*+cap         |
| Oct 3               | DASH-pad (PETRA\_DASH\_PAD) char\*+cap           |
| Oct 9               | HASH-pad (PETRA\_HASH\_PAD) char\*+cap           |
| Oct 12              | TILDE-pad (PETRA\_TILDE\_PAD) char\*+cap         |
| Oct 13              | TILDE-pad (PETRA\_TILDE\_PAD) span\<char\>       |
| **TODAY**           | **UNDERSCORE-pad (PETRA\_UNDERSCORE\_PAD) char\*+cap** |

## Headline findings

1. **The UNDERSCORE-pad char\*+cap sibling is a thin layer on
   top of Oct 12's TILDE-pad char\*+cap sibling.**  Today's
   UNDERSCORE-pad wrapper calls Oct 12's wrapper, then
   OVERWRITES the `'~'` fill with `'_'` on success.  This is
   the same pattern Oct 12 used (TILDE layer on Oct 9's HASH-pad
   char\*+cap sibling) and Oct 9 used (HASH layer on Oct 3's
   DASH-pad sibling).

2. **The composition rule "the LATER fill overwrites the
   EARLIER fill" now has SIX layers:
   `UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`.**  When
   ALL SIX pad-flags are set, the end byte state is always
   `'_'`.  Composition cells with `(UNDERSCORE=1, TILDE=*)`
   REUSE TODAY's NEW UNDERSCORE-pad wrappers verbatim (the
   `(UNDERSCORE=1, TILDE=*)` cell and `(UNDERSCORE=1, TILDE=0)`
   cell share wrappers because UNDERSCORE overwrites TILDE).

3. **No new error contracts are introduced.**  The new
   wrappers preserve the SAME contract as Oct 12's TILDE-pad
   char\*+cap wrappers:
   - **Success path:** returns the byte count via
     `expected<size_t, FormatError>` (EXPECTED siblings) or
     `std::size_t` (THROWING siblings).  Bytes
     `[written, cap)` are `'_'`.
   - **TooLarge path:** returns
     `unexpected{FormatError{TooLarge, ..., needed_bytes}}`
     (EXPECTED siblings) or throws `std::runtime_error` with
     the same message (THROWING siblings).  Bytes
     `[0, cap)` are UNTOUCHED (no partial `'_'` fill — the
     buffer's atomicity-on-error contract is preserved).
   - **Format error path:** returns
     `unexpected{FormatError{Format, ...}}` or throws
     `std::runtime_error` similarly.  Buffer UNTOUCHED.

4. **The CONSTEVAL gate is preserved regardless of the
   `(THROW, UNDERSCORE)` 2-bit tuple** —
   `TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW` uses a runtime
   sibling surface.  The CONSTEVAL counterparts
   (`try_format_bounded_runtime_n_underscore_pad` and
   `try_format_bounded_runtime_n_throwing_underscore_pad`)
   are provided as separately-named wrappers for callers who
   need the consteval-checked `std::format_string<Args...>`
   gate.

5. **The macro `TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW`
   (RUNTIME surface) is a verbatim thin dispatcher** that
   picks between FOUR sibling wrappers at preprocessor time
   based on the `(PETRA_THROW_ON_OVERFLOW,
   PETRA_UNDERSCORE_PAD)` 2-bit tuple.  The macro is a
   parenthesized expression (NOT `do { } while (0)`), so the
   caller can write
   `auto n = TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW(...)`
   and capture the result.  The macro uses `__VA_OPT__(,)` for
   the no-args case (mirrors Sep 13 / Sep 14 / Sep 15 /
   Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 /
   Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 /
   Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 /
   Oct 12 / Oct 13's macro shape).

6. **The CHAR\*+CAP × BOUNDED-BUFFER matrix on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_UNDERSCORE_PAD)` 2-bit
   tuple is now COMPLETE for all four cells.**  After today,
   `PETRA_UNDERSCORE_PAD` joins the ranks of completed pad
   characters on the CHAR\*+CAP × BOUNDED-BUFFER axis: NUL
   (Sep 25 / Sep 27) → SPACE (Sep 28 / Sep 29) → DASH (Oct 3 /
   Oct 4) → HASH (Oct 9 / Oct 11) → TILDE (Oct 12 / Oct 13)
   → UNDERSCORE (today).

## Important code

The four NEW sibling wrappers follow Oct 12's TILDE-pad-shape
verbatim.  The RUNTIME / EXPECTED char\*+cap sibling is the
canonical example:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_underscore_pad(char* out, std::size_t cap,
                                    std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_tilde_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        // LATER fill (UNDERSCORE) overwrites EARLIER fill
        // (TILDE) from the TILDE-pad sibling.  On error,
        // r.has_value() is false and the loop is skipped,
        // preserving the buffer's atomicity-on-error contract.
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
try_format_bounded_runtime_n_underscore_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt,
        const Args&... args) {
    return try_format_runtime_n_underscore_pad(
        out, cap, fmt.get(), args...);
}
```

The THROWING sibling uses the same idiom Oct 12 established,
so the `std::runtime_error` thrown by the underlying
TILDE-pad char\*+cap sibling is re-raised untouched:

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_underscore_pad(
        char* out, std::size_t cap, std::string_view fmt,
        const Args&... args) {
    try {
        auto r = try_format_runtime_n_underscore_pad(
            out, cap, fmt, args...);
        if (!r.has_value()) {
            const auto& e = r.error();
            if (e.kind == FormatErrorKind::TooLarge) {
                throw std::runtime_error{
                    std::string{"TooLarge: "} + e.message};
            }
            throw std::runtime_error{
                std::string{"Format: "} + e.message};
        }
        // On success, OVERWRITE the TILDE pad from the
        // TILDE-pad sibling with the UNDERSCORE pad.
        for (std::size_t i = r.value(); i < cap; ++i) {
            out[i] = '_';
        }
        return r.value();
    } catch (...) {
        throw;  // re-raise unchanged
    }
}
```

The RUNTIME-surface dispatcher macro:

```cpp
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_UNDERSCORE_PAD == 0
#define TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW(out, cap, fmt, ...)     \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                   \
                                   __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_UNDERSCORE_PAD == 1
#define TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW(out, cap, fmt, ...)     \
    (::petra::try_format_runtime_n_underscore_pad((out), (cap), (fmt)     \
                                                  __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_UNDERSCORE_PAD == 0
#define TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW(out, cap, fmt, ...)     \
    (::petra::try_format_runtime_n_throwing((out), (cap), (fmt)          \
                                            __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_UNDERSCORE_PAD == 1
#define TRY_FORMAT_BOUNDED_N_UNDERSCORE_OR_THROW(out, cap, fmt, ...)     \
    (::petra::try_format_runtime_n_throwing_underscore_pad((out), (cap), (fmt) \
                                                          __VA_OPT__(,) __VA_ARGS__))
#endif
```

## Where this lesson diverges from Oct 12 / Oct 13

| Aspect              | Oct 12 / Oct 13                                | TODAY                                       |
|---------------------|------------------------------------------------|---------------------------------------------|
| Pad character       | `'~'` (126)                                    | `'_'` (95)                                  |
| Composed on top of  | Oct 9's HASH-pad char\*+cap / span\<char\> sibling (`'#'` = 35) | Oct 12's TILDE-pad char\*+cap sibling (`'~'` = 126) |
| 2-bit-tuple surface | `(THROW, TILDE)` only                          | `(THROW, UNDERSCORE)` only                  |
| Composition rule    | `TILDE > HASH > DASH > SPACE > NUL`            | `UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL` |
| Other flags         | PAD / SPACE\_PAD / DASH\_PAD / HASH\_PAD must be 0 | PAD / SPACE\_PAD / DASH\_PAD / HASH\_PAD / TILDE\_PAD must be 0 |
| `#error` pointer    | → Sep 25 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 | → Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 / Oct 12 / Oct 13 |
| Axis number         | SIXTH pad character on CHAR\*+CAP / span\<char\> | SEVENTH pad character on CHAR\*+CAP only   |

The narrower scope (PAD / SPACE\_PAD / DASH\_PAD / HASH\_PAD /
TILDE\_PAD must all be 0) is the right move for one focused
lesson.  All those composition cells REUSE the Sep 25 / Sep 26
/ Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 /
Oct 5 / Oct 8 / Oct 9 / Oct 11 / Oct 12 / Oct 13 helpers; a
future lesson can introduce an 8-bit-tuple dispatcher
(THROW × PAD × SPACE × DASH × HASH × TILDE × UNDERSCORE) that
composes all seven flags following the same pattern.

## Observed test results (70 CHECKs across 9 sections)

- **(0, 0)** expected+untouched: **70/70 PASS** (default cell)
- **(0, 1)** expected+UNDERSCORE-padded: **70/70 PASS**
  (`-DPETRA_UNDERSCORE_PAD=1`)
- **(1, 0)** throwing+untouched: **70/70 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=1`)
- **(1, 1)** throwing+UNDERSCORE-padded: **70/70 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_UNDERSCORE_PAD=1`)

- `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`: **zero warnings** (verified on
  `build-strict` for all four `(THROW, UNDERSCORE)` cells)
- `-fsanitize=address -fsanitize=undefined -O1
  -fno-omit-frame-pointer`: **zero sanitizer diagnostics**
  (verified on the `(1, 1)` cell under ASan/UBSan)

## Friendly-error firing (verified)

- `-DPETRA_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_n) only supports
  PETRA_PAD=0.  For PETRA_PAD=1, use Sep 25's
  try_format_padded_n_or_into, Sep 26's
  try_format_padded_n_or_throw, or Sep 27's
  try_format_padded_span_n_or_throw (NUL-padded
  wrappers)."`
- `-DPETRA_SPACE_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_n) only supports
  PETRA_SPACE_PAD=0.  For PETRA_SPACE_PAD=1, use Sep 29's
  try_format_space_pad_n (CHAR*+CAP SPACE-padded sibling)
  or Oct 1's try_format_padded_space_pad_span_n_or_throw
  (composition cells)."`
- `-DPETRA_DASH_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_n) only supports
  PETRA_DASH_PAD=0.  For PETRA_DASH_PAD=1, use Oct 3's
  try_format_dash_pad_n (CHAR*+CAP DASH-padded sibling) or
  Oct 5 / Oct 8's composition cells."`
- `-DPETRA_HASH_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_n) only supports
  PETRA_HASH_PAD=0.  For PETRA_HASH_PAD=1, use Oct 9's
  try_format_hash_pad_n (CHAR*+CAP HASH-padded sibling) or
  Oct 11's try_format_hash_pad_span_n (STD::SPAN<CHAR>
  HASH-padded sibling)."`
- `-DPETRA_TILDE_PAD=1`: fires
  `"This lesson (try_format_underscore_pad_n) only supports
  PETRA_TILDE_PAD=0.  For PETRA_TILDE_PAD=1, use Oct 12's
  try_format_tilde_pad_n (CHAR*+CAP TILDE-padded sibling)
  or Oct 13's try_format_tilde_pad_span_n (STD::SPAN<CHAR>
  TILDE-padded sibling).  Note: today's UNDERSCORE-pad
  wrapper REUSES Oct 12's TILDE-pad wrapper internally — if
  you set PETRA_TILDE_PAD=1 explicitly, the dispatcher
  #errors to avoid double-pad ambiguity."`
- `-DPETRA_UNDERSCORE_PAD=2`: fires
  `"PETRA_UNDERSCORE_PAD must be 0 (untouched) or 1
  (UNDERSCORE-padded sibling)"`
- `-DPETRA_THROW_ON_OVERFLOW=2`: fires
  `"PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1
  (throwing)"`

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`
- `std::expected<T, E>` (P0323R12)
- `std::println` (P2093R14)
- `std::is_same_v<T1, T2>` for decltype pinning of all four
  `(THROW, UNDERSCORE)` cells
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

Default build (PETRA\_THROW=0, PETRA\_UNDERSCORE\_PAD=0):

```bash
cd late-may/cpp_practice/try_format_underscore_pad_n
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-14-try-format-underscore-pad-n.cpp \
        -o /tmp/P-2026-10-14-try-format-underscore-pad-n
/tmp/P-2026-10-14-try-format-underscore-pad-n
```

Or, via CMake:

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-14-try-format-underscore-pad-n
```

UNDERSCORE-pad cell (PETRA\_UNDERSCORE\_PAD=1):

```bash
cmake -S . -B build-understand -DPETRA_UNDERSCORE_PAD=ON
cmake --build build-understand
./build-understand/P-2026-10-14-try-format-underscore-pad-n
```

Throwing+UNDERSCORE-pad cell (PETRA\_THROW\_ON\_OVERFLOW=1,
PETRA\_UNDERSCORE\_PAD=1):

```bash
cmake -S . -B build-throw-underscore \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_UNDERSCORE_PAD=ON
cmake --build build-throw-underscore
./build-throw-underscore/P-2026-10-14-try-format-underscore-pad-n
```

Strict-warning build:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-14-try-format-underscore-pad-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-14-try-format-underscore-pad-n
```

## Where we go next

The TENTH axis (PETRA\_UNDERSCORE\_PAD) is now closed on the
CHAR\*+CAP side (today).  The matrix on the
`(PETRA\_THROW\_ON\_OVERFLOW × PETRA\_UNDERSCORE\_PAD)` 2-bit
tuple is COMPLETE for ALL FOUR cells on the CHAR\*+CAP side.
Future lessons can either:

1. **STD::SPAN\<CHAR\> MIRROR of today's lesson
   (try\_format\_underscore\_pad\_span\_n)** — mirror today's
   pattern exactly on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER
   axis (the `(THROW, UNDERSCORE)` 2-bit-tuple sibling
   wrappers + the CONSTEVAL+RUNTIME dispatcher macros).  Would
   close Oct 13's "Where we go next" item (1) verbatim on the
   STD::SPAN\<CHAR\> side; mirrors today's pattern exactly.
2. **A unified 8-bit-tuple dispatcher
   `(THROW × PAD × SPACE × DASH × HASH × TILDE × UNDERSCORE
   × NEW)`** — one macro covers all composition cells across
   all seven pad flags, picking the right wrapper at
   preprocessor time via `__VA_OPT__(,)`-based dispatch.
3. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now that
   the EIGHTH + TENTH axes are fully closed on BOTH sides.
4. **NTH axis (a different pad character on the
   `char\*+cap` / `std::span<char>` axis)** — e.g.
   `PETRA_STAR_PAD` (`'*'`, ASCII 42) or
   `PETRA_AT_PAD` (`'@'`, ASCII 64).  Each such lesson
   would mirror Oct 12 / Oct 13 / today's pattern exactly.

The current matrix on the BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis now reaches SEVEN
pad characters on the CHAR\*+CAP side: NONE (no fill) → NUL
(Sep 25 / Sep 27) → SPACE (Sep 28 / Sep 29) → DASH (Oct 3 /
Oct 4) → HASH (Oct 9 / Oct 11) → TILDE (Oct 12 / Oct 13) →
UNDERSCORE (today).  The STD::SPAN\<CHAR\> side mirrors on
SIX (NUL, SPACE, DASH, HASH, TILDE; UNDERSCORE span\<char\>
still open).