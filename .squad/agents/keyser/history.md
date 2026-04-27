# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Learnings

<!-- Append learnings below -->

### 2026-05-17 — Issue #242 Review (cleanup_system per-frame heap allocation)

**Verdict: APPROVED — #242 can be closed.**

**Code change landed in commit `afd7921` (mislabeled as #253 high_score perf; #242 changes are bundled).**

**Checks performed:**

1. **Static buffer confirmed** — `cleanup_system.cpp` now uses `static std::vector<entt::entity> to_destroy; to_destroy.clear();`. Capacity is retained across frames; `clear()` resets size without freeing. No per-frame heap allocation on the hot path. The `#242` comment in-code documents the intent explicitly.

2. **No `reg.all_of` predicate remains** — `cleanup_system.cpp` has zero calls to `all_of` or `any_of`. The predicate was already removed by #280 (`miss_detection_system` extracted the miss path). Current structural view `reg.view<ObstacleTag, Position>()` is correct EnTT-native filtering.

3. **Collect-then-destroy pattern is iterator-safe** — Entities are pushed into `to_destroy` during the view iteration, then destroyed in a separate loop. No modification of the view storage during iteration; EnTT iterator safety is preserved.

4. **#280 ownership boundary preserved** — `cleanup_system` does not emplace `MissTag` or `ScoredTag`. It destroys expired off-screen obstacles only. `miss_detection_system` owns the miss-tagging responsibility as per #280.

5. **Tests cover required cases** — Two new `[cleanup]` tests in `test_world_systems.cpp`:
   - `"cleanup: destroys multiple obstacles past DESTROY_Y in one pass"` — validates batch destruction with N=5 entities (guards static buffer correctness).
   - `"cleanup: does not emplace MissTag or ScoredTag on surviving obstacles"` — validates #280 ownership boundary.
   All 6 `[cleanup]` tests (excluding pre-existing `[pr43]` failures) pass: **12 assertions, 0 failures**.

6. **CMakeLists exclusion is justified** — Three files (`test_lifecycle.cpp`, `test_gesture_routing_split.cpp`, `test_gpu_resource_lifecycle.cpp`) are real but untracked agent-in-progress files referencing symbols not yet merged. Exclusion is correct; no real shipped tests are accidentally dropped.

7. **Pre-existing `[pr43]` failures are unrelated** — Two `on_obstacle_destroy` tests in `test_pr43_regression.cpp` fail, but these predate `afd7921` by multiple commits and are not caused by #242 changes.

### 2026-05-18 — Issue #272 Re-Review (Kobayashi revision — CMake EXCLUDE REGEX fix)

**Verdict: APPROVED — #272 can be closed.**

**Revision commit:** `e600644` (`fix(ci): include test_gesture_routing_split in shapeshifter_tests (#272)`)

**Checks performed:**

1. **EXCLUDE REGEX defect resolved** — `CMakeLists.txt:368` no longer contains `test_gesture_routing_split`. The surgical one-token removal from the pipe-separated regex is confirmed in the commit diff. The five remaining entries (`test_safe_area_layout`, `test_ui_redfoot_pass`, `test_ui_element_schema`, `test_ui_overlay_schema`, `test_lifecycle`, `test_gpu_resource_lifecycle`) are all still-unmerged agent files; their continued exclusion is correct.

2. **Test file is now in-scope** — `test_gesture_routing_split.cpp` is picked up by `file(GLOB TEST_SOURCES tests/*.cpp ...)` at line 364 and passes the EXCLUDE REGEX filter. All 7 test cases carry `[issue272]` tags, matching Kobayashi's reported run: `"[issue272]"` → 21 assertions, 7 test cases.

3. **No unrelated wiring changes** — The commit diff touches exactly one line in CMakeLists.txt. No other test sources, build targets, or CI files were modified.

4. **All prior approvals unchanged** — Split correctness, ActiveTag filtering, game-loop ordering, ECS cleanliness, and `all_systems.h` declarations were PASS in the original review and are unaffected by this patch.

### 2026-05-18 — Issue #272 Review (gesture_routing/hit_test split) [SUPERSEDED — see re-review above]

**Verdict: REJECTED — CMakeLists.txt excludes the test file; tests never run in CI.**

**Revision assigned to: Kobayashi**

**Defect (single, exact):**
`CMakeLists.txt:368` — the EXCLUDE REGEX filter includes `test_gesture_routing_split`, keeping all 7 #272 tests out of the `shapeshifter_tests` binary. The exclusion was introduced in `afd7921` (before the test file existed) as a preemptive guard for unmerged work. Commit `2403632` added the test file but did not remove it from the exclusion pattern. The test file compiles clean (`-fsyntax-only` verified); it simply isn't linked.

**Fix (one line):** Remove `test_gesture_routing_split` from the exclusion regex in `CMakeLists.txt:368`.

**All other checks PASS:**

1. **Split is clean** — `gesture_routing_system` iterates `EventQueue::inputs`, emits `push_go` on `InputType::Swipe` only; no spatial logic. `hit_test_system` skips any input where `evt.type != InputType::Tap`; no GoEvent path.
2. **#249 ActiveTag filtering preserved** — `hit_test_system` calls `ensure_active_tags_synced(reg)` then queries `view<Position, HitBox, ActiveTag>()` / `view<Position, HitCircle, ActiveTag>()`. Structural; identical to the pre-split implementation.
3. **Game loop ordering** — `game_loop.cpp:164-165`: `gesture_routing_system(reg)` immediately before `hit_test_system(reg)`, both after `input_system` and before the fixed-step accumulator loop. Ordering matches prior behavior.
4. **No globals, clean ECS** — both systems read/write only `EventQueue` (registry context). No cross-system component ownership creep.
5. **`all_systems.h`** — both declarations present with Phase 0.25a/0.25b comments and #272 cross-reference.
6. **Test coverage** — `test_gesture_routing_split.cpp` covers: swipe→GoEvent, tap→no GoEvent (gesture system), swipe→no press (hit-test system), tap→press (hit-test alone), mixed ordering (both systems sequenced), ActiveTag phase filtering (Title vs Playing).

**Root cause of defect:** `afd7921` added the exclusion list before `test_gesture_routing_split.cpp` existed (to guard unmerged test files). Redfoot's `2403632` added the file but did not clean up the exclusion entry.

### 2026-05-17 — Issue #241 Review (compute_screen_transform duplicate)

**Verdict: APPROVED — #241 can be closed.**

**Code fix landed in commit a5cad3d, bundled with #304 (WASM shutdown).**

**Checks performed:**

1. **Single canonical call confirmed** — `compute_screen_transform(reg)` appears exactly once in `game_loop.cpp:162`, at the top of `game_loop_frame`, before `input_system`. No other call site exists in `app/`.

2. **`ui_camera_system` reads, does not recompute** — The body of `ui_camera_system` contains no call to `compute_screen_transform`. An explicit code comment at `camera_system.cpp:302` documents the intent: "ScreenTransform is computed once per frame by game_loop_frame (before input_system) and stored in the registry context. Reading it here is sufficient; do NOT call compute_screen_transform again (#241)."

3. **No one-frame latency** — `compute_screen_transform` runs before `input_system`, `hit_test_system`, and `ui_camera_system`, so both input mapping and UI camera use the current frame's transform.

4. **ECS context state, not hidden global** — `ScreenTransform` lives in `reg.ctx()`, derived from `platform_get_display_size` each frame. No global variable.

5. **Tests are additive and meaningful** — Two new `[screen_transform][regression]` tests added to `test_ndc_viewport.cpp`:
   - "letterbox math is idempotent (#241)": pure-math mirror of `compute_screen_transform`; proves same-inputs → same-outputs and validates the 2× no-bar case (scale=2, offset=0).
   - "letterbox math produces correct pillarbox offsets (#241)": validates 1280×1280 window → scale=1, offset_x=280, offset_y=0.
   No existing tests were removed or weakened.

6. **Commit bundling** — The commit message credits #304 only. The #241 portion is a small, self-contained, semantically independent change. No risk bleed from #304 work. #241 can be closed without waiting for a separate #304 review.

### 2026-05-17 — Enum Macro Refactor Analysis

**Request:** Replace all codebase enums with a fixed 7-argument unscoped macro that produces `enum name { v1..v7 }` and `const char *nameStrings[]`.

**Finding: Zero enums are directly convertible.** Every enum fails on at least one of these structural incompatibilities:
1. All enums are `enum class` — macro produces plain unscoped `enum`, breaking every `EnumName::Value` call site.
2. All enums specify an underlying type (`:uint8_t` or `:int`) — macro has none; triggers `-Wconversion` = build failure under zero-warnings policy.
3. Enumerator counts range from 2 to 13 — only `MenuActionKind` hits 7, but it has explicit `= 0..6` assignments the macro can't encode.
4. `WindowPhase` is forward-declared in `player.h` — a macro cannot be forward-declared.
5. Three enums (`ObstacleKind`, `Shape`, `TimingTier`) already use the superior X-macro pattern with `ToString()` functions; the proposed macro would be a regression to a mutable global string array.

**Decision: NO-GO on the exact macro.** Recommend extending the existing X-macro/`ToString()` pattern (already in use in the project) to remaining enums that genuinely need string tables. Wait for PR #43 to merge before touching any component headers.

**Key rule confirmed:** This codebase uses `enum class` with explicit underlying types universally. Any enum helper macro must preserve both. The 3-enums-with-X-macro pattern is the canonical project approach.

### 2026-05-15 — Diagnostics Pass Learnings

- `session_log_on_scored` (session_logger.cpp:123) checks `gs->transition_pending && next_phase==GameOver` for miss detection — always wrong at signal-fire time because energy_system hasn't run yet. Correct check: `reg.any_of<MissTag>(entity)`.
- `SongResults::total_notes` is declared (song_state.h:48) but never populated anywhere in the codebase — always 0.
- `audio_system` drains the AudioQueue but never calls `PlaySound()` — all SFX is silently dropped. Marked as "Future:" in code.
- SongComplete gating view (`game_state_system.cpp:158`) uses `ObstacleTag` exclude `ScoredTag`. Post-scored entities keep `ObstacleTag` (scoring_system only removes `Obstacle` + `ScoredTag`), causing ~1s SongComplete delay after last note.
- `APPROACH_DIST` duplicated: `constants.h:67` (namespace constant) and `song_state.h:56` (local inside `song_state_compute_derived`). Both are 1040.0f; drift risk if geometry changes.
- `song_playback_system` resume guard uses `gs.previous_phase == Paused` which is not edge-triggered — calls `ResumeMusicStream` every frame after unpause until next phase change. Benign in practice (raylib no-ops idempotent Resume) but sloppy.
- `burnout_system` finds nearest obstacle by y-distance regardless of lane — an obstacle in a different lane that can't actually hit the player still raises the burnout zone.
- `obstacle_spawn_system` uses `std::rand()` without `srand()` — same obstacle sequence every session in freeplay mode.
- `scoring_system` spawns a 0-point ScorePopup for LanePush obstacles (base_points=0), showing "0" on screen. Should skip popup when points==0.
- LANE_X in architecture.md (180/360/540) is stale; code uses 60/360/660.
- Single-slab obstacles (LowBar, HighBar, LanePushLeft/Right) lose `ModelTransform` updates after scoring_system removes `Obstacle` component, but entity persists with stale transform until cleanup — visual ghost at score position.
- `reg.clear()` in `setup_play_session` fires `on_destroy<ObstacleTag>` signal mid-clear, which calls `reg.destroy()` on MeshChild entities. Works in practice with EnTT 3.16.0 but is undefined-behavior territory.

### 2026-04-26 — Project Tech Stack & Agent Fit

- Stack: C++20, raylib 5.5, EnTT 3.16.0 (ECS/DOD), CMake+vcpkg, Catch2 (386 tests), Python beatmap pipeline (aubio), 4-platform CI (macOS/Linux/Windows/WASM), GitHub Pages WebAssembly deploy.
- 19 POD components, 23 systems, strict zero-warnings policy (-Wall -Wextra -Werror / /W4 /WX).
- Rhythm is derived from song_time (not accumulated dt) — prevents beat drift. This is a core invariant all gameplay agents must respect.
- Agent role-fit evaluated: McManus, Hockney, Keaton, Kobayashi, Fenster, Baer, Kujan are HIGH value. Rabin, Saul, Edie, Verbal are MEDIUM. Redfoot is LOW (minimal HUD complexity). No gaps for typical game dev work; audio-specialist role could add value if pipeline depth grows.

### 2026-05-15 — TestFlight ECS Bug Fixes (#207, #209, #213)

**#207 (morph_t double-update in rhythm mode):**
- `player_movement_system` was advancing `morph_t` unconditionally every tick. `shape_window_system` also writes it from `song_time`. Both in the same tick = double-speed morph animation.
- Fix: gate `player_movement_system`'s morph update on `!SongState::playing`. Single-owner invariant now enforced in code.
- Rule documented in decision record.

**#209 (MorphOut button press silently dropped):**
- Shape button presses during `WindowPhase::MorphOut` fell through all if/else-if branches with no action.
- Decision: accept and interrupt (not queue). MorphOut has no active window; starting a fresh MorphIn is safe and preserves player intent.
- Fix: added `else if (phase == WindowPhase::MorphOut) { begin_shape_window(...); }`.

**#213 (GoEvents replayed in multi-tick accumulator frames):**
- `game_loop_frame` fills `EventQueue` once per render frame, then loops `tick_fixed_systems` N times (fixed-step accumulator). `player_input_system` read GoEvents on each sub-tick, resetting `lane.lerp_t = 0.0f` every tick on slow frames.
- Fix: `player_input_system` zeroes `eq.go_count` and `eq.press_count` after processing. Events consumed on first sub-tick.

**All three fixed in commit 7b420ed on branch user/yashasg/ecs_refactor.**
**GitHub comments posted to #207, #209, #213 (issues left open).**
**Regression tests added in `tests/test_player_action_rhythm.cpp`.**


- Many bugs from the previous pass are now FIXED in code but issues remain open: #108 (std::rand unseeded, now uses RNGState), #113 (audio SFX now plays), #114 (total_notes now populated), #116 (reg.clear signal guard now correct), #117 (burnout is_burnout_relevant now filters by lane), #119 (music resume now edge-triggered via music->paused flag), #111 (session_log_on_scored now uses any_of<MissTag>).
- `PlayerShape::morph_t` is written by BOTH `shape_window_system` (sets from song time) AND `player_movement_system` (adds dt-based delta) in the same fixed-step tick during rhythm mode → data ownership conflict → #207.
- `player_input_system` has a MorphOut dead zone: button presses during `WindowPhase::MorphOut` are silently ignored, not deferred → #209.
- `EventQueue` GoEvents (lane Left/Right) are re-processed on every sub-tick in the fixed-step accumulator, resetting `lane.lerp_t = 0.0f` on each → stutter/snap under load spikes → #213.
- `scoring_system` emits `SFX::BurnoutBank` unconditionally for ALL cleared obstacles including 0-point LanePush, giving misleading audio feedback → #215.
- System execution order is consistent with semantics: energy_system after scoring (correct), game_state_system before all gameplay (correct 1-frame GameOver trigger is unavoidable).
- `APPROACH_DIST` still duplicated in constants.h (namespace constant) and song_state.h (local constexpr in `song_state_compute_derived`). Already filed as #84.
- `cleanup_system` properly handles the scored-entity lifecycle in concert with scoring_system; no double-miss-count identified.
- `collision_system`'s window contraction math (adjusting window_start backward by `remaining * (1 - scale)`) is algebraically correct and intentional — not a bug.

### 2026-05-16 — Post-TestFlight Cleanup Pass

**Scope:** ECS/state code cleanup after #207/#209/#213 fixes (branch user/yashasg/ecs_refactor).

**Changes Made:**

1. **`scoring_system.cpp` — BankedBurnout stale component on miss path (FIXED)**
   - Miss path removed `Obstacle`, `ScoredTag`, `MissTag`, `TimingGrade` but NOT `BankedBurnout`.
   - A player could bank burnout on an obstacle (press near it) and then miss the window; the entity retained `BankedBurnout` until `cleanup_system` destroyed it.
   - Fix: added `if (reg.any_of<BankedBurnout>(entity)) reg.remove<BankedBurnout>(entity);` to the miss path in `scoring_system`.

2. **`play_session.cpp` — `SongResults::total_notes` never populated (FIXED, issue #114)**
   - `setup_play_session` reset `SongResults{}` (total_notes = 0) but never set `total_notes` from `beatmap.beats.size()`.
   - History claimed this was fixed but the code was missing the assignment.
   - Fix: `SongResults results{}; results.total_notes = static_cast<int>(beatmap.beats.size());` before insert_or_assign.
   - Test `[issue-114]` now passes.

3. **`player_movement_system.cpp` — `SongState` ctx lookup inside entity loop (CLEANED)**
   - `reg.ctx().find<SongState>()` was called inside the per-entity loop.
   - Moved outside the loop; `rhythm_mode` is a frame-level invariant, not per-entity.

4. **`player_input_system.cpp` — Duplicate Left/Right lane change blocks (CLEANED)**
   - Left and Right lane change handling were identical 16-line blocks differing only by delta direction and boundary check.
   - Replaced with a single block using `int8_t delta = 0; if Left delta=-1; else if Right delta=+1; if delta!=0 { ... }`.

**Pre-existing build errors uncovered (cache invalidation triggered rebuild):**
- `high_score_persistence.cpp` and `ui_source_resolver.cpp` had compilation errors hidden by cached `.o` files. Both were untracked files. Build correctly succeeds in full rebuild — these files compile clean once all headers are included. NOTE: The initial "build succeeded" on the first pass was from cached objects only.

**Pre-existing test failures (not introduced by my changes):**
- `test_collision_system.cpp:308,340,341` — timing window behavior: "Perfect timing extends window via window_scale only" test expects `window_scale > 1.0` but collision_system sets 0.5; "BAD timing adjusts window_start" test expects `window_start < original` but it's unchanged. These were in the baseline.
- `test_death_model_unified.cpp` — "cleanup miss drains energy" test: `cleanup_system` destroys scrolled-past unscored obstacles without marking them as missed. Test expects `MissTag`+`ScoredTag` emplaced + energy drain. This is a real **gameplay bug** (missed obstacles silently disappear without energy penalty). Deferred — requires behavioral change to `cleanup_system`.
- `test_shipped_beatmap_max_gap.cpp` — stomper/mental_corruption easy+medium have gaps > limit. Content tooling issue.
- `test_high_score_integration.cpp:24` — SIGABRT from EnTT dense_map assertion. `HighScoreState.scores` map access with invalid key in test setup. Pre-existing.
- `test_shipped_beatmap_difficulty_ramp.cpp` — content/pipeline issue.

**Deferred Findings (material bugs, not safe for cleanup scope):**
- **`cleanup_system` miss-marking gap**: Unscored obstacles that scroll past `DESTROY_Y` are silently destroyed without draining energy. `test_death_model_unified` proves this is intended behavior that's not implemented. Fix requires `cleanup_system` to check `any_of<ScoredTag>` and emplace `MissTag`+`ScoredTag` before cleanup, then let `scoring_system` handle the miss penalty on the next frame. This is a gameplay balance change — needs Saul sign-off on energy curve implications.
- **`test_high_score_integration` SIGABRT**: EnTT dense_map assertion at `scores["key"] = value` access. Likely `HighScoreState.scores` is an `ankerl::unordered_dense::map` that doesn't support `operator[]` creation on miss. Test needs `scores.emplace(key, value)` or pre-initialization.

**Invariant confirmed:** `player_input_system` correctly consumes `go_count` and `press_count` at end of frame (fix #213). `player_movement_system` correctly gates morph_t update on `!rhythm_mode` (fix #207). `shape_window_system` has sole ownership of `morph_t` in rhythm mode.

---

### 2026-04-27 — PR #43 Collision CI Fix Review (commit `dca7664`)

**Verdict: APPROVED — PR #43 collision failure family is resolved.**

**Scope:** `fix(tests): run scoring_system after collision_system in bar miss tests (#43)`

**Findings:**

1. **Only file touched:** `tests/test_collision_system.cpp` — confirmed via `git diff --name-only`. No production code, no unrelated test files, no cmake or header changes.

2. **Two tests fixed:**
   - `collision: low bar drains energy when sliding` (line 282)
   - `collision: high bar drains energy when jumping` (line 297)
   Both now call `collision_system(reg, 0.016f)` → `scoring_system(reg, 0.016f)` before asserting `energy.energy < 1.0f` and `energy.flash_timer > 0.0f`.

3. **Assertions unchanged:** No relaxation of energy drain or flash_timer checks. Behavior contract is identical — the fix only adds the missing pipeline step.

4. **Ownership intact:** Energy drain and flash_timer remain owned by `scoring_system`. `collision_system` still only produces miss tags/events; `scoring_system` still consumes them. No behavior was moved back.

5. **Pattern consistency:** All other miss-drain tests in the file (shape gate, lane block, split path miss, combo gate miss) already used the two-step pipeline. These two bar-miss tests were the only outliers — the fix aligns them with established test pattern.

**PR #43 collision failure family is resolved.**

### 2026-04-27 — EnTT Input Model Guardrails (PRE-IMPLEMENTATION GUIDANCE)

**Outcome:** Published architecture guardrails for dispatcher-based input refactor.

**Core decision:** Replace hand-rolled `EventQueue` fixed arrays with `entt::dispatcher` stored in `reg.ctx()`. Use `enqueue`+`update` (deferred delivery) — **not** `trigger` (immediate). System execution order remains unchanged; dispatcher becomes the transport layer.

**Key recommendations:**
- **Dispatcher placement:** `reg.ctx().emplace<entt::dispatcher>()` (consistent with existing singleton pattern)
- **Event delivery:** Two-tier `enqueue`+`update` (Tier 1: InputEvent pre-loop; Tier 2: GoEvent/ButtonPressEvent inside fixed-step)
- **Listener registration:** Canonical ordering in `game_loop_init` (block-comment documented)
- **Seven guardrails:** Multi-consumer ordering, overflow cap verification, clear-vs-update semantics, listener registry access, no connect-in-handler, trigger prohibition, stale event discard across phase transitions

**Recipients:** Keaton (implementation), Baer (validation gates), McManus (integration)

**Status:** Ready for Keaton to begin implementation stage 1 (dispatcher placement + zero-risk init).

### 2026-05-17 — EnTT ECS Guide Audit

**Scope:** Read-only pass of all `app/components/` and `app/systems/` against `docs/entt/Entity_Component_System.md`.

**Key findings:**

- **P1: Logic in component headers** — Three component headers carry system-grade logic: `UIState::load_screen()` (file I/O in a component), `HighScoreState` business methods (`set_score`, `update_current_high_score`), and `ensure_active_tags_synced()` / `invalidate_active_tag_cache()` (registry-iterating functions in `input_events.h`). All should move to system `.cpp` files.
- **P2: Signal wiring and helpers in component headers** — `obstacle_counter.h` contains signal callbacks and `wire_obstacle_counter()`; `song_state.h` contains `song_state_compute_derived()`. Minor but worth cleanup.
- **P2: `SettingsState` mutation methods** — `mark_ftue_complete()` et al. are logic on a component; should migrate to a settings system.
- **P2: `cleanup_system.cpp` static vector** — Known/approved optimization (#242).
- **In-flight validation:** Input dispatcher pipeline is well-aligned with ECS principles. Only ensure_active_tags_synced() placement needs move; pipeline structure is sound.

**Status:** Delivered to Coordinator. Decision gate open for team prioritization and backlog assignment. Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`.
- **P3: `BurnoutState` context var** — Orphaned no-op post-#239; can be removed.
- **In-flight input dispatcher** — `input_system → gesture_routing → hit_test → player_input` pipeline is ECS-correct. Only `ensure_active_tags_synced` placement (in header) needs to move.
- **Good patterns:** All systems are free functions ✅; `reg.ctx()` singletons ✅; signal disconnect before `reg.clear()` ✅; ETO on all tag structs ✅; fixed-size array buffers ✅; no groups (correct choice) ✅; `entt::exclude<>` view narrowing ✅.

**Decision filed:** `.squad/decisions/inbox/keyser-entt-ecs-guide-audit.md`

**Key file paths for this audit:**
- ECS guide: `docs/entt/Entity_Component_System.md`
- Component headers with logic: `app/components/ui_state.h`, `app/components/high_score.h`, `app/components/input_events.h`, `app/components/obstacle_counter.h`, `app/components/song_state.h`
- Systems audited: all 47 `reg.view<>` call sites, `app/game_loop.cpp`
