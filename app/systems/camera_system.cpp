#include "camera_system.h"
#include "../components/shape_mesh.h"
#include <raylib.h>
#include <rlgl.h>

namespace camera {

ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};
    for (int i = 0; i < 4; ++i) {
        sm.shapes[i] = build_prism(SHAPE_TABLE[i]);
        UploadMesh(&sm.shapes[i], false);
    }
    sm.slab = build_unit_slab();
    UploadMesh(&sm.slab, false);
    sm.quad = build_unit_quad();
    UploadMesh(&sm.quad, false);
    sm.material = LoadMaterialDefault();
    return sm;
}

void unload_shape_meshes(ShapeMeshes& sm) {
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.shapes[i]);
    UnloadMesh(sm.slab);
    UnloadMesh(sm.quad);
    UnloadMaterial(sm.material);
}

} // namespace camera
