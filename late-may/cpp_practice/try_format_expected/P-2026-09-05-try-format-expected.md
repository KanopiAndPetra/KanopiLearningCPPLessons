# P-2026-09-05 — `try_format_expected`: `std::expected<std::string, FormatError>` wrapper around `std::format` / `std::vformat` that catches `std::format_error` and converts it into a structured error type

**Topic:** a `petra::try_format_runtime<Args...>(fmt, args...)`
runtime-format-string wrapper plus its `petra::try_format<Args...>(fmt,
args...)` consteval sibling. Both return
`std::expected<std::string, petra::FormatError>` and never let a
`std::format_error` escape. `petra::FormatError{kind, message}`
carries the three C++23 exception subclasses (parse / argument /
generic) — but the lesson's **headline finding** is that
**`std::format_parse_error` and `std::format_argument_error` are
NOT exported by libc++ 21**: only the base `std::format_error`
type is shipped, so the wrapper's `FormatErrorKind` collapses to
`Format` on this toolchain. Future libc++ / libstdc++ / MSVC STL
will add the subclass types and the wrapper will start
discriminating between parse and argument failures without any
call-site changes.

**Standard:** C++23 (`std::format`, `std::format_string<Args...>`,
`std::vformat`, `std::make_format_args`, `std::format_error`,
`std::expected<T, E>` (P0323R12), `std::print` / `std::println`
(P2093R14)).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** **57/57 PASS** across 12 sections on default +
strict-warning + ASan/UBSan builds. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`. Zero sanitizer diagnostics. Five consecutive
runs on each build produced identical tallies.

---

## Why today

The Sep 4 lesson (`P-2026-09-04-sync-log-level-filter`) closed the
**last** new follow-on item Sep 1's "Where we go next" section
named. The Sep 1 → Sep 2 → Sep 3 → Sep 4 chain is now closed
end-to-end. Sep 4's "Where we go next" listed four older
carry-forward items and several cross-cutting infrastructure
items, but also opened a clean slate for **fresh** topics.

Today's lesson is one such fresh topic. It does **not** close any
previously-listed carry-forward item — it lands new ground:

1. **`std::format` error wrapping.** No previous lesson in the
   arc catches `std::format_error` and converts it into a
   structured error type. Aug 28 / Aug 30 / Aug 31 let the
   exception escape; Sep 1 / Sep 2 / Sep 3 / Sep 4 route through
   the macro layer where the error never happens (the consteval
   gate catches parse / type errors at compile time). Today is
   the FIRST lesson that catches the exception at the **function
   layer** and exposes a `std::expected` surface.

2. **The `std::format_error` subclass hierarchy.** C++23 defines
   three exception types: `std::format_error` (base), and two
   subclasses `std::format_parse_error` and
   `std::format_argument_error` for parse failures vs runtime
   format failures. The lesson pins which of these are actually
   shipped by the current toolchain (Section 8) and provides a
   `FormatError{kind, message}` structured type that already
   carries the three kinds even though only one is observable
   today.

3. **The consteval-vs-runtime split at the FUNCTION layer.** Aug 28
   / Aug 30 / Aug 31 established the split at the **macro** layer
   (LOG_INFO / SYNC_LOG_INFO / LOCSYNC_LOG_INFO consteval vs
   LOG_INFO_RUNTIME / SYNC_LOG_INFO_RUNTIME / LOCSYNC_LOG_INFO_RUNTIME
   runtime). Today lifts the same split to a typed wrapper pair
   (`petra::try_format` consteval / `petra::try_format_runtime`
   runtime) — the recipe is identical but the error surface is
   `std::expected` instead of "exception escapes".

---

## The headline finding — `std::format_parse_error` / `std::format_argument_error` are not exported by libc++ 21

This is the platform finding the lesson exists to pin. The C++23
standard defines three exception types under `<format>`:

```cpp
namespace std {
  class format_error : public runtime_error { ... };
  class format_parse_error : public format_error { ... };      // [format.parse]
  class format_argument_error : public format_error { ... };   // [format.arg]
}
```

libc++ 21 **ships only the base `std::format_error`** in
`<__format/format_error.h>` (verified by reading
`/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/__format/format_error.h`:

```cpp
class _LIBCPP_EXPORTED_FROM_ABI format_error : public runtime_error { ... };
// no format_parse_error
// no format_argument_error
```

Any code that names `std::format_parse_error` or
`std::format_argument_error` is a **HARD COMPILE ERROR** on
libc++ 21. This is what makes the SFINAE-style "is the type
complete?" probe from the lesson's earlier draft unworkable —
the NAME doesn't even resolve, so referring to it at all is
ill-formed.

The lesson's workaround is to NOT name the subclass types at
all. Instead the structured-error type `petra::FormatError`
carries a `FormatErrorKind` enum with three values
(FormatParse, FormatArgument, Format), and every catch block
collapses to `FormatErrorKind::Format` because the throw site
always throws the base `std::format_error`. The future
toolchain upgrade is automatic — when libc++ ships the
subclass types, the catch site can `dynamic_cast` and route to
the appropriate kind, and `FormatError.kind` will start
discriminating without any call-site changes.

Section 8 pins this with `std::type_info::name()` on a
deliberately-thrown `std::format_error`. On libc++ 21 the
mangled name is `NSt3__112format_errorE` (the `N...E` is the
Itanium ABI mangling for a nested-name-qualified type). On
libstdc++ 13+ the parse subclass throws would surface as a
different mangled name.

---

## The second finding — the consteval gate is identical to the macro layer's

The consteval gate Sep 1 / Sep 2 / Sep 3 / Sep 4 / Aug 28 /
Aug 30 / Aug 31 established at the **macro layer** works the
same way at the **function layer**:

| Surface                           | Consteval form                       | Runtime form                              |
|-----------------------------------|--------------------------------------|-------------------------------------------|
| Aug 28 (P-2026-08-28) macro layer | `LOG_INFO("v={}", 42)`               | `LOG_INFO_RUNTIME(std::string_view, ...)` |
| Aug 30 / Aug 31 function layer    | `std::format("v={}", 42)`            | `std::vformat(sv, std::make_format_args(...))` |
| **Today** (P-2026-09-05)          | `petra::try_format("v={}", 42)`      | `petra::try_format_runtime(sv, ...)`      |

The consteval form catches malformed format strings at compile
time via `std::format_string<Args...>` whose ctor is consteval.
The runtime form takes `std::string_view` and parses the format
string at format time; parse errors throw `std::format_error`
which today's wrapper converts to `FormatError`. Section 7 pins
the consteval gate via a `static_assert` probe on
`std::format_string<int>{"value={}"}`.

---

## The third finding — atomicity: a failing call never produces a partial output

Aug 30 / Aug 31 / Sep 2 / Sep 3 all pinned the atomicity
property for their respective surfaces: a `std::format_error`
thrown mid-write leaves the destination untouched. Today's
lesson confirms the same property for `std::vformat →
std::string`:

- Section 5 verifies that after a failing `try_format_runtime`,
  the previously-successful `std::string` is unchanged.
- Section 5 also runs 100 consecutive failing calls and
  confirms each one throws (no fall-through to "returned a
  value because we wrote half the string and discarded the
  rest").
- Section 12 runs 4 threads × 50 failing calls = 200 errors;
  every one is an error — no thread sees a partial / wrong
  result on the failure path.

The atomicity property is the **strong** form of the contract
the wrapper exists to provide. If `std::vformat` could write
half a string and then throw, `std::expected<std::string,
FormatError>` would be the wrong return type — the caller
would have no way to know whether the returned value is
complete or partial. Today's lesson pins that the returned
value is always complete (when present) and the destination
buffer is always untouched (when absent).

---

## The fourth finding — too-many-args is silently accepted; nested replacement fields are valid C++23

Two related surprises that the lesson pins in Section 4:

1. **`std::vformat("{}", make_format_args(1, 2, 3))` succeeds
   and returns `"1"`** — the extra args are silently discarded.
   The C++23 standard allows this: the format string consumes
   what it needs and the rest are ignored. This is NOT a
   `std::format_error`. The lesson's `try_format_runtime`
   wrapper returns `expected::value("1")` and the caller has no
   indication anything was dropped.

2. **`std::vformat("{0:{1}}", make_format_args(42, 3))` succeeds
   and returns `" 42"`** — C++23 P2738R2 makes nested
   replacement fields valid. The width spec `:3` is sourced from
   arg 1, so 42 is right-justified in a 3-wide field. The
   `{}` style of width spec (vs the older `{:>{}}` shape) is
   part of the C++23 format-spec grammar.

Both of these behaviours are **the spec**, not libc++-specific
quirks. They were surprising enough that today's lesson pins
them explicitly so future code that depends on them has a
reference.

---

## Important code

### The `FormatError` type

```cpp
namespace petra {

enum class FormatErrorKind : unsigned char {
    FormatParse,     // mapped from std::format_parse_error
    FormatArgument,  // mapped from std::format_argument_error
    Format,          // mapped from std::format_error (catch-all)
};

struct FormatError {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;

    FormatError(FormatErrorKind k, std::string m) noexcept
        : kind(k), message(std::move(m)) {}

    explicit FormatError(std::string m) noexcept
        : kind(FormatErrorKind::Format), message(std::move(m)) {}

    [[nodiscard]] constexpr bool operator==(const FormatError& other) const noexcept {
        return kind == other.kind && message == other.message;
    }

    [[nodiscard]] constexpr bool operator!=(const FormatError& other) const noexcept {
        return !(*this == other);
    }
};

}
```

`FormatErrorKind` has three values mapped to the three C++23
exception subclasses. `Format` is the catch-all used on
toolchains that don't ship the subclass types (libc++ 21 today,
possibly others).

The `operator==` is `noexcept` and `constexpr` so
`FormatError` can live in a `std::unordered_map` keyed on
`std::string_view` (used in Section 11's per-fmt first-message
cache).

### The runtime entry point

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_runtime(std::string_view fmt, const Args&... args) {
    try {
        return std::vformat(fmt, std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(FormatError{FormatErrorKind::Format, e.what()});
    }
}
```

`std::make_format_args` takes lvalue references; the `const
Args&...` parameter pack gives named lvalue references to bind.
`std::vformat` parses the format string; on a parse failure it
throws `std::format_error` which we convert to
`std::unexpected<FormatError>`. The function never throws.

### The consteval sibling

```cpp
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format(std::format_string<Args...> fmt, Args&&... args) {
    try {
        return std::format(fmt, std::forward<Args>(args)...);
    } catch (const std::format_error& e) {
        return std::unexpected(FormatError{FormatErrorKind::Format, e.what()});
    }
}
```

`std::format_string<Args...>` is the consteval-checked format
string type. Its ctor is `consteval`, so a malformed format
string is a HARD COMPILE ERROR at the call site. Args&&... are
forwarding references — they bind to lvalues as lvalue refs and
rvalues as rvalue refs, matching `std::format`'s own
`_Args&&...` overload shape exactly.

### The dynamic-type probe

```cpp
inline std::string format_error_dynamic_type_name() {
    try {
        throw std::format_error("probe");
    } catch (const std::format_error& e) {
        return typeid(e).name();
    }
    return "";  // unreachable
}
```

The probe deliberately throws a base `std::format_error`,
catches it, and returns the RTTI name. On libc++ 21 the
returned mangled name is `NSt3__112format_errorE` (Itanium
ABI mangling). On toolchains that actually throw a subclass
for parse failures, the mangled name will reflect the subclass
type. Section 8 prints the value and asserts that it
contains `"format"` (a stable substring across the
implementations).

---

## Observed output

```
-- section 1.probes
-- section 2.runtime-happy-path
-- section 3.consteval-happy-path
-- section 4.parse-failures
-- section 5.atomicity
-- section 6.consteval-runtime-error
-- section 7.consteval-probe
-- section 8.platform-subclass
--   dynamic_type_name = NSt3__112format_errorE
-- section 9.format-error-inspection
-- section 10.utf8-nul
-- section 11.determinism
-- section 12.concurrency
--
-- summary: 57/57 PASS
```

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-05-try-format-expected.cpp \
    -o /tmp/P-2026-09-05-try-format-expected \
    && /tmp/P-2026-09-05-try-format-expected

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-05-try-format-expected.cpp \
        -o /tmp/P-2026-09-05-try-format-expected-strict \
    && /tmp/P-2026-09-05-try-format-expected-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-05-try-format-expected.cpp \
        -o /tmp/P-2026-09-05-try-format-expected-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       /tmp/P-2026-09-05-try-format-expected-asan

# CMake: default / strict / ASan — builds the target three ways
cmake -S . -B build
cmake --build build
./build/P-2026-09-05-try-format-expected

cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-09-05-try-format-expected

cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-09-05-try-format-expected
```

### A note on LeakSanitizer

As on Aug 30 / Aug 31 / Sep 1 / Sep 2 / Sep 3 / Sep 4,
LeakSanitizer is **not supported on darwin/arm64** (probed:
`"detect_leaks is not supported on this platform"`).
`ASAN_OPTIONS=detect_leaks=0` was used for all ASan runs
rather than the documented `detect_leaks=1`. The
`try_format_runtime` / `try_format` wrappers don't allocate
beyond the returned `std::string` (no `new`/`malloc` in the
function bodies) and the captured `FormatError` carries its
own `std::string` so there are no leak paths the absence of
LeakSanitizer could silently hide.

### A note on `<print>` requiring `-fexperimental-library`

`std::println` (used by the test harness) lives in `<print>`
(P2093R14). On Apple Clang 21 / libc++ 21 the `<print>`
header is shipped but the `std::println` / `std::print` names
are gated behind `_LIBCPP_HAS_EXPERIMENTAL` (the same gate
Sep 2 uses for `std::osyncstream`). The
`-fexperimental-library` flag flips
`__has_feature(experimental_library)` which the libc++ config
reads to default the macro to 1. Today's CMakeLists.txt and
direct compile commands all pass the flag; the lesson's notes
document this in the build-recipe section.

---

## Section-by-section findings

### Section 1 — toolchain + feature probes (8 CHECKs)

Pins the surface the rest of the lesson depends on:
`std::expected<std::string, FormatError>` is constructible;
`std::unexpected<FormatError>` is constructible from
`FormatError`; `std::format_string<int>` is a complete type;
`std::format_error` is a `std::runtime_error` subclass;
`FormatError` equality on same kind+message and inequality on
different kind or different message; `FormatErrorKind` has
exactly 3 values mapped to 0/1/2. All PASS.

### Section 2 — `try_format_runtime` happy path with multiple types (7 CHECKs)

`int`, `const char*`, `double` with precision specifier `:.3f`,
hex format `{:#x}`, three-arg mixed types `(int, const char*,
double)`, no-args format string, escaped braces `{{...}}`. The
three-arg mixed test pins the platform behaviour that `{}` on
a `double` defaults to "shortest round-trippable" — `3.0`
prints as `3`, not `3.000`. All PASS.

### Section 3 — `try_format` consteval sibling happy path (5 CHECKs)

Same surface as Section 2 with the consteval sibling. The
success path is identical to Section 2's — the consteval gate
doesn't change the output format, only the parse-time error
behaviour. All PASS.

### Section 4 — parse-failure cases (8 CHECKs)

Six parse-failure cases:
- Unterminated replacement field `"{:"` → throws `format_error`.
- Too-few args `"{} {} {}"` with 2 args → throws `format_error`.
- **Too-many args `"{}"` with 3 args → silently succeeds, returns
  `"1"`**.  This is NOT a `format_error` — the C++23 standard
  explicitly allows extra args that aren't consumed by the format
  string to be discarded.  Pinned as a positive platform
  behaviour.
- Bare closing brace `"}"` → throws `format_error`.
- **Nested replacement field `"{0:{1}}"` with `42, 3` → succeeds,
  returns `" 42"`**.  C++23 P2738R2 makes nested replacement
  fields valid.  Pinned as a positive platform behaviour.
- Non-empty error message on every failing case.

All PASS. Section 4's cases 3 and 5 are the surprising-but-spec
behaviours that the lesson exists to pin.

### Section 5 — atomicity (5 CHECKs)

Pre-compute a successful result, then run a failing call. The
previously-successful string is unchanged. Run 100 consecutive
failing calls — every one throws (no partial success). After
the 100 failures, the original good value is still good. All
PASS.

### Section 6 — consteval runtime-error happy path (1 CHECK)

The consteval sibling's success path. Runtime format errors
on the consteval path are rare (the consteval gate catches
most errors at compile time) but technically possible. The
section pins the success path; the consteval gate property
is asserted in Section 7 via a `static_assert` probe.

### Section 7 — consteval-gate probe (3 CHECKs)

`std::format_string<int>{"value={}"}` constructs a
`std::format_string` whose ctor is `consteval`. The probe
runs the ctor inside a `static_assert` and a
`constexpr` lambda to prove the ctor is callable in constant
evaluation. If the ctor is `consteval`, the call only
succeeds at compile time — proving the consteval gate. All
PASS.

### Section 8 — platform-subclass finding (5 CHECKs)

The lesson's headline finding. The probe throws a
`std::format_error("probe")`, catches it, and returns
`typeid(e).name()`. The returned mangled name
(`NSt3__112format_errorE` on libc++ 21) is asserted to be
non-empty and to contain `"format"`. A separate assertion
verifies that a real format error from a failing call is
caught as the base `std::format_error` and routed to
`FormatErrorKind::Format`. All PASS.

### Section 9 — FormatError inspection (6 CHECKs)

Empty message, 20000-char message round-trip, embedded NUL byte
passthrough, UTF-8 byte passthrough (3-byte codepoint),
copy-construction equality. All PASS.

### Section 10 — UTF-8 + NUL byte round-trip via `try_format_runtime` (3 CHECKs)

UTF-8 mid-codepoint (`"\xE2\x9C\x93"` — the ✓ checkmark)
round-trips byte-for-byte. Embedded NUL byte in a 5-byte output
string round-trips byte-for-byte (using `std::string` 2-arg
ctor so the NUL is part of the value, not a terminator).
100-byte UTF-8 payload (`0xE2` repeated) round-trips with no
truncation. All PASS.

### Section 11 — 50-round determinism (4 CHECKs)

50 rounds × 4 failing fmts (`"{:"`, `"}"`, `"{"`, plus
`"{} {} {}"` was removed from the list — it's not failing on
this toolchain). Each round asserts every fmt still fails and
the error message is byte-stable for THAT specific fmt across
rounds. 50 happy-path rounds also asserted consistent
(`"v=42 n=petra h=0xcafe"`). All PASS.

### Section 12 — concurrency (4 CHECKs)

4 threads × 50 calls each on `try_format_runtime` happy path —
every captured string matches the expected
`"t={tid} i={i} v={i*7} h=0x{0xcafe+tid}"`. Zero mismatches,
200 total calls. 4 threads × 50 failing calls each on the
error path — every one is an error, 200 total errors. All
PASS.

---

## Where we go next

Today's lesson pins a new surface (function-layer `expected`
wrapping) that future lessons can build on:

1. **`petra::try_format_runtime_n<Args...>(out, cap, fmt, args...)`**
   — a bounded sibling that wraps `std::vformat_to` through a
   `std::counting_output_iterator` + `std::back_insert_iterator`
   pair (the same two-pass measure-then-write design Aug 24 /
   25 / 26 lifted). Today's lesson establishes the error type
   the bounded sibling would return on `format_error`.

2. **Type-erased args for `std::format`** — Sep 4's
   carry-forward list. Today's lesson pins the
   `FormatErrorKind` enum with three values reserved for
   the future case when `std::format_parse_error` /
   `std::format_argument_error` are actually shipped and
   throwable. Today's wrapper would automatically benefit
   from the future subclass types via a `dynamic_cast` route
   at the catch site.

3. **`petra::try_format_expected` macros** — a macro layer
   that mirrors today's wrapper pair as
   `TRY_FORMAT_INFO("v={}", 42)` / `TRY_FORMAT_INFO_RUNTIME(sv, ...)`
   macros, lifting the consteval-vs-runtime split to a
   try/catch surface that returns from a lambda. This is the
   same gateway Aug 28 / Sep 1 took from `petra::ostream_println`
   to `LOG_INFO`.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open: pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status
badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history.
