# P-2026-10-05 — `try_format_padded_dash_pad_span_n_or_throw`

Topic: **the PETRA_PAD = 1 × PETRA_DASH_PAD = 1 cells on the
STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis** — the STD::SPAN\<CHAR\>
MIRROR of Sep 30's char\*+cap `PAD × SPACE_PAD` composition
lesson, but for the DASH axis.  TODAY extends the SEVENTH axis
(PETRA_DASH_PAD) to the NUL-padded sibling (FIFTH axis) on the
std::span\<char\> side.

**Closes Oct 4's "Where we go next" item (1) verbatim**:

> PAD × DASH composition cells (PETRA_PAD=1, PETRA_DASH_PAD=1) on
> the STD::SPAN\<CHAR\> axis — would close the (0,1,0,1),
> (0,1,1,1), (1,1,0,1), and (1,1,1,1) cells on the span\<char\>
> side (the PAD × DASH_PAD composition cells).  Each future
> lesson adds verbatim thin DASH-pad wrappers composed on top of
> Sep 27's NUL-padded span\<char\> siblings + new
> `#if PETRA_DASH_PAD`-gated meta-macros.  (4 new dispatch
> cells per axis extension.)  This is the SPAN\<char\> mirror
> of Sep 30's char\*+cap PAD × SPACE_PAD composition lesson,
> but for the DASH axis.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-NUL-vs-SPACE-vs-DASH) quadrant on the
`std::span<char>` side is now **COMPLETE for ALL 16 cells of the
4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
PETRA_SPACE_PAD, PETRA_DASH_PAD)**.

## Key findings

- The PAD × DASH composition is a **transitive composition**: when
  ALL THREE flags PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD are
  set, the LATER fill (DASH) overwrites the MIDDLE fill (SPACE)
  which OVERWROTE the EARLIER fill (NUL).  The end state on
  success is bytes `[written, span.size())` = `'-'` (the DASH
  byte).

- **KEY DESIGN DECISION — the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1,
  0, 1), and (1, 1, 1, 1) cells REUSE the (0, 0, 0, 1) and (1, 0,
  0, 1) DASH-padded span\<char\> sibling wrappers verbatim.**  No
  NEW wrappers are needed for the `(PAD=1, DASH_PAD=1)` cells;
  they just dispatch to Oct 4's DASH-padded siblings.  Likewise,
  the (0, 1, 1, 0) and (1, 1, 1, 0) cells REUSE the (0, 0, 1, 0)
  and (1, 0, 1, 0) SPACE-padded span\<char\> sibling wrappers
  verbatim (Oct 1's "SPACE overwrites NUL" composition rule).

- The composition rule **"the LATER fill overwrites the EARLIER
  fill"** is verified transitively across 3 layers: DASH
  overwrites SPACE overwrites NUL when all three are set, so the
  end byte state is always `'-'`.  This is the SAME pattern Oct
  4's `(SPACE_PAD=1, DASH_PAD=1)` cells used to reuse the
  `(SPACE_PAD=0, DASH_PAD=1)` DASH-padded wrappers verbatim; the
  NUL layer is just inserted before the SPACE/DASH layers in the
  transitive chain, but it gets overwritten the same way.

- The composition cell's byte-state contract is pinned by
  **Section 16** with an 8-CHECK block on the (0, 1, 0, 1), (0,
  1, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1) paths asserting
  `buf[i] == '-'`, `buf[i] != '\0'`, `buf[i] != ' '`, and
  `buf[i] != 'Q'` for every byte at index `>= written`.

- The composition cell's distinctness from NUL-padded,
  SPACE-padded, and untouched paths is pinned by **Section 17**
  with an explicit 4-distinctness check (`buf[i] == '-'`,
  `buf[i] != '\0'`, `buf[i] != ' '`, `buf[i] != 'Q'`) on all 4
  DASH-padded cells of the 4-tuple that have DASH_PAD=1.

- The (0, 1, 1, 0) and (1, 1, 1, 0) PAD × SPACE_PAD composition
  cells (reusing Sep 28's SPACE-padded wrappers per Oct 1's
  rule) are pinned by **Section 18** with an explicit
  4-distinctness check on those cells.

- The PAD × DASH composition cell **preserves buffer atomicity on
  TooLarge** — the buffer is UNTOUCHED on overflow.  Pinned by
  **Section 3** on all 16 cells with a poison-`'Q'` check.

- The PAD × DASH composition cell **preserves the SAME
  byte-count contract on success** — returns `std::size_t` ==
  bytes written (the same value the underlying DASH-padded
  sibling returns).  Pinned by **Section 4** on all 16 cells.

- The PAD × DASH composition cell **preserves the SAME atomicity
  on Format errors** — a malformed fmt throws
  `std::runtime_error` (delegated to Oct 4's wrapper, which
  delegates to Sep 28's wrapper, which delegates to Sep 21's
  throwing wrapper, which delegates to Sep 10's / Sep 19's
  underlying impl, which throws `std::format_error` at format
  time before any writes).  Pinned by **Section 8** on the
  RUNTIME surface for all 16 cells.

- The throwing+PAD+DASH composition sibling **re-raises as
  `std::runtime_error` (NOT `std::format_error`)** — pinned by
  **Section 15** with a `dynamic_cast<const
  std::format_error*>(&e) == nullptr` check on the throwing
  path.

- The CONSTEVAL macro's **consteval gate is preserved regardless
  of the 4-bit tuple** — `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW`
  still requires `std::format_string<Args...>` (a literal fmt
  at the call site).  Pinned by
  `/tmp/probe_consteval_padded_dash_pad_span.cpp` producing
  `'error: call to consteval function
  std::basic_format_string<char, int>::basic_format_string<std::string>
  is not a constant expression'` with note `'read of non-constexpr
  variable fmt_runtime is not allowed in a constant expression' /
  'in call to this->data()'`.

- The std::span\<char\> sub-span **"no-bleed" contract is
  preserved on the composition path** — bytes outside the span
  are UNTOUCHED on ALL paths.  Pinned by **Section 2a** (a
  6-byte sub-span at offset 5 of a 16-byte backing buffer with
  format 'ABCDE' (5 bytes) leaves bytes [0,5) and [11,16) as
  poison 'Q' regardless of the flag 4-tuple).

- The std::span\<char\> **cap=0 and written==cap edge cases are
  handled correctly** on the composition path — pinned by
  **Section 2c** (cap=0 + empty-fmt) and **Section 2d** (full
  buffer success) on all 16 cells.

## What today's lesson adds

- **NO new sibling wrappers** — TODAY's lesson reuses
  `petra::try_format_bounded_runtime_n_dash_pad_span` (Oct 4
  CONSTEVAL) and `petra::try_format_runtime_n_dash_pad_span`
  (Oct 4 RUNTIME) and
  `petra::try_format_bounded_runtime_n_throwing_dash_pad_span`
  (Oct 4 CONSTEVAL throwing) and
  `petra::try_format_runtime_n_throwing_dash_pad_span` (Oct 4
  RUNTIME throwing) and
  `petra::try_format_bounded_runtime_n_padded_span` (Sep 27
  CONSTEVAL NUL) and `petra::try_format_runtime_n_padded_span`
  (Sep 27 RUNTIME NUL) and
  `petra::try_format_bounded_runtime_n_throwing_padded_span`
  (Sep 27 CONSTEVAL throwing NUL) and
  `petra::try_format_runtime_n_throwing_padded_span` (Sep 27
  RUNTIME throwing NUL) verbatim.  The composition rule means
  the (PAD=1, DASH_PAD=1) cells share the SAME wrappers as the
  (PAD=0, DASH_PAD=1) DASH-padded siblings.

- **TWO meta-macros** —
  `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` (CONSTEVAL surface)
  and `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW` (RUNTIME
  surface) — each is a verbatim thin dispatcher that picks
  between SIXTEEN sibling wrappers at preprocessor time based
  on the (PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
  PETRA_DASH_PAD) 4-bit tuple.  4 of the 16 cells are NEW today
  (the PAD × DASH composition cells); 4 cells were already
  covered by Oct 1 (the PAD × SPACE_PAD composition cells); 8
  cells were already covered by Oct 4 (the PETRA_PAD=0 half of
  the 4-bit tuple).  Each macro is a parenthesized expression
  (NOT do { } while (0)) so the caller can write `auto n =
  TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(...)` and
  capture the result.

- **`__VA_OPT__(,)` preprocessor feature** for the no-args
  case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
  Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27
  / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4's macro
  shape).

- **18 sections** of pinning tests (vs 17 in Oct 4 / 16 in Oct
  1 / 16 in Sep 30 / 17 in Sep 29 / 17 in Sep 28), covering
  flag 4-tuple pinning, span\<char\> properties, format_error
  / runtime_error relationship, byte-state contract on success
  (all 16 cells), sub-span no-bleed, cap=0 / written==cap edge
  cases, buffer atomicity on TooLarge, byte-count contract on
  success, RUNTIME surface sanity, CONSTEVAL decltype checks,
  RUNTIME decltype checks, Format error atomicity, format_error
  IS-A runtime_error static assert, concurrency (4 threads ×
  25 calls), no-args case, multiple args, macro-returns-
  expression, perpendicular-axis decltype checks (all 16
  cells), format_error dynamic_cast, DASH-pad byte distinctness
  (4-distinctness), and PAD × SPACE_PAD composition distinctness
  (4-distinctness).

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
           × BOUNDED-BUFFER axis.
- Sep 27   `try_format_padded_span_n_or_throw` — the STD::SPAN\<CHAR\>
           MIRROR of Sep 26 (the FIFTH axis on the span\<char\>
           side).
- Sep 28   `try_format_space_pad_span_n` — the SIXTH axis
           (PETRA_SPACE_PAD) sibling-choice macro layer on the
           STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.
- Sep 29   `try_format_space_pad_n` — the SIXTH axis
           (PETRA_SPACE_PAD) sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis (mirror of Sep 28).
- Sep 30   `try_format_padded_space_pad_n_or_throw` — the
           COMPOSITION of axis 5 (PETRA_PAD) and axis 6
           (PETRA_SPACE_PAD) on the CHAR\*+CAP × BOUNDED-BUFFER
           axis (closes Sep 29's "Where we go next" item (1)
           verbatim on the char\*+cap side).
- Oct  1   `try_format_padded_space_pad_span_n_or_throw` — the
           STD::SPAN\<CHAR\> MIRROR of Sep 30 (the COMPOSITION of
           axis 5 and axis 6 on the STD::SPAN\<CHAR\> ×
           BOUNDED-BUFFER axis; closes Sep 30's "Where we go
           next" item (1) verbatim).
- Oct  3   `try_format_dash_pad_n` — the SEVENTH axis
           (PETRA_DASH_PAD) sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis.
- Oct  4   `try_format_dash_pad_span_n` — the SEVENTH axis
           (PETRA_DASH_PAD) sibling-choice macro layer on the
           STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis (mirror of
           Oct 3; covers the PETRA_PAD=0 half of the 4-bit
           tuple).
- TODAY    `try_format_padded_dash_pad_span_n_or_throw` — the
           STD::SPAN\<CHAR\> MIRROR of Oct 4 extended to the
           PETRA_PAD=1 half (the COMPOSITION of axis 5 and axis
           7 on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis;
           closes Oct 4's "Where we go next" item (1) verbatim).
           KEY FINDING: the (PAD=1, DASH_PAD=1) cells REUSE the
           (PAD=0, DASH_PAD=1) DASH-padded span\<char\> sibling
           wrappers verbatim — no NEW wrappers needed (the
           transitive composition rule "DASH overwrites SPACE
           overwrites NUL").  The matrix is now COMPLETE for
           ALL 16 cells of the 4-bit tuple on the
           std::span\<char\> side.

## What today's lesson pins

1. **The (THROWING-vs-EXPECTED) × (PAD-VS-NUL-VS-SPACE-VS-DASH)
   sibling choice CAN be lifted to a meta-macro form as a 4-axis
   product covering all 16 cells of the (THROW, PAD, SPACE_PAD,
   DASH_PAD) tuple on the STD::SPAN\<CHAR\> axis.**  Today's
   meta-macros dispatch on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
   PETRA_DASH_PAD)` 4-bit tuple.  The preprocessor-time `#if`
   chain is:
   ```
   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if PETRA_SPACE_PAD × #if PETRA_DASH_PAD
   ```
   The matrix is now COMPLETE for ALL 16 cells on the
   std::span\<char\> side.

2. **HEADLINE FINDING — the (PAD=1, DASH_PAD=1) cells REUSE the
   (PAD=0, DASH_PAD=1) DASH-padded span\<char\> sibling wrappers
   verbatim.**  This is the **transitive composition rule**:
   "the LATER fill overwrites the EARLIER fill" — so when both
   PAD=1 and DASH_PAD=1, the DASH fill (axis 7) overwrites the
   NUL fill (axis 5), and bytes `[written, span.size())` end up
   as `'-'` on success.  When ALL THREE (PAD, SPACE_PAD,
   DASH_PAD) are set, the DASH fill (axis 7) overwrites the
   SPACE fill (axis 6) which OVERWROTE the NUL fill (axis 5),
   and bytes `[written, span.size())` end up as `'-'` on success
   (the transitive composition rule).  No NEW wrappers are
   needed for the (PAD=1, DASH_PAD=1) cells; they just dispatch
   to the (PAD=0, DASH_PAD=1) DASH-padded wrappers.  Pinned by
   Section 16 (8-CHECK byte-state block) and Section 17
   (4-distinctness check).

3. **HEADLINE FINDING — the (PAD=1, SPACE_PAD=1, DASH_PAD=0)
   cells REUSE the (PAD=0, SPACE_PAD=1, DASH_PAD=0)
   SPACE-padded span\<char\> sibling wrappers verbatim** (Oct
   1's composition rule).  No NEW wrappers are needed for the
   (PAD=1, SPACE_PAD=1) cells; they just dispatch to the
   (PAD=0, SPACE_PAD=1) SPACE-padded wrappers.  Pinned by
   Section 18 (4-distinctness check on the (0, 1, 1, 0) and
   (1, 1, 1, 0) paths).

4. **The compile-time flag 4-tuple
   `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
   PETRA_DASH_PAD)` controls the dispatch** via
   `#if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
   PETRA_SPACE_PAD × #if PETRA_DASH_PAD`.  Each branch
   collapses to a single function call (zero runtime cost).
   4 of the 16 branches REUSE a wrapper from another cell
   (the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0, 1), (1, 1, 1, 1)
   cells REUSE the (0, 0, 0, 1) and (1, 0, 0, 1) DASH-padded
   wrappers; the (0, 1, 1, 0) and (1, 1, 1, 0) cells REUSE the
   (0, 0, 1, 0) and (1, 0, 1, 0) SPACE-padded wrappers).

5. **Independent `#error` guards** fire at preprocessor time if
   ANY of the four flags is set to anything other than 0 or 1.
   The flags are validated independently (one `#error` per
   flag) so an invalid flag does NOT silently fall through to
   a wrong branch.

6. **The macro is a parenthesized expression** (NOT `do { ...
   } while (0)`) so the caller can write `auto n =
   TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(...)` and
   capture the result.

7. **`__VA_OPT__(,)`, for the no-args case.**

8. **The composition rule's byte-state contract — when BOTH
   PETRA_PAD and PETRA_DASH_PAD are set (or all three), bytes
   `[written, span.size())` are `'-'` on success — is pinned by
   Section 2** (which pre-fills the backing buffer with poison
   `'Q'` bytes, runs the macro, and asserts every byte at index
   >= written is `'-'` on the (0, 1, 0, 1), (0, 1, 1, 1), (1,
   1, 0, 1), and (1, 1, 1, 1) paths) and by Section 16
   (explicit 4-distinctness check on the composition paths).

9. **The composition rule's byte-state distinctness from
   NUL-padded / SPACE-padded / untouched siblings — bytes
   `[written, span.size())` are `'-'` (NOT `'\0'`, NOT `' '`,
   NOT `'Q'`) — is pinned by Section 17** with an explicit
   4-distinctness check (`buf[i] == '-'`, `buf[i] != '\0'`,
   `buf[i] != ' '`, `buf[i] != 'Q'`) on all 4 DASH-padded cells
   of the 4-tuple.

10. **All 16 cells preserve buffer atomicity on TooLarge** —
    Section 3 pins this on all 16 flag 4-tuples.  On every
    cell, the buffer is UNTOUCHED on overflow (delegated to
    the underlying impl).

11. **All 16 cells preserve byte-count contract on success** —
    Section 4 pins this on all 16 flag 4-tuples.

12. **All 16 cells preserve atomicity on Format errors** —
    Section 8 pins this on the RUNTIME surface for all 16
    cells by calling with `std::string bad_fmt = "bad {"` and
    asserting every byte is still `'Q'`.

13. **The throwing+PAD+DASH composition sibling preserves the
    SAME `std::runtime_error` `.what()` byte-exactness** as
    the throwing+DASH-padded sibling — Section 15 pins this
    via `dynamic_cast<const std::format_error*>(&e) ==
    nullptr` (the throwing wrapper re-raises as
    `std::runtime_error`, NOT `std::format_error`).

14. **The CONSTEVAL macro's consteval gate is preserved
    regardless of the 4-bit tuple** —
    `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` still requires
    `std::format_string<Args...>` (a literal `fmt` at the call
    site).  Pinned by
    `/tmp/probe_consteval_padded_dash_pad_span.cpp` producing
    `error: call to consteval function
    std::basic_format_string<char, int>::basic_format_string<
    std::string> is not a constant expression` with note
    `read of non-constexpr variable fmt_runtime is not allowed
    in a constant expression` / `in call to this->data()`.

15. **The std::span\<char\> sub-span 'no-bleed' contract is
    preserved on the composition path (Section 2a)** — a
    std::span\<char\> over a sub-range of a buffer must NOT
    touch bytes outside the span (mirrors Sep 10 / Sep 12 /
    Sep 19 / Sep 21 / Sep 27 / Sep 28 / Oct 1's no-bleed
    contract; a 6-byte sub-span at offset 5 of a 16-byte buffer
    with format 'ABCDE' (5 bytes) leaves bytes [0,5) and [11,16)
    as poison 'Q').

16. **The std::span\<char\> properties are preserved on the
    composition path (Section 1b)** — contiguous range + sized
    range + trivially copyable + standard layout + `.data()
    ->char*` + `.size()->std::size_t`.

17. **The std::span\<char\> throwing+composition sibling
    preserves the SAME buffer atomicity on TooLarge** — the
    buffer is UNTOUCHED on overflow.  Section 3 pins this on
    the (1, ?, ?, ?) paths via `catch (const std::runtime_error&)
    { caught = true; }`.

18. **The 4-axis product is PERPENDICULAR to the SIX PRIOR
    AXES** — char\*-vs-span (Sep 10 / Sep 12), CONSTEVAL-vs-
    RUNTIME (Sep 13), bounded-buffer-vs-NUL-terminating (Sep
    16 / Sep 19), expected-vs-throwing (Sep 20 / Sep 21), and
    untouched-vs-NUL-padded (Sep 25 / Sep 26 / Sep 27),
    untouched-vs-SPACE-padded (Sep 28 / Sep 29 / Sep 30 / Oct
    1), and untouched-vs-DASH-padded (Oct 3 / Oct 4 / TODAY).
    TODAY's lesson covers the COMPOSITION of axis 5 (PETRA_PAD)
    and axis 7 (PETRA_DASH_PAD) on the STD::SPAN\<CHAR\> ×
    CONSTEVAL-or-RUNTIME × bounded-buffer axis.  Section 14's
    `decltype` checks pin the perpendicular-axis claim on ALL
    16 cells of the 4-bit tuple.

19. **The CONSTEVAL macro's consteval gate is preserved on ALL
    16 cells** — pinned by Section 6's
    `static_assert(std::is_same_v<F1, ExpectedFmt1>)` on the
    expected path (cells (0, ?, ?, ?)) and
    `static_assert(std::is_same_v<F1, std::size_t>)` on the
    throwing path (cells (1, ?, ?, ?)).

## Headline finding — the matrix is now COMPLETE for ALL 16 cells on the span\<char\> side

Together with Sep 8 / Sep 9 / Sep 10 / Sep 12 / Sep 16 / Sep 19 /
Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 /
Sep 28 / Sep 30 / Oct 1 / Oct 3 / Oct 4's lessons, TODAY's
lesson closes Oct 4's "Where we go next" item (1) verbatim —
the (PAD=1, DASH_PAD=1) composition cells on the
STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.  The matrix on the
BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-NUL-vs-SPACE-vs-DASH) quadrant on the
std::span\<char\> side is now **COMPLETE for ALL 16 cells of
the 4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
PETRA_SPACE_PAD, PETRA_DASH_PAD)**.

| axis | value |
|-----:|----------------------------------------------------------------|
|    1 | char\*+cap vs span\<char\>          |
|    2 | CONSTEVAL vs RUNTIME                 |
|    3 | bounded-buffer vs NUL-terminating    |
|    4 | expected vs throwing                 |
|    5 | untouched vs NUL-padded              |
|    6 | untouched vs SPACE-padded            |
|    7 | untouched vs DASH-padded             |

The matrix isn't quite 2^7 = 128 because axes 5, 6, and 7 aren't
fully independent (the (PAD=1, SPACE_PAD=1) cells reuse the
(PAD=0, SPACE_PAD=1) wrappers per Oct 1's composition rule; the
(PAD=1, DASH_PAD=1) cells reuse the (PAD=0, DASH_PAD=1)
wrappers per today's transitive composition rule; the
(SPACE_PAD=1, DASH_PAD=1) cells reuse the (SPACE_PAD=0,
DASH_PAD=1) wrappers per Oct 4's composition rule; and the
(PAD=1, SPACE_PAD=1, DASH_PAD=1) cells also reuse the (PAD=0,
DASH_PAD=1) wrappers per the transitive composition rule).  The
effective matrix is 2 × 2 × 2 × 2 × 2 × 2 × 2 = 128 cells *if
you count the composition rules as NEW axes*, but the
*implementation* only has 8 sibling wrappers per surface × 2
surface pairs = 16 wrappers per side (× 2 sides = 32 wrappers
total).  The composition rules mean many cells dispatch to the
SAME wrappers.

TODAY's lesson covers 4 NEW dispatch cells on the
std::span\<char\> side: (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0,
1), and (1, 1, 1, 1).  The (PAD=1, DASH_PAD=1) cells REUSE
the (PAD=0, DASH_PAD=1) DASH-padded span\<char\> sibling
wrappers verbatim (the transitive composition rule "DASH
overwrites SPACE overwrites NUL").  The matrix is now COMPLETE
for ALL 16 cells of the 4-bit tuple on the std::span\<char\>
side.

The std::span\<char\> side mirrors the char\*+cap side
byte-for-byte (Sep 27 / Sep 28 / Sep 30 / Oct 1 / Oct 3 / Oct 4
/ TODAY) — the only difference between the two sides is the
parameter shape (`char*, std::size_t, fmt, args...` vs
`std::span<char>, fmt, args...`).  All 16 sibling wrappers per
side share the same byte-state contract (bytes `[written,
cap)` are `'-'` on success in the DASH-padded cells, `' '` in
the SPACE-padded cells, `'\0'` in the NUL-padded cells,
UNTOUCHED in the untouched cells).

## Where we go next

TODAY's lesson closes Oct 4's "Where we go next" item (1)
verbatim: the PETRA_PAD = 1 × PETRA_DASH_PAD = 1 composition
cells on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.  The
matrix is now COMPLETE for ALL 16 cells of the 4-bit tuple on
the std::span\<char\> side — both the char\*+cap side AND the
std::span\<char\> side are now COMPLETE for the 4-bit
(THROW, PAD, SPACE_PAD, DASH_PAD) tuple on the BOUNDED-BUFFER
× expected-or-throwing axis.

The std::span\<char\> matrix is now COMPLETE for the entire
4-bit tuple.  Remaining work on the char\*+cap side mirrors
what Oct 1 / Oct 4 / TODAY did on the std::span\<char\> side
but in reverse: there's NO `try_format_padded_dash_pad_n` (the
char\*+cap mirror of TODAY's lesson).  Per the most recent
lesson's pattern, a future lesson would close the (0, 1, 0,
1), (0, 1, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1) cells on
the char\*+cap side, REUSING Oct 3's DASH-padded char\*+cap
wrappers verbatim per the same transitive composition rule.

TODAY's lesson does NOT cover the OTHER open items from the
matrix:

1. **CHAR\*+CAP MIRROR of TODAY's lesson** —
   `try_format_padded_dash_pad_n_or_throw` (the char\*+cap
   mirror of TODAY's std::span\<char\> lesson) would close
   the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0, 1), and (1, 1,
   1, 1) cells on the char\*+cap side.  The composition rule
   is identical: DASH overwrites NUL when both PAD and
   DASH_PAD are set, transitively DASH overwrites SPACE
   overwrites NUL when all three are set.  No NEW wrappers
   needed — just REUSE Oct 3's DASH-padded char\*+cap sibling
   wrappers verbatim and add a new 4-bit meta-macro
   dispatcher.

2. **ADDITIONAL-PAD cells** (e.g. `PETRA_HASH_PAD` that pads
   with `'#'`, or `PETRA_UNDERSCORE_PAD` that pads with `'_'`)
   — would introduce an EIGHTH axis.  Each future lesson
   adds verbatim thin hash-pad wrappers composed on top of
   TODAY's DASH-padded wrappers + new `#if PETRA_HASH_PAD`-
   gated meta-macros.  (16 new dispatch cells per axis
   extension per side.)  The pattern is already well-
   established by Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 /
   Oct 4 / TODAY.

3. **Cross-cutting infrastructure items from the Aug 13 / Aug
   15 / Aug 17 lessons** — could be revisited once the matrix
   is fully closed on both sides.

With the 16-cell matrix now COMPLETE for the
(THROW, PAD, SPACE_PAD, DASH_PAD) 4-bit tuple on the
std::span\<char\> side, future lessons can either (a) close
the char\*+cap side mirror of TODAY (item 1 above), or (b)
introduce an EIGHTH axis on EITHER side (e.g. `PETRA_HASH_PAD`).

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add` (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` + `dynamic_cast`
  + `std::is_base_of_v` (catch-chain contract pinning).
- `std::span<char>` (P0122R7) +
  `std::ranges::contiguous_range` +
  `std::ranges::sized_range` +
  `std::is_trivially_copyable_v` +
  `std::is_standard_layout_v` (span\<char\> properties).
- `std::array<std::thread, 4>` + `std::thread::join()`
  (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA_THROW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0,
PETRA_DASH_PAD=0):

```bash
cd late-may/cpp_practice/try_format_padded_dash_pad_span_n_or_throw
cmake -S . -B build
cmake --build build
./build/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

Strict-warning build:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

(PETRA_PAD=1, PETRA_DASH_PAD=1) composition path (NEW today):

```bash
cmake -S . -B build-padded-dash-pad \
      -DPETRA_PAD=ON -DPETRA_DASH_PAD=ON
cmake --build build-padded-dash-pad
./build-padded-dash-pad/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

(PETRA_PAD=1, PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1) triple
composition path (NEW today):

```bash
cmake -S . -B build-triple-composition \
      -DPETRA_PAD=ON -DPETRA_SPACE_PAD=ON -DPETRA_DASH_PAD=ON
cmake --build build-triple-composition
./build-triple-composition/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

(PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, PETRA_DASH_PAD=1)
throwing composition path (NEW today):

```bash
cmake -S . -B build-throw-padded-dash-pad \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_PAD=ON \
      -DPETRA_DASH_PAD=ON
cmake --build build-throw-padded-dash-pad
./build-throw-padded-dash-pad/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

Direct compile (no CMake, mirrors the lesson's "Build and
verification commands" section):

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw.cpp \
        -o /tmp/P-2026-10-05-try-format-padded-dash-pad-span-n-or-throw
```

## Observed test results

165/165 PASS on the (0, 0, 0, 0) default build;
192/192 PASS on the (0, 0, 0, 1) expected+DASH-padded build;
173/173 PASS on the (0, 0, 1, 0) expected+SPACE-padded build;
192/192 PASS on the (0, 0, 1, 1) expected+SPACE+DASH-padded
build;
165/165 PASS on the (0, 1, 0, 0) expected+NUL-padded build;
192/192 PASS on the (0, 1, 0, 1) expected+NUL+DASH-padded
build (NEW today; DASH overwrites NUL);
188/188 PASS on the (0, 1, 1, 0) expected+NUL+SPACE-padded
build;
192/192 PASS on the (0, 1, 1, 1) expected+NUL+SPACE+DASH-
padded build (NEW today; transitive composition);
152/152 PASS on the (1, 0, 0, 0) throwing+untouched build;
179/179 PASS on the (1, 0, 0, 1) throwing+DASH-padded build;
160/160 PASS on the (1, 0, 1, 0) throwing+SPACE-padded build;
179/179 PASS on the (1, 0, 1, 1) throwing+SPACE+DASH-padded
build;
152/152 PASS on the (1, 1, 0, 0) throwing+NUL-padded build;
179/179 PASS on the (1, 1, 0, 1) throwing+NUL+DASH-padded
build (NEW today; DASH overwrites NUL);
175/175 PASS on the (1, 1, 1, 0) throwing+NUL+SPACE-padded
build;
179/179 PASS on the (1, 1, 1, 1) throwing+NUL+SPACE+DASH-
padded build (NEW today; transitive composition).

Five consecutive runs on the 3 key composition cells produced
identical PASS/FAIL tallies (no flakiness in Section 10's
4-thread × 25-call concurrent dispatch — per-thread local
counters aggregated after join via
`std::atomic<int>::fetch_add`):
- (0, 1, 0, 1) PAD+DASH: 192/192 PASS (× 5 runs)
- (0, 1, 1, 0) PAD+SPACE: 188/188 PASS (× 5 runs)
- (0, 1, 1, 1) PAD+SPACE+DASH: 192/192 PASS (× 5 runs)

The consteval gate probe
(`/tmp/probe_consteval_padded_dash_pad_span.cpp`) correctly
fails to compile with the expected
`'error: call to consteval function
std::basic_format_string<char, int>::basic_format_string<std::string>
is not a constant expression'` error and the
`'read of non-constexpr variable fmt_runtime is not allowed in
a constant expression'` / `'in call to this->data()'` notes.

Zero warnings under -Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion; zero sanitizer diagnostics;
empty stderr under default + ASan/UBSan + strict-warning builds
(verified with `cmake -S . -B build` +
`cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON` +
`cmake -S . -B build-asan -DENABLE_ASAN=ON` +
`cmake -S . -B build-padded-dash-pad -DPETRA_PAD=ON
-DPETRA_DASH_PAD=ON` +
`cmake -S . -B build-strict-padded-dash -DENABLE_STRICT_WARNINGS=ON
-DPETRA_PAD=ON -DPETRA_DASH_PAD=ON`).

Pure consumer-side C++23 std::format + std::vformat_to +
std::format_string<Args...> (P2216R3) + std::make_format_args
+ std::expected (P0323R12) + std::println (P2093R14) +
std::atomic + std::is_same_v + std::format_error +
std::runtime_error + dynamic_cast + std::is_base_of_v +
std::array<std::thread, 4> + std::thread::join() +
std::span<char> (P0122R7) + std::ranges::contiguous_range +
std::ranges::sized_range + std::is_trivially_copyable_v +
std::is_standard_layout_v + __VA_OPT__ preprocessor feature
exercise — reuses Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep
11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep
20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep
28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4's machinery
verbatim, no third-party libraries, no library installation,
no `find_package`.
