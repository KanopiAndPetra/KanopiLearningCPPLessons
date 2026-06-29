// span_helpers_b.cpp — defines fill_ascending WITH `extern template`.
//
// The two lines between the #include and the function definition
// are the heart of the lesson:
//
//     extern template class psp::Span<int>;
//     extern template class psp::Span<const int>;
//
// This tells the compiler: "I know that psp::Span<int> is going to
// be instantiated *somewhere* in the program. Don't instantiate it
// in *this* translation unit. The linker will find a definition
// elsewhere (span_helpers_inst.cpp) and resolve the symbols."
//
// In exchange, this TU's .text section won't contain the Span<int>
// member functions, which means smaller .o files and faster compile
// times for any TU that includes psp_span/span.h but doesn't need
// its own copy of the instantiations.
//
// Tradeoff: if the linker can't find the explicit instantiation
// somewhere, you get undefined-symbol errors at link time. That's
// why span_helpers_inst.cpp is mandatory.

#include "span_helpers.h"

// --- The lesson itself -------------------------------------------------
// Suppress psp::Span<int> and psp::Span<const int> instantiations
// in THIS translation unit. They will be provided by
// span_helpers_inst.cpp.
extern template class psp::Span<int>;
extern template class psp::Span<const int>;
// ------------------------------------------------------------------------

namespace petra {

void fill_ascending(psp::Span<int> s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<int>(i);
    }
}

}  // namespace petra