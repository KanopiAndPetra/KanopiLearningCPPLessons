#include "cpp-practice-2026-05-29.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>

using namespace std;

void printSection(const string& title) {
    cout << "\n" << string(50, '=') << "\n";
    cout << "  " << title << "\n";
    cout << string(50, '=') << "\n\n";
}

int main() {
    cout << fixed << setprecision(4);

    // ═══════════════════════════════════════
    // BASIC SETUP & CONSTRUCTORS
    // ═══════════════════════════════════════
    printSection("1. CONSTRUCTORS & BASIC DISPLAY");

    Fraction a;                  // default: 0/1
    Fraction b(3, 4);             // 3/4
    Fraction c(5);                // 5/1 = 5
    Fraction d(6, 8);             // should reduce to 3/4

    cout << "Default (no args):     " << a << endl;
    cout << "Fraction(3, 4):       " << b << endl;
    cout << "Fraction(5):         " << c << endl;
    cout << "Fraction(6, 8):       " << d << "  (auto-reduced to 3/4)\n";
    cout << "c as double:          " << c.getValue() << endl;

    // Verify reduction works
    cout << "\nReduction check: b == d? " << (b == d ? "YES" : "NO") << endl;

    // Try bad denominator
    cout << "\nCatching divide-by-zero at runtime:\n";
    try {
        Fraction bad(1, 0);
    } catch (const std::exception& e) {
        cout << "  Caught: " << e.what() << endl;
    }

    // ═══════════════════════════════════════
    // ARITHMETIC OPERATORS
    // ═══════════════════════════════════════
    printSection("2. ARITHMETIC OPERATORS (+, -, *, /)");

    Fraction x(1, 2);  // 1/2
    Fraction y(1, 3);  // 1/3

    cout << "x = " << x << ", y = " << y << "\n\n";

    Fraction sum    = x + y;       // 1/2 + 1/3 = 5/6
    Fraction diff   = x - y;       // 1/2 - 1/3 = 1/6
    Fraction prod   = x * y;       // 1/2 * 1/3 = 1/6
    Fraction quot   = x / y;       // 1/2 / 1/3 = 3/2

    cout << "x + y = " << sum  << "  (= " << sum.getValue()  << ")\n";
    cout << "x - y = " << diff << "  (= " << diff.getValue() << ")\n";
    cout << "x * y = " << prod << "  (= " << prod.getValue() << ")\n";
    cout << "x / y = " << quot << "  (= " << quot.getValue() << ")\n";

    cout << "\nChain of operations:\n";
    Fraction result = Fraction(1, 2) + Fraction(1, 4) - Fraction(1, 8);
    cout << "  1/2 + 1/4 - 1/8 = " << result << "  (= " << result.getValue() << ")\n";

    // Test division by zero catches
    cout << "\nCatching divide-by-zero:\n";
    try {
        Fraction zero(0, 1);
        Fraction bad = x / zero;
    } catch (const std::exception& e) {
        cout << "  Caught: " << e.what() << endl;
    }

    // ═══════════════════════════════════════
    // COMPOUND ASSIGNMENT
    // ═══════════════════════════════════════
    printSection("3. COMPOUND ASSIGNMENT (+=, -=, *=, /=)");

    Fraction p(3, 5);
    cout << "Starting with p = " << p << endl;

    p += Fraction(1, 5);  cout << "p += 1/5  → " << p << endl;
    p -= Fraction(1, 10); cout << "p -= 1/10 → " << p << endl;
    p *= Fraction(2, 3);  cout << "p *= 2/3  → " << p << endl;
    p /= Fraction(3, 4);  cout << "p /= 3/4  → " << p << endl;

    // Show += returns the updated object
    Fraction q(1, 4);
    Fraction& ref = (q += Fraction(1, 4));  // q is now 1/2
    cout << "\nq = " << q << ", ref points to q? " << ((&ref == &q) ? "YES" : "NO") << endl;

    // ═══════════════════════════════════════
    // COMPARISON OPERATORS
    // ═══════════════════════════════════════
    printSection("4. COMPARISON OPERATORS (==, !=, <, >, <=, >=)");

    vector<Fraction> fractions = {
        Fraction(1, 4),
        Fraction(1, 2),
        Fraction(3, 4),
        Fraction(1, 1),
        Fraction(5, 4),
    };

    cout << "Fractions: ";
    for (const auto& f : fractions) cout << f << " ";
    cout << "\n";

    // Sort using <
    cout << "\nSorted (using operator<):\n  ";
    sort(fractions.begin(), fractions.end());
    for (const auto& f : fractions) cout << f << " ";
    cout << "\n";

    cout << "\nEquality check:\n";
    cout << "  1/2 == 2/4 ? " << (Fraction(1,2) == Fraction(2,4) ? "YES" : "NO") << "\n";
    cout << "  1/2 == 1/3 ? " << (Fraction(1,2) == Fraction(1,3) ? "YES" : "NO") << "\n";
    cout << "  1/2 != 1/3 ? " << (Fraction(1,2) != Fraction(1,3) ? "YES" : "NO") << "\n";

    cout << "\nOrdering:\n";
    cout << "  1/4 < 1/2 ? " << (Fraction(1,4) < Fraction(1,2)  ? "YES" : "NO") << "\n";
    cout << "  3/4 > 1/2 ? " << (Fraction(3,4) > Fraction(1,2)  ? "YES" : "NO") << "\n";
    cout << "  1/2 <= 2/4 ? " << (Fraction(1,2) <= Fraction(2,4) ? "YES" : "NO") << "\n";
    cout << "  1/2 >= 2/4 ? " << (Fraction(1,2) >= Fraction(2,4) ? "YES" : "NO") << "\n";

    // Find min/max
    auto minFrac = min_element(fractions.begin(), fractions.end());
    auto maxFrac = max_element(fractions.begin(), fractions.end());
    cout << "\nMin: " << *minFrac << ", Max: " << *maxFrac << "\n";

    // ═══════════════════════════════════════
    // UNARY OPERATORS
    // ═══════════════════════════════════════
    printSection("5. UNARY OPERATORS (-, +)");

    Fraction orig(3, 4);
    cout << "orig = " << orig << endl;
    cout << "-orig = " << -orig << endl;
    cout << "+orig = " << +orig << " (unary plus, same value)\n";
    cout << "orig unchanged after unary ops: " << orig << "\n";

    // Negate a negative
    Fraction neg(-3, 4);
    cout << "\nneg = " << neg << endl;
    cout << "-neg = " << -neg << endl;

    // ═══════════════════════════════════════
    // INCREMENT/DECREMENT
    // ═══════════════════════════════════════
    printSection("6. INCREMENT/DECREMENT (++, --)");

    Fraction n(3, 4);
    cout << "Starting: n = " << n << "\n\n";

    cout << "Prefix ++n:\n";
    cout << "  Before: " << n << endl;
    cout << "  Result of ++n: " << ++n << endl;
    cout << "  After:  " << n << "\n\n";

    cout << "Postfix n++:\n";
    cout << "  Before: " << n << endl;
    cout << "  Result of n++: " << n++ << endl;
    cout << "  After:  " << n << "\n\n";

    cout << "Prefix --n:\n";
    cout << "  Before: " << n << endl;
    cout << "  Result of --n: " << --n << endl;
    cout << "  After:  " << n << "\n\n";

    cout << "Postfix n--:\n";
    cout << "  Before: " << n << endl;
    cout << "  Result of n--: " << n-- << endl;
    cout << "  After:  " << n << "\n\n";

    // Show the difference between prefix and postfix
    Fraction f1(1, 2);
    Fraction f2(1, 2);
    Fraction postResult = f1++;
    Fraction& preRef   = ++f2;
    cout << "f1++ returns old value (" << postResult << "), f1 is now " << f1 << "\n";
    cout << "++f2 returns reference to f2 (" << preRef << "), f2 is " << f2 << "\n";

    // ═══════════════════════════════════════
    // STREAM OPERATORS (<< and >>)
    // ═══════════════════════════════════════
    printSection("7. STREAM OPERATORS (<< output, >> input)");

    Fraction fr(7, 8);
    cout << "Output with << : " << fr << " (note: no std::endl used)\n";

    cout << "\nEnter a fraction in the format numerator/denominator: ";
    Fraction userFr;
    if (cin >> userFr) {
        cout << "Read successfully: " << userFr << " (≈ " << userFr.getValue() << ")\n";
    } else {
        cout << "Failed to read fraction.\n";
        cin.clear();
    }

    // Multi-line input demo
    cout << "\nTrying to read '2/3 + 3/4' (invalid format): ";
    Fraction bad;
    if (cin >> bad) {
        cout << "Read: " << bad << "\n";
    } else {
        cout << "Correctly rejected invalid input.\n";
        cin.clear();
    }

    // ═══════════════════════════════════════
    // PRACTICAL USES
    // ═══════════════════════════════════════
    printSection("8. PRACTICAL EXAMPLES");

    // Pythagorean theorem with fractions: a² + b² = c²
    // 3/5 and 4/5 are a Pythagorean triple
    cout << "Pythagorean check for 3/5 and 4/5:\n";
    Fraction a2(3, 5);
    Fraction b2(4, 5);
    Fraction aSqr = a2 * a2;  // 9/25
    Fraction bSqr = b2 * b2;  // 16/25
    Fraction cSqr = aSqr + bSqr;
    Fraction cActual(5, 5);  // c² should be 1
    cout << "  a = " << a2 << ", a² = " << aSqr << "\n";
    cout << "  b = " << b2 << ", b² = " << bSqr << "\n";
    cout << "  a² + b² = " << cSqr << " (= " << cSqr.getValue() << ")\n";
    cout << "  c (expected 1): " << cActual << " (== " << cSqr << "? " << (cActual == cSqr ? "YES ✓" : "NO") << ")\n";

    // Working with mixed numbers (represented as improper fractions)
    cout << "\nMixed number arithmetic support:\n";
    Fraction mixed1(7, 4);   // a mixed number like 1 3/4 represented as 7/4
    Fraction half(1, 2);
    cout << "  7/4 + 1/2 = " << (mixed1 + half) << " (= " << (mixed1 + half).getValue() << ")\n";
    cout << "  7/4 - 1/2 = " << (mixed1 - half) << " (= " << (mixed1 - half).getValue() << ")\n";
    cout << "  7/4 × 2 = " << (mixed1 * Fraction(2)) << " (= " << (mixed1 * Fraction(2)).getValue() << ")\n";

    // Recipe scaling (fractions in the kitchen)
    cout << "\nRecipe scaling with Fraction:\n";
    Fraction flour(3, 4);   // 3/4 cup
    Fraction scale(2, 1);    // double
    cout << "  Original flour: " << flour << " cup\n";
    cout << "  Scaled x2: " << (flour * scale) << " cups\n";

    // Accumulator pattern
    cout << "\nAccumulating multiple fractions:\n";
    Fraction acc(0);
    vector<Fraction> parts = {Fraction(1, 8), Fraction(1, 4), Fraction(1, 8), Fraction(1, 4)};
    cout << "  Parts: ";
    for (auto p : parts) cout << p << " ";
    cout << "\n  Sum: ";
    for (auto& p : parts) acc += p;
    cout << acc << " (= " << acc.getValue() << ")\n";

    // ═══════════════════════════════════════
    // const CORRECTNESS DEMONSTRATION
    // ═══════════════════════════════════════
    printSection("9. const CORRECTNESS WITH OPERATORS");

    const Fraction constFr(5, 6);  // const object
    cout << "const Fraction fr(5, 6) = " << constFr << endl;
    cout << "fr.getValue() = " << constFr.getValue() << endl;
    cout << "fr.toString() = \"" << constFr.toString() << "\"" << endl;
    cout << "fr + fr = " << (constFr + constFr) << endl;  // temp result
    cout << "fr == fr ? " << (constFr == constFr ? "YES" : "NO") << endl;
    cout << "fr < fr ? " << (constFr < constFr ? "YES" : "NO") << endl;

    // Key insight: const object can use const methods, non-mutating operators
    const Fraction constResult = constFr + Fraction(1, 6);  // 5/6 + 1/6 = 1
    cout << "\nconst result of constFr + 1/6 = " << constResult << "\n";

    // ═══════════════════════════════════════
    // SUMMARY
    // ═══════════════════════════════════════
    printSection("SUMMARY: Operator Overloading Key Concepts");

    cout <<
        "1. Member vs Non-Member:\n"
        "   - ARITHMETIC (+ - * /)    → member functions  (left operand is 'this')\n"
        "   - COMPOUND (+= -= *= /=)  → member functions  (modify *this)\n"
        "   - COMPARISON              → member functions\n"
        "   - << and >>               → NON-members (left operand is ostream/istream)\n\n"
        "2. const Correctness:\n"
        "   - Methods that don't modify state → mark const\n"
        "   - const objects can only call const methods\n"
        "   - Arithmetic operators return NEW fractions (no mutation)\n\n"
        "3. Compound Assignment:\n"
        "   - a += b  is equivalent to  a = a + b\n"
        "   - a + b   returns a NEW fraction (doesn't change a)\n"
        "   - a += b  changes a in-place and returns reference\n\n"
        "4. Prefix vs Postfix:\n"
        "   - prefix (++x, --x)  → increment, return *this (by reference)\n"
        "   - postfix (x++, x--)  → copy old value, increment, return COPY\n"
        "   - Use prefix in loops! (avoids copy)\n\n"
        "5. stream operators:\n"
        "   - operator<< for output (std::ostream& as first param)\n"
        "   - operator>> for input  (std::istream& as first param)\n"
        "   - Often made friends to access private members\n\n"
        "6. Comparison pattern for sorting:\n"
        "   - Define operator<  → enables sort(), min_element(), max_element()\n"
        "   - Define operator== → complements operator!= and with < gives <= >=\n\n"
        "   - Consider returning bool for simple comparisons\n\n"
        "7. Reducing fractions:\n"
        "   - Always reduce after construction\n"
        "   - GCD to find greatest common divisor\n"
        "   - Makes operator== trivial (already in lowest terms)\n\n"
        ;

    cout << "Goodbye from Fraction demo!\n";
    return 0;
}
