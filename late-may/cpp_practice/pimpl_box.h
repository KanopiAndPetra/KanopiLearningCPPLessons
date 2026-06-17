// pimpl_box.h
// Public interface for a PIMPL-style `Box` class.
//
// This header is deliberately minimal. The *only* private state
// the class carries is `std::unique_ptr<Impl> impl_`; the `Impl`
// struct itself is forward-declared here, with its full
// definition in pimpl_box.cpp.
//
// Compare this to the multi-file Box from 2026-06-16
// (box.h/box.cpp), which listed every private member in the
// header. There, adding a private field forced every TU that
// included box.h to recompile. Here, adding a private field
// means editing pimpl_box.cpp only -- the header (and therefore
// the build graph) is unchanged.
//
// The cost of PIMPL is one heap allocation per Box and one
// pointer indirection per method call. The benefit, in any
// project with more than a handful of translation units, is
// a smaller and faster rebuild graph and a more stable ABI.

#ifndef LATE_MAY_CPP_PRACTICE_PIMPL_BOX_H_
#define LATE_MAY_CPP_PRACTICE_PIMPL_BOX_H_

#include <cstddef>
#include <memory>
#include <string>

class Box {
public:
    explicit Box(const std::string& label);
    ~Box();                                              // declared, defined in .cpp
    Box(const Box& other);                               // declared, defined in .cpp
    Box& operator=(const Box& other);                    // declared, defined in .cpp
    Box(Box&& other) noexcept;                           // = default works in header
    Box& operator=(Box&& other) noexcept;                // = default works in header

    const std::string& label() const;
    void set_label(const std::string& label);

    // A second method that touches private state, so we can
    // observe that the indirection is uniform.
    std::size_t hash() const;

private:
    struct Impl;                                         // forward decl only
    std::unique_ptr<Impl> impl_;
};

#endif  // LATE_MAY_CPP_PRACTICE_PIMPL_BOX_H_
