// P-2026-06-17-pimpl.cpp
// The PIMPL (Pointer to Implementation) idiom in C++17.
//
// What PIMPL is: hide every private member of a class behind a
// pointer to a separately-defined struct that lives in the .cpp.
// The header exposes only the public interface plus a single
// `std::unique_ptr<Impl> impl_`. From the client's point of view,
// the class is a fixed size with a known layout, but its private
// state can be reorganised, expanded, or even changed in a binary-
// compatible way without forcing every translation unit to
// recompile.
//
// Why care: with a non-PIMPL class, every change to a private
// member (adding a field, changing a type, adding a virtual
// function) changes the class layout. Every .cpp that includes
// the header must be recompiled. With PIMPL, private state lives
// in the .cpp; touching it is invisible to the header and to
// every TU that includes the header. The build graph shrinks
// dramatically in real projects.
//
// What this program demonstrates:
//   1. A working PIMPL `Box` class, split into box.h and box.cpp.
//   2. The "Big Five" needed because `unique_ptr` is move-only
//      and is a non-trivial owning pointer.
//   3. The two failure modes the PIMPL pattern exists to prevent:
//      (a) header leaks the private data (no PIMPL); (b) declaring
//      the special members in the header with an incomplete
//      `Impl` type (broken PIMPL).
//   4. The compiler errors you would see for each failure mode,
//      produced by side-by-side "broken" variants in this TU.
//
// Build and run:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o /tmp/pimpl_demo P-2026-06-17-pimpl.cpp box.cpp
//   /tmp/pimpl_demo

#include "pimpl_box.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

namespace {

// ---------------------------------------------------------------------------
// Section 1 -- using the real PIMPL Box.
// ---------------------------------------------------------------------------
//
// Everything below uses the public interface declared in box.h.
// None of these callers know or care that Box has a private Impl;
// they just use the methods. The header doesn't drag in <string>
// or <vector> for any *public* reason (the public surface uses
// only `const std::string&` parameters and `std::size_t` -- the
// same as the non-PIMPL Box from 2026-06-16).
void section_1_basic_pimpl() {
    std::cout << "--- Section 1: basic PIMPL Box ---\n";

    Box a{"red"};
    std::cout << "a.label() = " << a.label() << '\n';

    // Move ctor. After the move, `a` is in a "valid but empty"
    // state -- its impl_ pointer is null. We must NOT call
    // a.label() (it would dereference null). The PIMPL idiom
    // shares this with every move-only type.
    Box b = std::move(a);
    std::cout << "after move, b.label() = " << b.label() << "  (a is empty; do not call a.label())\n";

    Box c{"blue"};
    std::cout << "c.label() = " << c.label() << '\n';

    // Move assign. Same story: the rhs is left in a moved-from
    // state. The lhs takes the rhs's pointer.
    c = std::move(b);
    std::cout << "after c = std::move(b), c.label() = " << c.label() << '\n';

    // std::swap exercises both move ctor and move assign. Both
    // sides are well-defined after.
    Box d{"green"};
    Box e{"yellow"};
    std::cout << "before swap: d = " << d.label() << ", e = " << e.label() << '\n';
    using std::swap;
    swap(d, e);
    std::cout << "after  swap: d = " << d.label() << ", e = " << e.label() << '\n';

    // hash() exercises the indirection: every call to hash()
    // dereferences impl_ and then calls into the std::function
    // inside Impl. The cost is one pointer hop and one
    // std::function call per hash() -- the trade for PIMPL.
    std::cout << "d.hash() = " << d.hash() << '\n';
    d.set_label("blue");
    std::cout << "d.hash() after set_label = " << d.hash()
              << "  (version_ bumped; hash changed)\n";

    // Copy ctor / copy assign. PIMPL must deep-copy, because the
    // impl_ is a unique_ptr (not shared). Without a deep copy,
    // the two Boxes would share a single Impl and double-delete.
    Box f = c;                                          // copy ctor
    std::cout << "f = c (copy): f.label() = " << f.label() << '\n';
    Box g{"purple"};
    g = f;                                              // copy assign
    std::cout << "g = f (copy): g.label() = " << g.label() << '\n';

    // Mutate one; the other should be unaffected (proves deep copy).
    f.set_label("f-was-changed");
    std::cout << "after f.set_label(\"f-was-changed\"):\n"
              << "  f.label() = " << f.label() << '\n'
              << "  c.label() = " << c.label() << "  (c is unchanged -- deep copy)\n"
              << "  g.label() = " << g.label() << "  (g is unchanged -- deep copy)\n";

    std::cout << "sizeof(Box) = " << sizeof(Box) << " bytes"
              << "  (just one unique_ptr<Impl> -- nothing else)\n";
}

// ---------------------------------------------------------------------------
// Section 2 -- observing the cost of a header change.
// ---------------------------------------------------------------------------
//
// With PIMPL, the header is "stable" in a way that the
// non-PIMPL header is not. We don't have a real multi-TU project
// to time here, but the test below shows the *class size* of
// PIMPL Box is independent of its private members. Add a 1KB
// buffer to `Impl` in box.cpp and the size of `Box` here is
// unchanged. That size stability is what gives PIMPL its
// binary-compatibility and fast-rebuild properties.
void section_2_size_independence() {
    std::cout << "\n--- Section 2: class size is independent of private state ---\n";
    std::cout << "sizeof(Box) = " << sizeof(Box) << " bytes\n";
    std::cout << "alignment of Box = " << alignof(Box) << " bytes\n";
    std::cout << "  (a Box is exactly one std::unique_ptr<Box::Impl>;\n"
              << "   add a 1KB scratch buffer to Impl in box.cpp and\n"
              << "   this number won't move. That is the whole point.)\n";
}

// ---------------------------------------------------------------------------
// Section 3 -- the special members, in plain English.
// ---------------------------------------------------------------------------
//
// A class that owns a `std::unique_ptr<T>` has the following
// compiler-default behaviour for the special members:
//
//   - Destructor:    NOT generated. The compiler will tell you
//                    `~unique_ptr()` requires T to be complete at
//                    the point of instantiation, and Box's dtor
//                    (implicitly) is instantiated wherever a Box
//                    is destroyed -- including inside the dtor of
//                    any container that holds Box. We must write
//                    `~Box()` ourselves, in box.cpp where Impl is
//                    complete.
//
//   - Copy ctor:     NOT generated (unique_ptr is move-only).
//                    We must declare it (in the header) but
//                    *define* it in box.cpp where Impl is complete.
//
//   - Copy assign:   NOT generated. Same shape as copy ctor.
//
//   - Move ctor:     Generated, and works correctly -- but only if
//                    the destructor is *user-declared* in the
//                    header first. (Rule of Five: declaring the
//                    dtor suppresses the implicit move ops, so we
//                    must `= default` them in the header.)
//
//   - Move assign:   Same as move ctor.
//
// In box.h we therefore declare and `= default` everything *in the
// header* EXCEPT the dtor and the copy ops, which we declare in
// the header and define in box.cpp. This is the canonical PIMPL
// shape. The reason move ops can be `= default`-ed in the header
// is that the defaulted move ctor does member-wise move, and
// moving a `unique_ptr<Impl>` is fine even when Impl is incomplete
// (the move only transfers the pointer; it never looks at the
// pointee). The destructor cannot be defaulted in the header for
// the same reason a destructor cannot be defaulted in the header
// of any owning class: the compiler would need to instantiate
// `~unique_ptr<Impl>()` at the point where the implicit dtor is
// generated, which is the header.
void section_3_special_members_explained() {
    std::cout << "\n--- Section 3: what the special members look like ---\n";
    std::cout << "  - dtor     : declared in header, defined in box.cpp\n"
              << "  - copy ctor: declared in header, defined in box.cpp\n"
              << "  - copy =   : declared in header, defined in box.cpp\n"
              << "  - move ctor: = default in header (works; unique_ptr move)\n"
              << "  - move =   : = default in header (works; unique_ptr move)\n";
}

// ---------------------------------------------------------------------------
// Section 4 -- the "broken" PIMPL variants, as side-by-side examples.
// ---------------------------------------------------------------------------
//
// These three local types mirror the three shapes a header can
// have. None of them is ever instantiated -- they exist so the
// reader can see the layout and the special-member declarations
// side by side, and so the demo can print their sizes.

// 4a. The NON-PIMPL failure mode:
//     Private members live in the header. Every change forces a
//     rebuild in every TU that includes the header.
struct NonPimpl {
    int x_ = 0;                                   // public, not the point
    int private_y_ = 0;                           // in the header -> leaky
};

// 4b. The BROKEN-PIMPL failure mode:
//     Owns unique_ptr<Impl> with no user-declared dtor. The
//     compiler tries to generate the dtor at the first point of
//     use, but `~unique_ptr<Impl>()` needs `Impl` to be complete.
//     The header is the only place guaranteed to be parsed, and
//     `Impl` is in the .cpp. Result: every call site where a
//     BrokenPimpl is destroyed gets a "incomplete type" error --
//     far from the cause.
struct BrokenPimpl {
    struct Impl;                                  // forward decl only
    std::unique_ptr<Impl> impl_;
    // No user-declared dtor -> broken.
};

// 4c. The CANONICAL PIMPL shape:
//     Destructor user-declared (in header), copy ops user-declared
//     (in header), move ops = default in header. Definitions that
//     need a complete Impl live in the .cpp.
struct CanonicalPimpl {
    struct Impl;                                  // forward decl
    std::unique_ptr<Impl> impl_;
    CanonicalPimpl();
    ~CanonicalPimpl();                            // declared
    CanonicalPimpl(CanonicalPimpl&&) noexcept;             // declared
    CanonicalPimpl& operator=(CanonicalPimpl&&) noexcept;  // declared
    CanonicalPimpl(const CanonicalPimpl&);                 // declared
    CanonicalPimpl& operator=(const CanonicalPimpl&);      // declared
};

void section_4_failure_modes() {
    std::cout << "\n--- Section 4: failure modes (side-by-side sizes) ---\n";
    std::cout << "  NonPimpl:         sizeof = " << sizeof(NonPimpl)
              << " bytes -- private_y_ leaks to every includer\n";
    std::cout << "  BrokenPimpl:      sizeof = " << sizeof(BrokenPimpl)
              << " bytes (just the unique_ptr); no user-declared dtor\n";
    std::cout << "                     -> would fail to compile at first use site\n";
    std::cout << "  CanonicalPimpl:   sizeof = " << sizeof(CanonicalPimpl)
              << " bytes (same shape, but dtor + copy ops declared)\n";
    std::cout << "  -- see comments in this file for the exact compiler\n"
              << "     errors the broken variants would produce.\n";

    // A couple of static_asserts to anchor the templates above in
    // case a future reader removes the comments.
    static_assert(std::is_default_constructible<NonPimpl>::value,
                  "NonPimpl is default-constructible");
    static_assert(!std::is_copy_constructible<BrokenPimpl>::value,
                  "BrokenPimpl is not copy-constructible (unique_ptr)");
    static_assert(std::is_move_constructible<BrokenPimpl>::value,
                  "BrokenPimpl is move-constructible (the move is fine)");
    static_assert(std::is_move_constructible<CanonicalPimpl>::value,
                  "CanonicalPimpl is move-constructible");
}

}  // namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    section_1_basic_pimpl();
    section_2_size_independence();
    section_3_special_members_explained();
    section_4_failure_modes();
    std::cout << "\nPIMPL: private state hidden behind a pointer, header stays small,\n"
              << "rebuilds stay fast, ABI stays stable. The cost is one heap\n"
              << "allocation per object and one indirection per method call.\n";
    return 0;
}
