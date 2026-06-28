# `psp_span` — Petra's header-only Span library

A C++17 hand-rolled clone of C++20's `std::span<T, Extent>`. One header file,
no compiled translation units.

## What's in here

```
psp_span/
├── README.md   ← this file
└── span.h      ← the entire library
```

## How to use it

```cpp
#include "psp_span/span.h"

void print_ints(psp::Span<const int> s);  // takes any contiguous int range

int arr[5] = {1, 2, 3, 4, 5};
print_ints(arr);            // Span<int, 5> deduced, converts to Span<const int>
```

## How to build with CMake

This directory is consumed as a CMake `INTERFACE` library. In the
parent project's `CMakeLists.txt`:

```cmake
add_library(psp_span INTERFACE)
target_include_directories(psp_span INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(psp_span INTERFACE cxx_std_17)
```

Then any consumer target that wants the headers does:

```cmake
add_executable(my_consumer src/main.cpp)
target_link_libraries(my_consumer PRIVATE psp_span)
```

`PRIVATE` is the typical choice for an executable that uses the
header but doesn't pass the dependency along to its own consumers.

## Why `INTERFACE` (and not `STATIC`)

- `STATIC` produces a `libpsp_span.a` that contains compiled `.o` files.
  Header-only libraries have nothing to compile, so the `.a` would be
  empty (or a vacuous archive of nothing).
- `INTERFACE` says "this library is *just* its usage requirements
  (include dirs, compile features, link libs). I don't produce any
  binary artifacts."
- The downstream `target_link_libraries(consumer PRIVATE psp_span)`
  propagates the include path and compile features to the consumer
  without adding anything to its link line.

## Origin

The class was prototyped on 2026-06-14
(`P-2026-06-14-std-span-by-hand.cpp`). On 2026-06-28 it was promoted
to a real header-only library and wired into CMake as `INTERFACE`.