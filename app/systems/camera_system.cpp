#include "camera_system.h"
#include "../components/shape_mesh.h"
#include <raylib.h>
#include <rlgl.h>
#include <cstdlib>
#include <cstring>

namespace camera {

static Mesh upload_shape_mesh(const ShapeMeshData& data) {
    int vc = data.vertex_count;
    Mesh mesh = {};
    mesh.vertexCount   = vc;
    mesh.triangleCount = data.tri_count;

    // ShapeMeshData arrays are laid out identically to raylib's flat arrays:
    //   Vertex3{x,y,z}      = 3 contiguous floats  = mesh.vertices layout
    //   VertexColor{r,g,b,a} = 4 contiguous bytes   = mesh.colors layout
    auto vb = static_cast<unsigned int>(vc);
    mesh.vertices  = static_cast<float*>(RL_CALLOC(vb * 3, sizeof(float)));
    mesh.normals   = static_cast<float*>(RL_CALLOC(vb * 3, sizeof(float)));
    mesh.texcoords = static_cast<float*>(RL_CALLOC(vb * 2, sizeof(float)));
    mesh.colors    = static_cast<unsigned char*>(RL_CALLOC(vb * 4, sizeof(unsigned char)));

    memcpy(mesh.vertices, data.positions, static_cast<size_t>(vc) * 3 * sizeof(float));
    memcpy(mesh.normals,  data.normals,   static_cast<size_t>(vc) * 3 * sizeof(float));
    memcpy(mesh.colors,   data.colors,    static_cast<size_t>(vc) * 4);

    UploadMesh(&mesh, false);
    return mesh;
}

ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};
    for (int i = 0; i < 4; ++i)
        sm.shapes[i] = upload_shape_mesh(build_prism(SHAPE_TABLE[i]));
    sm.slab = upload_shape_mesh(build_unit_slab());
    sm.quad = upload_shape_mesh(build_unit_quad());
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
