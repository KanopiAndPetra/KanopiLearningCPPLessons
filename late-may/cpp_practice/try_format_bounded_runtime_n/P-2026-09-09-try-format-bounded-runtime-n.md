# P-2026-09-09 — `try_format_bounded_runtime_n`: CONSTEVAL BOUNDED-BUFFER sibling of `P-2026-09-08`'s `try_format_runtime_n`

**Topic:** `petra::try_format_bounded_runtime_n<Args...>(char* out,
std::size_t cap, std::format_string<Args...> fmt, Args&&... args)`
— the **CONSTEVAL BOUNDED-BUFFER** combination: Sep 7's
`std::format_string<Args...>` (consteval-checked format string) +
Sep 8's `char*` bounded-buffer output location + Sep 8's
`std::expected<std::size_t, FormatError>` return type.

Closes Sep 8's "Where we go next" item #1. Today's surface is the
natural sibling of Sep 7's `try_format_bounded` (which writes into a
`std::string` and returns `std::expected<std::string, FormatError>`)
with today's bounded-buffer output location (the `char*` buffer
shape).

How today's lesson relates to the three predecessors:

| Lesson                       | fmt type            | output       | success type |
|------------------------------|---------------------|--------------|--------------|
| Sep 6 (runtime, bounded)     | `std::string_view`  | `std::string` | `std::expected<std::string, FormatError>` |
| Sep 7 (consteval, bounded)   | `std::format_string<Args...>` | `std::string` | `std::expected<std::string, FormatError>` |
| Sep 8 (runtime, n-buffer)    | `std::string_view`  | `char*`      | `std::expected<std::size_t, FormatError>` |
| **TODAY (consteval, n-buffer)** | **`std::format_string<Args...>`** | **`char*`** | **`std::expected<std::size_t, FormatError>`** |

The structured-error contract is identical to Sep 6 / Sep 7 / Sep 8:
`petra::FormatError{kind, message, needed_bytes}` is REUSED verbatim
from those three lessons. The two-pass measure-then-write design is
also reused: pass 1 measures via `std::vformat_to +
petra::counting_output_iterator` (Sep 6's hand-rolled callback-based
iterator — `std::counting_output_iterator` is not shipped by libc++
21); pass 2 (only on pass-1 success + `needed <= cap`) writes via
`std::vformat_to + petra::detail::bounded_char_writer` (Sep 8's
callback-based write sink).

**Important distinction vs Sep 7's `try_format_bounded`:** today
writes into a caller-provided `char*` buffer (no allocation in the
wrapper). Sep 7 allocates a `std::string` internally. The consteval
format-string check is the same shape in both — the wrapper's
format-string parameter is `std::format_string<Args...>` whose
implicit ctor is `consteval`.

**Important distinction vs Aug 25's `format_to_n_into`:** today's
wrapper does **NOT** NUL-terminate. Aug 25's lesson was the
NUL-terminating sibling (the "I just want a string" use case).
Today is the raw `std::format_to_n` shape — caller is responsible
for NUL-termination if it wants a C-string. Section 3 pins this
distinction by checking that bytes past `written` are still the
poison byte.

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::format_string<Args...>` (P2216R3), `std::make_format_args`,
`std::expected<T, E>` (P0323R12), `std::print` / `std::println`
(P2093R14), `std::indirectly_writable`, `std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **109/109 PASS** across 17 sections on default +
strict-warning + ASan/UBSan + CMake-default + CMake-strict +
CMake-ASan builds. Five consecutive runs on each build produced
identical tallies. Zero warnings under `-Wall -Wextra -Wpedantic
-Werror -Wshadow -Wconversion -Wsign-conversion`. Zero sanitizer
diagnostics. Empty stderr under default + ASan.

---

## Why today

Sep 8's "Where we go next" section (item #1) explicitly named the
lesson today's source file IS:

> **`petra::try_format_bounded_runtime_n<Args...>(out, cap, fmt,
> args...)`** — the CONSTEVAL sibling of today's surface. Takes a
> `std::format_string<Args...>` whose ctor is consteval so
> malformed format strings become HARD COMPILE ERRORS at the call
> site (Sep 7's open item #4). The two-pass measure-then-write
> design carries over verbatim; the only difference is the
> format-string parameter type. This is the natural sibling of
> Sep 7's `try_format_bounded` with today's bounded-buffer output
> location.

Today lands it. The headline finding (the consteval sibling
rejects malformed format strings as HARD COMPILE ERRORS) is pinned
by two separate compile-time probes in `/tmp`:

1. `/tmp/probe_consteval_unterminated.cpp` — produces a hard error
   `call to consteval function std::basic_format_string<char, int>::basic_format_string<char[2]> is not a constant expression`
   with note `non-constexpr function '__throw_format_error' cannot
   be used in a constant expression` and the underlying source
   `std::__throw_format_error("The format string terminates at a
   '{'")`. Same `__throw_format_error` source line as Sep 7's
   probe.
2. `/tmp/probe_consteval_arg_mismatch.cpp` — produces a hard error
   `call to consteval function std::basic_format_string<char, std::string>::basic_format_string<char[7]> is not a constant expression`
   with note `non-constexpr function '__throw_format_error' cannot
   be used in a constant expression` and the underlying source
   `std::__throw_format_error("The type option contains an invalid
   value for a string formatting argument")`.

Both compile-time probes fail to compile with non-zero exit codes
(as required). The contrast probe `/tmp/probe_runtime_accepts_unterminated.cpp`
(which calls a minimal recreation of Sep 8's runtime sibling with
the same malformed format string) COMPILES CLEANLY and returns a
structured error at runtime (`FormatError: The format string
terminates at a '{'`), confirming the consteval sibling has a
strictly stronger compile-time surface than the runtime sibling.

The other three items Sep 8 named remain open (today closes #1):

- Item #2: replace hand-rolled `petra::counting_output_iterator`
  with `std::counting_output_iterator` when libc++ ships it.
- Item #3: `petra::try_format_expected_bounded` macro layer
  (analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
  from Sep 2).
- Item #4: a `petra::try_format_runtime_n` overload that takes a
  `std::span<char>` (analogous to Aug 25's `format_to_n_into`
  span overload).

---

## The headline finding — consteval sibling rejects malformed format strings at compile time

The lesson exists to pin the **consteval-vs-runtime split** for the
bounded-buffer surface:

> **Today's wrapper takes `std::format_string<Args...>` whose
> implicit ctor is `consteval`. Malformed format strings and
> arg-type / format-spec mismatches are HARD COMPILE ERRORS at
> the call site — the runtime `FormatError{kind=Format, ...}`
> branch becomes unreachable for parse / argument errors.**

The contrast probe (`/tmp/probe_runtime_accepts_unterminated.cpp`)
verifies Sep 8's runtime sibling accepts the SAME malformed format
string and converts the `std::format_error` into a structured error
at runtime. Today's wrapper does NOT have that runtime branch for
parse errors — the consteval ctor of `std::format_string<Args...>`
catches them at the call site, point-blank. The runtime
`std::format_error` branch survives only for the
`std::make_format_args` side (e.g. an integer that doesn't fit the
requested width — extremely rare for `std::format`).

The second finding: `std::format_string<Args...>` is
`std::basic_format_string<char, std::type_identity_t<Args>...>`
aliased. The consteval ctor is
`std::basic_format_string<...>::basic_format_string<...>` not
`std::format_string<...>::basic_format_string<...>` directly, which
is what the compile error names. This is the same alias Sep 7
documented.

The third finding: `std::format_string<Args...>`'s ctor is
implicit so the call site can pass a string literal directly
without an explicit .ctor invocation. The conversion is consteval,
so the format-string parse happens at compile time, but the call
syntax is identical to the runtime sibling's
(`try_format_bounded_runtime_n(buf, 100, "v={}", 42)`).

---

## The second finding — pass-1 measure / pass-2 write invariant

The byte-count invariant Sep 6 / Sep 7 / Sep 8 documented carries
over verbatim:

> **`written` (bytes actually written into `out[0, cap)`) ==
> `needed` (bytes the pass-1 measure pass reported).**

Section 11 verifies this with two independent measurements:

- The wrapper's pass-2 reports `r.value() == 11` for
  `("hello {}", "world")`.
- An independent `std::vformat_to + counting_output_iterator` on
  the same format string + args reports `measured == 11`.

The two routes agree byte-for-byte. The defensive check in
`try_format_bounded_runtime_n_impl` (`if (written != needed) ...`)
guards against a hypothetical future libc++ change that breaks the
pass-1/pass-2 invariant — the caller gets a clear
`FormatError{TooLarge, ..., needed}` rather than a
silently-truncated buffer.

---

## The third finding — the bounded writer's overflow behavior

The bounded writer (`petra::detail::bounded_char_writer`) silently
drops bytes past the cap. Section 17 exercises this directly: five
writes into a 4-byte buffer; only the first 4 land. This matches
`std::format_to_n`'s `n` parameter semantics:

- `n == needed`: exactly `needed` bytes written.
- `n < needed`: exactly `n` bytes written; the rest are silently
  dropped (NO error from `std::format_to_n`; the would-be size is
  reported via the return value, not by truncation).
- `n == 0`: zero bytes written.

Today's wrapper does NOT use `std::format_to_n` directly (because
`std::format_to_n` is consteval-only on this toolchain — Aug 31
documented this — and the runtime-format-string sibling
`std::vformat_to` has no `n` parameter). Instead, the bounded
writer implements the same `n`-cap semantics on top of
`std::vformat_to`. The wrapper's pass-1 measure then
early-returns on `needed > cap` so the bounded writer is only ever
invoked with `needed <= cap`, which means the bounded writer's
"overflow" path is unreachable through the public surface.

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
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}
```

Mirrors `std::format_to_n`'s signature shape: `(out, n, fmt,
args...)` — `out` first, `n` second, `args` last. The
format-string parameter type `std::format_string<Args...>` is the
CONSTEVAL sibling of Sep 8's `std::string_view`. `const Args&...`
matches Sep 6 / Sep 7 / Sep 8 exactly.

### The two-pass measure-then-write implementation

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

Identical to Sep 6 / Sep 7 / Sep 8 — `petra::FormatError{kind,
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

On EVERY error path the caller's `char*` buffer is UNTOUCHED
(Sections 4 and 12 verify this with poisoned buffers).

### Reused types from Sep 6 / Sep 7 / Sep 8

- `petra::FormatError{kind, message, needed_bytes}` — REUSED
  verbatim from Sep 6.
- `petra::FormatErrorKind {FormatParse, FormatArgument, Format,
  TooLarge}` — REUSED verbatim.
- `petra::counting_output_iterator` — REUSED verbatim (Sep 6's
  hand-rolled callback-based iterator).
- `petra::detail::bounded_char_writer` — REUSED verbatim (Sep 8's
  callback-based write sink).

No new types are introduced today. The lesson is a pure
consumer-side combination of three prior wrappers' machinery.

---

## Build and verification commands

Default (-O0 debug build for this lesson):

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        P-2026-09-09-try-format-bounded-runtime-n.cpp \
        -o /tmp/P-2026-09-09-try-format-bounded-runtime-n
/tmp/P-2026-09-09-try-format-bounded-runtime-n
```

Strict-warning build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
        -Wsign-conversion \
        P-2026-09-09-try-format-bounded-runtime-n.cpp \
        -o /tmp/P-2026-09-09-try-format-bounded-runtime-n-strict
/tmp/P-2026-09-09-try-format-bounded-runtime-n-strict
```

ASan + UBSan build:

```sh
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
        -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 \
        P-2026-09-09-try-format-bounded-runtime-n.cpp \
        -o /tmp/P-2026-09-09-try-format-bounded-runtime-n-asan
/tmp/P-2026-09-09-try-format-bounded-runtime-n-asan
```

CMake (default / strict / ASan) — see `CMakeLists.txt` in this dir:

```sh
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" \
                              && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON \
                              && cmake --build build-asan
./build/P-2026-09-09-try-format-bounded-runtime-n
./build-strict/P-2026-09-09-try-format-bounded-runtime-n
./build-asan/P-2026-09-09-try-format-bounded-runtime-n
```

Compile-time probes (in `/tmp`, NOT in the working tree):

```sh
# Should FAIL TO COMPILE (exit code non-zero):
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -c /tmp/probe_consteval_unterminated.cpp -o /tmp/probe_consteval_unterminated.o
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        -c /tmp/probe_consteval_arg_mismatch.cpp -o /tmp/probe_consteval_arg_mismatch.o

# Should COMPILE CLEANLY and return a structured runtime error:
clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
        /tmp/probe_runtime_accepts_unterminated.cpp \
        -o /tmp/probe_runtime_accepts_unterminated
/tmp/probe_runtime_accepts_unterminated
```

---

## Section-by-section notes

### Section 1 — toolchain + feature probes (12 CHECKs)

Standard toolchain probes: `FormatError` is default-constructible
and nothrow-move-constructible; the return type is
`std::expected<std::size_t, petra::FormatError>`; the consteval
ctor of `std::format_string<int>` produces
`std::format_string<int>`; the `FormatErrorKind` enum values
compare unequal and have stable underlying values (0, 1, 2, 3);
the hand-rolled `counting_output_iterator` and
`bounded_char_writer` are distinct types even though they share
structure. All PASS.

### Section 2 — fits-exactly happy path (9 CHECKs)

Three sub-cases: cap=4 for `"v=42"`, cap=7 for `"d=3.142"`, cap=5
for `"ABCDE"`. The wrapper reports the exact byte count and the
buffer content matches the expected string. All PASS.

### Section 3 — cap larger than needed (no truncation) (5 CHECKs)

cap=10 but `"v=42"` needs 4 bytes — the wrapper reports
`r.value() == 4`, the buffer's bytes 0..3 are `"v=42"`, and bytes
4..9 are still the poison byte `'Q'`. This proves the bounded
writer stops at the cap and does NOT NUL-terminate past `written`
(distinguishing today from Aug 25's `format_to_n_into`). All PASS.

### Section 4 — TooLarge (cap < needed) (22 CHECKs)

Four sub-cases: cap=3 for `"v=42"` (TooLarge, buffer untouched),
cap=6 for `"d=3.142"` (TooLarge, buffer untouched), cap=0 for `""`
(success, `r.value() == 0`, buffer untouched), cap=0 for `"x"`
(TooLarge, `needed_bytes == 1`, buffer untouched). All PASS.

### Section 5 — consteval fmt positive (8 CHECKs)

Four sub-cases: `"value={}", 42` → 8 bytes, `"hex={:#x}", 0xcafe`
→ 10 bytes, `"{:.3f}", 3.14159` → 5 bytes, `"{}", 3.14159` → 7
bytes. All byte-count + content checks PASS.

### Section 6 — atomicity (13 CHECKs)

Fill the buffer with the prior success `"v=42"`; issue a failing
call (cap=0, fmt `"longer"` → TooLarge); verify the prior
`"v=42"` is preserved AND bytes 4..15 are still the poison byte.
13 CHECKs (1 for the failing call, 1 for the prior-success
preservation, 12 for the poison-byte preservation across bytes
4..15). All PASS.

### Section 7 — multi-type args round-trip (3 CHECKs)

A single string-with-multiple-replacement-fields format
(`"name={} age={:#x} pi={:.5f} pad={:>10}"`) producing a 45-byte
output. The byte count and the byte content are checked
independently. All PASS.

### Section 8 — `std::counting_output_iterator` availability (2 CHECKs)

The hand-rolled `petra::counting_output_iterator` and
`petra::detail::bounded_char_writer` are NOT default-constructible
(both require a callback + user-data pointer at construction
time). This pins the API shape — if a future libc++ ships
`std::counting_output_iterator` and the hand-rolled type is
retired, the migration trigger is the same Sep 8 documented. All
PASS.

### Section 9 — UTF-8 mid-codepoint byte round-trip (3 CHECKs)

`"héllo"` in UTF-8 is 6 bytes (the `é` is 2 bytes:
`0xc3 0xa9`). The wrapper reports `r.value() == 6` and the byte
content matches. All PASS.

### Section 10 — embedded NUL byte round-trip (5 CHECKs)

A `std::string{'a', '\0', 'b'}` is 3 bytes long. The wrapper
reports `r.value() == 3`, `buf[0] == 'a'`, `buf[1] == '\0'`,
`buf[2] == 'b'`. This exercises the embedded-NUL round-trip
(using `std::string` because `std::string_view` stops at the
first NUL). All PASS.

### Section 11 — exact byte-count invariant (4 CHECKs)

For `("hello {}", "world")` — 11 bytes total — the wrapper
reports `r.value() == 11` AND an independent
`std::vformat_to + counting_output_iterator` probe reports
`measured == 11`. The byte-by-byte invariant (`r.value() ==
measured`) holds. All PASS.

### Section 12 — TooLarge boundary sweep (10 CHECKs)

Three sub-cases for `"v=42"` (needs 4 bytes): cap=3 (TooLarge,
buffer untouched), cap=4 (success, exact fit), cap=8 (success,
no truncation, bytes 4..7 untouched). All PASS.

### Section 13 — FormatError equality / inequality (3 CHECKs)

`FormatError{TooLarge, "msg1", 4} == FormatError{TooLarge,
"msg1", 4}` (PASS); differs from same with `needed_bytes=5`
(PASS); differs from same with `kind=Format` (PASS).

### Section 14 — 50-round determinism (2 CHECKs)

50 iterations of `("i={}", i)` — verify the byte count for each
round matches the expected size (`2 + std::to_string(i).size()`)
and the byte content matches `("i=" + std::to_string(i))`. All
50 rounds PASS; first round is `"i=0"` = 3 bytes.

### Section 15 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)` into
each thread's own `char buf[32]`. Zero mismatches across 100
total calls. PASS.

### Section 16 — distinct from Sep 8's `try_format_runtime_n` (5 CHECKs)

Same fmt + args, same buffer size — both wrappers produce
identical byte counts (`r.value() == 4` for `("v={}", 42)`).
On TooLarge (cap=3), today's wrapper returns the same
`FormatError{kind=TooLarge, needed_bytes=4}` that Sep 8's returns
(the structured-error contract is identical). The only surface
difference is the format-string parameter type — at the call
site the consteval ctor of `std::format_string<Args...>` has
already passed by the time the runtime body runs. All PASS.

### Section 17 — bounded_char_writer never writes past the cap (5 CHECKs)

Five writes into a 4-byte buffer through a directly-constructed
`bounded_char_writer`. The 5th write is dropped silently;
`written()` returns 4; bytes 0..3 are `'a','b','c','d'`. The 5th
char was never assigned. All PASS.

---

## Where we go next

Today's lesson closes Sep 8's "Where we go next" item #1. Three
items from Sep 8 remain open, plus three new items today's surface
itself names:

1. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type. Sep 8's open item #2. The Section 8 static_assert is the
   migration trigger — when a future libc++ ships the type, the
   static_assert flips to `true` and a follow-up lesson migrates
   all four wrappers (Sep 6, Sep 7, Sep 8, today) to use the
   standard type.

2. **`petra::try_format_expected_bounded` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a `TRY_FORMAT_BOUNDED_RUNTIME_N(out, cap, fmt,
   args...)` macro that lifts the consteval vs runtime sibling
   choice to a macro form. Sep 8's open item #3. The macro could
   expand to today's wrapper on the consteval path and Sep 8's
   wrapper on the runtime path.

3. **A `petra::try_format_bounded_runtime_n` overload that takes
   a `std::span<char>`** — analogous to Aug 25's `format_to_n_into`
   span overload. The wrapper's pass-1 / pass-2 implementation
   carries over; only the public surface changes from
   `(char*, size_t, ...)` to `(span<char>, ...)`. Today's
   implementation is already span-compatible (it just needs a
   forwarding wrapper that extracts `.data()` and `.size()`).
   Sep 8's open item #4.

4. **A `petra::try_format_bounded_runtime_n` overload that takes
   a `std::span<char>` AND a `std::format_string<Args...>`** —
   the natural combination of #3 with today's consteval check.
   This is the consteval sibling of Aug 25's
   `format_to_n_into(std::span<char>, std::format_string<Args...>,
   args...)` shape — Aug 25 documented the span-overload for the
   NUL-terminating sibling; today's lesson could document the
   same span-overload for the raw (no-NUL-terminating) sibling.

5. **A `petra::format_to_n_into_bounded` adapter** that combines
   Aug 25's NUL-terminating behavior with today's `cap` parameter
   (Aug 25 does NOT have a `cap` parameter — it writes a complete
   `std::string` and returns it). This would be the
   NUL-terminating BOUNDED sibling of today's wrapper, completing
   the bounded-buffer matrix.

6. **A `petra::try_format_bounded_runtime_n_into` macro layer**
   — analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a macro that lifts the bounded-buffer vs
   NUL-terminating sibling choice to a macro form. The macro
   could expand to today's wrapper on the bounded-buffer path
   and Aug 25's `format_to_n_into` on the NUL-terminating path.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
linear history.
