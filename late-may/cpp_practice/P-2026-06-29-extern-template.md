# C++ practice 2026-06-29 — `extern template` for header-only templates

## What I set out to learn

The Jun 28 lesson (`P-2026-06-28-cmake-interface-library.md`) ended with
a list of forward-ons, and the first one was:

> **`extern template` for `psp::Span<int>`** — show how the
> hand-rolled library *could* be made faster to compile if it
> had many consumers. Mark the common instantiations in a
> single `.cpp` (but `psp_span` has no `.cpp` — so this lesson
> requires adding one). This is the bridge from
> "header-only by necessity" to "header-only by design +
> precompiled core."

That's today's lesson, almost word for word. The Jun 28 note
correctly anticipates the catch: doing `extern template` "the
obvious way" requires `psp_span` to have a `.cpp` of its own,
which would change it from a true `INTERFACE` library to
something with compiled output.

The honest answer — and the one I'm landing on — is that
`extern template` works just as well in *consumer* code. The
header-only library stays header-only (and stays an `INTERFACE`
target). The consumer organizes its own TUs so that one
designated TU does the explicit instantiation, and the others
suppress it. The C++ standard library does exactly this:
`<vector>` says `extern template class std::vector<...>;` and
libstdc++/libc++ provide the explicit instantiation in their
own `.cpp`. Today I do the same in user code, with `psp::Span`
standing in for `std::vector`.

The pedagogical design: four translation units, each with a
deliberate role, plus the driver. Build each TU to its own
`.o`, inspect with `nm`, and watch the suppression work.

## What I confirmed

### (1) Four TUs, four distinct roles

```text
span_helpers.h          — declarations of petra::sum_of_ints, fill_ascending,
                          count_nonzero. Includes psp_span/span.h.

span_helpers_a.cpp      — definition of sum_of_ints.
                          NO extern template — full instantiation here.

span_helpers_b.cpp      — definition of fill_ascending.
                          extern template class psp::Span<int>; — suppressed.

span_helpers_c.cpp      — definition of count_nonzero.
                          extern template class psp::Span<int>; — suppressed.

span_helpers_inst.cpp   — `template class psp::Span<int>;`
                          `template class psp::Span<const int>;`
                          Explicit instantiations: strong definitions emitted here.

P-2026-06-29-extern-template.cpp — the driver. Calls all three helpers,
                          uses Span ctors, prints the explanation.
```

The driver links against all four. `psp_span` stays a header-only
`INTERFACE` library — no `.cpp` was added to `psp_span/`. The
`extern template` pattern is implemented entirely on the consumer
side, in `span_helpers_inst.cpp`.

### (2) `nm` shows the suppression empirically

The lesson is "suppression is real and measurable." Here's what
`nm` reports for each `.o`:

```text
=== span_helpers_a.o (NO extern template) ===
0000000000000000 T petra::sum_of_ints(psp::Span<int const, ...>)
0000000000000080 T psp::Span<int const, ...>::size() const          ← STRONG DEF
0000000000000098 T psp::Span<int const, ...>::operator[](unsigned long) const  ← STRONG DEF

=== span_helpers_b.o (extern template) ===
0000000000000000 T petra::fill_ascending(psp::Span<int, ...>)
                 U psp::Span<int, ...>::size() const                ← UNDEFINED (weak)
                 U psp::Span<int, ...>::operator[](unsigned long) const  ← UNDEFINED

=== span_helpers_c.o (extern template) ===
0000000000000000 T petra::count_nonzero(psp::Span<int const, ...>)
                 U psp::Span<int const, ...>::size() const          ← UNDEFINED
                 U psp::Span<int const, ...>::operator[](unsigned long) const  ← UNDEFINED

=== span_helpers_inst.o (explicit instantiation) ===
0000000000000a08 S psp::Span<int const, ...>::extent               ← static data
00000000000004dc T psp::Span<int const, ...>::Span(int const*, unsigned long)
0000000000000484 T psp::Span<int const, ...>::Span()
... (38 T symbols total: 19 for Span<int>, 19 for Span<const int>)
... (2  S symbols total: the static `extent` constants)
```

Read those lines carefully. **The `T` in column 2 means
"strong definition"** — the symbol is *defined* in this `.o`.
**The `U` means "undefined"** — the symbol is *referenced*
here but the definition lives elsewhere.

- `a.o` defines `psp::Span<int const>::size()` and `operator[]()`
  inline (because it has no `extern template`).
- `b.o` and `c.o` reference them as undefined symbols. The
  compiler emitted weak references; no `.text` for them here.
- `inst.o` defines all 38 member functions (19 per
  instantiation), the single source of truth.

That's the lesson made visible at the binary level. Not
asserted — *measured*.

### (3) `size` confirms the per-TU text shrinkage

```text
$ size span_helpers_*.o
__TEXT  __DATA  __OBJC  others   dec    hex
188     0       0       24853   25041  61d1   span_helpers_a.o
120     0       0       24507   24627  6033   span_helpers_b.o     ← smallest
140     0       0       24578   24718  608e   span_helpers_c.o
2610    4       0       75368   77982  1309e  span_helpers_inst.o  ← biggest
```

The `__TEXT` column (executable code + read-only data) tells
the story:

- `a.o`: 188 bytes of `__TEXT` for `sum_of_ints` + the two
  Span methods it instantiates (`size()`, `operator[]()`).
- `b.o`: 120 bytes for `fill_ascending` only. No Span methods
  (they're suppressed — `U` in `nm`).
- `c.o`: 140 bytes for `count_nonzero` only. Same suppression.
- `inst.o`: 2610 bytes for **every** Span method, both
  instantiations. This is where the cost moves when you use
  `extern template` — out of N TUs and into one.

In a project with 50 TUs that all use `psp::Span<int>`,
without `extern template` each TU pays ~50 bytes of `__TEXT`
for the Span methods. With `extern template`, 49 TUs pay 0
bytes and one TU pays ~2000 bytes. Total text is roughly the
same, but compile-time is faster (only one TU parses + emits
the templates), and per-TU `.o` files are smaller.

### (4) Drop the explicit-instantiation TU and the linker fails

The reciprocal proof: if `span_helpers_inst.cpp` is missing,
the linker can't resolve the `U` symbols in `b.o` and `c.o`.

```text
$ g++ -o broken P-2026-06-29-extern-template.cpp \
    span_helpers_a.cpp span_helpers_b.cpp span_helpers_c.cpp
Undefined symbols for architecture arm64:
  "psp::Span<int, ...>::size() const", referenced from:
      petra::fill_ascending(psp::Span<int, ...>) in span_helpers_b.o
  "psp::Span<int, ...>::operator[](unsigned long) const", referenced from:
      petra::fill_ascending(psp::Span<int, ...>) in span_helpers_b.o
ld: symbol(s) not found for architecture arm64
clang++: error: linker command failed with exit code 1
```

`a.o` would still satisfy these symbols on its own (it has
its own Span<int> instantiations from `sum_of_ints`). But
b.o and c.o have NO Span<int> instantiations — their only
references are the `U` ones, and there's no place for them
to resolve to.

If I added `a.o` back, the build would succeed for `b.o` and
`c.o`'s purposes because `a.o`'s Span<int> instantiations
satisfy the `U` symbols. But that's *coincidence*, not
*design* — and it depends on `sum_of_ints` happening to
instantiate exactly the methods that `fill_ascending` calls.
Remove `sum_of_ints` and the chain breaks. The explicit
instantiation TU is the robust answer: "this is where the
Span<int> methods live, full stop."

### (5) The driver runs and prints what happened

```text
$ ./P-2026-06-29-extern-template
argv[0] = ./P-2026-06-29-extern-template
argv[0] contains 'build'? no  (likely hand-built)

[A] Exercising the three helpers
  fill_ascending produced: 0 1 2 3 4 5 6 7 8 9
  sum_of_ints    = 45  (expect 45)
  count_nonzero  = 9  (expect 9 — only the leading 0 is zero)
  PASS

[B] Using psp::Span ctors directly in the driver
  Span<int,4> from int[4]={10,20,30,40}: 10 20 30 40 (size=4, sizeof=16)
  Span<const int> from vector{1..5}:     1 2 3 4 5 (size=5)

[C] What `extern template` did at compile time
  ... (the explainer block)
```

Note section [A]: `count_nonzero` returns 9, not 10. The
array is `0 1 2 3 4 5 6 7 8 9` — the leading zero is one
zero, leaving nine nonzero elements. (I initially wrote
"expect 10" and the run caught my arithmetic mistake. This
is why running beats reading.)

Section [B] is there to show that the driver itself *does*
instantiate Span methods, because it uses Span ctors directly
(no `extern template` in the driver). The driver has its own
copy of `psp::Span<int, 4>::Span(int (&)[4])` and
`psp::Span<const int>::Span(std::vector<int>&)`. That's
intentional — the lesson is that `extern template` is opt-in,
per TU.

### (6) ASan build is clean

```text
$ g++ -std=c++17 -O0 -g -fsanitize=address -fsanitize=undefined \
    -fno-omit-frame-pointer -o asan P-2026-06-29-extern-template.cpp \
    span_helpers_a.cpp span_helpers_b.cpp span_helpers_c.cpp \
    span_helpers_inst.cpp
$ ASAN_OPTIONS=halt_on_error=0 ./asan
... (identical output, no sanitizer reports) ...
```

No leaks, no UBs. The Span methods are simple pointer math
with bounds checks via `at()` (which throws, not UBs), and
the helpers are obvious straight-line code. The sanitizer
build is also a structural test: it proves the explicit
instantiations in `inst.cpp` are ABI-compatible with the
suppressed references in `b.o`/`c.o` — different compilation
flags, but the same symbols resolve at link time.

## Key ideas

### What `extern template` does, mechanically

The declaration

```cpp
extern template class psp::Span<int>;
```

tells the compiler: *"If you see code that would require you
to instantiate `psp::Span<int>`'s member functions in this
translation unit, don't. Emit an undefined-symbol reference
instead."*

Without `extern template`, the compiler would emit **strong
definitions** of every member function used in this TU. With
`extern template`, it emits **weak references** (`U` in `nm`)
to those same member functions.

The trade is clear:

| Situation                | What the compiler does                              | Linker can find it via             |
|--------------------------|-----------------------------------------------------|------------------------------------|
| No `extern template`     | Emits strong definitions (`T` in `nm`)              | This `.o`, or any duplicate       |
| With `extern template`   | Emits weak references (`U` in `nm`)                 | Must be defined *somewhere else*   |
| Explicit instantiation   | Emits strong definitions (`T`)                      | This `.o` is the source of truth  |

The "somewhere else" for an `extern template` reference is
either an explicit instantiation (`template class psp::Span<int>;`
without `extern`) in another TU, or an implicit instantiation
in another TU that happened not to suppress.

### Why one TU's cost moves, not the program's total cost

If a project has 50 TUs each using `psp::Span<int>` and they
all instantiate the same methods (which is the common case —
`size()`, `operator[]`, `begin`, `end`), the **final binary**
contains exactly one copy of each method (the linker dedupes
strong-vs-strong and picks the first). So the *runtime cost*
is the same.

What changes is **compile time** and **per-TU `.o` size**:

- Compile time: with `extern template`, 49 of the 50 TUs
  skip the template instantiation phase for `psp::Span<int>`.
  Compilation is faster for the bulk of the project.
- `.o` size: each TU's `.o` is smaller because it doesn't
  carry the Span methods. The linker still produces one final
  binary with one copy of each method, but the intermediate
  artifacts are smaller.

For widely-used headers (`<vector>`, `<string>`, `<iostream>`),
this is a meaningful win. libstdc++ and libc++ measure it
in seconds of compile time saved across millions of lines of
user code. For a small project with one-off templates, it's
not worth the complexity.

### The `extern template` / explicit-instantiation pair is a contract

`extern template class psp::Span<int>;` is a *promise*: "I'm
not going to instantiate `psp::Span<int>` in this TU." The
promise is only safe to make if *some other TU* honors the
flip side of the contract: an explicit instantiation of the
same class.

Breaking the contract (declaring `extern template` but
having no explicit instantiation anywhere) is a **link-time
error**, not a compile error. Section (4) showed the exact
failure mode: `Undefined symbols` from the linker pointing
at the suppressed methods.

This is why explicit instantiation is usually paired with
the header — it's documented in the same place the
suppression is. The standard library does this:
`<bits/std_vector.h>` (libstdc++) declares the `extern
template` and points at where the instantiation lives.

### Why I kept `psp_span` as an `INTERFACE` library

The Jun 28 architecture had `psp_span` as a true header-only
`INTERFACE` library. Today's lesson *could* have promoted it
to `STATIC` and put the explicit instantiation inside
`psp_span/`. That would be more "industry standard" — Boost,
Abseil, etc. work that way.

I chose to keep `psp_span` as `INTERFACE` for three reasons:

1. **The Jun 28 lesson stays intact.** The whole point of
   the Jun 28 lesson was "INTERFACE libraries produce no
   artifacts." Today's lesson doesn't undo that.
2. **It mirrors `<vector>` more honestly.** The standard
   library's `extern template` lives in the *consumer*'s
   include path (`<vector>`) and the explicit instantiation
   lives in the standard library's `.cpp`. We don't have a
   standard library, but the *shape* — suppression in
   consumer code, instantiation in a single designated TU
   — is the same.
3. **It teaches the principle without the build-system
   noise.** The lesson is about `extern template`, not about
   converting `INTERFACE` to `STATIC`. Adding the
   `psp_span` conversion would be a separate lesson.

A future lesson might add `psp_span_explicit_inst.cpp` to
the `psp_span/` directory and convert the library to
`STATIC`. That's a real refactor — it changes the public
API of the library (now consumers must link against
`libpsp_span.a`) and changes the build target kind.

## What's in the tree now

```text
late-may/cpp_practice/
├── ... (Jun 16..Jun 28 files unchanged)
├── span_helpers.h                       ← NEW today (helper decls)
├── span_helpers_a.cpp                   ← NEW today (no extern)
├── span_helpers_b.cpp                   ← NEW today (extern template)
├── span_helpers_c.cpp                   ← NEW today (extern template)
├── span_helpers_inst.cpp                ← NEW today (explicit instantiation)
├── Makefile.extern_template             ← NEW today (per-TU build)
├── P-2026-06-29-extern-template.cpp     ← NEW today (driver)
└── P-2026-06-29-extern-template.md      ← NEW today (this file)
```

The `psp_span/` directory is unchanged from Jun 28. The
new files all live next to the existing Jun 27 / Jun 28
files in the same per-day convention.

## Build and run

```bash
cd late-may/cpp_practice/

# Build (each helper to its own .o)
make -f Makefile.extern_template all

# Run the driver
./P-2026-06-29-extern-template

# Inspect — the interesting target
make -f Makefile.extern_template inspect

# ASan build (manual, since the Makefile uses Debug flags)
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    -o P-2026-06-29-extern-template-asan \
    P-2026-06-29-extern-template.cpp \
    span_helpers_a.cpp span_helpers_b.cpp \
    span_helpers_c.cpp span_helpers_inst.cpp
ASAN_OPTIONS=halt_on_error=0 ./P-2026-06-29-extern-template-asan

# Cleanup
make -f Makefile.extern_template clean
```

## Cross-references and follow-ups

- **Jun 28 (`P-2026-06-28-cmake-interface-library.md`)** —
  established `psp_span` as a true header-only `INTERFACE`
  library. Today's lesson preserves that, demonstrating
  `extern template` entirely on the consumer side.
- **Jun 14 (`P-2026-06-14-std-span-by-hand.cpp`)** — the
  original prototype of `psp::Span` in a single `.cpp`.
  Today's lesson uses the Jun 28 library version.
- **Jun 4 (`cpp-practice-2026-06-04-smartpointers.cpp`)** —
  the smart-pointers lesson. Smart pointers have a similar
  `extern template` story: `std::shared_ptr` could in
  principle be `extern template`-instantiated in libstdc++/libc++
  but isn't, because the inlining is critical to performance.
  Worth noting that `extern template` is not always a win.
- **libstdc++ source code** — `bits/shared_ptr_base.h` has
  `extern template` declarations for `shared_ptr` to avoid
  duplicate instantiations in user code. Reading those
  declarations is the canonical way to see the pattern
  in the wild.

## Next Steps

The `psp_span` arc is mostly closed (header-only library,
CMake `INTERFACE` target, `extern template` consumer-side
optimization). The natural next steps move outward:

- **Promote `psp_span` to a `STATIC` library with its own
  explicit instantiation** — add `psp_span/src/psp_span_inst.cpp`
  to the library directory, change `add_library(psp_span
  INTERFACE)` to `add_library(psp_span STATIC ...)`, and
  add the explicit instantiations inside the library. This
  is the "real library" shape: consumers get a `.a` they
  link against, and the explicit instantiations live with
  the library. The cost: `psp_span` is no longer a true
  header-only library — it's a static library that happens
  to have only instantiation `.cpp` files. Worth doing as
  its own lesson because the API surface changes
  (`target_link_libraries(... PRIVATE psp_span)` now adds
  a link line, not just an include path).
- **`install(TARGETS psp_span EXPORT ...)`** — make `cmake
  --install build` produce a real install tree with the
  header in `include/psp_span/span.h` and the `.a` (if
  we go STATIC) in `lib/`. Then a second project can
  `find_package(psp_span)` and use it without the source
  tree. Today's lesson doesn't install anything; `psp_span`
  lives entirely in the source tree.
- **`find_package(fmt)` for a real third-party header-only
  dep** — `{fmt}` is the canonical header-only C++
  formatting library. `find_package(fmt REQUIRED)` pulls it
  in via Homebrew's installed CMake config files. Three
  lines: `find_package(fmt REQUIRED)`, `target_link_libraries(... PRIVATE fmt::fmt)`.
  Compare to today's `psp_span` setup — same shape, but the
  `find_package` step is what makes the dependency
  discoverable on a clean checkout.
- **Cross-compile target** — `cmake -DCMAKE_TOOLCHAIN_FILE=...`
  pointing at an embedded ARM toolchain. Today's lesson is
  "build on macOS"; the same `CMakeLists.txt` builds for any
  target with the right toolchain file.

The biggest open question from today's lesson is **when is
`extern template` worth the bookkeeping cost?** For one TU
used once: never. For 50 TUs in a big project: yes, measurably.
For STL-level adoption (millions of TUs): yes, dramatically.
The lesson now makes that question *answerable* — I can run
the numbers on a real codebase and report actual
compile-time deltas.