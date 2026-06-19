// P-2026-06-19-pimpl-virtual-functions.cpp
//
// Lesson: Polymorphic PIMPL — the vptr lives in the facade, not the Impl.
//
// Build: g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -o P-2026-06-19-pimpl-virtual-functions \
//            P-2026-06-19-pimpl-virtual-functions.cpp \
//            pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp
//
// Run:   ./P-2026-06-19-pimpl-virtual-functions
//
// Companion files: pimpl_shape.{h,cpp}, pimpl_circle.{h,cpp}, pimpl_square.{h,cpp}.
//
// What this program demonstrates
// ==============================
// 1. The PIMPL + polymorphism pattern: an abstract base (PimplShape) with
//    virtual dtor and pure-virtual print() and clone(); two concrete
//    deriveds (PimplCircle, PimplSquare) that PIMPL their state into a
//    forward-declared Impl. The base has *no* data; the deriveds each
//    own their own unique_ptr<Impl>.
//
// 2. The vtable dispatch: a std::vector<std::unique_ptr<PimplShape>>
//    holding a mix of circles and squares, and a single for-loop that
//    calls shape->print() on each, dispatching to the right derived
//    at runtime.
//
// 3. The vptr's location: sizeof(PimplShape) == 8 on a 64-bit system
//    (just a vptr — no data), sizeof(PimplCircle) == 16 (a vptr + a
//    unique_ptr<Impl>), and sizeof(PimplSquare) is the same. The
//    *derived* class's vptr is at the *base* offset, so the layout is
//    [vptr][impl_]. That's the whole point: polymorphism lives in the
//    facade, and the data lives in the Impl. The two are orthogonal.
//
// 4. The virtual copy constructor: cloning a polymorphic object via
//    shape->clone() returns a *new* derived-typed object owned via a
//    base pointer. This is the standard way to copy a polymorphic
//    object without slicing.
//
// 5. The PIMPL sizes don't change with virtuals: PimplCircle and
//    PimplSquare each carry one vptr and one unique_ptr<Impl>, and
//    the Impl size is independent of how many virtuals the base has.

#include <cstddef>     // std::size_t
#include <iostream>
#include <memory>      // std::unique_ptr, std::make_unique
#include <string>      // std::string
#include <utility>     // std::move
#include <vector>      // std::vector

#include "pimpl_circle.h"
#include "pimpl_shape.h"
#include "pimpl_square.h"

// ---------------------------------------------------------------------------
// Section 1: Layout and sizes -- where does the vptr live?
// ---------------------------------------------------------------------------
//
// On a typical 64-bit system:
//   sizeof(void*)              == 8
//   sizeof(PimplShape)         == 8   (just a vptr; the base has no data)
//   sizeof(PimplCircle)        == 16  (vptr + unique_ptr<Impl>)
//   sizeof(PimplSquare)        == 16  (vptr + unique_ptr<Impl>)
//
// The vptr is *inside* the PimplCircle / PimplSquare object, at the
// base-class offset. Adding a new virtual to PimplShape increases
// sizeof(PimplShape) by 0 (the vtable grows, but the vptr is one pointer
// regardless) and does NOT increase sizeof(PimplCircle) -- the derived
// class's vptr is the same pointer that resolves the *base* vtable too.
//
// The key observation: the *derived*'s sizeof is not the sum of the
// base's sizeof + the derived's data. It's base's vptr (8 bytes) +
// derived's unique_ptr<Impl> (8 bytes) + Impl's data (anywhere; the
// Impl lives on the heap). The Impl's size is independent of the
// vtable.
static void section1_layout_and_sizes() {
    std::cout << "\n--- Section 1: Layout and sizes ---\n";

    std::cout << "  sizeof(void*)              = " << sizeof(void*) << "\n";
    std::cout << "  sizeof(PimplShape)         = " << sizeof(PimplShape)
              << "   (just a vptr; the base has no data)\n";
    std::cout << "  sizeof(PimplCircle)        = " << sizeof(PimplCircle)
              << "   (vptr + unique_ptr<Impl>)\n";
    std::cout << "  sizeof(PimplSquare)        = " << sizeof(PimplSquare)
              << "   (vptr + unique_ptr<Impl>)\n";

    // The base PimplShape is *abstract*: you cannot construct one
    // directly. The compiler enforces this because print() and clone()
    // are pure virtual. (PimplShape's default ctor exists, but it's
    // protected, and the pure-virtuals still make the class
    // non-instantiable.)
    //
    // PimplShape s;       // ERROR: variable of abstract type
    // std::cout << sizeof(s) << "\n";
    //
    // We can, however, take sizeof and the address of a static
    // function. We can also hold pointers and references to a
    // PimplShape -- that's the whole point of having a polymorphic
    // base.

    // The derived classes are concrete. Make one of each and observe
    // the sizes via a pointer (which is what a polymorphic container
    // holds).
    PimplCircle c1(1, 2.0);
    PimplSquare s1(2, 3.0);

    std::cout << "  &c1 (PimplCircle*)         = " << static_cast<void*>(&c1)
              << "   -- this is the vptr address\n";
    std::cout << "  &c1.impl_ (via &impl_)    = (private)\n";
    std::cout << "  &s1 (PimplSquare*)         = " << static_cast<void*>(&s1) << "\n";

    // Why is the vptr address the same as the object's address?
    // Because in the Itanium ABI (used by gcc and clang on Linux/macOS),
    // the vptr is the FIRST data member of the object. The derived
    // class's data members come *after* the vptr. So `&c1` is the
    // vptr's address.
    //
    // On the MSVC ABI, the vptr is at the *end* of the object. The
    // program compiles either way (the compiler is allowed to choose),
    // but the addresses differ. The lesson's point -- "vptr is in the
    // facade, not the Impl" -- is true on both ABIs.

    // The takeaway: the vptr is the *facade*'s data. It costs 8 bytes
    // (one pointer) per object, regardless of how many virtuals the
    // base has. The cost is fixed; the benefit is dynamic dispatch.
}

// ---------------------------------------------------------------------------
// Section 2: Polymorphic container -- one loop, two kinds
// ---------------------------------------------------------------------------
//
// The "killer app" of polymorphism: a single container that holds
// heterogeneous types, accessed through a common interface. The
// container is `std::vector<std::unique_ptr<PimplShape>>`. Each
// element is a base-class pointer; the actual object is a circle or
// a square. The loop calls print() on each, and the vtable picks
// the right derived.
static void section2_polymorphic_container() {
    std::cout << "\n--- Section 2: Polymorphic container ---\n";

    std::vector<std::unique_ptr<PimplShape>> shapes;

    // Push a circle and a square. The make_unique<PimplCircle>(...)
    // returns unique_ptr<PimplCircle>, which is implicitly convertible
    // to unique_ptr<PimplShape> (the converting ctor is enabled for
    // derived-to-base).
    shapes.push_back(std::make_unique<PimplCircle>(101, 1.5));
    shapes.push_back(std::make_unique<PimplSquare>(102, 2.0));
    shapes.push_back(std::make_unique<PimplCircle>(103, 3.0));
    shapes.push_back(std::make_unique<PimplSquare>(104, 4.0));

    std::cout << "  shapes.size() = " << shapes.size() << "\n";

    // The loop. Each iteration calls print() through a
    // PimplShape& (because that's what operator[] returns on a
    // vector<unique_ptr<T>>: it returns unique_ptr<T>&, and the
    // call site is `*shapes[i].print(...)` or `shapes[i]->print(...)`).
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        const PimplShape& s = *shapes[i];
        std::cout << "  shapes[" << i << "]:\n";
        s.print("from container");
        std::cout << "    name() = " << s.name() << "\n";
        std::cout << "    kind() = "
                  << static_cast<int>(s.kind()) << "\n";
    }

    // Note: there is NO dynamic_cast in this loop. The whole point
    // of the polymorphic interface (print, name, kind) is to avoid
    // dynamic_cast. The base exposes everything a generic caller
    // needs; specific accessors (radius(), side()) require a
    // derived reference and are called *outside* this loop.

    // Specific accessors -- this is the cost of polymorphism. To
    // call radius() on a shape, you need a PimplCircle& or a
    // PimplCircle*. The way to get one is dynamic_cast (or the
    // visitor pattern, which is the Jun 15 lesson's territory).
    std::cout << "  specific accessors (via dynamic_cast):\n";
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        if (auto* c = dynamic_cast<PimplCircle*>(shapes[i].get())) {
            std::cout << "    shapes[" << i << "] is a circle, radius="
                      << c->radius() << "\n";
        } else if (auto* sq = dynamic_cast<PimplSquare*>(shapes[i].get())) {
            std::cout << "    shapes[" << i << "] is a square, side="
                      << sq->side() << "\n";
        } else {
            std::cout << "    shapes[" << i << "] is unknown\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Section 3: Virtual copy constructor -- clone(), no slicing
// ---------------------------------------------------------------------------
//
// The classic polymorphism trap: `PimplShape copy = *shape_ptr;` would
// *slice* -- only the base part would be copied, and the derived Impl
// (and the derived data: radius, side) would be lost. The fix is
// `shape_ptr->clone()`, which returns a heap-allocated *derived* type
// via a base pointer.
//
// This section demonstrates the round trip: make a circle, clone it,
// modify the original, and show that the clone is independent.
static void section3_virtual_copy_constructor() {
    std::cout << "\n--- Section 3: Virtual copy constructor ---\n";

    auto original = std::make_unique<PimplCircle>(201, 5.0);
    std::cout << "  original:\n";
    original->print("original");

    // clone() returns a unique_ptr<PimplShape> that actually points
    // at a PimplCircle. The pointer is upcast'd automatically.
    std::unique_ptr<PimplShape> copy = original->clone();
    std::cout << "  copy (after clone()):\n";
    copy->print("copy");

    // The copy is a *new* PimplCircle on the heap, with its own
    // unique_ptr<Impl> pointing at its own Impl. The two are
    // independent.

    // We can downcast back to a PimplCircle* to access the
    // specific accessor (radius). The dynamic_cast checks that
    // the dynamic type is actually a PimplCircle; if not, it
    // returns nullptr.
    if (auto* c = dynamic_cast<PimplCircle*>(copy.get())) {
        std::cout << "  copy is a PimplCircle, radius=" << c->radius() << "\n";
    } else {
        std::cout << "  copy is NOT a PimplCircle (unexpected)\n";
    }

    // Slicing -- this is what we *don't* do. The line below would
    // try to construct a PimplShape by copying the *base* part of
    // the PimplCircle. The compiler forbids it because PimplShape
    // is abstract (has pure virtuals). So slicing is impossible
    // here, which is one of the upsides of an abstract base.
    //
    //   PimplShape sliced = *original;   // ERROR: abstract class
    //
    // (For a non-abstract base, slicing would silently work and
    // lose the derived part. The lesson's design -- abstract base
    // -- makes slicing a compile error, which is the strongest
    // possible defense.)

    // The kind() / name() accessors work through the base pointer.
    std::cout << "  copy->name() = " << copy->name() << "\n";
    std::cout << "  copy->kind() = " << static_cast<int>(copy->kind())
              << "\n";
}

// ---------------------------------------------------------------------------
// Section 4: Big Five for a derived PIMPL class
// ---------------------------------------------------------------------------
//
// The PIMPL + polymorphism pattern has the same big-five shape as
// the non-polymorphic PIMPL: declared ctor, virtual dtor declared
// in header and defined in .cpp, move ops = default, copy ops
// declared in header and defined in .cpp. The only difference from
// Jun 17/18 is the `override` keyword on the dtor.
//
// This section exercises each operation: construct, move-construct,
// move-assign, copy-construct, copy-assign, and destroy. The trace
// shows the ctor / dtor count for each.
static void section4_big_five() {
    std::cout << "\n--- Section 4: Big Five ---\n";

    std::cout << "  -- construct --\n";
    PimplCircle c(301, 1.0);
    c.print("after construct");

    std::cout << "  -- move-construct --\n";
    PimplCircle c2 = std::move(c);
    // c.impl_ is now null (moved-from state); c2.impl_ owns the data.
    c2.print("c2 (after move)");
    // Calling c.print() would lazy-init or crash; we don't do it.
    // In the Jun 17/18 lessons, we made the moved-from state safe
    // (lazy-init in mutating methods, defensive read accessors).
    // The polymorphic version inherits the same convention from
    // unique_ptr -- calling on moved-from is the user's bug.

    std::cout << "  -- move-assign --\n";
    PimplCircle c3(303, 9.0);
    c3 = std::move(c2);
    c3.print("c3 (after move-assign)");

    std::cout << "  -- copy-construct (deep) --\n";
    PimplCircle c4 = c3;  // deep copy; c4 is a new PimplCircle
    c4.print("c4 (after copy-construct)");
    c3.print("c3 (unchanged after c4 = c3)");

    std::cout << "  -- copy-assign (deep) --\n";
    PimplCircle c5(305, 999.0);
    c5 = c4;
    c5.print("c5 (after copy-assign from c4)");

    std::cout << "  -- destroy in reverse order (c5, c4, c3) --\n";
    // c5.~PimplCircle() runs first (last constructed, first destroyed).
    // Each destruction deletes the unique_ptr<Impl>, which deletes
    // the Impl, which has no explicit dtor (the members are POD-like
    // or trivially destructible), so it's a no-op.
}

// ---------------------------------------------------------------------------
// Section 5: Virtual dispatch and a `Shape&` parameter
// ---------------------------------------------------------------------------
//
// A function that takes a `PimplShape&` (or `const PimplShape&`)
// is the *generic* way to operate on a shape. The function doesn't
// know the dynamic type, only the interface. This is what the
// std::vector<unique_ptr<PimplShape>>::operator[] gives you, and
// it's the design that lets the same code work for circles,
// squares, and (in a real program) triangles, hexagons, etc.
static void print_shape(const PimplShape& s, const std::string& tag) {
    std::cout << "  print_shape(" << tag << "): ";
    s.print(tag);
}

static void section5_pass_by_base_ref() {
    std::cout << "\n--- Section 5: Pass by base reference ---\n";

    PimplCircle c(401, 7.0);
    PimplSquare s(402, 8.0);

    // Same function, two different dynamic types. The vtable does
    // the dispatch.
    print_shape(c, "circle arg");
    print_shape(s, "square arg");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "P-2026-06-19: PIMPL and virtual functions\n";

    section1_layout_and_sizes();
    section2_polymorphic_container();
    section3_virtual_copy_constructor();
    section4_big_five();
    section5_pass_by_base_ref();

    std::cout << "\n--- Done ---\n";
    return 0;
}
