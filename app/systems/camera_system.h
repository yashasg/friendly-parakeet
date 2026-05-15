#pragma once
#include "../components/rendering.h"
#include "../entities/camera_entity.h"
#include "../rendering/camera_resources.h"
#include <cassert>
#include <cmath>
#include <entt/entt.hpp>
#include <raylib.h>

namespace camera {

// Initialize camera, screen transform, and GPU meshes into registry.
void init(entt::registry& reg);

// Unload GPU meshes and shader.
void shutdown(entt::registry& reg);

} // namespace camera

// Update ScreenTransform (letterbox scale/offset) from the current window size.
// Called before input_system so coordinate transforms are never one-frame stale.
void compute_screen_transform(entt::registry& reg);

// Convert a window-space pixel position into the virtual coordinate space using
// the current letterbox scale/offset. Lives in the camera system because the
// transform is owned and updated there; components/rendering.h stays data-only.
[[nodiscard]] inline Vector2 screen_to_virtual(const Vector2& screen_pos,
                                                const ScreenTransform& st) noexcept {
    assert(std::isfinite(st.scale));
    assert(st.scale > 0.0f);
    const float inv_scale = 1.0f / st.scale;
    return {
        (screen_pos.x - st.offset_x) * inv_scale,
        (screen_pos.y - st.offset_y) * inv_scale
    };
}
