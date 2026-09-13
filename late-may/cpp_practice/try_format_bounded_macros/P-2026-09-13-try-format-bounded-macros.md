# P-2026-09-13 — `try_format_bounded_macros`: a MACRO LAYER that lifts the CONSTEVAL vs RUNTIME sibling choice at the call site for all the bounded-format wrappers

**Topic:** a macro layer — eight `TRY_FORMAT_BOUNDED_*` macros — that
lifts the CONSTEVAL vs RUNTIME sibling choice at the call site for
all the bounded-format wrappers built up across Aug 25 / Sep 6 / Sep
7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12. Closes Sep 12's "Where
we go next" item #2 verbatim:

> `petra::try_format_expected_bounded` macro layer — analogous to
> `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO` from Sep 2: a
> `TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt, args...)` macro that
> lifts the consteval vs runtime sibling choice to a macro form.
> Sep 10's open item #2.

Today lands it. Eight macros, each a verbatim thin dispatcher to one
of the eight wrappers already shipped:

| Macro                                 | Surface     | Underlying wrapper (Sep / name) |
|---------------------------------------|-------------|--------------------------------|
| `TRY_FORMAT_BOUNDED`                  | CONSTEVAL   | Sep 7 `try_format_bounded` |
| `TRY_FORMAT_BOUNDED_RUNTIME`          | RUNTIME     | Sep 6 `try_format_runtime_bounded` |
| `TRY_FORMAT_BOUNDED_N`                | CONSTEVAL   | Sep 9 `try_format_bounded_runtime_n` |
| `TRY_FORMAT_BOUNDED_RUNTIME_N`        | RUNTIME     | Sep 8 `try_format_runtime_n` |
| `TRY_FORMAT_BOUNDED_INTO`             | CONSTEVAL   | Sep 11 `format_to_n_into_bounded` |
| `TRY_FORMAT_BOUNDED_RUNTIME_INTO`     | RUNTIME     | Sep 12 `try_format_runtime_n_into_bounded` |
| `TRY_FORMAT_BOUNDED_N_SPAN`           | CONSTEVAL   | Sep 10 `try_format_bounded_runtime_n_span` |
| `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN`   | RUNTIME     | Sep 12 `try_format_runtime_n_into_bounded` (span overload) |

**How today's lesson relates to the chain:**

| Lesson | output | fmt type | NUL-terminates? |
|--------|--------|----------|-----------------|
| Sep 6 | `std::string` | `std::string_view` | n/a |
| Sep 7 | `std::string` | `std::format_string<Args...>` | n/a |
| Sep 8 | `char*` | `std::string_view` | no |
| Sep 9 | `char*` | `std::format_string<Args...>` | no |
| Sep 10 | `std::span<char>` | `std::format_string<Args...>` | no |
| Sep 11 | `char*` | `std::format_string<Args...>` | yes |
| Sep 12 | `char*` / `std::span<char>` | `std::string_view` | yes |
| **TODAY** | — | — | — |

Today is a thin syntax-sugar surface over the seven wrappers already
shipped. No new impl, no new types, no new error contracts.

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::make_format_args`, `std::format_string<Args...>` (P2216R3),
`std::format_error`, `std::expected<T, E>` (P0323R12),
`std::print` / `std::println` (P2093R14), `std::span<char>`
(P0122R7), `std::ranges::contiguous_range`,
`std::indirectly_writable`, `std::output_iterator`).
Plus `__VA_OPT__` (C++20 / C++23 preprocessor feature).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.3.34.2`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **295 / 295 PASS** across 18 sections on default +
strict-warning + ASan/UBSan + CMake-default + CMake-strict +
CMake-ASan builds. Five consecutive runs on each build produced
identical `295 / 295 PASS, 0 FAIL` tallies. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`. Zero sanitizer diagnostics. Empty stderr under
default + ASan.

---

## Why today

Sep 12's "Where we go next" section (item #2) explicitly named the
lesson today's source file IS:

> `petra::try_format_expected_bounded` macro layer — analogous to
> `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO` from Sep 2: a
> `TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt, args...)` macro that
> lifts the consteval vs runtime sibling choice to a macro form.

Today lands it. The lesson exists to pin four things:

1. **The CONSTEVAL vs RUNTIME sibling distinction is the same
   pattern Sep 1 vs Sep 2 established for logging macros** (LOG_INFO
   vs LOG_INFO_RUNTIME): the CONSTEVAL surface catches malformed
   format strings as HARD COMPILE ERRORS via
   `std::format_string<Args...>`'s consteval ctor; the RUNTIME
   surface accepts `std::string_view` and catches `std::format_error`
   at format time. The MACRO is a dispatch layer the caller can
   pick at the call site without naming the underlying function.
2. **The macro layer covers all FOUR bounded-format axes built up
   across the arc:**
   - **BOUNDED** (`std::string` output): Sep 6 runtime + Sep 7
     consteval.
   - **BOUNDED_N** (`char*`+`cap` output): Sep 8 runtime + Sep 9
     consteval.
   - **BOUNDED_INTO** (`char*`+`cap` NUL-terminating): Sep 12 runtime
     + Sep 11 consteval.
   - Plus the `std::span<char>` overloads for the BOUNDED_N family
     (Sep 10 consteval + Sep 12 runtime).
3. **The macro layer is a VERBATIM thin dispatcher** — no new
   impls, no new types, no new error contracts. Each macro expands
   to EXACTLY ONE existing `petra::try_format_*` call. The lesson
   is a thin syntax-sugar surface over the eight wrappers already
   shipped.
4. **The compile-time / runtime sibling distinction carries through
   the macro layer** — Sep 7's consteval sibling is reachable
   through `TRY_FORMAT_BOUNDED` (literal fmt); Sep 12's runtime
   sibling is reachable through `TRY_FORMAT_BOUNDED_RUNTIME_INTO`
   (runtime-built fmt). Pinned by a `/tmp` probe pair (NOT in the
   working tree).

---

## The headline finding — the macro layer is a verbatim thin dispatcher

The natural first draft of today's macros was a uniform
`do { ... } while (0)` wrapper that lifts the consteval vs runtime
sibling choice at the call site. That ALMOST works, but two
complications arose:

1. **`do { ... } while (0)` cannot be assigned to a variable.** The
   caller wants to write `auto r = TRY_FORMAT_BOUNDED_RUNTIME(...)`
   and capture the result. A `do { ... } while (0)` block is a
   statement, not an expression, and can't appear on the right-hand
   side of an assignment. Solution: today's macros are
   parenthesized expressions (NOT `do { ... } while (0)` blocks).
   The downside is that the macros cannot be used as the body of
   an `if/else` without an extra `{ ... }` around them.

2. **`std::make_format_args` requires lvalues** (libc++ 21 — the
   function takes `_Args&...`). The natural design wraps the args
   in `std::make_format_args(args...)` at the macro expansion site
   — but that means a literal arg like `42` becomes an rvalue and
   the wrapper rejects it. Solution: today's macros forward the
   args DIRECTLY to the underlying wrapper, which takes
   `const Args&...` and calls `std::make_format_args(args...)`
   INSIDE the function body, where the args become lvalues via the
   const-ref parameter binding.

The final macro layer is therefore a parenthesized expression that
forwards `args...` directly to one of the eight wrappers:

```cpp
// CONSTEVAL surface: std::format_string<Args...> fmt.
#define TRY_FORMAT_BOUNDED(cap, fmt, ...) \
    (::petra::try_format_bounded((cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: std::string_view fmt + variadic args.
#define TRY_FORMAT_BOUNDED_RUNTIME(cap, fmt, ...) \
    (::petra::try_format_runtime_bounded((cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
```

`__VA_OPT__(,)` is the C++20 / C++23 preprocessor feature that
provides the `,` only when `__VA_ARGS__` is non-empty. Without
`__VA_OPT__`, a macro like `TRY_FORMAT_BOUNDED_N(buf, cap, "hi")`
would expand to `petra::try_format_bounded_runtime_n(buf, cap,
"hi", )` — a trailing comma and a syntax error.

---

## The second finding — `__VA_OPT__` makes the no-args case work

The CONSTEVAL macros take `(cap, fmt, args...)` where `fmt` MUST be
a string literal at the call site (because
`std::format_string<Args...>`'s ctor is consteval) and `args...` are
the variadic replacement-field args. The macros use `__VA_OPT__` to
handle the no-args case (a format with no replacement fields).

Without `__VA_OPT__`, a macro like `TRY_FORMAT_BOUNDED(cap, "hi")`
would expand to `petra::try_format_bounded(cap, "hi", )` — a
trailing comma and a syntax error. `__VA_OPT__(,)` provides the
comma only when `__VA_ARGS__` is non-empty.

The RUNTIME macros take the same shape — `(cap, fmt, args...)` —
where `fmt` is `std::string_view` and `args...` is variadic. The
macros forward `args...` directly to the underlying wrapper, which
takes `const Args&...` and calls `std::make_format_args(args...)`
INSIDE the function body, where the args become lvalues via the
const-ref parameter binding.

Section 15 verifies that the no-args variants work cleanly across
all eight macros (a format with no replacement fields).

---

## The third finding — the consteval gate fires through the macro layer

The CONSTEVAL macros reject malformed format strings as HARD
COMPILE ERRORS at the call site, point-blank. Pinned by a `/tmp`
probe saved outside the working tree:

**Probe 1** — `/tmp/probe_macro_consteval_rejects_unterminated.cpp`
calls `TRY_FORMAT_BOUNDED(100, "{:", 42)`. The compiler produces a
hard error:

```
error: call to consteval function
       'std::basic_format_string<char, int>::basic_format_string<char[3]>'
       is not a constant expression
note:  non-constexpr function '__throw_format_error' cannot be used
       in a constant expression
note:  __throw_format_error("The replacement field misses a
       terminating '}'")
```

This is the same `__throw_format_error` source line Sep 7 / Sep 9 /
Sep 10 / Sep 11 documented. The consteval ctor rejects the
malformed format string at compile time THROUGH the macro
expansion.

**Probe 2** — `/tmp/probe_macro_runtime_accepts_unterminated.cpp`
calls `TRY_FORMAT_BOUNDED_RUNTIME(100, "hello {")`. The compiler
produces ZERO errors, and the program prints at runtime:

```
PROBE OK: runtime FormatError: The format string terminates at a '{'
```

This confirms that the RUNTIME macro accepts the malformed format
string and (through the underlying Sep 6 wrapper) converts it to a
structured `FormatError{Format, e.what()}` at runtime.

**The contrast is the point**: the CONSTEVAL macros force
correctness at compile time; the RUNTIME macros allow correctness
at runtime. The choice is a trade-off between compile-time safety
and runtime flexibility (dynamic format strings).

Section 13 verifies the runtime format-error contract across all
four RUNTIME macros; Section 17 verifies the consteval-vs-runtime
sibling contrast.

---

## The fourth finding — every macro is a verbatim thin dispatcher

The eight macros each expand to exactly one call to one of the
eight underlying wrappers. No new types, no new impls, no new
error contracts:

```cpp
// CONSTEVAL surface (4 macros):
#define TRY_FORMAT_BOUNDED(cap, fmt, ...) \
    (::petra::try_format_bounded((cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_bounded<Args...>(cap, fmt, args...)
//   → std::expected<std::string, FormatError>  (Sep 7)

#define TRY_FORMAT_BOUNDED_N(buf, cap, fmt, ...) \
    (::petra::try_format_bounded_runtime_n((buf), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_bounded_runtime_n<Args...>(buf, cap, fmt, args...)
//   → std::expected<std::size_t, FormatError>  (Sep 9)

#define TRY_FORMAT_BOUNDED_INTO(buf, cap, fmt, ...) \
    (::petra::format_to_n_into_bounded((buf), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::format_to_n_into_bounded<Args...>(buf, cap, fmt, args...)
//   → std::expected<std::size_t, FormatError>  (Sep 11)

#define TRY_FORMAT_BOUNDED_N_SPAN(span, fmt, ...) \
    (::petra::try_format_bounded_runtime_n_span((span), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_bounded_runtime_n_span<Args...>(span, fmt, args...)
//   → std::expected<std::size_t, FormatError>  (Sep 10)

// RUNTIME surface (4 macros):
#define TRY_FORMAT_BOUNDED_RUNTIME(cap, fmt, ...) \
    (::petra::try_format_runtime_bounded((cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_runtime_bounded<Args...>(cap, fmt, args...)
//   → std::expected<std::string, FormatError>  (Sep 6)

#define TRY_FORMAT_BOUNDED_RUNTIME_N(buf, cap, fmt, ...) \
    (::petra::try_format_runtime_n((buf), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_runtime_n<Args...>(buf, cap, fmt, args...)
//   → std::expected<std::size_t, FormatError>  (Sep 8)

#define TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, cap, fmt, ...) \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_runtime_n_into_bounded<Args...>(buf, cap, fmt, args...)
//   → std::expected<std::size_t, FormatError>  (Sep 12)

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(span, fmt, ...) \
    (::petra::try_format_runtime_n_into_bounded((span), (fmt) __VA_OPT__(,) __VA_ARGS__))
//   → ::petra::try_format_runtime_n_into_bounded<Args...>(span, fmt, args...)
//   → std::expected<std::size_t, FormatError>  (Sep 12 span overload)
```

Section 14 verifies the return type of each macro is exactly the
return type of the underlying wrapper via `decltype` checks at the
call site.

---

## Important code

### The public surface — eight macros

```cpp
// ----- CONSTEVAL surface: std::format_string<Args...> fmt -----------------
#define TRY_FORMAT_BOUNDED(cap, fmt, ...)                                  \
    (::petra::try_format_bounded((cap), (fmt) __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_N(buf, cap, fmt, ...)                           \
    (::petra::try_format_bounded_runtime_n((buf), (cap), (fmt)            \
                                           __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_INTO(buf, cap, fmt, ...)                        \
    (::petra::format_to_n_into_bounded((buf), (cap), (fmt)                \
                                       __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_N_SPAN(span, fmt, ...)                          \
    (::petra::try_format_bounded_runtime_n_span((span), (fmt)             \
                                                __VA_OPT__(,) __VA_ARGS__))

// ----- RUNTIME surface: std::string_view fmt + variadic args -------------
#define TRY_FORMAT_BOUNDED_RUNTIME(cap, fmt, ...)                          \
    (::petra::try_format_runtime_bounded((cap), (fmt) __VA_OPT__(,)        \
                                         __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N(buf, cap, fmt, ...)                   \
    (::petra::try_format_runtime_n((buf), (cap), (fmt) __VA_OPT__(,)       \
                                   __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, cap, fmt, ...)                \
    (::petra::try_format_runtime_n_into_bounded((buf), (cap), (fmt)        \
                                                __VA_OPT__(,) __VA_ARGS__))

#define TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(span, fmt, ...)                  \
    (::petra::try_format_runtime_n_into_bounded((span), (fmt) __VA_OPT__(,)\
                                                __VA_ARGS__))
```

### The underlying wrappers (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12)

Each macro expands to a call to one of these wrappers. No new
machinery:

- `petra::try_format_bounded<Args...>(cap, fmt, args...)` (Sep 7 —
  CONSTEVAL std::string output).
- `petra::try_format_runtime_bounded<Args...>(cap, fmt, args...)`
  (Sep 6 — RUNTIME std::string output).
- `petra::try_format_bounded_runtime_n<Args...>(buf, cap, fmt,
  args...)` (Sep 9 — CONSTEVAL char*+cap, no NUL).
- `petra::try_format_runtime_n<Args...>(buf, cap, fmt, args...)`
  (Sep 8 — RUNTIME char*+cap, no NUL).
- `petra::format_to_n_into_bounded<Args...>(buf, cap, fmt,
  args...)` (Sep 11 — CONSTEVAL char*+cap, NUL-terminating).
- `petra::try_format_runtime_n_into_bounded<Args...>(buf, cap, fmt,
  args...)` (Sep 12 — RUNTIME char*+cap, NUL-terminating).
- `petra::try_format_bounded_runtime_n_span<Args...>(span, fmt,
  args...)` (Sep 10 — CONSTEVAL std::span<char>, no NUL).
- `petra::try_format_runtime_n_into_bounded<Args...>(span, fmt,
  args...)` (Sep 12 — RUNTIME std::span<char>, NUL-terminating).

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 / Sep 12

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- The eight `TRY_FORMAT_BOUNDED_*` macros — each is a VERBATIM
  thin dispatcher to one of the eight wrappers above. No new
  types, no new impls, no new error contracts.

---

## Build and verification commands

Default (`-O0` debug build for this lesson):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-13-try-format-bounded-macros.cpp \
        -o /tmp/P-2026-09-13-try-format-bounded-macros
/tmp/P-2026-09-13-try-format-bounded-macros
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-13-try-format-bounded-macros.cpp \
        -o /tmp/P-2026-09-13-try-format-bounded-macros-strict
/tmp/P-2026-09-13-try-format-bounded-macros-strict
```

ASan + UBSan build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-13-try-format-bounded-macros.cpp \
        -o /tmp/P-2026-09-13-try-format-bounded-macros-asan
/tmp/P-2026-09-13-try-format-bounded-macros-asan
```

CMake (default / strict / ASan) — see `CMakeLists.txt` in this
dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
./build/P-2026-09-13-try-format-bounded-macros
./build-strict/P-2026-09-13-try-format-bounded-macros
./build-asan/P-2026-09-13-try-format-bounded-macros
```

---

## Section-by-section notes

### Section 1 — toolchain + feature probes (12 CHECKs)

Standard toolchain probes: `sizeof(std::size_t) == 8`;
`__cpp_lib_format == 202110`; `__cpp_lib_expected == 202211`;
`__cpp_lib_span == 202002`; `petra::FormatError` is default
constructible and nothrow move constructible;
`petra::FormatErrorKind` values mapped to 0/1/2/3. PLUS
`FormatParse != FormatArgument`, `TooLarge != Format`. All PASS.

### Section 2 — `TRY_FORMAT_BOUNDED_RUNTIME` (RUNTIME std::string surface)

`auto r = TRY_FORMAT_BOUNDED_RUNTIME(64, std::string_view{"v={}"}, 42);`
returns `r.has_value() == true` and `r.value() == "v=42"`. PASS.

### Section 3 — `TRY_FORMAT_BOUNDED` (CONSTEVAL std::string surface)

`auto r = TRY_FORMAT_BOUNDED(64, "v={}", 42);` returns
`r.has_value() == true` and `r.value() == "v=42"`. Same shape as
Sep 7's wrapper. PASS.

### Section 4 — runtime macro accepts dynamic format strings

The KEY use case for the runtime surface: build the format string
dynamically. Sep 7's consteval wrapper would reject this at
compile time (no literal). The runtime wrapper accepts it. Two
sub-cases:

1. `std::string dynamic_fmt = std::string{"v="} + std::to_string(42);`
2. `std::string dynamic_fmt2 = std::string{"hello "} + "world";`

Both compile cleanly (no consteval ctor) and produce the expected
`std::string` output. PASS.

### Section 5 — `TRY_FORMAT_BOUNDED_N` (CONSTEVAL char*+cap surface)

`auto r = TRY_FORMAT_BOUNDED_N(buf, 16, "v={}", 42);` returns
`r.has_value() == true` and `r.value() == 4`; the buffer contains
`"v=42"`. No NUL written (Sep 9's contract). PASS.

### Section 6 — `TRY_FORMAT_BOUNDED_RUNTIME_N` (RUNTIME char*+cap surface)

`auto r = TRY_FORMAT_BOUNDED_RUNTIME_N(buf, 16,
std::string_view{"v={}"}, 42);` returns `r.has_value() == true` and
`r.value() == 4`; the buffer contains `"v=42"`; byte 4 is the
poison byte `'Q'` (no NUL written, Sep 8's contract). PASS.

### Section 7 — `TRY_FORMAT_BOUNDED_INTO` (CONSTEVAL NUL-terminating char*+cap surface)

`auto r = TRY_FORMAT_BOUNDED_INTO(buf, 16, "v={}", 42);` returns
`r.has_value() == true` and `r.value() == 4`; the buffer contains
`"v=42"`; byte 4 is `'\0'` (Sep 11's contract). PASS.

### Section 8 — `TRY_FORMAT_BOUNDED_RUNTIME_INTO` (RUNTIME NUL-terminating char*+cap surface)

`auto r = TRY_FORMAT_BOUNDED_RUNTIME_INTO(buf, 16,
std::string_view{"v={}"}, 42);` returns `r.has_value() == true`
and `r.value() == 4`; the buffer contains `"v=42"`; byte 4 is
`'\0'` (Sep 12's contract). PASS.

### Section 9 — `TRY_FORMAT_BOUNDED_N_SPAN` (CONSTEVAL std::span<char> surface)

`auto r = TRY_FORMAT_BOUNDED_N_SPAN(sp, "v={}", 42);` where `sp` is
a `std::span<char>` over a 16-byte buffer. Returns
`r.has_value() == true` and `r.value() == 4`; the buffer contains
`"v=42"`. PASS.

### Section 10 — `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN` (RUNTIME std::span<char> surface)

`auto r = TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN(sp,
std::string_view{"v={}"}, 42);` returns `r.has_value() == true`
and `r.value() == 4`; the buffer contains `"v=42"`; byte 4 is
`'\0'`. PASS.

### Section 11 — atomicity on TooLarge (4 sub-cases)

Pre-poison the buffer; call the macro with a TooLarge-triggering
format; verify the buffer is still poisoned. Four sub-cases:

1. `TRY_FORMAT_BOUNDED_RUNTIME_N` with `cap=3` and format `"v={}"`
   → `FormatError{TooLarge, needed_bytes=4}`; buffer untouched.
2. `TRY_FORMAT_BOUNDED_N` with `cap=3` and format `"v={}"` →
   `FormatError{TooLarge, needed_bytes=4}`; buffer untouched.
3. `TRY_FORMAT_BOUNDED_INTO` with `cap=3` and format `"v={}"` →
   `FormatError{TooLarge, needed_bytes=4}`; buffer untouched.
4. `TRY_FORMAT_BOUNDED_RUNTIME_INTO` with `cap=3` and format
   `"v={}"` → `FormatError{TooLarge, needed_bytes=4}`; buffer
   untouched.

PASS.

### Section 12 — truncation edge (4 sub-cases)

`cap=5`, format `"ABCDE"` (5 chars). For each macro:

1. `TRY_FORMAT_BOUNDED_INTO`: `r.value() == 4`, `buf[0..3] ==
   "ABCD"`, `buf[4] == '\0'` (truncation edge, NUL at `cap - 1`).
2. `TRY_FORMAT_BOUNDED_RUNTIME_INTO`: same as above.
3. `TRY_FORMAT_BOUNDED_N`: `r.value() == 5`, `buf[0..4] ==
   "ABCDE"`, no NUL written (Sep 9's contract).
4. `TRY_FORMAT_BOUNDED_RUNTIME_N`: same as above.

PASS.

### Section 13 — runtime format-error contract (4 sub-cases)

The headline finding for the runtime surface: a malformed format
string that the consteval macro would catch at compile time is
caught at runtime by the runtime macro, returning
`FormatError{Format, e.what()}`. Four sub-cases:

1. `TRY_FORMAT_BOUNDED_RUNTIME` with format `"hello {"` returns
   `FormatError{Format, "The format string terminates at a '{'"}`.
2. `TRY_FORMAT_BOUNDED_RUNTIME_N` with format `"hello {"` returns
   `FormatError{Format, ...}`.
3. `TRY_FORMAT_BOUNDED_RUNTIME_INTO` with format `"hello {"`
   returns `FormatError{Format, ...}`.
4. `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN` with format `"hello {"`
   returns `FormatError{Format, ...}`.

In every sub-case the buffer is UNTOUCHED on the Format error.
PASS.

### Section 14 — return type verification (8 sub-cases)

Each macro produces the EXACT same return type as the underlying
wrapper. Pinned by `decltype` checks at the call site:

1. `TRY_FORMAT_BOUNDED` → `std::expected<std::string,
   petra::FormatError>` (Sep 7's type).
2. `TRY_FORMAT_BOUNDED_RUNTIME` → `std::expected<std::string,
   petra::FormatError>` (Sep 6's type).
3. `TRY_FORMAT_BOUNDED_N` → `std::expected<std::size_t,
   petra::FormatError>` (Sep 9's type).
4. `TRY_FORMAT_BOUNDED_RUNTIME_N` → `std::expected<std::size_t,
   petra::FormatError>` (Sep 8's type).
5. `TRY_FORMAT_BOUNDED_INTO` → `std::expected<std::size_t,
   petra::FormatError>` (Sep 11's type).
6. `TRY_FORMAT_BOUNDED_RUNTIME_INTO` → `std::expected<std::size_t,
   petra::FormatError>` (Sep 12's type).
7. `TRY_FORMAT_BOUNDED_N_SPAN` → `std::expected<std::size_t,
   petra::FormatError>` (Sep 10's type).
8. `TRY_FORMAT_BOUNDED_RUNTIME_N_SPAN` → `std::expected<std::size_t,
   petra::FormatError>` (Sep 12's span-overload type).

PASS.

### Section 15 — no-args (zero-replacement-field) variants (8 sub-cases)

Each macro accepts a format with no replacement fields. `__VA_OPT__`
provides a clean fallback so the macro expansion produces a valid
one-arg call to the underlying wrapper. Format `"hello world"` (11
bytes). Each macro returns the expected value and the expected byte
content. PASS.

### Section 16 — multi-arg round-trip (3 sub-cases)

The macros transparently forward multi-arg format strings:

1. `TRY_FORMAT_BOUNDED(64, "n={} a={:#x} pi={:.5f}", 42, 42,
   3.14159)` returns `r.value() == "n=42 a=0x2a pi=3.14159"`.
2. `TRY_FORMAT_BOUNDED_RUNTIME(64, std::string_view{"n={} a={:#x}
   pi={:.5f}"}, 42, 42, 3.14159)` returns the same.
3. `TRY_FORMAT_BOUNDED_INTO(buf, 64, "n={} a={:#x} pi={:.5f}", 42,
   42, 3.14159)` returns `r.value() == 22`; `buf[0..21] == "n=42
   a=0x2a pi=3.14159"`; `buf[22] == '\0'`.

PASS.

### Section 17 — consteval-vs-runtime sibling contrast (2 sub-cases)

The runtime sibling accepts the same malformed format strings that
the consteval sibling rejects at compile time:

1. `TRY_FORMAT_BOUNDED_RUNTIME(16, std::string_view{"hello {"})`
   returns `FormatError{Format, "The format string terminates at a
   '{'"}`.
2. `TRY_FORMAT_BOUNDED_RUNTIME(16, std::string_view{":.3f}"}, 42)`
   returns `FormatError{Format, ...}` (the `:` is invalid before a
   `{`).

Both sub-cases verify the runtime side accepts malformed format
strings. The compile-time side is verified in `/tmp` (NOT in the
working tree):

- `/tmp/probe_macro_consteval_rejects_unterminated.cpp` produces a
  hard error: `call to consteval function
  std::basic_format_string<char, int>::basic_format_string<char[3]>
  is not a constant expression` with note `non-constexpr function
  '__throw_format_error' cannot be used in a constant expression`.
- `/tmp/probe_macro_runtime_accepts_unterminated.cpp` COMPILES
  CLEANLY and prints `PROBE OK: runtime FormatError: The format
  string terminates at a '{'`.

PASS.

### Section 18 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)` into
each thread's own `char buf[32]`. Each call's `r.value()` matches
the byte count, each call's `std::string_view{buf, r.value()}`
matches the expected content, and `buf[r.value()] == '\0'` for
every call. Zero mismatches across 100 total calls.

Per-thread result counters are aggregated AFTER the join to avoid
a data race on `g_pass` / `g_fail` from concurrent threads. PASS.

---

## Where we go next

Today's lesson closes Sep 12's "Where we go next" item #2. Three
items from Sep 12 remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type. Sep 10's open item #1. Today's lesson reuses the
   hand-rolled iterator from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep
   10 / Sep 11 / Sep 12 verbatim; when a future libc++ ships the
   standard type, a follow-up lesson migrates all eight wrappers.

2. **`petra::try_format_bounded_runtime_n_into` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a macro that lifts the bounded-buffer vs
   NUL-terminating sibling choice to a macro form. The macro
   could expand to today's `TRY_FORMAT_BOUNDED_RUNTIME_INTO` on
   the NUL-terminating path and `TRY_FORMAT_BOUNDED_RUNTIME_N` on
   the bounded-buffer path. Sep 10's open item #4. Today's lesson
   adds the runtime-format-string surface that this macro could
   dispatch to on the runtime path.

3. **A `std::span<const char>` overload for the format string
   parameter.** Today's macros take `std::string_view` for the
   runtime surface (which already accepts `std::span<const
   char>`-like inputs via implicit conversion). Sep 10's open
   item #5.

Today's lesson itself opens a new follow-on item:

4. **A `TRY_FORMAT_BOUNDED_RUNTIME_VAR` macro** that takes
   variadic args and wraps them in `std::make_format_args(args...)`
   at the macro expansion site — the natural alternative design
   that hit the `std::make_format_args` lvalue constraint and
   forced today's macros to forward `args...` directly to the
   underlying wrapper (where the const-ref parameter binding makes
   them lvalues inside the function body). A future lesson could
   explore a wrapper-side workaround that would let the macro do
   the wrapping.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
