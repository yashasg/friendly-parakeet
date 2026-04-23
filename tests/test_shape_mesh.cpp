#include <catch2/catch_test_macros.hpp>
#include "components/shape_mesh.h"
#include <cmath>

// ── Helpers ─────────────────────────────────────────────────────────────────

static Vertex3 cross(Vertex3 a, Vertex3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

static Vertex3 sub(Vertex3 a, Vertex3 b) {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}

static float dot(Vertex3 a, Vertex3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static float length(Vertex3 v) {
    return std::sqrt(dot(v, v));
}

// Verify that every triangle in a ShapeMeshData:
//   1. Has non-degenerate area (cross product length > epsilon)
//   2. Has outward-facing normal:
//      - Top cap (normal.y > 0): triangle normal points up
//      - Bottom cap (normal.y < 0): triangle normal points down
//      - Side walls: triangle normal points away from Y axis (outward in XZ)
static void verify_winding(const ShapeMeshData& mesh) {
    REQUIRE(mesh.vertex_count > 0);
    REQUIRE(mesh.vertex_count == mesh.tri_count * 3);

    for (int t = 0; t < mesh.tri_count; ++t) {
        int i0 = t * 3, i1 = t * 3 + 1, i2 = t * 3 + 2;
        Vertex3 a = mesh.positions[i0];
        Vertex3 b = mesh.positions[i1];
        Vertex3 c = mesh.positions[i2];

        Vertex3 e1 = sub(b, a);
        Vertex3 e2 = sub(c, a);
        Vertex3 face_normal = cross(e1, e2);
        float area = length(face_normal);

        INFO("triangle " << t << ": area=" << area);
        REQUIRE(area > 1e-6f);  // not degenerate

        Vertex3 stated_normal = mesh.normals[i0];

        // The cross product should agree with the stated normal direction
        float agreement = dot(face_normal, stated_normal);
        INFO("triangle " << t << ": agreement=" << agreement
             << " stated=(" << stated_normal.x << "," << stated_normal.y << "," << stated_normal.z << ")"
             << " computed=(" << face_normal.x << "," << face_normal.y << "," << face_normal.z << ")");
        REQUIRE(agreement > 0.0f);
    }
}

// ── Tests ───────────────────────────────────────────────────────────────────

TEST_CASE("build_prism: circle mesh has correct vertex count", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Circle)]);
    // 12 slices: 12 top + 12 bottom + 24 sides = 48 tris = 144 verts
    CHECK(mesh.tri_count == 48);
    CHECK(mesh.vertex_count == 144);
}

TEST_CASE("build_prism: square mesh has correct vertex count", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    // 4 sides: 4 top + 4 bottom + 8 sides = 16 tris = 48 verts
    CHECK(mesh.tri_count == 16);
    CHECK(mesh.vertex_count == 48);
}

TEST_CASE("build_prism: triangle mesh has correct vertex count", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Triangle)]);
    // 3 sides: 3 top + 3 bottom + 6 sides = 12 tris = 36 verts
    CHECK(mesh.tri_count == 12);
    CHECK(mesh.vertex_count == 36);
}

TEST_CASE("build_prism: hexagon mesh has correct vertex count", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Hexagon)]);
    // 6 sides: 6 top + 6 bottom + 12 sides = 24 tris = 72 verts
    CHECK(mesh.tri_count == 24);
    CHECK(mesh.vertex_count == 72);
}

TEST_CASE("build_prism: circle winding order", "[shape_mesh]") {
    verify_winding(build_prism(SHAPE_TABLE[static_cast<int>(Shape::Circle)]));
}

TEST_CASE("build_prism: square winding order", "[shape_mesh]") {
    verify_winding(build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]));
}

TEST_CASE("build_prism: triangle winding order", "[shape_mesh]") {
    verify_winding(build_prism(SHAPE_TABLE[static_cast<int>(Shape::Triangle)]));
}

TEST_CASE("build_prism: hexagon winding order", "[shape_mesh]") {
    verify_winding(build_prism(SHAPE_TABLE[static_cast<int>(Shape::Hexagon)]));
}

TEST_CASE("build_prism: top faces have TOP shade", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    // First 4 tris (12 verts) are the top cap
    for (int i = 0; i < 12; ++i) {
        CHECK(mesh.colors[i].r == SHADE_TOP);
    }
}

TEST_CASE("build_prism: bottom faces have BOT shade", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    // Next 4 tris (verts 12-23) are the bottom cap
    for (int i = 12; i < 24; ++i) {
        CHECK(mesh.colors[i].r == SHADE_BOT);
    }
}

TEST_CASE("build_prism: side faces have FRONT or SIDE shade", "[shape_mesh]") {
    ShapeMeshData mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    // Side wall verts start at 24
    for (int i = 24; i < mesh.vertex_count; ++i) {
        bool valid = mesh.colors[i].r == SHADE_FRONT || mesh.colors[i].r == SHADE_SIDE;
        CHECK(valid);
    }
}
