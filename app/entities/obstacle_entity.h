#pragma once

#include <entt/entt.hpp>
#include "../components/obstacle.h"
#include "../components/player.h"  // Shape, VMode
#include "../components/rhythm.h"  // BeatInfo

// Full construction parameters for an obstacle entity.
// Subsumes ObstacleArchetypeInput + the scroll speed.
struct ObstacleSpawnParams {
    ObstacleKind kind;
    float        x        = 0.0f;
    float        y        = 0.0f;
    Shape        shape    = Shape::Circle;  // ShapeGate, ComboGate, SplitPath
    uint8_t      mask     = 0;              // LaneBlock, ComboGate
    int8_t       req_lane = 0;              // SplitPath
    float        speed    = 0.0f;
};

// Creates and fully configures an obstacle entity:
//   ObstacleTag + Velocity + DrawLayer + kind-specific components
//   + BeatInfo (if beat_info != nullptr) + mesh child entities.
// Owns the complete component bundle contract for obstacles.
entt::entity spawn_obstacle(entt::registry& reg, const ObstacleSpawnParams& params,
                             const BeatInfo* beat_info = nullptr);
