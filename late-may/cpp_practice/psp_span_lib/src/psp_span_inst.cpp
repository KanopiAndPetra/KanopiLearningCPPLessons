// psp_span_lib/src/psp_span_inst.cpp — the SINGLE SOURCE OF TRUTH
// for the explicit instantiations of psp::Span<T, Extent>.
//
// This translation unit exists to complement the `extern template`
// suppression that consumer code can optionally opt into. The
// pattern (Jun 29 had this on the *consumer* side; today it lives
// inside the *library* proper) is exactly what libstdc++ and libc++
// do for std::basic_string, std::vector, std::cout, etc.: the
// header offers `extern template class std::basic_string<char>;`
// and the standard library's own .cpp provides the strong
// definitions in libstdc++.a / libc++.a.
//
// Today we own the equivalent of libstdc++.a for psp::Span. The
// header-side `extern template` is *not* declared inside span.h
// itself (we don't want to dictate to consumers who might be using
// psp::Span<int, 7> with an extent we didn't pre-instantiate).
// Instead, each consumer that wants compile-time savings declares
// `extern template class psp::Span<int>;` in its own .cpp and
// relies on this TU to satisfy the linker.
//
// What is instantiated here:
//
//   psp::Span<int>            (dynamic extent)
//   psp::Span<const int>      (dynamic extent)
//
// Why these two? `int` and `const int` are the specializations
// that show up in ~every consumer (you read ints, you pass them
// around as const). `Span<double>` is also instantiated as an
// example of extending the library — consumers can add their own
// `extern template` lines and we'll add the matching explicit
// instantiation here as the project grows.

// One header, all symbols. The header's contents are templates,
// so the explicit instantiations below are well-formed.
#include "psp_span/span.h"

// ---------------------------------------------------------------------------
// Explicit instantiation declarations.
//
// `template class psp::Span<int>;` (no `extern` keyword) tells the
// compiler: "Emit *definitions* for every member function of
// psp::Span<int> in this translation unit." That is the opposite
// of `extern template` (which suppresses emission).
//
// The resulting symbols are STRONG DEFINITIONS (`T` in `nm`'s
// output), and they live here — nowhere else in the program. The
// linker satisfies every reference to `psp::Span<int>::size()`,
// `psp::Span<int>::operator[]()`, etc. from this `.o`.
// ---------------------------------------------------------------------------
template class psp::Span<int>;
template class psp::Span<const int>;
// Bonus: instantiated for completeness / to demonstrate the
// pattern. The driver exercises Span<double> through it.
template class psp::Span<double>;
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Anchor symbols.
//
// `extern template` references appear in consumer TUs as WEAK
// UNDEFINED symbols (`U` in `nm`). The linker pulls in libpsp_span_lib.a
// and uses our strong definitions to resolve them. That's the happy path.
//
// The unhappy path is when nothing in *this* TU itself *uses* the
// instantiated methods. The compiler and linker are free to observe
// "this .cpp never references psp::Span<int>::size()" and GC the
// .o's entire .text section (or the whole .a archive) before the
// linker even sees consumer code. That's called "dead-code
// elimination" and is on by default at -O1 and higher.
//
// We defeat GC by:
//   (a) declaring a non-inline, non-constexpr function (`keep_*`)
//       that *uses* the Span API, and
//   (b) forcing the compiler to emit it by giving the symbol
//       external linkage and calling it from a constructor on a
//       translation-unit-local global.
//
// This is the same pattern the Jun 29 lesson used (`span_helpers_inst.cpp`'s
// `touch_anchor`). Here it lives in the library, which is the right
// home for it: every consumer that links against psp_span_lib gets
// the anchor for free, no per-consumer bookkeeping.
// ---------------------------------------------------------------------------
namespace psp_span_lib {
namespace {

// Volatile storage. `volatile` forces the compiler to actually
// read/write the variable, defeating any "this anchor is dead
// code" optimization. Without volatile, `g_anchor_read = g_anchor;`
// might be eliminated at -O1 as a no-op.
volatile int    g_anchor_int    = 0;
volatile double g_anchor_double = 0.0;

// keep_int: a function that uses psp::Span<int> methods. It is not
// inline, not constexpr, has external (well, internal — anonymous
// namespace) linkage. The compiler must emit a definition.
//
// The function body is intentionally "useless in spirit" — we're
// just exercising the Span<int> API so that:
//   (a) `psp::Span<int>::Span(int*, unsigned long)` gets a strong
//       definition here (used by the explicit instantiation above
//       anyway, but this provides redundant insistence).
//   (b) `size()`, `operator[]()`, `empty()`, `at()`, `data()` get
//       strong definitions in this .o even if a linker decided to
//       GC sections.
//   (c) The compiler cannot fold these calls into nothingness —
//       at() throws on OOB but we never actually trigger that,
//       so no UB / no exception escapes.
void keep_span_int() {
    int buffer[4] = {10, 20, 30, 40};
    psp::Span<int> s(buffer, 4);
    if (!s.empty()) {
        const std::size_t n = s.size();
        g_anchor_int = static_cast<int>(s[0] + s[n - 1]);   // 10 + 40 = 50
        try {
            g_anchor_int += s.at(2);                        // 30
        } catch (...) {
            g_anchor_int = -1;                             // never triggered
        }
    }
}

void keep_span_const_int() {
    const int buffer[3] = {1, 2, 3};
    psp::Span<const int> s(buffer, 3);
    g_anchor_int += static_cast<int>(s.front() + s.back()); // 1 + 3 = 4
    g_anchor_int += static_cast<int>(s[1]);                // +2
}

void keep_span_double() {
    double buffer[2] = {1.5, 2.5};
    psp::Span<double> s(buffer, 2);
    g_anchor_double = s[0] + s[1];                          // 4.0
}

// A "constructor" function the linker MUST call. By giving the
// global `keep_anchor` a non-trivial initializer, we force the
// linker to emit calls to keep_span_* in any executable that
// uses libpsp_span_lib.a. In practice, even without this, the
// strong symbols above survive GC because they're explicit
// instantiations — but the anchor is belt-and-suspenders.
//
// `__attribute__((constructor))` is GCC/Clang-only. We're on
// macOS with clang, so this works. On MSVC we'd use
// `#pragma section(".CRT$XCU", read)` and an init function.
// We don't build psp_span_lib on MSVC today; the attribute is
// acceptable.
[[maybe_unused]] int keep_anchor = []() -> int {
    keep_span_int();
    keep_span_const_int();
    keep_span_double();
    return 0;
}();

}  // namespace
}  // namespace psp_span_lib
// ---------------------------------------------------------------------------
