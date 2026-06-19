# C++ practice 2026-06-19 — PIMPL and virtual functions

## What I set out to learn

The Jun 17 and Jun 18 lessons introduced the PIMPL pattern
(forward-declared `Impl`, `unique_ptr<Impl>`, declared-in-header
/ defined-in-`.cpp` dtor and copy ops, `= default` move ops in
the header). Both lessons used *concrete* classes: `PimplBox`
and `PimplInventory`. They were never derived from; no
polymorphism, no vtable.

The "Next Steps" list at the end of Jun 18 had, as one of its
items:

> **PIMPL and virtual functions** — make `PimplInventory`
> polymorphic (add a virtual dtor and a virtual `print`) and
> observe that the `vptr` lives *outside* the `Impl` (in the
> `PimplInventory` itself, via the `unique_ptr`). Worth a small
> demo, and ties back to the Jun 17 follow-up on the same idea.

This lesson is that follow-up. The goal is to take the PIMPL
pattern and combine it with polymorphism, observing the *one*
new thing that polymorphism adds: a `vptr` per object. The
question the lesson answers is: where does the `vptr` live
when the class is PIMPLed? The answer: in the *facade*, not
in the `Impl`.

The setup is a small shape hierarchy. `PimplShape` is an
abstract base with a virtual dtor, a pure-virtual `print()`, a
pure-virtual `clone()`, and a non-pure-virtual `name()` /
`kind()`. `PimplCircle` and `PimplSquare` are concrete derived
PIMPL classes. The base has *no data*; each derived owns its
own `unique_ptr<Impl>`. The lesson demonstrates:

1. **The vptr lives in the facade, not the Impl.** `sizeof
   (PimplShape)` is 8 bytes (just a vptr). `sizeof(PimplCircle)`
   is 16 bytes (vptr + `unique_ptr<Impl>`). Adding more
   virtuals to `PimplShape` does not change `sizeof
   (PimplCircle)` -- the vtable grows, but the vptr is one
   pointer regardless. The lesson's program observes all of
   this in Section 1.
2. **Polymorphic dispatch is the killer app of virtuals.** A
   `std::vector<std::unique_ptr<PimplShape>>` holds a mix of
   circles and squares. A single for-loop calls `print()` on
   each, and the vtable picks the right derived at runtime.
   The program walks through this in Section 2.
3. **The virtual copy constructor (`clone()`) avoids slicing.**
   `PimplShape` is abstract, so the slicing trap
   (`PimplShape copy = *shape_ptr;`) is a *compile error*.
   The runtime equivalent is `shape_ptr->clone()`, which
   returns a heap-allocated *derived* type owned via a base
   pointer. The program shows this in Section 3.
4. **The Big Five of a derived PIMPL class is the same shape
   as the non-derived PIMPL** (Jun 17/18): declared dtor (this
   time `override`), declared copy ops, `= default` move ops,
   PIMPL pointer. The program walks through each operation
   in Section 4.
5. **Pass-by-base-reference is the generic shape-handling
   pattern.** A function that takes `const PimplShape&` works
   for any derived without knowing the derived's type. The
   program demonstrates this in Section 5.

The files for this lesson are at
`late-may/cpp_practice/P-2026-06-19-pimpl-virtual-functions.cpp`,
plus `pimpl_shape.{h,cpp}`, `pimpl_circle.{h,cpp}`,
`pimpl_square.{h,cpp}`. The naming convention (`pimpl_*`) is
the same as Jun 17/18, and the per-day dir is the same
(`late-may/cpp_practice/`).

## Build and run

```bash
# One-liner, matching the cron's default flags.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-19-pimpl-virtual-functions \
    P-2026-06-19-pimpl-virtual-functions.cpp \
    pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp
./P-2026-06-19-pimpl-virtual-functions

# ASan + UBSan, for the run that proves no leaks and no UB.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    -o P-2026-06-19-pimpl-virtual-functions-asan \
    P-2026-06-19-pimpl-virtual-functions.cpp \
    pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp
./P-2026-06-19-pimpl-virtual-functions-asan
```

Both builds are clean (no warnings under `-Wall -Wextra
-Wpedantic`). The output walks through five sections: layout,
polymorphic container, virtual copy, Big Five, and pass-by-
base-reference.

## Files in this lesson

| File                                       | Role                              | Compiles to       |
|--------------------------------------------|-----------------------------------|-------------------|
| `pimpl_shape.h`                            | `PimplShape` decl, abstract base  | (header, no .o)   |
| `pimpl_shape.cpp`                          | `PimplShape` dtor + name/kind     | (linker)          |
| `pimpl_circle.h`                           | `PimplCircle` decl, derived PIMPL | (header, no .o)   |
| `pimpl_circle.cpp`                         | `Impl` + member bodies            | (linker)          |
| `pimpl_square.h`                           | `PimplSquare` decl, derived PIMPL | (header, no .o)   |
| `pimpl_square.cpp`                         | `Impl` + member bodies            | (linker)          |
| `P-2026-06-19-pimpl-virtual-functions.cpp` | `main()` + 5 sections             | (linker)          |

The base (`PimplShape`) is *abstract*: it can't be instantiated,
only held via pointer or reference. Each derived (`PimplCircle`,
`PimplSquare`) is a concrete PIMPL class with the same Big Five
shape as Jun 17/18. The naming convention `pimpl_*` is the
giveaway, just like Jun 17/18.

## Key ideas

### Where the vptr lives, observed

The single most important observation in this lesson is the
size layout. On a 64-bit system (macOS, Linux, gcc/clang):

```
sizeof(void*)              = 8
sizeof(PimplShape)         = 8   (just a vptr; the base has no data)
sizeof(PimplCircle)        = 16  (vptr + unique_ptr<Impl>)
sizeof(PimplSquare)        = 16  (vptr + unique_ptr<Impl>)
```

The `PimplShape` base is 8 bytes. It has *no data members* --
the only thing it contains is the vptr. (The vptr is a hidden
pointer added by the compiler to every polymorphic class. It's
not in the source, but it occupies the first 8 bytes of the
object on the Itanium ABI used by gcc/clang.)

`PimplCircle` is 16 bytes. The layout is:

```
[vptr (8 bytes)] [impl_ (8 bytes)]
 ^               ^
 |               `--- std::unique_ptr<PimplCircle::Impl>
 `------------------- hidden, points at PimplCircle's vtable
```

The `Impl` itself lives on the heap. Its size is independent
of the vptr, the vtable, or the rest of `PimplShape`.

`PimplSquare` is also 16 bytes. Same layout. The two deriveds
have the *same* size, even though they have different data
(radius vs side) and different virtuals (well, they have the
same virtuals here, but if they had different ones, the size
would still be 16 -- only the vtable contents differ).

The lesson's program prints these sizes in Section 1. The
output is the headline:

> The vptr lives in the facade. The data lives in the Impl.
> The two are orthogonal.

### The "base is pure interface, derived is PIMPL" design

The natural alternative would be to give the *base* an
`Impl` of its own: `PimplShape::Impl`, then `PimplCircle::
Impl` derived from it, then each derived class's `impl_` is
a `unique_ptr<PimplCircle::Impl>` (a polymorphic pointer).
This works, but it means each object has *two* PIMPL
pointers: the base's `impl_` (which points at the
`PimplShape::Impl` part of the derived Impl) and the
derived's `impl_` (which points at the `PimplCircle::Impl`).
The two `impl_`s would shadow each other in the same object,
leading to a confusing tangle.

The cleaner design (which this lesson uses): the base has
*no data at all*. The base is a pure virtual interface --
a vtable and a handful of virtual function signatures. The
derived class is the PIMPL class, and it owns the `impl_`.

The trade-off is more boilerplate: each derived class has
its own PIMPL big-five. The benefit is clarity: the base
and the derived are clearly separated, and there's exactly
one `impl_` per object (in the derived class).

A real codebase with many deriveds would factor this out
with CRTP or a mixin. For a teaching example, the boilerplate
*is* the lesson.

### The virtual copy constructor (`clone()`)

The classic polymorphism trap: a `PimplShape copy = *shape_
ptr;` would *slice* -- only the base part of the object would
be copied, and the derived data (radius, side) would be
lost. The slice is silent; the program compiles, runs, and
gives the wrong answer.

For a non-abstract base, the fix is `shape_ptr->clone()`,
which returns a heap-allocated *derived* type owned via a
base pointer:

```cpp
std::unique_ptr<PimplShape> copy = original->clone();
```

The implementation in the derived class is:

```cpp
std::unique_ptr<PimplShape> PimplCircle::clone() const {
    return std::make_unique<PimplCircle>(*this);
}
```

`std::make_unique<PimplCircle>(*this)` calls the *copy ctor*
of `PimplCircle` with the current object as the source. The
result is a `unique_ptr<PimplCircle>`, which is implicitly
convertible to `unique_ptr<PimplShape>`. The caller gets a
base pointer that actually points at a derived object.

For an *abstract* base (this lesson), the slice is a *compile
error*:

```cpp
PimplShape sliced = *original;   // ERROR: cannot instantiate
                                 //        abstract class
```

The pure-virtual `print()` and `clone()` make `PimplShape`
abstract, so the compiler refuses to construct one. That's
the strongest possible defense against slicing.

### The polymorphic container

The "killer app" of polymorphism is a container that holds
heterogeneous types:

```cpp
std::vector<std::unique_ptr<PimplShape>> shapes;
shapes.push_back(std::make_unique<PimplCircle>(101, 1.5));
shapes.push_back(std::make_unique<PimplSquare>(102, 2.0));
shapes.push_back(std::make_unique<PimplCircle>(103, 3.0));

for (const auto& s : shapes) {
    s->print("from container");   // virtual dispatch
}
```

The container holds `unique_ptr<PimplShape>` -- base-class
pointers. The actual objects are circles and squares. The
`push_back` works because `unique_ptr<PimplCircle>` is
implicitly convertible to `unique_ptr<PimplShape>`. The
`print()` call dispatches through the vtable to the right
derived.

The program walks through this in Section 2. The output shows
two circles and two squares being printed in the right order,
with the right `name()` and `kind()`. The loop doesn't know
or care which is which; the vtable does.

The cost: to call a *specific* accessor (like `radius()` or
`side()`), you need a derived reference. The way to get one
from a base pointer is `dynamic_cast`. Section 2 also shows
this: a second loop uses `dynamic_cast<PimplCircle*>` and
`dynamic_cast<PimplSquare*>` to find the right derived.

(For a real codebase, the visitor pattern is often a better
fit -- that's the Jun 15 lesson's territory. `dynamic_cast`
is fine for a small example.)

### The Big Five of a derived PIMPL class

The Big Five for a derived PIMPL class is the same shape as
the non-derived PIMPL (Jun 17/18), with one tweak: the dtor
is `override`.

| Special member          | Where it goes                              | Why                                                                                |
|-------------------------|--------------------------------------------|------------------------------------------------------------------------------------|
| Default ctor            | `.cpp`                                     | The ctor calls `std::make_unique<Impl>()`, which needs `Impl` complete.            |
| Destructor              | `= default` in `.cpp` (`override` in header) | `delete impl_` needs `Impl` complete; the dtor must be defined where the type is visible. The `override` keyword is a safety net for the virtual signature. |
| Copy ctor               | Declared in header, defined in `.cpp`      | `unique_ptr` is non-copyable; we declare a deep-copy ctor in the header to suppress the implicit deleted one, and define it in the `.cpp` where `Impl` is complete. |
| Copy assign             | Declared in header, defined in `.cpp`      | Same reason as copy ctor.                                                          |
| Move ctor               | `= default` in header                       | Moving a `unique_ptr<Impl>` is a pointer transfer, no need for `Impl` to be complete. |
| Move assign             | `= default` in header                       | Same reason.                                                                       |

Section 4 of the program walks through each operation. The
trace shows the deep copies (the copy ctor prints a line),
and the move ops (no print, just pointer transfer).

The `override` keyword on the dtor is the only difference
from Jun 17/18. It tells the compiler "I expect this dtor
to override a base class's virtual dtor." If the base's
dtor signature ever changes (or the base stops having a
virtual dtor), the compiler will flag the `override` as
not overriding anything, and we'll know immediately.

### Pass-by-base-reference is the generic shape-handling pattern

A function that takes `const PimplShape&` is the *generic*
way to operate on a shape. The function doesn't know the
dynamic type, only the interface:

```cpp
void print_shape(const PimplShape& s, const std::string& tag) {
    std::cout << "  print_shape(" << tag << "): ";
    s.print(tag);
}
```

Calling `print_shape(circle, "...")` and `print_shape
(square, "...")` works because the function takes a base
reference, and the vtable dispatches `print()` to the
right derived. Section 5 of the program demonstrates this
in two lines.

The alternative is a `switch` on the dynamic type, or a
`dynamic_cast` to a known derived. Both are more verbose
and less extensible. Pass-by-base-reference is the C++
idiom for "I want a function that works for any derived
of this base, polymorphically."

## Design choices and trade-offs

### Why the base has no data, and each derived owns its own `impl_`

The natural alternative is a polymorphic `Impl`: `PimplShape::
Impl` is the base, `PimplCircle::Impl` derives from it, the
derived class's `impl_` is a `unique_ptr<PimplCircle::Impl>`
pointing at the derived Impl (and through it, at the base
Impl). This works, but each object has *two* PIMPL pointers
in its layout (the base's `impl_` and the derived's
`impl_`), and they shadow each other in the same object.

The cleaner design: the base has *no data*. The base is
just a vptr and a few virtual function signatures. The
derived class is the PIMPL class, and it owns the only
`impl_` in the object.

This is a stylistic choice, not a hard rule. The polymorphic-
`Impl` design is used in some codebases (notably Chromium's
content layer). The "base is pure interface" design is more
common in modern C++ (it's the basis of the
`std::pmr::memory_resource` family, for example). The
lesson uses the second design because it's simpler and
easier to teach.

### Why `name()` and `kind()` are non-pure virtual

The base declares `name()` and `kind()` as `virtual const
char* name() const` and `virtual ShapeKind kind() const`,
not pure virtual. The deriveds *can* override, but if they
don't, the base's defaults ("shape" / `kShape`) are used.

This is a small ergonomic choice: a derived that doesn't
care about its name (say, a debug-only or test-only derived)
can skip the override and inherit the base's default. A
real shape (Circle, Square) overrides both.

The alternative -- making `name()` and `kind()` pure
virtual -- would force every derived to spell them out.
For a teaching example, that's extra ceremony for no
benefit. The lesson keeps them non-pure.

### Why `clone()` returns `unique_ptr<PimplShape>`, not `unique_ptr<PimplCircle>`

Covariant return types are allowed for raw pointers and
`std::shared_ptr`, but *not* for `std::unique_ptr`. (The
reason is that `unique_ptr`'s converting constructor is
a template, not a constructor of a related type, and
templates don't have the same covariance rules.) So
`clone()` is declared as `unique_ptr<PimplShape>` in
the base, and the derived implements it as
`unique_ptr<PimplShape>` even though it returns a
`unique_ptr<PimplCircle>`. The implicit derived-to-base
conversion happens at the return statement.

If we had used `shared_ptr` instead, the derived could
declare its override as
`shared_ptr<PimplCircle> clone() const override` and
the compiler would accept the covariance. (That's a
nice feature of `shared_ptr`, but it's a digression
from the lesson's PIMPL focus.)

### Why no `noexcept` on the virtuals

`print()` doesn't throw (it just writes to `std::cout`).
`name()` and `kind()` don't throw. `clone()` allocates
and might throw `std::bad_alloc`. The dtor is implicitly
`noexcept` (all dtors are, by [C++ standard rules](https://en.cppreference.com/w/cpp/language/destructor)).

I left `noexcept` off the virtuals in this lesson because
marking them all `noexcept` is mostly cosmetic. A future
lesson could add `noexcept` to `name()` and `kind()` for
clarity, but it's not load-bearing for the PIMPL +
polymorphism story.

## Cross-references and follow-ups

- **Jun 17 (PIMPL `Box`)** — the same PIMPL pattern, applied
  to a single class. The boilerplate is identical. This
  lesson adds *polymorphism* on top of PIMPL.
- **Jun 18 (PIMPL `Inventory`)** — the same pattern, applied
  to a class with a `vector<Box>`. The big-five table from
  Jun 18 is reproduced (with the `override` keyword) above.
- **Jun 9 (rvalue refs, `std::move`)** — the move ops are
  `= default` in the header. The same shape as the move
  ops in this lesson. The only difference is the `override`
  on the dtor.
- **Jun 4 (smart pointers)** — `std::unique_ptr<Impl>` is
  the heart of PIMPL, and `std::unique_ptr<PimplShape>` is
  the heart of the polymorphic container in Section 2.
  The lesson's polymorphic container would also work with
  `std::shared_ptr<PimplShape>`, but `unique_ptr` is the
  more direct fit (no need for shared ownership of shapes).
- **Jun 15 (`std::visit` with stateful visitors)** — the
  alternative to `dynamic_cast` for "do something specific
  based on the dynamic type." A real shape library might
  use the visitor pattern for shape-specific operations
  (compute area, draw, etc.) instead of a `switch` on
  `kind()` or a `dynamic_cast`. A `std::variant<Circle,
  Square>` would also work, but loses the polymorphism
  in the container.
- **CRTP (Curiously Recurring Template Pattern)** — a
  way to factor the PIMPL boilerplate out of the derived
  classes, so each derived doesn't have to re-declare the
  big-five. The lesson's design (base = pure interface,
  derived = PIMPL) could be tightened with a CRTP base
  that provides the boilerplate. (Future lesson, not
  pursued here.)

## Next Steps

- **PIMPL `PimplInventory` with `shared_ptr`** (carried
  over from Jun 18) — replace the `unique_ptr<Impl>` with
  a `shared_ptr<Impl>`. The copy ctor / assign become
  defaultable in the header. The deep copy disappears.
  The cost is reference counting. A useful comparison
  and a small lesson. Still on the list.
- **Visitor pattern for shapes** — implement a
  `ShapeVisitor` with `visit(Circle&)` and `visit(Square&)`,
  and a `PimplShape::accept(visitor&)` method that
  dispatches via double-dispatch. The combination of PIMPL
  and visitor is a common pattern in real codebases (e.g.
  ASTs in compilers). New on the list.
- **`std::variant<Circle, Square>` instead of `vector
  <unique_ptr<PimplShape>>`** — the alternative to
  polymorphism. Compare the two designs in a small program:
  variant is heap-free and stack-only, polymorphism is
  heap-allocated and extensible. Both have their place.
  New on the list.
- **PIMPL and ABI** — compile two TUs: one that includes
  `pimpl_shape.h` only, one that defines a shape. Save the
  first's `.o`. Edit the shape's `.cpp` (add a field to
  the `Impl`), recompile *only* the shape's `.cpp`, link
  the binary, and run. The first TU's `.o` is unchanged.
  The lesson's capstone. (Carried over from Jun 17/18.)
- **PIMPL + virtuals + multiple inheritance** — give
  `PimplShape` a second base, e.g. `Serializable`, with
  its own virtual `serialize()`. The PIMPL pointer lives
  in the derived class, but the vptr layout is now more
  complex (two vtables, two vptrs, careful ordering of
  base subobjects). A small demo would be illuminating.
  New on the list.
- **A `ShapeFactory` that returns `unique_ptr<PimplShape>`**
  — a function that takes a `ShapeKind` and a numeric
  parameter, and returns a heap-allocated shape. Tests
  the polymorphic interface from the *creator* side
  (the lesson tests it from the *consumer* side). New
  on the list.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit (all under `late-may/cpp_practice/`):
  - `P-2026-06-19-pimpl-virtual-functions.cpp` (main)
  - `P-2026-06-19-pimpl-virtual-functions.md` (this file)
  - `pimpl_shape.h`
  - `pimpl_shape.cpp`
  - `pimpl_circle.h`
  - `pimpl_circle.cpp`
  - `pimpl_square.h`
  - `pimpl_square.cpp`
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*Combining PIMPL with polymorphism shows the pattern's
modularity. The base `PimplShape` is a pure virtual interface
with no data -- just a vptr. The deriveds `PimplCircle` and
`PimplSquare` are concrete PIMPL classes, each with their
own `unique_ptr<Impl>`. The two are orthogonal: polymorphism
lives in the facade (vtable), and data lives in the Impl
(heap). The lesson's empirical observation -- `sizeof
(PimplShape) == 8`, `sizeof(PimplCircle) == sizeof
(PimplSquare) == 16` -- is the proof. The polymorphic
container (`vector<unique_ptr<PimplShape>>`), the virtual
copy constructor (`clone()`), and the pass-by-base-reference
idiom are the natural consequences. The PIMPL boilerplate
from Jun 17/18 is unchanged: forward-declared `Impl`,
`unique_ptr<Impl>`, declared-in-header / defined-in-`.cpp`
dtor (with `override` this time) and copy ops, `= default`
move ops. The pattern generalises once more: a class can be
PIMPLed and polymorphic, and the two don't interfere.*
