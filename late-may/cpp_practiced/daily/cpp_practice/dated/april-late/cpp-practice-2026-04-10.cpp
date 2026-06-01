// cpp-practice-2026-04-10.cpp
// Practice: Iterators & std::complex
// Builds on: STL Containers (April 9), Templates (April 9)
// New concepts: iterator categories, std::complex, angle computation
// Direct DSR connection: phase angle θ = [Δ(n)]³ mod 2π for twin prime pairs

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <string>
#include <cmath>
#include <complex>
#include <sstream>

using namespace std;
const double PI = 3.14159265358979323846;

// ============================================================
// PRIME UTILITIES
// ============================================================
bool isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int i = 3; i <= sqrt(n); i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

int primeCount(int n) {
    int count = 0;
    for (int i = 2; i <= n; i++) {
        if (isPrime(i)) count++;
    }
    return count;
}

double chebyshevPsi(double n) {
    if (n < 1) return 0;
    return n / log(n);
}

// Δ(n) = π(n) - ψ(n)
double delta(int n) {
    return static_cast<double>(primeCount(n)) - chebyshevPsi(static_cast<double>(n));
}

// ============================================================
// PART 1: Iterator Categories & Manual Traversal
// ============================================================
void demonstrateIterators() {
    cout << "=== Iterator Categories ===" << endl;

    vector<int> primes;
    for (int i = 2; i <= 100; i++) {
        if (isPrime(i)) primes.push_back(i);
    }

    // Iterator categories:
    // InputIterator  — read, advance
    // OutputIterator — write, advance
    // ForwardIterator — multiple passes
    // BidirectionalIterator — ++ and --
    // RandomAccessIterator — pointer arithmetic [n]
    // ContiguousIterator — memory-adjacent (C++17)

    // Manual traversal with iterators
    cout << "Manual iterator traversal: ";
    for (vector<int>::iterator it = primes.begin(); it != primes.end(); ++it) {
        cout << *it << " ";
    }
    cout << endl;

    // const_iterator — read-only traversal
    cout << "Const iterator (reverse): ";
    for (vector<int>::const_reverse_iterator rit = primes.rbegin();
         rit != primes.rend(); ++rit) {
        cout << *rit << " ";
    }
    cout << endl;

    // Bidirectional: advance forward and back
    auto it = primes.begin() + 5;
    cout << "6th prime: " << *it << endl;
    advance(it, 3);
    cout << "After advance(3): " << *it << endl;
    next(it, -2);
    cout << "After next(-2): " << *next(it, -2) << endl;

    // Random access
    auto first = primes.begin();
    auto last = primes.begin() + 10;
    cout << "Distance(first, first+10): " << distance(first, last) << endl;

    cout << endl;
}

// ============================================================
// PART 2: std::complex — complex number arithmetic
// ============================================================
void demonstrateComplex() {
    cout << "=== std::complex ===" << endl;

    // std::complex<T> — complex number with type T
    complex<double> z1(3.0, 4.0);  // 3 + 4i
    complex<double> z2(1.0, -2.0); // 1 - 2i

    cout << "z1 = " << z1 << endl;
    cout << "z2 = " << z2 << endl;
    cout << "z1 + z2 = " << z1 + z2 << endl;
    cout << "z1 * z2 = " << z1 * z2 << endl;
    cout << "z1 / z2 = " << z1 / z2 << endl;

    // Polar form
    double r = abs(z1);      // magnitude
    double theta = arg(z1);  // phase angle in radians
    cout << "|z1| = " << r << endl;
    cout << "arg(z1) = " << theta << " rad = " << theta * 180 / PI << " deg" << endl;

    // Euler's formula
    complex<double> exp_i_theta = polar(1.0, theta);
    cout << "e^(i*theta) = " << exp_i_theta << endl;

    // Verify: e^(i*theta) = cos(theta) + i*sin(theta)
    complex<double> euler = complex<double>(cos(theta), sin(theta));
    cout << "cos(theta) + i*sin(theta) = " << euler << endl;

    // conj — complex conjugate
    cout << "conj(z1) = " << conj(z1) << endl;
    cout << "norm(z1) = " << norm(z1) << " (|z|^2)" << endl;

    // Phase angle modulo 2π
    double theta_mod_2pi = fmod(theta, 2 * PI);
    if (theta_mod_2pi < 0) theta_mod_2pi += 2 * PI;
    cout << "theta mod 2π = " << theta_mod_2pi << endl;

    cout << endl;
}

// ============================================================
// PART 3: Iterator + Complex — Twin Prime Phase Analysis
// ============================================================
void demonstrateTwinPrimePhaseAnalysis() {
    cout << "=== DSR: Twin Prime Phase Angle Analysis ===" << endl;

    // For each twin prime pair (p, p+2):
    // Compute Ω²(n) = [Δ(n)]³ · exp(i·[Δ(n)]³)
    // Extract phase angle θ = arg(Ω²) mod 2π
    // Check: do partners have systematically different phase angles?

    struct PrimeOmega {
        int n;
        double delta;
        complex<double> omega2;
        double phase;  // theta mod 2π
    };

    vector<PrimeOmega> twinPrimeData;

    for (int n = 3; n <= 500; n++) {
        if (isPrime(n) && isPrime(n + 2)) {
            double d = delta(n);
            double d3 = d * d * d;

            // Ω²(n) = [Δ]³ · exp(i·[Δ]³)
            complex<double> omega2 = pow(d, 3) * complex<double>(cos(d3), sin(d3));
            double phase = fmod(arg(omega2), 2 * PI);
            if (phase < 0) phase += 2 * PI;

            twinPrimeData.push_back({n, d, omega2, phase});
        }
    }

    // Print phase angles for twin prime pairs
    cout << "Twin prime phase angles (first 20 pairs):" << endl;
    cout << "Pair      Δ(n)     Δ(n+2)    Phase(n)  Phase(n+2)  ΔPhase(π)" << endl;

    int count = 0;
    for (const auto& tp : twinPrimeData) {
        double d2 = delta(tp.n + 2);
        double d3_2 = d2 * d2 * d2;
        complex<double> omega2_2 = pow(d2, 3) * complex<double>(cos(d3_2), sin(d3_2));
        double phase2 = fmod(arg(omega2_2), 2 * PI);
        if (phase2 < 0) phase2 += 2 * PI;

        double deltaPhase = fabs(tp.phase - phase2);
        // Normalize to [0, π]
        if (deltaPhase > PI) deltaPhase = 2 * PI - deltaPhase;
        double deltaPhaseNorm = deltaPhase / PI;  // in units of π

        printf("%3d,%3d   %+7.3f   %+7.3f   %6.3fπ    %6.3fπ    %5.2fπ\n",
               tp.n, tp.n + 2, tp.delta, d2,
               tp.phase / PI, phase2 / PI, deltaPhaseNorm);

        if (++count >= 20) break;
    }

    // Statistical summary
    cout << "\nPhase difference statistics (all pairs up to n=500):" << endl;
    vector<double> phaseDiffs;
    for (const auto& tp : twinPrimeData) {
        double d2 = delta(tp.n + 2);
        double d3_2 = d2 * d2 * d2;
        complex<double> omega2_2 = pow(d2, 3) * complex<double>(cos(d3_2), sin(d3_2));
        double phase2 = fmod(arg(omega2_2), 2 * PI);
        if (phase2 < 0) phase2 += 2 * PI;

        double deltaPhase = fabs(tp.phase - phase2);
        if (deltaPhase > PI) deltaPhase = 2 * PI - deltaPhase;
        phaseDiffs.push_back(deltaPhase / PI);  // normalized to π
    }

    double mean = 0;
    for (double d : phaseDiffs) mean += d;
    mean /= phaseDiffs.size();

    int near0 = 0, nearPi2 = 0, nearPi = 0;
    for (double d : phaseDiffs) {
        if (d < 0.2) near0++;
        else if (d > 0.8 && d < 1.2) nearPi++;
        else if (d > 0.3 && d < 0.7) nearPi2++;
    }

    printf("Mean phase diff: %.3fπ (%.1f deg)\n", mean, mean * 180);
    printf("Near 0π (same phase): %zu/%zu (%.1f%%)\n", near0, phaseDiffs.size(), 100.0 * near0 / phaseDiffs.size());
    printf("Near π/2 (quadrature): %zu/%zu (%.1f%%)\n", nearPi2, phaseDiffs.size(), 100.0 * nearPi2 / phaseDiffs.size());
    printf("Near π (opposite): %zu/%zu (%.1f%%)\n", nearPi, phaseDiffs.size(), 100.0 * nearPi / phaseDiffs.size());

    if (nearPi > phaseDiffs.size() * 0.4) {
        cout << "\n→ INTERPRETATION: Significant near-π phase separation detected." << endl;
        cout << "→ Twin prime partners tend toward opposite phase — STRAND SIGNAL." << endl;
    } else if (near0 > phaseDiffs.size() * 0.4) {
        cout << "\n→ INTERPRETATION: Twin primes mostly share the same phase." << endl;
        cout << "→ Phase separation is NOT coming from simple twin pairing." << endl;
    } else {
        cout << "\n→ INTERPRETATION: Phase differences are distributed — no strong pattern." << endl;
    }

    cout << endl;
}

// ============================================================
// PART 4: Iterator Adaptors — inserter, back_inserter
// ============================================================
void demonstrateIteratorAdaptors() {
    cout << "=== Iterator Adaptors ===" << endl;

    // back_inserter — creates a back-insert iterator
    vector<int> v1 = {1, 2, 3};
    vector<int> v2;

    // Copy elements using back_inserter
    copy(v1.begin(), v1.end(), back_inserter(v2));
    cout << "After copy with back_inserter: ";
    for (int x : v2) cout << x << " ";
    cout << endl;

    // front_inserter — prepends to front (requires list/deque)
    deque<int> d1 = {1, 2, 3};
    deque<int> d2;
    copy(d1.begin(), d1.end(), front_inserter(d2));
    cout << "After copy with front_inserter: ";
    for (int x : d2) cout << x << " ";
    cout << endl;

    // transform with inserter
    vector<double> primes = {2, 3, 5, 7, 11, 13};
    vector<double> deltaVals;
    transform(primes.begin(), primes.end(), back_inserter(deltaVals),
              [](double p) { return delta(static_cast<int>(p)); });
    cout << "Δ values: ";
    for (double d : deltaVals) printf("%.3f ", d);
    cout << endl;

    // istream_iterator — iterate over stream input
    cout << "Enter 3 numbers (or type 'q' to quit): ";
    vector<double> inputs;
    double val;
    int count = 0;
    while (count < 3 && (cin >> val)) {
        inputs.push_back(val);
        count++;
    }
    cin.clear();
    cout << "Read: ";
    for (double x : inputs) cout << x << " ";
    cout << endl;

    cout << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: Iterators & std::complex" << endl;
    cout << "========================================" << endl;
    cout << endl;

    demonstrateIterators();
    demonstrateComplex();
    demonstrateTwinPrimePhaseAnalysis();
    demonstrateIteratorAdaptors();

    cout << "========================================" << endl;
    cout << "Key concepts:" << endl;
    cout << "  - Iterator categories: input, output, forward, bidirectional, random access" << endl;
    cout << "  - begin/end, rbegin/rend, advance, next, distance" << endl;
    cout << "  - std::complex<T>: +, -, *, /, abs, arg, conj, norm, polar" << endl;
    cout << "  - arg(z) = atan2(imag, real) — phase angle in (-π, π]" << endl;
    cout << "  - back_inserter, front_inserter, inserter — iterator adaptors" << endl;
    cout << "  - copy, transform with iterator adaptors" << endl;
    cout << "  - DSR: θ = arg(Ω²) = arg([Δ]³ · e^(i·[Δ]³)) = [Δ]³ mod 2π" << endl;

    return 0;
}
