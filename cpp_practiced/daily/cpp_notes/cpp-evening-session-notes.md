# C++ Learning Notes — 2026-03-23 (Evening Session)

Repository study of Adam's C++ tutorial repos on GitHub.

---

## Repo 1: `1.1ClaudeCPP`
**URL:** https://github.com/Oppie1/1.1ClaudeCPP

### Structure
- **Top-level repo** acts as a course container
- Each lesson is a **numbered subdirectory** (1.1 → 1.23+)
- Inside each: a **Visual Studio solution** (.sln) with a project folder + x64 build artifacts
- All `.cpp` source files live inside the inner project folder
- Some later lessons (like classes) break the class definition into **separate .h and .cpp files**

### Lesson Progression Observed
```
1.1  HelloWorld
1.10 Functions with Multiple Parameters
1.11 Intro to Classes and Objects
1.12 Variables in Classes (get/set)
1.13.1 Constructors
1.13.2 Constructors with Get/Set
1.14 Classes in Separate Files
1.15 If Statement Again
1.16 If-Else Statement
1.17 While Loops
1.18 Simple Program Using Loop
1.19 Sentinel-Controlled Program
1.20 Assignment and Increment Operators
1.21 For Loops
1.22 Stock Market Simulation
1.23 Do-While Loops
```

### Key Concepts Studied

#### Functions with Multiple Parameters (Lesson 1.10)
```cpp
int addNumbers(int x, int y, int a, int b) {
    int answer = x + y + a + b;
    return answer;
}
```
- Functions can take **4+ parameters** as long as types match
- `return` sends a value back to the caller
- Call site: `addNumbers(10, 15, 28, 7)` → returns 60
- **Insight:** The function call expression is **replaced** by the returned value

#### Classes & Objects (Lesson 1.11)
```cpp
class AdamsClass {
public:
    void helloObject() {
        cout << "Hello, welcome..." << endl;
    }
    string name = "Adam";
};

AdamsClass object1;
object1.helloObject();       // calls member function
cout << object2.name;       // accesses member variable
```
- Class = **blueprint**; Object = **instance** created from blueprint
- `.` operator accesses members of an object
- Objects bundle **data (variables) + behavior (functions)**
- Key analogy: "Objects are modular building blocks"

#### Constructors (Lesson 1.13.1)
```cpp
class OppieClass1 {
public:
    string name;
    int age;
    double gpa;

    OppieClass1(string a, int b, double c) {
        name = a;
        age = b;
        gpa = c;
    }

    void study() { cout << "Claude is studying" << endl; }
};

// Usage:
OppieClass1 oppieObject1("Claude", 33, 3.7);
```
- Constructor = special function that **runs automatically** when object is created
- Same name as the class, no return type
- Allows **one-step object creation + initialization**
- Two benefits: (1) creates object, (2) sets initial values automatically
- Each object has **its own copy** of member variables

#### Classes in Separate Files (Lesson 1.14) — OOP File Organization
**AdamClass.h** (header — the "contract"):
```cpp
#pragma once

class AdamClass {
public:
    AdamClass();  // declaration only
};
```
**AdamClass.cpp** (implementation — the "chapters"):
```cpp
#include <iostream>
#include "AdamClass.h"

AdamClass::AdamClass() {
    cout << "Constructor executing!\n";
}
```
**main.cpp:**
```cpp
#include <iostream>
#include "AdamClass.h"

AdamClass adamObject;  // constructor runs automatically
```
- `#pragma once` = include guard (prevents double-loading)
- Header file: **declares** what the class has
- Implementation file: **defines** how it works
- `ClassName::methodName()` syntax = scope resolution operator
- Real-world C++ projects always use this 3-file pattern

---

## Repo 2: `74.ExtraCred.VectorsWithNotes`
**URL:** https://github.com/Oppie1/74.ExtraCred.VectorsWithNotes

### Focus: The `std::vector` Container
A vector is a **dynamic array** — its size can grow/shrink at runtime, unlike plain C++ arrays.

### Concepts Covered

#### 1. Declaration and Initialization
```cpp
vector<int> emptyVector(5);           // size 5, all zeros
vector<int> happyVector = {1, 2, 3};  // initialized with values
```

#### 2. Accessing Elements (like arrays)
```cpp
int elem = happyVector[2];   // index 2 = third element
happyVector2[0] = 6;        // modify via index
```

#### 3. `push_back()` — Add to end
```cpp
happyVector3.push_back(10);  // grows the vector by 1
```

#### 4. `pop_back()` — Remove from end
```cpp
happyVector4.push_back(10);
happyVector4.pop_back();     // removes last element
```

#### 5. `erase()` — Remove element by iterator
```cpp
happyVector5.erase(happyVector5.begin() + 1);
// Removes 2nd element (index 1)
```
- Uses **iterators** — `begin()` returns an iterator to first element
- `begin() + 1` = iterator to second element

#### 6. `insert()` — Insert element by iterator
```cpp
happyVector5.insert(happyVector5.begin() + 1, 33);
// Inserts 33 before index 1
```

#### 7. Iterating — Two Ways
```cpp
// Index-based loop
for (int i = 0; i < happyVector.size(); i++) {
    cout << happyVector[i] << "\n";
}

// Range-based for loop (C++11)
for (int curElm : happyVector) {
    cout << curElm << endl;
}
```
- `size()` returns current element count
- Range-based: `:` means "for each element in"

### Key Insight
Vectors internally use **iterators** (pointers to elements). The `.begin()` and `.end()` methods return iterator objects that mark positions. Functions like `erase` and `insert` operate on these iterator positions, not raw indices.

---

## General Patterns Across Both Repos

### 1. Heavily Commented "Workbook" Style
Every line or block has a comment explaining what's happening. Not just "what" but "why." These feel like interactive tutorial workbooks, not production code.

### 2. Analogies for Abstract Concepts
- Objects = "cookie cutter making cookies" (class = template)
- Header = "table of contents," implementation = "actual chapters"
- Functions = "black boxes that take input and return output"

### 3. Visual Studio Project Structure
```
LessonDir/
  LessonName.sln           ← Solution file
  LessonName/              ← Project folder
    LessonName.vcxproj     ← Project config
    main.cpp               ← Source
    ClassName.h            ← Header
    ClassName.cpp          ← Implementation
  x64/                     ← Build output
```

### 4. Concise Numbered Naming
Lessons numbered like `1.10`, `1.11`, `1.13.1` — easy to track progression. Repo name `1.1ClaudeCPP` suggests this is lesson 1, sub-part 1, written with Claude AI assistance.

### 5. Different AI Assistants Used
The repo naming suggests multiple AI helpers: `ClaudeCPP`, `ChatGPTExtraCred*`, `GeminiExtraCred*`, `Grok*` — showing Adam experimented with several AI coding assistants for learning C++.

---

## Things Worth Noting
- `#include <vector>` required to use vectors
- `#include <string>` needed for `std::string`
- `using namespace std;` used throughout (common in beginner tutorials, considered lazy in production)
- VS `x64` folder = 64-bit build artifacts — these repos use Windows Visual Studio
- `cout <<` vs `cout << endl` — both print, `endl` adds newline + flushes buffer
