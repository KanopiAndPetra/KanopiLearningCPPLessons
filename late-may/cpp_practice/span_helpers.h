// span_helpers.h — Jun 29 lesson header.
//
// Three helper function declarations, each of which uses psp::Span<int>
// (or psp::Span<const int>) internally. They're defined in three
// separate .cpp translation units so the driver can demonstrate that
// `extern template` suppresses redundant instantiation in some TUs
// while one designated TU provides the explicit instantiation.
//
// The point of the lesson: psp_span stays a true header-only INTERFACE
// library (the Jun 28 architecture is preserved). The `extern template`
// optimization happens entirely on the consumer side. This is the same
// pattern the C++ standard library uses: <vector> says
// `extern template class std::vector<...>;` and libstdc++/libc++
// provide the explicit instantiations in their own .cpp files. We do
// the same here, in user code.

#ifndef PETRA_2026_06_29_SPAN_HELPERS_H
#define PETRA_2026_06_29_SPAN_HELPERS_H

#include <cstddef>

#include "psp_span/span.h"

namespace petra {

// Sum a Span of ints. Defined in span_helpers_a.cpp (no extern template).
long long sum_of_ints(psp::Span<const int> s);

// Fill a Span with 0, 1, 2, ..., n-1.
// Defined in span_helpers_b.cpp (with extern template — instantiation
// suppressed in this TU; resolved at link time to the explicit
// instantiation in span_helpers_inst.cpp).
void fill_ascending(psp::Span<int> s);

// Count the non-zero elements. Defined in span_helpers_c.cpp
// (also with extern template — same suppression pattern).
std::size_t count_nonzero(psp::Span<const int> s);

}  // namespace petra

#endif  // PETRA_2026_06_29_SPAN_HELPERS_H