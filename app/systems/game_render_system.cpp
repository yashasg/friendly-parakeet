#include "all_systems.h"
#include "runtime_systems.h"
#include "floor_render_system.h"
#include "../components/render_mesh.h"
#include "../components/game_state.h"
#include "../util/shape_lane_mapping.h"
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
            if (mt.mesh_index >= kShapeCount) return;
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


void game_render_system(const entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& camera = game_camera(reg).cam;

    ClearBackground({15, 15, 25, 255});

    rlSetClipPlanes(1.0, 5000.0);
    BeginMode3D(camera);

    // ── Render passes ──────────────────────────────────────────
    floor_render_system(reg);

    if (game_render_should_draw_world_meshes(gs.phase)) {
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        draw_meshes(reg);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }

    EndMode3D();
}
