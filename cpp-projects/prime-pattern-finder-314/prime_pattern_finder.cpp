/**
 * Prime Pattern Finder - 314 Principle (Modular Prime Dynamics)
 * For ModularResonance-AI Research
 * 
 * Implements modular arithmetic patterns in prime numbers using the 314 approach:
 * - 3: Primary resonance patterns (mod 3)
 * - 1: Unified wave functions (mod 2)  
 * - 4: Harmonic overtones (mod 5, 7, 11)
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace std;

// Check if a number is prime
bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

// Calculate modular residue patterns (the 314 "fingerprint")
struct PrimeSignature {
    int prime;
    int mod3;      // 3-principle: primary resonance
    int mod5;      // Harmonic overtone
    int mod7;      // Harmonic overtone
    int mod11;     // Harmonic overtone
    int wavePhase; // Derived wave phase
};

// Calculate resonance strength between two primes
double resonanceStrength(int p1, int p2, int modulus) {
    int diff = abs(p1 - p2);
    int residue = diff % modulus;
    // Higher resonance when residue is 0 or near centers
    return 1.0 - (double)residue / modulus;
}

int main(int argc, char* argv[]) {
    int limit = 500;
    if (argc > 1) {
        limit = atoi(argv[1]);
    }
    
    cout << "========================================" << endl;
    cout << "  Prime Pattern Finder" << endl;
    cout << "  314 Principle - Modular Prime Dynamics" << endl;
    cout << "========================================" << endl;
    cout << endl;
    
    // Find all primes up to limit
    vector<int> primes;
    for (int i = 2; i <= limit; i++) {
        if (isPrime(i)) {
            primes.push_back(i);
        }
    }
    
    cout << "Found " << primes.size() << " primes up to " << limit << endl;
    cout << endl;
    
    // Calculate signatures for first 30 primes (sample)
    int sampleSize = min(30, (int)primes.size());
    vector<PrimeSignature> signatures;
    
    cout << "--- Prime Signatures (mod 3, 5, 7, 11) ---" << endl;
    cout << setw(6) << "Prime" 
         << setw(8) << "mod3" 
         << setw(8) << "mod5" 
         << setw(8) << "mod7" 
         << setw(8) << "mod11" 
         << setw(10) << "Phase" << endl;
    cout << string(48, '-') << endl;
    
    for (int i = 0; i < sampleSize; i++) {
        int p = primes[i];
        PrimeSignature sig;
        sig.prime = p;
        sig.mod3 = p % 3;
        sig.mod5 = p % 5;
        sig.mod7 = p % 7;
        sig.mod11 = p % 11;
        sig.wavePhase = (p % 3) * 64 + (p % 5) * 16 + (p % 7) * 4 + (p % 11);
        signatures.push_back(sig);
        
        cout << setw(6) << p 
             << setw(8) << sig.mod3 
             << setw(8) << sig.mod5 
             << setw(8) << sig.mod7 
             << setw(8) << sig.mod11 
             << setw(10) << sig.wavePhase << endl;
    }
    
    cout << endl;
    
    // Analyze resonance patterns
    cout << "--- Resonance Analysis (mod 3 Principle) ---" << endl;
    vector<int> mod3Groups[3];
    for (int p : primes) {
        mod3Groups[p % 3].push_back(p);
    }
    
    for (int i = 0; i < 3; i++) {
        cout << "Primes ≡ " << i << " (mod 3): " << mod3Groups[i].size() << " primes";
        if (mod3Groups[i].size() <= 8) {
            cout << " [";
            for (size_t j = 0; j < mod3Groups[i].size(); j++) {
                cout << mod3Groups[i][j];
                if (j < mod3Groups[i].size() - 1) cout << ", ";
            }
            cout << "]";
        } else {
            cout << " [first 8: ";
            for (int j = 0; j < 8; j++) cout << mod3Groups[i][j] << ", ";
            cout << "...]";
        }
        cout << endl;
    }
    
    cout << endl;
    
    // Twin prime detection with modular patterns
    cout << "--- Twin Prime Pairs with Modular Signatures ---" << endl;
    int twinCount = 0;
    for (size_t i = 0; i < primes.size() - 1; i++) {
        if (primes[i + 1] - primes[i] == 2) {
            twinCount++;
            if (twinCount <= 10) {  // Show first 10
                int p1 = primes[i];
                int p2 = primes[i + 1];
                cout << "(" << p1 << ", " << p2 << ") ";
                cout << "mod3: (" << p1 % 3 << ", " << p2 % 3 << ") ";
                cout << "mod5: (" << p1 % 5 << ", " << p2 % 5 << ") ";
                cout << "mod7: (" << p1 % 7 << ", " << p2 % 7 << ")";
                cout << endl;
            }
        }
    }
    cout << endl << "Total twin prime pairs found: " << twinCount << endl;
    
    cout << endl;
    
    // Goldbach analysis with modular patterns
    cout << "--- Goldbach Conjecture Analysis (even numbers up to " << min(limit, 100) << ") ---" << endl;
    int goldbachVerified = 0;
    for (int even = 4; even <= min(limit, 100); even += 2) {
        bool found = false;
        for (int p1 : primes) {
            if (p1 >= even) break;
            int p2 = even - p1;
            if (isPrime(p2)) {
                found = true;
                break;
            }
        }
        if (found) goldbachVerified++;
    }
    cout << "Goldbach verified for " << goldbachVerified << " even numbers up to " << min(limit, 100) << endl;
    
    cout << endl;
    
    // Inter-prime distance patterns (mod 3, 5, 7, 11)
    cout << "--- Prime Gap Distribution (mod 4 = 2n+2 gaps) ---" << endl;
    vector<int> gapMod3[3], gapMod5[5], gapMod7[7], gapMod11[11];
    
    for (size_t i = 0; i < primes.size() - 1; i++) {
        int gap = primes[i + 1] - primes[i];
        if (gap > 1) {
            gapMod3[gap % 3].push_back(gap);
            gapMod5[gap % 5].push_back(gap);
            gapMod7[gap % 7].push_back(gap);
            gapMod11[gap % 11].push_back(gap);
        }
    }
    
    cout << "Gap % 3 distribution: ";
    for (int i = 0; i < 3; i++) {
        cout << "[" << i << "]=" << gapMod3[i].size() << " ";
    }
    cout << endl;
    
    cout << "Gap % 5 distribution: ";
    for (int i = 0; i < 5; i++) {
        cout << "[" << i << "]=" << gapMod5[i].size() << " ";
    }
    cout << endl;
    
    cout << "Gap % 7 distribution: ";
    for (int i = 0; i < 7; i++) {
        cout << "[" << i << "]=" << gapMod7[i].size() << " ";
    }
    cout << endl;
    
    cout << "Gap % 11 distribution: ";
    for (int i = 0; i < 11; i++) {
        cout << "[" << i << "]=" << gapMod11[i].size() << " ";
    }
    cout << endl;
    
    cout << endl;
    
    // 314 Resonance Summary
    cout << "========================================" << endl;
    cout << "  314 PRINCIPLE SUMMARY" << endl;
    cout << "========================================" << endl;
    cout << endl;
    
    double totalRes3 = 0, totalRes5 = 0, totalRes7 = 0, totalRes11 = 0;
    int pairs = 0;
    
    for (size_t i = 0; i < primes.size() - 1 && i < 100; i++) {
        for (size_t j = i + 1; j < primes.size() && j < i + 10; j++) {
            totalRes3 += resonanceStrength(primes[i], primes[j], 3);
            totalRes5 += resonanceStrength(primes[i], primes[j], 5);
            totalRes7 += resonanceStrength(primes[i], primes[j], 7);
            totalRes11 += resonanceStrength(primes[i], primes[j], 11);
            pairs++;
        }
    }
    
    if (pairs > 0) {
        cout << "Average Inter-Prime Resonance Strength:" << endl;
        cout << "  Mod 3:  " << fixed << setprecision(4) << (totalRes3 / pairs) << " (Primary)" << endl;
        cout << "  Mod 5:  " << (totalRes5 / pairs) << " (Harmonic)" << endl;
        cout << "  Mod 7:  " << (totalRes7 / pairs) << " (Harmonic)" << endl;
        cout << "  Mod 11: " << (totalRes11 / pairs) << " (Harmonic)" << endl;
    }
    
    cout << endl;
    cout << "========================================" << endl;
    cout << "  Analysis Complete" << endl;
    cout << "========================================" << endl;
    
    return 0;
}
