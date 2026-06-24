// P-2026-06-24-pimpl-abi-stability.cpp
//
// DEMO: PIMPL preserves ABI across layout changes.
//
// This file is the "client" of Widget. The accompanying Makefile (or
// shell script in the .md) compiles this file ONCE into driver.o,
// then links it twice — once against the v1 widget .o (small Impl)
// and once against the v2 widget .o (BIG Impl with extra fields).
// The same driver.o is used for both links; this program does not
// recompile itself.
//
// What this proves:
//   1. sizeof(Widget) stays at 8 bytes regardless of Impl size.
//   2. The same driver source code, compiled once, runs correctly
//      against both implementations.
//   3. The v2 binary, despite having a much bigger Impl (256-byte
//      buffer, std::vector, helper struct), still destroys Widget
//      correctly because the dtor is defined out-of-line in
//      pimpl_widget.cpp.
//   4. The version() magic number changes between v1 and v2 because
//      Impl's member was given a different default — a deliberate
//      public API hook that demonstrates "internals can change
//      without recompiling clients, but a chosen observable can
//      reveal the change when wanted."
//
// Build (see the .md for the exact recipe; repeated here for grep-ability):
//
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -c P-2026-06-24-pimpl-abi-stability.cpp -o driver.o
//
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -DPIMPL_V1 \
//       -c pimpl_widget.cpp -o pimpl_widget_v1.o
//
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -DPIMPL_V2 \
//       -c pimpl_widget.cpp -o pimpl_widget_v2.o
//
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o driver_v1 driver.o pimpl_widget_v1.o
//
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o driver_v2 driver.o pimpl_widget_v2.o
//
//   # Both binaries are now on disk. Same source, same driver.o,
//   # different widget.o. Run them.
//
// The "main" below is the SHARED client logic that both driver_v1
// and driver_v2 execute. It uses Widget's public API only. The
// version() value it prints depends on which widget.o was linked.

#include "pimpl_widget.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

int main() {
    std::cout << "================================================\n";
    std::cout << "PIMPL ABI Stability Demo (driver)\n";
    std::cout << "================================================\n";
    std::cout << "sizeof(Widget)  = " << sizeof(Widget)  << " bytes\n";
    std::cout << "alignof(Widget) = " << alignof(Widget) << " bytes\n";
    std::cout << "(This is the facade — the WHOLE public ABI. "
              << "It does not depend on Impl.)\n\n";

    // Build a Widget, exercise its public API.
    Widget w("alpha");
    w.set_value(42);

    // Print everything we know about w from the public API only.
    std::cout << "Widget public state:\n";
    std::cout << "  name    = " << w.name()    << "\n";
    std::cout << "  value   = " << w.value()   << "\n";
    std::cout << "  version = 0x" << std::hex << std::uppercase
              << std::setw(16) << std::setfill('0') << w.version()
              << std::dec << "\n\n";

    // Move semantics. PIMPL is move-friendly because unique_ptr
    // is move-friendly; the move constructor is the default.
    Widget w2 = std::move(w);
    std::cout << "After std::move:\n";
    std::cout << "  w2.name() = " << w2.name() << "\n";
    std::cout << "  (w is moved-from; its handle is empty)\n\n";

    // Mutate through one of the methods to prove the vtable (or
    // rather the implicit dispatch) is wired up.
    w2.set_name("beta");
    w2.set_value(99);
    std::cout << "After w2.set_name(\"beta\"); w2.set_value(99):\n";
    std::cout << "  w2.name()  = " << w2.name()  << "\n";
    std::cout << "  w2.value() = " << w2.value() << "\n\n";

    // Print a fingerprint that the host shell script greps for.
    // v1: 0xA1A1A1A1A1A1A1A1     v2: 0xB2B2B2B2B2B2B2B2
    // This is the ONE observable difference between the two builds.
    std::uint64_t v = w2.version();
    std::cout << "FINAL version = 0x" << std::hex << std::uppercase
              << std::setw(16) << std::setfill('0') << v
              << std::dec << "\n";

    return 0;
}