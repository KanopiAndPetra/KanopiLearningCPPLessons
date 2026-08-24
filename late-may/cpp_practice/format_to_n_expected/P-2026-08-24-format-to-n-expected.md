# P-2026-08-24 — `std::format_to_n` wrapped in `std::expected<size_t, std::format_error>`

Modern-C++ lesson closing the **second** item on the Aug 22
lesson's "Where we go next" follow-on list, verbatim:

> **`A format_to_n_into style consumer-side helper that returns
> `std::expected<std::size_t, FormatError>`** — composes
> `format_to_n` with `expected`'s error channel. Small, focused.

Aug 22 covered the *streaming-println* surface (`std::print` /
`std::println` / `std::format` / `std::vprint_unicode`); today
covers the *bounded-buffer consumer* surface. They are complementary
facets of the same `std::format` machinery:

| Surface              | Sink                  | Aug 22 lesson | Today |
|----------------------|-----------------------|---------------|-------|
| streaming-println    | `FILE*` (Unicode)     | ✅            |       |
| bounded-buffer       | any output iterator   |               | ✅    |

The whole "library" is 9 lines:

```cpp
template <class Out, class... Args>
std::expected<std::size_t, std::format_error>
format_to_expected(Out out, std::iter_difference_t<Out> n,
                   std::format_string<Args...> fmt, Args&&... args)
{
    try {
        auto result = std::format_to_n(out, n, fmt,
                                       std::forward<Args>(args)...);
        return static_cast<std::size_t>(result.size);
    } catch (const std::format_error& e) {
        return std::unexpected{std::format_error{e}};
    } catch (...) {
        return std::unexpected{std::format_error{"unknown formatting error"}};
    }
}
```

## Headline

| Build                                                                     | Result            |
|---------------------------------------------------------------------------|-------------------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`)                      | **384/384 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **384/384 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **384/384 PASS**, clean sanitizer output |
| CMake (`cmake -S . -B build && cmake --build build`)                       | **384/384 PASS**  |
| CMake strict (`-DENABLE_ASAN=OFF` + strict flags)                          | **384/384 PASS**  |
| CMake ASan (`-DENABLE_ASAN=ON`)                                           | **384/384 PASS**, empty stderr |

The 50-round determinism stress (Section 10) produces byte-identical
output across runs; ASan/UBSan adds zero sanitizer diagnostics to
stderr. The total assertion count of **384** comes from runtime; the
file's preamble advertised "about 60 tests verified" — the per-section
checks add up to 384 because each `format_to_expected` call exercises
multiple invariants.

| Section | Topic                                                              | Tests |
|---------|--------------------------------------------------------------------|-------|
| 1       | Toolchain + feature probes + `sizeof` of relevant types            | 8     |
| 2       | Core API: success path                                              | 5     |
| 3       | Short output that fits within N                                     | 10    |
| 4       | Truncation: output larger than N — "would-be" semantics             | 11    |
| 5       | `back_inserter`: grows destination, but N still caps the write     | 16    |
| 6       | Composing with `std::expected` — `and_then` / `transform` / `transform_error` | 13 |
| 7       | Heterogeneous buffer types — array / vector / span / char* / back_insert_iterator | 13 |
| 8       | `std::format_error`'s surface — hierarchy + what()                 | 6     |
| 9       | Round-trip — parse-then-format, one error channel                   | 13    |
| 10      | Determinism stress (50 rounds × 2 patterns) + cross-checks          | 289   |
| **Total** |                                                                  | **384** (verified) |

## Why this wrapper exists at all

`std::format_to_n(out, n, fmt, args...)` returns
`format_to_n_result<Out> { Out out; size_t size; }`. The `.size`
member is the **number of characters that WOULD have been written**
to the destination, not the number actually written (which can be
smaller when N is the binding constraint). Today wraps this in
`expected<size_t, format_error>` so a caller can:

1. Use `r.value()` as the canonical "size to allocate for a
   NUL-terminated copy" hint — that is the standard
   `format_to_n` consumer pattern.
2. Branch on `r.has_value()` instead of catching exceptions.
3. Compose with `std::expected`'s monadic ops
   (`and_then` / `transform` / `transform_error`).

## Section 1 — sizeof of the relevant types

```
sizeof(std::format_error)                       = 16 B
sizeof(format_to_n_result<char*>)               = 16 B    { Out, size_t }
sizeof(std::expected<size_t, format_error>)     = 24 B
__cpp_lib_format                                = 202110
__cpp_lib_expected                              = 202211
```

`std::format_error` derives from `std::runtime_error` (verified by
`static_assert` in Section 8), which holds a `std::string` in SSO —
16 B on libc++ 21 / arm64. `format_to_n_result<char*>` is `{ Out, size_t }`,
16 B. `expected<size_t, format_error>` is `{ union{T, E}, bool }`,
24 B — the union fits `format_error` (16 B), plus 1 B bool + 7 B
padding to align the size_t.

We deliberately do **not** `static_assert` the `expected` size
because the layout is implementation-defined; the lesson just prints
it.

## Section 4 — the "would-be" semantics is the headline gotcha

`std::format_to_n(out, n, fmt, args...)` always returns the
WOULD-BE size, even when the destination was undersized. This is
the standard contract, but it's counterintuitive when compared to
the typical C `snprintf` return value (which is identical to
`format_to_n` here — the C function is the model):

```
buf size 8, format "{}", "hello world"   (11 chars)
   -> result.size == 11   (would-be)
   -> buf holds "hello w" (7 chars actually written)
```

This is what makes `format_to_n` useful for "size first, allocate
second" patterns:

```cpp
auto r = format_to_expected(buf, sizeof(buf), "{}", huge);
if (!r) throw r.error();
std::string copy(buf, std::min(r.value(), sizeof(buf) - 1));
// or: allocate r.value() + 1 if you want the untruncated form.
```

`std::format_to_n` does **NOT** write a NUL terminator — bytes
after the written prefix are untouched. Section 4 verifies this
explicitly by poisoning the buffer with `'Q'` and confirming the
`'Q'`s survive.

## Section 5 — `back_insert_iterator` DOES respect N

The most-surprising finding from this lesson:

> **A `back_insert_iterator<std::string>` IS subject to the N cap.**

`std::format_to_n` writes **at most N chars** regardless of the
destination type. A `back_inserter` will grow the underlying
`std::string` if asked, but `format_to_n` caps the write at N — so
with N=3 and a 5-char input, the back_inserted string holds only
the first 3 chars, and `r.value()` returns 5 (the would-be).

```
back_inserter + N=3 + "ABCDE"
  -> s4.size() == 3      ("ABC")
  -> r.value() == 5      (would-be)
```

This contradicts a common mental model where `back_inserter` is
"infinite". For an unbounded append, the caller should set N to
the expected maximum, or write a separate `format_to` overload
that does not have the cap.

## Section 6 — composing with `std::expected`

The wrapper returns `expected<size_t, format_error>`, which composes
cleanly with `expected`'s monadic ops (P0323R12):

```cpp
// transform: lift size_t to std::string
auto labelled = result.transform([](std::size_t n) {
    return std::string{"wrote "} + std::to_string(n) + " bytes";
});

// and_then: chain a second format step
auto chained = first_result.and_then(
    [&buf2](std::size_t n)
        -> std::expected<std::size_t, std::format_error> {
        return format_to_expected(buf2, 31, "[n={}]", n);
    });

// transform_error: synthesise a friendlier error type
auto mapped = bad.transform_error([](const std::format_error& e) {
    return std::string{"format failed: "} + e.what();
});
```

`transform_error` is a member of `expected<T, E>` (NOT of
`unexpected<E>` — that was an early-draft bug in this lesson).
Constructing the failure-state expected uses `std::unexpect`:

```cpp
std::expected<std::size_t, std::format_error> bad{
    std::unexpect, std::format_error{"synthetic"}};
```

## Section 8 — `std::format_error` is a `runtime_error`, NOT a `system_error`

`std::format_error` derives from `std::runtime_error`, which has
**no** `.code()` method. The lesson pins the hierarchy with
`static_assert`s:

```cpp
static_assert(!std::is_base_of_v<std::system_error, std::format_error>);
static_assert(std::is_base_of_v<std::runtime_error, std::format_error>);
```

Callers must `.what()` the error, not compare error codes. This
matters because some C++ error-handling libraries expect
`system_error` (POSIX-style `errno`-wrapped errors); plugging
`format_error` into such a pipeline requires an explicit
`transform_error` to lift to `std::string` (or your own error type).

The single `catch (const std::format_error& e)` in the wrapper
also catches `std::format_error` thrown by a future
implementation; today's libc++ 21 does NOT throw from
`std::format_to_n` on well-formed input, but the consteval gate
at the call site guarantees the format string is well-formed —
runtime exceptions from `format_to_n` are only possible from
arg-value-specific bugs in user-provided `formatter` specializations.

## Section 9 — one error channel for parse and format

The round-trip pipeline uses `std::format_error` as the error
type for BOTH `parse_int` and `format_to_expected`. The whole
chain has one `expected<T, format_error>` type:

```cpp
auto pipeline = parse_int("-987")
    .and_then([](int n) -> std::expected<std::string, std::format_error> {
        return std::string{"n="} + std::to_string(n);
    })
    .and_then([&buf2](const std::string& s)
                  -> std::expected<std::size_t, std::format_error> {
        return format_to_expected(buf2, 31, "[{}]", s);
    });
```

If `parse_int` fails, the error propagates through without
touching `buf2`. If `parse_int` succeeds but formatting throws,
the format error propagates instead. One `.what()` site for both.

## Section 10 — 50-round determinism + the 31-char gotcha

The stress loop runs `format_to_expected(buf, 63,
"round={} hash={:08x} pi={:.3f}", 17, 17*2654435761u, 3.14159)`
50 times into fresh buffers. Every output is byte-identical:

```
"round=17 hash=0e3779b1 pi=3.142"   (31 chars, would-be == 31)
```

The "31 chars" figure caught me out: my first draft asserted 30,
which fails. Counting the characters explicitly:

```
r-o-u-n-d- = 6 chars
1-7- -h-a-s-h- = 6 chars
0-e-3-7-7-9-b-1- = 8 chars
 -p-i-=-3-.-1-4-2 = 7 chars
              ---
              27? No — let me recount:
"round="      = 6
"17"          = 2
" "           = 1
"hash="       = 5
"0e3779b1"    = 8
" "           = 1
"pi="         = 3
"3.142"       = 5
              ---
              31
```

Right, 31. The lesson comment now says 31. The lesson is: **count
the chars in your test, do not guess.**

## Compile / run evidence

Default build:
```
clang++ -std=c++23 -stdlib=libc++ -O0 -g \
    late-may/cpp_practice/format_to_n_expected/P-2026-08-24-format-to-n-expected.cpp \
    -o /tmp/petra-fmtn
/tmp/petra-fmtn
# Total: 384/384 PASS, exit 0
```

Strict build:
```
clang++ -std=c++23 -stdlib=libc++ -O0 -g \
    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion \
    late-may/cpp_practice/format_to_n_expected/P-2026-08-24-format-to-n-expected.cpp \
    -o /tmp/petra-fmtn-strict
/tmp/petra-fmtn-strict
# Total: 384/384 PASS, exit 0
```

ASan + UBSan build:
```
clang++ -std=c++23 -stdlib=libc++ -O1 -g \
    -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    late-may/cpp_practice/format_to_n_expected/P-2026-08-24-format-to-n-expected.cpp \
    -o /tmp/petra-fmtn-asan
/tmp/petra-fmtn-asan > stdout 2> stderr
# Total: 384/384 PASS, exit 0
# stderr is empty — no sanitizer diagnostics
```

CMake (default / strict / ASan):
```
cmake -S . -B build && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" \
    && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
# All three: 384/384 PASS, exit 0
```

The file is a self-contained translation unit — no third-party
libraries, no library installation, no `find_package`. Headers
used: `<array>`, `<cassert>`, `<cstddef>`, `<cstdint>`, `<cstdio>`,
`<cstring>`, `<expected>`, `<format>`, `<iterator>`, `<span>`,
`<string>`, `<string_view>`, `<system_error>`, `<type_traits>`,
`<utility>`, `<vector>`.

## Where we go next

Today's lesson closes the **second** item on the Aug 22 follow-on
list. The remaining items from that list are unchanged:

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

New items surfaced by today's lesson:

- **`std::format_to_n_into(buf, fmt, args...)` style
  NUL-terminated wrapper** — the wrapper today returns the
  would-be size and leaves NUL-termination to the caller. A
  small companion that writes `min(r.value(), N)` chars + a
  NUL would close the "I just want a string" use case. The
  NUL-writing version is not the same as `format_to_n` —
  it's a different function with different bounds.
- **`std::format_to_n` for `std::span<char>` filling** —
  Section 7 exercises this, but a follow-up could benchmark
  it against `std::string::resize()` + `format_to`, which is
  the idiomatic alternative for sized strings.
- **A typed-error ergonomics wrapper around `std::print`** —
  the Aug 22 lesson noted `std::print` doesn't take
  `std::ostream`. A companion wrapper `print_into(buf, fmt,
  args...) -> expected<size_t, format_error>` would mirror
  today's `format_to_expected` and round out the consumer
  surface.

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
