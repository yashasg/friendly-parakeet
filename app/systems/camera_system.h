#pragma once
#include "../components/camera.h"
#include "../components/shape_mesh.h"
#include <raylib.h>

namespace camera {

struct ShapeMeshes {
    Mesh     shapes[4];
    Mesh     slab;
    Mesh     quad;
    Material material;
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

} // namespace camera
