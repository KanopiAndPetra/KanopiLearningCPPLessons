// span_helpers_a.cpp — defines sum_of_ints without `extern template`.
//
// This is the "naive" translation unit: it includes psp_span/span.h
// and writes a Span-based function. The compiler, on seeing
// psp::Span<const int>::begin() / end() / operator[] / size() etc.
// being used, instantiates those member functions in *this* .o's
// .text section. Standard header-only-template behavior.
//
// The Jun 29 lesson compares the .text size of this .o against
// span_helpers_b.o and span_helpers_c.o (which DO use `extern
// template`) to show how much code is generated per TU.

#include "span_helpers.h"

namespace petra {

long long sum_of_ints(psp::Span<const int> s) {
    long long total = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        total += s[i];
    }
    return total;
}

}  // namespace petra