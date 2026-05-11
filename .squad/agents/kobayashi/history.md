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
