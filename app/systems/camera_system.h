#pragma once
#include "../components/camera.h"
#include <raylib.h>
#include <entt/entt.hpp>

namespace camera {

// RAII owner for GPU mesh/shader resources.
// Destructor calls unload only when owned == true, so both the manual
// camera::shutdown path and registry-ctx destruction are safe to run in
// any order without double-unloading.
struct ShapeMeshes {
    Mesh     shapes[4] = {};  // Circle, Square, Triangle, Hexagon
    Mesh     slab      = {};  // unit cube for obstacles
    Mesh     quad      = {};  // flat quad for particles
    Material material  = {};  // shared material with directional shading shader
    bool     owned     = false;

    ShapeMeshes() = default;
    ShapeMeshes(const ShapeMeshes&)            = delete;
    ShapeMeshes& operator=(const ShapeMeshes&) = delete;
    ShapeMeshes(ShapeMeshes&&) noexcept;
    ShapeMeshes& operator=(ShapeMeshes&&) noexcept;
    ~ShapeMeshes();

    // Idempotent GPU unload. Safe to call more than once; clears owned flag.
    void release();
};

// Initialize camera, screen transform, and GPU meshes into registry.
void init(entt::registry& reg);

// Unload GPU meshes and shader.
void shutdown(entt::registry& reg);

} // namespace camera

// Update ScreenTransform (letterbox scale/offset) from the current window size.
// Called before input_system so coordinate transforms are never one-frame stale.
void compute_screen_transform(entt::registry& reg);
