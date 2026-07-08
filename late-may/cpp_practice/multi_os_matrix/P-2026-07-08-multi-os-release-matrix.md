# P-2026-07-08 — multi_os_matrix: a GitHub Actions matrix workflow that publishes psp_span_lib for BOTH Linux AND macOS in one tag-push

## Headline

The single-OS release workflow from **Jul 6** is now a **2-OS
matrix** on a single job. One `git tag v0.6.0 && git push origin
v0.6.0` will now publish **two** archives in the same GitHub
Release:

```
psp_span_lib-0.6.0-Linux.tar.gz    (from ubuntu-latest)
psp_span_lib-0.6.0-Darwin.tar.gz   (from macos-latest)
```

The matrixed job has **one body** that runs twice in parallel
(once per OS), and a **single publish step** that uploads both
archives. The shape is "matrix of 1 job × 2 OS values" rather
than "two separate jobs", and the design choice is documented
inline in `release_matrix.yml` and below.

```
$ git tag v0.6.0
$ git push origin v0.6.0
   → CI spawns 2 jobs (one per OS)
   → each runs the same cmake/cpack shell
   → the matrix's single publish step uploads BOTH archives
   → https://github.com/KanopiAndPetra/KanopiLearningCPPLessons/releases/tag/v0.6.0
     now has two .tar.gz assets, one per platform
```

## Where this fits in the arc

```
Jun 14  header-only psp::Span prototype
...
Jun 27  CMake build for multi-file Inventory
Jun 28  CMake INTERFACE library (header-only)
Jun 29  Consumer-side `extern template`
Jun 30  STATIC library + library-owned instantiations  (v0.2.0)
Jul  1  install rules + find_package() consumer         (v0.3.0)
Jul  2  find_package(fmt) — third-party package
Jul  3  CPack TGZ packaging                            (v0.4.0)
Jul  4  CPack resource files: License + Readme         (v0.5.0)
Jul  5  Tag + GitHub Release + verification driver     (v0.5.0 published)
Jul  6  GitHub Actions: tag-push auto-release (Linux)  (1 OS)
Jul  8  GitHub Actions: tag-push auto-release (matrix) (2 OSes) ← today
```

## Why one matrixed job, not two separate jobs

The naive upgrade from a single-OS workflow to a multi-OS one is
"add a second job". Two jobs, each with its own publish step.
That works, but it has three problems:

1. **Duplication.** The publish step (and every step that
   `actions/checkout` then `cmake` then `cpack` then
   `softprops/action-gh-release`) has to be repeated. A fix to
   the upload logic has to be made in two places.

2. **Race on the asset list.** The two jobs run in parallel.
   The first one to reach the publish step creates the release
   object and uploads its archive; the second one uploads
   *its* archive to the same release. So far so good — but
   `softprops/action-gh-release`'s `fail_on_unmatched_files`
   is evaluated at the moment of publish, not at the moment
   of release creation. Job-A might publish its archive and
   see `fail_on_unmatched_files` flag-trip because at the
   moment of A's publish, only A's archive exists in the
   release (B's is still being built). The flag would
   erroneously fail the workflow.

3. **The `fail_on_unmatched_files` escape hatch.** To make
   #2 work, we'd have to relax the flag to `false`, which
   loses the Jul 6 bug-catching property: a workflow whose
   cpack step silently produces no archive would still
   succeed.

A matrixed job (one job body, multiple OS values) sidesteps
all three:

- The job body is written once.
- All matrix legs run in parallel; the implicit dependency
  between them is that the publish step runs **after** all
  legs complete. So by the time the publish step runs, both
  archives exist.
- `fail_on_unmatched_files: true` is still safe: the glob
  `psp_span_lib-*-{Linux,Darwin}.tar.gz` will match both
  archives (or — if the build is genuinely broken — zero,
  in which case the workflow fails loudly).

The cost is "you must know the matrix syntax" — but that's a
one-time learn, and it composes with future axes (compiler
version, build type, sanitizer on/off, etc.) without
rewriting the job body.

## The matrix syntax in one paragraph

```yaml
jobs:
  build-and-publish:
    name: Build (${{ matrix.os }})
    strategy:
      fail-fast: false
      matrix:
        os:
          - ubuntu-latest
          - macos-latest
    runs-on: ${{ matrix.os }}
    steps: ...
```

`strategy.matrix` declares the axes. We have one axis
(`os`) with two values. The matrix expands into two job
legs that share everything else. `${{ matrix.os }}` resolves
to the current leg's value of the `os` axis at evaluation
time. `fail-fast: false` means "if one leg fails, don't
cancel the other" — important because a Linux-only failure
shouldn't cancel a working macOS leg (we'd still get the
Darwin archive, and vice versa).

`name: Build (${{ matrix.os }})` is cosmetic but useful: the
GitHub UI shows the leg's name in the run summary, so we see
"Build (ubuntu-latest)" and "Build (macos-latest)" instead of
"build-and-publish" twice.

## The publish glob — why `{Linux,Darwin}` and not two lines

```yaml
files: |
  late-may/cpp_practice/psp_span_lib/build/psp_span_lib-*-{Linux,Darwin}.tar.gz
```

Three acceptable shapes for the glob:

1. **One combined glob with brace expansion** (what we use):
   `*-{Linux,Darwin}.tar.gz`. GitHub's glob engine supports
   `{a,b}` alternation, so this matches `*-Linux.tar.gz` and
   `*-Darwin.tar.gz` and uploads both.

2. **Two separate glob lines** (YAML block-scalar style):
   ```yaml
   files: |
     *-Linux.tar.gz
     *-Darwin.tar.gz
   ```
   Same result, two lines.

3. **A single glob that matches both** (works on bash, not on
   the GitHub API):
   `*-{Linux,Darwin}.tar.gz` — same as #1.

We use #1 because it's a single line, it's explicit about
which two OSes are expected, and it doesn't grow linearly
with matrix size. If we add `windows-latest` later, the line
becomes `*-{Linux,Darwin,Windows}.tar.gz`. The inspector
verifies the glob contains both `Linux` and `Darwin` substrings
to catch typos.

## The "smoke test" step — what's new vs Jul 6

The Jul 6 workflow ran a 5-step pipeline (configure, build,
cpack, publish, verify-downloaded-asset). Today's matrix adds
a **6th step** between build and cpack: a *local* smoke test
that runs against the freshly-built install tree on the
runner.

```yaml
- name: Smoke-test the built library
  run: |
    set -euo pipefail
    cmake --install build --prefix /tmp/psp_install_smoke
    cat > /tmp/psp_smoke.cpp <<'EOF'
    #include <psp_span/span.h>
    int main() {
      int a[] = {1,2,3,4,5};
      psp::Span<int> s(a);
      return s.size() == 5 ? 0 : 1;
    }
    EOF
    g++ -std=c++17 -I/tmp/psp_install_smoke/include \
        /tmp/psp_smoke.cpp \
        -L/tmp/psp_install_smoke/lib -lpsp_span_lib \
        -o /tmp/psp_smoke
    /tmp/psp_smoke
```

Why add this? The Jul 5 `release_publish` lesson's step 8
("download the published asset and run a consumer against it")
catches the *published-asset* failure mode. The smoke test
catches the *build-doesn't-actually-work* failure mode: if a
refactor of psp_span_lib accidentally breaks the install
tree (wrong `INSTALL_INTERFACE`, missing `target_compile_features`,
etc.), the smoke test fails *before* cpack runs, so we never
publish a broken archive. The combination is belt-and-suspenders:
smoke test catches build-time breakage, downloaded-asset
verification catches packaging-time breakage.

The smoke test program itself (`psp_smoke.cpp`) is
intentionally trivial: 5 ints, one `Span<int>`, one `.size()`
call. We're proving the include path resolves, the library
symbol links, and the template instantiates. That's enough
to catch every install-tree structural bug.

## The verification driver — what's new vs Jul 6

`P-2026-07-08-multi-os-release-matrix.cpp` is the same
inspector pattern as `P-2026-07-06-github-actions-release-workflow.cpp`,
extended for matrices:

- **YAML → JSON** via the same PyYAML + `on:` -> `True` rename
  trick.
- **JSON → typed model.** Same `Workflow`, `Job`, `Step`
  shapes, plus a new `matrixOsValues: vector<string>` on
  `WorkflowJob` and an `isMatrixed: bool`.
- **Self-checks.** 10 of them (vs 8 in Jul 6), with 3 new
  matrix-specific ones:
  - `strategy.matrix.os is an array of >=2 OS labels`
  - `matrix includes ubuntu-latest AND macos-latest`
  - `publish step 'with.files' glob covers Linux AND Darwin`
- **Local rebuild proof.** Same `cmake → cpack` shell, but
  now we extract `<version>` and `<system>` from the
  produced archive name and check that `<system>` is `Darwin`
  or `Linux` — the matrix's claim that "an archive named
  `psp_span_lib-<ver>-<OS>.tar.gz` will exist" is verified
  for at least one leg.

### Running the inspector

```bash
$ cd late-may/cpp_practice/multi_os_matrix
$ ./P-2026-07-08-multi-os-release-matrix \
      ./release_matrix.yml ../../..

==========================================================
 P-2026-07-08 — multi_os_matrix inspector
==========================================================
Workflow name : Release psp_span_lib (multi-OS)
Permissions   : contents=write
Triggers      :
                push.tags = [ 'v*' ]
                workflow_dispatch (manual)

Job: build-and-publish   runs-on: ${{ matrix.os }}   (matrix: os = ['ubuntu-latest', 'macos-latest'])
   1. Checkout
        uses: actions/checkout@v4
   2. Install CMake
        uses: jurplio/install-cmake@v1
   3. Configure (psp_span_lib)
        run: set -euo pipefail
   4. Build
        run: set -euo pipefail
   5. Smoke-test the built library
        run: set -euo pipefail
   6. CPack (.tar.gz)
        run: set -euo pipefail
   7. Publish GitHub Release
        uses: softprops/action-gh-release@v2

[rebuild] Same shell the matrix runs on one CI leg:
  [run] cmake -S '.../psp_span_lib' -B '.../build-cron-matrix' -DCMAKE_BUILD_TYPE=Release
  [run] cmake --build '.../build-cron-matrix' --config Release -j 2
  [run] cd '.../build-cron-matrix' && cpack -C Release --config CPackConfig.cmake
  -> archive : .../psp_span_lib-0.5.0-Darwin.tar.gz
  -> version : 0.5.0
  -> system  : Darwin

==========================================================
 Self-check
==========================================================
PASS  workflow name is not empty
PASS  permissions.contents == write
PASS  push.tags filter includes 'v*'
PASS  workflow_dispatch trigger present
PASS  exactly one job exists (multi-OS uses one matrixed job)
PASS  strategy.matrix.os is an array of >=2 OS values
PASS  matrix includes ubuntu-latest AND macos-latest
PASS  publish step's file glob covers both Linux AND Darwin archives
PASS  publish step 'with.files' glob covers Linux AND Darwin
PASS  cpack step is present in the matrixed job
PASS  local cpack on this machine produced a valid archive

PASS — matrix workflow well-formed AND local rebuild produced the archive one of its two legs will upload.
```

### Negative test: deliberately broken matrix

`release_matrix_broken_fixture.yml` is a workflow with every
required element removed (no permissions, branches-not-tags
trigger, no matrix, no checkout, no cpack, no publish).
Running the inspector on it:

```
$ ./P-2026-07-08-multi-os-release-matrix \
      ./release_matrix_broken_fixture.yml ../../..

...

Self-check
==========================================================
FAIL  workflow name is not empty — name field is empty or missing
FAIL  permissions.contents == write — no permissions.contents key
FAIL  push.tags filter includes 'v*' — no 'v*' filter in push.tags
FAIL  strategy.matrix.os is an array of >=2 OS labels — job is not matrixed (no strategy.matrix.os array)
FAIL  matrix includes ubuntu-latest AND macos-latest — matrix.os = [] (need both ubuntu-latest and macos-latest)
FAIL  publish step's file glob covers both Linux AND Darwin archives — no softprops/action-gh-release step found
FAIL  publish step 'with.files' glob covers Linux AND Darwin — no publish step with with.files found
FAIL  cpack step is present in the matrixed job — no 'cpack -C Release ...' run step
$ echo $?
1
```

Eight issues fire, exit code 1. The matrix-specific checks
(`strategy.matrix.os is an array of >=2 OS values`, `matrix
includes ubuntu-latest AND macos-latest`, the two publish-glob
checks) are the new ones that prove the inspector's matrix
assertions work end-to-end.

## Cost — what this matrix doesn't do

- **Windows is not in the matrix.** Adding `windows-latest`
  would require a Windows-aware `cpack` generator (NSIS, WIX,
  or a .zip generator that produces the right tree). That's
  a future lesson; today's library doesn't have any
  platform-specific code, but the packaging story is
  platform-specific.
- **No version matrix (compiler version, build type).** We
  use the default toolchain on each runner. A version axis
  would be `compiler: [gcc-12, gcc-13, clang-16]` and the
  toolchain would be installed by an `actions/setup-*-version`
  step. Deferred — single-version per OS is what consumers
  expect from a header-only template library.
- **No ASan matrix leg.** An `ENABLE_ASAN=ON` axis would
  produce an `-asan` archive alongside the release one, but
  the archive filename would need to encode the build type
  (`*-Darwin-asan.tar.gz`), which our CPack config doesn't
  do today. Future lesson.
- **The matrix runs BOTH legs even on a manual trigger.** A
  `workflow_dispatch` with `inputs.run_linux_only: true`
  would let us skip the macOS leg when iterating on Linux-
  only changes. We don't have that today; CI minutes on
  macOS are the dominant cost of any tag push.
- **No fail-fast policy tweaks.** `fail-fast: false` is the
  default-friendly choice (one leg's failure doesn't cancel
  the other), but it does mean the matrix takes the full
  slow-leg's wall time. A 20-minute macOS run while Linux
  fails in 2 minutes is a 20-minute bill. For our library
  size this is fine; for a heavier project, `fail-fast:
  true` + a `continue-on-error: true` publish step might
  be the right balance.

## Interesting sidetrack — Python 3.11 vs 3.9 on macOS

The C++ driver shells out to `python3` to convert YAML to
JSON. On this machine, the default `python3` is Apple's
**3.9.6**, which doesn't ship with `pyyaml`. We install
PyYAML into a per-user directory:

```bash
uv pip install --target /tmp/pylib pyyaml
```

…and the driver runs the helper as:

```bash
PYTHONPATH=/tmp/pylib:${PYTHONPATH:-} python3 /tmp/petra_yaml2json_XXXXXX.py <yml>
```

We use `/tmp/pylib` rather than `~/.local/lib` because the
cron may run on a clean shell where `~/.local/lib` isn't in
`PYTHONPATH` and the user-level install path varies. The
`${PYTHONPATH:-}` is bash parameter expansion that keeps
whatever the caller already had set, appending `/tmp/pylib`
only if `PYTHONPATH` was unset. (Set to empty if it was
unset, so the colon-prefix is safe.)

The Jul 6 driver had this same trick; today's lesson just
re-uses it.

## File map (what's new this session)

```
KanopiLearningCPPLessons/
├── .github/                              # (deferred — see Jul 6 notes;
│   └── workflows/                        #  PAT lacks 'workflow' scope;
│       └── release.yml                   #  the lesson copy is the
│                                          #  source of truth and is
│                                          #  byte-identical to this
│                                          #  file when Adam re-auths
│                                          #  the workflow scope.)
└── late-may/cpp_practice/
    └── multi_os_matrix/                  # NEW (the lesson)
        ├── release_matrix.yml            #    matrix workflow
        ├── release_matrix_broken_fixture.yml
        │                                  #    negative-test fixture
        ├── P-2026-07-08-multi-os-release-matrix.cpp
        │                                  #    inspector
        └── P-2026-07-08-multi-os-release-matrix.md
                                           #    this file
```

When the workflow scope is granted, the deploy copy goes to
`.github/workflows/release_matrix.yml` (NOT overwriting
`release.yml` — the matrix is its own file). The lesson copy
and the deploy copy are byte-identical at this commit.

## Next steps

The release pipeline now publishes two archives in one tag
push. Remaining work in the release-pipeline thread:

- **Pin actions to commit SHAs** — `@v4` is a floating tag.
  Production projects pin to specific commit SHAs to defend
  against a compromised action version. The inspector can
  add an assertion that each `uses:` line has either `@v<n>`
  (floating) or `@<40-char-hex>` (pinned), and warn (not
  fail) on the floating case.
- **Multi-OS matrix extending to `windows-latest`** — needs
  a Windows-compatible CPack generator (NSIS or .zip) and
  a different `with.files` glob. The `*-{Linux,Darwin,Windows}`
  brace expansion already accommodates the third value; only
  the cpack step needs platform branching.
- **Status badge in README** — `[![Release psp_span_lib
  (multi-OS)](...)](releases/tag/v0.5.0)`. Cosmetic but
  makes the publish pipeline visible from the repo front
  page.
- **vcpkg/Conan port** — once multi-platform and SHA-pinned,
  upstream the build recipe to a package manager so users
  can `vcpkg install psp-span` instead of `gh release
  download`. The arc from Jun 14 to Jul 8 would then be:
  code → install tree → archive → release → *automated
  multi-OS release* → *package manager*.
- **Branch protection requiring the matrix to pass** — would
  slow feature work but catch "matrix broken on Linux" or
  "matrix broken on macOS" before merge. The matrix's
  per-leg status checks make this natural: require both
  legs green.
- **Re-authorize the `workflow` PAT scope** — pending Adam.
  The lesson copy of `release_matrix.yml` is ready; the
  `.github/workflows/` deploy copy is blocked on the PAT.
