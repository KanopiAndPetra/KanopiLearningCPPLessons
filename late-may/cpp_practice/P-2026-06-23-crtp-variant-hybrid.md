# C++ practice 2026-06-23 — CRTP + std::variant hybrid

## What I set out to learn

The Jun 22 session's "Next Steps" list closed with one item that
naturally combines what I just learned with what came before:

> **CRTP + `std::variant` hybrid** —
> `using ShapeV = std::variant<CRTP_Circle, CRTP_Square>` where the
> alternative types are CRTP shapes. Combine the zero-overhead
> dispatch (variant index) with the zero-vtable objects (CRTP
> storage). The interesting question is what happens when you
> put CRTP-typed objects into a variant.

This is that session. The arc has been:

| Jun   | Representation           | Where dispatch lives       | Per-shape obj |
|-------|--------------------------|----------------------------|---------------|
| 19/20 | `unique_ptr<DShape>`     | vtable + virtual call      | 16 bytes      |
| 10/15/21 | `std::variant<DCircle, DSquare>` | variant index + jump table | 16 bytes  |
| 22    | `tuple<vec<Circle>,vec<Square>>` | compile-time overload | 8 bytes   |
| **23 (today)** | **`std::variant<Circle, Square>`** | **variant index + jump table** | **8 bytes (CRTP)** |

Today's row takes the variant machinery (Jun 10/15/21) but with
CRTP alternatives (Jun 22), so the alternatives themselves are 8
bytes instead of 16. The variant doesn't *care* about CRTP — it
just stores data — but CRTP gives us the smaller alternative size.

## What I confirmed

### (1) The variant does not care that the alternatives are CRTP

```cpp
class Circle : public Shape<Circle> {
    double r_;   // 8 bytes total
};
class Square : public Shape<Square> {
    double s_;   // 8 bytes total
};
using ShapeV = std::variant<Circle, Square>;
```

The output shows:

```
sizeof(Circle)         = 8  (no vptr, just r_)
sizeof(Square)         = 8  (no vptr, just s_)
sizeof(DCircle)        = 16  (8 vptr + 8 radius)
sizeof(DSquare)        = 16  (8 vptr + 8 side)
sizeof(ShapeV)         = 16  (variant<Circle,Square>)
sizeof(DShapeV)        = 24  (variant<DCircle,DSquare>)
sizeof(PlainShapeV)    = 16  (variant<PlainCircle,PlainSquare>)
```

Two things stand out:

1. **`sizeof(ShapeV) == sizeof(PlainShapeV) == 16`.** The variant
   doesn't shrink because its alternatives are CRTP — it has the
   same size as a variant with plain (non-CRTP, non-polymorphic)
   alternatives. The variant stores the *data* of whichever
   alternative is active plus a small index. The CRTP base is empty
   (no data members), so adding it to Circle costs nothing.

2. **`sizeof(DShapeV) == 24`** — *bigger* than the CRTP variant
   because the polymorphic alternatives each carry an 8-byte
   vtable pointer. So `variant<DCircle,DSquare>` is `8 + 8 + 8`
   bytes of data + index, while `variant<Circle,Square>` is
   `8 + 8` bytes + index.

The CRTP bonus is real but it's at the *alternative* level, not
the variant level.

### (2) The CRTP base is still useful in visitor overloads

The canonical std::visit pattern is to write one `operator()`
per concrete alternative:

```cpp
struct AreaVisitor {
    double operator()(const Circle& c) const { ... }
    double operator()(const Square& s) const { ... }
};
```

You can also write the overloads to take the CRTP base reference
— and inside the body, the static type system knows the derived
type because `Shape<Derived>` encodes it:

```cpp
struct ShapeBaseAreaVisitorReal {
    double operator()(const Shape<Circle>& s) const {
        constexpr double pi = 3.14159265358979323846;
        // s.self() returns const Circle& (compile-time recovery)
        double r = s.self().radius();
        return pi * r * r;
    }
    double operator()(const Shape<Square>& s) const {
        double side = s.self().side();
        return side * side;
    }
};
```

Both visitors produce the same total area: `36.5619`. The CRTP
form is more verbose here, but it's the bridge to richer CRTP
machinery — if `Shape<D>` had more methods, taking `Shape<D>&`
gives the visitor access to them all without going through
`self()` explicitly.

### (3) You can't unify the visitor into one overload

I tried writing a single template overload:

```cpp
template <typename D>
double operator()(const Shape<D>& s) const { ... }
```

…and confirmed (with a commented-out test) that **this does not
work**. `std::visit` dispatches on the variant's *concrete*
alternative types (`Circle` and `Square`), not on a family of
`Shape<D>`. The visitor overload set must match the alternatives
exactly. You can dress the parameter up as `Shape<Derived>&` (one
overload per alternative) but you can't make a single template
overload match all alternatives via the CRTP base.

That's the closed-set invariant of `std::visit` (Jun 10/15): the
compiler enforces exhaustiveness at the visit-call site. For
two alternatives, you write two overloads — full stop.

### (4) All four representations agree on the same numbers

The head-to-head on identical data (`{1.0, 2.0}, {0.5, 3.0},
{2.5, 0.0}`):

```
(a) Polymorphic (mock) : 36.5619
(b) Plain variant      : 36.5619
(c) CRTP variant       : 36.5619
(d) CRTP tuple-of-vecs : 36.5619
All four agree: YES
```

The full table from the output:

```
| Style                   | Dispatch lives in...     | Per-shape obj |
|-------------------------|--------------------------|---------------|
| unique_ptr<DShape>      | vtable (runtime)         | 16 bytes      |
| variant<DCircle,DSquare>| variant index + jump tbl | 16 bytes      |
| variant<Circle,Square>  | variant index + jump tbl |  8 bytes (CRTP)|
| tuple<vec<Circle>,..>   | compile-time overload    |  8 bytes (CRTP)|
```

This is the design space laid out: at each row you give up
something (runtime openness, jump-table indirection, generic
container) in exchange for something smaller (no vtable, fewer
indirections, more inlined code). Today's row is the "closed set
*and* compact alternatives" row.

## A subtle thing: variant of polymorphic vs CRTP alternatives

I expected `variant<Circle, Square>` and `variant<DCircle,
DSquare>` to have the same size on a 64-bit system, because both
alternatives are 8 bytes of data and the index rounds up to one
word. The output shows otherwise:

```
sizeof(ShapeV)  = 16  (variant<Circle,Square>)
sizeof(DShapeV) = 24  (variant<DCircle,DSquare>)
```

Why? Because `DCircle` is 16 bytes (8 vptr + 8 radius), so the
variant needs 16 bytes of data storage + an index byte + padding
to round up to 24. With CRTP alternatives, both alternatives are
8 bytes, so the data is 8 bytes + index + padding = 16.

In other words, **CRTP variants are smaller than polymorphic
variants** because the alternatives themselves are smaller. The
index tag and padding overhead is constant; the data storage
scales with the alternatives.

This is a real, practical difference. If you have a vector of
1000 variant shapes, CRTP saves you `(16 - 8) * 1000 = 8000` bytes
of storage vs polymorphic alternatives — in *addition* to removing
the indirection cost.

## What I took away

- **`std::variant` is type-agnostic about its alternatives.** It
  doesn't care if they're plain structs, polymorphic, or CRTP. It
  stores `max(sizeof(alternative)) + index` and dispatches via a
  jump table.
- **CRTP shrinks the alternatives**, which makes the variant
  smaller — measurably so on 64-bit systems (16 vs 24 bytes for
  two 8-byte-data alternatives).
- **The CRTP base can appear in visitor overloads**, but the
  overload set must still match each concrete alternative. CRTP
  doesn't let you collapse N overloads into 1.
- **The dispatch cost of `variant<Circle, Square>` is the same as
  `variant<PlainCircle, PlainSquare>`** — the variant machinery
  doesn't change. CRTP doesn't speed up `std::visit`; it shrinks
  what the variant is *visiting over*.
- **The "give up X to gain Y" table is now four rows wide.** Each
  row is a coherent design choice; mixing two rows (e.g. "I want
  polymorphic alternatives with CRTP dispatch") doesn't compose
  well — the design axes are orthogonal.

This is the natural capstone for the variant/visitor arc
(Jun 10/15/20/21/22/23). Next steps in adjacent threads from the
Jun 22 list:

- **PIMPL ABI capstone** — compile two TUs, save one `.o`, edit
  the shape's `.cpp`, recompile only that, link, run. The
  unchanged `.o` is the proof that the PIMPL preserved ABI.
- **`shared_ptr<Impl>` PIMPL** — replace `unique_ptr<Impl>` with
  `shared_ptr<Impl>`. The copy ctor / assign become defaultable.
  Deep copy disappears; cost becomes refcount.
- **Acyclic visitor** — uses `dynamic_cast` *inside* the
  visitor's `visit(Base&)` to break the dependency cycle of the
  classic visitor pattern. Loses double-dispatch but gains
  extensibility.

## Build and run

```bash
cd late-may/cpp_practice/
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-23-crtp-variant-hybrid \
    P-2026-06-23-crtp-variant-hybrid.cpp
./P-2026-06-23-crtp-variant-hybrid

# ASan + UBSan
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    -o P-2026-06-23-crtp-variant-hybrid-asan \
    P-2026-06-23-crtp-variant-hybrid.cpp
./P-2026-06-23-crtp-variant-hybrid-asan
```

Both builds clean (no warnings under `-Wall -Wextra -Wpedantic`).
The ASan run is also clean — no leaks, no UB.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files (under `late-may/cpp_practice/`):**
  - `P-2026-06-23-crtp-variant-hybrid.cpp`
  - `P-2026-06-23-crtp-variant-hybrid.md` (this file)
- **No existing files modified.**
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*Today closes the variant/visitor arc by asking the obvious
follow-on question: what if the variant's alternatives are the
CRTP shapes I built yesterday? Answer: nothing breaks, the
variant is indifferent to the alternatives' ancestry, and the
storage shrinks by 8 bytes per shape because the vtable pointer
is gone. The CRTP base can still appear in visitor overloads —
you get `self()` machinery inside each overload — but you still
need one overload per concrete alternative. The closed-set
invariant of `std::visit` is preserved; CRTP gives you more
inside each overload, not fewer overloads.*