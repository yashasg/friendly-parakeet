#include "all_systems.h"
#include "runtime_systems.h"
#include "floor_render_system.h"
#include "../components/render_mesh.h"
#include "../components/game_state.h"
#include "../tags/tags.h"
#include "../util/shape_lane_mapping.h"
#include "camera_system.h"
#include <raylib.h>
#include <rlgl.h>

static void draw_shape(const camera::ShapeMeshes& sm, const ModelTransform& mt,
                       const MeshKindShape& kind) {
    if (kind.mesh_index >= kShapeCount) return;
    Material mat = sm.material;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
    DrawMesh(sm.shapes[kind.mesh_index], mat, mt.mat);
}

static void draw_slab(const camera::ShapeMeshes& sm, const ModelTransform& mt) {
    Material mat = sm.material;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
    DrawMesh(sm.slab, mat, mt.mat);
}

static void draw_quad(const camera::ShapeMeshes& sm, const ModelTransform& mt) {
    Material mat = sm.material;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
    DrawMesh(sm.quad, mat, mt.mat);
}

// Per Fabian existential processing (issue #1202/#1204), the former
// `switch(mesh_type)` is replaced by one transform per mesh-kind tag.
template <typename PassTag>
static void draw_pass(const entt::registry& reg, const camera::ShapeMeshes& sm) {
    for (auto [_, mt, kind] :
             reg.view<const ModelTransform, const PassTag, const MeshKindShape>().each()) {
        draw_shape(sm, mt, kind);
    }
    for (auto [_, mt] :
             reg.view<const ModelTransform, const PassTag, const MeshKindSlab>().each()) {
        draw_slab(sm, mt);
    }
    for (auto [_, mt] :
             reg.view<const ModelTransform, const PassTag, const MeshKindQuad>().each()) {
        draw_quad(sm, mt);
    }
}

static void draw_meshes(const entt::registry& reg) {
    const auto* sm = reg.ctx().find<camera::ShapeMeshes>();
    if (!sm) return;
    draw_pass<TagWorldPass>(reg, *sm);
    draw_pass<TagEffectsPass>(reg, *sm);
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
