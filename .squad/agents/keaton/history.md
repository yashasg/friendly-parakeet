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
