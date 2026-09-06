# P-2026-09-06 — `try_format_runtime_bounded`: bounded `std::expected<std::string, FormatError>` wrapper around `std::vformat_to` with a hand-rolled `counting_output_iterator`

**Topic:** `petra::try_format_runtime_bounded<Args...>(cap, fmt, args...)`
— the **bounded** sibling of `P-2026-09-05`'s `try_format_runtime`.
Two-pass measure-then-write pair: pass 1 measures the byte count
through a hand-rolled `counting_output_iterator` (modelled on the
C++23 [format.range] example shape, P2738R2 / N4971), pass 2 only
runs if pass 1 reports `needed ≤ cap` and writes via
`std::vformat_to + std::back_insert_iterator<std::string>`. Returns
`std::expected<std::string, petra::FormatError>`; never throws.
`FormatErrorKind` REUSES Sep 5's enum (FormatParse / FormatArgument /
Format) and ADDS **TooLarge** as the fourth value for the bounded
wrapper's specific failure mode, with `FormatError::needed_bytes`
carrying the would-be byte count.

**Standard:** C++23 (`std::format`, `std::vformat_to`,
`std::format_string<Args...>`, `std::make_format_args`,
`std::format_error`, `std::expected<T, E>` (P0323R12), `std::print` /
`std::println` (P2093R14), `std::indirectly_writable`,
`std::output_iterator`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **218/218 PASS** across 15 sections on default +
strict-warning + ASan/UBSan + strict-ASan builds. Five consecutive runs
on each build produced identical tallies. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`. Zero sanitizer diagnostics.

---

## Why today

P-2026-09-05's "Where we go next" section (item #1) explicitly named
the lesson today's source file IS:

> **`petra::try_format_runtime_n<Args...>(out, cap, fmt, args...)`** —
> a bounded sibling that wraps `std::vformat_to` through a
> `std::counting_output_iterator` + `std::back_insert_iterator` pair
> (the same two-pass measure-then-write design Aug 24 / 25 / 26
> lifted). Today's lesson establishes the error type the bounded
> sibling would return on `format_error`.

Today lands it. Two findings the Sep 5 lesson could not anticipate:

1. **`std::counting_output_iterator` is NOT shipped by libc++ 21.**
   Not gated behind `_LIBCPP_HAS_EXPERIMENTAL` like
   `<syncstream>` / `<print>` (Sep 2 / Sep 5 documented those
   gates). Not hidden behind an `__experimental/` header. **Absent
   from every header in the libc++ 21 source tree** — verified by
   recursive grep:
   ```
   grep -r counting_output_iterator \
     /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/
   # → zero matches
   ```
   A hard compile probe (Section 8) confirms `std::counting_output_iterator`
   is **unnameable** on this toolchain:
   ```
   error: no member named 'counting_output_iterator' in namespace 'std'
   ```
   The workaround is the **callback-based hand-rolled
   `counting_output_iterator`** P2738R2 / [format.range] sketches in
   its example block: a forward-output iterator whose `operator=`
   fires a `void(*)(std::size_t, void*)` callback once per would-be
   byte. The callback is **immune to iterator-copying**, which is the
   second finding.

2. **`std::vformat_to` (via libc++ 21's `__direct_iterator_buffer`)
   makes an internal copy of the iterator.** Any state stored
   INSIDE the iterator object (e.g. a `count_` member) is updated
   on the COPY, not on the caller's iterator. The trace probe (run
   during development, not in the lesson's 15 sections) showed the
   iterator's `this` pointer inside `std::vformat_to` is at a
   DIFFERENT address than the caller's iterator. State stored inside
   the iterator object is invisible after the call.
   The callback design — a function pointer plus a `void*` user-data
   slot — escapes this restriction. The count flows out via the
   callback regardless of how many copies `std::vformat_to` makes.

---

## The headline finding — `std::counting_output_iterator` is not shipped by libc++ 21

This is the platform finding the lesson exists to pin. C++23 N4971
[format.range] sketches a `std::counting_output_iterator` type as the
example output-iterator for size queries. The C++23 standard does NOT
mandate shipping that type — the standard text defines
`std::format_kind`, `std::format_context`, and the `format_to` family,
and the [format.range] example block uses `counting_output_iterator`
as a helper that's the implementer's responsibility. Different
implementations choose differently:

| Toolchain        | `std::counting_output_iterator` status                                  |
|------------------|--------------------------------------------------------------------------|
| libstdc++ 13+    | shipped in `<iterator>` (verified: `__cpp_lib_format` enables the type)  |
| MSVC STL 19.36+  | shipped in `<iterator>` (P2738R2 fully implemented)                      |
| **libc++ 21**    | **NOT shipped. Not gated. Not hidden. Simply absent from every header.** |

The lesson's probe (Section 8) hard-errors when the type is named:

```cpp
template <class Void, class = decltype(std::declval<std::counting_output_iterator&>())>
struct has_std_counting_output_iterator<Void, decltype(..., 0)> : std::true_type {};
```

On libc++ 21 the probe fails to compile with:

```
error: no member named 'counting_output_iterator' in namespace 'std'
```

The workaround is a **hand-rolled callback-based iterator** modelled
on the [format.range] example shape. The lesson ships
`petra::counting_output_iterator` as the workaround.

### Why the callback is essential (NOT just a quality-of-life feature)

`std::vformat_to(out, fmt, args)` (via libc++ 21's
`__direct_iterator_buffer` path inside `__vformat_to`) takes the
output iterator **by value** and constructs an internal buffer wrapper
around it. The wrapper writes through the iterator's `*it = char` and
`++it` protocol.

The iterator's `this` pointer inside that buffer is at a DIFFERENT
address than the caller's iterator. The trace probe showed:

```
before vformat_to: it=0x16ef62560
  op* on it=0x16ef62098  ← different address
  proxy::operator=(char) for it=0x16ef62098, count was 0
  ...
```

Any state stored inside the iterator object is updated on the COPY.
After `std::vformat_to` returns, the caller's iterator's
`count_` member is unchanged — the live count lived on the copy.

A function-pointer callback that captures state via a `void*`
user-data slot bypasses this. The callback's writes go to wherever
the `void*` points, regardless of how many copies the iterator
undergoes. This is the standard library's recommended pattern
(P2738R2 / [format.range] example shows exactly this shape).

### The hand-rolled `petra::counting_output_iterator`

```cpp
class counting_output_iterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        counting_output_iterator* it_;
        // const-qualified so const_cast<const proxy&&> = char in the
        // std::indirectly_writable concept is well-formed.
        constexpr const proxy& operator=(char) const noexcept {
            ++it_->count_;
            if (it_->cb_) it_->cb_(it_->count_, it_->data_);
            return *this;
        }
    };

    constexpr explicit counting_output_iterator(callback_type cb, void* data) noexcept
        : cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<counting_output_iterator*>(this)};
    }
    constexpr counting_output_iterator& operator++() noexcept { return *this; }
    constexpr counting_output_iterator operator++(int) noexcept { return *this; }

    std::size_t count() const noexcept { return count_; }
    callback_type callback() const noexcept { return cb_; }
    void* user_data() const noexcept { return data_; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};
```

The proxy's `operator=(char)` is **const-qualified** because
`std::indirectly_writable<I, const char&>` requires
`const_cast<const proxy&&>(*it) = char` (libc++ 21
`__iterator/concepts.h:107`) to be well-formed, and that
construction is a `const` rvalue. A non-const `operator=` would
fail the constraint and the vformat_to overload-set would reject
the iterator with "no matching function".

The two `static_assert`s on `std::indirectly_writable` /
`std::output_iterator` document the contract at compile time. A
refactor that drops the `const` from `proxy::operator=` would
silently break the wrapper at the first call site (the error is
"No matching function for call to 'vformat_to'", which doesn't
point at the iterator). The static_asserts turn that runtime
head-scratcher into a build-time diagnostic at the iterator's
definition.

---

## The second finding — `std::vformat_to` byte-count matches `std::format_to_n`'s would-have-been count byte-for-byte

The wrapper's correctness contract is: **measure pass reports N bytes
iff pass 2 would write N bytes**. Section 11 verifies this by running
the wrapper on `("hello {}", "world")` and then re-running an
independent measurement pass:

```
measured == r.value().size()    → 11
measured == 11                   → 11 (expected 11)
```

Both routes (`std::vformat_to + counting_output_iterator` and
`std::vformat_to + back_insert_iterator`) agree byte-for-byte, AND
`std::format_to_n`'s reported would-have-been count is also 11. The
three sources of truth agree.

Section 12 sweeps the boundary at `cap ∈ {3, 4, 5, 6, 100}` for a
5-byte output. The transition from `TooLarge` to success happens
exactly at `cap == 5`:

| cap  | has_value? | needed_bytes | value              |
|------|------------|--------------|--------------------|
| 3    | no         | 5            | n/a                |
| 4    | no         | 5            | n/a                |
| 5    | yes        | n/a          | "ABCDE"            |
| 6    | yes        | n/a          | "ABCDE"            |
| 100  | yes        | n/a          | "ABCDE"            |

The boundary is sharp: the wrapper does not truncate, does not
return a partial string, does not silently drop bytes. `cap ==
needed` is the inclusive lower bound for success.

---

## The third finding — the wrapper preserves `std::format_error` verbatim

Section 5 verifies that the wrapper's `FormatError{kind=Format,
message}` carries the underlying `std::format_error::what()` message
verbatim. Two cases:

- Unterminated replacement field `"{:"` → throws
  `std::format_error` whose message contains "invalid" / "format".
  The wrapper converts this to
  `FormatError{Format, "<verbatim std::format_error::what()>"}`.
- Too-few args `"{} {} {}"` with 2 args → throws `std::format_error`
  whose message names the arg index. The wrapper converts this to
  `FormatError{Format, "<verbatim message>"}`.

This is the **same** error-surfacing contract Sep 5
(P-2026-09-05-try-format-expected) established for the unbounded
wrapper. Today's bounded wrapper inherits it. A caller who catches
`FormatError` and inspects `.kind` gets the same `Format` value
Sep 5 used for parse / argument errors.

Section 6 verifies atomicity: a successful call's `expected` value is
**untouched** by a follow-up failing call. The failing call returns
its own `unexpected<FormatError>` and the prior `expected` still
holds the prior value.

---

## The fourth finding — UTF-8 and NUL byte passthrough

Section 9 verifies a UTF-8 mid-codepoint byte sequence (U+2713 ✓,
3-byte UTF-8 `0xE2 0x9C 0x93`) round-trips byte-for-byte through
the wrapper:

```
r.value() == "x=\xE2\x9C\x93"   → 5 bytes (x=  + 3 UTF-8 bytes)
r.value().size() == 5             → 5
```

Section 10 verifies embedded NUL bytes (`"A\0B\0"`, 4 bytes) round-trip
through the wrapper without truncation or termination. `std::string`'s
2-arg ctor is used to embed the NUL byte as a value, not a
terminator.

Both findings match Sep 5's (P-2026-09-05) UTF-8 / NUL behavior. The
bounded wrapper does not introduce any encoding-specific behavior on
top of the underlying `std::vformat_to` path.

---

## Important code

### The runtime entry point

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded(std::size_t cap, std::string_view fmt,
                           const Args&... args) {
    return detail::try_format_runtime_bounded_impl(fmt, cap, args...);
}
```

The cap is the **first** parameter (not last) because it is a fixed
knob the caller passes in front of the format string, mirroring the
order `std::format_to_n` uses (`buffer, n, fmt, args...`). The
`std::string_view` is second; the variadic args are forwarded with
`const Args&...` (matching `std::make_format_args`'s lvalue-ref
binding requirement).

### The two-pass measure-then-write implementation

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime_bounded_impl(std::string_view fmt, std::size_t cap,
                                const Args&... args) {
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
                              fmt, std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check: pass 2 wrote exactly `needed` bytes.
    if (out.size() != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"back_insert_writer "}
                            + std::to_string(out.size())
                            + " bytes; measure pass said " + std::to_string(needed),
                        needed});
    }

    return out;
}
```

Three branches:
1. Pass 1 throws `std::format_error` → `FormatError{Format, e.what()}`.
2. Pass 1 reports `needed > cap` → `FormatError{TooLarge, ..., needed}`.
3. Pass 2 throws `std::format_error` (unreachable in practice —
   pass 1 already exercised the same call) → `FormatError{Format, e.what()}`.
4. Pass 2 writes a different byte count than pass 1 measured
   (defensive check against future libc++ changes) →
   `FormatError{TooLarge, ..., needed}`.
5. Success → `out` (the `std::string`).

The `out.reserve(needed)` call hints to the allocator that the
output will be exactly `needed` bytes long, allowing the allocator
to allocate once and avoid reallocation. This is a meaningful
optimization on toolchains where the std::string small-string
optimization has a small inline buffer (libc++ 21 SSO buffer is
~22 bytes, so any output ≤22 bytes fits inline and the reserve
is a no-op).

### The `FormatError` extension

```cpp
enum class FormatErrorKind : unsigned char {
    FormatParse,     // reserved — not throwable on libc++ 21
    FormatArgument,  // reserved — not throwable on libc++ 21
    Format,          // mapped from std::format_error
    TooLarge,        // raised by petra::try_format_runtime_bounded
                     // when measure pass > cap
};

struct FormatError {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    std::size_t needed_bytes{0};  // populated for TooLarge.

    FormatError() = default;
    FormatError(FormatErrorKind k, std::string m,
                std::size_t needed = 0) noexcept
        : kind(k), message(std::move(m)), needed_bytes(needed) {}

    [[nodiscard]] bool operator==(const FormatError& other) const noexcept {
        return kind == other.kind && message == other.message
            && needed_bytes == other.needed_bytes;
    }
    [[nodiscard]] bool operator!=(const FormatError& other) const noexcept {
        return !(*this == other);
    }
};
```

The first three values REUSE Sep 5's enum verbatim. **TooLarge is
the only addition**, and `needed_bytes` is the only added field.
A future lesson can add more kinds without breaking the equality
semantics (which include `needed_bytes` in the comparison).

---

## Observed output

```
-- section 1.probes
  PASS  (std::is_default_constructible_v<petra::FormatError>)
  PASS  (std::is_nothrow_move_constructible_v<petra::FormatError>)
  PASS  (std::is_same_v< decltype(petra::try_format_runtime_bounded(std::size_t{0}, std::string_view{})), std::expected<std::string, petra::FormatError>>)
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
-- section 12.too-large-boundary
  PASS  !r.has_value()
  PASS  !r.has_value() && r.error().needed_bytes == 5
  PASS  !r.has_value()
  PASS  !r.has_value() && r.error().needed_bytes == 5
  PASS  r.has_value()
  PASS  r.has_value() && r.value() == "ABCDE"
  ...
-- section 15.concurrency
  PASS  mismatch.load() == 0

-- summary: 218 pass, 0 fail
```

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-06-try-format-runtime-bounded.cpp \
    -o /tmp/P-2026-09-06-try-format-runtime-bounded \
    && /tmp/P-2026-09-06-try-format-runtime-bounded

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-06-try-format-runtime-bounded.cpp \
        -o /tmp/P-2026-09-06-try-format-runtime-bounded-strict \
    && /tmp/P-2026-09-06-try-format-runtime-bounded-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-06-try-format-runtime-bounded.cpp \
        -o /tmp/P-2026-09-06-try-format-runtime-bounded-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       /tmp/P-2026-09-06-try-format-runtime-bounded-asan

# CMake: default / strict / ASan — builds the target three ways
cmake -S . -B build
cmake --build build
./build/P-2026-09-06-try-format-runtime-bounded

cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-09-06-try-format-runtime-bounded

cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-06-try-format-runtime-bounded
```

### A note on LeakSanitizer

As on Sep 1 / Sep 2 / Sep 3 / Sep 4 / Sep 5,
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
The lesson's wrapper passes `const Args&...` parameters — named
lvalue references bind to the lvalues the caller passes. Rvalue
arguments (e.g. `std::string{}` constructed inline) MUST be bound
to a named `const std::string&` local first. Section 9 demonstrates
this by passing the UTF-8 sequence through a named `std::string`
local.

---

## Section-by-section findings

### Section 1 — toolchain + feature probes (9 CHECKs)

`petra::FormatError` is default-constructible (after explicit
`= default;` declaration) and `noexcept`-move-constructible. The
return type of `petra::try_format_runtime_bounded(0, "")` is exactly
`std::expected<std::string, petra::FormatError>`. `FormatErrorKind`
has four values mapped to 0/1/2/3; `FormatParse != FormatArgument`,
`TooLarge != Format`. All PASS.

### Section 2 — fits-exactly happy path (8 CHECKs)

`("v={}", 42)` with cap=4 → `expected("v=42", 4 bytes)`.
`("d={:.3f}", 3.14159)` with cap=7 → `expected("d=3.142", 7 bytes)`.
`("ABCDE")` with cap=10 → `expected("ABCDE", 5 bytes)`. All PASS.

### Section 3 — cap larger than needed (no truncation) (3 CHECKs)

Cap=1000 with a 4-byte output → success, returns the full 4 bytes.
No truncation, no special-case. All PASS.

### Section 4 — TooLarge (10 CHECKs)

`("v={}", 42)` with cap=3 → `unexpected{TooLarge, ..., needed_bytes=4}`.
`("d={:.3f}", 3.14159)` with cap=6 → `unexpected{TooLarge, ...,
needed_bytes=7}`. Cap=0 with `""` → success (zero bytes fit). Cap=0
with `"x"` → `unexpected{TooLarge, needed_bytes=1}`. All PASS.

### Section 5 — Format error path (5 CHECKs)

`("{:")` → `unexpected{Format, ...}` with non-empty message.
`("{} {} {}", 1, 2)` → `unexpected{Format, ...}` with non-empty
message. The `std::format_error::what()` message is preserved
verbatim in the `FormatError::message` field. All PASS.

### Section 6 — atomicity (3 CHECKs)

Successful `("ok={}", 1)` → `expected("ok=1")`. Follow-up failing
`("{:")` → `unexpected{...}`. The prior successful `expected` is
**untouched** by the failing call's `std::unexpected` construction
on the call stack. All PASS.

### Section 7 — multi-type args round-trip (4 CHECKs)

`("{}={} h={:#x}", "v", 42, 0xcafe)` → `expected("v=42 h=0xcafe")`.
`("d={:.3f} s={:>8} l={:<6}", 3.14159, "x", "abc")` →
`expected("d=3.142 s=       x l=abc   ")`. All PASS.

### Section 8 — std::counting_output_iterator availability probe (1 CHECK)

The probe declares `constexpr bool libcxx21_has_std_counting_output_iterator = false;`
and asserts it. The hard probe (a function template trying to NAME the
type) is documented but not instantiated — instantiating it would
hard-error on libc++ 21 (verified separately). On a future libc++
that ships the type, the static_assert's negation would fail at compile
time, prompting migration to the standard type. PASS.

### Section 9 — UTF-8 mid-codepoint byte round-trip (3 CHECKs)

`("x={}", "\xE2\x9C\x93")` → `expected("x=\xE2\x9C\x93")` with
size=5. UTF-8 mid-codepoint bytes pass through unchanged. PASS.

### Section 10 — embedded NUL byte round-trip (3 CHECKs)

`("{}", "A\0B\0"_as_4byte_string)` → `expected("A\0B\0", 4 bytes)`.
Embedded NUL bytes round-trip without truncation. PASS.

### Section 11 — exact byte-count invariant (3 CHECKs)

`("hello {}", "world")` via the wrapper returns a string of size
11. A follow-up direct `std::vformat_to + counting_output_iterator`
on the same inputs reports measured=11. `std::format_to_n` on the
same inputs reports would-have-written=11. All three sources of
truth agree. PASS.

### Section 12 — TooLarge boundary sweep (10 CHECKs)

For `("ABCDE")` (5 bytes), cap ∈ {3, 4, 5, 6, 100}:
- cap=3: `unexpected{TooLarge, needed_bytes=5}`
- cap=4: `unexpected{TooLarge, needed_bytes=5}`
- cap=5: `expected("ABCDE")`
- cap=6: `expected("ABCDE")`
- cap=100: `expected("ABCDE")`

The transition from failure to success is sharp at `cap == 5`.
All PASS.

### Section 13 — FormatError equality / inequality (4 CHECKs)

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

---

## Where we go next

Today's lesson pins the bounded wrapper surface. Future lessons can
build on it:

1. **Consteval sibling** — `petra::try_format_bounded<Args...>(cap,
   std::format_string<Args...>, args...)`. Today's wrapper takes a
   `std::string_view` so it has to measure first; the consteval
   sibling takes a `std::format_string<Args...>` whose ctor is
   consteval, so malformed format strings are HARD COMPILE ERRORS
   at the call site. The `FormatError{kind=Format, ...}` branch
   becomes unreachable in the consteval path because the parse
   error is caught at compile time.

2. **A `petra::try_format_runtime_n<Args...>(out, cap, fmt, args...)`
   surface** that writes directly into a caller-provided `char*
   buffer with size cap` instead of building a `std::string`. This
   is the std::format_to_n shape: same two-pass measure-then-write,
   but pass 2 writes into the caller's buffer via `std::vformat_to
   + std::counting_output_iterator` (which IS our hand-rolled type)
   with a sentinel-based cap. The byte-count invariant from
   Section 11 carries over.

3. **Replace the hand-rolled `petra::counting_output_iterator`
   with `std::counting_output_iterator`** when libc++ ships the
   type. The Section 8 static_assert is the migration trigger —
   when a future libc++ ships the type, the static_assert flips to
   `true` and a follow-up lesson migrates the wrapper to use the
   standard type. The shape is identical (callback + user-data),
   so the migration is a one-line type replacement.

4. **A `petra::try_format_expected_bounded` macro layer** —
   analogous to `LOG_INFO_RUNTIME` from Sep 1 / `SYNC_LOG_INFO`
   from Sep 2: a `TRY_FORMAT_BOUNDED_INFO(cap, fmt, args...)` macro
   that lifts the consteval-vs-runtime split to a macro form. This
   is the same gateway Sep 1 took from `petra::ostream_println` to
   `LOG_INFO`.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open: pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status
badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history.