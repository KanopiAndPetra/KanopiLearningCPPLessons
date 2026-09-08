# P-2026-09-08 — `try_format_runtime_n`: BOUNDED-BUFFER sibling of `P-2026-09-07`'s `try_format_bounded`

**Topic:** `petra::try_format_runtime_n<Args...>(char* out, std::size_t cap, std::string_view fmt, Args&&... args)`
— the **bounded-buffer** sibling of `Sep 7`'s `try_format_bounded`.

The shape difference vs Sep 7 is the **output location** and **return type**:

| Lesson                | Output location        | Return type on success                  |
|-----------------------|------------------------|------------------------------------------|
| Sep 6 (runtime)       | internal `std::string` | `std::expected<std::string, FormatError>` |
| Sep 7 (consteval)     | internal `std::string` | `std::expected<std::string, FormatError>` |
| **Sep 8 (today)**     | **caller `char*` buffer** | **`std::expected<std::size_t, FormatError>`** |

Today's wrapper writes directly into a caller-provided `char*` buffer
with size `cap` (the `std::format_to_n` shape — NOT `std::format` /
`std::format_to_string`). On success, returns the number of bytes
actually written into `out[0, cap)`. On `TooLarge` (pass-1 measure
reports `needed > cap`), returns
`unexpected{FormatError{TooLarge, "would be N bytes; cap is C",
needed_bytes=N}}` and the caller's buffer is **untouched**. On
`std::format_error` from pass 1, returns
`unexpected{FormatError{Format, e.what()}}` and the buffer is also
untouched.

The structured-error contract is identical to Sep 6 / Sep 7:
`petra::FormatError{kind, message, needed_bytes}` is REUSED verbatim.
The two-pass measure-then-write design is also reused: pass 1 measures
via `std::vformat_to + petra::counting_output_iterator` (Sep 6's
hand-rolled callback-based iterator — `std::counting_output_iterator`
is not shipped by libc++ 21); pass 2 (only on pass-1 success +
`needed <= cap`) writes via `std::vformat_to + petra::detail::bounded_char_writer`
— the new sink Sep 8 ships.

**Important distinction vs Aug 25's `format_to_n_into`:** today's
wrapper does **NOT** NUL-terminate. Aug 25's lesson was the
NUL-terminating sibling (the "I just want a string" use case). Today
is the raw `std::format_to_n` shape — caller is responsible for
NUL-termination if it wants a C-string. Section 3 pins this distinction
by checking that bytes past `written` are still the poison byte (proving
the bounded writer stops at the cap without writing a NUL).

**Important distinction vs Sep 6 / Sep 7:** Sep 6 / Sep 7 return a
`std::string` on success (allocating). Today writes into a caller
buffer (no allocation in the wrapper). Section 16 documents this
distinction explicitly.

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::make_format_args`, `std::expected<T, E>` (P0323R12), `std::print`
/ `std::println` (P2093R14), `std::indirectly_writable`,
`std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **253/253 PASS** across 17 sections on default +
strict-warning + ASan/UBSan + CMake-default + CMake-strict + CMake-ASan
builds. Five consecutive runs on each build produced identical tallies.
Zero warnings under `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wconversion -Wsign-conversion`. Zero sanitizer diagnostics. Empty
stderr under default + ASan.

---

## Why today

`P-2026-09-07`'s "Where we go next" section (item #1) explicitly named
the lesson today's source file IS:

> **`petra::try_format_runtime_n<Args...>(out, cap, fmt, args...)`**
> surface that writes directly into a caller-provided `char*`
> buffer with size cap instead of building a `std::string`. This
> is the `std::format_to_n` shape: same two-pass measure-then-write,
> but pass 2 writes into the caller's buffer via `std::vformat_to +
> std::counting_output_iterator` (which IS our hand-rolled type)
> with a sentinel-based cap. The byte-count invariant from Section
> 11 carries over.

Today lands it. Two platform findings Sep 7 could not anticipate but
today's lesson documents:

1. **The bounded writer uses an EXTERNAL counter via callback** (the
   same `petra::counting_output_iterator` callback design Sep 6 used)
   because `std::vformat_to` copies the iterator internally — any
   state stored inside the iterator object itself is invisible to
   the caller after the call. Sep 6 documented the trace probe
   showing the iterator's `this` pointer inside `std::vformat_to`
   is at a DIFFERENT address than the caller's iterator, and the
   live count is updated on the copy, not the original.

2. **`std::format_to_n`'s `n` parameter is `std::iter_difference_t<Out>`**
   — a SIGNED type. Today's wrapper takes `std::size_t cap` and
   converts implicitly. The bounded writer's internal `written_`
   counter is `std::size_t` (unsigned); the implicit conversion
   from the signed iterator difference type to `std::size_t` is
   safe because the vformat_to write path can only produce
   non-negative byte counts.

The other three items Sep 7 named remain open (Sep 8 closes #1):
- Item #2: replace hand-rolled `petra::counting_output_iterator`
  with `std::counting_output_iterator` when libc++ ships it.
- Item #3: `petra::try_format_expected_bounded` macro layer
  (analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
  from Sep 2).
- Item #4: `petra::try_format_bounded_runtime_n<Args...>(out, cap,
  fmt, args...)` sibling combining constval format-string check
  with the runtime format-string check — a constval sibling of
  today's surface.

---

## The headline finding — pass-1 measure / pass-2 write invariant

The lesson exists to pin the byte-count invariant:

> **`written` (bytes actually written into `out[0, cap)`) ==
> `needed` (bytes the pass-1 measure pass reported).**

Section 11 verifies this with two independent measurements:
- The wrapper's pass-2 reports `r.value() == 11` for
  `("hello {}", "world")`.
- An independent `std::vformat_to + counting_output_iterator` on
  the same format string + args reports `measured == 11`.

The two routes agree byte-for-byte. The defensive check in
`try_format_runtime_n_impl` (`if (written != needed) ...`) guards
against a hypothetical future libc++ change that breaks the
pass-1/pass-2 invariant — the caller gets a clear
`FormatError{TooLarge, ..., needed}` rather than a silently-truncated
buffer.

The same invariant Sep 6 / Sep 7 documented for
`try_format_runtime_bounded` / `try_format_bounded`. Today's wrapper
is the bounded-buffer sibling — the invariant carries over.

---

## The second finding — the bounded writer's overflow behavior

The bounded writer (`petra::detail::bounded_char_writer`) silently
drops bytes past the cap. Section 17 exercises this directly: five
writes into a 4-byte buffer; only the first 4 land. This matches
`std::format_to_n`'s `n` parameter semantics:

- `n == needed`: exactly `needed` bytes written.
- `n < needed`: exactly `n` bytes written; the rest are silently
  dropped (NO error from std::format_to_n; the would-be size is
  reported via the return value, not by truncation).
- `n == 0`: zero bytes written.

Today's wrapper does NOT use `std::format_to_n` directly (because
`std::format_to_n` is consteval-only on this toolchain — Aug 31
documented this — and the runtime-format-string sibling
`std::vformat_to` has no `n` parameter). Instead, the bounded
writer implements the same `n`-cap semantics on top of
`std::vformat_to`. The wrapper's pass-1 measure then early-returns
on `needed > cap` so the bounded writer is only ever invoked with
`needed <= cap`, which means the bounded writer's "overflow" path
is unreachable through the public surface.

The overflow path is exercised only in Section 17's internal test
(verifying the bounded writer behaves correctly when its cap is
exceeded). On the public surface, the wrapper either succeeds
(`needed <= cap`) or returns `TooLarge` BEFORE the bounded writer
is invoked.

---

## Important code

### The public surface

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap,
                     std::string_view fmt, const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}
```

Mirrors `std::format_to_n`'s signature shape: `(out, n, fmt, args...)`
— `out` first, `n` second, `args` last. The format-string parameter
type `std::string_view` matches Sep 6's `try_format_runtime_bounded`
(runtime sibling — the consteval sibling of this surface is Sep 7's
open item #4, a separate lesson). `const Args&...` matches Sep 6
exactly.

### The two-pass measure-then-write implementation

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt, const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    // Pass 1: measure.
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
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
        (void)std::vformat_to(std::move(sink), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        // Should be unreachable — pass 1 already exercised the same
        // call.  out MAY be partially written here.
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check: pass 2 wrote exactly `needed` bytes.
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

Five branches:
1. Pass 1 throws `std::format_error` → `FormatError{Format, e.what()}`.
2. Pass 1 reports `needed > cap` → `FormatError{TooLarge, ...,
   needed_bytes=needed}`.
3. Pass 2 throws `std::format_error` (unreachable in practice —
   pass 1 already exercised the same call) → `FormatError{Format,
   e.what()}`.
4. Pass 2 writes a different byte count than pass 1 measured
   (defensive check against future libc++ changes) →
   `FormatError{TooLarge, ..., needed_bytes=needed}`.
5. Success → `written` (the `std::size_t` byte count).

In branches 1, 2, and 3, `out` is either untouched (1, 2) or
partially populated by pass 2 (3). The caller is responsible for
interpreting `out`'s state on each branch.

### The bounded writer

```cpp
class bounded_char_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        bounded_char_writer* it_;

        // const-qualified so const_cast<const proxy&&> = char in the
        // std::indirectly_writable concept is well-formed.  Accept char
        // by value (libc++'s __direct_iterator_buffer writes a single
        // char per slot).
        constexpr const proxy& operator=(char c) const noexcept {
            if (it_->written_ < it_->cap_) {
                it_->buf_[it_->written_] = c;
                ++it_->written_;
                if (it_->cb_) it_->cb_(it_->written_, it_->data_);
            }
            // overflow: silently drop (matches std::format_to_n's n cap).
            return *this;
        }
    };

    constexpr bounded_char_writer(char* buf, std::size_t cap,
                                  callback_type cb, void* data) noexcept
        : buf_(buf), cap_(cap), cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<bounded_char_writer*>(this)};
    }
    constexpr bounded_char_writer& operator++() noexcept { return *this; }
    constexpr bounded_char_writer operator++(int) noexcept { return *this; }

    std::size_t written() const noexcept { return written_; }

private:
    char* buf_;
    std::size_t cap_;
    std::size_t written_{0};
    callback_type cb_;
    void* data_;
};
```

The structure is identical to `petra::counting_output_iterator` —
same callback shape, same proxy design, same const-qualified
`operator=`. The only difference is that `operator=(char c)` writes
the char into `buf_[written_]` (and increments) instead of just
incrementing a counter.

The callback design is essential: `std::vformat_to` makes an
internal copy of the iterator (Sep 6 verified this with a trace
probe). Any state stored inside the iterator object itself is
invisible to the caller after the call. The callback's `void* data_`
slot lets the wrapper track `written_` externally so it can read
it back after `std::vformat_to` returns.

### The FormatError reuse

`petra::FormatError` is REUSED verbatim from Sep 6 / Sep 7. The
bounded-buffer sibling does not need a new kind — only `TooLarge`
and `Format` are reachable in practice. `FormatError` having four
values is a forward-compat feature: future lessons can extend the
surface (e.g. an `ArgumentType` for arg-type mismatches that
survive a hypothetical relaxation of consteval) without breaking
equality semantics.

---

## Observed output

```
-- section 1.probes
  PASS  (std::is_default_constructible_v<petra::FormatError>)
  PASS  (std::is_nothrow_move_constructible_v<petra::FormatError>)
  PASS  (std::is_same_v< decltype(petra::try_format_runtime_n(...)),
                          std::expected<std::size_t, petra::FormatError>>)
  PASS  petra::FormatErrorKind::FormatParse != petra::FormatErrorKind::FormatArgument
  PASS  petra::FormatErrorKind::TooLarge != petra::FormatErrorKind::Format
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::FormatParse) == 0
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::FormatArgument) == 1
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::Format) == 2
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::TooLarge) == 3
  PASS  !std::is_same_v<petra::counting_output_iterator, petra::detail::bounded_char_writer>
-- section 2.fits-exactly
  PASS  r.has_value()
  PASS  r.has_value() && r.value() == 4
  PASS  r.has_value() && std::string_view{buf.data(), 4} == "v=42"
  ...
-- section 4.too-large
  PASS  !r.has_value()
  PASS  !r.has_value() && r.error().kind == petra::FormatErrorKind::TooLarge
  PASS  !r.has_value() && r.error().needed_bytes == 4
  PASS  !r.has_value() && !r.error().message.empty()
  PASS  buf[0] == 'Q'
  PASS  buf[1] == 'Q'
  PASS  buf[2] == 'Q'
  ...
-- section 15.concurrency
  PASS  mismatch.load() == 0
-- section 17.bounded-writer-overflow
  PASS  sink.written() == 4
  PASS  buf[0] == 'a'
  PASS  buf[1] == 'b'
  PASS  buf[2] == 'c'
  PASS  buf[3] == 'd'

-- summary: 253 pass, 0 fail
```

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-08-try-format-runtime-n.cpp \
    -o /tmp/P-2026-09-08-try-format-runtime-n \
    && /tmp/P-2026-09-08-try-format-runtime-n

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-08-try-format-runtime-n.cpp \
        -o /tmp/P-2026-09-08-try-format-runtime-n-strict \
    && /tmp/P-2026-09-08-try-format-runtime-n-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-08-try-format-runtime-n.cpp \
        -o /tmp/P-2026-09-08-try-format-runtime-n-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       /tmp/P-2026-09-08-try-format-runtime-n-asan

# CMake: default / strict / ASan — builds the target three ways
cmake -S . -B build
cmake --build build
./build/P-2026-09-08-try-format-runtime-n

cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-09-08-try-format-runtime-n

cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-08-try-format-runtime-n
```

### A note on LeakSanitizer

As on Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6 / Sep 7,
LeakSanitizer is **not supported on darwin/arm64** (probed:
`"detect_leaks is not supported on this platform"`).
`ASAN_OPTIONS=detect_leaks=0` was used for all ASan runs.
The wrapper's hot path allocates at most the `FormatError`'s
captured `std::string` (only on the TooLarge / Format error paths);
the success path has zero heap allocations (the wrapper writes
directly into the caller's `char*` buffer — distinguishing today
from Sep 6 / Sep 7 which return a `std::string` that heap-allocates
on success).

### A note on `<print>` requiring `-fexperimental-library`

`std::println` (used by the test harness) lives in `<print>`
(P2093R14). On Apple Clang 21 / libc++ 21 the `<print>` header is
shipped but the `std::println` / `std::print` names are gated behind
`__cpp_lib_print`. The `-fexperimental-library` flag flips
`__has_feature(experimental_library)` which the libc++ config
reads to default the gate to 1. Today's CMakeLists.txt and direct
compile commands all pass the flag.

### A note on `std::make_format_args` requiring lvalues

`std::make_format_args` takes `_Args&...` (lvalue references).
The lesson's wrapper takes `const Args&...` (const lvalue refs).
`std::make_format_args(args...)` materializes an lvalue for each
const lvalue ref. Section 9 demonstrates this by passing the
UTF-8 sequence through a named `std::string` local; Section 10
demonstrates it by binding the embedded-NUL payload to a named
`std::string` with explicit length (because `std::string_view`
stops at the first NUL).

---

## Section-by-section findings

### Section 1 — toolchain + feature probes (10 CHECKs)

`petra::FormatError` is default-constructible (after explicit
`= default;` declaration) and `noexcept`-move-constructible. The
return type of `petra::try_format_runtime_n(0, 0, "")` is exactly
`std::expected<std::size_t, petra::FormatError>`. `FormatErrorKind`
has four values mapped to 0/1/2/3; `FormatParse != FormatArgument`,
`TooLarge != Format`. `petra::counting_output_iterator` and
`petra::detail::bounded_char_writer` are distinct types (the
latter is the new sink today's lesson ships). All PASS.

### Section 2 — fits-exactly happy path (9 CHECKs)

Three fits-exactly cases: `("v={}", 42)` with cap=4 →
`expected(4, "v=42")`. `("d={:.3f}", 3.14159)` with cap=7 →
`expected(7, "d=3.142")`. `("ABCDE")` with cap=5 → `expected(5,
"ABCDE")`. All PASS.

### Section 3 — cap larger than needed (no truncation) (5 CHECKs)

Cap=10 with a 4-byte output → success, returns 4 bytes. The wrapper
does NOT write past `written` — bytes 4..9 are still the poison
byte 'Q'. This proves the bounded writer stops at the cap and does
NOT NUL-terminate past `written` (distinguishing today from Aug
25's `format_to_n_into`). All PASS.

### Section 4 — TooLarge (17 CHECKs)

Three TooLarge cases plus two zero-cap edge cases. Buffer is
UNTOUCHED on every TooLarge path (all bytes still 'Q'). The
message includes both `needed_bytes` and the actual cap so the
caller can size the buffer correctly. All PASS.

### Section 5 — Format error path (7 CHECKs)

Unterminated replacement field and too-few args are caught at
format time (today's surface is the runtime sibling — the
consteval sibling would catch these at compile time). Buffer is
UNTOUCHED on every Format-error path (all bytes still 'Q').
All PASS.

### Section 6 — atomicity (5 CHECKs)

A successful call's `r.value()` and `buf[0..r.value())` are
untouched by a follow-up failing call's `std::unexpected`
construction on the call stack. All PASS.

### Section 7 — multi-type args round-trip (6 CHECKs)

`("{}={} h={:#x}", "v", 42, 0xcafe)` → `expected(13, "v=42
h=0xcafe")` (13 bytes). `("d={:.3f} s={:>8} l={:<6}",
3.14159, "x", "abc")` → `expected(27, "d=3.142 s=       x
l=abc   ")` (27 bytes). All PASS.

### Section 8 — std::counting_output_iterator availability probe (1 CHECK)

The same probe Sep 6 / Sep 7 use. The recursive grep across the
libc++ 21 header tree returns zero matches. The static_assert
flips to `true` on a future libc++ that ships the type. PASS.

### Section 9 — UTF-8 mid-codepoint byte round-trip (3 CHECKs)

`("x={}", "\xE2\x9C\x93")` → `expected(5, "x=\xE2\x9C\x93")`.
UTF-8 mid-codepoint bytes pass through unchanged. PASS.

### Section 10 — embedded NUL byte round-trip (3 CHECKs)

`("{}", "A\0B\0"_as_4byte_string)` → `expected(4, "A\0B\0")`.
Embedded NUL bytes round-trip without truncation. PASS.

### Section 11 — exact byte-count invariant (4 CHECKs)

The wrapper reports `r.value() == 11` for `("hello {}",
"world")`. An independent `std::vformat_to + counting_output_iterator`
on the same inputs reports `measured == 11`. Both routes agree
byte-for-byte. PASS.

### Section 12 — TooLarge boundary sweep (15 CHECKs)

For `("ABCDE")` (5 bytes), cap ∈ {3, 4, 5, 6, 100}:
- cap=3: `unexpected{TooLarge, needed_bytes=5}`; buffer untouched
- cap=4: `unexpected{TooLarge, needed_bytes=5}`; buffer untouched
- cap=5: `expected(5, "ABCDE")`
- cap=6: `expected(5, "ABCDE")`
- cap=100: `expected(5, "ABCDE")`

The transition from failure to success is sharp at `cap == 5`.
On every TooLarge path, the buffer is UNTOUCHED (all bytes
still 'Q'). All PASS.

### Section 13 — FormatError equality / inequality (4 CHECKs)

Two `TooLarge{kind, "x", 5}` are equal; different `needed_bytes`
makes them unequal; different `kind` makes them unequal. PASS.

### Section 14 — 50-round determinism (150 CHECKs)

50 consecutive `("i={}", i)` calls for i=0..49. Each call's
returned `r.value()` equals `std::to_string(i)` size + 2 bytes
for "i=". The byte-by-byte invariant (string_view of buffer
equals the expected string) holds for every call. PASS.

### Section 15 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)` into
each thread's own `char buf[32]` (the wrapper is thread-safe as
long as the output buffer is not shared). Zero mismatches across
100 total calls. PASS.

### Section 16 — distinct from Sep 7's `try_format_bounded` (7 CHECKs)

Today's wrapper returns `std::size_t` on success and writes into
the caller's `char*` buffer. Sep 7's wrapper returns `std::string`
on success. The byte-count agreement is preserved (both produce 4
bytes for `("v={}", 42)`). The structured-error contract is
identical: `FormatError{kind=TooLarge, needed_bytes=4}` on cap=3.
All PASS.

### Section 17 — bounded_char_writer never writes past the cap (5 CHECKs)

Five writes into a 4-byte buffer through a directly-constructed
`bounded_char_writer`. The 5th write is dropped silently; `written()`
returns 4; bytes 0..3 are 'a','b','c','d'. The 5th char was never
assigned. All PASS.

---

## Where we go next

Today's lesson pins the bounded-buffer wrapper surface. Three items
from Sep 7's "Where we go next" remain open, plus one new item
today's surface itself names:

1. **`petra::try_format_bounded_runtime_n<Args...>(out, cap,
   fmt, args...)`** — the CONSTEVAL sibling of today's surface.
   Takes a `std::format_string<Args...>` whose ctor is consteval
   so malformed format strings become HARD COMPILE ERRORS at the
   call site (Sep 7's open item #4). The two-pass measure-then-write
   design carries over verbatim; the only difference is the
   format-string parameter type. This is the natural sibling of
   Sep 7's `try_format_bounded` with today's bounded-buffer output
   location.

2. **Replace the hand-rolled `petra::counting_output_iterator` with
   `std::counting_output_iterator`** when libc++ ships the type.
   The Section 8 static_assert is the migration trigger — when a
   future libc++ ships the type, the static_assert flips to `true`
   and a follow-up lesson migrates the wrapper to use the standard
   type. Sep 7's item #3. Today's surface uses the same iterator
   type Sep 6 / Sep 7 use, so the migration affects all three
   wrappers simultaneously.

3. **`petra::try_format_expected_bounded` macro layer** — analogous
   to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO` from Sep 2: a
   `TRY_FORMAT_RUNTIME_N(out, cap, fmt, args...)` macro that lifts
   the runtime vs NUL-terminating sibling choice to a macro form.
   Sep 7's item #3. The macro could expand to today's wrapper on
   the runtime path and Aug 25's `format_to_n_into` on the
   NUL-terminating path.

4. **A `petra::try_format_runtime_n` overload that takes a
   `std::span<char>`** — analogous to Aug 25's `format_to_n_into`
   span overload. The wrapper's pass-1 / pass-2 implementation
   carries over; only the public surface changes from
   `(char*, size_t, ...)` to `(span<char>, ...)`. Today's
   implementation is already span-compatible (it just needs a
   forwarding wrapper that extracts `.data()` and `.size()`).

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open: pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status
badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history.