# C++ practice 2026-06-20 — Visitor pattern for PIMPL shapes

## What I set out to learn

The Jun 17 / Jun 18 / Jun 19 lessons built up the PIMPL pattern:
- Jun 17: a single concrete PIMPL class (`PimplBox`)
- Jun 18: a PIMPL class with a `vector` member (`PimplInventory`)
- Jun 19: a *polymorphic* PIMPL class (`PimplShape` base + `PimplCircle` /
  `PimplSquare` deriveds), with the vptr observed to live in the facade.

The Jun 19 "Next Steps" list had, as one of its items:

> **Visitor pattern for shapes** — implement a `ShapeVisitor` with
> `visit(Circle&)` and `visit(Square&)`, and a `PimplShape::accept
> (visitor&)` method that dispatches via double-dispatch. The
> combination of PIMPL and visitor is a common pattern in real
> codebases (e.g. ASTs in compilers). New on the list.

This lesson is that follow-up. The goal is to add the Visitor
pattern to the existing polymorphic PIMPL hierarchy, observe the
double-dispatch in action, and compare the visitor approach to
the `dynamic_cast` switch that Jun 19's Section 2 used.

The lesson's three concrete visitors (`AreaVisitor`,
`BoundingBoxVisitor`, `JsonVisitor`) each show a different shape
(pun intended) of accumulated state: a single number, four
numbers that update together, and a string. The lesson walks
through each, then explicitly contrasts the visitor pattern
against the `dynamic_cast` chain.

## What "double-dispatch" actually means

The Visitor pattern is the C++ idiom for *double-dispatch*: the
right method is chosen based on the runtime type of *both* the
shape AND the visitor. C++ has built-in support for single
dispatch (virtual functions) — picking the right method based on
one runtime type. Double dispatch falls out of two single
dispatches in sequence.

The setup:

```cpp
class ShapeVisitor {
public:
    virtual void visit(const PimplCircle& c) const = 0;
    virtual void visit(const PimplSquare& s) const = 0;
};

class PimplShape {
public:
    virtual void accept(ShapeVisitor& v) const = 0;
};

class PimplCircle : public PimplShape {
public:
    void accept(ShapeVisitor& v) const override { v.visit(*this); }
};
```

When a client calls `shape->accept(v)`:

1. **First dispatch (virtual on the shape):** the vtable in
   `*shape` picks `PimplCircle::accept` (or `PimplSquare::accept`)
   based on the runtime type of `*shape`.
2. **Second dispatch (virtual on the visitor):** inside
   `PimplCircle::accept`, the compiler picks `visit(const
   PimplCircle&)` at compile time (because `*this` has static type
   `PimplCircle&`). Then the vtable in `v` picks
   `AreaVisitor::visit(PimplCircle&)` (or `JsonVisitor::visit(...)`,
   etc.) based on the runtime type of `v`.

The combination of the two dispatches is the "double" in
"double-dispatch." The result: the right `visit(Derived&)` is
called for the right combination of shape and visitor, with no
`dynamic_cast` and no `switch` on type.

## Build and run

```bash
# One-liner, matching the cron's default flags.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-20-visitor-pattern \
    P-2026-06-20-visitor-pattern.cpp \
    pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp \
    concrete_visitors.cpp
./P-2026-06-20-visitor-pattern

# ASan + UBSan, for the run that proves no leaks and no UB.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    -o P-2026-06-20-visitor-pattern-asan \
    P-2026-06-20-visitor-pattern.cpp \
    pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp \
    concrete_visitors.cpp
./P-2026-06-20-visitor-pattern-asan
```

Both builds are clean (no warnings under `-Wall -Wextra
-Wpedantic`). The output walks through six sections: container
setup, three visitors (area / bounding box / JSON), a head-to-head
comparison of visitor vs `dynamic_cast`, and a note on PIMPL
encapsulation.

## Files in this lesson

| File                              | Role                                     |
|-----------------------------------|------------------------------------------|
| `P-2026-06-20-visitor-pattern.cpp` | `main()` + 6 sections                   |
| `shape_visitor.h`                 | `ShapeVisitor` base (pure virtuals)      |
| `concrete_visitors.h`             | `AreaVisitor`, `BoundingBoxVisitor`, `JsonVisitor` |
| `concrete_visitors.cpp`           | The three visitors' implementations      |
| `pimpl_shape.{h,cpp}`             | **MODIFIED** — added `accept()` virtual  |
| `pimpl_circle.{h,cpp}`            | **MODIFIED** — added `accept()` override |
| `pimpl_square.{h,cpp}`            | **MODIFIED** — added `accept()` override |

The `pimpl_*` files are the Jun 19 files; this lesson adds one
virtual method (`accept(ShapeVisitor&) const`) to each of them.
The Jun 19 binary still builds and runs identically (the vtable
grows, but `sizeof(PimplShape)` is unchanged — the lesson's
observation from Jun 19 holds).

The new files (`shape_visitor.h`, `concrete_visitors.{h,cpp}`) are
the visitor side of the pattern. The naming convention is
unchanged from Jun 17/18/19: `pimpl_*` for the data hierarchy,
plain names for the visitor side.

## Key ideas

### The double-dispatch, observed

The lesson's Section 2 (`AreaVisitor`) is the simplest case: a
visitor that sums the area of every visited shape. The output:

```
Visited 4 shapes; total area = 39.5929
Expected              = 39.5929
Match: YES
```

The expected value is `π·1.5² + 2² + π·3² + 0.5² ≈ 39.5929`. The
match is exact — the visitor accumulates the right total.

The "magic line" is `s->accept(v)` inside the apply loop. Each
shape's `accept` does `v.visit(*this)`, where `*this` has the
right static type for the compiler to pick the right `visit`
overload. The visitor's vtable then picks the right concrete
visitor body. No casts, no switches.

### Three visitors, three kinds of state

The lesson uses three visitors to show that the pattern isn't
limited to a single accumulator:

| Visitor             | State                       | Purpose                                  |
|---------------------|-----------------------------|------------------------------------------|
| `AreaVisitor`       | one `double` (the total)    | Sum areas across many shapes.            |
| `BoundingBoxVisitor`| four `double`s (min/max x/y)| Track the smallest enclosing rectangle.  |
| `JsonVisitor`       | one `std::string`           | Build a JSON-like serialization.         |

`AreaVisitor` is the trivial case. `BoundingBoxVisitor` shows
that the visitor's state can be *compound* (multiple fields
updating together). `JsonVisitor` shows that the visitor can
build a richer data structure than a number — it grows a string
by appending.

All three visitors' state is `mutable` because `visit()` is
`const` on the visitor (the visitor observes the shape, but its
own accumulator changes). This is the standard idiom for
"logical const, mutable accumulator."

The ctor of `BoundingBoxVisitor` calls `reset()` so that the
first `visit()` shrinks the box correctly (the first min/max
update must be from ±infinity, otherwise the initial state
would dominate the result). The lesson's program hits this bug
on first run, then fixes it — see the "bug story" section below.

### Visitor vs `dynamic_cast` switch

Section 5 of the program solves the same problem (compute total
area) two ways and compares the code:

```cpp
// (a) dynamic_cast switch
double total_area_dynamic_cast(const vector<unique_ptr<PimplShape>>& shapes) {
    double total = 0.0;
    for (const auto& s : shapes) {
        if (auto c = dynamic_cast<const PimplCircle*>(s.get())) {
            total += 3.14159 * c->radius() * c->radius();
        } else if (auto sq = dynamic_cast<const PimplSquare*>(s.get())) {
            total += sq->side() * sq->side();
        }
    }
    return total;
}

// (b) Visitor pattern
double total_area_visitor(const vector<unique_ptr<PimplShape>>& shapes) {
    AreaVisitor area;
    apply_visitor(shapes, area);
    return area.total();
}
```

Both produce the same number (39.5929). The difference is in
how much boilerplate each requires, and how they scale:

- The `dynamic_cast` version needs a new `else if` branch for
  every new derived. The branches grow as the hierarchy grows.
- The visitor version needs only one line (`s->accept(v)`) in
  the apply loop. New deriveds only need a new `visit(NewType&)`
  override on the visitor; the apply loop is unchanged.

This is the classic Visitor pattern trade-off: *closed for new
operations, open for new types* becomes *open for new
operations, closed for new types*. The data hierarchy becomes
hard to extend (every new derived needs a new visit override on
every existing visitor) but new operations are trivial (just add
a new visitor).

For shape hierarchies where the set of types is stable (the
common case: ASTs, expression trees, shape libraries), this is a
good trade. For hierarchies where new types are added often
(say, a plugin system), `std::variant<Circle, Square, ...>` is
often a better fit — that's the Jun 15 lesson's territory.

### The visitor respects PIMPL encapsulation

A subtle point: the visitors reach the shape's data through
*public accessors only* (`radius()`, `side()`). They never
poke into `PimplCircle::Impl` or `PimplSquare::Impl`.

This is the visitor pattern's compatibility with PIMPL:

- PIMPL says "the data is private to the .cpp file; the public
  API is the only stable surface."
- The visitor pattern says "I want to call type-specific code
  without dynamic_cast."
- Combining them: the visitor's `visit(Derived&)` calls the
  derived's public accessors. The visitor doesn't need (and
  can't have) visibility into the Impl. The public accessors
  are the visitor's only window into the shape's data.

This is a real benefit. The visitor pattern preserves the
encapsulation, which means the Impl can change (e.g. add new
fields, reorganize layout, switch to a different storage
strategy) without breaking the visitor — as long as the public
accessors preserve their meaning.

The lesson's program notes this in Section 6. The price: the
visitor can only see what the public accessors expose. If the
JSON visitor wanted to include `id_` in its output, it'd need
a new public accessor (`id()`), or it'd need to be a friend of
the shape.

### The visitor pattern lives alongside `print()` and `clone()`

The Jun 19 hierarchy had virtual methods: `print()`,
`clone()`, `name()`, `kind()`. This lesson adds one more:
`accept(ShapeVisitor&)`. The PIMPL + visitor hierarchy now has:

| Virtual method              | Where it dispatches       | What it does                                  |
|-----------------------------|---------------------------|-----------------------------------------------|
| `print(const string&)`      | The shape                 | Self-description                             |
| `clone()`                   | The shape                 | Virtual copy constructor                      |
| `name()` / `kind()`         | The shape                 | Type identification                          |
| `accept(ShapeVisitor&)`     | The shape AND the visitor | Double-dispatch to a type-specific operation |

`accept()` is the only one that does *two* virtual calls. The
others are single-dispatch (just on the shape's runtime type).

In a real codebase, `print()` and `accept()` overlap
significantly: both do "do something type-specific." The
difference is that `print()` knows the operation at compile
time (printing) and `accept()` parameterizes the operation via
the visitor. For shape libraries with a small set of stable
operations (print, area, serialize), `print()`-style methods
might be enough. For shape libraries where the operations are
open-ended (user-defined analyses, custom serializers), the
visitor pattern is the right abstraction.

## Design choices and trade-offs

### Why the visitor's `visit` takes `const &`, not `&`

A subtle point that the lesson handles carefully. The base
class's `accept` is declared `void accept(ShapeVisitor& v)
const` — `const` because the shape is being observed, not
modified. Inside the derived's `accept` body, `*this` is
`const PimplCircle&`.

If `visit` took `PimplCircle&` (non-const), the call
`v.visit(*this)` would try to bind `const PimplCircle&` to
`PimplCircle&`, which is a const violation and a compile error.

Two ways out:

1. Make `visit` take `const PimplCircle&` (the lesson's choice).
2. Make `*this` non-const by declaring `accept` non-const.

The lesson uses option 1. The argument: a visitor that *mutates
the shape* would be unusual. What a visitor mutates is itself
(the accumulator), not the shape it's visiting. Taking the
shape by `const &` enforces this — the visitor can read the
shape but can't write to it. The visitor's `this` (the
accumulator) is non-const, so the visitor's state can still
change.

A visitor that genuinely needs to mutate the shape (e.g. a
"transform" visitor that scales every shape by some factor)
would need a different design — maybe a non-const accept, or a
visitor that returns a new shape. The lesson doesn't go there
because it's not the common case.

### Why `ShapeVisitor` is opt-in (forward-declared in `pimpl_shape.h`)

The visitor pattern is a separate concern from "polymorphic
shape." A client that just wants to call `print()` or `clone()`
polymorphically doesn't need to know about visitors. To
preserve this, the lesson uses a *forward declaration* of
`ShapeVisitor` in `pimpl_shape.h`, rather than including
`shape_visitor.h`.

```cpp
// pimpl_shape.h
class ShapeVisitor;  // forward declaration

class PimplShape {
    virtual void accept(ShapeVisitor& v) const = 0;  // takes visitor by ref,
                                                     // so the forward decl is
                                                     // enough
    // ...
};
```

A client TU that wants to use the visitor pattern includes
both `pimpl_shape.h` and `shape_visitor.h`. A client TU that
only wants the basic polymorphic interface (print / clone /
name / kind) just includes `pimpl_shape.h`. The visitor is
opt-in, and `pimpl_shape.h` stays cheap to include.

This is the standard C++ idiom for "I want to refer to this
type by reference / pointer, but I don't need its full
definition": forward declaration in the header, full definition
in the implementation file. The visitor base itself is in
`shape_visitor.h`, which is included only where needed.

### Why no `noexcept` on `visit`

`visit` for `AreaVisitor` doesn't throw (it just adds doubles).
`visit` for `JsonVisitor` might throw `std::bad_alloc` when the
string grows. `visit` for `BoundingBoxVisitor` doesn't throw.

The lesson leaves `noexcept` off all three `visit` overrides.
Adding `noexcept` to `AreaVisitor::visit` and
`BoundingBoxVisitor::visit` would be a small win for callers
that care about exception safety, but it's mostly cosmetic. A
future lesson could add `noexcept` selectively, but it's not
load-bearing for the visitor pattern itself.

## Cross-references and follow-ups

- **Jun 19 (PIMPL + virtuals)** — this lesson extends the Jun
  19 hierarchy with one new virtual method (`accept`). The
  PIMPL + polymorphism pattern from Jun 19 is unchanged; the
  vtable grew by one entry, but `sizeof(PimplShape)` is still 8
  bytes. The lesson verifies this by re-running the Jun 19
  binary after the changes (still clean, sizes unchanged).
- **Jun 15 (`std::visit` with stateful visitors)** — the
  `std::variant` analog of this lesson. With `std::variant
  <Circle, Square>`, you don't have a polymorphic base class —
  you have a tagged union. `std::visit` dispatches to the right
  `operator()(Circle&)` / `operator()(Square&)` at runtime. The
  two approaches (polymorphism + visitor pattern vs variant +
  std::visit) solve the same problem with different trade-offs.
  Polymorphism is open for new types (a new derived requires
  touching every visitor); variant is open for new operations
  (a new visitor-like callable requires touching every type).
- **Jun 9 (rvalue refs, `std::move`)** — the visitors' state
  is `mutable`, which means `visit()` is `const` on the visitor
  but can still mutate the accumulator. This is unrelated to
  rvalue references, but it's the same "logical const" idiom
  that comes up in other contexts.
- **Compiler ASTs and expression trees** — the classic use
  case for the Visitor pattern. A compiler's AST has many node
  types (`BinaryOp`, `UnaryOp`, `Literal`, `Identifier`, ...),
  and operations on the AST (type-checking, code generation,
  optimization) are open-ended. The Visitor pattern is the
  standard C++ idiom for "many types, many operations."
- **CRTP for visitors** — the "static" version of the Visitor
  pattern, where the visitor is a template parameter and
  dispatch happens at compile time. Faster runtime, but more
  boilerplate and harder to compose. (Future lesson, not
  pursued here.)

## Bug story: `BoundingBoxVisitor` started with garbage

A teaching moment worth flagging. The first cut of
`BoundingBoxVisitor` had:

```cpp
class BoundingBoxVisitor : public ShapeVisitor {
public:
    BoundingBoxVisitor() = default;
    // ...
private:
    mutable double min_x_;
    mutable double max_x_;
    // ...
};
```

The `= default` ctor doesn't initialize the doubles. They're
left with indeterminate values. The first `visit` updates them
from those indeterminate values, but subsequent visits compare
against whatever the *previous* visit left. The output was:

```
min_x = -3
max_x = 3
min_y = -3
max_y = 39.5929     <-- BUG: should be 3, not the previous section's total area
width  = 6
height = 42.5929    <-- (= 39.5929 - (-3))
```

The number `39.5929` is suspiciously equal to the total area
from Section 2. The reason: the doubles' memory happened to
contain the previous section's accumulated total. The lesson
is that **a visitor with mutable state needs to initialize that
state in the constructor.** The fix:

```cpp
BoundingBoxVisitor() { reset(); }
```

Now the ctor calls `reset()`, which sets min/max to ±infinity.
The first `visit()` then *shrinks* the box correctly.

A subtler fix would be to default-initialize the doubles in the
class body:

```cpp
private:
    mutable double min_x_ =  std::numeric_limits<double>::infinity();
    mutable double max_x_ = -std::numeric_limits<double>::infinity();
    // ...
```

But that duplicates the reset() logic. The lesson prefers the
"always call reset()" idiom in the ctor so that `reset()` is the
single source of truth for the initial state. (And the lesson
*notes* this bug in the implementation file, so future readers
won't trip on it.)

## Next Steps

- **Visitor with return values** — currently each visitor
  accumulates state in itself. A more functional style would
  have `visit` *return* a value (e.g. `double visit(const
  PimplCircle&) const`), and the apply loop would fold the
  results. The visitor pattern with returns is closer to
  `std::visit` (Jun 15) in spirit. A useful comparison.
- **`std::variant<Circle, Square>` head-to-head** — re-implement
  the three visitors as `std::variant<PimplCircle, PimplSquare>`
  + `std::visit`. Compare the runtime (single virtual vs single
  index-into-jump-table), the compile time (templates vs
  vtables), and the code clarity. The Jun 15 lesson covered
  `std::visit`; this would be a direct comparison.
- **Acyclic visitor** — the classic Visitor pattern's weakness
  is the dependency cycle: every new derived needs a new
  `visit(Derived&)` on every visitor. The "acyclic visitor"
  pattern (Keyes, then Czarnecki & Eisenecker) breaks this by
  using `dynamic_cast` *inside* the visitor's `visit(Base&)`.
  Loses the double-dispatch but gains extensibility. A good
  follow-up for "when the visitor pattern isn't the right
  choice."
- **CRTP static visitor** — the compile-time version of the
  Visitor pattern. Each concrete visitor is a template that
  visits a specific type, and dispatch happens via `if constexpr`
  (C++17) instead of vtables. Faster but more boilerplate.
  Useful for tight inner loops where vtable indirection is too
  slow.
- **PIMPL and ABI** (carried over from Jun 17/18/19) — the
  capstone exercise for the PIMPL series. Compile two TUs:
  one that includes `pimpl_shape.h` only, one that defines a
  shape. Save the first's `.o`. Edit the shape's `.cpp` (add
  a field to the `Impl`), recompile *only* the shape's `.cpp`,
  link the binary, and run. The first TU's `.o` is unchanged.
- **PIMPL `PimplInventory` with `shared_ptr`** (carried over
  from Jun 18/19) — replace the `unique_ptr<Impl>` with a
  `shared_ptr<Impl>`. The copy ctor / assign become
  defaultable in the header. The deep copy disappears. The
  cost is reference counting.
- **`std::variant<Circle, Square>` instead of `vector
  <unique_ptr<PimplShape>>`** (carried over from Jun 19) — the
  alternative to polymorphism. Compare the two designs in a
  small program: variant is heap-free and stack-only,
  polymorphism is heap-allocated and extensible.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files to commit (all under `late-may/cpp_practice/`):
  - `P-2026-06-20-visitor-pattern.cpp` (main)
  - `P-2026-06-20-visitor-pattern.md` (this file)
  - `shape_visitor.h` (the visitor base)
  - `concrete_visitors.h` (three visitor declarations)
  - `concrete_visitors.cpp` (three visitor implementations)
- **Modified files (additive changes — `accept()` virtual):
  - `pimpl_shape.h`
  - `pimpl_circle.h`
  - `pimpl_circle.cpp`
  - `pimpl_square.h`
  - `pimpl_square.cpp`
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*The Visitor pattern adds double-dispatch to the polymorphic
PIMPL hierarchy: a base reference dispatches to the right
`visit(Derived&)` for the right concrete visitor, with no
`dynamic_cast` and no `switch` on type. Three concrete visitors
(AreaVisitor, BoundingBoxVisitor, JsonVisitor) demonstrate the
pattern with three different kinds of accumulated state. The
visitor respects PIMPL encapsulation — visitors reach the
shape's data through public accessors only. The combination of
PIMPL + visitor is the standard C++ idiom for "many types, many
operations" hierarchies like compiler ASTs and shape libraries.
The pattern's classic trade-off: adding new *operations* (new
visitors) is trivial; adding new *types* (new derived shapes)
requires updating every existing visitor.*