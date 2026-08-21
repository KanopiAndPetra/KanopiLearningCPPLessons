# P-2026-08-21 — std::flat_map / std::flat_set with a CUSTOM KeyContainer (C++23)

Modern-C++ lesson for today. Closes the **first** item on the
Aug 20 lesson's "Natural follow-on lessons for the C++23 stdlib
tour" list, verbatim:

> **`std::flat_map` with a custom `KeyContainer`** — supply a
> `std::deque` or `boost::container::small_vector` as the
> underlying storage; the API surface is unchanged but the
> reallocation / invalidation story changes.

Today **is** that lesson.

```
Jul  9   std::span     (C++20) — 1-D non-owning view
Jul 10   std::mdspan   (C++23) — N-D non-owning view
Jul 12   std::expected (C++23) — sum-type error channel
Aug 20   std::flat_map family (C++23) — DEFAULT std::vector storage
today    std::flat_map family (C++23) — CUSTOM storage:
         std::deque and a hand-rolled fixed-capacity static_vector
```

Aug 20 **asserted** that the reallocation and invalidation stories
change. Today **measures** both, with numbers rather than adjectives.

## Headline

| Build | Result |
|-------|--------|
| Default (`clang++ -std=c++23 -stdlib=libc++ -O0 -g`) | **88/88 PASS**, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | **88/88 PASS**, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer (`-O1 -g -fsanitize=address -fsanitize=undefined`) | **88/88 PASS**, clean sanitizer output |
| CMake (`cmake -S . -B build && cmake --build build`) | **88/88 PASS** |

100 consecutive ASan/UBSan runs: all 100 exit 0 with 88/88 PASS and
zero sanitizer diagnostics. Cross-build output is byte-identical
between the default and strict builds; the ASan build's 88 verdict
lines are identical too. Section 5's allocation counts are stable
across 10 independent runs.

| Section | Topic | Tests |
|---------|-------|-------|
| 1 | Toolchain + feature probes + the `sizeof` story | 6 |
| 2 | The `KeyContainer` / `MappedContainer` contract (compile-time) | 11 |
| 3 | API parity across `vector` / `deque` / `static_vector` | 17 |
| 4 | Bulk insert + `sorted_unique` on custom storage | 11 |
| 5 | **Allocation empirics — the reallocation story, counted** | 5 |
| 6 | **Reference invalidation, measured by address** | 11 |
| 7 | **Capacity exhaustion and the `clear()`-on-throw trap** | 8 |
| 8 | `extract()` / `replace()` + cross-storage migration | 9 |
| 9 | `flat_set` / `flat_multimap` on custom storage | 9 |
| 10 | ASan/UBSan stress (100 rounds, all three back-ends) | 1 |
| **Total** | | **88** |

## The three back-ends under test

```cpp
using VecMap = std::flat_map<int, std::string>;   // default: std::vector x2

using DeqMap = std::flat_map<int, std::string, std::less<>,
                             std::deque<int>, std::deque<std::string>>;

using SvMap  = std::flat_map<int, std::string, std::less<>,
                             static_vector<int, 32>,
                             static_vector<std::string, 32>>;
```

`std::flat_map`'s full template head is:

```cpp
template <class Key, class T,
          class Compare         = std::less<Key>,
          class KeyContainer    = std::vector<Key>,
          class MappedContainer = std::vector<T>>
class flat_map;
```

The last two parameters are the whole subject of this lesson. Note
they are **independent** — you can pair a `std::deque` key container
with a `std::vector` mapped container if the access patterns differ
between the two halves.

## What the KeyContainer contract actually requires

`[flat.map.overview]/8-10` states the requirements: `KeyContainer`
must meet the sequence-container requirements, its iterators must
model `random_access_iterator`, and it must not be
`std::vector<bool>`. In practice libc++'s `flat_map` touches exactly
this surface, and nothing more:

```
value_type / size_type / difference_type / iterator / const_iterator
default ctor, iterator-pair ctor
begin / end / cbegin / cend
size / empty / max_size / clear
insert(const_iterator, T)          (single, copy and move)
insert(const_iterator, It, It)     (range)
emplace(const_iterator, Args...)
erase(const_iterator)
erase(const_iterator, const_iterator)
swap
```

That is the entire contract. There is nothing about `capacity()`,
`reserve()`, `push_front`, or allocators. Section 2 confirms the
hand-rolled `static_vector<T, N>` in this TU implements exactly this
list and is accepted.

The random-access requirement is **hard**, not a quality-of-
implementation preference — the binary search that gives `flat_map`
its `O(log n)` lookup depends on it. `std::list` and
`std::forward_list` are therefore not legal `KeyContainer`s.

### The type aliases follow the storage choice

```cpp
static_assert(std::is_same_v<DeqMap::key_container_type,    std::deque<int>>);
static_assert(std::is_same_v<DeqMap::mapped_container_type, std::deque<std::string>>);
```

but `value_type` does **not**:

```cpp
static_assert(std::is_same_v<VecMap::value_type, std::pair<int, std::string>>);
static_assert(std::is_same_v<DeqMap::value_type, std::pair<int, std::string>>);
static_assert(std::is_same_v<SvMap::value_type,  std::pair<int, std::string>>);
```

`value_type` is a synthesised reference type — the pair is never
actually stored anywhere. That is the point of the "flat" design:
keys live contiguously in one container and mapped values in a
parallel one, so a key-only scan touches no mapped-value cache lines.

## Section 1 — the `sizeof` story

```
sizeof(VecMap)  [vector x2]        = 48 B
sizeof(DeqMap)  [deque  x2]        = 96 B
sizeof(SvMap)   [static_vector x2] = 912 B
sizeof(TinyMap) [static_vector<3>] = 48 B
```

Three different cost models in four numbers:

- **48 B** — two `std::vector`s (3 pointers each) plus the empty
  comparator, which is elided by EBO.
- **96 B** — libc++'s `std::deque` carries a block-map vector plus a
  size and a start offset, so it is exactly twice the vector control
  block.
- **912 B** — `static_vector<int, 32>` inlines its whole payload.
  Verified arithmetic: `sizeof(array<int,32>) == 128` plus an 8-byte
  count is 136; `sizeof(array<string,32>) == 768` (`sizeof(string)
  == 24`) plus 8 is 776; `136 + 776 == 912`. The object is huge
  because it owns everything.
- **48 B** — the same `static_vector` design at capacity 3. Capacity
  is a template parameter, so the size is a dial you control.

The `static_vector` number is the fixed-capacity trade in a single
figure: you pay a large, *statically known* object size, and in
exchange you never touch the heap.

## Section 3 — the "API surface is unchanged" half, proven

The proof is structural rather than a list of assertions. One
generic function template is instantiated three times:

```cpp
template <class Map>
std::string exercise(const char* label) {
    Map m;
    m.emplace(30, "thirty");
    m.emplace(10, "ten");
    m.emplace(20, "twenty");
    m[40] = "forty";
    m.insert_or_assign(10, "TEN");
    m.try_emplace(20, "ignored");   // no-op: key present
    const auto it = m.find(20);
    m.erase(30);
    return render(m);
}
```

If the API genuinely does not change with storage, this template
compiles for every back-end and returns the same string. It does:

```
vector x2      -> 10=TEN,20=twenty,40=forty (size=3, find(20)=ok)
deque x2       -> 10=TEN,20=twenty,40=forty (size=3, find(20)=ok)
static_vector  -> 10=TEN,20=twenty,40=forty (size=3, find(20)=ok)
```

The sorted-iteration invariant, `keys()` / `values()` access,
iterator arithmetic (`begin() + 3`), and `end() - begin() == size()`
all hold identically on all three.

## Section 5 — the reallocation story, COUNTED

Rather than reasoning about growth policies, this section counts
every heap allocation through a replaced **global** `operator new`:

```cpp
void* operator new(std::size_t n) {
    if (g_counting) { ++g_alloc_count; g_alloc_bytes += n; }
    void* p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) throw std::bad_alloc{};
    return p;
}
```

Global replacement rather than a custom `Allocator` template
argument, deliberately: an allocator would only observe the two
underlying containers' own allocations, whereas the global hook
observes every allocation the program makes. Only the non-aligned
overloads are replaced, so aligned news still pair with aligned
deletes and there is no mismatched-deallocation report under ASan.

50 ascending `int -> int` inserts (the cheapest possible pattern for
a sorted-vector container — every insert is a tail append):

```
50 ascending inserts, heap allocations:
  std::vector x2   : 14
  std::deque  x2   : 4
  static_vector x2 : 0
```

- **`std::vector`: 14.** Geometric growth gives `O(log n)`
  allocations, but every one of them **copies the entire payload**
  to fresh memory. Two containers growing independently, doubling
  from 1 up to 50 elements, is 7 allocations each.
- **`std::deque`: 4.** libc++'s deque allocates fixed-size blocks
  plus a block map, and **never relocates the elements already
  stored**. Measured directly: a single `std::deque<int>` taking 50
  `push_back`s costs exactly 2 allocations (one block map, one
  element block), and `flat_map` holds two such deques.
- **`static_vector`: 0.** Exactly zero. The payload is inline; there
  is no heap involvement at any point in the container's life.

`int -> int` and not `int -> std::string` on purpose: `std::string`
would add an allocation per value over the SSO threshold and drown
the container's own growth signal in noise. This measurement is
about the *container*.

These counts are identical on the default, strict, and ASan builds —
the counting hook sits above the sanitizer's `malloc` interception —
and stable across 10 independent runs.

## Section 6 — the invalidation story, MEASURED

Method note: addresses are captured as `std::uintptr_t` and a stale
pointer is **never** dereferenced. Comparing integers sidesteps the
pointer-provenance question entirely and produces no ASan
use-after-free report. The observation is purely *did the object
move?*

```
vector-backed: &at(0) MOVED     across 200 tail inserts
deque-backed : &at(0) UNCHANGED across 200 tail inserts
```

This is the concrete content of Aug 20's claim. With `std::vector`
storage, holding a `std::string&` into the map across an insert is a
dangling reference. With `std::deque` storage, the same reference
survives — because deque never relocates existing elements when it
grows at the ends.

### The caveat that stops this from being a general guarantee

`std::deque` protects references against insertion **at the ends**.
A `flat_map` insert lands wherever sorted order demands, and a
middle insert shifts every subsequent element down by one:

```
deque-backed : &at(70) MOVED across a MIDDLE insert
```

`d2.at(70)` still reads `"v7"` — the *mapping* is correct — but the
value now lives in the slot its neighbour used to occupy. A
reference captured beforehand points at the wrong element, not at
freed memory.

**Choosing `std::deque` storage buys tail-insert reference
stability, not immunity from invalidation.** For an append-mostly
key sequence (monotonic IDs, timestamps) that is a real and useful
guarantee. For scattered keys it buys almost nothing, and you have
paid a fatter control block and worse iteration locality for it.

## Section 7 — THE FINDING: no strong exception guarantee

This is the one thing Aug 20 could not have surfaced, because it
only exists once the underlying container is able to *refuse to
grow*.

```cpp
TinyMap m;              // static_vector<int, 3> x2
m.emplace(1, 10);
m.emplace(2, 20);
m.emplace(3, 30);       // now at capacity

try {
    m.emplace(4, 40);   // static_vector throws std::length_error
} catch (const std::length_error& e) {
    // what is m's state here?
}
```

The intuitive answer — the strong guarantee, map unchanged — is
wrong:

```
caught: static_vector: capacity exhausted
size after the failed insert: 0 (was 3)
```

**The map comes back EMPTY.** All three pre-existing entries are
gone. This is conforming, not a libc++ bug:
`[flat.map.modifiers]` specifies that if an operation throws, the
`flat_map` is left empty. The rationale is that `flat_map` must keep
two separate containers exactly in sync; if one throws partway
through a two-container update, restoring a consistent state without
introducing its own failure modes is not generally possible, so the
standard chose the basic guarantee plus a documented `clear()`.

The map is *emptied*, not poisoned — it remains a valid object and
can be reused immediately (verified).

The consequence for fixed-capacity storage is sharp: **an insert
that overflows a `static_vector`-backed `flat_map` destroys all your
data, not just the new entry.** The mitigation is a pre-flight
check. `flat_map` does not expose `capacity()`, but `keys()` hands
you the underlying container and the container knows:

```cpp
if (!(m.keys().full() && !m.contains(key))) {
    m.emplace(key, value);   // safe: either room exists or key is present
}
```

Section 10 runs this throw-and-unwind path 100 times under ASan/UBSan
so the `clear()`-on-throw unwinding gets sanitizer coverage rather
than a single-shot check.

## Section 8 — `extract()` / `replace()` and cross-storage migration

`extract()` is rvalue-qualified and returns an aggregate whose
member types follow the storage choice:

```cpp
auto parts = std::move(d).extract();
static_assert(std::is_same_v<decltype(parts.keys),   std::deque<int>>);
static_assert(std::is_same_v<decltype(parts.values), std::deque<std::string>>);
```

`replace()` adopts containers wholesale in `O(1)` with no re-sort —
the caller promises sorted and unique, and that promise is unchecked.

Migrating *between* storage types is where the cost shows up. The
container types differ, so there is no `O(1)` adoption path and you
must copy element by element:

```cpp
auto parts = std::move(deque_map).extract();
static_vector<int, 32>         sv_keys(parts.keys.begin(),   parts.keys.end());
static_vector<std::string, 32> sv_vals(parts.values.begin(), parts.values.end());
sv_map.replace(std::move(sv_keys), std::move(sv_vals));
```

This is the real cost of naming a storage back-end in a public API
signature: `flat_map<K, V, C, std::deque<K>, std::deque<V>>` and
`flat_map<K, V>` are unrelated types, and callers holding one cannot
hand it to a function expecting the other without an `O(n)` rebuild.
Storage choice is not an implementation detail once it appears in a
header — prefer keeping it behind a type alias.

## Section 9 — `flat_set` / `flat_multimap` and orthogonality

`flat_set` takes a single container parameter (there is no mapped
side). Custom storage composes cleanly with a custom comparator:

```cpp
using SvSet = std::flat_set<std::string, CiLess, static_vector<std::string, 16>>;
```

Heterogeneous lookup through the transparent comparator still costs
nothing, on custom storage as much as on the default:

```
heterogeneous contains() cost 0 allocations
```

`ss.contains(std::string_view{"chArLie"})` constructs no temporary
`std::string`. Storage choice and comparator choice are fully
orthogonal — neither interferes with the other.

## Observed output

```
=== Section 1 — toolchain, feature probes, sizeof ===
  __cplusplus                 = 202302
  __cpp_lib_flat_map          = 202207
  __cpp_lib_flat_set          = 202207
  sizeof(VecMap)  [vector x2]        = 48 B
  sizeof(DeqMap)  [deque  x2]        = 96 B
  sizeof(SvMap)   [static_vector x2] = 912 B
  sizeof(TinyMap) [static_vector<3>] = 48 B

=== Section 3 — API parity across vector / deque / static_vector ===
  vector x2      -> 10=TEN,20=twenty,40=forty (size=3, find(20)=ok)
  deque x2       -> 10=TEN,20=twenty,40=forty (size=3, find(20)=ok)
  static_vector  -> 10=TEN,20=twenty,40=forty (size=3, find(20)=ok)

=== Section 5 — allocation empirics: the reallocation story ===
  50 ascending inserts, heap allocations:
    std::vector x2   : 14
    std::deque  x2   : 4
    static_vector x2 : 0

=== Section 6 — reference invalidation: vector vs deque ===
  vector-backed: &at(0) MOVED across 200 tail inserts
  deque-backed : &at(0) UNCHANGED across 200 tail inserts
  deque-backed : &at(70) MOVED across a MIDDLE insert

=== Section 7 — capacity exhaustion and the clear()-on-throw trap ===
  caught: static_vector: capacity exhausted
  size after the failed insert: 0 (was 3)

=== Section 9 — flat_set / flat_multimap on custom storage ===
  heterogeneous contains() cost 0 allocations

=== Section 10 — ASan/UBSan stress (100 rounds, all three back-ends) ===
  100 rounds complete; checksum accumulator = 37800

=====================================
TOTAL: 88/88 PASS
=====================================
```

Extra verification (`/tmp/verify_fmcc.sh`, kept out of the repo):

```
=== 1. 100 consecutive ASan/UBSan runs ===
  100/100 runs: exit 0, 88/88 PASS, zero sanitizer diagnostics

=== 2. Cross-build output determinism ===
  default vs strict : IDENTICAL
  default vs asan   : IDENTICAL verdict lines (88 checks)

=== 3. Section 5 allocation-count stability (10 runs) ===
  distinct allocation-count tuples across 10 runs:
    14,4,0
  STABLE (vector,deque,static_vector)

ALL EXTRA VERIFICATION PASSED
```

## Design decisions and trade-offs

### Why this topic

It is the first named follow-on from yesterday's lesson, and it
closes a claim that was asserted but not demonstrated. "The
reallocation / invalidation story changes" is the kind of sentence
that is easy to write and easy to believe without ever checking what
it costs in practice. Counting allocations and comparing addresses
turns it into three numbers and two words.

### Why a hand-rolled `static_vector` rather than Boost

The Aug 20 item named `boost::container::small_vector`. Boost is not
available in this toolchain, and vendoring it for one lesson would
obscure the actual teaching point, which is *what the `KeyContainer`
contract requires*. Writing the container by hand answers that
question directly: the ~90-line class in this TU is the minimum
viable `KeyContainer`, and the fact that it is accepted unchanged is
the proof.

The `static_vector` here stores `std::array<T, N>`, so `T` must be
default-constructible. A production implementation would use aligned
storage plus placement `new` to lift that restriction. The
simplification is deliberate and does not affect any claim made
here — no test depends on the storage representation.

### Which back-end to actually choose

| Storage | Choose when |
|---------|-------------|
| `std::vector` (default) | Almost always. Best iteration locality, fewest indirections, smallest control block. |
| `std::deque` | Large maps built by append, where you must hold references across inserts, or where a single contiguous allocation of the full size is impractical. |
| fixed-capacity | Hard real-time, embedded, or allocation-free paths with a known upper bound — **and** only with the `keys().full()` pre-flight check, because overflow destroys the entire map. |

### What was verified

- The `KeyContainer` contract, as `static_assert`s on the public
  type aliases and `std::random_access_iterator` concept checks.
- API parity, structurally, via one template instantiated three
  times producing identical output.
- Allocation counts, through a replaced global `operator new`,
  stable across 10 runs and identical on all three builds.
- Reference invalidation, by comparing `uintptr_t` values before and
  after insertion — never by dereferencing a stale pointer.
- The `clear()`-on-throw contract, caught and asserted, then
  exercised 100 more times under sanitizers.
- `extract()` / `replace()` on both custom back-ends plus a
  round-trip and a cross-storage migration.
- 100 consecutive ASan/UBSan runs, clean.

### What was NOT covered

- **`std::pmr` polymorphic allocators as the container's allocator.**
  Works through the underlying container's own `Allocator`
  parameter; orthogonal to the `KeyContainer` question studied here.
- **Mixed storage** (`std::deque` keys with `std::vector` mapped
  values). Legal and occasionally useful when the two halves have
  different access patterns; all three configurations here use
  matched containers.
- **A real `small_vector`** with a spill-to-heap path. The
  `static_vector` used here throws instead of spilling, which is
  precisely what makes Section 7's finding observable.
- **Iteration-performance empirics** across back-ends. Section 5
  measures allocations, not cache behaviour; deque's block
  indirection should cost measurable iteration throughput versus
  vector, but that needs a proper `-O2` benchmark harness to say
  anything honest about.
- **`std::vector<bool>` as a `MappedContainer`.** Explicitly
  forbidden for `KeyContainer`; the mapped side is more subtle and
  not explored.

## Where we go next

Today closes the first item on the Aug 20 follow-on list. The
remaining items from that list are unchanged:

- **`std::function_ref` (C++26 — not yet in libc++)** — a non-owning
  view of a callable; the next "view-of-something" lesson once
  `<functional>` ships it.
- **`std::expected<T, E>` with monadic operations** — the
  `.and_then` / `.or_else` / `.transform` composition chain. A
  forward-on item since the Aug 8 lesson.
- **`std::print` (C++23)** — `__cpp_lib_print == 202207` is
  available in this toolchain, so this one is unblocked and ready.

New items surfaced by today's lesson:

- **`flat_map` iteration-throughput empirics at `-O2`** across the
  three back-ends — the honest version of the locality argument this
  lesson deliberately declined to make without measurements.
- **A spill-to-heap `small_vector` `KeyContainer`** — the variant
  that grows instead of throwing, which would sidestep Section 7's
  data-loss trap while keeping the zero-allocation fast path.

The cross-cutting infrastructure items from the Aug 13 / Aug 15 /
Aug 17 lessons remain open:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg / Conan port** for `psp_span_lib`.
- **Branch protection requiring the matrix to pass**.
- **`v0.16.0` promotion** — the mechanical lift of `validate_atomic`
  + `parse_and_apply_atomic_streaming_validated` +
  `resolve_with_validation` + `parse_patch_ops` into
  `<psp_span/json_schema.h>`, `<psp_span/json_pointer.h>`, and
  `<psp_span/json_ext.h>`.
