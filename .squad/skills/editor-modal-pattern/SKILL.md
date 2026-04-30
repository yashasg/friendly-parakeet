---
name: "editor-modal-pattern"
description: "Add dependency-free modal UX in static editor shells with shared wiring and source-level Node tests."
domain: "tooling-ui"
confidence: "high"
source: "observed"
---

## Context
Use this in the beatmap editor (or similar static tools) when adding new dialogs/help overlays without adding frameworks or build steps.

## Pattern
- Add static modal markup in `index.html` with stable IDs and `aria-hidden` defaults.
- Keep modal content static or code-authored (never user-controlled HTML injection).
- Reuse one modal binder in JS:
  - `bindModal({ trigger, modal, close })`
  - open/close via `.visible` class
  - overlay-click dismiss
  - Escape key closes active modal
  - toggle `aria-hidden` in `showModal`/`hideModal`
- Keep styling local in editor CSS (`.modal-help`, `.help-content`, shortcut `kbd` styles).

## Testing Seam (No DOM Runtime Needed)
- Add Node tests that read `index.html` and `js/main.js` as text and assert:
  - help trigger/modal/close IDs exist
  - required guidance sections exist
  - JS wiring contains binder usage + Escape dismissal logic

## Examples
- UI shell: `tools/beatmap-editor/index.html`
- Wiring: `tools/beatmap-editor/js/main.js`
- Source-level tests: `tools/beatmap-editor/test/help-modal-ui.test.js`
