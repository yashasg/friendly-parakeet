# Keyser — History Archive

Archived detailed audit findings from Ralph rounds and pre-2026-05-04 sessions.

## 2026-05-XX — Ralph Round 10: Keyser Phase-Guard Design B Audit + Test Delta Forensic

**Loop:** Ralph performance + SOLID iteration

**Status:** ✅ Audit complete; 🔴 order regression identified (fix in flight, R11); test count claim refuted; player_input guard claim retracted

**Severity:** 🔴 High (order regression); 🟡 Medium (test count documentation error); 🟡 Low (guard claim retraction)

**Findings:**
- Runner module SOLID audit: 🟢 SRP clean, phase-gate single early-return, 11 guards confirmed dropped (grep verified zero remaining), DIP clean
- **🔴 ORDER REGRESSION found:** `popup_feedback_system` and `energy_system` moved from post-despawn into runner (R9); now run BEFORE `obstacle_despawn_system`, breaking "score-feedback chain contiguous" design intent and comment at `game_loop.cpp:188`
  - Original order: despawn → popup_feedback → popup_display → energy_system
  - New order: runner contains [... popup_feedback ... energy ...] → despawn → popup_display
  - Impact: Low behavioral risk; high design-fidelity concern
  - Action: Revert or document intentional (preferred: revert); in flight R11
- **Test count claim (−14 cases) WRONG:** Keaton claimed 795 − 16 (consolidated) + 2 = 781. Actual: 795 + 2 (new phase-guard tests) = 797. All 8 "migrated" tests were 1:1 call swaps (no consolidation). Math error in Keaton's decision doc; code is correct.
- **player_input guard claim [R10 CORRECTION]:** Keyser-r8/r9 claimed double-guard was redundant. Keaton-r10 proved wrong: guards protect event-dispatcher callbacks that fire via `game_state_system`'s `disp.update<>()` calls OUTSIDE runner. Test `test_entt_dispatcher_contract.cpp:290` fails when guards dropped.

**Pattern Learned (REVERSAL):** Audit findings labeled 'redundant' must trace ALL invocation paths, not just the most obvious one. Event-dispatcher callbacks invoke functions outside static system runner; their guards aren't redundant just because the runner gates other paths.

## 2026-05-XX — Ralph Round 8: BarObstacleTag Audit + Phase-Guard Design B Scope

**Status:** ✅ Audit complete; Design B scope documented; no 🔴 blockers

**Findings:** BarObstacleTag behavior preserved (3-test suite confirms kind-independence + else-branch); scoring_system 🟢 confirmed kind-free; Design B recommends 11-system runner

**Phase-Guard Design B:**
- 11 systems affected (not 12 — obstacle_despawn_system has no guard)
- Recommended runner: hand-written `tick_playing_systems(reg, dt)` inline `if` block
- Rationale: only option that removes duplication (not just renames it); matches existing `tick_fixed_systems` aesthetic
- Diff estimate: +3 net lines (~28 lines touched across 14 files)
- Risks: No new risks introduced; transition-tick behavior preserved; player_input_system becomes double-guarded (safe, clean up in R10)

**Pattern Codification:** Tag-replacement of enum-kind branches is now a ratified pattern with 3 successful applications (NonScorableTag, BarObstacleTag, LanePushDelta). Recommend adding playbook entry `.squad/playbooks/tag-replace-inline-enum-branch.md`.

## 2026-05-XX — Ralph Round 7: LanePush Refactor Audit + 🔴 Critical Production Bug Discovery

**Status:** ✅ Audit complete; 🔴 critical bug found; Design B recommended for r8

**Findings:** SOLID clean except pre-existing SRP mix in collision_system (`TimingGrade`/`SongResults` mutations in `resolve` lambda)

**🔴 CRITICAL FINDING:** `lane_push_response_system` declared in `all_systems.h:32` but **NEVER CALLED in `game_loop.cpp`** between `collision_system` and `miss_detection_system`. Dead code or integration fault? Verify immediately. Phase-guard consolidation (Design B) must not bury this. Action: Find + fix callsite or remove declaration.

## 2026-05-03 — Ralph Round 4: Audit motion_system + ObstacleKind Pattern

**Status:** ✅ Audit complete; SRP clean; pattern confirmed (tag replacement viable)

**Findings:**
- motion_system: 🟢 SRP clean, no ObstacleKind refs, obstacle-agnostic
- Raylib dependency: 150 LOC input layer, 200 LOC render layer, 100 LOC audio — clear separation
- ObstacleKind pattern: Confirmed viable for LanePushDelta (R6) and BarObstacleTag (R8) tag replacements

## 2026-05-03 — Ralph Round 3: scoring_system SOLID Audit

**Status:** ✅ Audit complete; SRP clean; NonScorableTag pattern validated

**Findings:**
- scoring_system: 🟢 SRP clean, no kind-specific branches, ready for tag replacement
- NonScorableTag: Behaves identically to pre-refactor `ObstacleKind::NonScorableObstacle` branch
- Test coverage: Synthetic kind proof + else-branch test confirms behavior parity

## 2026-05-03T23:47 — Raylib Dependency Audit Pass

Ran concrete dependency audit across `app/`, `tests/`, `benchmarks/`, `CMakeLists.txt`, `vcpkg.json`.

**Counts:** 60 raylib/raymath/raygui includes (55 files), 315 direct API callsites (37 files), 30 build-coupling hits.

**Classification:**
- A (pure API) dominates by count
- B/C (type + lifecycle coupling) dominate migration difficulty

**Migration Readiness:** HIGH effort due to ECS type embedding and resource lifecycle ownership patterns.

## 2026-04-29 — Gameplay shape buttons migration (revisions R3 → rejected)

**Status:** TWO-PASS REVISION CYCLE ONGOING

**Reviewer:** Kujan  
**Verdict (R3):** REJECTED (reachability regression + geometry drift)

Tap forgiveness regression: 140×100 input rectangles cannot reach required ±70px vertical forgiveness band (legacy circular geometry). Shape slot geometry diverges from `gameplay.rgl`.

**Reassignment:** Different agent (non-Keyser) to address circular hit detection and generated-layout alignment.

## 2026-04-29T21:20:03Z — ActiveTag Audit Post-raygui Migration

**Session:** Keyser Leadership Audit — ActiveTag Component Viability  
**Outcome:** DECISION — Partial Obsolescence Confirmed

Audited remaining ECS `ActiveTag` + `hit_test_handle_input` code paths to determine runtime liveness post-raygui migration.

**Findings:**
- **Live:** Gameplay shape buttons use ECS hit path (`ShapeButtonTag + HitCircle + ActiveInPhase`)
- **Legacy/Transitional:** Menu/pause/end-screen buttons now raygui-driven in screen controllers
- **Dead:** `MenuButtonTag`, `HitBox` entities, menu-action branches never spawned at runtime

## 2026-04-29T09:55:21Z — Song Complete Text Fix Attempt (Rejected, Locked Out)

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability

**Result:** ❌ REJECTED

**Reason:** No visible artifacts demonstrated the fix. Default `GuiLabel` still in use; no centered-label helper added. Acceptance criteria unmet.

**Lockout:** Per reviewer lockout protocol, next reviser must be different from Keyser.

## 2026-04-29T08:38:21Z — Difficulty RayGUI Buttons: APPROVED & ORCHESTRATED

**Status:** ✅ MERGED TO DECISIONS

- Easy/Medium/Hard converted to raygui GuiButtons in `level_select_screen_controller.cpp`
- `selected_difficulty` updates with active styling; card selection and Start preserved
- All tests pass (756 cases / 2148 assertions), zero warnings
- **Kujan final review:** ✅ APPROVED — ready to merge
- **Orchestration log written:** `.squad/orchestration-log/2026-04-29T08-38-21Z-keyser.md`
