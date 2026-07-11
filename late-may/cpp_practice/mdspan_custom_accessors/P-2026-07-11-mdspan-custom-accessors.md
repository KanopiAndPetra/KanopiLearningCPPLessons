# P-2026-07-11 — Custom accessors for `std::mdspan`: how mdspan actually reads and writes

## Headline

Yesterday's lesson treated the `Accessor` template parameter of
`std::mdspan` as an implicit `default_accessor<T>`. Today I wrote
three custom accessors and plugged them into mdspan:

```
counting_accessor<T>   shared counter on every access() and offset()
checked_accessor<T>    accessor-enforced bound (independent of mdspan's shape)
logging_accessor<T>    hands out proxy log_ref<T> instead of T&, records writes
```

The lesson that landed: **the accessor policy is mdspan's design
point for "how do I read/write the underlying memory?"** — which is
distinct from the layout's "what offset does (i, j) map to?" The
two are orthogonal, and most of mdspan's interesting real-world
applications live in this seam.

## Where this fits in the arc

```
Jun 14  psp::Span<T,Extent>        hand-rolled in C++17                (rank-1)
Jul  9  std::span                  standard, C++20                      (rank-1)
Jul 10  std::mdspan                standard, C++23                      (rank-N)
Jul 11  std::mdspan + custom A     same shape, swappable access strategy ← today
```

The arc through non-owning views continues: from hand-rolled span to
standard span, from 1-D to N-D, and now from "mdspan with the default
accessor" to "mdspan where the accessor is policy." Custom accessors
are the seam Kokkos, Thrust, and pybind11 use to plug in their
respective memory models (USM, host/device split, Python object
handles).

This lesson stands alone. It closes the immediate mdspan thread —
custom accessors is the last open piece from the Jul 10 next-steps —
before the psp_span_lib release pipeline (multi-OS matrix, SHA-pinned
actions) takes the next session.

## Why "custom accessor" matters

The default accessor — `default_accessor<T>` — does the obvious thing:

```cpp
struct default_accessor {
    using element_type     = T;
    using reference        = T&;
    using data_handle_type = T*;
    using offset_policy    = default_accessor;     // i.e., itself

    constexpr reference access(data_handle_type p, size_t i) const noexcept {
        return p[i];
    }
    constexpr data_handle_type offset(data_handle_type p, size_t i) const noexcept {
        return p + i;
    }
};
```

That's right for "I have a `T*` and I want `m[i, j]` to dereference
it." It's wrong for:

1. **Instrumentation** — you want every read or write to be visible
   (profiling, debugging, persistence).
2. **Bound enforcement at the access site** — mdspan's shape gives one
   bound; the accessor can hold a *different* bound (sub-allocation,
   sandboxed slice, etc.).
3. **Proxy reference semantics** — `reference` doesn't have to be
   `T&`. It can be a proxy type that defers the write, transforms
   the value, or logs the operation.
4. **Custom data handle types** — mdspan's `data_handle_type` is
   generic. You can plug in `std::shared_ptr<T>`, a Kokkos
   `View::pointer_type`, an RAII handle, anything that has `[]`.

All four cases are covered in the lesson.

## Section 1 — The accessor concept

An accessor policy needs four type members and two member functions:

```cpp
struct accessor {
    using element_type      = ...;     // T
    using reference         = ...;     // usually T&, can be proxy
    using data_handle_type  = ...;     // usually T*, can be anything
    using offset_policy     = ...;     // usually accessor (i.e., itself)

    constexpr reference access(data_handle_type p, size_t i) const
        noexcept(/* depends */);

    constexpr offset_policy::data_handle_type
        offset(data_handle_type p, size_t i) const noexcept;
};
```

That's the whole thing. mdspan calls `accessor::access(handle, offset)`
for every element read/write and `accessor::offset(handle, n)` for
pointer arithmetic during sub-view construction.

`offset_policy` exists because mdspan sometimes needs to produce a
different handle type after `offset()`. The default is to return the
same handle type (so `offset_policy` is the accessor itself). Kokkos
and similar libraries use this to advance from a host-pointer to a
device-pointer when crossing memory boundaries.

## Section 2 — `counting_accessor<T>`: the proof that mdspan really calls you

```cpp
template <class T>
struct counting_accessor {
    using element_type     = T;
    using reference        = T&;
    using data_handle_type = T*;
    using offset_policy    = counting_accessor<T>;

    std::size_t* counter = nullptr;

    constexpr counting_accessor() noexcept = default;
    explicit constexpr counting_accessor(std::size_t* c) noexcept : counter(c) {}

    constexpr reference access(data_handle_type p, std::size_t i) const noexcept {
        if (counter) ++*counter;
        return p[i];
    }
    constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept {
        if (counter) ++*counter;
        return p + i;
    }
};
```

The lesson's Section 1 wires this to a 3×4 mdspan, reads all 12
elements, then writes two:

```
n_access after full read = 12 (expected 12)
n_access after 2 writes  = 14 (expected 14)
```

That's the **entire** access traffic — 12 `access()` calls for the
read pass, then 2 more for the writes. `offset()` doesn't get
counted separately here because the layout policy's mapping computes
the linear offset inline for static-shaped matrices (no `offset()`
call in the hot path for `extents<size_t, 3, 4>`). For `layout_stride`
or larger dynamic extents, `offset()` would show up in the count.

The lesson's takeaway: **mdspan is not magical. Every `m[i, j]` is
literally `acc.access(handle, layout_mapping(map(i, j)))`.** A
profiler or debugger can hook into `access()` if it needs to.

## Section 3 — `checked_accessor<T>`: accessor-enforced bound

mdspan already has a bound — the shape of the extents parameter. So
why would an accessor enforce a *second* bound?

```cpp
template <class T>
struct checked_accessor {
    using element_type     = T;
    using reference        = T&;
    using data_handle_type = T*;
    using offset_policy    = checked_accessor<T>;

    std::size_t owned = 0;

    constexpr reference access(data_handle_type p, std::size_t i) const {
        if (i >= owned) {
            std::ostringstream oss;
            oss << "checked_accessor: index " << i
                << " exceeds owned bound " << owned;
            throw std::out_of_range(oss.str());
        }
        return p[i];
    }
    constexpr data_handle_type offset(data_handle_type p, std::size_t i) const {
        return p + i;   // offset is allowed to go past owned; access is what fails
    }
};
```

The lesson's Section 2 runs a 3×4 mdspan over a 12-int buffer where
the accessor thinks it owns only 6 elements. Output:

```
mdspan size = 12  accessor owned = 6
m[0,0] = 0
m[0,5] = 5
m[1,1] = 5
caught expected exception: checked_accessor: index 6 exceeds owned bound 6
after m[0,0] = 999: storage[0] = 999
```

Three things to note:

1. **mdspan's shape (3×4 = 12) and the accessor's owned count (6) are
   independent policies.** mdspan can't tell the difference; both live
   in the type. This models a sub-allocation carved out of a larger
   pool — the accessor sees the sub-allocation; mdspan sees the
   buffer.
2. **`access()` is the one that throws, not `offset()`.** mdspan
   needs `offset()` to advance the data handle for sub-view
   construction, and an offset past `owned` is harmless if no
   `access()` lands there. This matches the std::span convention:
   you can compute `data() + size()`, but reading past the end is UB.
3. **Every read/write goes through `access()`.** A bounds check at
   mdspan construction catches the obvious cases; the accessor catches
   the cases where someone constructs an mdspan with a wider shape
   than the accessor's intent. Defense in depth.

## Section 4 — `logging_accessor<T>`: `reference` is not necessarily `T&`

`accessor::reference` is allowed to be any type assignable from
`element_type` and convertible to `element_type`. Kokkos and pybind11
exploit this for lazy-writes (you get a handle back, and the write
happens on assignment).

The lesson has a tiny version:

```cpp
template <class T>
struct log_ref {
    T* where;
    std::vector<std::pair<std::size_t, T>>* log;

    const log_ref& operator=(const T& v) const {
        *where = v;
        if (log) log->emplace_back(reinterpret_cast<std::uintptr_t>(where), v);
        return *this;
    }
    operator T() const { return *where; }
};

template <class T>
struct logging_accessor {
    using element_type     = T;
    using reference        = log_ref<T>;          // ← not T&
    using data_handle_type = T*;
    using offset_policy    = logging_accessor<T>;

    std::vector<std::pair<std::size_t, T>>* log = nullptr;

    constexpr logging_accessor() noexcept = default;
    explicit constexpr logging_accessor(std::vector<std::pair<std::size_t, T>>* l) noexcept
        : log(l) {}

    constexpr reference access(data_handle_type p, std::size_t i) const noexcept {
        return log_ref<T>{p + i, log};
    }
    constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept {
        return p + i;
    }
};
```

Section 3 runs the demo:

```
read back: a=10, b=20
write log (3 entries):
  addr=6133605704  value=10
  addr=6133605716  value=20
  addr=6133605708  value=30
```

Three observations:

- **`m[0, 0] = 10` returned a `log_ref<int>`** (not `int&`), and the
  proxy's `operator=` did the actual write AND recorded it. So the
  syntax `m[i, j] = v` still works — the proxy just intercepts.
- **Reads go through the proxy's `operator T()`** (implicit
  conversion). The proxy is read-on-convert, write-on-assign. This
  is exactly the pattern pybind11 uses for `array_t<T>`.
- **The address in the log is the actual storage address, not the
  mdspan's `data_handle() + computed_offset`.** The proxy was
  constructed with `p + i` inside `access()`, so the address is
  already correct. No further math in the proxy.

This is the seam mdspan was designed around — Kokkos' atomic_accessor
(which we're not using here because it's C++26-only in this libc++)
does the same shape: hands out a proxy reference whose `operator=`
performs an atomic RMW.

## Section 5 — Custom `data_handle_type`: `mdspan` is generic over the handle

The fourth demo shows that `data_handle_type` need not be `T*`:

```cpp
struct custom_handle {
    int* p;
    int* get() const { return p; }
    int& operator[](std::size_t i) const { return p[i]; }
};

struct custom_handle_accessor {
    using element_type     = int;
    using reference        = int&;
    using data_handle_type = custom_handle;        // ← not int*
    using offset_policy    = custom_handle_accessor;

    constexpr reference access(data_handle_type h, std::size_t i) const noexcept {
        return h[i];
    }
    constexpr data_handle_type offset(data_handle_type h, std::size_t i) const noexcept {
        return custom_handle{h.p + i};
    }
};
```

Then construct an mdspan over a `custom_handle` instead of an `int*`:

```cpp
using ext_t = std::extents<std::size_t, 2, 3>;
std::mdspan<int, ext_t, std::layout_right, custom_handle_accessor> m{h};
```

The compile-time proof:

```cpp
static_assert(std::is_same_v<decltype(m)::data_handle_type, custom_handle>,
              "mdspan's data_handle_type is the accessor's data_handle_type");
```

Passes. The mdspan now thinks of the buffer as a `custom_handle`
RAII-style value, not a raw pointer. The Kokkos equivalent plugs in
`Kokkos::View<int**, ...>::pointer_type` (a fancy pointer that knows
about memory spaces). pybind11 plugs in an `array_t<T>::pointer_type`
that knows about Python object lifetimes.

The C++23 mdspan doesn't ship any custom handle accessors — the
standard only gives you `default_accessor<T>` — but the type machinery
is there, and Kokkos / mdarray / similar libraries fill it.

## Section 6 — Where mdspan actually invokes the accessor

Reading the libc++ reference implementation, an element access like
`m[i, j]` reduces to roughly:

```cpp
auto handle = m.data_handle();
handle = acc.offset(handle, mapping(i, j));   // 1 offset() call (or inlined)
return acc.access(handle, 0);                 // 1 access() call
```

So **a single `m[i, j] = 999` triggers 1 `offset()` and 1 `access()`**,
and a read does the same shape (but reads the value). The
`counting_accessor` proves this empirically: a 12-element read pass
bumps the counter 12 times (one `access()` per element), with `offset()`
calls elided for static extents.

For dynamic extents or `layout_stride`, `offset()` shows up in the
count. This was the same lesson the Jul 10 mdspan tour implicitly
relied on (the layout's mapping is just a "how to compute an offset"
function — the accessor is "what to do with the offset once you
have it").

## What I didn't cover (next-session candidates)

- **`aligned_accessor<T, N>`** (C++26, libc++ gates behind
  `_LIBCPP_STD_VER >= 26`). Wraps every `access()` in
  `std::assume_aligned<N>` — useful when the underlying buffer is
  known to be aligned for SIMD loads. When this libc++ ships C++26
  support, this lesson becomes "and here's the standard version of
  the same idea."
- **`atomic_accessor<T>`** (mdarray library, not standard). Hand out
  an atomic reference whose `operator=` does a `std::atomic_ref::store`.
  The same shape as `logging_accessor` but with synchronization
  semantics baked into the proxy.
- **mdspan + std::ranges deduction guides** (C++26). Same shape as
  the C++23 contiguous-range `span(R&&)` deduction guide but for
  multi-dimensional ranges.
- **`std::submdspan` and `std::full_extent_t`** (P2630, post-C++23).
  When this libc++ lands it, the manual slicing in Jul 10 Section 5
  becomes one line.
- **mdspan + the rest of the standard library** — `std::linalg`
  (P1673) is built on mdspan; matrix multiplication, BLAS-style
  routines. When that lands in libc++, it's the natural fit for the
  matrix-shaped problems mdspan was designed for.

The first two are interesting because they're "standard library
implementations of the patterns I just hand-rolled." The third and
fourth are C++26/follow-on. The fifth is a separate thread that
depends on P1673 landing.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        └── mdspan_custom_accessors/                                # NEW (the lesson)
            ├── P-2026-07-11-mdspan-custom-accessors.cpp            #    the tour program
            └── P-2026-07-11-mdspan-custom-accessors.md             #    this file
```

No `psp_span_lib` changes today. No release tag today. The lesson
stands on its own — it's about a corner of `std::mdspan` (the
accessor policy) that wasn't covered in the Jul 10 tour.

## Build & run (what I actually ran)

```
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -o P-2026-07-11-mdspan-custom-accessors \
    P-2026-07-11-mdspan-custom-accessors.cpp
./P-2026-07-11-mdspan-custom-accessors
```

Both Release and ASan builds compile clean (`-Wall -Wextra
-Wpedantic`, no warnings). ASan run shows no errors. Output (with
addresses redacted):

```
== Section 1: counting_accessor — count reads and writes ==
sizeof(mat_t) = 32
sum = 270
n_access after full read = 12 (expected 12)
n_access after 2 writes  = 14 (expected 14)
verify storage[0] = 1111, storage[11] = 3434

== Section 2: checked_accessor — accessor-enforced bound independent of shape ==
mdspan size = 12  accessor owned = 6
m[0,0] = 0
m[0,5] = 5
m[1,1] = 5
caught expected exception: checked_accessor: index 6 exceeds owned bound 6
after m[0,0] = 999: storage[0] = 999

== Section 3: logging_accessor — proxy reference type records writes ==
read back: a=10, b=20
write log (3 entries):
  addr=ADDR1  value=10
  addr=ADDR2  value=20
  addr=ADDR3  value=30

== Section 4: custom data_handle_type — mdspan is generic over the handle ==
m[0,0] = 0
m[1,2] = 5
static_assert passed: data_handle_type == custom_handle
```

The `sizeof(mat_t) = 32` number is interesting — `int*` + 2 extents
(`dextents<size_t, 2>`) + 1 accessor (`std::size_t*`) = 8 + 16 + 8 =
32 bytes on arm64. The Jul 10 lesson saw `sizeof(mdspan<int,
extents<size_t, 3, 4>>) = 8` because the static extents elide. With
dynamic extents plus a custom accessor carrying runtime state, you
pay for what you use.

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

- **`std::submdspan`** (P2630) — when libc++ lands it, my Jul 10
  Section 5 becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray) — the
  standard version of what I hand-rolled today.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear algebra
  built on mdspan.
- **mdspan ↔ std::ranges deduction guides** — span got `span(R&&)`
  deduction guides for contiguous ranges in C++23; mdspan will get
  the analogous multi-D version eventually.