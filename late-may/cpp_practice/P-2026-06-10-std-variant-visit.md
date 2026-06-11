# C++ practice 2026-06-10 — `std::variant` and `std::visit` (C++17)

## What I set out to learn

`std::variant<Ts...>` is C++17's type-safe tagged union: a value that is
exactly one of a fixed set of alternatives, owning its storage inline. The
companion `std::visit` is the dispatch mechanism for doing something with
the value, regardless of which alternative is active.

This is the natural follow-up to the Jun 9 `std::move` work. `std::move` is
a *cast* that reclassifies an expression; `std::visit` is a *dispatch* that
inspects a value's active alternative and routes control accordingly. Both
are part of the same shift in C++ thinking away from inheritance-based
runtime polymorphism and toward value-based polymorphism in the type
system.

The file is `cpp_practice/P-2026-06-10-std-variant-visit.cpp` (per-day dir
matching the Jun 9 `cpp_practice/2026-06-09.cpp` pattern), with a small
`Traced` type carried over to make moves visible — exactly the same move
tracing the Jun 9 session established, so the two sessions stitch together
naturally.

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-10-std-variant-visit P-2026-06-10-std-variant-visit.cpp
./P-2026-06-10-std-variant-visit
```

ASan build, also clean:
```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
    -o P-2026-06-10-std-variant-visit-asan P-2026-06-10-std-variant-visit.cpp
./P-2026-06-10-std-variant-visit-asan
```

Build is clean under `-Wall -Wextra -Wpedantic`; ASan reports no leaks,
no use-after-free, no double-free (every `Traced` construction is paired
with a destruction in the trace).

## Sections at a glance

1. **Basics** — declare a variant, `index()`, `holds_alternative<T>()`
2. **`std::get<T>` and `std::get<idx>`** — throws `std::bad_variant_access`
3. **`std::get_if<T>`** — returns `nullptr` on miss
4. **`std::visit` with `overloaded`** — the classic "struct of lambdas" trick
5. **`std::visit` with a generic lambda** — `auto&&` accepts any alternative
6. **`std::visit<R>(...)`** — explicit return type
7. **`std::monostate`** — the default-constructible placeholder
8. **Move semantics** — variants own their active alternative
9. **Lexicographic comparison** — order of alternatives matters
10. **Mini-example** — a recursive `Number | BinaryOp` expression evaluator

## Key ideas

### `variant` is value-semantic, not pointer-based

Unlike a C `union` (which is just a chunk of memory you reinterpret based
on a tag), `std::variant` *knows* which alternative is active and
*destroys* the active alternative on assignment. It owns the storage
inline, sized to the largest alternative plus a discriminator. You can't
"smash" it by accessing the wrong alternative; the type system refuses.

### `std::visit` is the dispatch primitive

`std::visit(visitor, variant)` calls `visitor.operator()(active_value)`.
The visitor can be:

- **An overload set** — a struct with one `operator()` per alternative.
  This is the cleanest, most typesafe form. The `overloaded` helper from
  section 4 is the well-known trick: it inherits from each lambda, the
  deduction guide lets you write `overloaded{...}` without spelling the
  type, and std::visit's overload resolution picks the right `operator()`
  for the active alternative.
- **A generic lambda** — `(auto&& x) { ... }`. The compiler instantiates
  one `operator()` per alternative. Less typesafe (you don't know which
  type you got without a `decltype(x)` or `std::decay_t<...>` check), but
  shorter.
- **A function object with `operator()` templates** — equivalent to the
  generic lambda, just spelled as a class.

### `overloaded` is a tiny idiom worth memorizing

```cpp
template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;
```

That's the whole thing. Without the deduction guide you have to write
`overloaded<Lambda1, Lambda2, Lambda3>{...}`; with it, `overloaded{...}`
is enough.

### `std::monostate` solves a real problem

A `std::variant` is *default-constructible* only if its first alternative
is. So `std::variant<std::mutex, std::string>` isn't default-constructible
(`std::mutex` isn't). The fix is to put `std::monostate` first, which is
an empty struct that exists solely to give variants a "no value" state:

```cpp
using V = std::variant<std::monostate, std::mutex, std::string>;
static_assert(std::is_default_constructible_v<V>);  // OK now
```

You can then check `holds_alternative<std::monostate>` to mean "empty".
This is the idiomatic "not yet set" or "no error" sentinel.

### Variants own the active alternative; assignment destroys the old one

Section 8 demonstrates this with a `Traced` type. The trace shows:

- `v = Traced("fresh")` constructs a new `Traced` in place and destroys
  the old (in this case, the int that the default-constructed variant
  was holding).
- `std::move(v)` moves the `Traced` out, leaving the source in a
  moved-from state. The variant's index is unchanged, but the
  alternative's internal state is "I just got my guts taken out."

### `valueless_by_exception`

A `std::variant` can become "valueless" (index() == `variant_npos`) if an
exception is thrown from the new alternative's constructor during
assignment. In well-written code with `noexcept` move constructors, this
shouldn't happen — but the standard library guards against it anyway,
because vector reallocations can throw and the standard guarantees
exception safety. After a valueless variant: `std::get` throws,
`std::get_if` returns `nullptr`, and `std::visit` works only if every
operator() handles the "no value" case (or you check `index()` first).

### Comparison: order of alternatives matters

`std::variant` compares lexicographically: first by `index()`, then by
the active alternative's `<` operator. This means
`std::variant<int, std::string>(7) < std::variant<int, std::string>("anything")`
because `int` is index 0 and `std::string` is index 1. The result is
sensitive to the *declaration order* of alternatives, not the values
stored. This is usually what you want, but if it isn't, you have to
implement comparison yourself.

### `std::visit` as a non-exception error channel (the evaluator)

Section 10 builds a small arithmetic expression evaluator with the type:

```cpp
using EvalResult = std::variant<double, std::string>;
```

Success: a `double`. Failure: a `std::string` error message. The caller
*must* `std::visit` the result to handle both cases; there's no overload
of `+` on `std::string` that lets them forget. This is one of the two
main alternatives to exceptions for "this might fail" APIs (the other is
`std::expected<T, E>` in C++23, which is essentially the same idea with
the alternative names swapped: `expected<T, E>` holds either `T` or `E`,
whereas `variant<T, E>` holds either `T` or `E` and the "good" case isn't
distinguished syntactically).

The evaluator also shows the `overloaded` pattern in its natural habitat:
the dispatch is the entire algorithm. The expression type is a recursive
`variant<Number, BinaryOp>`, and `evaluate` is a `std::visit` that
recurses into the subexpressions.

## Sample output highlights

```
========== 1. Declaration, index(), holds_alternative<T>() ==========
default-constructed v.index() = 0 (int is alternative 0)
holds_alternative<int>(v)    = true
after v=42:        index=0 holds<int>=true
after v=3.14:      index=1 holds<double>=true
after v=string:    index=2 holds<string>=true
```

```
========== 4. std::visit with the `overloaded` helper ==========
int 7
double 2.500000
string 'hi'
```

```
========== 7. std::monostate -- the default-constructible placeholder ==========
default v.index() = 0 (monostate is 0)
holds<Traced>(v)  = false
  [Traced] ctor     (a-box) payload='payload-a-box'
  [Traced] MOVE     (a-box)
v now holds Traced 'payload-a-box'
  [Traced] dtor     ((moved)) payload=''          <-- the moved-from t
  [Traced] dtor     (a-box) payload='payload-a-box'  <-- the variant's copy
```

(One `Traced` construction, two destructions: the moved-from local
`Traced t("a-box")` plus the variant's owned `Traced` whose `payload`
was filled by the move. That is *not* a double-free — the moved-from
local's payload is empty, and the variant's local has the data. The
prints just look similar because the moved-from object remembers its
old name as `(moved)`.)

```
========== 8. Move semantics: variants own the active alternative ==========
  [Traced] ctor     (outer) payload='payload-outer'
  [Traced] MOVE     (outer)
  [Traced] dtor     ((moved)) payload=''          <-- moved-from outer
v created, holds Traced('outer')
  [Traced] MOVE     (outer)                       <-- move into w
w created from std::move(v)
v.valueless_by_exception() = false                <-- not valueless; just moved-from
  [Traced] dtor     (outer) payload='payload-outer'  <-- w's contents
  [Traced] dtor     ((moved)) payload=''          <-- v's moved-from Traced
```

```
========== 9. Lexicographic comparison ==========
(a < b) where a=int(7), b=string("z")        : true (true: int index 0 < string index 1)
(b < c) where b=string("z"), c=string("a")   : false (false: same index, 'z' > 'a')
(a < d) where a=int(7),    d=int(100)         : true (true: same index, 7 < 100)
```

```
========== 10. Mini-example: Expression evaluator ==========
  (1+2) * (3 - 4/2) = 3
  1 / 0 ERR: division by zero
```

## Bugs and gotchas I hit

1. **`std::get<2>(v)` throws if the variant is index 0.** My first draft
   had a `v` that started as `int` and tried to demonstrate
   `std::get<2>(v)` as if `v` were a string. It wasn't, and the program
   abort-trapped with `std::bad_variant_access`. I split the section
   into a `v` (int) and a `v2` (string) so the position-form demo and
   the throw-catch demo could coexist. Lesson: `std::get<index>` checks
   the runtime index, not a compile-time assertion that the variant
   always holds that alternative.

2. **LSP diagnostics are not the compiler.** The editor's language
   server complained about `No member named 'variant' in namespace
   'std'` and 50 follow-on errors. The actual `g++ -std=c++17` build
   compiled cleanly. Lesson: when the editor and the compiler disagree,
   the compiler wins. (LSP appears to be configured for an older
   standard. Not a code problem.)

3. **The `Traced` trace looks alarming on first read.** Two adjacent
   destructions with the same name in section 7 confused me for a
   second — I almost flagged it as a double-free. It's the moved-from
   local plus the variant's own. Worth a comment in the output
   explanation (above) so the next reader doesn't make the same
   mistake.

4. **`std::variant` is not default-constructible when the first
   alternative isn't.** Already discussed in the monostate section.
   Easy to forget. The static_assert in section 7 is a good
   belt-and-braces: it'll fire at compile time if someone removes the
   `std::monostate` from the front of the list.

## Verification

- `g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g`: **clean**
- `g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address`:
  **clean** (no leaks, no UAF, no double-free in the variant move
  traces)

The program terminates normally with exit status 0. Every section
prints what it claims to print; the evaluator's recursive
`std::visit` handles both the success and error alternatives.

## Key takeaways

- **`std::variant` is a value-semantic, type-safe union.** It owns its
  active alternative and destroys it on reassignment. It refuses to
  let you read the wrong alternative; the type system enforces it.
- **`std::visit` is the dispatch.** Pick the right visitor form for
  the situation: `overloaded{...}` for full typesafety, `(auto&&)`
  for terseness.
- **The `overloaded` helper is a one-liner that pays for itself.**
  Memorize it.
- **`std::monostate` is the empty placeholder** for variants whose
  first real alternative isn't default-constructible. Idiom: put it
  at index 0; check `holds_alternative<std::monostate>` for "empty."
- **Variants own their alternatives.** Moves of a variant move the
  active alternative. A moved-from variant isn't valueless — its
  alternative is just in a moved-from state.
- **Comparison is lexicographic and order-sensitive.** The
  declaration order of alternatives is part of the value's identity
  for `<` and `==`. Reorder them and the same values compare
  differently.
- **`std::variant<Result, Error>` is one of the two main alternatives
  to exceptions for "this might fail" APIs.** The other is
  `std::expected<T, E>` (C++23). The trade-off: every caller has to
  visit the result; the payoff: you can't accidentally drop the error
  case.

## Next Steps

- **`std::expected<T, E>` (C++23)** — same idea as `variant<T, Error>`
  but with the success type distinguished syntactically. The natural
  next step if you want to make "this might fail" APIs more readable.
- **`std::visit` with stateful visitors** — passing a visitor that
  accumulates results across alternatives. Useful for serialization,
  pretty-printing, hash computation.
- **Recursive `std::variant` types in `std::unique_ptr`** — the
  common pattern for tree-like types is
  `std::variant<Leaf, std::unique_ptr<Node>>` rather than the
  `std::shared_ptr` dance I used in section 10. Smaller, faster,
  value-semantic. Worth a follow-up.
- **Tagged union vs. inheritance for ASTs** — when is the variant
  approach clearer than a `virtual Expr::accept(...)` visitor? Both
  work; the variant is open-closed on the *alternatives* but closed
  on the *operations*, while the inheritance version is the
  opposite. Useful to think about.
- **Build the multi-file `Inventory` as separate compilation** — the
  Inventory example from Jun 9 is currently one big file; splitting
  it into `.h` / `.cpp` / `Makefile` would let us see how moves work
  at the link boundary. (Still on the list from Jun 9.)
- **`std::span` (C++20)** — but worth understanding the C++17 design
  by hand first. (Also still on the list from Jun 9.)
- **`enable_shared_from_this`** — combine with variant/visitor for
  a complete value-semantic + shared-ownership toolkit.

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `cpp_practice/P-2026-06-10-std-variant-visit.cpp`
  and this notes file (`.md`).
- **P- prefix per the new convention (this is Petra's session).**

---

*`std::variant` and `std::visit` are the C++17 replacements for the
inheritance-and-virtual-function idiom when you know the set of types
ahead of time. They give you value semantics, type safety, and an
in-place storage layout that won't beat pointers for cache-friendliness
but beats inheritance for clarity. The `overloaded` helper is the
single best one-liner in modern C++.*
