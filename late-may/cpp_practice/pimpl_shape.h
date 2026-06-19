// pimpl_shape.h -- Abstract PIMPL base class (PimplShape) declaration
//
// This is the header for the Jun 19 lesson: a polymorphic PIMPL class.
// The shape is the same as Jun 17/18's PIMPL class -- forward-declared
// Impl, unique_ptr<Impl>, dtor and copy ops declared in the header
// and defined in the .cpp, move ops = default in the header -- with
// one new twist: this class is *polymorphic*. It has a virtual dtor
// and pure virtual print() and clone() methods, plus a non-pure
// virtual name().
//
// Why virtual?
//   - Jun 17 and Jun 18 PIMPL classes were *concrete*: you'd never
//     derive from PimplBox or PimplInventory. Each object was its own
//     thing. No virtual table, no runtime dispatch.
//   - This lesson adds polymorphism: PimplShape is an *abstract base* --
//     print() is pure virtual, the dtor is virtual, and a client holds
//     a `PimplShape&` (or a `std::unique_ptr<PimplShape>`) and calls
//     print() through the base reference. The runtime picks the
//     derived class's print() based on the vptr.
//   - The interesting question -- which the program answers -- is
//     "where does the vptr live?" The answer: it lives in the
//     *PimplShape object* (in the same 8 bytes as the unique_ptr<Impl>
//     in the derived class), NOT in the Impl. Adding more virtuals to
//     PimplShape increases sizeof(PimplShape) by 0 (vptr already paid
//     for, in derived) but does NOT increase sizeof(Impl).
//
// What is the "polymorphism lives in the facade" design choice?
//   - The *base* class PimplShape is abstract. It has no data of its
//     own. The Impl is owned by the *derived* class, not the base.
//   - Why? Because if the base owned an Impl, the base's `impl_`
//     would shadow (or be shadowed by) the derived's `impl_` in the
//     same object, leading to a tangle of two PIMPL pointers in one
//     class. Putting the Impl in the *derived* class means there is
//     exactly one impl_ per object, and the base is just a thin
//     polymorphic interface with a vptr.
//   - The base *does* forward-declare Impl -- but only so it can name
//     the derived Impl types in helper signatures (e.g. clone()'s
//     return type is unique_ptr<PimplShape>, not unique_ptr<Impl>).
//   - The pattern is: "base is a pure virtual interface, derived
//     is the PIMPL class." The PIMPL boilerplate is identical to
//     Jun 17/18, but it lives in the derived class.
//
// What does the header need to know about the derived class?
//   - Nothing except a forward declaration. The header is for the
//     *base*. Separate headers pimpl_circle.h and pimpl_square.h
//     declare the deriveds.
//   - A client TU that wants to hold a `std::vector<std::unique_ptr
//     <PimplShape>>` and call print() polymorphically only needs
//     to include pimpl_shape.h. The .cpp files for circle/square
//     (where the Impls are complete) are linked in.
#pragma once

#include <cstddef>   // std::size_t
#include <memory>    // std::unique_ptr
#include <string>    // std::string (for the tag argument in print())

// Type-tag for the shape, so the program can identify kinds without
// dynamic_cast. Each derived class populates its Impl with the
// appropriate kind, and the base exposes it via a virtual kind().
//
// (The interesting part: the virtual kind() does NOT need to dispatch
// in this lesson's concrete deriveds -- they could just be a
// switch on `static_cast<ShapeKind>(...)` because the derived
// knows its own kind at compile time. But making it virtual is
// the more honest "polymorphic accessor" design and exercises the
// vtable layout.)
enum class ShapeKind { kShape, kCircle, kSquare };

class PimplShape {
public:
    // -- Virtual dtor --
    // The dtor is virtual so `delete pimpl_shape_ptr` runs the
    // *derived* class's dtor. This is the whole point of having
    // a virtual dtor on a base class.
    //
    // Why defined (= default) in the .cpp, not the header?
    //   - The derived class's dtor is what actually destroys the
    //     impl_. The base's dtor is what runs the vtable-cleanup
    //     part (the compiler-generated "call derived dtor" stub).
    //   - In our design the base has no data to destroy, so the
    //     base's dtor body is empty. Defining it `= default` in
    //     the .cpp keeps the convention from Jun 17/18: the dtor
    //     is declared in the header (so the compiler knows it
    //     exists) and defined in the .cpp (where the layout is
    //     visible, so the destructor can do its job correctly).
    virtual ~PimplShape();

    // -- Big Five --
    // No copy / move ops on the base. A polymorphic base is
    // *abstract* for a reason: it has no state of its own, so
    // there's nothing to copy or move at the base level. The
    // *derived* classes provide their own copy / move / dtor.
    //
    // Wait -- then how does `std::unique_ptr<PimplShape>` destroy
    // the object? It calls the *virtual* dtor, which dispatches
    // to the derived class's dtor. The base's dtor is the entry
    // point; the derived's dtor is what runs. That's the whole
    // point of the virtual keyword.
    //
    // We also don't need to declare the move / copy ops on the
    // base, because:
    //   - Moving a polymorphic object: clients use
    //     `std::move(*ptr)` only on *derived* references, not
    //     base references (moving through a base reference would
    //     slice). The base doesn't need move ops.
    //   - Copying a polymorphic object: same -- slice problem.
    //     The base provides `virtual clone()` instead.

    // -- Pure virtual interface --
    // print() is the lesson's headline: a client holds a
    // PimplShape& and calls print(), and the runtime picks the
    // derived class's print() based on the vptr.
    virtual void print(const std::string& tag) const = 0;

    // name() returns a stable, short string for the kind. The
    // base returns "shape"; the deriveds return "circle" /
    // "square". Non-pure virtual -- the base has a sensible
    // default. (A pure-virtual name() would force the deriveds
    // to spell it out, which is more ceremonial than useful.)
    virtual const char* name() const;

    // kind() returns the ShapeKind. Virtual for the same reason
    // as name(): a base reference doesn't statically know the
    // kind, so the accessor must dispatch through the vtable.
    virtual ShapeKind kind() const;

    // -- The virtual copy constructor --
    // The standard "virtual copy constructor" idiom. The derived
    // class implements clone() to return a `std::unique_ptr
    // <Derived>`, which is implicitly convertible to
    // `std::unique_ptr<PimplShape>`. The base declares the
    // interface as pure virtual.
    //
    // This is the *only* safe way to copy a polymorphic object:
    // a `PimplShape copy = *shape_ptr;` would slice (copy the
    // base part, lose the derived part). The virtual clone()
    // returns a heap-allocated *derived* object that the caller
    // owns via a base pointer.
    virtual std::unique_ptr<PimplShape> clone() const = 0;

protected:
    // The base has no data members. The derived class owns the
    // impl_. The base is a pure interface; the derived is the
    // PIMPL class. This avoids the "two impl_ in one object"
    // shadowing problem that would arise if both base and
    // derived had a unique_ptr<Impl>.

    // Protected default ctor. Derived ctors will call this as
    // part of their member-initializer list. The body is empty
    // -- the base has nothing to initialize.
    PimplShape() = default;
};
