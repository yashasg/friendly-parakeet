# Decision request — sequencing rabin level-content fixes around level_designer.py contention

**Author:** rabin (Level Designer)
**Date:** 2026-05-10
**Issues:** #391, #392, #394, #396
**Branch:** audit/autonomous-quality-loop
**Status:** blocked (second consecutive retry)

## Context

All four open level-content issues require edits to `tools/level_designer.py`.
Two consecutive Rabin runs (2026-05-10 earlier today, and this retry after
`80660e2`) have been blocked by overlapping in-flight changes from other
agents on the same file:

- First retry blocker: Fenton's source_event_idx plumbing — now committed.
- Current blocker: another agent's `_thin_selected_events_for_min_ioi` /
  `_promote_subdivision_coverage` / `MIN_SUBDIVISION_LABEL_KINDS` work that
  partially addresses #392 and #396 already.

## Decision needed

Pick one of:

1. **Single owner per file per cycle.**  Assign rabin sole edit ownership
   of `tools/level_designer.py` for one cycle, hold other agents off until
   #391/#392/#394/#396 land, then unlock.  Pro: no contention.  Con: blocks
   other in-flight selector work for ~1 cycle.

2. **Hand-off the partial diff to rabin.**  Have the current dirty agent
   commit their `_thin_selected_events_for_min_ioi` /
   `_promote_subdivision_coverage` work in isolation (no beatmap regen),
   then rabin layers #391 (max-run shape rotation) + #394 (Stomper-specific
   easy-fraction tuning) + beatmap regeneration on top.  Pro: preserves
   in-flight effort.  Con: requires explicit ordering signal.

3. **Split #391/#394 onto a separate branch.**  rabin starts a sibling
   branch with isolated edits to lane-rotation logic, then rebases when the
   selector branch lands.  Pro: parallel.  Con: more rebase churn for
   data-driven beatmap regen.

## Rabin's recommendation

Option **2**: the dirty diff is mostly orthogonal to #391's shape-rotation
need and to #394's `SEGMENT_FOCUS_DIFFICULTY_FRACTION` tuning.  If the
current agent commits at the next clean stopping point, rabin can take the
next cycle and add lane-rotation + per-song fraction overrides without
touching their helpers.

## Pending rabin deliverables (gated on unblock)

- `tools/level_designer.py`: post-selection shape-rotation pass capped by
  per-difficulty `MAX_SAME_SHAPE_RUN = {"easy": 4, "medium": 5, "hard": 6}`.
- `tools/level_designer.py`: per-song easy/medium/hard fraction overrides for
  Stomper (#394) and Mental (#392) where the global fraction does not give
  the IOI ramp.
- New tests:
  `tests/test_shipped_beatmap_max_lane_run.cpp`,
  `tests/test_shipped_beatmap_difficulty_ramp_ioi.cpp`,
  `tests/test_shipped_beatmap_subdivision_coverage.cpp`.
- Beatmap regeneration: all three songs.
- `design-docs/rhythm-spec.md`: per-difficulty IOI + max-run + subdivision
  coverage targets.
