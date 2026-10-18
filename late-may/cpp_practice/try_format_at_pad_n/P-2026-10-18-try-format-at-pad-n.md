# P-2026-10-18 — try_format_at_pad_n

**Closes Oct 17's "Where we go next" item (1) verbatim — the
CHAR\*+CAP-side NINTH pad-character axis (`PETRA_AT_PAD`),
introducing a TOTALLY DIFFERENT pad character from the prior
EIGHT-axis closure (Sep 25 / Sep 27 / Sep 28 / Oct 3 / Oct 9 /
Oct 12 / Oct 14 / Oct 16 / Oct 17).**

Today's lesson introduces `PETRA_AT_PAD` (a NEW NINTH
compile-time flag on top of Oct 17's 8-bit tuple) on the
CHAR\*+CAP × BOUNDED-BUFFER axis.  `PETRA_AT_PAD=1`
selects the AT-padded char\*+cap sibling, which fills
bytes `[written, cap)` with `'@'` (ASCII 64) on success.

The `'@'` character is **TOTALLY DIFFERENT** from Oct 17's
`'*'` (42), Oct 14's `'_'` (95), Oct 12's `'~'` (126), Oct 9's
`'#'` (35), Oct 3's `'-'` (45), Sep 28's `' '` (32), and Sep
25's `'\0'` (0).  The `'@'` byte has ASCII value `64` (0x40),
distinct from the seven prior pad bytes and the poison byte
`'Q'` (0x51).

After today, the CHAR\*+CAP × BOUNDED-BUFFER matrix on
the `(PETRA_THROW_ON_OVERFLOW × PETRA_AT_PAD)` 2-bit tuple
is COMPLETE for ALL FOUR cells.

## Where this fits in the arc

| Lesson      | Topic                                                                  |
|-------------|------------------------------------------------------------------------|
| Sep 8       | `try_format_runtime_n` (BOUNDED-BUFFER × CHAR\*+CAP × RUNTIME × EXPECTED) |
| Sep 9       | `try_format_bounded_runtime_n` (CONSTEVAL × EXPECTED)                  |
| Sep 20      | `try_format_runtime_n_throwing` (RUNTIME × THROWING)                   |
| Sep 25      | NUL-pad (`PETRA_PAD`) char\*+cap                                         |
| Sep 28      | SPACE-pad (`PETRA_SPACE_PAD`) char\*+cap                                |
| Oct 3       | DASH-pad (`PETRA_DASH_PAD`) char\*+cap                                  |
| Oct 9       | HASH-pad (`PETRA_HASH_PAD`) char\*+cap                                  |
| Oct 12      | TILDE-pad (`PETRA_TILDE_PAD`) char\*+cap                                |
| Oct 14      | UNDERSCORE-pad (`PETRA_UNDERSCORE_PAD`) char\*+cap                      |
| Oct 16      | STAR-pad (`PETRA_STAR_PAD`) span\<char\>                                |
| Oct 17      | STAR-pad (`PETRA_STAR_PAD`) char\*+cap                                  |
| **TODAY**   | **AT-pad (`PETRA_AT_PAD`) char\*+cap**                                  |

## Headline findings

1. **The AT-pad char\*+cap sibling is a thin layer on top of
   Oct 17's STAR-pad char\*+cap sibling.**  Today's AT-pad
   wrapper calls Oct 17's wrapper, then OVERWRITES the `'*'`
   fill with `'@'` on success.  This is the same pattern Oct
   17 used (STAR layer on Oct 14's UNDERSCORE-pad char\*+cap
   sibling), Oct 14 used (UNDERSCORE layer on Oct 12's
   TILDE-pad char\*+cap sibling), Oct 12 used (TILDE layer on
   Oct 9's HASH-pad char\*+cap sibling), and Oct 9 used (HASH
   layer on Oct 3's DASH-pad char\*+cap sibling).

2. **The composition rule "the LATER fill overwrites the
   EARLIER fill" now has EIGHT layers on the CHAR\*+CAP axis:
   `AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE >
   NUL`.**  When ALL EIGHT pad-flags are set, the end byte
   state is always `'@'`.  Composition cells with
   `(AT=1, STAR=*)` REUSE TODAY's NEW AT-pad wrappers
   verbatim (the `(AT=1, STAR=*)` cell and `(AT=1, STAR=0)`
   cell share wrappers because AT overwrites STAR, which
   overwrites UNDERSCORE, which overwrites TILDE which
   overwrites HASH which overwrites DASH which overwrites
   SPACE).

3. **No new error contracts are introduced.**  The new
   wrappers preserve the SAME contract as Oct 17's
   STAR-pad char\*+cap wrappers:
   - **Success path:** returns the byte count via
     `expected<size_t, FormatError>` (EXPECTED siblings) or
     `std::size_t` (THROWING siblings).  Bytes
     `[written, cap)` are `'@'`.
   - **TooLarge path:** returns
     `unexpected{FormatError{TooLarge, ..., needed_bytes}}`
     (EXPECTED siblings) or throws `std::runtime_error` with
     the same message (THROWING siblings).  Bytes
     `[0, cap)` are UNTOUCHED (no partial `'@'` fill — the
     buffer's atomicity-on-error contract is preserved).
   - **Format error path:** returns
     `unexpected{FormatError{Format, ...}}` or throws
     `std::runtime_error` similarly.  Buffer UNTOUCHED.

4. **The CONSTEVAL gate is preserved regardless of the
   `(THROW, AT)` 2-bit tuple** —
   `TRY_FORMAT_BOUNDED_N_AT_OR_THROW`'s CONSTEVAL form
   requires `std::format_string<Args...>` (a literal fmt at
   the call site).  The RUNTIME-surface dispatcher macro is
   provided today.

5. **The macro `TRY_FORMAT_BOUNDED_N_AT_OR_THROW`
   (RUNTIME surface) is a verbatim thin dispatcher** that
   picks between FOUR sibling wrappers at preprocessor time
   based on the `(PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD)`
   2-bit tuple.  The macro is a parenthesized expression
   (NOT `do { } while (0)`), so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_N_AT_OR_THROW(...)` and
   capture the result.  The macro uses `__VA_OPT__(,)` for the
   no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 /
   Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep
   26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 /
   Oct 4 / Oct 5 / Oct 8 / Oct 9 / Oct 11 / Oct 12 / Oct 13 /
   Oct 14 / Oct 16 / Oct 17's macro shape).

6. **The CHAR\*+CAP × BOUNDED-BUFFER matrix on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_AT_PAD)` 2-bit tuple
   is now COMPLETE for all four cells:**
   - `(0,0)` → Sep 8/9/20 underlying untouched sibling;
   - `(0,1)` → TODAY's NEW AT-pad expected sibling;
   - `(1,0)` → Sep 20 underlying throwing untouched sibling;
   - `(1,1)` → TODAY's NEW AT-pad throwing sibling.

## Important code

The four NEW sibling wrappers follow Oct 17's STAR-pad
shape verbatim.  The RUNTIME / EXPECTED char\*+cap sibling
is the canonical example:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_at_pad(char* out, std::size_t cap,
                            std::string_view fmt,
                            const Args&... args) {
    auto r = try_format_runtime_n_star_pad(out, cap, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        // LATER fill (AT) overwrites EARLIER fill (STAR) from
        // the STAR-pad sibling.  On error, r.has_value() is
        // false and the loop is skipped, preserving the
        // buffer's atomicity-on-error contract.
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '@';
        }
    }
    return r;
}
```

The CONSTEVAL sibling is a thin forwarding wrapper:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_at_pad(
        char* out, std::size_t cap,
        std::format_string<Args...> fmt,
        const Args&... args) {
    return try_format_runtime_n_at_pad(out, cap, fmt.get(), args...);
}
```

The THROWING sibling uses the same idiom Oct 17 established,
so the `std::runtime_error` thrown by the underlying
STAR-pad char\*+cap sibling is re-raised untouched:

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_at_pad(
        char* out, std::size_t cap, std::string_view fmt,
        const Args&... args) {
    try {
        auto r = try_format_runtime_n_at_pad(out, cap, fmt, args...);
        if (!r.has_value()) {
            const auto& e = r.error();
            if (e.kind == FormatErrorKind::TooLarge) {
                throw std::runtime_error{
                    std::string{"TooLarge: "} + e.message};
            }
            throw std::runtime_error{
                std::string{"Format: "} + e.message};
        }
        // On success, OVERWRITE the STAR pad from the STAR-pad
        // sibling with the AT pad.
        for (std::size_t i = r.value(); i < cap; ++i) out[i] = '@';
        return r.value();
    } catch (...) {
        throw;  // re-raise unchanged
    }
}
```

The RUNTIME-surface dispatcher macro:

```cpp
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_AT_PAD == 0
// (0, 0) -> expected+untouched char*+cap sibling path.
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...)              \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                    \
                                    __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_AT_PAD == 1
// (0, 1) -> expected+AT-padded char*+cap sibling path (NEW today).
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...)              \
    (::petra::try_format_runtime_n_at_pad((out), (cap), (fmt)            \
                                            __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_AT_PAD == 0
// (1, 0) -> throwing+untouched char*+cap sibling path.
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...)              \
    (::petra::try_format_runtime_n_throwing((out), (cap), (fmt)           \
                                              __VA_OPT__(,) __VA_ARGS__))
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_AT_PAD == 1
// (1, 1) -> throwing+AT-padded char*+cap sibling path (NEW today).
#define TRY_FORMAT_BOUNDED_N_AT_OR_THROW(out, cap, fmt, ...)              \
    (::petra::try_format_runtime_n_throwing_at_pad((out), (cap), (fmt)    \
                                                    __VA_OPT__(,) __VA_ARGS__))
#endif
```

## Observed output

Default build (`-DPETRA_THROW_ON_OVERFLOW=OFF -DPETRA_AT_PAD=OFF`):
```
== Section 1: expected+AT-pad (0, 1) cell — bytes after written are '@' ==
== Section 2: expected+AT-pad (0, 1) cell — buffer UNTOUCHED on TooLarge ==
== Section 3: expected+AT-pad (0, 1) cell — buffer UNTOUCHED on Format error ==
== Section 4: expected+AT-pad (0, 1) cell — written == cap and cap == 0 edge cases ==
== Section 5: AT-pad (0, 1) cell — byte-state distinctness ==
== Section 6: expected+AT-pad (0, 1) CONSTEVAL surface — same contract as RUNTIME ==
== Section 7: throwing+AT-pad (1, 1) cell — success path + throws on TooLarge ==
== Section 8: throwing+AT-pad (1, 1) CONSTEVAL surface ==
== Section 9: macro TRY_FORMAT_BOUNDED_N_AT_OR_THROW — works at preprocessor time ==
== Summary ==
  PASS=82  FAIL=0
```

AT-pad cell (`-DPETRA_AT_PAD=ON`):
```
PASS=82  FAIL=0
```

Throwing-untouched cell (`-DPETRA_THROW_ON_OVERFLOW=ON`):
```
PASS=82  FAIL=0
```

Throwing+AT-pad cell
(`-DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_AT_PAD=ON`):
```
PASS=82  FAIL=0
```

Build across the 4 cells produced identical PASS/FAIL tallies
across five consecutive runs (no flakiness; 5 runs × 4 cells =
20 total runs, all 82/82 PASS).

ASan/UBSan build cell (`-DENABLE_ASAN=ON`):
```
PASS=82  FAIL=0
```
Zero sanitizer diagnostics on stderr.

Strict-warning build cell (`-DENABLE_STRICT_WARNINGS=ON`):
```
PASS=82  FAIL=0
```
Zero warnings under `-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`.

## Verification performed

1. **Compile + run on the (0, 0) default build** — PASS=82,
   FAIL=0.
2. **Compile + run on the (0, 1) AT-pad expected build** —
   PASS=82, FAIL=0.
3. **Compile + run on the (1, 0) throwing untouched build** —
   PASS=82, FAIL=0.
4. **Compile + run on the (1, 1) throwing+AT-pad build** —
   PASS=82, FAIL=0.
5. **5 consecutive runs × 4 cells = 20 total runs**, all
   82/82 PASS, zero flakiness.
6. **Strict-warnings build** (`-Wall -Wextra -Wpedantic
   -Werror -Wshadow -Wconversion -Wsign-conversion`) on the
   (0, 0) build cell — zero warnings, PASS=82, FAIL=0.
7. **ASan + UBSan build** (`-fsanitize=address
   -fsanitize=undefined`) on the (0, 0) build cell — zero
   sanitizer diagnostics, PASS=82, FAIL=0.
8. **Friendly-errors fire at compile time** for
   `PETRA_PAD=1` (with pointer to Sep 25 / Sep 27 / Oct 1),
   `PETRA_SPACE_PAD=1` (pointer to Sep 28 / Sep 29 / Oct 1),
   `PETRA_DASH_PAD=1` (pointer to Oct 3 / Oct 5 / Oct 8),
   `PETRA_HASH_PAD=1` (pointer to Oct 9 / Oct 11),
   `PETRA_TILDE_PAD=1` (pointer to Oct 12 / Oct 13),
   `PETRA_UNDERSCORE_PAD=1` (pointer to Oct 14 / Oct 15),
   and `PETRA_STAR_PAD=1` (pointer to Oct 16 / Oct 17).
   Also `PETRA_AT_PAD=2` and `PETRA_THROW_ON_OVERFLOW=2`
   invalid-flag-value errors fire at compile time.

## Where we go next

The NINTH pad character (`PETRA_AT_PAD`) is now closed on the
CHAR\*+CAP axis (today).  Combined with Oct 17's closure of
the EIGHTH pad character (`PETRA_STAR_PAD`) on the same axis,
the CHAR\*+CAP × BOUNDED-BUFFER matrix now reaches NINE pad
characters.  The STD::SPAN\<CHAR\> axis reaches EIGHT pad
characters (closed by Oct 15 / Oct 16).  Future lessons can
either:

1. **A TENTH-axis pad character on either side** — e.g.
   `PETRA_PLUS_PAD` (`'+'`, ASCII 43) or `PETRA_BANG_PAD`
   (`'!'`, ASCII 33).  Each such lesson would mirror Oct 12 /
   Oct 13 / Oct 14 / Oct 15 / Oct 16 / Oct 17 / today's pattern
   exactly — a thin layer on top of today's AT-pad sibling,
   OVERWRITING the `'@'` fill with the new pad byte on success.
   Closes the matrix on the `(THROW, NEW_PAD)` 2-bit tuple
   for that axis.
2. **An STD::SPAN\<CHAR\> mirror of today's AT-pad lesson** —
   `try_format_at_pad_span_n` would close the STD::SPAN\<CHAR\>
   axis on the NINTH pad character, mirroring Oct 16's
   STD::SPAN\<CHAR\> closure of Oct 17's CHAR\*+CAP EIGHTH axis
   in reverse.  Today closes the CHAR\*+CAP side only; the
   STD::SPAN\<CHAR\> side remains a future follow-up.
3. **A unified 9-bit-tuple dispatcher
   `(THROW × PAD × SPACE × DASH × HASH × TILDE × UNDERSCORE
   × STAR × AT)`** — one macro covers all composition cells
   across all nine pad flags, picking the right wrapper at
   preprocessor time via `__VA_OPT__(,)`-based dispatch.
   This would close the composition matrix on both axes
   simultaneously (mirroring Oct 1's PAD × SPACE composition
   lesson on the span\<char\> side, and Sep 30 / Oct 5 / Oct 8
   on the char\*+cap side).
4. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited now that
   the NINTH axis is closed on the CHAR\*+CAP side (the
   STD::SPAN\<CHAR\> axis remains at EIGHT pad characters
   until future work closes the AT axis there too).

The current matrix on the BOUNDED-BUFFER ×
expected-vs-throwing × pad-character axis reaches NINE pad
characters on the CHAR\*+CAP side: NONE (no fill) → NUL →
SPACE → DASH → HASH → TILDE → UNDERSCORE → STAR → AT.  The
STD::SPAN\<CHAR\> side reaches EIGHT pad characters and is one
lesson behind (no AT axis yet).

## Reproducing the build

Default build (expected + untouched; PETRA_THROW=0,
PETRA_AT_PAD=0):

```bash
cd late-may/cpp_practice/try_format_at_pad_n
cmake -S . -B build
cmake --build build
./build/P-2026-10-18-try-format-at-pad-n
```

AT-pad cell (PETRA_AT_PAD=1):

```bash
cmake -S . -B build-at -DPETRA_AT_PAD=ON
cmake --build build-at
./build-at/P-2026-10-18-try-format-at-pad-n
```

Throwing + AT-pad cell (PETRA_THROW_ON_OVERFLOW=1,
PETRA_AT_PAD=1):

```bash
cmake -S . -B build-throw-at \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_AT_PAD=ON
cmake --build build-throw-at
./build-throw-at/P-2026-10-18-try-format-at-pad-n
```

Strict-warning mode:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-18-try-format-at-pad-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-18-try-format-at-pad-n
```