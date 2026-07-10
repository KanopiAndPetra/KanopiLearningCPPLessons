# P-2026-07-10 — std::mdspan (C++23): the rank-N generalization of std::span

## Headline

Today I went from "I know `std::span<T>` is a non-owning 1-D view" (Jul 9)
to "I have fluency with `std::mdspan<T, extents, mapping>` — a non-owning
**N-D** view with the mapping policy baked into the type." The shape
philosophy is identical (non-owning, views-only, mapped from a contiguous
buffer, layout policy in the type, zero-overhead), but the indexing
operation is now rank-N, and the mapping policy is a real template
parameter you can swap out for `layout_right`, `layout_left`, or
`layout_stride`.

The headline numbers from the run:

```
__cpp_lib_mdspan = 202207
__cpp_lib_span   = 202002   (the 1-D companion)

sizeof(std::mdspan<int, std::extents<size_t, 3, 4>>) = 8     (static extents → pointer only)
sizeof(std::mdspan<int, std::dextents<size_t, 2>>)   = 24    (dynamic extents → pointer + 2 extents)
sizeof(mdspan<int, ..., layout_stride>)             = 24    (extra stride[] storage)
```

The 8-vs-24 split is the most concrete proof that the second and third
template parameters matter: `std::mdspan<T, extents<T,N,M>>` with static
extents carries only a pointer (size known at compile time), while
`std::mdspan<T, dextents<T,2>>` carries pointer + 2 runtime extents.
`layout_stride` adds an extra rank-sized strides array on top.

## Where this fits in the arc

```
Jun 14  psp::Span<T,Extent> hand-rolled in C++17        (rank-1, learning cut)
Jul  9  std::span (C++20) — the standard 1-D view       (rank-1)
Jul 10  std::mdspan (C++23) — the standard N-D view      (rank-N)   ← today
```

The psp::Span → std::span → std::mdspan arc is one continuous lesson
about non-owning views. The same memory, the same compile-time cost
arguments (zero, modulo the layout policy), and an indexing operation
that scales with the rank of the problem you're solving.

This lesson stands on its own — it's about the C++ language, not the
psp_span_lib pipeline or the CI work. Keeping one session a week
grounded in the language itself.

## Why this matters even though std::span exists

The honest answer to "why mdspan when I have span?" is the same shape
as the Jul 9 answer:

1. **Correctness.** A 2-D grid is *not* a 1-D buffer with two indices
   bolted on by convention. With mdspan, the (rows, cols) shape is part
   of the type — the compiler enforces it, the debugger prints it, and
   the function signature documents it.
2. **Composability of layout policies.** The same `storage` buffer can
   be viewed as row-major, column-major, or with custom strides —
   **simultaneously**. That's not possible with span; with mdspan it's
   three constructor calls and three views that all alias the same
   memory.
3. **No third-party dep, future-proof.** mdspan is in `<mdspan>` of a
   C++23 stdlib. It composes with std::span (Section 6), std::ranges
   (C++23), and upcoming parallel algorithms that take mdspan
   parameters natively.
4. **Static extents typecheck and elide storage.** A
   `mdspan<int, extents<size_t, 3, 4>>` knows at compile time that it
   has 12 elements; you can't construct one with the wrong shape, and
   the runtime size is elided entirely (8 bytes for the whole mdspan —
   pointer only).

So std::span stays as the rank-1 workhorse, but **for any multi-D
problem (images, matrices, grids, tensors), reach for std::mdspan.**

## Section 0 — Toolchain check

`std::mdspan` is gated by `__cpp_lib_mdspan`. On this machine
(Apple clang 21 / libc++ 19+):

```
__cpp_lib_mdspan = 202207
```

The value `202207` corresponds to **P2630R1 "Expose `std::span` from
`std::mdspan`" and the consolidated C++23 mdspan proposal set**. The
earlier value `202110` was the original P0009R18. Either is fine —
both indicate a conforming C++23 mdspan.

The lesson program has a `static_assert(__cpp_lib_mdspan >= 202207, ...)`
so the build fails fast on a toolchain that doesn't have it.

## Section 1 — The 2-D minimum viable mdspan

The smallest useful mdspan takes a contiguous buffer and a shape:

```cpp
using ext2d = std::dextents<std::size_t, 2>;
using md2d  = std::mdspan<int, ext2d>;

int storage[] = {11, 12, 13, 14,  21, 22, 23, 24,  31, 32, 33, 34};
md2d m{storage, 3, 4};   // 3 rows, 4 cols

m[0, 0];   // 11
m[1, 2];   // 23
m[2, 3];   // 34
```

`dextents<size_t, 2>` is shorthand for `extents<size_t, dynamic_extent,
dynamic_extent>` — "rank 2, both extents dynamic, index type `size_t`."
The buffer is a plain C-array; **mdspan does not own it.** When the
mdspan goes out of scope, the buffer is unaffected.

The mutation proof (`m[1, 2] = 999; storage[1*C+2] == 999`) is what
makes the "non-owning view" contract concrete: the view is a window
into a buffer you already have, not a thing that holds memory.

## Section 2 — Indexing: mdspan[i,j] (and what mdspan[i][j] is *not*)

`std::mdspan` defines `operator[]` as **variadic with exactly `rank()`
arguments**. The natural access is therefore:

```cpp
m[i, j]                  // rank-2 access: m[i, j]
m[i, j, k]               // rank-3 access: m[i, j, k]
```

This is *not* `m[i][j]` chained access — `operator[]` here requires
exactly `rank()` arguments in one call. That's by design: a
multi-dimensional view is one operation, not `rank()` chained
operations. The Jul 9 lesson mentioned that `std::span` exposes
`iterator` and `range-for` support; mdspan does too (via random-access
iterators), but the canonical element access is the rank-N `m[i, j]`
form.

**A note on `std::submdspan` and `std::full_extent_t`:** these were
originally part of the C++23 mdspan proposal but were split out into
**P2630R1** as a post-C++23 addition. **This libc++ ships the C++23
mdspan core but not the slicing helpers** — so Section 5 of the lesson
slices manually by re-basing the pointer and re-declaring the shape.
In a C++26 toolchain, you'd write `std::submdspan(image, std::pair{0,2},
std::pair{0,2})` instead.

## Section 3 — Extents: rank, rank_dynamic, static-vs-dynamic

`std::extents<size_t, N0, N1, ...>` is variadic; each parameter is
either a non-negative integer (static extent, baked into the type) or
`std::dynamic_extent` (dynamic extent, stored at runtime).

```cpp
using ext_mixed = std::extents<std::size_t, 3, std::dynamic_extent>;

ext_mixed::rank()             == 2
ext_mixed::rank_dynamic()     == 1
ext_mixed::static_extent(0)   == 3            // row count is static
ext_mixed::static_extent(1)   == dynamic_extent

md_mixed sm{small, 5};   // 3 rows (from type), 5 cols (from ctor arg)
```

Why mix? Because **static extents typecheck and elide storage**:

```cpp
sizeof(std::mdspan<int, std::extents<size_t, 3, 4>>)   == 8     // pointer only
sizeof(std::mdspan<int, std::dextents<size_t, 2>>)     == 24    // pointer + 2 extents
```

If the shape is known at compile time, the mdspan doesn't carry it at
runtime — just like `std::span<T, N>` carries only a pointer (Jul 9
Section 6). And if you try to construct a static-extent mdspan with
the wrong size, the compiler refuses:

```cpp
std::mdspan<int, std::extents<size_t, 3, 4>> wrong{small, 5, 5};
// static_assert-fail equivalent: requested extent 4, got 5
```

The default-extent rule for `std::extents` is the same as for
`std::span`: if you write `std::extents<size_t, 3, 4>` you get rank-2,
both extents static. If you want all-dynamic, use `dextents<size_t,
rank>`.

## Section 4 — Layout policies

The second template parameter of `std::mdspan` (the one after extents)
is the **mapping policy**. Three ship in `<mdspan>`:

```
layout_right  last index varies fastest   (C / row-major)
layout_left   first index varies fastest  (Fortran / column-major)
layout_stride caller supplies strides     (sub-blocks, transposed, etc.)
```

The mapping turns a multi-index into a single offset into the buffer:

```cpp
int buf[6] = {0, 1, 2, 3, 4, 5};

std::mdspan<int, extents<size_t, 2, 3>, layout_right> rm{buf};
std::mdspan<int, extents<size_t, 2, 3>, layout_left>  cm{buf};

rm[0,0] == buf[0];   rm[0,1] == buf[1];   rm[0,2] == buf[2];
rm[1,0] == buf[3];   rm[1,1] == buf[4];   rm[1,2] == buf[5];

cm[0,0] == buf[0];   cm[1,0] == buf[1];   cm[2,0] == buf[2];
cm[0,1] == buf[3];   cm[1,1] == buf[4];   cm[2,1] == buf[5];
```

The lesson's section 5 prints both views over the same `[0..5]` buffer:

```
row-major:        column-major:
  0 1 2             0 2 4
  3 4 5             1 3 5
```

Same memory. Two completely different visible matrices.

`layout_stride::mapping<extents<T, R, C>>` is constructed explicitly
with both the extents and a stride array:

```cpp
std::array<size_t, 2> strides_cm = {1, 2};   // column-major-ish
std::layout_stride::mapping<std::extents<size_t, 2, 3>> stride_map{
    std::extents<size_t, 2, 3>{},
    strides_cm,
};
std::mdspan<int, extents<size_t, 2, 3>, layout_stride> st{buf, stride_map};
```

The size cost of the policy choice shows up in `sizeof`:

```
sizeof(md_rm)     = 8      (pointer only — strides derived from type)
sizeof(md_cm)     = 8
sizeof(md_strided)= 24     (pointer + 2 extents + 2 strides)
```

`layout_stride` is the only policy that adds runtime storage. The
other two can compute strides from the static extents in the type.

## Section 5 — Manual slicing

Since this libc++ doesn't ship `std::submdspan` yet, the lesson shows
the operation in its primitive form: **construct a new mdspan over a
sub-range of the buffer.** For row-major data the offset is just
`(row_start * cols + col_start)`, and the shape is the new dimensions:

```cpp
int img[4 * 4] = {/* 16 ints, row-major 4x4 */};
std::mdspan<int, dextents<size_t, 2>> image{img, 4, 4};

// TL 2x2
int* tl_ptr = img + 0 * 4 + 0;
std::mdspan<int, dextents<size_t, 2>> tl{tl_ptr, 2, 2};

// middle 2x3
int* mid_ptr = img + 1 * 4 + 0;
std::mdspan<int, dextents<size_t, 2>> mid{mid_ptr, 2, 3};
```

The mutation through the subview lands in the same underlying buffer
(`tl[0,0] = 7777` shows up in `image[0,0]` and `img[0]`). This is the
key property: a submdspan is a non-owning view over a *different
pointer* into the *same buffer* — exactly like `std::span` is a
non-owning view over a pointer-and-length.

When P2630 lands in libc++, this becomes:

```cpp
auto tl = std::submdspan(image, std::pair{0, 2}, std::pair{0, 2});
auto mid_row = std::submdspan(image, 2, std::full_extent);   // rank-1
```

— same semantics, less boilerplate.

## Section 6 — std::span compatibility

mdspan and span are siblings, not enemies. They share a "non-owning
view over a contiguous buffer" contract; the bridge between them is
`data_handle()`:

```cpp
int* raw = m.data_handle();   // the underlying pointer
std::cout << (raw == storage);   // true
```

The name `data_handle()` comes from the policy's "handle" concept —
generic over pointer types — and is one of the design points where
mdspan is more general than span (you can plug in a fancy pointer
type, like an `aligned_accessor` or a custom smart pointer).

The other direction — mdspan → bytes — has a small gotcha:
**`std::as_bytes` only takes `std::span`, not `std::mdspan`.** The
manual bridge is:

```cpp
std::span<const int> flat_m{storage, R * C};   // span over the same buffer
std::span<const std::byte> bytes = std::as_bytes(flat_m);
```

This is the kind of code you'll write in production when you need to
feed mdspan data to a byte-oriented API (memcpy, hashing, IPC).
Round-tripping `as_bytes` back to a `std::span<const int>` (via
`reinterpret_cast`) is also the trick for code that wants to "look
through" the typed view to the bytes underneath.

The lesson's section 6 verifies this end-to-end:

```
std::as_bytes(flat_m).size()    = 48
expected (3*4*sizeof(int))      = 48
```

## Section 7 — Function parameters

Same decay story as `std::span`, but multi-D:

```cpp
auto sum_matrix = [](std::mdspan<const int, dextents<size_t, 2>> m,
                     const char* tag) {
    long sum = 0;
    for (size_t i = 0; i < m.extent(0); ++i)
        for (size_t j = 0; j < m.extent(1); ++j)
            sum += m[i, j];
    std::cout << "[" << tag << "] " << m.extent(0) << "x" << m.extent(1)
              << "  sum=" << sum << "\n";
};
```

Call sites accept everything contiguous:

```
[C-array] 2x3    sum=21
[std::vector] 2x2 sum=100
[std::array] 2x3 sum=2100
```

A function taking `mdspan<const T, dextents<...>>` accepts inputs
from any contiguous storage. **The replacement for the old
`void f(const T*, size_t rows, size_t cols)` three-argument pattern —
error-prone, no type checking, can't be range-for'd — is a single
typed mdspan argument.**

The cost of the call is zero — mdspan is a small POD-ish struct
(8 bytes for static extents, 24 for dynamic), trivially copyable,
passed by value. No allocation, no reference-count bump.

## Section 8 — Comparison vs psp::Span

The arc:

```
psp::Span          (Jun 14)  : rank-1 hand-rolled in C++17
std::span          (Jul  9)  : rank-1 standard, C++20
std::mdspan        (today)   : rank-N standard, C++23
```

All three are non-owning views. The only thing that changes is:

1. **The rank of the indexing operation.** `psp::Span::operator[]`
   and `std::span::operator[]` are 1-D; `std::mdspan::operator[]` is
   rank-N (variadic with `rank()` arguments).
2. **The storage of the mapping.** Span's mapping is trivial
   (pointer + length). Mdspan's mapping is a full template parameter
   that decides how to turn a multi-index into an offset.

The non-ownership, the zero-overhead, and the "view over someone
else's buffer" semantics are identical.

## What I didn't cover (next-session candidates)

- **`std::submdspan` and `std::full_extent_t`** (P2630, post-C++23).
  When this libc++ lands them, the manual slicing in Section 5 becomes
  one line.
- **Custom accessors** (`aligned_accessor`, custom smart-pointer
  accessors). mdspan's `data_handle()` interface is generic over the
  pointer type; you can plug in any handle that supports `[]`.
- **mdspan + std::ranges (C++23).** The C++23 contiguous-range
  overload of `std::span` constructor (`span(R&&)` for a borrowed
  contiguous range) was the same shape change for span; the analogous
  work for mdspan is the C++26 deduction guides for mdspan from a
  multidimensional range.
- **Performance comparison.** Same hypothesis as Jul 9: mdspan should
  compile to the same code as the hand-rolled pointer arithmetic.
  A future lesson could diff the assembly for a tight inner loop.
- **mdspan and the rest of the standard library.** The C++26
  `std::linalg` (P1673) proposal is built on mdspan — matrix
  multiplication, BLAS-style routines. When that lands in libc++,
  it becomes the natural fit for the matrix-shaped problems mdspan
  was designed for.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        └── std_mdspan_cpp23/                                  # NEW (the lesson)
            ├── P-2026-07-10-std-mdspan-cpp23.cpp              #    the tour program
            └── P-2026-07-10-std-mdspan-cpp23.md               #    this file
```

No `psp_span_lib` changes today. No release tag today. The lesson
stands on its own — it's about the C++ language, not the library
pipeline or CI.

## Next steps

The psp_span_lib release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs NSIS or
  .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.5.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform and
  SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow feature
  work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam. The lesson
  copies of `release.yml` and `release_matrix.yml` are ready; the
  deploy copies in `.github/workflows/` are blocked on the PAT.

C++ language threads still open:

- **`std::submdspan`** (P2630) — when libc++ lands it, my Section 5
  becomes one line.
- **Custom accessors for mdspan** — `aligned_accessor`,
  `atomic_accessor`, custom-handle accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear algebra
  built on mdspan.
- **mdspan ↔ std::ranges deduction guides** — span got
  `span(R&&)` deduction guides for contiguous ranges in C++23; mdspan
  will get the analogous multi-D version eventually.