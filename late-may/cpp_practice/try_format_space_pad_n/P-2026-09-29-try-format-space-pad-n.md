# P-2026-09-29 — `try_format_space_pad_n`

Topic: a **SIXTH-AXIS (PETRA_SPACE_PAD) sibling-choice META-MACRO
layer** on the **CHAR\*+CAP × BOUNDED-BUFFER** axis — the mirror of
Sep 28's std::span\<char\> lesson (try_format_space_pad_span_n).  Two
new meta-macros — `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW` (CONSTEVAL
surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW` (RUNTIME
surface) — each dispatch to **ONE OF FOUR** sibling wrappers at
preprocessor time based on the caller-controlled compile-time flag
**triple** `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)`.

**Closes Sep 28's "Where we go next" item (b) verbatim**:

> extend the SPACE-pad axis to the char\*+cap side (mirror of Sep 26 /
> Sep 27's NUL-pad layer on the char\*+cap side).

Today covers that verbatim — FOUR new
`petra::try_format_*_space_pad(_handle|_throwing)(char*, std::size_t,
fmt, args...)` wrappers (a CONSTEVAL+RUNTIME pair of expected+SPACE-
padded siblings + a CONSTEVAL+RUNTIME pair of throwing+SPACE-padded
siblings) that compose on top of Sep 8's / Sep 9's / Sep 20's
char\*+cap untouched wrappers and space-fill `[written, cap)` with
`' '` on success only.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-padded) quadrant on the char\*+cap side grows
from **32 cells (Sep 26 / Sep 28's SEP-26 covering (THROW × PAD)
already-paired-with-SPAN)** to **64 cells (TODAY)** — a 2× expansion
from adding the SIXTH axis (PETRA_SPACE_PAD) on the char\*+cap side.
Concretely, today's lesson adds 4 NEW sibling wrappers (2 surface
pairs × 2 axis1 values when PETRA_PAD=0 AND PETRA_SPACE_PAD=1).

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
           meta-macro layer on the STD::SPAN\<CHAR\> axis.
- Sep 25   `try_format_padded_n_or_into` — the FIFTH axis: the
           PAD-VS-UNTOUCHED sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis (covers 2 NEW cells
           on the char\*+cap × CONSTEVAL-or-RUNTIME × bounded-
           buffer × expected × padded axis; the matrix grows
           from 16 to 32 cells on the BOUNDED-BUFFER × expected
           quadrant on the char\*+cap side).
- Sep 26   `try_format_padded_n_or_throw` — the FIFTH axis
           (PAD-VS-UNTOUCHED) × FOURTH axis (EXPECTED-VS-
           THROWING) COMBINED META-MACRO layer on the CHAR\*+CAP
           × BOUNDED-BUFFER axis (covers 4 NEW cells on the
           char\*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer ×
           throwing × untouched-or-padded axis; the matrix grows
           from 32 to 64 cells on the BOUNDED-BUFFER quadrant on
           the char\*+cap side; closes Sep 25's "Where we go next"
           item #1 verbatim).
- Sep 27   `try_format_padded_span_n_or_throw` — the STD::SPAN\<CHAR\>
           MIRROR of Sep 26 (the FIFTH axis on the span\<char\>
           side).
- Sep 28   `try_format_space_pad_span_n` — the SIXTH axis
           (PETRA_SPACE_PAD) sibling-choice macro layer on the
           STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis (closes Sep
           27's "Where we go next" item #2 verbatim; introduces
           PETRA_SPACE_PAD on the span\<char\> side; 4 NEW
           siblings).
- TODAY    `try_format_space_pad_n` — the SIXTH axis
           (PETRA_SPACE_PAD) sibling-choice macro layer on the
           CHAR\*+CAP × BOUNDED-BUFFER axis — the CHAR\*+CAP
           MIRROR of Sep 28's span\<char\> lesson (closes Sep
           28's "Where we go next" item (b) verbatim; introduces
           PETRA_SPACE_PAD on the char\*+cap side; 4 NEW
           siblings; the matrix on the BOUNDED-BUFFER ×
           (expected-or-throwing) × (untouched-vs-SPACE-padded)
           quadrant is now COMPLETE on BOTH the char\*+cap AND
           span\<char\> axes).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
  Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 verbatim.
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
  sibling on the char\*+cap axis).
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 9 verbatim
  (the CONSTEVAL expected+untouched sibling on the char\*+cap
  axis).
- `petra::try_format_runtime_n_padded(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 25 verbatim (the RUNTIME
  expected+NUL-padded sibling on the char\*+cap axis).
- `petra::try_format_bounded_runtime_n_padded(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 25 verbatim
  (the CONSTEVAL expected+NUL-padded sibling on the char\*+cap
  axis).
- `petra::try_format_runtime_n_throwing(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 20 verbatim (the RUNTIME
  throwing+untouched sibling on the char\*+cap axis).
- `petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 20 verbatim
  (the CONSTEVAL throwing+untouched sibling on the char\*+cap
  axis).
- `petra::try_format_runtime_n_padded_throwing(char*, std::size_t,
  std::string_view, args...)` — REUSED Sep 26 verbatim (the RUNTIME
  throwing+NUL-padded sibling on the char\*+cap axis).
- `petra::try_format_bounded_runtime_n_padded_throwing(char*,
  std::size_t, std::format_string<Args...>, args...)` — REUSED
  Sep 26 verbatim (the CONSTEVAL throwing+NUL-padded sibling on the
  char\*+cap axis).
- `petra::try_format_runtime_n_space_pad(char*, std::size_t,
  std::string_view, args...)` — **NEW today** (RUNTIME surface of
  the BOUNDED-BUFFER × EXPECTED × SPACE-PAD sibling on the
  char\*+cap axis; wraps Sep 8's RUNTIME expected+untouched sibling
  verbatim and space-fills `[written, cap)` with `' '` on success
  AFTER the underlying call completes).
- `petra::try_format_bounded_runtime_n_space_pad(char*, std::size_t,
  std::format_string<Args...>, args...)` — **NEW today** (CONSTEVAL
  surface of the BOUNDED-BUFFER × EXPECTED × SPACE-PAD sibling on
  the char\*+cap axis; wraps Sep 9's CONSTEVAL expected+untouched
  sibling verbatim and space-fills `[written, cap)` with `' '` on
  success AFTER the underlying call completes).
- `petra::try_format_runtime_n_throwing_space_pad(char*,
  std::size_t, std::string_view, args...)` — **NEW today** (RUNTIME
  surface of the BOUNDED-BUFFER × THROWING × SPACE-PAD sibling on
  the char\*+cap axis; wraps Sep 20's RUNTIME throwing+untouched
  sibling verbatim and space-fills `[written, cap)` with `' '` on
  success ONLY via try/catch around the throwing call).
- `petra::try_format_bounded_runtime_n_throwing_space_pad(char*,
  std::size_t, std::format_string<Args...>, args...)` — **NEW
  today** (CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
  SPACE-PAD sibling on the char\*+cap axis; wraps Sep 20's
  CONSTEVAL throwing+untouched sibling verbatim and space-fills
  `[written, cap)` with `' '` on success ONLY via try/catch around
  the throwing call).
- The TWO new meta-macros `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
  (CONSTEVAL surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_PAD_OR_THROW`
  (RUNTIME surface).  Each is a verbatim thin dispatcher that picks
  between FOUR sibling wrappers at preprocessor time based on the
  `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)` 3-bit
  tuple.  `PETRA_PAD` MUST be 0 in this lesson; the dispatcher
  `#error`s when `PETRA_PAD = 1` (pointing to Sep 26).  Each macro
  is a parenthesized expression (NOT `do { } while (0)`) so the
  caller can write `auto n = TRY_FORMAT_BOUNDED_N_PAD_OR_THROW(...)`
  and capture the result.  Each macro uses `__VA_OPT__(,)` for the
  no-args case (mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
  Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 /
  Sep 28's macro shape).

## What today's lesson pins

1. **The (THROWING-vs-EXPECTED) × (NUL-PADDED-vs-SPACE-PADDED)
   sibling choice CAN be lifted to a meta-macro form as a 2-axis
   product when PETRA_PAD=0.**  Today's meta-macros dispatch on
   the `(PETRA_THROW_ON_OVERFLOW, PETRA_SPACE_PAD)` 2-bit pair
   while `PETRA_PAD=0` (the NUL-padded branch is out of scope
   for this lesson).  The preprocessor-time `#if` chain is:
   ```
   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_SPACE_PAD
   ```
   - `(0, 0)` → expected+untouched sibling (returns
     `std::expected<std::size_t, FormatError>`; bytes `[written,
     cap)` UNTOUCHED on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n` (Sep 9)
       - RUNTIME:   `petra::try_format_runtime_n` (Sep 8)
   - `(0, 1)` → expected+SPACE-padded sibling (returns
     `std::expected<std::size_t, FormatError>`; bytes `[written,
     cap)` filled with `' '` on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_space_pad`
                    (NEW today; wraps Sep 9 verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_space_pad`
                    (NEW today; wraps Sep 8 verbatim)
   - `(1, 0)` → throwing+untouched sibling (returns `std::size_t`
     on success; throws `std::runtime_error` on overflow; bytes
     `[written, cap)` UNTOUCHED on success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing`
                    (Sep 20)
       - RUNTIME:   `petra::try_format_runtime_n_throwing` (Sep 20)
   - `(1, 1)` → throwing+SPACE-padded sibling (returns
     `std::size_t` on success; throws `std::runtime_error` on
     overflow; bytes `[written, cap)` filled with `' '` on
     success):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_space_pad`
                    (NEW today; wraps Sep 20 verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_space_pad`
                    (NEW today; wraps Sep 20 verbatim)
2. **The `PETRA_PAD` scope guard** — when `PETRA_PAD=1`, the
   dispatcher `#error`s with a clear pointer to Sep 26 (because
   the NUL-padded siblings of Sep 26 are NOT included in this
   file).  This prevents the lesson from silently falling
   through to a wrong branch.
3. **The compile-time flag triple `(PETRA_THROW_ON_OVERFLOW,
   PETRA_PAD, PETRA_SPACE_PAD)` controls the dispatch** via
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
   result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
   Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 /
   Sep 28's macro shape.
6. **`__VA_OPT__(,)`, for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 /
   Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28.
7. **The expected+SPACE-padded sibling preserves the SAME
   buffer atomicity on TooLarge** as the expected+untouched
   sibling — the buffer is UNTOUCHED on overflow (Sep 8 / Sep 9
   already pin this on the underlying wrappers; today's wrapper
   re-pins it on the SPACE-padded path because the new `for
   (i = written; i < cap; ++i) out[i] = ' ';` loop COULD
   theoretically diverge; Section 3 pins this specifically with
   a 16-CHECK block).
8. **The expected+SPACE-padded sibling preserves the SAME
   atomicity on Format errors** — a malformed `fmt` returns
   `FormatError{Format, e.what()}` and the buffer is UNTOUCHED
   (Section 8 pins this on the RUNTIME surface by calling with
   `std::string fmt = "bad {"` and asserting every byte is still
   `'Q'`).
9. **The expected+SPACE-padded sibling preserves the SAME
   byte-count contract on success** — returns `std::size_t ==
   bytes written` (the same value Sep 8's / Sep 9's
   expected+untouched siblings return); Section 4 pins this on
   both paths.
10. **The throwing+SPACE-padded sibling preserves the SAME
    buffer atomicity on TooLarge** as the throwing+untouched
    sibling — the buffer is UNTOUCHED on overflow.  TODAY's NEW
    throwing+SPACE-padded wrapper does the space-fill ONLY when
    the underlying call returns (the `std::size_t` return value);
    when the underlying call throws, control unwinds through the
    wrapper WITHOUT the space-fill loop executing.  Section 3
    pins this on the (1, 1) path.
11. **The throwing+SPACE-padded sibling preserves the SAME
    atomicity on Format errors** — a malformed `fmt` throws
    `std::runtime_error` (delegated to Sep 20's wrapper, which
    delegates to Sep 8's / Sep 9's underlying impl, which throws
    `std::format_error` at format time before any writes);
    Section 8 pins this on the RUNTIME surface.
12. **The throwing+SPACE-padded sibling preserves the SAME
    byte-count contract on success** — returns `std::size_t ==
    bytes written` (the same value Sep 20's / Sep 8's / Sep 9's
    throwing+untouched siblings return); Section 4 pins this on
    the (1, 1) path.
13. **The throwing+SPACE-padded sibling's byte-state contract
    — bytes `[written, cap)` are `' '` on success — is pinned by
    Section 2** (which pre-fills the buffer with poison `'Q'`
    bytes, runs the macro, and asserts every byte at index >=
    written is `' '` on the SPACE-padded path OR `'Q'` on the
    untouched path).
14. **The throwing+SPACE-padded sibling's byte-state
    distinctness from NUL-padded sibling — bytes `[written, cap)`
    are `' '` (NOT `'\0'` and NOT `'Q'`) — is pinned by Section
    16** with an explicit 8-CHECK block on the (0, 0, 1) and
    (1, 0, 1) paths.
15. **The throwing+SPACE-padded sibling handles the `cap=0`
    edge case correctly** — the `for (i = written; i < cap; ++i)`
    loop has `i < cap == 0` as the initial condition, so the
    loop body never executes.  Section 2c pins this on the (0,
    0, 1) and (1, 0, 1) paths with format `""` and `cap = 0`.
17. **The throwing+SPACE-padded sibling handles the
    `written==cap` edge case correctly** — the `for (i =
    written; i < cap; ++i)` loop has `i == cap` as the initial
    condition, so the loop body never executes.  Section 2d
    pins this with format `"ABCDEFGH"` (8 bytes) into `cap = 8`
    (full-buffer success).
18. **The CONSTEVAL macro's consteval gate is preserved
    regardless of `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    PETRA_SPACE_PAD)`** — `TRY_FORMAT_BOUNDED_N_PAD_OR_THROW`
    still requires `std::format_string<Args...>` (a literal
    `fmt` at the call site).  Pinned by `/tmp/
    probe_consteval_rejects_runtime_fmt_space_pad.cpp`
    producing `error: call to consteval function
    std::basic_format_string<char, int>::basic_format_string<
    std::string> is not a constant expression` with note
    `read of non-constexpr variable fmt_runtime is not allowed
    in a constant expression` / `in call to this->data()`.  Also
    pinned for the throwing path by `/tmp/
    probe_consteval_rejects_runtime_fmt_space_pad_throwing.cpp`
    producing the same error.
19. **The throwing+SPACE-padded sibling's `std::runtime_error`
    `.what()` byte-exactness is pinned by Section 15** — TODAY's
    wrapper does NOT construct a new `std::runtime_error`
    message; the throw TYPE and `.what()` message are both
    delegated to Sep 20's throwing wrapper (which constructs
    `std::runtime_error` with the SAME `.what()` message that
    `FormatError{TooLarge, ...}` would carry); Section 15's
    `dynamic_cast<const std::format_error*>(&e)` check returns
    `nullptr`, confirming that the throwing wrapper re-raises as
    `std::runtime_error` (NOT `std::format_error`) — this
    matches Sep 20 / Sep 22 / Sep 23's catch-chain contract.
20. **The PETRA_SPACE_PAD × PETRA_THROW_ON_OVERFLOW axis pair is
    PERPENDICULAR to the FIVE PRIOR AXES** — char\*-vs-span
    (Sep 10 / Sep 12), CONSTEVAL-vs-RUNTIME (Sep 13),
    bounded-buffer-vs-NUL-terminating (Sep 16), and
    expected-vs-throwing (Sep 20), and untouched-vs-NUL-padded
    (Sep 25 / Sep 26 / Sep 27).  TODAY's meta-macros are at the
    INTERSECTION of FOUR INDEPENDENT axes (axis 2, axis 4, axis
    5, axis 6) on the char\*+cap × CONSTEVAL-or-RUNTIME ×
    bounded-buffer × throwing × untouched-vs-SPACE-padded cell.
    Section 14's `decltype` checks pin the perpendicular-axis
    claim on ALL FOUR paths (PETRA_THROW × PETRA_SPACE_PAD).
21. **The "try/catch around a throwing call + pad-on-success"
    pattern is the C++ idiom for "space-fill on success only
    when the underlying throwing call returned normally"** —
    Sep 25's expected-sibling pattern (`if (r.has_value())`)
    does NOT work here because the throwing sibling returns
    `std::size_t` (NOT `std::expected`), and on TooLarge the
    throwing sibling throws `std::runtime_error` (does NOT
    return normally at all).  TODAY's wrapper uses the
    `try { auto n = THROWING_SIBLING(...); for (...) space-fill
    loop; return n; } catch (...) { throw; }` shape — the catch
    block re-raises the `std::runtime_error` UNCHANGED (with
    the SAME `.what()` message that Sep 20's throwing wrapper
    constructed; the throw TYPE is also preserved as
    `std::runtime_error` because `catch (...)` re-raises the
    in-flight exception unchanged).

## Headline finding — the matrix is now 64 cells on BOTH axes

Together with Sep 8's / Sep 9's / Sep 16's / Sep 19's / Sep 20's /
Sep 21's / Sep 22's / Sep 23's / Sep 25's / Sep 26's / Sep 27's /
Sep 28's lessons, TODAY's lesson extends the matrix to **64 cells
(2^5) on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-padded) quadrant on the char\*+cap side**:

| axis | value                                |
|-----:|--------------------------------------|
|    1 | char\*+cap vs span\<char\>            |
|    2 | CONSTEVAL vs RUNTIME                 |
|    3 | bounded-buffer vs NUL-terminating    |
|    4 | expected vs throwing (Sep 20 / 21)   |
|    5 | untouched vs NUL-padded (Sep 25 / 27)|
|    6 | untouched vs SPACE-padded (TODAY)    |

TODAY's lesson covers 4 NEW cells on the char\*+cap ×
CONSTEVAL-or-RUNTIME × bounded-buffer × throwing ×
untouched-vs-SPACE-padded axis (the (0, 0, 1) and (1, 0, 1)
cells on the char\*+cap side).  The matrix is now 64 cells on
the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-SPACE-padded) quadrant on BOTH the char\*+cap
AND span\<char\> sides (4 dispatch cells × 4 sibling wrappers ×
2 surface pairs = 32 cells per side × 2 sides = 64 cells
total).

The char\*+cap side mirrors the span\<char\> side byte-for-byte
(Sep 28 / TODAY) — the only difference between the two sides is
the parameter shape (`char*, std::size_t, fmt, args...` vs
`std::span<char>, fmt, args...`).  All 8 sibling wrappers per
side share the same byte-state contract (bytes `[written, cap)`
are `' '` on success in the SPACE-padded cells, `'\0'` in the
NUL-padded cells, UNTOUCHED in the untouched cells).

## Where we go next

TODAY's lesson closes Sep 28's "Where we go next" item (b)
verbatim: the SIXTH axis on the char\*+cap × BOUNDED-BUFFER
axis.  The matrix is now 2^6 = 64 cells on the BOUNDED-BUFFER ×
(expected-or-throwing) × (untouched-vs-SPACE-padded) quadrant on
BOTH the char\*+cap and span\<char\> sides.

TODAY's lesson does NOT cover the OTHER open items from the
matrix:

1. **PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells** (i.e.
   SPACE-padded siblings of Sep 26's / Sep 27's NUL-padded
   siblings) — would introduce 4 NEW SPACE+NUL-padded wrappers
   per side (8 total).  Each future lesson adds verbatim thin
   SPACE-padded wrappers composed on top of Sep 26's / Sep 27's
   NUL-padded siblings + new `#if`-gated meta-macros.  (4 new
   cells per axis extension.)

2. **ALTERNATE-PAD cells** (e.g. `PETRA_DASH_PAD` that pads
   with `'-'`) — would introduce a SEVENTH axis.  Each future
   lesson adds verbatim thin dash-pad wrappers composed on top
   of TODAY's SPACE-padded wrappers + new `#if PETRA_DASH_PAD`-
   gated meta-macros.  (4 new cells per axis extension.)

With the 4-cell char\*+cap matrix now COMPLETE for the
SPACE-padded axis (and the 4-cell span\<char\> matrix from
Sep 28 COMPLETE), future lessons can either (a) introduce a
SEVENTH axis on EITHER side (e.g. `PETRA_DASH_PAD`), (b) extend
the SPACE-pad axis to the NUL-padded sibling (mirror of Sep 28 /
TODAY on the PETRA_PAD=1 path), or (c) close cross-cutting
infrastructure items from the Aug 13 / Aug 15 / Aug 17 lessons.

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
installation, no `find_package`.  Reuses Sep 6 / Sep 7 / Sep 8 /
Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 /
Sep 16 / Sep 19 / Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 /
Sep 26 / Sep 27 / Sep 28's machinery verbatim.

## Build and verification commands

Default build (PETRA_THROW=0, PETRA_PAD=0, PETRA_SPACE_PAD=0):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-29-try-format-space-pad-n.cpp \
        -o /tmp/P-2026-09-29-try-format-space-pad-n
```

Strict-warning build:

```sh
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-09-29-try-format-space-pad-n
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-29-try-format-space-pad-n
```

Expected+SPACE-padded path (PETRA_THROW=0, PETRA_SPACE_PAD=1):

```sh
cmake -S . -B build-space-pad -DPETRA_SPACE_PAD=ON
cmake --build build-space-pad
./build-space-pad/P-2026-09-29-try-format-space-pad-n
```

Throwing+untouched path (PETRA_THROW=1, PETRA_SPACE_PAD=0):

```sh
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=ON
cmake --build build-throw
./build-throw/P-2026-09-29-try-format-space-pad-n
```

Throwing+SPACE-padded path (PETRA_THROW=1, PETRA_SPACE_PAD=1):

```sh
cmake -S . -B build-throw-space-pad \
      -DPETRA_THROW_ON_OVERFLOW=ON -DPETRA_SPACE_PAD=ON
cmake --build build-throw-space-pad
./build-throw-space-pad/P-2026-09-29-try-format-space-pad-n
```

## Consteval-gate probe

```sh
# Should FAIL with: call to consteval function ... is not a
# constant expression / read of non-constexpr variable fmt_runtime
# is not allowed in a constant expression / in call to this->data().
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_THROW_ON_OVERFLOW=0 -DPETRA_PAD=0 -DPETRA_SPACE_PAD=1 \
        /tmp/probe_consteval_rejects_runtime_fmt_space_pad.cpp \
        -o /tmp/probe_consteval_rejects_runtime_fmt_space_pad
```

## Observed output

(0, 0, 0) expected+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=0 (expected+untouched sibling)
=========================================================
PASS: 131  FAIL: 0
=========================================================
OVERALL: PASS
```

(0, 0, 1) expected+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=1 (expected+SPACE-padded sibling)
=========================================================
PASS: 151  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 0) throwing+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=0 (throwing+untouched sibling)
=========================================================
PASS: 122  FAIL: 0
=========================================================
OVERALL: PASS
```

(1, 0, 1) throwing+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=1 (throwing+SPACE-padded sibling)
=========================================================
PASS: 141  FAIL: 0
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
`(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)` —
pinned by `/tmp/
probe_consteval_rejects_runtime_fmt_space_pad.cpp` and
`/tmp/probe_consteval_rejects_runtime_fmt_space_pad_throwing.cpp`
both producing `error: call to consteval function
std::basic_format_string<char, int>::basic_format_string<
std::string> is not a constant expression` with note `read of
non-constexpr variable fmt_runtime is not allowed in a constant
expression` / `in call to this->data()`.

The `PETRA_PAD = 1` scope guard #error fires correctly —
`/tmp/probe_pad_noscope` produces
`error: "PETRA_PAD = 1 (NUL-padded sibling) is OUT OF SCOPE for
this lesson.  Use Sep 26's try_format_padded_n_or_throw/ for the
PETRA_PAD=1 combinations."` at preprocessor time (the
follow-on errors are downstream of the preprocessor error since
the dispatcher never emitted macros).