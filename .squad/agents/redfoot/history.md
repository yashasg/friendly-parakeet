# Redfoot — History (SUMMARIZED)

**Last Updated:** 2026-05-10  
**Current Focus:** Cross-platform audio analysis and validation  
**Status:** Recent work on analysis robustness and onset detection

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** UI/UX Designer
- **Joined:** 2026-04-26T02:12:00.632Z

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

## 2026-05-10 — Issue #387 (game-flow.md §2b BURNOUT → ENERGY)
- Branch: audit/autonomous-quality-loop, commit 413b3b5
- §2b HUD diagram already showed ENERGY; only the proportional layout table still listed "Burnout meter" + 30/60/90% zone markers.
- Renamed table row to "Energy bar" with depletion sub-rows + proximity-ring pointer (rhythm-spec). Kept edit surgical.
- Lesson: prior to closing #239-derived doc issues, grep both the diagram AND the layout table that follows — the table is easy to miss.
- Out-of-scope stale Burnout content remains in §4b/§4d/§5a (already marked ARCHIVED for §5a). Likely a follow-up issue worth filing.
- Issue #387 closed with comment.

## 2026-05-10 — Round 2 audit (audit/autonomous-quality-loop-2 @ cf2aa91)

Fresh UI/UX/editor pass after PR #408 merge. Goal: surface remaining
actionable items not already tracked. No file mods (audit-only).

### Filed (new)
- **#425 (documentation)** — `game-flow.md` still has unmarked stale BURNOUT
  content beyond §2b/§5a: §3 Tutorial Flow Summary row 4 ("Burnout intro"),
  §3a Run 1–3 "Burnout meter hidden/still hidden" phrasing, §4b BURNOUT BANK
  feedback spec table, §4d siblings ("CLUTCH"/"LEGENDARY"/Best/Avg Burnout),
  and §5 HUD State Machine table rows for `Burnout Meter` + `Burnout Popup`.
  These are normative-tone sections without per-section ARCHIVED markers; the
  global supersession banner at the doc top is not visible from deep links
  or grep hits, so contributors implementing one section will re-introduce
  removed mechanics. Distinct from #240 (code identifiers) and #237
  (Game-Over UI). This was flagged in my own prior history note as "likely a
  follow-up issue worth filing" — now filed.
- **#426 (bug)** — Beatmap-editor toolbar buttons `#btn-undo` (↩), `#btn-redo`
  (↪), `#btn-settings` (⚙), `#btn-zoom-in` (+), `#btn-zoom-out` (−), and
  `#btn-add-difficulty` (+) rely on `title=` only for their accessible name.
  Screen readers (NVDA/JAWS/VoiceOver) do not consistently expose `title`
  on `<button>`, and emoji-only labels announce as raw glyph names. Same
  precedent as the #389 fix on `#btn-settings-close`. Acceptance: 6 attrs
  added + a regression test extending `settings-modal-a11y.test.js`.

### Findings reviewed and *not* filed (already tracked or out of scope)
- All §4b/§5 stale burnout doc concerns → consolidated into #425 (no
  duplicates with #240, #237, #67, #103).
- `HapticEvent::Burnout1_5x / 3_0x / 5_0x` enum values in
  `app/components/haptics.h` + `app/platform/haptics_backend.cpp` →
  in scope of existing **#240** (stale code identifiers). Skipped.
- `feature-specs.md` SPEC 2 body still has 600 lines of stale content,
  but the entire spec is wrapped in a "HISTORICAL — SPEC 2 superseded"
  banner at top of file; readers cannot land on the body without seeing
  it. Lower urgency than game-flow.md sub-sections; skipped this round.
- **Tracking inconsistency**: #198 (Settings TOGGLE buttons hide their
  state on a separate label) is still OPEN but was substantively resolved
  by my own #390 work (draw_toggle helper with `[X]/[ ]` + colored border
  + corrected MOTION inversion). Per task constraint "do not modify files
  in audit pass" I did NOT close it; flagging here for triage to close as
  fixed-by-#390. Not filing a new issue.
- Modal focus-trap and focus-return-to-trigger gaps in beatmap-editor
  modals: real but lower priority and not regressing from PR #408. Held
  for later.

### Lessons
- Doc supersession via top-of-file banner alone is brittle: every
  search-driven contributor lands mid-document. Per-section ARCHIVED
  blocks (the #387/§5a pattern) are the durable form. Codifying as a
  doc-cleanup convention.
- For HTML a11y issues, write the regression test as DOM-attribute
  introspection (e.g., assert every `<button>` with `textContent.length<=2`
  has a non-empty `aria-label`). Catches the whole class, not one button.

### Final output
**Issues filed: #425, #426.**

## 2026-05-10 — Round 3 audit (audit/autonomous-quality-loop-3 @ fd52051)

UX/a11y/editor pass after PR #427 merge. Audit-only (only this history
file modified). Round 2 #409–#426 all CLOSED; PR #427 merged.

### Filed (new)
- **#438 (documentation)** — `design-docs/game-flow.md` §6 Transition
  Animations Frame Sequence ASCII art (L1752, L1792, L1886, L1937) and
  **Appendix A** Complete Audio Map (L1980–L1986) + **Appendix B**
  Complete Haptic Map (L2014–L2018) still describe the removed burnout
  meter and ×1.0–×5.0 multiplier SFX/haptic events with no per-section
  ARCHIVED markers. #387 fixed §2b; #425 fixed §3/§4b/§4d/§5; appendices
  + transition frames were explicitly out of those issues' scope.
  Highest risk because audio/haptic engineers grep the appendices when
  wiring SFX IDs (cf. closed #236: haptics layer was previously found
  unimplemented). Acceptance includes a grep-based regression: any
  `burnout` hit must live inside an ARCHIVED block.
- **#439 (editor a11y)** — Difficulty tabs (`.diff-tab` Easy/Medium/Hard
  in `tools/beatmap-editor/index.html` L31–35) behave as a tablist but
  use plain `<button>` semantics. No `role="tablist"`/`role="tab"`, no
  `aria-selected` — active state is conveyed only via the `class="active"`
  visual state in `editor.css:198`. `panels.js:384–413` rebuilds the tab
  strip without setting any aria attrs. Confirmed via grep: zero
  `aria-selected`/`role="tab"` occurrences in `tools/beatmap-editor/`.
  Distinct class from #426 (icon-only button names) — this is *control
  state*, not control name.
- **#440 (editor a11y)** — `<select>` controls miss accessible names.
  `#default-level-select` (L16) has only `title=` (same SR-unreliable
  pattern fixed for buttons in #426/#389) and the adjacent "Level" text
  is a `<span class="toolbar-label">`, not a `<label for=…>`.
  `#playback-rate` (L81) has *no* label, aria-label, or labelledby at
  all — SR announces "combobox, 1.0×" with zero context. Acceptance
  includes generalizing `toolbar-a11y.test.js` to assert every form
  control (`<button>`, `<select>`, non-hidden/file `<input>`) outside the
  `display:none` palette-compat sidebar has an accessible name.

### Findings reviewed and *not* filed
- Settings modal `<input>` controls (L189–215) all use proper
  `<label for=…>` — no issue.
- `#chk-metronome` (L89) is wrapped by an implicit `<label>` — fine.
- `#sidebar > .palette-btn` buttons (L241–247) have no name **and**
  no text, but the parent `#sidebar` is `display:none` (panels.js
  compat shell). Not user-reachable; explicitly noted out of scope.
- §4b/§5a in game-flow.md already have ARCHIVED blocks (closed #425).
- Reviewed #198 again — still OPEN, still substantively fixed by #390
  (PR merged into #408 / now superseded by #427's editor work). Audit
  pass; deferred to triage to close.
- Did not audit code-identifier surface (`HapticEvent::Burnout*`,
  `DEAD_BY_BURNOUT`, etc.) — already tracked in **#240**.

### Lessons
- The supersession banner + per-section ARCHIVED-blocks pattern needs
  to extend to **appendices and ASCII frame art**, not just numbered
  prose sections. ASCII frames in particular look authoritative and
  are copied wholesale by UI implementers.
- For HTML a11y, the next iteration of the regression test should
  generalize across element types: `<button>` *and* `<select>` *and*
  `<input>` all need an accessible name. A single DOM-introspection
  test ("every form control outside the hidden palette has either
  textContent ≥ 2 chars, aria-label, aria-labelledby, or an
  associated `<label>`") catches the whole class instead of one
  control type at a time. Codifying in #440's acceptance.
- Tablist patterns are a recurring oversight in icon/badge-styled
  toolbars — same root cause as #426 (custom button chrome hides the
  underlying ARIA contract). Worth a future "audit all editor
  composite widgets against WAI-ARIA APG patterns" pass.

### Final output
**Issues filed: #438, #439, #440.**
