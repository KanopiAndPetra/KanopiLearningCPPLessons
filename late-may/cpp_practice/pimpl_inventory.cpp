// pimpl_inventory.cpp -- PimplInventory implementation
//
// This file is where the PIMPL pattern earns its keep: every
// detail of how the Inventory actually stores Boxes is hidden
// here, behind the `Impl` struct, and the rest of the program
// never has to know.
//
// What's in `Impl`?
//   - The `std::vector<Box>` -- the actual storage. The PIMPL
//     version of Inventory owns this vector *transitively*, via
//     the unique_ptr<Impl>. The header has no idea a vector
//     exists.
//   - Optionally, anything else you want to hide. In this lesson
//     I add a `version_` counter that increments on every
//     mutation (add / replace_at / take). The version is a
//     free design choice: it has nothing to do with the PIMPL
//     pattern, but it shows that the *only* limit on what the
//     Impl can carry is "whatever you decide to put in it."
//   - The Jun 16 Inventory's `print` method was a one-liner
//     with no internal state. The PIMPL version gets a tiny
//     `print_` helper inside `Impl` so the trace formatting
//     lives next to the data it walks. That's the
//     data-and-operations-on-data split, applied to the
//     Impl struct. Not strictly required, but it makes the
//     difference between "a struct of state" and "a class"
//     visible.
//
// Why does everything but the move ops live here?
//   - The unique_ptr<Impl> needs Impl's size to delete it.
//     The destructor is the only member that *uses* Impl in
//     a way that requires the complete type.
//   - The copy ops need to read and write the *contents* of
//     Impl (the vector<Box>) to do a deep copy, so they
//     also need Impl complete.
//   - The move ops just transfer the pointer; they don't
//     touch Impl's contents, so they can be `= default`ed
//     in the header (where Impl is still incomplete). They
//     appear here only if you want to write them out by
//     hand -- I leave them as `= default` in the header.

#include "pimpl_inventory.h"
#include "box.h"  // Full Box definition is finally needed here.

#include <algorithm>  // std::min (used in the trace formatter)
#include <cstddef>
#include <iostream>
#include <iterator>   // std::ostream_iterator (used in the trace formatter)
#include <sstream>    // std::ostringstream (used in the trace formatter)
#include <utility>
#include <vector>

// =====================================================================
// The hidden implementation
// =====================================================================
struct PimplInventory::Impl {
    // The actual storage. Same type as Inventory::boxes_ in Jun 16,
    // but it's now a member of Impl, not of PimplInventory. That's
    // the move: from "the Inventory *is* a vector<Box>" to "the
    // Inventory *has a pointer to* a vector<Box>."
    std::vector<Box> boxes_;

    // A freebie: a version counter. Every mutating method bumps it.
    // The point isn't to be useful -- it's to make the next change
    // to Impl (e.g. add a name_ field, add a hash map) a one-line
    // edit here that invalidates *only* this translation unit.
    std::size_t version_ = 0;

    // -- Private helpers, operating on boxes_ --
    //
    // Why are print() and the bounds check here rather than as
    // free functions in pimpl_inventory.cpp? Two reasons:
    //   1. They naturally want access to the private state. Making
    //      them Impl members keeps the access pattern local.
    //   2. It demonstrates that the PIMPL class is really a
    //      *facade* over a class-with-state. The Impl is a class
    //      in its own right; the PimplInventory is a thin shim
    //      that exposes a public API and forwards to it.
    //
    // (If the lesson wanted to go further, the PimplInventory
    // methods below could each be a one-liner that just calls
    // the corresponding Impl method. That's the cleanest PIMPL
    // shape. I do exactly that for some methods, and I leave
    // others as direct impl_->..._ access to show both styles.)
    void bump_version() noexcept { ++version_; }

    // The format-and-print helper. Kept here because it has to
    // know about the size of boxes_ and the layout of Box.
    void print(const std::string& tag) const {
        std::cout << "  PimplInventory(" << tag << ")"
                  << " [size=" << boxes_.size()
                  << " version=" << version_ << "]:\n";
        for (std::size_t i = 0; i < boxes_.size(); ++i) {
            std::cout << "    [" << i << "] '" << boxes_[i].label() << "'\n";
        }
    }
};

// =====================================================================
// Special members
// =====================================================================

// Destructor: defined here (= default) where Impl is complete.
// Declared in the header so the compiler knows we'll handle it.
// Without this declaration in the header, the compiler would
// try to synthesise a dtor at the point of first use, where
// Impl is still incomplete, and ~unique_ptr<Impl> would fail:
// "member access into incomplete type" (the Jun 17 footgun,
// applied to a different class).
PimplInventory::~PimplInventory() = default;

// Copy ctor: declared in the header (so the compiler doesn't
// synthesise a deleted one), defined here as a deep copy of
// the vector<Box> inside Impl.
PimplInventory::PimplInventory(const PimplInventory& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {
    // Note: this calls Impl's *implicit* copy ctor, which copies
    // boxes_ and version_ in turn. Boxes are copyable, so the
    // vector copy just works -- but only because the vector
    // copy is itself a deep copy of the Boxes. A shallow copy
    // here would cause double-deletion on the strings inside
    // the Box labels. (Recall the Jun 4 / Jun 9 / Jun 17
    // Rule-of-Five threads -- copyable PIMPL needs a deep copy
    // of the inner state, transitively.)
    std::cout << "  [PimplInventory copy ctor] copying "
              << other.impl_->boxes_.size() << " Boxes (deep)\n";
}

PimplInventory& PimplInventory::operator=(const PimplInventory& other) {
    if (this != &other) {
        // Strong-exception guarantee: build the new Impl first,
        // then swap it in. If make_unique throws, *this is
        // untouched.
        auto fresh = std::make_unique<Impl>(*other.impl_);
        impl_.swap(fresh);
        // `fresh` goes out of scope here, destroying the old Impl.
        std::cout << "  [PimplInventory copy assign] copy-and-swap, "
                  << "now holding " << impl_->boxes_.size() << " Boxes\n";
    }
    return *this;
}

// Move ops: defaulted in the header. Defined here as `= default`
// for documentation purposes -- the same effect, but the reader
// of this file can see at a glance which members live where.
// (You don't have to repeat the `= default` in the .cpp; the
// header's `= default` is enough. I include it here for clarity.)
PimplInventory::PimplInventory(PimplInventory&& other) noexcept = default;
PimplInventory& PimplInventory::operator=(PimplInventory&& other) noexcept = default;

// =====================================================================
// Public interface -- forwards to the hidden Impl
// =====================================================================

PimplInventory::PimplInventory()
    : impl_(std::make_unique<Impl>()) {
    // The default-constructed vector is empty; version_ defaults to 0.
    std::cout << "  [PimplInventory ctor] empty inventory created\n";
}

void PimplInventory::add(Box b) {
    // Lazy-init: if this object has been moved-from, impl_ is null.
    // Standard PIMPL pattern -- the moved-from object is still
    // assignable and destructible, but mutation ops need a live Impl.
    // We construct a fresh, empty Impl on demand. This makes the
    // class "recoverable" after a move: you can do `dst = std::move(src);`
    // and then `dst.add(...)` and it Just Works.
    if (!impl_) impl_ = std::make_unique<Impl>();
    // b is an lvalue here, so std::move is needed to invoke Box's
    // move ctor into the vector. (Same as Jun 16's Inventory::add.)
    std::cout << "  [PimplInventory::add] storing '" << b.label() << "'\n";
    impl_->boxes_.push_back(std::move(b));
    impl_->bump_version();
}

void PimplInventory::replace_at(std::size_t i, Box&& b) {
    if (!impl_ || i >= impl_->boxes_.size()) return;
    std::cout << "  [PimplInventory::replace_at] swapping '"
              << impl_->boxes_[i].label() << "' <-> '"
              << b.label() << "'\n";
    impl_->boxes_[i] = std::move(b);
    impl_->bump_version();
}

Box PimplInventory::take(std::size_t i) {
    if (!impl_ || i >= impl_->boxes_.size()) return Box("empty");
    Box out = std::move(impl_->boxes_[i]);
    // erase() shifts the trailing elements down via move-assign.
    impl_->boxes_.erase(
        impl_->boxes_.begin() + static_cast<std::ptrdiff_t>(i));
    impl_->bump_version();
    return out;  // NRVO elides the move out of `out`. (Jun 9 §9.)
}

std::size_t PimplInventory::size() const noexcept {
    // The moved-from state of a PimplInventory is "impl_ is null"
    // (because the = default move ctor moved the unique_ptr out).
    // Standard library types treat the moved-from state as valid
    // but unspecified; for a PIMPL class the natural choice is
    // "empty inventory" -- i.e. size() == 0. This guard makes
    // size() on a moved-from object return 0 instead of crashing,
    // which matches std::vector's own behaviour after a move.
    return impl_ ? impl_->boxes_.size() : 0;
}

void PimplInventory::print(const std::string& tag) const {
    if (!impl_) {
        std::cout << "  PimplInventory(" << tag << ") [moved-from, empty]\n";
        return;
    }
    impl_->print(tag);
}
