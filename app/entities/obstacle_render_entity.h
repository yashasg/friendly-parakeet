#pragma once

#include "../components/rendering.h"
#include <entt/entt.hpp>

// Spawn render child entities for multi-slab obstacle types.
// Call after creating the logical obstacle entity.
void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical);

void build_obstacle_model(entt::registry& reg, entt::entity logical);

// Centralized mesh-child lifetime wiring. Safe to call more than once.
void wire_obstacle_mesh_lifetime(entt::registry& reg);
void unwire_obstacle_mesh_lifetime(entt::registry& reg);

void on_obstacle_model_destroy(entt::registry& reg, entt::entity entity);
void wire_obstacle_model_lifecycle(entt::registry& reg);
void unwire_obstacle_model_lifecycle(entt::registry& reg);

// EnTT listener: destroys MeshChild entities recorded by ObstacleChildren.
// Prefer wire_obstacle_mesh_lifetime() over connecting this directly.
void on_obstacle_destroy(entt::registry& reg, entt::entity parent);
