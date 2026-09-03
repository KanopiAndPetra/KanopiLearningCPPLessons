# P-2026-09-03 — `sync_log_file_line_macros`: `__FILE__` / `__LINE__` capture for the Sep 2 SYNC_LOG_* macros

**Topic:** automatic `__FILE__:__LINE__` capture at the call
site of the Sep 2 SYNC_LOG_* macros via a new LOCSYNC_LOG_*
surface that prepends the location to the committed line while
preserving the atomic-line property `std::osyncstream` gives us
(closes the **second** of three new forward-on items Sep 1's
"Where we go next" section named — Sep 2 closed the first;
today closes the second).

**Standard:** C++23 (P0053R7 syncstream / syncbuf,
`std::format_string<Args...>`, `std::format_args`,
`std::make_format_args`, `std::reference_wrapper<std::ostream>`).
Plus `-fexperimental-library` (Apple Clang 21.0.0 toolchain
flag — Sep 2 lesson owns the platform finding; replicated here
only as the build recipe).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:** 51/51 PASS across 16 sections on six builds —
direct default, direct strict-warning, direct ASan/UBSan, CMake
default, CMake strict, CMake ASan. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion`. Zero sanitizer diagnostics. Five consecutive
ASan/UBSan runs produced identical tallies (no flakiness in
the concurrent sections).

---

## Why today

The Sep 2 lesson (`P-2026-09-02-osyncstream-log-sink`) shipped
`std::osyncstream`-backed `SYNC_LOG_DEBUG` / `SYNC_LOG_INFO` /
`SYNC_LOG_WARN` / `SYNC_LOG_ERROR` macros closing the **first**
of three new forward-on items Sep 1's "Where we go next"
section named. Sep 2 named the **second**:

> **`__FILE__` / `__LINE__` capture.** A production logger
> prepends `__FILE__:__LINE__` to each line. The SYNC macros can
> be retemplated to take a hidden `const char*` and `int` from
> `__FILE__` / `__LINE__`; today's surface keeps the macros
> minimal.

Today IS that lesson. It does four things:

1. **Mirrors the Sep 2 surface in this TU** (Parts 1-4 lifted
   verbatim): `petra::LogLevel` enum,
   `petra::log_level_tag` (5-char aligned tags),
   `petra::log_sink()&` (raw, default `&std::cerr`),
   `petra::ostream_println` (Aug 28 / Sep 1 adapter),
   `petra::sync_sink_target()&` (the
   `reference_wrapper<std::ostream>`),
   `petra::sync_log_emit` / `sync_log_emit_runtime` (the
   two-helper consteval-vs-runtime split), and the Sep 2
   `SYNC_LOG_*` macros.
2. **Adds `petra::sync_log_emit_loc(...)` and
   `sync_log_emit_loc_runtime(...)`** — the file-and-line
   variant of the Sep 2 helpers. Both emit the prefix
   `[LEVEL ] file:line ` AND the payload through the SAME
   `std::osyncstream` so the committed line remains atomic
   w.r.t. concurrent callers. The full line shape is:

   ```
   [<LEVEL>] <file>:<line> <payload>\n
   ```

   where `<file>` is a path (the literal `__FILE__` passed in)
   and `<line>` is the integer literal `__LINE__`. The level
   tag keeps the 5-char Sep 1 / Sep 2 alignment (`INFO ` has a
   trailing space so all four levels column-align).
3. **Adds `LOCSYNC_LOG_DEBUG` / `LOCSYNC_LOG_INFO` /
   `LOCSYNC_LOG_WARN` / `LOCSYNC_LOG_ERROR`** (consteval path)
   plus `LOCSYNC_LOG_*_RUNTIME` siblings. Both expand through a
   `do { ... } while (0)` wrapper so if/else hygiene is
   preserved. The macros pass `__FILE__` and `__LINE__`
   automatically; the call site never names them. The
   consteval-vs-runtime split at the MACRO layer is identical
   to Sep 1 / Sep 2: the consteval surface catches malformed
   format strings at compile time via
   `std::format_string<Args...>::basic_format_string`'s
   consteval ctor; the runtime surface catches them at format
   time via `std::format_error` thrown from `std::vformat`.
4. **Verifies the headline property the lesson exists to pin**:
   the full line `[LEVEL ] file:line payload\n` is committed
   atomically even under concurrent load. Section 11 runs 4
   threads / 250 commits / 1000 total commits and asserts
   every committed line matches the regex
   `^\[(INFO |WARN )\] .+:[0-9]+ t[0-3]=\d+$`. Section 13
   widens to 8 threads / 200 commits / 1600 total commits.
   Section 15 runs 5x determinism.

---

## The headline finding — atomicity is preserved even with the longer prefix

The atomic-line property is the property the lesson inherits
from Sep 2. Adding `file:line` to the prefix DOES NOT change
the property: the `file:line`, the level tag, the payload,
and the trailing `\n` all route through the SAME
`std::osyncstream` and the syncbuf commits them as one
indivisible write to the underlying `std::ostream`. The
prefix is longer, but the osyncstream buffer batches the
whole longer line just the same.

The `LOCSYNC_*` prefix is just a reminder at the call site
that the line carries location info; the runtime guarantees
are identical to Sep 2.

---

## Important code

### The two helpers

```cpp
template <typename... Args>
void sync_log_emit_loc(LogLevel lvl,
                       const char* file,
                       int line,
                       std::format_string<Args...> fmt,
                       Args&&... args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
    // ONE osyncstream for the whole line — tag + file:line +
    // payload all route through it so the sync.flush() at the
    // end commits them as one atomic line.
    sync << '[' << petra::log_level_tag(lvl) << "] "
         << file << ':' << line << ' ';
    std::string payload = std::vformat(fmt.get(),
                                      std::make_format_args(args...));
    sync.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
    sync.put('\n');
    sync.flush();
}

inline void sync_log_emit_loc_runtime(LogLevel lvl,
                                      const char* file,
                                      int line,
                                      std::string_view fmt,
                                      std::format_args args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
    sync << '[' << petra::log_level_tag(lvl) << "] "
         << file << ':' << line << ' ';
    std::string payload = std::vformat(fmt, args);
    sync.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
    sync.put('\n');
    sync.flush();
}
```

Both functions differ from the Sep 2 `sync_log_emit` /
`sync_log_emit_runtime` by exactly two new parameters —
`const char* file` and `int line` — which are written into
the prefix **before** the payload and **inside the SAME**
`std::osyncstream` so the committed line is one indivisible
write.

### The four consteval macros + four runtime siblings

```cpp
#define PETRA_LOCSYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit_loc((::petra::LogLevel::level), \
                                   __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define LOCSYNC_LOG_INFO(...)     PETRA_LOCSYNC_LOG_EMIT(Info,  __VA_ARGS__)
// ... Debug, Warn, Error siblings ...

#define PETRA_LOCSYNC_LOG_EMIT_RUNTIME(level, fmt, args) \
    do { \
        ::petra::sync_log_emit_loc_runtime((::petra::LogLevel::level), \
                                           __FILE__, __LINE__, \
                                           (fmt), args); \
    } while (0)

#define LOCSYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Info, fmt, args)
// ... Debug, Warn, Error siblings ...
```

The macros capture `__FILE__` and `__LINE__` at the call site
and forward them to the helpers. The call site never names
them.

---

## Observed output

```
-- section 1.sizeof+probes
-- section 2.basic-success-loc
-- section 3.file-line-accuracy
-- section 4.zero-args-loc
-- section 5.comma-in-args-loc
-- section 6.if-else-hygiene-loc
-- section 7.format-specifiers-loc
-- section 8.consteval-gate-loc
-- section 9.runtime-sibling-loc
-- section 10.nul-in-payload-loc
-- section 11.atomic-concurrent-emission-loc
-- section 12.swap-round-trip-4-loc
-- section 13.loc-stress-8x200
-- section 14.determinism-loc-alternating
-- section 15.concurrent-round-trip-5x-loc
-- section 16.empty-payload-loc

51/51 PASS
```

Identical across all six builds (direct default / direct
strict / direct ASan / CMake default / CMake strict / CMake
ASan). Five consecutive ASan runs produced identical
tallies.

A representative capture of section 2's output, captured
via a test `std::ostringstream` swapped into
`petra::sync_sink_target()`, looks like:

```
[INFO ] /…/P-2026-09-03-sync-log-file-line-macros.cpp:552 hello petra
[WARN ] /…/P-2026-09-03-sync-log-file-line-macros.cpp:553 count=42
```

— exactly the `[LEVEL ] file:line payload\n` shape the lesson
documents.

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-03-sync-log-file-line-macros.cpp \
    -o P-2026-09-03-sync-log-file-line-macros \
    && ./P-2026-09-03-sync-log-file-line-macros

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-03-sync-log-file-line-macros.cpp \
        -o P-2026-09-03-sync-log-file-line-macros-strict \
    && ./P-2026-09-03-sync-log-file-line-macros-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-03-sync-log-file-line-macros.cpp \
        -o P-2026-09-03-sync-log-file-line-macros-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       ./P-2026-09-03-sync-log-file-line-macros-asan

# CMake: default / strict / ASan
cmake -S . -B build                && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
```

### A note on LeakSanitizer

As on Aug 30 / Aug 31 / Sep 1 / Sep 2, LeakSanitizer is **not
supported on darwin/arm64** (probed: `"detect_leaks is not
supported on this platform"`). `ASAN_OPTIONS=detect_leaks=0`
was used for all ASan runs rather than the documented
`detect_leaks=1`. Every format argument in the lesson is a
named local or a captured reference so the absence of
LeakSanitizer does not silently hide leaks.

---

## Section-by-section findings

### Section 1 — sizeof + feature probes

Five CHECKs pinning the lifted Sep 2 surface plus a probe
static_assert on the return type of
`sync_log_emit_loc(petra::LogLevel::Info, "f.cpp", 7,
std::format_string<int>{"v={}."}, 1)` — must be `void`. All
PASS.

### Section 2 — basic single-thread success: line shape with file/line

Pins the shape regex
`^\[(INFO |WARN )\] [^:]+:[0-9]+ hello petra$` for the INFO
call and `^\[(INFO |WARN )\] [^:]+:[0-9]+ count=42$` for the
WARN call. Both PASS.

### Section 3 — file/line accuracy: __FILE__ / __LINE__ are the preprocessor values at the LOCSYNC_LOG_* call site

Pins both values **literally** in the committed line:

- The `LOCSYNC_LOG_INFO("first-line")` call is on line 605
  of this source; the regex requires
  `<this file path>:605 first-line`.
- The `LOCSYNC_LOG_WARN("second-line")` call is on line 606
  of this source; the regex requires
  `<this file path>:606 second-line`.

Both PASS. (The line numbers are baked into the test as
literals — `constexpr int first_line_no = 605;` — so any
edit that moves the call sites must move the constants in
lockstep. The comment block at the call sites records the
expected `__LINE__` for each call.)

### Section 4 — zero-args (`LOCSYNC_LOG_INFO("ping")`)

`ping` and `empty=   ` (the `{:>3}`-padded empty string)
both round-trip through the location prefix. Both PASS.

### Section 5 — comma-in-arguments (`LOCSYNC_LOG_INFO("pair=({},{})", a, b)`)

`std::pair<int,int>{3, 7}` round-trips whole (no comma
between-args parsing hazard). PASS.

### Section 6 — if/else hygiene

Both branches of two `if (cond) LOCSYNC_LOG_INFO(...); else
LOCSYNC_LOG_INFO(...);` constructs emit one line each —
total 4 lines, with 2 `then-branch` and 2 `else-branch`. The
`do-while(0)` wrapping preserves the binding. Both PASS.

### Section 7 — format-specifier passthrough

`{:#x}` → `0xcafe`; `{:.3f}` → `3.142`; `{:>8}` → 8-char
right-aligned; `{:<6}` → 6-char left-aligned. All four
specifiers round-trip. PASS.

### Section 8 — consteval gate via static_assert probe

The consteval-ctor gate exists at the type level on the
LOC path too (same recipe as Sep 1 / Sep 2): the helper
takes `std::format_string<Args...>` by value, so a literal
format string routes through the consteval ctor. PASS.

### Section 9 — runtime sibling: `LOCSYNC_LOG_INFO_RUNTIME`

`LOCSYNC_LOG_INFO_RUNTIME("runtime={}", make_format_args(123))`
commits `<this file>:<line> runtime=123` correctly. PASS.

### Section 10 — NUL byte in payload

A `std::string` containing `a NUL b` round-trips whole
through the location prefix — the NUL byte sits between the
file:line prefix and the trailing `\n`, byte-equal to what
was passed in. PASS.

### Section 11 — atomic concurrent emission with file/line on every committed line (THE headline section)

4 threads alternate `LOCSYNC_LOG_INFO` and `LOCSYNC_LOG_WARN`
calls, 250 times each. Total 1000 commits. For each
captured line the prefix (level + file:line) AND the payload
must be on the SAME line — no half-line interleaving. The
regex matches the full line shape:
`^\[(INFO |WARN )\] .+:[0-9]+ t[0-3]=\d+$`.

The atomic-line property is preserved EVEN WITH the
file:line in the prefix; the prefix is longer, but the
osyncstream buffer batches the whole longer line just the
same. All four CHECKs PASS — total line count is 1000, INFO
count equals WARN count, sum equals 1000, every line matches
the regex.

### Section 12 — sink-swap behaviour with file/line on every swap

Four rounds of `ref = std::ref(oss); LOCSYNC_LOG_INFO(...);
ref = std::ref(*prev_raw);` all commit lines whose prefix
contains the literal `<this file>:`. All four lines match
the section-3 regex. PASS.

### Section 13 — LOC surface stress: 8 threads, 200 commits each

Wider experiment to confirm the longer prefix (level +
file:line) doesn't degrade the atomicity guarantee. 8
threads / 200 commits / 1600 total commits. The
committed-line regex widens the thread-index range to
`t[0-7]`. Every line matches. PASS.

### Section 14 — determinism: alternating LOC emit, 50 rounds

50 rounds of 4 alternating INFO/WARN calls each. Every
captured line matches the section-3 regex with
`round[0-9]+-i[0-3]` payload. Total: 100 INFO + 100 WARN.
PASS.

### Section 15 — concurrent round-trip, 5x determinism

5 separate runs of the section-11 concurrent experiment
(4 threads / 50 commits / 200 commits per round). Every run
produces exactly the expected count, every line matches the
section-11 regex. PASS — no flakiness across runs.

### Section 16 — empty payload (`LOCSYNC_LOG_INFO("")`)

The empty-format-string path commits the literal line
`"[INFO ] " + <this file path> + ":1146 \n"`. The literal
line 1146 is the line of the `LOCSYNC_LOG_INFO("")` call.
PASS.

---

## Where we go next

Today closes the **second of the three** new follow-on items
Sep 1's "Where we go next" section named. The remaining
one is:

- **Level-filter compile-time gate.** A
  `#define PETRA_SYNC_LOG_LEVEL 2` plus
  `#if PETRA_SYNC_LOG_LEVEL <= N` gating each SYNC /
  LOCSYNC macro to a no-op would let release builds pay
  zero cost for disabled levels. Today's macros always
  emit; the filter is follow-on. (The LOCSYNC_LOG_*
  macros inherit the same `#if`-gate recipe as the SYNC_*
  macros; both surfaces would be gated together.)

Plus the older items Sep 1 / Sep 2 carried forward:

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
