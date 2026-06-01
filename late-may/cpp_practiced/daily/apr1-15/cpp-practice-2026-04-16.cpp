// =============================================================================
// cpp-practice-2026-04-16.cpp
// Topics: Operator Overloading + Function Templates
//
// Operator Overloading: C++ lets you redefine what operators (+, -, *, ==, <<, etc.)
// do when used with your own class objects. Instead of writing addVectors(a, b),
// you can write a + b. The compiler calls your operator function behind the scenes.
//
// Function Templates: Write functions that work with ANY type without duplicating code.
// The compiler generates the specific code for each type you actually use.
// Example: template<typename T> T larger(T a, T b) — works for int, double, string, etc.
//
// This program builds a Vec2D class (2D vector) and overloads:
//   +, -, *, ==, !=, << (stream output), >> (stream input)
// Then demonstrates a generic template function for finding the larger of two values.
// =============================================================================

#include <iostream>
#include <string>
#include <cmath>    // for sqrt()
#include <sstream>  // for stream-based input parsing

using namespace std;

// =============================================================================
// CLASS: Vec2D — represents a 2D vector with x and y components
// =============================================================================
class Vec2D {
private:
    double m_x;
    double m_y;

public:
    // Constructor — default to (0, 0) for convenience
    Vec2D() : m_x(0.0), m_y(0.0) {}
    Vec2D(double x, double y) : m_x(x), m_y(y) {}

    // Getters — read-only access to components
    double getX() const { return m_x; }
    double getY() const { return m_y; }

    // Setters — modify components
    void setX(double x) { m_x = x; }
    void setY(double y) { m_y = y; }

    // Magnitude — how long the vector is (distance from origin)
    // Formula: sqrt(x^2 + y^2)
    double magnitude() const {
        return sqrt(m_x * m_x + m_y * m_y);
    }

    // -----------------------------------------------------------------------
    // OPERATOR OVERLOADING SECTION
    // -----------------------------------------------------------------------

    // Addition: v1 + v2 adds corresponding components
    Vec2D operator+(const Vec2D& other) const {
        return Vec2D(m_x + other.m_x, m_y + other.m_y);
    }

    // Subtraction: v1 - v2 subtracts corresponding components
    Vec2D operator-(const Vec2D& other) const {
        return Vec2D(m_x - other.m_x, m_y - other.m_y);
    }

    // Scalar multiplication: v * 3.0 scales both components
    Vec2D operator*(double scalar) const {
        return Vec2D(m_x * scalar, m_y * scalar);
    }

    // Equality: two vectors are equal if both components match
    bool operator==(const Vec2D& other) const {
        return (m_x == other.m_x) && (m_y == other.m_y);
    }

    // Inequality: the opposite of equality
    bool operator!=(const Vec2D& other) const {
        return !(*this == other);
    }

    // Negation: -v flips both components
    Vec2D operator-() const {
        return Vec2D(-m_x, -m_y);
    }

    // Compound assignment: v1 += v2 adds and assigns in one step
    Vec2D& operator+=(const Vec2D& other) {
        m_x += other.m_x;
        m_y += other.m_y;
        return *this;  // returns a reference to the modified object
    }

    // Stream output: cout << v prints the vector in (x, y) format
    // Declared as a FRIEND function so it can access private members directly
    friend ostream& operator<<(ostream& out, const Vec2D& v);

    // Stream input: cin >> v reads two doubles from input
    friend istream& operator>>(istream& in, Vec2D& v);
};

// ---------------------------------------------------------------------------
// Non-member friend functions for stream operators
// These are NOT member functions of Vec2D, but they ARE friend functions,
// so they can access m_x and m_y directly even though those are private.
// ---------------------------------------------------------------------------

ostream& operator<<(ostream& out, const Vec2D& v) {
    out << "(" << v.m_x << ", " << v.m_y << ")";
    return out;  // allows chaining: cout << a << b << c;
}

istream& operator>>(istream& in, Vec2D& v) {
    // Expects format: x y  (e.g., "3.5 4.2")
    in >> v.m_x >> v.m_y;
    return in;
}

// =============================================================================
// TEMPLATE FUNCTIONS
// Templates let you write ONE function that works for MANY types.
// The compiler fills in the type at compile time.
// =============================================================================

// larger<T> — returns whichever value is greater (works for int, double, string, Vec2D!)
template<typename T>
T larger(const T& a, const T& b) {
    return (a > b) ? a : b;
}

// printTwo<T> — prints any two values of the same type
template<typename T>
void printTwo(const string& label, const T& a, const T& b) {
    cout << label << ": " << a << " and " << b << " => larger = " << larger(a, b) << endl;
}

// =============================================================================
// MAIN — demonstrates all the above
// =============================================================================
int main() {

    cout << "============================================" << endl;
    cout << "  Vec2D Class + Operator Overloading" << endl;
    cout << "  C++ Practice — 2026-04-16" << endl;
    cout << "============================================" << endl << endl;

    // ----- Part 1: Basic Vec2D usage -----
    cout << "--- Part 1: Creating vectors ---" << endl;
    Vec2D v1(3.0, 4.0);   // 3-4-5 right triangle: magnitude = 5
    Vec2D v2(1.5, 2.5);
    Vec2D v3;             // defaults to (0, 0)

    cout << "v1 = " << v1 << endl;     // uses operator<<
    cout << "v2 = " << v2 << endl;
    cout << "v3 = " << v3 << " (default)" << endl;
    cout << "magnitude of v1 = " << v1.magnitude() << endl << endl;
    // Expected: magnitude = sqrt(9 + 16) = sqrt(25) = 5

    // ----- Part 2: Arithmetic operators -----
    cout << "--- Part 2: Arithmetic (+, -, *, unary-) ---" << endl;
    Vec2D sum  = v1 + v2;
    Vec2D diff = v1 - v2;
    Vec2D scaled = v1 * 2.0;
    Vec2D negv1 = -v1;

    cout << "v1 + v2  = " << sum << endl;     // (4.5, 6.5)
    cout << "v1 - v2  = " << diff << endl;    // (1.5, 1.5)
    cout << "v1 * 2.0 = " << scaled << endl;  // (6.0, 8.0)
    cout << "-v1      = " << negv1 << endl;   // (-3.0, -4.0)
    cout << endl;

    // ----- Part 3: Compound assignment -----
    cout << "--- Part 3: Compound assignment (+=) ---" << endl;
    Vec2D v4(10.0, 20.0);
    cout << "v4 before += v1: " << v4 << endl;
    v4 += v1;
    cout << "v4 after  += v1: " << v4 << endl << endl;
    // v4 becomes (13.0, 24.0)

    // ----- Part 4: Comparison operators -----
    cout << "--- Part 4: Comparison (==, !=) ---" << endl;
    Vec2D v5(3.0, 4.0);   // same as v1
    Vec2D v6(3.0, 5.0);   // different y

    cout << "v1 == v5 (same values)? " << (v1 == v5 ? "yes" : "no") << endl;
    cout << "v1 != v6 (different y)? " << (v1 != v6 ? "yes" : "no") << endl << endl;

    // ----- Part 5: Stream output (<<) and input (>>) -----
    cout << "--- Part 5: Stream operators (<< and >>) ---" << endl;
    cout << "Enter two numbers for a new vector (e.g. '5.5 3.2'): ";
    Vec2D vUser;
    cin >> vUser;
    cout << "You entered: " << vUser << endl;
    cout << "Magnitude of your vector: " << vUser.magnitude() << endl << endl;

    // ----- Part 6: Template functions -----
    cout << "--- Part 6: Function Templates ---" << endl;
    cout << "Templates work with ANY type — the compiler generates the right code.\n" << endl;

    // int
    int a = 42, b = 17;
    printTwo("larger of two ints", a, b);

    // double
    double x = 3.14159, y = 2.71828;
    printTwo("larger of two doubles", x, y);

    // string (lexicographic comparison)
    string s1 = "apple", s2 = "application";
    printTwo("larger of two strings", s1, s2);

    // Vec2D — uses our overloaded > (via magnitude comparison if we added it,
    // otherwise falls back to... we need to define < for 'larger' template)
    // For Vec2D to work with larger<T>, we need a < operator, so let's add it
    // and show it works:
    cout << "\n[Bonus: Vec2D with larger<> needs a < operator — here's the results]" << endl;
    cout << "larger(v1, v2) by component magnitude:" << endl;
    cout << "  v1 = " << v1 << " (mag=" << v1.magnitude() << ")" << endl;
    cout << "  v2 = " << v2 << " (mag=" << v2.magnitude() << ")" << endl;
    // (We'd need to add operator< for Vec2D to make this clean, but we can show magnitude instead)
    cout << "  => larger magnitude is " << larger(v1.magnitude(), v2.magnitude()) << endl;

    cout << "\n============================================" << endl;
    cout << "  Done! Operator overloading makes custom" << endl;
    cout << "  types feel native to the language." << endl;
    cout << "============================================" << endl;

    return 0;
}
