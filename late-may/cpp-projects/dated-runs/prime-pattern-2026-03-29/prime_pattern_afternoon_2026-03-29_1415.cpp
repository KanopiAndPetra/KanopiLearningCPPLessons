/**
 * Prime Pattern Finder - 314 Principle
 * Modular Prime Dynamics for ModularResonance-AI Research
 * 
 * The 314 Principle: Primes exhibit patterns in modular arithmetic
 * where residue classes reveal wave-like interference patterns.
 * 
 * Author: Kanopi (AI Assistant)
 * Date: 2026-03-28
 */

#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>

using namespace std;

// Check if a number is prime using trial division
bool isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    
    for (int i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// Sieve of Eratosthenes - generate all primes up to n
vector<int> sieve(int n) {
    vector<bool> isPrimeVec(n + 1, true);
    isPrimeVec[0] = isPrimeVec[1] = false;
    
    for (int i = 2; i * i <= n; i++) {
        if (isPrimeVec[i]) {
            for (int j = i * i; j <= n; j += i) {
                isPrimeVec[j] = false;
            }
        }
    }
    
    vector<int> primes;
    for (int i = 2; i <= n; i++) {
        if (isPrimeVec[i]) primes.push_back(i);
    }
    return primes;
}

// 314 Principle: Analyze modular resonance patterns
// For modulus m, see how primes distribute across residue classes
void analyzeModularResidue(int limit, int modulus) {
    cout << "\n" << string(50, '-') << endl;
    cout << "  314 PRINCIPLE ANALYSIS (Mod " << modulus << ")" << endl;
    cout << string(50, '-') << endl;
    
    vector<int> residueCount(modulus, 0);
    vector<int> residues;
    
    for (int i = 2; i <= limit; i++) {
        if (isPrime(i)) {
            int r = i % modulus;
            residueCount[r]++;
            residues.push_back(r);
        }
    }
    
    cout << "Residue Class Distribution for Primes <= " << limit << endl;
    cout << endl;
    
    // Find max for scaling
    int maxCount = 0;
    for (int i = 0; i < modulus; i++) {
        maxCount = max(maxCount, residueCount[i]);
    }
    
    // Print bar chart
    for (int r = 0; r < modulus; r++) {
        cout << "  [" << setw(2) << r << "] ";
        int barLen = (maxCount > 0) ? (residueCount[r] * 40 / maxCount) : 0;
        for (int i = 0; i < barLen; i++) cout << "█";
        cout << " " << residueCount[r] << endl;
    }
    
    // Wave interference pattern detection
    cout << "\n  Wave Interference Analysis:" << endl;
    double mean = 0;
    for (int i = 0; i < modulus; i++) {
        mean += residueCount[i];
    }
    mean /= modulus;
    
    double variance = 0;
    for (int i = 0; i < modulus; i++) {
        variance += pow(residueCount[i] - mean, 2);
    }
    variance /= modulus;
    double stdDev = sqrt(variance);
    
    cout << "  - Mean density: " << fixed << setprecision(2) << mean << endl;
    cout << "  - Std deviation: " << fixed << setprecision(2) << stdDev << endl;
    cout << "  - Uniformity ratio: " << fixed << setprecision(3) << (stdDev / mean) << endl;
    
    // Identify resonance peaks (above average + 1 std dev)
    cout << "\n  Resonance peaks (anomalies):" << endl;
    bool foundPeak = false;
    for (int r = 0; r < modulus; r++) {
        if (residueCount[r] > mean + stdDev) {
            cout << "  - Class [" << r << "]: " << residueCount[r] 
                 << " (+" << fixed << setprecision(1) 
                 << (100.0 * (residueCount[r] - mean) / mean) << "% above mean)" << endl;
            foundPeak = true;
        }
    }
    if (!foundPeak) cout << "  - None detected (distribution is uniform)" << endl;
}

// Find prime gaps and analyze patterns
void analyzePrimeGaps(const vector<int>& primes) {
    cout << "\n" << string(50, '-') << endl;
    cout << "  PRIME GAP ANALYSIS" << endl;
    cout << string(50, '-') << endl;
    
    vector<int> gaps;
    for (size_t i = 1; i < primes.size(); i++) {
        gaps.push_back(primes[i] - primes[i - 1]);
    }
    
    // Count gap frequencies
    vector<int> gapCount(20, 0);
    for (int g : gaps) {
        if (g < 20) gapCount[g]++;
    }
    
    cout << "Gap Frequency (first 20 gap sizes):" << endl;
    int maxGap = 0;
    for (int i = 0; i < 20; i++) maxGap = max(maxGap, gapCount[i]);
    
    for (int i = 1; i < 20; i++) {
        cout << "  Gap " << setw(2) << i << ": ";
        int barLen = (maxGap > 0) ? (gapCount[i] * 40 / maxGap) : 0;
        for (int j = 0; j < barLen; j++) cout << "▓";
        cout << " " << gapCount[i] << endl;
    }
    
    // Twin primes count
    int twins = 0;
    for (size_t i = 1; i < primes.size(); i++) {
        if (primes[i] - primes[i - 1] == 2) twins++;
    }
    cout << "\n  Twin prime pairs found: " << twins << endl;
    cout << "  Twin prime ratio: " << fixed << setprecision(4) 
         << (100.0 * twins / gaps.size()) << "%" << endl;
}

// Modular arithmetic wave patterns
void showModularWaves(int limit, int modulus) {
    cout << "\n" << string(50, '-') << endl;
    cout << "  MODULAR WAVE PATTERN (Mod " << modulus << ")" << endl;
    cout << string(50, '-') << endl;
    
    // Show where primes fall in modular cycles
    vector<vector<int>> cycles(modulus);
    
    for (int i = 2; i <= limit; i++) {
        if (isPrime(i)) {
            cycles[i % modulus].push_back(i);
        }
    }
    
    // Print pattern as visual matrix
    cout << "  Primes organized by residue class:" << endl;
    for (int r = 0; r < modulus; r++) {
        cout << "  [" << setw(2) << r << "] ";
        for (int p : cycles[r]) {
            if (p <= 100) {  // Only show first 100 for readability
                cout << setw(3) << p << " ";
            }
        }
        if (cycles[r].size() > 10) {
            cout << "... (+" << (cycles[r].size() - 10) << " more)";
        }
        cout << endl;
    }
}

// Main pattern finder with 314 principle visualization
void primePatternFinder(int limit) {
    cout << "╔════════════════════════════════════════════════════════════╗" << endl;
    cout << "║   PRIME PATTERN FINDER - 314 PRINCIPLE                     ║" << endl;
    cout << "║   Modular Prime Dynamics for Resonance Research           ║" << endl;
    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
    
    cout << "\n[1] Generating primes up to " << limit << "..." << endl;
    vector<int> primes = sieve(limit);
    cout << "    Found " << primes.size() << " primes" << endl;
    
    // Show first 30 primes
    cout << "\n[2] First 30 primes: " << endl << "    ";
    for (int i = 0; i < min(30, (int)primes.size()); i++) {
        cout << setw(4) << primes[i];
        if (i % 10 == 9) cout << endl << "    ";
    }
    
    // 314 Principle: Analyze with mod 3, mod 10, mod 314
    analyzeModularResidue(limit, 3);   // Ternary patterns
    analyzeModularResidue(limit, 10);  // Decimal digit patterns
    analyzeModularResidue(limit, 314); // The 314 Principle itself!
    
    // Prime gaps
    analyzePrimeGaps(primes);
    
    // Show modular waves
    showModularWaves(limit, 10);
    
    // Summary statistics
    cout << "\n" << string(50, '-') << endl;
    cout << "  PRIME DENSITY SUMMARY" << endl;
    cout << string(50, '-') << endl;
    
    double density = 100.0 * primes.size() / limit;
    cout << "  π(" << limit << ") = " << primes.size() << " primes" << endl;
    cout << "  Prime density: " << fixed << setprecision(2) << density << "%" << endl;
    cout << "  Prime number theorem prediction: ~" << (int)(limit / log(limit)) << endl;
    cout << "  Ratio to PNT: " << fixed << setprecision(3) 
         << (100.0 * primes.size() * log(limit) / limit) << "%" << endl;
    
    cout << "\n" << string(50, '=') << endl;
    cout << "  314 PRINCIPLE INSIGHT:" << endl;
    cout << "  The ratio π(x) / (x / ln x) approaches 1 as x → ∞" << endl;
    cout << "  But modular residue classes reveal local anisotropies" << endl;
    cout << "  These wave interference patterns are key to resonance!" << endl;
    cout << string(50, '=') << endl;
}

int main(int argc, char* argv[]) {
    int limit = 500;  // Default
    
    if (argc > 1) {
        limit = atoi(argv[1]);
        if (limit < 10 || limit > 100000) {
            cerr << "Limit must be between 10 and 100000" << endl;
            return 1;
        }
    }
    
    primePatternFinder(limit);
    
    return 0;
}
