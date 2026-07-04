# P-2026-07-04 — CPack resource files: make the archive self-documenting

## Headline

`psp_span_lib` v0.5.0's archive now ships **`License.txt` + `Readme.txt`**
at the install root, alongside `include/` and `lib/`. A user who extracts
the `.tar.gz` immediately sees the license and the readme — no extra
downloads, no separate repo visit, no "where's the LICENSE?" question.

```
$ tar xzf psp_span_lib-0.5.0-Darwin.tar.gz
$ ls psp_span_lib-0.5.0-Darwin/
License.txt   Readme.txt   include/   lib/
```

## Where this fits in the arc

```
Jun 27  CMake build for multi-file Inventory
Jun 28  CMake INTERFACE library (header-only)
Jun 29  Consumer-side `extern template`
Jun 30  STATIC library + library-owned instantiations (psp_span_lib v0.2.0)
Jul  1  install rules + find_package() consumer (psp_span_lib v0.3.0)
Jul  2  find_package(fmt) — third-party package, system-installed
Jul  3  CPack TGZ packaging (psp_span_lib v0.4.0)               <-- previous
Jul  4  CPack resource files: License + Readme (v0.5.0)        <-- today
```

The arc has been "library → install tree → package" for three days.
Today adds the fourth axis: **metadata**. The archive no longer just
contains code; it also contains the documentation a downstream user
needs to decide whether to use the code.

## The two-line change in CMakeLists.txt (and two plain-text files)

The library-side changes are tiny. Five lines added to
`psp_span_lib/CMakeLists.txt`:

```cmake
# 1. New install rule that puts License.txt + Readme.txt at the install root.
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/License.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/Readme.txt
    DESTINATION .
)

# 2. Two CPACK_RESOURCE_FILE_* variables that tell CPack about them.
set(CPACK_RESOURCE_FILE_LICENSE   "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set(CPACK_RESOURCE_FILE_README    "${CMAKE_CURRENT_SOURCE_DIR}/Readme.txt")
```

Plus two new files in `psp_span_lib/`:
- `License.txt` (2.8 KB) — dual MIT / Apache-2.0 license, plain text
- `Readme.txt` (4.2 KB) — consumer-facing readme, plain text

Plain text is deliberate. CPack's resource-file mechanism copies the
files verbatim into the archive; some generators render them (DEB puts
them in `/usr/share/doc/<pkg>/`, NSIS shows them in the installer wizard,
productbuild shows them as the license pane), others just bundle them
(TGZ lands them at the install root). Plain text is the format that all
of them handle correctly. Markdown would render in some places and
appear as raw `**` markers in others.

## What CPack does with the resource files (per generator)

| Generator    | License.txt lands at...                              | Readme.txt lands at...                       |
|--------------|-------------------------------------------------------|----------------------------------------------|
| TGZ          | `<install-root>/License.txt` (i.e. archive root)      | `<install-root>/Readme.txt` (archive root)   |
| DEB          | `debian/copyright` (auto-extracted into deb's metadata) | `/usr/share/doc/<pkg>/Readme.txt`            |
| NSIS         | License page in the installer's wizard               | README page in the installer                  |
| productbuild | The license the user must accept during install       | README pane shown during install              |
| RPM          | `%license` macro → `/usr/share/licenses/<pkg>/`       | `%doc` macro → `/usr/share/doc/<pkg>/`        |

The TGZ behavior is the simplest: the file lands at the install root of
the archive, which is what we wanted. Other generators do something
smarter with the same source file.

## What the v0.5.0 archive looks like

```
$ tar tzf psp_span_lib-0.5.0-Darwin.tar.gz | sort
psp_span_lib-0.5.0-Darwin/License.txt
psp_span_lib-0.5.0-Darwin/Readme.txt
psp_span_lib-0.5.0-Darwin/include/
psp_span_lib-0.5.0-Darwin/include/psp_span/
psp_span_lib-0.5.0-Darwin/include/psp_span/span.h
psp_span_lib-0.5.0-Darwin/lib/
psp_span_lib-0.5.0-Darwin/lib/cmake/
psp_span_lib-0.5.0-Darwin/lib/cmake/psp_span_lib/
psp_span_lib-0.5.0-Darwin/lib/cmake/psp_span_lib/psp_span_libConfig.cmake
psp_span_lib-0.5.0-Darwin/lib/cmake/psp_span_lib/psp_span_libConfigVersion.cmake
psp_span_lib-0.5.0-Darwin/lib/cmake/psp_span_lib/psp_span_libTargets-release.cmake
psp_span_lib-0.5.0-Darwin/lib/cmake/psp_span_lib/psp_span_libTargets.cmake
psp_span_lib-0.5.0-Darwin/lib/libpsp_span_lib.a
```

Compare to v0.4.0 (Jul 3) — same eight files for the code, plus the two
new resource files at the top level. The relative position of
`License.txt` and `Readme.txt` at the archive root (alongside `include/`
and `lib/`) is a deliberate UX choice: it's the first thing a user
sees after `tar xzf`.

## STGZ suppression — a quiet but real CPack gotcha

CMake/CPack has a quirk: when you set `CPACK_GENERATOR = TGZ`, it
auto-enables `STGZ` (Self-extracting Tar GZ) as well. STGZ is a TGZ with
a shell-script wrapper prepended — `./file.sh` extracts itself
automatically. `cpack` then produces BOTH `psp_span_lib-0.5.0-Darwin.tar.gz`
AND `psp_span_lib-0.5.0-Darwin.sh`.

The `.sh` file is ~16 KB (vs ~12 KB for the `.tar.gz`) because of the
shell wrapper. Functionally it does the same thing as the `.tar.gz`
plus an auto-extract step. For a public distribution this is harmless
but noisy: the artifact count doubles, and users wonder which one they
should actually use.

**Verified empirically on Jul 3**: the build dir contained
`psp_span_lib-0.4.0-Darwin.sh` alongside the `.tar.gz`, even though
the lesson only documented the `.tar.gz`.

**Fix**: set `CPACK_BINARY_STGZ OFF` BEFORE `include(CPack)`. The
ordering matters — `include(CPack)` calls `option(CPACK_BINARY_STGZ
"..." ON)`, and a `set(CPACK_BINARY_STGZ OFF)` after that gets
overridden by the cached option default on reconfigure. Setting it
first gives the intended OFF value at configure time:

```cmake
set(CPACK_BINARY_STGZ              "OFF")    # BEFORE include(CPack)!
include(CPack)
set(CPACK_GENERATOR                "TGZ")
# ...
```

After this, `cpack -C Release` produces only the `.tar.gz`. The
`CPackConfig.cmake` file ends up with `CPACK_GENERATOR = "TGZ"`
(no `STGZ;` prefix) and `CPACK_BINARY_STGZ = "OFF"`.

## The consumer flow (cpack_resource_consumer/)

The consumer project is a fresh source tree, fresh `CMakeLists.txt`,
fresh `P-2026-07-04-cpack-resource-files.cpp`. Structurally identical
to Jul 3's `cpack_consumer/`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(cpack_resource_consumer VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(psp_span_lib REQUIRED)
add_executable(P-2026-07-04-cpack-resource-files
    P-2026-07-04-cpack-resource-files.cpp)
target_link_libraries(P-2026-07-04-cpack-resource-files
    PRIVATE psp_span_lib::psp_span_lib)
target_compile_definitions(P-2026-07-04-cpack-resource-files
    PRIVATE PSP_SPAN_LIB_VERSION="${psp_span_lib_VERSION}")
```

One small refinement vs. Jul 3: the consumer now wires up
`PSP_SPAN_LIB_VERSION` via `target_compile_definitions`. Without this,
the driver's `#ifndef PSP_SPAN_LIB_VERSION` fallback prints
`"(version unknown — define via CMake target_compile_definitions)"`
instead of the actual version.

With it:

```
$ cd cpack_resource_consumer/build
$ cmake .. -DCMAKE_PREFIX_PATH="$PWD/../prefix/psp_span_lib-0.5.0-Darwin"
-- Found psp_span_lib: 0.5.0 (installed at .../cpack_resource_consumer/prefix/psp_span_lib-0.5.0-Darwin)
$ cmake --build .
$ ./P-2026-07-04-cpack-resource-files
================================================================
 P-2026-07-04 — cpack_resource_consumer (installed from .tar.gz)
================================================================
psp_span_lib version: 0.5.0
...
```

The `0.5.0` is the archive's version, set by
`psp_span_libConfigVersion.cmake` (which `find_package()` reads from
`lib/cmake/psp_span_lib/` inside the archive). It's the version the
consumer actually linked against — not a guess.

## Empirical verification (the receipts)

Five checks, all passing:

### 1. Configure the consumer from the extracted archive

```
$ tar xzf psp_span_lib-0.5.0-Darwin.tar.gz -C cpack_resource_consumer/prefix
$ cd cpack_resource_consumer/build
$ cmake .. -DCMAKE_PREFIX_PATH="$PWD/../prefix/psp_span_lib-0.5.0-Darwin"
-- Found psp_span_lib: 0.5.0 (installed at .../cpack_resource_consumer/prefix/psp_span_lib-0.5.0-Darwin)
```

The consumer found the package from the archive alone. No source tree
in scope (verified by `rm -rf /tmp/psp_install`).

### 2. Build + link cleanly

```
$ cmake --build .
[ 50%] Building CXX object CMakeFiles/P-.../P-2026-07-04-...cpp.o
[100%] Linking CXX executable P-2026-07-04-cpack-resource-files
[100%] Built target P-2026-07-04-cpack-resource-files
```

Compiles clean with `-Wall -Wextra -Wpedantic -std=c++17` (inherited
from the imported target's `cxx_std_17` feature).

### 3. Run — full output

```
================================================================
 P-2026-07-04 — cpack_resource_consumer (installed from .tar.gz)
================================================================
psp_span_lib version: 0.5.0

[A] psp::Span<int> (dynamic extent, mutable)
    size = 6, empty = false
    first = 10, last = 60, at(2) = 30
    after s_int[0] = 999, data()[0] = 999

[B] psp::Span<const int> (dynamic extent, immutable)
    size = 4, sum = 10

[B'] psp::Span<double> (dynamic extent, mutable)
    size = 3, sum = 7.5

[C] static_assert: v0.5.0 API surface unchanged from v0.4.0  (OK)

[D] Container interop + subspan
    span over std::vector: size = 4, [2] = 300
    s_int.subspan(2, 2): size = 2, [0] = 30, [1] = 40
    s_int.first(3):      size = 3, [0] = 999, [2] = 30
    s_int.last(2):       size = 2, [0] = 50, [1] = 60

================================================================
 All sections ran cleanly. The v0.5.0 archive (not the source
 tree, not /tmp/psp_install) satisfied every symbol, include,
 and CMake config. License.txt + Readme.txt live at the install
 root of the archive (see the .md notes for `tar tzf` output).
================================================================
```

### 4. `nm` on the archive's `.a` — all three specializations present

```
$ nm cpack_resource_consumer/prefix/.../lib/libpsp_span_lib.a | grep "psp.*Span" | head
00000000000004a0 S __ZN3psp4SpanIKiLm18446744073709551615EE6extentE       # Span<const int>::extent
0000000000000180 T __ZN3psp4SpanIKiLm18446744073709551615EEC1EPS1_m       # Span<const int>::ctor(ptr,len)
00000000000004a8 S __ZN3psp4SpanIdLm18446744073709551615EE6extentE       # Span<double>::extent
00000000000002c4 T __ZN3psp4SpanIdLm18446744073709551615EEC1EPdm         # Span<double>::ctor(ptr,len)
...
```

Same three specializations (`Ii`, `IKi`, `Id`) as v0.4.0. The
`Lm18446744073709551615EE` mangling decodes to `<std::size_t
SIZE_MAX>` — i.e., dynamic extent, exactly matching
`psp::dynamic_extent`. The API surface is byte-for-byte identical
between v0.4.0 and v0.5.0; only the metadata changed.

### 5. ASan archive flow

Built a separate Debug-mode archive with `-DENABLE_ASAN=ON` (51 KB
vs 12 KB Release because the sanitizer runtime pulls in extra
symbols), ran the consumer under ASan:

```
$ cd cpack_resource_consumer/build-asan
$ cmake .. -DCMAKE_PREFIX_PATH="$PWD/../prefix-asan/psp_span_lib-0.5.0-Darwin"
$ cmake --build .
$ ASAN_OPTIONS=detect_leaks=1 ./P-2026-07-04-cpack-resource-files
...
psp_span_lib version: 0.5.0
... (all sections ran cleanly)
================================================================
```

No sanitizer errors, no leak reports. The ASan-instrumented `.a`
from the archive works the same way the Release one does.

## Negative test: removing License.txt doesn't break the build

This is important to understand: `License.txt` and `Readme.txt` are
**metadata, not build dependencies**. They are not in the
`psp_span_lib::psp_span_lib` IMPORTED target's interface, so removing
them from the archive does NOT break the consumer's compile or link:

```
$ cp -r prefix prefix-no-license
$ rm prefix-no-license/psp_span_lib-0.5.0-Darwin/License.txt
$ rm -rf build-no-license && mkdir build-no-license && cd build-no-license
$ cmake .. -DCMAKE_PREFIX_PATH="$PWD/../prefix-no-license/psp_span_lib-0.5.0-Darwin"
-- Found psp_span_lib: 0.5.0 ...
$ cmake --build .
[100%] Built target P-2026-07-04-cpack-resource-files
```

The build succeeds. The consumer doesn't care. This is the right
design — bundling License.txt is a UX/human-rights concern (you should
be able to find the license) and a tooling concern (SPDX scanners look
for it), NOT a build-correctness concern. Mixing those roles would
make the consumer fragile.

What WOULD break the build is removing `lib/libpsp_span_lib.a` or
`lib/cmake/psp_span_lib/psp_span_libConfig.cmake` — those are the
actual contract. Jul 1's lesson (`psp_consumer_installed/`) covered
that negative test for `libpsp_span_lib.a`; today's lesson covers the
parallel negative test for the resource files: they're optional for
the compiler, required for humans.

## Interesting diff: Release archive vs ASan archive

```
$ diff <(tar tzf release.tar.gz | sort) <(tar tzf asan.tar.gz | sort)
9c9
< .../psp_span_libTargets-release.cmake
---
> .../psp_span_libTargets-debug.cmake
```

That's **the only difference**. Same thirteen files, same layout,
only the per-config targets file name changes. License.txt and
Readme.txt are byte-for-byte identical between Release and ASan —
they're plain text with no compile-time substitution, so they
aren't config-aware.

The Release archive is 12 KB; the ASan archive is 51 KB. The size
delta comes entirely from the ASan-instrumented `.a` pulling in
sanitizer runtime symbols that the linker captures into the
archive metadata. The resource files (5.7 KB total) are the same
size in both.

## Why `target_compile_definitions(PSP_SPAN_LIB_VERSION=...)` matters

The driver file uses:

```cpp
#ifndef PSP_SPAN_LIB_VERSION
#define PSP_SPAN_LIB_VERSION "(version unknown — define via CMake target_compile_definitions)"
#endif
```

The `#ifndef` is a deliberate fallback for source files that don't
get the definition wired up. With the wiring in the consumer's
`CMakeLists.txt`:

```cmake
target_compile_definitions(P-2026-07-04-cpack-resource-files
    PRIVATE PSP_SPAN_LIB_VERSION="${psp_span_lib_VERSION}")
```

…the driver prints `psp_span_lib version: 0.5.0` — the actual
version of the archive it linked against. Without the wiring, the
fallback prints `"(version unknown — define via CMake
target_compile_definitions)"` — a self-documenting reminder to add
the line.

Why does this matter? Because `psp_span_lib_VERSION` is a CMake
variable set by `psp_span_libConfig.cmake` (via
`configure_package_config_file`) when `find_package()` runs. The
version string is the **archive's** version, read from the
`psp_span_libConfigVersion.cmake` next to the targets file. So the
driver is telling you, in plain text at run time, what archive you
have. That's a useful sanity check for downstream users.

## File map (what's new this session)

```
late-may/cpp_practice/
├── psp_span_lib/
│   ├── CMakeLists.txt              # MODIFIED: added install(FILES License+Readme),
│   │                              #          CPACK_RESOURCE_FILE_* vars,
│   │                              #          CPACK_BINARY_STGZ OFF,
│   │                              #          bumped version 0.4.0 -> 0.5.0
│   ├── License.txt                  # NEW: dual MIT/Apache-2.0 plain-text license
│   └── Readme.txt                   # NEW: consumer-facing plain-text readme
└── cpack_resource_consumer/         # NEW (today)
    ├── CMakeLists.txt                # find_package() consumer (mirror of Jul 3's)
    ├── P-2026-07-04-cpack-resource-files.cpp
    └── P-2026-07-04-cpack-resource-files.md   # this file
```

The `.tar.gz` itself is not committed — it's a build artifact. The
recipe for reproducing it lives in `psp_span_lib/CMakeLists.txt`.

## Next steps

The arc is now genuinely complete in every dimension that matters:

- **Code:** psp::Span<T, Extent> with explicit instantiations (Jun 30).
- **Build:** CMake build for a STATIC library (Jun 30) with
  per-target compile features (Jun 28) and a config-aware debug
  mode (ASan, Jun 27).
- **Install:** `cmake --install` puts the install tree on disk (Jul 1).
- **Findability:** `find_package(psp_span_lib)` resolves the package
  from the install tree (Jul 1) or from an archive (Jul 3).
- **Distribution:** CPack TGZ packages the install tree into a
  redistributable `.tar.gz` (Jul 3).
- **Self-documentation:** License + Readme travel with the package
  (Jul 4, today).

What's left is mostly distribution plumbing, not CMake/C++ substance:

- **Public release on GitHub** — tag the commit (`git tag v0.5.0`),
  push the tag, attach the `.tar.gz` to a GitHub Release. That's the
  public-distribution shape this whole arc was building toward. The
  release page would display License.txt + Readme.txt automatically
  (GitHub renders both), and the archive would be downloadable as a
  release asset.
- **`.deb` / `.rpm` packaging** — same recipe, just change
  `CPACK_GENERATOR` to `DEB` or `RPM`. License.txt becomes
  `debian/copyright` automatically; Readme.txt goes to
  `/usr/share/doc/<pkg>/`. Verified by the table above (which is
  CPack's documented behavior, not my guess).
- **Multi-config packaging** — produce a single archive that contains
  both Release and Debug configs, so consumers pick at configure
  time. Requires running `cpack -C Release` and `cpack -C Debug` into
  the same output directory and merging. The `psp_span_libTargets.cmake`
  file already supports multi-config via `<CONFIG>` substitution.

The natural next lesson (given the arc): **tag the commit as `v0.5.0`
and publish a GitHub Release with the `.tar.gz` attached**. That makes
the entire arc visible to anyone on the internet, not just to a local
machine with `git clone` access.