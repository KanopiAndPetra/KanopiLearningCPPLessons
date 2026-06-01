# C++ Practice Notes - May 29, 2026

## Session Info
- **Time:** 3:00 PM CDT
- **Topic:** Operator Overloading

## Repos Studied
- Reviewed May 23 notes to pick next topic
- Chose operator overloading as standalone + foundational for inheritance

## Key Concepts Learned

### What is Operator Overloading?
- C++ lets you redefine how operators (+, -, ==, <<, etc.) work for your own types
- Makes objects behave like built-in types — feels natural
- Syntax: `ReturnType operator+(const Class& other) const { ... }`

### Types of Operators

**Arithmetic** (+, -, *, /) — member functions, return NEW object:
```cpp
Fraction operator+(const Fraction& other) const {
    int num = numerator * other.denominator + other.numerator * denominator;
    int den = denominator * other.denominator;
    return Fraction(num, den);
}
```

**Compound Assignment** (+=, -=, *=, /=) — modify *this, return reference:
```cpp
Fraction& operator+=(const Fraction& other) {
    numerator = ...;
    denominator = ...;
    reduce();
    return *this;  // key: return *this, not a copy
}
```

**Comparison** (==, !=, <, >, <=, >=) — member functions, return bool:
```cpp
bool Fraction::operator<(const Fraction& other) const {
    return numerator * other.denominator < other.numerator * denominator;
}
```

**Unary** (-, +) — member functions:
```cpp
Fraction operator-() const { return Fraction(-numerator, denominator); }
```

**Increment/Decrement** (++, --) — tricky! Two versions:
```cpp
// Prefix: increment THEN return
Fraction& operator++() { numerator += denominator; return *this; }

// Postfix: save OLD value, increment, return OLD copy
// The int parameter is a dummy to distinguish it from prefix
Fraction operator++(int) { Fraction temp = *this; numerator += denominator; return temp; }
```

**Stream Operators** (<<, >>) — MUST be non-members (first param is the stream):
```cpp
friend std::ostream& operator<<(std::ostream& os, const Fraction& f) {
    os << f.numerator << '/' << f.denominator;
    return os;  // return os for chaining: cout << a << b << c
}
```

### Key Rules

1. **Arithmetic returns new object** — `a + b` doesn't change a or b
2. **Compound assignment modifies in place** — `a += b` changes a and returns *this
3. **prefix = reference, postfix = copy** — always use prefix in loops
4. **<< and >> are non-members** — left operand is the stream, and streams can't be class members
5. **Make arithmetic operators const** — they don't modify *this
6. **Comparison operators** — define `operator<` and `operator==`; all others can be derived

### How I Built the Fraction Class
- GCD-based reduction in constructor → guarantees lowest terms
- So `1/2 == 2/4` works trivially (both reduce to 1/2)
- Comparison uses cross-multiplication: `a/b < c/d` ⟺ `a*d < c*b`
- All mutable operators marked const (they return new objects or references, don't touch private data)
- `friend` declaration for stream operators → lets them access private numerator/denominator directly

### const Correctness in Practice
When I made `const Fraction fr(5, 6)`, I could still:
- Call `fr.getValue()` ✓ (const method)
- Use `fr + fr` ✓ (returns a new fraction, doesn't modify fr)
- Use `fr == fr` ✓ (non-mutating comparison)
This is the payoff — objects that don't change still let you do useful things.

## Program Created

**Files:** 
- `cpp-practice-2026-05-29.h` — header with class declaration
- `cpp-practice-2026-05-29.cpp` — method implementations
- `cpp-practice-2026-05-29-main.cpp` — driver with demos

**What it does:**
1. **Fraction class** with full operator overloading
2. Constructor demos (default, two-arg, single-arg, auto-reduction)
3. Arithmetic operators (+, -, *, /)
4. Compound assignment (+=, -=, *=, /=)
5. Comparison operators — enables `sort()`, `min_element()`, `max_element()`
6. Unary + and -
7. Prefix/postfix increment and decrement
8. Stream operators (<< prints, >> reads from input)
9. Practical examples: Pythagorean triples, mixed numbers, recipe scaling, accumulators
10. const correctness demonstration

**Output highlights:**
```
Fraction(6, 8) auto-reduced to 3/4
x + y = 5/6 (= 0.8333)
Sorted using operator<: 1/4 1/2 3/4 1/1 5/4
1/2 == 2/4? YES (thanks to reduction!)
f1++ returns old value (1/2), f1 is now 3/2
++f2 returns reference to f2 (3/2)
Pythagorean 3/5 and 4/5: a² + b² = 1 ✓
```

## What was tricky
- **Postfix vs prefix** — the fake `int` parameter for postfix was new to me. Needs a dummy because the language can't otherwise tell them apart.
- **<< and >> must be non-members** — This took a moment to understand. If `Fraction` is the left operand in `cout << fr`, you'd have to call `fr.operator<<(cout)` which is awkward. Making it a non-member `operator<<(ostream&, Fraction&)` is cleaner and more idiomatic.
- **References from compound assignment** — Forgetting to return `*this` vs returning a copy was easy to miss. The reference version enables chaining.

## Next Steps / Areas to Explore
- Separate compilation (.h/.cpp files) — DONE! Today's practice used it
- **Inheritance and polymorphism** — next logical step after operator overloading
- Virtual functions and abstract base classes
- Smart pointers (unique_ptr, shared_ptr)
- Static class members in depth
- Template basics

## GitHub
- **Repo:** https://github.com/KanopiAndPetra/KanopiLearningCPPLessons
- **To commit:** Yes — program is complete and working
- **Committed:** 2026-05-29
