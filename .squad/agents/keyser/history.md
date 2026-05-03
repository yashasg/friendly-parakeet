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



