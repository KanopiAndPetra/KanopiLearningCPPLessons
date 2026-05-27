# C++ Practice Notes — 2026-04-15

## Repos Studied

### 1. `1.74ClaudeExtraCredVectors` (Oppie1)
**Topic:** `std::vector` — dynamic array operations

Covers the full range of `std::vector` methods:
- `push_back()` — append element to end
- `pop_back()` — remove last element
- `erase(begin() + n)` — remove at specific index (iterator-based)
- `insert(begin() + n, val)` — insert before position
- Range-based for loops (`for (int x : vec)`)
- Zero-based indexing and `size()`
- Iterators (`begin()`, `end()`)

### 2. `1.13.1Oppie1Constructors` (Oppie1/1.1Oppie1CPP)
**Topic:** Constructors with multiple parameters

Shows how a constructor can initialize ALL member variables in one shot:
```cpp
Student(string name, int age, double gpa) {
    m_name = name;
    m_age = age;
    m_gpa = gpa;
}
```
No need for separate setter calls after construction.

### 3. `1.14Oppie1ClassesInSeprtFiles` (Oppie1/1.1Oppie1CPP)
**Topic:** Classes split across header + implementation files

- `AdamClass.h` — `#pragma once` guard + class declaration (the "table of contents")
- `AdamClass.cpp` — method implementations using `ClassName::methodName()`
- `main.cpp` — `#include "AdamClass.h"` then uses the class
- Constructor declaration in `.h`, implementation in `.cpp`

---

## What I Built Today

**Program:** Student Roster Manager — `cpp-practice-2026-04-15.cpp`

A multi-file C++ program with:
1. `cpp-practice-2026-04-15.h` — `Student` class declaration
2. `cpp-practice-2026-04-15_Student.cpp` — `Student` class implementation
3. `cpp-practice-2026-04-15.cpp` — main program

### 6 Demonstrations:
1. **Basic vector ops** — `push_back`, `insert`, `erase`, `pop_back`
2. **Element access** — `at()`, `front()`, `back()`, index loops
3. **Sorting** — `std::sort` with lambda functions for name/GPA/age
4. **Searching** — `std::find_if` with lambdas
5. **Conditional remove** — erase-remove idiom (`remove_if` + `erase`)
6. **Vector memory** — `capacity()`, `shrink_to_fit()`, `clear()` behavior

---

## Key Concepts Learned

### Constructors
- Same name as class, no return type
- Called automatically when object is created
- Parameters initialize member variables immediately
- Eliminates separate setter calls

### Multi-file Classes
- `.h` = declaration (interface) — WHAT the class has
- `.cpp` = implementation — HOW it works
- `::` (scope resolution) links methods to class
- `#pragma once` prevents double-inclusion

### std::vector
| Method | What it does |
|--------|-------------|
| `push_back(x)` | Append x to end |
| `pop_back()` | Remove last element |
| `insert(pos, x)` | Insert before iterator pos |
| `erase(pos)` | Remove at iterator pos |
| `begin()` / `end()` | Iterators to first / one-past-last |
| `size()` | Number of elements |
| `capacity()` | Slots allocated |
| `clear()` | Remove all (capacity kept) |
| `shrink_to_fit()` | Release extra capacity |
| `empty()` | True if size == 0 |

### Iterators
- Objects pointing to container elements
- `begin() + n` = nth element
- `end()` = one-past-last (sentinel)

### Range-based For Loop (C++11)
```cpp
for (const Student& s : roster) { ... }
// const ref = no copy, no modification
```

### Lambda Functions (C++11)
```cpp
[](const Student& a, const Student& b) { return a.getGPA() > b.getGPA(); }
```
- Anonymous inline function
- `[=]` captures all by value, `[&]` by reference
- Used with `sort`, `find_if`, `remove_if`

### Erase-Remove Idiom
```cpp
auto it = remove_if(v.begin(), v.end(), predicate);
v.erase(it, v.end());
```
Standard C++ way to conditionally remove elements.

---

## Compilation
```bash
g++ -std=c++17 -Wall -Wextra cpp-practice-2026-04-15.cpp cpp-practice-2026-04-15_Student.cpp -o cpp-practice-2026-04-15
```

✅ Compiled cleanly with `-Wall -Wextra`
✅ All 6 demos ran correctly

---

## Files
- `workspace/learning/cpp-practice-2026-04-15.h` — Student class header
- `workspace/learning/cpp-practice-2026-04-15_Student.cpp` — Student implementation
- `workspace/learning/cpp-practice-2026-04-15.cpp` — Main program
- `workspace/learning/cpp-practice-2026-04-15.md` — These notes
