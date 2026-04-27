# #280 — Split Scroll-Past Miss Detection out of cleanup_system

**Author:** Saul (Game Design)
**Status:** ✅ APPROVED — ready for engineering pickup
**Issue:** https://github.com/yashasg/friendly-parakeet/issues/280
**Date:** 2026-05-17

## Decision

The proposed refactor is **design-correct**. `cleanup_system` is destroying entities AND mutating `EnergyState` / `SongResults` (cleanup_system.cpp:19–28); split the miss decision into its own system and let `scoring_system` remain the single owner of grade→energy effects.

## Required Behavior (Player-Facing — Must Not Regress)

1. An obstacle that scrolls past `DESTROY_Y` without ever being scored still counts as a miss.
2. That miss drains energy by `ENERGY_DRAIN_MISS`, clamped at 0.
3. That miss increments `SongResults::miss_count` by exactly 1.
4. That miss resets `ScoreState::chain_count` and `chain_timer` (currently handled by scoring_system's MissTag branch — preserve).
5. If the drain takes energy to 0, Game Over fires this frame, not next frame (current behavior also satisfies this only by coincidence — see Bonus Win).
6. `BankedBurnout`, `TimingGrade`, and `Obstacle` components are removed from the entity once the miss is processed (already covered by scoring_system's MissTag branch — keep).

## Engineering Constraints / Acceptance Criteria

**System layout**
- New system: `miss_detection_system(entt::registry&, float dt)` in `app/systems/`.
- Frame order in `game_loop.cpp` (run loop): `... collision_system → miss_detection_system → scoring_system → energy_system → ... → cleanup_system`.
  - **Must** run before `scoring_system` so the MissTag is processed the same frame.
  - **Must** run before `energy_system` so an energy-zeroing miss triggers Game Over without one-frame latency.

**miss_detection_system rules**
- Query: `reg.view<ObstacleTag, Obstacle, Position>(entt::exclude<ScoredTag>)`.
- For each entity where `pos.y > constants::DESTROY_Y`: `reg.emplace<MissTag>(e); reg.emplace<ScoredTag>(e);`.
- No direct mutation of `EnergyState`, `ScoreState`, or `SongResults` from this system. It is a pure tagger.
- LanePush obstacles are already `ScoredTag`-stamped by `collision_system.cpp:192` at pass-through, so they will not be tagged by this system. Do not add a kind-based exclusion; exclude-by-`ScoredTag` is the canonical guard.

**scoring_system extension (MissTag branch, scoring_system.cpp:36–44)**
- When processing a `MissTag` obstacle, additionally:
  - `energy->energy -= constants::ENERGY_DRAIN_MISS;` then clamp `energy->energy = max(0, energy->energy)`.
  - `results->miss_count++;` (guard `find<SongResults>()`).
  - Optionally trigger `energy->flash_timer = ENERGY_FLASH_DURATION` (parity with Bad timing — recommended for visual feedback consistency, but not required for sign-off).
- Existing chain reset and component cleanup remain as-is.

**cleanup_system reduction**
- After the change, `cleanup_system` only contains the `to_destroy.push_back(entity)` paths — i.e. destroy any entity past `DESTROY_Y` that has `ScoredTag` (or has `ObstacleTag` but no `Obstacle`, for already-stripped entities and decorations).
- Remove the `EnergyState` / `SongResults` writes entirely. No `MissTag`/`ScoredTag` emplace calls from this system.

**Edge cases**
- An obstacle that gets `ScoredTag` from `collision_system` *and* scrolls past in the same frame must not be double-counted. Exclude-by-`ScoredTag` in miss_detection_system handles this; do not key on `MissTag` presence.
- If `EnergyState` is absent from ctx (test fixtures), scoring_system's MissTag branch must no-op the energy drain gracefully (use the existing `find<EnergyState>()` pattern — already in place at scoring_system.cpp:34).

## Tests (must pass post-refactor)

- `test_death_model_unified` "cleanup miss drains energy" — keep green; consider renaming to "scroll-past miss drains energy" since cleanup is no longer the actor.
- Add a new test: a scroll-past miss whose drain takes energy from `ENERGY_DRAIN_MISS` to 0 transitions `GameState.phase` to `GameOver` **in the same frame** (this is the latency-bug regression guard the refactor enables).
- Add a new test: scroll-past miss does **not** double-drain (energy delta is exactly `-ENERGY_DRAIN_MISS`, miss_count delta is exactly 1).
- LanePush passing the player and scrolling off must not produce a `MissTag` or any energy delta (current behavior — keep green).

## Bonus Win (Call Out in PR Description)

The current cleanup-time energy drain runs *after* `energy_system` in the frame, so a scroll-past miss that should kill the player only triggers Game Over on the next frame. Moving the decision before `energy_system` closes that latency hole. Engineering should call this out so QA validates the same-frame Game Over assertion.

## Owners

- **Implementation:** McManus (gameplay systems)
- **Tests:** Verbal (edge cases) + Baer (regression)
- **Review:** Kujan
- **Design sign-off:** Saul (this note)
