// pimpl_circle.cpp -- PimplCircle::Impl definition + member bodies
//
// The Impl for a circle is small: an id, a ShapeKind tag (always
// kCircle), a name pointer (always "circle"), and the radius. The
// base class has no data, so the circle's Impl carries *everything*.
// That's the "base = pure interface, derived = PIMPL class" design.
#include "pimpl_circle.h"

#include <cmath>     // M_PI (or use 3.14159 explicitly)
#include <iostream>
#include <ostream>
#include <utility>   // std::move

#include "shape_visitor.h"  // ShapeVisitor (declared; full type needed for v.visit)

// -- Impl definition --
// Lives in the .cpp; the header only forward-declares it.
//
// The shape of this struct is the lesson: it's a small, private
// blob of state. The PimplCircle class is a `unique_ptr<Impl>`
// plus a vptr. That's it. The circle's "self" is entirely in
// the Impl, and the Impl is reached through the PIMPL pointer.
struct PimplCircle::Impl {
    std::size_t id_;
    ShapeKind kind_;
    const char* name_;
    double radius_;

    // area() is a derived-data helper. The class's print()
    // method uses it. Putting it in the Impl (rather than as
    // a free function in the .cpp) keeps the data and the
    // operations on it grouped.
    double area() const noexcept {
        // π * r²; using a literal to avoid pulling in <cmath>
        // just for M_PI. (Some toolchains don't define M_PI
        // without _USE_MATH_DEFINES on Windows.)
        return 3.14159265358979323846 * radius_ * radius_;
    }
};

// -- Ctor --
// Builds the Impl and stashes the radius. No base ctor work --
// the base is empty.
PimplCircle::PimplCircle(std::size_t id, double radius)
    : impl_(std::make_unique<Impl>()) {
    impl_->id_ = id;
    impl_->kind_ = ShapeKind::kCircle;
    impl_->name_ = "circle";
    impl_->radius_ = radius;
}

// -- Dtor --
// = default in the .cpp where Impl is complete. The `override`
// is in the header; the body here is just the unique_ptr<Impl>
// destruction.
PimplCircle::~PimplCircle() = default;

// -- Big Five (move) --
// Defaulted. Moving transfers the unique_ptr<Impl>.
PimplCircle::PimplCircle(PimplCircle&& other) noexcept = default;
PimplCircle& PimplCircle::operator=(PimplCircle&& other) noexcept = default;

// -- Big Five (copy) --
// Deep copy: make_unique<Impl>(*other.impl_) copies the radius,
// the id, the kind, the name pointer. The new PimplCircle is
// independent of the source.
PimplCircle::PimplCircle(const PimplCircle& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {
    std::cout << "  [PimplCircle copy ctor] deep copy of id="
              << other.impl_->id_ << " radius=" << other.impl_->radius_ << "\n";
}

PimplCircle& PimplCircle::operator=(const PimplCircle& other) {
    if (this != &other) {
        // Copy-and-swap would be cleaner, but for a PIMPL class
        // with a unique_ptr<Impl> member, the simplest correct
        // strong-exception-guarantee copy assign is:
        //   1. allocate a new Impl from *other.impl_
        //   2. assign it to impl_ (the old impl_ is destroyed)
        // The unique_ptr move-assign is noexcept, so this is
        // strong-guarantee. The make_unique<Impl>(*other.impl_)
        // might throw (std::bad_alloc, or anything Impl's copy
        // ctor throws), in which case the original impl_ is
        // untouched.
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

// -- Virtual interface overrides --

// print() is the lesson's headline. The base's print() is pure
// virtual; the derived implements it. When a client calls
// `shape_ptr->print("tag")` through a PimplShape& or
// unique_ptr<PimplShape>, the runtime dispatches through the
// vtable to *this* function.
//
// Inside the function, the access to impl_->id_ etc. is just a
// pointer dereference; the vtable is the *outer* dispatch, not
// the inner data access.
void PimplCircle::print(const std::string& tag) const {
    std::cout << "  PimplCircle(" << tag << ") [id=" << impl_->id_
              << " radius=" << impl_->radius_
              << " area=" << impl_->area() << "]\n";
}

// name() and kind() are the "self-description" accessors. The
// base has defaults ("shape" / kShape); the deriveds override.
const char* PimplCircle::name() const {
    return impl_->name_;
}

ShapeKind PimplCircle::kind() const {
    return impl_->kind_;
}

// -- Virtual copy constructor --
// Returns a *new* PimplCircle by deep copy. The base's signature
// is `unique_ptr<PimplShape>`. We return `unique_ptr<PimplCircle>`,
// which is implicitly convertible to `unique_ptr<PimplShape>`
// (unique_ptr has a converting constructor template that's
// enabled for derived-to-base conversion).
//
// The function does what the copy ctor does, but exposes the
// result via a base pointer so the caller can do:
//
//     std::unique_ptr<PimplShape> copy = original->clone();
//
// and have copy be a heap-allocated *Circle* that owns a *Circle* Impl.
std::unique_ptr<PimplShape> PimplCircle::clone() const {
    // std::make_unique<PimplCircle>(*this) calls the copy ctor
    // with `*this` as the source.
    return std::make_unique<PimplCircle>(*this);
}

// -- Visitor entry point --
// This is the *one* line of code that makes the double-dispatch
// pattern work. Inside this method, `*this` has static type
// `const PimplCircle&`. The call `v.visit(*this)` therefore
// resolves at compile time to `ShapeVisitor::visit(PimplCircle&)`
// -- the runtime dispatch through ShapeVisitor's vtable picks
// the right concrete visitor (AreaVisitor, JSONVisitor, etc.)
// for the right concrete shape (Circle).
//
// Why this works without dynamic_cast: the *static* type at the
// call site is `PimplCircle&` because we're inside
// `PimplCircle::accept`. The compiler does overload resolution
// on `v.visit(...)` based on `*this`'s static type, so it picks
// `visit(PimplCircle&)` -- the visitor's dispatch is then a
// single virtual call, picking the right concrete visitor.
//
// Compare to the alternative (a switch on `kind()` or a chain
// of `dynamic_cast` inside a free function): no cast, no switch,
// and adding a new shape just means adding a new `accept()` body
// in that shape's class.
void PimplCircle::accept(ShapeVisitor& v) const {
    v.visit(*this);
}

// -- Non-virtual accessor --
double PimplCircle::radius() const noexcept {
    return impl_->radius_;
}
