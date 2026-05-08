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

### 2026-05-08T13:03:11.140-07:00: Keaton cleanup — file_logger removal + session_logger scope decision

**By:** Keaton  
**Requested by:** yashasg

**Decision**
- Deleted dead `file_logger` module (`app/util/file_logger.h/.cpp`) and CMake entries.
- Deleted legacy forwarding header `app/components/camera.h`.
- Deleted obsolete `benchmarks/bench_file_logger.cpp`.
- Deferred raylib callback migration for `session_logger` to future work.

**Rationale for deferral**
- raylib `SetTraceLogCallback` is global process state; would capture unrelated TraceLog traffic.
- `session_logger` is scoped, structured, and ECS-driven (test-player telemetry only).
- Safe migration requires custom filtering/prefixing and callback lifecycle control (revisit later if needed).

**Implementation validated**
- Removed CMake source entries; stale references eliminated from `tests/test_camera_entity_contracts.cpp`.
- No build regressions or warnings.

### 2026-05-08T13:03:11.140-07:00: Kujan review — file_logger cleanup + session_logger deferral (APPROVED)

**By:** Kujan  
**Requested by:** yashasg
**Revision owner:** Keaton

**Findings**
- High-confidence removals were complete and safe. No stale references in app/tests/bench/CMake.
- CMake wiring correct after cleanup.
- Benchmark deletion appropriate; coverage maintained by other benchmark files.
- TraceLog callback deferral justified: callback is global state, current code emits many unrelated logs.
- No warnings or regression hazards.
- Validation: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh` and `./build/shapeshifter_tests` (2063 assertions, 758 test cases).

**Verdict:** APPROVE

### 2026-05-08T13:08:46.440-07:00: Keaton implementation audit — raylib replacement candidates

**By:** Keaton  
**Requested by:** yashasg  
**Scope:** Read-only audit of `app/` for homegrown implementations replaceable with direct raylib APIs.

**High-confidence replacements (replace-with-raylib)**
1. HUD hexagon path: replace manual trig + 6 `DrawTriangle` with `DrawPoly({cx, cy}, 6, radius, -90.0f, color)`.
2. Beatmap file loading: replace `std::ifstream` with `LoadFileText()` / `UnloadFileText()`.

**Medium-confidence needs design decision**
1. Floor geometry (`draw_floor_lines`/`draw_floor_rings`): immediate-mode `rlBegin`/`rlVertex3f` → possible `DrawLine3D`/`DrawTriangle3D`/textured-floor.
2. Input/display: `screen_to_virtual()` letterbox mapping and `Camera2D` alternatives.
3. Persistence: `settings_persistence.cpp` + `high_score_persistence.cpp` file I/O.

**Keep (do not replace)**
- Obstacle model assembly, session logger, platform display glue, procedural SFX, font loading, song playback logic, collision/timing math, haptics bridge.

**Proposed cleanup order**
1. Replace HUD hexagon with `DrawPoly`.
2. Replace beatmap file text ingestion with `LoadFileText`.
3. Decide floor rendering target architecture before touching rlgl floor primitives.
### 2026-05-08T13:15:08.642-07:00: Raylib cleanup stale-reference validation pattern

**By:** Baer  
**Requested by:** yashasg

**Pattern**
- When removing raylib-replacement leftovers (helpers/includes/files), run a scoped stale-reference grep against only `app/`, `tests/`, `benchmarks/`, and `CMakeLists.txt` for deleted symbols/paths, then run full build + test validation.

**Why it matters**
- Most regressions in cleanup passes are orphaned references in build wiring or test/benchmark includes, not runtime behavior changes.
- This catches dead-surface linkage failures quickly without widening noise to docs or archive files.

### 2026-05-08T13:15:08.642-07:00: Safe raylib API replacements for HUD/file I/O/floor lines

**By:** Keaton  
**Requested by:** yashasg

**Decision**
- Replace HUD hexagon fan triangulation with `DrawPoly(..., 6, radius, -90.0f, ...)`.
- Replace beatmap/constants full-file ingestion from `std::ifstream` iterators with `LoadFileText`/`UnloadFileText`.
- Replace floor lane/grid/beat `RL_LINES` emission with `DrawLine3D`, but keep floor ring annulus geometry on existing `RL_TRIANGLES` path (design-gated).

**Why**
- These substitutions reduce custom rendering/file-loading code while preserving behavior and diagnostics.
- They stay inside the approved cleanup surface and avoid touching design-gated rendering architecture.

### 2026-05-08T13:15:08.642-07:00: Kujan review: safe raylib API replacements (Keaton)

**Requested by:** yashasg  
**Revision owner:** Keaton  
**Verdict:** APPROVE

**Findings**
- Scope matches the approved replacement set and does not cross into design-gated floor annulus architecture changes.
- HUD hexagon fill replacement is correct and warning-safe: `DrawPoly(..., 6, radius, -90.0f, ...)`.
- Beatmap/constants loading correctly uses `LoadFileText`/`UnloadFileText`; constants parse failures now emit useful `TraceLog(LOG_WARNING, ...)` details without suppressing fallback behavior.
- Floor lane/grid/beat-line emission uses `DrawLine3D`; annulus ring geometry remains on the existing `RL_TRIANGLES` path (untouched by this review scope).
- No stale app/tests/bench/CMake references were found for removed helper surfaces relevant to this change set.

**Validation**
- Reproduced: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh`
- Reproduced: `./build/shapeshifter_tests`
- Result: all tests passed (2063 assertions in 758 test cases).
