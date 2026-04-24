#pragma once

#include "../components/rendering.h"
#include <entt/entt.hpp>

// Maximum mesh children per game object.
constexpr int MAX_MESH_CHILDREN = 4;

// Persistent component on the logical obstacle entity.
// Holds IDs of visual mesh child entities for lifecycle management.
struct ShapeObstacleGO {
    entt::entity meshes[MAX_MESH_CHILDREN] = {
        entt::null, entt::null, entt::null, entt::null
    };
    int mesh_count = 0;
};

// Spawn mesh child entities for multi-slab obstacle types.
// Attaches ShapeObstacleGO to the logical entity.
// Only call for: ShapeGate, LaneBlock, ComboGate, SplitPath.
void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical);

// Destroy all mesh children referenced by ShapeObstacleGO.
void destroy_obstacle_meshes(entt::registry& reg, entt::entity logical);
