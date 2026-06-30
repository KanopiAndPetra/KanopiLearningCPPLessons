// span_consumer_c.cpp — second consumer TU that suppresses
// psp::Span<double>. Same shape as span_consumer_b.cpp but for
// the double specialization. Today's `psp_span_lib.a` does carry
// the explicit instantiation for psp::Span<double> (line ~120 of
// psp_span_lib/src/psp_span_inst.cpp), so this TU's weak
// references resolve correctly at link time.
//
// IMPORTANT (gotcha!): the `extern template class psp::Span<double>;`
// below suppresses Span<double>, NOT Span<const double>. They are
// DIFFERENT specializations — `T` vs `const T` matters at the
// class level. The Jun 30 driver for section [C] uses
// `psp::Span<double>` (the non-const version) because that's
// what was suppressed. If we instead passed `psp::Span<const double>`,
// the linker would look for explicit instantiations of
// `Span<const double>` in the library, and they're NOT there
// today — link would fail. The fix is to add
// `template class psp::Span<const double>;` to the explicit-
// instantiation TU. This is the contract: the library's
// instantiation list must contain every specialization referenced
// in consumer `extern template` lines.

#include "psp_span/span.h"

// Suppress strong definitions of psp::Span<double> in this TU.
extern template class psp::Span<double>;

#include <array>
#include <iostream>

namespace c_section {

double dot_product(psp::Span<double> a, psp::Span<double> b) {
    double total = 0.0;
    const std::size_t n = a.size();
    for (std::size_t i = 0; i < n; ++i) {
        total += a[i] * b[i];
    }
    return total;
}

void run() {
    std::array<double, 4> ax{1.0, 2.0, 3.0, 4.0};
    std::array<double, 4> bx{5.0, 6.0, 7.0, 8.0};

    // Span<double> from std::array<double, N>& (non-const).
    psp::Span<double> sa(ax);
    psp::Span<double> sb(bx);

    std::cout << "[C] span_consumer_c.cpp (with extern template on\n";
    std::cout << "    psp::Span<double>)\n";
    std::cout << "    a =";
    for (double x : sa) std::cout << ' ' << x;
    std::cout << "\n    b =";
    for (double x : sb) std::cout << ' ' << x;
    std::cout << "\n    dot_product = " << dot_product(sa, sb) << '\n';
    std::cout << "    size() == " << sa.size() << "\n\n";
}

}  // namespace c_section
