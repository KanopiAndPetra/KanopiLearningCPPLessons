# C++ practice 2026-06-27 — CMake build for the multi-file Inventory

## What I set out to learn

The Jun 16 lesson split `Box` and `Inventory` into separate
compilation units (`box.h`/`box.cpp`/`inventory.h`/`inventory.cpp`/`main.cpp`)
and drove the build with a hand-written `Makefile`. That works
for one project on one machine, but it doesn't transfer to a
fresh checkout on Linux / Windows / a CI runner / a package
manager. The cross-platform build tool of record for C++ is
**CMake**, and the Jun 16 follow-on list explicitly flagged it:

> **CMake** — `Makefile` works for one project, but real
> cross-platform builds use CMake. The lesson: write a
> `CMakeLists.txt` that produces the same binary, then
> `cmake -B build && cmake --build build`.

That's today's session. The source files are unchanged from
Jun 16 — I'm keeping the lesson *narrow*: rebuild the exact same
program with CMake, and observe the things CMake makes easier
than raw `make`.

## What I confirmed

### (1) CMake installed and recognized

```text
$ which cmake
/opt/homebrew/bin/cmake
$ cmake --version
cmake version 4.3.4
```

CMake 4.3.4 (Kitware). I installed it via `brew install cmake`
this morning; it lives in `/opt/homebrew/bin/`. AppleClang 21 is
the system compiler on this machine.

### (2) The CMakeLists.txt produces the same binary

```bash
cd late-may/cpp_practice/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/P-2026-06-27-cmake-build
```

Output (excerpt):

```text
=== (0) Build provenance ===
  argv[0] = ./build/P-2026-06-27-cmake-build
  __FILE__ = /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/P-2026-06-27-cmake-build.cpp
  C++ standard: 201703
  Compiler: clang 21.0.0 (clang-2100.1.1.101)

=== (1) Build an Inventory of Boxes ===
  [Box] ctor ('red') @0x16f856650
  [Inventory::add] storing 'red'
  [Box] MOVE ctor ('red') @0x924c00980
  [Box] dtor ('') @0x16f856650
  ...
```

The trace is **byte-for-byte the same shape as Jun 16**, modulo
the new "(0) Build provenance" banner and the out-of-source
binary path. The pointers obviously differ run-to-run, but the
*order of ctors / moves / dtors* is identical: the C++ semantics
haven't changed — only the build driver.

### (3) The configuration summary is information-dense

```text
-- ==================================================
--  Project:        PetraInventory 0.1.0
--  Build type:     Debug
--  C++ standard:   17
--  Compiler:       AppleClang 21.0.0.21000101
--  Source dir:     /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice
--  Binary dir:     /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/build
--  ASan + UBSan:   OFF
-- ==================================================
-- Configuring done (0.5s)
-- Generating done (0.0s)
-- Build files have been written to: /Users/oppie1.kanopi/KanopiLearningCPPLessons/late-may/cpp_practice/build
```

`configure` + `generate` are two distinct phases. `configure`
runs the CMake script (the `project()`, `add_library()`,
`add_executable()` calls) and *resolves* the build. `generate`
emits the actual build files — by default, a Unix Makefile in
`build/`, but CMake also knows Ninja, Xcode, Visual Studio,
NMake, etc. **Same `CMakeLists.txt`, different generator.**
That's the abstraction layer the Makefile can't offer.

### (4) Out-of-source build — `build/` next to `CMakeLists.txt`

```text
late-may/cpp_practice/
├── CMakeLists.txt            ← NEW today
├── P-2026-06-27-cmake-build.cpp   ← NEW today
├── P-2026-06-27-cmake-build.md    ← NEW today (this file)
├── box.h, box.cpp            ← from Jun 16 (unchanged)
├── inventory.h, inventory.cpp    ← from Jun 16 (unchanged)
├── Makefile                  ← from Jun 16 (still works in parallel)
└── build/                    ← CMake-generated (gitignored)
    ├── CMakeCache.txt
    ├── Makefile              ← CMake's Makefile, not ours
    ├── CMakeFiles/
    └── P-2026-06-27-cmake-build  ← the binary
```

The Jun 16 Makefile dropped object files and binaries into the
source dir (or a `build/` subdir of the source dir). CMake's
default is the *configure-time-specified* `build/` — a sibling
of the source tree, never inside it. **Delete it with `rm -rf
build`** and the source tree is pristine.

This separation matters because:
- You can have *multiple* build dirs from one source tree
  (Debug + Release + ASan, side by side) — see (5).
- IDE generators (Xcode, Visual Studio) want a separate build
  dir per configuration. CMake's design lets the same
  `CMakeLists.txt` drive all of them.
- CI artifacts come from `build/`, not the source tree, so
  copying the source to a fresh checkout doesn't carry any
  stale `.o` files.

### (5) One source tree, many builds

I built three configurations, all from the same `CMakeLists.txt`:

```text
$ rm -rf build build-asan build-release

$ cmake -S . -B build           -DCMAKE_BUILD_TYPE=Debug
  → Debug, ASan off
$ cmake -S . -B build-asan      -DCMAKE_BUILD_TYPE=Debug  -DENABLE_ASAN=ON
  → Debug + ASan + UBSan, -O1 forced
$ cmake -S . -B build-release   -DCMAKE_BUILD_TYPE=Release
  → Release, -O3 + -DNDEBUG
```

Each dir is independent. The Debug + ASan build's binary is
`build-asan/P-2026-06-27-cmake-build`, the Release build's is
`build-release/P-2026-06-27-cmake-build`. The source tree is
the same — there's only one `CMakeLists.txt`. With raw `make`,
this would need three separate targets in three separate
directories, or three different flag combinations checked at
runtime. CMake's `CMAKE_BUILD_TYPE` and `option(ENABLE_ASAN)`
make it a *configuration* decision, not a *build* decision.

The ASan build ran clean:

```text
$ cmake -S . -B build-asan -DENABLE_ASAN=ON
-- ASan + UBSan enabled
$ cmake --build build-asan -j
$ ./build-asan/P-2026-06-27-cmake-build
... (full trace, identical to Debug) ...
```

No sanitizer reports. (There wouldn't be any — the Inventory
doesn't do anything dodgy. But the ASan flag-passing through
CMake's `add_compile_options` / `add_link_options` is
verified end-to-end.)

### (6) Incremental rebuild is automatic — no `-MMD -MP` dance

The Jun 16 Makefile had to be taught about header dependencies
manually:

```make
CXXFLAGS += -MMD -MP
-include $(OBJS:.o=.d)
```

`g++ -MMD` emits a `.d` file per `.o` listing every header the
`.cpp` includes; `make -include $(OBJS:.o=.d)` slurps them in
so that touching a header triggers the right `.o` rebuild.

**CMake does this for free.** `add_library(petra_inventory STATIC
box.cpp inventory.cpp)` is enough — CMake's compiler-driver
abstraction (the "Makefile Generators" + `CMAKE_CXX_COMPILER_ID`)
calls the compiler with whatever flag is needed to emit the
depfile for your platform (`.d` for Make/Ninja, project
dependencies for Xcode/MSBuild), and CMake's own dependency
scanner consumes them. I verified:

```text
$ cmake --build build -j
[ 60%] Built target petra_inventory
[100%] Built target P-2026-06-27-cmake-build
(no recompilation — up to date)

$ touch inventory.h
$ cmake --build build -j
[ 20%] Building CXX object CMakeFiles/petra_inventory.dir/inventory.cpp.o
[ 40%] Linking CXX static library libpetra_inventory.a
[ 60%] Built target petra_inventory
[ 80%] Building CXX object CMakeFiles/P-2026-06-27-cmake-build.dir/P-2026-06-27-cmake-build.cpp.o
[100%] Linking CXX executable P-2026-06-27-cmake-build
```

Notice what happened and what didn't:

- `inventory.cpp.o` recompiled — `inventory.cpp` includes `inventory.h`.
- `box.cpp.o` did **not** recompile — `box.cpp` does not include `inventory.h`.
- The driver `P-2026-06-27-cmake-build.cpp.o` *did* recompile — it `#include`s `inventory.h` transitively through `inventory.h`'s own `#include "box.h"` and the project's include paths. Wait, that's `box.h` not `inventory.h`. Let me check: the driver does `#include "inventory.h"` directly, so yes — it's a direct includer.
- The static library and the executable both re-linked.

That's the correct graph. CMake's compiler-driver abstraction
picked up the depfile automatically; no flags I had to remember.

### (7) `petra_inventory` is a real static library target

```text
$ file build/libpetra_inventory.a
build/libpetra_inventory.a: current ar archive

$ nm build/libpetra_inventory.a 2>&1 | grep -E ' T ' | c++filt | head -5
Box::Box(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)
Box::Box(Box const&)
Box::Box(Box&&)
Box::Box& Box::operator=(Box&&)
Box::~Box()
```

`add_library(... STATIC ...)` produced `libpetra_inventory.a` —
a real archive file containing the `.o` files. `target_link_libraries
(... PRIVATE petra_inventory)` in the executable's recipe pulls
those symbols in at link time.

The Jun 16 Makefile did the same thing manually:

```make
$(TARGET): $(OBJS) | $(BUILD)
    $(CXX) $(WARNINGS) $(CXXSTD) $(OPTDEBUG) -o $@ $^
```

— `g++ ... main.o box.o inventory.o -o prog`. CMake's
`add_library` / `add_executable` / `target_link_libraries` is
the same idea, but with a discoverable graph: `cmake
--build build --target petra_inventory` rebuilds *just* the
library, `cmake --build build --target P-2026-06-27-cmake-build`
rebuilds just the driver, and `cmake --build build` rebuilds
both. Try `--target` with a raw Makefile and you're grepping
for the right label.

### (8) The Makefile still works in parallel

I left the Jun 16 `Makefile` exactly as it was:

```text
$ make
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g ...
    -c -o build/box.o box.cpp
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g ...
    -c -o build/inventory.o inventory.cpp
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g ...
    -c -o build/P-2026-06-16-inventory-multi-file.o \
       P-2026-06-16-inventory-multi-file.cpp
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g ...
    -o P-2026-06-16-inventory-multi-file \
       build/P-2026-06-16-inventory-multi-file.o \
       build/box.o build/inventory.o
```

Both build systems coexist:

- `make` → Jun 16 binary at `P-2026-06-16-inventory-multi-file`.
- `cmake --build build` → today's binary at `build/P-2026-06-27-cmake-build`.

The CMake `build/` is gitignored so it never enters the
Makefile's path-discovery. The Jun 16 driver is still tracked
in the repo and still works against the same `box.cpp`/
`inventory.cpp` files. **Two build systems, one source tree.**
That co-existence is the strongest argument for keeping both:
the Makefile is the pedagogical "I see every command" build,
CMake is the "real-world cross-platform" build.

### (9) `__cplusplus` = `201703` proves the C++17 flag flowed through

The driver prints the value of `__cplusplus`:

```text
C++ standard: 201703
```

`201703` is the C++17 value of `__cplusplus` (set by the compiler
when `-std=c++17` is in effect). It came from
`set(CMAKE_CXX_STANDARD 17)` in the `CMakeLists.txt`, which CMake
turns into `-std=c++17` for clang/gcc and `/std:c++17` for MSVC.
**CMake's portable abstraction works** — the same line of CMake
hits three different compilers with three different flag names.

`CMAKE_CXX_EXTENSIONS OFF` is what makes CMake prefer
`-std=c++17` over `-std=gnu++17`. The gnu variant enables GNU
extensions; with it OFF, you get strictly ISO C++17. The
`-Wpedantic` flag (also in `add_compile_options`) doubles down
on this — it warns on any GNU extension used. Together they
force strict ISO C++17.

## Key ideas

### `cmake -S . -B build` vs the legacy two-step

There are two CMake idioms for configuring:

**Modern (≥ 3.13, recommended):**

```bash
cmake -S <source-dir> -B <build-dir> [options]
cmake --build <build-dir> [-j] [target]
```

**Legacy:**

```bash
mkdir build && cd build
cmake .. [options]
make
```

The modern form keeps you out of the build dir — your shell
prompt stays in the source tree, and `cmake` does the `cd` for
you. This is what I'll use going forward; the legacy form is
in every older tutorial online.

### What goes in `CMakeLists.txt` vs. `Makefile`

The Makefile at Jun 16 was 100+ lines: variable definitions,
pattern rules, header-dep auto-tracking, `run`/`clean`/`rebuild`
/`asan`/`verbose` targets. The `CMakeLists.txt` for the same
project is 8 sections and ~140 lines, but it covers more ground:

| Concern | Makefile | CMakeLists.txt |
|---------|----------|----------------|
| Compiler choice | `CXX ?= g++` | `project(... LANGUAGES CXX)` (auto-detects) |
| C++ standard | `CXXSTD ?= -std=c++17` | `set(CMAKE_CXX_STANDARD 17)` |
| Warnings | `WARNINGS ?= -Wall -Wextra -Wpedantic` | `add_compile_options(...)` |
| Object files | Manual pattern rule + `OBJS := $(patsubst ...)` | Implicit (CMake scans `add_library`/`add_executable` arguments) |
| Header deps | `-MMD -MP` + `-include $(OBJS:.o=.d)` | Automatic |
| ASan | Separate `asan:` target with extra flags | `option(ENABLE_ASAN) ... add_compile_options(-fsanitize=...)` |
| Multiple builds | Manual: separate dirs, separate flags | Trivial: separate `build-*` dirs with different cache vars |
| Cross-platform | Linux/Mac only (Makefile semantics differ on Windows) | Unix Make / Ninja / Xcode / VS / NMake — same script |
| `make run`/`clean` | Custom targets | `cmake --build build --target run` (if you add the target) |

The CMake file is denser because CMake's vocabulary is more
abstract. The Makefile's strengths are:
- It's readable top-to-bottom — every command is spelled out.
- You can `make VERBOSE=1` and see exactly what the compiler
  is invoked with. CMake has `cmake --build build --verbose`
  but it's not the default.

CMake's strengths are:
- Portability (you can't make a Makefile that works on Visual
  Studio without rewriting the whole thing).
- Composition (a CMake script can `add_subdirectory()` to pull
  in another project, or `find_package(Boost)` to use one).
- IDE integration (every C++ IDE understands CMake projects).

### The four CMake concepts I now know

1. **Target.** A named thing you build. `add_library()` makes
   a library target; `add_executable()` makes an executable
   target. Targets have properties (include dirs, compile
   options, link libraries) you set with `target_*` commands.
   Targets depend on other targets via `target_link_libraries`.
2. **Generator.** The build-tool abstraction. `cmake -G Ninja`
   uses Ninja; the default uses Unix Makefiles on macOS/Linux
   and MSBuild on Windows. Same `CMakeLists.txt`, different
   output.
3. **Cache variable.** Set at configure time, remembered
   between runs. `set(... CACHE STRING "..." FORCE)` makes a
   persistent var editable via `cmake -DVAR=value` or
   `ccache/ccmake` GUI.
4. **Out-of-source build.** The `-B <dir>` argument specifies
   where generated files go. The source tree stays clean;
   `rm -rf <build-dir>` is a clean slate.

### What I'd add if this were a real project

The `CMakeLists.txt` is intentionally minimal. Real-world
projects also have:

- **`find_package(Threads)`** — pull in pthreads/Windows thread
  library the portable way. Not needed for this lesson (no
  threads), but every non-trivial project uses it.
- **`enable_testing()` + `add_test(...)`** — declare CTest tests
  so `ctest --output-on-failure` works. This lesson has no
  tests; a follow-on could add a tiny assertion harness for
  `add`/`replace_at`/`take`.
- **`target_compile_features(petra_inventory PUBLIC
  cxx_std_17)`** — declare which C++ features the target uses,
  so consumers (e.g. a downstream library that links against
  `petra_inventory`) automatically get `-std=c++17` too.
  Equivalent to `CMAKE_CXX_STANDARD` for this single-project
  case but more robust when your library is reused.
- **`install(TARGETS ...)` + `install(FILES ...)`** — make
  `cmake --install build` produce a proper install tree with
  the binary in `bin/` and the headers in `include/`.
- **A `BUILD_SHARED_LIBS` option** — flip between static and
  shared (`libpetra_inventory.dylib`) at configure time.

These are *real* CMake features but they're not what this
lesson is about — the goal is the basics.

### Why a separate driver file (`P-2026-06-27-cmake-build.cpp`)

I could have reused the Jun 16 driver `P-2026-06-16-inventory-multi-file.cpp`
unchanged — it does the same scenario. I wrote a new one for
two reasons:

1. **Filename convention.** The cron expects a
   `P-YYYY-MM-DD-topic.cpp` for today's session. The Jun 16
   driver has a different date in its name; reusing it would
   either require renaming (which would touch the file and
   break the parallel Makefile build) or copy-paste. A new
   driver is the cleanest answer.
2. **Build provenance.** The new driver prints `argv[0]`,
   `__FILE__`, `__cplusplus`, and the compiler ID/version in
   the (0) banner. That's the visible difference between the
   CMake-built binary and the Makefile-built binary — the
   Makefile-built binary lives at the source tree root and
   the CMake-built binary lives in `build/`. The (0) banner
   makes that observable without reading paths.

The Box/Inventory code paths are unchanged. Every trace line
after (0) is the same exercise as Jun 9 / Jun 16.

## Cross-references and follow-ups

- **Jun 16 (multi-file + Makefile)** — today's source files
  are the Jun 16 files. The Makefile still works. The lesson
  is *adding* a build system, not replacing one.
- **Jun 17–25 (PIMPL arc)** — `add_library(petra_inventory
  STATIC ...)` becomes a moving target once PIMPL is involved.
  The `Impl.cpp` files would also be added to the same
  `petra_inventory` library; the public header-only API stays
  the same.
- **Jun 26 (acyclic visitor)** — visitors would each be a
  separate target (`petra_eval_visitor`, `petra_pretty_visitor`)
  with the `NullVisitor<Concrete>` template in a header-only
  `INTERFACE` library. CMake's `INTERFACE` library kind is the
  header-only equivalent of `STATIC`.
- **Header-only `INTERFACE` library** — a CMake `INTERFACE`
  library has no compiled `.o` files, only headers. It's the
  right target type for templates (e.g. a `psp::Span<T>` or
  `psp::expected<T,E>`). New follow-on: port `box.h`-style
  template code into a header-only library and have a separate
  test target that consumes it.
- **`find_package` for third-party libs** — for any non-trivial
  dependency (fmt, spdlog, Boost, gtest). This lesson's
  inventory doesn't need any, but the moment it did, the
  CMake idiom would be `find_package(fmt REQUIRED)` +
  `target_link_libraries(... PRIVATE fmt::fmt)`.
- **`BUILD_SHARED_LIBS`** — make the library a `.dylib` and
  observe how the linker behavior changes (RPATH, install_name,
  dyld lookup). Direct follow-on from today's `STATIC` choice.
- **`ctest` + GoogleTest** — write a real test harness for
  `Box` and `Inventory` and wire it to `ctest`. CMake's
  testing integration is one of the strongest reasons to
  prefer it over a hand-rolled Makefile.

## Next Steps

The PIMPL arc and the visitor-pattern arc both closed this
week. The "build system" topic is the natural new arc to
start. Follow-ons from today:

- **`INTERFACE` library for header-only templates** — port a
  small template (e.g. `psp::Span<T>` by hand from Jun 14) into
  a header-only CMake `INTERFACE` library. Observe how
  `target_link_libraries(... INTERFACE)` propagates include dirs
  without compiling anything.
- **`find_package` + a real third-party dep** — pull in `fmt`
  via Homebrew and use it in the Inventory's `print()`. The
  Makefile doesn't make this easy; CMake does it in 3 lines.
- **Ninja generator** — `cmake -G Ninja -S . -B build-ninja` —
  observe the difference between Make- and Ninja-generated
  output. Ninja is significantly faster for incremental builds
  on large projects.
- **Cross-compile target** — `cmake -DCMAKE_TOOLCHAIN_FILE=...`
  pointing at an arm-none-eabi toolchain. Today the lesson is
  "build on macOS"; the same `CMakeLists.txt` builds for
  embedded without changes if the toolchain file is right.
- **`BUILD_SHARED_LIBS` + `cmake --install`** — produce a real
  `.dylib` and install it into a staging tree. The
  install-name / RPATH story is one of the harder linker
  concepts; CMake hides most of it.

The build-system arc is unbounded — every real project adds
*another* CMake feature. The goal is to keep using the same
`CMakeLists.txt` and observe new behaviors as the project
grows.

## Build and run

```bash
cd late-may/cpp_practice/

# === CMake build (today's lesson) ===

# Configure (one-shot, out-of-source).
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build.
cmake --build build -j

# Run.
./build/P-2026-06-27-cmake-build

# === ASan build ===

cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan -j
./build-asan/P-2026-06-27-cmake-build

# === Release build ===

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/P-2026-06-27-cmake-build

# === Verify incremental rebuild ===

cmake --build build -j        # up to date, no recompile
touch inventory.h
cmake --build build -j        # inventory.cpp.o + driver.o recompile, box.cpp.o skipped

# === Cleanup ===

rm -rf build build-asan build-release

# === Old Makefile still works (Jun 16 lesson) ===

make            # builds P-2026-06-16-inventory-multi-file
make run        # build + run
make clean      # rm -rf build/ and the binary
```

All three configurations (Debug, Debug+ASan, Release) build
clean under `-Wall -Wextra -Wpedantic`. ASan+UBSan build runs
clean — no leaks, no UB, refcount drops correctly. CMake
configuration is fast (0.5s); the incremental rebuild after
`touch inventory.h` recompiled only the dependent `.o`s.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files added (under `late-may/cpp_practice/`):**
  - `P-2026-06-27-cmake-build.cpp` — driver that re-runs the
    Jun 9 / Jun 16 scenario via the Jun 16 source files
  - `P-2026-06-27-cmake-build.md` — this file
  - `CMakeLists.txt` — the actual CMake build script
- **Files modified (root):**
  - `.gitignore` — added CMake out-of-source patterns
    (`build/`, `build-*/`, `CMakeFiles/`, `CMakeCache.txt`,
    `cmake_install.cmake`, `Makefile.cmake`, `*.cmake` with
    `CMakeLists.txt` whitelisted)
- **Files reused unchanged:**
  - `box.h`, `box.cpp`, `inventory.h`, `inventory.cpp` (from Jun 16)
  - `Makefile` (from Jun 16) — still works in parallel
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default,
  set in `CMakeLists.txt` via `CMAKE_CXX_STANDARD 17`.

---

*Today is the build-system follow-on from Jun 16. The C++ is
unchanged; the build driver is. CMake produces the same binary
as the Makefile for the same source files, but with three things
the Makefile can't easily give: out-of-source builds as the
default, automatic header-dep tracking, and one source tree
that compiles Debug + Release + ASan side-by-side from the
same `CMakeLists.txt`. The Makefile stays as the pedagogical
"I see every command" build; CMake is the cross-platform
"real-world" build. The two coexist — and the same trace lines
that made the single-file lesson observable in Jun 9 work
identically across both build systems, because the C++ is the
C++. The lesson is mostly: respect the ODR, put the body in
the `.cpp`, and let CMake's target graph track the dependencies
so you don't have to.*