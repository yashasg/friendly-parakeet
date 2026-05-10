# Decision Proposal — Design Doc Lifecycle: `archive/` for Superseded Docs

**Author:** Saul (Game Designer)
**Date:** 2026-05-10
**Trigger:** Issue #393 (prototype.md confusion)
**Status:** For team review

## Context
`design-docs/prototype.md` was marked HISTORICAL in a header banner after the
burnout mechanic was removed (#239), but the file remained in the main
`design-docs/` directory next to the live GDD. Readers (future designers,
contractors, contributors) repeatedly missed the banner and internalized
obsolete burnout/multiplier mechanics. Issue #393 confirmed this is a
recurring source of confusion.

## Proposal
Adopt a project-wide convention for design-doc lifecycle:

1. **Live docs** stay in `design-docs/`.
2. **Superseded docs** move to `design-docs/archive/<original-name>.md`.
3. The archived file's header MUST contain:
   - An "ARCHIVED" banner (not just "HISTORICAL")
   - The reason (issue link)
   - A bullet list of current replacement docs to read instead
4. Any live doc that referenced the moved file is updated to point at the
   replacement(s), with a parenthetical note that the legacy doc is archived.
5. Historical logs under `.squad/log/` and `.squad/orchestration-log/` are
   NOT rewritten — those are dated audit trails.

## Rationale
- Physical relocation is a stronger signal than an in-file banner.
- Keeping the file (vs. deleting) preserves design history and rationale
  for future "why did we change this?" questions.
- A standardized location (`archive/`) is grep-friendly and tool-friendly
  (e.g., the `copilot-instructions.md` doc-index can list it as archived
  in one consistent way).

## Scope of this PR
Only `prototype.md` is moved. No other docs are reclassified in this pass.
Future archivals (e.g., if `feature-specs.md` SPEC 2 is split out) should
follow the same pattern.

## Open Question for Team
- Should `design-docs/archive/` get its own `README.md` index when it has
  ≥ 3 entries? (Defer until that threshold is hit.)
