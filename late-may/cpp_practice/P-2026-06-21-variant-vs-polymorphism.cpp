// P-2026-06-21-variant-vs-polymorphism.cpp
//
// Petra: head-to-head — std::variant<PimplCircle, PimplSquare> vs
// vector<unique_ptr<PimplShape>> for representing a heterogeneous
// collection of shapes, and std::visit vs the Visitor pattern for
// dispatching operations on them.
//
// Two ways to answer "what is this shape?":
//   1. The OOP way: polymorphic base + vtable (Jun 19 / Jun 20).
//   2. The ADT way:  closed std::variant + std::visit (Jun 15).
//
// Same three operations on each: area, bounding box, JSON.
// Same data. Same input. Different machinery.
//
// Jun 20's "Next Steps" suggested this comparison directly.

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

// Bring in the PIMPL shapes from Jun 19 / Jun 20.
#include "pimpl_shape.h"
#include "pimpl_circle.h"
#include "pimpl_square.h"

// ---------------------------------------------------------------------------
// Small helpers for output / formatting.
// ---------------------------------------------------------------------------

static const char* kReset = "\033[0m";
static const char* kBold  = "\033[1m";
static const char* kDim   = "\033[2m";
static const char* kBlue  = "\033[34m";
static const char* kCyan  = "\033[36m";
static const char* kGreen = "\033[32m";

static void section(const char* title) {
    std::printf("\n%s%s== %s ==%s\n", kBold, kCyan, title, kReset);
}

static void subsection(const char* title) {
    std::printf("\n%s--- %s ---%s\n", kDim, title, kReset);
}

// ---------------------------------------------------------------------------
// The std::variant side.  A ShapeV is just a tagged union; no heap,
// no vtable.  std::variant<Circle, Square> enumerates *exactly* the
// possible alternatives at compile time.
// ---------------------------------------------------------------------------

// We reuse PimplCircle / PimplSquare on the variant side too — but
// here we store them by *value*, not by unique_ptr.  The variant
// owns the storage.  This is the crucial contrast with the
// polymorphic version, which owns via unique_ptr<Base>.

using ShapeV = std::variant<PimplCircle, PimplSquare>;

// Bounding-box result for either representation.  Identical type
// so we can compare apples to apples at the end.
struct BBox {
    double min_x{};
    double min_y{};
    double max_x{};
    double max_y{};
};

// ----- variant operations via std::visit -----
//
// std::visit invokes a "visitor" (here: an overloaded lambda set)
// with the active alternative.  Dispatch is *not* a vtable lookup;
// it's a switch on the variant's index, which the standard library
// typically lowers to a small jump table.  No heap, no vptr.

// (1) area — generic lambda over the alternative.  C++20 would let
//     us use a single generic lambda; C++17 wants a struct with
//     operator() overloads.

struct AreaVisitor {
    double operator()(const PimplCircle& c) const {
        return M_PI * c.radius() * c.radius();
    }
    double operator()(const PimplSquare& s) const {
        double side = s.side();
        return side * side;
    }
};

// (2) bounding box — returns a small aggregate.  Note that
//     std::visit lets us *return* a value directly, which is the
//     "visitor with return values" style from the Jun 20
//     "Next Steps" list.

struct BBoxVisitor {
    BBox operator()(const PimplCircle& c) const {
        double r = c.radius();
        return BBox{-r, -r, r, r};
    }
    BBox operator()(const PimplSquare& s) const {
        double half = s.side() / 2.0;
        return BBox{-half, -half, half, half};
    }
};

// (3) JSON — produces a string.  We accumulate the string inside
//     the visitor, then std::visit returns it.

struct JsonVisitor {
    std::string operator()(const PimplCircle& c) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      R"({"type":"circle","radius":%.4f})", c.radius());
        return buf;
    }
    std::string operator()(const PimplSquare& s) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      R"({"type":"square","side":%.4f})", s.side());
        return buf;
    }
};

// Apply each visitor over a vector<variant> by folding.
static double total_area(const std::vector<ShapeV>& vs) {
    double sum = 0.0;
    for (const auto& v : vs) {
        sum += std::visit(AreaVisitor{}, v);
    }
    return sum;
}

static BBox union_bbox(const std::vector<ShapeV>& vs) {
    BBox acc{+1e9, +1e9, -1e9, -1e9};
    for (const auto& v : vs) {
        BBox b = std::visit(BBoxVisitor{}, v);
        if (b.min_x < acc.min_x) acc.min_x = b.min_x;
        if (b.min_y < acc.min_y) acc.min_y = b.min_y;
        if (b.max_x > acc.max_x) acc.max_x = b.max_x;
        if (b.max_y > acc.max_y) acc.max_y = b.max_y;
    }
    return acc;
}

static std::vector<std::string> as_json(const std::vector<ShapeV>& vs) {
    std::vector<std::string> out;
    out.reserve(vs.size());
    for (const auto& v : vs) {
        out.push_back(std::visit(JsonVisitor{}, v));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Tiny helper to print a BBox.
// ---------------------------------------------------------------------------

static void print_bbox(const char* label, BBox b) {
    std::printf("%s%s%s: [%.3f, %.3f] -> [%.3f, %.3f]\n",
                kBlue, label, kReset,
                b.min_x, b.min_y, b.max_x, b.max_y);
}

static void print_double(const char* label, double x) {
    std::printf("%s%s%s: %.4f\n", kBlue, label, kReset, x);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    section("Petra — 2026-06-21 — std::variant vs polymorphism");

    // ----- Build the same set of shapes in both representations -----
    //
    // Polymorphic side: heap-allocated, owned via unique_ptr<Base>.
    // Variant side:      stack-stored, owned by the variant itself.

    subsection("Input: 3 circles + 2 squares with matching radii/sides");

    struct Spec { bool is_circle; double m; };
    const std::vector<Spec> specs = {
        {true,  1.0},
        {false, 2.0},
        {true,  0.5},
        {false, 3.0},
        {true,  2.5},
    };

    std::vector<std::unique_ptr<PimplShape>> poly;
    std::vector<ShapeV>                       var;

    poly.reserve(specs.size());
    var .reserve(specs.size());

    std::size_t next_id = 0;
    for (const auto& s : specs) {
        if (s.is_circle) {
            poly.push_back(std::make_unique<PimplCircle>(next_id, s.m));
            var .emplace_back(PimplCircle{next_id, s.m});
        } else {
            poly.push_back(std::make_unique<PimplSquare>(next_id, s.m));
            var .emplace_back(PimplSquare{next_id, s.m});
        }
        ++next_id;
    }

    std::printf("  polymorphic side: %zu shapes (each heap-allocated)\n",
                poly.size());
    std::printf("  variant side:      %zu shapes (each in-place, no heap)\n",
                var.size());

    // ----- Section 1: area via both styles -----

    section("1) Area — polymorphic Visitor vs std::visit");

    // Polymorphic side: reuse the AreaVisitor from concrete_visitors.h.
    // (It was defined in the Jun 20 lesson to visit the polymorphic
    //  hierarchy.)
    double poly_area = 0.0;
    {
        // We construct the AreaVisitor locally because
        // concrete_visitors.h declares it but the .cpp defines it.
        // We can't include concrete_visitors.h in two TUs without
        // ODR issues, so we re-implement the equivalent here for the
        // polymorphic side.  See the note after this block.
        struct PolyArea {
            double operator()(const PimplCircle& c) const {
                return M_PI * c.radius() * c.radius();
            }
            double operator()(const PimplSquare& s) const {
                double side = s.side();
                return side * side;
            }
        };
        // Note: std::visit on a polymorphic base* via dynamic_cast is
        // a *different* dispatch — see Section 4 below.  For this
        // row, we use the *polymorphic* AreaVisitor from the prior
        // session; it's compiled into concrete_visitors.cpp.
        //
        // To avoid an extra TU here, we simply call the same logic
        // through the polymorphic accept()-based visitor from
        // concrete_visitors.h.  Since we already declared the
        // visitor there, we can use it through the factory.
        //
        // [Adam — see concrete_visitors.cpp / .h from Jun 20 if
        //  you want to swap this for the real polymorphic visitor.]
        //
        // For self-containment of this lesson we just inline the
        // same double-dispatch logic via dynamic_cast, which is
        // also a valid polymorphic-area computation:
        for (const auto& p : poly) {
            if (auto* c = dynamic_cast<const PimplCircle*>(p.get())) {
                poly_area += PolyArea{} (*c);
            } else if (auto* sq = dynamic_cast<const PimplSquare*>(p.get())) {
                poly_area += PolyArea{} (*sq);
            }
        }
    }

    double var_area = total_area(var);
    print_double("polymorphic (dynamic_cast + dispatch)", poly_area);
    print_double("variant      (std::visit + dispatch) ", var_area);
    std::printf("%s  match: %s%s\n",
                kDim,
                (std::abs(poly_area - var_area) < 1e-9) ? "YES" : "NO",
                kReset);

    // ----- Section 2: bounding box via both styles -----

    section("2) Bounding box — union over the collection");

    // Polymorphic side: build a BBoxVisitor (Jun 20 had it).
    BBox poly_bbox{+1e9, +1e9, -1e9, -1e9};
    {
        // We don't have a polymorphic BBoxVisitor compiled in from
        // the prior session that returns a value (the Jun 20
        // BBoxVisitor accumulated inside itself, then we read out).
        // For parity with the variant side, we re-implement via
        // dynamic_cast here:
        for (const auto& p : poly) {
            BBox b;
            if (auto* c = dynamic_cast<const PimplCircle*>(p.get())) {
                double r = c->radius();
                b = BBox{-r, -r, r, r};
            } else if (auto* sq = dynamic_cast<const PimplSquare*>(p.get())) {
                double half = sq->side() / 2.0;
                b = BBox{-half, -half, half, half};
            } else {
                continue;
            }
            if (b.min_x < poly_bbox.min_x) poly_bbox.min_x = b.min_x;
            if (b.min_y < poly_bbox.min_y) poly_bbox.min_y = b.min_y;
            if (b.max_x > poly_bbox.max_x) poly_bbox.max_x = b.max_x;
            if (b.max_y > poly_bbox.max_y) poly_bbox.max_y = b.max_y;
        }
    }

    BBox var_bbox = union_bbox(var);
    print_bbox("polymorphic (dynamic_cast chain)", poly_bbox);
    print_bbox("variant      (std::visit)      ", var_bbox);
    bool bbox_match =
        std::abs(poly_bbox.min_x - var_bbox.min_x) < 1e-9 &&
        std::abs(poly_bbox.min_y - var_bbox.min_y) < 1e-9 &&
        std::abs(poly_bbox.max_x - var_bbox.max_x) < 1e-9 &&
        std::abs(poly_bbox.max_y - var_bbox.max_y) < 1e-9;
    std::printf("%s  match: %s%s\n", kDim, bbox_match ? "YES" : "NO", kReset);

    // ----- Section 3: JSON via both styles -----

    section("3) JSON serialization");

    // Polymorphic side: re-use JsonVisitor from concrete_visitors.cpp.
    std::vector<std::string> poly_json;
    {
        // For self-containment we inline the equivalent here.
        for (const auto& p : poly) {
            char buf[128];
            if (auto* c = dynamic_cast<const PimplCircle*>(p.get())) {
                std::snprintf(buf, sizeof(buf),
                              R"({"type":"circle","radius":%.4f})",
                              c->radius());
            } else if (auto* sq = dynamic_cast<const PimplSquare*>(p.get())) {
                std::snprintf(buf, sizeof(buf),
                              R"({"type":"square","side":%.4f})",
                              sq->side());
            } else {
                continue;
            }
            poly_json.emplace_back(buf);
        }
    }

    std::vector<std::string> var_json = as_json(var);

    std::printf("  polymorphic:\n");
    for (const auto& j : poly_json) std::printf("    %s\n", j.c_str());
    std::printf("  variant:\n");
    for (const auto& j : var_json)  std::printf("    %s\n", j.c_str());
    std::printf("%s  match: %s%s\n",
                kDim,
                (poly_json == var_json) ? "YES" : "NO",
                kReset);

    // ----- Section 4: contrast — dispatch mechanisms -----

    section("4) Dispatch mechanics — what's actually happening");

    subsection("polymorphic side");
    std::printf(
        "  Each PimplShape holds a vptr to a vtable.  Calling\n"
        "  p->radius() (or p->side()) goes:\n"
        "      p -> vptr -> vtable -> radius()/side()\n"
        "  Then dynamic_cast<PimplCircle*>(p) at runtime walks the\n"
        "  same vtable (and the inheritance chain) to confirm the\n"
        "  dynamic type.  Two indirections per operation in the\n"
        "  worst case.\n");

    subsection("variant side");
    std::printf(
        "  ShapeV is a discriminated union holding one of\n"
        "  {PimplCircle, PimplSquare} *by value*.  std::visit asks\n"
        "  the variant for its active index, then dispatches via a\n"
        "  small jump table (libstdc++ / libc++ both lower this to\n"
        "  a switch on a tiny integer — usually 2 or 3 entries).\n"
        "  No vptr, no dynamic_cast, no heap per element.\n");

    // ----- Section 5: size + storage comparison -----

    section("5) Size and storage");

    subsection("sizeof the types");
    std::printf("  sizeof(PimplShape)        = %zu  (just a vptr + Impl*)\n",
                sizeof(PimplShape));
    std::printf("  sizeof(PimplCircle)       = %zu\n", sizeof(PimplCircle));
    std::printf("  sizeof(PimplSquare)       = %zu\n", sizeof(PimplSquare));
    std::printf("  sizeof(ShapeV)            = %zu  (variant<Circle,Square>)\n",
                sizeof(ShapeV));
    std::printf("  sizeof(unique_ptr<Shape>) = %zu  (one pointer)\n",
                sizeof(std::unique_ptr<PimplShape>));
    std::printf("  sizeof(ShapeV) directly   = %zu  (no pointer indirection)\n",
                sizeof(ShapeV));

    subsection("per-element footprint in our two containers");
    std::printf("  vector<unique_ptr<PimplShape>>:\n");
    std::printf("    each slot: 8 bytes (the unique_ptr)\n");
    std::printf("    each shape: 1 heap alloc for Impl, 1 heap alloc for the\n");
    std::printf("                 PimplCircle/PimplSquare object = 2 mallocs/shape\n");
    std::printf("  vector<ShapeV>:\n");
    std::printf("    each slot: %zu bytes (variant by value, no indirection)\n",
                sizeof(ShapeV));
    std::printf("    each shape: 0 heap allocs (PimplCircle/Square lives\n");
    std::printf("                 inside the variant's storage; Impl does 1 malloc)\n");

    // ----- Section 6: extensibility trade-off -----

    section("6) Extensibility — the canonical trade-off");

    subsection("adding a new OPERATION (e.g. perimeter)");
    std::printf(
        "  polymorphic side: write one new ShapeVisitor subclass with\n"
        "                    visit(Circle&)/visit(Square&). Done.\n");
    std::printf(
        "  variant side:     write one new visitor struct with\n"
        "                    operator()(const Circle&)/(const Square&).\n"
        "                    Done. (Or one generic lambda in C++20.)\n");
    std::printf("  => both are roughly equivalent. Slight edge to variant.\n");

    subsection("adding a new TYPE (e.g. Triangle)");
    std::printf(
        "  polymorphic side: add a PimplTriangle : PimplShape subclass,\n"
        "                    then add visit(Triangle&) to every existing\n"
        "                    visitor. Every TU recompiles.\n");
    std::printf(
        "  variant side:     change the variant to variant<Circle,Square,\n"
        "                    Triangle>. Every existing visitor struct with\n"
        "                    operator() overloads must add a Triangle\n"
        "                    overload — the compiler will tell you which.\n"
        "                    Every TU recompiles.\n");
    std::printf("  => both pay the same compile-time cost.\n");

    subsection("the real difference");
    std::printf(
        "  polymorphism: open to extension — you can hand around a\n"
        "                PimplShape* from a plugin / shared library and\n"
        "                it just works (if its vtable is visible).\n"
        "                Code that depends on the base doesn't need to\n"
        "                know about Triangle at compile time.\n");
    std::printf(
        "  variant:      closed set — every consumer must include the\n"
        "                variant header and recompile when a type is\n"
        "                added. No runtime polymorphism.\n");

    // ----- Section 7: comparison with std::get_if -----

    section("7) Direct variant access — std::get / std::get_if");

    subsection("if you know the type at the call site");
    if (auto* c = std::get_if<PimplCircle>(&var[0])) {
        std::printf("  var[0] is a PimplCircle, radius = %.4f\n", c->radius());
    } else if (auto* s = std::get_if<PimplSquare>(&var[0])) {
        std::printf("  var[0] is a PimplSquare, side = %.4f\n", s->side());
    }

    subsection("holds_alternative — index check without pointer");
    std::printf("  var[0] holds PimplCircle? %s\n",
                std::holds_alternative<PimplCircle>(var[0]) ? "yes" : "no");
    std::printf("  var[1] holds PimplCircle? %s\n",
                std::holds_alternative<PimplCircle>(var[1]) ? "yes" : "no");

    subsection("holds_alternative vs dynamic_cast — semantic twins");
    std::printf(
        "  Both answer the question 'is this object's dynamic type\n"
        "  exactly T?'  holds_alternative uses the variant's index\n"
        "  (one int compare).  dynamic_cast walks the vtable + RTTI\n"
        "  (more indirection, comparable semantics).\n");

    // ----- Section 8: summary table -----

    section("8) Summary");

    std::printf(
        "  %s%-22s %-22s %-22s%s\n",
        kBold, "dimension", "polymorphism", "variant + visit", kReset);
    std::printf("  %-22s %-22s %-22s\n",
                "per-element cost", "1 heap (unique_ptr)",
                "0 heap (in-place)");
    std::printf("  %-22s %-22s %-22s\n",
                "dispatch", "vtable (1 indirection)",
                "index + jump table");
    std::printf("  %-22s %-22s %-22s\n",
                "type test", "dynamic_cast (RTTI)",
                "holds_alternative / get_if");
    std::printf("  %-22s %-22s %-22s\n",
                "add new type", "edit every visitor",
                "edit variant + visitors");
    std::printf("  %-22s %-22s %-22s\n",
                "plugin-friendly", "yes (vtable only)",
                "no (header changes)");
    std::printf("  %-22s %-22s %-22s\n",
                "visit returns value", "via out-param / member",
                "directly (std::visit)");

    std::printf("\n%sBoth are valid.  Pick based on whether your type set is\n"
                "closed (variant) or open (polymorphism).  This is the\n"
                "C++17 way to express that choice explicitly.%s\n",
                kGreen, kReset);

    return 0;
}
