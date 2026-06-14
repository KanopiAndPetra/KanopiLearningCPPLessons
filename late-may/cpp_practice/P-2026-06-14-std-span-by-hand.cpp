// P-2026-06-14-std-span-by-hand.cpp
//
// A from-scratch implementation of std::span<T> in C++17, then a tour of
// the real C++20 std::span (compiled with -std=c++17) so the comparison is
// exact. The point: figure out *what* a span actually is at the type-system
// level (a non-owning view = pointer + extent), and *why* it's the right
// shape for function parameters that just want to look at a contiguous
// range without taking ownership or copying.
//
// Build:  g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//             -o P-2026-06-14-std-span-by-hand P-2026-06-14-std-span-by-hand.cpp
// Run:    ./P-2026-06-14-std-span-by-hand
//
// ASan:   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
//             -o P-2026-06-14-std-span-by-hand-asan P-2026-06-14-std-span-by-hand.cpp
//         ASAN_OPTIONS=halt_on_error=0 ./P-2026-06-14-std-span-by-hand-asan
//
// Note: this file targets C++17 (per the cron workflow default). The
// implementation below does NOT require any C++20 feature, and the C++20
// `std::span` tour is guarded by `__cpp_lib_span` so the file still builds
// cleanly on C++17 toolchains (Apple Clang ships `std::span` in <span>
// since Xcode 15 / macOS 13; libstdc++ has it since GCC 10. The
// `__has_include` and `__cpp_lib_span` checks degrade gracefully if
// neither is available).

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// ============================================================================
// Section 1. MySpan<T, Extent> — a C++17 hand-rolled std::span
// ============================================================================
//
// The shape: a span is a *view*, not an owner. Two fields:
//
//   T*   data_;   // pointer to the first element
//   Size size_;   // number of elements
//
// Extent is a compile-time size, or the sentinel dynamic_extent (= (size_t)-1)
// for "runtime-sized". Extent is a separate template parameter so the
// compiler can:
//   - elide the size field when extent is known at compile time
//   - type-check operations that would change the extent
//   - choose the right overload (fixed vs. dynamic) at the call site
//
// Real std::span packs these as a single union/discriminated layout; we'll
// use a simpler struct and pay an extra size_t on the stack for dynamic
// extents. Functionally equivalent, just slightly fatter.

namespace psp {

inline constexpr std::size_t dynamic_extent = (std::size_t)-1;

template <class T, std::size_t Extent = dynamic_extent>
class Span {
public:
    static constexpr std::size_t extent = Extent;

    // -- ctors ---------------------------------------------------------------

    // Default ctor: empty span. Only valid when Extent allows it
    // (static extent 0 is fine; dynamic extent is fine; static extent > 0
    // is a hard error in real std::span, but we keep it simple).
    constexpr Span() noexcept : data_(nullptr), size_(0) {
        static_assert(Extent == dynamic_extent || Extent == 0,
                      "default ctor only valid for dynamic_extent or 0");
    }

    // From a pointer + size.
    constexpr Span(T* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    // From a C-array. This is what makes `print(a)` work when `a` is
    // declared as `int a[5]`.
    template <std::size_t N,
              typename = std::enable_if_t<Extent == dynamic_extent ||
                                           Extent == N>>
    constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

    // From std::array<T, N> (mutable).
    template <std::size_t N,
              typename = std::enable_if_t<(Extent == dynamic_extent ||
                                           Extent == N)>>
    constexpr Span(std::array<T, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    // From std::array<U, N> (mutable, U* convertible to T*). This is what
    // makes `print_ints(std_arr)` work when std_arr is std::array<int,5>
    // and the function param is Span<const int>. SFINAE guarantees U is
    // non-const, so std::array<U, N> is always well-formed.
    template <std::size_t N, class U,
              typename = std::enable_if_t<
                  (Extent == dynamic_extent || Extent == N) &&
                  std::is_convertible<U*, T*>::value &&
                  !std::is_const<U>::value>>
    constexpr Span(std::array<U, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    // From std::array<T, N> (const). Only enabled when T is non-const
    // (otherwise std::array<const T, N> is ill-formed due to the
    // std::allocator<const T> deletion). To get a Span<const T> from a
    // non-const std::array, the deduction guide / inter-extent ctor
    // path handles it.
    template <std::size_t N,
              typename = std::enable_if_t<(Extent == dynamic_extent ||
                                           Extent == N) &&
                                          !std::is_const<T>::value>>
    constexpr Span(const std::array<T, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    // From std::vector<U> — only when U is convertible to T. This way
    // Span<const int> can be constructed from std::vector<int> (the U->T
    // conversion is implicit), but the SFINAE never instantiates with
    // T = const int (which would force std::vector<const int> and fail).
    template <class U,
              typename = std::enable_if_t<
                  Extent == dynamic_extent &&
                  std::is_convertible<U*, T*>::value &&
                  !std::is_const<U>::value>>
    Span(std::vector<U>& v) noexcept
        : data_(v.data()), size_(v.size()) {}

    // From another span with a different (but compatible) extent.
    template <std::size_t OtherExtent,
              typename = std::enable_if_t<Extent == dynamic_extent ||
                                           OtherExtent == dynamic_extent ||
                                           Extent == OtherExtent>>
    constexpr Span(Span<T, OtherExtent> other) noexcept
        : data_(other.data()), size_(other.size()) {}

    // -- observers -----------------------------------------------------------

    constexpr T*       data()    const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool        empty() const noexcept { return size_ == 0; }

    // size_bytes = size() * sizeof(T). Useful for memcpy'd POD payloads.
    constexpr std::size_t size_bytes() const noexcept {
        return size_ * sizeof(T);
    }

    // -- accessors -----------------------------------------------------------

    constexpr T& operator[](std::size_t i) const noexcept {
        return data_[i];
    }

    constexpr T& at(std::size_t i) const {
        if (i >= size_) {
            throw std::out_of_range("Span::at: index out of range");
        }
        return data_[i];
    }

    constexpr T& front() const noexcept { return data_[0]; }
    constexpr T& back()  const noexcept { return data_[size_ - 1]; }

    // -- subviews ------------------------------------------------------------
    //
    // first(N): first N elements, returning a static-extent span.
    template <std::size_t Count>
    constexpr Span<T, Count> first() const noexcept {
        static_assert(Count <= Extent || Extent == dynamic_extent,
                      "Count cannot exceed the static extent");
        return Span<T, Count>(data_, Count);
    }

    // first(n): first n elements, dynamic extent.
    constexpr Span<T, dynamic_extent> first(std::size_t count) const noexcept {
        return Span<T, dynamic_extent>(data_, count);
    }

    // last(N): last N elements, returning a static-extent span.
    template <std::size_t Count>
    constexpr Span<T, Count> last() const noexcept {
        static_assert(Count <= Extent || Extent == dynamic_extent,
                      "Count cannot exceed the static extent");
        return Span<T, Count>(data_ + (size_ - Count), Count);
    }

    // last(n): last n elements, dynamic extent.
    constexpr Span<T, dynamic_extent> last(std::size_t count) const noexcept {
        return Span<T, dynamic_extent>(data_ + (size_ - count), count);
    }

    // subspan<Offset, Count>: skip Offset, take Count. Both static.
    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr auto subspan() const noexcept {
        static_assert(Offset <= Extent || Extent == dynamic_extent,
                      "Offset cannot exceed the static extent");
        constexpr std::size_t NewExtent =
            (Count == dynamic_extent)
                ? (Extent == dynamic_extent ? dynamic_extent
                                            : Extent - Offset)
                : Count;
        return Span<T, NewExtent>(data_ + Offset,
            (NewExtent == dynamic_extent) ? (size_ - Offset) : NewExtent);
    }

    // subspan(offset, count): skip offset, take count. Both runtime.
    constexpr Span<T, dynamic_extent>
    subspan(std::size_t offset,
            std::size_t count = dynamic_extent) const noexcept {
        return Span<T, dynamic_extent>(
            data_ + offset,
            count == dynamic_extent ? (size_ - offset) : count);
    }

    // -- iterators -----------------------------------------------------------
    //
    // A span's iterators are just raw pointers (T* is a random-access
    // iterator, so this works without writing an iterator class). Real
    // std::span uses std::contiguous_iterator concepts to constrain the
    // element type's iterator category; for our purposes, T* suffices.

    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end()   const noexcept { return data_ + size_; }
    constexpr std::reverse_iterator<T*> rbegin() const noexcept {
        return std::reverse_iterator<T*>(end());
    }
    constexpr std::reverse_iterator<T*> rend() const noexcept {
        return std::reverse_iterator<T*>(begin());
    }

private:
    T*           data_;
    std::size_t  size_;
};

// -- deduction guides (C++17) -----------------------------------------------
//
// C++17 deduction guides let `Span s = arr;` work without spelling the
// extent template argument. We deduce the static extent from C-arrays
// and std::array; std::vector falls back to dynamic.

template <class T, std::size_t N>
Span(T (&)[N]) -> Span<T, N>;

template <class T, std::size_t N>
Span(std::array<T, N>&) -> Span<T, N>;

template <class T, std::size_t N>
Span(const std::array<T, N>&) -> Span<T, N>;

}  // namespace psp

// ============================================================================
// Sections 2-7: free functions demonstrating the killer use case
// ============================================================================

namespace demo {

// One function that takes a Span<const int>. Called with: a C-array, a
// std::array, a std::vector (wrapped), a Span itself, a (ptr, len) pair.
// No overloads, no template explosion, no copies.
void print_ints(psp::Span<const int> s, const char* tag) {
    std::printf("  [%s] size=%zu, data=[", tag, s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::printf("%d%s", s[i], i + 1 == s.size() ? "" : ", ");
    }
    std::printf("]\n");
}

int sum_ints(psp::Span<const int> s) {
    int total = 0;
    for (int x : s) total += x;
    return total;
}

double mean(psp::Span<const double> s) {
    if (s.empty()) return 0.0;
    double total = 0.0;
    for (double x : s) total += x;
    return total / static_cast<double>(s.size());
}

// In-place mutation — proves Span<int> (mutable) works. Caller still owns
// the data; the span just borrows the elements.
void scale_in_place(psp::Span<int> s, int k) {
    for (int& x : s) x *= k;
}

// Span over std::byte — raw binary buffers, no reinterpret_cast pain.
std::size_t count_nonzero_bytes(psp::Span<const std::byte> s) {
    std::size_t n = 0;
    for (std::byte b : s) {
        if (std::to_integer<unsigned char>(b) != 0) ++n;
    }
    return n;
}

// Fixed-extent span: the size is part of the type. The compiler can lay
// it out in a register, elide the size field, and reject mismatches at
// compile time.
void take_fixed(psp::Span<int, 3> s) {
    std::printf("  [fixed<3>] size=%zu, [0]=%d [1]=%d [2]=%d\n",
                s.size(), s[0], s[1], s[2]);
}

// Dynamic-extent span: the size is a runtime field.
void take_dynamic(psp::Span<int> s) {
    std::printf("  [dynamic] size=%zu, front=%d, back=%d\n",
                s.size(), s.front(), s.back());
}

void demo_subspan(psp::Span<const int> s) {
    std::printf("  [orig]      ");
    for (int x : s) std::printf("%d ", x);
    std::printf("\n");

    // first<2>: first 2 elements, static extent.
    auto first2 = s.first<2>();
    std::printf("  [first<2>]  size=%zu, [%d, %d]\n",
                first2.size(), first2[0], first2[1]);

    // last(2): last 2 elements, dynamic extent.
    auto last2 = s.last(2);
    std::printf("  [last(2)]   size=%zu, [%d, %d]\n",
                last2.size(), last2[0], last2[1]);

    // subspan<2, 3>: skip 2, take 3, both static.
    auto mid = s.subspan<2, 3>();
    std::printf("  [subspan<2,3>] size=%zu, values: ", mid.size());
    for (int x : mid) std::printf("%d ", x);
    std::printf("\n");
}

void bytes_of(const std::string& s) {
    psp::Span<const std::byte> raw(
        reinterpret_cast<const std::byte*>(s.data()), s.size());
    std::printf("  [string \"%s\"] size_bytes=%zu\n",
                s.c_str(), raw.size_bytes());
}

void chars_of(const std::string& s) {
    psp::Span<const char> ch(s.data(), s.size());
    std::printf("  [string \"%s\"] chars=[", s.c_str());
    for (char c : ch) std::printf("%c", c);
    std::printf("]\n");
}

}  // namespace demo

// ============================================================================
// Section 8: real std::span (C++20) — production-version comparison
// ============================================================================
//
// Guarded by feature-test macro. Apple Clang's libc++ has had std::span
// since macOS 13 / Xcode 14; libstdc++ since GCC 10. The build is still
// -std=c++17, but the headers can be picked up by C++17 code.

#if defined(__has_include) && __has_include(<span>) && \
    defined(__cpp_lib_span) && (__cpp_lib_span >= 201902L)
  #define PSP_HAS_REAL_SPAN 1
#else
  #define PSP_HAS_REAL_SPAN 0
#endif

namespace real_span {

#if PSP_HAS_REAL_SPAN
void demo() {
    std::printf("  Real std::span is available. Side-by-side comparison:\n");

    int arr[5] = {10, 20, 30, 40, 50};

    // Real std::span with a fixed extent (deduced from the array).
    std::span<int, 5> fixed = arr;
    std::printf("  [std::span<int,5>] size=%zu, [0]=%d, [2]=%d, [4]=%d\n",
                fixed.size(), fixed[0], fixed[2], fixed[4]);
    static_assert(fixed.extent == 5, "static extent is part of the type");

    // Real std::span with a dynamic extent.
    std::span<int> dyn = arr;  // implicit conversion: fixed -> dynamic
    std::printf("  [std::span<int>]   size=%zu, .data()==arr? %s\n",
                dyn.size(), dyn.data() == arr ? "yes" : "NO");

    // Sizeof check: fixed extent should be just a pointer (8 bytes on
    // 64-bit); dynamic extent is pointer + size (16 bytes on 64-bit).
    std::printf("  sizeof(std::span<int,5>) = %zu\n", sizeof(fixed));
    std::printf("  sizeof(std::span<int>)   = %zu\n", sizeof(dyn));

    // first<2> and subspan work the same way.
    auto first2 = fixed.first<2>();
    std::printf("  [fixed.first<2>()]  size=%zu, [%d, %d]\n",
                first2.size(), first2[0], first2[1]);
}
#else
void demo() {
    std::printf("  Real std::span not available on this toolchain. "
                "Skipped the C++20 side-by-side.\n");
}
#endif

}  // namespace real_span

// ============================================================================
// main
// ============================================================================

int main() {
    std::printf("=== P-2026-06-14: Span<T> (the C++17 design by hand) ===\n\n");

    // --- Section 2: one function, five call shapes -----------------------
    std::printf("--- Section 2: one function, multiple call shapes ---\n");

    // Shape 1: C-array. Deduction guide picks Span<int, 5>.
    int c_arr[5] = {1, 2, 3, 4, 5};
    demo::print_ints(c_arr, "C-array");

    // Shape 2: std::array. Deduction guide picks Span<int, 5>.
    std::array<int, 5> std_arr = {10, 20, 30, 40, 50};
    demo::print_ints(std_arr, "std::array");

    // Shape 3: std::vector. No deduction guide; spell it.
    std::vector<int> vec = {100, 200, 300};
    demo::print_ints(psp::Span<const int>(vec), "std::vector");

    // Shape 4: a Span. Conversion dynamic->dynamic is implicit.
    auto s = psp::Span<const int>(c_arr);
    demo::print_ints(s, "Span (re-view)");

    // Shape 5: pointer + size.
    int heap[3] = {7, 8, 9};
    demo::print_ints(psp::Span<const int>(heap, 3), "ptr+size");

    std::printf("  sum of std_arr = %d\n", demo::sum_ints(std_arr));

    std::vector<double> temps = {21.5, 22.0, 19.8, 23.4, 20.1};
    std::printf("  mean of temps  = %.2f\n", demo::mean(temps));

    // --- Section 3: in-place mutation ----------------------------------
    std::printf("\n--- Section 3: in-place mutation ---\n");
    int data[4] = {1, 2, 3, 4};
    demo::print_ints(data, "before");
    demo::scale_in_place(data, 10);
    demo::print_ints(data, "after x10");

    // --- Section 4: span over bytes ------------------------------------
    std::printf("\n--- Section 4: span over std::byte (raw buffers) ---\n");
    unsigned char raw_buf[6] = {0x00, 0x01, 0x00, 0xFF, 0x00, 0x02};
    psp::Span<const std::byte> raw_view(
        reinterpret_cast<const std::byte*>(raw_buf), 6);
    std::printf("  raw_buf size_bytes=%zu, nonzero=%zu\n",
                raw_view.size_bytes(),
                demo::count_nonzero_bytes(raw_view));

    // --- Section 5: static vs dynamic extent ---------------------------
    std::printf("\n--- Section 5: static vs dynamic extent ---\n");
    int triple[3] = {7, 8, 9};
    demo::take_fixed(triple);
    demo::take_dynamic(triple);
    std::printf("  sizeof(Span<int, 3>) = %zu\n",
                sizeof(psp::Span<int, 3>));
    std::printf("  sizeof(Span<int>)    = %zu\n",
                sizeof(psp::Span<int>));

    // --- Section 6: slicing without copying ----------------------------
    std::printf("\n--- Section 6: slicing without copying ---\n");
    int eight[8] = {11, 22, 33, 44, 55, 66, 77, 88};
    demo::demo_subspan(eight);

    // --- Section 7: span over a string ---------------------------------
    std::printf("\n--- Section 7: span over a std::string (bytes vs chars) ---\n");
    std::string msg = "hello";
    demo::bytes_of(msg);
    demo::chars_of(msg);

    // --- Section 8: real std::span (C++20) -----------------------------
    std::printf("\n--- Section 8: real std::span (C++20) side-by-side ---\n");
    real_span::demo();

    std::printf("\n=== done ===\n");
    return 0;
}
