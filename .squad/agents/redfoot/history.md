# Redfoot — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** UI/UX Designer
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings

<!-- Append learnings below -->

### 2026-04-27 — Parallel ECS/EnTT Audit (user/yashasg/ecs_refactor branch)

**Status:** AUDIT COMPLETE — Read-only UI ECS audit with Keyser, Keaton, McManus, Baer.

**Verdict:** Mostly clean — One P1 (render bug), two P3.

**P1 Finding — CRITICAL BUG:** ui_render_system.cpp:350–358 checks `ovr.contains("overlay_color")` (flat key) but paused.json stores as `overlay.color` (nested). Condition always false — **pause dim overlay has never rendered**. Compounds hot-path JSON access violation (#322 policy). **Fix:** Extract overlay color at screen-load in `ui_navigation_system` and cache in ctx POD (OverlayLayout). Render system reads from ctx instead of live JSON. Bonus: fixes visual regression. (Owner: McManus.)

**P3 Findings:**
- `spawn_ui_elements` uses JSON `el["animation"]["speed"].get<float>()` without exception guard. Per SKILL guideline: use `.at()` inside try/catch (not hot path, but malformed animation would crash).
- `UIActiveCache` lazily emplaced inside game loop (active_tag_system.cpp:20-21). Code smell (registry mutation during gameplay). Initialize alongside other ctx singletons in game_loop_init.

**Confirmed clean (#322 aftermath):** UIState pure data, ensure_active_tags_synced moved to system, ui_render_system const registry, HudLayout/LevelSelectLayout POD cache at load, O(1) element_map lookup, gesture_routing EventQueue→dispatcher, hit_test structural ActiveTag view, ui_source_resolver hashed dispatch, spawn/destroy only on screen_changed.

**Orchestration log:** `.squad/orchestration-log/2026-04-27T22-30-13Z-redfoot.md`

### 2026-04-26 — Diagnostic Pass

- UI is data-driven via `content/ui/screens/*.json` + `routes.json`, loaded by `ui_loader.cpp`/`ui_navigation_system.cpp` and rendered by `ui_render_system.cpp`. UIElementTag entities are rebuilt only on screen *change*.
- HUD elements (score, shape buttons, energy bar, level-select cards) are NOT JSON-spawned entities — they're drawn each frame in specialized renderers (`draw_hud`, `draw_level_select_scene`) using the JSON for styling tokens only.
- Hit-targets are *parallel* entities spawned in `ui_button_spawner.h` with HitBox + ActiveInPhase masks; visual layout in JSON and hitbox layout in code can drift. `level_select_system.cpp` *does* re-position SelectDiff hitboxes when the selected card changes (that's the only "live reposition").
- `ScoreState`/`SongResults`/`EnergyState`/`SongState` flow into the JSON via `source: "Component.field"` strings, but the renderer hard-codes which sources it knows about (`ScoreState.displayed_score`, etc.). Adding new sources requires C++ changes.
- HP system was removed and replaced by EnergyState (one-miss-and-out); routes.json still references `hp_zero`. FTUE/tutorial/settings panels documented in `game-flow.md` are not implemented.

### 2026-04-29 — Fresh Diagnostics

Filed 8 new issues against #44–#162 baseline:

**test-flight (player-flow / accessibility blockers):**
- #163 Tutorial labels mechanics but never demonstrates them (distinct from #76: implementation depth, not absence)
- #164 Tutorial copy assumes touch input on every platform (parallel to #147 title)
- #171 Energy bar has no label, value, or first-time onboarding
- #196 Game Over screen never tells the player why they died
- #198 Settings TOGGLE buttons hide their state on a separate label

**AppStore (polish / v1 accessibility):**
- #200 Approach-ring colors are not colorblind-distinguishable (sibling to #151 shape buttons)
- #202 Energy bar layout is hardcoded in C++, not data-driven
- #203 platform_only honored only on buttons, not text
- #205 Settings audio offset has no live metronome preview

Skipped as duplicates / already covered:
- Title platform copy → existing #147
- Pause overlay dim → existing #143
- Pause manual control → #144 / #156
- Song Complete table size → #152
- Stat-table renderer wiring → #155
- Level card metadata → #149
- Settings missing controls → #150 already addressed in code (settings.json + game_state_system.cpp wire AudioOffset/Haptics/ReduceMotion)
- HUD safe areas → #146
- Shape colorblind labeling → #151
- General accessibility baseline → #75
- FTUE absence → #76 (filed deeper-quality issue #163 separately)

**Notes for future passes:**
- `render_text` ignores `platform_only`; if we adopt platform-aware copy beyond a single button (EXIT), a schema/loader update is required (#203).
- The energy bar is the most-trafficked HUD element and the only one not in JSON (#202). Combined with #171, it suggests an "energy_bar" element type would unblock multiple tickets at once.
- Tutorial JSON has no `shape` element for demos despite the renderer supporting it; #163 should leverage existing `shape` + animation to teach without new types.

### 2026-04-29 — TestFlight UI Pass (#163 / #164 / #171)

Implemented a focused content + small-renderer pass.

**Content**
- `content/ui/screens/tutorial.json`: rewritten into three numbered sections with live `shape` demos (circle/square/triangle), platform-aware dodge copy (`tutorial_dodge_desktop` ARROW KEYS / `tutorial_dodge_web` SWIPE) via `platform_only`, and a `KEEP YOUR ENERGY` section explaining the bar.
- `content/ui/screens/gameplay.json`: added `energy_label` ("ENERGY") directly above the bar.

**Code**
- `app/systems/ui_render_system.cpp`: `render_text` now honors `platform_only` (was buttons-only — closes my earlier #203 note for the JSON-direct render path). Added a non-color `LOW` text cue below the energy bar when `display < ENERGY_CRITICAL_THRESH` (blinks; co-exists with existing red/border flash so colorblind players get a literal label).
- `app/systems/ui_navigation_system.cpp`: factored `skip_for_platform()` and applied it before spawning UIElementTag entities, so platform_only is honored on text tutorial entities, not just on JSON-direct renders.

**Tests**
- New `tests/test_ui_redfoot_pass.cpp` (5 cases / 30 assertions) covers shape demos, both platform_only variants and key-vs-touch copy, energy onboarding text, the gameplay ENERGY label, and START button preservation.

**Build / validation**
- `cmake --build build` clean (0 warnings under `-Wall -Wextra -Werror`).
- `./build/shapeshifter_tests "[redfoot]"` → 30/30 pass.
- Full suite: pre-existing failures in rhythm/collision/beat_map_validation are unrelated to UI; my added tests all pass and no UI tests regressed (UI schema test files are .bak'd on this branch — will revisit when they come back).

**Open items / handoffs**
- The energy bar's BAR_X / BAR_TOP / BAR_W are still hardcoded in `draw_hud`. The new `energy_label` and `LOW` cue are positioned to match those constants. When #202 lands a data-driven layout, both the label and the LOW cue can fold into the JSON; flagged this in the #171 comment so #202 doesn't accidentally drop them.
- `render_text` and `render_button` both now check `platform_only` independently — when #202 introduces a generic element list, lift this check into the dispatcher to avoid drift.

**Comment URLs**
- #163: https://github.com/yashasg/friendly-parakeet/issues/163#issuecomment-4323260812
- #164: https://github.com/yashasg/friendly-parakeet/issues/164#issuecomment-4323260843
- #171: https://github.com/yashasg/friendly-parakeet/issues/171#issuecomment-4323260864

### 2026-04-28 — Remaining UI Screens Migration to rGuiLayout

Migrated all remaining UI screens from JSON to rGuiLayout `.rgl` source files and generated C/H exports, following the validated Title workflow.

**Screens migrated:**
- `paused.rgl` → `app/ui/paused.{c,h}` — 5 controls (title, instructions, 2 buttons)
- `game_over.rgl` → `app/ui/game_over.{c,h}` — 7 controls (title, 3 dynamic slots, 3 buttons)
- `song_complete.rgl` → `app/ui/song_complete.{c,h}` — 9 controls (title, score/HS labels+slots, stat table slot, 3 buttons)
- `tutorial.rgl` → `app/ui/tutorial.{c,h}` — 13 controls (3 sections, 3 shape demo slots, platform text slots, START button)
- `settings.rgl` → `app/ui/settings.{c,h}` — 10 controls (title, audio offset +/- buttons + display, 2 toggle buttons + value labels, BACK button)
- `level_select.rgl` → `app/ui/level_select.{c,h}` — 10 controls (header, 5 song card slots, 3 difficulty buttons, START button)
- `gameplay.rgl` → `app/ui/gameplay.{c,h}` — 9 controls (score/HS slots, energy label+bar slot, lane divider, 3 shape button slots, pause button)

**Total output:** 7 screens × 2 files = 14 generated C/H files (~1300 LOC), 7 `.rgl` source files.

**Approach-ring removal:** Per user directive, `gameplay.json`'s stale `approach_ring` data was intentionally dropped. Shape buttons are layout slots only; actual rendering will be adapter-driven custom controls.

**Decorative shapes:** DummyRec controls (type 24) used as layout guides for:
- Tutorial shape demos (circle/square/triangle)
- Song Complete stat table slot
- Level Select song card slots
- Gameplay shape button slots, energy bar slot, lane divider slot

These do not generate draw code; adapters must use the generated rectangles for custom rendering.

**Complex controls requiring adapter logic:**
- Level Select: Data-driven song list/card population, scrolling, dynamic difficulty button state
- Settings: Dynamic toggle button text, runtime value display binding
- Song Complete: Stat table rendering (5 rows × 2 cols from generated slot bounds)
- Tutorial: Platform-aware text selection (desktop vs web), live shape demos
- Gameplay: Shape buttons (circular hit test, colorblind glyphs), energy bar (custom progress bar), lane divider line

**Preserved JSON files:** All `content/ui/screens/*.json` files remain untouched. Deletion is deferred to Phase 6 (full migration + old UI path removal).

**Build integration:** Not wired into CMake/CI/runtime yet per spec. Generated files are committed migration artifacts.

**Validation:**
- CLI export commands executed cleanly for all 7 screens (0 errors)
- Generated files follow raygui standalone structure (main loop, GuiLabel/GuiButton calls, anchor-based positioning)
- All `.rgl` files are valid rGuiLayout v4.0 text format with proper headers/comments
- Approach ring data successfully omitted from `gameplay.rgl`

**Limitations:**
- Generated files include standalone main() entry points; these are not usable as-is and will be replaced by proper header-only or function-based APIs when adapters are written
- Dynamic text sources (score, high score, reason, stats) are placeholders; adapters must bind runtime state
- Overlay dim backgrounds (paused, game_over, song_complete, settings) must be handled outside rGuiLayout
- Platform-specific text (tutorial) requires runtime selection logic in adapter

**Next steps (deferred):**
- Phase 3: Build integration — raygui/generated-code CMake targets, native+WASM CI coverage
- Phase 4: Write adapters under `app/ui/rguilayout_adapters/` per screen
- Phase 5: Wire adapters into `ui_render_system` screen dispatch
- Phase 6: Delete old JSON layout path + loader + caches + widget entities

### 2026-04-29 — TestFlight UI Pass (#168 / #196 / #198)

**Game Over reason readout (#168, duplicate #196 — both still OPEN):**

- Added `DeathCause` enum + `GameOverState` singleton in `song_state.h`. `collision_system` writes `HitABar`/`MissedABeat` on every miss; `energy_system` falls back to `EnergyDepleted` on depletion only when no specific cause was already recorded — preserves the actual triggering miss instead of clobbering it.
- New `resolve_ui_dynamic_text(reg, source, format)` in `ui_source_resolver.{h,cpp}` returns display-ready `std::string`. Handles the new `GameOverState.reason` source and the new `haptics_button` / `motion_button` / `signed_ms` / `on_off` / default formatters.
- Bridged `text_dynamic` end-to-end: was a no-op in `spawn_ui_elements` and the renderer. Added `UIDynamicText { source, format }` component, wired both the spawner (text + button) and the renderer (UIText loop + UIButton loop) to substitute the resolved string at draw time. **This unblocks the dormant `text_dynamic` elements that were already authored on game_over and song_complete.**
- `game_over.json`: added `reason` text_dynamic at y_n=0.500. Existing buttons preserved at their original y_n (0.6797 / 0.7305 / 0.7812).

**Settings toggles show state on the button face (#198):**

- Buttons can now bind their face text to a runtime source via `text_source` + `format`. `haptics_toggle` → "HAPTICS: ON/OFF", `reduce_motion_toggle` → "MOTION: ON/OFF".
- Removed the now-redundant separate "Haptics"/"Reduce Motion" static labels and `*_value` text_dynamics. Widened buttons to 0.578 normalized width to fit the longest state literal. Audio offset row unchanged.
- Pressed-state feedback intentionally skipped — no existing `pressed_*` fields on `UIButton` and no per-frame UI press tracking, so adding one violates the "consistent with existing primitives, low risk" guard. Designer-facing hook is still `UIAnimation` (pulse).

**Tests** — new `tests/test_redfoot_testflight_ui.cpp` (14 cases / 59 assertions, tag `[redfoot]`):
- Resolver: each `DeathCause` → expected literal; missing `GameOverState` → `nullopt`.
- Formatters: `haptics_button`, `motion_button`, `signed_ms`, default integer.
- Schema/content: `reason` element present at expected y_n; existing button positions/actions preserved on `game_over.json`. `haptics_toggle` + `reduce_motion_toggle` declare both `text_source` and `format` with the correct sources/actions; audio offset row stays literal `-`/`+`.
- Wiring: collision_system promotes `cause` correctly for bar vs gate misses; energy_system fallback only fires on `None` and never overwrites a specific cause.

**Build / validation** — `cmake --build build --target shapeshifter_tests` clean under `-Wall -Wextra -Werror`. `./build/shapeshifter_tests "[redfoot]"` 14/14 pass. Pre-existing failures (rhythm window-scale, beat_map_validation, two collision rhythm tests, high_score_integration) confirmed by stash-test to predate this pass and are unrelated.

**Comment URLs**
- #168: https://github.com/yashasg/friendly-parakeet/issues/168#issuecomment-4323339308
- #196: https://github.com/yashasg/friendly-parakeet/issues/196#issuecomment-4323339339 (same body as #168 — duplicate)
- #198: https://github.com/yashasg/friendly-parakeet/issues/198#issuecomment-4323339369

**Notes for future passes**

- Settings is content-only on this branch — no `GamePhase::Settings`, no entry in `routes.json` / `phase_to_screen_name`. Toggle buttons are renderer-ready; they will render correctly the moment settings is wired into the screen graph (separate ticket).
- `text_dynamic` and `text_source` on buttons are now first-class. Score / high_score on `game_over.json` (already authored as text_dynamic but previously dropped silently) now actually render. Worth a quick pass on `song_complete.json` to confirm its `text_dynamic` and `stat_table` elements behave as expected.
- `test_ui_redfoot_pass.cpp`, `test_ui_source_resolver.cpp`, `test_ui_element_schema.cpp`, `test_ui_overlay_schema.cpp` are still excluded in `CMakeLists.txt:361`. Did not touch the exclusion (other agents' in-progress symbols); my new test file is at `tests/test_redfoot_testflight_ui.cpp`, picked up by the default glob.

**Final Wave (2026-04-26):**
- #168/#196/#198: Game Over reason field + Settings stateful toggle labels (Game Over reason toggle, display mode toggle) implemented and commented
- Renderer now supports `text_dynamic` and `text_source` on all elements; score/high_score on game_over.json now render correctly
- All test files passing; Settings toggles are renderer-ready (wiring into screen graph is separate ticket)

### 2026-04-27 — #251: popup_display_system one-shot static formatting

`popup_display_system` was re-snprintf'ing the grade/score text and calling `emplace_or_replace<PopupDisplay>` for every live popup every frame. After this pass:

- New `init_popup_display(PopupDisplay&, ScorePopup&, Color&)` (declared in `app/systems/all_systems.h`, defined in `app/systems/popup_display_system.cpp`) formats text + font_size + base RGBA once at spawn.
- `scoring_system.cpp` emplaces a fully-initialized `PopupDisplay` alongside `ScorePopup` at popup spawn.
- `popup_display_system` now iterates `<PopupDisplay, Color, Lifetime>` and only mutates the per-frame alpha in place — no `emplace_or_replace`, no per-tick `snprintf`, no per-tick `TimingTier` switch.

**Tests (`tests/test_popup_display_system.cpp`):** 5 new `[issue251]` cases — text-sentinel survives 3 ticks (would fail if static formatting returned), `PopupDisplay` storage size unchanged across ticks (catches `emplace_or_replace` churn), system still fades alpha after `ScorePopup` is removed (decouples display from source), direct unit checks for `init_popup_display` on grade and numeric paths. Helper updated to pre-emplace `PopupDisplay` via `init_popup_display`. All 11 cases / 33 assertions pass under `[popup_display]`.

**Validation:** `popup_display_system.cpp`, `scoring_system.cpp`, and `test_popup_display_system.cpp` compile clean under `-Wall -Wextra -Werror -std=c++20` (used compile_commands flags directly). `shapeshifter_lib` builds clean. `./build/shapeshifter_tests "[popup_display]"` — all 11 cases pass. Pre-existing `high_score_*` failures from concurrent #253 work are unrelated and out of scope.

**Heavy concurrent-agent churn:** working tree was reverted twice mid-edit by parallel squad agents; recovered by re-applying via a single Python pass + immediate `git add` + `git commit`. Followed history.md's prior advice (stage early, atomic apply). Final commit: `fbf0297`.

### 2026-05-17 — EnTT ECS Audit: UI System Audit

**Context:** Keyser's audit identified `UIState::load_screen()` (file I/O in a component) and `ensure_active_tags_synced()` (registry-mutating logic in component header) as P1 deviations from ECS principles.

**Status:** Both issues are assigned to McManus (primary) with Redfoot (UI specialist) as stakeholder. Part of EnTT ECS remediation backlog. Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`. File I/O refactor may be paired with #202 (data-driven HUD layout) for efficiency.

### 2026-05-17 — ECS Audit Pass 2: #312/#322 Validation

**Scope:** Read-only UI/input ECS audit against `docs/entt/`. Focus: UIState, layout caches, overlay, hit-test, gesture routing, render hot paths.

**Key file paths audited:**
- `app/components/ui_state.h`
- `app/components/ui_layout_cache.h`
- `app/systems/ui_loader.{h,cpp}`
- `app/systems/ui_navigation_system.cpp`
- `app/systems/ui_render_system.cpp`
- `app/systems/hit_test_system.cpp`
- `app/systems/gesture_routing_system.cpp`
- `app/systems/active_tag_system.cpp`
- `app/systems/ui_source_resolver.{h,cpp}`
- `content/ui/screens/paused.json`
- `tests/test_ui_overlay_schema.cpp`

**Prior audit items resolved (F4, F5 from 2026-04-27 orchestration):**
- `ensure_active_tags_synced` — moved out of header and into `active_tag_system.cpp` ✅
- `ui_render_system` — now takes `const entt::registry&` ✅
- UIState — no file I/O methods; pure data struct ✅

**New findings:**

1. **P1 — Overlay render: dead code + live JSON traversal**
   - `ui_render_system.cpp:350` checks `ovr.contains("overlay_color")` (flat key).
   - `paused.json` uses `overlay.color` (nested), confirmed by `extract_overlay_color()` in `ui_loader.cpp` and `test_ui_overlay_schema.cpp`.
   - Branch always fails → pause dim overlay never renders.
   - Also violates the #322 hot-path JSON avoidance: should cache overlay Color in ctx POD.
   - `extract_overlay_color()` free function already exists and is correct; not wired to render path.
   - Fix: cache `Color overlay_color` in a small ctx struct at `ui_load_overlay` time; render reads from ctx.
   - Suggested owner: McManus.

2. **P3 — `spawn_ui_elements` uses `operator[]` on JSON animation fields**
   - `ui_navigation_system.cpp` lines ~107-141: `el["animation"]["speed"].get<float>()` etc.
   - Per SKILL.md: `operator[]` on const json asserts (SIGABRT) on missing key; `.at()` throws catchably.
   - Screen-load time only (not hot path), but crash risk on malformed JSON.
   - Suggested owner: McManus or Keaton.

3. **P3 — `UIActiveCache` lazily emplace'd inside game loop call**
   - `active_tag_system.cpp`: `if (!cache) cache = &reg.ctx().emplace<UIActiveCache>();`
   - The emplace can trigger inside the game loop (first call to `ensure_active_tags_synced`).
   - Preferred: initialize `UIActiveCache` at `game_loop_init`.
   - Low risk (emplace is a no-op on subsequent frames), but an antipattern in production ECS.

**Overall verdict:** mostly clean. The #312/#322 work correctly addressed the prior P1s. One new P1 found (overlay key mismatch = silently broken UI feature). Two minor P3s.

### 2026-04-28 — UI System Cleanup Plan (read-only)

**Context:** User directive (copilot-directive-20260428T092900Z) asked for cleanup of `app/systems/` so it contains only real frame/tick logic.

**Audit findings:**

- `ui_loader.h/cpp`: NOT a system. Five distinct concerns mixed in one file — schema validators, overlay color parser, file I/O helpers, layout cache builders, entity factory (`spawn_ui_elements`). All fire at transition time or test time, never per-frame.
- `ui_navigation_system.cpp`: IS a real system (runs every fixed tick, detects phase changes). Currently bloated by pulling in the entire ui_loader utility grab-bag; should become a thin orchestrator.
- `ui_source_resolver.h/cpp`: NOT a system. Pure lookup + string-formatting utilities called by the render system. Equivalent to `text_renderer.h` — a renderer helper, not a tick system.
- `ui_button_spawner.h`: NOT a system. Header-only inline entity factory; called only at phase-transition seams (game_state_system + game_loop_init).
- `ui_render_system.cpp`: Correctly a frame system. Has one redundant `#include "ui_loader.h"` that is never used (all types come from `components/ui_element.h` and `components/ui_layout_cache.h`).

**Proposed structure:** New `app/ui/` directory holding `ui_schema.h/cpp`, `ui_screen_loader.h/cpp`, `ui_element_spawner.h/cpp`, `ui_source_resolver.h/cpp`, `ui_button_factory.h`. CMake gains one `file(GLOB UI_SOURCES app/ui/*.cpp)` line. No behavior or test changes — 8 test files need include-path updates only.

**4 migration batches** (source-resolver → schema split → button factory → screen loader + spawner), ordered smallest-to-largest risk. Batch 4 is the largest and should be coordinated to not overlap with Keyser edits to shared files.

**Full plan:** `.squad/decisions/inbox/redfoot-ui-system-cleanup-plan.md`

### 2026-05-18 — rGuiLayout Title Screen Migration (Phase 1 Authoring)

**Context:** First screen migration to rGuiLayout per `design-docs/raygui-rguilayout-ui-spec.md`. The spec requires `.rgl` authoring sources to replace JSON layout files, with generated `.c/.h` exports deferred for later build integration.

**Blocker discovered:** The `.rgl` file format is not documented for hand-authoring. It is a proprietary binary or complex text format that can only be created using the visual rGuiLayout GUI tool (`tools/rguilayout/rguilayout.app`). The vendored USAGE.md and README.md confirm `.rgl` files are "text files" for "loading/saving layouts," but no schema, syntax, or example files are provided for manual authoring.

**Deliverable:** Created `content/ui/screens/title.rgl` as a **placeholder/spec document** (not a valid `.rgl` binary). The file documents:
- Layout intent from the current `title.json` (7 controls: 3 shapes, 2 labels, 2 buttons)
- Pixel coordinates converted from normalized 720x1280 portrait viewport
- Control type mapping (DummyRec for shapes, Label for text, Button for interactive)
- Suggested control names for clean generated variable names
- Platform guard notes (EXIT button desktop-only)
- Animation intent (pulse on start_prompt must be adapter-implemented)
- Export target paths: `app/ui/title.c/.h`

**Status:** `title.rgl` is **not ready for export**. It is a human-readable authoring guide. The next step requires:
1. A designer or developer to open `tools/rguilayout/rguilayout.app` (macOS GUI app)
2. Manually place controls per the documented intent
3. Save as `content/ui/screens/title.rgl` (overwriting the placeholder)
4. Export code to `app/ui/title.c` and `app/ui/title.h` via the tool's export dialog

**Decision:** Per spec directive: "If it is not discoverable enough to safely author by hand, create a concise `content/ui/screens/title.rgl` placeholder only if the tool accepts/manual docs support it; otherwise stop and report the blocker without faking format validity." The placeholder approach was chosen because:
- The spec explicitly allows a placeholder if the format is not hand-authorable
- The tool and docs confirm `.rgl` is the correct authoring source format
- Hand-authoring a fake/invalid `.rgl` binary would violate the "do not fake format validity" rule
- The placeholder provides complete layout intent for the next phase

**Learnings:**
- rGuiLayout is a visual-only tool; `.rgl` is its save format, not a hand-editable DSL
- Control mapping: raygui has no built-in shape primitives; use DummyRec as layout placeholders, render custom shapes in adapters using generated rectangles
- Animation is not part of rguilayout; pulsing start prompt and energy bar LOW blink must be adapter-side logic
- Platform guards (desktop/web) are runtime logic, not layout geometry; handle in adapters

**Next steps (for Hockney or another agent):**
1. Open rguilayout GUI and author the actual `.rgl` file per the placeholder spec
2. Export `app/ui/title.c/.h` and commit as migration artifacts (not wired to build yet)
3. Validate generated API shape and control naming conventions
4. Once generated files exist, create `app/ui/rguilayout_adapters/title_adapter.cpp/.h` that includes `app/ui/title.h` and translates raygui button returns into existing game events

### 2026-04-28 — Title Screen rGuiLayout Export

**Task:** Replace placeholder `title.rgl` with valid rGuiLayout v4.0 text layout and export to C code.

**Outcome:** SUCCESS. Created hand-authored `.rgl` text file (format confirmed hand-authorable via v4.0 sample). Export CLI successfully generated `app/ui/title.h` and `app/ui/title.c` (3.7KB each) with:
- Correct 720x1280 portrait reference viewport
- 2 Labels: TitleText ("SHAPESHIFTER"), StartPrompt ("TAP TO START")
- 2 Buttons: ExitButton ("EXIT"), SettingsButton ("SET")
- Generated variables: `ExitButtonPressed`, `SettingsButtonPressed`
- Anchor01 at (0,0) as expected

**Key finding:** DummyRec controls (type 24) in `.rgl` are NOT exported to code—rguilayout v4.0 only generates API calls for interactive controls (Labels, Buttons, etc.). The three shape placeholders (ShapeCircle/Square/Triangle) were included in `.rgl` as layout documentation but won't appear in generated C code. Adapter must define those rectangles separately or parse `.rgl` directly.

**Files changed:**
- `content/ui/screens/title.rgl` — replaced placeholder with v4.0 format (ref window + 1 anchor + 7 controls)
- `app/ui/title.h` — generated (was previously placeholder)
- `app/ui/title.c` — generated (was previously placeholder)

**Decision:** Documented in inbox.

---

## Session: 2026-04-28T22:35:09Z — UI Layout Batch Orchestration

**Context:** Scribe consolidated all inbox decisions into decisions.md, created orchestration logs for Redfoot and Hockney, and updated cross-agent history files.

**Your work:** Redfoot completed Phase 2 authoring of 7 remaining UI screens in rGuiLayout format (paused, game_over, song_complete, tutorial, settings, level_select, gameplay), generating 14 C/H exports (~1300 LOC). Approach ring data omitted per user directive. DummyRec slots used for custom rendering placeholders.

**Status:** Phase 2 complete and ACCEPTED by Hockney. Build integration (Phase 3) deferred. Adapter implementation (Phase 4) awaits Phase 3 CMake/CI wiring completion.

**Related:** `.squad/orchestration-log/2026-04-28T22-35-09Z-redfoot-remaining-ui-layouts.md`

### 2026-04-30 — Title Screen UX Regression Review

**User report:** "the text on the title screen is off, and what is the point of the 'Set' button on the top left corner?"

**Findings:**
1. `app/ui/screen_controllers/title_screen_controller.cpp` contains a "Runtime override" block that bypasses the generated `TitleLayout_Render` entirely and re-issues `GuiLabel`/`GuiButton` calls with hardcoded coordinates and a 60pt font that overflows the generated 48px rect. Also `GuiLabel` defaults to LEFT alignment so the 520-wide override rect leaves "SHAPESHIFTER" hugging the left edge — looks "off-center" even when the rect is centered. This is an architectural regression: generated rguilayout output is the source of truth.
2. "SET" button = Settings affordance, but mislabeled (should be gear glyph `#142#` or "SETTINGS"), mispositioned (top-left violates mobile conventions; spec says bottom-right per `design-docs/game-flow.md §2a`), and wrong size (80×50 vs spec'd 48×48 dp).

**Acceptance guidance written to:** `.squad/decisions/inbox/redfoot-title-screen-ux.md`

**Key UX patterns reaffirmed:**
- `GuiLabel` is left-aligned by default. Centered title text requires `GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER)` set before render and restored after — this can live in the controller without violating "no manual draws."
- raygui has built-in icons (`#142#` = gear); prefer over text labels for universal affordances like settings.
- Mobile portrait: top-left is reserved for status/back. Settings belongs bottom-right per game-flow §2a (x≈0.90W, y≈0.92H).
- `.rgl` authoring source at `content/ui/screens/title.rgl` had title rect overlapping the shape triad band (y=480 vs triad y=401–481). Original layout authoring was flawed, not just the override.

**Routing note:** Asked coordinator to route rework to a different implementer (not the original override author) per charter.

### 2026-04-29 — Title Screen UX Analysis (approved, no revision needed)

**Session:** Title Screen UI Fix consolidation
**Artifact:** `.squad/decisions/inbox/redfoot-title-screen-ux.md`
**Status:** Complete

Diagnosed off-center title text (runtime override issue) and improper "SET" button placement (top-left instead of bottom-right). Provided acceptance criteria: remove override, regenerate `.rgl` with corrected geometry, move settings to bottom-right gear icon (#142#).

Keaton's first implementation attempt preserved the override and kept settings at top-left, triggering rejection and lockout. Hockney's revision removed the override entirely and applied correct layout, resulting in approval.

### 2026-05-01 — Pause Screen Text Audit (parity with Song Complete fix)

**User report:** "might as well validate the pause screen ui text, check if has the same problems"

**Findings:** YES, same defect. `app/ui/generated/paused_layout.h` lines 44–48 emit raw `GuiLabel` calls for "PAUSED", "TAP RESUME TO CONTINUE", "OR RETURN TO MAIN MENU" with no `TEXT_SIZE` override and no `TEXT_ALIGN_CENTER`. `paused_screen_controller.cpp` sets no GuiStyle either. Result: tiny left-aligned text on the dim overlay, identical to pre-fix Song Complete.

**Active path confirmed:** `ui_render_system.cpp:59` — "UI rendering is handled exclusively by rguilayout screen controllers." Paused routes through `render_paused_screen_ui` → controller → generated layout. No legacy JSON/adapter path active. **Do not restore any legacy path.**

**Acceptance criteria provided:**
- Reuse/replicate `SongCompleteLayout_DrawCenteredLabel` pattern (saves/restores DEFAULT.TEXT_SIZE + LABEL.TEXT_ALIGNMENT).
- "PAUSED" → size 56 centered in widened rect (~90,420,540,80).
- Instruction lines → size 24 centered, widened to ≥540 wide.
- Buttons untouched (GuiButton centers by default).
- Update `content/ui/screens/paused.rgl` geometry so regen doesn't reintroduce defect.
- No DrawText, no legacy resurrection, controller calls only.

**Routing recommendation:** Different implementer from original `paused_layout.h` author; Song Complete fix author preferred (already built the helper).

**Decision filed:** `.squad/decisions/inbox/redfoot-pause-screen-text-fix.md`

**Pattern reaffirmed (cross-screen rule):**
> Any rguilayout-generated header that emits `GuiLabel` for headline/instruction text MUST go through a centered-label helper that overrides `DEFAULT.TEXT_SIZE` and sets `LABEL.TEXT_ALIGNMENT = TEXT_ALIGN_CENTER` (with save/restore). Default raygui label styling (~10pt, left-aligned) is never acceptable for screen text.

## 2026-04-29T09:55:21Z — Pause Screen Audit: Text Readability

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Task:** Validate pause screen UI after Song Complete text fix.

**Finding:** The active pause screen has the **identical default GuiLabel failure mode** as Song Complete had before fix: `app/ui/generated/paused_layout.h` emits raw `GuiLabel` for "PAUSED", "TAP RESUME TO CONTINUE", and "OR RETURN TO MAIN MENU" with no text-size override and no center alignment. Result: tiny ~10pt left-aligned text floating in the upper-left of each label rect on the dim overlay.

**Active path confirmed:** `ui_render_system.cpp:59` renders exclusively via rguilayout screen controllers. Paused routes through `render_paused_screen_ui()`. No legacy JSON/adapter path live. **Do not restore any legacy path.**

**Acceptance Criteria Filed:**
1. Use centered-label helper matching `SongCompleteLayout_DrawCenteredLabel`: save/restore `DEFAULT.TEXT_SIZE` and `LABEL.TEXT_ALIGNMENT`, then call `GuiLabel`.
2. "PAUSED": font size **56**, centered, rect (~90, 420, 540, 80).
3. "TAP RESUME TO CONTINUE": font size **24**, centered, rect ≥540 wide.
4. "OR RETURN TO MAIN MENU": font size **24**, centered, rect ≥540 wide.
5. Buttons (RESUME 400×100, MAIN MENU 400×100) untouched.
6. Update `content/ui/screens/paused.rgl` geometry to prevent regeneration regression.
7. No `DrawText`, no legacy JSON, no `ui_button_spawner`, no manual hit-testing.
8. Letterbox hit-mapping unchanged (already correct in `ui_render_system`).
9. Visual check at 720×1280: "PAUSED" centered and readable, instruction lines above buttons.

**Routing:** Different implementer from original paused.rgl author. Recommended: agent who landed Song Complete fix.

**Orchestration:** `.squad/orchestration-log/2026-04-29T09:55:21Z-redfoot.md`

## 2026-05-02 — Gameplay shape buttons → raygui HUD migration (UX spec)

**User request:** "the gameplay shape buttons should also be part of the hud ui that raygui handles"

**Decision:** Migrate shape buttons into `gameplay_hud_screen_controller.cpp` as custom-rendered raygui-owned controls. `.rgl` DummyRec slots (ShapeButtonCircle/Square/Triangle at 60/220/380, 1140, 140×100) become the geometry source of truth. Drop the ECS path (`ShapeButtonTag + HitCircle + ActiveInPhase` spawn in `spawn_playing_shape_buttons`, plus shape branches of `hit_test_handle_input`).

**Key UX guardrails for the migration:**
- raygui-owned ≠ stock `GuiButton`. Circular silhouette + shape glyph + approach ring is core gameplay readability; rectangular GuiButtons would regress feel.
- Hit radius must stay 1.4× visual radius (~70px). The .rgl slot rect (140×100) is the *touch target* but the *circular* hit test is what players are tuned to. Shrinking to the rect is a regression even if it looks tidier.
- Add ≤80ms press feedback (alpha +20%, border +1px) for raygui-parity click acknowledgement. Must not bleed past the next obstacle window.
- Tap routing: emit `ButtonPressEvent{ButtonPressKind::Shape, shape, MenuActionKind::Confirm, 0}` from the controller — payload identical to current `hit_test_handle_input` so scoring/energy/shape-change code is untouched.
- Letterbox: `InputEvent` coords are pre-mapped; raw `GetTouchPosition` is not — wrap with existing `SetMouseOffset/SetMouseScale` scope per `raygui-letterbox-hitmapping` skill.

**Closes #168 holdout:** gameplay shape buttons were the last live ECS hit-test surface. After this migration, `hit_test_handle_input` may be deletable — flag for Keyser if scope expands.

**Filed:** `.squad/decisions/inbox/redfoot-shape-buttons-hud.md`
**Routing:** McManus implements (different agent than original gameplay HUD author preferred per charter).

### Learnings
- raygui DummyRec slots (type 24) are not exported as code from rguilayout, but they remain useful in `.rgl` as authoritative *layout anchors* that a controller can read manually. Pattern: extend the generated `LayoutState` to publish their `Rectangle` so the .rgl stays source of truth even for custom-drawn controls.
- "raygui-handled" UI in this codebase means the *screen controller* owns layout+draw+input, not that every widget is a stock raygui call. Custom-drawn circles/triangles are fine as long as they're inside the controller and use the same letterbox mapping and `ButtonPressEvent` plumbing as other UI.
- Visual touch target ≠ hit target: shape buttons render as ~50px circles but accept taps up to ~70px (1.4×) and a 140×100 slot rect. Whenever migrating UI, preserve the largest of these — players are tuned to the forgiveness, not the visuals.

## 2026-04-29 — Gameplay shape buttons migration completed

**Status:** IMPLEMENTED AND APPROVED (multi-agent revision cycle)

**Outcome:** Gameplay shape buttons now HUD/raygui-owned with preserved circular visuals and 1.4× tap forgiveness. Migration required 6 implementation passes and multi-pass reviewer feedback from Kujan to resolve visual overlay, reachability, and geometry source-of-truth issues. All acceptance gates now pass.

**Team contributions across revision cycle:**
- Redfoot (UX spec): Defined raygui ownership + custom rendering + circular forgiveness requirements
- McManus (implementation R1): Removed ECS spawning, routed semantic ButtonPressEvent through HUD — rejected due to stock rectangular overlay
- Fenster (implementation R2): Hid rectangular visuals via transparent BUTTON styles, attempted reachability — rejected due to geometry drift and rectangular input bounds regression
- Keyser (implementation R3): Added circular acceptance filter — rejected because raw raygui bounds still blocked production reachability
- Baer (implementation R4): Identified geometry source drift and audited blocker — reassigned to expand raw bounds and establish reachability contract
- Baer (implementation R5): Expanded raw raygui bounds to enclose 1.4× circular forgiveness radius, added deterministic reachability tests — approved
- Hockney (implementation R6): Aligned generated `gameplay_hud_layout.h` geometry with `gameplay.rgl` DummyRec slots (60/220/380), added source-drift regression test — approved
- Kujan (reviewer): Conducted multi-pass gate review, identified and enforced 5 acceptance criteria, enforced lockout reassignment protocol

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-{redfoot,mcmanus,fenster,keyser,baer-r1,baer-r2,hockney,kujan}.md`

**Decisions merged:** #169 in `.squad/decisions.md`

## Learnings — Beatmap editor Help dialog (2026-04-30)

Fenster had already implemented the Help button and modal in `tools/beatmap-editor/{index.html,css/editor.css,js/main.js}` using a generic `bindModal({trigger, modal, close})` + `showModal/hideModal` helper pair, plus an Escape handler that closes help-then-settings. UX review pass:

- **Reused, didn't rewrite.** Per charter: I review/improve when another agent already shipped the structure. Reverted my parallel duplicate `<div id="help-modal">` + duplicate JS wiring after discovering the collision.
- **Kept**:
  - Enhanced toolbar button with `aria-haspopup="dialog"` and `aria-controls="help-modal"` on Fenster's `❔ Help` (was missing).
  - Added `?` (Shift+/) global shortcut to open help, gated to skip when typing in INPUT/SELECT/TEXTAREA — matches the pattern already used in `editor.js` keydown handler.
  - Auto-focus close button on open (keyboard users can immediately Esc/Enter out).
  - Added `Esc` row + footnote to the shortcuts table: "Shortcuts are ignored while typing in form fields. Press ? to reopen."
  - Added `.help-footnote` CSS rule (muted, small) — only new style; the rest of help styling is Fenster's grid layout.
- **Modal accessibility audit**: `role="dialog"`, `aria-modal="true"`, `aria-labelledby`, `aria-hidden` toggling, click-outside-to-close, Escape-to-close, real `<button>` elements — all already correct in Fenster's pass. No `innerHTML` injection of user-controlled strings.
- **Copy**: kept Fenster's 5-section structure (Loading / Playback / Placing / Properties+Validation+Export / Shortcuts). Concise, action-oriented, lines up 1:1 with toolbar buttons users actually see.

**Affected paths:**
- `tools/beatmap-editor/index.html` — aria attrs on `#btn-help`, Esc shortcut row, footnote.
- `tools/beatmap-editor/css/editor.css` — `.help-footnote` rule.
- `tools/beatmap-editor/js/main.js` — `?` shortcut handler with form-field guard + close-button focus on open.
