## Learnings

<!-- Append learnings below -->

### 2026-04-27 — Archetypes Folder Move Validation (Issue #344 / ecs_refactor)

**Status:** VALIDATED — Build + full test suite green.

**Task:** Validate the in-progress move of `app/systems/obstacle_archetypes.{h,cpp}` → `app/archetypes/obstacle_archetypes.{h,cpp}` without modifying source files. Validation only.

**What changed in working tree:**
- `app/systems/obstacle_archetypes.h` and `.cpp` deleted
- `app/archetypes/obstacle_archetypes.h` and `.cpp` added (untracked)
- `CMakeLists.txt`: `ARCHETYPE_SOURCES` glob added at line 90 (`file(GLOB ARCHETYPE_SOURCES app/archetypes/*.cpp)`), included in `shapeshifter_lib` at line 102
- `app/systems/beat_scheduler_system.cpp` and `obstacle_spawn_system.cpp`: include path updated to `"archetypes/obstacle_archetypes.h"`
- `tests/test_obstacle_archetypes.cpp`: include updated to `"archetypes/obstacle_archetypes.h"`

**Include path correctness:** `shapeshifter_lib` and `shapeshifter_tests` both have `${CMAKE_CURRENT_SOURCE_DIR}/app` as include root, so `"archetypes/obstacle_archetypes.h"` resolves to `app/archetypes/obstacle_archetypes.h` correctly for all consumers.

**Validation commands:**
```bash
cmake -B build -S . -Wno-dev          # configure (success)
cmake --build build --target shapeshifter_tests   # build (success, zero warnings from project code; one ld duplicate-lib linker note unrelated to this move)
./build/shapeshifter_tests "[archetype]" --reporter compact   # 11 tests, 64 assertions, all pass
./build/shapeshifter_tests "~[bench]" --reporter compact      # 810 test cases, 2748 assertions, all pass
```

**Verdict:** The archetype move is structurally correct. Compilation clean (zero new warnings). All 11 `[archetype]`-tagged factory tests pass. Full suite passes with no regressions. The `ld: warning: ignoring duplicate libraries: 'vcpkg_installed/arm64-osx/lib/libraylib.a'` linker note is pre-existing and unrelated to this move.

**P2/P3 status:** Not started. Stopping here per charter — only validation requested.

### 2026-04-27 — Parallel ECS/EnTT Audit (user/yashasg/ecs_refactor branch)

**Status:** AUDIT COMPLETE — Read-only test coverage audit with Keyser, Keaton, McManus, Redfoot.

**Verdict:** Coverage gaps — High-signal gaps only. All "mostly covered" areas preserved.

**P1 Gaps:**
- **R7 Stale event discard** — No test. Events queued before phase transition must not replay after (explicitly assigned in decisions.md). **Test:** Enqueue GoEvent → trigger transition → verify no delivery to post-transition player state.
- **Dispatcher singleton** — test_components.cpp asserts only 6 of ~17 singletons; dispatcher NOT checked. Any bare registry call to gesture_routing/hit_test/player_input systems hard-crashes. **Test:** Extend singleton assertion; add null-registry crash-guard for at least one system. (Owner: Baer.)

**P2 Gaps:**
- **miss_detection idempotency** — No test verifies collect-then-emplace is safe during exclude<ScoredTag> iteration. **Test:** Run on N obstacles past DESTROY_Y; verify MissTag count == N and ScoredTag count == N; run again, verify no second tag.
- **make_registry completeness** — Only 6 singletons asserted; ~11 others (SongState, EnergyState, BeatMap, etc.) emplaced but unchecked. **Test:** Extend to call ctx().get<T>() or find<T>() for every singleton.

**P3 Gaps:**
- **on_construct<> signals platform-gated** — All in `#ifdef PLATFORM_DESKTOP` — invisible on Linux CI. **Test:** Port at least one on_construct<ObstacleTag> test to non-gated file.

**Coverage to Preserve:** test_entt_dispatcher_contract.cpp (enqueue/update deferred + drain=no-replay), test_input_pipeline_behavior.cpp (same-frame delivery, #213 no-replay), test_event_queue.cpp (dispatcher listener hygiene), test_components.cpp [phase_mask] (enum_as_bitmask), test_pr43_regression.cpp on_obstacle_destroy (pool-insertion-order), test_lifecycle.cpp (ctx null-safe), test_ui_element_schema.cpp (_hs hashed dispatch), test_high_score_persistence.cpp (FNV-1a collision-free), test_world_systems.cpp [#242] (collect-then-remove).

**Platform risks:** PLATFORM_DESKTOP gates 12 tests (test_test_player_system.cpp invisible on Linux), WASM has zero tests, one #ifndef PLATFORM_WEB gate in test_pr43_regression.cpp, static buffer in cleanup_system.cpp (data race risk if parallelized), test_beat_log_system.cpp #ifdef _WIN32 path handling.

**Orchestration log:** `.squad/orchestration-log/2026-04-27T22-30-13Z-baer.md`

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


