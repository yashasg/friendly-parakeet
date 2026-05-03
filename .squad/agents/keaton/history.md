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

