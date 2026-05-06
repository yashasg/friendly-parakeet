# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z


## 2026-05-05T17:00:55.413-07:00 — Compatibility surface cleanup pass

- Adopted direct utility location for shape-window state by adding `app/util/window_phase.h` and rewiring `app/components/player.h` + `app/components/rhythm.h` to include it.
- Removed deleted SDL umbrella include usage from platform/input/audio runtime files by including SDL2 headers directly (`<SDL.h>`, `<SDL_opengl.h>`, `<SDL_mixer.h>`).
- Added explicit gesture constants in `app/util/gesture.h` and rewired gesture/platform input code to stop depending on deleted runtime compatibility headers.
- Began replacing deleted core type surface in components: `WorldTransform` now uses `glm::vec2`; shape constants now use `SDL_Color`; text/audio context structs now reference SDL_ttf/SDL_mixer native pointer types.
- Current hard blockers after incremental builds are unresolved rendering/audio/text compatibility migrations (`app/components/rendering.h`, `runtime/runtime_compat.h` call sites, and missing `util/shape_vertices.h`) plus still-raylib-style runtime calls that need owner wiring decisions.

## 2026-05-05T17:22:53.778-07:00 — Rendering math data re-home (slice)

- Re-homed `ScreenPosition`, `ScreenTransform`, `CameraClipPlanes`, `MeshType`, and `ModelTransform` into `app/components/transform.h` with direct glm/SDL-backed storage.
- Added `screen_to_virtual(const glm::vec2&, const ScreenTransform&)` helper in `transform.h` and rewired `app/systems/input_system.cpp` to use `glm::vec2` directly.
- Kept `WorldTransform` as existing `glm::vec2` position data in `transform.h` to preserve current gameplay/system contracts.

## 2026-05-05T17:31:22.482-07:00 — Stale include purge (genuine issue surfacing)

- Replaced remaining `components/rendering.h` includes in active app/tests/bench files with `components/render_tags.h` when only draw-layer/tag symbols were needed.
- Removed deleted include paths (`rendering/camera_resources.h`, `rendering/render_api.h`, `runtime/runtime_compat.h`, `runtime/core_types.h`) from app/tests to eliminate include-not-found noise.
- Re-ran `cmake -B build -S . -Wno-dev && cmake --build build -- -k 100` and captured the now-surfaced genuine blockers: unresolved `render_api` namespace surface, missing render-resource types (`RenderTargets`, `ShapeMeshes`), and legacy raylib type usage (`Model`, `Matrix`, `Sound`, `Wave`, `Vector2`) in migrated SDL/GLM codepaths.
## 2026-05-06: SDL/glm cross-agent migration team

**Orchestration Log:** .squad/orchestration-log/2026-05-06T00-38-50Z-keaton.md
**Session Log:** .squad/log/2026-05-06T00-38-50Z-direct-sdl-rewire.md

Collaborated on rendering/audio decoupling. All team work merged to decisions.md (2026-05-06).

## 2026-05-05T17:44:00.833-07:00 — SDL render-target compositing extraction

- Moved per-frame render target pass + composite orchestration out of `game_loop.cpp` into new `frame_render_system`.
- Added plain ECS `RenderTargets` resource in `app/components/render_tags.h` using raw `SDL_Texture*` handles with explicit move-only ownership + `release()`.
- Rewired `camera::init` to allocate world/UI target textures using `SDL_CreateTexture(..., SDL_TEXTUREACCESS_TARGET, ...)` and `SDL_SetTextureBlendMode`.
- Reworked `platform_state` runtime surface to own an `SDL_Renderer*` (target-texture capable) and present through `SDL_RenderPresent`, enabling direct SDL compositing APIs.

## Learnings

### 2026-05-05T17:52:30.482-07:00

- Real blocker isolation in this branch requires filtering out include-churn first: once stale include misses are cleaned, remaining failures cluster around removed raylib-era symbols (`Model`, `Matrix`, `Mesh`, `Material`, `render_api`) and missing direct SDL/glm replacements.
- For this codebase’s SDL-only architecture direction, there is no 1:1 SDL2 replacement for raylib `Model/Mesh/Material`; the durable path is ECS-owned domain geometry + direct SDL renderer submission, with glm as the only transform math authority.

### 2026-05-05T18:01:06.882-07:00

- `MotionVelocity` now lives in `components/transform.h`; systems using particle velocity must include that header directly or unity-build order will fail.
- The camera/render transform path is now glm-native in this slice: `ModelTransform.mat` and popup projection helpers should remain `glm::mat4`/`glm::vec*` end-to-end with no `m0..m15` field assumptions.
- SDL render-target compositing should gate allocation on `SDL_RenderTargetSupported(renderer)` and preserve a direct-backbuffer fallback when target textures are unavailable.

## 2026-05-06T02:01:56Z — Marquez Compile Pass Complete + New Revision Assignment (render_api ownership)

**Orchestration Log:** .squad/orchestration-log/2026-05-06T02-01-56Z-keaton.md

### Status Update
- ✅ Marquez model/shape compile pass completed
- ✅ All tests passing: 2148 assertions in 766 test cases
- ⚠️  Marquez introduced `app/systems/render_api.*` + `app/util/render_types.h`

### Architecture Review (Kujan)
- **REJECTION:** Kujan reviewed Marquez render surfaces and rejected them as violating no-wrapper and no-compat-layer directives
- **NEW ASSIGNMENT:** Keaton now owns revision of rejected `render_api.*`/`render_types.h` surfaces
- **CONSTRAINT:** Marquez locked out from further revisions on this artifact
- **PRIORITY:** Parked until after user tuning work completes

### User Decision (Coordinator)
- "Let's leave this as the last thing to fix, I have a few things we should tune."
- SDL2 model/mesh/material implementation deferred to end-of-cycle

### Your Next Steps (when resumed)
1. Await user tuning work to complete
2. Revise rejected render surfaces per Kujan's architecture requirements
3. Ensure no-wrapper and no-compat-layer directives are honored
4. Submit revised proposal for architecture re-review before merge

### Key Context
- User has final word on design choices; architecture gates are enforceable guardrails, not blockers
- Previous learning: SDL-only path requires ECS-owned geometry, not GPU resource wrapper layers
- Coordination with Marquez: Marquez locked out; you own this revision end-to-end
