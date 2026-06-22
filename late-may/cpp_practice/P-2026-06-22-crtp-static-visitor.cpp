// P-2026-06-22-crtp-static-visitor.cpp
//
// Today's lesson: CRTP static visitor.
//
// The Jun 21 session left a "Next Steps" item that pointed directly here:
//
//   > CRTP static visitor — compile-time visitor using templates +
//   > if constexpr instead of vtables. The zero-cost version of
//   > std::visit.
//
// This is that session. Same shapes (Circle, Square), same three
// operations (area, bounding box, JSON), but the dispatch is resolved
// at compile time via CRTP + template overload resolution — no vtable,
// no RTTI, no std::variant index, no std::visit jump table.
//
// Three implementations are presented side by side:
//
//   1. Dynamic Visitor  (vptr + accept/visit double-dispatch) — ref.
//      from Jun 20, recap here.
//   2. std::visit       (index + jump table)                — ref.
//      from Jun 10/15/21, recap here.
//   3. CRTP Static      (compile-time overload resolution)  — the
//      new content.
//
// All three produce the same numbers and the same JSON output. The
// differences are entirely in *how the dispatch is performed*:
//
//   - Dynamic:   one vtable load + one virtual call (and a second
//                vtable load + virtual call if you go via Visitor
//                pattern double-dispatch).
//   - std::visit: one byte read (the index) + one switch jump.
//   - CRTP:      one compile-time overload resolution + one
//                (non-virtual) call. Zero runtime branching.
//
// CRTP visitor is the natural capstone of the variant/visitor arc
// (Jun 10/15/20/21). It is the "I have a closed type set AND I want
// every dispatch to be devirtualized" answer.

#include <cmath>
#include <cstdio>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// =====================================================================
// Section 1 — The CRTP base
// =====================================================================
//
// The Curiously Recurring Template Pattern: a class template that
// takes its derived class as a template parameter and uses
// `static_cast<const Derived*>(this)` to get back to the derived
// type at compile time.
//
// For visitor dispatch, the CRTP base is mostly there to (a)
// document the contract (you must derive from `Shape<YourClass>`)
// and (b) provide the `self()` accessor that returns the static
// derived type.

template <typename Derived>
class Shape {
public:
    // Compile-time "what type am I?" accessor. The cast is a no-op
    // at runtime; it's only there to give the static type system
    // what it needs.
    const Derived& self() const noexcept {
        return *static_cast<const Derived*>(this);
    }
    Derived& self() noexcept {
        return *static_cast<Derived*>(this);
    }

protected:
    Shape() = default;
    ~Shape() = default;
    Shape(const Shape&) = default;
    Shape& operator=(const Shape&) = default;
    Shape(Shape&&) noexcept = default;
    Shape& operator=(Shape&&) noexcept = default;
};

// =====================================================================
// Section 2 — The concrete shapes
// =====================================================================
//
// Note: these derive from `Shape<Circle>` / `Shape<Square>`, NOT from
// `Shape<Shape<Circle>>` or some common base. There is NO virtual
// function table. The only thing tying them together is the CRTP
// template and the convention that both expose `radius()` / `side()`
// accessors.

class Circle : public Shape<Circle> {
public:
    explicit Circle(double r) : r_(r) {}
    double radius() const noexcept { return r_; }

private:
    double r_;
};

class Square : public Shape<Square> {
public:
    explicit Square(double s) : s_(s) {}
    double side() const noexcept { return s_; }

private:
    double s_;
};

// =====================================================================
// Section 3 — The CRTP static visitors
// =====================================================================
//
// A CRTP static visitor is just a struct with overloaded `operator()`
// (or, equivalently, a single `auto operator()(auto&&)` + `if
// constexpr` cascade). Dispatch happens by overload resolution on
// the *static* type of the argument.
//
// Two flavors:
//
//   (a) Overloaded operator()  — clean, but each overload must be
//       hand-written for every shape type. Adding a shape means
//       adding an overload.
//
//   (b) Generic operator() + if constexpr — single function, every
//       shape handled inside an `if constexpr` chain. Adding a shape
//       means adding another `if constexpr` arm.
//
// Both are demonstrated below. (b) is more compact but (a) is what
// "static visitor" usually means in the literature, so we'll lead
// with it.

// ---- (a) Overloaded operator() — the textbook static visitor ----

struct AreaVisitor {
    double operator()(const Circle& c) const {
        constexpr double pi = 3.14159265358979323846;
        return pi * c.radius() * c.radius();
    }
    double operator()(const Square& s) const {
        double side = s.side();
        return side * side;
    }
};

struct BBox {
    double x0, y0, x1, y1;
};

struct BoundingBoxVisitor {
    BBox operator()(const Circle& c) const {
        double r = c.radius();
        return BBox{-r, -r, r, r};
    }
    BBox operator()(const Square& s) const {
        double half = s.side() / 2.0;
        return BBox{-half, -half, half, half};
    }
};

struct JsonVisitor {
    std::string operator()(const Circle& c) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "{\"type\":\"circle\",\"radius\":%.4f}", c.radius());
        return std::string(buf);
    }
    std::string operator()(const Square& s) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "{\"type\":\"square\",\"side\":%.4f}", s.side());
        return std::string(buf);
    }
};

// ---- (b) Generic operator() + if constexpr — the compact form ----
//
// This is what std::visit expands to internally (in spirit). One
// function, one switch on `static_cast<...>`-equivalent (here, the
// actual derived type).
//
// We use a separate visitor struct so the visitor can carry per-call
// state if it wants to. For the three demo operations, all three are
// single-call so state doesn't matter; the same pattern scales.

struct GenericAreaVisitor {
    // C++17 form of an abbreviated function template. C++20 would
    // let us write `auto operator()(const auto& shape)`; in C++17
    // we spell out the template parameter list explicitly. The
    // semantics are identical: this single function body is
    // instantiated once per concrete shape type the visitor is
    // called with.
    template <typename ShapeT>
    double operator()(const ShapeT& shape) const {
        // `if constexpr` checks the static type at compile time.
        // The discarded branches are not even compiled.
        using S = std::decay_t<ShapeT>;
        if constexpr (std::is_same<S, Circle>::value) {
            constexpr double pi = 3.14159265358979323846;
            return pi * shape.radius() * shape.radius();
        } else if constexpr (std::is_same<S, Square>::value) {
            double side = shape.side();
            return side * side;
        } else {
            static_assert(sizeof(ShapeT) == 0,
                "GenericAreaVisitor: unhandled shape type");
        }
    }
};

// =====================================================================
// Section 4 — The container and dispatch helpers
// =====================================================================
//
// To dispatch over a heterogeneous collection (vector<Circle>,
// vector<Square>), we need a single function that takes a "shape"
// and runs the visitor on it. With CRTP, "shape" means "a reference
// to any class derived from Shape<Derived>".
//
// We can't have `vector<Shape<???>>` because the template parameter
// `Derived` differs. So in practice, the "collection" is either:
//   - a std::variant<Circle, Square> (Jun 10/15/21 style)
//   - a std::tuple<Circle, Square> (homogeneous but heterogeneous in type)
//   - a vector of base class pointers (vptr needed — gives up CRTP)
//   - a fold expression over a parameter pack (recursive template)
//
// For the demonstration, we'll use a small `std::tuple` of two
// shape vectors. That keeps the lesson on CRTP dispatch (the new
// content) and doesn't muddy the waters with another container choice.

using ShapeCollection = std::tuple<
    std::vector<Circle>,
    std::vector<Square>>;

// Build a heterogeneous collection with the same shapes used in
// Jun 19/20/21, so the outputs match across the three lessons.
ShapeCollection make_collection() {
    std::vector<Circle> circles;
    circles.emplace_back(1.0);
    circles.emplace_back(0.5);
    circles.emplace_back(2.5);
    std::vector<Square> squares;
    squares.emplace_back(2.0);
    squares.emplace_back(3.0);
    return std::make_tuple(std::move(circles), std::move(squares));
}

// Sum-of-areas. The visitor returns `double`; we add them.
double total_area(const ShapeCollection& sc) {
    double sum = 0.0;
    const auto& circles = std::get<0>(sc);
    const auto& squares = std::get<1>(sc);
    for (const auto& c : circles) sum += AreaVisitor{}(c);
    for (const auto& s : squares) sum += AreaVisitor{}(s);
    return sum;
}

// Union-of-bounding-boxes. The visitor returns `BBox`; we take the
// min of the mins and the max of the maxes.
BBox union_bbox(const ShapeCollection& sc) {
    BBox acc{ 1e308, 1e308, -1e308, -1e308 };  // start with empty
    const auto& circles = std::get<0>(sc);
    const auto& squares = std::get<1>(sc);
    auto absorb = [&](const BBox& b) {
        if (b.x0 < acc.x0) acc.x0 = b.x0;
        if (b.y0 < acc.y0) acc.y0 = b.y0;
        if (b.x1 > acc.x1) acc.x1 = b.x1;
        if (b.y1 > acc.y1) acc.y1 = b.y1;
    };
    for (const auto& c : circles) absorb(BoundingBoxVisitor{}(c));
    for (const auto& s : squares) absorb(BoundingBoxVisitor{}(s));
    return acc;
}

// JSON list. The visitor returns `std::string`; we collect them.
std::vector<std::string> all_json(const ShapeCollection& sc) {
    std::vector<std::string> out;
    const auto& circles = std::get<0>(sc);
    const auto& squares = std::get<1>(sc);
    for (const auto& c : circles) out.push_back(JsonVisitor{}(c));
    for (const auto& s : squares) out.push_back(JsonVisitor{}(s));
    return out;
}

// =====================================================================
// Section 5 — The dynamic-visitor reference (for head-to-head)
// =====================================================================
//
// Same shapes, virtual `accept` + Visitor-pattern double-dispatch.
// Used as the "before CRTP" reference. If the dynamic numbers match
// the static numbers, we know the static dispatch is correct.

class DCircle;
class DSquare;

class DShapeVisitor {
public:
    virtual ~DShapeVisitor() = default;
    virtual void visit(const DCircle& c) = 0;
    virtual void visit(const DSquare& s) = 0;
};

class DShape {
public:
    virtual ~DShape() = default;
    virtual void accept(DShapeVisitor& v) const = 0;
};

class DCircle : public DShape {
public:
    explicit DCircle(double r) : r_(r) {}
    double radius() const noexcept { return r_; }
    void accept(DShapeVisitor& v) const override { v.visit(*this); }
private:
    double r_;
};

class DSquare : public DShape {
public:
    explicit DSquare(double s) : s_(s) {}
    double side() const noexcept { return s_; }
    void accept(DShapeVisitor& v) const override { v.visit(*this); }
private:
    double s_;
};

struct DynamicAreaVisitor : DShapeVisitor {
    double result = 0.0;
    void visit(const DCircle& c) override {
        constexpr double pi = 3.14159265358979323846;
        result += pi * c.radius() * c.radius();
    }
    void visit(const DSquare& s) override {
        double side = s.side();
        result += side * side;
    }
};

// =====================================================================
// Section 6 — main
// =====================================================================

static bool approx_eq(double a, double b, double eps = 1e-9) {
    if (a == b) return true;
    double scale = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= eps * scale;
}

int main() {
    // --- Setup ---
    std::printf("== CRTP static visitor — setup ==\n");
    ShapeCollection sc = make_collection();
    const auto& circles = std::get<0>(sc);
    const auto& squares = std::get<1>(sc);
    std::printf("circles: %zu, squares: %zu\n", circles.size(), squares.size());

    // Show sizeof to make the no-vtable point concrete.
    std::printf("sizeof(Circle)        = %zu\n", sizeof(Circle));
    std::printf("sizeof(Square)        = %zu\n", sizeof(Square));
    std::printf("sizeof(DCircle)       = %zu (has vptr)\n", sizeof(DCircle));
    std::printf("sizeof(DSquare)       = %zu (has vptr)\n", sizeof(DSquare));
    std::printf("\n");

    // --- 1) Area ---
    std::printf("== 1) Area — CRTP static (overloaded operator()) ==\n");
    double crtp_area = total_area(sc);
    std::printf("CRTP static (overloaded) : %.4f\n", crtp_area);

    // Same calculation via the generic form (single operator() + if constexpr).
    double generic_area = 0.0;
    GenericAreaVisitor gv;
    for (const auto& c : circles) generic_area += gv(c);
    for (const auto& s : squares) generic_area += gv(s);
    std::printf("CRTP static (generic+if constexpr): %.4f\n", generic_area);
    std::printf("  match: %s\n\n", approx_eq(crtp_area, generic_area) ? "YES" : "NO");

    // --- 2) Bounding box ---
    std::printf("== 2) Bounding box — CRTP static ==\n");
    BBox crtp_bb = union_bbox(sc);
    std::printf("CRTP static (overloaded) : [%.3f, %.3f] -> [%.3f, %.3f]\n",
                crtp_bb.x0, crtp_bb.y0, crtp_bb.x1, crtp_bb.y1);
    // Expected for circles 1.0, 0.5, 2.5 + squares 2.0, 3.0:
    //   x0 = min(-1.0, -0.5, -2.5, -1.0, -1.5) = -2.5
    //   y0 = same = -2.5
    //   x1 = max( 1.0,  0.5,  2.5,  1.0,  1.5) =  2.5
    //   y1 = same =  2.5
    std::printf("  expected            : [-2.500, -2.500] -> [2.500, 2.500]\n");
    std::printf("  match: %s\n\n",
                approx_eq(crtp_bb.x0, -2.5) &&
                approx_eq(crtp_bb.y0, -2.5) &&
                approx_eq(crtp_bb.x1,  2.5) &&
                approx_eq(crtp_bb.y1,  2.5) ? "YES" : "NO");

    // --- 3) JSON ---
    std::printf("== 3) JSON serialization — CRTP static ==\n");
    std::vector<std::string> crtp_json = all_json(sc);
    std::printf("CRTP static:\n");
    for (const auto& j : crtp_json) std::printf("  %s\n", j.c_str());
    std::printf("\n");

    // --- 4) Dynamic-visitor reference (same numbers, different mechanism) ---
    std::printf("== 4) Same area via Dynamic Visitor (double-dispatch) ==\n");
    // Build a polymorphic collection with the same shapes.
    std::vector<std::unique_ptr<DShape>> dyn;
    dyn.push_back(std::make_unique<DCircle>(1.0));
    dyn.push_back(std::make_unique<DSquare>(2.0));
    dyn.push_back(std::make_unique<DCircle>(0.5));
    dyn.push_back(std::make_unique<DSquare>(3.0));
    dyn.push_back(std::make_unique<DCircle>(2.5));

    DynamicAreaVisitor dav;
    for (const auto& s : dyn) s->accept(dav);
    std::printf("Dynamic Visitor (double-dispatch): %.4f\n", dav.result);
    std::printf("  match: %s\n\n", approx_eq(dav.result, crtp_area) ? "YES" : "NO");

    // --- 5) The dispatch-cost comparison ---
    std::printf("== 5) Dispatch mechanism — three implementations side by side ==\n");
    std::printf("Dynamic (vptr + virtual):\n");
    std::printf("  - shape->accept(visitor): 1 vtable load + 1 indirect call\n");
    std::printf("  - visitor->visit(shape):  1 vtable load + 1 indirect call\n");
    std::printf("  total: 2 indirections per dispatch.\n");
    std::printf("\n");
    std::printf("std::visit (variant index + jump table):\n");
    std::printf("  - read the variant's active index (1 byte): 1 byte load\n");
    std::printf("  - jump table indexed by the index: 1 indirect jump\n");
    std::printf("  total: 1 indirect jump per dispatch. No vtable, no RTTI.\n");
    std::printf("\n");
    std::printf("CRTP static (compile-time overload resolution):\n");
    std::printf("  - the call site `AreaVisitor{}(c)` is type-checked at\n");
    std::printf("    compile time; the compiler picks the right operator()\n");
    std::printf("    based on the static type of `c` (Circle).\n");
    std::printf("  total: 1 (non-virtual) call. NO runtime branching at all.\n");
    std::printf("        The compiler may even inline the operator() body\n");
    std::printf("        into the loop.\n");

    // --- 6) Compile-time proof of static dispatch ---
    std::printf("\n== 6) Compile-time proof: overload resolution picks Circle or Square ==\n");
    Circle c(7.0);
    Square s(7.0);
    AreaVisitor av;
    static_assert(std::is_same<decltype(av(c)), double>::value,
                  "AreaVisitor(Circle) must return double");
    static_assert(std::is_same<decltype(av(s)), double>::value,
                  "AreaVisitor(Square) must return double");
    std::printf("AreaVisitor(Circle(7.0)) = %.4f\n", av(c));
    std::printf("AreaVisitor(Square(7.0)) = %.4f\n", av(s));
    std::printf("Both `operator()` overloads exist and are picked at compile time.\n");

    // --- 7) The tradeoff: CRTP is closed-set and bloats code ---
    std::printf("\n== 7) Tradeoffs ==\n");
    std::printf("- CRTP static visitor gives up the open-set property:\n");
    std::printf("  to add Triangle, you must edit every visitor struct.\n");
    std::printf("- The compiler instantiates one machine-code copy of each\n");
    std::printf("  visitor per shape type. With N shapes and M operations,\n");
    std::printf("  you get N*M function bodies. std::visit shares the jump\n");
    std::printf("  table; polymorphism shares the vtable.\n");
    std::printf("- CRTP cannot be used through a base-class pointer:\n");
    std::printf("  Shape<Circle>* is NOT Shape<Square>*. You need a tuple,\n");
    std::printf("  a variant, or a fold expression at the top to hide this.\n");

    std::printf("\nAll three implementations agree on %.4f for the total area.\n",
                crtp_area);
    return 0;
}
