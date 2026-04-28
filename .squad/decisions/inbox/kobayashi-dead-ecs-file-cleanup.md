---
date: 2026-04-28
author: kobayashi
status: implemented
---

# Decision: Dead ECS File / Build Cleanup Complete

## What Was Done

Completed the dead-file and build cleanup that prior agents left in a broken state.

### Files Deleted
- `app/components/ring_zone.h` — `RingZoneTracker` had no consumers after ring_zone_log_system removal
- `app/systems/ring_zone_log_system.cpp` — diagnostic zone-entry logger, no gameplay effect
- `app/archetypes/obstacle_archetypes.{h,cpp}` — superseded by `app/entities/obstacle_entity.{h,cpp}`
- `app/systems/obstacle_archetypes.{h,cpp}` — duplicate, was already staged for deletion

### Files Relocated (by prior agents, now confirmed)
- `app/components/audio.h` → `app/systems/audio_types.h`
- `app/components/music.h` → `app/systems/music_context.h`
- `app/components/settings.h` → `app/util/settings.h`
- `app/components/shape_vertices.h` → `app/util/shape_vertices.h`
- `app/components/obstacle_counter.h` → `app/systems/obstacle_counter_system.h`
- `app/components/obstacle_data.h` → deleted; types folded into `app/components/obstacle.h`

### CMakeLists Change
Added `file(GLOB ENTITY_SOURCES app/entities/*.cpp)` and wired into `shapeshifter_lib`. New source directories must always be explicitly added.

### Build Policy Established
C++20 designated initializers (`{.kind = ..., .x = ...}`) are required for `ObstacleSpawnParams` construction when not all fields are specified. This suppresses `-Wmissing-field-initializers` without relying on full positional initialization.

## Validation
- Zero warnings (`-Wall -Wextra -Werror`)
- All 887 test cases pass (2983 assertions)
- Commit: `0d642e2`
