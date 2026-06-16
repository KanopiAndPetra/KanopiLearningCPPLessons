// P-2026-06-16-inventory-multi-file.cpp
// Topic: Splitting a single-file program into separate compilation units.
//
// Build:  make                (preferred — uses the Makefile)
//   or:   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//             -o P-2026-06-16-inventory-multi-file \
//             P-2026-06-16-inventory-multi-file.cpp box.cpp inventory.cpp
// Run:    ./P-2026-06-16-inventory-multi-file
//
// What this lesson demonstrates
// -----------------------------
// The Jun 9 "Inventory of Boxes" lesson (2026-06-09.cpp §8) was a
// single translation unit: main.cpp, Box, and Inventory all in one
// .cpp file. That works for a 400-line demo, but as soon as a class
// has more than a few methods, you want it in its own .cpp:
//
//   1. Build time. Edit box.cpp; only box.o and the final binary
//      need to be rebuilt. main.o and inventory.o are stale-but-
//      valid and get re-linked. With one big file, every edit
//      rebuilds the world.
//
//   2. Encapsulation. The header is the API; the .cpp is the
//      implementation. You can give someone box.h and they
//      can use Box without ever seeing how the ctor prints
//      a trace line.
//
//   3. The linker is part of the language now. Once the project
//      has more than one .cpp, you have to understand what
//      the compiler emits (object code) and what the linker
//      does (resolves symbols across object files). The
//      trace lines make that visible.
//
// What this main() does
// ---------------------
// Same demo as Jun 9 §8, but every step is now spread across three
// translation units:
//
//   main.cpp  →  uses Box and Inventory
//   box.cpp   →  defines Box's member functions
//   inventory.cpp → defines Inventory's member functions
//
// The trace shows:
//   - Box ctor (from box.cpp) when a Box is constructed
//   - Inventory::add (from inventory.cpp) when add() is called
//   - Box MOVE ctor when the pass-by-value parameter is moved into
//     the vector
//   - Box dtor when local Box temporaries go out of scope
//   - Box MOVE assign when replace_at() reassigns a slot
//   - Box MOVE ctor + dtor when take() moves a Box out
//
// The order of "Box MOVE ctor" and "Inventory::add" lines is
// informative: it tells you whether the add() call site is
// passing a temporary (move-construct into the parameter, then
// move into the vector) or a named lvalue (copy-construct into
// the parameter, then move into the vector).

#include "box.h"
#include "inventory.h"
#include <iostream>
#include <string>
#include <utility>

static void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

int main() {
    std::cout << "=== Petra — C++ practice 2026-06-16 ===\n";
    std::cout << "Topic: multi-file Inventory (header / impl / Makefile)\n";

    Inventory inv;

    section("1. Adding boxes (rvalue -> move-construct into add()'s parameter -> move into vector)");
    inv.add(Box("red"));     // Box("red") is a prvalue; it move-constructs
    inv.add(Box("green"));   // the parameter; the parameter then move-
    inv.add(Box("blue"));    // assigns into the vector. Two moves total.
    inv.print("after adds");

    section("2. Replacing slot 0 with 'yellow' (rvalue overload)");
    inv.replace_at(0, Box("yellow"));   // rvalue-ref overload; one move-assign
    inv.print("after replace");

    section("3. Taking slot 1 (efficient move out of the vector)");
    Box taken = inv.take(1);            // returns by value; NRVO elides
    std::cout << "  taken.label() = '" << taken.label() << "'\n";
    inv.print("after take");

    section("4. Passing a NAMED lvalue to add() (copy-construct, not move)");
    Box local("purple");                // named local — an lvalue
    inv.add(local);                     // copy-construct the parameter,
    // then move-assign into the vector.
    // `local` is still valid afterward (valid but unspecified — its
    // label is empty after the move-out, but the object itself is
    // constructible / destructible).
    inv.print("after add(lvalue)");

    std::cout << "\n=== Done — RAII cleans up the Inventory and the local Box ===\n";
    return 0;
}
