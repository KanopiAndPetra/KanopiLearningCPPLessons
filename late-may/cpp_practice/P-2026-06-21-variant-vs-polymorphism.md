# C++ practice 2026-06-21 — `std::variant<Circle, Square>` vs polymorphism

## What I set out to learn

The Jun 20 session ended with a "Next Steps" item that called out a
head-to-head comparison explicitly:

> **`std::variant<Circle, Square>` head-to-head** — re-implement
> the three visitors as `std::variant<PimplCircle, PimplSquare>`
> + `std::visit`. Compare the runtime (single virtual vs single
> index-into-jump-table), the compile time (templates vs
> vtables), and the code clarity. The Jun 15 lesson covered
> `std::visit`; this would be a direct comparison.

This session is that comparison. The Jun 19 / Jun 20 PIMPL+Visitor
hierarchy (`PimplShape` base + `PimplCircle` / `PimplSquare`
deriveds + three concrete visitors) gets paired with the
`std::variant<PimplCircle, PimplSquare>` representation +
`std::visit` lambdas. Same input, same three operations (area,
bounding box, JSON), two parallel implementations.

The variant side also touches the **"visitor with return values"**
style that the Jun 20 "Next Steps" list mentioned: `std::visit`
returns a value directly from each `operator()` — no
out-parameter, no member variable for the result.

## The two representations

### Polymorphic (`vector<unique_ptr<PimplShape>>`)

From Jun 19 / Jun 20. Each shape is a heap-allocated
`PimplCircle` / `PimplSquare` whose `Impl` is also heap-allocated
inside it. The `unique_ptr<PimplShape>` is the slot in the vector;
the vptr is the dispatch handle.

Per shape: **2 heap allocations** (one for the derived object, one
for its `Impl`).

### Variant (`vector<std::variant<PimplCircle, PimplSquare>>`)

```cpp
using ShapeV = std::variant<PimplCircle, PimplSquare>;
```

A `ShapeV` is a *closed* tagged union — at compile time it can
hold exactly one of `PimplCircle` or `PimplSquare`, and no
others. The shapes live **in place** inside the variant's storage
— the variant owns the bytes directly. The discriminator is the
active index, stored in 8 bytes (on this libc++ build).

Per shape: **1 heap allocation** (the `Impl` inside the shape — the
shape itself is stack/in-vector).

`sizeof(ShapeV)` on the build: **24 bytes** (8-byte index +
16-byte aligned payload, where 16 is `max(sizeof PimplCircle,
sizeof PimplSquare)`).

## The two dispatch styles

### Polymorphic — `accept(visitor&)` via double-dispatch

```cpp
void PimplCircle::accept(ShapeVisitor& v) const override {
    v.visit(*this);   // second dispatch picks AreaVisitor::visit(Circle&)
}
```

Two vtable lookups per call: one to pick `accept` from the shape's
vtable, one to pick `visit(Derived&)` from the visitor's vtable.
With `dynamic_cast` (Jun 19 style), three indirections.

### Variant — `std::visit(visitor, v)` via index + jump table

```cpp
struct AreaVisitor {
    double operator()(const PimplCircle& c) const {
        return M_PI * c.radius() * c.radius();
    }
    double operator()(const PimplSquare& s) const {
        double side = s.side();
        return side * side;
    }
};

double total_area(const std::vector<ShapeV>& vs) {
    double sum = 0.0;
    for (const auto& v : vs) sum += std::visit(AreaVisitor{}, v);
    return sum;
}
```

`std::visit` asks the variant for its active index (one byte
read) and dispatches via a jump table. No vtable, no RTTI. In the
common case (variant over 2–3 types), this compiles to a switch on
an integer — fewer instructions per dispatch than vtable
indirection on most ISAs.

## The "visitor with return values" style

The Jun 20 `AreaVisitor`/`BoundingBoxVisitor`/`JsonVisitor`
all accumulated state inside the visitor object (a `double`
member, four `double` members, a `std::string` member) and let
the caller read it out afterwards. That's because the Visitor
pattern is fundamentally about "do something to this object" —
the result is by convention stored in the visitor itself.

`std::visit` lifts that constraint. The visitor's `operator()`
returns a value, and `std::visit` returns it directly. Compare
the variant BBoxVisitor:

```cpp
struct BBoxVisitor {
    BBox operator()(const PimplCircle& c) const {
        double r = c.radius();
        return BBox{-r, -r, r, r};
    }
    // ...
};
```

vs the polymorphic BBoxVisitor from Jun 20, which had four
private `double` members and the `visit` methods wrote into
them. The variant version is shorter and the call site is
cleaner:

```cpp
for (const auto& v : vs) {
    BBox b = std::visit(BBoxVisitor{}, v);  // direct return
    // ... fold into the union
}
```

This is one of the underrated wins of `std::visit` over the
Visitor pattern: the visitor's `operator()` is a function, and
functions return values.

## What the program actually does

The `main` builds 5 shapes (3 circles + 2 squares, alternating)
twice: once as `unique_ptr<PimplShape>` (polymorphic), once as
`std::variant<PimplCircle, PimplSquare>` (variant). Then it
runs three operations on each:

1. **Area** — sum of all shape areas.
2. **Bounding box** — the union of all shape bounding boxes.
3. **JSON** — serialize each shape to a JSON string.

For each operation, both representations produce the same
result, and the program asserts the match.

Then a comparison section walks through:
- The dispatch mechanism in each case.
- The size / storage of each representation.
- The extensibility trade-off.
- `std::get_if` and `std::holds_alternative` as the variant-side
  analogues of `dynamic_cast`.
- A summary table.

## Output highlights

```
== 1) Area — polymorphic Visitor vs std::visit ==
polymorphic (dynamic_cast + dispatch): 36.5619
variant      (std::visit + dispatch) : 36.5619
  match: YES

== 2) Bounding box — union over the collection ==
polymorphic (dynamic_cast chain): [-2.500, -2.500] -> [2.500, 2.500]
variant      (std::visit)      : [-2.500, -2.500] -> [2.500, 2.500]
  match: YES

== 3) JSON serialization ==
  polymorphic:
    {"type":"circle","radius":1.0000}
    {"type":"square","side":2.0000}
    {"type":"circle","radius":0.5000}
    {"type":"square","side":3.0000}
    {"type":"circle","radius":2.5000}
  variant:
    {"type":"circle","radius":1.0000}
    {"type":"square","side":2.0000}
    {"type":"circle","radius":0.5000}
    {"type":"square","side":3.0000}
    {"type":"circle","radius":2.5000}
  match: YES
```

The numbers match (as they must — they're computed from the same
shapes), and the JSON strings are byte-identical.

## The size breakdown

```
sizeof(PimplShape)        = 8   (vptr) + 8 (Impl*) = 16
sizeof(PimplCircle)       = 16  (vptr + Impl* — base members + own Impl*)
sizeof(PimplSquare)       = 16  (same)
sizeof(ShapeV)            = 24  (8-byte index + 16-byte payload)
sizeof(unique_ptr<Shape>) = 8
```

So one slot in the polymorphic vector is 8 bytes (a pointer); one
slot in the variant vector is 24 bytes (the union in place). But
the polymorphic slot *points to* a 16-byte heap object, which
*contains a pointer* to another heap object (`Impl`). Total per
shape in the polymorphic container: 16 (heap) + 16 (Impl heap) =
~32 bytes across two allocations. Total per shape in the variant
container: 24 (vector slot) + 16 (Impl heap) = ~40 bytes across
one allocation, but the 24 bytes is contiguous with the vector
itself.

For 5 shapes the difference is negligible; for 5 million shapes
the difference matters (cache locality, allocation count).

## The extensibility trade-off

The classic object-orientation pitch:

> Open for extension, closed for modification.

Both representations are open in the same direction (new
operations) and closed in the same direction (new types).

- **New operation (e.g. perimeter):**
  - polymorphism: write a new `PerimeterVisitor : ShapeVisitor`.
  - variant:      write a new `PerimeterVisitor` struct with
    two `operator()` overloads.
  - Cost: comparable.

- **New type (e.g. Triangle):**
  - polymorphism: write `PimplTriangle : PimplShape`, then add
    `visit(Triangle&)` to every existing visitor.
  - variant:      change the alias to
    `using ShapeV = std::variant<PimplCircle, PimplSquare,
    PimplTriangle>`. The compiler will tell you which existing
    visitors need a new overload.
  - Cost: comparable — both require editing every visitor.

**The real difference** is in the *philosophy*:

- **Polymorphism is open to extension.** A plugin / shared
  library can hand you a `PimplShape*` derived from a class you
  never saw. Your code that calls `accept(visitor)` on it works.
  Your visitor's `visit(Base&)` fallback (if you have one) gets
  the call. The closed-set invariant is enforced at runtime by
  RTTI, not at compile time.

- **Variant is closed by design.** Every consumer of `ShapeV`
  must include the variant header. When you add a type, every
  consumer recompiles. There's no plugin model; the type set is
  fixed in source.

This maps to two real-world patterns:

- **Polymorphism:** GUI frameworks, GUI events, plugin systems,
  AST nodes in compilers (where new node types come from new
  language features), device drivers.
- **Variant:** Expression types in a language interpreter
  (always Int / Float / Bool / String / Closure), HTTP request
  types (always GET / POST / PUT / DELETE), a fixed set of
  message types in a protocol.

## `std::get_if` and `std::holds_alternative`

Two direct variant-side analogues of `dynamic_cast`:

```cpp
if (auto* c = std::get_if<PimplCircle>(&var[0])) {
    std::printf("radius = %.4f\n", c->radius());
}

if (std::holds_alternative<PimplCircle>(var[0])) { /* ... */ }
```

`holds_alternative` is one int compare against the variant's
index — strictly cheaper than `dynamic_cast`, which walks the
vtable and RTTI.

For "I know the type at the call site and want to extract it",
`get_if` is the idiomatic variant equivalent. For "I want to do
type-specific work for each alternative", `std::visit` is the
idiomatic equivalent.

## What I took away

- `std::variant<T1, T2, ...>` is C++17's answer to "I have a
  closed set of alternatives and I want compile-time checking of
  exhaustiveness."
- `std::visit(visitor, v)` dispatches via index + jump table,
  not vtable + RTTI. Cheaper, but only because the type set is
  closed.
- The Visitor pattern (`accept`/`visit`) is the polymorphic
  equivalent. Open for extension (new types in plugins), but no
  compile-time exhaustiveness check.
- Both can do "area", "bounding box", "JSON". Same code length,
  similar complexity. The choice is about *open vs closed type
  set*, not about which is more concise.
- The "visitor with return values" pattern that Jun 20 hinted at
  is exactly what `std::visit` does natively. The polymorphic
  Visitor pattern can mimic it with a struct of `operator()`
  methods that return, but the canonical polymorphic visitor
  accumulates state in the visitor object instead.

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-21-variant-vs-polymorphism \
    P-2026-06-21-variant-vs-polymorphism.cpp \
    pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp
./P-2026-06-21-variant-vs-polymorphism

# ASan + UBSan
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    -o P-2026-06-21-variant-vs-polymorphism-asan \
    P-2026-06-21-variant-vs-polymorphism.cpp \
    pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp
./P-2026-06-21-variant-vs-polymorphism-asan
```

Both builds clean (no warnings under `-Wall -Wextra
-Wpedantic`). The ASan run is also clean — no leaks, no UB.

## Next Steps

The Jun 9 "Next Steps" list is now fully worked through:

- `std::variant` and `std::visit` — Jun 10 (basic) and Jun 15
  (stateful visitors) and **Jun 21 (vs polymorphism, this
  session)**.
- `enable_shared_from_this` — Jun 13.
- `std::move_if_noexcept` — Jun 12.
- Multi-file Inventory — Jun 16.
- `std::span` (C++17 by hand) — Jun 14.

So the original chain is done. New threads to pick up:

- **PIMPL ABI capstone** (carried from Jun 17/18/19) — compile two
  TUs, save one `.o`, edit the shape's `.cpp`, recompile only
  that, link, run. The unchanged `.o` is the proof that the
  PIMPL preserved ABI.
- **`shared_ptr<Impl>` PIMPL** (carried from Jun 18/19) —
  replace `unique_ptr<Impl>` with `shared_ptr<Impl>`. The copy
  ctor / assign become defaultable. Deep copy disappears; cost
  becomes refcount.
- **Acyclic visitor** — uses `dynamic_cast` *inside* the
  visitor's `visit(Base&)` to break the dependency cycle of the
  classic visitor pattern. Loses double-dispatch but gains
  extensibility.
- **CRTP static visitor** — compile-time visitor using
  templates + `if constexpr` instead of vtables. The zero-cost
  version of `std::visit`.
- **The PIMPL series written with `std::variant` instead** —
  take `PimplInventory<ShapeV>` from Jun 18 and replace the
  `vector<unique_ptr<PimplShape>>` member with
  `vector<ShapeV>`. Compare the resulting class — what changes,
  what stays the same, what gets simpler, what gets harder.

The CRTP static visitor is the natural next step after this
session — it answers "what if I want both open-set and
zero-cost?".

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files (under `late-may/cpp_practice/`):**
  - `P-2026-06-21-variant-vs-polymorphism.cpp` (main)
  - `P-2026-06-21-variant-vs-polymorphism.md` (this file)
- **No existing files modified.**
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*`std::variant` + `std::visit` is C++17's idiomatic answer to
"small closed set of alternatives with per-type operations." The
Visitor pattern is C++'s answer to the same problem when the
type set is open. Both work; the choice is about the type set's
shape (closed vs open), not about which is more concise or
faster. Variant is the lower-overhead choice when the set is
closed and known at compile time; polymorphism is the
extensible choice when new types must arrive from outside the
current translation unit. This session makes that trade-off
explicit and runs both side by side on the same shapes and the
same three operations.*
