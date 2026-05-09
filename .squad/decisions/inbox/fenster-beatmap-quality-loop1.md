# Fenster Decision Inbox — Beatmap Quality Loop 1

Date: 2026-05-09T00:41:48.960-07:00
Agent: Fenster (Tools Engineer)

## Decision

Ship Loop 1 as diagnostics-only instrumentation in `tools/level_designer.py` via CLI flags, without changing default beatmap generation behavior.

## Why

- Team asked for a low-risk loop focused on measuring quality gaps before schema/runtime changes.
- Existing runtime/beatmap schema is integer beat-index based; sub-beat authoring changes are out of scope for this loop.
- Diagnostics can quantify off-beat potential now and de-risk follow-up loops.

## What was implemented

1. Subdivision-aware candidate snapping for:
   - current quarter-only snap,
   - quarter grid,
   - eighth grid,
   - triplet grid.
2. Internal snapped-event diagnostics rows include:
   - beat index,
   - grid time,
   - subdivision label,
   - residual ms,
   - strength/confidence proxies (`flux`, `intensity`, derived intensity confidence, pass count).
3. Artifacts:
   - residual summaries,
   - subdivision histogram,
   - gap histogram (50ms bins),
   - same-shape run metrics baseline from generated beatmaps.
4. CLI controls:
   - `--diagnostics-out`,
   - `--diagnostics-only`.

## 2_drama Loop 1 findings (headline)

- Current quarter-only snap keeps 60/216 events.
- Eighth grid captures substantially more close-alignment events (170 within 20ms).
- Triplet grid captures additional structured off-beat material (all events within 80ms).

## Follow-up recommendation

Use this diagnostics path as the gate for Loop 2 candidate authoring experiments; keep runtime/schema untouched until diagnostics and playtest thresholds are agreed.
