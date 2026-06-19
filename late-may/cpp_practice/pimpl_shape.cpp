// pimpl_shape.cpp -- PimplShape member bodies (no Impl here)
//
// The base PimplShape has NO Impl. The derived classes (Circle,
// Square) own their own Impls. This file is therefore small:
// it just defines the virtual dtor and the non-pure virtual
// accessors (name(), kind()).
//
// Why is the file so small?
//   - The base is a pure polymorphic interface. It has no data,
//     no helpers, no big-five boilerplate. The PIMPL pattern
//     lives in the *derived* class, not the base.
//   - The only thing the base owns is the vptr (compiler-
//     generated, not visible in source) and the virtual-table
//     entries for the virtual functions.
//
// The trade-off:
//   - Cost: every derived class has its own PIMPL boilerplate.
//     A real codebase might factor this out with CRTP or a
//     mixin, but for a teaching example the boilerplate is
//     the lesson.
//   - Benefit: the base is a *stable* interface. Adding a
//     new virtual to PimplShape doesn't change any derived
//     class's storage (the vptr is in the base part of the
//     object, paid for once). The lesson's program observes
//     this directly.
#include "pimpl_shape.h"

#include <iostream>
#include <ostream>

// The virtual dtor. Body is empty because the base has no data
// to destroy; the derived dtor (which runs *first*, before the
// base dtor, in the destruction sequence) is what destroys the
// impl_. The base dtor's only role is to be the entry point of
// the virtual-dispatch chain.
PimplShape::~PimplShape() = default;

// name() -- base default returns "shape". Deriveds override.
const char* PimplShape::name() const {
    return "shape";
}

// kind() -- base default returns kShape. Deriveds override.
ShapeKind PimplShape::kind() const {
    return ShapeKind::kShape;
}
