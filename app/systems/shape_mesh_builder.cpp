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

    // Top cap: normal = (0,+1,0).
    // The 2D cross of ring vectors gives the opposite sign from the 3D
    // triangle cross product Y component (because XZ→3D mapping flips it).
    // So: 2D cross > 0 means 3D cross points DOWN → need reversed order.
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        float cross_y = ring[i].x * ring[nx].y - ring[i].y * ring[nx].x;
        if (cross_y > 0) {
            // 3D cross points down — reverse to get upward normal
            put(0, height, 0,                   0,1,0, SHADE_TOP);
            put(ring[nx].x, height, ring[nx].y, 0,1,0, SHADE_TOP);
            put(ring[i].x, height, ring[i].y,   0,1,0, SHADE_TOP);
        } else {
            put(0, height, 0,                   0,1,0, SHADE_TOP);
            put(ring[i].x, height, ring[i].y,   0,1,0, SHADE_TOP);
            put(ring[nx].x, height, ring[nx].y, 0,1,0, SHADE_TOP);
        }
    }

    // Bottom cap: normal = (0,-1,0).  Opposite winding from top.
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        float cross_y = ring[i].x * ring[nx].y - ring[i].y * ring[nx].x;
        if (cross_y > 0) {
            put(0, 0, 0,                   0,-1,0, SHADE_BOT);
            put(ring[i].x, 0, ring[i].y,   0,-1,0, SHADE_BOT);
            put(ring[nx].x, 0, ring[nx].y, 0,-1,0, SHADE_BOT);
        } else {
            put(0, 0, 0,                   0,-1,0, SHADE_BOT);
            put(ring[nx].x, 0, ring[nx].y, 0,-1,0, SHADE_BOT);
            put(ring[i].x, 0, ring[i].y,   0,-1,0, SHADE_BOT);
        }
    }

    // Side walls: 2 tris per edge.
    // Outward normal = (ring[i] + ring[nx]) / 2 in XZ.
    // Winding must produce a cross product pointing outward.
    for (int i = 0; i < n; ++i) {
        int nx = (i + 1) % n;
        float fnx = (ring[i].x + ring[nx].x) * 0.5f;
        float fnz = (ring[i].y + ring[nx].y) * 0.5f;
        uint8_t gray = (fnz < 0) ? SHADE_FRONT : SHADE_SIDE;

        // Tri 1: A=ring[i] bottom, B=ring[i] top, C=ring[nx] top
        // edge1 = B-A = (0, height, 0)
        // edge2 = C-A = (ring[nx].x - ring[i].x, height, ring[nx].y - ring[i].y)
        // cross = (height*(rnz-rz) - 0, 0 - height*(rnx-rx), 0)
        //       = (height*dz, -height*dx, 0)  where dx=rnx-rx, dz=rnz-rz
        // For outward: dot(cross_xz, normal_xz) > 0
        // cross_xz = (height*dz, -height*dx), normal = (fnx, fnz)
        // dot = height*dz*fnx + (-height*dx)*fnz = height*(dz*fnx - dx*fnz)
        float dx = ring[nx].x - ring[i].x;
        float dz = ring[nx].y - ring[i].y;
        float dot = dz * fnx - dx * fnz;

        // dot > 0 means the default order points outward (correct)
        // dot < 0 means we need to reverse
        if (dot >= 0) {
            put(ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
            put(ring[i].x,  height, ring[i].y,  fnx,0,fnz, gray);
            put(ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);

            put(ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
            put(ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);
            put(ring[nx].x, 0,      ring[nx].y, fnx,0,fnz, gray);
        } else {
            put(ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
            put(ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);
            put(ring[i].x,  height, ring[i].y,  fnx,0,fnz, gray);

            put(ring[i].x,  0,      ring[i].y,  fnx,0,fnz, gray);
            put(ring[nx].x, 0,      ring[nx].y, fnx,0,fnz, gray);
            put(ring[nx].x, height, ring[nx].y, fnx,0,fnz, gray);
        }
    }

    mesh.tri_count = mesh.vertex_count / 3;
    return mesh;
}
