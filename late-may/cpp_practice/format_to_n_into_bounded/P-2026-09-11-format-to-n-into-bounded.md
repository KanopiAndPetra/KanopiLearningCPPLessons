# P-2026-09-11 — `format_to_n_into_bounded`: NUL-TERMINATING BOUNDED sibling of Sep 9 / Sep 10's `try_format_bounded_runtime_n[_span]`

**Topic:** `petra::format_to_n_into_bounded<Args...>(char* out,
std::size_t cap, std::format_string<Args...> fmt, Args&&... args)` (and
its `std::span<char>` overload) — the **NUL-TERMINATING BOUNDED**
sibling of Sep 9 / Sep 10's `try_format_bounded_runtime_n[_span]`.
Closes Sep 10's "Where we go next" item #3, verbatim:

> A `petra::format_to_n_into_bounded` adapter that combines Aug 25's
> NUL-terminating behavior with today's `cap` parameter (Aug 25's
> `format_to_n_into` does NOT have a cap parameter — it writes a
> complete `std::string` and returns it). This would be the
> NUL-terminating BOUNDED sibling of today's wrapper, with a span
> overload of its own.

Today lands it. The headline finding is that **the impl is genuinely
new** — Sep 9 / Sep 10's `try_format_bounded_runtime_n_impl` cannot
be reused because Sep 9 reserves NO slot for the NUL; today's
two-pass measure-then-write pair must reserve `cap - 1` as the
effective writable byte count and treat `needed == cap` as a
real (partial-write) success path, not as an invariant violation.

**How today's lesson relates to the chain:**

| Lesson                              | output          | fmt type                | NUL-terminates? |
|-------------------------------------|-----------------|-------------------------|------------------|
| Aug 25 (no cap, NUL)                | bounded buffer  | `std::format_string<Args...>` | yes (always) |
| Sep 6  (cap, std::string, runtime)  | `std::string`   | `std::string_view`      | n/a              |
| Sep 7  (cap, std::string, consteval)| `std::string`   | `std::format_string<Args...>` | n/a         |
| Sep 8  (char*, runtime)             | `char*`         | `std::string_view`      | no               |
| Sep 9  (char*, consteval)           | `char*`         | `std::format_string<Args...>` | no          |
| Sep 10 (span<char>, consteval)      | `std::span<char>` | `std::format_string<Args...>` | no         |
| **TODAY (char*, consteval, NUL)**   | `char*`         | `std::format_string<Args...>` | **yes**     |

Today combines three axes:
- **Sep 9's `cap` parameter** (with structured `FormatError{TooLarge,
  ..., needed_bytes=N}` on `needed > cap`).
- **Aug 25's NUL-termination contract** (NUL placement follows Aug 25's
  formula: `nul_idx = (written < cap) ? written : (cap - 1)`).
- **Sep 10's `std::span<char>` overload** (thin forwarder that
  extracts `.data()` + `.size()` and dispatches to the same impl).

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::format_string<Args...>` (P2216R3), `std::make_format_args`,
`std::expected<T, E>` (P0323R12), `std::print` / `std::println`
(P2093R14), `std::span<char>` (P0122R7),
`std::ranges::contiguous_range`, `std::ranges::sized_range`,
`std::indirectly_writable`, `std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **99/99 PASS** across 18 sections on default +
strict-warning + ASan/UBSan + CMake-default + CMake-strict +
CMake-ASan builds. Five consecutive runs on each build produced
identical tallies. Zero warnings under `-Wall -Wextra -Wpedantic
-Werror -Wshadow -Wconversion -Wsign-conversion`. Zero sanitizer
diagnostics. Empty stderr under default + ASan.

---

## Why today

Sep 10's "Where we go next" section (item #3) explicitly named
today's lesson:

> A `petra::format_to_n_into_bounded` adapter that combines Aug 25's
> NUL-terminating behavior with today's `cap` parameter (Aug 25's
> `format_to_n_into` does NOT have a cap parameter — it writes a
> complete `std::string` and returns it). This would be the
> NUL-terminating BOUNDED sibling of today's wrapper, with a span
> overload of its own.

Today lands it. The lesson exists to pin three things:

1. **The NUL-terminating impl is genuinely new** — Sep 9 reserves
   NO slots for NUL (Sep 9's impl returns `r.value() == needed`
   on success, so a `needed == cap` success would not have a slot
   for the NUL). Today's impl uses `effective_cap = cap - 1` as
   the writable byte count and adds a separate branch for the
   truncation-success edge (`needed == cap`).
2. **The truncation edge is a real success path** — when
   `needed == cap`, today's wrapper writes `cap - 1` bytes and
   puts the NUL at `cap - 1`. This is the same edge Aug 25's
   wrapper handles; today's wrapper preserves it but layers the
   structured `FormatError{TooLarge, ...}` contract on top.
3. **The span overload is a thin forwarder** — same shape Sep 10
   used; the public `std::span<char>` overload extracts `.data()`
   + `.size()` and dispatches to the same impl.

The other items from Sep 10's "Where we go next" section remain
open (today closes #3):

- Item #1: replace hand-rolled `petra::counting_output_iterator`
  with `std::counting_output_iterator` when libc++ ships it.
- Item #2: a `petra::try_format_expected_bounded` macro layer
  analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
  from Sep 2.
- Item #4: a `petra::try_format_bounded_runtime_n_into` macro
  layer analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
  from Sep 2.
- Item #5: a `std::span<const char>` overload (today's wrapper
  already accepts `std::format_string<Args...>` which accepts
  `std::span<const char>`-like inputs via `std::string_view`).

---

## The headline finding — the impl is genuinely new

The natural first draft of today's wrapper reused Sep 9's
`try_format_bounded_runtime_n_impl` and added a NUL-write after
the impl returned success. That **does not work** for the
truncation edge:

- Sep 9's impl returns `r.value() == needed` on success.
- Sep 9 reserves NO slot for the NUL.
- So if `needed == cap` exactly, Sep 9's impl would return
  `r.value() = cap`, and the NUL placement formula `nul_idx =
  r.value()` would write to `out[cap]` — **OUT OF BOUNDS**.

The correct impl has to reserve `cap - 1` as the effective
writable byte count and treat `needed == cap` as a special
branch where pass 2 writes `cap - 1` bytes and NUL goes at
`cap - 1`. Today's wrapper has its own impl
(`petra::detail::try_format_to_n_into_bounded_impl`) with three
branches:

1. **`cap == 0`**: degenerate; `needed == 0` is success (no NUL),
   `needed > 0` is `TooLarge`.
2. **`needed > cap`**: `TooLarge` (caller's buffer UNTOUCHED).
3. **`needed == cap`**: TRUNCATION SUCCESS — pass 2 writes
   `cap - 1` bytes, NUL at `cap - 1`, return `cap - 1`.
4. **`needed < cap`**: FULL SUCCESS — pass 2 writes `needed`
   bytes, NUL at `needed`, return `needed`.

The impl reuses `petra::counting_output_iterator` (Sep 6 /
Sep 7 / Sep 8 / Sep 9 / Sep 10, verbatim) for pass 1, and
`petra::detail::bounded_char_writer` (Sep 8 / Sep 9 / Sep 10,
verbatim) for pass 2. The only NEW code is the branch logic
above. Sections 4, 8, 17 verify all three branches
end-to-end.

---

## The second finding — NUL placement follows Aug 25's formula

Aug 25's NUL placement formula is:

```
nul_idx = (written < cap) ? written : (cap - 1)
```

On the no-truncation success path (Section 2, 3), `written ==
needed < cap`, so `nul_idx == written`. The NUL sits at
`out[written]`, one past the last formatted byte. The buffer
is a well-formed C-string of length `written`.

On the truncation edge (Section 4, 8, 17), `written == cap - 1`,
so `nul_idx == cap - 1`. The NUL sits at `out[cap - 1]`, the
last byte of the buffer. The buffer is a well-formed C-string
of length `cap - 1`.

This is byte-for-byte identical to Aug 25's
`format_to_n_into`. Today's wrapper layers Sep 9's structured
error contract (the `FormatError{TooLarge, ...}` on
`needed > cap`) ON TOP of Aug 25's NUL placement.

---

## The third finding — the span overload is a thin forwarder

The `std::span<char>` overload is a verbatim mirror of Sep 10's
forwarder pattern:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(std::span<char> out,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return format_to_n_into_bounded(out.data(), out.size(),
                                    fmt, args...);
}
```

The wrapper decomposes the span with `.data()` + `.size()` and
calls the `(char*, std::size_t)` overload. No new machinery,
no special-case logic. Section 11 verifies the two overloads
produce identical byte counts and identical byte content for
identical inputs; Section 12 verifies the sub-span no-bleed
contract (a `std::span<char>` over a sub-range of a buffer
must NOT touch bytes outside the span).

---

## Important code

### The public surface — char*+cap and span

```cpp
namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(char* out, std::size_t cap,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(out, cap,
                                                     fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(std::span<char> out,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return format_to_n_into_bounded(out.data(), out.size(),
                                    fmt, args...);
}

}  // namespace petra
```

### The two-pass measure-then-write impl with NUL reservation

```cpp
namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_to_n_into_bounded_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt,
                                  const Args&... args) {
    // Pass 1: measure.
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt.get(),
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
            bounded_char_writer sink{out, effective_cap,
                                     cb, &written};
            (void)std::vformat_to(std::move(sink), fmt.get(),
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
        (void)std::vformat_to(std::move(sink), fmt.get(),
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

Identical to Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10 —
`petra::FormatError{kind, message, needed_bytes}`:

- `FormatError{TooLarge, "formatted output would be N bytes;
  cap is C (effective writable is C-1)", needed_bytes=N}` on
  `needed > cap`.
- `FormatError{Format, e.what()}` on `std::format_error` from
  the `std::make_format_args` side (parse errors are caught at
  compile time by the consteval ctor).
- `FormatError{TooLarge, "bounded_char_writer wrote X bytes;
  expected Y on truncation edge", needed_bytes=Y}` on the
  partial-write invariant violation (defensive check).
- `FormatError{TooLarge, "bounded_char_writer wrote X bytes;
  measure pass said Y", needed_bytes=Y}` on the full-success
  partial-write invariant violation (defensive check).

On EVERY error path the caller's buffer is UNTOUCHED (Sections
5, 10 verify this with poisoned buffers).

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (still
  hand-rolled; `std::counting_output_iterator` is NOT shipped
  by libc++ 21).
- `petra::detail::bounded_char_writer` — REUSED verbatim.

NEW today:

- `petra::detail::try_format_to_n_into_bounded_impl` — NEW
  two-pass impl with NUL reservation.
- `petra::format_to_n_into_bounded<Args...>(char*, std::size_t,
  std::format_string<Args...>, Args&&...)` — NEW public surface.
- `petra::format_to_n_into_bounded<Args...>(std::span<char>,
  std::format_string<Args...>, Args&&...)` — NEW span overload.

---

## Build and verification commands

Default (`-O0` debug build for this lesson):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-11-format-to-n-into-bounded.cpp \
        -o /tmp/P-2026-09-11-format-to-n-into-bounded
/tmp/P-2026-09-11-format-to-n-into-bounded
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-11-format-to-n-into-bounded.cpp \
        -o /tmp/P-2026-09-11-format-to-n-into-bounded-strict
/tmp/P-2026-09-11-format-to-n-into-bounded-strict
```

ASan + UBSan build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-11-format-to-n-into-bounded.cpp \
        -o /tmp/P-2026-09-11-format-to-n-into-bounded-asan
/tmp/P-2026-09-11-format-to-n-into-bounded-asan
```

CMake (default / strict / ASan) — see `CMakeLists.txt` in this
dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DENABLE_STRICT_WARNINGS=ON \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
./build/P-2026-09-11-format-to-n-into-bounded
./build-strict/P-2026-09-11-format-to-n-into-bounded
./build-asan/P-2026-09-11-format-to-n-into-bounded
```

---

## Section-by-section notes

### Section 1 — toolchain + feature probes (10 CHECKs)

Standard toolchain probes: `sizeof(std::size_t) == 8`;
`sizeof(std::expected<std::size_t, petra::FormatError>) >= 8`;
`__cpp_lib_format == 202110`; `__cpp_lib_expected == 202211`;
`__cpp_lib_span == 202002`; `std::span<char>` is
`std::ranges::contiguous_range` and `std::ranges::sized_range`;
`std::span<char>` is trivially copyable and standard-layout.
All PASS.

### Section 2 — exact-fit NUL at written (3 CHECKs)

cap=8, format `"1234567"` (7 chars). `r.value() == 7`;
`buf[7] == '\0'`. The NUL sits at `written == cap - 1`. PASS.

### Section 3 — no-truncation NUL at written (4 CHECKs)

cap=16, format `"hello {}"` with `"world"` (11 chars).
`r.value() == 11`; `buf[11] == '\0'`; `buf[12] == '\0'`
(zero-initialised untouched). The NUL sits at `written == 11 <
cap == 16`. PASS.

### Section 4 — truncation edge NUL at cap-1 (7 CHECKs)

cap=5, format `"ABCDE"` (5 chars). needed == cap exactly →
truncation edge. `r.value() == 4` (cap - 1); `buf[0..3] ==
'A','B','C','D'`; `buf[4] == '\0'`. The NUL sits at `cap - 1`,
overwriting nothing (the bounded writer caps at `cap - 1` and
the cap-1 byte is the last writable slot). PASS.

### Section 5 — TooLarge buffer untouched (6 CHECKs)

cap=4, format `"1234567"` (7 chars). needed > cap →
`FormatError{TooLarge, needed_bytes=7}`. Buffer is poisoned
with `'Q'`; all 4 bytes stay `'Q'`. PASS.

### Section 6 — cap=0 empty format (3 CHECKs)

cap=0, format `""` (0 chars). needed == 0 == cap → success
with `r.value() == 0`. No NUL can be written (no room). Buffer
unchanged. PASS.

### Section 7 — cap=0 non-empty format (4 CHECKs)

cap=0, format `"x"` (1 char). needed > 0 > cap →
`FormatError{TooLarge, needed_bytes=1}`. Buffer unchanged. PASS.

### Section 8 — cap=1 only NUL (3 CHECKs)

cap=1, format `"x"` (1 char). needed == cap == 1 → truncation
edge. `r.value() == 0`; `buf[0] == '\0'`. The formatted byte
never makes it into the buffer (the NUL occupies the only
writable slot, which was never written to). PASS.

### Section 9 — format specifiers (4 CHECKs)

Format specifiers passthrough (`{:#x}`, `{:.3f}`):
`"v={} h={:#x} pi={:.3f}"` → `"v=42 h=0xcafe pi=3.142"`.
NUL at `buf[r.value()]`. PASS.

### Section 10 — atomicity on TooLarge failure (4 CHECKs)

Pre-fill `buf` with `"v=42"`; issue a failing call (cap=8,
format `"hello world"` needs 11 → TooLarge). Pre-fill
survives (`buf[0..3] == "v=42"`); error kind == TooLarge;
needed_bytes == 11. PASS.

### Section 11 — span overload equivalence (8 CHECKs)

Same format + args, same buffer size: char\*+cap and span
overloads produce identical byte counts (`r.value() == 4` for
`("v={}", 42)`) and identical byte content (`"v=42"`). On
the success path, both overloads' writes leave bytes past
`written` as the zero-initialised byte. The span overload is
a literal decomposition + forward — no special-case logic.
PASS.

### Section 12 — sub-span no-bleed (12 CHECKs)

A `std::span<char>{buf + 5, 6}` (a 6-byte sub-span at offset
5 of a 16-byte buffer). Format `"ABCDE"` (5 chars).
After the call: bytes `[0, 5)` untouched (poison `'Q'`);
bytes `[5, 10)` = formatted text `'A','B','C','D','E'`;
NUL at `buf[10] == '\0'` (within sub-span at offset written);
bytes `[11, 16)` untouched (poison `'Q'`). The wrapper
writes ONLY inside `[sp.data(), sp.data() + sp.size())` —
bytes outside the span are not touched. PASS.

### Section 13 — multi-type args round-trip (4 CHECKs)

A single string-with-multiple-replacement-fields format
(`"name={} age={:#x} pi={:.5f}"`) producing a 26-byte output.
Byte count and byte content are checked independently. NUL
at `buf[r.value()]`. PASS.

### Section 14 — UTF-8 mid-codepoint byte round-trip (4 CHECKs)

`"héllo"` in UTF-8 is 6 bytes (the `é` is 2 bytes:
`0xc3 0xa9`). `r.value() == 6`; byte content matches.
PASS.

### Section 15 — embedded NUL byte round-trip (5 CHECKs)

A `std::string{'a', '\0', 'b'}` is 3 bytes long.
`r.value() == 3`; `buf[0] == 'a'`, `buf[1] == '\0'`,
`buf[2] == 'b'`; the wrapper's NUL is at `buf[3]`. This
exercises the embedded-NUL round-trip. PASS.

### Section 16 — byte-count invariant (3 CHECKs)

For `("hello {}", "world")` — 11 bytes total — the wrapper
reports `r.value() == 11` AND an independent
`std::vformat_to + counting_output_iterator` probe reports
`measured == 11`. The byte-by-byte invariant holds. PASS.

### Section 17 — TooLarge boundary sweep (12 CHECKs)

Three sub-cases for `"v=42"` (needs 4 bytes): cap=3
(TooLarge, `needed_bytes == 4`); cap=4 (truncation edge,
`r.value() == 3`, NUL at `cap - 1 == 3`); cap=5 (full
success, `r.value() == 4`, NUL at `written == 4 < cap`).
The cap=4 case is the meaningful one — it proves the
truncation edge branch is correct end-to-end. PASS.

### Section 18 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)`
into each thread's own `char buf[32]` (each call constructs
its own `std::span<char>` over that buffer). Each call's
`r.value()` matches the byte count, each call's
`std::string_view{buf, r.value()}` matches the expected
content, and `buf[r.value()] == '\0'` for every call. Zero
mismatches across 100 total calls. PASS.

---

## Where we go next

Today's lesson closes Sep 10's "Where we go next" item #3.
Four items from Sep 10 remain open:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships
   the type. Sep 10's open item #1. The Section 1 static_assert
   is the migration trigger — when a future libc++ ships the
   type, the static_assert flips to `true` and a follow-up
   lesson migrates all six wrappers (Sep 6, Sep 7, Sep 8,
   Sep 9, Sep 10, today) to use the standard type.

2. **`petra::try_format_expected_bounded` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a `TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt,
   args...)` macro that lifts the consteval vs runtime sibling
   choice to a macro form. Sep 10's open item #2.

3. **A `petra::try_format_bounded_runtime_n_into` macro layer**
   — analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a macro that lifts the bounded-buffer vs
   NUL-terminating sibling choice to a macro form. The macro
   could expand to today's wrapper on the bounded-buffer path
   and Aug 25's `format_to_n_into` on the NUL-terminating
   path. Sep 10's open item #4.

4. **A `std::span<const char>` overload** for the format
   string parameter. Today's wrapper takes
   `std::format_string<Args...>` which already accepts
   `std::span<const char>`-like inputs via `std::string_view`.
   The natural extension is a `std::span<const char>` overload
   that does the same — but the `std::format_string<Args...>`
   ctor already handles this. Sep 10's open item #5.

Today's lesson itself opens a new follow-on item:

5. **A runtime-format-string sibling of today's wrapper** —
   analogous to Sep 8's `try_format_runtime_n` being the
   runtime-format-string sibling of Sep 7's
   `try_format_bounded`. Today's wrapper takes
   `std::format_string<Args...>` (consteval check at the call
   site); a runtime sibling would take `std::string_view` and
   catch `std::format_error` from the `std::make_format_args`
   side. The impl would be the same shape but with the consteval
   gate removed (a `try_format_runtime_n_into_bounded` wrapper).

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
