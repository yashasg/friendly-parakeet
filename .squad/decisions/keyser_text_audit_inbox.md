# DECISION INBOX: Text Unification Audit

**Date:** 2025-04-27  
**Requester:** yashasg  
**Question:** "We have so much redundancy between text and ui text, isn't all text UI text?"

---

## Audit Findings

### Current Text Architecture

The codebase has **two parallel text rendering pathways**, both screen-space, but with distinct component models:

#### 1. **UI Text Path** (JSON-driven, UIElementTag)
- **Components:** `UIText`, `UIDynamicText`, `UIAnimation`, `Position`, `UIElementTag`
- **Spawning:** `ui_loader.cpp:spawn_ui_elements()` — data-driven from JSON
- **Content Sources:**
  - Static: `UIText.text` (64-byte buffer, immutable after spawn)
  - Dynamic: `UIDynamicText.source` + `format` → `resolve_ui_dynamic_text()` at render time
- **Render Pass:** `ui_render_system.cpp:358–381` — queries `UIElementTag + UIText + Position`
- **Layout:** Screen-space (normalized `x_n`, `y_n` in JSON scaled to pixels)
- **Lifetime:** `UIElementTag` entities destroyed on screen change

#### 2. **Score Popup Path** (game-generated, no tag)
- **Components:** `PopupDisplay`, `Position`, `Velocity`, `Lifetime`, `Color`, `DrawLayer`
- **Spawning:** `scoring_system.cpp:175` — creates one-shot popup per scored obstacle
- **Content Source:** `PopupDisplay.text` (16-byte buffer, pre-formatted by `init_popup_display()` at spawn)
- **Render Pass:** `ui_render_system.cpp:340–348` — queries `PopupDisplay + ScreenPosition`
- **Layout:** Game-space `Position` → converted to `ScreenPosition` by camera system, then drawn screen-space
- **Lifetime:** Has `Lifetime` component, auto-destroyed when `remaining ≤ 0`

#### 3. **Font Resources** (context singleton)
- **TextContext:** Registry context singleton holding three pre-loaded raylib Fonts
- **Accessed by:** `text_draw()` and `text_draw_number()` free functions
- **Used by:** Both UI text rendering and popup rendering

### Entity Boundary Analysis

**Current implicit rules:**
1. **Visible entities have render data:** Both paths have `Position` or `ScreenPosition`
2. **Render-pass membership explicit:** UI has `UIElementTag` (destroyed on screen change); popups have `DrawLayer::Effects`
3. **Screen-space indicator:**
   - UI text: uses normalized coordinates (JSON) → scaled to screen
   - Popups: uses game-space position → projected to screen via `camera_system` 
4. **Content authority:** Immutable at spawn (no re-computation per frame per architecture notes)

### The "Redundancy" Question

**YES, there is redundancy** — but it's intentional separation of concerns:

| Aspect | UIText | PopupDisplay |
|--------|--------|--------------|
| **Content binding** | Static in component OR dynamic via source resolver | Static, pre-formatted at spawn |
| **Lifetime model** | Persistent (screen-level scope) | Transient (fades via Lifetime component) |
| **Layout/anchor** | JSON-data-driven normalization | Game-space scrolling + projection |
| **Spawn trigger** | Screen load (UI system) | Obstacle score event (scoring system) |
| **Render cost** | per-frame text layout (dynamic source resolution) | per-frame alpha fade only; text static |

**Why NOT unified:**
- PopupDisplay uses `Lifetime` fadeout (game-loop lifecycle)
- UIText uses screen-change lifecycle (UI state machine)
- Different performance profiles: UI text may resolve `UIDynamicText.source` every frame; popups do one-shot formatting
- Different coordinate spaces in intent (UI normalized vs game-space projected)

### Unified Design Questions (If Reconsidered)

If unified, the combined entity would need:
1. **Text content:** `text[64]` + optional `UIDynamicText` + optional `PopupDisplay` (for format cache)
2. **Lifecycle:** `UIElementTag` OR `Lifetime` (mutually exclusive)
3. **Layout:** `Position` interpreted differently by render system based on other components
4. **Render flag:** Single tag or `DrawLayer::UI` to distinguish from game-space renders

**Risk:** Would require render system branching logic to interpret `Position` differently per entity type. Current parallel structure is clearer.

---

## Recommendation: **KEEP SEPARATE**

### Rationale
1. **Clarity wins:** Separating game-space popups from UI-space text makes intent explicit
2. **Performance:** No extra branching in render loop; each path optimized for its use case
3. **Maintainability:** Scope isolation — UI loader changes won't affect scoring system
4. **Extensibility:** Future popup variants (notifications, warnings) won't pollute UI component model

### Minimal Cleanup (Optional)
If the redundancy *bothers* teams (not a blocker):
- **Extract `text_draw` utilities to `ui_text.h`** — centralize TextContext access
- **Document implicit rule:** "Text is screen-space rendering only; game-space objects use shapes/meshes, never text"
- Add example in architecture.md: "Visible UI entities (UIText, UIButton) ≠ visible game entities (obstacles, player)"

---

## Durable Rule (If Stored)

**"Text rendering is UI-domain only. Game-space rendering uses shapes and meshes. Score popups are game-generated UI, not game-space text."**

This prevents future developers from adding game-space overhead text, numeric labels on obstacles, etc., which would blur boundaries.

