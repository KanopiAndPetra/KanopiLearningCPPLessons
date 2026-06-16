# C++ practice 2026-06-16 — Multi-file Inventory (header / impl / Makefile)

## What I set out to learn

The Jun 9 lesson's `Box` and `Inventory` classes lived in a single
`2026-06-09.cpp` file alongside `main()`. That worked at 400 lines,
but the moment a class gets a real implementation, the single-file
shape becomes a liability. This session takes that exact same
`Box` / `Inventory` and splits them into **separate compilation
units** with a header, an implementation, and a `Makefile`.

Three things I want this lesson to teach:

1. **The header is the API; the `.cpp` is the implementation.**
   Knowing which declarations go where (and why) is the day-one
   skill for any non-trivial C++ project.
2. **The linker is part of the language.** Once a project has more
   than one `.cpp`, you have to understand what the compiler emits
   (object code) and what the linker stitches together (symbol
   resolution). The trace lines in `Box`'s ctors/dtors make that
   observable.
3. **Build systems aren't optional.** A `Makefile` isn't just a
   convenience — it encodes the dependency graph (which `.o` depends
   on which headers) so that `make` can do incremental builds. The
   `-MMD -MP` flags generate that graph automatically.

The file is
`late-may/cpp_practice/P-2026-06-16-inventory-multi-file.cpp`,
plus `box.h`, `box.cpp`, `inventory.h`, `inventory.cpp`, and
`Makefile`. Path matches the Jun 9–15 convention
(`late-may/cpp_practice/`, P-prefix).

## Build and run

```bash
# Preferred: let the Makefile handle the flags and deps.
make            # build with -Wall -Wextra -Wpedantic -std=c++17 -O0 -g
make run        # build + run
make clean      # rm -rf build/ and the binary
make rebuild    # clean + build
make asan       # build with -fsanitize=address
make verbose    # show every command

# One-liner, no Makefile:
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-16-inventory-multi-file \
    P-2026-06-16-inventory-multi-file.cpp box.cpp inventory.cpp
./P-2026-06-16-inventory-multi-file
```

I confirmed the incremental build works: `touch box.h && make`
recompiles all three `.o` files (every TU includes `box.h`),
re-links, and reports "Nothing to be done" on a second `make`.

## Files in this lesson

| File | Role | Compiles to |
|------|------|-------------|
| `box.h` | `Box` class declaration | (header, no .o) |
| `box.cpp` | `Box` member-function definitions | `build/box.o` |
| `inventory.h` | `Inventory` declaration (uses `Box`) | (header) |
| `inventory.cpp` | `Inventory` definitions | `build/inventory.o` |
| `P-2026-06-16-inventory-multi-file.cpp` | `main()` | `build/P-2026-06-16-...o` |
| `Makefile` | Build graph + recipes | (drives the build) |

The linker is what joins them into the final binary. With `-O0 -g`
the call graph is straightforward to read in a debugger.

## Key ideas

### The header is a contract, not a copy

The class definition in `box.h` is the only thing other translation
units see when they `#include "box.h"`. It declares the class, the
public interface, and the private members. It does **not** define
the member functions (with one exception: inline functions defined
in the class body are implicitly inline, and the ODR permits the
same definition in every TU).

What goes in a header:

- Class definitions (members + member function declarations)
- Inline function definitions (small, hot, or templated)
- `constexpr` / `const` variable definitions
- Template declarations

What does **not** go in a non-inline header:

- Non-inline function *definitions*. If `box.h` defined
  `Box::Box(const std::string& label) { ... }` (not `inline`, not in
  the class body), then every `.cpp` that includes `box.h` would
  produce its own copy of that function. The linker would then
  complain about duplicate symbols — the classic "multiple
  definition" error.

The fix is the split: declarations in the header, definitions in
the `.cpp`. The header can be included in a thousand translation
units and the linker still sees only one definition.

`#pragma once` (or traditional `#ifndef BOX_H ... #endif` guards)
prevent the same header from being included twice in one TU. This
matters for me specifically because `main.cpp` includes both
`box.h` and `inventory.h`, and `inventory.h` itself includes
`box.h`. Without guards, `Box` would be declared twice in
`main.cpp` — an ODR violation.

### Forward declarations vs. full includes

`inventory.h` includes `box.h`, not just `class Box;`. Why?

A **forward declaration** (`class Box;`) tells the compiler "a
class named `Box` exists" without giving it the layout. That's
enough to declare a `Box*` member or a `Box&` parameter, because
pointers and references have a fixed size regardless of what they
point to.

But `Inventory` has a `std::vector<Box> boxes_` member. A
`std::vector` needs the full `Box` layout to know how big each
element is and how to move/destroy them. A forward declaration
isn't enough; you need `#include "box.h"`.

The rule of thumb:

| You need to do this with `Box` | You need |
|--------------------------------|----------|
| Hold a `Box*` or `Box&` | Forward declaration |
| Hold a `Box` by value | Full include |
| Call a method on a `Box` (without seeing the result type) | Forward declaration |
| Call a method that returns `Box` by value | Full include |
| Inherit from `Box` | Full include |
| Use `sizeof(Box)` | Full include |

### The linker resolves symbols across `.o` files

The compilation pipeline:

```
  box.cpp  ──►  g++ -c  ──►  build/box.o        (object file)
inv.cpp   ──►  g++ -c  ──►  build/inventory.o
main.cpp  ──►  g++ -c  ──►  build/main.o
                              │
                              ▼
                          g++ (linker)
                              │
                              ▼
                  P-2026-06-16-...-multi-file  (executable)
```

When `inventory.cpp` is compiled, every `boxes_.push_back(std::move(b))`
becomes a call to a `Box` member function. The compiler doesn't
have the body — that lives in `box.cpp`. So the compiler emits an
**undefined symbol** in `inventory.o`:

```
  U _ZN3BoxC1ERKSs   (or whatever the mangled name is)
```

The linker takes the undefined symbols from `inventory.o`, looks
them up across all the other `.o` files, and patches in the
addresses. `box.o` provides the body; `inventory.o` has the call
site; the linker welds them together.

If a function is *declared* in `box.h` but never *defined*, you
get a **linker error**, not a compiler error:

```
undefined reference to `Box::Box(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)'
```

That message — "undefined reference" rather than "undefined
function" — is the tell. The compiler was happy (it saw a
declaration). The linker is unhappy (it never found a body).

### The trace shows the call across translation units

The Jun 9 lesson had all the trace lines in one file. With the
multi-file build, the trace lines still appear, but now the
addresses come from three different `.o` files. The trace for
section 1 (adding `red`):

```
[Box] ctor ('red') @0x16fb52820         <- constructed in main()'s frame
[Inventory::add] storing 'red'          <- the body of add() is in inventory.o
[Box] MOVE ctor ('red') @0x798c00900    <- the parameter is a fresh Box on the heap/vector
[Box] dtor ('') @0x16fb52820            <- the original temporary dies
```

The first address (`0x16fb52820`) is in the stack frame of
`main()` — that's where `Box("red")` was constructed. The
MOVE-ctor address (`0x798c00900`) is inside the `std::vector`'s
storage — that's where the parameter ends up after the move. The
dtor at the original address is the temporary's death, *after*
its contents have been moved out.

These three lines correspond to *three* functions in *two*
different translation units, but the trace makes them sequential
and readable. The cross-TU call isn't visible in the source
text; it's visible in the trace.

### Why a Makefile (and not just `g++ *.cpp`)

A one-liner `g++ main.cpp box.cpp inventory.cpp -o prog` works,
but rebuilds everything on every invocation. The `Makefile` adds:

1. **Incremental builds.** Each `.o` depends on its `.cpp` and
   on the headers it (transitively) includes. If only `box.cpp`
   changes, only `box.o` and the final binary are rebuilt —
   `inventory.o` and `main.o` are still valid. With three files
   this is barely faster, but at 30 files the difference is
   the difference between "edit, save, test" in one second and
   "go get a coffee."

2. **Automatic header-dep tracking.** The `-MMD -MP` flags tell
   the compiler to emit a `.d` file (one per `.o`) listing the
   headers the `.cpp` includes. The Makefile does
   `-include $(OBJS:.o=.d)` so that every header change
   automatically invalidates the right `.o` files. Without
   this, touching a header doesn't trigger a rebuild (until
   you notice the stale binary).

3. **A single place for build flags.** `-Wall -Wextra -Wpedantic
   -std=c++17` appears in the Makefile's `WARNINGS` and `CXXSTD`
   variables. The `asan` target adds `-fsanitize=address` in one
   place. When a new compiler warning becomes a real catch
   (e.g. `-Wshadow`), I update the Makefile and every build
   picks it up.

The `clean` target is the "fresh start" button. `rebuild` is
`clean` then `all`. `verbose` echoes every command — useful for
debugging a Makefile that doesn't quite do what you think.

### Why the headers are tiny on purpose

Notice the headers don't include `<iostream>`. Why? Because
nothing in the *interface* of `Box` or `Inventory` uses `std::cout`
or any other iostream facility. The trace lines that print are in
the `.cpp` files, where `<iostream>` is included. The headers
include only what the *declaration* needs:

- `<string>` — for `std::string` in `Box::Box(const std::string&)`
  and `Box::label() const` returns.
- `<cstddef>` — for `std::size_t` in `Inventory::replace_at` and
  `Inventory::size`.
- `<vector>` — for `std::vector<Box>` (a private member; the
  definition still needs to be visible).
- `"box.h"` — for the full `Box` type used by value in
  `Inventory::boxes_` and as a parameter / return in member
  signatures.

This is the **"include what you use"** principle. The headers
that *use* `<iostream>` are `box.cpp` and `inventory.cpp`, not
the headers. Any translation unit that includes `box.h` gets
exactly the dependencies it needs to use the `Box` interface —
no more, no less. If `box.h` dragged in `<iostream>`, every
TU that included it (whether or not it printed anything) would
recompile when `<iostream>` changed.

### What the link still gets right

The biggest worry with a multi-file build is "did I forget to
make a function visible?" or "did I name the same function two
different ways in the header and the `.cpp`?" The answer is
usually in the linker error, but the lesson here is that the
**trace still works the same way as a single-file program**.
The compiler doesn't care whether `Box::Box` is defined in
`box.cpp` or in the same `.cpp` as `main()` — the symbol it
generates for the call site is the same. The linker's job is
to match them up.

So the multi-file shape is mostly an *organizational* choice,
not a *semantic* one. The same `Box` works the same way.
What changes is:

- Where the body lives (and therefore who can edit it
  without breaking other TUs).
- How long the build takes when something changes.
- How clearly the API is documented (the header is the
  documentation by default — but only if you read it that
  way).

## Design choices and trade-offs

### Where to put the trace lines

The Jun 9 lesson had trace lines in every `Box` ctor / dtor /
move. This lesson keeps them, but they live in `box.cpp`, not
`box.h`. That's the right place: the *interface* of `Box`
(what's in `box.h`) is "construct me with a label, ask me my
label." The *implementation* includes "and also print a line
saying what happened." The interface is the contract; the
implementation is the trace.

If I wanted to silence the traces in production, I'd compile
`box.cpp` with a different `-DBOX_QUIET` flag, or replace the
trace calls with a logging facade. The header doesn't change.

### Why `noexcept` on the move ctor / assign

`Box::Box(Box&& other) noexcept` and `Box& operator=(Box&&) noexcept`
are unchanged from the Jun 9 version. With multi-file builds,
`noexcept` matters more than usual: `std::vector<Box>`'s
reallocation logic looks at the move-ctor's `noexcept`-ness to
decide whether to use moves (fast) or copies (slow, and
exception-safe). If the move ctor isn't `noexcept`, the vector
copies on reallocation. Ties to the Jun 12 lesson.

### Why pass-by-value for `add()` and rvalue-ref for `replace_at()`

Same trade-off as Jun 9 §8:

- `void add(Box b)` — the parameter is a sink. The caller can
  pass an lvalue (we copy) or an rvalue (we move). The body
  moves the parameter into the vector.
- `void replace_at(std::size_t i, Box&& b)` — the caller is
  committing to sacrificing the `Box`. Cheaper than pass-by-value
  when the caller already owns a named `Box` and is happy to
  give it up.

Both shapes still work across the multi-file boundary: the
header declares them, the `.cpp` defines them, the call sites
in `main.cpp` use them.

### Why I split `Inventory` and `Box` into separate files, not together

`Box` and `Inventory` are related (Inventory *contains* Box), but
they have different rates of change: in a real project, `Box`'s
internal label might evolve (e.g. add a `weight_` field), but
`Inventory`'s public API might be stable. Keeping them in
separate `.cpp` files means editing `Box` doesn't trigger
recompilation of `Inventory` (only of TUs that include `box.h`
transitively, like `main.cpp`).

In a real project I'd also consider: header-only libraries
(template-heavy code), PIMPL (hiding the implementation behind
a `unique_ptr` to a forward-declared type), and shared libraries
(`.so`/`.dylib` for code that's loaded at runtime). All of those
build on the same idea: the header is the API, the `.cpp` is
the implementation, the linker is the joining.

## Cross-references and follow-ups

- **Jun 9 (`std::move` / rvalue refs)** — the move semantics
  demonstrated here are the same as Jun 9 §8. The traces
  should match. The *only* difference is the build layout.
- **Jun 4 (operator overloading)** — the Jun 4 `Inventory`
  could be split the same way. `operator<<` for `Box` would
  be a free function in `box.h` (or inline in the class).
  `operator+` for `Inventory` (combining two inventories)
  would be a member or free function in `inventory.h`.
- **Jun 12 (`std::move_if_noexcept`)** — `Box`'s `noexcept`
  move ctor is the reason `std::vector<Box>` moves on
  reallocation. Without `noexcept`, the vector would copy.
  This is the most important single word in the whole file.
- **Jun 13 (`enable_shared_from_this`)** — `Inventory` could
  inherit from `std::enable_shared_from_this<Inventory>` and
  hand out `shared_ptr`s to itself. The header would gain
  the base class; the `.cpp` would have a `weak_from_this()`
  cache.
- **Jun 14 (`std::span`)** — the `Inventory::print()` method
  could take a `psp::Span<Box const>` instead of indexing
  `boxes_[i]`. Same interface, slightly different shape.
- **Jun 15 (`std::visit` with stateful visitors)** — a
  stateful visitor over `Inventory` (a class derived from
  the visitor pattern) would have its own `.h` and `.cpp`.
  Same split, different domain.

## Next Steps

- **CMake** — `Makefile` works for one project, but real
  cross-platform builds use CMake. The lesson: write a
  `CMakeLists.txt` that produces the same binary, then
  `cmake -B build && cmake --build build`. (New on the
  list — this lesson covers the foundation; CMake is the
  real-world wrapper.)

- **Header-only library style** — templates and `inline`
  functions live in the header. Build a small
  `psp::optional<T>` or `psp::expected<T,E>` (the C++17
  hand-rolled versions) in a single `.h` file. (Still on
  the list from Jun 11.)

- **PIMPL (Pointer to Implementation)** — move the private
  members of `Box` into a `struct Impl` defined in
  `box.cpp`. The header holds `std::unique_ptr<Impl> impl_`.
  The build is faster and the ABI is stable, at the cost
  of an indirection. (New on the list — natural next step
  after header/impl split.)

- **Shared library (`.dylib` / `.so`)** — turn `box.cpp`
  into a shared library, link `main.cpp` against it. The
  `Box` symbols are dynamically resolved. (New on the
  list — bridges this lesson to the linker.)

- **A `static` library version** — turn `box.cpp` and
  `inventory.cpp` into `libinventory.a`, link `main.cpp`
  against it. Different from a shared library in
  important ways (no dynamic linking, faster startup,
  bigger binary). (New on the list — comparison with
  shared library.)

- **Forward-declaration optimization** — measure the
  build time of `inventory.h` with `#include "box.h"`
  vs. `class Box;`. The forward decl is faster to
  parse, but breaks if `Inventory` ever holds a `Box`
  by value. Worth a tiny test. (New on the list.)

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit (all under `late-may/cpp_practice/`):**
  - `P-2026-06-16-inventory-multi-file.cpp` (main)
  - `P-2026-06-16-inventory-multi-file.md` (this file)
  - `box.h`
  - `box.cpp`
  - `inventory.h`
  - `inventory.cpp`
  - `Makefile`
- **P- prefix per the new convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*Header is the API. `.cpp` is the implementation. The linker is
the joining. A `Makefile` is the dependency graph that lets
`make` rebuild only what changed. The trace lines that make the
single-file lesson visible in Jun 9 work the same way across
three translation units — the compiler doesn't care where the
body lives, and neither does the user, as long as the linker
finds it. The interesting failure modes (`undefined reference
to Box::Box`, "multiple definition of `vtable for Box`",
"declaration of `Box` shadows a previous declaration") all
come from getting the split wrong, and they're all visible in
the linker error. The lesson is mostly: respect the ODR, put
the body in the `.cpp`, and let the Makefile track the
dependencies.*
