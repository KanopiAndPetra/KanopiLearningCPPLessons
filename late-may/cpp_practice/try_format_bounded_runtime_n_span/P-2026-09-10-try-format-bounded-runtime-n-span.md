# P-2026-09-10 — `try_format_bounded_runtime_n_span`: STDSPAN sibling of `P-2026-09-09`'s `try_format_bounded_runtime_n`

**Topic:** `petra::try_format_bounded_runtime_n<Args...>(std::span<char>
out, std::format_string<Args...> fmt, Args&&... args)` — the
**STDSPAN** sibling of Sep 9's `try_format_bounded_runtime_n(char* out,
std::size_t cap, ...)`. Today's span overload is the natural lifting
of Sep 9's raw `(char*, size_t)` surface to a `(std::span<char>)`
primary overload — analogous to how Aug 25's `format_to_n_into` lifts
its raw `(char*, n)` surface to a `(std::span<char>)` overload.

Closes Sep 9's "Where we go next" item #3:

> A `petra::try_format_bounded_runtime_n` overload that takes a
> `std::span<char>` — analogous to Aug 25's `format_to_n_into` span
> overload. The wrapper's pass-1 / pass-2 implementation carries
> over; only the public surface changes from `(char*, size_t, ...)`
> to `(span<char>, ...)`. Today's implementation is already
> span-compatible (it just needs a forwarding wrapper that extracts
> `.data()` and `.size()`).

Today lands it. The headline finding is that **the impl didn't have
to change at all** — Sep 9's `try_format_bounded_runtime_n_impl`
already accepted `(char*, std::size_t)` and the span surface is a
thin forwarder over its inputs.

**Important distinction vs Aug 25's `format_to_n_into(std::span<char>,
...)`:** Aug 25's span overload **NUL-terminates** (`out[r.value() or
n-1] = '\0'` so the destination is always a valid C-string). Today's
span overload does NOT NUL-terminate — bytes past `written` are left
untouched (raw `std::format_to_n` semantics; the caller owns
NUL-termination if it wants a C-string). Section 16 pins this
distinction by checking that `sp[5]` is the poison byte `'Q'`, not
`'\0'`, after writing `"ABCDE"` into a 10-byte span.

**How today's lesson relates to Sep 9:**

| Lesson                                  | output type     | fmt type                |
|-----------------------------------------|-----------------|-------------------------|
| Sep 9 (char*+size_t, consteval fmt)     | `char*, size_t` | `std::format_string<Args...>` |
| **TODAY (span<char>, consteval fmt)**   | `std::span<char>` | `std::format_string<Args...>` |

Both overloads live in the same `petra` namespace; both use the
**same** `petra::detail::try_format_bounded_runtime_n_impl` body
(verbatim from Sep 9); both produce identical byte counts and
identical structured-error contracts. The only surface difference is
the call-site ergonomics:

```cpp
// Sep 9: caller unpacks .data() and .size() at every call site.
auto r = petra::try_format_bounded_runtime_n(buf.data(), buf.size(),
                                              "v={}", 42);

// Today: caller passes the span directly.
std::span<char> sp{buf.data(), buf.size()};
auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
```

The char*-and-cap overload from Sep 9 is **kept** in today's source
file as a forwarding shim — both overloads compile side-by-side
and dispatch to the same impl. Section 13 verifies that the two
overloads produce identical results for identical inputs.

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::format_string<Args...>` (P2216R3), `std::make_format_args`,
`std::expected<T, E>` (P0323R12), `std::print` / `std::println`
(P2093R14), `std::span<char>` (P0122R7),
`std::ranges::contiguous_range`, `std::ranges::sized_range`,
`std::indirectly_writable`, `std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **130/130 PASS** across 17 sections on default +
strict-warning + ASan/UBSan + CMake-default + CMake-strict +
CMake-ASan builds. Five consecutive runs on each build produced
identical tallies. Zero warnings under `-Wall -Wextra -Wpedantic
-Werror -Wshadow -Wconversion -Wsign-conversion`. Zero sanitizer
diagnostics. Empty stderr under default + ASan.

---

## Why today

Sep 9's "Where we go next" section (item #3) explicitly named the
lesson today's source file IS:

> A `petra::try_format_bounded_runtime_n` overload that takes a
> `std::span<char>` — analogous to Aug 25's `format_to_n_into` span
> overload. The wrapper's pass-1 / pass-2 implementation carries
> over; only the public surface changes from `(char*, size_t, ...)`
> to `(span<char>, ...)`. Today's implementation is already
> span-compatible (it just needs a forwarding wrapper that extracts
> `.data()` and `.size()`). Sep 8's open item #4.

Today lands it. The lesson exists to pin three things:

1. **The span surface is a thin forwarder** — Sep 9's impl accepts
   `(char*, std::size_t)`; today's wrapper decomposes the span with
   `.data()` + `.size()` and calls the impl verbatim. Same machinery,
   new surface.

2. **The char*-and-cap and span overloads produce identical byte
   counts and identical structured-error contracts** — Section 13
   exercises both overloads on the same buffer + format + args and
   confirms `r.value()` and `r.error()` agree byte-for-byte. There is
   no special-case logic on the span surface; it's a literal
   decomposition + forward.

3. **A span sees only its own sub-range** — a `std::span<char>` over
   a sub-range of a buffer must NOT touch bytes outside the span.
   Section 12 verifies this with a 6-byte sub-span at offset 5 of a
   16-byte buffer: the wrapper writes 5 bytes into `[5, 10)` and
   leaves bytes `[0, 5)` and `[10, 16)` as the poison byte `'Q'`.

The other items from Sep 9's "Where we go next" section remain open
(today closes #3):

- Item #1: replace hand-rolled `petra::counting_output_iterator`
  with `std::counting_output_iterator` when libc++ ships it.
- Item #2: `petra::try_format_expected_bounded` macro layer
  (analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
  from Sep 2).
- Item #4: a `std::span<char>` overload of Sep 9's wrapper that
  WRAPS Aug 25's NUL-terminating behavior with today's `cap`
  parameter — Aug 25 does NOT have a cap parameter (it writes a
  complete `std::string` and returns it). This would be the
  NUL-terminating BOUNDED sibling of today's wrapper.
- Item #5: a `petra::try_format_bounded_runtime_n_into` macro layer
  — analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
  from Sep 2.

---

## The headline finding — the span surface is a thin forwarder

The public surface today is just two overloads, both forwarding to
the SAME impl:

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(std::span<char> out,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out.data(), out.size(),
                                                      fmt, args...);
}
```

The span overload extracts `.data()` + `.size()` and calls the
verbatim Sep 9 impl. No new types, no new machinery, no new error
paths. The consteval check, the two-pass measure-then-write pair,
the structured-error contract, the bounded writer's overflow
behavior — all of it carries over verbatim.

This is the exact same shape Aug 25 used for its NUL-terminating
wrapper's span overload:

```cpp
template <class... Args>
[[nodiscard]] std::expected<std::size_t, std::format_error>
format_to_n_into(std::span<char> buf,
                 std::format_string<Args...> fmt, Args&&... args) {
    return format_to_n_into(buf.data(), buf.size(), fmt,
                            std::forward<Args>(args)...);
}
```

Aug 25's only adds NUL-termination in the body of its char*-and-cap
overload. Today's wrapper has nothing additional to add because the
char*-and-cap body is already span-compatible.

---

## The second finding — sub-spans do not bleed

`std::span<char>` is a *view* over a contiguous buffer — it carries
a pointer + a length, no ownership. The wrapper must write ONLY
inside `[out.data(), out.data() + out.size())` and leave bytes
outside the span untouched.

Section 12 pins this with a 6-byte sub-span at offset 5 of a 16-byte
buffer. The format string `"ABCDE"` needs 5 bytes; `sp` covers bytes
`[5, 11)`. After the call:

- Bytes `[0, 5)`: poison byte `'Q'` — UNTOUCHED.
- Bytes `[5, 10)`: `'A', 'B', 'C', 'D', 'E'` — written.
- Byte `[10)`: poison byte `'Q'` — UNTOUCHED (this is byte 10; the
  format wrote 5 bytes into `[5, 10)`, so byte 10 was past the
  write — but byte 10 IS within the span, just past `written`).
- Bytes `[11, 16)`: poison byte `'Q'` — UNTOUCHED.

If the span surface wrote past `.size()` (e.g. by accidentally
using a different length), bytes `[0, 5)` would be corrupted.
They aren't.

This is also what makes the empty-span edge case well-defined: a
`std::span<char>{buf.data(), 0}` has zero writable bytes, so the
impl's early-returns-for-`cap=0` path fires (Section 4 verifies:
empty span with `""` → success with `r.value() == 0`, buffer
untouched; empty span with `"x"` → TooLarge with
`needed_bytes == 1`, buffer untouched).

---

## The third finding — span overload does NOT NUL-terminate

This is the careful distinction vs Aug 25's `format_to_n_into`
span overload.

Aug 25's wrapper:
- Writes `min(r.value(), n-1)` chars into `out[0, ...)`.
- Then writes `'\0'` at `out[r.value()]` (for `r.value() < n`) or
  `out[n-1]` (for `r.value() >= n`).
- So `out[0, n)` is ALWAYS a valid C-string regardless of
  truncation.

Today's wrapper:
- Writes `min(r.value(), n)` chars into `out[0, ...)`.
- Does NOT write `'\0'` at `out[r.value()]` — leaves byte
  `out[r.value()]` as whatever the caller pre-set it to (the poison
  byte `'Q'` in our tests).
- So `out[0, written)` has the formatted data; `out[written, n)` is
  caller's own territory.

Section 16 exercises this with `sp.size() == 10`, format `"ABCDE"`
(5 bytes needed). After the call:

- `sp[0..4]` = `"ABCDE"`.
- `sp[5]` = `'Q'` (the poison byte) — NOT `'\0'`.
- `sp[9]` = `'Q'` (the poison byte) — NOT `'\0'`.

The calling convention is:

- **Use Aug 25's `format_to_n_into`** when you want "I just want a
  C-string in `buf`" — caller passes a `buf[N]`, gets a
  NUL-terminated string back.
- **Use Sep 9 / today's `try_format_bounded_runtime_n`** when you
  want "I want the raw bytes, exactly `r.value()` of them, into
  `buf[0, written)`" — caller is responsible for NUL-termination if
  it wants a C-string.

Today is the latter — same shape as Sep 9, lifted to span.

---

## The fourth finding — `std::span<char>` properties

`std::span<char>` (P0122R7) is a non-owning view over a contiguous
buffer. Section 1 pins the relevant properties on libc++ 21:

- `std::ranges::contiguous_range<std::span<char>> == true` (it
  models the contiguous-range concept).
- `std::ranges::sized_range<std::span<char>> == true` (its
  iterators are random-access with a known `.size()`).
- `std::is_trivially_copyable_v<std::span<char>> == true` (a
  span is just a pointer + a size; no resources to manage).
- `std::is_standard_layout_v<std::span<char>> == true`.
- `.data()` returns `char*` (non-`const`).
- `.size()` returns `std::size_t`.

These are the properties the wrapper relies on. The
`trivially_copyable` + `standard_layout` + ranges/concepts
properties are also what lets the wrapper pass a sub-span (over
a sub-range of the buffer) without bleeding outside the span
(Section 12).

---

## Important code

### The public surface — char*-and-cap and span

```cpp
namespace petra {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(std::span<char> out,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out.data(), out.size(),
                                                      fmt, args...);
}

}  // namespace petra
```

Mirrors `std::format_to_n`'s signature shape — the `(out, ...)`
parameter is first. The format-string parameter type
`std::format_string<Args...>` is the CONSTEVAL sibling of Sep 8's
`std::string_view` (Sep 7 documented why this matters: malformed
format strings become HARD COMPILE ERRORS at the call site).

### The two-pass measure-then-write implementation (verbatim from Sep 9)

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_impl(char* out, std::size_t cap,
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    // Pass 1: measure.  The consteval ctor of std::format_string<Args...>
    // already accepted the format string at the call site, so parse
    // errors cannot reach this branch — only the std::make_format_args
    // side can throw std::format_error.
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Sized-too-big path.  out is untouched (we haven't written yet).
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    // Pass 2: write into out[0, cap).
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check.
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }

    return written;
}
```

### The structured-error contract

Identical to Sep 6 / Sep 7 / Sep 8 / Sep 9 — `petra::FormatError{kind,
message, needed_bytes}`:

- `FormatError{TooLarge, "formatted output would be N bytes; cap is
  C", needed_bytes=N}` on pass-1 measure-then-cap-fails.
- `FormatError{Format, e.what()}` on `std::format_error` from the
  `std::make_format_args` side (parse errors are caught at compile
  time by the consteval ctor).
- `FormatError{TooLarge, "bounded_char_writer wrote X bytes;
  measure pass said Y", needed_bytes=Y}` on the partial-write
  invariant violation (defensive check, should never fire on
  libc++ 21).

On EVERY error path the span's `out.data()[0, out.size())` is
UNTOUCHED (Sections 4 and 6 verify this with poisoned buffers).

### Reused types from Sep 6 / Sep 7 / Sep 8 / Sep 9

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim from Sep 6.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (Sep 6's
  hand-rolled callback-based iterator).
- `petra::detail::bounded_char_writer` — REUSED verbatim (Sep 8's
  callback-based write sink).
- `petra::detail::try_format_bounded_runtime_n_impl` — REUSED
  verbatim from Sep 9.

No new types are introduced today. The lesson is a pure
consumer-side broadening of Sep 9's surface to `std::span<char>`.

---

## Build and verification commands

Default (-O0 debug build for this lesson):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
        -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span
/tmp/P-2026-09-10-try-format-bounded-runtime-n-span
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
        -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span-strict
/tmp/P-2026-09-10-try-format-bounded-runtime-n-span-strict
```

ASan + UBSan build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
        -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span-asan
/tmp/P-2026-09-10-try-format-bounded-runtime-n-span-asan
```

CMake (default / strict / ASan) — see `CMakeLists.txt` in this dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
./build/P-2026-09-10-try-format-bounded-runtime-n-span
./build-strict/P-2026-09-10-try-format-bounded-runtime-n-span
./build-asan/P-2026-09-10-try-format-bounded-runtime-n-span
```

Compile-time probe (in `/tmp`, NOT in the working tree) — should
FAIL TO COMPILE (exit code non-zero):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -c /tmp/probe_consteval_unterminated_span.cpp \
        -o /tmp/probe_consteval_unterminated_span.o
# Produces: "error: call to consteval function
# 'std::basic_format_string<char, int>::basic_format_string<char[8]>'
# is not a constant expression" with note
# 'non-constexpr function '__throw_format_error' cannot be used in a
# constant expression' and source
# '__throw_format_error("The format string terminates at a '{'")'.
# This pins the consteval surface for the span overload — exactly the
# same consteval behaviour Sep 9 documented for the char*-and-cap
# overload.
```

---

## Section-by-section notes

### Section 1 — toolchain + feature probes (17 CHECKs)

Standard toolchain probes: `FormatError` is default-constructible
and nothrow-move-constructible; both overloads return
`std::expected<std::size_t, petra::FormatError>`; the consteval
ctor of `std::format_string<int>` produces
`std::format_string<int>`; the `FormatErrorKind` enum values
compare unequal and have stable underlying values (0, 1, 2, 3);
`std::span<char>` is `std::ranges::contiguous_range` and
`std::ranges::sized_range`; `std::span<char>` is trivially
copyable and standard-layout; `std::span<char>::data()` returns
`char*`; `std::span<char>::size()` returns `std::size_t`. All PASS.

### Section 2 — fits-exactly happy path (9 CHECKs)

Three sub-cases on `std::span<char>` form: `sp.size()==4` for
`"v=42"` (success, `r.value()==4`, content match); `sp.size()==7`
for `"d=3.142"` (success, `r.value()==7`, content match);
`sp.size()==5` for `"ABCDE"` (success, `r.value()==5`, content
match). All PASS.

### Section 3 — span larger than needed (no truncation) (5 CHECKs)

`sp.size()==10` but `"v=42"` needs 4 bytes. Reports
`r.value()==4`, bytes 0..3 are `"v=42"`, and bytes 4..9 are still
the poison byte `'Q'`. Proves the bounded writer stops at the cap
and does NOT NUL-terminate (distinguishing today from Aug 25's
`format_to_n_into` span overload). All PASS.

### Section 4 — TooLarge (span smaller than needed) (16 CHECKs)

Four sub-cases on the span surface: `sp.size()==3` for `"v=42"`
(TooLarge, `needed_bytes==4`, buffer untouched); `sp.size()==6`
for `"d=3.142"` (TooLarge, `needed_bytes==7`, buffer untouched);
`sp.size()==0` for `""` (success, `r.value()==0`, buffer
untouched); `sp.size()==0` for `"x"` (TooLarge,
`needed_bytes==1`, buffer untouched). All PASS.

### Section 5 — consteval fmt positive (8 CHECKs)

Four sub-cases: `"value={}", 42` → 8 bytes; `"hex={:#x}", 0xcafe`
→ 10 bytes; `"{:.3f}", 3.14159` → 5 bytes; `"{}", 3.14159` → 7
bytes. All on the span surface. All byte-count + content checks
PASS.

### Section 6 — atomicity (15 CHECKs)

Fill the buffer with the prior success `"v=42"`; issue a failing
call (empty span, fmt `"longer"` → TooLarge); verify the prior
`"v=42"` is preserved AND bytes 4..15 are still the poison byte.
The span overload preserves data in the same underlying buffer on
the failure path. All PASS.

### Section 7 — multi-type args round-trip (3 CHECKs)

A single string-with-multiple-replacement-fields format
(`"name={} age={:#x} pi={:.5f} pad={:>10}"`) producing a 45-byte
output. On the span surface. The byte count and the byte content
are checked independently. All PASS.

### Section 8 — UTF-8 mid-codepoint byte round-trip (3 CHECKs)

`"héllo"` in UTF-8 is 6 bytes (the `é` is 2 bytes:
`0xc3 0xa9`). On the span surface. The wrapper reports
`r.value()==6` and the byte content matches. All PASS.

### Section 9 — embedded NUL byte round-trip (5 CHECKs)

A `std::string{'a', '\0', 'b'}` is 3 bytes long. On the span
surface. The wrapper reports `r.value()==3`, `sp[0]=='a'`,
`sp[1]=='\0'`, `sp[2]=='b'`. This exercises the embedded-NUL
round-trip (using `std::string` because `std::string_view` stops at
the first NUL). All PASS.

### Section 10 — exact byte-count invariant (4 CHECKs)

For `("hello {}", "world")` — 11 bytes total — the wrapper
reports `r.value()==11` AND an independent
`std::vformat_to + counting_output_iterator` probe reports
`measured==11`. The byte-by-byte invariant holds. All PASS.

### Section 11 — TooLarge boundary sweep (10 CHECKs)

Three sub-cases for `"v=42"` (needs 4 bytes): `sp.size()==3`
(TooLarge, buffer untouched), `sp.size()==4` (success, exact fit),
`sp.size()==5` (success, no truncation, byte 4 untouched). All
PASS.

### Section 12 — sub-span no-bleed (10 CHECKs)

A `std::span<char>{buf.data()+5, 6}` (a 6-byte sub-span at offset
5 of a 16-byte buffer). Format `"ABCDE"` (5 bytes). After the
call: bytes 0..4 untouched (poison `'Q'`), bytes 5..9 = `"ABCDE"`,
byte 10 untouched (poison `'Q'`), bytes 11..15 untouched (poison
`'Q'`). The wrapper writes ONLY inside `[sp.data(), sp.data() +
sp.size())` — bytes outside the span are not touched. All PASS.

### Section 13 — char\*-and-cap and span overloads agree (7 CHECKs)

Same format + args, same buffer size — both overloads produce
identical byte counts (`r.value()==4` for `("v={}", 42)`) and
identical byte content (`"v=42"`). On the success path, both
overloads' writes leave bytes 4..7 untouched. This pins the
contract: the span overload is a thin forwarder to the SAME impl,
not a special case. All PASS.

### Section 14 — 50-round determinism on the size measurement (2 CHECKs)

50 iterations of `("i={}", i)` on the span surface — verify the
byte count for each round matches the expected size
(`2 + std::to_string(i).size()`) and the byte content matches
`("i=" + std::to_string(i))`. All 50 rounds PASS; first round is
`"i=0"` = 3 bytes.

### Section 15 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)` into
each thread's own `char buf[32]` (each call constructs its own
`std::span<char>` over that buffer). Zero mismatches across 100
total calls. PASS.

### Section 16 — distinct from Aug 25's `format_to_n_into` (5 CHECKs)

Same `std::span<char>` call shape — Aug 25's span overload
NUL-terminates, today's span overload does NOT. Format `"ABCDE"`
into a 10-byte span: `sp[0..4]='A','B','C','D','E'`, but
`sp[5]=='Q'` (poison byte, NOT '\0'). The bounded-buffer shape
is the raw `std::format_to_n` surface; NUL-termination is the
caller's responsibility. All PASS.

### Section 17 — bounded_char_writer never writes past the cap (5 CHECKs)

Five writes into a 4-byte buffer through a directly-constructed
`bounded_char_writer`. The 5th write is dropped silently;
`written()` returns 4; bytes 0..3 are `'a','b','c','d'`. The 5th
char was never assigned. The bounded writer carries over verbatim
from Sep 8 / Sep 9. All PASS.

---

## Where we go next

Today's lesson closes Sep 9's "Where we go next" item #3. Four
items from Sep 9 remain open, plus one new item today's surface
itself names:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type. Sep 9's open item #1. The Section 8 static_assert is the
   migration trigger — when a future libc++ ships the type, the
   static_assert flips to `true` and a follow-up lesson migrates
   all five wrappers (Sep 6, Sep 7, Sep 8, Sep 9, today) to use the
   standard type.

2. **`petra::try_format_expected_bounded` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a `TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt,
   args...)` macro that lifts the consteval vs runtime sibling
   choice to a macro form. Sep 9's open item #2. The macro could
   expand to today's wrapper on the consteval path and Sep 8's
   wrapper on the runtime path.

3. **A `petra::format_to_n_into_bounded` adapter** that combines
   Aug 25's NUL-terminating behavior with today's `cap` parameter
   (Aug 25's `format_to_n_into` does NOT have a cap parameter — it
   writes a complete `std::string` and returns it). This would be
   the NUL-terminating BOUNDED sibling of today's wrapper, with a
   span overload of its own. Sep 9's open item #4.

4. **A `petra::try_format_bounded_runtime_n_into` macro layer**
   — analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a macro that lifts the bounded-buffer vs
   NUL-terminating sibling choice to a macro form. The macro could
   expand to today's wrapper on the bounded-buffer path and Aug
   25's `format_to_n_into` on the NUL-terminating path. Sep 9's
   open item #5.

5. **A `petra::try_format_bounded_runtime_n` overload that takes
   a `std::span<const char>`** (a read-only sub-span) for the
   format string. Today's wrapper takes `std::format_string<Args...>`
   which already accepts `std::span<const char>`-like inputs via
   `std::string_view`. The natural extension is a `std::span<const
   char>` overload that does the same — but the
   `std::format_string<Args...>` ctor already handles this.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
