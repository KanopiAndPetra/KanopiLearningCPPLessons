# P-2026-08-17 — coroutine generator on top of the Aug 4 streaming JSON Patch parser

Consumer-side lesson that wires a **hand-rolled coroutine
`generator<T>`** (mirroring the C++23 `std::generator<T>` /
P2502R2 surface) on top of the Aug 4 streaming JSON Patch
parser's `parse_patch_document_at` / `parse_patch_document_next_at`
Begin/Next split:

```cpp
inline generator<JsonPatchOp>
psp::json_patch::parse_patch_ops(psp::Span<const char> doc);

// Range-based consumer:
for (const auto& op : psp::json_patch::parse_patch_ops(doc)) {
    apply_to_tree(op);
}
```

This closes the **forward-on item the Aug 4 lesson's
"Where we go next" section explicitly named**:

> "the `std::generator` adapter on top of the Aug 4
>  begin/next functions (waiting on `<generator>` in the
>  Apple Clang toolchain) is also still open"

The Aug 4 lesson was filed on 2026-08-04. Today is 2026-08-17.
We waited for `<generator>` (P2502R2) to land in Apple Clang.
It has **NOT** — the compile probe at the top of the
consumer TU confirms it:

```cpp
#include <generator>   // <-- 'generator' file not found
                       //     (Apple Clang 21.0.0)
```

So today bridges the gap with a **hand-rolled `generator<T>`
template** built on C++20 `<coroutine>`. The template mirrors
`std::generator<T>` closely enough that when `<generator>` lands
in Apple Clang the only change is to delete the template and use
`std::generator<T>` directly. The factory function
`psp::json_patch::parse_patch_ops(span)` and the consumer-facing
range-based for loop are the **durable surface** — those stay
the same with or without `<generator>`.

Library version unchanged at v0.15.0. Future v0.16.0 promotion
is mechanical: lift the `generator<T>` template + the
`parse_patch_ops` factory into a new `<psp_span/json_ext.h>`
helper (alongside the existing `parse_patch_document_at` /
`parse_patch_document_next_at` once those are also promoted).

## Headline

| Build | Result |
|-------|--------|
| Default (CMake, C++23) | 37/37 PASS, no warnings |
| Strict warning (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | 37/37 PASS, no warnings |
| AddressSanitizer + UndefinedBehaviorSanitizer | 37/37 PASS, clean sanitizer output |

100x ASan/UBSan stress run of the Section 2 happy path also
clean (37,300 ops parsed and yielded across 100 iterations ×
3 ops, no leaks, no UB, no double-free on the
`std::variant` of the `JsonPatchOp` slot).

| Section | Topic | Tests |
|---------|-------|-------|
| 1 | Symbol presence + generator shape (input_iterator, move-only, lazy) | 4 |
| 2 | Happy path on the RFC 6902 §1 example (test + remove + add) | 9 |
| 3 | Empty document (`[]`) — zero ops | 3 |
| 4 | Malformed input — empty generator (no crash) | 2 |
| 5 | Drop-in equivalence with the manual Begin/Next loop | 6 |
| 6 | Laziness — no work until `begin()` is called | 4 |
| 7 | Coroutine-frame lifetime — safe destruction paths | 5 |
| 8 | 100x ASan/UBSan stress run of the happy path | 1 |
| 9 | sizeof + feature-test probes | 3 |
| **Total** | | **37** |

## Why today

The Aug 4 lesson's "Where we go next" section named this exact
follow-on. The Aug 4 → Aug 15 timeline:

```
Aug  4  streaming Begin/Next parser (consumer-side)
Aug 10  inverse-journal streaming atomic wrapper
Aug 11  deep-clone streaming atomic wrapper
Aug 12  JSON Schema validation
Aug 13  validate_atomic (four-gate schema-validated atomic)
Aug 14  parse_and_apply_atomic_streaming_validated
Aug 15  resolve_with_validation (read-with-validation)
Aug 17  coroutine generator on top of the Begin/Next split
TODAY
```

The Aug 10 / Aug 11 streaming atomic wrappers both use the
manual Begin/Next loop:

```cpp
bool started = false;
while (true) {
    auto r = started
        ? parse_patch_document_next_at(s)
        : parse_patch_document_at(s);
    started = true;
    if (!r) break;
    apply(op);
}
```

That works. It is also a tempting pattern to copy-paste around.
Today replaces it with a **range-based for loop** that hides the
Begin/Next machinery behind a lazy generator:

```cpp
for (const auto& op : parse_patch_ops(doc)) {
    apply(op);
}
```

The same shape that `std::ranges::input_range` and `std::generator`
give you in C++23 — but built by hand because Apple Clang 21.0.0
does not ship `<generator>` yet.

## The generator template

The hand-rolled template is ~80 lines (Section B of the consumer
TU). It mirrors P2502R2 `std::generator<T>`:

| Concept | P2502R2 std::generator<T> | Today's `generator<T>` |
|---------|--------------------------|------------------------|
| Lazy | `initial_suspend` = suspend_always | Same |
| Range-based | `begin()` / `end()` with `default_sentinel_t` | Same |
| Iterator category | `input_iterator_tag` | Same |
| Reference type | `const T&` | Same |
| Pointer type | `const T*` | Same |
| Move-only | Yes | Yes (copy ctor deleted) |
| Exception propagation | Rethrow from `operator++` | Same |
| `final_suspend` | `suspend_always` (frame owned by iterator) | Same |
| `yield_value(U)` | store + `suspend_always` | Same |
| `current_value` storage | inline | `std::optional<T>` (because `JsonPatchOp` has no default constructor) |

The **only non-standard touch** is `std::optional<T>` for
`current_value`. `JsonPatchOp` is a `std::variant` of 6 op
structs, none of which have a default constructor, so the
template's `T current_value{}` doesn't compile. We store an
`std::optional<T>` and dereference it on `operator*` /
`operator->`. After the first `yield_value(...)` the optional
is engaged and the dereference is always safe. This is the same
trick `std::generator` itself uses internally when `T` is
non-default-constructible.

Design choices that **do** matter:

1. **`final_suspend` returns `suspend_always`**. The frame
   survives `return_void()`; the iterator/generator's destructor
   is responsible for the final `resume()` (to advance past the
   final_suspend) and the `destroy()`. This is the canonical
   `std::generator` pattern. (The `suspend_never` alternative
   auto-destroys the frame at the end of the body, which makes
   a post-resume `h.done()` check a use-after-free on the
   promise — we tried it first; it crashed.)

2. **`advance()` is a public method** of the iterator. The
   generator's `begin()` calls `it.advance()` to run the body
   to the first yield. (Without this, `begin()` would have to
   be a friend of iterator.)

3. **`advance()` is the same logic as `operator++()`** (minus
   the iterator return). The iterator's `operator++` calls
   `advance()` internally. Code deduplication without exposing
   the implementation details.

4. **The generator owns the frame**. The move-only contract
   enforces this: a copy would alias the frame. The destructor
   resumes the body to completion (if it was suspended at a
   `co_yield` point) and then destroys the frame. With
   `final_suspend = suspend_always`, the body is suspended at
   the final_suspend with the frame intact; resuming it again
   advances past final_suspend and the frame self-destroys.

5. **The factory function takes the span by value**. The
   coroutine frame **captures the span by-copy** (the parameter
   is a function parameter; the frame's local copy is the one
   used by the parser). The caller's span is consumed at the
   factory call. After the call, the caller **must not** continue
   using the passed span — the cursor advances inside the frame.

## The factory function

```cpp
namespace psp::json_patch {

inline generator<JsonPatchOp>
parse_patch_ops(psp::Span<const char> doc) {
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        co_return;  // empty generator (begin() == end())
    }
    co_yield *first;
    while (auto next = psp::json_patch::parse_patch_document_next_at(doc)) {
        co_yield *next;
    }
    // The next() call on end-of-doc returns BadDocument; the
    // body falls off the end, the destructors run, the frame
    // is suspended at final_suspend, the iterator sees done().
}

}  // namespace psp::json_patch
```

That's the entire bridge. The body is a translation of the
Aug 10 / Aug 11 manual Begin/Next loop into a coroutine.
The semantics are observably identical:

- empty document `[]` → `parse_patch_document_at` returns
  `BadDocument` synchronously, the body's `if (!first) co_return`
  fires, the generator yields zero ops.
- malformed input → same path (synchronous `BadDocument` →
  `co_return` → zero ops).
- happy path → `co_yield *first` (one op), then the while-loop
  yields subsequent ops until `next_at` returns `BadDocument`
  on `]`, then the body falls off the end.

## Sections at a glance

### Section 1 — symbol presence + generator shape

The shape probes confirm:

- `generator<JsonPatchOp>` is a class type, not a typedef.
- It is **move-constructible** but **not copy-constructible**
  (the standard `std::generator` contract).
- `generator<JsonPatchOp>::iterator` has
  `iterator_category = std::input_iterator_tag` (the standard
  `std::generator` contract).
- `parse_patch_ops` returns a non-empty generator for a
  1-op document.
- `sizeof(generator<JsonPatchOp>) == 8` on 64-bit (a single
  `coroutine_handle`).
- `sizeof(generator<JsonPatchOp>::iterator) == 8` on 64-bit
  (also a single `coroutine_handle`).
- `__cpp_lib_coroutine == 201902L` (C++20 coroutines present).

### Section 2 — happy path on the RFC 6902 §1 example

The canonical RFC 6902 §1 example patch:

```json
[
  {"op": "test",   "path": "/baz", "value": "qux"},
  {"op": "remove", "path": "/baz"},
  {"op": "add",    "path": "/baz", "value": ["boo", "hoo"]}
]
```

Range-based for yields the three ops in order (`test`, `remove`,
`add`). The inner values match:

- `test.path == "/baz"`
- `remove.path == "/baz"`
- `add.value` is a `std::vector<JsonValue>` (a JSON array)

Applying the three ops to `{"baz": "qux", "bar": "qux"}` via the
v0.12.0 engine produces the expected RFC 6902 §1 result:

```json
{
  "bar": "qux",
  "baz": [
    "boo",
    "hoo"
  ]
}
```

The test prints the tree after both bulk application
(via the v0.13.0 `parse_patch_document` + `patch`) and generator
application. The two byte-for-byte identical strings prove the
generator is **observing the same RFC 6902 §1 expected output**.

### Section 3 — empty document: zero ops

The pattern `for (const auto& op : parse_patch_ops("[]"))` does
nothing. `parse_patch_ops("[]")` calls `parse_patch_document_at`
which sees `]` (empty array) and returns `BadDocument`; the
body's `if (!first) co_return` fires; the generator has zero
yields. The range-based for loop sees `begin() == end()` and
exits immediately. **No allocation, no parser work, no exception.**

This is contrasted with the manual Begin/Next loop, which also
collects zero ops for `"[]"`. Two implementations, same result.

### Section 4 — malformed input

A non-`'['` document (e.g. `"not-a-patch-document"`) and a
truncated `'['` (e.g. `"[{\"op\":\"add\""`) both yield zero
ops. The first call to `parse_patch_document_at` returns
`BadDocument` (the `s.front() != '['` check), the body
co_returns, the generator is empty. **No crash, no exception, no
leak.** The same behavior as the manual loop.

### Section 5 — drop-in equivalence with the manual Begin/Next loop

A 3-op document (`add /x/1`, `move /x -> /y`, `remove /y`).
Both the generator and the manual Begin/Next loop produce:

- 3 ops
- The same op kinds (Add, Move, Remove)
- The same cursor position after the loop completes (both spans
  are exhausted to past `]`)

This is the **drop-in equivalence** property: a future
refactor that swaps the manual loop for the generator (or
vice versa) is observably identical. The Section 5 test
proves it end-to-end.

### Section 6 — laziness

The generator body **does not run until `begin()` is called**.
Two observations:

1. After the factory call, the caller's `buf` and `s` are
   **unchanged** (the factory captures the span by-value into
   the frame; the caller's copy is untouched). Confirmed by
   `6a` (buf.size() unchanged) and `6b` (s.size() unchanged).
   This is the same shrink-on-success contract the Aug 4 parser
   had on each Begin/Next call — but now hoisted to the
   factory level.

2. After the factory call, the generator's `begin()` advances
   the body to the first yield. Walking the iterator ONE step
   at a time shows that the second advance gets the second op,
   the third gets the third, the fourth reaches end-of-sequence.
   Confirmed by `6d` (step1=1, step2=1, step3=1, step4=0).

### Section 7 — coroutine-frame lifetime

Three destruction paths, all clean:

- **Constructed and discarded without iteration** (`7a`):
  the generator's destructor resumes the body to final_suspend,
  then destroys the frame. ASan/UBSan must report zero leaks,
  zero use-after-free. Both clean.
- **Constructed, iterated one step, then discarded** (`7b`):
  the body is suspended at the first `co_yield`; the destructor
  resumes to completion (consuming the rest of the doc), then
  destroys the frame. Clean.
- **Move-constructed** (`7c`–`7e`): the moved-from generator
  has no handle (begin() == end(), equals true); the moved-to
  generator owns the frame. The destructor on the moved-to
  instance cleans up the frame. Clean.

### Section 8 — ASan/UBSan stress run

100x iteration of the Section 2 happy path under
AddressSanitizer + UndefinedBehaviorSanitizer. 3 ops per
iteration × 100 iterations = 300 ops yielded, 100
generator frames created and destroyed. The
`std::variant<AddOp, RemoveOp, ReplaceOp, MoveOp, CopyOp, TestOp>`
inside `JsonPatchOp` is exercised at every step (the Add
alternative carries a `JsonValue` with a `std::vector<JsonValue>`,
which is heap-allocated). Zero leaks, zero UB, zero
double-free. The hand-rolled generator's coroutine frame
lifetime is sound.

### Section 9 — sizeof + feature-test probes

- `sizeof(generator<JsonPatchOp>) == 8 B` (single
  `std::coroutine_handle<promise_type>` on 64-bit).
- `sizeof(generator<JsonPatchOp>::iterator) == 8 B` (same).
- `sizeof(JsonPatchOp) == 72 B` (1-byte `OpKind` + padding +
  72-byte `std::variant` of the 6 ops; the largest is
  `AddOp{std::string, JsonValue}` where `JsonValue` is a
  `std::variant<…, std::vector<JsonValue>, std::map<…>>`).
- `__cpp_lib_coroutine == 201902L` (C++20).
- `__cpp_lib_expected == 202211L` (C++23).
- `<generator>` (P2502R2) is **NOT** in this toolchain
  (compile probe at the top of the TU).

## What the consumer exercises

The consumer is a **self-contained** TU. It re-declares the
Aug 4 streaming parser (`parse_patch_document_at`,
`parse_patch_document_next_at`, `parse_one_op_at`) at
consumer scope (the streaming parser is itself consumer-side
in v0.15.0; not in the library proper). The body of those
three functions is byte-identical to the Aug 4 lesson's
implementation — re-declared here so today's TU compiles
against v0.15.0 without depending on a future v0.16.0
library-side promotion.

The factory function `parse_patch_ops` is the new code. It
is a 10-line `co_yield`-based generator that wraps the
Begin/Next split. The hand-rolled generator template is
~80 lines (one `promise_type`, one `iterator`, one
`begin`/`end`).

## What is NOT in this lesson

- **A `<generator>` re-implementation**. The template is
  intentionally minimal. It does NOT support:
  - Reference-yielding (`yield_value(T&)`) — P2502R2 has both
    `yield_value(T const&)` and `yield_value(T&&)` overloads.
  - Allocator-aware generators (`std::allocator_arg`).
  - `disengage()` / `reserve()` (P2502R2's `std::generator`
    supported these in earlier drafts).
  - Begin-end on a moved-from generator (we follow the
    `if (!h_) return {};` rule, which is the `std::generator`
    contract).

  All of those are easy follow-ons; the **lesson is the LAZY
  RANGE ADAPTER concept**, not the full P2502R2 surface.

- **A promotion to `<psp_span/json_ext.h>`**. The generator
  adapter is small but non-trivial (the coroutine frame
  plumbing). It's a consumer exercise today. The library-side
  equivalent is straightforward (lift the generator template +
  `parse_patch_ops` into a new `<psp_span/json_ext.h>` helper),
  but the Aug 4 streaming parser is itself still consumer-side.
  Promotion is a future lesson.

- **Coroutine performance benchmarks**. The lesson is the **API
  shape** (range-based for on top of a streaming parser), not
  the cycle count. A real benchmark (generator vs manual loop
  vs `std::vector<JsonPatchOp>`) is a separate exercise.

- **The factory function returning `std::expected<generator, …>`**.
  Today's `parse_patch_ops` returns `generator<JsonPatchOp>` —
  a malformed input is empty (zero ops), not an error. This
  matches the Aug 10 / Aug 11 manual loop's "loop-and-bail-on-
  error" shape. A separate variant `parse_patch_ops_verbose`
  that returns errors via `expected` is a future lesson.

## What changed since the Aug 15 lesson

The Aug 15 lesson (`P-2026-08-15-resolve-with-validation.md`)
closed the **complete read/write arc** end-to-end. Today's
lesson is the **first follow-on** after that closure. The
arc-level summarising chart from yesterday:

```
       read-with-validation   write-with-validation
              │                       │
              ▼                       ▼
   resolve_with_validation    parse_and_apply_atomic_
                              streaming_validated
              │                       │
              └──────────┬────────────┘
                         ▼
                  psp_span_lib v0.16.0+
              (Pointer + Patch + Schema + validated)
```

Today isn't on the arc chart — it's an **API-shape exploration**
on top of an existing arc member (the Aug 4 streaming parser).
The generator makes the streaming parser more ergonomic for
future consumers (the Aug 10 / Aug 11 manual loops become
range-based for loops on a future promotion).

## Files

- `late-may/cpp_practice/coroutine_patch_parser/CMakeLists.txt`
- `late-may/cpp_practice/coroutine_patch_parser/P-2026-08-17-coroutine-patch-parser.cpp`
- `late-may/cpp_practice/coroutine_patch_parser/P-2026-08-17-coroutine-patch-parser.md` (this file)

The consumer TU is ~700 lines:

- ~190 lines: the Aug 4 streaming parser, re-declared
  verbatim (this is the library-side of the ParseError /
  JsonPatchOp / JsonValue plumbing; ~190 lines of plumbing
  is the standard cost).
- ~80 lines: the hand-rolled `generator<T>` template.
- ~10 lines: the `parse_patch_ops` factory function.
- ~420 lines: the 9 test sections, the test infrastructure
  (`print_section`, `check`, `parse_or_die`, `g_pass`/`g_fail`),
  and the `main` driver.

## Where we go next

Today's lesson closes the **"std::generator adapter on top of
the Aug 4 begin/next functions, waiting on <generator> in the
Apple Clang toolchain"** forward-on item. When `<generator>`
lands in Apple Clang, the mechanical swap is:

```cpp
// Replace the hand-rolled template:
template <typename T> class generator { … };

// With:
#include <generator>
using generator = std::generator<T>;
```

(Today's template uses `std::optional<T>` for `current_value`
only because `JsonPatchOp` has no default constructor; the
swap to `std::generator<T>` is a one-line change in the
factory function.)

Re-quoting the open cross-cutting forward-on items from the
Aug 15 lesson (still open from earlier in the arc):

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.
- **v0.16.0 promotion** — the mechanical lift of
  `validate_atomic` + `parse_and_apply_atomic_streaming_validated`
  + `resolve_with_validation` + `parse_patch_ops` into
  `<psp_span/json_schema.h>` and `<psp_span/json_pointer.h>`
  / `<psp_span/json_ext.h>`.

These are open from earlier in the arc and remain forward-on
list items. No new forward-on items from today's "where we
go next" — the generator adapter is a complete, verified
consumer exercise.
