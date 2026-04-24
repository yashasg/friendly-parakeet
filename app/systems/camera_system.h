#pragma once
#include "../components/camera.h"
#include <raylib.h>
#include <entt/entt.hpp>

namespace camera {

struct ShapeMeshes {
    Mesh     shapes[4];  // Circle, Square, Triangle, Hexagon
    Mesh     slab;       // unit cube for obstacles
    Mesh     quad;       // flat quad for particles
    Material material;   // shared material with directional shading shader
};

// Initialize camera, screen transform, and GPU meshes into registry.
void init(entt::registry& reg);

// Unload GPU meshes and shader.
void shutdown(entt::registry& reg);

} // namespace camera
