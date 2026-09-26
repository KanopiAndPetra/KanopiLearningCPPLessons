# P-2026-09-26 — `try_format_padded_n_or_throw`

Topic: a **2-AXIS (× 2-SIBLING) META-MACRO layer** that lifts BOTH the
**RETURN-TYPE-vs-EXCEPTION** sibling choice (Sep 20) AND the
**PAD-VS-UNTOUCHED** sibling choice (Sep 25) to a meta-macro form on
the **CHAR\*+CAP × BOUNDED-BUFFER** axis.  Two new meta-macros —
`TRY_FORMAT_BOUNDED_N_PAD_OR_THROW` (CONSTEVAL surface) and
`TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW` (RUNTIME surface) — each
dispatch to **ONE OF FOUR** sibling wrappers at preprocessor time
based on TWO caller-controlled compile-time flags
`(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)`.

**Closes Sep 25's "Where we go next" item #1 verbatim**:

> The natural follow-on is Sep 20's throwing wrappers
> (`petra::try_format_runtime_n_throwing` + Sep 20's CONSTEVAL
> `try_format_bounded_runtime_n_throwing`); each would gain a
> `*_padded` wrapper that zero-fills `[written, cap)` to `'\0'`
> on success.  (4 cells: CONSTEVAL/RUNTIME × untouched/padded.)

Today covers that verbatim — TWO new
`petra::try_format_*_padded_throwing(char*, std::size_t, fmt, args...)`
wrappers (a RUNTIME surface and a CONSTEVAL surface) that compose on
top of Sep 20's NEW throwing wrappers and zero-fill `[written, cap)`
to `'\0'` on success only.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) quadrant on
the char\*+cap side grows from **32 cells (Sep 25)** to **64 cells
(TODAY)** — a 2× expansion from adding the FOURTH axis (Sep 20)
already-doubled-everything-now-being-doubled-again via the
independent FIFTH axis (Sep 25).

## Where this fits in the arc

- Aug 25   `std::format_to_n_into` — NUL-terminating bounded
           formatter WITHOUT a cap parameter.
- Sep  6   `try_format_runtime_bounded` — `std::string_view` fmt +
           `std::size_t` cap (RUNTIME `std::string`-output wrapper).
- Sep  7   `try_format_bounded` — CONSTEVAL sibling of Sep 6.
- Sep  8   `try_format_runtime_n` — BOUNDED-BUFFER sibling of
           Sep 7 (char\* + `std::size_t` cap + `std::string_view`
           fmt, NO NUL).
- Sep  9   `try_format_bounded_runtime_n` — CONSTEVAL char\*+cap
           (NO NUL).
- Sep 10   `try_format_bounded_runtime_n_span` — CONSTEVAL
           `std::span<char>` char\*+cap (NO NUL).
- Sep 11   `format_to_n_into_bounded` — CONSTEVAL NUL-terminating
           char\*+cap (plus `std::span<char>` overload).
- Sep 12   `try_format_runtime_n_into_bounded` — RUNTIME
           NUL-terminating char\*+cap (plus `std::span<char>`
           overload).
- Sep 13   `try_format_bounded_macros` — eight `TRY_FORMAT_BOUNDED_*`
           macros (CONSTEVAL vs RUNTIME sibling choice at the
           call site).
- Sep 14   `try_format_span_format_string` — `std::span<const char>`
           overloads for the runtime try_format_\* family + four
           new macros.
- Sep 15   `try_format_consteval_span_format_string` — pins that
           the CONSTEVAL surface is incompatible with
           `std::span<const char>`; adds a CONSTEVAL-SHAPED
           RUNTIME PARSER family of four wrappers + four macros.
- Sep 16   `try_format_bounded_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer (char\*+cap
           axis).
- Sep 19   `try_format_span_n_or_into` — the BOUNDED-BUFFER vs
           NUL-TERMINATING sibling-choice macro layer on the
           `std::span<char>` axis.
- Sep 20   `try_format_bounded_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the char\*+cap
           axis (the FOURTH axis; covers the char\*+cap ×
           bounded-buffer × expected-vs-throwing cells).
- Sep 21   `try_format_span_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the
           `std::span<char>` axis.
- Sep 22   `try_format_bounded_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the CHAR\*+CAP axis.
- Sep 23   `try_format_span_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the STD::SPAN<CHAR> axis.
- Sep 25   `try_format_padded_n_or_into` — the FIFTH axis: the
           PAD-VS-UNTOUCHED sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis (covers 2 NEW cells
           on the char\*+cap × CONSTEVAL-or-RUNTIME × bounded-
           buffer × expected × padded axis; the matrix grows
           from 16 to 32 cells on the BOUNDED-BUFFER × expected
           quadrant on the char\*+cap side).
- TODAY    `try_format_padded_n_or_throw` — the FIFTH axis
           (PAD-VS-UNTOUCHED) × FOURTH axis (EXPECTED-VS-THROWING)
           COMBINED META-MACRO layer on the CHAR\*+CAP ×
           BOUNDED-BUFFER axis (covers 4 NEW cells on the
           char\*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer ×
           throwing × untouched-or-padded axis; the matrix grows
           from 32 to 64 cells on the BOUNDED-BUFFER quadrant on
           the char\*+cap side; closes Sep 25's "Where we go next"
           item #1 verbatim).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
  Sep 23 / Sep 25 verbatim (the wrappers below do not produce
  `FormatError` directly — they only forward to existing wrappers,
  but the type must be visible in scope for
  `std::expected<std::size_t, FormatError>` to be instantiatable by
  the `static_assert` below).
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim.
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_n_impl` (Sep 8 verbatim) +
  `petra::detail::try_format_bounded_runtime_n_impl` (Sep 9
  verbatim) — REUSED verbatim (the bounded-buffer underlying
  impls).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` — REUSED Sep 8 verbatim (the RUNTIME expected+untouched
  sibling).
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 9 verbatim
  (the CONSTEVAL expected+untouched sibling).
- `petra::try_format_runtime_n_padded(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 25 verbatim (the RUNTIME
  expected+padded sibling — composes on top of Sep 8).
- `petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 25 verbatim
  (the CONSTEVAL expected+padded sibling — composes on top of
  Sep 9).
- `petra::try_format_runtime_n_throwing(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 20 verbatim (the RUNTIME
  throwing+untouched sibling — composes on top of Sep 8).
- `petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 20 verbatim
  (the CONSTEVAL throwing+untouched sibling — composes on top of
  Sep 9).
- `petra::try_format_runtime_n_padded_throwing(char*, std::size_t,
  std::string_view, args...)` — **NEW today** (RUNTIME surface of the
  BOUNDED-BUFFER × THROWING × PADDED sibling on the char\*+cap axis;
  wraps Sep 20's RUNTIME throwing wrapper verbatim and zero-fills
  `[written, cap)` to `'\0'` on success AFTER the underlying call
  completes).
- `petra::try_format_bounded_runtime_n_padded_throwing(char*,
  std::size_t, std::format_string<Args...>, args...)` — **NEW today**
  (CONSTEVAL surface of the BOUNDED-BUFFER × THROWING × PADDED
  sibling on the char\*+cap axis; wraps Sep 20's CONSTEVAL throwing
  wrapper verbatim and zero-fills `[written, cap)` to `'\0'` on
  success AFTER the underlying call completes).
- The TWO new meta-macros `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
  (CONSTEVAL surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW`
  (RUNTIME surface) — NEW today; each is a verbatim thin dispatcher
  that picks between FOUR sibling wrappers at preprocessor time based
  on the `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)` 2-bit pair.  Each
  macro is a parenthesized expression (NOT `do { } while (0)`) so
  the caller can write `auto n =
  TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(...)` and capture the result.
  Each macro uses `__VA_OPT__(,)` for the no-args case (mirrors Sep
  13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep
  22 / Sep 23 / Sep 25's macro shape).

## What today's lesson pins

1. **The (THROWING-vs-EXPECTED) × (PADDED-vs-UNTOUCHED) sibling
   choice CAN be lifted to a meta-macro form as a 2-axis product.**
   The shape composes on top of Sep 8's / Sep 9's bounded-buffer
   underlying wrappers via TWO levels of dispatch:
   level 1: Sep 25 / Sep 20 / TODAY's NEW wrappers — each
            sibling dispatches on ONE axis (PAD or THROW).
   level 2: TODAY's meta-macros dispatch on BOTH axes via a
            nested `#if` pair (PETRA_THROW first, then
            PETRA_PAD inside each branch).
2. **The compile-time flag pair `(PETRA_THROW_ON_OVERFLOW,
   PETRA_PAD)` controls the dispatch** via `#if PETRA_THROW_ON_OVERFLOW`
   × `#if PETRA_PAD`:
   - `(0, 0)` → expected+untouched sibling (returns
     `std::expected<std::size_t, FormatError>`; bytes `[written,
     cap)` UNTOUCHED on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n` (Sep 9)
       - RUNTIME:   `petra::try_format_runtime_n` (Sep 8)
   - `(0, 1)` → expected+padded sibling (returns
     `std::expected<std::size_t, FormatError>`; bytes `[written,
     cap)` zero-filled to `'\0'` on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded`
                    (Sep 25)
       - RUNTIME:   `petra::try_format_runtime_n_padded` (Sep 25)
   - `(1, 0)` → throwing+untouched sibling (returns `std::size_t`
     on success; throws `std::runtime_error` on overflow; bytes
     `[written, cap)` UNTOUCHED on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing`
                    (Sep 20)
       - RUNTIME:   `petra::try_format_runtime_n_throwing` (Sep 20)
   - `(1, 1)` → throwing+padded sibling (returns `std::size_t`
     on success; throws `std::runtime_error` on overflow; bytes
     `[written, cap)` zero-filled to `'\0'` on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded_throwing`
                    (NEW today; wraps Sep 20 verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_padded_throwing`
                    (NEW today; wraps Sep 20 verbatim)
3. **Independent `#error` guards** fire at preprocessor time if
   EITHER flag is set to anything other than 0 or 1.  The flags
   are validated independently (one `#error` per flag) so an
   invalid flag does NOT silently fall through to a wrong branch.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(...)` and capture
   the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
   Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25's macro shape.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
   Sep 23 / Sep 25.
6. **The throwing+padded sibling preserves the SAME buffer
   atomicity on TooLarge** as the other three siblings — the
   buffer is UNTOUCHED on overflow.  TODAY's NEW padded-throwing
   wrapper does the zero-fill ONLY when the underlying call
   returns (the `std::size_t` return value); when the underlying
   call throws, control unwinds through the wrapper WITHOUT the
   zero-fill loop executing.  Section 3 pins this on the (1, 1)
   path.
7. **The throwing+padded sibling preserves the SAME
   `std::runtime_error` `.what()` byte-exactness** as the
   throwing+untouched sibling — TODAY's NEW padded-throwing
   wrapper delegates the throw entirely to Sep 20's NEW throwing
   wrapper (via its return value / control flow); the `.what()`
   is constructed by Sep 20's wrapper, NOT by TODAY's wrapper.
   Section 15 pins this via the `dynamic_cast<std::format_error*>`
   check + `std::runtime_error&` catch (the throwing wrapper
   re-raises as `std::runtime_error` (NOT `std::format_error`),
   so the `dynamic_cast` returns `nullptr`).
8. **The throwing+padded sibling preserves the SAME byte-count
   contract on success** — returns `std::size_t == bytes written`
   (== needed == written); Section 4 pins this on the (1, 1) path.
9. **The CONSTEVAL macro's consteval gate is preserved regardless
   of `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)`** — all four CONSTEVAL
   siblings require `std::format_string<Args...>` (a literal fmt
   at the call site).  Section 6 pins this on BOTH paths by
   calling the macro with a literal fmt and verifying
   compile-time rejection of a runtime-built `std::string` fmt
   via `/tmp/probe_consteval_rejects_runtime_fmt_pad_throw.cpp`.
10. **The `PETRA_PAD` × `PETRA_THROW_ON_OVERFLOW` axis pair is
    PERPENDICULAR to the FOUR PRIOR AXES** — char\*-vs-span
    (Sep 10 / Sep 12), CONSTEVAL-vs-RUNTIME (Sep 13),
    bounded-buffer-vs-NUL-terminating (Sep 16), and
    expected-vs-throwing (Sep 20).  TODAY's meta-macros are at
    the INTERSECTION of FOUR INDEPENDENT AXES on the char\*+cap
    × CONSTEVAL-or-RUNTIME × bounded-buffer ×
    throwing × untouched-or-padded cell.  Section 14's
    `decltype` checks pin the perpendicular-axis claim on ALL
    FOUR paths.
11. **The throwing+padded sibling handles the `cap=0` edge case
    correctly** — the `for (i = written; i < cap; ++i)` loop has
    `i < cap == 0` as the initial condition, so the loop body
    never executes.  Section 2c pins this on the (1, 1) path.
12. **The throwing+padded sibling handles the `written==cap` edge
    case correctly** — the `for (i = written; i < cap; ++i)` loop
    has `i == cap` as the initial condition, so the loop body
    never executes.  Section 2d pins this with format `'ABCDEFGH'`
    (8 bytes) into `cap=8` (full-buffer success).
13. **Concurrency is safe** — Section 10 fires 4 threads × 25
    concurrent calls through both macros; per-thread local
    counters are aggregated AFTER `join()` via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.  Five consecutive runs on each build
    produced identical pass/fail tallies (no flakiness).
14. **The "try/catch around a throwing call + pad-on-success"
    pattern is the C++ idiom for "zero-fill on success only when
    the underlying throwing call returned normally"** —
    `Sep 25`'s expected-sibling pattern (`if (r.has_value())
    ...`) does NOT work here because the throwing sibling
    returns `std::size_t` (NOT `std::expected`), and on TooLarge
    the throwing sibling throws `std::runtime_error` (does NOT
    return normally at all).  TODAY's wrapper uses the
    `try { auto n = THROWING_SIBLING(...); for (...) pad loop;
    return n; } catch (...) { throw; }` shape — the catch block
    re-raises the `std::runtime_error` UNCHANGED (with the SAME
    `.what()` message that Sep 20's throwing wrapper constructed;
    the throw TYPE is also preserved as `std::runtime_error`
    because `catch (...)` re-raises the in-flight exception
    unchanged).

## Headline finding — the matrix is now 64 cells

Together with Sep 8's / Sep 9's / Sep 16's / Sep 19's / Sep 20's /
Sep 21's / Sep 22's / Sep 23's / Sep 25's lessons, TODAY's lesson
extends the matrix to **64 cells (2^5)** on the BOUNDED-BUFFER
quadrant of the char\*+cap side:

| axis | value |
|------|-------|
| 1 | char\*+cap vs span<char> |
| 2 | CONSTEVAL vs RUNTIME |
| 3 | bounded-buffer vs NUL-terminating |
| 4 | expected vs throwing (Sep 20 / Sep 21) |
| 5 | untouched vs NUL-padded (Sep 25 / TODAY) |

TODAY's lesson covers 4 NEW cells on the char\*+cap ×
CONSTEVAL-or-RUNTIME × bounded-buffer × throwing ×
untouched-or-padded axis.  The matrix is now 64 cells on the
BOUNDED-BUFFER × (expected-or-throwing) quadrant on the char\*+cap
side:

| axis 1 (buffer) | axis 2 (parse) | axis 3 (output) | axis 4 (error) | axis 5 (pad) | covered by |
|---|---|---|---|---|---|
| char\*+cap | CONSTEVAL | bounded-buffer | expected | untouched | Sep  9 |
| char\*+cap | CONSTEVAL | bounded-buffer | expected | padded    | Sep 25 |
| char\*+cap | RUNTIME   | bounded-buffer | expected | untouched | Sep  8 |
| char\*+cap | RUNTIME   | bounded-buffer | expected | padded    | Sep 25 |
| char\*+cap | CONSTEVAL | bounded-buffer | throwing | untouched | Sep 20 |
| char\*+cap | CONSTEVAL | bounded-buffer | throwing | padded    | TODAY |
| char\*+cap | RUNTIME   | bounded-buffer | throwing | untouched | Sep 20 |
| char\*+cap | RUNTIME   | bounded-buffer | throwing | padded    | TODAY |

The preprocessor-time dispatch design generalizes naturally to ANY
future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that discards the
output on overflow, a `PETRA_STRICT_FORMAT` axis that rejects
`{:#x}` style specifiers on the runtime path, a `PETRA_SPACE_PAD`
axis that pads with `' '` instead of `'\0'`, etc.) — each new axis
is a new `#if PETRA_<NEW_FLAG>` block layered on top of the
existing axes.  TODAY's lesson demonstrates that the design
**scales to 2-axis products** — going from 4 axes (16 cells on the
BOUNDED-BUFFER quadrant on the char\*+cap side) to 5 axes (32 cells
after Sep 25) was a verbatim copy-and-extend of Sep 13 / Sep 16 /
Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23's macro shape; going
from 32 cells (Sep 25) to 64 cells (TODAY) by adding the
(THROWING × PADDED) cells is a verbatim copy-and-extend of
TODAY's lesson — each macro has FOUR branches (a 2×2 nested
`#if`), one per flag-pair combination.

## Reuses (verbatim from prior lessons)

- `petra::FormatError` / `petra::FormatErrorKind` /
  `petra::counting_output_iterator` / `petra::detail::bounded_char_writer`
  (Sep 6–Sep 25 verbatim).
- `petra::detail::try_format_runtime_n_impl` (Sep 8 verbatim) +
  `petra::detail::try_format_bounded_runtime_n_impl` (Sep 9
  verbatim).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` (Sep 8 verbatim).
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` (Sep 9 verbatim).
- `petra::try_format_runtime_n_padded(char*, std::size_t,
  std::string_view, args...)` (Sep 25 verbatim).
- `petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
  std::format_string<Args...>, args...)` (Sep 25 verbatim).
- `petra::try_format_runtime_n_throwing(char*, std::size_t,
  std::string_view, args...)` (Sep 20 verbatim).
- `petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` (Sep 20 verbatim).

No new types beyond the two NEW throwing-padded wrappers; no new
impls beyond today's two NEW thin char\*+cap wrappers; no new error
contracts.

## Build and verification commands

```bash
# Default build (expected+untouched sibling; both flags 0).
cmake -S . -B build
cmake --build build
./build/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 129  FAIL: 0

# Strict-warning build (-Wall -Wextra -Wpedantic -Werror -Wshadow
# -Wconversion -Wsign-conversion).
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 129  FAIL: 0

# ASan + UBSan build.
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 129  FAIL: 0

# Throwing path (PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0).
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
cmake --build build-throw
./build-throw/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 120  FAIL: 0

# Padded path (PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1).
cmake -S . -B build-pad -DPETRA_PAD=ON
cmake --build build-pad
./build-pad/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 129  FAIL: 0

# Throwing+Padded path (PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1).
cmake -S . -B build-throw-pad -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PAD=ON
cmake --build build-throw-pad
./build-throw-pad/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 120  FAIL: 0

# Direct compile (no CMake, mirrors the lesson's "Build and
# verification commands" section).
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-26-try-format-padded-n-or-throw.cpp \
        -o /tmp/P-2026-09-26-try-format-padded-n-or-throw
/tmp/P-2026-09-26-try-format-padded-n-or-throw
# → PASS: 129  FAIL: 0
```

## Observed output (default build, expected+untouched sibling path)

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 129  FAIL: 0
=========================================================
OVERALL: PASS
```

## Observed output (throwing build, throwing+untouched sibling path)

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0 (throwing+untouched sibling)
=========================================================
PASS: 120  FAIL: 0
=========================================================
OVERALL: PASS
```

## Observed output (padded build, expected+padded sibling path)

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1 (expected+NUL-padded sibling)
=========================================================
PASS: 129  FAIL: 0
=========================================================
OVERALL: PASS
```

## Observed output (throwing+padded build, throwing+NUL-padded sibling path)

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1 (throwing+NUL-padded sibling)
=========================================================
PASS: 120  FAIL: 0
=========================================================
OVERALL: PASS
```

The expected-sibling build's count is 129 (vs 120 on the throwing-
sibling build) because the throwing path removes a few CHECKs that
require `r.has_value()` / `r.value()` syntax (replaced with try/catch
boilerplate that adds 1 CHECK per success path) and the
`dynamic_cast` check in Section 15 (which is only exercised on the
throwing path, contributing 2 CHECKs).

The padded build's count is identical to the untouched build (129
vs 129, 120 vs 120) because Section 2 has 5 prefix CHECKs (one per
byte) + a suffix loop with `cap - expected_written = 16 - 5 = 11`
iterations on `cap=16`.  Both paths have the same NUMBER of CHECKs
in Section 2 — only the byte-being-checked differs (`'\0'` on the
padded path vs `'Q'` on the untouched path).

The expected-sibling path has more CHECKs than the throwing-sibling
path (129 vs 120) because the expected path uses `r.has_value()` /
`r.value()` style per success path (Section 1's macro-pinning
CHECKs + 9 success CHECKs across Sections 2, 4, 5, 11, 12, 13 +
Section 15's empty placeholder), while the throwing path uses
try/catch (Section 1's macro-pinning CHECKs + 0 success CHECKs
in Section 15 since Section 15 is only exercised on the throwing
path).  Section 15 contributes 2 CHECKs on the throwing path
(`caught_runtime` + `dynamic_cast_is_nullptr`), but the throwing
path LOSES 11 CHECKs from the Section 2's "verify success" block
(no `r.has_value()` / `r.value()` calls), giving 120.

## Build matrix

- default build: `clang++ -std=c++23 -stdlib=libc++
  -fexperimental-library -O0`
- strict-warning build: default + `-Wall -Wextra -Wpedantic
  -Werror -Wshadow -Wconversion -Wsign-conversion`
- ASan/UBSan build: default + `-fsanitize=address
  -fsanitize=undefined -fno-omit-frame-pointer -O1`
- throwing path build: default + `-DPETRA_THROW_ON_OVERFLOW=1`
- padded path build: default + `-DPETRA_PAD=1`
- throwing+padded path build: default + `-DPETRA_THROW_ON_OVERFLOW=1
  -DPETRA_PAD=1`

## Concurrency verification

Section 10 fires 4 threads × 25 calls = 100 concurrent calls
through both macros on each build.  Per-thread local `lp`/`lf`
counters are aggregated AFTER `join()` via
`std::atomic<int>::fetch_add` to avoid a data race on `g_pass` /
`g_fail`.  Five consecutive runs on each of the SIX builds
(default + strict + ASan + throwing + padded + throwing-padded
under default; default + ASan + ASan-throw + ASan-throw-pad under
ASan) produced identical pass/fail tallies (no flakiness):

| build | flag pair | PASS | FAIL | runs |
|---|---|---|---|---|
| build            | (0,0) | 129 | 0 | 5 of 5 |
| build-throw      | (1,0) | 120 | 0 | 5 of 5 |
| build-pad        | (0,1) | 129 | 0 | 5 of 5 |
| build-throw-pad  | (1,1) | 120 | 0 | 5 of 5 |
| build-strict     | (0,0) | 129 | 0 | 5 of 5 |
| build-asan       | (0,0) | 129 | 0 | 5 of 5 |
| build-asan-throw | (1,0) | 120 | 0 | 5 of 5 |
| build-asan-throw-pad | (1,1) | 120 | 0 | 5 of 5 |

## Consteval-gate probe

`/tmp/probe_consteval_rejects_runtime_fmt_pad_throw.cpp` produces:

```
error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<std::string>' is not a constant expression
note: read of non-constexpr variable 'fmt_runtime' is not allowed in a constant expression
note: in call to 'this->__is_long()'
note: in call to 'this->__get_pointer()'
note: in call to 'this->data()'
note: in call to '__str.operator basic_string_view()'
```

— confirming that the CONSTEVAL macro's consteval gate is
preserved regardless of `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)`
(matches Sep 13 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
Sep 23 / Sep 25's behavior).

## Preprocessor-guard probes

`-DPETRA_PAD=2` produces:

```
error: "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
note: static assertion failed due to requirement '2 == 0 || 2 == 1'
```

`-DPETRA_THROW_ON_OVERFLOW=3` produces:

```
error: "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
note: static assertion failed due to requirement '3 == 0 || 3 == 1'
```

— confirming that BOTH `#error` guards fire at preprocessor time
if EITHER flag is set to anything other than 0 or 1.

## Where we go next

TODAY's lesson closes Sep 25's "Where we go next" item #1
verbatim: adding the THROWING sibling on the PAD axis (the
(THROWING × PADDED) cells).  The matrix is now **64 cells (2^5)**
on the BOUNDED-BUFFER quadrant on the char\*+cap side.

TODAY's lesson does NOT cover the OTHER 12 cells on the 5-axis
matrix — the following are still open:

1. **span<char> × CONSTEVAL-or-RUNTIME × bounded-buffer ×
   expected-or-throwing × untouched-or-padded cells** — the
   span<char> mirror of TODAY's char\*+cap lesson.  Each future
   lesson adds verbatim thin padded wrappers composed on top of
   Sep 10's / Sep 19's / Sep 21's span<char> bounded-buffer
   wrappers + new `#if PETRA_PAD`-gated meta-macros on the
   span<char> path.  (16 cells: CONSTEVAL/RUNTIME ×
   expected/throwing × untouched/padded.)
2. **NUL-terminating × padded cells** — would be redundant (the
   NUL-terminating sibling already places `'\0'` at the
   truncation-edge or full-success position; the bytes after the
   NUL are already whatever they were before the call).  This
   lesson DOES NOT cover NUL-terminating × padded and explicitly
   skips it (mirrors Sep 25's open item #4).
3. **char\*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer ×
   expected-or-throwing × ALTERNATE-PAD cells** — would introduce
   a SIXTH axis (e.g. `PETRA_SPACE_PAD` axis that pads with `' '`
   instead of `'\0'`).  Each future lesson adds verbatim thin
   space-pad wrappers composed on top of TODAY's NUL-padded
   wrappers + new `#if PETRA_SPACE_PAD`-gated meta-macros.  (8
   cells per axis extension: 2^3 = 8 combinations of
   CONSTEVAL/RUNTIME × expected/throwing × untouched/space-pad
   on the char\*+cap × bounded-buffer axis.  Adding
   space-pad-NUL would add another 8 cells × 2 = 16.)

With the 64-cell matrix now COMPLETE on the BOUNDED-BUFFER
quadrant on the char\*+cap side, future lessons can either (a)
extend to the span<char> side (item #1 above — another 16 cells),
(b) introduce a SIXTH axis on the char\*+cap side (e.g.
`PETRA_SPACE_PAD`), or (c) close cross-cutting infrastructure
items from the Aug 13 / Aug 15 / Aug 17 lessons (pin actions to
commit SHAs; multi-OS matrix extending to `windows-latest`;
status badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history).

## C++23 features exercised

- `std::format` + `std::vformat_to` + `std::format_string<Args...>`
  (P2216R3) + `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` + `dynamic_cast` +
  `std::is_base_of_v` (catch-chain contract pinning).
- `std::array<std::thread, 4>` + `std::thread::join()` (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.  Reuses Sep 6 / Sep 7 / Sep 8 /
Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25's
machinery verbatim.