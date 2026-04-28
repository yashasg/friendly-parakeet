# Authority Revision Checklist — McManus + Kujan Final Review

**Author:** Keyser  
**Date:** 2026-05-18  
**Status:** ACTIVE — McManus revision guide  
**Scope:** BF-1..BF-4 from Kujan's rejection. Read-only. Do not widen scope to P2/P3 kinds.

---

## State of current diff (HEAD ef7767d)

**None of the four blocking findings are resolved.** This checklist translates each one to the exact line-level change McManus must make.

---

## MUST-FIX checklist

### BF-1 — `LoadModelFromMesh` in `app/gameobjects/shape_obstacle.cpp:116–117`

```
❌  Model model = LoadModelFromMesh(mesh);
```

**Replace** with manual construction (exact pattern from Kujan's review):
```cpp
Model model = {};
model.transform     = MatrixIdentity();
model.meshCount     = 1;
model.meshes        = static_cast<Mesh*>(RL_MALLOC(sizeof(Mesh)));
model.meshes[0]     = GenMeshCube(constants::SCREEN_W_F, height, dsz->h);
UploadMesh(&model.meshes[0], false);
model.materialCount = 1;
model.materials     = static_cast<Material*>(RL_MALLOC(sizeof(Material)));
model.materials[0]  = LoadMaterialDefault();
model.meshMaterial  = static_cast<int*>(RL_CALLOC(model.meshCount, sizeof(int)));
```

Keep the existing `IsWindowReady()` early-return guard above this block — it is already in place and must not be removed.

**After this change also populate `ObstacleParts` (BF-4 is coupled here — see below).**

---

### BF-2 — `camera_system.cpp:261–279` still emits `ModelTransform`; `game_render_system.cpp:136` never reads `ObstacleModel`

Two sub-steps; both must land in the same changeset:

**2a. camera_system — section 1b (lines 261–279)**

Replace the `ModelTransform` emplace block with a direct write into `ObstacleModel.model.transform`:

```cpp
// 1b. Model-authority vertical bars: write transform directly into owned model.
{
    auto view = reg.view<ObstacleTag, ObstacleScrollZ, ObstacleModel, ObstacleParts, Obstacle>();
    for (auto [entity, oz, om, pd, obs] : view.each()) {
        if (!om.owned) continue;
        switch (obs.kind) {
            case ObstacleKind::LowBar:
                om.model.transform = slab_matrix(
                    pd.cx, oz.z, pd.width, constants::LOWBAR_3D_HEIGHT, pd.depth);
                break;
            case ObstacleKind::HighBar:
                om.model.transform = slab_matrix(
                    pd.cx, oz.z, pd.width, constants::HIGHBAR_3D_HEIGHT, pd.depth);
                break;
            default:
                break;
        }
    }
}
```

Remove the `Color` and `DrawSize` from the view query — they are no longer needed once `ObstacleParts` carries geometry and `om.model.materials[0]` is set at spawn.

Do **not** remove the existing `ModelTransform` section 1a (Position-based path) — that is P1 scope and must not regress.

**2b. game_render_system.cpp — add owned-model draw loop after `draw_meshes()`**

Add a new static function and call it from `game_render_system`:

```cpp
static void draw_owned_models(const entt::registry& reg) {
    auto view = reg.view<const ObstacleModel, const TagWorldPass>();
    for (auto [entity, om] : view.each()) {
        if (!om.owned || !om.model.meshes) continue;
        for (int i = 0; i < om.model.meshCount; ++i) {
            DrawMesh(om.model.meshes[i],
                     om.model.materials[om.model.meshMaterial[i]],
                     om.model.transform);
        }
    }
}
```

Call it after the existing `draw_meshes(reg)` call in the render pass block (line ~173). Do **not** remove the existing `draw_meshes` — it still serves P1 entities.

---

### BF-3 — ODR: `app/systems/obstacle_archetypes.*` must be deleted

**Confirmed:** Both `app/systems/obstacle_archetypes.cpp` and `app/archetypes/obstacle_archetypes.cpp` define `apply_obstacle_archetype`. This is a hard ODR violation — linker behaviour is undefined.

Steps:
1. `git rm app/systems/obstacle_archetypes.cpp app/systems/obstacle_archetypes.h`
2. In `tests/test_obstacle_archetypes.cpp:3`, change:
   ```cpp
   #include "systems/obstacle_archetypes.h"   // ← stale
   ```
   to:
   ```cpp
   #include "archetypes/obstacle_archetypes.h"  // ← canonical
   ```
3. Scan all other includes:
   ```
   grep -rn "systems/obstacle_archetypes" app/ tests/
   ```
   Must return zero results after the delete.
4. Re-run `cmake --build build` — confirm the linker no longer sees two `apply_obstacle_archetype` definitions.

---

### BF-4 — `ObstacleParts` is an empty tag in `app/components/rendering.h:105`

```
❌  struct ObstacleParts {};
```

**Replace** with:
```cpp
struct ObstacleParts {
    float cx     = 0.0f;   // obstacle centre X
    float cy     = 0.0f;   // obstacle centre Y offset from ground
    float cz     = 0.0f;   // local Z origin offset
    float width  = 0.0f;   // total world-space width
    float height = 0.0f;   // bar height in world coords
    float depth  = 0.0f;   // slab depth in world coords
};
```

In `shape_obstacle.cpp`, **after** the `RL_MALLOC` construction block (BF-1 fix), populate it:
```cpp
auto& pd = reg.get_or_emplace<ObstacleParts>(logical);
pd.cx    = 0.0f;                          // LowBar/HighBar are full-width, centered at 0
pd.cy    = 0.0f;
pd.cz    = 0.0f;
pd.width = constants::SCREEN_W_F;
pd.height = height;
pd.depth  = dsz->h;                       // dsz->h is slab depth already used for GenMeshCube
```

This is the data BF-2 section 1b reads for recomputing `model.transform` each frame.

---

## MUST-NOT-CHANGE (do not widen scope)

- `Position` emission for ShapeGate, LaneBlock, ComboGate, SplitPath, LanePushLeft/Right in `app/archetypes/obstacle_archetypes.cpp` — **correct**, these are P1-scope entities and keep Position.
- Section 1a of `camera_system.cpp` (Position-based `ModelTransform` emplace) — **must stay**; P1 entities still use it.
- `draw_meshes()` in `game_render_system.cpp` — **must stay**; still serves P1 entities via `ModelTransform`.
- `ObstacleScrollZ` emplace for LowBar/HighBar in the new `app/archetypes/obstacle_archetypes.cpp:42,50` — **already correct**; do not regress.
- All `IsWindowReady()` guard + RAII `owned` flag patterns — **must stay untouched**.

---

## Acceptance scans (run before re-review)

```bash
# BF-1: must return zero
grep -rn "LoadModelFromMesh" app/

# BF-2a: LowBar/HighBar must NOT emit ModelTransform anywhere in camera_system
grep -n "ModelTransform" app/systems/camera_system.cpp | grep -i "lowbar\|highbar\|1b\|oz\."

# BF-2b: draw_owned_models must exist and be called
grep -n "draw_owned_models\|TagWorldPass" app/systems/game_render_system.cpp

# BF-3: systems/obstacle_archetypes must be gone
ls app/systems/obstacle_archetypes* 2>&1 | grep "No such file"
grep -rn "systems/obstacle_archetypes" app/ tests/   # must be empty

# BF-4: ObstacleParts must have fields
grep -A 8 "struct ObstacleParts" app/components/rendering.h

# ODR final check — symbol should appear exactly once
grep -rn "^void apply_obstacle_archetype\|^entt::entity create_obstacle_base" app/
# expect: only app/archetypes/obstacle_archetypes.cpp
```

---

## Additional architectural hazard (non-blocking but call out to McManus)

**`camera_system.cpp:286` — `MeshChild` section still reads `Position` directly from parent:**
```cpp
auto& parent_pos = reg.get<Position>(mc.parent);  // line 286
```
This is **not** part of this slice and must not be changed here. Flag as a known P2 item: when `MeshChild`-carrying entities migrate, this line will also need a `ScrollZ` read. Do not touch it now.

---

## Re-review criteria for Kujan

All four BF items resolved per the checklist above, verified by the acceptance scans. At least one new test asserting a LowBar entity with `ObstacleModel` (owned=true) produces a non-identity `model.transform` after directly invoking the camera-system §1b logic on a manually emplaced entity (headless: manually emplace `ObstacleModel{owned=false}`, call the view logic, check `transform != MatrixIdentity()`).
