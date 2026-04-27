# Decision: PR #43 regression test suite + pool-priming fix

**Author:** Baer  
**Date:** 2026-04-26  
**Commit:** 31bc2d8

## Summary

Added regression tests for all 6 PR #43 review themes and fixed a real production bug discovered during investigation.

## New test files

- `tests/test_pr43_regression.cpp` — 14 test cases covering all 6 themes
- 3 new test cases appended to `tests/test_level_select_system.cpp`

## Bug fixed: ObstacleChildren pool ordering (game_loop.cpp)

### Problem

`entt::registry::destroy(entity)` iterates component pools in **reverse insertion order**. In production:
1. `game_loop_init` connects `on_destroy<ObstacleTag>` → ObstacleTag pool inserted at index 0
2. First obstacle spawn calls `add_slab_child` / `add_shape_child` → ObstacleChildren pool inserted at a higher index

When `cleanup_system` calls `reg.destroy(obstacle)`, reverse iteration removes ObstacleChildren first. By the time `on_obstacle_destroy` fires (for ObstacleTag), `try_get<ObstacleChildren>(parent)` returns null → children are silently NOT destroyed.

Consequence: orphaned MeshChild entities accumulate in the registry. `camera_system` then calls `reg.get<Position>(mc.parent)` on a destroyed (invalid) entity — **undefined behavior**.

### Fix

In `app/game_loop.cpp`, added `reg.storage<ObstacleChildren>();` before `reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();`. This primes the ObstacleChildren pool at a lower index so it is processed **last** in reverse iteration — still alive when the signal fires.

### For team

This is an EnTT footgun: any time you use `on_destroy<TagA>` to read component B from the same entity, component B's pool must be primed (via `reg.storage<B>()`) **before** the `on_destroy<TagA>` connection, or B will already be gone by the time the signal fires.

## Test setup pattern

`make_obs_registry()` (in test_pr43_regression.cpp) documents the correct setup:
```cpp
static entt::registry make_obs_registry() {
    auto reg = make_registry();
    reg.storage<ObstacleChildren>();   // prime BEFORE connecting signal
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();
    return reg;
}
```
