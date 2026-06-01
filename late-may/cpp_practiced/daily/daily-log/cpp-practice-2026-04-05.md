# C++ Practice Session — April 5, 2026

**Status:** COMPLETED — compiled and ran successfully

## What I Studied
- GitHub repo 1.1ClaudeCPP — lessons 50-59
- Virtual functions, inheritance, polymorphism, templates
- Connected to previous lessons on classes and pointers (April 1)

## What I Created
**File:** `workspace/learning/cpp_practice/cpp-practice-2026-04-05.cpp`

## Concepts Covered
- **Polymorphism** — same interface (getArea()), different implementations per shape
- **Virtual functions** — `virtual double getArea() const = 0` (pure virtual)
- **Abstract base class** — Shape can't be instantiated, only derived from
- **Dynamic binding** — Shape* pointer resolves to correct getArea() at RUNTIME
- **Override keyword** — C++11 explicit override
- **Virtual destructor** — ensures proper cleanup when deleting through base pointer
- **Virtual vs non-virtual** — demonstrated the difference with Base/Derived example

## Program Output
```
Shape Hierarchy: Rectangle(5x3=15), Circle(r=2=12.57), Triangle(4x6=12)
Total area via polymorphism: 39.57
Virtual binding: Derived::whoAmI() called correctly
Non-virtual: Base::identify() called (wrong!)
Virtual destructor: Both Derived AND Base destructors called ✓
```

## Key Insight
Virtual functions enable RUNTIME polymorphism — the function call is resolved based on the actual object type, not the pointer type. This is the foundation of interfaces and OOP design patterns. Without virtual, you'd need type-checking if/else chains.

## Telegram Delivery
⚠️ Attempted to send summary to Telegram but sessions_send timed out twice from this context. 
