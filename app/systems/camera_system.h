#pragma once
#include <entt/entt.hpp>

namespace camera {

// Initialize camera, screen transform, and GPU meshes into registry.
// Safe to call more than once on the same registry; context singletons are
// inserted-or-assigned.
void init(entt::registry& reg);

// Unload GPU meshes and shader.
void shutdown(entt::registry& reg);

} // namespace camera

// Update ScreenTransform (letterbox scale/offset) from the current window size.
// Called before input_system so coordinate transforms are never one-frame stale.
void compute_screen_transform(entt::registry& reg);
