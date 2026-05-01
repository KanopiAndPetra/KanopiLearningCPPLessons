#!/bin/bash
# Kanopi C++ Prime Day Script
# Prime Pattern Finder - 314 Principle (Modular Prime Dynamics)
# For ModularResonance-AI Research

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "========================================"
echo "  Running Prime Pattern Finder"
echo "  314 Principle - Modular Prime Dynamics"
echo "========================================"

# Compile if needed
if [ ! -f "prime_pattern_finder" ]; then
    echo "Compiling prime_pattern_finder.cpp..."
    g++ -std=c++17 -O2 -o prime_pattern_finder prime_pattern_finder.cpp
    if [ $? -ne 0 ]; then
        echo "Compilation failed!"
        exit 1
    fi
    echo "Compilation successful!"
fi

# Run with default or custom limit
LIMIT=${1:-500}
echo "Analyzing primes up to: $LIMIT"
echo ""

./prime_pattern_finder $LIMIT | tee prime_pattern_output.txt

echo ""
echo "========================================"
echo "  Output saved to prime_pattern_output.txt"
echo "========================================"
