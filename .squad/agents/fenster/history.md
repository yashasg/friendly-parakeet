# Fenster — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Tools Engineer
- **Joined:** 2026-04-26T02:07:46.545Z

## 2026-05-20 — Removed Stale JSON UI Draw Functions (Issue #73)

Investigated whether old JSON-driven UI rendering was still active behind the rguilayout screen controllers. Found:

- **JSON UI itself is NOT rendered** — `ui_render_system` does not access `UIState::screen` or `UIState::overlay_screen` JSON for rendering
- **Stale draw functions WERE rendering** — `draw_level_select_scene` and `draw_hud` rendered custom visuals before calling rguilayout screen controllers
- **JSON still loaded and used** — for layout caches (`HudLayout`, `LevelSelectLayout`) and ECS entity spawning (`spawn_ui_elements`)

**Cleanup:**
- Deleted `draw_level_select_scene` (121 lines) and `draw_hud` (205 lines) from `ui_render_system.cpp`
- Removed calls to these functions from LevelSelect, Gameplay, and Paused screen cases
- Updated comments in generated headers and screen controllers to reflect that dynamic rendering will be ported to rguilayout in future work

**Build validation:** Zero warnings, all 2635 assertions pass in 901 test cases.

**Learning:** When migrating to a new UI system, watch for layered rendering where old and new systems both execute sequentially. The old system may not access original data sources directly but still render via intermediate caches/derived state. Look for specialized draw functions that execute before the new render path, not just data access patterns.

## 2026-05-20 — Documentation Cleanup: Spec Consistency Pass (Kujan Follow-up)

Three minor stale doc snippets removed from `design-docs/rguilayout-portable-c-integration.md`:

1. **Line 268:** Removed "(future build integration)" wording from `raygui_impl.cpp` comment — it is already wired into CMakeLists.txt.
2. **Line 347:** Renamed section header from `### CMake Integration (Future)` to `### CMake Integration` — the build integration is current/implemented, not aspirational.
3. **Line 122 (code example):** Updated illustrative snippet from old `reg.ctx<GamePhase>()` syntax to current `reg.ctx().get<GamePhase>()` pattern matching real codebase usage across all screen controllers and systems.

Adapter terminology remains confined to the "Migration History: … (Completed)" section as intended. Final spec now reflects current implemented architecture without misleading future/pending wording.

## 2026-05-20 — Fixed Title/Level-Select UI Regressions Post-Migration

**Context:** After migrating from JSON-driven UI to rguilayout screen controllers, two regressions reported by user:
1. Title screen text (SHAPESHIFTER title + "TAP TO START" prompt) was too small
2. Level select screen showed no level cards or difficulty buttons

**Root Causes:**
- **Title screen:** Generated layout header had hardcoded font sizes (48px for title, 32px for prompt) that were too small. GuiLabel text size is controlled by raygui's global `DEFAULT`/`TEXT_SIZE` style property, not by Rectangle height values.
- **Level select:** The old `draw_level_select_scene()` function (removed in cleanup) rendered level cards and difficulty buttons using raylib primitives. The screen controller only rendered the static heading and Start button from the generated layout; dynamic content was never ported.

**Implementation:**
1. **Title screen fix:** Added `GuiSetStyle(DEFAULT, TEXT_SIZE, 60)` before rendering title screen and restored previous value afterward. Increased font size from implicit default (~10px) to 60px for better visibility.
2. **Level select fix:** Ported level card rendering logic into `render_level_select_screen_ui()`:
   - Draws 3 level cards with raylib `DrawRectangleRounded()` primitives
   - Uses GuiLabel for level titles (32px font) and track numbers
   - Renders difficulty buttons for selected card only (EASY/MEDIUM/HARD)
   - Colors and layout match old JSON-driven appearance
   - Maintains existing keyboard/mouse selection state from `LevelSelectState`

**Build & Test Results:**
- ✅ Zero warnings on macOS arm64 clang with `-Wall -Wextra -Werror`
- ✅ All 2595 assertions pass in 880 test cases

**Learnings:**

1. **GuiLabel font size is global, not per-control:** raygui `GuiLabel()` respects the global `GuiGetStyle(DEFAULT, TEXT_SIZE)` property, not the Rectangle height parameter. To change text size for a specific screen or control group, save the current style value, set the desired size, render, then restore. The Rectangle height only affects control bounds/clipping.

2. **Migration cleanup can hide regression scope:** When removing old rendering code, check for both direct data dependencies AND visual rendering functions. The old `draw_level_select_scene()` didn't read from `UIState::screen` JSON, but it was still the sole renderer for dynamic level cards. Deleting it without porting its visual output created a silent regression. Always grep for screen-specific draw/render functions when migrating UI systems.

3. **Screen controllers can mix generated+custom rendering:** rguilayout is best for static layouts (headings, buttons, labels), but dynamic content (lists, grids, conditionally visible elements) still needs custom rendering code. The pattern: generated layout renders static chrome, then screen controller adds dynamic content using raylib/raygui primitives. This hybrid approach is acceptable until rguilayout supports data-driven list/grid layouts.

## 2026-05-20 — Restored Gameplay HUD Shape Buttons + Energy Bar Fidelity

Ported the removed HUD behavior (shape buttons, approach-ring affordance, colorful segmented energy bar, lane divider) into `gameplay_hud_screen_controller.cpp` so gameplay UI stays fully in the screen-controller path. Kept recent fixes intact for pause button transitions and score/high-score labels.

**Learnings:**

1. **Preserve dynamic HUD visuals during controller migration:** generated rguilayout output can carry only static controls, so dynamic affordances (approach rings, active-shape highlighting, beat-reactive bars) must be explicitly re-implemented in the screen controller or they silently disappear.
2. **Keep HudLayout as style data even after JSON element rendering removal:** layout-cache structs remain a stable source of theme and spacing values for custom controller rendering; adding a small fallback prevents blank HUD when layout parsing fails.

## Learnings

- Pause screen text parity fix requires updating both generated call-site values in `app/ui/generated/paused_layout.h` and source geometry in `content/ui/screens/paused.rgl`; changing only one side risks regeneration drift.
- For centered-label readability fixes, preserve existing helper pattern and change only label rectangle/size arguments; keep pause button rectangles/actions untouched.
- Focused validation for this UI artifact path can be done with `cmake --build build --target shapeshifter_tests` followed by `./build/shapeshifter_tests` to confirm no regressions.
- For raygui-backed hit targets that must stay visually custom, wrap `GuiButton` with temporary per-control style overrides (transparent button state colors) and restore immediately; this preserves hit testing without leaking global HUD alpha/state.

## 2026-04-29T09:55:21Z — Pause Screen Text Fix (Approved)

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Task:** Apply exact numeric AC corrections to pause-screen text labels (non-Keaton reviser per lockout protocol).

**Scope:** Correct only the three pause label call-site bounds/sizes in `app/ui/generated/paused_layout.h` and mirror them in `content/ui/screens/paused.rgl`. Buttons/actions unchanged.

**Applied Values:**
- `PAUSED`: (x=90, y=420, w=540, h=80), text size **56**
- `TAP RESUME TO CONTINUE`: (x=90, y=540, w=540, h=36), text size **24**
- `OR RETURN TO MAIN MENU`: (x=90, y=760, w=540, h=36), text size **24**

**Guardrails Respected:**
- Pause buttons (RESUME, MAIN MENU) geometry/order/actions/timing intact
- No legacy UI paths restored (no loader/spawner/cache/adapter/vendor/standalone export changes)
- `PausedLayout_DrawCenteredLabel()` helper implementation matches `SongCompleteLayout_DrawCenteredLabel` exactly

**Validation:**
- Build: zero warnings (clang -Wall -Wextra -Werror)
- Tests: 2148 assertions, 771 test cases — all pass

**Verdict:** ✅ APPROVED by Kujan (2026-04-29T09:55:21Z)

**Sign-off:** All blocking AC from Keaton rejection met exactly. No new issues found. Pattern is consistent across both layout headers. Controller unchanged; action dispatch and timing preserved. Scope was precise: only three label call-site arguments plus mirrored .rgl geometry.

**Orchestration:** `.squad/orchestration-log/2026-04-29T09:55:21Z-fenster.md`

## 2026-04-29 — Gameplay shape buttons migration (revisions R2 → rejected)

**Status:** TWO-PASS REVISION CYCLE
**Reviewer:** Kujan
**Verdict (R2):** REJECTED (reachability regression + geometry drift)

**Revision 1 (Visual Fix):**
- Hid stock rectangular raygui button visuals by temporarily overriding BUTTON style colors to transparent during shape `GuiButton` calls
- Preserved input/state capture path (raygui ownership intact)
- Kept custom circular silhouettes and approach rings visible
- Build and tests passing

**Kujan feedback (R2):** Visual fix approved, but upstream blockers discovered:
- Tap forgiveness regression: 140×100 input rectangles cannot reach the required ±70px vertical forgiveness band (legacy circular geometry)
- Shape slot geometry diverges from `gameplay.rgl`: controller uses constants-derived math instead of `.rgl` as authoritative source

**Reassignment:** Keyser (Lead Architect, non-locked) to address circular hit detection and generated-layout alignment.

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-fenster.md`

---

## 2026-04-30T02:04:27Z — Dead Code Prune — Input Routing Doc Revision

**Session:** Multi-agent dead code cleanup; independent revision under reviewer lockout.

**Your role:** Rework rejected cleanup artifacts (Keaton code cleanup, McManus doc updates) to clarify input routing semantics.

**Files revised:**
- `design-docs/feature-specs.md` — clarified raw input routing (`InputEvent → GoEvent`)
- `design-docs/architecture.md` — documented semantic event routing; `ButtonPressEvent` originates from raygui/controller emitters only
- `tests/test_entt_dispatcher_contract.cpp` — revised test comments to reflect correct routing

**Key clarifications:**
- Raw input dispatcher: handles swipes, keyboard → emits `InputEvent` → system converts to `GoEvent`
- Semantic UI dispatcher: raygui/controller → emits `ButtonPressEvent` (no raw input involvement)

**Outcome:** ✓ APPROVED by Kujan (re-review). Rework integrated into cleanup session. Full test validation passed.
