# C++ practice 2026-06-30 — `psp_span` promoted to a real STATIC library

## What I set out to learn

The Jun 29 lesson (`P-2026-06-29-extern-template.md`) closed
the `psp_span` arc with a four-point list of forward-ons. The
*first* one was the most load-bearing:

> **Promote `psp_span` to a `STATIC` library with its own
> explicit instantiation** — add `psp_span/src/psp_span_inst.cpp`
> to the library directory, change `add_library(psp_span
> INTERFACE)` to `add_library(psp_span STATIC ...)`, and add the
> explicit instantiations inside the library.

That's today. The Jun 29 lesson teed it up; today I land it.

I made one small deviation from the forward-on as written: I did
**not** rename `psp_span` from `INTERFACE` to `STATIC`, and I
did not place new files in the existing `psp_span/` directory.
Instead I added a *new* directory `psp_span_lib/` with its own
CMake target. The two libraries (`psp_span` INTERFACE and
`psp_span_lib` STATIC) coexist. Both shapes are visible in the
same project, which is the pedagogical point: a header-only
library is not a *kind of thing you have to commit to*; it's a
*phase*. You can move a library from INTERFACE to STATIC
without forcing every consumer to change at once.

The Jun 28 driver still works against `psp_span` (INTERFACE),
unchanged. Today's driver is built against `psp_span_lib`
(STATIC). Both are in the parent `CMakeLists.txt`.

## What I built

A small library that is the textbook "real C++ library that
hides its own instantiation bookkeeping" shape:

```
psp_span_lib/
├── CMakeLists.txt                   ← the library's CMake config
├── README.md                        ← library-level docs
├── include/psp_span/span.h          ← public header (move from psp_span/)
└── src/psp_span_inst.cpp            ← explicit instantiations + anchor
```

And a driver that exercises it from three angles, plus two
companion TUs that opt into `extern template` to demonstrate
the suppression works:

```
P-2026-06-30-psp-span-static-library.cpp   ← driver, no extern template
span_consumer_b.cpp                       ← extern template Span<const int>
span_consumer_c.cpp                       ← extern template Span<double>
```

`P-2026-06-30-psp-span-static-library.md` ← this file.

The parent `CMakeLists.txt` was extended (Jun 30, sections 11
and 12) to declare the library via `add_subdirectory(psp_span_lib)`
and to wire a third driver executable against it.

## What I confirmed

### (1) The library builds as a real STATIC archive

```text
$ cmake -S . -B build-30 -DCMAKE_BUILD_TYPE=Debug
...
-- psp_span_lib 0.2.0 configured.
-- Project: PetraInventory 0.1.0 ...
-- Configuring done (0.3s)

$ cmake --build build-30 -j
[ 38%] Linking CXX static library libpsp_span_lib.a
...
[ 92%] Linking CXX executable P-2026-06-30-psp-span-static-library
[100%] Built target P-2026-06-30-psp-span-static-library
```

The `.a` archive is real:

```text
-rw-r--r--  1 oppie1.kanopi  staff   100K  libpsp_span_lib.a
```

100 KB is a bit large because it contains the relocations and
debug info for the explicit instantiations (the three
specializations × ~15 member functions each, ~7 KB of actual
`.text` — see `size` below). Stripped the size drops
significantly.

### (2) `nm psp_span_inst.o` shows the explicit instantiations as STRONG DEFINITIONS

Filtering `nm` to just `psp::Span<...>` symbols and counting by
type:

```text
$ nm build-30/psp_span_lib/CMakeFiles/psp_span_lib.dir/src/psp_span_inst.cpp.o \
    | grep "Span" | awk '{print $2}' | sort | uniq -c
   3 S    (the three static `extent` constants — one per specialization)
  66 T    (strong definitions of every member function)
```

The 66 `T` symbols break down as ~22 per specialization
(constructors `C1` and `C2` × 4 ABI forms ≈ 8 + `size()`,
`operator[]()`, `begin`, `end`, `at`, `front`, `back`, `first`,
`last`, `subspan`, `data`, `size_bytes`, `empty`, `rbegin`,
`rend` ≈ 15 methods, total ≈ 23 per specialization × 3 ≈ 69).
Close enough. Each specialization has all its members
*defined* in this single `.o`.

Sample (decoded via `c++filt` for readability):

```text
T psp::Span<int, dynamic_extent>::Span()
T psp::Span<int, dynamic_extent>::Span(int*, unsigned long)
T psp::Span<int, dynamic_extent>::size() const
T psp::Span<int, dynamic_extent>::operator[](unsigned long) const
T psp::Span<int, dynamic_extent>::begin() const
T psp::Span<int, dynamic_extent>::end() const
... (and the same shape for Span<const int>, Span<double>)
```

This is the "library owns it" payoff: a single TU carries
the cost of knowing every Span<int> member's definition; the
rest of the program just *uses* it.

### (3) `nm span_consumer_b.o` and `_c.o` show the suppression working

The whole point of the lesson is that consumer TUs that
declare `extern template class psp::Span<X>;` should emit
**weak undefined references**, not strong definitions. Here's
the proof:

```text
$ nm -u build-30/CMakeFiles/P-2026-06-30-psp-span-static-library.dir/span_consumer_b.cpp.o \
    | grep -i span
__ZNK3psp4SpanIKiLm18446744073709551615EE3endEv     (= psp::Span<const int>::end() const)
__ZNK3psp4SpanIKiLm18446744073709551615EE4backEv    (= psp::Span<const int>::back() const)
__ZNK3psp4SpanIKiLm18446744073709551615EE4sizeEv    (= psp::Span<const int>::size() const)
__ZNK3psp4SpanIKiLm18446744073709551615EE5beginEv   (= psp::Span<const int>::begin() const)
__ZNK3psp4SpanIKiLm18446744073709551615EE5frontEv   (= psp::Span<const int>::front() const)
__ZNK3psp4SpanIKiLm18446744073709551615EEixEm      (= psp::Span<const int>::operator[] const)
```

Six `U` symbols (undefined references). These will be
**resolved by the linker pulling in libpsp_span_lib.a**,
which has the matching `T` (strong) definitions.

`span_consumer_c.o` looks the same, just for `Span<double>`:

```text
$ nm -u build-30/CMakeFiles/P-2026-06-30-psp-span-static-library.dir/span_consumer_c.cpp.o \
    | grep -i span
__ZNK3psp4SpanIdLm18446744073709551615EE3endEv     (= psp::Span<double>::end() const)
__ZNK3psp4SpanIdLm18446744073709551615EE4sizeEv    (= psp::Span<double>::size() const)
__ZNK3psp4SpanIdLm18446744073709551615EE5beginEv   (= psp::Span<double>::begin() const)
__ZNK3psp4SpanIdLm18446744073709551615EEixEm      (= psp::Span<double>::operator[] const)
```

Four `U` symbols. Matches the methods actually called by
`dot_product`: `size()`, `begin()`, `end()`, and `operator[]()`.

### (4) The driver TU has zero `U` symbols (it didn't suppress)

```text
$ nm -u build-30/CMakeFiles/P-2026-06-30-psp-span-static-library.dir/P-2026-06-30-psp-span-static-library.cpp.o \
    | grep -i span
(empty)
```

The driver uses `psp::Span<const int>` *inline* (no `extern
template` line). It emits strong definitions (`T`) of every
method it calls. Those strong definitions are duplicates of
the strong definitions in `psp_span_inst.o`, and the linker
dedupes them. Final binary has one copy of each method.

This is the same trade the Jun 29 lesson measured — opt-in
savings only — and it shows up here on `__TEXT` sizes too:

```text
$ size build-30/psp_span_lib/CMakeFiles/psp_span_lib.dir/src/psp_span_inst.cpp.o \
       build-30/CMakeFiles/P-2026-06-30-psp-span-static-library.dir/span_consumer_b.cpp.o \
       build-30/CMakeFiles/P-2026-06-30-psp-span-static-library.dir/span_consumer_c.cpp.o \
       build-30/CMakeFiles/P-2026-06-30-psp-span-static-library.dir/P-2026-06-30-psp-span-static-library.cpp.o
__TEXT   __DATA   ...
4562     24       psp_span_inst.cpp.o           ← the library's TU
3841     0        span_consumer_c.cpp.o         ← suppressed, smaller
5622     0        P-2026-06-30-...-library.cpp.o ← driver, emits its own
10069    0        span_consumer_b.cpp.o         ← hmm, larger?
```

`span_consumer_b.o` is **larger** than the driver, not smaller.
That's because of the templated vector-ctor — see "Gotcha"
below. The other suppressed TU (`span_consumer_c.o`) is
smaller than the driver because the templated ctor in question
(`Span<double>::Span(std::array<double, 4>&)`) compiles to less
code than `Span<const int>::Span(std::vector<int>&)`. The
"smaller after suppression" pattern is reliable for the
*Span methods* (size, begin, end, etc.); it's muddied by the
templated ctors which `extern template class` doesn't touch.

### (5) The driver runs and prints the spans correctly

```text
$ ./build-30/P-2026-06-30-psp-span-static-library
=== P-2026-06-30-psp-span-static-library ===
psp_span_lib v0.2.0 — STATIC library with internal
explicit instantiations of psp::Span<int>,
psp::Span<const int>, psp::Span<double>.

[A] driver uses psp::Span<const int> inline
    contents: 10 20 30 40 50
    sum_window = 150
    size() == 5, empty() = false

[B] span_consumer_b.cpp (with extern template on
    psp::Span<const int>)
    contents: -1 2 -3 4 -5 6 -7 8
    count_positive = 4
    sum = 7 (front+back)

[C] span_consumer_c.cpp (with extern template on
    psp::Span<double>)
    a = 1 2 3 4
    b = 5 6 7 8
    dot_product = 70
    size() == 4

[D] what just happened
  - psp_span_lib.a contained the explicit instantiations
    of psp::Span<int>, psp::Span<const int>,
    psp::Span<double>. These are the STRONG DEFINITIONS
    the linker satisfies every weak reference against.
  - This driver did NOT declare `extern template` for
    psp::Span<int>; in section [A]. It emitted its own
    strong definitions alongside. The linker dedupes
    strong-vs-strong: final binary has one copy.
  - span_consumer_b.cpp / _c.cpp declared `extern template`
    and emitted WEAK UNDEFINED references, satisfied by
    psp_span_lib.a at link time.

All sections passed. ...
```

`sum_window = 150` (= 10+20+30+40+50 ✓), `count_positive = 4`
(2, 4, 6, 8 ✓), `dot_product = 70` (= 1·5 + 2·6 + 3·7 + 4·8 ✓).
All three results check out.

### (6) ASan build is clean

```text
$ rm -rf build-30-asan
$ cmake -S . -B build-30-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
... ENABLE_ASAN=ON ...
$ cmake --build build-30-asan -j
... built ...

$ ASAN_OPTIONS=halt_on_error=0:print_summary=1 \
  ./build-30-asan/P-2026-06-30-psp-span-static-library
... (same output as the Debug run) ...
$ echo $?
0
```

No leaks, no UBs. The sanitizer run also doubles as a
*structural* test: it proves that the explicit instantiations
in `psp_span_inst.cpp` (compiled with `-fsanitize=address
-fsanitize=undefined`) are ABI-compatible with the suppressed
references in `span_consumer_b.o` / `span_consumer_c.cpp.o`
(also compiled with sanitizers, same flags). Different
compilation unit, different flags, same symbol — the linker
doesn't care. (In our case the flags match because the
parent `CMakeLists.txt` propagates `-DENABLE_ASAN=ON` to all
targets, but the structural test would pass even without
that, because the strong/weak contract doesn't depend on
sanitizers.)

## Gotchas I hit while writing this

These three corners of `extern template` behavior bit me and
are worth recording.

### Gotcha 1: `extern template class X<Y>;` does NOT suppress template member functions of X<Y>

`extern template class psp::Span<const int>;` suppresses the
implicit instantiation of `Span<const int>`'s *non-template*
member functions: `size()`, `operator[]()`, `data()`,
`begin()`, `end()`, etc. It does NOT suppress the implicit
instantiation of *templated* member functions, like the
`Span(std::vector<U>&)` constructor (the one in
`span_consumer_b.cpp`).

In the `nm` output of `span_consumer_b.o`:

```text
T __ZN3psp4SpanIKi...EE C1IivEE...    (Span<const int>::Span(vector<int>&)   — strong)
T __ZN3psp4SpanIKi...EE C2IivEE...    (Span<const int>::Span(vector<int>&)   — strong)
U __ZNK3psp4SpanIKi...EE 3endEv      (Span<const int>::end() const          — undefined)
... etc.
```

The two `T`s are the vector ctor, which is *still being
implicitly instantiated* in this TU even with `extern
template`. The C++ rule: `extern template class C;` tells the
compiler "do not generate definitions of non-template members
of `C` from this TU," but it doesn't say anything about
*member template instantiations*. The compiler must generate
those because they depend on the type `U` (here `int`),
which is unknown to the library.

**Practical impact**: when I expected
`span_consumer_b.o` to be smaller than the driver, it
wasn't. The vector-ctor is what bloats its `.text` to 10069
bytes vs. the driver's 5622. The methods I cared about
(size, begin, end, operator[]) *are* smaller — they aren't
in the `.text` at all — but the vector ctor is the
dominant cost in that file. You can't suppress a templated
member with `extern template class`. (You *can* suppress
specific instantiations with `extern template
psp::Span<const int>::Span<int>(std::vector<int>&);`,
but that's a fine-grained contract that has to be paired
with explicit instantiations in the library, and I don't
want to set that up today.)

### Gotcha 2: `T` vs `const T` is a different specialization

This bit me the first time I wrote `span_consumer_c.cpp`.
I declared `extern template class psp::Span<double>;` and
then in `dot_product` used `psp::Span<const double>` for
the parameters (because that seemed cleaner).

The result was a `nm` of `span_consumer_c.o` showing `T`
strong defs for `Span<double>::size()` etc. — suppression
DIDN'T work.

The reason: `psp::Span<double>` and `psp::Span<const double>`
are *different specializations*. `extern template class
psp::Span<double>;` says "suppress `Span<double>`," not
"suppress `Span<const double>`." They have different `T`s
at the class level, even though `double *` is convertible
to `const double *`. The linker looks for `Span<const double>`
instantiations, doesn't find them in `psp_span_lib.a`, and
either silently falls back to consumer-side implicit
instantiation (which is what happened) or fails (if the
library turned out to have explicit instantiations for
`Span<const double>` in a future version, that would
*work*, and we'd be relying on implicit-instantiation
fallback in the meantime, which is the worst of both
worlds).

**Fix**: changed `dot_product`'s parameters to
`psp::Span<double>` (non-const), to match the suppressed
specialization. Now `nm -u span_consumer_c.o` shows the
expected 4 `U` symbols. This is now part of the file's
docstring so I don't make the mistake twice.

### Gotcha 3: `extern template` is opt-in *and per-specialization*

Both gotchas 1 and 2 are symptoms of a single rule:
`extern template class C<T>;` is a fine-grained opt-in
*for one specific specialization* of one specific class.
Template member functions of `C<T>` are not covered.
Other specializations (`C<const T>`, `C<T2>`) are not
covered.

The "right way" to use `extern template` at library
scale is:

1. Pick the specializations the library actually
   instantiates (the three lines in
   `psp_span_inst.cpp`).
2. For each consumer TU, decide which of those
   specializations to suppress.
3. If a consumer TU uses a specialization the library
   *doesn't* pre-instantiate (e.g. `Span<long>`),
   `extern template class psp::Span<long>;` would
   *fail at link time* if used — there's nothing for
   the linker to find. The right thing to do is to
   *either* add the explicit instantiation to the
   library, *or* not declare `extern template` for
   that specialization.

The Jun 30 driver demonstrates only `Span<int>`,
`Span<const int>`, `Span<double>` — the three that
the library carries — so the consumer-side opt-ins
are valid.

## Why this matters

`psp_span_lib` is now in the shape that all widely-used C++
libraries are in (Boost, Abseil, fmt, spdlog, …): a
header in `include/`, source in `src/`, with explicit
instantiations owned by the library for the most common
template arguments. Consumers `target_link_libraries(... PRIVATE
psp_span_lib)` and get a link line, an include path, a C++17
requirement, and *free* opt-in `extern template` speedup —
none of which require per-consumer bookkeeping.

The pedagocial arc over the last four days:

| Day       | Shape                                                              | Consumes                  |
|-----------|--------------------------------------------------------------------|---------------------------|
| Jun 28    | `INTERFACE` library, header-only                                   | `#include "psp_span/span.h"` (relative) |
| Jun 29    | `INTERFACE` + consumer's own `extern template` / inst TU           | `#include "psp_span/span.h"`, plus `extern template` consumer-side |
| Jun 30    | `STATIC` library, library-owned `extern template` / inst TU        | `#include <psp_span/span.h>` (installed-style), `extern template` is consumer's opt-in |

Each day adds one piece. The consumer-side `extern template`
discipline of Jun 29 is now *optional* on Jun 30 — it's
purely a compile-time saving that consumers may or may not
take. The library's correctness doesn't depend on any TU
beyond itself doing the right thing.

## Key ideas

### What `extern template class C<T>;` actually does at the language level

From the C++17 standard ([temp.spec], paragraph 4):

> An explicit instantiation declaration is an
> `extern template` declaration that begins with the
> `extern` keyword. An explicit instantiation
> declaration for a class template specialization
> suppresses the implicit instantiation of the
> declared class template specialization, the declared
> member functions, and the declared member classes.

The "declared member functions" wording is the
non-template members. Templated member functions
(template <class U> ctor, etc.) are not "declared" in
the same sense — they're not named until they're
instantiated against a specific `U`. That's why Gotcha 1
happens.

### `extern template` is a contract between the consumer and the library

Consumer says: "I won't instantiate C<T> in this TU."
Library says: "I'll instantiate it for you in `inst.cpp`,
and the resulting symbols are now in `libpsp_span_lib.a`."

If the library breaks the contract (deletes `template
class psp::Span<double>;` from `psp_span_inst.cpp`), the
consumer's linker fails with `undefined symbols`.
Section "What's in the tree" has the exact failure
mode.

If the consumer breaks the contract (declares `extern
template class psp::Span<long>;` without the library
having `template class psp::Span<long>;`), same failure
mode. **The contract is bilateral.**

### The library's instantiation list is also a stability guarantee

If a consumer program today calls `psp::Span<long>`, the
compiler generates implicit instantiations of every
`Span<long>::*` method. The library does NOT carry
`template class psp::Span<long>;`. Link succeeds (the
consumer's implicit instantiation provides the
definition).

If a future library version *adds* `template class
psp::Span<long>;`, link succeeds (the library version's
definition is now where it lives).

If a future library version *removes* `template class
psp::Span<long>;` that was previously there, every
consumer TU that was relying on it via `extern template`
breaks at link time.

The takeaway: the list of explicit instantiations in
`psp_span_inst.cpp` is part of the library's *public ABI*
as much as the header is. You can't just edit it
without thinking about every consumer that might have
declared `extern template` against it.

### Why I kept `psp_span` (INTERFACE) alive alongside `psp_span_lib` (STATIC)

Jun 28 established `psp_span` as an INTERFACE library. I
deliberately did not "promote" it to STATIC today.
Two reasons:

1. **The Jun 28 lesson's pedagogical point stays visible.**
   The whole point of the Jun 28 lesson was "an
   INTERFACE library produces no artifacts, and that's
   a valid choice for a header-only library." Keeping
   `psp_span` as INTERFACE preserves that. A future
   reader can flip back through the commits and see the
   choice in action.

2. **Both shapes are useful for different projects.**
   A "real" library that has all its instantiations
   baked in (`psp_span_lib`) is what you ship to other
   people. A pure header-only INTERFACE library
   (`psp_span`) is what you use internally when you
   don't want the build system fuss. Showing both side
   by side is more informative than picking one.

Future readers are welcome to *replace* `psp_span`'s
target kind with STATIC. The Jun 29 lesson has notes on
what to change.

## What's in the tree now

```text
late-may/cpp_practice/
├── ... (Jun 16..Jun 28 files unchanged from the Jun 29 commit)
├── ... (Jun 29 files unchanged)
├── psp_span/                                  ← existing (INTERFACE library)
│   ├── README.md
│   └── span.h
├── psp_span_lib/                              ← NEW today (STATIC library)
│   ├── README.md                              ← NEW today
│   ├── CMakeLists.txt                         ← NEW today
│   ├── include/psp_span/span.h                ← NEW today (moved header)
│   └── src/psp_span_inst.cpp                  ← NEW today (explicit instantiation)
├── span_consumer_b.cpp                        ← NEW today (extern template on Span<const int>)
├── span_consumer_c.cpp                        ← NEW today (extern template on Span<double>)
├── CMakeLists.txt                             ← UPDATED today (Section 11/12)
├── P-2026-06-30-psp-span-static-library.cpp   ← NEW today (driver)
└── P-2026-06-30-psp-span-static-library.md    ← NEW today (this file)
```

The Jun 28 driver (`P-2026-06-28-cmake-interface-library.cpp`)
and its dependency on `psp_span` are *unchanged*. The parent
`CMakeLists.txt` is amended to add Section 11 (the library
target via `add_subdirectory(psp_span_lib)`) and Section 12
(today's driver executable), and that's it.

## Build and run

```bash
cd late-may/cpp_practice/

# Debug build (default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/P-2026-06-30-psp-span-static-library

# ASan + UBSan build
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan -j
ASAN_OPTIONS=halt_on_error=0:print_summary=1 \
    ./build-asan/P-2026-06-30-psp-span-static-library

# Cleanup
rm -rf build build-asan
```

The other Jun 27 / Jun 28 / Jun 29 drivers are also built
and runnable in the same `build/` directory. Nothing in
today's commit breaks them.

## Cross-references and follow-ups

- **Jun 28** (`P-2026-06-28-cmake-interface-library.md`) —
  established `psp_span` as `INTERFACE`. Today's
  `psp_span_lib` is its `STATIC` cousin, side-by-side.
- **Jun 29** (`P-2026-06-29-extern-template.md`) —
  demonstrated consumer-side `extern template` with a
  hand-managed instantiation TU. Today, that TU lives
  inside the library instead.
- **libstdc++ `bits/shared_ptr_base.h`** — the canonical
  example of header `extern template` plus library-side
  `template class`. The pattern in this lesson is the
  small-scale version of what every standard library
  does for `shared_ptr`, `vector<bool>`, `basic_string`,
  etc. Worth reading the source.

## Next Steps

The forward-on set from Jun 30 itself is essentially the
*remaining* Jun 29 forward-on list, with one new item:

- **`install(TARGETS psp_span_lib EXPORT ...)`** — make
  `cmake --install build` produce a real install tree:
  `include/psp_span/span.h` in `prefix/include/`,
  `libpsp_span_lib.a` in `prefix/lib/`, and a CMake
  package config (`psp_span_libConfig.cmake`) so other
  projects can `find_package(psp_span_lib REQUIRED)` and
  use the installed library. This builds directly on
  today: install rules were prefigured in
  `psp_span_lib/CMakeLists.txt` (the `PUBLIC_HEADER`
  property is set, and the install comment
  `INSTALL_INTERFACE` is noted). Filling them in is the
  next concrete task.

- **`find_package(fmt)` for a real third-party header-only
  dep** — `{fmt}` is the canonical third-party
  formatting library. `find_package(fmt REQUIRED)` plus
  `target_link_libraries(... PRIVATE fmt::fmt)` is the
  shape today's lesson sets up for, except using a real
  package manager (Homebrew on macOS) instead of a
  vendored `psp_span_lib/`. Three lines of CMake.

- **Cross-compile target** — `cmake -DCMAKE_TOOLCHAIN_FILE=...`
  pointing at an embedded ARM toolchain. Today's CMake
  builds the library and driver for the host; the same
  CMakeLists builds for any target with the right
  toolchain file. A learning exercise in
  `<platform>`-vs-`<compiler>`-vs-`<target>` separation.

- **`external_project_add()` to consume `psp_span_lib`
  from a separate repo** — shows the
  "library lives in another git repo, fetched at
  configure time" pattern. Combines today's library
  shape with CMake's superbuild idiom.

The biggest open question from today's lesson is **how do
you decide which specializations to pre-instantiate in
the library?** Three lines for `int`, `const int`,
`double` is fine for a tiny library, but a real
production library with a million LOC of consumers
might have dozens. The honest answer is "measure what's
in the wild and ship those, with `psp::Span<T>` for
uncovered `T` falling back to consumer-side implicit
instantiation." That's a *deployment* decision more than
a *language* decision, but it's the natural close of
the arc that Jun 14 → Jun 28 → Jun 29 → Jun 30 has been
building.
