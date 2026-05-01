# C++ Practice Notes — 2026-04-21

## Repositories Studied

### 1. `1.1Oppie1CPP` — Lesson 1.60: Template Specialization
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.60Oppie1TmplatSpecialization

**Topic:** Template specialization lets you override a generic `template<typename T>` class for a specific type. The compiler picks the specialized version when the type matches.

**Key concept:**
```cpp
// Generic template — used for int, double, string, etc.
template<typename T>
class Adam {
public:
    Adam(T x) { cout << x << " is not a character!" << endl; }
};

// Specialization for char — has its own behavior
template<>
class Adam<char> {
public:
    Adam(char x) { cout << x << " is indeed a character!" << endl; }
};
```

When you write `Adam<int>(7)` → generic version runs.
When you write `Adam<char>('q')` → specialization runs.

### 2. `1.1Oppie1CPP` — Lesson 1.61: Exception Handling
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.61Oppie1Exceptions

**Topic:** `try`, `catch`, `throw` — graceful error handling without crashing.

```cpp
try {
    int sonsAge = 67, momsAge = 51;
    if (sonsAge > momsAge) throw 99;  // throws int to catch block
}
catch (int x) {
    cout << "Error number: " << x << endl;  // x == 99
}
```

### 3. `1.1Oppie1CPP` — Lesson 1.62: More Exceptions + catch-all
**URL:** https://github.com/Oppie1/1.1Oppie1CPP/tree/main/1.62Oppie1MorExceptinExmple

**Topic:** Division by zero example, and `catch(...)` — the catch-all handler.

```cpp
try {
    if (num2 == 0) throw f;  // throw a char
}
catch (...) {  // catches ANY exception type
    cout << "You cant divide by 0" << endl;
}
```

---

## What I Built

**File:** `cpp-practice-2026-04-21.cpp`

A single self-contained program demonstrating both concepts together:

### Part 1: Template Specialization — `Container<T>`
- Generic `Container<T>` works for int, double, string
- Specialized `Container<char>` adds bonus behavior:
  - Checks if the char is a vowel
  - Prints ASCII code
  - The compiler auto-selects the right version at compile time

### Part 2: Exception Handling — `AgeError` and `DivisionError`
- Custom exception classes inheriting from `std::runtime_error`
- `runtime_error::what()` returns the error message string
- Custom field `age` on `AgeError` lets you access the bad value in the catch block
- Three scenarios tested:
  1. Valid ages (no exception)
  2. Son older than mom → `AgeError` thrown and caught
  3. Division by zero → `DivisionError` thrown and caught
- `catch(...)` demo at the end — catches any thrown type

---

## Key Takeaways

### Template Specialization
- `template<>` with no parameters after it means "specialize for this specific type"
- `Adam<char>` means: when T is char, use this class instead of the generic one
- The compiler decides at compile time which version to use — zero runtime overhead
- Great for when one type needs completely different behavior

### Exception Handling
- `throw` is like `return` but transfers control to a `catch` block somewhere up the call stack
- `catch(int x)` only catches throws of type `int`
- `catch(...)` catches everything — useful for cleanup or logging before re-throwing
- Custom exception classes can carry extra context (like the bad `age` value)
- Throwing exceptions is expensive — don't use them for routine control flow

### Custom Exception Design
```cpp
class AgeError : public runtime_error {
public:
    int age;  // custom field — lets handler see the bad value
    AgeError(const string& msg, int a) : runtime_error(msg), age(a) {}
};
```

---

## Compilation

```bash
g++ -std=c++17 cpp-practice-2026-04-21.cpp -o cpp-practice-2026-04-21
./cpp-practice-2026-04-21
```

**Result:** Compiled clean, ran correctly — all three exception scenarios caught properly, all char vowel checks correct.

---

## File Locations

- **Source code:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-21.cpp`
- **Compiled binary:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-21`
- **Notes:** `/Users/oppie1.kanopi/.openclaw/workspace/learning/cpp-practice-2026-04-21.md`
