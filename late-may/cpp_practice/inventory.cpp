// inventory.cpp — Inventory implementation
//
// The body of every method declared in inventory.h. Each method
// prints a small trace line so the user can see which methods
// are called and in what order. The trace is the lesson —
// without it, the multi-file split would be invisible at runtime.

#include "inventory.h"
#include <cstddef>
#include <iostream>
#include <utility>

void Inventory::add(Box b) {
    std::cout << "  [Inventory::add] storing '" << b.label() << "'\n";
    boxes_.push_back(std::move(b));  // b is an lvalue here, std::move
                                     // is required to invoke the
                                     // Box move ctor.
}

void Inventory::replace_at(std::size_t i, Box&& b) {
    if (i >= boxes_.size()) return;
    std::cout << "  [Inventory::replace_at] swapping '"
              << boxes_[i].label() << "' <-> '" << b.label() << "'\n";
    boxes_[i] = std::move(b);
}

Box Inventory::take(std::size_t i) {
    if (i >= boxes_.size()) return Box("empty");
    Box out = std::move(boxes_[i]);
    // erase() shifts later elements; each shift is a move-assign.
    boxes_.erase(boxes_.begin() + static_cast<std::ptrdiff_t>(i));
    return out;  // NRVO elides the move out of `out`. (Cross-ref Jun 9 §9.)
}

std::size_t Inventory::size() const noexcept {
    return boxes_.size();
}

void Inventory::print(const std::string& tag) const {
    std::cout << "  Inventory(" << tag << ") [size=" << boxes_.size() << "]:\n";
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
        std::cout << "    [" << i << "] '" << boxes_[i].label() << "'\n";
    }
}
