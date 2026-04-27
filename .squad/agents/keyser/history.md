# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Learnings

<!-- Append learnings below -->

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
