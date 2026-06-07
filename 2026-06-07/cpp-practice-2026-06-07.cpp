// cpp-practice-2026-06-07.cpp
// -----------------------------------------------------------------------------
// Topic: Templates and the Standard Template Library (STL)
//
// What this program exercises (real, not toy):
//   1.  A class template Catalog<T, KeyFn> — type-safe generic collection
//       with add / find / filter / map.  KeyFn is a function-object type
//       that extracts a string key from T.
//   2.  A function template printAll(Container&) — works on any container
//       of anything that has operator<<, gated by a compile-time trait.
//   3.  A function template applyDiscount(T, double) — generic value
//       transformation.
//   4.  A variadic template function makeCatalog(args...) — perfect-forwarding
//       factory that builds a Catalog<T, KeyFn> from any number of T arguments
//       (C++17 fold expression).
//   5.  STL algorithms: std::transform, std::accumulate, std::sort,
//       std::find_if, std::partition, std::copy_if, std::any_of,
//       std::count_if.
//   6.  std::map<std::string, std::size_t> for indexed SKU lookup.
//   7.  std::optional<T> (C++17) for "not found" instead of sentinels.
//   8.  The detection idiom (std::void_t polyfill) for a small is_streamable
//       trait that gates printAll at compile time.
//
// Why this matters:
//   Templates are the seam where C++ stops being "C with classes" and gives
//   you zero-cost abstraction.  The STL algorithms are the *real* payoff —
//   once you can read std::transform and std::partition, your hand-rolled
//   for-loops start to feel like C.
//
// Build:
//   g++ -std=c++17 -Wall -Wextra -O0 -g -o cpp-practice-2026-06-07 \
//       cpp-practice-2026-06-07.cpp
// -----------------------------------------------------------------------------

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// =============================================================================
//  Polyfills for the few C++20 things this program reaches for, so the file
//  stays strictly -std=c++17.
// =============================================================================

namespace poly {

// std::void_t polyfill (C++17 doesn't have it; trivial to define).
template <typename...>
using void_t = void;

// std::identity polyfill (C++20).  Just returns its argument.
struct Identity {
    template <typename T>
    constexpr auto operator()(T&& t) const noexcept
        -> decltype(std::forward<T>(t)) {
        return std::forward<T>(t);
    }
};

}  // namespace poly

// =============================================================================
//  Domain types — concrete things we'll put in catalogs
// =============================================================================

struct Book {
    std::string title;
    std::string author;
    std::string isbn;        // unique key
    double      price{0.0};
    int         stock{0};

    friend std::ostream& operator<<(std::ostream& os, const Book& b) {
        os << "\"" << b.title << "\" by " << b.author
           << " (ISBN " << b.isbn << ")  $" << b.price
           << "  [" << b.stock << " in stock]";
        return os;
    }
};

struct Part {
    std::string sku;         // unique key
    std::string name;
    std::string category;    // e.g. "resistor", "capacitor", "ic"
    double      unitCost{0.0};
    int         stock{0};

    friend std::ostream& operator<<(std::ostream& os, const Part& p) {
        os << p.sku << "  " << p.name
           << " (" << p.category << ")  $" << p.unitCost
           << "  qty=" << p.stock;
        return os;
    }
};

// Key-extraction function objects
struct BookKey  { std::string operator()(const Book&  b) const { return b.isbn;  } };
struct PartKey  { std::string operator()(const Part&  p) const { return p.sku;   } };

// =============================================================================
//  Detection idiom: is_streamable<T> — true if `os << t` compiles.
//  Implemented with poly::void_t to keep -std=c++17.
// =============================================================================

namespace detail {

template <typename, typename = void>
struct is_streamable : std::false_type {};

template <typename T>
struct is_streamable<T,
        poly::void_t<decltype(std::declval<std::ostream&>()
                              << std::declval<const T&>())>>
    : std::true_type {};

}  // namespace detail

template <typename T>
inline constexpr bool is_streamable_v = detail::is_streamable<T>::value;

// =============================================================================
//  Class template: Catalog<T, KeyFn>
//
//      T     — element type (Book, Part, anything with operator<<).
//      KeyFn — callable (T) -> std::string, used for findByKey.
//
//  The whole point of making KeyFn a template parameter (rather than
//  hard-coding `t.isbn`) is that the SAME Catalog class can hold Books
//  (keyed by ISBN) and Parts (keyed by SKU) without code duplication.
// =============================================================================

template <typename T, typename KeyFn>
struct Catalog {
    std::vector<T> items;
    KeyFn          keyOf{};

    Catalog() = default;
    explicit Catalog(KeyFn fn) : keyOf(std::move(fn)) {}

    void add(const T& item) { items.push_back(item); }
    void add(T&& item)      { items.push_back(std::move(item)); }

    std::size_t size() const { return items.size(); }

    // Linear search by key.  Returns std::optional<T> — caller must check.
    std::optional<T> findByKey(const std::string& key) const {
        auto it = std::find_if(items.begin(), items.end(),
            [&](const T& x) { return keyOf(x) == key; });
        if (it == items.end()) return std::nullopt;
        return *it;            // copy out — caller gets an owned value
    }

    // Filter: returns a new Catalog<T, KeyFn> with items matching the pred.
    template <typename Pred>
    Catalog<T, KeyFn> filter(Pred pred) const {
        Catalog<T, KeyFn> out(keyOf);
        std::copy_if(items.begin(), items.end(),
                     std::back_inserter(out.items), pred);
        return out;
    }

    // Map: std::vector<R> of f(item) for every item.
    template <typename F>
    auto map(F f) const
        -> std::vector<decltype(f(std::declval<const T&>()))> {
        using R = decltype(f(std::declval<const T&>()));
        std::vector<R> out;
        out.reserve(items.size());
        std::transform(items.begin(), items.end(),
                       std::back_inserter(out), f);
        return out;
    }
};

// =============================================================================
//  Function template: applyDiscount — generic value transform.
//  Works for any T with a writable .price member.
// =============================================================================

template <typename T>
T applyDiscount(const T& item, double fraction) {
    T copy = item;
    copy.price = item.price * (1.0 - fraction);
    return copy;
}

// =============================================================================
//  Function template: printAll — gated on is_streamable_v<T>.
//  Trying to printAll(aCatalogOf<NotStreamable>) is a clear static_assert,
//  not a cryptic linker error.
// =============================================================================

template <typename Container>
void printAll(const Container& c, const std::string& label = "") {
    using V = typename Container::value_type;
    static_assert(is_streamable_v<V>,
                  "printAll requires value_type to be streamable to std::ostream");

    if (!label.empty()) {
        std::cout << "  " << label << " (" << c.size() << " item"
                  << (c.size() == 1 ? "" : "s") << "):\n";
    }
    for (const auto& x : c) {
        std::cout << "    - " << x << "\n";
    }
}

// Overload for raw std::vector<T>
template <typename T>
void printAll(const std::vector<T>& v, const std::string& label = "") {
    static_assert(is_streamable_v<T>,
                  "printAll requires T to be streamable to std::ostream");
    if (!label.empty()) {
        std::cout << "  " << label << " (" << v.size() << " item"
                  << (v.size() == 1 ? "" : "s") << "):\n";
    }
    for (const auto& x : v) {
        std::cout << "    - " << x << "\n";
    }
}

// =============================================================================
//  Variadic template: makeCatalog — perfect-forwarding factory.
//  C++17 fold expression expands each arg into out.add(...).
// =============================================================================

template <typename T, typename KeyFn, typename... Args>
Catalog<T, KeyFn> makeCatalog(KeyFn keyFn, Args&&... args) {
    Catalog<T, KeyFn> out(std::move(keyFn));
    (out.add(std::forward<Args>(args)), ...);
    return out;
}

// =============================================================================
//  Free helper: totalRevenue — works on any container; the caller supplies
//  a price-projection callable so Book.price and Part.unitCost are both
//  handled by the same implementation.  This is the standard STL pattern
//  (std::transform, std::accumulate, std::sort all rely on callables).
// =============================================================================

template <typename Container, typename PriceFn>
double totalRevenue(const Container& c, PriceFn priceOf) {
    return std::accumulate(c.begin(), c.end(), 0.0,
        [&](double sum, const auto& item) {
            return sum + priceOf(item) * item.stock;
        });
}

// =============================================================================
//  driver
// =============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << " Templates & STL — generic catalog, algorithms, std::optional\n";
    std::cout << "============================================================\n\n";

    // ---- 1. Build a Catalog<Book> using the variadic factory ---------------
    std::cout << "[1] makeCatalog<Book, BookKey>(...) — variadic perfect-forwarding\n";

    Catalog<Book, BookKey> books = makeCatalog<Book, BookKey>(
        BookKey{},
        Book{"The Pragmatic Programmer",   "Andrew Hunt",       "978-0201616224", 49.99,  12},
        Book{"Clean Code",                 "Robert C. Martin",  "978-0132350884", 39.95,   7},
        Book{"Effective Modern C++",       "Scott Meyers",      "978-1491903995", 44.99,   3},
        Book{"SICP",                       "Harold Abelson",    "978-0262510875", 55.00,   0},
        Book{"The C++ Programming Language","Bjarne Stroustrup","978-0321563842", 64.99,   5}
    );
    printAll(books.items, "all books");
    std::cout << "\n";

    // ---- 2. findByKey — std::optional for "not found" ----------------------
    std::cout << "[2] findByKey — std::optional<Book> instead of sentinel\n";
    auto hit  = books.findByKey("978-1491903995");
    auto miss = books.findByKey("000-0000000000");

    if (hit)  std::cout << "  hit : " << *hit  << "\n";
    if (!miss) std::cout << "  miss: not found (nullopt) — no sentinel magic\n";
    std::cout << "\n";

    // ---- 3. std::transform via Catalog::map --------------------------------
    std::cout << "[3] Catalog::map — std::transform under the hood\n";
    auto titles = books.map([](const Book& b) { return b.title; });
    printAll(titles, "titles extracted (vector<string>)");
    std::cout << "\n";

    // ---- 4. applyDiscount<T> — generic value transform --------------------
    std::cout << "[4] applyDiscount(0.20) — function template, type-generic\n";
    auto discounted = books.map(
        [](const Book& b) { return applyDiscount(b, 0.20); });
    printAll(discounted, "books at 20% off");
    std::cout << "\n";

    // ---- 5. std::sort, std::find_if, std::any_of, std::count_if -----------
    std::cout << "[5] STL algorithms on Catalog::items\n";

    Catalog<Book, BookKey> byPrice = books;             // copy ctor — shallow of items
    std::sort(byPrice.items.begin(), byPrice.items.end(),
              [](const Book& a, const Book& b) { return a.price < b.price; });
    std::cout << "  cheapest first: " << byPrice.items.front() << "\n";
    std::cout << "  priciest last : " << byPrice.items.back()  << "\n";

    auto oos = std::find_if(books.items.begin(), books.items.end(),
                            [](const Book& b) { return b.stock == 0; });
    if (oos != books.items.end())
        std::cout << "  first OOS: " << *oos << "\n";

    bool hasExpensive = std::any_of(books.items.begin(), books.items.end(),
                                    [](const Book& b) { return b.price > 50.0; });
    std::cout << "  any > $50? " << std::boolalpha << hasExpensive << "\n";

    auto inStockCount = std::count_if(books.items.begin(), books.items.end(),
                                      [](const Book& b) { return b.stock > 0; });
    std::cout << "  in stock: " << inStockCount << " / " << books.size() << "\n\n";

    // ---- 6. std::partition — split into "in stock" vs "out of stock" ------
    std::cout << "[6] std::partition — split by availability\n";
    Catalog<Book, BookKey> partitioned = books;
    auto split = std::partition(partitioned.items.begin(),
                                partitioned.items.end(),
                                [](const Book& b) { return b.stock > 0; });
    std::vector<Book> inStockVec, outOfStockVec;
    std::copy(partitioned.items.begin(), split,         std::back_inserter(inStockVec));
    std::copy(split,               partitioned.items.end(),
              std::back_inserter(outOfStockVec));
    printAll(inStockVec,    "in stock");
    printAll(outOfStockVec, "out of stock");
    std::cout << "\n";

    // ---- 7. std::map<std::string, std::size_t> — SKU -> catalog index ------
    std::cout << "[7] std::map — side index for O(log n) SKU lookup\n";
    std::map<std::string, std::size_t> skuIndex;
    for (std::size_t i = 0; i < books.size(); ++i)
        skuIndex[books.items[i].isbn] = i;
    std::cout << "  index size: " << skuIndex.size() << "\n";
    auto it = skuIndex.find("978-0321563842");
    if (it != skuIndex.end())
        std::cout << "  found C++PL at index " << it->second << "\n";
    std::cout << "\n";

    // ---- 8. totalRevenue — generic with a price projection ----------------
    //   Same implementation, different callables.  This is how STL
    //   algorithms stay type-agnostic — they don't peek at members, they
    //   take a callable that returns what the algorithm needs.
    std::cout << "[8] totalRevenue — same code, different price projections\n";
    std::cout << "  books total revenue: $"
              << totalRevenue(books.items,
                              [](const Book& b) { return b.price; }) << "\n";

    Catalog<Part, PartKey> parts = makeCatalog<Part, PartKey>(
        PartKey{},
        Part{"R-1k-0805",     "10k resistor 0805",      "resistor", 0.012, 5000},
        Part{"C-100n-0603",   "100nF capacitor 0603",   "capacitor", 0.045, 1200},
        Part{"IC-ATMEGA328",  "ATmega328P-PU",          "ic",        3.20,   85},
        Part{"R-100-1206",    "100 ohm resistor 1206",  "resistor",  0.018,    0}
    );
    std::cout << "  parts total revenue: $"
              << totalRevenue(parts.items,
                              [](const Part& p) { return p.unitCost; }) << "\n\n";

    // ---- 9. filter — copy_if under the hood -------------------------------
    std::cout << "[9] Catalog::filter — copy_if, predicate is a lambda\n";
    auto resistors = parts.filter(
        [](const Part& p) { return p.category == "resistor"; });
    printAll(resistors.items, "resistors only");
    std::cout << "\n";

    // ---- 10. Static-assert guard on printAll ------------------------------
    std::cout << "[10] printAll is compile-time-gated by is_streamable\n";
    std::cout << "  is_streamable_v<Book>            = "
              << std::boolalpha << is_streamable_v<Book>            << "\n";
    std::cout << "  is_streamable_v<int>             = "
              << is_streamable_v<int>             << "\n";
    std::cout << "  is_streamable_v<std::vector<int>> = "
              << is_streamable_v<std::vector<int>> << "\n";
    std::cout << "  (vector<int> is NOT streamable — std::vector deliberately"
                 " does not define operator<<, so generic code can't accidentally"
                 " dump its guts without a loop)\n\n";

    std::cout << "============================================================\n";
    std::cout << " Done. Templates + STL working together.\n";
    std::cout << "============================================================\n";
    return 0;
}
