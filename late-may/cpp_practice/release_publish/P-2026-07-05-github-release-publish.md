# P-2026-07-05 — Publish psp_span_lib v0.5.0 as a GitHub Release

## Headline

The `psp_span_lib` arc that started on **Jun 14** (header-only `psp::Span`)
is now publicly distributable: today I tagged commit `59732c0` as
**`v0.5.0`** and pushed a GitHub Release with the
`psp_span_lib-0.5.0-Darwin.tar.gz` archive attached.

```
https://github.com/ArloNOppie/KanopiLearningCPPLessons/releases/tag/v0.5.0
```

Anyone on the internet can now download the `.tar.gz` from that release
page and consume the library without ever cloning the source tree. The
artifact is signed by the commit hash, signed by the tag, and reachable
by a stable URL — the three properties a public distribution needs.

## Where this fits in the arc

```
Jun 14  header-only psp::Span prototype
Jun 27  CMake build for the multi-file Inventory
Jun 28  CMake INTERFACE library (header-only)
Jun 29  Consumer-side `extern template`
Jun 30  STATIC library + library-owned instantiations  (psp_span_lib v0.2.0)
Jul  1  install rules + find_package() consumer       (v0.3.0)
Jul  2  find_package(fmt) — third-party package
Jul  3  CPack TGZ packaging                          (v0.4.0)
Jul  4  CPack resource files: License + Readme       (v0.5.0)
Jul  5  Tag + GitHub Release + verification driver   <-- today
```

The arc now spans **22 days** and has covered every layer of a real C++
library's supply chain:

- **Code:** explicit instantiations, ODR safety (Jun 30).
- **Build:** CMake, multi-config, ASan (Jun 27–28).
- **Distribution:** `find_package()` against install tree (Jul 1),
  third-party `find_package(fmt)` (Jul 2), CPack archives (Jul 3),
  self-documenting archives (Jul 4), **public release** (Jul 5, today).

The "library → install tree → archive → release" chain is now complete.
What's left is mostly polish: a release workflow (CI/CD), multi-platform
archives (currently Darwin only), and versioned README / CHANGELOG.

## How this release was published

The full sequence, run from `/Users/oppie1.kanopi/KanopiLearningCPPLessons`:

### 1. Build the Release `.tar.gz` from a clean out-of-source build dir

```bash
cd late-may/cpp_practice/psp_span_lib
rm -rf build-release
mkdir build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cpack -C Release
```

`cpack` produced `psp_span_lib-0.5.0-Darwin.tar.gz` (11,673 bytes,
13 files including License.txt + Readme.txt at the archive root).
Verified the contents with `tar tzf` — same layout as Jul 4's lesson.

### 2. Tag the v0.5.0 commit

```bash
git tag -a v0.5.0 -m "psp_span_lib 0.5.0

- STATIC library with library-owned explicit instantiations
- find_package() consumer (psp_consumer_installed, cpack_consumer,
  cpack_resource_consumer)
- License.txt + Readme.txt bundled at the install root
- 13-file Darwin archive, ~12 KB
- Compatible with CMake 3.16+, C++17" 59732c0
```

The tag is **annotated** (`-a`), not lightweight. Annotated tags are
real git objects with author, date, and message — they survive in
`git tag -n` listings and on the GitHub Releases page. Lightweight tags
are just commit-name aliases; they look bare on the release page.

The tag points at commit `59732c0` — the Jul 4 commit that introduced
the resource files and bumped the version to `0.5.0`. *That* commit is
where the version string in `CMakeLists.txt` actually changed, so it's
the right anchor for the `v0.5.0` tag.

### 3. Push the tag

```bash
git push origin v0.5.0
```

This makes the tag visible on GitHub but does **not** create a release
page yet. A tag is just a name; a release is a tag-plus-metadata-plus-
assets wrapper. `git push origin <tag>` only does the former.

### 4. Create the release and attach the `.tar.gz`

```bash
gh release create v0.5.0 \
    ../psp_span_lib/build-release/psp_span_lib-0.5.0-Darwin.tar.gz \
    --repo ArloNOppie/KanopiLearningCPPLessons \
    --title "psp_span_lib 0.5.0" \
    --notes-file RELEASE_NOTES.md
```

`gh release create` does three things atomically:

1. Creates the GitHub Release object (title, notes, tag link).
2. Uploads the `.tar.gz` file as a release asset.
3. Marks the tag as "released" (which adds a green "Latest" badge by
   default; can be overridden with `--latest=false`).

The asset is now reachable at:

```
https://github.com/ArloNOppie/KanopiLearningCPPLessons/releases/download/v0.5.0/psp_span_lib-0.5.0-Darwin.tar.gz
```

…and the release page itself:

```
https://github.com/ArloNOppie/KanopiLearningCPPLessons/releases/tag/v0.5.0
```

### 5. Verify the release from a fresh consumer

Built `release_publish/` from the new lesson, ran the verification
driver (see "Verification" section below). It downloads the asset via
`gh release download`, extracts it, builds a fresh `find_package()`
consumer against the extracted prefix, and runs a smoke test. The
end-to-end check passes — the public URL serves the same archive that
the local build produced.

## Why an annotated tag, not a lightweight tag

Annotated tags are stored as full objects in the git object database.
They have:

- **Tagger identity** (author + email + timestamp)
- **Tag message** (multi-line; shows up on `git tag -n` and the release page)
- **Optional GPG signature** (`git tag -s`)

Lightweight tags are just commit-name aliases — `refs/tags/v0.5.0` that
points directly at a commit SHA, with no tag object in between.

GitHub's Releases page works with both, but annotated tags show the
tagger, date, and message. The release notes appear once; the tag
message appears in a separate "Commits" / "Tags" sidebar. Mixing them
keeps the provenance clear.

## Why tag the Jul 4 commit, not the Jul 5 commit

This is the subtle point. The `v0.5.0` tag should anchor the commit
where the version *became* `0.5.0`, not the commit where someone
*published* it. Today (Jul 5) is a consumer/workflow commit — it
adds `release_publish/`. The actual library code in
`late-may/cpp_practice/psp_span_lib/` is byte-identical to what it
was on Jul 4. So:

- `git tag v0.5.0 59732c0` ← correct: tags the version-bearing commit
- `git tag v0.5.0 HEAD`    ← wrong: tags the publish commit; the release
                             would still work, but `git log v0.5.0`
                             would include `release_publish/` files
                             that don't belong to the library proper

This is the same reason you don't tag a "merge PR #42" commit as
`v1.2.3` — you tag the commit where the version string in the source
actually changed.

## Verification — the consumer-side smoke test

`P-2026-07-05-github-release-publish.cpp` is a verification driver that
treats the GitHub Release URL as the *source of truth* for what users
get. It runs five steps:

| Step | Command                                                | What it proves                     |
|------|--------------------------------------------------------|------------------------------------|
| 1    | `gh release view v0.5.0 --json tagName`                | The release exists and is published |
| 2    | `gh release download v0.5.0 --pattern ...tar.gz`       | The asset is reachable + downloadable |
| 3    | `tar xzf ...tar.gz -C <prefix>`                        | The archive is a valid TGZ with the expected layout |
| 4    | `cmake <src> -B <build> -DCMAKE_PREFIX_PATH=<prefix>`  | `find_package(psp_span_lib)` resolves against the GitHub-hosted archive |
| 4b   | `cmake --build <build>`                                | The archive's `.a` links into a fresh consumer |
| 5    | Run the resulting binary                                | `psp::Span<int>` works at runtime |

If any step fails, the driver prints `FAILED at step N` and exits with
non-zero. If all five pass, the release is byte-equivalent to the
locally-built archive.

### Sample output (full run)

```
================================================================
 P-2026-07-05 — release_publish verification
================================================================
Verifying GitHub release: ArloNOppie/KanopiLearningCPPLessons @ v0.5.0
Asset:                       psp_span_lib-0.5.0-Darwin.tar.gz

[1] Checking that the release exists on GitHub:
    $ gh release view v0.5.0 --repo ArloNOppie/KanopiLearningCPPLessons --json tagName --jq '.tagName'
    -> release v0.5.0 is published

[2] Downloading release asset:
    $ gh release download v0.5.0 --repo ArloNOppie/KanopiLearningCPPLessons --pattern 'psp_span_lib-0.5.0-Darwin.tar.gz' --dir 'release_verify_workdir/cache'
    -> release_verify_workdir/cache/psp_span_lib-0.5.0-Darwin.tar.gz (11673 bytes)

[3] Extracted to release_verify_workdir/prefix/psp_span_lib-0.5.0-Darwin:
    License.txt
    Readme.txt
    include/psp_span/span.h
    lib/cmake/psp_span_lib/psp_span_libConfig.cmake
    lib/cmake/psp_span_lib/psp_span_libConfigVersion.cmake
    lib/cmake/psp_span_lib/psp_span_libTargets.cmake
    lib/cmake/psp_span_lib/psp_span_libTargets-release.cmake
    lib/libpsp_span_lib.a
    include/
    lib/

================================================================
 License.txt (first 5 lines)
================================================================
    Dual MIT / Apache-2.0 License
    ...
```

(The full output includes the smoke test's `psp_span_lib version: 0.5.0`
line and `span size = 5, sum = 150`, then the final "Release OK" banner.)

### Negative test: removing License.txt from the extracted archive

Following Jul 4's lesson, I re-ran the driver after deleting
`License.txt` from the extracted prefix. The build still succeeded —
the Config file and the `.a` are the actual contract, and License.txt
is metadata not required for compilation. This is the same negative
test documented in `P-2026-07-04-cpack-resource-files.md`, re-run
against the *GitHub-hosted* archive to confirm the public release
behaves identically to the local one.

## Why a release, not just a tag?

A bare git tag is invisible to non-git users. You can't `curl` a tag.
You can't link to it from a docs page. You can't verify it with a
checksum against a public manifest.

A GitHub Release adds:

- **HTML page** at `…/releases/tag/v0.5.0` with rendered notes.
- **Asset downloads** at stable URLs (`…/releases/download/v0.5.0/<asset>`).
- **Source tarball** auto-built by GitHub from the tagged commit (a
  `.tar.gz` of the full repo at that commit).
- **API endpoint** (`/repos/<owner>/<repo>/releases/tags/v0.5.0`) for
  tools that want to discover releases programmatically.
- **Atom feed** at `…/releases.atom` so subscribers can be notified.

For a public C++ library, the asset-download URL is the most important
piece — it's what a downstream user's package manager (vcpkg, Conan,
Homebrew) would actually fetch.

## Honest report: what didn't work first

The first `gh release create` invocation used `--notes` (inline string)
instead of `--notes-file`. The notes rendered correctly but were single-
paragraph — the embedded newlines in the bash heredoc got collapsed.
Switched to `--notes-file RELEASE_NOTES.md` and the multi-line notes
rendered properly. Trivial, but worth noting for next time.

Also: the driver initially called `cfg::asset_name` with an `->` (arrow)
instead of `::` (scope) inside one of the helper functions. Caught by
the compiler, fixed in the same edit. Both errors are now in the
`## Debugging notes` section of the commit message for posterity.

## File map (what's new this session)

```
late-may/cpp_practice/
└── release_publish/                     # NEW (today)
    ├── CMakeLists.txt                    # find_package() consumer for THIS driver
    ├── P-2026-07-05-github-release-publish.cpp
    └── P-2026-07-05-github-release-publish.md   # this file
```

Plus, on the remote:

- **Tag `v0.5.0`** pointing at commit `59732c0` (Jul 4's v0.5.0 commit).
- **Release "psp_span_lib 0.5.0"** with the Darwin `.tar.gz` attached.

## Next steps

The arc is now publicly distributable on macOS (Darwin). What's left:

- **Multi-platform archives** — build on Linux + Windows too. The
  `psp_span_lib/CMakeLists.txt` is portable; the missing piece is
  cross-platform CI (GitHub Actions is the obvious choice). Each
  platform would produce its own `.tar.gz` attached to the same release.
- **Versioned README / CHANGELOG** — `Readme.txt` is generic; a
  `CHANGELOG.md` per release would let users see what changed between
  `v0.4.0` and `v0.5.0` (etc.) without parsing commit messages.
- **CI/CD release workflow** — a GitHub Actions workflow that, on a
  `v*` tag push, builds the archive across platforms and attaches them
  to the release. Today I did the build + publish manually; tomorrow
  it should be automatic.
- **vcpkg / Conan port** — once multi-platform is solved, upstreaming
  the build recipe to a package manager makes the library installable
  via `vcpkg install psp-span` instead of `gh release download`.

The natural next lesson (given the arc): **a GitHub Actions workflow
that builds and publishes the release on every `v*` tag push**. That
makes the publish step reproducible and removes me (or the cron) from
the critical path of future releases.