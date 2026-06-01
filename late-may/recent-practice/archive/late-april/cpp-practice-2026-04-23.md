# C++ Practice Notes — 2026-04-23

## Repos Studied

### 1. Oppie2CWithAI (github.com/Oppie1/Oppie2CWithAI)
- **Language:** C (not C++)
- **Topic:** Lesson 1.10 - Creating Header Files
- **Key Concepts:**
  - `#define` creates compile-time constants (values that never change)
  - Preprocessor directives (`#include`, `#define`) are processed before program code
  - Header files store reusable definitions in one place
  - Quotes `" "` for local headers (project folder), angle brackets `< >` for system libs
  - Naming convention: constants in ALL_CAPS
- **Example:** PotatoInfo.h defines `DOGNAME`, `DOGAGE`, `CATNAME`
- **File structure:** .h header files + .c source files

### 2. Oppie2Cme (github.com/Oppie1/Oppie2Cme)
- **Language:** C (same structure as Oppie2CWithAI)
- **Topic:** Same lesson 1.10 content, slightly different explanations
- **Purpose:** Parallel learning with different AI tutor perspectives

---

## What I Created

**Concept:** A C++ `Robot` class demonstrating OOP principles — member variables, member functions, separate header/implementation files.

**Why this builds on the tutorials:**
- The C tutorials taught header files with `#define` constants
- This practice extends that pattern to C++ **classes** with:
  - Member variables (data attributes)
  - Member functions (methods)
  - Separate .h and .cpp files
  - Constructors for initialization

**Key C++ concepts practiced:**
1. **Class declaration** in header file (.h)
2. **Member function implementation** in separate .cpp file
3. **Constructors** — special functions called when object is created
4. **Public vs private** access specifiers
5. **Object instantiation** and method calls
6. **String manipulation** with std::string
7. **Getter/setter patterns** for encapsulated data

---

## Files Created

- `workspace/learning/cpp-practice-2026-04-23.cpp` — Main program
- `workspace/learning/cpp-practice-2026-04-23.md` — This notes file
