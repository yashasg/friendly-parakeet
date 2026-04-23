#pragma once
#include "../components/shape_mesh.h"
#include <raylib.h>

namespace camera {

constexpr float WORLD_SCALE = 10.0f;

struct ShapeMeshes {
    Mesh     meshes[4];
    Material material;
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

} // namespace camera
