# Hockney — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Platform Engineer
- **Joined:** 2026-04-26T02:07:46.546Z

## Learnings

<!-- Append learnings below -->

## Session: Issue #253 — HighScoreState compact flat array (2026-04-27)

### Problem
`HighScoreState::scores` was `std::map<std::string, int32_t>`: O(log N) node-based tree with heap allocations for ≤9 entries (3 songs × 3 difficulties = 9 max).

### Fix (commit 60bcd26 / afd7921)
- `app/components/high_score.h`: replaced `std::map` with `std::array<Entry, 9>` + `entry_count` counter.  `Entry = {char key[32]; int32_t score;}` — zero heap, trivially copyable.  Added `get_score(key)` and `set_score(key, val)` helpers (linear scan, O(N) for N≤9).  `update_current_high_score` semantics preserved (never-lower, clamp negative).  `current_key` kept as `std::string` (not hot-path).
- `app/util/high_score_persistence.cpp`: iteration changed to index loop over `entries[]`; `set_score()` replaces `operator[]`.  JSON on-disk format unchanged.
- `tests/test_high_score_persistence.cpp` + `tests/test_high_score_integration.cpp`: all `.scores["key"]=val` → `set_score("key", val)`, `.scores.at("key")` → `get_score("key")`, map equality check → per-key comparisons.

### Learning: shared working-tree contention
Concurrent agent commits in the shared tree caused commit `afd7921` (my message) to contain other agents' staged changes, and `60bcd26` (Redfoot's message) to contain my high_score changes.  The changes ARE correctly in HEAD; attribution is split across two commits.  Strategy for future sessions: use `git update-index --cacheinfo` (atomic index write) to avoid racing with concurrent `git add` from other agents.

## Session: iOS TestFlight Readiness #180 #182 #183 #184 #186

### Issues Addressed

| Issue | Title | Deliverable |
|-------|-------|-------------|
| #180 | iOS audio session interruption handling | `docs/ios-testflight-readiness.md` §1 |
| #182 | App background/foreground lifecycle mid-song | `docs/ios-testflight-readiness.md` §2 |
| #183 | iOS version and build number scheme | `docs/ios-testflight-readiness.md` §3, `app/ios/build_number.txt` |
| #184 | Bundle identifier, team, code signing | `docs/ios-testflight-readiness.md` §4 |
| #186 | Device and OS support matrix | `docs/ios-testflight-readiness.md` §5 |

### Key Decisions Made

- **AVAudioSession: `Playback` (not Ambient).** Rhythm game is primary audio — needs interruption-begin/end callbacks and mute-switch respect. `Ambient` loses the callbacks needed for `song_time` resync.
- **All audio interruptions + backgrounding map to the same pause state machine as #74.** No auto-resume — player must tap. Keeps one code path, avoids surprise desync.
- **`StopMusicStream` (not just Pause) on `applicationDidEnterBackground`.** Safer under iOS process suspension. Re-seek with `SetMusicTimePlayed` on foreground.
- **Version: `MAJOR.MINOR.PATCH` SemVer from `CMakeLists.txt`, build number from `app/ios/build_number.txt`.** Single source of truth per field; build number is a monotonic integer that never resets.
- **Bundle ID: `com.yashasg.shapeshifter` (proposed).** User must confirm and register. Team ID is user-provided.
- **iOS v1 signing: local Xcode Automatic Signing.** CI signing deferred post-TF.
- **Minimum iOS: 16.0.** iPhone-only, portrait-only. iPad deferred.
- **Viewport: 720×1280 centered with uniform scaling + black letterbox bars.** No layout reflow.
- **Cap at 60fps** until #204 (ProMotion) resolved.

### User-Provided Values Still Needed

1. Apple Developer Team ID (10-char) — for CMake `-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM`
2. Apple Developer Program type (individual/org)
3. Confirm or modify bundle ID `com.yashasg.shapeshifter`
4. App icons (1024×1024 + all sizes)
5. Bump `app/ios/build_number.txt` from 0 → 1 before first TF upload

### Files Changed

- **Created:** `docs/ios-testflight-readiness.md` (comprehensive, 5 sections)
- **Created:** `app/ios/build_number.txt` (initial value: 0)

### What's Still Needed (Not Hockney's scope)

- `ios/Info.plist`, `ios/Entitlements.plist`, `ios/LaunchScreen.storyboard`, `ios/Assets.xcassets/` — needs implementation work.
- `tools/ios_preflight.sh` — placeholder defined in doc; needs implementation.
- `CHANGELOG.md` — needs creating before first TF upload.
- iOS CMake toolchain integration (raylib iOS build, asset bundling) — larger implementation issue.

---

## Session: TestFlight Blockers #172 #173 #177 (2026-05-01)

### Issues Fixed

| Issue | Title | File |
|-------|-------|------|
| #172 | WASM CI branch triggers missing dev/preview/insider | `.github/workflows/ci-wasm.yml` |
| #173 | Six asset globs lack CONFIGURE_DEPENDS | `CMakeLists.txt` |
| #177 | emcmake cmake missing -DVCPKG_TARGET_TRIPLET | `.github/workflows/ci-wasm.yml` |

### Key Learnings

- **#172 pattern:** When a new long-lived branch is added to the project (`dev`, `preview`, `insider`), all CI workflow `pull_request.branches` lists must be updated. The WASM workflow was divergent from `squad-ci.yml` which already listed all four branches. Lesson: check branch parity across workflows any time a new protected branch is created.

- **#173 pattern:** `CONFIGURE_DEPENDS` must be added inline after the `GLOB` keyword, before the glob expressions. CMake 3.20 (our minimum) fully supports this. The flag does not affect build output, only re-configure triggering — safe to add retroactively. Only the six *asset-copy* globs needed fixing here; source-code globs (#55) are a separate issue.

- **#177 pattern:** When calling `emcmake cmake` directly (not through `build.sh`), all vcpkg context normally provided by the shell script must be replicated. For WASM cross-compilation, vcpkg's CMake toolchain reads `VCPKG_TARGET_TRIPLET` as a CMake variable — setting `VCPKG_DEFAULT_TRIPLET` as an env var is insufficient and not present in the WASM workflow anyway. Explicit `-DVCPKG_TARGET_TRIPLET=wasm32-emscripten` is the reliable fix.

### Validation Performed
- YAML: `python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/ci-wasm.yml"))'` → valid
- CMake: `cmake -B build -S . -Wno-dev` → `Configuring done` exit 0
- Commit: `61ed9d6` on `user/yashasg/ecs_refactor`

## Session: 2026-04-26 Diagnostics Run

### Platform / CI Findings

- **CMakeLists.txt** uses `cmake_minimum_required(VERSION 3.20)`, C++20, cmake EXPORT_COMPILE_COMMANDS.
- **Warning policy**: `shapeshifter_warnings` INTERFACE target applies `-Wall -Wextra -Werror` for GCC/Clang/AppleClang only. MSVC `/W4 /WX` is documented in `copilot-instructions.md` but NOT wired into CMake. Windows CI uses Clang (not MSVC), so this gap is currently latent.
- **file(GLOB)**: All `file(GLOB ...)` calls lack `CONFIGURE_DEPENDS`. New source files are silently ignored until re-configure.
- **LINK_FLAGS**: WASM preload-file flags use deprecated `LINK_FLAGS` property rather than `target_link_options()`.
- **Cache keys**: All four CI workflows hash only `CMakeLists.txt` + `vcpkg.json`. `vcpkg-overlay/**` changes don't invalidate cache.
- **WASM only passes `-Wno-dev`** at configure step; native CI uses `build.sh` which omits it.
- **Misleading comment** at CMakeLists.txt:146 claims "RGFW — no GLFW" but portfile builds `PLATFORM=Desktop` (bundled GLFW). WASM uses `-sUSE_GLFW=3`.
- **WASM deploy scripts** use `cp || cp || true` pattern — silent failure masking. A completely failed copy still exits 0.
### 2026-04-26 — Windows LLVM Pin: Issue #48 Diagnostics Validated

**Scope:** Hockney's earlier diagnostics (Windows LLVM unsafe `choco install llvm` pin) now finalized by Ralph/Coordinator/Kujan.

**Finding from diagnostics:** "Windows CI: `choco install llvm` has no version pin; version check fails if choco ships Clang 21+."

**Resolution:** Ralph + Coordinator pinned to `llvm 20.1.4`; Kujan approved. Windows CI now safe from Chocolatey version drift.

**Platform matrix update:** README.md documents Windows as `Clang 20.1.4 (Chocolatey LLVM)` — no longer "pre-installed" (clarified as CI-time install with version pin).

- **WASM missing `-sNO_EXIT_RUNTIME=1`**: If raylib's main loop exits, the Emscripten runtime tears down.
- **copilot-setup-steps** uses cache key prefix `cmake-linux-clang-` while `ci-linux.yml` uses `cmake-linux-clang20-v2-` — they never share cache entries.
- **Git hash** is captured at CMake configure time (`execute_process`). Stale after new commits without re-configure.

## Session: 2025 Fresh Diagnostics Run

### Scope
Fresh pass over CMakeLists.txt, vcpkg.json, vcpkg-overlay/, build.sh, run.sh, all .github/workflows/, README.md. Checked existing issues #44–#162 to avoid duplicates.

### Previously-fixed issues confirmed resolved
- #56 (CI cache keys omit vcpkg-overlay): FIXED — all workflows now hash `vcpkg-overlay/**`
- #59 (WASM missing `-sNO_EXIT_RUNTIME=1`): FIXED — present in current CMakeLists.txt `target_link_options`
- #62 (WASM CI no test execution): FIXED — `ci-wasm.yml` now has "Run WASM tests (via CTest + Node)"
- #54 (MSVC /W4 /WX not wired): FIXED — `shapeshifter_warnings` INTERFACE target has MSVC generator expressions

### New issues filed (this session)
| Issue | Title | Milestone |
|-------|-------|-----------|
| #165 | squad-promote.yml uses stale actions/checkout@v4 (all others @v6) | AppStore |
| #166 | GitHub Releases Linux-only (CLOSED as duplicate of #170) | — |
| #172 | WASM CI does not validate PRs targeting dev/preview/insider branches | test-flight |
| #173 | CMake content/asset globs lack CONFIGURE_DEPENDS (fonts, beatmaps, audio, UI, shaders) — distinct from source glob issue #55 | test-flight |

### Duplicates skipped
- #166 closed as dup of #170 (Kobayashi filed more complete version covering WASM artifacts too)

### Key architectural notes
- `squad-promote.yml` uses `actions/checkout@v4` (not @v6) — inconsistency
- Six asset-copy `file(GLOB)` calls in CMakeLists.txt all lack `CONFIGURE_DEPENDS`; adds new content silently skipped until re-configure
- WASM CI only triggers on PRs to `main`; dev/preview/insider branch PRs get Linux-only validation
- `LINK_FLAGS` deprecated property (#60) is still present (not yet fixed)
- `squad-release.yml`/`squad-insider-release.yml` are Linux-only release builders — covered by #170

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Diagnostics filed 3 new issues: #165 (actions@v4 EOL), #172 (WASM CI validation, test-flight blocker), #173 (CONFIGURE_DEPENDS).
- Hockney active: #180–#186 (5 issues in current wave).
- Findings merged to `.squad/decisions.md`.

**Final Wave (2026-04-26):**
- #180/#182/#183/#184/#186: iOS TestFlight readiness — audio session, app lifecycle, version scheme, bundle ID proposal, device matrix
- Created `docs/ios-testflight-readiness.md` (CMake generation, signing, device setup)
- Created `app/ios/build_number.txt` (initialized to 0)
- Decisions merged to `decisions.md` (Status: PROPOSED, 5 user-provided blockers documented)
- Comments posted to all five issues
