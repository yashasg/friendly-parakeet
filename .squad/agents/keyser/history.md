# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Learnings

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
