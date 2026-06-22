# C++ practice 2026-06-22 — CRTP static visitor

## What I set out to learn

The Jun 21 session's "Next Steps" list pointed here directly:

> **CRTP static visitor** — compile-time visitor using templates +
> `if constexpr` instead of vtables. The zero-cost version of
> `std::visit`.

This is that session. The whole variant/visitor arc has been
building toward a single question: **how do I dispatch a function
over a heterogeneous set of types, and how do the choices trade
off?**

| Session | Representation      | Dispatch             | Cost         |
|---------|---------------------|----------------------|--------------|
| Jun 20  | `unique_ptr<Base>`  | vtable + virtual     | 2 indirections (Visitor) |
| Jun 10/15/21 | `std::variant<T1,T2,...>` | index + jump table | 1 indirect jump |
| **Jun 22** | **CRTP `Shape<D>`** | **compile-time overload resolution** | **0 indirections** |

The CRTP static visitor is the "I want every dispatch fully
devirtualized" answer. The price is what CRTP always costs: no
runtime polymorphism (you can't store `Shape<???>*` polymorphically),
and code bloat from N×M template instantiations.

## The CRTP base

```cpp
template <typename Derived>
class Shape {
public:
    const Derived& self() const noexcept {
        return *static_cast<const Derived*>(this);
    }
protected:
    Shape() = default;
    ~Shape() = default;
    Shape(const Shape&) = default;
    Shape& operator=(const Shape&) = default;
    Shape(Shape&&) noexcept = default;
    Shape& operator=(Shape&&) noexcept = default;
};
```

The whole trick of CRTP: the base takes the *derived* type as a
template parameter, and uses `static_cast<const Derived*>(this)` to
"downcast" — but at a point where the static type system already
knows the cast is safe, because `Derived` is part of the static
type of `this`.

`self()` is the classical CRTP helper: a way to get back to the
derived type. For visitor dispatch I don't even need `self()` — the
visitor's `operator()` overloads are called directly with a typed
reference. But `self()` is the canonical CRTP breadcrumb, and it's
worth having for documentation.

## The concrete shapes

```cpp
class Circle : public Shape<Circle> {
public:
    explicit Circle(double r) : r_(r) {}
    double radius() const noexcept { return r_; }
private:
    double r_;
};

class Square : public Shape<Square> {
public:
    explicit Square(double s) : s_(s) {}
    double side() const noexcept { return s_; }
private:
    double s_;
};
```

Note: `Circle` derives from `Shape<Circle>` and `Square` from
`Shape<Square>`. There is **no common base type**. `Shape<Circle>*`
is not convertible to `Shape<Square>*`. This is the central
constraint of CRTP — and it's what makes the dispatch zero-cost.

Compared to the dynamic reference:

```cpp
sizeof(Circle)  = 8   (just the radius)
sizeof(Square)  = 8   (just the side)
sizeof(DCircle) = 16  (8 vptr + 8 radius)
sizeof(DSquare) = 16  (8 vptr + 8 side)
```

The vtable pointer costs 8 bytes per object on this build. For
5 shapes that's 40 bytes of vptrs that exist solely so that
`shape->accept(visitor)` can dispatch at runtime. CRTP doesn't
need any of it.

## The static visitors — two flavors

### (a) Overloaded `operator()` — the textbook static visitor

```cpp
struct AreaVisitor {
    double operator()(const Circle& c) const {
        constexpr double pi = 3.14159265358979323846;
        return pi * c.radius() * c.radius();
    }
    double operator()(const Square& s) const {
        double side = s.side();
        return side * side;
    }
};
```

When you write `AreaVisitor{}(some_circle)`, overload resolution
picks `operator()(const Circle&)` based on the *static* type of
`some_circle`. This happens entirely at compile time. The generated
code is the same as if you had written the body inline at the call
site.

`BoundingBoxVisitor` and `JsonVisitor` follow the same pattern,
each with two `operator()` overloads (one for `Circle`, one for
`Square`). Same data layout, same operations as Jun 21.

### (b) Generic `operator()` + `if constexpr` — the compact form

```cpp
struct GenericAreaVisitor {
    template <typename ShapeT>
    double operator()(const ShapeT& shape) const {
        using S = std::decay_t<ShapeT>;
        if constexpr (std::is_same<S, Circle>::value) {
            constexpr double pi = 3.14159265358979323846;
            return pi * shape.radius() * shape.radius();
        } else if constexpr (std::is_same<S, Square>::value) {
            double side = shape.side();
            return side * side;
        } else {
            static_assert(sizeof(ShapeT) == 0,
                "GenericAreaVisitor: unhandled shape type");
        }
    }
};
```

This is the same idea as (a) but expressed as a single template
function whose body uses `if constexpr` to specialize per type.
The compiler instantiates this template twice (once for `Circle`,
once for `Square`), each time discarding the irrelevant `if
constexpr` arm entirely. After compilation, the generated code is
*identical* to (a) — same machine code in the same two bodies.

The `static_assert` in the `else` branch is the C++17 equivalent
of a "you forgot to handle a case" compiler error. It's the
"exhaustive switch" check that `std::visit` also gives you: if you
add a `Triangle` type and forget to handle it here, the build
fails.

(C++20 lets you write `auto operator()(const auto& shape)` — an
*abbreviated function template*. I stuck with the C++17 form
because the cron workflow's standard build flag is `-std=c++17`.)

## The container — `std::tuple<vector<Circle>, vector<Square>>`

CRTP gives up one thing that the previous representations had: a
common base type you can put in a single container. You cannot
have `vector<Shape<???>>`. So the "heterogeneous collection" needs
to be something other than a vector of base pointers.

The cleanest C++17 option for a fixed, known-in-advance type set is
a `std::tuple<vector<Circle>, vector<Square>>`. It's exactly what
the type system has: two vectors, side by side. The dispatch is
then a small loop per vector:

```cpp
double total_area(const ShapeCollection& sc) {
    double sum = 0.0;
    const auto& circles = std::get<0>(sc);
    const auto& squares = std::get<1>(sc);
    for (const auto& c : circles) sum += AreaVisitor{}(c);
    for (const auto& s : squares) sum += AreaVisitor{}(s);
    return sum;
}
```

The element type in `circles` is `const Circle&`. The element type
in `squares` is `const Square&`. The visitor's two `operator()`
overloads are picked at compile time, one per loop. Zero runtime
branching.

Other options that also work:
- `std::variant<Circle, Square>` and then `std::visit(AreaVisitor{},
  v)` — but that brings back the variant's index + jump table, and
  this lesson is supposed to be about NOT doing that.
- A fold expression: `(sum + AreaVisitor{}(vec), ...)` over a
  parameter pack of `std::vector<ShapeT>`. More compact, but adds
  another template layer to read.
- A recursive template that walks a tuple element-by-element.
  Same as above.

The tuple-of-vectors form is the simplest and most legible for two
shape types. For five or ten shape types the `std::tuple` starts
to feel awkward and a fold expression or a generated visitor list
becomes more attractive.

## The dynamic reference

To prove the CRTP numbers are correct, I included a reference
implementation that does the same thing with the classic Visitor
pattern:

```cpp
class DShape {
public:
    virtual ~DShape() = default;
    virtual void accept(DShapeVisitor& v) const = 0;
};

class DCircle : public DShape {
public:
    void accept(DShapeVisitor& v) const override { v.visit(*this); }
    // ...
};

class DShapeVisitor {
public:
    virtual void visit(const DCircle& c) = 0;
    virtual void visit(const DSquare& s) = 0;
};

struct DynamicAreaVisitor : DShapeVisitor {
    double result = 0.0;
    void visit(const DCircle& c) override { /* ... */ }
    void visit(const DSquare& s) override { /* ... */ }
};
```

`shape->accept(visitor)` does two vtable lookups: one to find
`DCircle::accept` (which picks `DynamicAreaVisitor::visit(DCircle&)`),
and the visitor-side `visit` body runs. Total: 2 indirect calls per
dispatch.

Both implementations produce the same `36.5619` for the total
area, so we know the CRTP arithmetic is right.

## Output highlights

```
== CRTP static visitor — setup ==
circles: 3, squares: 2
sizeof(Circle)        = 8
sizeof(Square)        = 8
sizeof(DCircle)       = 16 (has vptr)
sizeof(DSquare)       = 16 (has vptr)

== 1) Area — CRTP static (overloaded operator()) ==
CRTP static (overloaded) : 36.5619
CRTP static (generic+if constexpr): 36.5619
  match: YES

== 2) Bounding box — CRTP static ==
CRTP static (overloaded) : [-2.500, -2.500] -> [2.500, 2.500]
  expected            : [-2.500, -2.500] -> [2.500, 2.500]
  match: YES

== 3) JSON serialization — CRTP static ==
CRTP static:
  {"type":"circle","radius":1.0000}
  {"type":"circle","radius":0.5000}
  {"type":"circle","radius":2.5000}
  {"type":"square","side":2.0000}
  {"type":"square","side":3.0000}

== 4) Same area via Dynamic Visitor (double-dispatch) ==
Dynamic Visitor (double-dispatch): 36.5619
  match: YES

== 6) Compile-time proof: overload resolution picks Circle or Square ==
AreaVisitor(Circle(7.0)) = 153.9380
AreaVisitor(Square(7.0)) = 49.0000
Both `operator()` overloads exist and are picked at compile time.

All three implementations agree on 36.5619 for the total area.
```

The two CRTP forms (overloaded and generic) produce the same
number. The dynamic Visitor produces the same number. Everything
matches.

## The three dispatch styles — side by side

| Style            | Mechanism                                  | Per-dispatch cost      | Open/closed |
|------------------|--------------------------------------------|------------------------|-------------|
| **Dynamic Visitor** | vtable load + virtual call (×2 for double-dispatch) | 2 indirections | Open (plugins OK) |
| **`std::visit`** | variant index load + jump table lookup     | 1 indirect jump        | Closed (compile-time set) |
| **CRTP static** | compile-time overload resolution           | 0 indirections (often inlined) | Closed (compile-time set) |

The progression from Dynamic → `std::visit` → CRTP is the
progression from "I can have any type, even one I haven't seen"
to "I have a fixed set and I want zero runtime cost."

## The tradeoffs of CRTP

These are real and matter for production code:

1. **No common base type.** `Shape<Circle>*` is not `Shape<Square>*`.
   You can't put CRTP shapes in a polymorphic container. You need
   a `std::tuple`, `std::variant`, fold expression, or visitor
   walk at the top level — which is the same plumbing `std::visit`
   does, just spelled differently.

2. **Code bloat.** With N shape types and M operations, the
   compiler instantiates `N × M` template bodies. The
   `std::variant`-based approach shares one jump table across all
   three visitors; the dynamic approach shares one vtable. CRTP
   shares nothing.

3. **Adding a type means editing every visitor.** Same as
   `std::visit` — both are closed-set. But the compiler enforces it
   harder for `std::visit` (the visit-call site won't compile if
   you forget a case, unless you provide a generic `operator()`
   fallback) and softer for CRTP (the `if constexpr` chain's
   `static_assert` only fires if you reach the `else` branch).

4. **Build time.** Every CRTP-instantiated function is a
   translation-unit-local template. `std::variant` is one type;
   CRTP with M visitors over N shapes is M×N type
   instantiations. For small N this is fine; for large N it adds
   up.

5. **No virtual destructor concern.** With CRTP you can't slice
   through a base pointer anyway, so the classic
   "base-destructor-must-be-virtual" rule doesn't apply. With
   polymorphism it does.

## What I took away

- CRTP is the natural endpoint of the variant/visitor arc if your
  priority is **zero-cost dispatch** and your type set is
  **closed at compile time**.
- The dispatch is so cheap that the compiler typically inlines the
  visitor body into the call site — the resulting machine code is
  the same as if you'd written the switch by hand.
- The cost is structural: no common base, M×N code bloat, and
  closed-set invariant maintained manually (or via the
  `static_assert` pattern).
- `std::visit` is the right choice when you want closed-set
  semantics but don't want to give up the "feels like one
  container" ergonomics of polymorphism.
- Dynamic polymorphism is the right choice when the type set is
  *open* — plugin architectures, AST nodes from a language
  frontend, GUI event hierarchies.

This is the third leg of the table. The Jun 20 (Visitor) lesson
covered the open-set case; Jun 10/15/21 covered the closed-set
index-jump case; today covers the closed-set zero-indirection
case. All three produce the same answers on the same shapes; the
choice is about where the work happens (runtime vtable, runtime
jump table, compile-time overload).

## Build and run

```bash
cd late-may/cpp_practice/
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-22-crtp-static-visitor \
    P-2026-06-22-crtp-static-visitor.cpp
./P-2026-06-22-crtp-static-visitor

# ASan + UBSan
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    -o P-2026-06-22-crtp-static-visitor-asan \
    P-2026-06-22-crtp-static-visitor.cpp
./P-2026-06-22-crtp-static-visitor-asan
```

Both builds clean (no warnings under `-Wall -Wextra -Wpedantic`).
The ASan run is also clean — no leaks, no UB.

## Next Steps

The variant/visitor arc (Jun 10/15/20/21/22) is now complete on
the "concrete dispatch mechanisms" side. Remaining threads in the
PIMPL + design-patterns family from earlier sessions:

- **PIMPL ABI capstone** (carried from Jun 17/18/19) — compile two
  TUs, save one `.o`, edit the shape's `.cpp`, recompile only
  that, link, run. The unchanged `.o` is the proof that the PIMPL
  preserved ABI.
- **`shared_ptr<Impl>` PIMPL** (carried from Jun 18/19) — replace
  `unique_ptr<Impl>` with `shared_ptr<Impl>`. The copy ctor /
  assign become defaultable. Deep copy disappears; cost becomes
  refcount.
- **Acyclic visitor** — uses `dynamic_cast` *inside* the visitor's
  `visit(Base&)` to break the dependency cycle of the classic
  visitor pattern. Loses double-dispatch but gains extensibility.
- **PIMPL series with `std::variant` instead** — take
  `PimplInventory<ShapeV>` from Jun 18 and replace the
  `vector<unique_ptr<PimplShape>>` member with
  `vector<ShapeV>`. Compare the resulting class — what changes,
  what stays the same.

A new thread suggested by this session:

- **CRTP + `std::variant` hybrid** — `using ShapeV = std::variant<
  CRTP_Circle, CRTP_Square>` where the alternative types are CRTP
  shapes. Combine the zero-overhead dispatch (variant index) with
  the zero-vtable objects (CRTP storage). The interesting question
  is what happens when you put CRTP-typed objects into a variant.

The acyclic visitor is the natural next thread: it answers "what if
the type set is open AND we want to decouple visitor from
hierarchy?"

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files (under `late-may/cpp_practice/`):**
  - `P-2026-06-22-crtp-static-visitor.cpp` (main)
  - `P-2026-06-22-crtp-static-visitor.md` (this file)
- **No existing files modified.**
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*The CRTP static visitor is C++17's "I have a closed type set and
I want every dispatch to be devirtualized" answer. Polymorphism is
the open-set answer. `std::visit` is the closed-set
index-and-jump answer. CRTP is the closed-set
overload-resolved-at-compile-time answer. All three produce the
same numbers; the choice is about which constraints you can
accept.*
