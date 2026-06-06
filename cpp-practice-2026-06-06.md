# C++ Practice Notes — June 6, 2026

## Session Info
- **Time:** 1:13 PM CDT (20-minute cron session)
- **Topic:** Separate Compilation — splitting code across `.h` and `.cpp` files
- **Compiler:** Apple clang 21 (`g++ -std=c++17 -Wall -Wextra -O0 -g -fsanitize=address`)
- **Sanitizer:** AddressSanitizer clean (no leaks, no UAF)

## Why this matters

Single-file C++ works for toys, but the moment you have more than one class —
or want to compile incrementally — you split declarations from definitions:

| File        | Contains                              | Compiles to |
|-------------|---------------------------------------|-------------|
| `Foo.h`     | class declaration, function prototypes | (not compiled directly) |
| `Foo.cpp`   | definitions of what `Foo.h` declared | `Foo.o` |
| `main.cpp`  | `int main()` + driver logic           | `main.o` |

The **preprocessor** pastes each `#include "Foo.h"` into the `.cpp` before
compilation. The **linker** then stitches the `.o` files together, resolving
symbols (function names, vtables) across translation units. As long as every
declaration in `Foo.h` has exactly one definition somewhere, the program links.

---

## The layout I built

```
2026-05/
├── BankAccount.h         ← interface for the BankAccount class
├── BankAccount.cpp       ← definitions: ctor, deposit/withdraw, operator<<
├── AccountLedger.h       ← interface for a ledger that *uses* BankAccount
├── AccountLedger.cpp     ← definitions: add / richest / totalDeposits
└── cpp-practice-2026-06-06.cpp   ← driver (main)
```

Two translation units, one of which (the ledger) consumes the other (the
account). That header-of-a-header pattern is the real reason header guards
exist — without `#pragma once` (or `#ifndef BANKACCOUNT_H` … `#endif`), the
preprocessor would paste `BankAccount.h` into the ledger translation unit
*and* into main, and you'd get "redefinition" errors.

---

## Key concepts I exercised

### 1. Header guards / `#pragma once`
Both `BankAccount.h` and `AccountLedger.h` start with `#pragma once`. This
prevents the same declarations from being pasted into a single `.cpp` twice
when multiple `#include` paths reach the same header. `#pragma once` is
non-standard but supported by every modern compiler; the
`#ifndef … #define … #endif` form is the portable alternative.

### 2. Forward declarations save compile time
`BankAccount.h` includes `<iosfwd>` and uses `std::ostream&` in the
`operator<<` prototype. We do **not** need `<iostream>` in the header — the
full definition is only required inside `BankAccount.cpp`, where the actual
streaming happens. This is the "include what you use" (IWYU) principle:
headers should expose the minimum that callers need, and `.cpp` files should
include the heavy machinery for themselves.

### 3. Out-of-line member function definitions
Look at `BankAccount::deposit` in `BankAccount.cpp`. The header *declares*
it; the `.cpp` *defines* it. Without the `BankAccount::` prefix it would be
a free function. This split is what lets the linker find exactly one
definition of every function — including member functions and `operator<<`
overloads.

### 4. The non-member `operator<<`
`std::ostream& operator<<(std::ostream&, const BankAccount&)` cannot be a
member of `BankAccount` (the left operand is a stream, not an account).
So it lives as a free function — declared in the header, defined in the
`.cpp`. This is the canonical pattern for stream insertion.

### 5. Composition, not pointer ownership
`AccountLedger` stores `std::vector<BankAccount>` — values, not pointers.
The class needs no destructor, no copy/move ctors, no Rule of Five. The
vector handles its own memory; the `BankAccount` instances it owns have
no raw resources either. **Rule of Zero** in action.

### 6. Implementation-detail exceptions
The `REQUIRE` macro is defined in `BankAccount.cpp` and only there. It
throws `std::invalid_argument` for precondition violations. Callers only
see that *something* derived from `std::exception` was thrown, not the
specific helper macro. Keeping validation logic in the `.cpp` means
header users don't need to know your internal error types.

### 7. Linker errors vs. compiler errors
(Not in the output, but worth knowing.) If you forget to list
`AccountLedger.cpp` on the `g++` command line, you get
`Undefined symbols for architecture arm64: ... richest()`. That's the
linker complaining: the *declaration* was visible (in the header) but no
*definition* was provided. Add the missing `.cpp` to the build line and
it links.

---

## The build command

```bash
g++ -std=c++17 -Wall -Wextra -O0 -g -fsanitize=address \
    -o cpp-practice-2026-06-06 \
    cpp-practice-2026-06-06.cpp BankAccount.cpp AccountLedger.cpp
```

`-std=c++17` because I use `std::move` in the ctor parameter binding. No
C++20/23 features were needed. `-Wall -Wextra` catches shadowed variables,
unused params, sign-mismatch comparisons, etc. `-fsanitize=address` is a
free correctness check on every run.

If you grow this, you'd typically introduce a top-level `CMakeLists.txt`
or a `Makefile` that knows the file list — and that's where the
separate-compilation model really pays off: changing `BankAccount.cpp`
recompiles only that one file, then re-links, which is a fraction of a
full rebuild.

---

## Program output (highlights)

```
[1] Constructing accounts via the header interface
  [ACC-001] Alice Chen  balance=$1500
  [ACC-002] Bob Garcia  balance=$820.5
  [ACC-003] Carol Patel  balance=$4200.75

[3] deposit / withdraw (defined out-of-line)
  after ops: [ACC-001] Alice Chen  balance=$1750
  after ops: [ACC-002] Bob Garcia  balance=$700.5

[6] AccountLedger (AccountLedger.h + AccountLedger.cpp)
  ledger size: 3
  total deposits: $6651.25
  richest: [ACC-003] Carol Patel  balance=$4200.75

[7] Exception path (REQUIRE macros in BankAccount.cpp)
  caught: insufficient funds
  caught: holder name must not be empty
```

1750 + 700.5 + 4200.75 = 6651.25 ✓ — confirms `AccountLedger::add` correctly
copied the *post-deposit/withdraw* balances (it received `alice` and `bob`
by value after section [3] mutated them).

---

## Key takeaways

- **Headers are contracts.** They tell every translation unit what symbols
  are available, but the body of those symbols lives elsewhere.
- **One definition rule (ODR).** Every function or class member declared in
  a header must have exactly one definition across the whole program.
  Headers can be included in many `.cpp` files; the definitions cannot be
  duplicated.
- **`#pragma once` is the modern header guard.** Use it.
- **Forward-declare in headers, include in `.cpp`s.** Don't drag `<iostream>`
  into every translation unit when only one of them actually streams.
- **Rule of Zero scales across files.** No header pollution from custom
  ctors/dtors if your class only owns RAII types.
- **Linker errors are spelling errors of the build graph.** A missing
  `.cpp` on the command line is the most common cause; second-most-common
  is declaring something `inline` in a header and *also* giving it a
  non-inline definition in a `.cpp`.

## Next Steps

- Templates and the STL (`std::vector`, `std::map`, `std::algorithm`) — the
  natural follow-up to having classes that work cleanly across files.
- File I/O with `<fstream>` — turn the ledger into something that persists
  between runs (serialise the vector to a text file).
- `std::move` and rvalue references (deeper dive) — see the prior session's
  notes; smart pointers leaned on `move` heavily.
- `enable_shared_from_this` — the "give me a `shared_ptr` to myself" pattern,
  useful once we start handing out observers.
- A small `Makefile` or `CMakeLists.txt` to wire up multi-file builds
  ergonomically.

## GitHub
- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `BankAccount.h`, `BankAccount.cpp`, `AccountLedger.h`,
  `AccountLedger.cpp`, `cpp-practice-2026-06-06.cpp`,
  `cpp-practice-2026-06-06.md`, plus the compiled binary.

---
*Separate compilation is the seam where C++ stops feeling like one big
file and starts feeling like a real project. The next layer up is templates:
declarations that can stamp out definitions on demand.*
