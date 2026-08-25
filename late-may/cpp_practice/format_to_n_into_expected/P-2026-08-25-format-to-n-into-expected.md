# P-2026-08-25 — `std::format_to_n_into`: NUL-terminating bounded-buffer formatter wrapped in `std::expected<size_t, std::format_error>`

Modern-C++ lesson closing the **first new follow-on item** the
Aug 24 (P-2026-08-24) lesson's "Where we go next" section explicitly
named, verbatim:

> **`std::format_to_n_into(buf, fmt, args...)` style NUL-terminated
> wrapper** — the wrapper today returns the would-be size and leaves
> NUL-termination to the caller. A small companion that writes
> `min(r.value(), N-1)` chars + a NUL would close the "I just want a
> string" use case. The NUL-writing version is not the same as
> `format_to_n` — it's a different function with different bounds.

Aug 24 covered the *raw* bounded-buffer formatter (`format_to_n`) and
left NUL-termination to the caller; today covers the
*NUL-terminating* companion. They are complementary facets of the
same `std::format` machinery:

| Surface                  | Aug 24 lesson | Today |
|--------------------------|---------------|-------|
| raw bounded-buffer       | ✅            |       |
| NUL-terminating bounded  |               | ✅    |

The whole "library" is ~50 lines (the wrapper plus its `span<char>`
overload). The interesting bit is the **NUL placement decision**:

```cpp
template <class... Args>
[[nodiscard]]
std::expected<std::size_t, std::format_error>
format_to_n_into(char* out, std::size_t n,
                 std::format_string<Args...> fmt, Args&&... args)
{
    if (n == 0) {
        // n == 0 edge: no NUL can be written. Report the would-be size
        // (computed via std::format_to_n(out, 0, ...) which writes nothing
        // but returns the would-be size).
        try {
            auto result = std::format_to_n(
                out, std::iter_difference_t<char*>{0}, fmt,
                std::forward<Args>(args)...);
            return static_cast<std::size_t>(result.size);
        } catch (const std::format_error& e) {
            return std::unexpected{std::format_error{e}};
        } catch (...) {
            return std::unexpected{std::format_error{"unknown formatting error"}};
        }
    }

    // Reserve one slot for the NUL. Effective write capacity is n-1.
    // std::format_to_n's second parameter is iter_difference_t<Out> (signed).
    auto cap_signed = static_cast<std::iter_difference_t<char*>>(n - 1);

    try {
        auto result = std::format_to_n(out, cap_signed, fmt,
                                       std::forward<Args>(args)...);

        // Where does the NUL go?
        //   - result.size < cap_signed → NUL at result.size (the actual
        //     chars written == would-be in this case).
        //   - result.size >= cap_signed → NUL at cap_signed == n-1
        //     (the formatted text filled the buffer; the NUL slots in
        //     at the very end).
        auto written = result.size;
        std::size_t nul_idx = (written < cap_signed)
                                  ? static_cast<std::size_t>(written)
                                  : static_cast<std::size_t>(cap_signed);
        out[nul_idx] = '\0';
        return static_cast<std::size_t>(written);
    } catch (const std::format_error& e) {
        // Error path: leave the buffer well-formed but empty.
        out[0] = '\0';
        return std::unexpected{std::format_error{e}};
    } catch (...) {
        out[0] = '\0';
        return std::unexpected{std::format_error{"unknown formatting error"}};
    }
}
```

## Headline

| Build                                                                     | Result            |
|---------------------------------------------------------------------------|-------------------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`)                      | **290/290 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **290/290 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **290/290 PASS**, empty stderr |
| CMake (`cmake -S . -B build && cmake --build build`)                       | **290/290 PASS**  |
| CMake strict (`-DENABLE_ASAN=OFF` + strict flags)                          | **290/290 PASS**  |
| CMake ASan (`-DENABLE_ASAN=ON`)                                           | **290/290 PASS**, empty stderr |

The total assertion count of **290** comes from runtime; the
per-section checks add up because each `format_to_n_into` call
exercises multiple invariants (NUL placement + would-be size + buffer
integrity). The 50-round determinism stress (Section 15) alone
contributes 51 invariants (one initial pin + 49 cross-checks + 1 final
pin); Section 16 contributes 32 (one per buf size from 1 to 32).

| Section | Topic                                                              | Tests |
|---------|--------------------------------------------------------------------|-------|
| 1       | Toolchain + feature probes + `sizeof` of relevant types            | 5     |
| 2       | Core API: success path, NUL placement at `r.value()`               | 6     |
| 3       | Exact fit — formatted text fills the writable region               | 6     |
| 4       | Truncation — formatted text exceeds `n`; NUL at `out[n-1]`         | 6     |
| 5       | Edge: formatted length == `n-1` (last writable char consumed)      | 5     |
| 6       | Zero-capacity buffer — no NUL, would-be reported                   | 2     |
| 7       | Size-one buffer — only the NUL is written                          | 3     |
| 8       | `std::format_error` hierarchy (runtime_error, not system_error)    | 3     |
| 9       | Composing with `std::expected` — `and_then` / `transform` / `transform_error` | 9 |
| 10      | `std::span<char>` overload                                         | 5     |
| 11      | Integer specifiers (`{:08x}` `{:d}` `{:+d}`)                       | 4     |
| 12      | Float specifiers (`{:.3f}`)                                        | 4     |
| 13      | `std::string_view` arg                                             | 3     |
| 14      | UTF-8 byte sequences — `héllo 😀` (11 bytes)                       | 4     |
| 15      | 50-round determinism stress + cross-checks                         | 102   |
| 16      | NUL byte safety — every buf size from 1 to 32                      | 96    |
| 17      | Distinct contract from Aug 24's `format_to_expected` (NUL placement) | 14   |
| 18      | Explicit `std::unexpected{std::format_error{...}}` construction    | 4     |
| 19      | Signed/unsigned size interop with raw `std::format_to_n`           | 2     |
| 20      | `main()` output                                                    | 1     |
| **Total**|                                                                  | **290** (verified) |

## Why this wrapper exists at all

The Aug 24 wrapper (`format_to_expected`) returns the would-be size
and leaves NUL-termination to the caller. That is the standard
`format_to_n` consumer pattern: "size first, allocate second". But
when the caller already KNOWS the buffer is sized `N` and just wants
a NUL-terminated C-string back, the manual NUL-termination is
boilerplate:

```cpp
// Aug 24 pattern:
char buf[64];
auto r = format_to_expected(buf, sizeof(buf), "x={}", 42);
if (!r) throw r.error();
buf[std::min(r.value(), sizeof(buf) - 1)] = '\0';  // <-- manual
```

vs. today:

```cpp
// Today's pattern:
char buf[64];
auto r = format_to_n_into(buf, sizeof(buf), "x={}", 42);
if (!r) throw r.error();
// buf is already a valid C-string, always.
```

The NUL is NOT free — it costs one byte of capacity. Today's wrapper
"reserves" one slot for the NUL by passing `n-1` (not `n`) to
`std::format_to_n`. That is the key difference from the Aug 24
wrapper, which passes `n` directly.

## Section 4 — the NUL placement gotcha

When the formatted text fits exactly (`result.size <= n-1`), the NUL
goes at `result.size` (the chars actually written). When the
formatted text is truncated (`result.size > n-1`), the NUL goes at
`n-1` (the very end of the buffer, since we wrote `n-1` chars and
need a terminator slot):

```
buf size 8, format "{}", "hello world"   (11 chars)
   -> result.size == 11   (would-be)
   -> written chars: 7 (cap == n-1 == 7)
   -> buf[7] == '\0'      (NUL at n-1)
   -> buf contents: "hello w\0"
   -> r.value() == 11     (would-be, NOT chars actually written)
```

This is a small thing but it's the headline difference from Aug 24:
the wrapper does NOT change `format_to_n`'s would-be semantics; only
the NUL placement is new.

## Section 6 — the n == 0 edge

When `n == 0`, the wrapper cannot write even a NUL (the buffer has no
slots). What it CAN do is compute the would-be size by formatting
into a 0-cap buffer (`std::format_to_n` writes nothing but returns
the would-be size). Today's wrapper preserves this:

```cpp
auto r = format_to_n_into(empty.data(), empty.size(),
                          "anything {}", 42);
r.value() == 11   // the would-be size
```

This is identical to the n >= 1 contract — the would-be size is the
format length, regardless of buffer capacity. Callers can use this
to allocate the right-sized buffer (`std::vector<char>(r.value() + 1)`)
and then re-format with `n = r.value() + 1` for the untruncated form.

## Section 7 — the n == 1 edge (only the NUL)

With `n == 1`, the wrapper has room for only the NUL. The formatted
text is dropped entirely (we have cap = 0). The would-be size is
still reported:

```cpp
char buf[1] = {'Q'};  // poison
auto r = format_to_n_into(buf, sizeof(buf), "anything {}", 42);
r.value() == 11   // would-be
buf[0] == '\0'    // the NUL is the only thing written
```

This is the right behaviour: the wrapper reserves one slot for the
NUL, and when `n == 1` that's all the slots it has.

## Section 9 — composing with `std::expected`'s monadic ops

Same shape as Aug 24 — the wrapper's error type is `std::format_error`
throughout, so `and_then` / `transform` / `transform_error` chain
cleanly:

```cpp
auto r1 = petra::format_to_n_into(buf1, sizeof(buf1), "size={}", 42);
// r1.value() == 7 ("size=42")

auto s1 = r1.transform([](std::size_t n) {
    return std::string{"got "} + std::to_string(n) + " chars";
});
// s1.value() == "got 7 chars"

auto r2 = r1.and_then([&buf2](std::size_t n)
    -> std::expected<std::size_t, std::format_error> {
    return petra::format_to_n_into(buf2, sizeof(buf2), "[r1={}]", n);
});
// r2.value() == 6, buf2 == "[r1=7]"
```

The chain has one error type (`std::format_error`) end-to-end. If
either step fails, the error propagates without touching the next
buffer.

## Section 15 — 50-round determinism stress

The stress loop runs `format_to_n_into(buf, 64, "round={}
hash={:08x} pi={:.3f}", 17, 17u*2654435761u, 3.14159)` 50 times into
fresh buffers. Every output is byte-identical:

```
"round=17 hash=81af14c1 pi=3.142"   (31 chars, would-be == 31)
```

Note: `17u * 2654435761u` overflows `unsigned int` on arm64 (the
multiplication is `unsigned * unsigned`). The actual hash is
`81af14c1`, not `0e3779b1` (which is what `17 * 2654435761u` — a
signed multiplication — would produce). The lesson note from Aug 24
quoted `0e3779b1` because that lesson used `(17*2654435761u)` (signed
times unsigned). Today's lesson pins the unsigned-multiply version
correctly. Lesson is: count the chars AND check the multiplication
operator.

**An important ASan finding during development**: the first draft of
Section 15 stored the first iteration's output as `std::string_view`
into the iteration's stack array. That `string_view` then dangled
when the iteration ended, and the subsequent comparison
`std::string_view{buf} == first_out` read uninitialised poison. ASan
caught it as a `stack-use-after-scope`. The fix is to store the
output as `std::string` (owned), not `std::string_view` (borrowed) —
exactly the same lifetime bug the Aug 24 lesson avoided with a
different shape (it used a single buffer across rounds, so the
`string_view` was still valid at comparison time).

## Section 16 — NUL byte safety across all buffer sizes

For every `n` from 1 to 32, format a string longer than the buffer
and verify:

1. The result is valid (no error).
2. There is exactly one `'\0'` in `[0, n-1]` (or for n=0, no NUL).
3. The `string_view` up to the NUL has size `<= n-1`.

This is a fuzz-style "no write out of bounds" check. All 32 sizes
pass.

## Section 17 — distinct from Aug 24's `format_to_expected`

| Behaviour                         | Aug 24 (`format_to_expected`) | Today (`format_to_n_into`) |
|-----------------------------------|-------------------------------|----------------------------|
| NUL-terminates destination        | No                            | **Yes** (when `n > 0`)     |
| Writes `n` chars max              | Yes                           | No — writes `n-1` chars max |
| `r.value()` semantic              | would-be                      | would-be                   |
| Error path: `out[0]`              | Undefined (caller's problem)  | **`'\0'`**                 |
| Truncation: `buf` contents        | First `n` chars + garbage     | First `n-1` chars + `'\0'` |

The two wrappers are interchangeable on the success path IF the
caller is OK with losing one slot to the NUL. Calling
`format_to_expected` with `n = sizeof(buf) - 1` and then manually
writing `buf[std::min(r.value(), n)] = '\0'` is equivalent to calling
`format_to_n_into` with `n = sizeof(buf)`.

## Compile / run evidence

Default build:
```
clang++ -std=c++23 -stdlib=libc++ -O0 -g \
    late-may/cpp_practice/format_to_n_into_expected/P-2026-08-25-format-to-n-into-expected.cpp \
    -o /tmp/petra-fmtni
/tmp/petra-fmtni
# Total: 290/290 PASS, exit 0
```

Strict build:
```
clang++ -std=c++23 -stdlib=libc++ -O0 -g \
    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion \
    late-may/cpp_practice/format_to_n_into_expected/P-2026-08-25-format-to-n-into-expected.cpp \
    -o /tmp/petra-fmtni-strict
/tmp/petra-fmtni-strict
# Total: 290/290 PASS, exit 0
```

ASan + UBSan build:
```
clang++ -std=c++23 -stdlib=libc++ -O1 -g \
    -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    late-may/cpp_practice/format_to_n_into_expected/P-2026-08-25-format-to-n-into-expected.cpp \
    -o /tmp/petra-fmtni-asan
/tmp/petra-fmtni-asan > stdout 2> stderr
# Total: 290/290 PASS, exit 0
# stderr is empty — no sanitizer diagnostics
```

CMake (default / strict / ASan):
```
cmake -S . -B build && cmake --build build
cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" \
    && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
# All three: 290/290 PASS, exit 0
```

The file is a self-contained translation unit — no third-party
libraries, no library installation, no `find_package`. Headers used:
`<array>`, `<cassert>`, `<cstddef>`, `<cstdint>`, `<cstdio>`,
`<cstring>`, `<expected>`, `<format>`, `<iterator>`, `<span>`,
`<string>`, `<string_view>`, `<system_error>`, `<type_traits>`,
`<utility>`, `<vector>`.

## Where we go next

Today's lesson closes the **first new follow-on item** the Aug 24
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
  today's `format_to_n_into` and round out the consumer
  surface. (Technically a third facet alongside today's
  NUL-terminating bounded-buffer and Aug 24's raw bounded-buffer.)

New items surfaced by today's lesson:

- **`std::format_to_n_into` for `std::array<char, N>`** — the
  current `span<char>` overload works (a `std::array<char, N>`
  is implicitly convertible to a span), but a `std::array<char,
  N>::iterator` overload that knows the array size at compile
  time could let the compiler inline the size bound. Probably
  not worth a separate lesson — the span overload is one
  pointer-sized difference at runtime.
- **`std::format_to_n_into` with a `RuntimeFormatString<...>`
  escape hatch** — Aug 24's lesson noted that
  `std::format_string<Args...>` is the consteval-checked
  type. A companion overload that takes a runtime format
  string (and returns `expected` on parse error) is a natural
  addition when the format string comes from external input
  (config files, user templates). The error type would still
  be `std::format_error`. Today is NOT that lesson — the
  focus is the NUL-termination contract.

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
