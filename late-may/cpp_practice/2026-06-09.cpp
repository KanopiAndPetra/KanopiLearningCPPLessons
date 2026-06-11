// C++ Practice — June 9, 2026
// Topic: std::move, rvalue references, and value categories
// Day 55-ish of C++ practice
//
// Build:  g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//            -o cpp-practice-2026-06-09 cpp-practice-2026-06-09.cpp
// Run:    ./cpp-practice-2026-06-09
//
// What this program exercises:
//   1. lvalue vs rvalue — the basic value-category distinction
//   2. lvalue references (T&) vs rvalue references (T&&)
//   3. std::move is a *cast*, not a transfer — it just marks a name
//      as an rvalue so the rvalue-reference overload can be selected
//   4. Move constructors and move-assignment operators
//   5. Why moved-from objects must remain in a "valid but unspecified" state
//   6. Overload resolution: lvalue-ref / rvalue-ref / const lvalue-ref
//   7. std::forward and forwarding references (T&& inside templates)
//   8. Reference collapsing rules (the four cases)
//   9. Why returning std::move(local) is usually a pessimization (RVO)
//  10. Move semantics inside a realistic small domain (Inventory of Boxes)

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// ------------------------------------------------------------
// A small "traced" string-like type so we can SEE the moves.
// Every constructor, assignment, and destructor prints a line.
// ------------------------------------------------------------
struct Traced {
    std::string data;

    Traced() : data() {
        std::cout << "  [Traced] default ctor ('" << data << "') @" << this << "\n";
    }

    explicit Traced(const std::string& s) : data(s) {
        std::cout << "  [Traced] string ctor  ('" << data << "') @" << this << "\n";
    }

    Traced(const Traced& other) : data(other.data) {
        std::cout << "  [Traced] COPY ctor     ('" << data << "') @" << this
                  << " from @" << &other << "\n";
    }

    Traced(Traced&& other) noexcept : data(std::move(other.data)) {
        std::cout << "  [Traced] MOVE ctor     ('" << data << "') @" << this
                  << " from @" << &other << " (source now '" << other.data << "')\n";
    }

    Traced& operator=(const Traced& other) {
        if (this != &other) {
            data = other.data;
            std::cout << "  [Traced] COPY assign   ('" << data << "') @" << this
                      << " from @" << &other << "\n";
        }
        return *this;
    }

    Traced& operator=(Traced&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            std::cout << "  [Traced] MOVE assign   ('" << data << "') @" << this
                      << " from @" << &other << " (source now '" << other.data << "')\n";
        }
        return *this;
    }

    ~Traced() {
        std::cout << "  [Traced] dtor          ('" << data << "') @" << this << "\n";
    }
};

// Helper to print a section header.
static void section(const std::string& title) {
    std::cout << "\n--- " << title << " ---\n";
}

// ------------------------------------------------------------
// 1. lvalue vs rvalue — the value categories
// ------------------------------------------------------------
static void demo_value_categories() {
    section("1. lvalue vs rvalue");

    int x = 42;            // x is an lvalue (has a name, has an address)
    // (x + 1) is a prvalue — a temporary. We discard the result to keep
    // the demo focused on type-binding rules.
    [[maybe_unused]] int dummy = x + 1; (void)dummy;
    int&& r2 = x + 1;      // OK: rvalue reference extends the temporary's lifetime
    const int& r3 = x + 1; // OK: const lvalue ref also extends temporaries (the "string-literal" rule)

    std::cout << "x        = " << x      << " (lvalue, has identity)\n";
    std::cout << "x + 1    = " << x + 1  << " (prvalue, no identity, temporary)\n";
    std::cout << "r2 binds x+1, r2 = " << r2 << " (rvalue ref, extends the temp)\n";
    std::cout << "r3 binds x+1, r3 = " << r3 << " (const lvalue ref also extends the temp)\n";

    // Short lifetime reminder: the temporaries bound to r2 and r3 live
    // until r2 and r3 go out of scope, NOT just for the full expression.
    static_cast<void>(r2);
    static_cast<void>(r3);
}

// ------------------------------------------------------------
// 2. Overload resolution: lvalue-ref / rvalue-ref / const lvalue-ref
// ------------------------------------------------------------
struct Sink {
    void take(Traced& /*t*/)         { std::cout << "  take(Traced&)         — lvalue overload\n"; }
    void take(const Traced& /*t*/)   { std::cout << "  take(const Traced&)   — const lvalue overload\n"; }
    void take(Traced&& /*t*/)        { std::cout << "  take(Traced&&)        — rvalue overload\n"; }
};

static void demo_overload_resolution() {
    section("2. Overload resolution on value category");

    Traced named("named");
    const Traced cnamed("cnamed");

    Sink s;
    std::cout << "Named lvalue:\n";
    s.take(named);                       // calls (Traced&)

    std::cout << "Const lvalue:\n";
    s.take(cnamed);                      // calls (const Traced&)

    std::cout << "Temporary (rvalue):\n";
    s.take(Traced("temporary"));         // calls (Traced&&)

    std::cout << "std::move(named) — same overload as a temporary:\n";
    s.take(std::move(named));            // calls (Traced&&); named is now "moved-from"
    std::cout << "  named.data after move: '" << named.data << "' (valid, empty)\n";
}

// ------------------------------------------------------------
// 3. std::move is a cast — it doesn't move anything
// ------------------------------------------------------------
static void demo_move_is_a_cast() {
    section("3. std::move is a cast, not a transfer");

    Traced a("hello");
    std::cout << "BEFORE std::move: a.data = '" << a.data << "'\n";

    // static_cast<Traced&&>(a) is exactly what std::move does.
    // It just turns `a` (an lvalue expression) into an rvalue expression.
    // The actual "transfer" only happens if/when a move constructor or
    // move-assignment operator is invoked.
    auto&& r = std::move(a);  // r is a named rvalue reference, so it's an lvalue!
    std::cout << "AFTER std::move:  a.data = '" << a.data << "'  (UNCHANGED — std::move did not move)\n";
    static_cast<void>(r);
}

// ------------------------------------------------------------
// 4. Move construction and assignment in action
// ------------------------------------------------------------
static Traced make_value(const std::string& s) {
    Traced local(s);
    return local;  // NRVO will likely elide the move entirely in -O0+ since C++17
                   // mandates the copy elision for prvalues in many cases
}

static void demo_move_ctor_and_assign() {
    section("4. Move ctor / move assign");

    std::cout << "Construct t1 with a string:\n";
    Traced t1("payload");

    std::cout << "Construct t2 from std::move(t1):\n";
    Traced t2 = std::move(t1);
    std::cout << "  t1.data = '" << t1.data << "'  (moved-from, empty)\n";
    std::cout << "  t2.data = '" << t2.data << "'  (now owns the buffer)\n";

    std::cout << "Move-assign t3 = std::move(t2):\n";
    Traced t3;
    t3 = std::move(t2);
    std::cout << "  t2.data = '" << t2.data << "'  (moved-from)\n";
    std::cout << "  t3.data = '" << t3.data << "'\n";

    std::cout << "Return-by-value from a factory:\n";
    Traced t4 = make_value("factory-output");
    std::cout << "  t4.data = '" << t4.data << "'\n";
}

// ------------------------------------------------------------
// 5. The "valid but unspecified" contract for moved-from objects
// ------------------------------------------------------------
static void demo_moved_from_state() {
    section("5. Moved-from state — \"valid but unspecified\"");

    Traced src("expensive-buf");
    Traced dst = std::move(src);

    // dst is fully constructed and usable.
    std::cout << "dst is fully usable: '" << dst.data << "'\n";
    dst.data = "reused-after-move";
    std::cout << "dst reassigned OK: '" << dst.data << "'\n";

    // src is in a valid-but-unspecified state. For our Traced, the
    // std::string move left src.data empty. The standard lets us
    // assign to it or destroy it — but we must NOT read it as if
    // it still held the original value.
    src.data = "recycled";
    std::cout << "src reused: '" << src.data << "'\n";
}

// ------------------------------------------------------------
// 6. std::forward and forwarding references (T&& inside templates)
// ------------------------------------------------------------
// Reference collapsing rules, written out explicitly:
//   T&  &   -> T&       (lvalue ref  + lvalue ref  -> lvalue ref)
//   T&  &&  -> T&       (lvalue ref  + rvalue ref  -> lvalue ref)
//   T&& &   -> T&       (rvalue ref  + lvalue ref  -> lvalue ref)
//   T&& &&  -> T&&      (rvalue ref  + rvalue ref  -> rvalue ref)

template <typename T>
std::string describe() {
    // Helper that maps a deduced T to a human-readable name.
    if (std::is_lvalue_reference_v<T>) return "lvalue ref";
    if (std::is_rvalue_reference_v<T>) return "rvalue ref";
    return "non-reference (value)";
}

template <typename T>
void inspect_deduced(T&& /*x*/) {
    // Inside a template, T&& is a *forwarding reference* (a.k.a. universal ref).
    // - If you pass an lvalue, T deduces to U&  -> T&& collapses to U&  (lvalue ref)
    // - If you pass an rvalue, T deduces to U   -> T&& is a real rvalue ref to U
    std::cout << "  deduced T = " << typeid(T).name()
              << "  — param type is " << describe<T>() << "\n";
}

static void demo_forwarding_reference() {
    section("6. Forwarding reference (T&& in a template)");

    Traced lv("lvalue");
    std::cout << "passing lvalue:\n";
    inspect_deduced(lv);

    std::cout << "passing rvalue (Traced(\"tmp\")):\n";
    inspect_deduced(Traced("tmp"));

    std::cout << "passing std::move(lv):\n";
    inspect_deduced(std::move(lv));
}

// A factory that uses std::forward to preserve value category.
template <typename T, typename... Args>
T make_traced(Args&&... args) {
    // std::forward<Args>(args)... casts each arg to its ORIGINAL value
    // category (lvalue or rvalue) so the inner constructor picks the
    // right overload.
    std::cout << "  forwarding " << sizeof...(args) << " arg(s) to T's ctor\n";
    return T(std::forward<Args>(args)...);
}

static void demo_std_forward() {
    section("7. std::forward preserves value category");

    std::cout << "make_traced<Traced>(\"forwarded\"):\n";
    Traced a = make_traced<Traced>(std::string("forwarded"));
    std::cout << "  a.data = '" << a.data << "'\n";
}

// ------------------------------------------------------------
// 8. Practical domain: a small Inventory of Boxes
// ------------------------------------------------------------
class Box {
public:
    explicit Box(const std::string& label) : label_(label) {
        std::cout << "  [Box] ctor ('" << label_ << "')\n";
    }
    Box(const Box& other) : label_(other.label_) {
        std::cout << "  [Box] COPY ctor ('" << label_ << "')\n";
    }
    Box(Box&& other) noexcept : label_(std::move(other.label_)) {
        std::cout << "  [Box] MOVE ctor ('" << label_ << "')\n";
    }
    Box& operator=(Box&& other) noexcept {
        if (this != &other) {
            label_ = std::move(other.label_);
            std::cout << "  [Box] MOVE assign ('" << label_ << "')\n";
        }
        return *this;
    }
    ~Box() = default;

    const std::string& label() const noexcept { return label_; }

private:
    std::string label_;
};

class Inventory {
public:
    void add(Box b) {                 // pass-by-value: sinks, can be moved-in
        std::cout << "  [Inventory::add] storing '" << b.label() << "'\n";
        boxes_.push_back(std::move(b));
    }

    // Take a Box from outside and *swap* it into a slot — no allocation.
    void replace_at(std::size_t i, Box&& b) {
        if (i >= boxes_.size()) return;
        std::cout << "  [Inventory::replace_at] swapping '" << boxes_[i].label()
                  << "' <-> '" << b.label() << "'\n";
        boxes_[i] = std::move(b);
    }

    // Take ownership of a Box returned to us — efficient move out.
    Box take(std::size_t i) {
        if (i >= boxes_.size()) return Box("empty");
        Box out = std::move(boxes_[i]);
        // Erase the slot. erase() shifts later elements; for a vector
        // this means a few move-assignments. pop-and-swap would also work.
        boxes_.erase(boxes_.begin() + static_cast<std::ptrdiff_t>(i));
        return out;
    }

    std::size_t size() const noexcept { return boxes_.size(); }

    void print(const std::string& tag) const {
        std::cout << "  Inventory(" << tag << ") [size=" << boxes_.size() << "]:\n";
        for (std::size_t i = 0; i < boxes_.size(); ++i) {
            std::cout << "    [" << i << "] '" << boxes_[i].label() << "'\n";
        }
    }

private:
    std::vector<Box> boxes_;
};

static void demo_inventory() {
    section("8. Inventory — moves at the API boundary");

    Inventory inv;

    std::cout << "Adding boxes (pass-by-value + std::move into the vector):\n";
    inv.add(Box("red"));          // temporary -> move-constructed into add()'s parameter
    inv.add(Box("green"));
    inv.add(Box("blue"));

    inv.print("after adds");

    std::cout << "\nReplacing slot 0 with 'yellow' (rvalue overload):\n";
    inv.replace_at(0, Box("yellow"));
    inv.print("after replace");

    std::cout << "\nTaking slot 1 (efficient move out of the vector):\n";
    Box taken = inv.take(1);
    std::cout << "  taken.label() = '" << taken.label() << "'\n";
    inv.print("after take");
}

// ------------------------------------------------------------
// 9. Don't std::move a local you are about to return
// ------------------------------------------------------------
static std::string make_greeting_rvo() {
    std::string s = "hello, world (RVO)";
    return s;             // C++17 prvalue rules often elide the move entirely
}

static std::string make_greeting_manually_moved() {
    std::string s = "hello, world (manual move)";
    // The std::move here is INTENTIONALLY a pessimization — that's the
    // point of section 9. We suppress -Wpessimizing-move so the build
    // is clean; the surrounding text explains why this is bad.
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wpessimizing-move"
#endif
    return std::move(s);  // pessimization: blocks NRVO, runs an actual move
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
}

static void demo_dont_move_returns() {
    section("9. Don't std::move a local you are returning");

    auto a = make_greeting_rvo();
    auto b = make_greeting_manually_moved();
    std::cout << "a = " << a << "\n";
    std::cout << "b = " << b << "\n";
    std::cout << "  (In optimized builds NRVO elides a's move entirely;\n";
    std::cout << "   the std::move version blocks NRVO and forces a real move.)\n";
}

// ------------------------------------------------------------
// main — run every demo in order
// ------------------------------------------------------------
int main() {
    std::cout << "=== C++ Practice — June 9, 2026 ===\n";
    std::cout << "Topic: std::move, rvalue references, value categories\n";

    demo_value_categories();
    demo_overload_resolution();
    demo_move_is_a_cast();
    demo_move_ctor_and_assign();
    demo_moved_from_state();
    demo_forwarding_reference();
    demo_std_forward();
    demo_inventory();
    demo_dont_move_returns();

    std::cout << "\n=== Done — RAII cleaned everything up ===\n";
    return 0;
}
