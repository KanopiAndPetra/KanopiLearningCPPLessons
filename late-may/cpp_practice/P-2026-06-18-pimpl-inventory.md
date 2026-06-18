# C++ practice 2026-06-18 — PIMPL the Inventory

## What I set out to learn

The Jun 17 lesson introduced PIMPL on the `Box` class. The
"Next Steps" list at the end of that lesson had, as item #1:

> **PIMPL-ify the `Inventory`** — apply the same pattern to
> the Jun 16 multi-file `Inventory`. The interesting bit is
> what goes in `Inventory::Impl`: the `std::vector<Box>`, yes,
> but also the index validation logic, the trace lines, and
> the move optimisation.

This lesson is that follow-up. The goal is to take the
non-PIMPL `Inventory` (a class with a `std::vector<Box>`
directly in its layout) and turn it into a PIMPL class where
the vector lives in a `unique_ptr<Impl>`.

The same five things I want this lesson to teach:

1. **The PIMPL pattern is a discipline, not a one-off trick.**
   It applied to `Box` on Jun 17; it applies to `Inventory`
   on Jun 18; it would apply to any class that wants a stable
   header. The boilerplate is identical — `struct Impl;`
   forward decl, `std::unique_ptr<Impl> impl_`, dtor and copy
   ops declared in the header / defined in the `.cpp`, move
   ops `= default` in the header. The lesson's payoff is the
   observation that the rule generalises.
2. **Forward-declaring `Box` in the header breaks a real
   dependency.** The Jun 16 `inventory.h` had to include
   `box.h` because it held a `std::vector<Box>` by value. The
   PIMPL `pimpl_inventory.h` *only mentions* `Box` in method
   signatures (parameters and return types), so a forward
   declaration of `Box` is enough. The `.cpp` is where the
   full definition is needed. This is the recompilation /
   ABI payoff.
3. **What goes in `Impl` is up to you.** I put the
   `std::vector<Box>` in `Impl`, plus a version counter, plus
   a private `print` helper. The version counter has no
   functional role — it's there to show that the *only*
   constraint on what the `Impl` carries is "whatever you
   decide to put in it." A real `Impl` might carry a hash map
   for O(1) lookup, a `std::string name_`, a mutex, anything.
4. **The "moved-from" state of a PIMPL class is `impl_ == nullptr`,
   and you can choose to make it safe to call methods on.**
   The Jun 17 lesson glossed over this; this lesson goes
   deeper. Standard library types make the moved-from state
   safe (e.g. `std::vector` after a move is empty, not
   null). For a PIMPL class, the natural choice is the same:
   the moved-from object is empty (size 0, no entries) and
   can be reused for new `add()` calls. I make `add()`,
   `replace_at()`, and `take()` lazy-init `impl_` if it's
   null, and make `size()` and `print()` defensive.
5. **The deep copy is a `*impl_` copy, which copies the
   vector, which copies the Boxes.** A subtle but important
   chain: the PIMPL copy ctor is
   `impl_(std::make_unique<Impl>(*other.impl_))`. This calls
   `Impl`'s copy ctor, which (implicitly) copies
   `boxes_`, which (implicitly) copies the `Box` objects via
   `Box`'s copy ctor. Each `Box` is deep-copied. The version
   counter is also copied, so the new `Inventory` starts at
   the same version as the source. (Mutations after the copy
   diverge the two, which Section 3 of the program shows.)

The files for this lesson are at
`late-may/cpp_practice/P-2026-06-18-pimpl-inventory.cpp`,
plus `pimpl_inventory.h` and `pimpl_inventory.cpp`. The
naming convention (`pimpl_inventory.{h,cpp}`) mirrors Jun
17's `pimpl_box.{h,cpp}`, so the lesson history stays
self-contained.

## Build and run

```bash
# One-liner, matching the cron's default flags.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o /tmp/pimpl_inv \
    P-2026-06-18-pimpl-inventory.cpp \
    pimpl_inventory.cpp box.cpp
/tmp/pimpl_inv

# ASan + UBSan, for the run that proves no leaks and no UB.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    -o /tmp/pimpl_inv_asan \
    P-2026-06-18-pimpl-inventory.cpp \
    pimpl_inventory.cpp box.cpp
/tmp/pimpl_inv_asan
```

Both builds are clean (no warnings under `-Wall -Wextra
-Wpedantic`). The output walks through six sections:
layout, basic ops, deep copy, move, header stability, and
a "pure client" demo.

## Files in this lesson

| File                          | Role                              | Compiles to       |
|-------------------------------|-----------------------------------|-------------------|
| `pimpl_inventory.h`           | `PimplInventory` decl, `Impl` fwd | (header, no .o)   |
| `pimpl_inventory.cpp`         | `Impl` definition + member bodies | (linker)          |
| `P-2026-06-18-pimpl-inventory.cpp` | `main()` + `PureClient` demo   | (linker)          |

Kept distinct from the Jun 16 `inventory.{h,cpp}` and the
Jun 17 `pimpl_box.{h,cpp}` so each lesson's history stays
intact. The naming convention `pimpl_*` is the giveaway.

## Key ideas

### The header is now a pure API

Compare the two headers side by side.

**Jun 16** (`inventory.h`, ~46 lines):

```cpp
#include <cstddef>
#include <string>
#include <vector>
#include "box.h"

class Inventory {
public:
    void add(Box b);
    void replace_at(std::size_t i, Box&& b);
    Box take(std::size_t i);
    std::size_t size() const noexcept;
    void print(const std::string& tag) const;

private:
    std::vector<Box> boxes_;
};
```

**Jun 18** (`pimpl_inventory.h`, ~95 lines, with comments):

```cpp
#include <cstddef>
#include <memory>
#include <string>

class Box;  // forward decl only -- the full definition
            // is not needed for the API surface

class PimplInventory {
public:
    PimplInventory();
    ~PimplInventory();

    PimplInventory(PimplInventory&&) noexcept;
    PimplInventory& operator=(PimplInventory&&) noexcept;
    PimplInventory(const PimplInventory&);
    PimplInventory& operator=(const PimplInventory&);

    void add(Box b);
    void replace_at(std::size_t i, Box&& b);
    Box take(std::size_t i);
    std::size_t size() const noexcept;
    void print(const std::string& tag) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

Differences:

- `pimpl_inventory.h` does **not** include `box.h` or
  `<vector>`. It forward-declares `Box` and `<memory>` is
  the only standard include (for `std::unique_ptr`).
- The class is now five members wider in declaration
  (default ctor, dtor, the Big Five) but **one member
  narrower in data**: `impl_` replaces `boxes_`, the
  version counter, the helpers.
- A client TU that includes `pimpl_inventory.h` doesn't see
  `box.h` at all. It can put a `PimplInventory` on the
  stack, call `size()`, call `print("hello")`, but it
  cannot construct a `Box` argument for `add()` without
  including `box.h` itself. That last constraint is the
  one the "pure client" demo in Section 6 of the program
  addresses: a class can hold a `PimplInventory` and *use*
  the API without ever including `box.h`, as long as the
  *implementation* of those uses (i.e. the `.cpp` that
  actually constructs a `Box`) does include it.

This is the same PIMPL idea as Jun 17's `Box`, applied to a
class that's strictly larger. The point is to show the rule
generalises: any class with a non-trivial private layout
can be PIMPLed, and the boilerplate is mechanical.

### The "Big Five" of a PIMPL Inventory, in one table

| Special member          | Where it goes | Why                                                                                |
|-------------------------|---------------|------------------------------------------------------------------------------------|
| Default ctor            | `.cpp`        | The ctor calls `std::make_unique<Impl>()`, which needs `Impl` complete.            |
| Destructor              | `= default` in `.cpp` (declared in header) | `delete impl_` needs `Impl` complete; the dtor must be defined where the type is visible. |
| Copy ctor               | Declared in header, defined in `.cpp` | `unique_ptr` is non-copyable; we declare a deep-copy ctor in the header to suppress the implicit deleted one, and define it in the `.cpp` where `Impl` is complete. |
| Copy assign             | Declared in header, defined in `.cpp` (copy-and-swap for strong-exception guarantee) | Same reason as copy ctor. |
| Move ctor               | `= default` in header | Moving a `unique_ptr<Impl>` is a pointer transfer, no need for `Impl` to be complete. |
| Move assign             | `= default` in header | Same reason. |

This is the same table as Jun 17's, with one extra row
(default ctor), because the Inventory has a non-default
constructor (the `std::vector<Box>` is default-constructed,
but the `Impl` itself needs to be allocated).

### What goes in `Impl`?

The decision is the interesting bit. In this lesson the
`Impl` is:

```cpp
struct PimplInventory::Impl {
    std::vector<Box> boxes_;
    std::size_t version_ = 0;

    void bump_version() noexcept { ++version_; }

    void print(const std::string& tag) const {
        std::cout << "  PimplInventory(" << tag << ")"
                  << " [size=" << boxes_.size()
                  << " version=" << version_ << "]:\n";
        for (std::size_t i = 0; i < boxes_.size(); ++i) {
            std::cout << "    [" << i << "] '"
                      << boxes_[i].label() << "'\n";
        }
    }
};
```

Three observations:

- **`boxes_`** is the actual storage. It had to move into
  `Impl` because the PIMPL class is now a `unique_ptr<Impl>`,
  not a `vector<Box>` directly.
- **`version_`** is a free addition. It bumps on every
  mutation. It's not load-bearing — the lesson would
  compile and work without it — but it gives the trace
  output a way to distinguish "the same inventory"
  (same version) from "a copy" (starts at the same
  version, then diverges). Section 3 of the program shows
  this concretely: copy `a` to `b`, mutate `b` twice, and
  the trace shows `a` at version 3 and `b` at version 5.
- **`bump_version()` and `print()`** are private helpers
  on `Impl`. They don't *have* to be members; they could
  be free functions in the `.cpp` that take an `Impl&`.
  Making them members is a stylistic choice — it groups
  the data and the operations on it, and the `PimplInventory`
  methods become thin one-liners that forward to the
  `Impl` member.

The lesson's deeper point: the `Impl` is a class in its
own right. The `PimplInventory` is a *facade* over it. The
two have a one-to-one correspondence in their public
methods (`add`, `replace_at`, `take`, `size`, `print`).
That's the textbook PIMPL shape.

### The moved-from state, in detail

After a `PimplInventory other = std::move(src);`, the
state of `other` is "owns a valid `Impl`" and the state of
`src` is "`impl_` is null." The `= default` move ctor
transfers the `unique_ptr`, leaving the source empty.

This is the same as `std::unique_ptr`'s own moved-from
state. The standard says moved-from objects are in a
"valid but unspecified" state — the *only* guarantees are
that destruction works and assignment into them works.
Methods other than those are not required to work.

For a PIMPL class, the natural extension is to make
methods safe to call on the moved-from state. In this
lesson I make:

- **`size()`** return 0 if `impl_` is null. The natural
  reading of "moved-from inventory" is "empty inventory."
- **`print()`** print a `[moved-from, empty]` line. This
  is honest: the user shouldn't be surprised that calling
  `print()` on a moved-from object doesn't show data.
- **`add()`**, **`replace_at()`**, **`take()`** lazy-init
  `impl_` if it's null. This makes the object *recoverable*:
  a moved-from inventory can be reused for new additions.
  This is the same pattern used by `std::vector` after a
  move: `vec = std::move(other); vec.push_back(x);` works
  because `vector::push_back` is robust to the moved-into
  state. (Strictly, `vector` doesn't "lazy init"; the
  moved-into vector is itself a valid vector. But the
  principle is the same — the user can keep using the
  object after a move.)

The pattern is documented in the program with a long
comment near the `size()` and `add()` definitions, and
Section 4 of the program demonstrates the round-trip:
move `src` into `dst`, see that `src.size() == 0`, then
move-assign `src` into `other`, then `other.add(...)` and
verify the result. The trace shows the full sequence
without any crash.

### The deep copy, in detail

The PIMPL copy ctor is:

```cpp
PimplInventory::PimplInventory(const PimplInventory& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {
    std::cout << "  [PimplInventory copy ctor] copying "
              << other.impl_->boxes_.size() << " Boxes (deep)\n";
}
```

The chain of copies:

1. `*other.impl_` calls `Impl`'s copy ctor.
2. `Impl`'s implicit copy ctor copies each member in turn.
3. The `std::vector<Box>` is copy-constructed; this calls
   `Box`'s copy ctor for each element.
4. `Box`'s copy ctor (declared in `box.h`, defined in
   `box.cpp`) is a deep copy of the `std::string label_`.

So a copy of a `PimplInventory` with three Boxes triggers
three `Box` copy ctors. The trace from Section 3 shows
exactly this:

```
[Box] COPY ctor ('a1') @0x607000000090
[Box] COPY ctor ('a2') @0x6070000000a8
[Box] COPY ctor ('a3') @0x6070000000c0
[PimplInventory copy ctor] copying 3 Boxes (deep)
```

(The `Box` copy ctors fire *first*, then the `PimplInventory`
copy ctor body. That's the order of construction: the
argument to a function is evaluated before the function is
called. `std::make_unique<Impl>(*other.impl_)` evaluates
`*other.impl_` — which copies the `Impl`, which copies the
vector, which copies the Boxes — *before* `make_unique`
actually allocates the new `Impl`.)

After the copy, `a` and `b` are independent: mutating `b`
triggers more `Box` move assigns but never touches `a`'s
Boxes. The trace shows `a` at version 3 throughout, and
`b`'s version climbing to 5 after two mutations.

### The header-stability payoff, observed

The capstone of the PIMPL lessons (Jun 17 and Jun 18) is
the observation that the client TU's `.o` file doesn't
change when the `Impl` evolves. The way to *observe* this
in a single program is the one Section 5 of the program
uses: it prints `sizeof(PimplInventory)` (8 bytes) and
`sizeof(Box)` (24 bytes) and points out that the first is
*unaffected* by the second.

A *more* compelling demo would be a two-step compile:

```bash
# Step 1: build the "library" -- pimpl_inventory.cpp and box.cpp
g++ -c pimpl_inventory.cpp box.cpp

# Step 2: build a "client" TU that only includes pimpl_inventory.h
g++ -c client.cpp

# Step 3: link
g++ -o my_app client.o pimpl_inventory.o box.o

# Step 4: edit pimpl_inventory.cpp -- add a field to Impl, e.g.
#         std::string name_; -- and rebuild only pimpl_inventory.cpp
g++ -c pimpl_inventory.cpp
g++ -o my_app client.o pimpl_inventory.o box.o   # link, no recompile of client.o
```

`client.o` is unchanged in Step 4. The linker resolves
the symbols against the *new* `pimpl_inventory.o`. The
binary works. That's the ABI / rebuild promise in action.

I didn't do this in the program because it would need a
second file and a build-script demonstration that's hard
to make scannable in a single `main()`. The static
assertion in the program (`static_assert(sizeof(PimplInventory)
== sizeof(void*))`) and the `sizeof` print in Section 1 are
the closest single-file proxies. The full demo is on the
"Next Steps" list for a future lesson.

## Design choices and trade-offs

### Why the `Impl` carries a `print` helper, not just data

A `PimplInventory` could be a *pure* facade — the `Impl`
is a `struct` of state, and the `PimplInventory` methods
are the ones that walk it. I chose to put a `print` member
on `Impl` instead, for two reasons:

- It keeps the trace-formatting code next to the data it
  walks. If the trace format ever changes, the change is
  one file, not two.
- It makes the `Impl` look more like a *class* (data +
  operations) and less like a *POD* (just data). The
  `PimplInventory` then becomes the facade, the `Impl` is
  the implementation class. This is a useful distinction
  when PIMPL gets bigger — the `Impl` is the natural place
  to grow the implementation; the `PimplInventory` is the
  natural place to grow the public API.

Other methods (`add`, `replace_at`, `take`, `size`) are
written as direct `impl_->boxes_.push_back(std::move(b))`
access from the `PimplInventory` body. The lesson shows
both styles — the all-in-one-`Impl` `print` and the
direct-access everything else — and lets the reader
decide which they prefer.

### Why the `Impl` carries a version counter

The version counter has no functional role. It's there
to make the deep-copy section (Section 3) of the program
*visible*. Without it, copying an inventory and then
mutating the copy would still work, but the trace would
just be a list of `Box` ctor / dtor lines with no signal
about which inventory was which.

With the version counter, the trace is unambiguous:
`PimplInventory(a) [size=3 version=3]` versus
`PimplInventory(b) [size=4 version=5]`. The two are
distinct objects, mutating `b` does not affect `a`'s
version, and the deep copy is independent. That's the
proof the lesson wanted to make.

A real `PimplInventory` would not necessarily carry a
version counter. But it's a one-line addition to `Impl`
that has zero cost (it's just a `size_t`) and makes the
trace a hundred times easier to read. I included it.

### Why lazy-init in `add()` (and friends), not in the ctor

The `= default` move ctor leaves the source's `impl_` as
a null `unique_ptr`. Calling `add()` on the source would
crash because `add()` dereferences `impl_` immediately.

Two ways to fix this:

1. **Lazy-init in `add()`.** Check `if (!impl_) impl_ = std::make_unique<Impl>();`
   at the start. This is what I did. The cost is one
   branch on every call, which is negligible. The
   benefit is that the moved-from object is "recoverable"
   in the same way `std::vector` is — you can keep using
   it after a move.
2. **Write out the move ctor in the `.cpp` to *re-initialise*
   the source's `impl_`.** This violates the "move ops
   are `= default` in the header" rule. I'd rather keep
   the move ops defaulted (they're obviously correct) and
   add a tiny branch in the mutating methods.

The lazy-init pattern is the more common one in real
PIMPL code. The moved-from state of a `PimplInventory`
is treated as an empty-but-valid inventory, the same way
`std::vector`'s moved-from state is treated as an
empty-but-valid vector.

### Why `size()` is `noexcept` but checks for null

`size()` is `noexcept` because the operation is supposed
to be non-throwing. The check `impl_ ? impl_->boxes_.size()
: 0` is also non-throwing. The combination is consistent:
`size()` cannot throw, even on a moved-from object.

(If the moved-from state were a "valid but unspecified
state" and we chose to *not* defend against it, the
function would be `noexcept` but would crash on a
moved-from object. That's strictly worse than defending,
and it would violate the principle of least surprise.)

### Why I kept the PIMPL Inventory separate from the Jun 16 Inventory

Same reason as Jun 17: the Jun 16 `Inventory` is used by
the multi-file example, and its `Inventory::boxes_` is a
`std::vector<Box>`. If I had edited `inventory.h` /
`inventory.cpp` to add PIMPL, the Jun 16 example would
have stopped compiling (a PIMPL `Inventory` doesn't *have*
a `boxes_` member, so any code that does
`inventory.boxes_.push_back(...)` would fail). Keeping the
two lessons separate (`inventory.{h,cpp}` and
`pimpl_inventory.{h,cpp}`) means each lesson's code can
be read and run in isolation.

## Cross-references and follow-ups

- **Jun 4 (smart pointers)** — `std::unique_ptr<Impl>` is
  the heart of PIMPL. The Jun 4 lesson introduced
  `unique_ptr` and `shared_ptr`; the Jun 17 and Jun 18
  lessons put `unique_ptr` to work in the canonical PIMPL
  shape.
- **Jun 9 (rvalue refs, `std::move`)** — the move ctor
  here is `= default` in the header. The same `= default`
  shape as Jun 17's `Box`. The trace would look the same:
  a single pointer transfer (no `Box` ctor calls), except
  for the `Box` ctor calls in the *contents* of the
  vector, which the moved-into object now owns.
- **Jun 12 (`std::move_if_noexcept`)** — the move ops are
  `noexcept`, so a `std::vector<PimplInventory>` would
  move on reallocation. If the move ops weren't
  `noexcept`, the vector would fall back to copy. The
  deep copy exists; whether it's used is up to `noexcept`.
- **Jun 13 (`enable_shared_from_this`)** — a PIMPL class
  can be put inside a `shared_ptr` and inherit from
  `enable_shared_from_this`. The `Impl` would hold a
  `weak_ptr<PimplInventory>` so the Impl can hand out
  shared references to its owner. Combines cleanly with
  this lesson's PIMPL shape.
- **Jun 14 (`std::span` by hand)** — a `PimplInventory::span()`
  method could return a `psp::Span<const Box>` over the
  internal vector. A small extension that exercises the
  PIMPL indirection in practice.
- **Jun 16 (multi-file `Inventory`)** — the public surface
  of this PIMPL `PimplInventory` is the same as Jun 16's
  `Inventory`. A natural next exercise: take an existing
  client of Jun 16's `Inventory` and re-target it at
  `PimplInventory`, and observe (via `-ftime-report` or a
  rebuild script) that the client's compile time drops
  because `box.h` is no longer transitively pulled in.
- **Jun 17 (PIMPL `Box`)** — the same pattern, applied
  one level deeper. PIMPL the Box, then PIMPL the
  Inventory, and you have two layers of indirection. The
  Box's `Impl` is independent of the Inventory's `Impl`,
  so the rebuild graph stays fine-grained: a change to
  the Box's private state invalidates only
  `pimpl_box.cpp`; a change to the Inventory's private
  state invalidates only `pimpl_inventory.cpp`.

## Next Steps

- **Real ABI measurement** (carried over from Jun 17) —
  write a tiny client TU that includes `pimpl_inventory.h`,
  save its `.o`. Edit `pimpl_inventory.cpp` (add a field
  to `Impl`), recompile *only* `pimpl_inventory.cpp` and
  the binary, and run the client. The client `.o` is
  unchanged. The link succeeds. The run works. That's
  PIMPL's promise in action. (Still on the list — would
  be a great capstone. Would require a small `Makefile`.)
- **PIMPL `PimplInventory` with `shared_ptr`** — replace
  the `unique_ptr<Impl>` with a `shared_ptr<Impl>`. The
  copy ctor / assign become defaultable in the header.
  The deep copy disappears. The cost is reference
  counting. A useful comparison and a small lesson. (New
  on the list.)
- **`std::span` accessor** — add a
  `PimplInventory::span() -> psp::Span<const Box>` method
  that exposes a read-only view of the internal vector.
  Combines with Jun 14's `psp::Span` and exercises the
  PIMPL indirection in a useful direction. (New on the
  list.)
- **The lazy-init pattern, formalised** — extract the
  "recoverable moved-from state" pattern into a small
  base class or CRTP helper. The pattern repeats in every
  PIMPL class: every mutating method needs the
  `if (!impl_) impl_ = std::make_unique<Impl>();` guard.
  A helper would let the methods be written straight
  through. (New on the list.)
- **PIMPL and virtual functions** — make `PimplInventory`
  polymorphic (add a virtual dtor and a virtual `print`)
  and observe that the `vptr` lives *outside* the `Impl`
  (in the `PimplInventory` itself, via the `unique_ptr`).
  Worth a small demo, and ties back to the Jun 17 follow-up
  on the same idea. (New on the list.)
- **PIMPL `PimplInventory` with a hash map in `Impl`** —
  a non-trivial extension that shows the `Impl` carrying
  multiple data structures. A `std::unordered_map<std::string,
  std::size_t>` keyed on `Box::label_` would let
  `PimplInventory::find_by_label(s) -> std::size_t` return
  an index in O(1). The `Impl` grows; the header doesn't.
  (New on the list.)

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit (all under `late-may/cpp_practice/`):
  - `P-2026-06-18-pimpl-inventory.cpp` (main)
  - `P-2026-06-18-pimpl-inventory.md` (this file)
  - `pimpl_inventory.h`
  - `pimpl_inventory.cpp`
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*PIMPL generalises. The boilerplate is mechanical: a forward
declaration of `Impl`, a `std::unique_ptr<Impl>` member, a
declared-but-not-defined dtor in the header, copy ops that
look at the inner `Impl` only in the `.cpp`, and `= default`
move ops in the header. The pattern applies to `Box` (Jun 17)
and `Inventory` (Jun 18) with the same shape, and it would
apply to anything with a non-trivial private layout. The
payoff is the same: a stable header, no transitive `box.h`
in client TUs, and a fine-grained rebuild graph where
private changes invalidate only the `.cpp` that owns the
`Impl`. The lesson's small but important new wrinkle is
the moved-from state: a `PimplInventory` after a move has
`impl_ == nullptr`, and we can choose to make that state
safe (lazy-init on mutating ops, defensive on `size`/
`print`) the same way `std::vector` makes its moved-from
state safe. After that, the pattern is just a discipline
applied uniformly across every class that wants it.*
