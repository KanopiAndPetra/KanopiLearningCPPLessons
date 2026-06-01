# C++ Practice Notes - 2026-04-24

## Repositories Studied

1. **Oppie2Cme** - C tutorial lessons by "me" (human notes)
   - Reviewed: Lesson 1.20 "What About More Than 2 Choices" - switch statements in C
   - Concept: `switch` statement for multi-way branching instead of many if-else chains
   - Syntax: `switch (variable) { case value: ... break; default: ... }`

2. **Oppie1.2CPPLessJustNotes** - C++ lessons
   - Reviewed: String class functions lesson
   - Concept: Using C++ `std::string` member functions like `.length()`, `.substr()`

## New Topic Today: Switch Statements + Enums in C++

While switch statements exist in C, C++ adds powerful features:
- **Enums** - define a set of named integer constants
- **Switch with enums** - clean, readable multi-way branching
- **Structs** - lightweight data containers (simpler than classes)

## Key Concepts Learned

### 1. Switch Statement
```cpp
switch (variable) {
    case value1:
        // code
        break;
    case value2:
        // code
        break;
    default:
        // code if no match
}
```
- Checks equality against multiple values
- `break` prevents fall-through to next case
- `default` handles cases not matched

### 2. Enumerations (enum)
```cpp
enum Direction { NORTH, SOUTH, EAST, WEST };
```
- Creates a new type with named constant values (0, 1, 2, 3)
- Makes code more readable than magic numbers
- Values are integers under the hood

### 3. Structs
```cpp
struct Point {
    int x;
    int y;
};
```
- Simple data grouping (no functions, unlike classes)
- Good for plain data containers

## Program Created

**cpp-practice-2026-04-24.cpp** - Adventure Game Character System
- Uses enum for character class (Warrior, Mage, Rogue, Cleric)
- Uses switch statement to handle class-based actions
- Uses struct to hold character data
- Demonstrates multiple concepts working together

## Location

Saved to: `workspace/learning/recent-practice/cpp-practice-2026-04-24.cpp`