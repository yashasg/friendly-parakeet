#include "all_systems.h"
#include "floor_render_system.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "camera_system.h"
#include <glm/mat4x4.hpp>
#include <raylib.h>
#include <rlgl.h>

static Matrix glm_to_raylib_matrix_render(const glm::mat4& m) {
    Matrix out{};
    out.m0  = m[0][0]; out.m4  = m[1][0]; out.m8  = m[2][0]; out.m12 = m[3][0];
    out.m1  = m[0][1]; out.m5  = m[1][1]; out.m9  = m[2][1]; out.m13 = m[3][1];
    out.m2  = m[0][2]; out.m6  = m[1][2]; out.m10 = m[2][2]; out.m14 = m[3][2];
    out.m3  = m[0][3]; out.m7  = m[1][3]; out.m11 = m[2][3]; out.m15 = m[3][3];
    return out;
}

static void draw_model_transform(const camera::ShapeMeshes& sm, const ModelTransform& mt) {
    // Use a per-draw local copy so the shared material is never mutated.
    Material mat = sm.material;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
    const Matrix matrix = glm_to_raylib_matrix_render(mt.mat);
    switch (mt.mesh_type) {
        case MeshType::Slab:
            DrawMesh(sm.slab, mat, matrix);
            break;
        case MeshType::Shape:
            DrawMesh(sm.shapes[mt.mesh_index], mat, matrix);
            break;
        case MeshType::Quad:
            DrawMesh(sm.quad, mat, matrix);
            break;
    }
}

static void draw_meshes(const entt::registry& reg) {
    const auto* sm = reg.ctx().find<camera::ShapeMeshes>();
    if (!sm) return;

    {
        auto view = reg.view<const ModelTransform, const TagWorldPass>();
        for (auto [entity, mt] : view.each()) {
            draw_model_transform(*sm, mt);
        }
    }

    {
        auto view = reg.view<const ModelTransform, const TagEffectsPass>();
        for (auto [entity, mt] : view.each()) {
            draw_model_transform(*sm, mt);
        }
    }
}


void game_render_system(const entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& camera = game_camera(reg).cam;

    ClearBackground({15, 15, 25, 255});

    rlSetClipPlanes(1.0, 5000.0);
    BeginMode3D(camera);

    // ── Render passes ──────────────────────────────────────────
    floor_render_system(reg);

    if (gs.phase != GamePhase::Title) {
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        draw_meshes(reg);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }

    EndMode3D();
}
