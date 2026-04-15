#include "perspective.h"
#include "shape_vertices.h"
#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "components/obstacle_data.h"
#include "components/rendering.h"
#include "components/particle.h"
#include "components/lifetime.h"
#include "components/song_state.h"
#include <raylib.h>
#include <rlgl.h>

namespace perspective {

// ── Filled rectangle (axis-aligned in world → trapezoid after projection) ────
// Height is scaled by depth at the rect's vertical centre so objects appear
// shorter when far from the camera.
void draw_rect(float x, float y, float w, float h, Color c) {
    float cy  = y + h / 2.0f;
    float d   = depth(cy);
    float sh  = h * d;                // perspective-scaled height
    float top = cy - sh / 2.0f;
    float bot = cy + sh / 2.0f;

    float d_top = depth(top);
    float d_bot = depth(bot);

    float tl_x = CENTER + (x     - CENTER) * d_top;
    float tr_x = CENTER + (x + w - CENTER) * d_top;
    float bl_x = CENTER + (x     - CENTER) * d_bot;
    float br_x = CENTER + (x + w - CENTER) * d_bot;

    Vector2 tl = {tl_x, top};
    Vector2 tr = {tr_x, top};
    Vector2 bl = {bl_x, bot};
    Vector2 br = {br_x, bot};

    DrawTriangle(tl, bl, tr, c);
    DrawTriangle(tr, bl, br, c);
}

// ── Filled shape (Circle, Square, Triangle, Hexagon) ─────────────────────────
// Vertex Y-offsets are scaled by depth(cy) so shapes shrink uniformly as they
// recede toward the vanishing point.
void draw_shape(Shape shape, float cx, float cy, float size, Color c) {
    float d_cy = depth(cy);

    switch (shape) {
        case Shape::Circle: {
            float r = size / 2.0f;
            Vector2 centre = project(cx, cy);
            Vector2 prev = project(cx + shape_verts::CIRCLE[0].x * r,
                                   cy + shape_verts::CIRCLE[0].y * r * d_cy);
            for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
                Vector2 cur = project(cx + shape_verts::CIRCLE[next].x * r,
                                      cy + shape_verts::CIRCLE[next].y * r * d_cy);
                DrawTriangle(centre, cur, prev, c);
                prev = cur;
            }
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            Vector2 corners[4];
            for (int i = 0; i < 4; ++i) {
                corners[i] = project(cx + shape_verts::SQUARE[i].x * half,
                                     cy + shape_verts::SQUARE[i].y * half * d_cy);
            }
            DrawTriangle(corners[0], corners[3], corners[1], c);
            DrawTriangle(corners[1], corners[3], corners[2], c);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            Vector2 verts[3];
            for (int i = 0; i < 3; ++i) {
                verts[i] = project(cx + shape_verts::TRIANGLE[i].x * half,
                                   cy + shape_verts::TRIANGLE[i].y * half * d_cy);
            }
            DrawTriangle(verts[0], verts[1], verts[2], c);
            break;
        }
        case Shape::Hexagon: {
            float radius = size * 0.6f;
            Vector2 centre = project(cx, cy);
            Vector2 prev = project(cx + shape_verts::HEXAGON[0].x * radius,
                                   cy + shape_verts::HEXAGON[0].y * radius * d_cy);
            for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::HEX_SEGMENTS;
                Vector2 cur = project(cx + shape_verts::HEXAGON[next].x * radius,
                                      cy + shape_verts::HEXAGON[next].y * radius * d_cy);
                DrawTriangle(centre, cur, prev, c);
                prev = cur;
            }
            break;
        }
    }
}

// ── Projected line ───────────────────────────────────────────────────────────
void draw_line(float x1, float y1, float x2, float y2, float thick, Color c) {
    Vector2 a = project(x1, y1);
    Vector2 b = project(x2, y2);
    DrawLineEx(a, b, thick, c);
}

// ── Projected ring (annulus) ─────────────────────────────────────────────────
void draw_ring(float cx, float cy, float inner_r, float outer_r, int segments, Color c) {
    int seg = (segments > 0 && segments <= shape_verts::CIRCLE_SEGMENTS)
              ? segments : shape_verts::CIRCLE_SEGMENTS;

    float d_cy = depth(cy);

    for (int i = 0; i < seg; ++i) {
        // Map evenly across the full 0..CIRCLE_SEGMENTS table so the ring
        // always closes correctly, regardless of the requested segment count.
        int idx      = (i       * shape_verts::CIRCLE_SEGMENTS) / seg;
        int next_idx = ((i + 1) * shape_verts::CIRCLE_SEGMENTS) / seg
                       % shape_verts::CIRCLE_SEGMENTS;

        Vector2 outer1 = project(cx + shape_verts::CIRCLE[idx].x      * outer_r,
                                 cy + shape_verts::CIRCLE[idx].y      * outer_r * d_cy);
        Vector2 outer2 = project(cx + shape_verts::CIRCLE[next_idx].x * outer_r,
                                 cy + shape_verts::CIRCLE[next_idx].y * outer_r * d_cy);
        Vector2 inner1 = project(cx + shape_verts::CIRCLE[idx].x      * inner_r,
                                 cy + shape_verts::CIRCLE[idx].y      * inner_r * d_cy);
        Vector2 inner2 = project(cx + shape_verts::CIRCLE[next_idx].x * inner_r,
                                 cy + shape_verts::CIRCLE[next_idx].y * inner_r * d_cy);

        DrawTriangle(outer1, outer2, inner1, c);
        DrawTriangle(inner1, outer2, inner2, c);
    }
}

// ── Projected rectangle outline ──────────────────────────────────────────────
void draw_rect_lines(float x, float y, float w, float h, float thick, Color c) {
    float cy  = y + h / 2.0f;
    float d   = depth(cy);
    float sh  = h * d;
    float top = cy - sh / 2.0f;
    float bot = cy + sh / 2.0f;

    float d_top = depth(top);
    float d_bot = depth(bot);

    Vector2 tl = {CENTER + (x     - CENTER) * d_top, top};
    Vector2 tr = {CENTER + (x + w - CENTER) * d_top, top};
    Vector2 br = {CENTER + (x + w - CENTER) * d_bot, bot};
    Vector2 bl = {CENTER + (x     - CENTER) * d_bot, bot};

    DrawLineEx(tl, tr, thick, c);
    DrawLineEx(tr, br, thick, c);
    DrawLineEx(br, bl, thick, c);
    DrawLineEx(bl, tl, thick, c);
}

// ── Projected triangle outline ───────────────────────────────────────────────
void draw_tri_lines(Vector2 v1, Vector2 v2, Vector2 v3, Color c) {
    Vector2 p1 = project(v1.x, v1.y);
    Vector2 p2 = project(v2.x, v2.y);
    Vector2 p3 = project(v3.x, v3.y);

    DrawLineV(p1, p2, c);
    DrawLineV(p2, p3, c);
    DrawLineV(p3, p1, c);
}

// ── Batched rect flush from ECS ──────────────────────────────────────────────
// Queries registry for ObstacleTag and ParticleTag entities, projects their
// rect geometry, and submits everything in a single RL_QUADS batch.
void flush_world_rects(entt::registry& reg) {
    rlBegin(RL_QUADS);

    // Helper: emit one projected quad (height scaled by depth)
    auto emit_quad = [](float x, float y, float w, float h,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        float cy  = y + h / 2.0f;
        float d   = depth(cy);
        float sh  = h * d;
        float top = cy - sh / 2.0f;
        float bot = cy + sh / 2.0f;

        float d_top = depth(top);
        float d_bot = depth(bot);
        float tl_x = CENTER + (x     - CENTER) * d_top;
        float tr_x = CENTER + (x + w - CENTER) * d_top;
        float bl_x = CENTER + (x     - CENTER) * d_bot;
        float br_x = CENTER + (x + w - CENTER) * d_bot;

        rlColor4ub(r, g, b, a);
        rlVertex2f(tl_x, top);
        rlVertex2f(bl_x, bot);
        rlVertex2f(br_x, bot);
        rlVertex2f(tr_x, top);
    };

    // ── Obstacles: iterate by ObstacleTag ────────────────────
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, DrawColor, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate:
                    emit_quad(0, pos.y, pos.x - 50, dsz.h, col.r, col.g, col.b, col.a);
                    emit_quad(pos.x + 50, pos.y,
                              constants::SCREEN_W - pos.x - 50, dsz.h,
                              col.r, col.g, col.b, col.a);
                    break;

                case ObstacleKind::LaneBlock: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - dsz.w / 2;
                                emit_quad(lx, pos.y, dsz.w, dsz.h,
                                          col.r, col.g, col.b, col.a);
                            }
                        }
                    }
                    break;
                }

                case ObstacleKind::LowBar:
                case ObstacleKind::HighBar:
                    emit_quad(0, pos.y, static_cast<float>(constants::SCREEN_W),
                              dsz.h, col.r, col.g, col.b, col.a);
                    break;

                case ObstacleKind::ComboGate: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - 120;
                                emit_quad(lx, pos.y, 240.0f, dsz.h,
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
                            emit_quad(lx, pos.y, 240.0f, dsz.h,
                                      col.r, col.g, col.b, col.a);
                        }
                    }
                    break;
                }
            }
        }
    }

    // ── Particles: iterate by ParticleTag ────────────────────
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, DrawColor, Lifetime>();
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

    // Corridor edges
    {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        Vector2 lt = project(0.0f, 0.0f);
        Vector2 lb = project(0.0f, sh);
        Vector2 rt = project(sw, 0.0f);
        Vector2 rb = project(sw, sh);
        rlColor4ub(40, 40, 60, 120);
        rlVertex2f(lt.x, lt.y); rlVertex2f(lb.x, lb.y);
        rlVertex2f(rt.x, rt.y); rlVertex2f(rb.x, rb.y);
    }

    // Floor connectors + outlines
    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float cx = constants::LANE_X[lane];
        Color c = LANE_COLORS[lane];
        c.a = fp.alpha;

        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            float cy = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
            float d     = depth(cy);
            float px    = project_x(cx, cy);
            float p_half = fp.half * d;

            if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                float next_cy = cy + constants::FLOOR_SHAPE_SPACING;
                float next_d  = depth(next_cy);
                float next_p_half = fp.half * next_d;
                Vector2 a = project(cx, cy + p_half);
                Vector2 b = project(cx, next_cy - next_p_half);
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex2f(a.x, a.y); rlVertex2f(b.x, b.y);
            }

            if (lane == 1) {
                float l = px - p_half, r = px + p_half;
                float t = cy - p_half, bt = cy + p_half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex2f(l, t); rlVertex2f(r, t);
                rlVertex2f(r, t); rlVertex2f(r, bt);
                rlVertex2f(r, bt); rlVertex2f(l, bt);
                rlVertex2f(l, bt); rlVertex2f(l, t);
            }

            if (lane == 2) {
                float apex_x = px, apex_y = cy - p_half;
                float bl_x = px - p_half, bl_y = cy + p_half;
                float br_x = px + p_half, br_y = cy + p_half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex2f(apex_x, apex_y); rlVertex2f(br_x, br_y);
                rlVertex2f(br_x, br_y);     rlVertex2f(bl_x, bl_y);
                rlVertex2f(bl_x, bl_y);     rlVertex2f(apex_x, apex_y);
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
        float cy = constants::FLOOR_Y_START
            + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
        float d = depth(cy);
        float px = project_x(constants::LANE_X[0], cy);
        float p_half  = fp.half * d;
        float p_thick = fp.thick * d;
        float inner_r = p_half - p_thick;
        float outer_r = p_half;

        int seg = 12;
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
            rlColor4ub(c.r, c.g, c.b, c.a);
            rlVertex2f(ox1, oy1); rlVertex2f(ix1, iy1); rlVertex2f(ox2, oy2);
            rlVertex2f(ix1, iy1); rlVertex2f(ix2, iy2); rlVertex2f(ox2, oy2);
        }
    }
    rlEnd();
}

// ── Pass 4: Gameplay triangles (ghost shapes + player) ──────────────────────
void flush_gameplay_tris(entt::registry& reg) {
    // Helper: emit a fan shape from cached vertex table (perspective-projected)
    // Y-offsets are scaled by depth(cy) for vertical foreshortening.
    auto emit_fan = [](const shape_verts::V2* table, int count, float radius,
                       float cx, float cy, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        float d_cy = depth(cy);
        Vector2 centre = project(cx, cy);
        Vector2 prev = project(cx + table[0].x * radius,
                               cy + table[0].y * radius * d_cy);
        for (int i = 0; i < count; ++i) {
            int next = (i + 1) % count;
            Vector2 cur = project(cx + table[next].x * radius,
                                  cy + table[next].y * radius * d_cy);
            rlColor4ub(r, g, b, a);
            rlVertex2f(centre.x, centre.y);
            rlVertex2f(cur.x, cur.y);
            rlVertex2f(prev.x, prev.y);
            prev = cur;
        }
    };

    auto emit_shape = [&emit_fan](Shape shape, float cx, float cy, float sz,
                                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        switch (shape) {
            case Shape::Circle:
                emit_fan(shape_verts::CIRCLE, shape_verts::CIRCLE_SEGMENTS,
                         sz / 2.0f, cx, cy, r, g, b, a);
                break;
            case Shape::Square: {
                float half = sz / 2.0f;
                float d_cy = depth(cy);
                Vector2 v[4];
                for (int i = 0; i < 4; ++i)
                    v[i] = project(cx + shape_verts::SQUARE[i].x * half,
                                   cy + shape_verts::SQUARE[i].y * half * d_cy);
                rlColor4ub(r, g, b, a);
                rlVertex2f(v[0].x, v[0].y); rlVertex2f(v[3].x, v[3].y); rlVertex2f(v[1].x, v[1].y);
                rlVertex2f(v[1].x, v[1].y); rlVertex2f(v[3].x, v[3].y); rlVertex2f(v[2].x, v[2].y);
                break;
            }
            case Shape::Triangle: {
                float half = sz / 2.0f;
                float d_cy = depth(cy);
                Vector2 v[3];
                for (int i = 0; i < 3; ++i)
                    v[i] = project(cx + shape_verts::TRIANGLE[i].x * half,
                                   cy + shape_verts::TRIANGLE[i].y * half * d_cy);
                rlColor4ub(r, g, b, a);
                rlVertex2f(v[0].x, v[0].y); rlVertex2f(v[1].x, v[1].y); rlVertex2f(v[2].x, v[2].y);
                break;
            }
            case Shape::Hexagon:
                emit_fan(shape_verts::HEXAGON, shape_verts::HEX_SEGMENTS,
                         sz * 0.6f, cx, cy, r, g, b, a);
                break;
        }
    };

    rlBegin(RL_TRIANGLES);

    // Ghost shapes (obstacle indicators)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, DrawColor, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) emit_shape(req->shape, pos.x, pos.y + dsz.h / 2, 40,
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
                                   pos.y + dsz.h / 2, 30, 255, 255, 255, 180);
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    if (req && rlane)
                        emit_shape(req->shape, constants::LANE_X[rlane->lane],
                                   pos.y + dsz.h / 2, 30, 255, 255, 255, 180);
                    break;
                }
                default: break;
            }
        }
    }

    // Player shape
    {
        auto view = reg.view<PlayerTag, Position, PlayerShape, VerticalState, DrawColor>();
        for (auto [entity, pos, pshape, vstate, col] : view.each()) {
            float draw_y = pos.y + vstate.y_offset;
            float sz = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) sz *= 0.5f;
            emit_shape(pshape.current, pos.x, draw_y, sz,
                       col.r, col.g, col.b, col.a);
        }
    }

    rlEnd();
}

} // namespace perspective
