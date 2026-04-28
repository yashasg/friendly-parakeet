# Kobayashi — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** CI/CD Release Engineer
- **Joined:** 2026-04-26T02:11:09.785Z

## Learnings

<!-- Append learnings below -->

---

### 2026-05 — PR #43 Dependency Submission 500 retry fix

**Scope:** `dependency-submission.yml` — transient GitHub 5xx on the POST to `/dependency-graph/snapshots` was failing the Windows matrix leg (run 25011478602 job 73248065522), and `fail-fast: true` (default) was cancelling the macOS sibling.

**Fixes (commit e32468a):**
- Added `retries: 3` to the `actions/github-script@v8` submit step — the input wires directly into Octokit's built-in retry logic and covers all 5xx responses without any custom scripting.
- Added `fail-fast: false` to the OS matrix strategy so a transient failure on one platform never cancels siblings.

**Key lesson:** `actions/github-script` exposes an `retries` input (integer) that passes straight to Octokit's `@octokit/plugin-retry`. Always set it on any step that POSTs to GitHub APIs that can transiently 5xx (dependency graph, check runs, deployments). Pair with `fail-fast: false` on matrix jobs that do external API calls so a single transient failure doesn't cascade.

---

### 2026-04-27 — PR #43 CI failure triage and resolution

**Scope:** Coordinated verification and resolution of all 4-platform CI failures on PR #43 (`user/yashasg/ecs_refactor`, HEAD `95031c07`).

**Failure families diagnosed (from CI job logs 73182280813/414/504/515):**

| Family | Files | Platforms | Root cause |
|---|---|---|---|
| Bar miss energy drain | `test_collision_system.cpp:286-301` | All 4 | After #280 moved miss energy drain from `collision_system` to `scoring_system`, two bar-miss tests were left calling only `collision_system` — the scoring side-effect (energy drain, flash_timer) was never exercised. |
| PR43 child cleanup | `test_pr43_regression.cpp:297-298,319` | All 4 | `make_registry()` called `wire_obstacle_counter()` which registered the `ObstacleTag` pool (via `on_construct<ObstacleTag>`) with a lower index than `ObstacleChildren`. EnTT `destroy()` iterates pools in reverse insertion order, so `ObstacleChildren` was removed first — `on_obstacle_destroy` found `try_get<ObstacleChildren>` null and silently skipped child cleanup. |
| beat_log Windows | `test_beat_log_system.cpp:98,110,164` | Windows only | `make_open_log()` opened `"/dev/null"` unconditionally; on Windows this path does not exist, `fopen` returns null, `beat_log_system` bailed early leaving `last_logged_beat == -1`. |

**Fixes landed (all 3 authored by yashasg / Co-authored Copilot):**
- `dca7664` — add `scoring_system(reg, 0.016f)` call after `collision_system` in both bar-miss tests
- `c6ca0e8` — `#ifdef _WIN32` guard in `make_open_log()` to use `"NUL"` on Windows
- `b0569a6` — `reg.storage<ObstacleChildren>()` added as the **first line** of `make_registry()`, before `wire_obstacle_counter()`, ensuring ObstacleChildren always has a lower pool index than ObstacleTag

**Local validation:** `./build/shapeshifter_tests "~[bench]"` → `All tests passed (2390 assertions in 733 test cases)`.

**CI state at handoff:** New 4-platform runs on `b0569a6c` / `a269665b` are `in_progress` (runs 25011375644, 25011375674, 25011375650, 25011375691). No duplicate reruns issued — new commits already triggered fresh checks automatically.

**Expected outcome:** All 4 platform checks (Linux, macOS, WASM, Windows) PASS. No remaining failures.

**Key lesson:** `wire_obstacle_counter()` in the shared test helper registers the `ObstacleTag` pool as a side effect. Any pool that `on_obstacle_destroy` must read must be primed (`reg.storage<T>()`) in the base `make_registry()` **before** `wire_obstacle_counter()` is called — not in a derived helper or after the fact.

---

### 2026-05 — Issue #272 CMake EXCLUDE REGEX revision (Keyser review)

**Scope:** Reviewer Keyser approved the #272 functional split but flagged a single build-wiring defect: `CMakeLists.txt:368` EXCLUDE REGEX contained `test_gesture_routing_split`, silently omitting the 7 new `[issue272]` tests from the `shapeshifter_tests` binary.

**Fix:** Removed `test_gesture_routing_split|` from the EXCLUDE REGEX pipe-delimited list. One-line surgical edit to `CMakeLists.txt`.

**Validation:** `cmake --build build --target shapeshifter_tests` compiled `test_gesture_routing_split.cpp` (confirmed in build output). `./build/shapeshifter_tests "[issue272]"` → `All tests passed (21 assertions in 7 test cases)`.

**Commit:** e600644 on `user/yashasg/ecs_refactor`

**Key lesson:** Whenever a new test file is added for a feature, explicitly verify it is NOT in any EXCLUDE REGEX in the CMakeLists test-sources section. The EXCLUDE list is a maintenance hazard — entries added for in-progress work on other agents can linger and silently exclude new tests when they are intentionally ready to ship.

---

### 2026-05 — Issue #177 (WASM CI VCPKG_TARGET_TRIPLET)

**Root cause:** `ci-wasm.yml` invoked `emcmake cmake` directly, bypassing `build.sh`'s `VCPKG_DEFAULT_TRIPLET` → `-DVCPKG_TARGET_TRIPLET` forwarding. Without the explicit CMake variable, vcpkg auto-detection during Emscripten cross-compile was not guaranteed to select `wasm32-emscripten`.

**Fix:** Added `-DVCPKG_TARGET_TRIPLET=wasm32-emscripten` directly to the `emcmake cmake` invocation in `ci-wasm.yml`. No `-Wno-dev` added; the pre-existing flag was left as-is (not a new workaround). Fix was committed in `61ed9d6` as part of a multi-issue batch commit alongside #172 and #173.

**Key lesson:** When a CI workflow bypasses `build.sh` (e.g. for cross-compile), any triplet/toolchain forwarding that lives in `build.sh` must be duplicated explicitly in the workflow step. Two options: (1) add the flag inline — simple and surgical for a single triplet, (2) route through `build.sh` with a new `--wasm` mode — more maintainable if the build script grows. This project chose option (1) as the minimal fix.

---

### 2026-05 — magic_enum vcpkg wiring

**vcpkg package name:** `magic-enum` (hyphen); CMake target: `magic_enum::magic_enum`; find_package module: `magic_enum`.

**Wiring pattern:** `find_package(magic_enum CONFIG REQUIRED)` → add to `_system_deps` SYSTEM-include loop → `target_link_libraries(shapeshifter_lib PUBLIC magic_enum::magic_enum)`. Public link on the lib propagates to both the exe and the test binary automatically — no need to repeat on downstream targets.

**Validated:** vcpkg installed `magic-enum:arm64-osx@0.9.7#1`; configure + `shapeshifter_lib` build clean, zero warnings.

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

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Diagnostics filed 6 new issues: #170 (multi-platform release, AppStore blocker), #174 (release trigger discipline), #177 (WASM triplet), #179 (macOS bundle), #187 (notarization), #197 (checkout@v4).
- Kobayashi findings merged to `.squad/decisions.md`; no duplicates with #44–#162.

---

### 2026-05 — PR #43 Windows checkout failure: colon filenames in .squad/

**Scope:** 6 `.squad/` files with ISO-8601 timestamps using `:` in filenames — invalid on Windows NTFS/FAT.

**Root cause:** Git allows `:` in tracked filenames on macOS/Linux, but `git checkout` on Windows-hosted runners fails with `error: invalid path` for any such file, blocking all subsequent build steps.

**Colon-containing paths renamed (`:` → `-`):**
- `.squad/log/2026-04-27T23:41:31Z-archetype-candidate-audit.md`
- `.squad/log/2026-04-27T23:58:23Z-squad-comment-cleanup.md`
- `.squad/orchestration-log/2026-04-27T23:41:31Z-keaton.md`
- `.squad/orchestration-log/2026-04-27T23:41:31Z-keyser.md`
- `.squad/orchestration-log/2026-04-27T23:58:23Z-keaton.md`
- `.squad/orchestration-log/2026-04-27T23:58:23Z-kujan.md`

**Fix:** `git mv` preserving content; pure path-only change, no C++ source touched.

**Validation:** `git ls-tree -r --name-only HEAD | grep ':'` → empty after commit. Full C++ suite (2808 assertions / 840 tests) already passed before this fix; no rebuild required for a path-only rename.

**Key lesson:** All squad log filenames using timestamps must replace `:` with `-` or `_` to stay Windows-safe. Any tooling that auto-generates log files should adopt the `-` convention permanently.

---

### 2026-04 — Dead ECS File / Build Cleanup

**Scope:** Prove dead-file removals are real by fixing the build that previous agents left broken.

**What was broken at start:**
- `obstacle_archetypes.{h,cpp}` in both `app/archetypes/` and `app/systems/` were supposed to be deleted; `app/archetypes/` copies were deleted in working tree but not committed; `app/systems/` copies were already deleted.
- `ring_zone.h` and `ring_zone_log_system.cpp` still alive despite Keaton's decision to delete them.
- `obstacle_entity.cpp` (new entity factory) existed but `app/entities/` was NOT in CMakeLists GLOB so it never compiled — guaranteed link failure.
- `obstacle_entity.cpp` still included `obstacle_data.h` which was deleted in working tree.
- `session_logger.cpp` still included `ring_zone.h` and emplaced `RingZoneTracker`.
- `test_obstacle_archetypes.cpp` included deleted `systems/obstacle_archetypes.h`.
- `obstacle_spawn_system.cpp` and test files used positional `ObstacleSpawnParams` init without `beat_info` field → `-Wmissing-field-initializers` errors.

**Fixes applied (commit 0d642e2):**
1. Deleted `app/components/ring_zone.h`
2. Deleted `app/systems/ring_zone_log_system.cpp`
3. Removed `ring_zone_log_system` declaration from `all_systems.h` and call from `game_loop.cpp`
4. Removed `ring_zone.h` include and `RingZoneTracker` emplace + `RING_APPEAR` log from `session_logger.cpp`
5. Removed stale `obstacle_data.h` include from `obstacle_entity.cpp`
6. Added `file(GLOB ENTITY_SOURCES app/entities/*.cpp)` to `CMakeLists.txt` and wired it into `shapeshifter_lib`
7. Rewrote `test_obstacle_archetypes.cpp` to use `entities/obstacle_entity.h` + `spawn_obstacle` + `ObstacleSpawnParams` with C++20 designated initializers
8. Fixed `test_obstacle_model_slice.cpp` 3-arg positional calls with designated initializers
9. Fixed `obstacle_spawn_system.cpp` multi-line positional init with explicit `beat_info = {}`

**Result:** Zero warnings. All 887 test cases pass (2983 assertions).

**Key lessons:**
- When a new `app/entities/` (or any new source dir) is created, it MUST be added to CMakeLists `file(GLOB ...)` and the `add_library` call. CMake GLOBs are dir-scoped; new dirs are invisible until explicitly added.
- `-Wmissing-field-initializers` fires for ALL positional aggregate inits shorter than the struct's field count, even when omitted fields have default member initializers. Use C++20 designated initializers (`{.kind = ..., .x = ...}`) for structs with optional/defaulted trailing fields — they suppress the warning and document intent.
- `app/components/` is exclusively for types emplaced on entities via `reg.emplace<T>()`. Context singletons live next to their wiring code (`obstacle_counter_system.h`). Non-component utility types live in `app/util/` or `app/systems/`.

---

### 2026-04-28 — Team Session Closure: ECS Cleanup Approval

**Status:** APPROVED ✅ — Deliverable logged; ready for merge.

Scribe documentation:
- Orchestration log written: .squad/orchestration-log/2026-04-28T08-12-03Z-kobayashi.md
- Team decision inbox merged into .squad/decisions.md
- Session log: .squad/log/2026-04-28T08-12-03Z-ecs-cleanup-approval.md

Next: Await merge approval.
