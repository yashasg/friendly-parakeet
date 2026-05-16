#pragma once

#include <entt/entt.hpp>
#include "../components/obstacle.h"
#include "../components/player.h"  // Shape
#include "../components/rhythm.h"  // BeatInfo

// Full construction parameters for an obstacle entity.
struct ObstacleSpawnParams {
    ObstacleKind kind;
    float        x        = 0.0f;
    float        y        = 0.0f;
    Shape        shape    = Shape::Circle;  // ShapeGate, SplitPath
    int8_t       req_lane = 0;              // SplitPath
    float        speed    = 0.0f;
};

// Creates and fully configures an obstacle entity:
//   ObstacleTag + TagWorldPass + kind-specific components + mesh child entities.
//   Non-rhythm obstacles get a Vector2 velocity (raw type, see transform.h).
// Owns the complete component bundle contract for obstacles.
entt::entity spawn_obstacle(entt::registry& reg, const ObstacleSpawnParams& params);

// Creates a song-time-authoritative rhythm obstacle:
//   ObstacleTag + BeatInfo + TagWorldPass + kind-specific components + mesh children.
// Rhythm obstacles intentionally do not get a velocity Vector2.
entt::entity spawn_rhythm_obstacle(entt::registry& reg, const ObstacleSpawnParams& params,
                                   const BeatInfo& beat_info);
