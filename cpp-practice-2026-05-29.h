#ifndef FRACTION_H
#define FRACTION_H

#include <iostream>
#include <string>

class Fraction {
private:
    int numerator;
    int denominator;

    void reduce();          // reduce to lowest terms
    int gcd(int a, int b);  // greatest common divisor

public:
    // Constructors
    Fraction();
    Fraction(int num, int den);
    Fraction(int whole);

    // Getters (const-correct)
    int getNumerator() const;
    int getDenominator() const;
    double getValue() const;

    // Display helpers
    std::string toString() const;

    // OPERATOR OVERLOADING
    // Arithmetic operators
    Fraction operator+(const Fraction& other) const;
    Fraction operator-(const Fraction& other) const;
    Fraction operator*(const Fraction& other) const;
    Fraction operator/(const Fraction& other) const;

    // Compound assignment
    Fraction& operator+=(const Fraction& other);
    Fraction& operator-=(const Fraction& other);
    Fraction& operator*=(const Fraction& other);
    Fraction& operator/=(const Fraction& other);

    // Comparison operators
    bool operator==(const Fraction& other) const;
    bool operator!=(const Fraction& other) const;
    bool operator<(const Fraction& other) const;
    bool operator>(const Fraction& other) const;
    bool operator<=(const Fraction& other) const;
    bool operator>=(const Fraction& other) const;

    // Unary operators
    Fraction operator-() const;  // negation
    Fraction operator+() const;  // unary plus (return copy)

    // Increment/decrement
    Fraction& operator++();     // prefix ++
    Fraction& operator--();     // prefix --
    Fraction operator++(int);   // postfix ++
    Fraction operator--(int);   // postfix --

    // Stream operators (must be non-members to work with << and >>)
    friend std::ostream& operator<<(std::ostream& os, const Fraction& f);
    friend std::istream& operator>>(std::istream& is, Fraction& f);
};

#endif // FRACTION_H
