# P-2026-10-20 — try_format_plus_pad_n

**Closes Oct 19's "Where we go next" item (1) verbatim — the
CHAR\*+CAP-side TENTH pad-character axis (`PETRA_PLUS_PAD`).**

Today's lesson introduces `PETRA_PLUS_PAD` (a NEW TENTH
compile-time flag) on top of Oct 18's 9-bit tuple
(`PETRA_THROW_ON_OVERFLOW`, `PETRA_PAD`, `PETRA_SPACE_PAD`,
`PETRA_DASH_PAD`, `PETRA_HASH_PAD`, `PETRA_TILDE_PAD`,
`PETRA_UNDERSCORE_PAD`, `PETRA_STAR_PAD`, `PETRA_AT_PAD`) on the
CHAR\*+CAP × BOUNDED-BUFFER axis.  `PETRA_PLUS_PAD=1` selects the
PLUS-padded `char*`+`cap` sibling, which fills bytes
`[written, cap)` with `'+'` (ASCII 0x2B = 43) on success.

The `'+'` byte is TOTALLY DIFFERENT from Oct 18's `'@'` (ASCII 64),
Oct 17's `'*'` (ASCII 42), Oct 14's `'_'` (ASCII 95), Oct 12's `'~'`
(ASCII 126), Oct 11's `'#'` (ASCII 35), Oct 4's `'-'` (ASCII 45),
Sep 28's `' '` (ASCII 32), and Sep 25's `'\0'` (ASCII 0).  After
today, the CHAR\*+CAP matrix on the
`(PETRA_THROW_ON_OVERFLOW × PETRA_PLUS_PAD)` 2-bit tuple is
COMPLETE for ALL FOUR cells, mirroring Oct 18 / Oct 19's closure of
the same axis on the CHAR\*+CAP side.  Oct 19 closed the NINTH axis
on the `std::span<char>` side; the STD::SPAN\<CHAR\> TENTH-axis
mirror (`PETRA_PLUS_PAD` on `std::span<char>`) remains open — that is
the natural next lesson.

The composition rule "the LATER fill overwrites the EARLIER fill"
now has NINE layers on the CHAR\*+CAP axis:
`PLUS > AT > STAR > UNDERSCORE > TILDE > HASH > DASH > SPACE > NUL`.
When all nine pad-flags are set, the end byte state is always `'+'`.
Composition cells with `(PLUS=1, AT=*)` REUSE TODAY's NEW PLUS-pad
wrappers verbatim because PLUS overwrites AT (which overwrites STAR
which overwrites UNDERSCORE which overwrites TILDE which overwrites
HASH which overwrites DASH which overwrites SPACE).

## Where this fits in the arc

```
   - Sep  6   try_format_runtime_bounded — std::string_view fmt +
              std::size_t cap (RUNTIME std::string-output wrapper).
   - Sep  7   try_format_bounded — CONSTEVAL sibling of Sep 6.
   - Sep  8   try_format_runtime_n — BOUNDED-BUFFER × CHAR*+CAP ×
              RUNTIME sibling (returns std::size_t via expected).
   - Sep  9   try_format_bounded_runtime_n — BOUNDED-BUFFER ×
              CHAR*+CAP × CONSTEVAL sibling.
   - Sep 25   try_format_padded_n_or_into — FIRST pad axis
              (PETRA_PAD) on the CHAR*+CAP side (NUL-fill).
   - Sep 28   try_format_space_pad_n — (PETRA_SPACE_PAD) sibling
              wrapper on the CHAR*+CAP side (' ', ASCII 32).
   - Oct  3   try_format_dash_pad_n — (PETRA_DASH_PAD) sibling
              wrapper on the CHAR*+CAP side ('-', ASCII 45).
   - Oct  9   try_format_hash_pad_n — (PETRA_HASH_PAD) sibling
              wrapper on the CHAR*+CAP side ('#', ASCII 35).
   - Oct 12   try_format_tilde_pad_n — (PETRA_TILDE_PAD) sibling
              wrapper on the CHAR*+CAP side ('~', ASCII 126).
   - Oct 14   try_format_underscore_pad_n — (PETRA_UNDERSCORE_PAD)
              sibling wrapper on the CHAR*+CAP side ('_', ASCII 95).
   - Oct 17   try_format_star_pad_n — (PETRA_STAR_PAD) sibling
              wrapper on the CHAR*+CAP side ('*', ASCII 42).
   - Oct 18   try_format_at_pad_n — (PETRA_AT_PAD) sibling wrapper
              on the CHAR*+CAP side ('@', ASCII 64).  TODAY's
              layer sits directly on top of it.
   - Oct 19   try_format_at_pad_span_n — Oct 18's STD::SPAN<CHAR>
              mirror; Oct 19's "Where we go next" item (1) is the
              TENTH pad character, which is what today closes on
              the CHAR*+CAP side.
   - TODAY    try_format_plus_pad_n — (PETRA_PLUS_PAD) sibling
              wrapper on the CHAR*+CAP side ('+', ASCII 43).
```

## Concepts exercised

**1. `std::format_string<Args...>` (P2216R3) — the consteval
format-string gate.** The CONSTEVAL-surface wrappers take
`std::format_string<Args...>`, whose converting constructor is
`consteval`.  That means a malformed or ill-typed format string is
a **compile-time** diagnostic, not a runtime `std::format_error` —
the class only exists if the format string validates.  The
RUNTIME-surface wrappers take `std::string_view`, where a malformed
format string is a **runtime** `std::format_error` that the impl
catches and converts into `FormatErrorKind::Format`.  Section 3 pins
the runtime half; Section 6 pins the consteval half.

**2. The two-pass `std::vformat_to` protocol.** The underlying impl
(Sep 8, reused verbatim) formats **twice**:

- Pass 1 uses `petra::counting_output_iterator` — an output iterator
  that writes nothing and only counts.  It survives
  `std::vformat_to`'s internal iterator *copy* because it holds no
  mutable state of its own: all state lives behind a
  `void (*)(std::size_t, void*)` callback plus a `void*`.
- If `needed > cap`, the impl returns `TooLarge` **before pass 2 ever
  writes a byte**.  *This* is what gives the buffer its
  atomicity-on-error contract: Sections 2, 3 and 7 all pin "every
  byte is still the poison byte `'Q'`" after a failed call.
- Pass 2 uses `petra::detail::bounded_char_writer`, which silently
  *drops* any byte past `cap` rather than overrunning.  The impl
  then asserts `written == needed` as a consistency check.

**3. `std::expected` (P0323R12) vs `std::runtime_error` — two error
surfaces over one impl.** The expected surface returns
`std::expected<std::size_t, FormatError>` with a structured
`kind` + `message` + `needed_bytes`; the throwing surface converts
that into `std::runtime_error` with a `"TooLarge: "` or
`"Format: "` message prefix.  Today's throwing wrappers **re-raise
untouched** (`catch (...) { throw; }`) — they never catch & rewrap,
so the exception type and `.what()` bytes are byte-exact with what
the underlying impl constructed.  Section 7 pins this.

**4. The "LATER fill overwrites the EARLIER fill" composition
rule.** Every pad wrapper is a thin layer:

```cpp
template <typename... Args>
std::expected<std::size_t, FormatError>
try_format_runtime_n_plus_pad(char* out, std::size_t cap,
                              std::string_view fmt, const Args&... args) {
    auto r = try_format_runtime_n_at_pad(out, cap, fmt, args...);
    if (r.has_value()) {                     // guard: buffer atomicity
        const std::size_t written = r.value();
        for (std::size_t i = written; i < cap; ++i) out[i] = '+';
    }
    return r;
}
```

Five lines of new logic buy the entire TENTH axis.  The
`if (r.has_value())` guard is the load-bearing line: drop it and the
fill loop would run on the error path, stamping `'+'` over a buffer
that the impl promised to leave untouched.

**5. `if constexpr` adapter for two return types — `petra::unwrap_dispatch`.**
The dispatcher macro returns `std::expected<std::size_t,
FormatError>` in the `(THROW=0, *)` cells and a bare `std::size_t` in
the `(THROW=1, *)` cells.  To write ONE call site that compiles in
all four cells, today uses the standard idiom:

```cpp
template <typename T>
std::size_t unwrap_dispatch(T&& r) {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, std::expected<std::size_t, FormatError>>) {
        if (r.has_value()) { return r.value(); }
        return static_cast<std::size_t>(-1);
    } else {
        return static_cast<std::size_t>(r);
    }
}
```

Only one branch is instantiated per cell — the discarded branch is
not even compiled, so there is no runtime cost and no risk of the
expected branch's `r.has_value()` being applied to a `std::size_t`.

**6. `__VA_OPT__(,)` in a parenthesized-expression macro.** The
dispatcher is an expression (not `do { } while (0)`), so the caller
can capture the result: `auto n = TRY_FORMAT_BOUNDED_N_PLUS_OR_THROW(...)`.
`__VA_OPT__(,)` supplies the comma only when `__VA_ARGS__` is
non-empty, which makes the no-args case `fmt` alone legal.

## Key design decisions

- **NARROW SCOPE.** Only `PETRA_PLUS_PAD` (and `PETRA_THROW_ON_OVERFLOW`)
  are live; the other eight pad flags `#error` at compile time with
  pointers to the lessons that own those cells.  A 10-bit-tuple
  dispatcher would need 2^10 cells — this lesson keeps the matrix at
  2 × 2 = 4 and keeps the file a viable single-file exercise.
- **SELF-CONTAINED.** The file does **not** `#include` Oct 18's
  1,185-line source.  It contains the full helper stack (FormatError,
  both iterator primitives, the impl, the eight prior pad wrappers,
  the four AT-pad wrappers) plus the four NEW PLUS-pad wrappers, so
  the whole thing verifies from a single `clang++` invocation.
- **COMPILE-TIME ASCII PROOF.** `'+'` is ASCII 43 and `'*'` is ASCII
  42 — **adjacent codes**.  An off-by-one literal typo between the two
  is invisible in review and in a casual buffer eyeball.  Section 5
  pins the literal codes with ten `static_assert`s (including
  `static_assert('+' == '*' + 1)`) so the adjacentness is an
  explicit, documented fact rather than a latent bug.
- **ATOMICITY RE-PINNED, NOT ASSUMED.** The new
  `for (i = written; i < cap; ++i) out[i] = '+';` loop *could*
  diverge from the underlying impl's promise if the `has_value()`
  guard were mis-coded.  Sections 2, 3 and 7 independently verify
  every byte is still `'Q'` after a `TooLarge` and after a `Format`
  error, on both the expected and the throwing surface.
- **CONCURRENCY.** The wrapper chain is stateless — all per-call
  state is a stack local.  Section 10 runs 4 threads × 25 calls into
  thread-local buffers and tallies 100/100 all-`'+'` tails.

## The FOUR new wrappers

| wrapper | surface | on success | on error |
|---|---|---|---|
| `try_format_runtime_n_plus_pad` | RUNTIME × EXPECTED | `std::expected<std::size_t, FormatError>`; bytes `[written, cap)` = `'+'` | returns the underlying `TooLarge` / `Format` error; buffer untouched |
| `try_format_bounded_runtime_n_plus_pad` | CONSTEVAL × EXPECTED | same, via `std::format_string<Args...>` | same |
| `try_format_runtime_n_throwing_plus_pad` | RUNTIME × THROWING | `std::size_t`; bytes `[written, cap)` = `'+'` | `throw;` re-raises the underlying `std::runtime_error` untouched |
| `try_format_bounded_runtime_n_throwing_plus_pad` | CONSTEVAL × THROWING | same, via `std::format_string<Args...>` | same |

Plus one meta-macro:

```cpp
TRY_FORMAT_BOUNDED_N_PLUS_OR_THROW(out, cap, fmt, ...)
```

which selects one of four wrappers at preprocessor time from the
`(PETRA_THROW_ON_OVERFLOW, PETRA_PLUS_PAD)` 2-bit tuple.

## The (THROW × PLUS) 2-bit matrix

| cell | `PETRA_THROW_ON_OVERFLOW` | `PETRA_PLUS_PAD` | wrapper selected | bytes `[written, cap)` on success |
|---|---|---|---|---|
| (0, 0) | 0 | 0 | `try_format_runtime_n` (Sep 8) | untouched `'Q'` |
| (0, 1) | 0 | 1 | `try_format_runtime_n_plus_pad` (NEW) | `'+'` |
| (1, 0) | 1 | 0 | `try_format_runtime_n_throwing` (Sep 20) | untouched `'Q'` |
| (1, 1) | 1 | 1 | `try_format_runtime_n_throwing_plus_pad` (NEW) | `'+'` |

## Observed output

Default build (`PETRA_THROW_ON_OVERFLOW=0`, `PETRA_PLUS_PAD=0`):

```
== Section 1: expected+PLUS-pad (0, 1) cell — bytes after written are '+' ==
== Section 2: expected+PLUS-pad (0, 1) cell — buffer UNTOUCHED on TooLarge ==
== Section 3: expected+PLUS-pad (0, 1) cell — buffer UNTOUCHED on Format error ==
== Section 4: expected+PLUS-pad (0, 1) cell — written == cap and cap == 0 edge cases ==
== Section 5: PLUS-pad (0, 1) cell — byte-state distinctness ==
== Section 6: expected+PLUS-pad (0, 1) CONSTEVAL surface — same contract as RUNTIME ==
== Section 7: throwing+PLUS-pad (1, 1) cell — success path + throws on TooLarge ==
== Section 8: throwing+PLUS-pad (1, 1) CONSTEVAL surface ==
== Section 9: macro TRY_FORMAT_BOUNDED_N_PLUS_OR_THROW — real call in all 4 cells ==
== Section 10: 4 threads × 25 calls — concurrent PLUS-pad dispatch ==
== Summary ==
  PASS=94  FAIL=0
```

## Verification performed

**Compiles clean under strict warnings, all four flag cells:**

```
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion \
        [cell flags] P-2026-10-20-try-format-plus-pad-n.cpp -o <out>
```

**Test tallies — 94/94 PASS on every configuration, zero stderr bytes:**

| configuration | build | run | result |
|---|---|---|---|
| default | ok | 0 | PASS=94 FAIL=0 |
| `-DPETRA_PLUS_PAD=ON` | ok | 0 | PASS=94 FAIL=0 |
| `-DPETRA_THROW_ON_OVERFLOW=ON` | ok | 0 | PASS=94 FAIL=0 |
| `-DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PLUS_PAD=ON` | ok | 0 | PASS=94 FAIL=0 |
| `ENABLE_STRICT_WARNINGS=ON` | ok | 0 | PASS=94 FAIL=0, 0 warnings |
| `ENABLE_ASAN=ON` | ok | 0 | PASS=94 FAIL=0, 0 sanitizer diagnostics |
| `STRICT + ASAN` | ok | 0 | PASS=94 FAIL=0 |
| `STRICT + PLUS` | ok | 0 | PASS=94 FAIL=0, 0 warnings |
| `STRICT + THROW + PLUS` | ok | 0 | PASS=94 FAIL=0, 0 warnings |
| `ASAN + THROW + PLUS` | ok | 0 | PASS=94 FAIL=0 |

**Flakiness:** five consecutive runs of each of the four flag cells
produced identical `PASS=94 FAIL=0` tallies — 20 runs, zero
failures.

**ASan + UBSan:** `ASAN_OPTIONS=detect_leaks=0 ./...` on the
`THROW + PLUS` cell and the `STRICT + ASAN` cell — zero diagnostics,
empty stderr.

**Friendly-error (compile-time) checks** — all fire as expected:

```
-DPETRA_PLUS_PAD=2            → "PETRA_PLUS_PAD must be 0 (untouched) or 1 (PLUS-padded sibling)"
-DPETRA_THROW_ON_OVERFLOW=2   → "PETRA_THROW_ON_OVERFLOW must be 0 (expected sibling) or 1 (throwing sibling)"
-DPETRA_AT_PAD=1              → pointer to Oct 18 / Oct 19 (AT-padded siblings)
-DPETRA_STAR_PAD=1            → pointer to Oct 17 / Oct 16 (STAR-padded siblings)
-DPETRA_PAD=1                 → pointer to Sep 25 / Sep 26 / Sep 27 (NUL-padded wrappers)
```

**All temporary artifacts were kept under `/tmp`** — the verification
driver (`/tmp/petra-verify-plus.sh`) and every CMake build tree and
binary live in `/tmp`; nothing compiled was staged.

## Rebuilding

Default build:

```bash
cmake -S . -B build
cmake --build build
./build/P-2026-10-20-try-format-plus-pad-n
```

PLUS-pad cell:

```bash
cmake -S . -B build-plus -DPETRA_PLUS_PAD=ON
cmake --build build-plus
./build-plus/P-2026-10-20-try-format-plus-pad-n
```

Throwing + PLUS-pad cell:

```bash
cmake -S . -B build-throw-plus \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PLUS_PAD=ON
cmake --build build-throw-plus
./build-throw-plus/P-2026-10-20-try-format-plus-pad-n
```

Strict-warning mode:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-20-try-format-plus-pad-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ./build-asan/P-2026-10-20-try-format-plus-pad-n
```

## Where we go next

The CHAR\*+CAP TENTH pad character (`PETRA_PLUS_PAD`) is now closed
across ALL FOUR cells of the `(PETRA_THROW_ON_OVERFLOW ×
PETRA_PLUS_PAD)` 2-bit tuple.  Combined with Oct 18 / Oct 17 / Oct
16 / Oct 15 / Oct 14 / Oct 13 / Oct 12 / Oct 9 / Oct 4 / Sep 28 / Sep
27 / Sep 25, the BOUNDED-BUFFER × expected-vs-throwing ×
pad-character matrix reaches TEN pad characters on the CHAR\*+CAP
axis: NONE (no fill) → NUL → SPACE → DASH → HASH → TILDE →
UNDERSCORE → STAR → AT → PLUS.

Future lessons can either:

1. **The STD::SPAN\<CHAR\> TENTH-axis mirror** — Oct 20's
   `try_format_plus_pad_span_n`, closing the PETRA_PLUS_PAD axis on
   the `std::span<char>` side, exactly mirroring Oct 19's shape
   (four new span\<char\> PLUS-pad sibling wrappers + two meta-macros
   `TRY_FORMAT_BOUNDED_SPAN_N_PLUS_OR_THROW` and
   `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PLUS_OR_THROW`).  This
   restores the "every pad character is closed on BOTH axes"
   invariant that Oct 18 / Oct 19 established.

2. **An ELEVENTH-axis pad character on either side** — e.g.
   `PETRA_BANG_PAD` (`'!'`, ASCII 33).  Each such lesson would
   mirror Oct 17 / Oct 18 / today's pattern exactly — a thin layer
   on top of today's PLUS-pad sibling, OVERWRITING the `'+'` fill
   with the new pad byte on success.

3. **A unified 10-bit-tuple dispatcher `(THROW × PAD × SPACE × DASH
   × HASH × TILDE × UNDERSCORE × STAR × AT × PLUS)`** — one macro
   covering all composition cells across all ten pad flags, picking
   the right wrapper at preprocessor time.  Because the composition
   rule is a strict total order (`PLUS > AT > STAR > UNDERSCORE >
   TILDE > HASH > DASH > SPACE > NUL`), such a dispatcher can select
   the *highest-ranked* enabled pad flag with a simple preprocessor
   priority chain rather than an exhaustive 2^10 table — worth
   working out explicitly in a lesson.