// span_helpers_c.cpp — defines count_nonzero WITH `extern template`.
//
// Identical pattern to span_helpers_b.cpp: tell the compiler "I know
// psp::Span<const int> is instantiated somewhere; don't duplicate
// the instantiations here."
//
// The lesson compares the size of this .o against span_helpers_a.o
// to show that the suppression measurably reduces per-TU code size.

#include "span_helpers.h"

extern template class psp::Span<int>;
extern template class psp::Span<const int>;

namespace petra {

std::size_t count_nonzero(psp::Span<const int> s) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != 0) ++n;
    }
    return n;
}

}  // namespace petra