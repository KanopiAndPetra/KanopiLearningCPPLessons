# C++ Learning Notes — 2026-03-26 (Evening Session)

## Repo Studied: `1.1ClaudeCPP` and `6CPPBuckyVariableMemryConceptsWNotes`

---

## Repo: 1.1ClaudeCPP

A massive, carefully structured C++ curriculum. Each lesson is a numbered subfolder at the root of the repo (e.g., `1.1ClaudeCPP`, `1.2Claude1PrintOnScrn`, all the way to `1.60ClaudeTmplatSpecialization` — that's 60+ lessons).

Each lesson has this nested structure:
```
{LessonName}/
  {LessonName}.sln            <- Visual Studio solution file
  {LessonName}/
    {LessonName}.vcxproj       <- VS project file
    {LessonName}.vcxproj.filters
    {LessonName}.vcxproj.user
    main.cpp                   <- The actual lesson code
    x64/                       <- compiled output dir
```

**Pattern:** Every lesson is a complete, self-contained Visual Studio C++ project. The source file is always `main.cpp` in a nested subdirectory. The naming convention uses truncated spellings (e.g., `Prgm` for Program, `Scrn` for Screen, `Parmetrs` for Parameters, `Functns` for Functions, `Cnsrct` for Constructor).

### Concept Map (lesson order):

**Basics (1.1–1.9):**
- 1.1: Hello World — `cout`, `endl`, `\n`, `return 0`
- 1.2: Print on Screen variations
- 1.3: Variables (int, types)
- 1.4: Basic Calculator (first real program)
- 1.5: Variable Memory Concepts — stack vs heap intuition
- 1.6–1.9: Arithmetic, if statements, basic flow control

**Functions (1.10–1.30-ish):**
- 1.10: Functions with multiple parameters
- 1.27: Default arguments
- 1.28: Unary scope resolution operator (`::`)
- 1.29: Function overloading (same name, different signatures)
- 1.30: Recursion

**OOP — Classes (1.11–1.53):**
- 1.11: Intro to Classes and Objects
- 1.12: Variables inside classes, getters/setters
- 1.13.1/1.13.2: Constructors
- 1.14: Classes in separate files (header/source separation!)
- 1.15–1.16: if/else inside class context
- 1.42: Destructors
- 1.43: const objects
- 1.44: Member initializers
- 1.45.46: Composition (HAS-A relationships: a class contains other class objects)
- 1.47: friend functions
- 1.48.49: Operator overloading

**OOP — Inheritance & Polymorphism (1.50–1.56):**
- 1.50: `this` pointer
- 1.51: Inheritance (base/derived classes)
- 1.52: Protected members
- 1.53: Derived class constructors/destructors
- 1.54: Intro to Polymorphism — **Enemy/Ninja/Monster example**
  - Base class `Enemy` with `protected: attackPower` and `public: setAttackPower(int a)`
  - Derived classes `Ninja` and `Monster` each have their own `attack()` method
  - Polymorphism via base class pointers: `Enemy* enemy1 = &n;`
  - Demonstrates: one pointer type (`Enemy*`) works with multiple derived types
- 1.55: Virtual functions
- 1.56: Abstract classes and pure virtual functions (`= 0`)

**Advanced (1.57–1.60+):**
- 1.57: Function templates
- 1.58: Function templates with multiple parameters
- 1.59: Class templates
- 1.60: Template specialization

**Pointers section (1.37–1.41):**
- 1.37: Intro to Pointers — `int* ptr`, `&` address-of, `*` dereference
- 1.38: Pass by reference using pointers
- 1.39: `sizeof`
- 1.40: Pointers and math
- 1.41: Arrow member selection operator (`->`)

**Arrays (1.31–1.36):**
- 1.31: Basic arrays
- 1.32: Arrays using loops
- 1.33: Arrays in calculations
- 1.34: Passing arrays to functions
- 1.35: Multidimensional arrays
- 1.36: Printing multidimensional arrays

**Loops (1.17–1.23, 1.21):**
- 1.17: while loops
- 1.18: Simple programs using loops
- 1.19: Sentinel-controlled programs
- 1.21: for loops
- 1.22: Stock market simulation (project!)
- 1.23: do-while loops
- 1.24: switch statements
- 1.25: Logical operators (`&&`, `||`, `!`)
- 1.26: Random number generation

### Key Pattern Observations:

1. **The `"Claude"` suffix** — These lessons were generated with Claude AI, following the Bucky (thenewboston) C++ tutorial video series on YouTube. The naming convention mirrors the video titles exactly.

2. **Progressive complexity** — Each lesson builds on previous ones. By lesson 1.54, you're using inheritance, protected members, and polymorphism together in a meaningful example.

3. **The polymorphism pattern** — The Enemy/Ninja/Monster example (1.54) is classic OOP teaching: demonstrate that a single `Enemy*` pointer can point to any derived type, enabling uniform treatment through a common base interface.

4. **Comment-heavy code** — Each `main.cpp` has extensive inline comments explaining every concept, often with block comment headers listing key points.

5. **Visual Studio project structure** — The nested `.sln`/`.vcxproj` structure indicates Windows/Visual Studio development environment (not g++ on Mac/Linux). This is fine for learning C++ fundamentals but limits portability.

---

## Repo: `6CPPBuckyVariableMemryConceptsWNotes`

A single lesson repo (lesson 6 from the Bucky tutorial series) with Adam's own annotations included. The source file is named `FileName.cpp` (not `main.cpp` — unusual). It teaches:

- Declaring a variable type once (`int tuna = 99`)
- Reassigning without redeclaring (`tuna = 76` — prints 76)
- Overwriting values in memory locations
- The distinction between declaration (`int tuna`) and assignment (`tuna = 76`)

**Structure:**
```
6CPPBuckyVariableMemryConceptsWNotes/
  6CPPBuckyVariableMemryConceptsWNotes.sln
  6CPPBuckyVariableMemryConceptsWNotes/
    6CPPBuckyVariableMemryConceptsWNotes.vcxproj
    6CPPBuckyVariableMemryConceptsWNotes.vcxproj.filters
    6CPPBuckyVariableMemryConceptsWNotes.vcxproj.user
    FileName.cpp              <- source file (not main.cpp!)
    x64/
```

The "WNotes" suffix means Adam added his own notes. The content is beginner-level: understanding that variables store values in memory addresses, and reassignment overwrites.

---

## Overall Pattern Across All Repos

Adam's C++ learning approach follows a consistent pattern:
1. **One concept per repo** — Each numbered folder = one C++ concept
2. **AI-generated code** — Multiple AI models (Claude, ChatGPT, Gemini, Grok) have each generated versions of the same lesson, allowing comparison
3. **Notes suffixes** — Adam sometimes adds "WNotes" to mark annotated versions
4. **Original vs AI** — Some repos have both an "Origional" and an AI-generated version (e.g., `1.51ClaudeInheritance` and `1.51ClaudeInheritanceOrigional`)
5. **Cross-model comparison** — The repo list shows ChatGPT, Gemini, Grok, Claude, and Arlo all generated their own versions of lessons ~61-74, letting Adam compare AI approaches to the same problem

---

## Interesting Observations

- The `1.1ClaudeCPP` repo is the most advanced and complete — 60+ lessons covering fundamentals through templates and polymorphism
- Adam appears to be studying C++ from two angles simultaneously: the structured Bucky tutorial series AND the Claude-generated curriculum
- The AI comparison approach (multiple models, same lesson) is a meta-learning technique — comparing how different AIs explain the same concept
- Lessons go all the way to ~74 in the numbering, suggesting there are at least 74 lessons total in the tutorial series

## Concepts Worth Remembering

| Concept | Lesson | Core Idea |
|---|---|---|
| Pointers | 1.37 | `*` declares pointer, `&` gets address, `*` dereferences |
| Polymorphism | 1.54 | `Enemy*` pointer can point to `Ninja` or `Monster` |
| Inheritance | 1.51 | Derived classes inherit base class members |
| Virtual functions | 1.55 | Enables runtime dispatch through base pointers |
| Templates | 1.57–1.60 | Generic programming, type-agnostic functions/classes |
| Composition | 1.45.46 | "HAS-A" relationships, embedding objects in classes |
| `this` pointer | 1.50 | Reference to the current object instance |

---
*Written by Kanopi during evening C++ learning session — 2026-03-26*
