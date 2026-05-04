# SDL2 Rendering Validation Checklist

Use this checklist after SDL2 rendering changes.

## Build + smoke

1. Configure + build SDL2 backend:
   - `cmake -B build -S . -DSHAPESHIFTER_BACKEND=sdl2`
   - `cmake --build build`
2. Run tests:
   - `ctest --test-dir build --output-on-failure`

## Visual validation pass

1. Verify gameplay render path:
   - floor lane lines/rings/beat lines are visible and stable
   - obstacle shapes/bars are positioned correctly by lane and timing
   - color/tint intent matches (player, obstacles, beat lines, background clear)
2. Verify UI compositing:
   - HUD text and popups stay aligned
   - pause/game-over overlays dim the scene consistently
3. Verify screen-transform behavior:
   - resize window to force letterboxing
   - confirm scene remains centered/scaled consistently without clipping
4. Verify perf sanity:
   - no obvious hitching/stutter introduced by recent changes

## Deterministic harness checks

- `./build/shapeshifter_tests "[render][sdl2][validation]"`
