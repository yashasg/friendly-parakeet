#pragma once
#include "../components/camera.h"
#include "../components/shape_mesh.h"
#include <entt/entt.hpp>
#include <raylib.h>

namespace camera {

// GPU meshes — built once at startup, drawn per frame.
struct ShapeMeshes {
    Mesh     shapes[4];
    Mesh     slab;
    Mesh     quad;
    Material material;
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

// Update the letterbox ScreenTransform from current window/canvas size.
void update_screen_transform(entt::registry& reg);

} // namespace camera
