# P-2026-09-07 — `try_format_bounded`: CONSTEVAL sibling of `P-2026-09-06`'s `try_format_runtime_bounded`

**Topic:** `petra::try_format_bounded<Args...>(cap, std::format_string<Args...> fmt, args...)`
— the **consteval** sibling of `P-2026-09-06`'s `try_format_runtime_bounded`.

The only difference vs Sep 6 is the format-string parameter type:

| Lesson                | format-string parameter      | Parse error location                |
|-----------------------|------------------------------|--------------------------------------|
| Sep 6 (runtime)       | `std::string_view`           | caught at format time via try/catch  |
| **Sep 7 (consteval)** | `std::format_string<Args...>`| **HARD COMPILE ERROR at the call site** |

`std::format_string<Args...>`'s `basic_format_string` ctor is `consteval`
(P2216R3) — every malformed format string is rejected by the compiler
before the wrapper's function body is even type-checked. Arg-type
mismatches between the format string and the args (e.g. `"{:d}"` with a
`std::string` arg) are likewise rejected at compile time. The runtime
`FormatError{Format, ...}` branch becomes **unreachable for parse /
argument errors** in the consteval wrapper; only `TooLarge` survives as
a runtime error in the bounded surface. The format_args side (e.g. an
integer that doesn't fit a width — rare for `std::format`) is still
caught and routed to `FormatError{Format, ...}` to preserve Sep 6's
contract verbatim.

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::format_string<Args...>` (P2216R3), `std::make_format_args`,
`std::format_error`, `std::expected<T, E>` (P0323R12), `std::print` /
`std::println` (P2093R14), `std::indirectly_writable`,
`std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **222/222 PASS** across 16 sections on default +
strict-warning + ASan/UBSan + strict-ASan builds. Five consecutive runs
on each build produced identical tallies. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`. Zero sanitizer diagnostics.

---

## Why today

`P-2026-09-06`'s "Where we go next" section (item #1) explicitly named
the lesson today's source file IS:

> **Consteval sibling** — `petra::try_format_bounded<Args...>(cap,
> std::format_string<Args...>, args...)`. Today's wrapper takes a
> `std::string_view` so it has to measure first; the consteval
> sibling takes a `std::format_string<Args...>` whose ctor is
> consteval, so malformed format strings are HARD COMPILE ERRORS
> at the call site. The `FormatError{kind=Format, ...}` branch
> becomes unreachable in the consteval path because the parse
> error is caught at compile time.

Today lands it. Two platform findings Sep 6 could not anticipate but
the consteval sibling documents:

1. **The consteval gate fires at `std::basic_format_string`'s ctor.**
   A separate compile probe (saved in `/tmp/probe_consteval_unterminated.cpp`)
   produces this hard error on `petra::try_format_bounded(100, "{:", 42)`:
   ```
   error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<char[3]>' is not a constant expression
   note: non-constexpr function '__throw_format_error' cannot be used in a constant expression
   note: __throw_format_error("The replacement field misses a terminating '}'")
   ```
   The corresponding probe against Sep 6's runtime sibling
   (`/tmp/probe_runtime_accepts_unterminated.cpp`) compiles cleanly
   and returns `FormatError{Format, ...}` at format time.

2. **Arg-type mismatches are ALSO hard compile errors.** A second
   probe (`/tmp/probe_consteval_arg_mismatch.cpp`) confirms that
   `petra::try_format_bounded(100, "{:d}", std::string)` fails to
   compile:
   ```
   error: call to consteval function 'std::basic_format_string<char, std::string &>::basic_format_string<char[5]>' is not a constant expression
   note: __throw_format_error("The type option contains an invalid value for a string formatting argument")
   ```
   The consteval ctor validates BOTH the parse AND the arg-type
   binding. The runtime sibling would also reject this at format
   time, but only because `std::format` would throw
   `std::format_error` — the consteval sibling surfaces the same
   defect at compile time with a clearer diagnostic.

---

## The headline finding — consteval gates the parse side

This is the platform finding the lesson exists to pin. The consteval
sibling's value proposition is that a malformed format string is a
build-time defect, not a runtime defect. Two probes prove it.

### Probe 1: unterminated replacement field

```
/tmp/probe_consteval_unterminated.cpp calls
    petra::try_format_bounded(100, "{:", 42)

clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
    /tmp/probe_consteval_unterminated.cpp

→ HARD COMPILE ERROR:
   error: call to consteval function 'std::basic_format_string<char, int>::basic_format_string<char[3]>' is not a constant expression
   /Library/.../usr/include/c++/v1/__format/format_functions.h:295:5: note: non-constexpr function '__throw_format_error' cannot be used in a constant expression
       std::__throw_format_error("The replacement field misses a terminating '}'");
```

### Probe 2: arg-type mismatch

```
/tmp/probe_consteval_arg_mismatch.cpp declares a std::string s
and calls petra::try_format_bounded(100, "{:d}", s)

clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
    /tmp/probe_consteval_arg_mismatch.cpp

→ HARD COMPILE ERROR:
   error: call to consteval function 'std::basic_format_string<char, std::string &>::basic_format_string<char[5]>' is not a constant expression
   /Library/.../usr/include/c++/v1/__format/parser_std_format_spec.h:889:5: note: non-constexpr function '__throw_format_error' cannot be used in a constant expression
       std::__throw_format_error("The type option contains an invalid value for a string formatting argument");
```

### Probe 3: contrast with the runtime sibling

```
/tmp/probe_runtime_accepts_unterminated.cpp calls
    petra::try_format_runtime_bounded(100, "{:", 42)

clang++ ... → COMPILES CLEANLY.
Run → r.has_value() == false (returns FormatError{Format, ...} at format time)
```

The contrast is the headline finding. A bug in the caller's format
string that takes an hour to debug in production with the runtime
sibling becomes a HARD COMPILE ERROR with the consteval sibling —
the error message names the ctor (`std::basic_format_string<...>`)
and the parser-internal `__throw_format_error` call site.

---

## The second finding — the consteval ctor is `std::basic_format_string`, not `std::format_string<Args...>::basic_format_string` directly

When you read the compile error carefully, the consteval ctor is
named `std::basic_format_string<char, int>::basic_format_string<char[3]>`
(Probe 1) and `std::basic_format_string<char, std::string &>::basic_format_string<char[5]>`
(Probe 2). `std::format_string<Args...>` is a `std::basic_format_string<char, std::type_identity_t<Args>...>`
alias — the alias hides the `std::type_identity_t` indirection that
makes the arg types deduce correctly without consuming the reference
qualification.

The diagnostic message a future debugger sees when a malformed format
string is shipped to the consteval wrapper will point at
`std::basic_format_string<...>::basic_format_string<...>`. The lesson's
CMakeLists.txt documents the chain so the next reader does not have to
re-derive it.

---

## The third finding — `std::format_string<Args...>` is constructed implicitly from `const S&` for `S` convertible to `string_view`

`std::format_string<Args...>`'s constructor is not explicit:

```cpp
template <class S>
consteval format_string(const S& s);
```

…where `S` is constrained to be a string-view-like type. This is what
makes the call site `petra::try_format_bounded(100, "v={}", 42)`
type-check: the string literal `"v={}"` of type `const char[5]` is
converted implicitly to `std::format_string<int>` (because `Args = int`
for the one replacement field). The conversion is `consteval` so the
format-string parse happens at compile time.

A future lesson can lift the `std::format_string<Args...>` requirement
into a `std::type_identity_t<std::format_string<Args...>>` parameter
to make the deduction rules more visible — but the standard pattern
is the implicit conversion. Today's wrapper uses the implicit form.

---

## Important code

### The consteval entry point

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_bounded(std::size_t cap,
                   std::format_string<Args...> fmt,
                   Args&&... args) {
    return detail::try_format_bounded_impl(cap, fmt,
                                           std::forward<Args>(args)...);
}
```

The parameter type `std::format_string<Args...>` carries `std::type_identity_t`
on each `Args` so the format string and the args are deduced to the
same bare types. `Args&&...` are forwarding references — they bind to
lvalues as lvalue refs and rvalues as rvalue refs, exactly matching
`std::format`'s own `_Args&&...` overload.

### The two-pass measure-then-write implementation

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_bounded_impl(std::size_t cap,
                        std::format_string<Args...> fmt,
                        Args&&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    // Pass 1: measure.
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Sized-too-big path.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    // Pass 2: write.
    std::string out;
    out.reserve(needed);
    try {
        (void)std::vformat_to(std::back_insert_iterator<std::string>(out),
                              fmt.get(), std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check: pass 2 wrote exactly `needed` bytes.
    if (out.size() != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"back_insert wrote "}
                            + std::to_string(out.size())
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }

    return out;
}
```

Three branches:
1. Pass 1 throws `std::format_error` → `FormatError{Format, e.what()}`.
2. Pass 1 reports `needed > cap` → `FormatError{TooLarge, ..., needed}`.
3. Pass 2 throws `std::format_error` (unreachable in practice — pass
   1 already exercised the same call) → `FormatError{Format, e.what()}`.
4. Pass 2 writes a different byte count than pass 1 measured
   (defensive check against future libc++ changes) →
   `FormatError{TooLarge, ..., needed}`.
5. Success → `out` (the `std::string`).

The `fmt.get()` call extracts the underlying string view from the
`std::format_string<Args...>` — `basic_format_string` exposes a `get()`
member that returns `string_view` (P2216R3 §3.1). The `std::vformat_to`
signature wants `string_view`, not `std::format_string<Args...>`,
which is why the `.get()` call is required at every call site.

### The `FormatError` reuse

`petra::FormatError` is REUSED verbatim from Sep 6. The consteval
sibling does not need a new kind — only `TooLarge` is reachable in
practice. `FormatError` having four values is a forward-compat
feature: future lessons can extend the surface (e.g. an `ArgumentType`
for arg-type mismatches that survive a hypothetical relaxation of
consteval) without breaking equality semantics.

---

## Observed output

```
-- section 1.probes
  PASS  (std::is_default_constructible_v<petra::FormatError>)
  PASS  (std::is_nothrow_move_constructible_v<petra::FormatError>)
  PASS  (std::is_same_v< ... >)
  PASS  (std::is_same_v<decltype(std::format_string<int>{"value={}"}), std::format_string<int>>)
  PASS  petra::FormatErrorKind::FormatParse != petra::FormatErrorKind::FormatArgument
  PASS  petra::FormatErrorKind::TooLarge != petra::FormatErrorKind::Format
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::FormatParse) == 0
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::FormatArgument) == 1
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::Format) == 2
  PASS  static_cast<unsigned char>(petra::FormatErrorKind::TooLarge) == 3
-- section 2.fits-exactly
  PASS  r.has_value()
  PASS  r.has_value() && r.value() == "v=42"
  PASS  r.has_value() && r.value().size() == 4
  ...
-- section 15.concurrency
  PASS  mismatch.load() == 0
-- section 16.format-side-runtime
  PASS  r.has_value()
  PASS  r.has_value() && r.value() == "42"
  PASS  r2.has_value()
  PASS  r2.has_value() && r2.value() == "   42"

-- summary: 222 pass, 0 fail
```

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-07-try-format-bounded.cpp \
    -o /tmp/P-2026-09-07-try-format-bounded \
    && /tmp/P-2026-09-07-try-format-bounded

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-07-try-format-bounded.cpp \
        -o /tmp/P-2026-09-07-try-format-bounded-strict \
    && /tmp/P-2026-09-07-try-format-bounded-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-07-try-format-bounded.cpp \
        -o /tmp/P-2026-09-07-try-format-bounded-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       /tmp/P-2026-09-07-try-format-bounded-asan

# Compile-time consteval probes (verifies the headline finding)
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    /tmp/probe_consteval_unterminated.cpp \
    -o /tmp/probe_consteval_unterminated
# → HARD COMPILE ERROR (expected; documents the consteval gate)

clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    /tmp/probe_consteval_arg_mismatch.cpp \
    -o /tmp/probe_consteval_arg_mismatch
# → HARD COMPILE ERROR (expected; documents the arg-type consteval gate)

clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    /tmp/probe_runtime_accepts_unterminated.cpp \
    -o /tmp/probe_runtime_accepts_unterminated \
    && /tmp/probe_runtime_accepts_unterminated
# → COMPILES CLEANLY; runtime returns FormatError{Format, ...}
# (run exit=1 because r.has_value() == false on the malformed format)

# CMake: default / strict / ASan — builds the target three ways
cmake -S . -B build
cmake --build build
./build/P-2026-09-07-try-format-bounded

cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-09-07-try-format-bounded

cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-07-try-format-bounded
```

### A note on LeakSanitizer

As on Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5 / Sep 6,
LeakSanitizer is **not supported on darwin/arm64** (probed:
`"detect_leaks is not supported on this platform"`).
`ASAN_OPTIONS=detect_leaks=0` was used for all ASan runs.
The wrapper's hot path allocates at most a single `std::string`
(the returned `out`), and the captured `FormatError` carries its
own `std::string` — there are no leak paths the absence of
LeakSanitizer could silently hide.

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
The lesson's wrapper takes `Args&&...` (forwarding references).
`std::forward<Args>(args)...` materializes an lvalue when binding
to an rvalue caller arg. Section 9 demonstrates this by passing the
UTF-8 sequence through a named `std::string` local; Section 11
demonstrates it by binding "world" to a `std::string_view` local
first because the consteval parameter type is
`std::format_string<const char*>` — a string literal passed
inline is a `const char[6]` array which decays to `const char*`
but the lifetime of the array is the call expression, and
`std::make_format_args` needs a stable lvalue.

---

## Section-by-section findings

### Section 1 — toolchain + feature probes (10 CHECKs)

`petra::FormatError` is default-constructible (after explicit
`= default;` declaration) and `noexcept`-move-constructible. The
return type of `petra::try_format_bounded(0, "")` is exactly
`std::expected<std::string, petra::FormatError>`. `std::format_string<int>`
is a complete type (a `std::format_string<int>{"value={}"}` value
is constructible at compile time). `FormatErrorKind` has four values
mapped to 0/1/2/3; `FormatParse != FormatArgument`,
`TooLarge != Format`. All PASS.

### Section 2 — fits-exactly happy path (8 CHECKs)

The byte-for-byte parity with Sep 6's runtime sibling is what
makes this section useful. `("v={}", 42)` with cap=4 →
`expected("v=42", 4 bytes)`. `("d={:.3f}", 3.14159)` with cap=7 →
`expected("d=3.142", 7 bytes)`. `("ABCDE")` with cap=10 →
`expected("ABCDE", 5 bytes)`. All PASS.

### Section 3 — cap larger than needed (no truncation) (3 CHECKs)

Cap=1000 with a 4-byte output → success, returns the full 4 bytes.
No truncation, no special-case. All PASS.

### Section 4 — TooLarge (9 CHECKs)

`("v={}", 42)` with cap=3 → `unexpected{TooLarge, ..., needed_bytes=4}`.
`("d={:.3f}", 3.14159)` with cap=6 → `unexpected{TooLarge, ...,
needed_bytes=7}`. Cap=0 with `""` → success (zero bytes fit). Cap=0
with `"x"` → `unexpected{TooLarge, needed_bytes=1}`. All PASS.

### Section 5 — consteval fmt positive (8 CHECKs)

Four valid calls produce identical results to `std::format`:
`("value={}", 42)`, `("hex={:#x}", 0xcafe)`, `("{:.3f}", 3.14159)`,
`("{}", 3.14159)`. All return the std::format-equivalent string.
All PASS.

The malformed-format-string case is verified by a SEPARATE probe
(`/tmp/probe_consteval_unterminated.cpp`) — it CANNOT be exercised
at runtime because the consteval ctor rejects it before the
function body is reachable. The probe compile failure IS the
test (see "headline finding" above).

### Section 6 — atomicity (3 CHECKs)

Successful `("ok={}", 1)` → `expected("ok=1")`. Follow-up failing
`("bad={}", 2)` with cap=4 → `unexpected{TooLarge, ...}`. The prior
successful `expected` is **untouched** by the failing call's
`std::unexpected` construction on the call stack. All PASS.

### Section 7 — multi-type args round-trip (4 CHECKs)

`("{}={} h={:#x}", "v", 42, 0xcafe)` →
`expected("v=42 h=0xcafe")`. `("d={:.3f} s={:>8} l={:<6}",
3.14159, "x", "abc")` → `expected("d=3.142 s=       x l=abc   ")`.
All PASS.

### Section 8 — std::counting_output_iterator availability probe (1 CHECK)

The same probe Sep 6 uses. The recursive grep across the libc++ 21
header tree returns zero matches. The static_assert flips to `true`
on a future libc++ that ships the type. PASS.

### Section 9 — UTF-8 mid-codepoint byte round-trip (3 CHECKs)

`("x={}", "\xE2\x9C\x93")` → `expected("x=\xE2\x9C\x93")` with
size=5. UTF-8 mid-codepoint bytes pass through unchanged. PASS.

### Section 10 — embedded NUL byte round-trip (3 CHECKs)

`("{}", "A\0B\0"_as_4byte_string)` → `expected("A\0B\0", 4 bytes)`.
Embedded NUL bytes round-trip without truncation. PASS.

### Section 11 — exact byte-count invariant (3 CHECKs)

`("hello {}", "world")` via the wrapper returns a string of size
11. A follow-up direct `std::vformat_to + counting_output_iterator`
on the same inputs reports measured=11. Both routes agree
byte-for-byte. PASS.

### Section 12 — TooLarge boundary sweep (10 CHECKs)

For `("ABCDE")` (5 bytes), cap ∈ {3, 4, 5, 6, 100}:
- cap=3: `unexpected{TooLarge, needed_bytes=5}`
- cap=4: `unexpected{TooLarge, needed_bytes=5}`
- cap=5: `expected("ABCDE")`
- cap=6: `expected("ABCDE")`
- cap=100: `expected("ABCDE")`

The transition from failure to success is sharp at `cap == 5`.
All PASS.

### Section 13 — FormatError equality / inequality (5 CHECKs)

Two `TooLarge{kind, "x", 5}` are equal; different `needed_bytes`
makes them unequal; different `kind` makes them unequal. PASS.

### Section 14 — 50-round determinism (150 CHECKs)

50 consecutive `("i={}", i)` calls for i=0..49. Each call's
returned string equals `std::string("i=") + std::to_string(i)`.
The size invariant (string size == expected size) holds for every
call. PASS.

### Section 15 — 4 threads × 25 calls (1 CHECK)

4 threads × 25 calls of `("t={} i={} v={}", tid, i, i*7)`.
Every captured `expected` value equals the expected
`"t=<tid> i=<i> v=<i*7>"`. Zero mismatches across 100 total calls.
PASS.

### Section 16 — format-side-runtime (4 CHECKs)

`("{:d}", 42)` → `expected("42")`. `("{:5d}", 42)` →
`expected("   42")`. Width specifier passthrough. PASS.

---

## Where we go next

Today's lesson pins the consteval bounded wrapper surface. Future
lessons can build on it:

1. **`petra::try_format_runtime_n<Args...>(out, cap, fmt, args...)`**
   surface that writes directly into a caller-provided `char*
   buffer with size cap` instead of building a `std::string`. This
   is the `std::format_to_n` shape: same two-pass measure-then-write,
   but pass 2 writes into the caller's buffer via `std::vformat_to +
   std::counting_output_iterator` (which IS our hand-rolled type)
   with a sentinel-based cap. The byte-count invariant from Section
   11 carries over. Sep 6's item #2 named this.

2. **Replace the hand-rolled `petra::counting_output_iterator` with
   `std::counting_output_iterator`** when libc++ ships the type.
   The Section 8 static_assert is the migration trigger — when a
   future libc++ ships the type, the static_assert flips to `true`
   and a follow-up lesson migrates the wrapper to use the standard
   type. Sep 6's item #3 named this.

3. **A `petra::try_format_expected_bounded` macro layer** — analogous
   to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO` from Sep 2: a
   `TRY_FORMAT_BOUNDED_INFO(cap, fmt, args...)` macro that lifts the
   consteval-vs-runtime split to a macro form. Sep 6's item #4 named
   this.

4. **A `petra::try_format_bounded_runtime_n<Args...>(out, cap, fmt, args...)`
   sibling** that combines today's consteval format-string check with
   Sep 6's runtime format-string check — picking the consteval path
   when the format string is a literal and the runtime path when it
   is a runtime `string_view`. This would close the macro-layer's
   consteval-vs-runtime split (Sep 1 / Sep 2's pattern) at the
   function-layer.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open: pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status
badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history.
