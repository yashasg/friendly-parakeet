# Model Authority Revision — BF-1..BF-4 Fixed

**Author:** McManus  
**Date:** 2026  
**Status:** COMPLETE — awaiting Kujan re-review  
**Scope:** Model authority slice revision; Keaton locked out.

---

## Summary

Four Kujan blockers resolved. Original LowBar/HighBar ObstacleScrollZ migration preserved.

---

## BF-1: Manual Model construction (no LoadModelFromMesh)

**File:** `app/gameobjects/shape_obstacle.cpp`

`LoadModelFromMesh` removed. `build_obstacle_model()` now manually assembles the raylib `Model`:
- `model.meshes    = RL_MALLOC(sizeof(Mesh))` — owned pointer
- `model.meshes[0] = GenMeshCube(...)` — raylib 5.5 returns uploaded mesh; no UploadMesh needed
- `model.materials = RL_MALLOC(sizeof(Material))`
- `model.materials[0] = LoadMaterialDefault()`
- `model.meshMaterial = RL_CALLOC(meshCount, sizeof(int))` — zeroed, maps mesh 0 → material 0

`IsWindowReady()` guard preserved; `UnloadModel()` path unchanged (RAII owned flag).

---

## BF-2: ObstacleModel.model.transform as render authority

**Files:** `app/systems/camera_system.cpp`, `app/systems/game_render_system.cpp`

`camera_system.cpp` section 1b: removed `ModelTransform` emission for LowBar/HighBar. New view: `ObstacleTag + ObstacleScrollZ + ObstacleModel + ObstacleParts`. Writes `om.model.transform = slab_matrix(pd.cx, oz.z + pd.cz, pd.width, pd.height, pd.depth)` when `om.owned`.

`game_render_system.cpp`: added `draw_owned_models()` — iterates `const ObstacleModel + const TagWorldPass`, calls `DrawMesh` per mesh with entity's owned material. Legacy `draw_meshes()` (`ModelTransform` path) unchanged for non-migrated entities.

Headless safety: `IsWindowReady()` guard means `build_obstacle_model()` is no-op in tests → entities won't have `ObstacleModel` → view skips them. No `ModelTransform` emitted for these entities either → no double-draw.

---

## BF-3: Duplicate obstacle_archetypes deleted

**Files removed:** `app/systems/obstacle_archetypes.cpp`, `app/systems/obstacle_archetypes.h`

Both were ODR duplicates of the canonical `app/archetypes/` versions (identical function body). CMake `file(GLOB SYSTEM_SOURCES app/systems/*.cpp)` was picking both up. Fixed includes in:
- `app/systems/beat_scheduler_system.cpp`
- `app/systems/obstacle_spawn_system.cpp`
- `tests/test_obstacle_archetypes.cpp`

All now reference `archetypes/obstacle_archetypes.h`.

---

## BF-4: ObstacleParts geometry descriptor

**File:** `app/components/rendering.h`

`struct ObstacleParts` now carries six POD fields: `cx, cy, cz, width, height, depth` (all float, all zero-defaulted). No methods with logic. `build_obstacle_model()` populates at spawn:
- `cx=0, cy=0, cz=0` (slab left-edge at X=0, no local offsets)
- `width = SCREEN_W_F`
- `height = LOWBAR_3D_HEIGHT` or `HIGHBAR_3D_HEIGHT` (per kind)
- `depth = dsz->h` (40.0f from archetype DrawSize)

`static_assert(!std::is_empty_v<ObstacleParts>)` added to test file.

---

## Tests added

Section D in `tests/test_obstacle_model_slice.cpp`:
- `BF-4`: `static_assert(!std::is_empty_v<ObstacleParts>)` + zero-init field test
- `BF-2`: LowBar/HighBar archetypes do NOT produce `ModelTransform` + non-identity transform formula validation
- `BF-1`: `ObstacleModel` default-init has null mesh/material pointers (confirming manual construction contract)

---

## Validation

- `cmake -B build -S . -Wno-dev`: clean
- `cmake --build build --target shapeshifter_tests`: clean (0 errors, 0 warnings)
- `./build/shapeshifter_tests "~[bench]" --reporter compact`: 2975/2975 pass
- `cmake --build build --target shapeshifter`: clean
- `git diff --check`: clean
- `grep -rn 'LoadModelFromMesh(' app/ tests/`: no actual calls
- `grep -rn '"systems/obstacle_archetypes' app/ tests/`: no stale includes
