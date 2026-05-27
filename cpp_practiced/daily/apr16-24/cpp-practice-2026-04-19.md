# C++ Practice Notes — 2026-04-19

## Repos Studied

### 1. `1.1Oppie1CPP` — Lesson 1.14: Classes in Separate Files
**URL:** https://github.com/Oppie1/1.1Oppie1CPP

This lesson demonstrates the fundamental C++ pattern of splitting a class across three files:
- **`.h` (header file):** Declares the class — like a table of contents or blueprint. Tells the compiler what the class contains (member variables, constructor, methods) without defining *how* they work.
- **`.cpp` (implementation file):** Actually writes the code for each method declared in the header. Connects the class name with `::` (scope resolution operator).
- **`main.cpp`:** Creates objects from the class. When an object is instantiated, the constructor runs automatically.

Key insight from the notes:
> The header file is like a table of contents with chapters. The .cpp file is the actual chapters with all the details.

### 2. `Oppie1.2CPPLessJustNotes` — Lesson 1.71: String Class Functions
**URL:** https://github.com/Oppie1/Oppie1.2CPPLessJustNotes

This lesson covers the `std::string` class — strings are objects with built-in methods:
- **`.length()`** — returns the number of characters in the string
- **`.at(index)`** — accesses a single character at a given index (like array indexing but as a method call)
- **`getline(cin, variable)`** — reads a full line of input including spaces (unlike `cin` which stops at whitespace)
- **`.assign(string)`** — copies one string to another (alternative to `=` operator)
- Strings are internally arrays of characters — indexing starts at 0

Key insight: `cin >> stringVar` only reads ONE word. Use `getline()` for full sentences.

---

## What I'm Building

A **`Book`** class split across separate files:
- Demonstrates classes in separate files (header + implementation)
- Uses a constructor that takes parameters
- Uses string class methods: `.length()`, `.at()`, `getline()`, `.assign()`
- Creates a practical program: stores book info and prints it nicely

The class will:
1. Store: title, author, genre
2. Use `.length()` to print title in a centered banner
3. Use `.at()` to print author's initials
4. Use `.assign()` to copy a short genre string
5. Use `getline()` to read multi-word user input
