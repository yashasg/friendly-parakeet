# Kobayashi — History


## Release Log — 2026-05-10

**PR Integration & Merge (Release Cycle)**
- **Task:** Integrate PR #378 (beatmap quality diagnostics) and PR #379 (onset-layer tuning) and merge to main
- **Approach:** Created integration branch `beatmap/integrated-quality-onset` from `origin/main`, merged both PR branches, validated with full test suite (1848 assertions passed)
- **Validation:** All tests passed locally; builds successful on all supported platforms via CI
- **Result:** 
  - PR #380 created and squash-merged to main (commit d65c4e7)
  - Integration branch deleted automatically
  - Original PRs #378 and #379 closed with summary comment
  - 128 files changed, 200k+ insertions across beatmap content, diagnostics, and test infrastructure
  
**Key Stats:**
- Commits integrated: 13 (quality-improvements) + 15 (onset-obstacle-spike) 
- Files in diff: 128 changed
- Diagnostics artifacts added: 42 CSV/JSON files
- Test coverage: All 687 test cases pass
- CI checks: Dependency review, squad CI, CodeQL all passing

**Worktree State:** Clean, on main branch (origin/main tracking updated)

## Learnings

- 2026-05-10T15:43:34.669-07:00 — **Squad loop tool permissions investigation:** No project-wide configurable permission policy found for `touch` or `apply_patch` in Ralph/Squad heartbeat or Squad CLI. GitHub Actions `permissions.contents: write` (set in commit 38b0847) enables repository file modifications (git commits, pushes). `touch` is a shell operation that works in the checked-out workspace without additional GitHub Actions scope; file persistence requires git commit + push logic (already in place in heartbeat template). `apply_patch` is a Copilot tooling capability, not a GitHub Actions permission scope — no YAML configuration grants it. Skill files (e.g., `content-root-consolidation/SKILL.md`) document `apply_patch` as a tool reference for agents' domain knowledge, not as a permission grant mechanism. Casting policies and MCP configs do not define tool allowlists. Conclusion: `contents: write` is the closest already-applied fix. No code changes needed; Squad loop already has the foundation to persist file edits if triggered agents apply git commits.
- 2026-05-10T15:35:04.939-07:00 — Granted Ralph/Squad heartbeat repository content write permission so the loop can push commits or modify repo files, while retaining existing issue write and pull-request read scopes. Touched `.github/workflows/squad-heartbeat.yml` and `.squad/templates/workflows/squad-heartbeat.yml`; referenced template paths `templates/workflows/squad-heartbeat.yml` and `packages/squad-cli/templates/workflows/squad-heartbeat.yml` are not present in this checkout.
- Integration branches work well for multi-PR consolidation; prefer squash-merge to keep main history clean
- CodeQL scanning adds ~30s latency to PR merge workflow; consider pre-validation locally for large changes
- Beatmap quality and onset tuning changes are data-driven (JSON configs + CSV diagnostics); changes don't affect C++ core logic


## Audit Loop Validation — 2026-05-10

- Ran full validation on `audit/autonomous-quality-loop` @ `45bcef6`:
  - `cmake -B build -S . -Wno-dev`
  - `cmake --build build`
  - `./build/shapeshifter_tests`
  - `./run.sh test`
- Result: all validation steps passed.
- Reconciled issues #382-#407:
  - Closed fixed issues #382-#386, #388-#407 with commit-linked close comments.
  - Left #387 open with explanation because `design-docs/game-flow.md` still contains Section 2b burnout-meter references.
- Release follow-up: prepared branch for PR to `main` after issue reconciliation.

## PR #408 CI Fix — 2026-05-10

- **Symptom:** CI Linux/Windows/macOS build jobs failed simultaneously on branch `audit/autonomous-quality-loop` at head `413b3b5`.
- **Root cause:** `app/systems/collision_system.cpp` used raylib `Rectangle`/`CheckCollisionRecs` without directly including `<raylib.h>`, relying on fragile transitive include behavior.
- **Fix:** Added explicit `#include <raylib.h>` to `collision_system.cpp` (commit `a80276c`).
- **Validation:** Targeted build (`shapeshifter_lib`) plus full validation command passed locally; tests: 1932 assertions in 714 cases.
- **Release learning:** In this zero-warning, multi-platform pipeline, any TU using raylib value types/functions must include `<raylib.h>` directly to avoid platform/toolchain-dependent compile breaks.

## Mechanical Cleanup — Beatmap Editor Docs Fix — 2026-05-10

- **Task:** Fix malformed palette example in `design-docs/beatmap-editor.md` §3.4 with duplicated `SplitPath` entry
- **Root cause:** Palette UI mockup had `[⑂ SplitPath]` on two consecutive lines (147–148) instead of one
- **Fix applied:** Removed duplicate line 148; palette now correctly shows `[■ ShapeGate] [⑂ SplitPath]` once with Shape options below
- **Validation:** `git diff --check` passed (zero trailing whitespace); no C++ changes required
- **Status:** Not committed per task instructions; ready for final review

- 2026-05-10T16:03:00.125-07:00 — Squad loop CI unblocked by replacing echo-only Squad CI/Preview workflows with real Linux native and WASM preview build/test gates, updating deprecated github-script actions to v8, and adding `vcpkg-overlay/**` to native cache hashes so overlay changes invalidate restored build directories.

- 2026-05-10T16:03:00.125-07:00 — Release placeholder workflows should mirror active repo validation gates: native release builds use the Squad CI Linux build/test path, insider artifacts use the Squad Preview WASM build/test/artifact validation path, and all CMake/vcpkg caches must hash `vcpkg-overlay/**` with `vcpkg.json`.

- 2026-05-10T18:10:33.662-07:00 — Reconciled Squad loop installed workflow templates with active repo automation: heartbeat template now matches event/manual trigger policy, `contents: write` parity, and `actions/github-script@v8`; related installed Squad workflow templates no longer pin stale github-script v7. Boundary confirmed: repository workflows can grant GitHub token scopes, but cannot grant Copilot/runtime UI permissions such as `apply_patch` or shell file-write tool allowlists.
- 2026-05-11T13:26:11.112-07:00 — Opened PR https://github.com/yashasg/friendly-parakeet/pull/704 from `squad/restore-stashed-squad-state` to `main` after committing restored dirty branch state (`7d5f0bd`). Validation run: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests` ✅ passed (`2313 assertions in 793 test cases`). Intentionally excluded local-only runtime control files from commit: `.squad/ralph-circuit-breaker.json`, `.squad/ralph-stop`.
- 2026-05-11T12:54:54.442-07:00 — Open PR bundle integration on branch `squad/open-pr-bundle-2026-05-11` in worktree `/Users/yashasgujjar/dev/friendly-parakeet-open-pr-bundle` merged PRs in ascending order: #664, #665, #666, #668, #669, #670, #671, #672, #673, #674, #675, #677, #678, #679, #680, #681, #685, #686, #687, #688, #689, #690, #691, #692, #693, #694, #695, #696, #697, #698, #699, #700, #701, #702.
- 2026-05-11T12:54:54.442-07:00 — Conflict resolutions applied for #668/#669/#671 (duplicate popup-width cache PRs, kept already-integrated implementation), #677 (combined chain-rest persistence + multiplicative-chain docs), #681 (non-semantic test formatting overlap), #686 (combined stricter playable-stream checks with unified unload helper), #688 (accepted newer superseding migration-doc wording), #694 (accepted superseding editor onset-layer implementation), #695 (kept both required test includes), #696 (kept multiplicative scoring while gating zero-point chain advancement), #697 (kept existing consolidated issue-221 timing coverage), #698 (combined RAII ownership with prior music stream validity/unload safeguards), #700 (kept added session_logger regressions), #702 (included both validate_offset_semantics and service_worker_cache_policy_node_tests in native CI explicit gate lists).
- 2026-05-11T12:54:54.442-07:00 — Validation commands: failed baseline configure without vcpkg (`cmake -B build -S . -Wno-dev`, missing EnTT package), failed `./build.sh` without `VCPKG_ROOT`, succeeded with `export VCPKG_ROOT=/Users/yashasgujjar/vcpkg && ./build.sh`. Initial `./build/shapeshifter_tests` run found one failing assertion in `tests/test_scoring_extended.cpp` due post-merge expectation mismatch; fixed with commit `722bf11`; final `./build.sh && ./build/shapeshifter_tests` passed.
- 2026-05-11T13:34:00.133-07:00 — Resolved PR #704 (`squad/restore-stashed-squad-state` → `main`) conflicts by merging `origin/main` into the PR branch (no history rewrite). Conflicts resolved in: `CMakeLists.txt`, `app/audio/music_context.h`, `app/input/keyboard_shape_mapping.h`, `app/systems/input_system.cpp`, `app/systems/scoring_system.cpp`, `design-docs/game.md`, `tests/test_input_pipeline_behavior.cpp`, `tests/test_test_player_system.cpp`; also fixed merge-introduced duplicate helper in `tests/test_scoring_system.cpp` surfaced by validation. Validation: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests` ✅ passed (`2382 assertions in 807 test cases`). Intentionally left uncommitted local runtime control files: `.squad/ralph-circuit-breaker.json`, `.squad/ralph-stop`; preserved unrelated local edit in `loop.md` and temporarily stashed/reapplied `.squad/agents/hockney/history.md`.
- 2026-05-11T13:41:28.438-07:00 — PR #704 comment-resolution pass completed: reviewed all Copilot review threads (3 total), classified 3 actionable / 0 informational, pushed fixes in `8083c85` (real Squad CI build/test job; clearer validation-constants fallback warning; test fixture updated to `Obstacle(points)`) and `f1702dd` (fix Squad CI vcpkg bootstrap by avoiding shallow clone and ensuring build-essential). Replied on each thread and resolved all 3 threads. Local validation rerun: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests` ✅ passed (`2382 assertions in 807 test cases`). PR URL: https://github.com/yashasg/friendly-parakeet/pull/704. Excluded local files from commits: `loop.md`, `.squad/ralph-circuit-breaker.json`, `.squad/ralph-stop`.
