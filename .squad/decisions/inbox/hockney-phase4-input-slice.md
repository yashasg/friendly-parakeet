# Hockney — Phase 4 Input Migration Slice (SDL2)

- **Date:** 2026-05-04T08:10:23Z
- **Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`
- **Issue:** #372
- **Owner:** Hockney (Platform Engineer)

## Decision
Ship a narrow Phase 4 starter slice that routes SDL2 event pumping through the input abstraction and enables keyboard + basic mouse behavior, while deferring touch/gesture to explicit TODO hooks.

## Why
- Unblocks gameplay/input pipeline integration on SDL2 without broad rewrites.
- Preserves existing `InputState` and dispatcher routing contracts.
- Keeps raylib backend stable as default/fallback while SDL2 input reaches functional parity for desktop controls.

## Implemented
1. Added `pump_events()` to `platform::input::InputHandler`.
2. Raylib input handler implements `pump_events()` as no-op.
3. SDL2 input handler now:
   - pumps SDL events,
   - maps keyboard edge presses (WASD/arrows/1-3/ZXC/Enter/Space),
   - maps mouse-left release + position,
   - reports window focus,
   - leaves touch/gesture APIs as TODO stubs.
4. SDL2 graphics context event pump now captures per-frame input snapshot state (key edge flags, mouse release, pointer position, focus).
5. `input_system` now calls `platform_input.pump_events()` before processing.
6. SDL2 window manager `should_close()` no longer polls events directly (event pumping is input-path owned).
7. SDL2 game loop path now initializes input singletons (`InputState`, dispatcher wiring, `input_system_init`, `ScreenTransform`) and runs input tier before render.

## Validation
- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=sdl2`
- `cmake --build build --parallel`
- `./build/shapeshifter_tests "~[bench]"`
- `./build/shapeshifter_tests "[input]"`
- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=raylib`
- `cmake --build build --parallel`
- `./build/shapeshifter_tests "~[bench]"`
- `./build/shapeshifter_tests "[input]"`

All passed.

## Follow-ups (Phase 4 completion)
- Add SDL2 touch + multitouch tracking.
- Implement gesture recognition parity.
- Add timing/latency parity checks for SDL2 vs raylib input path.
