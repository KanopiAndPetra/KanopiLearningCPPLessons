// P-2026-06-25-pimpl-shared-ptr-impl.cpp
// Driver for the shared_ptr<Impl> PIMPL Widget.
//
// Goals for this session:
//   1. Observe sizeof(Widget) and alignof(Widget) for the
//      shared_ptr<Impl> facade. Compare to the 8-byte
//      unique_ptr<Impl> facade from Jun 17/18/24.
//   2. Show that copy ctor / copy assign are defaultable (no
//      hand-written clone needed).
//   3. Show that copies SHARE the same Impl — use_count() rises
//      with each copy, falls as Widgets are destroyed.
//   4. Show the moved-from state — a moved-from Widget has a
//      null shared_ptr (use_count() == 0), unlike the unique_ptr
//      case where the moved-from state still had a valid Impl.
//   5. Show the public API works identically to the unique_ptr
//      version: name/value/set_name/set_value/version/use_count.
//   6. Confirm the ABI-style promise still holds: the driver's
//      compiled view of Widget is exactly the public API surface
//      in the header, and the Impl layout is hidden.

#include "pimpl_shared_widget.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

namespace {

void section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

}  // namespace

int main() {
    // ------------------------------------------------------------
    // (1) Layout: sizeof(Widget), alignof(Widget), is_trivially_copyable.
    // ------------------------------------------------------------
    section("layout");

    std::cout << "sizeof(Widget)       = " << sizeof(Widget)       << " bytes\n";
    std::cout << "alignof(Widget)      = " << alignof(Widget)      << " bytes\n";
    std::cout << "is_trivially_copyable = "
              << std::boolalpha << std::is_trivially_copyable<Widget>::value
              << "\n";
    std::cout << "is_copy_constructible = "
              << std::is_copy_constructible<Widget>::value << "\n";
    std::cout << "is_copy_assignable    = "
              << std::is_copy_assignable<Widget>::value    << "\n";
    std::cout << "is_nothrow_move_constructible = "
              << std::is_nothrow_move_constructible<Widget>::value << "\n";

    // Compare to the unique_ptr<Impl> facade from Jun 17/18/24:
    //   sizeof(Widget) = 8 bytes  (one raw pointer)
    //
    // shared_ptr<Impl> on libstdc++/libc++ is 16 bytes: two
    // pointers (the T* and the control block*). On platforms with
    // a packed representation (e.g. MSVC's _Ptr_base) it's 8 bytes
    // of payload plus padding, but conceptually it's TWO pointers.
    std::cout << "\nNote: unique_ptr<Impl> facade was 8 bytes (one raw ptr).\n"
              << "      shared_ptr<Impl> facade carries TWO pointers:\n"
              << "      the Impl* AND the control block*.\n";

    // ------------------------------------------------------------
    // (2) Construction + use_count observability.
    // ------------------------------------------------------------
    section("construction");

    Widget w1("alpha");
    std::cout << "after ctor:        w1.use_count() = " << w1.use_count()
              << ", name = \"" << w1.name() << "\"\n";

    // ------------------------------------------------------------
    // (3) Copy: shared_ptr bumps the refcount. Copies SHARE Impl.
    // ------------------------------------------------------------
    section("copy shares Impl (refcount)");

    Widget w2 = w1;   // copy ctor — defaultable
    std::cout << "after copy ctor:   w1.use_count() = " << w1.use_count()
              << ", w2.use_count() = " << w2.use_count() << "\n";

    // Mutations through one copy are visible through the other —
    // exactly the semantics you'd expect from a shared state.
    w2.set_name("alpha-via-w2");
    std::cout << "after w2.set_name: w1.name() = \"" << w1.name()
              << "\", w2.name() = \"" << w2.name() << "\"\n";

    w2.set_value(42);
    std::cout << "after w2.set_value(42): w1.value() = " << w1.value()
              << ", w2.value() = " << w2.value() << "\n";

    // ------------------------------------------------------------
    // (4) Copy assignment: refcount moves with the assignment.
    // ------------------------------------------------------------
    section("copy assignment");

    Widget w3("independent");
    std::cout << "before assign:     w3.use_count() = " << w3.use_count()
              << " (independent Impl), w1.use_count() = " << w1.use_count() << "\n";

    w3 = w1;   // copy assign — defaultable, w3's old Impl dies here
    std::cout << "after w3 = w1:     w3.use_count() = " << w3.use_count()
              << ", w1.use_count() = " << w1.use_count() << "\n";

    // ------------------------------------------------------------
    // (5) Move: shared_ptr transfers the pointer. Moved-from is null.
    // ------------------------------------------------------------
    section("move (moved-from becomes null)");

    Widget w4 = std::move(w1);
    std::cout << "after move ctor:   w4.use_count() = " << w4.use_count()
              << ", w1.use_count() = " << w1.use_count()
              << "  (w1 is moved-from -> 0)\n";

    // NOTE: at this point w1.impl_ is null. Calling w1.name() would
    // dereference a null Impl* and is undefined behavior. In real
    // code, treat a moved-from object as a zombie: check
    // use_count() > 0 before use, or re-assign before observing.
    //
    // We re-establish w1 with a fresh Impl below to continue safely.
    std::cout << "(do NOT call w1.name() here — impl_ is null.\n"
              << " Defensive pattern: check use_count() > 0, or\n"
              << " re-assign before observing.)\n";

    w1 = Widget("rebuilt");   // re-establish w1 with a fresh Impl
    std::cout << "after w1 = Widget(\"rebuilt\"):  w1.use_count() = "
              << w1.use_count()
              << ", name = \"" << w1.name() << "\"\n";

    // ------------------------------------------------------------
    // (6) Public API works.
    // ------------------------------------------------------------
    section("public API surface");

    std::cout << "w4.name()       = \"" << w4.name() << "\"\n";
    std::cout << "w4.value()      = "   << w4.value() << "\n";
    std::cout << "w4.version()    = 0x" << std::hex << w4.version()
              << std::dec << "\n";

    // ------------------------------------------------------------
    // (7) Scope-bound refcount tracking.
    // ------------------------------------------------------------
    section("refcount tracking with scope");

    Widget outer("outer");
    std::cout << "before inner scope: outer.use_count() = "
              << outer.use_count() << "\n";
    {
        Widget inner1 = outer;
        Widget inner2 = outer;
        std::cout << "inside scope (2 copies): outer.use_count() = "
                  << outer.use_count()
                  << ", inner1.use_count() = " << inner1.use_count()
                  << ", inner2.use_count() = " << inner2.use_count() << "\n";
        std::cout << "(all three share the same Impl)\n";
    }
    std::cout << "after scope: outer.use_count() = "
              << outer.use_count()
              << "  (inner1 and inner2 are destroyed -> refcount drops)\n";

    // ------------------------------------------------------------
    // (8) Driver's compiled view of Widget — like Jun 24's nm step,
    //     but for the shared_ptr version.
    // ------------------------------------------------------------
    section("API surface used by the driver");
    std::cout << "Methods invoked: name(), value(), set_name(),\n"
              << "                set_value(), version(), use_count(),\n"
              << "                copy ctor, copy assign, move ctor,\n"
              << "                dtor. (Compare to the 8-symbol Widget\n"
              << "                surface observed via `nm driver.o |\n"
              << "                grep Widget` in the Jun 24 session.)\n";

    std::cout << "\nDone.\n";
    return 0;
}