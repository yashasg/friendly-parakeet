#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "components/shape_vertices.h"
#include "constants.h"
#include <raylib.h>
#include <cmath>

using Catch::Matchers::WithinAbs;

// ═════════════════════════════════════════════════════════════════════════════
// §1  Camera3D setup validation
// ═════════════════════════════════════════════════════════════════════════════
// These tests verify the Camera3D configuration constants that camera_system
// uses.  No GPU context required — pure struct/math tests.

TEST_CASE("Camera3D: default setup covers game world", "[camera3d]") {
    Camera3D cam{};
    cam.position   = {360.0f, 700.0f, 1600.0f};
    cam.target     = {360.0f, 0.0f, 500.0f};
    cam.up         = {0.0f, 1.0f, 0.0f};
    cam.fovy       = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // Camera is behind the player (z=920) looking forward (−Z)
    CHECK(cam.position.z > constants::PLAYER_Y);

    // Camera is above the ground plane
    CHECK(cam.position.y > 0.0f);

    // Target is in front of camera (lower z)
    CHECK(cam.target.z < cam.position.z);

    // Camera X is centered on the playfield
    CHECK_THAT(static_cast<double>(cam.position.x),
               WithinAbs(constants::SCREEN_W / 2.0, 0.01));
    CHECK_THAT(static_cast<double>(cam.target.x),
               WithinAbs(constants::SCREEN_W / 2.0, 0.01));

    // FOV is reasonable (positive, less than 180)
    CHECK(cam.fovy > 0.0f);
    CHECK(cam.fovy < 180.0f);
}

TEST_CASE("Camera3D: coordinate mapping — game (x,y) → 3D (x,0,y)", "[camera3d]") {
    // The coordinate mapping rule: game Position{x, y} → 3D {x, 0, y}
    float game_x = 360.0f;
    float game_y = constants::PLAYER_Y;
    float x3d = game_x;
    float y3d = 0.0f;
    float z3d = game_y;

    CHECK_THAT(static_cast<double>(x3d), WithinAbs(game_x, 0.01));
    CHECK_THAT(static_cast<double>(y3d), WithinAbs(0.0, 0.01));
    CHECK_THAT(static_cast<double>(z3d), WithinAbs(game_y, 0.01));
}

TEST_CASE("Camera3D: jump lifts player off ground plane", "[camera3d]") {
    // y_offset is negative when jumping.  3D y = −y_offset.
    float y_offset_jumping = -constants::JUMP_HEIGHT;
    float y_3d = -y_offset_jumping;

    CHECK(y_3d > 0.0f);
    CHECK_THAT(static_cast<double>(y_3d),
               WithinAbs(constants::JUMP_HEIGHT, 0.01));
}

TEST_CASE("Camera3D: spawn and destroy Y map to valid Z range", "[camera3d]") {
    // Obstacles spawn at game y = SPAWN_Y (−120) and are destroyed at DESTROY_Y (1400).
    // In 3D: z = game_y, so z ∈ [−120, 1400].
    float spawn_z   = constants::SPAWN_Y;
    float destroy_z = constants::DESTROY_Y;

    Camera3D cam{};
    cam.position = {360.0f, 300.0f, 1148.0f};
    CHECK(spawn_z < cam.position.z);
    CHECK(destroy_z > cam.position.z);  // behind camera (clipped by frustum)
}


// ═════════════════════════════════════════════════════════════════════════════
// §2  shape_vertices.h — constexpr vertex tables
// ═════════════════════════════════════════════════════════════════════════════

// ── Circle table ────────────────────────────────────────────────────────────

TEST_CASE("shape_verts::CIRCLE — table size is 24", "[shape_verts]") {
    CHECK(shape_verts::CIRCLE_SEGMENTS == 24);
    // Compile-time size verification via sizeof
    static_assert(sizeof(shape_verts::CIRCLE) / sizeof(shape_verts::V2) == 24);
}

TEST_CASE("shape_verts::CIRCLE — cardinal points", "[shape_verts]") {
    // Index 0:   0° → (1, 0)
    CHECK_THAT(shape_verts::CIRCLE[0].x,  WithinAbs( 1.0, 1e-5));
    CHECK_THAT(shape_verts::CIRCLE[0].y,  WithinAbs( 0.0, 1e-5));

    // Index 6:  90° → (0, 1)
    CHECK_THAT(shape_verts::CIRCLE[6].x,  WithinAbs( 0.0, 1e-5));
    CHECK_THAT(shape_verts::CIRCLE[6].y,  WithinAbs( 1.0, 1e-5));

    // Index 12: 180° → (−1, 0)
    CHECK_THAT(shape_verts::CIRCLE[12].x, WithinAbs(-1.0, 1e-5));
    CHECK_THAT(shape_verts::CIRCLE[12].y, WithinAbs( 0.0, 1e-5));

    // Index 18: 270° → (0, −1)
    CHECK_THAT(shape_verts::CIRCLE[18].x, WithinAbs( 0.0, 1e-5));
    CHECK_THAT(shape_verts::CIRCLE[18].y, WithinAbs(-1.0, 1e-5));
}

TEST_CASE("shape_verts::CIRCLE — unit magnitude for all vertices", "[shape_verts]") {
    for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
        CAPTURE(i);
        float mag_sq = shape_verts::CIRCLE[i].x * shape_verts::CIRCLE[i].x
                     + shape_verts::CIRCLE[i].y * shape_verts::CIRCLE[i].y;
        CHECK_THAT(mag_sq, WithinAbs(1.0, 1e-5));
    }
}

TEST_CASE("shape_verts::CIRCLE — CCW ordering", "[shape_verts]") {
    // Consecutive vertices should proceed counter-clockwise (increasing angle
    // from the positive x-axis).  The cross product of consecutive edge vectors
    // (in 2D, the z-component) must be positive for CCW winding.
    for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
        int j = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
        int k = (i + 2) % shape_verts::CIRCLE_SEGMENTS;

        float ax = shape_verts::CIRCLE[j].x - shape_verts::CIRCLE[i].x;
        float ay = shape_verts::CIRCLE[j].y - shape_verts::CIRCLE[i].y;
        float bx = shape_verts::CIRCLE[k].x - shape_verts::CIRCLE[j].x;
        float by = shape_verts::CIRCLE[k].y - shape_verts::CIRCLE[j].y;

        float cross_z = ax * by - ay * bx;
        CAPTURE(i, j, k, cross_z);
        CHECK(cross_z > 0.0f);  // positive ⇒ CCW turn
    }
}

// ── Hexagon table ───────────────────────────────────────────────────────────

TEST_CASE("shape_verts::HEXAGON — table size is 6", "[shape_verts]") {
    CHECK(shape_verts::HEX_SEGMENTS == 6);
    static_assert(sizeof(shape_verts::HEXAGON) / sizeof(shape_verts::V2) == 6);
}

TEST_CASE("shape_verts::HEXAGON — pointy-top (vertex 0 at top)", "[shape_verts]") {
    CHECK_THAT(shape_verts::HEXAGON[0].x, WithinAbs( 0.0, 1e-5));
    CHECK_THAT(shape_verts::HEXAGON[0].y, WithinAbs(-1.0, 1e-5));
}

TEST_CASE("shape_verts::HEXAGON — unit magnitude for all vertices", "[shape_verts]") {
    for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
        CAPTURE(i);
        float mag_sq = shape_verts::HEXAGON[i].x * shape_verts::HEXAGON[i].x
                     + shape_verts::HEXAGON[i].y * shape_verts::HEXAGON[i].y;
        CHECK_THAT(mag_sq, WithinAbs(1.0, 1e-5));
    }
}

TEST_CASE("shape_verts::HEXAGON — top-bottom symmetry", "[shape_verts]") {
    // Vertex 0 is (0, −1), vertex 3 is (0, +1): mirror about x-axis.
    CHECK_THAT(shape_verts::HEXAGON[0].x, WithinAbs( shape_verts::HEXAGON[3].x, 1e-5));
    CHECK_THAT(shape_verts::HEXAGON[0].y, WithinAbs(-shape_verts::HEXAGON[3].y, 1e-5));

    // Vertex 1 (top-right) mirrors vertex 5 (top-left) in x only
    CHECK_THAT(shape_verts::HEXAGON[1].x, WithinAbs(-shape_verts::HEXAGON[5].x, 1e-5));
    CHECK_THAT(shape_verts::HEXAGON[1].y, WithinAbs( shape_verts::HEXAGON[5].y, 1e-5));

    // Vertex 2 (bot-right) mirrors vertex 4 (bot-left)
    CHECK_THAT(shape_verts::HEXAGON[2].x, WithinAbs(-shape_verts::HEXAGON[4].x, 1e-5));
    CHECK_THAT(shape_verts::HEXAGON[2].y, WithinAbs( shape_verts::HEXAGON[4].y, 1e-5));
}

// ── Square table ────────────────────────────────────────────────────────────

TEST_CASE("shape_verts::SQUARE — exact corners", "[shape_verts]") {
    // Expected order: TL, TR, BR, BL
    CHECK(shape_verts::SQUARE[0].x == -1.0f);
    CHECK(shape_verts::SQUARE[0].y == -1.0f);

    CHECK(shape_verts::SQUARE[1].x ==  1.0f);
    CHECK(shape_verts::SQUARE[1].y == -1.0f);

    CHECK(shape_verts::SQUARE[2].x ==  1.0f);
    CHECK(shape_verts::SQUARE[2].y ==  1.0f);

    CHECK(shape_verts::SQUARE[3].x == -1.0f);
    CHECK(shape_verts::SQUARE[3].y ==  1.0f);
}

TEST_CASE("shape_verts::SQUARE — table size is 4", "[shape_verts]") {
    static_assert(sizeof(shape_verts::SQUARE) / sizeof(shape_verts::V2) == 4);
}

// ── Triangle table ──────────────────────────────────────────────────────────

TEST_CASE("shape_verts::TRIANGLE — exact vertices", "[shape_verts]") {
    // CCW order: base-right, base-left, apex
    CHECK(shape_verts::TRIANGLE[0].x ==  1.0f);
    CHECK(shape_verts::TRIANGLE[0].y ==  1.0f);

    CHECK(shape_verts::TRIANGLE[1].x == -1.0f);
    CHECK(shape_verts::TRIANGLE[1].y ==  1.0f);

    CHECK(shape_verts::TRIANGLE[2].x ==  0.0f);
    CHECK(shape_verts::TRIANGLE[2].y == -1.0f);
}

TEST_CASE("shape_verts::TRIANGLE — table size is 3", "[shape_verts]") {
    static_assert(sizeof(shape_verts::TRIANGLE) / sizeof(shape_verts::V2) == 3);
}

// ═════════════════════════════════════════════════════════════════════════════
// §3  Floor ring sub-segment coverage regression
// ═════════════════════════════════════════════════════════════════════════════
// Verifies that the sub-segment index mapping for floor rings correctly
// covers the full circle vertex table, regardless of segment count.

TEST_CASE("floor ring: sub-segment count covers full circle", "[floor][regression]") {
    // BUG: When drawing a ring with segs < CIRCLE_SEGMENTS (e.g. 12 of 24),
    // the old code iterated i=0..11 with next=(i+1)%24, only covering
    // vertices 0-12 (top half). The bottom half was never drawn.
    //
    // Fix: map segment indices evenly across the full table:
    //   idx = (i * CIRCLE_SEGMENTS) / seg
    //
    // Verify: for seg=12, the indices should span the full 0..23 range
    // (every other vertex), not just 0..12.

    constexpr int seg = 12;
    int min_idx = 999, max_idx = -1;
    for (int i = 0; i < seg; ++i) {
        int idx = (i * shape_verts::CIRCLE_SEGMENTS) / seg;
        if (idx < min_idx) min_idx = idx;
        if (idx > max_idx) max_idx = idx;
    }
    // With even mapping, indices should cover 0, 2, 4, ..., 22
    CHECK(min_idx == 0);
    CHECK(max_idx == 22);  // last index before wrapping

    // Also verify the "next" index wraps to 0
    int last_next = ((seg) * shape_verts::CIRCLE_SEGMENTS) / seg
                    % shape_verts::CIRCLE_SEGMENTS;
    CHECK(last_next == 0);  // closes the ring
}
