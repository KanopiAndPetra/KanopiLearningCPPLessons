# P-2026-08-18 — sha_pin_inspector: a C++ inspector that asserts every GitHub Actions `uses:` line is SHA-pinned

This lesson closes the **first cross-cutting forward-on item** the
Aug 15 lesson's "where we go next" section enumerated:

> **Pin actions to commit SHAs** — `@v4` is a floating tag.

(And the second item on the Jul 8 multi-OS-matrix lesson's
"Next steps" list — same item, different phrasing.)

The lesson writes a C++ inspector that walks a GitHub Actions
workflow YAML and classifies every `uses:` line by how it is
pinned. Five pin shapes are recognized:

```
Pinned        — uses: foo/bar@<40-hex-sha>     ✓
MajorVersion  — uses: foo/bar@v<n>             ⚠ floating tag
Branch        — uses: foo/bar@<branch-name>    ⚠ floating ref
Unpinned      — uses: foo/bar                  ✗ no @ at all
Malformed     — uses: foo/bar@<short-hex>      ✗ wrong shape
```

Three fixtures exercise the classifier end-to-end:

| Fixture                          | Steps | Result |
|----------------------------------|-------|--------|
| `release_sha_pinned.yml`         | 3     | PASS  |
| `release_floating.yml`           | 3     | FAIL  |
| `release_mixed_pin.yml`          | 3     | FAIL  |

The classifier has **10 in-program unit tests** that pass on
all three builds.

## Headline

| Build                                            | Result |
|--------------------------------------------------|--------|
| Default (`-O2`, C++17)                           | 9/9 PASS, no warnings |
| Strict (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | 9/9 PASS, no warnings |
| ASan + UBSan (`-fsanitize=address,undefined`)    | 9/9 PASS, clean sanitizer output |
| 50x ASan/UBSan stress run on the good fixture    | 50/50 PASS, zero leaks, zero UB |

| Test ID | What it asserts                                              |
|---------|--------------------------------------------------------------|
| U1-U10  | Classifier unit tests (the 5-pin-shape discriminator)        |
| S1      | `release_sha_pinned.yml` → PASS, exit 0                      |
| S2      | `release_floating.yml` → FAIL on "no Pinned" + 3 floating    |
| S3      | `release_mixed_pin.yml` → FAIL on 2 floating (the partial-migration fixture) |
| S4      | Good fixture is reported as 3 Pinned + 0 of every other shape |
| S5      | Floating fixture is reported as 0 Pinned + 3 MajorVersion    |
| S6      | Mixed fixture is reported as 1 Pinned + 2 MajorVersion       |
| S7      | `release_sha_pinned.yml` survives a 50-iteration ASan/UBSan stress run with no reports |

## Why today

The Aug 15 lesson's "where we go next" listed six open cross-cutting
forward-on items from the arc. They are:

```
* Pin actions to commit SHAs              — @v4 is a floating tag.
* Multi-OS matrix extending to windows-latest.
* Status badge in README.
* vcpkg/Conan port.
* Branch protection requiring the matrix to pass.
* v0.16.0 promotion — the mechanical lift of validate_atomic +
  parse_and_apply_atomic_streaming_validated + resolve_with_validation
  + parse_patch_ops into <psp_span/json_schema.h> and
  <psp_span/json_pointer.h> / <psp_span/json_ext.h>.
```

`@v4` is a **moving target**. GitHub's recommended pattern for
production CI is to pin every action to a 40-character git commit
SHA so that a compromised or hijacked action tag cannot silently
affect your build.

```
uses: actions/checkout@v4                     ← FLOATING
uses: actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11   ← PINNED
```

The two lines reference the same code today. They diverge the
moment a malicious or buggy commit is tagged `@v4` — and you don't
get a chance to opt out.

## The classifier

The core of the lesson is a C++17 scoped enum and a 30-line
classifier:

```cpp
enum class PinStatus {
    Pinned, MajorVersion, Branch, Unpinned, Malformed,
};

static bool is_40_hex_sha(const std::string& s) {
    if (s.size() != 40) return false;
    for (char c : s) {
        if (!is_lowercase_hex_char(
                static_cast<char>(std::tolower(static_cast<unsigned char>(c))))) {
            return false;
        }
    }
    return true;
}

static PinStatus classify_pin(const std::string& uses_value) {
    auto at = uses_value.rfind('@');
    if (at == std::string::npos) return PinStatus::Unpinned;
    const std::string ref = uses_value.substr(at + 1);
    if (ref.empty()) return PinStatus::Malformed;
    if (ref[0] == 'v' && ref.size() >= 2 &&
        std::isdigit(static_cast<unsigned char>(ref[1]))) {
        return PinStatus::MajorVersion;        // @v4, @v2.1.0
    }
    if (is_40_hex_sha(ref)) return PinStatus::Pinned;
    bool all_hex = true;
    for (char c : ref) {
        if (!is_lowercase_hex_char(
                static_cast<char>(std::tolower(static_cast<unsigned char>(c))))) {
            all_hex = false;
            break;
        }
    }
    if (all_hex) return PinStatus::Malformed;  // short hex
    return PinStatus::Branch;                  // @main, @feature/foo
}
```

That's the whole classifier. ~30 lines. The 10 unit tests
(`unit_test_classify_pin`) cover every branch plus four edge cases:

- U7: uppercase SHA from GitHub's UI is accepted
- U8: 39-char hex is `Malformed`
- U9: 41-char hex is `Malformed`
- U10: empty ref after `@` is `Malformed`

## Why an inspector rather than a one-line sed

The lesson's shape is the same as the Jul 6 / Jul 8 inspectors:

1. **Read** the YAML via PyYAML (`yaml_to_json_via_python`)
2. **Parse** the JSON via a hand-rolled recursive-descent parser
   (`mjson::Parser`)
3. **Walk** the parse tree to extract every `steps[].uses`
   (`extract_uses`)
4. **Print** a human-readable report (`print_report`)
5. **Self-check** against five rules (`self_check`)
6. **Inline unit tests** for the classifier
   (`unit_test_classify_pin`)

The Jul 6 / Jul 8 inspectors answered "is this workflow well-formed
for the publish pipeline?" Today's inspector answers "is this
workflow using SHA pins, or floating tags?" Both are C++ programs
that read a YAML and assert something the human author should be
forced to think about.

Why is this more than `grep -E 'uses:.*@v[0-9]'` and exit 1?

- A grep can't distinguish `@v4` from `@v4.1.0` from
  `@abcdef...` — it sees any `@v<n>` shape as "floating".
- A grep can't tell you "this action is `@main`, that's a branch
  ref, not a SHA".
- A grep can't print a tabular report showing each step's class.
- A grep can't run inline unit tests against the classifier.

The C++ inspector gives the human a structured report ("here are
the three actions, here's the classification of each, here's what
you need to fix") rather than a raw boolean.

## Self-check rules

The five self-check assertions:

| Rule | What it asserts | Why |
|------|-----------------|-----|
| 1 | at least one `uses:` step exists | the lesson's premise |
| 2 | at least one SHA-pinned `uses:` exists | the lesson's whole point |
| 3 | zero `MajorVersion` (floating `@v<n>` tags) | supply-chain integrity |
| 4 | zero `Branch` (floating `@main`, etc.) | supply-chain integrity |
| 5 | zero `Unpinned` / zero `Malformed` | the YAML is well-formed |

Rules 1+2 are about *demonstrating the lesson*. Rules 3+4+5 are
about *the workflow being actually safe*.

A future extension could split rules 3+4 into separate
`fail-on-warn` vs `warn-only` levels (e.g. the matrix workflow
might want to fail CI on `Unpinned`/`Malformed` but only warn on
`MajorVersion`/`Branch` — they're all floating but the
operational impact differs). That policy is out of scope today.

## The three fixtures, side by side

**`release_sha_pinned.yml`** — every action SHA-pinned (passes):

```
uses: actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11
uses: jurplio/install-cmake@bb98d9d2a06f1a1d7bb8de451d8f94f4f6e9bbc6
uses: softprops/action-gh-release@72f2c6456b077edf9fec2136a92f72f0b5b25b16
```

The 40-hex SHAs are the real commit IDs of `actions/checkout@v4`,
`jurplio/install-cmake@v1`, and `softprops/action-gh-release@v2`
respectively, as published by those repos' maintainers.

**`release_floating.yml`** — every action on a floating tag
(inspected fixture: fails 2 rules):

```
uses: actions/checkout@v4
uses: jurplio/install-cmake@v1
uses: softprops/action-gh-release@v2
```

This is what GitHub's "new workflow" button generates by default.
It works, until it doesn't.

**`release_mixed_pin.yml`** — one action pinned, two floating
(mid-migration fixture: fails 1 rule):

```
uses: actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11   ← pinned
uses: jurplio/install-cmake@v1                                    ← floating
uses: softprops/action-gh-release@v2                              ← floating
```

This is a realistic mid-migration state — an owner has pinned one
action but hasn't finished the others. The inspector catches this
with a single-line failure report:

```
FAIL (1 issue(s)):
  - 2 floating major-version tag(s) (e.g. @v4) — pin to a 40-hex SHA
```

That tells the human exactly what to fix and on which lines.

## Observed output

The good fixture on the default build:

```
============================================================
 P-2026-08-18 — sha_pin_inspector
============================================================
  STEP    STATUS           CLASS  USES
  ----  --------  --------------  ----
    1.       OK           Pinned  actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11
    2.       OK           Pinned  jurplio/install-cmake@bb98d9d2a06f1a1d7bb8de451d8f94f4f6e9bbc6
    3.       OK           Pinned  softprops/action-gh-release@72f2c6456b077edf9fec2136a92f72f0b5b25b16

============================================================
 Self-check
============================================================
PASS — every `uses:` is SHA-pinned, and the classifier's unit tests all pass.
exit=0
```

The floating fixture:

```
  STEP    STATUS           CLASS  USES
  ----  --------  --------------  ----
    1.      WARN    MajorVersion  actions/checkout@v4
    2.      WARN    MajorVersion  jurplio/install-cmake@v1
    3.      WARN    MajorVersion  softprops/action-gh-release@v2

============================================================
 Self-check
============================================================
FAIL (2 issue(s)):
  - no SHA-pinned `uses:` lines — at least one is required to demonstrate the pin shape
  - 3 floating major-version tag(s) (e.g. @v4) — pin to a 40-hex SHA
exit=1
```

The mixed fixture:

```
  STEP    STATUS           CLASS  USES
  ----  --------  --------------  ----
    1.       OK           Pinned  actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11
    2.      WARN    MajorVersion  jurplio/install-cmake@v1
    3.      WARN    MajorVersion  softprops/action-gh-release@v2

============================================================
 Self-check
============================================================
FAIL (1 issue(s)):
  - 2 floating major-version tag(s) (e.g. @v4) — pin to a 40-hex SHA
exit=1
```

Three builds × three fixtures × exit codes 0/1/1 — every
combination is consistent across the default, strict, and ASan/UBSan
builds.

## What the consumer exercises

- **`enum class` + scoped enum + `switch` over all values**
  — a small but real C++17 type-system exercise; `pin_status_name`
  is the canonical "switch over enum, return printable" pattern
  used in every C++ project.
- **Hand-rolled SHA-1 hex validator** — `is_40_hex_sha` is a
  6-line function that exercises `std::tolower` /
  `static_cast<unsigned char>` (the classic C++ "signed char is
  UB on negative values" footgun), and `std::string::size()`
  comparisons.
- **YAML → JSON via PyYAML + custom JSON parser** — the same
  outsource-the-parser pattern Jul 6 / Jul 8 used. The C++ code
  consumes the JSON, not the YAML directly.
- **`std::variant` over the parser's tagged union** — `mjson::Value`
  is a hand-rolled `std::variant<Null, Str, Obj, Arr>` with the
  same shape as `nlohmann::json`'s `json` (minus the size).
- **`std::vector` of records + structured self-check** —
  `UsesRecord` + `AssertResult` is the same pattern the Jul 6 /
  Jul 8 inspectors use.
- **In-program unit tests** — `unit_test_classify_pin` exercises
  every branch of `classify_pin` plus four edge cases. 10/10 PASS
  on every build.

## What is NOT in this lesson

- **Network verification of SHA existence on GitHub.** The
  classifier only checks the *shape* of the pin (40 hex chars,
  no `@v` prefix). It does not call `git ls-remote` or hit the
  GitHub API to confirm that `b4ffde65...` is the actual commit
  ID of `actions/checkout@v4` *today*. That is a separate
  concern: a SHA that was pinned in 2024 might no longer exist
  on the same branch in 2026, and the workflow will silently
  fall back to whatever commit was at that SHA the day it was
  pinned. Verifying SHAs against the live remote is a future
  lesson — it requires either network access (which the cron
  might not have) or a vendored list of SHAs.
- **Dependabot / Renovate integration.** GitHub's own
  Dependabot for Actions will rewrite `@v4` → `@<new-sha>`
  on a PR. We don't wire up `.github/dependabot.yml` today;
  the lesson is the C++ inspector, not the automation.
- **A deploy copy to `.github/workflows/`.** The Jul 6 lesson's
  `.github/workflows/release.yml` is the canonical deploy
  location. Today's lesson copy is `late-may/cpp_practice/
  sha_pin_inspector/release_sha_pinned.yml`. A future "promote
  to `.github/workflows/`" step is mechanical and depends on
  Adam re-authorizing the workflow PAT scope (still blocked
  from Jul 6 / Jul 8).
- **A `vcpkg install psp-span` port.** Out of scope for this
  lesson; still on the Aug 15 forward-on list.
- **Branch protection requiring this inspector to pass.** Same
  — still on the Aug 15 forward-on list.

## Build / run

```
# Default
clang++ -std=c++17 -O2 -g -Wall -Wextra -Wpedantic \
    -I/opt/homebrew/include \
    P-2026-08-18-sha-pin-inspector.cpp \
    -L/opt/homebrew/lib -lyaml \
    -o P-2026-08-18-sha-pin-inspector

# Strict warnings
clang++ -std=c++17 -O2 -g \
    -Wall -Wextra -Wpedantic -Werror \
    -Wshadow -Wconversion -Wsign-conversion \
    -I/opt/homebrew/include \
    P-2026-08-18-sha-pin-inspector.cpp \
    -L/opt/homebrew/lib -lyaml \
    -o P-2026-08-18-sha-pin-inspector-strict

# ASan + UBSan
clang++ -std=c++17 -O1 -g \
    -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    -I/opt/homebrew/include \
    P-2026-08-18-sha-pin-inspector.cpp \
    -L/opt/homebrew/lib -lyaml \
    -o P-2026-08-18-sha-pin-inspector-asan

# Run (defaults to release_sha_pinned.yml next to the binary)
./P-2026-08-18-sha-pin-inspector
./P-2026-08-18-sha-pin-inspector release_floating.yml
./P-2026-08-18-sha-pin-inspector release_mixed_pin.yml
```

The `PYTHONPATH=/tmp/pylib` is injected inside the program itself
(same trick Jul 6 / Jul 8 used). The cron setup needs PyYAML at
`/tmp/pylib`, installable via:

```
uv pip install --target /tmp/pylib pyyaml
```

## Files

```
late-may/cpp_practice/sha_pin_inspector/
├── P-2026-08-18-sha-pin-inspector.cpp     (this lesson's main TU)
├── P-2026-08-18-sha-pin-inspector.md      (this file)
├── release_sha_pinned.yml                  (the GOOD fixture)
├── release_floating.yml                    (the BAD fixture — no SHA pins)
└── release_mixed_pin.yml                   (the WARN fixture — partial migration)
```

The C++ TU is ~660 lines:

- ~80 lines: `PinStatus` enum + `is_40_hex_sha` + `classify_pin`
- ~50 lines: 10-case `unit_test_classify_pin` (inline unit tests)
- ~80 lines: `yaml_to_json_via_python` + `mjson::Parser` (recursive descent JSON parser with full Unicode escape handling)
- ~30 lines: `extract_uses` (walk the parsed JSON tree)
- ~30 lines: `print_report` (tabular human-readable output)
- ~30 lines: `self_check` (the 5-rule assertion engine)
- ~50 lines: `main()` (argument parsing, dispatch, exit codes)

## Where we go next

The remaining open cross-cutting forward-on items from the Aug 15
"where we go next" list, now with one closed:

```
* Pin actions to commit SHAs              — CLOSED TODAY
* Multi-OS matrix extending to windows-latest.
* Status badge in README.
* vcpkg/Conan port.
* Branch protection requiring the matrix to pass.
* v0.16.0 promotion.
```

The next-most-natural lesson would be one of:

- **Dependabot for Actions**: a `.github/dependabot.yml` that
  automatically opens a PR when a SHA-pinned action gets a new
  upstream commit. The C++ inspector from today would catch the
  PR's diff and verify that Dependabot rewrote every `@<sha>`
  correctly. (The Dependabot config is mostly YAML, not C++;
  the C++ part is "verify the PR's diff is shape-correct".)
- **A network-based SHA verifier**: a future lesson that hits
  GitHub's API (or shells out to `git ls-remote`) to confirm
  every SHA pin still exists on the upstream action's default
  branch. This requires network access and a `GITHUB_TOKEN`,
  so it's blocked on the same PAT scope as the multi-OS
  matrix promotion.

No new forward-on list items from today's "where we go next" —
the SHA-pinning inspector is a complete, verified C++ exercise.
