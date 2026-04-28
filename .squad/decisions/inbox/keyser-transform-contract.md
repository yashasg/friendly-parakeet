# Transform Migration Contract
**Author:** Keyser  
**Date:** 2026-04-28  
**Executors:** Keaton (implementation), McManus (integration + tests)  
**Status:** READY FOR EXECUTION — pending answers to blocking questions in §4

---

## 1. What We're Replacing and Why

Current state: `transform.h` holds `Position {float x, y}` and `Velocity {float dx, dy}`. All movement systems read/write `Position` directly. `game_camera_system` derives the render matrix from Position + DrawSize + constants each frame and writes it into `ModelTransform`.

Target state: `Transform { Matrix mat }` replaces `Position` as the source of truth for world-space location of any entity that moves or occupies a position in the game world. `Velocity` stays. `ModelTransform` stays (camera_system still produces it from Transform + size/color/mesh data). The existing local matrix helpers inside `camera_system.cpp` get promoted to a shared helper header.

This matches every user directive from 2026-04-28:
- *"If we use Transform, we do not need a separate Vector2 for position"*
- *"Pretty much every entity that moves should have a Transform component"*
- *"Matrix-backed Transform updates should use raylib/EnTT-friendly helper APIs"*

---

## 2. Coordinate Mapping

The game uses a 2D logical coordinate space:
- `x` = left/right (maps to 3D X axis)
- `y` = scroll depth / beat timing axis (maps to 3D Z axis — positive going "into the screen")
- Vertical (`VerticalState.y_offset`) = 3D Y (height) — NOT stored in Transform; derived from VerticalState at render time by `game_camera_system`

raylib `Matrix` is column-major. Translation sits at `{m12, m13, m14}`.

**Mapping:**
```
game x  →  Matrix.m12  (3D X)
game y  →  Matrix.m14  (3D Z)
vertical → computed by camera_system from VerticalState, not stored in Transform
```

---

## 3. Migration Contract

### 3.1 New `Transform` struct (replaces `Position`)

**File:** `app/components/transform.h`

```cpp
#pragma once
#include <raymath.h>

struct Transform {
    Matrix mat = MatrixIdentity();
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};
```

`Position` is removed. `Velocity` is unchanged.

---

### 3.2 Helper API

**New file:** `app/util/transform_helpers.h`

These are the ONLY functions movement and logic systems should use to mutate or query Transform. No inline matrix math outside this header or camera_system.

```cpp
#pragma once
#include <raymath.h>
#include "../components/transform.h"

namespace transform {

// Construct a Transform placed at game coords (x, scroll_z).
// Returns a translation-only matrix; scale/rotation are identity.
inline Transform make_2d(float x, float scroll_z) {
    return Transform{ MatrixTranslate(x, 0.0f, scroll_z) };
}

// Extract 2D game position from a Transform: {m12=x, m14=scroll_z}.
inline Vector2 get_pos2d(const Matrix& m) {
    return { m.m12, m.m14 };
}

// Overwrite both x and scroll_z in-place; leaves m13 (Y) and scale/rotation untouched.
inline void set_pos2d(Matrix& m, float x, float scroll_z) {
    m.m12 = x;
    m.m14 = scroll_z;
}

// Update only the scroll_z (depth) channel — used by rhythm scroll path.
inline void set_pos_z(Matrix& m, float scroll_z) {
    m.m14 = scroll_z;
}

// Apply a positional delta. Movement systems use this for non-song-sync entities.
inline void translate_2d(Matrix& m, float dx, float dz) {
    m.m12 += dx;
    m.m14 += dz;
}

} // namespace transform
```

---

### 3.3 Phase-by-Phase Implementation Sequence

#### Phase 1 — Component definition (transform.h)
1. Replace `struct Position` with `struct Transform` in `app/components/transform.h`.
2. Add `#include <raymath.h>`.
3. `Velocity` is unchanged; stays in the same file.

#### Phase 2 — Helper API
1. Create `app/util/transform_helpers.h` as above.
2. Add it to CMakeLists.txt includes (it's header-only; no new source file).

#### Phase 3 — Archetype factory migration (`app/archetypes/obstacle_archetypes.cpp`)
All `reg.emplace<Position>(entity, in.x, in.y)` calls become:
```cpp
reg.emplace<Transform>(entity, transform::make_2d(in.x, in.y).mat);
```
`create_obstacle_base` — no Position emplace there (correct, none to change).

All 7 ObstacleKind cases in `apply_obstacle_archetype` touch Position; all 7 migrate to Transform.

#### Phase 4 — scroll_system.cpp

**Rhythm sync path** (absolute position set, song-driven):
```cpp
// Before:
pos.y = constants::SPAWN_Y + (song->song_time - info.spawn_time) * song->scroll_speed;

// After:
auto& t = reg.get<Transform>(entity);
transform::set_pos_z(t.mat,
    constants::SPAWN_Y + (song->song_time - info.spawn_time) * song->scroll_speed);
```
View changes: `reg.view<ObstacleTag, Transform, BeatInfo>()`.

**dt-based path** (freeplay obstacles, particles, popups):
```cpp
// Before:
pos.x += vel.dx * dt;
pos.y += vel.dy * dt;

// After:
transform::translate_2d(t.mat, vel.dx * dt, vel.dy * dt);
```
View changes: `reg.view<Transform, Velocity>(entt::exclude<BeatInfo>)`.

#### Phase 5 — player_movement_system.cpp

View changes: `reg.view<PlayerTag, Transform, PlayerShape, Lane, VerticalState>()`.

**Lane lerp x-update:**
```cpp
// Before:
pos.x = Lerp(from_x, to_x, lane.lerp_t);
// ...
pos.x = constants::LANE_X[lane.current];

// After:
transform::set_pos2d(t.mat, Lerp(from_x, to_x, lane.lerp_t), t.mat.m14);
// ...
transform::set_pos2d(t.mat, constants::LANE_X[lane.current], t.mat.m14);
```
`pos.y` (scroll depth) is never written by player_movement_system; preserve `t.mat.m14` in all set_pos2d calls.

#### Phase 6 — game_camera_system (camera_system.cpp)

game_camera_system currently reads `Position` to feed `slab_matrix` / `shape_matrix` / `prism_matrix`. Swap to `transform::get_pos2d(t.mat)`.

**Single-slab obstacles** (view changes to include Transform):
```cpp
auto [entity, t, obs, col, dsz] = ...;
auto p = transform::get_pos2d(t.mat);
// p.x = game x, p.y = scroll_z
slab_matrix(p.x - dsz.w/2, p.y, dsz.w, ...);
```

**MeshChild parent lookup:**
```cpp
auto& parent_t = reg.get<Transform>(mc.parent);
float z = transform::get_pos2d(parent_t.mat).y + mc.z_offset;
```

**Player shape:**
```cpp
auto [entity, t, pshape, vstate, col] = ...;
auto p = transform::get_pos2d(t.mat);
float y_3d = -vstate.y_offset;  // unchanged
make_shape_matrix(shape_idx, p.x, y_3d, p.y, sz, props.radius_scale);
```

**Particle transforms:**
```cpp
auto [entity, t, pdata, col, life] = ...;
auto p = transform::get_pos2d(t.mat);
MatrixTranslate(p.x - half, 0, p.y - half);
```

The local helper functions `slab_matrix`, `shape_matrix`, `prism_matrix`, `make_shape_matrix` are **not** removed — they are correct and stay. They just receive their inputs from the Transform now instead of Position.

#### Phase 7 — collision_system.cpp

View changes: `reg.view<PlayerTag, Transform, PlayerShape, ShapeWindow, Lane, VerticalState>()` and all obstacle views include Transform.

Replace direct `pos.x` / `pos.y` reads:
```cpp
// Before:
Vector2 player_timing_point(const Position& pos, ...)

// After:
Vector2 player_timing_point(const Transform& t, ...) {
    auto p = transform::get_pos2d(t.mat);
    return {0.0f, p.y + vstate.y_offset};
}
```

`player_in_timing_window` and `player_overlaps_lane` take Transform, extract via `get_pos2d`.

`centered_rect` signature is unchanged — just feed it `p.x`, `p.y` from get_pos2d.

#### Phase 8 — ui_camera_system.cpp (ScreenPosition projection)

`ScorePopup` entities currently have `Position`. After migration:
```cpp
auto [entity, popup, t] = ...; // Transform instead of Position
auto p = transform::get_pos2d(t.mat);
Vector3 world_pos = {p.x, 5.0f, p.y};
```

---

### 3.4 What Is NOT Changing in This Migration

| Component / System | Status | Reason |
|---|---|---|
| `Velocity {dx, dy}` | **Stays** | Movement intent; systems apply it to Transform |
| `ModelTransform` | **Stays** | Render output; computed from Transform by camera_system |
| `MeshChild` offset fields | **Stays** | Rendering descriptor; parent Z read from Transform |
| `ScreenTransform` ctx singleton | **Stays** | Letterbox scale/offset; not per-entity, not a Matrix |
| `ScreenPosition` | **Stays** | Computed screen-space from world Transform |
| `ObstacleChildren` | **Stays** | Entity handle list; no position data |
| `DrawSize` | **Stays (pending §4.Q5)** | Slab dimensions fed to slab_matrix; separate from logical position |
| Camera ctx singletons | **Stays (pending §4.Q3)** | Camera-as-entity is a separate scope |
| Local matrix helpers in camera_system.cpp | **Stays** | Correct; they're not ad hoc — they're the domain computation |
| `DrawLayer` enum | **Pending §4.Q6** | Render pass tag format needs resolution |

---

## 4. Blocking Questions for the User

These must be answered before Keaton/McManus begin implementation. I do NOT have enough information to decide these unilaterally.

---

**Q1 — MeshChild: offset descriptor or own Transform?**

`MeshChild` entities (multi-slab obstacles, ghost shapes) are positioned relative to a logical parent entity. Currently `game_camera_system` reads `parent Position.y + mc.z_offset` to produce the child `ModelTransform`.

**Option A (low lift):** Keep `MeshChild` as an offset descriptor. `game_camera_system` reads parent `Transform.mat`, extracts Z, adds `mc.z_offset`. MeshChild entities do NOT own a `Transform`. This preserves the existing structural pattern with minimal change.

**Option B (full parenting):** Give MeshChild entities their own `Transform`, computed by a new system from parent `Transform + offset`. Camera_system reads MeshChild's own Transform. More extensible (detachable children, animations), more work.

*My recommendation: Option A for now. We can always promote to B if animation/detach needs arise.*

> **User decision needed:** A or B?

---

**Q2 — Rhythm scroll: `set_pos_z` vs full set_pos2d?**

The rhythm sync path does an absolute position override each frame:
```
pos.y = SPAWN_Y + (song_time - spawn_time) * scroll_speed
```
With Transform, x never changes for obstacles (only y/depth scrolls). The migration plan uses `set_pos_z` which only writes m14, leaving m12 untouched.

Is `set_pos_z` the right primitive to expose, or would you prefer the caller always use `set_pos2d` with an explicit x (read from m12 first)? The former is slightly more ergonomic; the latter makes the intent explicit.

> **User decision needed:** `set_pos_z(mat, z)` or `set_pos2d(mat, mat.m12, z)`?

---

**Q3 — Camera-as-entity: this sprint or separate?**

The user directive says: *"Camera is also an entity with a Transform, a camera component, and a render target."* Currently `GameCamera`, `UICamera`, `RenderTargets` are `reg.ctx()` singletons.

Promoting camera to an entity affects:
- `camera_system.cpp` init/shutdown
- `game_render_system.cpp` (reads camera from ctx today)
- `ui_camera_system.cpp`

This is non-trivial additional scope. Is this part of the Transform migration sprint, or a follow-up issue?

> **User decision needed:** Camera entity in scope now or separate issue?

---

**Q4 — UI/HUD entities: Transform or not?**

HUD elements (score, energy bar, popups in screen-space) have no world position — they're screen-anchored. The directive says "every entity that moves gets Transform."

HUD elements don't move in world space. Options:
- **No Transform** on HUD entities — they use screen coordinates only (e.g., a `ScreenCoord` component or raw constants).
- **Screen-space Transform** — a 2D transform matrix in screen pixels.

`ScreenPosition` already exists as a computed component for world-anchored popups. Pure HUD elements (energy bar, score text) currently have no entity at all — they're drawn inline in the render pass.

> **User decision needed:** Do screen-only HUD elements become entities with a Transform? Or do they stay as inline draw calls?

---

**Q5 — DrawSize fate**

`DrawSize {w, h}` currently drives slab dimensions in `slab_matrix(pos.x, pos.y, dsz.w, ..., dsz.h)`. After the migration, `game_camera_system` still needs this to compute slab scale in ModelTransform.

Options:
- **Keep `DrawSize`** as a rendering-hint component separate from Transform (scale is NOT baked into Transform.mat).
- **Fold scale into Transform.mat** — the matrix encodes full TRS for the game-logic unit of the entity. camera_system reads Transform.mat directly as the model matrix with scale baked.

Folding scale into Transform means `apply_obstacle_archetype` must build a full TRS matrix at spawn time (scale from DrawSize constants). The advantage: ModelTransform becomes a trivial pass-through. The disadvantage: Transform now means different things for different entities (some have scale, some don't), making it harder to extract "just the position."

*My recommendation: Keep DrawSize separate. Transform = translation only (and possibly rotation). Scale is a rendering concern.*

> **User decision needed:** Scale in Transform.mat or keep DrawSize separate?

---

**Q6 — Render pass tag format**

The directive says: *"each entity should be tagged with the render-pass number that it is part of."*

Currently `DrawLayer { Layer layer; }` with `enum class Layer : uint8_t { Background=0, Game=1, Effects=2, HUD=3 }`.

Options:
- **Enum tag (current pattern):** Keep `DrawLayer` but rename to `RenderPass`. Single component, value is the pass number. Query via `reg.view<RenderPass>()` filtered at render time.
- **Empty struct tags:** `struct RenderPassGame{};`, `struct RenderPassHUD{};` etc. Query via archetype at compile time (`reg.view<RenderPassGame>()` only touches entities in that pass). Better for DOD, slightly more components.

*My recommendation: Empty struct tags. ECS archetype filtering is exactly the tool for this — no runtime branching in the render loop.*

> **User decision needed:** Enum component or empty struct tags for render pass?

---

## 5. Files Modified by Phase

| Phase | Files |
|---|---|
| 1 | `app/components/transform.h` |
| 2 | `app/util/transform_helpers.h` (new) |
| 3 | `app/archetypes/obstacle_archetypes.cpp`, `obstacle_archetypes.h` (if needed) |
| 4 | `app/systems/scroll_system.cpp` |
| 5 | `app/systems/player_movement_system.cpp` |
| 6 | `app/systems/camera_system.cpp` |
| 7 | `app/systems/collision_system.cpp` |
| 8 | `app/systems/camera_system.cpp` (ui_camera_system) |

Any spawn/factory code that emplaces `Position` outside of `app/archetypes/` must also be audited. Keaton to grep for `emplace<Position>` before starting Phase 3.

---

## 6. Zero-Warning Invariant

All phases must compile clean under `-Wall -Wextra -Werror`. Specific risks:

- **Unused `Position` component warning**: Ensure `Position` is fully removed from all includes and views before build.
- **Implicit float conversion**: `mat.m12` / `mat.m14` are `float`; no truncation risk since Velocity/Position were already float.
- **WASM build**: `transform_helpers.h` is header-only; no platform guards needed unless `<raymath.h>` differs between WASM and native (it doesn't — same raylib).

---

## 7. Test Coverage Expectations (McManus)

- Existing collision tests must continue passing after Phase 7 (no behavioral change, only how position is stored).
- Archetype tests (`test_obstacle_archetypes`) must be updated to check `Transform` instead of `Position` component presence and initial value.
- Add a `transform_helpers` unit test confirming `make_2d`, `get_pos2d`, `set_pos2d`, `set_pos_z`, `translate_2d` round-trip correctly.
