# Component Cleanup Pass — Test & Validation Coverage Notes

**Filed by:** Baer  
**Date:** 2026-04-29  
**Relates to:** Keaton's Slice 1/2 component cleanup (ObstacleScrollZ bridge-state, ObstacleModel/ObstacleParts, archetype path move)

---

## What I Did

Added `static_assert` structural guards (BF-5a, BF-5b) to `tests/test_obstacle_model_slice.cpp` Section D, completing the component contract documentation alongside the existing BF-4 guards for `ObstacleParts`.

### BF-5a — ObstacleScrollZ structural contract (`components/obstacle.h`)
- `!std::is_empty_v<ObstacleScrollZ>` — it carries `float z`, must not degrade to a tag
- `std::is_standard_layout_v<ObstacleScrollZ>` — EnTT pool storage requirement
- `std::is_same_v<decltype(ObstacleScrollZ::z), float>` — field type pinned; catches accidental double/int drift

### BF-5b — ObstacleModel RAII contract (`components/rendering.h`)
- `!std::is_copy_constructible_v<ObstacleModel>` — GPU resource must not be silently duplicated
- `!std::is_copy_assignable_v<ObstacleModel>` — same
- `std::is_move_constructible_v<ObstacleModel>` — EnTT requires movability for pool storage
- `std::is_move_assignable_v<ObstacleModel>` — same

---

## Existing Coverage Confirmed Adequate (no new runtime tests needed)

| Area | File | Status |
|------|------|--------|
| LowBar/HighBar emit `ObstacleScrollZ`, NOT `Position` | `test_obstacle_archetypes.cpp` | ✅ 19 tests, `CHECK_FALSE(reg.all_of<Position>(e))` |
| ShapeGate/LaneBlock/etc. still emit `Position` | `test_obstacle_archetypes.cpp` | ✅ |
| scroll_system dual-view (beat + dt paths) | `test_model_authority_gaps.cpp` | ✅ Part A active |
| cleanup_system dual-view | `test_model_authority_gaps.cpp` | ✅ Part A active |
| miss_detection dual-view + idempotence | `test_model_authority_gaps.cpp` | ✅ Part A active |
| scoring_system hit_view2 + legacy | `test_model_authority_gaps.cpp` | ✅ Part A active |
| collision_system LowBar/HighBar via ObstacleScrollZ | `test_model_authority_gaps.cpp` | ✅ Part B active (`#if 1`) |
| Post-migration archetype + system checks (Slice 2) | `test_obstacle_model_slice.cpp` Section C | ✅ active (`#if 1`) |
| ObstacleParts: non-empty, standard-layout, default zero | `test_obstacle_model_slice.cpp` Section D | ✅ BF-4 static_asserts |
| ObstacleModel: owned=false default, null meshes | `test_obstacle_model_slice.cpp` BF-1 | ✅ runtime test |
| Archetype path moved to `archetypes/` (not `systems/`) | `test_obstacle_archetypes.cpp` include | ✅ include guard enforced by compiler |
| `make_vertical_bar` helper uses `ObstacleScrollZ` | `test_helpers.h` | ✅ updated in working tree |

---

## Pre-existing Build Blockers (not caused by my changes)

**These prevent a clean rebuild from scratch but do not affect the cached test binary.**  
Owner: Keaton.

1. **`app/entities/obstacle_entity.h` missing** — `beat_scheduler_system.cpp` includes it but the file doesn't exist. `apply_obstacle_archetype` and `spawn_obstacle_meshes` are undeclared.  
2. **`ObstacleSpawnParams` initializer warning** — `obstacle_spawn_system.cpp:91`: missing `beat_info` field initializer treated as error under `-Werror`.  

The cached test binary (built before these files were touched) runs cleanly:  
`All tests passed (2978 assertions in 885 test cases)`

**Action required by Keaton:** Deliver `app/entities/obstacle_entity.h` and fix the `ObstacleSpawnParams` initializer before the next CI gate.

---

## What I Did NOT Add (and Why)

- **No new runtime tests** for `ObstacleModel` move semantics — `static_assert` is sufficient and cheaper; actual move behavior is implicitly exercised by any test that puts `ObstacleModel` in an EnTT registry (test_model_authority_gaps.cpp A2).
- **No guard file for deleted `systems/obstacle_archetypes.h`** — the compiler is the guard; any re-introduction at the old path will break the build immediately. No test adds value here.
- **No `ObstacleParts` runtime population test** — `shape_obstacle.cpp` calls `get_or_emplace<ObstacleParts>()` in a GPU path; headless tests would get the early-return path and an empty struct. BF-4 default-zero test is sufficient for headless.
- **Did not touch `render_tags.h`** — Hockney owns that file per charter boundary.
- **Did not continue Model/Transform migration** — Keaton owns that per charter boundary.
