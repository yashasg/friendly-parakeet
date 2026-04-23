#pragma once
#include "../components/camera.h"
#include "../components/shape_mesh.h"
#include <raylib.h>

namespace camera {

// GPU meshes — built once at startup from ShapeMeshData, drawn per frame.
struct ShapeMeshes {
    Mesh     shapes[4];   // indexed by Shape enum (prisms)
    Mesh     slab;        // unit slab for obstacles
    Mesh     quad;        // unit quad for particles
    Material material;    // shared material
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

} // namespace camera
