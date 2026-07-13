# P-2026-07-13 — Wire `std::expected` into `psp_span_lib`'s parser layer

## Headline

The Jul 12 `std::expected<T, E>` tour introduced the result type as a
concept. Today's lesson wires it into the **psp_span_lib** library: a
`parse_int(psp::Span<const char>) -> std::expected<int, ParseError>`
function that demonstrates how `std::span` (Jul 9), `psp::Span` (Jun 14),
and `std::expected` (Jul 12) compose into a small but realistic
embedded-DSL parser.

This is the first lesson that **links against the installed psp_span_lib
archive** (`/tmp/psp_install/lib/libpsp_span_lib.a`), proving the
library + result-type combination is shippable as a real consumer.

## Where this fits in the arc

```
Jun  9  std::move / rvalue refs                  move semantics
Jun 12  std::move_if_noexcept                     noexcept + move decision
Jun 13  enable_shared_from_this                   smart pointers + this
Jun 14  std::span (hand-rolled, psp::Span)        non-owning view, 1-D
Jun 15  std::visit + stateful visitors            variant visitation
Jun 16  Inventory multi-file                      separate compilation
Jun 28  psp_span as CMake INTERFACE library       installable target
Jun 30  psp_span_lib owns its instantiation TU    static library
Jul  1  install + find_package                    package config
Jul  2  find_package(fmt)                         third-party
Jul  3  CPack TGZ package                         redistributable
Jul  9  std::span (C++20)                         std version
Jul 10  std::mdspan (C++23)                       N-D view
Jul 11  std::mdspan custom accessor               policy seam
Jul 12  std::expected (C++23)                     result type          ← concept
Jul 13  wire std::expected into psp_span_lib      parser layer          ← today
```

Three threads are now converging on `psp_span_lib` as the
"shipping a library" arc:

1. **Templates / non-owning views** (`psp::Span<T, Extent>` → `std::span`).
2. **Error handling** (`std::expected<T, E>` for typed failures).
3. **Distribution** (`psp_span_lib` → install → CPack → consumer).

A consumer that does `target_link_libraries(... psp_span_lib)` and
includes `<psp_span/span.h>` can now build a parser that returns
`std::expected<int, ParseError>` and not pay anything they didn't ask
for.

## Why `parse_int` on `psp::Span<const char>`

The psp_span_lib library was built to be a *non-owning view of bytes*
that you can pass around without copying. A parser is the natural
use-case:

- The parser does not own the input bytes; the caller does. Passing
  a `psp::Span<const char>` says "here's a window into my buffer; you
  may borrow it but you don't get to keep it."
- The parser either returns the int (success) or reports *which* failure
  occurred (Empty / LeadingSign / NotADigit / Overflow). `std::optional`
  can't carry that information; throwing exceptions poisons every call
  site; `std::pair<bool, int>` has no error payload.
- `std::expected<int, ParseError>` is the designed-for-this tool: the
  success type is `int`, the failure type is the typed enum, and the
  parser can be composed into a pipeline via `.and_then(...)` and
  `.transform(...)` without unwinding the stack.

## The parser

```cpp
std::expected<int, ParseError> parse_int(psp::Span<const char> s) noexcept {
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }
    if (s.front() == '+' || s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }
    std::int64_t acc = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return std::unexpected{ParseError::NotADigit};
        }
        acc = acc * 10 + (c - '0');
        if (acc > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            return std::unexpected{ParseError::Overflow};
        }
    }
    return static_cast<int>(acc);
}
```

Four failure modes, each with a typed error:

| Failure          | When                                  |
|------------------|---------------------------------------|
| `Empty`          | `s.size() == 0`                       |
| `LeadingSign`    | `s.front()` is `+` or `-`             |
| `NotADigit`      | any character outside `'0'..'9'`      |
| `Overflow`       | accumulator exceeds `INT_MAX`         |

The accumulator type is `std::int64_t` so a single per-iteration check
against `std::numeric_limits<int>::max()` catches the moment the int
would wrap. For larger ranges, `__builtin_mul_overflow` would be the
right call; for `int`, this guard is enough.

The function is `noexcept` because every failure becomes an
`expected`-failure, never an exception. This matters for the
`std::move_if_noexcept` decision at call sites (the Jun 12 topic):
consumers that store the result in a container know the parser cannot
throw, so they can move without copy fallback.

## Section 1 — basic parse_int

```
== Section 1: parse_int on a psp::Span<const char> ==
  parse("12345") = 12345
  parse("0") = 0
  parse("") error: Empty
  parse("+9") error: LeadingSign
  parse("12a3") error: NotADigit
  parse("9999999999") error: Overflow
```

The four error paths are exercised; the success path is exercised at
two scales (`12345` and `0`) to make sure the accumulator handles
single-digit inputs without falling through into any of the failure
branches.

## Section 2 — monadic composition

The keystone demo for `std::expected`. Two extra functions chain with
the parser:

```cpp
std::expected<int, ParseError> validate_positive(int n) noexcept {
    if (n <= 0) return std::unexpected{ParseError::Overflow};
    return n;
}

std::expected<int, ParseError>
double_if_positive(psp::Span<const char> s) noexcept {
    return parse_int(s)
        .and_then(validate_positive)
        .transform([](int n) { return n * 2; });
}
```

Output:

```
== Section 2: monadic composition (.and_then / .transform) ==
  double("50") = 100
  double("-1") error: LeadingSign
  double("abc") error: NotADigit
  double("0") error: Overflow
  double("100") = 200
```

Three properties of the chain:

- **Short-circuit on parse failure**: `double("-1")` and `double("abc")`
  report the parse error (`LeadingSign`, `NotADigit`); `validate_positive`
  is never called.
- **Short-circuit on validation failure**: `double("0")` parses
  successfully (`0`), but `validate_positive(0)` returns `Overflow`.
- **Transform only on success**: `double("100")` parses to `100`,
  validates (it's positive), then `.transform(...)` doubles to `200`.

This is the function-as-pipeline-step pattern. Each step is an
independent unit; a fourth step could compose them via another
`.and_then(...)` without touching the existing code.

## Section 3 — error formatting

`std::format("{}", r.error())` works because of the
`std::formatter<ParseError>` specialization declared at file scope:

```cpp
template <>
struct std::formatter<ParseError> : std::formatter<std::string_view> {
    auto format(ParseError e, std::format_context& ctx) const {
        std::string_view name = "?";
        switch (e) {
            case ParseError::Empty:       name = "Empty";       break;
            case ParseError::LeadingSign: name = "LeadingSign"; break;
            case ParseError::NotADigit:   name = "NotADigit";   break;
            case ParseError::Overflow:    name = "Overflow";    break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};
```

Output:

```
== Section 3: error formatting ==
  formatted error: NotADigit
  chained error: LeadingSign
```

The first line uses `std::format` directly; the second line uses the
chained `parse_int(...).and_then(validate_positive)` and prints the
short-circuited error from the parse step.

## Section 4 — comparison operators

`std::expected<T, E>` provides `==` and `!=` (this libc++ does not
provide `<` or `<=>`):

```
== Section 4: comparison operators ==
  ok == 42: 1
  ok != 99: 1
  bad == unexpected<Empty>: 1
  bad == unexpected<Overflow>: 0
  ok == also_ok: 1
  bad == also_bad (different errors): 0
```

The four cases:

| Expression                                         | Result | Why                                       |
|----------------------------------------------------|--------|-------------------------------------------|
| `expected{42} == 42`                               | true   | success state, value matches              |
| `expected{42} != 99`                               | true   | success state, value differs              |
| `expected{Empty-err} == unexpected{Empty-err}`    | true   | failure state, error matches              |
| `expected{Empty-err} == unexpected{Overflow-err}` | false  | failure state, error differs              |

Two `expected`s are equal iff both state and payload match. The
cross-comparisons (`expected == T` on a failure state, or `expected ==
E` on a success state) are deleted by design — there's no natural
interpretation.

## Section 5 — psp_span_lib integration check

This is the proof-of-life for the consumer-side wiring:

```
== Section 5: psp_span_lib integration ==
  digits.data() == src.data()+6: 1
  digits.size_bytes() = 5 (expected 5)
  parse(digits) = 12345
  sizeof(expected<int, ParseError>) = 8
  sizeof(ParseError) = 4
  __cpp_lib_expected = 202211
```

Three checks:

1. **Non-owning aliasing**: `digits.data() == src.data()+6` is `1`,
   proving that `psp::Span<const char>::subspan(6, 5)` borrows into
   the source string's buffer without copying.
2. **The parser runs on the subview**: `parse(digits)` returns
   `12345`, the literal value at bytes 6..11 of `"hello 12345 world"`.
3. **Toolchain + sizeof**: `__cpp_lib_expected == 202211` confirms
   C++23 final. `sizeof(expected<int, ParseError>) == 8` matches the
   Jul 12 measurement.

The `sizeof` math: `int(4) + ParseError(4) = 8`, with the bool
discriminator packed into alignment slack via empty-base optimization
in libc++. (Note: `enum class` with no fixed underlying type defaults
to `int` on this toolchain, not `unsigned char` as I'd assumed in the
draft comments — caught it after the first run printed `sizeof =
4`. Fixed the comments in-place.)

## Section 6 — bridging to `std::optional`

`std::expected` doesn't implicitly convert to `std::optional`, but the
`.transform_error()` member can collapse the failure payload into a
sentinel:

```cpp
auto r = parse_int(as_span(std::string{""}));
auto with_sentinel = r.transform_error(
    [](ParseError) -> int { return -1; });
```

Output:

```
== Section 6: bridging to std::optional / sentinel ==
  with_sentinel = -1 (sentinel for any error)
  std::optional view has_value: 0
```

For a "give me the value or a sentinel" pipeline, this is cleaner
than writing `if/else` at every call site. The `std::optional` view
is the manual bridge for cases where you want a clean
"value-or-nothing" abstraction; `.transform_error()` is the bridge for
cases where you want a "value-or-default" abstraction.

## Bridging `std::string` → `psp::Span<const char>`

The lesson uses a small helper:

```cpp
psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>{s.data(), s.size()};
}
```

`psp::Span` does not have a `std::string` constructor (the library was
designed for C-array / `std::array` / `std::vector` / raw `(ptr, len)`
pairs). The `(ptr, len)` constructor is the most general form and
bridges any contiguous-storage container to the span.

If `psp_span_lib` ever grew a `std::string` ctor, the helper would
become a one-liner; until then, the `(ptr, len)` form is the standard
pattern.

## Build & run (what I actually ran)

```bash
# 1. Build and install psp_span_lib into /tmp/psp_install.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_span_lib
cmake -S . -B build-cron -DCMAKE_BUILD_TYPE=Release
cmake --build build-cron
cmake --install build-cron --prefix /tmp/psp_install

# 2. Compile the lesson against the installed library.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_expected
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -I /tmp/psp_install/include \
    P-2026-07-13-psp-parser-expected.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o P-2026-07-13-psp-parser-expected

./P-2026-07-13-psp-parser-expected

# 3. ASan build.
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -fsanitize=address,undefined \
    -I /tmp/psp_install/include \
    P-2026-07-13-psp-parser-expected.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o P-2026-07-13-psp-parser-expected-asan

./P-2026-07-13-psp-parser-expected-asan
```

Both builds compile clean (`-Wall -Wextra -Wpedantic`, no warnings)
and run to exit 0. ASan reports no memory errors. The
`__cpp_lib_expected = 202211` macro confirms C++23 final.

## What I didn't cover (next-session candidates)

- **A real consumer that uses `find_package(psp_span_lib)`** — today
  the lesson hard-codes the `/tmp/psp_install` path. A proper
  consumer-side `CMakeLists.txt` with `find_package(psp_span_lib
  REQUIRED)` would close the loop on the install-then-consume pattern
  from Jul 1.
- **A `parse_int` overload for `std::span<const char>`** — using the
  standard span alongside the psp span, to show they compose the same
  way. The hand-rolled `psp::Span` and the C++20 `std::span` are
  algorithm-compatible for `parse_int`'s usage; a templated overload
  could accept either via `Span`-like duck typing or a concept.
- **`parse_double`** — same shape, different accumulator (handle the
  decimal point and exponent). Demonstrates how the parser layer
  generalizes.
- **`std::expected` with a move-only `E`** — the conditional
  copy-constructibility of `expected` (copy-ctor deleted if either
  payload is move-only) is worth a dedicated demo.
- **`std::expected` in a coroutine** — `co_return std::expected<T,E>`
  is the natural way to lift a parser into a coroutine-based pipeline.
- **Wiring `std::expected` into `psp_span_lib` proper** — adding
  `<psp_span/parser.h>` (or similar) as a sibling header in the
  library would let consumers `target_link_libraries(... psp_span_lib)`
  and `#include <psp_span/parser.h>` to get both the span and the
  parser. That's a real "ship it" move; today the lesson is *built
  against* the library but doesn't extend it.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        └── psp_parser_expected/                                    # NEW (the lesson)
            ├── P-2026-07-13-psp-parser-expected.cpp                #    the program
            └── P-2026-07-13-psp-parser-expected.md                 #    this file
```

No `psp_span_lib` source changes today. The lesson demonstrates how a
**consumer** of `psp_span_lib` writes a parser using the library's
non-owning views plus `std::expected` for errors.

## Next steps

The psp_span_lib release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS or
  .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.5.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform and
  SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow feature
  work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **Wire `parse_double` (or other numeric parsers) into a real
  `<psp_span/parser.h>`** — the natural follow-on to today's lesson
  is to add a parser header to psp_span_lib itself, so consumers can
  `#include <psp_span/parser.h>` and get the parser layer for free.
- **`std::expected` and coroutines** — `co_return std::expected<T,E>`
  composes with the parser layer for an async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it, the Jul 10
  Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray) — the
  standard version of the Jul 11 hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear algebra
  built on mdspan.

The most natural follow-on is the `parse_double` + real
`<psp_span/parser.h>` move: it would let a third consumer exercise the
parser in a way that's actually portable (not hard-coded to
`/tmp/psp_install`), and it's a 100-line addition to the library
proper.
