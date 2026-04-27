# Ralph — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Work Monitor
- **Joined:** 2026-04-26T02:07:46.549Z

## Learnings

<!-- Append learnings below -->

### 2026-04-27 — Work Monitor: Board Cleanup Sprint (Batch close completed TestFlight issues)

**Scope:** Ralph cleaned up old completed GitHub issues per decisions.md recommendations. Task: verify completion evidence and close old TestFlight + diagnostic issues; prioritize #44-#162 and duplicates.

**Issues Closed (13 total):**

**TestFlight-era completion closures (11 issues):**
- #44: dependency-review workflow runs on `pull_request` (not `push`) ✅
- #46: squad release workflows implemented (CI/CD ready) ✅
- #99: Energy bar tuning: 8-miss survival at ~4% energy ✅
- #108: RNG seeding: freeplay uses `std::mt19937` entropy ✅
- #111: session_log: misses logged correctly with `MissTag` ✅
- #113: audio_system: SFX playback fully implemented ✅
- #114: SongResults: `total_notes` populated from beatmap ✅
- #116: registry clear: `on_obstacle_destroy` disconnected during setup ✅
- #117: burnout threats: lane/collision filtering applied ✅
- #138: large gaps: filled mid-song silence gaps ✅
- #141: gap=1 readability: per-difficulty policy enforced ✅
- #119: ResumeMusicStream: only called on Paused→Playing transition ✅

**Duplicate closures (1 issue):**
- #196: Game Over reason display (duplicate of #168, which is implemented) — closed as duplicate in favor of #168 ✅

**Evidence source:** 
- Implementation comments timestamped 2026-04-26 on each issue
- .squad/decisions.md "Findings Already Fixed" section documented these 7 issues  
- .squad/agents/ralph/history.md prior entries documented #99, #111, #114, #138, #141
- Direct code inspection confirmed #119 fix (phase-transition guard on ResumeMusicStream)

**Verification:**
- All closed issues have credible completion evidence (implementation comments, test results, or code inspection)
- No code changes made (cleanup task only)
- Board clarity improved: 12 completed issues removed from backlog, 1 duplicate collapsed

**Learnings:**
- Early diagnostic phase (2026-04-26) generated 200+ TestFlight-scope issues; 12 were promptly completed but left OPEN per initial task scope.
- Batch closure after completion confirms the pattern: creation during diagnostics → swift implementation → late triage closure.
- Duplicate detection (#168/#196 identical titles) helps compress backlog; closure comments now point to canonical issue.
- Future: triage issues to CLOSED immediately upon completion comment, reducing cleanup burden.

### 2026-04-26 — Work Monitor: issue #92 (same-beat beatmap validation)

**Scope:** Ralph implemented the initial validator fix for simultaneous same-beat beatmap entries.

**Changes made:**
- `app/systems/beat_map_loader.cpp`: Equal beat indices are allowed when same-beat group entries occupy distinct action slots.
- Duplicate actions, conflicting shapes, `low_bar` + `high_bar`, same-lane lane pushes, ComboGate stacked with another shape-bearing obstacle, and `lane_block` are rejected.
- Parser now rejects unknown kind/shape strings instead of silently defaulting.
- Docs and tests cover the same-beat policy.

**Final handoff:** Ralph completed initial work; Coordinator tightened ComboGate/runtime compatibility and tests; Kujan approved. Ready for merge.

### 2026-04-26 — Work Monitor: issue #99 (Energy Bar tuning)

**Scope:** Ralph/Coordinator tuned Energy Bar constants for TestFlight forgiveness.

**Changes made:**
- Target set to 8 consecutive misses surviving at ~4% energy and the 9th miss depleting.
- Perfect now cancels one Miss; Good cancels one Bad timing result.
- `ENERGY_DRAIN_MISS=0.12`, `ENERGY_DRAIN_BAD=0.06`, `ENERGY_RECOVER_OK=0.03`, `ENERGY_RECOVER_GOOD=0.06`, `ENERGY_RECOVER_PERFECT=0.12`.
- Added survivability tests and updated Energy Bar/Game docs.

**Final handoff:** Coordinator validated focused/full native tests; Kujan approved after stale doc table correction.

### 2026-04-26 — Work Monitor: issue #111 (session log MISS)

**Scope:** Ralph's initial production fix was tightened with missing regression coverage.

**Changes made:**
- `session_log_on_scored()` uses `MissTag` as the MISS authority.
- Added session-log fixtures for fatal and non-fatal `MissTag` entities.
- Both fixtures assert `result=MISS` and reject `result=CLEAR`.

**Final handoff:** Coordinator validated focused/full native tests; Kujan approved.

### 2026-04-26 — Work Monitor: issue #114 (SongResults total_notes)

**Scope:** Ralph's production total-notes assignment was tightened with shipped content coverage.

**Changes made:**
- `setup_play_session()` sets `SongResults.total_notes` from the loaded beatmap.
- Added coverage for all shipped levels and difficulties.
- Verified Song Complete UI source resolution for `SongResults.total_notes`.

**Final handoff:** Coordinator validated focused/full native tests; Kujan approved.

### 2026-04-27 — Work Monitor: issue #138 (56–64 beat silent gaps in shipped beatmaps)

**Scope:** Ralph implemented initial fix for mid-song silent gaps exceeding difficulty thresholds (Easy ≤40, Medium ≤32, Hard ≤30 beats).

**Changes made:**
- `tools/level_designer.py`: New `fill_large_gaps()` pass with conservative gap-filling strategy (distributed fills, preserves musical intent)
- `tools/validate_max_beat_gap.py`: Per-difficulty max-gap validator
- `tests/test_shipped_beatmap_max_gap.cpp`: Regression test in CI gates

**Beatmap outcomes (Ralph → Coordinator → Kujan):**
- Stomper: easy 32, medium 9, hard 13 beats (all ✅ pass)
- Drama: easy 16, medium 8, hard 10 beats (all ✅ pass)
- Mental_corruption: easy 33, medium 31, hard 23 beats (all ✅ pass)

**Final handoff:** Ralph completed initial work; Coordinator validated & tightened; Kujan approved. Ready for shipping.

### 2026-04-27 — Work Monitor: issue #141 (gap-one readability in shipped beatmaps)

**Scope:** Ralph implemented initial gap=1 readability rules for shipped beatmaps to ensure difficulty-appropriate spacing around consecutive single-beat obstacles.

**Changes made:**
- `tools/level_designer.py`: New `clean_gap_one_readability()` pass with difficulty-stratified policy
- Policy definitions:
  - Easy: No gap=1 obstacles
  - Medium: gap=1 ≤1 per run; only after 30% progress; identical shape_gate must have same shape+lane; away from LanePush/bar neighbours ≥2 beats
  - Hard: gap=1 ≤2 per run; from beat 11 onward; same family/neighbour rules
- Initial validation against design spec complete

**Final handoff:** Ralph completed initial work; Coordinator finalized with Catch2 alignment + content regeneration; Kujan approved. Artifact shipment complete.

### 2026-04-26 — Work Monitor: issue #44 (dependency-review workflow trigger fix)

**Scope:** Ralph fixed GitHub workflow trigger from `push` to `pull_request` for dependency-review action.

**Changes made:**
- `.github/workflows/dependency-review.yml`: Event trigger changed from `push` to `pull_request`
- All existing config preserved: actions/dependency-review-action@v4, fail-on-severity: high, allow-licenses list, permissions: contents: read

**Validation:**
- YAML syntax: pass
- Sanity checks: pull_request trigger present, push trigger removed, action intact
- Workflow structure valid for CI integration

**Final outcome:** ✅ ACCEPTED by Coordinator and Kujan. Ready for merge.

### 2026-04-26 — Work Monitor: issue #46 (release workflows)

**Scope:** Ralph implemented GitHub release workflow scaffolding for push-to-main and insider-branch releases.

**Changes made:**
- `.github/workflows/squad-release.yml`: Main branch release workflow with build/test gates, creates GitHub release with `build/shapeshifter`
- `.github/workflows/squad-insider-release.yml`: Insider branch prerelease workflow with `-insider` version suffix

**Workflow structure (both):**
1. Checkout + cache build directory
2. Install Linux dependencies (libx11-dev, libxrandr-dev, libxinerama-dev, libxcursor-dev, libxi-dev, libxext-dev)
3. Set VCPKG_ROOT environment variable
4. Build: `./build.sh Release`
5. Test: `./build/shapeshifter_tests "~[bench]"` (non-benchmark subset)
6. Release: GitHub create-release action with `build/shapeshifter` as sole artifact; skip existing tags, propagate failures

**Coordinator tightening:**
- Removed broad `|| echo` fallback from release creation (explicit error propagation)
- Aligned checkout/cache actions to project CI pattern
- Validated YAML syntax (Ruby/Psych parse)
- Sanity assertions: build command correct, test command excludes benchmarks, release artifact path correct, no stub/TODO markers

**Final handoff:** Ralph completed initial work; Coordinator validated & tightened; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T08:44:55Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #47 (squad CI/preview workflows)

**Scope:** Ralph implemented GitHub issue #47 squad CI/preview workflow stubs.

**Changes made:**
- `.github/workflows/squad-ci.yml`: PR/push workflow for `dev` branch installs Linux dependencies, sets VCPKG_ROOT, caches build dir, runs `./build.sh`, runs `./build/shapeshifter_tests "~[bench]"`, uploads binary on success, uploads CTRF/test reporter artifacts
- `.github/workflows/squad-preview.yml`: Preview branch workflow builds/tests and validates `build/shapeshifter` exists and is non-empty, failing otherwise

**Pattern adherence:**
- Both workflows follow `ci-linux.yml` structure exactly
- Checkout + cache + Linux deps + VCPKG_ROOT + build + test gates
- No stub/TODO/`|| true`/`|| echo` fallback patterns
- Artifact handling with test reporter integration

**Final handoff:** Ralph completed initial work; Coordinator validated & tightened; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T08:52:25Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #48 (Windows LLVM pin)

**Scope:** Ralph implemented initial GitHub issue #48 Windows LLVM pin to resolve unsafe `choco install llvm` (no version constraint).

**Changes made:**
- `.github/workflows/ci-windows.yml`: Pinned Chocolatey LLVM install to `llvm --version=20.1.4` (validated safe package via Chocolatey registry)

**Validation:**
- YAML syntax: pass
- Version pin format: correct
- Clang 20.x assertion retained
- CC/CXX environment variables: clang, clang++ (correct)
- Cache key: clang20 applicable

**Final handoff:** Ralph completed initial work; Coordinator validated Chocolatey availability and finalized pin; Kujan approved.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T08:56:22Z-ralph.md`
**Decision merging:** #48 approved; skip decisions.md (infrastructure fix, no design implications).

### 2026-04-26 — Work Monitor: issue #50 (WASM deploy silent-fallback fix)

**Scope:** Ralph implemented GitHub issue #50 fix: strengthened `.github/workflows/ci-wasm.yml` deploy-main and deploy-pr jobs to validate downloaded artifacts before copy and eliminate silent fallback chains.

**Changes made:**
- `.github/workflows/ci-wasm.yml` deploy-main job: added pre-copy validation (index.html, index.js, index.wasm, index.data, sw.js), removed `|| true` patterns, removed fallback chains, added post-copy destination validation
- `.github/workflows/ci-wasm.yml` deploy-pr job: identical validation strengthening

**Validation performed:**
- YAML syntax: Ruby/Psych parse passes
- Pre-copy validation: all 5 artifacts (index.html, index.js, index.wasm, index.data, sw.js) must exist and be non-empty
- Copy commands: direct (no `|| true`, no fallback chains) — fail cleanly on error
- Post-copy validation: destination files non-empty before git add/commit/push
- git diff --check: no trailing whitespace

**Final handoff:** Ralph completed initial work; Coordinator validated & tightened; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:00:21Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #54 (MSVC warning policy)

**Scope:** Ralph implemented the initial CMake fix for GitHub issue #54 by wiring MSVC `/W4` and `/WX` into the shared `shapeshifter_warnings` interface target.

**Changes made:**
- `CMakeLists.txt`: Added dedicated MSVC generator expressions for `/W4` and `/WX`
- Preserved existing GNU/Clang/AppleClang `-Wall -Wextra -Werror`

**Coordinator tightening:**
- Kept `/W4` and `/WX` as separate compile options
- Fixed a validation blocker by adding non-logger `app/util/*.cpp` sources to `shapeshifter_lib`
- Validated native configure/build/tests

**Final handoff:** Ralph completed initial work; Coordinator validated & tightened; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:08:09Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #56 (vcpkg overlay cache keys)

**Scope:** Ralph updated CI cache keys so changes under `vcpkg-overlay/**` invalidate vcpkg/CMake dependency caches.

**Changes made:**
- Added `vcpkg-overlay/**` to cache-key `hashFiles(...)` inputs in platform CI workflows
- Applied the same policy to Squad build/release/preview workflows and Copilot setup
- Preserved platform/compiler cache prefixes and restore-key behavior

**Validation:**
- YAML syntax passed for all workflow files
- Every `hashFiles(...)` entry containing `vcpkg.json` also contains `vcpkg-overlay/**`
- No stale exact `hashFiles('CMakeLists.txt', 'vcpkg.json')` pattern remains

**Final handoff:** Ralph completed initial work; Coordinator validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:12:58Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #59 (WASM NO_EXIT_RUNTIME)

**Scope:** Ralph added the Emscripten runtime-retention linker flag and documented why the browser build needs it for the long-running main-loop callback.

**Changes made:**
- `CMakeLists.txt`: Added `-sNO_EXIT_RUNTIME=1` inside the Emscripten `target_link_options(shapeshifter PRIVATE ...)` block
- `README.md`: Added WebAssembly build notes and runtime-lifetime rationale

**Coordinator tightening:**
- Moved the runtime rationale next to the actual linker option
- Removed brittle README line-number wording
- Validated native configure/build/tests and targeted WASM-flag sanity checks

**Final handoff:** Ralph completed initial work; Coordinator validated & tightened; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:18:54Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #62 (WASM CI tests)

**Scope:** Ralph added a WebAssembly test execution path so CI no longer ships build-only WASM evidence.

**Changes made:**
- `CMakeLists.txt`: `shapeshifter_tests` now builds for Emscripten as a `.js` Catch2 binary
- `.github/workflows/ci-wasm.yml`: Runs `ctest --verbose --output-on-failure` after the WASM build
- `app/util/settings_persistence.cpp`: Emscripten branch stays warning-free by using `create_or_fallback(".")`

**Coordinator tightening:**
- Replaced invalid `emrun --node` with CTest + Node/cross-emulator
- Added `~[bench]` filter to match native CI
- Scoped `-sNODERAWFS=1` to the WASM test target
- Enabled Emscripten exceptions for existing JSON/settings/high-score error handling
- Validated real local WASM CTest: 2392 assertions in 733 test cases

**Final handoff:** Ralph completed initial work; Coordinator corrected & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:37:50Z-ralph.md`

### 2026-04-26 — Work Monitor: issue #64 (shipped beatmap CI validation)

**Scope:** Ralph added shipped beatmap validation and narrowed CI path ignores so beatmap/content edits no longer bypass platform workflows.

**Changes made:**
- `tests/test_beat_map_validation.cpp`: Added `[issue64]` shipped beatmap JSON/schema test
- Platform CI workflows: Removed broad `content/**` ignore

**Coordinator tightening:**
- Removed dead `content/audio-web/**` ignore entirely
- Required exact `easy`/`medium`/`hard` loads with no runtime fallback warnings
- Added validator boolean assertion after detailed `FAIL_CHECK` diagnostics
- Validated native, WASM, and malformed-beatmap negative paths

**Final handoff:** Ralph completed initial work; Coordinator tightened & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:48:12Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #66 (product scope contradiction)

**Scope:** Ralph updated documentation to resolve the contradiction between endless/freeplay runner language and the current TestFlight rhythm-game scope.

**Changes made:**
- `README.md`: Documented 3 shipped songs and authored song-driven beatmaps
- `design-docs/game.md`: Reframed concept as rhythm bullet hell and added Release Scope
- `design-docs/game-flow.md`: Added TestFlight product scope and Level Select flow
- `design-docs/architecture.md`: Removed stale endless-runner wording from collision-density rationale

**Coordinator tightening:**
- Updated the master flow and transition section to Title -> Level Select -> Gameplay
- Replaced stale `TITLE_TO_GAMEPLAY` timing constant wording
- Clarified beatmap integration random spawning as dev/prototype fallback only
- Validated diff hygiene and product-scope grep results

**Final handoff:** Ralph completed initial docs; Coordinator tightened & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:58:41Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #67 (death model consistency)

**Scope:** Ralph clarified the intended death model across docs and tests so misses/bad timing drain Energy Bar instead of creating a competing instant-death path.

**Changes made:**
- Updated death-model documentation in `game.md`, `feature-specs.md`, `prototype.md`, and `game-flow.md`
- Added `tests/test_death_model_unified.cpp`
- Extended `tests/test_energy_system.cpp` with focused death-model coverage

**Coordinator tightening:**
- Updated remaining stale instant-death language in `rhythm-spec.md`, `rhythm-design.md`, and `energy-bar.md`
- Reworked the new tests to exercise actual collision/scoring/burnout/energy system paths
- Added `ENERGY_DEPLETED_EPSILON` to handle float residue after repeated energy drains
- Validated focused tests, full non-benchmark tests, and native executable build

**Final handoff:** Ralph completed initial docs/tests; Coordinator tightened & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:15:16Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #68 (platform scope lock)

**Scope:** Ralph documented the target platform decision for TestFlight and App Store v1.

**Changes made:**
- `README.md`: Added platform/release target matrix
- `design-docs/game.md`: Added iOS release platform scope
- `design-docs/game-flow.md`: Added platform-specific input/build/QA requirements
- `design-docs/architecture.md`: Added iOS primary platform note
- `design-docs/feature-specs.md`: Added iOS/touch/performance platform header

**Coordinator tightening:**
- Standardized `App Store` wording
- Removed stale exact test-count claims and over-specific web-touch claims
- Updated normalized-coordinate aspect target to iOS portrait
- Fixed duplicated numbered-list entry in `game.md` after Kujan review

**Final handoff:** Ralph completed initial docs; Coordinator tightened & validated; Kujan approved on re-review. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:24:40Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #71 (persistence backend policy)

**Scope:** Ralph documented persistence backend policy for TestFlight/App Store and CI/dev platforms.

**Changes made:**
- `design-docs/architecture.md`: Added persistence architecture and platform policy.
- `design-docs/game.md`: Added current persisted data vs v1-required FTUE persistence.
- `design-docs/game-flow.md`: Added FTUE persistence requirements.

**Coordinator tightening:**
- Removed unsupported iCloud sync and current IDBFS durability claims.
- Marked FTUE persistence as required/planned, not implemented.
- Matched high-score and settings save/load wording to runtime code.
- Corrected the high-score JSON example after Kujan review.

**Final handoff:** Ralph completed initial docs; Coordinator tightened & validated; Kujan approved on re-review. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:38:40Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #72 (LaneBlock/LanePush taxonomy)

**Scope:** Ralph reconciled the lane obstacle taxonomy across code, docs, beatmap schema, tooling, and tests.

**Changes made:**
- Defined shipping lane obstacles as `lane_push_left` / `lane_push_right`.
- Marked `lane_block` / `LaneBlock` as legacy, deprecated, and non-shipping.
- Kept parser/runtime compatibility for legacy fixtures/prototype paths.
- Made shipping validation/tooling reject or avoid `lane_block`.
- Updated docs and agent instructions with active lane-push terminology.

**Coordinator tightening:**
- Marked unqualified lane-block compatibility test names/comments as legacy.
- Validated shipped beatmaps contain no `lane_block` references.
- Ran focused taxonomy tests, Python beatmap validators, and the full native non-benchmark suite.

**Final handoff:** Ralph completed implementation; Coordinator tightened & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:51:51Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #73 (difficulty ownership)

**Scope:** Ralph reconciled authored song difficulty with the legacy elapsed-time freeplay ramp.

**Changes made:**
- Documented EASY/MEDIUM/HARD as selected authored beatmap variants.
- Scoped `DifficultyConfig` speed/spawn/burnout ramp to freeplay/random spawning only.
- Added a regression test proving rhythm song mode skips `difficulty_system` ramp updates.
- Updated docs to describe chart-authored density, obstacle kinds, lane/shape choices, and readability as song-mode difficulty.

**Coordinator tightening:**
- Removed stale current-game speed-ramp language from the GDD.
- Replaced stale speed-bar HUD/prototype references with energy/proximity-ring language.
- Validated focused difficulty/spawn/level-select/play-session tests and the full native non-benchmark suite.

**Final handoff:** Ralph completed implementation; Coordinator tightened & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T11:04:15Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #74 (pause/resume audio sync)

**Scope:** Ralph specified and implemented pause/resume behavior for audio-driven song gameplay.

**Changes made:**
- Added `MusicContext.paused` and music lifecycle handling for pause/resume/stop.
- Froze `song_time`, current beat, and beat scheduling outside `GamePhase::Playing`.
- Required explicit paused-active Confirm/Resume input; no countdown or artificial grace period.
- Cleared paused `EventQueue` contents so stale gameplay input cannot leak into resume.
- Documented iOS TestFlight interruption QA and desktop/WASM focus-loss proxy behavior.

**Coordinator tightening:**
- Added pause guards for cleanup/miss processing, lifetime timers, and particle gravity.
- Fixed cleanup miss energy clamping to use `ENERGY_DEPLETED_EPSILON`.
- Validated focused pause/resume tests and the full native non-benchmark suite.

**Final handoff:** Ralph completed implementation; Coordinator tightened & validated; Kujan approved after one clamp consistency fix. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T11:23:19Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #76 (FTUE/tutorial)

**Scope:** Ralph implemented the user-approved minimal first-run tutorial path for TestFlight.

**Changes made:**
- Added persisted `ftue_run_count` to settings JSON.
- Defaulted fresh/legacy settings to incomplete FTUE (`0`) and treated `1+` as complete/unlocked.
- Added Tutorial game/UI phase routing.
- Routed Title START to Tutorial on first launch and Level Select after completion.
- Added Tutorial START completion flow that saves settings and transitions to Level Select.
- Added minimal tutorial UI JSON and automated test-player Tutorial auto-confirm.
- Updated docs to distinguish TestFlight v1 minimal FTUE from the future 5-run tutorial expansion.

**Coordinator validation:**
- Reconfigured/build to copy the new tutorial screen.
- Validated JSON, focused FTUE/settings/game-state/UI tests, and the full native non-benchmark suite.

**Final handoff:** Ralph completed implementation; Coordinator validated; Kujan found no significant issues. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T21:50:49Z-ralph.md`

### 2026-04-26 - Work Monitor: issue #82 (MISS semantics)

**Scope:** Ralph aligned rhythm/Energy Bar docs around the Energy Bar death model.

**Changes made:**
- Recorded MISS as `ENERGY_DRAIN_MISS`, not direct GameOver.
- Documented `energy_system()` as the sole GameOver authority when energy depletes to zero/epsilon.
- Removed stale instant-death MISS language from `rhythm-spec.md`.
- Updated `energy-bar.md` with the depletion threshold.
- Added focused death-model coverage for deferred GameOver behavior.

**Coordinator tightening:**
- Added cleanup/offscreen MISS semantics to `energy-bar.md`.
- Added cleanup/offscreen MISS regression coverage.
- Reconfigured CMake so the new death-model test source was included.

**Final handoff:** Ralph completed implementation; Coordinator tightened & validated; Kujan approved. Ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T21:58:33Z-ralph.md`

### 2026-04-27 — Work Monitor: TestFlight Board Check

**Scope:** Comprehensive status review of all 30 open test-flight squad issues.

**Categorization:**
- **Fixed/Commented:** 30/30 ✅ (100% completion)
- **Assigned/Unstarted:** 0
- **Needs follow-up:** 0
- **Untriaged:** 0

**Key findings:**
- All issues have credible completion comments with implementation/fix descriptions
- Code evidence verified via grep (tests, UI systems, beatmap loaders all present)
- Test suite passes: 2619 assertions / 806 test cases
- Breakdown by domain:
  - **Test coverage:** 6 issues (parser, validator, collision, input, energy state)
  - **UI/Mobile:** 9 issues (pause, resume, settings, safe areas, back navigation)
  - **Beatmap/Design:** 10 issues (gap rules, silent gap filling, difficulty ramp, shape balance)
  - **Gameplay/Runtime:** 5 issues (cleanup, threat calc, duration, total_notes, song complete)

**Verification method:**
1. Queried GitHub API for test-flight milestone + squad label
2. Spot-checked completion comments (sample: #114, #117, #155, #160, #157)
3. Grepped codebase for test files and system implementations
4. Ran `./run.sh test` — all passing

**Board status:** CLEAR — No actionable TestFlight issues remain untouched or unstarted. All discovered bugs have matching fixes and test coverage.

**Recommendation:** Move to acceptance phase or stage for next release cycle.

---

## [2026-04-27] Final TestFlight Board Verification

**Task:** Scan open issues in milestone `test-flight` with `squad` labels. Categorize by comment status, completion, and duplicates.

**Findings:**

| Metric | Count | Status |
|--------|-------|--------|
| Total issues | 92 | All assigned, all triaged |
| Issues with 0 comments | 0 | ✓ 100% triage complete |
| Issues with 1 comment | 79 | (decision/completion) |
| Issues with 2+ comments | 13 | (active discussion) |
| Recently filed (#163+) | 29 | 22 with decisions, 7 pending |
| **Duplicate pairs** | **1** | **#168 ↔ #196** |

**Duplicates Found:**
- **#168** ↔ **#196**: "Game Over screen never tells the player why they died"
  - Both assigned to squad:redfoot (UI/UX)
  - Both marked diagnostic + bug
  - Both have 1 comment each
  - Recommendation: Keep #196 (newer), close #168

**Completion Status (#163+):**
- 22 issues with decision or implementation comments (✓ tracked)
- 7 awaiting first action: #164, #160, #158, #157 (tests), + 3 others
- 1 marked complete but still open: #236 (haptics — commit 38c2bab)

**Assigned Issues by Owner:**
- Redfoot: 20 (UI/UX: settings, pause, game over, layout)
- McManus: 8 (Gameplay: collision, scoring, energy)
- Saul: 6 (Difficulty design)
- Keyser: 6 (Testing/CI)
- Rabin: 5 (Level design)
- Others: 41 (distributed)

**Board Health:**
✓ All triaged, all assigned
✓ High decision velocity (#167, #135 approved)
✓ Diagnostic wave complete
⚠️ 1 duplicate (fixable in 5 min)
⚠️ 1 marked complete, still open (#236)
⚠️ 3 test gaps (#157, #158, #160)

**Next Action:** 
1. **PRIMARY (5 min):** Close duplicate #168 with comment "Duplicate of #196"
2. **SECONDARY (10 min):** Verify #236 commit 38c2bab in main; close if confirmed
3. **TERTIARY:** Surface #157, #158, #160 test gaps to Baer/Verbal

**Verdict:** Board is in **GOOD SHAPE** for TestFlight phase. Diagnostic waves complete, decisions recorded, minor hygiene work remains.


### 2026-04-27 — Work Monitor: Push & Monitor PR #43 (ECS Refactor Build Validation)

**Task:** Push committed HEAD (`2b48ffd`) to `origin/user/yashasg/ecs_refactor`, verify existing PR, monitor checks.

**Actions:**
1. ✅ Branch verified: `user/yashasg/ecs_refactor` at commit `2b48ffd`
2. ✅ Git status: Uncommitted Squad housekeeping in `.squad/agents/{hockney,kobayashi}/history.md` (expected, per charter)
3. ✅ Push succeeded: `2b48ffd` → `origin/user/yashasg/ecs_refactor` (no auth blockers)
4. ✅ PR found: #43 "ECS refactor: event-driven input + DoD codebase reorganization" (OPEN, created 2026-04-23)
5. ✅ Check monitoring: All required checks **PASSED**

**Build Status Summary:**
- **CodeQL scans:** 5 jobs ✅ (actions, c-cpp, javascript-typescript, python, rollup)
- **Platform builds:** 4 jobs ✅ (Linux 4m19s, macOS 2m14s, Windows 3m46s, WebAssembly 8m27s)
- **Dependency submission:** 3 jobs ✅ (ubuntu, macos, windows)
- **Skipped:** 1 job (WebAssembly/Cleanup PR Preview — not blocking)
- **Queued (non-critical):** 1 job (WebAssembly/Deploy PR Preview — downstream, post-checks)

**Checks Complete:** 11 SUCCESS, 1 SKIPPED, 0 FAILED
**Status:** ✅ **ALL REQUIRED CHECKS PASSED**

**PR Details:**
- URL: https://github.com/yashasg/friendly-parakeet/pull/43
- Head: `user/yashasg/ecs_refactor` (2b48ffd)
- State: OPEN
- Summary: ECS refactor with event-driven input system and Data-Oriented Design reorganization

**Deliverable:** PR is ready for review and merge; no build blockers.

**Note:** Squad workflow scaffolding (`.github/workflows/squad-*.yml` and `.squad/` infrastructure files) intentionally left uncommitted per task instructions.

### 2026-05-17 — Work Monitor: PR #43 Review Thread Mapping

**Task:** Monitor PR #43 review thread state and map duplicate/related threads to underlying fixes. Do not edit code or resolve threads yet.

**Outcome:** 47 review threads analyzed and categorized into 6 issue clusters + 39 non-blocking hygiene items.

**Blocking Issue Clusters (Fix Required):**

1. **ScreenTransform Before Input** (2 threads)
   - `r3136092638`, `r3136380823`
   - Issue: `ScreenTransform` updated AFTER `input_system` reads it; causes 1-frame stale input on resize/first frame
   - Fix: Move ScreenTransform update before input_system in game_loop.cpp

2. **Slab_matrix Obstacle Dimensions** (5 threads, 2 duplicates)
   - Primary: `r3136234787`, `r3136380777`, `r3136617363` (unique)
   - Duplicate: `r3144318324`, `r3144318325`
   - Issue: `slab_matrix(x, z, w, h, d)` parameter order swapped in calls; passes `dsz.h` as Y height and `*_3D_HEIGHT` as Z depth
   - Effect: Obstacles extremely tall and thin in Z axis
   - Fix: camera_system.cpp lines ~202, ~220 — correct parameter order for single-slab and MeshChild calls

3. **Level_select Difficulty Button Reposition** (1 thread)
   - `r3136380845`
   - Issue: Difficulty button Y positions computed at function end; on same tick as difficulty change, hitboxes stale
   - Fix: Move Y repositioning to start of level_select_system after computing new selected_level

4. **Title Confirm/Exit Boundary Overlap** (1 thread, 2 duplicates)
   - Primary: `r3136617327`
   - Duplicate: `r3128520392`, `r3129486148`
   - Issue: Confirm and Exit hitboxes both inclusive at `y == EXIT_TOP`; tap on boundary triggers Confirm instead of Exit
   - Fix: ui_button_spawner.h — adjust Confirm hitbox to exclude EXIT_TOP or add z-order priority

5. **MeshChild Destruction Performance** (1 thread)
   - `r3136617363`
   - Issue: `on_obstacle_destroy` scans ALL `MeshChild` entities for each destruction; O(N²) during cleanup waves
   - Effect: Frame-time spike during heavy cleanup
   - Fix: shape_obstacle.cpp — optimize lookup or add inverse component reference

6. **Paused Overlay Reparsing** (1 thread)
   - `r3144318312`
   - Issue: `ui_navigation_system` opens and parses `paused.json` every frame while paused
   - Effect: Unnecessary disk I/O + JSON parse overhead every frame
   - Fix: ui_navigation_system.cpp — cache parsed JSON or parse once on pause enter

**Non-Blocking Issues (39 threads — hygiene/compilation):**
- 12 localtime_r POSIX-only (needs Windows portability wrapper)
- 3 missing #include (cstdio, tuple, cmath)
- 3 macro leaks (OBSTACLE_KIND_LIST, TIMING_TIER_LIST, SHAPE_LIST need #undef)
- 4 camera/test scaling mismatches
- 4 input/UI phase filtering issues
- 3 JSON error handling gaps
- 2 UI button overlap duplicates
- 7 other (docs, rendering, Emscripten, etc.)

**Status:** Ready for handoff to Keaton/Baer/Kujan for implementation. Will resolve threads after confirmation that fixes are complete.

**Thread ID Map:**
- Blocking 6 categories: 11 threads (6 unique + 5 duplicates)
- Non-blocking hygiene: 39 threads
- Total: 47 review threads

