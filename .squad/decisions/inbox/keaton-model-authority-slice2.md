# Decision: ObstacleScrollZ as bridge component for Position-removal migration (Slice 2)

**Date:** 2026
**Owner:** Keaton
**Status:** Implemented

## Decision

For the model-authority ECS migration, use a narrow, domain-specific bridge component `ObstacleScrollZ { float z; }` instead of a generic transform abstraction when removing `Position` from obstacle kinds.

## Context

LowBar and HighBar obstacles needed their world-space scroll position decoupled from `Position` so that `Model.transform` (set by the camera/render system) becomes the authoritative render state. The scroll axis position still needs to be readable by non-render systems (collision, miss detection, cleanup, camera).

## Options considered

- **Option A (chosen):** Narrow bridge component `ObstacleScrollZ` carrying only `float z`
- **Option B:** Generic `WorldTranslation { Vector3 pos; }` — rejected as too broad, implies 3D authority
- **Option C:** Write scroll position directly into `Model.transform` — rejected; transforms are render-pass output, not gameplay input

## Consequences

- Each lifecycle system runs TWO structural views: one for legacy `Position` entities, one for `ObstacleScrollZ` entities
- `spawn_obstacle_meshes()` must use `try_get<Position>` since LowBar/HighBar no longer have Position
- LanePush* remain on Position (not yet migrated — future Slice 3)
- The "two-view migration pattern" is now the canonical approach for zero-regression component migrations
