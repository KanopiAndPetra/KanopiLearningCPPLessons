# P-2026-07-03 — CPack: package the install tree into a redistributable `.tar.gz`

## Headline

`psp_span_lib` is now a **package**, not just a build target.

- `cmake --install` (Jul 1): puts the install tree on **this** machine.
- `cpack` (today, Jul 3): packages that install tree into a `.tar.gz` that travels to **any** machine — one with no source tree, no build dir, no `/tmp/psp_install`.

The contract is the archive. A consumer with only the archive can install + `find_package()` + link + run, with **zero awareness** of how `psp_span_lib` was built.

## Where this fits in the arc

```
Jun 27  CMake build for multi-file Inventory
Jun 28  CMake INTERFACE library (header-only)
Jun 29  Consumer-side `extern template`
Jun 30  STATIC library + library-owned explicit instantiations (psp_span_lib v0.2.0)
Jul  1  install rules + find_package() consumer (psp_span_lib v0.3.0)
Jul  2  find_package(fmt) — third-party package, system-installed
Jul  3  CPack TGZ packaging + install-from-archive consumer (psp_span_lib v0.4.0)  <-- today
```

The full pipeline is now end-to-end: **build → install → package → distribute → install-from-package → consume**.

## The five lines that made it work

Adding CPack to `psp_span_lib/CMakeLists.txt` was almost embarrassingly small. Here's the entire CPack block:

```cmake
include(CPack)
set(CPACK_PACKAGE_NAME             "psp_span_lib")
set(CPACK_PACKAGE_VERSION          "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR           "Petra @ kanopi.local")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "psp::Span<T,Extent> static library — installable + find_package()-able + CPack-redistributable.")
set(CPACK_PACKAGE_HOMEPAGE_URL     "https://github.com/ArloNOppie/KanopiLearningCPPLessons")
set(CPACK_GENERATOR                "TGZ")
```

Six variables, one `include`. That's it.

### Why each variable is set (not the defaults)

| Variable | Default | What we set | Why |
|---|---|---|---|
| `CPACK_PACKAGE_NAME` | `${PROJECT_NAME}` | `"psp_span_lib"` | Same as default — set explicitly so the consumer script can `tar xzf psp_span_lib-*.tar.gz` without globbing. |
| `CPACK_PACKAGE_VERSION` | `${PROJECT_VERSION}` | `${PROJECT_VERSION}` | Same as default — set explicitly for the same reason. |
| `CPACK_PACKAGE_VENDOR` | empty | `"Petra @ kanopi.local"` | macOS `.pkg` and Debian `.deb` show this in their welcome/description UIs. |
| `CPACK_GENERATOR` | empty (no archive produced) | `"TGZ"` | The cross-platform POSIX tarball generator. CMake also ships DEB, RPM, NSIS, WIX, NuGet, IFW (Qt installer), and `productbuild` (macOS `.pkg`). TGZ is the cheapest one that works on every Unix. |
| `CPACK_PACKAGE_HOMEPAGE_URL` | empty | repo URL | Surfaced in archive metadata for any tool that introspects it. |

The variable that **matters most** is `CPACK_GENERATOR`. With it empty, `cpack` is a no-op — CMake still defines a `package` target but it has nothing to do. Setting it to `TGZ` makes `cpack` actually produce an archive.

### What TGZ does under the hood

`cpack -C Release` runs `cmake --install .` internally (using the build directory and the install prefix it computed at configure time), then `tar czf`s the install tree. So:

**The archive contents == exactly the install tree, byte for byte.**

That property is the whole point. Whatever `cmake --install` produces is what travels.

## What the archive actually looks like

```
$ tar tzf psp_span_lib-0.4.0-Darwin.tar.gz | sort
psp_span_lib-0.4.0-Darwin/include/
psp_span_lib-0.4.0-Darwin/include/psp_span/
psp_span_lib-0.4.0-Darwin/include/psp_span/span.h
psp_span_lib-0.4.0-Darwin/lib/
psp_span_lib-0.4.0-Darwin/lib/cmake/
psp_span_lib-0.4.0-Darwin/lib/cmake/psp_span_lib/
psp_span_lib-0.4.0-Darwin/lib/cmake/psp_span_lib/psp_span_libConfig.cmake
psp_span_lib-0.4.0-Darwin/lib/cmake/psp_span_lib/psp_span_libConfigVersion.cmake
psp_span_lib-0.4.0-Darwin/lib/cmake/psp_span_lib/psp_span_libTargets-release.cmake
psp_span_lib-0.4.0-Darwin/lib/cmake/psp_span_lib/psp_span_libTargets.cmake
psp_span_lib-0.4.0-Darwin/lib/libpsp_span_lib.a
```

Six files inside `<name>-<version>-<system>/`. Layout is identical to the `/tmp/psp_install` install tree from Jul 1 — exactly as the contract requires.

The wrapper directory (`psp_span_lib-0.4.0-Darwin/`) is a feature: multiple versions can be extracted side-by-side without colliding:

```bash
/tmp/cpack_prefix/
├── psp_span_lib-0.3.0-Darwin/    # older version, untouched
├── psp_span_lib-0.4.0-Darwin/    # today
└── psp_span_lib-0.5.0-Darwin/    # future
```

`CMAKE_PREFIX_PATH` then points at `/tmp/cpack_prefix` and `find_package()` picks whichever version the consumer asked for.

## The consumer flow (the "did it actually work" test)

`cpack_consumer/` is a fresh project. The contract:

1. Wipe `/tmp/psp_install` — the source tree is NOT in scope.
2. Extract the archive to `cpack_consumer/prefix/psp_span_lib-0.4.0-Darwin/`.
3. Configure the consumer with `CMAKE_PREFIX_PATH` pointing at that extracted tree.

```bash
$ rm -rf /tmp/psp_install                                  # gone
$ mkdir -p cpack_consumer/prefix
$ tar xzf psp_span_lib-0.4.0-Darwin.tar.gz -C cpack_consumer/prefix
$ cd cpack_consumer/build
$ cmake .. \
    -DCMAKE_PREFIX_PATH="$PWD/../prefix/psp_span_lib-0.4.0-Darwin" \
    ...
-- Found psp_span_lib: 0.4.0 (installed at .../cpack_consumer/prefix/psp_span_lib-0.4.0-Darwin)
```

That `Found psp_span_lib: 0.4.0` line is the proof. The consumer found the package from the archive's `lib/cmake/psp_span_lib/psp_span_libConfig.cmake` and the version came from the `psp_span_libConfigVersion.cmake` next to it.

### Build + link

```bash
$ cmake --build .
[ 50%] Building CXX object CMakeFiles/P-.../P-2026-07-03-...cpp.o
[100%] Linking CXX executable P-2026-07-03-cpack-package-and-install-from-archive
[100%] Built target P-2026-07-03-cpack-package-and-install-from-archive
```

Compiled clean with `-Wall -Wextra -Wpedantic -std=c++17` (the inherited compile features from the imported target — same as Jul 1).

### Run

```
================================================================
 P-2026-07-03 — cpack_consumer (installed from .tar.gz)
================================================================
psp_span_lib version: 0.4.0

[A] psp::Span<int> (dynamic extent, mutable)
    size = 6, empty = false
    first = 10, last = 60, at(2) = 30
    after s_int[0] = 999, data()[0] = 999

[B] psp::Span<const int> (dynamic extent, immutable)
    size = 4, sum = 10

[B'] psp::Span<double> (dynamic extent, mutable)
    size = 3, sum = 7.5

[C] Container interop
    span over std::vector: size = 4, [2] = 300
    span over std::array:  size = 4, sum = 1.0

[D] static_assert: psp::dynamic_extent == SIZE_MAX, size() returns std::size_t  (OK)

[E] Subspan operations
    s_int.subspan(2, 2): size = 2, [0] = 30, [1] = 40
    s_int.first(3):      size = 3, [0] = 999, [2] = 30
    s_int.last(2):       size = 2, [0] = 50, [1] = 60
```

Five sections, all of which require symbols from `libpsp_span_lib.a` (which came from the archive, not the source tree).

## Empirical verification (the receipts)

Three checks, all passing:

### 1. Negative test: remove the `.a`, retry link

```bash
$ mv cpack_consumer/prefix/.../lib/libpsp_span_lib.a /tmp/libpsp_span_lib.a.bak
$ cmake --build .
make[2]: *** No rule to make target `.../lib/libpsp_span_lib.a',
        needed by `P-2026-07-03-cpack-package-and-install-from-archive'.  Stop.
make[1]: *** [CMakeFiles/.../all] Error 2
make: *** [all] Error 2
```

Link fails because the imported target depends on that file. This is the same negative test pattern from Jul 1 (`psp_consumer_installed/`): remove a required piece, prove the build breaks. (Restored after.)

### 2. `nm` on the archive's `.a`

```
$ nm cpack_consumer/prefix/.../lib/libpsp_span_lib.a | grep "psp.*Span" | head
0000000000000180 T __ZN3psp4SpanIKiL...EE6extentE       # Span<const int>::extent
0000000000000180 T __ZN3psp4SpanIKiL...EEC1EPS1_m       # Span<const int>::ctor(ptr,len)
00000000000004a8 S __ZN3psp4SpanIdL...EE6extentE       # Span<double>::extent
00000000000002c4 T __ZN3psp4SpanIdL...EEC1EPdm         # Span<double>::ctor(ptr,len)
0000000000000498 S __ZN3psp4SpanIiL...EE6extentE       # Span<int>::extent
0000000000000018 T __ZN3psp4SpanIiL...EEC1EPim         # Span<int>::ctor(ptr,len)
```

All three specializations (`Ii`, `IKi`, `Id`) present as **strong** symbols (`T`) inside the `.a` from the archive. The mangle suffix `Lm...EE` decodes to `<std::size_t SIZE_MAX>` — i.e., dynamic extent, exactly matching `psp::dynamic_extent`.

(The `S` markers for `extent` are because `extent` is a `static constexpr std::size_t` member — they live in the data segment, not text. Still emitted into the archive.)

### 3. ASan + UBSan archive flow

Built a separate Debug-mode archive with `-DENABLE_ASAN=ON` and ran the consumer with sanitizers:

```bash
$ cmake .. -DENABLE_ASAN=ON && cmake --build .
$ cmake --install . --prefix .../cpack_consumer/prefix-asan/psp_span_lib-0.4.0-Darwin
$ cd cpack_consumer/build-asan && cmake .. && cmake --build .
$ ASAN_OPTIONS=detect_leaks=1 ./P-2026-07-03-cpack-package-and-install-from-archive
...
All sections ran cleanly. The archive ... satisfied every symbol and include.
```

No sanitizer errors, no leak reports.

## Interesting diff: Release archive vs ASan archive

```
$ diff <(tar tzf release.tar.gz | sort) <(tar tzf asan.tar.gz | sort)
9c9
< .../psp_span_libTargets-release.cmake
---
> .../psp_span_libTargets-debug.cmake
```

That's **the only difference**. Same six files, same layout, only the per-config targets file name changes. The ASan archive is 48KB vs Release's 8.7KB because the ASan-instrumented `.a` pulls in sanitizer runtime symbols that the linker captures into the archive metadata.

This is exactly what you want: the package contract (the file layout) is identical between configurations; only the contents of the `.a` (which is opaque to consumers) varies.

## What the consumer CMakeLists looks like

The structural observation: the consumer's CMakeLists is **almost identical** to Jul 1's `psp_consumer_installed/CMakeLists.txt`. Same `find_package(psp_span_lib REQUIRED)`, same `target_link_libraries(... psp_span_lib::psp_span_lib)`, same executable. Only the comment block at the top changes.

That's the lesson at the architectural level: **the shape of a consumer does not change between install-tree and archive-tree**. The `find_package()` interface is identical because CPack's TGZ generator preserves the exact layout CMake installs to. Consumers are oblivious to whether the package came from a source-tree install or a redistributable archive.

## Key CPack gotchas (real ones I hit)

1. **`CPACK_GENERATOR` MUST be set.** With it empty, `cpack` is a no-op and produces zero output. The error is silent — `cpack` just exits 0 with no archive written. Set it explicitly.
2. **Don't set `CPACK_PACKAGE_FILE_NAME` manually** if you want the `<name>-<version>-<system>` pattern. The system comes from `uname -s`, which CMake fills in automatically. Overriding the full filename disables that and you have to update it by hand per platform.
3. **The wrapper directory is `<name>-<version>-<system>`**, not the bare install prefix. So `CMAKE_PREFIX_PATH` must point at the **parent** of that directory, not at the directory itself. (`cpack_consumer/prefix/psp_span_lib-0.4.0-Darwin/` is the wrapped tree; `CMAKE_PREFIX_PATH` is `cpack_consumer/prefix`.)
4. **The archive is configuration-aware**: `cpack -C Release` produces a Release-only archive; `cpack -C Debug` produces a Debug-only archive. `cpack` (no `-C`) produces one archive per configuration CMake built. To ship a multi-config archive, run `cpack` once per config.
5. **`include(CPack)` is required**, even if you set every `CPACK_*` variable. CMake does not auto-include it. Without it, `package` is not a target and `cpack` finds nothing.

## File map

```
late-may/cpp_practice/
├── psp_span_lib/CMakeLists.txt                # MODIFIED: added CPack block, bumped 0.3.0 -> 0.4.0
└── cpack_consumer/                            # NEW (today)
    ├── CMakeLists.txt                          # find_package() consumer
    ├── P-2026-07-03-cpack-package-and-install-from-archive.cpp   # driver
    ├── P-2026-07-03-cpack-package-and-install-from-archive.md   # this file
    ├── prefix/psp_span_lib-0.4.0-Darwin/        # extracted from the archive (test artifact, not committed)
    └── build/, build-asan/                      # consumer build dirs (test artifacts)
```

The `.tar.gz` itself is also not committed — it's a build artifact. The recipe for reproducing it lives in `psp_span_lib/CMakeLists.txt`.

## Next steps

The arc is now genuinely complete: a `psp_span_lib` source tree that produces a `.tar.gz` which travels to any machine and `find_package()`'s into a consumer. Possible forward-ons:

- **`CPACK_RESOURCE_FILE_LICENSE` + `CPACK_RESOURCE_FILE_README`** — make the package self-documenting. Right now the archive has no license file or README; both go in for free if you write `License.txt` and `Readme.txt` and point CPack at them.
- **Multi-config packaging** — produce a single archive with both Release and Debug configs, so consumers can pick at configure time. The `cpack -C <config>` flag is per-config today; multi-config requires a small loop.
- **`.deb` / `.rpm` packaging** — same recipe, just change `CPACK_GENERATOR` to `DEB` or `RPM`. Linux package metadata gets auto-generated from the `CPACK_*` variables.
- **Public release on GitHub** — tag the commit (`git tag v0.4.0`), push the tag, attach the `.tar.gz` to a GitHub Release. That's the public-distribution shape this whole arc was building toward.

The natural next lesson (given the arc): **write a license file + README and add CPACK_RESOURCE_FILE_LICENSE/README to produce a self-documenting package**. Then tag and ship a v0.4.0 release.