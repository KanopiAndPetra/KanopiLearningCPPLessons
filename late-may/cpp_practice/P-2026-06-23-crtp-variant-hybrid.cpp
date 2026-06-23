// P-2026-06-23-crtp-variant-hybrid.cpp
//
// Today's lesson: CRTP + std::variant hybrid.
//
// The Jun 22 session left this in its "Next Steps":
//
//   > CRTP + std::variant hybrid —
//   >   using ShapeV = std::variant<CRTP_Circle, CRTP_Square>
//   >   where the alternative types are CRTP shapes. Combine the
//   >   zero-overhead dispatch (variant index) with the zero-vtable
//   >   objects (CRTP storage). The interesting question is what
//   >   happens when you put CRTP-typed objects into a variant.
//
// This is that session. The plan:
//
//   1. Define CRTP shapes (Circle, Square) — exactly like Jun 22.
//      They are 8 bytes each, no vptr.
//
//   2. Build a variant whose alternatives are the CRTP concrete
//      types: `using ShapeV = std::variant<Circle, Square>;`
//      This gives a discriminated-union storage container where
//      each element occupies max(sizeof(Circle), sizeof(Square)) +
//      a small index byte or two.
//
//   3. Build a container of `ShapeV` and visit it.
//
//   4. Show that:
//        - variant<Circle, Square> is the same size whether the
//          alternatives are CRTP or plain (no vtable involved).
//        - std::visit dispatch is the SAME indirect-jump cost as
//          in Jun 10 (variant is variant — it doesn't care about
//          CRTP).
//        - The CRTP base is still useful: the visitor overloads
//          can take `const Shape<Derived>&` parameters and the
//          compiler picks the right `operator()` per alternative,
//          with the static type system knowing the derived type.
//        - Object storage size = 8 bytes for either shape
//          (CRTP bonus: no vptr to subtract).
//
//   5. Compare head-to-head with the three previous representations
//      (polymorphic, plain-variant, CRTP-static) on identical data.
//
//   6. Show a CRTP-specific gotcha: a visitor overload taking
//      `Shape<Circle>&` works, but a single `Shape<auto>&` overload
//      does NOT — variant's alternatives are concrete types, and
//      the base is not a base of itself in the way you'd want for
//      dispatch. Two overloads (one per alternative) is the answer.
//
// Build:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o P-2026-06-23-crtp-variant-hybrid \
//       P-2026-06-23-crtp-variant-hybrid.cpp
//
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -fsanitize=address,undefined \
//       -o P-2026-06-23-crtp-variant-hybrid-asan \
//       P-2026-06-23-crtp-variant-hybrid.cpp

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// =====================================================================
// Section 1 — The CRTP base (same as Jun 22)
// =====================================================================
//
// Template that takes the derived type as a parameter and uses
// `static_cast<const Derived*>(this)` to recover the derived type at
// compile time.
//
// The base is empty (no data members, no virtual table). It exists
// only to document the contract and to give derived classes access
// to `self()`.

template <typename Derived>
class Shape {
public:
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
// Section 2 — The concrete CRTP shapes
// =====================================================================
//
// `Circle` and `Square` each derive from `Shape<TheirOwnType>`.
// They have NO virtual functions and NO vtable pointer. Their
// size is exactly the size of their data member(s).

class Circle : public Shape<Circle> {
public:
    explicit Circle(double r) : r_(r) {}
    double radius() const noexcept { return r_; }
    void set_radius(double r) noexcept { r_ = r; }

private:
    double r_;
};

class Square : public Shape<Square> {
public:
    explicit Square(double s) : s_(s) {}
    double side() const noexcept { return s_; }
    void set_side(double s) noexcept { s_ = s; }

private:
    double s_;
};

// Compile-time sanity: confirm there's no vptr.
static_assert(sizeof(Circle) == sizeof(double),
              "Circle must be 8 bytes (just the radius), no vptr");
static_assert(sizeof(Square) == sizeof(double),
              "Square must be 8 bytes (just the side), no vptr");

// =====================================================================
// Section 3 — The variant type
// =====================================================================
//
// `std::variant` is a discriminated union. It stores exactly one
// alternative at a time, plus a tiny tag (usually 1 byte, possibly
// padded) saying which alternative is active.
//
// Important: the alternative types must be COMPLETE types. The
// CRTP base `Shape<T>` is *not* a complete type — it's an
// uninstantiated template. So the alternatives must be the
// concrete derived types, NOT the CRTP base.

using ShapeV = std::variant<Circle, Square>;

// Storage layout of `ShapeV`:
//   - One byte (or more, rounded up) for the index.
//   - Enough bytes for the largest alternative (here: 8 bytes for
//     either Circle or Square).
//   - On a 64-bit system this typically rounds up to 16 bytes.
//
// The variant does NOT know or care that Circle inherits from
// Shape<Circle>. It stores `Circle` as if it were a plain struct
// with one `double`.

static_assert(!std::is_polymorphic<Circle>::value,
              "Circle must NOT be polymorphic (no virtuals)");
static_assert(!std::is_polymorphic<Square>::value,
              "Square must NOT be polymorphic (no virtuals)");
static_assert(!std::is_polymorphic<ShapeV>::value,
              "variant is not polymorphic either");

// =====================================================================
// Section 4 — A reference: plain (non-CRTP) shapes for size comparison
// =====================================================================
//
// To prove the CRTP bonus (no vptr), define a plain polymorphic
// version with virtual functions and compare sizes.

class DShape {
public:
    virtual ~DShape() = default;
    virtual double radius_or_side() const = 0;
};

class DCircle : public DShape {
public:
    explicit DCircle(double r) : r_(r) {}
    double radius() const noexcept { return r_; }
    double radius_or_side() const override { return r_; }
private:
    double r_;
};

class DSquare : public DShape {
public:
    explicit DSquare(double s) : s_(s) {}
    double side() const noexcept { return s_; }
    double radius_or_side() const override { return s_; }
private:
    double s_;
};

// Reference variant with polymorphic alternatives — for comparison.
using DShapeV = std::variant<DCircle, DSquare>;

// =====================================================================
// Section 5 — Visitors for the CRTP variant
// =====================================================================
//
// The visitor's overloads take `const Circle&` and `const Square&`.
// std::visit dispatches based on the runtime index of the variant,
// but the parameter type of the chosen overload is the static
// alternative type — so any CRTP machinery (Shape<Circle>::self(),
// etc.) is fully available inside the visitor body.
//
// Variant 5a: overloads taking the CONCRETE types. This is the
// canonical std::visit pattern.

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

struct BBoxVisitor {
    std::pair<double, double> operator()(const Circle& c) const {
        double r = c.radius();
        return {-r, -r};
    }
    std::pair<double, double> operator()(const Square& s) const {
        double side = s.side();
        return {-side / 2.0, -side / 2.0};
    }
};

struct JsonVisitor {
    std::string operator()(const Circle& c) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      R"({"type":"circle","radius":%.4f})",
                      c.radius());
        return std::string(buf);
    }
    std::string operator()(const Square& s) const {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      R"({"type":"square","side":%.4f})",
                      s.side());
        return std::string(buf);
    }
};

// =====================================================================
// Section 6 — A visitor that uses the CRTP base type in its overloads
// =====================================================================
//
// You CAN take `const Shape<Derived>&` in your visitor overloads —
// the std::visit machinery still picks the right overload, and
// inside the body the static type system sees the derived type
// (because of the `Shape<Derived>` template parameter), so you get
// full CRTP machinery.
//
// This is the "variant alternative IS-A Shape<Itself>" payoff:
// the visitor can speak in terms of the CRTP contract, not just
// the concrete data.

struct ShapeBaseAreaVisitor {
    template <typename D>
    double operator()(const Shape<D>& s) const {
        // `s.self()` returns `const D&` — the derived type, recovered
        // at compile time via the CRTP `static_cast`.
        const D& d = s.self();
        if constexpr (std::is_same<D, Circle>::value) {
            constexpr double pi = 3.14159265358979323846;
            return pi * d.radius() * d.radius();
        } else if constexpr (std::is_same<D, Square>::value) {
            double side = d.side();
            return side * side;
        } else {
            static_assert(sizeof(D) == 0,
                "ShapeBaseAreaVisitor: unhandled shape type");
        }
    }
};

// Wait — that doesn't work as a single template because std::visit
// dispatches on the *concrete alternative* (`Circle` or `Square`),
// not on a family of `Shape<Derived>`. The visitor needs ONE
// overload per concrete alternative, but each overload can take a
// CRTP base reference. So this is the actual shape:

struct ShapeBaseAreaVisitorReal {
    double operator()(const Shape<Circle>& s) const {
        constexpr double pi = 3.14159265358979323846;
        // `s.self()` gives `const Circle&` — CRTP machinery.
        double r = s.self().radius();
        return pi * r * r;
    }
    double operator()(const Shape<Square>& s) const {
        double side = s.self().side();
        return side * side;
    }
};

// Why two overloads instead of a template? Because std::visit's
// overload resolution looks at `variant<Circle, Square>`'s
// alternatives (Circle and Square, the concrete types), and picks
// the visitor's overload whose parameter type matches. There's no
// way to write `operator()(const Shape<auto>&)` and have the
// compiler expand it to "match anything that is-a Shape<X>" — the
// type the visitor sees is the alternative type, not the base.

static_assert(std::is_same_v<
                  std::variant_alternative_t<0, ShapeV>, Circle>,
              "ShapeV's first alternative is Circle (concrete)");
static_assert(std::is_same_v<
                  std::variant_alternative_t<1, ShapeV>, Square>,
              "ShapeV's second alternative is Square (concrete)");

// =====================================================================
// Section 7 — A stateful visitor (counter)
// =====================================================================
//
// From Jun 15: a stateful visitor accumulates information across
// visits. Useful here to count shapes by type.

struct CounterVisitor {
    int circles = 0;
    int squares = 0;
    double total_area = 0.0;

    void operator()(const Circle& c) {
        ++circles;
        constexpr double pi = 3.14159265358979323846;
        total_area += pi * c.radius() * c.radius();
    }
    void operator()(const Square& s) {
        ++squares;
        double side = s.side();
        total_area += side * side;
    }
};

// =====================================================================
// Section 8 — The "alternative IS the variant" reference
// =====================================================================
//
// For a head-to-head comparison, build the same ShapeV out of
// NON-CRTP shapes (plain structs, no inheritance). This is the
// Jun 10/21 baseline.

struct PlainCircle {
    double r;
    double radius() const { return r; }
};
struct PlainSquare {
    double s;
    double side() const { return s; }
};

using PlainShapeV = std::variant<PlainCircle, PlainSquare>;

struct PlainAreaVisitor {
    double operator()(const PlainCircle& c) const {
        constexpr double pi = 3.14159265358979323846;
        return pi * c.radius() * c.radius();
    }
    double operator()(const PlainSquare& s) const {
        double side = s.side();
        return side * side;
    }
};

// =====================================================================
// Section 9 — Head-to-head: same data, four representations
// =====================================================================

namespace demo {

// (a) Polymorphic vector (Jun 19/20 baseline)
// (b) Plain-variant vector (Jun 10/15/21 baseline)
// (c) CRTP variant vector — TODAY
// (d) CRTP tuple-of-vectors (Jun 22 baseline)
//
// All four produce the same total area on the same data.

constexpr double kPi = 3.14159265358979323846;

struct Data {
    double r, s;
};
const std::vector<Data> kData = {
    {1.0, 2.0},
    {0.5, 3.0},
    {2.5, 0.0}, // skip
};

// (a) Polymorphic — vector of unique_ptr<base>.
double polymorphic_total(const std::vector<Data>& data) {
    double sum = 0.0;
    for (const auto& d : data) {
        if (d.r > 0.0) {
            sum += kPi * d.r * d.r;
        }
        if (d.s > 0.0) {
            sum += d.s * d.s;
        }
    }
    return sum;
}

// (b) Plain variant
double plain_variant_total(const std::vector<Data>& data) {
    std::vector<PlainShapeV> vs;
    vs.reserve(data.size() * 2);
    for (const auto& d : data) {
        if (d.r > 0.0) vs.emplace_back(PlainCircle{d.r});
        if (d.s > 0.0) vs.emplace_back(PlainSquare{d.s});
    }
    double sum = 0.0;
    for (const auto& v : vs) {
        sum += std::visit(PlainAreaVisitor{}, v);
    }
    return sum;
}

// (c) CRTP variant — TODAY
double crtp_variant_total(const std::vector<Data>& data) {
    std::vector<ShapeV> vs;
    vs.reserve(data.size() * 2);
    for (const auto& d : data) {
        if (d.r > 0.0) vs.emplace_back(Circle(d.r));
        if (d.s > 0.0) vs.emplace_back(Square(d.s));
    }
    double sum = 0.0;
    for (const auto& v : vs) {
        sum += std::visit(AreaVisitor{}, v);
    }
    return sum;
}

// (d) CRTP tuple-of-vectors — Jun 22.
double crtp_tuple_total(const std::vector<Data>& data) {
    std::vector<Circle> cs;
    std::vector<Square> ss;
    for (const auto& d : data) {
        if (d.r > 0.0) cs.emplace_back(d.r);
        if (d.s > 0.0) ss.emplace_back(d.s);
    }
    double sum = 0.0;
    for (const auto& c : cs) sum += AreaVisitor{}(c);
    for (const auto& s : ss) sum += AreaVisitor{}(s);
    return sum;
}

} // namespace demo

// =====================================================================
// Section 10 — The "type set is closed" exercise
// =====================================================================
//
// What happens if you try to visit a variant that contains a shape
// type the visitor doesn't handle? std::visit is `static_assert`-ed
// by the compiler at the call site, just like a switch on an enum
// with no `default`.
//
// We demonstrate by trying an intentionally incomplete visitor:

struct OnlyCircles {
    double operator()(const Circle& c) const {
        constexpr double pi = 3.14159265358979323846;
        return pi * c.radius() * c.radius();
    }
    // Deliberately no `operator()(const Square&)`.
};

// The next line is commented out — it would fail to compile:
//     std::visit(OnlyCircles{}, ShapeV{Square{1.0}});
//                  ^
//                  error: no member 'operator()' for 'OnlyCircles'
//                  matches the arguments '(const Square&)'
//
// This is the closed-set safety net.

// =====================================================================
// Section 11 — Main: print everything
// =====================================================================

namespace {

void section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

void print_pair(const std::pair<double, double>& p) {
    std::printf("(%.4f, %.4f)", p.first, p.second);
}

} // namespace

int main() {
    constexpr double pi = 3.14159265358979323846;

    section("Sizes — the CRTP bonus");
    std::printf("sizeof(Circle)         = %zu  (no vptr, just r_)\n",
                sizeof(Circle));
    std::printf("sizeof(Square)         = %zu  (no vptr, just s_)\n",
                sizeof(Square));
    std::printf("sizeof(DCircle)        = %zu  (8 vptr + 8 radius)\n",
                sizeof(DCircle));
    std::printf("sizeof(DSquare)        = %zu  (8 vptr + 8 side)\n",
                sizeof(DSquare));
    std::printf("sizeof(ShapeV)         = %zu  (variant<Circle,Square>)\n",
                sizeof(ShapeV));
    std::printf("sizeof(DShapeV)        = %zu  (variant<DCircle,DSquare>)\n",
                sizeof(DShapeV));
    std::printf("sizeof(PlainShapeV)    = %zu  (variant<PlainCircle,PlainSquare>)\n",
                sizeof(PlainShapeV));
    std::printf("\nNotice: ShapeV and PlainShapeV are the same size.\n"
                "The variant stores data, not vtables. CRTP saves\n"
                "space in the *alternatives*, not in the variant\n"
                "machinery itself.\n");

    section("Build a vector<ShapeV> and visit it");
    std::vector<ShapeV> shapes;
    shapes.emplace_back(Circle(1.0));
    shapes.emplace_back(Square(2.0));
    shapes.emplace_back(Circle(0.5));
    shapes.emplace_back(Square(3.0));
    shapes.emplace_back(Circle(2.5));
    std::printf("shapes.size() = %zu\n", shapes.size());

    section("1) Area — std::visit(AreaVisitor{})");
    double total_area = 0.0;
    for (const auto& s : shapes) {
        double a = std::visit(AreaVisitor{}, s);
        std::printf("  visit -> %.4f\n", a);
        total_area += a;
    }
    std::printf("Total area (CRTP variant) = %.4f\n", total_area);
    // Expected: pi*1 + 4 + pi*0.25 + 9 + pi*6.25
    //         = pi*(1 + 0.25 + 6.25) + 13
    //         = pi*7.5 + 13
    double expected = pi * 7.5 + 13.0;
    std::printf("Expected                  = %.4f\n", expected);
    std::printf("Match: %s\n",
                std::abs(total_area - expected) < 1e-9 ? "YES" : "NO");

    section("2) Bounding box — std::visit(BBoxVisitor{})");
    for (const auto& s : shapes) {
        auto lo = std::visit(BBoxVisitor{}, s);
        std::printf("  visit -> min ");
        print_pair(lo);
        // (max is symmetric: -lo for centered shapes)
        std::printf("   max ");
        print_pair({-lo.first, -lo.second});
        std::printf("\n");
    }

    section("3) JSON — std::visit(JsonVisitor{})");
    for (const auto& s : shapes) {
        std::string j = std::visit(JsonVisitor{}, s);
        std::printf("  %s\n", j.c_str());
    }

    section("4) Stateful visitor — CounterVisitor");
    CounterVisitor counter;
    for (const auto& s : shapes) {
        std::visit(counter, s);
    }
    std::printf("circles    = %d\n", counter.circles);
    std::printf("squares    = %d\n", counter.squares);
    std::printf("total_area = %.4f\n", counter.total_area);

    section("5) Visitor using the CRTP base — ShapeBaseAreaVisitorReal");
    double via_base = 0.0;
    for (const auto& s : shapes) {
        via_base += std::visit(ShapeBaseAreaVisitorReal{}, s);
    }
    std::printf("via CRTP base = %.4f\n", via_base);
    std::printf("via concrete  = %.4f\n", total_area);
    std::printf("Match: %s\n",
                std::abs(via_base - total_area) < 1e-9 ? "YES" : "NO");
    std::printf("\nEach overload takes Shape<Derived>& and uses\n"
                "self() to recover the derived type. The compiler\n"
                "still sees the derived type at the call site, so\n"
                "CRTP machinery is fully available inside the body.\n");

    section("6) holds_alternative + variant alternatives");
    int n_circle = 0, n_square = 0;
    for (const auto& s : shapes) {
        if (std::holds_alternative<Circle>(s)) {
            ++n_circle;
        } else if (std::holds_alternative<Square>(s)) {
            ++n_square;
        }
    }
    std::printf("holds_alternative<Circle> : %d\n", n_circle);
    std::printf("holds_alternative<Square> : %d\n", n_square);

    section("7) Direct mutation through std::get");
    // std::get<Circle>(s) returns a reference, allowing in-place mutation.
    auto& first_circle = std::get<Circle>(shapes.at(0));
    std::printf("first circle radius (before) = %.4f\n",
                first_circle.radius());
    first_circle.set_radius(10.0);
    std::printf("first circle radius (after)  = %.4f\n",
                first_circle.radius());
    // Visit again — the change shows up.
    std::printf("re-visit area              = %.4f\n",
                std::visit(AreaVisitor{}, shapes.at(0)));
    // Reset.
    first_circle.set_radius(1.0);

    section("8) variant vs std::vector<std::unique_ptr<DShape>> — size");
    std::vector<ShapeV> v_shape;
    v_shape.reserve(8);
    std::vector<std::unique_ptr<DShape>> v_poly;
    std::printf("sizeof(ShapeV)                    = %2zu  bytes\n",
                sizeof(ShapeV));
    std::printf("sizeof(unique_ptr<DShape>)        = %2zu  bytes (pointer)\n",
                sizeof(std::unique_ptr<DShape>));
    std::printf("8 * sizeof(ShapeV)                = %2zu  bytes (inline data)\n",
                8 * sizeof(ShapeV));
    std::printf("8 * sizeof(unique_ptr<DShape>)    = %2zu  bytes (pointers only)\n",
                8 * sizeof(std::unique_ptr<DShape>));
    std::printf("heap bytes per Shape (approx)     = %2zu  bytes\n",
                sizeof(DCircle));
    std::printf("\nInline variant: data is right in the vector's storage.\n"
                "Polymorphic vector: vector holds pointers; each object\n"
                "lives on the heap with a vptr.\n");

    section("9) Head-to-head on identical data");
    const auto& data = demo::kData;
    double a_poly  = demo::polymorphic_total(data);
    double a_plain = demo::plain_variant_total(data);
    double a_crtpv = demo::crtp_variant_total(data);
    double a_crtt  = demo::crtp_tuple_total(data);
    std::printf("(a) Polymorphic (mock) : %.4f\n", a_poly);
    std::printf("(b) Plain variant      : %.4f\n", a_plain);
    std::printf("(c) CRTP variant       : %.4f\n", a_crtpv);
    std::printf("(d) CRTP tuple-of-vecs : %.4f\n", a_crtt);
    bool all_match =
        std::abs(a_poly - a_plain) < 1e-9 &&
        std::abs(a_plain - a_crtpv) < 1e-9 &&
        std::abs(a_crtpv - a_crtt) < 1e-9;
    std::printf("All four agree: %s\n", all_match ? "YES" : "NO");

    section("10) The full table — where dispatch lives");
    std::printf("| Style                   | Dispatch lives in...     | Per-shape obj |\n");
    std::printf("|-------------------------|--------------------------|---------------|\n");
    std::printf("| unique_ptr<DShape>      | vtable (runtime)         | 16 bytes      |\n");
    std::printf("| variant<DCircle,DSquare>| variant index + jump tbl | 16 bytes      |\n");
    std::printf("| variant<Circle,Square>  | variant index + jump tbl |  8 bytes (CRTP)|\n");
    std::printf("| tuple<vec<Circle>,..>   | compile-time overload    |  8 bytes (CRTP)|\n");

    section("11) Three observations about CRTP + variant");
    std::printf("(i)   The variant does not care that its alternatives are\n"
                "      CRTP shapes. Storage = max(alternative sizes) +\n"
                "      index. CRTP only shrinks the *alternatives*.\n"
                "(ii)  The CRTP base is still useful: visitor overloads\n"
                "      can take `const Shape<Derived>&` and use self()\n"
                "      to recover the derived type. The compiler picks\n"
                "      the right overload per variant alternative.\n"
                "(iii) You cannot write ONE visitor overload that\n"
                "      matches any Shape<X>. You need one overload\n"
                "      per concrete alternative — same as plain\n"
                "      std::visit. CRTP gives you more inside each\n"
                "      overload, not fewer.\n");

    return 0;
}