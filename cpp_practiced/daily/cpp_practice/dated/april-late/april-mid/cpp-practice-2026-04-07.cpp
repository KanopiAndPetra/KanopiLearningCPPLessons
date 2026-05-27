// cpp-practice-2026-04-07.cpp
// Practice: Operator Overloading
// Builds on: Virtual functions (April 5), File I/O (April 6)
// New concept: defining operators (+, -, ==, <<, >>) for custom types

#include <iostream>
#include <string>
#include <cmath>

using namespace std;

// ============================================================
// CLASS: Complex — complex number with operator overloading
// ============================================================
class Complex {
private:
    double real;
    double imag;

public:
    Complex(double r = 0, double i = 0) : real(r), imag(i) {}

    // ACCESSORS
    double getReal() const { return real; }
    double getImag() const { return imag; }

    // --- OPERATOR OVERLOADING ---

    // BINARY +: Complex + Complex
    Complex operator+(const Complex& other) const {
        return Complex(real + other.real, imag + other.imag);
    }

    // BINARY -: Complex - Complex
    Complex operator-(const Complex& other) const {
        return Complex(real - other.real, imag - other.imag);
    }

    // UNARY -: negate
    Complex operator-() const {
        return Complex(-real, -imag);
    }

    // SCALAR MULTIPLICATION: Complex * double
    Complex operator*(double scalar) const {
        return Complex(real * scalar, imag * scalar);
    }

    // EQUALITY: Complex == Complex
    bool operator==(const Complex& other) const {
        const double EPSILON = 1e-9;
        return fabs(real - other.real) < EPSILON &&
               fabs(imag - other.imag) < EPSILON;
    }

    // INEQUALITY: Complex != Complex
    bool operator!=(const Complex& other) const {
        return !(*this == other);
    }

    // COMPOUND ASSIGNMENT +=
    Complex& operator+=(const Complex& other) {
        real += other.real;
        imag += other.imag;
        return *this;
    }

    // PREFIX INCREMENT ++Complex
    Complex& operator++() {
        ++real;
        return *this;
    }

    // POSTFIX INCREMENT Complex++
    Complex operator++(int) {
        Complex temp = *this;
        ++real;
        return temp;
    }

    // SUBSCRIPT [] — 0 returns real, 1 returns imag
    double operator[](int index) const {
        if (index == 0) return real;
        return imag;
    }

    // MAGNITUDE
    double magnitude() const {
        return sqrt(real * real + imag * imag);
    }

    // FRIEND DECLARATIONS — stream operators need private access
    friend ostream& operator<<(ostream& out, const Complex& c);
    friend istream& operator>>(istream& in, Complex& c);
};

// ============================================================
// DEFINITION: operator<< — stream insertion
// ============================================================
ostream& operator<<(ostream& out, const Complex& c) {
    out << c.real;
    if (c.imag >= 0) {
        out << " + " << c.imag << "i";
    } else {
        out << " - " << -c.imag << "i";
    }
    return out;
}

// ============================================================
// DEFINITION: operator>> — stream extraction
// ============================================================
istream& operator>>(istream& in, Complex& c) {
    in >> c.real >> c.imag;
    return in;
}

// ============================================================
// DEMONSTRATION: Why operator overloading matters
// ============================================================
void demonstrateWhyItMatters() {
    cout << "=== Why Operator Overloading Matters ===" << endl;

    Complex a(3.0, 4.0);
    Complex b(1.0, 2.0);

    // Without overloading — verbose:
    // Complex sum = addComplex(a, b);  // hypothetical
    // or: a.add(b);

    // With overloading — natural math-like syntax:
    Complex sum = a + b;
    cout << "a + b = " << sum << " (written as 'a + b', not 'add(a, b)')" << endl;

    // == works naturally too
    Complex c(3.0, 4.0);
    cout << "a == c: " << (a == c ? "true" : "false") << endl;
}

// ============================================================
// DEMONSTRATION: All arithmetic operators
// ============================================================
void demonstrateAllOperators() {
    cout << "\n=== Arithmetic Operators ===" << endl;

    Complex a(3.0, 4.0);
    Complex b(1.0, 2.0);

    cout << "a = " << a << endl;
    cout << "b = " << b << endl;
    cout << "a + b = " << (a + b) << endl;
    cout << "a - b = " << (a - b) << endl;
    cout << "-a = " << (-a) << endl;
    cout << "a * 2 = " << (a * 2.0) << endl;

    Complex c = a;
    c += b;
    cout << "a += b → c = " << c << endl;

    Complex d(5.0, 6.0);
    cout << "\n++d = " << (++d) << endl;
    cout << "d++ = " << (d++) << endl;
    cout << "After postfix: d = " << d << endl;
}

// ============================================================
// DEMONSTRATION: Comparison operators
// ============================================================
void demonstrateComparison() {
    cout << "\n=== Comparison Operators ===" << endl;

    Complex a(1.0, 2.0);
    Complex b(1.0, 2.0);
    Complex c(1.0, 3.0);

    cout << "a == b: " << (a == b ? "true" : "false") << endl;
    cout << "a != c: " << (a != c ? "true" : "false") << endl;
    cout << "a == c: " << (a == c ? "true" : "false") << endl;
}

// ============================================================
// DEMONSTRATION: Subscript and magnitude
// ============================================================
void demonstrateSpecialOperators() {
    cout << "\n=== Special Operators ===" << endl;

    Complex a(3.0, 4.0);
    cout << "a[0] (real) = " << a[0] << endl;
    cout << "a[1] (imag) = " << a[1] << endl;
    cout << "|a| = " << a.magnitude() << " (sqrt(9+16) = 5)" << endl;
}

// ============================================================
// DEMONSTRATION: Operator chaining
// ============================================================
void demonstrateChaining() {
    cout << "\n=== Operator Chaining ===" << endl;

    Complex a(1.0, 2.0);
    Complex b(3.0, 4.0);
    Complex c(5.0, 6.0);

    Complex result = a + b + c;
    cout << "a + b + c = " << result << endl;

    result = (a * 2.0) + b - Complex(1.0, 0.0);
    cout << "(a * 2) + b - 1 = " << result << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: Operator Overloading" << endl;
    cout << "====================================" << endl;
    cout << endl;

    demonstrateWhyItMatters();
    demonstrateAllOperators();
    demonstrateComparison();
    demonstrateSpecialOperators();
    demonstrateChaining();

    cout << "\n====================================" << endl;
    cout << "Key concepts:" << endl;
    cout << "  - operator+ (binary)" << endl;
    cout << "  - operator- (unary and binary)" << endl;
    cout << "  - operator* (scalar)" << endl;
    cout << "  - operator== and !=" << endl;
    cout << "  - operator+=" << endl;
    cout << "  - operator++ (prefix and postfix)" << endl;
    cout << "  - operator[]" << endl;
    cout << "  - operator<< (friend, stream insertion)" << endl;
    cout << "  - operator>> (friend, stream extraction)" << endl;
    cout << "  - Why: makes custom types feel built-in" << endl;

    return 0;
}
