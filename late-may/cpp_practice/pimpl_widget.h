// pimpl_widget.h
// Public interface for a PIMPL widget.
//
// THIS HEADER IS DELIBERATELY MINIMAL.
// The Widget class owns exactly one piece of state: a single
// pointer (`std::unique_ptr<Impl> impl_`). The Impl struct is
// forward-declared here; its full definition lives in pimpl_widget.cpp
// and is never visible to translation units that include this header.
//
// The whole point of this layout is ABI stability:
//   - The header's view of Widget is fixed: a vtable (the implicit
//     one for the non-virtual class is empty) plus 8 bytes of pointer.
//   - The header does NOT include <string>, <vector>, <map>, or any
//     other header that the Impl might grow to need.
//   - Changing the Impl (adding a field, a std::string, a vector,
//     anything) does not change anything visible to client TUs.
//     They keep their cached precompiled pimpl_widget.h artifacts
//     and their already-built driver .o files.
//
// The compile unit that USES Widget only needs to know:
//   - Widget's size and alignment (both fixed by the pointer member)
//   - Widget's method signatures (fixed by the header)
//   - That `~Widget()` is where Impl's destructor runs (fixed by
//     the `= default` in the .cpp)
//
// What the using TU does NOT need to know:
//   - What fields Impl has
//   - What headers Impl pulls in
//   - The size or alignment of Impl
//
// The driver (P-2026-06-24-pimpl-abi-stability.cpp) demonstrates
// this by being compiled ONCE, then linked against two different
// pimpl_widget.cpp variants without recompilation.

#ifndef LATE_MAY_CPP_PRACTICE_PIMPL_WIDGET_H_
#define LATE_MAY_CPP_PRACTICE_PIMPL_WIDGET_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class Widget {
public:
    explicit Widget(std::string name);
    ~Widget();   // out-of-line: where Impl is destroyed

    // Move-only (the unique_ptr<Impl> makes this the natural choice)
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;

    // Disable copy (the unique_ptr makes copying expensive; we
    // don't support it. Document the intent.)
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // Public methods. The bodies are in pimpl_widget.cpp because
    // they need the full Impl definition.
    void set_name(std::string n);
    const std::string& name() const;

    void set_value(int v);
    int value() const;

    // A "version" method that is intentionally defined in two
    // different ways across the v1/v2 builds to make the demo
    // observable. Returns a std::uint64_t — a stable ABI-friendly
    // integer type.
    std::uint64_t version() const;

    // The "ABI fingerprint": sizeof the facade type. If PIMPL is
    // doing its job, this stays at 8 bytes regardless of how Impl
    // grows. We use this in the driver to print the result.
    static constexpr std::size_t facade_size() {
        return sizeof(Widget);
    }
private:
    struct Impl;                 // forward declaration
    std::unique_ptr<Impl> impl_; // the *only* member visible to clients
};

#endif  // LATE_MAY_CPP_PRACTICE_PIMPL_WIDGET_H_