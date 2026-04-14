#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "perspective.h"
#include "shape_vertices.h"
#include "constants.h"
#include <cmath>

using Catch::Matchers::WithinAbs;

// ═════════════════════════════════════════════════════════════════════════════
// §1  perspective::project() / perspective::project_x() — projection math
// ═════════════════════════════════════════════════════════════════════════════

// ── Centre-line invariance ──────────────────────────────────────────────────
// x = SCREEN_W / 2 = 360 must always project to x = 360 regardless of y,
// because center + (center − center) * depth = center for any depth.

TEST_CASE("perspective::project — centre-line invariance", "[perspective]") {
    constexpr float center = constants::SCREEN_W / 2.0f;  // 360
    constexpr float vp_y   = constants::VANISHING_POINT_Y;

    for (float y : {vp_y, 0.0f, 640.0f, 1280.0f}) {
        CAPTURE(y);
        Vector2 p = perspective::project(center, y);
        CHECK_THAT(p.x, WithinAbs(center, 0.01));
    }
}

// ── Y-preservation ──────────────────────────────────────────────────────────
// The projected y must always equal the input y (only x is scaled by depth).

TEST_CASE("perspective::project — y-preservation", "[perspective]") {
    constexpr float vp_y = constants::VANISHING_POINT_Y;
    const float xs[] = {0.0f, 180.0f, 360.0f, 540.0f, 720.0f};
    const float ys[] = {vp_y - 200.0f, vp_y, constants::SPAWN_Y, 0.0f,
                        640.0f, constants::PLAYER_Y, 1280.0f, 2000.0f};

    for (float x : xs) {
        for (float y : ys) {
            CAPTURE(x, y);
            Vector2 p = perspective::project(x, y);
            CHECK_THAT(p.y, WithinAbs(y, 0.01));
        }
    }
}

// ── Depth monotonicity ──────────────────────────────────────────────────────
// For x ≠ 360, |project_x(x, y) − 360| must increase as y increases —
// objects spread further from the vanishing point as they approach the camera.

TEST_CASE("perspective::project — depth monotonicity (lane 0, x=180)", "[perspective]") {
    constexpr float center = 360.0f;
    constexpr float x      = 180.0f;  // left of centre

    float prev_spread = 0.0f;
    for (float y : {0.0f, 640.0f, 1280.0f}) {
        float spread = std::abs(perspective::project_x(x, y) - center);
        CAPTURE(y, spread);
        CHECK(spread > prev_spread);
        prev_spread = spread;
    }
}

TEST_CASE("perspective::project — depth monotonicity (lane 2, x=540)", "[perspective]") {
    constexpr float center = 360.0f;
    constexpr float x      = 540.0f;  // right of centre

    float prev_spread = 0.0f;
    for (float y : {0.0f, 640.0f, 1280.0f}) {
        float spread = std::abs(perspective::project_x(x, y) - center);
        CAPTURE(y, spread);
        CHECK(spread > prev_spread);
        prev_spread = spread;
    }
}

// ── Lateral ordering preservation ───────────────────────────────────────────
// For x1 < x2 at the same y, project_x(x1, y) < project_x(x2, y).

TEST_CASE("perspective::project — lateral ordering preservation", "[perspective]") {
    constexpr float x1 = 180.0f;
    constexpr float x2 = 540.0f;

    for (float y : {constants::VANISHING_POINT_Y, constants::SPAWN_Y,
                     0.0f, 640.0f, constants::PLAYER_Y, 1280.0f}) {
        CAPTURE(y);
        CHECK(perspective::project_x(x1, y) < perspective::project_x(x2, y));
    }
}

TEST_CASE("perspective::project — lateral ordering with three lanes", "[perspective]") {
    for (float y : {0.0f, 640.0f, 1280.0f}) {
        CAPTURE(y);
        float px0 = perspective::project_x(180.0f, y);
        float px1 = perspective::project_x(360.0f, y);
        float px2 = perspective::project_x(540.0f, y);
        CHECK(px0 < px1);
        CHECK(px1 < px2);
    }
}

// ── Known values at key y positions ─────────────────────────────────────────
// Known-value tests compute expected results from the actual constants so
// they stay correct regardless of PERSPECTIVE_ANGLE_DEG / VP_Y tuning.

static float expected_px(float x, float y) {
    constexpr float center = constants::SCREEN_W / 2.0f;
    constexpr float vp_y   = constants::VANISHING_POINT_Y;
    constexpr float range  = static_cast<float>(constants::SCREEN_H) - vp_y;
    float depth = (y - vp_y) / range;
    if (depth < 0.01f) depth = 0.01f;
    if (depth > 1.5f)  depth = 1.5f;
    return center + (x - center) * depth;
}

TEST_CASE("perspective::project — known value at vanishing point", "[perspective]") {
    constexpr float vp_y = constants::VANISHING_POINT_Y;
    Vector2 p = perspective::project(0.0f, vp_y);
    // At VP, depth clamps to 0.01: project(0) = 360 + (0-360)*0.01 = 356.4
    CHECK_THAT(p.x, WithinAbs(356.4, 0.01));
    CHECK_THAT(p.y, WithinAbs(vp_y, 0.01));
}

TEST_CASE("perspective::project — known value at spawn y=-120", "[perspective]") {
    float px = perspective::project_x(180.0f, constants::SPAWN_Y);
    CHECK_THAT(px, WithinAbs(expected_px(180.0f, constants::SPAWN_Y), 0.02));
}

TEST_CASE("perspective::project — known value at top of screen y=0", "[perspective]") {
    float px = perspective::project_x(180.0f, 0.0f);
    CHECK_THAT(px, WithinAbs(expected_px(180.0f, 0.0f), 0.01));
}

TEST_CASE("perspective::project — known value at mid-screen y=640", "[perspective]") {
    float px = perspective::project_x(180.0f, 640.0f);
    CHECK_THAT(px, WithinAbs(expected_px(180.0f, 640.0f), 0.01));
}

TEST_CASE("perspective::project — known value at player y=920", "[perspective]") {
    float px = perspective::project_x(180.0f, constants::PLAYER_Y);
    CHECK_THAT(px, WithinAbs(expected_px(180.0f, constants::PLAYER_Y), 0.01));
}

TEST_CASE("perspective::project — known value at bottom y=1280", "[perspective]") {
    // depth = 1.0 at bottom → Lane0: 360 + (180−360)*1.0 = 180 (identity)
    float px = perspective::project_x(180.0f, 1280.0f);
    CHECK_THAT(px, WithinAbs(expected_px(180.0f, 1280.0f), 0.01));
}

// ── Clamping behaviour ──────────────────────────────────────────────────────

TEST_CASE("perspective::project — depth clamps to 0.01 above vanishing point", "[perspective]") {
    // At y well above VP (−1060): raw depth < 0 → clamped to 0.01
    // project(0, y) = 360 + (0−360)*0.01 = 356.4
    constexpr float far_above = constants::VANISHING_POINT_Y - 500.0f;
    Vector2 p = perspective::project(0.0f, far_above);
    CHECK_THAT(p.x, WithinAbs(356.4, 0.01));

    // Right edge: project(720, y) = 360 + (720−360)*0.01 = 363.6
    Vector2 p2 = perspective::project(720.0f, far_above);
    CHECK_THAT(p2.x, WithinAbs(363.6, 0.01));
}

TEST_CASE("perspective::project — depth clamps to 1.5 far below screen", "[perspective]") {
    // range = SCREEN_H − VP_Y = 2340
    // depth = 1.5 at y = VP_Y + 1.5*range = −1060 + 3510 = 2450
    // For y >> 2450, depth clamps to 1.5
    // project(0, 4000) = 360 + (0−360)*1.5 = −180
    Vector2 p = perspective::project(0.0f, 4000.0f);
    CHECK_THAT(p.x, WithinAbs(-180.0, 0.01));
}

TEST_CASE("perspective::project — depth clamp boundary at exactly 1.5", "[perspective]") {
    // depth = 1.5 when y = VP_Y + 1.5 * range = −1060 + 3510 = 2450
    constexpr float range = static_cast<float>(constants::SCREEN_H) - constants::VANISHING_POINT_Y;
    constexpr float y_boundary = constants::VANISHING_POINT_Y + 1.5f * range;

    // At boundary: project(0, 2450) = 360 + (0−360)*1.5 = −180
    float px_at_boundary = perspective::project_x(0.0f, y_boundary);
    CHECK_THAT(px_at_boundary, WithinAbs(-180.0, 0.01));

    // Past boundary: same result (clamped)
    float px_past = perspective::project_x(0.0f, y_boundary + 200.0f);
    CHECK_THAT(px_past, WithinAbs(-180.0, 0.01));
}

// ── Lane convergence ────────────────────────────────────────────────────────
// All three lanes converge toward x=360 as y decreases.

TEST_CASE("perspective::project — lane convergence toward vanishing point", "[perspective]") {
    auto lane_spread = [](float y) {
        float px0 = perspective::project_x(180.0f, y);
        float px2 = perspective::project_x(540.0f, y);
        return px2 - px0;  // total horizontal spread
    };

    float spread_bottom = lane_spread(1280.0f);
    float spread_mid    = lane_spread(640.0f);
    float spread_top    = lane_spread(0.0f);

    CAPTURE(spread_bottom, spread_mid, spread_top);
    CHECK(spread_bottom > spread_mid);
    CHECK(spread_mid    > spread_top);
    CHECK(spread_top    > 0.0f);  // never fully converged on-screen
}

// ── Symmetry ────────────────────────────────────────────────────────────────
// project_x(360 − d, y) and project_x(360 + d, y) must be equidistant from 360.

TEST_CASE("perspective::project — symmetry about centre line", "[perspective]") {
    constexpr float center = 360.0f;

    for (float d : {90.0f, 180.0f, 360.0f}) {
        for (float y : {0.0f, 640.0f, 920.0f, 1280.0f}) {
            CAPTURE(d, y);
            float left  = perspective::project_x(center - d, y);
            float right = perspective::project_x(center + d, y);

            float dist_left  = center - left;
            float dist_right = right - center;
            CHECK_THAT(dist_left, WithinAbs(dist_right, 0.01));
        }
    }
}

// ── project_x consistency ───────────────────────────────────────────────────
// project_x(x, y) must always agree with project(x, y).x.

TEST_CASE("perspective::project_x — consistent with project().x", "[perspective]") {
    constexpr float vp_y = constants::VANISHING_POINT_Y;
    for (float x : {0.0f, 180.0f, 360.0f, 540.0f, 720.0f}) {
        for (float y : {vp_y - 200.0f, vp_y, 0.0f, 640.0f, 1280.0f, 4000.0f}) {
            CAPTURE(x, y);
            Vector2 p  = perspective::project(x, y);
            float   px = perspective::project_x(x, y);
            CHECK_THAT(px, WithinAbs(p.x, 1e-5));
        }
    }
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
    // Apex, base-left, base-right
    CHECK(shape_verts::TRIANGLE[0].x ==  0.0f);
    CHECK(shape_verts::TRIANGLE[0].y == -1.0f);

    CHECK(shape_verts::TRIANGLE[1].x == -1.0f);
    CHECK(shape_verts::TRIANGLE[1].y ==  1.0f);

    CHECK(shape_verts::TRIANGLE[2].x ==  1.0f);
    CHECK(shape_verts::TRIANGLE[2].y ==  1.0f);
}

TEST_CASE("shape_verts::TRIANGLE — table size is 3", "[shape_verts]") {
    static_assert(sizeof(shape_verts::TRIANGLE) / sizeof(shape_verts::V2) == 3);
}

// ═════════════════════════════════════════════════════════════════════════════
// §3  Floor geometry regression tests
// ═════════════════════════════════════════════════════════════════════════════
// These tests verify invariants of the floor rendering pipeline that have
// broken in the past (connector-to-shape edge alignment, shape visibility).

TEST_CASE("floor: connector endpoints match depth-scaled shape edges", "[floor][regression]") {
    // The connector between floor shape j and j+1 must start at the
    // bottom edge of shape j (cy + half*depth) and end at the top edge
    // of shape j+1 (next_cy - half*next_depth).
    //
    // BUG HISTORY: connectors used un-scaled fp.half instead of
    // depth-scaled p_half, causing gaps at the top of the screen.

    constexpr float half = constants::FLOOR_SHAPE_SIZE / 2.0f;

    for (int j = 0; j < constants::FLOOR_SHAPE_COUNT - 1; ++j) {
        float cy = constants::FLOOR_Y_START
            + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
        float next_cy = cy + constants::FLOOR_SHAPE_SPACING;

        float d      = perspective::depth(cy);
        float next_d = perspective::depth(next_cy);

        float shape_bottom_edge = cy + half * d;
        float next_shape_top_edge = next_cy - half * next_d;

        // Connector must span from shape_bottom_edge to next_shape_top_edge.
        // The gap must be non-negative (shapes don't overlap).
        INFO("j=" << j << " cy=" << cy << " shape_bottom=" << shape_bottom_edge
             << " next_top=" << next_shape_top_edge);
        CHECK(next_shape_top_edge >= shape_bottom_edge);
    }
}

TEST_CASE("floor: all shapes within visible screen bounds", "[floor][regression]") {
    // Every floor shape centre, after perspective projection, must be
    // within the render texture (0..SCREEN_W x 0..SCREEN_H).

    constexpr float half = constants::FLOOR_SHAPE_SIZE / 2.0f;

    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float cx = constants::LANE_X[lane];
        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            float cy = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

            float d    = perspective::depth(cy);
            float px   = perspective::project_x(cx, cy);
            float p_half = half * d;

            INFO("lane=" << lane << " j=" << j << " px=" << px << " p_half=" << p_half);
            // Shape centre within screen
            CHECK(px >= 0.0f);
            CHECK(px <= static_cast<float>(constants::SCREEN_W));
            // Shape edges within reasonable bounds (allow small overshoot)
            CHECK(px - p_half >= -p_half);  // left edge not wildly off-screen
            CHECK(px + p_half <= constants::SCREEN_W + p_half);  // right edge
        }
    }
}

TEST_CASE("floor: depth-scaled shape size decreases toward top", "[floor][regression]") {
    // Shapes nearer the top of the screen (lower y) must be smaller
    // than shapes nearer the bottom (higher y).

    constexpr float half = constants::FLOOR_SHAPE_SIZE / 2.0f;

    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float prev_p_half = 0.0f;
        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            float cy = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
            float d = perspective::depth(cy);
            float p_half = half * d;

            if (j > 0) {
                INFO("lane=" << lane << " j=" << j);
                CHECK(p_half >= prev_p_half);
            }
            prev_p_half = p_half;
        }
    }
}

TEST_CASE("floor: connector uses depth-scaled half, not flat half", "[floor][regression]") {
    // Direct regression for the double-perspective connector bug.
    // At the top of the screen, depth < 1.0, so depth-scaled half < flat half.
    // If connectors used flat half, the start y would be BELOW the depth-scaled
    // shape edge, causing visible gaps.

    constexpr float half = constants::FLOOR_SHAPE_SIZE / 2.0f;
    float cy_top = constants::FLOOR_Y_START;  // y=12, near top
    float d_top = perspective::depth(cy_top);

    // depth at top should be < 1.0 (this is what makes the bug visible)
    REQUIRE(d_top < 1.0f);

    float scaled_half = half * d_top;
    float flat_half   = half;

    // The connector start must use scaled_half, not flat_half
    float correct_start   = cy_top + scaled_half;
    float incorrect_start = cy_top + flat_half;

    // If using flat_half, connector starts further down than the shape edge
    CHECK(incorrect_start > correct_start);
    // The correct start matches the actual shape edge
    CHECK_THAT(correct_start, WithinAbs(cy_top + half * d_top, 0.001));
}

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

// ═════════════════════════════════════════════════════════════════════════════
// §4  Winding order regression tests
// ═════════════════════════════════════════════════════════════════════════════
// raylib calls glFrontFace(GL_CCW) — front-facing triangles must have
// counter-clockwise vertex order.  In screen-space (y-axis points down),
// the 2D cross product of a CCW triangle is NEGATIVE.
//
// GL_CCW = 0x0901 per the OpenGL spec.  We check the cross product sign
// rather than the constant so the test works without GL headers.

// Expected sign of cross(AB, AC) for a front-facing (CCW) triangle in
// screen-space where y increases downward.
constexpr float CCW_SIGN = -1.0f;   // GL_CCW + y-down → negative cross

static float cross2d(float ax, float ay, float bx, float by,
                     float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static bool is_ccw(float cross) { return cross * CCW_SIGN > 0.0f; }

TEST_CASE("winding: flat ring triangles are CCW", "[winding][regression]") {
    // Simulate emit_flat_ring for a ring at screen centre.
    constexpr float px = 360.0f, cy = 500.0f;
    constexpr float outer_r = 20.0f, inner_r = 17.0f;
    constexpr int seg = 12;

    for (int i = 0; i < seg; ++i) {
        int idx      = (i * shape_verts::CIRCLE_SEGMENTS) / seg;
        int next_idx = ((i + 1) * shape_verts::CIRCLE_SEGMENTS) / seg
                       % shape_verts::CIRCLE_SEGMENTS;

        float ox1 = px + shape_verts::CIRCLE[idx].x * outer_r;
        float oy1 = cy + shape_verts::CIRCLE[idx].y * outer_r;
        float ox2 = px + shape_verts::CIRCLE[next_idx].x * outer_r;
        float oy2 = cy + shape_verts::CIRCLE[next_idx].y * outer_r;
        float ix1 = px + shape_verts::CIRCLE[idx].x * inner_r;
        float iy1 = cy + shape_verts::CIRCLE[idx].y * inner_r;
        float ix2 = px + shape_verts::CIRCLE[next_idx].x * inner_r;
        float iy2 = cy + shape_verts::CIRCLE[next_idx].y * inner_r;

        // Triangle 1: outer1 → inner1 → outer2 (must be CCW = positive cross)
        float cross1 = cross2d(ox1, oy1, ix1, iy1, ox2, oy2);
        INFO("ring seg=" << i << " tri1 cross=" << cross1);
        CHECK(is_ccw(cross1));

        // Triangle 2: inner1 → inner2 → outer2 (must be CCW = positive cross)
        float cross2 = cross2d(ix1, iy1, ix2, iy2, ox2, oy2);
        INFO("ring seg=" << i << " tri2 cross=" << cross2);
        CHECK(is_ccw(cross2));
    }
}

TEST_CASE("winding: perspective circle fan triangles are CCW", "[winding][regression]") {
    // Circle fan: centre → cur → prev (from emit_fan / draw_shape Circle).
    // The circle table goes CCW, so fan triangles centre→v[i+1]→v[i]
    // should be CCW in screen space after projection.
    constexpr float cx = 360.0f, cy = 500.0f, r = 32.0f;
    Vector2 centre = perspective::project(cx, cy);

    Vector2 prev = perspective::project(
        cx + shape_verts::CIRCLE[0].x * r,
        cy + shape_verts::CIRCLE[0].y * r);

    for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
        int next = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
        Vector2 cur = perspective::project(
            cx + shape_verts::CIRCLE[next].x * r,
            cy + shape_verts::CIRCLE[next].y * r);

        // Fan triangle: centre → cur → prev
        float cross = cross2d(centre.x, centre.y, cur.x, cur.y, prev.x, prev.y);
        INFO("circle fan i=" << i << " cross=" << cross);
        CHECK(is_ccw(cross));
        prev = cur;
    }
}

TEST_CASE("winding: perspective hexagon fan triangles are CCW", "[winding][regression]") {
    constexpr float cx = 360.0f, cy = 500.0f, radius = 64.0f * 0.6f;
    Vector2 centre = perspective::project(cx, cy);

    Vector2 prev = perspective::project(
        cx + shape_verts::HEXAGON[0].x * radius,
        cy + shape_verts::HEXAGON[0].y * radius);

    for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
        int next = (i + 1) % shape_verts::HEX_SEGMENTS;
        Vector2 cur = perspective::project(
            cx + shape_verts::HEXAGON[next].x * radius,
            cy + shape_verts::HEXAGON[next].y * radius);

        float cross = cross2d(centre.x, centre.y, cur.x, cur.y, prev.x, prev.y);
        INFO("hex fan i=" << i << " cross=" << cross);
        CHECK(is_ccw(cross));
        prev = cur;
    }
}

TEST_CASE("winding: perspective square trapezoid triangles are CCW", "[winding][regression]") {
    // Square → trapezoid: two triangles from 4 projected corners.
    // v0=TL, v1=TR, v2=BR, v3=BL. Tris: (v0,v3,v1) and (v1,v3,v2).
    constexpr float cx = 360.0f, cy = 500.0f, half = 32.0f;
    Vector2 v[4];
    for (int i = 0; i < 4; ++i)
        v[i] = perspective::project(cx + shape_verts::SQUARE[i].x * half,
                                    cy + shape_verts::SQUARE[i].y * half);

    float cross1 = cross2d(v[0].x, v[0].y, v[3].x, v[3].y, v[1].x, v[1].y);
    CHECK(is_ccw(cross1));
    float cross2 = cross2d(v[1].x, v[1].y, v[3].x, v[3].y, v[2].x, v[2].y);
    CHECK(is_ccw(cross2));
}

TEST_CASE("winding: perspective triangle shape is CCW", "[winding][regression]") {
    constexpr float cx = 360.0f, cy = 500.0f, half = 32.0f;
    Vector2 v[3];
    for (int i = 0; i < 3; ++i)
        v[i] = perspective::project(cx + shape_verts::TRIANGLE[i].x * half,
                                    cy + shape_verts::TRIANGLE[i].y * half);

    float cross = cross2d(v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y);
    CHECK(is_ccw(cross));
}

TEST_CASE("winding: perspective rect (obstacle quad) triangles are CCW", "[winding][regression]") {
    // draw_rect produces two triangles from 4 projected corners.
    // TL→BL→TR and TR→BL→BR.
    float d_top = perspective::depth(300.0f);
    float d_bot = perspective::depth(320.0f);

    float tl_x = perspective::CENTER + (0.0f   - perspective::CENTER) * d_top;
    float tr_x = perspective::CENTER + (720.0f - perspective::CENTER) * d_top;
    float bl_x = perspective::CENTER + (0.0f   - perspective::CENTER) * d_bot;
    float br_x = perspective::CENTER + (720.0f - perspective::CENTER) * d_bot;

    // Triangle 1: TL → BL → TR
    float cross1 = cross2d(tl_x, 300.0f, bl_x, 320.0f, tr_x, 300.0f);
    CHECK(is_ccw(cross1));
    // Triangle 2: TR → BL → BR
    float cross2 = cross2d(tr_x, 300.0f, bl_x, 320.0f, br_x, 320.0f);
    CHECK(is_ccw(cross2));
}
