# Decision: Safe ECS Cleanup — Obstacle Archetypes Dedup + RingZone Removal

**Date:** 2026-04-28  
**Author:** Keaton  
**Status:** Implemented

## What Changed

1. **Deleted `app/systems/obstacle_archetypes.{h,cpp}`** (duplicate). Canonical copy is `app/archetypes/obstacle_archetypes.{h,cpp}`. Systems and tests now include `"archetypes/obstacle_archetypes.h"` via the `app` include root.

2. **Deleted `RingZone`** — removed `app/components/ring_zone.h`, `app/systems/ring_zone_log_system.cpp`, all declarations, calls, includes, and `RingZoneTracker` emplace logic. The `RING_APPEAR` / `RING_ZONE` session log events are no longer emitted.

## Why

- Both files were blocked on Transform/Position/Velocity design that is not yet settled. Keeping them in the tree would cause confusion and could be picked up by a future agent as an authoritative source.
- RingZone depended on `RingZoneTracker` component which has no users outside the deleted system.

## What Was NOT Changed

- Transform/Position/Velocity migration: deferred.
- `shape_vertices.h`, `ui_layout_cache.h`: untouched.
- `app/util/settings.h` move: preserved as-is.

## Validation

Build: zero warnings (`-Wall -Wextra -Werror`). Tests: 2854/2854 pass.
