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

// ── Shading helpers ──────────────────────────────────────────────────────────
struct FaceColors {
    uint8_t tr, tg, tb;  // top (brightest)
    uint8_t fr, fg, fb;  // front
    uint8_t sr, sg, sb;  // side
    uint8_t dr, dg, db;  // dark (back/bottom)
    uint8_t a;
};

static FaceColors make_face_colors(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    auto boost = [](uint8_t c) -> uint8_t {
        int v = static_cast<int>(c * 1.2f);
        return static_cast<uint8_t>(v > 255 ? 255 : v);
    };
    return {
        boost(r), boost(g), boost(b),
        static_cast<uint8_t>(r * 0.65f), static_cast<uint8_t>(g * 0.65f), static_cast<uint8_t>(b * 0.65f),
        static_cast<uint8_t>(r * 0.50f), static_cast<uint8_t>(g * 0.50f), static_cast<uint8_t>(b * 0.50f),
        static_cast<uint8_t>(r * 0.35f), static_cast<uint8_t>(g * 0.35f), static_cast<uint8_t>(b * 0.35f),
        a,
    };
}

// ── Generic extruded prism ──────────────────────────────────────────────────
// Takes a compile-time unit ring (shape_verts::V2[]) and applies position +
// scale at emit time.  No per-frame vertex computation — just multiply.
//
//   ring[]   = constexpr unit vertices (from shape_vertices.h)
//   cx, cz   = world center
//   radius   = scale factor applied to ring
//   bot_y    = base height
//   top_y    = cap height

static void emit_prism_caps(const shape_verts::V2* ring, int n,
                             float cx, float cz, float radius,
                             float bot_y, float top_y,
                             const FaceColors& fc) {
    rlColor4ub(fc.tr, fc.tg, fc.tb, fc.a);
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        rlVertex3fScaled(cx, top_y, cz);
        rlVertex3fScaled(cx + ring[i].x * radius,    top_y, cz + ring[i].y * radius);
        rlVertex3fScaled(cx + ring[next].x * radius,  top_y, cz + ring[next].y * radius);
    }
    rlColor4ub(fc.dr, fc.dg, fc.db, fc.a);
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        rlVertex3fScaled(cx, bot_y, cz);
        rlVertex3fScaled(cx + ring[next].x * radius,  bot_y, cz + ring[next].y * radius);
        rlVertex3fScaled(cx + ring[i].x * radius,    bot_y, cz + ring[i].y * radius);
    }
}

static void emit_prism_walls(const shape_verts::V2* ring, int n,
                              float cx, float cz, float radius,
                              float bot_y, float top_y,
                              const FaceColors& fc) {
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        float x0 = cx + ring[i].x * radius,    z0 = cz + ring[i].y * radius;
        float x1 = cx + ring[next].x * radius,  z1 = cz + ring[next].y * radius;

        bool front = (z0 + z1) * 0.5f < cz;
        if (front) rlColor4ub(fc.fr, fc.fg, fc.fb, fc.a);
        else       rlColor4ub(fc.sr, fc.sg, fc.sb, fc.a);

        rlVertex3fScaled(x0, bot_y, z0);
        rlVertex3fScaled(x0, top_y, z0);
        rlVertex3fScaled(x1, top_y, z1);
        rlVertex3fScaled(x1, bot_y, z1);
    }
}

// ── Per-shape unit ring table ───────────────────────────────────────────────
// Maps Shape enum → {constexpr ring pointer, vertex count, radius scale, height ratio}.
// All data is compile-time constant.  The circle ring is a 12-point subset of
// the 24-segment table in shape_vertices.h, precomputed below.

static constexpr shape_verts::V2 CIRCLE_12[12] = {
    { 1.000000f,  0.000000f}, { 0.866025f,  0.500000f},
    { 0.500000f,  0.866025f}, { 0.000000f,  1.000000f},
    {-0.500000f,  0.866025f}, {-0.866025f,  0.500000f},
    {-1.000000f,  0.000000f}, {-0.866025f, -0.500000f},
    {-0.500000f, -0.866025f}, { 0.000000f, -1.000000f},
    { 0.500000f, -0.866025f}, { 0.866025f, -0.500000f},
};

struct ShapeDesc {
    const shape_verts::V2* ring;
    int    n;
    float  radius_scale;  // multiplied by size to get world radius
    float  height_scale;  // multiplied by size to get extrusion height
};

static constexpr ShapeDesc SHAPE_TABLE[] = {
    { CIRCLE_12,              12, 0.5f, 0.3f },  // Circle: radius = size/2
    { shape_verts::SQUARE,     4, 0.5f, 0.3f },  // Square: half-extent = size/2
    { shape_verts::TRIANGLE,   3, 0.5f, 0.3f },  // Triangle: half-extent = size/2
    { shape_verts::HEXAGON,    6, 0.6f, 0.7f },  // Hexagon: radius = size*0.6
};

// ── Standalone shape draw ────────────────────────────────────────────────────
void draw_shape(Shape shape, float cx, float y_3d, float cz, float size, Color c) {
    FaceColors fc = make_face_colors(c.r, c.g, c.b, c.a);
    const auto& desc = SHAPE_TABLE[static_cast<int>(shape)];
    float radius = size * desc.radius_scale;
    float top_y  = y_3d + size * desc.height_scale;

    rlBegin(RL_TRIANGLES);
    emit_prism_caps(desc.ring, desc.n, cx, cz, radius, y_3d, top_y, fc);
    rlEnd();
    rlBegin(RL_QUADS);
    emit_prism_walls(desc.ring, desc.n, cx, cz, radius, y_3d, top_y, fc);
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
    // Uses directional shading: top=bright, front=85%, sides=60%, back=45%.
    auto emit_slab = [](float x, float z, float w, float d, float h,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        auto boost = [](uint8_t c) -> uint8_t {
            int v = static_cast<int>(c * 1.2f);
            return static_cast<uint8_t>(v > 255 ? 255 : v);
        };
        uint8_t tr = boost(r), tg = boost(g), tb = boost(b);
        uint8_t fr = static_cast<uint8_t>(r * 0.65f);
        uint8_t fg = static_cast<uint8_t>(g * 0.65f);
        uint8_t fb = static_cast<uint8_t>(b * 0.65f);
        uint8_t sr = static_cast<uint8_t>(r * 0.50f);
        uint8_t sg = static_cast<uint8_t>(g * 0.50f);
        uint8_t sb = static_cast<uint8_t>(b * 0.50f);
        uint8_t dr = static_cast<uint8_t>(r * 0.35f);
        uint8_t dg = static_cast<uint8_t>(g * 0.35f);
        uint8_t db = static_cast<uint8_t>(b * 0.35f);

        // Top face (brightest — overhead light)
        rlColor4ub(tr, tg, tb, a);
        rlVertex3fScaled(x,     h, z);
        rlVertex3fScaled(x,     h, z + d);
        rlVertex3fScaled(x + w, h, z + d);
        rlVertex3fScaled(x + w, h, z);

        // Front face — facing the camera (low-z side, brighter)
        rlColor4ub(fr, fg, fb, a);
        rlVertex3fScaled(x,     0.0f, z);
        rlVertex3fScaled(x,     h,    z);
        rlVertex3fScaled(x + w, h,    z);
        rlVertex3fScaled(x + w, 0.0f, z);

        // Back face — away from camera (high-z side, darkest)
        rlColor4ub(dr, dg, db, a);
        rlVertex3fScaled(x,     0.0f, z + d);
        rlVertex3fScaled(x + w, 0.0f, z + d);
        rlVertex3fScaled(x + w, h,    z + d);
        rlVertex3fScaled(x,     h,    z + d);

        // Left side face (side shading)
        rlColor4ub(sr, sg, sb, a);
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

    // Lane guide lines — continuous lines from bottom to top of corridor.
    // With Camera3D perspective, these naturally converge toward the top.
    {
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        constexpr float lane_half = 120.0f;  // half lane width
        for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
            float cx = constants::LANE_X[lane];
            Color c = LANE_COLORS[lane];
            // Left edge of lane
            rlColor4ub(c.r, c.g, c.b, 50);
            rlVertex3fScaled(cx - lane_half, 0.0f, 0.0f);
            rlVertex3fScaled(cx - lane_half, 0.0f, sh);
            // Right edge of lane
            rlVertex3fScaled(cx + lane_half, 0.0f, 0.0f);
            rlVertex3fScaled(cx + lane_half, 0.0f, sh);
        }
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

// ── Pass 4: Gameplay shapes (ghost shapes + player) ─────────────────────────
void flush_gameplay_tris(entt::registry& reg) {
    // Helper to emit a shape across two batches (caps + sides).
    // Collects shape draw calls, then emits caps in RL_TRIANGLES and
    // sides in RL_QUADS (which handles winding correctly).
    struct ShapeDraw {
        Shape shape; float cx, y_3d, cz, sz;
        uint8_t r, g, b, a;
    };

    // Collect all shape draws (small fixed count: ~10-20 max)
    static constexpr int MAX_DRAWS = 64;
    ShapeDraw draws[MAX_DRAWS];
    int count = 0;

    auto add = [&](Shape shape, float cx, float y_3d, float cz, float sz,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (count < MAX_DRAWS)
            draws[count++] = {shape, cx, y_3d, cz, sz, r, g, b, a};
    };

    // Ghost shapes (obstacle indicators)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) add(req->shape, pos.x, 0.0f,
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
                        add(req->shape, constants::LANE_X[open],
                            0.0f, pos.y + dsz.h / 2, 30,
                            255, 255, 255, 180);
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    if (req && rlane)
                        add(req->shape, constants::LANE_X[rlane->lane],
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
            float y_3d = -vstate.y_offset;
            float sz = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) sz *= 0.5f;
            add(pshape.current, pos.x, y_3d, pos.y, sz,
                col.r, col.g, col.b, col.a);
        }
    }

    // Emit caps (top/bottom faces) — RL_TRIANGLES
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < count; ++i) {
        auto& d = draws[i];
        const auto& desc = SHAPE_TABLE[static_cast<int>(d.shape)];
        FaceColors fc = make_face_colors(d.r, d.g, d.b, d.a);
        float radius = d.sz * desc.radius_scale;
        float top_y  = d.y_3d + d.sz * desc.height_scale;
        emit_prism_caps(desc.ring, desc.n, d.cx, d.cz, radius,
                        d.y_3d, top_y, fc);
    }
    rlEnd();

    // Emit side walls — RL_QUADS (handles winding correctly)
    rlBegin(RL_QUADS);
    for (int i = 0; i < count; ++i) {
        auto& d = draws[i];
        const auto& desc = SHAPE_TABLE[static_cast<int>(d.shape)];
        FaceColors fc = make_face_colors(d.r, d.g, d.b, d.a);
        float radius = d.sz * desc.radius_scale;
        float top_y  = d.y_3d + d.sz * desc.height_scale;
        emit_prism_walls(desc.ring, desc.n, d.cx, d.cz, radius,
                         d.y_3d, top_y, fc);
    }
    rlEnd();
}

} // namespace camera
