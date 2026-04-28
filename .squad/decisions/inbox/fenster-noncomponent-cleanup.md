# Decision: Non-Component Header Cleanup

**Author:** Fenster  
**Date:** 2026-04-28

## What Changed

Six headers were removed from `app/components/` that were not ECS components:

| Deleted | Replacement |
|---|---|
| `app/components/audio.h` | `app/systems/audio_types.h` (was duplicate) |
| `app/components/music.h` | `app/systems/music_context.h` (was duplicate) |
| `app/components/settings.h` | `app/util/settings.h` (was duplicate) |
| `app/components/shape_vertices.h` | `app/util/shape_vertices.h` (moved) |
| `app/components/obstacle_counter.h` | `app/systems/obstacle_counter_system.h` (was duplicate) |
| `app/components/obstacle_data.h` | folded into `app/components/obstacle.h` |

## Impact

- `app/components/obstacle.h` now includes `player.h` and defines `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction` in addition to its original contents.
- All 2978 test assertions pass post-cleanup.
- `benchmarks/` also had stale includes — fixed.
