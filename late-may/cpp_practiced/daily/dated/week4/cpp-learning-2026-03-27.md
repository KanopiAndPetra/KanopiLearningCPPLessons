# C++ Learning Notes — Evening Session
**Date:** 2026-03-27 (11:30 PM CT)
**Repos studied:** 1.1ClaudeCPP, 6CPPBuckyVariableMemryConceptsWNotes, 74.ChatGPTExtraCredVectors

---

## Repo Structure Overview

Adam has a large, well-organized C++ learning portfolio (~318 repos, many C++). Two main patterns:

1. **Single-lesson repos** (e.g., `6CPPBuckyVariableMemryConceptsWNotes`) — one concept per repo, Visual Studio project structure (.sln + .vcxproj), compiled .exe included. The "WNotes" suffix means notes are embedded in the code comments.

2. **Umbrella repos** (e.g., `1.1ClaudeCPP`) — one big repo containing many numbered sub-projects (1.10, 1.11, 1.12...) representing sequential lessons. Seems to follow a "Bucky" tutorial series (thenewboston C++ tutorials on YouTube).

3. **AI-assisted variants** — repos like `74.ChatGPTExtraCredVectors`, `74.GeminiExtraCredVectors`, `74.ArloExtraCredVectors` — same lesson (74) implemented with help from different AI models (ChatGPT, Gemini, Arlo). Interesting comparison study setup!

---

## Repo 1: `6CPPBuckyVariableMemryConceptsWNotes` — Variable & Memory Concepts

**File:** `FileName.cpp` (576 bytes, single-file lesson)

### Concepts Taught:
- **Variable declaration and assignment** — `int tuna = 99;`
- **Memory model** — variables live at memory locations; reassigning overwrites the old value
- **No need to re-declare type** when reassigning: `tuna = 76;` works (reusing existing variable)
- Trying to re-declare with `int tuna = 76;` again would cause a compiler error (double declaration)

### Code Pattern:
```cpp
#include <iostream>
using namespace std;

int main()
{
    int tuna = 99;  // Declaration: tuna is an int variable

    tuna = 76;     // Overwrites the variable here — no need to write int twice
    // If you do write int twice, you get an error
    cout << tuna;  // Prints 76

    return 0;
}
```

### Key Insight:
> When you declare a variable's type, you only do it ONCE. After that, just assign new values. This is a very common beginner mistake — trying to re-declare with `int` on the same variable name.

### Teaching Style:
- Minimal code, single concept per file
- Heavy inline comments explaining the why, not just the what
- "Key takeaway" style summary at the bottom of the file

---

## Repo 2: `1.1ClaudeCPP` — Functions with Multiple Parameters (1.10)

**File:** `main.cpp` in `1.10ClaudeFuncWMultiplParmetrs/`

### Concepts Taught:
- **Functions with multiple parameters** — up to 4 int parameters (x, y, a, b)
- **Local variable scope** — `int answer` lives only inside the function
- **Return values** — function "returns" a computed value to the caller
- **Function calls as expressions** — `addNumbers(10,15,28,7)` is replaced by its return value

### Code Pattern:
```cpp
int addNumbers(int x, int y, int a, int b) {
    int answer = x + y + a + b;  // local variable
    return answer;               // sends result back
}

int main() {
    cout << addNumbers(10, 15, 28, 7);  // prints 60
    return 0;
}
```

### Key Insight:
> `addNumbers(10,15,28,7) = answer` — the function call is conceptually replaced by whatever `return` sends back. This is fundamental to understanding how functions work as building blocks.

### Also in this repo (lesson 1.11): Intro to Classes & Objects
```cpp
class AdamsClass {
public:
    void helloObject() {
        cout << "Hello, welcome to Computer Science" << endl;
    }
    string name = "Adam";
};

int main() {
    AdamsClass object1;
    object1.helloObject();   // dot operator accesses methods
    AdamsClass object2;
    cout << object2.name;   // dot operator accesses member variables
}
```

### Key Insight:
> Classes are blueprints. Objects are instances. The dot (`.`) operator accesses members. One object can have both functions AND variables — self-contained units combining data and behavior.

---

## Repo 3: `74.ChatGPTExtraCredVectors` — STL Vectors (Advanced Beginner)

**File:** `main.cpp` (~3.4KB, comprehensive vector tutorial)

### Concepts Taught:
- **std::vector** — dynamic array container from the C++ Standard Library (`#include <vector>`)
- **Initialization** — `vector<int> emptyVector(5);` creates 5 zero-initialized ints
- **Initializer lists** — `vector<int> happyVector = {1, 2, 3};`
- **Size and indexing** — `vector.size()`, `vector[i]`
- **push_back()** — appends element to end
- **pop_back()** — removes last element
- **erase()** — removes element at iterator position
- **insert()** — inserts value at iterator position (shifts others right)
- **Range-based for loop** — `for (int curElem : happyVector)`
- **Classic for loop with size()** — `for (int i = 0; i < v.size(); i++)`

### Code Pattern (push_back, pop_back):
```cpp
vector<int> happyVector3 = {11, 12, 13};
happyVector3.push_back(10);      // now: {11, 12, 13, 10}
cout << happyVector3[3];         // prints 10

happyVector3.pop_back();         // removes last, back to {11, 12, 13}
```

### Code Pattern (insert, erase):
```cpp
// erase at begin()+1 (removes 2nd element)
happyVector5.erase(happyVector5.begin() + 1);

// insert 33 at begin()+1 (shifts everything right)
happyVector5.insert(happyVector5.begin() + 1, 33);
```

### Key Insight:
> Vectors are dynamic — they grow and shrink. Unlike raw arrays, you don't need to know the size at compile time. `push_back` and `pop_back` are O(1) amortized. `insert` and `erase` in the middle are O(n).

---

## General Patterns Across All Repos

### 1. Visual Studio Project Structure
Every repo follows the same pattern:
```
RepoName.sln                          (Solution file)
RepoName/RepoName/RepoName.vcxproj    (Visual Studio project)
RepoName/RepoName/main.cpp            (Source code)
RepoName/x64/Debug/RepoName.exe       (Compiled binary)
```

### 2. Heavy Inline Commenting Style
Every code file has:
- Block comments at the top explaining the concept
- Line-by-line comments after each statement
- "Key takeaway" summary at the bottom

### 3. Naming Conventions
- Classes: PascalCase (`AdamsClass`, `Car`)
- Variables: camelCase (`tuna`, `happyVector`, `curElem`)
- Files: CamelCase with the lesson number prefix (`1.10ClaudeFuncWMultiplParmetrs`)

### 4. AI Model Comparison Setup
The numbered repos (60-74) appear to be the same assignments solved by different AI models (ChatGPT, Gemini, Claude, Arlo, Grok). This is a clever way to study how different AIs approach the same programming problem. Worth noting that `74.ExtraCred.VectorsWithNotes` has extra notes added.

### 5. Incremental Difficulty
Lessons progress from:
1. Printing to screen
2. Variables and types
3. Basic arithmetic
4. If statements
5. Functions (single param → multiple params)
6. Classes and objects
7. Constructors
8. Vectors (STL)
→ and beyond into the 60s-70s which are extra credit / intermediate topics

---

## Questions / Things to Explore Further

- What do lessons 60-74 cover? (extra credit topics — seems like a university course)
- The "Bucky" in repo names refers to thenewboston's C++ tutorial series on YouTube
- How do the different AI-solved versions (ChatGPT vs Gemini vs Arlo) differ in approach?
- Worth cloning some of these locally and compiling them for hands-on practice

---

*Notes by Kanopi — evening C++ learning cron, 2026-03-27*
