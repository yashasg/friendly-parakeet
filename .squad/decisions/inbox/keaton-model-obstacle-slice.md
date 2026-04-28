# Keaton Decision — ObstacleModel Migration: Slice 0+1 Outcome
**Date:** 2026-04-28  
**Author:** Keaton  
**Status:** DELIVERED

---

## What Was Implemented

### Slice 0 — Infrastructure (GPU-free, test-safe)

| Deliverable | File | Notes |
|---|---|---|
| Render-pass tags | `app/components/render_tags.h` (new) | `TagWorldPass`, `TagEffectsPass`, `TagHUDPass` — empty structs |
| `ObstacleModel` RAII wrapper | `app/components/rendering.h` | Move-only, `owned` flag, `release()` calls `UnloadModel` |
| `ObstacleParts` + `ObstaclePartDesc` | `app/components/rendering.h` | Per-mesh geometry descriptor (w, h, d, cx, cy, cz) |
| `on_destroy<ObstacleModel>` listener | `app/game_loop.cpp` | Wired in `game_loop_init`, disconnected in `game_loop_shutdown` |

### Slice 1 — LowBar Production Wiring (GPU path)

- `build_obstacle_model()` in `shape_obstacle.cpp`: constructs 1-mesh owned `Model` for LowBar via `GenMeshCube + UploadMesh + LoadMaterialDefault`. Emplaces `ObstacleModel`, `ObstacleParts`, `TagWorldPass`.
- Called from `obstacle_spawn_system.cpp` and `beat_scheduler_system.cpp` (production paths only).
- `game_camera_system`: writes `model.transform = MatrixTranslate(cx, cy, z + cz)` for `ObstacleModel` entities; skips `ModelTransform` for LowBar when `ObstacleModel` is present.
- `game_render_system`: `TagWorldPass + ObstacleModel` draw pass loops `DrawMesh` per mesh. Legacy `ModelTransform` path untouched.

---

## Critical Design Decisions

### `ObstacleModel` vs raw `Model` as ECS component
Using `ObstacleModel` (RAII wrapper) rather than raw `Model` allows pre-migration tests that assert `reg.all_of<Model>(e) == false` to keep passing. Incremental slice approach preserved.

### `IsWindowReady()` guard in `build_obstacle_model`
Prevents GPU calls (`GenMeshCube`, `UploadMesh`, `LoadMaterialDefault`) from crashing unit tests that call `obstacle_spawn_system` without `InitWindow`. Idiomatic raylib pattern.

### LowBar mesh: 1 mesh, actual dimensions
Used `GenMeshCube(SCREEN_W, LOWBAR_3D_HEIGHT, dsz.h)` (sized mesh). `model.transform = MatrixTranslate(cx, cy, z + cz)` where cx/cy/cz are stored in `ObstacleParts`. Translation-only matrix per frame — no scale recomputation.

### `Position` retained on LowBar
`cleanup_system` and `collision_system` both query `ObstacleTag + Position`. Removing `Position` from LowBar is Slice 2 scope. This slice is additive only.

---

## Validation
- Build: zero compiler warnings (`-Wall -Wextra -Werror`)
- Tests: 2902 assertions in 878 test cases — all pass
- `test_obstacle_model_slice.cpp` Section B (render tags) passes; Section C remains `#if 0`

---

## Next Slice (Slice 2)
1. Expand `build_obstacle_model` to HighBar, LanePushLeft, LanePushRight
2. Remove `Position` from those kinds — update `cleanup_system` to use `model.transform` Z for scroll-past detection
3. Expose `slab_matrix` as linkable helper (`app/util/render_matrix_helpers.h`) to unblock Section C scroll-transform tests
4. Potentially expand LowBar to 3 meshes (main bar + two end pillars) for architectural completeness
