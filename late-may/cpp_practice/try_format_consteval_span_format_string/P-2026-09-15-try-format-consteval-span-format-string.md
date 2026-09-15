# P-2026-09-15 — try_format_consteval_span_format_string: HEADLINE FINDING — the CONSTEVAL surface is fundamentally incompatible with `std::span<const char>` for the format-string parameter

## Topic

The CONSTEVAL surface (Sep 7 / Sep 9 / Sep 11's
`try_format_bounded` / `try_format_bounded_runtime_n` /
`format_to_n_into_bounded` wrappers) is **fundamentally
incompatible** with `std::span<const char>` for the format-string
parameter — by design, by the `consteval` keyword on
`std::basic_format_string`'s ctor.  Today's lesson pins the finding
and adds a CONSTEVAL-SHAPED RUNTIME PARSER family of wrappers
(`try_format_consteval_span_bounded` / `_runtime_n` /
`_runtime_n_into_bounded` / `_runtime_n_span`) that ACCEPT a
`std::span<const char>` format-string parameter and parse the
format string at runtime via `std::vformat_to`.  The trade-off is
explicit in the function-name prefix and the macro prefix: malformed
format strings produce RUNTIME errors, not compile errors.

Closes Sep 14's "Where we go next" item #3 verbatim.

## Concepts

### Sep 14's note was wrong on TWO counts

Sep 14's "Where we go next" item #3 said:

> 3. A `std::span<const char>` overload for the CONSTEVAL
>    surface (Sep 7's `try_format_bounded`, Sep 9's
>    `try_format_bounded_runtime_n`, Sep 11's
>    `format_to_n_into_bounded`).  Today's macros accept
>    `std::span<const char>` for the RUNTIME surface; the
>    CONSTEVAL surface's `std::format_string<Args...>` already
>    has a deduction guide for ranges satisfying
>    `contiguous_range` + `sized_range` over char, so the
>    CONSTEVAL wrappers should already accept
>    `std::span<const char>` at the call site via implicit
>    deduction — but a separate compile-time probe + Section
>    could pin this contract explicitly.

Sep 14's note was wrong on **TWO** counts.  Both are pinned by
today's lesson:

#### Count 1 — the consteval ctor has a `requires convertible_to<...>` clause that REJECTS `std::span<const char>`

The `std::basic_format_string<Args...>` consteval ctor has a
`requires convertible_to<const _Tp&, basic_string_view<_CharT>>`
clause.  The conversion `std::span<const char> -> std::string_view`
is `explicit` on libc++ 21 (Sep 14's finding, re-pinned in Section
1) — so `convertible_to` is FALSE for `std::span<const char>`,
and the consteval ctor REJECTS it entirely.

Pinned by `/tmp/probe_consteval_rejects_span_for_fmt.cpp`:

```
$ cat /tmp/probe_consteval_rejects_span_for_fmt.cpp
#include <format>
#include <span>
#include <string>

int main() {
    const char lit[8] = {'h','e','l','l','o',' ','{','}'};
    std::span<const char> fmt_span{lit};
    std::format_string<char const*> fs1 = std::format_string<char const*>{fmt_span};
    (void)fs1;
    return 0;
}

$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_consteval_rejects_span_for_fmt.cpp \
          -o /tmp/probe_consteval_rejects_span_for_fmt
/tmp/probe_consteval_rejects_span_for_fmt.cpp:14:43: error: no
    matching constructor for initialization of
    'std::format_string<const char *>' (aka 'basic_format_string<char, const char *>')
... note: candidate template ignored: constraints not satisfied
    [with _Tp = std::span<const char>]
    because 'convertible_to<const std::span<const char> &,
    basic_string_view<char>>' evaluated to false
    because 'is_convertible_v<const std::span<const char> &,
    std::string_view>' evaluated to false
```

#### Count 2 — even a runtime `std::string_view` cannot satisfy the consteval gate

Even if you construct a `std::string_view` from the span at the
call site and pass that to the consteval ctor, the consteval
function still REJECTS it — because the consteval function must
be called from a constant expression context, and a
`std::string_view` constructed at runtime is NOT a constant
expression.

Pinned by `/tmp/probe_consteval_rejects_runtime_stringview.cpp`:

```
$ cat /tmp/probe_consteval_rejects_runtime_stringview.cpp
#include <format>
#include <span>
#include <string>

int main() {
    const char lit[8] = {'h','e','l','l','o',' ','{','}'};
    std::span<const char> fmt_span{lit};
    std::string_view sv{fmt_span.data(), fmt_span.size()};
    std::format_string<char const*> fs1 = std::format_string<char const*>{sv};
    (void)fs1;
    return 0;
}

$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_consteval_rejects_runtime_stringview.cpp \
          -o /tmp/probe_consteval_rejects_runtime_stringview
/tmp/probe_consteval_rejects_runtime_stringview.cpp:14:43: error:
    call to consteval function 'std::basic_format_string<char,
    const char *>::basic_format_string<std::string_view>' is not
    a constant expression
... note: read of non-constexpr variable 'sv' is not allowed in
    a constant expression
... note: in call to 'basic_string_view(sv)'
```

The two probes together pin the HEADLINE finding: the CONSTEVAL
surface is fundamentally incompatible with `std::span<const char>`
for the format-string parameter, by design.

### The architectural answer: the RUNTIME surface (Sep 14)

The right architectural answer for "std::span<const char> format
string at the call site" is the RUNTIME surface — Sep 14's
`try_format_runtime_bounded` / `_n` / `_n_into_bounded` family
with the `std::span<const char>` format-string overloads Sep 14
added.  Today's lesson TRANSITIVELY exercises Sep 14's surface via
the new `try_format_consteval_span_bounded` wrapper (Section 3),
which shares Sep 6's `try_format_runtime_bounded_impl` with Sep
14's wrapper — so Section 3 is a transitive pin for Sep 14's
RUNTIME surface.

### Today's lesson: a CONSTEVAL-SHAPED RUNTIME PARSER family

Today's lesson ALSO adds a CONSTEVAL-SHAPED RUNTIME PARSER family
of wrappers — a deliberate trade-off.  The new wrappers accept a
`std::span<const char>` format-string parameter and use
`std::vformat_to` directly, bypassing the consteval gate.  The
trade-off is documented clearly in the function-name prefix
(`_consteval_span_`) and in the macro prefix
(`TRY_FORMAT_CONSTEVAL_SPAN_`):

- **GAIN**: callers get a `std::span<const char>` format-string
  parameter at the call site, just like Sep 14's RUNTIME surface.
- **LOSS**: malformed format strings are now RUNTIME errors, not
  COMPILE errors.  The consteval gate is intentionally bypassed.

The CONSTEVAL surface for **literal** format strings (Sep 7 / Sep
9 / Sep 11) is unchanged — those wrappers still reject malformed
format strings as HARD COMPILE ERRORS at the call site.

### The new wrappers

```cpp
namespace petra {

// CONSTEVAL-SHAPED RUNTIME PARSER.  TRADE-OFF: format-string parse
// happens at runtime, not consteval.  Accepts std::span<const char>
// for the format-string parameter.
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_consteval_span_bounded(std::span<const char> fmt, std::size_t cap,
                                  const Args&... args) {
    return detail::try_format_runtime_bounded_impl(
        std::string_view{fmt.data(), fmt.size()}, cap, args...);
}

// char*+cap bounded-buffer, NO NUL termination (Sep 8 shape).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_consteval_span_runtime_n(char* buf, std::size_t cap,
                                    std::span<const char> fmt,
                                    const Args&... args) {
    return detail::try_format_runtime_n_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

// char*+cap NUL-terminating (Sep 12 shape).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_consteval_span_runtime_n_into_bounded(char* buf, std::size_t cap,
                                                 std::span<const char> fmt,
                                                 const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

// std::span<char> output + NUL-terminating (Sep 12 std::span<char> shape).
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_consteval_span_runtime_n_span(std::span<char> out,
                                         std::span<const char> fmt,
                                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        out.data(), out.size(),
        std::string_view{fmt.data(), fmt.size()}, args...);
}

}  // namespace petra
```

Each new wrapper is a verbatim thin forwarding wrapper — it
builds a `std::string_view` from the span EXPLICITLY and
dispatches to the existing RUNTIME impl UNCHANGED.

### The new macros

```cpp
#define TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(cap, fmt, ...)                  \
    (::petra::try_format_consteval_span_bounded((fmt), (cap)              \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_CONSTEVAL_SPAN_N(buf, cap, fmt, ...)                   \
    (::petra::try_format_consteval_span_runtime_n((buf), (cap), (fmt)    \
                                                  __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_CONSTEVAL_SPAN_INTO(buf, cap, fmt, ...)                \
    (::petra::try_format_consteval_span_runtime_n_into_bounded(          \
        (buf), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN(out_span, fmt_span, ...)         \
    (::petra::try_format_consteval_span_runtime_n_span(                  \
        (out_span), (fmt_span) __VA_OPT__(,) __VA_ARGS__))
```

Each macro is a verbatim thin dispatcher to ONE of the new
wrappers.  The `_CONSTEVAL_SPAN_` prefix (vs Sep 14's `_SPAN_`
prefix on the RUNTIME surface) makes the trade-off explicit at
the call site: these macros bypass the consteval gate
intentionally.

Each macro uses `__VA_OPT__(,)` to handle the no-args case.  Each
macro is a parenthesized expression — NOT a `do { } while (0)`
block — so the caller can write `auto r = ...;` and capture the
result.  Mirrors Sep 13 / Sep 14's macro shape exactly.

### The full family of TRY_FORMAT_BOUNDED_* macros (Sep 13 / Sep 14 + TODAY)

| Macro                              | Surface        | Underlying wrapper                        |
|------------------------------------|----------------|-------------------------------------------|
| `TRY_FORMAT_BOUNDED`               | CONSTEVAL str  | Sep 7 try_format_bounded                  |
| `TRY_FORMAT_BOUNDED_RUNTIME`       | RUNTIME str    | Sep 6 try_format_runtime_bounded          |
| `TRY_FORMAT_BOUNDED_SPAN`          | RUNTIME str    | Sep 14: span<const char> fmt              |
| `TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED` | RUNTIME str    | TODAY: span<const char> fmt, no consteval |
| `TRY_FORMAT_BOUNDED_N`             | CONSTEVAL chr* | Sep 9 try_format_bounded_runtime_n        |
| `TRY_FORMAT_BOUNDED_RUNTIME_N`     | RUNTIME chr*   | Sep 8 try_format_runtime_n                |
| `TRY_FORMAT_BOUNDED_N_SPAN_FMT`    | RUNTIME chr*   | Sep 14: span<const char> fmt              |
| `TRY_FORMAT_CONSTEVAL_SPAN_N`      | RUNTIME chr*   | TODAY: span<const char> fmt, no consteval |
| `TRY_FORMAT_BOUNDED_INTO`          | CONSTEVAL NUL  | Sep 11 format_to_n_into_bounded           |
| `TRY_FORMAT_BOUNDED_RUNTIME_INTO`  | RUNTIME NUL    | Sep 12 try_format_runtime_n_into_bounded  |
| `TRY_FORMAT_BOUNDED_INTO_SPAN`     | RUNTIME NUL    | Sep 14: span<const char> fmt              |
| `TRY_FORMAT_CONSTEVAL_SPAN_INTO`   | RUNTIME NUL    | TODAY: span<const char> fmt, no consteval |
| `TRY_FORMAT_BOUNDED_N_SPAN`        | CONSTEVAL spn  | Sep 10 try_format_bounded_runtime_n_span  |
| `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN` | RUNTIME spn | Sep 14: span<char>+span<const char> fmt   |
| `TRY_FORMAT_CONSTEVAL_SPAN_N_SPAN` | RUNTIME spn    | TODAY: span<char>+span<const char> fmt, no consteval |

The CONSTEVAL surface's std::format_string<Args...> literal
primary (Sep 7 / Sep 9 / Sep 11) is unchanged — those wrappers
still reject malformed format strings as HARD COMPILE ERRORS at
the call site.

## Important code

### Section 1 — the HEADLINE finding (re-pinned)

```cpp
// 1a. Sep 14's finding (re-pinned): the conversion
//     std::span<const char> -> std::string_view is EXPLICIT on
//     libc++ 21 (P2499R0 range ctor).
static_assert(!std::is_convertible_v<std::span<const char>,
                                     std::string_view>,
              "std::span<const char> must NOT be implicitly convertible "
              "to std::string_view (libc++ 21 marks the range ctor "
              "explicit — P2499R0)");
static_assert(std::is_constructible_v<std::string_view,
                                      std::span<const char>>,
              "std::string_view must be EXPLICITLY constructible "
              "from std::span<const char> (P2499R0 range ctor)");

// 1b. std::span<const char> properties — pinned for posterity.
static_assert(std::ranges::contiguous_range<std::span<const char>>, ...);
static_assert(std::ranges::sized_range<std::span<const char>>, ...);
static_assert(std::is_trivially_copyable_v<std::span<const char>>, ...);
static_assert(std::is_standard_layout_v<std::span<const char>>, ...);
// ... and data()/size() return-type pins.

// 1d. Verify the consteval ctor still ACCEPTS a literal at the
//     call site — pinning that the consteval contract for Sep 7 /
//     Sep 9 / Sep 11 is unchanged.  Use a static constexpr so the
//     address is a constant expression.
static constexpr const char lit_check[9] = {'h','e','l','l','o',' ','{','}','\0'};
std::format_string<char const*> fs_lit{lit_check};  // OK: literal
```

### Section 3 — try_format_consteval_span_bounded

```cpp
// 3a. Happy path: format "hello {}" with arg "world".
std::array<char, 8> hello_fmt{'h','e','l','l','o',' ','{','}'};
std::span<const char> hello_fmt_span{hello_fmt};
auto r3 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, hello_fmt_span, "world");
CHECK(r3.has_value());
CHECK_EQ(r3.value(), std::string{"hello world"});

// 3d. The consteval-shaped runtime parser ACCEPTS a malformed
//     format string at the call site (the trade-off) and
//     returns a RUNTIME FormatError{Format, ...}.
char bad[3] = {'{', ':', ' '};
std::span<const char> bad_span{bad, 3};
auto r6 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, bad_span, 42);
CHECK(!r6.has_value());
CHECK_EQ(r6.error().kind, petra::FormatErrorKind::Format);
```

The Section 3 malformed-format-string test pins the trade-off:
the consteval-shaped runtime parser ACCEPTS a malformed format
string at the call site (the trade-off) and returns a RUNTIME
`FormatError{Format, ...}` — instead of producing a HARD COMPILE
ERROR like the CONSTEVAL surface's std::format_string<Args...>
literal primary would.

### Section 7 — byte-exactness of the explicit conversion

```cpp
// 7a. Build a runtime format string and a span<const char> over
//     a sub-range of it.  Verify the formatted output exactly
//     matches what std::string_view over the same sub-range would
//     produce — proving the conversion does NOT touch the bytes.
std::string fmt7 = ">>> hello {} <<<";
std::span<const char> fmt7_span{
    reinterpret_cast<const char*>(fmt7.data() + 4), 8};
auto r15 = TRY_FORMAT_CONSTEVAL_SPAN_BOUNDED(100, fmt7_span, "world");
CHECK(r15.has_value());
CHECK_EQ(r15.value(), std::string{"hello world"});
```

The span is a sub-range of a larger buffer that intentionally has
NO NUL anywhere — the conversion is purely byte-exact, no NUL
search or insertion happens.

## Observed output

### Default build (clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0)

```
$ /tmp/P-2026-09-15-try-format-consteval-span-format-string
=========================================================
PASS: 159  FAIL: 0
=========================================================
```

### Strict-warning build (+ -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion)

```
$ /tmp/P-2026-09-15-try-format-consteval-span-format-string-strict
=========================================================
PASS: 159  FAIL: 0
=========================================================
```

Five consecutive runs on each build produced identical 159/0
tallies (no flakiness in the 4-thread/100-call concurrent Section
8 — per-thread local counters are aggregated after join to avoid
a data race on g_pass / g_fail).

### ASan/UBSan build (+ -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1)

```
$ /tmp/P-2026-09-15-try-format-consteval-span-format-string-asan
=========================================================
PASS: 159  FAIL: 0
=========================================================
```

Five consecutive runs on the ASan/UBSan build produced identical
159/0 tallies.  Empty stderr under ASan.  Zero sanitizer
diagnostics.

### Probe pair (/tmp, NOT in the working tree)

```
$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_consteval_rejects_span_for_fmt.cpp \
          -o /tmp/probe_consteval_rejects_span_for_fmt
/tmp/probe_consteval_rejects_span_for_fmt.cpp:14:43: error: no
    matching constructor for initialization of
    'std::format_string<const char *>' (aka 'basic_format_string<char, const char *>')
... note: candidate template ignored: constraints not satisfied
    [with _Tp = std::span<const char>]
    because 'convertible_to<const std::span<const char> &,
    basic_string_view<char>>' evaluated to false
    because 'is_convertible_v<const std::span<const char> &,
    std::string_view>' evaluated to false

$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_consteval_rejects_runtime_stringview.cpp \
          -o /tmp/probe_consteval_rejects_runtime_stringview
/tmp/probe_consteval_rejects_runtime_stringview.cpp:14:43: error:
    call to consteval function 'std::basic_format_string<char,
    const char *>::basic_format_string<std::string_view>' is not
    a constant expression
... note: read of non-constexpr variable 'sv' is not allowed in
    a constant expression
... note: in call to 'basic_string_view(sv)'
```

The probe pair confirms the HEADLINE finding: the CONSTEVAL
surface is fundamentally incompatible with `std::span<const char>`
for the format-string parameter, by design.

### Build commands

```
# Default build (debug):
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Wshadow \
        P-2026-09-15-try-format-consteval-span-format-string.cpp \
        -o /tmp/P-2026-09-15-try-format-consteval-span-format-string

# Strict-warning build:
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-15-try-format-consteval-span-format-string.cpp \
        -o /tmp/P-2026-09-15-try-format-consteval-span-format-string-strict

# ASan/UBSan build:
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O1 \
        -Wall -Wextra -Wpedantic -Wshadow \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer \
        P-2026-09-15-try-format-consteval-span-format-string.cpp \
        -o /tmp/P-2026-09-15-try-format-consteval-span-format-string-asan
```

## Verification performed

- Default build runs cleanly: 159/159 PASS, 0 FAIL (5 runs).
- Strict-warning build runs cleanly under
  `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`: 159/159 PASS, 0 FAIL (5 runs).  No
  warnings.
- ASan/UBSan build runs cleanly: 159/159 PASS, 0 FAIL (5
  runs).  No sanitizer diagnostics.  Empty stderr.
- Probe pair
  (`/tmp/probe_consteval_rejects_span_for_fmt.cpp` +
  `/tmp/probe_consteval_rejects_runtime_stringview.cpp`) pins
  the HEADLINE finding: the CONSTEVAL surface is fundamentally
  incompatible with `std::span<const char>` for the format-string
  parameter, by design.  Both probes produce the exact
  diagnostics documented above.
- CMake default build: 159/159 PASS, 0 FAIL.
- CMake strict-warning build: 159/159 PASS, 0 FAIL.
- CMake ASan/UBSan build: 159/159 PASS, 0 FAIL.
- 4-thread/100-call concurrent Section 8 — per-thread local
  counters aggregated after join (no data race on g_pass /
  g_fail).

## Summary

- Topic: pins that the CONSTEVAL surface is fundamentally
  incompatible with `std::span<const char>` for the format-string
  parameter (by design), and adds a CONSTEVAL-SHAPED RUNTIME
  PARSER family of four wrappers + four macros (mirroring Sep
  14's RUNTIME surface, with the trade-off documented in the
  function-name prefix and the macro prefix).
- Created files:
  - `late-may/cpp_practice/try_format_consteval_span_format_string/P-2026-09-15-try-format-consteval-span-format-string.cpp`
  - `late-may/cpp_practice/try_format_consteval_span_format_string/P-2026-09-15-try-format-consteval-span-format-string.md`
  - `late-may/cpp_practice/try_format_consteval_span_format_string/CMakeLists.txt`
- Reused verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 /
  Sep 11 / Sep 12 / Sep 13 / Sep 14:
  petra::FormatError, petra::counting_output_iterator,
  petra::detail::bounded_char_writer,
  petra::detail::try_format_runtime_bounded_impl (Sep 6),
  petra::detail::try_format_runtime_n_impl (Sep 8),
  petra::detail::try_format_to_n_into_bounded_impl (Sep 12),
  petra::try_format_runtime_bounded (Sep 6, std::string_view
  primary).
- New today: four try_format_consteval_span_* wrappers + four
  TRY_FORMAT_CONSTEVAL_SPAN_* macros.  Each is a verbatim thin
  forwarding wrapper / dispatcher.
- HEADLINE finding (probed + pinned): the CONSTEVAL surface is
  fundamentally incompatible with `std::span<const char>` for
  the format-string parameter, by design (Sep 14's note was
  wrong on TWO counts — the consteval ctor has a `requires
  convertible_to<...>` clause that rejects `std::span<const
  char>`, AND even a runtime `std::string_view` cannot satisfy
  the consteval gate).
- Trade-off: today's wrappers bypass the consteval gate
  intentionally.  Malformed format strings are now RUNTIME
  errors (not compile errors).  Documented in the function-name
  prefix (`_consteval_span_`) and the macro prefix
  (`TRY_FORMAT_CONSTEVAL_SPAN_`).
- 159/159 PASS across 8 sections on default + strict-warning +
  ASan/UBSan builds; five consecutive runs on each build
  produced identical 159 PASS, 0 FAIL tallies.
- Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion`; zero sanitizer diagnostics;
  empty stderr under default + ASan.
- Lesson is a pure consumer-side C++23 std::format +
  std::vformat_to + std::string_view + std::span<const char>
  (P0122R7) + std::make_format_args + std::format_string<Args...>
  (P2216R3, consteval ctor) + std::expected (P0323R12) +
  std::println (P2093R14) + std::ranges::contiguous_range
  exercise — reuses Sep 6 / Sep 8 / Sep 12's machinery verbatim,
  no third-party libraries, no library installation, no
  find_package.

## Where we go next

Today's lesson closes Sep 14's "Where we go next" item #3 — and
pinned the HEADLINE finding that the CONSTEVAL surface is
fundamentally incompatible with `std::span<const char>` for the
format-string parameter, by design.  Two items from Sep 14
remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type.  Today's lesson reuses the hand-rolled iterator from
   Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12 /
   Sep 13 / Sep 14 verbatim; when a future libc++ ships the
   standard type, a follow-up lesson migrates all the wrappers
   that use the iterator.  (Today verified the type is NOT yet
   in libc++ 21 — `/tmp/probe_counting_iter.cpp` produces
   "no member named 'counting_output_iterator' in namespace
   'std'".)

2. **`petra::try_format_bounded_runtime_n_into` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 /
   `SYNC_LOG_INFO` from Sep 2: a macro that lifts the
   bounded-buffer vs NUL-terminating sibling choice to a macro
   form.  Sep 10's open item #4.

Today's lesson itself opens a new follow-on item:

3. **A `std::span<const char>` overload family with consteval
   parsing at the call site** is impossible by design (Sep 14's
   note was wrong, today's lesson pins the finding).  The
   alternatives are (a) the RUNTIME surface (Sep 14), and (b) the
   CONSTEVAL-SHAPED RUNTIME PARSER family (today).  A future
   lesson could explore **`std::format_string` with a constexpr
   builder** — i.e., a user-provided compile-time hash of the
   format string that the consteval ctor can verify against a
   precomputed parse — but that's a much larger undertaking
   (essentially re-implementing the format-string parser at
   compile time).

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
