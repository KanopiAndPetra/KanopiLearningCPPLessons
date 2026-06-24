// pimpl_widget.cpp
// Definition of Widget::Impl and all Widget's methods.
//
// This file's contents are deliberately a moving target for the
// demo. The driver (P-2026-06-24-pimpl-abi-stability.cpp) compiles
// against the header ONCE, then we relink against two different
// versions of THIS file (v1 = "small Impl", v2 = "big Impl with
// extra fields") WITHOUT recompiling the driver.
//
// The header is identical for both versions; only this .cpp differs.
// If the driver still runs correctly against v2 (with bigger Impl
// and possibly different version() return value), PIMPL preserved
// ABI.
//
// Build:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -c pimpl_widget.cpp -o pimpl_widget.o
//   # v1 vs v2 is selected by editing the comment block below.

#include "pimpl_widget.h"

#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// IMPL LAYOUT — selected at compile time via -DPIMPL_V1 or -DPIMPL_V2.
// =============================================================================
// v1 is the baseline (small Impl). v2 deliberately grows the Impl
// with extra fields, an embedded buffer, a helper struct, and a
// different version_ magic value. The header (pimpl_widget.h) is
// identical for both — only this .cpp differs.
//
// Build both:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -DPIMPL_V1 -c pimpl_widget.cpp -o pimpl_widget_v1.o
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -DPIMPL_V2 -c pimpl_widget.cpp -o pimpl_widget_v2.o
//
// Then link the SAME driver.o against each:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o driver_v1 driver.o pimpl_widget_v1.o
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o driver_v2 driver.o pimpl_widget_v2.o
//
// If PIMPL preserves ABI, both driver_v1 and driver_v2 should
// run identically except for the version() output.

#if defined(PIMPL_V1)

struct Widget::Impl {
    std::string name_;   // ~24 bytes (SSO)
    int value_;          //   4 bytes
    std::uint64_t version_ = 0xA1A1A1A1A1A1A1A1ULL;
};

#elif defined(PIMPL_V2)

struct Widget::Impl {
    std::string name_;                            //  ~24 bytes (SSO)
    int value_;                                   //   4 bytes
    std::vector<int> history_;                    //  24 bytes
    std::uint8_t big_buffer_[256];                // 256 bytes
    std::uint64_t checksum_;                      //   8 bytes
    std::uint64_t version_ = 0xB2B2B2B2B2B2B2B2ULL;
    struct Aux {
        int a;
        double b;
        char pad[16];
    };
    Aux aux_;
};

#else
#error "Must define either PIMPL_V1 or PIMPL_V2"
#endif

std::uint64_t Widget::version() const {
    return impl_->version_;
}

// =============================================================================
// Method definitions (unchanged across v1/v2)
// =============================================================================

Widget::Widget(std::string name)
    : impl_(std::make_unique<Impl>()) {
    impl_->name_ = std::move(name);
    impl_->value_ = 0;
}

Widget::~Widget() = default;  // out-of-line so the unique_ptr<Impl>
                              // destructor (which needs the full Impl
                              // definition) is instantiated here.

Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::set_name(std::string n) {
    impl_->name_ = std::move(n);
}

const std::string& Widget::name() const {
    return impl_->name_;
}

void Widget::set_value(int v) {
    impl_->value_ = v;
}

int Widget::value() const {
    return impl_->value_;
}