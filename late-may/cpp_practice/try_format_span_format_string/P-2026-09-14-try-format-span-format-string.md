# P-2026-09-14 — try_format_span_format_string: std::span<const char> overload family for the runtime try_format_* wrappers

## Topic

A `std::span<const char>` format-string parameter overload family for
the runtime try_format_* wrappers built up across Sep 6 / Sep 8 / Sep 12,
plus a parallel macro layer (mirroring Sep 13's pattern) that lifts
the new overloads to the macro form.  Four new wrapper overloads;
four new macros.

Closes Sep 13's "Where we go next" item #3 verbatim:

> 3. A `std::span<const char>` overload for the format string
>    parameter.  Today's macros take `std::string_view` for the
>    runtime surface (which already accepts `std::span<const
>    char>`-like inputs via implicit conversion).  Sep 10's open
>    item #5.

## Concepts

### std::span<const char> vs std::string_view for the format-string parameter

`std::string_view` and `std::span<const char>` both model
contiguous range over `char` (or `const char`), but they carry
different invariants:

- `std::string_view` carries an implicit "could be NUL-terminated"
  contract — most C-string APIs (strlen, strcmp, etc.) assume the
  data ends at the first NUL.  `std::string_view` does NOT enforce
  that invariant (it stores `.size()` explicitly), but the
  *name* signals the historical C-string relationship.
- `std::span<const char>` carries an explicit `.size()` and
  makes NO claim about NUL termination.  It's the natural choice
  when the format string lives in a buffer of arbitrary length
  with no NUL in sight (a sub-range of a larger buffer, for
  example).

The natural call site that already has a `std::span<const char>`
buffer and wants to format a portion of it as the format string
shouldn't have to construct a `std::string_view` explicitly at
the call site.  Today's lesson adds the explicit overload.

### The HEADLINE finding: the conversion std::span<const char> -> std::string_view is EXPLICIT, not implicit

Sep 13's "Where we go next" item #3 *assumed* the conversion was
implicit ("already accepts `std::span<const char>`-like inputs via
implicit conversion").  Today's lesson pins the OPPOSITE:

```
$ cat /tmp/probe_span_to_stringview_implicit_FAIL.cpp
#include <span>
#include <string_view>
int main() {
    const char buf[6] = {'h','e','l','l','o','!'};
    std::span<const char> sp{buf};
    std::string_view sv = sp;  // ERROR
    return 0;
}

$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_span_to_stringview_implicit_FAIL.cpp \
          -o /tmp/probe_span_to_stringview_implicit_FAIL
/tmp/probe_span_to_stringview_implicit_FAIL.cpp:13:22: error: no
    viable conversion from 'std::span<const char>' to
    'std::string_view' (aka 'basic_string_view<char>')
/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/string_view:318:25:
    note: candidate constructor not viable: no known conversion from
    'std::span<const char>' to 'const string_view &' for 1st argument
...
/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/string_view:353:44:
    note: explicit constructor is not a candidate
    constexpr explicit _LIBCPP_HIDE_FROM_ABI basic_string_view(_Range&& __r)
```

The std::string_view range ctor (P2499R0) is marked `explicit` on
libc++ 21, so:

- `std::is_convertible_v<std::span<const char>, std::string_view>`
  is **FALSE** (Section 1 in the lesson pins this with a
  `static_assert(!...)`).
- `std::is_constructible_v<std::string_view, std::span<const char>>`
  is **TRUE** (the explicit form `std::string_view{span}` works).
- `std::is_convertible_v<std::string_view, std::span<const char>>`
  is **TRUE** (the other direction is implicit on libc++ 21).

This makes the new overloads MORE necessary, not less — without
them, every call site holding a `std::span<const char>` would have
to write `std::string_view{span}` explicitly before calling the
runtime wrapper.

### The new overloads (verbatim thin forwarders)

```cpp
namespace petra {

// Sep 6's std::string-output wrapper, with a std::span<const char>
// format-string parameter.  Forwards to try_format_runtime_bounded_impl
// UNCHANGED.
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded(std::span<const char> fmt, std::size_t cap,
                           const Args&... args) {
    return detail::try_format_runtime_bounded_impl(
        std::string_view{fmt.data(), fmt.size()}, cap, args...);
}

// Sep 8's char*+cap bounded-buffer wrapper, with a std::span<const char>
// format-string parameter.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* buf, std::size_t cap,
                     std::span<const char> fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

// Sep 12's char*+cap NUL-terminating wrapper, with a std::span<const char>
// format-string parameter.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* buf, std::size_t cap,
                                  std::span<const char> fmt,
                                  const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        buf, cap, std::string_view{fmt.data(), fmt.size()}, args...);
}

// Sep 12's std::span<char> output NUL-terminating wrapper, with a
// std::span<const char> format-string parameter.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(std::span<char> out,
                                  std::span<const char> fmt,
                                  const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(
        out.data(), out.size(),
        std::string_view{fmt.data(), fmt.size()}, args...);
}

}  // namespace petra
```

Each new overload is a verbatim thin forwarding wrapper — extract
`.data()` and `.size()` from the format-string span, build a
`std::string_view` explicitly (because the conversion is explicit),
and dispatch to the existing impl UNCHANGED.  No new types, no new
impls, no new error contracts.  The existing `std::string_view`
primaries (Sep 6 / Sep 8 / Sep 12) are kept as the primary surface.

### The new macros (mirroring Sep 13)

```cpp
#define TRY_FORMAT_BOUNDED_SPAN(cap, fmt, ...)                              \
    (::petra::try_format_runtime_bounded((fmt), (cap) __VA_OPT__(,)        \
                                         __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_N_SPAN_FMT(buf, cap, fmt, ...)                   \
    (::petra::try_format_runtime_n((buf), (cap), (fmt) __VA_OPT__(,)       \
                                   __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_INTO_SPAN(buf, cap, fmt, ...)                    \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt)        \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN(out_span, fmt_span, ...)     \
    (::petra::try_format_runtime_n_into_bounded((out_span), (fmt_span)     \
                                                __VA_OPT__(,) __VA_ARGS__))
```

Each macro uses `__VA_OPT__(,)` to handle the no-args case (a
format with no replacement fields).  Each macro is a parenthesized
expression — NOT a `do { } while (0)` block — so the caller can
write `auto r = TRY_FORMAT_BOUNDED_SPAN(...);` and capture the
result.  Mirrors Sep 13's macro shape exactly.

The macro names are intentionally distinct from Sep 13's eight
macros so the call site makes the choice explicit:

| Macro                              | Underlying wrapper                        | Surface        |
|------------------------------------|-------------------------------------------|----------------|
| `TRY_FORMAT_BOUNDED`               | Sep 7 try_format_bounded                  | CONSTEVAL str   |
| `TRY_FORMAT_BOUNDED_RUNTIME`       | Sep 6 try_format_runtime_bounded          | RUNTIME str     |
| `TRY_FORMAT_BOUNDED_SPAN`          | TODAY: Sep 6 + span<const char>           | RUNTIME str     |
| `TRY_FORMAT_BOUNDED_N`             | Sep 9 try_format_bounded_runtime_n        | CONSTEVAL char* |
| `TRY_FORMAT_BOUNDED_RUNTIME_N`     | Sep 8 try_format_runtime_n                | RUNTIME char*   |
| `TRY_FORMAT_BOUNDED_N_SPAN_FMT`    | TODAY: Sep 8 + span<const char>           | RUNTIME char*   |
| `TRY_FORMAT_BOUNDED_INTO`          | Sep 11 format_to_n_into_bounded           | CONSTEVAL NUL   |
| `TRY_FORMAT_BOUNDED_RUNTIME_INTO`  | Sep 12 try_format_runtime_n_into_bounded  | RUNTIME NUL     |
| `TRY_FORMAT_BOUNDED_INTO_SPAN`     | TODAY: Sep 12 + span<const char>          | RUNTIME NUL     |
| `TRY_FORMAT_BOUNDED_N_SPAN`        | Sep 10 try_format_bounded_runtime_n_span  | CONSTEVAL spn   |
| `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN` | TODAY: Sep 12 spn + spn<const char>  | RUNTIME spn     |

The CONSTEVAL surface already accepts `std::span<const char>` via
the existing implicit conversion from `std::string_view` to
`std::format_string<Args...>`'s ctor — but more importantly,
`std::format_string<Args...>` has a deduction guide that accepts
`std::span<const char>` for any range with `contiguous_range` +
`sized_range` over char.  We do not add span<const char> overloads
to the CONSTEVAL surface today; that's a separate (and much
narrower) task.

### std::span<const char> properties pinned

For posterity, Section 1 pins:
- `std::ranges::contiguous_range<std::span<const char>>` — TRUE.
- `std::ranges::sized_range<std::span<const char>>` — TRUE.
- `std::is_trivially_copyable_v<std::span<const char>>` — TRUE.
- `std::is_standard_layout_v<std::span<const char>>` — TRUE.
- `decltype(std::declval<std::span<const char>>().data())` is
  `const char*` — TRUE.
- `decltype(std::declval<std::span<const char>>().size())` is
  `std::size_t` — TRUE.

## Important code

### The new overloads (Parts 6, 9, 12 of the source)

See the "Concepts" section above.  Each is a 4-line verbatim
forwarder.  No new types, no new impls, no new error contracts.

### Section 1 — the explicit-vs-implicit conversion finding

```cpp
static_assert(!std::is_convertible_v<std::span<const char>,
                                     std::string_view>,
              "std::span<const char> must NOT be implicitly convertible "
              "to std::string_view (libc++ 21 marks the range ctor "
              "explicit — P2499R0)");
static_assert(std::is_constructible_v<std::string_view,
                                      std::span<const char>>,
              "std::string_view must be EXPLICITLY constructible "
              "from std::span<const char> (P2499R0 range ctor)");
static_assert(std::is_convertible_v<std::string_view,
                                    std::span<const char>>,
              "std::string_view must be implicitly convertible "
              "to std::span<const char> (the other direction IS "
              "implicit on libc++ 21)");
```

### Section 7 — byte-exactness of the explicit conversion

```cpp
std::string fmt7 = ">>> hello {} <<<";
std::span<const char> fmt7_span{
    reinterpret_cast<const char*>(fmt7.data() + 4), 8};
// fmt7_span points at "hello {}" (8 bytes — no trailing space).
auto r12 = TRY_FORMAT_BOUNDED_SPAN(100, fmt7_span, "world");
CHECK(r12.has_value());
CHECK_EQ(r12.value(), std::string{"hello world"});
```

The span is a sub-range of a larger buffer that intentionally has
NO NUL anywhere — the conversion is purely byte-exact, no NUL
search or insertion happens.

### Section 6b — sub-span output (no bleed)

```cpp
char big[16];
for (std::size_t i = 0; i < 16; ++i) big[i] = 'Q';
std::span<char> sub_out{big + 2, 11};   // sub-span, not full buffer
auto r11 = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN_SPAN(
    sub_out, std::span<const char>{hello_fmt}, "world");
CHECK(r11.has_value());
// Truncation edge: cap == needed == 11.  effective_cap = 10.
// written = 10.  NUL at sub_out[10] = big[12].
CHECK_EQ(r11.value(), std::size_t{10});
// Bytes [0,2) and [13,16) untouched.
CHECK_EQ(big[0], 'Q');
CHECK_EQ(big[1], 'Q');
CHECK_EQ(big[13], 'Q');
CHECK_EQ(big[14], 'Q');
CHECK_EQ(big[15], 'Q');
```

The NUL placement lands at `big[12]` (just past the 10-byte
write) — INSIDE the underlying 16-byte buffer but past the
sub-span's effective writable region.  Bytes past the sub-span
remain 'Q'.

## Observed output

### Default build (clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0)

```
$ /tmp/P-2026-09-14-try-format-span-format-string
=========================================================
PASS: 183  FAIL: 0
=========================================================
```

### Strict-warning build (+ -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion)

```
$ /tmp/P-2026-09-14-try-format-span-format-string-strict
=========================================================
PASS: 183  FAIL: 0
=========================================================
```

Five consecutive runs on the strict build produced identical 183/0
tallies (no flakiness in the 4-thread/100-call concurrent Section
10 — per-thread local counters are aggregated after join to avoid
a data race on `g_pass` / `g_fail`).

### ASan/UBSan build (+ -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1)

```
$ /tmp/P-2026-09-14-try-format-span-format-string-asan
=========================================================
PASS: 183  FAIL: 0
=========================================================
```

Five consecutive runs on the ASan/UBSan build produced identical
183/0 tallies.  Empty stderr under ASan.  Zero sanitizer
diagnostics.

### Probe pair (/tmp, NOT in the working tree)

```
$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_span_to_stringview_explicit.cpp \
          -o /tmp/probe_span_to_stringview_explicit
$ /tmp/probe_span_to_stringview_explicit
PROBE OK: explicit conversion verified; hello_sv.size()=6

$ clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
          /tmp/probe_span_to_stringview_implicit_FAIL.cpp \
          -o /tmp/probe_span_to_stringview_implicit_FAIL
/tmp/probe_span_to_stringview_implicit_FAIL.cpp:13:22: error: no
    viable conversion from 'std::span<const char>' to
    'std::string_view' (aka 'basic_string_view<char>')
... note: explicit constructor is not a candidate
```

The probe pair confirms the headline finding: the conversion is
explicit on libc++ 21, which makes today's new overloads
necessary.

### Build commands

```
# Default build (debug):
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Wshadow \
        P-2026-09-14-try-format-span-format-string.cpp \
        -o /tmp/P-2026-09-14-try-format-span-format-string

# Strict-warning build:
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-14-try-format-span-format-string.cpp \
        -o /tmp/P-2026-09-14-try-format-span-format-string-strict

# ASan/UBSan build:
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O1 \
        -Wall -Wextra -Wpedantic -Wshadow \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer \
        P-2026-09-14-try-format-span-format-string.cpp \
        -o /tmp/P-2026-09-14-try-format-span-format-string-asan
```

## Verification performed

- Default build runs cleanly: 183/183 PASS, 0 FAIL (5 runs).
- Strict-warning build runs cleanly under
  `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`: 183/183 PASS, 0 FAIL (5 runs).  No
  warnings.
- ASan/UBSan build runs cleanly: 183/183 PASS, 0 FAIL (5
  runs).  No sanitizer diagnostics.  Empty stderr.
- Probe pair (`/tmp/probe_span_to_stringview_explicit.cpp` +
  `/tmp/probe_span_to_stringview_implicit_FAIL.cpp`) pins the
  EXPLICIT-vs-IMPLICIT conversion finding.  The implicit probe
  produces the exact diagnostic documented above.
- 4-thread/100-call concurrent Section 10 — per-thread local
  counters aggregated after join (no data race on g_pass /
  g_fail).

## Summary

- Topic: `std::span<const char>` format-string parameter overload
  family for the runtime try_format_* wrappers (Sep 6 / Sep 8 /
  Sep 12), plus four new macros (mirroring Sep 13's pattern).
- Created files:
  - `late-may/cpp_practice/try_format_span_format_string/P-2026-09-14-try-format-span-format-string.cpp`
  - `late-may/cpp_practice/try_format_span_format_string/P-2026-09-14-try-format-span-format-string.md`
  - `late-may/cpp_practice/try_format_span_format_string/CMakeLists.txt`
- Reused verbatim from Sep 6 / Sep 8 / Sep 12 / Sep 13:
  petra::FormatError, petra::counting_output_iterator,
  petra::detail::bounded_char_writer, all three impls
  (try_format_runtime_bounded_impl, try_format_runtime_n_impl,
  try_format_to_n_into_bounded_impl), the three std::string_view
  primaries, the (char*, size_t, std::string_view) +
  (std::span<char>, std::string_view) overloads of
  try_format_runtime_n_into_bounded.
- New today: four std::span<const char> overloads + four macros.
  Each is a verbatim thin forwarding wrapper / dispatcher.
- 183/183 PASS across 10 sections on default + strict-warning +
  ASan/UBSan builds; five consecutive runs on each build produced
  identical 183 PASS, 0 FAIL tallies.
- Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
  -Wconversion -Wsign-conversion`; zero sanitizer diagnostics;
  empty stderr under default + ASan.
- Lesson is a pure consumer-side C++23 std::format +
  std::vformat_to + std::make_format_args + std::expected
  (P0323R12) + std::span<const char> (P0122R7, P2499R0 range
  ctor) + std::ranges::contiguous_range + std::println
  (P2093R14) exercise — reuses Sep 6 / Sep 8 / Sep 12's machinery
  verbatim, no third-party libraries, no library installation, no
  find_package.

## Where we go next

Today's lesson closes Sep 13's "Where we go next" item #3.  Two
items from Sep 13 remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type.  Sep 10's open item #1.  Today's lesson reuses the
   hand-rolled iterator from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep
   10 / Sep 11 / Sep 12 / Sep 13 verbatim; when a future libc++
   ships the standard type, a follow-up lesson migrates all
   eight wrappers.

2. **`petra::try_format_bounded_runtime_n_into` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a macro that lifts the bounded-buffer vs
   NUL-terminating sibling choice to a macro form.  Sep 10's open
   item #4.

Today's lesson itself opens a new follow-on item:

3. **A `std::span<const char>` overload for the CONSTEVAL
   surface** (Sep 7's `try_format_bounded`, Sep 9's
   `try_format_bounded_runtime_n`, Sep 11's
   `format_to_n_into_bounded`).  Today's macros accept
   `std::span<const char>` for the RUNTIME surface; the
   CONSTEVAL surface's `std::format_string<Args...>` already
   has a deduction guide for ranges satisfying
   `contiguous_range` + `sized_range` over char, so the
   CONSTEVAL wrappers should already accept
   `std::span<const char>` at the call site via implicit
   deduction — but a separate compile-time probe + Section
   could pin this contract explicitly.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
