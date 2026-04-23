#include "camera_system.h"
#include "../components/shape_mesh.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/rendering.h"
#include "../components/particle.h"
#include "../components/lifetime.h"
#include "../constants.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

namespace camera {

static constexpr float S = camera::WORLD_SCALE;
static inline void rlVertex3fScaled(float x, float y, float z) {
    rlVertex3f(x / S, y / S, z / S);
}

// ── GPU mesh upload from ShapeMeshData ──────────────────────────────────────
// Converts the pure-data ShapeMeshData (testable, no raylib) into a raylib
// Mesh with GPU vertex buffers.

static Mesh upload_shape_mesh(const ShapeMeshData& data) {
    int vc = data.vertex_count;
    Mesh mesh = {};
    mesh.vertexCount   = vc;
    mesh.triangleCount = data.tri_count;
    mesh.vertices  = static_cast<float*>(RL_CALLOC(static_cast<unsigned int>(vc * 3), sizeof(float)));
    mesh.normals   = static_cast<float*>(RL_CALLOC(static_cast<unsigned int>(vc * 3), sizeof(float)));
    mesh.texcoords = static_cast<float*>(RL_CALLOC(static_cast<unsigned int>(vc * 2), sizeof(float)));
    mesh.colors    = static_cast<unsigned char*>(RL_CALLOC(static_cast<unsigned int>(vc * 4), sizeof(unsigned char)));

    for (int i = 0; i < vc; ++i) {
        mesh.vertices[i*3+0] = data.positions[i].x;
        mesh.vertices[i*3+1] = data.positions[i].y;
        mesh.vertices[i*3+2] = data.positions[i].z;
        mesh.normals[i*3+0]  = data.normals[i].x;
        mesh.normals[i*3+1]  = data.normals[i].y;
        mesh.normals[i*3+2]  = data.normals[i].z;
        mesh.colors[i*4+0]   = data.colors[i].r;
        mesh.colors[i*4+1]   = data.colors[i].g;
        mesh.colors[i*4+2]   = data.colors[i].b;
        mesh.colors[i*4+3]   = data.colors[i].a;
    }

    UploadMesh(&mesh, false);
    return mesh;
}

ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};
    for (int i = 0; i < 4; ++i)
        sm.meshes[i] = upload_shape_mesh(build_prism(SHAPE_TABLE[i]));
    sm.material = LoadMaterialDefault();
    return sm;
}

void unload_shape_meshes(ShapeMeshes& sm) {
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.meshes[i]);
    UnloadMaterial(sm.material);
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

// ── Pass 4: Gameplay shapes via GPU meshes ──────────────────────────────────
// Shape geometry lives on the GPU (built once by build_shape_meshes).
// Per frame: compute a transform matrix + set material color → DrawMesh.
void flush_gameplay_tris(entt::registry& reg) {
    auto* sm = reg.ctx().find<ShapeMeshes>();
    if (!sm) return;

    auto draw = [&](Shape shape, float cx, float y_3d, float cz,
                    float sz, Color tint) {
        int idx = static_cast<int>(shape);
        const auto& desc = SHAPE_TABLE[idx];
        float scale = sz * desc.radius_scale / S;
        Matrix mat = MatrixMultiply(MatrixScale(scale, scale, scale),
                                    MatrixTranslate(cx / S, y_3d / S, cz / S));
        sm->material.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(sm->meshes[idx], sm->material, mat);
    };

    // Ghost shapes
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) draw(req->shape, pos.x, 0.0f,
                                  pos.y + dsz.h / 2, 40,
                                  {col.r, col.g, col.b, 120});
                    break;
                }
                case ObstacleKind::ComboGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        auto* blocked = reg.try_get<BlockedLanes>(entity);
                        int open = 1;
                        if (blocked)
                            for (int i = 0; i < 3; ++i)
                                if (!((blocked->mask >> i) & 1)) { open = i; break; }
                        draw(req->shape, constants::LANE_X[open],
                             0.0f, pos.y + dsz.h / 2, 30,
                             {255, 255, 255, 180});
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    if (req && rlane)
                        draw(req->shape, constants::LANE_X[rlane->lane],
                             0.0f, pos.y + dsz.h / 2, 30,
                             {255, 255, 255, 180});
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
            draw(pshape.current, pos.x, y_3d, pos.y, sz, col);
        }
    }
}

} // namespace camera
