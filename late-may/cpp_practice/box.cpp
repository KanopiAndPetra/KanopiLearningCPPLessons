// box.cpp — Box class implementation
//
// This is the *definition* side. The compiler turns this into box.o
// (an object file). When main.o and inventory.o are linked into the
// final executable, the linker resolves the symbols declared in box.h
// to the definitions in box.o.
//
// Two important things to notice:
//   1. The trace lines that print on every ctor/dtor make the
//      "what gets called when" visible. With a multi-file build
//      you can no longer just read top-to-bottom in one file —
//      you have to follow includes. The trace lines are how
//      this lesson is observable.
//   2. The Box move ctor and move-assign are `noexcept`. That's
//      not decorative: it lets std::vector choose move over copy
//      during reallocation, which is the difference between
//      O(1) and O(n) per reallocation. (Cross-ref Jun 12.)

#include "box.h"
#include <iostream>
#include <utility>

// Out-of-line definitions. The `Box::` qualifier says "this is the
// Box class declared in box.h, and here's the body." Without the
// qualifier, these would be free functions, not member functions.

Box::Box(const std::string& label) : label_(label) {
    std::cout << "  [Box] ctor ('" << label_ << "') @"
              << static_cast<const void*>(this) << "\n";
}

Box::Box(const Box& other) : label_(other.label_) {
    std::cout << "  [Box] COPY ctor ('" << label_ << "') @"
              << static_cast<const void*>(this) << "\n";
}

Box::Box(Box&& other) noexcept : label_(std::move(other.label_)) {
    std::cout << "  [Box] MOVE ctor ('" << label_ << "') @"
              << static_cast<const void*>(this) << "\n";
}

Box& Box::operator=(Box&& other) noexcept {
    if (this != &other) {
        label_ = std::move(other.label_);
        std::cout << "  [Box] MOVE assign ('" << label_ << "') @"
                  << static_cast<const void*>(this) << "\n";
    }
    return *this;
}

Box::~Box() {
    std::cout << "  [Box] dtor ('" << label_ << "') @"
              << static_cast<const void*>(this) << "\n";
}

const std::string& Box::label() const noexcept {
    return label_;
}
