// P-2026-07-09 — std::span (C++20): the real thing.
//
// This is a tour of std::span<T, Extent> as it ships in <span>. The arc to
// here is:
//
//   Jun 14  P-2026-06-14-std-span-by-hand  — hand-rolled psp::Span<T,Extent>
//                                            in C++17, plus a brief
//                                            comparison section against the
//                                            real C++20 std::span.
//   Jun 28–Jul 8  psp_span_lib packaged as a static library + CI release
//                                            pipeline; the production span
//                                            is psp::Span (our hand-rolled).
//   Jul  9  std::span (C++20)             ← today: a dedicated deep-dive
//                                            into the standard one, with
//                                            concrete exercises and a
//                                            side-by-side against psp::Span.
//
// The point of today's lesson is NOT to replace psp::Span with std::span —
// they're functionally very close. It's to (a) get fluent with the standard
// name and API so I reach for it by default in new code, (b) understand
// exactly where std::span differs from the hand-rolled (no .at(), the union
// layout that elides the size field for static extents, the
// span<T> -> span<const T> implicit conversion, the .size_bytes() observer,
// the lack of a default-ctor for non-zero static extent), and (c) verify
// on the toolchain at hand that std::span is fully wired up and what
// feature-test macro gates it.
//
// BUILD / RUN
// -----------
//   g++ -Wall -Wextra -Wpedantic -std=c++20 -O0 -g \
//       -o P-2026-07-09-std-span-cpp20 P-2026-07-09-std-span-cpp20.cpp
//   ./P-2026-07-09-std-span-cpp20
//
// ASan (no surprises expected — span is non-owning, no allocations):
//   g++ -Wall -Wextra -Wpedantic -std=c++20 -O0 -g -fsanitize=address \
//       -o P-2026-07-09-std-span-cpp20-asan P-2026-07-09-std-span-cpp20.cpp
//   ./P-2026-07-09-std-span-cpp20-asan

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <span>
#include <type_traits>
#include <vector>

// `__cpp_lib_span` is the feature-test macro for std::span. C++20 ships it
// (value 201803L in the original proposal; P2118 adds 202002L for the
// bounds-safe changes that didn't make it into C++20; some implementations
// report 202110L once they add the constexpr fixes). We just want a
// compile-time "is std::span wired up?" check.
#ifdef __cpp_lib_span
#  define PETRA_HAS_STD_SPAN 1
#  define PETRA_SPAN_FEATURE_VALUE __cpp_lib_span
#else
#  define PETRA_HAS_STD_SPAN 0
#  define PETRA_SPAN_FEATURE_VALUE 0
#endif

// A small utility: a printer helper that takes std::span<const int> so it
// can accept anything that decays to a contiguous int range. This is the
// canonical "span as function parameter" idiom and the section 4 below
// makes it do real work.
static void print_ints(std::span<const int> s, const char* tag) {
    std::cout << "  [" << tag << "] size=" << s.size()
              << " data=" << s.data()
              << " first=" << (s.empty() ? -1 : s.front())
              << " last="  << (s.empty() ? -1 : s.back())
              << " values=";
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::cout << s[i] << (i + 1 == s.size() ? "" : ",");
    }
    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 0. Toolchain check.
// ----------------------------------------------------------------------------
// Before anything else, confirm std::span is available and what
// __cpp_lib_span reports. If this fails, the rest of the tour is moot.
static void section0_toolchain_check() {
    std::cout << "============================================================\n";
    std::cout << " Section 0. Toolchain check\n";
    std::cout << "============================================================\n";
    std::cout << "  __cpp_lib_span = " << PETRA_SPAN_FEATURE_VALUE << "\n";
    std::cout << "  PETRA_HAS_STD_SPAN = " << PETRA_HAS_STD_SPAN << "\n";
    static_assert(PETRA_HAS_STD_SPAN,
                  "This lesson requires a C++20 toolchain with std::span "
                  "in <span>. Use clang/gcc with -std=c++20 and a "
                  "sufficiently new libstdc++/libc++.");
    std::cout << "  [OK] std::span is available in <span>\n\n";
}

// ----------------------------------------------------------------------------
// Section 1. Construction.
// ----------------------------------------------------------------------------
// std::span is a non-owning view; ctors exist for every contiguous container
// shape. None of these allocate.
static void section1_construction() {
    std::cout << "============================================================\n";
    std::cout << " Section 1. Construction from contiguous ranges\n";
    std::cout << "============================================================\n";

    // From a C-style array. Note the deduction guide: span<int> deduces
    // DYNAMIC extent (we don't know at compile time how big this array is
    // *from inside a function*, though the compiler could in principle see
    // it — but the deduction is intentionally conservative).
    int carray[] = {10, 20, 30, 40, 50};
    std::span<int> from_carray = carray;
    std::cout << "  C-array -> span: size=" << from_carray.size()
              << " extent=" << from_carray.extent << "\n";

    // From std::array. With a known compile-time size, the deduction can
    // pick up a FIXED extent — std::span<int,5>.
    std::array<int, 5> stdarr = {11, 22, 33, 44, 55};
    std::span<int> from_stdarr = stdarr;   // dynamic-extent span
    std::span<int, 5> from_stdarr_fixed = stdarr;  // fixed-extent span
    std::cout << "  std::array<int,5> -> span: dynamic size=" << from_stdarr.size()
              << " fixed size=" << from_stdarr_fixed.size() << "\n";

    // From std::vector. Always dynamic (the vector's size is not part of
    // its type).
    std::vector<int> v = {1, 2, 3, 4, 5, 6};
    std::span<int> from_vec = v;
    std::cout << "  std::vector -> span: size=" << from_vec.size() << "\n";

    // From a pointer + size.
    int* p = carray + 1;
    std::span<int> from_ptr{p, 3};   // {20, 30, 40}
    std::cout << "  int* + size -> span: size=" << from_ptr.size()
              << " first=" << from_ptr.front() << "\n";

    // From another span.
    std::span<const int> from_span = from_carray;
    std::cout << "  span -> span<const>: size=" << from_span.size() << "\n";

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 2. Element access — what's there and what's deliberately NOT.
// ----------------------------------------------------------------------------
// std::span has the unchecked accessors by design: it's a view, and bounds
// checks on every access would defeat the purpose. There's no .at() (the
// hand-rolled psp::Span has one for paranoia; std::span doesn't).
static void section2_element_access() {
    std::cout << "============================================================\n";
    std::cout << " Section 2. Element access (unchecked)\n";
    std::cout << "============================================================\n";

    int a[] = {7, 8, 9};
    std::span<int> s = a;

    std::cout << "  s.front()  = " << s.front()   << "   (== s[0])\n";
    std::cout << "  s.back()   = " << s.back()    << "   (== s[size()-1])\n";
    std::cout << "  s.data()   = " << s.data()    << "   (== &a[0])\n";
    std::cout << "  s[1]       = " << s[1]        << "   (unchecked)\n";

    // The hand-rolled psp::Span has s.at(i) which throws on OOB; std::span
    // does NOT. This is intentional: spans are used in performance-sensitive
    // inner loops and bounds checks cost. If you want bounds checks, use
    // gsl::span (the parent proposal) or write your own .at() on top.
    std::cout << "  [note] std::span has NO .at() — operator[] is unchecked.\n";

    // `.size_bytes()` is new vs Jun 14's psp::Span (which has it as
    // .byte_size()). It's the standard name and it's important for things
    // like hashing and serialization.
    std::cout << "  s.size()       = " << s.size()       << "\n";
    std::cout << "  s.size_bytes() = " << s.size_bytes() << "\n";

    // .empty() is the right way to test "is this span empty" rather than
    // comparing size() to 0 — same idiom as std::string.
    std::span<int> empty_span{};
    std::cout << "  empty span: size=" << empty_span.size()
              << " empty=" << std::boolalpha << empty_span.empty() << "\n";

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 3. Iterators.
// ----------------------------------------------------------------------------
// std::span gives you begin/end (random-access), and rbegin/rend for free.
// You can range-for, use std::sort, std::find_if, etc. directly.
static void section3_iterators() {
    std::cout << "============================================================\n";
    std::cout << " Section 3. Iterators\n";
    std::cout << "============================================================\n";

    int a[] = {5, 3, 1, 4, 2};
    std::span<int> s = a;

    std::cout << "  Iterator category: "
              << (std::is_same_v<
                      typename std::iterator_traits<
                          decltype(s.begin())>::iterator_category,
                      std::random_access_iterator_tag>
                  ? "random_access_iterator_tag"
                  : "(other)")
              << "\n";

    // Range-for uses begin()/end() under the hood.
    std::cout << "  range-for (before): ";
    for (int x : s) std::cout << x << " ";
    std::cout << "\n";

    // Use std::sort from <algorithm> directly on a span — it's just a
    // contiguous range.
    std::sort(s.begin(), s.end());
    std::cout << "  after std::sort:    ";
    for (int x : s) std::cout << x << " ";
    std::cout << "\n";

    // Reverse iterators work too.
    std::cout << "  rbegin..rend:       ";
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << "\n";

    // .begin() and .end() are random-access iterators one size_t apart. For a
    // contiguous_range span this is guaranteed by the standard. We avoid
    // constructing iterators from raw T* because libc++'s __wrap_iter has
    // a private ctor for non-friends; instead we just compute the distance
    // between begin and end and compare it to size().
    assert(s.end() - s.begin() == static_cast<std::ptrdiff_t>(s.size()));
    // begin() must equal what std::begin() of a vector (or C array) would
    // give for the same range. We rebuild from the original array as
    // ground truth and compare distances.
    int ground[] = {5, 3, 1, 4, 2};
    int* ground_truth_begin = std::begin(ground);
    int* ground_truth_end   = std::end(ground);
    assert((s.end() - s.begin()) == (ground_truth_end - ground_truth_begin));
    std::cout << "  [check] end-begin == size, and matches std::begin/end on C-array\n";

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 4. Function parameters — the decay idiom.
// ----------------------------------------------------------------------------
// A function taking std::span<T> accepts anything that decays to a
// contiguous range of T: C-arrays, std::array, std::vector, std::span.
// This is the practical reason span exists.
static void sum_and_print(std::span<const int> s, const char* tag) {
    long sum = 0;
    for (int x : s) sum += x;
    std::cout << "  [" << tag << "] size=" << s.size() << " sum=" << sum << "\n";
}

static void section4_function_params() {
    std::cout << "============================================================\n";
    std::cout << " Section 4. Function parameters — the decay idiom\n";
    std::cout << "============================================================\n";

    int carray[] = {1, 2, 3};
    std::array<int, 4> stdarr = {10, 20, 30, 40};
    std::vector<int> vec = {100, 200};
    std::span<int> sp = carray;   // span<int> -> span<const int> implicit

    sum_and_print(carray, "C-array");        // int[3] -> span<const int>
    sum_and_print(stdarr,  "std::array");    // std::array<int,4> -> span
    sum_and_print(vec,     "std::vector");   // std::vector<int> -> span
    sum_and_print(sp,      "std::span");     // span<int> -> span<const int>

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 5. Subviews — .first(), .last(), .subspan().
// ----------------------------------------------------------------------------
// These return NEW spans (still views, still no allocation). They're
// the tool for "I want to look at just the first N of this range" without
// copying.
static void section5_subviews() {
    std::cout << "============================================================\n";
    std::cout << " Section 5. Subviews: .first() / .last() / .subspan()\n";
    std::cout << "============================================================\n";

    int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::span<int> s = a;

    print_ints(s.first(3),   "first(3)");   // {0,1,2}
    print_ints(s.last(2),    "last(2)");    // {8,9}
    print_ints(s.subspan(2, 4), "subspan(2,4)");   // {2,3,4,5}
    print_ints(s.subspan(5),    "subspan(5)");     // {5,6,7,8,9}

    // Subviews compose. .first(n).last(m) is a valid expression chain.
    print_ints(s.first(5).last(2), "first(5).last(2)");   // {3,4}
    print_ints(s.last(5).first(2), "last(5).first(2)");   // {5,6}

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 6. Static vs dynamic extent — what the second template parameter
// actually does.
// ----------------------------------------------------------------------------
// std::span<T>            == std::span<T, std::dynamic_extent>
// std::span<T, N>          fixed extent at compile time, where N >= 0
// std::dynamic_extent      the sentinel for "runtime-sized"
//
// The fixed-extent version is smaller at runtime (the size is encoded in
// the type, so no size_ field is needed in the object) and offers more
// type-checking: a std::span<int,5> can't be silently used where a
// std::span<int,7> is expected.
static void section6_extents() {
    std::cout << "============================================================\n";
    std::cout << " Section 6. Static vs dynamic extent\n";
    std::cout << "============================================================\n";

    std::cout << "  std::dynamic_extent = " << std::dynamic_extent << "\n";

    using DynSpan  = std::span<int>;
    using Fixed3   = std::span<int, 3>;
    using Fixed5   = std::span<int, 5>;

    std::cout << "  sizeof(std::span<int>)            = " << sizeof(DynSpan)
              << "  (pointer + size_t = " << (sizeof(void*) + sizeof(std::size_t))
              << " on 64-bit)\n";
    std::cout << "  sizeof(std::span<int, 3>)         = " << sizeof(Fixed3)
              << "  (pointer only when extent is static)\n";

    static_assert(std::is_same_v<DynSpan, std::span<int, std::dynamic_extent>>);

    int a[5] = {1, 2, 3, 4, 5};
    Fixed5 fixed{a};
    std::cout << "  Fixed5 size() = " << fixed.size()
              << "  (compile-time, no runtime cost)\n";

    // Fixed -> dynamic conversion is implicit (widening).
    DynSpan widened = fixed;
    std::cout << "  Fixed5 -> std::span<int>: size=" << widened.size() << "\n";

    // Dynamic -> fixed is NOT implicit. You have to say "I know this is
    // actually 5" with std::span<T,5>(dyn.data(), 5), which is checked.
    DynSpan dyn{a};
    // auto wrong = std::span<int, 7>(dyn);  // would fail at runtime — size
                                            // doesn't match extent

    // .extent is a static constexpr member. It's std::dynamic_extent for
    // the dynamic case and the literal N for fixed.
    static_assert(Fixed5::extent == 5);
    static_assert(DynSpan::extent == std::dynamic_extent);
    std::cout << "  Fixed5::extent   = " << Fixed5::extent << "\n";
    std::cout << "  DynSpan::extent  = " << DynSpan::extent << "\n";

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 7. span<T> -> span<const T> — the const-narrowing conversion.
// ----------------------------------------------------------------------------
// This is one of std::span's quietly brilliant design choices. A function
// taking std::span<const T> can be called with a span<T> (read-only view
// of a writable buffer) without copying, without reinterpret_cast, and
// without an explicit conversion at the call site.
static void write_only(std::span<int> s, const char* tag) {
    std::cout << "  [" << tag << "] (mutable) first=" << s.front() << "\n";
    s[0] = 999;   // would compile because s is span<int>, not span<const int>
}
static void read_only(std::span<const int> s, const char* tag) {
    std::cout << "  [" << tag << "] (read-only) first=" << s.front() << "\n";
    // s[0] = 1;   // would NOT compile — span<const int> is immutable
}

static void section7_const_conversion() {
    std::cout << "============================================================\n";
    std::cout << " Section 7. span<T> -> span<const T> conversion\n";
    std::cout << "============================================================\n";

    int a[] = {1, 2, 3, 4, 5};
    std::span<int> mutable_view = a;

    read_only(mutable_view, "span<int> -> span<const int>");

    // Same buffer, mutable view, write through it.
    write_only(mutable_view, "span<int>");
    read_only(mutable_view, "after mutation");

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// Section 8. Side-by-side vs the hand-rolled psp::Span from Jun 14.
// ----------------------------------------------------------------------------
// Quick recap of where the two diverge. Both are non-owning views over a
// contiguous range; the differences are mostly API surface.
static void section8_vs_psp_span() {
    std::cout << "============================================================\n";
    std::cout << " Section 8. std::span vs psp::Span (Jun 14 hand-rolled)\n";
    std::cout << "============================================================\n";

    std::cout << "  Same:\n";
    std::cout << "    - non-owning view (pointer + size)\n";
    std::cout << "    - T* + Extent template shape (dynamic_extent sentinel)\n";
    std::cout << "    - random-access iterators\n";
    std::cout << "    - .first/.last/.subspan subview ops\n";
    std::cout << "    - construction from C-array, std::array, std::vector\n";
    std::cout << "    - implicit span<T> -> span<const T> conversion\n";

    std::cout << "  Differ:\n";
    std::cout << "    - psp::Span has .at(i) (throws); std::span does NOT.\n";
    std::cout << "    - psp::Span stores size_ even for static extent;\n";
    std::cout << "      std::span elides it (smaller fixed-extent objects).\n";
    std::cout << "    - psp::Span names the byte-size observer .byte_size();\n";
    std::cout << "      std::span uses .size_bytes().\n";
    std::cout << "    - std::span is in the standard library (the big one —\n";
    std::cout << "      you can use it across libraries without a third-\n";
    std::cout << "      party dependency).\n";

    std::cout << "\n";
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main() {
    section0_toolchain_check();
    section1_construction();
    section2_element_access();
    section3_iterators();
    section4_function_params();
    section5_subviews();
    section6_extents();
    section7_const_conversion();
    section8_vs_psp_span();

    std::cout << "============================================================\n";
    std::cout << " Done — std::span tour complete.\n";
    std::cout << "============================================================\n";
    return 0;
}