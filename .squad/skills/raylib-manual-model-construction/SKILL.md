---
name: "raylib-manual-model-construction"
description: "How to manually assemble a raylib Model struct with owned mesh arrays instead of LoadModelFromMesh"
domain: "rendering, raylib, gpu-resources"
confidence: "high"
source: "earned — LoadModelFromMesh was prohibited; manual construction required for ownership verification"
---

## Context

`LoadModelFromMesh` is convenient but prohibited in this codebase: it calls `LoadMaterialDefault()` internally (opaque GPU dependency), and the resulting mesh ownership is not separately verifiable. The correct pattern is to manually assemble the `Model` struct so mesh/material arrays are explicitly owned by the entity.

## Pattern

```cpp
// 1. Create a zero-initialised Model (no GPU calls yet)
Model model = {};
model.transform     = MatrixIdentity();

// 2. Allocate and fill mesh array
model.meshCount     = 1;
model.meshes        = static_cast<Mesh*>(RL_MALLOC(sizeof(Mesh)));
model.meshes[0]     = GenMeshCube(1.0f, 1.0f, 1.0f);  // ALWAYS unit cube — slab_matrix scales it

// 3. Allocate and fill material array
model.materialCount = 1;
model.materials     = static_cast<Material*>(RL_MALLOC(sizeof(Material)));
model.materials[0]  = LoadMaterialDefault();

// 4. Allocate mesh→material mapping (zero = all meshes use material[0])
model.meshMaterial  = static_cast<int*>(RL_CALLOC(model.meshCount, sizeof(int)));

// 5. Store in RAII wrapper
auto& om = reg.emplace<ObstacleModel>(logical);
om.model = model;
om.owned = true;
```

## Key Notes

- **No `UploadMesh` after `GenMeshCube`** — raylib 5.5 GenMesh* functions already return uploaded (GPU-side) meshes. Calling `UploadMesh` again causes a double upload / potential GPU leak.
- **`RL_MALLOC` / `RL_CALLOC`** — use these instead of `malloc`/`calloc` to match raylib's internal allocator. `UnloadModel` uses `RL_FREE` to free them.
- **RAII owned flag** — set `om.owned = true` after manual construction so `UnloadModel` is called on destroy. Default `owned=false` is the headless-safe no-op path.
- **`IsWindowReady()` guard** — must wrap the entire construction block. GenMesh* + LoadMaterialDefault require an active OpenGL context.
- **Unit cube mesh is mandatory** — `GenMeshCube(1.0f, 1.0f, 1.0f)` must be used. `slab_matrix` applies `MatrixScale(w,h,d)` to scale a unit-cube to world dimensions. Using `GenMeshCube(w,h,d)` instead squares every dimension at render time (double-scale defect).
- **raylib Matrix layout** — column-major: scale diagonal is `m0, m5, m10`; translation is `m12, m13, m14`. Fields `m3, m7, m11` are row 3 col 0-2, always zero in a standard TRS matrix.

## Examples

- `app/gameobjects/shape_obstacle.cpp` — `build_obstacle_model()`: full manual construction for LowBar/HighBar.

## Anti-patterns

- **`LoadModelFromMesh(mesh)`** — prohibited. Opaque material allocation, unverifiable ownership.
- **`UploadMesh` after `GenMesh*` in raylib 5.5`** — causes double upload. GenMesh* already uploads.
- **Storing `Mesh` on the stack then passing to `LoadModelFromMesh`** — mesh ownership becomes ambiguous.
- **`GenMeshCube(w, h, d)` with real dimensions** — double-scale defect. Always use unit cube; let `slab_matrix` apply the scale.
