# C++ practice 2026-07-01 — `psp_span_lib` is installable; a separate consumer project `find_package()`s it

## What I set out to learn

The Jun 30 forward-on (the *first* of four) was concrete:

> **`install(TARGETS psp_span_lib EXPORT ...)`** — make
> `cmake --install build` produce a real install tree:
> `include/psp_span/span.h` in `prefix/include/`,
> `libpsp_span_lib.a` in `prefix/lib/`, and a CMake
> package config (`psp_span_libConfig.cmake`) so other
> projects can `find_package(psp_span_lib REQUIRED)` and
> use the installed library.

That's today. The Jun 30 lesson set up the `PUBLIC_HEADER`
property and left a commented `$<INSTALL_INTERFACE:include>`
waiting in `psp_span_lib/CMakeLists.txt` — both of which
are the *preconditions* for install rules to work. Today
I land the install rules and prove them with a consumer
project that has no awareness of the library's source tree.

## What I built

The library gains install rules (the `install(TARGETS ...)`
+ `install(FILES ...)` + `install(EXPORT ...)` trio that
turns a build target into a package). A new directory
holds a consumer project that consumes the *installed*
library via `find_package()`:

```
late-may/cpp_practice/
├── CMakeLists.txt              ← unchanged (already calls add_subdirectory(psp_span_lib))
├── psp_span_lib/
│   ├── CMakeLists.txt          ← MODIFIED: install rules + INSTALL_INTERFACE + ConfigVersion
│   ├── psp_span_libConfig.cmake.in   ← NEW: package config template
│   ├── include/psp_span/span.h
│   ├── src/psp_span_inst.cpp
│   └── README.md
│
└── psp_consumer_installed/     ← NEW: standalone consumer project
    ├── CMakeLists.txt          ← uses find_package(psp_span_lib REQUIRED)
    └── P-2026-07-01-cmake-install-find-package.cpp
```

Two design decisions worth flagging up front:

1. **`psp_consumer_installed/` is a separate directory, not a
   subdirectory of `psp_span_lib/`.** This is the structural
   statement of the lesson: a library with install rules is
   *packaged software* — it can be consumed by a project that
   doesn't `add_subdirectory()` it and may live in a different
   repo entirely. Putting the consumer inside the library's
   source tree would muddle that.

2. **Library version bumped 0.2.0 → 0.3.0.** Real bump: today
   the library ships a *new capability* (install + find_package)
   that 0.2.0 didn't. Bumping the version is what tells
   `find_package(... VERSION 0.3)` whether a given installed
   copy is acceptable.

## What I confirmed

### (1) `cmake --install` produces the standard install layout

```text
$ cmake --install build-01 --prefix /…/install-01
-- Installing: /…/install-01/lib/libpsp_span_lib.a
-- Installing: /…/install-01/include/psp_span/span.h
-- Installing: /…/install-01/lib/cmake/psp_span_lib/psp_span_libTargets.cmake
-- Installing: /…/install-01/lib/cmake/psp_span_lib/psp_span_libTargets-debug.cmake
-- Installing: /…/install-01/lib/cmake/psp_span_lib/psp_span_libConfig.cmake
-- Installing: /…/install-01/lib/cmake/psp_span_lib/psp_span_libConfigVersion.cmake
```

Six files in the right places:

```text
install-01/
├── include/
│   └── psp_span/
│       └── span.h                              ← public header (one dir deep!)
└── lib/
    ├── libpsp_span_lib.a                       ← static archive
    └── cmake/
        └── psp_span_lib/
            ├── psp_span_libConfig.cmake
            ├── psp_span_libConfigVersion.cmake
            ├── psp_span_libTargets.cmake
            └── psp_span_libTargets-debug.cmake
```

The `lib/cmake/<pkg>/` subdirectory nesting is what
`find_package(psp_span_lib)` looks for on each entry of
`CMAKE_PREFIX_PATH`. The `psp_span/span.h` (one extra dir
level) is so consumers can write `#include <psp_span/span.h>`
rather than `#include <span.h>` — namespace-style include
paths prevent collisions when a project depends on many
libraries that all might have a `span.h`.

### (2) The standalone consumer finds the installed library

The consumer's CMakeLists has zero knowledge of where
psp_span_lib was built. It just calls `find_package()`:

```cmake
find_package(psp_span_lib REQUIRED)
target_link_libraries(myexe PRIVATE psp_span_lib::psp_span_lib)
```

Configured with `-DCMAKE_PREFIX_PATH=/…/install-01`:

```text
-- Found psp_span_lib: 0.3.0 (installed at /…/install-01)
-- psp_span_lib found at: /…/install-01/lib/cmake/psp_span_lib
-- psp_span_lib version: 0.3.0
```

The two `Found psp_span_lib: ...` lines come from different
sources: the first is the courtesy status line in the
`Config.cmake.in` template, the second is the parent
consumer's `message(STATUS ...)` block. Both pull from
variables set by `find_package()`.

### (3) All three pre-instantiated specializations resolve from the installed archive

The consumer driver (`P-2026-07-01-cmake-install-find-package.cpp`)
exercises `psp::Span<int>`, `psp::Span<const int>`, and
`psp::Span<double>` — the three pre-instantiated specializations
from Jun 30's `psp_span_inst.cpp`. To force the linker to
pull the strong definitions from `libpsp_span_lib.a` (rather
than re-emitting them in the consumer TU), the driver takes
the address of one member function per specialization
(see "Anchor-function boilerplate" below):

```text
[A] psp::Span<int>        size=5  sum(1..5)=15
[B] psp::Span<const int>  size=3  sum(10+20+30)=60
[B] psp::Span<double>     size=4  sum=12.0
[C] sp_int.subspan(1, 3)  size=3  first=2  last=4
[OK] All three pre-instantiated specializations resolved
     from the installed archive. Install tree is complete.
```

### (4) Negative test — removing the .a breaks the link

To prove the archive is *actually* supplying the strong
definitions (and the install isn't just decorative), I moved
the `.a` aside, rebuilt, and observed the failure:

```text
$ mv install-01/lib/libpsp_span_lib.a /tmp/libpsp_span_lib.a.bak
$ cmake --build consumer-build-01
   "Undefined symbols for architecture arm64:
    psp::Span<int, ...>::size() const ..."
make: *** Error 2

$ mv /tmp/libpsp_span_lib.a.bak install-01/lib/libpsp_span_lib.a
$ cmake --build consumer-build-01
[100%] Built target P-2026-07-01-cmake-install-find-package
```

The link failure when the archive is gone is the proof:
without the installed `.a`, the consumer TU's external
references to `Span<int>::size()` etc. can't be resolved
by anyone. With it back, everything links cleanly. The
install is genuinely the *source of symbols*, not a
side-effect of an in-tree build.

### (5) Version-compat policy works (SameMajorVersion is strict)

The generated `psp_span_libConfigVersion.cmake` was made
with `COMPATIBILITY SameMajorVersion`. Verified:

```text
find_package(psp_span_lib 0.3.0 EXACT REQUIRED)   → accepted ✓
find_package(psp_span_lib 0.5.0 REQUIRED)         → rejected ✓ ("not compatible")
find_package(psp_span_lib 1.0.0 EXACT REQUIRED)   → rejected ✓ ("not compatible")
```

Both rejections produce the same diagnostic, which is
the standard CMake "Considered but not accepted" output
listing the installed version and the requested version.

`SameMajorVersion` means: same major (`0.x.y`), any minor/patch.
For library 0.3.0, that means `find_package(psp_span_lib 0.x)`
will accept any `x`. The other policies:

- `AnyNewerVersion` — accept anything ≥ requested. For pre-1.0
  libraries this is usually too permissive (semver treats 0.x as
  "anything goes").
- `SameMinorVersion` — same major AND same minor. Strictest of
  the "any patch" policies.
- `ExactVersion` — bit-for-bit equal.

`SameMajorVersion` is the right default for libraries that haven't
reached 1.0 yet.

## Anchor-function boilerplate (a real lesson)

The driver has to convince the linker to pull `Span<int>::size`,
`Span<int>::data`, etc. from `libpsp_span_lib.a` rather than
emitting them locally. Just calling `sp_int.size()` isn't enough —
the compiler is allowed to inline trivial accessors and never emit
a reference to the out-of-line copy in the archive. If the archive
is never *referenced*, it's never *linked in*, and we'd be
testing the wrong thing.

The fix is to take the address of the member function:

```cpp
auto sz = &psp::Span<int>::size;  // forces an external reference
```

Three things tripped me up on first cut, all worth recording:

**(a) `subspan()` is overloaded** (zero-arg vs `(offset, count)`),
so `&psp::Span<int>::subspan` is ambiguous. `operator[]` is
similarly overloaded (const vs non-const member). You have to
disambiguate with a `static_cast` to a specific pointer-to-member
type. For example:

```cpp
auto sub = static_cast<psp::Span<int> (psp::Span<int>::*)(
                  std::size_t, std::size_t) const noexcept>(
                  &psp::Span<int>::subspan);
```

**(b) Every member of `psp::Span` is `noexcept`.** That means
the pointer-to-member type needs `noexcept` in the trailing
position. `operator[]` is the easiest to forget because we don't
think of subscript accessors as `noexcept` in everyday code. The
Jun 14 design made everything `noexcept` (a `Span` over a
contiguous buffer can't throw), so the compiler enforces it
uniformly. (Confirmed in `span.h` — every member has `noexcept`.)

**(c) `static_cast` of an unused prvalue to a discarded type
warns.** The fix is to bind the cast to an `auto` variable and
`(void)` it. That keeps `-Wunused-value` quiet.

The combined pattern is:

```cpp
auto sz  = static_cast<std::size_t (psp::Span<int>::*)() const noexcept>(&psp::Span<int>::size);     (void)sz;
auto d   = static_cast<int*      (psp::Span<int>::*)() const noexcept>(&psp::Span<int>::data);     (void)d;
auto sub = static_cast<psp::Span<int> (psp::Span<int>::*)(std::size_t, std::size_t) const noexcept>(&psp::Span<int>::subspan); (void)sub;
auto idx = static_cast<int&      (psp::Span<int>::*)(std::size_t) const noexcept>(&psp::Span<int>::operator[]);  (void)idx;
```

Each cast is a single line (modulo the long type). The `(void)`
suppresses the unused-variable warning, even though the variable
isn't truly "unused" — its purpose is the side-effect of taking
the address, which forces an external linkage.

## Two CMake gotchas worth recording

### Gotcha 1: `${PACKAGE_VERSION}` is not a thing

My first cut of the `Config.cmake.in` template printed:

```cmake
message(STATUS "Found psp_span_lib: ${PACKAGE_VERSION} (installed at ${PACKAGE_PREFIX_PATH})")
```

On the consumer side this produced:

```text
-- Found psp_span_lib: (installed at )
```

— empty parens for both. Why?

- `${PACKAGE_VERSION}` is not a standard CMake variable. The
  package version is exposed by `find_package()` as
  `${<pkg>_VERSION}` — i.e. `${psp_span_lib_VERSION}` in our case.
  The `_VERSION` variable is set automatically by `find_package()`
  based on the `ConfigVersion.cmake` file, *not* substituted by
  `configure_package_config_file()`.
- `${PACKAGE_PREFIX_PATH}` is also not a thing. The variable you
  want is `${PACKAGE_PREFIX_DIR}`, which IS set — by the
  `@PACKAGE_INIT@` macro that `configure_package_config_file()`
  expands at the top of the generated `Config.cmake` file
  (look for `get_filename_component(PACKAGE_PREFIX_DIR ...)`).
  It points at the install prefix.

The fix:

```cmake
message(STATUS "Found psp_span_lib: ${psp_span_lib_VERSION} (installed at ${PACKAGE_PREFIX_DIR})")
```

Verified working:

```text
-- Found psp_span_lib: 0.3.0 (installed at /…/install-01)
```

The two gotchas share a root cause: `configure_package_config_file()`
substitutes `INSTALL_*` variables (paths to install dirs), but it
doesn't substitute package *metadata* — that comes from
`find_package()` itself (`<pkg>_FOUND`, `<pkg>_VERSION`,
`<pkg>_DIR`, etc.).

### Gotcha 2: `find_package(... VERSION 0.3)` does NOT default to `EXACT`

On first attempt I configured with `-Dpsp_span_lib_FIND_VERSION=1.0.0
-Dpsp_span_lib_FIND_VERSION_EXACT=ON` and got "Found psp_span_lib:
0.3.0". The variables were unused, with a warning:

```text
Manually-specified variables were not used by the project:
    psp_span_lib_FIND_VERSION
    psp_span_lib_FIND_VERSION_EXACT
```

These `-D` flags don't work for `find_package()`. The version
constraint has to be in the `find_package()` call itself:

```cmake
find_package(psp_span_lib 1.0.0 EXACT REQUIRED)   # cmake error if not exact
find_package(psp_span_lib 0.3.0 REQUIRED)         # accept any 0.3.x (SameMajorVersion)
```

`EXACT` is a separate keyword on `find_package()`, not a separate
variable. The `FIND_VERSION_EXACT` variable is internal state set
*by* the `EXACT` keyword for the version-check machinery to read;
setting it from the command line doesn't do anything because
`find_package()` reads it before processing user variables.

## ASan build is clean

Rebuilt with `-DENABLE_ASAN=ON` for the library, then with
`-fsanitize=address -fsanitize=undefined` flags passed directly
to the consumer (since the consumer doesn't propagate library
sanitizer flags by default — that's the same lesson the Jun 28
notes called out):

```text
$ cmake -S . -B build-asan -DENABLE_ASAN=ON && cmake --build build-asan -j
$ cmake --install build-asan --prefix /…/install-asan
$ cmake -S psp_consumer_installed -B psp_consumer_installed/consumer-build-asan \
      -DCMAKE_PREFIX_PATH=/…/install-asan \
      -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address -fsanitize=undefined"
$ cmake --build psp_consumer_installed/consumer-build-asan -j
$ psp_consumer_installed/consumer-build-asan/P-2026-07-01-cmake-install-find-package
[OK] All three pre-instantiated specializations resolved
     from the installed archive. Install tree is complete.
```

No sanitizer errors. The install rules preserve the sanitizer
configuration the library was built with, so consumers that
link against it get matching code.

## What's in the install tree (annotated)

```
install-01/
├── include/
│   └── psp_span/
│       └── span.h
│             ← from `install(FILES include/psp_span/span.h
│                              DESTINATION include/psp_span)`
│               The `psp_span/` parent dir is what makes
│               `#include <psp_span/span.h>` work (vs `<span.h>`).
│
└── lib/
    ├── libpsp_span_lib.a                  ← from `install(TARGETS ... ARCHIVE)`
    │                                         (~100 KB with debug info; ~7 KB of
    │                                          .text after strip)
    │
    └── cmake/
        └── psp_span_lib/
            ├── psp_span_libConfig.cmake
            │     ← generated by configure_package_config_file() from
            │        psp_span_libConfig.cmake.in. This is what
            │        `find_package(psp_span_lib)` triggers.
            │
            ├── psp_span_libConfigVersion.cmake
            │     ← generated by write_basic_package_version_file().
            │        Version check policy: SameMajorVersion.
            │        This is what makes `find_package(... 0.5)` reject
            │        an installed 0.3.0 with "not compatible".
            │
            ├── psp_span_libTargets.cmake
            │     ← generated by install(EXPORT psp_span_libTargets ...).
            │        Defines the IMPORTED targets (psp_span_lib::psp_span_lib).
            │
            └── psp_span_libTargets-debug.cmake
                  ← per-config variant, generated because the library
                     was built with -DCMAKE_BUILD_TYPE=Debug.
```

The `lib/cmake/<pkg>/` subdirectory is what CMake's
`find_package()` looks for on each entry of
`CMAKE_PREFIX_PATH`. Specifically, CMake searches for
`<prefix>/lib/cmake/<pkg>/<pkg>Config.cmake` — exactly the
file we generated. If you change the destination of the
`Config.cmake` install to anything else, `find_package()`
will silently fail to find the package even though the file
is on disk.

## What I learned (one paragraph)

A library that doesn't ship install rules is a build target,
not a package. Turning it into a package is three CMake
calls (`install(TARGETS ...)`, `install(FILES ...)`,
`install(EXPORT ...)`) plus a tiny template (`<pkg>Config.cmake.in`)
plus a version-check file. Once those exist, any consumer
project — in any repo, in any directory, on any disk — can
`find_package(psp_span_lib REQUIRED)` and use it as if it
were part of CMake itself. The Jun 30 library was a real
C++ library; today's version is a real C++ *package*.

The `Config.cmake.in` template is a one-time cost. After
that, the only maintenance burden is bumping the project
version (CMake's `VERSION` argument) when the library makes
backward-incompatible changes — which is exactly the
semver discipline any packaged software needs anyway.

## Next Steps

The remaining three items from Jun 30's forward-on list,
in roughly increasing order of complexity:

- **`find_package(fmt)` for a real third-party header-only dep**
  — `{fmt}` is the canonical third-party formatting library.
  `find_package(fmt REQUIRED)` + `target_link_libraries(...
  PRIVATE fmt::fmt)` is the same shape today's lesson uses,
  except the package comes from Homebrew (or vcpkg/conan)
  instead of a vendored `psp_span_lib/`. Three lines of CMake
  in a real environment; a chance to see what `find_package()`
  looks like when the package config was generated by the
  upstream library author, not by us.

- **Cross-compile target** — `cmake -DCMAKE_TOOLCHAIN_FILE=...`
  pointing at an embedded ARM toolchain. Today's CMake builds
  the library and driver for the host; the same CMakeLists
  builds for any target with the right toolchain file. A
  learning exercise in `<platform>`-vs-`<compiler>`-vs-`<target>`
  separation, where the install rules from today become the
  foundation: install once for host, install once for target,
  consumer picks which prefix to look at.

- **`external_project_add()` to consume `psp_span_lib` from
  a separate repo** — shows the "library lives in another
  git repo, fetched at configure time" pattern. Combines
  today's install shape with CMake's superbuild idiom:
  the consumer's CMakeLists clones `psp_span_lib`, runs its
  install rules into a local prefix, then `find_package()`s
  it from that prefix. The whole end-to-end story, with
  every step explicit.

Also, a small forward-on from today's lesson itself:

- **Make the install rules platform-aware.** The current
  `install(TARGETS ... ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})`
  works on macOS and Linux but won't put the .a in the right
  place on Windows (where static archives conventionally go in
  `lib/` too — fine — but the per-config variant naming
  differs). Worth a five-minute pass when someone has a
  Windows CI handy.

The biggest open question from today's lesson is **how
tight should the version-compat policy be?** `SameMajorVersion`
is what I picked; for a 0.x library where every minor
version can break ABI, that's possibly too permissive.
`ExactVersion` is too strict (forces every consumer to bump
on every patch release). A library author needs to make a
conscious call here, and the answer depends on whether
you're shipping `0.x.y` (anything goes), `1.x.y` (minor
versions can add features, no break), or `2.x.y+` (major
versions are the only breaking boundary). psp_span_lib is
0.3.0; the right call today is `SameMajorVersion`, but
that's the question to revisit when it hits 1.0.0.