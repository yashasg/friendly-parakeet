# Marquez — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 EnTT rhythm bullet hell game migrating toward direct SDL2 + SDL_mixer + SDL_ttf + glm + LVGL ownership.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-05-05T17:11:16.509-07:00
- **Purpose:** Load-balance Keaton on implementation-heavy C++ rewires and compile-error cleanup.

## Seeded Knowledge

- The project should expose actual dependencies directly at the file that uses them: glm for math, SDL_Color for color, SDL2/SDL_mixer/SDL_ttf for platform/audio/text, and EnTT for game-state ownership.
- Do not add `core_types.h`, SDL include umbrellas, `runtime_compat`, `audio_backend`, `text_backend`, `alloc_api`, or `render_api` compatibility wrappers unless explicitly approved.
- Current direct-rewire pass added `app/util/window_phase.h` and `app/util/gesture.h`, rewired initial components/systems off deleted runtime surfaces, and left remaining blockers around `app/components/rendering.h`, `runtime/runtime_compat.h` callsites, raylib-style audio calls, and missing shape geometry utility ownership.
- Gameplay components should stay plain data. SDL/OpenGL/LVGL handles belong only where the owning subsystem or runtime context explicitly requires them.

## Session: rendering.h Include Purge — 2026-05-05T17:22

### Task
Purge all `#include "components/rendering.h"` references after user deleted the file. Rewire simple glm-backed types (ScreenPosition, CameraClipPlanes, ModelTransform, WorldTransform, ScreenTransform). Run build and report remaining breaks.

### Findings
- Keaton had already landed all 5 requested types in `app/components/transform.h` (ScreenPosition=glm::vec2, ScreenTransform, CameraClipPlanes, ModelTransform with glm::mat4+SDL_Color, MeshType enum). No duplication needed.
- 12 app files and 5 test files had dead `#include "components/rendering.h"` lines.
- The old `rendering.h` also defined: `DrawSize`, `Layer`, `DrawLayer`, `TagWorldPass/TagEffectsPass/TagHUDPass`, `FloorParams`, `ObstacleParts`, `MeshChild`, `ObstacleChildren`, `ObstacleModel`. None of these were in transform.h.
- `test_obstacle_model_slice.cpp` had a comment explicitly waiting for `app/components/render_tags.h` (Section B guarded by `#if 0`).

### What I Created
- **`app/components/render_tags.h`** — new file containing all trivially-plain ECS data from old rendering.h that has no GPU/render-API dependencies: `TagWorldPass`, `TagEffectsPass`, `TagHUDPass`, `DrawSize`, `Layer`, `DrawLayer`, `FloorParams`, `ObstacleParts`, `MeshChild`, `ObstacleChildren`. Uses SDL_Color+MeshType (both already in transform.h/SDL.h). NOT included: `ObstacleModel` (depends on unresolved `Model` type from GPU render API).

### Include Sites Rewired
- Removed dead rendering.h from: `camera_system.cpp`, `game_render_system.cpp`, `ui_render_system.cpp`, `collision_system.cpp`, `player_entity.cpp`, `obstacle_render_entity.cpp`, `obstacle_entity.cpp`, `popup_entity.cpp`, `game_loop.cpp`, `tests/test_helpers.h`, `tests/test_popup_display_system.cpp`, `tests/test_pr43_regression.cpp`, `tests/test_redfoot_testflight_ui.cpp`, `tests/test_obstacle_model_slice.cpp`
- Added `transform.h` where files lacked it: `game_render_system.cpp`, `game_loop.cpp`
- Added `render_tags.h` to: `camera_system.cpp`, `game_render_system.cpp`, `popup_entity.cpp`, `obstacle_entity.cpp`, `obstacle_render_entity.cpp`, `test_helpers.h`, `test_pr43_regression.cpp`, `test_redfoot_testflight_ui.cpp`, `test_obstacle_model_slice.cpp`
- Fixed `Vector2` → `glm::vec2` in `scoring_system.cpp` (trivial glm rename, pre-existing error exposed by migration)

### Remaining Broken References (needs owner direction)
See table in session output for the full breakdown. Cluster summary:
1. `Matrix`/`Model`/`Mesh`/`Material`/`ObstacleModel` in camera_system.cpp, obstacle_render_entity.cpp, tests — depend on unresolved GPU render API
2. `render_api` namespace, `RenderTargets`, `RenderTexture2D`, `Rectangle`, `WHITE` — deleted rendering/render_api.h and rendering/camera_resources.h need canonical SDL2 replacements
3. `Sound`/`Wave`/`PlaySound`/etc. in sfx_bank_system.cpp — audio backend (pre-existing Keaton blocker)
4. `util/shape_vertices.h` in bench_perspective.cpp — missing geometry utility (pre-existing)
5. `runtime/core_types.h` in test_pr43_regression.cpp — deleted compat header reference in tests

### Key Architectural Decision Made
Introduced `render_tags.h` as the home for trivially-plain ECS render-metadata components (tags, draw metadata, floor params, mesh hierarchy). These have zero GPU dependencies. `ObstacleModel` deliberately excluded — it owns a GPU resource and needs its own resolution with the render API owner.

## 2026-05-06: SDL/glm cross-agent migration team

**Orchestration Log:** .squad/orchestration-log/2026-05-06T00-38-50Z-marquez.md
**Session Log:** .squad/log/2026-05-06T00-38-50Z-direct-sdl-rewire.md

Collaborated on rendering/audio decoupling. All team work merged to decisions.md (2026-05-06).

## 2026-05-06T02:01:56Z — Model/Shape Compile Pass Complete + Architecture Review (REJECTION)

**Orchestration Log:** .squad/orchestration-log/2026-05-06T02-01-56Z-marquez.md

### Status
- ✅ Model and shape architecture pass completed
- ✅ All tests passing: 2148 assertions in 766 test cases
- ✅ Clean build via `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests`
- ⚠️  Introduced `app/util/render_types.h` and `app/systems/render_api.*`

### Architecture Review Result (Kujan)
- **REJECTION:** Kujan reviewed render surfaces and rejected `app/systems/render_api.*` + `app/util/render_types.h` as violating no-wrapper and no-compat-layer directives
- **ACTION:** Revision ownership **transferred to Keaton** (if/when resumed)
- **CONSTRAINT:** Marquez locked out from revising this artifact in the next revision cycle
- **PRIORITY:** Parked until after user tuning work completes

### User Decision (Coordinator)
- "Let's leave this as the last thing to fix, I have a few things we should tune."
- SDL2 model/mesh/material implementation deferred to end-of-cycle

### Next Steps
- Await user tuning cycle
- Keaton to revise rejected render surfaces per Kujan's architecture requirements
- Final cleanup held as lower priority
