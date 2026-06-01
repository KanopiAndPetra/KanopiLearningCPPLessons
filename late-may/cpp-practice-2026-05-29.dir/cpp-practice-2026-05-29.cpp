#include "cpp-practice-2026-05-29.h"
#include <stdexcept>
#include <sstream>

// ─────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────

int Fraction::gcd(int a, int b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

void Fraction::reduce() {
    if (denominator == 0) return;
    int g = gcd(numerator, denominator);
    numerator /= g;
    denominator /= g;
    // Keep denominator positive
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
}

// ─────────────────────────────────────────
// Constructors
// ─────────────────────────────────────────

Fraction::Fraction() : numerator(0), denominator(1) {}

Fraction::Fraction(int num, int den) : numerator(num), denominator(den) {
    if (den == 0) {
        throw std::invalid_argument("Denominator cannot be zero");
    }
    reduce();
}

Fraction::Fraction(int whole) : numerator(whole), denominator(1) {}

// ─────────────────────────────────────────
// Getters
// ─────────────────────────────────────────

int Fraction::getNumerator() const { return numerator; }
int Fraction::getDenominator() const { return denominator; }

double Fraction::getValue() const {
    return static_cast<double>(numerator) / denominator;
}

std::string Fraction::toString() const {
    std::ostringstream oss;
    oss << numerator << "/" << denominator;
    return oss.str();
}

// ─────────────────────────────────────────
// Arithmetic operators
// ─────────────────────────────────────────

// a/b + c/d = (a*d + c*b) / (b*d)
Fraction Fraction::operator+(const Fraction& other) const {
    int num = numerator * other.denominator + other.numerator * denominator;
    int den = denominator * other.denominator;
    return Fraction(num, den);
}

// a/b - c/d = (a*d - c*b) / (b*d)
Fraction Fraction::operator-(const Fraction& other) const {
    int num = numerator * other.denominator - other.numerator * denominator;
    int den = denominator * other.denominator;
    return Fraction(num, den);
}

// a/b * c/d = (a*c) / (b*d)
Fraction Fraction::operator*(const Fraction& other) const {
    int num = numerator * other.numerator;
    int den = denominator * other.denominator;
    return Fraction(num, den);
}

// a/b / c/d = (a*d) / (b*c)
Fraction Fraction::operator/(const Fraction& other) const {
    if (other.numerator == 0) {
        throw std::invalid_argument("Cannot divide by zero");
    }
    int num = numerator * other.denominator;
    int den = denominator * other.numerator;
    return Fraction(num, den);
}

// ─────────────────────────────────────────
// Compound assignment operators
// ─────────────────────────────────────────

Fraction& Fraction::operator+=(const Fraction& other) {
    numerator = numerator * other.denominator + other.numerator * denominator;
    denominator = denominator * other.denominator;
    reduce();
    return *this;
}

Fraction& Fraction::operator-=(const Fraction& other) {
    numerator = numerator * other.denominator - other.numerator * denominator;
    denominator = denominator * other.denominator;
    reduce();
    return *this;
}

Fraction& Fraction::operator*=(const Fraction& other) {
    numerator *= other.numerator;
    denominator *= other.denominator;
    reduce();
    return *this;
}

Fraction& Fraction::operator/=(const Fraction& other) {
    if (other.numerator == 0) {
        throw std::invalid_argument("Cannot divide by zero");
    }
    numerator *= other.denominator;
    denominator *= other.numerator;
    reduce();
    return *this;
}

// ─────────────────────────────────────────
// Comparison operators
// ─────────────────────────────────────────

bool Fraction::operator==(const Fraction& other) const {
    // Already reduced, so just compare
    return numerator == other.numerator && denominator == other.denominator;
}

bool Fraction::operator!=(const Fraction& other) const {
    return !(*this == other);
}

bool Fraction::operator<(const Fraction& other) const {
    // a/b < c/d  ⟺  a*d < c*b  (when b,d > 0)
    return numerator * other.denominator < other.numerator * denominator;
}

bool Fraction::operator>(const Fraction& other) const {
    return other < *this;
}

bool Fraction::operator<=(const Fraction& other) const {
    return !(other < *this);
}

bool Fraction::operator>=(const Fraction& other) const {
    return !(*this < other);
}

// ─────────────────────────────────────────
// Unary operators
// ─────────────────────────────────────────

Fraction Fraction::operator-() const {
    return Fraction(-numerator, denominator);
}

Fraction Fraction::operator+() const {
    return *this; // unary plus, return copy
}

// ─────────────────────────────────────────
// Increment/decrement
// ─────────────────────────────────────────

// Prefix ++x  →  increment then return
Fraction& Fraction::operator++() {
    numerator += denominator;
    return *this;
}

Fraction& Fraction::operator--() {
    numerator -= denominator;
    return *this;
}

// Postfix x++  →  save old value, increment, return old
// The int dummy parameter distinguishes it from prefix
Fraction Fraction::operator++(int) {
    Fraction temp = *this;
    numerator += denominator;
    return temp;
}

Fraction Fraction::operator--(int) {
    Fraction temp = *this;
    numerator -= denominator;
    return temp;
}

// ─────────────────────────────────────────
// Stream operators
// ─────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, const Fraction& f) {
    os << f.numerator << '/' << f.denominator;
    return os;
}

std::istream& operator>>(std::istream& is, Fraction& f) {
    std::string s;
    is >> s;

    size_t slashPos = s.find('/');
    if (slashPos == std::string::npos) {
        is.setstate(std::ios::failbit);
        return is;
    }

    try {
        int num = std::stoi(s.substr(0, slashPos));
        int den = std::stoi(s.substr(slashPos + 1));
        f = Fraction(num, den);
    } catch (...) {
        is.setstate(std::ios::failbit);
    }
    return is;
}
