# C++ Practice Notes - May 30, 2026

## Session Info
- **Time:** 3:00 PM CDT
- **Topic:** Inheritance and Polymorphism

## Repos Studied
- Reviewed May 23 (Classes + OOP) and May 29 (Operator Overloading) notes
- Chose inheritance/polymorphism as the next logical step

## Key Concepts Learned

### What is Inheritance?
- **"is-a" relationship** — a Dog is an Animal
- Derived class inherits members from base class
- Can add its own data and override methods

### Access Specifiers in Inheritance
```
public inheritance:    Dog : public Animal → Dog IS-A Animal
protected inheritance: Vehicle's public → protected in Motorbike
private inheritance:   Vehicle's everything → private in Motorbike
```

### Protected Members
- `protected:` — accessible to derived classes, hidden from outside
- `private:` — only the class itself can access
- Used in base classes when derived classes need access but outside world shouldn't

### Base-Class Initialization (Initialization Lists)
```cpp
Car::Car(string b, int y, int d)
    : Vehicle(b, y), doors(d) { }
    // ↑ calls Vehicle's constructor first
    // then Car initializes doors
```

### Virtual Functions — The Key to Polymorphism
- `virtual` keyword enables **dynamic dispatch** at runtime
- Without `virtual`: which method runs is determined by the **pointer type**
- With `virtual`: which method runs is determined by the **object type**
- `override` keyword catches mistakes at compile time

### Slicing — Why Polymorphism Needs Pointers/References
```cpp
Dog dog2("Rex", 4, "German Shepherd");
Animal animal2 = dog2;  // SLICES — only copies Animal part!
// animal2.speak() calls Animal::speak(), not Dog::speak()!
```
Without pointers/references, you always get the base-class version.

### Abstract Base Classes and Pure Virtual Functions
```cpp
class Shape {
    virtual double area() const = 0; // pure virtual — no default
    // Shape is abstract → can't be instantiated
};
```
Abstract classes define interfaces; derived classes must implement the pure virtuals.

### Virtual Destructors — Critical for Cleanup
```cpp
virtual ~Animal() { } // MUST be virtual when using polymorphism
```
Without it, `delete animalPtr` only calls Animal's destructor, skipping Derived's destructor.

### Multiple Inheritance
```cpp
class Pet : public Animal, public Showable { ... };
```
A Pet is both an Animal AND a Showable. Works fine when base classes are independent.

### Smart Pointers for Polymorphic Ownership
```cpp
unique_ptr<Animal> pet1 = make_unique<Dog>("Max", 2, "Beagle");
```
- `unique_ptr` owns one object, auto-deletes when it goes out of scope
- No manual `delete` needed — prevents memory leaks
- `make_unique<T>()` is the safe way to create them

## Program Created

**File:** `cpp-practice-2026-05-30.cpp`

**What it does:**
1. **Animal base class** (non-abstract) — with virtual speak(), display(), protected name/age
2. **Dog, Cat, Bird** — three derived classes each overriding speak()
3. **Shape abstract base class** — with pure virtual area() and perimeter()
4. **Circle, Rectangle, Triangle** — concrete Shape implementations
5. **Showable interface** — multiple inheritance demo
6. **Pet** — inherits from BOTH Animal and Showable
7. **Vehicle/Car/Motorbike** — demonstrates public vs private inheritance

**Demos:**
- Basic inheritance + method overriding
- Polymorphism via `Animal*` pointer array — dynamic dispatch
- Slicing (object copy loses derived part)
- Virtual destructors (explained, not shown)
- `unique_ptr<Animal>` for safe polymorphic ownership
- Multiple inheritance (Pet from Animal + Showable)
- Access specifiers (public, protected, private inheritance)
- Vector of polymorphic objects with manual cleanup

**Output highlights:**
```
Via Animal copy (sliced): Rex makes a generic animal sound.
  (Lost Dog-specific speak — sliced!)
Using pointer (virtual):  Rex the German Shepherd says: WOOF!
A red shape (circle) with radius 5 | Area: 78.5398 | Perimeter: 31.4159
Fido the hamster makes a pet sound!
[Pet Show] Fido (hamster), age 2
```

## What Was Tricky
- **Slicing vs polymorphism** — initially made Animal abstract (pure virtual speak) so couldn't show slicing. Had to change speak() to `virtual void speak() const { ... }` with default implementation to allow `Animal animal2 = dog2`.
- **Initialization lists** — getting the syntax right: `: Vehicle(b, y), doors(d)` vs trying to set them inside the constructor body.
- **override vs virtual** — override is optional but catches typos at compile time. If you write `void spak() const override` it won't compile because there's no matching base method.

## Next Steps / Areas to Explore
- **Virtual functions and abstract base classes** — DONE! (covered)
- **Smart pointers** (unique_ptr, shared_ptr) — DONE! (intro)
- Static class members in depth
- Template basics
- Exception handling in C++
- File I/O

## GitHub
- **Repo:** https://github.com/KanopiAndPetra/KanopiLearningCPPLessons
- **To commit:** Yes — program is complete and working
- **Committed:** 2026-05-30