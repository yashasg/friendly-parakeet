#include "all_systems.h"
#include "../util/shape_vertices.h"
#include "../components/camera.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include "camera_system.h"
#include <raylib.h>
#include <rlgl.h>

// ═════════════════════════════════════════════════════════════════════════════
// 3D world draw passes — floor, obstacles, particles, shapes
// ═════════════════════════════════════════════════════════════════════════════

static constexpr int SHAPE_COLOR_COUNT =
    static_cast<int>(sizeof(constants::SHAPE_COLORS) / sizeof(constants::SHAPE_COLORS[0]));
static_assert(constants::LANE_COUNT <= SHAPE_COLOR_COUNT,
              "Lane rendering expects one canonical shape color per lane");

static Color floor_lane_color(int lane, uint8_t alpha) {
    Color c = constants::SHAPE_COLORS[lane];
    c.a = alpha;
    return c;
}

static void draw_floor_lines(const FloorParams& fp) {
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
            Color c = floor_lane_color(lane, 50);
            rlColor4ub(c.r, c.g, c.b, c.a);
            rlVertex3f(cx - lane_half, 0.0f, 0.0f);
            rlVertex3f(cx - lane_half, 0.0f, sh);
            rlVertex3f(cx + lane_half, 0.0f, 0.0f);
            rlVertex3f(cx + lane_half, 0.0f, sh);
        }
    }

    // Floor connectors + shape outlines
    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float cx = constants::LANE_X[lane];
        Color c = floor_lane_color(lane, fp.alpha);

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
    Color c = floor_lane_color(0, fp.alpha);

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

static void draw_meshes(const entt::registry& reg) {
    const auto* sm = reg.ctx().find<camera::ShapeMeshes>();
    if (!sm) return;

    auto view = reg.view<const ModelTransform>();
    for (auto [entity, mt] : view.each()) {
        // Use a per-draw local copy so the shared material is never mutated.
        Material mat = sm->material;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
        switch (mt.mesh_type) {
            case MeshType::Slab:
                DrawMesh(sm->slab, mat, mt.mat);
                break;
            case MeshType::Shape:
                DrawMesh(sm->shapes[mt.mesh_index], mat, mt.mat);
                break;
            case MeshType::Quad:
                DrawMesh(sm->quad, mat, mt.mat);
                break;
        }
    }
}

// Draw Model-authority entities (LowBar, HighBar) that own their mesh arrays.
// These entities carry ObstacleModel + TagWorldPass and are NOT in the ModelTransform pool.
static void draw_owned_models(const entt::registry& reg) {
    auto view = reg.view<const ObstacleModel, const TagWorldPass>();
    for (auto [entity, om] : view.each()) {
        if (!om.owned || !om.model.meshes) continue;
        for (int i = 0; i < om.model.meshCount; ++i) {
            DrawMesh(om.model.meshes[i],
                     om.model.materials[om.model.meshMaterial[i]],
                     om.model.transform);
        }
    }
}

void game_render_system(const entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& camera = reg.ctx().get<GameCamera>().cam;

    ClearBackground({15, 15, 25, 255});

    rlSetClipPlanes(1.0, 5000.0);
    BeginMode3D(camera);

    const auto& floor_params = reg.ctx().get<FloorParams>();

    // ── Render passes ──────────────────────────────────────────
    draw_floor_lines(floor_params);
    draw_floor_rings(floor_params);

    if (gs.phase != GamePhase::Title) {
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        draw_meshes(reg);
        draw_owned_models(reg);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }

    EndMode3D();
}
