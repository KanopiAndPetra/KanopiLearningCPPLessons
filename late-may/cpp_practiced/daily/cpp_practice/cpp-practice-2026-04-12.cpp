// cpp-practice-2026-04-12.cpp
// Practice: Structures + Statistical Analysis + Phase 10 Residue Class Test
// Builds on: Functors, STL algorithms (Apr 11)
// New concepts: struct for data aggregation, statistical distributions, histograms
// DSR Research: Phase 10 — do 6k-1 vs 6k+1 residue classes show different phase distributions?

#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <complex>
#include <iomanip>
#include <sstream>
#include <map>

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

double delta(int n) {
    return static_cast<double>(primeCount(n)) - chebyshevPsi(static_cast<double>(n));
}

// Phase angle of Ω²(n) = [Δ]³ · exp(i·[Δ]³)
double phaseAngle(int n) {
    double d = delta(n);
    double d3 = d * d * d;
    complex<double> omega2 = pow(d, 3) * complex<double>(cos(d3), sin(d3));
    double phase = fmod(arg(omega2), 2 * PI);
    if (phase < 0) phase += 2 * PI;
    return phase;
}

// ============================================================
// PART 1: Structures — aggregate related data
// ============================================================
struct PrimeData {
    int n;
    int residueClass;      // 1 (6k+1) or 5 (6k-1)
    double delta;
    double phase;          // in radians
    double phaseNormalized; // in units of π
};

void demonstrateStructures() {
    cout << "=== Structures — Aggregate Data ===" << endl;

    // struct groups related fields
    PrimeData p = {17, 5, 1.0, 2.0, 2.0 / PI};
    cout << "Prime: " << p.n << endl;
    cout << "  Residue class: " << p.residueClass << " (6k+" << p.residueClass << ")" << endl;
    cout << "  Δ = " << p.delta << endl;
    cout << "  Phase = " << p.phase << " rad = " << p.phaseNormalized << "π" << endl;

    // Vector of structs — great for data analysis
    vector<PrimeData> dataset = {
        {5, 5, -0.107, 6.283, 2.0},
        {7, 1, 0.403, 0.132, 0.042},
        {11, 5, 0.413, 0.138, 0.044},
        {13, 1, 0.932, 1.616, 0.514}
    };

    cout << "\nSample dataset:" << endl;
    for (const auto& d : dataset) {
        cout << "  p=" << d.n << " (6k+" << d.residueClass << ") θ=" << d.phaseNormalized << "π" << endl;
    }

    cout << endl;
}

// ============================================================
// PART 2: Statistical Distributions
// ============================================================
struct PhaseStats {
    int count;
    double mean;        // in units of π
    double variance;
    double stdDev;
    double minPhase;
    double maxPhase;

    // Bin counts for phase distribution (8 bins, 0 to 2π)
    int bins[8];        // [0-π/4, π/4-π/2, π/2-3π/4, 3π/4-π, π-5π/4, 5π/4-3π/2, 3π/2-7π/4, 7π/4-2π]
    int near0;          // phase near 0 (within 0.2π)
    int nearPi;         // phase near π (within 0.2π)
    int near2Pi;        // phase near 2π (within 0.2π)
};

PhaseStats computeStats(const vector<double>& phases) {
    PhaseStats s = {0, 0, 0, 0, 999, -999, {0}};

    if (phases.empty()) return s;

    s.count = phases.size();

    // Normalize to [0, 2π] then convert to units of π
    vector<double> normalized;
    for (double p : phases) {
        double n = fmod(p, 2 * PI);
        if (n < 0) n += 2 * PI;
        normalized.push_back(n / PI);  // now in [0, 2]
    }

    // Mean
    double sum = accumulate(normalized.begin(), normalized.end(), 0.0);
    s.mean = sum / s.count;

    // Variance
    double sqSum = 0;
    for (double n : normalized) sqSum += (n - s.mean) * (n - s.mean);
    s.variance = sqSum / s.count;
    s.stdDev = sqrt(s.variance);

    // Min/max
    s.minPhase = *min_element(normalized.begin(), normalized.end());
    s.maxPhase = *max_element(normalized.begin(), normalized.end());

    // Bins (8 bins of width π/4)
    for (double n : normalized) {
        int bin = min(7, static_cast<int>(n * 4));  // n in [0,2], bin in [0,7]
        s.bins[bin]++;
    }

    // Special counts
    for (double n : normalized) {
        if (n < 0.2 || n > 1.8) s.near0++;
        if (n > 0.8 && n < 1.2) s.nearPi++;
        if (n > 1.8) s.near2Pi++;
    }

    return s;
}

void printStats(const PhaseStats& s, const string& label) {
    printf("\n%s (n=%d):\n", label.c_str(), s.count);
    printf("  Mean phase: %.3fπ  StdDev: %.3fπ\n", s.mean, s.stdDev);
    printf("  Range: %.3fπ to %.3fπ\n", s.minPhase, s.maxPhase);
    printf("  Near 0π: %d (%.1f%%)\n", s.near0, 100.0 * s.near0 / max(1, s.count));
    printf("  Near π: %d (%.1f%%)\n", s.nearPi, 100.0 * s.nearPi / max(1, s.count));

    cout << "  Distribution:" << endl;
    cout << "  [0-π/4) [π/4-π/2) [π/2-3π/4) [3π/4-π) [π-5π/4) [5π/4-3π/2) [3π/2-7π/4) [7π/4-2π]" << endl;
    cout << "  ";
    for (int i = 0; i < 8; i++) {
        cout << setw(7) << s.bins[i] << " ";
    }
    cout << endl;
}

void demonstrateStatistics() {
    cout << "=== Statistical Distributions ===" << endl;

    // Generate test phases
    vector<double> testPhases;
    for (int i = 0; i < 100; i++) {
        testPhases.push_back(fmod(i * 0.2, 2 * PI));
    }

    PhaseStats s = computeStats(testPhases);
    printStats(s, "Test data");

    cout << endl;
}

// ============================================================
// PART 3: Phase 10 — 6k±1 Residue Class Test
// ============================================================
void demonstrateResidueClassTest() {
    cout << "=== DSR: 6k±1 Residue Class Phase Test ===" << endl;
    cout << "Hypothesis: 6k-1 primes (residue 5) and 6k+1 primes (residue 1)" << endl;
    cout << "should show different phase distributions if residue class" << endl;
    cout << "structure IS the origin of the two helices." << endl;
    cout << endl;

    vector<double> phasesClass1;   // 6k+1
    vector<double> phasesClass5;  // 6k-1
    vector<PrimeData> allData;

    // Collect all primes in range with their phase data
    for (int n = 7; n <= 5000; n++) {
        if (!isPrime(n)) continue;

        int residue = n % 6;  // primes > 3 are either 1 or 5
        if (residue != 1 && residue != 5) continue;

        double p = phaseAngle(n);
        double d = delta(n);

        PrimeData pd = {n, residue, d, p, p / PI};
        allData.push_back(pd);

        if (residue == 1) phasesClass1.push_back(p);
        else phasesClass5.push_back(p);
    }

    cout << "Collected " << allData.size() << " primes" << endl;
    cout << "  6k+1 (residue 1): " << phasesClass1.size() << " primes" << endl;
    cout << "  6k-1 (residue 5): " << phasesClass5.size() << " primes" << endl;
    cout << endl;

    // Compute statistics
    PhaseStats s1 = computeStats(phasesClass1);
    PhaseStats s5 = computeStats(phasesClass5);

    printStats(s1, "6k+1 primes");
    printStats(s5, "6k-1 primes");

    // Comparison
    cout << "\n--- COMPARISON ---" << endl;
    double meanDiff = fabs(s1.mean - s5.mean);
    printf("Mean phase difference: %.3fπ (%.1f degrees)\n", meanDiff, meanDiff * 180);
    printf("6k+1 mean: %.3fπ  6k-1 mean: %.3fπ\n", s1.mean, s5.mean);

    // Chi-squared-like comparison of distributions
    int maxDiff = 0;
    for (int i = 0; i < 8; i++) {
        int diff = abs(s1.bins[i] - s5.bins[i]);
        maxDiff += diff;
    }
    int total = s1.count + s5.count;
    printf("Bin-wise L1 distance: %d (out of %d total)\n", maxDiff, total);
    printf("Average bin difference: %.1f%%\n", 100.0 * maxDiff / (8 * total / 2));

    // Interpretation
    cout << "\n--- INTERPRETATION ---" << endl;
    if (meanDiff > 0.1) {
        cout << "→ Significant mean phase difference between residue classes." << endl;
        cout << "→ Residue class IS related to phase assignment." << endl;
        cout << "→ This supports the 6k±1 structure as the helix origin." << endl;
    } else {
        cout << "→ No significant mean phase difference between residue classes." << endl;
        cout << "→ Residue class alone does NOT determine phase." << endl;
        cout << "→ The helix origin lies elsewhere." << endl;
    }

    // Strand interpretation: if one class skews toward 0 and the other toward π
    double class1Near0 = 100.0 * s1.near0 / max(1, s1.count);
    double class5Near0 = 100.0 * s5.near0 / max(1, s5.count);
    double class1NearPi = 100.0 * s1.nearPi / max(1, s1.count);
    double class5NearPi = 100.0 * s5.nearPi / max(1, s5.count);

    cout << "\n--- STRAND SIGNAL ---" << endl;
    cout << "If one residue class consistently lands near 0 and the other near π:" << endl;
    printf("  6k+1: %.1f%% near 0π, %.1f%% near π\n", class1Near0, class1NearPi);
    printf("  6k-1: %.1f%% near 0π, %.1f%% near π\n", class5Near0, class5NearPi);

    if ((class1Near0 > class5Near0 + 5) && (class5NearPi > class1NearPi + 5)) {
        cout << "→ STRAND SIGNAL CONFIRMED: residue classes map to strands." << endl;
    } else if ((class5Near0 > class1Near0 + 5) && (class1NearPi > class5NearPi + 5)) {
        cout << "→ STRAND SIGNAL CONFIRMED: residue classes map to strands." << endl;
    } else {
        cout << "→ No clear strand mapping from residue class alone." << endl;
    }

    cout << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: Structures + Statistics + Phase 10 Residue Test" << endl;
    cout << "================================================================" << endl;
    cout << endl;

    demonstrateStructures();
    demonstrateStatistics();
    demonstrateResidueClassTest();

    cout << "================================================================" << endl;
    cout << "Key concepts:" << endl;
    cout << "  - struct: aggregate related data fields" << endl;
    cout << "  - PhaseStats: mean, variance, stdDev, bins, near-0, near-pi counts" << endl;
    cout << "  - computeStats: full statistical distribution analysis" << endl;
    cout << "  - Phase angle: θ = arg(Ω²) = [Δ]³ mod 2π" << endl;
    cout << "  - 6k±1: primes > 3 are ≡ 1 or 5 mod 6" << endl;
    cout << "  - DSR test: do residue classes show different phase distributions?" << endl;

    return 0;
}
