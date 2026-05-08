### 2026-05-08T11:47:09.246-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Floor rings should be drawn as 2D on the floor quad/texture in XY space, then the floor texture/quad should be rotated into the XZ floor plane; do not assume the ring must remain hand-emitted world-space geometry.
**Why:** User correction — captured for team memory
### 2026-05-08T11:47:09.246-07:00: Keaton re-evaluation — floor ring texture architecture

**Context**
- User clarified target architecture: draw floor ring in 2D (XY) on a floor texture/quad, then orient that textured quad onto the world floor (XZ).

**Decision Input**
- Under this architecture, `app/util/shape_vertices.h` can be deleted entirely after rewiring runtime ring rendering away from `shape_verts::CIRCLE` and updating tests/benchmarks that currently include it.
- Preferred raylib path to evaluate: `LoadRenderTexture` + `BeginTextureMode` + `ClearBackground` + `DrawRing`/`DrawCircleLines` (2D pass), then sample that texture on a world-floor mesh (`DrawMesh`/material diffuse map or `DrawPlane` with texture-capable shader/material path).

**Key Constraints**
- Current mesh shader (`app/systems/camera_system.cpp`) does not sample textures; it shades `colDiffuse` only, so a texture floor needs shader/material changes or a separate floor draw path.
- Current `RenderTargets` RAII (`world`, `ui`) will likely need an additional owned floor texture/render target resource and lifecycle coverage.
- RenderTexture Y-flip and UV mapping must be validated to avoid upside-down/mirrored floor rings.

**Likely rewiring scope**
- `app/systems/game_render_system.cpp` (replace immediate-mode annulus with textured-floor draw path)
- `app/systems/camera_system.cpp` + `app/rendering/camera_resources.h` (resource creation/ownership for floor texture and possibly floor material/shader)
- `tests/test_perspective.cpp`, `benchmarks/bench_perspective.cpp` (remove `shape_verts::*` dependencies)
- Optional: `tests/test_gpu_resource_lifecycle.cpp` (new RAII type-trait/idempotent-release checks for floor texture resource)

### 2026-05-08T11:53:19.588-07:00: **COMPLETED** shape_vertices.h removal

**Decision:** Floor ring geometry now generates circle points locally in `app/systems/game_render_system.cpp` using trig per segment. `app/util/shape_vertices.h` is deleted.

**Rationale:** Floor rings are a floor-rendering concern (2D ring logic on XZ plane), not shared reusable shape data. Removes stale custom vertex-table maintenance.

**Implementation:** 
- Rewired `draw_floor_rings()` off `shape_verts::CIRCLE` to local trig-based generation.
- Deleted `app/util/shape_vertices.h` and all references.
- Updated `tests/test_perspective.cpp` and `benchmarks/bench_perspective.cpp` to remove shape_vertices dependency.
- Verified: `./build.sh` + `./build/shapeshifter_tests` all pass (APPROVED by Kujan).

**Scope owned by:** Keaton (implementation) + Kujan (review, APPROVED).

### 2026-05-08T11:59:39.840-07:00: Floor render system split

**By:** Keaton  
**Requested by:** yashasg

**Decision**
- Move floor-only rendering helpers and draw orchestration from `app/systems/game_render_system.cpp` into `app/systems/floor_render_system.cpp`.
- Keep `game_render_system.cpp` responsible for pass flow (camera setup, floor pass call, world/effects meshes) while floor geometry/details live in the dedicated system.

**Why**
- Reduces file complexity and keeps floor rendering concerns isolated.
- Preserves ECS/raylib flow and behavior with a narrow interface: `floor_render_system(const entt::registry&)`.

**Implementation Notes**
- Added `app/systems/floor_render_system.cpp` and `app/systems/floor_render_system.h`.
- `game_render_system.cpp` now calls `floor_render_system(reg)` and no longer contains floor helper code.
- Added declaration in `app/systems/all_systems.h`.

### 2026-05-08T11:59:39.840-07:00: Kujan review — floor render split (approved)

**By:** Kujan  
**Requested by:** yashasg

**Decision**
- Approve Keaton's floor render split.

**Review outcome**
- `game_render_system.cpp` is materially cleaner and remains coherent as the world-pass orchestrator.
- Floor-specific drawing logic is isolated in `app/systems/floor_render_system.cpp` with a narrow ECS-facing entrypoint declared in headers.
- Build wiring is intact via existing systems glob; no CMake drift detected.
- No stale `shape_vertices` references remain in `app/`, `tests/`, or `benchmarks/`.
- Validation reproduced successfully: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh` and `./build/shapeshifter_tests`.

**Non-blocking note**
- Unrelated `.squad` churn (health reports/history updates) is present in working tree; keep product commit scope focused.
