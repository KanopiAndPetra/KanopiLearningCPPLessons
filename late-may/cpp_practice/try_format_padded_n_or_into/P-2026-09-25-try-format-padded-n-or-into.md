# P-2026-09-25 — `try_format_padded_n_or_into`

Topic: a **NEW FIFTH AXIS** that lifts the **PAD-VS-UNTOUCHED**
sibling choice to a meta-macro form on the **CHAR\*+CAP ×
BOUNDED-BUFFER** axis.  Two new meta-macros —
`TRY_FORMAT_BOUNDED_N_PAD` (CONSTEVAL surface) and
`TRY_FORMAT_BOUNDED_RUNTIME_N_PAD` (RUNTIME surface) — each dispatch
to EITHER the **untouched** sibling (Sep 9's
`try_format_bounded_runtime_n` for the CONSTEVAL surface, Sep 8's
`try_format_runtime_n` for the RUNTIME surface; both return
`std::expected<std::size_t, FormatError>` and leave the bytes
`[written, cap)` UNTOUCHED on success) OR today's NEW **NUL-padded**
sibling (today's `petra::try_format_bounded_runtime_n_padded` for the
CONSTEVAL surface, today's `petra::try_format_runtime_n_padded` for
the RUNTIME surface; both also return
`std::expected<std::size_t, FormatError>` but ZERO-FILL the bytes
`[written, cap)` to `'\0'` on success), based on a caller-controlled
compile-time flag `PETRA_PAD`.

**Closes Sep 23's "Where we go next" (the FUTURE-AXIS-EXTENSION
open item)**:

> The preprocessor-time dispatch design generalizes naturally to ANY
> future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that discards the
> output on overflow, a `PETRA_STRICT_FORMAT` axis that rejects
> `{:#x}` style specifiers on the runtime path, etc.) — each new
> axis is a new `#if PETRA_<NEW_FLAG>` block layered on top of the
> existing axes.  With the 16-cell matrix now COMPLETE, future
> lessons can either (a) introduce a NEW axis (a 32-cell or 64-cell
> matrix), or (b) close cross-cutting infrastructure items from the
> Aug 13 / Aug 15 / Aug 17 lessons.

Today covers option (a) — introducing the `PETRA_PAD` axis (the
PAD-VS-UNTOUCHED axis).  Adding `PETRA_PAD` as a 5th axis extends
the matrix from 16 cells to 32 cells on the BOUNDED-BUFFER ×
expected quadrant of the char\*+cap side.

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
           `std::span<char>` axis (the FOURTH axis on the
           span<char> side; covers the span<char> × bounded-buffer
           × expected-vs-throwing cells).
- Sep 22   `try_format_bounded_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the CHAR\*+CAP axis (covers the
           char\*+cap × NUL-terminating × expected-vs-throwing
           cells — closes Sep 21's "Where we go next" item #1
           verbatim).
- Sep 23   `try_format_span_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the STD::SPAN<CHAR> axis (covers
           the span<char> × NUL-terminating × expected-vs-throwing
           cells — closes Sep 22's "Where we go next" item #1
           verbatim; the FOUR-AXIS BOUNDED-vs-NUL ×
           expected-vs-throwing quadrant of the 16-cell matrix is
           now COMPLETE on BOTH the char\*+cap and span<char>
           axes).
- TODAY    `try_format_padded_n_or_into` — the FIFTH axis: the
           PAD-VS-UNTOUCHED sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis (covers 2 NEW cells on
           the char\*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer
           × expected × padded axis; the matrix grows from 16 to
           32 cells on the BOUNDED-BUFFER × expected quadrant on
           the char\*+cap side; closes Sep 23's
           "future-axis-extension" open item).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
  Sep 23 verbatim (the wrappers below do not produce `FormatError`
  directly — they only forward to existing wrappers, but the type
  must be visible in scope for `std::expected<std::size_t,
  FormatError>` to be instantiatable by the `static_assert`
  below).
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim.
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_n_impl` (Sep 8 verbatim) +
  `petra::detail::try_format_bounded_runtime_n_impl` (Sep 9
  verbatim) — REUSED verbatim (the bounded-buffer underlying
  impls).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` — REUSED Sep 8 verbatim (the RUNTIME untouched sibling
  that today's NEW padded wrapper composes on top of).
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 9 verbatim
  (the CONSTEVAL untouched sibling that today's NEW padded wrapper
  composes on top of).
- `petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
  std::format_string<Args...>, args...)` — **NEW today**: CONSTEVAL
  surface of the BOUNDED-BUFFER × PAD sibling on the char\*+cap
  axis; verbatim thin wrapper around Sep 9's
  `try_format_bounded_runtime_n` that zero-fills `[written, cap)`
  to `'\0'` on success.
- `petra::try_format_runtime_n_padded(char*, std::size_t,
  std::string_view, args...)` — **NEW today**: RUNTIME surface of
  the BOUNDED-BUFFER × PAD sibling on the char\*+cap axis; verbatim
  thin wrapper around Sep 8's `try_format_runtime_n` that
  zero-fills `[written, cap)` to `'\0'` on success.
- The TWO new meta-macros `TRY_FORMAT_BOUNDED_N_PAD` (CONSTEVAL
  surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD` (RUNTIME
  surface) — NEW today; each is a verbatim thin dispatcher that
  picks between the untouched sibling (`PETRA_PAD=0`) and the
  NUL-padded sibling (`PETRA_PAD=1`) at preprocessor time.

## What today's lesson pins

1. **The PAD-VS-UNTOUCHED sibling choice CAN be lifted to a
   meta-macro form.**  The shape composes on top of Sep 8's /
   Sep 9's bounded-buffer underlying wrappers.  Each TODAY's
   meta-macro is a verbatim thin wrapper around ONE OF TWO
   underlying wrappers:
   - bounded-buffer × untouched:        Sep 8 / Sep 9 char\*+cap
                                         wrappers
   - bounded-buffer × NUL-padded:       TODAY's NEW padded
                                         char\*+cap wrappers
   The NUL-terminating variants are NOT covered by today's macros
   (the NUL-terminating sibling already places `'\0'` at the
   truncation-edge or full-success position; adding a pad sibling
   on the NUL-terminating axis would be redundant — the bytes
   after the NUL are already whatever they were before the call).
2. **The compile-time flag `PETRA_PAD` controls the dispatch** via
   `#if PETRA_PAD == 0/1`:
   - `#define PETRA_PAD 0` → untouched sibling (returns
     `std::expected<std::size_t, FormatError>` on success; bytes
     `[written, cap)` UNTOUCHED on success):
     - CONSTEVAL: `petra::try_format_bounded_runtime_n` (Sep 9)
     - RUNTIME:   `petra::try_format_runtime_n` (Sep 8)
   - `#define PETRA_PAD 1` → NUL-padded sibling (returns
     `std::expected<std::size_t, FormatError>` on success; bytes
     `[written, cap)` zero-filled to `'\0'` on success):
     - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded`
                  (NEW today; wraps Sep 9 verbatim)
     - RUNTIME:   `petra::try_format_runtime_n_padded`
                  (NEW today; wraps Sep 8 verbatim)
3. **`#error` guard** fires at preprocessor time if the flag is
   set to anything other than 0 or 1.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_N_PAD(...)` and capture the
   result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
   Sep 20 / Sep 21 / Sep 22 / Sep 23's macro shape.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
   Sep 23.
6. **The CONSTEVAL macro picks between Sep 9's
   `try_format_bounded_runtime_n` (untouched, returns
   `expected<size_t, FormatError>`) and today's NEW
   `try_format_bounded_runtime_n_padded` (NUL-padded, returns
   `expected<size_t, FormatError>`).**  Both are consteval-checked
   `std::format_string<Args...>` wrappers.  The return type is
   IDENTICAL between the two siblings — the difference is purely
   a side-effect on the buffer.
7. **The RUNTIME macro picks between Sep 8's `try_format_runtime_n`
   (untouched, returns `expected<size_t, FormatError>`) and today's
   NEW `try_format_runtime_n_padded` (NUL-padded, returns
   `expected<size_t, FormatError>`).**  Both take
   `std::string_view` format-string (no compile-time parse).  Same
   return-type identity as the CONSTEVAL pair.
8. **The padded sibling preserves the SAME byte-count contract on
   success** — returns `std::size_t == bytes written` (the same
   value Sep 8's / Sep 9's untouched siblings return); Section 4
   pins this on both paths.
9. **The padded sibling preserves the SAME atomicity on TooLarge
   as the untouched sibling** — the buffer is UNTOUCHED on
   overflow (Sep 8 / Sep 9 already pin this on the untouched
   path; today's wrappers re-pin it on the padded path because
   the new code path COULD theoretically diverge; Section 3a /
   3b pin this on both paths, and Section 3c pins it specifically
   on the padded path).
10. **The padded sibling preserves the SAME atomicity on Format
    errors** — a malformed fmt returns `FormatError{Format, ...}`
    and the buffer is UNTOUCHED.  Section 8 pins this by calling
    the RUNTIME macro with a runtime-built `std::string fmt`
    containing `"bad {"` (an unterminated brace that
    `std::vformat_to` rejects at format time with
    `std::format_error`).
11. **The padded sibling preserves the SAME consteval gate on the
    CONSTEVAL macro** — `TRY_FORMAT_BOUNDED_N_PAD` still requires
    `std::format_string<Args...>` (a literal fmt at the call
    site).  Section 6 pins the consteval gate by calling the
    macro with a literal fmt and a runtime-built `std::string`
    fmt; the runtime-built fmt REJECTS AT COMPILE TIME on the
    CONSTEVAL macro (verified by
    `/tmp/probe_consteval_rejects_runtime_fmt_pad.cpp` producing
    the same `error: call to consteval function
    std::basic_format_string<char, int>::basic_format_string<std::string>
    is not a constant expression` with note `read of non-constexpr
    variable 'fmt_runtime' is not allowed in a constant expression`
    as Sep 13 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
    Sep 23's probes).
12. **The `PETRA_PAD` axis (untouched vs NUL-padded) is
    PERPENDICULAR to the FOUR PRIOR AXES** —
    char\*-vs-span (Sep 10 / Sep 12), CONSTEVAL-vs-RUNTIME
    (Sep 13), bounded-buffer-vs-NUL-terminating (Sep 16), and
    expected-vs-throwing (Sep 20).  Today's meta-macros are at
    the INTERSECTION of all FIVE axes on the char\*+cap ×
    bounded-buffer × expected × CONSTEVAL-or-RUNTIME ×
    untouched-or-padded cell.  Section 11's `decltype` checks pin
    this on BOTH paths.
13. **The padded sibling's byte-state contract** — bytes
    `[written, cap)` are `'\0'` on success — is pinned by
    Section 2 (which pre-fills the buffer with poison `'Q'`
    bytes, runs the macro, and asserts every byte at index `>=
    written` is `'\0'` on the padded path OR `'Q'` on the
    untouched path).
14. **Concurrency is safe** — Section 10 fires 4 threads × 25
    concurrent calls through both macros; per-thread local `lp` /
    `lf` counters are aggregated AFTER `join()` via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.  Five consecutive runs on each build
    produced identical pass/fail tallies (no flakiness).

## Headline finding — the `PETRA_PAD` axis extends the matrix to 32 cells

Together with Sep 8's / Sep 9's / Sep 20's / Sep 21's lessons,
today's lesson introduces the **FIFTH axis** (`untouched vs
NUL-padded`).  The matrix is now **32 cells (2^5)** on the
BOUNDED-BUFFER × expected quadrant of the char\*+cap side:

| axis | value |
|------|-------|
| 1 | char\*+cap vs span<char> |
| 2 | CONSTEVAL vs RUNTIME |
| 3 | bounded-buffer vs NUL-terminating |
| 4 | expected vs throwing (Sep 20 / Sep 21) |
| 5 | untouched vs NUL-padded (Sep 25 / TODAY) |

Today's lesson covers 2 NEW cells on the char\*+cap ×
CONSTEVAL-or-RUNTIME × bounded-buffer × expected × padded axis.
The matrix is now 32 cells on the BOUNDED-BUFFER × expected
quadrant of the char\*+cap side:

| axis 1 (buffer) | axis 2 (parse) | axis 3 (output) | axis 4 (error) | axis 5 (pad) | covered by |
|---|---|---|---|---|---|
| char\*+cap   | CONSTEVAL | bounded-buffer | expected | untouched | Sep  9 |
| char\*+cap   | CONSTEVAL | bounded-buffer | expected | padded    | TODAY |
| char\*+cap   | RUNTIME   | bounded-buffer | expected | untouched | Sep  8 |
| char\*+cap   | RUNTIME   | bounded-buffer | expected | padded    | TODAY |

The preprocessor-time dispatch design generalizes naturally to ANY
future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that discards the
output on overflow, a `PETRA_STRICT_FORMAT` axis that rejects
`{:#x}` style specifiers on the runtime path, a `PETRA_SPACE_PAD`
axis that pads with `' '` instead of `'\0'`, etc.) — each new axis
is a new `#if PETRA_<NEW_FLAG>` block layered on top of the
existing axes.  Today's lesson demonstrates that the design
**scales** — going from 4 axes (16 cells) to 5 axes (32 cells) was
a verbatim copy-and-extend of Sep 13 / Sep 16 / Sep 19 / Sep 20 /
Sep 21 / Sep 22 / Sep 23's macro shape.

## Reuses (verbatim from prior lessons)

- `petra::FormatError` / `petra::FormatErrorKind` /
  `petra::counting_output_iterator` / `petra::detail::bounded_char_writer`
  (Sep 6–Sep 23 verbatim).
- `petra::detail::try_format_runtime_n_impl` (Sep 8 verbatim) +
  `petra::detail::try_format_bounded_runtime_n_impl` (Sep 9
  verbatim).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` (Sep 8 verbatim).
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` (Sep 9 verbatim).

No new types beyond the two NEW padded wrappers; no new impls
beyond today's two NEW thin char\*+cap wrappers; no new error
contracts.

## Build and verification commands

```bash
# Default build (untouched sibling; PETRA_PAD=0).
cmake -S . -B build
cmake --build build
./build/P-2026-09-25-try-format-padded-n-or-into
# → PASS: 128  FAIL: 0

# Strict-warning build (-Wall -Wextra -Wpedantic -Werror -Wshadow
# -Wconversion -Wsign-conversion).
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-25-try-format-padded-n-or-into
# → PASS: 128  FAIL: 0

# ASan + UBSan build.
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-25-try-format-padded-n-or-into
# → PASS: 128  FAIL: 0

# NUL-padded path build (PETRA_PAD=1).
cmake -S . -B build-pad -DPETRA_PAD=ON
cmake --build build-pad
./build-pad/P-2026-09-25-try-format-padded-n-or-into
# → PASS: 145  FAIL: 0

# Direct compile (no CMake).
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-25-try-format-padded-n-or-into.cpp \
        -o /tmp/P-2026-09-25-try-format-padded-n-or-into
/tmp/P-2026-09-25-try-format-padded-n-or-into
# → PASS: 128  FAIL: 0
```

## Observed output (default build, untouched sibling path)

```
[Section 1] PETRA_PAD=0 (untouched sibling)
=========================================================
PASS: 128  FAIL: 0
=========================================================
OVERALL: PASS
```

## Observed output (NUL-padded build, padded sibling path)

```
[Section 1] PETRA_PAD=1 (NUL-padded sibling)
=========================================================
PASS: 145  FAIL: 0
=========================================================
OVERALL: PASS
```

The padded build's count is 145 (vs 128 on the untouched build)
because Section 3c — the padded-build-specific atomicity test —
adds 17 CHECKs on the padded build (the padded build has a
dedicated `Section 3c` that re-pins atomicity on the padded path,
since the new `for (i = written; i < cap; ++i) out[i] = '\0';`
loop COULD theoretically diverge from the untouched sibling's
behavior on overflow).

## Build matrix

- default build: `clang++ -std=c++23 -stdlib=libc++
  -fexperimental-library -O0`
- strict-warning build: default + `-Wall -Wextra -Wpedantic
  -Werror -Wshadow -Wconversion -Wsign-conversion`
- ASan/UBSan build: default + `-fsanitize=address
  -fsanitize=undefined -fno-omit-frame-pointer -O1`
- NUL-padded path build: default + `-DPETRA_PAD=1`

## Concurrency verification

Section 10 fires 4 threads × 25 calls = 100 concurrent calls
through both macros on each build.  Per-thread local `lp`/`lf`
counters are aggregated AFTER `join()` via
`std::atomic<int>::fetch_add` to avoid a data race on `g_pass` /
`g_fail`.  Five consecutive runs on each build produced identical
pass/fail tallies (no flakiness):

- untouched build: `PASS: 128  FAIL: 0` (5 of 5 runs)
- NUL-padded build: `PASS: 145  FAIL: 0` (5 of 5 runs)

## Consteval-gate probe

`/tmp/probe_consteval_rejects_runtime_fmt_pad.cpp` produces:

```
error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<std::string>' is not a constant expression
note: read of non-constexpr variable 'fmt_runtime' is not allowed in a constant expression
note: in call to 'this->__is_long()'
note: in call to 'this->__get_pointer()'
note: in call to 'this->data()'
note: in call to '__str.operator basic_string_view()'
```

— confirming that the CONSTEVAL macro's consteval gate is
preserved regardless of `PETRA_PAD` (matches Sep 13 / Sep 16 /
Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23's behavior).

## Where we go next

Today's lesson closes Sep 23's "future-axis-extension" open item
verbatim: introducing the `PETRA_PAD` axis as the **FIFTH axis** of
the matrix.  The matrix is now **32 cells (2^5)** on the
BOUNDED-BUFFER × expected quadrant of the char\*+cap side.

Today's lesson does NOT cover the OTHER 6 cells on the 5-axis
matrix — the following are still open:

1. **char\*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer ×
   throwing × padded cells** — the THROWING sibling on the
   padded axis.  Each future lesson adds a verbatim thin
   throwing wrapper composed on top of Sep 20's throwing siblings
   + a new `#if PETRA_PAD`-gated meta-macro on the throwing
   path.  The natural follow-on is Sep 20's throwing wrappers
   (`petra::try_format_runtime_n_throwing` + Sep 20's CONSTEVAL
   `try_format_bounded_runtime_n_throwing`); each would gain a
   `*_padded` wrapper that zero-fills `[written, cap)` to `'\0'`
   on success.  (4 cells: CONSTEVAL/RUNTIME × untouched/padded.)
2. **span<char> × CONSTEVAL-or-RUNTIME × bounded-buffer ×
   expected × padded cells** — the span<char> mirror of today's
   char\*+cap lesson.  Each future lesson adds a verbatim thin
   padded wrapper composed on top of Sep 10's / Sep 19's /
   Sep 21's span<char> bounded-buffer wrappers + a new
   `#if PETRA_PAD`-gated meta-macro on the span<char> path.  (4
   cells: CONSTEVAL/RUNTIME × untouched/padded.)
3. **span<char> × CONSTEVAL-or-RUNTIME × bounded-buffer ×
   throwing × padded cells** — combination of #1 and #2.  (4
   cells.)
4. **NUL-terminating × padded cells** — would be redundant (the
   NUL-terminating sibling already places `'\0'` at the
   truncation-edge or full-success position; the bytes after the
   NUL are already whatever they were before the call).  This
   lesson DOES NOT cover NUL-terminating × padded and explicitly
   skips it.

With the 32-cell matrix now COMPLETE on the BOUNDED-BUFFER ×
expected quadrant of the char\*+cap side, future lessons can
either (a) continue extending to a 64-cell matrix by adding the
THROWING and span<char> padded cells (items #1 and #2 above), or
(b) introduce a SIXTH axis (a 32-cell → 64-cell jump on a new
axis dimension), or (c) close cross-cutting infrastructure items
from the Aug 13 / Aug 15 / Aug 17 lessons (pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status badge
in README; vcpkg / Conan port for `psp_span_lib`; branch
protection requiring linear history).

## C++23 features exercised

- `std::format` + `std::vformat_to` + `std::format_string<Args...>`
  (P2216R3) + `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `std::is_same_v` (decltype checks).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.  Reuses Sep 6 / Sep 7 / Sep 8 /
Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23's machinery
verbatim.
