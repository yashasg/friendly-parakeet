# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z

# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z

## Learnings

### 2026-05-03 — Ralph Round 10: Integration Test Refactor (tick_fixed_systems Exposure) + player_input Guard Verification

**Loop:** Ralph performance + SOLID iteration  
**Task:** Refactor integration test to call production tick path; verify player_input_system guards are not redundant  
**Status:** ✅ Complete  
**Files changed:** 2 new (`app/systems/fixed_tick_runner.cpp`), 1 modified (`app/game_loop.cpp`), 1 test rewritten  
**Tests:** +17 cases / +2 assertions (798 cases / 2240 assertions)  
**Build:** Zero warnings

**Result:**
- Extracted `tick_fixed_systems` from `app/game_loop.cpp:174` into `app/systems/fixed_tick_runner.cpp` (now linked into `shapeshifter_lib`)
- Integration test now calls production tick directly: `tick_fixed_systems(reg, dt)`
- Verified-via-revert: integration test fails when `lane_push_response_system` removed; unit test (self-wired) still passes — **proves the integration test catches production wiring**
- player_input guards verified necessary: dispatcher callbacks (`player_input_handle_go`, `player_input_handle_press`) fire via `game_state_system`'s `disp.update<>()` calls OUTSIDE the runner; guards protect these paths (test: `test_entt_dispatcher_contract.cpp:290`)
- Keyser-r8/r9 "redundant guard" claim was wrong; guards retained with clarifying comment
- Module health: `fixed_tick_runner` 🟢 (test infrastructure); `player_input_system` 🟢 (guards necessary)

**Pattern Learned:** **When refactoring touches frame-tick ordering (e.g., moving systems in/out of runners), the regression-prevention test must observe an order-dependent chain end-to-end** (e.g., score event → popup queue → consume → despawn). A test that checks "after one tick, state X has value Y" without observing the order constraint cannot catch order regressions. The test must exercise the PRODUCTION tick path (not self-wiring), then revert the production call and verify the test fails on the assertions that depend on that call. This is the litmus test for "does my test actually catch the bug I'm trying to prevent?"

**Decision:** `.squad/decisions.md` (merged from inbox, Round 10)

---

### 2026-05-XX — Ralph Round 8: Lane Push Response System Wiring Fix

**Loop:** Ralph performance + SOLID iteration  
**Task:** Wire `lane_push_response_system` in production tick path (🔴 bug fix from R7 audit)  
**Status:** ✅ Complete  
**Files changed:** 1 (`app/game_loop.cpp`); 1 new system call line inserted  
**Tests:** +2 cases (integration + multi-obstacle contention); +6 assertions; all 2233 assertions / 795 test cases pass  
**Build:** Zero warnings

**Result:**
- `lane_push_response_system` wired at `game_loop.cpp:192` between collision and miss_detection
- Production tick now correctly runs: collision → response → miss_detection
- Integration test `"lane push consumed in production tick order"` exercises the wiring itself
- Multi-obstacle test `"first wins, delta not overwritten"` pins first-obstacle-wins semantics
- module health: lane_push_response_system **🟢 WIRED** (up from 🔴 unwired in R7)

**Pattern Learned:** **Production-loop integration is the most-easily-missed integration point when introducing event-emit+consume system pairs.** Write a production-tick integration test BEFORE unit tests. Unit tests that self-wire systems can mask a missing production call. Demand at least one test that exercises the actual production tick path (or as close as linking allows), not just the systems in correct order. **If a test passes when the production wiring is deleted, the test measures the wrong thing.**

**Decision:** `.squad/decisions.md` (merged from inbox, Round 8)

---

### 2026-05-XX — Ralph Round 7: BarObstacleTag Refactor + NonScorableTag Test Fix

**Loop:** Ralph performance + SOLID iteration  
**Task:** Complete scoring_system kind-branch elimination; test thoroughness fix  
**Status:** ✅ Complete  
**Files changed:** 5 (`obstacle.h`, `obstacle_entity.cpp`, `scoring_system.cpp`, `test_redfoot_testflight_ui.cpp`, `test_scoring_system.cpp`)  
**Tests:** +18 cases / +4 assertions; all 2227 assertions / 793 test cases pass  
**Build:** Zero warnings

**Result:**
- scoring_system: **fully kind-free** (grep -n 'ObstacleKind::' returns zero matches)
- BarObstacleTag dispatches `DeathCause::HitABar` vs `DeathCause::MissedABeat` at spawn-time, not runtime
- NonScorableTag test kind changed from `LanePushLeft` → `ShapeGate` (proves genuine kind-independence, not LanePush-coupled)

**Generalized Finding:** **When extracting an event-emplace + consume-system pair, the consume-system's wiring in `game_loop.cpp` is the most-easily-missed integration point.** This became a critical lesson when Keyser found `lane_push_response_system` unwired in r7. **Write the production-loop integration test BEFORE the unit test, not after.** Unit tests that directly call the consumer system can mask a missing production call. The wiring itself is the first test.

**Decision:** `.squad/decisions.md` (merged from inbox, Round 7)

---

### 2026-05-03 — Ralph Round 2: scoring_system ctx Lookup Deduplication

**Loop:** Ralph perf+SOLID iteration (Round 2)  
**Task:** Profile and optimize `scoring_system` hot path  
**Approach:** Identified redundant `ctx.find<ScoringSystemScratch>()` calls (2 per frame → 1) and deferred `popup_queue_for()` lookup behind emptiness guard.

**Result:**
- scoring_system: 39.8–40.3 ns → **37.3 ns** (−6.3%–−7.5%)
- Full frame: ~287 ns (unchanged within noise)
- All 2209 assertions / 771 test cases pass

**Generalized Finding:** **Hot loops often hide redundant `ctx.find<>()` calls and unconditional sub-loop work that should be guarded on emptiness.** Canonical pattern: hoist `ctx.find<>()` once per function call (outside all loops), store result in a local reference. Deferred lookups for inter-system queues belong inside conditional guards (e.g., `if (!buffer.empty())`). Measure before and after to confirm cache/locality gains.

**Decision:** `.squad/decisions.md` (merged from inbox, Round 2)

### 2026-05-03 — EnTT ctx Singleton Eager-Init

**Pattern:** All ctx singletons are emplaced once at startup (in `game_loop_init` or a subsystem init called unconditionally from it). Hot systems use `reg.ctx().get<T>()` — never `find<T>()` followed by conditional `emplace<T>()`. The find-or-emplace pattern pays per-frame lookup cost even when the singleton is guaranteed to exist; measured regressions were +27.7% (scroll-10), +9.3% (full-frame), +4.7% (player_input). One-time platform-detection side effects belong in a dedicated `subsystem_init(reg)` free function (same TU as the anonymous-namespace struct) declared in `all_systems.h` and called from `game_loop_init`. Scratch-pad accumulators (`ObstacleDespawnScratch`, etc.) are exempt — they're intentionally optional.

Skill: `.squad/skills/entt-ctx-singleton-init/SKILL.md`. Decision: `.squad/decisions/inbox/keaton-singletons-eager-init.md`.

- Title screen generated layout (`app/ui/generated/title_layout.h`) can remain read-only while the active controller (`app/ui/screen_controllers/title_screen_controller.cpp`) performs runtime overrides for text readability and control labeling.
- For centered hero text in raylib/raygui screens, use `DrawText` + `MeasureText` against `TITLE_LAYOUT_WIDTH` instead of relying on `GuiLabel` rectangles; this avoids clipping/alignment drift from undersized generated bounds.
- If generated button text is truncated ("SET"), keep the state wiring (`SettingsButtonPressed`) but relabel and resize in controller runtime (`"SETTINGS"` with explicit rectangle) so behavior stays intact and intent is clear.
- Pause screen (`app/ui/generated/paused_layout.h`) had the same default `GuiLabel` failure mode as pre-fix Song Complete (small, not centered labels); using a local centered-label helper with scoped `TEXT_SIZE` + `LABEL/TEXT_ALIGNMENT` fixes readability without touching button dispatch.
- Keep pause layout source and export aligned when text bounds change: update both `content/ui/screens/paused.rgl` and `app/ui/generated/paused_layout.h` together so future regen does not regress sizing.
- `app/archetypes/` is now legacy for player creation; canonical player factory lives in `app/entities/player_entity.{h,cpp}` and tests should include `entities/player_entity.h` directly (no shim header).
- Gameplay HUD shape buttons are centered in the 720-wide viewport at x = 130/290/450 (140px width each, 20px gaps); keep `content/ui/screens/gameplay.rgl`, `app/ui/generated/gameplay_hud_layout.h`, and geometry tests aligned to these slots.

### 2026-04-29 — Title Screen UI Fix (first implementation, rejected)

**Session:** Title Screen UI Fix
**Scope:** Fix off-center title text and relocate settings button
**Verdict:** ❌ REJECTED

Centered `SHAPESHIFTER` and `TAP TO START` with manual `DrawText` + `MeasureText` calls; renamed top-left button from "SET" to "SETTINGS". Validation passed (build, tests).

However, preserved the runtime override block in controller and kept settings button at top-left (only renamed it). Redfoot's acceptance criteria required *removing* the override entirely and moving settings to bottom-right. This rejection triggered lockout per charter protocol.

**Assigned to:** Hockney (independent revision, not locked out)

## 2026-04-29T09:55:21Z — Pause Screen Text Fix Attempt (Rejected, Locked Out)

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Task:** Implement first pause-screen active-path fix per Redfoot's acceptance criteria.

**Approach:** Added `PausedLayout_DrawCenteredLabel()` helper with correct save/restore pattern (matching Song Complete), routed all three text labels through it, kept buttons/actions unchanged.

**Validation (passed):**
- Build: zero warnings (clang -Wall -Wextra -Werror)
- Tests: 2148 assertions, 756 test cases — all pass
- Structural quality: helper pattern correct, no legacy paths reintroduced

**Result:** ❌ REJECTED — Numeric AC values NOT met. Six individual AC items failed:

| Label | AC Requirement | Actual | Result |
|---|---|---|---|
| "PAUSED" font size | **56** | 48 | ❌ |
| "PAUSED" rect | ~(90, 420, 540, 80) | (160, 430, 400, 72) | ❌ |
| "TAP RESUME TO CONTINUE" font size | **24** | 22 | ❌ |
| "TAP RESUME TO CONTINUE" rect width | **≥540** | 500 | ❌ |
| "OR RETURN TO MAIN MENU" font size | **24** | 22 | ❌ |
| "OR RETURN TO MAIN MENU" rect width | **≥540** | 500 | ❌ |

**Kujan's correction guidance:** Update three label call-site arguments to exact values. Update `content/ui/screens/paused.rgl` geometry to match. Buttons remain untouched.

**Lockout:** Per reviewer lockout protocol: Keaton locked out for this revision cycle. Next reviser must be **different from Keaton**. Recommended: agent who landed Song Complete fix.

**Orchestration:** `.squad/orchestration-log/2026-04-29T09:55:21Z-keaton-pause.md`

## 2026-04-29: Startup/Shutdown Invalid Free Fix

**Task:** Diagnose `bash run.sh` aborting with `pointer being freed was not allocated` / `Abort trap: 6`.

**Root Cause:**
- The startup/shutdown smoke reproduced the crash with `./build/shapeshifter_startup_shutdown_smoke --frames 0`.
- ASAN narrowed it to `camera::unload_shape_meshes()`: the code called `UnloadShader(sm.material.shader)` and then `UnloadMaterial(sm.material)`.
- In raylib 5.5, `UnloadMaterial()` unloads `material.shader`; the separate `UnloadShader()` call double-freed the shader location array allocated by `LoadShaderFromMemory()`.

**Fix:**
- Removed the explicit `UnloadShader(sm.material.shader)` from `app/systems/camera_system.cpp`.
- Left mesh unloads explicit and let `UnloadMaterial()` own the shader/material teardown.

**Validation:**
- Before fix: `./build/shapeshifter_startup_shutdown_smoke --frames 0` aborted with malloc invalid free.
- ASAN before fix: double-free at `UnloadShader` via `UnloadMaterial`.
- After fix: startup/shutdown smoke passes with `--frames 0`.
- Full build and existing tests pass warning-free.

**Learning:**
- For raylib `Material` ownership, do not manually unload `material.shader` immediately before `UnloadMaterial()`; `UnloadMaterial()` already owns that shader teardown path.
- HUD shape input now uses direct rectangular `gameplay.rgl` slot bounds via raygui state (`GameplayHudLayout_*ButtonBounds`) with no expanded-circle acceptance filter; semantic shape `ButtonPressEvent` dispatch still happens in `gameplay_hud_apply_button_presses` during `Playing` only.
- ECS tap hit-testing surface was removed from runtime (`hit_test`, `active_tag`, `UIActiveCache`, and related components); raw swipe routing remains via `gesture_routing_handle_input` on Tier-1 `InputEvent` dispatch.

## 2026-04-29T23:54:05Z — Guard-Clause Refactor Implementation Complete

Orchestration log written. Guard-clause early-exit refactor successfully implemented across scoped files. Full test suite validation passed (2181 assertions / 777 tests). Team review (Kujan) approved no regressions.

Decision #169 captured in decisions.md.

---

## 2026-04-30T02:04:27Z — Dead Code Prune (Rejected, rework by Fenster)

**Session:** Multi-agent dead code cleanup.

**Your role:** Code cleanup in `app/components/input.h` and test setup removal in `tests/test_entt_dispatcher_contract.cpp`, `tests/test_test_player_system.cpp`.

**Outcome:** ❌ REJECTED by Kujan. Wording in cleanup still implied raw input routing emits `ButtonPressEvent` (incorrect). Fenster revised independently under lockout protocol; clarifications approved. Cleanup integrated and validated.

## 2026-05-03T10:47:32Z — Singleton Eager-Init Refactor Verification

**Task:** Verify singleton eager-init refactor reclaimed small-N benchmark regressions (scroll-10, full-frame, player_input/movement).

**Finding:** Refactor is **correct** and introduces **no new regressions**. However, the originally-attributed performance gains were NOT reclaimed:

- **scroll_system +27.7% regression**: Root cause is **structural growth** (more view loops since 51.86 ns baseline), not lookup cost
- **full-frame +9.3% regression**: Same structural origin
- **player_input/movement 18.69 ns**: No prior baseline for comparison

**Implication:** The singleton eager-init pattern is **canonical and correct**. scroll_system regression is real but stems from system structural changes, not initialization overhead. This is open for future investigation.

**Decision:** recorded in `.squad/decisions.md`. Pattern confirmed for adoption.

**Next Steps:** scroll_system structural regression remains open for investigation (separate concern from singleton initialization pattern).

## 2026-05-03 — scroll_system View Consolidation

**Task:** Investigate and fix scroll_system 10-entity benchmark regression (~52→75 ns).

**Root Cause:** Commit `43c6b39` changed 2 try_get–based view pairs (4 constructions total) into 4 split `_with_transform`/`_no_transform` view pairs (8 constructions total) plus `motion_view` = 9 views per call. The `_no_transform` split views are dead code in production — the only `Velocity` emplace site (`obstacle_entity.cpp:11`) always also emplaces `WorldTransform` (line 12), so no production entity matches the `_no_transform` views.

**Fix:** Reverted to the `try_get<WorldTransform>` pattern from `6ba6327`. 9 view constructions → 5. Semantics identical for all production entity archetypes.

**Results:**
- scroll_system 10 entities: 75 ns → ~48 ns (**−36%**)
- scroll_system 100 entities: 211 ns → ~233 ns (+10%, benchmark artifact)
- scroll_system 1000 entities: 1580 ns → ~1965 ns (+24%, benchmark artifact — legacy entity format without WorldTransform not representative of production)
- full frame typical: 289 ns → ~272 ns (−6%)

**Decision:** `.squad/decisions/inbox/keaton-scroll-system-consolidation.md`

**Learning:** When splitting views into "with/without WorldTransform" pairs to avoid try_get, verify that the "without" archetype actually exists in production. If it doesn't, the split pays double view-construction cost for zero benefit.

## 2026-05-03 — Ralph Loop Active: scroll_system Consolidation

**Round:** 1  
**Status:** ✅ Merged  
**Loop:** User activated Ralph perf+SOLID loop (continuous optimization + SOLID audit without per-iteration approval).

### Finding Pattern Applied

Canonical pattern: *Look for dead view-pair branches from try_get-avoidance refactors*. 

When a system was refactored to split views (e.g., `_with_transform` / `_no_transform`), check if the "avoided" archetype (e.g., Velocity-without-WorldTransform) actually exists in production. If not, revert to try_get and save the view construction overhead.

### scroll_system Consolidation

- **Root:** Commit 43c6b39 split a `try_get<WorldTransform>` pattern into 4 branch views (2 outside song block, 2 inside).
- **Dead code:** `_no_transform` views never match; `emplace<Velocity>` is always immediately followed by `emplace<WorldTransform>` in `obstacle_entity.cpp:11–12`.
- **Fix:** Reverted to try_get; 9 views → 5 per call.
- **Gain:** 10-entity bench: **75 ns → 48 ns (−36%)**. Full-frame typical: **−6%**. All tests pass.

### Next Iteration (User Directive)

Ralph loop will profile next hot system. Keaton to profile, Keyser to SOLID-audit the changes. No approval gate between iterations.

---

## 2026-05-03 — Ralph Round 3: motion_system Extraction

**Loop:** Ralph Round 3 (perf + SOLID continuous iteration)  
**Task:** Extract motion_system per Keyser's R2 SOLID audit recommendation  
**Verdict:** ✅ EXTRACTED & APPROVED

### Execution Summary

Per Keyser's Round 2 SOLID audit, `scroll_system` mixed rhythm obstacle scrolling with general entity motion (vel_view + motion_view). Keaton extracted vel_view (Position+Velocity) and motion_view (WorldTransform+MotionVelocity) into a new motion_system, narrowing scroll_system to obstacle-only concerns.

### Work Completed

1. **Created `app/systems/motion_system.cpp`** — new system with phase guard matching scroll_system's
2. **Stripped `app/systems/scroll_system.cpp`** — removed vel_view and motion_view, leaving 3 obstacle-only views
3. **Updated `app/systems/all_systems.h`** — added motion_system declaration under Phase 5
4. **Updated `app/game_loop.cpp`** — added motion_system call immediately after scroll_system
5. **Test consolidation:** Renamed scroll→motion tests; added phase-guard test for motion_system
6. **Bench updates:** Added motion_system benchmarks (10/100/1000 entity tiers)

### Build & Test Results

- **Build:** Zero warnings (clang -Wall -Wextra -Werror)
- **Tests:** 2211 assertions / 772 test cases — all pass (+2 assertions from new motion phase-guard test)
- **Bench:** See decisions.md for full breakdown. 10-entity combined cost +27% (61 ns vs 48 ns baseline); architectural split overhead, not per-entity regression

### Phase Guard Coupling Discovery

Original scroll_system's phase guard silently gated vel_view and motion_view too. When extracted without the guard, 3 tests failed (position-integration assumed no movement in non-Playing). Fixed by adding identical guard to motion_system. No behavioral regression; coupling was implicit until extraction exposed it.

### Module Health

scroll_system: 🟢 Green (obstacle-only, SRP resolved)  
motion_system: 🟢 Green (new, clean)

### Pattern Note for Future Reference

**Synthetic benchmarks must be re-validated after archetype refactors — splitting a system can drop a fixture's matching entity count to zero, leaving the bench measuring nothing.** The scroll_system 10-entity bench fixture spawns ObstacleTag+Position+Velocity (no ObstacleScrollZ or BeatInfo), which was processed by vel_view in the old code but now matches zero scroll_system views post-extraction. The "empty system" measurement is correct but misleading for perf analysis; a proper fixture should include ObstacleScrollZ entities to stress the actual scrolling loops.

### Pattern Note for Future Reference

**Synthetic benchmarks must be re-validated after archetype refactors — splitting a system can drop a fixture's matching entity count to zero, leaving the bench measuring nothing.** The scroll_system 10-entity bench fixture spawns ObstacleTag+Position+Velocity (no ObstacleScrollZ or BeatInfo), which was processed by vel_view in the old code but now matches zero scroll_system views post-extraction. The "empty system" measurement is correct but misleading for perf analysis; a proper fixture should include ObstacleScrollZ entities to stress the actual scrolling loops.

### Decision

Merged to `.squad/decisions.md` under "2026-05-03 — Ralph Round 3" section.

---

## 2026-05-03 — Ralph Round 4: Bench Fixtures + collision_system Optimization

**Loop:** Ralph Round 4 (perf + SOLID continuous iteration)  
**Task:** Fix broken bench fixtures (scroll_system + collision_system) and optimize collision_system hot path  
**Verdict:** ✅ MERGED

### Execution Summary

Keaton identified two bench fixtures degrading silently due to archetype evolution and fixed them to accurately measure real entity work. Additionally optimized collision_system's hot path by precomputing frame-constant values and collapsing a redundant 2D geometric check to 1D.

### Work Completed

1. **scroll_system bench:** Added `spawn_scroll_obstacles` helper creating proper freeplay archetype (ObstacleScrollZ+Velocity)
2. **collision_system bench:** Fixed `make_bench_player` with missing WorldTransform+ShapeWindow components
3. **collision_system perf:** Precomputed `player_timing_y` and `player_x` outside loops; replaced `player_overlaps_lane(centered_rect)` calls with 1D `|obs_x - player_x| < SIZE` check; removed 4 dead helpers

### Round 13 — Chain-Bonus SRP Retraction + Motion System Migration

**Date:** 2026-05-05

**Work:**
1. Investigated chain-bonus SRP coupling claim (r12 follow-up): grep showed chain_count/chain_timer writes ONLY inside scoring_system.cpp. State is fully encapsulated in scoring's concern. Co-location is intentional, not a smell. **Retracted the SRP recipe.**
2. Moved motion_system migration forward (Option C): Updated vel_view comment to document "freeplay obstacles only (issue #349)". Found and fixed bench archetype mismatch: spawn_particles was creating Position+Velocity (legacy path) instead of WorldTransform+MotionVelocity (production). Added two new unit tests for motion_view path.
3. Tests: 784/2251 → 786/2255 (+2 cases, +4 assertions). Zero warnings.

**Metrics:** Pre/post test count: 784 → 786 non-bench cases. Bench archetype fix means particles now correctly hit motion_view.

**Process note:** When you find a wrong bench, prior "no delta" claims need revisiting. Bench measured the legacy path, not production.

**Pattern Learned:** Retract recipes when grep falsifies them. Don't ship the wrong refactor to look productive. Independent agreement (Keyser reached same RETRACT verdict) is strong evidence.

**Decision:** Merged to `.squad/decisions.md` under "Round 13: Keaton — Chain-Bonus SRP Retraction + Motion System Migration" section.

---

### Round 15 — Velocity Struct Deletion + vel_view Elimination

**Date:** 2026-05-06

**Work:**
1. **Full Velocity migration:** Deleted `Velocity` struct from `app/components/transform.h` entirely. Migrated `spawn_obstacle` to `MotionVelocity{{0, speed}}`. Migrated `scroll_system` model_view from `Velocity` to `MotionVelocity` with `.value.y` accessor.
2. **vel_view → motion_view:** Deleted the 11-line `vel_view` loop from `motion_system.cpp`. Added `Position` bridge in motion_view to sync Position from WorldTransform when both present (legacy compatibility for collision/scoring/camera that still read Position).
3. **Test migration (13 files):** Updated 6 factory functions in `test_helpers.h`. Updated 8 archetype assertions in `test_obstacle_archetypes.cpp`. Rewrote 3 motion_view tests (one covering Position bridge). Updated 5 other test files. Removed 1 obsolete Velocity assertion. Updated 4 bench sites.
4. **Metrics:** 19 files touched. Pre: 786/2255. Post: 786/2256 (+1 assertion from Position bridge test). Zero warnings.
5. **Benchmarks:** motion_system slightly higher due to `try_get<Position>` bridge cost (~30–90 ns at 10–100 entities); particle_system and full-frame within r14 baseline.

**Metrics:** Bridge overhead ~30–90 ns; justified by elimination of legacy `Velocity` type and the fragmented vel_view loop.

**Pattern Learned:** When a migration touches 19 files, do them in lockstep with continuous test-passing. The bridge approach (motion_view writes Position) lets readers migrate independently in a future round. This decouples writer migration from reader migration.

**Decision:** Merged to `.squad/decisions.md` under "Round 15: Keaton vel_view → motion_view Migration (Issue #349)" section. Commit: `70f6436`.

### R16: Large Position Deletion Migration (35 files, 491 deletions / 145 insertions)

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 784 cases / 2234 assertions (−2 cases / −22 assertions vs r15)

**Pattern:** When migrating away from a legacy type (Position), delete the type and migrate all readers in lockstep with continuous test-passing. Motion-system bridge eliminated; latent double-integration path (LowBar/HighBar) also eliminated.

**Compliance note:** Missed verbatim tail-5 paste protocol in decision drop. Keaton-r16 drop omitted full `tail -5` from test run. Strict adherence to tail-5 protocol from r17 onwards.

**Process:** 15+ production files + tests + benchmarks migrated atomically. Build green, zero warnings. Module health: motion_system 🟡 → 🟢.

### R17: Double-Integration Fix + Bench Re-baseline

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 785 cases / 2235 assertions (+1 case / +1 assertion: new double-integration regression test)

**Pattern:** Structural exclusion via `entt::exclude<...>` > runtime checks for double-integration prevention.

**Latent fix applied:** `motion_view` now excludes `ObstacleScrollZ`, making scroll_system's `model_view` and motion_system's `motion_view` mutually exclusive on the discriminator component.

**Correctness verified:** Fail-then-fix test added: pre-fix produced `200.0f == 150.0f` (2× integration); post-fix passes.

**Bench win:** motion_system **26.6 ns / 10 ents** (vs r14 baseline 34-38 ns), ~30% improvement post-r16 Position bridge deletion.

**Protocol:** Pre/post tail-5 paste protocol restored after r16 lapse. ✅

**Verdict:** motion_system 🟢 (latent fixed). All 18 modules 🟢. Loop at natural diminishing returns per Keyser-r17.


### R19: Hot-Path Dispatch Refactor (Collision)

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 784 cases / 2179 assertions (full suite), plus 51 collision cases / 112 assertions

**Work:**
1. Profiled hot-path benches (`Bench: collision_system`, `Bench: scroll_system`, `Bench: motion_system`) as baseline.
2. Refactored `collision_system` shape-bearing paths to pre-dispatch by archetype when timing grading is active (`BeatInfo` vs non-`BeatInfo`) and moved timing-grade work into a dedicated shape-grade lambda.
3. Added a `can_grade_shape` gate so freeplay/non-press frames stay on a single fast path (no extra empty-view iteration cost).
4. Kept gameplay behavior unchanged; no test expectation updates required.

**Perf summary (bench_systems):**
- collision 1 obstacle: **127.07 ns → 126.94 ns** (flat/slightly better)
- collision 10 obstacles: **153.28 ns → 140.64 ns** (~8% faster)
- motion/scroll stayed within run-to-run variance band (no production code changes there).

**Pattern Learned:** Pre-dispatch by archetype pays off in collision hot loops when gated to the active mode. Attempted scroll pre-dispatch was measured and rejected (regressed); keep direct loops unless branch elimination wins in benchmark.

### R20: Branch Fan-Out Reduction Pass (collision/scoring/scroll)

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 768 cases / 2179 assertions (non-bench suite)

**Work:**
1. `collision_system`: hoisted timing-grading eligibility into one top-level branch and split shape-bearing loops into graded/non-graded structural partitions (`BeatInfo` vs non-`BeatInfo`) to remove repeated mode checks in hot loops.
2. `scoring_system`: replaced per-entity `try_get<TimingGrade>` with structural timed/untimed views and consolidated tier side effects (energy + SongResults counters + multiplier source) into one helper switch.
3. `scroll_system`: replaced per-entity `try_get<WorldTransform>` with structural with/without-`WorldTransform` views for both beat and freeplay `ObstacleScrollZ` paths; removed `ObstacleTag` from these views to lower join cost.

**Perf summary (bench_systems, 30 samples):**
- scroll 10 entities: **73.11 ns → 56.57 ns** (faster)
- scroll 100 entities: **391.50 ns → 390.30 ns** (flat/slightly faster)
- scroll 1000 entities: **2.97 us → 3.58 us** (slower)
- collision 1 obstacle: **126.83 ns → 133.02 ns** (slower)
- collision 10 obstacles: **149.70 ns → 162.47 ns** (slower)
- full frame typical: **471.93 ns → 546.47 ns** (slower)
- full frame stress: **791.07 ns → 1.22 us** (slower/noisier but materially up)

**Pattern Learned:** Branch elimination via additional structural partitioning is not automatically a win in EnTT; extra join/select costs can dominate at scale. Keep benchmarking each partitioning step and reject “cleaner branch profile” changes that regress full-frame throughput.

## 2026-05-03 — Opt-123 Finalization (Keep-only-positive)

**Task:** Re-evaluate branch-reduction pass and keep only net-positive changes for this codebase.

**Decision:** Kept collision refactor; dropped scroll/scoring structural partitioning.

**Measured outcome (50 samples, before=all-3 refactors):**
- collision 1: 141.98 ns → 135.12 ns
- collision 10: 170.44 ns → 152.36 ns
- scroll 10: 75.64 ns → 86.43 ns
- scroll 100: 422.03 ns → 416.89 ns
- scroll 1000: 3.86 us → 3.05 us
- full-frame typical: 568.30 ns → 507.15 ns
- full-frame stress: 1.01 us → 901.23 ns

**Validation:** Build clean; tests pass (768 cases, 2179 assertions).

**Learning:** Structural split-by-archetype helps only when it does not multiply view passes on dense hot paths; for scroll/scoring in this codebase, fewer-pass loops outperform aggressive branch elimination.
