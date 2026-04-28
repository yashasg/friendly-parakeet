# Review Decision: Model Authority Slice (Keaton/Baer)

**Author:** Kujan  
**Date:** 2026-05-18  
**Status:** ❌ REJECTED  
**Reviewed:** Keaton (Slice 1 + 2) + Baer (test coverage)  
**Revision owner:** McManus (primary — BF-1, BF-2), Keyser (co-owner — BF-3, BF-4)  
**Keaton:** Locked out per reviewer rejection protocol.

---

## Verdict: FAIL

Four blocking findings. All must be resolved before re-review.

---

## Blocking Findings

### BF-1 (HIGH) — `LoadModelFromMesh` used in obstacle path

**File:** `app/gameobjects/shape_obstacle.cpp:116–117`

```cpp
Mesh mesh = GenMeshCube(constants::SCREEN_W_F, height, dsz->h);
Model model = LoadModelFromMesh(mesh);   // ← PROHIBITED
```

**Criterion:** B2 — "Zero calls to `LoadModelFromMesh` in the obstacle spawn path."  
**Why blocking:** User stated obstacles must use manually constructed models with owned mesh arrays. `LoadModelFromMesh` also calls `LoadMaterialDefault()` internally (GPU-dependent), making the material allocation opaque and untestable. The owned-mesh invariant (separate `RL_MALLOC` allocation, explicit `meshCount`/`materialCount`, `meshMaterial[i]` init) cannot be verified from `LoadModelFromMesh`.

**Required fix (McManus):**
```cpp
// Replace LoadModelFromMesh with manual construction:
Model model = {};
model.transform  = MatrixIdentity();
model.meshCount  = 1;
model.meshes     = static_cast<Mesh*>(RL_MALLOC(sizeof(Mesh)));
model.meshes[0]  = GenMeshCube(constants::SCREEN_W_F, height, dsz->h);
UploadMesh(&model.meshes[0], false);
model.materialCount = 1;
model.materials  = static_cast<Material*>(RL_MALLOC(sizeof(Material)));
model.materials[0] = LoadMaterialDefault();   // still GPU, still guarded by IsWindowReady()
model.meshMaterial  = static_cast<int*>(RL_CALLOC(model.meshCount, sizeof(int)));
// meshMaterial[0] = 0 (RL_CALLOC zeroes)
```

---

### BF-2 (HIGH) — Render path does not use owned `ObstacleModel`; camera writes legacy `ModelTransform`

**Files:** `app/systems/camera_system.cpp:261–279`, `app/systems/game_render_system.cpp:132–153`

**Camera system writes legacy `ModelTransform` for LowBar/HighBar:**
```cpp
// camera_system.cpp:267–268 (LowBar case)
reg.get_or_emplace<ModelTransform>(entity) =
    ModelTransform{slab_matrix(...), col, MeshType::Slab, 0};  // ← writes legacy component
```

**Render system has no owned-model draw loop:**
```cpp
// game_render_system.cpp: draw_meshes() only iterates ModelTransform
auto view = reg.view<const ModelTransform>();  // ← ObstacleModel/TagWorldPass never read
```

**Effect:** The owned `Model` allocated by `build_obstacle_model()` and its custom mesh (BF-1) is:
- Allocated every spawn (GPU memory consumed)
- Destroyed via RAII on entity death (GPU memory freed correctly)
- **Never drawn** — `TagWorldPass` emplacement in `build_obstacle_model()` is dead code

**Criteria violated:** B5 (Model.transform not authoritative), B7 (no `TagWorldPass` draw loop).

**Required fix (McManus):**

1. In `camera_system.cpp` section 1b: replace `ModelTransform` emission with direct write to `ObstacleModel.model.transform`:
   ```cpp
   auto view = reg.view<ObstacleTag, ObstacleScrollZ, ObstacleModel, ObstacleParts, Obstacle>();
   for (auto [entity, oz, om, pd, obs] : view.each()) {
       // Write scroll transform directly into the owned model
       om.model.transform = slab_matrix(0.0f, oz.z, constants::SCREEN_W_F, bar_height, pd.depth);
   }
   ```
2. In `game_render_system.cpp`: add owned-model draw loop alongside existing `ModelTransform` loop:
   ```cpp
   // draw_owned_models(): for entities with ObstacleModel + TagWorldPass
   auto owned_view = reg.view<const ObstacleModel, const TagWorldPass>();
   for (auto [entity, om] : owned_view.each()) {
       if (!om.owned || !om.model.meshes) continue;
       for (int i = 0; i < om.model.meshCount; ++i) {
           DrawMesh(om.model.meshes[i],
                    om.model.materials[om.model.meshMaterial[i]],
                    om.model.transform);
       }
   }
   ```
3. Remove `ModelTransform` emission for LowBar/HighBar from `camera_system.cpp:261–279` once the owned-model draw path is active.

---

### BF-3 (HIGH) — `app/systems/obstacle_archetypes.*` present — ODR violation

**Files:** `app/systems/obstacle_archetypes.cpp`, `app/systems/obstacle_archetypes.h`

**CMakeLists.txt lines 89 and 92:**
```cmake
file(GLOB SYSTEM_SOURCES   app/systems/*.cpp)   # picks up systems/obstacle_archetypes.cpp
file(GLOB ARCHETYPE_SOURCES app/archetypes/*.cpp)  # picks up archetypes/obstacle_archetypes.cpp
```

Both files define `apply_obstacle_archetype`. This is a C++ ODR violation: two translation units provide the same symbol. The linker will error or silently pick one; behavior is undefined.

**Additionally:** `tests/test_obstacle_archetypes.cpp:3` includes `"systems/obstacle_archetypes.h"`, not the canonical `"archetypes/obstacle_archetypes.h"`. Tests may be exercising a stale copy.

**Required fix (Keyser):**
1. Delete `app/systems/obstacle_archetypes.cpp` and `app/systems/obstacle_archetypes.h`.
2. Update `tests/test_obstacle_archetypes.cpp` include from `"systems/obstacle_archetypes.h"` → `"archetypes/obstacle_archetypes.h"`.
3. Verify no other file includes the `systems/` path.

---

### BF-4 (MEDIUM) — `ObstacleParts` is an empty tag, not a geometry descriptor

**File:** `app/components/rendering.h:105`

```cpp
struct ObstacleParts {};  // ← empty tag only
```

**Criterion:** B4 — "Each obstacle entity carries a typed part-descriptor component that records per-slab/per-shape geometry data (x, w, h, d) alongside the Model. At minimum: enough data for scroll_system to recompute model.transform each frame."

**Required fix (Keyser):**  
Replace the empty tag with geometry fields sufficient for the camera system to recompute `model.transform` per frame from `ObstacleScrollZ.z` + descriptor data, without reading raw mesh vertex buffers:
```cpp
struct ObstacleParts {
    float cx    = 0.0f;   // obstacle centre X
    float cy    = 0.0f;   // obstacle centre Y (vertical offset from ground)
    float cz    = 0.0f;   // local Z origin offset
    float width = 0.0f;   // total width in world coords
    float height= 0.0f;   // bar height in world coords
    float depth = 0.0f;   // slab depth in world coords
};
```
Populate from `build_obstacle_model()` at spawn time alongside the `ObstacleModel` emplace.

---

## What Passes — Do Not Regress

- LowBar/HighBar archetype: `ObstacleScrollZ` instead of `Position` ✅
- All lifecycle systems: scroll, cleanup, miss, collision, scoring bridge-state views ✅
- `IsWindowReady()` headless guard in `build_obstacle_model()` ✅
- `on_obstacle_model_destroy` owned-flag / double-unload protection ✅
- `ObstacleModel` RAII move/copy-delete semantics ✅
- All 898 test assertions pass ✅

---

## Re-review Conditions

1. BF-1: `LoadModelFromMesh` removed; manual mesh array construction in place.
2. BF-2: `camera_system` writes `ObstacleModel.model.transform`; `game_render_system` has owned-model draw loop; LowBar/HighBar no longer emit `ModelTransform`.
3. BF-3: `app/systems/obstacle_archetypes.*` deleted; test include updated to canonical path; build links cleanly.
4. BF-4: `ObstacleParts` has geometry fields; `build_obstacle_model()` populates them.
5. Tests must include at least one assertion that an LowBar entity with `ObstacleModel` (owned=true) produces a non-identity `model.transform` after a scroll + camera tick (can be done headlessly with a mocked `IsWindowReady` path or by directly invoking the camera-system logic on a manually emplaced owned model).

Re-route revised PR to Kujan for re-review.
