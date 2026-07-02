# C++ practice 2026-07-02 — `find_package(fmt)` against a real third-party library

## What I set out to learn

The Jul 1 forward-on list put this one first:

> **`find_package(fmt)` for a real third-party header-only dep** —
> `{fmt}` is the canonical third-party formatting library.
> `find_package(fmt REQUIRED)` + `target_link_libraries(... PRIVATE fmt::fmt)`
> is the same shape today's lesson uses, except the package comes from
> Homebrew (or vcpkg/conan) instead of a vendored `psp_span_lib`.

That's today. The Jul 1 lesson taught what `find_package()` actually does
(the Config.cmake / ConfigVersion.cmake / targets.cmake trio, generated
by `configure_package_config_file()`). Today's lesson is the *consumer*
side of the same pattern: someone else's package config files, used
exactly the same way.

The structural move is the same as Jul 1:

```
late-may/cpp_practice/
└── fmt_consumer/                       ← NEW: standalone CMake project
    ├── CMakeLists.txt
    └── P-2026-07-02-fmt-find-package.cpp
```

No `add_subdirectory()` into the parent project. `fmt_consumer/` is its
own CMake project that asks the system for fmt via `find_package()` —
exactly how a third-party project in any repo on disk would do it.

## What I built

A driver that exercises 15 distinct `fmt` features, so the output is
*proof* the link worked, not just "hello world". Highlights:

- **Positional & named args**: `{0}` / `{1}` / `fmt::arg("name", ...)`.
  printf can't do either; std::format supports positional but not named.
- **`fmt::format()` returns `std::string`** (composable), unlike printf.
- **`{:>10}` / `{:*^12}`** — width, alignment, custom fill character.
- **`{:.3f}` / `{:10.4f}`** — float precision and width.
- **`{:d} {:x} {:X} {:o} {:b}`** — all integer bases including binary
  (`{:b}` is fmt-specific; std::format doesn't have it).
- **`{:08d}` / `{:#08x}`** — fill+width on integers, with `#` for the
  `0x` / `0` prefix marker.
- **`fmt::join()`** — iterate any range and apply format spec to each.
- **Container formatting** — `fmt::print("{}", std::vector{...})` works
  out of the box (and prints as `["a", "b", "c"]`).
- **Tuple formatting** — `std::tuple` prints as `(a, b, c)`.
- **Map formatting** — `std::map` prints as `{key: value, ...}`.
- **Chrono** (`fmt/chrono.h`) — `{:%F %T}` strftime-style format for
  time points, and `%H:%M:%S` for durations.
- **Terminal color** (`fmt/color.h`) — `fg(fmt::color::green)` and
  `fmt::emphasis::bold` work in non-tty too (auto-detected).
- **`format_to` / `format_to_n`** — write to a back_insert_iterator or
  a fixed-size buffer (returns the end iterator and a char count).
- **Compile-time format-string check** — `consteval` validation; bad
  format strings are compile errors, not runtime exceptions.

## What I confirmed

### (1) `find_package(fmt)` finds Homebrew's fmt 12.1.0

```text
$ cmake -S late-may/cpp_practice/fmt_consumer -B .../build
-- ASan + UBSan: disabled
-- fmt found:        1
-- fmt version:      12.1.0
-- fmt imported targets: fmt::fmt, fmt::fmt-header-only
-- fmt include dirs:
-- Configuring done (0.3s)
```

`fmt_VERSION` is 12.1.0 — read from Homebrew's `fmt-config-version.cmake`.
Two imported targets are exposed: `fmt::fmt` (compiled, links libfmt)
and `fmt::fmt-header-only` (header-only, slower compile, larger binary).

Note `fmt_INCLUDE_DIRS` is empty in the message. That's because fmt
declares its include path *on the imported target itself* (via
`INTERFACE_INCLUDE_DIRECTORIES` in `fmt-targets.cmake`), so consumers
read it via `target_link_libraries(... fmt::fmt)` rather than the
package-level variable. This is a meaningful difference from the
`psp_span_lib` package — which used a different convention. More on
this below.

### (2) Build links cleanly with `-Wall -Wextra -Wpedantic`

```text
$ cmake --build build -j
[ 50%] Building CXX object CMakeFiles/P-2026-07-02-fmt-find-package.dir/P-2026-07-02-fmt-find-package.cpp.o
[100%] Linking CXX executable P-2026-07-02-fmt-find-package
[100%] Built target P-2026-07-02-fmt-find-package
```

Same flags as the rest of the repo. fmt itself compiles clean under
those flags (no warnings propagated into the consumer build).

### (3) The driver runs and produces the expected output

```text
===== 5. integer bases =====
  dec: 255  hex: ff  HEX: FF  oct: 377  bin: 11111111

===== 8. fmt::join() =====
  join default:  1, 2, 3, 4, 5
  join with pad: 1 | 2 | 3 | 4 | 5

===== 9. formatting containers and tuples =====
  vector<string>: ["find_package", "fmt::fmt", "header-only", "compile-time"]
  tuple:         ("Petra", 20, 3.14)

===== 12. map formatting =====
  map: {"Adam": 35, "Kanopi": 2, "Petra": 20}
```

Every section printed. The driver has 15 numbered sections — a complete
tour of fmt in 200 lines.

### (4) Symbol-level proof: we really linked against fmt 12.x

```text
$ nm build/P-2026-07-02-fmt-find-package | grep v12 | head -3
000000010003c010 D __ZGVZN3fmt3v126detail18get_classic_localeEvE6locale
0000000100005e38 T __ZN3fmt3v1210text_styleoRES1_
0000000100030ed0 T __ZN3fmt3v1210vformat_toI...

$ otool -L build/P-2026-07-02-fmt-find-package
build/P-2026-07-02-fmt-find-package:
    /opt/homebrew/opt/fmt/lib/libfmt.12.dylib (compatibility version 12.0.0, current version 12.1.0)
    /usr/lib/libc++.1.dylib ...
    /usr/lib/libSystem.B.dylib ...
```

Two pieces of evidence: (a) the binary references symbols mangled with
`v12` (the inline namespace fmt uses for ABI versioning), and (b) it
links dynamically against `libfmt.12.dylib` version 12.1.0. That confirms
the actual `libfmt` archive is supplying the implementations, not the
compiler synthesizing them locally.

### (5) Negative test: compile without linking libfmt

To prove the symbols really come from the library and not from the
compiler, I compiled the same driver against fmt's headers but
without `-lfmt`:

```text
$ g++ -std=c++17 -I/opt/homebrew/Cellar/fmt/12.1.0/include /tmp/no_link_test.cpp -o /tmp/no_link_test
Undefined symbols for architecture arm64:
  "fmt::v12::vprint(fmt::v12::basic_string_view<char>,
                    fmt::v12::basic_format_args<fmt::v12::context>)",
   referenced from:
      _main in no_link_test-163455.o
ld: symbol(s) not found for architecture arm64
```

The linker error names the exact unresolved symbol — `fmt::v12::vprint`.
That's the proof: `fmt::print(...)` resolves to `fmt::v12::vprint`,
which is defined in `libfmt.12.dylib`. Without the library, the symbol
is unresolved; with `target_link_libraries(... fmt::fmt)`, it links
cleanly because CMake adds `-lfmt` automatically.

### (6) ASan + UBSan build is clean

```text
$ cmake -S . -B build-asan -DENABLE_ASAN=ON
-- ASan + UBSan: ENABLED
-- fmt found:        1
$ cmake --build build-asan -j
[100%] Built target P-2026-07-02-fmt-find-package
$ ./build-asan/P-2026-07-02-fmt-find-package
... (full output, identical to non-ASan)
[OK] find_package(fmt) consumer ran successfully.
```

ASan + UBSan on the consumer + Homebrew's `libfmt.12.dylib`. No
sanitizer errors. fmt is internally fmt-aware about sanitizer use;
no surprises.

## The interesting difference between this and Jul 1

Yesterday's `psp_span_lib` package exposed include dirs via the
package-level variable (`psp_span_lib_INCLUDE_DIRS`). Today, fmt
exposes them via the *imported target* (`fmt::fmt` carries
`INTERFACE_INCLUDE_DIRECTORIES`). Both work, but the target-level
approach is preferred by modern CMake (CMake ≥ ~3.0):

| Convention                              | Where used                          |
| --------------------------------------- | ----------------------------------- |
| `<pkg>_INCLUDE_DIRS` variable           | Our `psp_span_lib` (Jul 1)          |
| `INTERFACE_INCLUDE_DIRECTORIES` on target | Homebrew's `fmt`, almost every modern package |

Modern CMake preference: always read paths from the *target*, never
from package variables. The reason is that target-level properties
participate in `target_link_libraries()` transitive resolution —
include directories, compile definitions, compile features, and
link flags all flow through the target graph. If you read
`<pkg>_INCLUDE_DIRS` and try to apply it manually, you miss the
compile features and transitive link dependencies. The target graph
is what makes modern CMake "just work" when you change a dependency.

That's why our `CMakeLists.txt` is just:

```cmake
find_package(fmt REQUIRED)
target_link_libraries(P-2026-07-02-fmt-find-package PRIVATE fmt::fmt)
```

— no need to set `target_include_directories(... fmt_INCLUDE_DIRS)`
manually. The target carries everything we need.

## Two more things worth recording

### `fmt::fmt` vs `fmt::fmt-header-only`

`fmt::fmt` is the compiled-library target — it links `libfmt`, which
contains the actual implementations of `format`, `vprint`, etc.
`fmt::fmt-header-only` has the same API but every `format()` is
inlined into the consumer TU. The trade-off:

| Target                  | Compile time       | Binary size         | Link speed   |
| ----------------------- | ------------------ | ------------------- | ------------ |
| `fmt::fmt`              | Fast (header-only for templates) | Small (libfmt is shared) | Fast    |
| `fmt::fmt-header-only`  | Slow (every TU re-parses all of fmt) | Large (each consumer re-emits the impls) | Fast |

We use `fmt::fmt` because we're a regular executable — no reason to
take the compile-time hit. For a header-only library that you *want*
to remain header-only (no compiled dep at all), `fmt::fmt-header-only`
is the right choice.

### Homebrew's fmt is `.dylib`, not `.a`

`otool -L` shows `libfmt.12.dylib`. Our `psp_span_lib` was a static
archive (`.a`). The difference matters:

- Static archive: linked into the consumer's binary. Consumer doesn't
  need libfmt at runtime. ABI is baked in at link time. If libfmt
  changes, no effect on consumers until they relink.
- Dynamic library: shared across all consumers on the system. Smaller
  consumer binaries. But: if Homebrew updates libfmt and changes the
  ABI, every consumer breaks at runtime.

For our learning purpose, either works. The `find_package()` flow
doesn't care which it is — the Config.cmake / targets.cmake files
are essentially identical. The differences are at link time
(`-lfmt` vs `-l:libfmt.a`) and at runtime (system lookup vs no
lookup), but both cases use the same `target_link_libraries(... fmt::fmt)`
syntax in the consumer's CMakeLists.

## What I learned (one paragraph)

The Jul 1 lesson was the *producer* side of `find_package()` — how to
make a library that *can* be found. Today's lesson was the *consumer*
side — how to *use* a library that someone else made findable. The
mechanical answer is two lines: `find_package(<name> REQUIRED)` and
`target_link_libraries(<target> PRIVATE <name>::<name>)`. The actual
lesson is that modern CMake has consolidated all the metadata
(include dirs, compile features, link flags, dependencies) onto the
imported target itself, so the consumer doesn't need to do anything
else — just link the target, and the rest flows through the target
graph.

The structure of today's lesson is also a repeat of Jul 1's lesson:
a standalone subdirectory that doesn't `add_subdirectory()` into the
parent project, that uses `find_package()` to consume a package from
somewhere else on the system. The pattern is the same; only the
identity of the package changed. By the time you've done this twice
(once for our own library, once for an upstream library), the
*pattern* is what you recognize, not the syntax.

## Next Steps

The Jul 1 forward-on list still has two items I haven't done yet:

- **Cross-compile target** — `cmake -DCMAKE_TOOLCHAIN_FILE=...`
  pointing at an embedded ARM toolchain. Today's CMake builds
  the consumer for the host; the same CMakeLists builds for any
  target with the right toolchain file. A learning exercise in
  `<platform>`-vs-`<compiler>`-vs-`<target>` separation, where the
  install rules from Jul 1 become the foundation: install once for
  host, install once for target, consumer picks which prefix to
  look at.

- **`external_project_add()` to consume `psp_span_lib` from a separate
  repo** — shows the "library lives in another git repo, fetched at
  configure time" pattern. Combines Jul 1's install shape with
  CMake's superbuild idiom: the consumer's CMakeLists clones
  `psp_span_lib`, runs its install rules into a local prefix, then
  `find_package()`s it from that prefix. The whole end-to-end story,
  with every step explicit.

Between these two, **cross-compile** is the next-step that combines
cleanly with today's lesson. Cross-compile with the toolchain set up
this way: host = macOS arm64, target = arm-none-eabi (or any other
toolchain). Configure with `-DCMAKE_TOOLCHAIN_FILE=...`, observe
which variables change (CMAKE_SYSTEM_NAME, CMAKE_C_COMPILER,
CMAKE_FIND_ROOT_PATH, etc.), watch `find_package(fmt)` either
succeed (if the toolchain's sysroot has fmt) or fail cleanly. Then
make the same cross-compile work for the install tree from Jul 1.

That's the next step. From this point, the remaining unknown is just
how CMake's toolchain file machinery talks to `find_package()`'s
search paths — which is itself a single, focused lesson.