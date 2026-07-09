# P-2026-07-09 — std::span (C++20): the real one, end to end

## Headline

Today I went from "I have a hand-rolled `psp::Span<T,Extent>` from
Jun 14" to "I have *fluency* with the C++20 standard `std::span<T,Extent>`."
The hand-rolled one was a learning tool — figuring out what a span *is*
by writing one. Today's lesson is the payoff: the standard version is
in `<span>`, has the same shape, and adds a few useful conveniences
that I should reach for in new code.

The headline numbers from the run:

```
__cpp_lib_span = 202002   (C++20 + P2118 bounds-safe revisions)
sizeof(std::span<int>)       = 16   (pointer + size_t)
sizeof(std::span<int, 3>)    = 8    (pointer only — size elided for static extent)
```

The 8-vs-16 split is the most concrete proof that the second template
parameter matters: a `std::span<int, N>` is *literally* one pointer
smaller than a `std::span<int>`. The compiler does not need to store
the size because it's encoded in the type.

## Where this fits in the arc

```
Jun 14  psp::Span<T,Extent> hand-rolled in C++17        (the learning cut)
...
Jun 28  CMake INTERFACE library for header-only psp::Span
Jun 29  extern template — library-owned instantiations
Jun 30  STATIC library (psp_span_lib v0.2.0)
Jul  1  install rules + find_package() consumer          (v0.3.0)
Jul  2  find_package(fmt) — third-party package
Jul  3  CPack TGZ packaging                              (v0.4.0)
Jul  4  CPack resource files: License + Readme           (v0.5.0)
Jul  5  GitHub Release for psp_span_lib 0.5.0
Jul  6  GitHub Actions: tag-push auto-release (Linux)
Jul  8  GitHub Actions: tag-push auto-release (matrix)
Jul  9  std::span (C++20) — tour of the standard name    ← today
```

The psp_span_lib pipeline arc continues, and the next likely step
from Jul 8's notes is still pinning actions to commit SHAs. Today's
lesson is **independent** of that thread — a C++-language topic rather
than a CI topic, because I want to keep one session a week grounded in
the *language* itself rather than tooling around it.

## Why this matters even though psp::Span exists

The honest answer to "why use std::span when I have psp::Span?" is:

1. **Reach.** Every C++ programmer knows `std::span`. None of them know
   `psp::Span`. If I write a library that takes `psp::Span<T>`, every
   user has to learn my API. If I write one that takes `std::span<T>`,
   they already know it.
2. **Composition.** A function taking `std::span<const T>` accepts
   inputs from *every* library that produces `std::span<const T>` — or
   `std::span<T>`, or `std::vector<T>`, or `std::array<T,N>`, or a C
   array. With `psp::Span` I'm limited to callers who know my type.
3. **No third-party dep.** When I install psp_span_lib as a
   find_package target, I drag in my header-only template library
   for *one* purpose: a span. `std::span` is in the compiler's
   standard library. No install, no version pin, no header to ship.
4. **Forward compatibility.** C++26's `std::ranges::subrange`,
   `std::ranges::ref_view`, and `std::ranges` algorithms that take
   `std::span`-compatible ranges all assume `std::span`. The standard
   type is what the *rest* of the standard library is being designed
   against.

So psp::Span stays in psp_span_lib as the production header, but
**new code that doesn't have a legacy reason to prefer it should reach
for std::span by default.** That's the takeaway.

## Section 0 — Toolchain check

`std::span` is gated by `__cpp_lib_span`. On this machine
(Apple clang 21 / libc++ 19+):

```
__cpp_lib_span = 202002
```

The value `202002` corresponds to **P2118R0 "Implementing
`std::span` for the bounded iterators-as-ranges case"** — the C++20
revision that hardened `std::span`'s iterator and sentinel handling
post-C++20. Earlier toolchains report `201803` (the original C++20
proposal P0122). Either value is fine; both indicate a conforming
C++20 `std::span`. If the macro is undefined, `<span>` either doesn't
ship `std::span` or it's behind a different feature gate.

The lesson program has a `static_assert(PETRA_HAS_STD_SPAN, ...)` so
the build fails fast on a toolchain that doesn't have it.

## Section 1 — Construction

`std::span` is a non-owning view. It constructs from anything
contiguous, without allocation:

```cpp
int carray[] = {10, 20, 30, 40, 50};
std::span<int> from_carray = carray;                     // C-array -> dynamic-extent span

std::array<int, 5> stdarr = {11, 22, 33, 44, 55};
std::span<int>       from_stdarr       = stdarr;        // std::array -> dynamic
std::span<int, 5>    from_stdarr_fixed = stdarr;        // std::array -> FIXED extent 5

std::vector<int> v = {1, 2, 3, 4, 5, 6};
std::span<int> from_vec = v;                            // vector -> dynamic

int* p = carray + 1;
std::span<int> from_ptr{p, 3};                          // (ptr, size) ctor

std::span<const int> from_span = from_carray;           // span -> span<const T>
```

The deduction guide for `std::array<T, N>` is the interesting case:
the compiler *can* see the compile-time size, and the deduction guide
*does* prefer the fixed-extent form. For C-arrays, the deduction is
intentionally conservative (dynamic extent) — see Section 6.

## Section 2 — Element access (deliberately unchecked)

```cpp
int a[] = {7, 8, 9};
std::span<int> s = a;

s.front();   // == s[0]
s.back();    // == s[size() - 1]
s.data();    // == &a[0]
s[i];        // UNCHECKED
// s.at(i);  // does NOT exist on std::span
```

The **no `.at()`** choice is a deliberate one. The whole point of a
span is performance-critical inner loops — bounds checks on every
access would defeat that. The hand-rolled `psp::Span` from Jun 14
*does* have `.at()` (it throws `std::out_of_range`), because at the
time I was being defensive. That was the right call for a learning
prototype; for production code, `std::span`'s "no checks" is the
contract the standard chose, and code that needs checks can wrap
with `gsl::span` (the parent proposal, which has `.at()`) or write
their own.

**`.size_bytes()`** is the other name to remember. It's the standard
name (psp::Span from Jun 14 calls it `.byte_size()`). Useful for
hashing, serialization, and `memcpy`-style operations:

```cpp
std::cout << s.size_bytes();   // 12 for an int[3]
```

## Section 3 — Iterators

`std::span<T>::iterator` is a **random-access iterator** — full
arithmetic, full range-for, and all the standard algorithms work
directly:

```cpp
int a[] = {5, 3, 1, 4, 2};
std::span<int> s = a;

// range-for works
for (int x : s) std::cout << x;

// std::sort works directly on the span's iterators
std::sort(s.begin(), s.end());

// reverse iterators work
for (auto it = s.rbegin(); it != s.rend(); ++it) std::cout << *it;
```

The lesson's check at the bottom of section 3 (`end() - begin() == size()`)
verifies the contiguous-iterator invariant. I tried to assert
`s.begin() == s.data()` directly but ran into a real-world gotcha:
**libc++ wraps `T*` in `__wrap_iter<T*>`, and the wrapper's pointer
constructor is private to the wrapper's friends.** So you can't
write `std::span<int>::iterator{s.data()}` in user code. The
distance check (`s.end() - s.begin() == s.size()`) is the portable
form. Lesson-learned.

## Section 4 — Function parameters: the decay idiom

The *reason* span exists, practically, is this:

```cpp
static void sum_and_print(std::span<const int> s, const char* tag) {
    long sum = 0;
    for (int x : s) sum += x;
    std::cout << "[" << tag << "] size=" << s.size() << " sum=" << sum;
}

int carray[] = {1, 2, 3};
std::array<int, 4> stdarr = {10, 20, 30, 40};
std::vector<int> vec = {100, 200};
std::span<int> sp = carray;

sum_and_print(carray, "C-array");       // int[3]         -> span<const int>
sum_and_print(stdarr,  "std::array");   // array<int,4>   -> span<const int>
sum_and_print(vec,     "std::vector");  // vector<int>    -> span<const int>
sum_and_print(sp,      "std::span");    // span<int>      -> span<const int>
```

All four call sites compile. None copy. The `span<const int>`
parameter accepts everything contiguous. That's the practical
"span as function parameter" idiom — replacing the old
`void f(const T*, size_t)` two-argument pattern (error-prone, no
type checking on the count, can't be range-for'd) with a single
typed argument.

The cost of the call is zero — span is two pointer-sized fields
(`T*` + `size_t`), trivially copyable, passed by value. No
allocation, no reference-count bump, no smart-pointer overhead.

## Section 5 — Subviews

`std::span` has three subview operations, all returning NEW spans
(also non-owning views, no copy):

```cpp
int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
std::span<int> s = a;

s.first(3);          // {0,1,2}
s.last(2);           // {8,9}
s.subspan(2, 4);     // {2,3,4,5}
s.subspan(5);        // {5,6,7,8,9}        (offset only, count = rest)

// subviews compose
s.first(5).last(2);  // {3,4}              (left-prefix then right-suffix)
s.last(5).first(2);  // {5,6}              (right-suffix then left-prefix)
```

These are the operations you reach for when a function wants "the
first N of these" or "everything except the first N" or "the middle
slice between offset O and offset O+L". The alternatives
(`{s.data() + O, L}` or a `gsl::span` constructor) are less safe and
no more efficient. The subview ops also play well with the const
narrowing in section 7.

One subtle property worth knowing: for a **fixed-extent** span,
`subspan` can return a **different** fixed extent. `s.subspan(1)`
on `std::span<int,5>` gives `std::span<int,4>` — the compiler
infers the new extent from the count. This is type-safe; a
function that takes `std::span<int,4>` will accept the subspan but
not the original.

## Section 6 — Static vs dynamic extent

The second template parameter (`Extent`) is the load-bearing
parameter:

```cpp
std::span<int>           // == std::span<int, std::dynamic_extent>
std::span<int, 5>        // fixed extent 5
std::dynamic_extent      // the sentinel (= SIZE_MAX)
```

The runtime difference is real:

```
sizeof(std::span<int>)      = 16   (T* + size_t)
sizeof(std::span<int, 3>)   = 8    (T* only — size encoded in type)
```

The fixed-extent version **elides the size field at runtime** because
the size is part of the type and the compiler knows it. Smaller
objects means better cache behavior and fewer stack bytes per
function call.

The type-checking difference is also real:

```cpp
std::span<int, 5> fixed;
std::span<int>    dyn = fixed;                  // OK: fixed -> dynamic widening

std::span<int>    dyn2;
// std::span<int, 7>(dyn2);                      // runtime error if sizes don't match
```

The dynamic -> fixed conversion *requires* an explicit extent
argument and *checks* at runtime that the size matches. You can't
silently write `std::span<int,7>` when you only have 5 elements.

`extent` is exposed as a `static constexpr` member on every span:

```cpp
std::span<int, 5>::extent    == 5
std::span<int>   ::extent    == std::dynamic_extent
```

So `std::is_same_v<MySpan, std::span<int, 5>>` works at compile time
for "is this exactly a span of 5 ints?". Use that in `static_assert`
and `if constexpr` branches to specialize behavior on extent.

## Section 7 — span<T> -> span<const T> conversion

One of the most quietly useful design choices in std::span:

```cpp
static void write_only(std::span<int> s);     // mutates s[0]
static void read_only (std::span<const int> s);   // cannot mutate

int a[] = {1, 2, 3, 4, 5};
std::span<int> mutable_view = a;

read_only(mutable_view);      // OK: span<int> -> span<const int> is implicit
write_only(mutable_view);     // OK: same type
// read_only's body: s[0] = 1;    // would NOT compile — s is span<const int>
```

A function taking `span<const T>` can be called with a `span<T>`
(creating a read-only view of a writable buffer) **without an
explicit conversion at the call site.** This is the same shape as
the `T* -> const T*` decay that raw pointers have always had, but
typed and bounds-aware.

In the lesson's section 7, we mutate `mutable_view` and then re-print
through a `span<const int>` — the read-only view sees the mutation.
Same memory, two different access policies, zero runtime overhead.

This is what makes "function takes `span<const T>` by default, takes
`span<T>` only if it actually mutates" the right default rule.

## Section 8 — Side-by-side vs psp::Span

The honest comparison from Jun 14 (when I built `psp::Span`) and today:

| Feature                                | psp::Span (Jun 14) | std::span (C++20) |
|----------------------------------------|--------------------|--------------------|
| Non-owning view (T* + size)            | ✓                  | ✓                  |
| `T, Extent` template shape             | ✓                  | ✓                  |
| `dynamic_extent` sentinel              | ✓                  | ✓                  |
| Random-access iterators                | ✓                  | ✓                  |
| `.first(N)`, `.last(N)`, `.subspan(O,L)`| ✓                  | ✓                  |
| Ctor from C-array / array / vector     | ✓                  | ✓                  |
| `span<T> -> span<const T>` implicit    | ✓                  | ✓                  |
| `.at(i)` (throws)                      | ✓                  | ✗                  |
| Elides size for fixed extent           | ✗                  | ✓                  |
| `.size_bytes()` observer               | `.byte_size()`     | `.size_bytes()`    |
| In the standard library                | ✗                  | ✓                  |
| Interoperable across libraries         | ✗ (only mine)      | ✓ (everyone's)     |

**Decision rule going forward:**

- New code: default to `std::span<T>` / `std::span<const T>`.
- Library code that has to compile on pre-C++20 toolchains: keep
  using `psp::Span` (or use the pre-`std::span` GSL one).
- psp_span_lib keeps shipping `psp::Span` for its current consumers;
  no breaking change. A future lesson could add a `psp::Span` →
  `std::span` alias header for users who want to migrate.

## What I didn't cover (next-session candidates)

- **Range adaptors that produce `std::span`.** `std::ranges::ref_view`
  and `std::span`'s constructor from a contiguous range (C++23
  deduction) make span the natural output of range pipelines.
- **`std::span` and the `gsl::span` ecosystem.** Bounds-safe span
  profiles (Lifetime safety, Bounds safety) — these were *not* in
  C++20 but are in P2118 / P2909 / WG23 discussions.
- **`std::mdspan` (C++23).** Multi-dimensional view. Same
  "non-owning view over a contiguous buffer" idea, with strides and
  extents per dimension.
- **Performance comparison.** I claim span is zero-cost, but I haven't
  measured it. A future lesson could generate identical code with
  `T*+size_t` and with `std::span` and diff the assembly.
- **Span and ABI.** Span's layout is part of its ABI; changing the
  implementation is breaking. The standard guarantees the layout
  (16 bytes for dynamic on 64-bit, 8 for fixed with pointer-only).
  psp::Span's layout is whatever I wrote — which is *more*
  flexibility but less interoperability.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/cpp_practice/
    └── std_span_cpp20/                                # NEW (the lesson)
        ├── P-2026-07-09-std-span-cpp20.cpp            #    the tour program
        └── P-2026-07-09-std-span-cpp20.md            #    this file
```

No `psp_span_lib` changes today. No release tag today. The lesson
stands on its own — it's about the C++ language, not the library.

## Next steps

The psp_span_lib release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS or .zip
  CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.5.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform and
  SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow feature
  work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam. The lesson
  copies of `release.yml` and `release_matrix.yml` are ready; the
  deploy copies in `.github/workflows/` are blocked on the PAT.

C++ language threads still open:

- **`std::mdspan` (C++23)** — multi-dimensional view, same
  non-owning-buffer philosophy.
- **`std::ranges::ref_view` and `std::span` interop** — span as the
  natural output of range pipelines.
- **Move-assignment and span swap** — `std::span` is trivially
  copyable, so swap is trivial, but `std::span<T, N>` from a
  `std::span<T, M>` constructor when N != M is interesting.