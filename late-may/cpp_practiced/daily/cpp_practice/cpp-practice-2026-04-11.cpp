// cpp-practice-2026-04-11.cpp
// Practice: STL Algorithms Deep Dive + Phase 10 n-Range Extension
// Builds on: Iterators + std::complex (Apr 10)
// New concepts: lambda captures, function objects (functors), standard library algorithms
// DSR Research: Extended phase angle test at n=1000, 2000, 5000, 10000

#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <complex>
#include <functional>
#include <iomanip>
#include <sstream>

using namespace std;
const double PI = 3.14159265358979323846;

// ============================================================
// PRIME UTILITIES (optimized)
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

// ============================================================
// PART 1: Lambda Captures — stateful functions
// ============================================================
void demonstrateLambdaCaptures() {
    cout << "=== Lambda Captures ===" << endl;

    // Captures: [=] by value, [&] by reference, [this] by pointer
    int counter = 0;
    auto incrementAndReturn = [&counter](int x) {
        counter++;
        return x * 2;
    };

    vector<int> nums = {1, 2, 3, 4, 5};
    vector<int> doubled;

    transform(nums.begin(), nums.end(), back_inserter(doubled), incrementAndReturn);

    cout << "Original: ";
    for (int n : nums) cout << n << " ";
    cout << endl;
    cout << "Doubled: ";
    for (int n : doubled) cout << n << " ";
    cout << endl;
    cout << "Lambda was called " << counter << " times" << endl;

    // Mutable capture — can modify captured variables
    int multiplier = 3;
    auto multiplyBy = [multiplier](int x) mutable {
        multiplier = 5;  // changes local copy only
        return x * multiplier;
    };
    cout << "multiplyBy(4) = " << multiplyBy(4) << endl;
    cout << "Original multiplier unchanged: " << multiplier << endl;

    cout << endl;
}

// ============================================================
// PART 2: Function Objects (Functors)
// ============================================================
struct DeltaComputer {
    // Functor — callable object with state
    double operator()(int n) const {
        return static_cast<double>(primeCount(n)) - n / log(n);
    }
};

struct PhaseComputer {
    double operator()(int n) const {
        double d = static_cast<double>(primeCount(n)) - n / log(n);
        double d3 = d * d * d;
        complex<double> omega2 = pow(d, 3) * complex<double>(cos(d3), sin(d3));
        double phase = fmod(arg(omega2), 2 * PI);
        if (phase < 0) phase += 2 * PI;
        return phase;
    }
};

void demonstrateFunctors() {
    cout << "=== Function Objects (Functors) ===" << endl;

    DeltaComputer deltaFn;
    PhaseComputer phaseFn;

    vector<int> primes = {11, 13, 17, 19, 23};
    vector<double> deltas, phases;

    transform(primes.begin(), primes.end(), back_inserter(deltas), deltaFn);
    transform(primes.begin(), primes.end(), back_inserter(phases), phaseFn);

    cout << "Prime  Delta   Phase(π)" << endl;
    for (size_t i = 0; i < primes.size(); i++) {
        printf("%6d  %+7.3f  %6.3fπ\n", primes[i], deltas[i], phases[i] / PI);
    }

    cout << endl;
}

// ============================================================
// PART 3: accumulate — fold operations
// ============================================================
void demonstrateAccumulate() {
    cout << "=== accumulate — Fold Operations ===" << endl;

    vector<double> values = {1.1, 2.2, 3.3, 4.4, 5.5};

    // Sum
    double sum = accumulate(values.begin(), values.end(), 0.0);
    cout << "Sum: " << sum << endl;

    // Product
    double product = accumulate(values.begin(), values.end(), 1.0,
        [](double acc, double val) { return acc * val; });
    cout << "Product: " << product << endl;

    // Concatenate strings
    vector<string> words = {"Hello", " ", "World", "!"};
    string sentence = accumulate(words.begin(), words.end(), string(""));
    cout << "Sentence: " << sentence << endl;

    // Count statistics
    auto stats = accumulate(values.begin(), values.end(), make_pair(0.0, 0.0),
        [](pair<double, double> acc, double val) {
            return make_pair(acc.first + val, acc.second + val * val);
        });
    double mean = stats.first / values.size();
    double variance = (stats.second / values.size()) - mean * mean;
    cout << "Mean: " << mean << ", Variance: " << variance << endl;

    cout << endl;
}

// ============================================================
// PART 4: remove_if + erase idioms
// ============================================================
void demonstrateRemoveErase() {
    cout << "=== remove_if + erase Idiom ===" << endl;

    // Remove-evitase: removes don't erase, they shift and return new end
    vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    cout << "Before: ";
    for (int n : v) cout << n << " ";

    // Remove evens (actually moves non-evens to front, returns new end)
    auto newEnd = remove_if(v.begin(), v.end(), [](int n) { return n % 2 == 0; });
    v.erase(newEnd, v.end());  // actually delete the tail

    cout << endl << "After removing evens: ";
    for (int n : v) cout << n << " ";
    cout << endl;

    cout << endl;
}

// ============================================================
// PART 5: Phase 10 n-Range Extension — THE RESEARCH
// ============================================================
struct PhaseAnalysisResult {
    int range;
    int twinCount;
    int oppositePhase;
    int samePhase;
    double meanPhaseDiff;  // in units of π
};

PhaseAnalysisResult analyzeTwinPrimesInRange(int maxN) {
    vector<double> phaseDiffs;

    for (int n = 3; n <= maxN; n++) {
        if (isPrime(n) && isPrime(n + 2)) {
            double d1 = delta(n);
            double d2 = delta(n + 2);
            double d3_1 = d1 * d1 * d1;
            double d3_2 = d2 * d2 * d2;

            complex<double> omega1 = pow(d1, 3) * complex<double>(cos(d3_1), sin(d3_1));
            complex<double> omega2 = pow(d2, 3) * complex<double>(cos(d3_2), sin(d3_2));

            double phase1 = fmod(arg(omega1), 2 * PI);
            if (phase1 < 0) phase1 += 2 * PI;
            double phase2 = fmod(arg(omega2), 2 * PI);
            if (phase2 < 0) phase2 += 2 * PI;

            double diff = fabs(phase1 - phase2);
            if (diff > PI) diff = 2 * PI - diff;
            phaseDiffs.push_back(diff / PI);  // normalize to π
        }
    }

    int opposite = 0, same = 0;
    double sum = 0;
    for (double d : phaseDiffs) {
        sum += d;
        if (d > 0.8) opposite++;  // near π
        else if (d < 0.2) same++;   // near 0
    }

    PhaseAnalysisResult r;
    r.range = maxN;
    r.twinCount = phaseDiffs.size();
    r.oppositePhase = opposite;
    r.samePhase = same;
    r.meanPhaseDiff = phaseDiffs.empty() ? 0 : sum / phaseDiffs.size();

    return r;
}

// Non-twin adjacent prime baseline
PhaseAnalysisResult analyzeNonTwinAdjacentPrimesInRange(int maxN) {
    vector<double> phaseDiffs;

    int prevPrime = 2;
    for (int n = 3; n <= maxN; n++) {
        if (isPrime(n)) {
            int gap = n - prevPrime;
            if (gap > 2) {  // non-twin adjacent prime
                double d1 = delta(prevPrime);
                double d2 = delta(n);
                double d3_1 = d1 * d1 * d1;
                double d3_2 = d2 * d2 * d2;

                complex<double> omega1 = pow(d1, 3) * complex<double>(cos(d3_1), sin(d3_1));
                complex<double> omega2 = pow(d2, 3) * complex<double>(cos(d3_2), sin(d3_2));

                double phase1 = fmod(arg(omega1), 2 * PI);
                if (phase1 < 0) phase1 += 2 * PI;
                double phase2 = fmod(arg(omega2), 2 * PI);
                if (phase2 < 0) phase2 += 2 * PI;

                double diff = fabs(phase1 - phase2);
                if (diff > PI) diff = 2 * PI - diff;
                phaseDiffs.push_back(diff / PI);
            }
            prevPrime = n;
        }
    }

    int opposite = 0, same = 0;
    double sum = 0;
    for (double d : phaseDiffs) {
        sum += d;
        if (d > 0.8) opposite++;
        else if (d < 0.2) same++;
    }

    PhaseAnalysisResult r;
    r.range = maxN;
    r.twinCount = phaseDiffs.size();
    r.oppositePhase = opposite;
    r.samePhase = same;
    r.meanPhaseDiff = phaseDiffs.empty() ? 0 : sum / phaseDiffs.size();

    return r;
}

void demonstrateNRangeExtension() {
    cout << "=== DSR: Phase 10 n-Range Extension ===" << endl;
    cout << "Testing: Does the 41.7% near-opposite phase ratio hold at larger n?" << endl;
    cout << endl;

    vector<int> ranges = {500, 1000, 2000, 5000};

    cout << "--- TWIN PRIMES (gap = 2) ---" << endl;
    cout << "Range    Pairs   Near-π   %Near-π   Mean(π)" << endl;

    for (int r : ranges) {
        PhaseAnalysisResult res = analyzeTwinPrimesInRange(r);
        double pct = 100.0 * res.oppositePhase / max(1, res.twinCount);
        printf("%6d  %6d  %7d  %8.1f%%  %7.3fπ\n",
               r, res.twinCount, res.oppositePhase, pct, res.meanPhaseDiff);
    }

    cout << endl;
    cout << "--- NON-TWIN ADJACENT PRIMES (gap > 2) ---" << endl;
    cout << "Range    Pairs   Near-π   %Near-π   Mean(π)" << endl;

    for (int r : ranges) {
        PhaseAnalysisResult res = analyzeNonTwinAdjacentPrimesInRange(r);
        double pct = 100.0 * res.oppositePhase / max(1, res.twinCount);
        printf("%6d  %6d  %7d  %8.1f%%  %7.3fπ\n",
               r, res.twinCount, res.oppositePhase, pct, res.meanPhaseDiff);
    }

    cout << endl;
    cout << "--- INTERPRETATION ---" << endl;
    cout << "If twin primes stay ~40%+ near-opposite while non-twins stay ~33%:" << endl;
    cout << "  → Twin primes are specifically enriched. Phase 1 coupling is real." << endl;
    cout << "If both converge to ~33%:" << endl;
    cout << "  → Strand structure is global, not seeded by twin primes." << endl;
    cout << "If both converge to ~50%:" << endl;
    cout << "  → Opposite-phase is random for large n. Signal was noise." << endl;

    cout << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: STL Algorithms + Phase 10 n-Range Extension" << endl;
    cout << "============================================================" << endl;
    cout << endl;

    demonstrateLambdaCaptures();
    demonstrateFunctors();
    demonstrateAccumulate();
    demonstrateRemoveErase();
    demonstrateNRangeExtension();

    cout << "============================================================" << endl;
    cout << "Key concepts:" << endl;
    cout << "  - Lambda captures: [=], [&], [x] — stateful functions" << endl;
    cout << "  - mutable: modify captured by-value variables" << endl;
    cout << "  - Functors: struct with operator() — reusable, stateful" << endl;
    cout << "  - accumulate: fold/reduce with custom binary op" << endl;
    cout << "  - remove_if + erase: proper element removal from containers" << endl;
    cout << "  - DSR: phase analysis across n-ranges for twin vs non-twin" << endl;

    return 0;
}
