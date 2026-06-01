# C++ Practice Notes — May 27, 2026

## Session Info
- **Time:** 3:00 PM CDT
- **Topic:** Operator Overloading

## Repos Studied
- Review of OOP concepts from `Oppie1/1.1Oppie1CPP` (previous sessions)
- Built on class/constructor knowledge from May 23 session

## Key Concepts Learned

### What is Operator Overloading?
C++ lets you give operators (`+`, `-`, `<<`, `==`, etc.) custom meaning for your own classes.
Instead of writing `add(a, b)`, you can write `a + b`. Same result, cleaner syntax.

**Syntax:** You define a function named `operator<symbol>` inside your class.

---

### Stream Operators (`<<` and `>>`)

These are the most immediately useful overloads.

```cpp
// Must be a friend function (not a member) because left side isn't our object
friend ostream& operator<<(ostream& out, const Rational& r) {
    out << r.numerator << "/" << r.denominator;
    return out;  // return the stream to allow chaining: cout << a << b << c
}

friend istream& operator>>(istream& in, Rational& r) {
    char slash;
    int num, den;
    in >> num >> slash >> den;
    if (slash == '/') r = Rational(num, den);
    return in;
}
```

**Key insight:** `operator<<` returns `ostream&` so you can chain: `cout << a << b << c`.
Same for `operator>>` returning `istream&`.

---

### Arithmetic Operators (`+`, `-`, `*`, `/`)

Member functions — called on the left operand:

```cpp
Rational operator+(const Rational& other) const {
    int newNum = numerator * other.denominator + other.numerator * denominator;
    int newDen = denominator * other.denominator;
    return Rational(newNum, newDen);  // creates new object
}
```

**Important:** Don't modify `this` in arithmetic ops — return a new object with the result.

**Return type should be `const Rational`** to prevent weird things like `(a + b) = c`.

---

### Compound Assignment (`+=`, `-=`, `*=`, `/=`)

These DO modify `this`, so they return `Rational&`:

```cpp
Rational& operator+=(const Rational& other) {
    *this = *this + other;  // reuse the + operator
    return *this;           // return self for chaining: a += b += c
}
```

---

### Comparison Operators (`==`, `!=`, `<`, `>`, `<=`, `>=`)

```cpp
bool operator==(const Rational& other) const {
    return numerator == other.numerator && denominator == other.denominator;
}

bool operator!=(const Rational& other) const {
    return !(*this == other);  // reuse ==
}
```

**Tip:** Once you have `<`, you can define all others in terms of it:
```cpp
bool operator>(const Rational& other) const { return other < *this; }
bool operator<=(const Rational& other) const { return !(other < *this); }
bool operator>=(const Rational& other) const { return !(*this < other); }
```

**Sorting works!** Used `<` to bubble-sort an array of Rational objects. Great demo.

---

### Unary Operators (`-`, `+`, `++`, `--`)

**Unary minus:**
```cpp
Rational operator-() const {
    return Rational(-numerator, denominator);
}
```

**Prefix vs Postfix** — this was the trickiest part:
```cpp
// PREFIX ++r — increment, then return
Rational& operator++() {
    numerator += denominator;
    return *this;
}

// POSTFIX r++ — return old value, then increment
// The fake int parameter is the signal that this is postfix
Rational operator++(int) {
    Rational temp = *this;
    numerator += denominator;
    return temp;  // return the saved copy
}
```

Same pattern for `--`.

**Confusion I had:** postfix returns by value (a copy), prefix returns by reference.
Postfix is inherently less efficient (creates a copy) but necessary for `r++` semantics.

---

### Member vs Non-Member (Friend)

| Operator Type | Preferred As | Why |
|---|---|---|
| `a + b` | Member (left operand) | `a.operator+(b)` |
| `a += b` | Member | modifies `a` |
| `<<` (stream) | Friend non-member | left operand is `ostream`, not your class |
| `>>` (stream) | Friend non-member | left operand is `istream`, not your class |

**Friend functions** have access to private members even though they're not member functions.
`friend ostream& operator<<(ostream& out, const Rational& r)` can access `r.numerator` directly.

---

### Mixed-Type Operations

`r1 + 2` works because `2` is converted to `Rational(2)` via the constructor.
`2 + r1` does NOT work automatically — you'd need a non-member friend:
```cpp
friend Rational operator+(int i, const Rational& r) { return Rational(i) + r; }
```

---

## Program Created

**File:** `cpp-practice-2026-05-27.cpp`

Two classes built from scratch:

### Rational class — full operator overloading demo
- Reduces fractions automatically (GCD-based)
- Tracks numerator/denominator
- Demonstrates: `<<`, `>>`, `+`, `-`, `*`, `/`, `+=`, `-=`, `*=`, `/=`, `-` (unary), `+` (unary), `++` (pre/postfix), `--` (pre/postfix), `==`, `!=`, `<`, `>`, `<=`, `>=`
- Bonus: used `<` to bubble-sort an array of Rationals — sorting works!

### Vector2D class — simpler overload demo
- `+` for vector addition
- `*` for scalar multiplication (vector * double)
- `*` for dot product (vector * vector) — same operator, different parameter type!
- Demonstrates that operators can have different meanings based on argument type

## Key Takeaways

1. **Operator overloading is syntax sugar** — `a + b` is just `a.operator+(b)` with nicer syntax
2. **Return types matter** — `+` returns a new object, `+=` returns `*this` for chaining
3. **Prefix vs postfix** is the most subtle part — postfix returns a copy (less efficient)
4. **Friend functions** for `<<` and `>>` since the left operand isn't your class
5. **Comparison operators chain nicely** — once you have `==` and `<`, derive the rest
6. **Sorting works** with overloaded operators — a real array of Rationals sorted using `<`

## Errors I Hit

**Error:** `3 - r1` failed to compile because there's no `operator-(int, Rational)`.
- Member operators only work when the LEFT operand is your class
- Fix: use `Rational(3) - r1` explicitly, or define a non-member friend

## Next Steps / Areas to Explore
- Separate compilation (.h/.cpp files) — next logical step to organize larger programs
- Inheritance and polymorphism
- Virtual functions and abstract base classes
- Smart pointers (unique_ptr, shared_ptr)
- Templates and the Standard Template Library (STL)

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Committed:** 2026-05-27
