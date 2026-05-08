#include "all_systems.h"
#include "floor_render_system.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "camera_system.h"
#include <raylib.h>
#include <rlgl.h>

static void draw_model_transform(const camera::ShapeMeshes& sm, const ModelTransform& mt) {
    // Use a per-draw local copy so the shared material is never mutated.
    Material mat = sm.material;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
    switch (mt.mesh_type) {
        case MeshType::Slab:
            DrawMesh(sm.slab, mat, mt.mat);
            break;
        case MeshType::Shape:
            DrawMesh(sm.shapes[mt.mesh_index], mat, mt.mat);
            break;
        case MeshType::Quad:
            DrawMesh(sm.quad, mat, mt.mat);
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

// Draw Model-authority entities (LowBar, HighBar) that own their mesh arrays.
// These entities carry ObstacleModel + TagWorldPass and are NOT in the ModelTransform pool.
static void draw_owned_models(const entt::registry& reg) {
    auto view = reg.view<const ObstacleModel, const Color, const TagWorldPass>();
    for (auto [entity, om, tint] : view.each()) {
        if (!om.owned || !om.model.meshes) continue;
        for (int i = 0; i < om.model.meshCount; ++i) {
            Material mat = om.model.materials[om.model.meshMaterial[i]];
            mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
            DrawMesh(om.model.meshes[i],
                     mat,
                     om.model.transform);
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
        draw_owned_models(reg);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }

    EndMode3D();
}
