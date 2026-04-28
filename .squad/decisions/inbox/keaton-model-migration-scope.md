# Model.transform Migration — Scope & First Slice

**Date:** 2026-04-28  
**Author:** Keaton  
**Status:** ANALYSIS ONLY — no production code changed

---

## What Was Audited

All files in the input artifact list plus `camera_system.h`, `player_archetype.cpp`, `game_loop.cpp`, `test_gpu_resource_lifecycle.cpp`, and the existing RAII patterns for `ShapeMeshes` / `RenderTargets`.

---

## Current Architecture (Baseline)

### Rendering pipeline (3 passes each frame):
1. **`game_camera_system`** — computes a `ModelTransform` component (Matrix + tint + MeshType + mesh_index) onto every visible entity.
2. **`game_render_system`** — iterates `ModelTransform` and calls `DrawMesh(shared_mesh, mat, mt.mat)`.
3. Shared meshes live in `camera::ShapeMeshes` (registry ctx singleton): 4 shape meshes + 1 slab mesh + 1 quad mesh + 1 shared material/shader.

### No per-entity GPU ownership today:
- Zero calls to `LoadModel`, `LoadModelFromMesh`, or per-entity `UnloadModel` anywhere in `app/`.
- `on_obstacle_destroy` only destroys child *entities* (no GPU unload).
- `cleanup_system` calls `reg.destroy(e)` — triggers the `on_destroy<ObstacleTag>` listener, which destroys child entities only.

---

## Resource Lifetime: Critical Facts

| Resource | Current Owner | Unload Call | Called From |
|---|---|---|---|
| `Mesh shapes[4]` | `camera::ShapeMeshes` | `UnloadMesh` | `camera::shutdown()` via `release()` |
| `Mesh slab` | `camera::ShapeMeshes` | `UnloadMesh` | `camera::shutdown()` via `release()` |
| `Mesh quad` | `camera::ShapeMeshes` | `UnloadMesh` | `camera::shutdown()` via `release()` |
| `Material material` | `camera::ShapeMeshes` | `UnloadMaterial` | `camera::shutdown()` via `release()` |
| Shader (inside material) | `camera::ShapeMeshes` | `UnloadShader` | `camera::shutdown()` via `release()` |
| Per-entity GPU resources | **none** | N/A | N/A |

`ShapeMeshes` already has a battle-tested `owned`-flag RAII pattern identical to `RenderTargets`. `camera::shutdown()` explicitly calls `release()` before `CloseWindow()`, and the ctx destructor no-ops because `owned` is cleared.

---

## The Migration Problem

### raylib `Model` is not RAII-safe by default
```c
typedef struct Model {
    Matrix transform;
    int meshCount;
    int materialCount;
    Mesh     *meshes;       // heap pointer
    Material *materials;    // heap pointer
    int      *meshMaterial; // heap pointer
    int boneCount;
    BoneInfo *bones;
    Transform *bindPose;
} Model;
```

`Model` is a C struct with raw heap pointers. Storing it by value in EnTT requires:
- **No copy** — copying would alias all mesh/material pointers → double-free on destruction.
- **Move** requires zeroing source pointers (C++ move semantics, which `Model` doesn't have natively).
- **`UnloadModel` must fire exactly once per entity** before `reg.destroy(e)`.

### `LoadModelFromMesh` borrows, not owns
`LoadModelFromMesh(mesh)` copies the mesh *pointer* into `model.meshes[0]`, not the mesh data. Calling `UnloadModel` on such a model **frees the shared mesh**, corrupting all other entities that reference it.

**Consequence:** For entities that draw from the shared `ShapeMeshes` pool, a per-entity `Model` must use a **partial unload** — free the wrapper arrays (`RL_FREE(model.meshes)`, `RL_FREE(model.materials)`, `RL_FREE(model.meshMaterial)`) but NOT call `UnloadMesh` on the borrowed mesh.

---

## First Slice Recommendation

### Scope: Player entity only

Rationale: The player is a single entity with no child-entity relationships, no bulk destroy path, and a well-defined spawn site (`player_archetype.cpp`). It is the lowest-risk entity to migrate first.

### New component: `OwnedModel` RAII wrapper

Analogous to `camera::ShapeMeshes` and `RenderTargets`:
- Wraps `Model model` + `bool owned`
- Destructor calls partial unload (NOT `UnloadMesh` — mesh borrowed from shared pool)
- Copy-deleted, move-constructible, move-assignable
- `Model.transform` is the authoritative world transform for this entity

```cpp
// app/components/rendering.h
struct OwnedModel {
    Model model  = {};
    bool  owned  = false;

    OwnedModel() = default;
    OwnedModel(const OwnedModel&)            = delete;
    OwnedModel& operator=(const OwnedModel&) = delete;
    OwnedModel(OwnedModel&&) noexcept;
    OwnedModel& operator=(OwnedModel&&) noexcept;
    ~OwnedModel();

    // Partial unload: frees wrapper arrays, does NOT call UnloadMesh.
    // Safe to call multiple times; clears owned flag.
    void release_borrowed();
};
```

### Files to edit in Slice 1

| File | Change |
|---|---|
| `app/components/rendering.h` | Declare `OwnedModel` struct |
| `app/systems/camera_system.cpp` (.h) | Implement `OwnedModel` move/dtor; player branch writes `model.transform` not `ModelTransform` |
| `app/archetypes/player_archetype.cpp` | Emplace `OwnedModel` on player (built from shared mesh via `LoadModelFromMesh`) |
| `app/systems/game_render_system.cpp` | Add `OwnedModel` draw pass (`DrawModel` or `DrawMesh` with `model.transform`) |
| `tests/test_gpu_resource_lifecycle.cpp` | Add `OwnedModel` type-trait static_asserts + move-ownership test |

### What stays unchanged in Slice 1
- `camera::ShapeMeshes` — still owns all GPU mesh data
- `ModelTransform` component — still used for all obstacle + particle + mesh-child entities
- `on_obstacle_destroy` — no changes
- `cleanup_system` — no changes
- Obstacle mesh-child architecture — deferred to Slice 2+

---

## Blockers Before Full Migration

1. **Partial unload semantics must be documented and enforced** — any `OwnedModel` that uses borrowed meshes must call `release_borrowed()`, not `UnloadModel`. A comment + static method name is the only guard; a future slice could use ownership tagging.

2. **Obstacle parent-child mesh entities** — `MeshChild` + `ObstacleChildren` form a two-entity-level graph. Migrating obstacles requires either:
   - Keeping the logical/child split and giving each child an `OwnedModel` with borrowed mesh
   - Collapsing mesh-children into inline `Model`s on the logical entity (architectural change)
   - Either way, `on_obstacle_destroy` must call `release_borrowed()` on each child before `reg.destroy`.

3. **`Velocity` component on obstacles** — user directive says "no generic Velocity; use domain-specific motion data". This is a separate concern but will touch the same obstacle archetype files.

4. **`Position` component** — directive says position can be derived from `model.transform`. Full removal of `Position` for player is Slice 2 (after `OwnedModel` is validated). Collision system and scroll system both read `Position` — removing it requires updating all those systems simultaneously.

---

## Tests to Run After Slice 1

```bash
./run.sh test
# Specifically:
# test_gpu_resource_lifecycle — new OwnedModel type-trait tests
# test_collision_system — player Position still present in Slice 1, should be unchanged
# test_components — regression
```

