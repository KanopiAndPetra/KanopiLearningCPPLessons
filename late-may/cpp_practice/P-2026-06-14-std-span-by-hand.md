# C++ practice 2026-06-14 — `std::span` (the C++17 design by hand)

## What I set out to learn

`std::span<T>` (C++20) is the standard library's answer to a question
that's been bothering C++ since the 1990s:

> I have a function that wants to look at a contiguous sequence of
> `T`s. It doesn't own the data. It just wants to read (or maybe
> write) the elements. What does the parameter type look like?

The old answers are all bad:

- **`const T*, std::size_t`** — works, but the two parameters can
  desynchronise (passing the wrong length is a real bug). No type
  enforcement of "size matches pointer."
- **`const std::vector<T>&`** — forces the caller to copy into a
  vector if they have a C-array. Allocates. Hides intent.
- **`const T (&)[N]`** — fixes the size at compile time. Rigid.
- **A `begin`/`end` iterator pair** — works, but the most common
  case (contiguous memory with a known length) is one of the more
  expensive patterns to spell.

`std::span<T>` is the right shape: **a non-owning view = pointer +
extent**. That's it. The caller still owns the data; the callee
borrows it. The pair `(ptr, size)` is bundled into a single
copy-by-value parameter, so it can't desynchronise, and `sizeof`
is just two words.

The shape matters for *all* the usual ownership arguments:

1. No allocation, no copy. The function gets a 16-byte value type
   (on 64-bit) and uses it as a window.
2. Caller keeps ownership. Lifetime is the caller's problem. The
   function can be `noexcept`.
3. The size is in the *type* if you want it there (`Span<int, 5>`),
   and the compiler can use that to elide the runtime size field.
4. Works equally well with C-arrays, `std::array`, `std::vector`,
   raw `(ptr, len)` pairs, and other spans.

The cron workflow builds with `-std=c++17`, and `std::span` is a
C++20 feature, so the main artifact in this file is a
**from-scratch `psp::Span<T, Extent>` in C++17** that exercises the
same shape. Section 8 then conditionally demos the real `std::span`
when it's available.

The file is
`late-may/cpp_practice/P-2026-06-14-std-span-by-hand.cpp`. Path
matches the Jun 9–13 convention (`late-may/cpp_practice/`,
P-prefix).

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-14-std-span-by-hand \
    P-2026-06-14-std-span-by-hand.cpp
./P-2026-06-14-std-span-by-hand
```

ASan build, also exercised:

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
    -o P-2026-06-14-std-span-by-hand-asan \
    P-2026-06-14-std-span-by-hand.cpp
ASAN_OPTIONS=halt_on_error=0 ./P-2026-06-14-std-span-by-hand-asan
```

## Sections at a glance

1. **`psp::Span<T, Extent>`** — the from-scratch type (pointer + size)
2. **One function, five call shapes** — the killer use case
3. **In-place mutation** — non-const spans
4. **Span over `std::byte`** — raw binary buffers
5. **Static vs dynamic extent** — same shape, different `sizeof`
6. **`first` / `last` / `subspan`** — slicing without copying
7. **Span over a `std::string`** — bytes vs chars
8. **Real `std::span` (C++20) side-by-side** — guarded by `__cpp_lib_span`

## Key ideas

### A span is `(pointer, extent)` — nothing more, nothing less

The whole type:

```cpp
template <class T, std::size_t Extent = dynamic_extent>
class Span {
    T*          data_;
    std::size_t size_;
public:
    constexpr T* data()    const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    // ... ctors, accessors, iterators
};
```

Two fields. The pointer is the start; the size is the count.
Iterators are just `T*` and `T* + size_` because raw pointers are
random-access iterators. There is no allocator, no refcount, no
ownership state. The view is exactly two machine words on a
64-bit system.

`Extent` is a *type-level* parameter. When it's a known
compile-time constant (e.g. `Span<int, 3>`), the compiler can do
things at compile time it can't do with a runtime size: elide the
size field, pick the right overload, reject mismatches. When it's
the sentinel `dynamic_extent` (= `(size_t)-1`), the size is a
runtime value and the type is more flexible. The two are
interconvertible: a `Span<int, 5>` can be passed where a
`Span<int>` is expected, and the size field is filled in at
construction time.

### The killer use case is the parameter type

The whole point of a span is that you can write a function
parameterised on it, and have it accept every common contiguous
range. Section 2 of the file:

```cpp
void print_ints(psp::Span<const int> s, const char* tag) {
    std::printf("[%s] size=%zu, data=[", tag, s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::printf("%d%s", s[i], i + 1 == s.size() ? "" : ", ");
    }
    std::printf("]\n");
}
```

One function. Five call shapes that all compile and do the right
thing:

```cpp
int c_arr[5] = {1, 2, 3, 4, 5};
print_ints(c_arr, "C-array");              // Span<int, 5> deduced

std::array<int, 5> std_arr = {10, 20, 30, 40, 50};
print_ints(std_arr, "std::array");         // Span<int, 5> deduced

std::vector<int> vec = {100, 200, 300};
print_ints(psp::Span<const int>(vec), "std::vector");  // explicit wrap

auto s = psp::Span<const int>(c_arr);
print_ints(s, "Span (re-view)");           // dynamic->dynamic

int heap[3] = {7, 8, 9};
print_ints(psp::Span<const int>(heap, 3), "ptr+size");
```

The trace in the run output:

```
[C-array]     size=5, data=[1, 2, 3, 4, 5]
[std::array]  size=5, data=[10, 20, 30, 40, 50]
[std::vector] size=3, data=[100, 200, 300]
[Span]        size=5, data=[1, 2, 3, 4, 5]
[ptr+size]    size=3, data=[7, 8, 9]
```

In real `std::span`, the `std::vector` case is also implicit
(there's a `Span(Container&)` constructor). In my hand-rolled
version, I had to spell `psp::Span<const int>(vec)` because making
the U-conversion constraint right was a chain of SFINAE headaches
(see the long comment near the `Span(std::vector<U>&)` ctor in
the file). The constraint is `is_convertible<U*, T*> && !is_const<U>`,
which is the minimum needed to get `std::vector<int>` → `Span<const int>`
without trying to instantiate `std::vector<const int>` (which is
ill-formed because `std::allocator<const T>` is deleted).

### Static extent: the size is in the type, not in the value

`Span<int, 5>` and `Span<int>` are *different types* with the same
*runtime shape* (pointer + size) in the by-hand version. In real
`std::span`, the static-extent version is fatter in the type
system but slimmer at runtime: the size field can be elided, the
pointer packs into a single machine word, and the size-checked
overloads can be picked at compile time.

The trace in section 5 of the file:

```
sizeof(Span<int, 3>) = 16
sizeof(Span<int>)    = 16
```

Both are 16 bytes on this build (pointer + size_t, no
empty-base optimization in my hand-rolled version). Real
`std::span<int, 5>` would print `sizeof = 8` here. I noted the
gap in the code comments — the by-hand version is *not* a
drop-in for production, it's a learning artifact. The shape
(pointer + extent) is what matters; the layout optimisation
(extent-aware union) is a real `std::span` implementation
detail that I'd reach for in production but isn't pedagogically
the point.

### The span does not own; the function may mutate

Section 3:

```cpp
void scale_in_place(psp::Span<int> s, int k) {
    for (int& x : s) x *= k;
}

int data[4] = {1, 2, 3, 4};
print_ints(data, "before");     // [1, 2, 3, 4]
scale_in_place(data, 10);
print_ints(data, "after x10");  // [10, 20, 30, 40]
```

The span is non-owning, but the elements don't have to be const.
`Span<int>` and `Span<const int>` are different types. The
const-ness of the *view* is independent of the const-ness of the
*element type*, which is independent of the const-ness of the
caller's storage. A non-const span over a non-const C-array is
exactly what you want for in-place updates; a const span over
the same array is exactly what you want for a read-only
visitor. The type system makes the difference explicit.

This is the bit that trips people up the first time: "non-owning
view" is not the same thing as "const view." Both are common, and
a span can be either depending on the `T`.

### Slicing is free; the size is in the type when it can be

Section 6 demonstrates `first<N>()` / `last<N>()` / `subspan<O, N>()`.
These are template overloads that return a *new span* with a
different extent. The static versions (`first<2>()`) return
`Span<T, 2>` — the size is now a type-level fact, the runtime
size field is gone (in real `std::span`). The dynamic versions
(`last(2)`) return `Span<T, dynamic_extent>` — size is a runtime
value.

The trace:

```
[orig]           11 22 33 44 55 66 77 88
[first<2>]       size=2, [11, 22]
[last(2)]        size=2, [77, 88]
[subspan<2,3>]   size=3, values: 33 44 55
```

Note that the result of `first<2>()` is `Span<int, 2>` — a
*different type* from the input `Span<int, dynamic_extent>`. The
caller can either pass it on as a static span, or assign it to a
dynamic span and pay a runtime size copy. The compiler picks
the right path based on context, and the type system makes the
right answer a compile-time fact when it can be.

### Span over `std::byte` for binary protocols

`std::byte` is the C++17 type for "raw byte, please don't
arithmetic on me by accident." Section 4:

```cpp
unsigned char raw_buf[6] = {0x00, 0x01, 0x00, 0xFF, 0x00, 0x02};
psp::Span<const std::byte> raw_view(
    reinterpret_cast<const std::byte*>(raw_buf), 6);
```

The `reinterpret_cast` happens *once*, at the call boundary.
Inside `count_nonzero_bytes` the iteration is over `std::byte`,
which prevents arithmetic accidents like `raw_view[0] + 1`. The
trace:

```
raw_buf size_bytes=6, nonzero=3
```

`size_bytes()` is `size() * sizeof(T)` — useful for the
memcpy-style "how many bytes do I need to send over the wire"
question that comes up in any binary protocol.

### String views: bytes and chars are the same memory

Section 7 shows that a `std::string`'s storage can be viewed two
ways:

```cpp
std::string msg = "hello";

// As bytes — for hashing, binary protocols, CRC, etc.
psp::Span<const std::byte> raw(
    reinterpret_cast<const std::byte*>(msg.data()), msg.size());

// As chars — for printf-style consumption.
psp::Span<const char> ch(msg.data(), msg.size());
```

Both views point at the *same memory*. The span type is what
makes the "I promise to treat this as bytes" or "I promise to
treat this as chars" intent explicit. Without a span, both would
be `(const void*, size_t)` and `(const char*, size_t)` pairs
that drift apart the moment you change one and not the other.

### Real `std::span` (C++20) — what production does differently

Section 8 is guarded by the feature-test macro `__cpp_lib_span` and
falls back to "not available" on this build. The check is:

```cpp
#if defined(__has_include) && __has_include(<span>) && \
    defined(__cpp_lib_span) && (__cpp_lib_span >= 201902L)
  #define PSP_HAS_REAL_SPAN 1
```

On this toolchain (Apple Clang, `-std=c++17`), `<span>` is
*installed* but the `__cpp_lib_span` macro is not defined —
`std::span` is a C++20 feature and the cron builds with C++17.
The guard fires and the C++20 demo block is skipped, which is
the correct behaviour.

If I had built with `-std=c++20`, the `__cpp_lib_span` value
would be `202002L` and the real-`std::span` demo would run.
The differences from the by-hand version are real but small:

- `std::span<int, 5>` is 8 bytes (just a pointer), not 16.
  The static extent is encoded in a union/discriminated layout.
- The vector ctor is implicit — no need to spell `Span<int>(v)`.
- There are additional constraints based on
  `std::contiguous_iterator` and `std::ranges::contiguous_range`
  concepts, which catch more bad use cases at compile time.

The conceptual point — *a non-owning view = pointer + extent,
cheap to pass, caller still owns* — is identical. That's the
lesson. The C++20 implementation is a polished version of the
shape we built by hand.

## Design choices and trade-offs

### Why pointer + size, not (begin, end)?

`Span<T>` could have been `(T*, T*)` (a begin/end iterator pair)
or `(T*, std::size_t)` (pointer + count). The standard picked
the latter because:

1. **Size is a frequent need.** "How long is this?" comes up in
   almost every loop. `(begin, end)` forces `end - begin` to
   compute it; `(ptr, count)` is already in the value.
2. **`size_bytes()` is a one-liner.** `size() * sizeof(T)` —
   not `(end - begin) * sizeof(T)`. Both work; the
   pointer+count version is more direct.
3. **Empty span is a one-liner check.** `size() == 0` —
   not `begin() == end()`, which is fine but requires a
   pointer comparison.

The iterators (`begin()`, `end()`) are still there for
range-based for and STL algorithms, so the convenience isn't
lost.

### Why the U-converter for `std::vector` was tricky

The by-hand `Span(std::vector<U>& v)` ctor has a SFINAE
constraint of `is_convertible<U*, T*> && !is_const<U>`. The
`!is_const<U>` is there because the ctor would otherwise
instantiate `std::vector<const int>`, which is ill-formed —
`std::allocator<const T>` is explicitly deleted. The
`is_convertible<U*, T*>` is there so the ctor is still
discoverable for `Span<const int>::Span(std::vector<int>&)`,
where the implicit `int* → const int*` conversion is what we
*want* the type system to do.

The chain of constraints ended up non-obvious; the comments in
the file walk through it. Real `std::span` has a
`std::ranges::contiguous_range` concept that hides most of this
behind a single trait, and the ctor template body is much
shorter.

### Why I didn't implement `as_bytes` / `as_writable_bytes`

Real `std::span` has two free functions that reinterpret a
`Span<T>` as a `Span<std::byte>` (read-only) or `Span<std::byte>`
(writable). They're one-liners:

```cpp
template <class T, std::size_t E>
auto as_bytes(Span<T, E> s) {
    return Span<const std::byte>(
        reinterpret_cast<const std::byte*>(s.data()),
        s.size_bytes());
}
```

I left these out of the by-hand version because `std::byte` is a
C++17 feature and the by-hand span is meant to feel C++14-ish.
Section 4 of the file does the same `reinterpret_cast` manually
to show the pattern. In production I'd reach for
`std::as_bytes` and never write the cast.

## Cross-references and follow-ups

- **Jun 10 (`std::variant` / `std::visit`)** — the AST pattern
  in the expression evaluator builds a `std::variant` over
  `std::unique_ptr<Node>` recursively. A `Span<const Token>`
  would be the natural parameter type for the lexer's input
  range; a `Span<Node*>` would be the natural parameter type
  for a tree-walker that wants to look at siblings. Both
  uses would replace `(begin, end)` iterator pairs with
  `(ptr, count)` views, with the usual benefits.
- **Jun 4 (operator overloading)** — the `Inventory` is a
  vector of items; a `Span<const Item>` parameter would let a
  `find_by_name` function accept a C-array, a vector, or a
  span, with one signature.
- **Jun 11 (`std::expected`)** — a `Span<const std::byte>`
  parameter for a binary parser would let the parser accept
  any contiguous byte source. The `ParseError` value would
  carry the byte offset (`size_t` of the span) for context.

## Next Steps

- **Recursive `std::variant` in `std::unique_ptr`** — the AST
  pattern `std::variant<Leaf, std::unique_ptr<Node>>` is
  smaller and faster than the `std::shared_ptr` version in
  the Jun 10 expression evaluator. Worth rebuilding the
  evaluator with `unique_ptr` to see the difference.
  (Still on the list from Jun 11, Jun 12, and Jun 13.)

- **Build the multi-file `Inventory` as separate compilation**
  (`.h` / `.cpp` / `Makefile`) — the Jun 9 Inventory is one
  big file; splitting it shows how moves work at the link
  boundary. (Still on the list from Jun 9, Jun 11, Jun 12,
  and Jun 13.)

- **Tagged union vs. inheritance for ASTs** — when is the
  variant approach clearer than `virtual Expr::accept(...)`?
  Open-closed on alternatives vs. open-closed on operations.
  (Still on the list from Jun 11, Jun 12, and Jun 13.)

- **A "real" error type with `std::error_code` integration** —
  the `ParseError` in Jun 11 is a POD struct; the production
  version uses `std::error_code` for OS-level errors and a
  custom category for domain errors. C++23's `std::expected`
  + `std::error_code` is a strong idiom for low-level
  libraries. (Still on the list from Jun 11, Jun 12, and
  Jun 13.)

- **Co-routines (C++20)** — the next big abstraction layer.
  Worth reading about after `std::span` and the recursive
  variant, because the patterns there are closer to the
  co-routine design space. (Still on the list from Jun 13.)

- **`std::visit` with stateful visitors** — a pretty-printer
  for `std::variant<A, B, C>` that writes the active
  alternative's fields to an `ostream`. Builds on the Jun 10
  variant/visit session. (Still on the list from Jun 11,
  Jun 12, and Jun 13.)

- **C++23 `std::expected<T, E>` for span-aware parsing** —
  the natural place to use a `Span<const std::byte>` is a
  binary parser that returns `expected<Parsed, ParseError>`.
  The span gives the parser its window; `expected` gives the
  caller the success-or-error story. (New on the list, a
  natural follow-up to Jun 11's `std::expected` session and
  today's `std::span` session.)

- **`std::mdspan` (C++23)** — the multi-dimensional sibling of
  `std::span`. Same non-owning-view idea, but over a
  2D/3D/N-D layout with custom strides. The lessons from
  `std::span` (static vs dynamic extent, no ownership, cheap
  to pass) carry over; the new piece is the layout policy.
  (New on the list, worth scheduling after a
  recursive-variant session.)

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:**
  `late-may/cpp_practice/P-2026-06-14-std-span-by-hand.cpp` and
  this notes file (`.md`).
- **P- prefix per the new convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.
  `std::span` itself is C++20, so the main artifact is a
  from-scratch `psp::Span<T, Extent>` in C++17. Section 8 has a
  feature-guarded demo of the real `std::span` that activates
  under `-std=c++20`.

---

*A span is the smallest possible useful view type: pointer + size.*
*The whole class is two fields, the value is two words, and the*
*benefit is enormous. Function parameters stop being a choice*
*between (ptr, len) pairs that can drift apart, vector references*
*that force allocation, or arrays of fixed size that break the*
*second you want to generalise. A span says "I'm borrowing this*
*contiguous memory for the duration of this call," and the type*
*system enforces the discipline. Static extent is the special*
*case where the size is so well-known that it can live in the*
*type, eliminating the runtime field. The C++20 production*
*version is a polished version of this same shape — better*
*constraints, slimmer layout, more constructors — but the*
*pedagogical core is what the by-hand version captures: a*
*view is just a (pointer, size) pair that bundles the two*
*into a single value, and that bundle is the right answer to*
*one of the most-asked parameter-type questions in C++.*
