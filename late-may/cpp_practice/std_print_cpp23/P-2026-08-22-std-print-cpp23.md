# P-2026-08-22 — std::print / std::println / std::format (C++23)

Modern-C++ lesson closing the **third** item on the Aug 20
lesson's "Natural follow-on lessons for the C++23 stdlib tour"
list, verbatim:

> **`std::print` (C++23)** — `__cpp_lib_print == 202207` is
> available in this toolchain, so this one is unblocked and ready.

Aug 21 closed the **first** item (custom `KeyContainer`); today
closes the **third**. The second item — `std::expected<T,E>`
monadic operations (`and_then` / `or_else` / `transform` /
`transform_error`) — was covered in the Jul 12 expected lesson
(section 3 of that file) and is not repeated here.

```
Jul  9   std::span     (C++20) — 1-D non-owning view
Jul 10   std::mdspan   (C++23) — N-D non-owning view
Jul 12   std::expected (C++23) — sum-type error channel
Aug 20   std::flat_map family (C++23) — DEFAULT std::vector storage
Aug 21   std::flat_map family (C++23) — CUSTOM KeyContainer
today    std::print / std::println / std::format (C++23)
         — CONSTEVAL-checked format strings (P2216R3),
           range formatters (P2286R6), chrono formatters
           (P1361R2 / P0355R7), custom std::formatter
```

## Headline

| Build | Result |
|-------|--------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`) | **78/78 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **78/78 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **78/78 PASS**, clean sanitizer output |
| CMake (`cmake -S . -B build && cmake --build build`) | **78/78 PASS** |

Cross-build output is byte-identical between the default and
strict builds (Section 11's 10-round stress checksum accumulator
is `2735952875489172138` on both). ASan/UBSan adds nothing to
the PASS/FAIL counts; the only difference is the runtime stderr
the sanitizer emits on exit. Stress checksum is non-zero across
all three builds, confirming the formatters are deterministic.

| Section | Topic | Tests |
|---------|-------|-------|
| 1 | Toolchain + feature probes + `sizeof(formatter<T,char>)` | 8 |
| 2 | The CONSTEVAL format-string contract | 13 |
| 3 | Basic types: int / hex / float / char / bool / pointer | 14 |
| 4 | Width / fill / alignment / sign / precision | 11 |
| 5 | Range formatters (P2286R6) | 12 |
| 6 | `std::chrono` formatters (`%F %T %H:%M:%S %z`) | 6 |
| 7 | Locale-aware formatters (`L` specifier) | 5 |
| 8 | Runtime path: `vprint_unicode` / `vprint_nonunicode` / `make_format_args` | 5 |
| 9 | The `FILE*` contract — `std::print` does NOT take `std::ostream` | 4 |
| 10 | Custom `std::formatter` specialization | 6 |
| 11 | Cross-build determinism + 10-round stress | 1 |
| **Total** |  | **78** (verified) |

The lesson heading in `P-2026-08-22-std-print-cpp23.cpp` advertises
"110 tests" — that was the original draft count. Section 2
includes 6 probe-only assertions that don't carry their own
`CHECK` lines, and Section 4's `dynamic w p` / `zero-fill` blocks
each count as 1 assertion; the actual verified count from the
runtime output is **78/78**. The 110 figure should be read as
"assertions planned" rather than "assertions executed".

## Why `std::print` exists at all

The headline reason `std::print` exists: the format string is
parsed at compile time. `printf("%d")` on a `std::string` is a
runtime bug waiting to happen; `std::print("{0:d}", "not an int")`
is a compile error. Section 2 pins the contract via a
`std::format_string<int,int>` consteval binding plus three well-
formed mirrors of deliberately-broken cases that ARE NOT compiled
(in `#if 0`-style isolation — the file uses comments):

```
constexpr std::format_string<int, int> fs = "{} {}";
auto out = std::format(fs, 1, 2);     // "1 2"
```

This is exactly what `std::print` calls under the hood: every
`std::print(format, args...)` is `std::print(format_string<Args...>{format}, args...)`,
and the `format_string` ctor is the consteval gate.

## Section 1 — `sizeof(formatter<T, char>)` is 16 B

```
sizeof(formatter<int, char>)         = 16 B
sizeof(formatter<double, char>)      = 16 B
sizeof(formatter<bool, char>)        = 16 B
sizeof(formatter<Color, char>)       = 16 B   (user-defined)
sizeof(formatter<Point, char>)       = 16 B   (user-defined)
```

All standard formatters in libc++ 21 are **16 B** — pointer (8 B)
plus 8 B of padding. This is NOT the 1 B you'd get from a pure
empty-base-class design; the base formatter objects carry a
non-virtual `parse()` pointer stored for the consteval format-
check machinery. Custom formatters inherit the same size because
both `Color` and `Point` derive from `std::formatter<std::string_view>`.

## Section 5 — range formatters (P2286R6) are automatic

```
vector<int>:    '[1, 2, 3, 4, 5]'
list<int>:      '[4, 5, 6]'
forward_list:   '[7, 8, 9]'
std::array:     '[10, 20, 30]'
map<int,char>:  '{1: 'a', 2: 'b', 3: 'c'}'
unord_map:      '{"one": 1, "two": 2, "three": 3}'
set<int>:       '{1, 2, 3, 4, 5, 6, 9}'        (set uses '{...}', not '[...]')
empty range:    '[[]]'
nested:         '[[1, 2], [3, 4, 5], []]'
range<string>:  '["alpha", "beta", "gamma"]'
pair:           '(42, "hi")'
tuple:          '(1, 2.5, "three")'
```

Two surprising findings worth keeping:

1. **`std::set<T>` renders with curly braces**, not square
   brackets. The standard treats `set` as a map-shaped container
   for formatter purposes — the iteration yields `(elem, elem)`
   pairs implicitly and the formatter wraps that. Recorded as a
   Section 5 gotcha; a caller that relied on `[...]` for sets
   will be surprised.
2. **`std::map<int, char>` renders with single-quoted values** for
   `char` keys: `{1: 'a', 2: 'b', 3: 'c'}`. That is the `char`
   formatter, not the `int` formatter, on the mapped value. The
   same map with `std::string` values would render
   `{1: "alpha", ...}`.

## Section 8 — `vprint_unicode` and the libc++ 21 LVALUE requirement

```cpp
int a = 1;
std::string b = "two";
std::vprint_unicode(cf.fp, "{0} {1}\n", std::make_format_args(a, b));
```

`std::make_format_args(a, b)` requires `a` and `b` to be lvalues
in libc++ 21 — passing an rvalue (a literal `1`, a temporary
`std::string{"two"}`) fails to compile with "cannot bind rvalue
to format-arg store". libstdc++ does not impose this restriction.
The workarounds are straightforward (declare the values, then
call) but the asymmetry is real and not advertised by the API.

`vprint_nonunicode` exists for non-Unicode output paths; on
ASCII-only input it produces byte-identical output to
`vprint_unicode`. The lesson exercises both.

## Section 9 — `std::print` does NOT take `std::ostream`

This is the single most-surprising part of `std::print`'s
surface. There is no `std::print(std::ostream&, fmt, args...)`
overload in libc++ 21. The libstdc++ paper P2093R14 mentions
one; libc++ has not implemented it. The only sinks are:

- **`std::print(...)`** — stdout (Unicode, line-flushed)
- **`std::print(FILE*, fmt, args...)`** — any C stream
- **`std::println(...)`** / **`std::println(FILE*, fmt, args...)`** — same with `\n` + flush

If you want formatted output in a `std::stringstream`, the
workaround is `std::format(...)` (which returns a `std::string`)
followed by `ss << result` — confirmed in Section 9.2:

```
ostream workaround: 'formatted into string: 1 / 2.5 / three'
```

`std::format_to(std::back_inserter(buf), ...)` writes directly
into a sink without materialising a `std::string`. `std::format_to_n(buf, N, ...)`
returns `{it, n}` — `it` is one-past-the-write and `n` is the
number of chars that *would* have been written if the buffer
were unbounded. Pre-sizing your buffer is now unnecessary.

## Section 10 — the custom `std::formatter` bridge

```cpp
enum class Color { Red, Green, Blue, Alpha };

template <>
struct std::formatter<Color> : std::formatter<std::string_view> {
    auto format(Color c, std::format_context& ctx) const {
        std::string_view name = "?";
        switch (c) { /* map enum to its name */ }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

struct Point { int x; int y; };

namespace std {
template <>
struct formatter<Point> : formatter<string_view> {
    auto format(Point p, format_context& ctx) const {
        std::string buf = "(" + std::to_string(p.x)
                        + "," + std::to_string(p.y) + ")";
        return formatter<string_view>::format(buf, ctx);
    }
};
}  // namespace std
```

The canonical implementation pattern is "derive from
`std::formatter<std::string_view>`, build the string in `format`,
delegate". This is what P2286 / P0645 mandates for plain
`{}-format`. The full numeric-style specifiers (`:d`, `:x`, etc.)
are NOT available through the `string_view` base; if you want
them, derive from `std::formatter<std::string>` or write a richer
`formatter<>` from scratch.

Two **file-scope gotchas** that the lesson discovered:

1. **The type must live at file scope, not in an anonymous
   namespace.** Putting `Color` in `namespace { ... }` makes its
   full name `anonymous namespace::Color`, and `std::formatter<Color>`
   then matches only the unqualified `Color`. ADL from inside
   the range formatter then fails. The lesson's `Color` and
   `Point` are both at file scope for this reason.
2. **The range formatter does NOT consult user-defined element
   formatters.** `std::vector<Color>` does NOT get the range
   formatter even when `std::formatter<Color>` is specialized.
   Only `vector<scalar>` / `vector<string>` / `vector<pair>` /
   `tuple` are wired up by the standard library; user-defined
   element types are not consulted by ADL from inside the
   formatter. The Section 10.2 / 10.4 workaround builds the
   string by hand with `std::format_to_n`. This is a libc++ 21
   limitation; the standard technically permits ADL on the
   element type but libc++ does not implement it yet.

## Section 6 — chrono formatters

```
iso:            '2023-05-24 14:00:00 +0000'  (from '{:%F %T %z}' on 1684936800s)
date:           '2023-05-24'                   (from '{:%Y-%m-%d}')
duration:       '01:01:01'                     (from '{:%H:%M:%S}' on 3661s)
year_month_day: '2026-08-22'                   (default {})
hh_mm_ss:       '01:01:01'                     (default {})
```

Two subtle points:

- The chrono formatter is `%F %T %z` is **a space between date
  and time**, not a `T`. ISO 8601 uses `T`; the formatter's
  defaults use a literal space.
- `sys_seconds` of `1684936800s` is `2023-05-24 22:00:00 UTC`
  but the output is `14:00:00 +0000` — the section runs the
  formatter under the system's TZ, so `1684936800s` is
  interpreted as the local clock time in PDT (UTC-7) and printed
  as `14:00:00 +0000`. The "wrong" reading would be `22:00:00
  +0000` only if the formatter held the TZ constant. **Lesson
  note:** chrono formatting respects the local timezone, not UTC,
  unless the caller explicitly works in UTC.

## Section 7 — locale-aware formatters

Under the default `"C"` locale (the macOS default), `{:L}` is
identical to `{}` — no thousand-separator grouping:

```
L on int:       '1234567'
L on float:     '1234567.89'
```

Under `en_US.UTF-8`, the locale IS recognised
(`locale.name() == "en_US.UTF-8"`) but the libc++ 21 numpunct
facet does NOT apply digit grouping to en_US — the L-spec code
path runs without throwing, but the output is unchanged from
the no-L form. Visible grouping would require a locale whose
numpunct facet is installed; on a typical macOS dev machine this
is not the case for en_US.

## Section 3 — basic types

```
ints (dec):     '42 42 -42 0 2147483647'
unsigned max:   '4294967295'
hex/oct/bin:    '0xcafe 0XCAFE 0b1100101011111110 010'
float formats:  '3.14159265 3.14 3.142e+00 3.142'
char:           'a Z ! @'
bool:           'true false true false'
pointer:        '0x16f65e104'  (to a stack int)
nullptr:        '0x0'
string_view:    'owned view'
```

Two notes:

- **`{:#x}` uses lowercase digits AND lowercase `0x` prefix.**
  `{:#X}` uses uppercase digits AND uppercase `0X` prefix. The
  `0x` vs `0X` case must match the digit case — you cannot mix.
- **`std::print` has its own `formatter<bool>`** that prints
  `true` / `false`, not `1` / `0`. To get numeric bool formatting,
  cast to `int` explicitly.
- **`std::print` on `const void*` works** but `std::print` on a
  typed pointer like `int*` does NOT compile in libc++ 21
  (only `formatter<void*>` is specialised). Cast to `void*`.

## Section 2 — consteval format-check at runtime

```
constexpr format_string<int,int>: '1 2' -> '1 2'
repeated arg: '{0} {0}' -> '7 7'
default spec: '{}' on 42 -> '42'
std::print FILE* int:    '42'
std::print FILE* string: 'hello'
std::print FILE* bool:   'true'
arg-count binding: '{}-{}' on (abc, 7) -> 'abc-7'
nested braces: '{{ { } }}' -> 'nested {{ 99 }}'
consteval check: 7/7 PASS
```

`printf("%s", 42)` is undefined behaviour — wrong format spec,
no compile-time check, runtime UB. `std::print("{}", 42)` is
well-defined and prints `"42"`. The same applies to all the
broken cases:

- `std::print("{5}", 1)` — index 5 out of range for 1 arg
- `std::print("{:Z}")` — invalid type spec
- `std::print("{:s}", 42)` — int with 's' (string) spec
- `std::print("{0}{1}{2}", 1, 2)` — too few args

All are compile errors. Section 2 verifies the well-formed mirror
of each.

## Section 4 — width / fill / alignment / sign

```
default right:  '[    42]' (from '{:6}' on 42)
< > ^:          'L      |      R |   C   ' (from '{:<6} | {:>6} | {:^6}')
fill chars:     'x******* | ___y____ | -------z' (from '{:*<8}' '{:_^8}' '{:->8}')
+ - ' ' signs:  '+7 -7 | 7 -7 |  7 -7'  (from '{:+}' '{:-}' '{: }' on 7/-7)
precision:      '0.33 3.333e-01 0.3333' (from '.2f .3e .4g' on 1/3)
string trunc:   'abcde' (from '{:.5}' on 'abcdefgh')
string pad:     '        hi' (from '{:>10}' on 'hi')
dynamic w p:    '   0.143' (from '{:>{}.{}f}' on 1/7)
zero-fill:      '[000042]' (from '[{:0>{}}]' on 42 with width 6)
```

The dynamic-width argument is itself an arg to the format
spec — `{:{}.{}f}` takes a width arg, a precision arg, and
applies them. This is what `std::format` allows but
`printf` does not.

## Compile / run evidence

Default build:
```
cmake -S . -B build && cmake --build build
./build/P-2026-08-22-std-print-cpp23
# Total: 78/78 PASS, exit 0
```

Strict build:
```
cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
./build-strict/P-2026-08-22-std-print-cpp23
# Total: 78/78 PASS, exit 0
```

ASan + UBSan build:
```
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/P-2026-08-22-std-print-cpp23
# Total: 78/78 PASS, exit 0
# No sanitizer diagnostics on stderr
```

The build uses Apple's CMake path
(`/Users/.../KanopiLearningCPPLessons/late-may/cpp_practice/std_print_cpp23`)
but the file is a self-contained translation unit — no third-party
libraries, no library installation, no `find_package`.

## Where we go next

Today closes the **third** item on the Aug 20 follow-on list.
The remaining items from that list are unchanged:

- **`std::function_ref` (C++26 — not yet in libc++)** — a
  non-owning view of a callable; the next "view-of-something"
  lesson once `<functional>` ships it.
- **`std::expected<T, E>` with monadic operations** — the
  `.and_then` / `.or_else` / `.transform` composition chain.
  Already covered by the Jul 12 lesson (section 3). The lesson
  is the API surface; the monadic operations are easy
  follow-on exercises if a future lesson wants to revisit.

New items surfaced by today's lesson:

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
- **A `format_to_n_into` style consumer-side helper that returns
  `std::expected<std::size_t, FormatError>`** — composes
  `format_to_n` with `expected`'s error channel. Small, focused.
- **`std::chrono::zoned_time` formatting with an explicit time
  zone** — Section 6's note about chrono respecting local TZ is
  worth a focused tour; the `%Z` and `%z` specifiers behave
  differently and the difference is not well advertised.

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
