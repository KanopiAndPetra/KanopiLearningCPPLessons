# C++ Practice Notes — 2026-04-16

## Repos Studied

### 1. `KanopiC++Learning/Lessons_11-20/` (local copy of Oppie1's tutorials)
**Topic:** Intro to Classes and Objects, Variables in Classes, Constructors

Reviewed these lesson files to understand the structure and teaching approach:
- `1.11OppieIntroClasNobjsJNts.cpp` — Classes as templates, objects as instances, calling member functions with `.` operator, string member variables
- `1.12Oppie1VarblsInClsesGetNSetJNtes.cpp` — Getters and setters for private member variables
- `1.13.1OppieCnstructrsJNtes.cpp` — Constructors with multiple parameters, initializing all members at once

### 2. `1.1Oppie1CPP` repo (GitHub)
**Topic:** Recent practice files from the full C++ tutorial sequence

Reviewed the broader structure of Adam's learning repo — covers classes, pointers, file I/O, polymorphism, and more across 70+ tutorials.

---

## What I Built Today

**Program:** `cpp-practice-2026-04-16.cpp` — Vec2D Operator Overloading + Function Templates

A self-contained C++ program that practices two advanced topics:

### Topic 1: Operator Overloading
Classes can redefine how C++ operators work with their objects. Instead of writing `addVectors(a, b)`, you write `a + b`. The compiler calls your `operator+` function behind the scenes.

**Operators overloaded in Vec2D class:**
- `operator+` — adds two vectors component-wise
- `operator-` — subtracts two vectors component-wise
- `operator*` — scalar multiplication (scales vector by a number)
- `operator-` (unary) — negation, flips both components
- `operator==` — equality check (both components must match)
- `operator!=` — inequality (opposite of ==)
- `operator+=` — compound assignment (add and assign in one step)
- `operator<<` — stream output (lets you `cout << v`)
- `operator>>` — stream input (lets you `cin >> v`)

Key insight: Stream operators (`<<`, `>>`) must be **friend functions** (not member functions) so they can be called as `cout << v` rather than `v << cout`.

### Topic 2: Function Templates
Write ONE function that works for MANY types. The compiler generates the specific code for each type at compile time.

```cpp
template<typename T>
T larger(const T& a, const T& b) {
    return (a > b) ? a : b;
}
```

Works for `int`, `double`, `string`, and any type with a `>` operator. Compiler generates three versions under the hood.

---

## Key Takeaways

- **Operator overloading** makes your custom types feel native — `v1 + v2` is cleaner than `addVectors(v1, v2)`
- The `friend` keyword lets non-member functions access private members (needed for `operator<<` and `operator>>` as standalone functions)
- Templates use `<typename T>` or `<class T>` to declare a generic type parameter
- Templates are resolved at **compile time** — no runtime overhead vs writing separate functions
- `operator+=` should return `*this` (a reference to the modified object) to allow chaining

---

## File Location

- **Code:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-16.cpp`
- **Compiled:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-16`

---

## Previous Practice Files (for reference)

- `cpp-practice-2026-04-15.cpp` — std::vector operations + multi-file classes (Student roster)
- `cpp-practice-2026-04-14.cpp` — Pointers, string manipulation, constructors/destructors
- `cpp-practice-2026-04-13.cpp` — Composition ("has-a" relationships) + Friend functions
- `cpp-practice-2026-04-08.cpp` — File I/O with fstream
- `cpp-practice-2026-04-07.cpp` — More on virtual functions
- `cpp-practice-2026-04-06.cpp` — File I/O + polymorphism
- `cpp-practice-2026-04-05.cpp` — Polymorphism and virtual functions

---

*Practice session completed successfully. Program compiles and runs correctly.*