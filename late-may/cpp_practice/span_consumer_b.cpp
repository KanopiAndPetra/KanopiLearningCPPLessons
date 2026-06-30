// span_consumer_b.cpp — a consumer TU that suppresses the
// psp::Span<const int> instantiation.
//
// This is the consumer-side `extern template` pattern from the
// Jun 29 lesson. What's NEW today (Jun 30) is that the matching
// explicit instantiation does NOT live in a separate "inst" file
// the consumer has to manage — it lives inside psp_span_lib.a.
// All this TU does is:
//
//   1. Opt out of generating strong definitions for
//      psp::Span<const int>'s member functions in this TU
//      (`extern template class psp::Span<const int>;`).
//
//   2. Use the API anyway. Each call site becomes a weak
//      undefined reference in span_consumer_b.o. The linker
//      pulls in libpsp_span_lib.a and resolves them to the strong
//      definitions of psp::Span<const int> inside that archive.
//
// The compile-time benefit (the *only* benefit of `extern
// template`, since link time is the same) is visible as a smaller
// .text section in span_consumer_b.o vs. an unsuppressed
// equivalent. The Jun 29 lesson measured this empirically with
// `nm`/`size`; today, the same numbers would show up for this
// file relative to a hypothetical `without_extern` variant.

#include "psp_span/span.h"

// Step 1: opt out. Without this line, the compiler would emit
// strong definitions of psp::Span<const int>::size(),
// ::operator[](), etc. into span_consumer_b.o. With it, the
// compiler emits *weak undefined references* (`U` in nm) instead.
extern template class psp::Span<const int>;

#include <iostream>
#include <vector>

namespace b_section {

// A small helper that exercises Span<const int> methods. The
// definitions for `size()`, `operator[]`, `front()`, `back()` are
// resolved by the linker from psp_span_lib.a, not from this .o.
int count_positive(psp::Span<const int> s) {
    int n = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] > 0) ++n;
    }
    return n;
}

void run() {
    std::vector<int> v{-1, 2, -3, 4, -5, 6, -7, 8};
    // Span<const int> from std::vector<int>& uses the templated
    // ctor (Span.h line ~98). The ctor is INLINE in the header,
    // so it gets defined here even with `extern template` (inlines
    // are special). What is suppressed is the NON-inline members
    // like size() and operator[].
    psp::Span<const int> s(v);

    std::cout << "[B] span_consumer_b.cpp (with extern template on\n";
    std::cout << "    psp::Span<const int>)\n";
    std::cout << "    contents:";
    for (int x : s) std::cout << ' ' << x;
    std::cout << "\n";
    std::cout << "    count_positive = " << count_positive(s) << '\n';
    std::cout << "    sum = " << s.front() + s.back() << " (front+back)\n\n";
}

}  // namespace b_section
