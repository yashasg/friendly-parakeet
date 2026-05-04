# SDL2 Phase 3 Rendering Parity Checklist

Use this checklist after each Phase 3 rendering change to validate SDL2 parity against raylib on macOS, Linux, and Windows.

## Build + smoke (both backends)

1. Configure + build raylib backend:
   - `cmake -B build-raylib -S . -DSHAPESHIFTER_BACKEND=raylib`
   - `cmake --build build-raylib`
2. Configure + build SDL2 backend:
   - `cmake -B build-sdl2 -S . -DSHAPESHIFTER_BACKEND=sdl2`
   - `cmake --build build-sdl2`
3. Run tests in each build:
   - `ctest --test-dir build-raylib --output-on-failure`
   - `ctest --test-dir build-sdl2 --output-on-failure`

## Visual parity pass (same seed/content, same difficulty)

1. Launch both binaries at the same window size.
2. Verify gameplay render path:
   - floor lane lines/rings/beat lines are visible and stable
   - obstacle shapes/bars are positioned identically by lane and timing
   - color/tint intent matches (player, obstacles, beat lines, background clear)
3. Verify UI compositing:
   - HUD text and popups stay aligned
   - pause/game-over overlays dim the scene consistently
4. Verify screen-transform behavior:
   - resize window to force letterboxing
   - confirm scene remains centered/scaled consistently without clipping
5. Verify perf sanity (non-benchmark):
   - no obvious hitching/stutter introduced by SDL2 path
   - frame pacing remains playable during dense obstacle sections

## Deterministic harness checks (SDL2 backend)

- Run render validation tests:
  - `./build-sdl2/shapeshifter_tests "[render][sdl2][validation]"`
- These tests assert:
  - render command-path counters are wired and stable
  - world/UI two-pass compositing commands are emitted (texture mode + dual blit)
  - 3D primitive command hooks used by SDL2 obstacle/particle draw paths are counted
  - deterministic frame-time override hook works for backend timing checks
