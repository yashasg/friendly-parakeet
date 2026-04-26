# Kobayashi — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** CI/CD Release Engineer
- **Joined:** 2026-04-26T02:11:09.785Z

## Learnings

<!-- Append learnings below -->

---

### 2026-05 — Issues #170 + #174 (4-platform release + semver gate)

**Scope:** Surgical fixes to `squad-release.yml` and `squad-insider-release.yml`.

**Root causes:**
- Both workflows ran on a single `ubuntu-latest` job, attaching only `build/shapeshifter` — Linux binary only. No macOS, Windows, or WASM artifacts were ever included in GitHub Releases (#170).
- `squad-release.yml` triggered on `push: branches: [main]`, creating a release on every commit including doc-only changes. `git describe --tags --always --dirty` generated hash-based dirty versions (#174).
- `squad-insider-release.yml` had the same `--dirty` issue and no `paths-ignore` to filter doc commits.

**Fix architecture — 5-job DAG:**
```
build-linux  ─┐
build-macos  ─┤
build-windows─┤──▶  release (collector)
build-wasm   ─┘
```
- Each platform build job mirrors its CI workflow (ci-*.yml), runs tests, uploads artifact
- WASM packaged as `shapeshifter-web.zip` (zip -j of 5 build-web/ outputs) before upload
- Release collector: `needs:` all 4, downloads via `download-artifact@v5`, validates non-empty (explicit `exit 1`, no `|| true`), creates release with 4 labeled assets

**Key decisions:**
- `squad-release.yml` trigger changed to `push: tags: v[0-9]+.[0-9]+.[0-9]+` + `workflow_dispatch` with version input. Version sourced from `github.ref_name` (tag) or dispatch input — never from `git describe`.
- `squad-insider-release.yml` keeps `push: branches: [insider]` but adds `paths-ignore` matching the CI workflows. Version = `git describe --tags --always` + `-insider` (no `--dirty`).
- Duplicate guard preserved: `gh release view $VERSION` + step output `skip=true/false` pattern (replaces the old `exit 0` which masked failures).
- `build-windows` job uses `shell: bash` for `build.sh` and test runner, matching `ci-windows.yml` exactly.
- `build.sh Release` explicit — default is already Release, but explicit is clearer in release context.

**Validated with Python YAML sanity script:** no `--dirty`, no `|| true`, all 4 `needs:`, validation step present, tag trigger pattern confirmed semver-only, `paths-ignore` present on insider.

**Commit:** d402668 on `user/yashasg/ecs_refactor`
**Comments posted:** #170 (issuecomment-4323248904), #174 (issuecomment-4323248909)

---

### Diagnostic Run — 2026-04 (Session)

**Scope:** Full CI/CD audit — all `.github/workflows/`, `CMakeLists.txt`, `vcpkg.json`, `build.sh`

**Platform coverage confirmed:** Linux (ubuntu-latest, clang-20), macOS (macos-latest, llvm@20 via brew), Windows (windows-latest, llvm via choco), WASM (emscripten 5.0.3). All four green on recent runs.

**Confirmed persistent failure:** `dependency-review.yml` fires on `push` to `main` but `actions/dependency-review-action@v4` requires a PR diff context (`base_ref`/`head_ref`). Has been failing on **every** main push. Fix: change trigger to `pull_request`.

**Squad workflow stubs:** `squad-release.yml`, `squad-insider-release.yml`, `squad-ci.yml`, `squad-preview.yml` all contain only `echo` stubs — no build/test/release logic. They run and pass trivially but provide zero value.

**`squad-promote.yml` C++ incompatibility:** References `package.json` (via `node -e "require('./package.json').version"`) and `CHANGELOG.md` — neither exists in this C++ project. The `preview→main` gate will error on first real use.

**No GitHub Release pipeline:** CI produces 4 platform artifacts (linux, macos, windows, web) but nothing creates a versioned GitHub Release.

### 2026-04-26 — Issue #48 (Windows LLVM pin) Resolved

**Scope:** CI/CD release engineer context on issue #48 final resolution.

**From diagnostics:** "Windows LLVM: `choco install llvm` without version pin — validated against `clang version 20.x` string. Breaks if Chocolatey ships 21."

**Resolution:** Ralph + Coordinator pinned Chocolatey LLVM to `llvm 20.1.4` (validated safe package); Kujan approved. Windows CI workflow now immune to version drift.

**README update:** Windows platform matrix now documents `Clang 20.1.4 (Chocolatey LLVM)` with clarity on CI-time install (not pre-installed).

**Integration status:** Aligns with ci-{linux,macos,wasm}.yml Clang 20 baseline. Ready for merge.

**Cache key mismatch:** `copilot-setup-steps.yml` uses key `cmake-linux-clang-` (no version/v-number) while `ci-linux.yml` uses `cmake-linux-clang20-v2-`. Permanent cache miss for Copilot agent setup.

**Action deprecation:** Squad workflows use `actions/github-script@v7` (Node 16, EOL). All CI workflows use `@v8` (Node 20) correctly. Need to lift squad workflows.

**WASM deploy fallback:** `cp -f _new_build/build-web/* . 2>/dev/null || cp -f _new_build/* . 2>/dev/null || true` — second fallback copies a directory rather than files; `|| true` masks all failures silently.

**No artifact retention policy:** Default 90 days; no explicit `retention-days` set anywhere.

**No milestones:** Repo has zero GitHub milestones defined despite `release:v0.4.0`, `release:v0.5.0`, etc. labels in the label schema.


---

### Diagnostic Run — 2026-05 (Session)

**Scope:** Fresh full CI/CD and release diagnostics for 4-platform release goal. Read-only audit of all 19 workflows, CMakeLists.txt, build.sh, vcpkg.json, vcpkg-overlay. Cross-checked against all open issues #44–#162 plus closed issues #3–#39 to avoid duplicates.

**Existing issues confirmed (NOT re-filed):**
- #44: dependency-review push trigger
- #45: squad-promote Node/npm-specific in C++ project
- #46: squad-release was echo stub (now implemented but #46 still open)
- #47: squad CI/preview stubs
- #48: Windows LLVM unpinned (resolved in decision)
- #49: Copilot setup cache key mismatch
- #50: WASM deploy silent failure
- #51: squad workflows using deprecated github-script Node 16
- #52: release label milestones missing
- #53: no artifact retention policy

**NEW issues filed this session:**
- **#170** [test-flight] Release pipeline ships Linux-only binary — squad-release.yml and squad-insider-release.yml both run on ubuntu-latest and attach only `build/shapeshifter`. macOS/Windows/WASM artifacts from CI jobs are never included in GitHub Releases.
- **#174** [test-flight] squad-release.yml triggers full build + new GitHub Release on every push, including doc-only commits. No paths-ignore. Uses `git describe --tags --always --dirty` generating hash-based versions per commit. Risk of dirty flag on CI too.
- **#177** [test-flight] WASM CI invokes `emcmake cmake` directly; bypasses build.sh, which contains the VCPKG_DEFAULT_TRIPLET → -DVCPKG_TARGET_TRIPLET forwarding logic. WASM cmake invocation has no explicit VCPKG_TARGET_TRIPLET.
- **#179** [AppStore] macOS CI artifact is a raw binary, not a distributable .app bundle. Ad-hoc `codesign --sign -` is local-only; not distributable or installable by testers without Gatekeeper bypass.
- **#187** [AppStore] No Developer ID signing or Apple notarization step in macOS CI. Required for TestFlight and any external distribution. No Apple credential secrets referenced anywhere in workflows.
- **#197** [AppStore] squad-promote.yml uses actions/checkout@v4 (Node 16, EOL) while all other workflows use @v6. Related to but distinct from #51 (github-script runtime).

**Milestones confirmed present:** `test-flight` (id=2), `AppStore` (id=3).

**Key architectural note:** The 4-platform CI matrix is well-structured and consistent. The gap is entirely in the release layer — CI builds everything but the release workflow only packages one platform. This is the highest-priority finding.
