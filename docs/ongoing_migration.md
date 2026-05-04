# SDL2 Migration Status (Issue #372)

> Branch: `feature/sdl2-migration-phase-1-abstraction-layer`

## Status Overview

| Phase | Description | Status |
|---|---|---|
| 1 | Abstraction seams (renderer/input/window) | ✅ Done |
| 2 | SDL2 native window + GL context bring-up | ✅ Done |
| 3 | Rendering parity core | ✅ Done |
| 4 | Input parity | ✅ Done |
| 5 | Audio/timing abstraction slice | ✅ Done |
| 6 | Active migration implementation + CI proof | ✅ Done |
| 7 | Deprecation/removal execution | ✅ Done (SDL2 sole backend path, external raylib dependency removed) |

## Backend policy

- SDL2 is the only supported backend.
- `SHAPESHIFTER_BACKEND=sdl2` is the only accepted backend selection.
- Runtime/build graph no longer links or installs raylib.

## Build + test (native)

```bash
cmake -B build -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build --target shapeshifter_tests
./build/shapeshifter_tests --skip-benchmarks -v quiet
./build/shapeshifter_tests "[render][sdl2][validation]" -v quiet
```

For detailed parity/validation commands and closure notes, see:
- `docs/sdl2-migration-runbook.md`
- `docs/raylib-removal-checklist.md`
