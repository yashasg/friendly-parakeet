# Skill: Editor help UI source-validation (browserless)

## Use when
- UI help/overlay changes land in `tools/beatmap-editor`.
- CI/runtime does not provide browser automation.

## Procedure
1. Inspect source contracts:
   - `index.html`: visible help trigger + dialog container + concise guidance copy.
   - `editor.css`: safe static styling (hidden by default, explicit visible class).
   - `js/main.js` (or owning module): open/close wiring, backdrop or close affordance, keyboard escape if specified.
2. Run syntax checks on touched editor modules:
   - `node --check tools/beatmap-editor/js/<touched>.js`
3. Run existing editor tests:
   - `node --test tools/beatmap-editor/test/*.test.js`
4. Run diff hygiene:
   - `git --no-pager diff --check`
5. If implementation is not present yet, report **NOT VERIFIED (not visible in source)** and avoid speculative edits.

## Coverage limits
- Node-only tests can enforce static contracts and parser safety.
- They cannot prove focus trapping/layout/a11y behavior without browser-capable integration tests.

## Confidence update (2026-04-30)
- **Confidence:** High for source-level acceptance when implementation is static HTML/CSS with explicit JS modal wiring.
- **New evidence:** Final help-modal rollout validated with Node syntax/tests (`22/22`) and clean diff hygiene.
- **Known blind spot:** focus management and runtime a11y UX still require browser-capable integration tests.
