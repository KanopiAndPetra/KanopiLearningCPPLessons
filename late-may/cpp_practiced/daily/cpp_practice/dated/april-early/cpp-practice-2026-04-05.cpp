// cpp-practice-2026-04-05.cpp
// Practice: Polymorphism and Virtual Functions
// Builds on: Classes (April 1), Pointers (April 1)
// New concept: runtime polymorphism via virtual functions

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// ============================================================
// BASE CLASS: Shape
// Demonstrates: virtual destructor, pure virtual function
// ============================================================
class Shape {
protected:
    string name;

public:
    Shape(string n) : name(n) {}
    virtual ~Shape() {
        cout << "Shape destructor: " << name << endl;
    }

    // PURE VIRTUAL FUNCTION — makes Shape abstract
    // Every derived class MUST implement this
    virtual double getArea() const = 0;

    // Regular virtual function with default implementation
    virtual void describe() const {
        cout << "This is a " << name << endl;
    }

    // Non-virtual function — uses static binding
    string getName() const { return name; }
};

// ============================================================
// DERIVED CLASS: Rectangle
// ============================================================
class Rectangle : public Shape {
private:
    double width;
    double height;

public:
    Rectangle(double w, double h)
        : Shape("Rectangle"), width(w), height(h) {}

    // Override getArea()
    double getArea() const override {
        return width * height;
    }

    // Override describe()
    void describe() const override {
        cout << "Rectangle: " << width << " x " << height
             << ", area = " << getArea() << endl;
    }

    // Rectangle-specific method
    double getPerimeter() const {
        return 2 * (width + height);
    }
};

// ============================================================
// DERIVED CLASS: Circle
// ============================================================
class Circle : public Shape {
private:
    double radius;
    static constexpr double PI = 3.14159265359;

public:
    Circle(double r) : Shape("Circle"), radius(r) {}

    double getArea() const override {
        return PI * radius * radius;
    }

    void describe() const override {
        cout << "Circle: radius " << radius
             << ", area = " << getArea() << endl;
    }

    double getCircumference() const {
        return 2 * PI * radius;
    }
};

// ============================================================
// DERIVED CLASS: Triangle
// ============================================================
class Triangle : public Shape {
private:
    double base;
    double height;

public:
    Triangle(double b, double h)
        : Shape("Triangle"), base(b), height(h) {}

    double getArea() const override {
        return 0.5 * base * height;
    }

    void describe() const override {
        cout << "Triangle: base " << base << ", height " << height
             << ", area = " << getArea() << endl;
    }
};

// ============================================================
// DEMONSTRATION: Why virtual matters (vs non-virtual)
// ============================================================
class Base {
public:
    // NON-VIRTUAL — static binding at compile time
    void identify() const {
        cout << "  Base::identify() called" << endl;
    }

    // VIRTUAL — dynamic binding at runtime
    virtual void whoAmI() const {
        cout << "  Base::whoAmI() called" << endl;
    }

    virtual ~Base() {}
};

class Derived : public Base {
public:
    void identify() const {
        cout << "  Derived::identify() called" << endl;
    }

    void whoAmI() const override {
        cout << "  Derived::whoAmI() called" << endl;
    }
};

// ============================================================
// DEMONSTRATION: Virtual Destructor
// ============================================================
class BaseWithVirtualDtor {
public:
    virtual ~BaseWithVirtualDtor() {
        cout << "  BaseWithVirtualDtor destroyed (virtual destructor)" << endl;
    }
};

class DerivedWithVirtualDtor : public BaseWithVirtualDtor {
public:
    ~DerivedWithVirtualDtor() override {
        cout << "  DerivedWithVirtualDtor destroyed" << endl;
    }
};

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: Polymorphism & Virtual Functions" << endl;
    cout << "=============================================" << endl;
    cout << endl;

    // -------------------------------------------------------
    // PART 1: Shape hierarchy — polymorphism in action
    // -------------------------------------------------------
    cout << "=== Shape Hierarchy Demo ===" << endl;

    // Create individual objects (stack)
    Rectangle rect(5.0, 3.0);
    Circle circle(2.0);
    Triangle tri(4.0, 6.0);

    rect.describe();
    circle.describe();
    tri.describe();

    // POLYMORPHISM: Store different types in ONE container
    // Shape* is a pointer to ANY derived class
    vector<Shape*> shapes;
    shapes.push_back(&rect);
    shapes.push_back(&circle);
    shapes.push_back(&tri);

    cout << endl;
    cout << "=== Polymorphism via Shape* array ===" << endl;
    cout << "Calling getArea() on each Shape*:" << endl;

    double totalArea = 0;
    for (const Shape* s : shapes) {
        // Which getArea() gets called?
        // Answer: determined at RUNTIME based on actual object type
        // This is DYNAMIC POLYMORPHISM
        totalArea += s->getArea();
        cout << "  " << s->getName() << " area: " << s->getArea() << endl;
    }
    cout << "Total area: " << totalArea << endl;

    // -------------------------------------------------------
    // PART 2: Why virtual matters — vs non-virtual
    // -------------------------------------------------------
    cout << endl;
    cout << "=== Virtual vs Non-Virtual Binding ===" << endl;

    Base* basePtr = new Derived();  // Base pointer to Derived object
    cout << "Calling on Base* pointing to Derived:" << endl;

    cout << "  basePtr->identify(): ";
    basePtr->identify();  // NON-VIRTUAL — Base::identify() called

    cout << "  basePtr->whoAmI(): ";
    basePtr->whoAmI();  // VIRTUAL — Derived::whoAmI() called!

    delete basePtr;

    // -------------------------------------------------------
    // PART 3: Virtual Destructor
    // -------------------------------------------------------
    cout << endl;
    cout << "=== Virtual Destructor Demo ===" << endl;
    cout << "Creating Base* pointing to Derived:" << endl;

    BaseWithVirtualDtor* poly = new DerivedWithVirtualDtor();
    cout << "Deleting poly..." << endl;
    delete poly;
    cout << "(Without virtual destructor, only Base destructor would run!)" << endl;

    // -------------------------------------------------------
    // SUMMARY
    // -------------------------------------------------------
    cout << endl;
    cout << "=============================================" << endl;
    cout << "Key concepts covered today:" << endl;
    cout << "  - Virtual functions (virtual keyword)" << endl;
    cout << "  - Pure virtual functions (= 0)" << endl;
    cout << "  - Abstract classes (can't instantiate)" << endl;
    cout << "  - Dynamic binding at RUNTIME" << endl;
    cout << "  - Shape* pointing to derived classes" << endl;
    cout << "  - Virtual destructor for proper cleanup" << endl;
    cout << "  - Override keyword (C++11)" << endl;
    cout << "  - Why: separate interface from implementation" << endl;

    return 0;
}
