// concrete_visitors.cpp -- Concrete visitor implementations
//
// The bodies of the three visitors declared in concrete_visitors.h.
// Each visit() body uses only the *public* accessors of the shape
// (radius(), side()), which means the visitor doesn't need to
// peek inside the Impl -- the PIMPL encapsulation is preserved.
//
// The pi constant used by AreaVisitor for circles is repeated here
// rather than in a header -- the lesson's point is the visitor
// pattern, not where pi is defined.
#include "concrete_visitors.h"

#include <cmath>       // (kept for symmetry with circle.cpp; not used)
#include <iomanip>     // std::setprecision
#include <ostream>
#include <sstream>     // std::ostringstream

#include "pimpl_circle.h"  // full type for visit(const PimplCircle&)
#include "pimpl_square.h"  // full type for visit(const PimplSquare&)

namespace {
// Local pi constant. Same value as pimpl_circle.cpp's
// implementation -- deliberately not shared via a header to keep
// the visitor's compilation independent of the shape's.
constexpr double kPi = 3.14159265358979323846;

// Format a double with reasonable precision for JSON output. The
// default precision (6 significant digits) is fine for the lesson,
// but we set it explicitly to avoid surprises across toolchains.
std::string fmt(double v) {
    std::ostringstream oss;
    oss << std::setprecision(10) << v;
    return oss.str();
}
}  // namespace

// ---------------------------------------------------------------------------
// AreaVisitor
// ---------------------------------------------------------------------------
void AreaVisitor::visit(const PimplCircle& c) const {
    const double r = c.radius();
    total_ += kPi * r * r;
}

void AreaVisitor::visit(const PimplSquare& s) const {
    const double side = s.side();
    total_ += side * side;
}

// ---------------------------------------------------------------------------
// BoundingBoxVisitor
// ---------------------------------------------------------------------------
void BoundingBoxVisitor::visit(const PimplCircle& c) const {
    // The circle is centered at the origin. Its bounding box extends
    // [-r, +r] in both x and y.
    const double r = c.radius();
    min_x_ = std::min(min_x_, -r);
    max_x_ = std::max(max_x_,  r);
    min_y_ = std::min(min_y_, -r);
    max_y_ = std::max(max_y_,  r);
}

void BoundingBoxVisitor::visit(const PimplSquare& s) const {
    // The square is centered at the origin with side length s.
    // Its bounding box extends [-s/2, +s/2] in both x and y.
    const double half = s.side() / 2.0;
    min_x_ = std::min(min_x_, -half);
    max_x_ = std::max(max_x_,  half);
    min_y_ = std::min(min_y_, -half);
    max_y_ = std::max(max_y_,  half);
}

// ---------------------------------------------------------------------------
// JsonVisitor
// ---------------------------------------------------------------------------
//
// The output format is a JSON array of objects:
//   [{"kind":"circle","radius":1.5},{"kind":"square","side":2.0}]
//
// Implementation notes:
//   - We use a mutable string `out_` and grow it via append. The
//     string is the visitor's "accumulator."
//   - We handle the comma separator explicitly: every visit except
//     the first one is preceded by a comma. We detect "first" by
//     checking if `out_` is empty.
//   - The shape's `id_` is *not* in the public API (it's in the
//     Impl), so the JSON only includes `radius` / `side` -- the
//     public accessors. That's the price of preserving PIMPL
//     encapsulation: the visitor can't see what it can't reach.
//     If we wanted to include `id_` in the JSON, we'd either
//     expose an `id()` accessor on the shape, or make JsonVisitor
//     a friend. The lesson keeps it public-accessor-only.

void JsonVisitor::visit(const PimplCircle& c) const {
    if (!out_.empty()) out_ += ',';
    out_ += R"({"kind":"circle","radius":)" + fmt(c.radius()) + '}';
}

void JsonVisitor::visit(const PimplSquare& s) const {
    if (!out_.empty()) out_ += ',';
    out_ += R"({"kind":"square","side":)" + fmt(s.side()) + '}';
}