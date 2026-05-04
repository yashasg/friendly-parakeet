# Verbal — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** QA Engineer
- **Joined:** 2026-04-26T02:07:46.547Z

## Learnings

### 2026-05-04T10:56:32Z — QA audit: validator/test parity and brittle contract checks

- **Cross-surface policy parity matters:** when a rule exists in both C++ regression tests and Python validators, drift appears fast and causes contradictory release signals (example surfaced: medium max-gap threshold mismatch and bar-policy conflict between `tests/test_shipped_beatmap_*` and `tools/validate_*.py`).
- **Brittle source-text assertions are high-maintenance:** tests that grep `.cpp` source strings (`test_pr43_regression.cpp`) are easy to break by refactors/formatting while missing semantic regressions; behavior-level assertions tied to ECS outputs are more durable.
- **Helper duplication is an ECS drift vector:** replicated entity construction in test helpers (`tests/test_helpers.h`) diverges from canonical factories (`app/entities/*_entity.cpp`) unless helpers delegate directly to production factories.

### 2026-04-30 — Assets root removal QA audit

- **Validation sweep pattern:** For root-folder migrations, run both textual sweeps and path sweeps: `rg "assets/|\bassets\b"` across code/docs/workflows plus `find . -type d -name '*assets*'`. Text-only sweeps miss directory-state regressions; path-only sweeps miss stale string literals.
- **Required post-change checks:** In this repo, use `cmake --build build -- -j4`, `./build/shapeshifter_tests "~[bench]"`, `node --check tools/beatmap-editor/js/*.js`, and `node --test tools/beatmap-editor/test/*.test.js` to validate migration safety across runtime + editor surfaces.
- **Key migration paths:** Runtime/bundle/font loading now resolves from `content/` only (`CMakeLists.txt`, `app/ui/text_renderer.cpp`, `content/fonts/`). `assets/` should not exist as a top-level game data root after commit.

**Task:** Read-only audit of test coverage for input/UI system consolidation. User directive: `hit_test_system`, `input_dispatcher`, `input_gesture`, `input_system` are over-fragmented; `level_select_system` is not a real system (no per-frame logic, just UI event handlers).

**Files audited:**
- `app/systems/hit_test_system.cpp` — dispatcher listener (`hit_test_handle_input`), not a per-frame system
- `app/systems/input_dispatcher.cpp` — setup/teardown only (`wire_input_dispatcher`/`unwire_input_dispatcher`)
- `app/systems/input_gesture.h` — single inline pure function `classify_touch_release`, header-only
- `app/systems/input_system.cpp` — real per-frame system; polls raylib mouse/touch/keyboard; calls `classify_touch_release`
- `app/systems/level_select_system.cpp` — two event handlers (`level_select_handle_go`, `level_select_handle_press`) + a thin `level_select_system(reg, dt)` that just calls `disp.update<>()` then runs confirmed→Playing transition

**Test files:**
| File | Tag | Cases | What it actually tests |
|---|---|---|---|
| `test_input_system.cpp` | `[input_gesture]` | 9 | `classify_touch_release` only — MISNAMED |
| `test_hit_test_system.cpp` | `[hit_test]` | 11 | `hit_test_handle_input` + ActiveTag lifecycle |
| `test_input_pipeline_behavior.cpp` | `[input_pipeline]` | 10 | End-to-end: push_input → player state |
| `test_level_select_system.cpp` | `[level_select]` | 26 | Both event handlers + level_select_system |

**Key findings:**
- `test_input_system.cpp` is a misnomer: it tests gesture classification, not `input_system()`. Should be renamed `test_input_gesture.cpp`.
- `input_system()` (the per-frame raylib poller) has zero direct unit tests. This is expected (raylib hardware can't be mocked), but the keyboard path (WASD → GoEvent, 1/2/3 → ButtonPressEvent) is an undocumented gap.
- `wire_input_dispatcher` is only tested implicitly via `make_registry()` calling it. No standalone test for listener registration order, which is load-bearing (comment says "Registration order = processing order").
- All test behavior contracts are stable and reusable across any renaming.

**Rename candidates:** See `verbal-input-ui-test-map.md` in decisions/inbox.

### 2026-04-30 — Diagram integrity check

- When reviewing tree diagrams in specs, validate parent-child indentation as strictly as code structure; duplicated parent nodes can silently change deployment assumptions.

## Session: Assets Root Removal (2026-04-30)

Part 1 (Audit): Audited all `assets/` references across runtime/build/workflows/editor/tooling/tests/docs. Confirmed only Apple-specific (`Assets.xcassets`) and historical `.squad/` logs contain `assets` term. Identified stale LanePush test contract blocker.

Part 2 (Doc Fix): Revised `docs/asset-bundle-spec.md` tree diagram per Kujan's feedback (fixed duplicate `content/` sibling nodes). Single `content/` root node with `beatmaps/`, `audio/`, `fonts/` children. Approved by Kujan.

**Manifested:** Decisions #173, #175 merged to `.squad/decisions.md`

### 2026-04-30T01:30:59.881-07:00 — WASM responsiveness validation

- **Root QA gap found:** The browser smoke test only validated asset load (loader hides) but did not assert post-load interactivity, so input regressions could slip through while CI stayed green.
- **Playwright pitfall:** `page.waitForFunction(fn, {timeout})` may treat the second argument as page-function arg; use `page.waitForFunction(fn, undefined, {timeout})` for deterministic timeout behavior.
- **Regression guard added:** Browser smoke now compares pre/post-input canvas screenshots (safe click at 200x200 + Enter) and fails with `no-visual-response-after-input` when the build loads but does not react.

## 2026-04-30T08:30:59Z — Scribe Session: Decision merge + orchestration logging

Session log written: `.squad/orchestration-log/2026-04-30T08-30-59Z-verbal.md`
Decision #170 merged to registry. Team session log: `.squad/log/2026-04-30T08-30-59Z-wasm-responsiveness-fix.md`

### 2026-04-30T03:05:46.543-07:00 — Low/High bar white-wall QA check

- The full-lane white wall symptom maps to Model-authority bars when `draw_owned_models` does not preserve ECS tint; if diffuse color is not overridden, `LoadMaterialDefault()` yields a white slab across all lanes.
- Add a source-gate regression in `tests/test_pr43_regression.cpp` that asserts `game_render_system.cpp` still queries `Color` in the owned-model path and applies `MATERIAL_MAP_DIFFUSE` tint.
- For manual repro targeting hard difficulty, use the first bar beats: stomper beat 182 (low_bar), drama beat 111 (high_bar), mental_corruption beat 113 (high_bar).

## Session: White Lane Wall Fix Validation (2026-04-30T10:05:46Z)

**Role:** Independent validation and regression coverage

**Task:** Independently reproduce and validate fix/regression coverage

**Work Summary:**
- Reproduced white wall visual in hard mode with LowBar and HighBar obstacles
- Validated McManus fix by verifying correct tint application
- Added regression guard in test suite to ensure `ObstacleModel` color tint is applied
- Verified all test suites pass with fix in place

**Outcome:** Fix validated; regression coverage added to test_pr43_regression.cpp; all suites passed

**Status:** Complete

**Artifacts:**
- Decision: `.squad/decisions.md` (white lane wall fix section)
- Orchestration: `.squad/orchestration-log/2026-04-30T10-05-46Z-verbal.md`
- Session Log: `.squad/log/2026-04-30T10-05-46Z-white-lane-wall-fix.md`

### 2026-04-30T03:24:36.429-07:00 — Low/High bar disablement QA verification

- **Gameplay-vs-content contract:** For temporary mechanic shutdowns, regressions should assert runtime gameplay behavior (`load_beat_map` output and scheduler spawning), not raw authored JSON. Source content may intentionally retain disabled kinds for later re-enable.
- **Safety pattern for temporary disables:** Pair a content-load guard with a runtime scheduler guard. In this repo, `difficulty_ramp` now verifies medium/hard loaded charts are bar-free, and `beat_scheduler` verifies bar entries are skipped even if injected manually.
- **Progression check while disabled:** Always assert remaining non-disabled obstacle content is still present after filtering; a pure “forbidden kind absent” check can hide accidental empty charts.

### 2026-05-03T21:55:12.617-07:00 — Issue #350 acceptance-criteria gate

- **Scope audited:** `app/util/enum_names.h`, `app/audio/audio_types.h`, `app/audio/sfx_bank.cpp`, and `tests/test_audio_system.cpp`, plus issue #350 comments.
- **Parity evidence present in code:** enum count behavior is compiler-guarded (`magic_enum::enum_count` static_asserts in audio types, SFX specs, and audio tests); invalid enum handling is guarded (`ToString()` fallback `"???"`, SFX bank bounds checks); `ToString()` remains `const char*`.
- **Blocking gap:** Issue #350 has no substantive recommendation comment from Keaton, so AC requiring a written recommendation posted on the issue is not satisfied.
- **Reviewer action:** Rejected for now and requested revision by a different agent (Fenster) per reviewer lockout protocol, with explicit parity callouts and final keep/replace recommendation posted on #350.

### 2026-05-04 — Issue #350 QA gate clear

- **Re-review outcome:** AC is now satisfied after recommendation comment was posted on-issue (`#issuecomment-4368370941`) with explicit parity coverage and keep-vs-replace conclusion.
- **Gate discipline reminder:** For research issues, “code already compliant” is not sufficient; AC remains blocked until the required narrative recommendation is present on the issue thread itself.

### 2026-05-04T04:55:12Z — Decision Registry: Issue #350 QA gate decision merged

- **Scribe action:** Merged inbox decision file to `.squad/decisions.md` (`# Decision: Issue #350 QA Gate Cleared`)
- **Orchestration log written:** `.squad/orchestration-log/2026-05-04T04:55:12Z-verbal.md`
- **Inbox deduplication:** Retained cleared entry; removed initial rejection (superseded)
- **Status in registry:** Approved (AC satisfied, gate clear comment posted on GitHub)

## 2026-05-04T10:56:32Z — Scribe: Team spawn manifest completion

Scribe orchestrated team spawn completion. Your audit findings have been merged to decisions.md.
