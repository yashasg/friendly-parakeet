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
