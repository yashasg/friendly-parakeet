
## 2026-05-08: Input Dead Code Cleanup (Scribe Log)

Team session: dead code elimination in input routing.
- Keaton: Deleted game_state_end_screen_routing.cpp, inlined routing helper
- Baer: Audited test/benchmark code
- Fenster: Removed duplicate GoEvent test, unused GamePhase param
- Kujan: Reviewed and approved all changes

Status: COMPLETE. All validation/build tests passed.

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

## 2026-05-08T13:15:08.642-07:00 — Raylib replacement cleanup validation

**Learning**
- For deletion-heavy raylib cleanup, the highest-signal regression check is a targeted stale-reference sweep in `app/`, `tests/`, `benchmarks/`, and `CMakeLists.txt`, followed by full build + test execution to confirm no orphaned compile/link references remain.

---

## 2026-05-08 Session: Raylib API Replacements Validation

**Task:** Validation coverage and stale reference checks.

**Findings:**
- Scoped grep against `app/`, `tests/`, `benchmarks/`, CMakeLists.txt.
- No orphaned references for HUD hex, file I/O, or floor line replacements.
- Full build + test validation: all pass (2063 assertions, 758 test cases).

**Pattern:** Established reusable stale-reference validation approach for future cleanup passes (recorded in decisions.md).

**Verdict:** All systems clear.

## 2026-05-08T13:44:53.252-07:00 — Utility cleanup validation preflight

**Learning**
- For deletion/rewire cleanups, run stale-reference sweeps first and explicitly classify findings as “cleanup not landed yet” versus “cleanup regressed” before requesting test realignment; this prevents accidental test churn against moving product ownership.

## 2026-05-08T13:57:50.741-07:00 — Systems raylib cleanup test audit

**Learning**
- System-wide raylib cleanup needs two gates: API equivalence from the cheatsheet and behavior equivalence from tests/manual validation; floor rings, haptics, global TraceLog, display sizing, collision edges, and real audio timing should not move on stale-reference checks alone.

## 2026-05-08T14:14:30.000-07:00 — Edge collision/audio cleanup validation

**Learning**
- For `CheckCollisionRecs` lane-overlap migrations, executable edge tests are mandatory: raylib uses strict rectangle comparisons, so intentional edge-inclusive gameplay needs an explicit minimal hitbox inflation plus boundary coverage.

## 2026-05-08T14:25:19.068-07:00 — Edge/audio slim cleanup validation

**Learning**
- Cleanup validation should report scoped added/deleted/net LOC separately for app and tests; here the edge/audio slice was app -23 LOC, tests +11 LOC, total -12 LOC, so behavior coverage was retained without net growth.
- Stale-symbol sweeps should be scoped to executable code paths (`app/`, `tests/`, `benchmarks/`, `CMakeLists.txt`) to avoid confusing archived squad notes with live references.

**Validation evidence**
- Scoped diff: 96 insertions, 108 deletions, net -12 across requested files.
- Live stale references for `SFXPlaybackBackend`, `sfx_playback_backend_init`, and `lane_overlaps`: zero.
- Coverage remains for exact-edge collision, just-beyond miss, audio queue drain, invalid SFX, unloaded SFX, and no-audio/headless safety.
- `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests` passed.

## 2026-05-08T14:50:05.765-07:00 — Popup partial-bundle expiry regression

**Learning**
- When splitting EnTT processing into structural views, enumerate every component-combination partition (full, partial, absent) or add explicit partial-bundle tests; otherwise entities can silently fall through and skip lifecycle updates.

## 2026-05-08T15:09 — EnTT round-2 regression coverage learning

- For EnTT wiring cleanups, compact regression tests in `test_components.cpp` can lock rewire/idempotent-disconnect behavior without touching product code, while direct `test_player_init` coverage should be moved to desktop integration paths because of hard raylib runtime dependencies in headless unit runs.
- Redfoot wiring tests that call `collision_system` must initialize `SongState`; missing ctx singletons can surface as intermittent SIGSEGV due to `reg.ctx().get<T>()` preconditions.

## 2026-05-08T15:52 — Semantic input collapse test adaptation

**Learning**
- After collapsing raw input tiers, unit tests should drive semantic `GoEvent`/`ButtonPressEvent` and execute `game_state_system` as the authoritative dispatcher drain; calling `player_input_system` directly no longer exercises event delivery.
- EnTT sink callback order is currently last-connected-first in this codepath; keep an explicit contract test so wiring-order changes are caught immediately.

## 2026-05-09T00:16:15.582-07:00 — Timing drift experiment verification

## Learnings
- Timing-drift acceptance should use two gates: freshness (new log hash + clean run) and quantization shape (non-negative, mean near frame/2, p95 near one frame).
- Verified artifact paths for this check:
  - `/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/test-player-pro-2026-05-08T23-49-24/session_pro_rt0000000345_n0001.log`
  - `/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/timing-drift-experiment-2026-05-09/session_pro_rt0000000345_n0001.log`
  - `/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/timing-drift-experiment-2026-05-09/baer-verification.md`
