#pragma once

#include "../components/rendering.h"
#include <entt/entt.hpp>

// Spawn render child entities for multi-slab obstacle types.
// Call after creating the logical obstacle entity.
void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical);


// Explicit obstacle lifetime helpers. Do not call from EnTT destroy listeners.
void destroy_obstacle_mesh_children(entt::registry& reg, entt::entity parent);
void destroy_obstacle_with_children(entt::registry& reg, entt::entity parent);
