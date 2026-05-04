# SDL2 Migration Runbook (Issue #372)

This runbook tracks the current dual-backend period while phase 6 continues and phase 7 prep is underway.

## Backends currently supported

- `raylib` (default)
- `sdl2` (migration target)

## Configure, build, and test both backends

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

## Optional targeted checks

```bash
# SDL2 render-validation slice
./build-sdl2/shapeshifter_tests "[render][sdl2][validation]"

# CTest mode (either build dir)
ctest --test-dir build-raylib --output-on-failure
ctest --test-dir build-sdl2 --output-on-failure
```

## Guardrails during phase 6/7

- Keep fallback paths until final deprecation gate is explicitly approved.
- No backend-specific warning regressions (`-Werror` remains enforced).
- Do not merge changes that only pass one backend.
