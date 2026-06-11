// P-2026-06-10-std-variant-visit.cpp
//
// C++17 <variant> and <visit> -- a type-safe tagged union.
//
// Sections:
//   1.  Declaring a variant; .index() and holds_alternative<T>()
//   2.  std::get<T> and std::get<index>: throwing on wrong alternative
//   3.  std::get_if<T>: null on wrong alternative
//   4.  std::visit: visitor overloading with the `overloaded` helper
//   5.  std::visit with a generic lambda (auto&& ...)
//   6.  std::visit<R>(...): explicit return type
//   7.  std::monostate: giving a variant a default-constructible "empty" state
//   8.  Move semantics: variants own the active alternative
//   9.  Lexicographic comparison and the order of alternatives matters
//  10.  Mini-example: arithmetic expression evaluator over a variant AST
//
// Build (C++17 required):
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o P-2026-06-10-std-variant-visit P-2026-06-10-std-variant-visit.cpp
//
// Verify cleanly under ASan:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
//       -o P-2026-06-10-std-variant-visit-asan P-2026-06-10-std-variant-visit.cpp
//   ./P-2026-06-10-std-variant-visit-asan

#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Small traced string-like type so we can see who owns what during moves.
// Same idea as the Jun 9 `Traced`, kept self-contained here.
// ---------------------------------------------------------------------------
struct Traced {
    std::string name;
    std::string payload;

    Traced() : name("?"), payload("") {
        std::cout << "  [Traced] default  (" << name << ")\n";
    }
    explicit Traced(std::string n) : name(std::move(n)), payload("payload-" + name) {
        std::cout << "  [Traced] ctor     (" << name << ") payload='" << payload << "'\n";
    }
    Traced(std::string n, std::string p) : name(std::move(n)), payload(std::move(p)) {
        std::cout << "  [Traced] ctor2    (" << name << ") payload='" << payload << "'\n";
    }
    Traced(const Traced& o) : name(o.name + "(cpy)"), payload(o.payload) {
        std::cout << "  [Traced] copy     (" << name << ") from (" << o.name << ")\n";
    }
    Traced(Traced&& o) noexcept
        : name(std::move(o.name)), payload(std::move(o.payload)) {
        o.name = "(moved)"; o.payload.clear();
        std::cout << "  [Traced] MOVE     (" << name << ")\n";
    }
    Traced& operator=(Traced o) {  // copy-and-swap
        std::cout << "  [Traced] assign   (" << name << ") <-> ("
                  << o.name << ")\n";
        name = std::move(o.name);
        payload = std::move(o.payload);
        return *this;
    }
    ~Traced() {
        std::cout << "  [Traced] dtor     (" << name << ") payload='" << payload << "'\n";
    }
};

// ---------------------------------------------------------------------------
// The classic `overloaded` helper for std::visit lambdas.
// Equivalent to a class with a templated operator() for each alternative.
// ---------------------------------------------------------------------------
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// C++17 deduction guide: lets you write `overloaded{...}` without a type.
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// ---------------------------------------------------------------------------
// Section helpers
// ---------------------------------------------------------------------------
void hr(const char* s) {
    std::cout << "\n========== " << s << " ==========\n";
}

// ---------------------------------------------------------------------------
// 1. Declaring a variant; .index() and holds_alternative<T>()
// ---------------------------------------------------------------------------
void section1_basics() {
    hr("1. Declaration, index(), holds_alternative<T>()");

    // std::variant<int, double, std::string> -- a value that is exactly
    // one of these three types. It is NOT a union of *pointers*; it owns
    // its alternative inline. The first listed type is NOT the default
    // -- the variant is value-initialised, which means its first
    // alternative must be default-constructible. (See std::monostate
    // in section 7 for the workaround when it isn't.)
    std::variant<int, double, std::string> v;

    std::cout << "default-constructed v.index() = " << v.index()
              << " (int is alternative 0)\n";
    std::cout << "holds_alternative<int>(v)    = "
              << std::boolalpha << std::holds_alternative<int>(v) << "\n";

    v = 42;
    std::cout << "after v=42:        index=" << v.index()
              << " holds<int>=" << std::holds_alternative<int>(v) << "\n";

    v = 3.14;
    std::cout << "after v=3.14:      index=" << v.index()
              << " holds<double>=" << std::holds_alternative<double>(v) << "\n";

    v = std::string("hello");
    std::cout << "after v=string:    index=" << v.index()
              << " holds<string>=" << std::holds_alternative<std::string>(v) << "\n";

    // .index() is always valid; alternatives are numbered 0..N-1 in
    // declaration order. It is the only way to tell which alternative
    // is active without knowing the types in advance.
}

// ---------------------------------------------------------------------------
// 2. std::get<T> and std::get<index>: throws on wrong alternative
// ---------------------------------------------------------------------------
void section2_get_throws() {
    hr("2. std::get<T>() and std::get<idx>() -- throw on miss");

    std::variant<int, double, std::string> v = 7;

    int& i = std::get<int>(v);
    std::cout << "std::get<int>(v) = " << i << "\n";
    i = 99;  // reference: modifies the variant's storage in place
    std::cout << "after i=99, v holds " << std::get<int>(v) << "\n";

    // std::get can also be indexed by POSITION. Make a fresh variant
    // holding a string (index 2) so we can demonstrate it without
    // immediately throwing.
    std::variant<int, double, std::string> v2 = std::string("by-index");
    std::cout << "std::get<2>(v2)  = '" << std::get<2>(v2) << "'  (index 2 = string)\n";
    std::cout << "std::get<double>(v2) throws below...\n";

    // Wrong type -> std::bad_variant_access. Catch it.
    try {
        (void)std::get<double>(v2);
    } catch (const std::bad_variant_access& e) {
        std::cout << "caught bad_variant_access: " << e.what() << "\n";
    }
}

// ---------------------------------------------------------------------------
// 3. std::get_if<T>: returns nullptr on wrong alternative
// ---------------------------------------------------------------------------
void section3_get_if() {
    hr("3. std::get_if<T>() -- returns nullptr on miss");

    std::variant<int, double, std::string> v = std::string("yo");

    if (auto* p = std::get_if<std::string>(&v)) {
        std::cout << "got string: '" << *p << "'\n";
        *p = "yo-ho-ho";  // mutable in place
        std::cout << "after in-place edit: '" << std::get<std::string>(v) << "'\n";
    } else {
        std::cout << "not a string right now\n";
    }

    if (auto* p = std::get_if<double>(&v)) {
        (void)p; std::cout << "shouldn't get here\n";
    } else {
        std::cout << "get_if<double> returned nullptr -- no exception\n";
    }
}

// ---------------------------------------------------------------------------
// 4. std::visit: visitor overloading with the `overloaded` helper
// ---------------------------------------------------------------------------
void section4_visit_overload() {
    hr("4. std::visit with the `overloaded` helper");

    using V = std::variant<int, double, std::string>;

    auto describe = [](V const& v) {
        return std::visit(overloaded{
            [](int i)    { return std::string("int ")    + std::to_string(i); },
            [](double d) { return std::string("double ") + std::to_string(d); },
            [](std::string const& s) { return std::string("string '") + s + "'"; },
        }, v);
    };

    std::cout << describe(V(7))            << "\n";
    std::cout << describe(V(2.5))          << "\n";
    std::cout << describe(V(std::string("hi"))) << "\n";

    // Why `overloaded`? `std::visit` calls `visitor.operator()(active_value)`.
    // A single lambda has one operator(); the std::visit API has no
    // "switch on type" builtin. The `overloaded` struct inherits from
    // several lambdas, exposing one operator() per alternative type.
    // The deduction guide lets us write `overloaded{...}` without
    // spelling out the type.
}

// ---------------------------------------------------------------------------
// 5. std::visit with a generic lambda (auto&& ...)
// ---------------------------------------------------------------------------
void section5_visit_generic() {
    hr("5. std::visit with a generic lambda");

    using V = std::variant<int, double, std::string>;

    // A single `auto&&` parameter accepts *any* alternative. The
    // compiler instantiates one operator() per alternative at the call
    // site. This is less type-safe (you can't be sure which alternative
    // you're looking at) but it's a one-liner and it composes.
    auto size_of = [](auto&& x) -> std::size_t {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_arithmetic_v<T>) {
            return sizeof(T);
        } else {
            return x.size();
        }
    };

    std::cout << "size_of(7)              = " << std::visit(size_of, V(7))            << "\n";
    std::cout << "size_of(2.5)            = " << std::visit(size_of, V(2.5))          << "\n";
    std::cout << "size_of(string(\"abc\")) = " << std::visit(size_of, V(std::string("abc"))) << "\n";

    // Note: `auto&&` does NOT collapse the same way a template parameter
    // does. Here it's a single lambda with a deduced parameter type, so
    // it can only ever take ONE argument at a time -- the std::visit
    // machinery takes care of picking the right alternative for you.
}

// ---------------------------------------------------------------------------
// 6. std::visit<R>(...): explicit return type
// ---------------------------------------------------------------------------
void section6_visit_explicit_return() {
    hr("6. std::visit<R>(...) -- explicit return type");

    using V = std::variant<int, double, std::string>;

    // Without an explicit R, std::visit deduces the return type from
    // the visitor's operator(). If the alternatives produce DIFFERENT
    // types, you have to disambiguate.
    auto len_or_zero = [](V const& v) {
        return std::visit([](auto&& x) -> long {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<long>(x);
            } else {
                return static_cast<long>(x.size());
            }
        }, v);
    };

    std::cout << "len_or_zero(7)              = " << len_or_zero(V(7))            << "\n";
    std::cout << "len_or_zero(2.5)            = " << len_or_zero(V(2.5))          << "\n";
    std::cout << "len_or_zero(string(\"abc\")) = " << len_or_zero(V(std::string("abc"))) << "\n";

    // The R parameter is a SECOND template argument:
    //   std::visit<long>(lambda, variant)
    // Useful when the lambda's return type would otherwise be 'auto' --
    // e.g. the explicit-return form above would NOT need it because the
    // trailing return type fixes it. But if the lambda returns just
    // `x` and the alternatives are mixed (int / double / string), the
    // deduction will fail; that's when you reach for the explicit R.
}

// ---------------------------------------------------------------------------
// 7. std::monostate: default-constructible "empty" state
// ---------------------------------------------------------------------------
void section7_monostate() {
    hr("7. std::monostate -- the default-constructible placeholder");

    // Without monostate, you cannot default-construct a variant whose
    // first alternative is not default-constructible. With it, you can:
    using V = std::variant<std::monostate, Traced, std::string>;
    static_assert(std::is_default_constructible_v<V>,
                  "variant with monostate first should be default-constructible");

    V v;  // monostate is the active alternative
    std::cout << "default v.index() = " << v.index()
              << " (monostate is 0)\n";
    std::cout << "holds<Traced>(v)  = " << std::holds_alternative<Traced>(v) << "\n";

    {
        Traced t("a-box");
        v = std::move(t);  // t is moved into the variant
        std::cout << "v now holds Traced '" << std::get<Traced>(v).payload << "'\n";
    }  // t's destructor would run here, but t is moved-from and empty

    v = std::monostate{};  // explicitly clear back to "empty"
    std::cout << "after monostate assignment, v.index() = " << v.index() << "\n";

    // Monostate is the idiomatic "no value yet" / "no error" sentinel.
    // It's an empty struct; sizeof(variant<monostate, X, Y>) is the same
    // as sizeof(variant<X, Y>) plus a few bytes of tag.
}

// ---------------------------------------------------------------------------
// 8. Move semantics: variants own the active alternative
// ---------------------------------------------------------------------------
void section8_moves() {
    hr("8. Move semantics: variants own the active alternative");

    using V = std::variant<int, Traced, std::string>;

    {
        V v = Traced("outer");
        std::cout << "v created, holds Traced('outer')\n";
        V w = std::move(v);  // should move the Traced, not copy it
        std::cout << "w created from std::move(v)\n";
        // v is now valueless_by_exception or in moved-from Traced state
        std::cout << "v.valueless_by_exception() = "
                  << std::boolalpha << v.valueless_by_exception() << "\n";
        // The variant's destructor will destroy the active alternative in w
    }

    // What is valueless_by_exception? A variant goes into this state if
    // an exception is thrown by the move- or copy-constructor of the
    // new alternative during assignment. (In well-written code with
    // noexcept moves, this should never happen.) A valueless variant
    // is index() == variant_npos; std::get will throw; std::get_if
    // returns nullptr; visiting works if every operator() accepts the
    // valueless case (or you check index first).
    {
        V v;
        std::cout << "default v.index() = " << v.index() << "\n";
        // assignment from int (alternative 0) to Traced (alternative 1)
        // constructs a new Traced in place; the old int is destroyed.
        v = Traced("fresh");
        std::cout << "after v=Traced('fresh'), v.index() = " << v.index() << "\n";
    }
}

// ---------------------------------------------------------------------------
// 9. Lexicographic comparison
// ---------------------------------------------------------------------------
void section9_compare() {
    hr("9. Lexicographic comparison");

    using V = std::variant<int, std::string>;

    // Variants are compared by: (a) their index, then (b) the value of
    // the active alternative. So variant(7) < variant("anything") because
    // int is alternative 0 and string is alternative 1. Within the same
    // alternative, the underlying type's operator< is used.
    V a = 7;
    V b = std::string("z");
    V c = std::string("a");
    V d = 100;

    std::cout << "(a < b) where a=int(7), b=string(\"z\")        : "
              << (a < b) << " (true: int index 0 < string index 1)\n";
    std::cout << "(b < c) where b=string(\"z\"), c=string(\"a\")   : "
              << (b < c) << " (false: same index, 'z' > 'a')\n";
    std::cout << "(a < d) where a=int(7),    d=int(100)         : "
              << (a < d) << " (true: same index, 7 < 100)\n";

    // ORDER OF ALTERNATIVES MATTERS for < and == semantics. If you
    // rearrange the alternatives, comparison results change even when
    // the values are identical.
}

// ---------------------------------------------------------------------------
// 10. Mini-example: arithmetic expression evaluator
// ---------------------------------------------------------------------------
//   Expression = Number | BinaryOp(left, op, right)
//   EvalResult = double | std::string (error message)
//
//   The point is to show std::visit as the dispatch mechanism for an
//   ADT-style tree, and std::variant as the return type that may
//   succeed or fail.
// ---------------------------------------------------------------------------
struct Number {
    double value;
};
struct BinaryOp {
    std::shared_ptr<struct Expression> left;   // shared_ptr to enable
    char op;                                    // recursive types in
    std::shared_ptr<struct Expression> right;  // a value-semantic API
};
struct Expression {
    std::variant<Number, BinaryOp> node;

    explicit Expression(double v) : node(Number{v}) {}
    Expression(std::shared_ptr<Expression> l, char op, std::shared_ptr<Expression> r)
        : node(BinaryOp{std::move(l), op, std::move(r)}) {}
};

using EvalResult = std::variant<double, std::string>;

EvalResult evaluate(Expression const& e) {
    return std::visit(overloaded{
        [](Number const& n) -> EvalResult {
            return n.value;
        },
        [](BinaryOp const& b) -> EvalResult {
            auto L = evaluate(*b.left);
            auto R = evaluate(*b.right);
            if (std::holds_alternative<std::string>(L)) return L;
            if (std::holds_alternative<std::string>(R)) return R;
            double lv = std::get<double>(L);
            double rv = std::get<double>(R);
            switch (b.op) {
                case '+': return lv + rv;
                case '-': return lv - rv;
                case '*': return lv * rv;
                case '/':
                    if (rv == 0.0) return std::string("division by zero");
                    return lv / rv;
                default:
                    return std::string(std::string("unknown op: ") + b.op);
            }
        },
    }, e.node);
}

void section10_mini_evaluator() {
    hr("10. Mini-example: Expression evaluator");

    // (1 + 2) * (3 - 4/2) = (3) * (3 - 2) = 3 * 1 = 3
    auto e1 = std::make_shared<Expression>(
        std::make_shared<Expression>(1.0), '+', std::make_shared<Expression>(2.0));
    auto e2 = std::make_shared<Expression>(
        std::make_shared<Expression>(3.0), '-',
        std::make_shared<Expression>(
            std::make_shared<Expression>(4.0), '/', std::make_shared<Expression>(2.0)));
    auto e = std::make_shared<Expression>(e1, '*', e2);

    auto print_result = [](char const* label, EvalResult const& r) {
        std::visit(overloaded{
            [label](double v) {
                std::cout << "  " << label << " = " << v << "\n";
            },
            [label](std::string const& err) {
                std::cout << "  " << label << " ERR: " << err << "\n";
            },
        }, r);
    };

    print_result("(1+2) * (3 - 4/2)", evaluate(*e));

    // Division-by-zero, returned as a string, not a thrown exception.
    auto bad = std::make_shared<Expression>(
        std::make_shared<Expression>(1.0), '/', std::make_shared<Expression>(0.0));
    print_result("1 / 0", evaluate(*bad));

    // Note that the variant-returning pattern is one of the two main
    // alternatives to exceptions for "this might fail":
    //   - exceptions: control flow via throw/catch, "unhappy path"
    //                 invisible in the signature
    //   - std::variant (or std::expected<T, E> in C++23): the unhappy
    //                 path is a real alternative, and std::visit is
    //                 the dispatch.
    // The downside: every caller of evaluate() has to std::visit the
    // result. The upside: you can never forget, because there's no
    // overload of operator+() on std::string.
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== std::variant / std::visit (C++17) ===\n";
    section1_basics();
    section2_get_throws();
    section3_get_if();
    section4_visit_overload();
    section5_visit_generic();
    section6_visit_explicit_return();
    section7_monostate();
    section8_moves();
    section9_compare();
    section10_mini_evaluator();
    std::cout << "\n=== done ===\n";
    return 0;
}
