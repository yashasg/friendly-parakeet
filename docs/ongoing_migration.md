# Ongoing Migration: raylib ↔ SDL2 backend abstraction (Issue #372)

> Branch: `feature/sdl2-migration-phase-1-abstraction-layer`

## Status Overview

| Phase | Description | Status | Notes |
|---|---|---|---|
| 1 | Abstraction seams (renderer/input/window) | ✅ Done | Core interfaces + raylib impls landed |
| 2 | SDL2 native window + GL context bring-up | ✅ Done | SDL2 startup/shutdown path wired |
| 3 | Rendering parity core | ✅ Done | SDL2 validation counters + parity checklist in place |
| 4 | Input parity | ✅ Done | SDL2 touch tracker + input bridge landed |
| 5 | Audio/timing abstraction slice | ✅ In progress | Slice started at `47eebc4` |
| 6 | Active migration implementation | 🚧 In progress | Keep both backends behavior-safe |
| 7 | Cleanup + deprecation prep | 🚧 Prep started | This slice removes safe dead scaffolding and adds final-removal checklists |

## Current backend policy

- **Do not remove raylib yet.**
- Both `raylib` and `sdl2` backend builds must stay green.
- Backend switches are compile-time via `-DSHAPESHIFTER_BACKEND=<raylib|sdl2>`.

## Build + test runbook (native)

```bash
# raylib backend
cmake -B build-raylib -S . -DSHAPESHIFTER_BACKEND=raylib -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build-raylib
./build-raylib/shapeshifter_tests

# SDL2 backend
cmake -B build-sdl2 -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build-sdl2
./build-sdl2/shapeshifter_tests
```

For backend-specific commands and release criteria, see:
- `docs/sdl2-migration-runbook.md`
- `docs/raylib-removal-checklist.md`
