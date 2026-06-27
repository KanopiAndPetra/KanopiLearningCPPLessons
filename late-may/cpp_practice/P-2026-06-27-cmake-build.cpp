// P-2026-06-27-cmake-build.cpp
//
// Topic: CMake — replacing the Jun 16 Makefile with a real
//        cross-platform CMake build for the same multi-file
//        Inventory lesson.
//
// Build with CMake (preferred):
//   cmake -S . -B build          # configure (one-shot, out-of-source)
//   cmake --build build          # build
//   ./build/P-2026-06-27-cmake-build    # run
//
//   # cleanup
//   rm -rf build
//
// Build with the Jun 16 Makefile (also still works — this is the
// point of the lesson: both build systems co-exist):
//   make            # uses the Makefile that lives next to this
//   make run        # build + run
//   make clean      # rm -rf build/ and the binary
//
// What this file does
// -------------------
// The driver is the same Jun 9 / Jun 16 demo: build an Inventory of
// Boxes, exercise add / replace_at / take, print the trace. The
// *point* of today's lesson is not the C++ — it's the build system.
// Everything `box.h` / `box.cpp` / `inventory.h` / `inventory.cpp`
// already exist from Jun 16; we are reusing them unchanged.
//
// Why a separate driver file from `P-2026-06-16-inventory-multi-file.cpp`?
// Two reasons:
//   1. The binary is named after today's date, so the CMake
//      target can be `P-2026-06-27-cmake-build` — matching the
//      cron workflow's filename convention.
//   2. The driver is a thin wrapper around the same Box/Inventory
//      pair. It exists so the CMake-built binary prints something
//      visibly different from the Makefile-built binary: namely,
//      the absolute path of the executable (`__FILE__` and the
//      argv[0] path), which proves the out-of-source build worked.
//
// The trace output is byte-for-byte identical to Jun 16. The
// difference is where the binary lives on disk: build/ on disk
// for CMake, the source dir for `make`.

#include "box.h"
#include "inventory.h"
#include <cstdlib>
#include <iostream>
#include <string>

// Mini-helper: print a banner so the CMake-built binary's output
// is unambiguously labelled in the cron summary.
static void banner(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

int main(int argc, char** argv) {
    banner("(0) Build provenance");
    std::cout << "  argv[0] = " << (argc > 0 ? argv[0] : "<none>") << "\n";
    std::cout << "  __FILE__ = " << __FILE__ << "\n";
    std::cout << "  C++ standard: " << __cplusplus << "\n";
    std::cout << "  Compiler: " <<
#ifdef __clang__
        "clang " __clang_version__
#elif defined(__GNUC__)
        "gcc " __VERSION__
#else
        "unknown"
#endif
        << "\n";

    banner("(1) Build an Inventory of Boxes");
    Inventory inv;
    inv.add(Box("red"));
    inv.add(Box("green"));
    inv.add(Box("blue"));
    inv.print("after 3 adds");

    banner("(2) replace_at: pass-by-rvalue into vector slot");
    inv.replace_at(1, Box("YELLOW"));
    inv.print("after replace_at(1, YELLOW)");

    banner("(3) take: move a Box out of the middle");
    Box stolen = inv.take(1);
    std::cout << "  stolen label = '" << stolen.label() << "'\n";
    inv.print("after take(1)");

    banner("(4) take: move the first one too");
    Box first = inv.take(0);
    std::cout << "  first label = '" << first.label() << "'\n";
    inv.print("after take(0)");

    banner("(5) Move the Inventory itself (NRVO / move-ctor)");
    Inventory moved = std::move(inv);
    std::cout << "  moved.size() = " << moved.size() << "\n";
    std::cout << "  src.size()   = " << inv.size() << " (moved-from: still queryable)\n";
    moved.print("moved");

    banner("(6) Copy the moved-from Inventory (deep copy)");
    Inventory copy = moved;  // copy ctor: vector deep-copies every Box
    std::cout << "  copy.size() = " << copy.size() << "\n";
    std::cout << "  moved.size() = " << moved.size() << " (unchanged by copy)\n";
    copy.print("copy");
    moved.print("moved (should still match copy)");

    return EXIT_SUCCESS;
}