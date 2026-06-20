// shape_visitor.h -- The Visitor base for the PimplShape hierarchy
//
// The Visitor pattern's classic formulation: a separate hierarchy
// ("operations") that visits the elements of the existing hierarchy
// ("data"). The data hierarchy (PimplShape, PimplCircle, PimplSquare)
// gets one new virtual method -- accept(visitor&) -- that does the
// "second half" of the dispatch. The visitor hierarchy (ShapeVisitor)
// has one virtual visit() per concrete element type.
//
// Why a Visitor pattern at all?
//
//   In Jun 19 we observed that calling a *specific* accessor through
//   a base pointer requires a `dynamic_cast` (or a `switch` on
//   kind()). Section 2 of the Jun 19 program did exactly that: for
//   each shape in the container, it used `dynamic_cast<PimplCircle*>`
//   to fish out the radius and print it. That works for two deriveds
//   and one operation, but doesn't scale:
//
//     - Adding a new operation (e.g. "compute total area") means
//       touching every derived's accessor and adding a case in the
//       dispatch switch.
//     - Adding a new derived (e.g. PimplTriangle) means touching every
//       operation that switches on type, and re-compiling those TUs.
//
//   The Visitor pattern inverts the situation. The data hierarchy is
//   *closed* (adding a new derived is a small change in one place:
//   add an `accept` override and the new `visit(NewType&)` on the
//   visitor; everything else stays the same). New *operations* are
//   added by deriving a new visitor from ShapeVisitor -- no changes
//   to the data hierarchy needed.
//
// The trade-off: adding a *new derived* (PimplTriangle) requires
// changing every existing visitor (a new `visit(PimplTriangle&)`
// pure virtual). For shape hierarchies where the set of types is
// stable (which is the common case -- ASTs, shape libraries,
// expression trees), this is a good trade. For hierarchies where
// new types are added often, std::variant<Circle, Square, ...> is
// often a better fit (see the Jun 15 lesson on std::visit).
//
// Why forward-declarations in this header?
//
//   We forward-declare PimplCircle and PimplSquare rather than
//   including their headers. The visitor base declares `visit` for
//   each, but the *implementations* (the actual visitors in
//   concrete_visitors.cpp) will need the full headers.
//
//   This keeps shape_visitor.h cheap to include. A TU that wants to
//   define a new visitor type only needs pimpl_shape.h (to know
//   ShapeKind) plus the forward decls here, and only needs the full
//   derived headers in the .cpp where the visit() bodies live.
#pragma once

// Forward declarations of the concrete shape classes. We need them
// so the visitor can declare `void visit(PimplCircle&)` and
// `void visit(PimplSquare&)` in the base -- the parameter types
// must be named. (The implementations of visit() will include the
// full headers.)
class PimplCircle;
class PimplSquare;

// The visitor base. Each `visit` is pure virtual; concrete visitors
// override only the ones they care about. The pattern is:
//
//   - An operation that needs *all* deriveds' data (e.g. total area)
//     overrides all of them.
//   - An operation that only cares about a subset (e.g. "count only
//     circles") overrides only that subset.
//
// Pure virtual is used here so the base is abstract and you can't
// accidentally instantiate it. A "no-op default" would also work but
// would let you instantiate a visitor that ignores every shape, which
// is more likely to be a bug than a feature.
//
// We take the shape by `const &` so that `accept()` -- which is `const`
// -- can call `v.visit(*this)` cleanly. Visitors that need to mutate
// the visitor itself (e.g. accumulating a total) can still have
// non-const member variables on the visitor; the visitor reference
// passed to `accept` is non-const so the visitor's state can change.
// What can't change is the shape being visited, which is the more
// natural contract: a visitor *observes* shapes and accumulates
// information in itself.
class ShapeVisitor {
public:
    virtual ~ShapeVisitor() = default;

    // The two concrete deriveds. Each visit takes the derived by
    // const reference so the visitor can call accessors (radius(),
    // side()) on the shape without modifying it. The visitor itself
    // is non-const (it's the accumulator), so visit's `this` is
    // non-const inside the visitor's bodies.
    //
    // Note: we don't take a base reference here. That's the whole
    // point of double-dispatch -- the *static* type at this call site
    // is the *dynamic* type of the shape, so the visitor gets a
    // derived reference and can call derived-only accessors without
    // dynamic_cast.
    virtual void visit(const PimplCircle& c) const = 0;
    virtual void visit(const PimplSquare& s) const = 0;
};