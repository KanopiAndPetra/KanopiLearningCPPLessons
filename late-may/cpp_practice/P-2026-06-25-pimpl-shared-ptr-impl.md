# C++ practice 2026-06-25 — `shared_ptr<Impl>` PIMPL

## What I set out to learn

The Jun 24 session closed the PIMPL arc with an ABI-stability
capstone (`unique_ptr<Impl>`, same `driver.o` linking against two
different `Impl.o`s). Jun 24's notes flagged one remaining
follow-on:

> **`shared_ptr<Impl>` PIMPL** — replace `unique_ptr<Impl>` with
> `shared_ptr<Impl>`. Copy ctor / assign become defaultable; deep
> copy disappears; cost becomes refcount.

That's today's session. The single facade-file swap is small, but
it surfaces several concrete design questions:

1. **How big is the facade now?** `unique_ptr<Impl>` was 8 bytes
   (one raw pointer). `shared_ptr<Impl>` is two pointers (the
   `T*` and the control-block*).
2. **Are copy ctor / copy assign defaultable?** With
   `shared_ptr<Impl>`, yes — and the compiler-generated versions
   correctly bump the refcount.
3. **What does the moved-from state look like?** With
   `unique_ptr<Impl>`, the source ends up null (a "zombie"
   `Widget` that has a null `unique_ptr`). With `shared_ptr<Impl>`,
   the source ends up with a **null** `shared_ptr` (not a
   `shared_ptr` to a copied Impl — the move transfers, doesn't
   duplicate).
4. **Does the ABI promise still hold?** Same question as Jun 24:
   the driver's compiled view of `Widget` should be exactly the
   public API in the header. No `Impl` leakage.

## What I confirmed

### (1) `sizeof(Widget) = 16` (vs 8 for `unique_ptr<Impl>`)

```text
sizeof(Widget)        = 16 bytes
alignof(Widget)       = 8  bytes
is_trivially_copyable = false
is_copy_constructible = true
is_copy_assignable    = true
is_nothrow_move_constructible = true
```

`shared_ptr<T>` on libstdc++/libc++ is two pointers: the `T*` and
the `control_block*` (which holds the strong refcount, the weak
refcount, and the deleter). The PIMPL facade is therefore **16
bytes** on 64-bit platforms — twice the size of the
`unique_ptr<Impl>` facade (8 bytes) from the Jun 17/18/24 sessions.

This is a real, observable cost. If you're using PIMPL precisely
because the facade should be small and ABI-stable, the 16-byte
facade is still small (it's two cache-line-friendly pointers), but
it is **not** as cheap as the 8-byte version. The tradeoff is what
you get in return.

### (2) Copy ctor / copy assign are defaultable

```cpp
// header
Widget(const Widget& other);
Widget& operator=(const Widget& other);

// cpp
Widget::Widget(const Widget& other)            = default;
Widget& Widget::operator=(const Widget& other) = default;
```

Compare to the `unique_ptr<Impl>` version (Jun 17):

```cpp
Widget(const Widget&)            = delete;
Widget& operator=(const Widget&) = delete;
```

With `shared_ptr<Impl>`, the copy is **free** at the source level —
the compiler-generated version calls `shared_ptr`'s copy ctor,
which atomically bumps the strong refcount. No manual clone of
`Impl`. No `Impl` knows-itself-by-value duplication. The header
gets shorter; the cpp gets shorter; the runtime cost is one
atomic increment on copy and one atomic decrement on destruction.

The output confirms the refcount rises on copy:

```text
=== copy shares Impl (refcount) ===
after copy ctor:   w1.use_count() = 2, w2.use_count() = 2
after w2.set_name: w1.name() = "alpha-via-w2", w2.name() = "alpha-via-w2"
after w2.set_value(42): w1.value() = 42, w2.value() = 42
```

Mutations through one copy are visible through the other — exactly
the semantics of a shared state. This is the **defining
characteristic** of `shared_ptr<Impl>` PIMPL: copies share.

### (3) Moved-from is null (not "transferred")

```text
=== move (moved-from becomes null) ===
after move ctor:   w4.use_count() = 3, w1.use_count() = 0  (w1 is moved-from -> 0)
```

After `Widget w4 = std::move(w1);`, `w1.impl_` is null. **Don't
call `w1.name()` after a move** — it dereferences a null `Impl*`
and is UB. The driver explicitly avoids this; instead it shows
`w1.use_count() == 0` and then re-establishes `w1` with a fresh
Impl.

The contrast with `unique_ptr<Impl>` is instructive:

```cpp
// unique_ptr<Impl>: moved-from has null impl_.
//                  (same shape, but now it's a "zombie" with
//                   a null unique_ptr — not assignable-to.)
// shared_ptr<Impl>: moved-from has null impl_.
//                  (same shape, but now reassignable via
//                   w1 = Widget("..."), which works because
//                   operator= doesn't care that the source was null.)
```

Both facades reach a "null impl_" state after move. The behavioral
difference is that **`shared_ptr<Impl>` allows clean re-assignment
of the moved-from object** without any special handling — the
`shared_ptr` operator= takes ownership of the new shared_ptr's
control block and the old null one is destroyed (a no-op).

### (4) Copy-assign drops the old Impl correctly

```text
=== copy assignment ===
before assign:     w3.use_count() = 1 (independent Impl), w1.use_count() = 2
after w3 = w1:     w3.use_count() = 3, w1.use_count() = 3
```

`w3 = w1` does two things atomically:

1. `w3`'s old `shared_ptr<Impl>` is destroyed — its refcount goes
   from 1 to 0 (the "independent Impl" is freed).
2. `w3`'s new `shared_ptr<Impl>` is copy-constructed from `w1`'s,
   bumping the shared group's refcount from 2 to 3.

The refcount went 1 → 3 because we destroy w3's old Impl
(decrementing its own private refcount) AND add w3 to the shared
group (incrementing w1's group's refcount). The driver observes
the net effect.

### (5) Scope-bound refcount tracking

```text
=== refcount tracking with scope ===
before inner scope: outer.use_count() = 1
inside scope (2 copies): outer.use_count() = 3, inner1.use_count() = 3, inner2.use_count() = 3
(all three share the same Impl)
after scope: outer.use_count() = 1  (inner1 and inner2 are destroyed -> refcount drops)
```

This is the cleanest demonstration of refcount-correctness:

- 1 → 3 when two copies are made (one refcount bump per copy).
- 3 → 1 when the two copies go out of scope (one refcount drop
  per destruction).
- The shared `Impl` itself is destroyed exactly once, when the
  refcount hits 0 — which happens at the end of `main()`, not
  earlier.

### (6) ABI promise still holds — `nm driver.o` shows public API only

```text
$ nm /tmp/driver.o | c++filt | grep Widget | sort -u
Widget::Widget(Widget const&)
Widget::Widget(Widget&&)
Widget::Widget(std::string)
Widget::name() const
Widget::operator=(Widget const&)
Widget::operator=(Widget&&)
Widget::set_name(std::string)
Widget::set_value(int)
Widget::use_count() const
Widget::value() const
Widget::version() const
Widget::~Widget()
```

Twelve symbols (vs eight for the `unique_ptr<Impl>` version) —
the extra four are the copy/move ctor and copy/move assign, all
declared in `pimpl_shared_widget.h`. **No `Impl` symbols leak
into the driver.** The driver's compiled view of `Widget` is
exactly the public API in the header, same as Jun 24.

The one observable difference at the public API level is the
addition of `use_count()`, which is the deliberate escape hatch
for observing the refcount — same role as `version()` in Jun 24:
expose internals when the design wants to.

### (7) ASan + UBSan clean

The ASan build runs to completion with no leaks, no
use-after-free, no UB. The refcount drops correctly as Widgets
go out of scope, and the shared `Impl` is destroyed exactly once
at program exit.

## The header — same shape, different smart pointer

```cpp
// pimpl_shared_widget.h
class Widget {
public:
    explicit Widget(std::string name);
    ~Widget();

    // Defaultable! No manual clone needed.
    Widget(const Widget& other);
    Widget& operator=(const Widget& other);

    // Defaultable! shared_ptr move is noexcept.
    Widget(Widget&& other) noexcept;
    Widget& operator=(Widget&& other) noexcept;

    const std::string& name() const;
    void set_name(std::string s);
    int value() const;
    void set_value(int v);

    long use_count() const;          // deliberate escape hatch
    std::uint64_t version() const;

private:
    struct Impl;                     // forward decl — header is still clean
    std::shared_ptr<Impl> impl_;     // <-- the only change vs Jun 17/18/24
};
```

Compare to Jun 17's header (the `unique_ptr<Impl>` version):

```cpp
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;     // 8-byte facade
```

That's the whole change. The PIMPL idiom generalizes to either
smart pointer; the choice is a design decision (see below).

## `unique_ptr<Impl>` vs `shared_ptr<Impl>` PIMPL — when to pick which

| Aspect               | `unique_ptr<Impl>` (Jun 17/18/24)        | `shared_ptr<Impl>` (today)              |
|----------------------|-------------------------------------------|------------------------------------------|
| `sizeof(Widget)`     | 8 bytes                                   | 16 bytes (two pointers)                  |
| Copy semantics       | DELETED (or hand-written deep clone)      | defaultable; copies share                |
| Move semantics       | pointer transfer, source becomes null     | pointer transfer, source becomes null    |
| Cost per copy        | one `Impl` clone (potentially expensive)  | one atomic refcount bump                 |
| Cost per destruction | one `Impl` delete                         | one atomic refcount drop                 |
| "Zombie" reassignment | requires careful state-tracking           | trivial: `w1 = Widget("rebuilt");`      |
| When to use          | when value semantics matter; when copies are expensive or unwanted; when you want strict ownership | when copies are natural and shared state is wanted; when you want cheap, defaultable copy |
| Real-world example   | `std::filesystem::path`'s PIMPL (libstdc++ uses unique_ptr in some impls) | Qt's COW data structures (historically) |

The two designs are not interchangeable. They encode different
ownership models. `unique_ptr<Impl>` says "I am the sole owner";
`shared_ptr<Impl>` says "I am one of several owners, all equal".
The PIMPL idiom preserves the ABI-isolation benefits either way.

## What I took away

- **`shared_ptr<Impl>` PIMPL is a real design choice, not just a
  smart-pointer swap.** The 16-byte facade is twice the size of
  the `unique_ptr<Impl>` facade. The trade is defaultable copy
  semantics.
- **Copies share the Impl.** Mutations through one Widget are
  visible through every other Widget that shares the same Impl.
  This is the defining property of `shared_ptr<Impl>` PIMPL and
  the reason the API deliberately exposes `use_count()` — so the
  caller can observe the sharing.
- **The moved-from state is a null `shared_ptr`.** Calling public
  observers on a moved-from Widget is UB. Re-assignment works
  cleanly because `shared_ptr<Impl>::operator=(shared_ptr<Impl>)`
  is fine with a null source.
- **The ABI promise is preserved.** Same as Jun 24: the driver's
  compiled view of `Widget` is exactly the public API declared
  in the header. The `Impl` is hidden behind the facade. The
  twelve `Widget` symbols in `driver.o` are exactly the public
  surface — no more.
- **The out-of-line dtor is still required.** Same reasoning as
  the `unique_ptr<Impl>` version: destroying `shared_ptr<Impl>`
  needs the complete `Impl` type, so the dtor must be defined in
  the cpp, not defaulted in the header.

This closes the `shared_ptr<Impl>` follow-on from Jun 24. The
remaining follow-on from Jun 24:

> **Acyclic visitor** — `dynamic_cast` *inside* the visitor's
> `visit(Base&)` to break the dependency cycle of the classic
> visitor pattern. Loses double-dispatch but gains extensibility.

That's a candidate for the next session.

## Build and run

```bash
cd late-may/cpp_practice/

# Standard build (with -Wall -Wextra -Wpedantic).
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    P-2026-06-25-pimpl-shared-ptr-impl.cpp pimpl_shared_widget.cpp \
    -o P-2026-06-25-pimpl-shared-ptr-impl
./P-2026-06-25-pimpl-shared-ptr-impl

# ASan + UBSan.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined \
    P-2026-06-25-pimpl-shared-ptr-impl.cpp pimpl_shared_widget.cpp \
    -o P-2026-06-25-pimpl-shared-ptr-impl-asan
./P-2026-06-25-pimpl-shared-ptr-impl-asan

# ABI-style observability (driver.o's view of Widget).
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -c P-2026-06-25-pimpl-shared-ptr-impl.cpp -o /tmp/driver.o
nm /tmp/driver.o | c++filt | grep Widget | sort -u
```

All builds clean under `-Wall -Wextra -Wpedantic`. ASan+UBSan
clean — no leaks, no UB, refcount drops correctly as Widgets go
out of scope.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files (under `late-may/cpp_practice/`):**
  - `pimpl_shared_widget.h` — the public header (16-byte facade)
  - `pimpl_shared_widget.cpp` — the Impl definition + methods
  - `P-2026-06-25-pimpl-shared-ptr-impl.cpp` — the driver
  - `P-2026-06-25-pimpl-shared-ptr-impl.md` — this file
- **No existing files modified.**
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*Today covers the `shared_ptr<Impl>` variant of PIMPL. The facade
doubles in size (8 → 16 bytes) but copy ctor and copy assign
become defaultable, and copies share the same Impl. The ABI
promise is preserved: the driver's compiled view of `Widget` is
exactly the public API in the header, with no `Impl` leakage.
The two PIMPL variants — `unique_ptr<Impl>` for value semantics
and `shared_ptr<Impl>` for shared semantics — encode genuinely
different ownership models, and the PIMPL idiom supports both
without compromising ABI isolation.*