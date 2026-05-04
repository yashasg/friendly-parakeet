# Keaton Phase 1 Abstraction Progress (raylib -> SDL2)

Date: 2026-05-04
Requester: yashasg
Scope: Phase 1 only (abstraction layer + compile scaffolding)

## Architectural choices applied

1. **Interface-first seams with raylib still authoritative**
   - Introduced three thin interfaces:
     - `platform::graphics::Renderer`
     - `platform::input::InputHandler`
     - `platform::window::WindowManager`
   - Added singleton accessors (`renderer()`, `input_handler()`, `window_manager()`) so existing systems can migrate call sites with minimal churn.

2. **Behavior parity via direct raylib delegation**
   - Raylib implementations are 1:1 wrappers around existing API usage patterns.
   - No gameplay/timing semantics were changed; only call path ownership changed.

3. **SDL2 compile-time scaffolding without functional cutover**
   - Added non-functional stubs:
     - `renderer_sdl2.cpp`
     - `input_handler_sdl2.cpp`
     - `window_manager_sdl2.cpp`
   - Stubs are intentionally inert and currently unselected at runtime.

4. **Highest-leverage call-site migration only**
   - `game_loop.cpp`: frame time, texture pass boundaries, frame composition draw calls, and window lifecycle moved to abstractions.
   - `input_system.cpp`: mouse/touch/keyboard/focus polling moved to input abstraction while preserving event enqueue behavior.
   - `game_render_system.cpp`: frame clear + 3D mode boundaries moved to renderer abstraction.
   - `platform_display.cpp`: native display dimensions now queried via window abstraction.

5. **Build integration strategy**
   - Updated CMake platform source glob to include:
     - `app/platform/graphics/*.cpp`
     - `app/platform/input/*.cpp`
     - `app/platform/window/*.cpp`
   - No SDL2 package linkage introduced in this phase.

## Validation

- Configure/build/tests executed successfully:
  - `cmake -B build -S . -Wno-dev`
  - `cmake --build build`
  - `./build/shapeshifter_tests`
- Outcome: `All tests passed (2176 assertions in 782 test cases)`.

## Phase 1 status (this pass)

- ✅ Thin abstraction interfaces added
- ✅ Raylib-backed implementations added
- ✅ SDL2 placeholders added for compile scaffolding
- ✅ High-leverage call sites migrated to abstraction layer
- ✅ Build/tests green, warning policy preserved

## Remaining for Phase 1 hardening (future follow-up within phase)

- Migrate additional direct raylib calls in UI/audio-adjacent paths that are still low-risk but outside this pass.
- Add explicit backend selection plumbing (compile-time option) while keeping raylib default.
- Expand abstraction usage in remaining window/input/render call sites where payoff > churn.
