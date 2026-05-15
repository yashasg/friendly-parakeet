# Redfoot — History (SUMMARIZED)

**Last Updated:** 2026-05-10  
**Current Focus:** Cross-platform audio analysis and validation  
**Status:** Recent work on analysis robustness and onset detection

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** UI/UX Designer
- **Joined:** 2026-04-26T02:12:00.632Z

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

## 2026-05-11 — High-level system design layout (Excalidraw spec)

User requested a high-level system design of the game in Excalidraw. Per
spawn-prompt charter, I produced the **layout recommendation only** (no
drawing — Coordinator owns the canvas). Spec written to
`.squad/decisions/inbox/redfoot-high-level-design.md`.

Durable design choices worth remembering:

- **5-band landscape layout** (Title / Inputs+Core+State / Render+Audio /
  State Machine) reads top-to-bottom, left-to-right and matches the
  frame-loop phase order in `architecture.md` §3. Don't reinvent — reuse
  for any future "whole system" diagram.
- **One semantic hue per role**, 5 colors total: sand=external input,
  blue=systems, green=render, peach=audio, red=state machine. Plus
  neutral grey for the `entt::registry` spine. Color-blind safe because
  shape+position also encode role.
- **Registry as a single tall rectangle on the right** with singleton
  chips inside, connected to the systems column by **two thick
  bidirectional arrows** — not N fine ones. This is the only honest way
  to draw EnTT context state without arrow spaghetti.
- **Critical nodes (collision, scoring) get a darker fill** within the
  same hue family so the eye lands on them first without breaking the
  palette.
- Deliberately out of scope for a high-level canvas: per-component
  fields, obstacle subtype taxonomy, HUD pixel layout, build/CI,
  beatmap tooling. If asked for those, draw a *second* canvas — never
  overload the system overview.

## Learnings
- Pattern: when delivering a "diagram spec" without drawing tools,
  always include (a) a locked color/typography palette, (b) explicit
  out-of-scope list, (c) acceptance checklist the canvas owner can
  verify against. Without those three, the recipient invents details
  and the diagram drifts.

### 2026-05-12T05:13:01Z: High-level system design Excalidraw layout spec

Produced comprehensive layout specification for single-canvas high-level architecture diagram:
- 5-band structure (Inputs, Core Loop, State/Singletons, Render+Audio, Game State Machine)
- 25+ nodes with locked 5-hue color palette
- Acceptance checklist for Coordinator pre-share review
- Spec merged into decisions.md; Coordinator owns canvas implementation.
