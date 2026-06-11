# C++ Practice Notes - June 4, 2026

## Session Info
- **Time:** 3:00 PM CDT
- **Topic:** Operator Overloading (<< and + operators)

## Key Concepts Learned

### What is Operator Overloading?
- **Operator overloading** allows you to define behavior for your own custom operators
- Lets you make your classes "feel" more natural to work with
- Think of it as giving your classes superpowers — they can now respond to familiar operations

### The `<<` (stream insertion) Operator Overload
- Normally `<<` is used for printing
- By overloading it, we can print custom objects like `BankAccount` and `vector<BankAccount>`
- Syntax: `std::ostream& operator<<(ostream& os, const BankAccount& account)`
- Returns the stream (allows chaining: `cout << acc1 << acc2;`)

### The `+` Operator Overload
- Normally `+` adds numbers
- By overloading it, we can add two `BankAccount` objects together
- The `const` keyword ensures the original accounts aren't modified
- Returns a new `BankAccount` with combined balances and alphabetically sorted holder names

## Program Created

**File:** `cpp-practice-2026-06-04.cpp`

**What it does:**
1. **BankAccount class** with overloaded operators:
   - Constructors: full init
   - Getter methods (const)
   - Deposit method
   - Overloaded `+` operator (combines balances)
   - Overloaded `<<` operator (custom printing)

2. **Demos:**
   - Creating objects with different data
   - Using overloaded `+` to combine two accounts
   - Using overloaded `<<` to print custom objects
   - Template operator overload for printing vectors of any type
   - Chaining multiple `<<` outputs

## Next Steps / Areas to Explore
- Inheritance and polymorphism
- Virtual functions and abstract base classes  
- Smart pointers (unique_ptr, shared_ptr)
- Operator overloading for other operators (=, *, <, etc.)
- Separate compilation (.h/.cpp files)

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Committed:** June 4, 2026 ✓

---
*Operator overloading lets your classes behave more naturally. Tomorrow: inheritance!*
