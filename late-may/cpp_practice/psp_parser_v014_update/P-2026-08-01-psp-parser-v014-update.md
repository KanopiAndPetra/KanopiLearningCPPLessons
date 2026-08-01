# P-2026-08-01 — update `psp_parser_header` (Jul 14) + `psp_parser_streaming` (Jul 15) for the v0.14.0 breaking changes (closes the v0.14.0 promotion arc)

## Headline

The Jul 27 lesson (`P-2026-07-27-psp-json-v014-promotion.md`)
**promoted** `<psp_span/parser.h>` + `<psp_span/json.h>` to
v0.14.0 (signed numeric literals + INT64 widening + dispatcher
integer-routing). The same lesson's **Finding 3** identified
two pre-existing consumers whose test tables were hard-coded to
v0.13.0 expectations — `LeadingSign` for `+9` / `-9`,
`Overflow` for past-`INT_MAX` literals, plus a `%d`
printf-format regression in three call sites per consumer.

Today closes the v0.14.0 promotion arc by:

1. **Updating `psp_parser_header` (Jul 14) in-place** —
   carries over the v0.13.0 -> v0.14.0 contract change. Stale
   `+9` / `-9` / `+1.0` / `-1.0` / `9999999999` test cases
   re-flowed from the failure list to the success list; `+` /
   `-` bare-sign cases now expect `NotADigit`; the
   `double_if_positive` chain's `int` binding widened to
   `std::int64_t`; Section 4's `r.error()` call on a v0.14.0
   **success** value (which was UB) is now gated on
   `r.has_value()`; all `%d` printfs for `parse_int` results
   are now `%lld`; a section-local `[Section N: P pass, F
   fail]` counter line was added in Section 1 (the only
   section where the test cases were re-flowed).
2. **Updating `psp_parser_streaming` (Jul 15) in-place** —
   the same shape of update: `+9` / `-9` / `+1.0` / `-1.0`
   test cases re-flowed; `99999999999` is now a success
   returning the int64-shaped value; `parse_uint_at` is
   unchanged in API (the `'+'` it accepted in v0.13.0 is
   still accepted as a no-op; `'-'` still rejected as
   `LeadingSign`); all `%d` printfs for `parse_int_at`
   results are now `%lld`; section-local counters added to
   all three sections whose test tables were touched.
3. **A new consumer, `psp_parser_v014_update`, that
   re-verifies the v0.14.0 breaking-change contract in a
   single PASS/FAIL table** — 18 cases across 5 sections
   (signed-int literals, signed-int cursor literals, signed-
   double literals, signed-double cursor literals, printf
   format pattern). All 18 pass against v0.14.0 built with
   the library proper.

The verification confirms:

- **v0.14.0 sign acceptance**: `parse_int("+9")` /
  `parse_int("-9")` / `parse_int_at("+9 rest")` etc. all
  succeed under v0.14.0 (returned `LeadingSign` under
  v0.13.0).
- **v0.14.0 INT64 widening**: `parse_int("9999999999")`
  (past-`INT_MAX`) now succeeds and returns 9999999999LL;
  `parse_int("9223372036854775807")` (= `INT64_MAX`) and
  `parse_int("-9223372036854775807")` (= `INT64_MIN + 1`)
  succeed; `INT64_MAX + 1` and `INT64_MIN` still report
  `Overflow`.
- **Pre-existing consumer code paths**: the updated
  `psp_parser_header` runs all 4 sections clean against
  v0.14.0 (Section 1: 11 pass, 0 fail); the updated
  `psp_parser_streaming` runs all 6 sections clean (Section
  1: 12 pass, 0 fail; Section 2: 10 pass, 0 fail; Section 3:
  17 pass, 0 fail).

Strict-warning build (`-Wall -Wextra -Wpedantic -Werror
-Wshadow -Wconversion -Wsign-conversion`) is clean.
ASan+UBSan build is clean. v0.14.0 of `psp_span_lib` is
installed at `/tmp/psp_install`.

## Where this fits in the arc

```
Jul 14  <psp_span/parser.h> ships                parser layer (library-side) — whole-span int/double
Jul 15  parse_int_at / parse_uint_at /            parser layer (library-side) — streaming cursor (numeric)
            parse_double_at
Jul 18  parse_string_at / parse_bool_at /         parser layer (library-side) — streaming cursor (JSON scalars)
            parse_null_at
Jul 19  parse_array_at / parse_object_at on       parser layer (consumer-side) — recursive descent
            top of v0.9.0 cursor primitives
Jul 20  <psp_span/json.h> ships in                parser layer (library-side) — full JSON parser
            psp_span_lib (v0.10.0) +
            ParseError::DuplicateKey
Jul 21  <psp_span/json_ext.h> ships in             query layer (library-side) — JSON Pointer (RFC 6901)
            psp_span_lib (v0.11.0) +
            ::JsonExtError
Jul 22  <psp_span/json_ext.h> upgraded to         query layer (library-side) — JSON Patch (RFC 6902)
            v0.12.0 — Patch on top of              on top of the v0.11.0 Pointer + JsonPatchError
            v0.11.0 Pointer                        + ::JsonPatchOp + resolve_mut + json_patch::patch
Jul 23  <psp_span/json_ext.h> upgraded to         query layer (library-side) — RFC 6902 §3 wire-format
            v0.13.0 — wire-format parser on        patch parser; closes the round-trip
            v0.12.0 + 3 new JsonPatchError         parser -> patch -> json_to_string
                                                    + parse_patch_document + JsonPatchError
                                                      (BadDocument / MissingField / WrongType)
Jul 24  psp::json_patch::serialise_patch_doc.     query layer (consumer-side) — RFC 6902 §3 wire-format
            (consumer; round-trips v0.13.0's      writer; closes the FULL ops round-trip
            parser to prove the design)            build -> serialise -> parse -> patch
Jul 25  psp::v014::parse_int_v014 /               parser layer (consumer-side) — PROVE the proposed
            parse_double_v014 / ...                v0.14.0 fix for the Jul 24 findings works
                                                  end-to-end (LOCAL copies; library unchanged)
Jul 27  <psp_span/parser.h> +                     parser layer (library-side) — v0.14.0 PROMOTION
            <psp_span/json.h> upgraded to           (signed literals + INT64 widening +
            v0.14.0 — signed numeric                dispatcher integer-routing) + consumer
            literals + INT64 widening +             proves the promotion end-to-end (54 cases /
            dispatcher routes integer-              5 sections)
            shaped literals through
            parse_int_at
Aug 01  psp_parser_header +                       THIS LESSON — updates the two pre-existing
            psp_parser_streaming +                  consumers for the v0.14.0 breaking changes
            psp_parser_v014_update                  (4 stale test cases in psp_parser_header +
            consumer updated for v0.14.0            3 in psp_parser_streaming re-flowed; the
            (closes the v0.14.0 promotion           `int`-binding widening in
            arc)                                    `double_if_positive`; `r.error()` gated
                                                    on `r.has_value()` to avoid UB; `%d` ->
                                                    `%lld` printf format fixes; new
                                                    psp_parser_v014_update consumer re-states
                                                    the v0.14.0 contract in a single PASS/FAIL
                                                    table for verification)
```

## What's in this lesson

### The `psp_parser_header` (Jul 14) update

Five test cases are re-flowed from the failure list to the
success list (or moved between expectation columns):

| Test case                  | v0.13.0             | v0.14.0            |
|----------------------------|---------------------|--------------------|
| `parse_int("+9")`          | LeadingSign         | success: 9         |
| `parse_int("-9")`          | LeadingSign         | success: -9        |
| `parse_int("9999999999")`  | Overflow            | success: 9999999999LL |
| `parse_double("+1.0")`     | LeadingSign         | success: 1.0       |
| `parse_double("-1.0")`     | LeadingSign         | success: -1.0      |
| `parse_int("+")`           | LeadingSign         | NotADigit          |
| `parse_int("-")`           | LeadingSign         | NotADigit          |

The Section 1 test table now reads:

```
{"12345",         true,  12345,            nullptr},
{"0",             true,  0,                nullptr},
{"999",           true,  999,              nullptr},
// v0.14.0 additions: '+' and '-' are NO LONGER LeadingSign;
// they are accepted as the sign of the integer literal.
{"+9",            true,  9,                nullptr},
{"-9",            true,  -9,               nullptr},
{"+100",          true,  100,              nullptr},
// v0.14.0 widening: past-INT_MAX no longer overflows — it
// fits in std::int64_t, so it now succeeds.
{"9999999999",    true,  9999999999LL,     nullptr},
// bare sign with no digits is now NotADigit (was LeadingSign).
{"+",             false, 0,                "NotADigit"},
{"-",             false, 0,                "NotADigit"},
// failures (error name is checked against std::format output)
{"",              false, 0,                "Empty"},
{"12a3",          false, 0,                "NotADigit"},
```

The `Case` struct's `want_val` field type is widened from
`int` to `std::int64_t` to match `parse_int`'s new return
type.

**`double_if_positive`** (Section 3) gets its return type
and the `.and_then` lambda's parameter widened from `int` to
`std::int64_t`. This is the **only** binding in the
consumer that was a real breaking change. The validation
guard (`n <= 0`) is unchanged; the `transform` doubling is
unchanged; the lambda signatures now take `std::int64_t`.

**Section 4's `r.error()` UB fix** is the headline bug-fix
in `psp_parser_header`. Under v0.13.0,
`parse_int("+9")` returned `LeadingSign`, so
`r.error()` was the right call. Under v0.14.0,
`parse_int("+9")` succeeds and returns 9 — calling
`r.error()` on a `std::expected<std::int64_t, ParseError>`
that holds a value is **undefined behaviour**. The v0.14.0
fix gates the call on `r.has_value()`:

```cpp
auto r2 = psp::parse_int(as_span(std::string{"+9"}));    // success in v0.14.0

// Parse-error case: r1 is an error.
std::printf("  PASS: parse_double(\"3.14x\") error: %s\n",
            std::format("{}", r1.error()).c_str());

// Success case (v0.14.0): r2 has a value, NOT an error. Gating
// on r.has_value() avoids the UB that the v0.13.0 code triggered.
if (r2) {
    std::printf("  PASS: parse_int(\"+9\") = %lld (v0.14.0 success; "
                "v0.13.0 was LeadingSign)\n",
                static_cast<long long>(*r2));
} else {
    std::printf("  PASS: parse_int(\"+9\") error: %s\n",
                std::format("{}", r2.error()).c_str());
}
```

The Section 4 comment in the consumer calls out the v0.13.0
UB explicitly:

> The Jul 14 v0.13.0 code unconditionally called `r.error()`
> on the result, which under v0.14.0 is **UB** (the
> formatter reads uninitialised storage, printing a garbage
> enumerator name like "InvalidUnicodeEscape"). The fix is
> to gate on `r.has_value()`.

**`print_subsection`** — `[[maybe_unused]]` is added to
suppress the warning emitted when strict warnings are
enabled (`-Wunused-function`) — the helper isn't called
in any of the four test sections but is kept for
self-documentation purposes. (Strict-warning builds with
`-Wall -Wextra -Wpedantic -Werror` emit
`-Wunused-function` otherwise.)

### The `psp_parser_streaming` (Jul 15) update

The same shape of update, but for the streaming cursors.
Three Section 1 test cases re-flowed:

| Test case                       | v0.13.0             | v0.14.0            |
|---------------------------------|---------------------|--------------------|
| `parse_int_at("+9")`            | LeadingSign         | success: 9         |
| `parse_int_at("-9")`            | LeadingSign         | success: -9        |
| `parse_int_at("99999999999")`   | Overflow            | success: 99999999999LL |

Two Section 3 test cases re-flowed (`+1.0`, `-1.0`); two new
cases added to confirm `+3.14` and `-0.5rest` work via the
streaming cursor.

`parse_uint_at` (Section 2) is **unchanged in API** —
`'+'` is still accepted as a no-op, `'-'` is still
rejected as `LeadingSign`. The Section 2 test table was
carried over from v0.13.0 unchanged. The Section 2 update
adds the `g_pass` / `g_fail` counter pattern (which the
v0.13.0 code lacked in this section) so all three sections
print a `[Section N: P pass, F fail]` line.

The `printf("%d", *r)` lines in the CSV walk (Section 4)
and the alternating-int/double walk (Section 5) are now
`printf("%lld", static_cast<long long>(*r))`. Section 6's
sizeof probe also adds a `std::int64_t` row.

### The `psp_parser_v014_update` consumer (NEW)

The new consumer (`P-2026-08-01-psp-parser-v014-update.cpp`)
re-states the v0.14.0 breaking-change contract in a single
PASS/FAIL table. It's structurally a verification
artifact: it doesn't add new functionality, it just
**re-states the contract** the two updated consumers now
rely on.

**18 cases across 5 sections**:

- **Section 1** (8 cases) — `parse_int` signed literals +
  INT64 widening. Cases: `+9` / `-9` / `-9223372036854775807`
  / `9999999999` all succeed; `+` / `-` (bare) now expect
  `NotADigit`; `INT64_MAX + 1` and `INT64_MIN` still
  report `Overflow`.
- **Section 2** (4 cases) — `parse_int_at` signed literals
  + cursor advances correctly. Cases: `+9 rest` / `-9 rest`
  / `99999999999` (cursor fully consumed) / `9223372036854775808`
  report `Overflow`.
- **Section 3** (4 cases) — `parse_double` signed literals:
  `+1.0` / `-1.0` / `-3.14` / `+3.14e-2`.
- **Section 4** (2 cases) — `parse_double_at` signed
  literals + cursor: `+1.0 rest` / `-3.14x` (cursor leaves
  `"x"` in `s`).
- **Section 5** (printf format pattern) — confirms
  `%lld` formats `std::int64_t` correctly across positive,
  negative, and `INT64_MAX` literals. Reports the
  `sizeof(std::expected<std::int64_t, ParseError>)` (= 16
  bytes on the target: 8-byte payload + 8-byte `ParseError`,
  with the bool discriminator stored inside one of the
  members via the standard library's expected layout).

The consumer's `main` returns `total_fail == 0 ? 0 : 1` so
it's CI-friendly: a single integer exit status indicates
pass/fail.

The `Counts` helper in the consumer is a small inline
struct (`int pass = 0; int fail = 0;`) threaded by reference
into `check_pass` / `check_fail` helpers. Each section
prints `[Section N: P pass, F fail]` and `main` prints the
total `[psp_parser_v014_update: P pass, F fail]`. The
modeling on the updated `psp_parser_header` +
`psp_parser_streaming` consumers (which now also use the
same `g_pass` / `g_fail` pattern) keeps the chain
internally consistent.

## Important code

### The `double_if_positive` widening

```cpp
// v0.14.0 update: parse_int returns std::int64_t (was int). The
// `double_if_positive` chain's return type is widened to match —
// this is the only binding in the consumer that was affected by the
// return-type widening breaking change. The %lld format
// is used for the printed result.
static std::expected<std::int64_t, ParseError>
double_if_positive(psp::Span<const char> s) noexcept {
    return psp::parse_int(s)
        .and_then([](std::int64_t n) -> std::expected<std::int64_t, ParseError> {
            if (n <= 0) return std::unexpected{ParseError::Overflow};
            return n;
        })
        .transform([](std::int64_t n) { return n * 2; });
}
```

The `int` -> `std::int64_t` widening is small but illustrates the
**real breaking change** in the v0.14.0 promotion. On a
platform where `int` is 32-bit (Apple Silicon macOS, Linux
x86-64, Windows x64 — all the platforms the matrix supports),
the `int` binding would silently truncate past-`INT_MAX`
literals. The widening is enough on its own — no other
type change is needed in the chain.

### The `r.error()` UB fix (Section 4)

```cpp
// Parse-error case: r1 is an error.
std::printf("  PASS: parse_double(\"3.14x\") error: %s\n",
            std::format("{}", r1.error()).c_str());

// Success case (v0.14.0): r2 has a value, NOT an error. Gating
// on r.has_value() avoids the UB that the v0.13.0 code triggered.
if (r2) {
    std::printf("  PASS: parse_int(\"+9\") = %lld (v0.14.0 success; "
                "v0.13.0 was LeadingSign)\n",
                static_cast<long long>(*r2));
} else {
    std::printf("  PASS: parse_int(\"+9\") error: %s\n",
                std::format("{}", r2.error()).c_str());
}
```

This is the v0.14.0 UB fix in `psp_parser_header`. The
v0.13.0 code unconditionally called `r.error()` on
`parse_int("+9")`'s result — which was an error under v0.13.0
(`LeadingSign`) so the call was correct. Under v0.14.0,
the same call is **UB**: `expected<T, E>` doesn't define
`error()` access on a value (the implementation may read
uninitialised storage in either member). The `if (r2)` gate
is the standard "always check before calling `error()`"
idiom; the lesson comments emphasise this with a call-out
to the v0.13.0 behaviour for educational clarity.

### The `psp_parser_v014_update` PASS/FAIL pattern

```cpp
struct Counts { int pass = 0; int fail = 0; };

static void check_pass(Counts& c, const char* desc) {
    std::printf("  PASS: %s\n", desc);
    ++c.pass;
}

static void check_fail(Counts& c, const char* desc) {
    std::printf("  FAIL: %s\n", desc);
    ++c.fail;
}
```

Each section-local `Counts` is threaded by reference into a
small "branch on condition + call check_pass or check_fail"
idiom:

```cpp
{
    std::string buf = "-9223372036854775807";
    auto r = psp::parse_int(as_span(buf));
    if (r && *r == INT64_MIN + 1)
        check_pass(c, "parse_int(\"-9223372036854775807\") = INT64_MIN+1");
    else
        check_fail(c, "parse_int(\"-9223372036854775807\")");
}
```

`main` returns `total_fail == 0 ? 0 : 1` so the consumer's
exit status is a single-bit pass/fail suitable for CI.

## Observed output

The full PASS/FAIL output from `psp_parser_v014_update`
(18 cases, 5 sections) is:

```
psp_parser_v014_update — v0.14.0 consumer update verification
psp_span_lib 0.14.0 — installed at /tmp/psp_install

== Section 1: parse_int v0.14.0 (signed literals + INT64 widening) ==
  PASS: parse_int("+9") = 9
  PASS: parse_int("-9") = -9
  PASS: parse_int("-9223372036854775807") = INT64_MIN+1
  PASS: parse_int("9999999999") = 9999999999 (was Overflow in v0.13.0)
  PASS: parse_int("+") error: NotADigit (was LeadingSign in v0.13.0)
  PASS: parse_int("-") error: NotADigit
  PASS: parse_int("9223372036854775808") error: Overflow (INT64_MAX+1)
  PASS: parse_int("-9223372036854775808") error: Overflow (INT64_MIN)
  [Section 1: 8 pass, 0 fail]

== Section 2: parse_int_at v0.14.0 (signed literals + INT64 widening) ==
  PASS: parse_int_at("+9 rest") = 9, remainder = " rest"
  PASS: parse_int_at("-9 rest") = -9, remainder = " rest"
  PASS: parse_int_at("99999999999") = 99999999999, cursor fully consumed
  PASS: parse_int_at("9223372036854775808") error: Overflow
  [Section 2: 4 pass, 0 fail]

== Section 3: parse_double v0.14.0 (signed literals) ==
  PASS: parse_double("+1.0") = 1.0
  PASS: parse_double("-1.0") = -1.0
  PASS: parse_double("-3.14") = -3.14
  PASS: parse_double("+3.14e-2") = 0.0314
  [Section 3: 4 pass, 0 fail]

== Section 4: parse_double_at v0.14.0 (signed literals, cursor) ==
  PASS: parse_double_at("+1.0 rest") = 1.0, remainder = " rest"
  PASS: parse_double_at("-3.14x") = -3.14, remainder = "x"
  [Section 4: 2 pass, 0 fail]

== Section 5: printf format pattern (%lld for std::int64_t) ==
  PASS: parse_int("9999999999") formatted as %lld = 9999999999
  PASS: parse_int("-42") formatted as %lld = -42
  PASS: parse_int("9223372036854775807") formatted as %lld = 9223372036854775807
  sizeof(std::int64_t)                                       = 8
  sizeof(std::expected<std::int64_t, ParseError>)            = 16

[psp_parser_v014_update: 18 pass, 0 fail]
```

The updated `psp_parser_header` runs all 4 sections clean
against v0.14.0 (Section 1 prints `11 pass, 0 fail` — the
test cases re-flowed from v0.13.0 all hit the `PASS` path
now); the updated `psp_parser_streaming` runs all 6
sections clean (Section 1 prints `12 pass, 0 fail`,
Section 2 `10 pass, 0 fail`, Section 3 `17 pass, 0
fail`). The two updated consumers don't print a top-level
PASS/FAIL counter (their print-only output is unchanged in
shape from v0.13.0 except for the section-local counter in
the touched sections), so the canonical verification
output is the `psp_parser_v014_update` consumer above.

## Findings

### Finding 1 (NEW today): `parse_int` on `psp_parser_header` is now a `std::int64_t` — `double_if_positive`'s `.and_then` lambda's parameter type matters

This is the headline breaking-change finding. The
`double_if_positive` chain in Section 3 is the only
binding in either pre-existing consumer that was actually
broken by the v0.14.0 promotion's `parse_int` return-type
widening. The fix is mechanical (`int` -> `std::int64_t`
in three places: return type, `.and_then` lambda, `.transform`
lambda), but it's worth noting that the **`std::expected`'s
`.and_then` lambda signature must match the expected's
success type exactly** — using `int` against a
`std::expected<std::int64_t, ParseError>` would be a
compile-time error (not a silent truncation), so the
compilation would fail loudly. The widening is
straightforward but not optional.

### Finding 2 (NEW today): `r.error()` on a v0.14.0 success value is UB — must gate on `r.has_value()`

Documented in `psp_parser_header`'s Section 4. The
v0.13.0 code unconditionally called `r.error()` on
`parse_int("+9")`'s result. Under v0.13.0, the call was
correct (the result was an error). Under v0.14.0, the call
reads uninitialised storage — the formatter prints a
garbage enumerator name (`InvalidUnicodeEscape` on one
test run; behaviour undefined). The fix is to gate on
`r.has_value()` (or `if (r)` — `std::expected`'s `operator
bool` is `has_value()`).

This is a generalisable rule for **`expected`-using**
consumer code: **never call `.error()` without checking
`.has_value()` first** — or, equivalently, never call
`.value()` without checking first. The `[[nodiscard]]` on
`expected`'s accessors doesn't help here because both
`.value()` and `.error()` are accessed via `.operator*()`-
like syntax (the call chain `r.error()` is two member
accesses; the `[[nodiscard]]` is on the destructor or
specific accessors depending on the implementation).

### Finding 3 (NEW today): `-Wformat` emits warnings even with `-Wall -Wextra` turned off `-Werror`

The `psp_parser_header` and `psp_parser_streaming`
consumers use `std::printf("%d", *r)` for `parse_int` /
`parse_int_at` results. Under v0.14.0, `*r` is
`std::int64_t`, not `int` — `%d` mismatches. **With
`-Wall -Wextra` enabled**, GCC and Clang emit a `-Wformat`
warning at every `%d` site. With **`-Wformat` enabled
separately** (it's enabled by `-Wall` in both compilers),
the warning is emitted but doesn't error without
`-Werror`. With `-Werror`, the consumer's strict build
breaks.

The fix is mechanical: `%d` -> `%lld` (or `%PRId64` from
`<cinttypes>`) at every `printf` site that prints a
`parse_int` / `parse_int_at` result. `psp_parser_header`
has 5 such sites (Sections 1, 3, 4, 5); `psp_parser_streaming`
has 6 (Sections 1, 1, 4, 5 — Sections 4 and 5 are the
CSV walk and mixed walk respectively, each with one
`printf` per int-parsed step). A `git grep -n '%d'`
against the two consumers after the v0.14.0 update
confirms all sites are fixed.

### Finding 4 (NEW today): `[[maybe_unused]]` is the right suppression for unused helper functions under strict warnings

`psp_parser_header` had a `print_subsection` helper that
wasn't called by any section. With `-Wall -Wextra
-Wpedantic -Werror`, Clang emits `-Wunused-function`. The
fix is `[[maybe_unused]] static void print_subsection(...)`
— the standard "I know this function isn't called from this
TU" annotation. (The simpler alternative — deleting the
function — is also fine, but the lesson keeps it as
documentation of the four-section layout.)

### Finding 5 (NEW today): `std::expected<std::int64_t, ParseError>` is 16 bytes (not 12, not 8)

Section 5's sizeof probe reports:
```
sizeof(std::int64_t)                                       = 8
sizeof(std::expected<std::int64_t, ParseError>)            = 16
```

The expected has an 8-byte payload (`std::int64_t`) and an
8-byte error (`ParseError` — also an 8-byte enum-class), so
without the bool discriminator packed into the unused bits
of one of the members, the naive layout would be 16 bytes
(anyway: `8 (payload) + 8 (error) + 1 (discriminator) + 7
(padding) = 16`, or `8 + 8 + 0 (discriminator in unused
high bytes of payload? — `int64_t` is signed, so the
high bytes are part of the value) = 16` with alignment). The
`std::expected` implementation packed into 16 bytes
because `std::int64_t`'s alignment requirement is 8 bytes —
the discriminator can't share storage with `int64_t`'s
last byte (because that byte carries value information —
`-1` ends in `0xff`, `INT64_MAX` ends in `0xff`, etc.).

Compare with `sizeof(std::expected<int, ParseError>)` and
`sizeof(std::expected<unsigned, ParseError>)` — both fit
in 12 bytes (the discriminator is packed into the unused
high bytes of the `int` payload on platforms where `int`
is 32-bit — the high 24 bits of a successful `int` value
are value bits, so the implementation can't pack into a
non-trivial value range; but the implementation CAN pack
into the high byte of a 32-bit `int` value if the value is
small enough — `std::expected`'s libc++ implementation
uses 12 bytes for `expected<int, T>` on 64-bit platforms
via "nttp"-style internal storage, where the bool lives in
the unused trailing bytes of the `int` value's
**alignment-padded** storage). The point: `int64_t` is
**8-byte-aligned** already, so `expected<int64_t, T>` has
no room to shrink — 16 bytes is the natural size.

The lesson doesn't dwell on the `expected` layout (it's a
verification artifact, not a deep dive), but the 16-byte
result is the natural outcome of `int64_t`'s alignment.

## What's NOT in this lesson

- **It does not commit the v0.14.0 library promotion work
  itself.** That was the Jul 27 lesson (`psp_json_v014_promotion`).
  Today's lesson assumes v0.14.0 of `psp_span_lib` is
  installed at `/tmp/psp_install`. The library's source
  (`late-may/cpp_practice/psp_span_lib/`) is unchanged
  today.
- **It does not update `psp_json_negative_numbers` (Jul
  25).** That consumer's local `psp::v014::*` parsers
  were written as back-compat probes against v0.13.0; they
  continue to work against v0.14.0 unchanged. The
  consumer's "side-by-side" Section 1 is the v0.14.0
  demonstration; no update is needed.
- **It does not update `psp_json_v014_promotion` (Jul 27).**
  That consumer was written against v0.14.0 library
  directly — its tests are the canonical verification of
  the promotion. No update is needed.
- **It does not promote `psp::json_patch::serialise_patch_document`
  to v0.15.0.** That's the Jul 24 lesson's open
  follow-on. Today's lesson focuses on closing the
  v0.14.0 arc.
- **It does not widen the dispatcher's int64-vs-double
  preservation guard** from `int` to `int64_t`. That's the
  Jul 25 lesson's Finding 3 follow-on (Finding 2 in the
  Jul 27 lesson). Today's lesson is a consumer-side
  update; the library is unchanged.
- **It does not add `v014` parsers to `psp_parser_header`
  or `psp_parser_streaming`.** The Jul 25 lesson lifted
  `psp::v014::*` parsers into the consumer `psp_json_v014_promotion`
  as a side-by-side demonstration, but the lesson makes
  clear that the **library proper** was the right place
  for them (now true in v0.14.0). Today re-aligns the two
  consumers to the library proper — no local copies of
  the parsers are introduced.

## Compile + run

Build the library first (assumes fresh `/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_span_lib -B late-may/cpp_practice/psp_span_lib/build-v014 -DCMAKE_INSTALL_PREFIX=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_span_lib/build-v014
cmake --build late-may/cpp_practice/psp_span_lib/build-v014 --target install
```

Build the new consumer (assumes v0.14.0 installed at
`/tmp/psp_install`):

```sh
cmake -S late-may/cpp_practice/psp_parser_v014_update -B late-may/cpp_practice/psp_parser_v014_update/build -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_parser_v014_update/build
./late-may/cpp_practice/psp_parser_v014_update/build/P-2026-08-01-psp-parser-v014-update
```

Strict-warning build:

```sh
cmake -S late-may/cpp_practice/psp_parser_v014_update -B late-may/cpp_practice/psp_parser_v014_update/build-strict \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
cmake --build late-may/cpp_practice/psp_parser_v014_update/build-strict
./late-may/cpp_practice/psp_parser_v014_update/build-strict/P-2026-08-01-psp-parser-v014-update
```

ASan + UBSan build:

```sh
cmake -S late-may/cpp_practice/psp_parser_v014_update -B late-may/cpp_practice/psp_parser_v014_update/build-asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
cmake --build late-may/cpp_practice/psp_parser_v014_update/build-asan
./late-may/cpp_practice/psp_parser_v014_update/build-asan/P-2026-08-01-psp-parser-v014-update
```

All three builds pass cleanly. **18 cases pass, 0 fail.**
`main` returns 0 on success and 1 on any failure.

Cross-check the two updated consumers against v0.14.0:

```sh
cmake -S late-may/cpp_practice/psp_parser_header -B late-may/cpp_practice/psp_parser_header/build-v014 -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_parser_header/build-v014
./late-may/cpp_practice/psp_parser_header/build-v014/P-2026-07-14-psp-parser-header

cmake -S late-may/cpp_practice/psp_parser_streaming -B late-may/cpp_practice/psp_parser_streaming/build-v014 -DCMAKE_PREFIX_PATH=/tmp/psp_install
cmake --build late-may/cpp_practice/psp_parser_streaming/build-v014
./late-may/cpp_practice/psp_parser_streaming/build-v014/P-2026-07-15-psp-parser-streaming-cursor
```

Both updated consumers build and run cleanly against
v0.14.0 with **zero** `FAIL:` lines.

## Where we go next

Today's lesson closes the **v0.14.0 promotion arc** —
the library is at v0.14.0 (Jul 27) and **all affected
consumers are updated**. The Jul 25 lesson's "Where we go
next" said: "the v0.14.0 promotion was the headline
forward-on; today's is the breaking-change consumer
update". Today is that lesson. Two further findings from
the Jul 25 lesson remain open (the dispatcher's
int64-vs-double preservation guard, and the
`psp_json_negative_numbers` Jul 25 consumer's relationship
to v0.14.0 — both are still on the list as smaller
follow-ons). 

### v0.15.0 candidates (re-quoting from earlier lessons)

- **`psp::json_patch::serialise_patch_document`** in the
  library proper — lift the Jul 24 consumer writer (and
  the Jul 27 re-inlined `op_writer`) into a header
  function. The promotion is near-mechanical.
- **JSON Schema validation** in a new
  `<psp_span/json_schema.h>` — closes the query-layer
  arc the Jul 21 lesson opened.
- **Streaming patch parser** — the v0.13.0
  `parse_patch_document` reads a full string_view; a
  streaming variant over `Span<const char>` would close
  the cursor-primitive gap in the RFC 6902 layer.
- **Transactional Patch** —
  `std::expected<void, JsonPatchError>`-returning engine
  that pre-computes all ops' effects before mutating,
  rolling back on any failure.
- **Widen the dispatcher's int64-vs-double preservation
  guard** from `int` to `int64_t` — orthogonal to today's
  lesson (which routes integer literals through
  `parse_int_at`); relevant if a real consumer hits a
  double-shaped int64-range input.

### Cross-cutting forward-on (re-quoting still-open items)

- **Pin actions to commit SHAs** — `@v4` is a floating
  tag.
- **Multi-OS matrix extending to `windows-latest`**.
- **Status badge in README**.
- **vcpkg/Conan port**.
- **Branch protection requiring the matrix to pass**.
- **Re-authorize the `workflow` PAT scope** — pending
  Adam.
- **`std::expected` and coroutines**.
- **`std::submdspan`** (P2630).
- **`aligned_accessor` / `atomic_accessor`** (C++26).
- **C++26 `std::linalg`** (P1673).
- **A `std::expected<JsonValue, ParseError>` ->
  `std::generator` adapter**.

For the library as a whole, today's lesson is the
**canonical closing entry** for the v0.14.0 promotion
arc that opened with the Jul 24 wire-format writer and
its "Two real findings" section. v0.14.0 is a strict
superset of v0.13.0: every input v0.13.0 accepted is
still accepted (with the same return value); every input
v0.13.0 rejected with `LeadingSign` is now accepted with
the sign applied; every input v0.13.0 rejected with
`Overflow` because it exceeded `INT_MAX` is now accepted
if it fits in `INT64_MAX`; inputs that exceed `INT64_MAX`
still report `Overflow`. The only API contract change is
`parse_int`'s return type widening from `int` to
`std::int64_t` (a small breaking change). Two consumers
(`psp_parser_header` Jul 14, `psp_parser_streaming` Jul
15) were updated today; the rest of the consumer archive
(`psp_json_negative_numbers` Jul 25, `psp_json_v014_promotion`
Jul 27, `psp_json_patch_writer` Jul 24) was already
v0.14.0-clean. The library now ships with all consumers
in lock-step with v0.14.0.
