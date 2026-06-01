// cpp-practice-2026-04-08.cpp
// Practice: Smart Pointers
// Builds on: Polymorphism (April 5), Operator Overloading (April 7)
// New concepts: unique_ptr, shared_ptr, make_unique, make_shared
// Why smart pointers: automatic memory management, prevent leaks, ownership semantics

#include <iostream>
#include <memory>   // smart pointers live here
#include <string>
#include <vector>

using namespace std;

// ============================================================
// PART 1: Why raw pointers are dangerous
// ============================================================
void demonstrateRawPointerDanger() {
    cout << "=== Raw Pointer Danger ===" << endl;

    // PROBLEM: Who deletes this?
    // If an exception is thrown before delete — MEMORY LEAK
    // If delete is forgotten — MEMORY LEAK
    // If delete is called twice — CRASH / undefined behavior
    int* raw = new int(42);
    cout << "raw pointer: " << *raw << endl;
    delete raw;  // Easy to forget!
    raw = nullptr;  // Easy to forget this too!

    // ANOTHER PROBLEM: Pointer aliasing
    // Multiple pointers to same memory — who owns it?
    int* p1 = new int(10);
    int* p2 = p1;  // Both point to same memory
    // If p1 deletes, p2 becomes DANGEROUS (dangling pointer)
    delete p1;
    // delete p2;  // Would crash — already deleted!
    p1 = nullptr;
    p2 = nullptr;

    cout << "Raw pointers require manual delete — error-prone." << endl;
    cout << endl;
}

// ============================================================
// PART 2: unique_ptr — exclusive ownership
// ============================================================
class Resource {
private:
    string name;
public:
    Resource(string n) : name(n) { cout << "Resource acquired: " << name << endl; }
    ~Resource() { cout << "Resource released: " << name << endl; }
    void use() { cout << "Using resource: " << name << endl; }
};

void demonstrateUniquePtr() {
    cout << "=== unique_ptr — Exclusive Ownership ===" << endl;

    // unique_ptr<T> — ONE owner at a time
    // When unique_ptr goes out of scope, it AUTOMATICALLY deletes
    unique_ptr<int> up1 = make_unique<int>(42);
    cout << "unique_ptr value: " << *up1 << endl;

    // unique_ptr<int> up2 = up1;  // ERROR: deleted copy constructor
    // unique_ptr<int> up2(up1);   // ERROR: deleted copy constructor

    // Transfer ownership with std::move
    unique_ptr<int> up2 = move(up1);  // up1 is now nullptr!
    cout << "After move, up2: " << *up2 << endl;
    // cout << *up1;  // CRASH — up1 is nullptr

    // unique_ptr with custom deleter
    unique_ptr<FILE, int(*)(FILE*)> 
        upFile(fopen("test.txt", "w"), fclose);
    fprintf(upFile.get(), "Hello from unique_ptr!\n");

    // Resource example
    cout << endl;
    unique_ptr<Resource> res1 = make_unique<Resource>("Camera");
    res1->use();

    {
        unique_ptr<Resource> res2 = make_unique<Resource>("Microphone");
        res2->use();
        cout << "  Exiting inner scope..." << endl;
        // res2 is destroyed here — "Microphone" released
    }
    cout << "Back in outer scope — res1 still alive." << endl;

    cout << endl;
}

// ============================================================
// PART 3: shared_ptr — shared ownership
// ============================================================
void demonstrateSharedPtr() {
    cout << "=== shared_ptr — Reference-Counted Ownership ===" << endl;

    // shared_ptr<T> — reference counted
    // Last shared_ptr to die → deletes the object
    shared_ptr<int> sp1 = make_shared<int>(100);
    cout << "sp1 use_count: " << sp1.use_count() << endl;

    {
        shared_ptr<int> sp2 = sp1;  // Shares ownership — count goes up
        cout << "sp2 after copy: " << sp2.use_count() << endl;
        cout << "sp1 == sp2: " << (sp1 == sp2 ? "same object" : "different") << endl;
        *sp2 = 200;
        cout << "Changed via sp2, sp1 now: " << *sp1 << endl;
        cout << "Both point to: " << *sp1 << endl;
    }  // sp2 destroyed — count decrements, but sp1 still alive

    cout << "After sp2 destroyed, sp1 use_count: " << sp1.use_count() << endl;

    // shared_ptr with custom deleter
    shared_ptr<FILE> spFile(fopen("test2.txt", "w"), fclose);
    fprintf(spFile.get(), "Hello from shared_ptr!\n");
    // File closed when last shared_ptr dies

    cout << endl;
}

// ============================================================
// PART 4: Smart pointers and polymorphism
// ============================================================
class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
    virtual string name() const = 0;
};

class Square : public Shape {
    double side;
public:
    Square(double s) : side(s) {}
    double area() const override { return side * side; }
    string name() const override { return "Square"; }
};

class Circle : public Shape {
    double radius;
public:
    Circle(double r) : radius(r) {}
    double area() const override { return 3.14159 * radius * radius; }
    string name() const override { return "Circle"; }
};

void demonstratePolymorphicSmartPointers() {
    cout << "=== Smart Pointers + Polymorphism ===" << endl;

    // PROBLEM with raw pointers + polymorphism:
    // Shape* shapes[2];
    // shapes[0] = new Square(5);  // Who deletes these?
    // shapes[1] = new Circle(3);
    // ... leaks if exception thrown ...

    // SOLUTION: vector of unique_ptr<Shape>
    // Can't copy unique_ptr, so use move()
    vector<unique_ptr<Shape>> shapes;
    shapes.push_back(make_unique<Square>(5.0));
    shapes.push_back(make_unique<Circle>(3.0));

    cout << "Shapes using unique_ptr:" << endl;
    double total = 0;
    for (const auto& s : shapes) {
        cout << "  " << s->name() << " area: " << s->area() << endl;
        total += s->area();
    }
    cout << "Total area: " << total << endl;
    // Automatic cleanup when vector goes out of scope!

    // shared_ptr for polymorphism when sharing is needed
    shared_ptr<Shape> base = make_shared<Square>(4.0);
    cout << endl;
    cout << "shared_ptr to Square: " << base->name() << " area: " << base->area() << endl;

    cout << endl;
}

// ============================================================
// PART 5: Smart pointers and the DSR connection
// ============================================================
/*
Why smart pointers matter for DSR/ModularResonance:

If you have a Helix class and a Manifold class that hold large
datasets (trajectory data, manifold fitting results), raw pointers
create ownership ambiguity:

  Helix* h = new Helix(trajectory_data);
  Manifold* m = new Manifold(h);  // Who owns h?

With smart pointers:

  auto h = make_unique<Helix>(trajectory_data);
  auto m = make_shared<Manifold>(h);  // Manifold shares ownership of h

Or for read-only access:
  shared_ptr<const Helix> h_view = h;  // Const view without transfer

The key insight: smart pointers encode OWNERSHIP INTENT.
- unique_ptr: "this is the only reference"
- shared_ptr: "multiple references, last one cleans up"
- raw pointer: "I'm looking at this, but I don't own it"

This is critical for Phase 10 — geometric operations on helices
and manifolds should use smart pointers to make ownership explicit.
*/

void demonstrateOwnershipPattern() {
    cout << "=== Ownership Pattern ===" << endl;

    // Scenario: Helix owns trajectory data, Manifold analyzes it
    struct TrajectoryData {
        vector<double> x, y;
    };

    // Raw approach — unclear ownership
    // Helix* helix = new Helix(data);
    // Manifold* manifold = new Manifold(helix);
    // // Who deletes what? When?

    // Smart pointer approach — ownership explicit
    auto trajectory = make_shared<TrajectoryData>();
    trajectory->x = {1.0, 2.0, 3.0, 4.0};
    trajectory->y = {0.5, 1.5, 2.5, 3.5};

    // Pass as raw pointer — "I'm borrowing, not owning"
    // void analyzeTrajectory(TrajectoryData* data) { ... }
    // Pass as shared_ptr — "I'm using this too"
    // void analyzeTrajectory(shared_ptr<TrajectoryData> data) { ... }

    cout << "Trajectory owns data: " << trajectory.use_count() << " references" << endl;

    // Pass to another component that SHARES ownership
    auto trajectory2 = trajectory;  // Now 2 references
    cout << "After shared copy: " << trajectory.use_count() << " references" << endl;

    cout << endl;
    cout << "Ownership rules:" << endl;
    cout << "  unique_ptr<T>: exclusive owner, automatic delete" << endl;
    cout << "  shared_ptr<T>: shared owner, ref-counted delete" << endl;
    cout << "  T* (raw): borrowing, no ownership implied" << endl;
    cout << "  const T* (raw): read-only borrow" << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: Smart Pointers" << endl;
    cout << "====================================" << endl;
    cout << endl;

    demonstrateRawPointerDanger();
    demonstrateUniquePtr();
    demonstrateSharedPtr();
    demonstratePolymorphicSmartPointers();
    demonstrateOwnershipPattern();

    cout << "====================================" << endl;
    cout << "Key concepts:" << endl;
    cout << "  - make_unique<T>(args) — create unique_ptr" << endl;
    cout << "  - make_shared<T>(args) — create shared_ptr" << endl;
    cout << "  - unique_ptr: exclusive ownership, move to transfer" << endl;
    cout << "  - shared_ptr: reference-counted, copy to share" << endl;
    cout << "  - use_count() — number of shared references" << endl;
    cout << "  - move() — transfer unique_ptr ownership" << endl;
    cout << "  - get() — get raw pointer from smart pointer" << endl;
    cout << "  - Custom deleters for resources (FILE*, etc.)" << endl;
    cout << "  - Smart pointers + polymorphism (vector<unique_ptr<T>>)" << endl;

    return 0;
}
