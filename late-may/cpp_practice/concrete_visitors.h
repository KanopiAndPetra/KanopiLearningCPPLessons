// concrete_visitors.h -- Concrete visitor implementations
//
// Three visitors are defined here, each demonstrating a different
// aspect of the pattern:
//
//   - AreaVisitor:     accumulates a sum (the most common visitor
//                      use case -- "compute a property that depends
//                      on the dynamic type").
//   - BoundingBoxVisitor: tracks min/max coords (a visitor that
//                      builds *compound* state, two values that
//                      depend on each other).
//   - JsonVisitor:     builds a string (a visitor that constructs
//                      a *serialization* of the visited shapes,
//                      using a JSON-like syntax).
//
// Why three? Each shows a different shape (pun intended) of state
// the visitor accumulates. AreaVisitor is the trivial case: one
// number. BoundingBoxVisitor is two numbers that update together.
// JsonVisitor is a string that grows by appending. The program
// demonstrates each one in turn, then compares the visitor pattern
// against the equivalent dynamic_cast / switch-on-kind dispatch.
#pragma once

#include <algorithm>   // std::min, std::max
#include <cstddef>     // std::size_t
#include <limits>      // std::numeric_limits
#include <sstream>     // std::ostringstream
#include <string>      // std::string

#include "shape_visitor.h"  // ShapeVisitor base

// Forward declarations of the concrete shapes (full types live in
// pimpl_circle.h / pimpl_square.h, included only where the visitor's
// body needs them).
class PimplCircle;
class PimplSquare;

// ---------------------------------------------------------------------------
// AreaVisitor: sums the area of every visited shape.
//
// Demonstrates the simplest case: one accumulator, two overrides,
// each adding the shape's area formula. The visitor owns the running
// total; the shapes are observed.
// ---------------------------------------------------------------------------
class AreaVisitor : public ShapeVisitor {
public:
    AreaVisitor() = default;

    void visit(const PimplCircle& c) const override;
    void visit(const PimplSquare& s) const override;

    // The result accessor. After visiting N shapes, total() is the
    // sum of their areas.
    double total() const noexcept { return total_; }

    // Reset for a fresh accumulation. Useful if the same visitor is
    // reused across multiple containers (a minor optimization, but
    // it shows the visitor is reusable like any other object).
    void reset() noexcept { total_ = 0.0; }

private:
    // mutable because visit() is `const` on the visitor (the visitor
    // observes the shape but its own state changes -- the visitor
    // is the *accumulator*, not the *thing being observed*). This is
    // the standard idiom for "logical const, mutable accumulator".
    mutable double total_ = 0.0;
};

// ---------------------------------------------------------------------------
// BoundingBoxVisitor: tracks the smallest axis-aligned rectangle that
// contains every visited shape (centered at the shape's local origin).
//
// This visitor shows two related fields updating together. The
// visitor could also be designed to compute the *exact* bounding box
// (treating each shape's center as the center of the shape) -- the
// program uses a simpler model where the bounding box is centered
// at (0,0) for every shape. This is enough to demonstrate the
// pattern without pulling in circle/square coordinate math.
//
// (A more realistic bounding box would use the actual geometry:
// for a circle of radius r, x ranges over [-r, +r]; for a square of
// side s, x ranges over [-s/2, +s/2]. The lesson uses these
// formulas.)
// ---------------------------------------------------------------------------
class BoundingBoxVisitor : public ShapeVisitor {
public:
    // Initialize min_/max_ to ±infinity so the first visit() actually
    // shrinks the box (rather than relying on the user to remember
    // to call reset()). The constructor calls reset() in the
    // initializer list -- this is the "always valid state" idiom.
    BoundingBoxVisitor() { reset(); }

    void visit(const PimplCircle& c) const override;
    void visit(const PimplSquare& s) const override;

    double min_x() const noexcept { return min_x_; }
    double max_x() const noexcept { return max_x_; }
    double min_y() const noexcept { return min_y_; }
    double max_y() const noexcept { return max_y_; }

    double width()  const noexcept { return max_x_ - min_x_; }
    double height() const noexcept { return max_y_ - min_y_; }

    void reset() noexcept {
        min_x_ =  std::numeric_limits<double>::infinity();
        max_x_ = -std::numeric_limits<double>::infinity();
        min_y_ =  std::numeric_limits<double>::infinity();
        max_y_ = -std::numeric_limits<double>::infinity();
    }

private:
    mutable double min_x_;
    mutable double max_x_;
    mutable double min_y_;
    mutable double max_y_;
};

// ---------------------------------------------------------------------------
// JsonVisitor: builds a JSON-like string representation of every
// visited shape. Demonstrates a visitor that *constructs* a richer
// data structure than a single number.
//
// The output is intentionally simple: a top-level array of shape
// objects. Each object has a "kind" field and a "value" field:
//
//     [{"kind":"circle","radius":1.5},{"kind":"square","side":2.0}]
//
// A real JSON visitor would build a recursive structure (a JSON
// value with arrays and objects), but a string is enough to
// demonstrate the pattern. The lesson's point is that *all* the
// type-specific logic lives in the visitor -- no dynamic_cast, no
// switch.
// ---------------------------------------------------------------------------
class JsonVisitor : public ShapeVisitor {
public:
    JsonVisitor() = default;

    void visit(const PimplCircle& c) const override;
    void visit(const PimplSquare& s) const override;

    // Read the accumulated JSON. After visiting N shapes, json()
    // is a JSON-like array containing all of them.
    std::string json() const { return out_; }

    void reset() { out_.clear(); }

private:
    // Append a comma before subsequent entries (handled internally
    // by checking `out_.empty()`). The output starts as empty
    // string and grows with each visit.
    mutable std::string out_;
};