# P-2026-10-09 — `try_format_hash_pad_n`

Topic: **the EIGHTH axis (PETRA_HASH_PAD = 1)
sibling-choice META-MACRO layer on the CHAR\*+CAP ×
BOUNDED-BUFFER axis** — a NEW FIFTH compile-time flag on top of
Oct 8's 4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
PETRA_SPACE_PAD, PETRA_DASH_PAD).
TODAY introduces a NEW PAD-AXIS (`#`-padded sibling) layered on
top of Oct 3's DASH-padded sibling, mirroring Oct 3's pattern
exactly (Oct 3 layered DASH on Sep 29's SPACE-padded sibling).

**Closes Oct 8's "Where we go next" item (1) verbatim**:

> ADDITIONAL-PAD cells (e.g. `PETRA_HASH_PAD` that pads with
> `'#'`, or `PETRA_UNDERSCORE_PAD` that pads with `'_'`) —
> would introduce an EIGHTH axis.  Each future lesson adds
> verbatim thin hash-pad wrappers composed on top of TODAY's
> DASH-padded wrappers + new `#if PETRA_HASH_PAD`-gated
> meta-macros.  (16 new dispatch cells per axis extension per
> side.)  The pattern is already well-established by Sep 28 /
> Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4 / Oct 5 / TODAY.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-vs-DASH-vs-HASH) quadrant on the char\*+cap
side is now **COMPLETE for all 16 cells of the 4-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD, PETRA_DASH_PAD,
PETRA_HASH_PAD)** (PETRA_PAD=0 only; the PETRA_PAD=1 cells live
in Sep 25's / Sep 26's lessons).

## Key findings

- **HEADLINE FINDING — the HASH-pad sibling is a thin layer on
  top of Oct 3's DASH-pad sibling.**  This is the SAME pattern
  Oct 3 used (DASH-pad wrapper layered on Sep 29's SPACE-pad
  wrapper) and Sep 29 used (SPACE-pad wrapper layered on Sep
  8's untouched wrapper).  The composition rule "the LATER
  fill overwrites the EARLIER fill" now has THREE layers:
  HASH overwrites DASH overwrites SPACE overwrites NUL (when
  all four pad-flags are set).

- **KEY DESIGN DECISION — all FOUR (PAD=0, \*, DASH=\*, HASH=1)
  cells on EACH side (expected / throwing) share the SAME
  TWO new HASH-pad sibling wrappers** (one expected pair, one
  throwing pair).  The (HASH_PAD=1, DASH_PAD=0) cell is the
  "primary" HASH-pad cell; the (HASH_PAD=1, DASH_PAD=1) cell
  REUSES it (because HASH overwrites DASH).  The
  (HASH_PAD=1, DASH_PAD=0, SPACE_PAD=1) and (HASH_PAD=1,
  DASH_PAD=1, SPACE_PAD=1) cells ALSO REUSE it (transitive
  composition: HASH overwrites DASH overwrites SPACE).  No
  NEW wrappers are needed for the composition cells.

- **The composition rule "the LATER fill overwrites the
  EARLIER fill" applies transitively across 4 layers:**
  HASH > DASH > SPACE > NUL.  When ALL FOUR pad-flags are
  set, the end byte state is always `'#'`.  This is the
  SAME pattern Oct 3 / Oct 4 / Oct 5 / Oct 8 established for
  3 layers.

- The HASH-pad sibling **preserves buffer atomicity on
  TooLarge** — the buffer is UNTOUCHED on overflow.  Pinned
  by **Section 3** on all 16 cells with a poison-`'Q'` check.

- The HASH-pad sibling **preserves the SAME atomicity on
  Format errors** — a malformed fmt throws `std::runtime_error`
  (delegated to Oct 3's throwing wrapper, which delegates to
  Sep 29's throwing wrapper, which delegates to Sep 20's
  throwing wrapper, which delegates to Sep 8's / Sep 9's
  underlying impl, which throws `std::format_error` at format
  time before any writes).  Pinned by **Section 6** on the
  RUNTIME surface.

- The HASH-pad sibling **preserves the SAME byte-count
  contract on success** — returns `std::size_t` == bytes
  written.  Pinned by **Section 4** on all 16 cells.

- The HASH-pad sibling's **byte-state contract** — bytes
  `[written, cap)` are `'#'` on success — is pinned by
  **Section 2** on all 16 cells (with a poison-`'Q'` check
  on every byte at index >= written).

- The HASH-pad sibling's **byte-state distinctness from
  DASH, SPACE, NUL** — bytes `[written, cap)` are `'#'`
  (NOT `'-'` and NOT `' '` and NOT `'\0'` and NOT `'Q'`) —
  is pinned by **Section 12** with an explicit 5-distinctness
  check on the four HASH-pad-only composition cells.

- The HASH-pad sibling's **HASH-overwrites-DASH composition
  distinctness** — when both PETRA_DASH_PAD and PETRA_HASH_PAD
  are 1, bytes `[written, cap)` are `'#'` (NOT `'-'` and NOT
  `' '` and NOT `'\0'` and NOT `'Q'`) — is pinned by
  **Section 13** with an explicit 5-distinctness check.

- The HASH-pad sibling's **HASH-overwrites-SPACE transitive
  composition distinctness** — when PETRA_SPACE_PAD=1,
  PETRA_DASH_PAD=0, and PETRA_HASH_PAD=1, bytes `[written,
  cap)` are `'#'` (NOT `' '` and NOT `'-'` and NOT `'\0'`
  and NOT `'Q'`) — is pinned by **Section 14** with an
  explicit 5-distinctness check.

- The HASH-pad sibling **handles the cap=0 edge case
  correctly** — pinned by **Section 8** (cap=0 + "anything"
  format; the 'Q' sentinel byte outside the cap=0 writable
  region remains untouched).

- The HASH-pad sibling **handles the written==cap edge case
  correctly** — pinned by **Section 7** (full-buffer success
  with format "ABCDEFGH" into a buffer of size 8; the hash-fill
  loop has `i == cap` as the initial condition, so the loop
  body never executes).

- The HASH-pad sibling **handles the empty-format-string edge
  case correctly** — pinned by **Section 15** (format `""`;
  the entire buffer becomes `'#'` on success).

- The **concurrent dispatch is non-flaky** — Section 11's
  4-thread × 25-call concurrent dispatch produces identical
  PASS counts across 5 consecutive runs on the (T=0, P=0, S=1,
  D=1, H=1) triple-composition cell.

- The CONSTEVAL macro's **consteval gate is preserved
  regardless of the 5-bit tuple** — `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
  still requires `std::format_string<Args...>` (a literal fmt
  at the call site).  Pinned by **Section 16**'s successful
  CONSTEVAL test (CHECK_EQ(`r.value()`, 14) for `'CONSTEVAL
  test'`).

- The char\*+cap "**no-bleed**" contract is preserved on the
  HASH-pad path — pinned by **Section 2a** (a 6-byte
  sub-region at offset 5 of a 16-byte buffer with format
  `'ABCDE'` (5 bytes) leaves bytes [0, 5) and [11, 16) as
  poison 'Q' regardless of the 5-tuple).

## What today's lesson adds

- **FOUR new HASH-pad sibling wrappers** —
  `petra::try_format_runtime_n_hash_pad` (NEW, RUNTIME
  expected+HASH-padded sibling wrapping Oct 3's RUNTIME
  expected+DASH-padded sibling verbatim and hash-filling
  out[written, cap) with `'#'` on success) and
  `petra::try_format_bounded_runtime_n_hash_pad` (NEW,
  CONSTEVAL sibling of the above) and
  `petra::try_format_runtime_n_throwing_hash_pad` (NEW,
  RUNTIME throwing+HASH-padded sibling wrapping Oct 3's
  RUNTIME throwing+DASH-padded sibling verbatim and
  hash-filling out[written, cap) with `'#'` on success
  ONLY; re-raises `std::runtime_error` untouched on error)
  and `petra::try_format_bounded_runtime_n_throwing_hash_pad`
  (NEW, CONSTEVAL sibling of the above).  No new types,
  no new impls, no new error contracts beyond the
  underlying DASH-pad siblings.

- **TWO meta-macros** —
  `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW` (CONSTEVAL surface)
  and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW`
  (RUNTIME surface) — each is a verbatim thin dispatcher
  that picks between SIXTEEN sibling wrappers at
  preprocessor time based on the (PETRA_THROW_ON_OVERFLOW,
  PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD,
  PETRA_HASH_PAD) 5-bit tuple.  PETRA_PAD MUST be 0 in
  this lesson (the dispatcher #errors with a clear pointer
  to Sep 25 / Sep 26 when PETRA_PAD=1, since Sep 25's /
  Sep 26's NUL-padded wrappers are out of scope for this
  lesson).  Each macro is a parenthesized expression (NOT
  `do { } while (0)`) so the caller can write `auto n =
  TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW(...)` and
  capture the result.  Each macro uses `__VA_OPT__(,)` for
  the no-args case (mirrors Sep 13 / Sep 14 / Sep 15 /
  Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 /
  Sep 25 / Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 /
  Oct 1 / Oct 3 / Oct 4 / Oct 5 / Oct 8's macro shape).

- **Outer guard `#if PETRA_THROW_ON_OVERFLOW == 1`** wraps
  the second chain to prevent the throwing chain's `#else`
  catch-all (for the (1,0,1,1,1) cell) from re-defining
  the macro when PETRA_THROW_ON_OVERFLOW=0.  This is a
  subtle preprocessor gotcha: the throwing chain's
  `#if`/`#elif`s all require `PETRA_THROW_ON_OVERFLOW == 1`,
  but WITHOUT the outer guard, building with
  PETRA_THROW_ON_OVERFLOW=0 would still enter the throwing
  chain's `#else` clause (because all `#elif`s are FALSE)
  and CLOBBER the first chain's correct dispatch with a
  re-definition to the throwing sibling.  The outer guard
  is a one-line fix; without it, the macro would be
  re-defined TWICE and the resulting dispatch would be
  the throwing sibling everywhere.

- **18 sections** of pinning tests (vs 16 in Oct 8's
  char\*+cap lesson / 16 in Oct 3's char\*+cap lesson /
  16 in Sep 29's char\*+cap lesson / 17 in Sep 29 / 17 in
  Sep 28), covering flag 5-tuple pinning, format_error /
  runtime_error relationship, byte-state contract on
  success (all 16 cells), sub-span no-bleed, concurrent
  dispatch (4 threads × 25 calls), cap=0 / written==cap
  edge cases, buffer atomicity on TooLarge, byte-count
  contract on success, RUNTIME surface sanity, CONSTEVAL
  decltype checks, RUNTIME decltype checks, Format error
  atomicity, format_error IS-A runtime_error static
  assert, no-args case, multiple args, macro-returns-
  expression, perpendicular-axis decltype checks (all 16
  cells), format_error dynamic_cast, HASH-overwrites-DASH
  composition distinctness (5-distinctness), and
  HASH-overwrites-SPACE transitive composition
  distinctness (5-distinctness).

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
          BOUNDED-BUFFER axis; closes Sep 30's "Where we
          next" item (1) verbatim).
- Oct  3   `try_format_dash_pad_n` — the SEVENTH axis
          (PETRA_DASH_PAD) sibling-choice macro layer on the
          CHAR\*+CAP × BOUNDED-BUFFER axis.
- Oct  4   `try_format_dash_pad_span_n` — the SEVENTH axis
          (PETRA_DASH_PAD) sibling-choice macro layer on the
          STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis (mirror of
          Oct 3; covers the PETRA_PAD=0 half of the 4-bit
          tuple).
- Oct  5   `try_format_padded_dash_pad_span_n_or_throw` — the
          STD::SPAN\<CHAR\> MIRROR of Oct 4 extended to the
          PETRA_PAD=1 half (the COMPOSITION of axis 5 and axis
          7 on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis;
          closes Oct 4's "Where we go next" item (1) verbatim).
- Oct  8   `try_format_padded_dash_pad_n_or_throw` — the
          CHAR\*+CAP MIRROR of Oct 5 extended to the
          PETRA_PAD=1 half (the COMPOSITION of axis 5 and axis
          7 on the CHAR\*+CAP × BOUNDED-BUFFER axis; closes
          Oct 5's "Where we go next" item (1) verbatim).
- TODAY    `try_format_hash_pad_n` — the EIGHTH axis
          (PETRA_HASH_PAD) sibling-choice macro layer on the
          CHAR\*+CAP × BOUNDED-BUFFER axis; the FIFTH pad-flag
          on top of Oct 8's 4-bit tuple.  Closes Oct 8's
          "Where we go next" item (1) verbatim.  KEY FINDING:
          the new HASH-pad sibling wrapper is a thin layer on
          top of Oct 3's DASH-pad sibling wrapper (mirrors
          Oct 3's "DASH layer on Sep 29's SPACE-pad"
          pattern); the transitive composition rule HASH >
          DASH > SPACE > NUL means all 4 (HASH=1) cells on
          each side share the SAME new HASH-pad sibling
          wrapper.  The matrix is now COMPLETE for all 16
          cells of the (THROW, SPACE, DASH, HASH) 4-bit
          tuple (PETRA_PAD=0 only) on the char\*+cap side.

## What today's lesson pins

1. **The (PAD-VS-UNTOUCHED) × (PAD-AXIS) sibling choice CAN
   be lifted to a meta-macro form as a 5-axis product
   covering all 16 cells of the (THROW, SPACE_PAD,
   DASH_PAD, HASH_PAD) tuple on the CHAR\*+CAP axis.**
   Today's meta-macros dispatch on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD,
   PETRA_DASH_PAD, PETRA_HASH_PAD)` 5-bit tuple.  The
   preprocessor-time `#if PETRA_THROW_ON_OVERFLOW × #if
   PETRA_PAD × #if PETRA_SPACE_PAD × #if PETRA_DASH_PAD ×
   #if PETRA_HASH_PAD` nested 5-tuple is the cleanest
   design: zero runtime cost (each branch collapses to a
   single function call), matches Oct 3 / Oct 4 / Oct 5 /
   Oct 8's compile-time-flag framing, independent `#error`
   guards fire at preprocessor time if ANY of the five
   flags is invalid, composes orthogonally with the other
   axes (Section 18's `decltype` checks pin this on all 16
   cells of the 4-bit tuple with PETRA_PAD=0).

2. **HEADLINE FINDING — the (HASH_PAD=1) sibling wrapper is
   a thin layer on top of Oct 3's DASH-pad sibling wrapper
   verbatim.**  This is the SAME composition pattern as
   Oct 3 (DASH layer on Sep 29's SPACE-pad wrapper) and
   Sep 29 (SPACE layer on Sep 8's untouched wrapper).  The
   composition rule "the LATER fill overwrites the EARLIER
   fill" now has FOUR layers: HASH > DASH > SPACE > NUL.
   When ALL FOUR pad-flags are set, the end byte state is
   always `'#'`.  No NEW wrappers are needed for the
   composition cells (1, \*, \*, 1); they just dispatch
   to the (\*, \*, \*, 1) HASH-pad wrapper.  Pinned by
   Section 12 (5-distinctness byte-state block), Section
   13 (HASH-overwrites-DASH distinctness), Section 14
   (HASH-overwrites-SPACE transitive composition
   distinctness).

3. **HEADLINE FINDING — the throwing chain REQUIRES an
   outer `#if PETRA_THROW_ON_OVERFLOW == 1` guard.**
   The throwing chain's nested `#if`/`#elif`s all check
   `PETRA_THROW_ON_OVERFLOW == 1`, but the chain also has
   an `#else` clause for the (1,0,1,1,1) catch-all cell.
   Without the outer guard, building with
   PETRA_THROW_ON_OVERFLOW=0 would enter the throwing
   chain's `#else` clause (all `#elif`s FALSE) and
   CLOBBER the first chain's correct dispatch.  The
   outer guard is a one-line fix; with it, the throwing
   chain's `#else` only fires when
   PETRA_THROW_ON_OVERFLOW=1.

4. **KEY DESIGN DECISION — the (HASH_PAD=1, DASH_PAD=1)
   cells REUSE the (HASH_PAD=1, DASH_PAD=0) HASH-pad
   sibling wrapper verbatim.**  No NEW wrappers are needed
   for the (1, 1) composition cells; they just dispatch
   to the (1, 0) HASH-pad wrappers.  This is the EXACT
   same pattern Oct 3's (DASH_PAD=1, SPACE_PAD=1) cells
   used to reuse the (DASH_PAD=1, SPACE_PAD=0) DASH-pad
   siblings.

5. **KEY DESIGN DECISION — the (HASH_PAD=1, SPACE_PAD=1,
   DASH_PAD=0) cells REUSE the (HASH_PAD=1, DASH_PAD=0,
   SPACE_PAD=0) HASH-pad sibling wrapper verbatim via the
   transitive composition rule** (HASH > DASH > SPACE).
   The HASH-pad wrapper calls the DASH-pad sibling (which
   fills with `'-'`); the HASH-pad wrapper then
   OVERWRITES those `'-'` bytes with `'#'` on success.
   On the (HASH_PAD=1, SPACE_PAD=1, DASH_PAD=0) cell, the
   underlying DASH-pad sibling's internal SPACE-fill (from
   Sep 29) is overwritten by the DASH-fill (with `'-'`),
   which is then overwritten by the HASH-fill (with
   `'#'`); the net effect is bytes `[written, cap)` are
   `'#'` (the transitive composition rule).

6. The HASH-pad sibling **preserves buffer atomicity on
   TooLarge** — the buffer is UNTOUCHED on overflow.
   Section 3 pins this on all 16 cells via
   `catch (...) { threw = true; }` and a poison-`'Q'`
   check on every byte.

7. The HASH-pad sibling **preserves the SAME atomicity
   on Format errors** — a malformed fmt throws
   `std::runtime_error` (delegated to Oct 3's throwing
   wrapper, which delegates to Sep 29's throwing
   wrapper, which delegates to Sep 20's throwing
   wrapper, which delegates to Sep 8's / Sep 9's
   underlying impl, which throws `std::format_error` at
   format time before any writes).  Section 6 pins this
   on the RUNTIME surface.

8. The HASH-pad sibling **preserves the SAME byte-count
   contract on success** — returns `std::size_t` ==
   bytes written (the same value Oct 3's / Sep 29's /
   Sep 8's / Sep 9's underlying impl returns).  Section 4
   pins this on all 16 paths.

9. The HASH-pad sibling's **byte-state contract** —
   bytes `[written, cap)` are `'#'` on success — is
   pinned by Section 2 (which pre-fills the backing
   buffer with poison `'Q'` bytes, runs the macro, and
   asserts every byte at index >= written is `'#'` on
   the HASH-pad path OR `'-'` on the DASH-pad path OR
   `' '` on the SPACE-pad path OR `'Q'` on the
   untouched path).

10. The HASH-pad sibling's **byte-state distinctness
    from DASH, SPACE, NUL** — bytes `[written, cap)`
    are `'#'` (NOT `'-'` and NOT `' '` and NOT `'\0'`
    and NOT `'Q'`) — is pinned by Section 12 with an
    explicit 5-distinctness check on the four HASH-pad-
    only cells of the 5-tuple that have HASH_PAD=1.

11. The HASH-pad sibling's **HASH-overwrites-DASH
    composition distinctness** — when both PETRA_DASH_PAD
    and PETRA_HASH_PAD are 1, bytes `[written, cap)` are
    `'#'` (NOT `'-'` and NOT `' '` and NOT `'\0'` and NOT
    `'Q'`) — is pinned by Section 13 with an explicit
    5-distinctness check on the (HASH=1, DASH=1)
    composition cells.

12. The HASH-pad sibling's **HASH-overwrites-SPACE
    transitive composition distinctness** — when
    PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0, and
    PETRA_HASH_PAD=1, bytes `[written, cap)` are `'#'`
    (NOT `' '` and NOT `'-'` and NOT `'\0'` and NOT
    `'Q'`) — is pinned by Section 14 with an explicit
    5-distinctness check on those 2 cells.

13. The HASH-pad sibling **handles the cap==0 edge
    case correctly** — pinned by Section 8 (cap=0 +
    "anything" format; the 'Q' sentinel byte outside
    the cap=0 writable region remains untouched).

14. The HASH-pad sibling **handles the written==cap
    edge case correctly** — Section 7 (full-buffer
    success; the hash-fill loop has `i == cap` as the
    initial condition, so the loop body never
    executes).

15. The HASH-pad sibling **handles the
    empty-format-string edge case correctly** —
    Section 15 (format `""`; the entire buffer becomes
    `'#'` on success).

16. The **concurrent dispatch is non-flaky** — Section
    11's 4-thread × 25-call concurrent dispatch
    produces identical PASS counts across 5 consecutive
    runs on the (T=0, P=0, S=1, D=1, H=1) cell.

17. The **5-axis product is PERPENDICULAR to the SIX
    PRIOR AXES** — char\*-vs-span (Sep 10 / Sep 12),
    CONSTEVAL-vs-RUNTIME (Sep 13), bounded-buffer-vs-
    NUL-terminating (Sep 16 / Sep 19), expected-vs-
    throwing (Sep 20 / Sep 21), and untouched-vs-NUL-
    padded (Sep 25 / Sep 26 / Sep 27), untouched-vs-
    SPACE-padded (Sep 28 / Sep 29 / Sep 30 / Oct 1),
    untouched-vs-DASH-padded (Oct 3 / Oct 4 / Oct 5 /
    Oct 8).  TODAY's lesson covers a NEW FIFTH
    PAD-AXIS (PETRA_HASH_PAD) on the CHAR\*+CAP ×
    CONSTEVAL-or-RUNTIME × bounded-buffer ×
    expected-or-throwing axis.  Section 18's `decltype`
    checks pin the perpendicular-axis claim on all 16
    cells of the 4-bit tuple with PETRA_PAD=0.

18. The **CONSTEVAL macro's consteval gate is preserved
    on all 16 cells** — pinned by Section 16's
    `static_assert(std::is_same_v<F1, ExpectedFmt1>)`
    on the expected path (cells (0, ?, ?, ?, ?)) and
    `static_assert(std::is_same_v<F1, std::size_t>)`
    on the throwing path (cells (1, ?, ?, ?, ?)).

## Headline finding — the matrix is now COMPLETE for ALL 16 cells of the (THROW, SPACE, DASH, HASH) 4-bit tuple

Together with Sep 8 / Sep 9 / Sep 10 / Sep 12 / Sep 16 /
Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep
26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct 3 /
Oct 4 / Oct 5 / Oct 8's lessons, TODAY's lesson closes
Oct 8's "Where we go next" item (1) verbatim — the
EIGHTH axis (PETRA_HASH_PAD) sibling-choice macro layer
on the char\*+cap axis.  The matrix on the BOUNDED-BUFFER
× (expected-or-throwing) × (untouched-vs-SPACE-vs-DASH-
vs-HASH) quadrant is now **COMPLETE for ALL 16 cells of
the 4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD,
PETRA_DASH_PAD, PETRA_HASH_PAD)** on the char\*+cap side
(PETRA_PAD=0 only).

| axis | value |
|-----:|----------------------------------------|
|    1 | char\*+cap vs span\<char\>            |
|    2 | CONSTEVAL vs RUNTIME                   |
|    3 | bounded-buffer vs NUL-terminating      |
|    4 | expected vs throwing                   |
|    5 | untouched vs NUL-padded                |
|    6 | untouched vs SPACE-padded              |
|    7 | untouched vs DASH-padded               |
|    8 | untouched vs HASH-padded (NEW today)   |

The matrix isn't quite 2^8 = 256 because axes 5, 6, 7,
and 8 aren't fully independent (the (PAD=1, SPACE_PAD=1)
cells reuse the (PAD=0, SPACE_PAD=1) wrappers per Sep 30's
composition rule; the (PAD=1, DASH_PAD=1) cells reuse the
(PAD=0, DASH_PAD=1) wrappers per Oct 8's transitive
composition rule; the (SPACE_PAD=1, DASH_PAD=1) cells
reuse the (SPACE_PAD=0, DASH_PAD=1) wrappers per Oct 3 /
Oct 4's composition rule; the (DASH_PAD=1, HASH_PAD=1)
cells reuse the (DASH_PAD=0, HASH_PAD=1) wrappers per
today's composition rule; and the (PAD=1, HASH_PAD=1)
cells reuse the (PAD=0, HASH_PAD=1) wrappers).  The
effective matrix is 256 cells *if you count the
composition rules as NEW axes*, but the *implementation*
on the PETRA_PAD=0 side has 16 sibling wrappers per
surface × 2 surfaces = 32 wrappers (RUNTIME+CONSTEVAL
expected HASH/DASH/SPACE/untouched pairs, RUNTIME+CONSTEVAL
throwing HASH/DASH/SPACE/untouched pairs, ... 4 HASH-pad
cells (RUNTIME+CONSTEVAL × expected/throwing = 4 wrappers
on the HASH axis).  Today adds the 4 HASH-pad wrappers;
the composition rules mean many cells dispatch to the
SAME wrappers.

TODAY's lesson covers 4 NEW dispatch cells on the
char\*+cap side: (0, 0, 0, 0, 1), (0, 0, 0, 1, 1), (0, 0,
1, 0, 1), and (0, 0, 1, 1, 1) (the cells that have
HASH_PAD=1 on the expected side) PLUS 4 throwing-side
cells (1, 0, 0, 0, 1), (1, 0, 0, 1, 1), (1, 0, 1, 0, 1),
and (1, 0, 1, 1, 1).  All 8 NEW cells dispatch to the
SAME TWO new HASH-pad sibling wrappers (one
expected-pair, one throwing-pair) via the transitive
composition rule (HASH > DASH > SPACE > NUL).  The
matrix on the char\*+cap side is now COMPLETE for ALL
16 cells of the 4-bit tuple (THROW, SPACE_PAD, DASH_PAD,
HASH_PAD).

The char\*+cap side and std::span\<char\> side will mirror
each other byte-for-byte when Oct 4's `try_format_dash_pad_span_n`
is extended to a HASH-pad variant (the mirror of today's
lesson on the std::span\<char\> side).  That work is out
of scope for TODAY's lesson (which focuses on the
char\*+cap side).  Future lessons will close that gap.

## Where we go next

TODAY's lesson closes Oct 8's "Where we go next" item (1)
verbatim: the EIGHTH axis (PETRA_HASH_PAD = 1)
sibling-choice macro layer on the CHAR\*+CAP ×
BOUNDED-BUFFER axis.  The matrix is now COMPLETE for ALL
16 cells of the (THROW, SPACE_PAD, DASH_PAD, HASH_PAD)
4-bit tuple on the CHAR\*+CAP × bounded-buffer ×
expected-or-throwing axis (PETRA_PAD=0 only).  The PETRA_PAD=1
cells require Sep 25's / Sep 26's NUL-pad wrappers, which
are out of scope for this lesson.

TODAY's lesson does NOT cover the OTHER open items from
the matrix:

1. **STD::SPAN\<CHAR\> MIRROR of TODAY's lesson** —
   `try_format_hash_pad_span_n` (the std::span\<char\>
   mirror of TODAY's char\*+cap lesson) would close the
   (PETRA_HASH_PAD=1) sibling-choice macro layer on the
   std::span\<char\> side.  The composition rule is
   identical: HASH overwrites DASH overwrites SPACE
   overwrites NUL when all four pad-flags are set.  No
   NEW wrappers needed — just REUSE TODAY's HASH-pad
   char\*+cap sibling wrappers verbatim and add a new
   5-bit meta-macro dispatcher for the std::span\<char\>
   side.

2. **Cross-cutting infrastructure items from the Aug 13 /
   Aug 15 / Aug 17 lessons** — could be revisited once
   the EIGHTH axis is fully closed on BOTH sides (TODAY
   closed the char\*+cap side; future work closes the
   std::span\<char\> side).

With the 16-cell matrix now COMPLETE for the (THROW,
SPACE_PAD, DASH_PAD, HASH_PAD) 4-bit tuple on the
char\*+cap side, future lessons can either (a) introduce
the STD::SPAN\<CHAR\> MIRROR of TODAY's lesson (to close
the EIGHTH axis on the std::span\<char\> side), or (b)
revisit the cross-cutting infrastructure items.

## C++23 features exercised

- `std::format` + `std::vformat_to` +
  `std::format_string<Args...>` (P2216R3) +
  `std::make_format_args`.
- `std::expected<T, E>` (P0323R12).
- `std::println` (P2093R14).
- `std::atomic<int>` + `std::atomic::fetch_add`
  (concurrency).
- `std::is_same_v` (decltype checks).
- `std::format_error` + `std::runtime_error` + `dynamic_cast`
  + `std::is_base_of_v` (catch-chain contract pinning).
- `std::array<std::thread, 4>` + `std::thread::join()`
  (concurrency).
- `__VA_OPT__(,)` preprocessor feature (C++20).

Pure consumer-side; no third-party libraries, no library
installation, no `find_package`.

## Build and verification commands

Default build (PETRA_THROW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0, PETRA_HASH_PAD=0):

```bash
cd late-may/cpp_practice/try_format_hash_pad_n
cmake -S . -B build
cmake --build build
./build/P-2026-10-09-try-format-hash-pad-n
```

Strict-warning build:

```bash
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-09-try-format-hash-pad-n
```

ASan + UBSan build:

```bash
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-09-try-format-hash-pad-n
```

HASH-pad cell (PETRA_HASH_PAD=1):

```bash
cmake -S . -B build-hash-pad -DPETRA_HASH_PAD=ON
cmake --build build-hash-pad
./build-hash-pad/P-2026-10-09-try-format-hash-pad-n
```

DASH+HASH composition cell (PETRA_DASH_PAD=1,
PETRA_HASH_PAD=1):

```bash
cmake -S . -B build-dash-hash \
      -DPETRA_DASH_PAD=ON -DPETRA_HASH_PAD=ON
cmake --build build-dash-hash
./build-dash-hash/P-2026-10-09-try-format-hash-pad-n
```

Throwing HASH-pad cell (PETRA_THROW_ON_OVERFLOW=1,
PETRA_HASH_PAD=1):

```bash
cmake -S . -B build-throw-hash \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_HASH_PAD=ON
cmake --build build-throw-hash
./build-throw-hash/P-2026-10-09-try-format-hash-pad-n
```

Direct compile (no CMake, mirrors the lesson's "Build and
verification commands" section):

```bash
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-10-09-try-format-hash-pad-n.cpp \
        -o /tmp/P-2026-10-09-try-format-hash-pad-n
```

## Observed test results

121/121 PASS on the (0, 0, 0, 0, 0) default build;
121/121 PASS on the (0, 0, 1, 0, 0) expected+SPACE-padded
build;
121/121 PASS on the (0, 0, 0, 1, 0) expected+DASH-padded
build;
121/121 PASS on the (0, 0, 1, 1, 0) expected+SPACE+DASH-
padded build;
177/177 PASS on the (0, 0, 0, 0, 1) expected+HASH-padded
build (NEW today);
250/250 PASS on the (0, 0, 0, 1, 1) expected+DASH+HASH-
padded build (NEW today; HASH overwrites DASH);
250/250 PASS on the (0, 0, 1, 0, 1) expected+SPACE+HASH-
padded build (NEW today; HASH overwrites SPACE
transitively);
250/250 PASS on the (0, 0, 1, 1, 1) expected+SPACE+DASH+
HASH-padded build (NEW today; transitive composition
HASH > DASH > SPACE);
119/119 PASS on the (1, 0, 0, 0, 0) throwing+untouched build;
119/119 PASS on the (1, 0, 1, 0, 0) throwing+SPACE-padded
build;
119/119 PASS on the (1, 0, 0, 1, 0) throwing+DASH-padded
build;
119/119 PASS on the (1, 0, 1, 1, 0) throwing+SPACE+DASH-
padded build;
175/175 PASS on the (1, 0, 0, 0, 1) throwing+HASH-padded
build (NEW today);
248/248 PASS on the (1, 0, 0, 1, 1) throwing+DASH+HASH-
padded build (NEW today; HASH overwrites DASH);
248/248 PASS on the (1, 0, 1, 0, 1) throwing+SPACE+HASH-
padded build (NEW today; HASH overwrites SPACE
transitively);
248/248 PASS on the (1, 0, 1, 1, 1) throwing+SPACE+DASH+
HASH-padded build (NEW today; transitive composition
HASH > DASH > SPACE).

Five consecutive runs on each of the 4 NEW HASH-pad-only
cells and the 4 NEW HASH-composition cells produced
identical PASS/FAIL tallies (no flakiness in Section 11's
4-thread × 25-call concurrent dispatch):
- (0, 0, 0, 0, 1) expected+HASH-padded: 177/177 (× 5 runs)
- (0, 0, 0, 1, 1) expected+DASH+HASH-padded: 250/250 (× 5 runs)
- (0, 0, 1, 0, 1) expected+SPACE+HASH-padded: 250/250 (× 5 runs)
- (0, 0, 1, 1, 1) expected+SPACE+DASH+HASH-padded: 250/250 (× 5 runs)
- (1, 0, 0, 0, 1) throwing+HASH-padded: 175/175 (× 5 runs)
- (1, 0, 0, 1, 1) throwing+DASH+HASH-padded: 248/248 (× 5 runs)
- (1, 0, 1, 0, 1) throwing+SPACE+HASH-padded: 248/248 (× 5 runs)
- (1, 0, 1, 1, 1) throwing+SPACE+DASH+HASH-padded: 248/248 (× 5 runs)

The PETRA_PAD=1 friendly error fires at compile time on
all 16 cells with the expected pointer:
`error: "This lesson (try_format_hash_pad_n) only supports
PETRA_PAD=0.  For PETRA_PAD=1, use Sep 25's
try_format_padded_n_or_into (expected+NUL-padded) or
Sep 26's try_format_padded_n_or_throw
(throwing+NUL-padded)."`

Zero warnings under -Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion; zero sanitizer
diagnostics; empty stderr under default + ASan/UBSan +
strict-warning builds (verified with `cmake -S . -B
build` + `cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON`
+ `cmake -S . -B build-asan -DENABLE_ASAN=ON` + the 8 NEW
HASH-pad cells under strict + ASan).

Pure consumer-side C++23 std::format + std::vformat_to +
std::format_string\<Args...\>(P2216R3) +
std::make_format_args + std::expected (P0323R12) +
std::println (P2093R14) + std::atomic + std::is_same_v +
std::format_error + std::runtime_error + dynamic_cast +
std::is_base_of_v + std::array\<std::thread, 4\> +
std::thread::join() + __VA_OPT__ preprocessor feature
exercise — reuses Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10
/ Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 /
Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
Sep 26 / Sep 27 / Sep 28 / Sep 29 / Sep 30 / Oct 1 / Oct
3 / Oct 4 / Oct 5 / Oct 8's machinery verbatim, no
third-party libraries, no library installation, no
`find_package`.
