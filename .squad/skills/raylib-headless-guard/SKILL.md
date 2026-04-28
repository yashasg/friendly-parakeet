---
name: "raylib-headless-guard"
description: "How to safely call GPU-dependent raylib functions from code paths that unit tests also exercise"
domain: "testing, raylib, gpu-resources"
confidence: "high"
source: "earned — discovered when build_obstacle_model caused SIGSEGV in headless tests"
---

## Context

raylib's GPU functions (`GenMeshCube`, `UploadMesh`, `LoadMaterialDefault`, etc.) require an active OpenGL context created by `InitWindow`. Unit tests in this project run headless (no window). When production spawn systems call GPU factory functions, those same code paths are exercised by tests — causing crashes.

## Patterns

### `IsWindowReady()` guard

Wrap any function that calls GPU functions in an early return:

```cpp
void build_obstacle_model(entt::registry& reg, entt::entity e) {
    if (!IsWindowReady()) return;  // headless/test mode: skip GPU construction
    // ... GenMeshCube, UploadMesh, LoadMaterialDefault ...
}
```

`IsWindowReady()` is a zero-cost raylib predicate that returns `true` only after `InitWindow()` has been called. It's the idiomatic raylib way to guard GPU paths.

### RAII wrapper with `owned` flag

GPU resource structs must use an `owned` flag to make default-construction GPU-free:

```cpp
struct ObstacleModel {
    Model model = {};
    bool  owned = false;     // default false → destructor is a no-op

    ~ObstacleModel() { release(); }
    void release() {
        if (!owned) return;  // safe without GPU context
        UnloadModel(model);
        model = {}; owned = false;
    }
};
```

Default-constructed `ObstacleModel` is safe to create and destroy in headless tests.

## Examples

- `app/gameobjects/shape_obstacle.cpp` — `build_obstacle_model()`: `IsWindowReady()` guard prevents `GenMeshCube` crash in test environment.
- `app/systems/camera_system.h` — `camera::ShapeMeshes`: `owned = false` default, `release()` no-ops without GPU.
- `tests/test_gpu_resource_lifecycle.cpp` — validates type traits and headless lifecycle of all GPU RAII wrappers.

## Anti-Patterns

- **Calling `GenMesh*`, `UploadMesh`, `LoadMaterial*` unconditionally** in functions also reachable from tests. This causes SIGSEGV without `InitWindow`.
- **Using `IsWindowReady()` inside constructors** — the struct won't be re-initialized if the GPU becomes available later. Use the guard in factory/builder functions instead.
- **Not guarding `on_destroy` listeners** — if a component carries GPU resources and the listener calls `UnloadMesh`/`UnloadMaterial`, it will crash in tests. Use the `owned` flag to make the listener a no-op when no GPU resources were allocated.
