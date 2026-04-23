#include "../components/shape_mesh.h"

// ── 12-point circle ring (30° increments, unit radius) ──────────────────────
static constexpr shape_verts::V2 CIRCLE_12[12] = {
    { 1.000000f,  0.000000f}, { 0.866025f,  0.500000f},
    { 0.500000f,  0.866025f}, { 0.000000f,  1.000000f},
    {-0.500000f,  0.866025f}, {-0.866025f,  0.500000f},
    {-1.000000f,  0.000000f}, {-0.866025f, -0.500000f},
    {-0.500000f, -0.866025f}, { 0.000000f, -1.000000f},
    { 0.500000f, -0.866025f}, { 0.866025f, -0.500000f},
};

const ShapeDesc SHAPE_TABLE[4] = {
    { CIRCLE_12,              12, 0.5f, 0.3f },   // Circle
    { shape_verts::SQUARE,     4, 0.5f, 0.3f },   // Square
    { shape_verts::TRIANGLE,   3, 0.5f, 0.3f },   // Triangle
    { shape_verts::HEXAGON,    6, 0.6f, 0.7f },   // Hexagon
};

ShapeMeshData build_prism(const ShapeDesc& desc) {
    ShapeMeshData mesh = {};
    const int n = desc.n;
    const float height = desc.height_scale / desc.radius_scale;
    const auto* ring = desc.ring;

    auto put = [&](float x, float y, float z,
                   float nx, float ny, float nz,
                   uint8_t gray) {
        int vi = mesh.vertex_count++;
        mesh.positions[vi] = {x, y, z};
        mesh.normals[vi]   = {nx, ny, nz};
        mesh.colors[vi]    = {gray, gray, gray, 255};
    };

    // All rings in shape_vertices.h are stored in CCW order (positive 2D
    // cross product for consecutive pairs).  This gives consistent OpenGL
    // front-face winding without any runtime cross-product checks.
    //
    // Top cap: (center, next, i) → cross product points +Y (outward) ✓
    // Bottom cap: (center, i, next) → cross product points -Y (outward) ✓
    // Side walls: (i_bot, i_top, next_top) → cross product points outward ✓

    // Top cap
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        put(0, height, 0,                   0,1,0, SHADE_TOP);
        put(ring[nx].x, height, ring[nx].y, 0,1,0, SHADE_TOP);
        put(ring[i].x,  height, ring[i].y,  0,1,0, SHADE_TOP);
    }

    // Bottom cap
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        put(0, 0, 0,                   0,-1,0, SHADE_BOT);
        put(ring[i].x, 0, ring[i].y,   0,-1,0, SHADE_BOT);
        put(ring[nx].x, 0, ring[nx].y, 0,-1,0, SHADE_BOT);
    }

    // Side walls: 2 tris per edge
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        float fnx = (ring[i].x + ring[nx].x) * 0.5f;
        float fnz = (ring[i].y + ring[nx].y) * 0.5f;
        uint8_t gray = (fnz < 0) ? SHADE_FRONT : SHADE_SIDE;

        put(ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
        put(ring[i].x,  height, ring[i].y,  fnx,0,fnz, gray);
        put(ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);

        put(ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
        put(ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);
        put(ring[nx].x, 0,      ring[nx].y, fnx,0,fnz, gray);
    }

    mesh.tri_count = mesh.vertex_count / 3;
    return mesh;
}
