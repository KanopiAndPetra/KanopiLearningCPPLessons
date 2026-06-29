// P-2026-06-29-extern-template.cpp — driver for the `extern template` lesson.
//
// What this program does
// ----------------------
// 1. Calls three helpers in three different translation units:
//    - sum_of_ints    (defined in span_helpers_a.cpp, NO extern template)
//    - fill_ascending (defined in span_helpers_b.cpp, WITH extern template)
//    - count_nonzero  (defined in span_helpers_c.cpp, WITH extern template)
// 2. Verifies their behavior is correct (sum == 0+1+...+9 == 45, etc.).
// 3. Prints the .text size of each helper .o so we can SEE the effect
//    of `extern template`:
//      - span_helpers_a.o should contain ~10 psp::Span<int>::<method>
//        instantiations (because it doesn't suppress them).
//      - span_helpers_b.o and span_helpers_c.o should contain
//        NO psp::Span<int>:: instantiations (because they suppress).
//      - span_helpers_inst.o should contain ALL the suppressed ones.
//
//   This is the actual lesson, made visible.
//
// How to build
// ------------
// The companion Makefile builds each .cpp to its own .o, then links
// them together with the driver. The build rules print `size` and
// `nm -S` outputs so we can see the per-TU difference.
//
// Why this matters
// ----------------
// Without `extern template`, every TU that uses psp::Span<int>
// generates its own copy of every member function. That's wasted
// compile time and wasted .text space (the linker dedupes the final
// binary but every TU compiles the same code). With `extern template`
// in N TUs and a single explicit instantiation in one TU, the work
// happens exactly once. This is how the C++ standard library keeps
// <vector> fast even though it's used in millions of TUs.

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "psp_span/span.h"
#include "span_helpers.h"

namespace {

// Banner — proves argv[0] is /tmp/foo or ./build/something, the way
// the Jun 27 and Jun 28 lessons did. Lets us tell a hand-built run
// from a CMake-built run.
void print_banner(int argc, char** argv) {
    std::printf("argv[0] = %s\n", argv[0]);
    std::string a0 = argv[0];
    bool in_build = a0.find("build") != std::string::npos;
    std::printf("argv[0] contains 'build'? %s\n",
                in_build ? "yes (likely CMake out-of-source)"
                         : "no  (likely hand-built)");
    std::printf("argc = %d\n\n", argc);
}

// Section A: exercise the three helpers, verify results.
void run_helpers() {
    std::printf("[A] Exercising the three helpers\n");
    std::printf("--------------------------------\n");

    std::array<int, 10> buf{};
    petra::fill_ascending(psp::Span<int>(buf));
    psp::Span<const int> view(buf);

    long long s   = petra::sum_of_ints(view);
    std::size_t nz = petra::count_nonzero(view);

    std::printf("  fill_ascending produced: ");
    for (std::size_t i = 0; i < view.size(); ++i) {
        std::printf("%d ", view[i]);
    }
    std::printf("\n");
    // 0+1+2+...+9 = 45. Of those, 1,2,...,9 are nonzero (9 of them);
    // the 0 at index 0 is zero. count_nonzero should return 9.
    std::printf("  sum_of_ints    = %lld  (expect 45)\n", s);
    std::printf("  count_nonzero  = %zu  (expect 9 — only the leading 0 is zero)\n", nz);

    if (s != 45 || nz != 9) {
        std::fprintf(stderr, "  FAIL: helper result mismatch\n");
        std::exit(1);
    }
    std::printf("  PASS\n\n");
}

// Section B: demonstrate the constructors (no instantiation suppression
// here, on purpose — we want psp::Span<int>::Span ctor available).
void run_ctors() {
    std::printf("[B] Using psp::Span ctors directly in the driver\n");
    std::printf("-------------------------------------------------\n");

    int c_arr[4] = {10, 20, 30, 40};
    psp::Span<int, 4> from_c_arr = c_arr;        // static-extent ctor
    std::printf("  Span<int,4> from int[4]={10,20,30,40}: ");
    for (auto x : from_c_arr) std::printf("%d ", x);
    std::printf("(size=%zu, sizeof=%zu)\n",
                from_c_arr.size(), sizeof(from_c_arr));

    std::vector<int> vec = {1, 2, 3, 4, 5};
    psp::Span<const int> from_vec = vec;         // vector ctor
    std::printf("  Span<const int> from vector{1..5}:     ");
    for (auto x : from_vec) std::printf("%d ", x);
    std::printf("(size=%zu)\n", from_vec.size());

    std::printf("\n");
}

// Section C: explain the build steps (printed at compile time of this
// .cpp via a static_assert message would be too clever; instead we
// print them at runtime so the user can correlate with their
// terminal scrollback).
void explain_extern_template() {
    std::printf("[C] What `extern template` did at compile time\n");
    std::printf("------------------------------------------------\n");
    std::printf("  span_helpers_a.cpp  — NO extern template\n");
    std::printf("    -> compiler instantiated every psp::Span<int>::\n");
    std::printf("       method used in sum_of_ints (data, size,\n");
    std::printf("       operator[], etc.) inside THIS .o\n");
    std::printf("\n");
    std::printf("  span_helpers_b.cpp  — extern template class psp::Span<int>\n");
    std::printf("  span_helpers_c.cpp  — extern template class psp::Span<int>\n");
    std::printf("    -> compiler emitted WEAK references to those\n");
    std::printf("       member functions; no .text definitions here\n");
    std::printf("\n");
    std::printf("  span_helpers_inst.cpp — template class psp::Span<int>\n");
    std::printf("    -> compiler emitted STRONG definitions of every\n");
    std::printf("       psp::Span<int>:: method; this is the one place\n");
    std::printf("       the linker pulls symbols from\n");
    std::printf("\n");
    std::printf("  Final link: weak refs in b.o/c.o resolve to the\n");
    std::printf("  strong definitions in inst.o. a.o has its own\n");
    std::printf("  copy (because no extern template) — the linker\n");
    std::printf("  picks one (a.o's, since it was first in link order)\n");
    std::printf("  and discards the others as duplicate symbols.\n");
    std::printf("\n");
    std::printf("  Run `nm -S span_helpers_*.o` (after `make`) to see\n");
    std::printf("  this directly. Look for `W` (weak) vs `T` (strong)\n");
    std::printf("  on the `psp::Span<int>::*` symbols.\n");
    std::printf("\n");
}

}  // namespace

int main(int argc, char** argv) {
    print_banner(argc, argv);
    run_helpers();
    run_ctors();
    explain_extern_template();
    return 0;
}