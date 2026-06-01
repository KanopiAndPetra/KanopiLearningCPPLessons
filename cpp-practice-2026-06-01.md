# C++ Practice Notes — June 1, 2026

## Session Info
- **Time:** 3:00 PM CDT (Monday)
- **Topic:** Inheritance & Polymorphism
- **Binary:** `cpp-practice-2026-06-01` ✅ compiled cleanly, ran successfully

## Key Concepts Learned

### 1. Inheritance Basics
```
Shape (base) ← Circle, Rectangle ← Square
```
- Derived class inherits members from base class
- `protected:` lets derived classes access members (not `private`)
- `override` keyword on overridden methods — compiler catches mistakes

### 2. Virtual Functions & Dynamic Dispatch
The core of polymorphism:
```cpp
Shape* shapes[] = { &c, &r, &s };
for (Shape* sh : shapes) {
    sh->describe();  // Calls DERIVED version, not Shape's!
}
```
- Without `virtual`: static binding — uses pointer type
- With `virtual`: dynamic binding — uses actual object type (vtable lookup)

### 3. Pure Virtual Functions & Abstract Classes
```cpp
virtual double area() const = 0;  // Must be overridden
virtual double perimeter() const = 0;
```
- Makes the class **abstract** — can't instantiate it
- Forces derived classes to provide implementations

### 4. Virtual Destructor — CRITICAL for polymorphism!
```cpp
virtual ~Shape() { ... }  // Always make base destructors virtual!
```
- Without `virtual`: only base destructor called
- With `virtual`: derived then base destructor called
- Without it, deleting through base pointer causes memory leak!

### 5. Slicing — The Danger
```cpp
Polygon p = Triangle(...);  // Sliced! Only Polygon part copied
p.area();  // WRONG! Calls Polygon::area(), not Triangle's
```
- Always use **pointers** or **references** for polymorphic behavior
- `unique_ptr<Base>` is the modern safe approach

### 6. `override` Keyword
```cpp
double area() const override { ... }
```
- Compiler verifies this actually overrides something
- Catches typos like `area()` vs `Area()` or wrong signature

### 7. `dynamic_cast` — Type Checking at Runtime
```cpp
Dog* d = dynamic_cast<Dog*>(genericAnimal);
if (d) { /* success */ } else { /* not a Dog */ }
```
- Returns `nullptr` if cast fails
- Useful for type-checking when you need specific behavior

### 8. `unique_ptr` for Polymorphic Cleanup
```cpp
vector<unique_ptr<Animal>> farm;
farm.push_back(make_unique<Dog>("Buddy", 3, "Golden"));
```
- Automatic cleanup when vector goes out of scope
- No raw `new`/`delete` needed

## Programs Created

### Shape Hierarchy
- `Shape` (abstract base) — area(), perimeter(), describe()
- `Circle : public Shape` — radius, πr²
- `Rectangle : public Shape` — width, height
- `Square : public Rectangle` — single side (shows inherited behavior)

### Animal Hierarchy
- `Animal` (abstract) — speak(), species(), getInfo()
- `Dog : public Animal` — with breed
- `Cat : public Animal` — with indoor flag
- `Cow : public Animal` — simple

### Demonstrations
- Virtual dispatch through base pointers
- Slicing (what goes wrong without pointers)
- Virtual destructor chain
- Reference polymorphism
- Factory pattern (`createAnimal`)
- `dynamic_cast` type checking

## Output
```
Iterating shapes through base pointer (virtual dispatch):
  Pizza (Circle, r=5) — Area: 78.5398, Perim: 31.4159
  Notebook — Area: 12, Perimeter: 14
  Tile (Square, side=4) — Area: 16

Slicing demo: Triangle assigned to Polygon (sliced): area=0 (should be 6, but slicing gave wrong vtable)

Virtual destructor: Derived destructor called first, then Base

Farm animals:
  Buddy (Canis familiaris), age 3, breed: Golden Retriever
  Says: "Woof! Woof!"
  Whiskers (Felis catus), age 7 [indoor]
  Says: "Meow..."
  Mabel (Bos taurus), age 5
  Says: "MooOOOOO!"

Dynamic cast: Cat cast to Dog: FAILED (as expected!)
```

## Next Steps
- Smart pointers (unique_ptr, shared_ptr) — more practice
- Templates and STL containers
- File I/O and serialization
- Exception handling