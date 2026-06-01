// ============================================================================
// C++ Practice — Inheritance & Polymorphism
// Date: 2026-06-01
// Topic: Inheritance, virtual functions, polymorphism, abstract classes
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cmath>

using namespace std;

// ============================================================================
// PART 1: Simple Inheritance - Shape hierarchy
// ============================================================================

class Shape {
protected:
    string name;

public:
    Shape(const string& n) : name(n) {}
    virtual ~Shape() { cout << "  [Shape destructor: " << name << "]\n"; }

    // Pure virtual function — makes Shape abstract
    virtual double area() const = 0;
    virtual double perimeter() const = 0;

    // Virtual function with default implementation
    virtual void describe() const {
        cout << "  " << name << " — Area: " << area()
             << ", Perimeter: " << perimeter() << "\n";
    }

    // Non-virtual — uses static binding
    string getName() const { return name; }
};

// Circle inherits from Shape
class Circle : public Shape {
private:
    double radius;

public:
    Circle(const string& n, double r) : Shape(n), radius(r) {}
    ~Circle() override { cout << "  [Circle destructor: " << name << "]\n"; }

    double area() const override { return M_PI * radius * radius; }
    double perimeter() const override { return 2 * M_PI * radius; }

    void describe() const override {
        cout << "  " << name << " (Circle, r=" << radius << ") — "
             << "Area: " << area() << ", Perim: " << perimeter() << "\n";
    }

    double getRadius() const { return radius; }
};

// Rectangle inherits from Shape
class Rectangle : public Shape {
protected:
    double width;
    double height;

public:
    Rectangle(const string& n, double w, double h) : Shape(n), width(w), height(h) {}
    ~Rectangle() override { cout << "  [Rectangle destructor: " << name << "]\n"; }

    double area() const override { return width * height; }
    double perimeter() const override { return 2 * (width + height); }

    double diagonal() const { return sqrt(width*width + height*height); }
};

// Square inherits from Rectangle (specialized)
class Square : public Rectangle {
public:
    Square(const string& n, double side) : Rectangle(n, side, side) {}
    ~Square() override { cout << "  [Square destructor: " << name << "]\n"; }

    void describe() const override {
        cout << "  " << name << " (Square, side=" << width << ") — "
             << "Area: " << area() << "\n";
    }
};

// ============================================================================
// PART 2: Polymorphism with pointers and references
// ============================================================================

void printShapeInfo(const Shape& s) {
    // Uses dynamic dispatch even though it's a reference!
    cout << "  Reference dispatch: " << s.getName()
         << " area=" << s.area() << "\n";
}

void scaleShape(Shape* s, double factor) {
    // NOTE: This won't work as expected with raw pointers without
    // virtual setter methods. Here we just demonstrate the pointer call.
    cout << "  Pointer dispatch: " << s->getName()
         << " area (pre-scale)=" << s->area() << "\n";
}

// ============================================================================
// PART 3: Virtual Destructor demonstration
// ============================================================================

class Base {
protected:
    string name;
public:
    Base(const string& n) : name(n) {
        cout << "  Base constructor: " << name << "\n";
    }
    virtual ~Base() {
        cout << "  Base destructor: " << name << "\n";
    }
    virtual void identify() const {
        cout << "  I am Base (" << name << ")\n";
    }
};

class Derived : public Base {
private:
    string extra;
public:
    Derived(const string& n, const string& e) : Base(n), extra(e) {
        cout << "  Derived constructor: " << n << " + " << e << "\n";
    }
    ~Derived() override {
        cout << "  Derived destructor: " << name << " + " << extra << "\n";
    }
    void identify() const override {
        cout << "  I am Derived (" << name << ", " << extra << ")\n";
    }
};

// ============================================================================
// PART 4: Abstract class with factory pattern
// ============================================================================

class Animal {
protected:
    string name;
    int age;
public:
    Animal(const string& n, int a) : name(n), age(a) {}
    virtual ~Animal() {}

    // Pure virtual — MUST be overridden
    virtual string speak() const = 0;
    virtual string species() const = 0;

    // Virtual with default — CAN be overridden
    virtual string getInfo() const {
        return name + " (" + species() + "), age " + to_string(age);
    }

    string getName() const { return name; }
    int getAge() const { return age; }
};

class Dog : public Animal {
private:
    string breed;
public:
    Dog(const string& n, int a, const string& b) : Animal(n, a), breed(b) {}

    string speak() const override { return "Woof! Woof!"; }
    string species() const override { return "Canis familiaris"; }
    string getInfo() const override {
        return Animal::getInfo() + ", breed: " + breed;
    }
};

class Cat : public Animal {
private:
    bool indoorOnly;
public:
    Cat(const string& n, int a, bool indoor) : Animal(n, a), indoorOnly(indoor) {}

    string speak() const override { return "Meow..."; }
    string species() const override { return "Felis catus"; }
    string getInfo() const override {
        return Animal::getInfo() + (indoorOnly ? " [indoor]" : " [outdoor]");
    }
};

class Cow : public Animal {
public:
    Cow(const string& n, int a) : Animal(n, a) {}

    string speak() const override { return "MooOOOOO!"; }
    string species() const override { return "Bos taurus"; }
};

// Factory function — demonstrates polymorphic creation
unique_ptr<Animal> createAnimal(const string& type, const string& name, int age) {
    if (type == "dog") return make_unique<Dog>(name, age, "Mixed");
    if (type == "cat") return make_unique<Cat>(name, age, true);
    if (type == "cow") return make_unique<Cow>(name, age);
    return nullptr;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {

    cout << "========================================\n";
    cout << "  INHERITANCE & POLYMORPHISM PRACTICE\n";
    cout << "========================================\n\n";

    // ------------------------------------------------------------------------
    // SECTION 1: Basic inheritance and object creation
    // ------------------------------------------------------------------------
    cout << "--- Part 1: Basic Inheritance ---\n\n";

    Circle c("Pizza", 5.0);
    Rectangle r("Notebook", 4.0, 3.0);
    Square s("Tile", 4.0);

    cout << "\nDirect calls:\n";
    c.describe();
    r.describe();
    s.describe();

    // ------------------------------------------------------------------------
    // SECTION 2: Polymorphism via pointers (THE KEY CONCEPT!)
    // ------------------------------------------------------------------------
    cout << "\n--- Part 2: Polymorphism via Pointers ---\n\n";

    // Pointer to BASE class, pointing to DERIVED object
    Shape* shapes[] = { &c, &r, &s };

    cout << "Iterating shapes through base pointer (virtual dispatch):\n";
    for (Shape* sh : shapes) {
        sh->describe();  // Calls the DERIVED version!
    }

    // Why pointers? Without them: SLICING
    cout << "\nWithout pointers (slicing — BAD):\n";
    Circle s2 = c;  // Sliced! Only the Circle part copied to a Circle (works fine)
    cout << "  Circle copied to Circle: area=" << s2.area() << " (OK, no slicing here)\n";

    // Slicing demo with a non-abstract base:
    class Polygon {
    protected:
        string name;
    public:
        Polygon(const string& n) : name(n) {}
        virtual ~Polygon() {}
        virtual double area() const { return 0; }
        string getName() const { return name; }
    };
    class Triangle : public Polygon {
    private:
        double base, height;
    public:
        Triangle(const string& n, double b, double h) : Polygon(n), base(b), height(h) {}
        double area() const override { return 0.5 * base * height; }
    };
    Polygon p = Triangle("MyTri", 3, 4);  // Sliced! Polygon part only
    cout << "  Triangle assigned to Polygon (sliced): area=" << p.area()
         << " (should be 6, but slicing gave wrong vtable)\n";

    // ------------------------------------------------------------------------
    // SECTION 3: Virtual destructor demo
    // ------------------------------------------------------------------------
    cout << "\n--- Part 3: Virtual Destructor ---\n\n";

    {
        cout << "Creating Derived through Base pointer...\n";
        Base* ptr = new Derived("Bob", "ExtraData");
        cout << "Calling identify() via Base pointer:\n";
        ptr->identify();  // Calls Derived::identify()!
        cout << "Deleting via Base pointer...\n";
        delete ptr;  // Virtual destructor calls Derived then Base
    }

    // ------------------------------------------------------------------------
    // SECTION 4: Reference polymorphism
    // ------------------------------------------------------------------------
    cout << "\n--- Part 4: Polymorphism via References ---\n";
    cout << "\nPassing shape by const reference:\n";
    printShapeInfo(c);
    printShapeInfo(r);

    // ------------------------------------------------------------------------
    // SECTION 5: Abstract class — Animal hierarchy
    // ------------------------------------------------------------------------
    cout << "\n--- Part 5: Abstract Classes & Factory ---\n\n";

    vector<unique_ptr<Animal>> farm;

    // Add animals polymorphically
    farm.push_back(make_unique<Dog>("Buddy", 3, "Golden Retriever"));
    farm.push_back(make_unique<Cat>("Whiskers", 7, true));
    farm.push_back(make_unique<Cow>("Mabel", 5));

    cout << "Our farm animals:\n";
    for (const auto& animal : farm) {
        cout << "  " << animal->getInfo() << "\n";
        cout << "  Says: \"" << animal->speak() << "\"\n\n";
    }

    // ------------------------------------------------------------------------
    // SECTION 6: Dynamic casting
    // ------------------------------------------------------------------------
    cout << "--- Part 6: Dynamic Cast ---\n\n";

    Animal* genericDog = new Dog("Rex", 4, "German Shepherd");
    cout << "Generic animal pointer: " << genericDog->getInfo() << "\n";

    // Can we recover the specific type?
    Dog* specificDog = dynamic_cast<Dog*>(genericDog);
    if (specificDog) {
        cout << "  Cast to Dog: SUCCESS! ";
        cout << "It's a " << specificDog->species() << "\n";
    } else {
        cout << "  Cast to Dog: FAILED\n";
    }

    // Cat can't be cast to Dog
    Animal* genericCat = new Cat("Mittens", 2, true);
    Dog* catAsDog = dynamic_cast<Dog*>(genericCat);
    if (catAsDog) {
        cout << "  Cat cast to Dog: SUCCESS!\n";
    } else {
        cout << "  Cat cast to Dog: FAILED (as expected!)\n";
    }

    // Clean up heap allocations
    delete genericDog;
    delete genericCat;

    cout << "\n========================================\n";
    cout << "  PRACTICE COMPLETE\n";
    cout << "========================================\n";

    return 0;
}