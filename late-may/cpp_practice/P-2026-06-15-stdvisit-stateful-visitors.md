# C++ practice 2026-06-15 — `std::visit` with stateful visitors

## What I set out to learn

On Jun 10 I covered the *stateless* shape of `std::visit`:
a `std::variant<A, B, C>` dispatched to a pack of lambdas bundled
by the `overloaded{...}` helper. That works for short
self-contained operations ("int? print it. string? print it."),
but the moment a visitor needs to "remember" something between
calls — current indentation depth, a running total, an output
stream, a stack of in-progress frames, a context pointer, an
error sink — it stops being a stateless lambda and starts
being a small class.

This session walks the *stateful* shapes from simplest to most
interesting:

1. Why "stateful": the question stateless visitors can't answer
2. Class-based visitor that *owns* its state (indentation
   pretty-printer for a recursive variant AST)
3. Visitor that *borrows* state via a context reference
4. Recursive lambda visitor with `self` capture and shared state
5. Visitor over a `std::vector<std::variant<...>>` with a
   fold-style accumulator (Summary over a JSON-ish tree)
6. Visitor that pushes/pops frames on a `std::vector<Frame>`
   (a logical call stack carried by the visitor)
7. Visitor across a span of variants (cross-references Jun 14's
   `std::span` and Jun 10's `std::variant`)

The file is
`late-may/cpp_practice/P-2026-06-15-stdvisit-stateful-visitors.cpp`.
Path matches the Jun 9–14 convention (`late-may/cpp_practice/`,
P-prefix).

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-15-stdvisit-stateful-visitors \
    P-2026-06-15-stdvisit-stateful-visitors.cpp
./P-2026-06-15-stdvisit-stateful-visitors
```

ASan build, also exercised:

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
    -o P-2026-06-15-stdvisit-stateful-visitors-asan \
    P-2026-06-15-stdvisit-stateful-visitors.cpp
./P-2026-06-15-stdvisit-stateful-visitors-asan
```

## Sections at a glance

1. **Why stateful** — recap of stateless shape and its limit
2. **Class-based visitor that owns its state** — `PrettyPrinter` with `indent_` + `outstream&`
3. **Visitor borrows state via context reference** — `PrintContext` shared across calls
4. **Recursive lambda with `self` capture** — the rare "I want a lambda but it must recurse" pattern
5. **Fold-style stateful visitor** — `Summary` accumulator over a JSON-ish tree
6. **Visitor with a borrowed call stack** — path-tracking via `std::vector<Frame>`
7. **Span of variants** — `TypeCounter` walking a `psp::Span<JsonValue>`

## Key ideas

### A "stateful" visitor is a value with members

The simplest mental model: a visitor that needs state becomes a
class. Each `operator()` overload reads or updates `this->` fields.
`std::visit(pp, v)` passes the visitor *by value* to visit, so
the visitor is just another argument to the function — but with
its own data.

Section 2's `PrettyPrinter` is the canonical example:

```cpp
class PrettyPrinter {
    std::ostream& out_;
    int           indent_ = 0;
public:
    void enter()  { indent_ += 2; }
    void leave()  { indent_ -= 2; }

    void operator()(IntLit const& i) {
        out_ << std::string(indent_, ' ') << "IntLit(" << i.value << ")\n";
    }
    void operator()(AddOp const& a) {
        out_ << std::string(indent_, ' ') << "AddOp('" << a.op << "')\n";
        enter();
        std::visit(*this, a.children[0]->alt);   // recurse with *this
        std::visit(*this, a.children[1]->alt);
        leave();
    }
    // ... MulOp similar
};
```

The trace for the AST `(1 + 2) * 3`:

```
MulOp('*')
  AddOp('+')
    IntLit(1)
    IntLit(2)
  IntLit(3)
  Final depth: 0  (must be 0)
```

Two things to notice:

1. The `enter()` / `leave()` calls are *not* in the overload set.
   They mutate state but are not alternative-dispatchers. `std::visit`
   only sees `operator()(IntLit)`, `operator()(AddOp)`,
   `operator()(MulOp)`. The class is the visitor, but the class
   has more than just the overloads.
2. `*this` is passed to `std::visit` recursively. Because the visitor
   is a value type, each recursion gets a copy — but the copy is
   shallow: the `outstream&` member is the same stream, the
   `indent_` member is copied in lockstep. By the time the descent
   finishes, `indent_` is back where it started (`Final depth: 0`).

The class form is what I'd reach for by default. It's faster than
the lambda + `std::function` form (section 4), and the *caller* can
hold a `PrettyPrinter` as a stack value, initialise it, run
`std::visit(pp, tree)`, and inspect `pp.depth()` afterwards.

### Borrowed state: pass a context by reference

Sometimes the visitor is conceptually a small dispatch table
(lambdas), but the state is shared infrastructure (an output
stream, a logger, a config object, an error sink). The cleanest
shape is: the *lambdas* are stateless, the *state* lives in a
context object passed by reference.

Section 3:

```cpp
struct PrintContext {
    std::ostream& out;
    int           depth = 0;
    std::string   prefix = "ctx";
};

PrintContext ctx{std::cout, 0, "ctx"};

auto printer = [&ctx](auto&& alt) {
    using T = std::decay_t<decltype(alt)>;
    if constexpr (std::is_same_v<T, int>) {
        ctx.out << "  [" << ctx.prefix << "] int = " << alt
                << " (depth=" << ctx.depth << ")\n";
    } // ... double, string
};

ctx.prefix = "first";   ctx.depth  = 0;
for (auto const& v : items) std::visit(printer, v);

ctx.prefix = "second";  ctx.depth  = 7;
for (auto const& v : items) std::visit(printer, v);
```

The trace shows two visits, same `printer` lambda, different
`ctx` state:

```
Run 1 (prefix='first', depth=0):
  [first] int = 42 (depth=0)
  [first] double = 3.14 (depth=0)
  [first] string = "hello" (depth=0)
Run 2 (prefix='second', depth=7):
  [second] int = 42 (depth=7)
  [second] double = 3.14 (depth=7)
  [second] string = "hello" (depth=7)
```

The state is *not* part of the lambda, so the lambda can be
called repeatedly with different `ctx` instances. The trade-off:
two concurrent visits reading the same `ctx` would race on
`ctx.depth`. That's the cost of borrowing: you only do this
when visits are sequential.

This shape is the right answer when the state is "config" (an
output stream, a verbosity level, a logger) rather than "per-call
mutation" (an indentation counter that wraps and unwraps).

### Recursive lambda: the `std::function` self-capture

If you want a lambda that calls itself, the pre-C++23 trick is
to capture a `std::function` by reference and use it as the
recursion entry point:

```cpp
std::function<int(TreeAlt const&)> sum =
    [&sum](TreeAlt const& alt) -> int {
    return std::visit(overloaded{
        // int branch doesn't need self
        [](int v) -> int { return v; },
        // recursive branch captures self
        [&sum](std::array<std::unique_ptr<Tree>, 2> const& p) -> int {
            return sum((*p[0]).alt) + sum((*p[1]).alt);
        },
    }, alt);
};
```

This works, but it has a real cost:

1. `std::function` allocates on the heap (the lambda might be
   small, but `std::function` doesn't know that).
2. The recursion is an indirect call through a type-erased
   function pointer.
3. The branch that doesn't recurse (`int v`) doesn't actually
   need to capture `sum`, but the capture set is fixed per
   outer lambda — you can omit it from the inner lambda as I
   did, but the outer capture is still there.

For one-shot recursion over a fixed type, a class-based visitor
is faster, smaller, and the same length once you get used to it.
The `std::function` form is the right answer when the *type* of
the recursion is dynamic (e.g. a plugin that registers a handler
at runtime) — which is exactly the code smell that section 8's
type-erased visitor demonstrates is usually a bad idea anyway.

C++23 fixes this with `deducing this` and the `this auto&& self`
recursive-lambda idiom. Until then, `std::function` is the
practical workaround, and it's worth knowing what it costs.

### Fold-style visitor: pure per-element, stateful aggregate

The most common stateful visitor in real code is a *collector*:
visit each alternative in a sequence, accumulate a result. The
visitor itself can be stateless; the state lives in the
accumulator. `std::visit` is the pure half; `std::accumulate`
is the stateful half.

Section 5's `Summary` is a value with six counters, and the
`Summarizer` visitor is a pure function from `JsonValue` to
`Summary`:

```cpp
struct Summary {
    std::size_t n_nulls    = 0;
    std::size_t n_bools    = 0;
    std::size_t n_numbers  = 0;
    std::size_t n_strings  = 0;
    std::size_t n_arrays   = 0;
    std::size_t n_elements = 0;
};

struct Summarizer {
    Summary operator()(std::monostate) const { ... }
    Summary operator()(bool)          const { ... }
    Summary operator()(double)        const { ... }
    Summary operator()(std::string const&) const { ... }
    Summary operator()(JsonArray const& arr) const {
        Summary s{/* counts the array itself */};
        for (auto const& e : arr) {
            Summary sub = std::visit(*this, e.alt);
            // merge sub into s
        }
        return s;
    }
};
```

The visitor returns a `Summary` *by value*. The caller can fold
a sequence of these with `std::accumulate`, or sum a single
visit's result with another, or push them onto a vector. The
key insight: **the visitor's overload set is pure, but the
caller's loop is stateful**. You can compose visitors freely
because each call is a value.

The trace for the JSON-ish document
`[null, true, 1.5, "x", [2, [3, 4]]]`:

```
  total.n_nulls    = 1
  total.n_bools    = 1
  total.n_numbers  = 4
  total.n_strings  = 1
  total.n_arrays   = 3
  total.n_elements = 10
```

Notice the recursion: the `JsonArray` overload calls `std::visit`
on each child, getting a `Summary` for each, and merges them.
This is the "visitor carries its own recursion" pattern from
section 2, but instead of a member field the *return value* is
the accumulator. Pure-as-possible.

### Borrowed call stack: a `vector<Frame>` driven by the visitor

Section 6's `StackFrameVisitor` borrows a `std::vector<Frame>`
and pushes a frame on the way into a JSON array, pops it on
the way out. The vector is the "call stack" — observable,
testable, and inspectable after the visit.

```cpp
struct StackFrameVisitor {
    std::vector<Frame>& stack_;   // BORROWED

    void operator()(JsonArray const& arr) const {
        for (std::size_t i = 0; i < arr.size(); ++i) {
            stack_.push_back(Frame{true, i, arr.size()});
            std::visit(*this, arr[i].alt);
            stack_.pop_back();
        }
    }
    // ... null, bool, double, string: print path + value
};
```

The trace for `[null, true, [1.0, [2.0, 3.0], "inner"]]`:

```
  path=[0] null
  path=[1] true
  path=[2, 0] 1.00
  path=[2, 1, 0] 2.00
  path=[2, 1, 1] 3.00
  path=[2, 2] "inner"
  Final stack size: 0  (must be 0 -- balanced push/pop)
```

The `Final stack size: 0` assertion is the testable invariant:
push/pop are balanced, no matter how the JSON is shaped. The
`stack_` member is a reference, so the visitor is short-lived —
it cannot outlive the stack it borrows. If the caller stashes
the visitor somewhere persistent, the reference is a dangling
trap. That's a real constraint; the trade-off is the
visitor can be passed around *during* a visit without
allocating a new frame per call.

### Span of variants: the killer combination

Section 7 combines Jun 14's `Span<T>` (pointer + size) with
Jun 10's `std::variant`. The result is a function that accepts
*any contiguous range* of variants and dispatches per
alternative:

```cpp
void count_types(Span<JsonValue const> values, TypeCounter& tc) {
    for (auto const& v : values) std::visit(tc, v.alt);
}
```

(That's the shape; section 7 inlines the loop to keep the
example self-contained.) The signature is cheaper than
`std::vector<JsonValue> const&` (no allocator, no ownership,
just a view), and the variant dispatch is in the function.
The state (the `counts` map) lives where the caller wants it —
in the function's caller, not in the visitor or the function
parameter.

The trace for a 9-element JSON vector:

```
  Span over a std::vector<JsonValue>, size=9
    array: 1
    bool: 2
    null: 1
    number: 3
    string: 2
```

This is the realistic use case: a *function* that takes a
span of variants and a stateful visitor, and the caller
controls where the state lives. The visitor is small
(stateless in this case) and the span is small (two words).
The function is two lines. The state is the *result*, not
the *mechanism*.

## Design choices and trade-offs

### When to use which shape

The five shapes map to real situations:

| Shape | State lives in | Use when |
|---|---|---|
| Class-based visitor (sec 2) | The visitor | Per-call mutable state (indent, current parent node, recursion depth) |
| Context-borrowed lambdas (sec 3) | A context object | Sequential visits with shared infrastructure (output stream, logger, config) |
| `std::function` self-capture (sec 4) | The lambda itself | Recursion where the type is fixed and the cost of indirection is acceptable |
| Fold over values (sec 5) | The return value | Visitor is pure, the caller folds a sequence of results |
| Borrowed call stack (sec 6) | A borrowed container | Visitor needs an observable, testable in-progress state (frame stack, error list) |
| Span of variants (sec 7) | The caller | Function takes any contiguous range of variants and dispatches per alternative |

In practice, **most of my visitors will be class-based** (sec 2)
or **fold-style** (sec 5). Sections 3, 4, 6 are the rare-but-real
patterns where state has to live somewhere specific.

### Why the class form is faster than the lambda form

The class form (`PrettyPrinter`) has a *non-virtual* `operator()`
overload set. The compiler knows the exact type of the visitor
at the call site, so `std::visit(pp, v)` devolves to a switch
on `v.index()`. The lambda + `std::function` form
(section 4) goes through a type-erased function pointer, which
is an indirect call. For a non-recursive visitor that's a small
constant cost; for a recursive visitor over a deep tree it's
per-node. The class form is the right answer for "hot" visitors;
the lambda form is the right answer for "configurable" visitors
(the alternative set is a runtime parameter).

### Why I don't recommend the type-erased visitor (sec 8 was cut)

The earlier draft of this file had a section 8 that built a
type-erased visitor via `std::map<std::type_index, std::function<...>>`.
It "worked" — the dispatch table got populated at runtime, and
`std::visit` was replaced by a hand-rolled `if (index() == 0) ...`
chain. But the code was worse than the compile-time shape:

1. The type-id branches duplicate the dispatch logic that
   `std::visit` already does, with no compile-time check that
   every alternative has a handler.
2. The `void*` erase loses all the type safety that motivated
   `std::variant` in the first place.
3. The runtime cost is much higher (one map lookup per visit).

I cut the section before committing. The lesson stays: the
compile-time overload set is the right answer 99% of the time,
and the rare "I don't know all my alternatives at compile time"
case is better served by inheritance or a different abstraction
than by reinventing `std::visit` badly.

## Cross-references and follow-ups

- **Jun 10 (`std::variant` / `std::visit`)** — the stateless
  shape. Today's file is "what if the stateless shape isn't
  enough?" The `overloaded{...}` helper is the same one.
- **Jun 4 (operator overloading)** — the `Inventory` is a
  vector of items; a stateful visitor over the vector could
  compute a "total value" or a "find by predicate" without
  re-iterating. The fold pattern (sec 5) is the natural fit.
- **Jun 11 (`std::expected`)** — a stateful visitor that
  returns `std::expected<Summary, ParseError>` would short-
  circuit on the first parse error. The fold pattern + early
  return is the right answer.
- **Jun 12 (`std::move_if_noexcept`)** — the recursive AST
  in sec 2 (`ExprNode` with `std::unique_ptr` children) is
  movable but its alternatives aren't *noexcept-movable* by
  default. If I want the variant to be safely stored in a
  container that prefers `noexcept` moves, I'd need to write
  a `noexcept` move ctor on each alternative. Ties into the
  Jun 12 lesson.
- **Jun 14 (`std::span`)** — sec 7 is the cross-reference:
  span over a vector of variants + stateful visitor. The
  hand-rolled `psp::Span` in this file is the minimum
  needed; the Jun 14 file has the full implementation.
- **Jun 9 (`std::move` / rvalue refs)** — the recursive
  AST in sec 2 owns its children via `std::unique_ptr`, so
  the AST is movable. The variant alternative types
  (`AddOp`, `MulOp`) hold `std::array<std::unique_ptr<...>, 2>`,
  which is itself movable. The whole AST moves as a value.

## Next Steps

- **Tag dispatch vs. `std::visit` for open sets of types** —
  when the alternative set is *not* known at compile time
  (e.g. a plugin that registers a handler at runtime), neither
  `std::visit` nor `dynamic_cast` is the right answer. Worth
  exploring `std::function`, type erasure, or a custom
  `type_index` table. (New on the list — cut from this
  session as a code smell, but worth a careful look
  elsewhere.)

- **`std::visit` over a `std::map` (heterogeneous lookup with
  variants)** — combining Jun 10's variant with a map keyed
  by `std::variant<...>` for type-tagged polymorphism in
  storage. (Still on the list from Jun 10.)

- **Pretty-printer for a real C++ AST** — sec 2 builds a
  pretty-printer for a toy `ExprNode`. The real version
  would have `if`/`while`/`for`/`return`/function calls, all
  as variant alternatives, and the visitor would handle
  line-breaks, operator precedence, and statement
  separators. (Still on the list from Jun 11, Jun 12,
  Jun 13, Jun 14.)

- **`std::visit` + `if constexpr` for SFINAE-friendly
  overload sets** — when the alternative type is a
  template parameter, `if constexpr` inside a generic
  lambda can dispatch on type traits the way `std::visit`
  dispatches on the runtime alternative. The two patterns
  have different cost profiles: `std::visit` is `O(1)`
  per alternative, `if constexpr` chains are `O(N)` but
  can use type traits the runtime tag can't see. Worth
  comparing side-by-side. (Still on the list from Jun 12.)

- **A "type-printer" visitor that recursively pretty-prints
  any `std::variant` whose alternatives are themselves
  printable** — a visitor that uses ADL-found `operator<<`
  on each alternative. Conceptually simpler than a
  hand-rolled overload set, but requires the alternatives
  to be in `namespace` where ADL can find them. (New on
  the list, a natural extension of sec 2 and sec 3.)

- **Build the multi-file `Inventory` as separate compilation**
  (`.h` / `.cpp` / `Makefile`) — the Jun 9 Inventory is one
  big file; splitting it shows how moves work at the link
  boundary. (Still on the list from Jun 9, Jun 11, Jun 12,
  Jun 13, Jun 14.)

- **Recursive `std::variant` in `std::unique_ptr`** — the
  AST pattern in sec 2 already does this. Worth rebuilding
  a more interesting example (a small expression evaluator
  with `if`/`while`) to see the `unique_ptr` cost vs. the
  `shared_ptr` cost from Jun 10. (Still on the list from
  Jun 11, Jun 12, Jun 13, Jun 14.)

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:**
  `late-may/cpp_practice/P-2026-06-15-stdvisit-stateful-visitors.cpp`
  and this notes file (`.md`).
- **P- prefix per the new convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.
  All of `std::visit`, `std::variant`, `std::function`, and
  `std::unique_ptr` are C++11/14/17 features, so no
  conditional compilation is needed.

---

*A stateful visitor is a class. The state lives in members; the
overload set is `operator()`. `std::visit` passes the visitor by
value, so the state travels with it across recursive calls —
shallow-copy through the call stack, deep-copy only when the
state is a container. When the state needs to be observable
(a frame stack, an error list), the visitor borrows a container
by reference. When the state is "the result," the visitor
returns it by value and the caller folds. The stateless
`overloaded{...}` shape from Jun 10 is the special case where
the state is empty; everything in this file is "what does the
shape become when state has to live somewhere?" The answer is
almost always "a class with members, or a borrowed context
passed by reference." The `std::function` self-capture and
the type-erased visitor exist, but they're specialised
patterns with real costs. The class form is the workhorse,
and it composes cleanly with spans, vectors, and the rest
of the standard library.*
