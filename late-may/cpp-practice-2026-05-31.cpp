// ============================================================================
// cpp-practice-2026-05-31.cpp
// Topic: Separate Compilation (.h / .cpp files)
// ============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>

using namespace std;

// ============================================================================
// PART 1: Separate Compilation Concepts (all in one file for demo, but
//         shows the structure you'd split across .h and .cpp files)
// ============================================================================

// -----------------------------------------------------------------------
// RATIONALE: Why separate compilation matters
// -----------------------------------------------------------------------
// 1. Faster builds: change one .cpp file → only recompile that file
// 2. Organization: headers声明 ("what"), .cpp files定义 ("how")
// 3. Reusability: #include headers in multiple .cpp files → one definition
// 4. Encapsulation: hide implementation details in .cpp, expose interface in .h
//
// THE RULE: OneDefinition
// - Header (.h): class definition, function declarations, templates, inline bodies
// - Source (.cpp): full implementations (non-inline)
// - Never put using namespace std; in a header (pollutes every file that includes it!)
// - Use #ifndef / #pragma once to prevent double-inclusion
// -----------------------------------------------------------------------

// ============================================================================
// HEADER-STYLE CLASS DEFINITION:
// This is what you'd put in Rectangle.h — just the blueprint
// ============================================================================

// Forward-declaration if needed (not here)
// Namespace declaration...
// Or put class in a namespace (shown below)

// ============================================================================
// Rectangle class — used as prototype for separate compilation demo
// ============================================================================
class Rectangle {
private:
    double width_;
    double height_;
    string name_;

public:
    // Constructor declarations
    Rectangle();
    Rectangle(double w, double h, const string& name = "Rect");
    Rectangle(double side, const string& name); // square shortcut

    // Getters
    double area() const;
    double perimeter() const;
    double getWidth() const;
    double getHeight() const;
    string getName() const;

    // Setters (with validation)
    void setWidth(double w);
    void setHeight(double h);
    void setSize(double w, double h);

    // Operators
    bool operator==(const Rectangle& other) const;
    bool operator<(const Rectangle& other) const; // for sorting
    Rectangle operator+(const Rectangle& other) const; // combine areas

    string toString() const;

    // Static factory method (note: implementation in .h is fine since inline)
    static Rectangle makeSquare(double side, const string& name = "Square") {
        return Rectangle(side, side, name);
    }
};

// ============================================================================
// INLINE IMPLEMENTATION:
// For trivial getters/setters, inline in the header is fine
// But for anything non-trivial, put implementation in .cpp
// ============================================================================

// Rectangle inline methods (trivial, so OK in header-style file)
inline double Rectangle::getWidth() const { return width_; }
inline double Rectangle::getHeight() const { return height_; }
inline string Rectangle::getName() const { return name_; }

// ============================================================================
// NON-INLINE IMPLEMENTATIONS:
// These go in Rectangle.cpp in a real project
// ============================================================================

// Default constructor
Rectangle::Rectangle() : width_(1.0), height_(1.0), name_("DefaultRect") {}

// Full constructor with initializer list
Rectangle::Rectangle(double w, double h, const string& name)
    : width_(w), height_(h), name_(name) {
    if (w <= 0 || h <= 0) {
        cerr << "Warning: Invalid dimensions (" << w << ", " << h << ") "
             << "— using 1x1 instead." << endl;
        width_ = 1.0;
        height_ = 1.0;
    }
}

// Square shortcut constructor
Rectangle::Rectangle(double side, const string& name)
    : width_(side), height_(side), name_(name) {
    if (side <= 0) {
        cerr << "Warning: Invalid side (" << side << ") — using 1x1 instead." << endl;
        width_ = 1.0;
        height_ = 1.0;
    }
}

double Rectangle::area() const {
    return width_ * height_;
}

double Rectangle::perimeter() const {
    return 2 * (width_ + height_);
}

void Rectangle::setWidth(double w) {
    if (w > 0) width_ = w;
    else cerr << "Error: width must be positive." << endl;
}

void Rectangle::setHeight(double h) {
    if (h > 0) height_ = h;
    else cerr << "Error: height must be positive." << endl;
}

void Rectangle::setSize(double w, double h) {
    if (w > 0) width_ = w;
    else cerr << "Error: width must be positive." << endl;
    if (h > 0) height_ = h;
    else cerr << "Error: height must be positive." << endl;
}

string Rectangle::toString() const {
    ostringstream oss;
    oss << name_ << " (" << width_ << " x " << height_ << ")";
    return oss.str();
}

bool Rectangle::operator==(const Rectangle& other) const {
    // Two rects are equal if same area
    // (Could also be width/height comparison)
    return fabs(area() - other.area()) < 1e-9;
}

bool Rectangle::operator<(const Rectangle& other) const {
    return area() < other.area();
}

Rectangle Rectangle::operator+(const Rectangle& other) const {
    // Combines two rectangles into one with combined area
    double combined = area() + other.area();
    double side = sqrt(combined);
    return Rectangle(side, side, name_ + "+" + other.name_);
}

// Stream insertion — declaring as friend
// (In real project: declare in .h, implement in .cpp)
ostream& operator<<(ostream& os, const Rectangle& r) {
    os << r.toString() << " | Area: " << r.area()
       << " | Perim: " << r.perimeter();
    return os;
}

// ============================================================================
// DEMONSTRATION: Linker Concepts
// ============================================================================
//
// In a REAL project with separate compilation, the build process is:
//
//   Rectangle.h      ← class definition (declarations only)
//
//   Rectangle.cpp    ← IMPLEMENTATION of Rectangle methods
//                      Compile this: g++ -c Rectangle.cpp → Rectangle.o
//
//   main.cpp         ← uses Rectangle
//                      Compile this: g++ -c main.cpp → main.o
//
//   Link:            g++ main.o Rectangle.o -o program
//
//   The LINKER stitches everything together — your main() can call
//   Rectangle methods because the linker finds them in Rectangle.o
//
// ============================================================================
//
// COMMON ISSUES with separate compilation:
// 1. "undefined reference" → didn't link Rectangle.o, or forgot to compile .cpp
// 2. "multiple definition"  → put non-inline implementation in .h (Violated ODR!)
// 3. "included twice"       → need #ifndef GUARD or #pragma once
// 4. "previously declared"  → declare twice (once in .h, once in .cpp for same func)
//
// ============================================================================


// ============================================================================
// PART 2: Include Guards — How Headers Prevent Double Inclusion
// ============================================================================
void demoIncludeGuards() {
    cout << "\n=== Include Guards & Structure ===" << endl;
    cout << "A proper .h file looks like this:" << endl;
    cout << "\n  #ifndef RECTANGLE_H      // or #pragma once" << endl;
    cout << "  #define RECTANGLE_H" << endl;
    cout << "  class Rectangle { ... };" << endl;
    cout << "  #endif // RECTANGLE_H" << endl;
    cout << "\n  In .cpp, implementations go OUTSIDE the #ifndef block!" << endl;
    cout << "  Template code goes IN the header (must be visible at compile time)" << endl;
}

// ============================================================================
// PART 3: Build System — How to Organize a Multi-File Project
// ============================================================================
void demoBuildSystem() {
    cout << "\n=== Build System: Makefiles ===" << endl;
    string makefile = R"(Typical Makefile structure:
  .PHONY: all clean run

  OBJECTS = main.o rectangle.o

  all: program

  program: $(OBJECTS)
      g++ $(OBJECTS) -o program

  main.o: main.cpp rectangle.h
      g++ -c main.cpp -std=c++17

  rectangle.o: rectangle.cpp rectangle.h
      g++ -c rectangle.cpp -std=c++17

  run: program
      ./program

  clean:
      rm -f $(OBJECTS) program
)";
    cout << makefile << endl;
}

// ============================================================================
// PART 4: Real-world pattern — Point3D with separate compilation
// ============================================================================
class Point3D {
    double x_, y_, z_;
    string label_;
public:
    Point3D();
    Point3D(double x, double y, double z, const string& label = "P");

    double distanceTo(const Point3D& other) const;

    // Inline friendly
    double getX() const { return x_; }
    double getY() const { return y_; }
    double getZ() const { return z_; }
    const string& getLabel() const { return label_; }

    // Non-inline
    string toString() const;
    bool operator==(const Point3D& other) const;

    // For std::sort with a custom comparator lambda
    struct CompareByDistance {
        bool operator()(const Point3D& a, const Point3D& b, const Point3D& origin) const {
            return a.distanceTo(origin) < b.distanceTo(origin);
        }
    };
};

Point3D::Point3D() : x_(0), y_(0), z_(0), label_("Origin") {}

Point3D::Point3D(double x, double y, double z, const string& label)
    : x_(x), y_(y), z_(z), label_(label) {}

double Point3D::distanceTo(const Point3D& other) const {
    double dx = x_ - other.x_;
    double dy = y_ - other.y_;
    double dz = z_ - other.z_;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

string Point3D::toString() const {
    ostringstream oss;
    oss << label_ << "(" << x_ << ", " << y_ << ", " << z_ << ")";
    return oss.str();
}

bool Point3D::operator==(const Point3D& other) const {
    return fabs(x_ - other.x_) < 1e-9 &&
           fabs(y_ - other.y_) < 1e-9 &&
           fabs(z_ - other.z_) < 1e-9;
}

ostream& operator<<(ostream& os, const Point3D& p) {
    os << p.toString();
    return os;
}

// ============================================================================
// MAIN — Run all demos
// ============================================================================
int main() {
    cout << "==============================================" << endl;
    cout << "  Separate Compilation Practice — May 31, 2026" << endl;
    cout << "==============================================" << endl;

    // ---------------------------------------------------------------
    // Demo 1: Rectangle class — the main example
    // ---------------------------------------------------------------
    cout << "\n=== Demo 1: Rectangle Class ===" << endl;

    Rectangle r1(4.0, 5.0, "Living Room");
    Rectangle r2(3.0, 7.0, "Kitchen");
    Rectangle r3(6.0, "Storage");

    cout << "r1: " << r1 << endl;
    cout << "r2: " << r2 << endl;
    cout << "r3 (square): " << r3 << endl;

    // Static factory
    Rectangle square = Rectangle::makeSquare(4.0, "Office");
    cout << "Square (static factory): " << square << endl;

    // Comparison operators
    cout << "\nComparisons:" << endl;
    cout << "r1 == r2? " << (r1 == r2 ? "yes" : "no") << endl;
    cout << "r2 < r1? " << (r2 < r1 ? "yes" : "no") << endl;

    // Addition operator
    Rectangle rCombined = r1 + r2;
    cout << "\nr1 + r2 (combined area): " << rCombined << endl;

    // Setters
    r1.setSize(10.0, 6.0);
    cout << "After setSize(10,6): " << r1 << endl;

    // Invalid dimension handling
    Rectangle bad(-5.0, 3.0);

    // Sorting
    cout << "\n=== Sorting Rectangles by area ===" << endl;
    vector<Rectangle> rects = { r3, r1, r2, square };
    sort(rects.begin(), rects.end());
    for (const auto& r : rects) {
        cout << "  " << r << endl;
    }

    // ---------------------------------------------------------------
    // Demo 2: Point3D
    // ---------------------------------------------------------------
    cout << "\n=== Demo 2: Point3D Class ===" << endl;

    Point3D origin;
    Point3D pointA(3.0, 4.0, 0.0, "A");
    Point3D pointB(0.0, 0.0, 5.0, "B");
    Point3D pointC(1.0, 1.0, 1.0, "C");

    cout << "Origin: " << origin << endl;
    cout << "A: " << pointA << " (distance from origin: " << pointA.distanceTo(origin) << ")" << endl;
    cout << "B: " << pointB << " (distance from origin: " << pointB.distanceTo(origin) << ")" << endl;
    cout << "C: " << pointC << " (distance from origin: " << pointC.distanceTo(origin) << ")" << endl;

    cout << "\nDistance A→B: " << pointA.distanceTo(pointB) << endl;
    cout << "A == B? " << (pointA == pointB ? "yes" : "no") << endl;

    // Sort by distance from origin using lambda
    vector<Point3D> points = { pointA, pointB, pointC };
    sort(points.begin(), points.end(),
        [&origin](const Point3D& a, const Point3D& b) {
            return a.distanceTo(origin) < b.distanceTo(origin);
        });
    cout << "\nPoints sorted by distance from origin:" << endl;
    for (const auto& p : points) {
        cout << "  " << p << endl;
    }

    // ---------------------------------------------------------------
    // Demo 3: Structuring a project (conceptual)
    // ---------------------------------------------------------------
    demoIncludeGuards();
    demoBuildSystem();

    cout << "\n==============================================" << endl;
    cout << "  All demos complete!" << endl;
    cout << "==============================================" << endl;

    return 0;
}
