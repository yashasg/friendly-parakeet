# Decision Drop — motion_system Extraction
**Date:** 2026-05-03
**Author:** Keaton
**Round:** 3

---

## What Shipped

Extracted `vel_view` (Position+Velocity) and `motion_view` (WorldTransform+MotionVelocity) loops from `scroll_system` into a new `motion_system`, per Keyser's R2 SOLID audit recommendation.

### Files Added
- `app/systems/motion_system.cpp` — new system with phase guard matching scroll_system's

### Files Modified
- `app/systems/scroll_system.cpp` — stripped to obstacle-only loops (3 views: model_beat_view, beat_view, model_view, all ObstacleTag-bearing)
- `app/systems/all_systems.h` — added `void motion_system(entt::registry& reg, float dt)` declaration under Phase 5
- `app/game_loop.cpp` — added `motion_system(reg, dt)` immediately after `scroll_system(reg, dt)`
- `tests/test_world_systems.cpp` — renamed 3 "scroll:" test cases to "motion:" and updated calls to `motion_system`; added "motion: no movement when not in Playing phase" phase guard test
- `tests/test_scroll_rhythm.cpp` — renamed "scroll: non-rhythm entities use dt-based movement even in rhythm mode" to "motion: non-rhythm entities use dt-based movement"; updated call to `motion_system`
- `benchmarks/bench_systems.cpp` — added "Bench: motion_system" (10/100/1000 entity tiers); added `motion_system(reg, DT)` to full-frame typical + stress benches

---

## Build Result

Zero warnings. `-Wall -Wextra -Werror` clang. CMake reconfigured cleanly (new file picked up via GLOB CONFIGURE_DEPENDS).

---

## Test Result

**2211 assertions in 772 test cases — all passed.**
(Up from 2209/771 in R2; the +2 assertions are the new motion phase-guard test.)

---

## Bench Delta

### Bench context note
The `Bench: scroll_system` fixture uses `spawn_obstacles` which creates `ObstacleTag + Position + Velocity` (no `ObstacleScrollZ`, no `BeatInfo`). In the old code, these entities were processed by `vel_view` inside `scroll_system`. Post-extraction, `scroll_system` touches 0 of them (all three remaining views require `ObstacleScrollZ` or `BeatInfo`); `motion_system` now owns them.

| Metric | R2 baseline | R3 scroll_system | R3 motion_system | R3 combined |
|---|---|---|---|---|
| 10 entities (mean) | 48 ns | 11.3 ns (empty — 0 matching entities) | 50 ns | ~61 ns |
| 100 entities | N/A | 11.3 ns | 214 ns | ~225 ns |
| 1000 entities | N/A | 11.5 ns | 1923 ns | ~1935 ns |

**Combined 10-entity cost vs R2 baseline: 61 ns vs 48 ns (+27%).** This overhead is the cost of the architectural split: one extra function call, one extra phase guard check, one extra view construction. The per-entity work is identical.

The scroll_system 11 ns floor is the cost of three empty EnTT view iterations + one `ctx().get<GameState>()` + one `ctx().find<SongState>()` — it will only shrink further once real `ObstacleScrollZ` entities are present in a rhythm game bench fixture.

---

## Surprises / Coupling Discovered

**Phase guard coupling (handled):** The original `scroll_system` phase guard (`if phase != Playing return`) was silently gating `vel_view` and `motion_view` too. When first extracted without the guard, 3 existing tests failed (position-integration tests assumed no movement in non-Playing phase). Fixed by adding an identical phase guard to `motion_system`. No behavioral change.

**No other coupling found.** The two loops had zero shared state with the obstacle-scroll loops. Split was clean.

---

## Follow-ups for R4

1. **Add ObstacleScrollZ to bench fixture** — the `Bench: scroll_system` fixture should spawn entities with `ObstacleScrollZ` to actually stress the obstacle loops. Currently the fixture measures an empty scroll_system.
2. **ObstacleTag filter on vel_view/motion_view** — now that motion_system is isolated, adding an `ObstacleTag` exclude (or a dedicated non-obstacle tag) could tighten its loops further if obstacle entities shouldn't be processed there. Audit which entity types actually carry Position+Velocity in production.
3. **Legacy Position+Velocity migration** — with the migration path now cleanly owned by `motion_system`, continue the #349 migration: move obstacle entities to `WorldTransform + MotionVelocity`, then vel_view can be deleted.
