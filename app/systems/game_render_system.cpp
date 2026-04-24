#include "all_systems.h"
#include "../components/shape_mesh.h"
#include "../components/shape_vertices.h"
#include "../components/camera.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "camera_system.h"
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
// 3D world draw passes — floor, obstacles, particles, shapes
// ═════════════════════════════════════════════════════════════════════════════

static void draw_floor_lines(entt::registry& reg, const FloorParams& fp) {
    (void)reg;
    static const Color LANE_COLORS[3] = {
        {80, 200, 255, 255}, {255, 100, 100, 255}, {100, 255, 100, 255},
    };

    rlBegin(RL_LINES);

    // Corridor edges
    {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        rlColor4ub(40, 40, 60, 120);
        rlVertex3f(0.0f, 0.0f, 0.0f);  rlVertex3f(0.0f, 0.0f, sh);
        rlVertex3f(sw,   0.0f, 0.0f);  rlVertex3f(sw,   0.0f, sh);
    }

    // Lane guide lines
    {
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        constexpr float lane_half = 120.0f;
        for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
            float cx = constants::LANE_X[lane];
            Color c = LANE_COLORS[lane];
            rlColor4ub(c.r, c.g, c.b, 50);
            rlVertex3f(cx - lane_half, 0.0f, 0.0f);
            rlVertex3f(cx - lane_half, 0.0f, sh);
            rlVertex3f(cx + lane_half, 0.0f, 0.0f);
            rlVertex3f(cx + lane_half, 0.0f, sh);
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

            if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                float next_cz = cz + constants::FLOOR_SHAPE_SPACING;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3f(cx, 0.0f, cz + fp.half);
                rlVertex3f(cx, 0.0f, next_cz - fp.half);
            }

            if (lane == 1) {
                float l = cx - fp.half, r = cx + fp.half;
                float t = cz - fp.half, b = cz + fp.half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3f(l, 0.0f, t); rlVertex3f(r, 0.0f, t);
                rlVertex3f(r, 0.0f, t); rlVertex3f(r, 0.0f, b);
                rlVertex3f(r, 0.0f, b); rlVertex3f(l, 0.0f, b);
                rlVertex3f(l, 0.0f, b); rlVertex3f(l, 0.0f, t);
            }

            if (lane == 2) {
                float apex_x = cx, apex_z = cz - fp.half;
                float bl_x = cx - fp.half, bl_z = cz + fp.half;
                float br_x = cx + fp.half, br_z = cz + fp.half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3f(apex_x, 0.0f, apex_z); rlVertex3f(br_x, 0.0f, br_z);
                rlVertex3f(br_x, 0.0f, br_z);     rlVertex3f(bl_x, 0.0f, bl_z);
                rlVertex3f(bl_x, 0.0f, bl_z);     rlVertex3f(apex_x, 0.0f, apex_z);
            }
        }
    }

    rlEnd();
}

static void draw_floor_rings(const FloorParams& fp) {
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
            rlVertex3f(ox1, 0.0f, oz1);
            rlVertex3f(ix1, 0.0f, iz1);
            rlVertex3f(ox2, 0.0f, oz2);
            rlVertex3f(ix1, 0.0f, iz1);
            rlVertex3f(ix2, 0.0f, iz2);
            rlVertex3f(ox2, 0.0f, oz2);
        }
    }
    rlEnd();
}

static void draw_meshes(entt::registry& reg) {
    auto* sm = reg.ctx().find<camera::ShapeMeshes>();
    if (!sm) return;

    auto view = reg.view<ModelTransform>();
    for (auto [entity, mt] : view.each()) {
        sm->material.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
        switch (mt.mesh_type) {
            case MeshType::Slab:
                DrawMesh(sm->slab, sm->material, mt.mat);
                break;
            case MeshType::Shape:
                DrawMesh(sm->shapes[mt.mesh_index], sm->material, mt.mat);
                break;
            case MeshType::Quad:
                DrawMesh(sm->quad, sm->material, mt.mat);
                break;
        }
    }
}

void game_render_system(entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& camera = reg.ctx().get<Camera3D>();

    ClearBackground({15, 15, 25, 255});

    rlSetClipPlanes(1.0, 5000.0);
    BeginMode3D(camera);

    // Floor pulse params
    FloorParams floor_params{};
    {
        auto* song = reg.ctx().find<SongState>();
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && song->current_beat >= 0) {
            float time_since_beat = song->song_time
                - (song->offset + static_cast<float>(song->current_beat) * song->beat_period);
            float pulse_t = std::clamp(time_since_beat / constants::FLOOR_PULSE_DECAY, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - pulse_t) * (1.0f - pulse_t);
            pulse = 1.0f - ease;
        }
        float alpha_f = constants::FLOOR_ALPHA_REST
            + (constants::FLOOR_ALPHA_PEAK - constants::FLOOR_ALPHA_REST) * pulse;
        float scale = constants::FLOOR_SCALE_REST
            + (constants::FLOOR_SCALE_PEAK - constants::FLOOR_SCALE_REST) * pulse;
        floor_params.size  = constants::FLOOR_SHAPE_SIZE * scale;
        floor_params.half  = floor_params.size / 2.0f;
        floor_params.thick = constants::FLOOR_OUTLINE_THICK;
        floor_params.alpha = static_cast<uint8_t>(alpha_f);
    }

    // ── Render passes ──────────────────────────────────────────
    draw_floor_lines(reg, floor_params);
    draw_floor_rings(floor_params);

    if (gs.phase != GamePhase::Title) {
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        draw_meshes(reg);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }

    EndMode3D();
}
