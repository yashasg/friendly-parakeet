# Decision: SDL2 Migration #372 — Phase 3 Rendering Core Progress (Floor Path)

**Date:** 2026-05-04  
**Owner:** Keaton (C++ Performance Engineer)  
**Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`  
**Scope:** Phase 3, incremental and shippable

## Summary

Implemented the highest-priority Phase 3 rendering core port for playfield/floor/lane-guide geometry through the abstraction layer, while preserving raylib behavior and compile-time backend selection.

## What Changed

1. **Playfield floor path port** (`app/systems/game_render_system.cpp`)
   - Replaced direct `rlBegin/rlVertex3f` floor rendering with abstraction-layer primitive calls.
   - Guarded raylib-specific mesh draw path behind backend compile-time checks to keep SDL2 path safe.

2. **SDL2 Phase-3 runtime wiring** (`app/game_loop.cpp`)
   - Upgraded SDL2 bring-up loop from black-screen bootstrap to world-floor render path by seeding minimal required state (`GameState`, `GameCamera`, `FloorParams`) and calling `game_render_system` each frame.

3. **SDL2 GL context compatibility** (`app/platform/sdl2/sdl2_graphics_context.cpp`)
   - Adjusted context attributes for compatibility-friendly profile needed by this incremental render-core port.

## Validation

Executed and passed:

- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=raylib`
- `cmake --build build`
- `./build/shapeshifter_tests --reporter compact`
- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=sdl2`
- `cmake --build build`
- `./build/shapeshifter_tests --reporter compact`
- Restored default backend config and rebuilt

Result: **Build/test green on both backends; zero-warning policy preserved.**

## Phase 3 Done vs Remaining

### Done now
- Floor/playfield/lane-guide/core floor geometry path renders through abstraction on SDL2 backend.
- Compile-time backend selection remains intact.
- Raylib path remains healthy and parity-oriented.

### Remaining
- Port obstacle mesh/model rendering (`DrawMesh`-driven paths) to SDL2 backend.
- Port particle geometry rendering to SDL2 backend.
- Port SDL2 UI render/composite path beyond floor-world slice.
- Add screenshot/pixel-diff validation for floor parity against raylib baseline.

## Decision

**Accept this as Phase 3 incremental milestone (core floor/playfield path complete), continue immediately with remaining Phase 3 geometry slices.**
