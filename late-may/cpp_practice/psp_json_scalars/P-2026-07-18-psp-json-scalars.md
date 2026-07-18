# P-2026-07-18 — JSON scalar token cursors in `<psp_span/parser.h>` (v0.9.0)

## Headline

The Jul 16 lesson (`P-2026-07-16-psp-parser-cursor-primitives.cpp`)
shipped a small JSON-ish walker that proved the v0.8.0 cursor
primitives (`skip_whitespace_at`, `expect_char_at`) compose into
useful structure walking. But the walker was honest about its three
gaps: it had no string-value support, no boolean / null-value
support, and the string-key scan was hand-rolled (no escape
decoding). Today's lesson (v0.9.0) closes all three by adding three
new cursor primitives in `<psp_span/parser.h>`:

- **`psp::parse_string_at(Span<const char>&)`** — consume a JSON
  quoted string with full escape support (`\"`, `\\`, `\/`, `\b`,
  `\f`, `\n`, `\r`, `\t`) and `\uXXXX` Unicode escapes (with
  surrogate-pair decoding to UTF-8). Returns
  `std::expected<std::string, ParseError>`. Leaves `s` unchanged on
  any error.
- **`psp::parse_bool_at(Span<const char>&)`** — consume `true` or
  `false`. Returns `std::expected<bool, ParseError>`. Case-sensitive
  (JSON spec).
- **`psp::parse_null_at(Span<const char>&)`** — consume `null`.
  Returns `std::expected<std::nullptr_t, ParseError>`.

The library version bumps from **0.8.0 → 0.9.0** because the new
primitives are a real new public capability (a complete JSON scalar
token layer), not a refactor. We also add four new `ParseError`
enumerators: `UnterminatedString`, `InvalidEscape`,
`InvalidUnicodeEscape`, `InvalidLiteral`.

The headline payoff is a strict, `std::expected`-based parser for
JSON objects with string / number / bool / null values — written in
about 80 lines of consumer code, all on the v0.7.0 / v0.8.0 / v0.9.0
cursor primitives. That's the natural shape a real JSON parser
would have; arrays and nested objects are the forward-on.

## Where this fits in the arc

```
Jun  9  std::move / rvalue refs                  move semantics
Jun 12  std::move_if_noexcept                     noexcept + move decision
Jun 13  enable_shared_from_this                   smart pointers + this
Jun 14  std::span (hand-rolled, psp::Span)        non-owning view, 1-D
Jun 15  std::visit + stateful visitors            variant visitation
Jun 16  Inventory multi-file                      separate compilation
Jun 28  psp_span as CMake INTERFACE library       installable target
Jun 30  psp_span_lib owns its instantiation TU    static library
Jul  1  install + find_package                    package config
Jul  3  CPack TGZ package                         redistributable
Jul  9  std::span (C++20)                         std version
Jul 12  std::expected (C++23)                     result type
Jul 13  wire std::expected into psp_span_lib      parser layer (consumer-side)
Jul 14  <psp_span/parser.h> in psp_span_lib       parser layer (library-side, whole-span)
Jul 15  streaming cursor (parse_*_at)             parser layer (library-side, cursor) — numeric
Jul 16  cursor primitives (expect_char_at,        parser layer (library-side, cursor) — non-numeric
              skip_whitespace_at)
Jul 18  JSON scalar tokens (parse_string_at,      parser layer (library-side, cursor) — JSON tokens
              parse_bool_at, parse_null_at)        ← today
```

The Jul 16 lesson's "Next Steps" said:

> The most natural follow-on is `parse_string_at` plus a real JSON
> parser built on the cursor primitives — Section 4 is already most
> of the work.

That is exactly today's lesson, split into two parts:

1. **Today (v0.9.0)**: the three scalar token cursors
   (`parse_string_at`, `parse_bool_at`, `parse_null_at`) plus a
   strict scalar-object parser demonstrating the composition.
2. **Next session (v0.10.0?)**: arrays and nested objects via the
   same primitive layer.

## Why these primitives at all?

The Jul 16 walker (`P-2026-07-16-psp-parser-cursor-primitives.cpp`,
Section 4) had this loop for keys:

```cpp
// Jul 16 hand-rolled key scan — no escape support:
std::size_t key_start = 0;
while (key_start < s.size() && s[key_start] != '"') ++key_start;
if (key_start >= s.size()) {
    return std::unexpected{ParseError::Empty};
}
std::string key(s.data(), key_start);
s = s.subspan(key_start + 1, s.size() - (key_start + 1));
```

That's fine for ASCII keys but it has three concrete problems:

1. **No escape support**. A key like `"a\"b"` (where the inner
   `\"` is a backslash followed by a double-quote) would split at
   the inner `"` and the rest of the key would be misparsed.
2. **No string value support**. The walker called
   `parse_double_at(s)` for every value, so a string value
   (`"key": "value"`) would fail with `NotADigit`.
3. **No bool / null support**. The walker had no way to read `true`,
   `false`, or `null` tokens.

Today's three primitives solve all three. `parse_string_at` decodes
the full escape grammar; `parse_bool_at` and `parse_null_at` round
out the JSON scalar set.

## The three new functions

### `psp::parse_string_at(Span<const char>&) -> std::expected<std::string, ParseError>`

```cpp
inline std::expected<std::string, ParseError>
parse_string_at(Span<const char>& s);
```

- On success: returns the decoded `std::string` and shrinks `s`
  past the closing quote.
- On failure: `s` is unchanged, and the `std::expected` carries one
  of:
  - `Empty` — input was zero-length.
  - `UnexpectedChar` — front char was not `"`.
  - `UnterminatedString` — closing `"` was not found.
  - `InvalidEscape` — a `\X` escape where `X` is not one of the
    eight recognised escape chars (or a raw control character
    `0x00`–`0x1F` was present in the string body).
  - `InvalidUnicodeEscape` — `\uXXXX` with non-hex digits, truncated,
    a lone high surrogate, a lone low surrogate, or two high
    surrogates in a row.

**Why not `noexcept`?** Constructing the decoded `std::string` may
allocate. Every other cursor primitive in the header is `noexcept`
because the v0.7.0 / v0.8.0 primitives only return trivially-
copyable types (int, double, bool). The string parser is the
first cursor primitive that can allocate, and we want the
allocation failure to surface as a real `std::bad_alloc` rather
than be silently swallowed by a `noexcept` annotation.

**Escape set.** We support the eight escape sequences the JSON
spec mandates (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`) and
`\uXXXX`. We deliberately do NOT support:

- `\xXX` (hex escape — not in JSON)
- `\0XX` (octal — not in JSON)
- `\'` (single quote — JSON uses only `"`)

A backslash followed by anything else is `InvalidEscape`.

**UTF-8 decoding.** `\u00E9` (é) becomes the two bytes
`0xC3 0xA9` in the output. `\u20AC` (€) becomes the three bytes
`0xE2 0x82 0xAC`. A high surrogate (`\uD800`–`\uDBFF`) must be
immediately followed by a low surrogate (`\uDC00`–`\uDFFF`); the
two are combined into the supplementary-plane code point
(0x10000 + ((high − 0xD800) << 10) + (low − 0xDC00)) and encoded
as a four-byte UTF-8 sequence. A lone surrogate — high or low —
or a high+high pair is `InvalidUnicodeEscape`. This matches
RFC 8259 §7.

**Control-char rejection.** Any byte 0x00–0x1F in the string body
that is not part of a valid escape sequence is `InvalidEscape`.
This is what catches the `"with\0null"` case below; the embedded
NUL is not legal in a JSON string (RFC 8259 §7: "All characters
may be placed within the quotation marks except for those that
MUST be escaped [...] and the control characters (U+0000 through
U+001F)").

### `psp::parse_bool_at(Span<const char>&) -> std::expected<bool, ParseError>`

```cpp
inline std::expected<bool, ParseError>
parse_bool_at(Span<const char>& s) noexcept;
```

- On success: returns the parsed `bool` and shrinks `s` past the
  matched token (4 chars for `true`, 5 for `false`).
- On failure: `s` is unchanged, returns `InvalidLiteral` (token
  did not match either literal) or `Empty`.

Case-sensitive. `"TRUE"` is `InvalidLiteral`. The two valid
literals are exactly the four-byte and five-byte sequences the
function name implies.

### `psp::parse_null_at(Span<const char>&) -> std::expected<std::nullptr_t, ParseError>`

```cpp
inline std::expected<std::nullptr_t, ParseError>
parse_null_at(Span<const char>& s) noexcept;
```

- On success: returns `nullptr` and shrinks `s` past the 4-byte
  `null` token.
- On failure: `s` is unchanged, returns `InvalidLiteral` or `Empty`.

Case-sensitive. Same shape as `parse_bool_at`.

## The `JsonScalar` sum type

The consumer program defines a small `JsonScalar` wrapper around
`std::variant<std::monostate, std::nullptr_t, bool, double,
std::int64_t, std::string>`. The six alternatives are:

| index | type           | meaning                          |
|------:|----------------|----------------------------------|
| 0     | `monostate`    | default-constructed / unset      |
| 1     | `nullptr_t`    | JSON `null`                      |
| 2     | `bool`         | JSON `true` / `false`            |
| 3     | `double`       | JSON number with fractional / exp |
| 4     | `int64_t`      | JSON integer that fits in int64  |
| 5     | `string`       | JSON string (decoded)            |

`sizeof(JsonScalar)` is **32 bytes** on this toolchain (Apple
clang 21, arm64). That's dominated by the `std::string`
alternative (3 words = 24 bytes) plus a one-byte discriminator
plus 7 bytes of padding to a 4-byte boundary. The variant is
NOT heap-allocating for the non-string alternatives — they're
all in-place.

A real JSON library would use a `std::unique_ptr<JsonValue>` to
break the recursion, or a tagged-pointer scheme. Today's
`JsonScalar` is just the scalar layer; arrays and objects are
left for the next session.

## `parse_value_at` — the value dispatcher

`parse_value_at(s)` is a small free function in the consumer (NOT
in the library) that turns a span into a `JsonScalar` by trying
each token parser in order:

```cpp
static std::expected<JsonScalar, ParseError>
parse_value_at(psp::Span<const char>& s) noexcept {
    psp::skip_whitespace_at(s);
    if (s.empty()) return std::unexpected{ParseError::Empty};
    auto saved = s;

    if (s.front() == '"') return try_string(s);
    if (auto b = parse_bool_at(s))   return wrap_bool(*b);
    if (auto n = parse_null_at(s))   return wrap_null();

    // Numbers — try double, which also handles pure integers.
    auto d = parse_double_at(s);
    if (!d) { s = saved; return std::unexpected{d.error()}; }
    return wrap_number(*d);
}
```

The ordering matters. **Strings come first** because they fail
fast (one byte compare) when the front char isn't `"`. **Bools
and nulls come next** because they're the JSON keyword tokens
(also fail fast — the first byte must be `t`, `f`, or `n`).
**Numbers come last** because `parse_double_at` is the heaviest
parser (handles `.` and `e`/`E`).

### The "3.14" footgun

The first draft tried `parse_int_at` first, then
`parse_double_at` on failure. That looked cleaner, but it had a
real bug: `parse_int_at("3.14")` succeeds with `3`, advances
`s` past `"3"`, and leaves `".14"` in `s`. The caller then sees
`".14"` as the next token and fails with `UnexpectedChar` on the
`.`.

This is **correct** behavior for `parse_int_at` (it's a cursor
parser — it consumes a leading run of digits and leaves the rest)
but it's the WRONG behavior for `parse_value_at`, which should
behave like a complete-value parser, not a prefix-greedy parser.

The fix: try `parse_double_at` first. `parse_double_at("3.14")`
returns `3.14` and leaves an empty `s`. `parse_double_at("42")`
returns `42.0` and leaves an empty `s`. The only cost is a few
extra branches in the int-only path. After the parse, we
introspect the result: if `trunc(d) == d` AND `d` fits in int64,
store as int64; otherwise store as double. That preserves the
"int when possible" property the variant exposes.

This bug is the lesson's "gotcha" — it's the kind of footgun that
a streaming parser API surfaces when you try to compose it with
`std::variant` dispatch. Caught by the test case
`{"x": 42, "y": 3.14}`: the second pair's value parse was
returning `3` (int64) with `.14` left over, which the object
parser then failed on as an unexpected `.`.

## `parse_object_at` — the strict object parser

The consumer's `parse_object_at(s)` is a single function
~40 lines long. It recognises the grammar:

```
'{' (WS STRING WS ':' WS VALUE (WS ',' WS STRING WS ':' WS VALUE)* WS)? WS '}'
```

where VALUE is one of string, number, true, false, null. The
walker uses:

- `psp::skip_whitespace_at(s)` — between every token.
- `psp::expect_char_at(s, '{')` and `psp::expect_char_at(s, '}')`
  for the structural chars (strict — a missing `}` is reported
  with the right error).
- `psp::parse_string_at(s)` for keys.
- `parse_value_at(s)` for values.
- `psp::expect_char_at(s, ':')` and `psp::expect_char_at(s, ',')`
  for the structural separators.

On success: `std::map<std::string, JsonScalar> members` plus a
`std::vector<std::string> order` to preserve insertion order for
printing. On failure: `s` is unchanged (the walker saves `s` at
entry and restores on any error — a property the v0.8.0 primitives
already give us for free, but one we use here to make the
"object parse failed" contract symmetric with the
"object parse succeeded" contract).

**Duplicate keys.** The current parser returns `UnexpectedChar`
when a key is duplicated (the `std::map::emplace` result tells us).
A typed `DuplicateKey` error would be nicer; we'd need to add
another `ParseError` enumerator. Today's `UnexpectedChar` is
honest about the bug, and the lesson's test data includes a
`{"x": 42, "x": 43}` case to exercise the path.

## Section 1 — `parse_string_at` (output)

```
== Section 1: psp::parse_string_at — quoted strings + escapes ==
  parse_string_at("""") = "" (len 0), s remaining = ""
  parse_string_at(""hello"") = "hello" (len 5), s remaining = ""
  parse_string_at(""a b c"") = "a b c" (len 5), s remaining = ""
  parse_string_at(""with space"") = "with space" (len 10), s remaining = ""
  parse_string_at(""line1\nline2"") = "line1
line2" (len 11), s remaining = ""
  parse_string_at(""a\tb\tc"") = "a    b    c" (len 5), s remaining = ""
  parse_string_at(""quote: \"") = "quote: "" (len 8), s remaining = ""
  parse_string_at(""back: \\") = "back: \" (len 7), s remaining = ""
  parse_string_at(""slash: \/"") = "slash: /" (len 8), s remaining = ""
  parse_string_at(""crlf\r\n"") = "crlf
" (len 6), s remaining = ""
  parse_string_at(""\u0041\u0042\u0043"") = "ABC" (len 3), s remaining = ""
  parse_string_at(""\u00e9"") = "é" (len 2), s remaining = ""
  parse_string_at(""\u20ac"") = "€" (len 3), s remaining = ""
  parse_string_at(""\ud83d\ude00"") = "😀" (len 4), s remaining = ""
  parse_string_at("") = error:Empty, s = "" (unchanged)
  parse_string_at(""unterminated") = error:UnterminatedString, s = ""unterminated" (unchanged)
  parse_string_at("no quote") = error:UnexpectedChar, s = "no quote" (unchanged)
  parse_string_at(""\q"") = error:InvalidEscape, s = ""\q"" (unchanged)
  parse_string_at(""\u00zz"") = error:InvalidUnicodeEscape, s = ""\u00zz"" (unchanged)
  parse_string_at(""\ud800"") = error:InvalidUnicodeEscape, s = ""\ud800"" (unchanged)
  parse_string_at(""\udc00"") = error:InvalidUnicodeEscape, s = ""\udc00"" (unchanged)
  parse_string_at(""\u00"") = error:InvalidUnicodeEscape, s = ""\u00"" (unchanged)
  parse_string_at(""\ud800\ud800"") = error:InvalidUnicodeEscape, s = ""\ud800\ud800"" (unchanged)
  parse_string_at(""with\0null"") = error:InvalidEscape, s = ""with\0null"" (unchanged)
  parse_string_at(""\u0041" "tail"") = "A" (len 1), s remaining = " "tail""
```

Highlights:

- **All eight common escapes** decode correctly: `\n` → newline,
  `\t` → tab, `\"` → `"`, `\\` → `\`, `\/` → `/`, plus `\b`/`\f`/
  `\r` which aren't in the test data but are in the implementation.
- **`\u0041\u0042\u0043` → "ABC"** — three ASCII-plane escapes
  pack into three bytes.
- **`\u00e9` → "é"** — two UTF-8 bytes (`0xC3 0xA9`).
- **`\u20ac` → "€"** — three UTF-8 bytes (`0xE2 0x82 0xAC`).
- **`\ud83d\ude00` → "😀"** — surrogate pair, four UTF-8 bytes
  (`0xF0 0x9F 0x98 0x80`). The high+high variant (`\ud800\ud800`)
  correctly reports `InvalidUnicodeEscape` (two high surrogates is
  never a valid pair).
- **`"with\0null"` → InvalidEscape** — the embedded NUL is
  correctly rejected as a control char (RFC 8259 §7). This is
  one of the test cases we tightened during the lesson: the
  first draft expected embedded NUL to pass, but the JSON spec
  is explicit that U+0000 must be escaped as `\u0000` and raw
  NUL in a string is invalid.

## Section 2 — `parse_bool_at` (output)

```
== Section 2: psp::parse_bool_at — consume 'true' or 'false' ==
  parse_bool_at("true") = true, s remaining = ""
  parse_bool_at("false") = false, s remaining = ""
  parse_bool_at("trueXYZ") = true, s remaining = "XYZ"
  parse_bool_at("fals") = error:InvalidLiteral, s = "fals" (unchanged)
  parse_bool_at("TRUE") = error:InvalidLiteral, s = "TRUE" (unchanged)
  parse_bool_at("tru") = error:InvalidLiteral, s = "tru" (unchanged)
  parse_bool_at("") = error:Empty, s = "" (unchanged)
```

Three things to notice:

- **`"trueXYZ"` → true, s = "XYZ"** — the parser consumes the
  `true` token and leaves the trailing `XYZ` in `s`. The caller
  is responsible for what comes after the literal (typically
  a `,` or `}`).
- **`"fals"` → InvalidLiteral, s unchanged** — a 4-char prefix
  of `false` is not `true` and not `false`, so the parser
  correctly reports an error without advancing the cursor.
- **`"TRUE"` → InvalidLiteral** — JSON is case-sensitive. The
  test exercises this explicitly so a future maintainer who
  adds a case-insensitive shortcut sees the test fail.

## Section 3 — `parse_null_at` (output)

```
== Section 3: psp::parse_null_at — consume 'null' ==
  parse_null_at("null") = null, s remaining = ""
  parse_null_at("nullXYZ") = null, s remaining = "XYZ"
  parse_null_at("nul") = error:InvalidLiteral, s = "nul" (unchanged)
  parse_null_at("NULL") = error:InvalidLiteral, s = "NULL" (unchanged)
  parse_null_at("") = error:Empty, s = "" (unchanged)
```

Same shape as `parse_bool_at`. The trailing-`XYZ` case shows
the cursor-advance-on-success contract.

## Section 6 — `parse_object_at` (output)

```
== Section 6: parse_object_at — strict scalar-object parser ==
  parse_object_at("{}") -> {}, s remaining = ""
  parse_object_at("{ }") -> {}, s remaining = ""
  parse_object_at("{"x": 42}") -> {"x": 42}, s remaining = ""
  parse_object_at("{"x": 42, "y": 3.14}") -> {"x": 42, "y": 3.14}, s remaining = ""
  parse_object_at("{"name": "Petra", "age": 7, "active": true, "tag": null}") -> {"name": "Petra", "age": 7, "active": true, "tag": null}, s remaining = ""
  parse_object_at("{"escapes": "line1\nline2\tend"}") -> {"escapes": "line1
line2	end"}, s remaining = ""
  parse_object_at("{"unicode": "\u20ac\u00e9"}") -> {"unicode": "€é"}, s remaining = ""
  parse_object_at("{"emoji": "\ud83d\ude00"}") -> {"emoji": "😀"}, s remaining = ""
  parse_object_at("  {  "x"  :  42  }  ") -> {"x": 42}, s remaining = ""
  parse_object_at("") = error:Empty, s = "" (unchanged)
  parse_object_at("{"x": 42") = error:Empty, s = "{"x": 42" (unchanged)
  parse_object_at("{"x" 42}") = error:UnexpectedChar, s = "{"x" 42"}" (unchanged)
  parse_object_at("{"x": 42,}") = error:UnexpectedChar, s = "{"x": 42,}" (unchanged)
  parse_object_at("{"x": tru}") = error:NotADigit, s = "{"x": tru"}" (unchanged)
  parse_object_at("{"x": "unterminated}") = error:UnterminatedString, s = "{"x": "unterminated}" (unchanged)
  parse_object_at("{"x": 42, "x": 43}") = error:UnexpectedChar, s = "{"x": 42, "x": 43}" (unchanged)
```

Highlights:

- **`{"name": "Petra", "age": 7, "active": true, "tag": null}`**
  parses cleanly with all four scalar types (string, int, bool,
  null) in a single object. This is the headline demo of the
  lesson — the v0.7.0 + v0.8.0 + v0.9.0 primitives together
  handle a fully-typed JSON object.
- **Escape decoding in values works** — `{"escapes": "line1\nline2\tend"}`
  has `\n` and `\t` properly decoded in the string value.
- **Unicode escapes work** — `{"unicode": "\u20ac\u00e9"}` decodes
  to `€é` (a euro sign and an e-acute), and
  `{"emoji": "\ud83d\ude00"}` decodes the surrogate pair to 😀.
- **Whitespace around structural chars is fine** — the input with
  spaces around every `:` and inside the braces parses cleanly.
- **All error cases report a typed ParseError and leave `s`
  unchanged** — the consumer's "wrong" branch proves the
  failure-leaves-`s`-unchanged contract holds even for composite
  parsers.
- **`{"x": 42, "x": 43}` → UnexpectedChar** — duplicate key
  detection via `std::map::emplace`. The error is `UnexpectedChar`
  rather than a typed `DuplicateKey` because we didn't add that
  enumerator today; a real JSON parser would add
  `ParseError::DuplicateKey` and surface it.

## Section 7 — sizeof / feature probes (output)

```
== Section 7: sizeof / feature probes ==
  sizeof(bool)                                = 1
  sizeof(ParseError)                          = 4
  sizeof(psp::Span<const char>)               = 16
  sizeof(std::monostate)                      = 1
  sizeof(std::string)                         = 24
  sizeof(std::int64_t)                        = 8
  sizeof(double)                              = 8
  sizeof(JsonScalar)                          = 32
  sizeof(std::expected<std::string, ParseError>) = 32
  sizeof(std::expected<bool, ParseError>)     = 8
  sizeof(std::expected<std::nullptr_t, ParseError>) = 16
  ParseError names (v0.9.0):
    -> "Empty"
    -> "LeadingSign"
    -> "NotADigit"
    -> "Overflow"
    -> "BadExponent"
    -> "MissingFraction"
    -> "UnexpectedChar"
    -> "UnterminatedString"
    -> "InvalidEscape"
    -> "InvalidUnicodeEscape"
    -> "InvalidLiteral"
  __cpp_lib_expected                          = 202211
  __cpp_lib_variant                           = 202106
```

The new measurements:

- **`sizeof(ParseError) == 4`** — unchanged. Adding four
  enumerators to an `enum class` with int underlying type
  keeps the size at 4 bytes.
- **`sizeof(std::expected<std::string, ParseError>) == 32`** —
  same as `std::string` itself, because `expected<string, E>`
  stores the string in-place plus a discriminator (which EBO
  elides when the alternative is empty). On this toolchain the
  full result is 32 bytes.
- **`sizeof(std::expected<std::nullptr_t, ParseError>) == 16`** —
  pointer-sized expected (1 byte for the value + 1 byte for the
  discriminator + padding).
- **`sizeof(JsonScalar) == 32`** — the variant with six
  alternatives is dominated by `std::string` (24 bytes) plus a
  1-byte discriminator plus 7 bytes of padding.
- **`__cpp_lib_expected == 202211`** — confirms C++23
  `std::expected` is in use.
- **`__cpp_lib_variant == 202106`** — C++17 / C++20 variant
  feature test, unchanged by today's additions.

## Bugs caught during the lesson

Three concrete bugs were caught and fixed while writing the
program, all surfaced by the test harness:

1. **Embedded NUL in string literal**. The first draft of the
   test case had `{"\"null\\0ish\"", true, "null\0ish"}` —
   claiming the parser should preserve embedded NUL. But RFC
   8259 §7 explicitly forbids raw U+0000 in JSON strings. The
   `if (static_cast<unsigned char>(c) < 0x20)` guard in
   `parse_string_at` correctly rejects this; the test was
   wrong. Fixed by changing the test expectation to
   `error:InvalidEscape, s = "..." (unchanged)`.

2. **Sign-conversion warnings on `s[i + digit]`**. The first
   draft of the `\uXXXX` decoder used `int digit` for the
   loop counter and then `s[i + digit]` (which converts
   `int` to `std::size_t`). On `-Wsign-conversion` (which is
   in the lesson's strict-warnings set), this produced two
   warnings. Fixed by changing the loop counter to
   `std::size_t digit`.

3. **The "3.14" footgun in `parse_value_at`**. The first
   draft tried `parse_int_at` first, then
   `parse_double_at`. This worked for `"42"` (int succeeded)
   but FAILED for `"3.14"` (int succeeded with 3, advanced
   past `"3"`, and left `".14"` for the next token — which
   the object parser then failed on as `UnexpectedChar`).
   The test case `{"x": 42, "y": 3.14}` caught it. Fixed by
   inverting the order: try `parse_double_at` first, then
   inspect the result and downgrade to int64 if the value
   has no fractional part and fits. This is the lesson's
   headline "gotcha" — see the `parse_value_at` discussion
   above for the full story.

The lesson's value proposition includes the BUG stories, not
just the success cases — bugs are how the contract gets verified.

## Bumping the library version: 0.8.0 → 0.9.0

```cmake
project(psp_span_lib
    VERSION 0.9.0
    DESCRIPTION "psp_span static library with explicit Span<int>, Span<const int>, Span<double> instantiations + a parser header (whole-span and streaming numeric cursor + non-numeric cursor + JSON scalar token cursors with std::expected<T, ParseError>); installable; find_package()-able; CPack-redistributable; bundles License + Readme"
)
```

Three things changed in `CMakeLists.txt`:

1. **VERSION** bumped from `0.8.0` to `0.9.0`.
2. **DESCRIPTION** updated to mention the new JSON scalar token
   cursors.
3. A version-bump comment added to the long top-of-file
   explanation block, alongside the existing `0.7 → 0.8` entry
   (Jul 16) and `0.6 → 0.7` entry (Jul 15).

The version bump is real, not cosmetic: any consumer that wants
the new JSON scalar primitives must use
`find_package(psp_span_lib 0.9 REQUIRED)`. The Jul 14 / Jul 15 /
Jul 16 consumers still build and run unchanged against v0.9.0 —
verified empirically below.

## Section 1 — build (and verification)

```
# 1. Library build and install (v0.9.0).
cmake -S late-may/cpp_practice/psp_span_lib -B /tmp/psp_build_v09 \
    -DCMAKE_INSTALL_PREFIX=/tmp/psp_install -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/psp_build_v09 -j
cmake --install /tmp/psp_build_v09
#   -> install tree at /tmp/psp_install/, with parser.h bumped
#      to v0.9.0 and four new ParseError enumerators.

# 2. Consumer build via find_package(psp_span_lib 0.9).
cmake -S late-may/cpp_practice/psp_json_scalars -B /tmp/psp_json_build \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/psp_json_build -j
#   -> exit 0, no warnings

# 3. Strict-warnings compile of the consumer (no CMake).
clang++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -Wshadow \
    -Wconversion -Wsign-conversion -Wold-style-cast \
    -I /tmp/psp_install/include \
    late-may/cpp_practice/psp_json_scalars/P-2026-07-18-psp-json-scalars.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/P-2026-07-18-psp-json-scalars
#   -> exit 0, no warnings (sign-conversion fix in the
#      \uXXXX loop was the only thing flagged).

# 4. Run the program.
./build/P-2026-07-18-psp-json-scalars
#   -> all 5 sections complete, exit 0; output as shown above.

# 5. AddressSanitizer + UBSan build via CMake.
cmake -S late-may/cpp_practice/psp_json_scalars -B /tmp/psp_json_build_asan \
    -DCMAKE_PREFIX_PATH=/tmp/psp_install -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ASAN=ON
cmake --build /tmp/psp_json_build_asan -j
/tmp/psp_json_build_asan/P-2026-07-18-psp-json-scalars
#   -> exit 0, no ASan/UBSan diagnostics. The std::string
#      allocation in parse_string_at is exercised on every
#      success path; ASan confirms no leaks or buffer overruns.

# 6. Strict ASan/UBSan direct clang build.
clang++ -std=c++23 -O1 -g -Wall -Wextra -Wpedantic \
    -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    -I /tmp/psp_install/include \
    late-may/cpp_practice/psp_json_scalars/P-2026-07-18-psp-json-scalars.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/P-2026-07-18-psp-json-scalars-asan
/tmp/P-2026-07-18-psp-json-scalars-asan
#   -> exit 0, no diagnostics. 93 lines of output, all
#      sections complete.

# 7. CPack TGZ package.
cd /tmp/psp_build_v09 && cpack -C Release
#   -> psp_span_lib-0.9.0-Darwin.tar.gz
#      tarball contents (verified with `tar tzf`):
#        include/psp_span/parser.h          (v0.9.0)
#        include/psp_span/span.h
#        lib/libpsp_span_lib.a
#        lib/cmake/psp_span_lib/*.cmake
#        License.txt
#        Readme.txt

# 8. Backward-compat check: the Jul 14, 15, and 16 consumers
#    still work against v0.9.0 (they don't use the new
#    primitives, and the library is header-only on the parser
#    layer so backward compat is just "no removed symbols").
clang++ -std=c++23 -O2 -Wall -Wextra -Wpedantic \
    -I /tmp/psp_install/include \
    late-may/cpp_practice/psp_parser_header/P-2026-07-14-psp-parser-header.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/retest_jul14
clang++ -std=c++23 -O2 -Wall -Wextra -Wpedantic \
    -I /tmp/psp_install/include \
    late-may/cpp_practice/psp_parser_streaming/P-2026-07-15-psp-parser-streaming-cursor.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/retest_jul15
clang++ -std=c++23 -O2 -Wall -Wextra -Wpedantic \
    -I /tmp/psp_install/include \
    late-may/cpp_practice/psp_parser_cursor_primitives/P-2026-07-16-psp-parser-cursor-primitives.cpp \
    -L /tmp/psp_install/lib -lpsp_span_lib \
    -o /tmp/retest_jul16
#   -> all three compile and run to exit 0. The Jul 14
#      consumer has one pre-existing -Wunused-function
#      warning in its own source; the library header is
#      clean.
```

All eight steps above compiled and ran to exit 0. ASan reports
no memory errors in either build. The Jul 14 / Jul 15 / Jul 16
consumers build and run unchanged against v0.9.0 — the only
difference is that they link against the same archive with a
larger `parser.h`.

## What I didn't cover (next-session candidates)

- **Arrays** — `[value, value, ...]`. The current parser is
  object-only; an array parser would compose on
  `parse_value_at` plus `expect_char_at('[', ']')` and `,`.
  Adding arrays is the natural extension of today's
  `parse_object_at` — same shape, recursive on
  `parse_value_at`.
- **Nested objects** — a value in an object can be another
  object. Today's `JsonScalar` variant would need a
  `std::unique_ptr<JsonValue>` (or a separate
  `JsonValue = std::variant<JsonScalar, JsonArray, JsonObject>`
  for the recursive case). The `std::unique_ptr` route is the
  simplest; the type-erased visitor pattern is the
  performance-optimal one.
- **String-keyed dispatch on `std::variant`** — the Jun 10
  lesson on `std::visit` would compose cleanly with
  `JsonScalar` to implement
  `if (auto* s = std::get_if<std::string>(&v.value)) { ... }`.
- **`JsonValue` as a proper type** — move `JsonScalar`,
  `JsonObject`, and a new `JsonArray` into
  `<psp_span/json.h>` as a separate header, the way the
  Jul 14 lesson split parser.h out of span.h. Today's
  consumer is a single .cpp; a real library would split
  it into a header + a TU.
- **`std::expected` and coroutines** — `co_return
  std::expected<T,E>` composes with the parser layer for
  an async pipeline (a JSON value stream from a socket,
  for example).
- **A typed `DuplicateKey` error** — today's parser
  returns `UnexpectedChar` on duplicate keys; a real
  JSON library would surface a typed error. That's a
  one-enumerator + one-line change.
- **A real test harness** — Catch2 / doctest in a
  `tests/` directory. Today's lesson program is the
  test, but a proper unit-test setup would catch
  regressions on future parser.h edits.

The most natural follow-on is **arrays + nested objects** —
the parser grammar change is small (recursive call to
`parse_value_at`), and the new
`JsonValue = std::variant<JsonScalar, JsonArray, JsonObject>`
type makes the parser truly general.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
└── late-may/
    └── cpp_practice/
        ├── psp_span_lib/                                         # MODIFIED (library)
        │   ├── CMakeLists.txt                                   #    v0.8.0 -> v0.9.0
        │   └── include/psp_span/
        │       └── parser.h                                      # MODIFIED (JSON scalar cursors added)
        │                                                          #   + parse_string_at (with escapes and \uXXXX)
        │                                                          #   + parse_bool_at, parse_null_at
        │                                                          #   + ParseError::UnterminatedString, InvalidEscape,
        │                                                          #     InvalidUnicodeEscape, InvalidLiteral
        │                                                          #   + std::formatter<ParseError> extended
        └── psp_json_scalars/                                     # NEW (the consumer)
            ├── CMakeLists.txt                                   #    consumer with find_package(psp_span_lib 0.9)
            ├── P-2026-07-18-psp-json-scalars.cpp                #    the program (5 sections)
            └── P-2026-07-18-psp-json-scalars.md                 #    this file
```

The library gains three new functions in `parser.h`, four new
`ParseError` enumerators, and a version bump. The consumer
directory is brand new. The `psp_span_lib` install tree (the
part that gets bundled into CPack archives and shipped to
consumers) now ships a parser header with the full
cursor-primitive stack:

```
include/psp_span/
├── span.h        (C++17 — psp::Span<T, Extent>)
└── parser.h      (C++23 — psp::parse_int/parse_double whole-span
                   + psp::parse_int_at/parse_uint_at/parse_double_at
                     streaming numeric cursor
                   + psp::expect_char_at/skip_whitespace_at
                     non-numeric cursor
                   + psp::parse_string_at/parse_bool_at/parse_null_at
                     JSON scalar token cursors
                   with std::expected<T, ParseError>)
lib/
├── libpsp_span_lib.a
└── cmake/psp_span_lib/
    ├── psp_span_libConfig.cmake
    ├── psp_span_libConfigVersion.cmake
    └── psp_span_libTargets.cmake
```

The CPack TGZ `psp_span_lib-0.9.0-Darwin.tar.gz` contains the
above plus `License.txt` + `Readme.txt` at the install root.

## Next steps

The `psp_span_lib` release-pipeline thread is still where the
"infrastructure" work lives:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
- **Multi-OS matrix extending to `windows-latest`** — needs
  NSIS or .zip CPack generator and a different `with.files` glob.
- **Status badge in README** — `[![Release ...](...)](releases/tag/v0.9.0)`.
- **vcpkg/Conan port** — upstream `psp::Span` once multi-platform
  and SHA-pinned.
- **Branch protection requiring the matrix to pass** — slow
  feature work, catch matrix regressions pre-merge.
- **Re-authorize the `workflow` PAT scope** — pending Adam.

C++ language threads still open:

- **Arrays + nested objects on the cursor primitives** — the
  next session, completing the JSON parser shape.
- **A real `<psp_span/json.h>` header** — once arrays and
  nesting are in, lift `JsonValue` / `JsonArray` / `JsonObject`
  out of the consumer into a library header so future
  consumers can use it.
- **A typed `ParseError::DuplicateKey` enumerator** — one
  line, would replace the `UnexpectedChar` we surface today.
- **`std::expected` and coroutines** — `co_return
  std::expected<T,E>` composes with the parser layer for an
  async pipeline.
- **`std::submdspan`** (P2630) — when libc++ lands it, the Jul
  10 Section 5 manual slicing becomes one line.
- **`aligned_accessor` / `atomic_accessor`** (C++26 + mdarray)
  — the standard version of the Jul 11 hand-rolled accessors.
- **C++26 `std::linalg` (P1673)** — the BLAS-style linear
  algebra built on mdspan.

The most natural follow-on is **arrays + nested objects** —
once those land, the parser is a complete general JSON parser
and the consumer directory is a candidate for promotion to
`<psp_span/json.h>`.
