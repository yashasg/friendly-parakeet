#pragma once

#include "../components/rendering.h"
#include <entt/entt.hpp>

// Spawn render child entities for each obstacle archetype.
// Call after creating the logical obstacle entity's per-kind rows.
void spawn_shape_gate_meshes(entt::registry& reg, entt::entity logical);
void spawn_split_path_meshes(entt::registry& reg, entt::entity logical);
void spawn_onset_marker_meshes(entt::registry& reg, entt::entity logical);


// Explicit obstacle lifetime helpers. Do not call from EnTT destroy listeners.
void destroy_obstacle_with_children(entt::registry& reg, entt::entity parent);
