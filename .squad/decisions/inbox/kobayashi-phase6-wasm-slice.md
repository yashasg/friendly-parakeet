# Kobayashi Phase 6 Slice — WASM + Platform Hardening

## Context
- Issue: #372 (SDL2 migration)
- Branch: `feature/sdl2-migration-phase-1-abstraction-layer`
- Requested: start Phase 6 slice without blocking on Phase 5

## Decision
Implement an incremental, shippable Phase 6 slice focused on build/config hardening:
1. Verify current CMake backend compatibility for Emscripten (`SHAPESHIFTER_BACKEND=raylib|sdl2`).
2. Verify backend-specific wasm linker plumbing:
   - raylib backend: `-sUSE_GLFW=3`
   - SDL2 backend: `-sUSE_SDL=2`
   - both retain `-sASYNCIFY` + `-sNO_EXIT_RUNTIME=1`
3. Harden CI to validate both backends where feasible:
   - Linux CI builds raylib + SDL2 native backends.
   - WASM CI builds raylib + SDL2 backends and validates link flags for both.
4. Keep raylib path intact and documented.

## Validation performed locally
- Native raylib: `./build.sh` + `./build/shapeshifter_tests "~[bench]"` ✅
- Native SDL2: `SHAPESHIFTER_BACKEND=sdl2 SHAPESHIFTER_BUILD_DIR=build-sdl2 ./build.sh` ✅
- WASM raylib (app): configure + build `shapeshifter` ✅
- WASM SDL2 (app): configure + build `shapeshifter` ✅
- Verified link flags in both wasm link commands ✅

## Known environment caveat
- Local attempt to build `shapeshifter_tests` for wasm failed on unresolved `glfwGetTime` from raylib in this host toolchain setup; Phase 6 slice keeps CI test path as-is and focuses this slice on backend build hardening.

## Outcome
Phase 6 slice started and shippable: backend selection/plumbing and CI validation are now expanded for native + wasm backend coverage.
