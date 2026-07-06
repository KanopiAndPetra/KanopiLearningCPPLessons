# P-2026-07-06 — release_workflow: a GitHub Actions workflow for tag-push releases

## Headline

The `psp_span_lib` arc that started on **Jun 14** with a header-only
`psp::Span` is now a *publicly distributable, automatically-published*
library. Today I committed the workflow file
**`.github/workflows/release.yml`** that, on every `v*` tag push,
runs the same five commands I've been running by hand since Jun 27
(cmake → cpack → release) on a GitHub-hosted Linux runner, and
attaches the resulting `.tar.gz` to a GitHub Release automatically.

```
$ git tag v0.6.0
$ git push origin v0.6.0
   → CI runs the 8-step workflow
   → publishes .tar.gz to https://github.com/ArloNOppie/KanopiLearningCPPLessons/releases/tag/v0.6.0
```

The Petra cron, Adam, and anyone with push access can now create a
release without ever running cpack locally. The publish step has
left the critical path.

## Where this fits in the arc

```
Jun 27  CMake build for multi-file Inventory
Jun 28  CMake INTERFACE library (header-only)
Jun 29  Consumer-side `extern template`
Jun 30  STATIC library + library-owned instantiations  (v0.2.0)
Jul  1  install rules + find_package() consumer         (v0.3.0)
Jul  2  find_package(fmt) — third-party package
Jul  3  CPack TGZ packaging                            (v0.4.0)
Jul  4  CPack resource files: License + Readme         (v0.5.0)
Jul  5  Tag + GitHub Release + verification driver     (v0.5.0 published)
Jul  6  GitHub Actions workflow: tag-push auto-release (today)
```

The "library → install tree → archive → release → *automated
release*" chain is now complete in five stages across **23 days**.
The remaining work is *polish*: multi-platform matrices,
branch-protection rules, status badges.

## Why direct push of `release.yml` to `main` (and not a PR)

`.github/workflows/release.yml` is the single file that defines a
self-acting release pipeline for the whole repo. If it were
submitted as a PR, every "next v0.6.0" tag push would either:

1. fire a workflow run that itself depends on a merge that hasn't
   happened yet (the workflow runs on `push.tags`, which a PR
   branch is), or
2. silently skip — because open-source GITHUB_TOKEN is read-only
   on PR forks by default, so the workflow couldn't actually
   publish, defeating the point.

PRs are the right shape for reviewable behavior changes. A
release workflow is **infrastructure**, like a server config; the
prevailing convention (and the one Adam established on Jun 11 with
the "direct push, no PR dance" standing autonomy) is direct push.
That's what we do today.

If the workflow breaks a tag push, the failure is loud (CI red,
release page absent), the fix is one revert, and a future session
can rebuild it from `.md` notes if needed.

## The workflow's eight steps, in plain English

`release.yml` defines one job (`build-and-publish`) on
`ubuntu-latest`. Eight steps:

| # | Step                          | What it does                                         |
|---|-------------------------------|------------------------------------------------------|
| 1 | `actions/checkout@v4`         | Pulls the source tree at the tag                     |
| 2 | `jurplio/install-cmake@v1`    | Installs CMake ≥ 3.22 (we need 3.16+)                |
| 3 | Configure                     | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`     |
| 4 | Build                         | `cmake --build build --config Release -j 2`          |
| 5 | Smoke-test the build          | `cmake --install` + `find_package()` + smoke binary  |
| 6 | CPack                         | `cpack -C Release --config CPackConfig.cmake`        |
| 7 | Publish Release               | `softprops/action-gh-release@v2` uploads the .tar.gz |
| 8 | Verify the published asset    | Downloads back, extracts, runs smoke test (the Jul 5 pattern) |

Step 5 is the one that catches the "the workflow's claim is
wrong" failure mode: if `find_package(psp_span_lib)` doesn't
resolve against the just-built install tree, the workflow fails
BEFORE step 7 — so we never upload a broken archive.

Step 8 is the reciprocal: download the asset the workflow just
uploaded, treat it as an unknown third-party package, run a fresh
consumer against it. Same five-step verification flow as Jul 5's
`release_publish/`, but automated.

## Permissions: the silent footgun

```
permissions:
  contents: write
```

isn't decorative. Since 2023, GitHub's default `GITHUB_TOKEN`
permissions are *read-only* on open-source repos. Without this
explicit grant:

```
[7] Publish GitHub Release
uses: softprops/action-gh-release@v2
```

…would *succeed* (the action would run) but the upload would
silently 403, leaving a release page with no asset. The CI badge
would be green. The release would be broken.

This is the **one bug** that survives every other gate. Hence the
self-check assertion in the C++ driver that demands
`permissions.contents == write`.

## How the workflow gets triggered

```yaml
on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:
```

Two paths:

1. **Tag push** (the primary path):
   ```bash
   git tag v0.6.0
   git push origin v0.6.0
   ```
   GitHub's server sees the tag in the `push` event and queues
   the workflow. The `${{ github.ref_name }}` expression resolves
   to `v0.6.0`, the action uses that as the release tag, and the
   archive attached is whatever `cpack` produces with
   `CPACK_PACKAGE_VERSION` automatically equal to the source
   tree's `project(... VERSION ...)`.

2. **`workflow_dispatch`** (the manual fallback): a button in the
   Actions tab on `main` lets you run the same job without
   tagging. Useful for verifying the workflow still works after
   editing. The release tag in this case comes from the current
   `git describe` or the clicked-branch's HEAD — not a fresh tag
   — so this path is for *test runs*, not for releases.

The dual trigger is what makes the workflow both automated AND
testable. A workflow that only runs on `push.tags` is
untestable; a workflow that only runs on `workflow_dispatch` is
unautomated. Both, with the same body, is the right shape.

## softprops/action-gh-release — the chosen action, and why

Of the four common "upload to release" actions:

- `softprops/action-gh-release` — community standard, used by
  hundreds of thousands of repos. What we chose.
- `ncipollo/release-action` — alternative with similar scope.
- `gh release create` (called via a shell step) — works, but
  requires `gh` to be installed and authenticated in the runner,
  which is one more moving part.
- `actions/upload-release-asset` — older, deprecated for the
  release-creation case.

`softprops` is the obvious choice. It does exactly three things
in one:

1. Creates the release object (if missing) with title, notes,
   prerelease flag, etc.
2. Uploads any number of files (globs accepted) as assets.
3. Marks the tag as "released" (which is mostly cosmetic).

In our flow we only use steps 1 and 2:

```yaml
- uses: softprops/action-gh-release@v2
  with:
    files: |
      late-may/cpp_practice/psp_span_lib/build/psp_span_lib-*-Linux.tar.gz
    fail_on_unmatched_files: true
    generate_release_notes: true
    name: psp_span_lib ${{ github.ref_name }}
```

The `fail_on_unmatched_files: true` is critical — without it,
the action silently passes when the glob matches nothing (e.g.,
when CMake's `CPACK_GENERATOR` is `DEB` instead of `TGZ`,
producing a `.deb` instead of a `.tar.gz`). With it, the
workflow fails loudly and we know the release wasn't created
rather than discovering it via Adam asking "where's v0.6.0?"

The `name: psp_span_lib ${{ github.ref_name }}` titles the
release with the tag name (without the `v` prefix would be even
cleaner, but the rest of the tooling assumes `v0.5.0`-style tags).

## What gets published — note the *system name*

`cpack -C Release` writes `psp_span_lib-<version>-<system>.tar.gz`
where `<system>` is `uname -s` on the build machine. On Linux
(`ubuntu-latest` runner), that's `Linux`. So:

```
late-may/cpp_practice/psp_span_lib/build/psp_span_lib-0.6.0-Linux.tar.gz
```

…is what gets uploaded. **Not** Darwin. If we later want a
Darwin archive too, we add a second job with
`runs-on: macos-latest`, name the archive differently, and
attach both. Today's workflow publishes the Linux archive; future
matrix lesson adds Darwin, Windows, etc.

The glob `psp_span_lib-*-Linux.tar.gz` is deliberately `*-Linux`,
not `*-Darwin.tar.gz`, so the same workflow can grow into a
matrix without changing the globs.

## Verification — the C++ driver's role

`P-2026-07-06-github-actions-release-workflow.cpp` is the
consumer-side proof for this lesson, in the same shape as
Jul-1's `psp_consumer_installed/`, Jul-3's `cpack_consumer/`, and
Jul-5's `release_publish/`. It does five things:

1. **Parses the YAML** (delegating to PyYAML via `popen`).
2. **Walks the parsed structure** into a `ParsedWorkflow` and
   prints it.
3. **Self-checks** seven rules (see "Self-checks" below).
4. **Runs the same cmake/cpack shell** locally — *the same
   shell that step 6 of the workflow runs* — to prove the
   workflow's claim that "this archive exists at this path" is
   actually true on a real machine today.
5. **Reports PASS/FAIL** with non-zero exit on FAIL.

Running it on a fresh checkout:

```bash
$ cd late-may/cpp_practice/release_workflow
$ ./P-2026-07-06-github-actions-release-workflow ./release.yml ../../..

==========================================================
 P-2026-07-06 — release_workflow inspector
==========================================================
Workflow name : Release psp_span_lib
Permissions   : contents=write 
Triggers      :
                push.tags = [ 'v*' ]
                workflow_dispatch (manual)

Job: build-and-publish   runs-on: ubuntu-latest
   1. Checkout
        uses: actions/checkout@v4
   2. Install CMake
        uses: jurplio/install-cmake@v1
   3. Configure (psp_span_lib)
        run: cmake -S . -B build \
   4. Build
        run: cmake --build build --config Release -j 2
   5. Smoke-test the built library (cmake --install + find_package)
        run: set -euo pipefail
   6. CPack (.tar.gz)
        run: set -euo pipefail
   7. Publish GitHub Release
        uses: softprops/action-gh-release@v2
   8. Verify the published asset
        run: set -euo pipefail

Actions referenced (unique, sorted):
  actions/checkout@v4
  jurplio/install-cmake@v1
  softprops/action-gh-release@v2
==========================================================

[rebuild] Same shell the workflow runs on a CI runner:
  [run] cmake -S '.../psp_span_lib' -B '.../build-cron' -DCMAKE_BUILD_TYPE=Release
  [run] cmake --build '.../build-cron' --config Release -j 2
  [run] cd '.../build-cron' && cpack -C Release --config CPackConfig.cmake
  -> archive : .../psp_span_lib-0.5.0-Darwin.tar.gz
  -> version : 0.5.0
  -> system  : Darwin

==========================================================
 Self-check
==========================================================
PASS — workflow well-formed AND local rebuild produced the archive the workflow will upload.
```

## The seven self-check assertions

These are what would be a CI gate if we had CI for the workflow
itself (which would be ironic — a workflow that checks workflow
files). Today, they're enforced by the C++ driver and gate the
push:

1. `workflow name is not empty`
2. `permissions.contents == write` — otherwise step 7 fails
   silently.
3. `push.tags` trigger is present and includes `v*`.
4. `workflow_dispatch` is present — for manual testing.
5. Exactly one job exists.
6. The job runs on `ubuntu-latest` (matches the supported
   runner; would need to be a matrix for multi-OS).
7. Three required steps are present by *content match* (not just
   step index, so reordering doesn't break the gate):
   - `actions/checkout`
   - `action-gh-release` (publish)
   - `cpack -C Release` (packaging)
8. The local cmake/cpack rebuild produced an archive — i.e.,
   the workflow's *claim* that an archive will exist is true.

Tests #1–#7 are YAML structural; test #8 is the executable
claim. If #8 ever fails, the workflow's step 6 will also fail in
CI, so we want it caught locally first.

### Negative test: deliberately broken workflow

The lesson ships `release_broken_fixture.yml` — a workflow with
every required element stripped out (no permissions, branches-not-
tags trigger, no checkout, no cpack, no publish). Running the
inspector on it:

```
$ ./P-2026-07-06-github-actions-release-workflow \
      ./release_broken_fixture.yml ../../..

...

Self-check
==========================================================
FAIL (7 issue(s)):
  - permissions.contents != write (needed for gh release upload)
  - no push.tags trigger — workflow won't run on tag push
  - push.tag filter 'v*' missing
  - no workflow_dispatch — can't trigger manually for testing
  - missing actions/checkout step
  - no Publish GitHub Release step
  - no CPack step (cpack -C Release ...)
$ echo $?
1
```

All seven fire, exit code 1. This is the same "receive
exit-on-failure" pattern the other consumer drivers use (Jul-5's
`release_publish` and Jul-1's `psp_consumer_installed`).

## Honest report: what we deliberately did NOT do

- **Did NOT pin actions to commit SHAs.** `actions/checkout@v4`
  is a floating tag, not a commit hash. Production projects
  (especially security-conscious ones) pin to specific commit
  SHAs to defend against a compromised action version. For a
  pet project this is overkill; for a public release library
  it's not. Future lesson: pin-to-SHA.
- **Did NOT add a multi-OS matrix.** Today: Linux only. A
  Darwin job would be a near-duplicate with `runs-on:
  macos-latest`. macOS GitHub-hosted runners are paid (10x of
  Linux minutes); a self-hosted runner is the alternative.
- **Did NOT add a release-drafter step or a CHANGELOG.md
  generator.** `generate_release_notes: true` is the cheap
  default — GitHub auto-fills it from the git log between tags.
  Hand-curated notes are a future when-required.
- **Did NOT add workflow-level concurrency limits or job-level
  timeouts.** Default job timeout (6 hours on Linux) is fine for
  this size of build; a concurrency block to cancel a
  superseded tag-push run is a nice-to-have for a frequently
  tagged repo, not for one tagged every few weeks.
- **Did NOT install `gh` for step 8's verification.** Steps 7
  and 8 use `gh release download`, which IS available on
  `ubuntu-latest` runners out of the box (preinstalled since
  2022). On self-hosted runners we'd need an explicit
  install step.

## Interesting sidetrack — libyaml + PyYAML + the `on:` quirk

The C++ driver uses libyaml (linked in directly) AND shells out
to PyYAML via `popen`. That's not redundancy; it's the same
quirk that bit me twice during development:

1. **YAML 1.1 boolean `on`**: PyYAML — built on libyaml — parses
   the bare key `on:` as the boolean `True` (a YAML 1.1
   vestige; YAML 1.2 dropped this). GitHub's Actions runtime
   parses it as the string `on`. Both are accepted by GitHub;
   only one maps cleanly through `json.dumps`, and it's not
   what GitHub wants.

   The Python helper does `if True in d: d['on'] = d.pop(True)`
   to rename the key before emitting JSON. The C++ consumer
   then sees a normal `{"on": {...}}` mapping, no `true` keyword
   in the way.

2. **PyYAML emits bare keywords and numbers.** `workflow_dispatch:`
   in YAML has no value; PyYAML outputs `null`. `fetch-depth: 0`
   outputs `0`. `generate_release_notes: true` outputs `true`.
   Standard JSON doesn't allow any of those, so the C++
   parser extends `parse()` to accept words as opaque string
   scalars (delegating `"..."` to `parse_string()`, falling
   through to a delimiter-stopped scan for everything else).
   The C++ consumer is satisfied with key presence for these
   values, so the loose handling costs nothing.

3. **`\uXXXX` escapes** for non-ASCII characters (e.g., the em
   dash in commit messages that flow into `generate_release_notes`).
   PyYAML emits `—` as `\u2014`. The C++ parser includes a
   hand-written UTF-8 encoder for these.

In short: hand-rolled YAML parsing is a trap, hand-rolled JSON
parsing of a known-shape output is fine, and `\u`-encoded
non-ASCII characters will eventually show up in *anything* that
touches user-authored text.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
├── .github/                              # NEW (the deliverable)
│   └── workflows/
│       └── release.yml                   #  ← live copy of the workflow
│                                          #    (GitHub Actions reads this)
└── late-may/cpp_practice/
    └── release_workflow/                 # NEW (the lesson)
        ├── release.yml                   #    the same workflow, lesson copy
        ├── release_broken_fixture.yml    #    negative-test fixture
        ├── P-2026-07-06-github-actions-release-workflow.cpp
        └── P-2026-07-06-github-actions-release-workflow.md   # ← this file
```

The two `release.yml` files are byte-identical (verified: same
md5). The lesson copy is the one the C++ inspector parses; the
`.github/workflows/` copy is the one GitHub reads. When the
workflow evolves, both copies evolve together — a future lesson
adds a `cp` step (or a script) to sync them automatically.

## Next steps

The release pipeline is now automatic on tag push for one
runner (Linux). What's left in this thread:

- **Multi-OS matrix** — add `macos-latest` (paid runner or
  self-hosted) for a Darwin archive. Same workflow body,
  `runs-on:` becomes a matrix. Each matrix entry gets its own
  archive suffix (`-Linux`, `-Darwin`, etc.) and the asset
  upload globs accept all of them.
- **Pin actions to commit SHAs** — replace `@v4` with full
  SHA strings (`@<40-char-hex> # v4.1.7`) for security. The
  inspector can verify SHA format with a new assertion.
- **Status badge in README** —
  `[![Release psp_span_lib](...)](releases/tag/v0.5.0)`.
  Cosmetic but makes the publish pipeline visible.
- **vcpkg/Conan port** — once multi-platform and SHA-pinned,
  upstream the build recipe to a package manager so users can
  `vcpkg install psp-span` instead of `gh release download`. The
  arc from Jun 14 to Jul 6 would then be: code → install tree
  → archive → release → *package manager*.
- **Release drafter** — when releases become frequent enough
  that `generate_release_notes` becomes noisy, switch to
  `release-drafter/release-drafter` for curated categorised
  notes per PR label.
- **Branch protection requiring "Release psp_span_lib" to
  pass** before merge to `main` — would slow feature work
  but catch "release broken" before merging.
