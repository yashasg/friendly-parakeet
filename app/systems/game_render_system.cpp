#include "all_systems.h"
#include "runtime_systems.h"
#include "floor_render_system.h"
#include "camera_resources.h"
#include "../components/render_mesh.h"
#include "../components/game_state.h"
#include "../tags/tags.h"
#include "../util/shape_lane_mapping.h"
#include "camera_system.h"
#include <raylib.h>
#include <rlgl.h>

// Per Fabian existential processing (issue #1202/#1204), the former
// `switch(mesh_type)` is replaced by one transform per mesh-kind tag.
template <typename PassTag>
static void draw_pass(const entt::registry& reg, const camera::ShapeMeshes& sm) {
    for (auto [_, mt, kind] :
             reg.view<const ModelTransform, const PassTag, const MeshKindShape>().each()) {
        if (kind.mesh_index >= kShapeCount) continue;
        Material mat = sm.material;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
        DrawMesh(sm.shapes[kind.mesh_index], mat, mt.mat);
    }
    for (auto [_, mt] :
             reg.view<const ModelTransform, const PassTag, const MeshKindSlab>().each()) {
        Material mat = sm.material;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
        DrawMesh(sm.slab, mat, mt.mat);
    }
    for (auto [_, mt] :
             reg.view<const ModelTransform, const PassTag, const MeshKindQuad>().each()) {
        Material mat = sm.material;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
        DrawMesh(sm.quad, mat, mt.mat);
    }
}

void game_render_system(const entt::registry& reg, float /*alpha*/) {
    auto& camera = game_camera(reg).cam;

    ClearBackground({15, 15, 25, 255});

    rlSetClipPlanes(1.0, 5000.0);
    BeginMode3D(camera);

    // ── Render passes ──────────────────────────────────────────
    floor_render_system(reg);

    if (game_render_should_draw_world_meshes(reg)) {
        if (const auto* sm = reg.ctx().find<camera::ShapeMeshes>()) {
            rlDrawRenderBatchActive();
            rlDisableDepthTest();
            draw_pass<TagWorldPass>(reg, *sm);
            draw_pass<TagEffectsPass>(reg, *sm);
            rlDrawRenderBatchActive();
            rlEnableDepthTest();
        }
    }

    EndMode3D();
}
