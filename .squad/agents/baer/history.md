# Baer — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Test Engineer
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings

<!-- Append learnings below -->

### 2026-04-25 — Regression/Platform Diagnostics Run

**Test run baseline:** 508 tests (CTRF), 526 test cases executed (including 18 bench), 1611 assertions. All pass.

**Key structural findings:**
- `shapeshifter_tests` is only built for `NOT EMSCRIPTEN` (CMakeLists.txt:350). WASM build has zero tests.
- `squad-ci.yml` and `squad-release.yml` contain placeholder TODO echo commands — no real build/test executed on `dev` branch PRs or releases.
- `std::srand(N)` in `tests/test_obstacle_spawn_extended.cpp` creates brittle coupling to `std::rand()` call order in `obstacle_spawn_system.cpp`.
- Bench tests excluded from CI run via `"~[bench]"` filter — no performance regression tracking.
- `input_system` is untested (calls raylib directly; swipe threshold, multi-touch, zone routing are spec'd but not exercised).
- `ring_zone_log_system` has zero test coverage.
- LanePush collision behavior (player push + edge no-op) only has a spawn-existence test, not a collision/behavior test.
- Real `content/beatmaps/*.json` files never loaded in tests; CI ignores `content/**` changes entirely.
- FTUE/tutorial flow has zero tests.
- `test_test_player_system.cpp` entire file is gated `#ifdef PLATFORM_DESKTOP` — 12 tests silently skipped on any non-desktop test run.

### 2026-04-25 — #125 low_bar/high_bar validation (parallel with Rabin's fix)

**Root cause confirmed:**
- `DIFFICULTY_KINDS["hard"]` in committed code only contains `{"shape_gate", "lane_push"}` — missing `"high_bar"` and `"low_bar"`.
- `SECTION_ROLE` in committed code is missing `"chorus"` and `"outro"` entries entirely, and none of the section types include `"high_bar"` or `"low_bar"`.
- Rabin's working-tree changes add both to `DIFFICULTY_KINDS["hard"]` and all active section roles.

**Beatmap JSON format:**
- `difficulties[diff]["beats"]` is the canonical key (level_designer.py emits `"beats"`, not `"obstacles"`).
- C++ loader (`beat_map_loader.cpp`) reads the `"beats"` key under `difficulties[difficulty]` and falls back to flat `"beats"` array.
- `low_bar` and `high_bar` parse successfully without `"shape"` or `"lane"` fields — the loader accepts absent fields gracefully.

**Regression risks for jump/slide readability:**
- `low_bar`/`high_bar` won't appear in easy or medium (DIFFICULTY_KINDS correctly excludes them). Easy readability is safe.
- `lane_of()` returns `fallback=1` for bars, so after a bar, `clean_lane_change_gap` treats the player as at lane 1. This resets the lane context, which is intentional (full-width bars are lane-agnostic), but downstream shape_gates will be sequenced relative to center lane after any bar.
- `clean_type_transition` treats bars as "movement" (non shape_gate), so they count as valid type-switch targets. Gap requirement (≥2 beats) still applies between bar and adjacent shape_gate.
- The `beat_scheduler_system` spawns bars with `constants::LANE_X[1]` (center) — no lane field needed at runtime.
- `obstacle_spawn_system.cpp` has a time-gate (LowBar at t≥45s, HighBar at t≥60s) for the infinite spawn mode, which is separate from beatmap-driven play.

**Deliverables:**
- `tools/validate_beatmap_bars.py` — Python acceptance script; run after beatmap regeneration to assert ≥1 `low_bar` + ≥1 `high_bar` per hard difficulty.
- `tests/test_beat_map_low_high_bars.cpp` — 10 isolated C++ tests (Catch2) covering parse + validate for both kinds; added via CMake GLOB, zero touchpoints on level_designer.py or beatmap JSONs.
- Test count post-merge: 751 test cases (was 741), 2388 assertions (was 2357).

### 2026-04-26 — #135 difficulty ramp validation

**Task:** Add CI-enforceable validation for issue #135 (easy zero variety, medium LanePush cliff).

**Root cause confirmed on current content:**
- 2_drama easy: `square` = 70% of shape_gates → exceeds 65% cap → zero meaningful variety.
- 2_drama medium: LanePush = 28.6% → exceeds 25% cap → readability cliff.
- Other songs (stomper, mental_corruption) currently pass both checks; only drama triggers violations.

**Threshold reasoning:**
- Easy dominant-shape cap: 65% (drama fails at 70%; the others stay comfortably under 50%).
- Medium LanePush bounds: 5–25%. Lower bound ensures the mechanic is taught (all songs already ≥5%). Upper bound catches the drama cliff at 28.6%.
- Consecutive LanePush cap: 3 (matches medium `variety_threshold` already in level_designer; no song currently exceeds 3 so this is a forward-looking guardrail).
- All 3 shapes required in easy: captures any future regression where a song has intro+bridge+outro only (all-square result).

**Deliverables:**
- `tools/validate_difficulty_ramp.py` — Python acceptance script; runs after beatmap regeneration and exits non-zero on violations. Prints exact % for each offending song/difficulty.
- `tests/test_shipped_beatmap_difficulty_ramp.cpp` — 4 Catch2 tests tagged `[difficulty_ramp][issue135]`:
  1. Guard: content directory reachable
  2. Easy: all 3 shapes present + no single shape > 65%
  3. Medium: LanePush% within [5%, 25%]
  4. Medium: max consecutive LanePush ≤ 3
- No edits to level_designer.py or beatmap JSON content.

**Current test run:** 4 test cases, 2 fail (2_drama easy variety + 2_drama medium cliff). Will pass after Rabin regenerates beatmaps.

**Coexistence verified:** `[low_high_bar]` and `[shipped_beatmaps]` tags both run 34 assertions, all pass.

**CI command:**
```
./build/shapeshifter_tests "[difficulty_ramp]"
python3 tools/validate_difficulty_ramp.py
```

### 2026-04-26 — Session closure: issue #125 merged to decisions

- Orchestration logged: `.squad/orchestration-log/2026-04-26T06:53:29Z-baer.md`
- Session log: `.squad/log/2026-04-26T06:53:29Z-issue-125-low-high-bars.md`
- Decision merged to `.squad/decisions.md` under "#125 — Ship LowBar/HighBar on Hard Difficulty"
- Status: APPROVED for merge
- All deliverables complete: validate_beatmap_bars.py added, test_beat_map_low_high_bars.cpp with 10 tests added
- Test count: 751 test cases, 2388 assertions (no failures)

### 2026-04-26 — #137 offset semantics validation

**Task:** Write deterministic regression tests for issue #137 (offset = beats[0] can cause off-beat collisions when beat_indices are non-zero or beats are irregular).

**Key findings:**
- All shipped beatmaps have `offset = beats[0]` (analysis file confirms) and first `beat_index >= 2` (none start at 0). First collision times range from 1.2s–6.8s depending on song/difficulty.
- Beat drift in all 3 shipped songs is ≤ 0.7ms across 500+ beats — safe for current content, but not a forward-looking guarantee.
- The formula `beat_time = offset + beat_index * beat_period` is internally consistent; the bug is *semantic*: if `offset` doesn't represent beat_index=0 of the authored grid, all collisions shift by the accumulated error.
- Rabin's inbox entry (`rabin-offset-content-137.md`) documents two valid interpretations; Fenster owns the semantics call.
- The C++ runtime formula is correct as long as offset is anchored to beat_index=0; the regression guard catches any future pipeline change that decouples them.

**Deliverables:**
- `tests/test_beat_scheduler_offset.cpp` — 9 Catch2 tests tagged `[beat_scheduler][offset][issue137]`, 26 assertions. Pin the formula contract, guard global offset shift, expose drift accumulation semantics, encode real shipped-beatmap values (stomper 2.27s, etc.).
- `tools/validate_offset_semantics.py` — 7 deterministic Python suites (no audio required). Tests: uniform zero-drift, jitter drift-grows meta-test, offset = beats[0] contract, non-zero first index, global shift, corrected-offset reduces drift, shipped beatmap cross-validation against analysis JSON.

**Post-add totals:** 739 test cases, 2392 assertions, all pass (vs. previous 734/2366 baseline).

**CI commands:**
```bash
# New gate
./build/shapeshifter_tests "[offset]"
python3 tools/validate_offset_semantics.py

# Existing gates (unchanged, all green)
./build/shapeshifter_tests "[shipped_beatmaps]"    # #134 shape gap
./build/shapeshifter_tests "[difficulty_ramp]"     # #135 ramp
./build/shapeshifter_tests "[low_high_bar]"        # #125 bars
```

**Coexistence:** All 3 prior gate suites pass unmodified. No CMakeLists.txt changes needed (GLOB picks up new test file).

**Decision filed:** `.squad/decisions/inbox/baer-offset-validation-137.md`

---

### 2026-04-26 — #134 min_shape_change_gap CI enforcement

**Task:** Add CI-enforceable validation for issue #134 (shipped beatmaps violate min_shape_change_gap=3).

**Key findings:**
- `validate_beat_map` Rule 6 in `beat_map_loader.cpp` already enforces `min_shape_change_gap` correctly — the C++ logic was never the bug.
- No existing test loaded shipped `content/beatmaps/*.json` files; CI was blind to content violations. This was the gap.
- Rabin's working-tree changes had already fixed all beatmap content before this session; `tools/check_shape_change_gap.py` (Python) was also added by Rabin and exits 0 on all current content.
- CTest runs with `WORKING_DIRECTORY = build/`, not `CMAKE_SOURCE_DIR`. CMake's `POST_BUILD` copy commands keep `build/content/beatmaps/` in sync with source, so relative paths `"content/beatmaps/"` work correctly in tests.

**Deliverable:**
- `tests/test_shipped_beatmap_shape_gap.cpp` — 2 Catch2 tests: (1) guard that verifies `content/beatmaps/` is reachable, (2) regression that loads every `*_beatmap.json` for easy/medium/hard and calls `load_beat_map` + `validate_beat_map`, failing with `FAIL_CHECK` per violation so all offenders are reported in one run. Tagged `[shipped_beatmaps][regression][issue134]`.
- Test picked up automatically via CMake `file(GLOB TEST_SOURCES tests/*.cpp)` after reconfigure.
- Post-add totals: 725 test cases, 2360 assertions, all pass.

**CI command:**
```
./build/shapeshifter_tests "[shipped_beatmaps]"
```
or via ctest:
```
ctest --test-dir build -R "shipped beatmaps"
```

### 2026-04-26 — Session closure: #134 CI enforcement approved

**Task completion:**
- `tests/test_shipped_beatmap_shape_gap.cpp` added with 2 Catch2 tests:
  1. Guard test verifies `content/beatmaps/` reachable from CTest working directory
  2. Regression test loads every `*_beatmap.json` for all difficulties, calls `load_beat_map` + `validate_beat_map`, reports all violations in single run via `FAIL_CHECK`
- Test picked up automatically by CMake `file(GLOB TEST_SOURCES)` — no CMakeLists.txt changes needed
- C++ test is authoritative CI gate; Python `check_shape_change_gap.py` is dev-time convenience tool
- Post-add totals: 725 test cases, 2360 assertions, all pass

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T07:03:05Z-baer.md`
**Session log:** `.squad/log/2026-04-26T07:03:05Z-issue-134-shape-gap.md`
**Decision merged** to `.squad/decisions.md` under "#134 — Enforce min_shape_change_gap in Shipped Beatmaps"
**Status:** APPROVED for merge

**CI command examples:**
```bash
# Direct test run
./build/shapeshifter_tests "[shipped_beatmaps]"

# Via ctest
ctest --test-dir build -R "shipped beatmaps"

# Exclude benches (CI filter)
./build/shapeshifter_tests "~[bench]"
```

**Coexistence:** `[low_high_bar]` tests (10 cases from #125) continue passing — `validate_beat_map` explicitly skips LowBar/HighBar in Rule 6 check.

## 2026-04-27 · #135 REJECTED by Kujan → Revision by Verbal

- **Blocking issue #2 (Baer's original artifact):** Test suite lacked kind-exclusion guard. Easy tests checked shape *variety* but not that *only* shape_gate obstacles appear. This allowed Rabin's lane_push contamination in easy beatmaps to pass silently.
- **Root cause pattern:** Checking distribution within a category (shape variety among shape_gates) does not imply purity of that category. Whenever a difficulty contract restricts *which kinds* are allowed, a separate kind-exclusion test is mandatory alongside distribution checks.
- **Kujan locked out Baer; reassigned to Verbal.**
- **Verbal revision:** Added `[shape_gate_only]` test case to `tests/test_shipped_beatmap_difficulty_ramp.cpp` and `check_easy_shape_gate_only()` function to `tools/validate_difficulty_ramp.py`. Both use `FAIL_CHECK` so every violation per file is reported in a single run.
- **Convention established:** Every difficulty contract test must include (1) kind-exclusion assertions and (2) distribution/variety assertions. This two-layer approach catches both *presence* violations and *distribution* violations.
- **Guard status:** Confirmed to detect violations (exits non-zero on dirty content with easy lane_push). Ready for McManus's beatmap fixes.

## 2026-04-27 · #135 APPROVED: McManus revisions + Verbal testing

- **McManus implementation fix:** Set `LANEPUSH_RAMP["easy"] = None`, disabled early injection entirely. Regenerated all 9 beatmaps: easy 100% shape_gate (zero lane_push/low_bar/high_bar), 3 shapes, dominant ≤60%. Medium lane_push preserved and in-bounds (9.3–19.5%), max consecutive ≤3, start_progress 0.30 respected. Hard bars intact (stomper 1/3, drama 2/2, mental 7/7).
- **Verbal testing:** New `[shape_gate_only]` C++ test + Python `check_easy_shape_gate_only()` guards pass. All 2366 C++ assertions pass (730 test cases).
- **Kujan approved revision.** Both blocking issues from prior rejection resolved. Non-blocking note: medium start_progress constraint not in C++ test (generator-enforced, content-valid; future regen hardening ticket).
- **Decision merged** to `.squad/decisions.md` as "#135 — Difficulty Ramp: Easy Variety + Medium LanePush Teaching".

## 2026-04-27 — Issue #137 Complete: Offset Validation & Team Outcome

- **Status:** ✅ APPROVED. Issue #137 offset semantics validation work completed with team.
- **Test infrastructure:** Added `tests/test_beat_scheduler_offset.cpp` (9 Catch2 tests tagged `[beat_scheduler][offset][issue137]`).
  - Validates offset invariant: `arrival_time = offset + beat_index * beat_period`
  - Offset shift propagation: changing offset by Δ shifts ALL collisions by exactly Δ
  - Per-difficulty offset storage
  - Loader rejects out-of-range beat indices
  - Jitter detection (tempo-variation pathological case)
- **Validation tooling:** `tools/validate_offset_semantics.py` — 7 deterministic Python suites (no audio, no aubio dependency).
  - Offset anchor correctness (computed vs. expected)
  - Beat-index consistency across difficulties
  - Jitter meta-test (certifies detection logic for tempo-varying songs)
- **CI gates unified:** Full orchestration runs `[shipped_beatmaps][difficulty_ramp][low_high_bar][offset]` — all pass (2392 assertions, 757 test cases).
- **Regression protection:** Tests will catch future pipeline changes that decouple offset from beat_index (e.g., if someone adds per-beat timestamps without updating indices).
- **Decisions merged:** `.squad/decisions/decisions.md` includes Baer's offset-validation decision + testing rationale.

## 2026-04-28 — Fresh Regression/Platform Validation Diagnostics

**Scope:** Full sweep of `tests/`, `app/`, `tools/`, `.github/workflows/`, `design-docs/` for coverage gaps and platform validation issues. No code changes made.

**Methodology:**
- Read all 50+ test files (tag inventory, grep for system invocations)
- Read all CI workflows (build steps, paths-ignore, artifact handling)
- Read app systems for untested behaviors
- Cross-referenced against `decisions.md` for known non-blocking notes
- Checked all #44-#162 known issues to avoid duplicates

**Key findings:**

### Coverage gaps

1. **`popup_display_system` — zero test coverage (#208, test-flight)**
   - 40-line system with two code paths (grade text, numeric score), alpha decay, and font-size selection — completely uncovered.

2. **`audio_offset` integration gap (#210, AppStore)**
   - `song_playback_system` and `beat_scheduler_system` both apply `SettingsState.audio_offset_ms` to effective_offset, but no test sets a non-zero offset and verifies the shift. Silent regression risk on any settings refactor.

3. **Medium LanePush `start_progress` not in C++ tests (#212, AppStore)**
   - Explicitly noted in decisions.md #135 as a non-blocking hardening gap. `validate_difficulty_ramp.py` tests it; no C++ equivalent.

### Platform / CI gaps

4. **Python validation tools never in CI (#216, AppStore)**
   - `tools/**` is in `paths-ignore` on all four CI workflows. No CI step executes `validate_difficulty_ramp.py`, `check_shape_change_gap.py`, `validate_offset_semantics.py`, or `validate_beatmap_bars.py`. A broken tool or a content violation caught only by Python silently passes CI.

5. **`test_test_player_system.cpp` silently absent from WASM CI (#218, AppStore)**
   - Entire file wrapped in `#ifdef PLATFORM_DESKTOP`; WASM builds define `PLATFORM_WEB`. Issue-111 regression tests never run in WASM CI.

6. **WASM CI has no CTRF report or PR comment (#220, AppStore)**
   - Native workflows (Linux/macOS/Windows) all upload CTRF artifacts and post PR comments. WASM only logs to CTest stdout. Cross-platform test count comparison is impossible without structured output.

**Duplicates skipped:** None (verified all against #44–#162 list).

**Issues filed:** #208, #210, #212, #216, #218, #220 (6 total).

**Confidence levels:**
- #208 (popup_display_system): High — grep confirmed zero coverage.
- #210 (audio_offset integration): High — searched all test files.
- #212 (start_progress gap): High — decisions.md explicitly calls it out.
- #216 (Python tools in CI): High — read all workflow files.
- #218 (PLATFORM_DESKTOP WASM skip): High — single guard at line 9 of file.
- #220 (WASM CTRF): High — ci-wasm.yml has no upload/report steps.

**Observation for future runs:** The `test_test_player_system.cpp` PLATFORM_DESKTOP guard is the highest-risk gap because it silently drops regression tests for known bugs (#111) in the WASM path. The Python CI gap (#216) is the easiest to fix (add a step to ci-linux.yml).

## 2026-04-28 — Issues #208 and #217 Implemented

### #208 — popup_display_system coverage

**File created:** `tests/test_popup_display_system.cpp` (6 test cases, tag `[popup_display][issue208]`)

**Coverage added:**
- `timing_tier == 3` → text `"PERFECT"`, `FontSize::Medium`
- `timing_tier == 2` → text `"GOOD"`, `FontSize::Small`
- `timing_tier == 1` → text `"OK"`, `FontSize::Small`
- `timing_tier == 0` → text `"BAD"`, `FontSize::Small`
- `timing_tier == 255` (non-shape) → numeric score string (e.g. `"150"`), `FontSize::Small`
- alpha at half lifetime → `pd.a == 127` (static_cast<uint8_t>(0.5f × 255))

**Approach:** Raw `entt::registry` (no `make_registry` helper needed — system uses only entity components, no context singletons). Helper `make_popup_entity()` builds minimal entity with `ScorePopup`, `ScreenPosition`, `Color{255,255,255,255}`, `Lifetime`.

**Result:** 17 assertions in 6 test cases, all pass.

### #217 — Medium first LanePush arrival time regression

**File modified:** `tests/test_shipped_beatmap_difficulty_ramp.cpp`
- Added `MEDIUM_MIN_FIRST_LANEPUSH_SEC = 30.0f` threshold constant.
- Added test `"difficulty ramp: medium first LanePush arrival time >= 30 s"` tagged `[difficulty_ramp][issue217][medium]`.

**Formula used:** `arrival = offset + min_beat_index × (60.0f / bpm)` — matches beat_scheduler_system semantics.

**Robustness:** Uses minimum `beat_index` across all LanePush beats (not just first in array) to guard against unsorted lists. Missing-LanePush maps silently skip.

**Verified shipped content:**
- 1_stomper: beat_index=177 → 66.8 s ✅
- 2_drama: beat_index=150 → 75.2 s ✅
- 3_mental: beat_index=209 → 83.7 s ✅

**Result:** 1 assertion in 1 test case, all pass.

### Full suite
795 test cases, 2637 assertions. Zero regressions introduced.

### Learning
- `popup_display_system` needs only entity components; a bare `entt::registry` suffices. Using `make_registry()` would also work but is unnecessary overhead.
- The `is_lane_push()` helper in `test_shipped_beatmap_difficulty_ramp.cpp` is file-scoped static — available to new test cases in the same file without any additional plumbing.
- `FontSize::Small` is the default in `PopupDisplay{}` (struct field default), so the numeric path gets Small without explicit assignment.

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Full diagnostics pass completed across 13 agents; 76 unique issues filed (163–238), zero duplicates with #44–#162.
- Five decisions merged: #125 (bars), #134 (shape-gap), #135 (difficulty-ramp), #46 (release), #167 (burnout-bank).
- Three TestFlight blockers identified: #222 (aubio timeout), #172 (WASM CI), #236 (haptics).
- Orchestration log: `.squad/orchestration-log/2026-04-26T23:40:25Z-wave-summary.md`.
- Session log: `.squad/log/2026-04-26T23:40:25Z-testflight-wave.md`.
- Decision inbox: merged and deleted.

---

**Session (test cleanup pass):**

## Failures Found and Fixed

### Categories fixed (13 → 0 failing test cases):

**Category A+B — Stale window_scale_for_tier values (issue #223 aftermath):**
- `test_rhythm_system.cpp`: Updated window_scale_for_tier expected values (Perfect=0.5, Good=0.75, Ok=1.0, Bad=1.0). Rewrote 4 window_scaling integration tests to match new inverted spec: better timing = smaller scale = faster window collapse.
- `test_beat_map_validation.cpp`: Updated 4 stale expected values for window_scale_for_tier.

**Category C — Stale collision system tests:**
- `test_collision_system.cpp`: Rewrote "BAD timing adjusts window_start" → "BAD timing does NOT adjust" (Bad scale=1.0, no change). Rewrote "Perfect timing extends window" → "Perfect timing shrinks via window_start adjustment" (Perfect scale=0.5).

**Category D — Death model stale expectations:**
- `test_death_model_unified.cpp`: Loop changed from 8 iterations to SAFE_MISS_COUNT=4 (ENERGY_MAX/ENERGY_DRAIN_MISS - 1). Energy assertions updated accordingly.
- `cleanup_system.cpp`: Restored miss-tagging behavior for unscored Obstacle-tagged entities that scroll past DESTROY_Y. Non-Obstacle entities are still immediately destroyed. Entities get MissTag+ScoredTag; energy drain applied (with epsilon snap < 1e-6f).
- `collision_system.cpp`: Added epsilon snap `if (energy < 1e-6f) energy = 0.0f` to prevent float accumulation drift from blocking game-over trigger.

**Category E — High score integration SIGABRT:**
- `test_helpers.h`: Added `HighScoreState`, `HighScorePersistence`, `GameOverState` to `make_registry()`.
- `play_session.cpp`: Added song_id extraction from beatmap path (strip `_beatmap` suffix), sets `HighScoreState.current_key`, loads `ScoreState.high_score` from stored high score after resetting ScoreState.
- `game_state_system.cpp`: In `enter_game_over` and `enter_song_complete`, when new high score: calls `hs->update_current_high_score()` and `high_score::save_high_scores()` if persistence path non-empty.

**Category F — Redfoot/wiring tests:**
- `collision_system.cpp`: Added `GameOverState.cause` setting in miss branch: `HitABar` for LowBar/HighBar, `MissedABeat` for all other obstacle kinds.

**Category G — Beatmap content violations:**
- `tools/level_designer.py`: Added `fill_max_gaps`, `clean_gap_one_early`, `fix_medium_lanepush_window`, `rebalance_medium_shapes` passes.
- Regenerated all 3 beatmaps to pass: max_gap, shape_gap, gap_one_readability, medium_balance, difficulty_ramp tests.

## Learnings
- Float accumulation drift: `1.0f - 5 * 0.2f ≠ 0.0f` in IEEE 754. Snap to zero when `< 1e-6f` after any energy drain. `> 0.0f` clamp is insufficient; must use epsilon threshold.
- `entt::view` is NOT safe to modify (emplace/destroy) entities during `view.each()`. Collect to-destroy list first, then destroy after iteration.
- `window_scale_for_tier` has been inverted (issue #223): Perfect=0.5, Good=0.75, Ok=1.0, Bad=1.0. The collision_system applies `window_start -= remaining * (1-scale)` only when `scale < 1.0`.
- `make_registry()` must include all context singletons that production code accesses via `reg.ctx().get<T>()` (not just `find<T>()`). Missing context items cause SIGABRT via EnTT dense_map assert.
- `cleanup_system` should only miss-tag entities that have the `Obstacle` component (scoreable). Structural entities with just `ObstacleTag` should be destroyed immediately.


### 2026-04-26 — magic_enum enum refactor test update

**Task:** Update/add tests for the DECLARE_ENUM → magic_enum refactor applied by Keaton.

**Key findings:**
- Keaton's refactor was already applied in the working tree before this session started: `audio.h`, `player.h`, `obstacle.h`, `rhythm.h` all converted from X-macro pattern to `magic_enum` (`magic_enum::enum_name`, `magic_enum::enum_count`).
- `SFX::COUNT` sentinel was **removed** from the `SFX` enum. Count is now via `magic_enum::enum_count<SFX>()`. `SFXBank::SFX_COUNT` and `sfx_bank.cpp::SFX_COUNT` both updated.
- `audio.h` already contains two static_asserts: `enum_count<SFX>() == 10` and `ShapeShift == 0`.
- `test_audio_system.cpp` was gated out of the build via the CMakeLists EXCLUDE filter (reason: "symbols not yet merged").

**Deliverables:**
1. **CMakeLists.txt** — removed `test_audio_system` from the exclusion regex; file is now compiled and linked.
2. **tests/test_audio_system.cpp** — added `kAllSfx[]` array (all 10 real SFX values, no COUNT), `kSfxCount` constant, and two static_asserts: zero-based guard (`ShapeShift == 0`) + count sync guard (`magic_enum::enum_count<SFX>() == kSfxCount`). Replaced `static_cast<SFX>(i % static_cast<int>(SFX::COUNT))` cycle with `kAllSfx[i % kSfxCount]`.
3. **tests/test_helpers_and_functions.cpp** — extended `ToString: ObstacleKind covers all kinds` to include `LanePushLeft` and `LanePushRight` (were in the enum but missing from the test after the 8-enumerator expansion).

**Test results:**
- Focused: `[ToString],[audio]` → 7 test cases, 26 assertions, all pass
- Full suite: 657 test cases, 2064 assertions, all pass

**Guards in place:**
- `static_assert(magic_enum::enum_count<SFX>() == kSfxCount)` in `test_audio_system.cpp` — breaks build if SFX enum diverges from explicit test array
- `static_assert(SFX::ShapeShift == 0)` in both header and test — enforces zero-based contract
- `kAllSfx[i % kSfxCount]` — queue-capacity test now cycles only real SFX values, never an invalid cast

**Residual note:** `SFX::COUNT` is fully gone from the codebase after Keaton's refactor. Any future use of the sentinel pattern would need to use `magic_enum::enum_count<SFX>()` or `SFXBank::SFX_COUNT` instead.

### 2026-04-26 — PR #43 regression test suite

**Task:** Write regression tests for all 6 unresolved PR #43 review threads.

**Files created/modified:**
- `tests/test_pr43_regression.cpp` — new (350 lines), covers all 6 themes (14 test cases)
- `tests/test_level_select_system.cpp` — 3 new test cases for Theme 3 (level select diff-Y same-tick)
- `app/game_loop.cpp` — added `reg.storage<ObstacleChildren>()` before on_destroy connection

**Key findings:**

1. **All 7 fixes were already applied** in commit `d90abf9` ("fix: resolve all PR #43 review issues") — tests confirm fixes work.

2. **EnTT `destroy()` reverse pool iteration bug (found during test investigation):**
   - `entt::registry::destroy(entity)` iterates component pools in **reverse insertion order** (line 545 of registry.hpp).
   - If `ObstacleChildren` pool is inserted after `ObstacleTag` pool (which happens in production because `on_destroy<ObstacleTag>` is connected before any obstacle is spawned), then `ObstacleChildren` is removed first during `destroy()`, before the `on_destroy<ObstacleTag>` signal fires.
   - Result: `on_obstacle_destroy` calls `try_get<ObstacleChildren>()` → null → skips child cleanup → MeshChild entities become zombies → camera_system then does `reg.get<Position>(mc.parent)` on a destroyed entity → UB.
   - **Fix:** Prime the `ObstacleChildren` pool with `reg.storage<ObstacleChildren>()` before connecting `on_destroy<ObstacleTag>`. This gives ObstacleChildren a lower pool index, so it survives until after the signal fires.

3. **Test pattern for signal + pool ordering:** Use `make_obs_registry()` helper that primes `ObstacleChildren` then connects signal — tests both the function logic AND the correct setup pattern.

4. **Catch2 + EnTT null comparisons** always require explicit parentheses: `REQUIRE((e != entt::null))` — without them, template expression SFINAE fails on `ExprLhs<entt::entity>`.

5. **Theme 4 (EXIT_TOP):** After the fix in ui_button_spawner.h, Confirm half_h = (EXIT_TOP - 1)/2 = 524.5 → `pos.y = 524.5, half_h = 524.5`, covering y ∈ [0, 1049]. Exit starts at EXIT_TOP = 1050. Regions are non-overlapping.

---

### 2026-05-17 — Review: Redfoot #251 popup_display_system one-shot formatting

**Decision: APPROVED — #251 can be closed.**

**What was reviewed:** Commit `fbf0297` (`perf(popup_display_system): format static text once at spawn (#251)`).

**Behavioral verification:**

- `init_popup_display()` is a new free function in `popup_display_system.cpp` that formats text, font size, and base RGB exactly once at spawn. It is declared in `all_systems.h` under the Phase 6.5 block with a concise comment. Scope is appropriate — it is not a member of any class and creates no ownership creep.
- `scoring_system.cpp` (lines 141–146) calls `init_popup_display()` at popup-spawn time, then `reg.emplace<PopupDisplay>()` with the pre-filled struct. No `emplace_or_replace` in scoring_system.
- `popup_display_system` iterates `<PopupDisplay, Color, Lifetime>` and **only** mutates `pd.a` per-frame via the alpha_ratio formula. No `snprintf`, no `switch`, no `emplace_or_replace` anywhere in the tick path.
- Alpha fade formula: `pd.a = static_cast<uint8_t>(col.a * (remaining / max_time))` with clamping — identical semantics to the prior implementation.

**Test coverage:**

| Test | Coverage | Pass |
|------|----------|------|
| Perfect/Good/Ok/Bad grade paths | text + font_size correct | ✅ |
| nullopt timing_tier | numeric string (e.g. "150") | ✅ |
| alpha at half lifetime | pd.a == 127 | ✅ |
| static text survives multiple ticks (sentinel) | `[issue251]` | ✅ |
| storage size unchanged after multiple ticks | `[issue251]` | ✅ |
| works after ScorePopup removed | `[issue251]` | ✅ |
| `init_popup_display` unit: grade + RGB copy | `[issue251]` | ✅ |
| `init_popup_display` unit: numeric path | `[issue251]` | ✅ |

**Test run:** `[popup_display]` → 11 test cases, 33 assertions, all pass. `[issue251]` → 5 test cases, 16 assertions, all pass. Pre-existing 4 failures (`test_collision_system.cpp`, `test_pr43_regression.cpp`) are documented in decisions.md and unrelated to #251.

**No gaps found.** All checklist items from the issue spec are covered: grade/numeric paths, alpha fade, no per-frame storage churn, static text surviving multiple ticks, behavior after `ScorePopup` removal, and direct `init_popup_display` unit tests. API scoping in `all_systems.h` is clean.

### PR #43 — Windows beat_log test failures (c6ca0e8)

**Task:** Investigate and fix Windows-only failures in `test_beat_log_system.cpp` (lines 98, 110, 164 all expanding to `-1 == N`).

**Root cause:** `make_open_log()` test helper opened `/dev/null` — which does not exist on Windows. `std::fopen("/dev/null", "w")` returns `nullptr` on Windows, so `beat_log_system` bails immediately at the `!log->file` guard, leaving `last_logged_beat` at its initial `-1`. All three failing tests use this helper.

**Fix:** `#ifdef _WIN32` guard in the helper — use `"NUL"` on Windows, `"/dev/null"` on POSIX. One-file, six-line change in `tests/test_beat_log_system.cpp`.

**Verification:**
- macOS local run: `[beat_log]` → 11 test cases, 13 assertions, all pass
- Fix is additive (no logic change, no test weakening)
- Commit: c6ca0e8 on `user/yashasg/ecs_refactor`

### 2026-04-27 — EnTT Input Model Guardrails (PRE-IMPLEMENTATION GUIDANCE from Keyser)

**Cross-agent context:** Keyser published pre-implementation guardrails for dispatcher-based input refactor. You are Baer (Testing), identified as the validation gate owner.

**Your role:** Implement validation tests for guardrails R1–R7, with specific focus on:

**R7 — Stale event discard across phase transitions (PRIORITY TEST):**
- Add a Catch2 test `[entt_input][stale_event_discard]` that:
  1. Enqueues GoEvents/ButtonPressEvents while game is in Playing phase
  2. Transitions to Paused/MenuOpen phase
  3. `player_input_system` returns early (events NOT drained)
  4. Next frame's `input_system` calls `disp.clear<GoEvent/ButtonPressEvent>()`
  5. Verify events were discarded, not replayed (prevent #213 regression)

**No-replay validation (R1 + R2):**
- Verify multi-consumer ordering in registration order (R1)
- Test that `update()` on empty queue = no-op on sub-ticks (R2 + #213 invariant)
- Verify test_player_system doesn't emit >8 events/frame (R2 cap check)

**Supporting tests (non-critical):**
- R3: Document clear-vs-update semantics in a comment test
- R4: Verify listener registry access pattern (payload/lambda, no naked global)
- R5: Cannot be tested directly (UB at runtime); document in code comment
- R6: Lint-only guarantee; document trigger prohibition in `input_system`/`player_input_system`

**Full decision:** `.squad/decisions.md` (EnTT Input Model Guardrails section)
**Orchestration log:** `.squad/orchestration-log/2026-04-27T19-09-18Z-keyser-entt-input-guardrails.md`
**Unblock condition:** Keaton completes migration step 1 (dispatcher placement)
