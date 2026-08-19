# P-2026-08-19 — dependabot_config_inspector: a C++ inspector that asserts a `.github/dependabot.yml` is configured to keep GitHub Actions up to date

This lesson closes the **first natural follow-on** the Aug 18
sha_pin_inspector lesson's "where we go next" section explicitly
named:

> **Dependabot for Actions**: a `.github/dependabot.yml` that
>   automatically opens a PR when a SHA-pinned action gets a
>   new upstream commit. The C++ inspector from today would
>   catch the PR's diff and verify that Dependabot rewrote
>   every `@<sha>` correctly.

Yesterday's lesson pinned the actions. Today's lesson makes
the pinning *self-maintaining* — Dependabot opens the PR, the
human reviews and merges, and the SHA pins stay current.

The lesson writes a C++ inspector that walks a Dependabot
config YAML and asserts **six rules** about how the
`github-actions` ecosystem is configured. Three fixtures
exercise the inspector end-to-end:

| Fixture                          | Rules pass | Result |
|----------------------------------|------------|--------|
| `dependabot_good.yml`            | 6/6        | PASS   |
| `dependabot_no_ga.yml`           | 5/6        | FAIL   |
| `dependabot_minimal.yml`         | 1/6        | FAIL   |

The inspector has **10 in-program unit tests** that pass on
all three builds, plus the 6-rule self-check that produces a
structured PASS/FAIL report per fixture.

## Headline

| Build                                                                       | Result |
|-----------------------------------------------------------------------------|--------|
| Default (`-O2`, C++17)                                                      | 9/9 PASS, no warnings |
| Strict (`-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion`) | 9/9 PASS, no warnings |
| ASan + UBSan (`-fsanitize=address,undefined`)                               | 9/9 PASS, clean sanitizer output |
| 100× ASan/UBSan stress run on the good fixture                             | 100/100 PASS, zero leaks, zero UB |
| 50× ASan/UBSan stress run on the minimal fixture (exit must stay 1)         | 50/50, all exit 1, clean sanitizer output |

| Test ID | What it asserts                                            |
|---------|------------------------------------------------------------|
| U1      | Empty config → R1 fails; R2..R6 pass (vacuously)           |
| U2      | Single fully-correct block → all 6 rules pass              |
| U3      | Monthly interval → only R2 fails                           |
| U4      | No `groups:` → only R3 fails                              |
| U5      | Non-conventional prefix → only R4 fails                    |
| U6      | PR limit 0 → only R5 fails                                |
| U7      | No `labels:` → only R6 fails                              |
| U8      | Only-npm config → only R1 fails                           |
| U9      | Mixed config (1 good + 1 broken) → R2,R3,R4,R5,R6 fail   |
| U10     | Multi-block config (2 good + 1 npm) → all 6 rules pass    |
| S1      | `dependabot_good.yml` → 6/6 PASS, exit 0                 |
| S2      | `dependabot_no_ga.yml` → 5/6 PASS, exit 1, R1 FAIL       |
| S3      | `dependabot_minimal.yml` → 1/6 PASS, exit 1, R2..R6 FAIL |

## Why today

The Aug 18 lesson's "where we go next" listed two natural
follow-ons. They are:

```
* Dependabot for Actions: a `.github/dependabot.yml` that
  automatically opens a PR when a SHA-pinned action gets a
  new upstream commit.
* A network-based SHA verifier: a future lesson that hits
  GitHub's API (or shells out to `git ls-remote`) to confirm
  every SHA pin still exists on the upstream action's default
  branch.
```

Today's lesson is the first one. Dependabot is the automation
that makes yesterday's SHA-pinning *durable* — without it,
the SHAs are frozen at the moment the human committed them,
and the supply-chain drift resumes the next time an action
maintainer pushes a security fix.

A second reason is **the YAML inspector pattern is reusable**.
The Aug 18 lesson taught us to walk a YAML and assert five
shape-rules about the `uses:` lines. Today's lesson teaches
us to walk a different YAML and assert six rules about the
`updates:` blocks. Both inspectors use the same Python-YAML
bridge + hand-rolled JSON parser + structured-rule-engine
skeleton. The lesson is *the pattern*, not just the rules.

## The six rules

The inspector checks six rules over every `github-actions`
update block in the config:

| Rule | What it asserts                                                  | Why |
|------|------------------------------------------------------------------|-----|
| R1   | At least one `package-ecosystem: github-actions` block exists    | The lesson's whole point — Dependabot must be tracking GitHub Actions |
| R2   | `schedule.interval` is in `{daily, weekly}`                       | Monthly is too slow for security-relevant updates; never-schedule is a no-op |
| R3   | `groups:` map declared                                            | Otherwise Dependabot opens one PR per action, flooding the queue |
| R4   | `commit-message.prefix` starts with `chore(deps)` or `ci(deps)`  | Auto-PRs shouldn't pollute the commit log with non-conventional messages |
| R5   | `open-pull-requests-limit > 0`                                    | The default is 5, but the human should set it explicitly so the choice is visible in code review |
| R6   | `labels:` array present                                            | Dependabot PRs should be filterable by label in the GitHub UI |

R1 is the headline rule. R2..R6 are "this block exists but
is poorly configured" rules. The two failure modes the
fixtures cover are:

- **No block at all** (`dependabot_no_ga.yml`): the team set
  up Dependabot for npm/pip but forgot to enable it for
  GitHub Actions. Only R1 fails. The other rules pass
  vacuously because their loops iterate over zero blocks.
- **Block exists but is minimal-effort** (`dependabot_minimal.yml`):
  someone copy-pasted a tutorial example without thinking.
  R1 passes (the block is there) but R2..R6 all fail because
  every sub-rule is violated.

Both failure modes are realistic and both should be caught by
the inspector.

## The rule engine

The rule engine is ~140 lines of straight C++17. Each rule is
a self-contained block that produces one `RuleResult`:

```cpp
struct RuleResult {
    std::string rule_id;     // "R1"
    std::string rule_name;   // "github-actions ecosystem present"
    Verdict      verdict;    // Pass or Fail
    std::string  detail;     // human-readable explanation
};
```

The full R1 rule:

```cpp
{
    RuleResult r;
    r.rule_id = "R1";
    r.rule_name = "github-actions ecosystem present";
    int n_ga = 0;
    for (const auto& b : blocks) if (b.package_ecosystem == "github-actions") ++n_ga;
    if (n_ga >= 1) {
        r.verdict = Verdict::Pass;
        r.detail  = "found " + std::to_string(n_ga) +
                    " github-actions block(s)";
    } else {
        r.verdict = Verdict::Fail;
        r.detail  = "no `package-ecosystem: github-actions` block — "
                    "Dependabot is not tracking GitHub Actions";
    }
    rs.push_back(std::move(r));
}
```

The full R4 rule (the most interesting one — it does string-prefix
matching on the commit prefix):

```cpp
{
    RuleResult r;
    r.rule_id = "R4";
    r.rule_name = "commit-message.prefix is conventional";
    std::vector<std::string> bad;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        if (b.package_ecosystem != "github-actions") continue;
        auto starts_with = [](const std::string& s, const std::string& p) {
            return s.size() >= p.size() &&
                   s.compare(0, p.size(), p) == 0;
        };
        if (!starts_with(b.commit_prefix, "chore(deps)") &&
            !starts_with(b.commit_prefix, "ci(deps)")) {
            bad.push_back("block[" + std::to_string(i) +
                          "] commit-message.prefix=\"" +
                          b.commit_prefix + "\"");
        }
    }
    if (bad.empty()) {
        r.verdict = Verdict::Pass;
        r.detail  = "every github-actions block has chore(deps)/ci(deps) prefix";
    } else {
        r.verdict = Verdict::Fail;
        r.detail  = std::to_string(bad.size()) +
                    " github-actions block(s) have non-conventional prefix: ";
        for (std::size_t i = 0; i < bad.size(); ++i) {
            if (i) r.detail += ", ";
            r.detail += bad[i];
        }
    }
    rs.push_back(std::move(r));
}
```

The R4 detail string includes the *block index* and the
*actual prefix* so the failure message is actionable: "block
[0] commit-message.prefix=\"bump\"" tells the human exactly
what to fix.

## The unit tests

The 10 in-program unit tests exercise the rule engine in
isolation from any YAML parsing:

```cpp
static UnitTestResult unit_test_rules() {
    UnitTestResult u;
    auto check_rules = [&](const std::vector<UpdatesBlock>& blocks,
                           const std::string& label,
                           std::vector<std::pair<std::string, Verdict>> want) {
        std::vector<RuleResult> got = self_check(blocks);
        bool ok = (got.size() == want.size());
        for (std::size_t i = 0; ok && i < want.size(); ++i) {
            if (got[i].rule_id != want[i].first ||
                got[i].verdict != want[i].second) ok = false;
        }
        u.cases.emplace_back(label, ok);
        ...
    };
    ...
}
```

Each test builds a synthetic `std::vector<UpdatesBlock>`,
runs `self_check`, and asserts the (rule_id, verdict) pairs
match the expected list. U9 is the most thorough — it mixes
a fully-correct block with a fully-broken one and asserts
that R1 passes (at least one good block exists) while
R2..R6 all fail (the broken block violates every sub-rule).

The test fixtures are built by a tiny helper:

```cpp
static UpdatesBlock make_block(std::string eco, std::string interval,
                               std::string prefix, int pr_limit,
                               bool groups, bool labels) {
    UpdatesBlock b;
    b.package_ecosystem = std::move(eco);
    b.schedule_interval = std::move(interval);
    b.commit_prefix     = std::move(prefix);
    b.open_pr_limit     = pr_limit;
    b.has_groups        = groups;
    b.has_labels        = labels;
    return b;
}
```

This is the same `AssertResult` / structured-result pattern
the Aug 18 lesson used — every test result carries both a
human-readable label and a boolean pass/fail, and the
summary prints `passed / total`.

## Why an inspector rather than a one-line grep

A grep can find `package-ecosystem: github-actions` lines.
A grep cannot tell you:

- whether the block's `schedule.interval` is in `{daily, weekly}`
- whether the block has a `groups:` map (and how many groups)
- whether the `commit-message.prefix` is conventional
- whether `open-pull-requests-limit` is set to a sane value
- whether the block declares any `labels:`

A grep also cannot run inline unit tests against the rule
engine, and cannot produce a structured PASS/FAIL report
that can be wired into a CI job.

The inspector gives the human a structured report ("here
are the three update blocks, here are the six rules, here's
which ones fail") rather than a raw boolean.

## The three fixtures, side by side

**`dependabot_good.yml`** — every rule satisfied:

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    commit-message:
      prefix: "chore(deps)"
    open-pull-requests-limit: 5
    groups:
      actions:
        patterns:
          - "*"
    labels:
      - "dependencies"
```

**`dependabot_no_ga.yml`** — Dependabot tracks npm and pip,
but NOT github-actions. This is the "I forgot to enable it"
anti-pattern. R1 fails; R2..R6 pass vacuously.

**`dependabot_minimal.yml`** — a github-actions block exists,
but every other rule fails. This is the "I copy-pasted from
a tutorial" anti-pattern. R1 passes (the block is there);
R2..R6 all fail (the block is poorly configured).

## Observed output

The good fixture on the default build:

```
============================================================
 P-2026-08-19 — dependabot_config_inspector
============================================================
   #        ECOSYSTEM    INTERVAL           PREFIX  PRLIM   GROUPS  LABELS  DIRECTORY
  --  ---------------  ----------  ---------------  -----  -------  ------  ---------
   0   github-actions      weekly      chore(deps)      5      yes     yes  /

============================================================
 Self-check
============================================================
  R1  PASS  github-actions ecosystem present — found 1 github-actions block(s)
  R2  PASS  schedule.interval in {daily, weekly} — every github-actions block has a daily/weekly interval
  R3  PASS  groups declared (avoid PR flood) — every github-actions block declares a `groups:` map
  R4  PASS  commit-message.prefix is conventional — every github-actions block has chore(deps)/ci(deps) prefix
  R5  PASS  open-pull-requests-limit > 0 — every github-actions block has a positive PR limit
  R6  PASS  labels array present — every github-actions block declares a `labels:` array

  6 / 6 rules PASS

PASS — every rule is satisfied and every unit test passes.
exit=0
```

The no_ga fixture:

```
============================================================
 P-2026-08-19 — dependabot_config_inspector
============================================================
   #        ECOSYSTEM    INTERVAL           PREFIX  PRLIM   GROUPS  LABELS  DIRECTORY
  --  ---------------  ----------  ---------------  -----  -------  ------  ---------
   0              npm      weekly      chore(deps)      5      yes     yes  /frontend
   1              pip      weekly      chore(deps)      5      yes     yes  /backend

============================================================
 Self-check
============================================================
  R1  FAIL  github-actions ecosystem present — no `package-ecosystem: github-actions` block — Dependabot is not tracking GitHub Actions
  R2  PASS  schedule.interval in {daily, weekly} — every github-actions block has a daily/weekly interval
  R3  PASS  groups declared (avoid PR flood) — every github-actions block declares a `groups:` map
  R4  PASS  commit-message.prefix is conventional — every github-actions block has chore(deps)/ci(deps) prefix
  R5  PASS  open-pull-requests-limit > 0 — every github-actions block has a positive PR limit
  R6  PASS  labels array present — every github-actions block declares a `labels:` array

  5 / 6 rules PASS (1 FAIL)

FAIL — one or more rules are violated (see above).
exit=1
```

The minimal fixture:

```
   0   github-actions     monthly             bump      -       no      no  /

  R1  PASS  github-actions ecosystem present — found 1 github-actions block(s)
  R2  FAIL  schedule.interval in {daily, weekly} — 1 github-actions block(s) have non-daily/weekly interval: block[0] schedule.interval="monthly"
  R3  FAIL  groups declared (avoid PR flood) — 1 github-actions block(s) have no `groups:` map: block[0]
  R4  FAIL  commit-message.prefix is conventional — 1 github-actions block(s) have non-conventional prefix: block[0] commit-message.prefix="bump"
  R5  FAIL  open-pull-requests-limit > 0 — 1 github-actions block(s) have a missing or zero PR limit: block[0] open-pull-requests-limit=-1
  R6  FAIL  labels array present — 1 github-actions block(s) have no `labels:` array: block[0]

  1 / 6 rules PASS (5 FAIL)
exit=1
```

Three builds × three fixtures × three exit codes (0/1/1) —
every combination is consistent across the default, strict,
and ASan/UBSan builds.

## What the consumer exercises

- **`enum class` + scoped enum + `switch` over all values**
  — the `Verdict` enum is a small but real C++17
  type-system exercise; `verdict_name` is the canonical
  "switch over enum, return printable" pattern.
- **`std::vector` of records + structured rule engine** —
  `UpdatesBlock` + `RuleResult` is the same pattern the
  Aug 18 sha_pin_inspector used for `UsesRecord` +
  `AssertResult`.
- **`std::filesystem::path` + `fs::exists`** — the
  argument-parsing path uses `fs::path` for both the
  CLI argument and the "default next to the binary"
  resolution.
- **Hand-rolled recursive-descent JSON parser** — `mjson::Parser`
  is byte-identical to the Aug 18 lesson's parser (a
  deliberate copy — the lesson reuses the same pattern,
  not for code-sharing but for demonstrating the pattern's
  reusability across YAML inspectors).
- **YAML → JSON via PyYAML** — the same `popen`-based
  Python-bridge the Aug 18 lesson used.
- **`std::move` on `std::vector` elements** — every
  `RuleResult` is moved into `rs` (`rs.push_back(std::move(r))`),
  same pattern as the Aug 18 lesson.
- **`std::string::compare(0, p.size(), p)`** — the R4 rule
  uses the bounds-checked prefix-match idiom; the size
  check before the compare is the C++17 way to do
  `starts_with` (P0452R1 / `std::string::starts_with` is
  C++20, but the bounds-checked `compare` works in C++17).
- **In-program unit tests** — 10 cases of a synthetic
  `std::vector<UpdatesBlock>` run against `self_check`,
  with pass/fail labels printed inline.

## What is NOT in this lesson

- **Verification that Dependabot is actually enabled on the
  repo.** That requires a GitHub API call and a
  `GITHUB_TOKEN`; both are out of scope today. The lesson
  asserts the *config file* is well-formed, not that the
  config file is *active*.
- **Verification that the generated PRs actually re-pin to
  a SHA.** That's what yesterday's `sha_pin_inspector`
  does on the resulting diff — combine yesterday's
  inspector with today's inspector in a CI job for full
  coverage.
- **Network verification of SHA existence on GitHub.** Same
  as yesterday — that's a separate future lesson.
- **A deploy copy to `.github/dependabot.yml`.** The
  fixtures live in this lesson's directory. Promoting the
  good fixture to the canonical location is mechanical but
  depends on the workflow PAT scope (still blocked from
  Jul 6 / Jul 8).

## Build / run

```
# Default
clang++ -std=c++17 -O2 -g -Wall -Wextra -Wpedantic \
    -I/opt/homebrew/include \
    P-2026-08-19-dependabot-config-inspector.cpp \
    -o P-2026-08-19-dependabot-config-inspector

# Strict warnings
clang++ -std=c++17 -O2 -g \
    -Wall -Wextra -Wpedantic -Werror \
    -Wshadow -Wconversion -Wsign-conversion \
    -I/opt/homebrew/include \
    P-2026-08-19-dependabot-config-inspector.cpp \
    -o P-2026-08-19-dependabot-config-inspector-strict

# ASan + UBSan
clang++ -std=c++17 -O1 -g \
    -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    -I/opt/homebrew/include \
    P-2026-08-19-dependabot-config-inspector.cpp \
    -o P-2026-08-19-dependabot-config-inspector-asan

# Run (defaults to dependabot_good.yml next to the binary)
./P-2026-08-19-dependabot-config-inspector
./P-2026-08-19-dependabot-config-inspector dependabot_no_ga.yml
./P-2026-08-19-dependabot-config-inspector dependabot_minimal.yml
```

The `PYTHONPATH=/tmp/pylib` is injected inside the program
itself (same trick the Aug 18 lesson used). The cron setup
needs PyYAML at `/tmp/pylib`, installable via:

```
uv pip install --target /tmp/pylib pyyaml
```

## Files

```
late-may/cpp_practice/dependabot_config_inspector/
├── P-2026-08-19-dependabot-config-inspector.cpp    (this lesson's main TU)
├── P-2026-08-19-dependabot-config-inspector.md     (this file)
├── dependabot_good.yml                              (the GOOD fixture)
├── dependabot_no_ga.yml                             (the BAD fixture — no github-actions block)
└── dependabot_minimal.yml                           (the WARN fixture — minimal-effort config)
```

The C++ TU is ~870 lines:

- ~30 lines: `Verdict` enum + `verdict_name` + `RuleResult` struct
- ~30 lines: `yaml_to_json_via_python` (PyYAML bridge)
- ~170 lines: `mjson::Parser` (recursive descent JSON parser)
- ~80 lines: `UpdatesBlock` + `extract_block` + `extract_updates`
- ~140 lines: `self_check` (six rules)
- ~30 lines: `print_blocks` + `print_rules` (tabular report)
- ~250 lines: `unit_test_rules` (10 cases)
- ~50 lines: `main()` (argument parsing, dispatch, exit codes)

## Where we go next

The remaining open cross-cutting forward-on items from the
Aug 15 "where we go next" list, still with one closed today:

```
* Pin actions to commit SHAs              — CLOSED Aug 18
* Dependabot config well-formed          — CLOSED TODAY
* Multi-OS matrix extending to windows-latest.
* Status badge in README.
* vcpkg/Conan port.
* Branch protection requiring the matrix to pass.
* v0.16.0 promotion.
```

The next-most-natural lesson would be one of:

- **A combined CI job**: a GitHub Actions workflow that runs
  yesterday's `sha_pin_inspector` AND today's
  `dependabot_config_inspector` on every PR, and fails the
  PR if either inspector fails. This is the natural
  deployment target for both inspectors and would close the
  "branch protection requiring the matrix to pass"
  forward-on item in one step.
- **A PR-diff inspector**: a C++ program that takes a git
  diff and verifies that the diff is consistent with the
  rules (every Dependabot-opened PR for actions must rewrite
  `@<old-sha>` → `@<new-sha>`, never to a floating tag).
  This is the inspector that would actually verify the
  end-to-end contract: "Dependabot opens a PR, the PR is
  correct, the human merges it, the SHA pins stay current".
- **A network-based SHA verifier** (already mentioned in
  the Aug 18 lesson's where-we-go-next): a future lesson
  that hits GitHub's API (or shells out to `git ls-remote`)
  to confirm every SHA pin still exists on the upstream
  action's default branch.

No new forward-on list items from today's "where we go next" —
the dependabot-config inspector is a complete, verified C++
exercise, and the natural follow-on (combined CI job) is
listed above.
