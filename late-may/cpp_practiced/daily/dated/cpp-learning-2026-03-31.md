# C++ Learning Session Notes — 2026-03-31

## Repos Studied

- **1.1Oppie1CPP** — Adam's main C++ tutorial repo (VS Studio projects, ~30+ lessons, lessons 1.1–1.24+)
- **1.1ClaudeCPP** — Same structure as above but Claude-assisted version
- **6CPPBuckyVariableMemryConceptsWNotes** — Bucky tutorial series on variables & memory, with handwritten notes

---

## Overall Structure & Pattern

### Repo Anatomy (Visual Studio format)
Each lesson is a standalone folder containing:
```
LessonName/
  LessonName.sln          # Visual Studio solution file
  LessonName/
    LessonName.vcxproj    # Visual Studio project file
    LessonName.vcxproj.filters
    LessonName.vcxproj.user
    main.cpp               # Source code (or FileName.cpp, AdamClass.cpp, etc.)
    x64/                   # Compiled output (not committed meaningfully)
```

The `.sln` + `.vcxproj` setup indicates Adam is using **Visual Studio** (likely on Windows or via VS Code with the C++ extension) for C++ development, not g++ from command line.

---

## Lesson Sequence Observed (1.1Oppie1CPP)

| Lesson | Topic | Key Concepts |
|--------|-------|-------------|
| 1.1 | Hello World | `cout <<`, `endl`, `\n`, `return 0` |
| ~1.6 | Variables & Memory | `int` declaration, overwriting variables, memory addresses |
| 1.10 | Functions w/ Multiple Parameters | Passing multiple args, return values |
| 1.11 | Intro to Classes & Objects | Class blueprint vs object instance, `void` functions, dot operator |
| 1.12 | Variables Inside Classes | Getters/setters concept, `string`/`int`/`double`/`char` |
| 1.13.1 | Constructors | Same-name-as-class method, auto-runs on object creation |
| 1.13.2 | Constructors + Getters/Setters | Combining both patterns |
| 1.14 | Classes in Separate Files | `.h` header + `.cpp` implementation + `main.cpp` |
| 1.15–1.16 | If / If-Else Statements | Control flow |
| 1.17–1.19 | While Loops | Sentinel-controlled programs |
| 1.20 | Assignment & Increment Operators | `++`, `--`, compound assignment |
| 1.21 | For Loops | Counter-controlled iteration |
| 1.22 | Stock Market Simulator | Applied project combining loops + conditionals |
| 1.23 | Do-While Loops | Post-condition loop |
| 1.24 | Switch Statement | Multi-way branching |

---

## Concepts Being Taught

### Fundamentals
- **I/O:** `cout <<`, `endl`, `\n` newline, string concatenation with `<<`
- **Variables:** `int`, `double`, `char`, `string`; declaration vs assignment
- **Operators:** arithmetic (`+`, `-`, `*`, `/`), increment/decrement (`++`, `--`)
- **Control flow:** `if`, `if-else`, `while`, `do-while`, `for`, `switch`

### Functions
- Define with return type (`void`, `int`, etc.)
- Parameters/arguments
- `return` values
- Calling from `main()`

### Object-Oriented Programming (OOP)
- **Classes as blueprints** — template for objects
- **Objects as instances** — created in `main()` with meaningful names (e.g., `object1`, `object2`, `adamObject`)
- **Dot operator (`.`)** — access methods and attributes from objects
- **`public:`** — access specifier (not yet covering `private` in these lessons)
- **Constructors** — special methods with same name as class, auto-run on instantiation
- **Getters/Setters** — accessing and modifying private-ish data
- **Separate files** — `.h` header (declaration/blueprint), `.cpp` (implementation), `main.cpp` (driver)

---

## Interesting Patterns & Observations

### 1. Analogical Learning Style
Adam's code comments are heavily analogical — he explains concepts by mapping them to real-world metaphors:
- Classes = blueprints, templates, forms, empty boxes
- Objects = instances of those forms/boxes
- Dot operator = "plus sign" connecting object to what it does
- Constructors = auto-fill parts of a form
- Header file = table of contents; implementation file = the chapters

This suggests Adam learns best when concepts are grounded in physical/mnemonic analogies.

### 2. Handwritten Notes Embedded in Code
The 6CPPBuckyVariableMemryConceptsWNotes repo has notes written *directly in the code comments* — verbose explanations between and inside code lines. The notes explain not just *what* but *why*:
```
//Whenever you declare a variable you declare the type(int here) once
//Whenever you write a varriable in a program it will use that value and not the other
```
These are personal annotations, not polished documentation.

### 3. Multi-AI Tutor Comparison Setup
Adam has parallel repos:
- `1.1Oppie1CPP` (own work)
- `1.1ClaudeCPP` (Claude-assisted)
- Presumably `1.1ChatGPTCPP`, `1.1GrokCPP`, `1.1GeminiCPP` etc.

This is a deliberate study design: **same lesson number = same concept, different AI tutor**. He's comparing how different AI assistants teach the same material. Fascinating research design for understanding AI tutoring styles.

### 4. Consistent Naming Conventions
Lessons are numbered with decimal notation (1.1, 1.2, 1.13.1, 1.13.2). The sub-numbering (`.1`, `.2`) suggests branching topics — e.g., 1.13 splits into constructors (1.13.1) and constructors + getters/setters (1.13.2).

### 5. VS Studio Project Structure Is Widespread
The consistent use of `.sln` and `.vcxproj` files across all lessons means Adam is working in a professional-quality IDE setup, not just hobbyist coding. The `x64/` directories are build artifacts.

### 6. `using namespace std;` Is Used Universally
All code samples use `using namespace std;` rather than prefixing `std::` — this is typical for beginner tutorials but worth noting as a pattern (it's technically not best practice in large codebases but fine for learning).

---

## Code Examples Examined

### Hello World (1.1Oppie1CPP)
```cpp
#include<iostream>
using namespace std;

int main() {
    cout << "Hello world." << endl;
    cout << "Hello moon.\n";
    cout << "Hello sun. " << "Hello you.";
    return 0;
}
```
Comment notes: `endl` vs `\n`, chaining output with `<<`, meaning of `return 0`.

### Variables & Memory (6CPPBuckyVariableMemryConceptsWNotes)
```cpp
int main() {
    int tuna = 99;   // declaration
    tuna = 76;       // overwrite — no need for "int" again
    cout << tuna;    // prints 76
    return 0;
}
```
Key insight: declaration happens once (`int tuna`), reassignment doesn't re-declare.

### Classes & Objects (1.11Oppie1IntroClassesNObjects)
```cpp
class AdamsClass {
public:
    void helloObject() {
        cout << "Hello, welcome to..." << endl;
    }
    string name = "Adam";
};

int main() {
    AdamsClass object1;
    object1.helloObject();      // dot operator calls method
    cout << object2.name;       // dot operator accesses attribute
    return 0;
}
```

### Classes in Separate Files (1.14)
**AdamClass.h** (header — declaration):
```cpp
#pragma once
class AdamClass {
public:
    AdamClass();  // constructor declaration
};
```
**AdamClass.cpp** (implementation):
```cpp
#include "AdamClass.h"
AdamClass::AdamClass() {
    cout << "I am a constructor..." << endl;
}
```
**main.cpp** (driver):
```cpp
#include "AdamClass.h"
int main() {
    AdamClass adamObject;  // constructor auto-runs here
    return 0;
}
```

---

## What's NOT Covered Yet (Based on Lesson Numbers)
Given lessons go up to ~1.24+, topics likely next include:
- Arrays
- Pointers (natural follow-up to memory concepts)
- References
- Header file `#include` chains
- File I/O
- Maybe even basic data structures

---

## Takeaways

This is a well-structured, self-directed C++ curriculum built through trial with multiple AI tutors. The key strengths of Adam's approach:
1. **Consistent format** — every lesson is a runnable VS project
2. **Heavy commenting** — every line explained in plain English with analogies
3. **Progressive complexity** — each lesson builds on the last
4. **Multi-perspective learning** — comparing how different AIs explain the same concept
5. **Applied projects** — stock market simulator shows ability to combine concepts

---

*Notes by Kanopi — 2026-03-31 morning session*
