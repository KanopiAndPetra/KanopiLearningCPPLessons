// pimpl_square.cpp -- PimplSquare::Impl definition + member bodies
//
// The square's Impl is structurally identical to the circle's
// Impl except for the numeric field name (side_ vs radius_) and
// the area formula (side² vs π r²). The lesson's point: the PIMPL
// boilerplate is mechanical, and the two deriveds are nearly
// identical because the polymorphism is in the *base* (the vtable),
// not in the *derived* (the Impl).
#include "pimpl_square.h"

#include <iostream>
#include <ostream>
#include <utility>   // std::move

struct PimplSquare::Impl {
    std::size_t id_;
    ShapeKind kind_;
    const char* name_;
    double side_;

    double area() const noexcept {
        return side_ * side_;
    }
};

PimplSquare::PimplSquare(std::size_t id, double side)
    : impl_(std::make_unique<Impl>()) {
    impl_->id_ = id;
    impl_->kind_ = ShapeKind::kSquare;
    impl_->name_ = "square";
    impl_->side_ = side;
}

PimplSquare::~PimplSquare() = default;

PimplSquare::PimplSquare(PimplSquare&& other) noexcept = default;
PimplSquare& PimplSquare::operator=(PimplSquare&& other) noexcept = default;

PimplSquare::PimplSquare(const PimplSquare& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {
    std::cout << "  [PimplSquare copy ctor] deep copy of id="
              << other.impl_->id_ << " side=" << other.impl_->side_ << "\n";
}

PimplSquare& PimplSquare::operator=(const PimplSquare& other) {
    if (this != &other) {
        impl_ = std::make_unique<Impl>(*other.impl_);
    }
    return *this;
}

void PimplSquare::print(const std::string& tag) const {
    std::cout << "  PimplSquare(" << tag << ") [id=" << impl_->id_
              << " side=" << impl_->side_
              << " area=" << impl_->area() << "]\n";
}

const char* PimplSquare::name() const {
    return impl_->name_;
}

ShapeKind PimplSquare::kind() const {
    return impl_->kind_;
}

std::unique_ptr<PimplShape> PimplSquare::clone() const {
    return std::make_unique<PimplSquare>(*this);
}

double PimplSquare::side() const noexcept {
    return impl_->side_;
}
