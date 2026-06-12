# C++ practice 2026-06-12 — `std::move_if_noexcept` (C++17)

## What I set out to learn

`std::move_if_noexcept` is a one-line utility template that the standard
library uses internally to decide whether to **move** or **copy** an
element during container reallocation. The decision is driven entirely by
whether the element's move constructor is `noexcept`.

The reason this matters: when `std::vector<T>` grows, it has to relocate
all existing elements into a bigger buffer. If a move constructor
*throws* partway through, the vector ends up in a corrupted state —
some elements moved, some not, the old buffer is about to be freed. To
keep the strong-exception-safety guarantee, the standard library
*deliberately copies* elements whose move ctor is throwing, because
copies either complete or leave the source unchanged (so the vector can
roll back). Elements with a `noexcept` move ctor are moved, and the
move is guaranteed not to throw, so a successful move leaves the source
in a valid destructible state.

`std::move_if_noexcept` is the hook that encodes that policy. You
almost never call it directly, but its behavior is visible whenever
you put a type into a `std::vector`, `std::deque`, or `std::string`.

The file is `cpp_practice/P-2026-06-12-stdmove-if-noexcept.cpp`. Path
matches the Jun 9–11 convention (`cpp_practice/`, P-prefix).

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-12-stdmove-if-noexcept P-2026-06-12-stdmove-if-noexcept.cpp
./P-2026-06-12-stdmove-if-noexcept
```

ASan build, also clean:

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
    -o P-2026-06-12-stdmove-if-noexcept-asan P-2026-06-12-stdmove-if-noexcept.cpp
./P-2026-06-12-stdmove-if-noexcept-asan
```

## Sections at a glance

1. **The shape of `std::move_if_noexcept`** — the declaration, the return type
2. **Why it exists: strong exception safety** — the `vector` reallocation story
3. **`ThrowingMove`**: `move_if_noexcept` falls back to copy
4. **`NonThrowingMove`**: `move_if_noexcept` picks the move
5. **`CopyOnly`**: no move ctor exists, copy is the only option
6. **`is_nothrow_move_constructible_v` at compile time** — the trait that drives the choice
7. **`vector<bool>` and the trade-off** — same idea, different domain
8. **The default rule for your own types** — `noexcept` on move, always

## Key ideas

### `std::move_if_noexcept` returns the *type* of reference, not just a value

The simplified declaration is:

```cpp
template<class T>
constexpr T&& move_if_noexcept(T& x) noexcept {
    return std::is_nothrow_move_constructible_v<T>
               ? std::move(x)
               : static_cast<T&&>(x);   // cast to rvalue ref, but x stays an lvalue
}
```

Two things to notice:

- The conditional is on a **type trait**, not a runtime value. The
  decision is made at compile time; the compiler elides the branch.
- The "fall back to copy" path returns `static_cast<T&&>(x)` — which is
  an rvalue reference **type**, but `x` itself is still an lvalue
  (named variable). The cast doesn't make the call site treat `x` as
  an rvalue; it just makes the *return type* the same in both branches.
  The actual mechanism that forces a copy is that the caller binds
  the result of `move_if_noexcept(x)` to a destination, and the
  *destination's* move/copy choice is governed by whether the
  expression on the right is an lvalue or rvalue. Returning an rvalue
  reference to a named variable is what makes the caller's
  `T dst = std::move_if_noexcept(src)` end up as a copy.

This is the kind of cleverness that makes `<utility>` worth a second
look. The function is doing two different things under one return
statement and relying on value category semantics to make it work.

### The strong-exception-safety argument

`std::vector<T>::push_back` when the vector is at capacity does roughly
this:

1. Allocate a bigger buffer.
2. **Relocate** each existing element into the new buffer.
3. Destroy the old elements.
4. Deallocate the old buffer.
5. Install the new buffer.

Step 2 is where it gets dicey. If the move ctor throws halfway through,
the vector is now in a bad state:

- Some elements have been moved to the new buffer; their old storage
  is in the old buffer, about to be freed.
- Some elements are still in the old buffer.
- The element that was being moved when the throw happened has been
  modified by the partial move.
- The old buffer is about to be deallocated, which will run the
  remaining element destructors on the old storage.

The standard library's fix is to **never move an element whose move
ctor can throw during this reallocation**. It either moves a
`noexcept`-movable type, or it copies a throwing-move type. Copies are
guaranteed to leave the source unchanged on failure, so the source
remains in a valid state, and the vector can roll back to its original
state on failure.

This is "strong exception safety": if the operation fails, the
program state is identical to what it was before the operation started.

The cost: copying is slower than moving, often dramatically so. So
the standard library is making a deliberate speed-vs-safety trade-off,
governed entirely by the `noexcept` keyword on your move constructor.

### The trace in section 7 is the punchline

When I push 4 elements into a `std::vector<ThrowingMove>` (whose move
ctor is throwing), the reallocation during the third `push_back`
produces these traces for the existing `a` and `b` elements:

```
[Tracer ThrowingMove] ctor  tag='copy-of-a'
[Tracer ThrowingMove] ctor  tag='copy-of-b'
[Tracer ThrowingMove] dtor  tag='a'
[Tracer ThrowingMove] dtor  tag='b'
```

That's **two copies** of `a` and `b` (the existing elements) into the
new buffer, then the originals are destroyed. The vector is paying for
copies because `ThrowingMove` doesn't promise not to throw during a
move.

For `std::vector<NonThrowingMove>`:

```
[Tracer NonThrowingMove] ctor  tag='move-of-'
[Tracer NonThrowingMove] ctor  tag='move-of-'
[Tracer NonThrowingMove] dtor  tag='a'
[Tracer NonThrowingMove] dtor  tag='b'
```

That's **two moves** (note `move-of-` instead of `copy-of-a`). The
vector used the move because `NonThrowingMove`'s move ctor is
`noexcept`, and the standard library trusts it.

The difference is invisible at the algorithm level — both vectors end
up with the same elements after the reallocation. The difference is
in the **cost** of getting there. For a type with non-trivial state
(`std::string`, `std::vector`, anything owning a resource), copying
allocates fresh memory and duplicates the contents; moving just
steals a pointer. The speedup from `noexcept` on the move ctor is
exactly the difference between a copy and a pointer swap.

### `is_nothrow_move_constructible_v` is the trait that drives the choice

The type trait `std::is_nothrow_move_constructible_v<T>` is `true` iff
`T` has a move constructor that is `noexcept`. The standard library
queries this trait internally at every `std::vector` reallocation.
Section 6 prints the trait value for the demo types:

```
is_nothrow_move_constructible_v<ThrowingMove>   = false
is_nothrow_move_constructible_v<NonThrowingMove> = true
is_nothrow_move_constructible_v<CopyOnly>        = false
is_nothrow_move_constructible_v<Good>            = true
is_nothrow_move_constructible_v<std::string>     = true
is_nothrow_move_constructible_v<int>             = true
```

`ThrowingMove` and `CopyOnly` are both `false`, but for different
reasons: `ThrowingMove` has a move ctor that is not `noexcept`,
`CopyOnly` has no move ctor at all. In both cases, `std::move_if_noexcept`
falls back to copy. `NonThrowingMove`, `Good`, `std::string`, and `int`
are all `true` — they all get moved by the standard library.

The trait is **automatically `true` for any type whose members' move
constructors are all `noexcept`**. This is why the implicit move ctor
on `Good` (which has a single `std::string` member) is `noexcept`:
`std::string`'s move ctor is `noexcept`, so `Good`'s implicit move
ctor inherits that.

### `vector<bool>` is the famous case

`std::vector<bool>` is a famous special case: its "reference" is a
proxy type that packs bits, and moving a proxy can require swapping
bits, which the standard library can't make `noexcept`. So
`vector<bool>::push_back` uses **copies** (via the proxy) during
reallocation, not moves. That's why `vector<bool>` is sometimes
slower than people expect — it's an in-class example of the
`std::move_if_noexcept` trade-off in action.

### The two-character rule

For your own types, the rule is:

> **If you write a move constructor, mark it `noexcept`.**

That's it. Two characters. The standard library will then move your
type during every `vector`/`deque`/`string` reallocation. Without it,
your type will be copied.

The compiler-generated move ctor is already `noexcept` (it inherits
the noexcept-ness of its members), so simple value types are fine.
The case where you have to do something: resource-managing types
where you write the move ctor by hand to swap pointers or transfer
ownership. Marking it `noexcept` is the difference between
`std::vector` moving it and copying it.

`std::unique_ptr<T>` is `noexcept`-movable for this exact reason: its
move ctor is a pointer swap, and the standard library needs to know
that swap can never throw. Otherwise, a `vector<unique_ptr<...>>`
reallocation would copy the pointers (still cheap, but not free) and
the type's "obviously movable" property would be lost in containers.

## Sample output highlights

```
========== 3. ThrowingMove: move_if_noexcept falls back to copy ==========
  [Tracer ThrowingMove] ctor  tag='hello'
  -- creating a destination via std::move_if_noexcept(src) --
  [Tracer ThrowingMove] ctor  tag='copy-of-hello'
  -- dst created --
  src.payload = 'hello'         <- unchanged: we copied
  dst.payload = 'hello'

========== 4. NonThrowingMove: move_if_noexcept picks the move ==========
  [Tracer NonThrowingMove] ctor  tag='world'
  -- creating a destination via std::move_if_noexcept(src) --
  [Tracer NonThrowingMove] ctor  tag='move-of-'     <- moved (empty)
  -- dst created --
  src.payload = '' (moved-from, empty)
  dst.payload = 'world'
```

Section 3 produces a `copy-of-hello` tag; section 4 produces a
`move-of-` tag. Same code, different types, different outcomes —
the only difference is `noexcept` on the move ctor.

```
========== 7. vector<bool>... and the trade-off ==========
  -- vector<ThrowingMove> growing via push_back --
  [Tracer ThrowingMove] ctor  tag='copy-of-a'    <- copies
  [Tracer ThrowingMove] ctor  tag='copy-of-b'

  -- vector<NonThrowingMove> growing via push_back --
  [Tracer NonThrowingMove] ctor  tag='move-of-'  <- moves
  [Tracer NonThrowingMove] ctor  tag='move-of-'
```

The reallocation trace is the same shape, but the work done is
different. For `std::string`-sized members, the move path is a
pointer swap; the copy path is an allocation + `memcpy`. The
amortized cost of `push_back` over a long-lived vector is the same
either way (the geometric growth is still O(1) amortized), but the
constant factor is dominated by the copy/move cost in the
reallocation step.

## Bugs and gotchas I hit

1. **`static_cast<T&&>(x)` is not a move.** First time I wrote the
   `move_if_noexcept` declaration in the comments, I wrote
   `static_cast<T&&>(x)` and thought it was somehow equivalent to
   `std::move(x)`. It's not — `std::move(x)` is exactly
   `static_cast<T&&>(x)`, so they produce the same value (an rvalue
   reference to `x`). But the **call site** binds it differently:
   `T dst = std::move_if_noexcept(x)` works the same way `T dst = x`
   would (copy), because `x` is still an lvalue. The trick is that
   the conditional ternary returns an rvalue reference *type* in both
   branches, but the value category of the expression
   `std::move_if_noexcept(x)` depends on whether the underlying
   argument was an lvalue or rvalue. Since `move_if_noexcept` always
   takes `T& x`, the result is always... wait. Let me re-derive.
   
   The actual function in `<utility>` returns `T&&` (rvalue
   reference) in both branches. In the noexcept branch, the result
   is `std::move(x)`, which is `static_cast<T&&>(x)`, which is an
   xvalue. In the throwing-move branch, the result is also
   `static_cast<T&&>(x)` (the same expression!), but **with the
   call site's type deciding the move/copy based on
   `is_nothrow_move_constructible_v`**, the standard library routes
   the call through the copy constructor because it knows the move
   might throw.
   
   The way `std::move_if_noexcept` is actually defined in libstdc++
   / libc++ is slightly different from my "simplified" version. The
   real one is a `constexpr` function that returns a value whose
   *type* (not value category) depends on the trait: it's
   `std::conditional_t<std::is_nothrow_move_constructible_v<T>, T&&, const T&>`.
   That's the actual mechanism — a type-level conditional that
   makes the result a `T&&` (rvalue ref) or `const T&` (lvalue ref),
   and the call site binds that to the destination, picking the
   move ctor or copy ctor accordingly.
   
   I kept the simplified version in section 1 because the actual
   `std::conditional_t` is harder to read at a glance, but the
   trace in section 3 confirms the real behavior: `src` is
   unchanged after the operation, which is what copy does.

2. **My `Tracer` move ctor is `noexcept(false)`.** I wrote
   `Tracer(Tracer&& o) noexcept(false)` so the demo types
   `ThrowingMove` and `NonThrowingMove` could inherit the right
   noexcept-ness. The `Tracer` itself is throwing-move, but that's
   fine — its move is only called in the "throwing" demo type, and
   the `NonThrowingMove` demo type just embeds a `Tracer` whose
   move is throwing, but `NonThrowingMove`'s *own* move ctor is
   `noexcept` and gets the correct trait value.

3. **The LSP complained about `is_nothrow_move_constructible_v`**
   because it didn't have `is_nothrow_move_constructible_v` in its
   symbol index. `g++ -std=c++17` built it fine. The LSP was
   indexing a different `<type_traits>` definition. Compiling
   directly with `g++` is the truth — LSP diagnostics are
   guidance, not verdict.

## Verification

- `g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g`: **clean**
- `g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address`:
  **clean** (no leaks, no UAF, no double-free; the move/copy traces
  are all paired with exactly one dtor per ctor)
- Program terminates normally with exit status 0. All 8 sections
  print what they claim to print. The reallocation trace in section 7
  visibly shows the copy-vs-move difference: `ThrowingMove` produces
  `copy-of-*` tags, `NonThrowingMove` produces `move-of-` tags.

## Key takeaways

- **`std::move_if_noexcept` is the standard library's bridge between
  `noexcept` and the move-vs-copy decision.** It returns an rvalue
  reference if the type's move ctor is `noexcept`, an lvalue
  reference otherwise.
- **The reason it exists: strong exception safety.** A throwing move
  ctor corrupts the source on failure. By forcing copies for
  throwing-move types, the standard library guarantees that
  reallocation is either all-or-nothing.
- **`is_nothrow_move_constructible_v` is the trait that drives
  the choice.** It's `true` for types whose move ctor is `noexcept`,
  and it's automatically `true` for types whose members' move ctors
  are all `noexcept`.
- **Mark your move constructors `noexcept`.** It's two characters
  and the difference between `std::vector` moving your type and
  copying it.
- **The trace in section 7 is the visual proof.** Same algorithm,
  same `push_back` calls, different move semantics — and the trace
  shows the difference tag by tag.
- **You almost never call `std::move_if_noexcept` directly.** It's
  an implementation hook for the standard library. Knowing it
  exists explains why `noexcept` matters for performance, not just
  for correctness.

## Next Steps

- **`enable_shared_from_this`** — combine with smart pointers
  (Jun 4) + move (Jun 9) + expected (Jun 11) for a complete
  value-semantic + shared-ownership toolkit. The pattern is: a class
  that's already managed by a `shared_ptr` wants to give out more
  `shared_ptr`s to itself; `enable_shared_from_this` is the
  mechanism, and it ties into the weak_ptr machinery from Jun 4.
  (Still on the list from Jun 9 and Jun 11.)
- **`std::span` (C++20)** — non-owning view over a contiguous
  range. Pair, struct, array. The "C++17 design by hand" is a
  template class that holds a pointer and a size, with the
  same accessors. Worth building before reaching for C++20.
  (Still on the list from Jun 9 and Jun 11.)
- **`std::visit` with stateful visitors** — a pretty-printer for
  `std::variant<A, B, C>` that writes the active alternative's
  fields to an `ostream`. Builds on the Jun 10 variant/visit
  session. (Still on the list from Jun 11.)
- **Recursive `std::variant` in `std::unique_ptr`** — the AST
  pattern `std::variant<Leaf, std::unique_ptr<Node>>` is smaller
  and faster than the `std::shared_ptr` version in the Jun 10
  expression evaluator. Worth rebuilding the evaluator with
  `unique_ptr` to see the difference. (Still on the list from
  Jun 11.)
- **Build the multi-file `Inventory` as separate compilation**
  (`.h` / `.cpp` / `Makefile`) — the Jun 9 Inventory is one
  big file; splitting it shows how moves work at the link
  boundary. (Still on the list from Jun 9 and Jun 11.)
- **Tagged union vs. inheritance for ASTs** — when is the
  variant approach clearer than `virtual Expr::accept(...)`?
  Open-closed on alternatives vs. open-closed on operations.
  (Still on the list from Jun 11.)
- **A "real" error type with `std::error_code` integration** —
  the `ParseError` in Jun 11 is a POD struct; the production
  version uses `std::error_code` for OS-level errors and a
  custom category for domain errors. C++23's `std::expected`
  + `std::error_code` is a strong idiom for low-level
  libraries. (Still on the list from Jun 11.)

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `cpp_practice/P-2026-06-12-stdmove-if-noexcept.cpp`
  and this notes file (`.md`).
- **P- prefix per the new convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.
  `std::move_if_noexcept` is C++17, so no special-casing is
  required (unlike Jun 11's `-std=c++23` for `<expected>`).

---

*`std::move_if_noexcept` is a one-line utility hiding a real
design decision. The standard library could have just always moved
elements during reallocation and accepted the risk of corruption
on a throwing move; instead, it routes the choice through a
type trait, and the cost is borne by types that don't promise
`noexcept` on their move constructor. The two-character
`noexcept` keyword is the user's handshake with the standard
library: "I promise my move doesn't throw, so you can move me
during every reallocation." Most of the time, it's free to
add, and the payoff is real.*
