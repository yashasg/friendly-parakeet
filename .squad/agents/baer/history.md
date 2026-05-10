
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

## 2026-05-09T00:41:48.960-07:00 — Loop 1 beatmap diagnostics validation

## Learnings
- `tools/level_designer.py` diagnostics quality can be regression-gated without touching production code by importing the generator and validating histogram shape/subdivision bins on shipped `*_analysis.json` inputs.
- A practical Loop 1 no-regression lane is: `./build/shapeshifter_tests "[shipped_beatmaps]"` + beatmap Python validators + the new Loop 1 histogram validator; keep `validate_offset_semantics.py` out of strict-gating because it currently reports large known drift violations in shipped data.

## 2026-05-09T12:47:58.529-07:00 — Onset-time spike validation gates

## Learnings
- Spike validation should target the experimental diagnostics contract (`snap_diagnostics_summary.json` + `onset_timing_events.csv`) and stay report-first by default so shipped/default beatmaps remain unaffected.
- The onset spike gate bundle now covers: min IOI floors (easy/medium/hard = 700/380/300ms), dense-cluster limits (max len 2/3/4 and short-IOI share 2%/20%/30%), residual-to-grid caps (median ≤65ms, p90 ≤120ms), event count/density (min 20/30/40, max 120/200/260 EPM), and subdivision coverage/offgrid caps (labels ≥1/2/2, offgrid ≤15%/25%/30%).
- Keep Loop 1 + Loop 2 validators as stable no-regression checks for shipped content while iterating spike-only thresholds in strict mode.

## 2026-05-09T12:55:54.558-07:00 — Onset spike level artifact validation run

## Learnings
- If no generated spike diagnostics are present yet, a safe smoke fallback is `tools/level_designer.py ... --diagnostics-only --experimental-onset-timing` into `tools/diagnostics/<spike_dir>` so shipped beatmaps are never rewritten.
- For the current smoke artifact (`tools/diagnostics/onset_spike_smoke` from `2_drama_analysis.json`), `validate_onset_spike_artifacts.py` report mode is non-blocking but strict mode correctly fails on medium/hard subdivision label coverage (`labels=1`, floor `2`).
- Existing shipped-content no-regression lane still passes in this state: `validate_loop1_diagnostics.py`, `validate_loop2_content_gates.py`, and `./build/shapeshifter_tests "[shipped_beatmaps]"`.

## 2026-05-09T13:00:47.473-07:00 — run.sh onset-level promotion validation

## Learnings
- Promotion can race in-flight: polling `content/beatmaps/*_beatmap.json` against `content/beatmaps/experimental/onset_spike/*_beatmap_onset_spike.json` confirmed transition from not-promoted to promoted within the same QA window, so wait/recheck before concluding failure.
- `./run.sh test` remains the safest noninteractive confidence lane for this branch and passed after promotion (686 test cases / 1840 assertions), confirming build+tests are stable with promoted shipped beatmaps.
- Promoted shipped beatmaps now include onset metadata at obstacle level (`timing_source=onset`, `onset_time_sec`, `subdivision_label`) while strict onset-artifact gates still fail on medium/hard label coverage; treat this as a known content-quality blocker, not a run.sh execution blocker.

## 2026-05-09T20:33:09.658-07:00 — Timing feedback visual unification validation

## Learnings
- Timing-tier popup visual unification (Perfect/Good/Ok/Bad all green and medium) requires simultaneous test expectation updates in `tests/test_popup_display_system.cpp`; otherwise `./run.sh test` fails on legacy small-font and Bad-orange assertions.
- High-signal source validation path: `app/entities/popup_entity.cpp` (tier text/font init + tier color assignment) plus `app/systems/ui_render_system.cpp` (font selection + DrawTextEx consuming `PopupDisplay`).

## 2026-05-09T20:35:29.370-07:00 — Popup test adaptation follow-through

## Learnings
- To fully lock the temporary popup visual contract, keep coverage split across both formatting and spawn paths: `popup_display_system` tests should assert Medium for Good/Ok/Bad, while `spawn_score_popup` tests should assert green for Good/Ok/Bad (not only Perfect).

## 2026-05-09T20:58:25.533-07:00 — Onset motif shipped-beatmap test realignment

## Learnings
- When shipped beatmaps are generated from the experimental onset-motif pipeline, legacy fixed distribution and min-shape-gap assertions become stale; regression tests should instead gate canonical class mapping and monotonic difficulty note counts.
- High-signal shipped-content checks for this design are: shape-gate-only easy, easy<=medium<=hard authored shape-gate counts, canonical shape↔lane pairing (triangle→0, square→1, circle→2), and medium multi-lane coverage.

## 2026-05-10T02:36:13.250-07:00 — Automated test coverage audit

**Status:** AUDIT COMPLETE (issues filed)

**Validation evidence:**
- Full native validation command passed: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests`.
- Python tool lane is not wired into CMake/run.sh/CI and currently fails manually: `python3 -m unittest tools/test_*.py` errors on stale `tools/test_get_audio_duration.py` import of removed `FFPROBE_TIMEOUT`/`get_audio_duration`.
- `tests/test_lifecycle.cpp` and `tests/test_safe_area_layout.cpp` are tracked but excluded by `CMakeLists.txt`, and their named cases are absent from `shapeshifter_tests --list-tests`.
- Shipped beatmaps currently contain onset metadata (`timing_source=onset`, broad `onset_class`, `onset_time_sec`, `subdivision_label`), but no native shipped-content test enforces those metadata invariants.

**Issues filed:**
- #397 — Re-enable tracked tests excluded from CMake discovery.
- #398 — Add shipped beatmap regression gate for onset metadata invariants.
- #399 — Wire Python tool tests into validation and fix stale audio-duration coverage.

**Learning:** Keep generator/tool Python tests as a first-class validation lane when they own content invariants; otherwise onset-layer guarantees can silently depend on tests that the standard green build never executes.

## 2026-05-10T09:49:00Z — Test coverage fixes for #397 #398 #399

## Learnings
- Re-enabling tracked lifecycle/safe-area tests required modernizing stale assertions to current `constants.h` fields; keeping the original test names/tags preserved discoverability gates while restoring build stability.
- A shipped-content metadata regression test is now necessary alongside structural beatmap tests: assert `timing_source=onset`, required onset metadata fields, broad `onset_class` values, and non-decreasing onset-backed counts across easy/medium/hard.
- Python tool validation is now wired in both CTest (`python_tool_tests`) and `run.sh test`; stale ffprobe-based coverage was replaced with duration-contract tests against `rhythm_pipeline.extract_features` so `python3 -m unittest discover -s tools -p "test_*.py"` stays green.

## 2026-05-10T02:40:52.785-07:00 — Round 2 CI/test wiring audit

## Learnings
- `python_tool_tests` is correctly registered in native CTest, but current native CI workflows still execute only `./build/shapeshifter_tests "~[bench]"`, so the Python `tools/test_*.py` suite is not part of blocking CI.
- Because native PR workflows also ignore `tools/**`, regressions in tool tests can merge without any CI signal unless a dedicated tool-test lane (or CTest-based native test step) is added.
