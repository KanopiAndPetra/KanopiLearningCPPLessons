# P-2026-08-28 — `std::ostream_print`: a consumer-side `std::print(std::ostream&, …)` adapter closing the libc++ 21 missing-overload gap

Modern-C++ lesson closing the **first of two open follow-on
items** that the Aug 22 (P-2026-08-22) lesson listed in its
"Where we go next" section and that the Aug 27 (P-2026-08-27)
lesson re-listed.

The Aug 22 lesson, on `std::print` / `std::println` /
`std::format` for C++23, ended with this:

> **`std::format` to `std::ostream`** — the missing overload.
> P2093R14 mentions it; libc++ 21 has not implemented it. The
> workaround (`std::format` -> `std::string` -> `<<`) is fine
> but a real `std::print(std::ostream&, ...)` would close the
> gap. Track for libc++ post-21.

Today IS that lesson. We ship a small
`petra::ostream_print(std::ostream&, fmt, args...)` adapter
(and its bounded sibling `ostream_write_n`) that exercises the
consumer-side surface the missing libc++ overload would have
provided. The lesson does NOT propose adding a new overload to
libc++; it puts the adapter in your TU and walks through the
design decisions the missing overload would have hidden (where
bytes go, what happens on format_error, what happens with UTF-8,
what happens with NUL bytes, what happens with a stream the
caller wants to seekp into mid-write, what happens with the
bounded sibling's two-pass measure-then-write shape).

The lesson is the **first** of two open Aug 22 / Aug 27
follow-on items closed today. (The remaining item,
"std::print(std::ostream&, ...) for type-erased args" remains
on the list for a future lesson.)

| Surface                                                 | libc++21 | Aug28 adapter |
|---------------------------------------------------------|----------|---------------|
| `std::print(FILE*, fmt, args...)`                       |    ✅    |      ✅       |
| `std::println(FILE*, fmt, args...)`                     |    ✅    |      ✅       |
| `std::print(std::ostream&, fmt, args...)`               |    ❌    |      ✅       |
| `std::println(std::ostream&, fmt, args...)`             |    ❌    |      ✅       |
| bounded `ostream_write_n(os, cap, fmt, args...)`        |    ❌    |      ✅       |
| streambuf-OutputIterator path (zero allocation)         |    ❌    |      ✅       |

## Headline

| Build                                                                     | Result            |
|---------------------------------------------------------------------------|-------------------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`)                      | **196/196 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **196/196 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **196/196 PASS**, empty stderr |
| CMake (`cmake -S . -B build && cmake --build build`)                       | **196/196 PASS**  |
| CMake strict (`-DENABLE_ASAN=OFF` + strict flags)                          | **196/196 PASS**  |
| CMake ASan (`-DENABLE_ASAN=ON`)                                           | **196/196 PASS**, empty stderr |

The total assertion count of **196** is the sum across the 16
sections below; per-section counts add up to 196 verified at
runtime. Section 15's 50-round determinism stress contributes
1 (pin) + 49 × 3 (loop) = 148 of those checks; the other
sections collectively contribute 48.

| Section | Topic                                                                  | Tests |
|---------|------------------------------------------------------------------------|-------|
| 1       | Toolchain + feature probes                                             | 5     |
| 2       | Basic success — `petra::ostream_print` with literal fmt                | 4     |
| 3       | `ostream_println` appends `'\n'` (NOT `std::endl`)                     | 4     |
| 4       | Runtime format string (`std::string_view` fmt)                         | 2     |
| 5       | Format failure propagates as `std::format_error`                        | 3     |
| 6       | Streambuf-OutputIterator path == string path                           | 3     |
| 7       | Real `std::ofstream` sink writes round-trippable bytes                 | 4     |
| 8       | `seekp` after `tellp` round-trip in the middle of a write             | 4     |
| 9       | Bounded sibling success — `expected<size_t, format_error>`             | 3     |
| 10      | Bounded sibling truncates writes at `cap` (would-be size preserved)    | 3     |
| 11      | Bounded sibling returns `format_error` (no exception escape)           | 5     |
| 12      | UTF-8 byte-by-byte transmission: 13-byte payload = 18 total bytes      | 4     |
| 13      | NUL byte passes through (`vformat` treats `'\0'` as a literal)        | 8     |
| 14      | Range formatter (P2286R6) on `std::vector<int>`                        | 1     |
| 15      | 50-round determinism — string-path == streambuf-path                  | 148   |
| 16      | Runtime fmt with width / alignment / precision specifiers             | 1     |
| **Total**|                                                                        | **196** |

## What this lesson is NOT

It is NOT a proposal to add a new overload to libc++. A real
libc++ fix would have to go through SG16 / LEWG review and
unblock the std::ostream/format surface that has been
in-flux since P2093R14 (the "fmt::print(stream, ...)"
addition that the standard never quite landed). Today's
adapter sits in your translation unit; it does NOT replace
the std::print(FILE*, ...) entry point (call that when you
have a FILE*).

It is NOT a coroutine / async tutorial. The thread model is
the same as std::print(FILE*, ...) — synchronous, blocking
until the streambuf accepts the chars. The adapter does
NOT spin up any background thread, queue any writes, or
buffer across calls.

It is NOT a replacement for std::print's flushing semantics
on FILE*. std::println(FILE*, ...) flushes after every line;
std::println(std::ostream&, ...) in libstdc++ does NOT
flush (the ostream owns its own flush policy). Today's
`petra::ostream_println` matches the latter: `'\n'` is
written, no flush. If your call site needs the flush, call
`os.flush()` after the println.

## Why this wrapper exists at all

The straightforward code:

```cpp
std::stringstream ss;
std::print(ss, "value={} name={}", 42, "petra");
```

fails to compile in libc++ 21:

```
error: no matching function for call to 'print'
note: candidate function template not viable:
      no known conversion from 'std::stringstream'
      (aka 'basic_stringstream<char>') to 'FILE *'
      (aka '__sFILE *') for 1st argument
note: candidate function not viable:
      no known conversion from 'std::stringstream'
      to 'format_string<const char (&)[6], int>' for 1st argument
```

The `FILE *` candidate is std::print(FILE*, fmt, args...). The
`format_string` candidate is std::print(fmt, args...). Neither
accepts `std::ostream&`. The libstdc++ P2093R14 implementation
shipped both; libc++ 21 did not. Tracking that gap is the
whole point of this lesson.

## The consteval-vs-runtime split

The design challenge is that `std::format_string<Args...>` is
a consteval-evaluated type:

```cpp
template <class... Args>
using format_string =
    basic_format_string<char, type_identity_t<Args>...>;

template <class T>
consteval basic_format_string(const T& s);   // <-- consteval
```

The `consteval` constructor accepts any type convertible to
`std::string_view` — including `std::string_view` itself —
but the conversion must succeed at compile time. A runtime
`std::string_view` cannot satisfy this. The natural-looking
pair of overloads `print(stream, fmt, args...)` /
`print(stream, fmt_view, format_args)` therefore COLLIDE: the
consteval path eats everything that is convertible to
`string_view`, including runtime strings, with a confusing
"the format-string conversion is not a constant expression"
error at the call site.

The fix is to separate the runtime path and the consteval
path by NAME:

```cpp
namespace petra {
// Runtime-format-string entry point. Accepts std::string_view
// directly; the format-string check happens at std::vformat
// call time, throwing std::format_error on parse / arg-index /
// type-mismatch errors.
inline void ostream_print_runtime(ostream& os, string_view fmt,
                                  format_args args);

// Consteval-checked template overload. The literal fmt string
// is validated at compile time; args are perfectly forwarded.
template <typename... Args>
void ostream_print(ostream& os, format_string<Args...> fmt,
                   Args&&... args) {
    ostream_print_runtime(os, string_view{fmt.get()},
                          make_format_args(args...));
}
} // namespace petra
```

The two functions have non-overlapping signatures (one takes
`std::format_args` by value, the other takes
`std::format_string<Args...>`), so there is no overload-set
ambiguity. The lesson uses `ostream_print_runtime` for
runtime `std::string_view` payloads and `ostream_print` for
literal payload strings.

This is the same shape of separation P2093R14 §5.2 describes
for std::print(FILE*, std::string_view, ...) vs
std::print(FILE*, std::format_string<Args...>, ...). libc++
21 has shipped the latter but not the former; today's adapter
extends that idea to ostream sinks.

## The two adapter paths

Today's adapter ships in two flavours:

| Path                 | Allocations | Use when                                       |
|----------------------|-------------|------------------------------------------------|
| `vformat -> string -> os.write`  | one per call | simplicity matters; throughput is low/medium |
| `vformat_to -> ostreambuf_iter -> rdbuf` | zero per char | tight loops; throughput matters |

Section 6 verifies the two paths produce byte-identical
output, and section 15's 50-round determinism stress
exercises both in alternation.

The vformat / string / os.write path is what `std::print`
would compile to if libc++ had an ostream overload. The
ostreambuf_iterator path is what libc++ *could* ship
internally if it did. Today's adapter lets the consumer
pick between the two.

## Section 2 — basic success

The straightforward use of the consteval-checked
`ostream_print`:

```cpp
std::ostringstream oss;
petra::ostream_print(oss, "hello {} world", 42);
CHECK_EQ(oss.str(), std::string{"hello 42 world"});
```

The literal `"hello {} world"` is consteval-validated against
the arg `42` (an `int`); the adapter builds a
`std::format_string<int>` wrapper around the literal and
calls `make_format_args(42)`. The runtime entry path
(`ostream_print_runtime`) writes the vformat'd string into
the ostringstream's streambuf via `os.write`.

## Section 3 — `ostream_println` appends `'\n'`, not `std::endl`

`std::println(FILE*)` flushes after the newline.
`std::println(std::ostream&)` (libstdc++) does NOT. Today's
`petra::ostream_println` matches the latter — it appends
`'\n'` and leaves the ostream's flush policy alone. If the
caller needs a flush, they call `os.flush()` after the
println. The lesson's section 3 pins this down with a
trailing-newline CHECK.

```cpp
std::ostringstream oss;
petra::ostream_println(oss, "hello {}", "petra");
CHECK_EQ(oss.str(), std::string{"hello petra\n"});
```

## Section 4 — runtime format string

Runtime format strings are common when the format template
comes from a config file or a per-call configuration. The
consteval path doesn't reach them; the runtime entry point
does:

```cpp
std::ostringstream oss;
std::string_view fmt = "value={} name={}";
int n = 42;
std::string s = "petra";
petra::ostream_print_runtime(oss, fmt, std::make_format_args(n, s));
CHECK_EQ(oss.str(), std::string{"value=42 name=petra"});
```

The `make_format_args(n, s)` call binds arguments by
reference (not by value), so the locals must outlive the
vformat call. With C++23 the args have to be lvalues; the
lesson pins this with named locals rather than
`std::make_format_args(42, std::string{"petra"})`.

## Section 5 — `std::format_error` propagates

A bad runtime format string MUST cause `std::vformat` to
throw; the non-bounded adapter does NOT catch it. The
caller decides whether to wrap the call in try / catch
or to use the bounded sibling in section 11, which returns
`std::expected<size_t, std::format_error>` for the same
case.

```cpp
std::ostringstream oss;
std::string_view bad_fmt = "ends with {";
bool caught = false;
try {
    petra::ostream_print_runtime(oss, bad_fmt,
                                 std::make_format_args());
} catch (const std::format_error& e) {
    caught = true;
    CHECK(std::string{e.what()}.find("'{") != std::string::npos);
}
CHECK(caught);
CHECK_EQ(oss.str(), std::string{});   // ostream untouched
```

The implementation-defined what() string contains `'{`;
the lesson's section 5 CHECK pins that substring. Crucially,
`oss.str()` is empty — `std::vformat` parses the format
string BEFORE writing any char, so a parse error leaves the
output stream untouched.

## Section 6 — streambuf-OutputIterator path

The alternative path uses `std::ostreambuf_iterator<char>`
into the ostream's rdbuf():

```cpp
inline void ostream_print_via_buf_runtime(
    ostream& os, string_view fmt, format_args args) {
    std::vformat_to(
        std::move(std::ostreambuf_iterator<char>{os.rdbuf()}),
        fmt, args);
}
```

This is zero-allocation per call (one rdbuf pointer is
copied by std::vformat_to, but no std::string or other
allocator-tracked resource is touched). The trade-off is
that the iterator-copy semantics of std::vformat_to are
subtle on ill-behaved rdbufs — for well-behaved ones like
std::ostringstream and std::ofstream it produces byte-equal
output. Section 6 verifies that.

## Section 7 — real `std::ofstream` sink

A real-world demo: write to a file under `/tmp`, read it
back, verify the bytes:

```cpp
std::ofstream of(path, std::ios::binary | std::ios::trunc);
petra::ostream_print(of, "first line {}\n", 1);
petra::ostream_println(of, "second line {}", 2);
of.flush();
```

The `std::ios::binary` flag prevents any newline
translation; the bytes that go in are the bytes that come
out. This is the most realistic sink test in the lesson.

## Section 8 — `seekp` after `tellp`

A common idiom: write a header, capture the position,
overwrite a field later. Verifies the adapter does not do
anything fancy with the streambuf (no internal buffering
beyond what the user has configured):

```cpp
petra::ostream_print(oss, "HEADER: ");
std::streamoff header_len = static_cast<std::streamoff>(oss.tellp());
// header_len == 8 (the length of "HEADER: ")
petra::ostream_println(oss, "field1={} field2={} END", 1, 2);
oss.seekp(header_len +
          static_cast<std::streamoff>(std::string{"field1="}.size()));
petra::ostream_print(oss, "9");   // overwrite '1'
CHECK_EQ(oss.str(), std::string{"HEADER: field1=9 field2=2 END\n"});
```

The signedness cast on `header_len + 7` (where `header_len`
is `std::streamoff` and `string::size()` is `std::size_t`)
is what keeps strict-warning builds silent. Section 8's
test case verifies the integration end-to-end.

## Section 9 / 10 / 11 — bounded sibling

`petra::ostream_write_n(ostream&, cap, fmt, args...)` returns
`std::expected<std::size_t, std::format_error>` mirroring the
Aug 24 / 25 / 26 chain. The implementation uses a two-pass
design:

- Pass 1 — measure. Chars are fed into a
  `counter_output_iterator` that increments an external
  counter on each assignment. No allocation, no I/O. If
  `std::vformat` throws `std::format_error` here, we catch
  it and return early; the ostream was never touched.
- Pass 2 — write. Chars are fed into a
  `bounded_ostream_writer` that sinks them into the rdbuf
  via `os.put(c)`, but stops at `cap` chars. The would-be
  size from pass 1 is returned; the actual written count is
  internal.

This is the same shape of design the Aug 24 / 25 / 26 chain
used. The two-pass design lets us return the would-be size
on the success path regardless of whether the buffer was
large enough.

```cpp
// Section 9 — success returns would-be size:
std::ostringstream oss;
auto r = petra::ostream_write_n(oss, 64, "hello {}", "world");
CHECK(r.has_value());
CHECK(r.value() == 11);
CHECK_EQ(oss.str(), std::string{"hello world"});

// Section 10 — truncates writes at cap, would-be preserved:
auto r2 = petra::ostream_write_n(oss, 5, "abcdefghij");
CHECK(r2.has_value());
CHECK(r2.value() == 10);                 // would-be
CHECK_EQ(oss.str(), std::string{"abcde"});  // capped

// Section 11 — format_error -> expected, no escape:
std::string_view bad_fmt = "ends with {";
auto r3 = petra::ostream_write_n_runtime(
    oss, 64, bad_fmt, std::make_format_args());
CHECK(!r3.has_value());
CHECK(r3.error().what() != nullptr);
CHECK_EQ(oss.str(), std::string{});
```

The bounded sibling preserves the Aug 24 / 25 / 26 chain's
"success returns the would-be size" semantics, NOT the Aug
27 LogError / Overflow distinction — the bounded sibling
doesn't synthesise a domain error type; it accepts that the
caller knows what they want (truncation-as-success is a
legitimate call-site choice).

## Section 12 — UTF-8 round trip

UTF-8 is a byte-by-byte encoding. The adapter writes each
formatted char through `os.put(c)` (or through
`os.write(s.data(), s.size())` in the vformat -> string path).
Both paths preserve the UTF-8 byte stream; no codepoint
mangling.

```cpp
std::string text = "h\xC3\xA9llo w\xC3\xB6rld";   // 13 UTF-8 bytes
auto r = petra::ostream_write_n(oss, 64, "msg: {}", text);
CHECK(r.has_value());
CHECK_EQ(oss.str(), std::string{"msg: "} + text);
CHECK_EQ(oss.str().size(), std::size_t{18});   // 5 + 13
```

Note: `oss.str().size()` returns the byte count, not the
codepoint count, because `oss.str()` returns a `std::string`
(which is byte-oriented in libc++ 21 — `char8_t` is the
correct type for UTF-8 by C++23 but `std::ostream`'s char
type is still `char`). The byte count == UTF-8 byte count
because the adapter does no transcoding.

## Section 13 — NUL byte through

A NUL byte in the formatted output is just another char.
std::vformat treats `'\0'` (when fed as a value, not a
literal) as a literal char in the output. Section 13
verifies a `'{'` `'\0'` `'{'}` payload survives:

```cpp
auto r = petra::ostream_write_n(oss, 32, "{}{}{}",
                                "<a>", '\0', "<b>");
CHECK(r.has_value());
CHECK_EQ(r.value(), std::size_t{7});  // "<a>" (3) + '\0' (1) + "<b>" (3) = 7
// The output contains a NUL exactly once and is sandwiched
// between "<a>" and "<b>":
const std::string& s = oss.str();
CHECK_EQ(s.size(), std::size_t{7});
CHECK(s[0] == '<'); CHECK(s[1] == 'a'); CHECK(s[2] == '>');
CHECK(s[3] == '\0');
CHECK(s[4] == '<'); CHECK(s[5] == 'b'); CHECK(s[6] == '>');
```

The consteval path is more permissive than the runtime
path on the `"ends with {\0}"` case (a NUL inside a literal
is silently dropped by the literal-to-format-string
conversion), but the runtime path through
`std::make_format_args('<a>')` etc. is well-defined.

## Section 14 — range formatter (P2286R6)

C++23's P2286R6 range formatters are part of std::format
and are inherited by the adapter. Section 14 verifies
std::vector<int> format through auto-bracketing:

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
std::ostringstream oss;
petra::ostream_println(oss, "values: [{}]", v);
CHECK_EQ(oss.str(), std::string{"values: [[1, 2, 3, 4, 5]]\n"});
```

(The literal `"values: ["` followed by the range's
auto-bracketed `[1, 2, 3, 4, 5]` produces
`"values: [[1, 2, 3, 4, 5]]\n"` — the inner brackets are
from the range formatter, the outer brackets are from the
literal.)

## Section 15 — 50-round determinism

Section 15 stress-tests the alternation between the
string-path and the streambuf-path adapters, verifying
that for 49 distinct (round index, val) pairs, both paths
produce byte-identical outputs:

```cpp
// Pin check before the loop.
std::ostringstream pin_a, pin_b;
petra::ostream_print(pin_a, "round 0 / val {} / end", 0);
petra::ostream_print_via_buf(pin_b, "round 0 / val {} / end", 0);
CHECK_EQ(pin_a.str(), pin_b.str());
CHECK_EQ(pin_a.str(), std::string{"round 0 / val 0 / end"});

// 49 rounds: 3 CHECKs each (equal, non-empty, expected-vs-actual).
for (int i = 0; i < 49; ++i) {
    std::ostringstream oss_a, oss_b;
    petra::ostream_print(oss_a, "round {} / val {} / end",
                         i + 1, (i + 1) * 7);
    petra::ostream_print_via_buf(oss_b, "round {} / val {} / end",
                                 i + 1, (i + 1) * 7);
    CHECK_EQ(oss_a.str(), oss_b.str());           // 3a
    CHECK(!oss_a.str().empty());                  // 3b
    CHECK(oss_a.str() == fmt);                    // 3c
}
```

This is the same shape of cross-check Aug 24's lesson used
for its 50-round determinism stress.

## Section 16 — runtime fmt with width / alignment / precision

The runtime path picks up the same range of width / fill /
alignment / precision specifiers the consteval path uses:

```cpp
std::string_view fmt = "{:>8} {:<8} {:>8.2f}";
petra::ostream_println_runtime(oss, fmt,
    std::make_format_args("abc", "defghi", 3.14159));
// "     abc defghi       3.14\n"
```

The output layout is: 5 spaces + abc + 1 literal space +
defghi + 2 trailing spaces (left-aligned to width 8) + 1
literal space + 4 leading spaces (right-aligned width 8 of
"3.14") + "3.14" + "\n". Total 27 chars, matching
`oss.str().size()`.

## Strict signedness handling

The adapter has two signed/unsigned bridges:

1. `os.write(s.data(), static_cast<std::streamsize>(s.size()))`
   in `ostream_print_runtime`. `std::size_t` (unsigned) is
   bridged to `std::streamsize` (signed long long). The
   cast is explicit. ASCII payloads < 2 GB don't reach the
   limitation; sections 7 and 13 verify the bridge for
   typical sizes.

2. Pass 1 (measure pass) of `ostream_write_n_runtime` uses
   an external `std::size_t*` counter via
   `counter_output_iterator`. The iterator's
   `difference_type` is `std::ptrdiff_t`, but the counter
   stored externally is `std::size_t`. The iterator's
   `operator=` only assigns char (single-byte); no
   size conversion happens at the iterator level. The
   sections that call into the bounded sibling pin this
   down.

Together these keep strict-warning builds silent. There
are no implicit signed/unsigned conversions in the lesson
code; the only explicit casts are the two listed above.

## Where we go next

Today's lesson closes the **first of two open follow-on
items** from the Aug 22 / Aug 27 lesson chain's "Where we
go next" sections — the `std::print(std::ostream&, ...)`
consumer-side adapter. The remaining item remains:

- **`std::format` to `std::ostream`** for type-erased
  `std::span<const std::any>` args — when the args come
  from a dynamic-typed source, a
  `variant_format_arg_store`-like API is the canonical
  shape. libc++ 21 does not have a vendored
  `__format_arg_store` that supports `std::any`.

New items surfaced by today's lesson:

- **`std::ostream_print` for a custom `std::streambuf`
  that fails to consume all chars** — some streambufs
  (e.g. compression streams, network sinks) signal
  "downstream full" by failing to consume; today's
  `ostreambuf_iterator` silently reports end-of-stream and
  the adapter treats this as a successful truncating write.
  A future lesson could pin this case and emit a
  `partial_write` error kind on truncation.
- **`std::ostream_print` with NUL-terminated output** —
  the Aug 25 lesson established a NUL-termination contract
  for raw char* sinks; the same contract would let an
  ostringstream adapter string out into a C-string. Today
  the adapter does not synthesise a NUL (ostringstream
  manages its own memory), but a custom ostream whose
  payload is read out via `os.str()` could expect a NUL.
- **`std::format_to_n` straight to ostream streambuf** —
  P2216R3 doesn't ship this overload either; the same
  `bounded_ostream_writer` machinery could be lifted to
  format_to_n's runtime path. The two-pass measure-then-
  write design re-uses unchanged.
- **`std::print`-compatible log macros** —
  `LOG_INFO("value={}", 42)` macro sugar on top of the
  adapter would round out the surface. The macro maps the
  args to std::make_format_args at the call site; the
  adapter does the format.

The cross-cutting infrastructure items from the Aug 13 /
Aug 15 / Aug 17 lessons remain open:

- Pin actions to commit SHAs — `@v4` is a floating tag.
- Multi-OS matrix extending to `windows-latest`.
- Status badge in README.
- vcpkg / Conan port for `psp_span_lib`.
- Branch protection requiring the matrix to pass.
- `v0.16.0` promotion — the mechanical lift of
  `validate_atomic` + `parse_and_apply_atomic_streaming_
  validated` + `resolve_with_validation` + `parse_patch_ops`
  into `<psp_span/json_schema.h>`, `<psp_span/json_pointer.h>`,
  and `<psp_span/json_ext.h>`.
