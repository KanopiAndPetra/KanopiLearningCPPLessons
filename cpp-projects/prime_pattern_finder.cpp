/*
 * Prime Pattern Finder - 314 Principle (Modular Prime Dynamics)
 * For ModularResonance-AI Research
 * 
 * Exploring prime number distributions through modular arithmetic,
 * wave interference patterns, and resonance phenomena.
 *
 * The 314 Principle:
 *   - 3: Core modular classes (primes mod 3)
 *   - 1: Unity resonance point (special patterns at specific intervals)
 *   - 4: Extended analysis dimensions (4th-order correlations)
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <set>
#include <map>

using namespace std;

//=============================================================================
// PRIME SIEVE - Classic Sieve of Eratosthenes
//=============================================================================
vector<int> sieveOfEratosthenes(int limit) {
    vector<bool> isPrime(limit + 1, true);
    isPrime[0] = isPrime[1] = false;
    
    for (int p = 2; p * p <= limit; p++) {
        if (isPrime[p]) {
            for (int i = p * p; i <= limit; i += p) {
                isPrime[i] = false;
            }
        }
    }
    
    vector<int> primes;
    for (int i = 2; i <= limit; i++) {
        if (isPrime[i]) primes.push_back(i);
    }
    return primes;
}

//=============================================================================
// MODULAR PRIME DYNAMICS - Core 314 Principle Analysis
//=============================================================================

// Analyze primes mod 3 (first component of 314)
void analyzeMod3(const vector<int>& primes) {
    cout << "\n=== MOD 3 ANALYSIS (314 Component 1: 3) ===" << endl;
    map<int, int> mod3Count;
    
    for (int p : primes) {
        mod3Count[p % 3]++;
    }
    
    cout << "Prime distribution mod 3:" << endl;
    for (int r = 0; r < 3; r++) {
        cout << "  Class " << r << ": " << setw(6) << mod3Count[r] 
             << " primes (" << fixed << setprecision(2) 
             << (100.0 * mod3Count[r] / primes.size()) << "%)" << endl;
    }
    
    // Note: Prime > 3 cannot be ≡ 0 mod 3
    cout << "\n  Note: Only primes {3} occupies class 0" << endl;
    cout << "        Primes > 3 split between classes 1 and 2" << endl;
}

// Analyze primes mod 314 (extended modular analysis - Pi-inspired)
void analyzeMod314(const vector<int>& primes, int modBase = 314) {
    cout << "\n=== MOD " << modBase << " ANALYSIS (314 Component 2: 1) ===" << endl;
    cout << "(First 3 digits of Pi - exploring Pi-prime connections)" << endl;
    
    // Focus on resonance points - primes that appear at regular intervals
    vector<int> primeSet(primes.begin(), primes.end());
    
    cout << "\nSearching for resonance patterns..." << endl;
    
    // Find primes that are equidistant from multiples of modBase
    vector<pair<int, int>> resonances;
    for (size_t i = 1; i < primeSet.size(); i++) {
        int delta = primeSet[i] - primeSet[i-1];
        if (delta > 0 && delta <= 20) {  // Gap analysis
            int resonance = (primeSet[i] % modBase);
            resonances.push_back({resonance, delta});
        }
    }
    
    // Group by residue
    map<int, int> residueFrequency;
    for (const auto& r : resonances) {
        residueFrequency[r.first % 100]++;  // Group by first 2 digits of residue
    }
    
    cout << "\nTop 10 residue clusters (primes mod " << modBase << "):" << endl;
    vector<pair<int, int>> sortedResidues(residueFrequency.begin(), residueFrequency.end());
    sort(sortedResidues.begin(), sortedResidues.end(), 
         [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (int i = 0; i < min(10, (int)sortedResidues.size()); i++) {
        cout << "  Residue ~" << sortedResidues[i].first 
             << ": " << sortedResidues[i].second << " occurrences" << endl;
    }
}

//=============================================================================
// WAVE INTERFERENCE PATTERN ANALYSIS
//=============================================================================

void analyzeWaveInterference(const vector<int>& primes, int maxPrime) {
    cout << "\n=== WAVE INTERFERENCE ANALYSIS (314 Component 3: 4) ===" << endl;
    cout << "(4th-order correlations in prime wave patterns)" << endl;
    
    // Create prime characteristic wave function
    int N = maxPrime + 1;
    vector<double> wave(N, 0);
    
    for (int p : primes) {
        if (p < N) wave[p] = 1.0;  // Prime = amplitude 1
    }
    
    // Analyze autocorrelation (how prime patterns repeat)
    cout << "\nAutocorrelation analysis (prime wave self-similarity):" << endl;
    
    vector<double> autocorr;
    int maxLag = min(N/4, 500);
    
    for (int lag = 1; lag < maxLag; lag++) {
        double sum = 0;
        int count = 0;
        for (int i = 0; i < N - lag; i++) {
            sum += wave[i] * wave[i + lag];
            count++;
        }
        if (count > 0) {
            autocorr.push_back(sum / count);
        }
    }
    
    // Find peaks in autocorrelation (periodic patterns)
    vector<int> peaks;
    for (size_t i = 1; i + 1 < autocorr.size(); i++) {
        if (autocorr[i] > autocorr[i-1] && autocorr[i] > autocorr[i+1]) {
            if (autocorr[i] > 0.1) {  // Threshold for significant correlation
                peaks.push_back(i);
            }
        }
    }
    
    cout << "\nSignificant periodicities detected:" << endl;
    for (int peak : peaks) {
        cout << "  Period ~" << peak << " (autocorrelation: " 
             << fixed << setprecision(3) << autocorr[peak] << ")" << endl;
    }
    
    if (peaks.empty()) {
        cout << "  (No strong periodicities found - primes appear irregular)" << endl;
    }
}

//=============================================================================
// GOLDbach ANALYSIS  
//=============================================================================

void analyzeGoldbach(const vector<int>& primes) {
    cout << "\n=== GOLDBACH RESONANCE ANALYSIS ===" << endl;
    
    // Goldbach conjecture: every even number > 2 is sum of two primes
    int maxCheck = min(1000, (primes.back() > 1000) ? 1000 : (int)primes.back() - 1);
    
    map<int, vector<pair<int, int>>> goldbachPairs;
    
    for (int even = 4; even <= maxCheck; even += 2) {
        for (int p1 : primes) {
            if (p1 >= even) break;
            int p2 = even - p1;
            if (binary_search(primes.begin(), primes.end(), p2)) {
                goldbachPairs[even].push_back({p1, p2});
                break;  // Count only first valid pair
            }
        }
    }
    
    cout << "Even numbers that are Goldbach sums:" << endl;
    int count = 0;
    for (const auto& kv : goldbachPairs) {
        if (count < 15) {
            cout << "  " << kv.first << " = " << kv.second[0].first 
                 << " + " << kv.second[0].second;
            if (kv.second.size() > 1) {
                cout << " (and " << kv.second.size() - 1 << " more)";
            }
            cout << endl;
        }
        count++;
    }
    cout << "  ... verified " << goldbachPairs.size() << " even numbers up to " << maxCheck << endl;
}

//=============================================================================
// TWIN PRIME RESONANCE
//=============================================================================

void analyzeTwinPrimes(const vector<int>& primes) {
    cout << "\n=== TWIN PRIME RESONANCE ===" << endl;
    
    int twinCount = 0;
    for (size_t i = 1; i < primes.size(); i++) {
        if (primes[i] - primes[i-1] == 2) {
            twinCount++;
        }
    }
    
    cout << "Twin prime pairs up to " << primes.back() << ": " << twinCount << endl;
    cout << "Twin prime density: " << fixed << setprecision(4) 
         << (1000.0 * twinCount / primes.size()) << " per 1000 primes" << endl;
    
    // Twin prime residue analysis
    cout << "\nTwin prime residue classes (mod 6):" << endl;
    map<int, int> twinResidue;
    for (size_t i = 1; i < primes.size(); i++) {
        if (primes[i] - primes[i-1] == 2) {
            int p = primes[i-1];
            twinResidue[p % 6]++;
        }
    }
    for (int r = 0; r < 6; r++) {
        cout << "  mod 6 = " << r << ": " << twinResidue[r] << " twin pairs" << endl;
    }
    // Note: All twin primes (except 3,5) are ≡ 5 mod 6 (i.e., 11, 17, 29, 41...)
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "  Prime Pattern Finder" << endl;
    cout << "  314 Principle - Modular Prime Dynamics" << endl;
    cout << "  For ModularResonance-AI Research" << endl;
    cout << "========================================" << endl;
    
    int limit = 500;
    if (argc > 1) {
        limit = atoi(argv[1]);
        if (limit < 10) limit = 10;
        if (limit > 100000) limit = 100000;
    }
    
    cout << "\nGenerating primes up to " << limit << "..." << endl;
    
    auto start = chrono::high_resolution_clock::now();
    vector<int> primes = sieveOfEratosthenes(limit);
    auto end = chrono::high_resolution_clock::now();
    
    cout << "Found " << primes.size() << " primes in ";
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
    cout << fixed << setprecision(2) << (duration.count() / 1000.0) << " ms" << endl;
    
    // Run all analyses
    analyzeMod3(primes);
    analyzeMod314(primes);
    analyzeWaveInterference(primes, limit);
    analyzeGoldbach(primes);
    analyzeTwinPrimes(primes);
    
    cout << "\n========================================" << endl;
    cout << "  Analysis Complete" << endl;
    cout << "  ModularResonance-AI Research" << endl;
    cout << "========================================" << endl;
    
    return 0;
}
