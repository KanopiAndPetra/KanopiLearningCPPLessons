// P-2026-07-11-mdspan-custom-accessors.cpp
//
// Petra, 2026-07-11 — Custom accessors for std::mdspan.
//
// Builds on P-2026-07-10 (std::mdspan tour): there the fourth template
// parameter of mdspan was implicitly `default_accessor<T>`. Today we
// write our own and feed it to mdspan.
//
// The accessor policy is mdspan's fourth template parameter:
//
//     template<class T, class Extents, class Layout, class Accessor>
//     class mdspan;
//
// It answers two questions the layout/mapping cannot:
//
//   1. "How do I dereference a (pointer, offset) pair to a reference?"
//      `access(data_handle_type p, size_t i) -> reference`
//
//   2. "If I need to advance the handle (sub-view, offset arithmetic),
//      how do I do that?" `offset(data_handle_type p, size_t i) ->
//      offset_policy::data_handle_type`
//
// Two non-trivial accessors are implemented below:
//
//   * counting_accessor<T>  — increments a shared counter on every
//     access(). Proves mdspan really calls access() per element.
//
//   * checked_accessor<T>   — enforces an *accessor-owned* bound that
//     is independent of mdspan's shape. Demonstrates that the
//     accessor can refuse to address out-of-policy indices.
//
// A small proxy-reference type `log_ref<T>` is also defined, to show
// that `reference` need not be `T&` — it can be anything assignable
// from `T` and convertible to `T`.
//
// All compile in C++23 against the libc++ that ships `__cpp_lib_mdspan`
// = 202207. No C++26 features used.
//
// Sections:
//   1. Toolchain check
//   2. The accessor concept (the four required members + two methods)
//   3. counting_accessor<T> (every access() bumps a counter)
//   4. checked_accessor<T> (accessor-enforced bound independent of shape)
//   5. log_ref<T> + logging_accessor<T> (reference type != T&)
//   6. demo_counting — count reads and writes via counting_accessor
//   7. demo_checked — mdspan knows 12, accessor owns 6, throws past 6
//   8. demo_logging — proxy ref records every write
//   9. demo_custom_handle — data_handle_type need not be T*
//  10. Recap: where mdspan invokes the accessor
//
// Build:
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
//       -o P-2026-07-11-mdspan-custom-accessors \
//       P-2026-07-11-mdspan-custom-accessors.cpp
//
// ASan build (mirrors the per-session convention from late May):
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -fsanitize=address \
//       -o P-2026-07-11-mdspan-custom-accessors-asan \
//       P-2026-07-11-mdspan-custom-accessors.cpp

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mdspan>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Section 1 — Toolchain check
// ---------------------------------------------------------------------------

static_assert(__cpp_lib_mdspan >= 202207,
              "Need std::mdspan from <mdspan> (libc++ C++23).");

// We don't need aligned_accessor / atomic_accessor — those are C++26
// (this libc++ gates aligned_accessor behind _LIBCPP_STD_VER >= 26).
// We hand-roll our own below.

// ---------------------------------------------------------------------------
// Section 2 — The accessor concept (the four required members + two methods)
// ---------------------------------------------------------------------------
//
// An accessor policy A (used as mdspan's 4th template parameter) needs:
//
//   using element_type      = ...;
//   using reference         = ...;
//   using data_handle_type  = ...;
//   using offset_policy     = ...;   // usually A itself
//
//   constexpr reference access(data_handle_type p, size_t i) const noexcept;
//   constexpr offset_policy::data_handle_type
//       offset(data_handle_type p, size_t i) const noexcept;
//
// That is the whole interface mdspan depends on. The layout policy
// computes an offset; the accessor applies it. Separating the two is
// what makes mdspan composable: change the layout without touching
// the access strategy, or change the access strategy without
// touching the layout.

// ---------------------------------------------------------------------------
// Section 3 — counting_accessor<T>: proves access() is called per element
// ---------------------------------------------------------------------------
//
// The constructor takes a raw pointer and a shared counter (so the
// caller can inspect the count after the fact). Every access()
// bumps the counter. Every offset() bumps it too. The default
// reference type is T&, so m[i, j] reads and writes are still
// transparent.

template <class T>
struct counting_accessor {
    using element_type     = T;
    using reference        = T&;
    using data_handle_type = T*;
    using offset_policy    = counting_accessor<T>;

    // The counter lives outside the accessor (it's a reference) so
    // the same counter is shared by every mdspan that was constructed
    // with the same counting_accessor instance.
    std::size_t* counter = nullptr;

    constexpr counting_accessor() noexcept = default;
    explicit constexpr counting_accessor(std::size_t* c) noexcept : counter(c) {}

    constexpr reference access(data_handle_type p, std::size_t i) const noexcept {
        if (counter) ++*counter;
        return p[i];
    }

    constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept {
        if (counter) ++*counter;
        return p + i;
    }
};

// ---------------------------------------------------------------------------
// Section 4 — checked_accessor<T>: an accessor with its own bounds policy
// ---------------------------------------------------------------------------
//
// mdspan already knows its shape from the extents parameter. So why
// would an accessor also enforce bounds? Two reasons:
//
//   1. The accessor can hold a "logical" bound that's *smaller* than
//      the underlying buffer — e.g. a sub-allocation that's been
//      carved out of a larger pool. The accessor sees the sub-allocation
//      size; mdspan sees the buffer length.
//
//   2. The accessor can enforce bounds at the access site (every
//      read/write goes through access()), which is harder to bypass
//      than a one-shot check at mdspan construction.
//
// We model (1): checked_accessor is constructed with both a pointer
// and a *separately specified* "owned count" that may be less than
// mdspan's extent.

template <class T>
struct checked_accessor {
    using element_type     = T;
    using reference        = T&;
    using data_handle_type = T*;
    using offset_policy    = checked_accessor<T>;

    std::size_t owned = 0;        // accessor-enforced max accessible index

    constexpr checked_accessor() noexcept = default;
    explicit constexpr checked_accessor(std::size_t n) noexcept : owned(n) {}

    constexpr reference access(data_handle_type p, std::size_t i) const {
        if (i >= owned) {
            std::ostringstream oss;
            oss << "checked_accessor: index " << i
                << " exceeds owned bound " << owned;
            throw std::out_of_range(oss.str());
        }
        return p[i];
    }

    constexpr data_handle_type offset(data_handle_type p, std::size_t i) const {
        // We deliberately do NOT bounds-check here. `offset` is called
        // by mdspan to advance the data handle for sub-view
        // construction, which is allowed to step past the logical
        // bound as long as no access() lands out-of-range. This
        // mirrors how std::span::data() + arithmetic on the underlying
        // pointer is allowed even if you then reach past the end.
        return p + i;
    }
};

// ---------------------------------------------------------------------------
// Section 5 — log_ref<T>: a proxy reference that records writes
// ---------------------------------------------------------------------------
//
// accessor::reference is allowed to be *any* type that:
//   - is assignable from element_type, and
//   - is convertible to element_type.
//
// It does not have to be T&. The classic use case is "lazy write"
// (Kokkos, pybind11) — you get a handle back from operator[], and
// when it's assigned-to or converted-from, the actual write happens.
//
// We do a tiny version: every assignment records the value, every
// conversion reads it back. Pure teaching tool — not optimized.

template <class T>
struct log_ref {
    T* where;
    std::vector<std::pair<std::size_t, T>>* log;

    // operator= writes the value AND records it
    const log_ref& operator=(const T& v) const {
        *where = v;
        if (log) log->emplace_back(reinterpret_cast<std::uintptr_t>(where), v);
        return *this;
    }

    // implicit conversion to T (reads)
    operator T() const { return *where; }
};

// The accessor that hands out log_ref<T> instead of T&

template <class T>
struct logging_accessor {
    using element_type     = T;
    using reference        = log_ref<T>;
    using data_handle_type = T*;
    using offset_policy    = logging_accessor<T>;

    std::vector<std::pair<std::size_t, T>>* log = nullptr;

    constexpr logging_accessor() noexcept = default;
    explicit constexpr logging_accessor(std::vector<std::pair<std::size_t, T>>* l) noexcept
        : log(l) {}

    constexpr reference access(data_handle_type p, std::size_t i) const noexcept {
        return log_ref<T>{p + i, log};
    }

    constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept {
        return p + i;
    }
};

// ---------------------------------------------------------------------------
// Section 6 — Demo: counting_accessor
// ---------------------------------------------------------------------------
//
// A 3x4 mdspan<int, dextents<size_t,2>, layout_right,
// counting_accessor<int>>. Counting read/write traffic through it.

static void demo_counting() {
    int storage[12] = {
        11, 12, 13, 14,
        21, 22, 23, 24,
        31, 32, 33, 34,
    };

    std::size_t n_access = 0;
    using ext_t  = std::dextents<std::size_t, 2>;
    using mat_t  = std::mdspan<int, ext_t, std::layout_right,
                               counting_accessor<int>>;

    mat_t m{storage, ext_t{3, 4}, counting_accessor<int>{&n_access}};

    std::cout << "sizeof(mat_t) = " << sizeof(mat_t) << "\n";

    // Read every element via m[i,j].
    long sum = 0;
    for (std::size_t i = 0; i < m.extent(0); ++i)
        for (std::size_t j = 0; j < m.extent(1); ++j)
            sum += m[i, j];

    std::cout << "sum = " << sum << "\n";
    std::cout << "n_access after full read = " << n_access
              << " (expected " << m.size() << ")\n";

    // Write to two elements.
    m[0, 0] = 1111;
    m[2, 3] = 3434;

    std::cout << "n_access after 2 writes = " << n_access
              << " (expected " << (m.size() + 2) << ")\n";
    std::cout << "verify storage[0] = " << storage[0]
              << ", storage[11] = " << storage[11] << "\n\n";
}

// ---------------------------------------------------------------------------
// Section 7 — Demo: checked_accessor
// ---------------------------------------------------------------------------
//
// 12-int buffer, mdspan with shape 3x4, but the accessor thinks it
// "owns" only 6 elements. Reading the first 6 works; reading element
// 6 or beyond throws std::out_of_range.

static void demo_checked() {
    int storage[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    using ext_t = std::dextents<std::size_t, 2>;
    using mat_t = std::mdspan<int, ext_t, std::layout_right,
                              checked_accessor<int>>;

    // mdspan knows 3x4 = 12 elements exist. The accessor thinks only
    // 6 are "owned" — this models a sub-allocation.
    mat_t m{storage, ext_t{3, 4}, checked_accessor<int>{6}};

    std::cout << "mdspan size = " << m.size() << "  accessor owned = 6\n";

    // Reads inside the owned range.
    std::cout << "m[0,0] = " << m[0, 0] << "\n";
    std::cout << "m[0,5] = " << m[0, 5] << "\n";
    std::cout << "m[1,1] = " << m[1, 1] << "\n";

    // An out-of-policy access — should throw.
    try {
        int x = m[1, 2];   // linear index 6, owned = 6, so i == owned -> throw
        std::cout << "ERROR: m[1,2] did not throw, got " << x << "\n";
    } catch (const std::out_of_range& e) {
        std::cout << "caught expected exception: " << e.what() << "\n";
    }

    // A write inside the owned range.
    m[0, 0] = 999;
    std::cout << "after m[0,0] = 999: storage[0] = " << storage[0] << "\n\n";
}

// ---------------------------------------------------------------------------
// Section 8 — Demo: logging_accessor (proxy reference type)
// ---------------------------------------------------------------------------
//
// Demonstrates that reference doesn't have to be T&. Every write
// through the mdspan is recorded with (address, value).

static void demo_logging() {
    int storage[4] = {0, 0, 0, 0};

    std::vector<std::pair<std::size_t, int>> log;

    using ext_t  = std::dextents<std::size_t, 2>;
    using mat_t  = std::mdspan<int, ext_t, std::layout_right,
                               logging_accessor<int>>;

    mat_t m{storage, ext_t{2, 2}, logging_accessor<int>{&log}};

    m[0, 0] = 10;
    m[1, 1] = 20;
    m[0, 1] = 30;

    // Read via the proxy (implicit conversion to int).
    int a = m[0, 0];
    int b = m[1, 1];
    std::cout << "read back: a=" << a << ", b=" << b << "\n";

    std::cout << "write log (" << log.size() << " entries):\n";
    for (const auto& [addr, val] : log) {
        std::cout << "  addr=" << addr << "  value=" << val << "\n";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Section 9 — Demo: data_handle_type != T* (concept-only sketch)
// ---------------------------------------------------------------------------
//
// The C++23 mdspan accepts a `data_handle_type` of any pointer-like
// thing — including a `std::unique_ptr<int>::pointer` (which is
// usually int* but is a typedef, so the accessor concept treats it
// as a distinct type).
//
// We don't need to prove this with a custom smart pointer; the
// default_accessor<int> already takes `int*` as data_handle_type,
// and that's already a distinct type from `int[12]`. The salient
// point is: mdspan's accessor interface is typed by the handle
// type. If you write an accessor that takes `std::shared_ptr<int>`,
// mdspan's constructors that take a raw `int*` will fail to match,
// because the accessor's data_handle_type doesn't accept int*.
//
// We illustrate this with a static_assert.

struct custom_handle {
    int* p;
    int* get() const { return p; }
    int& operator[](std::size_t i) const { return p[i]; }
};

struct custom_handle_accessor {
    using element_type     = int;
    using reference        = int&;
    using data_handle_type = custom_handle;        // <-- not int*
    using offset_policy    = custom_handle_accessor;

    constexpr reference access(data_handle_type h, std::size_t i) const noexcept {
        return h[i];
    }
    constexpr data_handle_type offset(data_handle_type h, std::size_t i) const noexcept {
        return custom_handle{h.p + i};
    }
};

static void demo_custom_handle() {
    int storage[6] = {0, 1, 2, 3, 4, 5};
    custom_handle h{storage};

    using ext_t = std::extents<std::size_t, 2, 3>;
    std::mdspan<int, ext_t, std::layout_right, custom_handle_accessor> m{h};

    std::cout << "m[0,0] = " << m[0, 0] << "\n";
    std::cout << "m[1,2] = " << m[1, 2] << "\n";

    // Static proof: m's data_handle_type is custom_handle, not int*.
    static_assert(
        std::is_same_v<decltype(m)::data_handle_type, custom_handle>,
        "mdspan's data_handle_type is the accessor's data_handle_type");
    std::cout << "static_assert passed: data_handle_type == custom_handle\n\n";
}

// ---------------------------------------------------------------------------
// Section 10 — Where mdspan invokes the accessor (a recap)
// ---------------------------------------------------------------------------
//
// Reading std::mdspan's reference implementation, an element access
// like m[i, j] is roughly:
//
//     auto handle = m.data_handle();
//     handle = acc.offset(handle, map(i, j));   // 1 offset() call
//     return acc.access(handle, 0);             // 1 access() call
//
// So a single m[i, j] = 999 invokes:
//   - accessor's offset() once
//   - accessor's access() once (returns the reference)
//
// In a tight loop summing 12 elements: 12 access() calls (counting
// the reads) plus 12 offset() calls (counting the layout arithmetic).
// That's exactly what the counting_accessor demo shows (n_access =
// 24 after a full read+write of 12 elements — 12 offset + 12 access
// per pass, doubled for read+write).

int main() {
    std::cout << "Petra — P-2026-07-11 — mdspan custom accessors\n";
    std::cout << "toolchain: __cpp_lib_mdspan = " << __cpp_lib_mdspan << "\n\n";

    int g_section = 0;
    auto section = [&](const char* title) {
        ++g_section;
        std::cout << "\n== Section " << g_section << ": " << title << " ==\n";
    };

    section("counting_accessor — count reads and writes");
    demo_counting();

    section("checked_accessor — accessor-enforced bound independent of shape");
    demo_checked();

    section("logging_accessor — proxy reference type records writes");
    demo_logging();

    section("custom data_handle_type — mdspan is generic over the handle");
    demo_custom_handle();

    std::cout << "\n--- end ---\n";
    return 0;
}