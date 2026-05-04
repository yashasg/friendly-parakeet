# Kobayashi — History

## 2026-05-03 | Steamworks Integration Path Assessment
- **Question:** Is SDL2 materially easier for Steamworks than raylib?
- **Finding:** No. Steamworks SDK is orthogonal to graphics/input library choice.
- **Why:** Overlay hooks use OS window handles (HWND/NSWindow); achievements/stats use async callbacks; both raylib and SDL2 can satisfy these. Switching engines for Steamworks is scope creep.
- **Recommendation:** Keep raylib, create isolated `SteamworksSystem` ECS wrapper. Real work is in platform/release pipeline (SteamPipe, AppID, depot setup), not engine choice.
- **Risk (switch to SDL2):** MEDIUM-HIGH — 6-12 week rewrite, breaks iOS/web builds, gameplay regression.
- **Risk (raylib + Steamworks):** LOW — 2-4 weeks, zero impact on existing systems.
- **Output:** Decision doc at `.squad/decisions/inbox/kobayashi-steam-integration-decision.md`


## 2026-05-04 | Phase 6 Slice Start (WASM + Platform Hardening)
- Started Phase 6 while Phase 5 remains in progress (parallel slice per directive).
- Verified existing CMake wasm backend wiring (raylib=`-sUSE_GLFW=3`, SDL2=`-sUSE_SDL=2`, shared async/runtime flags retained).
- Hardened CI:
  - `ci-linux.yml` now builds native SDL2 backend in addition to default native build.
  - `ci-wasm.yml` now builds wasm SDL2 compatibility target and validates backend-specific linker flags for both wasm build trees.
- Hardened `build.sh` to support `SHAPESHIFTER_BACKEND` and `SHAPESHIFTER_BUILD_DIR` for deterministic multi-backend validation.
- Validation run locally across native raylib tests, native SDL2 build, and wasm app builds for both backends.

## 2026-05-04 | Phase 6 Completion — GitHub Runner Confirmation
- Added `workflow_dispatch` to `ci-linux.yml` and `ci-wasm.yml` to make backend validation executable on migration branches without PR choreography.
- Confirmed successful GitHub runs for both touched workflows on head `7e46a24`:
  - Linux: https://github.com/yashasg/friendly-parakeet/actions/runs/25309909229 ✅
  - WASM: https://github.com/yashasg/friendly-parakeet/actions/runs/25309910455 ✅
- Migration-coupled failures found and fixed during confirmation:
  - WASM raylib tests linking: missing backend linker flag propagation to `shapeshifter_tests` (`glfwGetTime` unresolved).
  - WASM SDL2 compatibility build: missing `content/ui` directory creation before route JSON copy in second build tree.
- Pre-existing/infra-coupled failure found and fixed:
  - Linux fresh vcpkg resolution required additional host deps for `libxcrypt` (`libltdl-dev` plus autotools chain).
- Phase 6 closure result: Linux + WASM now reliably validate raylib/SDL2 backend paths on GitHub runners.
