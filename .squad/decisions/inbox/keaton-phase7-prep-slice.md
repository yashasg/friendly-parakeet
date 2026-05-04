# Keaton — Phase 7 Preparation Slice (Issue #372)

## Decision Summary

Proceed with a **safe cleanup prep slice** before full phase 6 completion:
- remove only inactive backend scaffolding from active raylib build graph,
- refresh migration runbook/docs for dual-backend operation,
- stage a concrete deprecation checklist for eventual raylib removal,
- keep raylib fallback intact for now.

## Changes Included

### 1) Safe scaffolding cleanup (no fallback deletion)

- `CMakeLists.txt`
  - For `SHAPESHIFTER_BACKEND=raylib`, exclude SDL2-only platform implementation units:
    - `app/platform/sdl2/*.cpp`
    - `app/platform/graphics/renderer_sdl2.cpp`
    - `app/platform/window/window_manager_sdl2.cpp`
    - `app/platform/input/input_handler_sdl2.cpp`
  - For non-SDL2 backend builds, exclude SDL2-only render validation test:
    - `tests/test_renderer_sdl2_validation.cpp`

Rationale: these units are migration scaffolding for the SDL2 path and are dead in raylib artifacts; removing them from the raylib build graph lowers risk and prevents cross-backend link coupling.

### 2) Docs/runbook status refresh

- Updated `docs/ongoing_migration.md` with actual status (phase 6 in progress, phase 7 prep started).
- Added `docs/sdl2-migration-runbook.md` with exact build/test commands for both backends.
- Updated `README.md` backend section to include test commands and links to migration docs.

### 3) Raylib removal preparation artifact

- Added `docs/raylib-removal-checklist.md` capturing:
  - preconditions/sign-off gates,
  - code removal plan,
  - dependency/CI cleanup,
  - docs/ops closure tasks.

## Validation Evidence

Executed:

```bash
cmake -B build-raylib -S . -DSHAPESHIFTER_BACKEND=raylib -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build-raylib
./build-raylib/shapeshifter_tests

cmake -B build-sdl2 -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build-sdl2
./build-sdl2/shapeshifter_tests
```

Result: both backend builds and tests pass for this slice.

## Explicit Non-Goals (held)

- No raylib implementation deletion.
- No fallback-path removal.
- No final deprecation execution.

## Remaining Work Toward Final Phase 7

1. Finish phase 6 parity work and regression closure.
2. Run final dual-backend parity validation pass (including backend-targeted tests/CI).
3. Execute `docs/raylib-removal-checklist.md` in a dedicated final-removal PR after owner sign-off.
