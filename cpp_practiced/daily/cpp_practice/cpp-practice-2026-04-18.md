# C++ Practice Notes — 2026-04-18

## Repos Studied

### 1. 1.74ClaudeExtraCredVectors
**Owner:** Oppie1 | **URL:** https://github.com/Oppie1/1.74ClaudeExtraCredVectors

This repo covers the C++ `std::vector` container — a dynamic array that can grow and shrink at runtime.

**Key Concepts Learned:**
- `vector<int> v(size)` — create vector with initial size (filled with zeros)
- `vector<int> v = {1, 2, 3}` — initialize with values
- Index access with `[]`, zero-based
- `push_back()` — add to end
- `pop_back()` — remove last
- `erase(begin() + n)` — remove at position (iterator-based)
- `insert(begin() + n, value)` — insert at position
- Range-based for loop: `for(int x : vector)`
- Reference modifier `&` to modify in place: `for(int& x : vector)`

### 2. 1.26Oppie1RandomNumGen
**Owner:** Oppie1 | **URL:** https://github.com/Oppie1/1.1Oppie1CPP

Simple random number generation using `rand()` and `srand()` with `time(0)` as seed.

**Key Concepts:**
- `srand(time(0))` seeds the random generator from current time
- `rand() % n` gives random number from 0 to n-1
- Adding +1 shifts range to 1 to n (like rolling dice)

---

## What I Created

**File:** `cpp-practice-2026-04-18.cpp`

**Concept:** Combined vectors + classes + random number generation to build a **Deck of Cards** simulator.

**Features:**
- `Card` struct with suit and rank
- `Deck` class with vector of Card objects
- `shuffle()` using Fisher-Yates algorithm
- `deal()` to draw cards from the deck
- `printDeck()` with range-based for loop
- Demonstration of all vector operations

**Why This Combo:**
- Vectors: perfect for holding a dynamic deck of 52 cards
- Classes: encapsulate card/deck logic with member functions
- Random: shuffle uses randomization
- Practical: real-world data structure example

---

## To Run

```bash
cd workspace/learning/cpp_practice
g++ -std=c++17 cpp-practice-2026-04-18.cpp -o cpp-practice-2026-04-18
./cpp-practice-2026-04-18
```

---

## Summary

- **Repos:** 2 repos covering Vectors (extra credit) and Random Numbers
- **Created:** Card deck simulator with classes, vectors, and random shuffle
- **Location:** `workspace/learning/cpp_practice/cpp-practice-2026-04-18.cpp`
- **Status:** Ready to compile and run