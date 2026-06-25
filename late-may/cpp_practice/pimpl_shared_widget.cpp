// pimpl_shared_widget.cpp
// Definition of Widget::Impl and the Widget member functions.
// Compiled into pimpl_shared_widget.o and linked against the driver.
//
// As with the unique_ptr PIMPL, the dtor and (for the shared_ptr
// version) the special members are declared in the header and
// defined here so that the header stays free of Impl.

#include "pimpl_shared_widget.h"

#include <utility>
#include <vector>

// ----- Impl definition -----

struct Widget::Impl {
    std::string name_;
    int value_ = 0;
    std::vector<int> history_;             // extra private state
    std::uint64_t version_ = 0xC3C3C3C3C3C3C3C3ULL;
};

// ----- ctor / dtor -----

Widget::Widget(std::string name)
    : impl_(std::make_shared<Impl>()) {
    impl_->name_ = std::move(name);
}

Widget::~Widget() = default;

// ----- copy: shared_ptr bumps the refcount -----

Widget::Widget(const Widget& other) = default;
Widget& Widget::operator=(const Widget& other) = default;

// ----- move: shared_ptr moves the pointer, leaves source null -----

Widget::Widget(Widget&& other) noexcept = default;
Widget& Widget::operator=(Widget&& other) noexcept = default;

// ----- public API: forward to impl_-> -----

const std::string& Widget::name() const { return impl_->name_; }
void Widget::set_name(std::string s) { impl_->name_ = std::move(s); }

int Widget::value() const { return impl_->value_; }
void Widget::set_value(int v) {
    impl_->value_ = v;
    impl_->history_.push_back(v);   // touches Impl-only state
}

long Widget::use_count() const { return impl_.use_count(); }
std::uint64_t Widget::version() const { return impl_->version_; }