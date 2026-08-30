# P-2026-08-30 — `ostream_write_checked`: partial-write detection for the ostream format adapter

**Topic:** turning silent truncation into a typed, counted
`std::expected<std::size_t, WriteError>` failure when the *sink*
refuses bytes.

**Standard:** C++23 (`std::vformat` / `std::vformat_to` P2216R3,
`std::expected` P0323R12, user `std::formatter` specialisation).

**Toolchain:** Apple Clang 21.0.0 (`clang-2100.1.1.101`),
arm64-apple-darwin25.6.0, libc++. CMake 4.3.4.

**Result:** 356/356 PASS across 16 sections on six builds — direct
default, direct strict-warning, direct ASan/UBSan, CMake default,
CMake strict, CMake ASan. Zero warnings under
`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`.
Zero sanitizer diagnostics.

---

## Why today

The Aug 28 lesson (`P-2026-08-28-ostream-print-cpp23`) shipped
`petra::ostream_print` / `ostream_println` / `ostream_print_via_buf`
— the consumer-side `std::print(std::ostream&, ...)` adapter that
closes the overload gap libc++ 21 leaves open. Its "Where we go next"
section listed four new follow-on items. The **first** was:

> **`std::ostream_print` for a custom `std::streambuf` that fails to
> consume all chars** — some streambufs (e.g. compression streams,
> network sinks) signal "downstream full" by failing to consume;
> today's `ostreambuf_iterator` silently reports end-of-stream and the
> adapter treats this as a successful truncating write. A future lesson
> could pin this case and emit a `partial_write` error kind on
> truncation.

Today is that lesson. It does three things:

1. **Builds the sink Aug 28 only hypothesised** (`capped_streambuf`)
   and proves the predicted silent-truncation bug is real.
2. **Finds that the bug is worse than predicted** — the two adapter
   paths fail in *different and mutually inconsistent* ways, and
   neither can report a byte count.
3. **Ships the fix**: `petra::ostream_write_checked` returning
   `std::expected<std::size_t, WriteError>` with a `PartialWrite`
   kind carrying the requested/accepted split.

---

## The headline finding

The two adapter paths Aug 28 shipped fail **differently**, and neither
failure mode is the one you would guess:

| signal after a short write | `vformat`→`string`→`os.write` | `vformat_to`→`ostreambuf_iterator` |
|---|---|---|
| `os.bad()` | `true` | **`false`** |
| `os.fail()` | `true` | **`false`** |
| returned iterator `.failed()` | n/a | `true` |
| *your* iterator `.failed()` | n/a | **`false`** (it was copied) |
| honours `exceptions(badbit)` | yes | **no** |
| refuses to write to a `bad()` stream | yes | **no** |
| byte count available | **no** | **no** |

`std::ostreambuf_iterator` writes through the streambuf **directly**
(`sb->sputc`), never touching `basic_ios::rdstate`. It is not part of
the stream's error-reporting machinery at all — it is a thin adapter
over the buffer that happens to be reachable via `os.rdbuf()`. So the
Aug 28 streambuf path drops bytes with the stream still reporting
`good()`. That is exactly the silent truncation Aug 28 predicted,
confirmed in **section 4**:

```cpp
petra::capped_streambuf sink{5};      // accepts 5 bytes, then refuses
std::ostream os{&sink};
petra::ostream_print_via_buf_runtime(os, "hello {}", make_format_args(n));

CHECK_EQ(sink.str(), "hello");   // 3 of 8 bytes silently lost
CHECK(!os.bad());                //  *** stream is CLEAN ***
CHECK(!os.fail());
CHECK(os.good());
```

The `os.write` path at least *signals* the problem
([ostream.unformatted]/3 requires `setstate(badbit)` when `sputn`
returns short), which **section 3** pins. But it still cannot say how
many bytes made it.

---

## The `failed()` flag is on the *returned* iterator

`std::vformat_to` takes the output iterator **by value** and copies it
on every advance. The `failed()` flag therefore propagates out only
through the return value. Your original copy never learns anything —
**section 5**:

```cpp
It mine{os.rdbuf()};
const It returned = std::vformat_to(It{mine}, "hello {}", make_format_args(n));

CHECK(returned.failed());   // the truncation signal
CHECK(!mine.failed());      //  *** our copy never learns ***
```

This is the **same copy-semantics trap** the Aug 25, Aug 26 and Aug 28
lessons each hit with their own hand-written bounded output iterators
(Aug 26's notes: *"std::vformat_to copies the iterator on every advance
so the counter MUST live externally"*). Here it appears in the standard
library's own iterator, which makes it easier to trip over: on a
healthy sink both copies report `false`, so the difference is invisible
in a passing test.

---

## The mechanism: a counting streambuf filter

Neither path reports a byte count. Rather than bolt a second
mechanism onto each path, the lesson settles on one that works for
**both**, and for any sink: splice a filter between the ostream and its
real buffer.

```cpp
class counting_streambuf : public std::streambuf {
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);
        ++offered_;
        const int_type r = down_->sputc(traits_type::to_char_type(ch));
        if (!traits_type::eq_int_type(r, traits_type::eof())) ++accepted_;
        return r;                       // forwarded verbatim
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        offered_ += static_cast<std::size_t>(n);
        const std::streamsize wrote = down_->sputn(s, n);
        accepted_ += static_cast<std::size_t>(wrote);
        return wrote;                   // forwarded verbatim
    }
};
```

`offered > accepted` **is** the definition of a partial write. The
filter observes; it never changes behaviour — return values are
forwarded verbatim, so `os.write` still sets `badbit` and the iterator
still sets `failed()`. **Section 2** verifies the filter is transparent
on a healthy sink (`offered == accepted`, output byte-identical).

---

## The `rdbuf(sb*)` state-clobber trap

Splicing the filter in and out is where the lesson found its second
non-obvious problem. `basic_ios::rdbuf(streambuf*)` **calls `clear()`**
([ios.members]/6). So a naive scoped filter:

- **erases** whatever error state the stream already carried, on
  install; and
- **erases the `badbit` the short write just set**, on restore.

Both silently. **Section 11** demonstrates the raw behaviour:

```
[r] before rdbuf swap: bad=1
[r] after  rdbuf swap: bad=0   <-- install cleared it
[r] after short write: bad=1
[r] after  restore:    bad=0   <-- restore cleared it again
```

The guard closes the hole by saving `rdstate()` **before** the
installing `rdbuf()` call, capturing the state produced during the
splice **before** the restoring call, and re-applying the union:

```cpp
~scoped_counting_filter() {
    const std::ios_base::iostate produced = os_.rdstate();
    (void)os_.rdbuf(previous_);        // this calls clear()
    os_.setstate(saved_state_ | produced);
}
```

### A name-hiding gotcha worth pinning once

`std::ostringstream::rdbuf()` and `std::ofstream::rdbuf()` are
**zero-argument** member functions that **hide** the inherited
one-argument `basic_ios::rdbuf(std::streambuf*)`. Writing
`oss.rdbuf(&filter)` is a hard compile error on libc++ 21:

```
error: too many arguments to function call, expected 0, have 1
note: 'rdbuf' declared here
    basic_stringbuf<char_type, traits_type, allocator_type>* rdbuf() const
```

The fix is to go through a base reference (`std::ios&` or
`std::ostream&`). `scoped_counting_filter`'s constructor takes
`std::ostream&`, so the conversion happens at the call site and callers
never meet this.

---

## The error type

```cpp
enum class WriteErrorKind { Format, PartialWrite };

struct WriteError {
    WriteErrorKind kind{};
    std::string    message{};
    std::size_t    requested = 0;   // bytes the text offered
    std::size_t    accepted  = 0;   // bytes the sink took
    std::size_t dropped() const noexcept;
};
```

This follows the **Aug 27 `LogError` shape** — project the library's
`std::format_error` into a domain error type rather than leaking it —
for the same reason: the caller wants to branch on a kind, not parse a
message. As on Aug 27, libc++ 21 has no formatter for arbitrary user
types, so a `std::formatter<petra::WriteError>` specialisation is
written so the `CHECK` macros can render one:

```
WriteError{PartialWrite, requested=8, accepted=5, dropped=3, msg="sink refused bytes"}
```

---

## The `requested` asymmetry — deliberate, not a bug

The two checked adapters report **different `requested` values** for
the same truncation, and **section 8** asserts both:

| path | `requested` | `dropped()` | what it means |
|---|---|---|---|
| `ostream_write_checked` | 8 | 3 | formatted-text size — **true loss** |
| `ostream_write_checked_via_buf` | 6 | 1 | bytes *offered to the stream* — bytes refused |

The write path builds the whole `std::string` first, so it knows the
full size. The streambuf path formats directly into the sink and the
formatter **stops offering** after the first refusal, so it never
learns how much more there would have been. Both numbers are honest
reports of what each path can actually observe. A caller who needs true
loss must use the write path (or format twice). That is the real cost
of the zero-allocation path, and it is the kind of trade that only
shows up once you build the failing sink.

---

## Is the `Format` path atomic?

A fair question, since the streambuf path formats *directly into the
sink*: could a long literal prefix land before a bad replacement field
throws? **Section 10** sweeps prefix lengths 8 / 100 / 300 / 1000 /
5000 / 20000 through `"{}{:d}"` with a `std::string` bound to `{:d}`:

```
plen=8      threw; partial bytes leaked = 0  (atomic)
plen=100    threw; partial bytes leaked = 0  (atomic)
plen=300    threw; partial bytes leaked = 0  (atomic)
plen=1000   threw; partial bytes leaked = 0  (atomic)
plen=5000   threw; partial bytes leaked = 0  (atomic)
plen=20000  threw; partial bytes leaked = 0  (atomic)
```

libc++ 21 buffers internally and validates the whole spec before
emitting. The write path is atomic **by construction** (the format
completes into a `std::string` before any byte moves). The section
pins this as an **observation about libc++ 21, not a guarantee** —
`[format.err]` promises nothing about how much output reaches the
iterator before the throw, so portable code must not rely on it.

---

## Other things the sections pin

- **Section 6 — the exact-fit boundary.** Sweeping the cap across the
  8-byte payload shows `failed()` flips exactly at `cap < len`, not at
  `cap <= len`; an exact fit is a success and the iterator does not
  over-offer a trailing byte.
- **Section 12 — `exceptions(badbit)`.** The write path honours the
  mask and throws `std::ios_base::failure` on truncation. The
  iterator path **bypasses the mask entirely** — no throw, no state.
  Same asymmetry as section 4, seen from the other side. The checked
  adapter reports the failure either way.
- **Section 13 — the iterator ignores stream state in both
  directions.** `os.write` on a stream already in `badbit` writes
  nothing (the sentry refuses). The iterator writes **3 bytes through
  a bad stream**.
- **Section 14 — UTF-8 splits mid-codepoint.** Byte-oriented
  truncation is not codepoint-aware. With `"ok 🚀!"` (8 bytes) and a
  cap of 5, the sink ends up holding `6f 6b 20 f0 9f` — a lead byte
  `0xF0` announcing four bytes, of which only two arrived. **Invalid
  UTF-8 in the sink.** The adapter reports the byte split; making the
  split codepoint-safe is a sink responsibility. Embedded NUL bytes,
  by contrast, pass through the pipeline unharmed.
- **Section 15 — a real `std::ofstream` sink.** The filter is
  sink-agnostic; splicing it over a `filebuf` works unchanged and the
  24 bytes on disk match the accepted counts.
- **Section 16 — 50-round determinism stress**, alternating both paths
  across cap sweeps 0–24, asserting the verdict against an
  independently computed expectation, plus that whatever landed is a
  **prefix** of the full text.

---

## Observed output

```
-- section 1.toolchain-probes
-- section 2.healthy-sink-baseline
-- section 3.capped-write-path-badbit
-- section 4.capped-streambuf-path-silent
-- section 5.failed-on-returned-iterator
-- section 6.exact-fit-boundary
-- section 7.checked-success
-- section 8.checked-partial-write
-- section 9.checked-format-error
-- section 10.format-error-atomicity
-- section 11.rdbuf-clobbers-rdstate
-- section 12.exception-mask
-- section 13.iterator-ignores-state
-- section 14.utf8-mid-codepoint
-- section 15.ofstream-sink
-- section 16.determinism-stress

356/356 PASS
```

Identical on all six builds; five consecutive runs produced identical
tallies.

---

## Build and verification commands

```bash
# default
clang++ -std=c++23 -O0 -o pw P-2026-08-30-ostream-partial-write.cpp && ./pw

# strict warnings — zero warnings, zero errors
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -O0 -o pw-strict P-2026-08-30-ostream-partial-write.cpp && ./pw-strict

# ASan + UBSan — clean, empty diagnostics
clang++ -std=c++23 -fsanitize=address -fsanitize=undefined \
        -fno-omit-frame-pointer -O1 -o pw-asan \
        P-2026-08-30-ostream-partial-write.cpp && ./pw-asan

# CMake: default / strict / ASan
cmake -S . -B build              && cmake --build build
cmake -S . -B build-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion" && cmake --build build-strict
cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan
```

### A note on LeakSanitizer

The first draft used `*(new int{42})` to obtain lvalues for
`std::make_format_args` (which binds its arguments by reference and
rejects rvalues). ASan reported **nothing** — LeakSanitizer is **not
supported on darwin/arm64**:

```
==30908==AddressSanitizer: detect_leaks is not supported on this platform.
```

`ASAN_OPTIONS=detect_leaks=1` aborts rather than enabling it. Eight
deliberate leaks therefore sailed through a "clean" sanitizer run. They
were found by reading, not by tooling, and every format argument is now
a named local. **Worth remembering: a clean ASan run on this platform
says nothing about leaks.**

---

## Where we go next

Today closes the **first of the four** new follow-on items from Aug
28's "Where we go next". The remaining three from that list stay open:

- **`std::ostream_print` with NUL-terminated output** — the Aug 25
  NUL-termination contract applied to an ostream sink read back via
  `os.str()`.
- **`std::format_to_n` straight to an ostream streambuf** — P2216R3
  ships no such overload; the `bounded_ostream_writer` machinery from
  Aug 28 lifts over unchanged.
- **`std::print`-compatible log macros** — `LOG_INFO("value={}", 42)`
  sugar over the adapter.

Plus the older item both Aug 22 and Aug 28 carried forward:

- **`std::format` to `std::ostream` for type-erased
  `std::span<const std::any>` args** — libc++ 21 has no
  `__format_arg_store` supporting `std::any`.

New items surfaced by today's lesson:

- **A codepoint-safe truncating sink.** Section 14 leaves invalid UTF-8
  in the buffer when the cap lands mid-sequence. A `utf8_capped_
  streambuf` that refuses a whole codepoint rather than splitting it
  would need a small incremental UTF-8 decoder in `overflow()`, and
  raises a real design question: does it refuse the codepoint, or
  substitute U+FFFD?
- **Retry / backpressure on `PartialWrite`.** The natural next step for
  a non-blocking socket sink: on `PartialWrite`, keep the unwritten
  tail and re-offer it when the sink drains. Requires the adapter to
  return the tail — an `expected<size_t, WriteError>` where `WriteError`
  carries a `std::string remaining`, or a resumable writer object.
  This is where the `requested` asymmetry above starts to bite: the
  streambuf path does not know the tail.
- **Does the filter change what the formatter offers?** Section 2 shows
  the filter is output-transparent, but `counting_streambuf` has no
  put-area, so every `overflow()` is one byte, whereas the downstream
  buffer might have accepted a bulk `xsputn`. Measuring the throughput
  cost of the filter (and giving it a put-area with `setp`) is a
  focused follow-on.
- **`std::ostream::flush()` and `sync()` failure.** Today's filter
  forwards `sync()` but no section exercises a sink that fails to
  sync — another way a write can be lost after the adapter has already
  reported success.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 / Aug 17
lessons remain open: pin actions to commit SHAs; multi-OS matrix
extending to `windows-latest`; status badge in README; vcpkg / Conan
port for `psp_span_lib`; branch protection requiring the matrix to
pass; and the `v0.16.0` promotion.
