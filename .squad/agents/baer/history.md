# Baer — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Test Engineer
- **Joined:** 2026-04-26T02:12:00.632Z

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

### 2026-04-27 — EnTT Input Model: Dispatcher Contract + Pipeline Behavior Tests

**Directive:** User directive to move input pipeline from EventQueue arrays to entt::dispatcher/signal model. Keaton owns implementation; Baer owns test coverage.

**Tests added (commit 0471598, branch user/yashasg/ecs_refactor):**

`tests/test_entt_dispatcher_contract.cpp` — 10 tests, `[entt_dispatcher]` tag:
- Pins `entt::dispatcher` v3.16.0 semantics: trigger() fires immediately, enqueue() without update() is silent, update() drains queue (no replay).
- Documents the **one-frame latency hazard**: if the GoEvent pool is registered BEFORE the InputEvent pool, then `update()` runs GoEvent pool first (empty), then InputEvent pool fires the router which enqueues GoEvent — but GoEvent pool already ran. The GoEvent is not delivered until the next `update()`. **Fix: gesture_routing must use `trigger()`, not `enqueue()`, when emitting GoEvents.** This is verified by the companion "trigger() in listener is always safe" test.
- ButtonPressEvent trigger/enqueue contract.

`tests/test_input_pipeline_behavior.cpp` — 10 tests, `[input_pipeline]` tag:
- Full pipeline: gesture_routing_system → hit_test_system → player_input_system.
- Asserts only on player component state (Lane::target, ShapeWindow::phase) — never on EventQueue internals. Survives dispatcher refactor unchanged.
- Covers: swipe→lane change same frame, tap→shape change same frame, mixed swipe+tap, wrong-phase tap (ActiveTag filter), boundary no-wrap, no-latency regression check, #213 consumption (lerp_t not reset on sub-tick 2, window_start not replayed on sub-tick 2).

**EnTT v3.16.0 dispatcher internals (learned):**
- `dispatcher_handler::publish()` snapshots `events.size()` before iterating. Events enqueued by listeners during `publish()` go into the same vector at the end, past the snapshot boundary — they are NOT processed in the same `publish()` call.
- `dispatcher::update()` iterates pools in registration order (order `sink<T>()` or `enqueue<T>()` was first called). This determines whether enqueue-during-update has zero or one-frame latency.
- `trigger()` fires listeners synchronously, bypassing the queue entirely — immune to registration order.

**Existing tests: zero regressions.** All 2390 assertions from the existing suite continue to pass (2430 total with new additions, 752 test cases excluding bench).

**Handoff note to Keaton:**
- Use `dispatcher.trigger<GoEvent>()` (not `enqueue`) inside gesture_routing's InputEvent listener.
- Register InputEvent sink before GoEvent sink in system setup to stay safe even if enqueue is used.
- `dispatcher.update()` per tick (once) satisfies #213: second sub-tick update is a no-op since the queue is drained.

### 2026-04-27 — Dispatcher Contract Tests APPROVED (Kujan review)

**Test outcome:** 40 contract + pipeline tests pass; zero warnings; dispatcher contract validated.

**Approved:** All guardrails documented in test suite accurately reflect implementation behavior.

**Findings from review:**
- Contract test comments reference outdated Keaton approach (trigger() vs enqueue()).
- Actual implementation avoids latency hazard via direct system call (not listener architecture).
- Comment should be updated to prevent future reader confusion.

**Status:** Ready for production; on-call for defensive queue clearing hardening (R7 follow-up).

### 2026-04-27 — #324 burnout dead-surface regression pass

**Task:** Test-focused regression pass on `d9be464` (Remove dead burnout ECS surface).

**Diff analyzed:** 22 files, 397 deletions / 39 insertions. Removed: `burnout.h`, `burnout_system.cpp`, `burnout_helpers.h`, `BurnoutState` singleton, `DifficultyConfig::burnout_window_scale`, `SongResults::best_burnout`, `burnout_mult` variable in `scoring_system`, constants (ZONE_*, MULT_*), `test_burnout_system.cpp`, `test_burnout_bank_on_action.cpp`.

**Behavior risks assessed:**
- All deleted tests tested dead behavior (no-op system, dead components, dead fields). Zero regression risk.
- `scoring_system.cpp`: `base_points * timing_mult * burnout_mult(1.0)` → `base_points * timing_mult`. Semantically identical; covered by existing `[scoring]` tests.
- LanePush exclusion from chain/popup: migrated from AC#4 of deleted `test_burnout_bank_on_action.cpp` to `test_scoring_extended.cpp`. Migration only covered `LanePushLeft`.

**Gap found:** `LanePushRight` had no dedicated scoring-exclusion test. The `scoring_system.cpp` condition covers both Left and Right in a single `||` branch — if the branch were narrowed to only one variant, the Left-only test would pass silently.

**Fix applied:** Added `TEST_CASE("scoring: LanePushRight excluded from chain and popup", "[scoring][lane_push]")` to `test_scoring_extended.cpp`. Mirror of the LanePushLeft test.

**Preserved coverage confirmed:**
- Energy bar: `test_energy_system.cpp` — 5 tests ✅
- Timing-graded scoring: `test_scoring_extended.cpp` + `test_scoring_system.cpp` — Perfect/Good/Bad/Ok multipliers ✅
- Chain: `test_scoring_system.cpp` — accumulation, reset, 5+ bonus ✅
- Distance bonus: `test_scoring_system.cpp` — accumulation from scroll_speed ✅
- LanePush exclusion: `test_scoring_extended.cpp` — both Left and Right ✅
- SongResults (max_chain, miss_count, etc.): multiple test files ✅

**Test run:** `[scoring][lane_push]` → 2 test cases, 8 assertions, all pass. Full suite: 2565 assertions, 792 test cases, all pass. Zero warnings.

**Commit:** `6ee912a` on `squad/324-remove-dead-burnout-surface`.

### 2026-04-27 — ECS/EnTT Test Coverage Audit (parallel subagents)

**Scope:** Read-only audit of tests/** and app/components/** and app/systems/** against docs/entt/. Dispatcher contract, collect-then-remove safety, component purity, ctx singleton init, UI layout/cache, hashed string, enum bitmask, signal/observer lifecycle, CI platform guards.

**Key file paths:**
- `tests/test_entt_dispatcher_contract.cpp` — dispatcher trigger/enqueue/update/drain, one-frame latency hazard, #213 no-replay
- `tests/test_event_queue.cpp` — GoEvent/ButtonPressEvent through ctx-stored dispatcher
- `tests/test_input_pipeline_behavior.cpp` — same-frame delivery, #213 sub-tick no-replay, mixed events, phase guard
- `tests/test_components.cpp` — GamePhaseBit bitmask, make_registry singletons (incomplete), entity creation
- `tests/test_lifecycle.cpp` — missing InputState ctx returns false (not crash)
- `tests/test_pr43_regression.cpp` — on_destroy<ObstacleTag> signal lifecycle, pool insertion order
- `tests/test_ui_element_schema.cpp` — hashed string (`_hs`) element_map lookups, rebuild-on-re-call
- `tests/test_ui_layout_cache.cpp` — HudLayout/LevelSelectLayout valid/invalid construction, integration
- `tests/test_high_score_persistence.cpp` — FNV-1a hash collision coverage (9 keys)
- `app/systems/cleanup_system.cpp` — collect-then-remove static vector pattern (#242)
- `app/systems/miss_detection_system.cpp` — emplace-during-exclude-view pattern (ScoredTag+MissTag)

**Verdict:** `coverage gaps`

**High-priority gaps:**
1. R7 stale event discard test explicitly missing (decisions.md: "add Baer test"). No test verifies that events enqueued before a phase transition are not replayed in the post-transition frame.
2. `entt::dispatcher` missing from ctx not tested for crash safety. `make_registry creates all singletons` verifies only 6 of ~17 singletons; dispatcher is absent from assertions.
3. Emplace-during-exclude-view safety (miss_detection_system) untested explicitly.

**Medium-priority gaps:**
4. on_construct<ObstacleTag>/on_construct<ScoredTag> disconnect lifecycle tests only run under #ifdef PLATFORM_DESKTOP — invisible on Linux CI.
5. make_registry singleton test incomplete (doesn't verify SongState, EnergyState, HighScoreState, entt::dispatcher, etc.).

**Coverage to preserve:** dispatcher contract tests, #213 no-replay, GamePhaseBit bitmask, on_destroy signal pool-order test, hashed string element_map, FNV-1a collision test, lifecycle ctx null test.

### 2026-04-27 — R7 Dispatcher/Setup Coverage (#331 + #332)

**Branch:** squad/331-dispatcher-r7-tests
**Commit:** 25e0950

**Tasks completed:**
- **#331 R7 stale-event discard**: Added 3 regression tests to `test_entt_dispatcher_contract.cpp` documenting the R7 guarantee (GoEvents queued before/during a phase transition do not cause effects in the post-transition phase). Tests cover: (1) GoEvent delivered in GameOver — phase guard prevents effect, (2) drain-first order proves pre-transition delivery + empty queue post-transition, (3) full two-tick sequence with lerp_t anti-replay check.
- **#332 ctx singleton coverage**: Expanded `test_components.cpp`'s existing "make_registry creates all singletons" test to assert ALL singletons including `entt::dispatcher` (previously absent). Added 3 new tests verifying `wire_input_dispatcher` wired GoEvent and ButtonPressEvent listeners, plus no-replay invariant for the wired dispatcher context.

**Key learnings:**
- `test_entt_dispatcher_contract.cpp` was designed implementation-independent (no game system calls), but R7 tests need real system behaviour (phase guards). Adding `#include "test_helpers.h"` is acceptable when registry-backed semantics are needed — the contract is still stable since it tests ECS + phase guard interaction, not rendering/audio.
- The worktree build requires explicit cmake flags: `-DCMAKE_PREFIX_PATH=<root>/build/vcpkg_installed/arm64-osx` plus `-Draylib_DIR`, `-DCatch2_DIR`, `-DEnTT_DIR` pointing to the team-root vcpkg_installed. The worktree's build/ is otherwise empty (no Makefile until cmake succeeds).
- R7 is satisfied by two cooperating mechanisms: drain-first order in game_state_system (events always drained before transition_pending is processed) + phase guards in every listener handler. Tests should cover both independently.

**Suite result:** 2730 assertions in 821 test cases — all pass.

### 2026-04-27T23:30:53Z — P0 TestFlight: ecs_refactor Archetype Audit (Issue #344)

**Status:** VALIDATED — Build clean, tests pass

**Role in manifest:** Test Engineer (background validation)

**Summary:**
- Build clean with zero project warnings
- 11 `[archetype]` tests passed (64 assertions)
- Full suite: 810 test cases / 2748 assertions — all pass
- No source changes made; validation only

**Note:** Pre-existing unrelated `ld: duplicate libraries` raylib linker warning (out of scope).

**Deferred:** P2/P3 source changes (miss_detection idempotency, on_construct platform-gate).


### 2026-04-28 — #336 + #342 ECS Regression Test Coverage

**Status:** CODE COMPLETE — Build blocked by pre-existing bench_systems.cpp breakage (another agent's in-progress EventQueue→dispatcher refactor), not by new code.

**Task:** Add regression tests for miss_detection_system idempotency (#336) and port on_construct signal lifecycle coverage out of PLATFORM_DESKTOP guard (#342).

**Files created:**

`tests/test_miss_detection_regression.cpp` — `[miss_detection][regression][issue336]`
- 5 test cases covering: N-expired=N-MissTag-N-ScoredTag; idempotence (second run no-ops); above-DESTROY_Y not tagged; pre-ScoredTag excluded; non-Playing phase no-op.

`tests/test_signal_lifecycle_nogated.cpp` — `[signal][lifecycle][issue342]`
- 6 test cases covering: on_construct<ObstacleTag> increments counter; on_destroy<ObstacleTag> decrements; wire_obstacle_counter idempotent (no double-count if called twice); counter zeroes after all destroys; no underflow guard; wired flag blocks re-entry.
- No PLATFORM_DESKTOP guard.

**Build validation:**
```bash
# Both files pass syntax check against dirty working tree
# Both compile to object files: zero errors, zero warnings
[ 46%] Building CXX object CMakeFiles/shapeshifter_tests.dir/tests/test_miss_detection_regression.cpp.o
[ 70%] Building CXX object CMakeFiles/shapeshifter_tests.dir/tests/test_signal_lifecycle_nogated.cpp.o

# Pre-existing binary (812 tests, 2749 assertions) — baseline green
./build/shapeshifter_tests "[death_model]" --reporter compact
# All tests passed (45 assertions in 10 test cases)
./build/shapeshifter_tests "[cleanup]" --reporter compact
# All tests passed (20 assertions in 8 test cases)
```

**Pre-existing blocker:** `benchmarks/bench_systems.cpp` uses `EventQueue` (removed in dirty `input_events.h`) and old `ButtonPressEvent` constructor. Not my code. Full link fails until that refactor is complete.

**Closure readiness:**
- #336: Code-complete. Closure-ready after bench fix allows the binary to rebuild.
- #342: Code-complete. Closure-ready after same.

**Decision filed:** `.squad/decisions/inbox/baer-ecs-regression-tests.md`

### 2026-04-28 — Malformed UI JSON Spawn Regression Coverage (Issue #346)

**Status:** COMPLETE — 12 new tests green; full suite 837/837 pass.

**Task:** Add focused regression coverage for `spawn_ui_elements()` catch branches flagged by Kujan in PR #338 review.

**Changes made:**
- `app/systems/ui_loader.cpp/h`: Extracted `spawn_ui_elements()` as a public free function (moved from static in `ui_navigation_system.cpp`). Also moved helpers: `skip_for_platform`, `json_font`, `json_align`, `json_shape_kind`. Added null-safety to `json_color_rl`.
- `app/systems/ui_navigation_system.cpp`: Removed moved statics; now calls `spawn_ui_elements` from `ui_loader.h`.
- `tests/test_ui_spawn_malformed.cpp`: 12 new tests covering all three catch paths (text animation, button outer, button inner animation, shape outer) plus baseline and mixed-element scenarios.

**Verified:**
- `./build/shapeshifter_tests "[ui][spawn]"` — 27 assertions in 12 test cases, all pass.
- Full suite `~[bench]` — 2845 assertions in 837 test cases, all pass.
- `git ls-tree -r --name-only HEAD | grep ':'` — no colons.
- Pushed to `origin user/yashasg/ecs_refactor` (commit 710ff34).
- Issue #346: **closure-ready** — not closed per task instructions.

### 2026-04-29 — Component Cleanup Pass Regression Coverage

**Status:** COMPLETE — static_assert guards added; decision note filed.

**Task:** Lightweight regression coverage for the ObstacleScrollZ bridge-state + ObstacleModel/ObstacleParts component cleanup pass without overlapping Hockney/Fenster implementation files.

**What I added:**
- BF-5a static_asserts in `tests/test_obstacle_model_slice.cpp` Section D: `ObstacleScrollZ` is non-empty, standard-layout, and `z` field is `float`.
- BF-5b static_asserts: `ObstacleModel` is non-copy-constructible, non-copy-assignable, move-constructible, move-assignable.
- These mirror the existing BF-4 guards for `ObstacleParts`.

**Pre-existing build blockers discovered (Keaton's WIP):**
- `app/entities/obstacle_entity.h` referenced from `beat_scheduler_system.cpp` but does not exist → `apply_obstacle_archetype` and `spawn_obstacle_meshes` undeclared.
- `obstacle_spawn_system.cpp:91` missing `beat_info` field initializer (Werror).
- The cached test binary was valid but a clean rebuild fails. Flagged in decision inbox.

**Coverage finding:** `test_model_authority_gaps.cpp` and `test_obstacle_model_slice.cpp` Section C already provide comprehensive dual-view bridge-state coverage for all 5 lifecycle systems. No new runtime tests were necessary.

**Key pattern:** Static_asserts for component RAII + layout contracts belong in `test_obstacle_model_slice.cpp` Section D alongside BF-4.

---

### 2026-04-28 — Team Session Closure: ECS Cleanup Approval

**Status:** APPROVED ✅ — Deliverable logged; ready for merge.

Scribe documentation:
- Orchestration log written: .squad/orchestration-log/2026-04-28T08-12-03Z-baer.md
- Team decision inbox merged into .squad/decisions.md
- Session log: .squad/log/2026-04-28T08-12-03Z-ecs-cleanup-approval.md

Next: Await merge approval.

### 2026-04-29 — Camera Cleanup Validation Gate

**Status:** COMPLETE — 7 new [camera_cleanup] tests green; static guards compile clean.

**Task:** Prepare validation for camera.h → entity migration. Keyser owns production code; Baer owns test/static coverage only.

**Discovery:** Keyser's implementation was already partially landed (camera_entity.h, camera_entity.cpp, camera_system.h updated). The test file `test_gpu_resource_lifecycle.cpp` had a stale `#include "components/camera.h"` causing a redefinition conflict with the updated `camera_system.h`. Fixed as compile prerequisite.

**Changes made:**
- `tests/test_gpu_resource_lifecycle.cpp`: Replaced `components/camera.h` include with `entities/camera_entity.h`; added 5 static_asserts for `GameCamera`/`UICamera` type traits (standard-layout, default-constructible, distinct types).
- `tests/test_camera_entity_contracts.cpp` (new): 7 runtime tests using `spawn_game_camera`/`spawn_ui_camera` factories covering: single-entity per type, distinct entity IDs, no dual-carry, accessor validity, independent destruction.

**Remaining gate items for Keyser:**
- `grep -r "components/camera.h" app/` must return zero
- `reg.ctx().get<GameCamera/UICamera>` in game_render_system, ui_render_system, camera_system must switch to `game_camera(reg)`/`ui_camera(reg)` accessors
- `reg.ctx().emplace<GameCamera>` in `camera::init()` must switch to `spawn_game_camera(reg)`

**Results:** 2547 assertions / 867 test cases — all pass. Zero warnings.

**Decision filed:** `.squad/decisions/inbox/baer-camera-cleanup-tests.md`

### 2026-04-29 — Title Settings Navigation Regression Strategy

- Added a **headless proxy regression** in `tests/test_game_state_extended.cpp` that simulates the title gear outcome (`transition_pending=true`, `next_phase=Settings`) and verifies the fixed-step + navigation chain reaches `GamePhase::Settings` and `ActiveScreen::Settings` without opening a window.
- This locks down the non-graphical contract between `title_screen_controller` output state, `game_state_system` transition consumption, and `ui_navigation_system` screen activation.
- **Testability gap:** we still cannot deterministically unit-test the actual `GuiButton` click for `SettingsButtonPressed` in `render_title_screen_ui()` because the controller has no injectable raygui input seam and depends on live GUI calls.
- Best validation proxy remains: (1) this headless transition-contract test, plus (2) manual smoke in desktop build confirming gear click sets Title → Settings.

---

### 2026-04-29 — macOS Startup/Shutdown Invalid-Free Isolation

**Task:** Independently reproduce the `bash run.sh` macOS allocator abort (`pointer being freed was not allocated`) without manual gameplay.

**Reproduction found:**
- LLDB automation can force `game_loop_should_quit()` to return true on the first quit check, so runtime executes `game_loop_init()` then `game_loop_shutdown()` without any manual click or gameplay.
- The crash reproduces before gameplay and before obstacle spawning.
- Backtrace: `UnloadMaterial()` → `camera::ShapeMeshes::release()` → `camera::shutdown()` → `game_loop_shutdown()` → `main`.

**Root-cause evidence:**
- `camera_system.cpp::unload_shape_meshes()` calls `UnloadShader(sm.material.shader)` and then `UnloadMaterial(sm.material)`.
- raylib 5.5 `UnloadMaterial()` already unloads any non-default shader before freeing material maps.
- Runtime log shows shader ID 6 unloaded twice, then macOS aborts in `UnloadMaterial()`.

**Existing coverage assessment:**
- `[gpu_resource_lifecycle]` covers copy/move/default-unowned RAII invariants only.
- `[model_slice][headless_guard]` covers `build_obstacle_model()` no-op behavior without `InitWindow()`.
- These headless tests pass and are useful, but they cannot catch live GPU-resource startup/shutdown invalid frees.
- `tests/test_lifecycle.cpp` only covers `game_loop_should_quit()` and is excluded from the current CMake test source list; it also does not initialize raylib.

**Test-only addition:**
- Added opt-in target `shapeshifter_startup_shutdown_smoke` with source `tests/smoke/startup_shutdown_smoke.cpp`.
- Command:
  ```bash
  cmake --build build --target shapeshifter_startup_shutdown_smoke
  ./build/shapeshifter_startup_shutdown_smoke --frames 0
  ```
- The target is `EXCLUDE_FROM_ALL` because it opens a real raylib window and is not headless-CI safe.
- `--frames 0` isolates init/shutdown; `--frames 1` also exercises one render frame before shutdown.

**Validation:**
- Smoke target builds warning-free.
- Smoke run currently fails with abort status 134, as expected until the production teardown bug is fixed.
- Focused headless tests: `[gpu_resource_lifecycle],[model_slice][headless_guard]` → 8 test cases / 12 assertions, all pass.
- Full non-bench suite: 777 test cases / 2217 assertions, all pass.

**Decision filed:** `.squad/decisions/inbox/baer-startup-shutdown-smoke.md`

### 2026-04-29 — Gameplay Shape Buttons HUD Migration Coverage

- For raygui-owned gameplay controls, deterministic tests need a seam at the controller boundary (e.g., injectable button-result provider or explicit controller action handler). Without that seam, headless tests can verify state-transition contracts but cannot reliably assert direct `GuiButton` click behavior.
- Migration off ECS shape-button entities requires replacing tests that assert `ShapeButtonTag` spawn/hit-test wiring with tests that assert semantic outcomes (`ButtonPressEvent` shape payloads and `player_input_system` shape-window transitions) from the HUD controller path.

## Learnings

- For HUD shape taps mediated by raygui `GuiButton`, preserve circular forgiveness by separating concerns: raygui should use an input-only square that encloses the intended circular radius, and acceptance should remain a circle check in controller logic.
- Deterministic reachability tests should assert both stages: (1) the production raw raygui bounds admit the +70px edge tap and exclude +71px, and (2) the circular filter still rejects out-of-radius taps.
- A small half-pixel expansion on raw input bounds avoids floating-edge misses at exact boundary taps while still letting the circular filter enforce the real hit radius.

## 2026-04-29 — Gameplay shape buttons migration (revisions R4, R5)

**Status:** TWO-PASS WORK CYCLE
**Reviewer:** Kujan
**Verdict (R4 / geometry audit):** REJECTED (source drift 60/220/380 vs 90/290/490)
**Verdict (R5 / reachability):** APPROVED

**Revision 1 (Regression Strategy, Pre-Migration):**
- Wrote deterministic regression coverage plan for gameplay shape HUD migration
- Tested both HUD controller shape event emission boundary and unchanged downstream `player_input_system` outcomes
- Deliberately avoided brittle ECS entity-structure tests during migration transition
- Plan itself approved but execution was blocked by prior geometry issues

**Revision 2 (Geometry Audit & Blocker Identification):**
- Audited geometry source drift: `gameplay.rgl` DummyRec slots are x=60/220/380, but generated header exposes x=90/290/490
- Identified live drift violating source-of-truth requirement and breaking safe regeneration
- Flagged mismatch as blocking issue for reviewer re-assignment

**Revision 3 (Reachability Expansion + Contract):**
- Implemented raw raygui input bounds expansion to enclose 1.4× circular hit radius
- Created `GameplayHudLayout_ExpandedShapeButtonBounds(...)` helper in generated layout
- Expanded bounds from slot center + `visual_radius * 1.4f` + 0.5 padding
- Added deterministic reachability contract tests: `+70px vertical tap accepted`, `+71px rejected`
- Circular acceptance gate (`CheckCollisionPointCircle`) remains final filter in controller before `ButtonPressEvent` dispatch

**Kujan approval (R5):** Reachability contract implementation approved. Production acceptance path now passes `+70` and rejects `+71` per spec. Flagged downstream geometry source alignment for final implementer (Hockney).

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-baer-r1.md` (R1 plan), `.squad/orchestration-log/2026-04-29T22-03-09Z-baer-r2.md` (R5 approval)

### 2026-04-29 — ActiveTag/ECS hit-test retirement test impact (read-only)

- If gameplay shape input is fully HUD-owned (raygui/controller path), tests asserting ECS `ShapeButtonTag` spawn state or `ActiveTag` structural sync become dead-surface checks and should be removed/replaced with controller-level shape press tests plus downstream `player_input_system` effects.
- Keep phase-gating behavior coverage, but assert it through observable outcomes (e.g., no shape morph outside Playing) instead of asserting `ActiveTag` component presence/absence.
- Minimal regression gate for this migration: `[input_pipeline][hud]`, `[gamestate][play_session]`, `[gesture_routing]`, plus any retained menu hit-test coverage if menu `HitBox` path remains ECS-based.

## 2026-04-29T23:54:05Z — Guard-Clause Refactor Validation Phase

Orchestration log written. Guard-clause validation planning completed successfully. Team coordination: Baer (planning) → Keaton (implementation) → Kujan (review gate) ✓ All passed.

Decision #169 captured in decisions.md. Risk hotspots validated during refactor sweep.

---

## 2026-04-30T02:04:27Z — Dead Code Cleanup Session (Team validation pass)

**Session:** Multi-agent dead code prune (Keaton/McManus cleanup, Kujan review, Fenster revision).

**Your role:** Validation foundation. Initial stale-symbol audit confirmed zero orphaned references across `app`, `tests`, `benchmarks`, `docs`, `design-docs` for removed components/systems (`ActiveTag`, `ActiveInPhase`, `UIActiveCache`, `UIState`, `ActiveScreen`, `ui_navigation_system`, `has_overlay`, `HitBox`, `HitCircle`, `hit_test`, `phase_activation`).

**Outcome:** All cleanup work passed final validation. Full test suite (2637 assertions, 795 test cases) passing. `git diff --check` clean. Orchestration logs written for all agents.

## 2026-04-30 — Difficulty-ramp test contract migration (LanePush → bars)

**File modified:** `tests/test_shipped_beatmap_difficulty_ramp.cpp`

- Replaced stale medium LanePush contract (percentage / consecutive run / first-arrival) with medium **bar** contract using `low_bar` + `high_bar` combined:
  - percentage within `[5%, 25%]`
  - max consecutive bars `<= 3`
  - first bar arrival `>= 30s`
- Kept easy strictness and strengthened wording: easy now asserts **only** `shape_gate` (no non-shape kinds).
- Added hard bar coverage regression: each shipped hard map must include both `low_bar` and `high_bar`.
- Added medium/hard legacy-kind regression: `lane_push_left`, `lane_push_right`, and `lane_block` must not appear.

**Validation evidence:**
- `cmake --build build -- -j4` ✅
- `./build/shapeshifter_tests "[difficulty_ramp]"` ✅ (9 assertions, 8 test cases)
- `./build/shapeshifter_tests "~[bench]"` ✅ (2180 assertions, 763 test cases)
- `python3 tools/validate_difficulty_ramp.py` ✅
- `python3 tools/validate_beatmap_bars.py --difficulty hard` ✅
- `git --no-pager diff --check` ✅

## Learnings
- When gameplay content migrates obstacle kinds, preserve regression intent by mirroring **validator contracts** (Python acceptance scripts) in C++ shipped-content tests; this avoids stale mechanics-specific assertions while keeping release-safety coverage intact.

## Session: Assets Root Removal (2026-04-30)

Replaced stale LanePush difficulty-ramp test contract in `tests/test_shipped_beatmap_difficulty_ramp.cpp`. Removed medium LanePush assertions (0% in shipped beatmaps). Implemented bar-focused medium contracts (low+high percentage window, max consecutive run, first-arrival readability gate). Hard: low/high coverage. Explicit rejection of legacy kinds. `./build/shapeshifter_tests "[difficulty_ramp]"` passes. Full suite + validators pass. Diff clean.

**Manifested:** Decision #174 merged to `.squad/decisions.md`

## 2026-04-30 — Song-complete playback loop/regression coverage

**Task:** Investigate report: on song end, Song Complete UI does not appear while music repeats.

**Test changes:**
- `tests/test_song_playback_system.cpp`
  - Added `song_playback: finished song stays latched and does not restart on later ticks`
  - Added `song_playback: end-of-song transitions to SongComplete and remains stopped`

**Root-cause surface analyzed:**
- Transition contract is split across systems: `song_playback_system` latches `finished/playing`, then `game_state_system` schedules and executes `SongComplete` on the next tick.
- Existing coverage did not pin the full two-tick contract (finish → transition pending → phase enter) together with "no restart" invariants after finish.
- Audio-device loop behavior when `GetMusicTimePlayed()` wraps cannot be deterministically forced in current headless tests; this requires an injectable music-clock seam.

**Validation evidence:**
- `cmake --build build -- -j4` ✅
- `./build/shapeshifter_tests "[song_playback]"` ✅ (66 assertions, 25 test cases)
- `./build/shapeshifter_tests "[gamestate]"` ✅ (98 assertions, 40 test cases)
- `./build/shapeshifter_tests "[song_complete]"` ✅ (12 assertions, 2 test cases)
- `./build/shapeshifter_tests "~[bench]"` ✅ (2197 assertions, 765 test cases)
- `git --no-pager diff --check` ✅
- **Decision logged:** #176 in `.squad/decisions.md` (2026-04-30T07:15:10Z)
- **Cross-team:** McManus fixed looping; Kujan approved full cycle.

## Learnings
- For terminal-phase regressions, use a deterministic two-tick contract test: tick A latches end-of-song state, tick B consumes `transition_pending` and enters `SongComplete`, then assert playback state remains latched (`finished=true`, `playing=false`) across later ticks.
- If production relies on non-deterministic runtime clocks (e.g., `GetMusicTimePlayed()`), add an explicit injectable clock seam; otherwise loop-at-track-end bugs are not reliably reproducible in headless CI.
