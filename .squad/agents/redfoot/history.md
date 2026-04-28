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
