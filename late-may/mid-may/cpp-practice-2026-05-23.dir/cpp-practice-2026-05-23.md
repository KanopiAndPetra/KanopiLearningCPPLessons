# C++ Practice Notes - May 23, 2026

## Session Info
- **Time:** 5:00 PM CDT
- **Topic:** Classes + Object-Oriented Programming (OOP)

## Repos Studied
- **Oppie1/1.1Oppie1CPP** — Classes lesson
- **Oppie1/1.1Oppie1CPP** — Constructors lesson

## Key Concepts Learned

### What is a Class?
- A **class** is a blueprint for creating **objects**
- Bundles **data** (variables) + **functions** (methods) together
- Think of it like a cookie cutter — the class defines the shape, objects are the actual cookies

### Public vs Private
- **private:** Data can't be accessed directly from outside — must use methods
- **public:** Methods/functions others can call
- Encapsulation = hiding internal details, exposing only what's needed

### Constructors
- Special function that **runs automatically** when an object is created
- Same name as the class, no return type
- Can have **multiple constructors** (overloading!)
- `BankAccount(string, double)` — full init
- `BankAccount(string)` — name only
- `BankAccount()` — default, no args

### Destructors
- `~ClassName()` — runs when object is destroyed
- Useful for cleanup (releasing memory, closing files)

### Member Functions (Methods)
- Functions defined inside a class
- Called on objects: `object.method()`
- Can access private data directly

### Getters
- Read-only access to private data
- `double getBalance() const { return balance; }`
- The `const` promises it won't modify the object

### const Correctness
- `const BankAccount` — can only call const methods on it
- Methods marked `const` promise not to modify the object

## Program Created

**File:** `cpp-practice-2026-05-23.cpp`

**What it does:**
1. **BankAccount class** — models a bank account with:
   - Private: accountHolder, accountNumber, balance
   - Constructors: full init, name-only, default
   - Methods: deposit(), withdraw(), transfer(), display()
   - Getters: getBalance(), getAccountNumber(), getHolderName()

2. **Student class** — tracks grades with:
   - Private: name, vector of grades
   - Methods: addGrade(), getAverage(), getLetterGrade(), display()

3. **Demos:**
   - Creating objects with different constructors
   - deposit/withdraw with error checking
   - Transfer between accounts (reference parameter!)
   - Student grade tracking + average
   - Vector of objects
   - Finding highest balance in a list
   - Getters for safe access
   - const correctness demonstration

## Output Summary
```
Bank accounts created with auto-incrementing numbers (1000, 1001, ...)
Deposits and withdrawals work correctly
Transfer moves money between accounts
Student: Charlie | Grades: 4 | Avg: 86.25% | Letter: B
Destructors fire in reverse order as objects go out of scope
```

## Next Steps / Areas to Explore
- Separate compilation (.h/.cpp files)
- Operator overloading (<< for printing, + for adding objects)
- Inheritance and polymorphism
- Virtual functions and abstract base classes
- Smart pointers (unique_ptr, shared_ptr)
- Static class members in depth

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Committed:** 2026-05-23