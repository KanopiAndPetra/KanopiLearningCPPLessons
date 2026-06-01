// cpp-practice-2026-04-09-stl.cpp
// Practice: STL Containers & Algorithms
// Builds on: Templates (April 9), Smart Pointers (April 8)
// New concepts: vector, map, set, algorithms, iterators, lambda expressions
// Direct connection: STL containers for prime data structures and twin prime analysis

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>
#include <string>
#include <cmath>
#include <sstream>

using namespace std;

// ============================================================
// PRIME UTILITIES (simplified for demonstration)
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

// Simplified Ω computation for demonstration
// Ω(n) = sign(Δ) * |Δ|^p for some power p
// For twin prime analysis, we track Δ(n) = π(n) - ψ(n)
double computeDelta(int n) {
    return static_cast<double>(primeCount(n)) - chebyshevPsi(n);
}

// ============================================================
// PART 1: vector — dynamic array
// ============================================================
void demonstrateVector() {
    cout << "=== vector — Dynamic Array ===" << endl;

    // vector<T> — dynamically sized array
    vector<int> primes;

    // Add primes up to 100
    for (int i = 2; i <= 100; i++) {
        if (isPrime(i)) primes.push_back(i);
    }

    cout << "Primes up to 100: ";
    for (int p : primes) cout << p << " ";
    cout << endl;

    cout << "Count: " << primes.size() << endl;
    cout << "First 5: ";
    for (int i = 0; i < 5 && i < primes.size(); i++) {
        cout << primes[i] << " ";
    }
    cout << endl;

    // Transform: compute Δ for each prime
    vector<double> deltas;
    transform(primes.begin(), primes.end(), back_inserter(deltas),
              [](int p) { return computeDelta(p); });

    cout << "Δ values for first 10 primes: ";
    for (int i = 0; i < 10 && i < deltas.size(); i++) {
        printf("%.2f ", deltas[i]);
    }
    cout << endl;

    // Algorithms on vectors
    auto [minIt, maxIt] = minmax_element(deltas.begin(), deltas.end());
    cout << "Min Δ: " << *minIt << " at prime " << primes[minIt - deltas.begin()] << endl;
    cout << "Max Δ: " << *maxIt << " at prime " << primes[maxIt - deltas.begin()] << endl;

    cout << endl;
}

// ============================================================
// PART 2: map — key-value dictionary
// ============================================================
void demonstrateMap() {
    cout << "=== map — Key-Value Dictionary ===" << endl;

    // map<key, value> — sorted by key
    map<int, double> primeDelta;

    for (int n = 2; n <= 100; n++) {
        if (isPrime(n)) {
            primeDelta[n] = computeDelta(n);
        }
    }

    cout << "Prime → Δ(n) map (first 10 entries):" << endl;
    int count = 0;
    for (const auto& [prime, delta] : primeDelta) {
        cout << "  π(" << prime << ") = " << delta << endl;
        if (++count >= 10) break;
    }

    // Lookup by key — O(log n)
    int target = 31;
    auto it = primeDelta.find(target);
    if (it != primeDelta.end()) {
        cout << "Δ(" << target << ") = " << it->second << " (found in map)" << endl;
    }

    // Count primes where Δ > 0
    int positiveCount = count_if(primeDelta.begin(), primeDelta.end(),
        [](const auto& pair) { return pair.second > 0; });
    cout << "Primes with positive Δ: " << positiveCount << " / " << primeDelta.size() << endl;

    cout << endl;
}

// ============================================================
// PART 3: set — unique sorted elements
// ============================================================
void demonstrateSet() {
    cout << "=== set — Unique Sorted Elements ===" << endl;

    // set<T> — sorted, unique elements, O(log n) operations
    set<int> twinPrimes;

    // Find twin primes up to 200
    for (int i = 3; i <= 200; i++) {
        if (isPrime(i) && isPrime(i - 2)) {
            twinPrimes.insert(i - 2);  // smaller of the pair
            twinPrimes.insert(i);        // larger of the pair
        }
    }

    cout << "Twin primes up to 200: ";
    int count = 0;
    for (int p : twinPrimes) {
        cout << p << " ";
        if (++count % 10 == 0) cout << endl;
    }
    cout << endl;
    cout << "Total twin primes: " << twinPrimes.size() << endl;

    // set operations: union, intersection, difference
    set<int> primesUnder100;
    for (int i = 2; i <= 100; i++) {
        if (isPrime(i)) primesUnder100.insert(i);
    }

    // Twin primes under 100
    set<int> twinUnder100;
    for (int p : twinPrimes) {
        if (p <= 100) twinUnder100.insert(p);
    }

    // Non-twin primes under 100
    vector<int> nonTwin;
    set_difference(primesUnder100.begin(), primesUnder100.end(),
                   twinUnder100.begin(), twinUnder100.end(),
                   back_inserter(nonTwin));

    cout << "Non-twin primes under 100: ";
    for (int p : nonTwin) cout << p << " ";
    cout << endl;
    cout << "Count: " << nonTwin.size() << " non-twin vs " << twinUnder100.size() << " twin" << endl;

    cout << endl;
}

// ============================================================
// PART 4: algorithms — the power of STL
// ============================================================
void demonstrateAlgorithms() {
    cout << "=== STL Algorithms ===" << endl;

    // Generate primes 2-100
    vector<int> primes;
    for (int i = 2; i <= 100; i++) {
        if (isPrime(i)) primes.push_back(i);
    }

    // Lambda: anonymous function
    auto isEven = [](int n) { return n % 2 == 0; };
    auto isOdd = [](int n) { return n % 2 != 0; };

    int evenCount = count_if(primes.begin(), primes.end(), isEven);
    int oddCount = count_if(primes.begin(), primes.end(), isOdd);
    cout << "Even primes: " << evenCount << endl;  // only 2
    cout << "Odd primes: " << oddCount << endl;

    // accumulate — sum
    double sum = accumulate(primes.begin(), primes.end(), 0.0);
    cout << "Sum of primes 2-100: " << sum << endl;

    // transform — apply function to each element
    vector<double> logPrimes;
    transform(primes.begin(), primes.end(), back_inserter(logPrimes),
              [](int p) { return log(static_cast<double>(p)); });
    cout << "log of first 5 primes: ";
    for (int i = 0; i < 5; i++) cout << logPrimes[i] << " ";
    cout << endl;

    // find — search
    auto it = find(primes.begin(), primes.end(), 47);
    if (it != primes.end()) {
        cout << "47 found at index " << (it - primes.begin()) << endl;
    }

    // sort and unique
    vector<int> withDupes = {3, 1, 4, 1, 5, 9, 2, 6, 5};
    sort(withDupes.begin(), withDupes.end());
    auto last = unique(withDupes.begin(), withDupes.end());
    withDupes.erase(last, withDupes.end());
    cout << "After sort+unique: ";
    for (int n : withDupes) cout << n << " ";
    cout << endl;

    cout << endl;
}

// ============================================================
// PART 5: Twin Prime Mapping — DSR Connection
// ============================================================
void demonstrateTwinPrimeMapping() {
    cout << "=== Twin Prime Mapping for DSR ===" << endl;

    // For each twin prime pair (p, p+2), compute Δ
    // The hypothesis: one prime lands on strand A, partner on strand B

    map<int, pair<double, double>> twinPrimeOmega;  // n → (Δ(n), Δ(n+2))

    for (int n = 3; n <= 500; n++) {
        if (isPrime(n) && isPrime(n + 2)) {
            twinPrimeOmega[n] = {computeDelta(n), computeDelta(n + 2)};
        }
    }

    cout << "Twin prime Δ analysis (first 15 pairs):" << endl;
    cout << "Pair\tΔ(n)\tΔ(n+2)\tSign(n)\tSign(n+2)\tPattern" << endl;
    int count = 0;
    for (const auto& [n, deltas] : twinPrimeOmega) {
        double d1 = deltas.first;
        double d2 = deltas.second;
        string sign1 = d1 > 0 ? "+" : (d1 < 0 ? "-" : "0");
        string sign2 = d2 > 0 ? "+" : (d2 < 0 ? "-" : "0");
        string pattern = (sign1 != sign2) ? "OPPOSITE" : "SAME";

        printf("%d,%d\t%.2f\t%.2f\t%s\t%s\t%s\n",
               n, n+2, d1, d2, sign1.c_str(), sign2.c_str(), pattern.c_str());

        if (++count >= 15) break;
    }

    // Count opposite-sign pairs
    int oppositeCount = 0;
    for (const auto& [n, deltas] : twinPrimeOmega) {
        if ((deltas.first > 0 && deltas.second < 0) ||
            (deltas.first < 0 && deltas.second > 0)) {
            oppositeCount++;
        }
    }

    cout << endl;
    cout << "Twin prime pairs with opposite-sign Δ: "
         << oppositeCount << " / " << twinPrimeOmega.size() << endl;
    printf("Ratio: %.1f%%\n", 100.0 * oppositeCount / twinPrimeOmega.size());

    if (twinPrimeOmega.size() > 0 && oppositeCount > twinPrimeOmega.size() * 0.5) {
        cout << "INTERPRETATION: Majority of twin primes show opposite-sign Δ" << endl;
        cout << "→ Consistent with strand A / strand B assignment" << endl;
    }

    cout << endl;
}

// ============================================================
// PART 6: string streams
// ============================================================
void demonstrateStringStreams() {
    cout << "=== stringstream ===" << endl;

    // Parse comma-separated values
    string data = "3,14,159,265,359";
    stringstream ss(data);
    string token;
    vector<int> values;

    while (getline(ss, token, ',')) {
        values.push_back(stoi(token));
    }

    cout << "Parsed from CSV: ";
    for (int v : values) cout << v << " ";
    cout << endl;

    // Format output
    stringstream out;
    out << "First: " << values[0] << ", Last: " << values.back();
    cout << out.str() << endl;

    cout << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: STL Containers & Algorithms" << endl;
    cout << "==========================================" << endl;
    cout << endl;

    demonstrateVector();
    demonstrateMap();
    demonstrateSet();
    demonstrateAlgorithms();
    demonstrateTwinPrimeMapping();
    demonstrateStringStreams();

    cout << "==========================================" << endl;
    cout << "Key concepts:" << endl;
    cout << "  - vector<T> — dynamic array, push_back, back_inserter" << endl;
    cout << "  - map<K,V> — sorted key-value, find(), operator[]" << endl;
    cout << "  - set<T> — unique sorted, insert, count_if" << endl;
    cout << "  - Algorithms: transform, accumulate, minmax_element" << endl;
    cout << "  - Lambdas: [](int n){ return n > 0; }" << endl;
    cout << "  - stringstream: parse CSV, format output" << endl;
    cout << "  - DSR connection: twin prime Δ mapping, strand hypothesis" << endl;

    return 0;
}
