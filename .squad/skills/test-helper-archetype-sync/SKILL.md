# SKILL: Test Helper / Production Archetype Sync Audit

**Domain:** C++ ECS Code Quality  
**Applicable to:** Any PR that adds or changes entity archetypes in `app/archetypes/`, or any PR that adds/changes entity construction in `tests/test_helpers.h`.

---

## Pattern

When a production archetype (`apply_obstacle_archetype`, `create_player_entity`, etc.) is introduced or modified, the parallel test helper factories in `test_helpers.h` may silently diverge — using hardcoded defaults instead of delegating to the shared factory. The divergence is invisible in CI because tests pass (functional behavior is correct) while the _component values_ (colors, sizes, point values) differ from production.

### Step-by-Step Sync Audit

1. **List all test helper factories** in `tests/test_helpers.h` that construct entity archetypes (search for `reg.create()` + `emplace<`-chains).

2. **For each helper, compare against the production archetype:**
   - Are all components the same?
   - Are all field values (especially Color, DrawSize, base_points) the same?
   - Does the helper add components the archetype doesn't (e.g., `Velocity`, `DrawLayer`) for test-only reasons?

3. **Check for intentional divergence:**
   - Test helpers may omit components not needed for the test (e.g., no `DrawLayer` in unit tests) — this is acceptable if documented.
   - Test helpers using simplified/sentinel values (white color instead of real color) are a risk if render logic or future systems branch on those values.

4. **Check the archetype's test** (`test_obstacle_archetypes.cpp` equivalent):
   - If an archetype has direct contract tests, it locks the production values.
   - The test helpers should **call the archetype function** rather than duplicate its logic.

5. **Verify Velocity/DrawLayer wiring in test helpers:**
   - Production spawners always add `Velocity` and `DrawLayer` before calling `apply_obstacle_archetype`.
   - Test helpers that skip these may fail system tests that iterate `<ObstacleTag, Velocity>` views.

---

## Key Heuristic

> "If a test helper constructs the same logical entity as a production archetype, it should _call_ the archetype, not duplicate it. Duplication means future archetype changes will not be reflected in tests."

---

## Known Divergences (as of ECS refactor audit, 2026-05)

| Test helper | Divergence | Risk |
|-------------|-----------|------|
| `make_vertical_bar(HighBar)` | Color `{255,180,0,255}` (yellow) vs archetype `{180,0,255,255}` (purple) | Render tests would use wrong color |
| `make_lane_push` | Color `{0,200,200,255}` (cyan) vs archetype `{255,138,101,255}` (orange) | Same |
| `make_shape_gate` | Always white `{255,255,255,255}` vs shape-conditional archetype colors | Shape-color contract untested |
| `make_popup_entity` | Uses `ScreenPosition` not `Position`+`Velocity` — intentional test isolation | Cannot catch scroll-path bugs |

---

## Checklist for Archetype PRs

- [ ] New/changed archetype function is covered by a direct contract test (`test_*_archetypes.cpp`)
- [ ] All `test_helpers.h` factories for the same entity type now call the archetype function
- [ ] Velocity/DrawLayer pre-emplace in test helpers matches production spawner
- [ ] If test helper intentionally diverges (e.g., skips DrawLayer), divergence is documented in a comment
- [ ] Full suite passes with zero warnings
