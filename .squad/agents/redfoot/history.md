# Redfoot — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** UI/UX Designer
- **Joined:** 2026-04-26T02:12:00.632Z

## 2026-05-10 — UI/HUD audit pass (clarity, accessibility, feedback)

**Audit scope:** app/ui, tests/test_*_ui.cpp, design-docs/game-flow.md, tools/beatmap-editor

**High-confidence findings (4 issues filed):**

### Issue #387: Stale BURNOUT spec in game-flow.md section 2b
- game-flow.md lines 284-286 show old BURNOUT meter diagram (SAFE/RISKY/DANGER zones)
- Document header (lines 8-22) already notes burnout was removed in #239
- Current implementation uses ENERGY bar (gameplay.rgl has EnergyBarSlot)
- Spec-code mismatch causes onboarding confusion and regression risk
- **Fix:** Update section 2b HUD diagram to show ENERGY bar; archive section 5a (BURNOUT animation spec)

### Issue #386: Score popup color lacks timing differentiation
- app/entities/popup_entity.cpp lines 43-46: ALL timing tiers get green (100, 255, 100)
- Perfect/Good/Ok/Bad render identically; only no-tier differs (yellow)
- Removes visual feedback for timing quality and breaks accessibility (color-blind players)
- Tests expect this broken behavior (test_popup_display_system.cpp lines 329-372)
- **Fix:** Color-code tiers (Perfect=cyan, Good=yellow-green, Ok=amber, Bad=red); update test matrix

### Issue #388: Beatmap editor settings modal close button inaccessible
- tools/beatmap-editor/index.html line 187: Close button has no aria-label
- Help modal (line 106) correctly uses aria-label="Close help"
- Screen reader users cannot understand button purpose
- **Fix:** Add aria-label="Close settings" to match help modal pattern

### Issue #390: Settings toggles lack visual ON/OFF distinction
- settings_screen_controller.cpp lines 43-57: Toggle state shown as text only (HAPTICS: ON/OFF)
- No color, icon, or visual indicator; breaks accessibility for color-blind users
- Violates toggle button UX pattern (should have icon + text + color)
- **Fix:** Add visual indicator (icon/color) beyond text label; ensure WCAG AA contrast (4.5:1 min)

**Learnings:**
- Spec-doc drift (game-flow.md) is a persistent issue; burnout removal wasn't fully reflected in section 2b
- Feedback/juice opportunities being left on table: timing grade colors should differentiate
- Accessibility gaps in both C++ UI (settings toggles) and HTML editor (aria labels)
- Test expectations may lock in broken behavior; test matrix should encode correct colors
- Level select layout has notes about incomplete adapter migration, but not filed as issue (test coverage exists)

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
- 2026-05-02: Completed TestFlight wave work; decision merged to decisions.md

---

## 2026-05-10 — UX accessibility fixes (issues #386, #387, #389, #390)

Closing-loop pass on the four UI/UX issues filed during the autonomous-quality
audit. Each one was a small, localized accessibility/clarity gap.

- **#386 (Score popup colors)** — `app/entities/popup_entity.cpp` set every
  timing tier to the same green `(100,255,100)`, so PERFECT/GOOD/OK/BAD were
  visually indistinguishable. Mapped each tier to a distinct hue on a green→red
  ramp: Perfect cyan `(80,255,220)`, Good lime `(140,255,80)`, Ok amber
  `(255,200,60)`, Bad red-orange `(255,90,70)`. Yellow no-tier popup unchanged.
  Rewrote `tests/test_popup_display_system.cpp` cases (re-tagged `[issue386]`)
  to assert per-tier colors *and* added a cross-tier distinctness check so we
  cannot regress to a single colour again.
- **#387 (game-flow.md stale BURNOUT)** — Section 2b ASCII HUD diagram now
  shows the ENERGY bar (matching `gameplay_hud_layout.h`'s `EnergyBarSlot`).
  Section 5a left intact for historical reference but explicitly marked
  ARCHIVED with a pointer to `energy-bar.md` and `rhythm-spec.md` §6, so
  contributors do not implement the stale animation spec.
- **#389 (Editor settings close a11y)** — Added `aria-label="Close settings"`
  to `tools/beatmap-editor/index.html#btn-settings-close`, matching the help
  modal pattern. Added `tools/beatmap-editor/test/settings-modal-a11y.test.js`
  regression test (29 editor tests now pass).
- **#390 (Settings toggle visual state)** — Replaced the plain text
  `HAPTICS: ON/OFF` / `MOTION: ON/OFF` GuiButton labels with a draw_toggle
  helper that adds (a) an `[X]`/`[ ]` glyph prefix and (b) a green
  border+fill for ON, grey+dark for OFF, via `GuiSetStyle(BUTTON, …)` save/
  restore (same pattern as `GameplayHudLayout_InputOnlyButton`). Color is now
  redundant with both icon and trailing text — color-blind friendly.
  Also fixed a long-standing display inversion: previous code wrote `MOTION: OFF`
  when `reduce_motion == true`. New helper computes `motion_on = !reduce_motion`
  and renders `MOTION: ON` accordingly, matching the persisted semantic.

**Validation:** `cmake -B build -S . -Wno-dev && cmake --build build` clean
(zero warnings); `./build/shapeshifter_tests` → 1855 assertions / 688 cases
pass; `node --test test/*.test.js` in `tools/beatmap-editor/` → 29 pass.

**Affected paths:**
- `app/entities/popup_entity.cpp`
- `tests/test_popup_display_system.cpp`
- `design-docs/game-flow.md`
- `tools/beatmap-editor/index.html`
- `tools/beatmap-editor/test/settings-modal-a11y.test.js` (new)
- `app/ui/screen_controllers/settings_screen_controller.cpp`
