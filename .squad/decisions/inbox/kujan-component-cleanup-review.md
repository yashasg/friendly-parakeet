# Kujan — Reviewer Gate: Component Cleanup Pass

**Date:** 2026-04-28  
**Artifact:** Component cleanup batch (working tree vs HEAD on `user/yashasg/ecs_refactor`)  
**Status:** ❌ REJECTED — BUILD BROKEN  
**Gate type:** Preliminary (implementation agents still active at inspection time)

---

## Build Result

```
cmake --build build → FAILED
```

**Hard errors (blocks merge):**

1. `app/entities/obstacle_entity.cpp:3: fatal error: '../components/obstacle_data.h' file not found`  
   — `obstacle_data.h` was deleted from `app/components/` but the new `obstacle_entity.cpp` still includes it.

2. `tests/test_obstacle_archetypes.cpp` — 13 `[-Werror,-Wmissing-field-initializers]` errors  
   — All callsites using `{ObstacleKind::..., x, y}` aggregate-init are missing the `beat_info` field.  
   — The old `ObstacleArchetypeInput` struct has been replaced/modified but the tests were not updated to match.

3. `app/archetypes/obstacle_archetypes.h` deleted; `tests/test_obstacle_archetypes.cpp` still attempts `#include "archetypes/obstacle_archetypes.h"` which is the deleted file — fatal include error blocks compilation.

---

## Gate Criteria Assessment

| Criterion | Result | Evidence |
|-----------|--------|---------|
| No new component headers | ✅ PASS | Only deletions + modifications to existing `rendering.h`, `obstacle.h` |
| `render_tags.h` deleted or folded | ✅ PASS | Tags folded into `rendering.h` (lines 122–124 in working tree); standalone file deleted; `shape_obstacle.cpp/h` and `game_render_system.cpp` no longer include it |
| Non-components deleted/folded | ✅ PASS (partial) | `audio.h`, `music.h`, `obstacle_counter.h`, `obstacle_data.h`, `ring_zone.h`, `settings.h`, `shape_vertices.h` all deleted; `shape_vertices.h` relocated to `app/util/` ✅ |
| `ui_layout_cache.h` | ⚠️ STILL BLOCKING | Still present; context singleton, not entity data; deferred — not part of this slice's mandate |
| Components have clear ECS entity-data meaning | ✅ PASS | `ObstacleScrollZ`, `ObstacleModel`, `ObstacleParts`, `TagWorldPass/EffectsPass/HUDPass` — all are entity-scoped data with no logic methods |
| Model/Transform authority migration scope | ✅ ACCEPTABLE | Narrow ObstacleScrollZ bridge for LowBar/HighBar only; not broad Position→Transform migration |
| Build/test clean | ❌ FAIL | 3 distinct build-breaking errors (see above) |

---

## What Is Correct

The substance of the cleanup is good:
- Tags properly folded into `rendering.h`
- Dead `audio/music/settings/shape_vertices/obstacle_data/obstacle_counter/ring_zone` component headers deleted
- `app/systems/obstacle_archetypes.*` dedup removed
- `ObstacleScrollZ` migration is the correct narrow bridge pattern
- `ObstacleModel` + `ObstacleParts` are valid entity-data components in the right header
- System dual-view pattern (Position + ObstacleScrollZ in parallel) is correct migration strategy

---

## Required Fixes Before Gate Passes

**Fix 1 — `obstacle_entity.cpp` include:**  
Remove `#include "../components/obstacle_data.h"` from `app/entities/obstacle_entity.cpp`. The types from that header (`RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction`) now live in `app/components/obstacle.h`.

**Fix 2 — Test `ObstacleArchetypeInput` migration:**  
`tests/test_obstacle_archetypes.cpp` must be updated to match the new entity-creation API. If `apply_obstacle_archetype` + `ObstacleArchetypeInput` are being replaced by `spawn_obstacle` + `ObstacleSpawnParams` (in `app/entities/obstacle_entity.h`), update every callsite. If `ObstacleArchetypeInput` is being retained with a new `beat_info` field, add `{}` defaults to all aggregate-init callsites.

**Fix 3 — Test include path:**  
`tests/test_obstacle_archetypes.cpp` includes `#include "archetypes/obstacle_archetypes.h"` which has been deleted. Update to the canonical header for archetype/entity creation (likely `entities/obstacle_entity.h`), or if `obstacle_archetypes.h` is being re-introduced, ensure it exists before this test compiles.

---

## Non-Blocking Notes

- `ui_layout_cache.h` (context singleton in `app/components/`) remains; this is a known deferred item. It should be addressed in the next cleanup slice.
- `tests/test_obstacle_model_slice.cpp` has `#if 1  // SLICE0_RENDER_TAGS_ADDED` guard — the comment references `render_tags.h` but the active include is `components/rendering.h` (correct). Comment can be cleaned up but is not a build blocker.

---

## Reviewer Lockout

**Keaton** authored the Slice 2 migration work (per `.squad/agents/keaton/history.md`). Per reviewer lockout policy, Keaton may NOT produce the next revision of this artifact.

**Assigned to:** Keyser — fix the 3 broken build errors and re-submit for gate.

---

## Approval Criteria (for re-gate)

- `cmake --build build` exits 0 with zero warnings
- `./build/shapeshifter_tests` runs and all assertions pass
- No new `#include "components/obstacle_data.h"` or any other deleted header referenced in working tree
- `ui_layout_cache.h` acknowledged as deferred (no need to fix in this pass)

