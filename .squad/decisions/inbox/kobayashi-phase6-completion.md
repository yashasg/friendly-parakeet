# Kobayashi Phase 6 Completion — CI Runner Confirmation (Issue #372)

**Date:** 2026-05-04  
**Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`  
**Requested by:** yashasg  
**Phase 6 slice base:** `153d969`

## Summary

Phase 6 closure work is complete. Linux + WASM workflows touched in the Phase 6 slice have been explicitly validated on GitHub-hosted runners for this migration branch, with both raylib and SDL2 backend paths exercised.

## GitHub CI Evidence

- Linux workflow (workflow_dispatch):  
  https://github.com/yashasg/friendly-parakeet/actions/runs/25309909229 ✅
  - `Build` (raylib/default) ✅
  - `Build SDL2 backend (Linux hardening)` ✅
  - `Run tests` ✅

- WebAssembly workflow (workflow_dispatch):  
  https://github.com/yashasg/friendly-parakeet/actions/runs/25309910455 ✅
  - `Build (Emscripten)` (raylib/default) ✅
  - `Build SDL2 backend (WASM compatibility)` ✅
  - `Verify WASM runtime flags` ✅
  - `Run WASM tests (via CTest + Node)` ✅

## Failure Classification During Confirmation

### Migration-coupled failures (resolved)
1. **WASM raylib test linking (`glfwGetTime` unresolved)**  
   Root cause: backend-specific emscripten linker flag was not applied to `shapeshifter_tests`.
   - Fix: propagate `${_emscripten_backend_link_flag}` to `shapeshifter_tests` link options.

2. **WASM SDL2 compatibility build file-copy failure**  
   Root cause: second build tree (`build-web-sdl2`) lacked `content/ui` directory before root UI JSON copy.
   - Fix: add explicit `make_directory "$<TARGET_FILE_DIR:shapeshifter>/content/ui"` before copy step.

### Pre-existing/infra-coupled failure (resolved)
1. **Linux fresh-runner vcpkg install failed on `libxcrypt`**  
   Root cause: missing host package `libltdl-dev` (and exposed autotools chain expectations on cache-miss runs).
   - Fix: install required Linux host deps in CI dependency step (`autoconf`, `autoconf-archive`, `automake`, `libtool`, `pkg-config`, `libltdl-dev`).

## Workflow Reliability Change

Added minimal `workflow_dispatch` triggers to `ci-linux.yml` and `ci-wasm.yml` so backend validation can be run directly on migration branches without requiring PR event setup.

## Final Phase 6 Status

**✅ Phase 6 fully complete**

- Backend matrix behavior is explicit and validated on GitHub runners (Linux + WASM).
- CI evidence captured with successful run URLs above.
- Migration-coupled failures identified and fixed; infra-coupled dependency issue identified and fixed.
