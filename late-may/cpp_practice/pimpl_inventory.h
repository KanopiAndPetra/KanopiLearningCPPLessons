// pimpl_inventory.h -- PimplInventory class declaration (PIMPL version)
//
// This is the PIMPL-flavored Inventory. The Jun 16 lesson put a
// `std::vector<Box>` directly in the Inventory class, with all the
// includes that entails. This header hides that vector behind a
// forward-declared `Impl` struct owned via `std::unique_ptr<Impl>`.
//
// Why PIMPL the Inventory when we already PIMPLed the Box (Jun 17)?
//   - The Inventory holds a `std::vector<Box>`, which means the
//     header pulls in `<vector>`, `<cstddef>`, and `box.h`. Every
//     translation unit that includes inventory.h pays that cost.
//   - The vector is also part of Inventory's *layout*. Adding a
//     private field to Inventory (e.g. a `std::string name_`) would
//     change the layout, invalidate every client TU's `#include`, and
//     force a recompile of every includer.
//   - With PIMPL, the header is just a `unique_ptr<Impl>` -- a
//     single 8-byte pointer. The header doesn't need to know
//     anything about vectors, Boxes, strings, or whatever else the
//     Impl might grow.
//
// What does the header need to know about Box?
//   - In the Jun 16 version, inventory.h *uses* Box by value (in the
//     `std::vector<Box>`), so the full Box definition is required
//     here. That pull-in is part of the cost.
//   - In this PIMPL version, the header only mentions Box in the
//     *public method signatures*: `add(Box)`, `replace_at(Box&&)`,
//     `take()` returns `Box`, etc. Those mentions only need a
//     *forward declaration* of Box, not the full definition.
//   - So this header forward-declares `class Box;` and does NOT
//     include `box.h`. The `.cpp` (where the methods are defined
//     and where `Impl` is complete) is the one that pulls in
//     `box.h`.
//   - That is the whole point: a *client* TU that wants to put
//     a PimplInventory on the stack, call add()/take()/print(),
//     and never touch a Box directly, can do so without ever
//     including box.h. The Box class definition is hidden behind
//     the PIMPL boundary, just like the vector is.
#pragma once

#include <cstddef>   // std::size_t
#include <memory>    // std::unique_ptr
#include <string>    // std::string (for tag argument in print())

// Forward declaration of Box. The PimplInventory's public surface
// uses Box by value (in parameters and return types) and by
// reference (in method signatures), but those uses do NOT require
// the full Box definition. Only the .cpp, where the methods are
// defined and the Impl is laid out, needs box.h.
class Box;

class PimplInventory {
public:
    // -- Construction --
    PimplInventory();
    ~PimplInventory();

    // -- Big Five --
    // Move ops: defaulted in the header. They only transfer the
    // unique_ptr<Impl>, so they don't need Impl to be complete.
    // (See Jun 17's pimpl_box.h for the same shape -- the rule
    // generalises to any PIMPL class.)
    PimplInventory(PimplInventory&& other) noexcept;
    PimplInventory& operator=(PimplInventory&& other) noexcept;

    // Copy ops: declared in the header, defined in the .cpp. The
    // compiler-generated copy would try to copy the unique_ptr,
    // which is non-copyable, so we *must* declare them ourselves
    // or the class becomes non-copyable. We declare them in the
    // header (so the compiler stops trying to generate one in the
    // header, where Impl is incomplete) and define them in the
    // .cpp as a deep copy of the underlying vector<Box>.
    PimplInventory(const PimplInventory& other);
    PimplInventory& operator=(const PimplInventory& other);

    // -- Public interface (same shape as Jun 16's Inventory) --
    //   add(Box b)        -- sink parameter, vector reallocation moves
    //   replace_at(i, b)  -- in-place swap of an element
    //   take(i)           -- move an element out and erase
    //   size()            -- number of stored Boxes
    //   print(tag)        -- labelled dump
    void add(Box b);
    void replace_at(std::size_t i, Box&& b);
    Box take(std::size_t i);
    std::size_t size() const noexcept;
    void print(const std::string& tag) const;

private:
    // The forward declaration of Impl is enough for the unique_ptr
    // member. The unique_ptr doesn't need to know Impl's size to
    // declare a member of type unique_ptr<Impl> -- the *destructor*
    // needs to know Impl's size, and that's why ~PimplInventory()
    // is declared in the header but defined (= default) in the .cpp
    // where Impl is complete.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
