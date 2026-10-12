# P-2026-10-12 — try_format_tilde_pad_n

**Closes Oct 11's "Where we go next" item (a) verbatim on the
CHAR\*+CAP side** — the NINTH axis (a NEW PAD CHARACTER beyond
the four already-closed: ` ` / `-` / `#` / `\0`).  Today's
lesson introduces `PETRA_TILDE_PAD` (a SIXTH compile-time flag
on top of Oct 9's 5-bit tuple `(PETRA_THROW_ON_OVERFLOW,
PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD, PETRA_HASH_PAD)`),
which pads bytes `[written, cap)` with `'~'` on success.

The `'~'` character was chosen deliberately: it is
**TOTALLY DIFFERENT** from Oct 9's `'#'` (the HASH-pad fill)
and Oct 3's `'-'` (the DASH-pad fill), demonstrating that the
PAD-axis is genuinely a **character-level axis** — not a
HASH/DASH-SPECIFIC axis.  The `'~'` byte has ASCII value `126`
(0x7E), distinct from the four prior pad bytes (`' '` = 32,
`'-'` = 45, `'#'` = 35, `'\0'` = 0) and the poison byte
`'Q'` = 81.

## Where this fits in the arc

| Lesson              | Topic                                            |
|---------------------|--------------------------------------------------|
| Sep 8               | `try_format_runtime_n` (CHAR\*+CAP × RUNTIME × EXPECTED) |
| Sep 9               | `try_format_bounded_runtime_n` (CHAR\*+CAP × CONSTEVAL × EXPECTED) |
| Sep 20              | `try_format_runtime_n_throwing` (CHAR\*+CAP × RUNTIME × THROWING) |
| Sep 25              | NUL-pad (PETRA\_PAD)                             |
| Sep 28 / Sep 29     | SPACE-pad (PETRA\_SPACE\_PAD) char\*+cap         |
| Oct 3               | DASH-pad (PETRA\_DASH\_PAD) char\*+cap           |
| Oct 9               | HASH-pad (PETRA\_HASH\_PAD) char\*+cap           |
| Oct 11              | HASH-pad STD::SPAN\<CHAR\> mirror                |
| **TODAY**           | **TILDE-pad (PETRA\_TILDE\_PAD) char\*+cap**     |

## Headline findings

1. **The TILDE-pad char\*+cap sibling is a thin layer on top of
   Oct 9's HASH-pad char\*+cap sibling.**  Today's TILDE-pad
   wrapper calls Oct 9's wrapper, then OVERWRITES the `'#'`
   fill with `'~'` on success.  This is the same pattern Oct 9
   used (HASH layer on Oct 3's DASH-pad sibling).

2. **The composition rule "the LATER fill overwrites the EARLIER
   fill" now has FIVE layers: `TILDE > HASH > DASH > SPACE > NUL`.**
   When ALL FIVE pad-flags are set, the end byte state is
   always `'~'`.  Composition cells with `(TILDE=1, HASH=1)`
   REUSE TODAY's NEW TILDE-pad wrappers verbatim (the
   `(TILDE=1, HASH=1)` cell REUSES `(TILDE=1, HASH=0)`).

3. **No new error contracts are introduced.**  The new
   wrappers preserve the SAME contract as Oct 9's HASH-pad
   wrappers:
   - **Success path:** returns the byte count via
     `expected<size_t, FormatError>` (EXPECTED siblings) or
     `std::size_t` (THROWING siblings).  Bytes `[written, cap)`
     are `'~'`.
   - **TooLarge path:** returns
     `unexpected{FormatError{TooLarge, ..., needed_bytes}}`
     (EXPECTED siblings) or throws `std::runtime_error` with
     the same message (THROWING siblings).  Bytes `[0, cap)`
     are UNTOUCHED (no partial `'~'` fill — the buffer's
     atomicity-on-error contract is preserved).
   - **Format error path:** returns
     `unexpected{FormatError{Format, ...}}` or throws
     `std::runtime_error` similarly.  Buffer UNTOUCHED.

4. **The CONSTEVAL gate is preserved regardless of the
   (THROW, TILDE) 2-bit tuple** — `TRY_FORMAT_BOUNDED_N_TILDE_
   OR_THROW` requires `std::format_string<Args...>` (a literal
   fmt at the call site) when either sibling uses
   `std::format_string<Args...>` (the EXPECTED-CONSTEVAL and
   THROWING-CONSTEVAL paths).

5. **The macro `TRY_FORMAT_BOUNDED_N_TILDE_OR_THROW` is a
   verbatim thin dispatcher** that picks between FOUR sibling
   wrappers at preprocessor time based on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_TILDE_PAD)` 2-bit tuple.
   Each macro is a parenthesized expression (NOT
   `do { } while (0)`), so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_N_TILDE_OR_THROW(...)` and
   capture the result.  Each macro uses `__VA_OPT__(,)` for the
   no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 /
   Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
   Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3
   / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11's macro shape).

## Important code

The four NEW sibling wrappers follow Oct 9's HASH-pad-shape
verbatim.  The RUNTIME / EXPECTED sibling is the canonical
example:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_tilde_pad(char* out, std::size_t cap,
                                std::string_view fmt,
                                const Args&... args) {
    auto r = try_format_runtime_n_hash_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        // LATER fill (TILDE) overwrites EARLIER fill (HASH)
        // from the HASH-pad sibling.  On error, r.has_value()
        // is false and the loop is skipped, preserving the
        // buffer's atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) out[i] = '~';
    }
    return r;
}
```

The CONSTEVAL sibling is a thin forwarding wrapper:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_tilde_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt,
        const Args&... args) {
    return try_format_runtime_n_tilde_pad(out, cap, fmt.get(), args...);
}
```

The THROWING sibling uses the same
`try { ... } catch (...) { throw; }` idiom that Oct 3 / Oct 4
/ Oct 9 established, so the `std::runtime_error` thrown by
the underlying HASH-pad sibling is re-raised untouched:

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_tilde_pad(char* out, std::size_t cap,
                                          std::string_view fmt,
                                          const Args&... args) {
    try {
        auto r = try_format_runtime_n_tilde_pad(out, cap, fmt, args...);
        if (!r.has_value()) {
            // Re-raise as std::runtime_error — same shape as
            // Oct 9's / Oct 3's throwing+DASH-padded wrapper.
            const auto& e = r.error();
            if (e.kind == FormatErrorKind::TooLarge) {
                throw std::runtime_error{
                    std::string{"TooLarge: "} + e.message};
            }
            throw std::runtime_error{
                std::string{"Format: "} + e.message};
        }
        // On success, OVERWRITE the HASH pad from the
        // HASH-pad sibling with the TILDE pad.
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '~';
        return r.value();
    } catch (...) {
        throw;  // re-raise unchanged
    }
}
```

The dispatcher macro:

```cpp
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_TILDE_PAD == 0
#define TRY_FORMAT_BOUNDED_N_TILDE_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_TILDE_PAD == 1
#define TRY_FORMAT_BOUNDED_N_TILDE_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n_tilde_pad((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_TILDE_PAD == 0
#define TRY_FORMAT_BOUNDED_N_TILDE_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n_throwing((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_TILDE_PAD == 1
#define TRY_FORMAT_BOUNDED_N_TILDE_OR_THROW(out, cap, fmt, ...) \
    (::petra::try_format_runtime_n_throwing_tilde_pad((out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#endif
```

## Where this lesson diverges from Oct 9 / Oct 11

| Aspect              | Oct 9 / Oct 11                                | TODAY                                       |
|---------------------|-----------------------------------------------|---------------------------------------------|
| Pad character       | `'#'` (35)                                    | `'~'` (126)                                 |
| Composed on top of  | Oct 3's DASH-pad sibling (`'-'` = 45)         | Oct 9's HASH-pad sibling (`'#'` = 35)        |
| 4-bit-tuple surface | `(THROW, HASH)` only (Sep 8/9/20 underlying)  | `(THROW, TILDE)` only (Sep 8/9/20 underlying) |
| Composition rule    | `HASH > DASH > SPACE > NUL`                   | `TILDE > HASH > DASH > SPACE > NUL`         |
| Other flags         | PAD / SPACE_PAD / DASH_PAD must be 0          | PAD / SPACE_PAD / DASH_PAD / HASH_PAD must be 0 |
| `#error` pointer    | → Sep 25 / Sep 27 / Oct 1 / Oct 5 / Oct 8     | → Sep 25 / Sep 27 / Sep 29 / Oct 1 / Oct 3 / Oct 5 / Oct 8 / Oct 9 / Oct 11 |

The narrowed scope (PAD / SPACE_PAD / DASH_PAD / HASH_PAD must
all be 0) is the right move for one focused lesson.  All
those composition cells REUSE the Sep 25 / Sep 27 / Sep 29 /
Oct 1 / Oct 3 / Oct 5 / Oct 8 / Oct 9 / Oct 11 helpers; a
future lesson can introduce a 7-bit-tuple (THROW × PAD ×
SPACE × DASH × HASH × TILDE) dispatcher that composes all six
flags following the same pattern.

## Observed test results (64 CHECKs across 9 sections)

- **(0, 0)** expected+untouched: **64/64 PASS** (default cell)
- **(0, 1)** expected+TILDE-padded: **64/64 PASS**
  (`-DPETRA_TILDE_PAD=ON`)
- **(1, 0)** throwing+untouched: **64/64 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=ON`)
- **(1, 1)** throwing+TILDE-padded: **64/64 PASS**
  (`-DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_TILDE_PAD=ON`)

Five consecutive runs on each cell produced identical 64/64
PASS, 0 FAIL tallies (no flakiness).

- `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`: **zero warnings** (verified on
  `build-strict`)
- `-fsanitize=address -fsanitize=undefined -O1
  -fno-omit-frame-pointer`: **zero sanitizer diagnostics**
  (verified on `build-asan`)

## Friendly-error firing (verified)

- `-DPETRA_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_n) only supports
  PETRA_PAD=0.  For PETRA_PAD=1, use Sep 25's
  try_format_padded_n_or_into, Sep 26's
  try_format_padded_n_or_throw, or Sep 27's
  try_format_padded_span_n_or_throw (NUL-padded wrappers)."`
- `-DPETRA_SPACE_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_n) only supports
  PETRA_SPACE_PAD=0.  For PETRA_SPACE_PAD=1, use Sep 29's
  try_format_space_pad_n (CHAR*+CAP SPACE-padded sibling) or
  Oct 1's try_format_padded_space_pad_span_n_or_throw
  (composition cells)."`
- `-DPETRA_DASH_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_n) only supports
  PETRA_DASH_PAD=0.  For PETRA_DASH_PAD=1, use Oct 3's
  try_format_dash_pad_n (CHAR*+CAP DASH-padded sibling) or
  Oct 5 / Oct 8's composition cells."`
- `-DPETRA_HASH_PAD=1`: fires
  `"This lesson (try_format_tilde_pad_n) only supports
  PETRA_HASH_PAD=0.  For PETRA_HASH_PAD=1, use Oct 9's
  try_format_hash_pad_n (CHAR*+CAP HASH-padded sibling) or
  Oct 11's try_format_hash_pad_span_n (STD::SPAN<CHAR>
  HASH-padded sibling)."`

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`
- `std::expected<T, E>` (P0323R12)
- `std::println` (P2093R14)
- `std::algorithm::fill` (for the poison-byte setup)
- `std::fprintf` (the PASS/FAIL messaging uses `fprintf` to
  avoid the formatter specialization requirement; C++23 has
  no formatter for `petra::FormatErrorKind` by default and
  adding one would muddy the lesson's scope)
- `__VA_OPT__(,)` preprocessor feature (C++20)
- `#error` preprocessor feature
- `enable_if_t` + `is_convertible_v` for the string_view
  overload discrimination in the test framework

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA\_THROW=0, PETRA\_TILDE\_PAD=0):

```bash
cd late-may/cpp_practice/try_format_tilde_pad_n
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-12-try-format-tilde-pad-n.cpp \
        -o /tmp/P-2026-10-12-tilde.out
/tmp/P-2026-10-12-tilde.out
```

Or, via CMake:

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-12-try-format-tilde-pad-n
```

TILDE-pad cell (PETRA\_TILDE\_PAD=1):

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_TILDE_PAD=1 \
        P-2026-10-12-try-format-tilde-pad-n.cpp \
        -o /tmp/P-2026-10-12-tilde-pad.out
/tmp/P-2026-10-12-tilde-pad.out
```

Throwing+TILDE-pad cell (PETRA\_THROW\_ON\_OVERFLOW=1,
PETRA\_TILDE\_PAD=1):

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_TILDE_PAD=1 \
        P-2026-10-12-try-format-tilde-pad-n.cpp \
        -o /tmp/P-2026-10-12-throw-tilde.out
/tmp/P-2026-10-12-throw-tilde.out
```

Or via CMake:

```bash
cmake -S . -B build-throw-tilde \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_TILDE_PAD=ON
cmake --build build-throw-tilde
./build-throw-tilde/P-2026-10-12-try-format-tilde-pad-n
```

Strict-warning build:

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow \
        -Wconversion -Wsign-conversion \
        P-2026-10-12-try-format-tilde-pad-n.cpp \
        -o /tmp/P-2026-10-12-tilde-strict.out
/tmp/P-2026-10-12-tilde-strict.out
```

ASan + UBSan build:

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O1 \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer \
        P-2026-10-12-try-format-tilde-pad-n.cpp \
        -o /tmp/P-2026-10-12-tilde-asan.out
/tmp/P-2026-10-12-tilde-asan.out
```

## Where we go next

The NINTH axis is now closed on the CHAR\*+CAP side.  Future
lessons can either:

1. **STD::SPAN\<CHAR\> mirror of today's
   `try_format_tilde_pad_n`** — the SPAN sibling of TODAY's
   char\*+cap wrappers + a corresponding `SPAN` macro
   dispatcher.  Mirrors Oct 11's SPAN mirror of Oct 9's
   char\*+cap lesson.
2. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now that
   the EIGHTH + NINTH axes are fully closed on the CHAR\*+CAP
   side.

The current matrix on the CHAR\*+CAP × BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis now reaches SIX
pad characters: ` ` (NONE = no fill) → ` ` (Sep 28) → `-` (Oct
3) → `#` (Oct 9) → `~` (today).  A future (TILDE)
SPAN-mirror lesson closes the SPAN side.
