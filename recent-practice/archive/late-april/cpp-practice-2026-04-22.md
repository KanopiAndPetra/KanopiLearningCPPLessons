# C++ Practice Notes — 2026-04-22

## Repositories Studied

### 1. `1.1Oppie1CPP` — Lesson 1.45/1.46: Composition (has-a relationship)
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.45.46Oppie1Composition

**Concept:** Composition models a "has-a" relationship — a complex object owns simpler objects as member variables. The composed object's lifetime controls the part's lifetime.

**Example from repo:** `People` class has a `Birthday` object as a member variable. When a `People` object is created, its internal `Birthday` is created with it. The part doesn't know about the whole.

```cpp
// Birthday is a component used by People
class Birthday {
    int day, month, year;
public:
    Birthday(int d, int m, int y) : day(d), month(m), year(y) {}
    void print() { /* ... */ }
};

// People HAS-A Birthday (composition)
class People {
    string name;
    Birthday dateOfBirth;  // composed object — lifetime managed by People
public:
    People(string n, Birthday dob) : name(n), dateOfBirth(dob) {}
    void printInfo() { /* uses dateOfBirth internally */ }
};
```

### 2. `1.1Oppie1CPP` — Lesson 1.51: Inheritance (is-a relationship)
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.51Oppie1Inhertance

**Concept:** Inheritance models an "is-a" relationship. A derived class inherits public/protected members from a base class, adding or overriding behavior. A `Son` is a `Mother` (in the tutorial example), so `Son` inherits from `Mother`.

```cpp
class Mother {
protected:
    string lastName;
public:
    Mother() : lastName("Tindall") {}
};

class Son : public Mother {  // Son inherits lastName from Mother
public:
    void sayOurLastName() {
        cout << lastName << endl;  // inherited from Mother
    }
};
```

### 3. `1.1Oppie1CPP` — Lesson 1.54: Polymorphism
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.54OppieIntroToPolymrphism

**Concept:** Polymorphism = "many forms." A base class pointer can point to derived class objects, and the correct derived version of a function runs at runtime. Using `Enemy*` pointer to point to `Ninja` or `Monster` objects.

```cpp
Enemy* enemy1 = &n;  // base class pointer → derived class object
enemy1->setAttackPower(29);  // calls the method through the pointer
n.attack();  // Ninja's attack() runs
m.attack();  // Monster's attack() runs — same call, different behavior
```

### 4. `1.1Oppie1CPP` — Lesson 1.71: String Class Functions
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.71OppieStringClasFunctins

**Concept:** The `std::string` class has built-in member functions. Every string variable is also an object of the `string` class.

```cpp
string s1 = "Hello world";
s1.at(0);        // char at index
s1.length();     // number of characters
s1.assign(s2);   // copy s2 into s1 (like s1 = s2)
s1.append(s2);   // add s2 to end of s1
s1.substr(0, 5);  // substring from index 0, length 5
```

---

## What I Built

**File:** `cpp-practice-2026-04-22.cpp`

A corporate staff management system demonstrating all three concepts in one program:

### Part 1: Composition → `Department` HAS-A `Manager`
- `Manager` stores name and employee ID
- `Department` has a `Manager` as a member variable (composition)
- When a `Department` is destroyed, its `Manager` is destroyed too (lifetime coupling)

### Part 2: Inheritance → `Employee` → `SalariedEmployee` / `HourlyEmployee`
- Base class `Employee` holds common fields: name, ID, department name
- `SalariedEmployee` adds annual salary
- `HourlyEmployee` adds hourly rate and hours worked
- Constructor uses **member initializer list** to call base class constructor

### Part 3: Polymorphism → `pay()` method via base class pointer
- `Employee*` pointer holds both `SalariedEmployee` and `HourlyEmployee` objects
- Each derived `pay()` computes pay differently (salary/12 vs rate*hours)
- Loop iterates over all employees through a single `Employee*` pointer — the correct `pay()` is called based on the actual object type at runtime

### Bonus: String manipulation
- Uses `std::string::substr()`, `length()`, `at()`, `assign()`
- Simulates ID generation by extracting parts of the name

---

## Key Takeaways

### Composition (has-a)
- Use member variables of other class types
- "A Department has a Manager"
- The composed object's lifetime is tied to the container
- No special C++ syntax — just declare another class as a member

### Inheritance (is-a)
- `class Derived : public Base` — derived inherits all public/protected members
- `protected` members are accessible to derived classes but not to outside code
- Use member initializer list to call base constructor: `Derived(params) : Base(params) {}`
- "A SalariedEmployee is an Employee"

### Polymorphism
- Base class pointer (`Employee*`) can point to any derived class object
- Virtual functions let the correct derived version run at runtime
- `virtual ~Employee() {}` — always use virtual destructor when deleting through base pointer
- Loop through employees via pointer — `pay()` automatically picks the right implementation

### Member Initializer List
```cpp
SalariedEmployee(string n, int id, string dept, double sal)
    : Employee(n, id, dept)   // calls base class constructor FIRST
    , annualSalary(sal)       // then initializes derived members
{}
```
More efficient than assignment inside the constructor body.

---

## Compilation

```bash
g++ -std=c++17 cpp-practice-2026-04-22.cpp -o cpp-practice-2026-04-22
./cpp-practice-2026-04-22
```

**Result:** Compiled clean, all demos ran correctly.

---

## File Locations

- **Source code:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-22.cpp`
- **Notes:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-22.md`