# P-2026-09-04 — `sync_log_level_filter`: `PETRA_SYNC_LOG_LEVEL` compile-time gate for the Sep 2 SYNC_LOG_* and Sep 3 LOCSYNC_LOG_* macros

**Topic:** a single `#define PETRA_SYNC_LOG_LEVEL N` that reduces
the gated Sep 2 / Sep 3 macro families to `((void)0)` no-ops
at compile time, with a runtime diagnostic accessor reporting
the gate value (read-only — the gate is fixed for the lifetime
of the binary), a macro-stringify probe via the
double-indirection stringification trick that statically asserts
the macro expansion matches the expected form, and a separate
gate-verify driver that exercises all four gate values (0/1/2/3)
and asserts the captured-line count matches the gate-emit rule
(closes the **third** and final new follow-on item Sep 1's
"Where we go next" section named — the "level-filter
compile-time gate" item Sep 1 / Sep 2 / Sep 3 carried forward).

**Standard:** C++23 (P0053R7 syncstream / syncbuf,
`std::format_string<Args...>`, `std::format_args`,
`std::make_format_args`, `std::reference_wrapper<std::ostream>`).
Plus `-fexperimental-library` (Apple Clang 21.0.0 toolchain
flag — Sep 2 lesson owns the platform finding; replicated here
only as the build recipe).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++ 21. CMake 4.3.4.

**Result:**
- Main TU (`P-2026-09-04-sync-log-level-filter.cpp`): **93/93 PASS**
  across 16 sections on default + strict-warning + ASan/UBSan
  builds at the default gate (0). Zero warnings under
  `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
  -Wsign-conversion`. Zero sanitizer diagnostics. Five
  consecutive runs produced identical tallies.
- Gate-verify driver (`P-2026-09-04-sync-log-level-filter-gate-verify.cpp`):
  **1/1 PASS** at each of gate=0 (16 emitted lines), gate=1
  (12 lines), gate=2 (8 lines), gate=3 (4 lines) on default +
  strict + ASan builds. Zero warnings. Zero sanitizer
  diagnostics. The exact 16/12/8/4 line counts confirm the
  gate's per-level drop rule (each level drops 4 lines — one
  per macro family: SYNC, SYNC_RUNTIME, LOCSYNC,
  LOCSYNC_RUNTIME).

---

## Why today

The Sep 3 lesson (`P-2026-09-03-sync-log-file-line-macros`) closed
the **second** of three new forward-on items Sep 1's "Where we
go next" section named. Sep 1 / Sep 2 / Sep 3 all carried forward
the last one:

> **Level-filter compile-time gate.** A `#define
> PETRA_SYNC_LOG_LEVEL 2` plus `#if PETRA_SYNC_LOG_LEVEL <= N`
> gating each SYNC / LOCSYNC macro to a no-op would let release
> builds pay zero cost for disabled levels. Today's macros
> always emit; the filter is follow-on. (The LOCSYNC_LOG_*
> macros inherit the same `#if`-gate recipe as the SYNC_*
> macros; both surfaces would be gated together.)

Today IS that lesson. It does five things:

1. **Mirrors the Sep 2 + Sep 3 surface in this TU** (Parts 1-5
   lifted verbatim): `petra::ostream_println`,
   `petra::LogLevel` / `log_level_tag` / `log_sink`,
   `petra::sync_sink_target`, `petra::sync_log_emit` /
   `sync_log_emit_runtime`, `petra::sync_log_emit_loc` /
   `sync_log_emit_loc_runtime`, and the Sep 2 SYNC_LOG_* + Sep 3
   LOCSYNC_LOG_* macros.

2. **Adds the COMPILE-TIME gate.** A `#define PETRA_SYNC_LOG_LEVEL`
   (default 0 — Debug — emit every level). Each SYNC_LOG_* and
   LOCSYNC_LOG_* macro is wrapped in `#if PETRA_SYNC_LOG_LEVEL <= N`
   so a level above the gate reduces to `((void)0)` and the helper
   call / the format-string construction / the `vformat()` parse
   / the osyncstream construction ALL disappear from the binary.
   At or above the gate, the expansion is unchanged from
   Sep 2 / Sep 3.

3. **Adds the runtime diagnostic accessor
   `petra::sync_log_level() noexcept`** returning the
   COMPILE-TIME gate value. The accessor exists so tests can
   probe "what gate was this binary built with?" without
   reaching into the preprocessor symbol directly. There is no
   runtime setter — the gate is fixed for the lifetime of the
   binary by design (see the headline-finding block below).

4. **Adds the macro-stringify probe (Section 14)** via the
   double-indirection stringification trick
   (`PETRA_STRINGIFY_INNER(x)` / `PETRA_STRINGIFY(x)`) plus
   `PETRA_PROBE_SYNC_LOG_*()` probe-macros. The probes
   statically assert the macro expansion matches the expected
   string at the current gate. At gate=0 all four macros expand
   to the full `do { ... } while (0)` emit form; at gate>0 some
   macros expand to `((void)0)`. The static_asserts make the
   gate's effect observable from inside the TU — no runtime knob
   is needed.

5. **Adds a separate gate-verify driver
   (`P-2026-09-04-sync-log-level-filter-gate-verify.cpp`)** that
   exercises every macro at every level, captures the output to
   a `std::ostringstream`, and asserts the captured-line count
   matches the gate-emit rule:
   - gate=0: 16 lines (4 levels × 4 macro families)
   - gate=1: 12 lines (drops DEBUG: 16 − 4)
   - gate=2: 8 lines (drops DEBUG + INFO: 16 − 8)
   - gate=3: 4 lines (drops DEBUG + INFO + WARN: 16 − 12)

The driver is built four times via CMake (one per gate value)
via `target_compile_definitions(... PETRA_SYNC_LOG_LEVEL=${gate})`
and runs the same source at every gate. The headline finding —
**dead-strip at compile time** — is observable directly via the
exact 16/12/8/4 line counts.

---

## The headline finding — compile-time `#if` gate, not a runtime check

The spec Sep 1 named the recipe literally:

> `#define PETRA_SYNC_LOG_LEVEL 2` plus `#if PETRA_SYNC_LOG_LEVEL <= N`
> gating each SYNC / LOCSYNC macro to a no-op.

Today's lesson IS that recipe. The gate is a **preprocessor
`#if`**, not a runtime `if`. Three reasons:

1. **Dead-strip.** A `#if PETRA_SYNC_LOG_LEVEL <= 0` followed by
   `((void)0)` lets the COMPILER drop the gated branch entirely.
   A runtime check would compile the gated branch into a
   conditional — the function call, the format-string parse, the
   osyncstream construction would still be in the binary, just
   skipped at runtime. For a DEBUG-heavy production build the
   difference is substantial: every DEBUG call site is a full
   format-string + `vformat()` + osyncstream construction that
   the preprocessor gate removes.

2. **The consteval gate stays at compile time.** Sep 2 / Sep 3
   routed every consteval macro through `std::format_string<Args...>`
   whose ctor is consteval — meaning parse / type-mismatch errors
   become HARD COMPILE ERRORS at the call site. A runtime
   `if`-gate doesn't break that property, but the format-string
   would still be parsed at compile time. A compile-time `#if`
   gate is consistent: the parse happens once and only for
   emitted levels.

3. **The recipe matches what the spec named.** The Sep 1 "Where
   we go next" section explicitly named `#define PETRA_SYNC_LOG_LEVEL
   2` plus `#if PETRA_SYNC_LOG_LEVEL <= N`. The lesson IS the
   recipe.

So the gate is `#if PETRA_SYNC_LOG_LEVEL <= N` and each macro has
up to four preprocessor branches. At gate=0 every macro is the
Sep 2 / Sep 3 expansion (unchanged behaviour). At gate=2 DEBUG
and INFO expand to `((void)0)`. At gate=3 only ERROR emits.

---

## The second finding — the lvalue-for-format-args recipe doesn't compose with a high gate

The runtime format-string macros (Sep 1 / Sep 2 / Sep 3) require
`std::make_format_args(lvalue)` because `make_format_args` takes
lvalue-refs:

```cpp
int val = 42;
SYNC_LOG_INFO_RUNTIME("value={}", std::make_format_args(val));
//                                 ^^^^^^^^^^^^^^^^^^^^^^^
//                                 passes val BY lvalue-ref
```

At default gate=0 this is fine — the macro expands to the
emit-form which references `val`. At higher gates, the macro
expands to `((void)0)` and `val` becomes unused. The
strict-warning build with `-Werror -Wunused-variable` then
fails with:

```
error: unused variable 'val' [-Werror,-Wunused-variable]
```

This is a **real and important lesson finding** — production
code that uses the runtime format-string siblings MUST gate the
lvalue declarations behind `#if PETRA_SYNC_LOG_LEVEL <= N` too,
or the strict-warning build breaks at any gate above the
declared level.

The gate-verify TU documents this with `(void)val;` discard
statements keyed on the gate:

```cpp
int d_val = 1, i_val = 2, w_val = 3, e_val = 4;
if (PETRA_SYNC_LOG_LEVEL > 0) (void)d_val;  // d_val used only if gate <= 0
if (PETRA_SYNC_LOG_LEVEL > 1) (void)i_val;  // i_val used only if gate <= 1
if (PETRA_SYNC_LOG_LEVEL > 2) (void)w_val;  // w_val used only if gate <= 2
(void)e_val;                                  // e_val always used (ERROR)
```

This pattern is awkward (the production idiom is to wrap the
whole declaration in `#if PETRA_SYNC_LOG_LEVEL <= N`). The
gate-verify TU uses the discard-statements because it must be
buildable at every gate value from a single source file; the
production idiom is the `#if`-wrapped declaration.

The main lesson TU doesn't hit this finding because it's
designed for default gate=0 — Section 14's `if constexpr
(false)`-discarded probe-calls also keep the lvalue
declarations referenced.

---

## The third finding — stringification of function-like macro names

Section 14's macro-stringify probe uses the
double-indirection stringification trick:

```cpp
#define PETRA_STRINGIFY_INNER(x) #x
#define PETRA_STRINGIFY(x) PETRA_STRINGIFY_INNER(x)
```

For object-like macros (`#define PETRA_EXPECTED_DEBUG_GATE_STR "..."`),
`PETRA_STRINGIFY(PETRA_EXPECTED_DEBUG_GATE_STR)` returns the
literal string. For function-like macros
(`#define SYNC_LOG_DEBUG(...) ...`), `#x` produces a string of
x's PRE-EXPANSION text — but the preprocessor only expands an
identifier if it's followed by `(`. `PETRA_STRINGIFY(SYNC_LOG_DEBUG)`
returns `"SYNC_LOG_DEBUG"` (the macro name), NOT its expansion.

To capture the expansion of a function-like macro NAME, the
trick is to define a helper that DOES call it and stringify
the helper:

```cpp
#define PETRA_PROBE_SYNC_LOG_DEBUG() SYNC_LOG_DEBUG()
#define PETRA_ACTUAL_DEBUG_GATE_STR PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_DEBUG())
```

`PETRA_PROBE_SYNC_LOG_DEBUG()` is a function-like macro that
calls `SYNC_LOG_DEBUG()` with no arguments. `PETRA_STRINGIFY`
sees the call, expands `SYNC_LOG_DEBUG()` to
`PETRA_SYNC_LOG_EMIT(Debug, )` (or the no-op form), then
stringifies the fully-expanded form. The resulting string is
the actual macro expansion at the current gate.

Section 14's `static_assert(std::string_view(PETRA_ACTUAL_DEBUG_GATE_STR)
== std::string_view(PETRA_EXPECTED_DEBUG_GATE_STR), ...)` is
the compile-time check. If the gate is correct, both strings
are byte-equal; if the gate is misconfigured, the compile
fails with a clear message identifying the mismatch.

This trick is the same one libstdc++ / libc++ use for
`__PRETTY_FUNCTION__` / `std::source_location` probes and is
worth pinning once.

---

## Important code

### The gate macro definition

```cpp
#ifndef PETRA_SYNC_LOG_LEVEL
#define PETRA_SYNC_LOG_LEVEL 0
#endif

static_assert(PETRA_SYNC_LOG_LEVEL >= 0 && PETRA_SYNC_LOG_LEVEL <= 3,
              "PETRA_SYNC_LOG_LEVEL must be 0..3");
```

The default is 0 — Debug — emit every level (matches Sep 2 /
Sep 3 behaviour). A release build sets
`-DPETRA_SYNC_LOG_LEVEL=2` to drop DEBUG + INFO at compile
time. The `static_assert` catches misconfigured values at
compile time.

### The four gated macro families

Each macro has the shape:

```cpp
#if PETRA_SYNC_LOG_LEVEL <= N
#define SYNC_LOG_INFO(...)  PETRA_SYNC_LOG_EMIT(Info, __VA_ARGS__)
#else
#define SYNC_LOG_INFO(...)  PETRA_SYNC_LOG_NOOP(Info)
#endif
```

`PETRA_SYNC_LOG_EMIT` is the unchanged Sep 2 do-block wrapper:

```cpp
#define PETRA_SYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit((::petra::LogLevel::level), __VA_ARGS__); \
    } while (0)
```

`PETRA_SYNC_LOG_NOOP` is the new today no-op:

```cpp
#define PETRA_SYNC_LOG_NOOP(level) ((void)0)
```

The same shape applies to SYNC_LOG_*, SYNC_LOG_*_RUNTIME,
LOCSYNC_LOG_*, and LOCSYNC_LOG_*_RUNTIME — 16 macros total.
The full set lives in Part 6 of the main TU.

### The runtime diagnostic accessor

```cpp
namespace petra {
[[nodiscard]] inline int sync_log_level() noexcept {
    return PETRA_SYNC_LOG_LEVEL;
}
}
```

Returns the COMPILE-TIME gate value. The accessor exists so
tests can probe "what gate was this binary built with?"
without reaching into the preprocessor symbol directly.
There is no setter — the gate is fixed for the lifetime of
the binary by design.

### The macro-stringify probe

```cpp
#define PETRA_STRINGIFY_INNER(x) #x
#define PETRA_STRINGIFY(x) PETRA_STRINGIFY_INNER(x)

#define PETRA_PROBE_SYNC_LOG_DEBUG() SYNC_LOG_DEBUG()
#define PETRA_PROBE_SYNC_LOG_INFO()  SYNC_LOG_INFO()
#define PETRA_PROBE_SYNC_LOG_WARN()  SYNC_LOG_WARN()
#define PETRA_PROBE_SYNC_LOG_ERROR() SYNC_LOG_ERROR()

#define PETRA_ACTUAL_DEBUG_GATE_STR PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_DEBUG())
#define PETRA_ACTUAL_INFO_GATE_STR  PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_INFO())
#define PETRA_ACTUAL_WARN_GATE_STR  PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_WARN())
#define PETRA_ACTUAL_ERROR_GATE_STR PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_ERROR())

static_assert(
    std::string_view(PETRA_ACTUAL_DEBUG_GATE_STR) ==
    std::string_view(PETRA_EXPECTED_DEBUG_GATE_STR),
    "SYNC_LOG_DEBUG macro expansion doesn't match the gate");
// ... INFO, WARN, ERROR siblings ...
```

The probe captures the actual fully-expanded form via the
double-indirection stringification trick. The `static_assert`
is the gate's compile-time identity check.

---

## Observed output (default gate=0, main TU)

```
-- section 1.sizeof+probes
-- section 2.default-emits-all
-- section 3.runtime-siblings
-- section 4.locsync-consteval
-- section 5.locsync-runtime
-- section 6.if-else-hygiene
-- section 7.format-specifiers
-- section 8.consteval-gate-preserved
-- section 9.sink-swap
-- section 10.helper-layer-unconditional
-- section 11.full-macro-sweep
-- section 12.concurrent-atomic
-- section 13.gate-value-at-build
-- PETRA_SYNC_LOG_LEVEL = 0
-- section 14.macro-stringify
-- PETRA_SYNC_LOG_LEVEL = 0
-- macro SYNC_LOG_DEBUG -> "do { ::petra::sync_log_emit((::petra::LogLevel::Debug), ); } while (0)"
-- macro SYNC_LOG_INFO  -> "do { ::petra::sync_log_emit((::petra::LogLevel::Info), ); } while (0)"
-- macro SYNC_LOG_WARN  -> "do { ::petra::sync_log_emit((::petra::LogLevel::Warn), ); } while (0)"
-- macro SYNC_LOG_ERROR -> "do { ::petra::sync_log_emit((::petra::LogLevel::Error), ); } while (0)"
-- section 15.gate-smoke
-- section 16.error-always-emits

93/93 PASS
```

## Observed output (gate-verify driver at every gate value)

```
gate=0: -- captured line count = 16 (expected 16)
gate=1: -- captured line count = 12 (expected 12)
gate=2: -- captured line count = 8 (expected 8)
gate=3: -- captured line count = 4 (expected 4)
```

The exact 16/12/8/4 progression confirms the gate's
per-level drop rule: each level above the gate drops 4 lines
(one per macro family — SYNC, SYNC_RUNTIME, LOCSYNC,
LOCSYNC_RUNTIME).

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
    P-2026-09-04-sync-log-level-filter.cpp \
    -o /tmp/P-2026-09-04-sync-log-level-filter \
    && /tmp/P-2026-09-04-sync-log-level-filter

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ -fexperimental-library \
        P-2026-09-04-sync-log-level-filter.cpp \
        -o /tmp/P-2026-09-04-sync-log-level-filter-strict \
    && /tmp/P-2026-09-04-sync-log-level-filter-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        -fexperimental-library \
        P-2026-09-04-sync-log-level-filter.cpp \
        -o /tmp/P-2026-09-04-sync-log-level-filter-asan \
    && ASAN_OPTIONS=detect_leaks=0 \
       /tmp/P-2026-09-04-sync-log-level-filter-asan

# gate-verify driver — built at every gate value
for g in 0 1 2 3; do
    clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
            -DPETRA_SYNC_LOG_LEVEL=$g -Wall -Wextra -Wpedantic -Werror \
            -Wshadow -Wconversion -Wsign-conversion \
            P-2026-09-04-sync-log-level-filter-gate-verify.cpp \
            -o /tmp/gate-verify-$g \
        && /tmp/gate-verify-$g
done

# CMake: default / strict / ASan — builds all five targets
cmake -S . -B build
cmake --build build
cmake -S . -B build-strict \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
```

### A note on LeakSanitizer

As on Aug 30 / Aug 31 / Sep 1 / Sep 2 / Sep 3, LeakSanitizer
is **not supported on darwin/arm64** (probed: `"detect_leaks is
not supported on this platform"`). `ASAN_OPTIONS=detect_leaks=0`
was used for all ASan runs rather than the documented
`detect_leaks=1`. Every format argument in the lesson is a
named local or a captured reference so the absence of
LeakSanitizer does not silently hide leaks.

---

## Section-by-section findings

### Section 1 — sizeof + feature probes + gate-constant probes

Five CHECKs pinning the lifted Sep 2 / Sep 3 surface plus a
probe static_assert on the gate value (0..3). All PASS.

### Section 2 — baseline (default gate=0) emits all four levels

At default gate=0, DEBUG / INFO / WARN / ERROR all emit. This
section is the "today's macros always emit" baseline that Sep 2
/ Sep 3 established — at the default gate the new gating macros
are observationally indistinguishable from yesterday's. The
section asserts `[DEBUG] d=1\n`, `[INFO ] i=2\n`,
`[WARN ] w=3\n`, and `[ERROR] e=4\n` all appear in the
captured output. All PASS.

### Section 3 — runtime siblings at default gate

Same shape as Section 2 with the runtime format-string siblings.
`std::make_format_args` requires lvalue refs so the values are
declared as `int d_val = 11, i_val = 12, ...` locals. The
captured output matches `[DEBUG] drt=11\n`, `[INFO ] irt=12\n`,
`[WARN ] wrt=13\n`, `[ERROR] ert=14\n`. All PASS.

### Section 4 — LOC macros at default gate

LOCSYNC_LOG_* macros at default gate emit `[LEVEL ] file:line
payload\n` (Sep 3's shape). All four levels are present. All PASS.

### Section 5 — LOC runtime siblings at default gate

LOCSYNC_LOG_*_RUNTIME macros at default gate. Same shape as
Section 3 with the Sep 3 file:line prefix. All PASS.

### Section 6 — if/else hygiene at default gate

Two `if (cond) SYNC_LOG_INFO(...); else SYNC_LOG_INFO(...);`
constructs emit `then-branch` / `else-branch` lines in the
correct branches. The `do-while(0)` wrapping preserves the
binding. Both PASS.

### Section 7 — format-specifier passthrough

`{:#x}` → `0xcafe`; `{:.3f}` → `3.142`; `{:>8}` → 8-char
right-aligned; `{:<6}` → 6-char left-aligned. All four
specifiers round-trip. PASS.

### Section 8 — consteval gate preserved on emit path

The macro layer still routes through `std::format_string<Args...>`
whose ctor is consteval — meaning parse / type-mismatch errors
remain hard compile errors at the call site. PASS.

### Section 9 — sink-swap behaviour

Two rounds of `ref = std::ref(oss); SYNC_LOG_INFO(...);
restore_sink();` correctly route lines to the test
`std::ostringstream` and back to `std::cerr`. PASS.

### Section 10 — helper layer is unconditional

Direct calls to `petra::sync_log_emit(LogLevel::Debug, ...)`
etc. bypass the macro gate and emit at every level
regardless of the gate value. This is the correct property:
programmers who want to bypass the gate (e.g. to always emit
ERROR from a library) call the helper directly. All four levels
emit. PASS.

### Section 11 — full sweep: 16 macros × 4 levels

Exercises every macro at every level (16 calls total, but
each emits one line). Asserts the captured output contains
all 16 expected lines. PASS.

### Section 12 — concurrent atomic emission (THE headline section)

4 threads alternate `SYNC_LOG_INFO` and `SYNC_LOG_WARN`,
250 calls each. Total 1000 commits. The atomic-line property
(Sep 2 / Sep 3) is preserved at default gate. All captured
lines match the regex `^\[(INFO |WARN )\] t[0-3]=\d+$`. PASS.

### Section 13 — gate value at build time

Prints `PETRA_SYNC_LOG_LEVEL` and asserts the value is 0..3.
At default gate the value is 0. PASS.

### Section 14 — macro-stringify probe (THE headline probe)

Prints the actual fully-expanded form of each macro via the
double-indirection stringification trick. At gate=0 all four
macros expand to:

```
do { ::petra::sync_log_emit((::petra::LogLevel::Debug), ); } while (0)
do { ::petra::sync_log_emit((::petra::LogLevel::Info), ); } while (0)
do { ::petra::sync_log_emit((::petra::LogLevel::Warn), ); } while (0)
do { ::petra::sync_log_emit((::petra::LogLevel::Error), ); } while (0)
```

The `static_assert` identity check proves the gate chose the
correct form. PASS.

### Section 15 — gate-smoke via helper layer

Calls `petra::sync_log_emit(LogLevel::Debug/Info/Warn/Error, ...)`
directly to verify the level-tag pipeline at the current gate
(gate-independent because the helper layer doesn't consult the
gate). All four levels emit. PASS.

### Section 16 — ERROR always emits

Independent of the gate value, `SYNC_LOG_ERROR` and
`LOCSYNC_LOG_ERROR` always emit (their level is 3, the
maximum). The captured output contains the literal
`[ERROR] this-must-emit-` and `[ERROR] loc-error-must-emit-`
prefixes. PASS.

---

## Section-by-section findings — gate-verify driver

### Section 1 — emit count matches gate rule

The driver emits 16 macros at every level and captures the
output to a `std::ostringstream`. The captured-line count must
match the gate-emit rule:

```
expected = 16 - 4 * PETRA_SYNC_LOG_LEVEL
```

At gate=0 the expected count is 16 (4 levels × 4 macro
families). At gate=1 the expected count is 12 (drops DEBUG:
16 − 4). At gate=2 the expected count is 8 (drops DEBUG + INFO:
16 − 8). At gate=3 the expected count is 4 (drops DEBUG + INFO +
WARN: 16 − 12). All four gate values produce the expected
count. 1/1 PASS at each gate value.

---

## Where we go next

Today closes the **third and final** new follow-on item Sep 1's
"Where we go next" section named. The Sep 1 → Sep 2 → Sep 3 →
today chain is now closed end-to-end:

- **Sep 2** closed the **first** item (the `std::osyncstream`
  fix for the two-phase-write concurrency gap).
- **Sep 3** closed the **second** item (`__FILE__` / `__LINE__`
  capture in the SYNC_LOG_* macros).
- **today** closes the **third** item (the
  `PETRA_SYNC_LOG_LEVEL` compile-time gate).

The older carry-forward items from the Aug 28 / Aug 30 / Aug 31
chain remain open (still relevant for future lessons):

- **`std::format` to `std::ostream` for type-erased
  `std::span<const std::any>` args** — libc++ 21 has no
  `__format_arg_store` supporting `std::any`.
- **`std::ostream_print` with NUL-terminated output** — the
  Aug 25 NUL-termination contract applied to an ostream sink
  read back via `os.str()`.
- **`std::format_to_n` to an ostream streambuf for
  NUL-terminated output** — today writes to a streambuf via
  `std::osyncstream`, so a single committed line is still the
  contract; a NUL terminator would require a separate piece of
  state.
- **`would_be` on the runtime path of the n-capped sibling** —
  Aug 31's runtime entry point returns `offered` on success
  because `std::vformat_to` does not expose `would_be`. A
  two-pass measure-then-write design would close this.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open: pin actions to commit
SHAs; multi-OS matrix extending to `windows-latest`; status
badge in README; vcpkg / Conan port for `psp_span_lib`;
branch protection requiring linear history.
