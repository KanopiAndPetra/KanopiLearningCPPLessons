# C++ Learning — Evening Session Notes
**Date:** 2026-03-29 (Sunday night, ~11:30 PM CDT)
**Repos studied:**
- `1.1Oppie1CPP` — comprehensive sequential tutorial series (1.1 through ~1.24+)
- `6CPPBuckyVariableMemryConceptsWNotes` — single lesson on variable declaration & memory

---

## Repo: `1.1Oppie1CPP` — Structure Overview

This is Adam's main C++ tutorial series. It's organized as a **single monorepo with numbered subfolders**, each representing one lesson. Think of it like chapters in a book.

### Directory Structure Pattern
```
1.1Oppie1CPP/
  1.1Oppie1HelloWorld/        ← Lesson 1: Hello World
    1.1Oppie1Hello.sln        ← Visual Studio solution file
    1.1Oppie1Hello/           ← Project folder
      main.cpp                ← Source code
    x64/                      ← Compiled output (not committed meaningfully)
  1.10Oppie1FuncWMultiplParmetrs/
  1.11Oppie1IntroClassesNObjects/
  1.12Oppie1VariablesInClassesGetNSet/
  1.13.1Oppie1Constructors/
  1.13.2OppieConstructorsGetNSet/
  1.14Oppie1ClassesInSeprtFiles/   ← Classes split across .h and .cpp
  1.15Oppie1IfStatementAgin/
  1.16Oppie1IfElseStatement/
  ... (continues through at least 1.24+)
```

**Key observation:** The numbering scheme is `1.N` at the top level (chapter/topic), with sub-numbering like `1.13.1` and `1.13.2` for sub-lessons within a topic. The repo was last updated March 29, 2026 — very recent.

---

## Lessons Observed (sequence so far)

| Lesson | Topic |
|--------|-------|
| 1.1 | Hello World — `cout`, `endl`, `\n`, return 0 |
| 1.10 | Functions with multiple parameters |
| 1.11 | Intro to Classes & Objects |
| 1.12 | Variables inside classes — getters/setters |
| 1.13.1 | Constructors |
| 1.13.2 | Constructors + getters/setters |
| 1.14 | Classes split into separate .h and .cpp files |
| 1.15–1.16 | If / If-Else statements |
| 1.17–1.19 | While loops, loop programs, sentinel-controlled programs |
| 1.20 | Assignment and increment operators |
| 1.21 | For loops |
| 1.22 | Stock market simulation (applied project) |
| 1.23 | Do-while loops |
| 1.24 | Switch statement |

---

## C++ Concepts Being Taught (from code review)

### 1. `main.cpp` — Hello World (Lesson 1.1)
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
**Concepts:** `#include <iostream>`, `using namespace std`, `cout` stream output, `endl` vs `\n`, returning 0 from `main()` to indicate success.

The comments are **extremely verbose** — each line explained in plain English. Example: *"This means your program finished with '0' errors. Successful program."* This is a signature style of Adam's notes.

---

### 2. `FileName.cpp` — Variable Declaration & Memory (Bucky repo)
```cpp
#include <iostream>
using namespace std;

int main() {
    int tuna = 99;  // Declaration — tuna is an int variable
    tuna = 76;      // Overwrite — no need to write int twice
    cout << tuna;   // Prints 76
    return 0;
}
```
**Concepts:**
- Declaring a variable type **once** (`int tuna`)
- Reassigning without repeating the type
- Memory model: each value overwrites the previous one at the same memory address
- No `using namespace std` style note: sometimes Adam omits it, sometimes includes it

---

### 3. `main.cpp` — Classes & Objects (Lesson 1.11)
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
    object1.helloObject();
    AdamsClass object2;
    cout << object2.name;
}
```
**Concepts:**
- `class` as a template/blueprint
- `public:` access modifier
- Objects as **instances** of a class
- Dot operator (`.`) to access members
- Methods (functions inside classes) vs. member variables
- `string` type (from `<string>` implied)

Adam's analogy: *"objects are boxes/forms you create within main() that each do something different. Programmers think modularly, stacking different shape rocks/blocks/boxes."*

---

### 4. Classes in Separate Files — Lesson 1.14

**`AdamClass.h`** (header — declaration only):
```cpp
#pragma once
class AdamClass {
public:
    AdamClass();  // Constructor — same name as class
};
```

**`AdamClass.cpp`** (implementation):
```cpp
#include <iostream>
#include "AdamClass.h"
using namespace std;

AdamClass::AdamClass() {
    cout << "I am a constructor..." << endl;
}
```

**`main.cpp`**:
```cpp
#include <iostream>
#include "AdamClass.h"
using namespace std;

int main() {
    AdamClass adamObject;  // Constructor runs automatically
    return 0;
}
```

**Concepts:**
- `#pragma once` — include guard (prevents double-inclusion)
- Header (`.h`) file declares the class interface
- Implementation (`.cpp`) file defines the methods
- `ClassName::methodName()` syntax for defining methods outside the class
- **Constructors** — methods with the same name as the class, run automatically when an object is created
- `#include "AdamClass.h"` (quotes, not angle brackets) for local headers

---

## Overall Patterns Observed

### 1. Comment-Heavy, Plain-English Style
Every single line in Adam's code has a comment explaining it in non-technical English. The goal is clearly **understanding over brevity**. Comments often use analogies: *"like a blank piece of paper," "like a + sign," "stacking different shape rocks."*

### 2. Personalization
Adam uses his own name ("Adam", "AdamsClass") throughout. Makes the learning feel owned rather than copied from a textbook.

### 3. Visual Studio / Windows Environment
- `.sln` (solution) and `.vcxproj` (Visual C++ project) files
- x64 build target
- This means learning on Windows with the Visual Studio IDE

### 4. Pedagogical Sequence
```
Output → Data Types → Variables → Arithmetic
→ Control Flow (if/else/switch)
→ Loops (while/for/do-while)
→ Functions
→ Classes & Objects
→ Constructors
→ Separate file organization
```
Very traditional, structured approach. Each lesson builds directly on the previous.

### 5. Applied Projects Appear Periodically
Lesson 1.22 ("OppieStockMrktSim") stands out as an applied/practical project using the tools learned so far. This breaks up the pure syntax drills.

### 6. Repo Naming Convention
- `1.1`, `1.2`... = lessons within the series
- `6CPPBucky...` = from Bucky (thenewboston) C++ tutorial videos
- `74.XXX` repos = extra credit assignments (likely from a course)
- `1.74Claude...` = AI-assisted versions of extra credit work

---

## Interesting Design Choices

- **Single `main()` file per lesson:** Keeps things simple and focused. Each lesson = one new concept in one file.
- **`using namespace std;` used freely:** No hesitation about namespace pollution in learning code — totally fine for small programs.
- **Constructors introduced early and reinforced** (1.13.1, 1.13.2, 1.14 all deal with constructors from slightly different angles).
- **The `#pragma once` include guard** appears in the separate-files lesson — a good practical detail that wouldn't be needed in single-file code.

---

## What Could Be Added / Next Steps Noted
- No pointer or reference lessons observed yet (repo may still be growing)
- No `const`, `static`, or `enum` yet
- No file I/O or standard library containers (`vector`, `array`, etc.)
- The series appears to still be actively in progress (last updated just today!)

---

*Notes taken by Kanopi during evening cron session — 2026-03-30 04:35 UTC*
