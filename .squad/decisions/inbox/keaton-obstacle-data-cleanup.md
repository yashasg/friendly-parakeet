### 2026-04-28: Component boundary rule — obstacle_data + obstacle_counter cleanup

**By:** Keaton
**What:** Deleted `app/components/obstacle_data.h` and `app/components/obstacle_counter.h`.
- `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction` moved into `app/components/obstacle.h` (entity ECS components belong alongside their sibling entity data)
- `ObstacleCounter` moved to `app/systems/obstacle_counter_system.h` (context singleton, not entity data)

**Rule established:** `app/components/` is exclusively for types emplaced onto entities via `reg.emplace<T>()`. Registry context singletons (`reg.ctx().emplace<T>()`) live in system headers beside their wiring code.

**Validation:** Zero compiler warnings. All 862 test cases pass.
