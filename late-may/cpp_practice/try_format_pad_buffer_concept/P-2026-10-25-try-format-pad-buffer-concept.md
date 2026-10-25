# P-2026-10-25 — try_format_pad_buffer_concept

## What today closes

Oct 24's `try_format_xmacro_pad_table` ended with a "Where we go
next" list. Item (1) is today's lesson, taken **verbatim**:

> **Item (1):** "Port the `char*`+`cap` axis onto the same generic
> wrapper — still open from Oct 22's item (1) and untouched by Oct 23.
> Oct 18 / Oct 20 built four `char*`+`cap` wrappers per pad by hand; the
> `template <bool Fills, char Fill>` shape is proven on the
> `std::span<char>` axis and the `char*`+`cap` overload is mechanical."

Oct 24 called the overload **mechanical**. It is not — and the reason
it is not is the lesson.

---

## KEY FINDING #1 — THE TWO AXES WERE NEVER TWO AXES

The arc has carried two "axes" since Oct 18: a `std::span<char>` axis
and a `char*`+`cap` axis. Oct 22 built the generic
`template <bool Fills, char Fill>` wrapper family — on the span axis
**only** — and the raw axis kept its hand-written siblings.

But look at what the two axes actually differ in. It is **one thing**:
how the *(pointer, length)* pair is spelled.

- `std::span<char>` bundles the pair into one object.
- the raw axis passes the halves as two arguments.

Everything else was already identical: the two-pass measure/write impl,
the `FormatError` contract, the buffer-atomicity contract, the fill
loop.

And the arc's own history says the pair was **always** the primitive.
`petra::detail::try_format_runtime_n_impl` has taken
`(char* out, std::size_t cap, ...)` since Sep 8, and every span-axis
wrapper has always been an *adapter* over it:

```cpp
try_format_runtime_n(std::span<char> out, std::string_view fmt, ...)
    -> detail::try_format_runtime_n_impl(out.data(), out.size(), fmt, ...)
```

So the span axis was the adapter and the raw axis was the primitive,
and the "port" was never about copying code — it was about **naming
the pair as a type**.

### Measured, not asserted

The dedup is real and countable. Scanning the whole arc for functions
whose parameter list starts with `char* out, std::size_t cap` and whose
name mentions a pad:

| | count |
| --- | ---: |
| **distinct** hand-written raw-axis padded signatures across the arc | **40** |
| lessons that re-declared them (total occurrences) | 137 |
| distinct pads with a hand-written raw-axis family | 7 (SPACE, DASH, HASH, TILDE, UNDERSCORE, STAR, AT) |
| **today's raw-axis padded siblings** | **6** |
| the generic core underneath them | **1** |

40 → 6, and the 6 are one-line forwarders. The four-per-pad explosion
was never about the pads; it was about the *spelling* of the buffer.

---

## KEY FINDING #2 — THE NAME IS A CONCEPT, AND THE CONCEPT IS WHERE THE WORK IS

In C++20, "a fixed, writable, non-owning extent of char" has a name:

```cpp
template <typename B>
concept CharBuffer =
    std::ranges::view<B> &&
    std::ranges::contiguous_range<B> &&
    std::ranges::sized_range<B> &&
    std::same_as<std::ranges::range_value_t<B>, char> &&
    std::same_as<std::ranges::range_reference_t<B>, char&>;
```

`std::span<char>` satisfies it **as-is** — it is already a
`std::ranges::view`, it is contiguous and sized, and its reference type
is `char&`.

The raw axis gets `petra::raw_buffer`, which is the pair as a value:

```cpp
class raw_buffer {
public:
    constexpr raw_buffer(char* data, std::size_t size) noexcept
        : data_(data), size_(size) {}
    [[nodiscard]] constexpr char* data() const noexcept { return data_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr char* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr char* end() const noexcept { return data_ + size_; }
private:
    char* data_;
    std::size_t size_;
};
```

...and joins the standard's view machinery with **one line**:

```cpp
namespace std::ranges {
template <>
inline constexpr bool enable_view<petra::raw_buffer> = true;
}
```

With that, ONE wrapper family — generic over the buffer type — serves
both axes:

```cpp
template <bool Fills, char Fill, CharBuffer Buf, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_buffer(Buf out, std::string_view fmt,
                                    const Args&... args);
```

and each axis is a **one-line forward**:

```cpp
template <bool Fills, char Fill, typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded(char* out, std::size_t cap,
                            std::string_view fmt, const Args&... args) {
    return try_format_runtime_n_padded_buffer<Fills, Fill>(
        raw_buffer{out, cap}, fmt, args...);
}
```

### The generalisable part

**When two APIs differ only in how one value is spelled, the fix is to
name the value, not to copy the code.** C++20 gives you a way to name
it — a concept — and the concept is where the *safety* lives too (see
findings #3 and #4 below). The alternative, which this arc spent
eleven lessons discovering the hard way, is a wrapper family per
spelling.

---

## KEY FINDING #3 — `std::ranges::enable_view` IS NOT INHERITED

This is the trap, and it is worth naming because it is silent.

A class **derived** from `std::span<char>` is a contiguous_range, it is
a sized_range, and it has `range_value_t == char`. It is **not** a
view — because `std::ranges::view<T>` is defined as

```
range<T> && movable<T> && enable_view<T>
```

and `enable_view<T>` is false for a user type unless it is opted in.
`enable_view` is looked up on the **exact** type, so a derived type
does not inherit the base's opt-in.

Consequence: a user who extends a span type with a helper method gets
a type that **silently fails** every view-based concept. The compiler
says nothing at the point of derivation; the failure surfaces later,
as "this type does not satisfy the concept".

Measured (Part 12a of the source pins all of it):

```cpp
static_assert(std::ranges::contiguous_range<derived_span>);   // true
static_assert(std::ranges::sized_range<derived_span>);         // true
static_assert(!std::ranges::view<derived_span>);               // true  <-- the trap
static_assert(!CharBuffer<derived_span>);                     // true
```

and the **cure** is one more line:

```cpp
namespace std::ranges {
template <>
inline constexpr bool enable_view<opted_in_span> = true;
}
static_assert(CharBuffer<opted_in_span>);                     // true
```

### A second, sneakier instance of the same trap

The `enable_view` specializations must live at **global scope**.
Inside `namespace petra`, the name `std::ranges` resolves to
`petra::std::ranges` — a namespace that does not exist — so the
specialization attaches to nothing and the concept silently rejects
the type. This bit today's own first draft: the error was
`no variable template matches specialization`, plus a cascade of
`no template named 'contiguous_range' in namespace 'petra::std::ranges'`.

The generalisable part: **a customization point for the standard
library is a name in `std`, and a name in `std` inside another
namespace is a different namespace.** Put standard-library
customizations at global scope, always.

---

## KEY FINDING #4 — THE CONCEPT'S FOURTH CLAUSE IS THE WRITABILITY ONE

The clauses are not all equally obvious:

| clause | what it buys |
| --- | --- |
| `view` | the extent does not own its storage — the wrapper writes into a buffer the *caller* owns |
| `contiguous_range` | the bytes are adjacent, so the fill loop is one `std::fill` |
| `sized_range` | the length is known, and it is the length of the *run* |
| `range_reference_t == char&` | the buffer is **writable** |

The last one is the one that catches people. A concept that stopped at
"contiguous range of char" would admit `std::span<const char>` — whose
`range_value_t` is `char` but whose `range_reference_t` is
`const char&` — and the fill loop would have to write through it.

The source pins the difference explicitly by defining the naive
concept next to the real one:

```cpp
template <typename B>
concept NaiveCharRange =
    std::ranges::contiguous_range<B> &&
    std::ranges::sized_range<B> &&
    std::same_as<std::ranges::range_value_t<B>, char>;

static_assert(NaiveCharRange<std::span<const char>>);  // true  <-- the bug
static_assert(NaiveCharRange<std::string>);             // true  <-- also a bug
```

The naive concept also admits `std::string` and `std::vector<char>` —
types that **own** their storage, so "a fixed writable extent" is not
the contract they offer (a `std::string`'s `size()` is its logical
length, not its capacity). The `view` clause is what excludes them.

---

## KEY FINDING #5 — `std::span`'s (pointer, count) CONSTRUCTOR CHANGES ITS EXPLICITNESS WITH THE EXTENT

This one is small but it is the kind of thing that only shows up when
you try to write ONE wrapper for both extents.

`std::span`'s (pointer, count) constructor is

```cpp
template <class It>
constexpr explicit(Extent != std::dynamic_extent) span(It first, size_type count);
```

so it is:

- **not** explicit for `std::span<char>` (dynamic extent), and
- **explicit** for `std::span<char, 16>` (static extent).

Verified by compilation:

```cpp
std::span<char> s = {buf, 4};        // OK — copy-list-init
std::span<char, 16> s = {buf, 16};   // ERROR — the ctor is explicit
```

So the two spellings are **not interchangeable**, and "just pass a
span" is not a single decision — it is a decision per extent. The
lesson's answer is to avoid the question: the raw axis has its own
`raw_buffer`, whose two-argument constructor is **not** explicit, so
`raw_buffer{out, cap}` is a copy-list-init that works everywhere.

---

## The cross-axis check — the headline verification

A refactor that makes two spellings share one implementation has to
prove they still behave the same. Today's source does that in two
places:

- **Section 4** — for the *selected* cell, the compile-time path, the
  runtime path and the CONSTEVAL path are each run on **both** axes and
  the buffers are compared **byte for byte**.
- **Section 10** — the same comparison, promoted to **all 1024 cells**
  in one process. The sweep now carries an `axis_fail` counter:

```
sweep: cells=1024 disagree=0 byte_fail=0 axis_fail=0 none_wins=1
```

`axis_fail=0` over 1024 cells is the claim "the two axes are the same
function" stated as a measurement rather than an assertion.

The raw axis also gets the full contract suite: buffer atomicity on
`TooLarge` (compile-time, runtime and throwing paths), buffer atomicity
on a `Format` error, the `written == size` and `size == 0` edge cases,
and the 4-thread × 25-call concurrent dispatch.

---

## The compile-time pins

Part 12 of the source carries the concept's assertions, and each one
is a claim about a **specific C++20 rule** rather than about this
file's plumbing:

- `CharBuffer<std::span<char>>` — a span is a view, contiguous, sized,
  and writable.
- `CharBuffer<raw_buffer>` — true **only** because of the
  `enable_view` specialization.
- `!CharBuffer<std::span<const char>>` — the writability clause.
- `!CharBuffer<std::string>`, `!CharBuffer<std::vector<char>>`,
  `!CharBuffer<std::array<char, 8>>` — the view clause.
- The `derived_span` trap and the `opted_in_span` cure.
- `is_constructible_v<std::span<char>, char*, std::size_t>` and
  `is_constructible_v<std::span<char, 16>, char*, std::size_t>` — the
  explicitness difference.

---

## Verification

Everything below was run for real; all artifacts live under
`/tmp/petra20261025/`.

1. **Strict compile of the default cell** —
   `-std=c++23 -stdlib=libc++ -fexperimental-library -O0 -Wall -Wextra
   -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`:
   **clean, zero warnings**.
2. **Build matrix — 20/20 pass.** Twelve cells (the default, each of
   the ten pads alone, and the all-ten-pads cell), the throw axis in
   three variants, and four multi-pad combinations — each compiled with
   the full strict warning set and required to exit 0 with `fail=0`
   **and empty stderr**.
3. **Invalid flag values rejected at compile time** — `PETRA_PAD=2`,
   `PETRA_BANG_PAD=2` and `PETRA_THROW_ON_OVERFLOW=2` each fail with
   the expected `#error`.
4. **ASan + UBSan — 5 cells clean.** `default`, `bang`, `nul`, `all10`
   and `throw+bang`, built with `-fsanitize=address
   -fsanitize=undefined -fno-omit-frame-pointer`,
   `ASAN_OPTIONS=detect_leaks=0`: zero diagnostics and **empty
   stderr** on all five.
5. **Flakiness — 5 runs × 7 cells, identical tallies every time**
   (`pass=90 fail=0`, `pass=92 fail=0`, `pass=94 fail=0`).
6. **In-process exhaustive sweep — 1024/1024 cells** (Section 10):
   the three runtime selection spellings agree on every cell, the
   runtime padded path writes the winner's byte in every cell, and the
   raw axis agrees with the span axis byte for byte
   (`disagree=0 byte_fail=0 axis_fail=0`).
7. **Independent Python cross-check** (`cross_check.py`, written from
   the *spec* — the rank order, the rank+1 score, the NONE floor, the
   fill contract — not from the C++): the program is compiled with
   `-DPETRA_CROSSCHECK_DUMP` and dumps one line per cell
   (`CELL <mask> <winner> <code> <span_hex> <raw_hex>`); the Python
   model recomputes the expected bytes independently and compares.
   Result: **1024/1024 cells, 0 disagreements, 0 byte failures, 0
   axis failures**, and the `2^(r-1)` win-count identity holds.
8. **Mutation test — the cross-axis check has teeth.** A check that
   passes is only worth what it would catch, so today's source was
   mutated on the **raw axis only** (the raw adapter passes `cap - 1`
   to the generic core, so the raw axis leaves the last byte
   unfilled; the span axis is untouched). Result:

   ```
   pristine: exit=0 pass=90 fail=0 | cells=1024 disagree=0 byte_fail=0 axis_fail=0    none_wins=1
   mutant  : exit=1 pass=81 fail=9 | cells=1024 disagree=0 byte_fail=0 axis_fail=1023 none_wins=1
   ```

   The mutant is caught by **exactly** the counter it should be
   caught by: `axis_fail` goes from 0 to 1023 and the program exits
   non-zero. (1023 rather than 1024 because the NONE cell — mask 0 —
   has no fill, so the mutation is invisible there; that is the
   correct behaviour, not a gap.)
9. **CMake** — `default`, `ENABLE_STRICT_WARNINGS=ON`,
   `ENABLE_STRICT_WARNINGS=ON + ENABLE_ASAN=ON`, `PETRA_BANG_PAD=ON`
   and `PETRA_THROW_ON_OVERFLOW=ON` all configure, build and run
   clean, with **0 warnings** in every build log.

### PASS counts differ per cell, and that is expected

| cell | PASS | why |
| --- | ---: | --- |
| default (no pad) | 90 | the baseline |
| one pad enabled | 92 | + the enabled-pad branches |
| throw axis | 92 | the THROW=1 exception checks |
| throw axis + a pad | 94 | both of the above |

Every one of them is `FAIL=0`.

---

## Notes / gotchas worth remembering

- **When two APIs differ only in how one value is spelled, name the
  value.** In C++20 that means a concept; the concept then carries the
  safety rules (writability, ownership) as clauses rather than as
  review discipline.
- **`std::ranges::enable_view` is not inherited.** A derived-from-span
  type is not a view. The one-line cure is a second `enable_view`
  specialization — and the failure mode is silent.
- **A standard-library customization point must be customized at
  global scope.** Inside another namespace, `std::ranges` names
  `petra::std::ranges`, and the specialization attaches to nothing.
- **`std::span`'s (pointer, count) constructor is explicit only for a
  static extent.** `std::span<char> s = {p, n};` compiles;
  `std::span<char, 16> s = {p, n};` does not.
- **A concept's clauses should each be load-bearing, and the naive
  version should be written down next to it.** Part 12a defines
  `NaiveCharRange` alongside `CharBuffer` precisely so the difference
  is a compile-time assertion rather than a paragraph of prose.
- **A `-Warray-bounds` warning that a sibling file does not produce is
  usually a symptom, not a finding.** Today's first draft produced two
  of them; the cause was three *earlier* compile errors (`NaiveCharRange`
  unqualified inside `namespace petra`) degrading clang's analysis of
  `sel.score - 2`. Fixing the errors removed the warnings. Read the
  first error, not the last.
- **`/usr/bin/g++` on this machine is Apple clang**, so a "g++" result
  is not an independent second implementation. (Carried over from Oct
  22's notes; still true.)
- **This session's terminal guard wedges on some invocations.**
  `bash script.sh` and absolute-path execution of a freshly built
  binary both hang the guard's process-identity probe, while
  `bash -c "$(cat script.sh)"` and PATH-resolved invocation work.
  Every verification script here was run the second way.

---

## What did NOT change (deliberately)

- `petra::FormatError` / `FormatErrorKind`, the two-pass
  (measure, then write) impls, `counting_output_iterator`,
  `bounded_char_writer` and `unwrap_dispatch` are reused **verbatim**
  from Oct 24 / Oct 23 / Oct 21 / Sep 8 / Sep 9.
- The compile-time flag **names** (`PETRA_PAD`, `PETRA_SPACE_PAD`, …)
  and the `-D` contract are unchanged, so yesterday's build cells keep
  their meaning and the CMake options still work as they did.
- The buffer-atomicity contract is unchanged and is re-checked on
  **both** axes and on all three paths.
- The X-macro rank list, the `rank+1` score semantics, the three
  selection spellings and the hand-written oracle are unchanged, so the
  numbers published on Oct 22 / Oct 23 / Oct 24 are unchanged.

---

## Where we go next

1. **Give the runtime path the CONSTEVAL surface too.** Today's runtime
   sibling takes a `std::string_view` format; the compile-time path
   still gets `std::format_string<Args...>`'s compile-time format
   checking (P2216R3). A runtime *selection* with a compile-time
   *format check* is still nobody's built.
2. **Make the runtime sweep cover the ERROR paths too.** Section 10
   sweeps the 1024 success cells; the `TooLarge` and `Format`-error
   paths are checked for a handful of cells but not for all 1024. Since
   the runtime path takes a `PadChoice` value, all 1024 cells' error
   paths are affordable to check in one process.
3. **Compare the hand-rolled `expected<size_t, FormatError>` contract
   against stdlib's `std::format_to_n`** — carried over from Oct 22's
   item (4), still untried.
4. **Revisit the oracle.** The hand-written `oracle_select` if-chain
   is now the one place a new pad must be added by hand. A generic
   oracle — one that reads the *oracle table* rather than being a
   chain — would remove that last per-pad edit, at the cost of the
   "two independent implementations" property. That trade is worth
   making deliberately rather than by accident.
5. **Port the `std::string_view`+`cap` axis.** The arc has
   `char*`+`cap` and `std::span<char>`, but not the const-buffer axis
   — which today's writability clause (`char&`, not `char`) currently
   *rejects* by design. Deciding whether a const output buffer is a
   legitimate contract (write to it, then let the caller see the result
   through a const view) is a real design question, and today the
   answer is "no, and here is the static_assert that says so".
