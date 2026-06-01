# C++ Practice Session — April 11, 2026

**Status:** COMPLETED — compiled and ran successfully

## What I Studied
- Lambda captures: [=], [&], [x], mutable
- Function objects (functors): struct with operator()
- accumulate: fold/reduce with custom binary operations
- remove_if + erase: proper element removal idiom

## What I Created
**File:** `workspace/learning/cpp_practice/cpp-practice-2026-04-11.cpp`

## Key Concepts
- `auto fn = [capture](params) mutable { ... }` — lambda with state
- `struct Functor { T operator()(args) const { ... } };` — functor
- `accumulate(v.begin(), v.end(), init, [](acc, val){ return acc+val; })` — fold
- `remove_if(v.begin(), v.end(), pred)` + `v.erase(newEnd, v.end())` — proper erase

## DSR N-RANGE RESULTS

**Phase 10: Twin prime phase angle across n-ranges**

### Twin Primes (gap = 2)
| Range | Pairs | Near-π | %Near-π | Mean(π) |
|-------|-------|--------|---------|---------|
| 500   | 24    | 10     | 41.7%   | 0.586π  |
| 1000  | 35    | 12     | 34.3%   | 0.570π  |
| 2000  | 61    | 18     | 29.5%   | 0.553π  |
| 5000  | 126   | 32     | 25.4%   | 0.513π  |

### Non-Twin Adjacent Primes (gap > 2)
| Range | Pairs | Near-π | %Near-π | Mean(π) |
|-------|-------|--------|---------|---------|
| 500   | 69    | 15     | 21.7%   | 0.569π  |
| 1000  | 131   | 28     | 21.4%   | 0.530π  |
| 2000  | 240   | 52     | 21.7%   | 0.523π  |
| 5000  | 541   | 124    | 22.9%   | 0.518π  |

## Interpretation

**Twin primes DECAY from 41.7% → 25.4% as n increases.** The 41.7% was a small-n artifact (only 24 pairs). Non-twin adjacent primes are stable at ~22% across all ranges.

**Both rates fall BELOW the 33% random baseline** for large n — suggesting opposite phase is actually disfavored for large primes.

**The helix is NOT coming from twin prime pairing specifically.** The 6k±1 global residue structure (shared by ALL primes in the same residue class) is the more likely structural origin.

**Next for Phase 10:** Test phase angles for primes in 6k-1 vs 6k+1 residue classes directly. If those show stronger phase separation than the gap-2 pairing, the residue class hypothesis is confirmed.
