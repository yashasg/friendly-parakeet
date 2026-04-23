#include "camera_system.h"
#include "../components/shape_vertices.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/rendering.h"
#include "../components/particle.h"
#include "../components/lifetime.h"
#include "../constants.h"
#include <raylib.h>
#include <rlgl.h>

// ═══════════════════════════════════════════════════════════════════════════════
// Camera3D world-space rendering
// ═══════════════════════════════════════════════════════════════════════════════
//
// Coordinate mapping:  game Position{x, y} → 3D {x, 0, y}
//   • x stays the same (0–720)
//   • 3D y = 0 is the ground plane (>0 for jumping player)
//   • 3D z = game's y coordinate (scroll direction)
//
// All geometry is emitted as 3D primitives using rlVertex3f.
// Camera3D's perspective projection matrix handles foreshortening, lane
// convergence, and depth scaling — no manual depth()/project() needed.
// ═══════════════════════════════════════════════════════════════════════════════

namespace camera {

// Scale game-pixel coordinates to world coordinates for Camera3D rendering.
static constexpr float S = camera::WORLD_SCALE;
static inline void rlVertex3fScaled(float x, float y, float z) {
    rlVertex3f(x / S, y / S, z / S);
}

// ── 3D shape vertex emitter (call inside an RL_TRIANGLES block) ─────────────
// Emits vertices for actual 3D primitives: gem, cube, pyramid, hex-prism.
// Top faces use the primary colour; bottom and side faces use a darker shade.
static void emit_3d_shape(Shape shape, float cx, float y_3d, float cz,
                           float size,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Darker shade for bottom / side faces
    uint8_t sr = static_cast<uint8_t>(r * 0.7f);
    uint8_t sg = static_cast<uint8_t>(g * 0.7f);
    uint8_t sb = static_cast<uint8_t>(b * 0.7f);

    switch (shape) {

    // ── Circle → Faceted gem (tapered decagon prism) ─────────────────────
    case Shape::Circle: {
        constexpr int N = 10;
        float radius    = size / 2.0f;
        float top_r     = radius * 0.6f;
        float height    = size * 0.4f;
        float top_y     = y_3d + height;

        float bx[N], bz_[N];   // bottom ring
        float tx[N], tz_[N];   // top ring (smaller)
        for (int i = 0; i < N; ++i) {
            int idx = (i * shape_verts::CIRCLE_SEGMENTS) / N;
            bx[i]  = cx + shape_verts::CIRCLE[idx].x * radius;
            bz_[i] = cz + shape_verts::CIRCLE[idx].y * radius;
            tx[i]  = cx + shape_verts::CIRCLE[idx].x * top_r;
            tz_[i] = cz + shape_verts::CIRCLE[idx].y * top_r;
        }

        // Bottom face (fan, darker)
        rlColor4ub(sr, sg, sb, a);
        for (int i = 0; i < N; ++i) {
            int n = (i + 1) % N;
            rlVertex3fScaled(cx,     y_3d, cz);
            rlVertex3fScaled(bx[n],  y_3d, bz_[n]);
            rlVertex3fScaled(bx[i],  y_3d, bz_[i]);
        }

        // Top face (fan, bright)
        rlColor4ub(r, g, b, a);
        for (int i = 0; i < N; ++i) {
            int n = (i + 1) % N;
            rlVertex3fScaled(cx,     top_y, cz);
            rlVertex3fScaled(tx[i],  top_y, tz_[i]);
            rlVertex3fScaled(tx[n],  top_y, tz_[n]);
        }

        // Side faces (quads as 2 tris each, darker)
        rlColor4ub(sr, sg, sb, a);
        for (int i = 0; i < N; ++i) {
            int n = (i + 1) % N;
            rlVertex3fScaled(bx[i],  y_3d,  bz_[i]);
            rlVertex3fScaled(bx[n],  y_3d,  bz_[n]);
            rlVertex3fScaled(tx[i],  top_y, tz_[i]);

            rlVertex3fScaled(bx[n],  y_3d,  bz_[n]);
            rlVertex3fScaled(tx[n],  top_y, tz_[n]);
            rlVertex3fScaled(tx[i],  top_y, tz_[i]);
        }
        break;
    }

    // ── Square → Cube ────────────────────────────────────────────────────
    case Shape::Square: {
        float half   = size / 2.0f;
        float height = size * 0.4f;
        float bot_y  = y_3d;
        float top_y  = y_3d + height;

        // SQUARE: 0=TL(-1,-1)  1=TR(1,-1)  2=BR(1,1)  3=BL(-1,1)
        float vx[4], vz[4];
        for (int i = 0; i < 4; ++i) {
            vx[i] = cx + shape_verts::SQUARE[i].x * half;
            vz[i] = cz + shape_verts::SQUARE[i].y * half;
        }

        // Top face (bright)
        rlColor4ub(r, g, b, a);
        rlVertex3fScaled(vx[0], top_y, vz[0]);
        rlVertex3fScaled(vx[1], top_y, vz[1]);
        rlVertex3fScaled(vx[3], top_y, vz[3]);
        rlVertex3fScaled(vx[1], top_y, vz[1]);
        rlVertex3fScaled(vx[2], top_y, vz[2]);
        rlVertex3fScaled(vx[3], top_y, vz[3]);

        // Bottom face (darker)
        rlColor4ub(sr, sg, sb, a);
        rlVertex3fScaled(vx[0], bot_y, vz[0]);
        rlVertex3fScaled(vx[3], bot_y, vz[3]);
        rlVertex3fScaled(vx[1], bot_y, vz[1]);
        rlVertex3fScaled(vx[1], bot_y, vz[1]);
        rlVertex3fScaled(vx[3], bot_y, vz[3]);
        rlVertex3fScaled(vx[2], bot_y, vz[2]);

        // Four side faces (darker) — front, back, left, right
        // Front: 3→2
        rlVertex3fScaled(vx[3], bot_y, vz[3]);
        rlVertex3fScaled(vx[2], bot_y, vz[2]);
        rlVertex3fScaled(vx[3], top_y, vz[3]);
        rlVertex3fScaled(vx[2], bot_y, vz[2]);
        rlVertex3fScaled(vx[2], top_y, vz[2]);
        rlVertex3fScaled(vx[3], top_y, vz[3]);
        // Back: 0→1
        rlVertex3fScaled(vx[0], bot_y, vz[0]);
        rlVertex3fScaled(vx[0], top_y, vz[0]);
        rlVertex3fScaled(vx[1], bot_y, vz[1]);
        rlVertex3fScaled(vx[1], bot_y, vz[1]);
        rlVertex3fScaled(vx[0], top_y, vz[0]);
        rlVertex3fScaled(vx[1], top_y, vz[1]);
        // Left: 0→3
        rlVertex3fScaled(vx[0], bot_y, vz[0]);
        rlVertex3fScaled(vx[3], bot_y, vz[3]);
        rlVertex3fScaled(vx[0], top_y, vz[0]);
        rlVertex3fScaled(vx[3], bot_y, vz[3]);
        rlVertex3fScaled(vx[3], top_y, vz[3]);
        rlVertex3fScaled(vx[0], top_y, vz[0]);
        // Right: 1→2
        rlVertex3fScaled(vx[1], bot_y, vz[1]);
        rlVertex3fScaled(vx[1], top_y, vz[1]);
        rlVertex3fScaled(vx[2], bot_y, vz[2]);
        rlVertex3fScaled(vx[2], bot_y, vz[2]);
        rlVertex3fScaled(vx[1], top_y, vz[1]);
        rlVertex3fScaled(vx[2], top_y, vz[2]);
        break;
    }

    // ── Triangle → Pyramid (tetrahedron) ─────────────────────────────────
    case Shape::Triangle: {
        float half   = size / 2.0f;
        float apex_y = y_3d + size * 0.5f;

        float vx[3], vz[3];
        for (int i = 0; i < 3; ++i) {
            vx[i] = cx + shape_verts::TRIANGLE[i].x * half;
            vz[i] = cz + shape_verts::TRIANGLE[i].y * half;
        }

        // Base face (darker)
        rlColor4ub(sr, sg, sb, a);
        rlVertex3fScaled(vx[0], y_3d, vz[0]);
        rlVertex3fScaled(vx[2], y_3d, vz[2]);
        rlVertex3fScaled(vx[1], y_3d, vz[1]);

        // Three side faces to apex (bright)
        rlColor4ub(r, g, b, a);
        for (int i = 0; i < 3; ++i) {
            int n = (i + 1) % 3;
            rlVertex3fScaled(vx[i], y_3d, vz[i]);
            rlVertex3fScaled(vx[n], y_3d, vz[n]);
            rlVertex3fScaled(cx,    apex_y, cz);
        }
        break;
    }

    // ── Hexagon → Hexagonal prism ────────────────────────────────────────
    case Shape::Hexagon: {
        constexpr int N = 6;
        float radius = size * 0.6f;
        float height = size * 0.3f;
        float top_y  = y_3d + height;

        float hx[N], hz[N];
        for (int i = 0; i < N; ++i) {
            hx[i] = cx + shape_verts::HEXAGON[i].x * radius;
            hz[i] = cz + shape_verts::HEXAGON[i].y * radius;
        }

        // Bottom face (fan, darker)
        rlColor4ub(sr, sg, sb, a);
        for (int i = 0; i < N; ++i) {
            int n = (i + 1) % N;
            rlVertex3fScaled(cx,    y_3d, cz);
            rlVertex3fScaled(hx[n], y_3d, hz[n]);
            rlVertex3fScaled(hx[i], y_3d, hz[i]);
        }

        // Top face (fan, bright)
        rlColor4ub(r, g, b, a);
        for (int i = 0; i < N; ++i) {
            int n = (i + 1) % N;
            rlVertex3fScaled(cx,    top_y, cz);
            rlVertex3fScaled(hx[i], top_y, hz[i]);
            rlVertex3fScaled(hx[n], top_y, hz[n]);
        }

        // Side faces (quads as 2 tris, darker)
        rlColor4ub(sr, sg, sb, a);
        for (int i = 0; i < N; ++i) {
            int n = (i + 1) % N;
            rlVertex3fScaled(hx[i], y_3d,  hz[i]);
            rlVertex3fScaled(hx[n], y_3d,  hz[n]);
            rlVertex3fScaled(hx[i], top_y, hz[i]);

            rlVertex3fScaled(hx[n], y_3d,  hz[n]);
            rlVertex3fScaled(hx[n], top_y, hz[n]);
            rlVertex3fScaled(hx[i], top_y, hz[i]);
        }
        break;
    }

    } // switch
}

// ── Standalone shape draw (wraps its own RL_TRIANGLES batch) ─────────────────
void draw_shape(Shape shape, float cx, float y_3d, float cz, float size, Color c) {
    rlBegin(RL_TRIANGLES);
    emit_3d_shape(shape, cx, y_3d, cz, size, c.r, c.g, c.b, c.a);
    rlEnd();
}

// ── Pass 3: World rects (obstacles + particles) ─────────────────────────────
void flush_world_rects(entt::registry& reg) {
    rlBegin(RL_QUADS);

    constexpr float OBSTACLE_HEIGHT = 20.0f;
    constexpr float LOWBAR_HEIGHT   = 30.0f;
    constexpr float HIGHBAR_HEIGHT  = 10.0f;

    // Emit a 3D slab (box) with top, front, back, left, and right faces.
    // Bottom at y=0, top at y=h.  Each face is a 4-vertex quad.
    auto emit_slab = [](float x, float z, float w, float d, float h,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        // Darker shade for side faces
        uint8_t sr = static_cast<uint8_t>(r * 0.7f);
        uint8_t sg = static_cast<uint8_t>(g * 0.7f);
        uint8_t sb = static_cast<uint8_t>(b * 0.7f);

        // Top face (bright) — visible from the camera above
        rlColor4ub(r, g, b, a);
        rlVertex3fScaled(x,     h, z);
        rlVertex3fScaled(x,     h, z + d);
        rlVertex3fScaled(x + w, h, z + d);
        rlVertex3fScaled(x + w, h, z);

        // Front face (darker) — facing the camera (low-z side)
        rlColor4ub(sr, sg, sb, a);
        rlVertex3fScaled(x,     0.0f, z);
        rlVertex3fScaled(x,     h,    z);
        rlVertex3fScaled(x + w, h,    z);
        rlVertex3fScaled(x + w, 0.0f, z);

        // Back face (darker) — away from camera (high-z side)
        rlVertex3fScaled(x,     0.0f, z + d);
        rlVertex3fScaled(x + w, 0.0f, z + d);
        rlVertex3fScaled(x + w, h,    z + d);
        rlVertex3fScaled(x,     h,    z + d);

        // Left side face
        rlVertex3fScaled(x, 0.0f, z);
        rlVertex3fScaled(x, 0.0f, z + d);
        rlVertex3fScaled(x, h,    z + d);
        rlVertex3fScaled(x, h,    z);

        // Right side face
        rlVertex3fScaled(x + w, 0.0f, z);
        rlVertex3fScaled(x + w, h,    z);
        rlVertex3fScaled(x + w, h,    z + d);
        rlVertex3fScaled(x + w, 0.0f, z + d);
    };

    // Flat quad on XZ plane at y=0 (used for particles).
    auto emit_quad = [](float x, float z, float w, float d,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        rlColor4ub(r, g, b, a);
        rlVertex3fScaled(x,     0.0f, z);
        rlVertex3fScaled(x,     0.0f, z + d);
        rlVertex3fScaled(x + w, 0.0f, z + d);
        rlVertex3fScaled(x + w, 0.0f, z);
    };

    // ── Obstacles ────────────────────────────────────────────
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate:
                    emit_slab(0, pos.y, pos.x - 50, dsz.h, OBSTACLE_HEIGHT,
                              col.r, col.g, col.b, col.a);
                    emit_slab(pos.x + 50, pos.y,
                              constants::SCREEN_W - pos.x - 50, dsz.h,
                              OBSTACLE_HEIGHT,
                              col.r, col.g, col.b, col.a);
                    break;

                case ObstacleKind::LaneBlock: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - dsz.w / 2;
                                emit_slab(lx, pos.y, dsz.w, dsz.h,
                                          OBSTACLE_HEIGHT,
                                          col.r, col.g, col.b, col.a);
                            }
                        }
                    }
                    break;
                }

                case ObstacleKind::LanePushLeft:
                case ObstacleKind::LanePushRight: {
                    float lx = pos.x - dsz.w / 2;
                    emit_slab(lx, pos.y, dsz.w, dsz.h,
                              OBSTACLE_HEIGHT,
                              col.r, col.g, col.b, col.a);
                    break;
                }

                case ObstacleKind::LowBar:
                    emit_slab(0, pos.y, static_cast<float>(constants::SCREEN_W),
                              dsz.h, LOWBAR_HEIGHT,
                              col.r, col.g, col.b, col.a);
                    break;

                case ObstacleKind::HighBar:
                    emit_slab(0, pos.y, static_cast<float>(constants::SCREEN_W),
                              dsz.h, HIGHBAR_HEIGHT,
                              col.r, col.g, col.b, col.a);
                    break;

                case ObstacleKind::ComboGate: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - 120;
                                emit_slab(lx, pos.y, 240.0f, dsz.h,
                                          OBSTACLE_HEIGHT,
                                          col.r, col.g, col.b, col.a);
                            }
                        }
                    }
                    break;
                }

                case ObstacleKind::SplitPath: {
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    for (int i = 0; i < 3; ++i) {
                        if (!rlane || i != rlane->lane) {
                            float lx = constants::LANE_X[i] - 120;
                            emit_slab(lx, pos.y, 240.0f, dsz.h,
                                      OBSTACLE_HEIGHT,
                                      col.r, col.g, col.b, col.a);
                        }
                    }
                    break;
                }
            }
        }
    }

    // ── Particles ────────────────────────────────────────────
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, Color, Lifetime>();
        for (auto [entity, pos, pdata, col, life] : view.each()) {
            float ratio = (life.max_time > 0.0f) ? (life.remaining / life.max_time) : 1.0f;
            float sz = pdata.size * ratio;
            float half = sz / 2.0f;
            emit_quad(pos.x - half, pos.y - half, sz, sz,
                      col.r, col.g, col.b, col.a);
        }
    }

    rlEnd();
}

// ── Pass 1: Floor lines (background layer) ──────────────────────────────────
void flush_floor_lines(entt::registry& reg, const FloorParams& fp) {
    (void)reg;

    static const Color LANE_COLORS[3] = {
        {80, 200, 255, 255}, {255, 100, 100, 255}, {100, 255, 100, 255},
    };

    rlBegin(RL_LINES);

    // Corridor edges on XZ plane
    {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        rlColor4ub(40, 40, 60, 120);
        rlVertex3fScaled(0.0f, 0.0f, 0.0f);  rlVertex3fScaled(0.0f, 0.0f, sh);
        rlVertex3fScaled(sw,   0.0f, 0.0f);  rlVertex3fScaled(sw,   0.0f, sh);
    }

    // Floor connectors + shape outlines
    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float cx = constants::LANE_X[lane];
        Color c = LANE_COLORS[lane];
        c.a = fp.alpha;

        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            float cz = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

            // Connector line between adjacent floor shapes
            if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                float next_cz = cz + constants::FLOOR_SHAPE_SPACING;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3fScaled(cx, 0.0f, cz + fp.half);
                rlVertex3fScaled(cx, 0.0f, next_cz - fp.half);
            }

            // Lane 1: square outlines
            if (lane == 1) {
                float l = cx - fp.half, r = cx + fp.half;
                float t = cz - fp.half, b = cz + fp.half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3fScaled(l, 0.0f, t); rlVertex3fScaled(r, 0.0f, t);
                rlVertex3fScaled(r, 0.0f, t); rlVertex3fScaled(r, 0.0f, b);
                rlVertex3fScaled(r, 0.0f, b); rlVertex3fScaled(l, 0.0f, b);
                rlVertex3fScaled(l, 0.0f, b); rlVertex3fScaled(l, 0.0f, t);
            }

            // Lane 2: triangle outlines
            if (lane == 2) {
                float apex_x = cx, apex_z = cz - fp.half;
                float bl_x = cx - fp.half, bl_z = cz + fp.half;
                float br_x = cx + fp.half, br_z = cz + fp.half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3fScaled(apex_x, 0.0f, apex_z); rlVertex3fScaled(br_x, 0.0f, br_z);
                rlVertex3fScaled(br_x, 0.0f, br_z);     rlVertex3fScaled(bl_x, 0.0f, bl_z);
                rlVertex3fScaled(bl_x, 0.0f, bl_z);     rlVertex3fScaled(apex_x, 0.0f, apex_z);
            }
        }
    }

    rlEnd();
}

// ── Pass 2: Floor rings (background layer) ──────────────────────────────────
void flush_floor_rings(const FloorParams& fp) {
    static const Color LANE0_COLOR = {80, 200, 255, 255};
    Color c = LANE0_COLOR;
    c.a = fp.alpha;

    rlBegin(RL_TRIANGLES);
    for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
        float cz = constants::FLOOR_Y_START
            + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
        float cx = constants::LANE_X[0];
        float inner_r = fp.half - fp.thick;
        float outer_r = fp.half;

        int seg = 12;
        for (int i = 0; i < seg; ++i) {
            int idx      = (i * shape_verts::CIRCLE_SEGMENTS) / seg;
            int next_idx = ((i + 1) * shape_verts::CIRCLE_SEGMENTS) / seg
                           % shape_verts::CIRCLE_SEGMENTS;

            float ox1 = cx + shape_verts::CIRCLE[idx].x      * outer_r;
            float oz1 = cz + shape_verts::CIRCLE[idx].y      * outer_r;
            float ox2 = cx + shape_verts::CIRCLE[next_idx].x * outer_r;
            float oz2 = cz + shape_verts::CIRCLE[next_idx].y * outer_r;
            float ix1 = cx + shape_verts::CIRCLE[idx].x      * inner_r;
            float iz1 = cz + shape_verts::CIRCLE[idx].y      * inner_r;
            float ix2 = cx + shape_verts::CIRCLE[next_idx].x * inner_r;
            float iz2 = cz + shape_verts::CIRCLE[next_idx].y * inner_r;

            rlColor4ub(c.r, c.g, c.b, c.a);
            rlVertex3fScaled(ox1, 0.0f, oz1);
            rlVertex3fScaled(ix1, 0.0f, iz1);
            rlVertex3fScaled(ox2, 0.0f, oz2);
            rlVertex3fScaled(ix1, 0.0f, iz1);
            rlVertex3fScaled(ix2, 0.0f, iz2);
            rlVertex3fScaled(ox2, 0.0f, oz2);
        }
    }
    rlEnd();
}

// ── Pass 4: Gameplay triangles (ghost shapes + player) ──────────────────────
void flush_gameplay_tris(entt::registry& reg) {
    // Thin wrapper — delegates to the file-local emit_3d_shape helper.
    // Must be called inside an active RL_TRIANGLES block (no rlBegin/rlEnd).
    auto emit_shape = [](Shape shape, float cx, float y_3d, float cz,
                         float sz,
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        emit_3d_shape(shape, cx, y_3d, cz, sz, r, g, b, a);
    };

    rlBegin(RL_TRIANGLES);

    // Ghost shapes (obstacle indicators)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) emit_shape(req->shape, pos.x, 0.0f,
                                        pos.y + dsz.h / 2, 40,
                                        col.r, col.g, col.b, 120);
                    break;
                }
                case ObstacleKind::ComboGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        auto* blocked = reg.try_get<BlockedLanes>(entity);
                        int open = 1;
                        if (blocked) {
                            for (int i = 0; i < 3; ++i)
                                if (!((blocked->mask >> i) & 1)) { open = i; break; }
                        }
                        emit_shape(req->shape, constants::LANE_X[open],
                                   0.0f, pos.y + dsz.h / 2, 30,
                                   255, 255, 255, 180);
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    if (req && rlane)
                        emit_shape(req->shape, constants::LANE_X[rlane->lane],
                                   0.0f, pos.y + dsz.h / 2, 30,
                                   255, 255, 255, 180);
                    break;
                }
                default: break;
            }
        }
    }

    // Player shape
    {
        auto view = reg.view<PlayerTag, Position, PlayerShape, VerticalState, Color>();
        for (auto [entity, pos, pshape, vstate, col] : view.each()) {
            float y_3d = -vstate.y_offset;  // jump lifts off ground plane
            float sz = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) sz *= 0.5f;
            emit_shape(pshape.current, pos.x, y_3d, pos.y, sz,
                       col.r, col.g, col.b, col.a);
        }
    }

    rlEnd();
}

} // namespace camera
