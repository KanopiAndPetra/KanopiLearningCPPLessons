// cpp-practice-2026-05-27.cpp
// Operator Overloading Practice
// Topics: arithmetic (+, -, *, /), comparison (==, !=, <, >),
//         stream (<<, >>), assignment (+=, -=), prefix/postfix (++, --)

#include <iostream>
#include <string>
#include <cmath>
using namespace std;

// ============================================================================
// Rational Number Class — demonstrates most operator overloading
// ============================================================================
class Rational {
private:
    int numerator;
    int denominator;

    // Helper: reduce fraction to simplest form
    void reduce() {
        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }
        int g = gcd(abs(numerator), denominator);
        if (g > 1) {
            numerator /= g;
            denominator /= g;
        }
    }

    int gcd(int a, int b) const {
        while (b != 0) {
            int temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

public:
    // Constructor
    Rational(int num = 0, int denom = 1) : numerator(num), denominator(denom) {
        if (denom == 0) {
            cout << "ERROR: denominator cannot be 0! Setting to 1.\n";
            denominator = 1;
        }
        reduce();
    }

    // ---- Getters ----
    int getNumerator()   const { return numerator; }
    int getDenominator() const { return denominator; }

    double toDouble() const {
        return static_cast<double>(numerator) / denominator;
    }

    // ---- Arithmetic Operators ----

    // Rational + Rational
    Rational operator+(const Rational& other) const {
        int newNum = numerator * other.denominator + other.numerator * denominator;
        int newDen = denominator * other.denominator;
        Rational result(newNum, newDen);
        return result;
    }

    // Rational - Rational
    Rational operator-(const Rational& other) const {
        int newNum = numerator * other.denominator - other.numerator * denominator;
        int newDen = denominator * other.denominator;
        Rational result(newNum, newDen);
        return result;
    }

    // Rational * Rational
    Rational operator*(const Rational& other) const {
        Rational result(numerator * other.numerator,
                        denominator * other.denominator);
        return result;
    }

    // Rational / Rational
    Rational operator/(const Rational& other) const {
        if (other.numerator == 0) {
            cout << "ERROR: division by zero!\n";
            return *this;
        }
        Rational result(numerator * other.denominator,
                        denominator * other.numerator);
        return result;
    }

    // ---- Compound Assignment Operators ----
    Rational& operator+=(const Rational& other) {
        *this = *this + other;
        return *this;
    }

    Rational& operator-=(const Rational& other) {
        *this = *this - other;
        return *this;
    }

    Rational& operator*=(const Rational& other) {
        *this = *this * other;
        return *this;
    }

    Rational& operator/=(const Rational& other) {
        *this = *this / other;
        return *this;
    }

    // ---- Unary Operators ----
    Rational operator-() const {
        return Rational(-numerator, denominator);
    }

    Rational operator+() const {
        return *this;
    }

    // Prefix ++ (e.g., ++r) — increment numerator, then return
    Rational& operator++() {
        numerator += denominator;
        reduce();
        return *this;
    }

    // Postfix ++ (e.g., r++) — save old value, increment, return old
    Rational operator++(int) {
        Rational temp = *this;
        numerator += denominator;
        reduce();
        return temp;
    }

    // Prefix -- (e.g., --r)
    Rational& operator--() {
        numerator -= denominator;
        reduce();
        return *this;
    }

    // Postfix -- (e.g., r--)
    Rational operator--(int) {
        Rational temp = *this;
        numerator -= denominator;
        reduce();
        return temp;
    }

    // ---- Comparison Operators ----
    bool operator==(const Rational& other) const {
        // Already reduced, so just compare
        return numerator == other.numerator && denominator == other.denominator;
    }

    bool operator!=(const Rational& other) const {
        return !(*this == other);
    }

    bool operator<(const Rational& other) const {
        return numerator * other.denominator < other.numerator * denominator;
    }

    bool operator>(const Rational& other) const {
        return other < *this;
    }

    bool operator<=(const Rational& other) const {
        return !(other < *this);
    }

    bool operator>=(const Rational& other) const {
        return !(*this < other);
    }

    // ---- Stream Operators (must be friend functions) ----
    friend ostream& operator<<(ostream& out, const Rational& r);
    friend istream& operator>>(istream& in, Rational& r);
};

// out << r  →  print like "3/4"
ostream& operator<<(ostream& out, const Rational& r) {
    if (r.denominator == 1) {
        out << r.numerator;           // print 5 instead of 5/1
    } else {
        out << r.numerator << "/" << r.denominator;
    }
    return out;
}

// in >> r  →  read two ints separated by /
istream& operator>>(istream& in, Rational& r) {
    char slash;
    int num, den;
    in >> num >> slash >> den;
    if (slash == '/') {
        r = Rational(num, den);
    } else {
        in.setstate(ios::failbit);  // mark stream as failed
    }
    return in;
}

// ============================================================================
// Vector2D Class — simpler demo with 2D vectors
// ============================================================================
class Vector2D {
private:
    double x, y;
public:
    Vector2D(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}

    // Vector addition
    Vector2D operator+(const Vector2D& other) const {
        return Vector2D(x + other.x, y + other.y);
    }

    // Scalar multiplication
    Vector2D operator*(double scalar) const {
        return Vector2D(x * scalar, y * scalar);
    }

    // Dot product
    double operator*(const Vector2D& other) const {
        return x * other.x + y * other.y;
    }

    // Magnitude
    double magnitude() const {
        return sqrt(x*x + y*y);
    }

    friend ostream& operator<<(ostream& out, const Vector2D& v);
};

// out << v → print "(x, y)"
ostream& operator<<(ostream& out, const Vector2D& v) {
    out << "(" << v.x << ", " << v.y << ")";
    return out;
}

// ============================================================================
// Demo functions
// ============================================================================

void printSection(const string& title) {
    cout << "\n" << string(60, '=') << "\n";
    cout << "  " << title << "\n";
    cout << string(60, '=') << "\n";
}

int main() {
    cout << "========================================================\n";
    cout << "  OPERATOR OVERLOADING PRACTICE — May 27, 2026\n";
    cout << "========================================================\n";

    // -------------------------------------------------------------------------
    printSection("1. STREAM OPERATOR << (output)");
    // -------------------------------------------------------------------------
    Rational r1(3, 4);
    Rational r2(5, 1);
    Rational r3(22, 7);
    cout << "r1 = " << r1  << "  (3/4)\n";
    cout << "r2 = " << r2  << "  (5/1 prints as 5)\n";
    cout << "r3 = " << r3  << "  (22/7 = 22/7, reduced)\n";
    cout << "r1 as double: " << r1.toDouble() << "\n";

    // -------------------------------------------------------------------------
    printSection("2. STREAM OPERATOR >> (input)");
    // -------------------------------------------------------------------------
    Rational rInput;
    cout << "Enter a rational number (format: a/b): ";
    if (cin >> rInput) {
        cout << "You entered: " << rInput << "\n";
    } else {
        cout << "Failed to read rational number.\n";
        cin.clear();
    }

    // -------------------------------------------------------------------------
    printSection("3. ARITHMETIC OPERATORS (+, -, *, /)");
    // -------------------------------------------------------------------------
    Rational a(1, 2);  // 1/2
    Rational b(1, 3);  // 1/3

    cout << "a = " << a << ", b = " << b << "\n";
    cout << "a + b = " << (a + b) << "\n";    // 1/2 + 1/3 = 5/6
    cout << "a - b = " << (a - b) << "\n";    // 1/2 - 1/3 = 1/6
    cout << "a * b = " << (a * b) << "\n";    // 1/2 * 1/3 = 1/6
    cout << "a / b = " << (a / b) << "\n";    // 1/2 / 1/3 = 3/2
    cout << endl;

    // Chain of operations
    Rational result = a + b * Rational(2, 1) - Rational(1, 4);
    cout << "a + b * 2 - 1/4 = " << result << "\n";

    // -------------------------------------------------------------------------
    printSection("4. COMPOUND ASSIGNMENT (+=, -=, *=, /=)");
    // -------------------------------------------------------------------------
    Rational r(3, 4);
    cout << "r starts as: " << r << "\n";
    r += Rational(1, 4);
    cout << "r += 1/4  →  " << r << "\n";
    r -= Rational(1, 2);
    cout << "r -= 1/2  →  " << r << "\n";
    r *= Rational(2, 1);
    cout << "r *= 2    →  " << r << "\n";
    r /= Rational(3, 1);
    cout << "r /= 3    →  " << r << "\n";

    // -------------------------------------------------------------------------
    printSection("5. UNARY OPERATORS (-, +, ++, --)");
    // -------------------------------------------------------------------------
    Rational neg = -r1;
    cout << "Unary -: -(" << r1 << ") = " << neg << "\n";
    cout << "Unary +: +(" << r1 << ") = " << +r1 << "\n";

    Rational pre(3, 4);
    cout << "\nPrefix ++: starting with " << pre << "\n";
    cout << "  ++pre  = " << ++pre << "  (pre is now " << pre << ")\n";

    Rational post(3, 4);
    cout << "\nPostfix ++: starting with " << post << "\n";
    cout << "  post++ = " << post++ << "  (post is now " << post << ")\n";

    Rational dec(5, 4);
    cout << "\nPrefix --: starting with " << dec << "\n";
    cout << "  --dec  = " << --dec << "  (dec is now " << dec << ")\n";

    // -------------------------------------------------------------------------
    printSection("6. COMPARISON OPERATORS (==, !=, <, >, <=, >=)");
    // -------------------------------------------------------------------------
    Rational c(1, 2);
    Rational d(2, 4);   // same as 1/2, will reduce
    Rational e(3, 4);

    cout << "c = " << c << ", d = " << d << " (2/4 reduces to 1/2), e = " << e << "\n";
    cout << boolalpha;
    cout << "c == d ? " << (c == d) << "   (1/2 == 1/2)\n";
    cout << "c != e ? " << (c != e) << "   (1/2 != 3/4)\n";
    cout << "c <  e ? " << (c < e)  << "   (1/2 < 3/4)\n";
    cout << "c >  e ? " << (c > e)  << "   (1/2 > 3/4 is false)\n";
    cout << "c <= d ? " << (c <= d) << "   (1/2 <= 1/2)\n";
    cout << "c >= e ? " << (c >= e) << "   (1/2 >= 3/4 is false)\n";

    // Sort a small array of Rationals
    Rational arr[4] = { Rational(3,1), Rational(1,4), Rational(5,2), Rational(1,1) };
    cout << "\nBefore sorting: ";
    for (int i = 0; i < 4; i++) cout << arr[i] << " ";
    cout << "\n";

    // Bubble sort using <
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3 - i; j++) {
            if (arr[j] > arr[j+1]) {
                Rational tmp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }
    cout << "After sorting:  ";
    for (int i = 0; i < 4; i++) cout << arr[i] << " ";
    cout << "\n";

    // -------------------------------------------------------------------------
    printSection("7. Vector2D — operator* for scalar & dot product");
    // -------------------------------------------------------------------------
    Vector2D v1(3.0, 4.0);
    Vector2D v2(1.0, 2.0);
    cout << "v1 = " << v1 << ", magnitude = " << v1.magnitude() << "  (3-4-5 triangle!)\n";
    cout << "v2 = " << v2 << "\n";
    cout << "v1 + v2  = " << (v1 + v2) << "\n";
    cout << "v1 * 2   = " << (v1 * 2.0) << "  (scalar multiply)\n";
    cout << "v1 * v2  = " << (v1 * v2) << "  (dot product = 3*1 + 4*2)\n";

    // -------------------------------------------------------------------------
    printSection("8. CONVERSION + MIXED OPERATIONS");
    // -------------------------------------------------------------------------
    // Rational + int (int converts to Rational via constructor)
    Rational mixed = r1 + 2;   // 3/4 + 2 = 11/4
    cout << "r1 + 2 = " << mixed << "  (Rational + int, int promoted to Rational)\n";

    mixed = Rational(3) - r1;  // 3 - 3/4 = 9/4
    cout << "3 - r1 = " << mixed << "  (using Rational(3) - r1)\n";

    cout << "\n========================================================\n";
    cout << "  All demos complete!\n";
    cout << "========================================================\n";

    return 0;
}
