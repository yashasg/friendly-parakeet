#include <catch2/catch_test_macros.hpp>
#include "components/shape_mesh.h"
#include <cmath>
#include <cstdlib>

// ── Helpers ─────────────────────────────────────────────────────────────────

struct Vec3 { float x, y, z; };

static Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static float len(Vec3 v) { return std::sqrt(dot(v, v)); }

static Vec3 get_pos(const Mesh& m, int i) {
    return {m.vertices[i*3], m.vertices[i*3+1], m.vertices[i*3+2]};
}
static Vec3 get_normal(const Mesh& m, int i) {
    return {m.normals[i*3], m.normals[i*3+1], m.normals[i*3+2]};
}

// Verify every triangle has outward-facing winding (cross product agrees
// with stated normal).
static void verify_winding(Mesh& mesh) {
    REQUIRE(mesh.vertexCount > 0);
    REQUIRE(mesh.vertexCount == mesh.triangleCount * 3);

    for (int t = 0; t < mesh.triangleCount; ++t) {
        int i0 = t*3, i1 = t*3+1, i2 = t*3+2;
        Vec3 a = get_pos(mesh, i0);
        Vec3 b = get_pos(mesh, i1);
        Vec3 c = get_pos(mesh, i2);
        Vec3 face_normal = cross(sub(b, a), sub(c, a));
        float area = len(face_normal);

        INFO("triangle " << t << ": area=" << area);
        REQUIRE(area > 1e-6f);

        Vec3 stated = get_normal(mesh, i0);
        float agreement = dot(face_normal, stated);
        INFO("triangle " << t << ": agreement=" << agreement);
        REQUIRE(agreement > 0.0f);
    }
    // Free CPU-side mesh data (not GPU-uploaded in tests)
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

// ── Tests ───────────────────────────────────────────────────────────────────

TEST_CASE("build_prism: circle mesh has correct vertex count", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Circle)]);
    CHECK(mesh.triangleCount == 48);
    CHECK(mesh.vertexCount == 144);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_prism: square mesh has correct vertex count", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    CHECK(mesh.triangleCount == 16);
    CHECK(mesh.vertexCount == 48);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_prism: triangle mesh has correct vertex count", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Triangle)]);
    CHECK(mesh.triangleCount == 12);
    CHECK(mesh.vertexCount == 36);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_prism: hexagon mesh has correct vertex count", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Hexagon)]);
    CHECK(mesh.triangleCount == 24);
    CHECK(mesh.vertexCount == 72);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_prism: circle winding order", "[shape_mesh]") {
    Mesh m = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Circle)]);
    verify_winding(m);
}

TEST_CASE("build_prism: square winding order", "[shape_mesh]") {
    Mesh m = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    verify_winding(m);
}

TEST_CASE("build_prism: triangle winding order", "[shape_mesh]") {
    Mesh m = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Triangle)]);
    verify_winding(m);
}

TEST_CASE("build_prism: hexagon winding order", "[shape_mesh]") {
    Mesh m = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Hexagon)]);
    verify_winding(m);
}

TEST_CASE("build_prism: top faces have TOP shade", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    for (int i = 0; i < 12; ++i)
        CHECK(mesh.colors[i*4] == SHADE_TOP);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_prism: bottom faces have BOT shade", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    for (int i = 12; i < 24; ++i)
        CHECK(mesh.colors[i*4] == SHADE_BOT);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_prism: side faces have FRONT or SIDE shade", "[shape_mesh]") {
    Mesh mesh = build_prism(SHAPE_TABLE[static_cast<int>(Shape::Square)]);
    for (int i = 24; i < mesh.vertexCount; ++i) {
        bool valid = mesh.colors[i*4] == SHADE_FRONT || mesh.colors[i*4] == SHADE_SIDE;
        CHECK(valid);
    }
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_unit_slab: 12 triangles, 36 vertices", "[shape_mesh]") {
    Mesh mesh = build_unit_slab();
    CHECK(mesh.triangleCount == 12);
    CHECK(mesh.vertexCount == 36);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_unit_slab: winding order", "[shape_mesh]") {
    Mesh m = build_unit_slab();
    verify_winding(m);
}

TEST_CASE("build_unit_quad: 2 triangles, 6 vertices", "[shape_mesh]") {
    Mesh mesh = build_unit_quad();
    CHECK(mesh.triangleCount == 2);
    CHECK(mesh.vertexCount == 6);
    RL_FREE(mesh.vertices); RL_FREE(mesh.normals);
    RL_FREE(mesh.texcoords); RL_FREE(mesh.colors);
}

TEST_CASE("build_unit_quad: winding order", "[shape_mesh]") {
    Mesh m = build_unit_quad();
    verify_winding(m);
}
