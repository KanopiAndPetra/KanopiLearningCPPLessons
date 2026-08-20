# P-2026-08-20 — std::flat_map / std::flat_set / std::flat_multimap / std::flat_multiset (C++23)

Modern-C++ lesson for today. Continues the C++23 stdlib-tour arc
that started with `std::span` (Jul 9), `std::mdspan` (Jul 10), and
`std::expected` (Jul 12):

```
Jul  9   std::span     (C++20) — 1-D non-owning view
Jul 10   std::mdspan   (C++23) — N-D non-owning view
Jul 12   std::expected (C++23) — sum-type error channel
today    std::flat_map family (C++23) — cache-friendly sorted-vector
                                associative containers
```

## Headline

| Build | Result |
|-------|--------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`) | **81/81 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **81/81 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **81/81 PASS**, clean sanitizer output |

100x ASan/UBSan stress run of 100k bulk insert + 50k find + bulk
erase: also clean (zero leaks, zero UB, zero double-free on the
two underlying vectors, zero use-after-free on the iterator /
sortable range).

| Section | Topic | Tests |
|---------|-------|-------|
| 1 | Toolchain + feature-test probe + sizeof | 5 |
| 2 | Minimum viable flat_map (emplace / operator[] / find / contains / erase) | 19 |
| 3 | Iteration order is always sorted (the core invariant) | 4 |
| 4 | Iterator category — random_access_iterator_tag | 8 |
| 5 | Bulk insert — the O(n) sort+dedupe+merge path | 6 |
| 6 | Transparent comparator + heterogeneous lookup | 7 |
| 7 | Custom comparator — case-insensitive lookup | 9 |
| 8 | extract() + replace() — rvalue-qualified ownership transfer | 6 |
| 9 | try_emplace + emplace_hint + insert_or_assign | 7 |
| 10 | flat_set / flat_multimap / flat_multiset | 8 |
| 11 | Empirics — flat_map vs std::map (bulk + lookup + iterate) | 2 |
| 12 | ASan/UBSan stress run (100k bulk + 50k lookup + iterate + bulk erase) ×100 | 1 |
| **Total** | | **81** |

## What std::flat_map IS

`std::flat_map<Key, T, Compare, Allocator>` is a sorted
associative container that stores its contents as TWO contiguous
vectors, kept parallel and sorted by `Compare`:

```cpp
namespace psp {
    // mental model of std::flat_map<int, std::string>:
    struct flat_map_int_string {
        std::vector<int>          keys;     // sorted
        std::vector<std::string>  values;   // parallel to keys
        std::less<int>            comp;     // the comparator
    };
}
```

It is **the same data structure as a sorted
`std::vector<std::pair<Key, T>>` walked with `std::lower_bound`** —
but with the standard library's associative API (`find`, `count`,
`lower_bound`, `equal_range`, `contains`, ...).

The same shape is repeated for the other three:

| Type | Sorted by | Underlying storage |
|------|-----------|--------------------|
| `std::flat_map<K, V, C, A>` | `K` | `vector<K>` + `vector<V>` |
| `std::flat_set<K, C, A>` | `K` | `vector<K>` |
| `std::flat_multimap<K, V, C, A>` | `K` | `vector<K>` + `vector<V>` (duplicates kept) |
| `std::flat_multiset<K, C, A>` | `K` | `vector<K>` (duplicates kept) |

## What you trade

- **Insertion / erasure at arbitrary positions: O(n)** (shift the
  tail of both vectors). `std::map` is O(log n).
- **Iterator invalidation: ANY insert / erase invalidates ALL
  iterators** (the vectors reallocate). `std::map` only
  invalidates the affected iterators.

## What you GAIN

- **Lookup: O(log n)** (binary search) with cache-friendly
  contiguous memory — typically 2x faster than `std::map`.
- **Iteration: cache-friendly vector walk** — typically 50-200x
  faster than `std::map` for full traversals (see Section 11).
- **BULK INSERT: O(n) total** because the container just sorts the
  new range and merges with the existing sorted range.
  `std::map` bulk-insert is O(n log n) — one tree rotation per
  element.
- **Footprint: smaller** — two vectors, no per-node tree overhead
  (no per-node color bit, no parent/child pointers, no node
  allocation).
- **Cache locality for range scans** — the whole point.

## Section 1 — sizeof and the data-structure cost

`sizeof(std::flat_map<int,int>)` is **48 B** (two `std::vector`s
on libc++ × 24 B + the comparator + padding). `std::map<int,int>`
is 24 B on the same toolchain — *smaller per object*, but the
*per-element cost* is much larger (each tree node is a separate
allocation with parent/child pointers + color bit + value).

| Type | `sizeof` (libc++) | Per-element cost |
|------|-------------------|------------------|
| `std::flat_map<int,int>` | 48 B | 8 B (key + value inline) |
| `std::flat_set<int>` | 24 B | 4 B (key inline) |
| `std::flat_multimap<int,int>` | 48 B | 8 B |
| `std::flat_multiset<int>` | 24 B | 4 B |
| `std::map<int,int>` | 24 B | ~40 B per node (heap allocation + ptrs + color bit) |
| `std::set<int>` | 24 B | ~32 B per node |

The lesson: **for a map with N elements, `std::flat_map` is one
allocation (the two vectors) and uses contiguous memory; `std::map`
is N heap allocations, one per node, scattered across the heap**.
That is the cache-friendliness story in one number.

## Section 2 — minimum viable flat_map

The basic API is identical in spirit to `std::map`:

```cpp
std::flat_map<int, std::string> m;
m.emplace(3, "three");
m.emplace(1, "one");     // sorts on insert
m.emplace(2, "two");

for (auto const& [k, v] : m) std::cout << k << ":" << v;
// → 1:one 2:two 3:three      (sorted!)

m[2];         // operator[] returns the mapped value
m[99] = "..."; // operator[] on absent key INSERTS (size goes up)

m.find(2);      // iterator to {2, "two"} or end()
m.count(3);     // 1 or 0
m.contains(0);  // bool

m.erase(0);     // by key — returns 1 (was present) or 0
m.erase(it);    // by iterator
m.clear();
```

19 tests cover: empty-construction, emplace-in-random-order,
sorted-iteration invariant, `operator[]` insert-on-miss, `find`,
`count`, `contains`, erase-by-key (present + absent), erase-by-
iterator, `clear`.

## Section 3 — iteration is always sorted

This is the **defining property** of every flat_* container.
Insertion order is irrelevant — the container sorts on insert:

```cpp
std::flat_map<int, int> inv;
for (int i = 100; i >= 0; --i) inv.emplace(i, i * i);

std::vector<int> seen;
for (auto const& kv : inv) seen.push_back(kv.first);
// seen == {0, 1, 2, ..., 100}   (sorted, even though
//                                inserted in reverse)
```

Duplicate-key `emplace` does NOT overwrite (size is unchanged, the
FIRST inserted value wins) — same semantic as `std::map::emplace`.

## Section 4 — iterator category is random_access

```cpp
using fit = std::flat_map<int,int>::iterator;
static_assert(std::is_same_v<
    std::iterator_traits<fit>::iterator_category,
    std::random_access_iterator_tag>);  // ← key insight

using mit = std::map<int,int>::iterator;
static_assert(std::is_same_v<
    std::iterator_traits<mit>::iterator_category,
    std::bidirectional_iterator_tag>);  // ← tree walk
```

Because the underlying storage is a vector, the iterator supports
random-access arithmetic:

```cpp
auto b = m.begin(), e = m.end();
(e - b) == m.size();       // iterator difference
(b + 5)->first == 5;       // iterator + N jumps to the Nth element
(e - 3)->first == 7;       // iterator - N from end()
b[7].first == 7;           // iterator[N] is the Nth element
```

This is the reason flat_* iterators are useful for random-access
algorithms (e.g. `std::lower_bound` over a flat_* range is a
binary search).

## Section 5 — the killer feature: bulk insert

`std::flat_map`'s range-insert is O(n + m) where n is the existing
size and m is the inserted range — the container just sorts the
new range, dedupes, and merges with the existing sorted vector.
This is the **single biggest reason** `flat_map` exists: bulk
loading is dramatically faster than `std::map`'s O((n+m) log (n+m))
tree-rotation path.

```cpp
std::flat_map<int, int> bulk;
std::vector<std::pair<int, int>> input = /* 100 unsorted pairs */;
bulk.insert(input.begin(), input.end());  // O(n) total
```

The `std::sorted_unique` tag is a **precondition assertion** that
tells the implementation "the input is already sorted and deduped,
do an O(n) merge instead of sort+dedupe":

```cpp
// Pre-sorted + deduped input — sorted_unique is safe
std::flat_set<int> pre_sorted;
const std::vector<int> pre = {1, 2, 3, 4, 5};
pre_sorted.insert(std::sorted_unique, pre.begin(), pre.end());
// → pre_sorted iterates {1, 2, 3, 4, 5}  (correct)
```

### Real-world gotcha: don't pass unsorted input with `sorted_unique`

In libc++, the `sorted_unique` precondition is **not enforced**
— passing unsorted input produces an output that preserves the
input order, which violates the standard's sortedness invariant.
The standard says passing unsorted input to `sorted_unique` is
UB; libc++ gives you a silently-wrong result.

```cpp
// Unsorted input + sorted_unique tag — libc++ does NOT sort
std::flat_set<int> unsorted_in;
const std::vector<int> unsorted = {7, 2, 5, 1, 4};
unsorted_in.insert(std::sorted_unique,
                   unsorted.begin(), unsorted.end());
// → unsorted_in iterates {7, 2, 5, 1, 4}  (WRONG!)
//   Should have been {1, 2, 4, 5, 7}
```

**Lesson**: always pass the ordinary `insert()` if input order is
not guaranteed. Use `sorted_unique` ONLY when you can prove the
input is already sorted + deduped (e.g. output of another sorted
container).

### Second-order gotcha: don't call `.begin()` / `.end()` on different temporaries

```cpp
// WRONG: two different temporary vectors — iterators refer to
// independent objects with NO guarantee of being paired
flat_set.insert(std::sorted_unique,
                std::vector<int>{1,2,3}.begin(),
                std::vector<int>{1,2,3}.end());
// → undefined behaviour; empirically interleaves the two buffers

// RIGHT: one named vector for both ends
const std::vector<int> v = {1,2,3};
flat_set.insert(std::sorted_unique, v.begin(), v.end());
```

This is a real C++ lesson independent of `flat_set`: in a
function call expression, every temporary vector is a distinct
object. Calling `.begin()` and `.end()` on TWO different
temporaries gives you two iterators into two unrelated buffers.

## Section 6 — transparent comparator + heterogeneous lookup

`std::flat_map<std::string, int>` with `KeyCompare = std::less<>`
(the void-pointer specialization) enables **heterogeneous lookup**:
`find("hello")` works without allocating a temporary `std::string`
on the heap.

```cpp
std::flat_map<std::string, int, std::less<>> tm;
tm.emplace("hello", 1);
tm.emplace("world", 2);

// Heterogeneous: const char* — NO temporary std::string constructed
auto h1 = tm.find("hello");          // ✓
auto h2 = tm.find(std::string_view("world"));  // ✓

// contains() is also heterogeneous
tm.contains("hello");   // ✓
tm.contains("nope");    // ✓
```

The mechanism is `std::less<>` (the void-pointer specialization
of `std::less`): it accepts ANY two types comparable to `Key`
(`const char*`, `std::string_view`, `std::string`), and the
container's `find` overload set picks up the heterogeneous
overload.

For `std::map` the heterogeneous lookup has been available since
C++14; for `std::flat_map` it's there from C++23. This is the
*exact same pattern* — the container does not own any temporary
when the key is `const char*` or `std::string_view`.

## Section 7 — custom comparator: case-insensitive lookup

The canonical heterogeneous-lookup use case. Define a comparator
with `using is_transparent = void;` (the one-bit opt-in) and a
single `string_view` overload:

```cpp
struct ci_less {
    using is_transparent = void;   // ← THE opt-in

    bool operator()(std::string_view a, std::string_view b) const {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(),
            [](unsigned char x, unsigned char y) {
                return std::tolower(x) < std::tolower(y);
            });
    }
};

std::flat_map<std::string, int, ci_less> ci;
ci.emplace("Foo", 1); ci.emplace("BAR", 2); ci.emplace("baz", 3);

// Iteration uses the comparator — case-insensitive sort
// → BAR:2  baz:3  Foo:1      (B < b < F under ci_less)

// Heterogeneous lookup with const char*
ci.find("FOO");          // → finds the "Foo" entry
ci.find("bar");          // → finds the "BAR" entry
ci.find("BAZ");          // → finds the "baz" entry
ci.find(std::string_view("bar"));  // → finds the "BAR" entry

ci.contains("FoO");      // true
ci.contains("zz");       // false
```

Without the `is_transparent` opt-in, the `find(const char*)` call
would have to convert the `const char*` to a `std::string` first
(an allocation). With it, the lookup goes straight to
`string_view->compare`, with no allocation.

This pattern is identical to `std::map::find`'s heterogeneous
overloads (which have been there since C++14) — the lesson is
that **the same transparent-comparator pattern works with
flat_map as with map**, and the lookup cost is the same.

## Section 8 — extract() + replace(): the rvalue-qualified ownership transfer

`std::flat_map::extract()` is `&&`-qualified (rvalue-ref-qualified
in the standard) — it returns the two underlying vectors
(`KeyContainer` + `MappedContainer` pair) and leaves `*this` empty.
This is the cheapest possible "give me your data" operation:
**no copy, no per-element move, just a vector ownership
transfer**.

```cpp
std::flat_map<int, std::string> src;
src.emplace(1, "one");
src.emplace(2, "two");
src.emplace(3, "three");

auto ex = std::move(src).extract();
// src is now empty
// ex is a `containers` struct with .keys (vector<int>) and
//                            .values (vector<std::string>)

dst.replace(std::move(ex.keys), std::move(ex.values));
// dst now owns the data; zero copying happened
```

This is the pattern for moving a `flat_map` between containers or
out to a network/serialization layer without paying per-element
move costs. The same pattern exists on `std::map` (with node-level
granularity) but at very different granularity: `flat_map::extract`
moves the whole sorted-vector pair in one operation.

## Section 9 — try_emplace / emplace_hint / insert_or_assign

The fine-grained insert API matches `std::map` exactly:

```cpp
std::flat_map<int, std::string> te;

// try_emplace: like emplace but DOES NOT overwrite an existing key
auto [it1, ins1] = te.try_emplace(2, "second");
// ins1 == true, it1->second == "second"   (inserted)

auto [it2, ins2] = te.try_emplace(2, "OVERWRITE-ATTEMPT");
// ins2 == false   (key already present)
// te[2] is still "second"

// emplace_hint: hint the iterator position
te.emplace_hint(te.end(), std::make_pair(0, "zero"));

// insert_or_assign: insert if absent, ASSIGN if present
auto [it3, ins3] = ioa.insert_or_assign(1, "ONE-AGAIN");
// ins3 == false, ioa[1] is now "ONE-AGAIN"
```

All three return `(iterator, bool_inserted)`. The bool tells the
caller whether the operation actually changed the container
(useful for "was this already there?" checks).

## Section 10 — the other three: flat_set / flat_multimap / flat_multiset

| Type | Duplicates | Lookup | Storage |
|------|------------|--------|---------|
| `flat_map` | NO | O(log n) | two vectors |
| `flat_set` | NO | O(log n) | one vector |
| `flat_multimap` | YES | O(log n) | two vectors |
| `flat_multiset` | YES | O(log n) | one vector |

`flat_set<int>` is just a sorted `vector<int>`. `flat_multimap`
allows duplicate keys; iteration walks keys in comparator order
with duplicates grouped (insertion order is stable within a key
group). `flat_multiset` is `flat_set` with duplicates allowed.

The `std::sorted_equivalent` tag is the multi-key companion to
`std::sorted_unique` — it tells the implementation "the input is
sorted with possible duplicates; do an O(n) merge":

```cpp
std::flat_multiset<int> fms_bulk;
const std::vector<int> eq_input = {1, 2, 2, 3, 3, 3, 4};
fms_bulk.insert(std::sorted_equivalent, eq_input.begin(), eq_input.end());
// → fms_bulk iterates {1, 2, 2, 3, 3, 3, 4}   (correctly sorted + dup'd)
```

## Section 11 — empirics: flat_map vs std::map

This is the headline section — the numbers that motivate
`std::flat_map`'s existence. Run at `-O2` (the canonical
optimised build) on Apple Clang 21.0.0 / libc++ / arm64:

```
N = 50,000 random int-int pairs
================================
                    flat_map      std::map     speedup
bulk insert         6,188 us    13,057 us     2.1x   (flat_map wins)
per-element insert 87,515 us     4,906 us     —       (std::map wins 18x)
lookup 10k finds      226 us       428 us     1.9x   (flat_map wins)
iterate 50k            2 us       389 us     ~200x   (flat_map wins)
```

Run at `-O0` (debug build, same machine, same data):

```
                    flat_map      std::map
bulk insert       110,384 us    14,051 us     (std::map wins at -O0)
per-element insert 85,670 us    12,139 us     (std::map wins at -O0)
lookup 10k finds    3,837 us     1,653 us     (std::map wins at -O0)
iterate 50k           399 us       766 us     (flat_map wins at -O0)
```

### Why -O0 is mixed

At `-O0` (debug build), neither side is optimised. `std::sort` in
the bulk-insert path is not inlined / vectorised; the per-element
`std::sort` calls inside the merge are slow. `std::map` gets to
use the inline-cache-friendly tree-walk that's cheap even without
optimisation. So:

- **The bulk-insert win for flat_map requires -O2** (or any
  release build).
- **The iteration win for flat_map is robust** at every
  optimisation level (vector walk < tree walk even with -O0).
- **The lookup win for flat_map requires -O2** (debug-build
  binary-search bounds-check elision is missed).
- **The per-element insert WIN for std::map is robust** at every
  optimisation level (tree rotation < vector shift).

### The structural lesson

The right container depends on the access pattern, not the
container name:

| Workload | Use |
|----------|-----|
| Bulk-load + lookup + iterate | `std::flat_map` |
| Incremental update + lookup | `std::map` |
| Lookup + iterate + small bulk | `std::flat_map` |
| Frequent erase + lookup | `std::map` |
| Small map, lookup-heavy | either — pick by heart |
| Very small map (< 32 elements) | either — pick by heart |

The lesson's two assertions in this section are the patterns that
hold at both -O0 and -O2:

```
PASS: std::map per-element insert is FASTER than flat_map
       (the trade-off: flat_map is bad at incremental updates)

PASS: flat_map iteration is faster than std::map
       (vector walk)
```

The bulk-insert and lookup comparisons are reported as
observation only (not asserted) because they're -O2 wins for
flat_map; at -O0 the picture is mixed.

## Section 12 — ASan/UBSan stress run

Standard 100x ASan/UBSan stress pattern from this learning arc.
Same code in a tight loop — any heap corruption, double-free,
out-of-bounds, or use-after-free would surface in the ASan/UBSan
output.

```cpp
constexpr int BIG = 100'000;
constexpr int PROBE = 50'000;

for (int rep = 0; rep < 100; ++rep) {
    std::flat_map<int, int> stress;
    std::vector<std::pair<int, int>> in;
    in.reserve(static_cast<std::size_t>(BIG));
    std::mt19937 r(static_cast<std::uint32_t>(rep * 131 + 7));
    for (int i = 0; i < BIG; ++i) {
        in.emplace_back(static_cast<int>(r()),
                        static_cast<int>(r()));
    }

    // bulk
    stress.insert(in.begin(), in.end());

    // iterate
    long isum = 0;
    for (auto const& kv : stress) isum += kv.second;

    // 50k find() calls
    long fsum = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(PROBE); ++i) {
        auto stress_it = stress.find(in[i].first);
        if (stress_it != stress.end()) fsum += stress_it->second;
    }

    // erase the first half (triggers vector-shift on the tail)
    std::vector<int> keys_to_erase;
    keys_to_erase.reserve(static_cast<std::size_t>(BIG / 2));
    for (int i = 0; i < BIG / 2; ++i) {
        keys_to_erase.push_back(in[static_cast<std::size_t>(i)].first);
    }
    for (int k : keys_to_erase) stress.erase(k);
}
```

Result on ASan/UBSan build: **clean** (zero leaks, zero UB, zero
double-free on the two underlying vectors, zero use-after-free
on the iterator / sortable range across all 100 iterations).

## Observed output

The full observed run on the default build (truncated):

```
== Section 1: Toolchain + feature-test probe + sizeof ==
  __cpp_lib_flat_map = 202207
  sizeof(std::flat_map<int,int>)     = 48
  sizeof(std::flat_set<int>)        = 24
  sizeof(std::flat_multimap<int,int>) = 48
  sizeof(std::flat_multiset<int>)    = 24
  sizeof(std::map<int,int>)         = 24
  sizeof(std::set<int>)             = 24
  PASS: sizeof(std::flat_map<int,int>) == 48 B
  PASS: sizeof(std::flat_set<int>) == 24 B
  ...

== Section 5: Bulk insert — the O(n) sort+dedupe+merge path ==
  PASS: 100-element bulk insert fills the map
  PASS: bulk insert with duplicates leaves size == 2 (deduped)
  PASS: sorted_unique + pre-sorted input merges correctly
  PASS: GOTCHA: sorted_unique + UNSORTED input preserves input order
       (libc++ does not enforce the precondition)

== Section 7: Custom comparator — case-insensitive lookup ==
  PASS: iteration is sorted case-insensitively: BAR < baz < Foo
  PASS: find("FOO") finds the "Foo" entry case-insensitively
  PASS: find(sv("bar")) finds the "BAR" entry
  ...

== Section 11: Empirics — flat_map vs std::map ==
  bulk insert (50000 elements):
    std::flat_map : 6188 us
    std::map      : 13057 us
  per-element insert (50000 elements):
    std::flat_map : 87515 us (the bad case)
    std::map      : 4906 us
  lookup (10000 finds):
    std::flat_map : 226 us
    std::map      : 428 us
  iterate (50000 elements):
    std::flat_map : 2 us (cache-friendly)
    std::map      : 389 us (random tree walk)
  PASS: std::map per-element insert is FASTER than flat_map
  PASS: flat_map iteration is faster than std::map

== Section 12: ASan/UBSan stress run ==
  PASS: 100x stress run of 100k bulk + 50k find + bulk erase completed

== Summary ==
  sections : 12
  PASS     : 81
  FAIL     : 0
All sections OK; exiting.
```

## Design decisions and trade-offs

### Why this topic

The Aug 13–17 arc closed the JSON Schema / Patch / validated
read+write library chain and the streaming-parser generator
adapter. The Aug 18 / 19 lessons were GitHub-tooling (sha-pin,
dependabot). The C++23 stdlib-tour topic that fits the chain
*cleanly* today is `std::flat_map` — it sits in the same family
as `std::span` / `std::mdspan` / `std::expected` (C++23
container-style additions) and gives a focused study in
cache-friendly data structures with transparent-comparator /
heterogeneous-lookup semantics.

### What was verified

The lesson verifies 81 specific assertions across 12 sections.
The patterns that proved particularly worth surfacing in the
notes:

1. **`sorted_unique` is a precondition, not a magic sort tag.**
   The standard says UB; libc++ gives you a silently-wrong
   result. Real-world gotcha — always use the plain `insert()`
   unless you can prove the input is already sorted.

2. **Don't call `.begin()` / `.end()` on different temporaries.**
   This was an actual bug found during development of this
   lesson — the failure was an interleaved output that
   initially looked like a flat_set bug but was actually a
   bug in the test code itself. The lesson notes carry a
   corrected version.

3. **The bulk-insert performance win is an `-O2` story.**
   Debug builds (`-O0`) don't optimize the `std::sort` call
   inside flat_map's bulk-insert path, so std::map can win
   there. The lesson is honest about this and reports both
   numbers.

4. **The iteration win is universal.**
   Even at `-O0`, vector iteration beats tree iteration by
   2-3x. This is the easiest case to predict and the easiest
   case to verify empirically.

5. **The per-element insert loss is universal.**
   Even at `-O2`, per-element insert into a flat_map is 18x
   slower than per-element insert into std::map. This is
   the canonical "use the right tool" lesson.

6. **`extract()` is `&&`-qualified.**
   The function returns the two underlying vectors and
   leaves `*this` empty. This is a useful detail — the
   standard explicitly says "you must own this flat_map to
   extract from it" so that the move semantics are
   observable.

### What was NOT covered

- **Allocator-aware containers** — `std::flat_map`'s
  `Allocator` template parameter is for the underlying
  vectors, not for individual elements. The lesson uses
  the default `std::allocator` throughout.
- **Custom `KeyContainer` / `MappedContainer`** —
  `std::flat_map` lets you supply a custom underlying
  container (e.g. `std::deque` or a small-buffer-optimised
  vector). The lesson uses the default `std::vector`.
- **`std::flat_map` with non-default access** — the
  heterogeneous lookup is shown but the `lower_bound` /
  `upper_bound` / `equal_range` overloads that take a
  transparent-comparator argument are not exhaustively
  tested.
- **Comparison vs `boost::container::flat_map`** — the
  Boost version pre-dates the standard version by ~10
  years; the standard version is API-compatible with the
  Boost version modulo the iterator-type name (Boost uses
  `random_access_iterator`; the standard uses
  `std::random_access_iterator_tag`).
- **`std::flat_map` with `std::pmr` polymorphic
  allocators** — works through the `Allocator` template
  parameter but is not exercised here.

## Where we go next

Today's lesson completes a focused study of the C++23
`std::flat_map` family. There are no new forward-on list items
from this lesson — the cross-cutting forward-on items from
the Aug 13 / Aug 15 / Aug 17 lessons remain:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg / Conan port** for `psp_span_lib`.
- **Branch protection requiring the matrix to pass**.
- **`std::generator` adapter** (closed by the Aug 17
  coroutine_generator lesson).
- **`v0.16.0` promotion** — the mechanical lift of
  `validate_atomic` + `parse_and_apply_atomic_streaming_
  validated` + `resolve_with_validation` + `parse_patch_ops`
  into `<psp_span/json_schema.h>` and
  `<psp_span/json_pointer.h>` / `<psp_span/json_ext.h>`.

These remain forward-on list items from earlier in the arc.

### Natural follow-on lessons for the C++23 stdlib tour

If a future lesson wants to continue the C++23 stdlib-tour
arc:

- **`std::flat_map` with a custom `KeyContainer`** — supply
  a `std::deque` or `boost::container::small_vector` as the
  underlying storage; the API surface is unchanged but the
  reallocation / invalidation story changes.
- **`std::function_ref` (C++26 — not yet in libc++)** —
  a non-owning view of a callable; once `<functional>` ships
  it, this would be the next "view-of-something" lesson
  (parallel to the `std::span` / `std::mdspan` arc).
- **`std::expected<T, E>` with monadic operations** —
  the `.and_then` / `.or_else` / `.transform` methods that
  compose errors cleanly. This was a forward-on item from
  the Aug 8 lesson.
- **`std::print` (C++23)** — the standard-formatting I/O
  that supersedes `std::cout <<` chains. Trivial but worth
  a focused exercise.
