# C++ Practice Notes - 2026-04-29

## Repositories Studied

### 1. `1.1Oppie1CPP` - Main C++ Tutorial Repository
A comprehensive C++ tutorial series covering everything from basics to advanced topics.

**Topics studied:**
- **Member Initializers (1.44)**: Initializer lists that initialize member variables BEFORE the constructor body runs. Essential for `const` members and can be more efficient.
- **Composition (1.45-1.46)**: The "has-a" relationship where one class contains objects of other classes as members. Lifetime of parts is managed by the whole.

### 2. `1.74ClaudeExtraCredVectors` 
Vectors tutorial for extra credit work (briefly reviewed)

---

## Concepts Learned

### Member Initializers
```cpp
ClassName::ClassName(int a, int b) : regVar(a), constVar(b) {
    // constructor body
}
```
The colon and list after the parameters is the **member initializer list**. It initializes `regVar` with `a` and `constVar` with `b` before the constructor body executes. Const members MUST use this syntax.

### Composition
- Models "has-a" relationships (e.g., a Bank Account **has a** Balance)
- When the parent object is created, the child objects are created first
- When the parent is destroyed, child objects are destroyed automatically
- Unidirectional: the part doesn't know about the whole

---

## Practice Program: Bank Account with Composition & Member Initializers

**Concept**: A banking system demonstrating:
1. **Composition**: `BankAccount` contains a `Money` object (composition - "has-a")
2. **Member Initializers**: Using initializer lists in constructors
3. **Const members**: Why member initializers are required for const data

**Files created:**
- `cpp-practice-2026-04-29.cpp` - The practice program
- `cpp-practice-2026-04-29.md` - This notes file

---

## Key Takeaways

1. **Member initializer lists** are required for `const` members and reference members
2. They can be more efficient (initialization vs assignment)
3. **Composition** is about ownership and lifetime management, not inheritance
4. When designing classes, think "has-a" vs "is-a" to choose composition vs inheritance
