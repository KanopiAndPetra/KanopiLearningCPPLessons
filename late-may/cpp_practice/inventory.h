// inventory.h — Inventory class declaration
//
// Same pattern as box.h: declaration in the header, definitions
// in inventory.cpp. The class holds a `std::vector<Box>` and
// exposes add/replace_at/take/print.
//
// Header guards / #pragma once are essential because box.h and
// inventory.h both end up transitively included by main.cpp.
// Without guards, the same Box declaration would appear twice in
// the translation unit, which is an ODR violation.
#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include "box.h"   // We USE Box by value, so the compiler needs
                   // the full Box definition here, not just a
                   // forward declaration. (Forward decl would
                   // work if we only held Box* or Box&, but a
                   // vector<Box> needs the full type.)

class Inventory {
public:
    // pass-by-value: the parameter is a sink. The caller can pass
    // an lvalue (we'll copy-construct) or an rvalue (we'll move-
    // construct). The body moves it into the vector.
    void add(Box b);

    // rvalue-reference overload: the caller must have an rvalue,
    // or use std::move on an lvalue. Cheaper than pass-by-value
    // when the caller already owns a named Box and is happy to
    // sacrifice it.
    void replace_at(std::size_t i, Box&& b);

    // Move the Box at index i out of the vector and return it
    // by value. The body erases the slot (which move-assigns
    // the trailing elements down).
    Box take(std::size_t i);

    std::size_t size() const noexcept;

    void print(const std::string& tag) const;

private:
    std::vector<Box> boxes_;
};
