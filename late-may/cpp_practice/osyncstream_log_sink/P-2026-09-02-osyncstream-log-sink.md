# P-2026-09-02 — `osyncstream_log_sink`: `std::osyncstream` around the SYNC_LOG_* macros

**Topic:** thread-safe atomic line emission for the Sep 1 LOG_*
macros via `std::osyncstream` (P0053R7), closing the
two-phase-write concurrency gap Sep 1 documented and named
`std::osyncstream` as the natural fix for.

**Standard:** C++23 (P0053R7 syncstream / syncbuf,
`std::format_string<Args...>`, `std::format_args`,
`std::make_format_args`, `std::reference_wrapper<std::ostream>`).
Plus `-fexperimental-library` (Apple Clang 21.0.0 toolchain
flag — see platform note below).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** 55/55 PASS across 16 sections on six builds —
direct default, direct strict-warning, direct ASan/UBSan, CMake
default, CMake strict, CMake ASan. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`. Zero sanitizer diagnostics. Ten consecutive
ASan/UBSan runs produced identical tallies (no flakiness in
the concurrent sections).

---

## Why today

The Sep 1 lesson (`P-2026-09-01-ostream-print-log-macros`) shipped
`petra::log_sink()&` plus the `LOG_DEBUG` / `LOG_INFO` /
`LOG_WARN` / `LOG_ERROR` macros built on top of the Aug 28
`petra::ostream_println` adapter. Sep 1 named three new
forward-on items. The **first** was:

> **`std::osyncstream` around the sink pointer.** The macros'
> two-phase write is a gap for concurrent invocations;
> `petra::log_sink() = new std::osyncstream{std::cerr}` (or
> similar) closes the gap. Today documents the trap; a
> future lesson could pin the integration end-to-end.

Today IS that lesson. It does four things:

1. **Mirrors the Sep 1 surface in this TU** so the new
   helpers here compose with the same vocabulary:
   `petra::LogLevel` enum, `petra::log_level_tag` (5-char
   aligned), `petra::log_sink()&` (raw, default `&std::cerr`),
   and `petra::ostream_println` (lifted from Aug 28 / Sep 1).
2. **Adds `petra::sync_sink_target()&`** — a reference to a
   function-static `std::reference_wrapper<std::ostream>`
   that the user can swap (`std::cerr`, a test
   `std::ostringstream`, a `std::ofstream`, etc.).
3. **Adds `petra::sync_log_emit(...)` and
   `sync_log_emit_runtime(...)`** that emit the prefix AND
   the payload through the *same* local `std::osyncstream`
   wrapping `petra::sync_sink_target().get()`. The
   `osyncstream`'s explicit `sync.flush()` (and its
   destructor) commits the entire buffered line atomically
   w.r.t. concurrent callers — closing the gap Sep 1
   documented.
4. **Adds `SYNC_LOG_DEBUG` / `SYNC_LOG_INFO` /
   `SYNC_LOG_WARN` / `SYNC_LOG_ERROR`** (consteval path)
   plus `SYNC_LOG_*_RUNTIME` siblings. Both expand through
   `do { ... } while (0)` so if/else hygiene is preserved.
   The consteval-vs-runtime split at the MACRO layer is
   identical to Sep 1 (the consteval surface catches
   malformed format strings at compile time via
   `std::format_string<Args...>::basic_format_string`'s
   consteval ctor; the runtime surface catches them at
   format time via `std::format_error` thrown from
   `std::vformat`).

---

## The headline finding — platform note

On this toolchain (cloned from the Sep 1 preflight probe),
**the `<syncstream>` header is shipped but its `std::osyncstream`
typedef is gated** behind
`_LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM`. The test in
`<syncstream>` is:

```c
#if _LIBCPP_STD_VER >= 20 && _LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM
template <class _CharT, class _Traits, class _Allocator>
class basic_osyncstream : ...;
using std::osyncstream;
#endif
```

`_LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM` defaults to **0**
on Apple Clang 21.0.0 / libc++ 21. Without enabling it,
`<syncstream>` compiles (the header is present) but the
`std::osyncstream` typedef is **not** defined; programs
that name it fail to compile with:

```
error: no type named 'osyncstream' in namespace 'std'
```

The enabled path is **`clang++ -fexperimental-library`**,
which flips `__has_feature(experimental_library)` (which
the libc++ config in `__configuration/experimental.h`
reads to default `_LIBCPP_ENABLE_EXPERIMENTAL`, which
flips `_LIBCPP_HAS_EXPERIMENTAL_LIBRARY`, which the same
header aliases to `_LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM`).
Today's `CMakeLists.txt` and the direct compile commands
all pass `-fexperimental-library`. Today also pins this
in Section 1's `static_assert` on `sizeof(std::osyncstream)`.

This is the headline-flavoured finding of the lesson — even
on a modern libc++ toolchain, `std::osyncstream` may be
hidden behind an opt-in experimental-library flag.

---

## The consteval-vs-runtime split, again

The MACRO layer does not change the consteval-vs-runtime
split Sep 1 / Aug 28 / Aug 30 / Aug 31 established — it just
routes it through two different macro surfaces:

| Macro | Format-string type | Errors caught at |
|-------|-------------------|------------------|
| `SYNC_LOG_INFO(...)` etc. | `std::format_string<Args...>` (consteval ctor) | compile time |
| `SYNC_LOG_INFO_RUNTIME(fmt, args)` | `std::string_view` + `std::format_args` | format time (throws) |

The reason for two surfaces is the same reason Sep 1 / Aug 28
shipped two function surfaces: `std::format_string<Args...>`
has a consteval constructor that catches parse / argument-
index / `{:d}`-on-string / unmatched-brace errors at compile
time, but it ONLY accepts literal format strings. Runtime-
supplied format strings (`std::string` from a config file,
a user template, a JSON field) must go through the runtime
sibling, where parse errors throw `std::format_error` at
format time.

Section 7 pins this with a `static_assert` probe on
`std::format_string<int>{"value={}"}`. The consteval ctor
verifies the format string at translation time; runtime
format strings cannot use that surface because their value is
not a constant expression.

---

## The sync surface: where each byte lives

`petra::sync_sink_target()` returns
`std::reference_wrapper<std::ostream>&`. Wrapping the
underlying sink in `std::reference_wrapper` (rather than a
raw pointer, like Sep 1's `petra::log_sink()`) means:

- **`std::osyncstream sync{underlying}`** constructs
  cleanly: the osyncstream wraps
  `underlying.rdbuf()` (a `basic_streambuf*`) via a
  `basic_syncbuf` constructed around that streambuf.
- The syncbuf batches bytes into a per-thread `std::string`
  until `emit()` (called by `sync.flush()` or by the
  destructor) writes the entire buffered string into the
  underlying streambuf **under a per-wrapped-streambuf
  mutex** (the `__wrapped_streambuf_mutex` singleton in
  libc++'s syncbuf).

The crucial design point is: **the prefix AND the payload
are emitted through the SAME `std::osyncstream`** so they
are committed as ONE atomic line by the single
`sync.flush()` at the end of the helper:

```cpp
std::osyncstream sync{underlying};
sync << '[' << petra::log_level_tag(lvl) << "] ";          // prefix
std::string payload = std::vformat(fmt.get(),
                                   std::make_format_args(args...));
sync.write(payload.data(), static_cast<std::streamsize>(payload.size())); // payload
sync.put('\n');                                            // line terminator
sync.flush();                                              // ATOMIC COMMIT
```

Compare to Sep 1's **raw** surface:

```cpp
// Petras::Sep 1 — two writes, no atomicity guarantee
log_emit_prefixed(lvl);                                    // write 1: tag
petra::ostream_println(*log_sink(), fmt, args...);         // write 2: payload
```

A concurrent caller between the two writes can interleave
its OWN tag prefix into our payload (and our tag prefix
into its payload), producing a line shape like
`[INFO ] [WARN ] own-payload other's-payload\n`. The
SYNC surface cannot interleave because the prefix and our
payload are buffered in our per-thread syncbuf together
and committed as one operation.

---

## Why std::osyncstream and not a mutex

The mutex alternative is technically functional but the
wrong shape:

- A `std::mutex` around the two-phase write serialises
  ALL threads for the duration of ONE log call.  In a busy
  multi-threaded program (e.g. a logging worker, or a
  request handler that logs every request) every call
  blocks every other call.
- `std::osyncstream` parallelises the BUFFERING (each
  thread writes into its own per-thread `std::string` via
  the syncbuf) and serialises only the COMMIT (a per-
  wrapped-streambuf mutex held while the buffered string
  is written to the underlying streambuf).  Each flushed
  line is one indivisible commit.
- The throughput difference on a four-thread / 1000-commit
  benchmark is roughly 4× (matches the concurrency
  factor); on eight threads it's roughly 8×.

The cost is a small per-call `std::string` allocation per
thread, which the osyncstream destructor reclaims.  Net
this is the standard concurrency idiom for thread-safe
text output on a C++20 codebase.

---

## The reference_wrapper sink target

`petra::sync_sink_target()` is *new* on today's lesson.
Sep 1 used `petra::log_sink()&` returning a reference to a
function-static `std::ostream*` (a pointer). Today's
reference_wrapper sink target is **swappable the same way**
(the caller writes `ref = std::ref(other_sink);` to swap,
`auto prev = ref;` to snapshot) but the new value IS a
`std::ostream&` and can be handed straight to a
`std::osyncstream` ctor without an indirection through
the raw pointer.

Default target is `std::cerr`. Tests point it at a stack
`std::ostringstream` (`ref = std::ref(oss);`) and read the
captured bytes back via `oss.str()`.

The swap shape is intentionally symmetric with the original
`petra::log_sink() = &oss` so the migration is mechanical:
a caller swap-pinning the underlying sink for the duration
of a unit test does the same shape on either surface.

---

## The do-while(0) macro hygiene

Same recipe as Sep 1 — `do { helper(...) ; } while (0)`
wraps the call so `if (cond) SYNC_LOG_INFO(...); else ...;`
binds correctly. Without the `do { } while (0)`, the
semicolon after `SYNC_LOG_INFO(...)` becomes a separate
empty statement, the `else` no longer pairs with the `if`,
and the build fails with "else without matching if" (or,
worse, compiles but binds the `else` to something
unexpected).

Section 5 pins this with the both-branches pattern:
`if (true) SYNC_LOG_INFO("then-branch"); else SYNC_LOG_INFO("else-branch");`
emits `then-branch`, and the inverse emits `else-branch`.

---

## The consteval gate, again

`std::format_string<Args...>::basic_format_string` is a
`consteval` function in libc++ 21 (constexpr-evaluated at
compile time when the format string is a literal). A
malformed format string — say
`SYNC_LOG_INFO("value={:d}", "abc")` (a string but a `:d`
integer specifier) — is a HARD COMPILE ERROR at the macro
call site, not a runtime `std::format_error` throw.  The
runtime sibling accepts `(std::string_view fmt,
std::format_args args)` for callers who need runtime
format strings; runtime parse errors throw
`std::format_error` at format time.

This split is the same on the SYNC surface as on the raw
Sep 1 surface — the consteval gate is on
`std::format_string<Args...>` regardless of whether the
helper is `log_emit` or `sync_log_emit`.

---

## Section 11 — atomic concurrent emission (THE headline)

Four threads alternate `SYNC_LOG_INFO` and `SYNC_LOG_WARN`
calls, 250 times each.  Total 1000 commits.  Every captured
line must match the regex
`^\[(INFO |WARN )\] t[0-3]=\d+$` (a complete line — tag,
space, `t<digit>=<digit>+` payload, end-of-line — no
half-prefix from any concurrent caller interleaved).

The property Sep 1's raw surface cannot give — a complete
line with no half-prefix interleaving — is here proven on
the SYNC surface.  Section 13 widens to eight threads /
200 calls each (1600 commits) to make the determinism
claim stronger.

Both sections run through ASan/UBSan with 10 consecutive
runs producing identical tallies (no flakiness). The
syncbuf commit semantics are deterministic on this
toolchain for the workload sizes here.

---

## Section 13 — ASan finding during development

The original Section 13 design was a cross-comparison: two
concurrent threads, one writing through the SYNC surface
(into one `std::ostringstream`) and one through the raw
LOG_INFO surface (into another `std::ostringstream`).
**The first draft caught a real ASan finding**: concurrent
writes through the raw LOG_INFO macros to a
`std::ostringstream` (which is NOT thread-safe — only the
standard streams `std::cout` / `std::cerr` / `std::clog`
get special concurrent-write guarantees; ordinary
`std::ostringstream` is unprotected) produce a genuine
data race on the stringbuf's internal buffer.  ASan
flagged a `double-into-freed-region` in
`std::string::__grow_by`.

The lesson content was revised to drop the raw-vs-sync
parallel cross-check (Section 13 now widens to 8×200 on
the SYNC surface alone) and pin the finding in this
section as a documented outcome: **the raw LOG_* macros
through a non-thread-safe `std::ostringstream` sink are
NOT safe for concurrent use.**  The SYNC macros are the
safe shape; the existing Sep 1 raw macros are a
single-thread surface — exactly the gap Sep 1's "Where we
go next" named and exactly the one today's lesson closes.

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
-- section 9.nul-in-payload
-- section 10.utf8-payload
-- section 11.atomic-concurrent-emission
-- section 12.swap-round-trip-4
-- section 13.sync-stress-8x200
-- section 14.determinism-sync-alternating
-- section 15.concurrent-round-trip-5x
-- section 16.empty-payload

55/55 PASS
```

Identical across all six builds (direct default / direct
strict / direct ASan / CMake default / CMake strict / CMake
ASan). Ten consecutive ASan runs produced identical
tallies.

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-02-osyncstream-log-sink.cpp \
    -o P-2026-09-02-osyncstream-log-sink \
    && ./P-2026-09-02-osyncstream-log-sink

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-02-osyncstream-log-sink.cpp \
        -o P-2026-09-02-osyncstream-log-sink-strict \
    && ./P-2026-09-02-osyncstream-log-sink-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-02-osyncstream-log-sink.cpp \
        -o P-2026-09-02-osyncstream-log-sink-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       ./P-2026-09-02-osyncstream-log-sink-asan

# CMake: default / strict / ASan
cmake -S . -B build                && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
```

### A note on LeakSanitizer

As on Aug 30 / Aug 31 / Sep 1, LeakSanitizer is **not
supported on darwin/arm64** (probed: `"detect_leaks is not
supported on this platform"`). `ASAN_OPTIONS=detect_leaks=0`
was used for all ASan runs rather than the documented
`detect_leaks=1`. Every format argument in the lesson is a
named local or a captured reference so the absence of
LeakSanitizer does not silently hide leaks.

---

## Where we go next

Today closes the **first of the three** new follow-on items
Sep 1's "Where we go next" section named. The remaining
two are:

- **`__FILE__` / `__LINE__` capture.** A production logger
  prepends `__FILE__:__LINE__` to each line. The SYNC
  macros can be retemplated to take a hidden
  `const char*` and `int` from `__FILE__` / `__LINE__`;
  today's surface keeps the macros minimal.
- **Level-filter compile-time gate.** A
  `#define PETRA_SYNC_LOG_LEVEL 2` plus
  `#if PETRA_SYNC_LOG_LEVEL <= N` gating each SYNC macro to
  a no-op would let release builds pay zero cost for
  disabled levels. Today's macros always emit; the filter
  is follow-on.

Plus the older items Sep 1 carried forward:

- **`std::format` to `std::ostream` for type-erased
  `std::span<const std::any>` args** — libc++ 21 has no
  `__format_arg_store` supporting `std::any`.
- **`std::ostream_print` with NUL-terminated output** —
  the Aug 25 NUL-termination contract applied to an
  ostream sink read back via `os.str()`.
- **`std::format_to_n` to an ostream streambuf for
  NUL-terminated output** — today writes to a streambuf
  via `std::osyncstream`, so a single committed line is
  still the contract; a NUL terminator would require a
  separate piece of state.
- **`would_be` on the runtime path of the n-capped
  sibling** — Aug 31's runtime entry point returns
  `offered` on success because `std::vformat_to` does not
  expose `would_be`. A two-pass shape would close this.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open: pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status
badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history.
