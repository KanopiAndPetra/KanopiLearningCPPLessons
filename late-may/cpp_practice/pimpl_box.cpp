// pimpl_box.cpp
// Implementation of the PIMPL Box. The Impl struct is fully
// defined here, where the rest of the program can see it but
// the public header cannot.

#include "pimpl_box.h"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

// The `Impl` struct. Anything we want to keep private to a
// `Box` instance lives here. Adding, removing, or reorganising
// these fields only forces a recompile of pimpl_box.cpp -- not
// of any TU that includes pimpl_box.h.
struct Box::Impl {
    std::string label_;
    std::size_t version_ = 0;                            // tracks label changes
    std::function<std::size_t(const std::string&)> hasher_;  // deliberately heavy

    explicit Impl(const std::string& l) : label_(l), hasher_(std::hash<std::string>{}) {}
};

// ---- ctor / dtor ----------------------------------------------------

Box::Box(const std::string& label)
    : impl_(std::make_unique<Impl>(label)) {}

Box::~Box() = default;                                  // now legal: Impl is complete

// ---- copy (deep: we own a unique_ptr, so we must clone) ------------

Box::Box(const Box& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {}    // delegating copy ctor

Box& Box::operator=(const Box& other) {
    if (this != &other) {
        *impl_ = *other.impl_;                          // delegating copy assign
    }
    return *this;
}

// ---- move (member-wise, cheap) --------------------------------------

Box::Box(Box&& other) noexcept = default;
Box& Box::operator=(Box&& other) noexcept = default;

// ---- public methods -------------------------------------------------

const std::string& Box::label() const { return impl_->label_; }

void Box::set_label(const std::string& label) {
    impl_->label_ = label;
    ++impl_->version_;
}

std::size_t Box::hash() const {
    return impl_->hasher_(impl_->label_) ^ impl_->version_;
}
