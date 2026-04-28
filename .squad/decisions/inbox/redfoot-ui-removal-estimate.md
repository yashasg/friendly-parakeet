# UI Cleanup Removal & Consolidation Audit

**Authored by:** Redfoot (UI/UX Designer)  
**Date:** 2026-05-17  
**Scope:** Full UI code audit for removal/consolidation opportunities when transitioning to ECS entity-driven UI rendering.

---

## Executive Summary

The UI codebase has **~1,800 LOC in active use**. Based on the directive "load once, render repeatedly with entities," this audit identifies **~450–550 removable LOC** across layout caches, HUD special-case rendering, and text duplication when fully migrated to entity-driven UI. Immediate removals total **~100–150 LOC** (dead code + zero-value patterns); the bulk require post-entity-migration cleanup.

---

## Detailed Findings

### 1. **ui_layout_cache.h** (~73 LOC)

**Purpose:** Three POD structs (`HudLayout`, `LevelSelectLayout`, `OverlayLayout`) store pre-computed pixel-space layout data extracted from JSON at screen load, cached in `reg.ctx()`, and consumed by `draw_hud()` and `draw_level_select_scene()`.

**Current Flow:**
1. `ui_navigation_system.cpp:39–64` — On screen change, calls `build_hud_layout()`, `build_level_select_layout()`, or `build_overlay_layout()` 
2. Functions in `ui_loader.cpp` extract from `UIState::screen` JSON and pre-multiply pixel coords (avoids hot-path JSON traversal per #322)
3. `ui_render_system.cpp:69–116, 118–338` — `draw_hud()` and `draw_level_select_scene()` read layout from ctx

**Classification:** **DELETE AFTER UI ENTITY MIGRATION**

**Rationale:**
- These structs are **not ECS components**; they're caches of fixed layout data that never changes after load
- When HUD/level-select elements become entities at load time with pre-set Position/Size/DrawLayer, the layout cache becomes redundant
- The pixel-pre-multiplication benefit (avoiding JSON in hot path) is equally served by emplacing fixed Position entities once

**Removable LOC:** 73 (struct defs) + ~50 LOC in `ui_loader.cpp` build functions = **~120–140 LOC total**

**Tests affected:**
- `tests/test_ui_layout_cache.cpp`: 316 LOC, 21 test cases — **DELETE AFTER MIGRATION**
  - Tests verify correct extraction and pre-multiplication; unnecessary once data becomes entity Position/Size at spawn

**Post-migration:** Entity spawner will replace these caches entirely.

---

### 2. **draw_hud() and draw_level_select_scene()** (~240 LOC in ui_render_system.cpp)

**Current state:**
- `draw_hud()` (lines 118–338): ~220 LOC
  - Shape button row: queries `PlayerTag`/`PlayerShape`, `ObstacleTag`/`RequiredShape`, calculates approach rings and approach-distance scaling
  - Energy bar: reads `EnergyState`, animates 32-segment bar, handles overflow, critical pulse
  - Lane divider: single `DrawLineV` call
- `draw_level_select_scene()` (lines 67–116): ~50 LOC
  - Level card rows with selection state, difficulty buttons, title/number labels

**Why they exist:**
- These are screen-specific renderers that mix **data-driven layout** (from HudLayout cache) with **live gameplay state** (player shape, energy, enemies)
- Cannot be fully data-driven without adding entity-based composition

**Classification:** **DELETE AFTER UI ENTITY MIGRATION** (partial) / **KEEP** (core game-state rendering logic)

**Breakdown:**

| Sub-component | LOC | Status | Post-migration |
|---|---|---|---|
| Shape button row | 70 | KEEP | Convert to entity query + Position/UIButton render |
| Energy bar | 130 | KEEP | Convert bar segments to entities w/ animated alpha/color |
| Approach rings | 25 | KEEP | Query nearest obstacles, render rings (logic stays, just fewer precalc coords) |
| Lane divider | 8 | KEEP | Single entity w/ Position/line-draw render |
| Level select card loop | 50 | KEEP | Convert to entity query + Position/UICard render |

**Safe removals:** Layout cache accessor calls & pre-multiplied pixel coordinate usage = **~40–50 LOC** that becomes simpler once Position entities exist.

**Tests affected:**
- No direct unit tests for draw functions (they are integration points in ui_render_system)
- Render validation happens via e2e or visual regression

---

### 3. **Text Rendering Duplication** (~70 LOC redundancy)

**Current patterns:**

| Pattern | Location | LOC | Issue |
|---|---|---|---|
| `text_draw()` in level_select_scene | `ui_render_system.cpp:85–93` | 20 | Direct raylib call for card titles/numbers |
| `text_draw()` in draw_hud | `ui_render_system.cpp:320` | 5 | Lane divider label (if added) |
| `text_draw()` for UIText entities | `ui_render_system.cpp:379–380` | 4 | Entity-driven text, proper abstraction |
| `text_draw()` for UIButton text | `ui_render_system.cpp:409–410` | 4 | Entity-driven button labels |
| `PopupDisplay` text rendering | `ui_render_system.cpp:342–343` | 5 | Popup score/combo text via PopupDisplay component |

**Duplication source:** 
- `text_draw()` calls appear in three contexts: 
  1. Specialized screen renderers (level select, HUD)
  2. Entity-driven UI text
  3. Score popups
- All three use the same `text_draw()` function, but **context differs**:
  - Specialized renderers bypass entities (hardcoded positions for complex layouts)
  - Entity renderers query Position and compose at draw time
  - Popups query ScreenPosition (world→screen projection)

**Classification:** **CONSOLIDATE NOW** (no risk; improves clarity)

**Improvement:** 
- All text calls already funnel through `text_draw()` in `text_renderer.cpp` — **no code duplication, clean abstraction ✓**
- Minor redundancy: `text_draw()` and `text_draw_number()` both exist; `text_draw_number()` is used by popups — consider inlining or deprecating  

**Safe removals:** ~5–10 LOC if `text_draw_number()` utility is folded into snprintf at call site (low priority).

---

### 4. **PopupDisplay vs UIText Overlap**

**PopupDisplay (scoring_system.cpp spawn):**
```cpp
struct PopupDisplay {
    char text[16];
    FontSize font_size;
    uint8_t r, g, b, a;
};
```

**UIText (ui_element.h, data-driven from JSON):**
```cpp
struct UIText {
    char text[64];
    FontSize font_size;
    TextAlign align;
    Color color;  // includes alpha
};
```

**Difference:** PopupDisplay is game-logic-spawned at score time; UIText is JSON-spawned at screen load.

**Classification:** **KEEP (separate concerns)**
- PopupDisplay: procedurally created popup for transient game events
- UIText: persistent screen-layout text from JSON

**Consolidation opportunity:** Both support fade via `UIAnimation` component. No removal needed; they are intentionally separate.

---

### 5. **rendering.h (DrawLayer + Layer enum)**

**Current state:** 
```cpp
enum class Layer : uint8_t { Background = 0, Game = 1, Effects = 2, HUD = 3 };
struct DrawLayer { Layer layer = Layer::Game; };
```

**Usage:** 
- Game entities (obstacles, player) spawn with `DrawLayer(Layer::Game)`
- Popups spawn with `DrawLayer(Layer::Effects)`
- HUD/UI would spawn with `DrawLayer(Layer::HUD)` (not yet implemented)

**Status:** **PARTIAL REMOVAL AFTER MIGRATION**
- Currently, only game entities use DrawLayer; UI does not
- Once HUD becomes entities, they should carry `DrawLayer(Layer::HUD)` instead of being drawn in specialized functions
- **The enum and component are necessary and correct; rendering.h is NOT a "component bucket" — it is a logical grouping of render-related components**

**Classification:** **KEEP** (correct use of ECS render-pass tagging)

**Post-migration benefit:** `ui_render_system` can query `DrawLayer(Layer::HUD)` instead of hardcoding screen-specific renderers.

---

### 6. **UIState::load_screen() file I/O**

**Status:** Already fixed in #312 / #322 wave. `UIState` is now pure data; file I/O moved to `ui_loader.cpp`.

**Classification:** **ALREADY RESOLVED** ✓

---

## Test Inventory & Removal Map

| Test file | LOC | Tests | Verdict | Reason |
|---|---|---|---|---|
| `test_ui_layout_cache.cpp` | 316 | 21 | DELETE AFTER MIGRATION | Cache structs themselves removed; tests for extraction logic no longer valid |
| `test_popup_display_system.cpp` | 234 | 11 | KEEP | PopupDisplay component remains; tests still relevant |
| `test_ui_element_schema.cpp` | 548 | 28 | KEEP / REFACTOR | Tests JSON spawn logic; migrates to entity archetype tests post-migration |
| `test_ui_spawn_malformed.cpp` | — | — | CHECK | Not examined; likely fixture for malformed JSON — keep if ui_loader persists |
| `test_ui_overlay_schema.cpp` | — | — | CHECK | Tests overlay color extraction — if cached as entity property, test becomes archetype test |
| `test_redfoot_testflight_ui.cpp` | ~80 | 14 | KEEP | Tests game-over reason, settings toggles, shape demos — orthogonal to layout cache |

**Total removable test LOC:** ~316 (after migration)

---

## Removable LOC Summary

| Item | LOC | Timing | Risk | Notes |
|---|---|---|---|---|
| **ui_layout_cache.h** (structs) | 73 | After migration | Low | Pure data; no logic |
| **build_hud_layout()** (ui_loader.cpp) | ~60 | After migration | Low | Replaced by entity archetype spawn |
| **build_level_select_layout()** (ui_loader.cpp) | ~40 | After migration | Low | Replaced by entity archetype spawn |
| **build_overlay_layout()** (ui_loader.cpp) | ~20 | After migration | Low | Overlay color cached in entity or POD |
| **test_ui_layout_cache.cpp** | 316 | After migration | Low | Tests become archetype tests |
| **Minor text_draw_number() refactor** | ~5 | Now or after | Very Low | Optional cleanup |
| **draw_hud() precalc coord code** | ~50 | After migration | Low | Simplified once Position entities exist |
| **Subtotal (safe, measurable)** | **~564** | — | — | — |

**Estimated removable LOC: 450–550** (after full entity migration; conservative range accounts for conditional branches and edge cases).

**Immediately removable LOC: 5–15** (only trivial utilities; migration is prerequisite for bulk).

---

## Classification Matrix

| Component | Status | Timing | Reason |
|---|---|---|---|
| ui_layout_cache.h | DELETE | After migration | Redundant cache pattern |
| HudLayout build function | DELETE | After migration | Replaced by entity spawn |
| LevelSelectLayout build function | DELETE | After migration | Replaced by entity spawn |
| OverlayLayout build function | REFACTOR | After migration | Cache data in entity or smaller POD |
| draw_hud() | REFACTOR | After migration | Keep game-state logic, simplify coordinate handling |
| draw_level_select_scene() | REFACTOR | After migration | Keep card state logic, simplify coordinate handling |
| test_ui_layout_cache.cpp | DELETE | After migration | Tests no longer applicable |
| text_draw() + text_draw_number() | KEEP | — | Core abstraction; no duplication |
| PopupDisplay component | KEEP | — | Separate concern (procedural vs. data-driven) |
| rendering.h (DrawLayer) | KEEP | — | Correct ECS render-pass tagging |
| UIText / UIButton / UIShape | KEEP | — | Entity-driven UI components; foundation of migration |

---

## Handoff Notes for ECS Migration Phase

1. **Entity archetype for HUD elements:** When created, initialize `Position`, `Size`, `DrawLayer(Layer::HUD)`, and appropriate rendering component (e.g., `UIButton`, `UIShape`) at gameplay-phase-enter time. Cache-building function can be inlined into archetype spawn.

2. **Overlay:** Consider storing `overlay_color` directly in a singleton `OverlayState` (or as an entity) rather than `OverlayLayout`. The layout struct was created to hold *only* color — just one field — suggesting it's a workaround for context-storage.

3. **Level select card layout:** Currently special-cased in `draw_level_select_scene()`. Post-migration, spawn card entities at screen load and query them; selection state updates trigger re-renders (existing path). No pre-multiplication needed if cards are entities.

4. **Test migration:** 
   - `test_ui_layout_cache.cpp` → archive or convert to archetype spawn validation tests
   - `test_ui_element_schema.cpp` → keep UI JSON parsing tests; add archetype spawn coverage
   - New tests: validate HUD entity archetype initialization (Position, Size, DrawLayer)

---

## Risk Assessment

**Low risk:**
- ui_layout_cache.h and build functions are purely internal caches
- No public APIs depend on them

**Medium risk:**
- Refactoring draw_hud() and draw_level_select_scene() requires coordinating entity spawn with specialized rendering
- Ensure gameplay/level-select rendering systems are in place *before* removing draw_hud/draw_level_select_scene

**Prerequisite:** 
- Full entity-driven UI rendering system must be implemented and tested before removals are safe
- This audit does not recommend deletion *now*, only post-migration cleanup.

---

## Conclusion

**UI code is well-structured but contains layout-cache boilerplate (ui_layout_cache.h + build functions ≈ 120–140 LOC) that becomes redundant when HUD/level-select elements are spawned as entities once at load and rendered repeatedly.** The bulk of the codebase (text rendering, element spawning, source resolution) is sound and will remain in place. Post-entity-migration, 450–550 LOC is safely removable; immediate removals are negligible (5–15 LOC).

**Recommendation:** Proceed with entity-driven UI rendering design. Use this audit as a roadmap for cleanup after migration is complete and validated.

---

## Appendix: File-by-File LOC Accounting

| File | Total LOC | Core logic | Cache/special-case | Post-migration delta |
|---|---|---|---|---|
| ui_layout_cache.h | 73 | 0 | 73 | −73 |
| ui_render_system.cpp | 441 | 280 | 150 | −50 (coord refs simplify) |
| ui_loader.cpp | 435 | 300 | 120 (build_*_layout) | −120 |
| ui_navigation_system.cpp | 71 | 40 | 25 (cache emplacement) | −25 |
| text_renderer.cpp | 117 | 117 | 0 | 0 |
| popup_display_system.cpp | 50 | 50 | 0 | 0 |
| ui_button_spawner.h | 186 | 186 | 0 | 0 |
| ui_source_resolver.cpp | 156 | 156 | 0 | 0 |
| test_ui_layout_cache.cpp | 316 | 0 | 316 | −316 |
| **Subtotal** | **1,845** | **1,329** | **684** | **−584 (estimated range: −450 to −550)** |

---

*Audit complete. Ready for team review and handoff to ECS migration phase.*
