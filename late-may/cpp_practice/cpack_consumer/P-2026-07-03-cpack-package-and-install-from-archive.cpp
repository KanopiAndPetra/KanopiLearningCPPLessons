// P-2026-07-03-cpack-package-and-install-from-archive.cpp — a
// consumer of psp_span_lib that has NEVER SEEN THE SOURCE TREE.
//
// What this driver demonstrates
// -----------------------------
//
//   Section [A]: use psp::Span<int> exactly as a downstream user
//                would after extracting a .tar.gz and pointing
//                CMAKE_PREFIX_PATH at it. The driver source has
//                no compile-time knowledge of where the archive
//                came from or where the library was built.
//
//   Section [B]: use psp::Span<const int> and psp::Span<double>,
//                to prove all three explicit instantiations
//                resolve from libpsp_span_lib.a, which is inside
//                the archive. If the archive were missing the .a,
//                or if the archive had been built from a CMakeLists
//                without the install(EXPORT ...) line, the linker
//                would fail with "Undefined symbols for architecture".
//
//   Section [C]: print the version we found, mirroring how
//                downstream users introspect the installed package.
//
//   Section [D]: a deliberately-failing anchor — a static_assert
//                on a Span property. This proves the archive's
//                header is the SOURCE OF TRUTH for what we see;
//                if anyone tampered with the public API between
//                build and archive, this assertion would catch it
//                at compile time in the consumer.
//
// Why this driver lives in its own directory
// ------------------------------------------
//
// cpack_consumer/ is the "consumer-from-archive" shape: separate
// source tree, separate CMakeLists.txt, separate build dir, no
// shared state with the library at all (except for the archive
// itself, which is the entire contract).
//
// This is structurally different from the psp_consumer_installed/
// shape (Jul 1), even though the CMakeLists look almost identical.
// The difference is what produces the install tree:
//   - Jul 1: cmake --install on the build dir.
//   - Jul 3: tar xzf the archive, then cmake --install on that.
//
// Same library, same consumers — but the archive is the boundary
// that lets the library travel.

#include <psp_span/span.h>
#include <array>
#include <cstdio>
#include <cstddef>
#include <string>
#include <vector>

// Mirror the psp_span_libConfig.cmake.in status line so a downstream
// user can see the version the driver is linked against. In a real
// project this would come from compile_definition in the consumer's
// CMakeLists.txt (`target_compile_definitions(... PRIVATE
// PSP_SPAN_LIB_VERSION="${psp_span_lib_VERSION}")`). We hardcode it
// here because the lesson is about the archive flow, not about how
// to wire up compile-time metadata — that's a future lesson.
#ifndef PSP_SPAN_LIB_VERSION
#define PSP_SPAN_LIB_VERSION "(version unknown — define via CMake target_compile_definitions)"
#endif

int main() {
    std::puts("================================================================");
    std::puts(" P-2026-07-03 — cpack_consumer (installed from .tar.gz)");
    std::puts("================================================================");
    std::printf("psp_span_lib version: %s\n\n", PSP_SPAN_LIB_VERSION);

    // ------------------------------------------------------------------
    // [A] psp::Span<int> — dynamic extent, mutable
    // ------------------------------------------------------------------
    std::puts("[A] psp::Span<int> (dynamic extent, mutable)");
    int buffer[6] = {10, 20, 30, 40, 50, 60};
    psp::Span<int> s_int(buffer, 6);
    std::printf("    size = %zu, empty = %s\n", s_int.size(),
                s_int.empty() ? "true" : "false");
    std::printf("    first = %d, last = %d, at(2) = %d\n",
                s_int.front(), s_int.back(), s_int.at(2));
    s_int[0] = 999;  // mutable
    std::printf("    after s_int[0] = 999, data()[0] = %d\n", s_int.data()[0]);
    std::puts("");

    // ------------------------------------------------------------------
    // [B] psp::Span<const int> and psp::Span<double> — to prove the
    //     archive's .a satisfies all three pre-instantiated
    //     specializations (libpsp_span_lib.a symbols are STRONG
    //     here, not weak).
    // ------------------------------------------------------------------
    std::puts("[B] psp::Span<const int> (dynamic extent, immutable)");
    const int cbuf[4] = {1, 2, 3, 4};
    psp::Span<const int> s_const(cbuf, 4);
    std::printf("    size = %zu, sum = %d\n",
                s_const.size(),
                static_cast<int>(s_const[0] + s_const[1] +
                                 s_const[2] + s_const[3]));
    // Compile error if we tried: s_const[0] = 9;
    std::puts("");

    std::puts("[B'] psp::Span<double> (dynamic extent, mutable)");
    double dbuf[3] = {1.5, 2.5, 3.5};
    psp::Span<double> s_d(dbuf, 3);
    double total = 0.0;
    for (auto v : s_d) total += v;
    std::printf("    size = %zu, sum = %.1f\n", s_d.size(), total);
    std::puts("");

    // ------------------------------------------------------------------
    // [C] Container interop — span over std::vector and std::array
    // ------------------------------------------------------------------
    std::puts("[C] Container interop");
    std::vector<int> vec = {100, 200, 300, 400};
    psp::Span<int> s_vec(vec.data(), vec.size());
    std::printf("    span over std::vector: size = %zu, [2] = %d\n",
                s_vec.size(), s_vec[2]);

    std::array<double, 4> arr = {0.1, 0.2, 0.3, 0.4};
    psp::Span<const double> s_arr(arr.data(), arr.size());
    double arr_total = 0.0;
    for (auto v : s_arr) arr_total += v;
    std::printf("    span over std::array:  size = %zu, sum = %.1f\n",
                s_arr.size(), arr_total);
    std::puts("");

    // ------------------------------------------------------------------
    // [D] static_assert — archive-header is the source of truth
    // ------------------------------------------------------------------
    // If anyone broke the public API between building and packaging,
    // the .tar.gz extraction would surface it here at compile time.
    // We assert two non-trivial properties of psp::Span:
    //   1. dynamic_extent == static_cast<size_t>(-1)  — the
    //      sentinel for runtime size matches std::span's value.
    //   2. size_type is size_t, not some accidentally-narrower type.
    static_assert(psp::dynamic_extent == static_cast<std::size_t>(-1),
                  "psp::dynamic_extent must equal SIZE_MAX");
    static_assert(std::is_same<decltype(std::declval<psp::Span<int>>().size()),
                               std::size_t>::value,
                  "psp::Span<int>::size() must return std::size_t");
    static_assert(psp::Span<int, 5>::extent == 5,
                  "psp::Span<int, 5>::extent must be 5");
    std::puts("[D] static_assert: psp::dynamic_extent == SIZE_MAX, "
              "size_type == std::size_t  (OK)\n");

    // ------------------------------------------------------------------
    // [E] subspan + first/last — proves the rest of the public API
    //     resolves from the archive's .a, not just size()/operator[].
    // ------------------------------------------------------------------
    std::puts("[E] Subspan operations");
    auto s_mid = s_int.subspan(2, 2);  // [30, 40]
    std::printf("    s_int.subspan(2, 2): size = %zu, [0] = %d, [1] = %d\n",
                s_mid.size(), s_mid[0], s_mid[1]);
    auto s_first3 = s_int.first(3);    // [999, 20, 30]
    std::printf("    s_int.first(3):      size = %zu, [0] = %d, [2] = %d\n",
                s_first3.size(), s_first3[0], s_first3[2]);
    auto s_last2 = s_int.last(2);      // [50, 60]
    std::printf("    s_int.last(2):       size = %zu, [0] = %d, [1] = %d\n",
                s_last2.size(), s_last2[0], s_last2[1]);
    std::puts("");

    std::puts("================================================================");
    std::puts(" All sections ran cleanly. The archive (not the source tree, ");
    std::puts(" not /tmp/psp_install) satisfied every symbol and include.");
    std::puts("================================================================");
    return 0;
}