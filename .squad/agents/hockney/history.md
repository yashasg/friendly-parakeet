# Hockney — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Platform Engineer
- **Joined:** 2026-04-26T02:07:46.546Z

## Active Focus

### Phase 4 SDL2 Input + Gesture System (2026-05-04)
- Completed SDL2 input abstraction: keyboard, mouse-left-release, touch multitouch (2 fingers), gesture recognition
- Implemented latency instrumentation hooks (`InputLatencyProbe`)
- All input/gesture/latency tests pass on both raylib and SDL2 backends
- See history-archive.md for earlier WASM/iOS investigation work

### Logging Migration (2026-05-05)
- Replaced `TraceLog` with direct SDL logging (`SDL_LogInfo/Warn/Error`)
- Removed raylib-only log dependency from gameplay/session/audio/text code
- Smoke test log-level setup migrated to SDL

### SDL/GLM Cross-Agent Team (2026-05-06)
- Collaborated on rendering/audio decoupling
- Team work: decisions merged to registry

## Learnings

- On raylib+Emscripten stack: `emscripten_set_main_loop(..., 0, 0)` can trigger immediate teardown. Use `simulate_infinite_loop=1` + prevent double-shutdown in `main()` on `__EMSCRIPTEN__`.
- GLM handedness/depth macros must be set once in CMake target graph so all targets (runtime/tests/native/wasm) compile with identical conventions and no backend drift.
- iOS officially unsupported in raylib 5.5; unmerged PR #3880 (ANGLE-based) is on-hold. DO NOT pursue.

## Recent Recommendations [PENDING OWNER APPROVAL]

### 2026-05-06T02:14:44Z — Platform Build Consistency
- Single project-wide GLM convention in CMakeLists.txt (`GLM_FORCE_RIGHT_HANDED` + `GLM_FORCE_DEPTH_ZERO_TO_ONE`)
- No workflow-level macro injection
- CI post-configure verification of macro consistency
- Coordinate with Keyser's handedness contract
- **Open questions:** canonical convention vs legacy; legacy OpenGL-only path requirements

See `.squad/decisions.md` for full recommendation + `.squad/orchestration-log/2026-05-06T02-14-44Z-hockney.md` for details.

## Archive

Earlier work (WASM fixes, iOS investigation, detailed phase history) moved to `history-archive.md`.
