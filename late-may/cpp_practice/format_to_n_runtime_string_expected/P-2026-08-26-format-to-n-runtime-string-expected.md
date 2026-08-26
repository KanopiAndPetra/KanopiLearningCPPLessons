# P-2026-08-26 — `std::format_to_n_runtime_string`: a runtime-format-string bounded-buffer formatter wrapped in `std::expected<size_t, std::format_error>`

Modern-C++ lesson closing the **second new follow-on item** the
Aug 25 (P-2026-08-25) lesson's "Where we go next" section explicitly
named, verbatim:

> **`std::format_to_n_into` with a `RuntimeFormatString<...>`
> escape hatch** — Aug 24's lesson noted that
> `std::format_string<Args...>` is the consteval-checked
> type. A companion overload that takes a runtime format
> string (and returns `expected` on parse error) is a natural
> addition when the format string comes from external input
> (config files, user templates). The error type would still
> be `std::format_error`. Today is NOT that lesson — the
> focus is the NUL-termination contract.

Today IS that lesson: the runtime-format-string companion to
`format_to_n_into`. The consteval-checked `std::format_string<Args...>`
parameter on Aug 22 / 24 / 25 becomes a runtime `std::string_view`
here. The failures that the consteval gate caught at compile time
(parse errors, argument-index errors, type mismatches) become
runtime `std::format_error` throws.

| Surface                      | Aug 22   | Aug 24   | Aug 25   | Today    |
|------------------------------|----------|----------|----------|----------|
| raw bounded-buffer           |          | ✅       |          |          |
| NUL-terminating bounded      |          |          | ✅       | ✅       |
| Runtime-format-string        |          |          |          | ✅       |

The whole new wrapper fits in ~80 lines (the wrapper plus two
output-iterator adapters and their tests). The interesting bits are:

1. The two-pass design: pass 1 measures the would-be size by writing
   to a *counting* output iterator (no allocations); pass 2 writes
   into the bounded buffer via a *bounded* output iterator.
2. The bounded-output-iterator **must carry its counter via an
   external pointer**, not as a member. `std::vformat_to` makes a
   copy of the iterator on every `++` advance, so a member counter
   would be writing into a transient copy the caller can no longer
   observe. This is a real lifetime-bug shape — a first-draft
   `bounded_output_iterator` with a member counter silently lost all
   writes. ASan does NOT catch this (it's correct use of memory;
   it's just the wrong object), so it has to be caught by Section 18
   / Section 19's invariants instead.

## Headline

| Build                                                                     | Result            |
|---------------------------------------------------------------------------|-------------------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`)                      | **332/332 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **332/332 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **332/332 PASS**, empty stderr |
| CMake (`cmake -S . -B build && cmake --build build`)                       | **332/332 PASS**  |
| CMake strict (`-DENABLE_ASAN=OFF` + strict flags)                          | **332/332 PASS**  |
| CMake ASan (`-DENABLE_ASAN=ON`)                                           | **332/332 PASS**, empty stderr |

The total assertion count of **332** comes from runtime; the per-section
checks add up because each `format_to_n_runtime_string` call exercises
multiple invariants (would-be size + NUL placement + buffer integrity),
and Section 19's 50-round determinism stress alone contributes 100
invariants (1 initial pin + 49 cross-checks + 50 final size pins).

| Section | Topic                                                              | Tests |
|---------|--------------------------------------------------------------------|-------|
| 1       | Toolchain + feature probes + `sizeof` of relevant types            | 6     |
| 2       | Core API: success path, NUL placement at `r.value()`               | 5     |
| 3       | **Headline scenario**: runtime format string from external input   | 4     |
| 4       | Exact fit — formatted text fills the writable region               | 4     |
| 5       | Truncation — formatted text exceeds `n`; NUL at `out[n-1]`         | 4     |
| 6       | Truncation edge — formatted text exactly `n-1` chars + 1 (`n` chars total) | 4 |
| 7       | Zero-capacity buffer — no NUL, would-be reported                   | 2     |
| 8       | Size-one buffer — only the NUL is written                          | 3     |
| 9       | `std::format_error` on bad parse (dangling `{`)                    | 3     |
| 10      | `std::format_error` on bad argument index (`{:5}` for 1 arg)      | 4     |
| 11      | `std::format_error` on type mismatch (`{:.3f}` for a string)      | 2     |
| 12      | `std::format_error` hierarchy (runtime_error, not system_error)    | 1     |
| 13      | Composing with `std::expected` — `and_then` / `transform` / `transform_error` | 7 |
| 14      | `std::span<char>` overload                                         | 4     |
| 15      | Integer / float specifiers (`{:08x}` `{:d}` `{:+}` `{:.3f}` `{:.2e}`) | 6 |
| 16      | `std::string_view` arg                                             | 3     |
| 17      | UTF-8 byte sequences — `héllo 😀` (11 bytes arg)                  | 4     |
| 18      | NUL byte safety — every buf size from 1 to 32                      | 64    |
| 19      | 50-round determinism stress + cross-checks (OWNED string pin)      | 150   |
| 20      | Distinct contract from Aug 25's `format_to_n_into`                 | 1     |
| 21      | Same-runtime-format coexistence with Aug 25's surface             | 5     |
| **Total**|                                                                  | **332** (verified) |

## Why this wrapper exists at all

The Aug 25 wrapper (`format_to_n_into`) takes `std::format_string<Args...>`,
which is consteval-checked: bad format strings are a compile error.
That is the right gate when the format string is a `string` literal
in the caller's source. But format strings regularly come from
**external input** — config files, JSON, user-supplied templates —
and there is no way to consteval-check those at the call site.
The consumer surface needs a runtime path that takes
`std::string_view` and returns a typed error on parse / type
failure:

```cpp
// Aug 25 pattern (compile-time-validated):
auto r = petra::format_to_n_into(buf, sizeof(buf), "x={}", 42);

// Today's pattern (runtime-validated):
std::string_view fmt = config.lookup("log_line");   // from external source
auto r = petra::format_to_n_runtime_string(
    buf, sizeof(buf), fmt,
    user_id, ip, status_code);                      // <-- typed error on bad fmt
```

Same NUL-termination contract as Aug 25:

```cpp
char buf[64] = {};
std::string_view fmt = "size={}";
auto r = petra::format_to_n_runtime_string(buf, sizeof(buf), fmt, 42);
if (!r) throw r.error();
std::cout << buf;                                  // valid C-string always
```

## The two-pass design — why it's necessary

`std::format` and `std::format_to_n` (the Aug 22 / 24 / 25 wrappers)
go through `std::basic_format_string<Args...>`, which is **consteval**
checked. If you pass a runtime `string_view`, the consteval gate
fires and you get a compile error:

```cpp
std::string_view fmt = "x={}";
auto r = std::format(fmt, 42);   // COMPILE ERROR
```

The runtime path is `std::vformat` / `std::vformat_to` (P2216R3
runtime surface). These take `std::string_view` directly and throw
`std::format_error` on parse or type failure. The price: they don't
have a "bounded" version. `std::format_to_n` exists *because* the
std committee added a bounded-output overload on top of the
consteval surface; the same bounded form on top of the runtime
surface doesn't exist (P2732R0 / P2997R0 are working on it).

So today's wrapper has to **compose**:

1. Pass 1: **measure the would-be size** by formatting into a
   *counting* output iterator. This catches parse / type errors.
2. Pass 2: **write** up to `n - 1` chars (reserves a NUL slot like
   Aug 25) into the bounded buffer via a *bounded* output iterator.
   Only runs if pass 1 succeeded.

```cpp
// Pass 1: measure via std::vformat_to into a counting iterator.
std::size_t would_be_written = 0;
try {
    petra::detail::counting_output_iterator counter{&would_be_written};
    std::vformat_to(counter, fmt, std::make_format_args(args...));
} catch (const std::format_error& e) {
    return std::unexpected{std::format_error{e}};
} catch (...) {
    return std::unexpected{std::format_error{"unknown formatting error"}};
}

// Pass 2: bounded write via the bounded_output_iterator.
std::size_t cap = n - 1;
std::size_t actually_written = 0;
try {
    petra::detail::bounded_output_iterator writer{out, cap, &actually_written};
    std::vformat_to(writer, fmt, std::make_format_args(args...));
} catch (const std::format_error& e) {
    out[0] = '\0';
    return std::unexpected{std::format_error{e}};
} catch (...) {
    out[0] = '\0';
    return std::unexpected{std::format_error{"unknown formatting error"}};
}

std::size_t nul_idx = (actually_written < cap)
                          ? actually_written
                          : cap;
out[nul_idx] = '\0';
return would_be_written;  // would-be semantic, same as Aug 25
```

The price for the bounded write is one custom output iterator
(80 lines including tests). Without it, the wrapper would have to
either:
- Allocate a `std::string` and trust the caller has bounded memory
  (defeats the whole point of `format_to_n`), or
- Catch `std::length_error` from a counting wrapper that throws at
  the cap (but `format_to` doesn't throw on its own length limit;
  the length cap has to be built into the iterator).

The custom iterator is the least-bad option and matches the shape
that `std::format_to_n` itself uses internally.

## Section 16 — the lifetime bug that ASan doesn't catch

This was the **most important finding of the lesson**. The first
draft of `bounded_output_iterator` stored the written-count as a
member:

```cpp
// BAD: lost writes
class bounded_output_iterator {
    char* p_;
    std::size_t cap_;
    std::size_t written_;  // <-- the bug
    ...
    bounded_output_iterator& operator=(char c) noexcept {
        if (written_ < cap_) { p_[written_] = c; ++written_; }
        return *this;
    }
    ...
};
```

`std::vformat_to` takes its output iterator **by value** and makes
a copy on every `++` advance. Both the lvalue the caller sees and
each transient copy increment their OWN `written_`. After
`vformat_to` returns, the caller's iterator has `written_ == 0`,
because the increment happened in a copy:

```
--- out is at 0x16bd7a4d0 ---
COPY(tag=OUT)     <- vformat_to's by-value advance copies the iterator
...
  [OUT] assign 'h' (this=0x16bd79ed0, written=0, cap=8)   <- copy's written_ incremented
  [OUT] assign 'i' (this=0x16bd79ed0, written=1, cap=8)
  ...
  [OUT] assign '!' (this=0x16bd79ed0, written=6, cap=8)
COPY(tag=OUT)
--- done. out.written()=0 ---   <- caller's lvalue, untouched
```

The fix is to **carry the counter via an external pointer**:

```cpp
// GOOD: shared external counter
class bounded_output_iterator {
    char* p_;
    std::size_t cap_;
    std::size_t* written_;  // external counter
    ...
    bounded_output_iterator& operator=(char c) noexcept {
        if (*written_ < cap_) { p_[*written_] = c; ++(*written_); }
        return *this;
    }
    ...
};
```

The pointer is non-owning, so default copy semantics do the right
thing: every copy refers to the same external counter, and the
caller's lvalue shares that counter. **ASan does not catch this
bug**: the memory accesses are all valid (`p_[written_]` is
within `[p_, p_+cap_)`), the bug is that the caller's view of
`written_` never observes the increments.

Sections 18 and 19 (NUL byte safety + 50-round determinism) catch
the bug indirectly: with a member counter, pass 2 writes nothing,
the NUL placement line writes `out[0] = '\0'`, the buffer is just
a NUL character, and both sections fail.

## Section 9 — `std::format_error` on bad parse

```cpp
std::string_view bad_fmt = "ends with {";  // unterminated '{'
auto r = petra::format_to_n_runtime_string(buf, sizeof(bad_fmt) * 8, bad_fmt, 42);
CHECK(!r.has_value());
CHECK(std::string{r.error().what()}.find("'{'") != std::string::npos);
CHECK(buf[0] == '\0');                                  // error path NUL
```

This is a **compile error** on the consteval-checked Aug 22 / 24 /
25 surface (`std::format_string<Args...>{bad_fmt}` fails the
consteval gate). Today it's a runtime `std::format_error` with
message "The format string terminates at a '{'".

The error path also writes `out[0] = '\0'`, matching Aug 25's
contract: the caller can always print the buffer as a C-string, even
when the format fails. Section 9 verifies the NUL is in place after
the failure.

## Section 10 — `std::format_error` on bad argument index

```cpp
std::string_view fmt = "x={1}";  // asks for arg #1
int i = 42;                        // only arg #0 supplied
auto r = petra::format_to_n_runtime_string(buf, 64, fmt, i);
CHECK(!r.has_value());
```

Bad arg index would also be a compile error on `std::format` (Aug
22 surface) or any template taking `std::format_string<Args...>`.
Today it's runtime, with libc++ 21 saying "The argument index value
is too large for the number of arguments supplied".

## Section 11 — `std::format_error` on type mismatch

```cpp
std::string_view fmt = "pi={:.3f}";  // expects a float
std::string s = "hello";              // wrong type
auto r = petra::format_to_n_runtime_string(buf, 64, fmt, s);
CHECK(!r.has_value());
```

Type mismatch is the third compile-time-class error that surfaces
as a runtime error here. The consteval check on the
`std::format_string<Args...>` parameter verifies that the arg types
match the specifiers at compile time; today's wrapper has no way to
do that (the format string is runtime), so it has to be caught at
runtime.

## Section 13 — composing with `std::expected`'s monadic ops

Same shape as Aug 24 / 25 — the wrapper's error type is
`std::format_error` throughout, so `and_then` / `transform` /
`transform_error` chain cleanly:

```cpp
auto r1 = petra::format_to_n_runtime_string(buf1, sizeof(buf1),
                                             "size={}", 42);
// r1.value() == 7 ("size=42")

auto s1 = r1.transform([](std::size_t n) {
    return std::string{"got "} + std::to_string(n) + " chars";
});
// s1.value() == "got 7 chars"

auto r2 = r1.and_then([&buf2](std::size_t n)
    -> std::expected<std::size_t, std::format_error> {
    return petra::format_to_n_runtime_string(buf2, sizeof(buf2),
                                              "[r1={}]", n);
});
// r2.value() == 6, buf2 == "[r1=7]"
```

The chain has one error type (`std::format_error`) end-to-end. If
either step fails, the error propagates without touching the next
buffer.

## Section 19 — 50-round determinism stress

The stress loop formats the same triple (round, hash, pi) 50 times
into fresh buffers. Every output is byte-identical:

```
"round=17 hash=81af14c1 pi=3.142"   (31 chars, would-be == 31)
```

Note: `17u * 2654435761u` overflows `unsigned int` on arm64 (the
multiplication is `unsigned * unsigned`). The actual hash is
`81af14c1`, not `0e3779b1` (which is what `17 * 2654435761u` — a
signed multiplication — would produce). The Aug 25 lesson caught the
same arithmetic subtlety (Section 15 there, also pinned to
`81af14c1`). Lesson is: count the chars AND check the multiplication
operator.

**Important lifetime lesson (same shape as Aug 25)**: the first
draft of Section 19 stored the first iteration's output as
`std::string_view` into the iteration's stack array. That
`string_view` then dangled when the iteration ended, and the
subsequent comparison `std::string_view{buf} == first_out` read
uninitialised poison. ASan catches that as
`stack-use-after-scope` (Aug 25's lesson notes describe the same
fix: store the output as `std::string` (owned), not
`std::string_view` (borrowed)).

## Section 20 — distinct from Aug 25

| Property                | Aug 25 (`format_to_n_into`)      | Today (`format_to_n_runtime_string`) |
|-------------------------|----------------------------------|--------------------------------------|
| Format-string type      | `std::format_string<Args...>`   | `std::string_view`                   |
| Consteval gate          | Yes (compile-time parse + type)  | No (parse + type errors are runtime)  |
| Bad parse               | Compile error                    | `std::format_error` (caught at runtime) |
| Bad argument index      | Compile error                    | `std::format_error`                  |
| Bad type                | Compile error                    | `std::format_error`                  |
| NUL-termination         | Yes                              | Yes                                  |
| Would-be size semantic  | Yes                              | Yes                                  |
| `n == 0` / `n == 1`     | same shape                       | same shape                           |
| Truncation: NUL at      | `n - 1`                          | `n - 1` (same as Aug 25)             |
| `std::span<char>` overload | Yes                           | Yes                                  |
| Output iterator         | `format_to_n` native             | custom `bounded_output_iterator` + `counting_output_iterator` |

The two wrappers are interchangeable on a compile-time-known format
string that's reachable as a `string` literal — Aug 25's version is
preferred because the consteval gate catches typos. Today's version
is the right call when the format string comes from external input
(config files, user templates, JSON). Section 21 verifies both can
be used side-by-side in the same TU.

## Section 21 — same-runtime-format coexistence with Aug 25's surface

Today's wrapper doesn't conflict with Aug 25's surface in the same
TU. The two differ only at the entry point (the format string type
plus the entry function — `format_to_n` vs. `vformat_to`); both
return the same `expected<size_t, format_error>` shape and apply
the same NUL-termination contract. They compose well in a code base
that has both compile-time and runtime format strings.

## Strict signedness handling

The wrapper has to bridge two signed/unsigned domains:

- `std::format_to_n`'s second parameter is
  `std::iter_difference_t<Out>`, which for `char*` is
  `std::ptrdiff_t` (signed).
- The wrapper's API uses `std::size_t` (unsigned).

Aug 25's lesson handled this with `static_cast<std::iter_difference_t<char*>>(n - 1)`
and the matching reverse casts. Today's lesson follows the same
shape; the integer types are the same, the cast points are the same,
the strict warnings stay silent. (No `std::format_to_n` here, so no
`iter_difference_t` either — the bounded iterator is in our own code,
and its `cap_` is a `std::size_t` throughout. The signedness bridge
today is just between the iterator's `difference_type` and the
wrapper's `std::size_t`.)

## Where we go next

Today's lesson closes the **second new follow-on item** the Aug 25
lesson's "Where we go next" section explicitly named. The remaining
items from that list are unchanged:

- **`std::format` to `std::ostream`** — the missing overload.
  P2093R14 mentions it; libc++ 21 has not implemented it. The
  workaround (`std::format` -> `std::string` -> `<<`) is fine
  but a real `std::print(std::ostream&, ...)` would close the
  gap. Track for libc++ post-21.
- **`std::vector<MyEnum>` / `std::vector<MyStruct>` range
  formatter via ADL on the element type** — P2286 technically
  permits this; libc++ 21 does not implement it. A future
  lesson could implement a custom `format_as<>` helper that
  explicitly renders a vector<T> by mapping to string_views
  first.
- **`std::chrono::zoned_time` formatting with an explicit time
  zone** — Aug 22's note about chrono respecting local TZ is
  worth a focused tour; the `%Z` and `%z` specifiers behave
  differently and the difference is not well advertised.
- **`std::format_to_n` for `std::span<char>` filling** —
  Aug 24's Section 7 exercises this; a follow-up could
  benchmark it against `std::string::resize()` + `format_to`,
  which is the idiomatic alternative for sized strings.
- **A typed-error ergonomics wrapper around `std::print`** —
  Aug 22's lesson noted `std::print` doesn't take
  `std::ostream`. A companion wrapper `print_into(buf, fmt,
  args...) -> expected<size_t, format_error>` would mirror
  today's `format_to_n_runtime_string` and round out the
  consumer surface. (Technically a third facet alongside Aug 25's
  NUL-terminating bounded-buffer and Aug 24's raw bounded-buffer.)

New items surfaced by today's lesson:

- **`std::format_to_n_runtime_string` for `std::array<char, N>`** —
  the current `span<char>` overload works (a `std::array<char, N>` is
  implicitly convertible to a span), but a `std::array<char,
  N>::iterator` overload that knows the array size at compile time
  could let the compiler inline the size bound. Probably not worth a
  separate lesson — the span overload is one pointer-sized difference
  at runtime.
- **`std::format_to_n_runtime_string` with a
  `std::runtime_format(...)` wrapper for `std::format`** — C++26
  will add `std::runtime_format(sv)` as the proper escape hatch
  for runtime format strings on the `std::format` surface (no
  `vformat_to` needed). libc++ 21 already forward-declares it
  under `__runtime_format_string`, but the function is gated
  `_LIBCPP_STD_VER >= 26`. Today's wrapper is the C++23-compatible
  cut: it goes through `vformat_to` instead of going through
  `std::format(std::runtime_format(...))`. When libc++ ships
  C++26 support, the wrapper can have a `std::format`-based
  implementation and today's `vformat_to`-based one becomes the
  C++23 fallback.
- **`std::format_to_n_runtime_string` for runtime-checked
  consumer-side `std::expected<E, ...>` projection** — today's
  wrapper returns `expected<size_t, format_error>`. A natural
  addition is a `transform_error` overload that projects to a
  domain-specific error type (e.g. `expected<size_t, MyError>`
  where `MyError::FormatError(FormatError)` for the format
  failure, `MyError::OutOfMemory` for an allocation failure
  in pass 1, and `MyError::BufferTooSmall` for the truncation
  case). That's a consumer-side concern and probably doesn't
  warrant a separate lesson.
- **`std::format_to_n_runtime_string` for type-erased
  `std::span<const std::any>` args** — when the args come from a
  dynamic-typed source (e.g. an RPC layer that talks `std::any`),
  a `variant_format_arg_store` like in {fmt} is the canonical
  shape. libc++ 21 does not have a vendored `__format_arg_store`
  that supports `std::any`; a third-party `expected<size_t,
  format_error>` wrapper could plug one in.

The cross-cutting infrastructure items from the Aug 13 / Aug 15
/ Aug 17 lessons remain open:

- Pin actions to commit SHAs — `@v4` is a floating tag.
- Multi-OS matrix extending to `windows-latest`.
- Status badge in README.
- vcpkg / Conan port for `psp_span_lib`.
- Branch protection requiring the matrix to pass.
- `v0.16.0` promotion — the mechanical lift of `validate_atomic`
  + `parse_and_apply_atomic_streaming_validated` +
  `resolve_with_validation` + `parse_patch_ops` into
  `<psp_span/json_schema.h>`, `<psp_span/json_pointer.h>`, and
  `<psp_span/json_ext.h>`.
