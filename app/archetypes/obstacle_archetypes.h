#pragma once

#include <entt/entt.hpp>
#include "../components/obstacle.h"
#include "../components/player.h"  // Shape, VMode

// Parameters for constructing a full obstacle archetype onto an entity.
// Callers pre-compute x/y and any kind-specific fields from their context
// (beatmap entry data for the scheduler, RNG-derived values for the spawner).
struct ObstacleArchetypeInput {
    ObstacleKind kind;
    float        x        = 0.0f;
    float        y        = 0.0f;
    Shape        shape    = Shape::Circle;  // ShapeGate, ComboGate, SplitPath
    uint8_t      mask     = 0;              // LaneBlock, ComboGate
    int8_t       req_lane = 0;              // SplitPath
};

// Creates a new entity and emplaces the shared obstacle pre-bundle:
//   ObstacleTag + Velocity(0, speed) + DrawLayer(Layer::Game).
// Pass the returned entity to apply_obstacle_archetype to complete it.
entt::entity create_obstacle_base(entt::registry& reg, float speed);

// Emplace kind-specific components (Position, Obstacle, DrawSize, Color,
// and optional RequiredShape/RequiredVAction/RequiredLane/BlockedLanes) onto
// an already-created entity.  Callers are responsible for emplacing
// ObstacleTag, Velocity, DrawLayer, and BeatInfo (scheduler only) separately.
void apply_obstacle_archetype(entt::registry&               reg,
                               entt::entity                 entity,
                               const ObstacleArchetypeInput& in);
