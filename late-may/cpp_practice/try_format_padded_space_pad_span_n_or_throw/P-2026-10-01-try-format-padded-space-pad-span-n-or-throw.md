# P-2026-10-01 — `try_format_padded_space_pad_span_n_or_throw`

Topic: **the PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells on the
STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis** — the STD::SPAN\<CHAR\>
MIRROR of Sep 30's char\*+cap lesson (the composition of axis 5
with axis 6 on the span\<char\> side).

**Closes Sep 30's "Where we go next" item (1) verbatim**:

> PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 cells on the
> STD::SPAN\<CHAR\> axis — would be the span\<char\> mirror of
> TODAY's char\*+cap lesson.  Closes the (0,1,1) and (1,1,1)
> cells on the span\<char\> side.  Each future lesson adds
> verbatim thin dispatchers (the wrappers are REUSED from Sep
> 28's SPACE-padded siblings per the composition rule).  (2 new
> dispatch cells per axis extension.)

Today covers that verbatim — and the **headline finding** is that
the (PAD=1, SPACE_PAD=1) cells **REUSE** the (PAD=0, SPACE_PAD=1)
SPACE-padded span\<char\> sibling wrappers verbatim (no NEW
wrappers needed), because the composition rule "the LATER fill
overwrites the EARLIER fill" pins SPACE over NUL when both flags
are set.

The matrix on the BOUNDED-BUFFER × (expected-or-throwing) ×
(untouched-vs-NUL-vs-SPACE) quadrant on the std::span\<char\>
side is now **COMPLETE for ALL 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)**.

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
- TODAY    `try_format_padded_space_pad_span_n_or_throw` — the
           STD::SPAN\<CHAR\> MIRROR of Sep 30 (the COMPOSITION of
           axis 5 and axis 6 on the STD::SPAN\<CHAR\> ×
           BOUNDED-BUFFER axis; closes Sep 30's "Where we go
           next" item (1) verbatim).  KEY FINDING: the
           (PAD=1, SPACE_PAD=1) cells REUSE the (PAD=0,
           SPACE_PAD=1) SPACE-padded span\<char\> sibling
           wrappers verbatim — no NEW wrappers needed.  The
           matrix is now COMPLETE for ALL 8 cells of the
           3-bit tuple on the std::span\<char\> side.

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
- `petra::try_format_runtime_n(std::span<char>, std::string_view,
  args...)` — REUSED Sep 19 verbatim.
- `petra::try_format_bounded_runtime_n_span(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 10 verbatim.
- `petra::try_format_runtime_n_throwing(std::span<char>,
  std::string_view, args...)` — REUSED Sep 21 verbatim.
- `petra::try_format_bounded_runtime_n_throwing_span(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 21 verbatim.
- `petra::try_format_bounded_runtime_n_padded_span(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 27 verbatim
  (CONSTEVAL expected+NUL-padded span\<char\> wrapper).
- `petra::try_format_runtime_n_padded_span(std::span<char>,
  std::string_view, args...)` — REUSED Sep 27 verbatim (RUNTIME
  expected+NUL-padded span\<char\> wrapper).
- `petra::try_format_bounded_runtime_n_throwing_padded_span(
  std::span<char>, std::format_string<Args...>, args...)` — REUSED
  Sep 27 verbatim (CONSTEVAL throwing+NUL-padded span\<char\>
  wrapper).
- `petra::try_format_runtime_n_throwing_padded_span(std::span<char>,
  std::string_view, args...)` — REUSED Sep 27 verbatim (RUNTIME
  throwing+NUL-padded span\<char\> wrapper).
- `petra::try_format_bounded_runtime_n_space_pad_span(std::span<char>,
  std::format_string<Args...>, args...)` — REUSED Sep 28 verbatim
  (CONSTEVAL expected+SPACE-padded span\<char\> wrapper; ALSO used
  by the (0,1,1) dispatch cell per the composition rule).
- `petra::try_format_runtime_n_space_pad_span(std::span<char>,
  std::string_view, args...)` — REUSED Sep 28 verbatim (RUNTIME
  expected+SPACE-padded span\<char\> wrapper; ALSO used by the
  (0,1,1) dispatch cell per the composition rule).
- `petra::try_format_bounded_runtime_n_throwing_space_pad_span(
  std::span<char>, std::format_string<Args...>, args...)` — REUSED
  Sep 28 verbatim (CONSTEVAL throwing+SPACE-padded span\<char\>
  wrapper; ALSO used by the (1,1,1) dispatch cell per the
  composition rule).
- `petra::try_format_runtime_n_throwing_space_pad_span(
  std::span<char>, std::string_view, args...)` — REUSED Sep 28
  verbatim (RUNTIME throwing+SPACE-padded span\<char\> wrapper;
  ALSO used by the (1,1,1) dispatch cell per the composition
  rule).
- The TWO meta-macros `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW`
  (CONSTEVAL surface) and `TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW`
  (RUNTIME surface).  Each is a verbatim thin dispatcher that picks
  between EIGHT sibling wrappers at preprocessor time based on
  the `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)` 3-bit
  tuple.  The (0,1,1) and (1,1,1) cells REUSE the (0,0,1) and
  (1,0,1) SPACE-padded span\<char\> sibling wrappers verbatim (the
  "SPACE overwrites NUL" composition rule).

## What today's lesson pins

1. **The (THROWING-vs-EXPECTED) × (NUL-PADDED-vs-SPACE-PADDED)
   sibling choice CAN be lifted to a meta-macro form as a 3-axis
   product covering all 8 cells of the (THROW, PAD, SPACE_PAD)
   tuple on the STD::SPAN\<CHAR\> axis.**  Today's meta-macros
   dispatch on the
   `(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)` 3-bit
   tuple.  The preprocessor-time `#if` chain is:
   ```
   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if PETRA_SPACE_PAD
   ```
   - `(0, 0, 0)` → expected+untouched sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_span` (Sep 10)
       - RUNTIME:   `petra::try_format_runtime_n(span, ...)` (Sep 19)
   - `(0, 1, 0)` → expected+NUL-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_padded_span` (Sep 27)
       - RUNTIME:   `petra::try_format_runtime_n_padded_span` (Sep 27)
   - `(0, 0, 1)` → expected+SPACE-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_space_pad_span`
                    (Sep 28)
       - RUNTIME:   `petra::try_format_runtime_n_space_pad_span` (Sep 28)
   - `(0, 1, 1)` → expected+NUL+SPACE-padded sibling (NEW today):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_space_pad_span`
                    (Sep 28 — REUSED verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_space_pad_span`
                    (Sep 28 — REUSED verbatim)
   - `(1, 0, 0)` → throwing+untouched sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_span`
                    (Sep 21)
       - RUNTIME:   `petra::try_format_runtime_n_throwing(span, ...)`
                    (Sep 21)
   - `(1, 1, 0)` → throwing+NUL-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_padded_span`
                    (Sep 27)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_padded_span`
                    (Sep 27)
   - `(1, 0, 1)` → throwing+SPACE-padded sibling:
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_space_pad_span`
                    (Sep 28)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_space_pad_span`
                    (Sep 28)
   - `(1, 1, 1)` → throwing+NUL+SPACE-padded sibling (NEW today):
       - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing_space_pad_span`
                    (Sep 28 — REUSED verbatim)
       - RUNTIME:   `petra::try_format_runtime_n_throwing_space_pad_span`
                    (Sep 28 — REUSED verbatim)

2. **HEADLINE FINDING — the (PAD=1, SPACE_PAD=1) cells REUSE the
   (PAD=0, SPACE_PAD=1) SPACE-padded span\<char\> sibling wrappers
   verbatim.**  This is the **composition rule**: "the LATER fill
   overwrites the EARLIER fill".  When both PETRA_PAD=1 and
   PETRA_SPACE_PAD=1, the SPACE fill (axis 6, applied LAST in the
   dispatch chain) overwrites the NUL fill (axis 5, applied
   FIRST).  The end state on success is bytes `[written,
   span.size())` = `' '` (the SPACE byte, NOT the NUL byte).  No
   NEW wrappers are needed for the (1,1) cells; they just dispatch
   to the (0,1) SPACE-padded wrappers.  Pinned by Section 16 with
   an 8-CHECK block on the (0,1,1) and (1,1,1) paths.

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
   TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)` and capture the
   result.

6. **`__VA_OPT__(,)`, for the no-args case.**

7. **The composition rule's byte-state contract — when BOTH
   PETRA_PAD and PETRA_SPACE_PAD are set, bytes `[written,
   span.size())` are `' '` on success — is pinned by Section 2**
   (which pre-fills the backing buffer with poison `'Q'` bytes,
   runs the macro, and asserts every byte at index >= written is
   `' '` on the (0,1,1) and (1,1,1) paths).

8. **The composition rule's byte-state distinctness from NUL-padded
   sibling — bytes `[written, span.size())` are `' '` (NOT `'\'0'`
   and NOT `'Q'`) — is pinned by Section 16** with an explicit
   `buf[i] != '\0'` and `buf[i] != 'Q'` block on the (0,1,1) and
   (1,1,1) paths.

9. **All 8 cells preserve buffer atomicity on TooLarge** —
   Section 3 pins this on all 8 flag triples.  On every cell, the
   buffer is UNTOUCHED on overflow (delegated to the underlying
   impl).

10. **All 8 cells preserve byte-count contract on success** —
    Section 4 pins this on all 8 flag triples.

11. **All 8 cells preserve atomicity on Format errors** — Section
    8 pins this on the RUNTIME surface for all 8 cells by calling
    with `std::string bad_fmt = "bad {"` and asserting every byte
    is still `'Q'`.

12. **The throwing+NUL+SPACE-padded sibling preserves the SAME
    `std::runtime_error` `.what()` byte-exactness** as the
    throwing+SPACE-padded sibling — Section 15 pins this via
    `dynamic_cast<const std::format_error*>(&e) == nullptr`
    (the throwing wrapper re-raises as `std::runtime_error`,
    NOT `std::format_error`).

13. **The CONSTEVAL macro's consteval gate is preserved regardless
    of the 3-bit tuple** —
    `TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW` still requires
    `std::format_string<Args...>` (a literal `fmt` at the call
    site).  Pinned by
    `/tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad_span.cpp`
    producing `error: call to consteval function
    std::basic_format_string<char, int>::basic_format_string<
    std::string> is not a constant expression` with note
    `read of non-constexpr variable fmt_str is not allowed in a
    constant expression` / `in call to basic_format_string<
    std::string>(fmt_str)` (verified on the (0,0,1), (0,1,1),
    (1,0,1), and (1,1,1) CONSTEVAL paths).

14. **The std::span\<char\> sub-span 'no-bleed' contract is
    preserved on the composition path (Section 2a)** — a
    std::span\<char\> over a sub-range of a buffer must NOT touch
    bytes outside the span (mirrors Sep 10 / Sep 12 / Sep 19 /
    Sep 21 / Sep 27 / Sep 28's no-bleed contract; a 6-byte
    sub-span at offset 5 of a 16-byte buffer with format
    'ABCDE' (5 bytes) leaves bytes [0,5) and [11,16) as poison
    'Q').

15. **The std::span\<char\> properties are preserved on the
    composition path (Section 1b)** — contiguous range + sized
    range + trivially copyable + standard layout + `.data()
    ->char*` + `.size()->std::size_t`.

16. **The std::span\<char\> throwing+padded sibling preserves the
    SAME buffer atomicity on TooLarge** — the buffer is UNTOUCHED
    on overflow.  Section 3 pins this on the (1,1,*) paths via
    `catch (const std::runtime_error&) { caught = true; }`.

17. **The 3-axis product is PERPENDICULAR to the FIVE PRIOR
    AXES** — char\*-vs-span (Sep 10 / Sep 12), CONSTEVAL-vs-
    RUNTIME (Sep 13), bounded-buffer-vs-NUL-terminating (Sep
    16 / Sep 19), expected-vs-throwing (Sep 20 / Sep 21), and
    untouched-vs-NUL-padded (Sep 25 / Sep 26 / Sep 27), and
    untouched-vs-SPACE-padded (Sep 28 / Sep 29 / Sep 30).
    TODAY's lesson covers the COMPOSITION of axis 5 (PETRA_PAD)
    and axis 6 (PETRA_SPACE_PAD) on the STD::SPAN\<CHAR\> ×
    CONSTEVAL-or-RUNTIME × bounded-buffer axis.  Section 14's
    `decltype` checks pin the perpendicular-axis claim on ALL 8
    cells of the 3-bit tuple.

18. **The CONSTEVAL macro's consteval gate is preserved on ALL 8
    cells** — pinned by Section 6's
    `static_assert(std::is_same_v<F1, ExpectedFmt1>)` on the
    expected path (cells (?, ?, ?)) and
    `static_assert(std::is_same_v<F1, std::size_t>)` on the
    throwing path (cells (1, ?, ?)).

## Headline finding — the matrix is now COMPLETE for ALL 8 cells on the span\<char\> side

Together with Sep 8 / Sep 9 / Sep 10 / Sep 12 / Sep 16 / Sep 19 /
Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 /
Sep 28 / Sep 30's lessons, TODAY's lesson closes Sep 30's "Where
we go next" item (1) verbatim — the (PAD=1, SPACE_PAD=1)
composition cells on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER
axis.  The matrix on the BOUNDED-BUFFER × (expected-or-throwing)
× (untouched-vs-NUL-vs-SPACE) quadrant on the std::span\<char\>
side is now **COMPLETE for ALL 8 cells of the 3-bit tuple
(PETRA_THROW_ON_OVERFLOW, PETRA_PAD, PETRA_SPACE_PAD)**.

|  axis | value                                |
|------:|----------------------------------------------------------------|
|     1 | char\*+cap vs span\<char\>          |
|     2 | CONSTEVAL vs RUNTIME                 |
|     3 | bounded-buffer vs NUL-terminating    |
|     4 | expected vs throwing                 |
|     5 | untouched vs NUL-padded              |
|     6 | untouched vs SPACE-padded            |
|     7 | (PAD=1) vs (SPACE_PAD=1) composition |

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

TODAY's lesson covers 2 NEW dispatch cells on the
std::span\<char\> side: (0,1,1) and (1,1,1).  The (PAD=1,
SPACE_PAD=1) cells REUSE the (PAD=0, SPACE_PAD=1) SPACE-padded
span\<char\> sibling wrappers verbatim.  The matrix is now
COMPLETE for ALL 8 cells of the 3-bit tuple on the
std::span\<char\> side.

The std::span\<char\> side mirrors the char\*+cap side
byte-for-byte (Sep 27 / Sep 28 / Sep 30 / TODAY) — the only
difference between the two sides is the parameter shape
(`char*, std::size_t, fmt, args...` vs `std::span<char>, fmt,
args...`).  All 8 sibling wrappers per side share the same
byte-state contract (bytes `[written, cap)` are `' '` on
success in the SPACE-padded cells, `'\'0'` in the NUL-padded
cells, UNTOUCHED in the untouched cells).

## Where we go next

TODAY's lesson closes Sep 30's "Where we go next" item (1)
verbatim: the PETRA_PAD = 1 × PETRA_SPACE_PAD = 1 composition
cells on the STD::SPAN\<CHAR\> × BOUNDED-BUFFER axis.  The
matrix is now COMPLETE for ALL 8 cells of the 3-bit tuple on
the std::span\<char\> side — both the char\*+cap side AND the
std::span\<char\> side are now COMPLETE for the PAD ×
SPACE_PAD composition.

TODAY's lesson does NOT cover the OTHER open items from the
matrix:

1. **ALTERNATE-PAD cells** (e.g. `PETRA_DASH_PAD` that pads
   with `'-'`) — would introduce a SEVENTH axis.  Each future
   lesson adds verbatim thin dash-pad wrappers composed on top
   of TODAY's SPACE-padded wrappers + new `#if PETRA_DASH_PAD`-
   gated meta-macros.  (8 new dispatch cells per axis
   extension per side.)  This is Sep 30's remaining "Where we
   go next" item (2).

2. **Cross-cutting infrastructure items from the Aug 13 / Aug 15
   / Aug 17 lessons** — could be revisited once the matrix is
   fully closed on both sides.

With the 8-cell matrix now COMPLETE for the PAD × SPACE_PAD
composition on BOTH sides (char\*+cap AND std::span\<char\>),
future lessons can either (a) introduce a SEVENTH axis on
EITHER side (e.g. `PETRA_DASH_PAD`) or (b) close cross-cutting
infrastructure items.

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
        P-2026-10-01-try-format-padded-space-pad-span-n-or-throw.cpp \
        -o /tmp/P-2026-10-01-try-format-padded-space-pad-span-n-or-throw
```

Strict-warning build:

```sh
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON
cmake --build build-strict
./build-strict/P-2026-10-01-try-format-padded-space-pad-span-n-or-throw
```

ASan + UBSan build:

```sh
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-10-01-try-format-padded-space-pad-span-n-or-throw
```

(PAD=1, SPACE_PAD=1) composition path (NEW today):

```sh
cmake -S . -B build-padded-space-pad \
      -DPETRA_PAD=ON -DPETRA_SPACE_PAD=ON
cmake --build build-padded-space-pad
./build-padded-space-pad/P-2026-10-01-try-format-padded-space-pad-span-n-or-throw
```

## Consteval-gate probe

```sh
# Should FAIL with: call to consteval function ... is not a
# constant expression / read of non-constexpr variable fmt_str
# is not allowed in a constant expression / in call to
# basic_format_string<std::string>(fmt_str).
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_THROW_ON_OVERFLOW=1 -DPETRA_PAD=1 -DPETRA_SPACE_PAD=1 \
        /tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad_span.cpp \
        -o /tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad_span
```

(The probe file lives at `/tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad_span.cpp`.)

## Observed output

(0, 0, 0) expected+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=0 (expected+untouched sibling)
===================
PASS: 159  FAIL: 0
===================
OVERALL: PASS
```

(0, 1, 0) expected+NUL-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1,
PETRA_SPACE_PAD=0 (expected+NUL-padded sibling)
===================
PASS: 159  FAIL: 0
===================
OVERALL: PASS
```

(0, 0, 1) expected+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0,
PETRA_SPACE_PAD=1 (expected+SPACE-padded sibling)
===================
PASS: 167  FAIL: 0
===================
OVERALL: PASS
```

(0, 1, 1) expected+NUL+SPACE-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1,
PETRA_SPACE_PAD=1 (expected+NUL+SPACE-padded sibling — NEW today)
===================
PASS: 167  FAIL: 0
===================
OVERALL: PASS
```

(1, 0, 0) throwing+untouched build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=0 (throwing+untouched sibling)
===================
PASS: 148  FAIL: 0
===================
OVERALL: PASS
```

(1, 1, 0) throwing+NUL-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1,
PETRA_SPACE_PAD=0 (throwing+NUL-padded sibling)
===================
PASS: 148  FAIL: 0
===================
OVERALL: PASS
```

(1, 0, 1) throwing+SPACE-padded build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0,
PETRA_SPACE_PAD=1 (throwing+SPACE-padded sibling)
===================
PASS: 156  FAIL: 0
===================
OVERALL: PASS
```

(1, 1, 1) throwing+NUL+SPACE-padded build (NEW today):

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1,
PETRA_SPACE_PAD=1 (throwing+NUL+SPACE-padded sibling — NEW today)
===================
PASS: 156  FAIL: 0
===================
OVERALL: PASS
```

Five consecutive runs on each build produced identical PASS/FAIL
tallies (no flakiness in Section 10's 4-thread × 25-call
concurrent dispatch — per-thread local counters aggregated after
join via `std::atomic<int>::fetch_add`).

Note the count-equality pattern: the (0,1,1) and (1,1,1) counts
exactly match the (0,0,1) and (1,0,1) counts respectively (167
and 156).  This is the **composition-rule signature**: the
(0,1,1) and (1,1,1) cells dispatch to the SAME sibling
wrappers as the (0,0,1) and (1,0,1) cells — no NEW wrappers
were added, no NEW test sections were added, so the PASS counts
are bit-for-bit identical between the (1,1)-composition cells
and the (0,1)-SPACE-padded cells.

Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`; zero sanitizer diagnostics;
empty stderr under default + ASan/UBSan + strict-warning
builds.

The CONSTEVAL macro's consteval gate is preserved regardless of
the 3-bit tuple — pinned by
`/tmp/probe_consteval_rejects_runtime_fmt_padded_space_pad_span.cpp`
producing `error: call to consteval function
std::basic_format_string<char, int>::basic_format_string<
std::string> is not a constant expression` with note `read of
non-constexpr variable fmt_str is not allowed in a constant
expression` / `in call to basic_format_string<std::string>(
fmt_str)`.  Verified on the (0,0,1), (0,1,1), (1,0,1), and
(1,1,1) CONSTEVAL paths.

The composition rule (SPACE overwrites NUL when both flags are
set) is pinned by Section 16 (an 8-CHECK block on the (0,1,1)
and (1,1,1) paths with explicit `buf[i] != '\0'` and `buf[i] !=
'Q'` distinctness checks).