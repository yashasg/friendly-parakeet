# SKILL: Dead-Surface Deletion Regression Coverage

**Domain:** Test Engineering  
**Applicable to:** Any PR that deletes dead ECS components, systems, or fields after a design pivot.

---

## Pattern

When a PR deletes dead ECS surface (components, systems, constants, fields), the regression risk is not from what was deleted, but from **what was preserved** and how test coverage was transferred.

### Step-by-Step Coverage Audit

1. **List deleted tests.** For each deleted test file, read the full content from the commit diff. Identify which test cases tested *dead* behavior vs. which tested *preserved* behavior with burnout/dead-surface involvement.

2. **Verify all preserved behaviors have direct test coverage:**
   - If a behavior test was migrated: confirm the migration is complete (all variants, not just the first one).
   - If no migration was needed (behavior unchanged, separate test file): confirm the existing tests still compile and pass.

3. **Check symmetric code branches.** If a scoring/chain/exclusion branch covers two symmetric variants (e.g., `LanePushLeft || LanePushRight`, `TierA || TierB`), both variants must have independent dedicated tests. A single test covering one side is insufficient — if the branch is later narrowed, the single test will still pass.

4. **Check struct cleanups.** Removed fields (e.g., `SongResults::best_burnout`, `DifficultyConfig::burnout_window_scale`) may have been referenced in UI source resolver tests or UI JSON schemas. Grep for the field name in all test files.

5. **Verify make_registry / test helpers.** Removed singleton types (e.g., `BurnoutState`) must be absent from `make_registry()` and all test helpers. A lingering include or emplace will cause a compile error.

6. **Run focused tests before full suite.** Use the tag filter to verify the migrated/new tests in isolation before running all 700+ test cases.

7. **Decommission retired folders end-to-end.** If a namespace/folder migration (e.g., `archetypes/` → `entities/`) is complete, ensure there are no forwarding headers left, remove obsolete CMake globs/sources, and update docs/comments that still call the old folder canonical.

8. **Preserve low-value taxonomy tags, fix high-signal ownership wording.**
   - Keep legacy test tags (e.g., `[archetype]`) when renaming would be churn-only.
   - Still update code comments, test titles, and docs so they do not imply removed paths (e.g., `app/archetypes/`) are canonical.

---

## Key Heuristic

> "The test coverage gap from deletion is almost always in the **migration**, not the deletion itself — specifically in symmetric variants that were tested as a unit and only partially migrated."

---

## Example (from #324)

- `test_burnout_bank_on_action.cpp` AC#4 tested `LanePushLeft` exclusion from chain/popup. Migrated to `test_scoring_extended.cpp`.
- `LanePushRight` uses the same `||` branch in `scoring_system.cpp` but had no dedicated chain/popup exclusion test.
- Added `"scoring: LanePushRight excluded from chain and popup"` to close the gap.

---

## Checklist for Dead-Surface PRs

- [ ] All deleted test files read from diff — no live behavior tests silently dropped
- [ ] All migrated tests cover both/all variants of symmetric branches
- [ ] Removed struct fields checked in UI resolver tests, schema tests
- [ ] `make_registry()` and `test_helpers.h` cleaned of removed types
- [ ] Focused test run on migrated/new tests passes
- [ ] Full suite passes with zero warnings
