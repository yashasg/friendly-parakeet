# Decision: SDL2 Migration #372 — Phase 3 remaining rendering scope completed

- **Date:** 2026-05-04
- **Owner:** Keaton (C++ Performance Engineer)
- **Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`
- **Issue:** #372

## Summary
Completed the remaining Phase 3 SDL2 rendering scope: obstacle meshes, particle path, and UI compositing parity in the current architecture, while preserving raylib behavior behind backend selection.

## Changes

1. **Obstacle + particle rendering on SDL2 backend**
   - `app/systems/game_render_system.cpp`
   - Added SDL2 draw path for `ModelTransform` entities across `TagWorldPass` and `TagEffectsPass`.
   - Added SDL2 draw path for `ObstacleModel` owned geometry in world pass.
   - Uses renderer triangle primitives and mesh-backed triangle emission where mesh CPU data is present; includes procedural fallback for slab/shape/quad.

2. **UI compositing parity path for SDL2 backend**
   - `app/game_loop.cpp`
   - SDL2 frame path now executes world pass + UI pass + composite blit sequence (mirrors architecture-level pass flow).
   - SDL2 init now seeds `UICamera` and `RenderTargets` context required by the parity path.

3. **Safe SDL2 UI render-pass handling**
   - `app/systems/ui_render_system.cpp`
   - Added SDL2-safe branch to avoid raylib-only UI drawing calls in SDL2 runtime while preserving pass invocation.

4. **Parity checks updates**
   - `tests/test_renderer_sdl2_validation.cpp`
   - Expanded validation counters test to verify:
     - two-pass texture-mode boundaries,
     - dual compositing blits,
     - 3D triangle primitive counters used by obstacle/particle draw hooks.
   - `docs/sdl2-phase3-rendering-parity-checklist.md`
   - Added explicit checklist bullets for the above deterministic checks.

## Validation

### Raylib backend
- `cmake --build build-raylib --target shapeshifter_tests` ✅
- `ctest --test-dir build-raylib --output-on-failure` ⚠️ 7 known pre-existing failures (out of 791 tests):
  - multiple `test_player:*` subprocess-abort tests
  - `redfoot/#168` layout test

### SDL2 backend
- `cmake --build build-sdl2 --target shapeshifter_tests` ✅
- `ctest --test-dir build-sdl2 --output-on-failure` ⚠️ 23 known pre-existing failures (out of 791 tests):
  - existing content-path validations in this environment
  - existing `test_player:*` subprocess-abort tests
  - `redfoot/#168` layout test
- `./build-sdl2/shapeshifter_tests "[render][sdl2][validation]"` ✅
  - All tests passed (20 assertions in 2 test cases)

## Risk/compatibility
- No backend behavior switch for raylib path.
- SDL2 runtime now executes parity pass orchestration without introducing raylib-only calls in SDL2 UI pass.
- Scope intentionally limited to Phase 3 remaining items.
