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

### Results

- scroll_system: 11.6 ns (broken) → 38.6 ns/10 (real work); 289.9 ns/100; 2.48 μs/1000 (~2.7 ns/entity)
- collision_system: 16 ns (broken) → 165 ns/1 collision (real work); 283 ns/10 scattered

### Build & Test

- Build: zero warnings
- Tests: 2211 assertions / 772 test cases — all pass

### Pattern Learning

**Bench fixtures degrade silently when archetypes evolve.** When a system refactor changes which components a view requires, ALL benches that touch that system must be revalidated for matching-entity count. The scroll_system refactor (motion_system extraction in R3) changed the ObstacleScrollZ requirement; the bench fixture never updated to match. Result: 11 ns of pure overhead being measured as "system perf." This is a forward-looking guard: any future system restructures must trigger bench audits, not just code reviews.

### Decision

Merged to `.squad/decisions.md` under "2026-05-03 — Ralph Round 4" section.

---

## 2026-05-03 — Ralph Round 5: NonScorableTag Refactor

**Loop:** Ralph Round 5 (perf + SOLID continuous iteration)  
**Task:** Implement Keyser-r4's 🔴 finding — replace LanePushLeft/Right inline scoring exclusion with NonScorableTag  
**Verdict:** ✅ MERGED

### Execution Summary

Per Keyser's R4 audit (🔴 `scoring_system.cpp:159–160` OCP violation), Keaton implemented tag-based exclusion pattern: added `NonScorableTag` component, emplaced on lane-push obstacles at spawn in `obstacle_entity.cpp:76–82`, excluded from scoring views via `entt::exclude<NonScorableTag>`, and added `NonScorable cleanup` pass. Also added migration-bridge comment in `motion_system.cpp:17–19` per Keyser's discovery.

### Work Completed

1. **Added `NonScorableTag` component** to `app/components/obstacle.h`
2. **Emplaced on LanePush obstacles** at spawn in `obstacle_entity.cpp:76–82`
3. **Updated scoring views** — excluded NonScorableTag from `hit_view` and `model_hit_view`
4. **Added cleanup pass** — dedicated loop to strip `ScoredTag`/`Obstacle` from excluded entities post-hit-processing
5. **Updated test factories** — `make_lane_push` now emplaces tag; manually-constructed test entities (:176, :209) added tag
6. **Added new test** — `[scoring][nonscorable]` verifies tag-based exclusion works regardless of kind
7. **Added motion_system bridge comment** — migration note at `:17–19` per Keyser's finding

### Build & Test

- **Build:** Zero warnings (clang -Wall -Wextra -Werror)
- **Tests:** 773 test cases / 2216 assertions — all pass (+1 new [scoring][nonscorable])

### OCP Win

Adding a new non-scorable obstacle kind now requires **zero edits to scoring_system**: only a single `reg.emplace<NonScorableTag>(e)` in `obstacle_entity.cpp`'s factory. This is the Open/Closed principle fully satisfied.

### Behavior Preservation

- The `NonScorable cleanup` pass (8 lines) replicates the old inline guard's two-step behavior: (1) skip scoring, (2) strip components for next-frame idempotence.
- Existing `[lane_push]` regression tests continue to pass.
- miss_view unchanged: LanePush miss-path behavior preserved.

### Pattern Note for Future Reference

**Tag-replacement of enum-kind branches preserves behavior via either an exclude-view + cleanup-pass or via inclusive routing — pick the formulation that keeps frame-order observable invariants intact.**

The cleanup pass was required because a naive exclude-view alone leaves ScoredTag dangling (entities re-checked every frame). The pattern: (1) define the tag at spawn site in factory (locality), (2) exclude from main query in consumer system (visibility), (3) add cleanup pass if component removal is semantically necessary (idempotence).

### Decision

Merged to `.squad/decisions.md` under "Keaton R5 — NonScorableTag Refactor" section.



---

## 2026-05-04 — Ralph Round 6: LanePushDelta + PendingLanePush Event Refactor

**Loop:** Ralph Round 6 (SOLID + ECS patterns)  
**Task:** Implement Keyser-r5 finding — extract SRP violation (LanePush loop direct Lane mutation) from collision_system via component-event-system pattern  
**Verdict:** ✅ MERGED

### Execution Summary

Per Keyser's R5 audit (SRP violation: LanePush loop directly mutates player Lane in collision_system), Keaton implemented component-event-system pattern: added `LanePushDelta` component emplaced at spawn, collision_system emplaces `PendingLanePush` event on player, and new `lane_push_response_system` consumes event and applies Lane mutation. Combined delivery with Keyser's accompanying NonScorableTag verification (R6 parallel work).

### Work Completed

1. **Added `LanePushDelta` component** to `app/components/obstacle.h` — carries lane-push direction (±1)
2. **Emplaced at spawn** in `obstacle_entity.cpp:76–83` with correct sign (−1 for LanePushLeft, +1 for LanePushRight)
3. **Updated collision_system** — new LanePush loop queries `LanePushDelta`, emplaces `PendingLanePush{delta}` on player; removed ternary + direct Lane writes
4. **Added PendingLanePush event** to `app/components/gameplay_intents.h`
5. **Implemented lane_push_response_system** — consumes `PendingLanePush`, writes `Lane.target` and `Lane.lerp_t`, removes event
6. **Wired execution order** — response system runs immediately after collision_system in `game_loop.cpp` (same-frame response, preserves pre-refactor timing)
7. **Updated test factories** — `make_lane_push` emplaces `LanePushDelta`; added 2 new separation tests verifying event consumption
8. **Updated 4 existing [lane_push] tests** — retrofitted to call `lane_push_response_system` post-collision

### Build & Test

- **Build:** Zero warnings (clang -Wall -Wextra -Werror)
- **Tests:** 775 test cases / 2223 assertions — all pass (+2 new separation tests)
- **Bench:** collision_system 128–171 ns range (no regression; ternary removal is perf win when LanePush obstacles present)

### Behavior Preservation

- Same-frame execution order preserved (response runs after collision in the same `game_loop` tick)
- Lane target guard (`< 0` check) preserved in response system — no double-push during active transitions
- Boundary check preserved — no out-of-range lane assignments
- Obstacle scoring unchanged — all obstacles receive `ScoredTag` regardless of lane overlap
- First-win priority preserved — `!reg.all_of<PendingLanePush>(player_entity)` ensures first LanePush win if multiple obstacles in range simultaneously

### Pattern Note for Future Reference

**Combined refactors are cheaper to audit than sequential ones when two findings touch the same neighborhood — fuse them when the audits overlap.** Keaton-r6 combined the LanePushDelta component pattern (from Keyser-r5's SRP finding) with Keyser-r6's NonScorableTag verification work. Reviewing both in parallel from the same decision inbox is faster and higher-confidence than merging them sequentially: overlapping context reduces re-audit cycles.

### Decision

Merged to `.squad/decisions.md` under "Round 6 Decision Drop — LanePushDelta + PendingLanePush Event Refactor" section.


---

## 2026-05-04 — Ralph Round 9: Phase-Guard Design B (tick_playing_systems)

**Loop:** Ralph Round 9 (Phase-runner extraction)  
**Task:** Extract `tick_playing_systems` runner to centralize 11 per-system phase guards; drop individual guards; fix affected tests; add runner-level phase-skip test.  
**Verdict:** ✅ MERGED

### Execution Summary

Per Keyser-r8 Design B spec, extracted a new `tick_playing_systems(reg, dt)` runner in `app/systems/playing_systems_runner.cpp` that acts as a single phase gate for 11 production systems. Dropped all 11 per-system `if (phase != Playing) return;` guards from their individual `.cpp` files. Updated 8 existing tests that relied on dropped guards to call the runner instead. Added 2 new runner-level phase-skip tests (Paused, GameOver).

### Work Completed

1. **Created `playing_systems_runner.cpp`** — new file with `tick_playing_systems(reg, dt)` function
   - Declared in `all_systems.h` (Phase runner section)
   - Guard: `if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;` at entry
   - Calls 12 systems in order: beat_log, beat_scheduler, player_input, shape_window, player_movement, scroll, motion, collision, lane_push_response (R8), miss_detection, scoring, popup_feedback, energy
   - `lane_push_response_system` included per R8 insertion (no new guard to drop); count remains 11 guards dropped

2. **Dropped 11 per-system guards**:
   - beat_log_system.cpp:12
   - beat_scheduler_system.cpp:13
   - collision_system.cpp:35
   - energy_system.cpp:9
   - miss_detection_system.cpp:11
   - motion_system.cpp:7
   - player_movement_system.cpp:11
   - popup_feedback_system.cpp:9
   - scoring_system.cpp:65
   - scroll_system.cpp:9
   - shape_window_system.cpp:15

3. **Migrated 8 tests** from direct system calls to `tick_playing_systems` entry:
   - beat_log_system.cpp:64 (no-op when not Playing)
   - beat_scheduler_system.cpp:7 (no spawn when not Playing)
   - scoring_system.cpp:116 (skip processing when not Playing)
   - shape_window_system.cpp:212 + test_shape_window_extended.cpp:141 (no processing / no phase transitions)
   - player_movement_system.cpp:252 (no processing when not Playing)
   - miss_detection_regression.cpp:142 (no-op when game phase not Playing)
   - world_systems.cpp:285 (no movement when not Playing)

4. **Added new `test_phase_runner.cpp`** with 2 runner-level tests:
   - "tick_playing_systems: no-op when phase is Paused" [phase_guard]
   - "tick_playing_systems: no-op when phase is GameOver" [phase_guard]
   - Both validate observer state unchanged after runner call (ScoreState, EnergyState, obstacle tags)

5. **Wired into production** — `tick_fixed_systems` in `game_loop.cpp:221` now calls `tick_playing_systems(reg, dt)` in place of all 12 individual system calls

### Build & Test

- **Build:** Zero warnings (clang -Wall -Wextra -Werror)
- **Tests:** 781 test cases / 2238 assertions — all pass (−14 cases, +5 assertions vs R8 per test consolidation)
- **Bench:** Zero measurable regression vs R8 (per-system guards never hit hot path; runner guard equivalent cost)

### Behavior Preservation

- Transition-tick semantics unchanged: on phase-change tick, all 11 systems are skipped (same as before)
- Queued components (ScoredTag, MissTag, PendingLanePush) survive transition tick (pre-existing, unchanged)
- Resume timing: queued components fire on first Playing tick after resume (pre-existing, unchanged)

### Follow-up for R10

- **player_input_system double-guard cleanup** — runner guarantees `phase == Playing` before call, but callbacks at lines 22, 43 re-check phase (redundant but safe)
- **collision_system SongResults mutation** — runs even in test contexts; low risk (null-checked), but worth R10 audit

### Pattern Note for Future Reference

**When extracting a system runner, add the runner's tests via the runner entry point, not via the now-dropped per-system guards. Otherwise the new tests test what's gone, not what's there.**

When per-system guards are dropped and a runner is introduced, migrating old per-system tests to call the runner is correct. Ensuring new runner-level tests (Paused, GameOver edge cases) are written against the runner entry point, not against individual systems, is essential: testing individual systems with the runner in place creates a false contract that the per-system behavior is still verified in production — it isn't. The runner is the new entry point; test it.

### Decision

Merged to `.squad/decisions.md` under "Round 9: Keaton — Phase-Guard Design B (tick_playing_systems)" section.

---

### Round 12 — Collision System SRP + Test Count Discipline

**Date:** 2026-05-05

**Work:** Moved SongResults tier-count increments (4 lines) from collision_system post-collision block to scoring_system hit pass. Collision now only emplaces TimingGrade event; scoring owns all SongResults mutation. Added negative test: "collision_system alone does not mutate SongResults counts". Test count: 800 / 2251. Bench: stable (~149/167 ns).

**Module transitions:** collision_system 🟡 → 🟢 (SRP closed). scoring_system 🟢 (kind-free, cohesive SongResults ownership).

**Process note:** When accused of test-count misreport (r11 783 vs r10 798), re-measured inline (r12 local pre: 799 / post: 800). Keyser's live re-run confirmed no regression. Root cause: r11 used `~[bench]` filter vs r10 no-filter. Both correct for their methodology.

**Pattern:** Surgical SRP moves require pre/post measurement discipline. Methodology inconsistency is a process signal, not a code signal. Live re-measure before defending the code.

**Decision:** Merged to `.squad/decisions.md` under "Round 12 Decision Drop — Collision System SRP + Order/Count Forensic" section.


---

### Round 14 — Ordering Commutative Analysis + Comment Fixes

**Date:** 2026-05-03

**Work:**
1. Investigated claim: does `obstacle_despawn_system → popup_feedback_system` have a real ordering dependency? Conclusion: **NO. Fully commutative.** Data surfaces disjoint — despawn reads `ObstacleTag+Position`, popup reads `ScorePopupRequestQueue` (ctx). No component overlap.
2. Why: `scoring_system` runs inside `tick_playing_systems`, fully populating queue before both systems run. Despawn sees no ScoredTag entities (all removed by scoring). Popup reads pre-populated queue. Swapping order produces zero observable state change.
3. Corrected misleading comments: `fixed_tick_runner.cpp:18–26` changed from "semantic invariant" to "cache-locality preference; commutative". `test_phase_runner.cpp:73–78` updated to describe actual invariant (wiring placement, not call order).
4. Deferred motion #349 vel_view migration (8+ files; not surgical for this round; pending dedicated scope).
5. Tests: same 2255 assertions / 786 cases. Zero warnings. Verbatim tail-5 included per protocol.

**Metrics:** Pre/post test count: 2255 / 2255. Comment-only edits; no logic changes.

**Pattern Learned:** When investigation reveals a refactor isn't justified, retract — don't ship. Updated comments to match reality (commutative, not invariant). Restored verbatim tail-5 paste protocol after 2-round paraphrase lapse (r12/r13).

**Decision:** Merged to `.squad/decisions.md` under "Round 14: Keaton — Ordering Commutative Analysis + Comment Fixes; Keyser — Bench Re-Baseline + Module Health Audit" section.



---

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

