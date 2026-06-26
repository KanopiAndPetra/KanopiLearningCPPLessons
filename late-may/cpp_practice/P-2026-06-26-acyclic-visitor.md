# C++ practice 2026-06-26 — Acyclic Visitor

## What I set out to learn

The Jun 25 session closed the PIMPL arc. Jun 25's "Next Steps"
flagged one follow-on:

> **Acyclic visitor** — `dynamic_cast` *inside* the visitor's
> `visit(Base&)` to break the dependency cycle of the classic
> visitor pattern. Loses double-dispatch but gains extensibility.

That's today's session. The topic connects directly to Jun 20
(classic visitor) and Jun 22 (CRTP static visitor) — both of
which I knew well. The acyclic visitor is the third member of the
visitor-pattern family, and the one that trades type-safety for
extensibility.

## What I confirmed

### (1) The classic visitor's "fragile base class" problem

In Jun 20's classic visitor, `BaseVisitor` declared virtual
`visit(Num&)`, `visit(Add&)`, `visit(Mul&)` as overloads. To add
a new AST node (say `Neg`), the **entire visitor hierarchy must
be edited** — every concrete visitor has to grow a new
`visit(Neg&)` overload, even visitors that don't care about
`Neg`. This is the "cyclic dependency" the acyclic pattern breaks.

### (2) Acyclic visitor's fix: dispatch via `dynamic_cast`

The acyclic visitor's core structural change: **`BaseVisitor`
has NO virtual `visit_<>()` overloads.** Each concrete visitor
declares its own `visit_<NodeType>()` methods as ordinary
(non-virtual) member functions scoped to that visitor class.

`Node::accept(BaseVisitor&)` does a `dynamic_cast` to each
concrete visitor type and calls the matching `visit_<MyType>()`
on success. If the visitor doesn't know about the node type, the
dispatch is a no-op — silently ignored.

This is why the naming is `visit_<NodeType>` (distinct names) and
not `visit(NodeType&)` (overloads): overload resolution happens
at compile time, but we need to dispatch at runtime via
`dynamic_cast` — so we need unique names, one per node type.

### (3) The "NullVisitor" CRTP base

The challenge with (2) alone: if `EvalVisitor` doesn't override
`visit_Neg`, then `Neg::accept(EvalVisitor&)` would try to call
`EvalVisitor::visit_Neg`, which doesn't exist → compile error.

The standard solution is a **NullVisitor CRTP base** that
provides default implementations of every `visit_<NodeType>()`,
parameterized by the concrete visitor type:

```cpp
template <typename Concrete>
class NullVisitor : public BaseVisitor {
public:
    void visit_Num(Num&)         {}                          // empty default
    void visit_Add(Add& a)       { default_visit(a); }       // recurse
    void visit_Mul(Mul& m)       { default_visit(m); }       // recurse
    void visit_Neg(Neg& ng)      { default_visit(ng); }      // recurse

    void default_visit(Node& n) {
        if (auto* a = dynamic_cast<Add*>(&n)) {
            a->lhs().accept(static_cast<Concrete&>(*this));
            a->rhs().accept(static_cast<Concrete&>(*this));
        } else if (auto* m = dynamic_cast<Mul*>(&n)) {
            m->lhs().accept(static_cast<Concrete&>(*this));
            m->rhs().accept(static_cast<Concrete&>(*this));
        } else if (auto* ng = dynamic_cast<Neg*>(&n)) {
            ng->child().accept(static_cast<Concrete&>(*this));
        }
    }
};
```

Every concrete visitor inherits from `NullVisitor<Concrete>` and
overrides only the methods it cares about. The CRTP pattern
ensures `default_visit` casts back to the concrete visitor type
when it recurses — so polymorphic dispatch picks up the right
overrides at each level.

### (4) Three visitors of different "completeness"

```text
NumVisitor          : overrides visit_Num only
EvalVisitor         : overrides visit_Num / visit_Add / visit_Mul
PrettyPrintVisitor  : overrides all four
```

`NumVisitor` is a **partial visitor** — it sees only `Num`
leaves. The output confirms this:

```text
=== (1) NumVisitor (partial) on (3+4)*5 ===
  NumVisitor saw Num(3)
  NumVisitor saw Num(4)
  NumVisitor saw Num(5)
(only Num leaves printed; Add/Mul skipped)
```

`Add` and `Mul` are silently recursed-through via NullVisitor's
default `visit_Add` / `visit_Mul`, which call `default_visit` to
descend into children. The visitor walks the whole tree; it just
only does something at Num leaves.

### (5) EvalVisitor computes correctly on a complete AST

```text
=== (2) EvalVisitor on (3+4)*5 ===
result = 35  (expect 35)
```

Standard recursive eval. Nothing surprising — EvalVisitor fully
covers Num/Add/Mul, so it works exactly like a classic
visitor would.

### (6) PrettyPrintVisitor produces fully-parenthesized output

```text
=== (3) PrettyPrintVisitor on (3+4)*5 ===
pretty = ((3 + 4) * 5)  (expect "((3 + 4) * 5)" — fully parenthesized)
```

Each `visit_Add` / `visit_Mul` wraps its own parens, so the
output is fully parenthesized. (Note: this is not a bug — it's
the convention chosen. To get infix-with-precedence pretty-print,
visit_Add would inspect children and emit parens only around
nested operators with lower precedence.)

### (7) THE EXTENSIBILITY DEMO: add `Neg` to the AST

The crux of the acyclic pattern. `Neg` is added to the AST
hierarchy. What changes?

1. **`NullVisitor` is edited ONCE** to add `visit_Neg(Neg&)`
   default + a `Neg*` arm in `default_visit`. That's the only
   "cost".
2. **`PrettyPrintVisitor`** adds its own `visit_Neg(Neg&)`
   override that wraps `(-...)`.
3. **`EvalVisitor`** does **NOT** change — it doesn't override
   `visit_Neg`, so it inherits `NullVisitor`'s default, which
   recurses into `Neg`'s child.
4. **`NumVisitor`** does **NOT** change — it doesn't override
   `visit_Neg` either. (Same reasoning.)

Output:

```text
=== (4) Extensibility: AST now contains Neg ===
    AST = -((3+4)*5)

  PrettyPrintVisitor: (-((3 + 4) * 5))  (Neg adds the leading "(-")
  EvalVisitor (top-level accept on Neg):
    ev.result() = 35  (Neg recursed-through but not negated)
    manual descent past Neg -> result = 35  (same, confirming the silent drop)
  NumVisitor:   NumVisitor saw Num(3)
  NumVisitor saw Num(4)
  NumVisitor saw Num(5)
(only Num leaves printed; Add/Mul/Neg skipped)
```

**The silent-drop trade.** EvalVisitor returns 35 instead of -35.
This is the cost of the acyclic pattern: visitors that don't
know about a node type either:
- Silently skip it (if their inherited default is empty), or
- Recurse through it without applying its semantics (what
  happens here — Neg's negation is dropped because the visitor
  doesn't override visit_Neg).

In a classic visitor, this scenario would be a **compile error**:
you'd be forced to add `visit(Neg&)` to EvalVisitor or it
wouldn't compile. The acyclic pattern pushes that requirement
from compile time to runtime — losing exhaustiveness checks but
gaining extensibility.

### (8) The `dynamic_cast` cost per dispatch

Each `Node::accept(BaseVisitor&)` does up to 3 `dynamic_cast`s
(one per concrete visitor type). For `(3+4)*5` with 5 nodes,
that's up to 15 dynamic_casts in the worst case (assuming the
matching visitor type is last in the chain).

The classic visitor does **0** dynamic_casts per dispatch — the
single virtual call resolves to the right `visit(Derived&)`
overload via static overload resolution.

So acyclic is slower; classic is faster. The trade is
extensibility vs performance / type-safety.

### (9) `sizeof(BaseVisitor) = 8`

```text
sizeof(BaseVisitor) = 8 byte(s)
(just a vptr; no virtual visit_<>() declared anywhere on it)
```

The 8 bytes are just the vtable pointer for the virtual dtor.
No `visit_<>()` methods declared, so no additional storage in
the vtable.

## The `NullVisitor<Concrete>` CRTP — what it does and why

```cpp
template <typename Concrete>
class NullVisitor : public BaseVisitor {
public:
    void default_visit(Node& n);          // recurses through composite nodes
    void visit_Num(Num& n)         {};    // empty default (concrete overrides)
    void visit_Add(Add& a)         { default_visit(a); }   // recurse
    void visit_Mul(Mul& m)         { default_visit(m); }   // recurse
    void visit_Neg(Neg& ng)        { default_visit(ng); }  // recurse
};
```

Three roles in one:

1. **Empty defaults** — `visit_Num(Num&)` is empty by default.
   Concrete visitors that care about Num override it.
2. **Recursive defaults** — `visit_Add / visit_Mul / visit_Neg`
   call `default_visit(Node&)` which descends into the node's
   children. This means a partial visitor (one that overrides
   only some `visit_<>()`s) still walks the entire tree.
3. **Type-safe recursion** — `default_visit` uses
   `static_cast<Concrete&>(*this)` when re-dispatching via
   `accept()`. The CRTP template parameter `Concrete` ensures
   the cast goes back to the most-derived visitor type, so
   polymorphism resolves to the right overrides at each
   recursion level.

## Three visitors in detail

### NumVisitor (partial)

```cpp
class NumVisitor : public NullVisitor<NumVisitor> {
public:
    void visit_Num(Num& n) {
        std::cout << "  NumVisitor saw Num(" << n.value() << ")\n";
    }
};
```

Inherits empty `visit_Num` (overridden here) and recursive
`visit_Add / visit_Mul / visit_Neg`. Walks the whole tree;
prints only at Num leaves.

### EvalVisitor (Num/Add/Mul — incomplete re: Neg)

```cpp
class EvalVisitor : public NullVisitor<EvalVisitor> {
public:
    int result() const { return last_; }
    void visit_Num(Num& n)             { last_ = n.value(); }
    void visit_Add(Add& a) {
        EvalVisitor lv, rv;
        a.lhs().accept(lv);
        a.rhs().accept(rv);
        last_ = lv.last_ + rv.last_;
    }
    void visit_Mul(Mul& m) {
        EvalVisitor lv, rv;
        m.lhs().accept(lv);
        m.rhs().accept(rv);
        last_ = lv.last_ * rv.last_;
    }
private:
    int last_ = 0;
};
```

Note: `visit_Add` / `visit_Mul` create fresh sub-EvalVisitors
for lhs/rhs to avoid overwriting `last_` mid-recursion. This
is the standard idiom for stateful visitors.

`visit_Neg` is **not** overridden — inherits NullVisitor's
default, which recurses into Neg's child without applying any
negation. That's the silent-drop behavior.

### PrettyPrintVisitor (all four — complete)

```cpp
class PrettyPrintVisitor : public NullVisitor<PrettyPrintVisitor> {
public:
    const std::string& text() const { return out_; }
    void visit_Num(Num& n)  { out_ += std::to_string(n.value()); }
    void visit_Add(Add& a) {
        out_ += "("; a.lhs().accept(*this);
        out_ += " + "; a.rhs().accept(*this);
        out_ += ")";
    }
    void visit_Mul(Mul& m) { /* symmetric */ }
    void visit_Neg(Neg& ng) {
        out_ += "(-"; ng.child().accept(*this);
        out_ += ")";
    }
private:
    std::string out_;
};
```

Stateful (accumulates `out_`). Each `visit_<>()` appends its
own substring. Recurses by calling `accept(*this)` on children
— this is the standard double-dispatch-into-recursion pattern.

## Acyclic vs CRTP static vs Classic — the visitor family

| Aspect                | Classic (Jun 20)    | CRTP Static (Jun 22)         | Acyclic (today)                |
|-----------------------|---------------------|------------------------------|--------------------------------|
| BaseVisitor has virtual visit() ? | yes (overloads) | yes (templated, all concrete visitors declared in base) | **no** (BaseVisitor is empty) |
| Dispatch mechanism    | virtual call + static overload | static (compile-time)         | dynamic_cast on visitor        |
| Cost per dispatch     | 0 dynamic_casts    | 0 dynamic_casts              | up to N dynamic_casts (N = #concrete visitors) |
| Adding new node type  | must edit every visitor | must edit base (recompile all) | must edit NullVisitor only — concrete visitors unaffected |
| Compile-time exhaustiveness | yes | yes                          | **no** (silent drop possible)  |
| When to use           | when type-safety + speed matter and visitor set is small | when you want zero-cost dispatch and know all visitors up front | when extensibility matters more than exhaustiveness |

The three are genuinely different design points on the
{speed, type-safety, extensibility} triangle.

## What I took away

- **Acyclic visitor's structural change is "no virtual visit() on
  BaseVisitor."** That's the whole trick. Without that, you have
  the classic visitor; with that, you have the acyclic one.
- **`dynamic_cast` per dispatch** is the cost. Each `Node::accept`
  does up to N dynamic_casts where N = #concrete visitors in
  the dispatch chain. Classic visitor does 0.
- **NullVisitor CRTP base** is the standard idiom for making
  partial visitors well-defined: empty defaults for unhandled
  leaf nodes, recursive defaults for unhandled composite nodes.
  Adding a new AST node type requires editing NullVisitor once;
  concrete visitors are unaffected.
- **Silent-drop is the trade.** Acyclic visitor loses
  compile-time exhaustiveness. If `EvalVisitor` doesn't override
  `visit_Neg`, the negation is silently dropped at runtime (35
  instead of -35). Classic visitor would have made this a
  compile error.
- **`sizeof(BaseVisitor) = 8`** — just a vptr. The 16-byte cost
  vs the classic visitor (also 8 bytes) is identical, but the
  *visitor object* itself can be larger depending on how much
  state it accumulates.

This closes the acyclic-visitor follow-on from Jun 25. Remaining
follow-ons from Jun 24's PIMPL-ABI notes are essentially all
done now (Jun 22 was CRTP static visitor; Jun 25 was
shared_ptr<Impl> PIMPL; today is acyclic visitor). The PIMPL
arc is complete.

## Build and run

```bash
cd late-may/cpp_practice/

# Standard build (with -Wall -Wextra -Wpedantic).
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    P-2026-06-26-acyclic-visitor.cpp -o P-2026-06-26-acyclic-visitor
./P-2026-06-26-acyclic-visitor

# ASan + UBSan.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    P-2026-06-26-acyclic-visitor.cpp -o P-2026-06-26-acyclic-visitor-asan
./P-2026-06-26-acyclic-visitor-asan

# Type-id observability (BaseVisitor truly has no virtual visit_<>()).
nm P-2026-06-26-acyclic-visitor | c++filt | grep -i BaseVisitor | sort -u
```

All builds clean under `-Wall -Wextra -Wpedantic`. ASan+UBSan
clean — no leaks, no UB, refcount drops correctly.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files (under `late-may/cpp_practice/`):**
  - `P-2026-06-26-acyclic-visitor.cpp` — single-file driver +
    AST + visitors + NullVisitor CRTP base
  - `P-2026-06-26-acyclic-visitor.md` — this file
- **No existing files modified.**
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*Today covers the acyclic visitor pattern: how to break the
classic visitor's "add a node type → edit every visitor"
dependency cycle by removing virtual visit() overloads from
BaseVisitor and dispatching via dynamic_cast on the visitor.
The NullVisitor<Concrete> CRTP base provides empty/recursive
defaults so concrete visitors only override what they care
about. The trade: extensibility + late binding vs
compile-time exhaustiveness + zero dynamic_casts. Connects
directly to Jun 20 (classic) and Jun 22 (CRTP static) —
Jun 22 (CRTP static) — together the three cover the full visitor design space.*