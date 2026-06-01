# C++ Practice Notes - 2026-04-14

## Repositories Studied

### 1. BroCodeC- (BroCode C++ Tutorial Series)
**URL:** https://github.com/Oppie1/BroCodeC-

This repo covers the BroCode C++ tutorial playlist with topics like:
- If statements, switch statements
- Logical operators
- While loops, do-while loops, for loops
- String methods
- Temperature conversion programs
- Console calculator projects

**Key files examined:** 
- `16.BroCodeUseFulStringMethodsNoNotes/FileName.cpp`

This tutorial demonstrates:
- `getline()` for input
- `length()` to check string length
- `empty()` to check if string is blank
- `clear()` to clear a string
- `append()` to add to end of string
- `at()` to access individual characters
- `insert()` to insert at position
- `find()` to locate characters
- `erase()` to remove characters

### 2. 63CPPGeminiWString (Wide Strings Tutorial)
**URL:** https://github.com/Oppie1/63CPPGeminiWString

This repo covers `wstring` - wide character strings for Unicode support.

**Topics learned:**
- `wstring` uses `wchar_t` instead of `char`
- L prefix for wide string literals: `L"Hello"`
- Wide strings needed for international characters (Japanese, Chinese, etc.)
- Same methods as regular string but with `w` prefix: `length()`, `at()`, `append()`, etc.

## What I Created

**File:** `workspace/learning/cpp-practice-2026-04-14.cpp`

A comprehensive C++ program that combines:
1. **Classes with member variables and functions** - CharacterProfile class
2. **Constructors** - default and parameterized constructors
3. **String manipulation** - length, append, insert, find, erase, at, clear, empty
4. **Pointers** - creating objects on heap, accessing via pointers
5. **Wide strings** - demonstrating Unicode support with wstring
6. **Destructors** - cleanup when objects are destroyed

## Key Concepts Practiced

### Classes
```cpp
class CharacterProfile {
private:
    std::string name;      // member variable
    int level;            // member variable
    
public:
    CharacterProfile();    // constructor
    void setName(...);     // member function
    std::string getName(); // getter
};
```

### String Methods
- `length()` → character count
- `at(index)` → character at position
- `empty()` → check if blank
- `clear()` → empty the string
- `append(str)` → add to end
- `insert(pos, str)` → insert at position
- `erase(pos, count)` → remove characters
- `find(char)` → locate character

### Pointers
```cpp
CharacterProfile* ptr = new CharacterProfile();
ptr->setName("Kanopi");
delete ptr;  // cleanup!
```

### Wide Strings
```cpp
std::wstring name = L"Hattori Hanzo";
```

## Compilation & Testing

Compiled successfully with:
```bash
g++ -std=c++17 cpp-practice-2026-04-14.cpp -o cpp-practice-2026-04-14
./cpp-practice-2026-04-14
```

Output verified - all sections ran correctly including:
- Character creation and modification
- Experience/leveling system
- String method demonstrations
- Pointer usage examples
- Wide string demo

## Location
- Source code: `workspace/learning/cpp-practice-2026-04-14.cpp`
- Notes: `workspace/learning/cpp-practice-2026-04-14.md`