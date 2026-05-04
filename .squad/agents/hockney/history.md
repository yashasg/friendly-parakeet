# Hockney — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Platform Engineer
- **Joined:** 2026-04-26T02:07:46.546Z

## 2026-04-30 — PR #357 WASM preview abort (`emscripten_sleep`) hotfix

- **Root cause:** `shapeshifter` WebAssembly link flags did not enable async stack transformation, so runtime calls that resolve to `emscripten_sleep` aborted in browser preview (`Aborted(Please compile your program with async support...)`).
- **Fix:** Added `-sASYNCIFY` to the `if(EMSCRIPTEN)` link options for the `shapeshifter` target in `CMakeLists.txt` (web-only scope).
- **Regression guard:** Added CI step `Verify WASM async support flag` in `.github/workflows/ci-wasm.yml` that fails if `build-web/CMakeFiles/shapeshifter.dir/link.txt` does not contain `-sASYNCIFY`.
- **Validation:** `emcmake cmake -B build-web -S . ... -DVCPKG_TARGET_TRIPLET=wasm32-emscripten && cmake --build build-web -- -j$(sysctl -n hw.ncpu) && (cd build-web && ctest --verbose --output-on-failure)` passed (WASM build + `shapeshifter_tests_wasm` pass). `git diff --check -- CMakeLists.txt .github/workflows/ci-wasm.yml` clean.
- **PR status:** Changes prepared for `squad/level-designer-html-hardening`; push/check monitoring pending this hotfix commit.

## 2026-04-30 — WASM responsiveness lifecycle hardening

- **Root cause diagnosed:** Web runtime lifecycle depended on `emscripten_set_main_loop(..., simulate_infinite_loop=1)` unwind semantics while `main()` always called `game_loop_shutdown()` after `game_loop_run()`. That coupling is brittle across Emscripten/runtime modes and can tear down state at the wrong time, surfacing as an unresponsive preview.
- **Fix shipped:** `main()` now skips post-run shutdown on `__EMSCRIPTEN__`; web shutdown remains owned by `frame_callback` + `beforeunload` in `platform_display.cpp`.
- **Loop ownership hardening:** switched to `emscripten_set_main_loop(frame_callback, 0, 0)` and rely on `-sNO_EXIT_RUNTIME=1` to keep runtime alive instead of unwind control flow.
- **Regression guard:** expanded `.github/workflows/ci-wasm.yml` runtime-flag check to require both `-sASYNCIFY` and `-sNO_EXIT_RUNTIME=1` in `build-web/CMakeFiles/shapeshifter.dir/link.txt`.
- **Validation:** native build/tests pass (`./build/shapeshifter_tests "~[bench]"`), wasm configure/build/tests pass (`ctest` running `shapeshifter_tests_wasm` passes) with Emscripten toolchain + vcpkg chainload.

## 2026-04-30 08:19:42Z — PR #357 Orchestration Complete

PR #357 WASM responsiveness work finalized and logged:
- Orchestration entry: `.squad/orchestration-log/2026-04-30T08-19-42Z-hockney-pr357.md`
- Session summary: `.squad/log/2026-04-30T08-19-42Z-pr357-session.md`
- Decisions merged to `.squad/decisions.md` (lifecycle + linker enforcement + user directive)
- Inbox cleared: 5 decision entries processed

Team ready for next phase.

## Learnings

- 2026-04-30T01:30:59.881-07:00 — On this raylib+Emscripten stack, `emscripten_set_main_loop(..., 0, 0)` can trigger immediate teardown (`memory access out of bounds` / non-responsive runtime). Keep `simulate_infinite_loop=1` and prevent double-shutdown by skipping `game_loop_shutdown()` in `main()` under `__EMSCRIPTEN__`.

## 2026-04-30T08:30:59Z — Scribe Session: Decision merge + orchestration logging

Session log written: `.squad/orchestration-log/2026-04-30T08-30-59Z-hockney.md`
Decision #171 merged to registry. Team session log: `.squad/log/2026-04-30T08-30-59Z-wasm-responsiveness-fix.md`

## 2026-04-30T08:51:34Z Scribe spawned Hockney

**Manifest:** WASM desktop click fix

Merged 3 inbox decisions:
- hockney-wasm-mouse-tap-fallback.md
- hockney-wasm-pointer-release-unification.md  
- verbal-wasm-click-regression-guard.md

**Hockney Task:** Reproduce click failure → fix release-edge miss → validate native+WASM+browser smoke pass.

## 2026-05-03T10:34:36Z — Cross-Agent Note: Singleton Eager-Init WASM Follow-Up

**From:** Scribe (decision merge session)  
**For:** Hockney

Keaton completed singleton eager-init refactor (2026-05-03). Clang native build compiles warning-free; 2209 assertions / 771 cases pass. **WASM build sanity-check recommended** on next CI run — the `#ifdef PLATFORM_WEB && __EMSCRIPTEN__` branch in `input_system_init` compiles cleanly on native but was not verified on Emscripten locally. Recommendation: run web CI build-test suite to confirm no regression.

## 2026-05-03T23:13:31-07:00 — iOS Build Path Investigation

**Scope:** Determined how raylib iOS is supported when CMake PLATFORM option doesn't include iOS.

**Finding 1: raylib 5.5 does NOT have native iOS support**
- Evidence: No `rcore_ios.c` in `/Users/yashasgujjar/vcpkg/buildtrees/raylib/src/5.5-3d7ad3c5f1.clean/src/platforms/` (only `rcore_android.c`, `rcore_desktop_glfw.c`, `rcore_desktop_rgfw.c`, `rcore_desktop_sdl.c`, `rcore_drm.c`, `rcore_web.c`)
- Evidence: `ROADMAP.md` lists iOS as planned feature: `[ ] rcore: Support additional platforms: iOS, Xbox Series S|X`
- Evidence: CMakeOptions.txt enum for PLATFORM is `Desktop;Web;Android;Raspberry Pi;DRM;SDL` — **no iOS option**

**Finding 2: vcpkg portfile iOS workaround is broken**
- Location: `/Users/yashasgujjar/dev/bullethell/vcpkg-overlay/raylib/portfile.cmake` lines 60-61
- Current: `if(VCPKG_TARGET_IS_IOS) list(APPEND PLATFORM_OPTIONS -DPLATFORM=Desktop -DUSE_EXTERNAL_GLFW=OFF -DOPENGL_VERSION=ES\ 2.0)`
- Problem: Forces iOS to use `PLATFORM=Desktop` + GLFW. GLFW is windowing library for *desktop*, not iOS. This will compile but not run on iOS devices.

**Finding 3: Upstream iOS path exists but unmerged**
- **Location:** PR #3880 (https://github.com/raysan5/raylib/pull/3880) — "[rcore] Porting raylib to iOS and implement `rcore_ios.c`"
- **Author:** @blueloveTH (blueloveTH, GitHub ID 28104173)
- **Status:** Closed on 2025-07-27T19:25:40Z, **NOT merged** (merged_at=null); labeled "on hold" and "help needed - please!"
- **Implementation:** 
  - Creates `rcore_ios.c` with iOS-specific platform layer (PollInputEvents, InitPlatform, ClosePlatform)
  - Uses **ANGLE framework** (Google's OpenGL ES → Metal translator for iOS)
  - Requires prebuilt xcframeworks: `libEGL.xcframework`, `libGLESv2.xcframework` (downloadable from PR files)
  - Requires Xcode project integration (not pure CMake)
  - Demonstrated working: iPhone 8 demo video attached to PR
- **Build system:** Hybrid: CMake generates, but final linking done in Xcode + ANGLE xcframeworks (native iOS approach)

**Finding 4: iOS officially uses Xcode + native frameworks**
- Apple's standard: iOS apps built with Xcode, not CMake alone
- raylib `projects/` directory contains no iOS project (only CMake, VS2022, VSCode, CodeBlocks, etc.)
- The rcore_ios.c approach aligns with Apple's native tooling: Xcode + system frameworks + ANGLE for GL translation

**Recommendation for this repo:**
**Status quo: DO NOT pursue iOS right now.** Reasons:
1. raylib 5.5 has no iOS support; the unmerged PR is a work-in-progress (labeled "on hold")
2. iOS path requires Xcode + native framework integration, incompatible with vcpkg+CMake-only build model
3. vcpkg-managed iOS build would need: (a) custom iOS CMake cross-compile toolchain, (b) pre-built ANGLE xcframeworks in vcpkg, (c) Xcode integration layer — non-trivial
4. **Better path:** Wait for raylib 6.0 (roadmap item) or fork PR #3880, or use alternative (Unity, Unreal, SDL-based iOS wrapper)

**If iOS is required in future:**
- Option A: Adopt the @blueloveTH PR #3880 + maintain fork with ANGLE + Xcode integration (6-8 week effort for team)
- Option B: Wrap raylib in SDL or Godot (established mobile SDKs)
- Option C: Use Emscripten (Web) as iOS bridge via WebView (limited, not native)


---

## Session: 2026-05-03 — iOS Non-CMake Analysis → DECISION ARCHIVE

**Outcome:** Raylib 5.5 has no iOS support. vcpkg iOS config is non-functional (Desktop+GLFW on iOS). Upstream PR #3880 provides ANGLE-based path (unmerged, on hold). **Recommendation: DO NOT PURSUE.**

**Decision:** Merged to `.squad/decisions.md` (lines 147–266).

**Next Monitor:** Raylib 6.0 release roadmap.

## 2026-05-03T23:24:56Z — CMake-Xcode-iOS Viability Clarification

**Question from yashasg:** "CMake can call xcode build, that should still work right? I am fine with the rcore_ios and UIApplicationMain"

**Answer:** NO. CMake → Xcode invocation works, but the generated app **will NOT run on iOS**.

**Findings:**
1. **CMake CAN generate Xcode projects** — this works. Existing `ios/testflight_archive.sh` does this.
2. **raylib 5.5 has NO iOS platform code** — no `rcore_ios.c`, no `PLATFORM=iOS` CMake enum option.
3. **vcpkg-overlay forces iOS to use Desktop+GLFW** — GLFW is desktop-only; will crash/hang at runtime on iOS hardware.
4. **Upstream iOS support exists but unmerged** — PR #3880 (blueloveTH) uses ANGLE framework + Xcode integration, but PR is on-hold, not merged into 5.5.
5. **No fast path** — "Just add UIApplicationMain" is necessary but not sufficient; still need `rcore_ios.c` platform layer + ANGLE xcframeworks.

**Options:**
- Option A (6–8 weeks): Adopt PR #3880 fork + maintain iOS support in this repo
- Option B (unknown timeline): Wait for raylib 6.0 roadmap release
- Option C (short term): Use Godot/Unity wrapper if iOS is hard requirement

**Recommendation:** DO NOT pursue iOS with raylib 5.5. Status quo is correct.

**Decision:** Inbox entry written to `.squad/decisions/inbox/hockney-cmake-xcode-ios-viability.md`.
