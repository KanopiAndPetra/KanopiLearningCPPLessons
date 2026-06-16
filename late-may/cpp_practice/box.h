// box.h — Box class declaration, separate-compilation version
//
// This is the *header* for Box. It declares the class and the public
// interface; the *definitions* live in box.cpp. The split is the lesson:
// the header is what other translation units see, the .cpp is what the
// compiler turns into object code and the linker stitches together.
//
// Why a separate Box.h/Box.cpp at all?
//   - Build times: changing box.cpp recompiles only box.cpp and links
//     against the stale inventory.o and main.o.
//   - Encapsulation: the header is the API; the .cpp is the implementation
//     detail. Other code can include box.h and never see the ctors
//     spelled out.
//   - The ODR (One Definition Rule): every translation unit that
//     includes box.h must see the SAME Box class. That's what
//     header guards / #pragma once are for.
//
// What goes in the header?
//   - Class definition (members + member functions declared, not defined).
//   - Inline functions (defined in the class body, or marked `inline`).
//   - Type aliases, constexpr values, template declarations.
//
// What MUST NOT go in the header (without `inline` or `static`)?
//   - Non-inline function *definitions* — that violates the ODR
//     (multiple .cpp files including the header would each define
//     their own copy, and the linker would complain about duplicate
//     symbols).
#pragma once

#include <cstddef>
#include <string>
#include <utility>  // std::move

class Box {
public:
    explicit Box(const std::string& label);
    Box(const Box& other);
    Box(Box&& other) noexcept;
    Box& operator=(Box&& other) noexcept;
    ~Box();

    // Declared in the header, defined in box.cpp. The compiler
    // needs to see the declaration here so that callers can pass
    // `Box` by reference and read `label_` indirectly. The *body*
    // doesn't have to live here.
    const std::string& label() const noexcept;

private:
    std::string label_;
};
