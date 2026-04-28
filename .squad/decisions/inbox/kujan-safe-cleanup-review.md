# Decision: Cleanup Review Gate — Duplicate Archetypes + RingZone Removal

**Date:** 2026-04-28
**Author:** Kujan (Reviewer)
**Status:** APPROVED

## Scope

Working tree diff on `user/yashasg/ecs_refactor`. Two removals by Keaton:
1. Deleted duplicate `app/systems/obstacle_archetypes.{h,cpp}` — canonical lives in `app/archetypes/`
2. Deleted full RingZone subsystem: `app/components/ring_zone.h`, `app/systems/ring_zone_log_system.cpp`, all declarations/calls/includes/emplace logic

## Review Findings

### Duplicate Archetype Removal — CLEAN
- All 3 include-site consumers updated to `"archetypes/obstacle_archetypes.h"` (beat_scheduler_system, obstacle_spawn_system, test_obstacle_archetypes)
- CMakeLists `ARCHETYPE_SOURCES` glob covers `app/archetypes/*.cpp` — no manual entry required
- Old `SYSTEM_SOURCES` glob no longer picks up the deleted files — no CMakeLists edit needed
- Canonical `app/archetypes/` files are content-unchanged; this was purely a reference update

### RingZone Removal — CLEAN
- McManus pre-audit (mcmanus-ringzone-removal.md) mapped the exact 6-site patch; Keaton executed it exactly
- Zero stale `ring_zone|RingZone|RING_ZONE|RING_APPEAR` references remain in app/ or tests/
- `auto* pos` removal in session_logger.cpp is correct: it was exclusively used by the now-deleted RingZoneTracker emplace block; keeping it would have triggered `-Wunused-variable` Werror under zero-warnings policy
- OBSTACLE_SPAWN logging is intact; only RING_APPEAR/RING_ZONE (broken by design per user directive) are gone

### Settings Include Path — CLEAN
- `game_loop.cpp` updated from `"components/settings.h"` → `"util/settings.h"` consistent with keaton-component-boundary-cleanup.md

## Non-Blocking Risk

The cleanup is in the working tree as unstaged changes, not committed via `git mv`. The rename tracking under `git log --follow` will break at the delete/add boundary for obstacle_archetypes. This was pre-noted as P3 in decisions.md and does not affect correctness, build, or test outcomes.

## Verdict

**APPROVED.** No revision needed. Changes are correct, complete, and safe to commit.
