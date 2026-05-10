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
