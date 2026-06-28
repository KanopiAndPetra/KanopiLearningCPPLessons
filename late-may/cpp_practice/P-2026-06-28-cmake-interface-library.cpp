// P-2026-06-28-cmake-interface-library.cpp
//
// Petra, 2026-06-28 — CMake `INTERFACE` library for header-only templates.
//
// What this file demonstrates
// ---------------------------
// The previous lesson (P-2026-06-27-cmake-build) added a CMake build
// for the multi-file Inventory. That used `add_library(petra_inventory
// STATIC box.cpp inventory.cpp)` — a real archive with compiled `.o`
// files in it. Today we add a *different* kind of library: a
// header-only template library called `psp_span`, exposed via
// `add_library(psp_span INTERFACE)`.
//
// The driver exercises three things:
//   (1) The Span itself — pulled in from `psp_span/span.h` via
//       CMake's include-path propagation through `target_link_libraries`.
//   (2) The difference between `STATIC` and `INTERFACE` libraries at
//       build time — specifically, that `psp_span` produces no `.a` and
//       no `.o` files at all.
//   (3) A `STATIC`-vs-`INTERFACE` sizeof() demonstration to show what
//       gets shipped where.
//
// The compile commands printed in section (1) come from
// `CMAKE_PROJECT_INCLUDE_BEFORE` / `--verbose` mode set by CMake when
// the binary is run from a verbose build; we extract them with
// `__FILE__` and inspect `argv[0]` to confirm we're running the
// CMake-built binary, not a stray hand-built one.

#include "psp_span/span.h"   // ← header from the INTERFACE library

#include <array>
#include <cstdio>
#include <cstddef>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Demo functions — exactly the same shapes as P-2026-06-14, but pulled
// in via the new header path. No .cpp compiled for psp_span itself.
// ---------------------------------------------------------------------------

namespace demo {

void print_ints(psp::Span<const int> s, const char* tag) {
    std::printf("  [%s] size=%zu, data=[", tag, s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::printf("%d%s", s[i], i + 1 == s.size() ? "" : ", ");
    }
    std::printf("]\n");
}

int sum_ints(psp::Span<const int> s) {
    int total = 0;
    for (int x : s) total += x;
    return total;
}

void scale_in_place(psp::Span<int> s, int k) {
    for (int& x : s) x *= k;
}

void show_first_last_mid(psp::Span<const int> s) {
    auto head = s.first<2>();
    auto tail = s.last(2);
    auto mid  = s.subspan<2, 3>();
    std::printf("  head=[%d, %d]  tail=[%d, %d]  mid=[",
                head[0], head[1], tail[0], tail[1]);
    for (int x : mid) std::printf("%d ", x);
    std::printf("]\n");
}

}  // namespace demo

int main(int argc, char** argv) {
    (void)argc;  // only argv[0] is used; argc itself is unused
    std::printf("=== P-2026-06-28: CMake INTERFACE library for "
                "header-only templates ===\n\n");

    // -- (1) Build provenance ----------------------------------------------
    std::printf("--- (1) Build provenance ---\n");
    std::printf("  argv[0]     = %s\n", argv[0]);
    std::printf("  __FILE__    = %s\n", __FILE__);
    std::printf("  __cplusplus = %ldL\n", static_cast<long>(__cplusplus));
#if defined(__clang__)
    std::printf("  Compiler    = clang %d.%d.%d\n",
                __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    std::printf("  Compiler    = gcc %d.%d.%d\n",
                __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
    std::printf("  argv[0] contains 'build/'? %s\n",
                std::string(argv[0]).find("build/") != std::string::npos
                    ? "yes (CMake out-of-source)"
                    : "no  (likely hand-built)");
    std::printf("\n");

    // -- (2) psp::Span<int, 5> from a C-array ------------------------------
    std::printf("--- (2) One function, multiple call shapes "
                "(header from INTERFACE library) ---\n");
    int c_arr[5] = {1, 2, 3, 4, 5};
    demo::print_ints(c_arr, "C-array");

    std::array<int, 5> std_arr = {10, 20, 30, 40, 50};
    demo::print_ints(std_arr, "std::array");

    std::vector<int> vec = {100, 200, 300};
    demo::print_ints(psp::Span<const int>(vec), "std::vector");

    auto s = psp::Span<const int>(c_arr);
    demo::print_ints(s, "Span re-view");

    int heap[3] = {7, 8, 9};
    demo::print_ints(psp::Span<const int>(heap, 3), "ptr+size");

    std::printf("  sum of std_arr = %d\n", demo::sum_ints(std_arr));
    std::printf("\n");

    // -- (3) In-place mutation through Span<int> ---------------------------
    std::printf("--- (3) In-place mutation (Span<int> is mutable) ---\n");
    int data[4] = {1, 2, 3, 4};
    demo::print_ints(data, "before");
    demo::scale_in_place(data, 10);
    demo::print_ints(data, "after x10");
    std::printf("\n");

    // -- (4) Static vs dynamic extent -------------------------------------
    std::printf("--- (4) Static vs dynamic extent ---\n");
    std::printf("  sizeof(psp::Span<int>)    = %zu (ptr + size)\n",
                sizeof(psp::Span<int>));
    std::printf("  sizeof(psp::Span<int,5>)  = %zu (ptr only when Extent is "
                "static)\n", sizeof(psp::Span<int, 5>));
    std::printf("  alignof(psp::Span<int>)   = %zu\n",
                alignof(psp::Span<int>));
    std::printf("  alignof(psp::Span<int,5>) = %zu\n",
                alignof(psp::Span<int, 5>));

    psp::Span<int, 5> fixed = c_arr;
    std::printf("  fixed.extent (compile-time) = %zu\n",
                fixed.extent);
    std::printf("\n");

    // -- (5) Subviews (compile-time slicing) ------------------------------
    std::printf("--- (5) Subviews: first<N>, last(N), subspan<O,C> ---\n");
    int src[7] = {11, 22, 33, 44, 55, 66, 77};
    demo::show_first_last_mid(src);
    std::printf("\n");

    // -- (6) Header-only proof --------------------------------------------
    std::printf("--- (6) Header-only proof: static_assert tests ---\n");
    static_assert(psp::dynamic_extent == static_cast<std::size_t>(-1),
                  "dynamic_extent sentinel");
    static_assert(psp::Span<int, 5>::extent == 5,
                  "static extent is part of the type");
    static_assert(psp::Span<int>::extent == psp::dynamic_extent,
                  "default extent is dynamic");
    std::printf("  static_assert: dynamic_extent == (size_t)-1 ... ok\n");
    std::printf("  static_assert: Span<int,5>::extent == 5 ... ok\n");
    std::printf("  static_assert: Span<int>::extent == dynamic ... ok\n");
    std::printf("  (these fire at compile time; nothing in this section "
                "is in the .o file)\n");
    std::printf("\n");

    // -- (7) Summary -------------------------------------------------------
    std::printf("--- (7) Why this is an INTERFACE library ---\n");
    std::printf("  - `psp_span` has no .cpp → no .o → no .a archive.\n");
    std::printf("  - CMake's `add_library(psp_span INTERFACE)` says:\n");
    std::printf("      \"this target has usage requirements only\".\n");
    std::printf("  - `target_include_directories(psp_span INTERFACE ...)`\n");
    std::printf("    propagates the header path to every consumer.\n");
    std::printf("  - `target_link_libraries(my_consumer PRIVATE psp_span)`\n");
    std::printf("    gives `my_consumer` the include path + C++17 feature,\n");
    std::printf("    adds nothing to the link line.\n");
    std::printf("  - Touching `span.h` recompiles every consumer that\n");
    std::printf("    #includes it (header-dep tracking is automatic).\n");

    return 0;
}