#include "../components/shape_mesh.h"
#include <cstdlib>

static constexpr shape_verts::V2 CIRCLE_12[12] = {
    { 1.000000f,  0.000000f}, { 0.866025f,  0.500000f},
    { 0.500000f,  0.866025f}, { 0.000000f,  1.000000f},
    {-0.500000f,  0.866025f}, {-0.866025f,  0.500000f},
    {-1.000000f,  0.000000f}, {-0.866025f, -0.500000f},
    {-0.500000f, -0.866025f}, { 0.000000f, -1.000000f},
    { 0.500000f, -0.866025f}, { 0.866025f, -0.500000f},
};

const ShapeDesc SHAPE_TABLE[4] = {
    { CIRCLE_12,              12, 0.5f, 0.3f },
    { shape_verts::SQUARE,     4, 0.5f, 0.3f },
    { shape_verts::TRIANGLE,   3, 0.5f, 0.3f },
    { shape_verts::HEXAGON,    6, 0.6f, 0.7f },
};

// Allocate a Mesh with space for `vert_count` vertices.
static Mesh alloc_mesh(int vert_count) {
    Mesh mesh = {};
    mesh.vertexCount   = vert_count;
    mesh.triangleCount = vert_count / 3;
    mesh.vertices  = static_cast<float*>(RL_CALLOC(static_cast<size_t>(vert_count) * 3, sizeof(float)));
    mesh.normals   = static_cast<float*>(RL_CALLOC(static_cast<size_t>(vert_count) * 3, sizeof(float)));
    mesh.texcoords = static_cast<float*>(RL_CALLOC(static_cast<size_t>(vert_count) * 2, sizeof(float)));
    mesh.colors    = static_cast<unsigned char*>(RL_CALLOC(static_cast<size_t>(vert_count) * 4, sizeof(unsigned char)));
    return mesh;
}

// Write one vertex into a Mesh at index `vi`.
static void put(Mesh& mesh, int& vi,
                float x, float y, float z,
                float nx, float ny, float nz,
                uint8_t gray) {
    mesh.vertices[vi*3+0] = x;  mesh.vertices[vi*3+1] = y;  mesh.vertices[vi*3+2] = z;
    mesh.normals[vi*3+0] = nx;  mesh.normals[vi*3+1] = ny;  mesh.normals[vi*3+2] = nz;
    mesh.colors[vi*4+0] = gray; mesh.colors[vi*4+1] = gray;
    mesh.colors[vi*4+2] = gray; mesh.colors[vi*4+3] = 255;
    ++vi;
}

Mesh build_prism(const ShapeDesc& desc) {
    const int n = desc.n;
    Mesh mesh = alloc_mesh(4 * n * 3);
    int vi = 0;
    const float height = desc.height_scale / desc.radius_scale;
    const auto* ring = desc.ring;

    // Top cap
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        put(mesh, vi, 0, height, 0,                   0,1,0, SHADE_TOP);
        put(mesh, vi, ring[nx].x, height, ring[nx].y, 0,1,0, SHADE_TOP);
        put(mesh, vi, ring[i].x,  height, ring[i].y,  0,1,0, SHADE_TOP);
    }
    // Bottom cap
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        put(mesh, vi, 0, 0, 0,                   0,-1,0, SHADE_BOT);
        put(mesh, vi, ring[i].x, 0, ring[i].y,   0,-1,0, SHADE_BOT);
        put(mesh, vi, ring[nx].x, 0, ring[nx].y, 0,-1,0, SHADE_BOT);
    }
    // Side walls
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        float fnx = (ring[i].x + ring[nx].x) * 0.5f;
        float fnz = (ring[i].y + ring[nx].y) * 0.5f;
        uint8_t gray = (fnz < 0) ? SHADE_FRONT : SHADE_SIDE;

        put(mesh, vi, ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
        put(mesh, vi, ring[i].x,  height, ring[i].y,  fnx,0,fnz, gray);
        put(mesh, vi, ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);

        put(mesh, vi, ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
        put(mesh, vi, ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);
        put(mesh, vi, ring[nx].x, 0,      ring[nx].y, fnx,0,fnz, gray);
    }
    return mesh;
}

Mesh build_unit_slab() {
    Mesh mesh = alloc_mesh(36);
    int vi = 0;
    // Top (+Y)
    put(mesh,vi, 0,1,0, 0,1,0, SHADE_TOP); put(mesh,vi, 1,1,1, 0,1,0, SHADE_TOP); put(mesh,vi, 1,1,0, 0,1,0, SHADE_TOP);
    put(mesh,vi, 0,1,0, 0,1,0, SHADE_TOP); put(mesh,vi, 0,1,1, 0,1,0, SHADE_TOP); put(mesh,vi, 1,1,1, 0,1,0, SHADE_TOP);
    // Bottom (-Y)
    put(mesh,vi, 0,0,0, 0,-1,0, SHADE_BOT); put(mesh,vi, 1,0,0, 0,-1,0, SHADE_BOT); put(mesh,vi, 1,0,1, 0,-1,0, SHADE_BOT);
    put(mesh,vi, 0,0,0, 0,-1,0, SHADE_BOT); put(mesh,vi, 1,0,1, 0,-1,0, SHADE_BOT); put(mesh,vi, 0,0,1, 0,-1,0, SHADE_BOT);
    // Front (-Z)
    put(mesh,vi, 0,0,0, 0,0,-1, SHADE_FRONT); put(mesh,vi, 0,1,0, 0,0,-1, SHADE_FRONT); put(mesh,vi, 1,1,0, 0,0,-1, SHADE_FRONT);
    put(mesh,vi, 0,0,0, 0,0,-1, SHADE_FRONT); put(mesh,vi, 1,1,0, 0,0,-1, SHADE_FRONT); put(mesh,vi, 1,0,0, 0,0,-1, SHADE_FRONT);
    // Back (+Z)
    put(mesh,vi, 0,0,1, 0,0,1, SHADE_BOT); put(mesh,vi, 1,0,1, 0,0,1, SHADE_BOT); put(mesh,vi, 1,1,1, 0,0,1, SHADE_BOT);
    put(mesh,vi, 0,0,1, 0,0,1, SHADE_BOT); put(mesh,vi, 1,1,1, 0,0,1, SHADE_BOT); put(mesh,vi, 0,1,1, 0,0,1, SHADE_BOT);
    // Left (-X)
    put(mesh,vi, 0,0,0, -1,0,0, SHADE_SIDE); put(mesh,vi, 0,0,1, -1,0,0, SHADE_SIDE); put(mesh,vi, 0,1,1, -1,0,0, SHADE_SIDE);
    put(mesh,vi, 0,0,0, -1,0,0, SHADE_SIDE); put(mesh,vi, 0,1,1, -1,0,0, SHADE_SIDE); put(mesh,vi, 0,1,0, -1,0,0, SHADE_SIDE);
    // Right (+X)
    put(mesh,vi, 1,0,0, 1,0,0, SHADE_SIDE); put(mesh,vi, 1,1,0, 1,0,0, SHADE_SIDE); put(mesh,vi, 1,1,1, 1,0,0, SHADE_SIDE);
    put(mesh,vi, 1,0,0, 1,0,0, SHADE_SIDE); put(mesh,vi, 1,1,1, 1,0,0, SHADE_SIDE); put(mesh,vi, 1,0,1, 1,0,0, SHADE_SIDE);
    return mesh;
}

Mesh build_unit_quad() {
    Mesh mesh = alloc_mesh(6);
    int vi = 0;
    put(mesh,vi, 0,0,0, 0,1,0, 255); put(mesh,vi, 1,0,1, 0,1,0, 255); put(mesh,vi, 1,0,0, 0,1,0, 255);
    put(mesh,vi, 0,0,0, 0,1,0, 255); put(mesh,vi, 0,0,1, 0,1,0, 255); put(mesh,vi, 1,0,1, 0,1,0, 255);
    return mesh;
}
