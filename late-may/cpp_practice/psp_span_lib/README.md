# `psp_span_lib` — Petra's Span library, static form

The "real library" shape that complements the `psp_span` header-only
INTERFACE library (Jun 28). Where `psp_span` had nothing but usage
requirements, this one has actual compiled code.

## What's in here

```
psp_span_lib/
├── README.md                          ← this file
├── CMakeLists.txt                     ← the library's own CMake config
├── include/psp_span/span.h            ← the public header (move from psp_span/)
└── src/psp_span_inst.cpp              ← explicit instantiations + anchor
```

The library ships:
- A header at the standard `include/psp_span/span.h` install path, so
  consumers can `#include <psp_span/span.h>` (note the angle-brackets).
- One translation unit, `psp_span_inst.cpp`, that holds explicit
  instantiations of `psp::Span<int>`, `psp::Span<const int>`, and
  `psp::Span<double>`.

The result is `libpsp_span_lib.a`, a static archive with one object file
that contains every member function of every pre-instantiated
specialization.

## How to consume it

```cmake
add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE psp_span_lib)
# ... that's it. The PUBLIC include directory and cxx_std_17 requirement
# propagate automatically from the library's CMake config.
```

To opt into compile-time suppression (per the Jun 29 lesson):

```cpp
#include <psp_span/span.h>
extern template class psp::Span<int>;   // suppress strong defs in this TU
// ... use psp::Span<int> normally; the linker satisfies calls from libpsp_span_lib.a
```

## What's instantiated, and why

```cpp
template class psp::Span<int>;
template class psp::Span<const int>;
template class psp::Span<double>;
```

`int` and `const int` are the dominant specializations in the wild —
you read ints, you pass them around as const. `double` is included to
demonstrate how to extend the list. If a consumer needs `Span<long>`,
`Span<float>`, etc., the library grows by adding lines here.

## What's in `psp_span_inst.cpp` besides the instantiations

A "anchor" function (`keep_anchor`) that exercises the Span API
on TU-local globals. This prevents aggressive linkers from
garbage-collecting the `.o` if no consumer has yet referenced
its symbols at the moment the linker scans `libpsp_span_lib.a`.
The Jun 29 lesson had this in the consumer's `span_helpers_inst.cpp`;
today it lives inside the library, which is the right home for it.

## The shape

```
psp_span (header-only INTERFACE library, 2026-06-28)
   ↓
psp_span_lib (static library with explicit instantiations, 2026-06-30)
```

Both targets live in the parent `CMakeLists.txt`. Consumers of
`psp_span` (the Jun 28 driver) are unchanged. Consumers of
`psp_span_lib` (today's Jun 30 driver) opt into the static archive
and may also declare `extern template` to suppress their own
implicit instantiations.

## Origin / cross-references

- 2026-06-14: `P-2026-06-14-std-span-by-hand.cpp` — original prototype.
- 2026-06-28: `P-2026-06-28-cmake-interface-library.cpp` — header-only
  INTERFACE library, `psp_span/`.
- 2026-06-29: `P-2026-06-29-extern-template.cpp` — consumer-side
  `extern template` suppression with hand-managed instantiation TU.
- 2026-06-30: `P-2026-06-30-psp-span-static-library.cpp` — this one.
  The library owns the instantiation TU; consumers just `target_link_libraries(...)`.
