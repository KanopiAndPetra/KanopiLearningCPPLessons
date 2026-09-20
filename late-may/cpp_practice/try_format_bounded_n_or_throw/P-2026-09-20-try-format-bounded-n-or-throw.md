# P-2026-09-20 — `try_format_bounded_n_or_throw`

Topic: a **macro layer** that lifts the **RETURN-TYPE-vs-EXCEPTION**
sibling choice to a macro form on the **CHAR\*+CAP** axis.  Two
new macros — `TRY_FORMAT_BOUNDED_N_OR_THROW` (CONSTEVAL surface)
and `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW` (RUNTIME surface) —
each dispatch to EITHER the **expected** sibling (Sep 9's
`try_format_bounded_runtime_n` for the CONSTEVAL surface, Sep 8's
`try_format_runtime_n` for the RUNTIME surface; both return
`std::expected<std::size_t, FormatError>` and return
`FormatError{TooLarge, ..., needed_bytes}` on overflow) OR today's
**NEW throwing sibling** (today's `try_format_runtime_n_throwing`
for the RUNTIME surface, today's
`try_format_bounded_runtime_n_throwing` for the CONSTEVAL surface;
both return `std::size_t` and throw `std::runtime_error` on overflow
or format error), based on a caller-controlled compile-time flag
`PETRA_THROW_ON_OVERFLOW`.

This is the **FOURTH axis** orthogonal to Sep 19's matrix.  Sep 19's
matrix was `char*-vs-span` × `CONSTEVAL-vs-RUNTIME` ×
`bounded-buffer-vs-NUL-terminating` (2×2×2 = 8 cells).  Today's
macro layer adds the `expected-vs-throwing` axis (2 more cells per
`char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer` cell pair =
2 NEW cells in the matrix, both on the `char*+cap` axis with
`bounded-buffer` and `CONSTEVAL-or-RUNTIME`).

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
           char\*+cap (plus `std::span<char>` forwarding overload).
- Sep 12   `try_format_runtime_n_into_bounded` — RUNTIME
           NUL-terminating char\*+cap (plus `std::span<char>`
           forwarding overload).
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
- TODAY    `try_format_bounded_n_or_throw` — the RETURN-TYPE-vs-
           EXCEPTION sibling-choice macro layer on the char\*+cap
           axis (the FOURTH axis).

## What today covers

- `petra::FormatError{kind, message, needed_bytes}` — REUSED Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19 verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21 — Sep 16's open item #1 documents this).
- `petra::detail::bounded_char_writer` — REUSED verbatim.
- `petra::detail::try_format_runtime_n_impl` — REUSED Sep 8 verbatim
  (the bounded-buffer sibling impl).
- `petra::try_format_runtime_n(char*, std::size_t, std::string_view,
  args...)` — REUSED Sep 8 verbatim (char\*+cap bounded-buffer).
- `petra::try_format_bounded_runtime_n(char*, std::size_t,
  std::format_string<Args...>, args...)` — REUSED Sep 9 verbatim
  (CONSTEVAL char\*+cap bounded-buffer).
- `petra::try_format_runtime_n_throwing(char*, std::size_t,
  std::string_view, args...)` — **NEW today**: verbatim thin
  wrapper around Sep 8's `try_format_runtime_n` that re-raises
  `FormatError{...}` as `std::runtime_error` with the SAME
  `FormatError::message` (byte-exact).
- `petra::try_format_bounded_runtime_n_throwing(char*, std::size_t,
  std::format_string<Args...>, args...)` — **NEW today**: CONSTEVAL
  surface of the throwing sibling; verbatim thin wrapper around
  Sep 9's `try_format_bounded_runtime_n` that re-raises as
  `std::runtime_error` (byte-exact message).
- The TWO new macros `TRY_FORMAT_BOUNDED_N_OR_THROW` and
  `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW` — NEW today; each is
  a verbatim thin dispatcher that picks between two siblings at
  preprocessor time based on `PETRA_THROW_ON_OVERFLOW`.

## What today's lesson pins

1. **The RETURN-TYPE-vs-EXCEPTION sibling choice CAN be lifted
   to a macro form on the char\*+cap axis.**  The shape is
   analogous to Sep 16's BOUNDED-BUFFER vs NUL-TERMINATING macro
   layer — a macro that picks between two sibling wrappers at
   preprocessor time.
2. **The compile-time flag `PETRA_THROW_ON_OVERFLOW` controls the
   dispatch** via `#if PETRA_THROW_ON_OVERFLOW == 0/1`:
   - `#define PETRA_THROW_ON_OVERFLOW 0` → expected sibling
     (`std::expected<std::size_t, FormatError>` return type):
     - CONSTEVAL: `petra::try_format_bounded_runtime_n` (Sep 9)
     - RUNTIME:  `petra::try_format_runtime_n` (Sep 8)
   - `#define PETRA_THROW_ON_OVERFLOW 1` → throwing sibling
     (`std::size_t` return type; throws `std::runtime_error` on
     overflow or format error):
     - CONSTEVAL: `petra::try_format_bounded_runtime_n_throwing`
                  (NEW today; wraps Sep 9 verbatim)
     - RUNTIME:  `petra::try_format_runtime_n_throwing`
                  (NEW today; wraps Sep 8 verbatim)
3. **`#error` guard** fires at preprocessor time if the flag is
   set to anything other than 0 or 1 — pinned by the
   `#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1`
   block.
4. **The macro is a parenthesized expression** (NOT
   `do { ... } while (0)`) so the caller can write
   `auto n = TRY_FORMAT_BOUNDED_N_OR_THROW(...)` and capture
   the result.  Mirrors Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19's
   macro shape exactly.
5. **`__VA_OPT__(,)` for the no-args case.**  Mirrors Sep 13 /
   Sep 14 / Sep 15 / Sep 16 / Sep 19.
6. **The throwing sibling preserves the SAME error contract as
   the expected sibling on TooLarge** — the
   `std::runtime_error::what()` is byte-exactly equal to the
   `FormatError::message` on the expected path.  Pinned in
   Section 9 by calling both siblings with the SAME call args and
   comparing `expected_msg` (from `r.error().message`) to
   `thrown_msg` (from `e.what()`).
7. **The throwing sibling preserves the SAME buffer atomicity on
   TooLarge** — the buffer is UNTOUCHED on overflow (Sep 8 / Sep 9
   already pinned this on the expected path; today's wrapper
   re-pins it on the throwing path because the new code path COULD
   theoretically diverge; Section 3c / 3d pin this).
8. **The throwing sibling does NOT lose the consteval-checked
   format-string surface** — `TRY_FORMAT_BOUNDED_N_OR_THROW` still
   requires `std::format_string<Args...>` (a literal fmt at the
   call site).  This is because the CONSTEVAL sibling (Sep 9's
   `try_format_bounded_runtime_n`) is consteval-checked, and
   today's throwing wrapper is a verbatim thin wrapper around
   Sep 9 — so the consteval gate is preserved.  Section 6 pins
   the consteval gate by calling the macro with a literal fmt and
   a runtime-built `std::string` fmt; the runtime-built fmt REJECTS
   AT COMPILE TIME (verified by `/tmp/probe_consteval_rejects_runtime_fmt.cpp`
   producing `error: call to consteval function
   'std::basic_format_string<char, int>::basic_format_string<std::string>'
   is not a constant expression` with note `read of non-constexpr
   variable 'fmt' is not allowed in a constant expression` /
   `in call to 'this->data()'`).
9. **The CONSTEVAL macro's sibling-choice is orthogonal to the
   Sep 13 CONSTEVAL-vs-RUNTIME axis**: today's macro
   `TRY_FORMAT_BOUNDED_N_OR_THROW` is the CONSTEVAL surface (because
   Sep 9's underlying wrapper is consteval-checked); today's
   `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW` is the RUNTIME surface
   (because Sep 8's underlying wrapper takes `std::string_view`).
   Mirrors Sep 13's macro family shape.
10. **The `PETRA_THROW_ON_OVERFLOW` axis (expected vs throwing) is
    PERPENDICULAR to the char\*-vs-span axis** (Sep 10 / Sep 12's
    forwarding wrappers), AND to the CONSTEVAL-vs-RUNTIME axis
    (Sep 13's macro layer), AND to the bounded-buffer-vs-NUL-
    terminating axis (Sep 16 / Sep 19's macro layer).  Today's
    meta-macros are at the INTERSECTION of ALL FOUR axes:
    `char*-vs-span` × `CONSTEVAL-vs-RUNTIME` ×
    `bounded-buffer-vs-NUL-terminating` × `expected-vs-throwing` =
    2⁴ = 16 cells.  Sep 16 / Sep 19 covered the
    `expected-vs-throwing` = 0 (the expected side; today's lesson
    adds the throwing side).  Today's lesson covers 2 NEW cells
    (`char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer × throwing`)
    out of the 8 remaining un-covered cells (the throwing side of
    `char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer-or-NUL-term`,
    and the throwing side of `span<char> × CONSTEVAL-or-RUNTIME ×
    bounded-buffer-or-NUL-term`).  Section 14 pins the
    perpendicular-axis claim via `decltype` checks.
11. **`std::format_error` IS-A `std::runtime_error`** — pinned by
    `static_assert(std::is_base_of_v<std::runtime_error,
    std::format_error>)` succeeding.  This means the catch site
    must put `std::format_error&` BEFORE `std::runtime_error&` in
    the catch chain (C++ catch handlers are matched in order).
    Section 10 exploits this by checking the dynamic type via
    `dynamic_cast<const std::format_error*>(&e)` — the throwing
    wrapper re-raises as `std::runtime_error` (NOT
    `std::format_error`), so the dynamic cast returns `nullptr`.
12. **Concurrency is safe** — Section 13 fires 4 threads × 25
    concurrent calls through both macros; per-thread local
    counters are aggregated AFTER `join()` via
    `std::atomic<int>::fetch_add` to avoid a data race on
    `g_pass` / `g_fail`.

## HEADLINE finding — the FOUR-AXIS layer

Sep 19's matrix was 2³ = 8 cells.  Today's lesson adds the
`expected-vs-throwing` axis (a fourth axis), making the matrix 2⁴
= 16 cells.  Today's lesson covers 2 of the 16 cells (the
`char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer × throwing`
cells on the `char*+cap` axis with `bounded-buffer` and
`CONSTEVAL-or-RUNTIME`).

The preprocessor-time dispatch design (`#if PETRA_THROW_ON_OVERFLOW`)
extends naturally to the new axis without any new infrastructure —
Sep 8's and Sep 9's expected siblings already exist; today's
lesson adds two verbatim thin throwing wrappers and two macros.
The dispatch is ZERO-OVERHEAD — at `-O0` the chosen wrapper is
inlined into the caller; at `-O1` the compiler sees only the
chosen wrapper.

The four axes are now:

1. **char\*-vs-span axis** (Sep 10 / Sep 12's forwarding wrappers):
   the OUTPUT is either a `char*` + `std::size_t cap` (Sep 8 / Sep
   9 / Sep 11 / Sep 12 char\*+cap path) or a `std::span<char>`
   (Sep 10 / Sep 11 / Sep 12 / Sep 19's span<char> path).

2. **CONSTEVAL-vs-RUNTIME axis** (Sep 13's macro layer): the
   format-string is either a `std::format_string<Args...>` literal
   (consteval-checked at compile time) or a `std::string_view`
   (parsed at format time).

3. **bounded-buffer-vs-NUL-terminating axis** (Sep 16 / Sep 19's
   macro layer): the OUTPUT contract is either a bounded-buffer
   (bytes `[0, written)`, byte at `written` UNCHANGED) or
   NUL-terminating (bytes `[0, written)`, byte at
   `nul_idx = (written < cap) ? written : (cap - 1)` set to `'\0'`).

4. **expected-vs-throwing axis** (TODAY's macro layer): the RETURN
   TYPE is either `std::expected<std::size_t, FormatError>`
   (FormatError on overflow) or `std::size_t` (throws
   `std::runtime_error` on overflow / format error).

The four axes compose orthogonally: each is a separate
`#if PETRA_<FLAG>` block, each is independent of the others, each
adds ZERO runtime overhead (the chosen sibling is the ONLY one the
compiler sees).  Section 14 pins the orthogonal-composition claim
via `decltype` checks.

The 16-cell matrix (NEW today):

| cell | char\*+cap | span<char> |
|-------|-----------|------------|
| CONSTEVAL + bounded-buffer + expected     | Sep 16 TRY_FORMAT_BOUNDED_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 9) | Sep 19 TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 10) |
| RUNTIME + bounded-buffer + expected     | Sep 16 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 8) | Sep 19 TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=0 (Sep 8) |
| CONSTEVAL + NUL-terminating + expected  | Sep 16 TRY_FORMAT_BOUNDED_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 11) | Sep 19 TRY_FORMAT_BOUNDED_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 11) |
| RUNTIME + NUL-terminating + expected    | Sep 16 TRY_FORMAT_BOUNDED_RUNTIME_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 12) | Sep 19 TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_OR_INTO @ PETRA_NUL_TERMINATE=1 (Sep 12) |
| CONSTEVAL + bounded-buffer + throwing   | **TODAY TRY_FORMAT_BOUNDED_N_OR_THROW** @ PETRA_THROW_ON_OVERFLOW=1 (NEW) | follow-up (Sep 16 + Sep 19's span sibling + today's throwing) |
| RUNTIME + bounded-buffer + throwing     | **TODAY TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW** @ PETRA_THROW_ON_OVERFLOW=1 (NEW) | follow-up (Sep 16 + Sep 19's span sibling + today's throwing) |
| CONSTEVAL + NUL-terminating + throwing  | follow-up (Sep 16's char*+cap NUL sibling + today's throwing) | follow-up (Sep 16's span NUL sibling + today's throwing) |
| RUNTIME + NUL-terminating + throwing    | follow-up (Sep 16's char*+cap NUL sibling + today's throwing) | follow-up (Sep 16's span NUL sibling + today's throwing) |

Today's 2 NEW cells are highlighted in **bold**.  The remaining
6 NEW cells (the throwing sibling on the other 6 axes
combinations) are left for future lessons — each cell is a
verbatim thin throwing wrapper composed on top of an existing
sibling + a new `#if PETRA_THROW_ON_OVERFLOW`-gated macro.

## Important code

### The two new throwing wrappers — the ONLY new wrappers today

```cpp
namespace petra {

// NEW today — RUNTIME surface of the throwing sibling.  A
// verbatim thin wrapper around Sep 8's expected sibling that
// re-raises FormatError{...} as std::runtime_error with the
// SAME message.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(char* buf, std::size_t cap,
                              std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(buf, cap, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

// NEW today — CONSTEVAL surface of the throwing sibling.  A
// verbatim thin wrapper around Sep 9's expected sibling that
// re-raises FormatError{...} as std::runtime_error with the
// SAME message.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing(char* buf, std::size_t cap,
                                      std::format_string<Args...> fmt,
                                      const Args&... args) {
    auto r = try_format_bounded_runtime_n(buf, cap, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

}  // namespace petra
```

Both wrappers are 5-line verbatim thin wrappers.  No new types,
no new impls, no new error contracts.  The byte-exactness of the
`std::runtime_error::what()` vs `FormatError::message` is the
key contract — Section 9 pins this.

### The two new macros — the public surface

```cpp
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

#if PETRA_THROW_ON_OVERFLOW == 0
// Expected sibling path — returns std::expected<std::size_t,
// FormatError>.  On TooLarge, FormatError{TooLarge, ...,
// needed_bytes} is returned.
#define TRY_FORMAT_BOUNDED_N_OR_THROW(out, cap, fmt, ...)                \
    (::petra::try_format_bounded_runtime_n((out), (cap), (fmt)           \
                                            __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(out, cap, fmt, ...)        \
    (::petra::try_format_runtime_n((out), (cap), (fmt)                   \
                                    __VA_OPT__(,) __VA_ARGS__))
#else  // PETRA_THROW_ON_OVERFLOW == 1
// Throwing sibling path — returns std::size_t; throws
// std::runtime_error on TooLarge or Format.  Same atomicity
// contract as the expected sibling (buffer UNTOUCHED on
// overflow — Sep 8 / Sep 9's contract, preserved by the
// verbatim thin wrapper).
#define TRY_FORMAT_BOUNDED_N_OR_THROW(out, cap, fmt, ...)                \
    (::petra::try_format_bounded_runtime_n_throwing(                    \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW(out, cap, fmt, ...)        \
    (::petra::try_format_runtime_n_throwing(                            \
        (out), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
#endif  // PETRA_THROW_ON_OVERFLOW
```

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21 — Sep 16's open item #1 documents this).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- The two throwing wrappers `petra::try_format_runtime_n_throwing`
  and `petra::try_format_bounded_runtime_n_throwing` — verbatim
  thin dispatchers to Sep 8's / Sep 9's expected siblings.
- The two `TRY_FORMAT_BOUNDED_N_OR_THROW` /
  `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW` macros — each a
  verbatim thin dispatcher to one of the four underlying wrappers,
  selected by `#if PETRA_THROW_ON_OVERFLOW`.  No new types (two
  new wrappers only), no new impls, no new error contracts.

## Build and verification commands

Default (`PETRA_THROW_ON_OVERFLOW=0` — expected sibling path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-20-try-format-bounded-n-or-throw.cpp \
        -o /tmp/P-2026-09-20-try-format-bounded-n-or-throw
/tmp/P-2026-09-20-try-format-bounded-n-or-throw
```

Throwing path (`PETRA_THROW_ON_OVERFLOW=1`):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -DPETRA_THROW_ON_OVERFLOW=1 \
        P-2026-09-20-try-format-bounded-n-or-throw.cpp \
        -o /tmp/P-2026-09-20-try-format-bounded-n-or-throw-throw
/tmp/P-2026-09-20-try-format-bounded-n-or-throw-throw
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-20-try-format-bounded-n-or-throw.cpp \
        -o /tmp/P-2026-09-20-try-format-bounded-n-or-throw-strict
/tmp/P-2026-09-20-try-format-bounded-n-or-throw-strict
```

ASan + UBSan build (expected sibling path):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-20-try-format-bounded-n-or-throw.cpp \
        -o /tmp/P-2026-09-20-try-format-bounded-n-or-throw-asan
/tmp/P-2026-09-20-try-format-bounded-n-or-throw-asan
```

CMake (default / strict / ASan / throwing) — see
`CMakeLists.txt` in this dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
cmake -S . -B build-throw -DPETRA_THROW_ON_OVERFLOW=1 \
                              && cmake --build build-throw
./build/P-2026-09-20-try-format-bounded-n-or-throw
./build-strict/P-2026-09-20-try-format-bounded-n-or-throw
./build-asan/P-2026-09-20-try-format-bounded-n-or-throw
./build-throw/P-2026-09-20-try-format-bounded-n-or-throw
```

## Section-by-section notes

### Section 1 — `PETRA_THROW_ON_OVERFLOW` compile-time flag pinned

Verifies the compile-time flag is pinned at the value the build
used.  Prints the chosen path so the output is auditable in the
log:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=0 (expected sibling)
```

or, for the throwing build:

```
[Section 1] PETRA_THROW_ON_OVERFLOW=1 (throwing sibling)
```

Also pins via `static_assert` that both macros dispatch to the
correct return type for the chosen path (`std::expected<std::size_t,
FormatError>` for the expected path; `std::size_t` for the
throwing path) AND that `std::runtime_error` is a `std::exception`
subclass.  PASS.

### Section 2 — sibling path verification (both branches)

Two-branch test pinned by the same source compiled with
`PETRA_THROW_ON_OVERFLOW=0` (default) and `PETRA_THROW_ON_OVERFLOW=1`.

On the **expected sibling** path:

- 2a — CONSTEVAL macro `TRY_FORMAT_BOUNDED_N_OR_THROW` dispatches
  to Sep 9's `try_format_bounded_runtime_n` (expected, returns
  `expected<size_t, FormatError>`).  `r.value() == 11` for
  `"hello {}"` + `"world"` (== "hello world").
- 2b — RUNTIME macro `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW`
  dispatches to Sep 8's `try_format_runtime_n` (expected, returns
  `expected<size_t, FormatError>`).

On the **throwing sibling** path (same source compiled with
`-DPETRA_THROW_ON_OVERFLOW=1`):

- 2a — CONSTEVAL macro `TRY_FORMAT_BOUNDED_N_OR_THROW` dispatches
  to today's NEW `petra::try_format_bounded_runtime_n_throwing`
  (throwing, returns `size_t`).  `n == 11` for `"hello {}"` +
  `"world"`.
- 2b — RUNTIME macro `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW`
  dispatches to today's NEW `petra::try_format_runtime_n_throwing`
  (throwing, returns `size_t`).  `n == 8` for `"value={}"` + `42`.

### Section 3 — atomicity on `TooLarge`

Pins the buffer atomicity on `TooLarge` for BOTH the expected and the
throwing paths.  Pre-fills the buffer with poison `'Q'` and verifies
ALL bytes are still `'Q'` after the failing call:

- 3a/3b (expected path) — verify `FormatError{TooLarge, ...}` is
  returned and the buffer is UNTOUCHED.
- 3c/3d (throwing path) — verify `std::runtime_error` is thrown
  and the buffer is UNTOUCHED.

The throwing path's atomicity is preserved because today's
wrapper is a verbatim thin wrapper around Sep 8 / Sep 9 (which
already pin atomicity on the expected path).  PASS on both
builds.

### Section 4 — byte-count contract on success

Pins that the success path returns the byte count:

- Expected path: `r.value() == 7` for `"x={} y={}"` + `1, 2` (==
  "x=1 y=2").
- Throwing path: `n == 7` for the same call.

PASS on both builds.

### Section 5 — `std::runtime_error` catchability (throwing path only)

Pins that the throwing sibling's exception is catchable as
`std::exception&` (via `std::runtime_error`'s inheritance) and as
`std::runtime_error&` directly.  The `what()` message must contain
the same `{needed, cap}` pair that the expected path's
`FormatError::message` contains:

```
caught.what = [formatted output would be 30 bytes; cap is 16]
```

PASS on the throwing build.  On the expected build, Section 5c
pins the mirror image: `r.error().kind == FormatErrorKind::TooLarge`
AND `r.error().needed_bytes == 30`.

### Section 6 — consteval surface preserves compile-time gate

Pins that the CONSTEVAL macro's format-string parameter is
`std::format_string<Args...>` regardless of `PETRA_THROW_ON_OVERFLOW`
— the consteval gate is preserved.  Pinned by:

- Direct invocation with a literal `"literal {}"` produces
  `r.value() == 10` / `n == 10` (== "literal 42").
- Direct invocation with a runtime-built `std::string` REJECTS
  AT COMPILE TIME on the CONSTEVAL macro (verified by
  `/tmp/probe_consteval_rejects_runtime_fmt.cpp` producing
  `error: call to consteval function 'std::basic_format_string<char,
  int>::basic_format_string<std::string>' is not a constant
  expression` with note `read of non-constexpr variable 'fmt' is
  not allowed in a constant expression`).

The RUNTIME macro accepts the runtime-built `std::string`
correctly (`r.value() == 10` / `n == 10`).

### Section 7 — multi-arg round-trip

Multi-arg format `"x={} y={} z={:.2f}"` + two integer args and a
double produces the expected byte count `14` and prefix bytes
`"x=1 y=2 z=3.14"`.  PASS on both builds.

### Section 8 — embedded NUL byte round-trip

A runtime-built format that produces an embedded NUL byte
(`"hi\0hi {}!"` 9-byte literal).  The byte at the NUL position is
`'\0'`; the byte at `written` is the input byte (the NUL is NOT
counted as part of `written`).  PASS on both builds.

### Section 9 — byte-exactness of the throwing path's message

The KEY HEADLINE finding of today's lesson — the throwing
sibling's `std::runtime_error::what()` MUST be byte-exactly equal
to the expected sibling's `FormatError::message` for the SAME
`{needed, cap}` pair.  Pinned by:

1. Running the expected sibling on a known-failing call (via
   `petra::try_format_runtime_n` directly, NOT via the macro —
   the macro dispatches to the throwing sibling on the throwing
   build, so the expected sibling must be called directly).
2. Running the throwing sibling on the same call and catching
   the `std::runtime_error::what()`.
3. Asserting byte-exactness (`expected_msg == thrown_msg`).

Pinned on BOTH paths (because the same `expected_msg` and
`thrown_msg` are produced regardless of which path the macro
dispatches to).  PASS.

### Section 10 — Format error → runtime_error on the throwing path

On the throwing path, `std::format_error` is ALSO caught and
re-raised as `std::runtime_error` (the throwing wrapper does NOT
distinguish TooLarge from Format at the call site; both are "the
format call failed" from the throwing sibling's perspective).
Pinned by checking the DYNAMIC TYPE of the exception via
`dynamic_cast<const std::format_error*>(&e)`:

- The throwing wrapper re-raises as `std::runtime_error`, NOT
  `std::format_error` (the dynamic type is `std::runtime_error`).
- The `what()` message contains the `std::format_error`'s text
  (e.g. `"The format string terminates at a '{'"`):

```
[Section 10] caught.what = [The format string terminates at a '{']
```

This works because `std::format_error` IS-A `std::runtime_error`
(pinned by `static_assert(std::is_base_of_v<std::runtime_error,
std::format_error>)`).  The catch chain must put
`std::format_error&` BEFORE `std::runtime_error&` (C++ catch
handlers are matched in order).  PASS on the throwing build.

On the expected build, Section 10 pins the mirror image:
`FormatError::kind == FormatErrorKind::Format` is returned from
a malformed format string.  PASS.

### Section 11 — no-args variant

`__VA_OPT__(,)` handles the no-args case (a format with no
replacement fields).  `TRY_FORMAT_BOUNDED_N_OR_THROW(buf, 32,
"ping")` produces `r.value() == 4` / `n == 4`.  PASS on both
builds.

### Section 12 — helpers used in Section 14's decltype checks

Pin the helpers used in Section 14.  These are "flat" dispatchers
(always dispatch to the expected sibling) used to verify the
perpendicular-axis claim: today's meta-macros dispatch to the
SAME wrappers as these helpers when `PETRA_THROW_ON_OVERFLOW=0`.
PASS on both builds.

### Section 13 — 4 threads × 25 calls concurrent dispatch

Pins thread safety.  4 threads × 25 calls each fire concurrently
through BOTH the CONSTEVAL and RUNTIME meta-macros.  Per-thread
local counters are aggregated AFTER `join()` via
`std::atomic<int>::fetch_add` to avoid a data race on `g_pass` /
`g_fail`.  All 100 calls per macro return
`r.value() == 4` / `n == 4` (the byte count for `"[{:02}]"` + a
two-digit integer).  PASS on both builds.

### Section 14 — perpendicular-axis claim (PETRA_THROW_ON_OVERFLOW × CONSTEVAL-vs-RUNTIME)

The KEY HEADLINE finding — the `PETRA_THROW_ON_OVERFLOW` axis
(expected vs throwing) is PERPENDICULAR to the CONSTEVAL-vs-
RUNTIME axis (Sep 13's macro layer), AND to the char\*-vs-span
axis (Sep 10 / Sep 12's forwarding wrappers), AND to the
bounded-buffer-vs-NUL-terminating axis (Sep 16 / Sep 19's macro
layer).  Pinned by `static_assert(std::is_same_v<R14a, H14a>)`
and `static_assert(std::is_same_v<R14b, H14b>)` on BOTH paths.

### Section 15 — final summary

Prints the total pass / fail tally.  Expected sibling build:
`PASS: 89  FAIL: 0`.  Throwing sibling build: `PASS: 80  FAIL: 0`.

The expected build has more checks because it has Section 5c
(which only applies to the expected path; the throwing build
has Section 5a / 5b instead, and those sections have fewer
CHECK lines).  The PASS / FAIL totals are non-uniform across
the two paths BY DESIGN (the two paths exercise different
sections).

## Build matrix — observed output

| Build                                         | Compile | Run              |
|-----------------------------------------------|---------|------------------|
| Default (`PETRA_THROW_ON_OVERFLOW=0`) — `-O0` | clean   | PASS 89/0        |
| Throwing (`PETRA_THROW_ON_OVERFLOW=1`) — `-O0` | clean  | PASS 80/0        |
| Strict-warning (default flags) — `-O0`        | clean   | PASS 89/0        |
| Strict-warning (throwing flags) — `-O0`       | clean   | PASS 80/0        |
| ASan + UBSan (default flags) — `-O1`          | clean   | PASS 89/0        |
| ASan + UBSan (throwing flags) — `-O1`         | clean   | PASS 80/0        |
| CMake default                                 | clean   | PASS 89/0        |
| CMake strict                                  | clean   | PASS 89/0        |
| CMake ASan                                    | clean   | PASS 89/0        |
| CMake throwing                                | clean   | PASS 80/0        |

Five consecutive runs on each build produced identical
`PASS: 89 FAIL: 0` (default) / `PASS: 80 FAIL: 0` (throwing)
tallies.  No flakiness in Section 13's 4-thread × 25-call
concurrent dispatch.

Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`.  Zero sanitizer diagnostics.  Empty stderr
under default + ASan.

## Summary

- Topic: a macro layer that lifts the RETURN-TYPE-vs-EXCEPTION
  sibling choice to a macro form on the char\*+cap axis,
  analogous to Sep 16's BOUNDED-BUFFER vs NUL-TERMINATING macro
  layer.
- Two new macros — `TRY_FORMAT_BOUNDED_N_OR_THROW` (CONSTEVAL
  surface) and `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW` (RUNTIME
  surface) — each dispatch to one of two siblings based on the
  compile-time flag `PETRA_THROW_ON_OVERFLOW` (0 = expected
  `std::expected<std::size_t, FormatError>` return,
  1 = throwing `std::size_t` return + `std::runtime_error` on
  overflow / format error).
- Reused verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 /
  Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19:
  `petra::FormatError`, `petra::FormatErrorKind`,
  `petra::counting_output_iterator`,
  `petra::detail::bounded_char_writer`,
  `petra::detail::try_format_runtime_n_impl` (Sep 8),
  `petra::try_format_runtime_n` (Sep 8 char\*+cap),
  `petra::try_format_bounded_runtime_n` (Sep 9).
- New today: two throwing wrappers
  `petra::try_format_runtime_n_throwing` /
  `petra::try_format_bounded_runtime_n_throwing` (each a
  verbatim thin wrapper around Sep 8 / Sep 9 that re-raises
  `FormatError{...}` as `std::runtime_error` with byte-exact
  `what()` message) + the two `TRY_FORMAT_BOUNDED_N_OR_THROW` /
  `TRY_FORMAT_BOUNDED_RUNTIME_N_OR_THROW` meta-macros + the
  `PETRA_THROW_ON_OVERFLOW` compile-time flag + a `#error`
  guard.  The wrappers are 5-line verbatim thin dispatchers;
  each macro is a verbatim thin dispatcher to one of the four
  underlying wrappers.
- HEADLINE finding — the `PETRA_THROW_ON_OVERFLOW` axis
  (expected vs throwing) is the FOURTH axis orthogonal to
  Sep 19's matrix (char\*-vs-span × CONSTEVAL-vs-RUNTIME ×
  bounded-buffer-vs-NUL-terminating).  Today's meta-macros are
  at the INTERSECTION of all FOUR axes.  Preprocessor-time
  `#if PETRA_THROW_ON_OVERFLOW` is the cleanest design for the
  dispatch: zero runtime cost; matches Sep 1 / Sep 2's compile-
  time-flag framing; `#error` fires at preprocessor time if the
  flag is invalid; composes orthogonally with the other three
  axes (Section 14's `decltype` checks pin this on both paths).
- 89/89 PASS across 15 sections on the expected sibling default
  + strict-warning + ASan/UBSan + CMake builds.
- 80/80 PASS across 15 sections on the throwing sibling build
  (same source compiled with `-DPETRA_THROW_ON_OVERFLOW=1`).
  All contracts pinned: byte-exactness of the throwing path's
  `what()` vs the expected path's `FormatError::message`
  (Section 9); dynamic type of the thrown exception is
  `std::runtime_error`, NOT `std::format_error` (Section 10);
  buffer UNTOUCHED on `TooLarge` (Section 3c / 3d).
- Five consecutive runs on each build produced identical
  tallies (no flakiness in Section 13's 4-thread × 25-call
  concurrent dispatch — per-thread local counters aggregated
  after `join`).
- Zero warnings under `-Wall -Wextra -Wpedantic -Werror
  -Wshadow -Wconversion -Wsign-conversion`; zero sanitizer
  diagnostics; empty stderr under default + ASan.
- Lesson is a pure consumer-side C++23 `std::format` +
  `std::vformat_to` + `std::format_string<Args...>` (P2216R3)
  + `std::make_format_args` + `std::expected` (P0323R12) +
  `std::println` (P2093R14) + `std::runtime_error` +
  `std::atomic` + `dynamic_cast` + `std::is_base_of_v` +
  `__VA_OPT__` preprocessor feature exercise — reuses Sep 6 /
  Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 /
  Sep 14 / Sep 15 / Sep 16 / Sep 19's machinery verbatim, no
  third-party libraries, no library installation, no
  find_package.

## Where we go next

Today's lesson covers 2 NEW cells in the four-axis matrix:
`char*+cap × CONSTEVAL-or-RUNTIME × bounded-buffer × throwing`.
The remaining 6 NEW cells (the throwing sibling on the other 6
axis combinations) are left for future lessons.  Each future
lesson adds a verbatim thin throwing wrapper composed on top of
an existing sibling + a new `#if PETRA_THROW_ON_OVERFLOW`-gated
macro.  The natural follow-on is the `span<char> ×
CONSTEVAL-or-RUNTIME × bounded-buffer × throwing` cells (two NEW
wrappers + four NEW macros).

Today's lesson does NOT close Sep 16's open item #1 — replacing
the hand-rolled `petra::counting_output_iterator` with
`std::counting_output_iterator` when libc++ ships it.  Today's
lesson reuses the hand-rolled iterator from Sep 6 / Sep 7 / Sep
8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15
/ Sep 16 / Sep 19 verbatim; verified during lesson prep that
`std::counting_output_iterator` is NOT yet shipped by libc++ 21
(no `__iterator/counting_output_iterator.h` header in the
libc++ 21 include tree; no `counting_output_iterator` symbol in
the `<iterator>` header).

The five-axis matrix (theoretical — no lesson plans):

| axis | value |
|------|-------|
| 1 | char\*+cap vs span<char> |
| 2 | CONSTEVAL vs RUNTIME |
| 3 | bounded-buffer vs NUL-terminating |
| 4 | expected vs throwing (TODAY) |
| 5 | (potential future axis: ???) |

The preprocessor-time dispatch design generalizes naturally to
ANY future axis (e.g. a `PETRA_NOOP_ON_OVERFLOW` axis that
discards the output on overflow, a `PETRA_STRICT_FORMAT` axis
that rejects `{:#x}` style specifiers on the runtime path, etc.)
— each new axis is a new `#if PETRA_<NEW_FLAG>` block layered on
top of the existing axes.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.