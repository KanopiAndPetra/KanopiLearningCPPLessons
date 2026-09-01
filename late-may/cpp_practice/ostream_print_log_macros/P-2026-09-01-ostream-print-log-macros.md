# P-2026-09-01 — `ostream_print_log_macros`: `std::print`-compatible LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR macros

**Topic:** the consteval-checked logging sugar layer over the Aug
28 `std::print(std::ostream&, ...)` adapter, with a runtime-format-string
sibling and a configurable sink pointer (default `std::cerr`).

**Standard:** C++23 (`std::format` P0645R10, `std::format_string<Args...>`,
`std::vformat`, `std::make_format_args`, `std::format_error`).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++. CMake 4.3.4.

**Result:** 41/41 PASS across 15 sections on six builds — direct
default, direct strict-warning, direct ASan/UBSan, CMake default,
CMake strict, CMake ASan. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`.
Zero sanitizer diagnostics. Five consecutive runs on each build
produced identical tallies.

---

## Why today

The Aug 28 lesson (`P-2026-08-28-ostream-print-cpp23`) shipped
`petra::ostream_print` / `ostream_println` — the consumer-side
`std::print(std::ostream&, ...)` adapter that closes the
overload gap libc++ 21 leaves open. The Aug 31 lesson
(`P-2026-08-31-ostream-write-n-via-buf`) extended it with the
`n`-capped sibling `std::expected<size_t, FormatError>` entry.
Aug 28's "Where we go next" section listed four new follow-on
items. The **fourth** was:

> **`std::print`-compatible log macros** — `LOG_INFO("value={}", 42)`
> sugar over the adapter.

Today IS that lesson. It does three things:

1. **Ships the macros** — `LOG_DEBUG` / `LOG_INFO` / `LOG_WARN` /
   `LOG_ERROR` (the consteval path) and a separate
   `LOG_DEBUG_RUNTIME` / `LOG_INFO_RUNTIME` / `LOG_WARN_RUNTIME` /
   `LOG_ERROR_RUNTIME` quartet (the runtime path), both routed
   through the Aug 28 `petra::ostream_println` adapter with a
   configurable `petra::log_sink()&` pointer (default
   `std::cerr`, swappable to a heap / test `std::ostringstream`).
2. **Pins down the macro-hygiene contract** — `do { ... } while (0)`
   wrapping so `if (x) LOG_INFO(...); else ...;` binds correctly,
   comma-in-arguments (an `std::pair<int,int>` round-trips whole),
   `std::string` with a comma inside it, format-specifier
   passthrough (`{:#x}`, `{:.3f}`, `{:>8}`, `{:<6}`), NUL byte in
   payload, UTF-8 in payload.
3. **Pins down the consteval gate at the macro layer** — the
   macros route through `std::format_string<Args...>` so a
   malformed format string is a HARD COMPILE ERROR at the macro
   call site, not a runtime `std::format_error` throw. The
   runtime sibling accepts `std::string_view` + `std::format_args`
   for callers who need runtime format strings.

---

## The headline finding

The macro layer does not change the consteval-vs-runtime split
Aug 28 / Aug 30 / Aug 31 established — it just routes it through
two different macro surfaces:

| Macro | Format-string type | Errors caught at |
|-------|-------------------|------------------|
| `LOG_INFO(...)` etc. | `std::format_string<Args...>` (consteval ctor) | compile time |
| `LOG_INFO_RUNTIME(fmt, args)` | `std::string_view` + `std::format_args` | format time (throws) |

The reason for two surfaces is the same reason Aug 28 / Aug 30
shipped two function surfaces: `std::format_string<Args...>`
has a `consteval` constructor that catches parse /
argument-index / `{:d}`-on-string / unmatched-brace errors at
compile time, but it ONLY accepts literal format strings.
Runtime-supplied format strings (`std::string` from a config
file, a user template, a JSON field) must go through the
runtime sibling, where parse errors throw
`std::format_error` at format time.

Section 7 pins this with a `static_assert` probe on
`std::format_string<int>{"value={}"}`. The consteval ctor
verifies the format string at translation time; runtime
format strings cannot use that surface because their value is
not a constant expression.

---

## The macros and their two-phase emission

The macros expand to a `do { ... } while (0)` block around a
call to a helper template. The helper does two writes:

1. **Prefix** — `"[INFO ] "` (or `"[DEBUG] "`, `"[WARN ] "`,
   `"[ERROR] "`) via `petra::log_emit_prefixed(lvl)`. Five
   chars of tag, padded with one trailing space on the 4-char
   levels so log lines align in columns.
2. **Payload** — `petra::ostream_println(sink, fmt, args...)`
   from Aug 28, which appends `'\n'`.

The two writes are NOT atomic with respect to each other on
the default `std::cerr` sink. A concurrent log call from
another thread can interleave between prefix and payload —
the same shape of finding the Aug 25 / Aug 26 / Aug 28 chain
hit with their iterator-side writes. A production logger
that needs thread-safe emission should compose
`std::osyncstream` around the sink pointer; today's lesson
documents the trap and names `std::osyncstream` as the
natural next step but does not implement it.

---

## The sink pointer

`petra::log_sink()` returns `std::ostream*&`, a reference to
a function-static pointer initialised to `&std::cerr`. A
caller swaps it for tests (sections 2, 4, 5, 6, 8, 9, 11,
12, 13, 14, 15 all use a stack `std::ostringstream`); setting
it to `nullptr` silences every macro (section 9).

The pointer is a raw pointer, not a `std::ostream&`, because
`std::cerr`'s lifetime is the program's, but a heap or stack
stream's lifetime is the caller's responsibility. The lesson
does NOT own the lock for thread-safe writes — `std::cerr`
writes from multiple threads are an unsynchronised data race
unless wrapped in `std::osyncstream`.

---

## Macro-hygiene contract

The `do { ... } while (0)` wrapping is the standard "trailing
`;` is harmless" idiom. Section 5 pins it explicitly:

```cpp
if (cond)
    LOG_INFO("branch-A {}", 1);   // macro expands to do { ... } while (0);
else
    LOG_WARN("branch-B {}", 2);   // binds to else correctly
```

Without the do-while wrapping, `LOG_INFO("x")` would expand
to `petra::log_emit(...);` and the dangling `;` would attach
to the `if` branch, breaking `if/else` binding. Section 5
exercises both branches to pin the contract.

Section 4 pins comma-in-arguments:

```cpp
std::pair<int, int> p{3, 4};
LOG_INFO("pair=({},{})", p.first, p.second);   // std::pair has a comma in its type
```

The macro passes `__VA_ARGS__` as a single token group to
`PETRA_LOG_EMIT(level, __VA_ARGS__)`, which expands to
`petra::log_emit(level, <whole expr>)`. The comma inside the
expression does NOT split the macro arguments.

---

## Format-specifier passthrough

The macros do not interpret the format string at all — they
forward `__VA_ARGS__` to `petra::ostream_println`, which
forwards to `std::vformat`. Every C++23 format specifier
arrives unchanged at the stdlib. Section 6 pins
`{:#x}`, `{:.3f}`, `{:>8}`, `{:<6}` round-trip byte-equal.

---

## The runtime-format-string sibling

`LOG_INFO_RUNTIME(fmt, args)` takes two args after the level
— `std::string_view fmt` and `std::format_args args`. The
caller writes `std::make_format_args(...)` explicitly because
the consteval surface does not accept runtime values. Parse
errors throw `std::format_error` at format time.

Section 8 pins this end-to-end with both a `std::string_view`
literal and a `std::string` constructed at runtime. The
runtime path is reachable only through the `_RUNTIME` macros
because the consteval gate on `std::format_string<Args...>`
refuses runtime values.

---

## Sink-pointer reset silences output

Section 9 sets `petra::log_sink() = nullptr` and verifies
that subsequent `LOG_INFO` / `LOG_ERROR` invocations produce
no output and no crash. The check is straightforward: the
helper template returns early on `log_sink() == nullptr`
before either the prefix or the payload write.

---

## NUL byte and UTF-8 in payload

The macros do not alter the stdlib's `std::format` behaviour
for embedded NUL or UTF-8 — they just preprend a tag. Section
12 pins that an NUL byte in the payload round-trips as a NUL
byte, exactly as `oss.str()` preserves it. Section 13 pins
that a 4-byte UTF-8 emoji in the payload round-trips as
4 bytes, exactly as Aug 28 pinned for `ostream_println` /
`ostream_print`.

The prefix itself is ASCII (`"[INFO ] "` etc.) so terminal
column alignment in a log file is preserved even when the
payload contains multi-byte UTF-8.

---

## Other things the sections pin

- **Section 1** — `__cpp_lib_format == 202110` (the C++23
  `std::format` surface); `__cpp_lib_print == 202207` (the
  C++23 `std::print` surface that the macros piggy-back on
  via the Aug 28 adapter).
- **Section 2** — `LOG_INFO("hello {}", "petra")` produces
  `"[INFO ] hello petra\n"`; same shape for `LOG_WARN`,
  `LOG_ERROR`, `LOG_DEBUG`.
- **Section 3** — `LOG_INFO("ping")` (zero args) works; the
  consteval gate accepts a payload with no `{}` replacements
  as `std::format_string<>`.
- **Section 10** — every level tag is exactly 5 chars wide;
  the 4-char levels (`"INFO "`, `"WARN "`) are right-padded
  with one space so log columns align.
- **Section 11** — 5 consecutive `LOG_INFO` invocations
  produce 5 lines, each ending in `'\n'` with no spurious
  trailing data.
- **Section 14** — round-robin all four levels in one
  sequence and verify the expected byte-equality of the
  full output.
- **Section 15** — 50-round determinism stress alternating
  levels (each round uses the level `round % 4`), asserting
  byte equality with the predicted string built alongside.

---

## What the lesson does NOT do

- **Level filtering.** Every macro always emits. A future
  expansion could `#if PETRA_LOG_LEVEL <= N` gate each macro
  to a no-op; today treats every level as always-emitted.
- **File / line capture.** No `__FILE__` / `__LINE__` in the
  output. A production logger would prepend
  `std::format("[{}:{}] ", __FILE__, __LINE__)`; today keeps
  the surface minimal.
- **Thread safety.** `std::cerr` writes from multiple threads
  without external sync are an unsynchronised data race.
  Composing `std::osyncstream` around the sink pointer is
  the natural fix; today documents the trap but does not
  own the lock.
- **Async sinks.** No background worker, no queue. The
  macros emit synchronously through `petra::ostream_println`
  which is `std::vformat -> std::string -> os.write`.

---

## Observed output

```
-- section 1.sizeof+probes
-- section 2.basic-success
-- section 3.zero-args
-- section 4.comma-in-args
-- section 5.if-else-hygiene
-- section 6.format-specifiers
-- section 7.consteval-gate
-- section 8.runtime-sibling
-- section 9.sink-null-silences
-- section 10.tag-width
-- section 11.many-invocations
-- section 12.nul-in-payload
-- section 13.utf8-payload
-- section 14.all-levels-round-trip
-- section 15.determinism-stress

41/41 PASS
```

Identical on all six builds; five consecutive runs on each
build produced identical tallies.

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ \
    P-2026-09-01-ostream-print-log-macros.cpp \
    -o P-2026-09-01-ostream-print-log-macros \
    && ./P-2026-09-01-ostream-print-log-macros

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ \
        P-2026-09-01-ostream-print-log-macros.cpp \
        -o P-2026-09-01-ostream-print-log-macros-strict \
    && ./P-2026-09-01-ostream-print-log-macros-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        P-2026-09-01-ostream-print-log-macros.cpp \
        -o P-2026-09-01-ostream-print-log-macros-asan \
    && ASAN_OPTIONS=detect_leaks=0 ./P-2026-09-01-ostream-print-log-macros-asan

# CMake: default / strict / ASan
cmake -S . -B build                && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
```

### A note on LeakSanitizer

As on Aug 30 / Aug 31, LeakSanitizer is **not supported on
darwin/arm64** (probed: `"detect_leaks is not supported on
this platform"`). `ASAN_OPTIONS=detect_leaks=0` was used for
the ASan run rather than the documented `detect_leaks=1`,
which aborts rather than enabling it on this platform. Every
format argument in the lesson is a named local so the absence
of LeakSanitizer does not silently hide leaks.

---

## Where we go next

Today closes the **fourth of the four** new follow-on items
from Aug 28's "Where we go next". The remaining three from
that list (already closed by Aug 30 and Aug 31) leave the
arc in a stable state for the chain. Today adds three new items:

- **`std::osyncstream` around the sink pointer.** The macros'
  two-phase write is a gap for concurrent invocations;
  `petra::log_sink() = new std::osyncstream{std::cerr}` (or
  similar) closes the gap. Today documents the trap; a
  future lesson could pin the integration end-to-end.
- **`__FILE__` / `__LINE__` capture.** A production logger
  prepends `__FILE__` / `__LINE__` to each line. The macros
  can be retemplated to take a hidden `const char*` and
  `int` from `__FILE__` / `__LINE__`; today's surface keeps
  the macros minimal.
- **Level-filter compile-time gate.** A
  `#define PETRA_LOG_LEVEL 2` plus
  `#if PETRA_LOG_LEVEL <= N` gating each macro to a no-op
  would let release builds pay zero cost for disabled
  levels. Today's macros always emit; the filter is
  follow-on.

Plus the older items Aug 22 / Aug 28 / Aug 31 carried forward:

- **`std::format` to `std::ostream` for type-erased
  `std::span<const std::any>` args** — libc++ 21 has no
  `__format_arg_store` supporting `std::any`.
- **`std::ostream_print` with NUL-terminated output** — the
  Aug 25 NUL-termination contract applied to an ostream sink
  read back via `os.str()`.
- **`std::format_to_n` to an ostream streambuf for
  NUL-terminated output** — today writes to a streambuf via
  `os.write`, not a raw buffer, so no NUL is synthesised. A
  custom ostream whose payload is read out via `os.str()`
  could expect one.
- **`would_be` on the runtime path of the n-capped sibling**
  — Aug 31's runtime entry point returns `offered` on
  success because `std::vformat_to` does not expose
  `would_be`. A two-pass shape would close this.

The cross-cutting infrastructure items from the Aug 13 / Aug
15 / Aug 17 lessons remain open: pin actions to commit SHAs;
multi-OS matrix extending to `windows-latest`; status badge
in README; vcpkg / Conan port for `psp_span_lib`; branch
protection requiring linear history.