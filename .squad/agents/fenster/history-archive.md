# Fenster — History Archive

Archived entries: 2026-05-20 through 2026-04-29 (detailed UI/rendering/testing work)

## 2026-05-20 — Removed Stale JSON UI Draw Functions (Issue #73)

JSON UI rendered via `draw_level_select_scene` and `draw_hud` functions that bypassed modern rguilayout paths. Deleted 326 lines of stale draw code; validated against 2635 test assertions. Learning: watch for layered rendering in UI migrations—old code may not access source data directly but still render via intermediate caches.

## 2026-05-20 — Documentation Cleanup: Spec Consistency Pass

Removed "(future)" wording from `design-docs/rguilayout-portable-c-integration.md` (3 stale snippets). Build integration is implemented; updated code example to match current `reg.ctx().get<GamePhase>()` syntax.

## 2026-05-20 — Fixed Title/Level-Select UI Regressions Post-Migration

Title screen text too small (raygui text size is global `DEFAULT/TEXT_SIZE`, not Rectangle height). Level select missing cards (old `draw_level_select_scene` not ported). Ported card rendering into screen controller; set `GuiSetStyle(DEFAULT, TEXT_SIZE, 60)` for title. Learning: migration cleanup can hide regression scope; check for both data dependencies AND visual render functions.

## 2026-05-20 — Restored Gameplay HUD Shape Buttons + Energy Bar Fidelity

Ported removed HUD behavior (shape buttons, approach ring, segmented energy bar) into screen controller. Learning: dynamic affordances (approach rings, beat-reactive bars) must be re-implemented in controller or they silently disappear.

## 2026-04-29 — Pause Screen Text Fix (Approved)

Applied exact numeric AC corrections to pause labels. Kujan-approved; all tests passed (2148 assertions, 771 test cases).

## 2026-04-29 — Gameplay Shape Buttons Migration (R2 Rejection)

Tap forgiveness regression: 140×100 input rectangles cannot reach required ±70px vertical forgiveness band. Shape slot geometry diverges from `.rgl`. Reassigned to Keyser (Lead Architect).

## 2026-04-30 — Dead Code Prune — Input Routing Doc Revision

Clarified input routing semantics in design docs: raw input dispatcher emits `InputEvent` → converted to `GoEvent`; semantic UI dispatcher (raygui/controller) emits `ButtonPressEvent`. Kujan re-approved.

## Key Learnings (Archived Period)

- GuiLabel font size is global, not per-control; use `GuiSetStyle(DEFAULT, TEXT_SIZE, N)` to change.
- Screen controllers can mix generated+custom rendering; generated layout for static chrome, controller adds dynamic content.
- Pause screen text parity requires updating both generated call-site and source `.rgl` geometry.
- Audio backend ownership must have one source of truth; keep SDL_mixer lifecycle in single runtime module.
- Runtime init policy should be explicit/strict: return init success/failure; gate shutdown on result; make shutdown resilient to partial init.
