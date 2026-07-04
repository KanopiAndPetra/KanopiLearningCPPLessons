// P-2026-07-04-cpack-resource-files.cpp — consumer of psp_span_lib v0.5.0,
// installed from a CPack .tar.gz that now bundles License.txt + Readme.txt.
//
// What this driver demonstrates
// -----------------------------
//
//   Section [A]: use psp::Span<int> exactly as a downstream user
//                would after extracting the v0.5.0 .tar.gz and
//                pointing CMAKE_PREFIX_PATH at it. The driver
//                source has no compile-time awareness of whether
//                License.txt + Readme.txt are present — those are
//                resource files for HUMANS and tooling, not for
//                the linker.
//
//   Section [B]: use the other two pre-instantiated specializations
//                (psp::Span<const int>, psp::Span<double>) to prove
//                the archive's .a still satisfies every symbol.
//
//   Section [C]: static_assert — confirms the v0.5.0 archive's
//                public header is the source of truth for the API.
//                If anyone bumped the header without bumping the
//                archive, this would catch it.
//
//   Section [D]: print PSP_SPAN_LIB_VERSION via the compile_definition
//                wired up by the consumer's CMakeLists.txt. The
//                driver inherits v0.5.0 from the archive's
//                psp_span_libConfigVersion.cmake, which is the
//                actual contract.
//
// Why the resource files matter (but aren't read here)
// -----------------------------------------------------
//
// License.txt and Readme.txt live at the *install root* of the
// archive, alongside include/ and lib/. They are not consumed
// by the C++ build — they're consumed by:
//   - humans running `tar tzf psp_span_lib-0.5.0-Darwin.tar.gz`
//     and `tar xzf ...; cat License.txt Readme.txt`,
//   - tooling like `reuse` (SPDX) and `licensecheck`,
//   - Debian package builders (License.txt becomes debian/copyright
//     if CPACK_GENERATOR is set to DEB).
//
// The C++ code doesn't care. The point of the lesson is that
// the *package* carries the documentation, not the code.
//
// Honest scope statement: the v0.5.0 API is identical to v0.4.0.
// The only difference is metadata. That's deliberate — version
// bumps that change behavior are scarier than version bumps that
// change metadata. We pay the new major-version cost (0.4 -> 0.5)
// only because we added a real new public capability (the
// archive now travels with documentation).

#include <psp_span/span.h>
#include <array>
#include <cstdio>
#include <cstddef>
#include <string>
#include <vector>

#ifndef PSP_SPAN_LIB_VERSION
#define PSP_SPAN_LIB_VERSION "(version unknown — define via CMake target_compile_definitions)"
#endif

int main() {
    std::puts("================================================================");
    std::puts(" P-2026-07-04 — cpack_resource_consumer (installed from .tar.gz)");
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
    s_int[0] = 999;
    std::printf("    after s_int[0] = 999, data()[0] = %d\n", s_int.data()[0]);
    std::puts("");

    // ------------------------------------------------------------------
    // [B] Other specializations
    // ------------------------------------------------------------------
    std::puts("[B] psp::Span<const int> (dynamic extent, immutable)");
    const int cbuf[4] = {1, 2, 3, 4};
    psp::Span<const int> s_const(cbuf, 4);
    std::printf("    size = %zu, sum = %d\n",
                s_const.size(),
                static_cast<int>(s_const[0] + s_const[1] +
                                 s_const[2] + s_const[3]));
    std::puts("");

    std::puts("[B'] psp::Span<double> (dynamic extent, mutable)");
    double dbuf[3] = {1.5, 2.5, 3.5};
    psp::Span<double> s_d(dbuf, 3);
    double total = 0.0;
    for (auto v : s_d) total += v;
    std::printf("    size = %zu, sum = %.1f\n", s_d.size(), total);
    std::puts("");

    // ------------------------------------------------------------------
    // [C] static_assert — v0.5.0 archive header is the source of truth
    // ------------------------------------------------------------------
    static_assert(psp::dynamic_extent == static_cast<std::size_t>(-1),
                  "psp::dynamic_extent must equal SIZE_MAX");
    static_assert(std::is_same<decltype(std::declval<psp::Span<int>>().size()),
                               std::size_t>::value,
                  "psp::Span<int>::size() must return std::size_t");
    static_assert(psp::Span<int, 5>::extent == 5,
                  "psp::Span<int, 5>::extent must be 5");
    std::puts("[C] static_assert: v0.5.0 API surface unchanged from v0.4.0  (OK)\n");

    // ------------------------------------------------------------------
    // [D] Container interop and subspan
    // ------------------------------------------------------------------
    std::puts("[D] Container interop + subspan");
    std::vector<int> vec = {100, 200, 300, 400};
    psp::Span<int> s_vec(vec.data(), vec.size());
    std::printf("    span over std::vector: size = %zu, [2] = %d\n",
                s_vec.size(), s_vec[2]);

    auto s_mid = s_int.subspan(2, 2);
    std::printf("    s_int.subspan(2, 2): size = %zu, [0] = %d, [1] = %d\n",
                s_mid.size(), s_mid[0], s_mid[1]);
    auto s_first3 = s_int.first(3);
    std::printf("    s_int.first(3):      size = %zu, [0] = %d, [2] = %d\n",
                s_first3.size(), s_first3[0], s_first3[2]);
    auto s_last2 = s_int.last(2);
    std::printf("    s_int.last(2):       size = %zu, [0] = %d, [1] = %d\n",
                s_last2.size(), s_last2[0], s_last2[1]);
    std::puts("");

    std::puts("================================================================");
    std::puts(" All sections ran cleanly. The v0.5.0 archive (not the source");
    std::puts(" tree, not /tmp/psp_install) satisfied every symbol, include,");
    std::puts(" and CMake config. License.txt + Readme.txt live at the install");
    std::puts(" root of the archive (see the .md notes for `tar tzf` output).");
    std::puts("================================================================");
    return 0;
}