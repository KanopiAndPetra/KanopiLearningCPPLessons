// cpp-practice-2026-05-30.cpp
// Topic: Inheritance and Polymorphism
//
// Covers:
//   - Base & derived classes, access specifiers
//   - Protected members
//   - Base-class constructors and initialization lists
//   - Method overriding and polymorphism with virtual
//   - Pure virtual functions and abstract base classes
//   - Object slicing and why we need pointers/references
//   - Override specifier
//   - Virtual destructors
//   - Multiple inheritance

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cmath>

using namespace std;

// ============================================================
// BASE CLASS: Animal (not abstract — needed for slicing demo)
// ============================================================
class Animal {
protected:
    string name;
    int age;

public:
    // Virtual with default — not pure-virtual, so we can instantiate Animal
    // (for the slicing demo). Shape hierarchy below shows abstract classes.
    virtual void speak() const {
        cout << name << " makes a generic animal sound.";
    }

    // Virtual destructor is CRITICAL when using polymorphism
    // Without it, deleting a Derived* through Animal* won't call Derived's destructor
    virtual ~Animal() {
        // cout << "Animal destructor: " << name << endl;
    }

    // Non-pure virtual with default implementation
    virtual void display() const {
        cout << "Animal: " << name << ", age " << age;
    }

    // Getter
    string getName() const { return name; }
    int getAge() const { return age; }
};

// ============================================================
// DERIVED: Dog
// ============================================================
class Dog : public Animal {
private:
    string breed;

public:
    Dog(const string& n, int a, const string& b)
        : breed(b) {
        name = n;
        age = a;
    }

    void speak() const override {
        cout << name << " the " << breed << " says: WOOF!";
    }

    void display() const override {
        Animal::display();
        cout << ", breed: " << breed;
    }

    // Dog-specific method
    void fetch() const {
        cout << name << " fetches the ball!" << endl;
    }
};

// ============================================================
// DERIVED: Cat
// ============================================================
class Cat : public Animal {
private:
    bool indoor;

public:
    Cat(const string& n, int a, bool ind)
        : indoor(ind) {
        name = n;
        age = a;
    }

    void speak() const override {
        cout << name << " the " << (indoor ? "indoor" : "outdoor") << " cat says: MEOW!";
    }

    void display() const override {
        Animal::display();
        cout << ", indoor: " << (indoor ? "yes" : "no");
    }

    void purr() const {
        cout << name << " purrs contentedly..." << endl;
    }
};

// ============================================================
// DERIVED: Bird
// ============================================================
class Bird : public Animal {
private:
    double wingSpan;

public:
    Bird(const string& n, int a, double ws)
        : wingSpan(ws) {
        name = n;
        age = a;
    }

    void speak() const override {
        cout << name << " the bird chirps: CHIRP CHIRP!";
    }

    void display() const override {
        Animal::display();
        cout << ", wing span: " << wingSpan << "m";
    }

    void fly() const {
        cout << name << " soars through the air!" << endl;
    }
};

// ============================================================
// ABSTRACT BASE CLASS: Shape (polymorphic geometry)
// ============================================================
class Shape {
protected:
    string color;

public:
    Shape(const string& c = "white") : color(c) {}

    // Pure virtual — makes Shape abstract
    virtual double area() const = 0;
    virtual double perimeter() const = 0;

    virtual ~Shape() {}

    virtual void describe() const {
        cout << "A " << color << " shape";
    }
};

// ============================================================
class Circle : public Shape {
private:
    double radius;

public:
    Circle(double r, const string& c = "red") : radius(r), Shape(c) {}

    double area() const override {
        return M_PI * radius * radius;
    }

    double perimeter() const override {
        return 2 * M_PI * radius;
    }

    void describe() const override {
        Shape::describe();
        cout << " (circle) with radius " << radius;
    }

    double getRadius() const { return radius; }
};

// ============================================================
class Rectangle : public Shape {
private:
    double width;
    double height;

public:
    Rectangle(double w, double h, const string& c = "blue")
        : width(w), height(h), Shape(c) {}

    double area() const override {
        return width * height;
    }

    double perimeter() const override {
        return 2 * (width + height);
    }

    void describe() const override {
        Shape::describe();
        cout << " (rectangle) " << width << "x" << height;
    }

    double getWidth()  const { return width;  }
    double getHeight() const { return height; }
};

// ============================================================
class Triangle : public Shape {
private:
    double a, b, c;

public:
    Triangle(double sideA, double sideB, double sideC, const string& col = "green")
        : a(sideA), b(sideB), c(sideC), Shape(col) {
    }

    double area() const override {
        double s = (a + b + c) / 2.0;
        return sqrt(s * (s - a) * (s - b) * (s - c));
    }

    double perimeter() const override {
        return a + b + c;
    }

    void describe() const override {
        Shape::describe();
        cout << " (triangle) sides " << a << ", " << b << ", " << c;
    }
};

// ============================================================
// MULTIPLE INHERITANCE: Pet (from Animal and Showable)
// ============================================================
class Showable {
public:
    virtual void show() const = 0;
    virtual ~Showable() {}
};

class Pet : public Animal, public Showable {
private:
    string species;

public:
    Pet(const string& n, int a, const string& sp)
        : species(sp) {
        name = n;
        age = a;
    }

    void speak() const override {
        cout << name << " the " << species << " makes a pet sound!";
    }

    void show() const override {
        cout << "[Pet Show] " << name << " (" << species << "), age " << age;
    }
};

// ============================================================
// INHERITANCE WITH access specifiers
// ============================================================
class Vehicle {
protected:
    string brand;
    int year;

public:
    Vehicle(const string& b, int y) : brand(b), year(y) {}

    virtual void start() const {
        cout << brand << " vehicle starting..." << endl;
    }

    virtual void stop() const {
        cout << brand << " vehicle stopping." << endl;
    }

    string getBrand() const { return brand; }
    int getYear() const { return year; }
};

class Car : public Vehicle {
private:
    int doors;

public:
    Car(const string& b, int y, int d)
        : doors(d), Vehicle(b, y) {}

    void start() const override {
        cout << brand << " car (with " << doors << " doors) vroom vroom!" << endl;
    }

    int getDoors() const { return doors; }
};

// private inheritance: Vehicle's public methods become private in Motorbike
class Motorbike : private Vehicle {
public:
    Motorbike(const string& b, int y) : Vehicle(b, y) {}

    void start() const override {
        cout << brand << " motorbike: BRRRRMM!" << endl;
    }

    void wheelie() const {
        cout << brand << " motorbike does a wheelie!" << endl;
    }
};

// ============================================================
// DEMONSTRATION DRIVER
// ============================================================
int main() {
    cout << "==============================================" << endl;
    cout << "   INHERITANCE & POLYMORPHISM PRACTICE" << endl;
    cout << "==============================================" << endl;
    cout << endl;

    // ------------------------------------------------
    // 1. Basic inheritance and overriding
    // ------------------------------------------------
    cout << "--- 1. Basic Inheritance ---" << endl;

    Dog d("Buddy", 3, "Golden Retriever");
    Cat c("Whiskers", 5, true);
    Bird b("Tweety", 2, 0.3);

    d.speak();  cout << endl;
    c.speak();  cout << endl;
    b.speak();  cout << endl;
    cout << endl;

    // ------------------------------------------------
    // 2. Polymorphism via base-class pointers
    // ------------------------------------------------
    cout << "--- 2. Polymorphism via Base-Class Pointers ---" << endl;

    Animal* animals[3];
    animals[0] = &d;
    animals[1] = &c;
    animals[2] = &b;

    for (int i = 0; i < 3; ++i) {
        animals[i]->speak();
        cout << " (via Animal pointer)" << endl;
    }
    cout << endl;

    // ------------------------------------------------
    // 3. Slicing — what happens WITHOUT pointers
    // ------------------------------------------------
    cout << "--- 3. Slicing (Object Slicing) ---" << endl;

    Dog dog2("Rex", 4, "German Shepherd");
    Animal animal2 = dog2;  // SLICING! Copies only the Animal part
    cout << "Via Animal copy (sliced): "; animal2.speak(); cout << endl;
    cout << "  (Lost Dog-specific speak — sliced!)" << endl;
    cout << endl;

    // ------------------------------------------------
    // 4. Virtual functions — dynamic dispatch
    // ------------------------------------------------
    cout << "--- 4. Virtual Functions and Dynamic Dispatch ---" << endl;

    cout << "Using pointer (virtual):  ";
    Animal* pDog = &dog2;
    pDog->speak(); cout << endl;

    cout << "Using reference (virtual): ";
    Animal& rDog = dog2;
    rDog.speak(); cout << endl;
    cout << endl;

    // ------------------------------------------------
    // 5. Override specifier
    // ------------------------------------------------
    cout << "--- 5. Override Specifier ---" << endl;
    cout << "After speak(), override means:" << endl;
    cout << "  'I claim to be overriding a base method.'" << endl;
    cout << "  If no matching base method → compile error!" << endl;
    cout << "  e.g. 'void spak()' override would fail to compile." << endl;
    cout << endl;

    // ------------------------------------------------
    // 6. Abstract base classes — Shape hierarchy
    // ------------------------------------------------
    cout << "--- 6. Abstract Base Classes (Shape hierarchy) ---" << endl;

    Circle circ(5.0, "red");
    Rectangle rect(4.0, 3.0, "blue");
    Triangle tri(3.0, 4.0, 5.0, "green");

    Shape* shapes[] = { &circ, &rect, &tri };

    for (int i = 0; i < 3; ++i) {
        shapes[i]->describe();
        cout << " | Area: " << shapes[i]->area()
             << " | Perimeter: " << shapes[i]->perimeter() << endl;
    }
    cout << endl;

    // ------------------------------------------------
    // 7. Virtual destructors
    // ------------------------------------------------
    cout << "--- 7. Virtual Destructors ---" << endl;
    cout << "Animal base has virtual destructor." << endl;
    cout << "Dog, Cat, Bird destructors run in proper order" << endl;
    cout << "when deleting through Animal*." << endl;
    cout << "Without 'virtual ~Animal()', only Animal destructor runs!" << endl;
    cout << endl;

    // ------------------------------------------------
    // 8. Smart pointers for polymorphic ownership
    // ------------------------------------------------
    cout << "--- 8. Smart Pointers (unique_ptr) ---" << endl;

    unique_ptr<Animal> pet1 = make_unique<Dog>("Max", 2, "Beagle");
    unique_ptr<Animal> pet2 = make_unique<Cat>("Luna", 4, true);

    cout << "unique_ptr<Animal>: ";
    pet1->speak(); cout << endl;
    cout << "unique_ptr<Animal>: ";
    pet2->speak(); cout << endl;
    cout << "(unique_ptr auto-deletes — no memory leak!)" << endl;
    cout << endl;

    cout << "Vector of polymorphic objects:" << endl;
    vector<unique_ptr<Animal>> zoo;
    zoo.push_back(make_unique<Dog>("Spots", 1, "Dalmatian"));
    zoo.push_back(make_unique<Cat>("Shadow", 6, false));
    zoo.push_back(make_unique<Bird>("Polly", 10, 0.5));

    for (const auto& pet : zoo) {
        cout << "  "; pet->speak(); cout << endl;
    }
    cout << endl;

    // ------------------------------------------------
    // 9. Multiple inheritance
    // ------------------------------------------------
    cout << "--- 9. Multiple Inheritance ---" << endl;

    Pet myPet("Fido", 2, "hamster");
    myPet.speak(); cout << endl;
    myPet.show();  cout << endl;
    cout << endl;

    // ------------------------------------------------
    // 10. Protected access and inheritance types
    // ------------------------------------------------
    cout << "--- 10. Inheritance Access Specifiers ---" << endl;
    cout << "public inheritance:   IS-A relationship" << endl;
    cout << "  Dog : public Animal  → Dog is-a Animal" << endl;
    cout << "  Car : public Vehicle → Car is-a Vehicle" << endl;
    cout << endl;
    cout << "protected inheritance: Vehicle's public methods" << endl;
    cout << "  become protected in Motorbike" << endl;
    cout << endl;
    cout << "private inheritance:  HAS-IMPLEMENTED-IN terms" << endl;
    cout << "  Motorbike uses Vehicle internally but" << endl;
    cout << "  outside world can't treat Motorbike as Vehicle" << endl;
    cout << endl;

    Car car("Toyota", 2022, 4);
    car.start();
    cout << "  Car brand via getter: " << car.getBrand() << endl;

    Motorbike bike("Harley", 2021);
    bike.start();
    bike.wheelie();
    // bike.getBrand() — ERROR: private inheritance hides it
    cout << endl;

    // ------------------------------------------------
    // 11. Polymorphic collection with raw pointers
    // ------------------------------------------------
    cout << "--- 11. Polymorphic Collections ---" << endl;
    cout << "Mixing Dog, Cat, Bird in one container:" << endl;

    vector<Animal*> menagerie;
    menagerie.push_back(new Dog("Clifford", 5, "Big Red"));
    menagerie.push_back(new Cat("Felix", 3, false));
    menagerie.push_back(new Bird("Kiwi", 1, 0.1));
    menagerie.push_back(new Dog("Snoopy", 7, "Beagle"));
    menagerie.push_back(new Cat("Garfield", 4, true));

    for (size_t i = 0; i < menagerie.size(); ++i) {
        cout << "  " << (i+1) << ". ";
        menagerie[i]->speak(); cout << endl;
    }

    for (auto p : menagerie) delete p;
    cout << "(Deleted each via Animal* → virtual destructor called)" << endl;
    cout << endl;

    // ------------------------------------------------
    // 12. Base-class initialization
    // ------------------------------------------------
    cout << "--- 12. Base-Class Initialization ---" << endl;
    cout << "Car(string brand, int year, int doors)" << endl;
    cout << "  : Vehicle(brand, year), doors(doors) {}" << endl;
    cout << "  → Vehicle constructor called with brand & year" << endl;
    cout << "  → Car sets doors separately" << endl;
    cout << endl;

    cout << "==============================================" << endl;
    cout << "   PRACTICE COMPLETE" << endl;
    cout << "==============================================" << endl;

    return 0;
}