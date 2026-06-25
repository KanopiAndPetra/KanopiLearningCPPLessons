// pimpl_shared_widget.h
// Public facade for a Widget whose implementation lives behind
// std::shared_ptr<Impl> instead of std::unique_ptr<Impl>.
//
// Difference from the Jun 17/18/24 sessions (which used unique_ptr<Impl>):
//
//   unique_ptr<Impl>  -> move-only, deep copy requires hand-written
//                        operator=/copy ctor that clones Impl.
//   shared_ptr<Impl>  -> copy is free (defaultable), each Widget gets
//                        its own control block reference; cost is a
//                        refcount increment on copy / decrement on
//                        destruction.
//
// Header is still free of any Impl definition. The out-of-line
// destructor is required (same reason as the unique_ptr PIMPL —
// destroying shared_ptr<Impl> needs the complete Impl type).
#pragma once

#include <memory>
#include <string>
#include <cstdint>

class Widget {
public:
    // ctor: takes the same public constructor surface as the
    // unique_ptr version so we can compare them.
    explicit Widget(std::string name);
    ~Widget();

    // Copy ctor / copy assign are DEFAULTABLE with shared_ptr<Impl>.
    // The compiler-generated versions correctly bump the refcount
    // on each copy and decrement on each destruction. Compare this
    // to the unique_ptr version which has these members DELETED.
    Widget(const Widget& other);
    Widget& operator=(const Widget& other);

    // Move ctor / move assign are also DEFAULTABLE: shared_ptr move
    // is just a pointer + refcount transfer. The moved-from source
    // ends up with a null shared_ptr (refcount is preserved on the
    // new owner).
    Widget(Widget&& other) noexcept;
    Widget& operator=(Widget&& other) noexcept;

    // Public API — same surface as the Jun 24 ABI-stability Widget.
    const std::string& name() const;
    void set_name(std::string s);

    int value() const;
    void set_value(int v);

    // Deliberate escape hatch: returns the strong refcount of Impl.
    // Demonstrates that copies share state. Same role as the
    // version() magic in Jun 24 — exposes internals when wanted.
    long use_count() const;
    std::uint64_t version() const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};