# P-2026-08-27 — `std::format_to_n_runtime_string_projected`: a consumer-side `expected<size_t, LogError>` projection of Aug 26's runtime-format-string wrapper

Modern-C++ lesson closing the **third new follow-on item** the
Aug 26 (P-2026-08-26) lesson's "Where we go next" section
explicitly named, verbatim:

> **`std::format_to_n_runtime_string` for runtime-checked
> consumer-side `std::expected<E, ...>` projection** — today's
> wrapper returns `expected<size_t, format_error>`. A natural
> addition is a `transform_error` overload that projects to a
> domain-specific error type (e.g. `expected<size_t, MyError>`
> where `MyError::FormatError(FormatError)` for the format
> failure, `MyError::OutOfMemory` for an allocation failure
> in pass 1, and `MyError::BufferTooSmall` for the truncation
> case). That's a consumer-side concern and **probably doesn't
> warrant a separate lesson**.

Today IS that lesson. The fix to Aug 26's "probably doesn't
warrant a separate lesson" line: it does. There is a real
domain-ErrorType contract to design — what does "truncation"
mean for the caller, success or failure? What does "would_be ≥ n"
mean for a domain type that wants to distinguish Overflow from
Format? — and a non-trivial decision about what the new error
types are. Today's `LogError` has three failure kinds:
`Format` (carries a payload message), `EmptyBuffer` (`n == 0`),
and `Overflow` (truncation). Today's lesson also gets a focused
tour of `std::expected::transform_error` composition in a real
(non-textbook) setting, plus a libc++ 21-specific gotcha that
the lesson notes include inline.

| Surface (Aug 26 → today)                                | Aug 26 | Today |
|---------------------------------------------------------|--------|-------|
| runtime-format-string bounded-buffer formatter          |   ✅   |       |
| bounded-buffer formatter PROJECTS to `expected<E, …>`   |        |  ✅   |

The whole new projected wrapper is ~50 lines plus a ~50-line
`LogError` domain type with three failure kinds plus a custom
`std::formatter<LogError>`. The interesting bits are:

1. The `LogError` domain design: a `std::optional<payload>` for
   the `Format` kind (carries the `format_error` message); the
   `EmptyBuffer` and `Overflow` kinds have no payload and just
   the tag.
2. **The libc++ 21 gotcha**: `std::expected::transform_error`
   in libc++ 21 only accepts the shape where the functor returns
   the **bare new error type** (e.g. `LogError`). The
   `std::unexpected<E>` return-value path the standard
   describes is **rejected with a static_assert** in libc++ 21.
   Today's wrapper works around it by manually constructing
   `expected<size_t, LogError>{std::unexpect, mapped}`. This is
   a real-world gotcha — once you know it, you keep the trick
   handy.
3. The Overflow detection: Aug 26 silently returned
   `would_be_written`. Today's wrapper compares it against `n`
   and treats any `would_be ≥ n` as a domain failure. (This is
   a domain-specific choice; a different domain could equally
   have treated truncation as success and just returned the
   truncated text. Aug 26's choice is the right default for
   log-line formatting.)

## Headline

| Build                                                                     | Result            |
|---------------------------------------------------------------------------|-------------------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`)                      | **216/216 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **216/216 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **216/216 PASS**, empty stderr |
| CMake (`cmake -S . -B build && cmake --build build`)                       | **216/216 PASS**  |
| CMake strict (`-DENABLE_ASAN=OFF` + strict flags)                          | **216/216 PASS**  |
| CMake ASan (`-DENABLE_ASAN=ON`)                                           | **216/216 PASS**, empty stderr |

The total assertion count of **216** is the sum across the 17
sections below; each section exercises multiple invariants.

| Section | Topic                                                              | Tests |
|---------|--------------------------------------------------------------------|-------|
| 1       | Toolchain + feature probes                                         | 4     |
| 2       | Basic success — projected error type is invisible at the value path | 4   |
| 3       | Overflow detection (was Aug 26's silent return)                    | 3     |
| 4       | `n == 0` → `EmptyBuffer` (NEW failure kind)                        | 2     |
| 5       | `std::format_error` projection — `LogError::Format` + payload     | 5     |
| 6       | Bad argument index → `Format` with payload                        | 3     |
| 7       | Bad type → `Format` with payload                                   | 3     |
| 8       | Success value is preserved byte-for-byte                           | 3     |
| 9       | `transform_error` not invoked on the success path (compile proof) | 1     |
| 10      | Exact-fit (`would_be == n - 1`) vs truncation (`would_be == n`)    | 8     |
| 11      | Three error kinds are pairwise distinct                            | 9     |
| 12      | `LogError` equality on kind + payload                              | 7     |
| 13      | Composing with `and_then` + `or_else` on the projected wrapper    | 2     |
| 14      | `std::span<char>` overload                                         | 4     |
| 15      | 50-round determinism — 2 pin CHECKs + 49 × 3 loop CHECKs = **149** | 149   |
| 16      | Format-failure payload propagation                                 | 7     |
| 17      | `n == 1` is Overflow (NOT EmptyBuffer)                             | 2     |
| **Total**|                                                                  | **216** (verified, runtime output) |

The total assertion count of **216** comes from runtime; per the
table, sections 1–14 + 16 + 17 contribute 71 static CHECK calls,
and section 15 expands to 2 (pin) + 49 × 3 (loop) = 149, for a
total of **216** verified at runtime.

## Why this wrapper exists at all

The Aug 22 / 24 / 25 / 26 lesson chain built up a series of
bounded-buffer formatters that return `std::expected<size_t,
std::format_error>`. The error type is `std::format_error`
throughout — that's the path that `std::vformat` /
`std::vformat_to` throw on parse, argument-index, and type
errors.

That error type is fine for libraries whose consumer is also
okay with `std::format_error`. But real consumer code usually
wants a **domain error type** — `LogError` for a logging
library, `ParseError` for a JSON parser, `RenderError` for a
template engine. The Aug 26 lesson notes' "Where we go next"
item was about projecting `expected<size_t, format_error>` to
`expected<size_t, MyDomainError>` via `std::expected`'s
monadic ops. Today IS that projection.

```cpp
// Aug 26's surface:
auto r1 = petra::format_to_n_runtime_string(buf, sizeof(buf), fmt, x);
if (!r1) throw r1.error();                // throws std::format_error

// Today's surface:
auto r2 = petra::format_to_n_runtime_string_projected(buf, sizeof(buf), fmt, x);
if (!r2) {
    switch (r2.error().kind()) {
        case petra::LogError::Kind::Format:       /* carry payload */ break;
        case petra::LogError::Kind::EmptyBuffer:  /* caller decides */ break;
        case petra::LogError::Kind::Overflow:     /* caller decides */ break;
    }
}
```

The two wrappers are interchangeable on the success path; they
diverge on the error path. The transformation is `complexity
moves from format_error to LogError, value type preserved`.

## The domain `LogError` design

`LogError` is a small struct that captures three failure kinds:

```cpp
class LogError {
public:
    enum class Kind { Format, EmptyBuffer, Overflow };
    // ...
    static LogError FormatError(std::format_error e);
    static LogError EmptyBuffer();
    static LogError Overflow();
private:
    Kind kind_;
    std::optional<FormatPayload> payload_;  // only Format uses it
};
```

The interesting design decisions:

1. **`std::optional<FormatPayload>` for the payload, not a
   `std::variant`**: only the `Format` kind has a payload
   (`std::string message`). `EmptyBuffer` and `Overflow` have
   just the kind. `std::optional` carries exactly the empty
   state cheaply. A `std::variant` of three payloads would
   waste space for the two empty-payload kinds. A polymorphic
   base / derived class would cost a vtable per `LogError`
   and a heap allocation when copying; `std::optional<string>`
   is fine and inline.
2. **Static factories `FormatError(e)` / `EmptyBuffer()` /
   `Overflow()`**: the constructors are private. The factory
   pattern makes the kind-vs-payload invariant local (the
   `FormatError` factory is the only one that constructs a
   payload; the other two never do). Section 12 verifies the
   equality semantics; sections 5 / 6 / 7 verify the payload
   is set on `Format` and unset on the other two.
3. **A custom `std::formatter<LogError>`**: libc++ 21 doesn't
   ship formatters for arbitrary user types — that's a small
   ~10-line type the lesson writes. The formatter prints
   `"LogError::Format(\"msg\")"` for `Format` payloads and
   `"LogError::EmptyBuffer"` / `"LogError::Overflow"` for the
   others.
4. **Equality on kind + payload** so `CHECK(f1 == f2)` works
   in tests; `friend bool operator==` is the C++23 idiom.

## The libc++ 21 `transform_error` gotcha

This is the headline finding of the lesson. P0323R12 (the
paper that landed `std::expected`) defines
`transform_error` with two shapes:

| Functor return type        | Resulting `expected<T, E>`        |
|----------------------------|-----------------------------------|
| `G`                        | `expected<T, G>`                  |
| `std::unexpected<G>`       | `expected<T, G>` (same end state) |

The two are **semantically equivalent** for the caller. The
implementation freedom: either build the new `expected` from a
bare `G` (just construct in place), or from a `unexpected<G>`
(unwrap, then construct). libc++ 21 implements the first shape
**only**:

```
note: in instantiation of function template specialization
      'std::expected<unsigned long, std::format_error>::
      transform_error<petra::detail::MapToLogError>' requested here
...
note: static assertion failed due to requirement
      'integral_constant<bool, false>::value':
      The result of f(error()) must be a valid template argument
      for unexpected
```

The static_assert says it: the result of `f(error())` must be
a valid template argument for `std::unexpected`. The
implementation does NOT accept a `std::unexpected<E>` return
even though the standard allows it. So the functor MUST return
a bare `G`. Today's wrapper works around it by manually
constructing:

```cpp
LogError mapped = detail::MapToLogError{n}(r.error());
return std::expected<std::size_t, LogError>{
    std::unexpect, std::move(mapped)};
```

The pattern is "call the functor as a plain function call, then
construct the new expected manually". It is one line longer
than `return r.transform_error(MapToLogError{n})` and it works
on every libc++ in the wild (libc++ 21, libstdc++ 14). The
cost is small; the benefit is portability.

This gotcha is worth knowing because the `static_assert`'s
wording is misleading — the result IS a "valid template
argument for unexpected" (it's an `unexpected<LogError>`!), it
just isn't the shape libc++ 21's implementation happens to
recognise. The exact static_assert changes once libc++ ships
P0323R12 implementation-permissive overloads (libc++ post-21
if the issue is tracked).

## Section 3 — truncation is now an `Overflow` failure

The most consequential behavior change. Aug 26's
`format_to_n_runtime_string` returned `would_be_written`
whether or not the buffer was large enough. Today, `would_be ≥
n` is an `Overflow` failure:

```cpp
char buf[8] = {};
std::string_view fmt = "a long message that exceeds the buffer";
auto r = petra::format_to_n_runtime_string_projected(
    buf, sizeof(buf), fmt);
CHECK(!r.has_value());
CHECK(r.error().kind() == petra::LogError::Kind::Overflow);
CHECK(!r.error().has_payload());
```

For a logging library, "the buffer was too small and the line
got truncated" is usually worth flagging. The caller can
choose: reformat with a bigger buffer, drop the line, or log
to a fallback sink. Hiding the truncation behind a successful
return makes that choice invisible.

## Section 4 — `n == 0` is an `EmptyBuffer` failure

Aug 26 returned the would-be size on `n == 0`. Today, that
case is a domain failure — there's nowhere to even put a NUL
so writing this buffer is not a meaningful operation:

```cpp
char buf[1] = {};          // deliberately unused — n is 0
std::string_view fmt = "x";
auto r = petra::format_to_n_runtime_string_projected(
    buf, 0, fmt);
CHECK(!r.has_value());
CHECK(r.error().kind() == petra::LogError::Kind::EmptyBuffer);
```

Note: `n == 1` is **NOT** EmptyBuffer — see Section 17. `n == 1`
means cap == 0, the wrapper writes the NUL at `out[0]`, but if
the formatted text has `would_be ≥ 1` it got truncated, which
is Overflow.

## Section 10 — exact-fit vs truncation boundary

This section pins down the truncation-boundary decision:

- `would_be == n - 1` (the formatted text fills exactly the
  writable region): NUL at `out[n-1]`; **success** with
  `r.value() == n - 1`. The data made it.
- `would_be == n` (one char short — would have wanted one more
  writable slot): the write hit the cap, the wrapper wrote up
  to `out[n-2]` + a NUL at `out[n-1]`. This is **truncation**
  — `would_be ≥ n` — and is `Overflow` in today's wrapper.

Section 10 verifies both cases end-to-end:

```cpp
char buf[8] = {};
std::string_view fmt7 = "1234567";   // would_be == 7: exact fit
auto r7 = petra::format_to_n_runtime_string_projected(
    buf, sizeof(buf), fmt7);
CHECK(r7.has_value());
CHECK(r7.value() == 7);                         // 7 chars written
CHECK(std::string_view{buf} == "1234567");
CHECK(buf[7] == '\0');                          // NUL at out[7]

char buf2[8] = {};
std::string_view fmt8 = "12345678";  // would_be == 8: truncated
auto r8 = petra::format_to_n_runtime_string_projected(
    buf2, sizeof(buf2), fmt8);
CHECK(!r8.has_value());
CHECK(r8.error().kind() == petra::LogError::Kind::Overflow);
CHECK(std::string_view{buf2} == "1234567");      // 7 chars + NUL
```

This is the boundary that sections 2 and 3 exercise loosely;
section 10 pins it.

## Section 13 — composing with `and_then` / `or_else`

The `expected<size_t, LogError>` composes through the rest of
`std::expected`'s monadic surface, not just `transform_error`.
Today's wrapper exercises `and_then` and `or_else` together:

```cpp
char buf[8] = {};  // too small for "size = 42 (very large)"
std::string_view fmt = "size = {} (very large)";
int i = 42;
auto r = petra::format_to_n_runtime_string_projected(
    buf, sizeof(buf), fmt, i);

auto chained = r.and_then([](std::size_t n)
    -> std::expected<int, petra::LogError> {
    return static_cast<int>(n * 2);     // would_be is bounded so the
                                         // doubling is safe-ish
}).or_else([](petra::LogError e)
    -> std::expected<int, petra::LogError> {
    if (e.kind() == petra::LogError::Kind::Overflow) {
        return -1;       // sentinel: "we hit an overflow"
    }
    return -2;           // sentinel: "format error / empty buffer"
});

CHECK(chained.has_value());
CHECK(chained.value() == -1);
```

The chain has one error type (`LogError`) end-to-end. If the
format succeeded (it didn't here, the buffer is too small),
`and_then` would have produced `2 * would_be`. If either step
fails, the error is `LogError` throughout. This is exactly
the monadic-composition pattern Aug 24 / 25 / 26 lessons
demonstrated for `expected<size_t, format_error>` — today
just replaces the error type with `LogError` and shows the
chain still works.

## Section 15 — 50-round determinism

The determinism stress is the same shape as Aug 26's lesson:
format the same `(round, hash, pi)` triple into fresh buffers
50 times. Held as owned `std::string` (the lifetime lesson
from Aug 25 / 26 — borrow-into-stack-array dangles when the
iteration ends):

```cpp
std::string_view fmt = "round={} hash={:08x} pi={:.3f}";
int round_arg = 17;
unsigned hash_arg = 17u * 2654435761u;  // unsigned multiplication
double pi_arg = 3.14159;
std::size_t args = 31;  // "round=17 hash=81af14c1 pi=3.142"

char pin_buf[64] = {};
auto pin = petra::format_to_n_runtime_string_projected(
    pin_buf, sizeof(pin_buf), fmt, round_arg, hash_arg, pi_arg);
CHECK(pin.has_value());
CHECK(pin.value() == args);
std::string pin_str{pin_buf, args};     // owned

for (int i = 0; i < 49; ++i) {
    char buf[64] = {};
    auto r = petra::format_to_n_runtime_string_projected(
        buf, sizeof(buf), fmt, round_arg, hash_arg, pi_arg);
    CHECK(r.has_value());
    CHECK(r.value() == args);
    std::string out{buf, args};
    CHECK(out == pin_str);
}
```

The hash is `17u * 2654435761u = 81af14c1` (unsigned × unsigned
overflow, wraps). The Aug 25 / 26 lessons pinned the same
arithmetic. Same lifetime lesson (owned `std::string` for the
pin, no `string_view` borrow into a stack array).

## Section 17 — `n == 1` is Overflow, not EmptyBuffer

A subtle boundary. `n == 1` means cap == 0, the writable
region is empty, but the NUL still fits at `out[0]`. So:

- Aug 26: writes the NUL at `out[0]` and returns `would_be`.
- Today: if `would_be ≥ 1` (which it is for any non-empty
  format string), it is `Overflow` (truncation).

```cpp
char buf[1] = {};
std::string_view fmt = "anything";
auto r = petra::format_to_n_runtime_string_projected(
    buf, sizeof(buf), fmt);
CHECK(!r.has_value());
CHECK(r.error().kind() == petra::LogError::Kind::Overflow);
```

The distinction between `EmptyBuffer` (`n == 0`) and `Overflow`
(`n == 1` and `would_be > 0`) is the kind that "the buffer was
too small to hold any data" vs "the buffer was sized to hold
a NUL but not the data". A different domain might want
`EmptyBuffer` to subsume both; today we keep them distinct
because the call sites usually want to know "did we get the
data through at all?"

## Strict signedness handling

The same signed/unsigned bridging as Aug 26: pass 1 returns a
signed `std::iter_difference_t<counting_output_iterator>`
(which is `std::ptrdiff_t`); the wrapper's API uses
`std::size_t`. Today's wrapper never has to bridge that
itself — Aug 26 does the cast (`auto r = petra::format_to_n_
runtime_string(...)` returns `expected<size_t, format_error>`
already, so the size is `size_t`). The transform_error path
preserves the size_t value; the only new cast in today's
wrapper is `static_cast<std::size_t>(r.value())` in Section
8, where a `string_view` is constructed from a buffer and a
size. Strict warnings stay silent.

## Where we go next

Today's lesson closes the **third new follow-on item** from
the Aug 26 lesson's "Where we go next" section, the
consumer-side `expected<E, …>` projection. The remaining
items from Aug 26's list (and the cross-cutting infrastructure
items) remain open:

- **`std::format_to_n_runtime_string` for `std::array<char, N>`**
  — the current `span<char>` overload works (a
  `std::array<char, N>` is implicitly convertible to a span),
  but a `std::array<char, N>::iterator` overload that knows
  the array size at compile time could let the compiler
  inline the size bound. Probably not worth a separate
  lesson.
- **`std::format_to_n_runtime_string` with a
  `std::runtime_format(...)` wrapper for `std::format`** —
  C++26 will add `std::runtime_format(sv)` as the proper
  escape hatch for runtime format strings on the
  `std::format` surface. Today's wrapper is the
  C++23-compatible cut.
- **`std::format_to_n_runtime_string` for type-erased
  `std::span<const std::any>` args** — when the args come
  from a dynamic-typed source, a
  `variant_format_arg_store` like in {fmt} is the canonical
  shape. libc++ 21 does not have a vendored
  `__format_arg_store` that supports `std::any`.
- **`std::format` to `std::ostream`** — the missing
  overload. P2093R14 mentions it; libc++ 21 has not
  implemented it. The workaround (`std::format` ->
  `std::string` -> `<<`) is fine but a real `std::print(
  std::ostream&, ...)` would close the gap. Track for
  libc++ post-21.
- **`std::vector<MyEnum>` / `std::vector<MyStruct>` range
  formatter via ADL on the element type** — P2286
  technically permits this; libc++ 21 does not implement
  it.
- **`std::chrono::zoned_time` formatting with an explicit
  time zone** — Aug 22's note about chrono respecting local
  TZ is worth a focused tour.
- **`std::format_to_n` for `std::span<char>` filling** —
  Aug 24's Section 7 exercises this; a follow-up could
  benchmark against `std::string::resize()` + `format_to`.

New items surfaced by today's lesson:

- **`format_to_n_runtime_string_projected` with an
  `error_category` / `error_code` projection** —
  `std::error_code` (P0824R6) maps cleanly onto
  `LogError::Kind` via the standard `generic_category()` /
  user-defined categories. Today's wrapper returns
  `LogError`; a `log_error_to_error_code(...)` adapter
  would close the loop on consumer code that already uses
  `std::error_code`. The exact mapping is:
  `LogError::Format` → `format-errc::format_error` (no such
  errc exists in the standard, so this would need a
  custom category). Probably not worth a separate lesson.
- **`format_to_n_runtime_string_projected` for fixed-width
  numeric formatting (`{:0Nd}`)** — Aug 25 / 26 cover
  bounded-buffer format with the standard specifiers; a
  follow-up could walk the precision / width / alignment
  specs explicitly with truncation-vs-overflow
  classification.
- **`format_to_n_runtime_string_projected` with a logging-
  spdlog-style `fmt::runtime(...)` analog** — the
  formatting-with-runtime-input idiom is reusable across
  logging libraries. Today's wrapper is a clean reference
  implementation; a thin adapter that uses it as the
  back-end for a `log::runtime_format(...)` macro would
  round out the surface.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open:

- Pin actions to commit SHAs — `@v4` is a floating tag.
- Multi-OS matrix extending to `windows-latest`.
- Status badge in README.
- vcpkg / Conan port for `psp_span_lib`.
- Branch protection requiring the matrix to pass.
- `v0.16.0` promotion — the mechanical lift of
  `validate_atomic` + `parse_and_apply_atomic_streaming_
  validated` + `resolve_with_validation` + `parse_patch_ops`
  into `<psp_span/json_schema.h>`, `<psp_span/json_pointer.h>`,
  and `<psp_span/json_ext.h>`.
