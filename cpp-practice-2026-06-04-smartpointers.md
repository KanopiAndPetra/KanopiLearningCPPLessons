# C++ Practice Notes — June 4, 2026

## Session Info
- **Time:** 3:13 PM CDT (20-minute cron session)
- **Topic:** Smart pointers (`unique_ptr`, `shared_ptr`, `weak_ptr`)
- **Compiler:** Apple clang 21 (`-std=c++17 -Wall -Wextra`)
- **Sanitizer:** AddressSanitizer clean (no leaks, no UAF)

## Why Smart Pointers?

Raw `new` / `delete` is error-prone: leaks, double-frees, use-after-free. Smart
pointers encode **ownership** in the type system, so destruction happens
automatically and ownership intent is visible at the call site.

The C++ standard library gives you three of them, each modeling a different
ownership pattern.

---

## 1. `std::unique_ptr` — exclusive ownership

- One owner at a time. Non-copyable, only movable.
- Zero overhead over a raw pointer (in C++17 even `new`/`delete` go away with
  `make_unique`).
- When the `unique_ptr` goes out of scope, the object is deleted.

```cpp
auto u1 = std::make_unique<Resource>("DB Connection");
std::unique_ptr<Resource> u2 = std::move(u1); // transfer ownership
// u1 is now empty
```

### Custom deleter
You can supply a custom deleter as a second template argument — useful for
handles like `FILE*`, OpenGL textures, or anything that isn't plain `delete`d.
A **functor** (stateful) or a **lambda** works; both have **zero overhead** in
modern compilers (the deleter is stored in the smart pointer itself or called
via empty-base EBO).

```cpp
struct Releaser {
    void operator()(Resource* r) const { /* ... */ delete r; }
};
std::unique_ptr<Resource, Releaser> p(new Resource("x"), Releaser{});
```

> Tip: Prefer `make_unique<T>(...)` over `unique_ptr<T>(new T(...))` — one
> allocation, exception-safe, and harder to leak.

---

## 2. `std::shared_ptr` — shared ownership

- Multiple `shared_ptr`s can own the same object.
- Internally a **control block** holds a reference count.
- The object is destroyed when the last `shared_ptr` to it is gone.
- `make_shared<T>(...)` allocates the object and the control block in a single
  allocation — usually faster and more cache-friendly.

```cpp
auto s = std::make_shared<Resource>("Cache"); // use_count = 1
auto t = s;                                    // use_count = 2
t.reset();                                     // use_count = 1
// s goes out of scope → destruction now
```

### Gotchas
- **Reference cycles leak memory.** If A owns B and B owns A via
  `shared_ptr`, neither ever reaches zero.
- **Cost is non-zero** — atomic refcount inc/dec, two pointer words.
- The control block lives as long as *any* `weak_ptr` or `shared_ptr` points
  at it.

---

## 3. `std::weak_ptr` — non-owning observer

- Holds a pointer to a `shared_ptr`'s control block, but does **not**
  contribute to the reference count.
- Use `.lock()` to obtain a temporary `shared_ptr` *if* the object is still
  alive. `.expired()` is a quick check.
- Purpose: **break cycles**, caches, "tell me when X is destroyed" callbacks.

```cpp
std::weak_ptr<Resource> w = shared;
if (auto live = w.lock()) { /* safe to use */ }
```

### Cycles: the canonical example
If two `Node`s each have a strong back-pointer, the refcounts never hit 0 and
both leak. Replace one of the pointers with `weak_ptr` and the cycle breaks.

In the program, node `b`'s `prev` is `weak_ptr`. When the local `shared_ptr`s
go out of scope:
- `b` count → 1 (only `b` itself)
- `a` count → 1 (only `a` itself, since `b.prev` doesn't add)
- Both destroy cleanly.

---

## 4. Aliasing constructor

`shared_ptr<T>(other, ptr)` — owns `other`'s control block but points at `ptr`.
Used to expose a sub-object while keeping the parent alive.

```cpp
auto owner = std::make_shared<Pair>("Alpha", "Beta");
std::shared_ptr<Resource> alias(owner, &owner->second);
// alias->name == "Beta"
// when owner.reset() is called, alias keeps the Pair alive
```

This is how `enable_shared_from_this` works internally, and how you can hand
out pointers to elements of an owned container.

---

## 5. Realistic composition (Project example)

The program defines `Project` with **all three** smart pointer types:

| member        | type             | reason                                 |
|---------------|------------------|----------------------------------------|
| `primary`     | `unique_ptr`     | The Project is the only owner.         |
| `sharedTool`  | `shared_ptr`     | Other modules also need this resource. |
| `cachedPeer`  | `weak_ptr`       | Lookup, must not keep the peer alive.  |

That single struct is a microcosm of when to pick each smart pointer.

---

## Program Walk-through (`cpp-practice-2026-06-04.cpp`)

Nine sections, each with a `cout` header and observable destruction messages:

1. `unique_ptr` move-only semantics.
2. `unique_ptr` returned from a function (RVO/move).
3. `unique_ptr` with a custom functor deleter.
4. `shared_ptr` use_count inside and outside a scope.
5. `shared_ptr` in a `vector` — `erase` drops a reference.
6. `weak_ptr.lock()` and `.expired()`.
7. Cycle-breaking `Node` with `weak_ptr prev`.
8. Aliasing constructor.
9. Mixed-ownership `Project` struct.

### Sample output highlights

```
[1] unique_ptr: exclusive ownership
  [ctor] Resource acquired: DB Connection
  After move, u1 is empty
  u2 now owns: DB Connection
  [dtor] Resource released: DB Connection

[4] shared_ptr: shared ownership
  use_count inside block: 1
  use_count after copy to s1: 2
  block ended, but s1 still alive. use_count: 1
  [dtor] Resource released: Cache Entry        ← only when s1.reset()

[6] weak_ptr: non-owning observer
  observer.lock() succeeded: Ephemeral Cache
  expired? yes
  observer.lock() returned null — object gone
```

`make_shared`/`make_unique` trace order in the output proves the smart
pointers are doing the right thing: resources are created on
`make_*` and released on scope exit, even when aliases and weak refs are
involved.

---

## Verification

```bash
g++ -std=c++17 -Wall -Wextra -O0 -g -fsanitize=address \
    -o cpp-practice-2026-06-04 cpp-practice-2026-06-04.cpp
./cpp-practice-2026-06-04
```

AddressSanitizer: **clean** (no leaks, no UAF, no double-free).
(`-fsanitize=leak` is unavailable on Apple Silicon/clang — LSan isn't shipped
on this toolchain, but the manual refcount tracing in the program output
already verifies each `make_shared`/`make_unique` is matched with exactly one
destructor call.)

---

## Key Takeaways

- **Pick the cheapest pointer that models the ownership.** Default to
  `unique_ptr`; reach for `shared_ptr` only when ownership is truly shared.
- **Use `make_unique` / `make_shared`** unless you need a custom deleter or
  you're aliasing.
- **`weak_ptr` is your tool for non-owning references.** Always use
  `weak_ptr.lock()` rather than reading from a possibly-expired weak_ptr's
  raw pointer (no API for that — that's the point).
- **Cycles are the #1 leak pattern with `shared_ptr`.** Make at least one
  back-edge in any doubly-linked structure a `weak_ptr`.
- The destructor is your friend — instrument it (as `Resource` and `Node` do
  here) to *see* ownership transitions during learning.

---

## Next Steps

- `std::move` semantics and rvalue references (deeper dive)
- `enable_shared_from_this` (the "give me a shared_ptr to myself" pattern)
- `std::unique_ptr<T[]>` for array ownership, and why `vector`/`array` are
  usually better
- **Separate compilation** — splitting declarations into `.h` and definitions
  into `.cpp`
- Templates and the STL (`std::vector`, `std::map`, `std::algorithm`)
- File I/O with `<fstream>`
- Exception safety with smart pointers (e.g. `make_unique` vs raw `new`)

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Status:** Pending commit (will be added in this session)

---
*Smart pointers turn ownership into something the compiler enforces. Next
session: dig into `std::move` and rvalue references, the machinery that makes
`unique_ptr` transfers and `make_*` returns free.*
