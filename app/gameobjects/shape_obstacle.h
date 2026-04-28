#pragma once

#include "../components/rendering.h"
#include <entt/entt.hpp>

// Spawn mesh child entities for multi-slab obstacle types.
// Call after creating the logical obstacle entity.
void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical);

// EnTT on_destroy listener: destroys all MeshChild entities referencing parent.
// Connect with: reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();
void on_obstacle_destroy(entt::registry& reg, entt::entity parent);
