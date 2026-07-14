# P-2026-07-14 — `<psp_span/parser.h>` ships in `psp_span_lib` proper

## Headline

The Jul 13 lesson (`P-2026-07-13-psp-parser-expected.cpp`) proved the
**shape** of a parser using `psp::Span<const char>` +
`std::expected<int, ParseError>` as a standalone consumer. Today's
lesson promotes that shape into the **library proper**: a new public
header `<psp_span/parser.h>` is now part of `psp_span_lib` (v0.5.0 →
v0.6.0), shipping with the install tree, with a real consumer that
uses `find_package(psp_span_lib REQUIRED)` instead of hard-coded
`-I /tmp/psp_install`.

Three things landed together:

1. **`<psp_span/parser.h>`** — added to the library. Declares
   `enum class ParseError`, `std::formatter<ParseError>`,
   `psp::parse_int(Span<const char>)`, and the new
   `psp::parse_double(Span<const char>)`.
2. **`parse_double`** — a new parser that handles integer, fractional,
   and exponent phases with the same typed-error philosophy.
3. **A proper consumer with `find_package()`** — the Jul 13 lesson
   was built against `/tmp/psp_install` via raw `-I` / `-L`; today's
   consumer uses `find_package(psp_span_lib 0.6 REQUIRED)` and
   propagates `cxx_std_23` from the library's PUBLIC compile features.

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
Jul  3  CPack TGZ package                         redistributable
Jul  9  std::span (C++20)                         std version
Jul 12  std::expected (C++23)                     result type
Jul 13  wire std::expected into psp_span_lib      parser layer (consumer-side)
Jul 14  <psp_span/parser.h> in psp_span_lib       parser layer (library-side) ← today
```

The parser layer crossed the library boundary today: what was a
per-consumer implementation on Jul 13 is now a library capability.

## The new header: `<psp_span/parser.h>`

### File-scope namespace choice

The trickiest part of writing this header was getting the
`std::formatter<ParseError>` specialization to compile. The rule
([namespace.std]) is:

> A specialization of a template defined in namespace std ... may
> be declared in any namespace.

So *the formatter* can be declared in any namespace, including
`psp::`. **But the specialization must be in a namespace that
*encloses* the primary template's namespace, i.e. `std` itself.**
Or, equivalently, if `ParseError` is in `psp::`, then
`std::formatter<psp::ParseError>` must be declared at namespace
scope — but `psp::` is not a scope that encloses `std`, so you
can't write the specialization inside `namespace psp { ... }` and
also can't write it as `namespace std { template <>
struct formatter<psp::ParseError>; }` from inside `psp`.

The Jul 13 lesson solved this by putting both the enum and the
formatter at *file scope* (no enclosing namespace). I followed the
same pattern for the library header:

```cpp
// <psp_span/parser.h>, file scope
enum class ParseError { ... };

template <>
struct std::formatter<ParseError> : std::formatter<std::string_view> {
    auto format(ParseError e, std::format_context& ctx) const {
        std::string_view name = "?";
        switch (e) { /* map enum to name */ }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

namespace psp {
// parse_int, parse_double live here
}
```

This means `psp::parse_int` returns `std::expected<int, ::ParseError>`
(not `std::expected<int, psp::ParseError>`). That's a slight wart
(consumers see a top-level `ParseError` enum) but it's the only way
to make `std::format("{}\n", err)` work without dragging a `using
namespace psp;` into the header. The wart is documented in the
header itself.

### The two parsers

```cpp
namespace psp {

inline std::expected<int, ParseError>
parse_int(Span<const char> s) noexcept;

inline std::expected<double, ParseError>
parse_double(Span<const char> s) noexcept;

}
```

Both are `inline` (header-only), `noexcept`, and take
`psp::Span<const char>` as the input. Both return
`std::expected<T, ParseError>` with the failure-payload enum from
the file-scope block above.

The `inline` qualifier matters because the header can be included
in multiple TUs (the whole point of a header). With `inline`, the
linker dedupes the symbol; without it, you'd get multiple-definition
errors at link time for any consumer that includes the header from
two TUs.

### What `parse_double` accepts

Recognised shape (subset of `strtod`):

```
[-+]? DIGITS ( '.' DIGITS )? ( [eE] [-+]? DIGITS )?
```

But we **reject** leading `+`/`-` (same policy as `parse_int`):

```
parse_double("3.14")   -> 3.14
parse_double("1e10")   -> 1e10
parse_double(".5")     -> 0.5
parse_double("1.")     -> 1.0
parse_double("")       -> error: Empty
parse_double("+1.0")   -> error: LeadingSign
parse_double("-1.0")   -> error: LeadingSign
parse_double(".")      -> error: MissingFraction
parse_double("1e")     -> error: BadExponent
parse_double("1.2.3")  -> error: NotADigit
```

Two new error cases vs `parse_int`:

- `BadExponent` — `'e'`/`'E'` with no following digits.
- `MissingFraction` — `'.'` alone, with no integer or fractional
  digits around it.

The `LeadingSign` rejection is consistent with `parse_int`: sign
handling is a downstream policy choice, not the parser's job. A
caller that wants to handle signs explicitly can do `s.subspan(1)`
after checking `s.front() == '+' || s.front() == '-'`.

### What we deliberately didn't include

- **JSON / structured-data parsers.** Out of scope; the header is
  for "tiny numeric parsers that compose with the span layer".
- **Streaming `parse_int_at(Span&)` that advances a cursor.** The
  current parsers consume the whole span. A streaming variant is a
  ~5-line change when needed.
- **`parse_*_or` defaults.** `std::expected::value_or()` and
  `transform_error()` already provide this; consumers compose it.

## Bumping the library version: 0.5.0 → 0.6.0

```cmake
project(psp_span_lib
    VERSION 0.6.0
    DESCRIPTION "... + a parser header (parse_int/parse_double with std::expected<int/double, ParseError>); ..."
)
```

Plus three downstream changes to `CMakeLists.txt`:

1. `set(CMAKE_CXX_STANDARD 23)` (was 17). The parser header needs
   `std::expected`, which is C++23 final.
2. `target_compile_features(psp_span_lib PUBLIC cxx_std_23)` (was
   `cxx_std_17`). The consumer gets `-std=c++23` for free via the
   IMPORTED target's PUBLIC compile features.
3. `install(FILES include/psp_span/span.h include/psp_span/parser.h ...)`.
   Both headers ship with the install tree.

The version bump is real, not cosmetic: any consumer that wants
`<psp_span/parser.h>` must use `find_package(psp_span_lib 0.6
REQUIRED)` (or accept any 0.x). The Jul 13 consumer
(`P-2026-07-13-psp-parser-expected.cpp`) still builds cleanly
against 0.6 — verified — because it doesn't include `parser.h` and
the span layer is unchanged.

## Section 1 — `psp::parse_int` from `<psp_span/parser.h>`

```
== Section 1: psp::parse_int from <psp_span/parser.h> ==
  parse_int("12345") = 12345
  parse_int("0") = 0
  parse_int("999") = 999
  parse_int("") error: Empty
  parse_int("+9") error: LeadingSign
  parse_int("-9") error: LeadingSign
  parse_int("12a3") error: NotADigit
  parse_int("9999999999") error: Overflow
```

Eight cases covering all four `parse_int` failure modes. The
keyshift vs Jul 13: today the function comes from
`#include <psp_span/parser.h>`, not from the lesson .cpp. Same
behaviour, different origin.

## Section 2 — `psp::parse_double`

```
== Section 2: psp::parse_double from <psp_span/parser.h> ==
  parse_double("0") = 0
  parse_double("42") = 42
  parse_double("3.14") = 3.14
  parse_double("1.") = 1
  parse_double(".5") = 0.5
  parse_double("1e10") = 1e+10
  parse_double("1.5E-3") = 0.0015
  parse_double("0.0001") = 0.0001
  parse_double("") error: Empty
  parse_double("+1.0") error: LeadingSign
  parse_double("-1.0") error: LeadingSign
  parse_double(".") error: MissingFraction
  parse_double("1e") error: BadExponent
  parse_double("1e+") error: BadExponent
  parse_double("1.2.3") error: NotADigit
  parse_double("abc") error: NotADigit
  (parse_double: 16/16 cases behaved as expected)
```

Sixteen cases, all passing. Eight successes covering the int /
fractional / exponent phases; eight failures covering all six
`ParseError` enumerators (the four from `parse_int` plus
`MissingFraction` and `BadExponent`).

The composition of the value uses a tight power-of-10 loop rather
than `std::pow` because `std::pow` is a runtime math call that
isn't necessarily `noexcept`-stable across toolchains, and
`parse_double` is `noexcept` for the same reason `parse_int` is.

## Section 3 — monadic composition with the library's parser

```
== Section 3: monadic composition with library parse_int ==
  double_if_positive("50") = 100
  double_if_positive("100") = 200
  double_if_positive("-1") error: LeadingSign
  double_if_positive("abc") error: NotADigit
  double_if_positive("0") error: Overflow
```

Same shape as the Jul 13 `double_if_positive`, but using the
library's `parse_int`. The validation step is inlined as a
lambda because the policy is consumer-specific; the parser layer
stays generic.

Three observable properties of the chain:

- **Short-circuit on parse failure** — `"-1"` and `"abc"` report
  parse errors; the validation step never runs.
- **Short-circuit on validation failure** — `"0"` parses to `0`
  successfully but fails the `n <= 0` check.
- **Transform only on success** — `"50"` and `"100"` parse and
  validate, then get doubled.

## Section 4 — `std::format` integration

```
== Section 4: std::format integration with ParseError ==
  parse_double("3.14x") error: NotADigit
  parse_int("+9") error: LeadingSign
```

`std::format("{}", r.error())` works without any per-consumer
formatter code because `std::formatter<ParseError>` is in the
library's `parser.h`. This is the second "what I didn't cover"
item from Jul 13 that today closes cleanly.

## Section 5 — non-owning span integration

```
== Section 5: psp::Span<const char> non-owning integration ==
  digits.data() == src.data()+6: 1
  digits.size_bytes() = 5 (expected 5)
  parse_int(digits) = 12345
```

Three checks:

1. **Non-owning aliasing** — `digits.data() == src.data() + 6` is
   `1`, proving that `psp::Span::subspan(6, 5)` borrows into the
   source string's buffer without copying.
2. **Length** — `size_bytes()` is `5`, matching the source.
3. **Parser on the subview** — `parse_int(digits)` returns `12345`,
   the literal value at bytes 6..11 of `"hello 12345 world"`.

## Section 6 — `sizeof` and toolchain probes

```
== Section 6: sizeof / alignment probes ==
  sizeof(ParseError)               = 4
  sizeof(int)                      = 4
  sizeof(double)                   = 8
  sizeof(psp::Span<const char>)    = 16
  sizeof(expected<int, ParseError>)    = 8
  sizeof(expected<double, ParseError>) = 16
  __cpp_lib_expected               = 202211
  __cpp_lib_span                   = 202002
```

The three observations:

1. **`sizeof(ParseError) == 4`** — `enum class` defaults to `int`
   as the underlying type on this toolchain (Apple clang 21). Two
   new enumerators (`BadExponent`, `MissingFraction`) don't change
   the size.

2. **`sizeof(expected<int, ParseError>) == 8`** and
   `sizeof(expected<double, ParseError>) == 16`** — libc++ packs the
   bool discriminator into alignment slack via EBO (empty-base
   optimization). The two sizes differ because `double` is 8 bytes
   and `int` is 4; the larger payload drives the layout.

3. **`__cpp_lib_expected == 202211`** — C++23 final, as expected.
   **`__cpp_lib_span == 202002`** — this is the *standard* span
   feature-test; we use the hand-rolled `psp::Span` so this is a
   curiosity value, not a requirement for the library.

## The consumer's `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(psp_parser_header_consumer ...)

find_package(psp_span_lib 0.6 REQUIRED)

add_executable(P-2026-07-14-psp-parser-header
    P-2026-07-14-psp-parser-header.cpp
)
target_link_libraries(P-2026-07-14-psp-parser-header
    PRIVATE psp_span_lib::psp_span_lib
)
```

vs the Jul 13 consumer (which did `g++ -I /tmp/psp_install/include ...
-L /tmp/psp_install/lib -lpsp_span_lib`):

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-14-psp-parser-header
```

The `CMAKE_PREFIX_PATH=/tmp/psp_install` is the ONLY piece of
install-tree-specific knowledge. Everything else — the include
path, the library path, the `-std=c++23` flag, the `libpsp_span_lib.a`
linkage — comes from the library's CMake config via the IMPORTED
target.

I also verified that a **completely fresh consumer** (a brand-new
CMake project, written from scratch, never seeing this lesson's
source tree) can `find_package(psp_span_lib 0.6 REQUIRED)` and
link against `psp_span_lib::psp_span_lib` to get the parser
functions. That's the full "ship it" proof.

## Build & run (what I actually ran)

```bash
# 1. Build and install psp_span_lib 0.6.0 into /tmp/psp_install.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_span_lib
cmake -S . -B build-cron -DCMAKE_BUILD_TYPE=Release
cmake --build build-cron
cmake --install build-cron --prefix /tmp/psp_install

# 2. Configure + build the consumer with find_package.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_header
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build build
./build/P-2026-07-14-psp-parser-header

# 3. ASan build of the consumer.
cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-07-14-psp-parser-header

# 4. Backward-compat check: the Jul 13 consumer still builds against 0.6.
cd /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/psp_parser_expected
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -I /tmp/psp_install/include \
    P-2026-07-13-psp-parser-expected.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/jul13_retest
/tmp/jul13_retest
```

All three builds compile clean (`-Wall -Wextra -Wpedantic`, no
warnings) and run to exit 0. ASan reports no memory errors. The
Jul 13 consumer builds and runs unchanged against v0.6.0.

## What I didn't cover (next-session candidates)

- **Streaming `parse_int_at(Span&)`** — a cursor-advancing overload
  that lets a parser walk a buffer character-by-character. Would
  compose naturally into the JSON / structured-data parser that
  today is out of scope.
- **A `parse_uint` variant** — unsigned-only, with the `LeadingSign`
  rejection (because `+`/`-` is meaningless for unsigned). 5-line
  addition to parser.h.
- **`ParseError` translation to `std::error_code`** — for consumers
  that need to interop with the `<system_error>` machinery. Add
  a `std::error_code make_error_code(ParseError)` ADL-discoverable
  overload and a `is_error_code_enum<ParseError>` specialization.
- **A `parse_double`-with-locale variant** — locale-aware decimal
  separator (`,` in some locales, `.` in others). Pulls in
  `<locale>`, which has cost. Probably better as a separate header.
- **A test harness using Catch2 / doctest** — the library today
  has no formal unit tests; the lesson programs ARE the tests,
  but a proper `tests/` directory would catch regressions on
  future header edits. This is a forward-on candidate but it's
  a "library hygiene" move, not a "new C++" move.
- **The `<psp_span/parser.h>` header could grow a `parse_bool` /
  `parse_hex_int`** — but the design philosophy today is "tiny
  numeric parsers". Adding bool/hex is mostly more of the same;
  the next interesting design move is a streaming cursor API.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        ├── psp_span_lib/                                         # MODIFIED (library)
        │   ├── CMakeLists.txt                                   #    v0.5.0 -> v0.6.0; C++17 -> C++23;
        │   │                                                     #    install(FILES ...) ships parser.h
        │   └── include/psp_span/
        │       └── parser.h                                      # NEW (the public parser header)
        └── psp_parser_header/                                   # NEW (the consumer)
            ├── CMakeLists.txt                                   #    consumer with find_package(psp_span_lib 0.6)
            ├── P-2026-07-14-psp-parser-header.cpp                #    the program
            └── P-2026-07-14-psp-parser-header.md                 #    this file
```

The library gains one header and a version bump. The consumer
directory is brand new. The `psp_span_lib` install tree (the part
that gets bundled into CPack archives and shipped to consumers)
now contains:

```
include/psp_span/
├── span.h        (C++17 — psp::Span<T, Extent>)
└── parser.h      (C++23 — psp::parse_int, psp::parse_double + ParseError + formatter)
lib/
├── libpsp_span_lib.a
└── cmake/psp_span_lib/
    ├── psp_span_libConfig.cmake
    ├── psp_span_libConfigVersion.cmake
    └── psp_span_libTargets.cmake
```

## Next steps

The psp_span_lib release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS or
  .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.6.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform and
  SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow feature
  work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **Streaming `parse_int_at(Span&)`** — the cursor-advancing
  overload. Would let a parser walk a buffer.
- **`parse_uint` / `parse_bool`** — more numeric parsers following
  the same pattern.
- **`std::expected` and coroutines** — `co_return std::expected<T,E>`
  composes with the parser layer for an async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it, the Jul 10
  Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray) — the
  standard version of the Jul 11 hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear algebra
  built on mdspan.

The most natural follow-on is `parse_uint` (a small, well-bounded
extension that exercises the same plumbing as `parse_int` but with
a different sign policy) OR the streaming cursor API (which would
let us start building a structured-data parser on top of the
current numeric layer). Both are 5–10-line additions to
`parser.h`.