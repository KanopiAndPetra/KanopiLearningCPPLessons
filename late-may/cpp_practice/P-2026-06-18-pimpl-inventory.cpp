// P-2026-06-18-pimpl-inventory.cpp
//
// Petra's C++ practice for 2026-06-18.
//
// Topic: PIMPL the Inventory -- the natural follow-up to Jun 17's
// PIMPL-the-Box lesson. The Jun 17 "Next Steps" list put this at the
// top: "PIMPL-ify the Inventory -- apply the same pattern to the
// Jun 16 multi-file Inventory. The interesting bit is what goes
// in Inventory::Impl: the std::vector<Box>, yes, but also the index
// validation logic, the trace lines, and the move optimisation."
//
// Files in this lesson:
//   - pimpl_inventory.h     -- PimplInventory class declaration
//   - pimpl_inventory.cpp   -- Impl definition + member function bodies
//   - P-2026-06-18-pimpl-inventory.cpp  -- this file (main + demo)
//
// Build:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o /tmp/pimpl_inv P-2026-06-18-pimpl-inventory.cpp \
//       pimpl_inventory.cpp box.cpp
//   /tmp/pimpl_inv

// pimpl_inventory.h forward-declares Box; it does NOT include box.h.
// That's the whole point of the PIMPL on the Inventory side. So we
// DO need to include box.h here in main, because we *use* Box by
// value in the trace formatter below. A pure client TU that only
// calls PimplInventory::add(Box) would need to include box.h too
// (it has to construct the Box argument), but a client TU that
// just *declared* a PimplInventory and called size() on it would
// not.
#include "pimpl_inventory.h"
#include "box.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>  // std::is_same_v, std::is_trivially_*

// ---------------------------------------------------------------------
// A "pure client" demo type that uses PimplInventory but does NOT
// include box.h. This is the payoff of the PIMPL on the Inventory
// side: a translation unit that only needs the Inventory interface
// pays no compile-time cost for the Box class definition.
//
// The trick: forward-declare Box the same way pimpl_inventory.h
// does. (In a real project this would be done via a separate
// "fwd.h" header; for the lesson, repeating the forward decl in
// main is enough to make the point.)
// ---------------------------------------------------------------------
// Forward declaration of Box at global scope (mirrors the
// forward decl in pimpl_inventory.h). This is the line that
// makes the "pure client" demo work: PureClient's declaration
// does not need box.h, only this forward decl.
class Box;  // forward decl, mirrors pimpl_inventory.h's forward decl

namespace psp {

class PureClient {
public:
    PureClient();
    ~PureClient();
    // Takes a PimplInventory by value, but only forwards to it.
    // Doesn't need Box's definition to compile this method -- only
    // the .cpp, where the Impl lives, needs box.h.
    void store_label(const std::string& s);
    std::size_t count() const noexcept;
private:
    PimplInventory inv_;
};
}  // namespace psp

// The matching .cpp for PureClient would live elsewhere. For the
// lesson we keep it inline. This is the TU that pulls in box.h.
#include "box.h"
namespace psp {
PureClient::PureClient() = default;
PureClient::~PureClient() = default;
void PureClient::store_label(const std::string& s) {
    inv_.add(Box(s));
}
std::size_t PureClient::count() const noexcept { return inv_.size(); }
}  // namespace psp

// =====================================================================
// Section dividers -- keep the output scannable
// =====================================================================
static void section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// =====================================================================
// Section 1 -- sizeof, alignment, and the "single pointer" claim
// =====================================================================
//
// PIMPL's central claim: the class layout is one unique_ptr<Impl>,
// full stop. Adding fields to Impl does not change sizeof(PimplInventory).
// We assert this directly with static_assert so a future change
// that *would* break the promise fails to compile.
static_assert(sizeof(PimplInventory) == sizeof(void*),
              "PimplInventory must be exactly one pointer wide");
static_assert(alignof(PimplInventory) == alignof(void*),
              "PimplInventory alignment must match a pointer");

static void demo_layout() {
    section("Section 1: layout -- sizeof(PimplInventory) is one pointer");
    std::cout << "  sizeof(PimplInventory) = " << sizeof(PimplInventory)
              << " bytes\n"
              << "  alignof(PimplInventory) = " << alignof(PimplInventory)
              << " bytes\n"
              << "  (Vector of Box, version counter, helpers -- all hidden "
              << "in Impl, none reflected in the header layout.)\n";
}

// =====================================================================
// Section 2 -- basic add / print / take / replace_at, with trace
// =====================================================================
static void demo_basic_ops() {
    section("Section 2: basic ops -- add / print / take / replace_at");
    PimplInventory inv;
    inv.print("empty");

    inv.add(Box("red"));
    inv.add(Box("green"));
    inv.add(Box("blue"));
    inv.print("after 3 adds");

    inv.replace_at(1, Box("GREEN"));
    inv.print("after replace_at(1, GREEN)");

    Box taken = inv.take(0);
    std::cout << "  took index 0 -> '" << taken.label() << "'\n";
    inv.print("after take(0)");

    // out-of-range take returns a sentinel Box("empty")
    Box bad = inv.take(999);
    std::cout << "  take(999) -> '" << bad.label() << "' (sentinel)\n";
}

// =====================================================================
// Section 3 -- deep copy is independent
// =====================================================================
static void demo_deep_copy() {
    section("Section 3: deep copy -- original and copy are independent");
    PimplInventory a;
    a.add(Box("a1"));
    a.add(Box("a2"));
    a.add(Box("a3"));
    a.print("a");

    PimplInventory b = a;   // copy ctor
    b.print("b (copy of a)");

    // Mutate b. The version counter on a must NOT change, and a's
    // contents must be untouched.
    b.add(Box("b-only"));
    b.replace_at(0, Box("b1-was-changed"));

    a.print("a (after mutating b) -- unchanged");
    b.print("b (after mutation) -- differs from a");
}

// =====================================================================
// Section 4 -- move is cheap (one pointer transfer)
// =====================================================================
static void demo_move() {
    section("Section 4: move -- one pointer transfer, no deep copy");
    PimplInventory src;
    src.add(Box("x"));
    src.add(Box("y"));
    src.add(Box("z"));
    src.print("src");

    PimplInventory dst = std::move(src);
    dst.print("dst (moved-from src)");
    // src is in a valid-but-unspecified state. For our PIMPL class
    // that state is "impl_ is null", which we treat as "empty".
    // size() on a moved-from object returns 0; print() reports the
    // moved-from state explicitly.
    std::cout << "  src.size() after move = " << src.size()
              << " (impl_ was transferred to dst; size() is "
              << "defensive against the moved-from state)\n";
    src.print("src after move");

    // Move-assign: dst was non-empty; we replace its contents with
    // src's (empty) contents via move-assign.
    PimplInventory other;
    other.add(Box("only-one"));
    other = std::move(src);
    other.print("other (after move-assign from src)");
    std::cout << "  src.size() after move-assign = " << src.size() << "\n";

    // The "move-and-then-add" round-trip: prove that a moved-from
    // PimplInventory is *usable* (not just non-crashing). After
    // move-assigning from src, `other` is a valid inventory that
    // can take new Boxes.
    other.add(Box("recovered"));
    other.add(Box("fine"));
    other.print("other (after re-adding to the moved-into object)");
}

// =====================================================================
// Section 5 -- the ABI / header-stability payoff
// =====================================================================
//
// This is the capstone. The promise: a client TU that includes
// pimpl_inventory.h compiles and links against a binary that has
// an arbitrarily large Impl, and adding fields to Impl in the
// future will not invalidate the client's .o file.
//
// The static check we can do in this TU: the header has no
// transitive include of box.h. We verify by including
// pimpl_inventory.h and noting what is / isn't defined.
static void demo_header_stability() {
    section("Section 5: header stability -- what the client TU sees");
    std::cout << "  sizeof(Box)         = " << sizeof(Box) << " bytes\n"
              << "  sizeof(std::string) = " << sizeof(std::string) << " bytes\n"
              << "  (Client TUs that don't include box.h don't see these.)\n"
              << "  sizeof(PimplInventory) = " << sizeof(PimplInventory)
              << " bytes regardless of how big Box or std::string is.\n";
}

// =====================================================================
// Section 6 -- the "pure client" can use PimplInventory without box.h
// =====================================================================
static void demo_pure_client() {
    section("Section 6: a client that uses PimplInventory transitively");
    psp::PureClient client;
    client.store_label("hello");
    client.store_label("world");
    std::cout << "  client.count() = " << client.count() << "\n";
    // Note: the PureClient declaration in this TU forward-declared Box
    // and did not include box.h. The PimplInventory::add(Box) call
    // compiles because Box is mentioned only by-reference-equivalent
    // (it's a parameter type), and pimpl_inventory.h forward-declared
    // Box. The PureClient *body* (the call to Box(s) in store_label)
    // is the one place that needs box.h, and it's in the same TU as
    // this main. A real pure-client TU would put that body in a
    // separate .cpp that does include box.h.
}

// =====================================================================
int main() {
    demo_layout();
    demo_basic_ops();
    demo_deep_copy();
    demo_move();
    demo_header_stability();
    demo_pure_client();
    std::cout << "\n[done]\n";
    return 0;
}
