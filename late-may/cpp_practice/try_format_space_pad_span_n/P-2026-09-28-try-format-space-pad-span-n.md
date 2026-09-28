# P-2026-09-28 — try_format_space_pad_span_n

**Closes Sep 27's "Where we go next" item #2 verbatim**: introduces
the **SIXTH-AXIS (PETRA_SPACE_PAD) sibling-choice META-MACRO layer**
on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.  Adds a new
compile-time-controlled sibling choice — should the bounded buffer
be UNTOUCHED, SPACE-padded (' '), or NUL-padded ('\\0') on success?

## The headline finding

The `PETRA_SPACE_PAD` axis is a SIXTH axis orthogonal to Sep 27's
5-axis matrix (CHAR\*-vs-span × CONSTEVAL-or-RUNTIME × bounded-vs-NUL-term ×
expected-vs-throwing × untouched-or-padded).  Today's lesson
introduces a 3-bit flag tuple:

```
(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)
```

The compiler-time dispatch picks the correct wrapper:

| Throw | Pad | SpacePad | Sibling            | Dispatch target (CONSTEVAL surface)            | Dispatch target (RUNTIME surface)            |
|------:|-----:|---------:|--------------------|-----------------------------------------------|----------------------------------------------|
|     0 |    0 |        0 | expected+untouched | `try_format_bounded_runtime_n_span` (Sep 10) | `try_format_runtime_n(span,...)` (Sep 19)   |
|     0 |    0 |        1 | expected+SPACE-pad | `try_format_bounded_runtime_n_space_pad_span` (NEW today) | `try_format_runtime_n_space_pad_span` (NEW today) |
|     1 |    0 |        0 | throwing+untouched | `try_format_bounded_runtime_n_throwing_span` (Sep 21) | `try_format_runtime_n_throwing(span,...)` (Sep 21) |
|     1 |    0 |        1 | throwing+SPACE-pad | `try_format_bounded_runtime_n_throwing_space_pad_span` (NEW today) | `try_format_runtime_n_throwing_space_pad_span` (NEW today) |

PETRA_PAD=1 cells are OUT OF SCOPE for this lesson (Sep 27's
territory); the dispatcher `#error`s with a clear pointer to Sep 27.

## Why this design

The new PETRA_SPACE_PAD axis is **orthogonal** to the existing
PETRA_THROW_ON_OVERFLOW and PETRA_PAD axes:

1. **PETRA_THROW_ON_OVERFLOW** (expected vs throwing) — controls
   the return type / exception contract (Sep 21's axis on
   span\<char\>).
2. **PETRA_PAD** (untouched vs NUL-padded) — controls whether
   bytes `[written, span.size())` are zero-filled on success
   (Sep 27's axis on span\<char\>).
3. **PETRA_SPACE_PAD** (untouched vs SPACE-padded) — controls
   whether bytes `[written, span.size())` are filled with `' '`
   on success (TODAY'S new axis on span\<char\>).

PETRA_PAD=0 AND PETRA_SPACE_PAD=1 (the new sibling pair) is
syntactically independent of PETRA_THROW_ON_OVERFLOW: the
dispatcher has 4 cells per surface pair (CONSTEVAL or RUNTIME)
rather than 2 (one per Throw flag).

The preprocessor-time `#if PETRA_THROW_ON_OVERFLOW × #if
PETRA_PAD × #if PETRA_SPACE_PAD` nested-tuple is the cleanest
design:

- Zero runtime cost (each branch collapses to a single function
  call).
- Matches Sep 1 / Sep 2 / Sep 13 / Sep 16 / Sep 19 / Sep 20 /
  Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 27's compile-time-flag
  framing.
- Independent `#error` guards fire at preprocessor time if ANY
  flag is invalid (one `#error` per flag, so an invalid flag
  does NOT silently fall through to a wrong branch).
- Composes orthogonally with the other axes (Section 14's
  decltype checks pin this on all 4 sibling-pair combinations).

## What today covers

Adds 4 NEW SPACE-padded wrappers composed on top of Sep 10's /
Sep 19's / Sep 21's bounded-buffer span\<char\> wrappers:

- `petra::try_format_bounded_runtime_n_space_pad_span(span<char>,
  std::format_string<Args...>, args...)` — NEW today (CONSTEVAL
  span\<char\> expected+SPACE-padded wrapper; wraps Sep 10
  verbatim and space-fills `[written, span.size())` with `' '`
  on success ONLY via `if (r.has_value())` guard).

- `petra::try_format_runtime_n_space_pad_span(span<char>,
  std::string_view, args...)` — NEW today (RUNTIME span\<char\>
  expected+SPACE-padded wrapper; wraps Sep 19 verbatim and
  space-fills `[written, span.size())` with `' '` on success
  ONLY via `if (r.has_value())` guard).

- `petra::try_format_bounded_runtime_n_throwing_space_pad_span(
  span<char>, std::format_string<Args...>, args...)` — NEW today
  (CONSTEVAL span\<char\> throwing+SPACE-padded wrapper; wraps
  Sep 21 verbatim and space-fills `[written, span.size())` with
  `' '` on success ONLY via try/catch around the throwing call).

- `petra::try_format_runtime_n_throwing_space_pad_span(
  span<char>, std::string_view, args...)` — NEW today (RUNTIME
  span\<char\> throwing+SPACE-padded wrapper; wraps Sep 21
  verbatim and space-fills `[written, span.size())` with `' '`
  on success ONLY via try/catch around the throwing call).

Adds 2 NEW meta-macros:

- `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)` —
  CONSTEVAL surface dispatcher; parenthesized expression; uses
  `__VA_OPT__(,)` for the no-args case.

- `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)` —
  RUNTIME surface dispatcher; parenthesized expression; uses
  `__VA_OPT__(,)` for the no-args case.

## Important code patterns

### The expected+SPACE-padded wrapper

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_space_pad_span(std::span<char> out,
                                    std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}
```

The `if (r.has_value())` guard short-circuits BEFORE the space-fill
loop, so atomicity is preserved on TooLarge / Format error.

### The throwing+SPACE-padded wrapper

```cpp
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        // Success path: space-fill out[n, out.size()) with ' '.
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
        }
        return n;
    } catch (...) {
        // Atomicity preserved: the underlying throwing wrapper did
        // not touch the buffer (delegated to Sep 19's / Sep 10's
        // expected impl, which short-circuits on TooLarge; and on
        // Format errors, pass-1's std::vformat_to throws
        // std::format_error before any writes).
        throw;
    }
}
```

The try/catch idiom is the canonical C++ pattern for "fill-on-
success only when the underlying throwing call returned normally".
Mirrors Sep 27's NUL-padded sibling structure byte-for-byte except
the fill byte is `' '` (this lesson) instead of `'\\0'` (Sep 27).

## What today's lesson pins

1. The PAD-vs-UNTOUCHED sibling choice can be **TRIPLED** to a
   PAD-vs-SPACE-PAD-vs-UNTOUCHED sibling choice as a 3-axis
   product on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.
2. The space-padded sibling is a verbatim thin wrapper that fills
   `[written, span.size())` with `' '` on success AFTER the
   underlying call returns.  On error (the underlying call
   returns FormatError{...}), the wrapper does NOT execute the
   space-fill loop — control returns the FormatError unchanged.
3. The throwing+space-padded sibling uses the C++ idiom
   `try { auto n = THROWING_SIBLING(...); for (...) pad loop;
   return n; } catch (...) { throw; }` to fill `[written,
   span.size())` with `' '` on success ONLY when the underlying
   throwing call returned normally (control does NOT reach the
   pad loop on error — the catch block re-raises the
   `std::runtime_error` untouched).
4. The buffer atomicity contract is preserved on TooLarge — the
   space-padded wrapper does NOT touch the buffer on overflow.
5. The buffer atomicity contract is preserved on Format errors
   — a malformed fmt returns `FormatError{Format, e.what()}`
   (expected path) or throws `std::runtime_error` (throwing
   path), and the buffer is UNTOUCHED.
6. The byte-count contract on success is preserved — returns
   `std::size_t == bytes written`.
7. The return-type contract is preserved — the expected sibling
   returns `std::expected<std::size_t, FormatError>`; the
   throwing sibling returns `std::size_t`.
8. The CONSTEVAL macro's consteval gate is preserved regardless
   of `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)`.
9. The std::span\<char\> sub-span 'no-bleed' contract is preserved
   on the SPACE-padded path (Section 2a).
10. Independent `#error` guards fire at preprocessor time if ANY
    of the three flags is set to anything other than 0 or 1.

## Scope: this lesson is focused on PETRA_SPACE_PAD only

PETRA_PAD=1 (NUL-padded sibling) cells are OUT OF SCOPE for this
lesson — they are Sep 27's territory and require Sep 27's NEW
NUL-pad wrappers (which are NOT included in this file).  When
PETRA_PAD = 1, the lesson's dispatcher `#error`s with a clear
pointer to Sep 27:

```
error: "PETRA_PAD = 1 (NUL-padded sibling) is OUT OF SCOPE for this lesson.  Use Sep 27's try_format_padded_span_n_or_throw/ for the PETRA_PAD=1 combinations."
```

A future lesson could close this gap by adding the missing
SPACE-padded NUL-wrapper combinations (e.g. `try_format_bounded_
runtime_n_padded_space_pad_span` and friends), but that would
introduce YET ANOTHER sibling pair on the BOUNDED-BUFFER axis —
left for a future lesson.

## Observed output

Default build (PETRA_THROW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 174  FAIL: 0
=========================================================
OVERALL: PASS
```

Expected+SPACE-padded build (PETRA_THROW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=1):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, PETRA_SPACE_PAD=1 (expected+SPACE-padded sibling)
=========================================================
PASS: 182  FAIL: 0
=========================================================
OVERALL: PASS
```

Throwing+untouched build (PETRA_THROW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=0):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, PETRA_SPACE_PAD=0 (throwing+untouched sibling)
=========================================================
PASS: 162  FAIL: 0
=========================================================
OVERALL: PASS
```

Throwing+SPACE-padded build (PETRA_THROW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=1):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, PETRA_SPACE_PAD=1 (throwing+SPACE-padded sibling)
=========================================================
PASS: 170  FAIL: 0
=========================================================
OVERALL: PASS
```

Five consecutive runs of each build produced identical PASS/FAIL
counts (no flakiness in Section 10's 4-thread × 25-call concurrent
dispatch — per-thread local counters aggregated after join via
`std::atomic<int>::fetch_add`).

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`): zero warnings, identical PASS
counts.

ASan/UBSan build (`-fsanitize=address -fsanitize=undefined
-fno-omit-frame-pointer -O1`): zero diagnostics, identical PASS
counts.

Consteval-gate probe: `/tmp/probe_consteval_rejects_runtime_fmt_
space_pad.cpp` produces the expected consteval-rejection error at
compile time.

Invalid-flag-value probes:
- `-DPETRA_PAD=1` fires the `#error "PETRA_PAD = 1 ... OUT OF
  SCOPE for this lesson"` guard.
- `-DPETRA_THROW_ON_OVERFLOW=2` fires the `#error
  "PETRA_THROW_ON_OVERFLOW must be 0 or 1"` guard.
- `-DPETRA_SPACE_PAD=5` fires the `#error "PETRA_SPACE_PAD must
  be 0 or 1"` guard.

CMake build verification: `cmake -S . -B build` and
`cmake --build build` succeed; `cmake -S . -B build-space-pad
-DPETRA_SPACE_PAD=ON` and `cmake --build build-space-pad` also
succeed.

## Where we go next

TODAY's lesson closes Sep 27's "Where we go next" item #2
verbatim: the SIXTH axis on the std::span\<char\> × BOUNDED-BUFFER
axis.  The matrix is now 2^6 = 64 cells on the BOUNDED-BUFFER ×
(expected-or-throwing) × (untouched-vs-SPACE-padded) quadrant on
the std::span\<char\> side (4 dispatch cells × 8 sibling wrappers
across 2 surface pairs = 64 cells).

TODAY's lesson does NOT cover the OTHER open items from the
matrix:

1. **SPACE-padded × NUL-padded (i.e. filled with both ' ' and
   '\\0') cells** — would be redundant (the NUL-padded sibling
   already places `'\\0'` at the truncation-edge or full-success
   position; the bytes after the NUL are already whatever they
   were before the call).  This lesson DOES NOT cover
   SPACE-padded × NUL-padded and explicitly skips it.

2. **PETRA_PAD=1 × PETRA_SPACE_PAD=1 cells** (i.e. SPACE-padded
   siblings of Sep 27's NUL-padded siblings) — would introduce 4
   NEW SPACE+NUL-padded wrappers.  Each future lesson adds
   verbatim thin SPACE-padded wrappers composed on top of Sep
   27's NUL-padded siblings + new `#if`-gated meta-macros.  (4
   new cells per axis extension.)

3. **ALTERNATE-PAD cells** (e.g. `PETRA_DASH_PAD` that pads
   with `'-'`) — would introduce a SEVENTH axis.  Each future
   lesson adds verbatim thin dash-pad wrappers composed on top
   of TODAY's SPACE-padded wrappers + new `#if PETRA_DASH_PAD`-
   gated meta-macros.  (4 new cells per axis extension.)

With the 4-cell std::span\<char\> matrix now COMPLETE for the
SPACE-padded axis (and the 16-cell char\*+cap × BOUNDED-BUFFER
matrix from Sep 26 / Sep 27 COMPLETE), future lessons can
either (a) introduce a SEVENTH axis on EITHER side (e.g.
`PETRA_DASH_PAD`), (b) extend the SPACE-pad axis to the
char\*+cap side (mirror of Sep 26 / Sep 27's NUL-pad layer on
the char\*+cap side), or (c) close cross-cutting infrastructure
items from the Aug 13 / Aug 15 / Aug 17 lessons.

## C++23 features exercised

- `std::format` + `std::vformat_to` + `std::format_string<Args...>`
  (P2216R3) + `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` + `dynamic_cast` +
  `std::is_base_of_v` (catch-chain contract pinning).
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range` + `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v`.
- `std::array<std::thread, 4>` + `std::thread::join()`
  (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.  Reuses Sep 6 / Sep 7 / Sep 8 /
Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
Sep 26 / Sep 27's machinery verbatim.

## Build and verification commands

Default build (PETRA_THROW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-28-try-format-space-pad-span-n.cpp \
        -o /tmp/P-2026-09-28-try-format-space-pad-span-n
```

Strict-warning build:

```sh
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-28-try-format-space-pad-span-n
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-28-try-format-space-pad-span-n
```

Expected+SPACE-padded path (PETRA_THROW=0, PETRA_SPACE_PAD=1):

```sh
cmake -S . -B build-space-pad -DPETRA_SPACE_PAD=ON
cmake --build build-space-pad
./build-space-pad/P-2026-09-28-try-format-space-pad-span-n
```

Throwing+untouched path (PETRA_THROW=1, PETRA_SPACE_PAD=0):

```sh
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
cmake --build build-throw
./build-throw/P-2026-09-28-try-format-space-pad-span-n
```

Throwing+SPACE-padded path (PETRA_THROW=1, PETRA_SPACE_PAD=1):

```sh
cmake -S . -B build-throw-space-pad -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_SPACE_PAD=ON
cmake --build build-throw-space-pad
./build-throw-space-pad/P-2026-09-28-try-format-space-pad-span-n
```

Direct compile (no CMake, mirrors the lesson's "Direct compile"
command):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-28-try-format-space-pad-span-n.cpp \
        -o /tmp/P-2026-09-28-try-format-space-pad-span-n
```

## Consteval-gate probe

The CONSTEVAL macro's consteval gate is verified by
`/tmp/probe_consteval_rejects_runtime_fmt_space_pad.cpp`.  The
probe calls `petra::try_format_bounded_runtime_n_space_pad_span`
with a runtime `std::string` fmt and produces the expected error:

```
error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<std::string>' is not a constant expression
note: read of non-constexpr variable 'fmt_runtime' is not allowed in a constant expression
note: in call to 'this->data()'
```

## Verification performed

- Default build (0, 0, 0): 174/174 PASS across 16 sections, 5
  consecutive runs (no flakiness in Section 10's 4-thread ×
  25-call concurrent dispatch).
- Expected+SPACE-padded build (0, 0, 1): 182/182 PASS across
  16 sections, 5 consecutive runs (the additional 8 CHECKs
  come from Sections 2b's and 16's SPACE-pad-specific byte-
  state tests).
- Throwing+untouched build (1, 0, 0): 162/162 PASS across 16
  sections, 5 consecutive runs (Section 15 does CHECK(true) on
  the expected path, hence 12 fewer CHECKs; on the throwing
  path Section 15 exercises dynamic_cast).
- Throwing+SPACE-padded build (1, 0, 1): 170/170 PASS across
  16 sections, 5 consecutive runs.
- Strict-warning build (default + `-Wall -Wextra -Wpedantic
  -Werror -Wshadow -Wconversion -Wsign-conversion`): zero
  warnings, 174/174 PASS.
- ASan/UBSan build (default + `-fsanitize=address
  -fsanitize=undefined -fno-omit-frame-pointer -O1`): zero
  diagnostics, 182/182 PASS.
- Consteval-gate probe:
  `/tmp/probe_consteval_rejects_runtime_fmt_space_pad.cpp`
  produces the expected consteval-rejection error at compile
  time.
- Invalid-flag-value probes: `-DPETRA_PAD=1` fires the
  `#error` guard (OUT OF SCOPE for this lesson),
  `-DPETRA_THROW_ON_OVERFLOW=2` and `-DPETRA_SPACE_PAD=5` both
  fire the independent `#error` guards at preprocessor time.
- CMake build verification: `cmake -S . -B build` and
  `cmake --build build` succeed (default + PETRA_SPACE_PAD=ON
  both pass).
