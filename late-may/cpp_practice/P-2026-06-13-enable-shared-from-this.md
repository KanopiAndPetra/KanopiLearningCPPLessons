# C++ practice 2026-06-13 — `std::enable_shared_from_this` (C++11)

## What I set out to learn

`std::enable_shared_from_this<T>` is the standard library's answer to a
question that comes up the moment you start writing classes that need
to hand out references to themselves:

> I have a class. Some `shared_ptr<MyClass>` already owns it. I need
> a method on the class to *return a `shared_ptr<MyClass>` to `this`*.
> What do I do?

The naive answer — `return shared_ptr<MyClass>(this);` — is a bug. Each
call constructs a *fresh control block* that thinks it owns the object.
The original `shared_ptr` (the one that actually allocated `this`) and
the one returned here will *both* try to delete the object when their
refcounts hit zero. That's a double-free, or worse, a use-after-free
when one control block's refcount hits zero first and frees while the
other is still in use.

`std::enable_shared_from_this<T>` is the fix. You derive your class
from it, and it gives you two methods that use the *same* control
block the original `shared_ptr` installed:

- `shared_from_this()` — returns a `shared_ptr<T>` with unified refcount.
- `weak_from_this()` (C++17) — returns a `weak_ptr<T>` that observes
  the same control block without extending the lifetime.

The class is a CRTP base, and the magic happens inside `std::make_shared`
and the `shared_ptr` constructors. When you create a `shared_ptr<T>`
for a type `T` that derives from `enable_shared_from_this<T>`, the
`shared_ptr` constructor recognizes the base and stores a *weak pointer*
to `this` inside the base. `shared_from_this()` later *promotes* that
weak pointer by constructing a fresh `shared_ptr` that uses the same
control block. No second control block, no double-free, no problem.

The file is `cpp_practice/P-2026-06-13-enable-shared-from-this.cpp`.
Path matches the Jun 9–12 convention (`cpp_practice/`, P-prefix).

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o P-2026-06-13-enable-shared-from-this \
    P-2026-06-13-enable-shared-from-this.cpp
./P-2026-06-13-enable-shared-from-this
```

ASan build, also exercised:

```bash
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address \
    -o P-2026-06-13-enable-shared-from-this-asan \
    P-2026-06-13-enable-shared-from-this.cpp
ASAN_OPTIONS=halt_on_error=0 \
    ./P-2026-06-13-enable-shared-from-this-asan
```

## Sections at a glance

1. **`enable_shared_from_this<T>`**: derive from it, get `shared_from_this()` and `weak_from_this()` for free
2. **`shared_from_this()`**: returns a `shared_ptr<T>` with unified refcount
3. **`weak_from_this()`**: a non-owning observer that shares the control block
4. **Self-registration with a `Registry`**: the canonical real-world use case
5. **`shared_from_this()` before being managed by `shared_ptr` throws `std::bad_weak_ptr`**
6. **Move + esft**: returning `shared_from_this()` from a method is fine
7. **The aliasing ctor**: a member's `shared_ptr` shares the owner's control block
8. **The buggy pattern (in a forked child)**: why `shared_from_this()` exists

## Key ideas

### The CRTP base installs a weak pointer to `this` when the first `shared_ptr` is constructed

`std::enable_shared_from_this<T>` is a CRTP base class:

```cpp
template<class T>
class enable_shared_from_this {
    // ... internals: weak_ptr<T> weak_this_;
    // ... constructors, etc.
protected:
    enable_shared_from_this() noexcept = default;
    // ...
public:
    shared_ptr<T> shared_from_this();
    weak_ptr<T> weak_from_this() noexcept;
    // ...
};
```

The interesting bit is what happens *during* `std::make_shared<T>(...)`
or the `shared_ptr` constructor for a derived type. The standard
library's `shared_ptr` constructor template detects the `enable_shared_from_this`
base and calls an internal `__enable_weak_this()` helper that stores
a `weak_ptr` to `this` inside the base. From that point on, every call
to `shared_from_this()` from any method of the derived class constructs
a `shared_ptr<T>` that **promotes** that weak pointer, ending up with
a `shared_ptr` that uses the **same** control block as the original.

The key insight: the control block is installed by the *first* `shared_ptr`
to manage the object. After that, all subsequent `shared_ptr`s to the
same object must come from the same control block, or you get a
double-free. `shared_from_this()` is the only way to get a "new"
`shared_ptr` to an object that's *already* managed by one, *without*
creating a new control block.

### `weak_from_this()` is C++17 and the lifetime observer

Before C++17, if you wanted a `weak_ptr<T>` to `this`, you'd call
`shared_from_this()` and construct a `weak_ptr` from the result. C++17
added `weak_from_this()` directly, which avoids the throw-or-not
question and makes the intent explicit.

The use case for `weak_from_this()` is breaking reference cycles. If a
class has children that hold a `shared_ptr<parent>` back to it, the
parent's lifetime is pinned forever by the children's refs. The fix:
have the children hold a `weak_ptr<parent>` instead, so the parent
can expire when its *own* owner releases it. The child can `lock()`
the weak pointer to get a `shared_ptr` when it needs to use the parent.

Section 3 shows this in miniature: a `Session` exposes
`observe_self()` returning `weak_from_this()`. The trace shows the
weak pointer's `use_count()` (non-owning) staying at 1 even after
the strong `shared_ptr` is reset, and `weak.expired()` flipping to
`true` cleanly.

### Self-registration is the canonical reason esft exists

The pattern in section 4 is the one I keep coming back to:

```cpp
class Server : public std::enable_shared_from_this<Server> {
    // ...
    void register_with(Registry& reg) {
        reg.add(shared_from_this());
    }
};

static std::shared_ptr<Server> make_server(const std::string& name,
                                           Registry& reg) {
    auto sp = std::make_shared<Server>(name, reg);
    sp->register_with(reg);
    return sp;
}
```

Why this is the canonical pattern:

1. **The constructor can't safely call `shared_from_this()`.** Inside
   the constructor, the object isn't yet owned by any `shared_ptr` —
   `shared_from_this()` would throw `std::bad_weak_ptr`. The factory
   function constructs the object via `make_shared` (which installs
   the control block), then calls `register_with` (which can now use
   `shared_from_this()`), then returns a `shared_ptr` to the caller.

2. **Multiple `shared_ptr`s can co-own the object.** The caller holds
   one, the registry holds one, the worker holds one, etc. — all
   pointing to the same `Server` and the same control block, with a
   unified refcount.

3. **The object registers itself.** The factory doesn't need to know
   what registrations to do — the object knows what it needs to do,
   and it does them in `register_with` using the shared ownership
   semantics esft provides.

The "Why make the constructor private?" answer: if a caller could
bypass the factory and write `Server s(reg);` on the stack, then
somewhere else try to do `shared_ptr<Server>(...)` to it, they'd
create the double-free trap from section 8. The factory is the
*only* way to get a `Server` into existence, and it always uses
`make_shared`, so the control block is always installed before any
`shared_from_this()` call.

### Section 5: the failure mode of `shared_from_this()` is loud

`shared_from_this()` throws `std::bad_weak_ptr` if the object isn't
currently managed by any `shared_ptr`. The trace in section 5:

```
  [BadCall] ctor
  about to call raw.get_self() on a stack-allocated object...
  caught std::bad_weak_ptr: bad_weak_ptr
  [BadCall] dtor
```

The reason: `BadCall` is on the stack, never wrapped in a `shared_ptr`,
so the `__enable_weak_this()` helper was never called and the weak
pointer inside the base is empty. `shared_from_this()` tries to
promote the empty weak pointer, fails, and throws.

This is the *right* failure mode — a loud exception that says "you're
doing this wrong". The wrong failure mode (silent UB leading to a
crash 10 minutes later) is what `shared_ptr<T>(this)` gives you.

The detection story is also instructive: the same code that throws
on a stack-allocated object works fine on a heap-allocated object
managed by `shared_ptr` or `make_shared`. So the same line of code
(`return shared_from_this();`) is correct in a method that's called
on a managed object and throws in a method that's called on an
unmanaged one. The factory pattern guarantees it's always the former.

### The aliasing ctor (section 7) is a different trick

The aliasing ctor is `shared_ptr<Y>(shared_ptr<X> const& x, Y* p)`. It
constructs a `shared_ptr<Y>` that *uses* `x`'s control block (so `Y`'s
lifetime is tied to `x`'s) but stores `p` as its raw pointer. The
result is "a `shared_ptr<Y>` whose lifetime is governed by `X`".

Section 7 shows this with a `Tracked` member and a `Session` owner:

```cpp
auto session = std::make_shared<Session>("epsilon");
auto tracked = std::make_shared<Tracked>(42);

// tracked_via_session's control block is session's control block.
// tracked_via_session's raw pointer points to *tracked.get().
std::shared_ptr<Tracked> tracked_via_session(session, tracked.get());
// ... later ...
session.reset();  // tracked_via_session is now dangling, even if tracked is alive.
```

The use case: you have an `Outer` with a `Member`, and you want a
`shared_ptr<Member>` to expose to callers. The naive approach is
`shared_ptr<Member>(this)` from a method on `Outer`, which is the
section 8 bug — two control blocks, double-free. The right approach
is to have `Outer` keep its own `shared_ptr<Member>` internally
(via `make_shared` on construction) and have it return a *copy* of
that `shared_ptr` to callers. The copy shares the same control
block, so when the inner `Member` is reset (or the outer object is
destroyed), the refcount is unified.

If you don't want to store the inner `shared_ptr` on `Outer` (maybe
the inner object is constructed lazily), the aliasing ctor lets you
tie the inner's `shared_ptr` lifetime to the outer's. The trace in
section 7 shows this: `session.reset()` drops `session.use_count()`
to 0 (and destroys the Session), and the aliasing `tracked_via_session`
is now dangling even though the underlying `tracked` is still alive.

The reason the aliasing ctor is in this file: it pairs naturally with
`enable_shared_from_this`. The typical pattern is:

```cpp
class Outer : public std::enable_shared_from_this<Outer> {
    // ...
    shared_ptr<Inner> get_inner() {
        if (!inner_) {
            inner_ = make_shared<Inner>(...);
        }
        return inner_;  // shares the inner_'s own control block
    }
    // Or, if you want the inner's lifetime tied to the outer's:
    shared_ptr<Inner> get_inner_aliased() {
        if (!raw_inner_) {
            raw_inner_ = new Inner(...);
        }
        return shared_ptr<Inner>(shared_from_this(), raw_inner_);
    }
private:
    shared_ptr<Inner> inner_;
    Inner* raw_inner_ = nullptr;
};
```

The second form is what section 7 demonstrates. The first form
(simple shared `shared_ptr<Inner>` member) is what I usually use
in real code — simpler, no aliasing needed.

### Section 8: the bug, in a forked child

Section 8 is the *reason* the rest of the file exists. It runs the
buggy pattern in a forked child process so the crash is contained:

```cpp
class NaiveSession {
    // ...
    std::shared_ptr<NaiveSession> get_self_buggy() {
        return std::shared_ptr<NaiveSession>(this);
    }
};
```

The trace under the default (non-ASan) build:

```
========== 8. The bug that shared_from_this() prevents (in a child) ==========
  About to fork(). The child will run the buggy pattern; the parent waits.
  child terminated by signal 5 (SIGTRAP — libc++ debug-mode heap detected the double-free)
```

Apple's libc++ ships a debug-mode heap that detects double-frees and
traps with SIGTRAP. Other allocators (glibc, jemalloc) may silently
double-free, in which case the child exits "normally" and you might
not notice the bug for years. The child process model lets us
demonstrate the bug in a way that:

- Doesn't kill the parent (so the rest of the program continues).
- Reports the child exit status (signal vs. normal exit) so we
  can see *whether* the allocator caught the double-free.

Under ASan, the child doesn't SIGTRAP — it fails with a different
diagnostic:

```
==1988==ERROR: AddressSanitizer: attempting free on address which was not malloc()-ed
    #0 _ZdlPv (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x50358)
    #1 std::__1::default_delete<NaiveSession>::operator()(NaiveSession*) const
    #2 std::__1::__shared_ptr_pointer<...>::__on_zero_shared()
    ...
SUMMARY: AddressSanitizer: bad-free
```

ASan caught the second free as a "bad-free" — the second `delete`
on an address that was already freed by the first control block.
This is a different manifestation of the same underlying bug
(two control blocks, two deletes) but with a different detector.

The takeaway: **never write `return std::shared_ptr<T>(this);`**.
Always derive from `enable_shared_from_this<T>` and return
`shared_from_this()`. The first version compiles, runs, and
crashes unpredictably. The second is the standard library's
blessed way to give out `shared_ptr`s to `this`.

## Sample output highlights

```
========== 1. enable_shared_from_this<T> ==========
  [Session] ctor  name='alpha'
  sp1.use_count() = 1
  sp1.use_count() = 2  (after sp2 = sp1->get_self())
  sp2.use_count() = 2  <- shares the same control block
  sp1.get() == sp2.get() = true
  [Session] dtor  name='alpha'
```

`sp1` and `sp2` see the same refcount and the same raw pointer. One
`dtor` total when both expire. That's the contract.

```
========== 3. weak_from_this() — non-owning observer ==========
  [Session] ctor  name='gamma'
  sp.use_count()       = 1  (owning)
  weak.use_count()      = 1  <- non-owning
  weak.expired()        = false
  weak.lock()           = 'gamma'  (succeeded)
  [Session] dtor  name='gamma'
  sp.reset() — weak.expired() = true
  weak.lock() returned nullptr  (object is gone)
```

`weak.use_count()` is the *strong* refcount (which is 1, the owner),
not the weak refcount. `weak.expired()` is true after `sp.reset()`
because the strong refcount hit zero. `weak.lock()` returns a
`shared_ptr` while the object is alive and `nullptr` after.

```
========== 4. Self-registration with a Registry ==========
  [Server] ctor  name='web-01'
  [Server] ctor  name='web-02'
  [Server] ctor  name='db-01'
  registry.size() = 3  (after 3 make_server calls)
  registry contents:
    - db-01
    - web-02
    - web-01
  [Server] dtor  name='db-01'
  [Server] dtor  name='web-02'
  [Server] dtor  name='web-01'
```

Three `make_server` calls produce three `Server` objects, each held
by the local `auto a/b/c` and by the registry. The locals go out
of scope, but the registry holds its three refs; the dtor traces
fire when the registry itself goes out of scope at the end of
section 4. The ordering is alphabetical (because `unordered_set`
on macOS uses a hash that produces a stable order across the
demo's lifetime).

```
========== 5. shared_from_this() requires shared_ptr ownership ==========
  [BadCall] ctor
  about to call raw.get_self() on a stack-allocated object...
  caught std::bad_weak_ptr: bad_weak_ptr
  [BadCall] dtor
```

The exception is caught cleanly. The object is destroyed normally
when the stack unwinds.

```
========== 8. The bug that shared_from_this() prevents (in a child) ==========
  About to fork(). The child will run the buggy pattern; the parent waits.
  child terminated by signal 5 (SIGTRAP — libc++ debug-mode heap detected the double-free)
```

The crash is contained to the child. The parent reports the signal
back and continues. This is the right way to demonstrate a bug
that would otherwise take down the demo.

## Bugs and gotchas I hit

1. **LSP complained about `weak_from_this` and `shared_from_this`.**
   The clangd LSP was using an older `<memory>` definition and
   couldn't find the symbols. `g++ -std=c++17` built it fine; the
   real fix is to use a real compiler, not an indexer. Same lesson
   as Jun 12 — LSP diagnostics are guidance, not verdict.

2. **`BadCall` and `Tracked` initially didn't derive from
   `enable_shared_from_this`**, so `shared_from_this()` was
   undeclared. Compile error was clear, but my mental model was
   "I want to show the *bug* in section 5, so the class doesn't
   derive from esft" — but section 5's bug isn't "esft is missing",
   it's "the object isn't owned by a shared_ptr". The class *does*
   need to derive from esft, but the *caller* needs to misuse it
   by constructing it on the stack. Fix was to make both classes
   derive from `enable_shared_from_this<>`.

3. **Initial `Server` forward declaration made `Registry::list_names`
   not compile** because `Server` was incomplete. The fix was to
   forward-declare `Registry` with member-function declarations
   only, and define the bodies inline *after* `Server` is complete.
   That's the standard C++ idiom for breaking the cyclic dependency:
   "definition order is independent of declaration order, but the
   bodies need the complete type."

4. **The first run crashed with SIGTRAP at section 1**, not
   section 8. That's because I'd put the buggy `get_self_buggy()`
   call in section 1 (`NaiveSession`), and the second `delete` of
   the NaiveSession hit during the section 1 scope exit. The fix
   was to move all buggy code to section 8 and run it in a child
   process. The point of the demo is to *explain* the bug; crashing
   the demo program before sections 2-7 ever ran is bad pedagogy.

5. **ASan reports "bad-free" instead of "double-free"** for the
   section 8 buggy pattern. This is because the first `delete` already
   freed the address, so the second `delete` is trying to free an
   address that's not on the live-allocation list — "bad-free" rather
   than "double-free". The underlying bug is the same; the detector's
   vocabulary differs.

## Verification

- `g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g`: **clean** (1 unused-parameter
  warning, fixed with `(void)reg;` in the Server ctor)
- `g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -fsanitize=address`:
  **sections 1-7 clean, section 8 child triggers ASan's "bad-free" detector**
  (the bug is intentional; the parent ASan run still completes with
  exit code 0)
- Default (non-ASan) build: **sections 1-7 print what they claim, section 8
  child crashes with SIGTRAP under Apple's debug-mode libc++ heap**;
  parent reports the child exit status and continues.
- Program terminates normally with exit status 0. All 8 sections print
  their section header.

## Key takeaways

- **`enable_shared_from_this<T>` is the standard library's bridge
  between "I have `this`" and "I want a `shared_ptr` to it".**
  Derive from it, and `shared_from_this()` returns a `shared_ptr<T>`
  that shares the control block the original `shared_ptr` installed.

- **The bug it prevents is real and silent.** `shared_ptr<T>(this)`
  compiles, runs, and double-frees. The error message (or the
  absence of one) depends on the allocator: libc++ debug heap traps
  with SIGTRAP, ASan reports "bad-free", glibc may silently corrupt
  the heap. The fix is one line: derive from `enable_shared_from_this`
  and return `shared_from_this()`.

- **`weak_from_this()` is the C++17 way to break reference cycles.**
  Children can hold a `weak_ptr<parent>` and `lock()` it when they
  need a strong reference. The parent can expire; the child can
  detect that with `weak_ptr::expired()`.

- **The factory pattern is mandatory.** A class that derives from
  `enable_shared_from_this` should *only* be constructible via a
  factory function that uses `make_shared`. If a caller can bypass
  the factory and stack-allocate the object, `shared_from_this()`
  will throw `std::bad_weak_ptr` on the first method call.

- **The aliasing ctor pairs naturally with esft** for cases where
  you want a member object's `shared_ptr` to have its lifetime
  tied to the outer object's control block (e.g. for caching or
  lazy construction). The simpler form — store a `shared_ptr<Member>`
  on the outer and return a copy — usually suffices.

- **Self-registration is the canonical use case.** A class that
  needs to register itself with a central registry (a connection
  pool, a name service, an event bus) uses `shared_from_this()` in
  a post-construction `register_with()` method called by the
  factory. The factory uses `make_shared` to install the control
  block, then calls `register_with` (which now has a working
  `shared_from_this()`), then returns the `shared_ptr` to the
  caller. All three parties share the same control block.

## Next Steps

- **`std::span` (C++20)** — non-owning view over a contiguous
  range. Pair, struct, array. The "C++17 design by hand" is a
  template class that holds a pointer and a size, with the
  same accessors. Worth building before reaching for C++20.
  (Still on the list from Jun 9, Jun 11, and Jun 12.)

- **Recursive `std::variant` in `std::unique_ptr`** — the AST
  pattern `std::variant<Leaf, std::unique_ptr<Node>>` is smaller
  and faster than the `std::shared_ptr` version in the Jun 10
  expression evaluator. Worth rebuilding the evaluator with
  `unique_ptr` to see the difference. (Still on the list from
  Jun 11 and Jun 12.)

- **Build the multi-file `Inventory` as separate compilation**
  (`.h` / `.cpp` / `Makefile`) — the Jun 9 Inventory is one
  big file; splitting it shows how moves work at the link
  boundary. (Still on the list from Jun 9, Jun 11, and Jun 12.)

- **Tagged union vs. inheritance for ASTs** — when is the
  variant approach clearer than `virtual Expr::accept(...)`?
  Open-closed on alternatives vs. open-closed on operations.
  (Still on the list from Jun 11 and Jun 12.)

- **A "real" error type with `std::error_code` integration** —
  the `ParseError` in Jun 11 is a POD struct; the production
  version uses `std::error_code` for OS-level errors and a
  custom category for domain errors. C++23's `std::expected`
  + `std::error_code` is a strong idiom for low-level
  libraries. (Still on the list from Jun 11 and Jun 12.)

- **Co-routines (C++20)** — the next big abstraction layer.
  Worth reading about after `std::span` and the recursive
  variant, because the patterns there are closer to the
  co-routine design space. (New on the list, worth scheduling.)

- **`std::visit` with stateful visitors** — a pretty-printer
  for `std::variant<A, B, C>` that writes the active
  alternative's fields to an `ostream`. Builds on the Jun 10
  variant/visit session. (Still on the list from Jun 11
  and Jun 12.)

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `cpp_practice/P-2026-06-13-enable-shared-from-this.cpp`
  and this notes file (`.md`).
- **P- prefix per the new convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.
  `enable_shared_from_this` is C++11; `weak_from_this()` is C++17.

---

*`enable_shared_from_this` is a small class with a big job: it
turns a method's `this` pointer into a properly-ownership-aware
`shared_ptr`. The mechanism is a CRTP base that holds a weak
pointer to the derived object, installed by the `shared_ptr`
machinery when the first `shared_ptr` is constructed. After that,
every `shared_from_this()` call promotes that weak pointer,
producing a `shared_ptr` that shares the control block — and
therefore the lifetime — of the original. The bug it prevents
is silent and allocator-dependent: `shared_ptr<T>(this)` compiles,
runs, and double-frees. The fix is one line: derive from
`enable_shared_from_this` and return `shared_from_this()`. The
factory pattern (with `make_shared`) is mandatory, because the
object must be born already-managed by a `shared_ptr` for
`shared_from_this()` to work. The result is a class that can
hand out references to itself safely, even across threads and
even if the original owner goes away first.*
