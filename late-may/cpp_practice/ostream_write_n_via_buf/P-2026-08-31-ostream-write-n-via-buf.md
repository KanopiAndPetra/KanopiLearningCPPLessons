# P-2026-08-31 — `ostream_write_n_via_buf`: `std::format_to_n` straight to `std::ostreambuf_iterator<char>`

**Topic:** the bounded zero-allocation sibling of the Aug 28
`std::print(std::ostream&, ...)` adapter, returning
`std::expected<std::size_t, FormatError>` from `std::format_to_n`
(consteval) and `std::vformat_to` (runtime) directly through a
`std::ostreambuf_iterator<char>` sink.

**Standard:** C++23 (`std::format_to_n` P2216R3, `std::vformat_to`,
`std::expected` P0323R12, user `std::formatter` specialisation).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++. CMake 4.3.4.

**Result:** 477/477 PASS across 13 sections on six builds — direct
default, direct strict-warning, direct ASan/UBSan, CMake default,
CMake strict, CMake ASan. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`.
Zero sanitizer diagnostics. Five consecutive runs produced identical
tallies.

---

## Why today

The Aug 28 lesson (`P-2026-08-28-ostream-print-cpp23`) shipped
`petra::ostream_print` / `ostream_println` / `ostream_print_via_buf`
— the consumer-side `std::print(std::ostream&, ...)` adapter that
closes the overload gap libc++ 21 leaves open. Its "Where we go next"
section listed four new follow-on items. The Aug 30 lesson
(`P-2026-08-30-ostream-partial-write`) closed the **first** of those
four (partial-write detection on a custom streambuf). The **third**
was:

> **`std::format_to_n` straight to ostream streambuf** — P2216R3
> doesn't ship this overload either; the same `bounded_ostream_writer`
> machinery from Aug 28 could be lifted to `format_to_n`'s runtime
> path. The two-pass measure-then-write design re-uses unchanged.

Today IS that lesson. It does three things:

1. **Builds the wrapper Aug 28 predicted** (`std::format_to_n` straight
   through `std::ostreambuf_iterator<char>`), confirming the predicted
   point that P2216R3 ships `format_to_n` such that `ostreambuf_iterator`
   is a legal Out parameter.
2. **Finds that Aug 28's prediction about the "runtime path" is wrong
   in a structural way** — there is no public runtime-format-string
   `format_to_n` in libc++ 21 (or in P2216R3 / C++23). The standard
   library deliberately SEPARATES the two surfaces: `format_to_n` is
   consteval-only with `std::format_string<Args...>`; the
   runtime-format-string sibling is `std::vformat_to` which has **no n
   parameter**. So today's wrapper has TWO entry points, not one, and
   they take different forms.
3. **Ships the fix** — `petra::ostream_write_n_via_buf` (consteval)
   and `petra::ostream_write_n_via_buf_runtime` (runtime), sharing
   the same `FormatError` type and the same `counting_streambuf`
   filter Aug 30 established.

---

## The headline finding

**libc++ 21 / P2216R3 deliberately SEPARATES the two paths.**

| Form | Symbol | Iterator-side cap | Format-string check |
|------|--------|-------------------|---------------------|
| consteval | `std::format_to_n` | yes (the `n` parameter) | `std::format_string<Args...>` ctor is `consteval` |
| runtime | `std::vformat_to` | **no** | `std::string_view` + `std::format_args` |

The internal `std::__vformat_to_n` exists in libc++ 21 and is reachable
through the `std::format_to_n` implementation, but it is `__`-prefixed
and implementation-private. There is **no public** runtime-format-string
`format_to_n`. The runtime-format-string sibling is `std::vformat_to`,
and it has no `n` parameter — the iterator-side cap is the user's
responsibility.

So today's wrapper has two entry points that take different forms:

```cpp
// CONSTEVAL: one stdlib call — std::format_to_n(It{sb}, n, fmt, args...)
template <typename... Args>
std::expected<std::size_t, FormatError>
ostream_write_n_via_buf(std::ostream& os, std::size_t cap,
                        std::format_string<Args...> fmt, Args&&... args);

// RUNTIME: std::vformat_to through a hand-rolled bounded_ostream_writer
std::expected<std::size_t, FormatError>
ostream_write_n_via_buf_runtime(std::ostream& os, std::size_t cap,
                                std::string_view fmt,
                                std::format_args args);
```

Both share `counting_streambuf` and `FormatError{kind, message,
requested, accepted, dropped()}` with Format and PartialWrite kinds
(Aug 30's shape). Section 6 pins the consteval-vs-runtime split
end-to-end; sections 1–5 pin the byte-level mechanics on the
consteval path; sections 6–9 do the same for the runtime path.

The split is the **exact same split** Aug 28 / Aug 30 established:
a `consteval`-checked template that forwards via
`std::make_format_args` to a runtime entry point that takes
`std::string_view` + `std::format_args`. Today's lesson just
exposes it through two different stdlib surfaces (`format_to_n` and
`vformat_to`) instead of one (`vformat` and `vformat_to`).

---

## The "iterator offered one byte past the sink cap" finding

The first naive mental model of `std::format_to_n(It{sb}, n, ...)`
on a sink that caps below `n` is: "the iterator offers `n` chars, the
sink takes what it can, the iterator's `failed()` flag fires." The
actual behavior on Apple Clang 21.0.0 / libc++ is different and
**worth pinning down**: the iterator does NOT offer all `n` chars
before checking for sink refusal. It stops after `sink_cap + 1`
offers — the (sink_cap+1)th offer is the one that fails.

Section 5 pins this on the four regimes. The headline table (with
`cap = N`, `sink = S`, `would_be = L`):

| N | S | observed offered | observed accepted | iterator.failed() | r.has_value() |
|---|---|---|---|---|---|
| 14 | 16 | 14 | 14 | false | true |
| 14 | 14 | 14 | 14 | false | true |
| 14 | 5  | **6**  | 5  | **true** | false |
| 5  | 16 | 5 | 5 | false | true |
| 5  | 5  | 5 | 5 | false | true |
| 5  | 3  | **4**  | 3  | **true** | false |
| 1024 | 0 | **1** | 0 | **true** | false |
| 0 | 16 | 0 | 0 | false | true |

The general rule the table pins: on a sink-refusal path the iterator
offers `min(n, S+1)` chars and the sink takes `min(S, offered)`. On
a success path the iterator offers `min(n, L)` chars and the sink
takes all of them. The success condition is
`min(n, L) <= S` (or equivalently "no sink refusal ever occurs").

This is the same copy-semantics trap the Aug 25, Aug 26, Aug 28
lessons each hit: the iterator's `failed()` flag is observed on the
**returned** iterator, not the one you pass in. But the
`+1`-on-failure is new behavior — the iterator tries one byte past
the sink cap to confirm the refusal, then stops. Section 5's row
`{14, 5}` shows this: 14 chars are `would_be`, but the iterator
stops after offering 6.

---

## The `would_be` semantic split between paths

A subtle difference the lesson found between the two entry points:

| Path | On success, returns | Why |
|------|---------------------|-----|
| `ostream_write_n_via_buf` (consteval) | `would_be` (the formatter's full intent) | `std::format_to_n` returns `format_to_n_result{out, size}` where `size` is `would_be` |
| `ostream_write_n_via_buf_runtime` (runtime) | `offered` (the iterator's actual writes) | `std::vformat_to` has no return value |

So on the **iterator-capped** success path the two paths disagree:
the consteval path says "the formatter wanted 14, you capped at 5,
here's 14", while the runtime path says "the iterator wrote 5,
here's 5". The reason is structural — the runtime path's source
information (`std::vformat_to`'s return) doesn't carry `would_be`,
only the iterator's last position. Section 6's second test pins this
contrast; section 12 (drop-in equivalence with Aug 28's
`ostream_write_n`) verifies the consteval path matches Aug 28's
report, where the runtime path by definition can't.

The fix would be a second-pass `std::vformat_to` through a
`counter_output_iterator` to measure `would_be` the same way
Aug 24's lesson did. Today's wrapper leaves that as a follow-on
(the trade-off is one extra full-pass over the format string per
call, which is the exact thing this lesson was trying to avoid).
Section 4 makes the contrast explicit by showing that
`would_be == filter.offered == filter.accepted` on a healthy sink
with `cap == would_be`, so on the COMMON path the two surfaces
agree; only the iterator-capped path can show a divergence.

---

## The `counting_streambuf` filter (lifted from Aug 30)

Neither `std::format_to_n` nor `std::vformat_to` reports a byte
count on the success path. The durable mechanism Aug 30's lesson
established — splice a `counting_streambuf` between the ostream
and its real buffer, observe offered/accepted — lifts into this
TU unchanged. The filter forwards verbatim, so
`std::ostreambuf_iterator`'s `failed()` flag still works the way
the standard library intends; the wrapper's PartialWrite branch
trips when `accepted < offered`, which is the standard definition
of a partial write.

The Aug 30 `counting_streambuf` has the `rdbuf(sb*)` clear-clobber
trap baked in (via `scoped_counting_filter`'s RAII guard). It
fires again here in section 11's exception-mask test, and the
guard handles it. The same hidden-name trap
(`std::ostringstream::rdbuf()` hides the 1-arg overload) is
worked around by the guard's `std::ostream&` constructor signature.

---

## The runtime path's `bounded_ostream_writer`

The runtime entry point needs to cap the iterator because
`std::vformat_to` does not. Aug 28 built this kind of iterator
for its own `ostream_write_n`; today lifts the same shape over.
The trap (and the one the Aug 25, Aug 26, and Aug 28 lessons all
hit): `std::vformat_to` copies the iterator on every advance, so
the counter MUST live externally. `bounded_ostream_writer` carries
a `std::size_t*`, and the caller initialises a local
`std::size_t written = 0;` on the stack. The first design without
the pointer (storing the counter as a member) silently lost every
increment after the first copy; section 6's second probe
(`"[7|tr"` == 5 chars) catches the failure mode via the output.

---

## What `counting_streambuf` reports in the four regimes

`std::format_to_n`'s success/failure report does not decompose into
"what the iterator capped" and "what the sink refused" — it folds
both into `failed()`. The filter splits them:

| regime | offered | accepted | iterator.failed() |
|--------|---------|----------|-------------------|
| `n >= L, S >= L` (full fit) | `L` | `L` | false |
| `n < L, S >= n` (iterator caps) | `n` | `n` | false |
| `n >= L, S < L` (sink caps) | `S+1` | `S` | **true** |
| `n < L, S < n` (sink caps, n not binding) | `S+1` | `S` | **true** |
| `n == 0` (iterator offers nothing) | 0 | 0 | false |

The success criterion the wrapper uses is
`iterator_failed == false`, which is the same condition the
standard library defines for `format_to_n`'s own return. Section 4
shows that on the success path `r.value() == would_be` (always) and
`guard.offered() == would_be` (when `S >= L`). The failure path's
`error.requested == guard.offered() == min(n, S+1)` — section 5's
table is the spec.

---

## Other things the sections pin

- **Section 3 — `format_to_n` accepts `ostreambuf_iterator<char>`
  directly.** No adapter needed. The probe-style tests run
  `std::format_to_n(It{oss.rdbuf()}, ...)` with caps 32, 14, 5,
  and 0; each round trips the iterator's `failed()` flag exactly
  when `min(n, L) > sink_cap`, and `n` (the `would_be` returned)
  is always `L`.

- **Section 6 — the consteval-vs-runtime surface.** Both paths
  produce the same byte-identical output on a healthy sink with
  `cap >= would_be`; the difference is the return value when the
  iterator caps. The runtime path can't report `would_be` because
  `std::vformat_to` doesn't expose it; the consteval path can
  because `std::format_to_n` does.

- **Section 7 — `format_error` atomicity.** Both paths throw
  `std::format_error` before any byte moves; the filter's
  `offered == accepted == 0` is verified up to a 20000-char prefix
  on the runtime path, exactly like Aug 30's section 8 did for
  `vformat_to`. The consteval path's compile-time gate catches
  most parse errors (`{:d}` on a string is a hard compile error);
  type-mismatches that sail through consteval still throw
  atomically at format time.

- **Section 10 — UTF-8 mid-codepoint split.** With `"ok 🚀!"`
  (8 bytes) and a sink cap of 4, the iterator stops mid-emoji and
  the sink ends up holding `6f 6b 20 f0` — a lead byte `0xF0`
  announcing four bytes, of which only one arrived. **Invalid UTF-8
  in the sink.** The adapter reports the byte split; making the
  split codepoint-safe is a sink responsibility (same finding the
  Aug 30 lesson pinned for the unbounded sibling). Embedded NUL
  passes through the runtime path unchanged.

- **Section 11 — exception mask bypassed.** The iterator path
  bypasses `exceptions(badbit)` entirely — no throw on truncation,
  same finding Aug 30 pinned. The wrapper reports the failure
  either way.

- **Section 12 — drop-in equivalence with Aug 28's
  `ostream_write_n`.** On the success path the consteval wrapper
  produces byte-identical output and reports the same size as
  Aug 28's `vformat -> string -> os.write` adapter. The Aug 28
  consumer can switch to today's wrapper by renaming the call
  (the runtime path requires a different signature — that
  substitution needs the `make_format_args` rewriting Aug 28's
  notes describe).

- **Section 13 — 100-round determinism stress.** The consteval
  path runs 50 rounds (cap = `round % 25`, sinkc = `(round * 7) % 25`,
  text = `std::format("r{} payload", round)`); the runtime path
  runs another 50 rounds (sinkc = `(round * 11) % 25`, text =
  `std::format("R{} hello", round)`). Each round predicts
  success/failure from `predict_success(n, sinkc, would_be)`
  (the rule pinned in the headline table) and asserts the verdict
  plus that whatever landed is a prefix of the full text.

---

## Observed output

```
-- section 1.toolchain-probes
-- section 2.happy-path
-- section 3.format-to-n-with-ostreambuf-iterator
-- section 4.would-be-vs-filter
-- section 5.cap-independence
-- section 6.runtime-format-string
-- section 7.format-error-atomicity
-- section 8.error-formatter
-- section 9.ofstream-sink
-- section 10.utf8-and-nul
-- section 11.exception-mask
-- section 12.drop-in-equivalence
-- section 13.determinism-stress

477/477 PASS
```

Identical on all six builds; five consecutive runs on each build
produced identical tallies.

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -stdlib=libc++ \
    P-2026-08-31-ostream-write-n-via-buf.cpp \
    -o P-2026-08-31-ostream-write-n-via-buf \
    && ./P-2026-08-31-ostream-write-n-via-buf

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -stdlib=libc++ \
        P-2026-08-31-ostream-write-n-via-buf.cpp \
        -o P-2026-08-31-ostream-write-n-via-buf-strict \
    && ./P-2026-08-31-ostream-write-n-via-buf-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -stdlib=libc++ \
        P-2026-08-31-ostream-write-n-via-buf.cpp \
        -o P-2026-08-31-ostream-write-n-via-buf-asan \
    && ASAN_OPTIONS=detect_leaks=0 ./P-2026-08-31-ostream-write-n-via-buf-asan

# CMake: default / strict / ASan
cmake -S . -B build                && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
```

### A note on LeakSanitizer

As on Aug 30, LeakSanitizer is **not supported on darwin/arm64**
(probed: `"detect_leaks is not supported on this platform"`).
`ASAN_OPTIONS=detect_leaks=0` was used for the ASan run rather than
the documented `detect_leaks=1`, which aborts rather than enabling
it on this platform. Every format argument in the lesson is a named
local so the absence of LeakSanitizer does not silently hide leaks.

---

## Where we go next

Today closes the **third of the four** new follow-on items from Aug
28's "Where we go next". The remaining three from that list stay
open:

- **`std::ostream_print` with NUL-terminated output** — the Aug 25
  NUL-termination contract applied to an ostream sink read back via
  `os.str()`.
- **`std::format_to_n` to an ostream streambuf for NUL-terminated
  output** — today writes to a streambuf, not a raw buffer, so no
  NUL is synthesised. A custom ostream whose payload is read out
  via `os.str()` could expect one.
- **`std::print`-compatible log macros** — `LOG_INFO("value={}", 42)`
  sugar over the adapter.

Plus the older item Aug 22 and Aug 28 carried forward:

- **`std::format` to `std::ostream` for type-erased
  `std::span<const std::any>` args** — libc++ 21 has no
  `__format_arg_store` supporting `std::any`.

New items surfaced by today's lesson:

- **`would_be` on the runtime path.** Today's runtime entry point
  returns `offered` on success because `std::vformat_to` does not
  expose `would_be`. A two-pass shape (measure via
  `counter_output_iterator`, then write via
  `bounded_ostream_writer`) would give the runtime path the same
  `would_be` report the consteval path returns, at the cost of one
  full pass over the format string per call. The trade-off is the
  exact one Aug 24's lesson spelled out for the bounded-buffer
  family. Today leaves it as a follow-on.

- **`__vformat_to_n` as a non-portable escape hatch.** The libc++
  21 internal `std::__vformat_to_n` does support runtime format
  strings with `n`; calling it directly would let a single
  template dispatch to one path. But it's `__`-prefixed and
  implementation-private; relying on it is the same shape of
  portability hole Aug 26's note about `runtime_format` under
  `_LIBCPP_STD_VER >= 26` warns against. Today's wrapper keeps the
  surface public-stable.

- **The `would_be` divergence between paths.** Section 6's second
  test pins that the consteval and runtime paths disagree on
  `r.value()` when the iterator caps. A user who wants both
  behaviours (consteval speed + runtime flexibility + consistent
  return semantics) could compose today's wrapper with a
  per-call-format-string feature test (`if constexpr (consteval)
  ... else ...`). Out of scope for a focused lesson.

- **The `+1` sink-cap surprise.** The iterator tries one byte
  past the sink cap before reporting `failed()`. Section 5's table
  is the empirical spec; a portable wrapper that wants
  **strict** equality between `min(n, L)` and the bytes that
  landed would need a smarter sink interface (or a counted
  fallback). Today's wrapper reports the stdlib's iterator
  behaviour verbatim and surfaces the `accepted < offered`
  partial-write; a sink that wants byte-exact cap on a
  truncated-write path needs to wire its own cap into the
  formatter-side offer.

- **The consteval gate rejects runtime format strings in a
  surprising direction.** `std::format_string<Args...>`'s consteval
  ctor does NOT reject all bad format strings — it rejects
  argument-index / parse / `{:d}`-on-string / unmatched-brace at
  compile time, but it accepts format strings that throw at
  format time on a per-arg-type mismatch. The wrapper catches the
  latter through `try`/`catch std::format_error`; section 7 pins
  this end-to-end.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open: pin actions to commit SHAs; multi-OS
matrix extending to `windows-latest`; status badge in README;
vcpkg / Conan port for `psp_span_lib`; branch protection requiring
the matrix to pass.
