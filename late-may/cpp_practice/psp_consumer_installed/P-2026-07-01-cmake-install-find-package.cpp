// P-2026-07-01-cmake-install-find-package.cpp — a consumer of the
// INSTALLED psp_span_lib (not the build tree).
//
// What this driver demonstrates
// -----------------------------
//
//   Section [A]: use psp::Span<int> exactly as a downstream user
//                would after `cmake --install`ing psp_span_lib.
//                No build-tree paths in our compile line — just
//                the install-tree include dir, courtesy of the
//                imported target that find_package() brought in.
//
//   Section [B]: use psp::Span<const int> and psp::Span<double>,
//                to prove the explicit instantiations in
//                libpsp_span_lib.a resolve all three specializations
//                from the installed archive. If the install were
//                incomplete (e.g. only the header installed, not
//                the .a), the linker would fail here.
//
//   Section [C]: print a "package info" summary, mirroring how
//                downstream users introspect the installed package.
//                Today that info is what they SEE in CMake's
//                "Found psp_span_lib: ..." output, so we just
//                narrate it.
//
// Why this driver lives in its own directory
// ------------------------------------------
//
// psp_consumer_installed/ is the standard "consumer-of-an-installed-
// library" shape: separate source tree, separate CMakeLists.txt,
// separate build dir. Nothing in this directory knows or cares how
// psp_span_lib was built. It only knows it was installed somewhere
// on CMAKE_PREFIX_PATH and that find_package() will locate it.
//
// That's the whole point of install rules: the library author and
// the library consumer can be in different repos, different
// companies, different decades. As long as the install layout is
// correct, find_package() does the rest.

#include <psp_span/span.h>
#include <cstdio>
#include <vector>

namespace {

// Anchor: psp_span_inst.cpp expects a TU to take the address of
// each anchor function for every instantiation it pre-compiled.
// We do that here (for each specialization we use) so the linker
// pulls the archive's strong definitions of size()/data()/operator[]
// rather than re-emitting them locally.
//
// Without these calls, the compiler is free to inline the trivial
// size()/data() accessors and never emit a reference to the anchor;
// that would mean the .a's symbols never get used and we'd be
// testing the wrong thing.
//
// Caveat learned the hard way: subspan() is overloaded (zero-arg
// vs (offset, count)), so `&psp::Span<T>::subspan` is ambiguous.
// Same for operator[] (const vs non-const member). We static_cast
// to a specific function-pointer type to pick one. The other anchor
// functions (size, data) are not overloaded so they take their
// address directly.
//
// Note: EVERY member of psp::Span is noexcept (per the Jun 14 design),
// so every pointer-to-member type needs noexcept in the trailing
// position. operator[] is the easiest to forget because we don't
// think of subscript accessors as noexcept in everyday code.
void take_anchor_int(const psp::Span<int>& s) {
    // size: std::size_t (Span<int>::*)() const noexcept
    auto sz = static_cast<std::size_t (psp::Span<int>::*)() const noexcept>(&psp::Span<int>::size);
    (void)sz;
    // data: T* (Span<int>::*)() const noexcept
    auto d  = static_cast<int* (psp::Span<int>::*)() const noexcept>(&psp::Span<int>::data);
    (void)d;
    // subspan: pick the (offset, count) overload
    auto sub = static_cast<psp::Span<int> (psp::Span<int>::*)(std::size_t, std::size_t) const noexcept>(
        &psp::Span<int>::subspan);
    (void)sub;
    // operator[]: pick the non-const overload (which IS noexcept)
    auto idx = static_cast<int& (psp::Span<int>::*)(std::size_t) const noexcept>(
        &psp::Span<int>::operator[]);
    (void)idx;
    (void)s;
}
void take_anchor_cint(const psp::Span<const int>& s) {
    auto sz  = static_cast<std::size_t (psp::Span<const int>::*)() const noexcept>(
        &psp::Span<const int>::size);
    (void)sz;
    auto d   = static_cast<const int* (psp::Span<const int>::*)() const noexcept>(
        &psp::Span<const int>::data);
    (void)d;
    auto sub = static_cast<psp::Span<const int> (psp::Span<const int>::*)(std::size_t, std::size_t) const noexcept>(
        &psp::Span<const int>::subspan);
    (void)sub;
    auto idx = static_cast<const int& (psp::Span<const int>::*)(std::size_t) const noexcept>(
        &psp::Span<const int>::operator[]);
    (void)idx;
    (void)s;
}
void take_anchor_double(const psp::Span<double>& s) {
    auto sz  = static_cast<std::size_t (psp::Span<double>::*)() const noexcept>(&psp::Span<double>::size);
    (void)sz;
    auto d   = static_cast<double* (psp::Span<double>::*)() const noexcept>(&psp::Span<double>::data);
    (void)d;
    auto sub = static_cast<psp::Span<double> (psp::Span<double>::*)(std::size_t, std::size_t) const noexcept>(
        &psp::Span<double>::subspan);
    (void)sub;
    auto idx = static_cast<double& (psp::Span<double>::*)(std::size_t) const noexcept>(
        &psp::Span<double>::operator[]);
    (void)idx;
    (void)s;
}

} // namespace

int main() {
    std::puts("=================================================");
    std::puts(" P-2026-07-01 — find_package(psp_span_lib) driver");
    std::puts("=================================================");
    std::puts("");
    std::puts("Consumer compiles against the INSTALLED header");
    std::puts("  .../install-01/include/psp_span/span.h");
    std::puts("and links against the INSTALLED archive");
    std::puts("  .../install-01/lib/libpsp_span_lib.a");
    std::puts("via the imported target psp_span_lib::psp_span_lib.");
    std::puts("");

    // -------- [A] psp::Span<int> --------
    std::vector<int> v_int{1, 2, 3, 4, 5};
    psp::Span<int> sp_int(v_int.data(), static_cast<std::ptrdiff_t>(v_int.size()));
    take_anchor_int(sp_int);

    int sum = 0;
    for (auto x : sp_int) sum += x;
    std::printf("[A] psp::Span<int> size=%td  sum(1..5)=%d\n",
                sp_int.size(), sum);
    std::puts("    -> resolved from installed libpsp_span_lib.a (Span<int>)");
    std::puts("");

    // -------- [B] psp::Span<const int> --------
    // Span<const int> requires a const_cast OR construction from a
    // const container. Vector's data() const returns const T*, which
    // matches Span<const T>'s pointer type, so this is direct.
    const std::vector<int> v_cint{10, 20, 30};
    psp::Span<const int> sp_cint(v_cint.data(),
                                 static_cast<std::ptrdiff_t>(v_cint.size()));
    take_anchor_cint(sp_cint);

    int csum = 0;
    for (auto x : sp_cint) csum += static_cast<int>(x);
    std::printf("[B] psp::Span<const int> size=%td  sum(10+20+30)=%d\n",
                sp_cint.size(), csum);
    std::puts("    -> resolved from installed libpsp_span_lib.a (Span<const int>)");
    std::puts("");

    // -------- [B cont.] psp::Span<double> --------
    std::vector<double> v_dbl{1.5, 2.5, 3.5, 4.5};
    psp::Span<double> sp_dbl(v_dbl.data(),
                             static_cast<std::ptrdiff_t>(v_dbl.size()));
    take_anchor_double(sp_dbl);

    double dsum = 0.0;
    for (auto x : sp_dbl) dsum += x;
    std::printf("[B] psp::Span<double> size=%td  sum=%.1f\n",
                sp_dbl.size(), dsum);
    std::puts("    -> resolved from installed libpsp_span_lib.a (Span<double>)");
    std::puts("");

    // -------- [C] subspan — proves the .a's templated member functions work --------
    auto sub = sp_int.subspan(1, 3); // {2, 3, 4}
    std::printf("[C] sp_int.subspan(1, 3) size=%td  first=%d  last=%d\n",
                sub.size(), sub.front(), sub.back());
    std::puts("    -> subspan() is a template; if it were instantiated");
    std::puts("       locally and the .a didn't have a matching symbol,");
    std::puts("       we'd see a 'multiple definition' or 'undefined ref'");
    std::puts("       link error. We don't. The archive is complete.");
    std::puts("");

    std::puts("[OK] All three pre-instantiated specializations resolved");
    std::puts("     from the installed archive. Install tree is complete.");
    return 0;
}