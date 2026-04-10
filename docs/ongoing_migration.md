# Ongoing Migration: SDL2 → raylib (RGFW backend)

> Auto-maintained during the refactor. Check this file for current status.

## Status Overview

| Phase | Description | Status | Files |
|-------|-------------|--------|-------|
| 0 | Dependency swap + build system | ✅ Done | `vcpkg.json`, `CMakeLists.txt`, `build.sh`, `vcpkg-overlay/raylib/*` |
| 1 | Main loop rewrite | ✅ Done | `app/main.cpp` |
| 2 | Input system | ✅ Done | `app/systems/input_system.cpp` |
| 3 | Render system | ✅ Done | `app/systems/render_system.cpp` |
| 4 | Text rendering | ✅ Done | `app/text_renderer.h`, `app/text_renderer.cpp` |
| 5 | System headers | ✅ Done | `app/systems/all_systems.h` |
| 6 | Component cleanup (Color → DrawColor, Gesture → SwipeGesture) | ✅ Done | `app/components/rendering.h`, `app/components/input.h`, all systems, all tests |
| 7 | Build verification | ✅ Done | 335 assertions, 188 test cases pass |

---

## Current State: **All phases complete — migration verified ✅**

### Platform backend

**RGFW** (Riley's General Framework for Windowing) — single-header, bundled inside raylib source.
- No GLFW dependency
- macOS: Cocoa + OpenGL + IOKit + CoreFoundation + CoreVideo
- Linux: X11 + OpenGL
- Windows: WinAPI + OpenGL + gdi32 + winmm

### vcpkg overlay port

The standard vcpkg raylib 5.5 port builds with GLFW. We use a **custom overlay port** (`vcpkg-overlay/raylib/`) that patches the raylib 5.5 CMake build to:
1. Add `RGFW` to the platform enum in `CMakeOptions.txt`
2. Skip GLFW in `GlfwImport.cmake` for RGFW builds
3. Add RGFW platform config to `LibraryConfigurations.cmake`

---

## Change Log

| Phase | File | Change Summary |
|-------|------|----------------|
| 0 | `vcpkg.json` | Removed `sdl2`, `sdl2-ttf`; added `raylib` |
| 0 | `CMakeLists.txt` | Replaced SDL2 find_package/link with `raylib`; added platform framework links |
| 0 | `build.sh` | Added `VCPKG_OVERLAY_PORTS` for RGFW overlay port |
| 0 | `vcpkg-overlay/raylib/*` | Custom port: patches raylib 5.5 CMake to support `PLATFORM=RGFW` |
| 1 | `app/main.cpp` | SDL_Init/Window/Renderer → InitWindow/SetTargetFPS; SDL timing → GetFrameTime |
| 2 | `app/systems/input_system.cpp` | SDL_PollEvent → IsMouseButton*/IsKeyPressed/GetTouchPosition polling |
| 3 | `app/systems/render_system.cpp` | All SDL draw calls → raylib equivalents; Color passed per-call |
| 4 | `app/text_renderer.h` | SDL_ttf types → raylib Font; removed SDL_Renderer* params |
| 4 | `app/text_renderer.cpp` | TTF_* calls → LoadFontEx/DrawTextEx/MeasureTextEx |
| 5 | `app/systems/all_systems.h` | Removed SDL_Renderer forward decl; updated render_system signature |
| 6 | `app/components/rendering.h` | `Color` → `DrawColor` (avoids raylib `Color` conflict) |
| 6 | `app/components/input.h` | `Gesture` → `SwipeGesture` (avoids raylib `Gesture` conflict) |
| 6 | *all systems + tests* | Updated all `Color`/`Gesture` usages to new names |

---

## Notes

- **Unaffected:** All ECS components (except renames), all game-logic systems, all tests
- **Tests:** 335 assertions in 188 test cases — all passing
- Reference: `docs/raylib-migration.md` for original plan details
