# P-2026-10-16 — try_format_star_pad_span_n

**Closes Oct 15's "Where we go next" item (1) verbatim — the
STD::SPAN\<CHAR\>-side EIGHTH pad-character axis (`PETRA_STAR_PAD`).**

Today's lesson introduces `PETRA_STAR_PAD` (a NEW EIGHTH
compile-time flag on top of Oct 15's 7-bit tuple) on the
STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.  `PETRA_STAR_PAD=1`
selects the STAR-padded span\<char\> sibling, which fills
bytes `[written, span.size())` with `'*'` (ASCII 42) on success.

The `'*'` character is **TOTALLY DIFFERENT** from Oct 15's
`'_'` (95), Oct 13's `'~'` (126), Oct 11's `'#'` (35), Oct 4's
`'-'` (45), Sep 28's `' '` (32), and Sep 27's `'\0'` (0).  The
`'*'` byte has ASCII value `42` (0x2A), distinct from the
six prior pad bytes and the poison byte `'Q'` (0x51).

After today, the STD::SPAN\<CHAR\> × BOUNDED-BUFFER matrix on
the `(PETRA_THROW_ON_OVERFLOW × PETRA_STAR_PAD)` 2-bit tuple
is COMPLETE for ALL FOUR cells.

## Where this fits in the arc

| Lesson | Topic |
|--------|-------|
| Sep 10 | `try_format_bounded_runtime_n_span` (STD::SPAN\<CHAR\> × CONSTEVAL × EXPECTED) |
| Sep 19 | `try_format_runtime_n(span, ...)` (STD::SPAN\<CHAR\> × RUNTIME × EXPECTED) |
| Sep 21 | `try_format_runtime_n_throwing(span, ...)` (STD::SPAN\<CHAR\> × RUNTIME × THROWING) |
| Sep 27 | NUL-pad (PETRA_PAD) span\<char\> |
| Sep 28 | SPACE-pad (PETRA_SPACE_PAD) span\<char\> |
| Oct 1  | NUL × SPACE composition on span\<char\> |
| Oct 4  | DASH-pad (PETRA_DASH_PAD) span\<char\> |
| Oct 5 / Oct 8 | PAD × DASH composition on span\<char\> |
| Oct 11 | HASH-pad (PETRA_HASH_PAD) span\<char\> |
| Oct 13 | TILDE-pad (PETRA_TILDE_PAD) span\<char\> |
| Oct 14 | UNDERSCORE-pad (PETRA_UNDERSCORE_PAD) char\*+cap |
| Oct 15 | UNDERSCORE-pad (PETRA_UNDERSCORE_PAD) span\<char\> |
| **TODAY** | **STAR-pad (PETRA_STAR_PAD) span\<char\>** |

## Headline findings

1. **The STAR-pad span\<char\> sibling is a thin layer on top
   of Oct 15's UNDERSCORE-pad span\<char\> sibling.**  Today's
   STAR-pad wrapper calls Oct 15's wrapper, then OVERWRITES
   the `'_'` fill with `'*'` on success.  This is the same
   pattern Oct 15 used (UNDERSCORE layer on Oct 13's TILDE-pad
   span\<char\> sibling), Oct 13 used (TILDE layer on Oct 11's
   HASH-pad span\<char\> sibling), and Oct 11 used (HASH layer
   on Oct 4's DASH-pad span\<char\> sibling).

2. **The composition rule "the LATER fill overwrites the
   EARLIER fill" now has SEVEN layers:
   `STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`.**
   When ALL SEVEN pad-flags are set, the end byte state is
   always `'*'`.  Composition cells with `(STAR=1,
   UNDERSCORE=*)` REUSE TODAY's NEW STAR-pad wrappers
   verbatim (the `(STAR=1, UNDERSCORE=*)` cell and
   `(STAR=1, UNDERSCORE=0)` cell share wrappers because
   STAR overwrites UNDERSCORE, which overwrites TILDE which
   overwrites HASH which overwrites DASH which overwrites
   SPACE).

3. **No new error contracts are introduced.**  The new
   wrappers preserve the SAME contract as Oct 15's
   UNDERSCORE-pad span\<char\> wrappers:
   - **Success path:** returns the byte count via
     `expected<size_t, FormatError>` (EXPECTED siblings) or
     `std::size_t` (THROWING siblings).  Bytes
     `[written, span.size())` are `'*'`.
   - **TooLarge path:** returns
     `unexpected{FormatError{TooLarge, ..., needed_bytes}}`
     (EXPECTED siblings) or throws `std::runtime_error` with
     the same message (THROWING siblings).  Bytes
     `[0, span.size())` are UNTOUCHED (no partial `'*'`
     fill — the buffer's atomicity-on-error contract is
     preserved).
   - **Format error path:** returns
     `unexpected{FormatError{Format, ...}}` or throws
     `std::runtime_error` similarly.  Buffer UNTOUCHED.
   - **Sub-span no-bleed:** bytes outside the span's
     writable region are UNTOUCHED (a 6-byte sub-span at
     offset 5 of a 16-byte buffer leaves bytes `[0,5)` and
     `[11,16)` as poison `'Q'` — pinned by Section 2b).

4. **The CONSTEVAL gate is preserved regardless of the
   `(THROW, STAR)` 2-bit tuple** —
   `TRY_FORMAT_BOUNDED_SPAN_N_STAR_OR_THROW` requires
   `std::format_string<Args...>` (a literal fmt at the call
   site).  Both the CONSTEVAL-surface and the RUNTIME-surface
   dispatcher macros are provided.

5. **The macros `TRY_FORMAT_BOUNDED_SPAN_N_STAR_OR_THROW`
   (CONSTEVAL surface) and
   `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW`
   (RUNTIME surface) are verbatim thin dispatchers** that
   pick between FOUR sibling wrappers at preprocessor time
   based on the `(PETRA_THROW_ON_OVERFLOW,
   PETRA_STAR_PAD)` 2-bit tuple.  Each macro is a
   parenthesized expression (NOT `do { } while (0)`), so the
   caller can write
   `auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW(...)`
   and capture the result.  Each macro uses `__VA_OPT__(,)`
   for the no-args case (mirrors Sep 13 / Sep 14 / Sep 15 /
   Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep
   25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 /
   Oct 3 / Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 / Oct 13 /
   Oct 14 / Oct 15's macro shape).

6. **The STD::SPAN\<CHAR\> × BOUNDED-BUFFER matrix on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_STAR_PAD)` 2-bit tuple
   is now COMPLETE for all four cells.**  Both axes share
   the SAME wrapper(s) per cell:
   `(0,0)` → Sep 10/19/21 underlying untouched sibling;
   `(0,1)` → TODAY's NEW STAR-pad expected sibling;
   `(1,0)` → Sep 21 underlying throwing untouched sibling;
   `(1,1)` → TODAY's NEW STAR-pad throwing sibling.

## Important code

The four NEW sibling wrappers follow Oct 15's UNDERSCORE-pad
shape verbatim.  The RUNTIME / EXPECTED span\<char\> sibling
is the canonical example:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_star_pad_span(std::span<char> out,
                                   std::string_view fmt,
                                   const Args&... args) {
    auto r = try_format_runtime_n_underscore_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        // LATER fill (STAR) overwrites EARLIER fill
        // (UNDERSCORE) from the UNDERSCORE-pad span<char>
        // sibling.  On error, r.has_value() is false and
        // the loop is skipped, preserving the buffer's
        // atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '*';
        }
    }
    return r;
}
```

The CONSTEVAL sibling is a thin forwarding wrapper:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_star_pad_span(
        std::span<char> out, std::format_string<Args...> fmt,
        const Args&... args) {
    auto r = try_format_bounded_runtime_n_underscore_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '*';
        }
    }
    return r;
}
```

The THROWING sibling uses the same idiom Oct 15 established,
so the `std::runtime_error` thrown by the underlying
UNDERSCORE-pad span\<char\> sibling is re-raised untouched:

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_star_pad_span(
        std::span<char> out, std::string_view fmt,
        const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_underscore_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '*';
        }
        return n;
    } catch (...) {
        throw;  // re-raise unchanged
    }
}
```

The CONSTEVAL + RUNTIME dispatcher macros:

```cpp
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_STAR_PAD == 0
// (0, 0) -> expected+untouched span<char> sibling path.
#define TRY_FORMAT_BOUNDED_SPAN_N_STAR_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)                \
                                                 __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                             \
                                    __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_STAR_PAD == 1
// (0, 1) -> expected+STAR-padded span<char> sibling path (NEW today).
#define TRY_FORMAT_BOUNDED_SPAN_N_STAR_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_star_pad_span((out), (fmt)       \
                                                           __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_star_pad_span((out), (fmt)               \
                                                   __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_STAR_PAD == 0
// (1, 0) -> throwing+untouched span<char> sibling path.
#define TRY_FORMAT_BOUNDED_SPAN_N_STAR_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)       \
                                                          __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                    \
                                             __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_STAR_PAD == 1
// (1, 1) -> throwing+STAR-padded span<char> sibling path (NEW today).
#define TRY_FORMAT_BOUNDED_SPAN_N_STAR_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_star_pad_span(          \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_star_pad_span((out), (fmt)      \
                                                           __VA_OPT__(,) __VA_ARGS__))
#endif
```

## Observed output

Default build (`-DPETRA_THROW_ON_OVERFLOW=OFF -DPETRA_STAR_PAD=OFF`):
```
== Section 1: expected+STAR-pad (0, 1) cell — bytes after written are '*' ==
== Section 2: expected+STAR-pad (0, 1) cell — buffer UNTOUCHED on TooLarge + sub-span no-bleed ==
== Section 3: expected+STAR-pad (0, 1) cell — buffer UNTOUCHED on Format error ==
== Section 4: expected+STAR-pad (0, 1) cell — written == span.size() and span.size()==0 edge cases ==
== Section 5: STAR-pad (0, 1) cell — byte-state distinctness ==
== Section 6: expected+STAR-pad (0, 1) CONSTEVAL surface — same contract as RUNTIME ==
== Section 7: throwing+STAR-pad (1, 1) cell — success path + throws on TooLarge ==
== Section 8: throwing+STAR-pad (1, 1) CONSTEVAL surface ==
== Section 9: macro TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_STAR_OR_THROW — works at preprocessor time ==
== Section 10: concurrent dispatch — 4 threads x 25 calls on the STAR-pad path ==
== Section 11: decltype pinning on the (THROW, STAR) 2-bit tuple ==
== Summary ==
  PASS=94  FAIL=0
```

STAR-pad cell (`-DPETRA_STAR_PAD=ON`):
```
PASS=94  FAIL=0
```

Throwing-untouched cell (`-DPETRA_THROW_ON_OVERFLOW=ON`):
```
PASS=93  FAIL=0
```

Throwing+STAR-pad cell
(`-DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_STAR_PAD=ON`):
```
PASS=93  FAIL=0
```

(The (THROW=0) build cells have 94 PASSes vs (THROW=1) cells
with 93 PASSes because Section 9's macro check has 2 distinct
CHECKs on the `std::expected<std::size_t, FormatError>` return
type (`has_value` + `value`) but only 1 CHECK on the
`std::size_t` return type — direct check_eq on the size_t
return.  This is consistent with Oct 15's Section 9 design.)

Build across the 4 cells produced identical PASS/FAIL tallies
across five consecutive runs (no flakiness; Section 10's
4-thread × 25-call concurrent dispatch × 5 runs = 500
concurrent calls per cell × 4 cells = 2,000 concurrent calls
total, all PASS).

## Verification performed

1. **Compile + run on the (0, 0) default build** — PASS=94,
   FAIL=0.
2. **Compile + run on the (0, 1) STAR-pad expected build** —
   PASS=94, FAIL=0.
3. **Compile + run on the (1, 0) throwing untouched build** —
   PASS=93, FAIL=0.
4. **Compile + run on the (1, 1) throwing+STAR-pad build** —
   PASS=93, FAIL=0.
5. **Strict-warnings build** (`-Wall -Wextra -Wpedantic
   -Werror -Wshadow -Wconversion -Wsign-conversion`) on the
   (0, 0) build cell — zero warnings, PASS=94, FAIL=0.
6. **ASan + UBSan build** (`-fsanitize=address
   -fsanitize=undefined`) on the (0, 0) build cell — zero
   sanitizer diagnostics, PASS=94, FAIL=0.
7. **Friendly-errors fire at compile time** for
   `PETRA_PAD=1` (with pointer to Sep 27 / Oct 1),
   `PETRA_SPACE_PAD=1` (pointer to Sep 28 / Oct 1),
   `PETRA_DASH_PAD=1` (pointer to Oct 4 / Oct 5 / Oct 8),
   `PETRA_HASH_PAD=1` (pointer to Oct 11),
   `PETRA_UNDERSCORE_PAD=1` (pointer to Oct 15), and
   `PETRA_TILDE_PAD=1` (pointer to Oct 13).  Also
   `PETRA_STAR_PAD=2` and `PETRA_THROW_ON_OVERFLOW=2`
   invalid-flag-value errors fire at compile time.

## Where we go next

The EIGHTH pad character (PETRA_STAR_PAD) is now closed on
the STD::SPAN\<CHAR\> side (today).  The matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_STAR_PAD)` 2-bit tuple is
COMPLETE for ALL FOUR cells on the STD::SPAN\<CHAR\> axis.
Future lessons can either:

1. **A NINTH-axis pad character on the STD::SPAN\<CHAR\>
   side** — e.g. `PETRA_AT_PAD` (`'@'`, ASCII 64) or
   `PETRA_PLUS_PAD` (`'+'`, ASCII 43).  Each such lesson
   would mirror Oct 12 / Oct 13 / Oct 14 / Oct 15 / today's
   pattern exactly — a thin layer on top of today's
   STAR-pad sibling, OVERWRITING the `'*'` fill with the
   new pad byte on success.  Closes the matrix on the
   `(THROW, NEW_PAD)` 2-bit tuple for the STD::SPAN\<CHAR\>
   axis.
2. **A STAR-pad char\*+cap mirror lesson** — bringing the
   CHAR\*+CAP axis in line with the STD::SPAN\<CHAR\> axis
   on the EIGHTH pad-character axis (mirrors Oct 14's
   UNDERSCORE-pad shape on the CHAR\*+CAP side).
3. **A unified 8-bit-tuple dispatcher
   `(THROW × PAD × SPACE × DASH × HASH × TILDE × UNDERSCORE
   × STAR)`** — one macro covers all composition cells
   across all eight pad flags, picking the right wrapper at
   preprocessor time via `__VA_OPT__(,)`-based dispatch.
4. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now that
   the EIGHTH axis is fully closed on the STD::SPAN\<CHAR\>
   side.

The current matrix on the BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis now reaches EIGHT
pad characters on the STD::SPAN\<CHAR\> side: NONE (no
fill) → NUL (Sep 27) → SPACE (Sep 28) → DASH (Oct 4) →
HASH (Oct 11) → TILDE (Oct 13) → UNDERSCORE (Oct 15) →
STAR (today).  On the CHAR\*+CAP side, the matrix reaches
SEVEN: NONE → NUL (Sep 25) → SPACE (Sep 29) → DASH (Oct
3) → HASH (Oct 9) → TILDE (Oct 12) → UNDERSCORE (Oct 14).
The EIGHTH-axis lesson (today on STD::SPAN\<CHAR\>) is the
FINAL axis on the STD::SPAN\<CHAR\> side; bringing the
CHAR\*+CAP axis in line is a natural follow-up.

## Reproducing the build

Default build (expected + untouched; PETRA_THROW=0,
PETRA_STAR_PAD=0):

```bash
cd late-may/cpp_practice/try_format_star_pad_span_n
cmake -S . -B build
cmake --build build
./build/P-2026-10-16-try-format-star-pad-span-n
```

STAR-pad cell (PETRA_STAR_PAD=1):

```bash
cmake -S . -B build-star -DPETRA_STAR_PAD=ON
cmake --build build-star
./build-star/P-2026-10-16-try-format-star-pad-span-n
```

Throwing + STAR-pad cell (PETRA_THROW_ON_OVERFLOW=1,
PETRA_STAR_PAD=1):

```bash
cmake -S . -B build-throw-star \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_STAR_PAD=ON
cmake --build build-throw-star
./build-throw-star/P-2026-10-16-try-format-star-pad-span-n
```

Strict-warning mode:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-16-try-format-star-pad-span-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-16-try-format-star-pad-span-n
```
