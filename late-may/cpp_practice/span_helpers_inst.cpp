// span_helpers_inst.cpp — the single source of truth for the
// psp::Span<int> and psp::Span<const int> instantiations used in
// this lesson.
//
// `extern template` (used in span_helpers_b.cpp / _c.cpp) says
// "don't instantiate here; the linker will find it elsewhere."
// This .cpp *is* the elsewhere. Without this file, the linker
// would emit "undefined symbols" for every suppressed Span<int>
// member function used in the other TUs.
//
// Two declarations — one for Span<int> and one for Span<const int>
// — give us explicit instantiation of the entire class. The
// compiler emits every member function (data, size, operator[],
// begin, end, etc.) in *this* .o's .text section. The linker then
// uses this .o to satisfy every reference to psp::Span<int>::*
// emitted by the other TUs.
//
// Pattern: this is the exact mechanism libstdc++ and libc++ use
// for std::string, std::vector, std::cout, etc. The headers say
// `extern template class std::basic_string<char>;` and the
// standard library .cpp files provide the explicit instantiations.
// We do the same here, in our own code.

#include "psp_span/span.h"

// --- Explicit instantiation: psp::Span<int> ---------------------------
// `template class psp::Span<int>;` (without `extern`) tells the
// compiler: "Emit definitions for every member function of
// psp::Span<int> in this translation unit." This is the opposite
// of `extern template`, which suppresses emission.
template class psp::Span<int>;
template class psp::Span<const int>;
// ----------------------------------------------------------------------

// Anchor symbol so the linker doesn't optimize this TU away when
// nothing directly references its instantiations. (Each consumer TU
// emits weak references to psp::Span<int>::*, which the linker
// resolves to the strong definition here; without anything in this
// file itself referencing the Span members, an aggressive linker
// might decide "nothing depends on this .o" and drop it. The
// volatile sink prevents that.)
namespace {
volatile int anchor = 0;
}

// Touch the Span<int> API in a way that survives link-time
// elimination, so `nm` shows the symbols are present in this .o
// even if the linker is configured to GC unreferenced sections.
void touch_anchor() {
    int buffer[1] = {anchor};
    psp::Span<int> s(buffer, 1);
    if (!s.empty()) {
        s[0] = static_cast<int>(s.size());  // size() == 1
    }
    anchor = buffer[0];
}