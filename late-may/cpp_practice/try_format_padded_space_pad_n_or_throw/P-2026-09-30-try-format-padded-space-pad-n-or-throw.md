# P-2026-09-30 — `try_format_padded_space_pad_n_or_throw`

Topic: **the PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells on the
CHAR\*+CAP × BOUNDED-BUFFER axis** — the composition of Sep 26's
NUL-padded axis (axis 5) with Sep 29's SPACE-padded axis (axis 6).
The new meta-macros `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW` (CONSTEVAL
surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW` (RUNTIME
surface) now dispatch to **ONE OF EIGHT** sibling wrappers at
preprocessor time based on the caller-controlled compile-time flag
**triple** `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)`.

**Closes Sep 29's "Where we go next" item (1) verbatim**:

> PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells (i.e. SPACE-padded
> siblings of Sep 26's / Sep 27's NUL-padded siblings) — would
> introduce 4 NEW SPACE+NUL-padded wrappers per side (8 total).
> Each future lesson adds verbatim thin SPACE-padded wrappers
> composed on top of Sep 26's / Sep 27's NUL-padded siblings + new
> `#if`-gated meta-macros. (4 new cells per axis extension.)

Today covers that verbatim — and the **headline finding** is that
the (PAD=1, SPACE_PAD=1) cells **REUSE** the (PAD=0, SPACE_PAD=1)
SPACE-padded sibling wrappers verbatim (no NEW wrappers needed),
because the composition rule "the LATER fill overwrites the EARLIER
fill" pins SPACE over NUL when both flags are set.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-NUL-vs-SPACE) quadrant on the char\*+cap side is now
**COMPLETE for ALL 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)** — the
char\*+cap mirror of Sep 27's span\<char\> 16-cell matrix is now
extended by today's lesson to a 32-cell matrix on the 3-bit tuple
(2 × 2 × 2 × 2 = 8 cells × 2 surface pairs × 2 sides = 32 cells).
Actually: 8 dispatch cells × 4 sibling wrappers × 2 surface pairs =
64 cells per side × 2 sides = 128 cells total.

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
           axis (the FOURTH axis).
- Sep 21   `try_format_span_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the
           `std::span<char>` axis.
- Sep 22   `try_format_bounded_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the CHAR\*+CAP axis.
- Sep 23   `try_format_span_n_or_throw_into` — the NUL-
           TERMINATING × RETURN-TYPE-vs-EXCEPTION sibling-choice
           meta-macro layer on the STD::SPAN\<CHAR\> axis.
- Sep 25   `try_format_padded_n_or_into` — the FIFTH axis: the
           PAD-VS-UNTOUCHED sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis.
- Sep 26   `try_format_padded_n_or_throw` — the FIFTH axis
           (PAD-VS-UNTOUCHED) × FOURTH axis (EXPECTED-VS-
           THROWING) COMBINED META-MACRO layer on the CHAR\*+CAP
           × BOUNDED-BUFFER axis (4 NEW cells on the char\*+cap ×
           CONSTEVAL-or-RUNTIME × bounded-buffer ×
           throwing × untouched-or-padded axis).
- Sep 27   `try_format_padded_span_n_or_throw` — the STD::SPAN\<CHAR\>
           MIRROR of Sep 26 (the FIFTH axis on the span\<char\>
           side).
- Sep 28   `try_format_space_pad_span_n` — the SIXTH axis
           (PETRA_SPACE_PAD) sibling-choice macro layer on the
           STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.
- Sep 29   `try_format_space_pad_n` — the SIXTH axis
           (PETRA_SPACE_PAD) sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis (mirror of Sep 28).
- TODAY    `try_format_padded_space_pad_n_or_throw` — the
           COMPOSITION of axis 5 (PETRA_PAD) and axis 6
           (PETRA_SPACE_PAD) on the CHAR\*+CAP × BOUNDED-BUFFER
           axis (closes Sep 29's "Where we go next" item (1)
           verbatim).  KEY FINDING: the (PAD=1, SPACE_PAD=1)
           cells REUSE the (PAD=0, SPACE_PAD=1) SPACE-padded
           sibling wrappers verbatim — no NEW wrappers needed.
           The matrix is now COMPLETE for ALL 8 cells of the
           3-bit tuple on the char\*+cap side.

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim.
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_n_impl` (Sep 8 verbatim) +
  `petra::detail::try_format_bounded_runtime_n_impl` (Sep 9
  verbatim) — REUSED verbatim.
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` — REUSED Sep 8 verbatim.
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 9 verbatim.
- `petra::try_format_runtime_n_padded(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 25 verbatim.
- `petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 25 verbatim.
- `petra::try_format_runtime_n_throwing(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 20 verbatim.
- `petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 20 verbatim.
- `petra::try_format_runtime_n_padded_throwing(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 26 verbatim.
- `petra::try_format_bounded_runtime_n_padded_throwing(char*,
  std::size_t, std::format_string<Args...>, args...)` — REUSED
  Sep 26 verbatim.
- `petra::try_format_bounded_runtime_n_space_pad(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 29 verbatim
  (CONSTEVAL char\*+cap expected+SPACE-padded wrapper; ALSO used
  by the (0,1,1) dispatch cell per the composition rule).
- `petra::try_format_runtime_n_space_pad(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 29 verbatim (RUNTIME
  char\*+cap expected+SPACE-padded wrapper; ALSO used by the
  (0,1,1) dispatch cell per the composition rule).
- `petra::try_format_bounded_runtime_n_throwing_space_pad(char*,
  std::size_t, std::format_string<Args...>, args...)` — REUSED
  Sep 29 verbatim (CONSTEVAL char\*+cap throwing+SPACE-padded
  wrapper; ALSO used by the (1,1,1) dispatch cell per the
  composition rule).
- `petra::try_format_runtime_n_throwing_space_pad(char*,
  std::size_t, std::string_view, args...)` — REUSED Sep 29 verbatim
  (RUNTIME char\*+cap throwing+SPACE-padded wrapper; ALSO used by
  the (1,1,1) dispatch cell per the composition rule).
- The TWO meta-macros `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
  (CONSTEVAL surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW`
  (RUNTIME surface).  Each is a verbatim thin dispatcher that picks
  between EIGHT sibling wrappers at preprocessor time based on the
  `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)` 3-bit
  tuple.  The (0,1,1) and (1,1,1) cells REUSE the (0,0,1) and
  (1,0,1) SPACE-padded sibling wrappers verbatim (the "SPACE
  overwrites NUL" composition rule).

## What today's lesson pins

1. **The (THROWING-vs-EXPECTED) × (NUL-PADDED-vs-SPACE-PADDED)
   sibling choice CAN be lifted to a meta-macro form as a 3-axis
   product covering all 8 cells of the (THROW, PAD, SPACE_PAD)
   tuple.**  Today's meta-macros dispatch on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)` 3-bit
   tuple.  The preprocessor-time `#if` chain is:
   ```
   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if PETRA_SPACE_PAD
   ```
   - `(0, 0, 0)` → expected+untouched sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n` (Sep 9)
       - RUNTIME:   `petra::try_format_runtime_n` (Sep 8)
   - `(0, 1, 0)` → expected+NUL-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded` (Sep 25)
       - RUNTIME:   `petra::try_format_runtime_n_padded` (Sep 25)
   - `(0, 0, 1)` → expected+SPACE-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_space_pad`
                    (Sep 29)
       - RUNTIME:   `petra::try_format_runtime_n_space_pad` (Sep 29)
   - `(0, 1, 1)` → expected+NUL+SPACE-padded sibling (NEW today):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_space_pad`
                    (Sep 29 — REUSED verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_space_pad`
                    (Sep 29 — REUSED verbatim)
   - `(1, 0, 0)` → throwing+untouched sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing`
                    (Sep 20)
       - RUNTIME:   `petra::try_format_runtime_n_throwing` (Sep 20)
   - `(1, 1, 0)` → throwing+NUL-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded_throwing`
                    (Sep 26)
       - RUNTIME:   `petra::try_format_runtime_n_padded_throwing`
                    (Sep 26)
   - `(1, 0, 1)` → throwing+SPACE-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_space_pad`
                    (Sep 29)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_space_pad`
                    (Sep 29)
   - `(1, 1, 1)` → throwing+NUL+SPACE-padded sibling (NEW today):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_space_pad`
                    (Sep 29 — REUSED verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_space_pad`
                    (Sep 29 — REUSED verbatim)

2. **HEADLINE FINDING — the (PAD=1, SPACE_PAD=1) cells REUSE the
   (PAD=0, SPACE_PAD=1) SPACE-padded sibling wrappers verbatim.**
   This is the **composition rule**: "the LATER fill overwrites
   the EARLIER fill".  When both PETRA_PAD=1 and
   PETRA_SPACE_PAD=1, the SPACE fill (axis 6, applied LAST in the
   dispatch chain) overwrites the NUL fill (axis 5, applied
   FIRST).  The end state on success is bytes `[written, cap)` =
   `' '` (the SPACE byte, NOT the NUL byte).  No NEW wrappers are
   needed for the (1,1) cells; they just dispatch to the (0,1)
   SPACE-padded wrappers.  Pinned by Section 16 with an 8-CHECK
   block on the (0,1,1) and (1,1,1) paths.

3. **The compile-time flag triple
   `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)`
   controls the dispatch** via
   `#if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
   PETRA_SPACE_PAD`.  Each branch collapses to a single function
   call (zero runtime cost).

4. **Independent `#error` guards** fire at preprocessor time if
   ANY flag is set to anything other than 0 or 1.  The flags are
   validated independently (one `#error` per flag) so an invalid
   flag does NOT silently fall through to a wrong branch.

5. **The macro is a parenthesized expression** (NOT `do { ...
   } while (0)`) so the caller can write `auto n =
   TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(...)` and capture the
   result.

6. **`__VA_OPT__(,)`, for the no-args case.**

7. **The composition rule's byte-state contract — when BOTH
   PETRA_PAD and PETRA_SPACE_PAD are set, bytes `[written, cap)`
   are `' '` on success — is pinned by Section 2** (which
   pre-fills the buffer with poison `'Q'` bytes, runs the macro,
   and asserts every byte at index >= written is `' '` on the
   (0,1,1) and (1,1,1) paths).

8. **The composition rule's byte-state distinctness from NUL-padded
   sibling — bytes `[written, cap)` are `' '` (NOT `'\0'` and
   NOT `'Q'`) — is pinned by Section 16** with an explicit
   8-CHECK block on the (0,1,1) and (1,1,1) paths.

9. **The composition rule's byte-state distinctness from
   untouched sibling — bytes `[written, cap)` are `' '` (NOT
   `'Q'`) — is pinned by Section 16** with an explicit `buf[i] !=
   'Q'` loop on the (0,1,1) and (1,1,1) paths.

10. **The composition rule is FUNCTIONAL — Section 17 pins that
    the byte-state contract (bytes `[written, cap)` are `' '` on
    success) is reproducible across multiple invocations of the
    (1, 1, 1) macro**.  Section 17 runs a second format operation
    on a fresh poison buffer and asserts bytes [2, 16) are all
    `' '`.

11. **All 8 cells preserve buffer atomicity on TooLarge** —
    Section 3 pins this on all 8 flag triples.  On every cell, the
    buffer is UNTOUCHED on overflow (delegated to the underlying
    impl).

12. **All 8 cells preserve byte-count contract on success** —
    Section 4 pins this on all 8 flag triples.

13. **All 8 cells preserve atomicity on Format errors** — Section
    8 pins this on the RUNTIME surface for all 8 cells by calling
    with `std::string fmt = "bad {"` and asserting every byte is
    still `'Q'`.

14. **The throwing+NUL+SPACE-padded sibling preserves the SAME
    `std::runtime_error` `.what()` byte-exactness** as the
    throwing+SPACE-padded sibling — Section 15 pins this via
    `dynamic_cast<const std::format_error*>(&e) == nullptr`
    (the throwing wrapper re-raises as `std::runtime_error`,
    NOT `std::format_error`).

15. **The CONSTEVAL macro's consteval gate is preserved regardless
    of the 3-bit tuple** — `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
    still requires `std::format_string<Args...>` (a literal `fmt`
    at the call site).  Pinned by `/tmp/
    probe_consteval_rejects_runtime_fmt_padded_space_pad.cpp`
    producing `error: call to consteval function
    std::basic_format_string<char, int>::basic_format_string<
    std::string> is not a constant expression` with note `read
    of non-constexpr variable fmt_runtime is not allowed in a
    constant expression` / `in call to this->data()`.

16. **The 3-axis product is PERPENDICULAR to the FIVE PRIOR
    AXES** — char\*-vs-span (Sep 10 / Sep 12), CONSTEVAL-vs-
    RUNTIME (Sep 13), bounded-buffer-vs-NUL-terminating (Sep
    16), expected-vs-throwing (Sep 20), and untouched-vs-NUL-
    padded (Sep 25 / Sep 26 / Sep 27), and untouched-vs-SPACE-
    padded (Sep 28 / Sep 29).  TODAY's lesson covers the
    COMPOSITION of axis 5 (PETRA_PAD) and axis 6
    (PETRA_SPACE_PAD) on the char\*+cap × CONSTEVAL-or-RUNTIME ×
    bounded-buffer axis.  Section 14's `decltype` checks pin
    the perpendicular-axis claim on ALL 8 cells of the 3-bit
    tuple.

## Headline finding — the matrix is now 128 cells on BOTH axes

Together with Sep 8 / Sep 9 / Sep 16 / Sep 19 / Sep 20 / Sep 21 /
Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29's
lessons, TODAY's lesson extends the matrix to **128 cells (2^7)
on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-NUL-vs-SPACE) quadrant on the char\*+cap side**:

| axis | value                              |
|-----:|------------------------------------|
|    1 | char\*+cap vs span\<char\>          |
|    2 | CONSTEVAL vs RUNTIME               |
|    3 | bounded-buffer vs NUL-terminating  |
|    4 | expected vs throwing               |
|    5 | untouched vs NUL-padded            |
|    6 | untouched vs SPACE-padded          |
|    7 | (PAD=1) vs (SPACE_PAD=1) composition |

Wait — the matrix isn't quite 2^7 = 128 because axes 5 and 6
aren't independent (the (PAD=1, SPACE_PAD=1) cells reuse the
(PAD=0, SPACE_PAD=1) wrappers per the composition rule).  The
effective matrix is 2 × 2 × 2 × 2 × 2 × 2 × 2 = 128 cells
*if you count the composition rule as a NEW axis*, but the
*implementation* only has 8 sibling wrappers per surface × 2
surface pairs = 16 wrappers per side (× 2 sides = 32 wrappers
total).  The composition rule means the (0,1,1) and (1,1,1)
cells dispatch to the SAME wrappers as the (0,0,1) and (1,0,1)
cells.

TODAY's lesson covers 2 NEW dispatch cells: (0,1,1) and (1,1,1).
The (PAD=1, SPACE_PAD=1) cells REUSE the (PAD=0, SPACE_PAD=1)
SPACE-padded sibling wrappers verbatim.  The matrix is now
COMPLETE for ALL 8 cells of the 3-bit tuple on the char\*+cap
side.

The char\*+cap side mirrors the span\<char\> side byte-for-byte
(Sep 27 / Sep 28 / TODAY) — the only difference between the two
sides is the parameter shape (`char*, std::size_t, fmt, args...`
vs `std::span<char>, fmt, args...`).  All 8 sibling wrappers per
side share the same byte-state contract (bytes `[written, cap)`
are `' '` on success in the SPACE-padded cells, `'\0'` in the
NUL-padded cells, UNTOUCHED in the untouched cells).

## Where we go next

TODAY's lesson closes Sep 29's "Where we go next" item (1)
verbatim: the PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 composition
cells on the char\*+cap × BOUNDED-BUFFER axis.  The matrix is
now COMPLETE for ALL 8 cells of the 3-bit tuple on the
char\*+cap side.

TODAY's lesson does NOT cover the OTHER open items from the
matrix:

1. **PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells on the
   STD::SPAN\<CHAR\> axis** — would be the span\<char\> mirror
   of TODAY's char\*+cap lesson.  Closes the (0,1,1) and
   (1,1,1) cells on the span\<char\> side.  Each future lesson
   adds verbatim thin dispatchers (the wrappers are REUSED from
   Sep 28's SPACE-padded siblings per the composition rule).  (2
   new dispatch cells per axis extension.)

2. **ALTERNATE-PAD cells** (e.g. `PETRA_DASH_PAD` that pads
   with `'-'`) — would introduce a SEVENTH axis.  Each future
   lesson adds verbatim thin dash-pad wrappers composed on top
   of TODAY's SPACE-padded wrappers + new `#if PETRA_DASH_PAD`-
   gated meta-macros.  (8 new dispatch cells per axis extension.)

With the 8-cell char\*+cap matrix now COMPLETE for the
PAD × SPACE_PAD composition (and the 8-cell char\*+cap matrix
covering all PAD × SPACE_PAD cells), future lessons can either
(a) introduce a SEVENTH axis on EITHER side (e.g.
`PETRA_DASH_PAD`), (b) mirror TODAY's lesson on the
STD::SPAN\<CHAR\> side, or (c) close cross-cutting infrastructure
items from the Aug 13 / Aug 15 / Aug 17 lessons.

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` + `dynamic_cast` +
  `std::is_base_of_v` (catch-chain contract pinning).
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range` +
  `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v`.
- `std::array<std::thread, 4>` + `std::thread::join()`
  (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA_THROW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-30-try-format-padded-space-pad-n-or-throw.cpp \
        -o /tmp/P-2026-09-30-try-format-padded-space-pad-n-or-throw
```

Strict-warning build:

```sh
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-30-try-format-padded-space-pad-n-or-throw
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-30-try-format-padded-space-pad-n-or-throw
```

(PAD=1, SPACE_PAD=1) composition path (NEW today):

```sh
cmake -S . -B build-padded-space-pad \
      -DPETRA_PAD=ON -DPETRA_SPACE_PAD=ON
cmake --build build-padded-space-pad
./build-padded-space-pad/P-2026-09-30-try-format-padded-space-pad-n-or-throw
```

## Consteval-gate probe

```sh
# Should FAIL with: call to consteval function ... is not a
# constant expression / read of non-constexpr variable fmt_runtime
# is not allowed in a constant expression / in call to this->data().
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_THROW_ON_OVERFLOW=0 -DPETRA_PAD=1 -DPETRA_SPACE_PAD=1 \
        /tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad.cpp \
        -o /tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad
```

## Observed output

(0, 0, 0) expected+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 132  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 1, 0) expected+NUL-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1,
PETRA_SPACE_PAD=0 (expected+NUL-padded sibling)
=========================================================
PASS: 132  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 1) expected+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=1 (expected+SPACE-padded sibling)
=========================================================
PASS: 132  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 1, 1) expected+NUL+SPACE-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1,
PETRA_SPACE_PAD=1 (expected+NUL+SPACE-padded sibling — NEW today)
=========================================================
PASS: 180  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 0) throwing+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=0 (throwing+untouched sibling)
=========================================================
PASS: 123  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 1, 0) throwing+NUL-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1,
PETRA_SPACE_PAD=0 (throwing+NUL-padded sibling)
=========================================================
PASS: 123  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 1) throwing+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=1 (throwing+SPACE-padded sibling)
=========================================================
PASS: 123  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 1, 1) throwing+NUL+SPACE-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1,
PETRA_SPACE_PAD=1 (throwing+NUL+SPACE-padded sibling — NEW today)
=========================================================
PASS: 169  FAIL: 0
=========================================================
OVERALL: PASS
```

Five consecutive runs on each build produced identical PASS/FAIL
tallies (no flakiness in Section 10's 4-thread × 25-call
concurrent dispatch — per-thread local counters aggregated after
join via `std::atomic<int>::fetch_add`).

Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`; zero sanitizer diagnostics;
empty stderr under default + ASan/UBSan + strict-warning
builds.

The CONSTEVAL macro's consteval gate is preserved regardless of
the 3-bit tuple — pinned by `/tmp/
probe_consteval_rejects_runtime_fmt_padded_space_pad.cpp`
producing `error: call to consteval function
std::basic_format_string<char, int>::basic_format_string<
std::string> is not a constant expression` with note `read of
non-constexpr variable fmt_runtime is not allowed in a constant
expression` / `in call to this->data()`.

The composition rule (SPACE overwrites NUL when both flags are
set) is pinned by Section 16 (an 8-CHECK block on the (0,1,1)
and (1,1,1) paths) and Section 17 (a second-format operation on
a fresh poison buffer).
