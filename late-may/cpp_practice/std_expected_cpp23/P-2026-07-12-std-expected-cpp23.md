# P-2026-07-12 — `std::expected<T, E>` (C++23): error handling without exceptions

## Headline

Today's lesson is the C++23 result type: `std::expected<T, E>` — a
discriminated union of a value of type `T` and an error of type `E`,
with monadic composition, comparison operators, and a `std::optional`-like
API for access. It's the standard answer to "should this function throw
or return a `pair<bool, T>` or return a `std::variant<T, E>`?"

The lesson is built around the `psp_span_lib` release-pipeline context
where I'm shipping a parser on top of `std::span`. Exceptions don't
compose well across the span boundary (a span of bytes is supposed to be
lightweight; throwing through a low-level parser poisons every call
site), and `pair<bool, T>` can't carry an error payload. `std::expected`
fits perfectly: the parser returns the int on success and a tagged
error enum on failure, and consumers chain with `and_then` / `or_else`.

## Where this fits in the arc

```
Jun  9  std::move / rvalue refs                  move semantics
Jun 12  std::move_if_noexcept                     noexcept + move decision
Jun 13  enable_shared_from_this                   smart pointers + this
Jun 15  std::visit + stateful visitors            variant visitation
Jun 16  Inventory multi-file                      separate compilation
Jun 21  variant vs polymorphism                   type-erasure tradeoffs
Jul  9  std::span (C++20)                         non-owning view, 1-D
Jul 10  std::mdspan (C++23)                       non-owning view, N-D
Jul 11  std::mdspan + custom accessor             accessor policy seam
Jul 12  std::expected (C++23)                     result type, monadic  ← today
```

The psp_span_lib thread is the "shipping a library" arc. `std::expected`
is the error type I'd want at the parsing boundary. `std::span` is the
input; `std::expected` is the output. Together: `std::expected<int,
ParseError> parse_int_from_span(std::span<const char>)`.

## Why `std::expected` matters

Three design alternatives, and why each is worse than expected:

1. **Throw an exception.** Control flow is hidden, every call site has
   to think about `try/catch`, and exceptions are slow on the failure
   path. Bad for a parser that you want to be predictable and embeddable.

2. **`std::optional<T>`.** Says "there might not be a value" but throws
   away the *reason*. The consumer can't say "if it's empty, distinguish
   empty-input from invalid-digit from overflow" — they have to do extra
   work (call another function, check a global, etc.) to know what went
   wrong.

3. **`std::pair<bool, T>`** (or `struct { bool ok; T value; }`).
   - `T` must be default-constructible (so you can return `{false, T{}}`).
   - The `bool` is redundant with "did we set `T`?"
   - No error payload.
   - No monadic operations.
   - Verbose to construct and consume.

4. **`std::variant<T, E>`** with `std::visit`. Closer, but:
   - `visit` requires an overload set (`overloaded` lambda trick) for
     every consumer.
   - No `.value()`, no `.value_or()`, no `has_value()`-style predicates
     by default (just `std::holds_alternative`).
   - No monadic operations.
   - Tag-ordered by index, not by the underlying value's natural order.

`std::expected` is the C++23 standardized version of the Rust `Result<T,
E>` and the Haskell `Either e a`. It has:
- `T` + `E` payloads (no constraint on E being default-constructible).
- `value()`, `value_or()`, `error()`, `operator*`, `operator->` for
  access.
- `has_value()` / `operator bool()` for the state.
- Monadic `and_then` / `or_else` / `transform` / `transform_error`.
- `==` and `!=` (the libc++ I'm on does not provide `<` / `<=>`).
- A `std::bad_expected_access<E>` exception type for the "I demanded
  the value from a failure-state expected" case.

The lib check is `__cpp_lib_expected == 202211` (C++23 final).

## Section 1 — Construction

```cpp
std::expected<int, ParseError> ok1 = 42;                 // T ctor
std::expected<int, ParseError> ok2{std::in_place, 42};  // in_place ctor
std::expected<int, ParseError> bad1 = std::unexpected{ParseError::Empty};
std::expected<int, ParseError> bad2{std::unexpect, ParseError::OutOfRange};
std::expected<int, ParseError> def{};                   // T must be default-ctible
```

Two key points:
- `std::unexpected<E>` is the failure tag. You can't construct a
  failure-state expected from a bare `E`; you must wrap it.
- `std::unexpect` is the failure analog of `std::in_place` — it disambiguates
  "construct the failure payload in place" from "implicit-convert the
  argument to the success type".

Output:
```
def = 0 (default-constructed T; .has_value() = true)
ok1 = 42  ok2 = 42
bad1 has_value = false  bad2 has_value = false
bad1.error() = Empty
bad2.error() = OutOfRange
```

## Section 2 — Predicates and access

```cpp
ok.has_value()      // bool: true if success
ok.operator bool()  // same thing
*ok                  // precondition: has_value() (else UB)
ok->                 // member access; precondition: has_value()
ok.value()           // returns T; throws bad_expected_access<E> on failure
ok.value_or(default) // returns T or default; never throws
ok.error()           // returns E; UB on success
```

The interesting case: `ok.value()` on a failure-state expected throws
`std::bad_expected_access<ParseError>`, and the exception carries the
error payload. The `try`/`catch` in the lesson demonstrates this:

```
caught bad_expected_access<ParseError>: error() = NotANumber
```

The lesson also prints `ok.value_or(-1) = 100` and `bad.value_or(-1) = -1` —
the safe accessor that picks a default on failure.

## Section 3 — Monadic operations

Four functions, all returning a new `expected`:

| Function          | Input  | Function type           | Output type              | Skips when...  |
|-------------------|--------|-------------------------|--------------------------|----------------|
| `transform(f)`    | `T`    | `T -> U`                | `expected<U, E>`          | failure-state  |
| `and_then(f)`     | `T`    | `T -> expected<U, E>`   | `expected<U, E>`          | failure-state  |
| `transform_error(f)` | `E` | `E -> F`                | `expected<T, F>`          | success-state  |
| `or_else(f)`      | `E`    | `E -> expected<T, F>`   | `expected<T, F>`          | success-state  |

The functions don't run unless the expected is in the right state. That's
the monadic property: the lambda body sees only the "valid" state, and
the chain short-circuits if any step is in the wrong state.

The lesson's chain demo:

```cpp
std::expected<int, std::string> numbered = 20;
auto chained = numbered
    .and_then([](int n) -> std::expected<int, std::string> {
        if (n % 2 != 0) return std::unexpected{"odd input"};
        return n * 2;
    })
    .and_then([](int n) -> std::expected<int, std::string> {
        return n + 1;
    });
// chained == 41   (20 -> *2 -> +1)
```

If you start with `21`, the first and_then fails and the second is
skipped. The lesson also shows short-circuiting on an upstream failure:

```
chained and_then = 41 (20 -> *2 -> +1)
odd_chained error = odd input (short-circuit on first step)
chained_bad (short-circuits) error = upstream
```

The 21-case is the one I almost got wrong. The first version of the
demo started with `21` and then dereferenced the chain with `*chained`,
which is **UB on a failure-state expected** — the binary printed
`543450223` (uninitialized storage). Fixed by always checking
`has_value()` first. Worth noting because that's the exact bug a
beginner would write and then blame on `std::expected`.

## Section 4 — Comparison operators

`std::expected` provides `==` and `!=`. It does **not** provide `<` or
`<=>` in this libc++ (verified by the compile error before I removed the
demo line). That's a deliberate library choice — orderings on a "value
OR error" union don't have a natural semantics, and forcing a
`<` would invite people to write `if (e < some_err)` to test "is this
error worse?" which is not a thing.

The comparison rules:

- `expected<T, E> == expected<T, E>`: T == T, E == E.
- `expected<T, E> == T` (success state, comparing against a T).
- `expected<T, E> == unexpected<E>` (failure state, comparing against
  an unexpected<E>).
- The two cross-comparisons (`expected == E` on a value-state, or
  `expected == T` on an error-state) do **not** exist.

```
a == b : false
a == 10: true
c == unexpected<str>: true
c == unexpected<str>: false
c == d : false (errors differ)
c != d : true
```

## Section 5 — A small realistic use-case: span-based int parser

This is the keystone demo. It puts `std::span` (Jul 9) and
`std::expected` (today) together into a working API:

```cpp
std::expected<int, ParseError> parse_int_from_span(std::span<const char> s) {
    if (s.empty()) return std::unexpected{ParseError::Empty};
    if (s.front() == '+' || s.front() == '-') return std::unexpected{ParseError::LeadingSign};
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return std::unexpected{ParseError::NotANumber};
        v = v * 10 + (c - '0');
        if (v > 1'000'000) return std::unexpected{ParseError::OutOfRange};
    }
    return v;
}
```

The function takes a non-owning view, never allocates, and either returns
the int or a typed error. The consumer can pattern-match on the error
enum without unwinding the stack. Output:

```
parse("12345") = 12345
parse("") error: Empty
parse("+9") error: LeadingSign
parse("12a3") error: NotANumber
parse("9999999") error: OutOfRange
```

The compose demo shows the monadic chain in the wild:

```cpp
auto r = parse_int_from_span(sp)
    .and_then([](int n) -> std::expected<int, ParseError> {
        if (n < 0) return std::unexpected{ParseError::OutOfRange};
        return n * 2;
    });
```

`parse("50")` → 100. `parse("abc")` → `NotANumber`. The error from the
first step propagates without the second step running.

This is the model I want for psp_span_lib's parse layer. Throw it across
a parser step and you can implement an entire pipeline as a chain of
`.and_then()`. No exceptions, no `pair<bool, T>`, no `std::visit`
overload-set boilerplate.

## Section 6 — Comparison with alternative designs

```
== Section 6: alternatives to std::expected ==
pair<bool,T>: failed (no error info)
variant holds error: Empty
expected picks: T + E payload, monadic ops, exceptions only on demand.
```

The output shows:
- `pair<bool, T>` carries no error info — the consumer only knows "no
  value", not "why no value".
- `std::variant<int, ParseError>` needs `std::visit` and a lambda
  overload set to consume (the demo uses an `if constexpr` lambda).
- `std::expected` is the designed-for-this tool.

The C++23 std::expected also has two properties the alternatives don't:
- The error type `E` doesn't have to be default-constructible.
- The success value `T` doesn't have to be default-constructible
  (as long as you don't default-construct the expected itself).

## Section 7 — Corner cases

Three things worth knowing:

1. **`expected<T, T>`** — when the success and error types are the same.
   `std::expected<int, int>` works; the disambiguator is the value
   category. Output:
   ```
   dual = 5  dual_err error = -1
   ```

2. **`expected<void, E>`** — for "operation that either succeeded or
   failed with a reason" without a payload on success. The lesson
   shows `.emplace()` to mark a previously-failure-state expected as
   successful:
   ```
   op failed: disk full
   op_result.has_value() after emplace = true
   ```

3. **Don't call `.error()` on a success-state expected** — that's UB,
   just like `*optional{}` is UB. The lesson has a comment with the
   code that would UB but does not run it.

## Format specialization for `ParseError`

`std::print` and `std::println` use `std::format`, which dispatches
through `std::formatter<T, char>`. For an enum `ParseError`, the
standard doesn't ship a formatter, so I added one in the program:

```cpp
template <>
struct std::formatter<ParseError> : std::formatter<std::string_view> {
    auto format(ParseError e, format_context& ctx) const {
        std::string_view name = "?";
        switch (e) { /* set name from the enum */ }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};
```

Inheriting from `std::formatter<std::string_view>` gets you the parse
step (handling width, alignment, fill chars) for free; only the
`format` member is overridden. This is a clean pattern for any enum
you want to print.

## Sizeof probes (the runtime cost of `std::expected`)

```
sizeof(expected<int, std::string>)  = 32    // int + string + bool + padding
sizeof(expected<int, ParseError>)   = 8     // int + 1-byte enum (packed)
sizeof(expected<std::string, int>)  = 32    // string + int + bool + padding
```

`std::expected` is essentially `{union{T,E}; bool has_value;}` plus
any required alignment. The discriminator (`has_value`) lives next to
the payload, so the type is the size of `max(sizeof(T), sizeof(E))`
plus one byte plus padding to the alignment of the larger side.

For `int` and a 1-byte enum, that's 4 + 1 + 3 (padding) = 8 bytes.
For `int` and `std::string` (24 bytes on libc++ with SSO), that's
24 + 1 + 7 (padding to 8) = 32 bytes — the same size as `std::string`
alone, because `int` fits in the string's tail padding and the
discriminator is just a byte.

The lesson's takeaway: **the cost of `std::expected<T, E>` is the
cost of `T` (or `E`, whichever is larger), plus one byte for the
discriminator and padding to the next alignment boundary.** There
is no heap allocation; the union sits inline.

## Build & run (what I actually ran)

```bash
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -o P-2026-07-12-std-expected-cpp23 \
    P-2026-07-12-std-expected-cpp23.cpp
./P-2026-07-12-std-expected-cpp23

# ASan build
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -fsanitize=address,undefined \
    -o P-2026-07-12-std-expected-cpp23-asan \
    P-2026-07-12-std-expected-cpp23.cpp
./P-2026-07-12-std-expected-cpp23-asan
```

Both builds compile clean (`-Wall -Wextra -Wpedantic`, no warnings) and
run to exit 0. ASan reports no memory errors. The `__cpp_lib_expected =
202211` macro confirms C++23.

## What I didn't cover (next-session candidates)

- **`std::expected` and exceptions** — the `bad_expected_access<E>`
  type can be caught as `std::bad_expected_access<void>` for the case
  where you don't care about the error payload. The lesson only caught
  the typed form.
- **`std::expected<T, E>` with a move-only T or E** — works, but the
  copy-constructible / copy-assignable members of `expected` are
  conditionally defined (deleted if T or E is move-only). Worth a
  dedicated demo.
- **`std::expected` and `std::optional` interop** — `expected<T, E>`
  has no implicit conversion to `optional<T>` and vice versa, but you
  can write one with `transform` (drop the error) or `transform_error`
  (replace with a sentinel).
- **Using `std::expected` in a `co_await` / coroutine** — `operator
  co_await` doesn't have a standard `expected` integration, but
  Coroutines TS is the obvious next step. The psp_span_lib
  coroutine-based parser would use this.
- **`std::expected<T, std::error_code>`** for low-level APIs that
  want POSIX-style error reporting. The pattern is "the T is a
  successful return value (e.g. file size), the E is the system
  error code." Pairs well with `std::error_category`.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        └── std_expected_cpp23/                                       # NEW (the lesson)
            ├── P-2026-07-12-std-expected-cpp23.cpp                   #    the tour program
            └── P-2026-07-12-std-expected-cpp23.md                    #    this file
```

No `psp_span_lib` changes today. No release tag today. The lesson is
about `std::expected` as a tool; the next session can wire it into
`psp_span_lib` as the parser's return type.

## Next steps

The psp_span_lib release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag. (Unchanged
  from Jul 11.)
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS or
  .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.5.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform and
  SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow feature
  work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam. The lesson
  copies of `release.yml` and `release_matrix.yml` are ready; the
  deploy copies in `.github/workflows/` are blocked on the PAT.

C++ language threads still open:

- **Wire `std::expected` into psp_span_lib's parser layer** — the
  natural fit for what I learned today. A `parse_int_from_span` (or
  `parse_double_from_span`) returning `expected<T, ParseError>` is
  one of the most useful pieces a small library can ship.
- **`std::submdspan`** (P2630) — when libc++ lands it, my Jul 10
  Section 5 becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray) — the
  standard version of what I hand-rolled in Jul 11.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear algebra
  built on mdspan.
- **mdspan ↔ std::ranges deduction guides** — span got `span(R&&)`
  deduction guides for contiguous ranges in C++23; mdspan will get
  the analogous multi-D version eventually.

The first new step (wire `std::expected` into psp_span_lib) is the
most natural follow-on; everything else is either C++26+ (not yet
buildable) or release-pipeline (orthogonal to the C++ learning).
