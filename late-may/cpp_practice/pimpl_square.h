// pimpl_square.h -- PimplSquare: the second concrete derived PIMPL class
//
// Same shape as PimplCircle, but stores a side length instead of a
// radius. The two deriveds are deliberately parallel: a circle has
// one numeric parameter (radius), a square has one numeric parameter
// (side). A real shape hierarchy would have more (e.g. Rectangle with
// width and height), but two is enough to demonstrate the lesson.
//
// The interesting part of having *two* concrete deriveds in the
// same program: the program can put circles and squares in the
// *same* `std::vector<std::unique_ptr<PimplShape>>` and call
// `print()` on each, watching the vtable dispatch pick the right
// derived's print() at runtime.
#pragma once

#include <cstddef>   // std::size_t
#include <memory>    // std::unique_ptr
#include <string>    // std::string
#include <utility>   // std::move

#include "pimpl_shape.h"

class PimplSquare : public PimplShape {
public:
    explicit PimplSquare(std::size_t id, double side);

    ~PimplSquare() override;

    // Big Five: same shape as PimplCircle.
    PimplSquare(PimplSquare&& other) noexcept;
    PimplSquare& operator=(PimplSquare&& other) noexcept;
    PimplSquare(const PimplSquare& other);
    PimplSquare& operator=(const PimplSquare& other);

    // Virtual interface overrides.
    void print(const std::string& tag) const override;
    const char* name() const override;
    ShapeKind kind() const override;
    std::unique_ptr<PimplShape> clone() const override;

    double side() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
