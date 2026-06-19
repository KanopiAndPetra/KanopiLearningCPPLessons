// pimpl_circle.h -- PimplCircle: a concrete derived PIMPL class
//
// The Jun 19 lesson's first concrete shape. The structure mirrors
// Jun 17/18's PIMPL classes exactly: forward-declared Impl, unique_ptr
// <Impl>, declared dtor (this time virtual, overrides the base's),
// declared copy ops with definitions in the .cpp, defaulted move ops
// in the header.
//
// What's *new* in this lesson:
//   - The class derives from PimplShape. The base is abstract; we
//     implement print() (pure virtual) and clone() (pure virtual),
//     and override name() and kind() (virtual with defaults).
//   - The destructor is `override` (and virtual by inheritance).
//     It's declared in the header and defined `= default` in the
//     .cpp, for the same reason as Jun 17/18: the unique_ptr<Impl>
//     member needs Impl to be complete to destruct correctly.
//   - The class has its own PIMPL member `impl_`. There is NO
//     member in the base; the design is "base = pure interface,
//     derived = PIMPL class."
#pragma once

#include <cstddef>   // std::size_t
#include <memory>    // std::unique_ptr
#include <string>    // std::string (for print's tag)
#include <utility>   // std::move

#include "pimpl_shape.h"  // base class + ShapeKind

class PimplCircle : public PimplShape {
public:
    // -- Construction --
    // The ctor builds an Impl and stashes the radius. The base
    // ctor is `= default` in the header (it's protected, but the
    // derived is allowed to call it via the member-initializer list).
    explicit PimplCircle(std::size_t id, double radius);

    // -- Virtual dtor (override) --
    // Declared in the header (compiler needs to know it's virtual
    // and overriding the base). Defined in the .cpp where Impl
    // is complete, so `delete impl_` works.
    ~PimplCircle() override;

    // -- Big Five --
    // Move ops: defaulted in the header. They only transfer the
    // unique_ptr<Impl>, so they don't need Impl to be complete.
    PimplCircle(PimplCircle&& other) noexcept;
    PimplCircle& operator=(PimplCircle&& other) noexcept;

    // Copy ops: declared in the header, defined in the .cpp. The
    // compiler-generated copy would try to copy the unique_ptr,
    // which is non-copyable, so we declare them ourselves. The
    // definition is a deep copy of the underlying Impl.
    PimplCircle(const PimplCircle& other);
    PimplCircle& operator=(const PimplCircle& other);

    // -- Virtual interface (overrides) --
    // print() is the lesson's headline. The base declares it
    // pure virtual; the derived must implement it. The `override`
    // keyword is a safety net: if we misspell the signature, the
    // compiler tells us.
    void print(const std::string& tag) const override;

    // name() and kind() are virtual but not pure in the base. The
    // derived overrides them with the circle-specific values. We
    // could also leave them unimplemented (and inherit the base's
    // "shape" / kShape), but that's wrong -- a Circle is *not* a
    // generic shape from the program's perspective.
    const char* name() const override;
    ShapeKind kind() const override;

    // -- Virtual copy constructor --
    // Returns a heap-allocated PimplCircle. The base's signature
    // is `unique_ptr<PimplShape>`. The covariance (returning
    // `unique_ptr<PimplCircle>` instead of `unique_ptr<PimplShape>`)
    // is *not* allowed for unique_ptr (smart pointers don't have
    // covariant return types in C++17), so we declare the return
    // as unique_ptr<PimplShape> and let the implicit conversion
    // happen.
    std::unique_ptr<PimplShape> clone() const override;

    // -- Non-virtual accessor --
    // The radius is stored in the Impl. Reading it doesn't need
    // to be virtual because it's not polymorphic -- every Circle
    // has a radius, and reading it through a derived reference
    // (which is what the caller must do) is fine.
    double radius() const noexcept;

private:
    // The PIMPL pointer. Same convention as Jun 17/18.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
