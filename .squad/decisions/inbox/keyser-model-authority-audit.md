# Architecture Audit — Model.transform Authority Slice (Slice 2)
**Author:** Keyser  
**Date:** 2026-05-18  
**Status:** ACTIVE — read-only audit for Keaton/Kujan  
**Scope:** Removing `Position` from LowBar (and HighBar); making `Model.transform` the sole world-space authority for those kinds.

---

## Q1 — Component set that should remain after this slice

### LowBar (and HighBar when `build_obstacle_model` covers it)

| Component | Keep? | Reason |
|---|---|---|
| `ObstacleTag` | ✓ | Structural tag; all systems exclude non-obstacle entities via this |
| `Obstacle` (kind, base_points) | ✓ | collision, scoring, miss_detection read kind + points |
| `RequiredVAction` | ✓ | collision_system LowBar/HighBar view pivot |
| `ObstacleModel` | ✓ | RAII GPU resource owner |
| `ObstacleParts` | ✓ | Geometry descriptors; scroll_system + camera_system read these |
| `TagWorldPass` | ✓ | Render pass membership |
| `BeatInfo` | ✓ (if rhythm) | Scroll anchor; must remain for scroll_system |
| `ScrollZ` | ✓ NEW | Bridge component (see Q3); written by scroll_system, read by logic systems |
| `Velocity` | optional | Harmless on rhythm entities; can keep or remove |
| `DrawSize` | keep at spawn, vestigial after | Still read by `build_obstacle_model` at spawn time (`dsz->h` = slab depth). Removable only after spawn path uses constants directly |
| `Color` | keep at spawn, vestigial after | Same: baked into `om.model.materials[0].maps[...].color` at spawn |

**Remove:**
- `Position` — the migration goal
- `ModelTransform` — was already guarded away from LowBar in `game_camera_system`; confirm it was never emitted for any Model-owning entity

---

## Q2 — Systems that must stop relying on Position for migrated obstacles

All five systems below query `Position` for obstacle data. Each will silently drop migrated entities when `Position` is removed.

| System | Current Position usage | Required change |
|---|---|---|
| `scroll_system` | Writes `pos.y` for `ObstacleTag + Position + BeatInfo` view (rhythm path) | Add view: `ObstacleTag, ScrollZ, BeatInfo` (no Position). Write `scroll_z.z = SPAWN_Y + (song_time - spawn_time) * scroll_speed` |
| `game_camera_system` §1b | `reg.view<ObstacleModel, ObstacleParts, Position>()` — reads `pos.y` to compute `model.transform` | Replace `Position` with `ScrollZ`. Compute `om.model.transform = MatrixTranslate(pd.cx, pd.cy, scroll_z.z + pd.cz)` |
| `cleanup_system` | `reg.view<ObstacleTag, Position>()` — checks `pos.y > DESTROY_Y` | Add second view: `reg.view<ObstacleTag, ScrollZ>(exclude<Position>)`. Check `scroll_z.z > DESTROY_Y` |
| `miss_detection_system` | `reg.view<ObstacleTag, Obstacle, Position>(exclude<ScoredTag>)` | Add second view: `reg.view<ObstacleTag, Obstacle, ScrollZ>(exclude<ScoredTag, Position>)`. Check `scroll_z.z > DESTROY_Y` |
| `collision_system` | `reg.view<ObstacleTag, Position, Obstacle, RequiredVAction>` — uses `pos.y` in `player_in_timing_window` | Add per-kind view variant that reads `ScrollZ.z` instead of `pos.y`. Pass `scroll_z.z` as the timing position. Note: LowBar/HighBar have no lane X — the check is timing-only, not spatial. |
| `scoring_system` | `hit_view` queries `Position` to spawn popup at `pos.x, pos.y - 40` | Read `r.scroll_z` (from `ScrollZ`) for popup Z-origin; use `0.0f` or `constants::SCREEN_W / 2.0f` for popup X since LowBar/HighBar are full-width. |

---

## Q3 — Bridge component recommendation

**Yes — `struct ScrollZ { float z; }` is the correct narrow bridge for this slice.**

Rationale:
- `collision_system`, `cleanup_system`, `miss_detection_system`, `scoring_system` are game-logic systems. They must not depend on `Matrix` or `model.transform` internals. If the TRS encoding of `model.transform` changes, `m14` is no longer the Z position — all five systems would silently break.
- `ScrollZ.z` is a plain float with a single write owner (`scroll_system`). One source of truth per frame. Zero GPU dependency.
- Headless-testable: `ScrollZ` has no raylib/GPU dependency. Tests for scroll, cleanup, miss, collision can all run without `InitWindow`.
- Matches the DOD principle: separate the scroll-axis position (game logic data) from the model transform (render output). `game_camera_system` derives `model.transform` FROM `ScrollZ`, not the other way around.

**Type:** `struct ScrollZ { float z = 0.0f; }` in `app/components/transform.h` (alongside `Position`).  
**Write owner:** `scroll_system` (rhythm path writes absolute; non-rhythm path could keep Position or also migrate).  
**Readers:** `game_camera_system`, `cleanup_system`, `miss_detection_system`, `collision_system`, `scoring_system`.

Do NOT read `model.transform.m14` directly in any game logic system. That is render data.

---

## Q4 — Architectural blockers that make removing Position unsafe now

These are hard blockers. Removing Position before any one of these is addressed causes silent data loss or game logic failure.

| # | Blocker | System | Failure mode |
|---|---|---|---|
| **HB-1** | `game_camera_system` §1b queries `ObstacleModel, ObstacleParts, **Position**` | camera_system.cpp:281 | Without Position, the view returns empty. `model.transform` is never updated. Obstacle freezes at spawn Z (Identity matrix). Invisible or misplaced every frame. |
| **HB-2** | `scroll_system` rhythm path writes `**pos**.y` | scroll_system.cpp:17-22 | No write path for Z. After Position removal, `ScrollZ` is never updated. All scroll motion stops for migrated kinds. |
| **HB-3** | `cleanup_system` only queries `ObstacleTag, **Position**` | cleanup_system.cpp:14 | Migrated entities are never destroyed. Registry grows unbounded. BeatInfo state corrupts. |
| **HB-4** | `miss_detection_system` only queries `ObstacleTag, Obstacle, **Position**` | miss_detection_system.cpp:15 | Migrated obstacles that scroll past never receive `MissTag`. Missed beats not penalized. Energy drain silent. |
| **HB-5** | `collision_system` LowBar/HighBar view requires `**Position**` | collision_system.cpp:150 | LowBar/HighBar invisible to `player_in_timing_window`. Hits and misses undetected. Game unscored. |

**Soft blockers (will degrade UX but not corrupt state):**
- `scoring_system` hit view reads `Position` for popup spawn origin — popup would appear at wrong location or crash if Position is absent and not guarded.

---

## Q5 — Tests to enable/rewrite

### Section A (4 pre-migration tests) — Delete when Slice 2 lands
`[pre_migration][model_slice]` tests assert `reg.all_of<Position>(e)` and `CHECK_FALSE(reg.all_of<Model>(e))`. They will fail once Position is removed. **This is the designed signal.** Delete them (or mark `[!shouldfail]`) — do not merely `#if 0` them, as they'll accumulate dead code.

### Section B — Keep, no changes needed

### Section C — Must be rewritten before enabling
The current Section C guards reference wrong types from the pre-RAII design. Concrete changes required:

| Section C reference | Actual type in codebase | Fix |
|---|---|---|
| `reg.all_of<Model>(e)` | `ObstacleModel` (RAII wrapper) | `reg.all_of<ObstacleModel>(e)` |
| `reg.get<Model>(e)` | `ObstacleModel` | `reg.get<ObstacleModel>(e).model` |
| `reg.all_of<ObstaclePartDescriptor>(e)` | `ObstacleParts` | `reg.all_of<ObstacleParts>(e)` |
| `m.meshCount == 3` | Current LowBar: `meshCount == 1` | `CHECK(reg.get<ObstacleModel>(e).model.meshCount == 1)` — or update if/when 3-mesh LowBar lands |
| on_destroy test wires `on_destroy<Model>` | Production wires `on_destroy<ObstacleModel>` | Test should wire `on_destroy<ObstacleModel>` to track; or test the RAII destructor path directly |

BLOCKER-3 (LoadMaterialDefault in headless) is NOT resolved by this slice. Section C tests that call `apply_obstacle_archetype` for LowBar still require GPU context. Use the `IsWindowReady()` guard in `build_obstacle_model` — it means Section C spawn tests will silently no-op in headless. Acceptable IF Section C tests assert `reg.all_of<ObstacleModel>(e) == false` in headless (since no-op), or if they require `InitWindow` in a dedicated integration test binary.

### New headless tests to add (all GPU-free)
1. **scroll_system → ScrollZ** — Verify `ScrollZ.z = SPAWN_Y + (song_time - spawn_time) * speed` for a BeatInfo entity. No GPU needed.  
2. **cleanup_system → ScrollZ path** — Emplace `ObstacleTag + ScrollZ{z = DESTROY_Y + 1.0f}`, run cleanup_system, verify entity destroyed.  
3. **miss_detection_system → ScrollZ path** — Same setup, run miss_detection, verify `MissTag + ScoredTag` emplaced.  
4. **collision_system timing window → ScrollZ** — Emplace `ObstacleTag + ScrollZ + Obstacle + RequiredVAction`, assert timing window logic works without Position.

---

## Summary

**The slice is safe to start once:**
1. `ScrollZ { float z; }` is added to `transform.h`
2. `scroll_system` has a `ScrollZ` write path (replaces `pos.y` for BeatInfo entities)
3. `game_camera_system` §1b reads `ScrollZ` instead of `Position`

Everything else (cleanup, miss, collision, scoring) can be updated in the same PR as Position removal — they are all mechanical view additions.

**Do not remove `Position` from `obstacle_archetypes.cpp` until all 5 HB-1–HB-5 blockers are addressed in the same changeset.**
