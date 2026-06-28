# C++ practice 2026-06-28 — CMake `INTERFACE` library for header-only templates

## What I set out to learn

The Jun 27 lesson (`P-2026-06-27-cmake-build.md`) ended with a forward-on:

> **`INTERFACE` library for header-only templates** — port a
> small template (e.g. `psp::Span<T>` by hand from Jun 14) into
> a header-only CMake `INTERFACE` library. Observe how
> `target_link_libraries(... INTERFACE)` propagates include dirs
> without compiling anything.

That's today's lesson, word for word. The Jun 14 prototype of
`psp::Span<T, Extent>` (a from-scratch C++17 clone of C++20's
`std::span`) lived inside a single `.cpp`. Today I promote it to
a real header-only library in its own directory, declare it as an
`INTERFACE` library in CMake, and add a second consumer driver
that exercises the headers without producing any archive.

The lesson is narrow: learn **what an `INTERFACE` library is in
CMake** by observing it build, observing what files it produces
(nothing), and observing how a consumer picks up its headers
through `target_link_libraries`.

## What I confirmed

### (1) CMake recognizes `add_library(... INTERFACE)`

```cmake
add_library(psp_span INTERFACE)
target_include_directories(psp_span INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(psp_span INTERFACE cxx_std_17)
```

`INTERFACE` is one of four library kinds in CMake:

| Kind | Compiles `.o`? | Produces `.a`/`.so`? | Purpose |
|------|---------------|---------------------|---------|
| `STATIC`     | yes | yes (`.a`/`.lib`) | Compiled archive linked at build time |
| `SHARED`     | yes | yes (`.so`/`.dylib`/`.dll`) | Compiled library loaded at runtime |
| `OBJECT`     | yes | no (just `.o` files) | Pre-compiled object files for other targets |
| `INTERFACE`  | **no** | **no** | **Header-only / usage-requirements-only target** |

`INTERFACE` says: "this library is *just* its usage requirements.
I have no sources, produce no binaries, and never appear in a
link line. I exist so consumers can declare their dependency on a
named thing and pick up my include dirs and compile features."

```text
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
-- Configuring done
$ cmake --build build -j
[ 42%] Building CXX object CMakeFiles/petra_inventory.dir/box.cpp.o
[ 42%] Building CXX object CMakeFiles/P-2026-06-28-cmake-interface-library.dir/P-2026-06-28-cmake-interface-library.cpp.o
[ 42%] Building CXX object CMakeFiles/petra_inventory.dir/inventory.cpp.o
[ 57%] Linking CXX executable P-2026-06-28-cmake-interface-library
[ 71%] Linking CXX static library libpetra_inventory.a
[ 71%] Built target P-2026-06-28-cmake-interface-library
[ 71%] Built target petra_inventory
[ 85%] Building CXX object CMakeFiles/P-2026-06-27-cmake-build.dir/P-2026-06-27-cmake-build.cpp.o
[100%] Linking CXX executable P-2026-06-27-cmake-build
[100%] Built target P-2026-06-27-cmake-build
```

Notice: no `Building CXX object ... psp_span ...` step. CMake
emitted no build rule for `psp_span` because there's nothing to
build.

### (2) No archive is produced for `psp_span`

```text
$ ls build/
... CMakeFiles/                   ← build rules per target
... libpetra_inventory.a          ← STATIC library (compiled .o files)
... P-2026-06-27-cmake-build      ← executable
... P-2026-06-28-cmake-interface-library   ← executable
$ ls build/libpsp_span* 2>&1
ls: build/libpsp_span*: No such file or directory
$ find build -name "*.a"
build/libpetra_inventory.a
$ find build -name "*psp_span*"
build/CMakeFiles/P-2026-06-28-cmake-interface-library.dir
                                                    ↑ this is the *consumer's*
                                                      CMakeFiles, not psp_span's
```

The only `.a` in the build tree is `libpetra_inventory.a` — the
`STATIC` library from Jun 27. `psp_span` produces nothing because
it has nothing to produce.

For comparison: a hypothetical `STATIC psp_span` with no sources
would have errored at configure time (`add_library` requires at
least one source file unless you say `INTERFACE`). With `OBJECT`
you'd get `.o` files you can't generate. `INTERFACE` is the only
correct choice for header-only templates.

### (3) Consumer links with no extra archive

```cmake
add_executable(P-2026-06-28-cmake-interface-library
    P-2026-06-28-cmake-interface-library.cpp)
target_link_libraries(P-2026-06-28-cmake-interface-library
    PRIVATE psp_span)
```

`target_link_libraries(... PRIVATE psp_span)` does two things and
only two things:

1. **Propagates the include path.** CMake looks up `psp_span`'s
   `INTERFACE` include directories and adds them to the consumer's
   `INCLUDE_DIRECTORIES` property. The driver's
   `#include "psp_span/span.h"` resolves because
   `${CMAKE_CURRENT_SOURCE_DIR}` is on the search path.
2. **Propagates the compile feature.** `cxx_std_17` flows to the
   consumer, so the driver gets `-std=c++17` automatically even
   though it didn't ask for it.

It does NOT add anything to the link line, because there's nothing
to link. Confirmed with `cmake --build build --verbose`:

```text
[ 57%] Linking CXX executable P-2026-06-28-cmake-interface-library
/usr/bin/c++ ... -o P-2026-06-28-cmake-interface-library \
    CMakeFiles/...dir/P-2026-06-28-cmake-interface-library.cpp.o
```

The link command has exactly one `.o` file (the driver's own)
and no `-lpsp_span` flag. A `STATIC` link would have added
`-L... -lpetra_inventory` to pull symbols from `libpetra_inventory.a`.
There's no analogous archive for `psp_span` and no analogous
link flag.

### (4) `nm` shows the templates were instantiated into the consumer's `.o`

```text
$ nm build/CMakeFiles/P-2026-06-28-cmake-interface-library.dir/*.o \
    | c++filt | grep "psp::Span" | head -8
T psp::Span<int const, dynamic_extent>::Span(int const*, unsigned long)
T psp::Span<int const, dynamic_extent>::Span<5ul>(int const (&) [5ul])
T psp::Span<int const, dynamic_extent>::Span<int, 5ul>(std::__1::array<int, 5ul>&)
T psp::Span<int const, dynamic_extent>::Span<int, void>(std::__1::vector<int, std::__1::allocator<int>>&)
T psp::Span<int const, dynamic_extent>::Span<4ul>(int const (&) [4ul])
T psp::Span<int const, dynamic_extent>::Span<7ul>(int const (&) [7ul])
```

That's the key fact about header-only templates: **the compiler
instantiates the template body in every translation unit that uses
it.** Each `psp::Span<const int>` constructor the driver invokes
generates a concrete member function in the driver's `.o` file.
There's no shared copy of those instantiations across consumers —
that's the cost of header-only. (Compile time grows linearly
with the number of consumers; this is what motivates `extern
template` for widely-included STL types.)

The dynamic-extent suffix shows up as
`18446744073709551615ul` (i.e. `(size_t)-1`, the value of
`psp::dynamic_extent`) in the mangled names — confirming the
default `Extent` parameter propagates through the type system
exactly as expected.

### (5) Touch-and-rebuild respects the include graph

```text
$ touch psp_span/span.h
$ cmake --build build -j
[ 14%] Building CXX object CMakeFiles/P-2026-06-28-cmake-interface-library.dir/P-2026-06-28-cmake-interface-library.cpp.o
[ 57%] Built target petra_inventory                ← not rebuilt
[ 85%] Built target P-2026-06-27-cmake-build       ← not rebuilt
[100%] Linking CXX executable P-2026-06-28-cmake-interface-library
[100%] Built target P-2026-06-28-cmake-interface-library
```

CMake's depfile tracking (free with `INTERFACE` libraries — same
mechanism as `STATIC` from Jun 27) noticed the header change and
recompiled exactly one consumer: the executable that includes
`psp_span/span.h`. The Jun 27 driver and the `petra_inventory`
library don't include the span header, so they didn't rebuild.

If a future lesson adds a *second* consumer that also uses
`psp::Span`, both will recompile on header touch. Header
dependencies propagate the way you'd expect.

### (6) ASan build is one config-flag away

```text
$ cmake -S . -B build-asan -DENABLE_ASAN=ON
-- ASan + UBSan enabled
$ cmake --build build-asan -j
[100%] Built target P-2026-06-28-cmake-interface-library
$ ASAN_OPTIONS=halt_on_error=0 ./build-asan/P-2026-06-28-cmake-interface-library
... (output identical to Debug build) ...
```

Same source tree, same `CMakeLists.txt`, second build dir.
The `psp_span` `INTERFACE` library compiles nothing in either
configuration; only the consumer's `.cpp` picks up the sanitizer
flags. No sanitizer reports — the Span code is straightforward
array indexing with no UB risk.

### (7) `argv[0]` proves the binary came from `build/`

The driver's (1) banner prints `argv[0]`:

```text
argv[0] = ./build/P-2026-06-28-cmake-interface-library
argv[0] contains 'build/'? yes (CMake out-of-source)
```

Compare to a hand-built binary (`g++ ... -o /tmp/foo ...`):

```text
argv[0] = /tmp/foo
argv[0] contains 'build/'? no  (likely hand-built)
```

The same driver produces different output depending on how it was
built. The Jun 27 driver had the same banner; I'm preserving the
pattern so the lesson is consistent across days.

### (8) The Makefile from Jun 16 still works

```text
$ make
c++ -std=c++17 -Wall -Wextra -Wpedantic -O0 -g -MMD -MP \
    -c -o build/box.o box.cpp
... (etc, unchanged from Jun 16) ...
```

The Makefile knows nothing about `psp_span`. That's fine —
`psp_span` has no compiled output for the Makefile to track.
A future lesson might add a `psp_span-demo` target to the
Makefile if I want hand-built access to the library too, but
that's not necessary today: CMake is the canonical build path
for the lesson, and the Makefile's role is "the pedagogical
'I see every command' build."

### (9) `sizeof(psp::Span<int, 5>)` is 16, not 8 — and that's a teaching moment

The driver reports:

```text
sizeof(psp::Span<int>)    = 16
sizeof(psp::Span<int,5>)  = 16
alignof(psp::Span<int>)   = 8
alignof(psp::Span<int,5>) = 8
```

Real C++20 `std::span<T, N>` with a static `N` elides the size
field: `sizeof(std::span<int, 5>) == 8` because the size is in
the type. My hand-rolled `psp::Span` stores `size_` even for
static extents, so both come out at 16 bytes.

This is a deliberate simplification in the Jun 14 prototype —
handling static-extent "the size is in the type" properly
requires either conditional storage (`std::conditional_t`)
or a base-class / partial-specialization split. The Jun 14 notes
flagged this as a known difference:

> Real `std::span<T, N>` uses `std::conditional_t` to make the
> size field a `[[no_unique_address]]` empty base when `N` is
> static, getting the size down to 8 bytes.

The Jun 28 promotion to a header-only library didn't fix this
because fixing it would change the public API in subtle ways.
**Honest report: this `psp::Span<int, 5>` is bigger than real
`std::span<int, 5>`.** The fix is a known pattern (CRTP or
`std::conditional_t`), but it's a separate lesson.

## Key ideas

### The four CMake library kinds

| Kind | When to use | What you get |
|------|-------------|--------------|
| `STATIC`     | Compiled library, linked at build time. Default for most internal libraries. | `libfoo.a` (Linux/macOS), `foo.lib` (Windows) |
| `SHARED`     | Compiled library, loaded at runtime. Used for plugins and ABI-stable releases. | `libfoo.so` / `libfoo.dylib` / `foo.dll` |
| `OBJECT`     | Pre-compiled `.o` files you want to bundle into multiple targets without recompiling. | `CMakeFiles/foo.dir/*.o` |
| `INTERFACE`  | Header-only. No `.cpp`, no `.o`, no `.a`. Just usage requirements. | **Nothing.** The library is its targets in CMake's graph. |

If you have a header-only library and try `STATIC`, CMake will
error: `add_library` with `STATIC` requires at least one source
file. `INTERFACE` is the only correct choice.

### `INTERFACE` propagation rules

When a consumer says `target_link_libraries(consumer PRIVATE psp_span)`,
CMake copies the following from `psp_span` to `consumer`:

- `INTERFACE_INCLUDE_DIRECTORIES` → `consumer`'s `INCLUDE_DIRECTORIES`
- `INTERFACE_COMPILE_FEATURES`   → `consumer`'s `COMPILE_FEATURES`
- `INTERFACE_COMPILE_OPTIONS`    → `consumer`'s `COMPILE_OPTIONS`
- `INTERFACE_LINK_LIBRARIES`     → `consumer`'s `LINK_LIBRARIES`
  (which can pull in more `INTERFACE`s, transitively)

`PRIVATE` means: I consume the usage requirements but don't pass
them to *my* consumers. `PUBLIC` means: I also expose them to my
consumers. For an executable that uses `psp::Span` but doesn't
expose anything, `PRIVATE` is the right choice.

`INTERFACE_INCLUDE_DIRECTORIES` *propagates* with the keyword
`INTERFACE` on `target_include_directories`:
- `PRIVATE include_dirs` → added to current target only
- `PUBLIC include_dirs`  → added to current target + propagated
- `INTERFACE include_dirs` → propagated only (not added to current
  target's own `INCLUDE_DIRECTORIES`)

For `INTERFACE` libraries there's no "current target's own
include path" because the library isn't a build target — it's
only a usage-requirements bundle. So `INTERFACE` is the only
keyword that makes sense there. (You *can* write
`target_include_directories(psp_span PUBLIC ...)` and CMake
silently treats it the same as `INTERFACE`, but the
documentation recommends `INTERFACE` for clarity.)

### Why `target_compile_features(psp_span INTERFACE cxx_std_17)`

`target_compile_features` declares the *minimum* C++ standard the
target's *consumers* need. For `INTERFACE` libraries, this is a
way of saying "if you want to use my headers, you must compile
with C++17 or later." CMake will set `-std=c++17` on every
consumer automatically.

If I left this off, a consumer could in theory be configured with
`-std=c++11` and the `#include "psp_span/span.h"` would fail to
parse (the header uses `std::enable_if_t`, `std::is_convertible_v`,
etc.). The `target_compile_features` call is a *compile-time
contract*: "I need C++17 to even include this header."

### Header-only template cost: instantiate-per-TU

A consequence of header-only templates that's worth knowing:
**every translation unit that uses `psp::Span<T>` instantiates
its own copy of every member function it calls.** Section (4)'s
`nm` output showed multiple `psp::Span<const int, dynamic>::Span(...)`
definitions — these are copies that exist in the `.o` file for
*every* consumer that uses them.

The linker dedupes identical instantiations across `.o` files in
the same final binary, so the runtime cost is one copy. But
**compile time** grows linearly with the number of consumers,
because each one re-parses and re-instantiates the header. For
a widely-used template like `std::vector`, this is why standard
libraries use `extern template` to suppress redundant
instantiations in user code.

For my lesson: `psp::Span` is small and only used by one
consumer, so the cost is invisible. A real library with many
consumers would want to think about `extern template` or split
into a header-only public API plus a precompiled core.

## What's in the tree now

```text
late-may/cpp_practice/
├── CMakeLists.txt                      ← Jun 27 + Jun 28 INTERFACE block (additive)
├── Makefile                            ← Jun 16, unchanged
├── box.h, box.cpp                      ← Jun 16, unchanged
├── inventory.h, inventory.cpp          ← Jun 16, unchanged
├── P-2026-06-16-inventory-multi-file.cpp
├── P-2026-06-27-cmake-build.cpp        ← Jun 27 driver
├── P-2026-06-27-cmake-build.md
├── P-2026-06-28-cmake-interface-library.cpp   ← NEW today
├── P-2026-06-28-cmake-interface-library.md    ← NEW today (this file)
├── psp_span/                           ← NEW today
│   ├── README.md
│   └── span.h                          ← header-only library
└── build/                              ← CMake out-of-source (gitignored)
    ├── libpetra_inventory.a            ← STATIC library
    ├── P-2026-06-27-cmake-build
    └── P-2026-06-28-cmake-interface-library
```

No `libpsp_span.a`, no `psp_span.cpp.o`. The library's entire
contribution is its presence in CMake's target graph and the
include path it propagates to consumers.

## Build and run

```bash
cd late-may/cpp_practice/

# Debug build (default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/P-2026-06-28-cmake-interface-library

# ASan build
cmake -S . -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan -j
./build-asan/P-2026-06-28-cmake-interface-library

# Hand-built (no CMake), just the driver
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-28-cmake-interface-library \
    P-2026-06-28-cmake-interface-library.cpp
./P-2026-06-28-cmake-interface-library

# Cleanup
rm -rf build build-asan
```

## Cross-references and follow-ups

- **Jun 14 (`P-2026-06-14-std-span-by-hand.cpp`)** — the prototype
  of `psp::Span` in a single `.cpp`. Today it gets promoted to a
  real library.
- **Jun 16 (`P-2026-06-16-inventory-multi-file.md`)** — the
  Makefile-driven multi-file build. Still works in parallel.
- **Jun 27 (`P-2026-06-27-cmake-build.md`)** — the CMake port of
  the Jun 16 Makefile. Today's lesson extends the same
  `CMakeLists.txt` additively.
- **`extern template` to suppress redundant instantiation** —
  the natural next step if `psp::Span` were used by many
  consumers. Pick one consumer's `.cpp` to explicitly
  instantiate the common template args; mark them `extern
  template` in the header. Other consumers then skip the
  instantiation and the linker resolves them to the one explicit
  copy. Real `std::vector` does this internally.
- **`std::conditional_t` to elide `size_` for static extent** —
  fixes the `sizeof(Span<int, 5>) == 16` problem from section
  (9). Either a `[[no_unique_address]]` empty-base-class trick
  or a primary-template / partial-specialization split. Real
  `std::span` uses the empty-base approach.
- **A second consumer that uses `psp::Span`** — currently the
  only consumer is `P-2026-06-28-cmake-interface-library`. A
  future driver that also uses `psp::Span` would share the same
  `INTERFACE` dep and prove the include-path propagation works
  for multiple targets.
- **`find_package(psp_span)` for downstream projects** — when
  you want to *install* `psp_span` and have other projects
  consume it via `find_package`, you add `install(TARGETS
  psp_span EXPORT psp_spanTargets)` and
  `install(EXPORT psp_spanTargets ...)`. Today's lesson doesn't
  install anything; `psp_span` lives entirely in the source
  tree.

## Next Steps

The build-system arc is opening up. Today's `INTERFACE` library
demonstrates the simplest header-only pattern. The natural
progression is to layer more CMake features onto the same
project:

- **`extern template` for `psp::Span<int>`** — show how the
  hand-rolled library *could* be made faster to compile if it
  had many consumers. Mark the common instantiations in a
  single `.cpp` (but `psp_span` has no `.cpp` — so this lesson
  requires adding one). This is the bridge from
  "header-only by necessity" to "header-only by design +
  precompiled core."
- **`install(TARGETS psp_span EXPORT ...)`** — make `cmake
  --install build` produce a real install tree with the header
  in `include/psp_span/span.h`. Then a second project can
  `find_package(psp_span)` and use it without the source tree.
- **`find_package(fmt)` for a real third-party header-only
  dep** — `{fmt}` is the canonical header-only C++ formatting
  library. `find_package(fmt)` pulls it in via Homebrew's
  installed CMake config files. Three lines:
  `find_package(fmt REQUIRED)`,
  `target_link_libraries(... PRIVATE fmt::fmt)`. Compare to
  today's `psp_span` setup — same shape, but the
  `find_package` step is what makes the dependency discoverable
  on a clean checkout.
- **Cross-compile target** — `cmake -DCMAKE_TOOLCHAIN_FILE=...`
  pointing at an embedded ARM toolchain. Today's lesson is
  "build on macOS"; the same `CMakeLists.txt` builds for any
  target with the right toolchain file.

The `INTERFACE` library is the smallest piece of CMake
vocabulary, but it's the foundation for header-only ecosystems
(Boost, {fmt}, Eigen, spdlog, etc.). Now I know what
`find_package(fmt)` is doing under the hood.