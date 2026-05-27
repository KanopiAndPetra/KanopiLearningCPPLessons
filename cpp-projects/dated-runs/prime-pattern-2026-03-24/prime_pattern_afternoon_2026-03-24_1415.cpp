// Prime Pattern Finder - 314 Principle (Modular Prime Dynamics)
// For ModularResonance-AI Research
// Exploring mathematical patterns in prime numbers

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <map>

using namespace std;

// Check if a number is prime
bool isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int i = 3; i <= sqrt(n); i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// Calculate digit sum
int digitSum(int n) {
    int sum = 0;
    while (n > 0) {
        sum += n % 10;
        n /= 10;
    }
    return sum;
}

// Calculate modular residue for base 314 (Pi relationship)
int mod314(int n) {
    return n % 314;
}

int main(int argc, char* argv[]) {
    int limit = 500;
    if (argc > 1) {
        limit = stoi(argv[1]);
    }

    cout << "========================================\n";
    cout << "  Prime Pattern Finder - 314 Principle\n";
    cout << "  Modular Prime Dynamics Analysis\n";
    cout << "========================================\n\n";

    vector<int> primes;
    for (int i = 2; i <= limit; i++) {
        if (isPrime(i)) primes.push_back(i);
    }

    cout << "Total primes found up to " << limit << ": " << primes.size() << "\n\n";

    // Pattern 1: Prime gaps analysis
    cout << "--- Prime Gap Analysis ---\n";
    vector<int> gaps;
    for (size_t i = 1; i < primes.size(); i++) {
        gaps.push_back(primes[i] - primes[i-1]);
    }
    
    map<int, int> gapFreq;
    for (int g : gaps) gapFreq[g]++;
    
    cout << "Most common gaps:\n";
    vector<pair<int,int>> sortedGaps(gapFreq.begin(), gapFreq.end());
    sort(sortedGaps.begin(), sortedGaps.end(), 
         [](auto &a, auto &b) { return a.second > b.second; });
    
    for (size_t i = 0; i < min(size_t(5), sortedGaps.size()); i++) {
        cout << "  Gap " << sortedGaps[i].first << ": " << sortedGaps[i].second << " times\n";
    }
    cout << "\n";

    // Pattern 2: Modular 314 distribution
    cout << "--- Modular 314 Distribution (Pi Connection) ---\n";
    map<int, int> mod314Freq;
    for (int p : primes) {
        mod314Freq[mod314(p)]++;
    }
    
    // Show distribution across key regions
    int regionSize = 314 / 4;
    for (int r = 0; r < 4; r++) {
        int start = r * regionSize;
        int end = (r == 3) ? 314 : (r + 1) * regionSize;
        int count = 0;
        for (int m = start; m < end; m++) {
            count += mod314Freq[m];
        }
        double pct = 100.0 * count / primes.size();
        cout << "  Region " << start << "-" << end << ": " << count << " primes (" 
             << fixed << setprecision(1) << pct << "%)\n";
    }
    cout << "\n";

    // Pattern 3: Digit sum patterns
    cout << "--- Digit Sum Prime Analysis ---\n";
    map<int, vector<int>> digitSumPrimes;
    for (int p : primes) {
        int ds = digitSum(p);
        digitSumPrimes[ds].push_back(p);
    }
    
    cout << "Primes with digit sum = 2 (first prime factors):\n  ";
    for (int p : digitSumPrimes[2]) {
        cout << p << " ";
    }
    cout << "\n";
    
    cout << "Primes with digit sum = 3 (trinity pattern):\n  ";
    for (int p : digitSumPrimes[3]) {
        cout << p << " ";
    }
    cout << "\n";
    
    cout << "Primes with digit sum = 7 (sacred/cycle number):\n  ";
    for (int p : digitSumPrimes[7]) {
        cout << p << " ";
    }
    cout << "\n\n";

    // Pattern 4: Prime clusters near 314
    cout << "--- Prime Clusters Around 314 ---\n";
    for (int base = 310; base <= 320; base++) {
        if (isPrime(base)) {
            cout << "  " << base << " [PRIME]";
        } else {
            cout << "  " << base;
        }
        if (base % 5 == 4) cout << "\n";
    }
    cout << "\n\n";

    // Pattern 5: Modular resonance check
    cout << "--- Modular Resonance (314 Base) ---\n";
    cout << "Primes that are perfect mod 314 residues:\n  ";
    int resonanceCount = 0;
    for (int p : primes) {
        if (p % 314 == 0) {
            cout << p << " ";
            resonanceCount++;
        }
    }
    if (resonanceCount == 0) cout << "(none in range)";
    cout << "\n\n";

    // Summary
    cout << "--- 314 Principle Summary ---\n";
    double avgGap = 0;
    for (int g : gaps) avgGap += g;
    avgGap /= gaps.size();
    
    cout << "  Average prime gap: " << fixed << setprecision(2) << avgGap << "\n";
    cout << "  314 Mod distribution: " << (mod314Freq[0] + mod314Freq[314]) 
         << " primes at x314 boundary\n";
    cout << "  Digit sum 7 primes: " << digitSumPrimes[7].size() << "\n";
    cout << "\n  [ModularResonance-AI: Patterns in primes may inform\n";
    cout << "   modular arithmetic optimization for AI resonance models]\n";
    cout << "========================================\n";

    return 0;
}
