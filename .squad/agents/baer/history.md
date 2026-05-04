# Baer — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Test Engineer
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings
- For browserless editor UI checks, gate regressions by asserting static HTML/CSS/JS contracts (element IDs, dismiss wiring, guidance text) in Node tests, then reserve interactive behavior for browser-capable runs.
- When cross-agent UI changes are pending, perform a timed re-check before validation and explicitly report “not visible yet” rather than inferring intent.

## 2026-04-30 — Level editor help UI final validation

**Status:** VALIDATED (source-level + node tests)

**Verification result:**
- Help entrypoint exists and is visible: `#btn-help` in toolbar.
- Help modal exists with static authored content: `#help-modal` + `#btn-help-close` + guidance sections for loading audio/levels, playback/seek, placing/selecting/moving obstacles, editing properties, validation/export, and shortcuts.
- Dismiss behavior is wired for close button, backdrop click, and Escape key (`bindModal` + Escape handler in `main.js`).
- Accessibility semantics present: trigger `aria-haspopup="dialog"`/`aria-controls`, modal `role="dialog"`, `aria-modal`, labelled title, and `aria-hidden` toggled in JS.
- Rendering path is safe/static for help copy (no dynamic HTML injection; text is authored in `index.html`, modal state toggled via classes/attributes).

**Validation evidence:**
- `node --check tools/beatmap-editor/js/main.js` ✅
- `node --test tools/beatmap-editor/test/*.test.js` ✅ (22/22 passing)
- `git --no-pager diff --check` ✅

## Learnings
- The browserless help-UI validation pattern is now confirmed end-to-end: static contract checks plus Node tests are effective for modal presence, wiring, and safety assertions.
- Limitation: without browser automation, we still cannot prove focus trapping, screen-reader announcement timing, or computed visual layering beyond source/test contracts.

## 2026-04-30 — PR #357 WASM preview runtime repro

**Status:** REPRODUCED (runtime boot failure)

**Target:**
- Preview URL: `https://yashasg.github.io/friendly-parakeet/squad-level-designer-html-hardening/`

**Repro method:**
- Headless Chromium via Playwright from `tools/beatmap-editor` (not curl/static-only).
- Captured browser console logs, `pageerror`, failed requests, and DOM/canvas state after load.
- Repeated load 3x to test determinism.

**Observed failure:**
- Unhandled runtime exception on boot:
  - `RuntimeError: Aborted(Please compile your program with async support in order to use asynchronous operations like emscripten_sleep).`
- Deterministic repro across repeated loads:
  - `run1: ABORTED_ASYNC_SUPPORT`
  - `run2: ABORTED_ASYNC_SUPPORT`
  - `run3: ABORTED_ASYNC_SUPPORT`

**Observed page state:**
- Document reaches `readyState=complete`.
- Canvas exists and is visible (`#canvas`, 1280x720).
- No network request failures captured for preview assets.
- Runtime abort is JS-side after startup logs, indicating “artifacts present” but boot not healthy.

## Learnings
- Static artifact existence + MIME checks are insufficient for WASM acceptance; PR preview validation needs an actual browser runtime boot assertion and a fail-on-pageerror gate.

## 2026-04-30 — WASM responsiveness regression hardening

**Status:** IMPROVED (CI smoke gate added)

**What changed:**
- Added `tests/wasm_runtime_smoke.cjs`: a Playwright-core smoke runner that fails on:
  - runtime abort signatures in console (`RuntimeError: Aborted`, `Aborted(`, `emscripten_sleep`),
  - browser `pageerror`,
  - failed `.js/.wasm/.data` requests.
- The smoke also requires:
  - `#canvas` visible,
  - `#loader` hidden,
  - title containing `SHAPESHIFTER` within timeout.
- Wired into `.github/workflows/ci-wasm.yml` as `WASM browser responsiveness smoke`, served from local `build-web` via `python3 -m http.server`.

**Validation result:**
- Native regression suite still green: `ctest --test-dir build --output-on-failure` → `782/782` passing.
- Smoke script syntax + runtime probe passed against active preview URL when launched with Playwright Chromium executable path.

## Learnings
- WASM runtime liveness needs a browser event-loop assertion (canvas shown + loader gone + no pageerror), not only linker-flag checks.
- For CI portability, `playwright-core` + detected system Chromium avoids bundled browser installs while still providing real runtime coverage.

## 2026-04-30 08:19:42Z — PR #357 Orchestration Complete

PR #357 WASM regression coverage work finalized and logged:
- Orchestration entry: `.squad/orchestration-log/2026-04-30T08-19-42Z-baer-pr357.md`
- Session summary: `.squad/log/2026-04-30T08-19-42Z-pr357-session.md`
- Decisions merged to `.squad/decisions.md` (smoke gate decision documented)
- Inbox cleared: 5 decision entries processed

Team ready for next phase.

## 2026-05-04 — Phase 3 rendering parity validation prep (SDL2)

**Status:** PREPARED (tests + hooks + checklist)

**What was added:**
- Deterministic SDL2 renderer validation hooks:
  - command-path counters (begin/end drawing, texture mode, clear, 3D mode, draw texture, swap)
  - frame-time override hook for stable timing assertions
- New non-snapshot test coverage:
  - `tests/test_renderer_sdl2_validation.cpp`
  - validates command-path counter wiring and deterministic frame-time override behavior
- Manual cross-platform parity checklist:
  - `docs/sdl2-phase3-rendering-parity-checklist.md`
  - includes raylib vs SDL2 build/test flow plus visual parity checks for macOS/Linux/Windows

**Validation intent:**
- Give Keaton immediate, low-flake parity guardrails for Phase 3 render-path work without brittle image snapshot debt.

## 2026-05-04 — Final migration gate parity matrix + pre-existing failure ledger

**Status:** COMPLETE (docs + validation evidence)

**What changed:**
- Updated `docs/sdl2-migration-runbook.md` with:
  - final backend/platform/category parity matrix,
  - baseline pre-existing failure ledger with reproducible commands,
  - repeatable final migration acceptance command set.
- Added decision artifact:
  - `.squad/decisions/inbox/baer-final-parity-matrix.md`

**Validation evidence (local):**
- `./build-raylib/shapeshifter_tests --skip-benchmarks -v quiet` ✅
- `./build-sdl2/shapeshifter_tests --skip-benchmarks -v quiet` ✅
- `ctest --test-dir build-raylib --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ❗ expected pre-existing fail
- `ctest --test-dir build-sdl2 --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ❗ expected pre-existing fail
- `ctest --test-dir build-sdl2 --output-on-failure` ❗ known pre-existing Not Run cascade (`Unable to find executable .../build-sdl2/shapeshifter_tests`)

## 2026-05-04 — Final migration acceptance gate execution (Issue #372)

**Status:** PASS (no new migration regressions)

**Scope executed (current branch reality):**
- `cmake -B build -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev`
- `cmake --build build --target shapeshifter_tests`
- `./build/shapeshifter_tests --skip-benchmarks -v quiet` (backend-relevant path: full regression)
- `./build/shapeshifter_tests "[render][sdl2][validation]" -v quiet` (backend-relevant path: deterministic render validation)
- `ctest --test-dir build --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` (baseline sentinel)

**Classification vs baseline ledger:**
- Core regression suite: ✅ pass
- SDL2 render validation slice: ✅ pass
- Sentinel `redfoot/#168`: ⚠️ fails as expected (pre-existing baseline)

**Gate verdict:**
- ✅ PASS for migration acceptance gate on this branch state.
- Residual non-gating note unchanged: direct `raylib` dependency eviction outside backend dispatch remains a follow-up slice.
