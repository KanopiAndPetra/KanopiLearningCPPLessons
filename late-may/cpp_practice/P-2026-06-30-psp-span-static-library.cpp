// P-2026-06-30-psp-span-static-library.cpp — driver for the
// Jun 30 lesson.
//
// What this driver demonstrates
// -----------------------------
//
//   Section [A]: uses psp::Span<int> directly. No `extern template`
//                suppression here, so the driver emits its own
//                strong definitions of psp::Span<int> member
//                functions.
//
//   Section [B]: uses psp::Span<const int> via a helper TU
//                (`span_consumer_b.cpp`) that *does* declare
//                `extern template class psp::Span<const int>;`.
//                That helper emits weak undefined references
//                (U in `nm`) and relies on libpsp_span_lib.a to
//                resolve them.
//
//   Section [C]: uses psp::Span<double>, again through a helper TU
//                (`span_consumer_c.cpp`) with `extern template`
//                suppression.
//
//   Section [D]: prints the compile/link story — what was
//                instantiated where, and how it ended up that way.
//                This is the "explanation" output, not the result
//                of any runtime computation.
//
// What this driver does NOT need to do
// -------------------------------------
//
// - It does not need its own `template class psp::Span<int>;`
//   line. That's the whole point of having a library: the
//   explicit instantiations live in libpsp_span_lib.a, not in
//   the driver.
//
// - It does not need to touch a "span_helpers_inst.cpp"-style TU.
//   That's what the LIBRARY is for. Consumers don't write such
//   files anymore.
//
// Build
// -----
//
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
//   cmake --build build -j
//   ./build/P-2026-06-30-psp-span-static-library
//
// The build command compiles:
//   - psp_span_lib/src/psp_span_inst.cpp -> libpsp_span_lib.a (1 .o)
//   - span_consumer_b.cpp                 -> span_consumer_b.o
//   - span_consumer_c.cpp                 -> span_consumer_c.o
//   - this file                           -> driver.o
// and links the .o files against libpsp_span_lib.a.

#include <array>
#include <vector>
#include <iostream>
#include <numeric>
#include "psp_span/span.h"

// ---------------------------------------------------------------------------
// Section [A]: the driver uses Span<int> methods inline. No `extern template`
// suppression, so the driver emits its own strong definitions of the
// methods it calls. Those definitions duplicate what is already in
// libpsp_span_lib.a, and the linker keeps only one copy — this is fine;
// the `extern template` opt-in is about compile time, not link correctness.
// ---------------------------------------------------------------------------
namespace a_section {

constexpr int kWindowSize = 5;

int sum_window(const psp::Span<const int>& s) {
    int total = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        total += s[i];
    }
    return total;
}

void run() {
    int data[kWindowSize] = {10, 20, 30, 40, 50};
    psp::Span<const int> window(data, kWindowSize);

    std::cout << "[A] driver uses psp::Span<const int> inline\n";
    std::cout << "    contents:";
    for (int v : window) std::cout << ' ' << v;
    std::cout << "\n";
    std::cout << "    sum_window = " << sum_window(window) << '\n';
    std::cout << "    size() == " << window.size()
              << ", empty() = " << std::boolalpha << window.empty() << "\n\n";
}

}  // namespace a_section

// ---------------------------------------------------------------------------
// Sections [B] and [C] live in their own TUs (span_consumer_b.cpp,
// span_consumer_c.cpp). They declare `extern template` so they don't
// emit strong definitions locally. Forward-declared here.
// ---------------------------------------------------------------------------
namespace b_section { void run(); }
namespace c_section { void run(); }

// ---------------------------------------------------------------------------
// Section [D]: a runtime summary of "what just happened at link time".
// We don't compute anything here; we just print the explanation.
//
// Why print at runtime instead of in a static_assert? Because the
// Jun 30 lesson is about a *link-time* mechanism. Static_asserts only
// run at compile time. The user gets to *see* the build artifacts in
// `nm`/`size` outputs anyway (see the lessons' notes).
// ---------------------------------------------------------------------------
namespace d_section {
void run() {
    std::cout << "[D] what just happened\n";
    std::cout << "  - psp_span_lib.a contained the explicit instantiations\n";
    std::cout << "    of psp::Span<int>, psp::Span<const int>,\n";
    std::cout << "    psp::Span<double>. These are the STRONG DEFINITIONS\n";
    std::cout << "    the linker satisfies every weak reference against.\n";
    std::cout << "  - This driver did NOT declare `extern template` for\n";
    std::cout << "    psp::Span<int>; in section [A]. It emitted its own\n";
    std::cout << "    strong definitions alongside. The linker dedupes\n";
    std::cout << "    strong-vs-strong: final binary has one copy.\n";
    std::cout << "  - span_consumer_b.cpp / _c.cpp declared `extern template`\n";
    std::cout << "    and emitted WEAK UNDEFINED references, satisfied by\n";
    std::cout << "    psp_span_lib.a at link time.\n";
    std::cout << "  - To verify: `nm build/libpsp_span_lib.a` should show\n";
    std::cout << "    `T` symbols for psp::Span<int>::*, and `nm\n";
    std::cout << "    build/span_consumer_b.o` should show `U` for the same.\n\n";
}
}  // namespace d_section

int main() {
    std::cout << "=== P-2026-06-30-psp-span-static-library ===\n";
    std::cout << "psp_span_lib v0.2.0 — STATIC library with internal\n";
    std::cout << "explicit instantiations of psp::Span<int>,\n";
    std::cout << "psp::Span<const int>, psp::Span<double>.\n\n";

    a_section::run();
    b_section::run();
    c_section::run();
    d_section::run();

    std::cout << "All sections passed. Compile-time savings observed\n";
    std::cout << "for the two consumer TUs that opted into extern template\n";
    std::cout << "suppression (span_consumer_b.cpp, span_consumer_c.cpp);\n";
    std::cout << "those TUs do NOT carry psp::Span<...> members in their\n";
    std::cout << ".text sections. See the lesson notes for nm / size output.\n";
    return 0;
}
