# P-2026-09-27 — `try_format_padded_span_n_or_throw`

Topic: a **2-AXIS (× 2-SIBLING) META-MACRO layer** that lifts BOTH
the **RETURN-TYPE-vs-EXCEPTION** sibling choice (Sep 21) AND the
**PAD-VS-UNTOUCHED** sibling choice (Sep 25 / Sep 26) to a
meta-macro form on the **STD::SPAN<CHAR> × BOUNDED-BUFFER** axis —
the **STD::SPAN<CHAR> MIRROR** of Sep 26's char\*+cap lesson.  Two
new meta-macros —
`TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` (CONSTEVAL surface) and
`TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW` (RUNTIME surface) —
each dispatch to **ONE OF FOUR** sibling wrappers at preprocessor
time based on TWO caller-controlled compile-time flags
`(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)`.

**Closes Sep 26's "Where we go next" item #1 verbatim**:

> The natural follow-on is the **span<char> mirror** of today's
> char\*+cap lesson.  Each future lesson adds verbatim thin padded
> wrappers composed on top of Sep 10's / Sep 19's / Sep 21's
> span<char> bounded-buffer wrappers + new `#if PETRA_PAD`-gated
> meta-macros on the span<char> path.  (16 cells:
> CONSTEVAL/RUNTIME × expected/throwing × untouched/padded.)

Today covers that verbatim — FOUR new
`petra::try_format_*_padded_span(std::span<char>, fmt, args...)`
wrappers (RUNTIME surface + CONSTEVAL surface of both the
expected+padded and throwing+padded siblings) that compose on top
of Sep 19's / Sep 10's / Sep 21's NEW span<char> wrappers and zero-
fill `[written, span.size())` to `'\0'` on success only.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) quadrant
on the **std::span<char>** side grows from **8 cells (Sep 23)** to
**16 cells (TODAY)** — a 2× expansion from adding the FIFTH axis
(Sep 25 / Sep 26) on the span<char> side, mirroring Sep 26's 64-cell
completion on the char\*+cap side.

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
           `std::span<char>` axis (closes Sep 20's "Where we go
           next" item #1 verbatim; covers the `std::span<char>`
           × bounded-buffer × expected-vs-throwing cells; the
           natural follow-on from Sep 20's char\*+cap lesson).
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
- Sep 26   `try_format_padded_n_or_throw` — the FIFTH axis
           (PAD-VS-UNTOUCHED) × FOURTH axis (EXPECTED-VS-THROWING)
           COMBINED META-MACRO layer on the CHAR\*+CAP ×
           BOUNDED-BUFFER axis (covers 4 NEW cells on the
           char\*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer ×
           throwing × untouched-or-padded axis; the matrix grows
           from 32 to 64 cells on the BOUNDED-BUFFER quadrant on
           the char\*+cap side).
- TODAY    `try_format_padded_span_n_or_throw` — the STD::SPAN<CHAR>
           MIRROR of Sep 26's char\*+cap lesson (closes Sep 26's
           "Where we go next" item #1 verbatim — covers 4 NEW
           cells on the `std::span<char>` × CONSTEVAL-or-RUNTIME ×
           bounded-buffer × throwing × untouched-or-padded axis;
           the matrix grows from 8 to 16 cells on the
           BOUNDED-BUFFER × (expected-or-throwing) quadrant on the
           `std::span<char>` side).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
  Sep 23 / Sep 25 / Sep 26 verbatim (the wrappers below do not
  produce `FormatError` directly — they only forward to existing
  wrappers, but the type must be visible in scope for
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
  args...)` — REUSED Sep 8 verbatim.
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 9 verbatim.
- `petra::try_format_runtime_n(std::span<char>, std::string_view,
  args...)` — REUSED Sep 19 verbatim (RUNTIME span<char>
  untouched wrapper).
- `petra::try_format_bounded_runtime_n_span(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 10 verbatim
  (CONSTEVAL span<char> untouched wrapper).
- `petra::try_format_runtime_n_throwing(std::span<char>,
  std::string_view, args...)` — REUSED Sep 21 verbatim (RUNTIME
  span<char> throwing untouched wrapper).
- `petra::try_format_bounded_runtime_n_throwing_span(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 21 verbatim
  (CONSTEVAL span<char> throwing untouched wrapper).
- `petra::try_format_bounded_runtime_n_padded_span(std::span<char>,
  std::format_string<Args...>, args...)` — **NEW today** (CONSTEVAL
  span<char> expected+padded wrapper; wraps Sep 10's CONSTEVAL
  span<char> untouched wrapper verbatim and zero-fills `[written,
  span.size())` to `'\0'` on success).
- `petra::try_format_runtime_n_padded_span(std::span<char>,
  std::string_view, args...)` — **NEW today** (RUNTIME span<char>
  expected+padded wrapper; wraps Sep 19's RUNTIME span<char>
  untouched wrapper verbatim and zero-fills `[written,
  span.size())` to `'\0'` on success).
- `petra::try_format_bounded_runtime_n_throwing_padded_span(
  std::span<char>, std::format_string<Args...>, args...)` — **NEW
  today** (CONSTEVAL span<char> throwing+padded wrapper; wraps
  Sep 21's CONSTEVAL span<char> throwing untouched wrapper
  verbatim and zero-fills `[written, span.size())` to `'\0'` on
  success only via try/catch around the throwing call).
- `petra::try_format_runtime_n_throwing_padded_span(std::span<char>,
  std::string_view, args...)` — **NEW today** (RUNTIME span<char>
  throwing+padded wrapper; wraps Sep 21's RUNTIME span<char>
  throwing untouched wrapper verbatim and zero-fills `[written,
  span.size())` to `'\0'` on success only via try/catch around
  the throwing call).
- The TWO new meta-macros
  `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` (CONSTEVAL surface) and
  `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW` (RUNTIME
  surface) — NEW today; each is a verbatim thin dispatcher that
  picks between FOUR sibling wrappers at preprocessor time based
  on the `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)` 2-bit pair.  Each
  macro is a parenthesized expression (NOT `do { } while (0)`) so
  the caller can write `auto n =
  TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)` and capture the
  result.  Each macro uses `__VA_OPT__(,)` for the no-args case
  (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 /
  Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26's macro shape).

## What today's lesson pins

1. **The (THROWING-vs-EXPECTED) × (PADDED-vs-UNTOUCHED) sibling
   choice CAN be lifted to a meta-macro form as a 2-axis product
   on the STD::SPAN<CHAR> × BOUNDED-BUFFER axis.**  This is the
   **STD::SPAN<CHAR> MIRROR** of Sep 26's char\*+cap lesson — the
   exact same shape with the first argument changed from
   `char*, std::size_t` to `std::span<char>`.  The shape composes
   on top of Sep 10's / Sep 19's bounded-buffer span<char>
   underlying wrappers via TWO levels of dispatch:
   level 1: TODAY's NEW padded wrappers — each sibling dispatches
            on ONE axis (PAD or THROW).
   level 2: TODAY's meta-macros dispatch on BOTH axes via a
            nested `#if` pair (PETRA_THROW first, then PETRA_PAD
            inside each branch).
2. **The compile-time flag pair `(PETRA_THROW_ON_OVERFLOW,
   PETRA_PAD)` controls the dispatch** via `#if PETRA_THROW_ON_OVERFLOW`
   × `#if PETRA_PAD`:
   - `(0, 0)` → expected+untouched sibling (returns
     `std::expected<std::size_t, FormatError>`; bytes `[written,
     span.size())` UNTOUCHED on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_span` (Sep 10)
       - RUNTIME:   `petra::try_format_runtime_n(span, ...)` (Sep 19)
   - `(0, 1)` → expected+padded sibling (returns
     `std::expected<std::size_t, FormatError>`; bytes `[written,
     span.size())` zero-filled to `'\0'` on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded_span`
                    (NEW today; wraps Sep 10 verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_padded_span`
                    (NEW today; wraps Sep 19 verbatim)
   - `(1, 0)` → throwing+untouched sibling (returns `std::size_t`
     on success; throws `std::runtime_error` on overflow; bytes
     `[written, span.size())` UNTOUCHED on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_span`
                    (Sep 21; wraps Sep 10 verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_throwing(span, ...)`
                    (Sep 21; wraps Sep 19 verbatim)
   - `(1, 1)` → throwing+padded sibling (returns `std::size_t`
     on success; throws `std::runtime_error` on overflow; bytes
     `[written, span.size())` zero-filled to `'\0'` on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_padded_span`
                    (NEW today; wraps Sep 21 verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_padded_span`
                    (NEW today; wraps Sep 21 verbatim)
3. **Independent `#error` guards** fire at preprocessor time if
   EITHER flag is set to anything other than 0 or 1.  The flags
   are validated independently (one `#error` per flag) so an
   invalid flag does NOT silently fall through to a wrong branch:
   ```
   error: "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
   error: "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
   ```
4. **The throwing+padded sibling preserves the SAME buffer
   atomicity on TooLarge as the throwing+untouched sibling** —
   the buffer is UNTOUCHED on overflow (Sep 21's throwing wrapper
   already pins this; TODAY's NEW padded wrapper re-pins it via
   the try/catch structure — the catch block re-raises BEFORE the
   pad loop runs).  Section 3 pins this on the throwing+padded
   path with a 16-byte (or 8-byte) CHECK block.
5. **The throwing+padded sibling preserves the SAME buffer
   atomicity on Format errors** — a malformed fmt throws
   `std::runtime_error` (delegated to Sep 21's wrapper, which
   delegates to Sep 19's / Sep 10's underlying impl, which throws
   `std::format_error` at format time before any writes).  Section
   8 pins this on the RUNTIME surface by calling with `std::string
   fmt = "bad {"` and asserting every byte is still `Q`.
6. **The padded sibling preserves the SAME byte-count contract
   on success** — returns the byte count written (== needed ==
   written, the same value Sep 19's / Sep 10's / Sep 21's
   untouched siblings return).  Section 4 pins this on both
   surfaces.
7. **The padded sibling preserves the SAME return-type
   contract** — both surfaces return `std::expected<std::size_t,
   FormatError>` regardless of `PETRA_PAD` on the expected path;
   both surfaces return `std::size_t` regardless of `PETRA_PAD` on
   the throwing path.  Pinned by Section 1b / Section 14's
   decltype checks on BOTH paths.
8. **The CONSTEVAL macro's consteval gate is preserved
   regardless of `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD)`** —
   `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` still requires
   `std::format_string<Args...>` (a literal fmt at the call site).
   Pinned by `/tmp/probe_consteval_rejects_runtime_fmt_span_pad.cpp`
   producing `error: call to consteval function std::basic_format_string<char,
   int>::basic_format_string<std::string> is not a constant expression`
   with note `read of non-constexpr variable fmt_runtime is not
   allowed in a constant expression` / `in call to this->data()`.
9. **The throwing+padded sibling's `std::runtime_error`
   `.what()` byte-exactness is pinned** by Section 15 — TODAY's
   wrapper does NOT construct a new `std::runtime_error` message;
   the throw TYPE and `.what()` message are both delegated to
   Sep 21's throwing wrapper (which constructs `std::runtime_error`
   with the SAME `.what()` message that `FormatError{TooLarge,
   ...}` would carry).  Section 15's
   `dynamic_cast<const std::format_error*>(&e)` check returns
   `nullptr`, confirming that the throwing wrapper re-raises as
   `std::runtime_error` (NOT `std::format_error`) — this matches
   Sep 21's catch-chain contract.
10. **The `std::span<char>` sub-span 'no-bleed' contract is
    preserved on the padded path** (Section 2a) — a
    `std::span<char>` over a sub-range of a buffer must NOT touch
    bytes outside the span (mirrors Sep 10 / Sep 12 / Sep 19 /
    Sep 21's no-bleed contract; a 6-byte sub-span at offset 5 of
    a 16-byte buffer with format `'ABCDE'` (5 bytes) leaves bytes
    `[0,5)` and `[11,16)` as poison `'Q'` regardless of
    `PETRA_PAD`).
11. **The `std::span<char>` properties are pinned** in Section
    1b: `std::ranges::contiguous_range` +
    `std::ranges::sized_range` + trivially copyable + standard
    layout + `.data()->char*` + `.size()->std::size_t`.
12. **The padded sibling handles the `span.size() == 0` edge
    case correctly** — the `'for (i = written; i < cap; ++i)'`
    loop has `i < cap == 0` as the initial condition, so the
    loop body never executes.  Section 2c pins this on the
    `cap=0 + empty-fmt` case (the `'Q'` sentinel byte outside
    the `cap=0` writable region remains untouched).
13. **The padded sibling handles the `written == cap` edge case
    correctly** — the `'for (i = written; i < cap; ++i)'` loop
    has `i == cap` as the initial condition, so the loop body
    never executes (the span is exactly filled).  Section 2d pins
    this with format `'ABCDEFGH'` (8 bytes) into a span of size
    8 (full-buffer success).

## What today's lesson does NOT cover

- **STD::SPAN<CHAR> × NUL-TERMINATING × (any) cells** — already
  covered by Sep 11 / Sep 12 / Sep 19 / Sep 23 (the NUL-terminating
  span<char> axis).  Mirrors Sep 26's open item #2.
- **STD::SPAN<CHAR> × ALTERNATE-PAD cells** — would introduce a
  SIXTH axis (e.g. `PETRA_SPACE_PAD` axis that pads with `' '`
  instead of `'\0'`).  Mirrors Sep 26's open item #3.
- **STD::SPAN<CONST CHAR> axis** — out of scope; Sep 14 / Sep 15
  already established that the CONSTEVAL surface is incompatible
  with `std::span<const char>`.

## Reuses

- `petra::FormatError` / `petra::FormatErrorKind` /
  `petra::counting_output_iterator` /
  `petra::detail::bounded_char_writer` /
  `petra::detail::try_format_runtime_n_impl` (Sep 8) /
  `petra::detail::try_format_bounded_runtime_n_impl` (Sep 9) /
  `petra::try_format_runtime_n(char*, std::size_t, ...)` (Sep 8
  char\*+cap) / `petra::try_format_bounded_runtime_n(char*,
  std::size_t, ...)` (Sep 9 char\*+cap CONSTEVAL) /
  `petra::try_format_runtime_n(span<char>, ...)` (Sep 19 RUNTIME
  span<char>) / `petra::try_format_bounded_runtime_n_span(span<
  char>, ...)` (Sep 10 CONSTEVAL span<char>) /
  `petra::try_format_runtime_n_throwing(span<char>, ...)` (Sep
  21 RUNTIME span<char> throwing) /
  `petra::try_format_bounded_runtime_n_throwing_span(span<char>,
  ...)` (Sep 21 CONSTEVAL span<char> throwing) verbatim — no new
  types beyond the FOUR NEW padded span<char> wrappers, no new
  impls, no new error contracts.

## Where we go next

TODAY's lesson closes Sep 26's "Where we go next" item #1
verbatim: the **STD::SPAN<CHAR> MIRROR** of Sep 26's char\*+cap
(THROWING × PADDED) cells.  The matrix is now **16 cells (2^4)**
on the BOUNDED-BUFFER × (expected-or-throwing) quadrant on the
**std::span<char>** side (4 dispatch cells × 4 sibling wrappers;
the std::span<char> side does NOT have the additional
NUL-terminating-or-bounded-buffer axis of the char\*+cap side
because the bounded-buffer axis IS the only writable axis on
std::span<char> — there is no NUL-terminating overload on
std::span<char>).

TODAY's lesson does NOT cover the OTHER open items from the
matrix:

1. **NUL-terminating × padded cells** on the char\*+cap side —
   would be redundant (the NUL-terminating sibling already places
   `'\0'` at the truncation-edge or full-success position; the
   bytes after the NUL are already whatever they were before the
   call).  This lesson DOES NOT cover NUL-terminating × padded
   and explicitly skips it (mirrors Sep 25's / Sep 26's open
   item #2).
2. **ALTERNATE-PAD cells** (e.g. `PETRA_SPACE_PAD` that pads with
   `' '` instead of `'\0'`) on EITHER axis — would introduce a
   SIXTH axis.  Each future lesson adds verbatim thin space-pad
   wrappers composed on top of TODAY's NUL-padded wrappers + new
   `#if PETRA_SPACE_PAD`-gated meta-macros.  (16 cells per axis
   extension: 2^4 = 16 combinations on the std::span<char> side,
   64 on the char\*+cap side.)

With the 16-cell std::span<char> matrix now COMPLETE (and the
64-cell char\*+cap matrix from Sep 26 COMPLETE), future lessons
can either (a) introduce a SIXTH axis on EITHER side (e.g.
`PETRA_SPACE_PAD`), or (b) close cross-cutting infrastructure
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
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range` + `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v`.
- `std::array<std::thread, 4>` + `std::thread::join()` (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.  Reuses Sep 6 / Sep 7 / Sep 8 /
Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
Sep 26's machinery verbatim.

## Build and verification commands

Default build (expected+untouched sibling; both flags 0):

```sh
cd late-may/cpp_practice/try_format_padded_span_n_or_throw
cmake -S . -B build
cmake --build build
./build/P-2026-09-27-try-format-padded-span-n-or-throw
```

Expected output:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 149  FAIL: 0
=========================================================
OVERALL: PASS
```

Strict-warning build:

```sh
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-27-try-format-padded-span-n-or-throw
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-27-try-format-padded-span-n-or-throw
```

Throwing path (PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0):

```sh
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
cmake --build build-throw
./build-throw/P-2026-09-27-try-format-padded-span-n-or-throw
```

Padded path (PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1):

```sh
cmake -S . -B build-pad -DPETRA_PAD=ON
cmake --build build-pad
./build-pad/P-2026-09-27-try-format-padded-span-n-or-throw
```

Throwing+Padded path (PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1):

```sh
cmake -S . -B build-throw-pad -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PAD=ON
cmake --build build-throw-pad
./build-throw-pad/P-2026-09-27-try-format-padded-span-n-or-throw
```

Direct compile (no CMake, mirrors the lesson's "Direct compile"
command):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-27-try-format-padded-span-n-or-throw.cpp \
        -o /tmp/P-2026-09-27-try-format-padded-span-n-or-throw
```

## Consteval-gate probe

The CONSTEVAL macro's consteval gate is verified by
`/tmp/probe_consteval_rejects_runtime_fmt_span_pad.cpp`.  The
probe calls `petra::try_format_bounded_runtime_n_padded_span` with
a runtime `std::string` fmt and produces the expected error:

```
error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<std::string>' is not a constant expression
note: read of non-constexpr variable 'fmt_runtime' is not allowed in a constant expression
note: in call to 'this->data()'
```

## Verification performed

- Default build (0, 0): 149/149 PASS across 15 sections, 5
  consecutive runs (no flakiness in Section 10's 4-thread ×
  25-call concurrent dispatch).
- Padded build (0, 1): 149/149 PASS across 15 sections, 5
  consecutive runs (the additional 17 CHECKs from Section 2a's
  sub-span no-bleed test on the padded path).
- Throwing build (1, 0): 139/139 PASS across 15 sections, 5
  consecutive runs (Section 15 does CHECK(true) on the expected
  path, hence 10 fewer CHECKs; on the throwing path Section 15
  exercises dynamic_cast).
- Throwing+Padded build (1, 1): 139/139 PASS across 15 sections,
  5 consecutive runs.
- Strict-warning build (default + `-Wall -Wextra -Wpedantic
  -Werror -Wshadow -Wconversion -Wsign-conversion`): zero
  warnings, 149/149 PASS.
- ASan/UBSan build (default + `-fsanitize=address
  -fsanitize=undefined -fno-omit-frame-pointer -O1`): zero
  diagnostics, 149/149 PASS.
- Consteval-gate probe:
  `/tmp/probe_consteval_rejects_runtime_fmt_span_pad.cpp` produces
  the expected consteval-rejection error at compile time.
- Invalid-flag-value probes: `-DPETRA_THROW_ON_OVERFLOW=2` and
  `-DPETRA_PAD=5` both fire the independent `#error` guards at
  preprocessor time.
- CMake build verification: `cmake -S . -B build` and
  `cmake --build build` succeed (default + PETRA_THROW_ON_OVERFLOW=ON +
  PETRA_PAD=ON both pass).