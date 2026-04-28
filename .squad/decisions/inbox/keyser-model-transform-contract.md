# Model-Centric Transform Contract (Revision 2)
**Author:** Keyser  
**Date:** 2026-05-18  
**Supersedes:** `keyser-transform-contract.md`  
**Status:** READY FOR REVIEW — 5 hard questions listed in §4 must be answered before implementation begins  
**Executors:** Keaton (implementation), McManus (integration + tests)

---

## 1. What Changed and Why

The original contract (`keyser-transform-contract.md`) introduced `Transform { Matrix mat }` as a separate ECS component that `game_camera_system` would read each frame and convert into a `ModelTransform` output. This is a two-step chain with a redundant intermediate.

The updated user directives eliminate the intermediary entirely:

> *"For visible entities that own a raylib `Model`, `Model.transform` is the authoritative transform. Do not keep a separate gameplay `Transform` component that must be copied/synced into `Model.transform` for those entities."*

**New model:** Visible entities own a `raylib::Model` by value as an ECS component. `model.transform` IS the world-space matrix. No separate `Transform` component. No `ModelTransform` camera-system output. The render system reads the entity's `Model` directly and calls `DrawMesh` or `DrawModel`.

Additional directives that supersede the original contract:
- No generic `Velocity` — replace with domain-specific motion data
- Render-pass membership via empty tag components (not an enum field)
- Model is unloaded when the owning entity is destroyed (not at session shutdown)
- Camera is also an entity — separate scope, confirmed by user

---

## 2. Entity Classes Under the New Architecture

### Class A — Visible 3D Entities (own `raylib::Model`)
Player, obstacles, particles (rendered quads).

- ECS component: `Model model;` (by value, owned by entity)
- `model.transform` = authoritative world-space Matrix
- No separate `Transform`, `Position`, or `ModelTransform` component on these entities
- `model.meshes` reference shared GPU geometry from `ShapeMeshes` ctx singleton (not copied — see §4.HQ1)
- `model.materials` is a per-entity copy (for per-entity tint)
- On entity destroy: EnTT `on_destroy<Model>` listener calls `UnloadModel` — but only frees the per-entity material copy, not the shared mesh VBO (see §4.HQ2)

**Render pass tag** (empty struct, per user directive):
```cpp
struct TagWorldPass {};   // drawn in BeginMode3D
struct TagEffectsPass {}; // particles, post-process
struct TagHUDPass {};     // screen-space UI
```
One tag per entity; `game_render_system` queries each pass's view independently.

### Class B — Non-Visible World-Anchored Entities (no `Model`)
`ScorePopup`: has a world-space position used for `GetWorldToScreenEx` projection, but no mesh.

- Keep a minimal `WorldPos { float x; float y; }` component (rename of current `Position`)
- `ui_camera_system` reads `WorldPos` instead of `Position`
- These entities are **not** in `TagWorldPass`

### Class C — Pure-Logic Entities
Timing markers, `BeatInfo` carriers with no visual. No position component at all.

---

## 3. `transform.h` Fate: **Shrink to non-visible-only**

| Symbol | Fate | Rationale |
|---|---|---|
| `struct Position { float x, y; }` | **Rename to `WorldPos`** | Retained only for Class B (non-visible world-anchored entities). Not used by any entity that owns a `Model`. |
| `struct Velocity { float dx, dy; }` | **Delete** | Replaced by domain-specific motion data (see §3.1). |
| `struct Transform { Matrix mat; }` | **Not introduced** | The previous contract's proposed `Transform` component is skipped entirely — `Model.transform` serves this role for Class A, `WorldPos` serves it for Class B. |

The file `app/components/transform.h` survives but is renamed or narrowed to contain only `WorldPos`. Its `#include <cstdint>` becomes a `#include <raylib.h>` (since `WorldPos` may not need raylib directly, but scroll_system does).

### 3.1 Domain-Specific Motion (replacing `Velocity`)

| Entity type | Motion data | System that writes `model.transform` |
|---|---|---|
| Rhythm obstacles | `BeatInfo { spawn_time, arrival_time }` + `SongState.scroll_speed` | `scroll_system` (absolute song-time-driven) |
| Freeplay obstacles | `ObstacleMotion { float scroll_speed; }` (new tiny component) | `scroll_system` (dt-based) |
| Player | `Lane`, `VerticalState` (unchanged) | `player_movement_system` |
| Particles | `ParticleData.vx/vy` (add fields, or new `ParticleMotion`) | `particle_update_system` |
| ScorePopup | `WorldPos` incremented by popup logic | `scoring_system` or popup handler |

---

## 4. Hard Questions Before Code Changes

These are blocking. Each must be answered before Keaton starts Slice 0.

---

### HQ1 — Shared mesh ownership in entity-owned `Model`

`ShapeMeshes` ctx singleton owns the GPU-uploaded meshes (slab, quad, 4 shape meshes). If each entity's `Model.meshes` points into `ShapeMeshes` memory, the entity does NOT own those meshes — it only borrows them.

**Proposed pattern:** At spawn, build the entity `Model` like this:
```cpp
Model m = {};
m.meshCount    = 1;
m.meshes       = &sm.slab;          // pointer into ShapeMeshes — NOT owned
m.materialCount = 1;
m.materials    = (Material*)RL_MALLOC(sizeof(Material));
m.materials[0] = sm.material;       // copy, tint set per entity
m.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = entity_color;
m.transform    = slab_matrix(x, z, w, h, d);
// DO NOT call UploadMesh — mesh is already on GPU
```

`on_destroy<Model>` listener:
```cpp
void on_model_destroy(entt::registry& reg, entt::entity e) {
    auto& m = reg.get<Model>(e);
    RL_FREE(m.materials);      // free per-entity material copy
    // DO NOT call UnloadMesh — mesh is shared
    m.materials    = nullptr;
    m.meshes       = nullptr;  // clear borrow pointer
}
```

**Question for user:** Is this borrow-pointer pattern acceptable, or should each entity hold a full deep-copy of the mesh (per-entity GPU upload)?  
*Recommendation: borrow-pointer. The shared ShapeMeshes lifetime (camera::init → camera::shutdown) already envelops all entity lifetimes via the existing game loop order.*

---

### HQ2 — Multi-mesh obstacle representation (biggest structural decision)

ShapeGate, ComboGate, SplitPath each need 2–3 visual meshes. Three options:

**Option A — Single `Model` with `meshCount > 1`**  
All slabs + shape for one obstacle packed into one Model. Each mesh has its own local transform baked as an offset from the obstacle's origin. Scroll update patches the top-level `model.transform` origin; each mesh offset remains fixed. `DrawModel(model, pos, 1.0f, WHITE)` draws all in one call.

*Con:* Building a dynamic multi-mesh Model requires managing the meshes array; offsets must be computed at spawn and stored separately for re-computation when Z changes (scroll).

**Option B — Typed auxiliary model components**  
Primary mesh on `Model model`, extra meshes on typed components `AuxModel1`, `AuxModel2`. Render system queries each type separately. Scroll system writes to `model.transform` (primary) and uses a fixed offset to compute `aux1.transform`, `aux2.transform`.

*Con:* Max aux mesh count varies per obstacle kind; awkward fixed-component typing for variable geometry.

**Option C — Keep MeshChild child entities, but each MeshChild now owns a `Model`**  
Logical obstacle entity: owns `Model` (primary mesh for collision and scroll). Child entities: each owns its own `Model`. `MeshChild.z_offset` is still used to offset from parent's `model.transform`.

*Pro:* Minimal change from current architecture; existing `ObstacleChildren` + `on_obstacle_destroy` pattern works as-is.  
*Con:* Child entities with their own `Model` require the same `on_destroy<Model>` listener.

**User decision needed:** Which option? This determines Slice 0's implementation path entirely.  
*Recommendation: Option C initially — lowest risk, preserves existing spawner logic, only changes what each child entity carries (`Model` instead of `ModelTransform`).*

---

### HQ3 — `Model.transform` content: translation-only vs full TRS

The current `slab_matrix` helper returns a full TRS matrix:
```cpp
MatrixMultiply(MatrixScale(w, h, d), MatrixTranslate(x + w/2, h/2, z + d/2))
```
The Z-translation in this matrix is NOT simply `m14` — it's `d/2 + z` multiplied into a scaled column. Patching `m14` alone after a scroll update would corrupt the scale encoding.

**Consequence for scroll_system:** When Z changes, scroll_system must recompute the full `model.transform` using `slab_matrix(x, new_z, w, height, d)`. This requires storing `{x, w, height, d}` on the entity (currently in `DrawSize {w, h}` + constants). For the scroll path to work cleanly:

```cpp
// In scroll_system for rhythm obstacles:
auto& m   = reg.get<Model>(entity);
auto& dsz = reg.get<DrawSize>(entity);
float z   = constants::SPAWN_Y + (song->song_time - info.spawn_time) * song->scroll_speed;
m.transform = slab_matrix(obs_x, z, dsz.w, constants::OBSTACLE_3D_HEIGHT, dsz.h);
```

`obs_x` must be accessible. Options: store in `DrawSize`, or add `ObstacleX { float x; }`, or re-read from the entity's `Obstacle.kind` + `Lane`.

**Question for user:** Should `DrawSize` be extended with `float x` for the scroll-path lookup? Or is there a cleaner place to store the obstacle's fixed x-coordinate once it's on a `Model`?  
*Recommendation: Extend `DrawSize` with `float x = 0.0f` for now; rename to `MeshParams` in a future pass.*

---

### HQ4 — Camera entity: in scope or deferred?

User directive: *"Camera is also an entity with a Transform, a camera component, and a render target."*

This affects `camera::init` (ctx emplace → entity create), `game_render_system` (reads camera from ctx → reads from entity), and `ui_camera_system`. It also introduces a new question: does a camera entity own a `Model` (i.e., does it render anything), or just a `CameraComponent` holding `Camera3D`?

This is non-trivial scope added to an already large migration.

**Question for user:** Is camera-as-entity part of this sprint, or a follow-up issue after Model adoption is complete?  
*Recommendation: Defer to a dedicated issue. The migration is already large; camera entity is additive and doesn't block obstacle/player Model adoption.*

---

### HQ5 — `ScorePopup` and other Class B entities: `WorldPos` vs give them a `Model`

`ScorePopup` needs a world-space {x, y} for `GetWorldToScreenEx`. It renders no mesh.

**Option A (recommended):** Keep `WorldPos { float x, float y; }` in `transform.h` (renamed `Position`). `ui_camera_system` reads `WorldPos`. Class B entities are explicitly not `TagWorldPass` entities.

**Option B:** Give `ScorePopup` a `Model` with `meshCount = 0`. Purely for transform storage. `ui_camera_system` reads `model.transform.m12` and `model.transform.m14` for {x, y}. Avoids needing `WorldPos` at all, but using a `Model` with no mesh is an unusual pattern and wastes a material-array alloc.

**Question for user:** Which representation for non-visible world-anchored entities?  
*Recommendation: Option A — `WorldPos` in `transform.h`. Explicit, minimal, correct.*

---

## 5. First Safe Implementation Slice (post-cleanup)

The archetype folder move (#344) is already complete. The first safe implementation slice is:

**Slice 0 — Wire up Model resource infrastructure (no behavior change)**

1. Add `on_destroy<Model>` listener in `camera::init` (or `game_loop_init`) that frees per-entity material copies and clears borrow pointers. No entity yet carries a `Model` so this is inert.
2. Define the three render-pass tag structs (`TagWorldPass`, `TagEffectsPass`, `TagHUDPass`) in a new `app/components/render_tags.h`.
3. Add `WorldPos { float x, float y; }` to `transform.h` (alongside `Position` briefly, or replacing it immediately for Class B entities).

These three changes are additive, compile cleanly, and don't touch any system logic.

**Slice 1 — Single-mesh obstacle adoption (LowBar, HighBar, LanePushLeft, LanePushRight)**

These four obstacle kinds produce exactly one slab mesh. Safest first target.

1. In `apply_obstacle_archetype` for these four cases: construct a `Model` (borrow-pointer to `sm.slab`, per-entity material) and emplace it. Remove `Position` emplace for these cases. Tag with `TagWorldPass`.
2. In `scroll_system`: for entities with `Model` (not `WorldPos`), recompute and set `model.transform` using `slab_matrix`. Keep existing `WorldPos`-based path for Class B entities.
3. In `game_camera_system`: remove `ModelTransform` writes for these four kinds.
4. In `game_render_system`: add a `reg.view<Model, TagWorldPass>()` draw path alongside the existing `reg.view<ModelTransform>()` path. Both coexist until migration is complete.
5. Tests: Update archetype tests to verify `Model` presence instead of `Position`/`ModelTransform`. Verify scroll produces correct `model.transform.m14` values.

**Slice 2+ — Multi-mesh obstacles, player, particles** (sequenced after HQ2 answer)

---

## 6. What Is NOT Changing in This Migration

| Component / System | Status | Reason |
|---|---|---|
| `ModelTransform` | **Removed progressively** | Replaced by reading `Model` directly in render system; removed per-archetype as each adopts `Model` |
| `MeshChild` + `ObstacleChildren` | **Kept or evolved per HQ2 answer** | If Option C: MeshChild entities now own `Model`; structure is otherwise unchanged |
| `DrawSize { w, h }` | **Kept (possibly extended with x)** | Slab dimensions needed for matrix recomputation in scroll_system |
| `ScreenTransform` ctx singleton | **Stays** | Letterbox scale/offset; not per-entity, not a Matrix |
| `ScreenPosition` | **Stays** | Computed screen-space for Class B entity projection |
| Local matrix helpers in `camera_system.cpp` | **Stays** | `slab_matrix`, `shape_matrix`, `prism_matrix`, `make_shape_matrix` are promoted to `app/util/render_matrix_helpers.h` for use by spawn and scroll systems |
| `ShapeMeshes` ctx singleton | **Stays** | Shared GPU mesh owner; entity Models borrow from it |
| `GameCamera`, `UICamera`, `RenderTargets` ctx | **Stays** (camera entity deferred per HQ4) | |
| `DrawLayer` enum | **Replaced** by `TagWorldPass` / `TagEffectsPass` / `TagHUDPass` empty struct tags |

---

## 7. Zero-Warning Invariant

- `Model` is a C struct; no destructor warning risk from EnTT. The `on_destroy<Model>` listener covers cleanup.
- `RL_FREE` for per-entity materials: ensure no double-free if `on_destroy` fires on an entity that never got a `Model` (guard: `if (m.materials) RL_FREE(m.materials)`).
- WASM: `<raylib.h>` already included in all affected systems; no new platform guards needed.
- Render-pass tag empty structs: zero-size components, no warnings.

---

## 8. Test Coverage Expectations (McManus)

- `on_destroy<Model>` listener: unit test that destroys an entity carrying a Model and verifies materials pointer is null afterward (no use-after-free).
- Scroll tests: after Slice 1, assert `model.transform` Z channel matches expected `slab_matrix` output for a given `song_time - spawn_time`.
- Archetype tests: for Slice 1 kinds, assert `Model` component is present, `Position`/`ModelTransform` are absent.
- Render-pass tag tests: assert each archetype emplaces exactly one render-pass tag.
