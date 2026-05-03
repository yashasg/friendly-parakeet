# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Learnings

### 2026-05-03 — Ralph Round 10: Phase-Guard Design B Audit + 🔴 Order Regression Discovery + player_input Retraction

**Loop:** Ralph performance + SOLID iteration  
**Task:** Full SOLID audit of `tick_playing_systems` runner (R9 Keaton); forensic analysis of test count claim; verify player_input double-guard claim  
**Status:** ✅ Audit complete; 🔴 order regression identified (fix in flight, R11); test count claim refuted; player_input guard claim retracted  
**Severity:** 🔴 High (order regression); 🟡 Medium (test count documentation error); 🟡 Low (guard claim retraction)

**Result:**
- Runner module SOLID audit: 🟢 SRP clean, phase-gate single early-return, 11 guards confirmed dropped (grep verified zero remaining), DIP clean
- **🔴 ORDER REGRESSION found:** `popup_feedback_system` and `energy_system` moved from post-despawn into runner (R9); now run BEFORE `obstacle_despawn_system`, breaking "score-feedback chain contiguous" design intent and comment at `game_loop.cpp:188`
  - Original order: despawn → popup_feedback → popup_display → energy_system
  - New order: runner contains [... popup_feedback ... energy ...] → despawn → popup_display
  - Impact: Low behavioral risk; high design-fidelity concern
  - Action: Revert or document intentional (preferred: revert); in flight R11
- **Test count claim (−14 cases) WRONG:** Keaton claimed 795 − 16 (consolidated) + 2 = 781. Actual: 795 + 2 (new phase-guard tests) = 797. All 8 "migrated" tests were 1:1 call swaps (no consolidation). Math error in Keaton's decision doc; code is correct.
- **player_input guard claim [R10 CORRECTION]:** Keyser-r8/r9 claimed double-guard was redundant. Keaton-r10 proved wrong: guards protect event-dispatcher callbacks that fire via `game_state_system`'s `disp.update<>()` calls OUTSIDE runner. Test `test_entt_dispatcher_contract.cpp:290` fails when guards dropped.
- Module health: playing_systems_runner 🔴 (order regression in flight); fixed_tick_runner 🟢 (new test infrastructure)

**Pattern Learned (REVERSAL):** **Audit findings labeled 'redundant' must trace ALL invocation paths, not just the most obvious one.** Event-dispatcher callbacks invoke functions outside the static system runner; their guards aren't redundant just because the runner gates other paths. Burn the lesson: trace `dispatcher.update<...>()` before declaring a guard redundant. Redundancy claims on guards require either (a) proof that all calling sites are gated (including indirect dispatcher paths), or (b) a failing test when the guard is removed.

**Decision:** `.squad/decisions.md` (merged from inbox, Round 10); Keaton-r9 decision doc requires annotation with order regression note and test count correction

---

### 2026-05-XX — Ralph Round 8: BarObstacleTag Audit + Phase-Guard Design B Scope

**Loop:** Ralph performance + SOLID iteration  
**Task:** Audit BarObstacleTag refactor (R7 Keaton); detailed scope for Design B phase-guard runner (R9)  
**Status:** ✅ Audit complete; Design B scope documented; no 🔴 blockers  
**Findings:** BarObstacleTag behavior preserved (3-test suite confirms kind-independence + else-branch); scoring_system 🟢 confirmed kind-free; Design B recommends 11-system runner

**BarObstacleTag Audit:**
- Behavior preservation: ✅ spawn-time emplacement (obstacle_entity.cpp:48,57) matches pre-refactor kind checks exactly
- Test thoroughness: ✅ synthetic kind proof (ShapeGate+tag) + else-branch test (ShapeGate, no tag)
- scoring_system module health: 🟢 zero `ObstacleKind::` refs remain
- Pattern codification: Third tag-replacement (NonScorableTag R5, LanePushDelta R6, BarObstacleTag R7) — recipe is stable and repeatable

**Phase-Guard Design B:**
- 11 systems affected (not 12 — obstacle_despawn_system has no guard)
- Recommended runner: hand-written `tick_playing_systems(reg, dt)` inline `if` block
- Rationale: only option that removes duplication (not just renames it); matches existing `tick_fixed_systems` aesthetic
- Diff estimate: +3 net lines (~28 lines touched across 14 files)
- Risks: No new risks introduced; transition-tick behavior preserved; player_input_system becomes double-guarded (safe, clean up in R10)

**Pattern Codification:** Tag-replacement of enum-kind branches is now a ratified pattern with 3 successful applications (NonScorableTag, BarObstacleTag, LanePushDelta). Recommend adding playbook entry `.squad/playbooks/tag-replace-inline-enum-branch.md` with recipe for future applications.

**Decision:** `.squad/decisions.md` (merged from inbox, Round 8)

---

### 2026-05-XX — Ralph Round 7: LanePush Refactor Audit + 🔴 Critical Production Bug Discovery

**Loop:** Ralph performance + SOLID iteration  
**Task:** Post-Keaton-r6 SOLID audit of lane_push refactor + phase-guard consolidation design  
**Status:** ✅ Audit complete; 🔴 critical bug found; Design B recommended for r8  
**Findings:** SOLID clean except pre-existing SRP mix in collision_system (`TimingGrade`/`SongResults` mutations in `resolve` lambda)

**🔴 CRITICAL FINDING:**
- `lane_push_response_system` declared in `all_systems.h:32` but **NEVER CALLED in `game_loop.cpp`** between `collision_system` and `miss_detection_system`
- `PendingLanePush` events accumulate indefinitely; first-obstacle-wins guard fires on every frame after first contact, permanently blocking future LanePush events
- Lane.target never updated in production; mechanic fully broken
- **Root cause:** Tests self-wired the system, masking the missing production call

**Phase-Guard Consolidation Design (Design B):**
- 13+ systems each check `GamePhase::Playing` independently (14 guard sites total)
- Design B: central phase-gated runner in `game_loop.cpp` removes guards from ~12 system bodies
- Only option that actually removes duplication (not just renames it)
- Design A (helper fn) and C (ctx tag) only reduce boilerplate, keep scatter across systems
- Estimated impact: +6 net lines, ~27 lines touched across 14 files

**Generalized Anti-Pattern:** **Self-wiring tests can mask production bugs by short-circuiting the very integration the test is meant to verify.** When a refactor introduces a new system, demand at least one test that calls the production tick path, not the system directly. This is the litmus test for "is the system actually wired?" — if the test passes when the production call is deleted, the test was measuring the wrong thing.

**Test Coverage Gap (noted but not blocking):**
- Multi-obstacle delta-overwrite test missing (first-obstacle-wins guard semantic untested)
- Recommend as follow-up, not part of r7 scope

**Decision:** `.squad/decisions.md` (merged from inbox, Round 7)  
**Follow-up:** Keaton-r8-wirefix (in flight) adds missing `lane_push_response_system` call; Keyser-r8-bartag-and-scope scoping Design B refactor for round 9

---

### 2026-05-03 — Ralph Round 2: scroll_system Post-Refactor SOLID Audit

**Loop:** Ralph perf+SOLID iteration (Round 2)  
**Task:** Audit scroll_system after Keaton's view consolidation (refactor pre-dating Ralph loop)  
**Verdict:** Module health: 🟡 (yellow — SRP minor smell, otherwise clean)

**Findings:**
- **S (SRP) 🟡:** Function carries two concerns: rhythm obstacle scrolling (`model_beat_view` + `beat_view`) + general entity motion (`vel_view` + `motion_view` without ObstacleTag constraint). The "scroll" system doubles as motion integrator for particles/popups.
- **O (Open/Closed) 🟡:** New motion modes add view+loop blocks inside scroll_system; concern divergence already visible.
- **L (Liskov), I (Interface), D (Dependency) 🟢:** Clean — no virtual dispatch, views claim exactly what they use, singleton resolution is canonical.

**Recommendation:** Extract `vel_view` + `motion_view` into separate `motion_system`. scroll_system narrows to obstacle-only; motion_system owns legacy Position+Velocity bridge + WorldTransform+MotionVelocity integration. No production breakage — views are independent of rhythm views with no shared state.

**Action (in-flight):** Keaton round 3 will extract motion_system in parallel. Scribe round 3 will merge.

**Decision:** `.squad/decisions.md` (merged from inbox, Round 2)

**Generalized Anti-Pattern:** **Tagless view pairs (vel_view, motion_view without component filter) hidden inside named "rhythm" systems.** When splitting systems, verify constraints at each view site. If a system carries concerns, its views should make that visible via explicit filtering (tags, component subsets). Otherwise, layered or unrelated systems accumulate in one function — a strong SRP smell.

### 2026-04-29T08:05:08Z — Root UI Surface Cleanup & Test-Only Component Removal (Session)
- ActiveTag/ActiveInPhase currently remain runtime-live only for gameplay shape buttons (`ShapeButtonTag + HitCircle`) consumed by `hit_test_handle_input`.
- Archetype-to-entity migration rule: when runtime and tests call `app/entities/*_entity.h` factories directly, any remaining `app/archetypes/` forwarding header is dead surface and should be removed.
- Decommission checklist for namespace-folder migrations: remove include shims, remove CMake source globs for the retired folder, and scrub stale docs/comments that still mark the old folder as canonical.
- Verification command for this migration class: `cmake -B build -S . -Wno-dev && cmake --build build --target shapeshifter_tests && ./build/shapeshifter_tests "[archetype]"`.

- Gameplay HUD restoration should read live `ShapeButtonTag + ShapeButtonData + UIPosition` entities first, then fall back to layout-derived defaults; this keeps visible buttons aligned with actual input-hit targets.
- Keep gameplay HUD dynamic behavior (shape buttons, approach rings, segmented energy feedback) inside `gameplay_hud_screen_controller.cpp` while leaving generated rguilayout header focused on static controls like pause.
- Compact, multi-stop segmented energy bars with critical pulse + flash overlays better preserve readability on 720x1280 than oversized single-fill bars.
- Root UI cleanup (post-rguilayout activation): classified `ui_loader` + `text_renderer` + `ui_button_spawner` as live runtime dependencies, `ui_source_resolver` as live test-only dependency, and `tests/test_ui_spawn_malformed.cpp` as dead legacy spawn-surface documentation.
- Applied surgical cleanup: removed `tests/test_ui_spawn_malformed.cpp`, excluded `app/ui/ui_source_resolver.cpp` from `shapeshifter_lib` runtime build while compiling it directly into `shapeshifter_tests`, and updated root UI comments to reflect the active screen-controller path.
- Key files changed: `CMakeLists.txt`, `app/ui/ui_loader.cpp`, `app/ui/ui_source_resolver.h`, deleted `tests/test_ui_spawn_malformed.cpp`.
- Validation: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass: 867 test cases).
- Second-pass dead-surface cleanup: deleted test-only `app/ui/ui_source_resolver.*`, removed legacy `app/components/ui_element.h`, and retired resolver-only tests (`tests/test_ui_source_resolver.cpp` plus resolver assertions in game-state/high-score/redfoot tests) because runtime now renders via screen controllers without JSON dynamic-source resolution.
- Kept live runtime dependencies untouched (`ui_loader`, `text_renderer`, `ui_button_spawner`, level-select + screen controllers, navigation/render systems) and refreshed stale loader/render comments to reflect the active rguilayout controller path.
- Validation and dead-surface proof: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` passed (848 test cases); no app/tests/CMake references remain to `ui_source_resolver`, legacy `UIElement*` components, `spawn_ui_elements`, `app/ui/vendor`, or generated standalone exports.

### 2026-04-29T08:05:08Z — Root UI Surface Cleanup & Test-Only Component Removal (Session)

**Tasks:**
1. `keyser-root-ui-cleanup` — First-pass audit of root `app/ui/` files; classified live dependencies vs. test-only vs. dead
2. `keyser-test-only-ui-leftovers` — Second-pass cleanup; deleted runtime-dead `ui_source_resolver.*`, `ui_element.h`, legacy tests; kept all live dependencies

**Outcome:** ✅ Completed
- Identified live runtime: `ui_loader`, `text_renderer`, `ui_button_spawner`, `level_select_controller`, all 8 screen controllers
- Moved `ui_source_resolver.cpp` to test sources (test-only)
- Deleted `ui_element.h` and `test_ui_spawn_malformed.cpp` (no runtime use)
- Updated stale comments and empty vendor dir notes
- Validated native + unity builds pass (867 tests, zero warnings)

**Orchestration logs:**
- `.squad/orchestration-log/2026-04-29T08:05:08Z-keyser-first-pass.md`
- `.squad/orchestration-log/2026-04-29T08:05:08Z-keyser-second-pass.md`


- 2026-04-29: Removed legacy runtime UI JSON loader and invisible ECS menu-hitbox spawner paths.
- Replacement path: ui_navigation now only maps `GamePhase -> ActiveScreen` (+ paused overlay flag), while title/level-select/paused/game-over/song-complete interactions are handled by raygui screen controllers (including direct pointer hit-testing for title tap-to-start and level card/difficulty selection).
- Key files changed: deleted `app/ui/ui_loader.cpp/.h`, deleted `app/ui/ui_button_spawner.h`, updated `app/game_loop.cpp`, `app/systems/ui_navigation_system.cpp`, `app/systems/game_state_system.cpp`, `app/input/game_state_routing.cpp`, `app/ui/level_select_controller.*`, `app/ui/screen_controllers/{title,level_select,paused}_screen_controller.cpp`, `app/components/ui_state.h`, `app/components/ui_layout_cache.h`, plus removal/updates of legacy loader/spawner tests.
- Validation: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass: 753 test cases).

### 2026-04-29T08:23:46Z — Legacy UI Removal Orchestration & Approval

**Session:** Legacy UI Removal Final Wave
**Task:** keyser-remove-legacy-ui-runtime (orchestration + implementation summary)
**Status:** ✅ COMPLETED & APPROVED

**Work Summary:**
- Implemented full removal of `ui_loader`, `ui_button_spawner`, `ui_source_resolver`, and `ui_element` component per user directive
- Resolved all live references; migrated menu interactions to raygui screen controllers
- Coordinated with Kujan on final validation gate

**Artifacts:**
- Orchestration log: `.squad/orchestration-log/2026-04-29T08-23-46Z-keyser.md`

**Verification:**
- All 753 tests pass; zero warnings
- 6 legacy files deleted, zero references remain
- Screen-controller coverage complete (title, level-select, paused, game-over, song-complete)

**Next:** Ready for merge.

- 2026-04-29: Root cause of non-working Easy/Medium/Hard was controller hit-test ordering: level-card collision consumed clicks before difficulty regions, and difficulty UI was drawn labels/rects without raygui button interaction.
- Fix: moved difficulty selection to controller-owned raygui `GuiButton` controls per difficulty chip, kept level-card selection as controller hit region, and preserved Start button confirm flow.
- Files changed: `app/ui/screen_controllers/level_select_screen_controller.cpp`, `tests/test_level_select_controller.cpp`.
- Validation: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` plus focused `./build/shapeshifter_tests "[level_select]"` and `./build/shapeshifter_tests "[ui]"`.

## 2026-04-29T08:38:21Z — Difficulty RayGUI Buttons: APPROVED & ORCHESTRATED

**Status:** ✅ MERGED TO DECISIONS

- Spawn: `keyser-fix-difficulty-raygui-b` completed
- Easy/Medium/Hard converted to raygui GuiButtons in `level_select_screen_controller.cpp`
- `selected_difficulty` updates with active styling; card selection and Start preserved
- All tests pass (756 cases / 2148 assertions), zero warnings
- **Kujan final review:** ✅ APPROVED — ready to merge
- **Orchestration log written:** `.squad/orchestration-log/2026-04-29T08-38-21Z-keyser.md`
- 2026-04-29: Song Complete text regression cause was missing raygui style overrides in `song_complete_screen_controller`; labels were rendered with default small font and non-centered label alignment.
- Fix: wrapped `song_complete_controller.render()` with scoped `GuiSetStyle(DEFAULT, TEXT_SIZE, 28)` and `GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER)`, then restored prior style values.
- Files changed: `app/ui/screen_controllers/song_complete_screen_controller.cpp`.
- Validation: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]' && ./build/shapeshifter_tests '[song_complete]' && ./build/shapeshifter_tests '[ui]'`.

## 2026-04-29T09:55:21Z — Song Complete Text Fix Attempt (Rejected, Locked Out)

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Task:** Fix Song Complete text rendering per Redfoot's user-reported defect (tiny, non-centered labels).
**Submission:** Modified `app/ui/generated/song_complete_layout.h` and `app/ui/screen_controllers/song_complete_screen_controller.cpp`.

**Result:** ❌ REJECTED

**Reason:** No visible artifacts demonstrated the fix. Default `GuiLabel` still in use; no centered-label helper added. Acceptance criteria unmet.

**Lockout:** Per reviewer lockout protocol, next reviser must be **different from Keyser**. Recommended: non-Keyser architect/tools engineer with song-complete expertise.

**Related:**
- Orchestration log: `.squad/orchestration-log/2026-04-29T09:55:21Z-keyser.md`
- Decision merged: Song Complete & Pause Screen Text Readability Fixes (decisions.md)

## 2026-04-29T21:20:03Z — ActiveTag Audit Post-raygui Migration

**Session:** Keyser Leadership Audit — ActiveTag Component Viability
**Outcome:** DECISION — Partial Obsolescence Confirmed
**Decision ID:** #168

Audited remaining ECS `ActiveTag` + `hit_test_handle_input` code paths to determine runtime liveness post-raygui migration.

**Findings:**
- **Live:** Gameplay shape buttons use ECS hit path (`ShapeButtonTag + HitCircle + ActiveInPhase`)
- **Legacy/Transitional:** Menu/pause/end-screen buttons now raygui-driven in screen controllers
- **Dead:** `MenuButtonTag`, `HitBox` entities, and menu-action branches never spawned at runtime

**Decision:** Keep `ActiveTag` for gameplay shapes; deprecate menu ECS branches.

**Migration Guidance (Future Work):**
1. Split `hit_test_handle_input` into gameplay-only path
2. Replace `ActiveInPhase`/`ActiveTag` with simpler `GamePhase::Playing` guard
3. Update synthetic menu-button tests
4. Preserve keyboard/semantic routing during transition

**Artifacts:**
- Orchestration: `.squad/orchestration-log/2026-04-29T21:20:03Z-keyser.md`
- Session: `.squad/log/2026-04-29T21:20:03Z-active-tag-raygui-audit.md`
- Decision: Merged to `.squad/decisions.md` as #168
- Gameplay HUD shape controls can stay raygui-owned by using invisible rguilayout button slots for input while preserving circular tap semantics in the controller: center from slot bounds, `visual_radius = slot.width/2.8f`, `hit_radius = visual_radius*1.4f`.
- Regression guard for HUD tap geometry: `[input_pipeline][hud]` now asserts vertical offsets of +60 and +70 from slot center are accepted while +71 is rejected, preventing fallback to rectangular 140x100 hit behavior.

## 2026-04-29 — Gameplay shape buttons migration (revisions R3 → rejected)

**Status:** TWO-PASS REVISION CYCLE
**Reviewer:** Kujan
**Verdict (R3):** REJECTED (production reachability blocked)
**Verdict (post-feedback):** REJECTED (geometry source drift 60/220/380 vs 90/290/490)

**Revision 1 (Circular Acceptance Filter):**
- Added explicit circular acceptance filter (`CheckCollisionPointCircle`) before `ButtonPressEvent` dispatch
- Preserved 1.4× forgiveness (hit_radius ~70) while keeping raygui HUD ownership
- Design: raygui bounds capture raw press; circular filter applied after raygui reports press

**Kujan feedback (R3):** Circular filter math correct in isolation, but **production acceptance gate blocked**: raygui input bounds (140×100 rectangles at y=1140..1240) cannot reach center.y±70 taps required for legacy 1.4× behavior. Must expand raw raygui bounds or provide alternative production path.

**Revision 2 (Raw Bounds Investigation):**
- Attempted to expand raw raygui input bounds to enclose the circular hit radius
- Discovered upstream geometry source drift: `gameplay.rgl` slots define x=60/220/380, but generated header hard-codes x=90/290/490
- Mismatch violates source-of-truth requirement and blocks safe regeneration

**Reassignment:** Baer (Test Engineer, non-locked) to establish reachability contract and implement raw bounds expansion from correct geometry source.

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-keyser.md`

- 2026-04-29: Post-HUD migration audit confirms ActiveTag/HitBox/HitCircle/UIActiveCache path is runtime-dead; gameplay input now flows via raygui HUD direct ButtonPressEvent dispatch + keyboard semantic events.

## 2026-05-03 — Ralph Round 3: scoring_system SOLID Audit

**Loop:** Ralph Round 3 (perf + SOLID continuous iteration)  
**Task:** Comprehensive SOLID audit of scoring_system post-Keaton-optimization  
**Verdict:** 🟡 Yellow (two actionable 🟡 items: SRP, OCP; no 🔴)

### Execution Summary

Keyser conducted a full SOLID audit of `app/systems/scoring_system.cpp` to identify architectural improvements and module quality post-Keaton's Round 2 optimization. The system passed most principles but revealed two actionable violations related to mixed concerns and hardcoded branching.

### Audit Results

| Principle | Status | Finding |
|---|---|---|
| **S (Single Responsibility)** | 🟡 | Mixes four concerns: (1) score/chain computation, (2) miss processing, (3) hit processing, (4) `displayed_score` animation interpolation. Popup queue writes (`:207`) are presentation concerns; `displayed_score` smoothing (`:218–223`) is rendering—its only consumer is `gameplay_hud_screen_controller.cpp:339`. |
| **O (Open/Closed)** | 🟡 | New obstacle kinds need hardcoded `LanePushLeft`/`LanePushRight` guard edits (`:158–163`); new combo rules need edits to chain-bonus ladder (`:192–196`); energy tier→delta mapping is inline switch (`:170–184`). Design docs call for data-driven patterns. |
| **L (Liskov Substitution)** | 🟢 | No inheritance/polymorphism. Clean. |
| **I (Interface Segregation)** | 🟢 | All views claim exactly what they use. miss_view correctly filters for ObstacleTag+ScoredTag+MissTag+Obstacle; hit_view uses Obstacle+Position; model_hit_view uses Obstacle+ObstacleScrollZ. |
| **D (Dependency Inversion)** | 🟡 | Direct coupling to `popup_queue_for(reg)` (`:55–60`, `:155`) and `enqueue_energy_effect` (`:50–53`). Scoring computation tightly bound to presentation message queue. |

### Key Finding: Behavior Preservation Verified ✅

Keaton's Round 2 claim (deferring popup_queue_for lookup behind `!hit_buf.empty()` guard) was verified:
- Miss pass never calls popup_queue_for; deferral is safe
- Guard skips lookup on zero-hit frames; identical behavior on frames with hits
- popup_feedback_system has null-check guard; no functional regression

### Top Improvement Priority

**Extract `displayed_score` interpolation (`:218–223`) to a dedicated `score_display_system`.** This is pure rendering/UI concern with one consumer (HUD controller) — clearest SRP violation with zero downside.

### Cross-System Pattern Surfaced

**Hardcoded kind-checks vs data-driven dispatch:**
- scroll_system: structural over-breadth (swept all moving entities)
- scoring_system: hardcoded obstacle-kind exclusions inline (`:158–163`)

Both systems prefer branching over tag/flag-driven dispatch. This pattern was invisible at the scroll_system or scoring_system level alone but emerges clearly in cross-system audit.

### Module Health: 🟡 Yellow

Two actionable improvements (SRP, OCP); no blockers. Tests pass, warnings zero, Keaton's behavior preservation holds.

### Pattern Note for Future Reference

**Cross-system audits surface refactor candidates faster than per-module audits — `ObstacleKind` hardcoded branches were invisible at the scroll_system or scoring_system level alone.** When multiple systems show the same hardcoding anti-pattern (kind-checks, tier mappings), a unified data-driven refactor across systems is more valuable than point fixes in individual systems. Consider a "Bullet Pattern Registry" or "Obstacle Metadata" singleton that all systems query (read-only, pre-baked at startup) instead of embedding branching logic.

### Decision

Merged to `.squad/decisions.md` under "2026-05-03 — Ralph Round 3" section.

### Next: Round 4 Planning

Keaton R4 (perf focus): Fix bench fixture to include ObstacleScrollZ entities; audit ObstacleTag filtering on motion_system views.  
Keyser R4 (audit focus): Cross-system "hardcoded kind-check" pattern refactor design.

---

## 2026-05-03 — Ralph Round 4: Audit motion_system + ObstacleKind Pattern

**Loop:** Ralph Round 4 (perf + SOLID continuous iteration)  
**Task:** SOLID audit of motion_system + codebase-wide cross-cutting ObstacleKind dispatch audit  
**Verdict:** ✅ MERGED

### Execution Summary

Keyser conducted comprehensive audits of motion_system (post-Keaton's R3 extraction) and performed a codebase-wide cross-cutting audit of ObstacleKind branching across all 7 files. The audit surfaced a single 🔴 OCP violation in scoring_system and recommended it as the first refactor target.

### Work Completed

1. **motion_system SOLID audit:** Classified SRP 🟡 (migration bridge embedded in loop), other principles 🟢
2. **Codebase-wide ObstacleKind audit:** 13 branch sites across 7 files; 1 🔴, 11 🟡, 1 🟢
3. **First refactor design:** NonScorableTag pattern for scoring_system.cpp:159–160

### Findings Summary

**motion_system:** 🟡 yellow. The try_get → WorldTransform sync at `:17–19` is migration plumbing (Position → WorldTransform copy) living inside integration logic. Should be extracted or labelled as transient. No functionality risk; SRP smell only.

**ObstacleKind Dispatch:**
- 🟢 sites (3): Debug utility (enum_names), canonical deserialization (beat_map_loader), factory pattern (obstacle_entity)
- 🟡 sites (10): Temporary workarounds (2), parse-time routing (1), render factory (2), scheduler compute (2), camera/collision/scoring data mappings (3)
- 🔴 site (1): **scoring_system.cpp:159–160** — inline LanePushLeft/Right exclusion from scoring violates OCP. Adding future non-scoring kinds requires editing scoring_system.

### Recommended First Action

**NonScorableTag pattern for scoring_system.cpp:159–160:**
1. Add `struct NonScorableTag {};` to `app/components/obstacle.h`
2. Emplace on LanePushLeft/Right in `obstacle_entity.cpp:76–79`
3. Replace inline guard with `entt::exclude<NonScorableTag>` on hit_view
4. Delete the kind check

**Impact:** Branch disappears entirely. Future non-scoring obstacle kinds need only an emplace call in the factory — no gameplay system edits required.

### Pattern Discovery

**Codebase-wide cross-cutting audits surface 🔴 findings that single-module audits miss.** The inline kind-check at scoring_system:159–160 is the same smell as R3 audit site `:158–163`. When multiple systems show hardcoded enum branching, a unified data-driven refactor is more valuable than point fixes.

### Decision

Merged to `.squad/decisions.md` under "2026-05-03 — Ralph Round 4" section.

---

## 2026-05-04 — Ralph Round 5: collision_system Audit

**Loop:** Ralph Round 5 (perf + SOLID continuous iteration)  
**Task:** SOLID audit of collision_system post-Keaton-r4 optimization; verify behavior preservation of 1D lane-collapse  
**Verdict:** 🟡 Yellow (two SRP findings; no blockers; behavior-preserving audit confirms Keaton-r4 safe)

### Execution Summary

Keyser conducted comprehensive SOLID audit of `app/systems/collision_system.cpp` post-Keaton-r4 perf optimization (1D lane-overlap collapse, frame-constant hoist, dead helper removal). Verified mathematical correctness of the collapse, confirmed all removed helpers were genuinely dead, and surfaced two SRP violations amenable to component-based refactoring in future rounds.

### Audit Results: SOLID Principles

| Principle | Status | Finding |
|---|---|---|
| **S (SRP)** | 🟡 | Two violations: (1) resolve lambda (:55–109) conflates five concerns (timing gate, tagging, rhythm grade, SongResults mutation, ShapeWindow mutation); (2) LanePush loop (:173–193) directly mutates player Lane component — movement response in a detection system. |
| **O (OCP)** | 🟡 | Per-kind view blocks; adding new collision logic requires editing collision_system. Carry-forward: LanePushDelta component removes :188 ternary (same pattern as Keaton-r5 NonScorableTag). |
| **L (Liskov)** | 🟢 | No inheritance/polymorphism. Clean. |
| **I (Interface)** | 🟢 | Views structurally narrow per kind; no overwide interfaces. Minor: dead `#include <raylib.h>` at :10 (left over after helper removal). |
| **D (Dependency)** | 🟢 | Singleton resolution canonical; no file-statics. |

### Section 2 — Behavior Preservation Verification ✅

#### 2a. Frame-constant hoist: SAFE

**Verdict:** Safe. `player_timing_y` and `player_x` precomputed once before loops (:51–52). Obstacle loops never mutate `WorldTransform` or `VerticalState` — only `Lane` component is written (:189–190). Later iterations cannot observe stale values.

#### 2b. 1D lane-overlap collapse: SAFE — Algebraic Reduction

**Verdict:** Mathematically equivalent by construction. Not a behavior change.

**Original code:** Both `centered_rect` calls hardcoded `cy=0.0f, h=1.0f`:
- Player rect: `{player_x - SIZE/2, -0.5f, SIZE, 1.0f}`
- Obstacle rect: `{obs_x - SIZE/2, -0.5f, SIZE, 1.0f}`

**Y-axis collision:** Both rects span `[-0.5f, 0.5f]` identically; overlap **always true**. The `VerticalState::y_offset` (used in `player_timing_y` computation) was **never an input** to `player_overlaps_lane`. No scenario exists where y-overlap would be false.

**X-axis collision:** Keaton's 1D check `|obs_x - player_x| < SIZE` is algebraic reduction of original 2D CheckCollisionRecs on x-interval.

**Key insight:** Verify perf optimizations as algebraic reductions by tracing every input variable to its source. Here, y_offset was never wired into player_overlaps_lane, so the 1D collapse was safe.

#### 2c. Removed helpers: All GENUINELY DEAD ✅

Helpers removed by Keaton-r4: `centered_rect`, `player_timing_point`, `player_in_timing_window`, `player_overlaps_lane`.

**Search proof:** 
- `grep -r "centered_rect|player_timing_point|..." --include="*.cpp" --include="*.h"` returned only comment-text references in `test_model_authority_gaps.cpp` (WIP bug description, not call sites)
- No other file in codebase calls any of the four helpers
- Build compiles zero-warning, confirming no dead references

**All four removals are confirmed dead-code eliminations.**

### Module Health

🟡 Yellow — no 🔴 blockers. Keaton-r4's optimization is correct and safe. Two actionable SRP improvements identified (resolve lambda extraction, LanePush response extraction via PendingLanePush component).

### Top Finding: SRP Violation

**LanePush loop directly mutates player's Lane component** (`p_lane.target`, `p_lane.lerp_t` at :188–192). This is a player-movement *response* embedded inside a collision *detector* — clearest SRP violation in the file.

**Recipe:** Emplace `PendingLanePush{int8_t delta}` on the obstacle; a new `lane_push_response_system` reads and applies it. Identical pattern to Keaton-r5's NonScorableTag (factory-local emplace, system-level routing).

### Cross-Cutting Pattern

The LanePushDelta carry-forward recommendation mirrors Keaton-r5's NonScorableTag pattern: component carries data (delta or presence), factory emplaces at spawn, system reads at response. This is now a proven, reusable recipe.

### Pattern Note for Future Reference

**Verify perf optimizations as algebraic reductions by tracing every input variable to its source — y_offset was never wired into player_overlaps_lane, so the 1D collapse was safe.**

When auditing perf changes, enumerate all inputs to the original function and verify whether each input remains reachable in the new code. If an input is provably constant or unreachable, the optimization is not a behavior change—it's a mathematical simplification.

### Decision

Merged to `.squad/decisions.md` under "Keyser R5 — collision_system SOLID Audit" section.




---

## 2026-05-04 — Ralph Round 6: NonScorableTag Verification + Tag-vs-Kind Audit

**Loop:** Ralph Round 6 (SOLID + ECS patterns)  
**Task:** Verify Keaton-r5's NonScorableTag refactor for behavior preservation; classify all remaining ObstacleKind branch sites codebase-wide  
**Verdict:** ✅ MERGED — scoring_system: 🟡 → 🟢

### Execution Summary

Keyser conducted behavior-preservation audit of Keaton-r5's NonScorableTag refactor by diffing pre-refactor (git ref `29e3ab8`) vs. post-refactor code path. Verified cleanup pass removes identical components (ScoredTag + Obstacle) as old inline continue. Classified all 13 kind-branch sites codebase-wide: 2 actionable (tag patterns), 6 keep-as-is (factory dispatch, data pipeline), 5 deferred (minor helpers). Updated module health.

### Work Completed

1. **Behavior-Preservation Audit**
   - Compared `29e3ab8:scoring_system.cpp:159–163` (old inline continue removing ScoredTag+Obstacle) vs. `scoring_system.cpp:218–236` (new cleanup pass)
   - Verified cleanup happens in same function frame (no inter-system window)
   - Verified miss_view path unchanged for NonScorableTag entities
   - Verified LanePush entities no longer enter hit_buf (excluded from hit_view)
   - **Result:** Behavior preservation CONFIRMED

2. **Test Thoroughness Gap Identified**
   - Test `[scoring][nonscorable]` validates correctness but uses ObstacleKind::LanePushLeft
   - Gap: test couples to legacy mechanism; cannot regress by reintroducing kind guard
   - Fix: change entity kind from LanePushLeft to ShapeGate; one-line change
   - Severity: 🟡 (test is correct, but OCP proof incomplete)

3. **Kind-vs-Tag Classification** — 13 sites across 5 files:
   - **🔴 (actionable tag refactors):** None remaining in scoring_system (OCP resolved)
   - **🟡 (secondary targets, single-site, low priority):** 1 site
     - scoring_system.cpp:107 — `is_bar` dispatch → recommend `BarObstacleTag` (Round 7 optional)
   - **🟢 (keep-as-is, design-correct):** 6 sites
     - beat_scheduler_system (data pipeline — BeatMap, not ECS)
     - beat_map_loader (data validation — parser, not runtime)
     - obstacle_render_entity (factory dispatch — different mesh hierarchies per kind)
     - bar_height_for helper (trivial render-time constant lookup, deferred)
   - **Deferred:** 5 sites (GamePhase::Playing guard, temporary workarounds, etc.)

4. **Module Health Update**
   - **scoring_system:** �� → 🟢 (OCP violation resolved; remaining is_bar dispatch is acceptable)
   - **motion_system bridge:** ✅ verified correct per Keyser-r4 spec
   - **collision_system:** Pending Keaton-r6 LanePushDelta + PendingLanePush delivery

5. **Round 7 Target Recommendation**
   - **BarObstacleTag** for scoring_system.cpp:107 (same factory-locality pattern as NonScorableTag)
   - 4 files, ~8 lines changed, low risk
   - Eliminates only remaining kind branch in scoring_system

### Build & Test

- **Validation:** git diff + codebase-wide grep audit (no test regressions)
- **Pattern confirmation:** Migration-bridge comment in motion_system.cpp:17–21 correct and well-positioned

### Pattern Note for Future Reference

**Cleanup-pass refactors of inline `continue` paths preserve behavior iff the components removed AND the side effects (popup queue, score events) match; verify both via git diff before approving.** When extracting an exclusion from a hot loop to a dedicated cleanup pass, confirm: (1) old path's component removals are replicated in new pass, (2) old path's queues/events are guarded consistently (e.g., hit_buf emptiness), (3) cleanup and hit processing execute in same function frame. This tri-part verification transforms a risky refactor into a provable equivalence.

### Decision

Merged to `.squad/decisions.md` under "Keyser R6 — NonScorableTag Verification + Tag-vs-Kind Pattern Audit" section.


---

## 2026-05-04 — Ralph Round 9: Wirefix Audit + Self-Wiring Meta-Scan

**Loop:** Ralph Round 9 (Keaton-r8 wirefix verification + codebase-wide self-wiring risk scan)  
**Task:** (1) Verify Keaton-r8 wiring of `lane_push_response_system` in production loop; (2) meta-scan all 29 systems in `all_systems.h` to detect any called-but-not-wired (r6→r7 failure mode); (3) recommend CI convention check for future regression prevention.  
**Verdict:** ✅ COMPLETED — 1 finding flagged for Keaton-r10

### Execution Summary

Audited Keaton-r8's production wiring fix and performed a comprehensive codebase-wide scan of all 29 systems to prevent silent self-wiring failures. Found wiring confirmed, but identified a false-positive in the r8 integration test comment. Recommended a CI grep check for future detection, pending user approval.

### Work Completed

#### Part 1: r8 Wirefix Verification ✅

1. **Wiring location confirmed** — `lane_push_response_system(reg, dt)` at `game_loop.cpp:192`
   - Inside `tick_fixed_systems` (lines 174–204)
   - Sole fixed-step production entry point; called at `game_loop.cpp:221` in deterministic `while (accumulator >= FIXED_DT)` loop
   - Same entry point for native + Emscripten (wrapped via `platform_run_loop`)
   - **Verdict:** ✅ Production wiring correct and not test-only

2. **Integration test discrepancy flagged** — 🔴 **FINDING FOR KEATON-R10**
   - Test `"integration: lane push consumed in production tick order"` (test_collision_system.cpp:496) calls systems individually (lines 504–506), NOT via `tick_fixed_systems`
   - Comment claims: *"This test would have FAILED before lane_push_response_system was wired in game_loop.cpp"*
   - **Truth:** Test would pass regardless (it calls `lane_push_response_system` directly). If production wiring were reverted, the test would still green while production broke — identical r6→r7 failure mode.
   - **Correct validation:** Test validates relative ordering + component lifecycle (PendingLanePush emplaced, then consumed) — valuable. But does NOT validate production wiring.
   - **Fix:** Keaton-r10 to rewrite as true integration test calling `tick_fixed_systems` or wrapping `game_loop_frame` headless harness.

3. **Multi-obstacle test verified correct** — ✅ Two-LanePush test (test_collision_system.cpp:518)
   - Correctly does NOT pin which side wins (Left vs. Right) — EnTT iteration order not guaranteed
   - Contracts: exactly one PendingLanePush after collision_system, delta is ±1, consumed and Lane.target updated after response_system
   - No false contracts introduced
   - **Verdict:** ✅ Correct design

#### Part 2: Self-Wiring Meta-Scan (29 systems)

Scanned every symbol in `app/systems/all_systems.h` against production wiring in `game_loop.cpp` and inter-system delegation patterns.

**Results:**
- 🟢 **27 systems:** Wired in production + tested
- 🟡 **2 systems:** Wired via delegation (indirect)
  - `game_state_enter_terminal_phase` — called from `game_state_system.cpp:54,57`, not from game_loop.cpp directly
  - `game_state_end_screen_system` — called from `game_state_system.cpp:116`, not from game_loop.cpp directly
- 🔴 **0 systems:** Called from tests but not wired anywhere in production

**Risk assessment:** 🟡 items are a different risk class than r6→r7:
- r6→r7 pattern: tested directly, never wired in production
- 🟡 here: wired in production (via delegation), never tested directly
- If `game_state_system.cpp` silently stops calling these, no test detects it — coverage gap, not self-wiring false-negative
- **Verdict:** No immediate action. Worth noting for test coverage backlog.

#### Part 3: Convention Recommendation (PENDING USER APPROVAL)

**Recommended:** Add CI grep check to detect future self-wiring regressions (r6→r7 class).

**Script to add:**
```bash
# Every top-level system name from all_systems.h must appear in game_loop.cpp
# (Exceptions: game_state_enter_terminal_phase, game_state_end_screen_system,
#  input_system_init — documented sub-systems or one-time initializers)
for fn in [list of all top-level systems]; do
  grep -q "$fn" app/game_loop.cpp || { echo "WIRING MISSING: $fn"; exit 1; }
done
```

**Status:** Recommendation logged in decisions.md under "Pending User Approval" section. Awaiting yashasg decision on adding new CI infrastructure.

### Build & Test

- **Validation:** Source audits + grep patterns (no code changes; no build/test run needed)
- **Analysis tool:** ripgrep across app/systems/*.cpp and tests/test_*.cpp

### Findings Summary

| Finding | Severity | Action | Owner |
|---------|----------|--------|-------|
| Wiring confirmed in production | ✅ Clean | None | — |
| Integration test comment is vibes claim | 🔴 Fix | Rewrite test as true integration (call `tick_fixed_systems`) | Keaton-r10 |
| Multi-obstacle test contracts correct | ✅ Clean | None | — |
| Meta-scan: 2 🟡 (delegation), 0 🔴 | 🟡 Note | Coverage gap tracking (R10+ backlog) | — |
| CI grep check recommended | ⏳ Awaiting | Add to CI pipeline if approved | User (yashasg) |

### Pattern Note for Future Reference

**Audit the test, not the test description — a comment claiming "would have failed before the fix" is a vibes claim until verified. Demand a fail-then-fix run as evidence.**

When a test includes a comment asserting pre-fix failure, don't trust the prose. Run the test against the pre-fix code (git stash production changes, run tests, git unstash) to verify the claim. In this case, the test calls `lane_push_response_system` directly; reverting the production wiring does not cause it to fail. The test is actually validating local semantics (ordering + consumption), not production wiring. These are different contracts. Call them out separately.

### Decision

Merged to `.squad/decisions.md` under "Round 9: Keyser — Wirefix Audit + Self-Wiring Meta-Scan" section.

---

### Round 12 — R11 Order-Fix Audit + Test Count Forensic

**Date:** 2026-05-05

**Work:** 
1. Audited r11 order-fix: verified fixed_tick_runner.cpp:17,20,27–30 matches pre-r9 form (e32dc82) byte-for-byte. Verified popup_feedback_system.cpp:9 and energy_system.cpp:9 guards are character-identical to pre-r9 source.
2. Diagnosed test count anomaly (783 vs 798): Root cause is methodology drift (r10 used no-filter → 798 total; r11 used `~[bench]` → 783 non-bench). No tests deleted. Reconciliation: r9 797 total, r10 798 (+1), r11 799 (+1), r12 800 (+1, SRP test). All consistent.
3. Established canonical test command: `./build/shapeshifter_tests '~[bench]' --reporter compact 2>&1 | tail -5`. Every future decision drop must use this exact command and include verbatim `tail -5` snippet.
4. Flagged r13 cleanup: test_phase_runner.cpp:80 catches wiring (systems present) but not ordering (systems in wrong order). Recommend fail-then-fix verification for order invariant.

**Module transitions:** playing_systems_runner 🟡 → 🟢 (order verified, guards correct, 11 calls).

**Pattern:** Forensic via live re-run. Don't trust a reported number without re-running with the canonical command. Filter-flag asymmetry is the most insidious source of false alarms. Check methodology before declaring regression.

**Meta-lesson:** Methodology inconsistency generates false alarms that consume auditor bandwidth. Canonical commands + verbatim output enforce discipline and prevent future drift.

**Decision:** Merged to `.squad/decisions.md` under "Round 12 Decision Drop — Collision System SRP + Order/Count Forensic" section.


---

### Round 14 — Bench Re-Baseline + Module Health Audit

**Date:** 2026-05-03

**Work:**
1. Re-baselined bench numbers post-r13 archetype fix. Found spawn_particles was creating Position+Velocity (legacy vel_view) instead of WorldTransform+MotionVelocity (production motion_view). R13 fixed it. Captured corrected baseline: motion_system 10 ents ~34–38 ns, 100 ents ~191 ns, 1000 ents ~1.81 µs; particle_system 50: ~32–34 ns; full frame stress ~926–1012 ns.
2. Flagged 3 prior round bench claims as questionable (R4 motion "already tight", R4/R9 full-frame, R12 "stable"): measured wrong particle archetype. R12 collision claim unaffected.
3. Verified motion_view tests are real coverage (fail-sensitive to exclusion invariant + path switching).
4. **Tentatively demoted** `fixed_tick_runner` 🟢 → 🟡 pending Keaton-r14 ordering verdict. If ordering test wiring-only, ordering assumption must be re-validated. **UPDATE:** Keaton-r14 confirmed commutative; `fixed_tick_runner` returns 🟢 with reconciliation note.
5. Module health: 12 🟢 / 1 🟡 (motion #349). Zero regressions.
6. Verified Scribe-20 commit selective (only `.squad/` paths). Protocol fix held.

**Metrics:** 12 modules green. motion_system pending #349 migration.

**Pattern Learned:** When tooling changes (archetype fix), prior measurements need re-validation. Bench baseline drift invalidates prior "no delta" claims. Commutativity proof reverses tentative degradations.

**Decision:** Merged to `.squad/decisions.md` under "Round 14: Keaton — Ordering Commutative Analysis + Comment Fixes; Keyser — Bench Re-Baseline + Module Health Audit" section.



---

### Round 13 — SRP Audit (Keaton R13) + R12 Forensic + Module Health Reclassification

**Date:** 2026-05-05

**Work:**
1. **Parallel chain-bonus investigation:** Independent grep confirmed Keaton's RETRACT verdict. Chain_count/chain_timer writes isolated inside scoring_system.cpp. No cross-system coupling. **Independent agreement strengthens retraction evidence.**
2. **R12 forensic:** Found Scribe protocol bug via commit history analysis. R12 commit (7db518b) contains only tests; actual source move (collision_system −10, scoring_system +8) happened in Scribe's R11 commit (e2ca118). Root cause: Scribe likely used `git add -A` and swept up Keaton's working-tree edits. Commit-message attribution error breaks git blame.
3. **player_input_system reclassification:** R12 labeled guards as "redundant but safe." Audit traced all invocation paths: dispatcher callbacks invoked via game_state_system's disp.update<>() calls OUTSIDE the runner. Guards protect these callback paths from executing in non-Playing phases. **Guards are load-bearing, not redundant. Reclassified 🟡 → 🟢.**
4. **Module health snapshot:** Updated post-R13 with motion_system 🟡 (vel_view path #349 still open) and player_input_system 🟢 (guards load-bearing with added comment).

**Process findings:**
- Scribe must use explicit `git add` paths, never `git add -A`. Parallel agent edits are swept up.
- When auditing a refactor, check WHICH commit landed the diff, not just that it landed. Commit-message attribution can be wrong.

**Pattern Learned:** Parallel-investigated chain-bonus and reached same RETRACT verdict. Independent agreement is strong evidence. Audit findings labeled 'redundant' must trace ALL invocation paths, including indirect dispatcher paths, before declaring guards redundant.

**Decision:** Merged to `.squad/decisions.md` under "Round 13: Keaton — Chain-Bonus SRP Retraction + Motion System Migration; Keyser — R12 Forensic + Module Health Reclassification" section.

---

### Round 15 — Fixed-Tick-Runner Demotion Reconciliation + r15 Migration Pre-Audit

**Date:** 2026-05-06

**Work:**
1. **fixed_tick_runner demotion reversal:** Independently verified Keaton-r14's commutativity proof. Traced obstacle_despawn_system and popup_feedback_system source code: data surfaces are completely disjoint (despawn reads ObstacleScrollZ/Position; popup_feedback reads only ctx ScorePopupRequestQueue). No ordering invariant exists. Queue is fully populated and ScoredTag is stripped before both systems run. **Revoked r14 demotion. fixed_tick_runner 🟡 → 🟢.**
2. **Keaton-r15 migration pre-audit:** As of r15 start, Keaton-r15 was not yet landed. Documented pre-r15 baseline: 8 Position read-sites across scoring/collision/camera/despawn systems will need evaluation when r15 ships. Recommended r16 scope as "complete or audit #349 migration" — audit whether readers can migrate to WorldTransform/ObstacleScrollZ.
3. **Module health snapshot (pre-r15):** 11 🟢 / 1 🟡 (motion_system #349 pending). fixed_tick_runner revoked to 🟢.
4. **(Post-r15 confirmation):** Keaton-r15 landed. Position bridge added to motion_view as compatibility layer. motion_system remains 🟡 pending Position bridge audit for r16 (whether readers can migrate).

**Pattern Learned:** When implementation hasn't landed, audit only what exists — don't speculate. Pre-document baselines for post-implementation comparison.

**Decision:** Merged to `.squad/decisions.md` under "Round 15: Keyser r14 Demotion Reconciliation + r15 Audit Pending" section.
