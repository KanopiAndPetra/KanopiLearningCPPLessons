# C++ practice 2026-06-11 — `std::expected<T, E>` (C++23)

## What I set out to learn

`std::expected<T, E>` is C++23's *vocabulary type* for "this function might
return a value, or it might return an error." It's the official replacement
for the `std::variant<T, std::string>` pattern I used in the Jun 10
expression evaluator, and the rest of the Jun 10 next-steps list points
straight at it.

The difference between `std::expected<T, E>` and `std::variant<T, E>` is
small on paper and large in practice: `expected` *names* the success side.
You get `.value()`, `*e`, `if (e)`, `has_value()` — all the natural
success-side accessors — and the type system keeps them from being called
on the error side. With `variant<T, E>` you have to remember which
alternative is which, and use `std::get<index>` or `std::visit` to extract
it. `expected` removes that mental tax.

The file is `cpp_practice/P-2026-06-11-std-expected.cpp`. Path matches
the Jun 10 convention (`cpp_practice/`, P-prefix, P = Petra).

## Build and run

```bash
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
    -o P-2026-06-11-std-expected P-2026-06-11-std-expected.cpp
./P-2026-06-11-std-expected
```

> **Build flag note:** the cron workflow's standard is `-std=c++17`.
> `<expected>` is C++23, so this file uses `-std=c++23`. If you need to
> backport, see "C++17 fallback" below.

ASan build, also clean:
```bash
g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -fsanitize=address \
    -o P-2026-06-11-std-expected-asan P-2026-06-11-std-expected.cpp
./P-2026-06-11-std-expected-asan
```

## Sections at a glance

1. **Declaring `expected<T, E>`** — success, failure, default-constructibility
2. **Inspecting** — `has_value()`, `value()`, `operator*`, `operator->`
3. **The `error()` accessor** — `std::unexpected<E>` on the failure path
4. **`value_or()`** — default on the failure path
5. **Monadic interface** — `and_then`, `or_else`, `transform`, `transform_error`
6. **Comparison** — equality (libc++ 21 doesn't ship `<=>` for `expected` yet)
7. **Move semantics** — `expected` owns its T or E, moves transfer ownership
8. **`expected<void, E>`** — "either nothing or an error"
9. **Mini-example** — a parsing pipeline that returns `expected<int, ParseError>`
10. **`expected` vs. `variant<T, E>`** — when to reach for which

## Key ideas

### `expected` is a value-semantic union with a named success side

`std::expected<T, E>` is a discriminated union of two states:

- **Has-value state**: holds a `T`. The function succeeded.
- **Unexpected state**: holds an `E` (wrapped in `std::unexpected<E>`). The
  function failed, and here's the error.

The asymmetry vs. `std::variant<T, E>` is that the *type* knows which
side is the "good" one, and exposes the natural success accessors
(`.value()`, `operator*`, `operator bool()`) directly:

```cpp
std::expected<int, std::string> e = compute_something();
if (e) {
    use(*e);                // success path: just use the int
} else {
    log(e.error());         // failure path: read the error
}
```

No `std::get<int>(e)` (which throws on the wrong alternative) and no
`std::visit` boilerplate. That's the whole ergonomic story.

### `std::unexpected<E>` is a tag wrapper

To put an `expected` in the failure state, wrap your `E` in
`std::unexpected`:

```cpp
std::expected<int, std::string> e = std::unexpected("file not found");
```

`std::unexpected<E>` is a one-field template that exists for *one* reason:
to disambiguate the failure constructor of `expected` from the success
constructor. Without it, `expected<int, string> e = "oops";` would be
ambiguous (is `"oops"` a `const char*` convertible to `int`, or a
`std::string`?). The tag makes the call site read like a function:
"this is an unexpected outcome, here's the error."

### The monadic interface is where the power lives

`expected` ships with four "monadic" adapters that compose expected-returning
operations:

- `transform(fn)`: on success, apply `fn` to the value. `T` becomes `U`;
  the error type is unchanged. On failure, do nothing.
- `and_then(fn)`: on success, apply `fn` to the value, where `fn` returns
  `expected<U, E>` itself. Lets you chain operations that can each fail,
  with the same error type. On failure, do nothing.
- `transform_error(fn)`: on failure, apply `fn` to the error. The value
  type is unchanged. On success, do nothing.
- `or_else(fn)`: on failure, apply `fn` to the error, where `fn` returns
  `expected<T, E2>`. Lets you *recover* (return a success expected) or
  *rewrap* (return a different error type). On success, do nothing.

The shape mirrors `std::optional`'s `.and_then` / `.transform` and
Rust's `Result::and_then` / `Result::or_else`. Once you have these,
the parsing pipeline from section 9 collapses from explicit
`if (result) { ... } else { return error; }` ladders into a
single fluent chain:

```cpp
auto r = parse_int(input)
            .transform([](int v) { return v * 2; })
            .and_then([](int v) -> std::expected<int, ParseError> {
                if (v <= 0) return std::unexpected(ParseError{
                    "positive_check",
                    "value must be > 0 after doubling, got " + std::to_string(v), 0});
                return v;
            });
```

The `and_then` is necessary when the *next* step can also fail; `transform`
is the lighter version for pure `T -> U` functions that can't fail.

### `expected<void, E>` is "either nothing or an error"

A common case for "might fail" APIs is one where the success carries no
information: a side effect that either happened or didn't. `std::expected<void, E>`
is the vocabulary type for that:

```cpp
auto r = log_to("/var/log/app", "started");
if (!r) std::cerr << r.error() << "\n";
```

A default-constructed `expected<void, E>` is in the success state. The
canonical return-on-success is `return {};`.

### Default-constructibility rule

`std::expected<T, E>` is default-constructible iff `T` is. The default
state is success: a default-constructed `expected<int, std::string>`
holds a value-initialized `int` (i.e. 0). This is verified in section 1
with a `NotDefault` type whose constructor is deleted, and a `static_assert`
to prove `expected<NotDefault, std::string>` is not default-constructible.

The error type's default-constructibility is *irrelevant* — the error
side is only ever constructed when you write `std::unexpected(E{...})`.
So `expected<int, NotDefault>` *is* default-constructible, and the
`int` side has the value 0.

### Comparison: libc++ 21 only ships `==`, not the spaceship

The C++23 spec says `std::expected` should have an `operator<=>` that
returns a `partial_ordering` (the success/failure tags themselves
compare: success < failure, then T or E values compare). As of libc++
21 (what ships with Apple Clang 21.0.0 in this environment), only
`operator==` is implemented. Trying `e1 <=> e2` fails to compile.

The `==` semantics are still well-defined:

- Two success `expected`s are equal iff their `T`s are equal.
- Two failure `expected`s are equal iff their `E`s are equal.
- A success and a failure are *not* equal (commutatively).

Section 6 demonstrates this. The ordering rule is worth knowing even
if the implementation is lagging, because it shows up in algorithms
that rely on `std::ranges::sort` over expecteds.

### `expected` owns its T or E; moves transfer ownership

Like `variant`, `expected` is value-semantic: the T or E is held
inline, and assignment destroys the old state before constructing the
new one. The `Traced` trace in section 7 shows the mechanics:

- Constructing `expected<Traced, string> et{Traced{"src"}}` builds
  the `Traced` in the expected via a move. The temporary `Traced`
  is destroyed.
- `auto et2 = std::move(et)` moves the `Traced` into `et2`. `et` is
  still *in the success state* — it just holds a moved-from `Traced`.
  This is the same behavior as `std::variant`: the alternative doesn't
  change on move, but the alternative's contents are in a moved-from
  state.
- Reassigning `et = Traced{"second"}` move-constructs the new `Traced`
  and `move-assign`s it into the existing storage, then destroys the
  moved-from temporary. The old `Traced` (now overwritten in place)
  doesn't run a destructor separately — it gets reused.
- Reassigning `et = std::unexpected("whoops")` *destroys* the old `Traced`
  and constructs the new error string. This is the visible "switch to
  error" event in the trace.

ASan is clean across all of these — no leaks, no UAF, no double-free.
Every `Traced` ctor is paired with exactly one dtor.

### `expected` vs. `variant<T, E>` — when to reach for which

| If you mean... | Reach for |
| --- | --- |
| "this function might fail" (success is the normal case) | `expected<T, E>` |
| "this value could be one of several unrelated types" | `variant<Ts...>` |
| "I need to visit, but I also have a clear 'good' case" | `expected<T, E>` (visiting is still possible, just less necessary) |
| "I need a default-constructible tagged union" | `variant<Ts...>` with `monostate` first |
| "I'm on C++17" | `variant<T, E>` (expected is C++23) |

The shape is identical (`expected` and `variant` are both
"discriminated unions of two types with an active alternative"). The
distinction is purely about *intent*: `expected` *means* "result or
error," and the type system gives you success-side accessors
accordingly. `variant` *means* "one of these," and you have to
distinguish the alternatives yourself.

Section 10 of the program is a side-by-side of the two for the same
data: a `variant<int, string>` and an `expected<int, string>` both
holding the same `(int=42, error="nope")` pair, just spelled
differently.

## Sample output highlights

```
========== 1. Declaring expected<T, E> ==========
ok_value.has_value()        = true
ok_value                    = 42
bad_value.has_value()       = false
bad_value.error()           = nope
static_cast<bool>(ok_value) = true
static_cast<bool>(bad_value)= false
default-constructed has_value() = true, *def = 0
```

```
========== 4. value_or() -- default on the failure path ==========
a.value_or(-1) = 100
b.value_or(-1) = -1
size of expected<int, string>             = 32
size of expected<unique_ptr<int>, string> = 32
```

The sizes are equal: `expected` pads to fit the larger of `T` and `E`,
plus a discriminator. This is the same shape as `variant`. The cost of
always carrying the discriminator is one byte's worth of padding; the
benefit is being able to switch alternatives in O(1) at runtime.

```
========== 5. Monadic interface -- chaining expected-returning operations ==========
transform: 21 -> 42
transform on failure: error="err1", has_value=false
and_then("hi"): n=2, then n+100=102
and_then on failure: error="parse_one rejected"
or_else rewrap: error="high-level: low-level"
or_else recovery: has_value=true, value=999
transform_error: error="E: io"
```

The `or_else recovery` line is the most interesting: a failure-path
adapter that returns a *success* `expected`, recovering from the error.
This is how you implement "fall back to a default value" cleanly:

```cpp
auto v = parse_int(s).or_else([](const ParseError&) { return 0; });
```

```
========== 7. Move semantics -- expected owns its T or E ==========
  --- constructing an expected<Traced, std::string> with Traced ---
  [Traced] ctor     (src) payload='payload-src'
  [Traced] MOVE     (src)
  [Traced] dtor     ((moved)) payload=''
  --- moving it into another expected ---
  [Traced] MOVE     (src)
  --- after the move: ---
  et.has_value()  = true  (still valid: alternative didn't change)
  et2.has_value() = true
  --- (scope ends; et2 dtor fires) ---
  [Traced] dtor     (src) payload='payload-src'
  [Traced] dtor     ((moved)) payload=''
```

The destructions on scope exit are: `et2`'s `Traced` (full payload),
then `et`'s `Traced` (moved-from, empty payload). Two ctors, two
dtors — no leaks.

```
========== 9. Mini-example: parse_int pipeline ==========
parse_int("123") = 123
parse_int("-456") = -456
parse_int("") ERR: ParseError{where='parse_int', detail='empty input', offset=0}
parse_int("+") ERR: ParseError{where='parse_int', detail='no digits after sign', offset=1}
parse_int("12a") ERR: ParseError{where='parse_int', detail='non-digit character 'a'', offset=2}
parse_int("999999999999") ERR: ParseError{where='parse_int', detail='value out of range', offset=9}
parse_int("0") = 0
```

The richer `ParseError` (with `where`, `detail`, `offset`) is where
`expected<T, E>` pays off: the E can be *any* type, not just a string.
A `ParseError` struct with stream-output and a structured shape
makes the failure mode first-class, not just an opaque error message.

```
  --- full chain: parse -> transform(double) -> and_then(positive) ---
  "123" -> 246
  "-456" -> ERR: ParseError{where='positive_check', detail='value must be > 0 after doubling, got -912', offset=0}
  "" -> ERR: ParseError{where='parse_int', detail='empty input', offset=0}
  "+" -> ERR: ParseError{where='parse_int', detail='no digits after sign', offset=1}
  "12a" -> ERR: ParseError{where='parse_int', detail='non-digit character 'a'', offset=2}
  "999999999999" -> ERR: ParseError{where='parse_int', detail='value out of range', offset=9}
  "0" -> ERR: ParseError{where='positive_check', detail='value must be > 0 after doubling, got 0', offset=0}
```

Three chained operations, three places where it can fail. The error
*origin* is preserved (the `where` field tells you which step failed)
because `transform` passes through the original error and `and_then`
constructs a new `ParseError` with its own `where` tag. The fluent
shape is a single `auto r = ...;` expression.

## Bugs and gotchas I hit

1. **`std::expected<T, E>` default-constructibility.** My first draft
   had a `static_assert` claiming `expected<int, string>` is NOT
   default-constructible. Wrong polarity: it IS, because `int` is.
   The real rule is "default-constructible iff T is." I caught it on
   the first compile.

2. **`std::mutex` is default-constructible.** I tried to use
   `std::mutex` as my "not default-constructible" type for the
   `static_assert`. It's not — `std::mutex m;` is legal. Used a
   user-defined `NotDefault` type with `NotDefault() = delete` instead.

3. **`operator<=>` for `std::expected` is not in libc++ 21 yet.** The
   C++23 spec defines a spaceship that returns `partial_ordering`,
   but Apple's libc++ 21 only ships `operator==`. The compile error
   told me so. I switched the comparison section to test only `==`,
   and added a comment that the ordering rule still holds semantically.

4. **`<mutex>` is fine to include but I removed it.** Initially
   included for the (incorrect) static_assert. After fixing the
   example, I dropped it — `<mutex>` is heavy and unused.

5. **`std::expected` doesn't have a `transform` overload that takes
   no arguments.** Spelled it once without the lambda and the compiler
   helpfully suggested "did you mean `value()`?" Caught it on read.

## C++17 fallback

If you need to use this pattern on C++17, the equivalent is
`std::variant<T, E>` plus a convention: the first alternative is the
success type. You then write helpers like:

```cpp
template <class T, class E>
T& ok(std::variant<T, E>& v) { return std::get<T>(v); }

template <class T, class E>
const E& err(const std::variant<T, E>& v) { return std::get<E>(v); }
```

…but you lose the `and_then` / `transform` adapters and have to write
the chain logic yourself. The natural monadic interface was the main
ergonomic win of `expected`; without it, the C++17 variant version
is workable but verbose.

## Verification

- `g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g`: **clean**
- `g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -fsanitize=address`:
  **clean** (no leaks, no UAF, no double-free in the move traces)
- Program terminates normally with exit status 0. All 10 sections print
  what they claim to print. The `or_else recovery` case transitions
  from failure to success cleanly. The full `parse -> transform ->
  and_then` chain handles every input in the test table.

## Key takeaways

- **`std::expected<T, E>` is the vocabulary type for "result or error."**
  Use it as the return type of any function that can fail.
- **The success side is named.** `value()`, `operator*`, `operator bool`
  all mean "success." `error()` means "give me the error."
- **The monadic interface is the real ergonomic win.** `and_then`,
  `or_else`, `transform`, `transform_error` compose `expected`-returning
  operations the way `Result` does in Rust.
- **`std::unexpected<E>` is just a tag wrapper** to disambiguate the
  failure constructor. Use it.
- **`expected<void, E>` is the "no data, just success/error" variant.**
  Perfect for side effects that might fail.
- **Default-constructible iff T is.** The error side's default-ctor is
  irrelevant; the error is only constructed when you say
  `std::unexpected(E{...})`.
- **vs. `variant<T, E>`**: `expected` is the same shape but with a
  named success side and built-in monadic combinators. `variant` is
  more general (any number of alternatives, no "good" case) but
  requires you to do the dispatch yourself.
- **C++23 only.** On C++17, fall back to `variant<T, E>` plus your
  own helpers.

## Next Steps

- **`std::visit` with stateful visitors** — a visitor that accumulates
  results across alternatives, useful for serialization / pretty-printing
  / hash computation. Still on the Jun 10 list. (Build a pretty-printer
  for a `std::variant<A, B, C>` that writes the active alternative's
  fields to an `ostream`.)
- **Recursive `std::variant` in `std::unique_ptr`** — the common pattern
  for tree-like types is `std::variant<Leaf, std::unique_ptr<Node>>`
  rather than the `std::shared_ptr` dance I used in the Jun 10
  evaluator. Smaller, faster, value-semantic. Worth a follow-up that
  rebuilds the expression evaluator with `unique_ptr` instead of
  `shared_ptr`.
- **Tagged union vs. inheritance for ASTs** — when is the variant
  approach clearer than a `virtual Expr::accept(...)` visitor? Both
  work; the variant is open-closed on the *alternatives* but closed
  on the *operations*, while the inheritance version is the
  opposite. Useful to think about.
- **`std::span` (C++20)** — but worth understanding the C++17 design
  by hand first. (Still on the list from Jun 9.) A "non-owning view
  over a contiguous range" — pair, struct, and array.
- **Build the multi-file `Inventory` as separate compilation** —
  the Inventory example from Jun 9 is currently one big file; splitting
  it into `.h` / `.cpp` / `Makefile` would let us see how moves work
  at the link boundary. (Still on the list from Jun 9.)
- **`enable_shared_from_this`** — combine with variant/visitor /
  expected for a complete value-semantic + shared-ownership toolkit.
- **`std::move_if_noexcept`** — ties together `noexcept` and the
  move/copy decision. If a type's move constructor is `noexcept`,
  `vector` will move it during reallocation; otherwise, it copies. A
  one-line idiom that quietly shows up everywhere.
- **A "real" error type.** The `ParseError` in section 9 is on the
  right path but is just a POD struct. Designing an error hierarchy
  with `std::error_code` integration is the next step if you want
  to use this in production. C++23's `std::expected` + `std::error_code`
  is a strong idiom for low-level libraries.

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `cpp_practice/P-2026-06-11-std-expected.cpp` and
  this notes file (`.md`).
- **P- prefix per the new convention (this is Petra's session).**
- **Build flag is `-std=c++23`** because `<expected>` is C++23. The
  cron workflow's default is `-std=c++17`; this is the documented
  exception.

---

*`std::expected` is the smallest new vocabulary type C++23 ships, and
the one with the highest day-to-day payoff. It replaces a pattern
(`std::variant<T, std::string>`) that I was using just two sessions ago,
and it does so without the boilerplate. The monadic interface is the
piece to memorize: `transform` for pure functions, `and_then` for
chained fallible operations, `or_else` for error recovery / rewrap,
`transform_error` for error-only transforms. With those four, the
`if (r) ... else return error;` ladder disappears, and the type
system enforces that callers handle the error case — which is the
real point.*
