# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z

## 2026-04-29: Screen Controller Migration (adapters → screen_controllers)

**Task:** Migrate `app/ui/adapters/` to `app/ui/screen_controllers/` per design spec `rguilayout-portable-c-integration.md`. User directive: remove dead code, start fresh.

**Changes:**
1. Created `app/ui/screen_controllers/` with 8 controller pairs (`*.cpp`/`*.h`) + `screen_controller_base.h`
2. Preserved all dispatch logic from old adapters verbatim; function names follow spec: `init_<screen>_screen_ui()` / `render_<screen>_screen_ui()`
3. `screen_controller_base.h` consolidates `RGuiScreenController<>` template (renamed from `RGuiAdapter`), `offset_rect()`, and `dispatch_end_screen_choice<>()`
4. Deleted `app/ui/adapters/` entirely (no dead code)
5. Updated `ui_render_system.cpp`: replaced adapter includes/calls with screen controller includes/calls
6. Updated `CMakeLists.txt`: `UI_ADAPTER_SOURCES` → `UI_SCREEN_CONTROLLER_SOURCES` glob

**Validation:** Clean build (zero warnings, zero errors). All 2635 test assertions pass.

**Key Learnings:**
- `RGuiAdapter` template from `adapter_base.h` was renamed `RGuiScreenController` in `screen_controller_base.h` — same pattern, new home
- Anonymous namespace static instances in each `.cpp` prevent ODR collisions under unity builds (each TU gets its own unique identifier)
- Migrating from `*_adapter_render()` → `render_*_screen_ui()` naming is purely mechanical; zero logic changes required

---

## 2026-04-29: app/ui/ Dead Code Cleanup (Dual UI Rendering Path Removal)

**Task:** Remove dead code from `app/ui/` after rguilayout screen-controller migration. Specifically remove the old JSON UI entity spawning/rendering path that was running in parallel with the new rguilayout controllers, creating duplicate rendering.

**Changes:**
1. **Removed duplicate UI rendering code** from `app/systems/ui_render_system.cpp`:
   - Deleted lines 101-161: old JSON-driven UIText/UIButton/UIShape entity rendering
   - Removed `draw_shape_flat()` helper (unused after removal)
   - Removed imports: `ui_element.h`, `ui_source_resolver.h`
   - Result: UI now renders exclusively through rguilayout screen controllers

2. **Removed JSON UI entity spawning** from `app/systems/ui_navigation_system.cpp`:
   - Deleted `destroy_ui_elements()` helper function
   - Removed `spawn_ui_elements()` call on screen transitions
   - Removed import: `ui_element.h`
   - JSON screens still loaded for layout cache building (HudLayout, LevelSelectLayout, OverlayLayout)

3. **Removed `spawn_ui_elements()` implementation** from `app/ui/ui_loader.cpp`:
   - Deleted 180+ lines of JSON→entity spawning logic
   - Removed helper functions: `skip_for_platform`, `json_font`, `json_align`, `json_shape_kind`
   - Removed imports: `ui_element.h`, `transform.h`, `rendering.h`
   - Function declaration removed from `ui_loader.h`
   - Added comment explaining removal and referencing disabled tests

4. **Disabled obsolete tests** in `tests/test_ui_spawn_malformed.cpp`:
   - Wrapped entire test file body in `#if 0 ... #endif`
   - Tests preserved as documentation of old schema validation behavior
   - 14 test cases disabled (all tested `spawn_ui_elements()` error paths)

**Files Retained:**
- `app/ui/ui_loader.{cpp,h}` — still needed for JSON screen loading and layout cache building
- `app/ui/ui_source_resolver.{cpp,h}` — currently unused but small utility with test coverage; kept for potential future use
- `app/ui/level_select_controller.{cpp,h}` — still used by input_dispatcher for level select navigation
- `app/ui/ui_button_spawner.h` — still used for menu button entity spawning (separate from JSON UI path)
- `app/components/ui_element.h` — components still exist but no longer spawned/rendered (may be cleaned up later)

**Validation:** Clean build (zero warnings), all 2595 test assertions pass (880 test cases).

**Root Cause:** The migration to rguilayout left the old JSON UI spawning/rendering path active. Both paths ran in parallel — JSON entities were spawned on every screen transition, then the old render loop drew them, then the new screen controllers also drew their raygui widgets. Kujan correctly identified this as duplicate rendering.

**Impact:** Removed ~240 lines of dead code from production and ~180 lines from tests. UI rendering path now has single source of truth (rguilayout screen controllers).

**Learnings:**
- When migrating UI architectures, verify old render paths are fully disabled, not just bypassed
- Dual rendering paths are easy to miss when both produce visual output — requires careful code inspection
- Test files that exercise removed code should be explicitly disabled (not deleted) to preserve schema documentation
- JSON UI schemas (`content/ui/screens/*.json`) are still loaded for layout cache data extraction (not dead files)

---

## 2026-04-29: UI Regression Fix — Title Text, Level Cards, and Gameplay HUD Restoration

**Context:** User reported three UI regressions after screen-controller migration:
1. Title screen "SHAPESHIFTER" and "TAP TO START" text too small to read
2. Level select screen showing no level cards or difficulty buttons
3. Gameplay HUD missing score and energy bar

Kujan rejected Fenster's initial fix and assigned revision to Keaton/Keyser due to reviewer lockout protocol.

**Root Cause Analysis:**

1. **Title Text Size:** Generated `title_layout.h` has hardcoded Rectangle heights (48px for title, 32px for prompt) that are too small for readable text on mobile. Fenster's approach of wrapping `render()` with `GuiSetStyle(TEXT_SIZE, 60)` failed because GuiLabel clips text to its Rectangle height regardless of font size.

2. **Level Select Cards:** Fenster correctly added manual level card rendering in `level_select_screen_controller.cpp` with proper styling and difficulty buttons. The generated layout only provides the "SELECT LEVEL" heading and Start button.

3. **Gameplay HUD:** The dead-code cleanup removed `draw_hud()` function that rendered score, high score, and vertical energy bar. Only the Pause button remained from the generated layout.

**Solution:**

Applied runtime overrides in screen controllers to preserve generated code ownership:

1. **Title Screen (`title_screen_controller.cpp`):**
   - Manually render title (60px) and prompt (32px) at larger Rectangle sizes
   - Manually render EXIT and SET buttons from generated positions
   - Skip calling `title_controller.render()` to avoid duplicate drawing
   - Documented TODO to regenerate title.rgl with proper sizes

2. **Level Select (`level_select_screen_controller.cpp`):**
   - Override "SELECT LEVEL" heading to 40px size with larger Rectangle
   - Retain Fenster's level card rendering (3 cards with difficulty buttons)
   - Manually render Start button from generated position
   - Skip calling `level_select_controller.render()`

3. **Gameplay HUD (`gameplay_hud_screen_controller.cpp`):**
   - Restored score display (28px) and high score (18px) at top-left
   - Restored vertical energy bar (left side, 20 segments, 22px wide)
   - Preserved flash effect on drain and critical pulse below 25%
   - Manually render Pause button from generated position

**Technical Details:**

- All controllers now manually render UI elements at correct sizes instead of calling generated `_Render()` functions
- Used `state.Anchor01` from generated layout for button positions to preserve portability
- Energy bar uses segmented vertical design from git history (commit ddcd9bb)
- Score display uses `ScoreState.displayed_score` (smoothed value)
- Preserved all color schemes and accessibility features (ENERGY label, etc.)

**Validation:**
- Clean build: zero warnings (clang -Wall -Wextra -Werror)
- All 2595 test assertions pass (880 test cases)
- Text sizes verified against mobile readability requirements

**Key Learnings:**

**Pattern: Runtime Override for Generated Layout Limitations**
- **Context:** When generated UI layouts (rguilayout, Qt Designer, etc.) have hardcoded element sizes too small for production needs and regeneration is not practical
- **Solution:** Manually render affected elements in screen controller at correct sizes while preserving generated layout's button positions/logic
- **Documentation:** Add TODO comments explaining why override exists and referencing source .rgl/.ui file to regenerate
- **Trade-off:** Controllers become slightly coupled to generated layout structure, but ownership boundary remains clear (generated code is read-only reference, controllers own presentation)

**Anti-Pattern: Global Font Size Wrapping**
- **Problem:** Setting `GuiSetStyle(TEXT_SIZE, N)` before calling generated render does NOT fix text clipping if Rectangle heights are too small
- **Why:** GuiLabel and GuiButton clip rendered text to Rectangle bounds regardless of font size
- **Correct Fix:** Override individual element rendering with properly-sized Rectangles

**Energy Bar Restoration:**
- Vertical segmented design (20 segments, left side) is more space-efficient than horizontal bar for mobile portrait layout
- Flash effect (white pulse) and critical pulse (red throb < 25%) are key accessibility cues
- Energy label ("ENERGY") above bar satisfies #171 non-color accessibility requirement

**Files Changed:**
- app/ui/screen_controllers/title_screen_controller.cpp (manual text/button rendering)
- app/ui/screen_controllers/level_select_screen_controller.cpp (manual heading/cards/button rendering)
- app/ui/screen_controllers/gameplay_hud_screen_controller.cpp (score + energy bar restoration)

---

## 2026-04-29: Broken UI Fix Verification Pass

**Task:** Re-verify the approved runtime-override UI fix remains present and review-visible.

**Learnings:**
- For shared-tree work, verify both source content and index state (`A`/`AM`) so reviewers can actually see new screen-controller files.
- Regression-safe UI cleanup means keeping rendering inside `screen_controllers/` and confirming no `spawn_ui_elements()`/adapter path slips back in.

## Learnings

- Title screen generated layout (`app/ui/generated/title_layout.h`) can remain read-only while the active controller (`app/ui/screen_controllers/title_screen_controller.cpp`) performs runtime overrides for text readability and control labeling.
- For centered hero text in raylib/raygui screens, use `DrawText` + `MeasureText` against `TITLE_LAYOUT_WIDTH` instead of relying on `GuiLabel` rectangles; this avoids clipping/alignment drift from undersized generated bounds.
- If generated button text is truncated ("SET"), keep the state wiring (`SettingsButtonPressed`) but relabel and resize in controller runtime (`"SETTINGS"` with explicit rectangle) so behavior stays intact and intent is clear.
- Pause screen (`app/ui/generated/paused_layout.h`) had the same default `GuiLabel` failure mode as pre-fix Song Complete (small, not centered labels); using a local centered-label helper with scoped `TEXT_SIZE` + `LABEL/TEXT_ALIGNMENT` fixes readability without touching button dispatch.
- Keep pause layout source and export aligned when text bounds change: update both `content/ui/screens/paused.rgl` and `app/ui/generated/paused_layout.h` together so future regen does not regress sizing.

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
