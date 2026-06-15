// P-2026-06-15-stdvisit-stateful-visitors.cpp
//
// C++17 std::visit with STATEFUL visitors.
//
// On Jun 10 I covered the stateless shape: a `std::variant<A, B, C>` and
// an `overloaded{ ... }` pack of lambdas that dispatch by alternative.
// That works fine when the visitor has no per-call context, but the
// moment a visitor needs to "remember" something between calls -- an
// indentation level, a running total, an output stream, a stack of
// in-progress frames, a context pointer, an error sink -- it stops
// being a stateless lambda and starts being a small class.
//
// This file walks the stateful shapes from simplest to most interesting:
//
//   1.  Why "stateful": the question stateless visitors can't answer
//   2.  Class-based visitor that owns its state (indentation pretty-printer)
//   3.  Visitor that BORROWS state via a context reference (PrintContext&)
//   4.  Recursive lambda visitor with `self` capture and shared state
//   5.  Visitor over a std::vector<std::variant<...>> with a fold-style
//       accumulator (sum, count, errors)
//   6.  Visitor that pushes/pops frames on a std::vector<Frame>
//   7.  Visitor across a span of variants (cross-references Jun 14's
//       std::span and Jun 10's variant)
//
// Build (C++17 required):
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       -o P-2026-06-15-stdvisit-stateful-visitors \
//       P-2026-06-15-stdvisit-stateful-visitors.cpp
//   ./P-2026-06-15-stdvisit-stateful-visitors
//
// ASan build, also exercised:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
//       -o P-2026-06-15-stdvisit-stateful-visitors-asan \
//       P-2026-06-15-stdvisit-stateful-visitors.cpp
//   ./P-2026-06-15-stdvisit-stateful-visitors-asan

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace psp {

// ---------------------------------------------------------------------------
// The classic `overloaded` helper (same one as Jun 10, kept here so the file
// is self-contained). Turns a pack of lambdas into a single type whose
// operator() overloads dispatch by argument type -- which is exactly what
// std::visit needs.
// ---------------------------------------------------------------------------
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// ---------------------------------------------------------------------------
// Section helpers
// ---------------------------------------------------------------------------
void hr(const char* s) {
    std::cout << "\n========== " << s << " ==========\n";
}

void note(const char* s) {
    std::cout << "  -- " << s << "\n";
}

// ---------------------------------------------------------------------------
// 1.  Why "stateful": the question stateless visitors can't answer
// ---------------------------------------------------------------------------

// A simple, non-recursive variant to start with. A stateless visitor can
// print each alternative's content. A stateful one can do much more.
using V1 = std::variant<int, double, std::string>;

// Stateless visitor: one operator() per alternative, no fields, no state.
struct StatelessPrinter {
    void operator()(int i) const         { std::cout << "int("    << i    << ")"; }
    void operator()(double d) const      { std::cout << "double(" << d    << ")"; }
    void operator()(std::string const& s) const {
        std::cout << "string(\"" << s << "\")";
    }
};

void section1_motivation() {
    hr("1. Why stateful visitors");

    std::vector<V1> items = {42, 3.14, std::string("hello")};

    std::cout << "  Stateless printer over [42, 3.14, \"hello\"]:\n    ";
    for (auto const& v : items) {
        std::visit(StatelessPrinter{}, v);
        std::cout << " ";
    }
    std::cout << "\n";

    note("A pretty-printer that recurses, indents, and shows precedence "
         "needs to remember its current depth. The stateless shape can "
         "neither recurse nor carry a depth. That's what the rest of the "
         "file builds.");
}

// ---------------------------------------------------------------------------
// 2.  Class-based visitor that owns its state
//     -- a pretty-printer for a recursive variant AST.
// ---------------------------------------------------------------------------

// A real, owned AST node. Recursive variants of this shape are the standard
// "expression tree" pattern. The unique_ptr inside the alternative means
// the AST is self-owning: parent owns children via unique_ptr.
//
// We avoid duplicate alternatives in the variant by tagging each binary
// operation with its own struct that holds a char and a unique_ptr to a
// 2-element array of children. Different struct types => different
// alternatives, even though they share a payload shape.

struct ExprNode;  // fwd

struct IntLit { int value; };
struct AddOp {
    char op;
    std::array<std::unique_ptr<ExprNode>, 2> children;
};
struct MulOp {
    char op;
    std::array<std::unique_ptr<ExprNode>, 2> children;
};
using ExprAlt = std::variant<IntLit, AddOp, MulOp>;
struct ExprNode { ExprAlt alt; };

// A class-based visitor that OWNS its state (current indent + an ostream).
// This is the canonical "stateful" pattern: the visitor is a value that
// gets passed to std::visit, and the overload set lives as members.
class PrettyPrinter {
    std::ostream& out_;
    int           indent_ = 0;
public:
    explicit PrettyPrinter(std::ostream& out) : out_(out) {}

    // Enter/exit helpers -- they update state but are NOT in the overload
    // set (we don't want std::visit trying to call them on a variant).
    void enter()  { indent_ += 2; }
    void leave()  { indent_ -= 2; }
    int  depth()  const { return indent_; }

    // The overload set: one operator() per alternative. Each one uses
    // `this` to read indent_ and out_, and recurses via std::visit.
    void operator()(IntLit const& i) {
        out_ << std::string(indent_, ' ') << "IntLit(" << i.value << ")\n";
    }
    void operator()(AddOp const& a) {
        out_ << std::string(indent_, ' ') << "AddOp('" << a.op << "')\n";
        enter();
        std::visit(*this, a.children[0]->alt);   // recurse, *this carries state
        std::visit(*this, a.children[1]->alt);
        leave();
    }
    void operator()(MulOp const& m) {
        out_ << std::string(indent_, ' ') << "MulOp('" << m.op << "')\n";
        enter();
        std::visit(*this, m.children[0]->alt);
        std::visit(*this, m.children[1]->alt);
        leave();
    }
};

// Helper: build (1 + 2) * 3 in a self-owning AST.
std::unique_ptr<ExprNode> make_ast_demo() {
    auto mk_int = [](int v) {
        auto n = std::make_unique<ExprNode>();
        n->alt = IntLit{v};
        return n;
    };
    auto mk_bin = [](char op, std::unique_ptr<ExprNode> L,
                     std::unique_ptr<ExprNode> R) {
        auto n = std::make_unique<ExprNode>();
        if (op == '+') {
            AddOp a; a.op = op; a.children[0] = std::move(L); a.children[1] = std::move(R);
            n->alt = std::move(a);
        } else {
            MulOp m; m.op = op; m.children[0] = std::move(L); m.children[1] = std::move(R);
            n->alt = std::move(m);
        }
        return n;
    };
    // (1 + 2) * 3
    return mk_bin('*',
        mk_bin('+', mk_int(1), mk_int(2)),
        mk_int(3));
}

void section2_class_based() {
    hr("2. Class-based visitor that owns its state");

    auto tree = make_ast_demo();

    std::ostringstream oss;
    PrettyPrinter pp(oss);
    std::visit(pp, tree->alt);

    std::cout << "  AST = (1 + 2) * 3\n";
    std::cout << "  Pretty-printed (visitor owns indent_ + ostream):\n";
    std::cout << oss.str();
    std::cout << "  Final depth: " << pp.depth() << "  (must be 0)\n";
    assert(pp.depth() == 0);

    note("The visitor is a value (`pp`), passed to std::visit by value. The "
         "`*this` recursion passes the same visitor -- so the indent_ field "
         "is shared across the whole descent. Two parallel visits with two "
         "different PrettyPrinters would not interfere: each instance has "
         "its own state.");
}

// ---------------------------------------------------------------------------
// 3.  Visitor that BORROWS state via a context reference
//     -- lambdas, but a context object travels alongside.
// ---------------------------------------------------------------------------

// Sometimes the visitor is conceptually a small dispatch table (lambdas),
// but the state is shared infrastructure (output stream, error sink, etc.).
// Passing a context reference into each lambda is cleaner than capturing
// everything by reference. The state lives outside the visitor.

struct PrintContext {
    std::ostream& out;
    int           depth = 0;
    std::string   prefix = "ctx";
};

void section3_context_borrowed() {
    hr("3. Visitor borrows state via a context reference");

    PrintContext ctx{std::cout, 0, "ctx"};

    std::vector<V1> items = {42, 3.14, std::string("hello")};

    auto printer = [&ctx](auto&& alt) {
        // A generic lambda over each alternative. ctx is shared.
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, int>) {
            ctx.out << "  [" << ctx.prefix << "] int = " << alt
                    << " (depth=" << ctx.depth << ")\n";
        } else if constexpr (std::is_same_v<T, double>) {
            ctx.out << "  [" << ctx.prefix << "] double = "
                    << std::fixed << std::setprecision(2) << alt
                    << " (depth=" << ctx.depth << ")\n";
        } else if constexpr (std::is_same_v<T, std::string>) {
            ctx.out << "  [" << ctx.prefix << "] string = \"" << alt
                    << "\" (depth=" << ctx.depth << ")\n";
        }
    };

    ctx.prefix = "first";
    ctx.depth  = 0;
    std::cout << "  Run 1 (prefix='first', depth=0):\n";
    for (auto const& v : items) {
        std::visit(printer, v);
    }

    ctx.prefix = "second";
    ctx.depth  = 7;
    std::cout << "  Run 2 (prefix='second', depth=7):\n";
    for (auto const& v : items) {
        std::visit(printer, v);
    }

    note("The lambda is stateless; the state lives in `ctx` and is borrowed "
         "by reference. Two concurrent visitors reading the same ctx would "
         "race on ctx.depth, but that's the point of borrowing: you only do "
         "this when visits are sequential.");
}

// ---------------------------------------------------------------------------
// 4.  Recursive lambda visitor with `self` capture and shared state
//     -- the rare "I want a lambda but it must recurse" pattern.
// ---------------------------------------------------------------------------

void section4_recursive_lambda() {
    hr("4. Recursive lambda visitor (self-capture + shared state)");

    // A small recursive variant: Number, or Pair<Left, Right> where Left/Right
    // are themselves RecTree. We use a unique_ptr in Pair to break the
    // recursive type.
    struct Tree;
    using TreeAlt = std::variant<int, std::array<std::unique_ptr<Tree>, 2>>;
    struct Tree { TreeAlt alt; };

    // Recursive generic lambda. The trick: capture `self` by value, where
    // self is a `std::function` that takes a TreeAlt and returns whatever
    // we want. That's how you get a lambda to call itself before C++23.
    std::function<int(TreeAlt const&)> sum =  // NOLINT(misc-no-recursion)
        [&sum](TreeAlt const& alt) -> int {
        return std::visit(overloaded{
            // No need to capture `sum` for the int branch.
            [](int v) -> int {
                return v;
            },
            [&sum](std::array<std::unique_ptr<Tree>, 2> const& p) -> int {
                return sum((*p[0]).alt) + sum((*p[1]).alt);
            },
        }, alt);
    };

    // Build a small tree: 1 + (2 + 3) -> 6
    auto mk_leaf = [](int v) {
        Tree t; t.alt = v; return t;
    };
    auto mk_pair = [](std::unique_ptr<Tree> L, std::unique_ptr<Tree> R) {
        Tree t; t.alt = std::array<std::unique_ptr<Tree>, 2>{
            std::move(L), std::move(R)};
        return t;
    };

    Tree tree = mk_pair(
        std::make_unique<Tree>(mk_leaf(1)),
        std::make_unique<Tree>(mk_pair(
            std::make_unique<Tree>(mk_leaf(2)),
            std::make_unique<Tree>(mk_leaf(3)))));

    std::cout << "  Tree: 1 + (2 + 3) (sum should be 6)\n";
    std::cout << "  sum (with self-capture) = " << sum(tree.alt) << "\n";
    assert(sum(tree.alt) == 6);
    std::cout << "  [assertion passed]\n";

    note("The `std::function` self-capture pattern works but it costs an "
         "allocation per lambda and an indirect call per recursion. For "
         "recursive visitors over a fixed type, a class-based visitor "
         "(`operator()` calls `std::visit(*this, ...)`) is faster and the "
         "same length once you get used to it.");
}

// ---------------------------------------------------------------------------
// 5.  Visitor over a std::vector<std::variant<...>> with a fold-style
//     accumulator.  This is the most common stateful shape in real code:
//     a collector visitor that walks a sequence of variants and folds a
//     running result.
// ---------------------------------------------------------------------------

// Forward-declare the recursive variant. The full definition is below.
struct JsonValue;
using JsonArray = std::vector<JsonValue>;
struct JsonValue {
    std::variant<std::monostate, bool, double, std::string, JsonArray> alt;
};

// A "summarize" visitor that, given a JSON-ish value, returns a small
// summary: {n_nulls, n_bools, n_numbers, n_strings, n_arrays, n_elements}.
// The summary is a *value*, but the visitor's operator() overloads all
// produce it -- so a fold over a vector<JsonValue> is just accumulate.
struct Summary {
    std::size_t n_nulls    = 0;
    std::size_t n_bools    = 0;
    std::size_t n_numbers  = 0;
    std::size_t n_strings  = 0;
    std::size_t n_arrays   = 0;
    std::size_t n_elements = 0;  // total scalar/array count
};

struct Summarizer {
    Summary operator()(std::monostate) const {
        Summary s; s.n_nulls = 1; s.n_elements = 1; return s;
    }
    Summary operator()(bool) const {
        Summary s; s.n_bools = 1; s.n_elements = 1; return s;
    }
    Summary operator()(double) const {
        Summary s; s.n_numbers = 1; s.n_elements = 1; return s;
    }
    Summary operator()(std::string const&) const {
        Summary s; s.n_strings = 1; s.n_elements = 1; return s;
    }
    Summary operator()(JsonArray const& arr) const {
        Summary s; s.n_arrays = 1; s.n_elements = 1;
        // Fold over the elements. Each call returns a Summary; merge them.
        for (auto const& e : arr) {
            Summary sub = std::visit(*this, e.alt);
            s.n_nulls    += sub.n_nulls;
            s.n_bools    += sub.n_bools;
            s.n_numbers  += sub.n_numbers;
            s.n_strings  += sub.n_strings;
            s.n_arrays   += sub.n_arrays;
            s.n_elements += sub.n_elements;
        }
        return s;
    }
};

// A small merge helper. std::visit over a vector would otherwise need a
// "combiner" lambda; doing it explicitly is clearer for a first example.
Summary merge(Summary a, Summary const& b) {
    a.n_nulls    += b.n_nulls;
    a.n_bools    += b.n_bools;
    a.n_numbers  += b.n_numbers;
    a.n_strings  += b.n_strings;
    a.n_arrays   += b.n_arrays;
    a.n_elements += b.n_elements;
    return a;
}

void section5_fold_visitor() {
    hr("5. Fold-style stateful visitor (Summary over JsonValue vector)");

    // Build a small JSON-ish document: [null, true, 1.5, "x", [2, [3, 4]]]
    JsonValue doc{std::vector<JsonValue>{
        JsonValue{std::monostate{}},
        JsonValue{true},
        JsonValue{1.5},
        JsonValue{std::string("x")},
        JsonValue{std::vector<JsonValue>{
            JsonValue{2.0},
            JsonValue{std::vector<JsonValue>{
                JsonValue{3.0}, JsonValue{4.0}}},
        }},
    }};

    Summarizer sz;
    Summary total = std::visit(sz, doc.alt);

    std::cout << "  doc: [null, true, 1.5, \"x\", [2, [3, 4]]]\n";
    std::cout << "  total.n_nulls    = " << total.n_nulls    << "\n";
    std::cout << "  total.n_bools    = " << total.n_bools    << "\n";
    std::cout << "  total.n_numbers  = " << total.n_numbers  << "\n";
    std::cout << "  total.n_strings  = " << total.n_strings  << "\n";
    std::cout << "  total.n_arrays   = " << total.n_arrays   << "\n";
    std::cout << "  total.n_elements = " << total.n_elements << "\n";

    // Sanity: 1 null, 1 bool, 4 numbers (1.5, 2, 3, 4),
    // 1 string ("x"), 3 arrays (outer, middle, inner), and 10 elements
    // total (each container counts itself + its leaves).
    assert(total.n_nulls    == 1);
    assert(total.n_bools    == 1);
    assert(total.n_numbers  == 4);
    assert(total.n_strings  == 1);
    assert(total.n_arrays   == 3);
    assert(total.n_elements == 10);
    std::cout << "  [assertions passed]\n";

    note("The visitor is *stateless* -- but the *result* is a stateful "
         "accumulator. That's a useful distinction: the per-element "
         "computation is a pure function from alternative to Summary; the "
         "fold over the container is the stateful part. std::visit is the "
         "pure half, std::accumulate is the stateful half.");
}

// ---------------------------------------------------------------------------
// 6.  Visitor that pushes/pops frames on a std::vector<Frame>
//     -- a "call stack" carried by the visitor.
// ---------------------------------------------------------------------------

// Imagine a JSON pretty-printer that records a stack of `Frame` records
// during its descent. Each Frame is {is_array, index, total}. The visitor
// doesn't recurse "physically" via a stack frame -- it pushes and pops a
// logical frame, which is observable and testable.

struct Frame {
    bool        is_array;
    std::size_t index;
    std::size_t total;
};

struct StackFrameVisitor {
    std::vector<Frame>& stack_;   // BORROWED: caller owns the stack

    void operator()(std::monostate) const {
        // Print path to root: e.g. [array 0/3, array 2/2] null
        print_path();
        std::cout << " null\n";
    }
    void operator()(bool b) const {
        print_path();
        std::cout << " " << (b ? "true" : "false") << "\n";
    }
    void operator()(double d) const {
        print_path();
        std::cout << " " << d << "\n";
    }
    void operator()(std::string const& s) const {
        print_path();
        std::cout << " \"" << s << "\"\n";
    }
    void operator()(JsonArray const& arr) const {
        // Push frame, recurse, pop frame -- a real stack discipline.
        for (std::size_t i = 0; i < arr.size(); ++i) {
            stack_.push_back(Frame{true, i, arr.size()});
            std::visit(*this, arr[i].alt);
            stack_.pop_back();
        }
    }

    void print_path() const {
        std::cout << "  path=[";
        for (std::size_t k = 0; k < stack_.size(); ++k) {
            if (k) std::cout << ", ";
            std::cout << stack_[k].index;
        }
        std::cout << "]";
    }
};

void section6_stack_frame_visitor() {
    hr("6. Visitor with a borrowed call stack (path-tracking)");

    JsonValue doc{std::vector<JsonValue>{
        JsonValue{std::monostate{}},
        JsonValue{true},
        JsonValue{std::vector<JsonValue>{
            JsonValue{1.0},
            JsonValue{std::vector<JsonValue>{
                JsonValue{2.0}, JsonValue{3.0}}},
            JsonValue{std::string("inner")},
        }},
    }};

    std::vector<Frame> stack;
    StackFrameVisitor visitor{stack};

    std::cout << "  doc: [null, true, [1.0, [2.0, 3.0], \"inner\"]]\n";
    std::cout << "  Each scalar prints its path from root:\n";
    std::visit(visitor, doc.alt);

    std::cout << "  Final stack size: " << stack.size()
              << "  (must be 0 -- balanced push/pop)\n";
    assert(stack.size() == 0);
    std::cout << "  [assertion passed: stack balanced]\n";

    note("The visitor borrows the stack (no copies, no allocation per "
         "visit). The stateful pattern here is: *the visitor is a value "
         "type with a reference member*. That's legal because the visitor "
         "itself is short-lived -- it doesn't outlive the call to "
         "std::visit. If the visitor were stored, the reference would be "
         "a dangling trap.");
}

// ---------------------------------------------------------------------------
// 7.  Visitor across a span of variants (cross-references Jun 14's
//     std::span and Jun 10's variant).
//     -- We use std::span if available (C++20), else a hand-rolled
//        psp::Span that's the same shape (pointer + size). Same teaching.
// ---------------------------------------------------------------------------

// A hand-rolled psp::Span<T> identical in shape to the Jun 14 file. Copied
// here so this file builds cleanly under -std=c++17. The Jun 14 file has
// a more fully-featured one; this one is the minimum needed for section 7.
template <class T>
class Span {
    T*          data_ = nullptr;
    std::size_t size_ = 0;
public:
    constexpr Span() noexcept = default;
    constexpr Span(T* p, std::size_t n) noexcept : data_(p), size_(n) {}
    template <std::size_t N>
    constexpr Span(T (&a)[N]) noexcept : data_(a), size_(N) {}

    constexpr T*          data() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr T&          operator[](std::size_t i) const noexcept { return data_[i]; }
    constexpr T*          begin()  const noexcept { return data_; }
    constexpr T*          end()    const noexcept { return data_ + size_; }
};

// A "type-counter" visitor: for each variant alternative, bump a counter.
// State = std::map<std::string, std::size_t>. Stateless overloads, stateful
// container -- the visitor pattern from section 5, applied across a span.
struct TypeCounter {
    std::map<std::string, std::size_t>& counts;

    void count_alternative(std::string_view name) const {
        ++counts[std::string{name}];
    }
    void operator()(std::monostate)        const { count_alternative("null");   }
    void operator()(bool)                 const { count_alternative("bool");   }
    void operator()(double)               const { count_alternative("number"); }
    void operator()(std::string const&)   const { count_alternative("string"); }
    void operator()(JsonArray const&)     const {
        // Don't descend into arrays here; we only want top-level counts.
        count_alternative("array");
    }
};

void section7_span_of_variants() {
    hr("7. Visitor across a span of variants (Jun 10 + Jun 14)");

    // A std::vector<JsonValue> is the natural shape; a span over it lets
    // us pass it around without losing size information.
    std::vector<JsonValue> doc = {
        JsonValue{std::monostate{}},
        JsonValue{true},
        JsonValue{false},
        JsonValue{1.0},
        JsonValue{2.5},
        JsonValue{3.0},
        JsonValue{std::string("a")},
        JsonValue{std::string("b")},
        JsonValue{std::vector<JsonValue>{JsonValue{1.0}, JsonValue{2.0}}},
    };

    Span<JsonValue> view(doc.data(), doc.size());

    std::map<std::string, std::size_t> counts;
    TypeCounter tc{counts};
    for (auto const& v : view) {
        std::visit(tc, v.alt);
    }

    std::cout << "  Span over a std::vector<JsonValue>, size="
              << view.size() << "\n";
    for (auto const& [name, n] : counts) {
        std::cout << "    " << name << ": " << n << "\n";
    }

    assert(counts["null"]   == 1);
    assert(counts["bool"]   == 2);
    assert(counts["number"] == 3);
    assert(counts["string"] == 2);
    assert(counts["array"]  == 1);
    std::cout << "  [assertions passed]\n";

    note("This is the *real* use case for combining std::visit with a span: "
         "a function like `void count_types(Span<JsonValue const> values, "
         "TypeCounter& tc)` accepts any contiguous range (vector, C-array, "
         "another span) and dispatches per alternative. The signature is "
         "cheaper than `std::vector<JsonValue> const&` (no allocator, no "
         "size, no ownership), the variant dispatch is in the function, and "
         "the state lives where the caller wants it.");
}

}  // namespace psp

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== std::visit with stateful visitors (C++17) ===\n";
    psp::section1_motivation();
    psp::section2_class_based();
    psp::section3_context_borrowed();
    psp::section4_recursive_lambda();
    psp::section5_fold_visitor();
    psp::section6_stack_frame_visitor();
    psp::section7_span_of_variants();
    std::cout << "\n=== done ===\n";
    return 0;
}
