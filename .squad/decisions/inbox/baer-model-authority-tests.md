# Baer — Model Authority Test Gaps

**Filed:** 2026-05-01  
**Updated:** 2026-05-01 (all blockers resolved — build green, all tests passing)  
**Owner:** Baer (test coverage), Keaton (system implementation), Kujan (review gate)  
**Status:** COMPLETE — All tests active, build green, 898 tests pass.

---

## Summary

Hardened tests for the Model.transform authority migration (Slice 1).
Keaton's Slice 1 implementation was fully committed (scroll_system, cleanup_system,
miss_detection_system, scoring_system, collision_system, archetypes, shape_obstacle
all updated). The component type definitions (ObstacleScrollZ, ObstacleModel,
ObstacleParts) were accidentally missing from headers and have been restored.

---

## What Was Implemented (Slice 1 — Complete)

`ObstacleScrollZ { float z = 0.0f; }` added to `app/components/obstacle.h`.
`ObstacleModel` (RAII GPU wrapper) and `ObstacleParts` added to `app/components/rendering.h`.
`build_obstacle_model()` and `on_obstacle_model_destroy()` in `shape_obstacle.h/cpp`.

Systems updated with a second structural view for `ObstacleScrollZ`:

| System | New view | Outcome |
|--------|----------|---------|
| scroll_system | `<ObstacleTag, ObstacleScrollZ, BeatInfo>` | ✅ Tests pass |
| cleanup_system | `<ObstacleTag, ObstacleScrollZ>` | ✅ Tests pass |
| miss_detection_system | `<ObstacleTag, Obstacle, ObstacleScrollZ>` | ✅ Tests pass |
| scoring_system (hit pass) | `<ObstacleTag, ScoredTag, Obstacle, ObstacleScrollZ>` | ✅ Tests pass |
| collision_system (LowBar/HighBar) | `<ObstacleTag, ObstacleScrollZ, Obstacle, RequiredVAction>` | ✅ Tests pass |

---

## Tests Active

### `tests/test_obstacle_model_slice.cpp`

**Section B.5** (active, 2 tests, 3 assertions):
- `build_obstacle_model: no-op when window not ready` — confirmed headless guard ✅
- `on_obstacle_model_destroy: safe on unowned ObstacleModel` — listener safety ✅

**Section SLICE2** (`#if 1`, 6 tests, 19 assertions):
- `[post_migration]` — LowBar/HighBar archetype components, scroll, cleanup ✅

### `tests/test_model_authority_gaps.cpp`

**Part A** (active, 10 tests, 21 assertions) — `[model_authority]`:
- bridge: scroll_system writes ObstacleScrollZ.z ✅
- bridge: scroll legacy Position path unchanged ✅
- bridge: entity with ObstacleModel only ignored by scroll ✅
- bridge: cleanup_system destroys ObstacleScrollZ entities past DESTROY_Y ✅
- bridge: cleanup legacy Position path unchanged ✅
- bridge: entity with ObstacleModel only ignored by cleanup ✅
- bridge: miss_detection_system tags ObstacleScrollZ entities ✅
- bridge: miss_detection idempotent for ObstacleScrollZ ✅
- bridge: scoring_system processes ObstacleScrollZ scored entity ✅
- bridge: scoring legacy Position path unchanged ✅
- bridge: scoring skips bare ObstacleModel ✅

**Part B** (active, 4 tests, 8 assertions) — `[model_authority][collision]`:
- collision_system resolves LowBar ObstacleScrollZ in timing window ✅
- collision_system resolves HighBar ObstacleScrollZ in timing window ✅
- collision_system misses LowBar out of timing window ✅
- collision_system misses LowBar wrong v-action ✅

---

## Test Suite Summary

```
All tests passed (898 test cases)
```

---

## Validation Commands

```bash
# Bridge-state validation
./build/shapeshifter_tests "[model_authority]" --reporter compact

# Headless guard
./build/shapeshifter_tests "[headless_guard]" --reporter compact

# Post-migration archetype tests
./build/shapeshifter_tests "[post_migration]" --reporter compact

# Full model suite
./build/shapeshifter_tests "[model_slice]" --reporter compact

# Full test run
./build/shapeshifter_tests --reporter compact
```
