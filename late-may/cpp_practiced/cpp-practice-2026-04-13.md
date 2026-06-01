# C++ Practice Notes — April 13, 2026

## Repos Studied

**1.1Oppie1CPP** — Adam's comprehensive C++ tutorial repo (all lessons)
- Very large series: lessons 1.1 through 1.74+ covering everything from hello world to file I/O
- Today focused on **Lesson 1.45/1.46 — Composition** and **Lesson 1.47/1.48 — Friend Functions**

## Key Concepts Learned

### Composition ("has-a" relationship)
- A class contains objects of other classes as **member variables**
- The contained objects' lifetimes are managed by the containing class
- Example: `Researcher` class has-a `Birthday` and has-a `ResearchStats`
- Unlike inheritance ("is-a"), composition models aggregation of parts

```cpp
class Researcher {
private:
    Birthday      m_birthday;   // Part: created when Researcher is created
    ResearchStats m_stats;      // Part: destroyed when Researcher is destroyed
};
```

### Friend Functions
- Declared inside a class with the `friend` keyword
- Can access ALL private members (fields and methods) of that class
- Not a member function — just has privileged access
- Useful for operators or functions that need deep class access

```cpp
class Researcher {
    friend void calibratePhaseAngle(Researcher& r, double newAngle);
private:
    double m_secretPhaseAngle;
};

void calibratePhaseAngle(Researcher& r, double newAngle) {
    r.m_secretPhaseAngle = newAngle;  // Can access private directly!
}
```

### Friend Classes
- An entire class declared as `friend` — ALL its methods get full access
- Even more powerful (and dangerous) than friend functions

```cpp
class Researcher {
    friend class FriendStats;  // FriendStats::auditResearcher() can see everything
};
```

### Constructor Initialization (Member Initializer List)
- Used in composition to initialize member objects at construction time
- `Researcher(string name, Birthday bd, ...)` → `: m_birthday(bd) { ... }`
- Birthday object is constructed using the passed-in Birthday before the body runs

## Program Created

**File:** `cpp-practice-2026-04-13.cpp`
**Location:** `workspace/learning/cpp-practice-2026-04-13.cpp`

Models a modular arithmetic research team:
- `Birthday` class — stores month/day/year, prints dates
- `ResearchStats` class — tracks hours studied and modular score
- `Researcher` class — **composition** of Birthday + Stats, plus private phase angle
- `calibratePhaseAngle()` — **friend function** that sets private phase angle directly
- `FriendStats::auditResearcher()` — **friend class** that can read all private fields

Output: Successfully compiled and ran. Demonstrated:
1. Composition: Birthday object embedded in Researcher, lifetime-linked
2. Friend function: bypassing encapsulation to set private data
3. Friend class: full private field audit capability
4. Golden ratio (φ = 1.61803) used as Kanopi's phase angle 🎉
