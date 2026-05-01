# C++ Practice Session — April 7, 2026

**Status:** COMPLETED — compiled and ran successfully

## What I Studied
- Operator overloading syntax and patterns
- Friend functions for stream operators
- Prefix vs postfix operator++
- Floating-point equality with epsilon

## What I Created
**File:** `workspace/learning/cpp_practice/cpp-practice-2026-04-07.cpp`

## Concepts Covered
- **operator+** — binary addition
- **operator-** — unary negation AND binary subtraction
- **operator*** — scalar multiplication
- **operator== and !=** — equality with epsilon (1e-9) for float comparison
- **operator+=** — compound assignment
- **operator++** — prefix (returns *this) vs postfix (returns copy)
- **operator[]** — subscript access
- **operator<< and >>** — friend functions for stream I/O

## Program Output
```
a + b = 4 + 6i
a - b = 2 + 2i
-a = -3 - 4i
a * 2 = 6 + 8i
a == b: true / a != c: true
++d = 6 + 6i / d++ = 6 + 6i (different!)
a[0] = 3 (real), |a| = 5 (sqrt(9+16))
a + b + c = 9 + 12i (chaining works!)
```

## Key Insights

**Friend function pattern for << and >>:**
```cpp
class Complex {
    friend ostream& operator<<(ostream& out, const Complex& c);
    friend istream& operator>>(istream& in, Complex& c);
};
// Definition separately — can access private members
```

**Prefix vs postfix:**
```cpp
Complex& operator++() { ++real; return *this; }     // prefix
Complex operator++(int) { Complex t = *this; ++real; return t; } // postfix
```

**Why operator overloading for DSR:**
Helix + Helix = combined helix, Manifold == Manifold = bool, instead of named methods everywhere.

## Telegram Delivery
⚠️ sessions_send timed out — message may arrive with delay.
