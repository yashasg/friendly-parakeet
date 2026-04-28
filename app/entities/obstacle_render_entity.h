#pragma once

#include "../components/rendering.h"
#include <entt/entt.hpp>

// Spawn render child entities for multi-slab obstacle types.
// Call after creating the logical obstacle entity.
void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical);

void build_obstacle_model(entt::registry& reg, entt::entity logical);

void on_obstacle_model_destroy(entt::registry& reg, entt::entity entity);
void wire_obstacle_model_lifecycle(entt::registry& reg);
void unwire_obstacle_model_lifecycle(entt::registry& reg);

// EnTT on_destroy listener: destroys all MeshChild entities referencing parent.
// Connect with: reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();
void on_obstacle_destroy(entt::registry& reg, entt::entity parent);
