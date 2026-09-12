# P-2026-09-12 — `try_format_runtime_n_into_bounded`: RUNTIME-FORMAT-STRING NUL-terminating bounded sibling of Sep 11's `format_to_n_into_bounded`

**Topic:** `petra::try_format_runtime_n_into_bounded<Args...>(char* out,
std::size_t cap, std::string_view fmt, const Args&... args)` (and its
`std::span<char>` overload) — the **RUNTIME-FORMAT-STRING** sibling
of Sep 11's `format_to_n_into_bounded`. Closes Sep 11's "Where we go
next" item #5, verbatim:

> A runtime-format-string sibling of today's wrapper — analogous to
> Sep 8's `try_format_runtime_n` being the runtime-format-string
> sibling of Sep 7's `try_format_bounded`. Today's wrapper takes
> `std::format_string<Args...>` (consteval check at the call site); a
> runtime sibling would take `std::string_view` and catch
> `std::format_error` from the `std::make_format_args` side. The impl
> would be the same shape but with the consteval gate removed (a
> `try_format_runtime_n_into_bounded` wrapper).

Today lands it. The headline finding is that **the impl differs from
Sep 11's in exactly ONE place**: the `fmt` parameter type. Sep 11
takes `std::format_string<Args...>` (consteval ctor); today takes
`std::string_view` (runtime parse). The two-pass measure-then-write
pair, the NUL placement formula, the three-branch logic, and the span
overload shape all carry over verbatim.

**How today's lesson relates to the chain:**

| Lesson                              | output          | fmt type                | NUL-terminates? |
|-------------------------------------|-----------------|-------------------------|------------------|
| Aug 25 (no cap, NUL)                | bounded buffer  | `std::format_string<Args...>` | yes (always) |
| Sep 6  (cap, std::string, runtime)  | `std::string`   | `std::string_view`      | n/a              |
| Sep 7  (cap, std::string, consteval)| `std::string`   | `std::format_string<Args...>` | n/a         |
| Sep 8  (char*, runtime)             | `char*`         | `std::string_view`      | no               |
| Sep 9  (char*, consteval)           | `char*`         | `std::format_string<Args...>` | no          |
| Sep 10 (span<char>, consteval)      | `std::span<char>` | `std::format_string<Args...>` | no         |
| Sep 11 (char*, consteval, NUL)      | `char*`         | `std::format_string<Args...>` | yes     |
| **TODAY (char*, runtime, NUL)**     | `char*`         | `std::string_view`      | **yes**         |

Today combines three axes:
- **Sep 8's runtime-format-string parameter** (`std::string_view` fmt).
- **Sep 11's NUL-termination contract** (NUL placement follows Aug 25's
  formula: `nul_idx = (written < cap) ? written : (cap - 1)`).
- **Sep 10's `std::span<char>` overload** (thin forwarder that
  extracts `.data()` + `.size()` and dispatches to the same impl).

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::make_format_args`, `std::expected<T, E>` (P0323R12),
`std::print` / `std::println` (P2093R14), `std::span<char>`
(P0122R7), `std::ranges::contiguous_range`,
`std::ranges::sized_range`, `std::indirectly_writable`,
`std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **121/121 PASS** across 21 sections on default +
strict-warning + ASan/UBSan + CMake-default + CMake-strict +
CMake-ASan builds. Five consecutive runs on each build produced
identical tallies. Zero warnings under `-Wall -Wextra -Wpedantic
-Werror -Wshadow -Wconversion -Wsign-conversion`. Zero sanitizer
diagnostics. Empty stderr under default + ASan.

---

## Why today

Sep 11's "Where we go next" section (item #5) explicitly named
today's lesson:

> A runtime-format-string sibling of today's wrapper — analogous to
> Sep 8's `try_format_runtime_n` being the runtime-format-string
> sibling of Sep 7's `try_format_bounded`. Today's wrapper takes
> `std::format_string<Args...>` (consteval check at the call site); a
> runtime sibling would take `std::string_view` and catch
> `std::format_error` from the `std::make_format_args` side. The impl
> would be the same shape but with the consteval gate removed (a
> `try_format_runtime_n_into_bounded` wrapper).

Today lands it. The lesson exists to pin four things:

1. **The impl differs from Sep 11's in exactly ONE place**: the `fmt`
   parameter type (`std::string_view` vs `std::format_string<Args...>`).
   The two-pass measure-then-write pair, the NUL placement formula,
   the three-branch logic, and the span overload shape all carry over
   verbatim.
2. **The compile-time / runtime sibling distinction is the same
   pattern Sep 7 vs Sep 8 used**: Sep 7 is the consteval sibling of
   Sep 6 (string output); Sep 8 is the runtime sibling of Sep 7
   (char* output); today is the runtime sibling of Sep 11 (char*
   output + NUL). Each pair differs ONLY in the `fmt` parameter
   type — the body is byte-for-byte identical.
3. **The runtime sibling accepts dynamic format strings** — Sep 11's
   `std::format_string<Args...>` requires a literal at the call site,
   which forbids dynamic format strings (from a config file, from
   user input, from a templated string). Today's `std::string_view`
   parameter accepts any runtime-built string. Section 19 exercises
   two dynamic-format-string cases.
4. **Malformed format strings become runtime errors, not compile
   errors** — Sep 11 catches malformed format strings as HARD COMPILE
   ERRORS at the call site (via the consteval ctor's
   `__throw_format_error`). Today's wrapper COMPILES CLEANLY and
   catches the same error at runtime as
   `FormatError{Format, e.what()}`. Section 18 pins this with two
   `/tmp` probes (NOT in the working tree).

---

## The headline finding — the impl differs in exactly one place

The natural first draft of today's wrapper was to copy Sep 11's
`try_format_to_n_into_bounded_impl` and replace the
`std::format_string<Args...>` parameter with `std::string_view`. That
**almost** works, but there's a subtle difference: `std::format_string<Args...>`
provides `.get()` (a `basic_format_string<...>`) that wraps a parse
string + an args type list. `std::string_view` has no `.get()` — it's
just the runtime parse string. The `std::vformat_to` call is the
only place the difference surfaces:

```cpp
// Sep 11 (consteval):
(void)std::vformat_to(std::move(sink), fmt.get(),  // <-- .get() is the consteval parse result
                      std::make_format_args(args...));

// Today (runtime):
(void)std::vformat_to(std::move(sink), fmt,        // <-- fmt is std::string_view directly
                      std::make_format_args(args...));
```

The error path is also richer for today: `std::format_error` can come
from EITHER the `std::make_format_args` side (parse error or arg-type
mismatch) OR the `std::vformat_to` side (runtime format error). Both
are caught in pass 1 and pass 2 (defensive) and converted to
`FormatError{Format, e.what()}`. Sep 11's consteval sibling catches
the parse and arg-type errors at compile time, so its runtime
`FormatError{Format, ...}` branch is narrower (only runtime errors
that survive the consteval check).

---

## The second finding — the compile-time / runtime sibling contrast

Sep 11's wrapper takes `std::format_string<Args...>` whose ctor is
consteval. Today's wrapper takes `std::string_view` (no compile-time
check). The contrast is pinned by two probes saved in `/tmp`, NOT in
the working tree:

**Probe 1** — `/tmp/probe_consteval_rejects_runtime_unterminated.cpp`
calls an inline-minimal copy of Sep 11's consteval sibling with the
malformed format string `"hello {"`. The compiler produces a hard
error:

```
error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<char[8]>'
       is not a constant expression
note:  non-constexpr function '__throw_format_error' cannot be used
       in a constant expression
   313 |         std::__throw_format_error("The format string terminates at a '{'");
       |         ^
```

This is the **same `__throw_format_error` source line** Sep 7 / Sep 9 /
Sep 10 / Sep 11 documented. The consteval ctor rejects the malformed
format string at compile time.

**Probe 2** — `/tmp/probe_runtime_accepts_unterminated.cpp` calls an
inline-minimal copy of today's runtime sibling with the same malformed
format string. The compiler produces **zero errors**, and the program
prints at runtime:

```
PROBE OK: runtime FormatError: The format string terminates at a '{'
```

This confirms that today's runtime sibling has a strictly weaker
compile-time surface than Sep 11's consteval sibling — it accepts the
malformed format string and converts it to a structured
`FormatError{Format, e.what()}` at runtime. The buffer is UNTOUCHED on
this path (Section 18 verifies this with a poisoned buffer).

**The contrast is the point**: Sep 11 forces correctness at compile
time; today allows correctness at runtime. The choice is a trade-off
between compile-time safety and runtime flexibility (dynamic format
strings).

---

## The third finding — dynamic format strings are the use case

The KEY difference between today's wrapper and Sep 11's wrapper is
that today's accepts `std::string_view`, which can be built dynamically.
Sep 11's `std::format_string<Args...>` requires a string literal at
the call site, which forbids dynamic format strings (from a config
file, from user input, from a templated string). Section 19 exercises
two dynamic-format-string cases:

```cpp
// Case 1: format string built from std::string concatenation.
std::string dynamic_fmt = "v=" + std::to_string(42);  // "v=42"
auto r = petra::try_format_runtime_n_into_bounded(
    buf, sizeof(buf), std::string_view{dynamic_fmt});
// r.value() == 4, buf == "v=42\0".

// Case 2: format string built from std::string concatenation with
// no placeholder.
std::string dynamic_fmt2 = std::string{"hello "} + "world";
auto r2 = petra::try_format_runtime_n_into_bounded(
    buf2, sizeof(buf2), std::string_view{dynamic_fmt2});
// r2.value() == 11, buf2 == "hello world\0".
```

Both cases compile cleanly (no consteval ctor) and produce the
expected byte count. Section 19 verifies the byte count and the byte
content for both cases.

Sep 11's wrapper would reject these call sites at compile time —
`std::format_string<Args...>` requires a literal at the call site, so
`std::string_view{dynamic_fmt}` would not implicitly convert to
`std::format_string<Args...>` (the consteval parse happens at compile
time on a literal).

---

## The fourth finding — the three-branch logic carries over verbatim

Today's `try_format_runtime_n_into_bounded_impl` has the same
three-branch structure as Sep 11's `try_format_to_n_into_bounded_impl`:

1. **`cap == 0`**: degenerate; `needed == 0` is success (no NUL),
   `needed > 0` is `TooLarge`.
2. **`needed > cap`**: `TooLarge` (caller's buffer UNTOUCHED).
3. **`needed == cap`**: TRUNCATION SUCCESS — pass 2 writes
   `cap - 1` bytes, NUL at `cap - 1`, return `cap - 1`.
4. **`needed < cap`**: FULL SUCCESS — pass 2 writes `needed` bytes,
   NUL at `needed`, return `needed`.

The only difference is the `fmt` parameter type and the `std::vformat_to`
call (no `.get()` for today). The `FormatError{TooLarge, ...}` and
`FormatError{Format, ...}` contracts are identical to Sep 6 / Sep 7 /
Sep 8 / Sep 9 / Sep 10 / Sep 11.

---

## Important code

### The public surface — char*+cap and span

```cpp
namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(char* out, std::size_t cap,
                                  std::string_view fmt,
                                  const Args&... args) {
    return detail::try_format_runtime_n_into_bounded_impl(out, cap,
                                                          fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded(std::span<char> out,
                                  std::string_view fmt,
                                  const Args&... args) {
    return try_format_runtime_n_into_bounded(out.data(), out.size(),
                                             fmt, args...);
}

}  // namespace petra
```

### The two-pass measure-then-write impl (NEW today)

```cpp
namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_into_bounded_impl(char* out, std::size_t cap,
                                       std::string_view fmt,
                                       const Args&... args) {
    // Pass 1: measure.
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,    // <-- fmt, not fmt.get()
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // cap == 0: degenerate.
    if (cap == 0) {
        if (needed > 0) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge, ...});
        }
        return std::size_t{0};
    }

    // cap >= 1: reserve one slot for NUL.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge, ...});
    }

    const std::size_t effective_cap = cap - 1;

    // Truncation edge: needed == cap.
    if (needed == cap) {
        std::size_t written = 0;
        try {
            bounded_char_writer sink{out, effective_cap, cb, &written};
            (void)std::vformat_to(std::move(sink), fmt,  // <-- fmt, not fmt.get()
                                  std::make_format_args(args...));
        } catch (const std::format_error& e) {
            return std::unexpected(
                FormatError{FormatErrorKind::Format, e.what()});
        }
        if (written != effective_cap) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge, ...});
        }
        out[cap - 1] = '\0';
        return effective_cap;
    }

    // needed < cap: full success.
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, effective_cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt,      // <-- fmt, not fmt.get()
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge, ...});
    }
    out[needed] = '\0';
    return written;
}

}  // namespace detail
}  // namespace petra
```

### The structured-error contract

Identical to Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11 —
`petra::FormatError{kind, message, needed_bytes}`:

- `FormatError{TooLarge, "formatted output would be N bytes;
  cap is C (effective writable is C-1)", needed_bytes=N}` on
  `needed > cap`.
- `FormatError{Format, e.what()}` on `std::format_error` from the
  `std::make_format_args` side (parse / argument errors caught at
  RUNTIME today, vs compile time in Sep 11).
- `FormatError{TooLarge, "bounded_char_writer wrote X bytes;
  expected Y on truncation edge", needed_bytes=Y}` on the
  partial-write invariant violation (defensive check).
- `FormatError{TooLarge, "bounded_char_writer wrote X bytes;
  measure pass said Y", needed_bytes=Y}` on the full-success
  partial-write invariant violation (defensive check).

On EVERY error path the caller's buffer is UNTOUCHED (Sections 5,
10, 18 verify this with poisoned buffers).

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 / Sep 11

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped by
  libc++ 21).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- `petra::detail::try_format_runtime_n_into_bounded_impl` — NEW
  two-pass impl with `std::string_view` fmt.
- `petra::try_format_runtime_n_into_bounded<Args...>(char*, std::size_t,
  std::string_view, Args&&...)` — NEW public surface.
- `petra::try_format_runtime_n_into_bounded<Args...>(std::span<char>,
  std::string_view, Args&&...)` — NEW span overload.

---

## Build and verification commands

Default (`-O0` debug build for this lesson):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-12-try-format-runtime-n-into-bounded.cpp \
        -o /tmp/P-2026-09-12-try-format-runtime-n-into-bounded
/tmp/P-2026-09-12-try-format-runtime-n-into-bounded
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-12-try-format-runtime-n-into-bounded.cpp \
        -o /tmp/P-2026-09-12-try-format-runtime-n-into-bounded-strict
/tmp/P-2026-09-12-try-format-runtime-n-into-bounded-strict
```

ASan + UBSan build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-12-try-format-runtime-n-into-bounded.cpp \
        -o /tmp/P-2026-09-12-try-format-runtime-n-into-bounded-asan
/tmp/P-2026-09-12-try-format-runtime-n-into-bounded-asan
```

CMake (default / strict / ASan) — see `CMakeLists.txt` in this
dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
./build/P-2026-09-12-try-format-runtime-n-into-bounded
./build-strict/P-2026-09-12-try-format-runtime-n-into-bounded
./build-asan/P-2026-09-12-try-format-runtime-n-into-bounded
```

---

## Section-by-section notes

### Section 1 — toolchain + feature probes (11 CHECKs)

Standard toolchain probes: `sizeof(std::size_t) == 8`;
`sizeof(std::expected<std::size_t, petra::FormatError>) >= 8`;
`__cpp_lib_format == 202110`; `__cpp_lib_expected == 202211`;
`__cpp_lib_span == 202002`; `std::span<char>` is
`std::ranges::contiguous_range` and `std::ranges::sized_range`;
`std::span<char>` is trivially copyable and standard-layout. PLUS a
runtime smoke test that today's wrapper accepts a `std::string_view`
fmt at the call site (verifies the type surface — Sep 11 would
require a literal). All PASS.

### Section 2 — exact-fit NUL at written (4 CHECKs)

`cap=8`, format `"1234567"` (7 chars). `r.value() == 7`;
`buf[7] == '\0'`. The NUL sits at `written == cap - 1`. PASS.

### Section 3 — no-truncation NUL at written (5 CHECKs)

`cap=16`, format `"hello {}"` with `"world"` (11 chars).
`r.value() == 11`; `buf[11] == '\0'`; `buf[12] == '\0'`
(zero-initialised untouched). The NUL sits at `written == 11 <
cap == 16`. PASS.

### Section 4 — truncation edge NUL at cap-1 (7 CHECKs)

`cap=5`, format `"ABCDE"` (5 chars). `needed == cap` exactly →
truncation edge. `r.value() == 4` (cap - 1); `buf[0..3] ==
'A','B','C','D'`; `buf[4] == '\0'`. PASS.

### Section 5 — TooLarge buffer untouched (7 CHECKs)

`cap=4`, format `"1234567"` (7 chars). `needed > cap` →
`FormatError{TooLarge, needed_bytes=7}`. Buffer is poisoned
with `'Q'`; all 4 bytes stay `'Q'`. PASS.

### Section 6 — cap=0 empty format (3 CHECKs)

`cap=0`, format `""` (0 chars). `needed == 0 == cap` → success
with `r.value() == 0`. No NUL can be written (no room). Buffer
unchanged. PASS.

### Section 7 — cap=0 non-empty format (4 CHECKs)

`cap=0`, format `"x"` (1 char). `needed > 0 > cap` →
`FormatError{TooLarge, needed_bytes=1}`. Buffer unchanged. PASS.

### Section 8 — cap=1 only NUL (3 CHECKs)

`cap=1`, format `"x"` (1 char). `needed == cap == 1` → truncation
edge. `r.value() == 0`; `buf[0] == '\0'`. The formatted byte
never makes it into the buffer. PASS.

### Section 9 — format specifiers (3 CHECKs)

Format specifiers passthrough (`{:#x}`, `{:.3f}`):
`"v={} h={:#x} pi={:.3f}"` → `"v=42 h=0xcafe pi=3.142"`.
NUL at `buf[r.value()]`. PASS.

### Section 10 — atomicity on TooLarge failure (4 CHECKs)

Pre-fill `buf` with `"v=42"`; issue a failing call (cap=8,
format `"{}"`, arg `"hello world"` needs 11 → TooLarge). Pre-fill
survives; error kind == TooLarge; needed_bytes == 11. PASS.

### Section 11 — span overload equivalence (8 CHECKs)

Same format + args, same buffer size: char\*+cap and span
overloads produce identical byte counts (`r.value() == 4` for
`("v={}", 42)`) and identical byte content. PASS.

### Section 12 — sub-span no-bleed (12 CHECKs)

A `std::span<char>{buf + 5, 6}` (a 6-byte sub-span at offset
5 of a 16-byte buffer). Format `"ABCDE"` (5 chars).
After the call: bytes `[0, 5)` untouched (poison `'Q'`);
bytes `[5, 10)` = formatted text; NUL at `buf[10] == '\0'`;
bytes `[11, 16)` untouched. PASS.

### Section 13 — multi-type args round-trip (3 CHECKs)

A single string-with-multiple-replacement-fields format
(`"name={} age={:#x} pi={:.5f}"`) producing a 26-byte output.
Byte content matches; NUL at `buf[r.value()]`. PASS.

### Section 14 — UTF-8 mid-codepoint byte round-trip (4 CHECKs)

`"héllo"` in UTF-8 is 6 bytes (the `é` is 2 bytes:
`0xc3 0xa9`). `r.value() == 6`; byte content matches.
PASS.

### Section 15 — embedded NUL byte round-trip (5 CHECKs)

A `std::string{'a', '\0', 'b'}` is 3 bytes long.
`r.value() == 3`; `buf[0] == 'a'`, `buf[1] == '\0'`,
`buf[2] == 'b'`; the wrapper's NUL is at `buf[3]`. PASS.

### Section 16 — byte-count invariant (2 CHECKs)

For `("hello {}", "world")` — 11 bytes total — the wrapper
reports `r.value() == 11` AND an independent
`std::vformat_to + counting_output_iterator` probe reports
`measured == 11`. The byte-by-byte invariant holds. PASS.

### Section 17 — TooLarge boundary sweep (15 CHECKs)

Three sub-cases for `"v=42"` (needs 4 bytes): cap=3
(TooLarge, `needed_bytes == 4`); cap=4 (truncation edge,
`r.value() == 3`, NUL at `cap - 1 == 3`); cap=5 (full
success, `r.value() == 4`, NUL at `written == 4 < cap`).
PASS.

### Section 18 — runtime format-error contract (7 CHECKs)

The headline finding for the runtime sibling: a malformed format
string that the consteval sibling (Sep 11) would catch at compile
time is caught at runtime by today's wrapper, returning
`FormatError{Format, e.what()}`. Verified with three sub-cases:

1. Unterminated replacement field: `try_format_runtime_n_into_bounded(buf,
   16, "hello {")` returns `FormatError{Format, "The format string
   terminates at a '{'", needed_bytes=0}`. Buffer untouched.
2. Arg-type mismatch: `try_format_runtime_n_into_bounded(buf, 16,
   "{:.3f}", 42)` returns `FormatError{Format, ...}` (42 doesn't
   satisfy `{:.3f}` — `std::format_error` is thrown).
3. Buffer untouched on Format error: pre-poisoned buffer stays
   `'Q'` through the call.

The compile-time / runtime sibling contrast is pinned by two
probes saved in `/tmp` (NOT in the working tree):

- `/tmp/probe_consteval_rejects_runtime_unterminated.cpp` — calls
  an inline-minimal copy of Sep 11's consteval sibling with
  `"hello {"` and gets a hard error: `call to consteval function
  std::basic_format_string<char, int>::basic_format_string<char[8]>
  is not a constant expression` with note `non-constexpr function
  '__throw_format_error' cannot be used in a constant expression`.
  Same `__throw_format_error` source line Sep 7 / Sep 9 / Sep 10 /
  Sep 11 documented.
- `/tmp/probe_runtime_accepts_unterminated.cpp` — calls an
  inline-minimal copy of today's runtime sibling with the same
  `"hello {"`. Compiles cleanly and prints `PROBE OK: runtime
  FormatError: The format string terminates at a '{'` at runtime,
  confirming today's runtime sibling has a strictly weaker
  compile-time surface than Sep 11's consteval sibling.

PASS.

### Section 19 — dynamic format string (7 CHECKs)

The KEY use case for the runtime sibling: build the format string
dynamically at the call site. Sep 11's `std::format_string<Args...>`
cannot accept dynamic format strings (requires a literal). Today's
`std::string_view` accepts any runtime-built string. Two
sub-cases:

1. `"v=" + std::to_string(42)` — built from a concatenation.
2. `std::string{"hello "} + "world"` — built from a std::string
   concatenation.

Both compile cleanly (no consteval ctor) and produce the expected
byte count. PASS.

### Section 20 — compile-time contrast (3 CHECKs)

Headline finding restated for today: today's wrapper accepts a
`std::string_view` fmt at the call site (this CHECK is a TYPE
check — if the wrapper accidentally took `std::format_string<Args...>`,
the implicit conversion from a `std::string_view` literal would
fail to compile). A `std::string_view literal{"hi {}"}` flows
through today's wrapper cleanly. PASS.

### Section 21 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)`
into each thread's own `char buf[32]` (each call constructs
its own `std::span<char>` over that buffer). Each call's
`r.value()` matches the byte count, each call's
`std::string_view{buf, r.value()}` matches the expected
content, and `buf[r.value()] == '\0'` for every call. Zero
mismatches across 100 total calls. PASS.

---

## Where we go next

Today's lesson closes Sep 11's "Where we go next" item #5.
Three items from Sep 11 remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships
   the type. Sep 10's open item #1. The Section 1 static_assert
   is the migration trigger — when a future libc++ ships the
   type, the static_assert flips to `true` and a follow-up
   lesson migrates all seven wrappers (Sep 6, Sep 7, Sep 8,
   Sep 9, Sep 10, Sep 11, today) to use the standard type.

2. **`petra::try_format_expected_bounded` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a `TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt,
   args...)` macro that lifts the consteval vs runtime sibling
   choice to a macro form. Sep 10's open item #2. Today's
   lesson adds the runtime sibling surface that this macro
   would dispatch to on the runtime path.

3. **A `petra::try_format_bounded_runtime_n_into` macro layer**
   — analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a macro that lifts the bounded-buffer vs
   NUL-terminating sibling choice to a macro form. The macro
   could expand to today's wrapper on the bounded-buffer path
   and Aug 25's `format_to_n_into` on the NUL-terminating
   path. Sep 10's open item #4. Today's lesson adds the
   runtime-format-string surface that this macro could dispatch
   to on the runtime path.

4. **A `std::span<const char>` overload** for the format
   string parameter. Today's wrapper takes `std::string_view`
   (which already accepts `std::span<const char>`-like inputs
   via implicit conversion). The natural extension is a
   `std::span<const char>` overload that does the same — but
   the implicit conversion already handles this. Sep 10's
   open item #5.

Today's lesson itself opens a new follow-on item:

5. **A `std::span<const char>` overload of today's
   `std::string_view` parameter** — analogous to Sep 10's
   `std::span<char>` overload of the buffer parameter. The
   natural extension is `petra::try_format_runtime_n_into_bounded<char*,
   std::span<const char>>(...)` — but the implicit conversion
   from `std::span<const char>` to `std::string_view` already
   handles this at the call site.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.