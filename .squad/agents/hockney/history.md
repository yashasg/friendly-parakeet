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
