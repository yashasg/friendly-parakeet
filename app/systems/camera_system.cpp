#include "camera_system.h"
#include "../components/shape_mesh.h"
#include <raylib.h>
#include <rlgl.h>
#include <cstdlib>

namespace camera {

static Mesh upload_shape_mesh(const ShapeMeshData& data) {
    int vc = data.vertex_count;
    Mesh mesh = {};
    mesh.vertexCount   = vc;
    mesh.triangleCount = data.tri_count;
    mesh.vertices  = static_cast<float*>(RL_CALLOC(static_cast<unsigned int>(vc * 3), sizeof(float)));
    mesh.normals   = static_cast<float*>(RL_CALLOC(static_cast<unsigned int>(vc * 3), sizeof(float)));
    mesh.texcoords = static_cast<float*>(RL_CALLOC(static_cast<unsigned int>(vc * 2), sizeof(float)));
    mesh.colors    = static_cast<unsigned char*>(RL_CALLOC(static_cast<unsigned int>(vc * 4), sizeof(unsigned char)));

    for (int i = 0; i < vc; ++i) {
        mesh.vertices[i*3+0] = data.positions[i].x;
        mesh.vertices[i*3+1] = data.positions[i].y;
        mesh.vertices[i*3+2] = data.positions[i].z;
        mesh.normals[i*3+0]  = data.normals[i].x;
        mesh.normals[i*3+1]  = data.normals[i].y;
        mesh.normals[i*3+2]  = data.normals[i].z;
        mesh.colors[i*4+0]   = data.colors[i].r;
        mesh.colors[i*4+1]   = data.colors[i].g;
        mesh.colors[i*4+2]   = data.colors[i].b;
        mesh.colors[i*4+3]   = data.colors[i].a;
    }

    UploadMesh(&mesh, false);
    return mesh;
}

ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};
    for (int i = 0; i < 4; ++i)
        sm.meshes[i] = upload_shape_mesh(build_prism(SHAPE_TABLE[i]));
    sm.material = LoadMaterialDefault();
    return sm;
}

void unload_shape_meshes(ShapeMeshes& sm) {
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.meshes[i]);
    UnloadMaterial(sm.material);
}

} // namespace camera
