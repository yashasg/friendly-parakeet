# Decision: Obstacle Entity Layer — app/entities/ Introduction

**Author:** McManus  
**Date:** 2026-04-28  
**Status:** Implemented

## Decision

Introduce `app/entities/` as the canonical surface for named gameplay entity construction. Obstacle entity/component bundle logic is now owned exclusively by `app/entities/obstacle_entity.*`.

## What Changed

- **New:** `app/entities/obstacle_entity.h` — `ObstacleSpawnParams` struct + `spawn_obstacle(reg, params, beat_info*)`.
- **New:** `app/entities/obstacle_entity.cpp` — single implementation of the full obstacle bundle contract (base components + kind-specific components + mesh children).
- **Deleted:** `app/archetypes/obstacle_archetypes.h` and `app/archetypes/obstacle_archetypes.cpp` — fully superseded.
- **Updated:** `obstacle_spawn_system.cpp`, `beat_scheduler_system.cpp` — call `spawn_obstacle` directly.
- **Updated:** `test_obstacle_archetypes.cpp`, `test_obstacle_model_slice.cpp` — use `spawn_obstacle`.

## API Contract

```cpp
struct ObstacleSpawnParams {
    ObstacleKind kind;
    float x = 0.0f, y = 0.0f;
    Shape shape = Shape::Circle;
    uint8_t mask = 0;
    int8_t req_lane = 0;
    float speed = 0.0f;
};

entt::entity spawn_obstacle(entt::registry& reg,
                             const ObstacleSpawnParams& params,
                             const BeatInfo* beat_info = nullptr);
```

## Why pointer for BeatInfo

`std::optional<BeatInfo>` as a trailing struct field triggers `-Wmissing-field-initializers` on all positional brace-init callsites under `-Wextra -Werror`. A nullable pointer keeps the struct trivially aggregate-initializable and is zero-overhead.

## Impact on Other Agents

- `app/archetypes/obstacle_archetypes.*` is gone. Any future agent referencing it must use `entities/obstacle_entity.h` instead.
- The `app/archetypes/` directory still exists for `player_archetype.*` (not touched).
- CMakeLists.txt already has `file(GLOB ENTITY_SOURCES app/entities/*.cpp)` and it is compiled into `shapeshifter_lib`.
