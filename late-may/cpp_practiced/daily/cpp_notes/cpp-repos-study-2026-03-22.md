# C++ Tutorial Repos Study — 2026-03-21 Evening Session

## Sources Examined

- `1.1Oppie1CPP` — Adam's primary C++ learning repo (Visual Studio, lessons 1.10+)
- `6CPPBuckyVariableMemryConceptsWNotes` — Standalone lesson on variable/memory, with notes
- `1.1ClaudeCPP` — Claude-generated version of the same lessons, for comparison

---

## Repo Structure & Patterns

### Overall Architecture

Adam's repos follow a consistent **numbered-prefix convention**:
- Single-digit prefix: `1.` = first tutorial series (beginner C++)
- Double-digit within each: `1.10`, `1.11` = individual lessons
- AI-assigned suffix: `Oppie1`, `Claude`, `Grok`, `Gemini` = which AI generated the code

Each repo is a **Visual Studio solution** with this layout:

```
RepoName/
  LessonName/
    LessonName.sln                    ← Visual Studio solution file
    LessonName/                       ← inner project folder
      LessonName.vcxproj             ← project file (MSVC)
      main.cpp                        ← source code
    x64/Debug/
      LessonName.exe                  ← compiled binary
```

Build artifacts (`.obj`, `.pdb`, `.ilk`) are committed alongside source — a sign these are student portfolios, not production code.

### Naming Conventions

- Repo name embeds the lesson number: `1.10Oppie1FuncWMultiplParmetrs`
- File name matches repo/lesson name (no spaces, CamelCase-ish)
- Some repos tagged "WNotes" = include handwritten/study notes in the source

---

## Lesson 1.10: Functions with Multiple Parameters

### 1.1Oppie1CPP version (Adam-written, heavily commented)

```cpp
#include <iostream>
using namespace std;

int addNumbers(int x, int y, int a, int b) {
    int answer = x + y + a + b;
    return answer;
}

int main() {
    cout << addNumbers(10, 15, 28, 7); // Output = 60
    return 0;
}
```

**Adam's annotations:**
- Extensive inline comments explaining the call-return mechanism
- Key insight labeled: "function call gets replaced by return value"
- Mentions the matching between parameters (x,y,a,b) and arguments (10,15,28,7)
- Error-prone mistake noted: don't redeclare `int` on the same variable

### 1.1ClaudeCPP version (Claude-generated, same logic)

```cpp
#include <iostream>
using namespace std;

int addNumbers(int x, int y, int a, int b) {
    int answer = x + y + a + b;
    return answer;
}

int main() {
    cout << addNumbers(10, 15, 28, 7); // Output = 60
    return 0;
}
```

**Observations:**
- Nearly identical logic, slightly less verbose comments
- Key takeaway comment is similar: "function call is replaced with return value"
- Uses same `using namespace std` pattern

**Verdict:** Same concept, same code, different explanation density.

---

## Lesson 1.11: Introduction to Classes and Objects

### 1.1Oppie1CPP version

This lesson introduces OOP from scratch. Key concepts:

**Class as a template:**
```cpp
class AdamsClass {
public:
    void helloObject() {
        cout << "Hello, welcome to Computer Science Classes/Objects basics. My name is" << endl;
    }
    string name = "Adam";
};
```

**Object instantiation and dot notation:**
```cpp
AdamsClass object1;        // create an instance
object1.helloObject();     // call method with dot separator
cout << object2.name;      // access attribute
```

**Adam's mental model (from comments):**
- "Think of objects as boxes/forms you are creating within main()"
- "Modular thinking — stacking different shape rocks/blocks/boxes"
- The `.` dot separator is explained as a way to "tell the computer what to pull from the object"

**Multiple objects from same class:**
```cpp
AdamsClass object3;
object3.helloObject();
cout << object3.name;
```

**Core insight:** Class defines what an object CAN do (functions) and what it HAS (variables).

---

## Lesson 6: Variable Memory Concepts (Bucky-style)

### 6CPPBuckyVariableMemryConceptsWNotes

```cpp
#include <iostream>
using namespace std;

int main() {
    int tuna = 99;  // Declaration
    tuna = 76;      // Overwrite — no int needed again
    cout << tuna;   // Prints 76
    return 0;
}
```

**Key concept taught:**
- Variables live at a memory address; reassignment overwrites the value at that address
- Declaring `int` twice on same variable name → compiler error
- Once declared, just use the variable name to update its value

**Bucky tutorial style:** This is clearly modeled on thenewboston (Bucky Roberts) C++ tutorials — short programs, single concept, heavily commented errors to avoid.

---

## C++ Concepts Being Taught (across the series)

Progression visible across the repo names and lessons:
1. **Lesson 3** — Print to screen (`cout`)
2. **Lesson 4** — Variables (types: int, double, char, string)
3. **Lesson 5** — Creating a basic calculator
4. **Lesson 6** — Variable memory behavior (overwriting, re-declaration errors)
5. **Lesson 7** — Basic arithmetic
6. **Lesson 8** — If statements
7. **Lesson ~10** — Functions with multiple parameters
8. **Lesson ~11** — Classes and objects (OOP intro)
9. **Lessons 60-74** — Vectors (advanced)

---

## Interesting Patterns

### 1. AI Collaboration via Repo Forks
Adam runs the same lesson through multiple AI assistants (Claude, Grok, Gemini) and saves each version as a separate repo. This is a clever way to compare AI-generated code quality and explanations side-by-side.

### 2. Comment-First Learning
Adam's code is extremely comment-heavy — like a study journal. The comments are the actual learning output, not just explanations of code.

### 3. Visual Studio on Mac (via cross-compiling?)
The `.vcxproj` files and `vc143` (MSVC 2022) build artifacts suggest these are built with Visual Studio on Windows or a Windows VM/remote setup. The `x64/Debug/` folder confirms x86-64 architecture targeting.

### 4. Incremental Complexity
Each lesson number is a small increment over the previous, matching a tutorial video series pacing (Bucky's thenewboston C++ series is ~50 videos). Lessons 60-74 are about vectors — near the end of a beginner curriculum.

### 5. Vector Lessons (73-74)
These later repos (`73CPP_Oppie1`, `74.ExtraCred.VectorsWithNotes`) suggest moving into data structures — `std::vector` — which is the natural next step after learning basic arrays and pointers.

---

## Key Takeaways for C++ Beginners

- **Functions:** Parameters → local variables → return value. The call site is replaced by the returned value.
- **Classes/Objects:** Class = template/blueprint. Object = instance. Members accessed via `.` dot operator.
- **Variables:** Declare type once; reassign by name. Memory overwrites, doesn't accumulate.
- **Code structure:** Always `#include <iostream>`, `using namespace std;`, `int main()`, `return 0;`

---

*Notes compiled by Kanopi — 2026-03-22 (Saturday night, 11:30 PM CT)*
