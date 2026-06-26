// P-2026-06-26: Acyclic Visitor
//
// Topic: The "acyclic visitor" pattern (Robert Martin / Kevlin Henney).
//         Solves the "fragile base class" / extensibility problem of
//         the classic visitor (Jun 20) by removing the abstract visit()
//         overload in the visitor interface. Each visitor only declares
//         visit_<NodeType>() for the node types it cares about; nodes
//         that no visitor can handle are silently skipped.
//
//         The KEY implementation trick used here: a "NullVisitor" CRTP
//         base provides empty default implementations of visit_X for
//         every node type. Concrete visitors inherit from it and
//         override only the methods they care about. This way:
//           - Each Node::accept(BaseVisitor&) does ONE dynamic_cast on
//             the visitor; if it succeeds, it calls visit_<MyType>().
//           - Unhandled node types fall through to NullVisitor's empty
//             default — silently ignored at runtime.
//           - Adding a new AST node (like Neg) requires editing only
//             NullVisitor (to add an empty default visit_Neg), NOT
//             every concrete visitor. This is the "acyclic" property:
//             no cycle through which every visitor must be updated.
//
// What this program demonstrates:
//   1. The classic visitor's "add a node -> edit every visitor" pain.
//   2. The acyclic visitor's fix: visitors only know the nodes they care
//      about. Adding a new node type does not break existing visitors
//      that don't care about it.
//   3. The cost: dynamic_cast per dispatch (vs static overload resolution
//      in the classic visitor).
//   4. A clean AST example (Num/Add/Mul/Neg) with three visitors of
//      different "completeness": NumVisitor (only Num), EvalVisitor
//      (Num/Add/Mul), PrettyPrintVisitor (all four).
//   5. Extensibility: add a Neg to the AST; NumVisitor and EvalVisitor
//      keep working unchanged — Neg is silently dropped from their walks.
//
// Build and run:
//   g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//       P-2026-06-26-acyclic-visitor.cpp -o P-2026-06-26-acyclic-visitor
//   ./P-2026-06-26-acyclic-visitor

#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

// ---------------------------------------------------------------------------
// Forward declarations of AST nodes.
// ---------------------------------------------------------------------------
class Node;
class Num;
class Add;
class Mul;
class Neg;

// ---------------------------------------------------------------------------
// 1. BaseVisitor. Crucially, it has NO virtual visit_<>() overloads.
//    Each concrete visitor introduces its OWN set of visit_<NodeType>()
//    methods as ordinary (non-virtual) member functions, scoped to that
//    visitor class.
// ---------------------------------------------------------------------------
class BaseVisitor {
public:
    virtual ~BaseVisitor() = default;
    // No visit_<>() overloads. By design.
};

// ---------------------------------------------------------------------------
// 2. NullVisitor — provides default implementations of every
//    visit_<NodeType>() so concrete visitors can override only what they
//    care about. This is the CRTP base that gives us the acyclic property.
//
//    The defaults RECURSE into children via default_visit(Node&). That
//    way a partial visitor (one that overrides only some visit_<>()s)
//    still traverses the entire AST — the recursion is handled by the
//    default methods, not the overrides.
//
//    Visitor method bodies are defined out-of-line AFTER the AST nodes
//    below — default_visit calls methods (lhs(), rhs(), child()) that
//    require the complete AST types.
// ---------------------------------------------------------------------------
class Num;
class Add;
class Mul;
class Neg;

template <typename Concrete>
class NullVisitor : public BaseVisitor {
public:
    // default_visit recurses through any composite node, calling accept
    // on each child with `*this` (this visitor). Subclasses can override
    // default_visit to skip children entirely (leaf-only visitors) or to
    // preprocess children (stateful visitors).
    void default_visit(Node& n);

    void visit_Num(Num&);          // defined below
    void visit_Add(Add&);          // defined below
    void visit_Mul(Mul&);          // defined below
    void visit_Neg(Neg&);          // defined below
};

// ---------------------------------------------------------------------------
// 3. Concrete visitors — inherit from NullVisitor<Concrete> so they
//    automatically get no-op defaults for every visit_<NodeType>(). They
//    override only the methods they actually want to handle.
// ---------------------------------------------------------------------------

// (a) Partial visitor: only knows Num.
class NumVisitor : public NullVisitor<NumVisitor> {
public:
    void visit_Num(Num& n);
    // visit_Add / visit_Mul / visit_Neg are inherited from NullVisitor
    // as empty no-ops.
};

// (b) EvalVisitor: knows Num/Add/Mul. Will silently ignore Neg.
class EvalVisitor : public NullVisitor<EvalVisitor> {
public:
    int result() const { return last_; }
    void visit_Num(Num& n);
    void visit_Add(Add& a);
    void visit_Mul(Mul& m);
    // visit_Neg is inherited as a no-op — the acyclic pattern lets us
    // accept that.

private:
    int last_ = 0;
};

// (c) PrettyPrintVisitor: knows all four.
class PrettyPrintVisitor : public NullVisitor<PrettyPrintVisitor> {
public:
    const std::string& text() const { return out_; }
    void visit_Num(Num& n);
    void visit_Add(Add& a);
    void visit_Mul(Mul& m);
    void visit_Neg(Neg& ng);

private:
    std::string out_;
};

// ---------------------------------------------------------------------------
// 4. AST node hierarchy.
//    Node::accept(BaseVisitor&) does ONE dynamic_cast to its concrete
//    visitor type and calls the matching visit_<Derived>() method.
// ---------------------------------------------------------------------------
class Node {
public:
    virtual ~Node() = default;
    virtual void accept(BaseVisitor& v) = 0;
};

class Num : public Node {
public:
    explicit Num(int v) : value_(v) {}
    int value() const { return value_; }

    void accept(BaseVisitor& v) override;
private:
    int value_;
};

class Add : public Node {
public:
    Add(std::unique_ptr<Node> lhs, std::unique_ptr<Node> rhs)
        : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
    Node& lhs() { return *lhs_; }
    Node& rhs() { return *rhs_; }

    void accept(BaseVisitor& v) override;
private:
    std::unique_ptr<Node> lhs_, rhs_;
};

class Mul : public Node {
public:
    Mul(std::unique_ptr<Node> lhs, std::unique_ptr<Node> rhs)
        : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
    Node& lhs() { return *lhs_; }
    Node& rhs() { return *rhs_; }

    void accept(BaseVisitor& v) override;
private:
    std::unique_ptr<Node> lhs_, rhs_;
};

class Neg : public Node {
public:
    explicit Neg(std::unique_ptr<Node> child) : child_(std::move(child)) {}
    Node& child() { return *child_; }

    void accept(BaseVisitor& v) override;
private:
    std::unique_ptr<Node> child_;
};

// ---------------------------------------------------------------------------
// 5. NullVisitor member-function definitions. Num/Add/Mul/Neg are now
//    complete types, so default_visit() can call lhs()/rhs()/child().
//    The defaults RECURSE into children so partial visitors still
//    traverse the entire AST.
// ---------------------------------------------------------------------------
template <typename Concrete>
void NullVisitor<Concrete>::default_visit(Node& n) {
    if (auto* a = dynamic_cast<Add*>(&n)) {
        a->lhs().accept(static_cast<Concrete&>(*this));
        a->rhs().accept(static_cast<Concrete&>(*this));
    } else if (auto* m = dynamic_cast<Mul*>(&n)) {
        m->lhs().accept(static_cast<Concrete&>(*this));
        m->rhs().accept(static_cast<Concrete&>(*this));
    } else if (auto* ng = dynamic_cast<Neg*>(&n)) {
        ng->child().accept(static_cast<Concrete&>(*this));
    }
    // Num leaves have no children to recurse into.
}

template <typename Concrete>
void NullVisitor<Concrete>::visit_Num(Num&)         {}
template <typename Concrete>
void NullVisitor<Concrete>::visit_Add(Add& a)       { default_visit(a); }
template <typename Concrete>
void NullVisitor<Concrete>::visit_Mul(Mul& m)       { default_visit(m); }
template <typename Concrete>
void NullVisitor<Concrete>::visit_Neg(Neg& ng)      { default_visit(ng); }

// ---------------------------------------------------------------------------
// 6. Concrete visitor member-function definitions. They override only the
//    visit_<>()s they care about. The other visit_<>()s are inherited from
//    NullVisitor<Concrete> as either no-ops (visit_Num) or recursive
//    descent (visit_Add / visit_Mul / visit_Neg).
// ---------------------------------------------------------------------------
void NumVisitor::visit_Num(Num& n) {
    std::cout << "  NumVisitor saw Num(" << n.value() << ")\n";
}

void EvalVisitor::visit_Num(Num& n) {
    last_ = n.value();
}
void EvalVisitor::visit_Add(Add& a) {
    EvalVisitor lv, rv;
    a.lhs().accept(lv);
    a.rhs().accept(rv);
    last_ = lv.last_ + rv.last_;
}
void EvalVisitor::visit_Mul(Mul& m) {
    EvalVisitor lv, rv;
    m.lhs().accept(lv);
    m.rhs().accept(rv);
    last_ = lv.last_ * rv.last_;
}

void PrettyPrintVisitor::visit_Num(Num& n) {
    out_ += std::to_string(n.value());
}
void PrettyPrintVisitor::visit_Add(Add& a) {
    out_ += "(";
    a.lhs().accept(*this);
    out_ += " + ";
    a.rhs().accept(*this);
    out_ += ")";
}
void PrettyPrintVisitor::visit_Mul(Mul& m) {
    out_ += "(";
    m.lhs().accept(*this);
    out_ += " * ";
    m.rhs().accept(*this);
    out_ += ")";
}
void PrettyPrintVisitor::visit_Neg(Neg& ng) {
    out_ += "(-";
    ng.child().accept(*this);
    out_ += ")";
}

// ---------------------------------------------------------------------------
// 7. Node::accept() — single dynamic_cast per node. If the visitor is the
//    expected concrete type, call visit_<MyType>; otherwise the dispatch
//    is a no-op (NullVisitor's empty default would be called if we
//    dispatched through the base, but here we skip even that).
// ---------------------------------------------------------------------------
void Num::accept(BaseVisitor& v) {
    if (auto* p = dynamic_cast<NumVisitor*>(&v))          { p->visit_Num(*this); return; }
    if (auto* p = dynamic_cast<EvalVisitor*>(&v))        { p->visit_Num(*this); return; }
    if (auto* p = dynamic_cast<PrettyPrintVisitor*>(&v)) { p->visit_Num(*this); return; }
    // else: this visitor doesn't know Num -> silently ignored.
}
void Add::accept(BaseVisitor& v) {
    if (auto* p = dynamic_cast<NumVisitor*>(&v))          { p->visit_Add(*this); return; }
    if (auto* p = dynamic_cast<EvalVisitor*>(&v))        { p->visit_Add(*this); return; }
    if (auto* p = dynamic_cast<PrettyPrintVisitor*>(&v)) { p->visit_Add(*this); return; }
}
void Mul::accept(BaseVisitor& v) {
    if (auto* p = dynamic_cast<NumVisitor*>(&v))          { p->visit_Mul(*this); return; }
    if (auto* p = dynamic_cast<EvalVisitor*>(&v))        { p->visit_Mul(*this); return; }
    if (auto* p = dynamic_cast<PrettyPrintVisitor*>(&v)) { p->visit_Mul(*this); return; }
}
void Neg::accept(BaseVisitor& v) {
    if (auto* p = dynamic_cast<NumVisitor*>(&v))          { p->visit_Neg(*this); return; }
    if (auto* p = dynamic_cast<EvalVisitor*>(&v))        { p->visit_Neg(*this); return; }
    if (auto* p = dynamic_cast<PrettyPrintVisitor*>(&v)) { p->visit_Neg(*this); return; }
}

// ---------------------------------------------------------------------------
// 8. Helpers to build test ASTs.
// ---------------------------------------------------------------------------
static std::unique_ptr<Node> make_simple() {
    auto add = std::make_unique<Add>(
        std::make_unique<Num>(3),
        std::make_unique<Num>(4));
    return std::make_unique<Mul>(std::move(add), std::make_unique<Num>(5));
}

static std::unique_ptr<Node> make_with_neg() {
    auto add = std::make_unique<Add>(
        std::make_unique<Num>(3),
        std::make_unique<Num>(4));
    auto mul = std::make_unique<Mul>(std::move(add), std::make_unique<Num>(5));
    return std::make_unique<Neg>(std::move(mul));
}

// ---------------------------------------------------------------------------
// 9. Driver
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== Acyclic Visitor: BaseVisitor has NO virtual visit_<>() ===\n";
    std::cout << "typeid(BaseVisitor) is " << typeid(BaseVisitor).name() << "\n";
    std::cout << "(no virtual visit_<>() declared on BaseVisitor by design)\n";
    std::cout << "Concrete visitors inherit from NullVisitor<Concrete> which\n";
    std::cout << "provides empty default implementations of every visit_<>().\n\n";

    // -------------------------------------------------------------------
    // (1) NumVisitor (partial) on (3+4)*5.
    //     Only Num leaves produce output; Add and Mul are silently
    //     skipped (their accept() finds no concrete NumVisitor override
    //     applicable, so the dispatch is a no-op).
    // -------------------------------------------------------------------
    {
        std::cout << "=== (1) NumVisitor (partial) on (3+4)*5 ===\n";
        auto ast = make_simple();
        NumVisitor nv;
        ast->accept(nv);
        std::cout << "(only Num leaves printed; Add/Mul skipped)\n\n";
    }

    // -------------------------------------------------------------------
    // (2) EvalVisitor on (3+4)*5.
    // -------------------------------------------------------------------
    {
        std::cout << "=== (2) EvalVisitor on (3+4)*5 ===\n";
        auto ast = make_simple();
        EvalVisitor ev;
        ast->accept(ev);
        std::cout << "result = " << ev.result() << "  (expect 35)\n\n";
    }

    // -------------------------------------------------------------------
    // (3) PrettyPrintVisitor on (3+4)*5 -> "((3 + 4) * 5)"
    //     (Fully parenthesized — every node's visit_<>() wraps its own
    //     parens. Add wraps with "(a + b)", Mul wraps with "(a * b)".)
    // -------------------------------------------------------------------
    {
        std::cout << "=== (3) PrettyPrintVisitor on (3+4)*5 ===\n";
        auto ast = make_simple();
        PrettyPrintVisitor pp;
        ast->accept(pp);
        std::cout << "pretty = " << pp.text()
                  << "  (expect \"((3 + 4) * 5)\" — fully parenthesized)\n\n";
    }

    // -------------------------------------------------------------------
    // (4) EXTENSIBILITY: add Neg to the AST.
    //     - PrettyPrintVisitor handles Neg -> "(-((3 + 4) * 5))".
    //     - EvalVisitor has NO visit_Neg override. NullVisitor's default
    //       visit_Neg recurses through default_visit, which descends
    //       into Neg's child (the Mul). The Mul's accept() then dispatches
    //       back to EvalVisitor::visit_Mul, which computes 35.
    //       So EvalVisitor actually DOES evaluate the subtree under Neg
    //       — it just doesn't know Neg exists. The negation is silently
    //       dropped (35 instead of -35).
    //     - NumVisitor still works exactly as before — no recompile,
    //       no edit of NumVisitor. The acyclic pattern lets it ignore
    //       Neg silently while still seeing all Num leaves.
    //     - NullVisitor<> was edited ONCE (to add visit_Neg default)
    //       — that's the only "cost" of adding Neg.
    // -------------------------------------------------------------------
    {
        std::cout << "=== (4) Extensibility: AST now contains Neg ===\n";
        std::cout << "    AST = -((3+4)*5)\n\n";

        // (a) PrettyPrintVisitor — knows all four.
        {
            std::cout << "  PrettyPrintVisitor: ";
            auto ast = make_with_neg();
            PrettyPrintVisitor pp;
            ast->accept(pp);
            std::cout << pp.text()
                      << "  (expect \"(-((3 + 4) * 5))\" — Neg adds the leading \"(-\")\n";
        }

        // (b) EvalVisitor — incomplete w.r.t. Neg, but NullVisitor's
        //     default_visit recurses through Neg into its child, and the
        //     child's accept() re-dispatches to EvalVisitor::visit_Mul
        //     which computes 35. The negation is silently dropped.
        {
            std::cout << "  EvalVisitor (top-level accept on Neg):\n";
            auto ast = make_with_neg();
            EvalVisitor ev;
            ast->accept(ev);
            std::cout << "    ev.result() = " << ev.result()
                      << "  (expect 35; Neg recursed-through but not negated)\n";

            // Confirm: manual descent bypassing Neg gives the same answer.
            Neg* ng = dynamic_cast<Neg*>(ast.get());
            if (ng) {
                EvalVisitor inner;
                ng->child().accept(inner);
                std::cout << "    manual descent past Neg -> result = "
                          << inner.result() << "  (expect 35 — same, confirming the silent drop)\n";
            }
        }

        // (c) NumVisitor — partial; prints only Num leaves.
        {
            std::cout << "  NumVisitor: ";
            auto ast = make_with_neg();
            NumVisitor nv;
            ast->accept(nv);
            std::cout << "(only Num leaves printed; Add/Mul/Neg skipped)\n";
        }
        std::cout << "\n";
    }

    // -------------------------------------------------------------------
    // (5) dynamic_cast cost per dispatch.
    //     Each Node::accept(BaseVisitor&) does up to 3 dynamic_casts
    //     (one per concrete visitor type). For (3+4)*5 with 5 nodes:
    //     up to 5 * 3 = 15 dynamic_casts. The classic visitor does 0
    //     dynamic_casts (one virtual call -> static overload resolution).
    //     Acyclic is therefore slower; the trade is extensibility.
    // -------------------------------------------------------------------
    {
        std::cout << "=== (5) dynamic_cast cost per dispatch ===\n";
        std::cout << "Each Node::accept(BaseVisitor&) does up to 3 dynamic_casts\n";
        std::cout << "(one per concrete visitor type we care about).\n";
        std::cout << "For (3+4)*5 with 5 nodes: up to 5 * 3 = 15 dynamic_casts.\n";
        std::cout << "Classic visitor: 0 dynamic_casts per dispatch.\n";
        std::cout << "(Acyclic is slower but extensible; classic is faster but fragile.)\n\n";
    }

    // -------------------------------------------------------------------
    // (6) sizeof(BaseVisitor).
    // -------------------------------------------------------------------
    {
        std::cout << "=== (6) sizeof(BaseVisitor) ===\n";
        std::cout << "sizeof(BaseVisitor) = " << sizeof(BaseVisitor) << " byte(s)\n";
        std::cout << "(just a vptr; no virtual visit_<>() declared anywhere on it)\n";
    }

    return 0;
}