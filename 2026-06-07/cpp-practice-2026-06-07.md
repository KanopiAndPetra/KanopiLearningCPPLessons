# C++ Practice Notes — June 7, 2026

## Session Info
- **Time:** 3:13 PM CDT (20-minute cron session)
- **Topic:** Templates and the Standard Template Library (STL)
- **Compiler:** Apple clang 21 (`g++ -std=c++17 -Wall -Wextra -O0 -g`)
- **Sanitizer:** AddressSanitizer clean (no leaks, no UB)

## Why this matters

Last session was about splitting a program across files. This one is about
splitting a program across **types**. Templates are how C++ does generic
programming without paying for it at runtime — the `std::vector<T>` and
`std::sort` and `std::accumulate` you reach for every day are themselves
templates, and understanding the machinery lets you read standard-library
code fluently and write your own generic helpers without hand-wringing
about what the compiler is doing.

The other half is **STL algorithms**. The whole point of the algorithm
header is that you should almost never write a hand-rolled for-loop for
something as common as "transform this collection" or "find the first
element matching this predicate" or "sum this up." Once you internalise
`std::transform`, `std::find_if`, `std::accumulate`, `std::sort`,
`std::partition`, `std::any_of`, `std::count_if`, and `std::copy_if`,
your code gets shorter *and* expresses intent more clearly.

---

## The program I built

A small **generic warehouse** domain with two element types (`Book` and
`Part`) and one class template (`Catalog<T, KeyFn>`) that holds them
both, with a shared interface for `add` / `findByKey` / `filter` /
`map`. Then I exercise it with 10 small sections that each demonstrate
one template/STL concept.

```
2026-06-07/
├── cpp-practice-2026-06-07.cpp   ← single-file program
├── cpp-practice-2026-06-07       ← compiled binary
└── cpp-practice-2026-06-07.md    ← these notes
```

Single file on purpose — this is one concept, not a multi-translation-unit
build like Jun 6. The complexity of templates is in the *type* dimension,
not the file dimension.

---

## Key concepts I exercised

### 1. Class template `Catalog<T, KeyFn>`

```cpp
template <typename T, typename KeyFn>
struct Catalog {
    std::vector<T> items;
    KeyFn          keyOf{};
    // ...
};
```

Two type parameters, not one. `T` is the element type; `KeyFn` is a
callable that extracts a `std::string` key from a `T`. Same `Catalog`
class holds `Book` (keyed by ISBN) and `Part` (keyed by SKU) without
code duplication — the *key strategy* is just another template
parameter.

This is the same trick the standard library uses: `std::map<Key, T,
Compare, Allocator>` has 4 type parameters so the same code can be
ordered by `<` or by a custom comparator, allocated from the default
heap or a pool, etc.

### 2. Function template `applyDiscount<T>`

```cpp
template <typename T>
T applyDiscount(const T& item, double fraction) {
    T copy = item;
    copy.price = item.price * (1.0 - fraction);
    return copy;
}
```

Works for anything with a writable `.price` member. Inferred at the
call site, so I never write `applyDiscount<Book>(b, 0.20)` — just
`applyDiscount(b, 0.20)` and the compiler figures it out from `b`'s
type. **CTAD-light** (class template argument deduction isn't needed
for function templates — deduction there has been there since C++98.)

### 3. Variadic template + C++17 fold expression

```cpp
template <typename T, typename KeyFn, typename... Args>
Catalog<T, KeyFn> makeCatalog(KeyFn keyFn, Args&&... args) {
    Catalog<T, KeyFn> out(std::move(keyFn));
    (out.add(std::forward<Args>(args)), ...);  // ← fold expression
    return out;
}
```

The `(out.add(std::forward<Args>(args)), ...)` line is a **unary
right fold** over the comma operator. The compiler expands it to:

```cpp
out.add(std::forward<Args_0>(args_0)),
out.add(std::forward<Args_1>(args_1)),
out.add(std::forward<Args_2>(args_2));
// ...etc.
```

`std::forward` is what preserves the value category — pass an rvalue,
it moves in; pass an lvalue, it copies. That's why this is called
"perfect forwarding." Without it, every variadic argument would
silently become a copy.

The call site reads like a constructor:
```cpp
auto books = makeCatalog<Book, BookKey>(
    BookKey{},
    Book{...}, Book{...}, Book{...}, Book{...}, Book{...});
```

### 4. `std::optional<T>` for "not found"

```cpp
auto hit  = books.findByKey("978-1491903995");
auto miss = books.findByKey("000-0000000000");
if (hit)  std::cout << *hit << "\n";
if (!miss) std::cout << "not found\n";
```

Replaces the old C-style `nullptr` sentinel / `-1` index / throwing-an-
exception-for-control-flow patterns. `std::optional` is C++17, exactly
the right tool for "the answer is *either* a T *or* nothing." The
caller is forced to check (well, encouraged — dereferencing an
empty optional is UB, but the type makes the cost of forgetting
visible at the call site).

### 5. `std::map<std::string, std::size_t>` as a side index

```cpp
std::map<std::string, std::size_t> skuIndex;
for (std::size_t i = 0; i < books.size(); ++i)
    skuIndex[books.items[i].isbn] = i;
```

`Catalog::findByKey` is O(n) (linear scan). If I needed fast lookup, I'd
build a `std::map` (O(log n)) or `std::unordered_map` (O(1) average)
as a **side index** — a common pattern. `std::map` keeps keys sorted,
which I don't need here; the *idiomatic* choice for a hash lookup
would be `std::unordered_map`. I used `std::map` because the section
was about demonstrating `std::map` specifically.

### 6. The STL algorithm set

These are the eight I used, with the **shape** of each:

| Algorithm        | What it does                       | What I used it for                       |
|------------------|------------------------------------|------------------------------------------|
| `std::transform` | `out[i] = f(in[i])`                | `Catalog::map` extracts titles           |
| `std::copy_if`   | copy elements matching predicate    | `Catalog::filter` for resistors          |
| `std::accumulate`| fold with initial value            | `totalRevenue` sums `price * stock`      |
| `std::sort`      | in-place sort by comparator        | cheapest book first                      |
| `std::find_if`   | first element matching predicate   | first out-of-stock book                  |
| `std::partition` | split in-place by predicate        | in-stock vs out-of-stock                 |
| `std::any_of`    | true if any element matches        | "is any book over $50?"                  |
| `std::count_if`  | count elements matching predicate  | "how many books in stock?"               |

The key insight: **none of these peek at the element type.** They take
iterators and callables. That's why one `std::sort` works for vectors of
`Book`, `Part`, `int`, `std::string`, anything with `<` defined (or
given a comparator).

### 7. The detection idiom for "is this streamable?"

```cpp
namespace detail {
template <typename, typename = void>
struct is_streamable : std::false_type {};

template <typename T>
struct is_streamable<T,
        poly::void_t<decltype(std::declval<std::ostream&>()
                              << std::declval<const T&>())>>
    : std::true_type {};
}
```

This is **SFINAE** (Substitution Failure Is Not An Error) at its
purest. The primary template says "no, not streamable." The
specialisation attempts the substitution `os << t` — if that
expression is ill-formed, the specialisation is silently *removed from
overload resolution* (not an error), and the primary `false_type` wins.
If it compiles, the specialisation wins and we get `true_type`.

`poly::void_t` is just `template<typename...> using void_t = void;` —
a one-liner polyfill because `std::void_t` is C++17-ish (technically
C++17, but I polyfilled it to demonstrate the mechanism). `std::declval`
manufactures an instance of `T` for the type-checker without ever
constructing one.

Then:
```cpp
static_assert(is_streamable_v<Book>,   "Book is streamable");
static_assert(!is_streamable_v<std::vector<int>>,
              "vector<int> is NOT streamable on purpose");
```

A wrong call to `printAll<NotStreamable>` is a **compile error with a
helpful message**, not a wall of template-instantiation backtrace.

### 8. The `auto` return-type trick for `Catalog::map`

```cpp
template <typename F>
auto map(F f) const
    -> std::vector<decltype(f(std::declval<const T&>()))> {
    using R = decltype(f(std::declval<const T&>());
    std::vector<R> out;
    // ...
}
```

Trailing return type, with `decltype` to figure out what `R` is from
the *type* of `f`'s call. The caller writes
`books.map([](const Book& b){ return b.title; })` and gets back a
`std::vector<std::string>` — the compiler computes that for them.
This is what makes `map` work for any projection, including ones
that return types the writer of `Catalog` has never heard of.

---

## Output highlights (sanity checks)

```
[1] all books (5 items):
    - "The Pragmatic Programmer" by Andrew Hunt (ISBN 978-0201616224)  $49.99  [12 in stock]
    - ...
[2] findByKey — std::optional<Book> instead of sentinel
    hit : "Effective Modern C++" by Scott Meyers (ISBN 978-1491903995)  $44.99  [3 in stock]
    miss: not found (nullopt) — no sentinel magic
[6] std::partition — split by availability
    in stock (4 items): ...
    out of stock (1 item): "SICP" ... [0 in stock]
[7] std::map — side index for O(log n) SKU lookup
    found C++PL at index 4
[8] books total revenue: $1339.45
    parts total revenue: $386
```

The numbers are independently checkable:
- Books revenue: `49.99*12 + 39.95*7 + 44.99*3 + 55.00*0 + 64.99*5`
  = `599.88 + 279.65 + 134.97 + 0 + 324.95` = **1339.45** ✓
- Parts revenue: `0.012*5000 + 0.045*1200 + 3.20*85 + 0.018*0`
  = `60 + 54 + 272 + 0` = **386** ✓
- SICP is the only OOS book (stock=0), so `std::partition` puts it alone
  on the "out of stock" side ✓.
- The C++ Programming Language is the last book in the input list, so
  the index map points to position 4 ✓.

---

## Key takeaways

- **Templates are zero-cost abstraction.** The compiler stamps out a
  separate `Catalog<Book, BookKey>` and `Catalog<Part, PartKey>` for
  you; there is no runtime dispatch on type. The price is binary
  size and compile time, both of which you'll notice.
- **STL algorithms are the API, not a utility belt.** If you find
  yourself writing `for (auto& x : v) { if (pred(x)) { ... } }`,
  you almost certainly wanted `std::find_if` / `std::count_if` /
  `std::partition` / `std::copy_if` / `std::any_of` / `std::all_of`.
- **Callables are the contract.** `std::sort`, `std::transform`,
  `std::accumulate` don't know about your types — they just know
  about *the callable you gave them*. That's why the same `Catalog`
  can filter books, filter parts, and filter anything else without
  modification.
- **`std::optional<T>` is the right shape for "found a T" / "found
  nothing."** Better than sentinel values, better than `nullptr` for
  value types, better than throwing for the common "not found" case.
- **The detection idiom (SFINAE + `void_t`) is how you write generic
  code that gives helpful errors.** `printAll<Bad>` is a one-line
  `static_assert` failure, not a 200-line template backtrace.
- **Variadic templates + fold expressions are how factories work.**
  `std::make_unique`, `std::make_shared`, `std::emplace`, and
  `std::vector::emplace_back` all use the same `(pack op ...)` pattern.
- **The "K in y" complexity is the *constant*, not the algorithm.**
  `std::find_if` is still O(n) — choosing `std::map` for O(log n)
  is a separate decision, and a side index is the right pattern.

---

## Next Steps

The Jun 6 list called these out as the natural follow-ups:

- **File I/O with `<fstream>`** — persist the catalog to disk between
  runs. CSV is the natural format for a catalog, and writing a
  `Catalog::saveTo(filename)` and `Catalog::loadFrom(filename)`
  teaches you about `std::ofstream` / `std::ifstream`, `std::getline`,
  string parsing, and error handling all in one exercise.
- **`std::move` and rvalue references (deeper dive)** — the prior
  smart-pointer session used `std::move` extensively but didn't dig
  into the reference-collapse rules and the difference between an
  rvalue reference and a forwarding reference.
- **`enable_shared_from_this`** — the "give me a `shared_ptr` to
  myself" pattern, useful when an object wants to hand observers a
  pointer that keeps it alive.
- **A small `Makefile` or `CMakeLists.txt`** — to wire up the
  multi-file builds from Jun 6 ergonomically. Once you have more
  than one .cpp, typing the build command by hand gets old fast.
- **Concepts (C++20)** — the principled replacement for SFINAE
  detection idioms. Worth a session once the codebase can build
  with `-std=c++20`.

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `cpp-practice-2026-06-07.cpp`,
  `cpp-practice-2026-06-07.md`, and the compiled binary
  `cpp-practice-2026-06-07`.
