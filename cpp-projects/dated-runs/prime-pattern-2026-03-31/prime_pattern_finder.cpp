// Prime Pattern Finder - 314 Principle (Modular Prime Dynamics)
// For ModularResonance-AI Research
// Kanopi C++ Learning Session - March 30, 2026

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

// 3-1-4 Modular Prime Dynamics Core
// pi(n) ~ n / log(n)           (Prime Number Theorem)
// Modular resonance: primes cluster near multiples of 6
// 314 Principle: every prime > 3 ≡ ±1 (mod 6) — sits on the spine of 6k±1

struct ResonanceResult {
    int prime;
    int residue_mod6;
    int resonance_strength;   // how close to 6k±1 boundary
    int wave_interference;     // constructive (1) or destructive (-1)
    double modular_phase;      // angle in the 314 cycle
};

bool isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// 314 Principle: every prime > 3 lives on the 6k±1 "spine"
int getResidueMod6(int p) {
    return ((p % 6) + 6) % 6;  // normalize to 0-5
}

int getResonanceStrength(int p) {
    // Strength = distance from nearest 6k boundary
    int r = getResidueMod6(p);
    if (r == 1) return 1;     // 6k+1 — prime resonance
    if (r == 5) return 1;     // 6k-1 (≡ -1 mod 6) — prime resonance
    return 0;                  // 0,2,3,4 — not prime (except 2,3)
}

double getModularPhase(int p) {
    // Phase within the 314 cycle: 3→1→4→3→1→4...
    // Maps to angle: (p mod 314) / 314 * 2π
    return fmod((double)p, 314.0) / 314.0 * 2.0 * M_PI;
}

// Wave interference: primes as standing waves on modular ring
int getWaveInterference(int p) {
    // Constructive interference when prime aligns with 6k±1 spine
    // Destructive when falling in gaps
    int r = getResidueMod6(p);
    return (r == 1 || r == 5) ? 1 : -1;
}

// Analyze primes up to limit
std::vector<ResonanceResult> analyzePrimes(int limit) {
    std::vector<ResonanceResult> results;
    
    for (int n = 2; n <= limit; ++n) {
        if (isPrime(n)) {
            ResonanceResult r;
            r.prime = n;
            r.residue_mod6 = getResidueMod6(n);
            r.resonance_strength = getResonanceStrength(n);
            r.modular_phase = getModularPhase(n);
            r.wave_interference = getWaveInterference(n);
            results.push_back(r);
        }
    }
    return results;
}

// Print header for 314 analysis
void print314Analysis(const std::vector<ResonanceResult>& primes) {
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          314 PRINCIPLE - MODULAR PRIME DYNAMICS                  ║\n";
    std::cout << "║     Every prime > 3 sits on the 6k±1 spine: p ≡ ±1 (mod 6)        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

// Print table of primes with their modular properties
void printPrimeTable(const std::vector<ResonanceResult>& primes) {
    std::cout << std::setw(8) << "Prime" 
              << std::setw(10) << "p mod 6" 
              << std::setw(14) << "Resonance" 
              << std::setw(12) << "Interference"
              << std::setw(14) << "ModularPhase" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (const auto& r : primes) {
        std::cout << std::setw(8) << r.prime
                  << std::setw(10) << r.residue_mod6
                  << std::setw(14) << (r.resonance_strength ? "ON SPINE" : "GAP")
                  << std::setw(12) << (r.wave_interference > 0 ? "CONSTRUCTIVE" : "DESTRUCTIVE")
                  << std::setw(13) << std::fixed << std::setprecision(3) << r.modular_phase << "\n";
    }
}

// Statistical summary of the 314 principle
void print314Stats(const std::vector<ResonanceResult>& primes, int limit) {
    int spine_count = 0;
    int on_spine = 0;
    
    for (const auto& r : primes) {
        if (r.prime > 3) {
            spine_count++;
            if (r.resonance_strength == 1) on_spine++;
        }
    }
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     314 PRINCIPLE STATISTICS                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "  Search limit:          " << limit << "\n";
    std::cout << "  Total primes found:    " << primes.size() << "\n";
    std::cout << "  Primes on 6k±1 spine: " << on_spine << " / " << spine_count << "\n";
    if (spine_count > 0) {
        std::cout << "  Spine accuracy:       " << std::fixed << std::setprecision(2) 
                  << (100.0 * on_spine / spine_count) << "%\n";
    }
    std::cout << "  Prime Number Theorem π(" << limit << ") ≈ " << std::setprecision(0) 
              << (double)limit / log(limit) << "\n";
    std::cout << "  Actual π(" << limit << ") = " << primes.size() << "\n";
}

// Explore 314 modular cycles
void explore314Cycles(int limit) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              314 MODULAR CYCLE EXPLORATION                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "  Prime clusters near 314-cycle boundaries:\n\n";
    
    for (int cycle = 0; cycle < limit / 314; ++cycle) {
        int base = cycle * 314;
        int count = 0;
        int strongest_resonance = 0;
        int resonance_prime = 0;
        
        for (int offset = 1; offset <= 314; ++offset) {
            int p = base + offset;
            if (isPrime(p) && getResidueMod6(p) == 1) {
                count++;
                // Find strongest resonance (closest to boundary)
                int r5 = getResidueMod6(p + 4);  // distance to next 6k-1
                if (r5 == 5 && p > resonance_prime) {
                    resonance_prime = p;
                    strongest_resonance = 1;
                }
            }
        }
        
        if (count > 0) {
            std::cout << "  Cycle " << std::setw(4) << cycle << " (n=" << std::setw(5) << base 
                      << "): " << count << " primes on 6k+1 spine";
            if (strongest_resonance) {
                std::cout << " | Strongest: " << resonance_prime;
            }
            std::cout << "\n";
        }
    }
}

// Wave interference pattern
void printWavePattern(const std::vector<ResonanceResult>& primes) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║             WAVE INTERFERENCE PATTERN (first 20 primes)         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "  Prime: Resonance Wave (∿ = constructive, × = destructive)\n";
    std::cout << "  " << std::string(50, '-') << "\n";
    
    int shown = 0;
    for (const auto& r : primes) {
        if (r.prime > 3) {
            std::string wave = r.wave_interference > 0 ? "∿∿∿∿" : "××××";
            std::cout << "  " << std::setw(5) << r.prime << ": " << wave;
            if (shown % 4 == 3) std::cout << "\n";
            shown++;
            if (shown >= 20) break;
        }
    }
    std::cout << "\n";
}

// Main
int main(int argc, char* argv[]) {
    int limit = 500;
    if (argc > 1) {
        limit = std::atoi(argv[1]);
    }
    
    std::cout << "\n";
    std::cout << "  ╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "  ║         KANOPI PRIME PATTERN FINDER                   ║\n";
    std::cout << "  ║         314 Principle — Modular Prime Dynamics        ║\n";
    std::cout << "  ║         Applied to ModularResonance-AI Research       ║\n";
    std::cout << "  ║         Session: 2026-03-30                           ║\n";
    std::cout << "  ╚═══════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "  Analyzing primes up to " << limit << "...\n\n";
    
    auto primes = analyzePrimes(limit);
    
    print314Analysis(primes);
    printPrimeTable(primes);
    print314Stats(primes, limit);
    explore314Cycles(limit);
    printWavePattern(primes);
    
    std::cout << "\n  ═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  Kanopi C++ Learning — Prime Pattern Finder Complete\n";
    std::cout << "  314 Principle: primes live on the 6k±1 modular spine\n";
    std::cout << "  ═══════════════════════════════════════════════════════════════════\n";
    
    return 0;
}
