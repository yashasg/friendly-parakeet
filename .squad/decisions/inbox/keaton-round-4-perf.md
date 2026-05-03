# Keaton Round 4 — Perf Decision Drop

Date: 2026-05-03  
Author: Keaton  

---

## Part 1 — scroll_system bench fixture fix

### Root cause

`spawn_obstacles` creates entities with `ObstacleTag + Position + Velocity`.
All three `scroll_system` views require `ObstacleScrollZ`:

| view | required components |
|------|---------------------|
| model_beat_view | ObstacleTag + ObstacleScrollZ + BeatInfo |
| beat_view | ObstacleTag + Position + BeatInfo |
| model_view (freeplay) | ObstacleTag + ObstacleScrollZ + Velocity (exclude BeatInfo) |

Spawned obstacles entered none of them. Measured 11 ns was pure
`GameState` check + `ctx.find<SongState>()` overhead — zero entity work.
Flat across 10/100/1000 entities confirmed zero scaling.

### Fix

Added `spawn_scroll_obstacles` helper in `benchmarks/bench_systems.cpp`
that spawns `ObstacleTag + ObstacleScrollZ + Velocity` (freeplay archetype,
no BeatInfo) — exactly what scroll_system's `model_view` iterates.
Updated "Bench: scroll_system" to use it.

### Before → after numbers

| entities | before (broken) | after (real work) |
|----------|-----------------|-------------------|
| 10       | 11.6 ns         | 38.6 ns           |
| 100      | 11.4 ns         | 289.9 ns          |
| 1000     | 11.5 ns         | 2.48 μs           |

Per-entity cost: ~2.7 ns/entity (linear, as expected for `oz.z += vel.dy * dt`
+ optional `wt->position.y = oz.z` write). The ~11 ns base overhead remains
(GameState + SongState ctx lookups + 2 empty beat views).

---

## Part 2 — collision_system bench fix + optimization

### Bench fixture broken (same class of bug)

`make_bench_player` was missing `WorldTransform` and `ShapeWindow`.
`collision_system`'s `player_view` requires:
`PlayerTag, WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState`.

Without the two components, `player_view.begin() == player_view.end()`
was always true → early return. Measured 16 ns was pure overhead, not
collision work.

### Fix

Added `WorldTransform` and `ShapeWindow` to `make_bench_player`.

### Optimization: precompute frame-constants + 1D lane check

**Hypothesis (with file:line citations):**

`collision_system.cpp` computed two frame-constant values redundantly
per obstacle:

1. `player_timing_point(p_transform, p_vstate)` (line 75 before change) —
   called inside `resolve` for every obstacle. Expanded to:
   `p_transform.position.y + p_vstate.y_offset`. Both inputs are read-only
   across the frame. A scalar precompute outside the loops eliminates the
   struct construction + function call per entity.

2. `player_overlaps_lane(p_transform, pos)` (lines 153/205 before change) —
   called `centered_rect(player_x, 0, SIZE, 1)` for the player on every
   obstacle iteration. Player x is constant per frame. The y-axis always
   overlaps (both rects use y=0, h=1), so `CheckCollisionRecs` reduces to a
   1D interval test: `|obs_x - player_x| < PLAYER_SIZE`.

**Changes made (`app/systems/collision_system.cpp`):**

- Added `const float player_timing_y = p_transform.position.y + p_vstate.y_offset`
  and `const float player_x = p_transform.position.x` before all loops.
- `resolve` lambda: replaced `!player_in_timing_window(...)` with
  `obs_z < player_timing_y` (scalar comparison).
- Added `lane_overlaps` lambda: `dx = obs_x - player_x; return dx > -SIZE && dx < SIZE`.
- Replaced all `player_overlaps_lane(p_transform, pos)` call sites with
  `lane_overlaps(pos)`.
- Replaced inline `player_in_timing_window(...)` in LanePush loop with
  `pos.y < player_timing_y`.
- Removed now-dead helpers: `centered_rect`, `player_timing_point`,
  `player_in_timing_window`, `player_overlaps_lane`.

**Before → after bench numbers:**

| scenario | before (broken fixture) | after (real work + opt) |
|----------|-------------------------|--------------------------|
| 1 obstacle at player | 16 ns (early return) | 165 ns (full resolve path) |
| 10 obstacles scattered | 27 ns (early return) | 283 ns (6 view iters, all timing-miss) |

Note: can't isolate optimization delta vs fixture fix — the fixture was
broken before so there's no "same fixture, old code" baseline. The
optimization eliminates 2× `centered_rect` + `CheckCollisionRecs` per
ShapeGate obstacle and 1 addition + Vector2 per resolve call. At 10
obstacles: ~40–80 ns saved (within bench noise).

---

## Build + test status

- `cmake --build build`: **zero warnings, zero errors** (Clang, `-Wall -Wextra -Werror`)
- `./build/shapeshifter_tests '~[bench]'`: **2211 assertions, 772 test cases, all passed**
- `./run.sh bench`: ran cleanly, all 16 bench cases executed

---

## No-wins noted

- `scoring_system`: 37 ns baseline is ~6 ctx lookups + 3 view setups.
  No clean optimization without restructuring. Flagged but not touched.
- `motion_system`: already tight (`pos += vel * dt`); per-entity cost is
  ~2.1 ns at 100 entities. Not a target this round.

---

## Follow-up for Round 5

**Highest-value next action**: issue #349 migration — once obstacles carry
`WorldTransform + MotionVelocity` instead of `Position + Velocity`,
`motion_system`'s `vel_view` becomes deletable and the scroll/motion split
is fully realized. Also unlocks accurate `scroll_system` + `motion_system`
benchmarks at scale without synthetic helpers. The `spawn_scroll_obstacles`
vs `spawn_obstacles` divergence in the bench is a sign the migration is
half-done.
