# Keaton — Phase 2 SDL2 Window Progress

Date: 2026-05-04
Issue: #372
Branch: `feature/sdl2-migration-phase-1-abstraction-layer`

## Decision

Implement Phase 2 as a **native SDL2 bring-up path behind compile-time backend selection**, while preserving raylib as default runtime backend.

## Why

- Satisfies phased migration requirement without destabilizing current gameplay path.
- Enables incremental validation of SDL2 lifecycle/event loop/GL context before rendering-system port.
- Maintains existing test coverage and warning policy under default backend.

## What changed

1. Added `SHAPESHIFTER_BACKEND` CMake option (`raylib` default, `sdl2` optional) and compile defs.
2. Added native SDL2 dependency wiring in CMake + `vcpkg.json`.
3. Added SDL2 platform module (`app/platform/sdl2/`) for:
   - SDL init/shutdown
   - Window + GL context creation
   - Event pumping and close handling
   - Frame-time source and swap-buffer path
4. Upgraded abstraction selectors to dispatch to SDL2 implementations when enabled.
5. Added SDL2 bring-up frame path in `game_loop.cpp` (blank-screen render loop + clean close).

## Validation

- `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests` (raylib default) ✅
- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=sdl2 && cmake --build build && ./build/shapeshifter_tests` ✅
- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=raylib` (restore default config) ✅

## Follow-up

- Next phases should port input + rendering systems off raylib runtime assumptions and replace SDL2 input stub behavior.
