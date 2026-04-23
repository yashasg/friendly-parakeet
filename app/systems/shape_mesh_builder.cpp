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

ShapeMeshData build_unit_slab() {
    // Unit slab: x=[0,1], y=[0,1], z=[0,1]. Non-uniform scale at draw time
    // encodes width (sx), height (sy), depth (sz) per obstacle.
    // Winding: all faces outward-facing (CCW from outside).
    ShapeMeshData mesh = {};
    auto put = [&](float x, float y, float z,
                   float nx, float ny, float nz, uint8_t gray) {
        int vi = mesh.vertex_count++;
        mesh.positions[vi] = {x, y, z};
        mesh.normals[vi]   = {nx, ny, nz};
        mesh.colors[vi]    = {gray, gray, gray, 255};
    };

    // Top face (+Y)
    put(0,1,0, 0,1,0, SHADE_TOP); put(1,1,1, 0,1,0, SHADE_TOP); put(1,1,0, 0,1,0, SHADE_TOP);
    put(0,1,0, 0,1,0, SHADE_TOP); put(0,1,1, 0,1,0, SHADE_TOP); put(1,1,1, 0,1,0, SHADE_TOP);
    // Bottom face (-Y)
    put(0,0,0, 0,-1,0, SHADE_BOT); put(1,0,0, 0,-1,0, SHADE_BOT); put(1,0,1, 0,-1,0, SHADE_BOT);
    put(0,0,0, 0,-1,0, SHADE_BOT); put(1,0,1, 0,-1,0, SHADE_BOT); put(0,0,1, 0,-1,0, SHADE_BOT);
    // Front face (-Z)
    put(0,0,0, 0,0,-1, SHADE_FRONT); put(0,1,0, 0,0,-1, SHADE_FRONT); put(1,1,0, 0,0,-1, SHADE_FRONT);
    put(0,0,0, 0,0,-1, SHADE_FRONT); put(1,1,0, 0,0,-1, SHADE_FRONT); put(1,0,0, 0,0,-1, SHADE_FRONT);
    // Back face (+Z)
    put(0,0,1, 0,0,1, SHADE_BOT); put(1,0,1, 0,0,1, SHADE_BOT); put(1,1,1, 0,0,1, SHADE_BOT);
    put(0,0,1, 0,0,1, SHADE_BOT); put(1,1,1, 0,0,1, SHADE_BOT); put(0,1,1, 0,0,1, SHADE_BOT);
    // Left face (-X)
    put(0,0,0, -1,0,0, SHADE_SIDE); put(0,0,1, -1,0,0, SHADE_SIDE); put(0,1,1, -1,0,0, SHADE_SIDE);
    put(0,0,0, -1,0,0, SHADE_SIDE); put(0,1,1, -1,0,0, SHADE_SIDE); put(0,1,0, -1,0,0, SHADE_SIDE);
    // Right face (+X)
    put(1,0,0, 1,0,0, SHADE_SIDE); put(1,1,0, 1,0,0, SHADE_SIDE); put(1,1,1, 1,0,0, SHADE_SIDE);
    put(1,0,0, 1,0,0, SHADE_SIDE); put(1,1,1, 1,0,0, SHADE_SIDE); put(1,0,1, 1,0,0, SHADE_SIDE);

    mesh.tri_count = mesh.vertex_count / 3;
    return mesh;
}

ShapeMeshData build_unit_quad() {
    // Unit quad: x=[0,1], z=[0,1] at y=0. For particles.
    ShapeMeshData mesh = {};
    auto put = [&](float x, float y, float z,
                   float nx, float ny, float nz, uint8_t gray) {
        int vi = mesh.vertex_count++;
        mesh.positions[vi] = {x, y, z};
        mesh.normals[vi]   = {nx, ny, nz};
        mesh.colors[vi]    = {gray, gray, gray, 255};
    };

    put(0,0,0, 0,1,0, 255); put(1,0,1, 0,1,0, 255); put(1,0,0, 0,1,0, 255);
    put(0,0,0, 0,1,0, 255); put(0,0,1, 0,1,0, 255); put(1,0,1, 0,1,0, 255);

    mesh.tri_count = mesh.vertex_count / 3;
    return mesh;
}
