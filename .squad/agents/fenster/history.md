
## 2026-05-08: Input Dead Code Cleanup (Scribe Log)

Team session: dead code elimination in input routing.
- Keaton: Deleted game_state_end_screen_routing.cpp, inlined routing helper
- Baer: Audited test/benchmark code
- Fenster: Removed duplicate GoEvent test, unused GamePhase param
- Kujan: Reviewed and approved all changes

Status: COMPLETE. All validation/build tests passed.

# Fenster — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Tools Engineer
- **Joined:** 2026-04-26T02:07:46.545Z

## 2026-05-20 — Removed Stale JSON UI Draw Functions (Issue #73)

Investigated whether old JSON-driven UI rendering was still active behind the rguilayout screen controllers. Found:

- **JSON UI itself is NOT rendered** — `ui_render_system` does not access `UIState::screen` or `UIState::overlay_screen` JSON for rendering
- **Stale draw functions WERE rendering** — `draw_level_select_scene` and `draw_hud` rendered custom visuals before calling rguilayout screen controllers
- **JSON still loaded and used** — for layout caches (`HudLayout`, `LevelSelectLayout`) and ECS entity spawning (`spawn_ui_elements`)

**Cleanup:**
- Deleted `draw_level_select_scene` (121 lines) and `draw_hud` (205 lines) from `ui_render_system.cpp`
- Removed calls to these functions from LevelSelect, Gameplay, and Paused screen cases
- Updated comments in generated headers and screen controllers to reflect that dynamic rendering will be ported to rguilayout in future work

**Build validation:** Zero warnings, all 2635 assertions pass in 901 test cases.

**Learning:** When migrating to a new UI system, watch for layered rendering where old and new systems both execute sequentially. The old system may not access original data sources directly but still render via intermediate caches/derived state. Look for specialized draw functions that execute before the new render path, not just data access patterns.

## 2026-05-20 — Documentation Cleanup: Spec Consistency Pass (Kujan Follow-up)

Three minor stale doc snippets removed from `design-docs/rguilayout-portable-c-integration.md`:

1. **Line 268:** Removed "(future build integration)" wording from `raygui_impl.cpp` comment — it is already wired into CMakeLists.txt.
2. **Line 347:** Renamed section header from `### CMake Integration (Future)` to `### CMake Integration` — the build integration is current/implemented, not aspirational.
3. **Line 122 (code example):** Updated illustrative snippet from old `reg.ctx<GamePhase>()` syntax to current `reg.ctx().get<GamePhase>()` pattern matching real codebase usage across all screen controllers and systems.

Adapter terminology remains confined to the "Migration History: … (Completed)" section as intended. Final spec now reflects current implemented architecture without misleading future/pending wording.

## 2026-05-20 — Fixed Title/Level-Select UI Regressions Post-Migration

**Context:** After migrating from JSON-driven UI to rguilayout screen controllers, two regressions reported by user:
1. Title screen text (SHAPESHIFTER title + "TAP TO START" prompt) was too small
2. Level select screen showed no level cards or difficulty buttons

**Root Causes:**
- **Title screen:** Generated layout header had hardcoded font sizes (48px for title, 32px for prompt) that were too small. GuiLabel text size is controlled by raygui's global `DEFAULT`/`TEXT_SIZE` style property, not by Rectangle height values.
- **Level select:** The old `draw_level_select_scene()` function (removed in cleanup) rendered level cards and difficulty buttons using raylib primitives. The screen controller only rendered the static heading and Start button from the generated layout; dynamic content was never ported.

**Implementation:**
1. **Title screen fix:** Added `GuiSetStyle(DEFAULT, TEXT_SIZE, 60)` before rendering title screen and restored previous value afterward. Increased font size from implicit default (~10px) to 60px for better visibility.
2. **Level select fix:** Ported level card rendering logic into `render_level_select_screen_ui()`:
   - Draws 3 level cards with raylib `DrawRectangleRounded()` primitives
   - Uses GuiLabel for level titles (32px font) and track numbers
   - Renders difficulty buttons for selected card only (EASY/MEDIUM/HARD)
   - Colors and layout match old JSON-driven appearance
   - Maintains existing keyboard/mouse selection state from `LevelSelectState`

**Build & Test Results:**
- ✅ Zero warnings on macOS arm64 clang with `-Wall -Wextra -Werror`
- ✅ All 2595 assertions pass in 880 test cases

**Learnings:**

1. **GuiLabel font size is global, not per-control:** raygui `GuiLabel()` respects the global `GuiGetStyle(DEFAULT, TEXT_SIZE)` property, not the Rectangle height parameter. To change text size for a specific screen or control group, save the current style value, set the desired size, render, then restore. The Rectangle height only affects control bounds/clipping.

2. **Migration cleanup can hide regression scope:** When removing old rendering code, check for both direct data dependencies AND visual rendering functions. The old `draw_level_select_scene()` didn't read from `UIState::screen` JSON, but it was still the sole renderer for dynamic level cards. Deleting it without porting its visual output created a silent regression. Always grep for screen-specific draw/render functions when migrating UI systems.

3. **Screen controllers can mix generated+custom rendering:** rguilayout is best for static layouts (headings, buttons, labels), but dynamic content (lists, grids, conditionally visible elements) still needs custom rendering code. The pattern: generated layout renders static chrome, then screen controller adds dynamic content using raylib/raygui primitives. This hybrid approach is acceptable until rguilayout supports data-driven list/grid layouts.

## 2026-05-20 — Restored Gameplay HUD Shape Buttons + Energy Bar Fidelity

Ported the removed HUD behavior (shape buttons, approach-ring affordance, colorful segmented energy bar, lane divider) into `gameplay_hud_screen_controller.cpp` so gameplay UI stays fully in the screen-controller path. Kept recent fixes intact for pause button transitions and score/high-score labels.

**Learnings:**

1. **Preserve dynamic HUD visuals during controller migration:** generated rguilayout output can carry only static controls, so dynamic affordances (approach rings, active-shape highlighting, beat-reactive bars) must be explicitly re-implemented in the screen controller or they silently disappear.
2. **Keep HudLayout as style data even after JSON element rendering removal:** layout-cache structs remain a stable source of theme and spacing values for custom controller rendering; adding a small fallback prevents blank HUD when layout parsing fails.


## 2026-05-10 — Segment-Focus Onset Generation (replaces motif n-gram selection)

**Context:** Motif n-gram detection (`detect_variable_length_onset_motifs`) was making level 3 (3_mental_corruption) unreadable because n-gram driven obstacle selection over-densified patterns in a way disconnected from musical feel. The team approved moving to segment-level onset focus as the active experimental generation path.

**Implementation:**
- Added `_segment_class_stats()`, `_choose_segment_focus()`, `select_segment_focus_beats()`, and `design_level_segment_focus()` to `tools/level_designer.py`.
- Each song structure section (segment) independently votes a focus class (percussive/harmonic/full-spectrum) based on total onset flux. Anti-repetition: if the same class dominates ≥ `SEGMENT_FOCUS_ANTI_REPEAT_MAX` (2) consecutive segments, its score is halved and another class may win.
- Class→lane/shape mapping unchanged: percussive→lane 0+triangle, harmonic→lane 2+circle, full-spectrum→lane 1+square.
- Difficulty is purely rank-based on flux within the chosen focus class: easy=top 40%, medium=top 65%, hard=top 90%. No random selection.
- `design_level_onset_motif()` kept but demoted to diagnostic-only; motif n-gram code unchanged.
- Routing: `build_beatmap(experimental_onset_timing=True)` now calls `design_level_segment_focus()` instead of `design_level_onset_motif()`.
- `write_snap_diagnostics` now emits `generation_path: "segment_focus"` and per-segment diagnostics (start/end, focus, class counts, flux stats, difficulty thresholds, fallback reason) in `selection_summary_by_difficulty`.

**Tests:** `tools/test_level_designer_onset_spike.py` rewritten — old motif_id assertions removed, new segment-focus tests added (11 tests, all pass). Game tests: 1848 assertions, all pass.

**Shipped beatmaps regenerated:** `content/beatmaps/{1_stomper,2_drama,3_mental_corruption}_beatmap.json` using `--experimental-onset-timing` flag. Diagnostics updated under `tools/diagnostics/onset_spike/`.

**Learning:** Motif n-gram detection is useful as a diagnostic but fragile as a primary obstacle selector — it over-privileges statistically repeated sub-sequences regardless of musical feel. Segment-level focus is more robust: it asks "what does this section of the song sound like?" and answers consistently, which means the player gets a readable musical story even when onset density is high.

## Learnings

- 2026-05-08T13:57:50.741-07:00 — Raylib systems audits should separate three buckets: direct safe swaps (e.g. `CheckCollisionRecs`, `Clamp`/`Fade`), design-gated substitutions that change state/test seams (`IsMusicStreamPlaying`, `SetGamepadVibration`, direct `PlaySound`), and domain timing/beat logic where raylib has no replacement.
- Pause screen text parity fix requires updating both generated call-site values in `app/ui/generated/paused_layout.h` and source geometry in `content/ui/screens/paused.rgl`; changing only one side risks regeneration drift.
- For centered-label readability fixes, preserve existing helper pattern and change only label rectangle/size arguments; keep pause button rectangles/actions untouched.
- Focused validation for this UI artifact path can be done with `cmake --build build --target shapeshifter_tests` followed by `./build/shapeshifter_tests` to confirm no regressions.
- For raygui-backed hit targets that must stay visually custom, wrap `GuiButton` with temporary per-control style overrides (transparent button state colors) and restore immediately; this preserves hit testing without leaking global HUD alpha/state.
- Beatmap editor help UX can reuse a single `bindModal({ trigger, modal, close })` helper in `tools/beatmap-editor/js/main.js` to keep settings/help dialog behavior consistent (open, overlay click dismiss, Escape dismiss, `aria-hidden` toggling).
- Static, dependency-free UI regression coverage is practical in Node by asserting shell/wiring invariants from source files (`tools/beatmap-editor/test/help-modal-ui.test.js`) when DOM execution seams are unavailable.
- Validation evidence for the help-modal change set: `node --check tools/beatmap-editor/js/main.js`, `node --check tools/beatmap-editor/test/help-modal-ui.test.js`, `node --test tools/beatmap-editor/test/*.test.js` (22/22 pass), and `git --no-pager diff --check` all passed.
- 2026-05-09T12:47:58.529-07:00 — Onset-time spike can stay low-risk by gating it behind an explicit CLI flag, preserving default beat-snapped `time_sec`, and emitting onset-vs-beat comparison diagnostics (`residuals`, `IOI`, dense-cluster, subdivision/source histograms) as artifacts.
- 2026-05-09T20:33:09.658-07:00 — Timing popup readability hotfix: when tuning judgement text visibility, centralize grade color/size in `app/entities/popup_entity.cpp` (`init_popup_display` + timing-tier color branch) to keep gameplay scoring semantics untouched.

## 2026-04-29T09:55:21Z — Pause Screen Text Fix (Approved)

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Task:** Apply exact numeric AC corrections to pause-screen text labels (non-Keaton reviser per lockout protocol).

**Scope:** Correct only the three pause label call-site bounds/sizes in `app/ui/generated/paused_layout.h` and mirror them in `content/ui/screens/paused.rgl`. Buttons/actions unchanged.

**Applied Values:**
- `PAUSED`: (x=90, y=420, w=540, h=80), text size **56**
- `TAP RESUME TO CONTINUE`: (x=90, y=540, w=540, h=36), text size **24**
- `OR RETURN TO MAIN MENU`: (x=90, y=760, w=540, h=36), text size **24**

**Guardrails Respected:**
- Pause buttons (RESUME, MAIN MENU) geometry/order/actions/timing intact
- No legacy UI paths restored (no loader/spawner/cache/adapter/vendor/standalone export changes)
- `PausedLayout_DrawCenteredLabel()` helper implementation matches `SongCompleteLayout_DrawCenteredLabel` exactly

**Validation:**
- Build: zero warnings (clang -Wall -Wextra -Werror)
- Tests: 2148 assertions, 771 test cases — all pass

**Verdict:** ✅ APPROVED by Kujan (2026-04-29T09:55:21Z)

**Sign-off:** All blocking AC from Keaton rejection met exactly. No new issues found. Pattern is consistent across both layout headers. Controller unchanged; action dispatch and timing preserved. Scope was precise: only three label call-site arguments plus mirrored .rgl geometry.

**Orchestration:** `.squad/orchestration-log/2026-04-29T09:55:21Z-fenster.md`

## 2026-04-29 — Gameplay shape buttons migration (revisions R2 → rejected)

**Status:** TWO-PASS REVISION CYCLE
**Reviewer:** Kujan
**Verdict (R2):** REJECTED (reachability regression + geometry drift)

**Revision 1 (Visual Fix):**
- Hid stock rectangular raygui button visuals by temporarily overriding BUTTON style colors to transparent during shape `GuiButton` calls
- Preserved input/state capture path (raygui ownership intact)
- Kept custom circular silhouettes and approach rings visible
- Build and tests passing

**Kujan feedback (R2):** Visual fix approved, but upstream blockers discovered:
- Tap forgiveness regression: 140×100 input rectangles cannot reach the required ±70px vertical forgiveness band (legacy circular geometry)
- Shape slot geometry diverges from `gameplay.rgl`: controller uses constants-derived math instead of `.rgl` as authoritative source

**Reassignment:** Keyser (Lead Architect, non-locked) to address circular hit detection and generated-layout alignment.

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-fenster.md`

---

## 2026-05-09T00:41:48.960-07:00 — Beatmap Quality Loop 1 (Diagnostics-First)

Implemented a subdivision-aware diagnostics path in `tools/level_designer.py` behind opt-in CLI flags:

- `--diagnostics-out <dir>` writes diagnostics artifacts.
- `--diagnostics-only` runs diagnostics without generating/updating beatmap output.

Added candidate snap analysis for:

- current quarter-only snap (`snap_events_to_beats`, 80ms tolerance, one event per beat),
- quarter grid,
- eighth grid,
- triplet grid.

Each diagnostic row now includes candidate label, source event index, beat index, grid time, subdivision label, residual (ms), abs residual (ms), flux, intensity, derived intensity confidence, and pass count.

Loop 1 output artifacts generated for `2_drama` in `tools/diagnostics/2_drama_loop1/`:

- `snap_candidate_events.csv`
- `snap_diagnostics_summary.json`

Summary signal on `2_drama`:

- Current snap keeps 60/216 events.
- Eighth grid captures much more usable off-beat material (170 events within 20ms).
- Triplet grid shows broader but still quantifiable alignment (216 within 80ms).
- Same-shape run metrics were added as baseline diagnostics from generated difficulties.

Validation run:

- `python3 -m py_compile tools/level_designer.py`
- `python3 tools/level_designer.py content/beatmaps/2_drama_analysis.json --diagnostics-out tools/diagnostics/2_drama_loop1 --diagnostics-only`
- `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests`
- Result: all tests passed (1840 assertions in 686 test cases).

## 2026-05-09T12:55:54.558-07:00 — Generated Onset-Timed Spike Levels for Shipped Songs

Generated experimental onset-timed beatmaps for all shipped songs using existing analysis JSON files and `tools/level_designer.py --experimental-onset-timing`, while preserving shipped defaults.

- Inputs: `content/beatmaps/{1_stomper,2_drama,3_mental_corruption}_analysis.json`
- Experimental outputs: `content/beatmaps/experimental/onset_spike/*_beatmap_onset_spike.json`
- Diagnostics outputs: `tools/diagnostics/onset_spike/<song>/{snap_candidate_events.csv,snap_diagnostics_summary.json,onset_timing_events.csv}`

Smoke check confirmed each generated JSON parses and includes onset timing metadata (`timing_source`, `beat_time_sec`, onset-backed entries) and each diagnostics bundle includes onset comparison summaries for easy/medium/hard.

Validation runs:

- `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests`
- generation loop for all shipped `*_analysis.json` with `--experimental-onset-timing --diagnostics-out`
- post-generation `./build/shapeshifter_tests`

Result: all tests passed (1840 assertions in 686 test cases) before and after generation.

Learning: use a stable convention for non-destructive spike outputs —
`content/beatmaps/experimental/onset_spike/<song>_beatmap_onset_spike.json` plus
`tools/diagnostics/onset_spike/<song>/` — then selectively copy into shipped paths only when doing in-game playtests.

## 2026-04-30T02:04:27Z — Dead Code Prune — Input Routing Doc Revision

**Session:** Multi-agent dead code cleanup; independent revision under reviewer lockout.

**Your role:** Rework rejected cleanup artifacts (Keaton code cleanup, McManus doc updates) to clarify input routing semantics.

**Files revised:**
- `design-docs/feature-specs.md` — clarified raw input routing (`InputEvent → GoEvent`)
- `design-docs/architecture.md` — documented semantic event routing; `ButtonPressEvent` originates from raygui/controller emitters only
- `tests/test_entt_dispatcher_contract.cpp` — revised test comments to reflect correct routing

**Key clarifications:**
- Raw input dispatcher: handles swipes, keyboard → emits `InputEvent` → system converts to `GoEvent`
- Semantic UI dispatcher: raygui/controller → emits `ButtonPressEvent` (no raw input involvement)

**Outcome:** ✓ APPROVED by Kujan (re-review). Rework integrated into cleanup session. Full test validation passed.

## 2026-05-08T13:32:04.383-07:00 — app/util cleanup audit learning

For util cleanup passes, classify each helper by owning runtime surface first (beatmap/session/game-state/persistence) before proposing deletions. Most safe removals are single-consumer headers (`test_player_helpers`, `safe_localtime`, `enum_names`) that can be inlined into their owning systems; persistence and beatmap loaders need staged migration because tests explicitly assert structured failure statuses.
- 2026-05-09T13:00:47.473-07:00 — For onset-timing playtest spikes, it is acceptable to intentionally overwrite shipped `content/beatmaps/*_beatmap.json` on a worktree branch so `./run.sh` exercises the new timing path without runtime flags; keep experimental copies/diagnostics for easy rollback.

## 2026-05-09T20:58:25.533-07:00 — Onset Motif Spike: Variable-Length Pattern Selection

Implemented a new experimental onset pipeline in `tools/level_designer.py` that detects repeated variable-length onset-token motifs (class + subdivision + relative beat spacing) and drives difficulty selection by motif role rather than random/first-N onset inclusion.

- Added onset class tagging (`percussive` / `harmonic` / `full-spectrum`) and fixed class→lane/shape mapping for spike output.
- Added variable-length motif detection with repeat-count scoring and event role tagging (`skeleton`, `motif_core`, `ornament`, `fill`).
- Experimental beatmap generation now uses motif-aware event ranking per section and difficulty, preserving onset timing metadata while authoring obstacles.
- Added diagnostics fields in `onset_timing_events.csv` and summary metrics (`motif_stats`, role/class distributions) to prove motif behavior.
- Updated spike-focused Python tests for new diagnostics schema + class mapping expectations.

Validation executed:
- `python3 -m py_compile tools/level_designer.py tools/validate_onset_spike_artifacts.py tools/test_level_designer_onset_spike.py tools/test_validate_onset_spike_artifacts.py`
- `python3 -m unittest tools/test_level_designer_onset_spike.py tools/test_validate_onset_spike_artifacts.py`
- `./run.sh test` (fails on shipped beatmap balance/shape-gap assertions after intentional onset-spike beatmap overwrite).

## 2026-05-09T21:17:01.672-07:00 — Onset/Motif-Only Generation Rules Enforced

Implemented the approved onset/motif-only spike rules in `tools/level_designer.py` for `--experimental-onset-timing` (used to regenerate shipped beatmaps on this branch):

- Selection now derives only from onset-token motif roles (no section-density target curves, no random thinning/sampling).
- Difficulty inclusion now follows approved motif depth rules:
  - easy: skeleton
  - medium: skeleton + motif_core
  - hard: skeleton + motif_core + ornament + fill
- Fixed onset class mapping remains authoritative:
  - percussive → lane 0 + triangle
  - harmonic → lane 2 + circle
  - full-spectrum → lane 1 + square
- Legacy cleanup/post-process influence was removed from this onset path (no first-collision floor, max-gap fill, gap-one/shape-gap/readability balancing passes in onset generation).
- Onset path now errors if an authored obstacle lacks a source onset event (beat fallback disabled on this path).
- Diagnostics now explicitly mark legacy influence disabled and list disabled legacy rule families.

Regenerated shipped beatmaps (`content/beatmaps/*_beatmap.json`) plus onset diagnostics (`tools/diagnostics/onset_spike/*`) so `./run.sh` reads the new levels immediately.

Validation:
- `python3 -m py_compile tools/level_designer.py tools/validate_onset_spike_artifacts.py tools/test_level_designer_onset_spike.py tools/test_validate_onset_spike_artifacts.py`
- `python3 -m unittest tools/test_level_designer_onset_spike.py tools/test_validate_onset_spike_artifacts.py`
- `python3 tools/validate_onset_spike_artifacts.py --diagnostics-dir tools/diagnostics/onset_spike/{1_stomper,2_drama,3_mental_corruption}` (report mode)
- `./run.sh test` ✅

Test updates required due deliberate legacy-rule disablement on this path:
- `tests/test_shipped_beatmap_max_gap.cpp`
- `tests/test_shipped_beatmap_gap_one_readability.cpp`
- `tests/test_shipped_beatmap_first_collision.cpp`
- `tests/test_shipped_beatmap_medium_balance.cpp`

These now validate onset-spike-compatible structural/canonical mapping invariants instead of legacy max-gap/gap-one/first-collision/lane-coverage enforcement.

## 2026-05-10 — Onset extraction tuning + diagnostics

**Task**: Improve onset extraction sensitivity for sparse beatmaps (especially `1_stomper`).

### Root cause found
`_detect_onsets_from_envelope` had `wait=2` hardcoded (was conserving but slightly tight) and most critically,
`threshold=0.3` (peak_pick `delta`) on a 0→1 normalized envelope was far too aggressive.
The finetuned config had reverted to a single resolution (`n_fft=2048, hop_length=384`) instead of 3,
losing 2/3 of detection surface. Combined: only 33 events from 5 onsets/pass for a 200s song.

### Changes made
1. **`rhythm_pipeline.py`** — `_detect_onsets_from_envelope` and `_detect_pass_onsets`:
   - Peak-pick params (`pre_max`, `post_max`, `pre_avg`, `post_avg`, `wait`) now read from `peak_pick` config section.
   - Zone-specific threshold overrides (`zone_thresholds`) now applied per-pass before `peak_pick`.
   - `build_analysis` emits `onset_diagnostics` key with raw_per_pass, merged_events, events_per_minute, segment_coverage.

2. **Config files** (both `rhythm_librosa_params.json` and `rhythm_librosa_finetuned_params.json`):
   - `threshold`: 0.30 → 0.15 (global)
   - `zone_thresholds`: `{bass: 0.10, low_mid: 0.13, high_mid: 0.15}`
   - `peak_pick`: `{wait: 1}` (was hardcoded 2), rest default 3/3/5/5
   - Finetuned: restored from 1 resolution to 3 (`n_fft: 1024/2048/4096, hop_length: 256`)

3. **`level_designer.py`** — `write_snap_diagnostics`:
   - `onset_pool_summary` now always emitted (not gated on experimental flag).
   - Fields: `total_events`, `events_per_minute`, `snapped_events`, `snap_residual_stats`,
     `segment_count`, `empty_segment_count`, `segment_coverage`, `raw_per_pass`, `raw_total`.
   - Experimental block now includes `obstacle_counts_by_difficulty`.

4. **Tests** (`test_level_designer_onset_spike.py`) — 6 new tests added:
   - `test_onset_pool_summary_present_in_diagnostics`
   - `test_onset_pool_summary_experimental_adds_obstacle_counts`
   - `test_build_beatmap_counts_scale_with_difficulty`
   - `test_richer_onset_pool_in_varied_fixture`
   - `test_segment_diagnostics_include_n_difficulty_fields`
   - `test_diagnostics_deterministic_across_runs`

### Results
| Song | Events before | Events after | BPM |
|---|---|---|---|
| 1_stomper | 33 (5/pass) | 204 (61/min) | 160 |
| 2_drama | (regenerated) | 1085 (266/min) | 130 |
| 3_mental_corruption | (regenerated) | 1736 (460/min) | 150 |

**1_stomper obstacles**: 14/17/19 → **23/35/51** (easy/medium/hard)

All 17 Python tests pass. All 1848 C++ assertions pass.

## 2026-05-10 — Tools/Beatmap Audit (Issues #382-#385)

Audit scope: `tools/level_designer.py`, rhythm/beatmap validators, diagnostics generation, test tools, run scripts, and the onset beatmap generation workflow. No code changes were made in this pass beyond this history note.

Findings filed:

1. **#385 — Onset segment-focus path collapses layer-specific events sharing a beat**
   - `snap_events_to_beats()` preserves one event per `(beat_idx, onset_class)`, but the active segment-focus path rekeys selected events by `beat_idx` only.
   - Learning: layer-aware preservation must be verified end-to-end, not only at merge/snap boundaries. Any necessary same-beat collision policy should be explicit and diagnosed.

2. **#384 — Diagnostics output can leave stale onset spike artifacts across modes**
   - `write_snap_diagnostics()` uses `exist_ok=True` and overwrites the summary, but only writes `onset_timing_events.csv` in experimental mode.
   - Learning: diagnostics directories should be reproducible run outputs. Generated artifacts need cleanup, a manifest, or mode-specific output directories to prevent stale-file validation.

3. **#382 — run.sh ignores caller-provided VCPKG_ROOT**
   - `run.sh` unconditionally exports `VCPKG_ROOT="$HOME/vcpkg"` before calling `build.sh`, defeating environment-based toolchain configuration.
   - Learning: wrapper scripts should preserve caller-provided tool paths and let lower-level scripts validate them.

4. **#383 — Loop 2 content validator count check ignores invalid beat rows**
   - The validator compares declared `count` to raw `len(beats)` while the rest of the metrics use filtered integer-beat rows.
   - Learning: validator diagnostics should distinguish raw rows, valid authored obstacles, and declared counts so malformed rows cannot hide behind matching totals.

Duplicate checks were run with `gh issue list --state all --search` for each finding before filing. Labels applied: `squad`, `squad:fenster`.

## 2026-05-10 — Fixed audit issues #382, #383, #384, #385

Implemented focused fixes on branch `audit/autonomous-quality-loop`.

- **#382** (`run.sh`): preserved caller-provided toolchain path by defaulting only when unset:
  - `export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"`

- **#383** (`tools/validate_loop2_content_gates.py`): count gate now compares declared `count` against valid integer-beat obstacles (same population used by other metrics), and diagnostics now report all three numbers: declared count, valid beat count, and raw rows.
  - Added test coverage in `tools/test_validate_loop2_content_gates.py` for invalid beat rows.

- **#384** (`tools/level_designer.py`, `tools/validate_onset_spike_artifacts.py`): diagnostics writer now clears known generated artifacts in output dir before writing, preventing stale `onset_timing_events.csv` carryover across mode switches. Spike validator now hard-errors when summary is not `experimental_onset_timing.enabled=true`.
  - Added tests for stale-artifact cleanup and non-experimental validator failure.

- **#385** (`tools/level_designer.py`): segment-focus selection now keys selected/snapped events by `(beat_idx, onset_class, source_event_idx)` and preserves per-event onset class instead of collapsing by beat index. Timing attachment/diagnostics lookups now resolve events by `source_event_idx` (with safe beat fallback only when unambiguous).
  - Added focused test proving same-beat cross-layer events remain distinct when selected.

Validation run:
- Targeted tests:
  - `python3 -m unittest tools/test_validate_loop2_content_gates.py tools/test_level_designer_onset_spike.py tools/test_validate_onset_spike_artifacts.py` ✅
- Full validation:
  - `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests` ✅

## 2026-05-10T02:40:52-07:00 — Autonomous Quality Loop 2 Audit (Post-PR #408)

Filed follow-up tooling audit issues:
- #409 Validators crash on malformed/non-integer beat rows (`validate_max_beat_gap.py`, `validate_gap_one_readability.py`) instead of reporting diagnostics.
- #410 `validate_beatmap_offset.py` is cwd-dependent (`Path("content/beatmaps")`) and fails outside repo root.

Learning: After hardening one validator path (#383), audit sibling validators for the same malformed-row and path-resolution assumptions; these regressions tend to exist in parallel scripts.
