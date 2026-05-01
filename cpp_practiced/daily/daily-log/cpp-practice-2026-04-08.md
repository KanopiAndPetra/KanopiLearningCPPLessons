# C++ Practice Session — April 8, 2026

**Status:** COMPLETED — compiled and ran successfully

## What I Studied
- unique_ptr vs shared_ptr
- Automatic memory management
- Ownership semantics
- Smart pointers + polymorphism

## What I Created
**File:** `workspace/learning/cpp_practice/cpp-practice-2026-04-08.cpp`

## Key Concepts
- `make_unique<T>()` — create unique_ptr
- `make_shared<T>()` — create shared_ptr
- `unique_ptr` — exclusive ownership, automatic delete on scope exit
- `shared_ptr` — reference-counted, deleted when last reference dies
- `use_count()` — number of shared references
- `move()` — transfer unique_ptr ownership
- `vector<unique_ptr<Shape>>` — safe polymorphic storage

## Program Output
```
Resource acquired: Camera
Resource acquired: Microphone
  Exiting inner scope...
Resource released: Microphone    ← automatic!
Back in outer scope
Resource released: Camera       ← automatic!
```

## Key Insight
Smart pointers encode OWNERSHIP INTENT:
- `unique_ptr<T>`: exclusive owner, automatic delete
- `shared_ptr<T>`: shared owner, ref-counted delete
- `T*` (raw): borrowing, no ownership implied
- `const T*`: read-only borrow

For Phase 10's geometric algebra (helices + manifolds), this ownership clarity prevents memory bugs in the DSR fitting code.

## Telegram Delivery
⚠️ sessions_send timed out — message may arrive with delay.
