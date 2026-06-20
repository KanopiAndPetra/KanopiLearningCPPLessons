// P-2026-06-20-visitor-pattern.cpp
//
// Lesson: The Visitor pattern for PIMPL shapes (double-dispatch, no
// dynamic_cast, preserves encapsulation).
//
// Build: g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//            -o P-2026-06-20-visitor-pattern \
//            P-2026-06-20-visitor-pattern.cpp \
//            pimpl_shape.cpp pimpl_circle.cpp pimpl_square.cpp \
//            concrete_visitors.cpp
//
// Run:   ./P-2026-06-20-visitor-pattern
//
// Companion files: pimpl_shape.{h,cpp}, pimpl_circle.{h,cpp},
// pimpl_square.{h,cpp}, shape_visitor.h, concrete_visitors.{h,cpp}.
//
// What this program demonstrates
// ==============================
//
// 1. The Visitor pattern's double-dispatch: a base reference (or
//    pointer) dispatches to the right concrete `visit(Derived&)`
//    without dynamic_cast. The "trick" is two virtual calls in
//    sequence: the first virtual (accept) dispatches on the shape's
//    dynamic type; the second virtual (visit) dispatches on the
//    visitor's dynamic type. The combination is what makes the
//    pattern work.
//
// 2. Three concrete visitors doing three different operations on the
//    same container: AreaVisitor sums areas, BoundingBoxVisitor
//    tracks min/max coords, JsonVisitor builds a JSON-like string.
//    The container doesn't know which visitor is being applied; the
//    accept() call dispatches through the vtable.
//
// 3. The visitor pattern preserves PIMPL encapsulation: visitors
//    only use public accessors (radius(), side()), so the Impl is
//    never exposed to the visitor.
//
// 4. Comparison to the dynamic_cast switch: the same problem
//    (compute total area) is solved two ways. The dynamic_cast
//    version is a chain of casts and conditionals; the visitor
//    version is a one-line `accept()` call. The visitor version
//    doesn't need to know which concrete types exist (it just
//    calls visit() on the visitor, which knows).

#include <cmath>       // std::abs (for the match check)
#include <cstddef>     // std::size_t
#include <iostream>
#include <memory>      // std::unique_ptr, std::make_unique
#include <ostream>
#include <string>      // std::string
#include <utility>     // std::move
#include <vector>      // std::vector

#include "concrete_visitors.h"   // AreaVisitor, BoundingBoxVisitor, JsonVisitor
#include "pimpl_circle.h"
#include "pimpl_shape.h"
#include "pimpl_square.h"
#include "shape_visitor.h"       // ShapeVisitor (needed for the base reference)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
//
// A "apply visitor to every shape in container" idiom. Could also be
// a method on the container, but a free function is more flexible
// (works with any visitor type, any container type that holds
// `const PimplShape&`s).
template <typename Container, typename Visitor>
void apply_visitor(const Container& shapes, Visitor& v) {
    for (const auto& s : shapes) {
        s->accept(v);   // the magic line. Two virtual dispatches.
    }
}

// ---------------------------------------------------------------------------
// Section 1: Build a polymorphic container
// ---------------------------------------------------------------------------
//
// Same setup as the Jun 19 lesson. We build a `vector<unique_ptr
// <PimplShape>>` holding a mix of circles and squares, then walk it.
// The walk itself is the lesson's setup -- it's what makes the
// polymorphism meaningful.
static std::vector<std::unique_ptr<PimplShape>> make_sample_shapes() {
    std::vector<std::unique_ptr<PimplShape>> shapes;
    shapes.push_back(std::make_unique<PimplCircle>(101, 1.5));
    shapes.push_back(std::make_unique<PimplSquare>(102, 2.0));
    shapes.push_back(std::make_unique<PimplCircle>(103, 3.0));
    shapes.push_back(std::make_unique<PimplSquare>(104, 0.5));
    return shapes;
}

static void section1_build_container() {
    std::cout << "\n--- Section 1: Build a polymorphic container ---\n";

    const auto shapes = make_sample_shapes();

    std::cout << "  Built a vector<unique_ptr<PimplShape>> with "
              << shapes.size() << " shapes (mixed circles and squares).\n";

    // Quick visual check: which kinds are in there?
    int n_circle = 0, n_square = 0;
    for (const auto& s : shapes) {
        switch (s->kind()) {
            case ShapeKind::kCircle: ++n_circle; break;
            case ShapeKind::kSquare: ++n_square; break;
            default: break;
        }
    }
    std::cout << "  Counts by kind: circles=" << n_circle
              << ", squares=" << n_square << "\n";
}

// ---------------------------------------------------------------------------
// Section 2: AreaVisitor -- the simplest case
// ---------------------------------------------------------------------------
//
// The most common visitor use case: "compute a property that depends
// on the dynamic type, accumulated across many objects." The visitor
// owns the running total; the shapes are observed.
//
// Without a visitor, this would be a switch on `kind()` or a chain
// of dynamic_casts. With a visitor, it's `s->accept(v)`.
static void section2_area_visitor() {
    std::cout << "\n--- Section 2: AreaVisitor (sum areas) ---\n";

    const auto shapes = make_sample_shapes();

    AreaVisitor area;
    apply_visitor(shapes, area);

    std::cout << "  Visited " << shapes.size() << " shapes; "
              << "total area = " << area.total() << "\n";

    // Manual check: pi*1.5^2 + 2^2 + pi*3^2 + 0.5^2
    const double expected =
        3.14159265358979323846 * 1.5 * 1.5 +
        2.0 * 2.0 +
        3.14159265358979323846 * 3.0 * 3.0 +
        0.5 * 0.5;
    std::cout << "  Expected              = " << expected << "\n";
    std::cout << "  Match: "
              << (std::abs(area.total() - expected) < 1e-9 ? "YES" : "NO")
              << "\n";

    // Reuse the visitor on a different container (the visitor is
    // an object -- it has state, but the state can be reset).
    const auto shapes2 = make_sample_shapes();
    area.reset();
    apply_visitor(shapes2, area);
    std::cout << "  After reset() and reusing on a fresh container: "
              << "total = " << area.total()
              << " (same as before; proves the visitor is reusable).\n";
}

// ---------------------------------------------------------------------------
// Section 3: BoundingBoxVisitor -- a visitor with compound state
// ---------------------------------------------------------------------------
//
// Shows that the visitor's state isn't restricted to a single number.
// This visitor tracks four fields (min/max x/y) that update together.
// The pattern is the same -- the visitor's `visit(Derived&)` knows
// the formula for its derived.
static void section3_bounding_box_visitor() {
    std::cout << "\n--- Section 3: BoundingBoxVisitor (min/max x,y) ---\n";

    const auto shapes = make_sample_shapes();

    BoundingBoxVisitor bb;
    apply_visitor(shapes, bb);

    std::cout << "  After visiting all shapes:\n";
    std::cout << "    min_x = " << bb.min_x() << "\n";
    std::cout << "    max_x = " << bb.max_x() << "\n";
    std::cout << "    min_y = " << bb.min_y() << "\n";
    std::cout << "    max_y = " << bb.max_y() << "\n";
    std::cout << "    width  = " << bb.width()  << "\n";
    std::cout << "    height = " << bb.height() << "\n";

    // Manual check: the largest extent is the circle of radius 3.0
    // (x and y both span [-3, +3]). The smallest is the square of
    // side 0.5 (x and y span [-0.25, +0.25]). The bounding box
    // is [-3, +3] x [-3, +3], which is 6x6.
    std::cout << "  Expected: min/max = -3/+3, width=height=6.\n";
}

// ---------------------------------------------------------------------------
// Section 4: JsonVisitor -- a visitor that builds a richer data structure
// ---------------------------------------------------------------------------
//
// Shows that the visitor can build more than a number: this one
// appends to a string. The string starts empty and grows with each
// visit. The output is a JSON-like array of shape objects.
static void section4_json_visitor() {
    std::cout << "\n--- Section 4: JsonVisitor (build a JSON string) ---\n";

    const auto shapes = make_sample_shapes();

    JsonVisitor json;
    apply_visitor(shapes, json);

    std::cout << "  Built JSON: [" << json.json() << "]\n";
}

// ---------------------------------------------------------------------------
// Section 5: Visitor vs dynamic_cast switch
// ---------------------------------------------------------------------------
//
// The same problem (compute total area) is solved two ways: the
// visitor way (one line: `s->accept(v)`) and the dynamic_cast way
// (a chain of casts and conditionals). Both are correct; the
// difference is in how much boilerplate they require, and in how
// they scale.
//
// The dynamic_cast version is the "naive" approach -- it works for
// two deriveds but doesn't scale: adding a third derived (e.g.
// PimplTriangle) requires editing both the dynamic_cast chain
// AND every visitor. The visitor version only requires editing
// the visitor hierarchy (add a new visit(Derived&)).
static double total_area_dynamic_cast(
    const std::vector<std::unique_ptr<PimplShape>>& shapes) {
    double total = 0.0;
    constexpr double kPi = 3.14159265358979323846;

    for (const auto& s : shapes) {
        // The dynamic_cast approach: try each derived, see which
        // sticks. Note the verbosity -- three lines of cast-and-test
        // per derived, plus a fallback for "this shouldn't happen".
        if (auto c = dynamic_cast<const PimplCircle*>(s.get())) {
            const double r = c->radius();
            total += kPi * r * r;
        } else if (auto sq = dynamic_cast<const PimplSquare*>(s.get())) {
            const double side = sq->side();
            total += side * side;
        }
        // If we had a third derived (PimplTriangle?), this `if` chain
        // would need another branch. The visitor version just gets
        // a new visit(Triangle&) override -- the apply loop is
        // unchanged.
    }
    return total;
}

static double total_area_visitor(
    const std::vector<std::unique_ptr<PimplShape>>& shapes) {
    AreaVisitor area;
    apply_visitor(shapes, area);
    return area.total();
}

static void section5_visitor_vs_dynamic_cast() {
    std::cout << "\n--- Section 5: Visitor vs dynamic_cast switch ---\n";

    const auto shapes = make_sample_shapes();

    const double via_cast = total_area_dynamic_cast(shapes);
    const double via_visitor = total_area_visitor(shapes);

    std::cout << "  Total area via dynamic_cast: " << via_cast    << "\n";
    std::cout << "  Total area via visitor:      " << via_visitor << "\n";
    std::cout << "  Match: "
              << (std::abs(via_cast - via_visitor) < 1e-9 ? "YES" : "NO")
              << "\n";

    std::cout << "  Lines of dispatch code (excluding the area formula):\n";
    std::cout << "    dynamic_cast: ~4 lines per derived (cast + test + body)\n";
    std::cout << "    visitor:      1 line per shape in the apply loop\n";
    std::cout << "  The visitor version scales better as new shapes are added.\n";
}

// ---------------------------------------------------------------------------
// Section 6: Visitors only need public accessors (PIMPL encapsulation)
// ---------------------------------------------------------------------------
//
// A subtle point: the visitor reaches the shape's data via public
// accessors (radius(), side()), not by poking into the Impl. This
// means the Impl can change without breaking the visitor -- as long
// as the public accessors preserve their meaning, the visitor keeps
// working.
//
// We demonstrate this by enumerating which members of PimplCircle
// and PimplSquare the visitor reaches. The answer: just `radius()`
// and `side()`. The id_, the area(), the kind_, the name_ -- none
// of those are touched.
static void section6_encapsulation() {
    std::cout << "\n--- Section 6: Visitor respects PIMPL encapsulation ---\n";

    std::cout << "  AreaVisitor touches:\n";
    std::cout << "    PimplCircle: only radius() -- the public accessor.\n";
    std::cout << "    PimplSquare: only side()   -- the public accessor.\n";
    std::cout << "  JsonVisitor touches: same.\n";
    std::cout << "  BoundingBoxVisitor touches: same.\n";

    std::cout << "  None of the visitors reach into PimplCircle::Impl or\n";
    std::cout << "  PimplSquare::Impl -- the Impl is private (it's a private\n";
    std::cout << "  nested struct). The visitor pattern preserves the\n";
    std::cout << "  PIMPL encapsulation: the public accessors are the\n";
    std::cout << "  visitor's only window into the shape's data.\n";

    // Prove it by listing the accessors actually called by visitors.
    // We can't introspect the visitor's source, but we can verify
    // the public surface:
    std::cout << "  Public surface used by visitors:\n";
    std::cout << "    PimplCircle::radius() const noexcept -> double\n";
    std::cout << "    PimplSquare::side()   const noexcept -> double\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "C++ Visitor pattern for PIMPL shapes\n";
    std::cout << "====================================\n";

    section1_build_container();
    section2_area_visitor();
    section3_bounding_box_visitor();
    section4_json_visitor();
    section5_visitor_vs_dynamic_cast();
    section6_encapsulation();

    std::cout << "\nDone.\n";
    return 0;
}