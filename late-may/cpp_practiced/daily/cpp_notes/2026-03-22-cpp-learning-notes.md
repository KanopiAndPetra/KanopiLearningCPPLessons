# C++ Learning Notes — 2026-03-22 Evening Session

## Repositories Studied

1. **`1.1ClaudeCPP`** — Large tutorial repo (~30 lessons), Visual Studio projects
2. **`6CPPBuckyVariableMemryConceptsWNotes`** — Single lesson on variables/memory, Bucky-style tutorial
3. **`1.1Oppie1CPP`** — Parallel repo to #1, same structure but Adam's own version

---

## Repo 1: `1.1ClaudeCPP` — Structure & Patterns

### Directory/Naming Convention
Each lesson gets a numbered subdirectory (e.g., `1.10ClaudeFuncWMultiplParmetrs`, `1.13.1Claude1Constructors`). The prefix matches the lesson number, followed by a short topic descriptor. Within each:
```
<LessonDir>/
  <LessonDir>.sln                          (Visual Studio solution file)
  <LessonDir>/
    <LessonDir>.vcxproj                   (VS project file)
    <LessonDir>.vcxproj.filters
    <LessonDir>.vcxproj.user
    main.cpp                              ← THE source file
    x64/                                  (build artifacts)
```

### Curriculum Progression
Starting from the top-level lesson names visible:
- **Basics**: Hello World, variables, simple output
- **Functions**: Single param → multiple params
- **OOP**: Intro to classes & objects → member variables → getters/setters → constructors → classes in separate files
- **Control flow**: If statements, if-else, while loops, sentinel-controlled programs
- **Loops**: for loops, do-while loops, simple programs using loops
- **Operators**: Assignment, increment/decrement
- **Applications**: Stock market simulation (compound project)
- **Switch statements** (in `1.1Oppie1CPP` parallel repo)

### C++ Concepts Being Taught

**1. Functions with Multiple Parameters**
```cpp
int addNumbers(int x, int y, int a, int b) {
    int answer = x + y + a + b;
    return answer;
}
// Key insight: function call = replaced by return value
// addNumbers(10,15,28,7) = 60
```

**2. Classes & Objects (OOP)**
```cpp
class OppieClass1 {
public:
    string name;
    int age;
    double gpa;
    void study() { cout << "Claude is studying" << endl; }
};
OppieClass1 oppieObject1("Claude", 33, 3.7);
cout << oppieObject1.name;
oppieObject1.study();
```

**3. Constructors** — Special functions that run automatically on object creation
```cpp
class CarClass {
public:
    int year;
    string make;
    string model;
    CarClass(int x, string y, string z) {
        year = x; make = y; model = z;
    }
    void brake() { cout << "Car hit the brakes!" << endl; }
};
CarClass carObject(2015, "Toyota", "Corolla");
```
Constructor benefits: 1) Creates object 2) Sets initial values — all in one step.

**4. For Loops** — Three-part syntax: init; condition; increment
```cpp
for (int x = 5; x < 30; x += 5) {
    // Pass 1: x=5  (5<30 true)  → print 5,  x=10
    // Pass 2: x=10 (10<30 true) → print 10, x=15
    // ...
    // Pass 6: x=30 (30<30 false) → EXIT loop
    cout << x << endl;
}
```

### Notable Patterns

**Inline commenting style**: Every line has a comment explaining what it does — very verbose, beginner-friendly. Comments are the primary teaching mechanism.
```cpp
cout << oppieObject1.name << endl;//Access member variables using dot notation
oppieObject1.study();//Call member function using dot notation
```

**`using namespace std;`** — Used in all beginner lessons (not ideal for production, but common in tutorials).

**Two-class pattern**: Many lessons show TWO classes back-to-back to reinforce that classes are reusable blueprints.

**Real-world analogies**: Car objects with year/make/model, Person objects with name/age/gpa.

**Key insight box**: Each lesson ends (or starts) with a "Key takeaway" or "Key Concept" summary block.

---

## Repo 2: `6CPPBuckyVariableMemryConceptsWNotes`

Follows Bucky (thenewboston) C++ tutorial naming. Single project, focused on one concept. VS project structure identical but simpler (no nested lesson dirs).

Note: The source file is named `FileName.cpp` (not `main.cpp`) — unusual, possibly a tutorial artifact.

---

## Patterns Across Both Repos

1. **Visual Studio as the IDE** — `.sln`, `.vcxproj`, `.vcxproj.filters`, `.vcxproj.user` files everywhere, x64 build folders. Adam is using Visual Studio on Windows.
2. **Verbose inline comments** — Every single line explained in comments. The code IS the notes.
3. **Consistent naming**: Classes end in "Class" (e.g., `OppieClass1`, `CarClass`), objects have descriptive names.
4. **No `#ifndef` header guards** in single-file lessons — everything in `main.cpp`.
5. **Heavy use of `iostream` and `string`** — typical beginner C++.
6. **`.gitignore`d build artifacts** — `x64/` folders are build output, not source.

---

## Questions / Things to Ponder

- The repos `1.1ClaudeCPP` and `1.1Oppie1CPP` have identical lesson sequences — was one used as a reference while building the other?
- The constructor lesson shows parameterized constructors but doesn't cover default constructors or initializer lists — natural next steps.
- No use of `const` methods, no mention of `this->` pointer, no access specifier variations (private by default) — all still in the beginner zone.
- The OOP section covers up to "classes in separate files" — next logical lessons would cover header files, `#include` chains, and the full `.h` + `.cpp` separation pattern.

---

## Suggested Next Lesson to Build
`1.25Oppie1ClassesInHeaderFiles` — Practice splitting a class declaration (`.h`) from its implementation (`.cpp`) and using `#include`.
