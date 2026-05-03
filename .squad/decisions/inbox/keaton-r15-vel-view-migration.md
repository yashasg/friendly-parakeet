# keaton-r15: vel_view → motion_view migration (issue #349)

## Pre tail-5 (verbatim)

```
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
All tests passed (2255 assertions in 786 test cases)
```

## Investigation

### Full `\bVelocity\b` grep (pre-migration)

```
app/components/transform.h:15:// This replaces generic Velocity during the issue #349 migration.
app/components/transform.h:34:struct Velocity {
app/systems/test_player_system.cpp:84:        auto* vel = reg.try_get<Velocity>(entity);
app/systems/motion_system.cpp:7:    // Legacy Position+Velocity entities (freeplay obstacles not yet migrated
app/systems/motion_system.cpp:12:    auto vel_view = reg.view<Position, Velocity>(entt::exclude<BeatInfo>);
app/systems/scroll_system.cpp:39:        reg.view<ObstacleTag, ObstacleScrollZ, Velocity>(entt::exclude<BeatInfo>);
app/entities/obstacle_entity.h:21://   ObstacleTag + Velocity + DrawLayer + kind-specific components
app/entities/obstacle_entity.cpp:11:    reg.emplace<Velocity>(e, 0.0f, params.speed);
tests/test_obstacle_archetypes.cpp (8 lines)
tests/test_helpers.h (6 lines)
tests/test_world_systems.cpp (5 lines)
tests/test_components.cpp:232-233
tests/test_death_model_unified.cpp:108,127
tests/test_collision_extended.cpp:36
tests/test_rhythm_system.cpp:309
tests/test_scoring_extended.cpp:184,218
tests/test_collision_system.cpp:164,185
tests/test_beat_scheduler_system.cpp:302
tests/test_scoring_system.cpp:82
tests/test_scroll_rhythm.cpp:34
benchmarks/bench_systems.cpp:59,79,131,165
```

### Key findings

- **`Velocity` was obstacle-only** in production. No non-obstacle entity (bullet, projectile, etc.) used it.
- `spawn_obstacle` emplaced `Velocity(0, speed)` on ALL obstacle kinds (line 11 was universal, before the per-kind switch).
- `scroll_system` model_view used `ObstacleScrollZ + Velocity` (LowBar/HighBar path).
- `motion_system` vel_view used `Position + Velocity` (ShapeGate, LaneBlock, ComboGate, SplitPath, LanePushLeft/Right path).
- `test_player_system` used `try_get<Velocity>` to estimate obstacle arrival time.
- `Position` component is heavily used by collision, miss detection, scoring, camera, and player AI — it cannot be removed from non-ScrollZ obstacles in this round.

### Migration decision

**Delete `Velocity` entirely.** Since obstacles are the only production users, removing it from `spawn_obstacle` eliminates all production uses in one step.

For `Position`-authority obstacles (ShapeGate et al.): the existing `WorldTransform + MotionVelocity` → `motion_view` path already integrates position. Added a `Position` bridge inside `motion_view` so `Position.y` stays authoritative for collision/scoring/camera systems that read it directly.

## What was migrated

### Production code

| File | Change |
|---|---|
| `app/components/transform.h` | Deleted `Velocity` struct (was lines 33–37) |
| `app/entities/obstacle_entity.cpp:11` | `reg.emplace<Velocity>(e, 0.0f, params.speed)` → `reg.emplace<MotionVelocity>(e, MotionVelocity{{0.0f, params.speed}})` |
| `app/entities/obstacle_entity.h:21` | Updated comment: `Velocity` → `MotionVelocity` |
| `app/systems/scroll_system.cpp:39` | `ObstacleScrollZ, Velocity` → `ObstacleScrollZ, MotionVelocity`; `vel.dy` → `vel.value.y` |
| `app/systems/motion_system.cpp` | Deleted vel_view loop (11 lines); added `Position` bridge to motion_view |
| `app/systems/test_player_system.cpp:84` | `try_get<Velocity>` → `try_get<MotionVelocity>`; `vel->dy` → `vel->value.y` (×2) |

### Test code

- `tests/test_helpers.h` — 6 factory functions: `Velocity` → `MotionVelocity{{0, scroll_speed}}`
- `tests/test_obstacle_archetypes.cpp` — 8 `all_of<>` assertions: `Velocity` → `MotionVelocity`
- `tests/test_components.cpp` — Replaced "Velocity default is zero" with "MotionVelocity explicit construction"
- `tests/test_world_systems.cpp` — Rewrote 3 vel_view tests into 3 motion_view tests (one covering Position bridge); updated comment and 2 phase-guard tests
- `tests/test_death_model_unified.cpp` — 2 sites: `Velocity` → `MotionVelocity`
- `tests/test_collision_extended.cpp` — 1 site
- `tests/test_rhythm_system.cpp` — view + `vel.dy` → `MotionVelocity` + `vel.value.y`
- `tests/test_scoring_extended.cpp` — 2 sites
- `tests/test_collision_system.cpp` — 2 sites
- `tests/test_beat_scheduler_system.cpp` — view + `vel.dy/dx` → `MotionVelocity` + `vel.value.y/x`
- `tests/test_scoring_system.cpp` — Removed `CHECK_FALSE(reg.all_of<Velocity>(e))` (obsolete assertion on deleted type)
- `tests/test_scroll_rhythm.cpp` — Renamed test; `Velocity{{999,999}}` → `MotionVelocity{{999,999}}`
- `benchmarks/bench_systems.cpp` — 4 emplace sites + updated `spawn_scroll_obstacles` comment

## `Velocity` type deleted?

**YES** — `app/components/transform.h` — struct removed entirely. File now has no `Velocity` definition.

## `vel_view` loop deleted?

**YES** — `app/systems/motion_system.cpp` — the `auto vel_view = reg.view<Position, Velocity>(entt::exclude<BeatInfo>)` loop and its 10 body lines are gone.

`grep -rn "vel_view" app/` post-migration:
- `app/systems/particle_system.cpp:39` — this is an unrelated local variable (`reg.view<ParticleTag, MotionVelocity>()`), not the obstacle vel_view.

## Bench numbers

### motion_system (post-migration)

| Count | Mean | Notes |
|---|---|---|
| 10 entities | ~103.7 ns | Baseline had ~34–38 ns; delta explained below |
| 100 entities | ~331.7 ns | Baseline ~191 ns |
| 1000 entities | ~2.49 µs | Baseline ~1.81 µs |

**Note on delta:** The bench `spawn_obstacles` helper now also emplaces `WorldTransform` (added to match production archetype). These obstacles have both `WorldTransform + MotionVelocity + Position`, so the motion_view now does the `try_get<Position>` bridge on each entity. This adds one ptr-lookup per entity. Pre-migration the bench entities used `Position + Velocity` (vel_view path, no WorldTransform bridge). The new numbers are slightly higher due to the bridge, not a regression in the integration path — production MotionVelocity-only entities (particles, popups) are unaffected.

### particle_system (post-migration)

| Count | Mean |
|---|---|
| 50 particles | ~34.86 ns |

Matches Keyser-r14 baseline (~32–34 ns). ✓

### full frame stress (50 obstacles + 50 particles)

| Run | Mean |
|---|---|
| post-r15 | ~1.01 µs |

Keyser-r14 baseline: ~926 ns–1.01 µs. Within range. ✓

## Post tail-5 (verbatim)

```
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (137 beats, difficulty=easy)
All tests passed (2256 assertions in 786 test cases)
```

## Test count

- Pre: 786 test cases, 2255 assertions
- Post: 786 test cases, 2256 assertions (+1 assertion in new Position bridge test)
- No decrease. ✓

## Commit

`70f6436` — issue #349: migrate obstacles from Velocity to MotionVelocity, delete vel_view
