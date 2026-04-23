#pragma once
#include "../components/camera.h"
#include "../components/shape_mesh.h"
#include <raylib.h>

namespace camera {

// GPU shape meshes — built once at startup from ShapeMeshData, drawn per frame.
struct ShapeMeshes {
    Mesh     meshes[4];
    Material material;
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

} // namespace camera
