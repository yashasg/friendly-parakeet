#pragma once
#include "../entities/camera_entity.h"
#include "../rendering/camera_resources.h"
#include <raylib.h>
#include <entt/entt.hpp>

namespace camera {

// Initialize camera, screen transform, and GPU meshes into registry.
void init(entt::registry& reg);

// Unload GPU meshes and shader.
void shutdown(entt::registry& reg);

} // namespace camera

// Update ScreenTransform (letterbox scale/offset) from the current window size.
// Called before input_system so coordinate transforms are never one-frame stale.
void compute_screen_transform(entt::registry& reg);
